/*!
 * \file 	sccp_pbx.c
 * \brief 	SCCP PBX Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$  
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include <asterisk/musiconhold.h>
#include "chan_sccp.h"
#include "sccp_pbx.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include "sccp_features.h"
#include "sccp_conference.h"

#include <assert.h>

#include <asterisk/pbx.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#ifdef CS_AST_HAS_AST_STRING_FIELD
#include <asterisk/stringfields.h>
#endif
#ifdef CS_MANAGER_EVENTS
#include <asterisk/manager.h>
#endif
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#endif




#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
#endif

/*!
 * \brief Call Auto Answer Thead
 *
 * The Auto Answer thread is started by ref sccp_pbx_call if necessary
 * \param data Data
 */
static void * sccp_pbx_call_autoanswer_thread(void *data)
{
	uint32_t *tmp = data;
	uint32_t callid = *tmp;
	sccp_channel_t 		*c;
	int instance = 0;

	sleep(GLOB(autoanswer_ring_time));
	pthread_testcancel();
	c = sccp_channel_find_byid(callid);
	if (!c || !c->device)
		return NULL;
	if (c->state != SCCP_CHANNELSTATE_RINGING)
		return NULL;
	sccp_channel_answer(c->device, c);
	if (GLOB(autoanswer_tone) != SKINNY_TONE_SILENCE && GLOB(autoanswer_tone) != SKINNY_TONE_NOTONE){
		sccp_device_lock(c->device);
		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_device_unlock(c->device);
		sccp_dev_starttone(c->device, GLOB(autoanswer_tone), instance, c->callid, 0);
	}
	if (c->autoanswer_type == SCCP_AUTOANSWER_1W)
		sccp_dev_set_microphone(c->device, SKINNY_STATIONMIC_OFF);

	return NULL;
}


/*!
 * \brief Incoming Calls by Asterisk SCCP_Request
 * \param ast Asterisk Channel as ast_channel
 * \param dest Destination as char
 * \param timeout Timeout after which incoming call is cancelled as int
 * \return Success as int
 */
static int sccp_pbx_call(struct ast_channel *ast, char *dest, int timeout) {
	sccp_line_t		* l;
	sccp_device_t  		*d=NULL;
	sccp_channel_t 		*c;
	const char 		*ringermode = NULL;
	pthread_attr_t 		attr;
	pthread_t t;
	char 			suffixedNumber[255] = { '\0' }; //!< For saving the digittimeoutchar to the logs */
	boolean_t		hasSession = FALSE;

	/* why let the phone ringing on a call forward ??? */
	if (!ast_strlen_zero(ast->call_forward)) {
		ast_queue_control(ast, AST_CONTROL_PROGRESS);
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Forwarding Call to '%s'\n", ast->call_forward);
		return 0;
	}

#ifndef CS_AST_CHANNEL_HAS_CID
	char * name, * number, *cidtmp; // For the callerid parse below
#endif

#ifdef ASTERISK_CONF_1_2
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif

	c = CS_AST_CHANNEL_PVT(ast);

	if (!c) {
		ast_log(LOG_WARNING, "SCCP: Asterisk request to call %s channel: %s, but we don't have this channel!\n", dest, ast->name);
		return -1;
	}

	l = c->line;
	if(l){
		sccp_linedevices_t *linedevice;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
			if(!linedevice->device)
				continue;

			if(linedevice->device->session)
				hasSession = TRUE;
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}
//	d = l->device;
//	s = d->session;

	if (!l || !hasSession) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line or device or session\n", (c ? c->callid : 0) );
		return -1;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Asterisk request to call %s\n", l->id, ast->name);

	//handle DND on lines
	sccp_mutex_lock(&l->lock);
	if (0) {
		if (l->dndmode == SCCP_DNDMODE_REJECT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT )) {
			ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
			if ( ringermode && !ast_strlen_zero(ringermode) ) {
				sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
				if (strcasecmp(ringermode, "urgent") == 0)
					c->ringermode = SKINNY_STATION_URGENTRING;
			} else {
				sccp_mutex_unlock(&l->lock);
				ast_setstate(ast, AST_STATE_BUSY);
				ast_queue_control(ast, AST_CONTROL_BUSY);
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND for line \"%s\" is on. Call %s rejected\n", d->id, l->name, ast->name);
				return 0;
			}

		}
		/* else if (l->dndmode == SCCP_DNDMODE_SILENT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_SILENT ) ) {
			// disable the ringer and autoanswer options
			c->ringermode = SKINNY_STATION_SILENTRING;
			c->autoanswer_type = SCCP_AUTOANSWER_NONE;
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND (silent) for line \"%s\" is on. Set the ringer = silent and autoanswer = off for %s\n", d->id, l->name, ast->name);
		}*/
	}
	sccp_mutex_unlock(&l->lock);
	//end DND handling

	/* if incoming call limit is reached send BUSY */
	sccp_mutex_lock(&l->lock);
	if ( l->channelCount > l->incominglimit ) { /* >= just to be sure :-) */
		sccp_log(1)(VERBOSE_PREFIX_3 "Incoming calls limit (%d) reached on SCCP/%s... sending busy\n", l->incominglimit, l->name);
		sccp_mutex_unlock(&l->lock);
		ast_setstate(ast, AST_STATE_BUSY);
		ast_queue_control(ast, AST_CONTROL_BUSY);
		return 0;
	}
	sccp_mutex_unlock(&l->lock);

	/* autoanswer check */
// 	if (c->autoanswer_type) {
// 		if (d->channelCount > 1) {
// 			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Autoanswer requested, but the device is in use\n", l->id);
// 			c->autoanswer_type = SCCP_AUTOANSWER_NONE;
// 			if (c->autoanswer_cause) {
// 				switch (c->autoanswer_cause) {
// 					case AST_CAUSE_CONGESTION:
// 						ast_queue_control(ast, AST_CONTROL_CONGESTION);
// 						break;
// 					default:
// 						ast_queue_control(ast, AST_CONTROL_BUSY);
// 						break;
// 				}
// 				sccp_mutex_unlock(&d->lock);
// 				return 0;
// 			}
// 		} else {
// 			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Autoanswer requested and activated %s\n", d->id, (c->autoanswer_type == SCCP_AUTOANSWER_1W) ? "with MIC OFF" : "with MIC ON");
// 		}
// 	}
// 	sccp_mutex_unlock(&d->lock);

  /* Set the channel callingParty Name and Number */
#ifdef CS_AST_CHANNEL_HAS_CID
	if(GLOB(recorddigittimeoutchar))
	{
		/* The hack to add the # at the end of the incoming number
		   is only applied for numbers beginning with a 0,
		   which is appropriate for Germany and other countries using similar numbering plan.
		   The option should be generalized, moved to the dialplan, or otherwise be replaced. */
		/* Also, we require an option whether to add the timeout suffix to certain
		   enbloc dialed numbers (such as via 7970 enbloc dialing) if they match a certain pattern.
		   This would help users dial from call history lists on other phones, which do not have enbloc dialing,
		   when using shared lines. */
                if(NULL != ast->cid.cid_num && strlen(ast->cid.cid_num) > 0 && strlen(ast->cid.cid_num) < sizeof(suffixedNumber)-2 && '0' == ast->cid.cid_num[0])
                {
                        strncpy(suffixedNumber, (const char*) ast->cid.cid_num, strlen(ast->cid.cid_num));
                        suffixedNumber[strlen(ast->cid.cid_num)+0] = '#';
                        suffixedNumber[strlen(ast->cid.cid_num)+1] = '\0';
                        sccp_channel_set_callingparty(c, ast->cid.cid_name, suffixedNumber);
                }
                else
                        sccp_channel_set_callingparty(c, ast->cid.cid_name, ast->cid.cid_num);
	}
	else
	{
		sccp_channel_set_callingparty(c, ast->cid.cid_name, ast->cid.cid_num);
	}
#else
	if (ast->callerid && (cidtmp = strdup(ast->callerid))) {
		ast_callerid_parse(cidtmp, &name, &number);
		sccp_channel_set_callingparty(c, name, number);
		if(cidtmp)
			ast_free(cidtmp);
		cidtmp = NULL;

		if(name)
			ast_free(name);
		name = NULL;

		if(number)
			ast_free(number);
		number = NULL;
	}
#endif
        /* Set the channel calledParty Name and Number 7910 compatibility*/
	sccp_channel_set_calledparty(c, l->cid_name, l->cid_num);

	if (!c->ringermode) {
		c->ringermode = SKINNY_STATION_OUTSIDERING;
		//ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
	}

	/*!\bug{seems it does not work }
	 */
	ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");

	if ( ringermode && !ast_strlen_zero(ringermode) ) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
		if (strcasecmp(ringermode, "inside") == 0)
			c->ringermode = SKINNY_STATION_INSIDERING;
		else if (strcasecmp(ringermode, "feature") == 0)
			c->ringermode = SKINNY_STATION_FEATURERING;
		else if (strcasecmp(ringermode, "silent") == 0)
			c->ringermode = SKINNY_STATION_SILENTRING;
		else if (strcasecmp(ringermode, "urgent") == 0)
			c->ringermode = SKINNY_STATION_URGENTRING;
	}

	/* release the asterisk channel lock */
	// sccp_ast_channel_unlock(ast);

//	if ( sccp_channel_get_active(d) ) {
//		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CALLWAITING);
//		ast_queue_control(ast, AST_CONTROL_RINGING);
//	} else {
//		sccp_indicate_lock(c, SCCP_CHANNELSTATE_RINGING);
//		ast_queue_control(ast, AST_CONTROL_RINGING);
//		if (c->autoanswer_type) {
//			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", d->id, ast->name);
//			pthread_attr_init(&attr);
//			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//			if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, &c->callid)) {
//				ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
//			}
//		}
//	}


	if(l->devices.size == 1 && SCCP_LIST_FIRST(&l->devices) && SCCP_LIST_FIRST(&l->devices)->device && SCCP_LIST_FIRST(&l->devices)->device->session){
		//TODO check if we have to do this
		c->device = SCCP_LIST_FIRST(&l->devices)->device;
		sccp_channel_updateChannelCapability(c);
	}

	boolean_t isRinging = FALSE;

	/*
	if(c->device){
		if(c->device->session){ // Call to a single device
			if ( sccp_channel_get_active(c->device) ) {
				sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_CALLWAITING);
				isRinging = TRUE;
			} else {
				if(c->device->dndFeature.enabled && c->device->dndFeature.status == SCCP_DNDMODE_REJECT ){
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: is on dnd\n", c->device->id);
				}else{
					sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_RINGING);
					isRinging = TRUE;
					if (c->autoanswer_type) {
						sccp_log(1)(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", c->device->id, ast->name);
						pthread_attr_init(&attr);
						pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
						if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, &c->callid)) {
							ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", c->device->id, l->name, c->callid, strerror(errno));
						}
					}
				}
			}
		}
	}else{ */
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
			if(!linedevice->device)
				continue;

			d=linedevice->device;

			/* do we have cfwd enabled? */
			if(linedevice->cfwdAll.enabled ){
				ast_log(LOG_NOTICE, "%s: initialize cfwd for line %s\n", d->id, l->name);
				int instance = sccp_device_find_index_for_line(d, l->name);
				sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_INTERCOMONEWAY, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_channel_send_callinfo(d, c);
				sccp_channel_forward(c, linedevice, linedevice->cfwdAll.number);
				isRinging = TRUE;
				continue;
			}



			if(!d->session)
				continue;
			
			/* check if c->subscriptionId.number is matching deviceSubscriptionID */
			/* This means that we call only those devices on a shared line
			   which match the specified subscription id in the dial parameters. */
			if( !sccp_util_matchSubscriptionId(c, linedevice->subscriptionId.number) ){
				continue;
			}

			if ( sccp_channel_get_active(d) ) {
				sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CALLWAITING);
				isRinging = TRUE;
			} else {
				if(d->dndFeature.enabled && d->dndFeature.status == SCCP_DNDMODE_REJECT)
					continue;

				sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_RINGING);
				isRinging = TRUE;
				if (c->autoanswer_type) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", d->id, ast->name);
					pthread_attr_init(&attr);
					pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
					if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, &c->callid)) {
						ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
					}
				}
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
/* 	} */

	/* someone forget to restore the asterisk lock */
	// sccp_ast_channel_lock(ast);
	if (isRinging){
		ast_queue_control(ast, AST_CONTROL_RINGING);
		sccp_channel_setSkinnyCallstate(c, SKINNY_CALLSTATE_RINGIN);
	}else{
		ast_queue_control(ast, AST_CONTROL_CONGESTION);
	}

	return 0;
}

/*!
 * \brief Handle Hangup Request by Asterisk
 * \param ast Asterisk Channel as ast_channel
 * \return Success as int
 */
static int sccp_pbx_hangup(struct ast_channel * ast) {
	sccp_channel_t * c;
	sccp_line_t    * l = NULL;
	sccp_device_t  * d = NULL;

	/* here the ast channel is locked */
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", ast->name);

	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)--;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

	c = CS_AST_CHANNEL_PVT(ast);

	while (c && sccp_channel_trylock(c)) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		ast_log(LOG_DEBUG, "SCCP: Waiting to lock the channel %s for hangup\n", ast->name);
		usleep(200);
		//c = CS_AST_CHANNEL_PVT(ast);
		// Strange why this assignment was included in the code in the first place (-DD)
	}

	if (!c) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel %s. SCCP channel already deleted\n", ast->name);
		sccp_pbx_needcheckringback(d);
		return 0;
	}

	//c->line->channelCount--;
#ifdef AST_FLAG_ANSWERED_ELSEWHERE
	if (ast_test_flag(ast, AST_FLAG_ANSWERED_ELSEWHERE) || ast->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere");
		c->answered_elsewhere = TRUE;
	}
#endif
	d = c->device;
	if(c->state != SCCP_CHANNELSTATE_DOWN) {
		if (GLOB(remotehangup_tone) && d && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active(d))
				sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
		sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_ONHOOK);
	}
	CS_AST_CHANNEL_PVT(ast) = NULL;
	c->owner = NULL;
	l = c->line;
//	d = l->device;
#ifdef CS_SCCP_CONFERENCE
	if(c->conference){
		sccp_conference_removeParticipant(c->conference, c);
	}
#endif

	if (c->rtp.audio) {
		sccp_channel_closereceivechannel(c);
		usleep(200);
		sccp_channel_destroy_rtp(c);
		usleep(200);
	}

	// removing scheduled dialing
	SCCP_SCHED_DEL(sched, c->digittimeout);

	sccp_line_lock(c->line);
	c->line->statistic.numberOfActiveChannels--;
	sccp_line_unlock(c->line);


	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Current channel %s-%08x state %s(%d)\n", (d)?DEV_ID_LOG(d):"(null)", l ? l->name : "(null)", c->callid, sccp_indicate2str(c->state), c->state);


	/* end callforwards */
	sccp_channel_t	*channel;
	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if(channel->parentChannel == c){
			 sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);
			 sccp_channel_endcall(channel);
		}
	}
	SCCP_LIST_UNLOCK(&c->line->channels);
	/* */

	if(!d){
		/* channel is not answerd, just ringin over all devices */
		//uint8_t instance;
		sccp_linedevices_t	*linedevice;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if(!linedevice->device)
				continue;
			d = linedevice->device;
			sccp_indicate_nolock(d, c, SKINNY_CALLSTATE_ONHOOK);
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}else{
		sccp_channel_send_callinfo(d, c);
		sccp_pbx_needcheckringback(d);
	}
	sccp_channel_cleanbeforedelete(c);
	sccp_channel_unlock(c);

	sccp_channel_delete_wo(c,0,0);

//	switch(c->state) {
//		case SCCP_CHANNELSTATE_DOWN:
//			break;
//		case SCCP_CHANNELSTATE_GETDIGITS:
//		case SCCP_CHANNELSTATE_OFFHOOK:
//		default:
//			/* we are in a passive hangup */
//			if (GLOB(remotehangup_tone) && d && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active(d))
//				sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
//			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_ONHOOK);
//			break;
//	}



	return 0;
}

/*!
 * \brief Thread to check Device Ring Back
 *
 * The Auto Answer thread is started by ref sccp_pbx_needcheckringback if necessary
 *
 * \param d SCCP Device
 */
void sccp_pbx_needcheckringback(sccp_device_t * d) {

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
 */
static int sccp_pbx_answer(struct ast_channel *ast)
{
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);

#ifdef ASTERISK_CONF_1_2
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif

	if (!c || !c->line || (!c->device && !c->parentChannel) ) {
		ast_log(LOG_ERROR, "SCCP: Answered %s but no SCCP channel\n", ast->name);
		return -1;
	}

	if( c->parentChannel){
		/* we are a forwarded call, bridge me with my parent */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_DEVICE))(VERBOSE_PREFIX_4 "SCCP: bridge me with my parent, device %s\n", DEV_ID_LOG(c->device));

		struct ast_channel	*astChannel = NULL, *br = NULL, *astForwardedChannel = c->parentChannel->owner;


		/*
		  on this point we do not have a pointer to ou bridge channel
		  so we search for it -MC
		*/
		const char *bridgePeer = pbx_builtin_getvar_helper(c->owner, "BRIDGEPEER");
		if(bridgePeer){
			while ((astChannel = ast_channel_walk_locked(astChannel)) != NULL) {
				sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_4 "(sccp_pbx_answer) searching for channel on %s\n", bridgePeer);
				sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_4 "(sccp_pbx_answer) asterisk channels %s\n", astChannel->name);
				if(strlen(astChannel->name) == strlen(bridgePeer) && !strncmp(astChannel->name, bridgePeer, strlen( astChannel->name ))){
					ast_channel_unlock(astChannel);
					br = astChannel;
					break;
				}
				ast_channel_unlock(astChannel);
			}
		}

		ast_log(LOG_ERROR, "SCCP: bridge: %s\n", (br)?br->name:" -- no bridget -- ");
		/* did we found our bridge */
		if(br){
			c->parentChannel = NULL;
			ast_channel_masquerade(astForwardedChannel, br); /* bridge me */
			return 0;
		}else{
			/* we have no bridge and can not make a masquerade -> end call */
			ast_log(LOG_ERROR, "SCCP: We did not found bridge channel for call forwarding call. Hangup\n");
			sccp_channel_endcall(c);
		}
		return -1;
	}


	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Outgoing call has been answered %s on %s@%s-%08x\n", ast->name, c->line->name, DEV_ID_LOG(c->device), c->callid);
	//ast->nativeformats = c->device ? c->device->capability : GLOB(global_capability);
	sccp_channel_updateChannelCapability(c);
	/* This seems like brute force, and doesn't seem to be of much use. However, I want it to be remebered
	   as I have forgotten what my actual motivation was for writing this strange code. (-DD) */

	sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_DIALING);
	sccp_channel_send_callinfo(c->device,c);
	sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_PROCEED);
	sccp_channel_send_callinfo(c->device,c);

	sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_CONNECTED);
	return 0;
}

/*!
 * \brief Read from an Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 */
static struct ast_frame * sccp_pbx_read(struct ast_channel *ast)
{
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	struct ast_frame *frame = NULL;

#ifndef ASTERISK_CONF_1_2
	frame = &ast_null_frame;
#else
	frame = NULL;
#endif

	if(!c || !c->rtp.audio)
		return frame;

	switch(ast->fdno) {
#ifndef CS_AST_HAS_RTP_ENGINE
		case 0:
			frame = ast_rtp_read(c->rtp.audio);	/* RTP Audio */
#ifdef 	CS_SCCP_CONFERENCE
			if(c->conference){
				//sccp_conference_readFrame(frame, c);
			}
#endif
			break;
		case 1:
			frame = ast_rtcp_read(c->rtp.audio);	/* RTCP Control Channel */
			break;
		case 2:
			frame = ast_rtp_read(c->rtp.video);	/* RTP Video */
			//ast_log(LOG_NOTICE, "%s: Got Video frame from device\n", DEV_ID_LOG(c->device));
			break;
		case 3:
			frame = ast_rtcp_read(c->rtp.video);	/* RTCP Control Channel for video */
			break;
#else
		case 0:
			frame = ast_rtp_instance_read(c->rtp.audio, 0); /* RTP Audio */
			break;
		case 1:
			frame = ast_rtp_instance_read(c->rtp.audio, 1); /* RTCP Control Channel */
			break;
		case 2:
			frame = ast_rtp_instance_read(c->rtp.video, 0); /* RTP Video */
			break;
		case 3:
			frame = ast_rtp_instance_read(c->rtp.video, 1); /* RTCP Control Channel for video */
			break;
#endif


		default:
			return frame;
	}

	if (frame->frametype == AST_FRAME_VOICE) {
		//sccp_log(1)(VERBOSE_PREFIX_3 "%s: read format: %s(%d)\n",
		//				DEV_ID_LOG(c->device),
		//				ast_getformatname(frame->subclass),
		//				frame->subclass);
#ifndef ASTERISK_CONF_1_2
		if (!(frame->subclass & (ast->nativeformats & AST_FORMAT_AUDIO_MASK)))
#else
		if (!(frame->subclass & (ast->nativeformats)))
#endif
		{
		      sccp_log(1)(VERBOSE_PREFIX_3 "%s: Channel %s changed format from %s(%d) to %s(%d)\n",
						DEV_ID_LOG(c->device),
						ast->name,
						ast_getformatname(ast->nativeformats),
						ast->nativeformats,
						ast_getformatname(frame->subclass),
						frame->subclass);

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
 */
static int sccp_pbx_write(struct ast_channel *ast, struct ast_frame *frame) {
	int res = 0;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);

	if(c) {
		switch (frame->frametype) {
			case AST_FRAME_VOICE:
				// checking for samples to transmit
				if (!frame->samples) {
					if(strcasecmp(frame->src, "ast_prod")) {
						ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", DEV_ID_LOG(c->device), frame->subclass);
						return -1;
					} else {
						// frame->samples == 0  when frame_src is ast_prod
						sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", DEV_ID_LOG(c->device), ast->name);
					}
				} else if (!(frame->subclass & ast->nativeformats)) {
					char s1[512], s2[512], s3[512];
					ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%d) read/write = %s(%d)/%s(%d)\n",
						DEV_ID_LOG(c->device),
						frame->subclass,
#ifndef ASTERISK_CONF_1_2
						ast_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats & AST_FORMAT_AUDIO_MASK),
						ast->nativeformats & AST_FORMAT_AUDIO_MASK,
#else
						ast_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats),
						ast->nativeformats,
#endif
						ast_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat),
						ast->readformat,
						ast_getformatname_multiple(s3, sizeof(s3) - 1, ast->writeformat),
						ast->writeformat);
					//return -1;

				}
				if (c->rtp.audio){
					res = sccp_rtp_read(c->rtp.audio, frame);
				}
				break;
			case AST_FRAME_IMAGE:
			case AST_FRAME_VIDEO:
				//ast_log(LOG_NOTICE, "%s: Got Video frame type %d\n", DEV_ID_LOG(c->device), frame->subclass);
				if (c && c->rtp.video){
					res = sccp_rtp_read(c->rtp.video, frame);
				}else{
					//ast_log(LOG_NOTICE, "%s: drop video frame\n", DEV_ID_LOG(c->device));
				}
				break;

			case AST_FRAME_TEXT:
#ifndef ASTERISK_CONF_1_2
			case AST_FRAME_MODEM:
#endif
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
static char *sccp_control2str(int state) {
		switch(state) {
		case AST_CONTROL_HANGUP:
				return "Hangup";
		case AST_CONTROL_RING:
				return "Ring";
		case AST_CONTROL_RINGING:
				return "Ringing";
		case AST_CONTROL_ANSWER:
				return "Answer";
		case AST_CONTROL_BUSY:
				return "Busy";
		case AST_CONTROL_TAKEOFFHOOK:
				return "TakeOffHook";
		case AST_CONTROL_OFFHOOK:
				return "OffHook";
		case AST_CONTROL_CONGESTION:
				return "Congestion";
		case AST_CONTROL_FLASH:
				return "Flash";
		case AST_CONTROL_WINK:
				return "Wink";
		case AST_CONTROL_OPTION:
				return "Option";
		case AST_CONTROL_RADIO_KEY:
				return "RadioKey";
		case AST_CONTROL_RADIO_UNKEY:
				return "RadioUnKey";
		case AST_CONTROL_PROGRESS:
				return "Progress";
		case AST_CONTROL_PROCEEDING:
				return "Proceeding";
#ifdef CS_AST_CONTROL_HOLD
		case AST_CONTROL_HOLD:
				return "Hold";
		case AST_CONTROL_UNHOLD:
				return "UnHold";
#endif
#ifdef CS_AST_CONTROL_VIDUPDATE
		case AST_CONTROL_VIDUPDATE:
				return "VideoFrameUpdate";
#endif
#ifdef AST_CONTROL_T38
		case AST_CONTROL_T38:
				return "T38RequestNotification";
#endif
#ifdef AST_CONTROL_T38_PARAMETERS
		case AST_CONTROL_T38_PARAMETERS:
				return "T38RequestNotification";
#endif
#ifdef CS_AST_CONTROL_SRCUPDATE
		case AST_CONTROL_SRCUPDATE:
				return "MediaSourceUpdate";
#endif

		case -1:
				return "ChannelProdding";
		default:
				return "Unknown";
		}
}

#ifdef ASTERISK_CONF_1_2
/*!
 * \brief Indicate to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param ind AST_CONTROL_* State as int (Indication)
 * \return Result as int
 */
static int sccp_pbx_indicate(struct ast_channel *ast, int ind) {
#else
/*!
 * \brief Indicate to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param ind AST_CONTROL_* State as int (Indication)
 * \param data Data
 * \param datalen Data Length as size_t
 * \return Result as int
 */
static int sccp_pbx_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	int res = 0;
	if (!c)
		return -1;

	if (!c->device)
			return -1;

	//sccp_channel_lock(c);
	//avoid deadlock @see https://sourceforge.net/tracker/?func=detail&aid=2979751&group_id=186378&atid=917045 -MC
	int counter = 0;
	while (c && sccp_channel_trylock(c)) {
		counter++;
		if(counter > 200){
			ast_log(LOG_ERROR, "Unable to lock channel in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
			return -1;
		}
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		usleep(10);
		
	}
	
	
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' (%s) condition on channel %s\n", DEV_ID_LOG(c->device), ind, sccp_control2str(ind), ast->name);

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	if (c->rtp.audio)
		res = -1;

	switch(ind) {

	case AST_CONTROL_RINGING:
		if(SKINNY_CALLTYPE_OUTBOUND == c->calltype)
		{
			// Allow signalling of RINGOUT only on outbound calls.
			// Otherwise, there are some issues with late arriving ringing
			// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
			sccp_indicate_nolock(c->device, c, SCCP_CHANNELSTATE_RINGOUT);
		}
		break;
	case AST_CONTROL_BUSY:
		sccp_indicate_nolock(c->device, c, SCCP_CHANNELSTATE_BUSY);
		break;
	case AST_CONTROL_CONGESTION:
		sccp_indicate_nolock(c->device, c, SCCP_CHANNELSTATE_CONGESTION);
		break;
	case AST_CONTROL_PROGRESS:
	case AST_CONTROL_PROCEEDING:
		sccp_indicate_nolock(c->device, c, SCCP_CHANNELSTATE_PROCEED);
		res = 0;
		break;
#ifdef CS_AST_CONTROL_SRCUPDATE
        case AST_CONTROL_SRCUPDATE:
        /* Source media has changed. */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");



                /* update channel format */
                int oldChannelFormat = c->format;
                c->format = ast->rawreadformat;

                if(oldChannelFormat != c->format ){
                        /* notify of changing format */
                        char s1[512], s2[512];
                        ast_log(LOG_NOTICE, "SCCP: SCCP/%s-%08x, changing format from: %s(%d) to: %s(%d) \n",
                                c->line->name,
                                c->callid,
#ifndef ASTERISK_CONF_1_2
                                ast_getformatname_multiple(s1, sizeof(s1) -1, c->format & AST_FORMAT_AUDIO_MASK),
#else
                                ast_getformatname_multiple(s1, sizeof(s1) -1, c->format),
#endif
                                ast->nativeformats,
#ifndef ASTERISK_CONF_1_2
                                ast_getformatname_multiple(s2, sizeof(s2) -1, ast->rawreadformat & AST_FORMAT_AUDIO_MASK),
#else
                                ast_getformatname_multiple(s2, sizeof(s2) -1, ast->rawreadformat),
#endif
                                ast->rawreadformat);
                        ast_set_read_format(ast, c->format);
                        ast_set_write_format(ast, c->format);
                }

                ast_log(LOG_NOTICE, "SCCP: SCCP/%s-%08x, state: %s(%d) \n",c->line->name, c->callid, sccp_indicate2str(c->state), c->state);
                if(c->rtp.audio){
                        if(oldChannelFormat != c->format){
                                if(c->mediaStatus.receive == TRUE || c->mediaStatus.transmit == TRUE){
                                        sccp_channel_closereceivechannel(c);	/* close the already openend receivechannel */
                                        sccp_channel_openreceivechannel(c);	/* reopen it */
                                }
                        }
                }
#ifdef CS_AST_RTP_NEW_SOURCE
                if(c->rtp.audio) {
                        ast_rtp_new_source(c->rtp.audio);
                        sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: Source UPDATE ok\n");
                }
#endif
                break;
#endif

#ifdef CS_AST_CONTROL_HOLD
        /* when the bridged channel hold/unhold the call we are notified here */
	case AST_CONTROL_HOLD:
#ifdef ASTERISK_CONF_1_2
		ast_moh_start(ast, c->musicclass);
#else
		ast_moh_start(ast, data, c->musicclass);
#endif
		res = 0;
		break;
	case AST_CONTROL_UNHOLD:
		ast_moh_stop(ast);
		res = 0;
		break;
#endif
	case -1: // Asterisk prod the channel
		res = -1;
		break;
#ifdef CS_AST_CONTROL_CONNECTED_LINE
	case AST_CONTROL_CONNECTED_LINE:
		sccp_pbx_update_connectedline(ast, data, datalen);
		sccp_indicate_nolock(c->device, c, c->state);
	break;
#endif
	case AST_CONTROL_VIDUPDATE:	/* Request a video frame update */
		if (c->rtp.video) {
			res = 0;
		} else
			res = -1;
	break;
#ifdef 	AST_CONTROL_SRCCHANGE
	case AST_CONTROL_SRCCHANGE:
		//TODO handle src update
		res = -1;
	break;
#endif
	default:
		ast_log(LOG_WARNING, "SCCP: Don't know how to indicate condition %d\n", ind);
		res = -1;
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
static void sccp_pbx_update_connectedline(sccp_channel_t *channel, const void *data, size_t datalen){
	struct ast_channel *ast_channel = channel->owner;
	if(!ast_channel)
		return;

	if (ast_strlen_zero(ast_channel->connected.id.name) || ast_strlen_zero(ast_channel->connected.id.number))
		return;

	sccp_channel_set_calledparty(c, ast_channel->connected.id.name, ast_channel->connected.id.number);
}
#endif


/*!
 * \brief Asterisk Fix Up
 * \param oldchan Asterisk Channel as ast_channel
 * \param newchan Asterisk Channel as ast_channel
 * \return Success as int
 */
static int sccp_pbx_fixup(struct ast_channel *oldchan, struct ast_channel *newchan) {
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: we gote a fixup request for %s\n", newchan->name);

	sccp_channel_t * c = CS_AST_CHANNEL_PVT(newchan);

	if (!c) {
		ast_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, oldchan, newchan->name, newchan);
		return -1;
	}
	sccp_mutex_lock(&c->lock);
	if (c->owner != oldchan) {
		ast_log(LOG_WARNING, "SCCP: old channel wasn't %p but was %p\n", oldchan, c->owner);
		sccp_mutex_unlock(&c->lock);
		return -1;
	}

	c->owner = newchan;
	sccp_mutex_unlock(&c->lock);
	return 0;
}

#ifndef ASTERISK_CONF_1_2
/*!
 * \brief Receive First Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit First Digit as char
 * \return Always Return -1 as int
 */
static int sccp_pbx_recvdigit_begin(struct ast_channel *ast, char digit) {
	return -1;
}
#endif

#ifdef ASTERISK_CONF_1_2
/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \return Always Return -1 as int
 * \todo FIXME Always returns -1
 */
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit) {
#else
/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \param duration Duration as int
 * \return Always Return -1 as int
 * \todo FIXME Always returns -1
 */
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit, unsigned int duration) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	sccp_device_t * d = NULL;
	uint8_t			instance;

	return -1;

	if (!c || !c->device)
		return -1;

	d = c->device;

	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, (d->dtmfmode) ? "outofband" : "inband");

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		return -1;
	}

	if (d->dtmfmode == SCCP_DTMFMODE_INBAND)
		return -1 ;

	if (digit == '*') {
		digit = 0xe; /* See the definition of tone_list in chan_protocol.h for more info */
	} else if (digit == '#') {
		digit = 0xf;
	} else if (digit == '0') {
		digit = 0xa; /* 0 is not 0 for cisco :-) */
	} else {
		digit -= '0';
	}

	if (digit > 16) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Cisco phones can't play this type of dtmf. Sinding it inband\n", d->id);
		return -1;
	}

	instance = sccp_device_find_index_for_line(c->device, c->line->name);
	sccp_dev_starttone(c->device, (uint8_t) digit, instance, c->callid, 0);
	return 0;
}

#ifdef CS_AST_HAS_TECH_PVT
/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 */
static int sccp_pbx_sendtext(struct ast_channel *ast, const char *text) {
#else
/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 */
static int sccp_pbx_sendtext(struct ast_channel *ast, char *text) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	sccp_device_t * d;
	uint8_t			instance;

	if (!c || !c->device)
		return -1;

	d = c->device;
	if (!text || ast_strlen_zero(text))
		return 0;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_displayprompt(d, instance, c->callid, (char *)text, 10);
	return 0;
}

#ifdef CS_AST_HAS_TECH_PVT
/*!
 * \brief SCCP Tech Structure
 */
const struct ast_channel_tech sccp_tech = {
	.type = "SCCP",
	.description = "Skinny Client Control Protocol (SCCP)",
	.capabilities = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A |AST_FORMAT_H263|AST_FORMAT_H264|AST_FORMAT_H263_PLUS,
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
#ifndef ASTERISK_CONF_1_2
	.send_digit_begin = sccp_pbx_recvdigit_begin,
	.send_digit_end = sccp_pbx_recvdigit_end,
#else
	.send_digit = sccp_pbx_recvdigit_end,
#endif
	.send_text = sccp_pbx_sendtext,
#ifndef ASTERISK_CONF_1_2
	.bridge = ast_rtp_bridge,
#endif
#ifdef ASTERISK_CONF_1_6
	.early_bridge = ast_rtp_early_bridge,
#endif
};
#endif

/*!
 * \brief Allocate an Asterisk Channel
 * \param c SCCP Channel
 * \return 1 on Success as uint8_t
 */
uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c) {
//	sccp_device_t 			*d = c->device;
	struct ast_channel 		*tmp;
	sccp_line_t 			*l = c->line;
	int fmt;


	#ifndef CS_AST_CHANNEL_HAS_CID
	char cidtmp[256];
	memset(&cidtmp, 0, sizeof(cidtmp));
#endif

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP: try to allocate channel \n");
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP: Line: %s\n", l->name);

	if (!l) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Unable to allocate asterisk channel %s\n", l->name);
		ast_log(LOG_ERROR, "SCCP: Unable to allocate asterisk channel\n");
		return 0;
	}

//	sccp_mutex_unlock(&c->lock);
//	/* Don't hold a sccp pvt lock while we allocate a channel */
	if(c->device){
		sccp_linedevices_t 	*linedevice;
			
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
			  if(linedevice->device == c->device)
				break;
		}
		SCCP_LIST_UNLOCK(&l->devices);
		
		/* append subscriptionId to cid */
		if(linedevice && !ast_strlen_zero(linedevice->subscriptionId.number)){
			sprintf(c->callingPartyNumber, "%s%s", l->cid_num, linedevice->subscriptionId.number);
		}else{
			sprintf(c->callingPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number)?l->defaultSubscriptionId.number:"");
		}
		
		if(linedevice && !ast_strlen_zero(linedevice->subscriptionId.name)){
			sprintf(c->callingPartyName, "%s%s", l->cid_name, linedevice->subscriptionId.name);
		}else{
			sprintf(c->callingPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name)?l->defaultSubscriptionId.name:"");
		}
	  
	}else{
		sprintf(c->callingPartyNumber, "%s%s", l->cid_num, (l->defaultSubscriptionId.number)?l->defaultSubscriptionId.number:"");
		sprintf(c->callingPartyName, "%s%s", l->cid_name, (l->defaultSubscriptionId.name)?l->defaultSubscriptionId.name:"");
	}




#ifdef ASTERISK_CONF_1_2
	tmp = ast_channel_alloc(1);
#else
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:     cid_num: \"%s\"\n", c->callingPartyNumber);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:    cid_name: \"%s\"\n", c->callingPartyName);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP: accountcode: \"%s\"\n", l->accountcode);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:       exten: \"%s\"\n", c->dialedNumber);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:     context: \"%s\"\n", l->context);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:    amaflags: \"%d\"\n", l->amaflags);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "SCCP:   chan/call: \"%s-%08x\"\n", l->name, c->callid);

	
	
	
	/* This should definetly fix CDR */
    	tmp = ast_channel_alloc(1, AST_STATE_DOWN, c->callingPartyNumber, c->callingPartyName, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08x", l->name, c->callid);
#endif
       // tmp = ast_channel_alloc(1); function changed in 1.4.0
       // Note: Assuming AST_STATE_DOWN is starting state
	if (!tmp) {
		ast_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", l->id, l->name);
		return 0;
	}

	/* need to reset the exten, otherwise it would be set to s */
	memset(&tmp->exten,0,sizeof(tmp->exten));

	/* let's connect the ast channel to the sccp channel */
	sccp_channel_lock(c);
	c->owner = tmp;
//	sccp_channel_unlock(c);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Global Capabilities: %d\n", l->id, GLOB(global_capability));

//	sccp_line_lock(l);
//	sccp_device_lock(d);

	//TODO check locking

	while(sccp_line_trylock(l)) {
		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		usleep(1);
	}
//
//	while(sccp_device_trylock(d)) {
//		sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
//		sccp_line_unlock(l);
//		usleep(1);
//		sccp_line_lock(l);
//	}

	// tmp->nativeformats = (d->capability ? d->capability : GLOB(global_capability));
	//tmp->nativeformats = ast_codec_choose(&d->codecs, (d->capability ? d->capability : GLOB(global_capability)), 1);
	//tmp->nativeformats = (l->capability ? l->capability : GLOB(global_capability));
	//tmp->nativeformats = ast_codec_choose(&c->codecs, c->capability, 1);




	//fmt = ast_codec_choose(&d->codecs, tmp->nativeformats, 1);
	//fmt = tmp->nativeformats;
	//c->format = fmt;
	//c->format = ast_codec_choose(&c->codecs, tmp->nativeformats, 1);
	sccp_channel_updateChannelCapability(c);
	if(!tmp->nativeformats)	{
		ast_log(LOG_ERROR, "%s: No audio format to offer. Cancelling call on line %s\n", l->id, l->name);
		return 0;
	}
	fmt = tmp->readformat;

#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(tmp, name, "SCCP/%s-%08x", l->name, c->callid);
#else
	snprintf(tmp->name, sizeof(tmp->name), "SCCP/%s-%08x", l->name, c->callid);
#endif

	sccp_line_unlock(l);
	sccp_channel_unlock(c);
//	sccp_device_unlock(d);


#ifndef ASTERISK_CONF_1_2
	ast_jb_configure(tmp, &GLOB(global_jbconf));
#endif

	char s1[512], s2[512];
	sccp_log(2)(VERBOSE_PREFIX_3 "%s: Channel %s, capabilities: CHANNEL %s(%d) PREFERRED %s(%d) USED %s(%d)\n",
	l->id,
	tmp->name,
#ifndef ASTERISK_CONF_1_2
	ast_getformatname_multiple(s1, sizeof(s1) -1, c->capability & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s1, sizeof(s1) -1, c->capability),
#endif
	c->capability,
#ifndef ASTERISK_CONF_1_2
	ast_getformatname_multiple(s2, sizeof(s2) -1, tmp->nativeformats & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s2, sizeof(s2) -1, tmp->nativeformats),
#endif
	tmp->nativeformats,
	ast_getformatname(fmt),
	fmt);

	//tmp->writeformat		= fmt;
	//tmp->readformat 		= fmt;

#ifdef CS_AST_HAS_TECH_PVT
	tmp->tech = &sccp_tech;
	tmp->tech_pvt = c;
	//tmp->rawreadformat = fmt;
	//tmp->rawwriteformat = fmt;
#else
	tmp->type = "SCCP";
	tmp->pvt->pvt = c;
	//tmp->pvt->rawwriteformat = fmt;
	//tmp->pvt->rawreadformat = fmt;

	tmp->pvt->answer = sccp_pbx_answer;
	tmp->pvt->hangup = sccp_pbx_hangup;
	tmp->pvt->call = sccp_pbx_call;

	tmp->pvt->read = sccp_pbx_read;
	tmp->pvt->write	= sccp_pbx_write;
/*	tmp->pvt->devicestate    	= sccp_devicestate; */
	tmp->pvt->indicate = sccp_pbx_indicate;
	tmp->pvt->fixup	= sccp_pbx_fixup;
	tmp->pvt->send_digit = sccp_pbx_recvdigit;
	tmp->pvt->send_text = sccp_pbx_sendtext;
#endif

	tmp->adsicpe = AST_ADSI_UNAVAILABLE;

	// XXX: Bridge?
	// XXX: Transfer?

	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)++;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

#ifdef CS_AST_CHANNEL_HAS_CID
	if (l->cid_num){
		tmp->cid.cid_num = strdup(c->callingPartyNumber);	  
	}
	if (l->cid_name)
		tmp->cid.cid_name = strdup(c->callingPartyName);
#else
	snprintf(cidtmp, sizeof(cidtmp), "\"%s\" <%s>", c->callingPartyName, c->callingPartyNumber);
	tmp->callerid = strdup(cidtmp);
#endif

	sccp_copy_string(tmp->context, l->context, sizeof(tmp->context));
	if (!ast_strlen_zero(l->language))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, language, l->language);
#else
		sccp_copy_string(tmp->language, l->language, sizeof(tmp->language));
#endif
	if (!ast_strlen_zero(l->accountcode))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, accountcode, l->accountcode);
#else
		sccp_copy_string(tmp->accountcode, l->accountcode, sizeof(tmp->accountcode));
#endif
	if (!ast_strlen_zero(l->musicclass))
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(tmp, musicclass, c->musicclass);
#else
		sccp_copy_string(tmp->musicclass, l->musicclass, sizeof(tmp->musicclass));
#endif
	tmp->amaflags = l->amaflags;
#ifdef CS_SCCP_PICKUP
	tmp->callgroup = l->callgroup;
	tmp->pickupgroup = l->pickupgroup;
#endif
	tmp->priority = 1;

	// export sccp informations in asterisk dialplan
	if(c->device){
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_MAC" , c->device->id);
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_IP"  , ast_inet_ntoa(c->device->session->sin.sin_addr));
		pbx_builtin_setvar_helper(tmp, "SCCP_DEVICE_TYPE", devicetype2str(c->device->skinny_type));
	}
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s-%08x\n", (l)?l->id:"(null)", (l)?l->name:"(null)", (c)?c->callid:-1);

	return 1;
}

/*!
 * \brief Schedule Asterisk Dial
 * \param data Data as constant
 * \return Success as int
 */
int sccp_pbx_sched_dial(const void *data)
{
	sccp_channel_t *c = (sccp_channel_t *)data;

	if(c && c->owner && !c->owner->pbx) {
		sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: Timeout for call '%d'. Going to dial '%s'\n", c->callid, c->dialedNumber);
		sccp_pbx_softswitch(c);
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
	struct ast_channel * chan = c->owner;
	sccp_line_t * l = c->line;
	sccp_device_t * d = c->device;

	if(!l || !d || !chan)
		return 0;

	if (!ast_strlen_zero(c->dialedNumber)) {
		if(GLOB(recorddigittimeoutchar) && GLOB(digittimeoutchar) == c->dialedNumber[strlen(c->dialedNumber) - 1]) {
			return 1;
		}
	}

	int ignore_pat = ast_ignore_pattern(chan->context, c->dialedNumber);
	int ext_exist = ast_exists_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);
	int ext_canmatch = ast_canmatch_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);
	int ext_matchmore = ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num);
	sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: extension helper says that:\n"
								  "ignore pattern  : %d\n"
								  "exten_exists    : %d\n"
								  "exten_canmatch  : %d\n"
								  "exten_matchmore : %d\n",
								  ignore_pat,
								  ext_exist,
								  ext_canmatch,
								  ext_matchmore);
	if( (c->ss_action != SCCP_SS_GETCBARGEROOM) && (c->ss_action != SCCP_SS_GETMEETMEROOM) &&
	    (!ignore_pat) && (ext_exist)
		&& ((d->overlapFeature.enabled && !ext_canmatch) || (!d->overlapFeature.enabled && !ext_matchmore))
		&& ((d->overlapFeature.enabled && !ext_canmatch) || (!d->overlapFeature.enabled && !ext_matchmore))
		) {
		return 1;
	}

	return 0;
}

/*!
 * \brief Handle Soft Switch
 * \param c SCCP Channel as sccp_channel_t
 * \todo clarify Soft Switch Function
 */
void * sccp_pbx_softswitch(sccp_channel_t * c) {
	if (!c) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <channel> available. Returning from dial thread.\n");
		return NULL;
	}

	struct ast_channel * chan = c->owner;
	struct ast_variable *v = NULL;
	uint8_t	instance;
	int len = 0;

	sccp_line_t * l;
	sccp_device_t * d;

	char shortenedNumber[256] = { '\0' }; /* For recording the digittimeoutchar */

	/* removing scheduled dialing */
	SCCP_SCHED_DEL(sched, c->digittimeout);

	/* channel is here, so locking it */
	sccp_channel_lock(c);

	/* we should just process outbound calls, let's check calltype */
	if (c->calltype != SKINNY_CALLTYPE_OUTBOUND) {
		sccp_channel_unlock(c);
		return NULL;
	}

	/* assume d is the channel's device */
	d = c->device;

	/* does it exists ? */
	if (!d) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> available. Returning from dial thread.\n");
		sccp_channel_unlock(c);
		return NULL;
	}

	/* we don't need to check for a device type but just if the device has an id, otherwise back home  -FS */
	if (!d->id || ast_strlen_zero(d->id)) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <device> identifier available. Returning from dial thread.\n");
		sccp_channel_unlock(c);
		return NULL;
	}

	l = c->line;
	if (!l) {
		ast_log(LOG_ERROR, "SCCP: (sccp_pbx_softswitch) No <line> available. Returning from dial thread.\n");
		sccp_channel_unlock(c);
		if (chan)
			ast_hangup(chan);
		return NULL;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) New call on line %s\n", DEV_ID_LOG(d), l->name);

	/* assign callerid name and number */
	sccp_channel_set_callingparty(c, l->cid_name, l->cid_num);


	// we use shortenedNumber but why ???
	// If the timeout digit has been used to terminate the number
	// and this digit shall be included in the phone call history etc (recorddigittimeoutchar is true)
	// we still need to dial the number without the timeout char in the pbx
	// so that we don't dial strange extensions with a trailing characters.

	sccp_copy_string(shortenedNumber, c->dialedNumber, sizeof(shortenedNumber));
	len = strlen(shortenedNumber);
	assert(strlen(c->dialedNumber) == len);

	if(GLOB(digittimeoutchar) == shortenedNumber[len-1])
	{
		shortenedNumber[len-1] = '\0';

		// If we don't record the timeoutchar in the logs, we remove it from the sccp channel structure
		// Later, the channel dialed number is used for directories, etc.,
		// and the shortened number is used for dialing the actual call via asterisk pbx.

		if(!GLOB(recorddigittimeoutchar)) {
			c->dialedNumber[len-1] = '\0';
		}
	}


	/* This will choose what to do */
	switch(c->ss_action) {
		case SCCP_SS_GETFORWARDEXTEN:
//				sccp_channel_unlock(c);
			if(!ast_strlen_zero(shortenedNumber)) {
				sccp_line_cfwd(l, d, c->ss_data, shortenedNumber);
			}
			sccp_channel_unlock(c);
			sccp_channel_endcall(c);
			return NULL; // leave simple switch without dial
#ifdef CS_SCCP_PICKUP
		case SCCP_SS_GETPICKUPEXTEN:
//				sccp_channel_unlock(c);
			// like we're dialing but we're not :)
			instance = sccp_device_find_index_for_line(d, c->line->name);

			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_DIALING);
			sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT); 
			sccp_channel_send_callinfo(d, c);
			sccp_dev_clearprompt(d, instance, c->callid);
			sccp_dev_displayprompt(d,instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);

			if(!ast_strlen_zero(shortenedNumber)) {
				sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk request to pickup exten '%s'\n", shortenedNumber);
				sccp_channel_unlock(c);
				if(sccp_feat_directpickup(c, shortenedNumber)) {
					sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
			}
			else {
				// without a number we can also close the call. Isn't it true ?
				sccp_channel_unlock(c);
				sccp_channel_endcall(c);
			}
			return NULL; // leave simpleswitch without dial
#endif
		case SCCP_SS_GETMEETMEROOM:
//				sccp_channel_unlock(c);
			if(!ast_strlen_zero(shortenedNumber) && !ast_strlen_zero(c->line->meetmenum)) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Meetme request for room '%s' on extension '%s'\n", d->id, shortenedNumber, c->line->meetmenum);
				if(c->owner && !ast_check_hangup(c->owner))
					pbx_builtin_setvar_helper(c->owner, "SCCP_MEETME_ROOM", shortenedNumber);
				sccp_copy_string(shortenedNumber, c->line->meetmenum, sizeof(shortenedNumber));
				sccp_copy_string(c->dialedNumber, SKINNY_DISP_CONFERENCE, sizeof(c->dialedNumber));
			} else {
				// without a number we can also close the call. Isn't it true ?
				sccp_channel_unlock(c);
				sccp_channel_endcall(c);
				return NULL;
			}
			break;
		case SCCP_SS_GETBARGEEXTEN:
//				sccp_channel_unlock(c);
			// like we're dialing but we're not :)
			instance = sccp_device_find_index_for_line(d, c->line->name);
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_DIALING);
			sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT); 
			sccp_channel_send_callinfo(d, c);

			sccp_dev_clearprompt(d, instance, c->callid);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
			if(!ast_strlen_zero(shortenedNumber)) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge exten '%s'\n", d->id, shortenedNumber);
				sccp_channel_unlock(c);
				if(sccp_feat_barge(c, shortenedNumber)) {
					sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
			} else {
				// without a number we can also close the call. Isn't it true ?
				sccp_channel_unlock(c);
				sccp_channel_endcall(c);
			}
			return NULL; // leave simpleswitch without dial
		case SCCP_SS_GETCBARGEROOM:
			// like we're dialing but we're not :)
			instance = sccp_device_find_index_for_line(d, c->line->name);

			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_DIALING);
			sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT); 
			sccp_channel_send_callinfo(d, c);
			sccp_dev_clearprompt(d, instance, c->callid);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
			if(!ast_strlen_zero(shortenedNumber)) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (sccp_pbx_softswitch) Device request to barge conference '%s'\n", d->id, shortenedNumber);
				sccp_channel_unlock(c);
				if(sccp_feat_cbarge(c, shortenedNumber)) {
					sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
			} else {
				// without a number we can also close the call. Isn't it true ?
				sccp_channel_unlock(c);
				sccp_channel_endcall(c);
			}
			return NULL; // leave simpleswitch without dial
		case SCCP_SS_DIAL:
		default:
		break;
	}



	/* set private variable */
	if (chan && !ast_check_hangup(chan)) {
		sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", c->privacy ? "1" : "0");

		uint32_t result = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;
		result |= c->privacy;
		if(d->privacyFeature.enabled && result ){
			sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", "1");
			pbx_builtin_setvar_helper(chan, "SKINNY_PRIVATE", "1");
		}else{
			sccp_log((DEBUGCAT_PBX))(VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", "0");
			pbx_builtin_setvar_helper(chan, "SKINNY_PRIVATE", "0");
		}
	}

	/* set devicevariables */
	v = ((d) ? d->variables : NULL);
	while (chan && !ast_check_hangup(chan) && d && v) {
		pbx_builtin_setvar_helper(chan, v->name, v->value);
		v = v->next;
	}

	/* set linevariables */
	v = ((l) ? l->variables : NULL);
	while (chan && !ast_check_hangup(chan) && l && v) {
		pbx_builtin_setvar_helper(chan, v->name, v->value);
		v = v->next;
	}

	sccp_copy_string(chan->exten, shortenedNumber, sizeof(chan->exten));
	sccp_copy_string(d->lastNumber, c->dialedNumber, sizeof(d->lastNumber));
	sccp_channel_set_calledparty(c, c->dialedNumber, shortenedNumber);

	/* The 7961 seems to need the dialing callstate to record its directories information. */
 	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_DIALING);

	/* proceed call state is needed to display the called number.
	The phone will not display callinfo in offhook state */
	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT); 
	sccp_channel_send_callinfo(d, c);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_clearprompt(d, instance, c->callid);
	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);

	if (!ast_strlen_zero(shortenedNumber) && !ast_check_hangup(chan)
			&& ast_exists_extension(chan, chan->context, shortenedNumber, 1, l->cid_num) ) {
		/* found an extension, let's dial it */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) channel %s-%08x is dialing number %s\n", DEV_ID_LOG(d), l->name, c->callid, shortenedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		sccp_ast_setstate(c, AST_STATE_RING);
		if (ast_pbx_start(chan)) {
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
		}
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_SYSTEM, "ChannelUpdate",
			"Channel: %s\r\nUniqueid: %s\r\nChanneltype: %s\r\nSCCPdevice: %s\r\nSCCPline: %s\r\nSCCPcallid: %d\r\n",
			chan->name, chan->uniqueid, "SCCP", (d)?DEV_ID_LOG(d):"(null)", (l)?l->name:"(null)", (c)?c->callid:-1);
#endif
	} else {
		/* timeout and no extension match */
		sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_INVALIDNUMBER);
	}
	sccp_channel_unlock(c);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_DEVICE))(VERBOSE_PREFIX_1 "%s: (sccp_pbx_softswitch) quit\n", DEV_ID_LOG(d));

	return NULL;
}

/*!
 * \brief Send Digit to Asterisk
 * \param c SCCP Channel
 * \param digit Digit as char
 */
void sccp_pbx_senddigit(sccp_channel_t * c, char digit) {
	struct ast_frame f = { AST_FRAME_DTMF, };

	f.src = "SCCP";
	f.subclass = digit;

//	sccp_channel_lock(c);
	sccp_queue_frame(c, &f);
//	sccp_channel_unlock(c);
}

/*!
 * \brief Send Multiple Digits to Asterisk
 * \param c SCCP Channel
 * \param digits Multiple Digits as char
 */
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]) {
	int i;
	struct ast_frame f = { AST_FRAME_DTMF, 0};
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Sending digits %s\n", DEV_ID_LOG(c->device), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	f.offset = 0;
#ifdef CS_AST_NEW_FRAME_STRUCT
	f.data.ptr = NULL;
#else
	f.data = NULL;
#endif
	f.datalen = 0;

//	sccp_channel_lock(c);
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass = digits[i];
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(c->device), digits[i]);
		sccp_queue_frame(c, &f);
	}
//	sccp_channel_unlock(c);
}

/*!
 * \brief Queue an outgoing Asterisk frame
 * \param c SCCP Channel
 * \param f Asterisk Frame
 */
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame * f)
{
    if(c && c->owner)
        ast_queue_frame(c->owner, f);

/*
	for(;;) {
		if (c && c->owner && c->owner->_state == AST_STATE_UP) {
			if (!sccp_ast_channel_trylock(c->owner)) {
				ast_queue_frame(c->owner, f);
				sccp_ast_channel_unlock(c->owner);
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
}

/*!
 * \brief Queue a control frame
 * \param c SCCP Channel
 * \param control as Asterisk Control Frame Type
 */
#ifndef ASTERISK_CONF_1_2
int sccp_ast_queue_control(sccp_channel_t * c, enum ast_control_frame_type control)
#else
int sccp_ast_queue_control(sccp_channel_t * c, uint8_t control)
#endif
{
	struct ast_frame f = { AST_FRAME_CONTROL, };

	f.subclass = control;

	sccp_queue_frame(c, &f);

	return 0;
}
