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
#include "sccp_linedevice.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

SCCP_FILE_VERSION(__FILE__, "");

static void __sccp_indicate_remote_device(constDevicePtr device, channelPtr c, linePtr line, const sccp_channelstate_t state);

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
//void __sccp_indicate(sccp_device_t * _device, sccp_channel_t * c, uint8_t state, uint8_t debug, char *file, int line, const char *pretty_function)
void __sccp_indicate(constDevicePtr maybe_device, channelPtr c, const sccp_channelstate_t state, const uint8_t debug, const char * file, const int line, const char * pretty_function)
{
	if (debug) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: [INDICATE] state '%d' in file '%s', on line %d (%s)\n", state, file, line, pretty_function);
	}

	AUTO_RELEASE(sccp_device_t, d , (maybe_device) ? sccp_device_retain(maybe_device) : sccp_channel_getDevice(c));
	if (!d) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: The channel %d does not have a device\n", c->callid);
		return;
	}

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(c->line));
	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: The channel %d does not have a line\n", c->callid);
		return;
	}
	uint16_t lineInstance = sccp_device_find_index_for_line(d, l->name);

	/* all the check are ok. We can safely run all the dev functions with no more checks */
	sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Indicate SCCP new state:%s, current channel state:%s on call:%s, lineInstance:%d (previous channelstate:%s)\n", d->id, sccp_channelstate2str(state), sccp_channelstate2str(c->state), c->designator, lineInstance, sccp_channelstate2str(c->previousChannelState));
	sccp_channel_setChannelstate(c, state);
	sccp_callinfo_t * const ci = sccp_channel_getCallInfo(c);

	if (SCCP_CHANNELSTATE_Idling(state) || SCCP_CHANNELSTATE_IsTerminating(state)) {
		sccp_device_indicateMWI(d);
	} else {
		sccp_device_suppressMWI(d);
	}
	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
			{
				//iPbx.set_callstate(c, AST_STATE_DOWN);
			}
			break;
		case SCCP_CHANNELSTATE_OFFHOOK:
			{
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
				sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_ON);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_set_cplane(d, lineInstance, 1);
				if (SCCP_CHANNELSTATE_DOWN == c->previousChannelState) {		// new call
					sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
					c->setTone(c, SKINNY_TONE_INSIDEDIALTONE, SKINNY_TONEDIRECTION_USER);
					if(!(d->hasMWILight()) && d->voicemailStatistic.newmsgs) {
						sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_MWI))(VERBOSE_PREFIX_2 "%s: Device does not have MWI-light and does have messages waiting -> stutter\n", d->id);
						c->setTone(c, SKINNY_TONE_PARTIALDIALTONE, SKINNY_TONEDIRECTION_USER); /* Add Stutter Pattern for ATA/Analog devices */
					}
				}
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_OFFHOOK);
			}
			break;
		case SCCP_CHANNELSTATE_GETDIGITS:
			{
				c->state = SCCP_CHANNELSTATE_OFFHOOK;
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_DIGITSFOLL);
				sccp_dev_set_cplane(d, lineInstance, 1);
				c->setTone(c, SKINNY_TONE_ZIP, SKINNY_TONEDIRECTION_USER);
			}
			break;
		case SCCP_CHANNELSTATE_DIGITSFOLL:
			{
				int lenDialed = sccp_strlen(c->dialedNumber);
				int lenSecDialtoneDigits = sccp_strlen(l->secondary_dialtone_digits);
				skinny_tone_t secondary_dialtone_tone = l->secondary_dialtone_tone;
				sccp_log(DEBUGCAT_INDICATE)(VERBOSE_PREFIX_3 "%s: digitsfollow: dialed:%s, lenDialed:%d, secDialtoneDigits:%s, lenSecDialtoneDigits:%d, secondary_dialtine:%d\n", 
					c->designator,
					c->dialedNumber,
					lenDialed,
					l->secondary_dialtone_digits,
					lenSecDialtoneDigits,
					secondary_dialtone_tone);
				if (lenSecDialtoneDigits > 0 && lenDialed == lenSecDialtoneDigits && !strncmp(c->dialedNumber, l->secondary_dialtone_digits, lenSecDialtoneDigits)) {
					c->setTone(c, secondary_dialtone_tone, SKINNY_TONEDIRECTION_USER);
				} else if (lenDialed > 0) {
					c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
				}
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_DIGITSFOLL);
				break;
			}
		case SCCP_CHANNELSTATE_SPEEDDIAL:
			{
				c->state = SCCP_CHANNELSTATE_OFFHOOK;
				/* clear all the display buffers */
				sccp_dev_cleardisplaynotify(d);
				sccp_dev_clearprompt(d, 0, 0);

				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				//sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_OFFHOOK);
				sccp_dev_set_cplane(d, lineInstance, 1);
			}
			break;
		case SCCP_CHANNELSTATE_DIALING:
			{
				if (c->previousChannelState == SCCP_CHANNELSTATE_DIALING) {
					break;
				}
				d->indicate->dialing(d, lineInstance, c->callid, c->calltype, ci, c->dialedNumber);
			}
			break;
		case SCCP_CHANNELSTATE_RINGOUT:
			{
				// we already send out the ringing state before */
				// first ringout indicate (before connected line update */
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_RING_OUT, GLOB(digittimeout));
				c->setTone(c, SKINNY_TONE_ALERTINGTONE, SKINNY_TONEDIRECTION_USER);
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_RINGOUT);
				iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, FALSE);
			}
			break;
		case SCCP_CHANNELSTATE_RINGOUT_ALERTING:
			/* send by connected line update, to show that we know the remote end, we can now update the callinfo */
			sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_RINGOUT, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, TRUE);
			break;
		case SCCP_CHANNELSTATE_RINGING:
			{
				sccp_dev_cleardisplaynotify(d);
				sccp_dev_clearprompt(d, lineInstance, 0);

				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, TRUE);
				sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_BLINK);

				if ((d->dndFeature.enabled && d->dndFeature.status == SCCP_DNDMODE_SILENT && c->ringermode != SKINNY_RINGTYPE_URGENT)) {
					sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: DND is active on device\n", d->id);
					sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
					if (GLOB(dnd_tone) && d->dndFeature.status == SCCP_DNDMODE_SILENT) {
						sccp_dev_starttone(d, GLOB(dnd_tone), 0, 0, SKINNY_TONEDIRECTION_USER);
					}
				} else {
					sccp_linedevice_t * ownlinedevice = NULL;
					sccp_device_t *remoteDevice = NULL;

					SCCP_LIST_TRAVERSE(&l->devices, ownlinedevice, list) {
						remoteDevice = ownlinedevice->device;

						if (d && remoteDevice && remoteDevice == d) {
							sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Found matching ld. Aux parameter = %s\n", d->id, ownlinedevice->subscriptionId.aux);
							if (0 == strncmp(ownlinedevice->subscriptionId.aux, "silent", 6)) {
								sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
								sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: Forcing silent ring for specific device.\n", d->id);
							} else {
								sccp_dev_set_ringer(d, c->ringermode, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
								sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Normal ring occurred.\n", d->id);
							}
						}
					}
				}

				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_RINGIN);
				char prompt[100];

				char orig_called_name[StationMaxNameSize] = {0};
				char orig_called_num[StationMaxDirnumSize] = {0};
				char calling_name[StationMaxNameSize] = {0};
				char calling_num[StationMaxDirnumSize] = {0};
				iCallInfo.Getter(ci,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &orig_called_name,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &orig_called_num,
					SCCP_CALLINFO_CALLINGPARTY_NAME, &calling_name, 
					SCCP_CALLINFO_CALLINGPARTY_NUMBER, &calling_num,
					SCCP_CALLINFO_KEY_SENTINEL);

				/* GPL - Modify below to expand possible data shown on phone */
				char caller[100];
				if (!sccp_strlen_zero(calling_name)) {
					if (!sccp_strlen_zero(calling_num)) {
						snprintf(caller,sizeof(caller), "%s (%s)", calling_name, calling_num);
					} else {
						snprintf(caller,sizeof(caller), "%s", calling_name);
					}
				} else {
					if (!sccp_strlen_zero(calling_num)) {
						snprintf(caller,sizeof(caller), "%s", calling_num);
					} else {
						snprintf(caller,sizeof(caller), "%s", SKINNY_DISP_UNKNOWN_NUMBER);
					}
				}
				snprintf(prompt, sizeof(prompt), "%s%s", (c->ringermode == SKINNY_RINGTYPE_URGENT) ? SKINNY_DISP_FLASH : SKINNY_DISP_FROM, caller);
				sccp_dev_displayprompt(d, lineInstance, c->callid, prompt, GLOB(digittimeout));
			}
			break;
		case SCCP_CHANNELSTATE_PROGRESS:
			sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s (%s) wantsEarlyRTP:%s, progressSent:%s\n", c->designator, __func__, c->wantsEarlyRTP() ? "yes" : "no", c->progressSent() ? "yes" : "no");
			if(c->wantsEarlyRTP() && !c->progressSent()) {
				c->makeProgress(c);
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_CALL_PROGRESS, GLOB(digittimeout));
			}
			break;
		case SCCP_CHANNELSTATE_PROCEED:
			{
				if (c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) {		// this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app)
					sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
					break;
				}
				d->indicate->proceed(d, lineInstance, c->callid, c->calltype, ci);
			}
			break;
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			{
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
				sccp_dev_clearprompt(d, lineInstance, c->callid);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/** send connected, so it is not listed as missed call */
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_ONHOOKSTEALABLE);
			}
			break;
		case SCCP_CHANNELSTATE_CONNECTED:
			{
				d->indicate->connected(d, lineInstance, c->callid, c->calltype, ci);
				sccp_rtp_setCallback(&c->rtp.audio, SCCP_RTP_RECEPTION, sccp_channel_startMediaTransmission);
				if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_RECEPTION)) {
					sccp_channel_openReceiveChannel(c);
				} else if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_TRANSMISSION)) {
					/* this looks a little confusing, maybe we should not have two but only one rtp callback */
					sccp_rtp_runCallback(&c->rtp.audio, SCCP_RTP_RECEPTION, c);
				} else {
					sccp_rtp_setCallback(&c->rtp.audio, SCCP_RTP_RECEPTION, NULL);
				}
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_CONNECTED);
			}
			break;
		case SCCP_CHANNELSTATE_BUSY:
			{
			if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_RECEPTION)) {
				c->setTone(c, SKINNY_TONE_LINEBUSYTONE, SKINNY_TONEDIRECTION_USER);
			}
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_BUSY, GLOB(digittimeout));
			}
			break;
		case SCCP_CHANNELSTATE_HOLD:
			{
				sccp_channel_closeAllMediaTransmitAndReceive(c);
				if (d->session) {
					sccp_handle_time_date_req(d->session, d, NULL);
				}
				sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_WINK);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_HOLD, GLOB(digittimeout));
				iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, TRUE);
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
#if CS_SCCP_CONFERENCE
				if (c->conference && d->conference) {
					sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_HOLDCONF);
				} else 
#endif
				{
					sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_ONHOLD);
				}
			}
			break;
		case SCCP_CHANNELSTATE_CALLWAITING:
			{
				/* When dialing a shared line which you also have registered, we don't want the outgoing call to show up on our own device as a callwaiting call */
				AUTO_RELEASE(sccp_channel_t, activeChannel , sccp_device_getActiveChannel(d));
				if (activeChannel && (sccp_strequals(iPbx.getChannelLinkedId(activeChannel), iPbx.getChannelLinkedId(c)))) {
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: (SCCP_CHANNELSTATE_CALLWAITING) Already Own Part of the Call: Skipping\n", DEV_ID_LOG(d));
					sccp_log_and(DEBUGCAT_INDICATE + DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "%s: LinkedId: %s / %s: LinkedId Remote: %s\n", DEV_ID_LOG(d), iPbx.getChannelLinkedId(c), DEV_ID_LOG(d), iPbx.getChannelLinkedId(activeChannel));
					break;
				}
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_CALLWAITING (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->previousChannelState));
				sccp_channel_callwaiting_tone_interval(d, c);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, TRUE);
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_CALL_WAITING, GLOB(digittimeout));
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, SKINNY_RINGDURATION_SINGLE, lineInstance, c->callid);
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_RINGIN);

#ifdef CS_SCCP_CONFERENCE
				if (d->conferencelist_active) {
					sccp_conference_hide_list_ByDevice(d);
				}
#endif
			}
			break;
		case SCCP_CHANNELSTATE_CALLPARK:
			{
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_CALLPARK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			}
			break;
		case SCCP_CHANNELSTATE_CALLTRANSFER:
			{
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TRANSFER, GLOB(digittimeout));
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, SKINNY_RINGDURATION_NORMAL, lineInstance, c->callid);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_CALLTRANSFER, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, FALSE);
			}
			break;
		case SCCP_CHANNELSTATE_BLINDTRANSFER:							// \todo SCCP_CHANNELSTATE_BLINDTRANSFER To be implemented
			{
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_BLINDTRANSFER (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			}
			break;
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
			{
				// sccp_device_sendcallstate(d, lineInstance, c->callid, SCCP_CHANNELSTATE_CALLCONFERENCE, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			}
			break;
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
			{
				d->indicate->connected(d, lineInstance, c->callid, c->calltype, ci);
				sccp_rtp_setCallback(&c->rtp.audio, SCCP_RTP_RECEPTION, sccp_channel_startMediaTransmission);
				if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_RECEPTION)) {
					sccp_channel_openReceiveChannel(c);
				} else if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_TRANSMISSION)) {
					sccp_rtp_runCallback(&c->rtp.audio, SCCP_RTP_TRANSMISSION, c);
				} else {
					sccp_rtp_setCallback(&c->rtp.audio, SCCP_RTP_RECEPTION, NULL);
				}
				sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_CONNCONF);
			}
			break;
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			{
				/*! \todo SCCP_CHANNELSTATE_INVALIDCONFERENCE To be implemented */
				sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_INVALIDCONFERENCE (%s)\n", d->id, sccp_channelstate2str(c->previousChannelState));
			}
			break;
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
			{
				/* this is for the earlyrtp. The 7910 does not play tones if a rtp stream is open */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_UNKNOWN_NUMBER, GLOB(digittimeout));
				sccp_channel_closeAllMediaTransmitAndReceive(c);
				c->setTone(c, SKINNY_TONE_REORDERTONE, SKINNY_TONEDIRECTION_USER);
				sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);			// wait 15 seconds, then hangup automatically
			}
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
			{
			if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_RECEPTION)) {
				/* congestion will be emulated if the rtp audio stream is not yet open */
				c->setTone(c, SKINNY_TONE_REORDERTONE, SKINNY_TONEDIRECTION_USER);
			}
			iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, FALSE);
			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, GLOB(digittimeout));
			sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);                                        // wait 15 seconds, then hangup automatically
			}
			break;
		case SCCP_CHANNELSTATE_ONHOOK:
			{
				c->state = SCCP_CHANNELSTATE_DOWN;
				if (c->answered_elsewhere && d->indicate->callhistory) {
					iCallInfo.Send(ci, c->callid, c->calltype, lineInstance, d, TRUE);
					d->indicate->callhistory(d, lineInstance, c->callid, d->callhistory_answered_elsewhere);
				}
				if (d->indicate && d->indicate->onhook) {
					d->indicate->onhook(d, lineInstance, c->callid);
				}
			}
			break;
		default:										//! \todo SCCP_CHANNELSTATE:default To be implemented
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE:default  %s (%d) -> %s (%d)\n", d->id, sccp_channelstate2str(c->previousChannelState), c->previousChannelState, sccp_channelstate2str(c->state), c->state);
			break;
	}

	/* if channel state has changed, notify the others */
	if (d && c->state != c->previousChannelState) {
		/* if it is a shared line and a state of interest */
		if((SCCP_RWLIST_GETSIZE(&l->devices) > 1) && !c->conference
		   && (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DOWN || c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_CONNECTED
		       || c->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE || c->state == SCCP_CHANNELSTATE_HOLD)) {
			/* notify all remote devices */
			__sccp_indicate_remote_device(d, c, l, state);
		}

		/* notify features (sccp_feat_channelstateChanged = empty function, skipping) */
		//sccp_feat_channelstateChanged(d, c);
		sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_LINESTATUS_CHANGED);
		if (event) {
			event->lineStatusChanged.line = sccp_line_retain(l);
			event->lineStatusChanged.optional_device = sccp_device_retain(d);
			event->lineStatusChanged.state = c->state;
			sccp_event_fire(event);
		}
	}

	sccp_log((DEBUGCAT_INDICATE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Finish to indicate channel state:%s on call:%s, lineInstance:%d. New channel state:%s\n", d->id, sccp_channelstate2str(state), c->designator, lineInstance, sccp_channelstate2str(c->state));
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
static void __sccp_indicate_remote_device(constDevicePtr device, channelPtr c, linePtr line, const sccp_channelstate_t state)
{
	int lineInstance = 0;

	if (!c || !line) {
		return;
	}

	/** \todo move this to channel->privacy */

	/* do not propagate status of hotline */
	if (line == GLOB(hotline)->line) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: (__sccp_indicate_remote_device) I'm a hotline, do not notify me!\n");
		return;
	}
	sccp_linedevice_t * ld = NULL;

	/* copy temp variables, information to be send to remote device (in another thread) */
	const uint32_t callid = c->callid;
	const skinny_calltype_t calltype = c->calltype;
	char dialedNumber[SCCP_MAX_EXTENSION];
	sccp_copy_string(dialedNumber, c->dialedNumber, SCCP_MAX_EXTENSION);
	sccp_callinfo_t * ci = iCallInfo.CopyConstructor(sccp_channel_getCallInfo(c));

	sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Remote Indicate state %s (%d) with reason: %s (%d) on remote devices for channel %s\n", DEV_ID_LOG(device), sccp_channelstate2str(state), state, sccp_channelstatereason2str(c->channelStateReason), c->channelStateReason, c->designator);
	SCCP_LIST_TRAVERSE(&line->devices, ld, list) {
		if(!ld->device) {
			pbx_log(LOG_NOTICE, "Strange to find a ld (%p) here without a valid device connected to it !", ld);
			continue;
		}

		if(ld->device == device) {
			// skip self
			continue;
		}

		/* check if we have one part of the remote channel */
		AUTO_RELEASE(sccp_device_t, remoteDevice, sccp_device_retain(ld->device));

		if (remoteDevice) {
			sccp_callerid_presentation_t presenceParameter = CALLERID_PRESENTATION_ALLOWED;
			iCallInfo.Getter(ci, SCCP_CALLINFO_PRESENTATION, &presenceParameter, SCCP_CALLINFO_KEY_SENTINEL);
			skinny_callinfo_visibility_t stateVisibility = (c->privacy || !presenceParameter) ? SKINNY_CALLINFO_VISIBILITY_HIDDEN : SKINNY_CALLINFO_VISIBILITY_DEFAULT;

			/* Remarking the next piece out, solves the transfer issue when using sharedline as default on the transferer. Don't know why though (yet) */
			if (state != SCCP_CHANNELSTATE_ONHOOK) {
				AUTO_RELEASE(sccp_channel_t, activeChannel , sccp_device_getActiveChannel(remoteDevice));

				if (activeChannel && (sccp_strequals(iPbx.getChannelLinkedId(activeChannel), iPbx.getChannelLinkedId(c)) || (activeChannel->conference_id && activeChannel->conference_id == c->conference_id))) {
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: (indicate_remote_device) Already Own Part of the Call: Skipped\n", DEV_ID_LOG(device));
					//sccp_log_and(DEBUGCAT_INDICATE + DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "%s: LinkedId: %s / %s: LinkedId Remote: %s\n", DEV_ID_LOG(device), iPbx.getChannelLinkedId(c), DEV_ID_LOG(remoteDevice), iPbx.getChannelLinkedId(activeChannel));
					continue;
				}
			}

			if(ld) {
				lineInstance = ld->lineInstance;                                        // sccp_device_find_index_for_line(remoteDevice, line->name);
			}
			switch (state) {
				case SCCP_CHANNELSTATE_DOWN:
				case SCCP_CHANNELSTATE_ONHOOK:
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s -> %s: indicate remote onhook (lineInstance: %d, callid: %d %s)\n", DEV_ID_LOG(device), DEV_ID_LOG(remoteDevice), lineInstance, c->callid, c->answered_elsewhere ? ", answered elsewhere" :"");
					if (SKINNY_CALLTYPE_INBOUND == c->calltype && c->answered_elsewhere && remoteDevice->indicate->callhistory) {
						remoteDevice->indicate->callhistory(remoteDevice, lineInstance, c->callid, remoteDevice->callhistory_answered_elsewhere);
					}
					remoteDevice->indicate->remoteOnhook(remoteDevice, lineInstance, callid);
					break;

				case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
				case SCCP_CHANNELSTATE_CONNECTED:
					sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s -> %s: indicate remote connected (lineInstance: %d, callid: %d %s)\n", DEV_ID_LOG(device), DEV_ID_LOG(remoteDevice), lineInstance, c->callid, c->answered_elsewhere ? ", answered elsewhere" : "");
					if (SKINNY_CALLTYPE_INBOUND == c->calltype && remoteDevice->indicate->callhistory) {
						//remoteDevice->indicate->callhistory(remoteDevice, lineInstance, c->callid, remoteDevice->callhistory_answered_elsewhere);
						remoteDevice->indicate->callhistory(remoteDevice, lineInstance, c->callid, SKINNY_CALL_HISTORY_DISPOSITION_IGNORE);
					}
					
					/* if line is not currently active on remote device, collapse the callstate */
					// if (remoteDevice->currentLine && ld->line != remoteDevice->currentLine && !(c->privacy || !presenceParameter)) {
					if((c->privacy || presenceParameter == CALLERID_PRESENTATION_FORBIDDEN)
					   || (!sccp_softkey_isSoftkeyInSoftkeySet(remoteDevice, KEYMODE_ONHOOKSTEALABLE, SKINNY_LBL_INTRCPT)
					       && !sccp_softkey_isSoftkeyInSoftkeySet(remoteDevice, KEYMODE_ONHOOKSTEALABLE, SKINNY_LBL_BARGE))) {
						stateVisibility = SKINNY_CALLINFO_VISIBILITY_COLLAPSED;
					}
					if (c->channelStateReason == SCCP_CHANNELSTATEREASON_BARGE || c->isBarging || c->isBarged) {
						stateVisibility = SKINNY_CALLINFO_VISIBILITY_HIDDEN;
					}
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
