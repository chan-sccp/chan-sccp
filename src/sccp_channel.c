/*!
 * \file        sccp_channel.c
 * \brief       SCCP Channel Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks
 * Purpose:     SCCP Channels
 * When to use: Only methods directly related to sccp channels should be stored in this source file.
 * Relations:   SCCP Channels connect Asterisk Channels to SCCP Lines
 */

#include <config.h>
#include "common.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_channel.h"
#include "sccp_utils.h"
#include "sccp_conference.h"
#include "sccp_features.h"
#include "sccp_line.h"
#include "sccp_indicate.h"
#include "sccp_rtp.h"
#include "sccp_socket.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");
static uint32_t callCount = 1;
void __sccp_channel_destroy(sccp_channel_t * channel);

AST_MUTEX_DEFINE_STATIC(callCountLock);

/*!
 * \brief Private Channel Data Structure
 */
struct sccp_private_channel_data {
	sccp_device_t *device;
	boolean_t microphone;											/*!< Flag to mute the microphone when calling a baby phone */
};

/*!
 * \brief Helper Function to set to FALSE
 * \return FALSE
 */
static boolean_t sccp_always_false(void)
{
	return FALSE;
}

/*!
 * \brief Helper Function to set to TRUE
 * \return TRUE
 */
static boolean_t sccp_always_true(void)
{
	return TRUE;
}

/*!
 * \brief Set Microphone State
 * \param channel SCCP Channel
 * \param enabled Enabled as Boolean
 */
static void sccp_channel_setMicrophoneState(sccp_channel_t * channel, boolean_t enabled)
{
#if !CS_EXPERIMENTAL
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

	if (!c) {
		return;
	}
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}

	c->privateData->microphone = enabled;

	switch (enabled) {
		case TRUE:
			c->isMicrophoneEnabled = sccp_always_true;
			if ((c->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
				sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
			}

			break;
		case FALSE:
			c->isMicrophoneEnabled = sccp_always_false;
			if ((c->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
				sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
			}
			break;
	}
#else														/* show how WITHREF / GETWITHREF would/could work */
	sccp_device_t *d = NULL;

	WITHREF(channel) {
		GETWITHREF(d, channel->privateData->device) {
			channel->privateData->microphone = enabled;
			pbx_log(LOG_NOTICE, "Within retain section\n");
			switch (enabled) {
				case TRUE:
					channel->isMicrophoneEnabled = sccp_always_true;
					if ((channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
						sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
					}

					break;
				case FALSE:
					channel->isMicrophoneEnabled = sccp_always_false;
					if ((channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
						sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
					}
					break;
			}
		}
	}
#endif
}

/*!
 * \brief Allocate SCCP Channel on Device
 * \param l SCCP Line
 * \param device SCCP Device
 * \return a *retained* SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *  - callCountLock
 */
sccp_channel_t *sccp_channel_allocate(sccp_line_t * l, sccp_device_t * device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t *channel;
	char designator[CHANNEL_DESIGNATOR_SIZE];
	struct sccp_private_channel_data *private_data;
	AUTO_RELEASE sccp_line_t *line = sccp_line_retain(l);
	int32_t callid;

	/* If there is no current line, then we can't make a call in, or out. */
	if (!line) {
		pbx_log(LOG_ERROR, "SCCP: Tried to open channel on a device with no lines\n");
		return NULL;
	}

	if (device && !device->session) {
		pbx_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", device->id);
		return NULL;
	}
	sccp_mutex_lock(&callCountLock);
	callid = callCount++;
	/* callcount limit should be reset at his upper limit :) */
	if (callCount == 0xFFFFFFFF) {
		pbx_log(LOG_NOTICE, "%s: CallId re-starting at 00000001 again (RollOver)\n", device->id);
		callCount = 1;
	}
	snprintf(designator, CHANNEL_DESIGNATOR_SIZE, "SCCP/%s-%08X", l->name, callid);
	sccp_mutex_unlock(&callCountLock);

	channel = (sccp_channel_t *) sccp_refcount_object_alloc(sizeof(sccp_channel_t), SCCP_REF_CHANNEL, designator, __sccp_channel_destroy);
	if (!channel) {
		/* error allocating memory */
		pbx_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n", l->id, l->name);
		return NULL;
	}
	memset(channel, 0, sizeof(sccp_channel_t));
	//ast_mutex_init(&channel->lock);
	sccp_copy_string(channel->designator, designator, sizeof(channel->designator));

	private_data = sccp_malloc(sizeof(struct sccp_private_channel_data));
	if (!private_data) {
		/* error allocating memory */
		pbx_log(LOG_ERROR, "%s: No memory to allocate channel private data on line %s\n", l->id, l->name);
		channel = sccp_channel_release(channel);
		return NULL;
	}
	memset(private_data, 0, sizeof(struct sccp_private_channel_data));
	channel->privateData = private_data;
	channel->privateData->microphone = TRUE;
	channel->privateData->device = NULL;

	channel->line = sccp_line_retain(line);

	/* this is for dialing scheduler */
	channel->scheduler.digittimeout = -1;
	channel->enbloc.digittimeout = GLOB(digittimeout);

	PBX(set_owner) (channel, NULL);
	/* default ringermode SKINNY_RINGTYPE_OUTSIDE. Change it with SCCPRingerMode app */
	channel->ringermode = SKINNY_RINGTYPE_OUTSIDE;
	/* inbound for now. It will be changed later on outgoing calls */
	channel->calltype = SKINNY_CALLTYPE_INBOUND;
	channel->answered_elsewhere = FALSE;

	/* by default we allow callerid presentation */
	channel->callInfo.presentation = CALLERID_PRESENCE_ALLOWED;

	channel->callid = callid;
	channel->passthrupartyid = callid ^ 0xFFFFFFFF;

	channel->peerIsSCCP = 0;
	channel->maxBitRate = 15000;
	channel->videomode = SCCP_VIDEO_MODE_AUTO;

	sccp_channel_setDevice(channel, device);
	sccp_line_addChannel(l, channel);

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, channel->callid, l->name);
#if DEBUG
	channel->getDevice_retained = __sccp_channel_getDevice_retained;
#else
	channel->getDevice_retained = sccp_channel_getDevice_retained;
#endif
	channel->setDevice = sccp_channel_setDevice;
	if (device) {
		channel->dtmfmode = device->getDtmfMode(device);
	} else {
		channel->dtmfmode = SCCP_DTMFMODE_RFC2833;
	}

	channel->isMicrophoneEnabled = sccp_always_true;
	channel->setMicrophone = sccp_channel_setMicrophoneState;
	channel->hangupRequest = sccp_wrapper_asterisk_requestHangup;
	//sccp_rtp_createAudioServer(channel);
#ifndef SCCP_ATOMIC
	ast_mutex_init(&channel->scheduler.lock);
#endif
	return channel;
}

#if DEBUG
/*!
 * \brief Retrieve Device from Channels->Private Channel Data
 * \param channel SCCP Channel
 * \param filename Debug Filename
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Device
 */
sccp_device_t *__sccp_channel_getDevice_retained(const sccp_channel_t * channel, const char *filename, int lineno, const char *func)
#else
/*!
 * \brief Retrieve Device from Channels->Private Channel Data
 * \param channel SCCP Channel
 * \return SCCP Device
 */
sccp_device_t *sccp_channel_getDevice_retained(const sccp_channel_t * channel)
#endif
{
	ast_assert(channel != NULL);
	if (channel->privateData && channel->privateData->device) {
#if DEBUG
		channel->privateData->device = sccp_refcount_retain((sccp_device_t *) channel->privateData->device, filename, lineno, func);
#else
		channel->privateData->device = sccp_device_retain((sccp_device_t *) channel->privateData->device);
#endif
		return (sccp_device_t *) channel->privateData->device;
	} else {
		return NULL;
	}
}

/*!
 * \brief Set Device in Channels->Private Channel Data
 * \param channel SCCP Channel
 * \param device SCCP Device
 */
void sccp_channel_setDevice(sccp_channel_t * channel, const sccp_device_t * device)
{
	if (!channel || !channel->privateData) {
		return;
	}

	/** for previous device,set active channel to null */
	if (!device) {
		if (!channel->privateData->device) {
			/* channel->privateData->device was already set to NULL */
			return;
		}
		sccp_device_setActiveChannel(channel->privateData->device, NULL);
	}

	sccp_device_refreplace(channel->privateData->device, (sccp_device_t *) device);

	if (device) {
		sccp_device_setActiveChannel((sccp_device_t *) device, channel);
	}

	if (channel->privateData && channel->privateData->device) {
		memcpy(&channel->preferences.audio, &channel->privateData->device->preferences.audio, sizeof(channel->preferences.audio));
		memcpy(&channel->capabilities.audio, &channel->privateData->device->capabilities.audio, sizeof(channel->capabilities.audio));
		sccp_copy_string(channel->currentDeviceId, channel->privateData->device->id, sizeof(char[StationMaxDeviceNameSize]));
		channel->dtmfmode = channel->privateData->device->getDtmfMode(channel->privateData->device);
		return;
	}
	/* \todo instead of copying caps / prefs from global */
	memcpy(&channel->preferences.audio, &GLOB(global_preferences), sizeof(channel->preferences.audio));
	memcpy(&channel->capabilities.audio, &GLOB(global_preferences), sizeof(channel->capabilities.audio));
	/* \todo we should use */
	// sccp_line_copyMinimumCodecSetFromLineToChannel(l, c); 
	
	sccp_copy_string(channel->currentDeviceId, "SCCP", sizeof(char[StationMaxDeviceNameSize]));
	channel->dtmfmode = SCCP_DTMFMODE_RFC2833;
}

/*!
 * \brief recalculating read format for channel 
 * \param channel a *retained* SCCP Channel
 */
static void sccp_channel_recalculateReadformat(sccp_channel_t * channel)
{

	if (channel->rtp.audio.writeState != SCCP_RTP_STATUS_INACTIVE && channel->rtp.audio.writeFormat != SKINNY_CODEC_NONE) {
		//pbx_log(LOG_NOTICE, "we already have a write format, dont change codec\n");
		channel->rtp.audio.readFormat = channel->rtp.audio.writeFormat;
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);
		return;
	}
	/* check if remote set a preferred format that is compatible */
	if ((channel->rtp.audio.readState == SCCP_RTP_STATUS_INACTIVE)
	    || !sccp_utils_isCodecCompatible(channel->rtp.audio.readFormat, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio))
	    ) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: recalculateReadformat\n", channel->currentDeviceId);
		channel->rtp.audio.readFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.readFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.readFormat = SKINNY_CODEC_WIDEBAND_256K;

			char s1[512];

			sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "can not calculate readFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.readFormat, 1), channel->rtp.audio.readFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		skinny_codec_t codecs[] = { channel->rtp.audio.readFormat };
		PBX(set_nativeAudioFormats) (channel, codecs, 1);
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);

	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log((DEBUGCAT_CODEC + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \nREAD: %s\n",
						       channel->line->name,
						       channel->callid, sccp_multiple_codecs2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)), sccp_multiple_codecs2str(s3, sizeof(s3) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)), sccp_multiple_codecs2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)), sccp_multiple_codecs2str(s2, sizeof(s2) - 1,
																																																								    &channel->rtp.audio.readFormat, 1)
	    );
}

/*!
 * \brief recalculating write format for channel 
 * \param channel a *retained* SCCP Channel
 */
static void sccp_channel_recalculateWriteformat(sccp_channel_t * channel)
{
	//pbx_log(LOG_NOTICE, "writeState %d\n", channel->rtp.audio.writeState);
	if (channel->rtp.audio.readState != SCCP_RTP_STATUS_INACTIVE && channel->rtp.audio.readFormat != SKINNY_CODEC_NONE) {
		channel->rtp.audio.writeFormat = channel->rtp.audio.readFormat;
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
		return;
	}
	/* check if remote set a preferred format that is compatible */
	if ((channel->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE)
	    || !sccp_utils_isCodecCompatible(channel->rtp.audio.writeFormat, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio))
	    ) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: recalculateWriteformat\n", channel->currentDeviceId);

		channel->rtp.audio.writeFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.writeFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.writeFormat = SKINNY_CODEC_WIDEBAND_256K;

			char s1[512];

			sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "can not calculate writeFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.writeFormat, 1), channel->rtp.audio.writeFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		skinny_codec_t codecs[] = { channel->rtp.audio.readFormat };
		PBX(set_nativeAudioFormats) (channel, codecs, 1);
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	} else {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: audio.writeState already active %d\n", channel->currentDeviceId, channel->rtp.audio.writeState);
	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log((DEBUGCAT_CODEC + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \n\tWRITE: %s\n",
						       channel->line->name,
						       channel->callid, sccp_multiple_codecs2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)), sccp_multiple_codecs2str(s3, sizeof(s3) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)), sccp_multiple_codecs2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)), sccp_multiple_codecs2str(s2, sizeof(s2) - 1,
																																																								    &channel->rtp.audio.writeFormat, 1)
	    );
}

void sccp_channel_updateChannelDesignator(sccp_channel_t * c)
{
	if (c) {
		if (c->callid) {
			if (c->line) {
				snprintf(c->designator, CHANNEL_DESIGNATOR_SIZE, "SCCP/%s-%08x", c->line->name, c->callid);
			} else {
				snprintf(c->designator, CHANNEL_DESIGNATOR_SIZE, "SCCP/%s-%08x", "UNDEF", c->callid);
			}
		} else {
			snprintf(c->designator, CHANNEL_DESIGNATOR_SIZE, "SCCP/UNDEF-UNDEF");
		}
		sccp_refcount_updateIdentifier(c, c->designator);
	}
}

/*!
 * \brief Update Channel Capability
 * \param channel a *retained* SCCP Channel
 */
void sccp_channel_updateChannelCapability(sccp_channel_t * channel)
{
	sccp_channel_recalculateReadformat(channel);
	sccp_channel_recalculateWriteformat(channel);
}

/*!
 * \brief Send Call Information to Device/Channel
 *
 * Wrapper function that calls sccp_channel_send_staticCallinfo or sccp_channel_send_dynamicCallinfo
 *
 * \param device SCCP Device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \todo find a difference solution for sccp_conference callinfo update
 */
void sccp_channel_send_callinfo(const sccp_device_t * device, const sccp_channel_t * channel)
{
	uint8_t instance = 0;

	if (device && channel && channel->callid) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: send callInfo of callid %d\n", DEV_ID_LOG(device), channel->callid);
		if (device->protocol && device->protocol->sendCallInfo) {
			instance = sccp_device_find_index_for_line(device, channel->line->name);
			device->protocol->sendCallInfo(device, channel, instance);
		}
	}
}

/*!
 * \brief Send Dialed Number to SCCP Channel device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_send_callinfo2(sccp_channel_t * channel)
{
	ast_assert(channel != NULL);

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);
	AUTO_RELEASE sccp_line_t *line = sccp_line_retain(channel->line);

	if (d) {
		sccp_channel_send_callinfo(d, channel);
	} else if (line) {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
			AUTO_RELEASE sccp_device_t *device = sccp_device_retain(linedevice->device);

			sccp_channel_send_callinfo(device, channel);
		}
		SCCP_LIST_UNLOCK(&line->devices);
	}
}

/*!
 * \brief Set Call State for SCCP Channel sccp_channel, and Send this State to SCCP Device d.
 * \param channel SCCP Channel
 * \param state channel state
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_setChannelstate(sccp_channel_t * channel, sccp_channelstate_t state)
{
	channel->previousChannelState = channel->state;
	channel->state = state;
}

/*!
 * \brief Set CallingParty on SCCP Channel c
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_display_callInfo(sccp_channel_t * channel)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x callInfo:\n", channel->line->name, channel->callid);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - calledParty: %s <%s>, valid: %s\n", (channel->callInfo.calledPartyName) ? channel->callInfo.calledPartyName : "", (channel->callInfo.calledPartyNumber) ? channel->callInfo.calledPartyNumber : "", (channel->callInfo.calledParty_valid) ? "TRUE" : "FALSE");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - callingParty: %s <%s>, valid: %s\n", (channel->callInfo.callingPartyName) ? channel->callInfo.callingPartyName : "", (channel->callInfo.callingPartyNumber) ? channel->callInfo.callingPartyNumber : "", (channel->callInfo.callingParty_valid) ? "TRUE" : "FALSE");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - originalCalledParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCalledPartyName) ? channel->callInfo.originalCalledPartyName : "", (channel->callInfo.originalCalledPartyNumber) ? channel->callInfo.originalCalledPartyNumber : "", (channel->callInfo.originalCalledParty_valid) ? "TRUE" : "FALSE");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - originalCallingParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCallingPartyName) ? channel->callInfo.originalCallingPartyName : "", (channel->callInfo.originalCallingPartyNumber) ? channel->callInfo.originalCallingPartyNumber : "", (channel->callInfo.originalCallingParty_valid) ? "TRUE" : "FALSE");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - lastRedirectingParty: %s <%s>, valid: %s\n", (channel->callInfo.lastRedirectingPartyName) ? channel->callInfo.lastRedirectingPartyName : "", (channel->callInfo.lastRedirectingPartyNumber) ? channel->callInfo.lastRedirectingPartyNumber : "", (channel->callInfo.lastRedirectingParty_valid) ? "TRUE" : "FALSE");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 " - originalCalledPartyRedirectReason: %d, lastRedirectingReason: %d, CallInfo Presentation: %s\n\n", channel->callInfo.originalCdpnRedirectReason, channel->callInfo.lastRedirectingReason, channel->callInfo.presentation ? "ALLOWED" : "FORBIDDEN");
}

/*!
 * \brief Set CallingParty on SCCP Channel c
 * \param channel SCCP Channel
 * \param name Name as char
 * \param number Number as char
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_set_callingparty(sccp_channel_t * channel, char *name, char *number)
{
	if (!channel) {
		return;
	}

	/* in case we want to clear out the current name or number use "" instead of NULL */
	if (name) {
		if (!sccp_strlen_zero(name)) {
			sccp_copy_string(channel->callInfo.callingPartyName, name, sizeof(channel->callInfo.callingPartyName));
		} else {
			channel->callInfo.callingPartyName[0] = '\0';
		}
	}

	if (number) {
		if (!sccp_strlen_zero(number)) {
			sccp_copy_string(channel->callInfo.callingPartyNumber, number, sizeof(channel->callInfo.callingPartyNumber));
			channel->callInfo.callingParty_valid = 1;
		} else {
			channel->callInfo.callingPartyNumber[0] = '\0';
			channel->callInfo.callingParty_valid = 0;
		}
	}
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_callingparty) Set callingParty Name '%s', Number '%s' on channel %d\n", channel->currentDeviceId, channel->callInfo.calledPartyName, channel->callInfo.calledPartyNumber, channel->callid);
}

/*!
 * \brief Set Original Calling Party on SCCP Channel c (Used during Forward)
 * \param channel SCCP Channel * \param name Name as char
 * \param number Number as char
 * \return TRUE/FALSE - TRUE if info changed
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_channel_set_originalCallingparty(sccp_channel_t * channel, char *name, char *number)
{
	boolean_t changed = FALSE;

	if (!channel) {
		return FALSE;
	}
	/* in case we want to clear out the current name or number use "" instead of NULL */
	if (name && strncmp(name, channel->callInfo.originalCallingPartyName, StationMaxNameSize - 1)) {
		if (!sccp_strlen_zero(name)) {
			sccp_copy_string(channel->callInfo.originalCallingPartyName, name, sizeof(channel->callInfo.originalCallingPartyName));
		} else {
			channel->callInfo.originalCallingPartyName[0] = '\0';
		}
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCallingPartyNumber, StationMaxNameSize - 1)) {
		if (!sccp_strlen_zero(number)) {
			sccp_copy_string(channel->callInfo.originalCallingPartyNumber, number, sizeof(channel->callInfo.originalCallingPartyNumber));
			channel->callInfo.originalCallingParty_valid = 1;
		} else {
			channel->callInfo.originalCallingPartyNumber[0] = '\0';
			channel->callInfo.originalCallingParty_valid = 0;
		}
		changed = TRUE;
	}
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_originalCallingparty) Set originalCallingparty Name '%s', Number '%s' on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCallingPartyName, channel->callInfo.originalCallingPartyNumber, channel->callid);
	return changed;
}

/*!
 * \brief Set CalledParty on SCCP Channel c
 * \param channel SCCP Channel
 * \param name Called Party Name
 * \param number Called Party Number
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_set_calledparty(sccp_channel_t * channel, char *name, char *number)
{
	if (!channel || sccp_strequals(number, "s") /* skip update for immediate earlyrtp + s-extension */ ) {
		return;
	}

	/* in case we want to clear out the current name or number use "" instead of NULL */
	if (name) {
		if (!sccp_strlen_zero(name)) {
			sccp_copy_string(channel->callInfo.calledPartyName, name, sizeof(channel->callInfo.calledPartyName));
		} else {
			channel->callInfo.calledPartyName[0] = '\0';
		}
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_calledparty) Set calledParty Name '%s' on channel %d\n", channel->currentDeviceId, channel->callInfo.calledPartyName, channel->callid);
	}

	if (number) {
		if (!sccp_strlen_zero(number)) {
			sccp_copy_string(channel->callInfo.calledPartyNumber, number, sizeof(channel->callInfo.calledPartyNumber));
			channel->callInfo.calledParty_valid = 1;
		} else {
			channel->callInfo.calledPartyNumber[0] = '\0';
			channel->callInfo.calledParty_valid = 0;
		}
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_calledparty) Set calledParty Number '%s' on channel %d\n", channel->currentDeviceId, channel->callInfo.calledPartyNumber, channel->callid);
	}
}

/*!
 * \brief Set Original CalledParty on SCCP Channel c (Used during Forward)
 * \param channel SCCP Channel
 * \param name Name as char
 * \param number Number as char
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_channel_set_originalCalledparty(sccp_channel_t * channel, char *name, char *number)
{
	boolean_t changed = FALSE;

	if (!channel) {
		return FALSE;
	}
	/* in case we want to clear out the current name or number use "" instead of NULL */
	if (name && strncmp(name, channel->callInfo.originalCalledPartyName, StationMaxNameSize - 1)) {
		if (!sccp_strlen_zero(name)) {
			sccp_copy_string(channel->callInfo.originalCalledPartyName, name, sizeof(channel->callInfo.originalCalledPartyName));
		} else {
			channel->callInfo.originalCalledPartyName[0] = '\0';
		}
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCalledPartyNumber, StationMaxNameSize - 1)) {
		if (!sccp_strlen_zero(number)) {
			sccp_copy_string(channel->callInfo.originalCalledPartyNumber, number, sizeof(channel->callInfo.originalCalledPartyNumber));
			channel->callInfo.originalCalledParty_valid = 1;
		} else {
			channel->callInfo.originalCalledPartyNumber[0] = '\0';
			channel->callInfo.originalCalledParty_valid = 0;
		}
		changed = TRUE;
	}
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_originalCalledparty) Set originalCalledparty Name '%s', Number '%s' on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCalledPartyName, channel->callInfo.originalCalledPartyNumber, channel->callid);
	return changed;

}

/*!
 * \brief Request Call Statistics for SCCP Channel
 * \param channel SCCP Channel
 */
void sccp_channel_StatisticsRequest(sccp_channel_t * channel)
{
	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	d->protocol->sendConnectionStatisticsReq(d, channel, SKINNY_STATSPROCESSING_CLEAR);
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device is Requesting CallStatisticsAndClear\n", DEV_ID_LOG(d));
}

/*!
 * \brief Tell Device to Open a RTP Receive Channel
 *
 * At this point we choose the codec for receive channel and tell them to device.
 * We will get a OpenReceiveChannelAck message that includes all information.
 *
 */
void sccp_channel_openReceiveChannel(sccp_channel_t * channel)
{
	uint16_t instance;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	ast_assert(channel->line != NULL);									/* should not be possible, but received a backtrace / report */

	/* Mute mic feature: If previously set, mute the microphone prior receiving media is already open. */
	/* This must be done in this exact order to work on popular phones like the 7975. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}

	/* calculating format at this point doesn work, because asterisk needs a nativeformat to be set before dial */
	//sccp_channel_recalculateWriteformat(channel);

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: channel %s payloadType %d\n", DEV_ID_LOG(d), PBX(getChannelName) (channel), channel->rtp.audio.writeFormat);

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", d->id, channel->callid, codec2str(channel->rtp.audio.writeFormat), channel->line->echocancel ? "ON" : "OFF");

	if (!channel->rtp.audio.rtp && !sccp_rtp_createAudioServer(channel)) {
		pbx_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%08X\n", DEV_ID_LOG(d), channel->line->name, channel->callid);

		instance = sccp_device_find_index_for_line(d, channel->line->name);
		sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, channel->callid, 0);
		return;
	} else {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Started RTP on channel %s-%08X\n", DEV_ID_LOG(d), channel->line->name, channel->callid);
	}

	if (channel->owner) {
		PBX(set_nativeAudioFormats) (channel, &channel->rtp.audio.writeFormat, 1);
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	}

	sccp_log((DEBUGCAT_RTP + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Open receive channel with format %s[%d], payload %d, echocancel: %d, passthrupartyid: %u, callid: %u\n", DEV_ID_LOG(d), codec2str(channel->rtp.audio.writeFormat), channel->rtp.audio.writeFormat, channel->rtp.audio.writeFormat, channel->line->echocancel, channel->passthrupartyid, channel->callid);
	channel->rtp.audio.writeState = SCCP_RTP_STATUS_PROGRESS;

	if (d->nat >= SCCP_NAT_ON) {										/* device is natted */
		uint16_t port = sccp_rtp_getServerPort(&channel->rtp.audio);					/* get rtp server port */

		if (!sccp_socket_getExternalAddr(&channel->rtp.audio.phone_remote)) {				/* Use externip (PBX behind NAT Firewall */
			memcpy(&channel->rtp.audio.phone_remote, &d->session->ourip, sizeof(struct sockaddr_storage));	/* Fallback: use ip-address of incoming interface */
		}
		sccp_socket_ipv4_mapped(&channel->rtp.audio.phone_remote, &channel->rtp.audio.phone_remote);
		sccp_socket_setPort(&channel->rtp.audio.phone_remote, port);
	}

	d->protocol->sendOpenReceiveChannel(d, channel);
#ifdef CS_SCCP_VIDEO
	if (sccp_device_isVideoSupported(d) && channel->videomode == SCCP_VIDEO_MODE_AUTO) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: We can have video, try to start vrtp\n", DEV_ID_LOG(d));
		if (!channel->rtp.video.rtp && !sccp_rtp_createVideoServer(channel)) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can not start vrtp\n", DEV_ID_LOG(d));
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: video rtp started\n", DEV_ID_LOG(d));
			sccp_channel_startMultiMediaTransmission(channel);
		}
	}
#endif
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 * \note sccp_channel_stopMediaTransmission is explicit call within this function!
 * 
 */
void sccp_channel_closeReceiveChannel(sccp_channel_t * channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	// stop transmitting before closing receivechannel (\note maybe we should not be doing this here)
	sccp_channel_stopMediaTransmission(channel, KeepPortOpen);
	//sccp_rtp_stop(channel);

	if (channel->rtp.audio.writeState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Close receivechannel on channel %d (KeepPortOpen: %s)\n", DEV_ID_LOG(d), channel->callid, KeepPortOpen ? "YES" : "NO");
		REQ(msg, CloseReceiveChannel);
		msg->data.CloseReceiveChannel.lel_conferenceId = htolel(channel->callid);
		msg->data.CloseReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.CloseReceiveChannel.lel_callReference = htolel(channel->callid);
		msg->data.CloseReceiveChannel.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		channel->rtp.audio.writeState = SCCP_RTP_STATUS_INACTIVE;
	}
}

void sccp_channel_updateReceiveChannel(sccp_channel_t * channel)
{
	/* \todo possible to skip the closing of the receive channel (needs testing) */
	/* \todo if this works without closing, this would make changing codecs on the fly possible */
	if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.audio.writeState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateReceiveChannel) Close Receive Channel on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_closeReceiveChannel(channel, TRUE);
	}
	if (SCCP_RTP_STATUS_INACTIVE == channel->rtp.audio.writeState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateReceiveChannel) Open Receive Channel on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_openReceiveChannel(channel);
	}
}

/*!
 * \brief Open Multi Media Channel (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_openMultiMediaReceiveChannel(sccp_channel_t * channel)
{
	uint32_t skinnyFormat;
	int payloadType;
	uint8_t lineInstance;
	int bitRate = 1500;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}

	if ((channel->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE)) {
		return;
	}

	channel->rtp.video.writeState |= SCCP_RTP_STATUS_PROGRESS;
	skinnyFormat = channel->rtp.video.writeFormat;

	if (skinnyFormat == 0) {
		pbx_log(LOG_NOTICE, "SCCP: Unable to find skinny format for %d\n", channel->rtp.video.writeFormat);
		return;
	}

	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, channel->rtp.video.writeFormat);
	lineInstance = sccp_device_find_index_for_line(d, channel->line->name);

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Open receive multimedia channel with format %s[%d] skinnyFormat %s[%d], payload %d\n", DEV_ID_LOG(d), codec2str(channel->rtp.video.writeFormat), channel->rtp.video.writeFormat, codec2str(skinnyFormat), skinnyFormat, payloadType);
	d->protocol->sendOpenMultiMediaChannel(d, channel, skinnyFormat, payloadType, lineInstance, bitRate);
}

/*!
 * \brief Open Multi Media Channel (Video) on Channel
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 */
void sccp_channel_closeMultiMediaReceiveChannel(sccp_channel_t * channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	// stop transmitting before closing receivechannel (\note maybe we should not be doing this here)
	sccp_channel_stopMediaTransmission(channel, KeepPortOpen);

	if (channel->rtp.video.writeState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Close multimedia receive channel on channel %d (KeepPortOpen: %s)\n", DEV_ID_LOG(d), channel->callid, KeepPortOpen ? "YES" : "NO");
		REQ(msg, CloseMultiMediaReceiveChannel);
		msg->data.CloseMultiMediaReceiveChannel.lel_conferenceId = htolel(channel->callid);
		msg->data.CloseMultiMediaReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.CloseMultiMediaReceiveChannel.lel_callReference = htolel(channel->callid);
		msg->data.CloseMultiMediaReceiveChannel.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		channel->rtp.video.writeState = SCCP_RTP_STATUS_INACTIVE;
	}
}

void sccp_channel_updateMultiMediaReceiveChannel(sccp_channel_t * channel)
{
	if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.video.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateMultiMediaReceiveChannel) Stop multimedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_closeMultiMediaReceiveChannel(channel, TRUE);
	}
	if (SCCP_RTP_STATUS_INACTIVE == channel->rtp.video.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateMultiMediaReceiveChannel) Start media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_openMultiMediaReceiveChannel(channel);
	}
}

/*!
 * \brief Tell a Device to Start Media Transmission.
 *
 * We choose codec according to sccp_channel->format.
 *
 * \param channel SCCP Channel
 * \note rtp should be started before, otherwise we do not start transmission
 */
void sccp_channel_startMediaTransmission(sccp_channel_t * channel)
{
	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_channel_startMediaTransmission) Could not retrieve device from channel\n");
		return;
	}

	if (!channel->rtp.audio.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%08X\n", channel->currentDeviceId, channel->line->name, channel->callid);
		return;
	}

	/* Mute mic feature: If previously set, mute the microphone after receiving of media is already open, but before starting to send to rtp. */
	/* This must be done in this exact order to work also on newer phones like the 8945. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}
	uint16_t usFamily = (d->session->ourip.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&d->session->ourip)) ? AF_INET6 : AF_INET;
	uint16_t remoteFamily = (channel->rtp.audio.phone_remote.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&channel->rtp.audio.phone_remote)) ? AF_INET6 : AF_INET;

	/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
	if (d->nat >= SCCP_NAT_ON) {
		if ((usFamily == AF_INET) != remoteFamily) {							/* device needs correction for ipv6 address in remote */
			uint16_t port = sccp_rtp_getServerPort(&channel->rtp.audio);				/* get rtp server port */

			memcpy(&channel->rtp.audio.phone_remote, &d->session->ourip, sizeof(struct sockaddr_storage));	/* Not sure if this should not be the externip in case of nat */
			sccp_socket_ipv4_mapped(&channel->rtp.audio.phone_remote, &channel->rtp.audio.phone_remote);	/*!< we need this to convert mapped IPv4 to real IPv4 address */
			sccp_socket_setPort(&channel->rtp.audio.phone_remote, port);

		} else if ((usFamily == AF_INET6) != remoteFamily) {						/* the device can do IPv6 but should send it to IPv4 address (directrtp possible) */
			struct sockaddr_storage sas;

			memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
			sccp_socket_ipv4_mapped(&sas, &sas);
		}
	}
	//sccp_channel_recalculateReadformat(channel);
	if (channel->owner) {
		PBX(set_nativeAudioFormats) (channel, &channel->rtp.audio.readFormat, 1);
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);
	}
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Using codec: %s(%d), TOS %d, Silence Suppression: %s for call with PassThruId: %u and CallID: %u\n", DEV_ID_LOG(d), codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat, d->audio_tos, channel->line->silencesuppression ? "ON" : "OFF", channel->passthrupartyid, channel->callid);

	channel->rtp.audio.readState |= SCCP_RTP_STATUS_PROGRESS;
	d->protocol->sendStartMediaTransmission(d, channel);

	char buf1[NI_MAXHOST + NI_MAXSERV];
	char buf2[NI_MAXHOST + NI_MAXSERV];

	sccp_copy_string(buf1, sccp_socket_stringify(&channel->rtp.audio.phone), sizeof(buf1));
	sccp_copy_string(buf2, sccp_socket_stringify(&channel->rtp.audio.phone_remote), sizeof(buf2));

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell Phone to send RTP/UDP media from %s to %s (NAT: %s)\n", DEV_ID_LOG(d), buf1, buf2, sccp_nat2str(d->nat));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Using codec: %s(%d), TOS %d, Silence Suppression: %s for call with PassThruId: %u and CallID: %u\n", DEV_ID_LOG(d), codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat, d->audio_tos, channel->line->silencesuppression ? "ON" : "OFF", channel->passthrupartyid, channel->callid);
}

/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 * 
 */
void sccp_channel_stopMediaTransmission(sccp_channel_t * channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	// stopping phone rtp
	if (channel->rtp.audio.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop mediatransmission on channel %d (KeepPortOpen: %s)\n", DEV_ID_LOG(d), channel->callid, KeepPortOpen ? "YES" : "NO");
		REQ(msg, StopMediaTransmission);
		msg->data.StopMediaTransmission.lel_conferenceId = htolel(channel->callid);
		msg->data.StopMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.StopMediaTransmission.lel_callReference = htolel(channel->callid);
		msg->data.StopMediaTransmission.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		channel->rtp.audio.readState = SCCP_RTP_STATUS_INACTIVE;
	}
}

void sccp_channel_updateMediaTransmission(sccp_channel_t * channel)
{
	/* \note apparently startmediatransmission allows us to change the ip-information midflight without stopping mediatransmission beforehand */
	/* \note this would indicate that it should also be possible to change codecs midflight ! */
	/* \test should be able to do without this block to stopmediatransmission (Sometimes results in "OpenIngressChan: Potential buffer leak" (phone log) */
	if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.audio.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMediaTransmission) Stop media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_stopMediaTransmission(channel, TRUE);
	}
	if (SCCP_RTP_STATUS_INACTIVE == channel->rtp.audio.readState) {
		/*! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMediaTransmission) Start/Update media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_startMediaTransmission(channel);
	}
}

/*!
 * \brief Start Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel)
{
	int payloadType;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	// int packetSize;
	channel->rtp.video.readFormat = SKINNY_CODEC_H264;
	PBX(set_nativeVideoFormats) (channel, SKINNY_CODEC_H264);
	//// packetSize = 3840;
	// packetSize = 1920;

	int bitRate = channel->maxBitRate;

	if (!channel->rtp.video.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can't start vrtp media transmission, maybe channel is down %s-%08X\n", channel->currentDeviceId, channel->line->name, channel->callid);
		return;
	}

	channel->preferences.video[0] = SKINNY_CODEC_H264;
	//channel->preferences.video[0] = SKINNY_CODEC_H263;

	channel->rtp.video.readFormat = sccp_utils_findBestCodec(channel->preferences.video, ARRAY_LEN(channel->preferences.video), channel->capabilities.video, ARRAY_LEN(channel->capabilities.video), channel->remoteCapabilities.video, ARRAY_LEN(channel->remoteCapabilities.video));

	if (channel->rtp.video.readFormat == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: fall back to h264\n", DEV_ID_LOG(d));
		channel->rtp.video.readFormat = SKINNY_CODEC_H264;
	}

	/* lookup payloadType */
	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, channel->rtp.video.readFormat);
	//! \todo handle payload error
	//! \todo use rtp codec map

	//check if bind address is an global bind address
	/*
	if (!channel->rtp.video.phone_remote.sin_addr.s_addr) {
		channel->rtp.video.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
	}
	*/

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: using payload %d\n", DEV_ID_LOG(d), payloadType);
	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: using payload %d\n", DEV_ID_LOG(d), payloadType);

	uint16_t usFamily = (d->session->ourip.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&d->session->ourip)) ? AF_INET6 : AF_INET;
	uint16_t remoteFamily = (channel->rtp.audio.phone_remote.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&channel->rtp.audio.phone_remote)) ? AF_INET6 : AF_INET;

	/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
	if (AF_INET6 == remoteFamily && usFamily == AF_INET6) {
		/* we do not support video over IPv6 -> fallback to indirect media and use ourIPv4 as destination */
		uint16_t port = sccp_rtp_getServerPort(&channel->rtp.video);					/* get rtp server port */

		memcpy(&channel->rtp.video.phone_remote, &d->session->ourIPv4, sizeof(struct sockaddr_storage));	/* use ourip as destination (rtp server) */
		sccp_socket_ipv4_mapped(&channel->rtp.video.phone_remote, &channel->rtp.video.phone_remote);	/*!< we need this to convert mapped IPv4 to real IPv4 address */
		sccp_socket_setPort(&channel->rtp.video.phone_remote, port);
	} else if (d->nat >= SCCP_NAT_ON || ((usFamily == AF_INET) != remoteFamily)) {				/* natted or needs correction for ipv6 address in remote */
		uint16_t port = sccp_rtp_getServerPort(&channel->rtp.video);					/* get rtp server port */

		memcpy(&channel->rtp.video.phone_remote, &d->session->ourip, sizeof(struct sockaddr_storage));	/* use ourip as destination (rtp server) */
		sccp_socket_ipv4_mapped(&channel->rtp.video.phone_remote, &channel->rtp.video.phone_remote);	/*!< we need this to convert mapped IPv4 to real IPv4 address */
		sccp_socket_setPort(&channel->rtp.video.phone_remote, port);

	} else if ((usFamily == AF_INET6) != remoteFamily) {							/* the device can do IPv6 but should send it to IPv4 address (directrtp posible) */
		struct sockaddr_storage sas;

		memcpy(&sas, &channel->rtp.video.phone_remote, sizeof(struct sockaddr_storage));
		sccp_socket_ipv4_mapped(&sas, &sas);

	} else if (!d->directrtp) {
		/* I think we do not need this part, because phone_remote will be set on sccp_rtp_createAudioServer -MC */
		/*
		sccp_rtp_getUs(&channel->rtp.audio, &channel->rtp.audio.phone_remote);

		if(sccp_socket_is_any_addr(&channel->rtp.audio.phone_remote) && channel->rtp.audio.phone_remote.ss_family == AF_INET6){
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&channel->rtp.audio.phone_remote;
			memcpy(&in6->sin6_addr, &((struct sockaddr_in6 *)&d->session->ourip)->sin6_addr, 16);
		}
		channel->rtp.audio.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
		memcpy(&channel->rtp.audio.phone_remote, &d->session->ourip, sizeof(channel->rtp.audio.phone_remote));
		*/
	}

	sccp_socket_ipv4_mapped(&channel->rtp.video.phone_remote, &channel->rtp.video.phone_remote);		/*!< we need this to convert mapped IPv4 to real IPv4 address */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send VRTP media to %s with codec: %s(%d), payloadType %d, tos %d\n", d->id, sccp_socket_stringify(&channel->rtp.video.phone_remote), codec2str(channel->rtp.video.readFormat), channel->rtp.video.readFormat, payloadType, d->audio_tos);

	channel->rtp.video.readState = SCCP_RTP_STATUS_PROGRESS;
	d->protocol->sendStartMultiMediaTransmission(d, channel, payloadType, bitRate);
	PBX(queue_control) (channel->owner, AST_CONTROL_VIDUPDATE);
}

/*!
 * \brief Stop Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 */
void sccp_channel_stopMultiMediaTransmission(sccp_channel_t * channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg;

	ast_assert(channel != NULL);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}
	// stopping phone vrtp
	if (channel->rtp.video.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop multimediatransmission on channel %d (KeepPortOpen: %s)\n", DEV_ID_LOG(d), channel->callid, KeepPortOpen ? "YES" : "NO");
		REQ(msg, StopMultiMediaTransmission);
		msg->data.StopMultiMediaTransmission.lel_conferenceId = htolel(channel->callid);
		msg->data.StopMultiMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.StopMultiMediaTransmission.lel_callReference = htolel(channel->callid);
		msg->data.StopMultiMediaTransmission.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		channel->rtp.video.readState = SCCP_RTP_STATUS_INACTIVE;
	}
}

void sccp_channel_updateMultiMediaTransmission(sccp_channel_t * channel)
{
	if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.video.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMultiMediaTransmission) Stop multiemedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_stopMultiMediaTransmission(channel, TRUE);
	}
	if (SCCP_RTP_STATUS_INACTIVE == channel->rtp.video.readState) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMultiMediaTransmission) Start multimedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_startMultiMediaTransmission(channel);
	}
}

void sccp_channel_closeAllMediaTransmitAndReceive(sccp_device_t * d, sccp_channel_t * channel)
{
	ast_assert(channel != NULL);

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_closeAllMediaTransmitAndReceive) Stop All Media Reception and Transmission on channel %d\n", channel->currentDeviceId, channel->callid);
	if (d && SKINNY_DEVICE_RS_OK == d->registrationState) {
		if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.audio.readState) {
			sccp_channel_stopMediaTransmission(channel, FALSE);
		}
		if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.video.readState) {
			sccp_channel_stopMultiMediaTransmission(channel, FALSE);
		}
		if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.audio.writeState) {
			sccp_channel_closeReceiveChannel(channel, FALSE);
		}
		if (SCCP_RTP_STATUS_INACTIVE != channel->rtp.video.writeState) {
			sccp_channel_closeMultiMediaReceiveChannel(channel, FALSE);
		}
	}
	if (channel->rtp.audio.rtp || channel->rtp.video.rtp) {
		sccp_rtp_stop(channel);
	}
}

/*
 * \brief Check if we are in the middle of a transfer and if transfer on hangup is wanted, function is only called by sccp_handle_onhook for now 
 */
boolean_t sccp_channel_transfer_on_hangup(sccp_channel_t * channel)
{
	boolean_t result = FALSE;

	if (GLOB(transfer_on_hangup) &&										/* Complete transfer when one is in progress */
	    (channel->state != SCCP_CHANNELSTATE_BUSY || channel->state != SCCP_CHANNELSTATE_INVALIDNUMBER || channel->state != SCCP_CHANNELSTATE_CONGESTION)) {
		sccp_channel_t *transferee = channel->privateData->device->transferChannels.transferee;
		sccp_channel_t *transferer = channel->privateData->device->transferChannels.transferer;

		if ((transferee && transferer) && (channel == transferer) && (pbx_channel_state(transferer->owner) == AST_STATE_UP || pbx_channel_state(transferer->owner) == AST_STATE_RING)
		    ) {
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion (channel_name: %s, transferee_name: %s, transferer_name: %s, transferer_state: %d)\n", channel->designator, pbx_channel_name(channel->owner), pbx_channel_name(transferee->owner), pbx_channel_name(transferer->owner), pbx_channel_state(transferer->owner));
			sccp_channel_transfer_complete(transferer);
			result = TRUE;
		}
	}
	return result;
}

/*
 * \brief End all forwarding parent channels
 */
void sccp_channel_end_forwarding_channel(sccp_channel_t * orig_channel)
{
	sccp_channel_t *c = NULL;

	if (!orig_channel || !orig_channel->line) {
		return;
	}

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&orig_channel->line->channels, c, list) {
		if (c->parentChannel == orig_channel) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_end_forwarding_channel) Send Hangup to CallForwarding Channel\n", c->designator);
			c->parentChannel = sccp_channel_release(c->parentChannel);				/* release refcounted parentChannel */
			/* make sure a ZOMBIE channel is hungup using requestHangup if it is still available after the masquerade */
			c->hangupRequest = sccp_wrapper_asterisk_requestHangup;
			/* need to use scheduled hangup, so that we clear any outstanding locks (during masquerade) before calling hangup */
			sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
			orig_channel->answered_elsewhere = TRUE;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

/*!
 * \brief Scheduled Hangup for a channel channel (Used by invalid number)
 */
static int _sccp_channel_sched_endcall(const void *data)
{
	sccp_channel_t *channel = (sccp_channel_t *) data;

	if (!channel) {
		return -1;
	}
	channel->scheduler.hangup = -1;
	sccp_log(DEBUGCAT_CHANNEL) ("%s: Scheduled Hangup\n", channel->designator);
	if (!channel->scheduler.deny) {										/* we cancelled all scheduled tasks, so we should not be hanging up this channel anymore */
		sccp_channel_stop_and_deny_scheduled_tasks(channel);
		sccp_channel_endcall(channel);
	}
	sccp_channel_release(channel);										/* releasing the ref taken when creating the scheduled hangup */
	return 0;
}

/* 
 * Remove Schedule digittimeout
 */
gcc_inline void sccp_channel_stop_schedule_digittimout(sccp_channel_t * channel)
{
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

	if (c) {
		if (c->scheduler.digittimeout > 0) {
			PBX(sched_del_ref) (&c->scheduler.digittimeout, c);
		}
	}
}

/* 
 * Schedule hangup if allowed and not already scheduled
 * \note needs to take retain on channel to pass it on the the scheduled hangup
 */
gcc_inline void sccp_channel_schedule_hangup(sccp_channel_t * channel, uint timeout)
{
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

	if (c) {
		if (c && !c->scheduler.deny && !c->scheduler.hangup) {						/* only schedule if allowed and not already scheduled */
			if (PBX(sched_add_ref) (&c->scheduler.hangup, timeout, _sccp_channel_sched_endcall, c) < 0) {
				pbx_log(LOG_NOTICE, "%s: Unable to schedule dialing in '%d' ms\n", c->designator, timeout);
			}
		}
	}
}

/* 
 * Schedule digittimeout if allowed
 * Release any previously scheduled digittimeout
 */
gcc_inline void sccp_channel_schedule_digittimout(sccp_channel_t * channel, uint timeout)
{
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

	if (c) {
		if (!c->scheduler.deny) {									/* only schedule if allowed and not already scheduled */
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: schedule digittimeout %d\n", c->designator, timeout);
			PBX(sched_replace_ref) (&c->scheduler.digittimeout, timeout * 1000, sccp_pbx_sched_dial, c);
		}
	}
}

void sccp_channel_stop_and_deny_scheduled_tasks(sccp_channel_t * channel)
{
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

	if (c) {
		ATOMIC_INCR(&c->scheduler.deny, TRUE, &c->scheduler.lock);
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Disabling scheduler / Removing Scheduled tasks\n", c->designator);
		if (c->scheduler.digittimeout > 0) {
			PBX(sched_del_ref) (&c->scheduler.digittimeout, c);
		}
		if (c->scheduler.hangup > 0) {
			PBX(sched_del_ref) (&c->scheduler.hangup, c);
		}
	}
}

/*!
 * \brief Hangup this channel.
 * \param channel *retained* SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_endcall(sccp_channel_t * channel)
{
	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}
	sccp_channel_stop_and_deny_scheduled_tasks(channel);
	/* end all call forwarded channels (our children) */
	sccp_channel_end_forwarding_channel(channel);

	/* this is a station active endcall or onhook */
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (d) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "%s: Ending call %s (state:%s)\n", DEV_ID_LOG(d), channel->designator, sccp_channelstate2str(channel->state));
		if (channel->privateData->device) {
			sccp_channel_transfer_cancel(channel->privateData->device, channel);
			sccp_channel_transfer_release(channel->privateData->device, channel);
		}
	}
	if (channel->owner) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending hangupRequest to Call %s (state: %s)\n", DEV_ID_LOG(d), channel->designator, sccp_channelstate2str(channel->state));
		channel->hangupRequest(channel);
	} else {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No Asterisk channel to hangup for sccp channel %s\n", DEV_ID_LOG(d), channel->designator);
	}
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \param parentChannel SCCP Channel for which the channel was created
 * \return a *retained* SCCP Channel or NULL if something is wrong
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_channel_t *sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, const char *dial, uint8_t calltype, PBX_CHANNEL_TYPE * parentChannel, const void *ids)
{
	/* handle outgoing calls */
	sccp_channel_t *channel;

	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a line is not defined!\n");
		return NULL;
	}

	if (!device) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a device is not defined!\n");
		return NULL;
	}

	/* look if we have a call to put on hold */
	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(device);

		if ((c)
#if CS_SCCP_CONFERENCE
		    //&& (NULL == c->conference)
#endif
		    ) {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c)) {
				pbx_log(LOG_ERROR, "%s: Putting Active Channel %s OnHold failed -> Cancelling new CaLL\n", device->id, l->name);
				return NULL;
			}
		}
	}

	channel = sccp_channel_allocate(l, device);

	if (!channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	channel->softswitch_action = SCCP_SOFTSWITCH_DIAL;							/* softswitch will catch the number to be dialed */
	channel->ss_data = 0;											/* nothing to pass to action */

	channel->calltype = calltype;

	/* copy the number to dial in the ast->exten */
	if (dial) {
		if (sccp_strequals(dial, "pickupexten")) {
			char *pickupexten;

			if (PBX(getPickupExtension) (channel, &pickupexten)) {
				sccp_copy_string(channel->dialedNumber, pickupexten, sizeof(channel->dialedNumber));
				sccp_indicate(device, channel, SCCP_CHANNELSTATE_SPEEDDIAL);
				PBX(set_callstate) (channel, AST_STATE_OFFHOOK);
				sccp_free(pickupexten);
			}
		} else {
			sccp_copy_string(channel->dialedNumber, dial, sizeof(channel->dialedNumber));
			sccp_indicate(device, channel, SCCP_CHANNELSTATE_SPEEDDIAL);
			PBX(set_callstate) (channel, AST_STATE_OFFHOOK);
		}
	} else {
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_OFFHOOK);
		PBX(set_callstate) (channel, AST_STATE_OFFHOOK);
	}

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(channel, ids, parentChannel)) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_CONGESTION);
		return channel;
	}

	PBX(set_callstate) (channel, AST_STATE_OFFHOOK);

	if (device->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !channel->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(channel);
	}

	if (!dial && (device->earlyrtp == SCCP_EARLYRTP_IMMEDIATE)) {
		sccp_copy_string(channel->dialedNumber, "s", sizeof(channel->dialedNumber));
		sccp_pbx_softswitch(channel);
		channel->dialedNumber[0] = 0;
		return channel;
	}

	if (dial) {
		sccp_pbx_softswitch(channel);
		return channel;
	}
	sccp_channel_schedule_digittimout(channel, GLOB(firstdigittimeout));

	return channel;
}

/*!
 * \brief Answer an Incoming Call.
 * \param device SCCP Device who answers
 * \param channel incoming *retained* SCCP channel
 * \todo handle codec choose
 *
 * \callgraph
 * \callergraph
 *
 * \todo sccp_channel_answer should be changed to make the answer action an atomic one. Either using locks or atomics to change the c->state and c->answered_elsewhere
 *       Think of multiple devices on a shared line, whereby two answer the incoming call at exactly the same time.
 *       Adding a mutex for just c->state should not be impossible, be would require quite a bit of lock debugging (again)
 *       Can also be solved atomically by using a CAS32 / ATOMIC_INCR
 */
void sccp_channel_answer(const sccp_device_t * device, sccp_channel_t * channel)
{

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Answer Channel %d / %s\n", DEV_ID_LOG(device), channel->callid, channel->designator);

	if (!channel || !channel->line) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no line\n", (channel ? channel->callid : 0));
		return;
	}
	// prevent double answer of the same channel
	if (channel->privateData && channel->privateData->device) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Channel %d has already been answered (%s)\n", DEV_ID_LOG(device), channel->callid, channel->designator);
		return;
	}

	if (!channel->owner) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no owner\n", channel->callid);
		return;
	}

	if (!device) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no device\n", (channel ? channel->callid : 0));
		return;
	}
	AUTO_RELEASE sccp_line_t *l = sccp_line_retain(channel->line);

	channel->subscribers = 1;

#if 0	/** @todo we have to test this code section */
	/* check if this device holds the line channel->line */
	{
		AUTO_RELEASE sccp_linedevices_t *linedevice1 = sccp_linedevice_find(device, l);

		if (!linedevice1) {

			/** this device does not have the original line, mybe it is pickedup with cli or ami function */
			AUTO_RELEASE sccp_line_t *activeLine = sccp_dev_get_activeline(device);

			if (!activeLine) {
				return;
			}
			// sccp_channel_set_line(channel, activeLine);                     // function is to be removed
			sccp_line_refreplace(l, activeLine);
		}
	}
#endif

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer channel %s-%08X\n", DEV_ID_LOG(device), l->name, channel->callid);

	/* answering an incoming call */
	/* look if we have a call to put on hold */
	{
		AUTO_RELEASE sccp_channel_t *sccp_channel_1 = sccp_device_getActiveChannel(device);

		if (sccp_channel_1) {
			/* If there is a ringout or offhook call, we end it so that we can answer the call. */
			if (sccp_channel_1->state == SCCP_CHANNELSTATE_OFFHOOK || sccp_channel_1->state == SCCP_CHANNELSTATE_RINGOUT) {
				sccp_channel_endcall(sccp_channel_1);
			} else if (!sccp_channel_hold(sccp_channel_1)) {
				pbx_log(LOG_ERROR, "%s: Putting Active Channel %s OnHold failed -> Cancelling new CaLL\n", DEV_ID_LOG(device), l->name);
				return;
			}
		}
	}
	// auto released if it was set before
	sccp_channel_setDevice(channel, device);

	/* channel was on hold, restore active -> inc. channelcount */
	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		channel->line->statistic.numberOfActiveChannels--;
	}

	/** check if we have preferences from channel request */
#if 0
	skinny_codec_t preferredCodec = channel->preferences.audio[0];
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) current preferredCodec=%d\n", DEV_ID_LOG(device), preferredCodec);

	/** we changed channel->preferences.audio in sccp_channel_setDevice, so push the preferred codec back to pos 1 */
	if (preferredCodec != SKINNY_CODEC_NONE) {
		skinny_codec_t tempCodecPreferences[ARRAY_LEN(channel->preferences.audio)];
		uint8_t numFoundCodecs = 1;

		/** we did not allow this codec in device prefence list, so do not use this as primary preferred codec */
		if (!sccp_utils_isCodecCompatible(preferredCodec, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio))) {
			numFoundCodecs = 0;
		}

		/* save original preferences */
		memcpy(&tempCodecPreferences, channel->preferences.audio, sizeof(channel->preferences.audio));
		channel->preferences.audio[0] = preferredCodec;

		memcpy(&channel->preferences.audio[numFoundCodecs], tempCodecPreferences, sizeof(skinny_codec_t) * (ARRAY_LEN(channel->preferences.audio) - numFoundCodecs));
	}
	//! \todo move this to openreceive- and startmediatransmission (we do calc in openreceiv and startmedia, so check if we can remove)
#endif
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Update Channel Capability\n", DEV_ID_LOG(device));
	sccp_channel_updateChannelCapability(channel);

	/* end callforwards if any */
	sccp_channel_end_forwarding_channel(channel);

	/** set called party name */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Set CallInfo\n", DEV_ID_LOG(device));
	{
		AUTO_RELEASE sccp_linedevices_t *linedevice2 = sccp_linedevice_find(device, channel->line);

		if (linedevice2) {
			if (!sccp_strlen_zero(linedevice2->subscriptionId.number)) {
				sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, linedevice2->subscriptionId.number);
			} else {
				sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, (channel->line->defaultSubscriptionId.number) ? channel->line->defaultSubscriptionId.number : "");
			}

			if (!sccp_strlen_zero(linedevice2->subscriptionId.name)) {
				sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, linedevice2->subscriptionId.name);
			} else {
				sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, (channel->line->defaultSubscriptionId.name) ? channel->line->defaultSubscriptionId.name : "");
			}

			PBX(set_callerid_number) (channel, channel->callInfo.calledPartyNumber);
			PBX(set_callerid_name) (channel, channel->callInfo.calledPartyName);
		}
	}
	/* done */

	{
		AUTO_RELEASE sccp_device_t *d = sccp_device_retain((sccp_device_t *) device);			/* get non-const device */

		if (d) {
			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Set Active Line\n", d->id);
			sccp_dev_set_activeline(d, channel->line);

			/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
			/* do we need this ? -FS */
#ifdef CS_AST_HAS_FLAG_MOH
			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Stop Music On Hold\n", d->id);
			PBX_CHANNEL_TYPE *pbx_bridged_channel = PBX(get_bridged_channel)(channel->owner);
			if (pbx_bridged_channel && pbx_test_flag(pbx_channel_flags(pbx_bridged_channel), AST_FLAG_MOH)) {
				PBX(moh_stop) (pbx_bridged_channel);						//! \todo use pbx impl
				pbx_clear_flag(pbx_channel_flags(pbx_bridged_channel), AST_FLAG_MOH);
				pbx_channel_unref(pbx_bridged_channel);
			}
#endif
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answering channel with state '%s' (%d)\n", DEV_ID_LOG(device), pbx_state2str(pbx_channel_state(channel->owner)), pbx_channel_state(channel->owner));

			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Go OffHook\n", d->id);
			if (channel->state != SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_OFFHOOK);
				PBX(set_callstate) (channel, AST_STATE_OFFHOOK);
			}

			/* set devicevariables */
			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Copy Variables\n", d->id);
			PBX_VARIABLE_TYPE *v = ((d) ? d->variables : NULL);

			while (channel->owner && !pbx_check_hangup(channel->owner) && d && v) {
				pbx_builtin_setvar_helper(channel->owner, v->name, v->value);
				v = v->next;
			}

			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Set Connected Line\n", d->id);
			PBX(set_connected_line) (channel, channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);

			/** check for monitor request */
			if ((device->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) && !(device->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
				pbx_log(LOG_NOTICE, "%s: request monitor\n", device->id);
				sccp_feat_monitor(d, NULL, 0, channel);
			}

			sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_answer) Set Connected\n", d->id);
			sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONNECTED);
#ifdef CS_MANAGER_EVENTS
			if (GLOB(callevents)) {
				manager_event(EVENT_FLAG_CALL, "CallAnswered", "Channel: %s\r\n" "SCCPLine: %s\r\n" "SCCPDevice: %s\r\n"
					      "Uniqueid: %s\r\n" "CallingPartyNumber: %s\r\n" "CallingPartyName: %s\r\n" "originalCallingParty: %s\r\n" "lastRedirectingParty: %s\r\n", channel->designator, l->name, d->id, PBX(getChannelUniqueID) (channel), channel->callInfo.callingPartyNumber, channel->callInfo.callingPartyName, channel->callInfo.originalCallingPartyName, channel->callInfo.lastRedirectingPartyName);
			}
#endif
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answered channel %s on line %s\n", d->id, channel->designator, l->name);
		}
	}

}

/*!
 * \brief Put channel on Hold.
 *
 * \param channel *retained* SCCP Channel
 * \return Status as in (0 if something was wrong, otherwise 1)
 *
 * \callgraph
 * \callergraph
 */
int sccp_channel_hold(sccp_channel_t * channel)
{
	uint16_t instance;

	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to put on hold\n");
		return FALSE;
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_retain(channel->line);

	if (!l) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line attached to it\n", channel->callid);
		return FALSE;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no device attached to it\n", channel->callid);
		return FALSE;
	}

	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		pbx_log(LOG_WARNING, "SCCP: Channel already on hold\n");
		return FALSE;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* put on hold an active call */
	if (channel->state != SCCP_CHANNELSTATE_CONNECTED && channel->state != SCCP_CHANNELSTATE_CONNECTEDCONFERENCE && channel->state != SCCP_CHANNELSTATE_PROCEED) {	// TOLL FREE NUMBERS STAYS ALWAYS IN CALL PROGRESS STATE
		/* something wrong on the code let's notify it for a fix */
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s can't put on hold an inactive channel %s-%08X with state %s (%d)... cancelling hold action.\n", d->id, l->name, channel->callid, sccp_channelstate2str(channel->state), channel->state);
		/* hard button phones need it */
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		return FALSE;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold the channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);

#ifdef CS_SCCP_CONFERENCE
	if (d->conference) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Putting conference on hold.\n", d->id);
		sccp_conference_hold(d->conference);
		sccp_dev_set_keyset(d, instance, channel->callid, KEYMODE_ONHOLD);
	} else
#endif
	{
		if (channel->owner) {
			PBX(queue_control_data) (channel->owner, AST_CONTROL_HOLD, S_OR(l->musicclass, NULL), !sccp_strlen_zero(l->musicclass) ? strlen(l->musicclass) + 1 : 0);
		}
	}
	//sccp_rtp_stop(channel);
	sccp_dev_set_activeline(d, NULL);
	sccp_indicate(d, channel, SCCP_CHANNELSTATE_HOLD);							// this will also close (but not destroy) the RTP stream
	sccp_channel_setDevice(channel, NULL);

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: On\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", PBX(getChannelName) (channel), PBX(getChannelUniqueID) (channel));
	}
#endif

	if (l) {
		l->statistic.numberOfHoldChannels++;
	}

	sccp_log_and((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);
	return TRUE;
}

/*!
 * \brief Resume a channel that is on hold.
 * \param device device who resumes the channel
 * \param channel channel
 * \param swap_channels Swap Channels as Boolean
 * \return 0 if something was wrong, otherwise 1
 *
 * \callgraph
 * \callergraph
 * 
 */
int sccp_channel_resume(sccp_device_t * device, sccp_channel_t * channel, boolean_t swap_channels)
{
	uint16_t instance;

	if (!channel || !channel->owner || !channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to resume\n");
		return FALSE;
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_retain(channel->line);

	if (!l) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", channel->callid);
		return FALSE;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no device attached to it\n", channel->callid);
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no device or device could not be retained on channel %d\n", channel->callid);
		return FALSE;
	}

	/* look if we have a call to put on hold */
	if (swap_channels) {
		AUTO_RELEASE sccp_channel_t *sccp_channel_on_hold = sccp_device_getActiveChannel(d);

		/* there is an active call, let's put it on hold first */
		if (sccp_channel_on_hold && !(sccp_channel_hold(sccp_channel_on_hold))) {
			pbx_log(LOG_WARNING, "SCCP: swap_channels failed to put channel %d on hold. exiting\n", sccp_channel_on_hold->callid);
			return FALSE;
		}
	}

	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_PROCEED) {
		if (!(sccp_channel_hold(channel))) {
			pbx_log(LOG_WARNING, "SCCP: channel still connected before resuming, put on hold failed for channel %d. exiting\n", channel->callid);
			return FALSE;
		}
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* resume an active call */
	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER && channel->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong in the code let's notify it for a fix */
		pbx_log(LOG_ERROR, "%s can't resume the channel %s-%08X. Not on hold\n", d->id, l->name, channel->callid);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_NO_ACTIVE_CALL_TO_PUT_ON_HOLD, SCCP_DISPLAYSTATUS_TIMEOUT);
		return FALSE;
	}

	/* release transfer if we are in the middle of a transfer */
	sccp_channel_transfer_release(d, channel);

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume the channel %s-%08X\n", d->id, l->name, channel->callid);
	sccp_channel_setDevice(channel, d);

#if ASTERISK_VERSION_GROUP >= 111
	// update callgroup / pickupgroup
	ast_channel_callgroup_set(channel->owner, l->callgroup);
#if CS_SCCP_PICKUP
	ast_channel_pickupgroup_set(channel->owner, l->pickupgroup);
#endif
#else
	channel->owner->callgroup = l->callgroup;
#if CS_SCCP_PICKUP
	channel->owner->pickupgroup = l->pickupgroup;
#endif
#endif														// ASTERISK_VERSION_GROUP >= 111

#ifdef CS_SCCP_CONFERENCE
	if (d->conference) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Conference on the channel %s-%08X\n", d->id, l->name, channel->callid);
		sccp_conference_resume(d->conference);
		sccp_dev_set_keyset(d, instance, channel->callid, KEYMODE_CONNCONF);
	} else
#endif
	{
		if (channel->owner) {
			PBX(queue_control) (channel->owner, AST_CONTROL_UNHOLD);
		}
	}

	//! \todo move this to openreceive- and startmediatransmission
	sccp_channel_updateChannelCapability(channel);

	channel->state = SCCP_CHANNELSTATE_HOLD;
#ifdef CS_AST_CONTROL_SRCUPDATE
	PBX(queue_control) (channel->owner, AST_CONTROL_SRCUPDATE);						// notify changes e.g codec
#endif
	sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONNECTED);							// this will also reopen the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: Off\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", PBX(getChannelName) (channel), PBX(getChannelUniqueID) (channel));
	}
#endif

	/* state of channel is set down from the remoteDevices, so correct channel state */
	channel->state = SCCP_CHANNELSTATE_CONNECTED;
	l->statistic.numberOfHoldChannels--;

	/** set called party name */
	{
		AUTO_RELEASE sccp_linedevices_t *linedevice = sccp_linedevice_find(d, l);

		if (linedevice) {
			if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {

				if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.number)) {
					sprintf(channel->callInfo.callingPartyNumber, "%s%s", channel->line->cid_num, linedevice->subscriptionId.number);
				} else {
					sprintf(channel->callInfo.callingPartyNumber, "%s%s", channel->line->cid_num, (channel->line->defaultSubscriptionId.number) ? channel->line->defaultSubscriptionId.number : "");
				}

				if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.name)) {
					sprintf(channel->callInfo.callingPartyName, "%s%s", channel->line->cid_name, linedevice->subscriptionId.name);
				} else {
					sprintf(channel->callInfo.callingPartyName, "%s%s", channel->line->cid_name, (channel->line->defaultSubscriptionId.name) ? channel->line->defaultSubscriptionId.name : "");
				}
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set callingPartyNumber '%s' callingPartyName '%s'\n", DEV_ID_LOG(d), channel->callInfo.callingPartyNumber, channel->callInfo.callingPartyName);
				PBX(set_connected_line) (channel, channel->callInfo.callingPartyNumber, channel->callInfo.callingPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);

			} else if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {

				if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.number)) {
					sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, linedevice->subscriptionId.number);
				} else {
					sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, (channel->line->defaultSubscriptionId.number) ? channel->line->defaultSubscriptionId.number : "");
				}

				if (linedevice && !sccp_strlen_zero(linedevice->subscriptionId.name)) {
					sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, linedevice->subscriptionId.name);
				} else {
					sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, (channel->line->defaultSubscriptionId.name) ? channel->line->defaultSubscriptionId.name : "");
				}
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set calledPartyNumber '%s' calledPartyName '%s'\n", DEV_ID_LOG(d), channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName);
				PBX(set_connected_line) (channel, channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);
			}
		}
	}
	/* */

	sccp_log_and((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);
	return TRUE;
}

/*!
 * \brief Cleanup Channel before Free.
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_channel_clean(sccp_channel_t * channel)
{
	sccp_selectedchannel_t *sccp_selected_channel;

	if (!channel) {
		pbx_log(LOG_ERROR, "SCCP:No channel provided to clean\n");
		return;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	// l = channel->line;
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Cleaning channel %08x\n", channel->callid);

	sccp_channel_stop_and_deny_scheduled_tasks(channel);

	/* mark the channel DOWN so any pending thread will terminate */
	if (channel->owner) {
		pbx_setstate(channel->owner, AST_STATE_DOWN);
		/* postponing ast_channel_unref to sccp_channel destructor */
		//PBX(set_owner)(channel, NULL);
	}

	if (channel->state != SCCP_CHANNELSTATE_DOWN) {
		PBX(set_callstate) (channel, AST_STATE_DOWN);
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
	}

	if (d) {
		/* make sure all rtp stuff is closed and destroyed */
		sccp_channel_closeAllMediaTransmitAndReceive(d, channel);

		/* deactive the active call if needed */
		if (d->active_channel == channel) {
			sccp_channel_setDevice(channel, NULL);
		}
		sccp_channel_transfer_release(d, channel);
#ifdef CS_SCCP_CONFERENCE
		if (d->conference && d->conference == channel->conference) {
			d->conference = sccp_refcount_release(d->conference, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
		if (channel->conference) {
			channel->conference = sccp_refcount_release(channel->conference, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		}
#endif
		if (channel->privacy) {
			channel->privacy = FALSE;
			d->privacyFeature.status = FALSE;
			sccp_feat_changed(d, NULL, SCCP_FEATURE_PRIVACY);
		}

		if ((sccp_selected_channel = sccp_device_find_selectedchannel(d, channel))) {
			SCCP_LIST_LOCK(&d->selectedChannels);
			sccp_selected_channel = SCCP_LIST_REMOVE(&d->selectedChannels, sccp_selected_channel, list);
			SCCP_LIST_UNLOCK(&d->selectedChannels);
			sccp_free(sccp_selected_channel);
		}
		sccp_dev_set_activeline(d, NULL);
	}
	if (channel && channel->privateData && channel->privateData->device) {
		sccp_channel_setDevice(channel, NULL);
	}
}

/*!
 * \brief Destroy Channel
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \warning
 *  - line->channels is not always locked
 */
void __sccp_channel_destroy(sccp_channel_t * channel)
{
	if (!channel) {
		pbx_log(LOG_NOTICE, "SCCP: channel destructor called with NULL pointer\n");
		return;
	}

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Destroying channel %08x\n", channel->callid);
	if (channel->rtp.audio.rtp || channel->rtp.video.rtp) {
		sccp_rtp_stop(channel);
		sccp_rtp_destroy(channel);
	}
	sccp_channel_release(channel->line);
	if (channel->owner) {
		PBX(set_owner) (channel, NULL);
	}
	if (channel->privateData) {
		sccp_free(channel->privateData);
	}
#ifndef SCCP_ATOMIC
	ast_mutex_destroy(&channel->scheduler.lock);
#endif
	//ast_mutex_destroy(&channel->lock);
	return;
}

/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param channel *retained* SCCP Channel
 * \param device *retained* SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_transfer(sccp_channel_t * channel, sccp_device_t * device)
{
	uint8_t prev_channel_state = 0;
	uint32_t blindTransfer = 0;
	uint16_t instance = 0;
	PBX_CHANNEL_TYPE *pbx_channel_owner = NULL;
	PBX_CHANNEL_TYPE *pbx_channel_bridgepeer = NULL;

	if (!channel) {
		return;
	}

	if (!(channel->line)) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", channel->callid);
		return;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		/* transfer was pressed on first (transferee) channel, check if is our transferee channel and continue with d <= device */
		if (channel == device->transferChannels.transferee && device->transferChannels.transferer) {
			d = sccp_device_retain(device);
		} else {
			pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no device on channel %d\n", channel->callid);
			return;
		}
	}
	instance = sccp_device_find_index_for_line(d, channel->line->name);

	if (!d->transfer || !channel->line->transfer) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", DEV_ID_LOG(d));
		return;
	}

	/* are we in the middle of a transfer? */
	if (d->transferChannels.transferee && d->transferChannels.transferer) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", DEV_ID_LOG(d));
		sccp_channel_transfer_complete(d->transferChannels.transferer);
		return;
	}
	/* exceptional case, we need to release half transfer before retaking, should never occur */
	// if (d->transferChannels.transferee && !d->transferChannels.transferer) {
	//d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
	// }
	if (!d->transferChannels.transferee && d->transferChannels.transferer) {
		d->transferChannels.transferer = sccp_channel_release(d->transferChannels.transferer);
	}

	if ((d->transferChannels.transferee = sccp_channel_retain(channel))) {								/** channel to be transfered */
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%08X\n", DEV_ID_LOG(d), (channel->line) ? channel->line->name : "(null)", channel->callid);

		prev_channel_state = channel->state;

		if (channel->state == SCCP_CHANNELSTATE_HOLD) {							/* already put on hold manually */
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_TRANSFER;
			// sccp_indicate(d, channel, SCCP_CHANNELSTATE_HOLD);                      /* do we need to reindicate ? */
		}
		if ((channel->state != SCCP_CHANNELSTATE_OFFHOOK && channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER)) {
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_TRANSFER;

			if (!sccp_channel_hold(channel)) {							/* hold failed, restore */
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
				return;
			}
		}

		if ((pbx_channel_owner = pbx_channel_ref(channel->owner))) {
			if (channel->state != SCCP_CHANNELSTATE_CALLTRANSFER) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CALLTRANSFER);
			}
			AUTO_RELEASE sccp_channel_t *sccp_channel_new = sccp_channel_newcall(channel->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND, pbx_channel_owner, NULL);

			if (sccp_channel_new && (pbx_channel_bridgepeer = PBX(get_bridged_channel)(pbx_channel_owner))) {
				pbx_builtin_setvar_helper(sccp_channel_new->owner, "TRANSFEREE", pbx_channel_name(pbx_channel_bridgepeer));

				sccp_dev_set_keyset(d, instance, channel->callid, KEYMODE_OFFHOOKFEAT);

				/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER */
#if 0
				if (blindTransfer || (sccp_channel_new && sccp_channel_new->owner && pbx_channel_owner && pbx_channel_bridgepeer)) {
					//! \todo use pbx impl
					pbx_builtin_setvar_helper(sccp_channel_new->owner, "BLINDTRANSFER", pbx_channel_name(pbx_channel_bridgepeer));
					pbx_builtin_setvar_helper(pbx_channel_bridgepeer, "BLINDTRANSFER", pbx_channel_name(sccp_channel_new->owner));
				}
#else
				if (blindTransfer || (sccp_channel_new && sccp_channel_new->owner && pbx_channel_owner && pbx_channel_bridgepeer)) {
					pbx_builtin_setvar_helper(sccp_channel_new->owner, "BLINDTRANSFER", pbx_channel_name(channel->owner));
				}
#endif
				// should go on, even if there is no bridged channel (yet/anymore) ?
				d->transferChannels.transferer = sccp_channel_retain(sccp_channel_new);
				pbx_channel_unref(pbx_channel_bridgepeer);
			} else if (sccp_channel_new && (pbx_channel_appl(pbx_channel_owner) != NULL)) {
				// giving up
				sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Cannot transfer a dialplan application, bridged channel is required on %s-%08X\n", DEV_ID_LOG(d), (channel->line) ? channel->line->name : "(null)", channel->callid);
				sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONGESTION);
				d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
			} else {
				// giving up
				if (!sccp_channel_new) {
					sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: New channel could not be created to complete transfer for %s-%08X\n", DEV_ID_LOG(d), (channel->line) ? channel->line->name : "(null)", channel->callid);
				} else {
					sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No bridged channel or application on %s-%08X\n", DEV_ID_LOG(d), (channel->line) ? channel->line->name : "(null)", channel->callid);
				}
				sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONGESTION);
				d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
			}
			pbx_channel_owner = pbx_channel_unref(pbx_channel_owner);
		} else {
			// giving up
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No pbx channel owner to transfer %s-%08X\n", DEV_ID_LOG(d), (channel->line) ? channel->line->name : "(null)", channel->callid);
			sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
			sccp_indicate(d, channel, prev_channel_state);
			d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
		}
	}
}

/*!
 * \brief Release Transfer Variables
 */
void sccp_channel_transfer_release(sccp_device_t * d, sccp_channel_t * c)
{
	if (!d || !c) {
		return;
	}

	if ((d->transferChannels.transferee && c == d->transferChannels.transferee) || (d->transferChannels.transferer && c == d->transferChannels.transferer)) {
		d->transferChannels.transferee = d->transferChannels.transferee ? sccp_channel_release(d->transferChannels.transferee) : NULL;
		d->transferChannels.transferer = d->transferChannels.transferer ? sccp_channel_release(d->transferChannels.transferer) : NULL;
		sccp_log_and((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%08X released\n", d->id, c->line->name, c->callid);
	}
	c->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
}

/*!
 * \brief Cancel Transfer
 */
void sccp_channel_transfer_cancel(sccp_device_t * d, sccp_channel_t * c)
{
	if (!d || !c || !d->transferChannels.transferee) {
		return;
	}

	/**
	 * workaround to fix issue with 7960 and protocol version != 6
	 * 7960 loses callplane when cancel transfer (end call on other channel).
	 * This script sets the hold state for transfered channel explicitly -MC
	 */
	if (d && d->transferChannels.transferee && d->transferChannels.transferee != c) {
		if (d->transferChannels.transferer && d->transferChannels.transferer != c) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_transfer_cancel) Denied Receipt of Transferee %d %s by the Receiving Party. Cancelling Transfer and Putting transferee channel on Hold.\n", DEV_ID_LOG(d), d->transferChannels.transferee->callid, d->transferChannels.transferee->line->name);
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_transfer_cancel) Denied Receipt of Transferee %d %s by the Transfering Party. Cancelling Transfer and Putting transferee channel on Hold.\n", DEV_ID_LOG(d), d->transferChannels.transferee->callid, d->transferChannels.transferee->line->name);
		}

		d->transferChannels.transferee->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
		sccp_rtp_stop(d->transferChannels.transferee);
		sccp_dev_set_activeline(d, NULL);
		sccp_indicate(d, d->transferChannels.transferee, SCCP_CHANNELSTATE_HOLD);
		sccp_channel_setDevice(d->transferChannels.transferee, NULL);
#if ASTERISK_VERSION_GROUP >= 108
		enum ast_control_transfer control_transfer_message = AST_TRANSFER_FAILED;
		PBX(queue_control_data) (c->owner, AST_CONTROL_TRANSFER, &control_transfer_message, sizeof(control_transfer_message));
#endif
		sccp_channel_transfer_release(d, d->transferChannels.transferee);
	}
}

/*!
 * \brief Bridge Two Channels
 * \param sccp_destination_local_channel Local Destination SCCP Channel
 * \todo Find a way solve the chan->state problem
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_transfer_complete(sccp_channel_t * sccp_destination_local_channel)
{
	PBX_CHANNEL_TYPE *pbx_source_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_source_remote_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_remote_channel = NULL;
#if ASTERISK_VERSION_GROUP >= 108
	enum ast_control_transfer control_transfer_message = AST_TRANSFER_FAILED;
#endif
	uint16_t instance;

	if (!sccp_destination_local_channel) {
		return;
	}
	// Obtain the device from which the transfer was initiated
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(sccp_destination_local_channel);

	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has or device on channel %d\n", sccp_destination_local_channel->callid);
		return;
	}
	if (!sccp_destination_local_channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", sccp_destination_local_channel->callid);
		return;
	}
	// Obtain the source channel on that device
	AUTO_RELEASE sccp_channel_t *sccp_source_local_channel = sccp_channel_retain(d->transferChannels.transferee);

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%08X\n", d->id, sccp_destination_local_channel->line->name, sccp_destination_local_channel->callid);
	instance = sccp_device_find_index_for_line(d, sccp_destination_local_channel->line->name);

	if (sccp_destination_local_channel->state != SCCP_CHANNELSTATE_RINGOUT && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_CONNECTED && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_PROGRESS) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected. ChannelState: %s (%d)\n", sccp_channelstate2str(sccp_destination_local_channel->state), sccp_destination_local_channel->state);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, sccp_destination_local_channel->callid, 0);
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
		goto EXIT;
	}

	if (!sccp_destination_local_channel->owner || !sccp_source_local_channel || !sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%08X and %s-%08X\n",
							      d->id,
							      (sccp_destination_local_channel && sccp_destination_local_channel->line) ? sccp_destination_local_channel->line->name : "(null)",
							      (sccp_destination_local_channel && sccp_destination_local_channel->callid) ? sccp_destination_local_channel->callid : 0, (sccp_source_local_channel && sccp_source_local_channel->line) ? sccp_source_local_channel->line->name : "(null)", (sccp_source_local_channel && sccp_source_local_channel->callid) ? sccp_source_local_channel->callid : 0);
		goto EXIT;
	}

	pbx_source_local_channel = sccp_source_local_channel->owner;
	pbx_source_remote_channel = PBX(get_bridged_channel)(sccp_source_local_channel->owner);
	pbx_destination_remote_channel = PBX(get_bridged_channel)(sccp_destination_local_channel->owner);
	pbx_destination_local_channel = sccp_destination_local_channel->owner;

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_local_channel       %s\n", d->id, pbx_source_local_channel ? pbx_channel_name(pbx_source_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_remote_channel      %s\n\n", d->id, pbx_source_remote_channel ? pbx_channel_name(pbx_source_remote_channel) : "NULL");

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_local_channel  %s\n", d->id, pbx_destination_local_channel ? pbx_channel_name(pbx_destination_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_remote_channel %s\n\n", d->id, pbx_destination_remote_channel ? pbx_channel_name(pbx_destination_remote_channel) : "NULL");

	sccp_source_local_channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;

	if (!(pbx_source_remote_channel && pbx_destination_local_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");

		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
		goto EXIT;
	}

	{
		char *fromName = NULL;
		char *fromNumber = NULL;
		char *toName = NULL;
		char *toNumber = NULL;

		char *originalCallingPartyName = NULL;
		char *originalCallingPartyNumber = NULL;

		int connectedLineUpdateReason = (sccp_destination_local_channel->state == SCCP_CHANNELSTATE_RINGOUT) ? AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER_ALERTING : AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER;

		/* update redirecting info line for source part */
		fromNumber = sccp_destination_local_channel->callInfo.callingPartyNumber;
		fromName = sccp_destination_local_channel->callInfo.callingPartyName;

		toNumber = sccp_destination_local_channel->callInfo.calledPartyNumber;
		toName = sccp_destination_local_channel->callInfo.calledPartyName;

		if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
			originalCallingPartyName = sccp_source_local_channel->callInfo.callingPartyName;
			originalCallingPartyNumber = sccp_source_local_channel->callInfo.callingPartyNumber;
		} else {
			originalCallingPartyName = sccp_source_local_channel->callInfo.calledPartyName;
			originalCallingPartyNumber = sccp_source_local_channel->callInfo.calledPartyNumber;
		}

		/* update our source part */
		sccp_copy_string(sccp_source_local_channel->callInfo.lastRedirectingPartyName, fromName ? fromName : "", sizeof(sccp_source_local_channel->callInfo.callingPartyName));
		sccp_copy_string(sccp_source_local_channel->callInfo.lastRedirectingPartyNumber, fromNumber ? fromNumber : "", sizeof(sccp_source_local_channel->callInfo.lastRedirectingPartyNumber));
		sccp_source_local_channel->callInfo.lastRedirectingParty_valid = 1;
		sccp_channel_display_callInfo(sccp_source_local_channel);

		/* update our destination part */
		sccp_copy_string(sccp_destination_local_channel->callInfo.lastRedirectingPartyName, fromName ? fromName : "", sizeof(sccp_destination_local_channel->callInfo.callingPartyName));
		sccp_copy_string(sccp_destination_local_channel->callInfo.lastRedirectingPartyNumber, fromNumber ? fromNumber : "", sizeof(sccp_destination_local_channel->callInfo.lastRedirectingPartyNumber));
		sccp_destination_local_channel->callInfo.lastRedirectingParty_valid = 1;
		sccp_destination_local_channel->calltype = SKINNY_CALLTYPE_FORWARD;
		sccp_channel_display_callInfo(sccp_destination_local_channel);

		/* update transferee */
		PBX(set_connected_line) (sccp_source_local_channel, toNumber, toName, connectedLineUpdateReason);
#if ASTERISK_VERSION_GROUP > 106										/*! \todo change to SCCP_REASON Codes, using mapping table */
		if (PBX(sendRedirectedUpdate)) {
			PBX(sendRedirectedUpdate) (sccp_source_local_channel, fromNumber, fromName, toNumber, toName, AST_REDIRECTING_REASON_UNCONDITIONAL);
		}
#endif

		/* update ringin channel directly */
		PBX(set_connected_line) (sccp_destination_local_channel, originalCallingPartyNumber, originalCallingPartyName, connectedLineUpdateReason);
#if ASTERISK_VERSION_GROUP > 106										/*! \todo change to SCCP_REASON Codes, using mapping table */
//		if (PBX(sendRedirectedUpdate)) {
//			PBX(sendRedirectedUpdate) (sccp_destination_local_channel, fromNumber, fromName, toNumber, toName, AST_REDIRECTING_REASON_UNCONDITIONAL);
//		}
#endif
	}

	if (sccp_destination_local_channel->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, pbx_channel_name(pbx_source_remote_channel));
		pbx_indicate(pbx_source_remote_channel, AST_CONTROL_RINGING);					// Shouldn't this be ALERTING?

		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (Ringing within Transfer %s)\n", pbx_channel_name(pbx_source_remote_channel));
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (Transfer destination %s)\n", pbx_channel_name(pbx_destination_local_channel));

		if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_complete) Send ringing indication to %s\n", pbx_channel_name(pbx_source_remote_channel));
			pbx_indicate(pbx_source_remote_channel, AST_CONTROL_RINGING);
		} else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_complete) Started music on hold for channel %s\n", pbx_channel_name(pbx_source_remote_channel));
			PBX(moh_start) (pbx_source_remote_channel, NULL, NULL);					//! \todo use pbx impl
		}
	}

	if (!PBX(attended_transfer) (sccp_destination_local_channel, sccp_source_local_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", pbx_channel_name(pbx_destination_local_channel), pbx_channel_name(pbx_source_remote_channel));
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
		goto EXIT;
	}
	
	if (GLOB(transfer_tone) && sccp_destination_local_channel->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* while connected not all the tones can be played */
		sccp_dev_starttone(d, GLOB(autoanswer_tone), instance, sccp_destination_local_channel->callid, 0);
	}

#if ASTERISK_VERSION_GROUP >= 108
	control_transfer_message = AST_TRANSFER_SUCCESS;
#endif
EXIT:
	if (pbx_source_remote_channel) {
		pbx_channel_unref(pbx_source_remote_channel);
	}
	if (pbx_destination_remote_channel) {
		pbx_channel_unref(pbx_destination_remote_channel);
	}
	if (!sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free resources\n");
		return;
	}
#if ASTERISK_VERSION_GROUP >= 108
	PBX(queue_control_data) (sccp_source_local_channel->owner, AST_CONTROL_TRANSFER, &control_transfer_message, sizeof(control_transfer_message));
#endif
	sccp_channel_transfer_release(d, d->transferChannels.transferee);
}

/*!
 * \brief Reset Caller Id Presentation
 * \param channel SCCP Channel
 */
void sccp_channel_reset_calleridPresenceParameter(sccp_channel_t * channel)
{
	channel->callInfo.presentation = CALLERID_PRESENCE_ALLOWED;
	if (PBX(set_callerid_presence)) {
		PBX(set_callerid_presence) (channel);
	}
}

/*!
 * \brief Set Caller Id Presentation
 * \param channel SCCP Channel
 * \param presenceParameter SCCP CallerID Presence ENUM
 */
void sccp_channel_set_calleridPresenceParameter(sccp_channel_t * channel, sccp_calleridpresence_t presenceParameter)
{
	channel->callInfo.presentation = presenceParameter;
	if (PBX(set_callerid_presence)) {
		PBX(set_callerid_presence) (channel);
	}
}

/*!
 * \brief Forward a Channel
 * \param sccp_channel_parent SCCP parent channel
 * \param lineDevice SCCP LineDevice
 * \param fwdNumber fwdNumber as char *
 *
 * \callgraph
 * \callergraph
 */
int sccp_channel_forward(sccp_channel_t * sccp_channel_parent, sccp_linedevices_t * lineDevice, char *fwdNumber)
{
	char dialedNumber[256];

	if (!sccp_channel_parent) {
		pbx_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return -1;
	}

	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));
	AUTO_RELEASE sccp_channel_t *sccp_forwarding_channel = sccp_channel_allocate(sccp_channel_parent->line, NULL);

	if (!sccp_forwarding_channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", lineDevice->device->id);
		return -1;
	}

	sccp_forwarding_channel->parentChannel = sccp_channel_retain(sccp_channel_parent);
	sccp_forwarding_channel->softswitch_action = SCCP_SOFTSWITCH_DIAL;					/* softswitch will catch the number to be dialed */
	sccp_forwarding_channel->ss_data = 0;									// nothing to pass to action
	sccp_forwarding_channel->calltype = SKINNY_CALLTYPE_OUTBOUND;

	/* copy the number to dial in the ast->exten */
	sccp_copy_string(sccp_forwarding_channel->dialedNumber, dialedNumber, sizeof(sccp_forwarding_channel->dialedNumber));
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Incoming: %s (%s) Forwarded By: %s (%s) Forwarded To: %s\n", sccp_channel_parent->callInfo.callingPartyName, sccp_channel_parent->callInfo.callingPartyNumber, lineDevice->line->cid_name, lineDevice->line->cid_num, dialedNumber);

	/* Copy Channel Capabilities From Predecessor */
	memset(&sccp_forwarding_channel->remoteCapabilities.audio, 0, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memcpy(&sccp_forwarding_channel->remoteCapabilities.audio, sccp_channel_parent->remoteCapabilities.audio, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memset(&sccp_forwarding_channel->preferences.audio, 0, sizeof(sccp_forwarding_channel->preferences.audio));
	memcpy(&sccp_forwarding_channel->preferences.audio, sccp_channel_parent->preferences.audio, sizeof(sccp_channel_parent->preferences.audio));

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(sccp_forwarding_channel, NULL, sccp_channel_parent->owner)) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", lineDevice->device->id, sccp_forwarding_channel->line->name);
		sccp_line_removeChannel(sccp_forwarding_channel->line, sccp_forwarding_channel);
		sccp_channel_clean(sccp_forwarding_channel);
		// sccp_channel_destroy(sccp_forwarding_channel);

		return -1;
	}
	/* Update rtp setting to match predecessor */
	skinny_codec_t codecs[] = { SKINNY_CODEC_WIDEBAND_256K };
	PBX(set_nativeAudioFormats) (sccp_forwarding_channel, codecs, 1);
	PBX(rtp_setWriteFormat) (sccp_forwarding_channel, SKINNY_CODEC_WIDEBAND_256K);
	PBX(rtp_setWriteFormat) (sccp_forwarding_channel, SKINNY_CODEC_WIDEBAND_256K);
	sccp_channel_updateChannelCapability(sccp_forwarding_channel);

	/* setting callerid */
	if (PBX(set_callerid_number)) {
		PBX(set_callerid_number) (sccp_forwarding_channel, sccp_channel_parent->callInfo.callingPartyNumber);
	}

	if (PBX(set_callerid_name)) {
		PBX(set_callerid_name) (sccp_forwarding_channel, sccp_channel_parent->callInfo.callingPartyName);
	}

	if (PBX(set_callerid_ani)) {
		PBX(set_callerid_ani) (sccp_forwarding_channel, dialedNumber);
	}

	if (PBX(set_callerid_dnid)) {
		PBX(set_callerid_dnid) (sccp_forwarding_channel, dialedNumber);
	}

	if (PBX(set_callerid_redirectedParty)) {
		PBX(set_callerid_redirectedParty) (sccp_forwarding_channel, sccp_channel_parent->callInfo.calledPartyNumber, sccp_channel_parent->callInfo.calledPartyName);
	}

	if (PBX(set_callerid_redirectingParty)) {
		PBX(set_callerid_redirectingParty) (sccp_forwarding_channel, sccp_forwarding_channel->line->cid_num, sccp_forwarding_channel->line->cid_name);
	}

	/* dial sccp_forwarding_channel */
	PBX(setChannelExten) (sccp_forwarding_channel, dialedNumber);

	/* \todo copy device line setvar variables from parent channel to forwarder->owner */
	PBX(set_callstate) (sccp_forwarding_channel, AST_STATE_OFFHOOK);
	if (!sccp_strlen_zero(dialedNumber)
	    && PBX(checkhangup) (sccp_forwarding_channel)
	    && pbx_exists_extension(sccp_forwarding_channel->owner, sccp_forwarding_channel->line->context, dialedNumber, 1, sccp_forwarding_channel->line->cid_num)) {
		/* found an extension, let's dial it */
		pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x is dialing number %s\n", "SCCP", sccp_forwarding_channel->line->name, sccp_forwarding_channel->callid, dialedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		PBX(set_callstate) (sccp_forwarding_channel, AST_STATE_RING);
		pbx_channel_call_forward_set(sccp_forwarding_channel->owner, dialedNumber);
#if CS_AST_CONTROL_REDIRECTING
		PBX(queue_control) (sccp_forwarding_channel->owner, AST_CONTROL_REDIRECTING);
#endif
		if (pbx_pbx_start(sccp_forwarding_channel->owner)) {
			pbx_log(LOG_WARNING, "%s: invalid number\n", "SCCP");
		}
		return 0;
	} else {
		pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x cannot dial this number %s\n", "SCCP", sccp_forwarding_channel->line->name, sccp_forwarding_channel->callid, dialedNumber);
		sccp_forwarding_channel->parentChannel = sccp_channel_release(sccp_forwarding_channel->parentChannel);
		sccp_channel_endcall(sccp_forwarding_channel);
		return -1;
	}
}

#ifdef CS_SCCP_PARK
/*!
 * \brief Park an SCCP Channel
 * \param channel SCCP Channel
 */
void sccp_channel_park(sccp_channel_t * channel)
{
	sccp_parkresult_t result;

	if (!PBX(feature_park)) {
		pbx_log(LOG_WARNING, "SCCP, parking feature not implemented\n");
		return;
	}

	/* let the pbx implementation do the rest */
	result = PBX(feature_park) (channel);

	if (PARK_RESULT_SUCCESS != result) {
		char extstr[20];

		extstr[0] = 128;
		extstr[1] = SKINNY_LBL_CALL_PARK;
		sprintf(&extstr[2], " failed");
		AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

		if (d) {
			sccp_dev_displaynotify(d, extstr, 10);
		}
	}
}
#endif

/*!
 * \brief Set Preferred Codec on Channel
 * \param c SCCP Channel
 * \param data Stringified Skinny Codec ShortName
 * \return Success as Boolean
 */
boolean_t sccp_channel_setPreferredCodec(sccp_channel_t * c, const void *data)
{

	char text[64] = { '\0' };
	uint64_t x;
	unsigned int numFoundCodecs = 0;

	if (!data || !c) {
		return FALSE;
	}

	skinny_codec_t tempCodecPreferences[ARRAY_LEN(c->preferences.audio)];

	sccp_copy_string(text, data, sizeof(text));

	/* save original preferences */
	memcpy(&tempCodecPreferences, c->preferences.audio, sizeof(c->preferences.audio));

	for (x = 0; x < ARRAY_LEN(skinny_codecs) && numFoundCodecs < ARRAY_LEN(c->preferences.audio); x++) {
		if (!strcasecmp(skinny_codecs[x].key, text)) {

			c->preferences.audio[numFoundCodecs] = skinny_codecs[x].codec;
			numFoundCodecs++;
			/* we can have multiple codec versions, so dont break on this step */

			//! \todo we should remove our prefs from original list -MC
		}
	}
	memcpy(&c->preferences.audio[numFoundCodecs], tempCodecPreferences, sizeof(skinny_codec_t) * (ARRAY_LEN(c->preferences.audio) - numFoundCodecs));

	/** update capabilities if somthing changed */
	if (numFoundCodecs > 0) {
		sccp_channel_updateChannelCapability(c);
	}

	return TRUE;
}

boolean_t sccp_channel_setVideoMode(sccp_channel_t * c, const void *data){
	boolean_t res = TRUE;
	
	if (!strcasecmp(data, "off")) {
		c->videomode = SCCP_VIDEO_MODE_OFF;
	} else if (!strcasecmp(data, "user")) {
		c->videomode = SCCP_VIDEO_MODE_USER;
	} else if (!strcasecmp(data, "auto")) {
		c->videomode = SCCP_VIDEO_MODE_AUTO;
	}else {
		res = TRUE;
	}

	

	return res;
}

/*!
 * \brief Send callwaiting tone to device multiple times
 */
int sccp_channel_callwaiting_tone_interval(sccp_device_t * device, sccp_channel_t * channel)
{
	if (GLOB(callwaiting_tone)) {
		AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

		if (d) {
			AUTO_RELEASE sccp_channel_t *c = sccp_channel_retain(channel);

			if (c) {
				ast_assert(c->line != NULL);
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Handle Callwaiting Tone on channel %d\n", c->callid);
				if (c && c->owner && (SCCP_CHANNELSTATE_CALLWAITING == c->state || SCCP_CHANNELSTATE_RINGING == c->state)) {
					sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending Call Waiting Tone \n", DEV_ID_LOG(d));
					int instance = sccp_device_find_index_for_line(d, c->line->name);

					sccp_dev_starttone(d, GLOB(callwaiting_tone), instance, c->callid, 0);
					return 0;
				} else {
					sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) channel has been hungup or pickuped up by another phone\n");
					return -1;
				}
			}
		}
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No valid device/channel to handle callwaiting\n");
	} else {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No callwaiting_tone set\n");
	}
	return -1;
}

/*!
 * \brief Helper function to retrieve the pbx channel LinkedID
 */
const char *sccp_channel_getLinkedId(const sccp_channel_t * channel)
{
	if (!PBX(getChannelLinkedId)) {
		return NULL;
	}

	return PBX(getChannelLinkedId) (channel);
}

/*=================================================================================== FIND FUNCTIONS ==============*/
/*!
 * \brief Find Channel by ID, using a specific line
 *
 * \callgraph
 * \callergraph
 * 
 * \param l     SCCP Line
 * \param id    channel ID as int
 * \return *refcounted* SCCP Channel (can be null)
 * \todo rename function to include that it checks the channelstate != DOWN
 */
sccp_channel_t *sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);

	SCCP_LIST_LOCK(&l->channels);
	c = SCCP_LIST_FIND(&l->channels, c, list, (c->callid == id && c->state != SCCP_CHANNELSTATE_DOWN), TRUE);
	SCCP_LIST_UNLOCK(&l->channels);
	return c;
}

/*!
 * Find channel by lineInstance and CallId, connected to a particular device;
 * \return *refcounted* SCCP Channel (can be null)
 */
sccp_channel_t *sccp_find_channel_by_lineInstance_and_callid(const sccp_device_t * d, const uint32_t lineInstance, const uint32_t callid)
{
	sccp_channel_t *c = NULL;

	if (!d || !lineInstance || !callid) {
		return NULL;
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_find_byid((sccp_device_t *) d, lineInstance);

	if (l) {
		SCCP_LIST_LOCK(&l->channels);
		c = SCCP_LIST_FIND(&l->channels, c, list, (c->callid == callid), TRUE);
		SCCP_LIST_UNLOCK(&l->channels);
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find channel for lineInstance:%u and callid:%d on device\n", DEV_ID_LOG(d), lineInstance, callid);
	}
	return c;
}

/*!
 * \brief Find Line by ID
 *
 * \callgraph
 * \callergraph
 * 
 * \param callid Call ID as uint32_t
 * \return *refcounted* SCCP Channel (can be null)
 *
 * \todo rename function to include that it checks the channelstate != DOWN (sccp_find_channel_on_line_byid)
 */
sccp_channel_t *sccp_channel_find_byid(uint32_t callid)
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", callid);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		channel = sccp_find_channel_on_line_byid(l, callid);
		if (channel) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	if (!channel) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Could not find channel for callid:%d on device\n", callid);
	}
	return channel;
}

/*!
 * \brief Find Channel by Pass Through Party ID
 * We need this to start the correct rtp stream.
 *
 * \note Does check that channel state not is DOWN.
 *
 * \callgraph
 * \callergraph
 * 
 * \param passthrupartyid Party ID
 * \return *refcounted* SCCP Channel - cann bee NULL if no channel with this id was found
 */
sccp_channel_t *sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid)
{
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u\n", passthrupartyid);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		c = SCCP_LIST_FIND(&l->channels, c, list, (c->passthrupartyid == passthrupartyid && c->state != SCCP_CHANNELSTATE_DOWN), TRUE);
		SCCP_LIST_UNLOCK(&l->channels);
		if (c) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Could not find active channel with Passthrupartyid %u\n", passthrupartyid);
	}
	return c;
}

/*!
 * \brief Find Channel by Pass Through Party ID on a line connected to device provided
 * We need this to start the correct rtp stream.
 *
 * \param d SCCP Device
 * \param passthrupartyid Party ID
 * \return retained SCCP Channel - can be NULL if no channel with this id was found. 
 *
 * \note does not take channel state into account, this need to be asserted in the calling function
 * \note this is different from the sccp_channel_find_bypassthrupartyid behaviour
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid)
{
	sccp_channel_t *c = NULL;

	if (!d) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "SCCP: No device provided to look for %u\n", passthrupartyid);
		return NULL;
	}
	uint8_t instance = 0;

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u on device %s\n", passthrupartyid, d->id);
	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE sccp_line_t *l = sccp_line_retain(d->lineButtons.instance[instance]->line);

			if (l) {
				sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Found line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				c = SCCP_LIST_FIND(&l->channels, c, list, (c->passthrupartyid == passthrupartyid), TRUE);
				SCCP_LIST_UNLOCK(&l->channels);

				if (c) {
					break;
				}
			}
		}
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find active channel with Passthrupartyid %u on device\n", DEV_ID_LOG(d), passthrupartyid);
	}

	return c;
}

/*!
 * \brief Find Channel by State on Line
 * \return *refcounted* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \param l SCCP Line
 * \param state State
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, sccp_channelstate_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_LIST_LOCK(&l->channels);
	c = SCCP_LIST_FIND(&l->channels, c, list, (c->state == state), TRUE);
	SCCP_LIST_UNLOCK(&l->channels);

	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find active channel with state %s(%u) on line\n", l->id, sccp_channelstate2str(state), state);
	}

	return c;
}

/*!
 * \brief Find Channel by State on Device
 *
 * \callgraph
 * \callergraph
 * 
 * \param device SCCP Device
 * \param state State as int
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_device(sccp_device_t * device, sccp_channelstate_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (!d) {
		return NULL;
	}
	uint8_t instance = 0;

	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE sccp_line_t *l = sccp_line_retain(d->lineButtons.instance[instance]->line);

			if (l) {
				sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_CHANNEL + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				c = SCCP_LIST_FIND(&l->channels, c, list, (c->state == state && sccp_util_matchSubscriptionId(c, d->lineButtons.instance[instance]->subscriptionId.number)), TRUE);
				SCCP_LIST_UNLOCK(&l->channels);
				if (c) {
					break;
				}
			}
		}
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find active channel with state %s(%u) on device\n", DEV_ID_LOG(d), sccp_channelstate2str(state), state);
	}
	return c;
}

/*!
 * \brief Find Selected Channel by Device
 * \param d SCCP Device
 * \param channel channel
 * \return x SelectedChannel
 *
 * \callgraph
 * \callergraph
 * 
 * \todo Currently this returns the selectedchannel unretained (there is no retain/release for selectedchannel at the moment)
 */
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * channel)
{
	if (!d) {
		return NULL;
	}
	sccp_selectedchannel_t *sc = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channel (%d)\n", DEV_ID_LOG(d), channel->callid);

	SCCP_LIST_LOCK(&d->selectedChannels);
	sc = SCCP_LIST_FIND(&d->selectedChannels, sc, list, (sc->channel == channel), FALSE);
	SCCP_LIST_UNLOCK(&d->selectedChannels);
	return sc;
}

/*!
 * \brief Count Selected Channel on Device
 * \param device SCCP Device
 * \return count Number of Selected Channels
 * 
 */
uint8_t sccp_device_selectedchannels_count(sccp_device_t * device)
{
	uint8_t count = 0;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channels count\n", device->id);
	SCCP_LIST_LOCK(&device->selectedChannels);
	count = SCCP_LIST_GETSIZE(&device->selectedChannels);
	SCCP_LIST_UNLOCK(&device->selectedChannels);

	return count;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
