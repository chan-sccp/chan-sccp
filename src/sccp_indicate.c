
/*!
 * \file        sccp_indicate.c
 * \brief       SCCP Indicate Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static void __sccp_indicate_remote_device(sccp_device_t * device, sccp_channel_t * c, sccp_line_t * line, uint8_t state);

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
 *      - line->devices is not always locked
 * 
 * \lock
 *      - device
 *        - see sccp_device_find_index_for_line()
 *      - see sccp_mwi_lineStatusChangedEvent() via sccp_event_fire()
 */
void __sccp_indicate(sccp_device_t * device, sccp_channel_t * c, uint8_t state, uint8_t debug, char *file, int line, const char *pretty_function)
{
	sccp_device_t *d;
	sccp_line_t *l;
	int instance;
	sccp_linedevices_t *linedevice;

	if (debug) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: [INDICATE] state '%d' in file '%s', on line %d (%s)\n", state, file, line, pretty_function);
	}

	d = (device) ? sccp_device_retain(device) : sccp_channel_getDevice_retained(c);
	if (!d) {
		sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_1 "SCCP: The channel %d does not have a device\n", c->callid);
		return;
	}

	if (!(l = sccp_line_retain(c->line))) {
		pbx_log(LOG_ERROR, "SCCP: The channel %d does not have a line\n", c->callid);
		d = sccp_device_release(d);
		return;
	}
	instance = sccp_device_find_index_for_line(d, l->name);
	linedevice = sccp_linedevice_find(d, l);

	/* all the check are ok. We can safely run all the dev functions with no more checks */
	sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Indicate SCCP state %d (%s),channel state %d (%s) on call %s-%08x (previous channelstate %d (%s))\n", d->id, state, sccp_indicate2str(state), c->state, sccp_indicate2str(c->state), l->name, c->callid, c->previousChannelState, sccp_indicate2str(c->previousChannelState));
	sccp_channel_setSkinnyCallstate(c, state);

	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
			//PBX(set_callstate)(c, AST_STATE_DOWN);
			break;
		case SCCP_CHANNELSTATE_OFFHOOK:
			if (SCCP_CHANNELSTATE_DOWN == c->previousChannelState) {				// new call
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
				PBX(set_callstate) (c, AST_STATE_OFFHOOK);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_set_cplane(d, instance, 1);
				sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
				sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_OFFHOOK);
				sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, c->callid, 0);
			} else {										// call pickup
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
				PBX(set_callstate) (c, AST_STATE_OFFHOOK);
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_set_cplane(d, instance, 1);
				sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_OFFHOOK);
			}
			/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
			break;
		case SCCP_CHANNELSTATE_GETDIGITS:
			c->state = SCCP_CHANNELSTATE_OFFHOOK;
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
			PBX(set_callstate) (c, AST_STATE_OFFHOOK);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
			sccp_dev_set_cplane(d, instance, 1);
			sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, instance, c->callid, 0);
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
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 0);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_OFFHOOK);
			sccp_dev_set_cplane(d, instance, 1);
			PBX(set_callstate) (c, AST_STATE_OFFHOOK);
			/* for earlyrtp take a look at sccp_channel_newcall because we have no c->owner here */
			break;
		case SCCP_CHANNELSTATE_ONHOOK:
			c->state = SCCP_CHANNELSTATE_DOWN;
			PBX(set_callstate) (c, AST_STATE_DOWN);
			if (c == d->active_channel) {
				sccp_dev_stoptone(d, instance, c->callid);
			}
			
			sccp_dev_stoptone(d, instance, c->callid);
			sccp_dev_cleardisplaynotify(d);
			sccp_dev_clearprompt(d, instance, c->callid);

			/** if channel was answered somewhere, set state to connected before onhook -> no missedCalls entry */
			if (c->answered_elsewhere) {
				sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
			}

			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_cplane(d, instance, 0);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOOK);

			sccp_handle_time_date_req(d->session, d, NULL);	/** we need datetime on hangup for 7936 */
			if (c == d->active_channel) {
				sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
			}

			if (c->previousChannelState == SCCP_CHANNELSTATE_RINGING) {
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			}

			break;
		case SCCP_CHANNELSTATE_RINGOUT:
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGOUT, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			d->protocol->sendDialedNumber(d, c);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_RING_OUT, 0);
			sccp_channel_send_callinfo(d, c);
			if (!c->rtp.audio.rtp) {
				if (d->earlyrtp) {
					sccp_channel_openReceiveChannel(c);
				} else {
					sccp_dev_starttone(d, (uint8_t) SKINNY_TONE_ALERTINGTONE, instance, c->callid, 0);
				}
			}
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGOUT);
			
			PBX(set_callstate) (c, AST_STATE_RING);
			break;
		case SCCP_CHANNELSTATE_RINGING:
			sccp_dev_cleardisplaynotify(d);
			sccp_dev_clearprompt(d, instance, 0);

			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_channel_send_callinfo(d, c);

			if ((d->dndFeature.enabled && d->dndFeature.status == SCCP_DNDMODE_SILENT && c->ringermode != SKINNY_RINGTYPE_URGENT)) {
				sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: DND is activated on device\n", d->id);
				sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
			} else {
				sccp_linedevices_t *ownlinedevice;
				sccp_device_t *remoteDevice;

				SCCP_LIST_TRAVERSE(&l->devices, ownlinedevice, list) {
					remoteDevice = ownlinedevice->device;

					if (d && remoteDevice && remoteDevice == d) {
						sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found matching linedevice. Aux parameter = %s\n", d->id, ownlinedevice->subscriptionId.aux);

						/** Check the auxiliary parameter of the linedevice to enable silent ringing for certain devices on a certain line.**/
						if (0 == strncmp(ownlinedevice->subscriptionId.aux, "silent", 6)) {
							sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
							sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Forcing silent ring for specific device.\n", d->id);
						} else {
							sccp_dev_set_ringer(d, c->ringermode, instance, c->callid);
							sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Normal ring occurred.\n", d->id);
						}
					}
				}
			}

			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);

			char prompt[100];

			if (c->ringermode == SKINNY_RINGTYPE_URGENT) {
				snprintf(prompt, sizeof(prompt), "Urgent Call from: %s", strlen(c->callInfo.callingPartyName) ? c->callInfo.callingPartyName : c->callInfo.callingPartyNumber);
			} else {
				snprintf(prompt, sizeof(prompt), "Incoming Call from: %s", strlen(c->callInfo.callingPartyName) ? c->callInfo.callingPartyName : c->callInfo.callingPartyNumber);
			}
			sccp_dev_displayprompt(d, instance, c->callid, prompt, 0);

			PBX(set_callstate) (c, AST_STATE_RINGING);						/*!\todo thats not the right place to update pbx state */
			break;
		case SCCP_CHANNELSTATE_CONNECTED:

			d->indicate->connected(d, linedevice, c);
			if (!c->rtp.audio.rtp || c->previousChannelState == SCCP_CHANNELSTATE_HOLD || c->previousChannelState == SCCP_CHANNELSTATE_CALLTRANSFER || c->previousChannelState == SCCP_CHANNELSTATE_CALLCONFERENCE || c->previousChannelState == SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_channel_openReceiveChannel(c);
			} else if (c->rtp.audio.rtp) {
				sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Did not reopen an RTP stream as old SCCP state was (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
			}
			break;
		case SCCP_CHANNELSTATE_BUSY:
			if (!c->rtp.audio.rtp) {
				sccp_dev_starttone(d, SKINNY_TONE_LINEBUSYTONE, instance, c->callid, 0);
			}
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_BUSY, 0);
			PBX(set_callstate) (c, AST_STATE_BUSY);
			break;
		case SCCP_CHANNELSTATE_PROGRESS:								/* \todo SCCP_CHANNELSTATE_PROGRESS To be checked */
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_2 "%s: SCCP_CHANNELSTATE_PROGRESS\n", d->id);
			if (c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) {

				/** this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app) */
				sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
			} else {
				sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) from (%s)\n", sccp_indicate2str(c->previousChannelState));
				if (!c->rtp.audio.rtp && d->earlyrtp) {
					sccp_channel_openReceiveChannel(c);
				}
				sccp_dev_displayprompt(d, instance, c->callid, "Call Progress", 0);
			}
			break;
		case SCCP_CHANNELSTATE_PROCEED:
			if (c->previousChannelState == SCCP_CHANNELSTATE_CONNECTED) {				// this is a bug of asterisk 1.6 (it sends progress after a call is answered then diverted to some extensions with dial app)
				sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Asterisk requests to change state to (Progress) after (Connected). Ignoring\n");
				break;
			}
			sccp_dev_stoptone(d, instance, c->callid);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
			sccp_channel_send_callinfo(d, c);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_PROCEED, 0);
			if (!c->rtp.audio.rtp && d->earlyrtp) {
				sccp_channel_openReceiveChannel(c);
			}
			break;
		case SCCP_CHANNELSTATE_HOLD:
			if (c->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) {
				sccp_channel_closeReceiveChannel(c);
			}
			if (c->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE) {
				sccp_channel_closeMultiMediaReceiveChannel(c);
			}
			sccp_handle_time_date_req(d->session, d, NULL);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_ONHOLD);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_HOLD, 0);
			sccp_channel_send_callinfo(d, c);
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
			/* it will be emulated if the rtp audio stream is open */
			if (!c->rtp.audio.rtp) {
				sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
			}
			sccp_channel_send_callinfo(d, c);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TEMP_FAIL, 0);
			break;
		case SCCP_CHANNELSTATE_CALLWAITING:
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_CALLWAITING (%s)\n", DEV_ID_LOG(d), sccp_indicate2str(c->previousChannelState));
			sccp_channel_callwaiting_tone_interval(d, c);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
			sccp_channel_send_callinfo(d, c);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CALL_WAITING, 0);
			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_SILENT, instance, c->callid);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_RINGIN);
			PBX(set_callstate) (c, AST_STATE_RINGING);
#ifdef CS_SCCP_CONFERENCE
			if (d->conferencelist_active) {
				sccp_conference_hide_list_ByDevice(d);
			}
#endif
			break;
		case SCCP_CHANNELSTATE_CALLTRANSFER:
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_TRANSFER, 0);
			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_device_sendcallstate(d, instance, c->callid, SCCP_CHANNELSTATE_CALLTRANSFER, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_channel_send_callinfo(d, c);
			break;
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
			sccp_device_sendcallstate(d, instance, c->callid, SCCP_CHANNELSTATE_CALLCONFERENCE, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			break;
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			/*! \todo SCCP_CHANNELSTATE_INVALIDCONFERENCE To be implemented */
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_INVALIDCONFERENCE (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
			break;
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_CONNECTEDCONFERENCE (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
			sccp_dev_set_ringer(d, SKINNY_RINGTYPE_OFF, instance, c->callid);
			sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_ON);
			sccp_dev_stoptone(d, instance, c->callid);
			sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_channel_send_callinfo(d, c);
			sccp_dev_set_cplane(d, instance, 1);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_CONNCONF);
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CONNECTED, 0);

			if (!c->rtp.audio.rtp) {
				sccp_channel_openReceiveChannel(c);
			} else if (c->rtp.audio.rtp) {
				sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Did not reopen an RTP stream as old SCCP state was (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
			}

			break;
		case SCCP_CHANNELSTATE_CALLPARK:
			sccp_device_sendcallstate(d, instance, c->callid, SCCP_CHANNELSTATE_CALLPARK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
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
			sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_UNKNOWN_NUMBER, 0);
			if (c->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) {
				sccp_channel_closeReceiveChannel(c);
			}
			if (c->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE) {
				sccp_channel_closeMultiMediaReceiveChannel(c);
			}
			sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
			break;
		case SCCP_CHANNELSTATE_DIALING:
			sccp_dev_stoptone(d, instance, c->callid);
			d->protocol->sendDialedNumber(d, c);
			sccp_channel_send_callinfo(d, c);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
			if (d->earlyrtp == SCCP_CHANNELSTATE_DIALING && !c->rtp.audio.rtp) {
				sccp_channel_openReceiveChannel(c);
			}
			break;
		case SCCP_CHANNELSTATE_DIGITSFOLL:
			sccp_channel_send_callinfo(d, c);
			sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_DIGITSFOLL);
			break;
		case SCCP_CHANNELSTATE_BLINDTRANSFER:								/* \todo SCCP_CHANNELSTATE_BLINDTRANSFER To be implemented */
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE_BLINDTRANSFER (%s)\n", d->id, sccp_indicate2str(c->previousChannelState));
			break;
		default:											/* \todo SCCP_CHANNELSTATE:default To be implemented */
			sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "%s: SCCP_CHANNELSTATE:default  %s (%d) -> %s (%d)\n", d->id, sccp_indicate2str(c->previousChannelState), c->previousChannelState, sccp_indicate2str(c->state), c->state);
			break;
	}

	/* if channel state has changed, notify the others */
	if (c->state != c->previousChannelState) {
		/* if it is a shalred line and a state of interest */
		if ((SCCP_RWLIST_GETSIZE(l->devices) > 1) && (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DOWN || c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_HOLD)
		    ) {
			/* notify all remote devices */
			__sccp_indicate_remote_device(d, c, l, state);
		}

		/* notify features (sccp_feat_channelstateChanged = empty function, skipping) */
		//      sccp_feat_channelstateChanged(d, c);

		sccp_event_t event;

		memset(&event, 0, sizeof(sccp_event_t));
		event.type = SCCP_EVENT_LINESTATUS_CHANGED;
		event.event.lineStatusChanged.line = sccp_line_retain(l);
		event.event.lineStatusChanged.device = sccp_device_retain(d);
		event.event.lineStatusChanged.state = c->state;
		sccp_event_fire(&event);
	}

	sccp_log((DEBUGCAT_INDICATE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Finish to indicate state SCCP (%s) on call %s-%08x\n", d->id, sccp_indicate2str(state), l->name, c->callid);

	d = sccp_device_release(d);
	l = sccp_line_release(l);
	linedevice = linedevice ? sccp_linedevice_release(linedevice) : linedevice;
}

/*!
 * \brief Indicate to Remote Device
 * \param device SCCP Device
 * \param c SCCP Channel
 * \param line SCCP Line
 * \param state State as int
 * \todo Explain Pretty Function
 * 
 * \warning
 *      - line->devices is not always locked
 */
static void __sccp_indicate_remote_device(sccp_device_t * device, sccp_channel_t * c, sccp_line_t * line, uint8_t state)
{
	sccp_device_t *remoteDevice;
	sccp_channel_t *activeChannel;
	int instance;
	sccp_phonebook_t phonebookRecord = SCCP_PHONEBOOK_NONE;

	//      uint32_t privacyStatus=0;
	if (!c || !line) {
		return;
	}

	uint8_t stateVisibility = (!c->privacy) ? SKINNY_CALLINFO_VISIBILITY_DEFAULT : SKINNY_CALLINFO_VISIBILITY_HIDDEN;

	/** \todo move this to channel->privacy */
	//      if (sccp_channel_getDevic(c))
	//              privacyStatus = sccp_channel_getDevic(c)->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;
	//      /* do not display private lines */
	//      if (state !=SCCP_CHANNELSTATE_CONNECTED && (c->privacy || privacyStatus > 0) ){
	//              sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "privacyStatus status is set, ignore remote devices\n");
	//              return;
	//      }

	/* do not propagate status of hotline */
	if (line == GLOB(hotline)->line) {
		sccp_log(DEBUGCAT_INDICATE) (VERBOSE_PREFIX_3 "SCCP: (__sccp_indicate_remote_device) I'm a hotline, do not notify me!\n");
		return;
	}
	sccp_linedevices_t *linedevice;

	sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Indicate state %s (%d) on remote devices for channel %s (call %08x)\n", DEV_ID_LOG(device), sccp_indicate2str(state), state, c->designator, c->callid);
	SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
		if (!linedevice->device) {
			pbx_log(LOG_NOTICE, "Strange to find a linedevice (%p) here without a valid device connected to it !", linedevice);
			continue;
		}

		if (device && linedevice->device == device) {
			continue;
		}

		if ((remoteDevice = sccp_device_retain(linedevice->device))) {
			sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Indicate state %s (%d) on remote device %s for channel %s (call %08x)\n", DEV_ID_LOG(device), sccp_indicate2str(state), state, DEV_ID_LOG(remoteDevice), c->designator, c->callid);
			/* check if we have one part of the remote channel */
			if ((activeChannel = sccp_channel_get_active(remoteDevice))) {
				if (sccp_strequals(PBX(getChannelLinkedId) (activeChannel), PBX(getChannelLinkedId) (c))) {
					stateVisibility = SKINNY_CALLINFO_VISIBILITY_HIDDEN;
				}
				activeChannel = sccp_channel_release(activeChannel);
			}
			/* done */

			instance = sccp_device_find_index_for_line(remoteDevice, line->name);
			switch (state) {
				case SCCP_CHANNELSTATE_OFFHOOK:
					if (c->previousChannelState == SCCP_CHANNELSTATE_RINGING) {
						sccp_dev_set_ringer(remoteDevice, SKINNY_RINGTYPE_OFF, instance, c->callid);
						sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, stateVisibility);	/* send connected, so it is not listed as missed call */
					}
					break;

				case SCCP_CHANNELSTATE_DOWN:
				case SCCP_CHANNELSTATE_ONHOOK:

					switch(phonebookRecord){
						case SCCP_PHONEBOOK_RECEIVED:
							pbx_log(LOG_NOTICE, "%s: call was answered elsewhere, record this as received call\n", DEV_ID_LOG(remoteDevice));
							remoteDevice->indicate->offhook(remoteDevice, linedevice, c->callid);
							remoteDevice->indicate->connected(remoteDevice, linedevice, c);
						break;
						case SCCP_PHONEBOOK_MISSED:
						  
						break;
						case SCCP_PHONEBOOK_NONE:
							sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
						break;
					}
					

					sccp_dev_cleardisplaynotify(remoteDevice);
					sccp_dev_clearprompt(remoteDevice, instance, c->callid);
					sccp_dev_set_ringer(remoteDevice, SKINNY_RINGTYPE_OFF, instance, c->callid);
					sccp_dev_set_speaker(remoteDevice, SKINNY_STATIONSPEAKER_OFF);
					sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_NORMAL, stateVisibility);
					sccp_dev_set_cplane(remoteDevice, linedevice->lineInstance, 1);
					sccp_dev_set_keyset(remoteDevice, linedevice->lineInstance, c->callid, KEYMODE_ONHOOK);
					break;

				case SCCP_CHANNELSTATE_CONNECTED:
					/* DD: We sometimes set the channel to offhook first before setting it to connected state.
					   This seems to be necessary to have incoming calles logged properly.
					   If this is done, the ringer would not get turned off on remote devices.
					   So I removed the if clause below. Hopefully, this will not cause other calls to stop
					   ringing if multiple calls are ringing concurrently on a shared line. */

					sccp_dev_set_ringer(remoteDevice, SKINNY_RINGTYPE_OFF, instance, c->callid);
					sccp_dev_clearprompt(remoteDevice, instance, c->callid);
					sccp_device_sendcallstate(remoteDevice, instance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE, SKINNY_CALLPRIORITY_NORMAL, stateVisibility);
					sccp_channel_send_callinfo(remoteDevice, c);
					sccp_dev_set_keyset(remoteDevice, instance, c->callid, KEYMODE_ONHOOKSTEALABLE);
					break;

				case SCCP_CHANNELSTATE_HOLD:
					remoteDevice->indicate->remoteHold(remoteDevice, instance, c->callid, SKINNY_CALLPRIORITY_NORMAL, stateVisibility);
					sccp_channel_send_callinfo(remoteDevice, c);
					break;

				default:
					break;

			}
			//sccp_log((DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Finish Indicating state %s (%d) on remote device %s for channel %s (call %08x)\n", DEV_ID_LOG(device), sccp_indicate2str(state), state, DEV_ID_LOG(remoteDevice), c->designator, c->callid);
			remoteDevice = sccp_device_release(remoteDevice);
		}
	}
}

/*!
 * \brief Convert SCCP Indication/ChannelState 2 String
 * \param state SCCP_CHANNELSTATE_*
 * \return Channel State String
 */
const char *sccp_indicate2str(uint8_t state)
{
	switch (state) {
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
		case SCCP_CHANNELSTATE_PROGRESS:
			return "Progress";
		case SCCP_CHANNELSTATE_SPEEDDIAL:
			return "SpeedDial";
		case SCCP_CHANNELSTATE_DIGITSFOLL:
			return "DigitsFoll";
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			return "InvalidConf";
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
			return "ConnConf";
		case SCCP_CHANNELSTATE_BLINDTRANSFER:
			return "BlindTransfer";
		case SCCP_CHANNELSTATE_ZOMBIE:
			return "Zombie";
		case SCCP_CHANNELSTATE_DND:
			return "Dnd";
		default:
			return "Unknown";
	}
}

/*!
 * \brief Convert SKINNYCall State 2 String
 * \param state SKINNY_CALLSTATE_*
 * \return Call State String
 */
const char *sccp_callstate2str(uint8_t state)
{
	switch (state) {
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
