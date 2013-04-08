
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
 * \remarks     Purpose:        SCCP Channels
 *              When to use:    Only methods directly related to sccp channels should be stored in this source file.
 *              Relationships:  SCCP Channels connect Asterisk Channels to SCCP Lines
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
static uint32_t callCount = 1;
void __sccp_channel_destroy(sccp_channel_t * channel);
void sccp_channel_unsetDevice(sccp_channel_t * channel);

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
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	
	if (!(c = sccp_channel_retain(channel))) {
		return;
	}

	if (!(d = sccp_channel_getDevice_retained(channel))) {
		c = sccp_channel_release(c);
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
	d = sccp_device_release(d);
	c = sccp_channel_release(c);
#else 												/* show how WITHREF / GETWITHREF would/could work */
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
 * \lock
 *      - callCountLock
 *      - channel
 */
sccp_channel_t *sccp_channel_allocate(sccp_line_t * l, sccp_device_t * device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t *channel;
	char designator[CHANNEL_DESIGNATOR_SIZE];
	struct sccp_private_channel_data *private_data;
	int callid;

	/* If there is no current line, then we can't make a call in, or out. */
	if (!l) {
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
	if (callCount == 0xFFFFFFFF)
		callCount = 1;
	snprintf(designator, CHANNEL_DESIGNATOR_SIZE, "SCCP/%s-%08X", l->name, callid);
	sccp_mutex_unlock(&callCountLock);

	channel = (sccp_channel_t *) sccp_refcount_object_alloc(sizeof(sccp_channel_t), SCCP_REF_CHANNEL, designator, __sccp_channel_destroy);
	if (!channel) {
		/* error allocating memory */
		pbx_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n", l->id, l->name);
		return NULL;
	}
	memset(channel, 0, sizeof(sccp_channel_t));
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

	sccp_mutex_init(&channel->lock);
	sccp_mutex_lock(&channel->lock);
	pbx_cond_init(&channel->astStateCond, NULL);

	/* this is for dialing scheduler */
	channel->scheduler.digittimeout = -1;
	channel->enbloc.digittimeout = GLOB(digittimeout) * 1000;

	channel->owner = NULL;
	/* default ringermode SKINNY_STATION_OUTSIDERING. Change it with SCCPRingerMode app */
	channel->ringermode = SKINNY_STATION_OUTSIDERING;
	/* inbound for now. It will be changed later on outgoing calls */
	channel->calltype = SKINNY_CALLTYPE_INBOUND;
	channel->answered_elsewhere = FALSE;

	/* by default we allow callerid presentation */
	channel->callInfo.presentation = CALLERID_PRESENCE_ALLOWED;

	channel->callid = callid;
	channel->passthrupartyid = callid ^ 0xFFFFFFFF;

	channel->line = l;
	channel->peerIsSCCP = 0;
	channel->enbloc.digittimeout = GLOB(digittimeout) * 1000;
	channel->maxBitRate = 15000;

	if (device) {
		sccp_channel_setDevice(channel, device);
	}

	sccp_line_addChannel(l, channel);

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, channel->callid, l->name);
#if DEBUG
	channel->getDevice_retained = __sccp_channel_getDevice_retained;
#else
	channel->getDevice_retained = sccp_channel_getDevice_retained;
#endif
	channel->setDevice = sccp_channel_setDevice;

	channel->isMicrophoneEnabled = sccp_always_true;
	channel->setMicrophone = sccp_channel_setMicrophoneState;
	sccp_mutex_unlock(&channel->lock);
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
 * \brief unSet Device in Channels->Private Channel Data
 * \param channel SCCP Channel
 */
void sccp_channel_unsetDevice(sccp_channel_t * channel)
{
	if (channel && channel->privateData && channel->privateData->device) {
		channel->privateData->device = sccp_device_release((sccp_device_t *) channel->privateData->device);
		memcpy(&channel->capabilities.audio, &GLOB(global_preferences), sizeof(channel->capabilities.audio));
		memcpy(&channel->preferences.audio, &GLOB(global_preferences), sizeof(channel->preferences.audio));
		strncpy(channel->currentDeviceId, "SCCP", sizeof(char[StationMaxDeviceNameSize]));
	} else {
		pbx_log(LOG_NOTICE, "SCCP: unsetDevice without channel->privateData->device set\n");
	}
}

/*!
 * \brief Set Device in Channels->Private Channel Data
 * \param channel SCCP Channel
 * \param device SCCP Device
 */
void sccp_channel_setDevice(sccp_channel_t * channel, const sccp_device_t * device)
{
	if (channel->privateData->device == device) {
		return;
	}

	if (NULL != channel->privateData->device) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_4 "SCCP: sccp_channel_setDevice: Auto Release was Necessary\n");
		sccp_channel_unsetDevice(channel);
	}

	if (NULL != device) {
		channel->privateData->device = sccp_device_retain((sccp_device_t *) device);
		memcpy(&channel->preferences.audio, &channel->privateData->device->preferences.audio, sizeof(channel->preferences.audio));	/* our preferred codec list */
		memcpy(&channel->capabilities.audio, &channel->privateData->device->capabilities.audio, sizeof(channel->capabilities.audio));	/* our capability codec list */
		sccp_copy_string(channel->currentDeviceId, device->id, sizeof(char[StationMaxDeviceNameSize]));
	}
}

/*!
 * \brief recalculating read format for channel 
 * \param channel a *retained* SCCP Channel
 */
static void sccp_channel_recalculateReadformat(sccp_channel_t * channel)
{

#ifndef CS_EXPERIMENTAL_RTP
	if (channel->rtp.audio.writeState != SCCP_RTP_STATUS_INACTIVE && channel->rtp.audio.writeFormat != SKINNY_CODEC_NONE) {
		//pbx_log(LOG_NOTICE, "we already have a write format, dont change codec\n");
		channel->rtp.audio.readFormat = channel->rtp.audio.writeFormat;
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);
		return;
	}
#endif
	/* check if remote set a preferred format that is compatible */
	if ((channel->rtp.audio.readState == SCCP_RTP_STATUS_INACTIVE)
	    || !sccp_utils_isCodecCompatible(channel->rtp.audio.readFormat, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio))
	    ) {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: recalculateReadformat\n", channel->currentDeviceId);
		channel->rtp.audio.readFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.readFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.readFormat = SKINNY_CODEC_WIDEBAND_256K;

			char s1[512];

			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "can not calculate readFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.readFormat, 1), channel->rtp.audio.readFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);

	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log((DEBUGCAT_CODEC | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \nREAD: %s\n",
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

#ifndef CS_EXPERIMENTAL_RTP
	if (channel->rtp.audio.readState != SCCP_RTP_STATUS_INACTIVE && channel->rtp.audio.readFormat != SKINNY_CODEC_NONE) {
		channel->rtp.audio.writeFormat = channel->rtp.audio.readFormat;
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
		return;
	}
#endif
	/* check if remote set a preferred format that is compatible */
	if ((channel->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE)
	    || !sccp_utils_isCodecCompatible(channel->rtp.audio.writeFormat, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio))
	    ) {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: recalculateWriteformat\n", channel->currentDeviceId);

		channel->rtp.audio.writeFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.writeFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.writeFormat = SKINNY_CODEC_WIDEBAND_256K;

			char s1[512];

			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "can not calculate writeFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.writeFormat, 1), channel->rtp.audio.writeFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	} else {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: audio.writeState already active %d\n", channel->currentDeviceId, channel->rtp.audio.writeState);
	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log((DEBUGCAT_CODEC | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \n\tWRITE: %s\n",
						       channel->line->name,
						       channel->callid, sccp_multiple_codecs2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)), sccp_multiple_codecs2str(s3, sizeof(s3) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)), sccp_multiple_codecs2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)), sccp_multiple_codecs2str(s2, sizeof(s2) - 1,
																																																								    &channel->rtp.audio.writeFormat, 1)
	    );
}

void sccp_channel_updateChannelDesignator(sccp_channel_t * c)
{
	if (c) {
		if (c->callid) {
			if (c->line && c->line->name) {
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
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t *sccp_channel_get_active(const sccp_device_t * d)
{
	sccp_channel_t *channel;

	if (!d || !d->active_channel) {
		return NULL;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Getting the active channel on device.\n", d->id);

	if (!(channel = sccp_channel_retain(d->active_channel))) {
		return NULL;
	}

	if (channel->state == SCCP_CHANNELSTATE_DOWN) {
		channel = sccp_channel_release(channel);
		return NULL;
	}

	return channel;
}

/*!
 * \brief Set SCCP Channel to Active
 * \param d SCCP Device
 * \param channel SCCP Channel
 * 
 * \lock
 *      - device
 */
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * channel)
{
	sccp_device_t *device = NULL;

	if ((device = sccp_device_retain(d))) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (channel) ? channel->callid : 0);
		if (device->active_channel) {
			device->active_channel->line->statistic.numberOfActiveChannels--;
			device->active_channel = sccp_channel_release(device->active_channel);
		}
		if (channel) {
			device->active_channel = sccp_channel_retain(channel);
			sccp_channel_updateChannelDesignator(channel);
			sccp_dev_set_activeline(device, channel->line);
			device->active_channel->line->statistic.numberOfActiveChannels++;
		}
		device = sccp_device_release(device);
	}
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
void sccp_channel_send_callinfo(sccp_device_t * device, sccp_channel_t * channel)
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
	sccp_device_t *d = sccp_channel_getDevice_retained(channel);
	sccp_line_t *line = sccp_line_retain(channel->line);

	if (NULL != d) {
		sccp_channel_send_callinfo(d, channel);
		d = sccp_device_release(d);

	} else {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
			d = sccp_device_retain(linedevice->device);
			sccp_channel_send_callinfo(d, channel);
			d = sccp_device_release(d);
		}
		SCCP_LIST_UNLOCK(&line->devices);
	}

	line = line ? sccp_line_release(line) : NULL;
}

/*!
 * \brief Set Call State for SCCP Channel sccp_channel, and Send this State to SCCP Device d.
 * \param channel SCCP Channel
 * \param state channel state
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_setSkinnyCallstate(sccp_channel_t * channel, skinny_callstate_t state)
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
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x callInfo:\n", channel->line->name, channel->callid);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - calledParty: %s <%s>, valid: %s\n", (channel->callInfo.calledPartyName) ? channel->callInfo.calledPartyName : "", (channel->callInfo.calledPartyNumber) ? channel->callInfo.calledPartyNumber : "", (channel->callInfo.calledParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - callingParty: %s <%s>, valid: %s\n", (channel->callInfo.callingPartyName) ? channel->callInfo.callingPartyName : "", (channel->callInfo.callingPartyNumber) ? channel->callInfo.callingPartyNumber : "", (channel->callInfo.callingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCalledParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCalledPartyName) ? channel->callInfo.originalCalledPartyName : "", (channel->callInfo.originalCalledPartyNumber) ? channel->callInfo.originalCalledPartyNumber : "", (channel->callInfo.originalCalledParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCallingParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCallingPartyName) ? channel->callInfo.originalCallingPartyName : "", (channel->callInfo.originalCallingPartyNumber) ? channel->callInfo.originalCallingPartyNumber : "", (channel->callInfo.originalCallingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - lastRedirectingParty: %s <%s>, valid: %s\n", (channel->callInfo.lastRedirectingPartyName) ? channel->callInfo.lastRedirectingPartyName : "", (channel->callInfo.lastRedirectingPartyNumber) ? channel->callInfo.lastRedirectingPartyNumber : "", (channel->callInfo.lastRedirectingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCalledPartyRedirectReason: %d, lastRedirectingReason: %d, CallInfo Presentation: %s\n\n", channel->callInfo.originalCdpnRedirectReason, channel->callInfo.lastRedirectingReason, channel->callInfo.presentation ? "ALLOWED" : "FORBIDDEN");
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

	if (name && strncmp(name, channel->callInfo.callingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.callingPartyName, name, sizeof(channel->callInfo.callingPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Name %s on channel %d\n", channel->currentDeviceId, channel->callInfo.callingPartyName, channel->callid);
	}

	if (number && strncmp(number, channel->callInfo.callingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.callingPartyNumber, number, sizeof(channel->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Number %s on channel %d\n", channel->currentDeviceId, channel->callInfo.callingPartyNumber, channel->callid);
		channel->callInfo.callingParty_valid = 1;
	}
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

	if (name && strncmp(name, channel->callInfo.originalCallingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCallingPartyName, name, sizeof(channel->callInfo.originalCallingPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set original Calling Party Name %s on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCallingPartyName, channel->callid);
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCallingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCallingPartyNumber, number, sizeof(channel->callInfo.originalCallingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set original Calling Party Number %s on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCallingPartyNumber, channel->callid);
		changed = TRUE;
		channel->callInfo.originalCallingParty_valid = 1;
	}
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
	if (!channel) {
		return;
	}

	if (!sccp_strlen_zero(name)) {
		sccp_copy_string(channel->callInfo.calledPartyName, name, sizeof(channel->callInfo.calledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_calledparty) Set calledParty Name %s on channel %d\n", channel->currentDeviceId, channel->callInfo.calledPartyName, channel->callid);
	} else {
		channel->callInfo.calledPartyName[0] = '\0';
	}

	if (!sccp_strlen_zero(number)) {
		sccp_copy_string(channel->callInfo.calledPartyNumber, number, sizeof(channel->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_calledparty) Set calledParty Number %s on channel %d\n", channel->currentDeviceId, channel->callInfo.calledPartyNumber, channel->callid);
	} else {
		channel->callInfo.calledPartyNumber[0] = '\0';
	}
	if (!sccp_strlen_zero(channel->callInfo.calledPartyName) && !sccp_strlen_zero(channel->callInfo.calledPartyNumber)) {
		channel->callInfo.calledParty_valid = 1;
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

	if (name && strncmp(name, channel->callInfo.originalCalledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCalledPartyName, name, sizeof(channel->callInfo.originalCalledPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Name %s on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCalledPartyName, channel->callid);
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCalledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCalledPartyNumber, number, sizeof(channel->callInfo.originalCalledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Number %s on channel %d\n", channel->currentDeviceId, channel->callInfo.originalCalledPartyNumber, channel->callid);
		changed = TRUE;
		channel->callInfo.originalCalledParty_valid = 1;
	}
	return changed;
}

/*!
 * \brief Request Call Statistics for SCCP Channel
 * \param channel SCCP Channel
 */
void sccp_channel_StatisticsRequest(sccp_channel_t * channel)
{
	sccp_moo_t *r;
	sccp_device_t *d;

	if (!channel || !(d = sccp_channel_getDevice_retained(channel))) {
		return;
	}
	//TODO use protocol implementation
	if (d->protocol->version < 19) {
		REQ(r, ConnectionStatisticsReq);
	} else {
		REQ(r, ConnectionStatisticsReq_V19);
	}

	/*! \todo We need to test what we have to copy in the DirectoryNumber */
	if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, channel->callInfo.calledPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));
	else
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, channel->callInfo.callingPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));

	r->msg.ConnectionStatisticsReq.lel_callReference = htolel((channel) ? channel->callid : 0);
	r->msg.ConnectionStatisticsReq.lel_StatsProcessing = htolel(SKINNY_STATSPROCESSING_CLEAR);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device is Requesting CallStatisticsAndClear\n", DEV_ID_LOG(d));
	d = sccp_device_release(d);
}

/*!
 * \brief Tell Device to Open a RTP Receive Channel
 *
 * At this point we choose the codec for receive channel and tell them to device.
 * We will get a OpenReceiveChannelAck message that includes all information.
 *
 * \param channel a locked SCCP Channel
 * 
 * \lock
 *        - see sccp_channel_updateChannelCapability()
 *        - see sccp_channel_start_rtp()
 *        - see sccp_device_find_index_for_line()
 *        - see sccp_dev_starttone()
 *        - see sccp_dev_send()
 */
void sccp_channel_openreceivechannel(sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;

	uint16_t instance;

	if (!channel || (!(d = sccp_channel_getDevice_retained(channel)))) {
		return;
	}

	/* Mute mic feature: If previously set, mute the microphone prior receiving media is already open. */
	/* This must be done in this exact order to work on popular phones like the 7975. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}

	/* calculating format at this point doesn work, because asterisk needs a nativeformat to be set before dial */
#ifdef CS_EXPERIMENTAL_CODEC
	sccp_channel_recalculateWriteformat(channel);
#endif

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: channel %s payloadType %d\n", DEV_ID_LOG(d), PBX(getChannelName) (channel), channel->rtp.audio.writeFormat);

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", d->id, channel->callid, codec2str(channel->rtp.audio.writeFormat), channel->line->echocancel ? "ON" : "OFF");

	if (!channel->rtp.audio.rtp && !sccp_rtp_createAudioServer(channel)) {
		pbx_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%08X\n", DEV_ID_LOG(d), channel->line->name, channel->callid);

		instance = sccp_device_find_index_for_line(d, channel->line->name);
		sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, channel->callid, 0);
		d = sccp_device_release(d);
		return;
	} else {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Started RTP on channel %s-%08X\n", DEV_ID_LOG(d), channel->line->name, channel->callid);
	}

	if (channel->owner) {
		PBX(set_nativeAudioFormats) (channel, &channel->rtp.audio.writeFormat, 1);
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	}

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Open receive channel with format %s[%d], payload %d, echocancel: %d, passthrupartyid: %u, callid: %u\n", DEV_ID_LOG(d), codec2str(channel->rtp.audio.writeFormat), channel->rtp.audio.writeFormat, channel->rtp.audio.writeFormat, channel->line->echocancel, channel->passthrupartyid, channel->callid);
	channel->rtp.audio.writeState = SCCP_RTP_STATUS_PROGRESS;
	d->protocol->sendOpenReceiveChannel(d, channel);
#ifdef CS_SCCP_VIDEO
	if (sccp_device_isVideoSupported(d)) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We can have video, try to start vrtp\n", DEV_ID_LOG(d));
		if (!channel->rtp.video.rtp && !sccp_rtp_createVideoServer(channel)) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can not start vrtp\n", DEV_ID_LOG(d));
		} else {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: video rtp started\n", DEV_ID_LOG(d));
			sccp_channel_startMultiMediaTransmission(channel);
		}
	}
#endif
	d = sccp_device_release(d);
}

/*!
 * \brief Open Multi Media Channel (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_openMultiMediaChannel(sccp_channel_t * channel)
{
	uint32_t skinnyFormat;
	int payloadType;
	uint8_t lineInstance;
	int bitRate = 1500;
	sccp_device_t *d = NULL;

	if (!(d = channel->privateData->device)) {
		return;
	}

	if ((channel->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE)) {
		return;
	}

	if (!(d = sccp_channel_getDevice_retained(channel))) {
		return;
	}

	channel->rtp.video.writeState |= SCCP_RTP_STATUS_PROGRESS;
	skinnyFormat = channel->rtp.video.writeFormat;

	if (skinnyFormat == 0) {
		pbx_log(LOG_NOTICE, "SCCP: Unable to find skinny format for %d\n", channel->rtp.video.writeFormat);
		d = sccp_device_release(d);
		return;
	}

	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, channel->rtp.video.writeFormat);
	lineInstance = sccp_device_find_index_for_line(d, channel->line->name);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Open receive multimedia channel with format %s[%d] skinnyFormat %s[%d], payload %d\n", DEV_ID_LOG(d), codec2str(channel->rtp.video.writeFormat), channel->rtp.video.writeFormat, codec2str(skinnyFormat), skinnyFormat, payloadType);
	d->protocol->sendOpenMultiMediaChannel(d, channel, skinnyFormat, payloadType, lineInstance, bitRate);
	d = sccp_device_release(d);
}

/*!
 * \brief Start Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel)
{
	int payloadType;
	sccp_device_t *d = NULL;
	struct sockaddr_in sin;
	struct ast_hostent ahp;
	struct hostent *hp;

	//      int packetSize;
	channel->rtp.video.readFormat = SKINNY_CODEC_H264;
	////    packetSize = 3840;
	//      packetSize = 1920;

	int bitRate = channel->maxBitRate;

	if (!channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start vrtp media transmission, maybe channel is down %s-%08X\n", channel->currentDeviceId, channel->line->name, channel->callid);
		return;
	}

	if ((d = sccp_channel_getDevice_retained(channel))) {
		channel->preferences.video[0] = SKINNY_CODEC_H264;
		//channel->preferences.video[0] = SKINNY_CODEC_H263;

		channel->rtp.video.readFormat = sccp_utils_findBestCodec(channel->preferences.video, ARRAY_LEN(channel->preferences.video), channel->capabilities.video, ARRAY_LEN(channel->capabilities.video), channel->remoteCapabilities.video, ARRAY_LEN(channel->remoteCapabilities.video));

		if (channel->rtp.video.readFormat == 0) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: fall back to h264\n", DEV_ID_LOG(d));
			channel->rtp.video.readFormat = SKINNY_CODEC_H264;
		}

		/* lookup payloadType */
		payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, channel->rtp.video.readFormat);
		//! \todo handle payload error
		//! \todo use rtp codec map

		//check if bind address is an global bind address
		if (!channel->rtp.video.phone_remote.sin_addr.s_addr) {
			channel->rtp.video.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
		}

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: using payload %d\n", DEV_ID_LOG(d), payloadType);

		sccp_rtp_getUs(&channel->rtp.video, &sin);

		if (d->nat) {
			if (GLOB(externip.sin_addr.s_addr)) {
				if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
					time(&GLOB(externexpire));
					GLOB(externexpire) += GLOB(externrefresh);
					if ((hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
						memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
					} else
						pbx_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
				}
				memcpy(&sin.sin_addr, &GLOB(externip.sin_addr), 4);
			}
		}

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send VRTP media to %s:%d with codec: %s(%d), payloadType %d, tos %d\n", d->id, pbx_inet_ntoa(channel->rtp.video.phone_remote.sin_addr), ntohs(channel->rtp.video.phone_remote.sin_port), codec2str(channel->rtp.video.readFormat), channel->rtp.video.readFormat, payloadType, d->audio_tos);
		d->protocol->sendStartMultiMediaTransmission(d, channel, payloadType, bitRate, sin);
		d = sccp_device_release(d);
		PBX(queue_control) (channel->owner, AST_CONTROL_VIDUPDATE);
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
void sccp_channel_startmediatransmission(sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;

	if (!channel->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%08X\n", channel->currentDeviceId, channel->line->name, channel->callid);
		return;
	}

	if ((d = sccp_channel_getDevice_retained(channel))) {
		/* Mute mic feature: If previously set, mute the microphone after receiving of media is already open, but before starting to send to rtp. */
		/* This must be done in this exact order to work also on newer phones like the 8945. It must also be done in other places for other phones. */
		if (!channel->isMicrophoneEnabled()) {
			sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
		}

		/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
		if (d->nat) {
			if (!sccp_strlen_zero(GLOB(externhost))) {
				pbx_log(LOG_NOTICE, "Device is behind NAT use external hostname translation: %s\n", GLOB(externhost));
				struct ast_hostent ahp;
				struct hostent *hp;

				// replace us.sin_addr if we are natted
				if (GLOB(externip.sin_addr.s_addr)) {
					if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
						time(&GLOB(externexpire));
						GLOB(externexpire) += GLOB(externrefresh);
						if ((hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
							memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
						} else {
							pbx_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
						}
					}
					memcpy(&channel->rtp.audio.phone_remote.sin_addr, &GLOB(externip.sin_addr), 4);
				}
			} else if (GLOB(externip.sin_addr.s_addr)) {
				pbx_log(LOG_NOTICE, "Device is behind NAT use external address: %s\n", pbx_inet_ntoa(GLOB(externip.sin_addr)));
				memcpy(&channel->rtp.audio.phone_remote.sin_addr, &GLOB(externip.sin_addr), 4);
			}
		} else {
			/** \todo move this to the initial part, otherwise we overwrite direct rtp destination */
			channel->rtp.audio.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
		}

#ifdef CS_EXPERIMENTAL_CODEC
		sccp_channel_recalculateReadformat(channel);
#endif
		if (channel->owner) {
			PBX(set_nativeAudioFormats) (channel, &channel->rtp.audio.readFormat, 1);
			PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);
		}

		channel->rtp.audio.readState |= SCCP_RTP_STATUS_PROGRESS;
		d->protocol->sendStartMediaTransmission(d, channel);

		char cbuf1[128] = "";
		char cbuf2[128] = "";
		sprintf (cbuf1,"%15s:%d", pbx_inet_ntoa(channel->rtp.audio.phone.sin_addr), ntohs(channel->rtp.audio.phone.sin_port));
		sprintf (cbuf2,"%15s:%d", pbx_inet_ntoa(channel->rtp.audio.phone_remote.sin_addr), ntohs(channel->rtp.audio.phone_remote.sin_port));

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell Phone to send RTP/UDP media from:%s to %s (NAT: %s)\n", DEV_ID_LOG(d), cbuf1, cbuf2, d->nat ? "yes" : "no");
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Using codec: %s(%d), TOS %d, Silence Suppression: %s for call with PassThruId: %u and CallID: %u\n", DEV_ID_LOG(d), codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat, d->audio_tos, channel->line->silencesuppression ? "ON" : "OFF", channel->passthrupartyid, channel->callid);

		d = sccp_device_release(d);
	} else {	
		pbx_log(LOG_ERROR, "SCCP: (sccp_channel_startmediatransmission) Could not retrieve device from channel\n");
	}
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param channel SCCP Channel
 * \note sccp_channel_stopmediatransmission is explicit call within this function!
 * 
 * \lock
 *      - channel
 *        - see sccp_dev_send()
 */
void sccp_channel_closereceivechannel(sccp_channel_t * channel)
{
	sccp_moo_t *r;
	sccp_device_t *d = NULL;

	if (!channel) {
		return;
	}

	if ((d = sccp_channel_getDevice_retained(channel))) {
		REQ(r, CloseReceiveChannel);
		r->msg.CloseReceiveChannel.lel_conferenceId = htolel(channel->callid);
		r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Close receivechannel on channel %d\n", DEV_ID_LOG(d), channel->callid);
	}
	channel->rtp.audio.readState = SCCP_RTP_STATUS_INACTIVE;

	if (d && channel->rtp.video.rtp) {
		REQ(r, CloseMultiMediaReceiveChannel);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId = htolel(channel->callid);
		r->msg.CloseMultiMediaReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
	}

	d = d ? sccp_device_release(d) : NULL;
	sccp_channel_stopmediatransmission(channel);
}

/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param channel SCCP Channel
 * 
 * \lock
 *      - channel
 *        - see sccp_channel_stop_rtp()
 *        - see sccp_dev_send()
 */
void sccp_channel_stopmediatransmission(sccp_channel_t * channel)
{
	sccp_moo_t *r;
	sccp_device_t *d = NULL;

	if (!channel) {
		return;
	}

	if ((d = sccp_channel_getDevice_retained(channel))) {
		REQ(r, StopMediaTransmission);
		r->msg.StopMediaTransmission.lel_conferenceId = htolel(channel->callid);
		r->msg.StopMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StopMediaTransmission.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Stop media transmission on channel %d\n", DEV_ID_LOG(d), channel->callid);
	}
	// stopping rtp
	if (channel->rtp.audio.rtp) {
		sccp_rtp_stop(channel);
	}
	channel->rtp.audio.readState = SCCP_RTP_STATUS_INACTIVE;

	// stopping vrtp
	if (d && channel->rtp.video.rtp) {
		REQ(r, StopMultiMediaTransmission);
		r->msg.StopMultiMediaTransmission.lel_conferenceId = htolel(channel->callid);
		r->msg.StopMultiMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StopMultiMediaTransmission.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
	}
	d = d ? sccp_device_release(d) : NULL;

	/* requesting statistics */
	sccp_channel_StatisticsRequest(channel);
}

/*!
 * \brief Hangup this channel.
 * \param channel *retained* SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *      - line->channels
 *        - see sccp_channel_endcall()
 */
void sccp_channel_endcall(sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;

	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}

	/* end all call forwarded channels (our children) */
	/* should not be necessary here. Should have been handled in sccp_pbx_softswitch / sccp_pbx_answer / sccp_pbx_hangup already */
	sccp_channel_t *c;

	SCCP_LIST_LOCK(&channel->line->channels);
	SCCP_LIST_TRAVERSE(&channel->line->channels, c, list) {
		if (c->parentChannel == channel) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Send Hangup to CallForwarding Channel %s-%08X\n", DEV_ID_LOG(d), c->line->name, c->callid);
			// No need to lock because sccp_channel->line->channels is already locked. 
			sccp_channel_endcall(c);
			c->parentChannel = sccp_channel_release(c->parentChannel);				// release from sccp_channel_forward retain
		}
	}
	SCCP_LIST_UNLOCK(&channel->line->channels);

	/* this is a station active endcall or onhook */
	if ((d = sccp_channel_getDevice_retained(channel))) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "%s: Ending call %d on line %s (%s)\n", DEV_ID_LOG(d), channel->callid, channel->line->name, sccp_indicate2str(channel->state));

#if !CS_EXPERIMENTAL
		/**
		 *	workaround to fix issue with 7960 and protocol version != 6
		 *	7960 loses callplane when cancel transfer (end call on other channel).
		 *	This script set the hold state for transfer_channel explicitly -MC
		 */
		 
		if (channel->privateData->device && channel->privateData->device->transferChannels.transferee && channel->privateData->device->transferChannels.transferee != channel) {
			uint8_t instance = sccp_device_find_index_for_line(channel->privateData->device, channel->privateData->device->transferChannels.transferee->line->name);

			sccp_device_sendcallstate(channel->privateData->device, instance, channel->privateData->device->transferChannels.transferee->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_keyset(channel->privateData->device, instance, channel->privateData->device->transferChannels.transferee->callid, KEYMODE_ONHOLD);
			channel->privateData->device->transferChannels.transferee = channel->privateData->device->transferChannels.transferee ? sccp_channel_release(channel->privateData->device->transferChannels.transferee) : NULL;
		}

		// request a hangup for channel that are part of a transfer call 
		if (channel->privateData->device) {
			if ((channel->privateData->device->transferChannels.transferee && channel == channel->privateData->device->transferChannels.transferee) || (channel->privateData->device->transferChannels.transferer && channel == channel->privateData->device->transferChannels.transferer)
			    ) {
				channel->privateData->device->transferChannels.transferee = channel->privateData->device->transferChannels.transferee ? sccp_channel_release(channel->privateData->device->transferChannels.transferee) : NULL;
				channel->privateData->device->transferChannels.transferer = channel->privateData->device->transferChannels.transferer ? sccp_channel_release(channel->privateData->device->transferChannels.transferer) : NULL;
			}
		}
#else		
		/* Complete transfer when one is in progress */
		if (channel->privateData->device) {
			if (	
				((channel->privateData->device->transferChannels.transferee) && (channel->privateData->device->transferChannels.transferer)) && 
				((channel == channel->privateData->device->transferChannels.transferee) || (channel == channel->privateData->device->transferChannels.transferer))
			    ) {
			    sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", (d && d->id) ? d->id : "SCCP");
			    sccp_channel_transfer_complete(channel->privateData->device->transferChannels.transferer);
			    d = sccp_device_release(d);
			    return;
			}
		}
#endif
		
		if (channel->owner) {
			PBX(requestHangup) (channel->owner);
		} else {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(d), channel->callid, channel->line->name);
		}
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Call %d Ended on line %s (%s)\n", DEV_ID_LOG(d), channel->callid, channel->line->name, sccp_indicate2str(channel->state));
		d = sccp_device_release(d);
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "SCCP: Ending call %d on line %s (%s)\n", channel->callid, channel->line->name, sccp_indicate2str(channel->state));

		if (channel->owner) {
			PBX(requestHangup) (channel->owner);
		} else {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(d), channel->callid, channel->line->name);
		}
	}
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \param linkedId PBX LinkedId which unites related calls under one specific Id.
 * \return a *retained* SCCP Channel or NULL if something is wrong
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - channel
 *        - see sccp_channel_set_active()
 *        - see sccp_indicate_nolock()
 */
sccp_channel_t *sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, const char *dial, uint8_t calltype, const char *linkedId)
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
	if ((channel = sccp_channel_get_active(device))
#if CS_SCCP_CONFERENCE
	    && (NULL == channel->conference)
#endif
	    ) {
		/* there is an active call, let's put it on hold first */
		int ret = sccp_channel_hold(channel);

		channel = sccp_channel_release(channel);
		if (!ret)
			return NULL;
	}

	channel = sccp_channel_allocate(l, device);

	if (!channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	channel->ss_action = SCCP_SS_DIAL;									/* softswitch will catch the number to be dialed */
	channel->ss_data = 0;											/* nothing to pass to action */

	channel->calltype = calltype;

	sccp_channel_set_active(device, channel);

	/* copy the number to dial in the ast->exten */
	if (dial) {
		sccp_copy_string(channel->dialedNumber, dial, sizeof(channel->dialedNumber));
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_SPEEDDIAL);
	} else {
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_OFFHOOK);
	}

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(channel, linkedId)) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_CONGESTION);
		return channel;
	}

	PBX(set_callstate) (channel, AST_STATE_OFFHOOK);

	if (device->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !channel->rtp.audio.rtp) {
		sccp_channel_openreceivechannel(channel);
	}

	if (dial) {
		sccp_pbx_softswitch(channel);
		return channel;
	}

	if ((channel->scheduler.digittimeout = sccp_sched_add(GLOB(firstdigittimeout) * 1000, sccp_pbx_sched_dial, channel)) < 0) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unable to schedule dialing in '%d' ms\n", GLOB(firstdigittimeout));
	}

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
 * \lock
 *      - line
 *      - line->channels
 *        - see sccp_channel_endcall()
 */
void sccp_channel_answer(const sccp_device_t * device, sccp_channel_t * channel)
{
	sccp_line_t *l;
	skinny_codec_t preferredCodec = SKINNY_CODEC_NONE;

#ifdef CS_AST_HAS_FLAG_MOH
	PBX_CHANNEL_TYPE *pbx_bridged_channel;
#endif

	if (!channel || !channel->line) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no line\n", (channel ? channel->callid : 0));
		return;
	}

	if (!channel->owner) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no owner\n", channel->callid);
		return;
	}

	l = sccp_line_retain(channel->line);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer channel %s-%08X\n", DEV_ID_LOG(device), l->name, channel->callid);

	if (!device) {
		pbx_log(LOG_ERROR, "SCCP: Channel %d has no device\n", (channel ? channel->callid : 0));
		if (l)
			l = sccp_line_release(l);
		return;
	}

	/* channel was on hold, restore active -> inc. channelcount */
	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		channel->line->statistic.numberOfActiveChannels--;
	}

	/** check if we have preferences from channel request */
	preferredCodec = channel->preferences.audio[0];
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: current preferredCodec=%d\n", preferredCodec);

	// auto released if it was set before
	sccp_channel_setDevice(channel, device);

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
	//      //! \todo move this to openreceive- and startmediatransmission (we do calc in openreceiv and startmedia, so check if we can remove)
	sccp_channel_updateChannelCapability(channel);

	/* answering an incoming call */
	/* look if we have a call to put on hold */
	sccp_channel_t *sccp_channel_1;

	if ((sccp_channel_1 = sccp_channel_get_active(device)) != NULL) {
		/* If there is a ringout or offhook call, we end it so that we can answer the call. */
		if (sccp_channel_1->state == SCCP_CHANNELSTATE_OFFHOOK || sccp_channel_1->state == SCCP_CHANNELSTATE_RINGOUT) {
			sccp_channel_endcall(sccp_channel_1);
		} else if (!sccp_channel_hold(sccp_channel_1)) {
			/* there is an active call, let's put it on hold first */
			sccp_channel_1 = sccp_channel_release(sccp_channel_1);
			if (l) {
				l = sccp_line_release(l);
			}
			return;
		}
		sccp_channel_1 = sccp_channel_release(sccp_channel_1);
	}

	/* end callforwards */
	sccp_channel_t *sccp_channel_2;

	SCCP_LIST_LOCK(&channel->line->channels);
	SCCP_LIST_TRAVERSE(&channel->line->channels, sccp_channel_2, list) {
		if (sccp_channel_2->parentChannel == channel) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CHANNEL Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(device), l->name, sccp_channel_2->callid);
			/* No need to lock because sccp_channel->line->channels is already locked. */
			sccp_channel_endcall(sccp_channel_2);
			channel->answered_elsewhere = TRUE;
			sccp_channel_2->parentChannel = sccp_channel_release(sccp_channel_2->parentChannel);	// release from sccp_channel_forward_retain
		}
	}
	SCCP_LIST_UNLOCK(&channel->line->channels);
	/* */

	/** set called party name */
	sccp_linedevices_t *linedevice = NULL;

	if ((linedevice = sccp_linedevice_find(device, channel->line))) {
		if (!sccp_strlen_zero(linedevice->subscriptionId.number)) {
			sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, linedevice->subscriptionId.number);
		} else {
			sprintf(channel->callInfo.calledPartyNumber, "%s%s", channel->line->cid_num, (channel->line->defaultSubscriptionId.number) ? channel->line->defaultSubscriptionId.number : "");
		}

		if (!sccp_strlen_zero(linedevice->subscriptionId.name)) {
			sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, linedevice->subscriptionId.name);
		} else {
			sprintf(channel->callInfo.calledPartyName, "%s%s", channel->line->cid_name, (channel->line->defaultSubscriptionId.name) ? channel->line->defaultSubscriptionId.name : "");
		}
		linedevice = sccp_linedevice_release(linedevice);
	}

	/* done */
	sccp_device_t *non_const_device = sccp_device_retain((sccp_device_t *) device);

	if (non_const_device) {
		sccp_channel_set_active(non_const_device, channel);
		sccp_dev_set_activeline(non_const_device, channel->line);

		/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
		/* do we need this ? -FS */
#ifdef CS_AST_HAS_FLAG_MOH
		pbx_bridged_channel = CS_AST_BRIDGED_CHANNEL(channel->owner);
		if (pbx_bridged_channel && pbx_test_flag(pbx_channel_flags(pbx_bridged_channel), AST_FLAG_MOH)) {
			PBX(moh_stop) (pbx_bridged_channel);							//! \todo use pbx impl
			pbx_clear_flag(pbx_channel_flags(pbx_bridged_channel), AST_FLAG_MOH);
		}
#endif
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answering channel with state '%s' (%d)\n", DEV_ID_LOG(device), pbx_state2str(pbx_channel_state(channel->owner)), pbx_channel_state(channel->owner));
		PBX(queue_control) (channel->owner, AST_CONTROL_ANSWER);

		if (channel->state != SCCP_CHANNELSTATE_OFFHOOK) {
			sccp_indicate(non_const_device, channel, SCCP_CHANNELSTATE_OFFHOOK);
		}

		PBX(set_connected_line) (channel, channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);

		sccp_indicate(non_const_device, channel, SCCP_CHANNELSTATE_CONNECTED);
		non_const_device = sccp_device_release(non_const_device);
	}
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answered channel %s-%08X\n", DEV_ID_LOG(device), l->name, channel->callid);
	l = l ? sccp_line_release(l) : NULL;
}

/*!
 * \brief Put channel on Hold.
 *
 * \param channel *retained* SCCP Channel
 * \return Status as in (0 if something was wrong, otherwise 1)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device
 */
int sccp_channel_hold(sccp_channel_t * channel)
{
	sccp_line_t *l;
	sccp_device_t *d;
	uint16_t instance;

	if (!channel) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to put on hold\n");
		return 0;
	}

	if (!(l = channel->line)) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no line attached to it\n", channel->callid);
		return 0;
	}

	if (!(d = sccp_channel_getDevice_retained(channel))) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %d has no device attached to it\n", channel->callid);
		return 0;
	}

	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		pbx_log(LOG_WARNING, "SCCP: Channel already on hold\n");
		d = sccp_device_release(d);
		return 0;
	}

	/* put on hold an active call */
	if (channel->state != SCCP_CHANNELSTATE_CONNECTED && channel->state != SCCP_CHANNELSTATE_CONNECTEDCONFERENCE && channel->state != SCCP_CHANNELSTATE_PROCEED) {	// TOLL FREE NUMBERS STAYS ALWAYS IN CALL PROGRESS STATE
		/* something wrong on the code let's notify it for a fix */
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s can't put on hold an inactive channel %s-%08X with state %s (%d)... cancelling hold action.\n", d->id, l->name, channel->callid, sccp_indicate2str(channel->state), channel->state);
		/* hard button phones need it */
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		d = sccp_device_release(d);
		return 0;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold the channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);

#ifdef CS_SCCP_CONFERENCE
	if (d->conference) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Putting conference on hold.\n", d->id);
		sccp_conference_hold(d->conference);
	} else
#endif
	{
		if (channel->owner) {
			PBX(queue_control_data) (channel->owner, AST_CONTROL_HOLD, S_OR(l->musicclass, NULL), !sccp_strlen_zero(l->musicclass) ? strlen(l->musicclass) + 1 : 0);
		}
	}
	sccp_rtp_stop(channel);
	sccp_channel_set_active(d, NULL);
	sccp_dev_set_activeline(d, NULL);
	sccp_indicate(d, channel, SCCP_CHANNELSTATE_HOLD);							// this will also close (but not destroy) the RTP stream
	sccp_channel_unsetDevice(channel);

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: On\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", PBX(getChannelName) (channel), PBX(getChannelUniqueID) (channel));
#endif

	if (l) {
		l->statistic.numberOfHoldChannels++;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);

	d = sccp_device_release(d);
	return 1;
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
 * \lock
 *      - device
 *      - channel
 *        - see sccp_channel_updateChannelCapability()
 *      - channel
 */
int sccp_channel_resume(sccp_device_t * device, sccp_channel_t * channel, boolean_t swap_channels)
{
	sccp_line_t *l;
	sccp_device_t *d;
	sccp_channel_t *sccp_channel_on_hold;
	uint16_t instance;

	if (!channel || !channel->owner) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to resume\n");
		return 0;
	}

	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", channel->callid);
		return 0;
	}
	l = channel->line;
	if (!(d = sccp_device_retain(device))) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no device or device could not be retained on channel %d\n", channel->callid);
		return 0;
	}

	/* look if we have a call to put on hold */
	if (swap_channels && (sccp_channel_on_hold = sccp_channel_get_active(d))) {
		/* there is an active call, let's put it on hold first */
		if (!(sccp_channel_hold(sccp_channel_on_hold))) {
			pbx_log(LOG_WARNING, "SCCP: swap_channels failed to put channel %d on hold. exiting\n", sccp_channel_on_hold->callid);
			sccp_channel_on_hold = sccp_channel_release(sccp_channel_on_hold);
			d = sccp_device_release(d);
			return 0;
		}
		sccp_channel_on_hold = sccp_channel_release(sccp_channel_on_hold);
	}

	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_PROCEED) {
		if (!(sccp_channel_hold(channel))) {
			pbx_log(LOG_WARNING, "SCCP: channel still connected before resuming, put on hold failed for channel %d. exiting\n", channel->callid);
			d = sccp_device_release(d);
			return 0;
		}
	}

	/* resume an active call */
	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER && channel->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong in the code let's notify it for a fix */
		pbx_log(LOG_ERROR, "%s can't resume the channel %s-%08X. Not on hold\n", d->id, l->name, channel->callid);
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, channel->callid, "No active call to put on hold", 5);
		d = sccp_device_release(d);
		return 0;
	}

	/* check if we are in the middle of a transfer */
	if (d->transferChannels.transferee == channel) {
		d->transferChannels.transferee = d->transferChannels.transferee ? sccp_channel_release(d->transferChannels.transferee) : NULL;
		d->transferChannels.transferer = d->transferChannels.transferer ? sccp_channel_release(d->transferChannels.transferer) : NULL;
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%08X\n", d->id, l->name, channel->callid);
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume the channel %s-%08X\n", d->id, l->name, channel->callid);

	l = channel->line;

	sccp_channel_set_active(d, channel);
	sccp_channel_setDevice(channel, d);

#if ASTERISK_VERSION_GROUP >= 112
	// update callgroup / pickupgroup
	ast_channel_callgroup_set(channel->owner, l->callgroup);
#if CS_SCCP_PICKUP
	ast_channel_pickupgroup_set(channel->owner, l->pickupgroup);
#endif
#endif														// ASTERISK_VERSION_GROUP >= 112

#ifdef CS_SCCP_CONFERENCE
	if (d->conference) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Conference on the channel %s-%08X\n", d->id, l->name, channel->callid);
		sccp_conference_resume(d->conference);
	} else
#endif
	{
		if (channel->owner) {
			PBX(queue_control) (channel->owner, AST_CONTROL_UNHOLD);
		}
	}

	//! \todo move this to openreceive- and startmediatransmission
	sccp_channel_updateChannelCapability(channel);
	//        sccp_rtp_createAudioServer(channel);

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
	if (l) {
		l->statistic.numberOfHoldChannels--;
	}

	/** set called party name */
	sccp_linedevices_t *linedevice = NULL;

	if ((linedevice = sccp_linedevice_find(d, channel->line))) {
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
		linedevice = sccp_linedevice_release(linedevice);
	}
	/* */

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);
	d = sccp_device_release(d);
	return 1;
}

/*!
 * \brief Cleanup Channel before Free.
 * \param channel SCCP Channel
 * \note we assume channel is locked
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device
 *        - see sccp_device_find_selectedchannel()
 *        - device->selectedChannels
 */
void sccp_channel_clean(sccp_channel_t * channel)
{
	//      sccp_line_t *l;
	sccp_device_t *d;
	sccp_selectedchannel_t *sccp_selected_channel;

	if (!channel) {
		pbx_log(LOG_ERROR, "SCCP:No channel provided to clean\n");
		return;
	}

	d = sccp_channel_getDevice_retained(channel);
	//      l = channel->line;
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Cleaning channel %08x\n", channel->callid);

	/* mark the channel DOWN so any pending thread will terminate */
	if (channel->owner) {
		pbx_setstate(channel->owner, AST_STATE_DOWN);
		channel->owner = NULL;
	}

	/* this is in case we are destroying the session */
	if (channel->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);

	sccp_rtp_stop(channel);

	if (d) {
		/* deactive the active call if needed */
		if (d->active_channel == channel)
			sccp_channel_set_active(d, NULL);
		if (d->transferChannels.transferee == channel) {
			d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
		}
		if (d->transferChannels.transferer == channel) {
			d->transferChannels.transferer = sccp_channel_release(d->transferChannels.transferer);
		}
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
		d = sccp_device_release(d);
	}
	if (channel && channel->privateData && channel->privateData->device) {
		sccp_channel_unsetDevice(channel);
	}
}

/*!
 * \brief Destroy Channel
 * \param channel SCCP Channel
 * \note We assume channel is locked
 *
 * \callgraph
 * \callergraph
 *
 * \warning
 *      - line->channels is not always locked
 * 
 * \lock
 *      - channel
 */
void __sccp_channel_destroy(sccp_channel_t * channel)
{
	if (!channel) {
		return;
	}

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Destroying channel %08x\n", channel->callid);

	if (channel->privateData) {
		sccp_free(channel->privateData);
	}
	sccp_mutex_destroy(&channel->lock);
	pbx_cond_destroy(&channel->astStateCond);

	return;
}

/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param channel *retained* SCCP Channel
 * \param device *retained* SCCP Device
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device
 */
void sccp_channel_transfer(sccp_channel_t * channel, sccp_device_t * device)
{
	sccp_device_t *d;
	sccp_channel_t *sccp_channel_new = NULL;
	uint8_t prev_channel_state = 0;
	uint32_t blindTransfer = 0;
	uint16_t instance;

	if (!channel)
		return;

	if (!(channel->line)) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", channel->callid);
		return;
	}

	if (!(d = sccp_channel_getDevice_retained(channel))) {

		/* transfer was pressed on first (transferee) channel, check if is our transferee channel and continue with d <= device */
		if (channel == device->transferChannels.transferee && device->transferChannels.transferer) {
			d = sccp_device_retain(device);
		} else {
			pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no device on channel %d\n", channel->callid);
			return;
		}
	}

	if (!d->transfer || !channel->line->transfer) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", (d && d->id) ? d->id : "SCCP");
		d = sccp_device_release(d);
		return;
	}

	/* are we in the middle of a transfer? */
	if (d->transferChannels.transferee && d->transferChannels.transferer) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", (d && d->id) ? d->id : "SCCP");
		sccp_channel_transfer_complete(d->transferChannels.transferer);
		d = sccp_device_release(d);
		return;
	}
	/* exceptional case, we need to release half transfer before retaking, should never occur */
	//      if (d->transferChannels.transferee && !d->transferChannels.transferer) {
	//              d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
	//      }
	if (!d->transferChannels.transferee && d->transferChannels.transferer) {
		d->transferChannels.transferer = sccp_channel_release(d->transferChannels.transferer);
	}

	if ((d->transferChannels.transferee = sccp_channel_retain(channel))) {								/** channel to be transfered */
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%08X\n", (d && d->id) ? d->id : "SCCP", (channel->line && channel->line->name) ? channel->line->name : "(null)", channel->callid);

		if (!channel->owner) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No bridged channel to transfer on %s-%08X\n", (d && d->id) ? d->id : "SCCP", (channel->line && channel->line->name) ? channel->line->name : "(null)", channel->callid);
			instance = sccp_device_find_index_for_line(d, channel->line->name);
			sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
			d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
			d = sccp_device_release(d);
			return;
		}
		prev_channel_state = channel->state;
		if ((channel->state != SCCP_CHANNELSTATE_OFFHOOK && channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER) && !sccp_channel_hold(channel)) {
			d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
			d = sccp_device_release(d);
			return;
		}
		if (channel->state != SCCP_CHANNELSTATE_CALLTRANSFER) {
			sccp_indicate(d, channel, SCCP_CHANNELSTATE_CALLTRANSFER);
		}
		if ((sccp_channel_new = sccp_channel_newcall(channel->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND, sccp_channel_getLinkedId(channel)))) {
			/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER */
			if (blindTransfer || (sccp_channel_new && sccp_channel_new->owner && channel->owner && CS_AST_BRIDGED_CHANNEL(channel->owner))) {
				//! \todo use pbx impl
				pbx_builtin_setvar_helper(sccp_channel_new->owner, "BLINDTRANSFER", pbx_channel_name(CS_AST_BRIDGED_CHANNEL(channel->owner)));
				pbx_builtin_setvar_helper(CS_AST_BRIDGED_CHANNEL(channel->owner), "BLINDTRANSFER", pbx_channel_name(sccp_channel_new->owner));

			}
			d->transferChannels.transferer = sccp_channel_retain(sccp_channel_new);
			sccp_channel_new = sccp_channel_release(sccp_channel_new);
		} else {
			// giving up
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: New channel could not be created to complete transfer for %s-%08X\n", (d && d->id) ? d->id : "SCCP", (channel->line && channel->line->name) ? channel->line->name : "(null)", channel->callid);
			sccp_indicate(d, channel, prev_channel_state);
			d->transferChannels.transferee = sccp_channel_release(d->transferChannels.transferee);
		}
	}
	d = sccp_device_release(d);
}

/*!
 * \brief Bridge Two Channels
 * \param sccp_destination_local_channel Local Destination SCCP Channel
 * \todo Find a way solve the chan->state problem
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device
 */
void sccp_channel_transfer_complete(sccp_channel_t * sccp_destination_local_channel)
{
	PBX_CHANNEL_TYPE *pbx_source_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_source_remote_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_remote_channel = NULL;

	sccp_channel_t *sccp_source_local_channel;
	sccp_device_t *d = NULL;
	uint16_t instance;

	if (!sccp_destination_local_channel) {
		return;
	}
	// Obtain the device from which the transfer was initiated
	d = sccp_channel_getDevice_retained(sccp_destination_local_channel);
	if (!sccp_destination_local_channel->line || !d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", sccp_destination_local_channel->callid);
		return;
	}
	// Obtain the source channel on that device
	sccp_source_local_channel = sccp_channel_retain(d->transferChannels.transferee);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%08X\n", d->id, sccp_destination_local_channel->line->name, sccp_destination_local_channel->callid);
	instance = sccp_device_find_index_for_line(d, sccp_destination_local_channel->line->name);

	if (sccp_destination_local_channel->state != SCCP_CHANNELSTATE_RINGOUT && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_CONNECTED) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected. ChannelState: %s (%d)\n", channelstate2str(sccp_destination_local_channel->state), sccp_destination_local_channel->state);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, sccp_destination_local_channel->callid, 0);
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		d = sccp_device_release(d);
		sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
		return;
	}

	if (!sccp_destination_local_channel->owner || !sccp_source_local_channel || !sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%08X and %s-%08X\n",
							      d->id,
							      (sccp_destination_local_channel && sccp_destination_local_channel->line && sccp_destination_local_channel->line->name) ? sccp_destination_local_channel->line->name : "(null)",
							      (sccp_destination_local_channel && sccp_destination_local_channel->callid) ? sccp_destination_local_channel->callid : 0, (sccp_source_local_channel && sccp_source_local_channel->line && sccp_source_local_channel->line->name) ? sccp_source_local_channel->line->name : "(null)", (sccp_source_local_channel && sccp_source_local_channel->callid) ? sccp_source_local_channel->callid : 0);
		d = sccp_device_release(d);
		sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
		return;
	}

	pbx_source_local_channel = sccp_source_local_channel->owner;
	pbx_source_remote_channel = CS_AST_BRIDGED_CHANNEL(sccp_source_local_channel->owner);
	pbx_destination_remote_channel = CS_AST_BRIDGED_CHANNEL(sccp_destination_local_channel->owner);
	pbx_destination_local_channel = sccp_destination_local_channel->owner;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_local_channel       %s\n", d->id, pbx_source_local_channel ? pbx_channel_name(pbx_source_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_remote_channel      %s\n\n", d->id, pbx_source_remote_channel ? pbx_channel_name(pbx_source_remote_channel) : "NULL");

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_local_channel  %s\n", d->id, pbx_destination_local_channel ? pbx_channel_name(pbx_destination_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_remote_channel %s\n\n", d->id, pbx_destination_remote_channel ? pbx_channel_name(pbx_destination_remote_channel) : "NULL");

	if (!(pbx_source_remote_channel && pbx_destination_local_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");

		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		d = sccp_device_release(d);
		sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
		return;
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
		if (PBX(sendRedirectedUpdate)) {
			PBX(sendRedirectedUpdate) (sccp_destination_local_channel, fromNumber, fromName, toNumber, toName, AST_REDIRECTING_REASON_UNCONDITIONAL);
		}
#endif
	}

	if (sccp_destination_local_channel->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, pbx_channel_name(pbx_source_remote_channel));
		pbx_indicate(pbx_source_remote_channel, AST_CONTROL_RINGING);					// Shouldn't this be ALERTING?

		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (Ringing within Transfer %s)\n", pbx_channel_name(pbx_source_remote_channel));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (Transfer destination %s)\n", pbx_channel_name(pbx_destination_local_channel));

		if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING) {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_complete) Send ringing indication to %s\n", pbx_channel_name(pbx_source_remote_channel));
			pbx_indicate(pbx_source_remote_channel, AST_CONTROL_RINGING);
		} else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH) {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_complete) Started music on hold for channel %s\n", pbx_channel_name(pbx_source_remote_channel));
			PBX(moh_start) (pbx_source_remote_channel, NULL, NULL);						//! \todo use pbx impl
		}
	}

	if (pbx_channel_masquerade(pbx_destination_local_channel, pbx_source_remote_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", pbx_channel_name(pbx_destination_local_channel), pbx_channel_name(pbx_source_remote_channel));

		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		d = sccp_device_release(d);
		sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
		return;
	}

	if (!sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free ressources\n");
		d = sccp_device_release(d);
		sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
		return;
	}

	d->transferChannels.transferee = d->transferChannels.transferee ? sccp_channel_release(d->transferChannels.transferee) : NULL;
	d->transferChannels.transferer = d->transferChannels.transferer ? sccp_channel_release(d->transferChannels.transferer) : NULL;

	if (GLOB(transfer_tone) && sccp_destination_local_channel->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* while connected not all the tones can be played */
		sccp_dev_starttone(d, GLOB(autoanswer_tone), instance, sccp_destination_local_channel->callid, 0);
	}
	d = sccp_device_release(d);
	sccp_source_local_channel = sccp_channel_release(sccp_source_local_channel);
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
 * 
 * \lock
 *      - channel
 */
int sccp_channel_forward(sccp_channel_t * sccp_channel_parent, sccp_linedevices_t * lineDevice, char *fwdNumber)
{
	sccp_channel_t *sccp_forwarding_channel = NULL;
	char dialedNumber[256];
	int res = 0;

	if (!sccp_channel_parent) {
		pbx_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return -1;
	}

	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));
	sccp_forwarding_channel = sccp_channel_allocate(sccp_channel_parent->line, NULL);

	if (!sccp_forwarding_channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", lineDevice->device->id);

		return -1;
	}

	sccp_forwarding_channel->parentChannel = sccp_channel_retain(sccp_channel_parent);
	sccp_forwarding_channel->ss_action = SCCP_SS_DIAL;							/* softswitch will catch the number to be dialed */
	sccp_forwarding_channel->ss_data = 0;									// nothing to pass to action
	sccp_forwarding_channel->calltype = SKINNY_CALLTYPE_OUTBOUND;

	/* copy the number to dial in the ast->exten */
	sccp_copy_string(sccp_forwarding_channel->dialedNumber, dialedNumber, sizeof(sccp_forwarding_channel->dialedNumber));
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Incoming: %s Forwarded By: %s Forwarded To: %s", sccp_channel_parent->callInfo.callingPartyName, lineDevice->line->cid_name, dialedNumber);

	/* Copy Channel Capabilities From Predecessor */
	memset(&sccp_forwarding_channel->remoteCapabilities.audio, 0, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memcpy(&sccp_forwarding_channel->remoteCapabilities.audio, sccp_channel_parent->remoteCapabilities.audio, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memset(&sccp_forwarding_channel->preferences.audio, 0, sizeof(sccp_forwarding_channel->preferences.audio));
	memcpy(&sccp_forwarding_channel->preferences.audio, sccp_channel_parent->preferences.audio, sizeof(sccp_channel_parent->preferences.audio));

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(sccp_forwarding_channel, sccp_channel_getLinkedId(sccp_channel_parent))) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", lineDevice->device->id, sccp_forwarding_channel->line->name);
		sccp_line_removeChannel(sccp_forwarding_channel->line, sccp_forwarding_channel);
		sccp_channel_clean(sccp_forwarding_channel);
		//              sccp_channel_destroy(sccp_forwarding_channel);

		res = -1;
		goto EXIT_FUNC;
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
		if (pbx_pbx_start(sccp_forwarding_channel->owner)) {
			pbx_log(LOG_WARNING, "%s: invalide number\n", "SCCP");
		}
	} else {
		pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x cannot dial this number %s\n", "SCCP", sccp_forwarding_channel->line->name, sccp_forwarding_channel->callid, dialedNumber);
		sccp_channel_endcall(sccp_forwarding_channel);
		res = -1;
		goto EXIT_FUNC;
	}
EXIT_FUNC:
	sccp_forwarding_channel = sccp_forwarding_channel ? sccp_channel_release(sccp_forwarding_channel) : NULL;
	return res;
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
		sccp_device_t *d = sccp_channel_getDevice_retained(channel);

		sccp_dev_displaynotify(d, extstr, 10);
		d = sccp_device_release(d);
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

	skinny_codec_t tempCodecPreferences[ARRAY_LEN(c->preferences.audio)];

	if (!data || !c) {
		return FALSE;
	}

	strncpy(text, data, sizeof(text) - 1);

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

/*!
 * \brief Send callwaiting tone to device multiple times
 * \note this caused refcount / segfault issues, when channel would be lost before thread callback
 * \todo find another method to reimplement this.
 */
int sccp_channel_callwaiting_tone_interval(sccp_device_t * device, sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;
	sccp_channel_t *c = NULL;
	int res = -1;

	if (GLOB(callwaiting_tone)) {
		if ((d = sccp_device_retain(device))) {
			if ((c = sccp_channel_retain(channel))) {
				if (c->line && c->line->name) {
					sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Handle Callwaiting Tone on channel %d\n", c->callid);
					if (c && c->owner && (SCCP_CHANNELSTATE_CALLWAITING == c->state || SCCP_CHANNELSTATE_RINGING == c->state)) {
						sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Sending Call Waiting Tone \n", DEV_ID_LOG(d));
						int instance = sccp_device_find_index_for_line(d, c->line->name);

						sccp_dev_starttone(d, GLOB(callwaiting_tone), instance, c->callid, 0);

						//! \todo reimplement callwaiting_interval

						res = 0;
					} else {
						sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval)channel has been hungup or pickup up by another phone\n");
					}
				} else {
					sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No valid line received to handle callwaiting\n");
				}
				c = sccp_channel_release(channel);
			} else {
				sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No valid channel received to handle callwaiting\n");
			}
			d = sccp_device_release(d);
		} else {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No valid device received to handle callwaiting\n");
		}
	} else {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No callwaiting_tone set\n");
	}
	return res;
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
