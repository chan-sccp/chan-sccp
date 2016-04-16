/*!
 * \file        sccp_indicate.c
 * \brief       SCCP Indicate Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_conference.h"
#include "sccp_actions.h"
#include "sccp_device.h"
#include "sccp_indicate.h"
#include "sccp_line.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

static void __sccp_indicate_remote_device(const sccp_device_t * const device, const sccp_channel_t * const c, const sccp_line_t * const line, const sccp_channelstate_t state);

/*!
 * \brief Indicate Without Lock
 * \param device SCCP Device
 * \param c *locked* SCCP Channel
 * \param state State as uint8_t
 * \param debug Debug as uint8_t
 * \param file File as char
 * \param line Line Number as int
 * \param file Pretty Function as char
 * \param pretty_function
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 *  - line->devices is not always locked
 * 
 */
//void __sccp_indicate(sccp_device_t * device, sccp_channel_t * c, uint8_t state, uint8_t debug, char *file, int line, const char *pretty_function)
void __sccp_indicate(const sccp_device_t * const device, sccp_channel_t * const c, const sccp_channelstate_t state, const uint8_t debug, const char *file, const int line, const char *pretty_function)
{
	int instance = 0;

	if (debug) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: [INDICATE] state '%d' in file '%s', on line %d (%s)\n", state, file, line, pretty_function);
	}

	AUTO_RELEASE sccp_device_t *d = (device) ? sccp_device_retain(device) : sccp_channel_getDevice(c);

	if (!d) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: The channel %d does not have a device\n", c->callid);
		return;
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_retain(c->line);

	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: The channel %d does not have a line\n", c->callid);
		return;
	}

	AUTO_RELEASE sccp_linedevices_t *linedevice = NULL;

	if ((linedevice = sccp_linedevice_find(d, l))) {
		instance = linedevice->lineInstance;
	} else {
		pbx_log(LOG_ERROR, "%s: The linedevice/instance for device %s and line %s belonging to channel %d could not be found. Skipping indicate.\n", DEV_ID_LOG(d), DEV_ID_LOG(d), l->name, c->callid);
		return;
	}

	/* all the check are ok. We can safely run all the dev functions with no more checks */
	sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Indicate SCCP new state %d (%s), current channel state %d (%s) on call %s (previous channelstate %d (%s))\n", d->id, state, sccp_channelstate2str(state), c->state, sccp_channelstate2str(c->state), c->designator, c->previousChannelState, sccp_channelstate2str(c->previousChannelState));
	sccp_channel_setChannelstate(c, state);
	sccp_callinfo_t * const ci = sccp_channel_getCallInfo(c);

	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
			//iPbx.set_callstate(c, AST_STATE_DOWN);
			break;
		case SCCP_CHANNELSTATE_OFFHOOK:
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
			sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_cplane(d, instance, 1);
			if (SCCP_CHANNELSTATE_DOWN == c->previousChannelState) {				// new call
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
				if (d->earlyrtp != SCCP_EARLYRTP_IMMEDIATE) {
					sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, c->callid, SKINNY_TONEDIRECTION_USER);
				}
			}
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_OFFHOOK);
			/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
			break;
		case SCCP_CHANNELSTATE_GETDIGITS:
			c->state = SCCP_CHANNELSTATE_OFFHOOK;
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
			sccp_dev_set_cplane(d, instance, 1);
			sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, instance, c->callid, SKINNY_TONEDIRECTION_USER);
			/* for earlyrtp take a look at sccp_feat_handle_callforward because we have no c->owner here */
			break;
		case SCCP_CHANNELSTATE_SPEEDDIAL:
			c->state = SCCP_CHANNELSTATE_OFFHOOK;
			/* clear all the display buffers */
			sccp_dev_cleardisplaynotify(d);
			sccp_dev_clearprompt(d, 0, 0);

			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_OFFHOOK);
			sccp_dev_set_cplane(d, instance, 1);
			/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
			break;
		case SCCP_CHANNELSTATE_ONHOOK:
			c->state = SCCP_CHANNELSTATE_DOWN;
			sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_OFF);
			if (c->answered_elsewhere) {
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
			}
			if (d->indicate && d->indicate->onhook) {
				d->indicate->onhook(d, instance, c->callid);
			}
			break;
		case SCCP_CHANNELSTATE_RINGOUT:
			{
				/*
				fixes a problem with remembering dialed numbers in case of overlap dialing onto the trunks. 
				It adds sending the DialedNumber message also to a situation, when a PROCEEDING signal is received
				from the B-side (the trunk). Many public trunk types (SS7, ISDN etc) don't
				always send RINGOUT, sometimes they send only PROCEEDING. In such a case, the
				complete number was never set to the phone and it remembered only a part of
				the number or possibly even nothing. We are already sending DialedNumber
				message upon receiving of the RINGOUT state, so it's just extension to do
				the same for another state and I hope it won't break anything. BTW the primary
				meaning of the PROCEEDING signal is "number complete, attempting to make the
				connection", so it's a really good place to send the number back to the
				caller. -Pavel Troller
				*/ 
				if( !sccp_strequals(c->dialedNumber, "s") ){
					//d->protocol->sendDialedNumber(d, c);
					d->protocol->sendDialedNumber(d, instance, c->callid, c->dialedNumber);
				}
				iCallInfo.Send(ci, c->callid, c->calltype, instance, device, d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE ? TRUE : FALSE);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			}

			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGOUT, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_RING_OUT, GLOB(digittimeout));

			if (d->earlyrtp <= SCCP_EARLYRTP_RINGOUT && c->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE) {
				sccp_channel_openReceiveChannel(c);
			}
			if (c->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE) {				/* send tone if ther is no rtp for inband signaling */
				sccp_dev_starttone(d, (uint8_t) SKINNY_TONE_ALERTINGTONE, instance, c->callid, SKINNY_TONEDIRECTION_USER);
			}
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGOUT);
			break;
		case SCCP_CHANNELSTATE_RINGING:
			sccp_dev_cleardisplaynotify(d);
			sccp_dev_clearprompt(d, instance, 0);

			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			iCallInfo.Send(ci, c->callid, c->calltype, instance, device, TRUE);

			sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_BLINK);

			if ((d->dndFeature.enabled && d->dndFeature.status == SCCP_DNDMODE_SILENT && c->ringermode != SKINNY_RINGTYPE_URGENT)) {
				sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: DND is activated on device\n", d->id);
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
			} else {
				sccp_linedevices_t *ownlinedevice = NULL;
				sccp_device_t *remoteDevice = NULL;

				SCCP_LIST_TRAVERSE(&l->devices, ownlinedevice, list) {
					remoteDevice = ownlinedevice->device;

					if (d && remoteDevice && remoteDevice == d) {
						sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found matching linedevice. Aux parameter = %s\n", d->id, ownlinedevice->subscriptionId.aux);

						/** Check the auxiliary parameter of the linedevice to enable silent ringing for certain devices on a certain line.**/
						if (0 == strncmp(ownlinedevice->subscriptionId.aux, "silent", 6)) {
							sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
							sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Forcing silent ring for specific device.\n", d->id);
						} else {
							sccp_dev_set_ringer(d, c->ringermode, instance, c->callid);
							sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Normal ring occurred.\n", d->id);
						}
					}
				}
			}

			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);
			char prompt[100];

			char orig_called_name[StationMaxNameSize] = {0};
			char orig_called_num[StationMaxDirnumSize] = {0};
			char calling_name[StationMaxNameSize] = {0};
			char calling_num[StationMaxDirnumSize] = {0};
			iCallInfo.Getter(sccp_channel_getCallInfo(c), 
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &orig_called_name,
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &orig_called_num,
				SCCP_CALLINFO_CALLINGPARTY_NAME, &calling_name, 
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, &calling_num,
				SCCP_CALLINFO_KEY_SENTINEL);
			snprintf(prompt, sizeof(prompt), "%s%s: %s", 
				(c->ringermode == SKINNY_RINGTYPE_URGENT) ? SKINNY_DISP_FLASH : "", 
				!sccp_strlen_zero(orig_called_name) ? orig_called_name : (!sccp_strlen_zero(orig_called_num) ? orig_called_num : SKINNY_DISP_FROM), 
				!sccp_strlen_zero(calling_name) ? calling_name : calling_num);
			sccp_dev_displayprompt(d, instance, c->callid, prompt, GLOB(digittimeout));
			/*
			if (c->ringermode) {
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_FLASH, GLOB(digittimeout));
			}
			*/

// 			iPbx.set_callstate(c, AST_STATE_RINGING);						/*!\todo thats not the right place to update pbx state */
			break;
		case SCCP_CHANNELSTATE_CONNECTED:
			d->indicate->connected(d, instance, c->callid, c->calltype, ci);
			if (!c->rtp.audio.instance || c->previousChannelState == SCCP_CHANNELSTATE_HOLD || c->previousChannelState == SCCP_CHANNELSTATE_CALLTRANSFER || c->previousChannelState == SCCP_CHANNELSTATE_CALLCONFERENCE || c->previousChannelState == SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_channel_openReceiveChannel(c);
			} else if (c->rtp.audio.instance) {
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Did not reopen an RTP stream as old SCCP state was (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			}
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_CONNECTED);
			break;
		case SCCP_CHANNELSTATE_BUSY:
			if (c->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE) {
				sccp_dev_starttone(d, SKINNY_TONE_LINEBUSYTONE, instance, c->callid, SKINNY_TONEDIRECTION_USER);
			}
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_BUSY, GLOB(digittimeout));
			break;
		case SCCP_CHANNELSTATE_PROGRESS:								/* \todo SCCP_CHANNELSTATE_PROGRESS To be checked */
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_2 "%s: SCCP_CHANNELSTATE_PROGRESS\n", d->id);
			if (c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) {

				/** this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app) */
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
			} else {
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) from (%s)\n", sccp_channelstate2str(c->previousChannelState));
				if (!c->rtp.audio.instance && d->earlyrtp <= SCCP_EARLYRTP_PROGRESS) {
					sccp_channel_openReceiveChannel(c);
				}
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROGRESS, GLOB(digittimeout));
			}
			break;
		case SCCP_CHANNELSTATE_PROCEED:
			if (c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) {				// this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app)
				sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
				break;
			}
			sccp_dev_stoptone(d, instance, c->callid);
			
			/* nessesary for overlap dialing */
			{
				/* suppresses sending of the DialedNumber message in the case, when the number is just "s" (initial dial string in immeediate mode) -Pavel Troller */
				if( !sccp_strequals(c->dialedNumber, "s") ){
					d->protocol->sendDialedNumber(d, instance, c->callid, c->dialedNumber);
				}
				iCallInfo.Send(ci, c->callid, c->calltype, instance, device, d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE ? TRUE : FALSE);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			}
			/* done */
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, GLOB(digittimeout));
			if (!c->rtp.audio.instance && d->earlyrtp <= SCCP_EARLYRTP_RINGOUT) {
				sccp_channel_openReceiveChannel(c);
			}
			break;
		case SCCP_CHANNELSTATE_HOLD:
			if (c->rtp.audio.writeState != SCCP_RTP_STATUS_INACTIVE) {
				sccp_channel_closeReceiveChannel(c, TRUE);
			}
			if (c->rtp.video.writeState != SCCP_RTP_STATUS_INACTIVE) {
				sccp_channel_closeMultiMediaReceiveChannel(c, TRUE);
			}
			sccp_handle_time_date_req(d->session, d, NULL);
			sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_WINK);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_HOLD, GLOB(digittimeout));
			iCallInfo.Send(ci, c->callid, c->calltype, instance, device, TRUE);
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
#if CS_SCCP_CONFERENCE
			if (c->conference && d->conference) {
				sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_HOLDCONF);
			} else 
#endif
			{
				sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOLD);
			}
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
			/* it will be emulated if the rtp audio stream is open */
			if (c->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE) {
				sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, SKINNY_TONEDIRECTION_USER);
			}
			iCallInfo.Send(ci, c->callid, c->calltype, instance, device, d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE ? TRUE : FALSE);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TEMP_FAIL, GLOB(digittimeout));
			// wait 15 seconds, then hangup automatically
			sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
			break;
		case SCCP_CHANNELSTATE_CALLWAITING:
			{
				{
					/* When dialing a shared line which you also have registered, we don't want to outgoing call to show up on our own device as a callwaiting call */
					AUTO_RELEASE sccp_channel_t *activeChannel = sccp_device_getActiveChannel(d);

					if (activeChannel && (sccp_strequals(iPbx.getChannelLinkedId(activeChannel), iPbx.getChannelLinkedId(c)))) {
						sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: (SCCP_CHANNELSTATE_CALLWAITING) Already Own Part of the Call: Skipping\n", DEV_ID_LOG(d));
						sccp_log_and(DEBUGCAT_INDICATE + DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "%s: LinkedId: %s / %s: LinkedId Remote: %s\n", DEV_ID_LOG(d), iPbx.getChannelLinkedId(c), DEV_ID_LOG(d), iPbx.getChannelLinkedId(activeChannel));
						break;
					}
				}
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_CALLWAITING (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->previousChannelState));
				sccp_channel_callwaiting_tone_interval(d, c);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				iCallInfo.Send(ci, c->callid, c->calltype, instance, device, TRUE);
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_WAITING, GLOB(digittimeout));
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
				sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);

#ifdef CS_SCCP_CONFERENCE
				if (d->conferencelist_active) {
					sccp_conference_hide_list_ByDevice(d);
				}
#endif
			}
			break;
		case SCCP_CHANNELSTATE_CALLTRANSFER:
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TRANSFER, GLOB(digittimeout));
			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CALLTRANSFER, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			iCallInfo.Send(ci, c->callid, c->calltype, instance, device, d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE ? TRUE : FALSE);
			break;
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
			// sccp_device_sendcallstate(d, instance, c->callid, SCCP_CHANNELSTATE_CALLCONFERENCE, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			break;
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			/*! \todo SCCP_CHANNELSTATE_INVALIDCONFERENCE To be implemented */
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_INVALIDCONFERENCE (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			break;
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_CONNECTEDCONFERENCE (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			d->indicate->connected(d, instance, c->callid, c->calltype, ci);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_CONNCONF);

			if (!c->rtp.audio.instance || c->previousChannelState == SCCP_CHANNELSTATE_HOLD || c->previousChannelState == SCCP_CHANNELSTATE_CALLTRANSFER || c->previousChannelState == SCCP_CHANNELSTATE_CALLCONFERENCE || c->previousChannelState == SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_channel_openReceiveChannel(c);
			} else if (c->rtp.audio.instance) {
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Did not reopen an RTP stream as old SCCP state was (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			}
			break;
			break;
		case SCCP_CHANNELSTATE_CALLPARK:
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CALLPARK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			break;
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_dev_clearprompt(d, instance, c->callid);

			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/** send connected, so it is not listed as missed call */
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOOKSTEALABLE);
			break;
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
			/* this is for the earlyrtp. The 7910 does not play tones if a rtp stream is open */
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_UNKNOWN_NUMBER, GLOB(digittimeout));
			sccp_channel_closeAllMediaTransmitAndReceive(d, c);
			sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, SKINNY_TONEDIRECTION_USER);
			// wait 15 seconds, then hangup automatically
			sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
			break;
		case SCCP_CHANNELSTATE_DIALING:
			//d->indicate->dialing(d, instance, c);
			d->indicate->dialing(d, instance, c->callid, c->calltype, ci, c->dialedNumber);
			if (d->earlyrtp <= SCCP_EARLYRTP_DIALING && !c->rtp.audio.instance) {
				sccp_channel_openReceiveChannel(c);
			}
			break;
		case SCCP_CHANNELSTATE_DIGITSFOLL:
			{
				int lenDialed = 0, lenSecDialtoneDigits = 0;
				uint32_t secondary_dialtone_tone = 0;

				lenDialed = sccp_strlen(c->dialedNumber);

				/* secondary dialtone check */
				lenSecDialtoneDigits = sccp_strlen(l->secondary_dialtone_digits);
				secondary_dialtone_tone = l->secondary_dialtone_tone;

				if (lenSecDialtoneDigits > 0 && lenDialed == lenSecDialtoneDigits && !strncmp(c->dialedNumber, l->secondary_dialtone_digits, lenSecDialtoneDigits)) {
					/* We have a secondary dialtone */
					sccp_dev_starttone(d, secondary_dialtone_tone, instance, c->callid, SKINNY_TONEDIRECTION_USER);
				} else if (lenDialed > 0) {
					sccp_dev_stoptone(d, instance, c->callid);
				}
			}

			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
			break;
		case SCCP_CHANNELSTATE_BLINDTRANSFER:								/* \todo SCCP_CHANNELSTATE_BLINDTRANSFER To be implemented */
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_BLINDTRANSFER (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			break;
		default:											/* \todo SCCP_CHANNELSTATE:default To be implemented */
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE:default  %s (%d) -> %s (%d)\n", d->id, sccp_channelstate2str(c->previousChannelState), c->previousChannelState, sccp_channelstate2str(c->state), c->state);
			break;
	}

	/* if channel state has changed, notify the others */
	if (c->state != c->previousChannelState) {
		/* if it is a shared line and a state of interest */
		if ((SCCP_RWLIST_GETSIZE(&l->devices) > 1) && (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DOWN || c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_HOLD || c->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE) && !c->conference) {
			/* notify all remote devices */
			__sccp_indicate_remote_device(d, c, l, state);
		}

		/* notify features (sccp_feat_channelstateChanged = empty function, skipping) */
		//sccp_feat_channelstateChanged(d, c);

		sccp_event_t event = {{{0}}};
		event.type = SCCP_EVENT_LINESTATUS_CHANGED;
		event.event.lineStatusChanged.line = sccp_line_retain(l);
		event.event.lineStatusChanged.optional_device = sccp_device_retain(d);
		event.event.lineStatusChanged.state = c->state;
		sccp_event_fire(&event);
	}

	sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Finish to indicate state SCCP (%s) on call %s. New state on channel: %s (%d)\n", d->id, sccp_channelstate2str(state), c->designator, sccp_channelstate2str(c->state), c->state);
	//sccp_do_backtrace();
}

/*!
 * \brief Indicate to Remote Device
 * \param device SCCP Device
 * \param c SCCP Channel
 * \param line SCCP Line
 * \param state State as int
 * 
 * \warning
 *  - line->devices is not always locked
 */
static void __sccp_indicate_remote_device(const sccp_device_t * const device, const sccp_channel_t * const c, const sccp_line_t * const line, const sccp_channelstate_t state)
{
	int lineInstance = 0;
	sccp_phonebook_t __attribute__((unused)) phonebookRecord = SCCP_PHONEBOOK_NONE;

	if (!c || !line) {
		return;
	}

	/** \todo move this to channel->privacy */
	//uint32_t privacyStatus=0;
	//if (sccp_channel_getDevice(c)) {
	//privacyStatus = sccp_channel_getDevic(c)->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;
	//}
	///* do not display private lines */
	//if (state !=SCCP_CHANNELSTATE_CONNECTED && (c->privacy || privacyStatus > 0) ){
	//sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "privacyStatus status is set, ignore remote devices\n");
	//return;
	//}

	/* do not propagate status of hotline */
	if (line == GLOB(hotline)->line) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: (__sccp_indicate_remote_device) I'm a hotline, do not notify me!\n");
		return;
	}
	sccp_linedevices_t *linedevice = NULL;

	/* copy temp variables, information to be send to remote device (in another thread) */
	const uint32_t callid = c->callid;
	const skinny_calltype_t calltype = c->calltype;
	char dialedNumber[SCCP_MAX_EXTENSION];
	sccp_copy_string(dialedNumber, c->dialedNumber, SCCP_MAX_EXTENSION);
	sccp_callinfo_t * ci = iCallInfo.CopyConstructor(sccp_channel_getCallInfo(c));

	sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Remote Indicate state %s (%d) with reason: %s (%d) on remote devices for channel %s\n", DEV_ID_LOG(device), sccp_channelstate2str(state), state, sccp_channelstatereason2str(c->channelStateReason), c->channelStateReason, c->designator);
	SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
		if (!linedevice->device) {
			pbx_log(LOG_NOTICE, "Strange to find a linedevice (%p) here without a valid device connected to it !", linedevice);
			continue;
		}

		if (linedevice->device == device) {
			// skip self
			continue;
		}
		
		/* check if we have one part of the remote channel */
		AUTO_RELEASE sccp_device_t *remoteDevice = sccp_device_retain(linedevice->device);

		if (remoteDevice) {
			sccp_callerid_presentation_t presenceParameter = CALLERID_PRESENTATION_ALLOWED;
			iCallInfo.Getter(ci, SCCP_CALLINFO_PRESENTATION, &presenceParameter, SCCP_CALLINFO_KEY_SENTINEL);
			uint8_t stateVisibility = (c->privacy || !presenceParameter) ? SKINNY_CALLINFO_VISIBILITY_HIDDEN : SKINNY_CALLINFO_VISIBILITY_DEFAULT;

			/* Remarking the next piece out, solves the transfer issue when using sharedline as default on the transferer. Don't know why though (yet) */
			if (state != SCCP_CHANNELSTATE_ONHOOK) {
				AUTO_RELEASE sccp_channel_t *activeChannel = sccp_device_getActiveChannel(remoteDevice);

				if (activeChannel && (sccp_strequals(iPbx.getChannelLinkedId(activeChannel), iPbx.getChannelLinkedId(c)) || (activeChannel->conference_id && activeChannel->conference_id == c->conference_id))) {
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: (indicate_remote_device) Already Own Part of the Call: Skipped\n", DEV_ID_LOG(device));
					//sccp_log_and(DEBUGCAT_INDICATE + DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "%s: LinkedId: %s / %s: LinkedId Remote: %s\n", DEV_ID_LOG(device), iPbx.getChannelLinkedId(c), DEV_ID_LOG(remoteDevice), iPbx.getChannelLinkedId(activeChannel));
					continue;
				}
			}

			if (linedevice) {
				lineInstance = linedevice->lineInstance;							//sccp_device_find_index_for_line(remoteDevice, line->name);
			}
			switch (state) {
				case SCCP_CHANNELSTATE_OFFHOOK:
					/* do nothing here, we will do the offhook simulation in CONNECTED or ONHOOK -MC */
					break;

				case SCCP_CHANNELSTATE_DOWN:
				case SCCP_CHANNELSTATE_ONHOOK:
					if (SKINNY_CALLTYPE_INBOUND == c->calltype && c->answered_elsewhere) {
#if 0 /* phonebookRecord was set to NONE at the top, does not make sense to switch on it */
						switch (phonebookRecord) {
							case SCCP_PHONEBOOK_RECEIVED:
								pbx_log(LOG_NOTICE, "%s: call was answered elsewhere, record this as received call\n", DEV_ID_LOG(remoteDevice));
								remoteDevice->indicate->remoteOffhook(remoteDevice, lineInstance, callid);
								remoteDevice->indicate->connected(remoteDevice, lineInstance, callid, calltype, ci);
								break;
							case SCCP_PHONEBOOK_NONE:
								sccp_device_sendcallstate(remoteDevice, lineInstance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
								break;
							case SCCP_PHONEBOOK_MISSED:
							case SCCP_PHONEBOOK_SENTINEL:
								/* do nothing */
								break;
						}
#endif
					}
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s -> %s: indicate remote onhook (lineInstance: %d, callid: %d)\n", DEV_ID_LOG(device), DEV_ID_LOG(remoteDevice), lineInstance, c->callid);
					remoteDevice->indicate->remoteOnhook(remoteDevice, lineInstance, callid);
					break;

				case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
				case SCCP_CHANNELSTATE_CONNECTED:
#if 0 /* phonebookRecord was set to NONE at the top, does not make sense to switch on it */
					switch (c->calltype) {
						case SKINNY_CALLTYPE_OUTBOUND:
							switch (phonebookRecord) {
								case SCCP_PHONEBOOK_RECEIVED:
									remoteDevice->indicate->remoteOffhook(remoteDevice, lineInstance, callid);
									remoteDevice->indicate->dialing(remoteDevice, instance, callid, calltype, ci, dialedNumber);
									remoteDevice->indicate->proceed(remoteDevice, lineInstance, callid, calltype, ci);
									remoteDevice->indicate->connected(remoteDevice, lineInstance, callid, calltype, ci);
									break;
								case SCCP_PHONEBOOK_MISSED:
								case SCCP_PHONEBOOK_NONE:
									/* do nothing */
									//sccp_device_sendcallstate(remoteDevice, lineInstance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
									break;
							}
							break;
						case SKINNY_CALLTYPE_INBOUND:
							switch (phonebookRecord) {
								case SCCP_PHONEBOOK_RECEIVED:
									remoteDevice->indicate->remoteOffhook(remoteDevice, lineInstance, callid);
									remoteDevice->indicate->offhook(remoteDevice, linedevice, callid);
									remoteDevice->indicate->connected(remoteDevice, lineInstance, callid, calltype, ci);

									break;
								case SCCP_PHONEBOOK_NONE:
									sccp_device_sendcallstate(remoteDevice, lineInstance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
									break;
								case SCCP_PHONEBOOK_MISSED:
								case SCCP_PHONEBOOK_SENTINEL:
									/* do nothing */
									break;
							}
							break;

						default:
							break;
					}
#endif

					remoteDevice->indicate->remoteConnected(remoteDevice, lineInstance, callid, stateVisibility);
					iCallInfo.Send(ci, callid, calltype, lineInstance, remoteDevice, TRUE);
					break;

				case SCCP_CHANNELSTATE_HOLD:
					if (c->channelStateReason == SCCP_CHANNELSTATEREASON_NORMAL) {
						remoteDevice->indicate->remoteHold(remoteDevice, lineInstance, callid, SKINNY_CALLPRIORITY_NORMAL, stateVisibility);
						iCallInfo.Send(ci, callid, calltype, lineInstance, remoteDevice, TRUE);
					} else {
						sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Skipped Remote Hold Indication for reason: %s\n", DEV_ID_LOG(device), sccp_channelstatereason2str(c->channelStateReason));
					}
					break;

				default:
					break;

			}
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Finish Indicating state %s (%d) with reason: %s (%d) on remote device %s for channel %s\n", DEV_ID_LOG(device), sccp_channelstate2str(state), state, sccp_channelstatereason2str(c->channelStateReason), c->channelStateReason, DEV_ID_LOG(remoteDevice), c->designator);
		}
	}
	iCallInfo.Destructor(&ci);
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
