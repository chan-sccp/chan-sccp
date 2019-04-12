/*!
 * \file        sccp_softkeys.c
 * \brief       SCCP SoftKeys Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
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
#include "sccp_softkeys.h"
#include "sccp_actions.h"
#include "sccp_device.h"
#include "sccp_feature.h"
#include "sccp_line.h"
#include "sccp_linedevice.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

SCCP_FILE_VERSION(__FILE__, "");

const uint8_t softkeysmap[32] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_HOLD,
	SKINNY_LBL_TRANSFER,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
	SKINNY_LBL_CFWDNOANSWER,
	SKINNY_LBL_BACKSPACE,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_RESUME,
	SKINNY_LBL_ANSWER,
	SKINNY_LBL_INFO,
	SKINNY_LBL_CONFRN,
	SKINNY_LBL_PARK,
	SKINNY_LBL_JOIN,
	SKINNY_LBL_MEETME,
	SKINNY_LBL_PICKUP,
	SKINNY_LBL_GPICKUP,
	SKINNY_LBL_MONITOR,
	SKINNY_LBL_CALLBACK,
	SKINNY_LBL_BARGE,
	SKINNY_LBL_DND,
	SKINNY_LBL_CONFLIST,
	SKINNY_LBL_SELECT,
	SKINNY_LBL_PRIVATE,
	SKINNY_LBL_TRNSFVM,
	SKINNY_LBL_DIRTRFR,
	SKINNY_LBL_IDIVERT,
	SKINNY_LBL_VIDEO_MODE,
	SKINNY_LBL_INTRCPT,
	SKINNY_LBL_EMPTY,
	SKINNY_LBL_DIAL,
	//SKINNY_LBL_CBARGE,
};														/*!< SKINNY Soft Keys Map as INT */


/* =========================================================================================== Global */
/*!
 * \brief Global list of softkeys
 */
struct softKeySetConfigList softKeySetConfig;									/*!< List of SoftKeySets */

typedef enum {
	CB_KIND_LINEDEVICE = 0x0,
	CB_KIND_MAYBELINE = 0x1,
	CB_KIND_URIACTION = 0x2,
	//CB_KIND_CHANNEL = 0x3,
	//CB_KIND_DEVICE = 0x4,
} cb_kind_t;
/*!
 * \brief SCCP SoftKeyMap Callback
 *
 * Used to Map Softkeys to there Handling Implementation
 */
struct sccp_softkeyMap_cb {
	uint32_t event;
	boolean_t channelIsNecessary;
	cb_kind_t type;
	union {
		void (*LineDevice_cb) (constLineDevicePtr ld, channelPtr c);
		void (*MaybeLine_cb) (constDevicePtr d, constLinePtr l, const uint32_t lineInstance, channelPtr c);
		void (*UriAction_cb) (const sccp_softkeyMap_cb_t *const sccp_softkeyMap_cb, constLineDevicePtr ld, channelPtr c);
		//void (*Channel_cb) (channelPtr c);
		//void (*Device_cb) (constDevicePtr d, channelPtr c);
	};
	char *uriactionstr;
};

/*
 * \brief Helper to find the correct line to use
 * \returns retained line
 */
static const sccp_line_t * sccp_sk_get_retained_line(constDevicePtr d, constLinePtr l, const uint32_t lineInstance, constChannelPtr c, char *error_str) {
	const sccp_line_t *line = NULL;
	if (l && (line = sccp_line_retain(l))) {
		return line;
	}
	if (c && c->line && (line = sccp_line_retain(c->line))) {
		return line;
	}
	if (d && lineInstance && (line = sccp_line_find_byid(d, lineInstance))) {
		return line;
	}
	if (d && d->currentLine && (line = sccp_dev_getActiveLine(d))) {
		return line;
	}
	if (d && d->defaultLineInstance > 0 && (line = sccp_line_find_byid(d, d->defaultLineInstance))) {
		return line;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line found\n", DEV_ID_LOG(d));
	sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, lineInstance, c ? c->callid : 0, SKINNY_TONEDIRECTION_USER);
	sccp_dev_displayprompt(d, lineInstance, 0, error_str, SCCP_DISPLAYSTATUS_TIMEOUT);
	return NULL;
}

/* ============================================================================================================================ CB_KIND_MAYBELINE */
/*!
 * \brief Initiate a New Call
 */
static void sccp_sk_newcall(constDevicePtr d, constLinePtr l, const uint32_t lineInstance, channelPtr c)
{
	char *adhocNumber = NULL;
	sccp_speed_t k;
	AUTO_RELEASE(const sccp_line_t, line , sccp_sk_get_retained_line(d, l, lineInstance, c, SKINNY_DISP_NO_LINE_AVAILABLE));
	if (!line) {
		return;
	}

	uint8_t instance = sccp_device_find_index_for_line(d, line->name);
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey NewCall Pressed\n", DEV_ID_LOG(d));

	if (!line || instance != lineInstance) {
		/* handle dummy speeddial */
		sccp_dev_speed_find_byindex(d, lineInstance, TRUE, &k);
		if (sccp_strlen(k.ext) > 0) {
			adhocNumber = pbx_strdupa(k.ext);
		}
	}
	if (!adhocNumber && !sccp_strlen_zero(line->adhocNumber)) {
		adhocNumber = pbx_strdupa(line->adhocNumber);
	}

	/* check if we have an active channel on an other line, that does not have any dialed number 
	 * (Can't select line after already off-hook - https://sourceforge.net/p/chan-sccp-b/discussion/652060/thread/878fe455/?limit=25#c06e/6006/a54d) 
	 */
	AUTO_RELEASE(sccp_channel_t, activeChannel , NULL);
	if (!adhocNumber && (activeChannel = sccp_device_getActiveChannel(d))) {
		if (activeChannel->line != l && sccp_strlen(activeChannel->dialedNumber) == 0) {
			sccp_channel_endcall(activeChannel);
		}
	}
	/* done */

	AUTO_RELEASE(sccp_channel_t, new_channel , NULL);
	AUTO_RELEASE(sccp_linedevice_t, ld , sccp_linedevice_find(d, line));
	new_channel = sccp_channel_newcall(ld, adhocNumber,SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);		/* implicit release */
}

/*!
 * \brief Set DND on Current Line if Line is Active otherwise set on Device
 *
 * \todo The line param is not used 
 */
static void sccp_sk_dnd(constDevicePtr d, constLinePtr l, const uint32_t lineInstance, channelPtr c)
{
	if (!d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: sccp_sk_dnd function called without specifying a device\n");
		return;
	}

	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey DND Pressed (Current Status: %s, Feature enabled: %s)\n", DEV_ID_LOG(d), sccp_dndmode2str((sccp_dndmode_t)d->dndFeature.status), d->dndFeature.enabled ? "YES" : "NO");

	if (!d->dndFeature.enabled) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: SoftKey DND Feature disabled\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c ? c->callid : 0, SKINNY_DISP_DND " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, SKINNY_TONEDIRECTION_USER);
		return;
	}

	//AUTO_RELEASE(const sccp_line_t, line , sccp_sk_get_retained_line(d, l, lineInstance, c, SKINNY_DISP_NO_LINE_AVAILABLE));
	AUTO_RELEASE(const sccp_line_t, line , NULL);
	if (l) {
		line = sccp_line_retain(l);
	}
	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));
	if (device) {
		do {
			// check line/device dnd config flag, if found skip rest
			if (line) {
				if (line->dndmode == SCCP_DNDMODE_REJECT) {					// line config is set to: dnd=reject
					if (device->dndFeature.status == SCCP_DNDMODE_OFF) {
						device->dndFeature.status = SCCP_DNDMODE_REJECT;
					} else {
						device->dndFeature.status = SCCP_DNDMODE_OFF;
					}
					break;
				} else if (line->dndmode == SCCP_DNDMODE_SILENT) {				// line config is set to: dnd=silent
					if (device->dndFeature.status == SCCP_DNDMODE_OFF) {
						device->dndFeature.status = SCCP_DNDMODE_SILENT;
					} else {
						device->dndFeature.status = SCCP_DNDMODE_OFF;
					}
					break;
				}
			} else {
				if (device->dndmode == SCCP_DNDMODE_REJECT) {					// device config is set to: dnd=reject
					if (device->dndFeature.status == SCCP_DNDMODE_OFF) {
						device->dndFeature.status = SCCP_DNDMODE_REJECT;
					} else {
						device->dndFeature.status = SCCP_DNDMODE_OFF;
					}
					break;
				} else if (device->dndmode == SCCP_DNDMODE_SILENT) {				// device config is set to: dnd=silent
					if (device->dndFeature.status == SCCP_DNDMODE_OFF) {
						device->dndFeature.status = SCCP_DNDMODE_SILENT;
					} else {
						device->dndFeature.status = SCCP_DNDMODE_OFF;
					}
					break;
				}
			}
			// for all other config use the toggle mode
			switch (device->dndFeature.status) {
				case SCCP_DNDMODE_OFF:
					device->dndFeature.status = SCCP_DNDMODE_REJECT;
					break;
				case SCCP_DNDMODE_REJECT:
					device->dndFeature.status = SCCP_DNDMODE_SILENT;
					break;
				case SCCP_DNDMODE_SILENT:
					device->dndFeature.status = SCCP_DNDMODE_OFF;
					break;
				default:
					device->dndFeature.status = SCCP_DNDMODE_OFF;
					break;
			}
			sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey DND Pressed (New Status: %s, Feature enabled: %s)\n", DEV_ID_LOG(d), sccp_dndmode2str((sccp_dndmode_t)device->dndFeature.status), device->dndFeature.enabled ? "YES" : "NO");
		} while (0);

		sccp_feat_changed(device, NULL, SCCP_FEATURE_DND);							// notify the modules the the DND-feature changed state
		sccp_dev_check_displayprompt(device);									//! \todo we should use the feature changed event to check displayprompt
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey DND Pressed (New Status: %s, Feature enabled: %s)\n", DEV_ID_LOG(device), sccp_dndmode2str((sccp_dndmode_t)device->dndFeature.status), device->dndFeature.enabled ? "YES" : "NO");
	}
}

static void sccp_sk_private(constDevicePtr d, constLinePtr l, const uint32_t lineInstance, channelPtr c)
{
	AUTO_RELEASE(sccp_channel_t, channel , NULL);
	if (!d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: sccp_sk_private function called without specifying a device\n");
		return;
	}

	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Private Pressed\n", DEV_ID_LOG(d));

	if (!d->privacyFeature.enabled) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: private function is not active on this device\n", d->id);
		sccp_dev_displayprompt(d, lineInstance, 0, SKINNY_DISP_PRIVATE_FEATURE_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		return;
	}

	uint8_t instance = 0;
	if (c) {
		channel = sccp_channel_retain(c);
		instance = lineInstance;
	} else {
		AUTO_RELEASE(const sccp_line_t, line , sccp_sk_get_retained_line(d, l, lineInstance, c, SKINNY_DISP_PRIVATE_WITHOUT_LINE_CHANNEL));
		AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Creating new PRIVATE channel\n", d->id);
		if (line && device) {
			instance = sccp_device_find_index_for_line(device, line->name);
			sccp_dev_setActiveLine(device, line);
			sccp_dev_set_cplane(device, instance, 1);
			AUTO_RELEASE(sccp_linedevice_t, ld , sccp_linedevice_find(d, line));
			channel = sccp_channel_newcall(ld, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		}
	}

	if (!channel) {
		sccp_dev_displayprompt(d, lineInstance, 0, SKINNY_DISP_PRIVATE_WITHOUT_LINE_CHANNEL, SCCP_DISPLAYSTATUS_TIMEOUT);
		return;
	}
	// check device->privacyFeature.status before toggeling

	// toggle
	channel->privacy = (channel->privacy) ? FALSE : TRUE;

	// update device->privacyFeature.status  using sccp_feat_changed
	//sccp_feat_changed(d, NULL, SCCP_FEATURE_PRIVACY);

	// Should actually use the messageStack instead of using displayprompt directly
	if (channel->privacy) {
		//sccp_device_addMessageToStack(d, SCCP_MESSAGE_PRIORITY_PRIVACY, SKINNY_DISP_PRIVATE);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_PRIVATE, 300);			// replaced with 5 min instead of always, just to make sure we return
		sccp_channel_set_calleridPresentation(channel, CALLERID_PRESENTATION_FORBIDDEN);
	} else {
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_ENTER_NUMBER, 1);
		//sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_PRIVACY);
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Private %s on call %d\n", d->id, channel->privacy ? "enabled" : "disabled", channel->callid);
}

/*!
 * \brief Monitor Current Line
 */
static void sccp_sk_monitor(constDevicePtr d, constLinePtr l _U_, const uint32_t lineInstance _U_, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Monitor Pressed\n", DEV_ID_LOG(d));
	sccp_feat_monitor(d, c);
}

/* ============================================================================================================================ CB_KIND_LINEDEVICE */

/*!
 * \brief Redial last Dialed Number by this Device
 */
static void sccp_sk_redial(constDevicePtr d, constLinePtr l, const uint32_t lineInstance, channelPtr c)
{
	AUTO_RELEASE(const sccp_line_t, line , NULL);
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Redial Pressed\n", DEV_ID_LOG(d));
	if (!d) {
		return;
	}
	char *data;

	if (d->useRedialMenu) {
		if (d->protocol->type == SCCP_PROTOCOL) {
			if (d->protocolversion < 15) {
				data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Key:Directories\"/><ExecuteItem Priority=\"0\" URL=\"Key:KeyPad3\"/></CiscoIPPhoneExecute>";
			} else {
				data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Application:Cisco/PlacedCalls\"/></CiscoIPPhoneExecute>";
			}
		} else {
			data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Key:Setup\"/><ExecuteItem Priority=\"0\" URL=\"Key:KeyPad1\"/><ExecuteItem Priority=\"0\" URL=\"Key:KeyPad3\"/></CiscoIPPhoneExecute>";
			//data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Application:Cisco/PlacedCalls\"/></CiscoIPPhoneExecute>";
		}

		d->protocol->sendUserToDeviceDataVersionMessage(d, 0, lineInstance, 0, 0, data, 0);
		return;
	}

	if (sccp_strlen_zero(d->redialInformation.number)) {
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: No number to redial\n", d->id);
		return;
	}

	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Get ready to redial number %s lineInstance: %d\n", d->id, d->redialInformation.number, d->redialInformation.lineInstance ? d->redialInformation.lineInstance : lineInstance);
	if (c) {
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
			/* we have a offhook channel */
			sccp_copy_string(c->dialedNumber, d->redialInformation.number, sizeof(c->dialedNumber));
			sccp_pbx_softswitch(c);
		}
		/* here's a KEYMODE error. nothing to do */
		return;
	} 
	if (d->redialInformation.lineInstance == 0 || !(line = sccp_line_find_byid(d, d->redialInformation.lineInstance))) {
		line = sccp_sk_get_retained_line(d, l, lineInstance, c, SKINNY_DISP_NO_LINE_AVAILABLE);
	}
	if (line) {
		AUTO_RELEASE(sccp_channel_t, new_channel , NULL);
		AUTO_RELEASE(sccp_linedevice_t, ld , sccp_linedevice_find(d, line));
		new_channel = sccp_channel_newcall(ld, d->redialInformation.number, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);		/* implicit release */
	} else {
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Redial pressed on a device without a registered line\n", d->id);
	}
}

/*!
 * \brief Forces Dialling before timeout
 */
static void sccp_sk_dial(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Dial Pressed\n", DEV_ID_LOG(ld->device));
	if (c && !iPbx.getChannelPbx(c)) {									// Prevent dialling if in an inappropriate state.
		/* Only handle this in DIALING state. AFAIK GETDIGITS is used only for call forward and related input functions. (-DD) */
		if (c->state == SCCP_CHANNELSTATE_DIGITSFOLL || c->softswitch_action == SCCP_SOFTSWITCH_GETFORWARDEXTEN) {
			//sccp_pbx_softswitch(ld, c);
			sccp_pbx_softswitch(c);
		}
	}
}

/*!
 * \brief Start/Stop VideoMode
 *
 * \todo Add doxygen entry for sccp_sk_videomode
 * \todo Implement stopping video transmission
 */
static void sccp_sk_videomode(constLineDevicePtr ld, channelPtr c)
{
#ifdef CS_SCCP_VIDEO
	if (sccp_device_isVideoSupported(ld->device)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: We can have video, try to start vrtp\n", DEV_ID_LOG(ld->device));
		if (!c->rtp.video.instance && !sccp_rtp_createServer(ld->device, c, SCCP_RTP_VIDEO)) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can not start vrtp\n", DEV_ID_LOG(ld->device));
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: vrtp started\n", DEV_ID_LOG(ld->device));
			sccp_channel_startMultiMediaTransmission(c);
		}
	}
#endif
}


/*!
 * \brief Hold Call on Current Line
 */
static void sccp_sk_hold(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Hold Pressed\n", DEV_ID_LOG(ld->device));
	if (!c) {
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: No call to put on hold, check your softkeyset, hold should not be available in this situation.\n", DEV_ID_LOG(ld->device));
		sccp_dev_displayprompt(ld->device, ld->lineInstance, 0, SKINNY_DISP_NO_ACTIVE_CALL_TO_PUT_ON_HOLD, SCCP_DISPLAYSTATUS_TIMEOUT);
		return;
	}
	//sccp_channel_hold(ld, c);
	sccp_channel_hold(c);
}

/*!
 * \brief Resume Call on Current Line
 */
static void sccp_sk_resume(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Resume Pressed\n", DEV_ID_LOG(ld->device));
	if (!c) {
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: No call to resume. Ignoring\n", ld->device->id);
		return;
	}
	sccp_channel_resume(ld->device, c, TRUE);
}

/*!
 * \brief Transfer Call on Current Line
 *
 * \todo discus Marcello's transfer experiment
 */
static void sccp_sk_transfer(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Transfer Pressed\n", DEV_ID_LOG(ld->device));
	sccp_channel_transfer(c, ld->device);
}

/*!
 * \brief End Call on Current Line
 */
static void sccp_sk_endcall(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey EndCall Pressed\n", DEV_ID_LOG(ld->device));
	if (!c) {
		pbx_log(LOG_NOTICE, "%s: Endcall with no call in progress\n", ld->device->id);
		return;
	}

	sccp_device_t *d = ld->device;
	if (c->calltype == SKINNY_CALLTYPE_INBOUND && 1 < c->subscribers--) {
		if (d && d->indicate && d->indicate->onhook) {
			d->indicate->onhook(ld, c->callid);
		}
	} else {
		sccp_channel_endcall(c);
	}
}


/*!
 * \brief BackSpace Last Entered Number
 */
static void sccp_sk_backspace(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Backspace Pressed\n", DEV_ID_LOG(ld->device));
	int len;

	if (((c->state != SCCP_CHANNELSTATE_DIALING) && (c->state != SCCP_CHANNELSTATE_DIGITSFOLL) && (c->state != SCCP_CHANNELSTATE_OFFHOOK) && (c->state != SCCP_CHANNELSTATE_GETDIGITS)) || iPbx.getChannelPbx(c)) {
		return;
	}

	len = sccp_strlen(c->dialedNumber);

	/* we have no number, so nothing to process */
	if (!len) {
		sccp_channel_schedule_digittimout(c, GLOB(firstdigittimeout));
		return;
	}

	if (len >= 1) {
		c->dialedNumber[len - 1] = '\0';
		sccp_channel_schedule_digittimout(c, GLOB(digittimeout));
	}
	// sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: backspacing dial number %s\n", c->device->id, c->dialedNumber);
	sccp_handle_dialtone(ld, c);
	sccp_handle_backspace(ld, c->callid);
}

/*!
 * \brief Answer Incoming Call
 */
static void sccp_sk_answer(constLineDevicePtr ld, channelPtr c)
{
	if (!c) {
		char buf[100];
		snprintf(buf, 100, SKINNY_DISP_NO_CHANNEL_TO_PERFORM_XXXXXXX_ON " " SKINNY_GIVING_UP, "ANSWER");
		sccp_dev_displayprinotify(ld->device, buf, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
		sccp_dev_starttone(ld->device, SKINNY_TONE_BEEPBONK, ld->lineInstance, 0, SKINNY_TONEDIRECTION_USER);
		return;
	}
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Answer Pressed, instance: %d\n", DEV_ID_LOG(ld->device), ld->lineInstance);

	/* taking the reference during a locked ast channel allows us to call sccp_channel_answer unlock without the risk of loosing the channel */
	if (c->owner) {
		pbx_channel_lock(c->owner);
		PBX_CHANNEL_TYPE *pbx_channel = pbx_channel_ref(c->owner);
		pbx_channel_unlock(c->owner);

		if (pbx_channel) {
			sccp_channel_answer(ld->device, c);
			pbx_channel_unref(pbx_channel);
		}
	}
}

/*!
 * \brief Bridge two selected channels
 */
static void sccp_sk_dirtrfr(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Direct Transfer Pressed\n", DEV_ID_LOG(ld->device));

	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(ld->device));
	AUTO_RELEASE(sccp_channel_t, chan1, NULL);
	AUTO_RELEASE(sccp_channel_t, chan2, NULL);
	if ((sccp_device_selectedchannels_count(device)) == 2) {
		sccp_selectedchannel_t *x = NULL;
		SCCP_LIST_LOCK(&device->selectedChannels);
		x = SCCP_LIST_FIRST(&device->selectedChannels);
		chan1 = sccp_channel_retain(x->channel);
		chan2 = SCCP_LIST_NEXT(x, list)->channel;
		chan2 = sccp_channel_retain(chan2);
		SCCP_LIST_UNLOCK(&device->selectedChannels);
	} else {
		AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(ld->line));
		if (line) {
			if (SCCP_RWLIST_GETSIZE(&line->channels) == 2) {
				SCCP_LIST_LOCK(&line->channels);
				sccp_channel_t *tmp = NULL;
				if ((tmp  = SCCP_LIST_FIRST(&line->channels))) {
					chan1 = sccp_channel_retain(tmp);
					if ((tmp = SCCP_LIST_NEXT(tmp, list))) {
						chan2 = sccp_channel_retain(tmp);
					}
				}
				SCCP_LIST_UNLOCK(&line->channels);
			} else if (SCCP_RWLIST_GETSIZE(&line->channels) < 2) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Not enough channels to transfer\n", device->id);
				sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_NOT_ENOUGH_CALLS_TO_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: More than 2 channels to transfer, please use softkey select\n", device->id);
				sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_MORE_THAN_TWO_CALLS ", " SKINNY_DISP_USE " " SKINNY_DISP_SELECT, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}


	if (chan1 && chan2) {
		//for using the sccp_channel_transfer_complete function
		//chan2 must be in RINGOUT or CONNECTED state
		sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_CALL_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) First Channel Status (%d), Second Channel Status (%d)\n", DEV_ID_LOG(device), chan1->state, chan2->state);
		if (chan2->state != SCCP_CHANNELSTATE_CONNECTED && chan1->state == SCCP_CHANNELSTATE_CONNECTED) {
			/* reverse channels */
			sccp_channel_t *tmp = NULL;
			tmp = chan1;
			chan1 = chan2;
			chan2 = tmp;
		} else if (chan1->state == SCCP_CHANNELSTATE_HOLD && chan2->state == SCCP_CHANNELSTATE_HOLD) {
			//resume chan2 if both channels are on hold
			sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) Resuming Second Channel (%d)\n", DEV_ID_LOG(device), chan2->state);
			sccp_channel_resume(device, chan2, TRUE);
			sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) Resumed Second Channel (%d)\n", DEV_ID_LOG(device), chan2->state);
		}
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) First Channel Status (%d), Second Channel Status (%d)\n", DEV_ID_LOG(device), chan1->state, chan2->state);
		device->transferChannels.transferee = sccp_channel_retain(chan1);
		device->transferChannels.transferer = sccp_channel_retain(chan2);
		if (device->transferChannels.transferee && device->transferChannels.transferer) {
			sccp_channel_transfer_complete(chan2);
		}
	}
}

/*!
 * \brief Select a Line for further processing by for example DirTrfr
 */
static void sccp_sk_select(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Select Pressed\n", DEV_ID_LOG(ld->device));
	sccp_selectedchannel_t *selectedchannel = NULL;
	sccp_msg_t *msg = NULL;
	uint8_t numSelectedChannels = 0, status = 0;

	if (!c) {
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_select) No channel to be selected\n", DEV_ID_LOG(ld->device));
		return;
	}

	AUTO_RELEASE(sccp_device_t, device, sccp_device_retain(ld->device));
	if (device) {
		if ((selectedchannel = sccp_device_find_selectedchannel(device, c))) {
			SCCP_LIST_LOCK(&device->selectedChannels);
			selectedchannel = SCCP_LIST_REMOVE(&device->selectedChannels, selectedchannel, list);
			SCCP_LIST_UNLOCK(&device->selectedChannels);
			sccp_channel_release(&selectedchannel->channel);
			sccp_free(selectedchannel);
		} else {
			selectedchannel = (sccp_selectedchannel_t *) sccp_calloc(sizeof *selectedchannel, 1);
			if (selectedchannel != NULL) {
				selectedchannel->channel = sccp_channel_retain(c);
				SCCP_LIST_LOCK(&device->selectedChannels);
				SCCP_LIST_INSERT_HEAD(&device->selectedChannels, selectedchannel, list);
				SCCP_LIST_UNLOCK(&device->selectedChannels);
				status = 1;
			} else {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return;
			}	
		}
		numSelectedChannels = sccp_device_selectedchannels_count(device);
		
		sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_sk_select) '%d' channels selected\n", DEV_ID_LOG(device), numSelectedChannels);

		REQ(msg, CallSelectStatMessage);
		msg->data.CallSelectStatMessage.lel_status = htolel(status);
		msg->data.CallSelectStatMessage.lel_lineInstance = htolel(ld->lineInstance);
		msg->data.CallSelectStatMessage.lel_callReference = htolel(c->callid);
		sccp_dev_send(device, msg);
	}
}

/*!
 * \brief Set Call Forward All on Current Line
 */
static void sccp_sk_cfwdall(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Call Forward All Pressed, line: %s, instance: %d, channel: %d\n", DEV_ID_LOG(ld->device), ld->line->name, ld->lineInstance, c ? c->callid : 0);

	if (ld->device->cfwdall) {
		sccp_feat_handle_callforward(ld, SCCP_CFWD_ALL);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDALL disabled on device\n", ld->device->id);
	sccp_dev_displayprompt(ld->device, ld->lineInstance, 0, SKINNY_DISP_CFWDALL " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_dev_starttone(ld->device, SKINNY_TONE_BEEPBONK, ld->lineInstance, 0, SKINNY_TONEDIRECTION_USER);
}

/*!
 * \brief Set Call Forward when Busy on Current Line
 */
static void sccp_sk_cfwdbusy(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Call Forward Busy Pressed, line: %s, instance: %d, channel: %d\n", DEV_ID_LOG(ld->device), ld->line->name, ld->lineInstance, c ? c->callid : 0);

	if (ld->device->cfwdbusy) {
		sccp_feat_handle_callforward(ld, SCCP_CFWD_BUSY);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY disabled on device\n", ld->device->id);
	sccp_dev_displayprompt(ld->device, ld->lineInstance, 0, SKINNY_DISP_CFWDBUSY " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_dev_starttone(ld->device, SKINNY_TONE_BEEPBONK, ld->lineInstance, 0, SKINNY_TONEDIRECTION_USER);
}

/*!
 * \brief Set Call Forward when No Answer on Current Line
 */
static void sccp_sk_cfwdnoanswer(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Call Forward NoAnswer Pressed, line: %s, instance: %d, channel: %d\n", DEV_ID_LOG(ld->device), ld->line->name, ld->lineInstance, c ? c->callid : 0);

	if (ld->device->cfwdnoanswer) {
		sccp_feat_handle_callforward(ld, SCCP_CFWD_ALL);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER disabled on device\n", ld->device->id);
	sccp_dev_displayprompt(ld->device, ld->lineInstance, 0, SKINNY_DISP_CFWDNOANSWER " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_dev_starttone(ld->device, SKINNY_TONE_BEEPBONK, ld->lineInstance, 0, SKINNY_TONEDIRECTION_USER);
}

/*!
 * \brief Park Call on Current Line
 */
static void sccp_sk_park(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Park Pressed\n", DEV_ID_LOG(ld->device));
#ifdef CS_SCCP_PARK
	//sccp_channel_park(ld, c);
	sccp_channel_park(c);
#else
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
}


/*!
 * \brief Transfer to VoiceMail on Current Line
 */
static void sccp_sk_trnsfvm(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Transfer to Voicemail Pressed\n", DEV_ID_LOG(ld->device));
	sccp_feat_idivert(ld, c);		//! \note using idivert to implement this function (sccp_feat_trnsfvm(ld, c) does not (yet) exist)
}

/*!
 * \brief Transfer to VoiceMail on Current Line
 */
static void sccp_sk_idivert(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey iDivert Pressed\n", DEV_ID_LOG(ld->device));
	sccp_feat_idivert(ld, c);
}

/*!
 * \brief Initiate Private Call on Current Line
 *
 */

/*!
 * \brief Put Current Line into Conference
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
static void sccp_sk_conference(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Conference Pressed\n", DEV_ID_LOG(ld->device));
#ifdef CS_SCCP_CONFERENCE
	sccp_feat_handle_conference(ld, c);
#else
	sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Conference was not compiled in\n");
#endif
}

/*!
 * \brief Show Participant List of Current Conference
 * \todo Conferencing option is under development.
 */
static void sccp_sk_conflist(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Conflist Pressed\n", DEV_ID_LOG(ld->device));
#ifdef CS_SCCP_CONFERENCE
	sccp_feat_conflist(ld, c);
#else
	sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Conference was not compiled in\n");
#endif
}

/*!
 * \brief Join Current Line to Conference
 * \todo Conferencing option needs to be build and implemented
 */
static void sccp_sk_join(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Join Pressed\n", DEV_ID_LOG(ld->device));
#ifdef CS_SCCP_CONFERENCE
	sccp_feat_join(ld, c);
#else
	sccp_dev_displayprompt(ld->device, ld->lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Conference was not compiled in\n");
#endif
}

/*!
 * \brief Barge into Call on the Current Line
 */
static void sccp_sk_barge(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Barge Pressed\n", DEV_ID_LOG(ld->device));
	sccp_feat_handle_barge(ld, c);
}

/*!
 * \brief Barge into Call on the Current Line
 */
static void sccp_sk_cbarge(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey cBarge Pressed\n", DEV_ID_LOG(ld->device));
	sccp_feat_handle_cbarge(ld, c);
}

/*!
 * \brief Put Current Line in to Meetme Conference
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
static void sccp_sk_meetme(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Meetme Pressed\n", DEV_ID_LOG(ld->device));
	sccp_feat_handle_meetme(ld, c);
}

/*!
 * \brief Pickup Parked Call
 */
static void sccp_sk_pickup(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Pickup Pressed\n", ld->device->id);
#ifndef CS_SCCP_PICKUP
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Native EXTENSION PICKUP was not compiled in\n");
#else
	AUTO_RELEASE(const sccp_device_t, call_assoc_device, (c ? c->getDevice(c) : NULL));
	if (!call_assoc_device || call_assoc_device == ld->device) {
		sccp_feat_handle_directed_pickup((lineDevicePtr)ld, c);				// discard const
		return;
	}
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: there is already a call:%s present on this (shared)line:%s. Skipping request\n", ld->device->id, c ? c->designator : "SCCP", ld->line->name);
#endif
}

/*!
 * \brief Pickup Ringing Line from Pickup Group
 */
static void sccp_sk_gpickup(constLineDevicePtr ld, channelPtr c)
{
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Group Pickup Pressed\n", ld->device->id);
#ifndef CS_SCCP_PICKUP
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "### Native GROUP PICKUP was not compiled in\n");
#else
	AUTO_RELEASE(const sccp_device_t, call_assoc_device, (c ? c->getDevice(c) : NULL));
	if (!call_assoc_device || call_assoc_device == ld->device) {
		sccp_feat_grouppickup((lineDevicePtr)ld, c);					// discard const
		return;
	}
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: there is already a call:%s present on this (shared)line:%s. Skipping request\n", ld->device->id, c ? c->designator : "SCCP", ld->line->name);
#endif
}

/* ============================================================================================================================ Remappable Types */
/*!
 * \brief Execute URI(s) 
 */
static void sccp_sk_uriaction(const sccp_softkeyMap_cb_t *const softkeyMap_cb, constLineDevicePtr ld, channelPtr c)
{
	if (!ld) {
		return;
	}
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: SoftKey Pressed\n", DEV_ID_LOG(ld->device));
	unsigned int transactionID = sccp_random();

	/* build parameters */
	struct ast_str *paramStr = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	if (!paramStr) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}
	ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "name=%s", ld->device->id);
	ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;softkey=%s", label2str(softkeyMap_cb->event));
	if (ld->line) {
		ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;line=%s", ld->line->name);
	}
	if (ld->lineInstance) {
		ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;lineInstance=%d", ld->lineInstance);
	}
	if (c) {
		ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;channel=%s", c->designator);
		ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;callid=%d", c->callid);
		if (c->owner) {
			ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;linkedid=%s", iPbx.getChannelLinkedId(c));
			ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;uniqueid=%s", pbx_channel_uniqueid(c->owner));
		}
	}
	ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;appID=%d", APPID_URIHOOK);
	ast_str_append(&paramStr, DEFAULT_PBX_STR_BUFFERSIZE, "&amp;transactionID=%d", transactionID);

	/* build xmlStr */
	struct ast_str *xmlStr = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	if (!xmlStr) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}

	ast_str_append(&xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "%s", "<CiscoIPPhoneExecute>");

	char delims[] = ",";
	char *uris = pbx_strdupa(softkeyMap_cb->uriactionstr);
	char *tokenrest = NULL;
	char *token = strtok_r(uris, delims, &tokenrest);

	while (token) {
		token = sccp_trimwhitespace(token);
		if (!strncasecmp("http:", token, 5)) {
			if (!strchr(token, '?')) {
				ast_str_append(&xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<ExecuteItem Priority=\"0\" URL=\"%s?%s\"/>", token, pbx_str_buffer(paramStr));
			} else {
				ast_str_append(&xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<ExecuteItem Priority=\"0\" URL=\"%s&amp;%s\"/>", token, pbx_str_buffer(paramStr));
			}
		} else {
			ast_str_append(&xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<ExecuteItem Priority=\"0\" URL=\"%s\"/>", token);
		}
		token = strtok_r(NULL, delims, &tokenrest);
	}
	ast_str_append(&xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "%s", "</CiscoIPPhoneExecute>");

	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Send '%s' to Phone\n", DEV_ID_LOG(ld->device), pbx_str_buffer(xmlStr));
	ld->device->protocol->sendUserToDeviceDataVersionMessage(ld->device, APPID_URIHOOK, ld->lineInstance, c ? c->callid : 0, transactionID, pbx_str_buffer(xmlStr), 0);
}

/* ============================================================================================================================ Mapping */
/*!
 * \brief Softkey Function Callback by SKINNY LABEL
 */
static const struct sccp_softkeyMap_cb softkeyCbMap[] = {
	{SKINNY_LBL_NEWCALL, FALSE, CB_KIND_MAYBELINE, .MaybeLine_cb = sccp_sk_newcall, NULL},
	{SKINNY_LBL_REDIAL, FALSE, CB_KIND_MAYBELINE, .MaybeLine_cb = sccp_sk_redial, NULL},
	{SKINNY_LBL_DND, FALSE, CB_KIND_MAYBELINE, .MaybeLine_cb = sccp_sk_dnd, NULL},
	{SKINNY_LBL_PRIVATE, FALSE, CB_KIND_MAYBELINE, .MaybeLine_cb = sccp_sk_private, NULL},
	{SKINNY_LBL_MONITOR, TRUE, CB_KIND_MAYBELINE, .MaybeLine_cb = sccp_sk_monitor, NULL},
	
	{SKINNY_LBL_HOLD, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_hold, NULL},
	{SKINNY_LBL_TRANSFER, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_transfer, NULL},
	{SKINNY_LBL_DIRTRFR, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_dirtrfr, NULL},
	{SKINNY_LBL_RESUME, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_resume, NULL},
	{SKINNY_LBL_INTRCPT, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_resume /*intercept*/, NULL},
	{SKINNY_LBL_MEETME, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_meetme, NULL},
	{SKINNY_LBL_BARGE, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_barge, NULL},
	{SKINNY_LBL_CBARGE, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_cbarge, NULL},
	{SKINNY_LBL_CONFRN, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_conference, NULL},
	{SKINNY_LBL_JOIN, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_join, NULL},
	{SKINNY_LBL_CONFLIST, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_conflist, NULL},
	{SKINNY_LBL_PICKUP, FALSE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_pickup, NULL},
	{SKINNY_LBL_GPICKUP, FALSE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_gpickup, NULL},
	{SKINNY_LBL_BACKSPACE, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_backspace, NULL},
	{SKINNY_LBL_ANSWER, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_answer, NULL},
	{SKINNY_LBL_TRNSFVM, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_trnsfvm, NULL},
	{SKINNY_LBL_IDIVERT, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_idivert, NULL},
	{SKINNY_LBL_PARK, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_park, NULL},
	{SKINNY_LBL_DIAL, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_dial, NULL},
	{SKINNY_LBL_SELECT, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_select, NULL},
	{SKINNY_LBL_CFWDALL, FALSE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_cfwdall, NULL},
	{SKINNY_LBL_CFWDBUSY, FALSE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_cfwdbusy, NULL},
	{SKINNY_LBL_CFWDNOANSWER, FALSE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_cfwdnoanswer, NULL},
	{SKINNY_LBL_ENDCALL, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_endcall, NULL},
	{SKINNY_LBL_VIDEO_MODE, TRUE, CB_KIND_LINEDEVICE, .LineDevice_cb = sccp_sk_videomode, NULL},
};

/*!
 * \brief Get SoftkeyMap by Event
 */
gcc_inline static const sccp_softkeyMap_cb_t *sccp_getSoftkeyMap_by_SoftkeyEvent(constDevicePtr d, uint32_t event)
{
	uint8_t i;

	const sccp_softkeyMap_cb_t *mySoftkeyCbMap = softkeyCbMap;

	if (d->softkeyset && d->softkeyset->softkeyCbMap) {
		mySoftkeyCbMap = d->softkeyset->softkeyCbMap;
	}
	sccp_log(DEBUGCAT_SOFTKEY) (VERBOSE_PREFIX_3 "%s: (sccp_getSoftkeyMap_by_SoftkeyEvent) default: %p, softkeyset: %p, softkeyCbMap: %p\n", d->id, softkeyCbMap, d->softkeyset, d->softkeyset ? d->softkeyset->softkeyCbMap : NULL);

	for (i = 0; i < ARRAY_LEN(softkeyCbMap); i++) {
		if (mySoftkeyCbMap[i].event == event) {
			return &mySoftkeyCbMap[i];
		}
	}
	return NULL;
}

/* =========================================================================================== Public */

/*!
 * \brief Softkey Pre Reload
 *
 */
void sccp_softkey_pre_reload(void)
{
	sccp_softkey_clear();
}

/*!
 * \brief Softkey Post Reload
 */
void sccp_softkey_post_reload(void)
{
	/* only required because softkeys are parsed after devices */
	/* incase softkeysets have changed but device was not reloaded, then d->softkeyset needs to be fixed up */
	sccp_softKeySetConfiguration_t *softkeyset = NULL;
	sccp_softKeySetConfiguration_t *default_softkeyset = NULL;
	sccp_device_t *d = NULL;
	
	SCCP_LIST_LOCK(&softKeySetConfig);
	SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
		if (sccp_strcaseequals("default", softkeyset->name)) {
                	default_softkeyset = softkeyset;
                }
	}
	SCCP_LIST_UNLOCK(&softKeySetConfig);
	
	if (!default_softkeyset) {
		pbx_log(LOG_ERROR, "SCCP: 'default' softkeyset could not be found. Something is horribly wrong here\n");
	}

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
			if (sccp_strcaseequals(d->softkeyDefinition, softkeyset->name)) {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Re-attaching softkeyset: %s to device d: %s\n", softkeyset->name, d->id);
				d->softkeyset = softkeyset;
				d->softKeyConfiguration.modes = softkeyset->modes;
				d->softKeyConfiguration.size = softkeyset->numberOfSoftKeySets;
			}
		}
		SCCP_LIST_UNLOCK(&softKeySetConfig);
		
		if (default_softkeyset && !d->softkeyset) {
			d->softkeyset = default_softkeyset;
			d->softKeyConfiguration.modes = default_softkeyset->modes;
			d->softKeyConfiguration.size = default_softkeyset->numberOfSoftKeySets;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
}

/*!
 * \brief Softkey Clear (Unload/Reload)
 */
void sccp_softkey_clear(void)
{
	sccp_softKeySetConfiguration_t *k;
	uint8_t i;

	SCCP_LIST_LOCK(&softKeySetConfig);
	while ((k = SCCP_LIST_REMOVE_HEAD(&softKeySetConfig, list))) {
		for (i = 0; i < StationMaxSoftKeySetDefinition; i++) {
			if (k->modes[i].ptr) {
				//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Freeing KeyMode Ptr: %p for KeyMode %i\n", k->modes[i].ptr, i);
				sccp_free(k->modes[i].ptr);
				k->modes[i].count = 0;
			}
		}
		if (k->softkeyCbMap) {
			for (i = 0; i < ARRAY_LEN(softkeyCbMap); i++) {
				if (!sccp_strlen_zero(k->softkeyCbMap[i].uriactionstr)) {
					sccp_free(k->softkeyCbMap[i].uriactionstr);
				}
			}
			sccp_free(k->softkeyCbMap);
		}
		//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Softkeyset: %s removed\n", k->name);
		sccp_free(k);
	}
	SCCP_LIST_UNLOCK(&softKeySetConfig);
}

/*!
 * \brief Return a Copy of the statically defined SoftkeyMap
 * \note malloc, needs to be freed
 */
sccp_softkeyMap_cb_t __attribute__ ((malloc)) * sccp_softkeyMap_copyStaticallyMapped(void)
{
	sccp_softkeyMap_cb_t *newSoftKeyMap = (sccp_softkeyMap_cb_t *) sccp_malloc((sizeof *newSoftKeyMap) * ARRAY_LEN(softkeyCbMap));
	if (!newSoftKeyMap) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}
	memcpy(newSoftKeyMap, softkeyCbMap, ARRAY_LEN(softkeyCbMap) * sizeof(sccp_softkeyMap_cb_t));
	sccp_log(DEBUGCAT_SOFTKEY) (VERBOSE_PREFIX_3 "SCCP: (sccp_softkeyMap_copyIfStaticallyMapped) Created copy of static version, returning: %p\n", newSoftKeyMap);
	return newSoftKeyMap;
}

/*!
 * \brief Replace a specific softkey callback entry by sccp_sk_uriaction and fill it's uriactionstr
 * \note pbx_strdup, needs to be freed
 */
boolean_t sccp_softkeyMap_replaceCallBackByUriAction(sccp_softkeyMap_cb_t * const softkeyMap, uint32_t event, char *uriactionstr)
{
	uint i;

	sccp_log(DEBUGCAT_SOFTKEY) (VERBOSE_PREFIX_3 "SCCP: (sccp_softkeyMap_replaceCallBackByUriHook) %p, event: %s, uriactionstr: %s\n", softkeyMap, label2str(event), uriactionstr);
	for (i = 0; i < ARRAY_LEN(softkeyCbMap); i++) {
		if (event == softkeyMap[i].event) {
			softkeyMap[i].type = CB_KIND_URIACTION;
			softkeyMap[i].UriAction_cb = sccp_sk_uriaction;
			softkeyMap[i].uriactionstr = pbx_strdup(sccp_trimwhitespace(uriactionstr));
			return TRUE;
		}
	}
	return FALSE;
}

/* ============================================================================================================================ Execute CallBack */
/*!
 * \brief Execute Softkey Callback by SofkeyEvent
 */
boolean_t sccp_SoftkeyMap_execCallbackByEvent(devicePtr d, linePtr l, uint32_t lineInstance, channelPtr c, uint32_t event)
{
	if (!d || !event) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_execSoftkeyMapCb_by_SoftkeyEvent) no device or event provided\n");
		return FALSE;
	}
	const sccp_softkeyMap_cb_t *softkeyMap_cb = sccp_getSoftkeyMap_by_SoftkeyEvent(d, event);

	if (!softkeyMap_cb) {
		pbx_log(LOG_WARNING, "%s: Don't know how to handle keypress %d\n", d->id, event);
		return FALSE;
	}
	if (softkeyMap_cb->channelIsNecessary == TRUE && !c) {
		pbx_log(LOG_WARNING, "%s: Channel required to handle keypress %d\n", d->id, event);
		return FALSE;
	}
	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Handling Softkey: %s on line: %s and channel: %s\n", d->id, label2str(event), l ? l->name : "UNDEF", c ? sccp_channel_toString(c) : "UNDEF");
	//! \todo tie the device to the channel, we know the user pressed a button on a particular device
	/*
	if (c) {
		AUTO_RELEASE(sccp_device_t, tmpdevice , sccp_channel_getDevice(c));
		if (!tmpdevice) {
			sccp_channel_setDevice(c, d);
		}
	}
	*/
	AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_findByLineInstance(d, lineInstance));
	switch (softkeyMap_cb->type){
		case CB_KIND_LINEDEVICE:
			softkeyMap_cb->LineDevice_cb(ld, c);
			break;
		case CB_KIND_MAYBELINE:
			softkeyMap_cb->MaybeLine_cb(d, l, lineInstance, c);
			break;
		case CB_KIND_URIACTION:
			softkeyMap_cb->UriAction_cb(softkeyMap_cb, ld, c);
			break;
/*
		case CB_KIND_CHANNEL:
			softkeyMap_cb->Channel_cb(c);
			break;
		case CB_KIND_DEVICE:
			softkeyMap_cb->Device_cb(d, c);
			break;
*/
	}
	return TRUE;
}

/* ============================================================================================================================ Get/Set SoftKeyState */
/*!
 * \brief Enable or Disable one softkey on a specific softKeySet
 */
void sccp_softkey_setSoftkeyState(devicePtr device, skinny_keymode_t softKeySet, uint8_t softKey, boolean_t enable)
{
	uint8_t i;

	if (!device || !device->softKeyConfiguration.size) {
		return;
	}

	sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: softkey '%s' on %s to %s\n", DEV_ID_LOG(device), label2str(softKey), skinny_keymode2str(softKeySet), enable ? "on" : "off");
	/* find softkey */
	for (i = 0; i < device->softKeyConfiguration.modes[softKeySet].count; i++) {
		if (device->softKeyConfiguration.modes[softKeySet].ptr && device->softKeyConfiguration.modes[softKeySet].ptr[i] == softKey) {
			sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_4 "%s: found softkey '%s' at %d\n", DEV_ID_LOG(device), label2str(device->softKeyConfiguration.modes[softKeySet].ptr[i]), i);
			if (enable) {
				device->softKeyConfiguration.activeMask[softKeySet] |= (1 << i);
			} else {
				device->softKeyConfiguration.activeMask[softKeySet] &= (~(1 << i));
			}
		}
	}
}

boolean_t __PURE__ sccp_softkey_isSoftkeyInSoftkeySet(constDevicePtr device, const skinny_keymode_t softKeySet, const uint8_t softKey)
{
	uint8_t i;

	if (!device || !device->softKeyConfiguration.size) {
		return FALSE;
	}

	/* find softkey */
	for (i = 0; i < device->softKeyConfiguration.modes[softKeySet].count; i++) {
		if (device->softKeyConfiguration.modes[softKeySet].ptr && device->softKeyConfiguration.modes[softKeySet].ptr[i] == softKey) {
			return TRUE;
		}
	}
	return FALSE;
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
