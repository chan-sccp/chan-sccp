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
#include "sccp_pbx.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"

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

static struct ast_frame * sccp_rtp_read(sccp_channel_t * c) {
	/* Retrieve audio/etc from channel.  Assumes c->lock is already held. */
	struct ast_frame * f;
	/* if c->rtp not null */
	if (!c->rtp)
		return NULL;

	f = ast_rtp_read(c->rtp);

	if (c->owner) {
		/* We already hold the channel lock */
		if (f->frametype == AST_FRAME_VOICE) {
			if (f->subclass != c->owner->nativeformats) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Oooh, format changed to %d from %d on channel %d\n", c->device->id, f->subclass, c->owner->nativeformats, c->callid);
				c->owner->nativeformats = f->subclass;
				ast_set_read_format(c->owner, c->owner->readformat);
				ast_set_write_format(c->owner, c->owner->writeformat);
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
	if (c->state != SCCP_CHANNELSTATE_RINGIN)
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

#ifndef CS_AST_CHANNEL_HAS_CID
	char * name, * number, *cidtmp; // For the callerid parse below
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

	ast_mutex_lock(&d->lock);

	
	/* DND handling*/
	if (d->dnd) {
		if (d->dndmode == SCCP_DNDMODE_REJECT || (d->dndmode == SCCP_DNDMODE_USERDEFINED && d->dnd == SCCP_DNDMODE_REJECT )) {
			ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
			if ( ringermode && !ast_strlen_zero(ringermode) ) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
				if (strcasecmp(ringermode, "urgent") == 0)
					c->ringermode = SKINNY_STATION_URGENTRING;
			} else {
				ast_mutex_unlock(&d->lock);
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
	ast_mutex_lock(&l->lock);
	if (1) {
		if (l->dndmode == SCCP_DNDMODE_REJECT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT )) {
			ringermode = pbx_builtin_getvar_helper(ast, "ALERT_INFO");
			if ( ringermode && !ast_strlen_zero(ringermode) ) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Found ALERT_INFO=%s\n", d->id, ringermode);
				if (strcasecmp(ringermode, "urgent") == 0)
					c->ringermode = SKINNY_STATION_URGENTRING;
			} else {
				ast_mutex_unlock(&d->lock);
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
	ast_mutex_unlock(&l->lock);
	//end DND handling

	/* if incoming call limit is reached send BUSY */
	ast_mutex_lock(&l->lock);
	if ( l->channelCount > l->incominglimit ) { /* >= just to be sure :-) */
		sccp_log(1)(VERBOSE_PREFIX_3 "Incoming calls limit (%d) reached on SCCP/%s... sending busy\n", l->incominglimit, l->name);
		ast_mutex_unlock(&l->lock);
		ast_mutex_unlock(&d->lock);
		ast_setstate(ast, AST_STATE_BUSY);
		ast_queue_control(ast, AST_CONTROL_BUSY);
		return 0;
	}
	ast_mutex_unlock(&l->lock);

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
				ast_mutex_unlock(&d->lock);
				return 0;
			}
		} else {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Autoanswer requested and activated %s\n", d->id, (c->autoanswer_type == SCCP_AUTOANSWER_1W) ? "with MIC OFF" : "with MIC ON");
		}
	}
	ast_mutex_unlock(&d->lock);

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

	/* release the asterisk lock */
	ast_mutex_unlock(&ast->lock);
	if ( sccp_channel_get_active(d) ) {
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CALLWAITING);
		ast_queue_control(ast, AST_CONTROL_RINGING);
	} else {
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_RINGIN);
		ast_queue_control(ast, AST_CONTROL_RINGING);
		if (c->autoanswer_type) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Running the autoanswer thread on %s\n", d->id, ast->name);
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			if (ast_pthread_create(&t, &attr, sccp_pbx_call_autoanswer_thread, &c->callid)) {
				ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%d) %s\n", d->id, l->name, c->callid, strerror(errno));
			}
		}
	}
	return 0;
}


static int sccp_pbx_hangup(struct ast_channel * ast) {
	sccp_channel_t * c;
	sccp_line_t    * l = NULL;
	sccp_device_t  * d = NULL;
	int res = 0;
	
	/* here the ast channel is locked */

	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk request to hangup channel %s\n", ast->name);
	
	ast_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)--;
	ast_mutex_unlock(&GLOB(usecnt_lock));

	ast_update_use_count();

	c = CS_AST_CHANNEL_PVT(ast);

	while (c && ast_mutex_trylock(&c->lock)) {
		ast_log(LOG_DEBUG, "SCCP: Waiting to lock the channel %s for hangup\n", ast->name);
		usleep(1000);
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
		sccp_channel_stop_rtp(c);
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Current channel %s-%d state %s(%d)\n", DEV_ID_LOG(d), l ? l->name : "(null)", c->callid, sccp_indicate2str(c->state), c->state);

	sccp_channel_send_callinfo(c);

	if ( c->state != SCCP_CHANNELSTATE_DOWN) {
		/* we are in a passive hangup */
		if (GLOB(remotehangup_tone) && d->state == SCCP_DEVICESTATE_OFFHOOK && c == sccp_channel_get_active(d))
			sccp_dev_starttone(d, GLOB(remotehangup_tone), 0, 0, 10);
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_ONHOOK);
	}

	if (c->calltype == SKINNY_CALLTYPE_OUTBOUND && !c->hangupok) {
		ast_mutex_unlock(&c->lock);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Waiting for the dialing thread to go down on channel %s\n", DEV_ID_LOG(d), ast->name);
		do {
			usleep(10000);
			ast_mutex_lock(&c->lock);
			res = c->hangupok;
			ast_mutex_unlock(&c->lock);
		} while (!res);
	} else {
		ast_mutex_unlock(&c->lock);
	}

	sccp_channel_delete(c);

OUT:

	if (d && d->session) {
		ast_mutex_lock(&d->session->lock);
		d->session->needcheckringback = 1;
		ast_mutex_unlock(&d->session->lock);
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


	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Outgoing call has been answered %s on %s@%s-%d\n", ast->name, c->line->name, c->device->id, c->callid);
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
	struct ast_frame *fr;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);
	if (!c)
		return NULL;
	ast_mutex_lock(&c->lock);
	fr = sccp_rtp_read(c);
	ast_mutex_unlock(&c->lock);
	return fr;
}

static int sccp_pbx_write(struct ast_channel *ast, struct ast_frame *frame) {
	int res = 0;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(ast);

	if (!c)
		return 0;

	if (frame->frametype != AST_FRAME_VOICE) {
		if (frame->frametype == AST_FRAME_IMAGE)
			return 0;
		else {
			ast_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %d\n", DEV_ID_LOG(c->device), frame->frametype, (c) ? c->callid : 0);
			return 0;
		}
	} else {
		if (!(frame->subclass & ast->nativeformats)) {
			ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats is %d (read/write = %d/%d)\n",
			DEV_ID_LOG(c->device), frame->subclass, ast->nativeformats, ast->readformat, ast->writeformat);
			/* As asterisk will hang up if we return -1 here, we better don't ;-)
			 * We can safely do so as asterisk returns 0 if there is no need for a conversion.
			 * with some channels like IAX. However, we need some serious discussion on this
			 * topic. This is currently a dirty hack to cope with *'s new behaviour. (-DD)
			 */
			 /* return -1; */
		}
	}
	if (c) {
		ast_mutex_lock(&c->lock);
		if (c->rtp) {
			res =  ast_rtp_write(c->rtp, frame);
		}
		ast_mutex_unlock(&c->lock);
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

	ast_mutex_lock(&c->lock);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' (%s) condition on channel %s\n", DEV_ID_LOG(c->device), ind, sccp_control2str(ind), ast->name);
	if (c->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* let's asterisk emulate it */
		ast_mutex_unlock(&c->lock);
		return -1;;

	}
	
	/* when the rtp media stream is open we will let asterisk emulate the tones */
	if (c->rtp)
		res = -1;

	switch(ind) {

	case AST_CONTROL_RINGING:
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_RINGOUT);
		break;

	case AST_CONTROL_BUSY:
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_BUSY);
		break;

	case AST_CONTROL_CONGESTION:
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_CONGESTION);
		break;

	case AST_CONTROL_PROGRESS:
	case AST_CONTROL_PROCEEDING:
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_PROCEED);
		res = 0;
		break;

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

	ast_mutex_unlock(&c->lock);
	return res;
}

static int sccp_pbx_fixup(struct ast_channel *oldchan, struct ast_channel *newchan) {
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(newchan);
	if (!c) {
		ast_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, oldchan, newchan->name, newchan);
		return -1;
	}
	ast_mutex_lock(&c->lock);
	if (c->owner != oldchan) {
		ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, c->owner);
		ast_mutex_unlock(&c->lock);
		return -1;
	}
	c->owner = newchan;
	ast_mutex_unlock(&c->lock);
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

#ifdef ASTERISK_CONF_1_2
	tmp = ast_channel_alloc(1);
#else
	tmp = ast_channel_alloc(1, AST_STATE_DOWN, l->cid_num, l->cid_name, "SCCP/%s", l->name, NULL, 0, NULL);
#endif
       // tmp = ast_channel_alloc(1); function changed in 1.4.0
       // Note: Assuming AST_STATE_DOWN is starting state
	if (!tmp) {
		ast_log(LOG_ERROR, "%s: Unable to allocate asterisk channel on line %s\n", d->id, l->name);
		return 0;
	}

	/* need to reset the exten, otherwise it would be set to s */
	memset(&tmp->exten,0,sizeof(tmp->exten));

	/* let's connect the ast channel to the sccp channel */
	ast_mutex_lock(&c->lock);
	c->owner = tmp;
	ast_mutex_unlock(&c->lock);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Global Capabilities: %d\n", d->id, GLOB(global_capability));

	ast_mutex_lock(&l->lock);
	ast_mutex_lock(&d->lock);
	tmp->nativeformats = (d->capability ? d->capability : GLOB(global_capability));
	if (tmp->nativeformats & c->format) {
		fmt = c->format;
	} else {
		fmt = ast_codec_choose(&d->codecs, tmp->nativeformats, 1);
		c->format = fmt;
	}
	ast_mutex_unlock(&l->lock);
	ast_mutex_unlock(&d->lock);
	sccp_log(2)(VERBOSE_PREFIX_3 "%s: format request: %d/%d\n", d->id, tmp->nativeformats, c->format);

#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(tmp, name, "SCCP/%s-%08x", l->name, c->callid);
#else
	snprintf(tmp->name, sizeof(tmp->name), "SCCP/%s-%08x", l->name, c->callid);
#endif

	if (GLOB(debug) > 2) {
	  const unsigned slen=512;
	  char s1[slen];
	  char s2[slen];
	  sccp_log(2)(VERBOSE_PREFIX_3 "%s: Channel %s, capabilities: DEVICE %s NATIVE %s BEST %d (%s)\n",
		d->id,
		c->owner->name,
		ast_getformatname_multiple(s1, slen, d->capability),
		ast_getformatname_multiple(s2, slen, tmp->nativeformats),
		fmt, ast_getformatname(fmt));
	}

#ifndef CS_AST_HAS_TECH_PVT
	tmp->type = "SCCP";
#endif
	tmp->nativeformats		= fmt;
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

	ast_mutex_lock(&GLOB(usecnt_lock));
	GLOB(usecnt)++;
	ast_mutex_unlock(&GLOB(usecnt_lock));

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
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Allocated asterisk channel %s-%d\n", d->id, l->name, c->callid);
	return 1;
}

void * sccp_pbx_startchannel(void *data) {
	struct ast_channel * chan = data;
	struct ast_variable *v = NULL;
	sccp_channel_t * c;
	sccp_line_t * l;
	sccp_device_t * d;
	uint8_t res_exten = 0, res_wait = 0, res_timeout = 0;
	char shortenedNumber[256] = { '\0' }; /* For recording the digittimeoutchar */

	/*sccp_log(1)( VERBOSE_PREFIX_3 "CS_AST_CHANNEL_PVT: %s\n", chan->tech_pvt);*/

	c = CS_AST_CHANNEL_PVT(chan);
	if (!c || !c->device || !c->device->id) {
		/* let's go out as soon as possibile */
		if (!ast_test_flag(chan, AST_FLAG_ZOMBIE)) {
			ast_log(LOG_ERROR, "SCCP: return from the dial thread. No sccp channel available for %s\n", (chan) ? chan->name : "(null)");
			if (chan)
				ast_hangup(chan);
		} else {
			ast_log(LOG_DEBUG, "SCCP: return from the dial thread. No sccp channel available for zombie\n");
		}
		return NULL;
	}
	if (strlen(c->device->id)<3 || (strncmp(c->device->id,"SEP",3)!=0 && strncmp(c->device->id,"ATA",3)!=0)) {
		if (!ast_test_flag(chan, AST_FLAG_ZOMBIE)) {
			sccp_log(1)( VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for %s\n", (chan) ? chan->name : "(null)");
				if (chan)
					ast_hangup(chan);
		} else {
			sccp_log(1)( VERBOSE_PREFIX_3 "SCCP: return from the dial thread. No sccp channel available for zombie\n");
		}
		return NULL;
	}
	
	/* this is an outgoung call */
	ast_mutex_lock(&c->lock);
	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	c->hangupok = 0;
	
	l = c->line;
	d = c->device;
	
	sccp_log(1)( VERBOSE_PREFIX_3 "GOT data %s (%p)\n", chan->name, chan);
	sccp_log(1)( VERBOSE_PREFIX_3 "GOT line %s\n", c->line->name);
	sccp_log(1)( VERBOSE_PREFIX_3 "GOT device %s\n", c->device->id);

	if ( !l || !d) {
		/* let's go out as soon as possibile */
		ast_log(LOG_ERROR, "%s: return from the dial thread. No line or device defined for channel %d\n", DEV_ID_LOG(d), c->callid);
		c->hangupok = 1;
		ast_mutex_unlock(&c->lock);
		if (chan)
			ast_hangup(chan);
		return NULL;
	}
	
	sccp_log(1)( VERBOSE_PREFIX_3 "%s: New call on line %s\n", DEV_ID_LOG(d), l->name);

	sccp_channel_set_callingparty(c, l->cid_name, l->cid_num);

	if (!ast_strlen_zero(c->dialedNumber)) {
		/* we have a number to dial. Let's do it */
		sccp_log(10)( VERBOSE_PREFIX_3 "%s: Dialing %s on channel %s-%d\n", DEV_ID_LOG(l->device), c->dialedNumber, l->name, c->callid);
		sccp_channel_set_calledparty(c, c->dialedNumber, c->dialedNumber);
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
		ast_mutex_unlock(&c->lock);
		goto dial;
	}

	/* we have to collect the number */
	/* the phone is on TsOffHook state */
	sccp_log(10)( VERBOSE_PREFIX_3 "%s: Waiting for the number to dial on channel %s-%d\n", DEV_ID_LOG(l->device), l->name, c->callid);
	/* let's use the keypad to collect digits */
	c->digittimeout = time(0)+GLOB(firstdigittimeout);
	ast_mutex_unlock(&c->lock);

	res_exten = 1;

	do {
		usleep(100);
		pthread_testcancel();
		ast_mutex_lock(&c->lock);
		if (!ast_strlen_zero(c->dialedNumber)) {
			/* It never occured to me what the purpose of the following line was. I modified it to avoid the limitation to
			   extensions not starting with '*' only. (-DD) 
			/res_exten = (c->dialedNumber[0] == '*' || ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num)); */
			res_exten = (ast_matchmore_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num));
		}
		if (! (res_wait = ( c->state == SCCP_CHANNELSTATE_DOWN || chan->_state == AST_STATE_DOWN
							|| chan->_softhangup || c->calltype == SKINNY_CALLTYPE_INBOUND)) ) {
			if (CS_AST_CHANNEL_PVT(chan)) {
				res_timeout = (time(0) < c->digittimeout);
			} else
				res_timeout = 0;
		}
		ast_mutex_unlock(&c->lock);
	} while ( (res_wait == 0) && res_exten &&  res_timeout);

	if (res_wait != 0) {
		/* AST_STATE_DOWN or softhangup */
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: return from the dial thread for DOWN, HANGUP or PICKUP cause\n", DEV_ID_LOG(l->device));
		ast_mutex_lock(&c->lock);
		c->hangupok = 1;
		ast_mutex_unlock(&c->lock);
		return NULL;
	}

dial:
	
	
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


	ast_mutex_lock(&c->lock);
	if(GLOB(recorddigittimeoutchar)) {
		if(strlen(c->dialedNumber) > 0 && strlen(c->dialedNumber) < 255 && '#' == c->dialedNumber[strlen(c->dialedNumber)-1])
			  {
				       strncpy(shortenedNumber, (const char*) c->dialedNumber, strlen(c->dialedNumber)-1);
				       sccp_copy_string(chan->exten, (const char*) shortenedNumber, sizeof(chan->exten));
				   }
				   else
					       sccp_copy_string(chan->exten, c->dialedNumber, sizeof(chan->exten));
	} else {
		sccp_copy_string(chan->exten, c->dialedNumber, sizeof(chan->exten));
	}
	sccp_copy_string(d->lastNumber, c->dialedNumber, sizeof(d->lastNumber));
	sccp_channel_set_calledparty(c, c->dialedNumber, c->dialedNumber);
	/* The 7961 seems to need the dialing callstate to record its directories information. */
 	sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
	/* proceed call state is needed to display the called number.
	The phone will not display callinfo in offhook state */
	sccp_channel_set_callstate(c, SKINNY_CALLSTATE_PROCEED);
	sccp_channel_send_callinfo(c);
	sccp_dev_clearprompt(d,c->line->instance, c->callid);
	sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
	c->hangupok = 1;
	ast_mutex_unlock(&c->lock);

	if ( !ast_strlen_zero(c->dialedNumber)
			&& ast_exists_extension(chan, chan->context, c->dialedNumber, 1, l->cid_num) ) {
		/* found an extension, let's dial it */
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: channel %s-%d is dialing number %s\n", DEV_ID_LOG(l->device), l->name, c->callid, c->dialedNumber);
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

	ast_mutex_lock(&c->lock);
	ast_queue_frame(c->owner, &f);
	ast_mutex_unlock(&c->lock);
}

void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]) {
	int i;
	struct ast_frame f = { AST_FRAME_DTMF, 0};
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending digits %s\n", DEV_ID_LOG(c->device), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	f.offset = 0;
	f.data = NULL;
	f.datalen = 0;
	ast_mutex_lock(&c->lock);
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass = digits[i];
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(c->device), digits[i]);
		ast_queue_frame(c->owner, &f);
	}
	ast_mutex_unlock(&c->lock);
}
