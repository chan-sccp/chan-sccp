/*!
 * \file        sccp_pbx.c
 * \brief       SCCP PBX Asterisk Wrapper Class
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
#include "config.h"
#include "common.h"
#include "sccp_pbx.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_conference.h"
#include "sccp_feature.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_linedevice.h"
#include "sccp_rtp.h"
#include "sccp_netsock.h"
#include "sccp_session.h"
#include "sccp_atomic.h"
#include "sccp_labels.h"
#include "sccp_threadpool.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/callerid.h>
#include <asterisk/module.h>		// ast_update_use_count
#include <asterisk/causes.h>		// AST_CAUSE_NORMAL_CLEARING

/*!
 * \brief SCCP Request Channel
 * \param lineName              Line Name as Char
 * \param autoanswer_type       SCCP Auto Answer Type
 * \param autoanswer_cause      SCCP Auto Answer Cause
 * \param ringermode            Ringer Mode
 * \param channel               SCCP Channel
 * \return SCCP Channel Request Status
 * 
 * \called_from_asterisk
 */
sccp_channel_request_status_t sccp_requestChannel(const char * lineName, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, skinny_ringtype_t ringermode, sccp_channel_t * const * channel)
{
	if (!lineName) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	char mainId[SCCP_MAX_EXTENSION];
	sccp_subscription_id_t subscriptionId;
	if(!sccp_parseComposedId(lineName, 80, &subscriptionId, mainId)) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "Could not parse lineName:%s !\n", lineName);
		return SCCP_REQUEST_STATUS_LINEUNKNOWN;
	};

	AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(mainId, FALSE));
	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", mainId);
		return SCCP_REQUEST_STATUS_LINEUNKNOWN;
	}
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (SCCP_RWLIST_GETSIZE(&l->devices) == 0) {
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		return SCCP_REQUEST_STATUS_LINEUNAVAIL;
	}
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	AUTO_RELEASE(sccp_channel_t, my_sccp_channel, sccp_channel_allocate(l, NULL));
	if (!my_sccp_channel) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	/* set subscriberId for individual device addressing */
	if (!sccp_strlen_zero(subscriptionId.number)) {
		sccp_copy_string(my_sccp_channel->subscriptionId.number, subscriptionId.number, sizeof(my_sccp_channel->subscriptionId.number));
		if (!sccp_strlen_zero(subscriptionId.name)) {
			sccp_copy_string(my_sccp_channel->subscriptionId.name, subscriptionId.name, sizeof(my_sccp_channel->subscriptionId.name));
		} else {
			//pbx_log(LOG_NOTICE, "%s: calling subscriber id=%s\n", l->id, my_sccp_channel->subscriptionId.number);
		}
	} else {
		sccp_copy_string(my_sccp_channel->subscriptionId.number, l->defaultSubscriptionId.number, sizeof(my_sccp_channel->subscriptionId.number));
		sccp_copy_string(my_sccp_channel->subscriptionId.name, l->defaultSubscriptionId.name, sizeof(my_sccp_channel->subscriptionId.name));
		//pbx_log(LOG_NOTICE, "%s: calling all subscribers\n", l->id);
	}

	//memset(&channel->preferences.audio, 0, sizeof(channel->preferences.audio));
	//memset(&channel->preferences.video, 0, sizeof(channel->preferences.video));

	my_sccp_channel->autoanswer_type = autoanswer_type;
	my_sccp_channel->autoanswer_cause = autoanswer_cause;
	my_sccp_channel->ringermode = ringermode;
	my_sccp_channel->hangupRequest = sccp_astgenwrap_requestQueueHangup;
	(*(sccp_channel_t **)channel) = sccp_channel_retain(my_sccp_channel);
	return SCCP_REQUEST_STATUS_SUCCESS;
}

/*!
 * \brief SCCP Structure to pass data to the pbx answer thread
 */
struct sccp_answer_conveyor_struct {
	sccp_linedevice_t * ld;
	uint32_t callid;
};
/*!
 * \brief Call Auto Answer Thead
 * \param data Data
 *
 * The Auto Answer thread is started by ref sccp_pbx_call if necessary
 */
static void *sccp_pbx_call_autoanswer_thread(void *data)
{
	struct sccp_answer_conveyor_struct *conveyor = (struct sccp_answer_conveyor_struct *)data;

	sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "SCCP: autoanswer thread started\n");

	sleep(GLOB(autoanswer_ring_time));
	pthread_testcancel();

	if (!conveyor) {
		pbx_log(LOG_ERROR, "SCCP: (autoanswer_thread) No conveyor\n");
		return NULL;
	}
	if(!conveyor->ld) {
		pbx_log(LOG_ERROR, "SCCP: (autoanswer_thread) No linedevice\n");
		goto FINAL;
	}

	{
		AUTO_RELEASE(sccp_device_t, device, sccp_device_retain(conveyor->ld->device));

		if (!device) {
			pbx_log(LOG_ERROR, "SCCP: (autoanswer_thread) no device\n");
			goto FINAL;
		}

		AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(conveyor->callid));

		if (!c || c->state != SCCP_CHANNELSTATE_RINGING) {
			pbx_log(LOG_WARNING, "%s: (autoanswer_thread) %s\n", device->id, c ? "not ringing" : "no channel");
			goto FINAL;
		}

		sccp_channel_answer(device, c);
		c->setTone(c, GLOB(autoanswer_tone), SKINNY_TONEDIRECTION_USER);
		if (c->autoanswer_type == SCCP_AUTOANSWER_1W) {
			sccp_dev_set_microphone(device, SKINNY_STATIONMIC_OFF);
		}
	}
FINAL:
	if(conveyor->ld) {
		sccp_linedevice_release(&conveyor->ld);                                        // retained in calling thread, explicit release required here
	}
	sccp_free(conveyor);
	return NULL;
}

/*!
 * \brief Incoming Calls by Asterisk SCCP_Request
 * \param c SCCP Channel
 * \param dest Destination as char
 * \param timeout Timeout after which incoming call is cancelled as int
 * \return Success as int
 *
 * \todo reimplement DNDMODES, ringermode=urgent, autoanswer
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 *
 * \note called with c retained
 */
// improved sharedline handling
// - calculate c->subscribers correctly			(using when handling sccp_softkey_onhook, to define behaviour)
// - handle dnd and callforward after calculating c->subscribers
// - use asterisk local channel to resolve forward bei non-shared line, which does evade
//   * sccp_channel_callforward
//   * sccp_channel_end_forwarding_channel
//   * c->parentChannel
//   * masquerading in sccp_pbx_answer
//   in that case
int sccp_pbx_call(channelPtr c, const char * dest, int timeout)
{
	if (!c) {
		return -1;
	}
	int res = 0;

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(c->line));
	if (!l) {
		pbx_log(LOG_WARNING, "SCCP: The channel %08X has no line. giving up.\n", (c->callid));
		return -1;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Asterisk request to call %s\n", l->name, iPbx.getChannelName(c));

	int incomingcalls = 0;
	sccp_channel_t *count_channel = NULL;
	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, count_channel, list) {
		if (count_channel != c && count_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
			incomingcalls++;
		}
	}
	SCCP_LIST_UNLOCK(&l->channels);
	sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP/%s: Incoming calls:%d, Incoming calls limit: %d\n", l->name, incomingcalls, l->incominglimit);
	/* if incoming call limit is reached send BUSY */
	if(l->incominglimit && incomingcalls >= l->incominglimit) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP/%s: Incoming calls: %d, Incoming calls limit (%d) -> sending BUSY\n", l->name, incomingcalls, l->incominglimit);
		iPbx.queue_control(c->owner, AST_CONTROL_BUSY);
		//iPbx.set_callstate(c, AST_STATE_BUSY);
		pbx_channel_set_hangupcause(c->owner, AST_CAUSE_USER_BUSY);
		return 0;
	}

	/* Reinstated this call instead of the following lines */
	char cid_name[StationMaxNameSize] = {0};
	char cid_num[StationMaxDirnumSize] = {0};
	char suffixedNumber[StationMaxDirnumSize] = {0};
	sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;

	sccp_callinfo_t *ci = sccp_channel_getCallInfo(c);
	iCallInfo.Getter(ci, 
		SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name, 
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num, 
		SCCP_CALLINFO_PRESENTATION, &presentation, 
		SCCP_CALLINFO_KEY_SENTINEL);
	sccp_copy_string(suffixedNumber, cid_num, sizeof(suffixedNumber));
	//! \todo implement dnid, ani, ani2 and rdnis
	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk callerid='%s <%s>'\n", cid_name, cid_num);

	/* Set the channel callingParty Name and Number, called Party Name and Number, original CalledParty Name and Number, Presentation */
	if (GLOB(recorddigittimeoutchar)) {
		/* The hack to add the # at the end of the incoming number
		   is only applied for numbers beginning with a 0,
		   which is appropriate for Germany and other countries using similar numbering plan.
		   The option should be generalized, moved to the dialplan, or otherwise be replaced. */
		/* Also, we require an option whether to add the timeout suffix to certain
		   enbloc dialed numbers (such as via 7970 enbloc dialing) if they match a certain pattern.
		   This would help users dial from call history lists on other phones, which do not have enbloc dialing,
		   when using shared lines. */
		int length = sccp_strlen(cid_num);
		if (length && (length + 2  < StationMaxDirnumSize) && ('\0' == cid_num[0])) {
			suffixedNumber[length + 0] = GLOB(digittimeoutchar);
			suffixedNumber[length + 1] = '\0';
		}
		/* Set the channel calledParty Name and Number 7910 compatibility */
	}
	//! \todo implement dnid, ani, ani2 and rdnis
	sccp_callerid_presentation_t pbx_presentation = iPbx.get_callerid_presentation ? iPbx.get_callerid_presentation(c->owner) : SCCP_CALLERID_PRESENTATION_SENTINEL;
	if (	(!sccp_strequals(suffixedNumber, cid_num)) || 
		(pbx_presentation != SCCP_CALLERID_PRESENTATION_SENTINEL && pbx_presentation != presentation)
	) {
		iCallInfo.Setter(ci, 
			SCCP_CALLINFO_CALLINGPARTY_NUMBER, (!sccp_strlen_zero(suffixedNumber) ? suffixedNumber : NULL), 
			SCCP_CALLINFO_PRESENTATION, (pbx_presentation != SCCP_CALLERID_PRESENTATION_SENTINEL) ? pbx_presentation : presentation,
			SCCP_CALLINFO_KEY_SENTINEL);
	}
	sccp_channel_display_callInfo(c);

	if (!c->ringermode) {
		c->ringermode = GLOB(ringtype);
	}
	boolean_t isRinging = FALSE;
	boolean_t hasDNDParticipant = FALSE;
	boolean_t bypassCallForward = !sccp_strlen_zero(pbx_builtin_getvar_helper(c->owner, "BYPASS_CFWD"));
	sccp_linedevice_t * ForwardingLineDevice = NULL;

	sccp_linedevice_t * ld = NULL;
	sccp_channelstate_t previousstate = c->previousChannelState;
	
	SCCP_LIST_LOCK(&l->devices);
	int num_devices = SCCP_LIST_GETSIZE(&l->devices);
	c->subscribers = num_devices;

	pbx_str_t * cfwds_all = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	pbx_str_t * cfwds_busy = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	pbx_str_t * cfwds_noanswer = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	int cfwd_all = 0;
	int cfwd_busy = 0;
	int cfwd_noanswer = 0;
	SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
		AUTO_RELEASE(sccp_channel_t, active_channel, sccp_device_getActiveChannel(ld->device));

		// skip incoming call on a shared line from the originator. (sharedline calling same sharedline)
		if (active_channel && active_channel != c && sccp_strequals(iPbx.getChannelLinkedId(active_channel), iPbx.getChannelLinkedId(c))) {
			sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) skip ringing on %s\n", ld->device->id);
			c->subscribers--;
			continue;
		}
		
		/* do we have cfwd enabled? */
		if(ld->cfwd[SCCP_CFWD_ALL].enabled) {
			ast_str_append(&cfwds_all, DEFAULT_PBX_STR_BUFFERSIZE, "%s%s", cfwd_all++ ? "," : "", ld->cfwd[SCCP_CFWD_ALL].number);
		}
		if(ld->cfwd[SCCP_CFWD_BUSY].enabled) {
			ast_str_append(&cfwds_busy, DEFAULT_PBX_STR_BUFFERSIZE, "%s%s", cfwd_busy++ ? "," : "", ld->cfwd[SCCP_CFWD_BUSY].number);
		}
		if(!bypassCallForward
		   && (ld->cfwd[SCCP_CFWD_ALL].enabled || (ld->cfwd[SCCP_CFWD_BUSY].enabled && (sccp_device_getDeviceState(ld->device) != SCCP_DEVICESTATE_ONHOOK || sccp_device_getActiveAccessory(ld->device))))) {
			if (num_devices == 1) {
				/* when single line -> use asterisk functionality directly, without creating new channel + masquerade */
				sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: Call Forward active on line %s\n", ld->device->id, ld->line->name);
				ForwardingLineDevice = ld;
			} else {
				/* shared line -> create a temp channel to call forward destination and tie them together */
				pbx_log(LOG_NOTICE, "%s: handle cfwd to %s for line %s\n", ld->device->id, ld->cfwd[SCCP_CFWD_ALL].enabled ? ld->cfwd[SCCP_CFWD_ALL].number : ld->cfwd[SCCP_CFWD_BUSY].number, l->name);
				if(sccp_channel_forward(c, ld, ld->cfwd[SCCP_CFWD_ALL].enabled ? ld->cfwd[SCCP_CFWD_ALL].number : ld->cfwd[SCCP_CFWD_BUSY].number) == 0) {
					sccp_device_sendcallstate(ld->device, ld->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
					sccp_channel_send_callinfo(ld->device, c);
					isRinging = TRUE;
				}
			};
			c->subscribers--;
			continue;
		}

		if(ld->cfwd[SCCP_CFWD_NOANSWER].enabled && c) {
			sccp_channel_schedule_cfwd_noanswer(c, GLOB(cfwdnoanswer_timeout));
			ast_str_append(&cfwds_noanswer, DEFAULT_PBX_STR_BUFFERSIZE, "%s%s", cfwd_noanswer++ ? "," : "", ld->cfwd[SCCP_CFWD_NOANSWER].number);
		}

		if(!ld->device->session) {
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: line device has no session\n", DEV_ID_LOG(ld->device));
			c->subscribers--;
			continue;
		}
		/* check if c->subscriptionId.number is matching deviceSubscriptionID */
		/* This means that we call only those devices on a shared line
		   which match the specified subscription id in the dial parameters. */
		if(!sccp_util_matchSubscriptionId(c, ld->subscriptionId.number)) {
			sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: device does not match subscriptionId.number c->subscriptionId.number: '%s', deviceSubscriptionID: '%s'\n", DEV_ID_LOG(ld->device),
						 c->subscriptionId.number, ld->subscriptionId.number);
			c->subscribers--;
			continue;
		}
		/* reset channel state (because we are offering the same call to multiple (shared) lines)*/
		c->previousChannelState = previousstate;
		if (active_channel) {
			sccp_indicate(ld->device, c, SCCP_CHANNELSTATE_CALLWAITING);
			/* display the new call on prompt */
			AUTO_RELEASE(sccp_linedevice_t, activeChannelLinedevice, active_channel->getLineDevice(active_channel));
			if (activeChannelLinedevice) {
				char caller[100] = {0};
				if(!sccp_strlen_zero(cid_name)) {
					if(!sccp_strlen_zero(cid_num)) {
						snprintf(caller,sizeof(caller), "%s %s <%s>", SKINNY_DISP_CALL_WAITING, cid_name, cid_num);
					} else {
						snprintf(caller,sizeof(caller), "%s %s", SKINNY_DISP_CALL_WAITING, cid_name);
					}
				} else {
					if (!sccp_strlen_zero(cid_num)) {
						snprintf(caller,sizeof(caller), "%s %s", SKINNY_DISP_CALL_WAITING, cid_num);
					} else {
						snprintf(caller,sizeof(caller), "%s %s", SKINNY_DISP_CALL_WAITING, SKINNY_DISP_UNKNOWN_NUMBER);
					}
				}
				// snprintf(prompt, sizeof(prompt), "%s: %s: %s", active_channel->line->name, SKINNY_DISP_FROM, cid_num);
				// sccp_dev_displayprompt(ld->device, activeChannelLinedevice->lineInstance, active_channel->callid, caller, SCCP_DISPLAYSTATUS_TIMEOUT);
				sccp_dev_set_message(ld->device, caller, SCCP_DISPLAYSTATUS_TIMEOUT, FALSE, FALSE);
			}
			ForwardingLineDevice = NULL;	/* reset cfwd if shared */
			isRinging = TRUE;
		} else {
			/** check if ringermode is not urgent and device enabled dnd in reject mode */
			if(SKINNY_RINGTYPE_URGENT != c->ringermode && ld->device->dndFeature.enabled && ld->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: DND active on line %s, returning Busy\n", ld->device->id, ld->line->name);
				hasDNDParticipant = TRUE;
				c->subscribers--;
				continue;
			}
			ForwardingLineDevice = NULL;	/* reset cfwd if shared */
			sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "%s: Ringing %sLine: %s on device:%s using channel:%s, ringermode:%s\n", ld->device->id, SCCP_LIST_GETSIZE(&l->devices) > 1 ? "Shared" : "", ld->line->name,
					       ld->device->id, c->designator, skinny_ringtype2str(c->ringermode));
			sccp_indicate(ld->device, c, SCCP_CHANNELSTATE_RINGING);
			isRinging = TRUE;
			if (c->autoanswer_type) {
				struct sccp_answer_conveyor_struct *conveyor = (struct sccp_answer_conveyor_struct *)sccp_calloc(1, sizeof(struct sccp_answer_conveyor_struct));
				if (conveyor) {
					sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", DEV_ID_LOG(ld->device), iPbx.getChannelName(c));
					conveyor->callid = c->callid;
					conveyor->ld = sccp_linedevice_retain(ld);

					sccp_threadpool_add_work(GLOB(general_threadpool), sccp_pbx_call_autoanswer_thread, (void *) conveyor);
				} else {
					pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, c->designator);
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	//sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "%s: isRinging:%d, hadDNDParticipant:%d, ForwardingLineDevice:%p\n", c->designator, isRinging, hasDNDParticipant, ForwardingLineDevice);
	if(cfwd_all) {
		pbx_builtin_setvar_helper(c->owner, "_CFWD_ALL", pbx_str_buffer(cfwds_all));
	}
	if(cfwd_busy) {
		pbx_builtin_setvar_helper(c->owner, "_CFWD_BUSY", pbx_str_buffer(cfwds_busy));
	}
	if(cfwd_noanswer) {
		pbx_builtin_setvar_helper(c->owner, "_CFWD_NOANSWER", pbx_str_buffer(cfwds_noanswer));
	}
	if (isRinging) {
		sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_RINGING);
		iPbx.set_callstate(c, AST_STATE_RINGING);
		iPbx.queue_control(c->owner, AST_CONTROL_RINGING);
	} else if (ForwardingLineDevice) {
		/* when single line -> use asterisk functionality directly, without creating new channel + masquerade */
		pbx_log(LOG_NOTICE, "%s: handle cfwd to %s for line %s\n", ForwardingLineDevice->device->id,
			ForwardingLineDevice->cfwd[SCCP_CFWD_ALL].enabled ? ForwardingLineDevice->cfwd[SCCP_CFWD_ALL].number : ForwardingLineDevice->cfwd[SCCP_CFWD_BUSY].number, l->name);
#if CS_AST_CONTROL_REDIRECTING
		iPbx.queue_control(c->owner, AST_CONTROL_REDIRECTING);
#endif
		pbx_channel_call_forward_set(c->owner, ForwardingLineDevice->cfwd[SCCP_CFWD_ALL].enabled ? ForwardingLineDevice->cfwd[SCCP_CFWD_ALL].number : ForwardingLineDevice->cfwd[SCCP_CFWD_BUSY].number);
		sccp_device_sendcallstate(ForwardingLineDevice->device, ForwardingLineDevice->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(ForwardingLineDevice->device, c);
	} else if (hasDNDParticipant) {
		iPbx.queue_control(c->owner, AST_CONTROL_BUSY);
		// iPbx.set_callstate(c, AST_STATE_BUSY);
		// pbx_channel_set_hangupcause(c->owner, AST_CAUSE_USER_BUSY);
		pbx_channel_set_hangupcause(c->owner, AST_CAUSE_BUSY);
		res = 0;
	} else {
		iPbx.queue_control(c->owner, AST_CONTROL_CONGESTION);
		res = -1;
	}

	/* set linevariables */
	PBX_VARIABLE_TYPE *v = l->variables;
	while (c->owner && !pbx_check_hangup(c->owner) && l && v) {
		pbx_builtin_setvar_helper(c->owner, v->name, v->value);
		v = v->next;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_call) Returning: %d\n", c->designator, res);
	return res;
}

/*!
 * callback function to handle callforward when recipient does not answer within GLOB(cfwdnoanswer_timeout)
 *
 * this callback is scheduled in sccp_pbx_call / sccp_channel_schedule_cfwd_noanswer
 */
int sccp_pbx_cfwdnoanswer_cb(const void * data)
{
	AUTO_RELEASE(sccp_channel_t, c, sccp_channel_retain(data));
	if(!c || !c->owner) {
		pbx_log(LOG_WARNING, "SCCP: No channel provided.\n");
		return -1;
	}

	if((ATOMIC_FETCH(&c->scheduler.deny, &c->scheduler.lock) == 0)) {
		c->scheduler.cfwd_noanswer_id = -3; /* prevent further cfwd_noanswer_id scheduling */
	}

	AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(c->line));
	if(!l) {
		pbx_log(LOG_WARNING, "SCCP: The channel %08X has no line. giving up.\n", (c->callid));
		return -2;
	}

	pbx_channel_lock(c->owner);
	PBX_CHANNEL_TYPE * forwarder = pbx_channel_ref(c->owner);
	pbx_channel_unlock(c->owner);

	if(!forwarder || pbx_check_hangup(forwarder)) {
		pbx_log(LOG_WARNING, "SCCP: The channel %08X was already hungup. giving up.\n", (c->callid));
		pbx_channel_unref(forwarder);
		return -3;
	}

	boolean_t bypassCallForward = !sccp_strlen_zero(pbx_builtin_getvar_helper(c->owner, "BYPASS_CFWD"));

	sccp_linedevice_t * ld = NULL;
	SCCP_LIST_LOCK(&l->devices);
	int num_devices = SCCP_LIST_GETSIZE(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
		/* do we have cfwd enabled? */
		if(ld->cfwd[SCCP_CFWD_NOANSWER].enabled && !sccp_strlen_zero(ld->cfwd[SCCP_CFWD_NOANSWER].number)) {
			if(!bypassCallForward) {
				if(num_devices == 1) {
					/* when single line -> use asterisk functionality directly, without creating new channel + masquerade */
					sccp_log(DEBUGCAT_PBX)("%s: Redirecting call to callforward noanswer:%s\n", c->designator, ld->cfwd[SCCP_CFWD_NOANSWER].number);
#if CS_AST_CONTROL_REDIRECTING
					iPbx.queue_control(c->owner, AST_CONTROL_REDIRECTING);
#endif
					pbx_channel_call_forward_set(c->owner, ld->cfwd[SCCP_CFWD_NOANSWER].number);
					sccp_device_sendcallstate(ld->device, ld->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
					sccp_channel_send_callinfo(ld->device, c);
					iPbx.set_owner(c, NULL);
					break;                                        //! \todo currently using only the first match.
				} else {
					/* shared line -> create a temp channel to call forward destination and tie them together */
					pbx_log(LOG_NOTICE, "%s: handle cfwd to %s for line %s\n", ld->device->id, ld->cfwd[SCCP_CFWD_NOANSWER].number, l->name);
					sccp_channel_forward(c, ld, ld->cfwd[SCCP_CFWD_NOANSWER].number);
				};
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);
	pbx_channel_unref(forwarder);
	return 0;
}

/*!
 * \brief Handle Hangup Request by Asterisk
 * \param channel SCCP Channel
 * \return Success as int
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk via sccp_wrapper_asterisk.._hangup
 *
 * \note sccp_channel should be retained in calling function
 */

channelPtr sccp_pbx_hangup(constChannelPtr channel)
{

	/* here the ast channel is locked */
	// sccp_log((DEBUGCAT_CORE)) (;VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", iPbx.getChannelName(c));
	(void) ATOMIC_DECR(&GLOB(usecnt), 1, &GLOB(usecnt_lock));

	pbx_update_use_count();

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	if (!c) {
		sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel. SCCP channel already deleted\n");
		return NULL;
	}
	c->isHangingUp = TRUE;
	sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Asked to hangup channel.\n", c->designator);

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if(d && d->session) {
		sccp_session_waitForPendingRequests(d->session);
		/*		if (
					GLOB(remotehangup_tone) &&
					SKINNY_DEVICE_RS_OK == sccp_device_getRegistrationState(d) &&
					SCCP_DEVICESTATE_OFFHOOK == sccp_device_getDeviceState(d) &&
					SCCP_CHANNELSTATE_IsConnected(c->state) &&
					c == d->active_channel
				) {
					c->setTone(c, GLOB(remotehangup_tone), SKINNY_TONEDIRECTION_USER);
				}*/
	}

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(c->line));

#ifdef CS_SCCP_CONFERENCE
	if (c && c->conference) {
		sccp_conference_release(&c->conference);								/* explicit release required here */
	}
	if (d && d->conference) {
		sccp_conference_release(&d->conference);								/* explicit release required here */
	}
#endif														// CS_SCCP_CONFERENCE
	if (c->rtp.audio.instance || c->rtp.video.instance) {
		sccp_channel_closeAllMediaTransmitAndReceive(c);
	}

	// removing scheduled dialing
	sccp_channel_stop_schedule_digittimout(c);
	sccp_channel_stop_schedule_cfwd_noanswer(c);

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Current pbxchannelstate %s(%d)\n", c->designator, sccp_channelstate2str(c->state), c->state);

	/* end callforwards */
	sccp_channel_end_forwarding_channel(c);

	/* cancel transfer if in progress */
	if(d) {
		sccp_channel_transfer_cancel(d, c);
	}

	if (l) {
		/* remove call from transferee, transferer */
		sccp_linedevice_t * ld = NULL;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
			AUTO_RELEASE(sccp_device_t, tmpDevice, sccp_device_retain(ld->device));
			if(!d && tmpDevice && SKINNY_DEVICE_RS_OK == sccp_device_getRegistrationState(tmpDevice)) {
				d = sccp_device_retain(tmpDevice);
			}
			if (tmpDevice) {
				sccp_channel_transfer_release(tmpDevice, c); /* explicit release required here */
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
		sccp_line_removeChannel(l, c);
	}

	if (d) {
		if (d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE) {
			d->monitorFeature.status &= ~SCCP_FEATURE_MONITOR_STATE_ACTIVE;
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Reset monitor state after hangup\n", DEV_ID_LOG(d));
			sccp_feat_changed(d, NULL, SCCP_FEATURE_MONITOR);
		}

		if (SCCP_CHANNELSTATE_DOWN != c->state && SCCP_CHANNELSTATE_ONHOOK != c->state) {
			sccp_indicate(d, c, SCCP_CHANNELSTATE_ONHOOK);
		}

		/* requesting statistics */
		sccp_channel_StatisticsRequest(c);
		sccp_channel_clean(c);
		return c;								/* returning unretained so that sccp_wrapper_asterisk113_hangup can clear out the last reference */
	}
	return NULL;
}

/*!
 * \brief Asterisk Channel as been answered by remote
 * \note we have no bridged channel at this point
 *
 * \param channel SCCCP channel
 * \return Success as int
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 *
 * \todo masquarade does not succeed when forwarding to a dialplan extension which starts with PLAYBACK (Is this still the case, i think this might have been resolved ?? - DdG -)
 */
int sccp_pbx_remote_answer(constChannelPtr channel)
{
	int res = -1;

	/* \todo perhaps we should lock channel here. */
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	if(!c || !c->owner) {
		return res;
	}
	sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_2 "%s: (%s) Remote end has answered the call.\n", c->designator, __func__);

	sccp_channel_stop_schedule_cfwd_noanswer(c);

	// sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: sccp_pbx_answer checking parent channel\n", c->currentDeviceId);
	if (c->parentChannel) {										// containing a retained channel, final release at the end
		/* we are a forwarded call, bridge me with my parent (the forwarded channel will take the place of the forwarder.) */
		sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: (%s) handling forwarded call\n", c->designator, __func__);

		pbx_channel_lock(c->parentChannel->owner);
		PBX_CHANNEL_TYPE * forwarder = pbx_channel_ref(c->parentChannel->owner);
		pbx_channel_unlock(c->parentChannel->owner);

		pbx_channel_lock(c->owner);
		PBX_CHANNEL_TYPE * tmp_channel = pbx_channel_ref(c->owner);
		pbx_channel_unlock(c->owner);

		const char * destinationChannelName = pbx_builtin_getvar_helper(tmp_channel, CS_BRIDGEPEERNAME);

		PBX_CHANNEL_TYPE * destination = NULL;
		if(sccp_strlen_zero(destinationChannelName) || !iPbx.getChannelByName(destinationChannelName, &destination)) {
			pbx_log(LOG_NOTICE, "%s: (%s) Could not retrieve channel for destination: %s", c->designator, __func__, destinationChannelName);
			return -2;
		}
		/*
		if (iPbx.getChannelAppl(c)) {
			sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_answer) %s bridging to dialplan application %s\n", c->currentDeviceId, iPbx.getChannelName(c), iPbx.getChannelAppl(c));
		}
		*/
		sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "\n"
							"\tendpoint1           | bridge             | endpoint2          | comment\n"
							"\t=================== | ================== | ================== | =================\n"
							"\t%-20.20s| primary_call       |%20.20s| creates tmp_channel in sccp_pbx_call to call forwarding destination\n"
							"\t%-20.20s| temp_bridge        |%20.20s| masquerade on answer of endpoint 2\n"
							"\t------------------- | ------------------ | ------------------ | <-- masquerade temp_bridge:endpoint2 -> primary_call:endpoint2\n"
							"\t%-20.20s| primary call       |%20.20s| after masquerading, hangup temp_bridge:temp_channel\n",
				       "incoming", pbx_channel_name(forwarder), pbx_channel_name(tmp_channel), destinationChannelName, "incoming", destinationChannelName);
		do {
			// retrieve channel by name which will replace in the forwarded channel
			sccp_channel_release(&c->parentChannel);
			if(destination) {
				sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: (%s) handling forwarded call. Replace %s with %s.\n", c->designator, __func__, pbx_channel_name(forwarder), pbx_channel_name(destination));
#if ASTERISK_VERSION_GROUP < 116
				/* set the channel and the bridge to state UP to fix problem with fast pickup / autoanswer */
				pbx_setstate(tmp_channel, AST_STATE_UP);
				pbx_setstate(destination, AST_STATE_UP);
#endif
				if(!iPbx.masqueradeHelper(destination, forwarder)) {
					pbx_log(LOG_ERROR, "%s: (%s) Failed to masquerade bridge into forwarded channel\n", c->designator, __func__);
					if(destination) {
						pbx_channel_unref(destination);
					}
					res = -3;
					break;
				}
#if ASTERISK_VERSION_GROUP > 106
				// Note: destination has taken the place of forwarder
				pbx_indicate(forwarder, AST_CONTROL_CONNECTED_LINE);
#endif
				sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_4 "%s: (%s) Masqueraded into %s\n", c->designator, __func__, pbx_channel_name(forwarder));
				res = 0;
			} else {
				pbx_log(LOG_ERROR, "%s: (%s) Could not retrieve forwarding channel by name:%s: -> Hangup\n", c->designator, __func__, destinationChannelName);
				if(pbx_channel_state(tmp_channel) == AST_STATE_RING && pbx_channel_state(forwarder) == AST_STATE_DOWN && iPbx.getChannelPbx(c)) {
					sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "SCCP: Receiver Hungup: (hasPBX: %s)\n", iPbx.getChannelPbx(c) ? "yes" : "no");
					pbx_channel_set_hangupcause(forwarder, AST_CAUSE_CALL_REJECTED);
				} else {
					pbx_log(LOG_ERROR, "%s: (%s) We did not find bridge channel for call forwarding call. Hangup\n", c->currentDeviceId, __func__);
					pbx_channel_set_hangupcause(forwarder, AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
					sccp_channel_endcall(c);
				}
				pbx_channel_set_hangupcause(forwarder, AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
				pbx_channel_unref(forwarder);
				if(destination) {
					pbx_channel_unref(destination);
				}
				res = -4;
			}
		} while(0);
		if(tmp_channel) {
			pbx_channel_unref(tmp_channel);
		}
	} else {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (%s) Outgoing call %s being answered by remote party\n", c->currentDeviceId, __func__, iPbx.getChannelName(c));
		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
		if (d) {
#if CS_SCCP_CONFERENCE
			sccp_indicate(d, c, d->conference ? SCCP_CHANNELSTATE_CONNECTEDCONFERENCE : SCCP_CHANNELSTATE_CONNECTED);
#else
			sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
#endif
			/** check for monitor request */
			if((d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) && !(d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
				pbx_log(LOG_NOTICE, "%s: (%s) request monitor/record\n", d->id, __func__);
				sccp_feat_monitor(d, NULL, 0, c);
			}

			sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "%s: (%s) Switching to STATE_UP\n", c->designator, __func__);
			iPbx.set_callstate(c, AST_STATE_UP);
			res = 0;
		}

		if((sccp_rtp_getState(&c->rtp.video, SCCP_RTP_RECEPTION) & SCCP_RTP_STATUS_ACTIVE)) {
			iPbx.queue_control(c->owner, AST_CONTROL_VIDUPDATE);
		}
	}
	return res;
}

/*!
 * \brief Allocate an Asterisk Channel
 * \param channel SCCP Channel
 * \param ids Void Character Pointer (either Empty / LinkedId / Channel ID, depending on the asterisk version)
 * \param parentChannel SCCP Channel for which the channel was created
 * \return 1 on Success as uint8_t
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *  - usecnt_lock
 */
boolean_t sccp_pbx_channel_allocate(constChannelPtr channel, const void * ids, const PBX_CHANNEL_TYPE * parentChannel)
{
	PBX_CHANNEL_TYPE * tmp = NULL;
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	AUTO_RELEASE(sccp_device_t, d , NULL);

	if (!c) {
		return FALSE;
	}
	pbx_assert(c->owner == NULL);										// prevent calling this function when the channel already has a pbx channel
	
#ifndef CS_AST_CHANNEL_HAS_CID
	char cidtmp[256];

	memset(&cidtmp, 0, sizeof(cidtmp));
#endif														// CS_AST_CHANNEL_HAS_CID

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(c->line));
	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_channel_allocate) Unable to find line for channel %s\n", c->designator);
		pbx_log(LOG_ERROR, "SCCP: Unable to allocate asterisk channel... returning 0\n");
		return FALSE;
	}

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (pbx_channel_allocate) try to allocate %s channel on line: %s\n", skinny_calltype2str(c->calltype), l->name);
	/* Don't hold a sccp pvt lock while we allocate a channel */
	char s1[512];

	char s2[512];

	char cid_name[StationMaxNameSize] = {0};
	char cid_num[StationMaxDirnumSize] = {0};
	{
		sccp_linedevice_t * ld = NULL;
		// if ((d = sccp_channel_getDevice(c))) {
		d = sccp_channel_getDevice(c) /*ref_replace*/;
		if(d) {
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
				if(ld->device == d) {
					break;
				}
			}
			SCCP_LIST_UNLOCK(&l->devices);
		} else if(SCCP_LIST_GETSIZE(&l->devices) > 0) {
			SCCP_LIST_LOCK(&l->devices);
			ld = SCCP_LIST_FIRST(&l->devices);
			SCCP_LIST_UNLOCK(&l->devices);
			if(ld && ld->device) {
				d = sccp_device_retain(ld->device) /*ref_replace*/;                                        // ugly hack just picking the first one !
			}
		}

		if(!ld) {
			pbx_log(LOG_NOTICE, "%s: Could not find an appropriate ld to assign this channel to. Line:%s exists, but was not assigned to any device (yet). We should give up here.\n", c->designator, l->name);
			goto error_exit;
		}

		sccp_callinfo_t *ci = sccp_channel_getCallInfo(c);
		if(ld->subscriptionId.replaceCid) {
			snprintf(cid_num, StationMaxDirnumSize, "%s", sccp_strlen_zero(ld->subscriptionId.number) ? l->cid_num : ld->subscriptionId.number);
			snprintf(cid_name, StationMaxNameSize, "%s", sccp_strlen_zero(ld->subscriptionId.name) ? l->cid_name : ld->subscriptionId.name);
		} else {
			snprintf(cid_num, StationMaxDirnumSize, "%s%s", l->cid_num, sccp_strlen_zero(ld->subscriptionId.number) ? "" : ld->subscriptionId.number);
			snprintf(cid_name, StationMaxNameSize, "%s%s", l->cid_name, sccp_strlen_zero(ld->subscriptionId.name) ? "" : ld->subscriptionId.name);
		}
		switch (c->calltype) {
			case SKINNY_CALLTYPE_INBOUND:
				iCallInfo.Setter(ci, SCCP_CALLINFO_CALLEDPARTY_NAME, &cid_name, SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cid_num, SCCP_CALLINFO_KEY_SENTINEL);
				break;
			case SKINNY_CALLTYPE_FORWARD:
				iCallInfo.Setter(ci,
					SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name,
					SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &cid_name,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &cid_num,
					SCCP_CALLINFO_LAST_REDIRECT_REASON, 4,
					SCCP_CALLINFO_KEY_SENTINEL);
				break;
			case SKINNY_CALLTYPE_OUTBOUND:
				iCallInfo.Setter(ci, 
					SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name, 
					SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num, 
					//SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, &cid_name,
					//SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, &cid_num,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &cid_name,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &cid_num,
					SCCP_CALLINFO_LAST_REDIRECT_REASON, 0,
					SCCP_CALLINFO_KEY_SENTINEL);
				break;
			case SKINNY_CALLTYPE_SENTINEL:
				break;
		}

		// make sure preferences only contains the codecs that this channel is capable of
		if (SCCP_LIST_GETSIZE(&l->devices) == 1 && d) {				// singleline
			//sccp_line_copyCodecSetsFromLineToChannel(l, d, c);
			sccp_codec_reduceSet(c->preferences.audio, d->capabilities.audio);
			sccp_codec_reduceSet(c->preferences.video, d->capabilities.video);
		} else {
			//sccp_line_copyCodecSetsFromLineToChannel(l, NULL, c);		// sharedline
			sccp_codec_reduceSet(c->preferences.audio, c->capabilities.audio);
			sccp_codec_reduceSet(c->preferences.video, c->capabilities.video);
		}

		if (c->preferences.audio[0] == SKINNY_CODEC_NONE || c->capabilities.audio[0] == SKINNY_CODEC_NONE) {
			pbx_log(LOG_ERROR, "%s: Expect trouble ahead.\n"
				"The audio preferences:%s of this channel have been reduced to nothing.\n"
				"Because they are not compatible with this %s capabilities:%s.\n"
				"Please fix your config. Ending Call !.\n",
				c->designator, 
				sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->preferences.audio, SKINNY_MAX_CAPABILITIES),
				l->preferences_set_on_line_level ? "line's" : "device's",
				sccp_codec_multiple2str(s2, sizeof(s2) - 1, c->capabilities.audio, SKINNY_MAX_CAPABILITIES));
			goto error_exit;
		}
	}
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:             cid_num: %s\n", cid_num);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:            cid_name: %s\n", cid_name);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:         accountcode: %s\n", l->accountcode);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:               exten: %s\n", c->dialedNumber);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:             context: %s\n", l->context);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:            amaflags: %d\n", (int)l->amaflags);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:           chan/call: %s\n", c->designator);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: combined audio caps: %s\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->capabilities.audio, SKINNY_MAX_CAPABILITIES));
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: reduced audio prefs: %s\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->preferences.audio, SKINNY_MAX_CAPABILITIES));
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: combined video caps: %s\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->capabilities.video, SKINNY_MAX_CAPABILITIES));
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: reduced video prefs: %s\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->preferences.video, SKINNY_MAX_CAPABILITIES));
/*
	// this should not be done here at this moment, leaving it to alloc_pbxChannel to sort out.
	if (c->calltype == SKINNY_CALLTYPE_INBOUND) {
		if (c->remoteCapabilities.audio[0] != SKINNY_CODEC_NONE) {
			sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:   remote audio prefs: \"%s\"\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->remoteCapabilities.audio, SKINNY_MAX_CAPABILITIES));
			skinny_codec_t ordered_audio_prefs[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONE};
			memcpy(&ordered_audio_prefs, c->remoteCapabilities.audio, sizeof(ordered_audio_prefs));
			sccp_codec_reduceSet(ordered_audio_prefs, c->preferences.audio);
			memcpy(&c->preferences.audio, ordered_audio_prefs, sizeof(c->preferences.audio));
			sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:      set audio prefs: \"%s\"\n", sccp_codec_multiple2str(s2, sizeof(s2) - 1, c->preferences.audio, SKINNY_MAX_CAPABILITIES));
		}

		if (c->remoteCapabilities.video[0] != SKINNY_CODEC_NONE && (d ? sccp_device_isVideoSupported(d) : TRUE)) {
			sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:   remote video prefs: \"%s\"\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->remoteCapabilities.video, SKINNY_MAX_CAPABILITIES));
			skinny_codec_t ordered_video_prefs[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONE};
			memcpy(&ordered_video_prefs, c->remoteCapabilities.video, sizeof(ordered_video_prefs));
			sccp_codec_reduceSet(ordered_video_prefs, c->preferences.video);
			memcpy(&c->preferences.video, ordered_video_prefs, sizeof(c->preferences.video));
			sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:      set video prefs: \"%s\"\n", sccp_codec_multiple2str(s2, sizeof(s2) - 1, c->preferences.video, SKINNY_MAX_CAPABILITIES));
		}
	}
*/

	/* This should definitely fix CDR */
	iPbx.alloc_pbxChannel(c, ids, parentChannel, &tmp);

	if (!tmp || !c->owner) {
		pbx_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", c->designator, l->name);
		goto error_exit;
	}
	//sccp_channel_updateChannelCapability(c);
	//iPbx.set_nativeAudioFormats(c, c->preferences.audio, 1);
	//iPbx.set_nativeAudioFormats(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));
	iPbx.setChannelName(c, c->designator);

	// \todo: Bridge?
	// \todo: Transfer?

	(void) ATOMIC_INCR(&GLOB(usecnt), 1, &GLOB(usecnt_lock));

	pbx_update_use_count();

	if (iPbx.set_callerid_number) {
		iPbx.set_callerid_number(c->owner, cid_num);
	}
	if (iPbx.set_callerid_ani) {
		iPbx.set_callerid_ani(c->owner, cid_num);
	}
	if (iPbx.set_callerid_name) {
		iPbx.set_callerid_name(c->owner, cid_name);
	}

	/* call ast_channel_call_forward_set with the forward destination if this device is forwarded */
	if (SCCP_LIST_GETSIZE(&l->devices) == 1) {
		sccp_linedevice_t * ld = NULL;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
			if(ld->line == l) {
				if(ld->cfwd[SCCP_CFWD_ALL].enabled) {
					sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: ast call forward channel_set: %s\n", c->designator, ld->cfwd[SCCP_CFWD_ALL].number);
					iPbx.setChannelCallForward(c, ld->cfwd[SCCP_CFWD_ALL].number);
				} else if(ld->cfwd[SCCP_CFWD_BUSY].enabled && (sccp_device_getDeviceState(ld->device) != SCCP_DEVICESTATE_ONHOOK || sccp_device_getActiveAccessory(ld->device))) {
					sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: ast call forward channel_set: %s\n", c->designator, ld->cfwd[SCCP_CFWD_BUSY].number);
					iPbx.setChannelCallForward(c, ld->cfwd[SCCP_CFWD_BUSY].number);
				}
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}

#if CS_SCCP_VIDEO
	const char *VideoStr = pbx_builtin_getvar_helper(c->owner, "SCCP_VIDEO_MODE");
	if (VideoStr && !sccp_strlen_zero(VideoStr)) {
		sccp_channel_setVideoMode(c, VideoStr);
	}
#endif
	/* asterisk needs the native formats bevore dialout, otherwise the next channel gets the whole AUDIO_MASK as requested format
	 * chan_sip dont like this do sdp processing */
	//iPbx.set_nativeAudioFormats(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));

	/* start audio rtp server early, to facilitate choosing codecs via sdp */
	if (d) {
		if (c->calltype == SKINNY_CALLTYPE_OUTBOUND) {
			if (!c->rtp.audio.instance && !sccp_rtp_createServer(d, c, SCCP_RTP_AUDIO)) {
				pbx_log(LOG_WARNING, "%s: Error opening RTP instance for channel %s\n", d->id, c->designator);
				goto error_exit;
			}
			/*
			#if CS_SCCP_VIDEO
						if (sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF && sccp_device_isVideoSupported(d) && c->preferences.video[0] != SKINNY_CODEC_NONE && !c->rtp.video.instance &&
			!sccp_rtp_createServer(d, c, SCCP_RTP_VIDEO)) { pbx_log(LOG_WARNING, "%s: Error opening VRTP instance for channel %s\n", d->id, c->designator); sccp_channel_setVideoMode(c, "off");
						}
			#endif
			*/
		}
		// export sccp informations in asterisk dialplan
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_MAC", d->id);
		struct sockaddr_storage sas = { 0 };
		sccp_session_getSas(d->session, &sas);
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_IP", d->session ? sccp_netsock_stringify_addr(&sas) : "");
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_TYPE", skinny_devicetype2str(d->skinny_type));
	}
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s\n", (l) ? l->id : "(null)", c->designator);

	return TRUE;

error_exit:
	if(c) {
		pbx_log(LOG_WARNING, "%s: (pbx_channel_allocate) Unable to allocate a new channel for line %s\n -> Hanging up call.", DEV_ID_LOG(d), l->name);
		if(c->owner) {
			if(d) {
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
			}
			sccp_channel_endcall(c);
		} else {
			if(d) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
			}
			if(c->line) {
				sccp_line_removeChannel(c->line, c);
			}
			sccp_channel_clean(c);
			sccp_channel_release(&c);
		}
	}
	return FALSE;
}

/*!
 * \brief Schedule Asterisk Dial
 * \param data Data as constant
 * \return Success as int
 *
 * \called_from_asterisk
 */
int sccp_pbx_sched_dial(const void * data)
{
	AUTO_RELEASE(sccp_channel_t, channel, sccp_channel_retain(data));
	if(channel) {
		if ((ATOMIC_FETCH(&channel->scheduler.deny, &channel->scheduler.lock) == 0) && channel->scheduler.hangup_id == -1) {
			channel->scheduler.digittimeout_id = -3;	/* prevent further digittimeout scheduling */
			if (channel->owner && !iPbx.getChannelPbx(channel) && !sccp_strlen_zero(channel->dialedNumber)) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Timeout for call '%s'. Going to dial '%s'\n", channel->designator, channel->dialedNumber);
				sccp_pbx_softswitch(channel);
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Timeout for call '%s'. Nothing to dial -> INVALIDNUMBER\n", channel->designator);
				channel->dialedNumber[0] = '\0';
				sccp_indicate(NULL, channel, SCCP_CHANNELSTATE_INVALIDNUMBER);
			}
		}
		sccp_channel_release((sccp_channel_t **)&data);	// release channel retained in scheduled event
	}
	return 0;						// return 0 to release schedule !
}

/*!
 * \brief Asterisk Helper
 * \param c SCCP Channel as sccp_channel_t
 * \return Success as int
 */
sccp_extension_status_t sccp_pbx_helper(constChannelPtr c)
{
	sccp_extension_status_t extensionStatus = 0;
	int dialedLen = sccp_strlen(c->dialedNumber);

	if (dialedLen > 1) {
		if (GLOB(recorddigittimeoutchar) && GLOB(digittimeoutchar) == c->dialedNumber[dialedLen - 1]) {
			/* we finished dialing with digit timeout char */
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: We finished dialing with digit timeout char %s\n", c->designator, c->dialedNumber);
			return SCCP_EXTENSION_EXACTMATCH;
		}
	}

	if ((c->softswitch_action != SCCP_SOFTSWITCH_GETCBARGEROOM) && (c->softswitch_action != SCCP_SOFTSWITCH_GETMEETMEROOM)
#ifdef CS_SCCP_CONFERENCE
	    && (c->softswitch_action != SCCP_SOFTSWITCH_GETCONFERENCEROOM)
#endif
	    ) {

		//! \todo check overlap feature status -MC
		extensionStatus = iPbx.extension_status(c);
		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

		if (d) {
			if (((d->overlapFeature.enabled && !extensionStatus) || (!d->overlapFeature.enabled && !extensionStatus))) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: %s Matches More\n", c->designator, c->dialedNumber);
				return SCCP_EXTENSION_MATCHMORE;
			}
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: %s Matches %s\n", c->designator, c->dialedNumber, extensionStatus == SCCP_EXTENSION_EXACTMATCH ? "Exactly" : "More");
		}
		return extensionStatus;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: %s Does Exists\n", c->designator, c->dialedNumber);
	return SCCP_EXTENSION_NOTEXISTS;
}

/*!
 * \brief Handle Soft Switch
 * \param channel SCCP Channel as sccp_channel_t
 * \todo clarify Soft Switch Function
 */
void * sccp_pbx_softswitch(constChannelPtr channel)
{
	PBX_CHANNEL_TYPE * pbx_channel = NULL;
	PBX_VARIABLE_TYPE * v = NULL;

	{
		AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));

		if (!c) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <channel> available. Returning from dial thread.\n");
			goto EXIT_FUNC;
		}
		sccp_channel_stop_schedule_digittimout(c);
		
		/* Reset Enbloc Dial Emulation */
		c->enbloc.deactivate = 0;
		c->enbloc.totaldigittime = 0;
		c->enbloc.totaldigittimesquared = 0;
		c->enbloc.digittimeout = GLOB(digittimeout);

		/* prevent softswitch from being executed twice (Pavel Troller / 15-Oct-2010) */
		if (iPbx.getChannelPbx(c)) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) PBX structure already exists. Dialing instead of starting.\n");
			/* If there are any digits, send them instead of starting the PBX */
			if (!sccp_strlen_zero(c->dialedNumber)) {
				if (iPbx.send_digits) {
					iPbx.send_digits(channel, c->dialedNumber);
				}
				sccp_channel_set_calledparty(c, NULL, c->dialedNumber);
			}
			goto EXIT_FUNC;
		}

		if (!c->owner) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No PBX <channel> available. Returning from dial thread.\n");
			goto EXIT_FUNC;
		}
		pbx_channel = pbx_channel_ref(c->owner);

		/* we should just process outbound calls, let's check calltype */
		if (c->calltype != SKINNY_CALLTYPE_OUTBOUND && c->softswitch_action == SCCP_SOFTSWITCH_DIAL) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) This function is for outbound calls only. Exiting\n");
			goto EXIT_FUNC;
		}

		/* assume d is the channel's device */
		/* does it exists ? */
		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

		if (!d) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> available. Returning from dial thread. Exiting\n");
			goto EXIT_FUNC;
		}

		if (pbx_check_hangup(pbx_channel)) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) Channel already hungup, no need to go any further. Exiting\n");
			sccp_indicate(d, c, SCCP_CHANNELSTATE_ONHOOK);
			goto EXIT_FUNC;
		}

		/* we don't need to check for a device type but just if the device has an id, otherwise back home  -FS */
		if (sccp_strlen_zero(d->id)) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> identifier available. Returning from dial thread. Exiting\n");
			goto EXIT_FUNC;
		}

		AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(c->line));

		if (!l) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <line> available. Returning from dial thread. Exiting\n");
			c->hangupRequest(c);
			goto EXIT_FUNC;
		}
		uint8_t instance = sccp_device_find_index_for_line(d, l->name);

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) New call on line %s\n", DEV_ID_LOG(d), l->name);

		/* assign callerid name and number */
		//sccp_channel_set_callingparty(c, l->cid_name, l->cid_num);

		// we use shortenedNumber but why ???
		// If the timeout digit has been used to terminate the number
		// and this digit shall be included in the phone call history etc (recorddigittimeoutchar is true)
		// we still need to dial the number without the timeout char in the pbx
		// so that we don't dial strange extensions with a trailing characters.
		char shortenedNumber[256] = { '\0' };
		sccp_copy_string(shortenedNumber, c->dialedNumber, sizeof(shortenedNumber));
		unsigned int len = sccp_strlen(shortenedNumber);

		pbx_assert(sccp_strlen(c->dialedNumber) == len);

		if (len > 0 && GLOB(digittimeoutchar) == shortenedNumber[len - 1]) {
			shortenedNumber[len - 1] = '\0';

			// If we don't record the timeoutchar in the logs, we remove it from the sccp channel structure
			// Later, the channel dialed number is used for directories, etc.,
			// and the shortened number is used for dialing the actual call via asterisk pbx.
			if (!GLOB(recorddigittimeoutchar)) {
				c->dialedNumber[len - 1] = '\0';
			}
		}

		/* This will choose what to do */
		switch (c->softswitch_action) {
			case SCCP_SOFTSWITCH_GETFORWARDEXTEN:
				{
				sccp_cfwd_t type = (sccp_cfwd_t)c->ss_data;
				sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Forward %s Extension\n", d->id, sccp_cfwd2str(type));
				if(!sccp_strlen_zero(shortenedNumber)) {
					c->setTone(c, SKINNY_TONE_ZIP, SKINNY_TONEDIRECTION_USER);
					sccp_line_cfwd(l, d, type, shortenedNumber);
				}
					sccp_channel_endcall(c);
					goto EXIT_FUNC;								// leave simple switch without dial
				}
			case SCCP_SOFTSWITCH_ENDCALLFORWARD:
				{
				sccp_cfwd_t type = (sccp_cfwd_t)c->ss_data;
				sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Clear Forward %s\n", d->id, sccp_cfwd2str(type));
				sccp_line_cfwd(l, d, type, NULL);
				switch(type) {
					case SCCP_CFWD_ALL:
						sccp_device_setLamp(d, SKINNY_STIMULUS_FORWARDALL, instance, SKINNY_LAMP_OFF);
						break;
					case SCCP_CFWD_BUSY:
						sccp_device_setLamp(d, SKINNY_STIMULUS_FORWARDBUSY, instance, SKINNY_LAMP_OFF);
						break;
					case SCCP_CFWD_NOANSWER:
						sccp_device_setLamp(d, SKINNY_STIMULUS_FORWARDNOANSWER, instance, SKINNY_LAMP_OFF);
						break;
					case SCCP_CFWD_NONE:
					case SCCP_CFWD_SENTINEL:
					default:
						pbx_log(LOG_ERROR, "%s: (sccp_pbx_softswitch) EndCallForward unknown CFWD_TYPE\n", d->id);
				}
					sccp_channel_endcall(c);
					goto EXIT_FUNC;								// leave simple switch without dial
				}
#ifdef CS_SCCP_PICKUP
			case SCCP_SOFTSWITCH_GETPICKUPEXTEN:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Pickup Extension\n", d->id);
				sccp_dev_clearprompt(d, instance, c->callid);

				if (!sccp_strlen_zero(shortenedNumber)) {
 					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) Asterisk request to pickup exten '%s'\n", shortenedNumber);
					sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_PICKUP, GLOB(digittimeout));
					if (sccp_feat_directed_pickup(d, c, instance, shortenedNumber) == 0) {
						goto EXIT_FUNC;
					}
				}
				sccp_dev_displayprinotify(d, SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
				if (c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_DOWN) {
					c->setTone(c, SKINNY_TONE_BEEPBONK, SKINNY_TONEDIRECTION_USER);
				}
				sccp_channel_schedule_hangup(c, 500);
				goto EXIT_FUNC;									// leave simpleswitch without dial
#endif														// CS_SCCP_PICKUP
#ifdef CS_SCCP_CONFERENCE
			case SCCP_SOFTSWITCH_GETCONFERENCEROOM:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Conference request\n", d->id);
				if (c->owner && !pbx_check_hangup(c->owner)) {
					sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_PROCEED);
					iPbx.set_callstate(channel, AST_STATE_UP);
					if (!d->conference) {
						if (!(d->conference = sccp_conference_create(d, c))) {
							goto EXIT_FUNC;
						}
					} else {
						pbx_log(LOG_NOTICE, "%s: There is already a conference running on this device.\n", DEV_ID_LOG(d));
						sccp_channel_endcall(c);
						goto EXIT_FUNC;
					}
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Start Conference\n", d->id);
					sccp_feat_conference_start(d, instance, c);
				}
				goto EXIT_FUNC;
#endif														// CS_SCCP_CONFERENCE
			case SCCP_SOFTSWITCH_GETMEETMEROOM:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request\n", d->id);
				if (!sccp_strlen_zero(shortenedNumber) && !sccp_strlen_zero(c->line->meetmenum)) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request for room '%s' on extension '%s'\n", d->id, shortenedNumber, c->line->meetmenum);
					if (c->owner && !pbx_check_hangup(c->owner)) {
						pbx_builtin_setvar_helper(c->owner, "SCCP_MEETME_ROOM", shortenedNumber);
					}
					sccp_copy_string(shortenedNumber, c->line->meetmenum, sizeof(shortenedNumber));

					//sccp_copy_string(c->dialedNumber, SKINNY_DISP_CONFERENCE, sizeof(c->dialedNumber));
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Start Meetme Thread\n", d->id);
					sccp_feat_meetme_start(c);						/* Copied from Federico Santulli */
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme Thread Started\n", d->id);
					goto EXIT_FUNC;
				} else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
					goto EXIT_FUNC;
				}
				break;
			case SCCP_SOFTSWITCH_GETBARGEEXTEN:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Barge Extension\n", d->id);
				sccp_dev_clearprompt(d, instance, c->callid);
				if (!sccp_strlen_zero(shortenedNumber)) {
 					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) Asterisk request to barge exten '%s'\n", shortenedNumber);
					sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_BARGE, GLOB(digittimeout));
					if (sccp_feat_singleline_barge(c, shortenedNumber)) {
						//sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
						goto EXIT_FUNC;
					}
				}
				sccp_dev_displayprinotify(d, SKINNY_DISP_FAILED_TO_SETUP_BARGE, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
				if (c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_DOWN) {
					c->setTone(c, SKINNY_TONE_BEEPBONK, SKINNY_TONEDIRECTION_USER);
				}
				sccp_channel_endcall(c);
				goto EXIT_FUNC;									// leave simpleswitch without dial
			case SCCP_SOFTSWITCH_GETCBARGEROOM:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Conference Barge Extension\n", d->id);
				// like we're dialing but we're not :)
				sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_channel_send_callinfo(d, c);
				sccp_dev_clearprompt(d, instance, c->callid);
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, GLOB(digittimeout));
				if (!sccp_strlen_zero(shortenedNumber)) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge conference '%s'\n", d->id, shortenedNumber);
					if (sccp_feat_cbarge(c, shortenedNumber)) {
						sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
					}
				} else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
				}
				goto EXIT_FUNC;									// leave simpleswitch without dial
			case SCCP_SOFTSWITCH_SENTINEL:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Unknown Softswitch Request\n", d->id);
				goto EXIT_FUNC;
			case SCCP_SOFTSWITCH_DIAL:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Dial Extension %s\n", d->id, shortenedNumber);
				sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
				/* fall through */
		}

		/* set private variable */
		if (pbx_channel && !pbx_check_hangup(pbx_channel)) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) set variable SKINNY_PRIVATE to: %s\n", c->privacy ? "1" : "0");
			if (c->privacy) {
				sccp_channel_set_calleridPresentation(c, CALLERID_PRESENTATION_FORBIDDEN);
			}

			uint32_t result = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

			result |= c->privacy;
			if (d->privacyFeature.enabled && result) {
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) set variable SKINNY_PRIVATE to: %s\n", "1");
				pbx_builtin_setvar_helper(pbx_channel, "SKINNY_PRIVATE", "1");
			} else {
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) set variable SKINNY_PRIVATE to: %s\n", "0");
				pbx_builtin_setvar_helper(pbx_channel, "SKINNY_PRIVATE", "0");
			}
		}

		/* set devicevariables */
		v = d->variables;
		while (pbx_channel && !pbx_check_hangup(pbx_channel) && d && v) {
			pbx_builtin_setvar_helper(pbx_channel, v->name, v->value);
			v = v->next;
		}

		/* set linevariables */
		v = l->variables;
		while (pbx_channel && !pbx_check_hangup(pbx_channel) && l && v) {
			pbx_builtin_setvar_helper(pbx_channel, v->name, v->value);
			v = v->next;
		}

		iPbx.setChannelExten(c, shortenedNumber);

		/*! \todo DdG: Extra wait time is incurred when checking pbx_exists_extension, when a wrong number is dialed. storing extension_exists status for sccp_log use */
		int extension_exists = SCCP_EXTENSION_NOTEXISTS;

		if (!sccp_strlen_zero(shortenedNumber) && ((extension_exists = iPbx.extension_status(c) != SCCP_EXTENSION_NOTEXISTS))
		    ) {
			if (pbx_channel && !pbx_check_hangup(pbx_channel)) {
				/* found an extension, let's dial it */
				sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s is dialing number %s\n", DEV_ID_LOG(d), c->designator, shortenedNumber);

				/* Answer dialplan command works only when in RINGING OR RING ast_state */
				iPbx.set_callstate(c, AST_STATE_RING);

				enum ast_pbx_result pbxStartResult = pbx_pbx_start(pbx_channel);

				/* \todo replace AST_PBX enum using pbx_impl wrapper enum */
				switch (pbxStartResult) {
					case AST_PBX_FAILED:
						pbx_log(LOG_ERROR, "%s: (sccp_pbx_softswitch) channel %s failed to start new thread to dial %s\n", DEV_ID_LOG(d), c->designator, shortenedNumber);
						/* \todo change indicate to something more suitable */
						sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);		/* will auto hangup after SCCP_HANGUP_TIMEOUT */
						break;
					case AST_PBX_CALL_LIMIT:
						pbx_log(LOG_WARNING, "%s: (sccp_pbx_softswitch) call limit reached for channel %s-%08x failed to start new thread to dial %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
						sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);		/* will auto hangup after SCCP_HANGUP_TIMEOUT */
						break;
					case AST_PBX_SUCCESS:
						sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) pbx started\n", DEV_ID_LOG(d));
#ifdef CS_MANAGER_EVENTS
						if (GLOB(callevents)) {
							manager_event(EVENT_FLAG_SYSTEM, "ChannelUpdate", "Channel: %s\r\nUniqueid: %s\r\nChanneltype: %s\r\nSCCPdevice: %s\r\nSCCPline: %s\r\nSCCPcallid: %08X\r\nSCCPCallDesignator: %s\r\n", 
								(pbx_channel) ? pbx_channel_name(pbx_channel) : "(null)", 
								(pbx_channel) ? pbx_channel_uniqueid(pbx_channel) : "(null)", 
								"SCCP", 
								(d) ? d->id : "(null)", 
								(l) ? l->name : "(null)", 
								(c && c->callid) ? c->callid : 0,
								(c) ? c->designator : "(null)");
						}
#endif														// CS_MANAGER_EVENTS
						break;
				}
				AUTO_RELEASE(sccp_linedevice_t, ld, c->getLineDevice(c));
				if(ld) {
					sccp_device_setLastNumberDialed(d, shortenedNumber, ld);
				}
				if(iPbx.set_dialed_number) {
					iPbx.set_dialed_number(c, shortenedNumber);
				}
			} else {
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) pbx_check_hangup(chan): %d on line %s\n", DEV_ID_LOG(d), (pbx_channel && pbx_check_hangup(pbx_channel)), l->name);
			}
		} else {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel:%s shortenedNumber:%s, extension exists:%s\n", DEV_ID_LOG(d), c->designator, shortenedNumber, (extension_exists != SCCP_EXTENSION_NOTEXISTS) ? "TRUE" : "FALSE");
			pbx_log(LOG_NOTICE, "%s: Call from '%s' to extension '%s', rejected because the extension could not be found in context '%s'\n", DEV_ID_LOG(d), l->name, shortenedNumber, pbx_channel ? pbx_channel_context(pbx_channel) : "pbx_channel==NULL");
			/* timeout and no extension match */
			if (pbx_channel && !pbx_check_hangup(pbx_channel)) {
				pbx_log(LOG_NOTICE, "%s: Scheduling Hangup of Call: %s\n", DEV_ID_LOG(d), c->designator);
				sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);				/* will auto hangup after SCCP_HANGUP_TIMEOUT */
			}
		}

		sccp_log((DEBUGCAT_PBX + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) finished\n", DEV_ID_LOG(d));
	}
EXIT_FUNC:
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "SCCP: (sccp_pbx_softswitch) exit\n");
	if (pbx_channel) {
		pbx_channel_unref(pbx_channel);
	}
	return NULL;
}

#if 0
/*!
 * \brief Handle Dialplan Transfer
 *
 * This will allow asterisk to transfer an SCCP Channel via the dialplan transfer function
 *
 * \param ast Asterisk Channel
 * \param dest Destination as char *
 * \return result as int
 *
 * \test Dialplan Transfer Needs to be tested
 * \todo pbx_transfer needs to be implemented correctly
 *
 * \called_from_asterisk
 */
int sccp_pbx_transfer(PBX_CHANNEL_TYPE * ast, const char *dest)
{
	int res = 0;

	if (dest == NULL) {											/* functions below do not take a NULL */
		dest = "";
		return -1;
	}

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));

	if (!c) {
		return -1;
	}

	/*
	   sccp_device_t *d = NULL;
	   sccp_channel_t *newcall = NULL;
	 */

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "Transferring '%s' to '%s'\n", iPbx.getChannelName(c), dest);
	if (pbx_channel_state(ast) == AST_STATE_RING) {
		//! \todo Blindtransfer needs to be implemented correctly

		/*
		   res = sccp_blindxfer(p, dest);
		 */
		res = -1;
	} else {
		//! \todo Transfer needs to be implemented correctly

		/*
		   res=sccp_channel_transfer(p,dest);
		 */
		res = -1;
	}
	return res;
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
