
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
 * $Date$
 * $Revision$  
 */
#ifndef __PBX_IMPL_C
#    define __PBX_IMPL_C

#    include "config.h"
#    include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/*!
 * \brief SCCP Structure to pass data to the pbx answer thread 
 */
struct sccp_answer_conveyor_struct {
	uint32_t callid;
	sccp_linedevices_t *linedevice;
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

	sccp_channel_t *c;

	int instance = 0;

	sleep(GLOB(autoanswer_ring_time));
	pthread_testcancel();

	if (!conveyor)
		return NULL;

	if (!conveyor->linedevice)
		return NULL;

	c = sccp_channel_find_byid_locked(conveyor->callid);
	if (!c)
		return NULL;

	if (c->state != SCCP_CHANNELSTATE_RINGING)
		return NULL;

	sccp_channel_answer_locked(conveyor->linedevice->device, c);

	if (GLOB(autoanswer_tone) != SKINNY_TONE_SILENCE && GLOB(autoanswer_tone) != SKINNY_TONE_NOTONE) {
		sccp_device_lock(conveyor->linedevice->device);
		instance = sccp_device_find_index_for_line(conveyor->linedevice->device, c->line->name);
		sccp_device_unlock(conveyor->linedevice->device);
		sccp_dev_starttone(conveyor->linedevice->device, GLOB(autoanswer_tone), instance, c->callid, 0);
	}
	if (c->autoanswer_type == SCCP_AUTOANSWER_1W)
		sccp_dev_set_microphone(conveyor->linedevice->device, SKINNY_STATIONMIC_OFF);

	sccp_channel_unlock(c);
	sccp_free(conveyor);

	return NULL;
}


/*!
 * \brief Incoming Calls by Asterisk SCCP_Request
 * \param ast 		Asterisk Channel as ast_channel
 * \param dest 		Destination as char
 * \param timeout 	Timeout after which incoming call is cancelled as int
 * \return Success as int
 *
 * \todo reimplement DNDMODES, ringermode=urgent, autoanswer
 *
 * \callgraph
 * \callergraph
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- line
 * 	  - line->devices
 * 	- line
 * 	- line->devices
 * 	  - see sccp_device_sendcallstate()
 * 	  - see sccp_channel_send_callinfo()
 * 	  - see sccp_channel_forward()
 * 	  - see sccp_util_matchSubscriptionId()
 * 	  - see sccp_channel_get_active()
 * 	  - see sccp_indicate_lock()
 */

int sccp_pbx_call(PBX_CHANNEL_TYPE *ast, char *dest, int timeout)
{
	sccp_line_t *l;

	sccp_channel_t *c;

	char *cid_name = NULL;
	char *cid_number = NULL;
	char *cid_dnid = NULL;
	char *cid_ani = NULL;
	int cid_ani2 = 0;
	char *cid_rdnis = NULL;

	char suffixedNumber[255] = { '\0' };					/*!< For saving the digittimeoutchar to the logs */
	boolean_t hasSession = FALSE;

	if (!sccp_strlen_zero(ast->call_forward)) {
		pbx_queue_control(ast, -1);					/* Prod Channel if in the middle of a call_forward instead of proceed */
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Forwarding Call to '%s'\n", ast->call_forward);
		return 0;
	}									// CS_AST_CHANNEL_HAS_CID

#if ASTERISK_VERSION_NUMBER < 10400
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif										// ASTERISK_VERSION_NUMBER < 10400

	c = get_sccp_channel_from_pbx_channel(ast);
	if (!c) {
		pbx_log(LOG_WARNING, "SCCP: Asterisk request to call %s channel: %s, but we don't have this channel!\n", dest, ast->name);
		return -1;
	}

	/* \todo perhaps we should lock the sccp_channel here. */
	l = c->line;
	if (l) {
		sccp_linedevices_t *linedevice;

		sccp_line_lock(l);
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			assert(linedevice->device);

			if (linedevice->device->session)
				hasSession = TRUE;
		}
		SCCP_LIST_UNLOCK(&l->devices);
		sccp_line_unlock(l);
	}

	if (!l || !hasSession) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line or device or session\n", (c ? c->callid : 0));
		return -1;
	}

	sccp_log(1) (VERBOSE_PREFIX_3 "%s: Asterisk request to call %s\n", l->id, ast->name);

	/* if incoming call limit is reached send BUSY */
	sccp_line_lock(l);
	if (SCCP_RWLIST_GETSIZE(l->channels) > l->incominglimit) {		/* >= just to be sure :-) */
		sccp_log(1) (VERBOSE_PREFIX_3 "Incoming calls limit (%d) reached on SCCP/%s... sending busy\n", l->incominglimit, l->name);
		sccp_line_unlock(l);
		pbx_setstate(ast, AST_STATE_BUSY);
		pbx_queue_control(ast, AST_CONTROL_BUSY);
		return 0;
	}
	sccp_line_unlock(l);

//      if (PBX(get_callerid_name))
//              PBX(get_callerid_name)(c, &cid_name);
// 
//      if (PBX(get_callerid_number))
//              PBX(get_callerid_number)(c, &cid_number);

	if (strlen(c->callInfo.callingPartyNumber) > 0)
		cid_number = strdup(c->callInfo.callingPartyNumber);

	if (strlen(c->callInfo.callingPartyName) > 0)
		cid_name = strdup(c->callInfo.callingPartyName);

	//! \todo implement dnid, ani, ani2 and rdnis
	/* Set the channel callingParty Name and Number */
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_num = '%s'\n", (cid_number) ? cid_number : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_name = '%s'\n", (cid_name) ? cid_name : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_dnid = '%s'\n", (cid_dnid) ? cid_dnid : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_ani = '%s'\n", (cid_ani) ? cid_ani : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_ani2 = '%i'\n", cid_ani2);
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_rdnis = '%s'\n", (cid_rdnis) ? cid_rdnis : "");

	if (GLOB(recorddigittimeoutchar)) {
		/* The hack to add the # at the end of the incoming number
		   is only applied for numbers beginning with a 0,
		   which is appropriate for Germany and other countries using similar numbering plan.
		   The option should be generalized, moved to the dialplan, or otherwise be replaced. */
		/* Also, we require an option whether to add the timeout suffix to certain
		   enbloc dialed numbers (such as via 7970 enbloc dialing) if they match a certain pattern.
		   This would help users dial from call history lists on other phones, which do not have enbloc dialing,
		   when using shared lines. */
		if (NULL != cid_number && strlen(cid_number) > 0 && strlen(cid_number) < sizeof(suffixedNumber) - 2 && '0' == cid_number[0]) {
			strncpy(suffixedNumber, cid_number, strlen(cid_number));
			suffixedNumber[strlen(cid_number) + 0] = '#';
			suffixedNumber[strlen(cid_number) + 1] = '\0';
			sccp_channel_set_callingparty(c, cid_name, suffixedNumber);
		} else
			sccp_channel_set_callingparty(c, cid_name, cid_number);

	} else {
		sccp_channel_set_callingparty(c, cid_name, cid_number);
	}

	/* check if we have an forwared call */
	if (!sccp_strlen_zero(cid_ani) && strncmp(cid_ani, c->callInfo.callingPartyNumber, strlen(cid_ani))) {
		sccp_copy_string(c->callInfo.originalCalledPartyNumber, cid_ani, sizeof(c->callInfo.originalCalledPartyNumber));
	}

	/* Set the channel calledParty Name and Number 7910 compatibility */
	sccp_channel_set_calledparty(c, l->cid_name, l->cid_num);

	if (!c->ringermode) {
		c->ringermode = SKINNY_STATION_OUTSIDERING;
		//ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
	}

	if (l->devices.size == 1 && SCCP_LIST_FIRST(&l->devices) && SCCP_LIST_FIRST(&l->devices)->device && SCCP_LIST_FIRST(&l->devices)->device->session) {
		//! \todo check if we have to do this
		sccp_channel_setDevice(c, SCCP_LIST_FIRST(&l->devices)->device);
// 		c->device = SCCP_LIST_FIRST(&l->devices)->device;
		sccp_channel_updateChannelCapability_locked(c);
	}

	boolean_t isRinging = FALSE;
	boolean_t hasDNDParticipant = FALSE;

	sccp_linedevices_t *linedevice;

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		assert(linedevice->device);

		/* do we have cfwd enabled? */
		if (linedevice->cfwdAll.enabled) {
			pbx_log(LOG_NOTICE, "%s: initialize cfwd for line %s\n", linedevice->device->id, l->name);
			sccp_device_sendcallstate(linedevice->device, linedevice->lineInstance, c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_channel_send_callinfo(linedevice->device, c);
			sccp_channel_forward(c, linedevice, linedevice->cfwdAll.number);
			isRinging = TRUE;
			continue;
		}

		if (!linedevice->device->session)
			continue;

		/* check if c->subscriptionId.number is matching deviceSubscriptionID */
		/* This means that we call only those devices on a shared line
		   which match the specified subscription id in the dial parameters. */
		if (!sccp_util_matchSubscriptionId(c, linedevice->subscriptionId.number)) {
			continue;
		}

		if (sccp_channel_get_active_nolock(linedevice->device)) {
			/* \todo perhaps lock the channel on global section */
			sccp_channel_lock(c);
			sccp_indicate_locked(linedevice->device, c, SCCP_CHANNELSTATE_CALLWAITING);
			sccp_channel_unlock(c);
			isRinging = TRUE;
		} else {
			if (linedevice->device->dndFeature.enabled && linedevice->device->dndFeature.status == SCCP_DNDMODE_REJECT){
				hasDNDParticipant = TRUE;
				continue;
			}

			/* \todo perhaps lock the channel on global section */
			sccp_channel_lock(c);
			sccp_indicate_locked(linedevice->device, c, SCCP_CHANNELSTATE_RINGING);
			sccp_channel_unlock(c);
			isRinging = TRUE;
			if (c->autoanswer_type) {

				struct sccp_answer_conveyor_struct *conveyor = sccp_calloc(1, sizeof(struct sccp_answer_conveyor_struct));

				if (conveyor) {
					sccp_log(1) (VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", DEV_ID_LOG(linedevice->device), ast->name);
					conveyor->callid = c->callid;
					conveyor->linedevice = linedevice;
					
					pthread_t t;
					pthread_attr_t attr;
					
					pthread_attr_init(&attr);
					pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
					if (pbx_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, conveyor)) {
						pbx_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", DEV_ID_LOG(linedevice->device), l->name, c->callid, strerror(errno));
                                      }
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if (isRinging) {
		pbx_queue_control(ast, AST_CONTROL_RINGING);
		sccp_channel_setSkinnyCallstate(c, SKINNY_CALLSTATE_RINGIN);
	} else if(hasDNDParticipant) {
		pbx_queue_control(ast, AST_CONTROL_BUSY);
	} else{
		pbx_queue_control(ast, AST_CONTROL_CONGESTION);
	}

	if (cid_name)
		free(cid_name);
	if (cid_number)
		free(cid_number);
	if (cid_dnid)
		free(cid_dnid);
	if (cid_ani)
		free(cid_ani);
	if (cid_rdnis)
		free(cid_rdnis);

	return 0;
}

/*!
 * \brief Handle Hangup Request by Asterisk
 * \param c SCCP Channel
 * \return Success as int
 *
 * \callgraph
 * \callergraph
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- usecnt_lock
 * 	- channel
 * 	  - sccp_channel_get_active()
 * 	  - see sccp_dev_starttone()
 * 	  - see sccp_indicate_nolock()
 * 	  - see sccp_conference_removeParticipant()
 * 	  - see sccp_channel_closereceivechannel()
 * 	  - see sccp_channel_destroy_rtp()
 * 	  - line
 * 	  - line->channels
 * 	    - see sccp_channel_endcall()
 * 	  - line->devices
 * 	    - sccp_indicate_nolock()
 * 	  - see sccp_channel_send_callinfo()
 * 	  - see sccp_pbx_needcheckringback()
 * 	  - see sccp_dev_check_displayprompt()
 * 	  - see sccp_channel_cleanbeforedelete()
 */

int sccp_pbx_hangup_locked(sccp_channel_t * c)
{
	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;

	/* here the ast channel is locked */
	//sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", ast->name);

	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)--;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	pbx_update_use_count();

	if (!c) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel. SCCP channel already deleted\n");
		sccp_pbx_needcheckringback(d);
		return 0;
	}

	d = sccp_channel_getDevice(c);
	if (c->state != SCCP_CHANNELSTATE_DOWN) {
		if (GLOB(remotehangup_tone) && d && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active_nolock(d))
			sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_ONHOOK);
	}

	c->owner = NULL;
	l = c->line;
#ifdef CS_SCCP_CONFERENCE
	if (c->conference) {
		//sccp_conference_removeParticipant(c->conference, c);
		sccp_conference_retractParticipatingChannel(c->conference, c);
	}
#endif										// CS_SCCP_CONFERENCE

	if (c->rtp.audio.rtp) {
		sccp_channel_closereceivechannel_locked(c);
		sccp_rtp_destroy(c);
	}
	// removing scheduled dialing
	SCCP_SCHED_DEL(c->scheduler.digittimeout);

	sccp_line_lock(c->line);
	c->line->statistic.numberOfActiveChannels--;
	sccp_line_unlock(c->line);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Current channel %s-%08x state %s(%d)\n", (d) ? DEV_ID_LOG(d) : "(null)", l ? l->name : "(null)", c->callid, sccp_indicate2str(c->state), c->state);

	/* end callforwards */
	sccp_channel_t *channel;

	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if (channel->parentChannel == c) {
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);
			/* No need to lock because c->line->channels is already locked. */
			sccp_channel_endcall_locked(channel);
		}
	}
	SCCP_LIST_UNLOCK(&c->line->channels);

	sccp_line_removeChannel(c->line, c);

	if (!d) {
		/* channel is not answerd, just ringin over all devices */
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			d = linedevice->device;
			sccp_indicate_locked(d, c, SKINNY_CALLSTATE_ONHOOK);
		}
		SCCP_LIST_UNLOCK(&l->devices);
	} else {
		sccp_channel_send_callinfo(d, c);
		sccp_pbx_needcheckringback(d);
		sccp_dev_check_displayprompt(d);
	}

	sccp_channel_clean_locked(c);

	if (sccp_sched_add(0, sccp_channel_destroy_callback, c) < 0) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unable to schedule destroy of channel %08X\n", c->callid);
	}
	//sccp_channel_unlock(c);

	return 0;
}

/*!
 * \brief Thread to check Device Ring Back
 *
 * The Auto Answer thread is started by ref sccp_pbx_needcheckringback if necessary
 *
 * \param d SCCP Device
 * 
 * \lock
 * 	- device->session
 */
void sccp_pbx_needcheckringback(sccp_device_t * d)
{

	if (d && d->session) {
		sccp_session_lock(d->session);
		d->session->needcheckringback = 1;
		sccp_session_unlock(d->session);
	}
}

/*!
 * \brief Answer an Asterisk Channel
 * \note we have no bridged channel at this point
 *
 * \param c SCCCP channel
 * \return Success as int
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 */
int sccp_pbx_answer(sccp_channel_t * c)
{
	/* \todo perhaps we should lock channel here. */

	if (c->parentChannel) {
		/* we are a forwarded call, bridge me with my parent */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_4 "SCCP: bridge me with my parent, device %s\n", DEV_ID_LOG(sccp_channel_getDevice(c)));

		PBX_CHANNEL_TYPE *astChannel = NULL, *br = NULL, *astForwardedChannel = c->parentChannel->owner;

		/*
		   on this point we do not have a pointer to ou bridge channel
		   so we search for it -MC
		 */
		const char *bridgePeer = pbx_builtin_getvar_helper(c->owner, "BRIDGEPEER");

		if (bridgePeer) {
			while ((astChannel = pbx_channel_walk_locked(astChannel)) != NULL) {
				sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) searching for channel where %s == %s\n", bridgePeer, astChannel->name);
				if (strlen(astChannel->name) == strlen(bridgePeer) && !strncmp(astChannel->name, bridgePeer, strlen(astChannel->name))) {
					pbx_channel_unlock(astChannel);
					br = astChannel;
					break;
				}
				pbx_channel_unlock(astChannel);
			}
		}

		/* did we find our bridge */
		if (br) {
			pbx_log(LOG_NOTICE, "SCCP: bridge: %s\n", (br) ? br->name : " -- no bridged found -- ");

			/* set the channel and the bridge to state UP to fix problem with fast pickup / autoanswer */
			pbx_setstate(c->owner, AST_STATE_UP);
			pbx_setstate(br, AST_STATE_UP);

			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Going to Masquerade %s into %s\n", br->name, astForwardedChannel->name);
			if (pbx_channel_masquerade(astForwardedChannel, br)) {
				pbx_log(LOG_ERROR, "(sccp_pbx_answer) Failed to masquerade bridge into forwarded channel\n");
				return -1;
			} else {
				c->parentChannel = NULL;
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Masqueraded into %s\n", astForwardedChannel->name);
			}

			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: ast %s\n", pbx_state2str(c->owner->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: astForwardedChannel %s\n", pbx_state2str(astForwardedChannel->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: br %s\n", pbx_state2str(br->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: astChannel %s\n", pbx_state2str(astChannel->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");
			return 0;
		}

		/* we have no bridge and can not make a masquerade -> end call */
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: ast %s\n", pbx_state2str(c->owner->_state));
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: astForwardedChannel %s\n", pbx_state2str(astForwardedChannel->_state));
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");

		if (c->owner->_state == AST_STATE_RING && astForwardedChannel->_state == AST_STATE_DOWN) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "SCCP: Receiver Hungup\n");
			astForwardedChannel->hangupcause = AST_CAUSE_CALL_REJECTED;
			astForwardedChannel->_softhangup |= AST_SOFTHANGUP_DEV;
			pbx_queue_hangup(astForwardedChannel);
#if 0
			sccp_channel_lock(c->parentChannel);
			sccp_channel_endcall_locked(c->parentChannel);
			sccp_channel_unlock(c->parentChannel);
#endif
			return 0;
		}
		pbx_log(LOG_ERROR, "SCCP: We did not find bridge channel for call forwarding call. Hangup\n");
		astForwardedChannel->hangupcause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		astForwardedChannel->_softhangup |= AST_SOFTHANGUP_DEV;
		pbx_queue_hangup(astForwardedChannel);
#if 0
		sccp_channel_lock(c->parentChannel);
		sccp_channel_endcall_locked(c->parentChannel);
		sccp_channel_unlock(c->parentChannel);
#endif
		sccp_channel_lock(c);
		sccp_channel_endcall_locked(c);
		sccp_channel_unlock(c);
		return -1;
	}

	sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Outgoing call has been answered %s on %s@%s-%08x\n", c->owner->name, c->line->name, DEV_ID_LOG(sccp_channel_getDevice(c)), c->callid);
	sccp_channel_lock(c);
	sccp_channel_updateChannelCapability_locked(c);

	/*! \todo This seems like brute force, and doesn't seem to be of much use. However, I want it to be remebered
	   as I have forgotten what my actual motivation was for writing this strange code. (-DD) */
	sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_DIALING);
	sccp_channel_send_callinfo(sccp_channel_getDevice(c), c);
	sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_PROCEED);
    
	sccp_channel_send_callinfo(sccp_channel_getDevice(c), c);
	sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_CONNECTED);

	sccp_channel_unlock(c);
	return 0;
}


/*!
 * \brief Allocate an Asterisk Channel
 * \param c SCCP Channel
 * \return 1 on Success as uint8_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line->devices
 * 	- channel
 * 	  - line
 * 	  - see sccp_channel_updateChannelCapability()
 * 	- usecnt_lock
 */
uint8_t sccp_pbx_channel_allocate_locked(sccp_channel_t * c)
{
	PBX_CHANNEL_TYPE *tmp;

	sccp_line_t *l = c->line;

#ifndef CS_AST_CHANNEL_HAS_CID
	char cidtmp[256];

	memset(&cidtmp, 0, sizeof(cidtmp));
#endif										// CS_AST_CHANNEL_HAS_CID

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: try to allocate channel \n");
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Line: %s\n", l->name);

	if (!l) {
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Unable to allocate asterisk channel %s\n", l->name);
		pbx_log(LOG_ERROR, "SCCP: Unable to allocate asterisk channel\n");
		return 0;
	}
//      /* Don't hold a sccp pvt lock while we allocate a channel */
	if (sccp_channel_getDevice(c)) {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice->device == sccp_channel_getDevice(c))
				break;
		}
		SCCP_LIST_UNLOCK(&l->devices);

		switch (c->calltype) {
		case SKINNY_CALLTYPE_INBOUND:
			/* append subscriptionId to cid */
			if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.number)) {
				sprintf(c->callInfo.calledPartyNumber, "%s%s", l->cid_num, linedevice->subscriptionId.number);
			} else {
				sprintf(c->callInfo.calledPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number) ? l->defaultSubscriptionId.number : "");
			}

			if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.name)) {
				sprintf(c->callInfo.calledPartyName, "%s%s", l->cid_name, linedevice->subscriptionId.name);
			} else {
				sprintf(c->callInfo.calledPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name) ? l->defaultSubscriptionId.name : "");
			}
			break;
		case SKINNY_CALLTYPE_FORWARD:
		case SKINNY_CALLTYPE_OUTBOUND:
			/* append subscriptionId to cid */
			if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.number)) {
				sprintf(c->callInfo.callingPartyNumber, "%s%s", l->cid_num, linedevice->subscriptionId.number);
			} else {
				sprintf(c->callInfo.callingPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number) ? l->defaultSubscriptionId.number : "");
			}

			if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.name)) {
				sprintf(c->callInfo.callingPartyName, "%s%s", l->cid_name, linedevice->subscriptionId.name);
			} else {
				sprintf(c->callInfo.callingPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name) ? l->defaultSubscriptionId.name : "");
			}
			break;
		}
	} else {

		switch (c->calltype) {
		case SKINNY_CALLTYPE_INBOUND:
			sprintf(c->callInfo.calledPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number) ? l->defaultSubscriptionId.number : "");
			sprintf(c->callInfo.calledPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name) ? l->defaultSubscriptionId.name : "");
			break;
		case SKINNY_CALLTYPE_FORWARD:
		case SKINNY_CALLTYPE_OUTBOUND:
			sprintf(c->callInfo.callingPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number) ? l->defaultSubscriptionId.number : "");
			sprintf(c->callInfo.callingPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name) ? l->defaultSubscriptionId.name : "");
			break;
		}
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:     cid_num: \"%s\"\n", c->callInfo.callingPartyNumber);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:    cid_name: \"%s\"\n", c->callInfo.callingPartyName);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: accountcode: \"%s\"\n", l->accountcode);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:       exten: \"%s\"\n", c->dialedNumber);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:     context: \"%s\"\n", l->context);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:    amaflags: \"%d\"\n", l->amaflags);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP:   chan/call: \"%s-%08x\"\n", l->name, c->callid);

	/* This should definitely fix CDR */
//      tmp = pbx_channel_alloc(1, AST_STATE_DOWN, c->callInfo.callingPartyNumber, c->callInfo.callingPartyName, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08x", l->name, c->callid);
	PBX(alloc_pbxChannel) (c, &tmp);
	if (!tmp) {
		pbx_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", l->id, l->name);
		return 0;
	}

	/* need to reset the exten, otherwise it would be set to s */
	//! \todo we should move this
	memset(&tmp->exten, 0, sizeof(tmp->exten));

	//\todo should we move this to pbx implementation ? -MC
	c->owner = tmp;
	tmp->tech_pvt = c;

#ifdef CS_EXPERIMENTAL
	PBX(set_nativeAudioFormats)(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));
#else
	if(c->calltype == SKINNY_CALLTYPE_OUTBOUND){
		PBX(set_nativeAudioFormats)(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));
	}else{
		PBX(set_nativeAudioFormats)(c, c->preferences.audio, 1);
	}
#endif
	
	
	sccp_channel_updateChannelCapability_locked(c);
	//! \todo check locking
	/* \todo we should remove this shit. */
	while (sccp_line_trylock(l)) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		usleep(1);
	}
	

#ifdef CS_AST_HAS_AST_STRING_FIELD
	pbx_string_field_build(tmp, name, "SCCP/%s-%08x", l->name, c->callid);
#else
	snprintf(tmp->name, sizeof(tmp->name), "SCCP/%s-%08x", l->name, c->callid);
#endif										// CS_AST_HAS_AST_STRING_FIELD

	sccp_line_unlock(l);

	pbx_jb_configure(tmp, &GLOB(global_jbconf));
                                                                              // CS_AST_HAS_TECH_PVT
	tmp->adsicpe = AST_ADSI_UNAVAILABLE;

	// \todo: Bridge?
	// \todo: Transfer?
	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)++;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	pbx_update_use_count();

	if (PBX(set_callerid_number))
		PBX(set_callerid_number) (c, c->callInfo.callingPartyNumber);

	if (PBX(set_callerid_name))
		PBX(set_callerid_name) (c, c->callInfo.callingPartyName);
	
	
	if(c->getDevice(c) && c->getDevice(c)->monitorFeature.status == SCCP_FEATURE_MONITOR_STATE_ENABLED_NOTACTIVE){
		sccp_feat_monitor(c->getDevice(c), c->line, 0, c);
		sccp_feat_changed(c->getDevice(c), SCCP_FEATURE_MONITOR);
	}
	
	
	/* asterisk needs the native formats bevore dialout, otherwise the next channel gets the whole AUDIO_MASK as requested format
	 * chan_sip dont like this do sdp processing */
// 	PBX(set_nativeAudioFormats)(c, c->preferences.audio, ARRAY_LEN(c->preferences.audio));

	// export sccp informations in asterisk dialplan
	if (sccp_channel_getDevice(c)) {
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_MAC", sccp_channel_getDevice(c)->id);
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_IP", pbx_inet_ntoa(sccp_channel_getDevice(c)->session->sin.sin_addr));
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_TYPE", devicetype2str(sccp_channel_getDevice(c)->skinny_type));
	}
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s-%d\n", (l) ? l->id : "(null)", (l) ? l->name : "(null)", c->callid);

	return 1;
}

/*!
 * \brief Schedule Asterisk Dial
 * \param data Data as constant
 * \return Success as int
 * 
 * \called_from_asterisk
 */
int sccp_pbx_sched_dial(const void *data)
{
	sccp_channel_t *c = (sccp_channel_t *) data;

	if (c && c->owner && !c->owner->pbx) {
		sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: Timeout for call '%d'. Going to dial '%s'\n", c->callid, c->dialedNumber);
		sccp_channel_lock(c);
		sccp_pbx_softswitch_locked(c);
		sccp_channel_unlock(c);
		// return 1;
	}

	return 0;
}

/*!
 * \brief Asterisk Helper
 * \param c SCCP Channel as sccp_channel_t
 * \return Success as int
 */
sccp_extension_status_t sccp_pbx_helper(sccp_channel_t * c)
{
	sccp_extension_status_t extensionStatus;

	if (!sccp_strlen_zero(c->dialedNumber)) {
		if (GLOB(recorddigittimeoutchar) && GLOB(digittimeoutchar) == c->dialedNumber[strlen(c->dialedNumber) - 1]) {
			/* we finished dialing with digit timeout char */
			sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: We finished dialing with digit timeout char %s\n", c->dialedNumber);
			return SCCP_EXTENSION_EXACTMATCH;
		}
	}

	if ((c->ss_action != SCCP_SS_GETCBARGEROOM) && (c->ss_action != SCCP_SS_GETMEETMEROOM)) {

		//! \todo check overlap feature status -MC
		extensionStatus = PBX(extension_status) (c);
		if (((sccp_channel_getDevice(c)->overlapFeature.enabled && !extensionStatus) || (!sccp_channel_getDevice(c)->overlapFeature.enabled && !extensionStatus))
		    && ((sccp_channel_getDevice(c)->overlapFeature.enabled && !extensionStatus) || (!sccp_channel_getDevice(c)->overlapFeature.enabled && !extensionStatus))) {
			sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: %s Matches more\n", c->dialedNumber);
			return SCCP_EXTENSION_MATCHMORE;
		}
		sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: %s Match %s\n", c->dialedNumber, extensionStatus == SCCP_EXTENSION_EXACTMATCH ? "Exact" : "More");
		return extensionStatus;
	}
	sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: %s Does Exists\n", c->dialedNumber);
	return SCCP_EXTENSION_NOTEXISTS;
}

/*!
 * \brief Handle Soft Switch
 * \param c SCCP Channel as sccp_channel_t
 * \todo clarify Soft Switch Function
 *
 * \lock
 * 	- channel
 * 	  - see sccp_pbx_senddigits()
 * 	  - see sccp_channel_set_calledparty()
 * 	  - see sccp_indicate_nolock()
 * 	- channel
 * 	  - see sccp_line_cfwd()
 * 	  - see sccp_indicate_nolock()
 * 	  - see sccp_device_sendcallstate()
 * 	  - see sccp_channel_send_callinfo()
 * 	  - see sccp_dev_clearprompt()
 * 	  - see sccp_dev_displayprompt()
 * 	  - see sccp_feat_meetme_start()
 * 	  - see PBX(set_callstate)()
 * 	  - see pbx_pbx_start()
 * 	  - see sccp_indicate_nolock()
 * 	  - see manager_event()
 */
void *sccp_pbx_softswitch_locked(sccp_channel_t * c)
{
	if (!c) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <channel> available. Returning from dial thread.\n");
		return NULL;
	}

	/* Reset Enbloc Dial Emulation */
	c->enbloc.deactivate=0;
	c->enbloc.totaldigittime=0;
	c->enbloc.totaldigittimesquared=0;
	c->enbloc.digittimeout = GLOB(digittimeout) * 1000;
	
	/* prevent softswitch from being executed twice (Pavel Troller / 15-Oct-2010) */
	if (c->owner->pbx) {
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) PBX structure already exists. Dialing instead of starting.\n");
		/* If there are any digits, send them instead of starting the PBX */
		if (sccp_is_nonempty_string(c->dialedNumber)) {
			sccp_pbx_senddigits(c, c->dialedNumber);
			sccp_channel_set_calledparty(c, c->dialedNumber, c->dialedNumber);
			if (sccp_channel_getDevice(c))
				sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_DIALING);
		}
		return NULL;
	}
	

	PBX_CHANNEL_TYPE *chan = c->owner;

	PBX_VARIABLE_TYPE *v = NULL;

	uint8_t instance;

	unsigned int len = 0;

	sccp_line_t *l;

	sccp_device_t *d;

	char shortenedNumber[256] = { '\0' };					/* For recording the digittimeoutchar */

	/* removing scheduled dialing */
	SCCP_SCHED_DEL(c->scheduler.digittimeout);

	/* we should just process outbound calls, let's check calltype */
	if (c->calltype != SKINNY_CALLTYPE_OUTBOUND) {
		return NULL;
	}

	/* assume d is the channel's device */
	d = sccp_channel_getDevice(c);

	/* does it exists ? */
	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> available. Returning from dial thread.\n");
		return NULL;
	}

	/* we don't need to check for a device type but just if the device has an id, otherwise back home  -FS */
	if (!d->id || sccp_strlen_zero(d->id)) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> identifier available. Returning from dial thread.\n");
		return NULL;
	}

	l = c->line;
	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <line> available. Returning from dial thread.\n");
		if (chan)
			ast_hangup(chan);
		return NULL;
	}

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) New call on line %s\n", DEV_ID_LOG(d), l->name);

	/* assign callerid name and number */
	//sccp_channel_set_callingparty(c, l->cid_name, l->cid_num);

	// we use shortenedNumber but why ???
	// If the timeout digit has been used to terminate the number
	// and this digit shall be included in the phone call history etc (recorddigittimeoutchar is true)
	// we still need to dial the number without the timeout char in the pbx
	// so that we don't dial strange extensions with a trailing characters.
	sccp_copy_string(shortenedNumber, c->dialedNumber, sizeof(shortenedNumber));
	len = strlen(shortenedNumber);
	assert(strlen(c->dialedNumber) == len);

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
	switch (c->ss_action) {
	case SCCP_SS_GETFORWARDEXTEN:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Forward Extension\n", d->id);
		if (!sccp_strlen_zero(shortenedNumber)) {
			sccp_line_cfwd(l, d, c->ss_data, shortenedNumber);
		}
		sccp_channel_endcall_locked(c);
		return NULL;							// leave simple switch without dial
#ifdef CS_SCCP_PICKUP
	case SCCP_SS_GETPICKUPEXTEN:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Pickup Extension\n", d->id);
		// like we're dialing but we're not :)
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_DIALING);
		sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_clearprompt(d, instance, c->callid);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);

		if (!sccp_strlen_zero(shortenedNumber)) {
			sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to pickup exten '%s'\n", shortenedNumber);
			if (sccp_feat_directpickup_locked(c, shortenedNumber)) {
				sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
			}
		} else {
			// without a number we can also close the call. Isn't it true ?
			sccp_channel_endcall_locked(c);
		}
		return NULL;							// leave simpleswitch without dial
#endif										// CS_SCCP_PICKUP
	case SCCP_SS_GETMEETMEROOM:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request\n", d->id);
		if (!sccp_strlen_zero(shortenedNumber) && !sccp_strlen_zero(c->line->meetmenum)) {
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request for room '%s' on extension '%s'\n", d->id, shortenedNumber, c->line->meetmenum);
			if (c->owner && !pbx_check_hangup(c->owner))
				pbx_builtin_setvar_helper(c->owner, "SCCP_MEETME_ROOM", shortenedNumber);
			sccp_copy_string(shortenedNumber, c->line->meetmenum, sizeof(shortenedNumber));

			//sccp_copy_string(c->dialedNumber, SKINNY_DISP_CONFERENCE, sizeof(c->dialedNumber));
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Start Meetme Thread\n", d->id);
			sccp_feat_meetme_start(c);				/* Copied from Federico Santulli */
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme Thread Started\n", d->id);
			return NULL;
		} else {
			// without a number we can also close the call. Isn't it true ?
			sccp_channel_endcall_locked(c);
			return NULL;
		}
		break;
	case SCCP_SS_GETBARGEEXTEN:
		// like we're dialing but we're not :)
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Barge Extension\n", d->id);
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_DIALING);
		sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(d, c);

		sccp_dev_clearprompt(d, instance, c->callid);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
		if (!sccp_strlen_zero(shortenedNumber)) {
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge exten '%s'\n", d->id, shortenedNumber);
			if (sccp_feat_barge(c, shortenedNumber)) {
				sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
			}
		} else {
			// without a number we can also close the call. Isn't it true ?
			sccp_channel_endcall_locked(c);
		}
		return NULL;							// leave simpleswitch without dial
	case SCCP_SS_GETCBARGEROOM:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Get Conference Barge Extension\n", d->id);
		// like we're dialing but we're not :)
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_DIALING);
		sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_clearprompt(d, instance, c->callid);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
		if (!sccp_strlen_zero(shortenedNumber)) {
			sccp_log(1) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge conference '%s'\n", d->id, shortenedNumber);
			if (sccp_feat_cbarge(c, shortenedNumber)) {
				sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
			}
		} else {
			// without a number we can also close the call. Isn't it true ?
			sccp_channel_endcall_locked(c);
		}
		return NULL;							// leave simpleswitch without dial
	case SCCP_SS_DIAL:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Dial Extension\n", d->id);
	default:
		break;
	}

	/* set private variable */
	if (chan && !pbx_check_hangup(chan)) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", c->privacy ? "1" : "0");
		if (c->privacy) {

			//chan->cid.cid_pres = AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
			sccp_channel_set_calleridPresenceParameter(c, CALLERID_PRESENCE_FORBIDDEN);
		}

		uint32_t result = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

		result |= c->privacy;
		if (d->privacyFeature.enabled && result) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", "1");
			pbx_builtin_setvar_helper(chan, "SKINNY_PRIVATE", "1");
		} else {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", "0");
			pbx_builtin_setvar_helper(chan, "SKINNY_PRIVATE", "0");
		}
	}

	/* set devicevariables */
	v = ((d) ? d->variables : NULL);
	while (chan && !pbx_check_hangup(chan) && d && v) {
		pbx_builtin_setvar_helper(chan, v->name, v->value);
		v = v->next;
	}

	/* set linevariables */
	v = ((l) ? l->variables : NULL);
	while (chan && !pbx_check_hangup(chan) && l && v) {
		pbx_builtin_setvar_helper(chan, v->name, v->value);
		v = v->next;
	}

	sccp_copy_string(chan->exten, shortenedNumber, sizeof(chan->exten));
	sccp_copy_string(d->lastNumber, c->dialedNumber, sizeof(d->lastNumber));
	sccp_softkey_setSoftkeyState(d, KEYMODE_ONHOOK, SKINNY_LBL_REDIAL, TRUE); /** enable redial key */
	sccp_channel_set_calledparty(c, c->dialedNumber, shortenedNumber);

	/* The 7961 seems to need the dialing callstate to record its directories information. */
	sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_DIALING);

	/* proceed call state is needed to display the called number.
	   The phone will not display callinfo in offhook state */
	sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_channel_send_callinfo(d, c);

	sccp_dev_clearprompt(d, instance, c->callid);
	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);

	if (!sccp_strlen_zero(shortenedNumber) && !pbx_check_hangup(chan)
	    && pbx_exists_extension(chan, chan->context, shortenedNumber, 1, l->cid_num)) {
		/* found an extension, let's dial it */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x is dialing number %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		PBX(set_callstate) (c, AST_STATE_RING);
		
		
		int8_t pbxStartResult = pbx_pbx_start(chan);
		
		/* \todo replace AST_PBX enum using pbx_impl wrapper enum */
		switch(pbxStartResult){
			case AST_PBX_FAILED:
				pbx_log(LOG_ERROR, "%s: (sccp_pbx_softswitch) channel %s-%08x failed to start new thread to dial %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
				/* \todo change indicate to something more suitable */
				sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				break;
			case AST_PBX_CALL_LIMIT:
				pbx_log(LOG_WARNING, "%s: (sccp_pbx_softswitch) call limit reached for channel %s-%08x failed to start new thread to dial %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
				sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_CONGESTION);
				break;
			default:
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) pbx started\n", DEV_ID_LOG(d));
#ifdef CS_MANAGER_EVENTS
				if (GLOB(callevents)) {
					manager_event(EVENT_FLAG_SYSTEM, "ChannelUpdate", "Channel: %s\r\nUniqueid: %s\r\nChanneltype: %s\r\nSCCPdevice: %s\r\nSCCPline: %s\r\nSCCPcallid: %s\r\n", chan->name, chan->uniqueid, "SCCP", (d) ? DEV_ID_LOG(d) : "(null)", (l) ? l->name : "(null)", (c) ? (char *)&c->callid : "(null)");
				}
#endif
				break;
		}
	} else {
		
		sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x shortenedNumber: %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
		sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x pbx_check_hangup(chan): %d\n", DEV_ID_LOG(d), l->name, c->callid, pbx_check_hangup(chan));
		sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x extension exists: %s\n", DEV_ID_LOG(d), l->name, c->callid, pbx_exists_extension(chan, chan->context, shortenedNumber, 1, l->cid_num)?"TRUE":"FALSE");
		/* timeout and no extension match */
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) quit\n", DEV_ID_LOG(d));

	return NULL;
}

/*!
 * \brief Send Digit to Asterisk
 * \param c SCCP Channel
 * \param digit Digit as char
 */
void sccp_pbx_senddigit(sccp_channel_t * c, char digit)
{
	if (PBX(send_digit))
		PBX(send_digit) (c, digit);
}

/*!
 * \brief Send Multiple Digits to Asterisk
 * \param c SCCP Channel
 * \param digits Multiple Digits as char
 */
void sccp_pbx_senddigits(sccp_channel_t * c, char *digits)
{
	if (PBX(send_digits))
		PBX(send_digits) (c, digits);
}

/*!
 * \brief Queue a control frame
 * \param c SCCP Channel
 * \param control as Asterisk Control Frame Type
 *
 * \deprecated
 */
int sccp_pbx_queue_control(sccp_channel_t * c, uint8_t control)
{
//      PBX_FRAME_TYPE f;
//      f = pbx_null_frame;
//      f.frametype = AST_FRAME_DTMF;
//      f.subclass = control;
//      sccp_queue_frame(c, &f);

	//! \todo implement this in pbx implementation
	return 0;

}


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

/*
        sccp_device_t *d;
        sccp_channel_t *c;
        sccp_channel_t *newcall;
*/

	if (dest == NULL) {							/* functions below do not take a NULL */
		dest = "";
		return -1;
	}
	sccp_log(1) (VERBOSE_PREFIX_1 "Transferring '%s' to '%s'\n", ast->name, dest);
	if (ast->_state == AST_STATE_RING) {
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

/*
 * \brief ACF Channel Read callback
 *
 * \param ast Asterisk Channel
 * \param funcname	functionname as const char *
 * \param args		arguments as char *
 * \param buf		buffer as char *
 * \param buflen 	bufferlenght as size_t
 * \return result as int
 *
 * \called_from_asterisk
 * 
 * \test ACF Channel Read Needs to be tested
 */
// int acf_channel_read(PBX_CHANNEL_TYPE *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen);
// 
// int acf_channel_read(PBX_CHANNEL_TYPE *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen)
// {
//      sccp_channel_t *c;
// 
//      c = get_sccp_channel_from_pbx_channel(ast);
//      if (c == NULL)
//              return -1;
// 
//      if (!strcasecmp(args, "peerip")) {
//              sccp_copy_string(buf, c->rtp.audio.phone_remote.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr) : "", buflen);
//      } else if (!strcasecmp(args, "recvip")) {
//              sccp_copy_string(buf, c->rtp.audio.phone.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone.sin_addr) : "", buflen);
//      } else if (!strcasecmp(args, "from") && c->device) {
//              sccp_copy_string(buf, (char *)c->device->id, buflen);
//      } else {
//              return -1;
//      }
//      return 0;
// }

/*!
 * \brief Get (remote) peer for this channel
 * \param channel 	SCCP Channel
 *
 * \deprecated
 */
sccp_channel_t *sccp_pbx_getPeer(sccp_channel_t * channel)
{
	//! \todo implement internal peer search
	return NULL;
}

/*!
 * \brief Get codec capabilities for local channel
 * \param channel 	SCCP Channel
 * \param capabilities 	Codec Capabilities
 *
 * \deprecated
 * \note Variable capabilities will be malloced by function, caller must destroy this later
 */
int sccp_pbx_getCodecCapabilities(sccp_channel_t * channel, void **capabilities)
{
	//! \todo implement internal peer search
	return -1;
}

/**
 * \brief Get Peer Codec Capabilies
 * \param channel 	SCCP Channel
 * \param capabilities 	Codec Capabilities
 *
 * \note Variable capabilities will be malloced by function, caller must destroy this later
 */
int sccp_pbx_getPeerCodecCapabilities(sccp_channel_t * channel, void **capabilities)
{
	sccp_channel_t *peer;

	peer = sccp_pbx_getPeer(channel);
	if (peer) {
		/* we got a sccp peer */
		return sccp_pbx_getCodecCapabilities(channel, capabilities);
	} else {
		/* no local information available, ask pbx */
		PBX(getPeerCodecCapabilities) (channel, capabilities);
		return 1;
	}
}

#endif
