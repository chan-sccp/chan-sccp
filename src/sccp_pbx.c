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
#include "sccp_features.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_netsock.h"
#include "sccp_session.h"
#include "sccp_atomic.h"
#include "sccp_labels.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/callerid.h>
#include <asterisk/module.h>		// ast_update_use_count
#include <asterisk/causes.h>		// AST_CAUSE_NORMAL_CLEARING

/*!
 * \brief SCCP Request Channel
 * \param lineName              Line Name as Char
 * \param requestedCodec        Requested Skinny Codec
 * \param capabilities          Array of Skinny Codec Capabilities
 * \param capabilityLength      Length of Capabilities Array
 * \param autoanswer_type       SCCP Auto Answer Type
 * \param autoanswer_cause      SCCP Auto Answer Cause
 * \param ringermode            Ringer Mode
 * \param channel               SCCP Channel
 * \return SCCP Channel Request Status
 * 
 * \called_from_asterisk
 */
sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel)
{
	sccp_channel_t *my_sccp_channel = NULL;
	AUTO_RELEASE(sccp_line_t, l , NULL);

	if (!lineName) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	char mainId[SCCP_MAX_EXTENSION];
	sccp_subscription_id_t subscriptionId;
	if (sccp_parseComposedId(lineName, 80, &subscriptionId, mainId)) {
		l = sccp_line_find_byname(mainId, FALSE);
	};

	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", mainId);
		return SCCP_REQUEST_STATUS_LINEUNKNOWN;
	}
	sccp_log_and((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (SCCP_RWLIST_GETSIZE(&l->devices) == 0) {
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		return SCCP_REQUEST_STATUS_LINEUNAVAIL;
	}
	sccp_log_and((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	*channel = my_sccp_channel = sccp_channel_allocate(l, NULL);

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

	uint8_t size = (capabilityLength < sizeof(my_sccp_channel->remoteCapabilities.audio)) ? capabilityLength : sizeof(my_sccp_channel->remoteCapabilities.audio);

	memset(&my_sccp_channel->remoteCapabilities.audio, 0, sizeof(my_sccp_channel->remoteCapabilities.audio));
	memcpy(&my_sccp_channel->remoteCapabilities.audio, capabilities, size);

	/** set requested codec as prefered codec */
	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "prefered audio codec (%d)\n", requestedCodec);
	if (requestedCodec != SKINNY_CODEC_NONE) {
		my_sccp_channel->preferences.audio[0] = requestedCodec;
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "SCCP: prefered audio codec (%d)\n", my_sccp_channel->preferences.audio[0]);
	}

	/** done */

	my_sccp_channel->autoanswer_type = autoanswer_type;
	my_sccp_channel->autoanswer_cause = autoanswer_cause;
	my_sccp_channel->ringermode = ringermode;
	my_sccp_channel->hangupRequest = sccp_wrapper_asterisk_requestQueueHangup;
	return SCCP_REQUEST_STATUS_SUCCESS;
}

/*!
 * \brief SCCP Structure to pass data to the pbx answer thread
 */
struct sccp_answer_conveyor_struct {
	sccp_linedevices_t *linedevice;
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
	struct sccp_answer_conveyor_struct *conveyor = data;

	int instance = 0;

	sleep(GLOB(autoanswer_ring_time));
	pthread_testcancel();

	if (!conveyor) {
		return NULL;
	}
	if (!conveyor->linedevice) {
		goto FINAL;
	}

	{
		AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(conveyor->linedevice->device));

		if (!device) {
			goto FINAL;
		}

		AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(conveyor->callid));

		if (!c || c->state != SCCP_CHANNELSTATE_RINGING) {
			goto FINAL;
		}

		sccp_channel_answer(device, c);

		if (GLOB(autoanswer_tone) != SKINNY_TONE_SILENCE && GLOB(autoanswer_tone) != SKINNY_TONE_NOTONE) {
			instance = sccp_device_find_index_for_line(device, c->line->name);
			sccp_dev_starttone(device, GLOB(autoanswer_tone), instance, c->callid, SKINNY_TONEDIRECTION_USER);
		}
		if (c->autoanswer_type == SCCP_AUTOANSWER_1W) {
			sccp_dev_set_microphone(device, SKINNY_STATIONMIC_OFF);
		}
	}
FINAL:
	if (conveyor->linedevice) {
		sccp_linedevice_release(&conveyor->linedevice);			// retained in calling thread, explicit release required here
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
int sccp_pbx_call(sccp_channel_t * c, char *dest, int timeout)
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

	/* if incoming call limit is reached send BUSY */
	if ((l->incominglimit && SCCP_LIST_GETSIZE(&l->channels) > l->incominglimit)) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Incoming calls limit (%d) reached on SCCP/%s... sending busy\n", l->incominglimit, l->name);
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
	sccp_linedevices_t *ForwardingLineDevice = NULL;

	sccp_linedevices_t *linedevice = NULL;
	sccp_channelstate_t previousstate = c->previousChannelState;

	SCCP_LIST_LOCK(&l->devices);
	c->subscribers = SCCP_LIST_GETSIZE(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		AUTO_RELEASE(sccp_channel_t, active_channel , sccp_device_getActiveChannel(linedevice->device));

		// skip incoming call on a shared line from the originator. (sharedline calling same sharedline)
		if (active_channel && active_channel != c && sccp_strequals(iPbx.getChannelLinkedId(active_channel), iPbx.getChannelLinkedId(c))) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) skip ringing on %s\n", linedevice->device->id);
			c->subscribers--;
			continue;
		}
		/* do we have cfwd enabled? */
		//pbx_log(LOG_NOTICE, "%s: BYPASS_CFWD: %s\n", linedevice->device->id, pbx_builtin_getvar_helper(c->owner, "BYPASS_CFWD"));
		if (sccp_strlen_zero(pbx_builtin_getvar_helper(c->owner, "BYPASS_CFWD"))) {
			if (
				linedevice->cfwdAll.enabled || 
				(linedevice->cfwdBusy.enabled && (sccp_device_getDeviceState(linedevice->device) != SCCP_DEVICESTATE_ONHOOK || sccp_device_getActiveAccessory(linedevice->device)))
			) {
				if (SCCP_LIST_GETSIZE(&l->devices) == 1) {
					/* when single line -> use asterisk functionality directly, without creating new channel + masquerade */
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward active on line %s\n", linedevice->device->id, linedevice->line->name);
					ForwardingLineDevice = linedevice;
				} else {
					/* shared line -> create a temp channel to call forward destination and tie them together */
					pbx_log(LOG_NOTICE, "%s: initialize cfwd%s for line %s\n", linedevice->device->id, (linedevice->cfwdAll.enabled ? "All" : (linedevice->cfwdBusy.enabled ? "Busy" : "None")), l->name);
					if (sccp_channel_forward(c, linedevice, linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : linedevice->cfwdBusy.number) == 0) {
						sccp_device_sendcallstate(linedevice->device, linedevice->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
						sccp_channel_send_callinfo(linedevice->device, c);
						isRinging = TRUE;
					}
				}
				c->subscribers--; 
				continue;
			}
		}

		if (!linedevice->device->session) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: line device has no session\n", DEV_ID_LOG(linedevice->device));
			c->subscribers--;
			continue;
		}

		/* check if c->subscriptionId.number is matching deviceSubscriptionID */
		/* This means that we call only those devices on a shared line
		   which match the specified subscription id in the dial parameters. */
		if (!sccp_util_matchSubscriptionId(c, linedevice->subscriptionId.number)) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: device does not match subscriptionId.number c->subscriptionId.number: '%s', deviceSubscriptionID: '%s'\n", DEV_ID_LOG(linedevice->device), c->subscriptionId.number, linedevice->subscriptionId.number);
			c->subscribers--;
			continue;
		}

		/* reset channel state (because we are offering the same call to multiple (shared) lines)*/
		c->previousChannelState = previousstate;
		if (active_channel) {
			sccp_indicate(linedevice->device, c, SCCP_CHANNELSTATE_CALLWAITING);

			/* display the new call on prompt */
			//AUTO_RELEASE(sccp_linedevices_t, activeChannelLinedevice , sccp_linedevice_find(linedevice->device, active_channel->line));
			AUTO_RELEASE(sccp_linedevices_t, activeChannelLinedevice , active_channel->getLineDevice(active_channel));
			if (activeChannelLinedevice) {
				char caller[100] = {0};
				if (!sccp_strlen_zero(cid_name)) {
					if (!sccp_strlen_zero(cid_num)) {
						snprintf(caller,sizeof(caller), "%s%s (%s)", SKINNY_DISP_CALL_WAITING, cid_name, cid_num);
					} else {
						snprintf(caller,sizeof(caller), "%s%s", SKINNY_DISP_CALL_WAITING, cid_name);
					} 
				} else {
					if (!sccp_strlen_zero(cid_num)) {
						snprintf(caller,sizeof(caller), "%s%s", SKINNY_DISP_CALL_WAITING, cid_num);
					} else {
						snprintf(caller,sizeof(caller), "%s%s", SKINNY_DISP_CALL_WAITING, SKINNY_DISP_UNKNOWN_NUMBER);
					}
				}
				//snprintf(prompt, sizeof(prompt), "%s: %s: %s", active_channel->line->name, SKINNY_DISP_FROM, cid_num);
				sccp_dev_displayprompt(linedevice->device, activeChannelLinedevice->lineInstance, active_channel->callid, caller, GLOB(digittimeout));
			}
			ForwardingLineDevice = NULL;	/* reset cfwd if shared */
			isRinging = TRUE;
		} else {

			/** check if ringermode is not urgent and device enabled dnd in reject mode */
			if (SKINNY_RINGTYPE_URGENT != c->ringermode && linedevice->device->dndFeature.enabled && linedevice->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: DND active on line %s, returning Busy\n", linedevice->device->id, linedevice->line->name);
				hasDNDParticipant = TRUE;
				c->subscribers--;
				continue;
			}
			ForwardingLineDevice = NULL;	/* reset cfwd if shared */
			sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "%s: Ringing %sLine: %s on device:%s using channel:%s, ringermode:%s\n", linedevice->device->id, SCCP_LIST_GETSIZE(&l->devices) > 1 ? "Shared" : "",linedevice->line->name, linedevice->device->id, c->designator, skinny_ringtype2str(c->ringermode));
			sccp_indicate(linedevice->device, c, SCCP_CHANNELSTATE_RINGING);
			isRinging = TRUE;
			if (c->autoanswer_type) {
				struct sccp_answer_conveyor_struct *conveyor = sccp_calloc(1, sizeof(struct sccp_answer_conveyor_struct));
				if (conveyor) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", DEV_ID_LOG(linedevice->device), iPbx.getChannelName(c));
					conveyor->callid = c->callid;
					conveyor->linedevice = sccp_linedevice_retain(linedevice);

					sccp_threadpool_add_work(GLOB(general_threadpool), (void *) sccp_pbx_call_autoanswer_thread, (void *) conveyor);
				} else {
					pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, c->designator);
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	//sccp_log(DEBUGCAT_PBX)(VERBOSE_PREFIX_3 "%s: isRinging:%d, hadDNDParticipant:%d, ForwardingLineDevice:%p\n", c->designator, isRinging, hasDNDParticipant, ForwardingLineDevice);
	if (isRinging) {
		sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_RINGING);
		iPbx.set_callstate(c, AST_STATE_RINGING);
		iPbx.queue_control(c->owner, AST_CONTROL_RINGING);
	} else if (ForwardingLineDevice) {
		/* when single line -> use asterisk functionality directly, without creating new channel + masquerade */
		pbx_log(LOG_NOTICE, "%s: initialize cfwd%s for line %s\n", ForwardingLineDevice->device->id, (ForwardingLineDevice->cfwdAll.enabled ? "All" : (ForwardingLineDevice->cfwdBusy.enabled ? "Busy" : "None")), l->name);
#if CS_AST_CONTROL_REDIRECTING
		iPbx.queue_control(c->owner, AST_CONTROL_REDIRECTING);
#endif
		pbx_channel_call_forward_set(c->owner, ForwardingLineDevice->cfwdAll.enabled ? ForwardingLineDevice->cfwdAll.number : ForwardingLineDevice->cfwdBusy.number);
		sccp_device_sendcallstate(ForwardingLineDevice->device, ForwardingLineDevice->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(ForwardingLineDevice->device, c);
	} else if (hasDNDParticipant) {
		iPbx.queue_control(c->owner, AST_CONTROL_BUSY);
		//iPbx.set_callstate(c, AST_STATE_BUSY);
		pbx_channel_set_hangupcause(c->owner, AST_CAUSE_USER_BUSY);
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

sccp_channel_t * sccp_pbx_hangup(sccp_channel_t * channel)
{

	/* here the ast channel is locked */
	//sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", iPbx.getChannelName(c));
	(void) ATOMIC_DECR(&GLOB(usecnt), 1, &GLOB(usecnt_lock));

	pbx_update_use_count();

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));

	if (!c) {
		sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel. SCCP channel already deleted\n");
		return NULL;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

	if (d && !SCCP_CHANNELSTATE_Idling(c->state) && SKINNY_DEVICE_RS_OK == sccp_device_getRegistrationState(d)) {
		// if (GLOB(remotehangup_tone) && d && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_device_getActiveChannel_nolock(d))	/* Caused active channels never to be full released */
		if (GLOB(remotehangup_tone) && d && SCCP_DEVICESTATE_OFFHOOK == sccp_device_getDeviceState(d) && SCCP_CHANNELSTATE_IsConnected(c->state) && c == d->active_channel) {
			uint16_t instance = sccp_device_find_index_for_line(d, c->line->name);
			sccp_dev_starttone(d, GLOB(remotehangup_tone), instance, c->callid, SKINNY_TONEDIRECTION_USER);
		}
		//sccp_indicate(d, c, SCCP_CHANNELSTATE_ONHOOK);
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

	sccp_channel_closeAllMediaTransmitAndReceive(d, c);

	// removing scheduled dialing
	sccp_channel_stop_schedule_digittimout(c);

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Current channel %s state %s(%d)\n", (d) ? DEV_ID_LOG(d) : "(null)", c->designator, sccp_channelstate2str(c->state), c->state);

	/* end callforwards */
	sccp_channel_end_forwarding_channel(c);

	/* cancel transfer if in progress */
	sccp_channel_transfer_cancel(d, c);

	/* remove call from transferee, transferer */
	sccp_linedevices_t *linedevice = NULL;
	if (l) {
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			AUTO_RELEASE(sccp_device_t, tmpDevice , sccp_device_retain(linedevice->device));

			if (tmpDevice) {
				sccp_channel_transfer_release(tmpDevice, c);					/* explicit release required here */
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
		/* done - remove call from transferee, transferer */

		sccp_line_removeChannel(l, c);

		if (!d) {
			/* channel is not answered, just ringin over all devices */
			/* find the first the device on which it is registered and hangup that one (__sccp_indicate_remote_device will do the rest) */
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
				if (linedevice->device && SKINNY_DEVICE_RS_OK == sccp_device_getRegistrationState(linedevice->device)) {
					d = sccp_device_retain(linedevice->device);
					break;
				}
			}
			SCCP_LIST_UNLOCK(&l->devices);
		}
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
 * \brief Answer an Asterisk Channel
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
int sccp_pbx_answered(sccp_channel_t * channel)
{
	int res = 0;

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: sccp_pbx_answer\n");

	/* \todo perhaps we should lock channel here. */
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));

	if (!c) {
		return -1;
	}

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: sccp_pbx_answer checking parent channel\n", c->currentDeviceId);
	if (c->parentChannel) {											// containing a retained channel, final release at the end
		/* we are a forwarded call, bridge me with my parent */
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: bridge me with my parent's channel %s\n", c->currentDeviceId, iPbx.getChannelName(c));

		PBX_CHANNEL_TYPE *astForwardedChannel = c->parentChannel->owner;
		PBX_CHANNEL_TYPE *br = NULL;

		if (iPbx.getChannelAppl(c)) {
			sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_answer) %s bridging to dialplan application %s\n", c->currentDeviceId, iPbx.getChannelName(c), iPbx.getChannelAppl(c));
		}

		/* at this point we do not have a pointer to our bridge channel so we search for it -MC */
		const char *bridgePeerChannelName = pbx_builtin_getvar_helper(c->owner, CS_BRIDGEPEERNAME);

		if (!sccp_strlen_zero(bridgePeerChannelName)) {
			sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) searching for bridgepeer by name: %s\n", bridgePeerChannelName);
			iPbx.getChannelByName(bridgePeerChannelName, &br);
		}

		/* did we find our bridge */
		pbx_log(LOG_NOTICE, "SCCP: bridge: %s\n", (br) ? pbx_channel_name(br) : " -- no bridgepeer found -- ");
		if (br) {
			/* set the channel and the bridge to state UP to fix problem with fast pickup / autoanswer */
			pbx_setstate(c->owner, AST_STATE_UP);
			pbx_setstate(br, AST_STATE_UP);

			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Going to Masquerade %s into %s\n", pbx_channel_name(br), pbx_channel_name(astForwardedChannel));

			if (iPbx.masqueradeHelper(br, astForwardedChannel)) {
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Masqueraded into %s\n", pbx_channel_name(astForwardedChannel));
				sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: ast %s\n", pbx_state2str(pbx_channel_state(c->owner)));
				sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: astForwardedChannel %s\n", pbx_state2str(pbx_channel_state(astForwardedChannel)));
				sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: br %s\n", pbx_state2str(pbx_channel_state(br)));
				sccp_log_and((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");
#if ASTERISK_VERSION_GROUP > 106
				pbx_indicate(br, AST_CONTROL_CONNECTED_LINE);
#endif
			} else {
				pbx_log(LOG_ERROR, "(sccp_pbx_answer) Failed to masquerade bridge into forwarded channel\n");

				res = -1;
			}
		} else {
			/* we have no bridge and can not make a masquerade -> end call */
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: ast %s\n", pbx_state2str(pbx_channel_state(c->owner)));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: astForwardedChannel %s\n", pbx_state2str(pbx_channel_state(astForwardedChannel)));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");

			pbx_log(LOG_ERROR, "%s: We did not find bridge channel for call forwarding call. Hangup\n", c->currentDeviceId);
			if (pbx_channel_state(c->owner) == AST_STATE_RING && pbx_channel_state(astForwardedChannel) == AST_STATE_DOWN && iPbx.getChannelPbx(c)) {
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "SCCP: Receiver Hungup: (hasPBX: %s)\n", iPbx.getChannelPbx(c) ? "yes" : "no");
				pbx_channel_set_hangupcause(astForwardedChannel, AST_CAUSE_CALL_REJECTED);
				c->parentChannel->hangupRequest(c->parentChannel);
			} else {
				pbx_log(LOG_ERROR, "%s: We did not find bridge channel for call forwarding call. Hangup\n", c->currentDeviceId);
				pbx_channel_set_hangupcause(astForwardedChannel, AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
				c->parentChannel->hangupRequest(c->parentChannel);
				sccp_channel_endcall(c);
				res = -1;
			}
			pbx_channel_set_hangupcause(astForwardedChannel, AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
			res = -1;
		}
		// FINISH
	} else {

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_answer) Outgoing call %s has been answered by remote party\n", c->currentDeviceId, iPbx.getChannelName(c));
		sccp_channel_updateChannelCapability(c);

		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
		if (d) {
			if (d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE) {
				/* 
				* Redial button isnt't working properly in immediate mode, because the
				* last dialed number was being remembered too early. This fix
				* remembers the last dialed number in the same cases, where the dialed number
				* is being sent - after receiving of RINGOUT -Pavel Troller
				*/
				//AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, c->line));
				AUTO_RELEASE(sccp_linedevices_t, linedevice , c->getLineDevice(c));
				if(linedevice){ 
					sccp_device_setLastNumberDialed(d, c->dialedNumber, linedevice);
				}
				if (iPbx.set_dialed_number){
					iPbx.set_dialed_number(c, c->dialedNumber);
				}
			}

			sccp_indicate(d, c, SCCP_CHANNELSTATE_PROCEED);
#if CS_SCCP_CONFERENCE 
			sccp_indicate(d, c, d->conference ? SCCP_CHANNELSTATE_CONNECTEDCONFERENCE : SCCP_CHANNELSTATE_CONNECTED);
#else
			sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
#endif

			/** check for monitor request */
			if (d && (d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) && !(d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
				pbx_log(LOG_NOTICE, "%s: request monitor\n", d->id);
				sccp_feat_monitor(d, NULL, 0, c);
			}
		}

		if (c->rtp.video.receiveChannelState & SCCP_RTP_STATUS_ACTIVE) {
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
uint8_t sccp_pbx_channel_allocate(sccp_channel_t * channel, const void *ids, const PBX_CHANNEL_TYPE * parentChannel)
{
	PBX_CHANNEL_TYPE *tmp;
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	AUTO_RELEASE(sccp_device_t, d , NULL);

	if (!c) {
		return -1;
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
		return 0;
	}

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (pbx_channel_allocate) try to allocate %s channel on line: %s, \n", skinny_calltype2str(c->calltype), l->name);
	/* Don't hold a sccp pvt lock while we allocate a channel */

	char cid_name[StationMaxNameSize] = {0};
	char cid_num[StationMaxDirnumSize] = {0};
	{
		sccp_linedevices_t *linedevice = NULL;
		if ((d = sccp_channel_getDevice(c))) {
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
				if (linedevice->device == d) {
					break;
				}
			}
			SCCP_LIST_UNLOCK(&l->devices);
		} else if (SCCP_LIST_GETSIZE(&l->devices) == 1) {
			SCCP_LIST_LOCK(&l->devices);
			linedevice = SCCP_LIST_FIRST(&l->devices);
			d = sccp_device_retain(linedevice->device);
			SCCP_LIST_UNLOCK(&l->devices);
		}
		sccp_callinfo_t *ci = sccp_channel_getCallInfo(c);
		
		if (linedevice) {
			if (linedevice->subscriptionId.replaceCid) {
				snprintf(cid_num, StationMaxDirnumSize, "%s", sccp_strlen_zero(linedevice->subscriptionId.number) ? l->cid_num : linedevice->subscriptionId.number);
				snprintf(cid_name, StationMaxNameSize, "%s", sccp_strlen_zero(linedevice->subscriptionId.name) ? l->cid_name : linedevice->subscriptionId.name);
			} else {
				snprintf(cid_num, StationMaxDirnumSize, "%s%s", l->cid_num, sccp_strlen_zero(linedevice->subscriptionId.number) ? "" : linedevice->subscriptionId.number);
				snprintf(cid_name, StationMaxNameSize, "%s%s", l->cid_name, sccp_strlen_zero(linedevice->subscriptionId.name) ? "" : linedevice->subscriptionId.name);
			}
		}
		switch (c->calltype) {
			case SKINNY_CALLTYPE_INBOUND:
				iCallInfo.Setter(ci, 
					SCCP_CALLINFO_CALLEDPARTY_NAME, &cid_name, 
					SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cid_num,  
					SCCP_CALLINFO_KEY_SENTINEL);
				break;
			case SKINNY_CALLTYPE_FORWARD:
			case SKINNY_CALLTYPE_OUTBOUND:
				iCallInfo.Setter(ci, 
					SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name, 
					SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num, 
					//SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, &cid_num,
					//SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, &cid_name,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &cid_num,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &cid_name,
					SCCP_CALLINFO_LAST_REDIRECT_REASON, 0,
					SCCP_CALLINFO_KEY_SENTINEL);
				break;
			case SKINNY_CALLTYPE_SENTINEL:
				break;
		}
		if (d) {
			memcpy(&c->capabilities.audio, &d->capabilities.audio, sizeof(c->capabilities.audio));
			memcpy(&c->capabilities.video, &d->capabilities.video, sizeof(c->capabilities.video));
			memcpy(&c->preferences.audio , &d->preferences.audio , sizeof(c->preferences.audio));
			memcpy(&c->preferences.video , &d->preferences.video , sizeof(c->preferences.video));
		} else {			/* shared line */
			/* \todo we should be doing this when a device is attached to a line, and store the caps/prefs inside the sccp_line_t */
			/* \todo it would be nice if we could set audio preferences by line instead of only per device, especially in case of shared line */
			sccp_line_copyCodecSetsFromLineToChannel(l, c);
		}
		// make sure preferences only contains the codecs that this channel is capable of
		sccp_codec_reduceSet(c->preferences.audio , c->capabilities.audio);
		sccp_codec_reduceSet(c->preferences.video , c->capabilities.video);
	}
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:              cid_num: \"%s\"\n", cid_num);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:             cid_name: \"%s\"\n", cid_name);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:          accountcode: \"%s\"\n", l->accountcode);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:                exten: \"%s\"\n", c->dialedNumber);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:              context: \"%s\"\n", l->context);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:             amaflags: \"%d\"\n", l->amaflags);
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:            chan/call: \"%s\"\n", c->designator);
	char s1[512], s2[512];
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:combined capabilities: \"%s\"\n", sccp_codec_multiple2str(s1, sizeof(s1) - 1, channel->capabilities.audio, SKINNY_MAX_CAPABILITIES));
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:  reduced preferences: \"%s\"\n", sccp_codec_multiple2str(s2, sizeof(s2) - 1, channel->preferences.audio, SKINNY_MAX_CAPABILITIES));

	/* This should definitely fix CDR */
	iPbx.alloc_pbxChannel(c, ids, parentChannel, &tmp);
	if (!tmp || !c->owner) {
		pbx_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", c->designator, l->name);
		return 0;
	}
       	sccp_channel_updateChannelCapability(c);
	//iPbx.set_nativeAudioFormats(c, c->preferences.audio, 1);
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
		sccp_linedevices_t *linedevice = NULL;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice->line == l) {
				if (linedevice->cfwdAll.enabled) {
					sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: ast call forward channel_set: %s\n", c->designator, linedevice->cfwdAll.number);
					iPbx.setChannelCallForward(c, linedevice->cfwdAll.number);
				} else if (linedevice->cfwdBusy.enabled && (sccp_device_getDeviceState(linedevice->device) != SCCP_DEVICESTATE_ONHOOK || sccp_device_getActiveAccessory(linedevice->device))) {
					sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: ast call forward channel_set: %s\n", c->designator, linedevice->cfwdBusy.number);
					iPbx.setChannelCallForward(c, linedevice->cfwdBusy.number);
				}
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}
#if 0
	{

		/* (shared line version) call ast_channel_call_forward_set if all devices for this line are forwarded. Send the first forward destination to PBX */
		sccp_linedevices_t *linedevice = NULL;
		int numdevices = SCCP_LIST_GETSIZE(&l->devices);
		int numforwards = 0;
		char cfwdnum[SCCP_MAX_EXTENSION] = "";

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice->line == l && (linedevice->cfwdAll.enabled || (linedevice->cfwdBusy.enabled && (sccp_device_getDeviceState(linedevice->device) != SCCP_DEVICESTATE_ONHOOK || sccp_device_getActiveAccessory(linedevice->device))))
			    ) {
				numforwards++;
				if (sccp_strlen_zero(cfwdnum)) {
					if (linedevice->cfwdAll.number) {
						sccp_copy_string(cfwdnum, linedevice->cfwdAll.number, SCCP_MAX_EXTENSION);
					} else {
						sccp_copy_string(cfwdnum, linedevice->cfwdBusy.number, SCCP_MAX_EXTENSION);
					}
				}
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
		if (numdevices == numforwards) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: setting ast call forward channel: %s\n", c->designator, cfwdnum);
			iPbx.setChannelCallForward(c, cfwdnum);
		}
	}
#endif

	/* asterisk needs the native formats bevore dialout, otherwise the next channel gets the whole AUDIO_MASK as requested format
	 * chan_sip dont like this do sdp processing */
	//iPbx.set_nativeAudioFormats(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));

	// export sccp informations in asterisk dialplan
	if (d) {
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_MAC", d->id);
		struct sockaddr_storage sas = { 0 };
		sccp_session_getSas(d->session, &sas);
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_IP", d->session ? sccp_netsock_stringify_addr(&sas) : "");
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_TYPE", skinny_devicetype2str(d->skinny_type));
	}
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s\n", (l) ? l->id : "(null)", c->designator);
	return 1;
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
	AUTO_RELEASE(sccp_channel_t, channel , NULL);
	if ((channel = sccp_channel_retain(data))) {
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
sccp_extension_status_t sccp_pbx_helper(sccp_channel_t * c)
{
	sccp_extension_status_t extensionStatus;
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
void *sccp_pbx_softswitch(sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = NULL;
	PBX_VARIABLE_TYPE *v = NULL;

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
					sccp_callforward_t type = (sccp_callforward_t) c->ss_data;
					sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Forward %s Extension\n", d->id, sccp_callforward2str(type));
					if (!sccp_strlen_zero(shortenedNumber)) {
						sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, instance, c->callid, SKINNY_TONEDIRECTION_USER);
						sccp_line_cfwd(l, d, type, shortenedNumber);
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
					sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, SKINNY_TONEDIRECTION_USER);
				} else {
					sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, c->callid, SKINNY_TONEDIRECTION_USER);
				}
				sccp_channel_schedule_hangup(c, 500);
				goto EXIT_FUNC;									// leave simpleswitch without dial
#endif														// CS_SCCP_PICKUP
#ifdef CS_SCCP_CONFERENCE
			case SCCP_SOFTSWITCH_GETCONFERENCEROOM:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Conference request\n", d->id);
				if (c->owner && !pbx_check_hangup(c->owner)) {
					sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
					sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_PROCEED);
					iPbx.set_callstate(channel, AST_STATE_UP);
					if (!d->conference) {
						if (!(d->conference = sccp_conference_create(d, c))) {
							goto EXIT_FUNC;
						}
						sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);
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
				// like we're dialing but we're not :)
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Barge Extension\n", d->id);
				sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_channel_send_callinfo(d, c);

				sccp_dev_clearprompt(d, instance, c->callid);
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, GLOB(digittimeout));
				if (!sccp_strlen_zero(shortenedNumber)) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge exten '%s'\n", d->id, shortenedNumber);
					if (sccp_feat_barge(c, shortenedNumber)) {
						sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
					}
				} else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
				}
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

				//pbx_channel->cid.cid_pres = AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
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
#if 0														/* Remarked out by Pavel Troller for the earlyrtp immediate implementation. Checking if there will be negative fall out.
														   It might have to check the device->earlyrtp state, to do the correct thing (Let's see). */

		/* proceed call state is needed to display the called number. The phone will not display callinfo in offhook state */
		sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(d, c);

		sccp_dev_clearprompt(d, instance, c->callid);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, GLOB(digittimeout));
#endif

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
				if (d->earlyrtp != SCCP_EARLYRTP_IMMEDIATE){
					/* 
					 * too early to set last dialed number for immediate mode -Pavel Troller
					 */
					//AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, c->line));
					AUTO_RELEASE(sccp_linedevices_t, linedevice , c->getLineDevice(c));
					if(linedevice){ 
						sccp_device_setLastNumberDialed(d, shortenedNumber, linedevice);
					}
					if (iPbx.set_dialed_number){
						iPbx.set_dialed_number(c, shortenedNumber);
					}
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
