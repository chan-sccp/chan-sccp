/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include <asterisk/musiconhold.h>
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_pbx.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include "sccp_features.h"

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
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#endif

static struct ast_frame * sccp_rtp_read(sccp_channel_t * c) {
#ifndef ASTERISK_CONF_1_2
	struct ast_frame * f = &ast_null_frame;
#else
	struct ast_frame * f = NULL;
#endif 
	
	if (c->rtp) {
		f = ast_rtp_read(c->rtp);
		if (c->owner) {
			/* We already hold the channel lock */
			if (f->frametype == AST_FRAME_VOICE) {
#ifndef ASTERISK_CONF_1_2
				if (!(f->subclass & (c->owner->nativeformats & AST_FORMAT_AUDIO_MASK))) {
#else
				if (!(f->subclass & (c->owner->nativeformats))) {
#endif				
					sccp_log(2)(VERBOSE_PREFIX_3 "%s: Channel %s changed format from %s(%d) to %s(%d)\n",
					DEV_ID_LOG(c->device),
					c->owner->name,
					ast_getformatname(f->subclass),
					f->subclass,
					ast_getformatname(c->owner->nativeformats),
					c->owner->nativeformats);

					c->owner->nativeformats = f->subclass;
					ast_set_read_format(c->owner, c->owner->readformat);
					ast_set_write_format(c->owner, c->owner->writeformat);
				}
			}
		}
	}
		
	return f;
}

static void * sccp_pbx_call_autoanswer_thread(void *data) {
	uint32_t *tmp = data;
	uint32_t callid = *tmp;
	sccp_channel_t * c;

	sleep(GLOB(autoanswer_ring_time));
	pthread_testcancel();
	c = sccp_channel_find_byid(callid);
	if (!c || !c->device)
		return NULL;
	if (c->state != SCCP_CHANNELSTATE_RINGING)
		return NULL;
	sccp_channel_answer(c);
	if (GLOB(autoanswer_tone) != SKINNY_TONE_SILENCE && GLOB(autoanswer_tone) != SKINNY_TONE_NOTONE)
		sccp_dev_starttone(c->device, GLOB(autoanswer_tone), c->line->instance, c->callid, 0);
	if (c->autoanswer_type == SCCP_AUTOANSWER_1W)
		sccp_dev_set_microphone(c->device, SKINNY_STATIONMIC_OFF);

	return NULL;
}


/** 
  * \brief this is for incoming calls asterisk sccp_request.
*/
static int sccp_pbx_call(struct ast_channel *ast, char *dest, int timeout) {
	sccp_line_t	 * l;
	sccp_device_t  * d;
	sccp_session_t * s;
	sccp_channel_t * c;
	const char * ringermode = NULL;
	pthread_attr_t attr;
	pthread_t t;
	char suffixedNumber[255] = { '\0' }; //!< For saving the digittimeoutchar to the logs */
	
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
	d = l->device;
	s = d->session;

	if (!l || !d || !s) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line or device or session\n", (c ? c->callid : 0) );
		return -1;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Asterisk request to call %s\n", d->id, ast->name);


	sccp_mutex_lock(&d->lock);

	/* DND handling*/
	if (d->dnd) {
		if (d->dndmode == SCCP_DNDMODE_REJECT || (d->dndmode == SCCP_DNDMODE_USERDEFINED && d->dnd == SCCP_DNDMODE_REJECT )) {
			ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
			if ( ringermode && !ast_strlen_zero(ringermode) ) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
				if (strcasecmp(ringermode, "urgent") == 0)
			    {
                    /* only if urgent call can be passed to phone */
					c->ringermode = SKINNY_STATION_URGENTRING;
                }
				else
				{
                    /* rejecting call as not urgent */ 
        			sccp_mutex_unlock(&d->lock);
        			ast_setstate(ast, AST_STATE_BUSY);
        			ast_queue_control(ast, AST_CONTROL_BUSY);
        			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Unurgent call with DND activated. Rejecting %s\n", d->id, ast->name);
        			return 0;
                }
			} else {
				sccp_mutex_unlock(&d->lock);
				ast_setstate(ast, AST_STATE_BUSY);
				ast_queue_control(ast, AST_CONTROL_BUSY);
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND is on. Call %s rejected\n", d->id, ast->name);
				return 0;
			}

		} else if (d->dndmode == SCCP_DNDMODE_SILENT || (d->dndmode == SCCP_DNDMODE_USERDEFINED && d->dnd == SCCP_DNDMODE_SILENT ) ) {
			/* disable the ringer and autoanswer options */
			c->ringermode = SKINNY_STATION_SILENTRING;
			c->autoanswer_type = SCCP_AUTOANSWER_NONE;
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND (silent) is on. Set the ringer = silent and autoanswer = off for %s\n", d->id, ast->name);
		}
	} 

	//handle DND on lines
	sccp_mutex_lock(&l->lock);
	if (1) {
		if (l->dndmode == SCCP_DNDMODE_REJECT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT )) {
			ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
			if ( ringermode && !ast_strlen_zero(ringermode) ) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
				if (strcasecmp(ringermode, "urgent") == 0)
					c->ringermode = SKINNY_STATION_URGENTRING;
			} else {
				sccp_mutex_unlock(&d->lock);
				ast_setstate(ast, AST_STATE_BUSY);
				ast_queue_control(ast, AST_CONTROL_BUSY);
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND for line \"%s\" is on. Call %s rejected\n", d->id, l->name, ast->name);
				return 0;
			}

		} else if (l->dndmode == SCCP_DNDMODE_SILENT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_SILENT ) ) {
			/* disable the ringer and autoanswer options */
			c->ringermode = SKINNY_STATION_SILENTRING;
			c->autoanswer_type = SCCP_AUTOANSWER_NONE;
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: DND (silent) for line \"%s\" is on. Set the ringer = silent and autoanswer = off for %s\n", d->id, l->name, ast->name);
		}
	}
	sccp_mutex_unlock(&l->lock);
	//end DND handling

	/* if incoming call limit is reached send BUSY */
	sccp_mutex_lock(&l->lock);
	if ( l->channelCount > l->incominglimit ) { /* >= just to be sure :-) */
		sccp_log(1)(VERBOSE_PREFIX_3 "Incoming calls limit (%d) reached on SCCP/%s... sending busy\n", l->incominglimit, l->name);
		sccp_mutex_unlock(&l->lock);
		sccp_mutex_unlock(&d->lock);
		ast_setstate(ast, AST_STATE_BUSY);
		ast_queue_control(ast, AST_CONTROL_BUSY);
		return 0;
	}
	sccp_mutex_unlock(&l->lock);

	/* autoanswer check */
	if (c->autoanswer_type) {
		if (d->channelCount > 1) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Autoanswer requested, but the device is in use\n", d->id);
			c->autoanswer_type = SCCP_AUTOANSWER_NONE;
			if (c->autoanswer_cause) {
				switch (c->autoanswer_cause) {
					case AST_CAUSE_CONGESTION:
						ast_queue_control(ast, AST_CONTROL_CONGESTION);
						break;
					default:
						ast_queue_control(ast, AST_CONTROL_BUSY);
						break;
				}
				sccp_mutex_unlock(&d->lock);
				return 0;
			}
		} else {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Autoanswer requested and activated %s\n", d->id, (c->autoanswer_type == SCCP_AUTOANSWER_1W) ? "with MIC OFF" : "with MIC ON");
		}
	}
	sccp_mutex_unlock(&d->lock);

  /* Set the channel callingParty Name and Number */
#ifdef CS_AST_CHANNEL_HAS_CID
	if(GLOB(recorddigittimeoutchar))
	{
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
		free(cidtmp);
		cidtmp = NULL;
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

	if ( sccp_channel_get_active(d) ) {
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CALLWAITING);
		ast_queue_control(ast, AST_CONTROL_RINGING);
	} else {
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_RINGING);
		ast_queue_control(ast, AST_CONTROL_RINGING);
		if (c->autoanswer_type) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", d->id, ast->name);
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, &c->callid)) {
				ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
			}
		}
	}
	/* someone forget to restore the asterisk lock */
	// sccp_ast_channel_lock(ast);
	
	return 0;
}


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
		sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		ast_log(LOG_DEBUG, "SCCP: Waiting to lock the channel %s for hangup\n", ast->name);
		usleep(200);
		c = CS_AST_CHANNEL_PVT(ast);
	}

	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asked to hangup channel %s. SCCP channel already deleted\n", ast->name);
		goto OUT;
	}

	CS_AST_CHANNEL_PVT(ast) = NULL;
	c->owner = NULL;
	l = c->line;
	d = l->device;

	if (c->rtp) {
		sccp_channel_closereceivechannel(c);
		usleep(200);
		sccp_channel_destroy_rtp(c);
		usleep(200);
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Current channel %s-%08x state %s(%d)\n", DEV_ID_LOG(d), l ? l->name : "(null)", c->callid, sccp_indicate2str(c->state), c->state);

	sccp_channel_send_callinfo(c);

	if (c->state != SCCP_CHANNELSTATE_DOWN) {
		/* we are in a passive hangup */
		if (GLOB(remotehangup_tone) && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active(d))
			sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_ONHOOK);
	}
	
	if (c->calltype != SKINNY_CALLTYPE_INBOUND && !c->hangupok) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Waiting for the dialing thread to go down on channel %s\n", DEV_ID_LOG(d), ast->name);
		while(!c->hangupok) {
			sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
			sccp_channel_unlock(c);
			usleep(200);
			sccp_channel_lock(c);
		}
	}
	
	sccp_channel_unlock(c);	
	sccp_channel_delete(c);

OUT:

	if (d && d->session) {
		sccp_session_lock(d->session);
		d->session->needcheckringback = 1;
		sccp_session_unlock(d->session);
	}

	return 0;
}

static int sccp_pbx_answer(struct ast_channel *ast) {
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	
#ifdef ASTERISK_CONF_1_2
	// if channel type is undefined, set to SCCP
	if (!ast->type) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Channel type undefined, setting to type 'SCCP'\n");
		ast->type = "SCCP";
	}
#endif

	if (!c || !c->device || !c->line) {
		ast_log(LOG_ERROR, "SCCP: Answered %s but no SCCP channel\n", ast->name);
		return -1;
	}


	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Outgoing call has been answered %s on %s@%s-%08x\n", ast->name, c->line->name, c->device->id, c->callid);
	/* This seems like brute force, and doesn't seem to be of much use. However, I want it to be remebered
	   as I have forgotten what my actual motivation was for writing this strange code. (-DD) */
	/*
    sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
    sccp_channel_send_callinfo(c);
    sccp_indicate_nolock(c, SCCP_CHANNELSTATE_PROCEED);
    sccp_channel_send_callinfo(c);
	*/
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONNECTED);
	return 0;
}


static struct ast_frame * sccp_pbx_read(struct ast_channel *ast) {
#ifndef ASTERISK_CONF_1_2	
	struct ast_frame *frame = &ast_null_frame;
#else
	struct ast_frame *frame = NULL;
#endif	
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	if(c) {
		// sccp_channel_lock(c);
		frame = sccp_rtp_read(c);
		// sccp_channel_unlock(c);
	}	
	return frame;
}

static int sccp_pbx_write(struct ast_channel *ast, struct ast_frame *frame) {
	int res = 0;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);

	if(c) {
		switch (frame->frametype) {
			case AST_FRAME_VOICE:
				// checking for samples to transmit
				if (!frame->samples) {
					if(strcasecmp(frame->src, "ast_prod")) {
						ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples\n", DEV_ID_LOG(c->device), frame->subclass);
						return -1;
					} else {
						// frame->samples == 0  when frame_src is ast_prod
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s\n", DEV_ID_LOG(c->device), ast->name);
					}
				} else if (!(frame->subclass & ast->nativeformats)) {
					char s1[512], s2[512], s3[512];
					ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats is %s(%d) read/write = %s(%d)/%s(%d)\n",
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
					return -1;
				}
				sccp_channel_lock(c);
				if (c->rtp)
					res = ast_rtp_write(c->rtp, frame);
				sccp_channel_unlock(c);
				break;
			case AST_FRAME_IMAGE:
			case AST_FRAME_VIDEO:
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
#ifdef CS_AST_CONTROL_SRCUPDATE
		case AST_CONTROL_SRCUPDATE:
				return "MediaSourceUpdate";
#endif
		default:
				return "Unknown";
		}
}

#ifdef ASTERISK_CONF_1_2
static int sccp_pbx_indicate(struct ast_channel *ast, int ind) {
#else
static int sccp_pbx_indicate(struct ast_channel *ast, int ind, const void *data, size_t datalen) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	int res = 0;
	if (!c)
		return -1;

	sccp_channel_lock(c);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' (%s) condition on channel %s\n", DEV_ID_LOG(c->device), ind, sccp_control2str(ind), ast->name);
	
/*
	if (c->state == SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: state for device is connected on channel %s\n", DEV_ID_LOG(c->device), ast->name);
		// let's asterisk emulate it 
		sccp_channel_unlock(c);
		return -1;
	}
*/
	
	/* when the rtp media stream is open we will let asterisk emulate the tones */
	if (c->rtp)
		res = -1;

	switch(ind) {

	case AST_CONTROL_RINGING:
		if(c->state != SCCP_CHANNELSTATE_CONNECTED)
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_RINGOUT);
		break;

	case AST_CONTROL_BUSY:
		if(c->state != SCCP_CHANNELSTATE_CONNECTED)
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_BUSY);
		break;

	case AST_CONTROL_CONGESTION:
		if(c->state != SCCP_CHANNELSTATE_CONNECTED)
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_CONGESTION);
		break;

	case AST_CONTROL_PROGRESS:
	case AST_CONTROL_PROCEEDING:
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_PROCEED);
		res = 0;
		break;
#ifdef CS_AST_CONTROL_SRCUPDATE
    case AST_CONTROL_SRCUPDATE:
        /* Source media has changed. */ 
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Source media has changed\n");
#ifdef CS_AST_RTP_NEW_SOURCE		
        if(c->rtp) {			
			ast_rtp_new_source(c->rtp);
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

	case -1:
		res = -1;
		break;

	default:
	  ast_log(LOG_WARNING, "SCCP: Don't know how to indicate condition %d\n", ind);
	  res = -1;
	}

	sccp_channel_unlock(c);
	return res;
}

static int sccp_pbx_fixup(struct ast_channel *oldchan, struct ast_channel *newchan) {
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(newchan);
	if (!c) {
		ast_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, oldchan, newchan->name, newchan);
		return -1;
	}
	sccp_mutex_lock(&c->lock);
	if (c->owner != oldchan) {
		ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, c->owner);
		sccp_mutex_unlock(&c->lock);
		return -1;
	}
	c->owner = newchan;
	sccp_mutex_unlock(&c->lock);
	return 0;
}

#ifndef ASTERISK_CONF_1_2
static int sccp_pbx_recvdigit_begin(struct ast_channel *ast, char digit) {
	return -1;
}
#endif

#ifdef ASTERISK_CONF_1_2
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit) {
#else
static int sccp_pbx_recvdigit_end(struct ast_channel *ast, char digit, unsigned int duration) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	sccp_device_t * d = NULL;

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
	sccp_dev_starttone(c->device, (uint8_t) digit, c->line->instance, c->callid, 0);
	return 0;
}

#ifdef CS_AST_HAS_TECH_PVT
static int sccp_pbx_sendtext(struct ast_channel *ast, const char *text) {
#else
static int sccp_pbx_sendtext(struct ast_channel *ast, char *text) {
#endif
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	sccp_device_t * d;

	if (!c || !c->device)
		return -1;

	d = c->device;
	if (!text || ast_strlen_zero(text))
		return 0;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);
	sccp_dev_displayprompt(d, c->line->instance, c->callid, (char *)text, 10);
	return 0;
}

#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech = {
	.type = "SCCP",
	.description = "Skinny Client Control Protocol (SCCP)",
	.capabilities = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A,
	.requester = sccp_request,
	.devicestate = sccp_devicestate,
	.call = sccp_pbx_call,
	.hangup = sccp_pbx_hangup,
	.answer = sccp_pbx_answer,
	.read = sccp_pbx_read,
	.write = sccp_pbx_write,
	.indicate = sccp_pbx_indicate,
	.fixup = sccp_pbx_fixup,
#ifndef ASTERISK_CONF_1_2
	.send_digit_begin = sccp_pbx_recvdigit_begin,
	.send_digit_end = sccp_pbx_recvdigit_end,
#else
	.send_digit = sccp_pbx_recvdigit_end,
#endif
	.send_text = sccp_pbx_sendtext,
/*	.bridge = ast_rtp_bridge */
};
#endif

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c) {
	sccp_device_t * d = c->device;
	struct ast_channel * tmp;
	sccp_line_t * l = c->line;
	int fmt;

	#ifndef CS_AST_CHANNEL_HAS_CID
	char cidtmp[256];
	memset(&cidtmp, 0, sizeof(cidtmp));
#endif

	if (!l || !d || !d->session) {
		ast_log(LOG_ERROR, "SCCP: Unable to allocate asterisk channel\n");
		return 0;
	}

//	sccp_mutex_unlock(&c->lock);
//	/* Don't hold a sccp pvt lock while we allocate a channel */	
	
#ifdef ASTERISK_CONF_1_2
	tmp = ast_channel_alloc(1);
#else
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:     cid_num: \"%s\"\n", l->cid_num);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:    cid_name: \"%s\"\n", l->cid_name);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: accountcode: \"%s\"\n", l->accountcode);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:       exten: \"%s\"\n", c->dialedNumber);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:     context: \"%s\"\n", l->context);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:    amaflags: \"%d\"\n", l->amaflags);
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP:   chan/call: \"%s-%08x\"\n", l->name, c->callid);
	
	/* This should definetly fix CDR */
    tmp = ast_channel_alloc(1, AST_STATE_DOWN, l->cid_num, l->cid_name, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08x", l->name, c->callid);
#endif
       // tmp = ast_channel_alloc(1); function changed in 1.4.0
       // Note: Assuming AST_STATE_DOWN is starting state
	if (!tmp || tmp == NULL) {
		ast_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", d->id, l->name);
		return 0;
	}

	/* need to reset the exten, otherwise it would be set to s */
	memset(&tmp->exten,0,sizeof(tmp->exten));

	/* let's connect the ast channel to the sccp channel */
	sccp_channel_lock(c);
	c->owner = tmp;
	sccp_channel_unlock(c);
	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Global Capabilities: %d\n", d->id, GLOB(global_capability));

	sccp_line_lock(l);
	sccp_device_lock(d);
	
	// tmp->nativeformats = (d->capability ? d->capability : GLOB(global_capability)); 
	tmp->nativeformats = ast_codec_choose(&d->codecs, (d->capability ? d->capability : GLOB(global_capability)), 1);
	
	if(!tmp->nativeformats)	{
		ast_log(LOG_ERROR, "%s: No audio format to offer. Cancelling call on line %s\n", d->id, l->name);
		return 0;
	}
			
	// fmt = ast_codec_choose(&d->codecs, tmp->nativeformats, 1);
	fmt = tmp->nativeformats;	
	c->format = fmt;
	
	sccp_device_unlock(d);
	sccp_line_unlock(l);
	
#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(tmp, name, "SCCP/%s-%08x", l->name, c->callid);
#else
	snprintf(tmp->name, sizeof(tmp->name), "SCCP/%s-%08x", l->name, c->callid);
#endif

  char s1[512], s2[512];
  sccp_log(2)(VERBOSE_PREFIX_3 "%s: Channel %s, capabilities: DEVICE %s(%d) PREFERRED %s(%d) USED %s(%d)\n",
	DEV_ID_LOG(c->device),
	tmp->name,
#ifndef ASTERISK_CONF_1_2
	ast_getformatname_multiple(s1, sizeof(s1) -1, d->capability & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s1, sizeof(s1) -1, d->capability),
#endif		
	d->capability,
#ifndef ASTERISK_CONF_1_2		
	ast_getformatname_multiple(s2, sizeof(s2) -1, tmp->nativeformats & AST_FORMAT_AUDIO_MASK),
#else
	ast_getformatname_multiple(s2, sizeof(s2) -1, tmp->nativeformats),
#endif		
	tmp->nativeformats,
	ast_getformatname(fmt),
	fmt);

#ifndef CS_AST_HAS_TECH_PVT
	tmp->type = "SCCP";
#endif
	// tmp->nativeformats		= fmt;
	tmp->writeformat		= fmt;
	tmp->readformat 		= fmt;

#ifdef CS_AST_HAS_TECH_PVT
	tmp->tech			= &sccp_tech;
	tmp->tech_pvt			= c;
#else
	tmp->pvt->pvt			= c;
	tmp->pvt->rawwriteformat 	= fmt;
	tmp->pvt->rawreadformat  	= fmt;

	tmp->pvt->answer		= sccp_pbx_answer;
	tmp->pvt->hangup		= sccp_pbx_hangup;
	tmp->pvt->call			= sccp_pbx_call;

	tmp->pvt->read			= sccp_pbx_read;
	tmp->pvt->write 		= sccp_pbx_write;
/*	tmp->pvt->devicestate    	= sccp_devicestate; */
	tmp->pvt->indicate		= sccp_pbx_indicate;
	tmp->pvt->fixup 		= sccp_pbx_fixup;
	tmp->pvt->send_digit	 	= sccp_pbx_recvdigit;
	tmp->pvt->send_text		= sccp_pbx_sendtext;
#endif

	tmp->adsicpe		 	= AST_ADSI_UNAVAILABLE;

	// XXX: Bridge?
	// XXX: Transfer?

	sccp_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)++;
	sccp_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

#ifdef CS_AST_CHANNEL_HAS_CID
	if (l->cid_num)
	  tmp->cid.cid_num = strdup(l->cid_num);
	if (l->cid_name)
	  tmp->cid.cid_name = strdup(l->cid_name);
#else
	snprintf(cidtmp, sizeof(cidtmp), "\"%s\" <%s>", l->cid_name, l->cid_num);
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
	tmp->callgroup = l->callgroup;
#ifdef CS_SCCP_PICKUP
	tmp->pickupgroup = l->pickupgroup;
#endif
	tmp->priority = 1;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s-%08x\n", d->id, l->name, c->callid);
	return 1;
}

void * sccp_pbx_simpleswitch(sccp_channel_t * c) {
	struct ast_channel * chan = c->owner;
	struct ast_variable *v = NULL;
	// sccp_channel_t * c;
	sccp_line_t * l;
	sccp_device_t * d;
	uint8_t res_exten = 0, res_wait = 0, res_timeout = 0;
	char shortenedNumber[256] = { '\0' }; /* For recording the digittimeoutchar */
	
    if(!c || c == NULL)
    {
         sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Channel not found. Leaving\n");
         return NULL;
    }

	/* channel already locked in sccp_channel_newcall_thread */
	// sccp_channel_lock(c);
	sccp_device_lock(c->device);
	
	if(c->device && c->device->id)
	{
        sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Got device %s\n", c->device->id);
    }
    else
    {
        sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available.");
        return NULL;
    }

    if (!c->device || !c->device->id) {
		/* let's go out as soon as possibile */
		if (!ast_test_flag(chan, AST_FLAG_ZOMBIE)) {
			// sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for %s\n", (chan) ? chan->name : "(null)");
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available (1)\n");
			if (chan)
				ast_hangup(chan);
		} else {
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for zombie (1)\n");
		}
		return NULL;
	}
  
	if (strlen(c->device->id)<3 || (strncmp(c->device->id,"SEP",3)!=0 && strncmp(c->device->id,"ATA",3)!=0)) {
		if (!ast_test_flag(chan, AST_FLAG_ZOMBIE)) {
			// sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for %s\n", (chan) ? chan->name : "(null)");
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available (2)\n");
				if (chan)
					ast_hangup(chan);
		} else {
			sccp_log(1)( VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for zombie (2)\n");
		}
		return NULL;
	}

	sccp_device_unlock(c->device);
	
	c->hangupok = 0;

	l = c->line;
	d = c->device;
      
	if ( !l || !d) {
		/* let's go out as soon as possibile */
        ast_log(LOG_ERROR, "%s: return from the dial thread. No line or device defined for channel %d\n", (d ? DEV_ID_LOG(d) : "SCCP"), (c ? c->callid : -1));
		c->hangupok = 1;
		sccp_channel_unlock(c);
		if (chan)
			ast_hangup(chan);
		return NULL;
	}
	
	sccp_log(1)( VERBOSE_PREFIX_3 "%s: New call on line %s\n", DEV_ID_LOG(d), l->name);

	/* Maybe inbound calls should not be processed -FS */
	if(c->calltype != SKINNY_CALLTYPE_INBOUND)
		sccp_channel_set_callingparty(c, l->cid_name, l->cid_num);
	else
	{
		sccp_channel_unlock(c);
		return NULL;
	}

	if (!ast_strlen_zero(c->dialedNumber) && (c->ss_action == SCCP_SS_DIAL)) {
		/* we have a number to dial. Let's do it */
		sccp_log(10)( VERBOSE_PREFIX_3 "%s: Dialing %s on channel %s-%08x\n", DEV_ID_LOG(l->device), c->dialedNumber, l->name, c->callid);
		sccp_channel_set_calledparty(c, c->dialedNumber, c->dialedNumber);
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
		sccp_channel_unlock(c);
		
		if(GLOB(recorddigittimeoutchar)) {
			if(strlen(c->dialedNumber) > 0 && strlen(c->dialedNumber) < 255 && '#' == c->dialedNumber[strlen(c->dialedNumber)-1])
				strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-1);
			else
				strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-0);
		}
		else
			strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-0);		
	} else {
		/* we have to collect the number */
		
		/* the phone is on TsOffHook state */
		sccp_log(10)( VERBOSE_PREFIX_3 "%s: Waiting for an extension on channel %s-%08x\n", DEV_ID_LOG(l->device), l->name, c->callid);
		
		/* let's use the keypad to collect digits */		
		c->digittimeout = time(0)+GLOB(firstdigittimeout);

		sccp_channel_unlock(c);

		pthread_testcancel();
		
		res_exten = 1;
		
		do {
			sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
			usleep(100);			
			sccp_channel_lock(c);
			if (!ast_strlen_zero(c->dialedNumber)) {
				/* It never occured to me what the purpose of the following line was. I modified it to avoid the limitation to
				   extensions not starting with '*' only. (-DD) 
				/res_exten = (c->dialedNumber[0] == '*' || ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num)); */
				if(GLOB(recorddigittimeoutchar)) {
					if(strlen(c->dialedNumber) > 0 && strlen(c->dialedNumber) < 255 && '#' == c->dialedNumber[strlen(c->dialedNumber)-1])
				  		strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-1);
					else
				  		strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-0);
					if((c->ss_action != SCCP_SS_GETCBARGEROOM) && (c->ss_action != SCCP_SS_GETMEETMEROOM))
						res_exten = (ast_matchmore_extension(chan, chan->context, shortenedNumber, 1, l->cid_num));
				}
				else if((c->ss_action != SCCP_SS_GETCBARGEROOM) && (c->ss_action != SCCP_SS_GETMEETMEROOM))
					res_exten = (ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num));
			}
			if (! (res_wait = ( c->state == SCCP_CHANNELSTATE_DOWN || chan->_state == AST_STATE_DOWN
								|| chan->_softhangup || c->calltype == SKINNY_CALLTYPE_INBOUND)) ) {
				if (CS_AST_CHANNEL_PVT(chan)) {
					res_timeout = (time(0) < c->digittimeout);
				} else
					res_timeout = 0;
			}
			sccp_channel_unlock(c);
		} while (!res_wait && res_exten && res_timeout);

		if (res_wait != 0) {			
			if(c->calltype == SKINNY_CALLTYPE_INBOUND) {			
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (simpleswitch) Leaving the dial thread for PICKUP\n", d->id);
			}
			else if(chan->_softhangup) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (simpleswitch) Leaving the dial thread cause SOFTHANGUP\n", d->id);
			}
			else {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: (simpleswitch) Leaving the dial thread on state: SCCP (%s) - ASTERISK (%s)\n", d->id, sccp_indicate2str(c->state), ast_state2str(chan->_state));
			}
			c->hangupok = 1;
			return NULL;
		}

		if(GLOB(recorddigittimeoutchar)) {
			if(strlen(c->dialedNumber) > 0 && strlen(c->dialedNumber) < 255 && '#' == c->dialedNumber[strlen(c->dialedNumber)-1])
				strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-1);
			else
				strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-0);
		}
		else
			strncpy(shortenedNumber, c->dialedNumber, strlen(c->dialedNumber)-0);
		
		// we would hear last keypad stroke before starti to dial. right ? so let's wait 200 ms
		sccp_safe_sleep(200); 
		
		/* This will choose what to do */
		switch(c->ss_action) {
			case SCCP_SS_GETFORWARDEXTEN:
				sccp_channel_unlock(c);				
				c->hangupok = 1;
				sccp_channel_endcall(c);
				usleep(200); // this is needed as main thread should wait sccp_channel_endcall do his work.
				if(!ast_strlen_zero(shortenedNumber)) {
					sccp_line_cfwd(l, c->ss_data, shortenedNumber);
				}
				return NULL; // leave simpleswitch without dial
#ifdef CS_SCCP_PICKUP
			case SCCP_SS_GETPICKUPEXTEN:
				sccp_channel_unlock(c);									
				c->hangupok = 1;
				// like we're dialing but we're not :)
				sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
				sccp_channel_set_callstate(c, SKINNY_CALLSTATE_PROCEED);
				sccp_channel_send_callinfo(c);
				sccp_dev_clearprompt(d, c->line->instance, c->callid);
				sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
				
				if(!ast_strlen_zero(shortenedNumber)) {
					sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk request to pickup exten '%s'\n", shortenedNumber);									
					if(sccp_feat_directpickup(c, shortenedNumber)) {
						sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
					}
				}
				else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
					usleep(200); // this is needed as main thread should wait sccp_channel_endcall do his work.
					// sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
				return NULL; // leave simpleswitch without dial
#endif
			case SCCP_SS_GETMEETMEROOM:
				sccp_channel_unlock(c);
				if(!ast_strlen_zero(shortenedNumber) && !ast_strlen_zero(c->line->meetmenum)) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Meetme request for room '%s' on extension '%s'\n", d->id, shortenedNumber, c->line->meetmenum);
					pbx_builtin_setvar_helper(c->owner, "SCCP_MEETME_ROOM", shortenedNumber);
					sccp_copy_string(shortenedNumber, c->line->meetmenum, sizeof(shortenedNumber));
					sccp_copy_string(c->dialedNumber, SKINNY_DISP_CONFERENCE, sizeof(c->dialedNumber));
				} else {
					// without a number we can also close the call. Isn't it true ?
					c->hangupok = 1;
					sccp_channel_endcall(c);
					usleep(200); // this is needed as main thread should wait sccp_channel_endcall do his work.
					return NULL;
				}
				break;
			case SCCP_SS_GETBARGEEXTEN:
				sccp_channel_unlock(c);
				c->hangupok = 1;
				// like we're dialing but we're not :)
				sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
				sccp_channel_set_callstate(c, SKINNY_CALLSTATE_PROCEED);
				sccp_channel_send_callinfo(c);
				sccp_dev_clearprompt(d, c->line->instance, c->callid);
				sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);				
				if(!ast_strlen_zero(shortenedNumber)) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Device request to barge exten '%s'\n", d->id, shortenedNumber);
					if(sccp_feat_barge(c, shortenedNumber)) {
						sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
					}
				} else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
					usleep(200); // this is needed as main thread should wait sccp_channel_endcall do his work.
					// sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
				return NULL; // leave simpleswitch without dial
			case SCCP_SS_GETCBARGEROOM:
				sccp_channel_unlock(c);
				c->hangupok = 1;
				// like we're dialing but we're not :)
				sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
				sccp_channel_set_callstate(c, SKINNY_CALLSTATE_PROCEED);
				sccp_channel_send_callinfo(c);
				sccp_dev_clearprompt(d, c->line->instance, c->callid);
				sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);				
				if(!ast_strlen_zero(shortenedNumber)) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Device request to barge conference '%s'\n", d->id, shortenedNumber);
					if(sccp_feat_cbarge(c, shortenedNumber)) {
						sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
					}
				} else {
					// without a number we can also close the call. Isn't it true ?
					sccp_channel_endcall(c);
					usleep(200); // this is needed as main thread should wait sccp_channel_endcall do his work.
					// sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
				}
				return NULL; // leave simpleswitch without dial			
			case SCCP_SS_DIAL:
			default:
			break;			
		}
	}
	
	/* LET'S DIAL */
	
	/* set devicevariables */
	for (v = d->variables ; v ; v = v->next)
		pbx_builtin_setvar_helper(chan, v->name, v->value);

	/* set linevariables */
	for (v = l->variables ; v ; v = v->next)
		pbx_builtin_setvar_helper(chan, v->name, v->value);

	//set private var
	if(chan){
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: set variable SKINNY_PRIVATE to: %s\n", c->private ? "1" : "0");
		pbx_builtin_setvar_helper(chan, "SKINNY_PRIVATE", c->private ? "1" : "0");
	}
	//private set

	sccp_channel_lock(c);	
	sccp_copy_string(chan->exten, shortenedNumber, sizeof(chan->exten));
	sccp_copy_string(d->lastNumber, c->dialedNumber, sizeof(d->lastNumber));
	sccp_channel_set_calledparty(c, c->dialedNumber, shortenedNumber);
	/* The 7961 seems to need the dialing callstate to record its directories information. */
 	sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
	/* proceed call state is needed to display the called number.
	The phone will not display callinfo in offhook state */
	sccp_channel_set_callstate(c, SKINNY_CALLSTATE_PROCEED);
	sccp_channel_send_callinfo(c);
	sccp_dev_clearprompt(d, c->line->instance, c->callid);
	sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
	sccp_channel_unlock(c);
	
	c->hangupok = 1;
	
	if ( !ast_strlen_zero(shortenedNumber)
			&& ast_exists_extension(chan, chan->context, shortenedNumber, 1, l->cid_num) ) {
		/* found an extension, let's dial it */
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: channel %s-%08x is dialing number %s\n", DEV_ID_LOG(l->device), l->name, c->callid, shortenedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		sccp_ast_setstate(c, AST_STATE_RING);
		if (ast_pbx_run(chan)) {
			sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
		}
	} else {
		/* timeout and no extension match */
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_INVALIDNUMBER);
	}
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: return from the startchannel on exit\n", DEV_ID_LOG(l->device));
	return NULL;
}

void sccp_pbx_senddigit(sccp_channel_t * c, char digit) {
	struct ast_frame f = { AST_FRAME_DTMF, };

	f.src = "SCCP";
	f.subclass = digit;

	sccp_channel_lock(c);
	sccp_queue_frame(c, &f);
	sccp_channel_unlock(c);
}

void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]) {
	int i;
	struct ast_frame f = { AST_FRAME_DTMF, 0};
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending digits %s\n", DEV_ID_LOG(c->device), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	f.offset = 0;
#ifdef CS_AST_NEW_FRAME_STRUCT
	f.data.ptr = NULL;
#else	
	f.data = NULL;
#endif	
	f.datalen = 0;

	sccp_channel_lock(c);
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass = digits[i];
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(c->device), digits[i]);
		sccp_queue_frame(c, &f);
	}
	sccp_channel_unlock(c);
}

/**
 * \brief Queue an outgoing frame 
 * 
 * \param c channel
 * \param f ast_frame
 */
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame * f)
{
	for(;;) {
		if (c && c->owner && c->owner->_state == AST_STATE_UP) {
			if (!sccp_ast_channel_trylock(c->owner)) {
				ast_queue_frame(c->owner, f);
				sccp_ast_channel_unlock(c->owner);
				break;
			} else {
				/* strange deadlocks happens here :D -FS */
				// sccp_channel_unlock(c);
				usleep(1);
				// sccp_channel_lock(c);
			}
		} else
			break;
	}
}

/*! \brief Queue a control frame
  * 
  * \param c channel
  * \param control ast_control_frame_type
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
