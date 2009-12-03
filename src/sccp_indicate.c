/*!
 * \file 	sccp_indicate.c
 * \brief 	SCCP Indicate Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * 
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_indicate.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_features.h"
#include "sccp_hint.h"

/*!
 * \brief Indicate Without Lock
 * \param device SCCP Device
 * \param c SCCP Channel
 * \param state State as uint8_t
 * \param debug Debug as uint8_t
 * \param file File as char
 * \param line Line Number as int
 * \param file Pretty Function as char
 * \param pretty_function
 */
void __sccp_indicate_nolock(sccp_device_t *device, sccp_channel_t * c, uint8_t state, uint8_t debug, char * file, int line, const char * pretty_function)
{
	sccp_device_t * d;
	sccp_line_t * l;
	int instance;

	if(debug)
		sccp_log(93)(VERBOSE_PREFIX_1 "SCCP: [INDICATE] mode '%s' in file '%s', on line %d (%s)\n", "UNLOCK", file, line, pretty_function);

	if (!c) {
		ast_log(LOG_ERROR, "SCCP: (sccp_indicate_nolock) No channel to indicate.\n");
		return;
	}
	d = (device)?device:c->device;
	if (!d) {
		sccp_log(93)(VERBOSE_PREFIX_1 "SCCP: The channel %d does not have a device\n",c->callid);
		return;
	}


	if (!c->line) {
		ast_log(LOG_ERROR, "SCCP: The channel %d does not have a line\n",c->callid);
		return;
	}

	l = c->line;
	instance = sccp_device_find_index_for_line(d, l->name);
	/* all the check are ok. We can safely run all the dev functions with no more checks */
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Indicate SCCP state (%s) old (%s) on call %s-%08x\n",d->id, sccp_indicate2str(state), sccp_indicate2str(c->state), l->name, c->callid);

	c->previousChannelState =c->state;
	c->state = state;

	switch (state) {
	case SCCP_CHANNELSTATE_DOWN:
//		sccp_ast_setstate(c, AST_STATE_DOWN);
		break;
	case SCCP_CHANNELSTATE_OFFHOOK:
		/* clear all the display buffers */
		// sccp_dev_cleardisplaynotify(d);
		// sccp_dev_clearprompt(d, 0, 0);
		sccp_dev_set_mwi(d, l, 0);
		if (!d->mwioncall)
			sccp_dev_set_mwi(d, NULL, 0);

		// sccp_dev_set_offhook(d);
		// sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, l->instance, c->callid);
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
//		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
		sccp_ast_setstate(c, AST_STATE_OFFHOOK);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_OFFHOOK);
		sccp_dev_set_cplane(l, d, 1);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
		sccp_dev_set_keyset(d, instance,c->callid, KEYMODE_OFFHOOK);

		sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, c->callid, 0);

		/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
		break;
	case SCCP_CHANNELSTATE_GETDIGITS:
		c->state = SCCP_CHANNELSTATE_OFFHOOK;
		/* clear all the display buffers */
		// sccp_dev_cleardisplaynotify(d);
		// sccp_dev_clearprompt(d, 0, 0);
		sccp_dev_set_mwi(d, l, 0);
		if (!d->mwioncall)
			sccp_dev_set_mwi(d, NULL, 0);

		// sccp_dev_set_offhook(d);
		// sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, l->instance, c->callid);
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
//		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
		sccp_ast_setstate(c, AST_STATE_OFFHOOK);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_OFFHOOK);

		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
		sccp_dev_set_keyset(d, instance,c->callid, KEYMODE_DIGITSFOLL);
		sccp_dev_set_cplane(l, d, 1);
		sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, instance, c->callid, 0);

		/* for earlyrtp take a look at sccp_feat_handle_callforward because we have no c->owner here */
		break;
	case SCCP_CHANNELSTATE_SPEEDDIAL:
		c->state = SCCP_CHANNELSTATE_OFFHOOK;
		/* clear all the display buffers */
		sccp_dev_cleardisplaynotify(d);
		sccp_dev_clearprompt(d, 0, 0);
		sccp_dev_set_mwi(d, l, 0);
		if (!d->mwioncall)
			sccp_dev_set_mwi(d, NULL, 0);

		// sccp_dev_set_offhook(d);
		sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, instance, c->callid);
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
//		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_OFFHOOK);

		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
		sccp_dev_set_keyset(d, instance,c->callid, KEYMODE_OFFHOOK);
		sccp_dev_set_cplane(l, d, 1);
//		sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, l->instance, c->callid, 0);
		sccp_ast_setstate(c, AST_STATE_OFFHOOK);
		/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
		break;
	case SCCP_CHANNELSTATE_ONHOOK:
		c->state = SCCP_CHANNELSTATE_DOWN;
		sccp_ast_setstate(c, AST_STATE_DOWN);
		if (c == d->active_channel)
			sccp_dev_stoptone(d, instance, c->callid);
		if (c->previousChannelState != SCCP_CHANNELSTATE_CALLWAITING)
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_OFF);


		sccp_dev_clearprompt(d, instance, c->callid);

		/* if channel was answered somewhere, set state to connected before onhook -> no missedCalls entry*/
		if(c->answered_elsewhere)
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);

		/* request channel hangup on remote device without answer*/
		if(!c->device && c->state == SCCP_CHANNELSTATE_RINGING){
			sccp_device_sendcallstate(device, instance, c->callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_ringer(device, SKINNY_STATION_RINGOFF, instance, c->callid);
			sccp_dev_set_lamp(device, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_OFF);
		}
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_ONHOOK);
		//sccp_channel_set_callstate_full(d, instance, c->callid, SKINNY_CALLSTATE_ONHOOK);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOOK);


//		sccp_handle_time_date_req(d->session, NULL);
		if (c == d->active_channel)
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
		
		if (c->previousChannelState == SCCP_CHANNELSTATE_RINGING)
			sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, instance, c->callid);

		break;
	case SCCP_CHANNELSTATE_RINGOUT:
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_RINGOUT);
		sccp_channel_send_callinfo(d, c);
		if (d->earlyrtp == SCCP_CHANNELSTATE_RINGOUT && !c->rtp) {
			sccp_channel_openreceivechannel(c);
		}
		/* it will be emulated if the rtp audio stream is open */
		if (!c->rtp)
			sccp_dev_starttone(d, SKINNY_TONE_ALERTINGTONE, instance, c->callid, 0);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGOUT);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_RING_OUT, 0);
		sccp_ast_setstate(c, AST_STATE_RING);
		break;
	case SCCP_CHANNELSTATE_RINGING:
		/* clear all the display buffers */
		sccp_dev_cleardisplaynotify(d);
		sccp_dev_clearprompt(d, instance, 0);
		sccp_dev_set_mwi(d, l, 0);
		if (!d->mwioncall)
			sccp_dev_set_mwi(d, NULL, 0);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_RINGIN);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_BLINK);

		if ((d->dnd && d->dndmode == SCCP_DNDMODE_SILENT) || (d->dndmode == SCCP_DNDMODE_USERDEFINED && d->dnd == SCCP_DNDMODE_SILENT ) ) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: DND is activated on device(%d/%d)\n",d->id, d->dnd, d->dndmode);
			sccp_dev_set_ringer(d, SKINNY_STATION_SILENTRING, instance, c->callid);
		}else
			sccp_dev_set_ringer(d, c->ringermode, instance, c->callid);

		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);
		sccp_dev_displayprompt(d, instance, c->callid, "Incoming Call", 0);
		sccp_ast_setstate(c, AST_STATE_RINGING);
		break;
	case SCCP_CHANNELSTATE_CONNECTED:
		sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, instance, c->callid);
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
//		if (c->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_dev_stoptone(d, instance, c->callid);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_CONNECTED);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_set_cplane(l, d, 1);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_CONNECTED);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CONNECTED, 0);
		// if no rtp or was in old openreceivechannel (note that rtp doens't reinitialize as channel was in hold state or offhook state due to a transfer abort)
		if (!c->rtp
				|| c->previousChannelState == SCCP_CHANNELSTATE_HOLD
				|| c->previousChannelState == SCCP_CHANNELSTATE_CALLTRANSFER
				|| c->previousChannelState == SCCP_CHANNELSTATE_CALLCONFERENCE
				|| c->previousChannelState == SCCP_CHANNELSTATE_OFFHOOK) {
			sccp_channel_openreceivechannel(c);
		} else if(c->rtp) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: (for debug purposes) did not reopen an RTP stream as old SCCP state was (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
		}
		/* asterisk wants rtp open before AST_STATE_UP
		 * so we set it in OPEN_CHANNEL_ACK in sccp_actions.c.
		 */
		if(d->earlyrtp)
			sccp_ast_setstate(c, AST_STATE_UP);
		break;
	case SCCP_CHANNELSTATE_BUSY:
		/* it will be emulated if the rtp audio stream is open */
		if (!c->rtp)
			sccp_dev_starttone(d, SKINNY_TONE_LINEBUSYTONE, instance, c->callid, 0);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_BUSY, 0);
		sccp_ast_setstate(c, AST_STATE_BUSY);
		break;
	case SCCP_CHANNELSTATE_PROCEED:
		if(c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) { // this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app)
			sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
			c->state = SCCP_CHANNELSTATE_CONNECTED;
			// sccp_feat_updatecid(c);
			return;
		}
		sccp_dev_stoptone(d, instance, c->callid);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_PROCEED);
		// sccp_dev_set_keyset(d, l->instance, c->callid, KEYMODE_CONNECTED);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
		if (!c->rtp) {
			sccp_channel_openreceivechannel(c);
		}
		break;
	case SCCP_CHANNELSTATE_HOLD:
		sccp_channel_closereceivechannel(c);
		sccp_handle_time_date_req(d->session, NULL);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_WINK);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_HOLD);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOLD);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_HOLD, 0);
//		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
		break;
	case SCCP_CHANNELSTATE_CONGESTION:
		/* it will be emulated if the rtp audio stream is open */
		if (!c->rtp)
			sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
		/* In fact, newer firmware versions (the 8 releases for the 7960 etc.) and
		   the newer Cisco phone models don't seem to like this at all, resulting in
		   crashes. Federico observed that also congestion is affected. We have to find a
		   signalling replacement for the display promptif this is neccessary for some reason.(-DD)*/
//		sccp_channel_set_callstate(c, SKINNY_CALLSTATE_CONGESTION);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TEMP_FAIL, 0);
		break;
	case SCCP_CHANNELSTATE_CALLWAITING:
		if (GLOB(callwaiting_tone))
			sccp_dev_starttone(d, GLOB(callwaiting_tone), instance, c->callid, 0);
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_RINGIN);
		sccp_channel_send_callinfo(d, c);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_WAITING, 0);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);
		sccp_ast_setstate(c, AST_STATE_RINGING);
		break;
	case SCCP_CHANNELSTATE_CALLTRANSFER:
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TRANSFER, 0);
		/* sccp_channel_set_callstate(c, SKINNY_CALLSTATE_CALLTRANSFER); */
		break;
	case SCCP_CHANNELSTATE_CALLCONFERENCE:
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CONFERENCE, 0);
		/* sccp_channel_set_callstate(c, SKINNY_CALLSTATE_CALLTRANSFER); */
		break;
	case SCCP_CHANNELSTATE_CALLPARK:
		sccp_channel_set_callstate(d, c, SKINNY_CALLSTATE_CALLPARK);
		break;
	case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
		sccp_dev_set_ringer(d, SKINNY_STATION_RINGOFF, instance, c->callid);
		sccp_dev_clearprompt(d, instance, c->callid);
		sccp_device_sendcallstate(d, instance,c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT); /* send connected, so it is not listed as missed call*/
		sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOOK);
		break;
	case SCCP_CHANNELSTATE_INVALIDNUMBER:
		/* this is for the earlyrtp. The 7910 does not play tones if a rtp stream is open */
		if (c->rtp)
			sccp_channel_closereceivechannel(c);

		sccp_safe_sleep(100);
		sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
		/* 7936 does not like the skinny ivalid message callstate */
		/* In fact, newer firmware versions (the 8 releases for the 7960 etc.) and
		   the newer Cisco phone models don't seem to like this at all, resulting in
		   crashes. Interestingly, the message imho does cause obvious effects anyway. (-DD)*/
		/*if (d->skinny_type != SKINNY_DEVICETYPE_CISCO7936)
			sccp_channel_set_callstate(c, SKINNY_CALLSTATE_INVALIDNUMBER); */
		sccp_channel_send_callinfo(d, c);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_UNKNOWN_NUMBER, 0);

//ast_hangup(c->owner);
		/* don't set AST_STATE_DOWN. we hangup only on onhook and endcall softkey */
		break;
	case SCCP_CHANNELSTATE_DIALING:
		sccp_dev_stoptone(d, instance, c->callid);
		sccp_channel_send_dialednumber(c);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
		// sccp_dev_clearprompt(d,l->instance, c->callid);
		if (d->earlyrtp == SCCP_CHANNELSTATE_DIALING && !c->rtp) {
			sccp_channel_openreceivechannel(c);
		}
		sccp_ast_setstate(c, AST_STATE_DIALING);
		break;
	case SCCP_CHANNELSTATE_DIGITSFOLL:
		//sccp_dev_stoptone(d, l->instance, c->callid);
		sccp_channel_send_dialednumber(c);
		sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
		sccp_ast_setstate(c, AST_STATE_DIALING);
		break;
	}

	/* notify all remote devices */
	__sccp_indicate_remote_device(device, c, state, debug, file, line,pretty_function);

	/* notify features */
	sccp_feat_channelStateChanged(device, c);

	/* notify state change to hint system (incl. asterisk ) */
	sccp_hint_lineStatusChanged(l, d, c, c->previousChannelState, c->state);
	sccp_device_stateChanged(d);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Finish to indicate state SCCP (%s), SKINNY (%s) on call %s-%08x\n",d->id, sccp_indicate2str(state), sccp_callstate2str(c->callstate), l->name, c->callid);
}


/*!
 * \brief Indicate to Remote Device
 * \param device SCCP Device
 * \param c SCCP Channel
 * \param state State as int
 * \param debug Debug as int
 * \param file File as char
 * \param line Line as int 
 * \param pretty_function Pretty Function as char
 * \todo Explain Pretty Function
 */
void __sccp_indicate_remote_device(sccp_device_t *device, sccp_channel_t * c, uint8_t state, uint8_t debug, char * file, int line, const char * pretty_function)
{
	sccp_device_t	*remoteDevice;
	int instance;

	if(!c || !c->line)
		return;




//	SCCP_LIST_LOCK(&c->line->devices);
	//TODO find working lock
	sccp_linedevices_t *linedevice;
	SCCP_LIST_TRAVERSE(&c->line->devices, linedevice, list) {
			if(!linedevice->device)
				continue;
			remoteDevice=linedevice->device;

			if(device && remoteDevice == device)
				continue;

			

			sccp_log(64)(VERBOSE_PREFIX_3 "%s: Notify remote device.\n", DEV_ID_LOG(remoteDevice));
			sccp_log(64)(VERBOSE_PREFIX_3 "%s: channelcount: %d\n", DEV_ID_LOG(remoteDevice), c->line->channelCount);

			instance = sccp_device_find_index_for_line(remoteDevice, c->line->name);
			
			switch (state) {
				case SCCP_CHANNELSTATE_DOWN:
				break;
				case SCCP_CHANNELSTATE_OFFHOOK:
					if(c->previousChannelState == SCCP_CHANNELSTATE_RINGING){
						sccp_dev_set_ringer(remoteDevice, SKINNY_STATION_RINGOFF, instance, c->callid);
						sccp_device_sendcallstate(remoteDevice, instance,c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, (!c->private)?SKINNY_CALLINFO_VISIBILITY_DEFAULT:SKINNY_CALLINFO_VISIBILITY_HIDDEN); /* send connected, so it is not listed as missed call*/
					}
					break;
				case SCCP_CHANNELSTATE_GETDIGITS:

					break;
				case SCCP_CHANNELSTATE_SPEEDDIAL:

					break;
				case SCCP_CHANNELSTATE_ONHOOK:
					if(c->previousChannelState == SCCP_CHANNELSTATE_RINGING)
							sccp_dev_set_ringer(remoteDevice, SKINNY_STATION_RINGOFF, instance, c->callid);

					/* if channel was answered somewhere, set state to connected before onhook -> no missedCalls entry*/
					if(c->answered_elsewhere)
						sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);

					sccp_dev_clearprompt(remoteDevice, instance, c->callid);
					sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_NORMAL, (!c->private)?SKINNY_CALLINFO_VISIBILITY_DEFAULT:SKINNY_CALLINFO_VISIBILITY_HIDDEN);

					break;
				case SCCP_CHANNELSTATE_RINGOUT:

					break;
				case SCCP_CHANNELSTATE_RINGING:

					break;
				case SCCP_CHANNELSTATE_CONNECTED:
					if(c->previousChannelState == SCCP_CHANNELSTATE_RINGING){
						sccp_dev_set_ringer(remoteDevice, SKINNY_STATION_RINGOFF, instance, c->callid);
						sccp_device_sendcallstate(remoteDevice, instance,c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, (!c->private)?SKINNY_CALLINFO_VISIBILITY_DEFAULT:SKINNY_CALLINFO_VISIBILITY_HIDDEN); /* send connected, so it is not listed as missed call*/
					}
					sccp_dev_clearprompt(remoteDevice, instance, c->callid);
					sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE, SKINNY_CALLPRIORITY_LOW, (!c->private)?SKINNY_CALLINFO_VISIBILITY_DEFAULT:SKINNY_CALLINFO_VISIBILITY_HIDDEN);
					sccp_channel_send_callinfo(remoteDevice, c);
					//sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CONNECTED, 0);
					sccp_dev_set_keyset(remoteDevice, instance, c->callid, KEYMODE_ONHOOK);
// 					sccp_dev_set_keyset(remoteDevice, instance, c->callid, KEYMODE_ONHOOK);
					break;
				case SCCP_CHANNELSTATE_BUSY:

					break;
				case SCCP_CHANNELSTATE_PROCEED:

					break;
				case SCCP_CHANNELSTATE_HOLD:
					sccp_dev_set_lamp(remoteDevice, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_FLASH);
					sccp_device_sendcallstate(remoteDevice, instance,c->callid, SKINNY_CALLSTATE_HOLDRED, SKINNY_CALLPRIORITY_NORMAL, (!c->private)?SKINNY_CALLINFO_VISIBILITY_DEFAULT:SKINNY_CALLINFO_VISIBILITY_HIDDEN);
					sccp_dev_set_keyset(remoteDevice, instance, c->callid, KEYMODE_ONHOLD);
					sccp_dev_displayprompt(remoteDevice, instance, c->callid, SKINNY_DISP_HOLD, 0);
					sccp_channel_send_callinfo(remoteDevice, c);
					break;;
				case SCCP_CHANNELSTATE_CONGESTION:

					break;
				case SCCP_CHANNELSTATE_CALLWAITING:

					break;
				case SCCP_CHANNELSTATE_CALLTRANSFER:

					break;
				case SCCP_CHANNELSTATE_CALLCONFERENCE:

					break;
				case SCCP_CHANNELSTATE_CALLPARK:

					break;
				case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:

					break;
				case SCCP_CHANNELSTATE_INVALIDNUMBER:

					break;
				case SCCP_CHANNELSTATE_DIALING:

					break;
				case SCCP_CHANNELSTATE_DIGITSFOLL:
					break;
			}
	}
//	SCCP_LIST_UNLOCK(&c->line->devices);
}





/*!
 * \brief Convert SCCP Indication/ChannelState 2 String
 * \param state SCCP_CHANNELSTATE_*
 * \return Channel State String
 */
const char * sccp_indicate2str(uint8_t state)
{
		switch(state) {
		case SCCP_CHANNELSTATE_DOWN:
				return "Down";
		case SCCP_CHANNELSTATE_OFFHOOK:
				return "OffHook";
		case SCCP_CHANNELSTATE_ONHOOK:
				return "OnHook";
		case SCCP_CHANNELSTATE_RINGOUT:
				return "RingOut";
		case SCCP_CHANNELSTATE_RINGING:
				return "Ringing";
		case SCCP_CHANNELSTATE_CONNECTED:
				return "Connected";
		case SCCP_CHANNELSTATE_BUSY:
				return "Busy";
		case SCCP_CHANNELSTATE_PROCEED:
				return "Proceed";
		case SCCP_CHANNELSTATE_CONGESTION:
				return "Congestion";
		case SCCP_CHANNELSTATE_HOLD:
				return "Hold";
		case SCCP_CHANNELSTATE_CALLWAITING:
				return "CallWaiting";
		case SCCP_CHANNELSTATE_CALLTRANSFER:
				return "CallTransfer";
		case SCCP_CHANNELSTATE_CALLPARK:
				return "CallPark";
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
				return "CallRemoteMultiline";
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
				return "InvalidNumber";
		case SCCP_CHANNELSTATE_DIALING:
				return "Dialing";
		case SCCP_CHANNELSTATE_DIGITSFOLL:
				return "DigitsFoll";
		case SCCP_CHANNELSTATE_SPEEDDIAL:
				return "SpeedDial";
		default:
				return "Unknown";
		}
}

/*!
 * \brief Convert SKINNYCall State 2 String
 * \param state SKINNY_CALLSTATE_*
 * \return Call State String
 */
const char * sccp_callstate2str(uint8_t state)
{
		switch(state) {
		case SKINNY_CALLSTATE_OFFHOOK:
				return "OffHook";
		case SKINNY_CALLSTATE_ONHOOK:
				return "OnHook";
		case SKINNY_CALLSTATE_RINGOUT:
				return "RingOut";
		case SKINNY_CALLSTATE_RINGIN:
				return "Ringing";
		case SKINNY_CALLSTATE_CONNECTED:
				return "Connected";
		case SKINNY_CALLSTATE_BUSY:
				return "Busy";
		case SKINNY_CALLSTATE_PROCEED:
				return "Proceed";
		case SKINNY_CALLSTATE_CONGESTION:
				return "Congestion";
		case SKINNY_CALLSTATE_HOLD:
				return "Hold";
		case SKINNY_CALLSTATE_CALLWAITING:
				return "CallWaiting";
		case SKINNY_CALLSTATE_CALLTRANSFER:
				return "CallTransfer";
		case SKINNY_CALLSTATE_CALLPARK:
				return "CallPark";
		case SKINNY_CALLSTATE_CALLREMOTEMULTILINE:
				return "CallRemoteMultiline";
		case SKINNY_CALLSTATE_INVALIDNUMBER:
				return "InvalidNumber";
		default:
				return "Unknown";
		}
}

