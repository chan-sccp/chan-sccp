
/*!
 * \file 	sccp_pbx.c
 * \brief 	SCCP PBX Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note		Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
#endif										// CS_AST_HAS_TECH_PVT

#ifdef CS_AST_RTP_NEW_SOURCE
#    define RTP_NEW_SOURCE(_c,_log) 								\
if(c->rtp.audio.rtp) { 										\
	pbx_rtp_new_source(c->rtp.audio.rtp); 							\
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
}
#    define RTP_CHANGE_SOURCE(_c,_log) 								\
if(c->rtp.audio.rtp) {										\
	pbx_rtp_change_source(c->rtp.audio.rtp);						\
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
}
#else
#    define RTP_NEW_SOURCE(_c,_log)
#    define RTP_CHANGE_SOURCE(_c,_log)
#endif

/* Structure to pass data to the thread */
struct sccp_answer_conveyor_struct {
	uint32_t callid;
	sccp_linedevices_t *linedevice;
};

/*!
 * \brief Call Auto Answer Thead
 *
 * The Auto Answer thread is started by ref sccp_pbx_call if necessary
 * \param data Data
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

	return NULL;
}

/*!
 * \brief Incoming Calls by Asterisk SCCP_Request
 * \param ast Asterisk Channel as ast_channel
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
static int sccp_pbx_call(struct ast_channel *ast, char *dest, int timeout)
{
	sccp_line_t *l;

	sccp_channel_t *c;

	const char *ringermode = NULL;

	pthread_attr_t attr;

	pthread_t t;
	char suffixedNumber[255] = { '\0' };					/*!< For saving the digittimeoutchar to the logs */
	boolean_t hasSession = FALSE;

	if (!sccp_strlen_zero(ast->call_forward)) {
		ast_queue_control(ast, -1);					/* Prod Channel if in the middle of a call_forward instead of proceed */
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Forwarding Call to '%s'\n", ast->call_forward);
		return 0;
	}
#ifndef CS_AST_CHANNEL_HAS_CID
	char *name, *number, *cidtmp;						// For the callerid parse below
#endif										// CS_AST_CHANNEL_HAS_CID

#if ASTERISK_VERSION_NUM < 10400
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif										// ASTERISK_VERSION_NUM < 10400

	c = get_sccp_channel_from_ast_channel(ast);
	if (!c) {
		ast_log(LOG_WARNING, "SCCP: Asterisk request to call %s channel: %s, but we don't have this channel!\n", dest, ast->name);
		return -1;
	}

	/* XXX perhaps we should lock the sccp_channel here. */
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
		ast_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line or device or session\n", (c ? c->callid : 0));
		return -1;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Asterisk request to call %s\n", l->id, ast->name);

	/* if incoming call limit is reached send BUSY */
	sccp_line_lock(l);
	if (SCCP_RWLIST_GETSIZE(l->channels) > l->incominglimit) {		/* >= just to be sure :-) */
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "Incoming calls limit (%d>%d) reached on SCCP/%s... sending busy\n", SCCP_RWLIST_GETSIZE(l->channels), l->incominglimit, l->name);
		sccp_line_unlock(l);
		ast_setstate(ast, AST_STATE_BUSY);
		ast_queue_control(ast, AST_CONTROL_BUSY);
		return 0;
	}
	sccp_line_unlock(l);

	/* Set the channel callingParty Name and Number */
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_num = '%s'\n", (ast->cid.cid_num) ? ast->cid.cid_num : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_name = '%s'\n", (ast->cid.cid_name) ? ast->cid.cid_name : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_dnid = '%s'\n", (ast->cid.cid_dnid) ? ast->cid.cid_dnid : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_ani = '%s'\n", (ast->cid.cid_ani) ? ast->cid.cid_ani : "");
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_ani2 = '%i'\n", (ast->cid.cid_ani) ? ast->cid.cid_ani2 : -1);
	sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_call) asterisk cid_rdnis = '%s'\n", (ast->cid.cid_rdnis) ? ast->cid.cid_rdnis : "");
#ifdef CS_AST_CHANNEL_HAS_CID
	if (GLOB(recorddigittimeoutchar)) {
		/* The hack to add the # at the end of the incoming number
		   is only applied for numbers beginning with a 0,
		   which is appropriate for Germany and other countries using similar numbering plan.
		   The option should be generalized, moved to the dialplan, or otherwise be replaced. */
		/* Also, we require an option whether to add the timeout suffix to certain
		   enbloc dialed numbers (such as via 7970 enbloc dialing) if they match a certain pattern.
		   This would help users dial from call history lists on other phones, which do not have enbloc dialing,
		   when using shared lines. */
		if (NULL != ast->cid.cid_num && strlen(ast->cid.cid_num) > 0 && strlen(ast->cid.cid_num) < sizeof(suffixedNumber) - 2 && '0' == ast->cid.cid_num[0]) {
			strncpy(suffixedNumber, (const char *)ast->cid.cid_num, strlen(ast->cid.cid_num));
			suffixedNumber[strlen(ast->cid.cid_num) + 0] = '#';
			suffixedNumber[strlen(ast->cid.cid_num) + 1] = '\0';
			sccp_channel_set_callingparty(c, ast->cid.cid_name, suffixedNumber);
		} else
			sccp_channel_set_callingparty(c, ast->cid.cid_name, ast->cid.cid_num);

	} else {
		sccp_channel_set_callingparty(c, ast->cid.cid_name, ast->cid.cid_num);
	}

	/* check if we have an forwared call */
	if (!sccp_strlen_zero(ast->cid.cid_ani) && strncmp(ast->cid.cid_ani, c->callInfo.callingPartyNumber, strlen(ast->cid.cid_ani))) {
		sccp_copy_string(c->callInfo.originalCalledPartyNumber, ast->cid.cid_ani, sizeof(c->callInfo.originalCalledPartyNumber));
	}
#else										// CS_AST_CHANNEL_HAS_CID
	if (ast->callerid && (cidtmp = strdup(ast->callerid))) {
		ast_callerid_parse(cidtmp, &name, &number);
		sccp_channel_set_callingparty(c, name, number);
		if (cidtmp)
			ast_free(cidtmp);
		cidtmp = NULL;

		if (name)
			ast_free(name);
		name = NULL;

		if (number)
			ast_free(number);
		number = NULL;
	}
#endif
	/* Set the channel calledParty Name and Number 7910 compatibility */
	sccp_channel_set_calledparty(c, l->cid_name, l->cid_num);

	if (!c->ringermode) {
		c->ringermode = SKINNY_STATION_OUTSIDERING;
		//ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
	}

	/*!
	 * \bug RingerMode=ALERT_INFO seems it does not work
	 */
	ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");

	if (ringermode && !sccp_strlen_zero(ringermode)) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "line %s: Found ALERT_INFO=%s\n", l->name, ringermode);
		if (strcasecmp(ringermode, "inside") == 0)
			c->ringermode = SKINNY_STATION_INSIDERING;
		else if (strcasecmp(ringermode, "feature") == 0)
			c->ringermode = SKINNY_STATION_FEATURERING;
		else if (strcasecmp(ringermode, "silent") == 0)
			c->ringermode = SKINNY_STATION_SILENTRING;
		else if (strcasecmp(ringermode, "urgent") == 0)
			c->ringermode = SKINNY_STATION_URGENTRING;
	}

	if (l->devices.size == 1 && SCCP_LIST_FIRST(&l->devices) && SCCP_LIST_FIRST(&l->devices)->device && SCCP_LIST_FIRST(&l->devices)->device->session) {
		// \todo TODO check if we have to do this
		c->device = SCCP_LIST_FIRST(&l->devices)->device;
		sccp_channel_updateChannelCapability_locked(c);
	}

	boolean_t isRinging = FALSE;

	sccp_linedevices_t *linedevice;

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		assert(linedevice->device);

		/* do we have cfwd enabled? */
		if (linedevice->cfwdAll.enabled) {
			ast_log(LOG_NOTICE, "%s: initialize cfwd for line %s to %s\n", linedevice->device->id, l->name, linedevice->cfwdAll.number);
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
			/* XXX perhaps lock the channel on global section */
			sccp_channel_lock(c);
			sccp_indicate_locked(linedevice->device, c, SCCP_CHANNELSTATE_CALLWAITING);
			sccp_channel_unlock(c);
			isRinging = TRUE;
		} else {
			if (linedevice->device->dndFeature.enabled && linedevice->device->dndFeature.status == SCCP_DNDMODE_REJECT)
				continue;

			/* XXX perhaps lock the channel on global section */
			sccp_channel_lock(c);
			if (!c->autoanswer_type) {
				sccp_indicate_locked(linedevice->device, c, SCCP_CHANNELSTATE_RINGING);
			};
			sccp_channel_unlock(c);
			isRinging = TRUE;
			if (c->autoanswer_type) {

				struct sccp_answer_conveyor_struct *conveyor = ast_calloc(1, sizeof(struct sccp_answer_conveyor_struct));

				if (conveyor) {
					sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", DEV_ID_LOG(linedevice->device), ast->name);
					conveyor->callid = c->callid;
					conveyor->linedevice = linedevice;

					pthread_attr_init(&attr);
					pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
					if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, conveyor)) {
						ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", DEV_ID_LOG(linedevice->device), l->name, c->callid, strerror(errno));
					}
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if (isRinging) {
		ast_queue_control(ast, AST_CONTROL_RINGING);
		sccp_channel_setSkinnyCallstate(c, SKINNY_CALLSTATE_RINGIN);
	} else {
		ast_queue_control(ast, AST_CONTROL_CONGESTION);
	}

	return 0;
}

/*!
 * \brief Handle Hangup Request by Asterisk
 * \param ast Asterisk Channel as ast_channel
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
static int sccp_pbx_hangup(struct ast_channel *ast)
{
	sccp_channel_t *c;

	sccp_line_t *l = NULL;

	sccp_device_t *d = NULL;

	/* here the ast channel is locked */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", ast->name);

	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)--;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

	c = get_sccp_channel_from_ast_channel(ast);

	if (!c) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel %s. SCCP channel already deleted\n", ast->name);
		sccp_pbx_needcheckringback(d);
		return 0;
	}

	sccp_channel_lock(c);

#if CS_ADV_FEATURES
	/* Try to get the hangup cause from PRI_CAUSE Helper (mIsdn / dahdi) and store the hangupcause in c->pri_cause */
	const char *ds = pbx_builtin_getvar_helper(ast, "PRI_CAUSE");

	if (ds && atoi(ds)) {
		c->pri_cause = atoi(ds);
	} else if (ast->hangupcause) {
		c->pri_cause = ast->hangupcause;
	} else if (ast->_softhangup) {
		c->pri_cause = ast->_softhangup;
	} else {
		c->pri_cause = AST_CAUSE_NORMAL_CLEARING;
	}
	sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "TECH HANGUP [%s] HangCause=%s(%i) ds=%s\n", ast->name, astcause2skinnycause_message(ast->hangupcause), ast->hangupcause,  ds ? ds : "PriCause N/A");
#endif

#ifdef AST_FLAG_ANSWERED_ELSEWHERE
	if (ast_test_flag(ast, AST_FLAG_ANSWERED_ELSEWHERE) || ast->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere");
		c->answered_elsewhere = TRUE;
	}
#endif										// AST_FLAG_ANSWERED_ELSEWHERE
	d = c->device;
	if (c->state != SCCP_CHANNELSTATE_DOWN) {
		if (GLOB(remotehangup_tone) && d && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active_nolock(d))
			sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_ONHOOK);
	}
	CS_AST_CHANNEL_PVT(ast) = NULL;
	c->owner = NULL;							/* TODO: (DD) Isn't this the same as above? Couldn't we move these both lines to the end of this function? */
	l = c->line;
#ifdef CS_SCCP_CONFERENCE
	if (c->conference) {
//		sccp_conference_removeParticipant(c->conference, c);
		sccp_conference_retractParticipatingChannel(c->conference, c);
	}
#endif										// CS_SCCP_CONFERENCE

	if (c->rtp.audio.rtp) {
		sccp_channel_closereceivechannel_locked(c);
		usleep(200);
		sccp_rtp_destroy(c);
		usleep(200);
	}
	// removing scheduled dialing
	SCCP_SCHED_DEL(sched, c->digittimeout);

	sccp_line_lock(c->line);
	c->line->statistic.numberOfActiveChannels--;
	sccp_line_unlock(c->line);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Current channel %s-%08x state %s(%d)\n", (d) ? DEV_ID_LOG(d) : "(null)", l ? l->name : "(null)", c->callid, sccp_indicate2str(c->state), c->state);

	/* end callforwards */
	sccp_channel_t *channel;

	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if (channel->parentChannel == c) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);
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
			assert(linedevice->device);

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

	if (sccp_sched_add(sched, 0, sccp_channel_destroy_callback, c) < 0) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unable to schedule destroy of channel %08X\n", c->callid);
	}

	sccp_channel_unlock(c);

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
 * \param ast Asterisk Channel as ast_channel
 * \return Success as int
 * \note we have no bridged channel at this point
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 */
static int sccp_pbx_answer(struct ast_channel *ast)
{
	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

#if ASTERISK_VERSION_NUM < 10400
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif										// ASTERISK_VERSION_NUM < 10400

	if (!c || !c->line || (!c->device && !c->parentChannel)) {
		ast_log(LOG_ERROR, "SCCP: Answered %s but no SCCP channel\n", ast->name);
		return -1;
	}

	/* XXX perhaps we should lock channel here. */

	if (c->parentChannel) {
		/* we are a forwarded call, bridge me with my parent */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_4 "SCCP: bridge me with my parent, device %s\n", DEV_ID_LOG(c->device));

		struct ast_channel *astChannel = NULL, *br = NULL, *astForwardedChannel = c->parentChannel->owner;

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
			ast_log(LOG_NOTICE, "SCCP: bridge: %s\n", (br) ? br->name : " -- no bridged found -- ");

			/* set the channel and the bridge to state UP to fix problem with fast pickup / autoanswer */
			ast_setstate(ast, AST_STATE_UP);
			ast_setstate(br, AST_STATE_UP);

			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Going to Masquerade %s into %s\n", br->name, astForwardedChannel->name);
			if (ast_channel_masquerade(astForwardedChannel, br)) {
				ast_log(LOG_ERROR, "(sccp_pbx_answer) Failed to masquerade bridge into forwarded channel\n");
				return -1;
			} else {
				c->parentChannel = NULL;
				sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer) Masqueraded into %s\n", astForwardedChannel->name);
			}

			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: ast %s\n", ast_state2str(ast->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: astForwardedChannel %s\n", ast_state2str(astForwardedChannel->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: br %s\n", ast_state2str(br->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) bridged. channel state: astChannel %s\n", ast_state2str(astChannel->_state));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");
			return 0;
		}

		/* we have no bridge and can not make a masquerade -> end call */
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: ast %s\n", ast_state2str(ast->_state));
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) no bridge. channel state: astForwardedChannel %s\n", ast_state2str(astForwardedChannel->_state));
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "(sccp_pbx_answer: call forward) ============================================== \n");

		if (ast->_state == AST_STATE_RING && astForwardedChannel->_state == AST_STATE_DOWN) {
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_4 "SCCP: Receiver Hungup\n");
			astForwardedChannel->hangupcause = AST_CAUSE_CALL_REJECTED;
			astForwardedChannel->_softhangup |= AST_SOFTHANGUP_DEV;
			/* sorry MC functioniert, einfach nicht,reverted */
//			ast_queue_hangup(astForwardedChannel);
			sccp_channel_lock(c->parentChannel);
			sccp_channel_endcall_locked(c->parentChannel);
			sccp_channel_unlock(c->parentChannel);
			
			return 0;
		}
		ast_log(LOG_ERROR, "SCCP: We did not find bridge channel for call forwarding call. Hangup\n");
		astForwardedChannel->hangupcause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		astForwardedChannel->_softhangup |= AST_SOFTHANGUP_DEV;
		/* sorry MC functioniert, einfach nicht, reverted */
//		ast_queue_hangup(astForwardedChannel);
		sccp_channel_lock(c->parentChannel);
		sccp_channel_endcall_locked(c->parentChannel);
		sccp_channel_unlock(c->parentChannel);
		
		sccp_channel_lock(c);
		sccp_channel_endcall_locked(c);
		sccp_channel_unlock(c);
		return -1;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Outgoing call has been answered %s on %s@%s-%08x\n", ast->name, c->line->name, DEV_ID_LOG(c->device), c->callid);
	sccp_channel_lock(c);
	sccp_channel_updateChannelCapability_locked(c);

	/* \todo This seems like brute force, and doesn't seem to be of much use. However, I want it to be remebered
	   as I have forgotten what my actual motivation was for writing this strange code. (-DD) */
	sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_DIALING);
	sccp_channel_send_callinfo(c->device, c);
	sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_PROCEED);
	sccp_channel_send_callinfo(c->device, c);
	sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_CONNECTED);

	sccp_channel_unlock(c);
	return 0;
}

/*!
 * \brief Read from an Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * 
 * \called_from_asterisk
 */
static struct ast_frame *sccp_pbx_read(struct ast_channel *ast)
{
	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

	struct ast_frame *frame;

#if ASTERISK_VERSION_NUM >= 10400
	frame = &pbx_null_frame;
#else
	frame = NULL;
#endif										// ASTERISK_VERSION_NUM >= 10400

	if (!c || !c->rtp.audio.rtp)
		return frame;

	switch (ast->fdno) {
#ifndef CS_AST_HAS_RTP_ENGINE
	case 0:
		frame = ast_rtp_read(c->rtp.audio.rtp);				/* RTP Audio */
		break;
	case 1:
		frame = ast_rtcp_read(c->rtp.audio.rtp);			/* RTCP Control Channel */
		break;
	case 2:
#    ifdef CS_SCCP_VIDEO
		frame = ast_rtp_read(c->rtp.video.rtp);				/* RTP Video */
//                      sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s: Got Video frame from device; frametype: %d, subclass:%d\n", DEV_ID_LOG(c->device), frame->frametype, frame->subclass);
		if (frame) {
			frame->frametype = AST_FRAME_VIDEO;
		}
#    endif									// CS_SCCP_VIDEO
		break;
	case 3:
		frame = ast_rtcp_read(c->rtp.video.rtp);			/* RTCP Control Channel for video */
		break;
#else
	case 0:
		frame = ast_rtp_instance_read(c->rtp.audio.rtp, 0);		/* RTP Audio */
		break;
	case 1:
		frame = ast_rtp_instance_read(c->rtp.audio.rtp, 1);		/* RTCP Control Channel */
		break;
	case 2:
		frame = ast_rtp_instance_read(c->rtp.video.rtp, 0);		/* RTP Video */
		break;
	case 3:
		frame = ast_rtp_instance_read(c->rtp.video.rtp, 1);		/* RTCP Control Channel for video */
		break;
#endif										// CS_AST_HAS_RTP_ENGINE
	default:
		return frame;
	}

	if (!frame) {
		ast_log(LOG_WARNING, "%s: error reading frame == NULL\n", DEV_ID_LOG(c->device));
		return frame;
	}
	//sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: read format: ast->fdno: %d, frametype: %d, %s(%d)\n", DEV_ID_LOG(c->device), ast->fdno, frame->frametype, pbx_getformatname(frame->subclass), frame->subclass);

	if (frame->frametype == AST_FRAME_VOICE) {
		//sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: read format: %s(%d)\n",
		//                              DEV_ID_LOG(c->device),
		//                              pbx_getformatname(frame->subclass),
		//                              frame->subclass);
#if ASTERISK_VERSION_NUM >= 10400
		if (!(frame->subclass & (ast->nativeformats & AST_FORMAT_AUDIO_MASK)))
#else
		if (!(frame->subclass & (ast->nativeformats)))
#endif										// ASTERISK_VERSION_NUM >= 10400
		{
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Channel %s changed format from %s(%d) to %s(%d)\n", DEV_ID_LOG(c->device), ast->name, pbx_getformatname(ast->nativeformats), ast->nativeformats, pbx_getformatname(frame->subclass), frame->subclass);

			ast->nativeformats = frame->subclass;
			ast_set_read_format(ast, ast->readformat);
			ast_set_write_format(ast, ast->writeformat);
		}
	}
	return frame;
}

/*!
 * \brief Write to an Asterisk Channel
 * \param ast Channel as ast_channel
 * \param frame Frame as ast_frame
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_write(struct ast_channel *ast, struct ast_frame *frame)
{
	int res = 0;

	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

	if (c) {
		switch (frame->frametype) {
		case AST_FRAME_VOICE:
			// checking for samples to transmit
			if (!frame->samples) {
				if (strcasecmp(frame->src, "ast_prod")) {
					ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", DEV_ID_LOG(c->device), frame->subclass);
					return -1;
				} else {
					// frame->samples == 0  when frame_src is ast_prod
					sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", DEV_ID_LOG(c->device), ast->name);
				}
			} else if (!(frame->subclass & ast->nativeformats)) {
				char s1[512], s2[512], s3[512];

				ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%d) read/write = %s(%d)/%s(%d)\n", DEV_ID_LOG(c->device), frame->subclass, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, ast->writeformat), ast->writeformat);
				//return -1;
			}
			if (c->rtp.audio.rtp) {
				res = sccp_rtp_write(c->rtp.audio.rtp, frame);
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if ((c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) == 0 && c->rtp.video.rtp && c->device && (frame->subclass & AST_FORMAT_VIDEO_MASK)
			    //      && (c->device->capability & frame->subclass) 
			    ) {
				ast_log(LOG_NOTICE, "%s: got video frame\n", DEV_ID_LOG(c->device));
				c->rtp.video.writeFormat = (frame->subclass & AST_FORMAT_VIDEO_MASK);
				sccp_channel_openMultiMediaChannel(c);
			}
#endif
			if (c->rtp.video.rtp && (c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) != 0) {
				res = sccp_rtp_write(c->rtp.video.rtp, frame);
			}
			break;

		case AST_FRAME_TEXT:
#if ASTERISK_VERSION_NUM >= 10400
		case AST_FRAME_MODEM:
#endif										// ASTERISK_VERSION_NUM >= 10400
		default:
			ast_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %s\n", DEV_ID_LOG(c->device), frame->frametype, ast->name);
			break;
		}
	}
	return res;
}

/*!
 * \brief Convert Control 2 String
 * \param state AST_CONTROL_* as int
 * \return Asterisk Control State String
 */
static char *sccp_control2str(int state)
{
	switch (state) {
	case AST_CONTROL_HANGUP:
		return "Other end has hungup";
	case AST_CONTROL_RING:
		return "Local Ring";
	case AST_CONTROL_RINGING:
		return "Remote end is ringing";
	case AST_CONTROL_ANSWER:
		return "Remote end has answered";
	case AST_CONTROL_BUSY:
		return "Remote end is busy";
	case AST_CONTROL_TAKEOFFHOOK:
		return "Make it go off hook";
	case AST_CONTROL_OFFHOOK:
		return "Line is off hook";
	case AST_CONTROL_CONGESTION:
		return "Congestion (all cirtuits are busy)";
	case AST_CONTROL_FLASH:
		return "Flash Hook";
	case AST_CONTROL_WINK:
		return "Wink";
	case AST_CONTROL_OPTION:
		return "Set a low-level option";
	case AST_CONTROL_RADIO_KEY:
		return "Key Radio";
	case AST_CONTROL_RADIO_UNKEY:
		return "Un-key Radio";
	case AST_CONTROL_PROGRESS:
		return "Remote end is making progress";
	case AST_CONTROL_PROCEEDING:
		return "Remote end is proceeding";
#ifdef CS_AST_CONTROL_HOLD
	case AST_CONTROL_HOLD:
		return "Hold";
	case AST_CONTROL_UNHOLD:
		return "UnHold";
#endif										// CS_AST_CONTROL_HOLD
#ifdef CS_AST_CONTROL_VIDUPDATE
	case AST_CONTROL_VIDUPDATE:
		return "Video Frame Update";
#endif										// CS_AST_CONTROL_VIDUPDATE
#ifdef CS_AST_CONTROL_T38
	case AST_CONTROL_T38:
		return "T38 Request Notification";
#endif										// AST_CONTROL_T38
#ifdef CS_AST_CONTROL_T38_PARAMETERS
	case AST_CONTROL_T38_PARAMETERS:
		return "T38 Request Notification";
#endif										// AST_CONTROL_T38_PARAMETERS
#ifdef CS_AST_CONTROL_SRCUPDATE
	case AST_CONTROL_SRCUPDATE:
		return "Media Source Update";
#endif										// CS_AST_CONTROL_SRCUPDATE
#ifdef CS_AST_CONTROL_SRCCHANGE
	case AST_CONTROL_SRCCHANGE:
		return "Media Source Change";
#endif										// CS_AST_CONTROL_SRCCHANGE
	case -1:
		return "Channel Prodding (Stop Tone)";
	default:
		return "Unknown";
	}
}

#if ASTERISK_VERSION_NUM < 10400

/*!
 * \brief Indicate from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param ind AST_CONTROL_* State as int (Indication)
 * \return Result as int
 *
 * \callgraph
 * \callergraph
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_indicate(struct ast_channel *ast, int ind)
#else

/*!
 * \brief Indicate from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param ind AST_CONTROL_* State as int (Indication)
 * \param data Data
 * \param datalen Data Length as size_t
 * \return Result as int
 *
 * \callgraph
 * \callergraph
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_closereceivechannel()
 * 	  - see sccp_channel_openreceivechannel()
 */
static int sccp_pbx_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen)
#endif										// ASTERISK_VERSION_NUM < 10400
{
	int oldChannelFormat, oldChannelReqFormat;

	struct ast_channel *astcSourceRemote = NULL;

	sccp_channel_t *cSourceRemote = NULL;
	
	boolean_t canDoCOLP = FALSE;


	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

	int res = 0;

	int deadlockAvoidanceCounter = 0;

	if (!c)
		return -1;

	if (!c->device)
		return -1;

	while (sccp_channel_trylock(c)) {
		if (deadlockAvoidanceCounter++ > 100) {
			ast_log(LOG_ERROR, "SCCP: We have a deadlock in %s:%d\n", __FILE__, __LINE__);
			return -1;
		}

		usleep(100);
	}

	if (c->state == SCCP_CHANNELSTATE_DOWN) {
		sccp_channel_unlock(c);
		return -1;
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' (%s) condition on channel %s\n", DEV_ID_LOG(c->device), ind, sccp_control2str(ind), ast->name);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: with cause '%d' (%s)\n", DEV_ID_LOG(c->device), ast->hangupcause, ast_cause2str(ast->hangupcause));

	struct ast_channel *bridge = ast_bridged_channel(ast);

	if (NULL != bridge) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: with cause at bridge '%d' (%s)\n", DEV_ID_LOG(c->device), bridge->hangupcause, ast_cause2str(bridge->hangupcause));
	}

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = ((c->device->earlyrtp || c->rtp.audio.rtp) ? -1 : 0);

	switch (ind) {
	case AST_CONTROL_RINGING:
		if (SKINNY_CALLTYPE_OUTBOUND == c->calltype) {
			// Allow signalling of RINGOUT only on outbound calls.
			// Otherwise, there are some issues with late arrival of ringing
			// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
			sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_RINGOUT);
		}
		break;
	case AST_CONTROL_BUSY:
		sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_BUSY);
		break;
	case AST_CONTROL_CONGESTION:
		sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_CONGESTION);
		break;
	case AST_CONTROL_PROGRESS:
		sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_PROGRESS);
		break;
	case AST_CONTROL_PROCEEDING:
		sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_PROCEED);
		break;

#ifdef CS_AST_CONTROL_SRCCHANGE
	case AST_CONTROL_SRCCHANGE:
		RTP_NEW_SOURCE(c, "Source Change");
		break;
#endif										//CS_AST_CONTROL_SRCCHANGE
#ifdef CS_AST_CONTROL_SRCUPDATE
	case AST_CONTROL_SRCUPDATE:
#endif										//CS_AST_CONTROL_SRCUPDATE

#if defined(CS_AST_CONTROL_SRCCHANGE) || defined(CS_AST_CONTROL_SRCUPDATE)
		/* Source media has changed. */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

		/* update channel format */
		oldChannelReqFormat = c->requestedFormat;
		oldChannelFormat = c->format;
		c->requestedFormat = ast->rawreadformat;

		if (oldChannelReqFormat != c->requestedFormat) {
			/* notify of changing format */
			sccp_channel_updateChannelCapability_locked(c);
		}

		ast_log(LOG_NOTICE, "SCCP: SCCP/%s-%08x, state: %s(%d) \n", c->line->name, c->callid, sccp_indicate2str(c->state), c->state);
		if (c->rtp.audio.rtp) {
			if (oldChannelFormat != c->format) {
				if (c->mediaStatus.receive == TRUE || c->mediaStatus.transmit == TRUE) {
					sccp_channel_closereceivechannel_locked(c);	/* close the already openend receivechannel */
					sccp_channel_openreceivechannel_locked(c);	/* reopen it */
				}
			}
		}
		
			/* TODO: Handle COLP here */
		
		pbx_channel_lock(c->owner);
		astcSourceRemote = CS_AST_BRIDGED_CHANNEL(c->owner);
		pbx_channel_lock(astcSourceRemote);

	
	if (!astcSourceRemote) {
		ast_log(LOG_WARNING, "SCCP: Failed to make COLP decision on answer - no bridged channel. Weird.\n");
	} else if (CS_AST_CHANNEL_PVT_IS_SCCP(astcSourceRemote)) {
		canDoCOLP = TRUE;
		cSourceRemote = CS_AST_CHANNEL_PVT(astcSourceRemote);
	}

	if (canDoCOLP) {
		sccp_channel_lock(c);
		sccp_channel_lock(cSourceRemote);
		sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "Performing COLP signalling between two SCCP devices.\n");
		
		
				if (c->calltype == SKINNY_CALLTYPE_OUTBOUND) {
					/* First case of COLP: Signal our CID to the remote caller. */
					/* copy old callerid */
					sccp_copy_string(c->callInfo.originalCalledPartyName, c->callInfo.calledPartyName, sizeof(c->callInfo.originalCalledPartyName));
					sccp_copy_string(c->callInfo.originalCalledPartyNumber, c->callInfo.calledPartyNumber, sizeof(c->callInfo.originalCalledPartyNumber));

					sccp_copy_string(c->callInfo.calledPartyName, cSourceRemote->callInfo.calledPartyName, sizeof(c->callInfo.calledPartyName));
					sccp_copy_string(c->callInfo.calledPartyNumber, cSourceRemote->callInfo.calledPartyNumber, sizeof(c->callInfo.calledPartyNumber));
					
				} else if (c->calltype == SKINNY_CALLTYPE_INBOUND) {
				
					/* Second part of COLP: Signal remote CID to us. */
					/* copy old callerid */
					sccp_copy_string(c->callInfo.originalCallingPartyName, c->callInfo.callingPartyName, sizeof(c->callInfo.originalCallingPartyName));
					sccp_copy_string(c->callInfo.originalCallingPartyNumber, c->callInfo.callingPartyNumber, sizeof(c->callInfo.originalCallingPartyNumber));

					sccp_copy_string(c->callInfo.callingPartyName, cSourceRemote->callInfo.callingPartyName, sizeof(c->callInfo.callingPartyName));
					sccp_copy_string(c->callInfo.callingPartyNumber, cSourceRemote->callInfo.callingPartyNumber, sizeof(c->callInfo.callingPartyNumber));
				}
		sccp_channel_unlock(c);
		sccp_channel_unlock(cSourceRemote);

				if(c->device) {
					sccp_channel_send_callinfo(c->device, c);
				};
	}
	pbx_channel_unlock(astcSourceRemote);
	pbx_channel_unlock(c->owner);
		
		/* TODO COLP END */
		
#if ASTERISK_VERSION_NUM >= 10620
		//FIXME check for asterisk 1.6 and 1.4
		RTP_CHANGE_SOURCE(c, "Source Update: RTP NEW SOURCE");
#endif
		res = 0;
		break;
#endif										//defined(CS_AST_CONTROL_SRCCHANGE) || defined(CS_AST_CONTROL_SRCUPDATE)
#ifdef CS_AST_CONTROL_HOLD
		/* when the bridged channel hold/unhold the call we are notified here */
	case AST_CONTROL_HOLD:
		pbx_moh_start(ast, data, c->musicclass);
		res = 0;
		break;
	case AST_CONTROL_UNHOLD:
		pbx_moh_stop(ast);
		RTP_NEW_SOURCE(c, "Source Update");
		res = 0;
		break;
#endif										// CS_AST_CONTROL_HOLD
#ifdef CS_AST_CONTROL_CONNECTED_LINE
	case AST_CONTROL_CONNECTED_LINE:
		sccp_pbx_update_connectedline(ast, data, datalen);
		sccp_indicate_locked(c->device, c, c->state);
		break;
#endif										// CS_AST_CONTROL_CONNECTED_LINE
	case AST_CONTROL_VIDUPDATE:						/* Request a video frame update */
		res = 0;
		break;
	case -1:								// Asterisk prod the channel
		break;
	default:
		ast_log(LOG_WARNING, "SCCP: Don't know how to indicate condition %d\n", ind);
	}

	sccp_channel_unlock(c);
	return res;
}

#ifdef CS_AST_CONTROL_CONNECTED_LINE

/*!
 * \brief Update Connected Line
 * \param channel Asterisk Channel as ast_channel
 * \param data Asterisk Data
 * \param datalen Asterisk Data Length
 */
static void sccp_pbx_update_connectedline(sccp_channel_t * channel, const void *data, size_t datalen)
{
	struct ast_channel *ast_channel = channel->owner;

	if (!ast_channel)
		return;

	if (sccp_strlen_zero(ast_channel->connected.id.name) || sccp_strlen_zero(ast_channel->connected.id.number))
		return;

	sccp_channel_set_calledparty(c, ast_channel->connected.id.name, ast_channel->connected.id.number);
}
#endif										// CS_AST_CONTROL_CONNECTED_LINE

/*!
 * \brief Asterisk Fix Up
 * \param oldchan Asterisk Channel as ast_channel
 * \param newchan Asterisk Channel as ast_channel
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s\n", newchan->name);

	sccp_channel_t *c = get_sccp_channel_from_ast_channel(newchan);

	if (!c) {
		ast_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, (void *)oldchan, newchan->name, (void *)newchan);
		return -1;
	}

	c->owner = newchan;

	return 0;
}

#if ASTERISK_VERSION_NUM >= 10400

/*!
 * \brief Receive First Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit First Digit as char
 * \return Always Return -1 as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_recvdigit_begin(struct ast_channel *ast, char digit)
{
	return -1;
}
#endif										// ASTERISK_VERSION_NUM >= 10400

#if ASTERISK_VERSION_NUM < 10400

/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \return Always Return -1 as int
 * \todo FIXME Always returns -1
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit)
#else

/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \param duration Duration as int
 * \return Always Return -1 as int
 * \todo FIXME Always returns -1
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit, unsigned int duration)
#endif										// ASTERISK_VERSION_NUM < 10400
{
	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

	sccp_device_t *d = NULL;

	return -1;

	if (!c || !c->device)
		return -1;

	d = c->device;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, (d->dtmfmode) ? "outofband" : "inband");

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		return -1;
	}

/* 
	if (d->dtmfmode == SCCP_DTMFMODE_INBAND)
		return -1;
*/
	sccp_dev_keypadbutton(c->device, digit, sccp_device_find_index_for_line(c->device, c->line->name), c->callid);

	return 0;
}

#ifdef CS_AST_HAS_TECH_PVT

/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_sendtext(struct ast_channel *ast, const char *text)
#else

/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_sendtext(struct ast_channel *ast, char *text)
#endif										// CS_AST_HAS_TECH_PVT
{
	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

	sccp_device_t *d;

	uint8_t instance;

	if (!c || !c->device)
		return -1;

	d = c->device;
	if (!text || sccp_strlen_zero(text))
		return 0;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_displayprompt(d, instance, c->callid, (char *)text, 10);
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
//      sccp_device_t                   *d = c->device;
	struct ast_channel *tmp;

	sccp_line_t *l = c->line;

	int fmt;

#ifndef CS_AST_CHANNEL_HAS_CID
	char cidtmp[256];

	memset(&cidtmp, 0, sizeof(cidtmp));
#endif										// CS_AST_CHANNEL_HAS_CID

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: try to allocate channel \n");
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Line: %s\n", l->name);

	if (!l) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Unable to allocate asterisk channel %s\n", l->name);
		ast_log(LOG_ERROR, "SCCP: Unable to allocate asterisk channel\n");
		return 0;
	}
//      /* Don't hold a sccp pvt lock while we allocate a channel */
	if (c->device) {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice->device == c->device)
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

	/* This should definetly fix CDR */
	tmp = pbx_channel_alloc(1, AST_STATE_DOWN, c->callInfo.callingPartyNumber, c->callInfo.callingPartyName, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08x", l->name, c->callid);
	if (!tmp) {
		ast_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", l->id, l->name);
		return 0;
	}

	/* need to reset the exten, otherwise it would be set to s */
	memset(&tmp->exten, 0, sizeof(tmp->exten));

	/* let's connect the ast channel to the sccp channel */
	c->owner = tmp;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Global Capabilities: %d\n", l->id, GLOB(global_capability));

	// \todo TODO check locking
	/* XXX we should remove this shit. */
	while (sccp_line_trylock(l)) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		usleep(1);
	}
	sccp_channel_updateChannelCapability_locked(c);
	if (!tmp->nativeformats) {
		ast_log(LOG_ERROR, "%s: No audio format to offer. Cancelling call on line %s\n", l->id, l->name);
		return 0;
	}
	fmt = tmp->readformat;

#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(tmp, name, "SCCP/%s-%08x", l->name, c->callid);
#else
	snprintf(tmp->name, sizeof(tmp->name), "SCCP/%s-%08x", l->name, c->callid);
#endif										// CS_AST_HAS_AST_STRING_FIELD

	sccp_line_unlock(l);

	pbx_jb_configure(tmp, &GLOB(global_jbconf));

	char s1[512], s2[512];

	sccp_log(2) (VERBOSE_PREFIX_3 "%s: Channel %s, capabilities: CHANNEL %s(%d) PREFERRED %s(%d) USED %s(%d)\n", l->id, tmp->name, pbx_getformatname_multiple(s1, sizeof(s1) - 1, c->capability), c->capability, pbx_getformatname_multiple(s2, sizeof(s2) - 1, tmp->nativeformats), tmp->nativeformats, pbx_getformatname(fmt), fmt);

#ifdef CS_AST_HAS_TECH_PVT
	tmp->tech = &sccp_tech;
	tmp->tech_pvt = c;
#else
	tmp->type = "SCCP";
	tmp->pvt->pvt = c;
	tmp->pvt->answer = sccp_pbx_answer;
	tmp->pvt->hangup = sccp_pbx_hangup;
	tmp->pvt->call = sccp_pbx_call;
	tmp->pvt->read = sccp_pbx_read;
	tmp->pvt->write = sccp_pbx_write;
	tmp->pvt->indicate = sccp_pbx_indicate;
	tmp->pvt->fixup = sccp_pbx_fixup;
	tmp->pvt->send_digit = sccp_pbx_recvdigit;
	tmp->pvt->send_text = sccp_pbx_sendtext;
#endif										// CS_AST_HAS_TECH_PVT
	tmp->adsicpe = AST_ADSI_UNAVAILABLE;

	// XXX: Bridge?
	// XXX: Transfer?
	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)++;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

#ifdef CS_AST_CHANNEL_HAS_CID
	if (l->cid_num) {
		tmp->cid.cid_num = strdup(c->callInfo.callingPartyNumber);
	}
	if (l->cid_name)
		tmp->cid.cid_name = strdup(c->callInfo.callingPartyName);
#else
	snprintf(cidtmp, sizeof(cidtmp), "\"%s\" <%s>", c->callInfo.callingPartyName, c->callInfo.callingPartyNumber);
	tmp->callerid = strdup(cidtmp);
#endif										// CS_AST_CHANNEL_HAS_CID

	sccp_copy_string(tmp->context, l->context, sizeof(tmp->context));
	if (!sccp_strlen_zero(l->language))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, language, l->language);
#else
		sccp_copy_string(tmp->language, l->language, sizeof(tmp->language));
#endif										// CS_AST_HAS_AST_STRING_FIELD
	if (!sccp_strlen_zero(l->accountcode))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, accountcode, l->accountcode);
#else
		sccp_copy_string(tmp->accountcode, l->accountcode, sizeof(tmp->accountcode));
#endif										// CS_AST_HAS_AST_STRING_FIELD
	if (!sccp_strlen_zero(l->musicclass))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, musicclass, c->musicclass);
#else
		sccp_copy_string(tmp->musicclass, l->musicclass, sizeof(tmp->musicclass));
#endif										// CS_AST_HAS_AST_STRING_FIELD
	tmp->amaflags = l->amaflags;
#ifdef CS_SCCP_PICKUP
	tmp->callgroup = l->callgroup;
	tmp->pickupgroup = l->pickupgroup;
#endif										// CS_SCCP_PICKUP
	tmp->priority = 1;

	// export sccp informations in asterisk dialplan
	if (c->device) {
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_MAC", c->device->id);
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_IP", pbx_inet_ntoa(c->device->session->sin.sin_addr));
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_TYPE", devicetype2str(c->device->skinny_type));
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
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Timeout for call '%d'. Going to dial '%s'\n", c->callid, c->dialedNumber);
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
int sccp_pbx_helper(sccp_channel_t * c)
{
	struct ast_channel *chan = c->owner;

	sccp_line_t *l = c->line;

	sccp_device_t *d = c->device;

	if (!l || !d || !chan)
		return 0;

	if (!sccp_strlen_zero(c->dialedNumber)) {
		if (GLOB(recorddigittimeoutchar) && GLOB(digittimeoutchar) == c->dialedNumber[strlen(c->dialedNumber) - 1]) {
			return 1;
		}
	}

	int ignore_pat = ast_ignore_pattern(chan->context, c->dialedNumber);

	int ext_exist = ast_exists_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);

	int ext_canmatch = ast_canmatch_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);

	int ext_matchmore = ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);

	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_2 "%s: extension helper says: ignore pattern: %d, exten_exists: %d, exten_canmatch: %d, exten_matchmore: %d\n", d->id, ignore_pat, ext_exist, ext_canmatch, ext_matchmore);

#if CS_ADV_FEATURES
	/* using the regcontext to match numbers early. Only registered SCCP phones will turn up here with their dialplan extension */
	int regcontext_exist = ast_exists_extension(chan, GLOB(regcontext), c->dialedNumber, 1, l->cid_num);
	int regcontext_matchmore = ast_matchmore_extension(chan, GLOB(regcontext), c->dialedNumber, 1, l->cid_num);
	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_2 "%s: extension helper says: regcontext_exist: %d, regcontext_matchmore: %d\n", d->id, regcontext_exist, regcontext_matchmore);
#endif

	if ((c->ss_action != SCCP_SS_GETCBARGEROOM) && (c->ss_action != SCCP_SS_GETMEETMEROOM) && (!ignore_pat) && (ext_exist)) {
#if CS_ADV_FEATURES
		/* dial fast if number exists in regcontext and only matches exactly number */
		if (regcontext_exist && !regcontext_matchmore) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "%s: Entered Number: %s is a Registered SCCP Number\n", d->id, c->dialedNumber);
			return 2;
		}
#endif
		if ((d->overlapFeature.enabled && !ext_canmatch) || (!d->overlapFeature.enabled && !ext_matchmore)) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "%s: Entered Number: %s is a Full Match\n", d->id, c->dialedNumber);
			return 1;
		}
	}

	return 0;
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
 * 	  - see sccp_ast_setstate()
 * 	  - see ast_pbx_start()
 * 	  - see sccp_indicate_nolock()
 * 	  - see manager_event()
 */
void *sccp_pbx_softswitch_locked(sccp_channel_t * c)
{
	if (!c) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <channel> available. Returning from dial thread.\n");
		return NULL;
	}

	/* prevent softswitch from being executed twice (Pavel Troller / 15-Oct-2010) */
	if (c->owner->pbx) {
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: (sccp_pbx_softswitch) PBX structure already exists. Dialing instead of starting.\n");
		/* If there are any digits, send them instead of starting the PBX */
		if (sccp_is_nonempty_string(c->dialedNumber)) {
			sccp_pbx_senddigits(c, c->dialedNumber);
			sccp_channel_set_calledparty(c, c->dialedNumber, c->dialedNumber);
			if (c->device)
				sccp_indicate_locked(c->device, c, SCCP_CHANNELSTATE_DIALING);
		}
		return NULL;
	}

	struct ast_channel *chan = c->owner;

	struct ast_variable *v = NULL;

	uint8_t instance;

	unsigned int len = 0;

	sccp_line_t *l;

	sccp_device_t *d;

	char shortenedNumber[256] = { '\0' };					/* For recording the digittimeoutchar */

	/* removing scheduled dialing */
	SCCP_SCHED_DEL(sched, c->digittimeout);

	/* we should just process outbound calls, let's check calltype */
	if (c->calltype != SKINNY_CALLTYPE_OUTBOUND) {
		return NULL;
	}

	/* assume d is the channel's device */
	d = c->device;

	/* does it exists ? */
	if (!d) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> available. Returning from dial thread.\n");
		return NULL;
	}

	/* we don't need to check for a device type but just if the device has an id, otherwise back home  -FS */
	if (!d->id || sccp_strlen_zero(d->id)) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> identifier available. Returning from dial thread.\n");
		return NULL;
	}

	l = c->line;
	if (!l) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <line> available. Returning from dial thread.\n");
		if (chan)
			ast_hangup(chan);
		return NULL;
	}

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) New call on line %s\n", DEV_ID_LOG(d), l->name);

	/* assign callerid name and number */
	
	/* CAVE: This should not be done here. It overwrites the complicated detailed callerid (including shared line subscription IDs)
	        which has already been set in sccp_pbx_allocate_channel. (-DD). */
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
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to pickup exten '%s'\n", shortenedNumber);
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
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request for room '%s' on extension '%s'\n", d->id, shortenedNumber, c->line->meetmenum);
			if (c->owner && !pbx_check_hangup(c->owner))
				pbx_builtin_setvar_helper(c->owner, "SCCP_MEETME_ROOM", shortenedNumber);
			sccp_copy_string(shortenedNumber, c->line->meetmenum, sizeof(shortenedNumber));

			//sccp_copy_string(c->dialedNumber, SKINNY_DISP_CONFERENCE, sizeof(c->dialedNumber));
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Start Meetme Thread\n", d->id);
			sccp_feat_meetme_start(c);				/* Copied from Federico Santulli */
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme Thread Started\n", d->id);
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
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge exten '%s'\n", d->id, shortenedNumber);
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
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge conference '%s'\n", d->id, shortenedNumber);
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
			chan->cid.cid_pres = AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
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
	    && ast_exists_extension(chan, chan->context, shortenedNumber, 1, l->cid_num)) {
		/* found an extension, let's dial it */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x is dialing number %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		sccp_ast_setstate(c, AST_STATE_RING);
		if (ast_pbx_start(chan)) {
			/* \todo actually the next line is not correct. ast_pbx_start returns false when it was not able to start a new thread correcly */
			ast_log(LOG_ERROR, "%s: (sccp_pbx_softswitch) channel %s-%08x failed to start new thread to dial %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
			sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
		}
#ifdef CS_MANAGER_EVENTS
		if (GLOB(callevents)) {
			manager_event(EVENT_FLAG_SYSTEM, "ChannelUpdate", "Channel: %s\r\nUniqueid: %s\r\nChanneltype: %s\r\nSCCPdevice: %s\r\nSCCPline: %s\r\nSCCPcallid: %s\r\n", chan->name, chan->uniqueid, "SCCP", (d) ? DEV_ID_LOG(d) : "(null)", (l) ? l->name : "(null)", (c) ? (char *)&c->callid : "(null)");
		}
#endif										// CS_MANAGER_EVENTS
	} else {
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
	struct ast_frame f;

	f = pbx_null_frame;
	f.frametype = AST_FRAME_DTMF;
	f.src = "SCCP";
	f.subclass = digit;
	sccp_queue_frame(c, &f);
}

/*!
 * \brief Send Multiple Digits to Asterisk
 * \param c SCCP Channel
 * \param digits Multiple Digits as char
 */
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION])
{
	int i;

	struct ast_frame f;

	f = pbx_null_frame;
	f.frametype = AST_FRAME_DTMF;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digits %s\n", DEV_ID_LOG(c->device), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	f.offset = 0;
	f.datalen = 0;
#ifdef CS_AST_NEW_FRAME_STRUCT
	f.data.ptr = NULL;
#else
	f.data = NULL;
#endif										// CS_AST_NEW_FRAME_STRUCT
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass = digits[i];
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(c->device), digits[i]);
		sccp_queue_frame(c, &f);
	}
}

/*!
 * \brief Queue an outgoing Asterisk frame
 * \param c SCCP Channel
 * \param f Asterisk Frame
 *
 * \note should be moved to sccp_pbx_wrapper
 */
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame *f)
{
	if (c && c->owner)
		ast_queue_frame(c->owner, f);

#if 0										/* \todo remove/reimplement (why this piece of code is removed? -romain) */

/*
	for(;;) {
		if (c && c->owner && c->owner->_state == AST_STATE_UP) {
			if (!sccp_ast_channel_trylock(c->owner)) {
				ast_queue_frame(c->owner, f);
				pbx_channel_unlock(c->owner);
				break;
			} else {
				// strange deadlocks happens here :D -FS
				// sccp_channel_unlock(c);
				usleep(1);
				// sccp_channel_lock(c);
			}
		} else
			break;
	}
*/
#endif
}

/*!
 * \brief Queue a control frame
 * \param c SCCP Channel
 * \param control as Asterisk Control Frame Type
 */
#if ASTERISK_VERSION_NUM >= 10400
int sccp_ast_queue_control(sccp_channel_t * c, enum ast_control_frame_type control)
#else
int sccp_ast_queue_control(sccp_channel_t * c, uint8_t control)
#endif										// ASTERISK_VERSION_NUM >= 10400
{
	struct ast_frame f;

	f = pbx_null_frame;
	f.frametype = AST_FRAME_DTMF;
	f.subclass = control;
	sccp_queue_frame(c, &f);
	return 0;
}

#if ASTERISK_VERSION_NUM >= 10600

/*!
 * \brief Deliver SCCP call ID for the call
 * \param ast Asterisk Channel
 * \return result as char *
 *
 * \test Deliver SCCP Call ID needs to be tested
 */
static const char *sccp_pbx_get_callid(struct ast_channel *ast)
{
	static char callid[8];

	sccp_channel_t *c;

	c = get_sccp_channel_from_ast_channel(ast);
	if (c) {
		snprintf(callid, sizeof callid, "%i", c->callid);
		sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_1 "Get CallID is Returning %s ('%i')\n", callid, c->callid);
		return callid;
	} else {
		return "";
	}
}

#endif										// ASTERISK_VERSION_NUM >= 10600

#ifdef CS_AST_HAS_TECH_PVT

/*!
 * \brief SCCP Tech Structure
 */
const struct ast_channel_tech sccp_tech = {
	.type = SCCP_TECHTYPE_STR,
	.description = "Skinny Client Control Protocol (SCCP)",
#if ASTERISK_VERSION_NUM >= 10600
	.capabilities = AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_G722 | AST_FORMAT_G729A | AST_FORMAT_ILBC | AST_FORMAT_H263 | AST_FORMAT_H264 | AST_FORMAT_H263_PLUS | AST_FORMAT_SLINEAR16 | AST_FORMAT_GSM,
#    else
	.capabilities = AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_G722 | AST_FORMAT_G729A | AST_FORMAT_H263 | AST_FORMAT_H264 | AST_FORMAT_H263_PLUS | AST_FORMAT_GSM,
#endif
	.properties = AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester = sccp_request,
	.devicestate = sccp_devicestate,
	.call = sccp_pbx_call,
	.hangup = sccp_pbx_hangup,
	.answer = sccp_pbx_answer,
	.read = sccp_pbx_read,
	.write = sccp_pbx_write,
	.write_video = sccp_pbx_write,
	.indicate = sccp_pbx_indicate,
	.fixup = sccp_pbx_fixup,
	.send_text = sccp_pbx_sendtext,

#    if ASTERISK_VERSION_NUM < 10400
	.send_digit = sccp_pbx_recvdigit_end,
#    else
	.send_digit_begin = sccp_pbx_recvdigit_begin,
	.send_digit_end = sccp_pbx_recvdigit_end,
	.bridge = sccp_rtp_bridge,

	.transfer = sccp_pbx_transfer,						// new >1.4.0
	.func_channel_read = acf_channel_read,					// new

#    endif									// ASTERISK_VERSION_NUM < 10400

#    if ASTERISK_VERSION_NUM >= 10600
	.early_bridge = ast_rtp_early_bridge,
	.get_pvt_uniqueid = sccp_pbx_get_callid,				// new >1.6.0
#    endif									// ASTERISK_VERSION_NUM >= 10600
};
#endif										// CS_AST_HAS_TECH_PVT

#if ASTERISK_VERSION_NUM > 10400
enum ast_bridge_result sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms)
{
	enum ast_bridge_result res;

	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	sccp_channel_t *sc0, *sc1;

	if ((sc0 = get_sccp_channel_from_ast_channel(c0)) && (sc1 = get_sccp_channel_from_ast_channel(c1))) {
		// Switch off DTMF between SCCP phones
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_0;
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_1;
		if ((sc0->device->directrtp && sc1->device->directrtp) || GLOB(directrtp)) {
			ast_channel_defer_dtmf(c0);
			ast_channel_defer_dtmf(c1);
		}
		sc0->peerIsSCCP = 1;
		sc1->peerIsSCCP = 1;
		// SCCP Key handle direction to asterisk is still to be implemented here
		// sccp_pbx_senddigit
	} else {
		// Switch on DTMF between differing channels
		ast_channel_undefer_dtmf(c0);
		ast_channel_undefer_dtmf(c1);
	}
	res = ast_rtp_bridge(c0, c1, new_flags, fo, rc, timeoutms);
	switch (res) {
	case AST_BRIDGE_COMPLETE:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Complete\n", c0->name, c1->name);
		break;
	case AST_BRIDGE_FAILED:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed\n", c0->name, c1->name);
		break;
	case AST_BRIDGE_FAILED_NOWARN:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed NoWarn\n", c0->name, c1->name);
		break;
	case AST_BRIDGE_RETRY:
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed Retry\n", c0->name, c1->name);
		break;
	}
	/* \todo Implement callback function queue upon completion */
	return res;
}
#endif										// ASTERISK_VERSION_NUM > 10400

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
int sccp_pbx_transfer(struct ast_channel *ast, const char *dest)
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
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "Transferring '%s' to '%s'\n", ast->name, dest);
	if (ast->_state == AST_STATE_RING) {
		// \todo Blindtransfer needs to be implemented correctly

/*
		res = sccp_blindxfer(p, dest);
*/
		res = -1;
	} else {
		// \todo Transfer needs to be implemented correctly

/*
		res=sccp_channel_transfer(p,dest);
*/
		res = -1;
	}
	return res;
}

/*!
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
int acf_channel_read(struct ast_channel *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen)
{
	sccp_channel_t *c;

#ifdef CS_AST_HAS_TECH_PVT
	if (!ast || ast->tech != &sccp_tech) {
		ast_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}
#endif										// CS_AST_HAS_TECH_PVT
	c = get_sccp_channel_from_ast_channel(ast);
	if (c == NULL)
		return -1;

	if (!strcasecmp(args, "peerip")) {
		sccp_copy_string(buf, c->rtp.audio.phone_remote.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr) : "", buflen);
	} else if (!strcasecmp(args, "recvip")) {
		sccp_copy_string(buf, c->rtp.audio.phone.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone.sin_addr) : "", buflen);
	} else if (!strcasecmp(args, "from") && c->device) {
		sccp_copy_string(buf, (char *)c->device->id, buflen);
	} else {
		return -1;
	}
	return 0;
}

#ifndef ASTERISK_CONF_1_2

/*!
 * \brief Setting parameters for RTP callbacks (no RTP Glue Engine)
 */
#    ifndef CS_AST_HAS_RTP_ENGINE
struct ast_rtp_protocol sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk_get_rtp_peer,
	.set_rtp_peer = sccp_wrapper_asterisk_set_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk_get_vrtp_peer,
	.get_codec = sccp_device_get_codec,
};
#    else

/*!
 * \brief using RTP Glue Engine
 */
struct ast_rtp_glue sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk_get_vrtp_peer,
	.update_peer = sccp_wrapper_asterisk_set_rtp_peer,
	.get_codec = sccp_device_get_codec,
};
#    endif									// CS_AST_HAS_RTP_ENGINE
#endif										// ASTERISK_CONF_1_2
