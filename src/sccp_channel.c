
/*!
 * \file 	sccp_channel.c
 * \brief 	SCCP Channel Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks	Purpose: 	SCCP Channels
 * 		When to use:	Only methods directly related to sccp channels should be stored in this source file.
 *   		Relationships: 	SCCP Channels connect Asterisk Channels to SCCP Lines
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static uint32_t callCount = 1;

AST_MUTEX_DEFINE_STATIC(callCountLock);

/*!
 * \brief Private Channel Data Structure
 */
struct sccp_private_channel_data {
	sccp_device_t *device;
	boolean_t microphone;							/*!< Flag to mute the microphone when calling a baby phone */
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
static void sccp_channel_setMicrophoneState(const sccp_channel_t * channel, boolean_t enabled)
{
	sccp_channel_t *c = (sccp_channel_t *) channel;

	c->privateData->microphone = enabled;

	switch (enabled) {
	case TRUE:
		c->isMicrophoneEnabled = sccp_always_true;
		if (c->privateData->device && (c->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
			sccp_dev_set_microphone(c->privateData->device, SKINNY_STATIONMIC_ON);
		}

		break;
	case FALSE:
		c->isMicrophoneEnabled = sccp_always_false;
		if (c->privateData->device && (c->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) {
			sccp_dev_set_microphone(c->privateData->device, SKINNY_STATIONMIC_OFF);
		}
		break;
	}

}

/*!
 * \brief Allocate SCCP Channel on Device
 * \param l SCCP Line
 * \param device SCCP Device
 * \return a *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- callCountLock
 * 	- channel
 */
sccp_channel_t *sccp_channel_allocate_locked(sccp_line_t * l, sccp_device_t * device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t *channel;

	/* If there is no current line, then we can't make a call in, or out. */
	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: Tried to open channel on a device with no lines\n");
		return NULL;
	}

	if (device && !device->session) {
		pbx_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", device->id);
		return NULL;
	}

	channel = sccp_malloc(sizeof(sccp_channel_t));
	if (!channel) {
		/* error allocating memory */
		pbx_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n", l->id, l->name);
		return NULL;
	}
	memset(channel, 0, sizeof(sccp_channel_t));

	channel->privateData = sccp_malloc(sizeof(struct sccp_private_channel_data));
	if (!channel->privateData) {
		/* error allocating memory */
		pbx_log(LOG_ERROR, "%s: No memory to allocate channel private data on line %s\n", l->id, l->name);
		sccp_free(channel);
		return NULL;
	}
	memset(channel->privateData, 0, sizeof(struct sccp_private_channel_data));
	channel->privateData->microphone = TRUE;

	sccp_mutex_init(&channel->lock);
	sccp_channel_lock(channel);

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

	/* callcount limit should be reset at his upper limit :) */
	if (callCount == 0xFFFFFFFF)
		callCount = 1;

	sccp_mutex_lock(&callCountLock);
	channel->callid = callCount++;
	channel->passthrupartyid = channel->callid ^ 0xFFFFFFFF;
	sccp_mutex_unlock(&callCountLock);

	channel->line = l;
	channel->peerIsSCCP = 0;
	channel->enbloc.digittimeout = GLOB(digittimeout) * 1000;
	channel->maxBitRate = 15000;

	sccp_channel_setDevice(channel, device);

	sccp_line_addChannel(l, channel);

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, channel->callid, l->name);
	channel->getDevice = sccp_channel_getDevice;
	channel->setDevice = sccp_channel_setDevice;

	channel->isMicrophoneEnabled = sccp_always_true;
	channel->setMicrophone = sccp_channel_setMicrophoneState;
	return channel;
}

/*!
 * \brief Retrieve Device from Channels->Private Channel Data
 * \param channel SCCP Channel
 * \return SCCP Device
 */
sccp_device_t *sccp_channel_getDevice(const sccp_channel_t * channel)
{
	return channel->privateData->device;
}

/*!
 * \brief Set Device in Channels->Private Channel Data
 * \param channel SCCP Channel
 * \param device SCCP Device
 */
void sccp_channel_setDevice(sccp_channel_t * channel, const sccp_device_t * device)
{
	channel->privateData->device = (sccp_device_t *) device;

	if (channel->privateData->device) {
		memcpy(&channel->preferences.audio, &channel->privateData->device->preferences.audio, sizeof(channel->preferences.audio));	/* our preferred codec list */
		memcpy(&channel->capabilities.audio, &channel->privateData->device->capabilities.audio, sizeof(channel->capabilities.audio));	/* our capability codec list */
	} else {
		memcpy(&channel->capabilities.audio, &GLOB(global_preferences), sizeof(channel->capabilities.audio));
		memcpy(&channel->preferences.audio, &GLOB(global_preferences), sizeof(channel->preferences.audio));
	}
}

/*!
 * \brief recalculating read format for channel 
 * \param channel a *locked* SCCP Channel
 */
static void sccp_channel_recalculateReadformat(sccp_channel_t * channel)
{

#ifndef CS_EXPERIMENTAL_RTP
	//pbx_log(LOG_NOTICE, "readState %d\n", channel->rtp.audio.readState);
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
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: recalculateReadformat\n", DEV_ID_LOG(sccp_channel_getDevice(channel)));
		channel->rtp.audio.readFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.readFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.readFormat = SKINNY_CODEC_G711_ULAW_64K;

			char s1[512];

			pbx_log(LOG_NOTICE, "can not calculate readFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.readFormat, 1), channel->rtp.audio.readFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		PBX(rtp_setReadFormat) (channel, channel->rtp.audio.readFormat);

	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \nREAD: %s\n",
				 channel->line->name,
				 channel->callid, sccp_multiple_codecs2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)), sccp_multiple_codecs2str(s3, sizeof(s3) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)), sccp_multiple_codecs2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)), sccp_multiple_codecs2str(s2, sizeof(s2) - 1, &channel->rtp.audio.readFormat, 1)
	    );
}

/*!
 * \brief recalculating write format for channel 
 * \param channel a *locked* SCCP Channel
 */
static void sccp_channel_recalculateWriteformat(sccp_channel_t * channel)
{
	//pbx_log(LOG_NOTICE, "writeState %d\n", channel->rtp.audio.writeState);

#ifndef CS_EXPERIMENTAL_RTP
	if (channel->rtp.audio.readState != SCCP_RTP_STATUS_INACTIVE && channel->rtp.audio.readFormat != SKINNY_CODEC_NONE) {
		//pbx_log(LOG_NOTICE, "we already have a read format, dont change codec\n");
		channel->rtp.audio.writeFormat = channel->rtp.audio.readFormat;
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
		return;
	}
#endif
	/* check if remote set a preferred format that is compatible */
	if ((channel->rtp.audio.writeState == SCCP_RTP_STATUS_INACTIVE)
	    || !sccp_utils_isCodecCompatible(channel->rtp.audio.writeFormat, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio))
	    ) {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: recalculateWriteformat\n", DEV_ID_LOG(sccp_channel_getDevice(channel)));

		channel->rtp.audio.writeFormat = sccp_utils_findBestCodec(channel->preferences.audio, ARRAY_LEN(channel->preferences.audio), channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio), channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio));

		if (channel->rtp.audio.writeFormat == SKINNY_CODEC_NONE) {
			channel->rtp.audio.writeFormat = SKINNY_CODEC_G711_ULAW_64K;

			char s1[512];

			pbx_log(LOG_NOTICE, "can not calculate writeFormat, fall back to %s (%d)\n", sccp_multiple_codecs2str(s1, sizeof(s1) - 1, &channel->rtp.audio.writeFormat, 1), channel->rtp.audio.writeFormat);
		}
		//PBX(set_nativeAudioFormats)(channel, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio));
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	} else {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: audio.writeState already active %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->rtp.audio.writeState);
	}
	char s1[512], s2[512], s3[512], s4[512];

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, \ncapabilities: %s \npreferences: %s \nremote caps: %s \n\tWRITE: %s\n",
				 channel->line->name,
				 channel->callid, sccp_multiple_codecs2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)), sccp_multiple_codecs2str(s3, sizeof(s3) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)), sccp_multiple_codecs2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)), sccp_multiple_codecs2str(s2, sizeof(s2) - 1, &channel->rtp.audio.writeFormat, 1)
	    );
}

/*!
 * \brief Update Channel Capability
 * \param channel a *locked* SCCP Channel
 */
void sccp_channel_updateChannelCapability_locked(sccp_channel_t * channel)
{
#ifdef CS_EXPERIMENTAL_CODEC
	//sccp_channel_recalculateReadformat(channel);
#else
	sccp_channel_recalculateReadformat(channel);
	sccp_channel_recalculateWriteformat(channel);
#endif

}

/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t *sccp_channel_get_active_nolock(sccp_device_t * d)
{
	sccp_channel_t *channel;

	if (!d)
		return NULL;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Getting the active channel on device.\n", d->id);

	channel = d->active_channel;

	if (!channel || channel->state == SCCP_CHANNELSTATE_DOWN)
		return NULL;

	return channel;
}

/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP *locked* Channel
 */
sccp_channel_t *sccp_channel_get_active_locked(sccp_device_t * d)
{
	sccp_channel_t *channel = sccp_channel_get_active_nolock(d);

	if (channel)
		sccp_channel_lock(channel);

	return channel;
}

/*!
 * \brief Set SCCP Channel to Active
 * \param d SCCP Device
 * \param channel SCCP Channel
 * 
 * \lock
 * 	- device
 */
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * channel)
{
	if (!d)
		return;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (channel) ? channel->callid : 0);
	sccp_device_lock(d);
	if (channel) {
		d->active_channel = channel;
		d->currentLine = channel->line;
	} else {
		d->active_channel = NULL;
		d->currentLine = NULL;
	}
	sccp_device_unlock(d);
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
 */
void sccp_channel_send_callinfo(sccp_device_t * device, sccp_channel_t * channel)
{

	if (!device || !channel || !device->protocol)
		return;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: send callInfo of callid %d\n", DEV_ID_LOG(device), (channel) ? channel->callid : 0);
	device->protocol->sendCallInfo(device, channel);
}

/*!
 * \brief Send Dialed Number to SCCP Channel device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_send_dialednumber(sccp_channel_t * channel)
{
//      sccp_moo_t *r;
//      sccp_device_t *device;
//      uint8_t instance;
// 
//      if (sccp_strlen_zero(channel->callInfo.calledPartyNumber))
//              return;
// 
//      if (!channel->device)
//              return;
//      device = channel->device;
// 
//      REQ(r, DialedNumberMessage);
// 
//      instance = sccp_device_find_index_for_line(device, channel->line->name);
//      sccp_copy_string(r->msg.DialedNumberMessage.calledParty, channel->callInfo.calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));
// 
//      r->msg.DialedNumberMessage.lel_lineId = htolel(instance);
//      r->msg.DialedNumberMessage.lel_callRef = htolel(channel->callid);
//      sccp_dev_send(device, r);
//      sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", device->id, channel->callInfo.calledPartyNumber, calltype2str(channel->calltype), channel->callid);

	channel->privateData->device->protocol->sendCallInfo(channel->privateData->device, channel);
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

	return;
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
	if (!channel || !&channel->callInfo)
		return;
	sccp_channel_lock(channel);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x callInfo:\n", channel->line->name, channel->callid);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - calledParty: %s <%s>, valid: %s\n", (channel->callInfo.calledPartyName) ? channel->callInfo.calledPartyName : "", (channel->callInfo.calledPartyNumber) ? channel->callInfo.calledPartyNumber : "", (channel->callInfo.calledParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - callingParty: %s <%s>, valid: %s\n", (channel->callInfo.callingPartyName) ? channel->callInfo.callingPartyName : "", (channel->callInfo.callingPartyNumber) ? channel->callInfo.callingPartyNumber : "", (channel->callInfo.callingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCalledParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCalledPartyName) ? channel->callInfo.originalCalledPartyName : "", (channel->callInfo.originalCalledPartyNumber) ? channel->callInfo.originalCalledPartyNumber : "", (channel->callInfo.originalCalledParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCallingParty: %s <%s>, valid: %s\n", (channel->callInfo.originalCallingPartyName) ? channel->callInfo.originalCallingPartyName : "", (channel->callInfo.originalCallingPartyNumber) ? channel->callInfo.originalCallingPartyNumber : "", (channel->callInfo.originalCallingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - lastRedirectingParty: %s <%s>, valid: %s\n", (channel->callInfo.lastRedirectingPartyName) ? channel->callInfo.lastRedirectingPartyName : "", (channel->callInfo.lastRedirectingPartyNumber) ? channel->callInfo.lastRedirectingPartyNumber : "", (channel->callInfo.lastRedirectingParty_valid) ? "TRUE" : "FALSE");
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 " - originalCalledPartyRedirectReason: %d, lastRedirectingReason: %d, CallInfo Presentation: %d\n\n", channel->callInfo.originalCdpnRedirectReason, channel->callInfo.lastRedirectingReason, channel->callInfo.presentation);
	sccp_channel_unlock(channel);
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
	if (!channel)
		return;

	if (name && strncmp(name, channel->callInfo.callingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.callingPartyName, name, sizeof(channel->callInfo.callingPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Name %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.callingPartyName, channel->callid);
	}

	if (number && strncmp(number, channel->callInfo.callingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.callingPartyNumber, number, sizeof(channel->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Number %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.callingPartyNumber, channel->callid);
		channel->callInfo.callingParty_valid = 1;
	}
	return;
}

/*!
 * \brief Set Original Calling Party on SCCP Channel c (Used during Forward)
 * \param channel SCCP Channel
 * \param name Name as char
 * \param number Number as char
 * \return TRUE/FALSE - TRUE if info changed
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_channel_set_originalCallingparty(sccp_channel_t * channel, char *name, char *number)
{
	boolean_t changed = FALSE;

	if (!channel)
		return FALSE;

	if (name && strncmp(name, channel->callInfo.originalCallingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCallingPartyName, name, sizeof(channel->callInfo.originalCallingPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set original Calling Party Name %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.originalCallingPartyName, channel->callid);
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCallingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCallingPartyNumber, number, sizeof(channel->callInfo.originalCallingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set original Calling Party Number %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.originalCallingPartyNumber, channel->callid);
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
	if (!channel)
		return;

	if (name && strncmp(name, channel->callInfo.calledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.calledPartyName, name, sizeof(channel->callInfo.calledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set calledParty Name %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.calledPartyName, channel->callid);
	}

	if (number && strncmp(number, channel->callInfo.calledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.calledPartyNumber, number, sizeof(channel->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set calledParty Number %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.calledPartyNumber, channel->callid);
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

	if (!channel)
		return FALSE;

	if (name && strncmp(name, channel->callInfo.originalCalledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCalledPartyName, name, sizeof(channel->callInfo.originalCalledPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Name %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.originalCalledPartyName, channel->callid);
		changed = TRUE;
	}

	if (number && strncmp(number, channel->callInfo.originalCalledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(channel->callInfo.originalCalledPartyNumber, number, sizeof(channel->callInfo.originalCalledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Number %s on channel %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->callInfo.originalCalledPartyNumber, channel->callid);
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

	if (!channel || !(d = sccp_channel_getDevice(channel)))
		return;

	if (d->protocol->version < 19)
		REQ(r, ConnectionStatisticsReq);
	else
		REQ(r, ConnectionStatisticsReq_V19);

	/*! \todo We need to test what we have to copy in the DirectoryNumber */
	if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, channel->callInfo.calledPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));
	else
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, channel->callInfo.callingPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));

	r->msg.ConnectionStatisticsReq.lel_callReference = htolel((channel) ? channel->callid : 0);
	r->msg.ConnectionStatisticsReq.lel_StatsProcessing = htolel(SKINNY_STATSPROCESSING_CLEAR);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device is Requesting CallStatisticsAndClear\n", (d && d->id) ? d->id : "SCCP");
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
 * 	  - see sccp_channel_updateChannelCapability_locked()
 * 	  - see sccp_channel_start_rtp()
 * 	  - see sccp_device_find_index_for_line()
 * 	  - see sccp_dev_starttone()
 * 	  - see sccp_dev_send()
 */
void sccp_channel_openreceivechannel_locked(sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;

	uint16_t instance;

	if (!channel || !sccp_channel_getDevice(channel))
		return;

	d = channel->privateData->device;

	/* Mute mic feature: If previously set, mute the microphone prior receiving media is already open. */
	/* This must be done in this exact order to work on popular phones like the 7975. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}

	/* calculating format at this point doesn work, because asterisk needs a nativeformat to be set before dial */
#ifdef CS_EXPERIMENTAL_CODEC
	sccp_channel_recalculateWriteformat(channel);
#endif

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: channel %s payloadType %d\n", channel->privateData->device->id, (channel->owner) ? channel->owner->name : "NULL", channel->rtp.audio.writeFormat);

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", channel->privateData->device->id, channel->callid, codec2str(channel->rtp.audio.writeFormat), channel->line->echocancel ? "ON" : "OFF");
	if (!channel->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Starting RTP on channel %s-%08X\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);
		sccp_rtp_createAudioServer(channel);
	}
	if (!channel->rtp.audio.rtp && !sccp_rtp_createAudioServer(channel)) {
		pbx_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%08X\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);

		instance = sccp_device_find_index_for_line(sccp_channel_getDevice(channel), channel->line->name);
		sccp_dev_starttone(channel->privateData->device, SKINNY_TONE_REORDERTONE, instance, channel->callid, 0);
		return;
	}

	if (channel->owner) {
		PBX(set_nativeAudioFormats) (channel, &channel->rtp.audio.writeFormat, 1);
		PBX(rtp_setWriteFormat) (channel, channel->rtp.audio.writeFormat);
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Open receive channel with format %s[%d], payload %d, echocancel: %d\n", channel->privateData->device->id, codec2str(channel->rtp.audio.writeFormat), channel->rtp.audio.writeFormat, channel->rtp.audio.writeFormat, channel->line->echocancel);
	channel->rtp.audio.writeState = SCCP_RTP_STATUS_PROGRESS;
	d->protocol->sendOpenReceiveChannel(d, channel);
#ifdef CS_SCCP_VIDEO
	if (!channel->rtp.video.rtp) {
		sccp_rtp_createVideoServer(channel);
	}
	if (sccp_device_isVideoSupported(channel->privateData->device)) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We can have video, try to start vrtp\n", DEV_ID_LOG(channel->privateData->device));
		if (!channel->rtp.video.rtp && !sccp_rtp_createVideoServer(channel)) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can not start vrtp\n", DEV_ID_LOG(channel->privateData->device));
		} else {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: vrtp started\n", DEV_ID_LOG(channel->privateData->device));
			sccp_channel_startMultiMediaTransmission(channel);
		}
	}
#endif
	//pbx_rtp_set_vars(sccp_channel->owner, sccp_channel->rtp.audio.rtp);
	//sccp_channel_openMultiMediaChannel(sccp_channel);
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

	if (!(d = channel->privateData->device))
		return;

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
	lineInstance = sccp_device_find_index_for_line(channel->privateData->device, channel->line->name);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Open receive multimedia channel with format %s[%d] skinnyFormat %s[%d], payload %d\n", DEV_ID_LOG(channel->privateData->device), codec2str(channel->rtp.video.writeFormat), channel->rtp.video.writeFormat, codec2str(skinnyFormat), skinnyFormat, payloadType);
	d->protocol->sendOpenMultiMediaChannel(d, channel,skinnyFormat,payloadType,lineInstance,bitRate);
	
}

/*!
 * \brief Start Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel)
{
//	sccp_moo_t *r;
	int payloadType;
	sccp_device_t *d = NULL;
	struct sockaddr_in sin;
	struct ast_hostent ahp;
	struct hostent *hp;

	int packetSize = 20;							/*! \todo unused? */

	channel->rtp.video.readFormat = SKINNY_CODEC_H264;
//	packetSize = 3840;
	packetSize = 1920;

	int bitRate = 15000;
	bitRate = channel->maxBitRate;

	if (!channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start vrtp media transmission, maybe channel is down %s-%08X\n", channel->privateData->device->id, channel->line->name, channel->callid);
		return;
	}

	if (!(d = channel->privateData->device))
		return;

	channel->preferences.video[0] = SKINNY_CODEC_H264;
	//channel->preferences.video[0] = SKINNY_CODEC_H263;

	channel->rtp.video.readFormat = sccp_utils_findBestCodec(channel->preferences.video, ARRAY_LEN(channel->preferences.video), channel->capabilities.video, ARRAY_LEN(channel->capabilities.video), channel->remoteCapabilities.video, ARRAY_LEN(channel->remoteCapabilities.video));

	if (channel->rtp.video.readFormat == 0) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: fall back to h264\n", d->id);
		channel->rtp.video.readFormat = SKINNY_CODEC_H264;
	}

	/* lookup payloadType */
	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, channel->rtp.video.readFormat);
	//! \todo handle payload error
	//! \todo use rtp codec map

	sccp_rtp_getUs(&channel->rtp.video, &channel->rtp.video.phone_remote);

	//check if bind address is an global bind address
	if (!channel->rtp.video.phone_remote.sin_addr.s_addr) {
		channel->rtp.video.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: using payload %d\n", d->id, payloadType);

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

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send VRTP media to %s:%d with codec: %s(%d) (%d ms), payloadType %d, tos %d\n", d->id, pbx_inet_ntoa(channel->rtp.video.phone_remote.sin_addr), ntohs(channel->rtp.video.phone_remote.sin_port), codec2str(channel->rtp.video.readFormat), channel->rtp.video.readFormat, packetSize, payloadType, d->audio_tos);
	d->protocol->sendStartMultiMediaTransmission(d, channel, payloadType, bitRate, sin);

	sccp_moo_t *r = sccp_build_packet(FlowControlCommandMessage, sizeof(r->msg.FlowControlCommandMessage));
	r->msg.FlowControlCommandMessage.lel_conferenceID = htolel(channel->callid);
	r->msg.FlowControlCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.FlowControlCommandMessage.lel_callReference = htolel(channel->callid);
	r->msg.FlowControlCommandMessage.maxBitRate = htolel(bitRate);
	sccp_dev_send(d, r);

	PBX(queue_control)(channel->owner, AST_CONTROL_VIDUPDATE);
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
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%08X\n", channel->privateData->device->id, channel->line->name, channel->callid);
		return;
	}

	if (!(d = channel->privateData->device))
		return;

	/* Mute mic feature: If previously set, mute the microphone after receiving of media is already open, but before starting to send to rtp. */
	/* This must be done in this exact order to work also on newer phones like the 8945. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}
	//check if bind address is an global bind address
	//if (!channel->rtp.audio.phone_remote.sin_addr.s_addr) {
	//      channel->rtp.audio.phone_remote.sin_addr.s_addr = d->session->ourip.s_addr;
	//}

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

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to: '%s:%d' with codec: %s(%d), tos %d, silencesuppression: %s\n", d->id, pbx_inet_ntoa(channel->rtp.audio.phone_remote.sin_addr), ntohs(channel->rtp.audio.phone_remote.sin_port), codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat, d->audio_tos, channel->line->silencesuppression ? "ON" : "OFF");
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param channel SCCP Channel
 * \note sccp_channel_stopmediatransmission is explicit call within this function!
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_dev_send()
 */
void sccp_channel_closereceivechannel_locked(sccp_channel_t * channel)
{
	sccp_moo_t *r;
	sccp_device_t *d = channel->privateData->device;

	if (d) {
		REQ(r, CloseReceiveChannel);
		r->msg.CloseReceiveChannel.lel_conferenceId = htolel(channel->callid);
		r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Close receivechannel on channel %d\n", DEV_ID_LOG(d), channel->callid);
	}
	channel->rtp.audio.readState = SCCP_RTP_STATUS_INACTIVE;

	if (channel->rtp.video.rtp) {
		REQ(r, CloseMultiMediaReceiveChannel);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId = htolel(channel->callid);
		r->msg.CloseMultiMediaReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
	}

	sccp_channel_stopmediatransmission_locked(channel);
}

/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param channel SCCP Channel
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_stop_rtp()
 * 	  - see sccp_dev_send()
 */
void sccp_channel_stopmediatransmission_locked(sccp_channel_t * channel)
{
	sccp_moo_t *r;
	sccp_device_t *d = NULL;

	if (!channel)
		return;

	d = channel->privateData->device;

	REQ(r, StopMediaTransmission);
	if (d) {
		r->msg.StopMediaTransmission.lel_conferenceId = htolel(channel->callid);
		r->msg.StopMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StopMediaTransmission.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Stop media transmission on channel %d\n", (d && d->id) ? d->id : "(none)", channel->callid);
	}
	// stopping rtp
	if (channel->rtp.audio.rtp) {
		sccp_rtp_stop(channel);
	}
	channel->rtp.audio.readState = SCCP_RTP_STATUS_INACTIVE;

	// stopping vrtp
	if (channel->rtp.video.rtp) {
		REQ(r, StopMultiMediaTransmission);
		r->msg.StopMultiMediaTransmission.lel_conferenceId = htolel(channel->callid);
		r->msg.StopMultiMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StopMultiMediaTransmission.lel_conferenceId1 = htolel(channel->callid);
		sccp_dev_send(d, r);
	}

	/* requesting statistics */
	sccp_channel_StatisticsRequest(channel);
}

/*!
 * \brief Update Channel Media Type / Native Bridged Format Match
 * \param channel SCCP Channel
 * \note Copied function from v2 (FS)
 */
void sccp_channel_updatemediatype_locked(sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_bridged_channel = NULL;

	/* checking for ast_channel owner */
	if (!channel->owner) {
		return;
	}
	/* is owner ast_channel a zombie ? */
	if (pbx_test_flag(channel->owner, AST_FLAG_ZOMBIE)) {
		return;								/* sccp_channel->owner is zombie, leaving */
	}
	/* channel is hanging up */
	if (channel->owner->_softhangup != 0) {
		return;
	}
	/* channel is not running */
	if (!channel->owner->pbx) {
		return;
	}
	/* check for briged ast_channel */
	if (!(pbx_bridged_channel = CS_AST_BRIDGED_CHANNEL(channel->owner))) {
		return;								/* no bridged channel, leaving */
	}
	/* is bridged ast_channel a zombie ? */
	if (pbx_test_flag(pbx_bridged_channel, AST_FLAG_ZOMBIE)) {
		return;								/* bridged channel is zombie, leaving */
	}
	//! \todo fix directRTP with peer channel
#if 0
	/* channel is hanging up */
	if (pbx_bridged_sccp_channel->_softhangup != 0) {
		return;
	}
	/* channel is not running */
	if (!pbx_bridged_sccp_channel->pbx) {
		return;
	}
	if (channel->state != SCCP_CHANNELSTATE_ZOMBIE) {
		pbx_log(LOG_NOTICE, "%s: Channel %s -> nativeformats:%d - r:%d/w:%d - rr:%d/rw:%d\n", DEV_ID_LOG(channel->privateData->device), pbx_bridged_sccp_channel->name, pbx_bridged_sccp_channel->nativeformats, pbx_bridged_sccp_channel->writeformat, pbx_bridged_sccp_channel->readformat, pbx_bridged_sccp_channel->rawreadformat, pbx_bridged_sccp_channel->rawwriteformat);
		if (!(pbx_bridged_sccp_channel->nativeformats & channel->owner->nativeformats) == (pbx_bridged_sccp_channel->nativeformats & channel->privateData->device->capability)) {
			pbx_log(LOG_NOTICE, "%s: Doing the dirty thing.\n", DEV_ID_LOG(channel->privateData->device));
			channel->owner->nativeformats = channel->format = pbx_bridged_sccp_channel->nativeformats;
			sccp_channel_closereceivechannel_locked(channel);
			usleep(100);
			sccp_channel_openreceivechannel_locked(channel);
			pbx_set_read_format(channel->owner, channel->format);
			pbx_set_write_format(channel->owner, channel->format);
		}
	}
#endif
}

/*!
 * \brief Callback function to destroy an SCCP channel
 * \param data Data cast to SCCP Channel
 * \return success as int
 *
 * \lock
 * 	- channel
 */
int sccp_channel_destroy_callback(const void *data)
{
	sccp_channel_t *channel = (sccp_channel_t *) data;

	if (!channel)
		return 0;

	sccp_channel_lock(channel);
	sccp_channel_destroy_locked(channel);

	return 0;
}

/*!
 * \brief Hangup this channel.
 * \param channel *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- line->channels
 * 	  - see sccp_channel_endcall()
 */
void sccp_channel_endcall_locked(sccp_channel_t * channel)
{
	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}

	/* this is a station active endcall or onhook */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "%s: Ending call %d on line %s (%s)\n", DEV_ID_LOG(channel->privateData->device), channel->callid, channel->line->name, sccp_indicate2str(channel->state));

	/* end all call forward channels (our childs) */
	sccp_channel_t *x;

	SCCP_LIST_LOCK(&channel->line->channels);
	SCCP_LIST_TRAVERSE(&channel->line->channels, x, list) {
		if (x->parentChannel == channel) {
			/* No need to lock because sccp_channel->line->channels is already locked. */
			sccp_channel_endcall_locked(x);
		}
	}
	SCCP_LIST_UNLOCK(&channel->line->channels);

	/**
	workaround to fix issue with 7960 and protocol version != 6
	7960 loses callplane when cancel transfer (end call on other channel).
	This script set the hold state for transfer_channel explicitly -MC
	*/
	if (channel->privateData->device && channel->privateData->device->transfer_channel && channel->privateData->device->transfer_channel != channel) {
		uint8_t instance = sccp_device_find_index_for_line(channel->privateData->device, channel->privateData->device->transfer_channel->line->name);

		sccp_device_sendcallstate(channel->privateData->device, instance, channel->privateData->device->transfer_channel->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_dev_set_keyset(channel->privateData->device, instance, channel->privateData->device->transfer_channel->callid, KEYMODE_ONHOLD);
		channel->privateData->device->transfer_channel = NULL;
	}

	if (channel->owner) {
		PBX(requestHangup) (channel->owner);
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(channel->privateData->device), channel->callid, channel->line->name);
	}
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \return TRUE if the newcall is successful
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_newcall_locked()
 */
boolean_t sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype)
{
	sccp_channel_t *sccp_channel_new;

	sccp_channel_new = sccp_channel_newcall_locked(l, device, dial, calltype);
	if (sccp_channel_new) {
		sccp_channel_unlock(sccp_channel_new);
		return TRUE;
	} else
		return FALSE;
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \return a *locked* SCCP Channel or NULL if something is wrong
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_set_active()
 * 	  - see sccp_indicate_nolock()
 */
sccp_channel_t *sccp_channel_newcall_locked(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype)
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
	if ((channel = sccp_channel_get_active_locked(device)) && (NULL == channel->conference)) {
		/* there is an active call, let's put it on hold first */
		int ret = sccp_channel_hold_locked(channel);

		sccp_channel_unlock(channel);
		if (!ret)
			return NULL;
	}

	channel = sccp_channel_allocate_locked(l, device);

	if (!channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	channel->ss_action = SCCP_SS_DIAL;					/* softswitch will catch the number to be dialed */
	channel->ss_data = 0;							/* nothing to pass to action */

	channel->calltype = calltype;

	sccp_channel_set_active(device, channel);
	sccp_dev_set_activeline(device, l);

	/* copy the number to dial in the ast->exten */
	if (dial) {
		sccp_copy_string(channel->dialedNumber, dial, sizeof(channel->dialedNumber));
		sccp_indicate_locked(device, channel, SCCP_CHANNELSTATE_SPEEDDIAL);
	} else {
		sccp_indicate_locked(device, channel, SCCP_CHANNELSTATE_OFFHOOK);
	}

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate_locked(channel)) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate_locked(device, channel, SCCP_CHANNELSTATE_CONGESTION);
		return channel;
	}

	PBX(set_callstate) (channel, AST_STATE_OFFHOOK);

#if ASTERISK_VERSION_NUMBER >= 10800
	/* create linkid */
	char linkid[50];

	sprintf(linkid, "SCCP::%-10d", channel->callid);
	pbx_string_field_set(channel->owner, linkedid, linkid);
	/* done */
#endif

	if (device->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !channel->rtp.audio.rtp) {
		sccp_channel_openreceivechannel_locked(channel);
	}

	if (dial) {
		sccp_pbx_softswitch_locked(channel);
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
 * \param channel incoming *locked* SCCP channel
 * \todo handle codec choose
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- line
 * 	- line->channels
 * 	  - see sccp_channel_endcall()
 */
void sccp_channel_answer_locked(sccp_device_t * device, sccp_channel_t * channel)
{
	sccp_line_t *l;
	sccp_device_t *d;
	sccp_channel_t *sccp_channel_1;

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

	l = channel->line;
	d = (channel->state == SCCP_CHANNELSTATE_HOLD) ? device : channel->privateData->device;

	/* channel was on hold, restore active -> inc. channelcount */
	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		sccp_line_lock(channel->line);
		channel->line->statistic.numberOfActiveChannels--;
		sccp_line_unlock(channel->line);
	}

	if (!d) {
		if (!device) {
			pbx_log(LOG_ERROR, "SCCP: Channel %d has no device\n", (channel ? channel->callid : 0));
			return;
		}
		d = device;
	}

	sccp_channel_setDevice(channel, d);

//      //! \todo move this to openreceive- and startmediatransmission (we do calc in openreceiv and startmedia, so check if we can remove)
	sccp_channel_updateChannelCapability_locked(channel);

	/* answering an incoming call */
	/* look if we have a call to put on hold */
	if ((sccp_channel_1 = sccp_channel_get_active_locked(d)) != NULL) {
		/* If there is a ringout or offhook call, we end it so that we can answer the call. */
		if (sccp_channel_1->state == SCCP_CHANNELSTATE_OFFHOOK || sccp_channel_1->state == SCCP_CHANNELSTATE_RINGOUT) {
			sccp_channel_endcall_locked(sccp_channel_1);
		} else if (!sccp_channel_hold_locked(sccp_channel_1)) {
			/* there is an active call, let's put it on hold first */
			sccp_channel_unlock(sccp_channel_1);
			return;
		}
		sccp_channel_unlock(sccp_channel_1);
		sccp_channel_1 = NULL;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer the channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);

	/* end callforwards */
	sccp_channel_t *sccp_channel_2;

	SCCP_LIST_LOCK(&channel->line->channels);
	SCCP_LIST_TRAVERSE(&channel->line->channels, sccp_channel_2, list) {
		if (sccp_channel_2->parentChannel == channel) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CHANNEL Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, sccp_channel_2->callid);
			/* No need to lock because sccp_channel->line->channels is already locked. */
			sccp_channel_endcall_locked(sccp_channel_2);
			channel->answered_elsewhere = TRUE;
		}
	}
	SCCP_LIST_UNLOCK(&channel->line->channels);
	/* */

	/** set called party name */
	sccp_linedevices_t *linedevice = sccp_util_getDeviceConfiguration(device, channel->line);

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

	/* done */

	sccp_channel_set_active(d, channel);
	sccp_dev_set_activeline(d, channel->line);

	/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
	/* do we need this ? -FS */
#ifdef CS_AST_HAS_FLAG_MOH
	pbx_bridged_channel = CS_AST_BRIDGED_CHANNEL(channel->owner);
	if (pbx_bridged_channel && pbx_test_flag(pbx_bridged_channel, AST_FLAG_MOH)) {
		PBX(moh_stop)(pbx_bridged_channel);				//! \todo use pbx impl
		pbx_clear_flag(pbx_bridged_channel, AST_FLAG_MOH);
	}
#endif

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answering channel with state '%s' (%d)\n", DEV_ID_LOG(channel->privateData->device), pbx_state2str(channel->owner->_state), channel->owner->_state);
	PBX(queue_control)(channel->owner, AST_CONTROL_ANSWER);

	if (channel->state != SCCP_CHANNELSTATE_OFFHOOK)
		sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_OFFHOOK);

	PBX(set_connected_line) (channel, channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);

	sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_CONNECTED);
}

/*!
 * \brief Put channel on Hold.
 *
 * \param channel *locked* SCCP Channel
 * \return Status as in (0 if something was wrong, otherwise 1)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 */
int sccp_channel_hold_locked(sccp_channel_t * channel)
{
	sccp_line_t *l;
	sccp_device_t *d;
	uint16_t instance;

	if (!channel)
		return 0;

	if (!channel->line || !channel->privateData->device) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", channel->callid);
		return 0;
	}

	l = channel->line;
	d = channel->privateData->device;

	if (channel->state == SCCP_CHANNELSTATE_HOLD) {
		pbx_log(LOG_WARNING, "SCCP: Channel already on hold\n");
		return 0;
	}

	/* put on hold an active call */
	if (channel->state != SCCP_CHANNELSTATE_CONNECTED && channel->state != SCCP_CHANNELSTATE_PROCEED) {	// TOLL FREE NUMBERS STAYS ALWAYS IN CALL PROGRESS STATE
		/* something wrong on the code let's notify it for a fix */
		pbx_log(LOG_ERROR, "%s can't put on hold an inactive channel %s-%08X (%s)\n", d->id, l->name, channel->callid, sccp_indicate2str(channel->state));
		/* hard button phones need it */
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		return 0;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold the channel %s-%08X\n", d->id, l->name, channel->callid);

	if (channel->owner) 
		PBX(queue_control_data)(channel->owner, AST_CONTROL_HOLD, S_OR(l->musicclass, NULL), !ast_strlen_zero(l->musicclass) ? strlen(l->musicclass) + 1 : 0);

	sccp_device_lock(d);
//	d->active_channel = NULL;
	sccp_channel_set_active(d, NULL);
	sccp_device_unlock(d);
	sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_HOLD);		// this will also close (but not destroy) the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: On\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", channel->owner->name, channel->owner->uniqueid);
#endif

	if (l) {
		l->statistic.numberOfActiveChannels--;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);

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
 * 	- device
 * 	- channel
 * 	  - see sccp_channel_updateChannelCapability_locked()
 * 	- channel
 */
int sccp_channel_resume_locked(sccp_device_t * device, sccp_channel_t * channel, boolean_t swap_channels)
{
	sccp_line_t *l;
	sccp_device_t *d;
	sccp_channel_t *sccp_channel_on_hold;
	uint16_t instance;

	if (!channel || !channel->owner)
		return 0;

	if (!channel->line || !channel->privateData->device) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", channel->callid);
		return 0;
	}

	l = channel->line;
	d = channel->privateData->device;

	/* on remote device pickups the call */
	if (d != device)
		d = device;

	/* look if we have a call to put on hold */
	if (swap_channels && (sccp_channel_on_hold = sccp_channel_get_active_locked(d))) {
		/* there is an active call, let's put it on hold first */
		int ret = sccp_channel_hold_locked(sccp_channel_on_hold);

		sccp_channel_unlock(sccp_channel_on_hold);
		sccp_channel_on_hold = NULL;

		if (!ret)
			return 0;
	}

	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_PROCEED) {
		sccp_channel_hold_locked(channel);
	}

	/* resume an active call */
	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER && channel->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong on the code let's notify it for a fix */
		pbx_log(LOG_ERROR, "%s can't resume the channel %s-%08X. Not on hold\n", d->id, l->name, channel->callid);
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, channel->callid, "No active call to put on hold", 5);
		return 0;
	}

	/* check if we are in the middle of a transfer */
	sccp_device_lock(d);
	if (d->transfer_channel == channel) {
		d->transfer_channel = NULL;
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%08X\n", d->id, l->name, channel->callid);
	}

	if (d->conference_channel == channel) {
		d->conference_channel = NULL;
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Conference on the channel %s-%08X\n", d->id, l->name, channel->callid);
	}

	sccp_device_unlock(d);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume the channel %s-%08X\n", d->id, l->name, channel->callid);

	if (channel->owner)
		PBX(queue_control)(channel->owner, AST_CONTROL_UNHOLD);

	sccp_rtp_stop(channel);
	channel->setDevice(channel, d);
	sccp_channel_set_active(d, channel);

	//! \todo move this to openreceive- and startmediatransmission
	sccp_channel_updateChannelCapability_locked(channel);

	channel->state = SCCP_CHANNELSTATE_HOLD;
	sccp_rtp_createAudioServer(channel);

	sccp_channel_set_active(d, channel);
#ifdef CS_AST_CONTROL_SRCUPDATE
	PBX(queue_control)(channel->owner, AST_CONTROL_SRCUPDATE);			// notify changes e.g codec
#endif
	sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_CONNECTED);		// this will also reopen the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: Off\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", channel->owner->name, channel->owner->uniqueid);
#endif

	/* state of channel is set down from the remoteDevices, so correct channel state */
	channel->state = SCCP_CHANNELSTATE_CONNECTED;
	if (l) {
		l->statistic.numberOfHoldChannels--;
	}

	/** set called party name */
	sccp_linedevices_t *linedevice = sccp_util_getDeviceConfiguration(channel->privateData->device, channel->line);

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
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set callingPartyNumber '%s' callingPartyName '%s'\n", DEV_ID_LOG(channel->privateData->device), channel->callInfo.callingPartyNumber, channel->callInfo.callingPartyName);
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
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set calledPartyNumber '%s' calledPartyName '%s'\n", DEV_ID_LOG(channel->privateData->device), channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName);
		PBX(set_connected_line) (channel, channel->callInfo.calledPartyNumber, channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);
	}
	/* */

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);
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
 * 	- device
 * 	  - see sccp_device_find_selectedchannel()
 * 	  - device->selectedChannels
 */
void sccp_channel_clean_locked(sccp_channel_t * channel)			// we assume channel is locked
{
//      sccp_line_t *l;
	sccp_device_t *d;
	sccp_selectedchannel_t *sccp_selected_channel;

	if (!channel)
		return;

	d = channel->privateData->device;
//      l = channel->line;
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Cleaning channel %08x\n", channel->callid);

	/* mark the channel DOWN so any pending thread will terminate */
	if (channel->owner) {
		pbx_setstate(channel->owner, AST_STATE_DOWN);
		channel->owner = NULL;
	}

	/* this is in case we are destroying the session */
	if (channel->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_ONHOOK);

	sccp_rtp_stop(channel);

	if (d) {
		/* deactive the active call if needed */
		sccp_device_lock(d);

		if (d->active_channel == channel)
			d->active_channel = NULL;
		if (d->transfer_channel == channel)
			d->transfer_channel = NULL;
		if (d->conference_channel == channel)
			d->conference_channel = NULL;

		if (channel->privacy) {
			channel->privacy = FALSE;
			d->privacyFeature.status = FALSE;
			sccp_feat_changed(d, SCCP_FEATURE_PRIVACY);
		}

		if ((sccp_selected_channel = sccp_device_find_selectedchannel(d, channel))) {
			SCCP_LIST_LOCK(&d->selectedChannels);
			sccp_selected_channel = SCCP_LIST_REMOVE(&d->selectedChannels, sccp_selected_channel, list);
			SCCP_LIST_UNLOCK(&d->selectedChannels);
			sccp_free(sccp_selected_channel);
		}
		sccp_device_unlock(d);
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
 * 	- line->channels is not always locked
 * 
 * \lock
 * 	- channel
 */
void sccp_channel_destroy_locked(sccp_channel_t * channel)
{
	if (!channel)
		return;

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Destroying channel %08x\n", channel->callid);

	sccp_channel_unlock(channel);
	sccp_mutex_destroy(&channel->lock);
	pbx_cond_destroy(&channel->astStateCond);
	sccp_free(channel);

	return;
}

/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param channel *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 */
void sccp_channel_transfer_locked(sccp_channel_t * channel)
{
	sccp_device_t *d;
	sccp_channel_t *sccp_channel_new = NULL;
	uint32_t blindTransfer = 0;
	uint16_t instance;

	if (!channel)
		return;

	if (!channel->line || !channel->privateData->device) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", channel->callid);
		return;
	}

	d = channel->privateData->device;

	if (!d->transfer || !channel->line->transfer) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", (d && d->id) ? d->id : "SCCP");
		return;
	}

	sccp_device_lock(d);
	/* are we in the middle of a transfer? */
	if (d->transfer_channel && (d->transfer_channel != channel)) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", (d && d->id) ? d->id : "SCCP");
		sccp_device_unlock(d);
		sccp_channel_transfer_complete(channel);
		return;
	}

	d->transfer_channel = channel;
	sccp_device_unlock(d);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%08X\n", (d && d->id) ? d->id : "SCCP", (channel->line && channel->line->name) ? channel->line->name : "(null)", channel->callid);

	if (!channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No bridged channel to transfer on %s-%08X\n", (d && d->id) ? d->id : "SCCP", (channel->line && channel->line->name) ? channel->line->name : "(null)", channel->callid);
		instance = sccp_device_find_index_for_line(d, channel->line->name);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}
	if ((channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER) && !sccp_channel_hold_locked(channel))
		return;
	if (channel->state != SCCP_CHANNELSTATE_CALLTRANSFER)
		sccp_indicate_locked(d, channel, SCCP_CHANNELSTATE_CALLTRANSFER);
	sccp_channel_new = sccp_channel_newcall_locked(channel->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
	/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER */
	if (blindTransfer || (sccp_channel_new && sccp_channel_new->owner && channel->owner && CS_AST_BRIDGED_CHANNEL(channel->owner))) {

		//! \todo use pbx impl
		pbx_builtin_setvar_helper(sccp_channel_new->owner, "BLINDTRANSFER", CS_AST_BRIDGED_CHANNEL(channel->owner)->name);
		pbx_builtin_setvar_helper(CS_AST_BRIDGED_CHANNEL(channel->owner), "BLINDTRANSFER", sccp_channel_new->owner->name);

	}
	sccp_channel_unlock(sccp_channel_new);
}

/*!
 * \brief SCCP Dual Structure
 */
struct sccp_dual {
	PBX_CHANNEL_TYPE *pbx_channel_1;
	PBX_CHANNEL_TYPE *pbx_channel_2;
};

/*!
 * \brief SCCP Dual Structure for transfer thread
 */
struct sccp_dual_transfer {
	PBX_CHANNEL_TYPE *transfered;						/*!< transfered channel */
	PBX_CHANNEL_TYPE *destination;
};

/*!
 * \brief Handle Transfer Ringing Thread
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- asterisk channel
 */
static void *sccp_channel_transfer_ringing_thread(void *data)
{
	struct sccp_dual_transfer *dual = data;

	if (!data || !dual->transfered)
		return NULL;

	pbx_channel_lock(dual->transfered);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (Ringing within Transfer %s(%p)\n", dual->transfered->name, dual->transfered);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (Transfer destination %s(%p)\n", dual->destination->name, dual->destination);

	if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Send ringing indication to %s(%p)\n", dual->transfered->name, (void *)dual->transfered);
		pbx_indicate(dual->transfered, AST_CONTROL_RINGING);

	} else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Started music on hold for channel %s(%p)\n", dual->transfered->name, (void *)dual->transfered);
		PBX(moh_start)(dual->transfered, NULL, NULL);			//! \todo use pbx impl
	}
	ast_channel_unlock(dual->transfered);
	sccp_free(dual);

	return NULL;
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
 * 	- device
 */
void sccp_channel_transfer_complete(sccp_channel_t * sccp_destination_local_channel)
{
	PBX_CHANNEL_TYPE *pbx_source_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_source_remote_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_local_channel = NULL;
	PBX_CHANNEL_TYPE *pbx_destination_remote_channel = NULL;

	sccp_channel_t *sccp_source_local_channel;
	sccp_channel_t *sccp_source_remote_channel = NULL;
	sccp_channel_t *sccp_destination_remote_channel;

	sccp_device_t *d = NULL;
	pthread_attr_t attr;
	pthread_t t;
	uint16_t instance;

	if (!sccp_destination_local_channel)
		return;

	if (!sccp_destination_local_channel->line || !sccp_destination_local_channel->privateData->device) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", sccp_destination_local_channel->callid);
		return;
	}
	// Obtain the device from which the transfer was initiated
	d = sccp_destination_local_channel->privateData->device;
	sccp_device_lock(d);
	// Obtain the source channel on that device
	sccp_source_local_channel = d->transfer_channel;
	sccp_device_unlock(d);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%08X\n", d->id, sccp_destination_local_channel->line->name, sccp_destination_local_channel->callid);
	instance = sccp_device_find_index_for_line(d, sccp_destination_local_channel->line->name);

	if (sccp_destination_local_channel->state != SCCP_CHANNELSTATE_RINGOUT && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_CONNECTED) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected\n");
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, sccp_destination_local_channel->callid, 0);
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (!sccp_destination_local_channel->owner || !sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%08X and %s-%08X\n", d->id, sccp_destination_local_channel->line->name, sccp_destination_local_channel->callid, sccp_source_local_channel->line->name, sccp_source_local_channel->callid);
		return;
	}

	pbx_source_local_channel = sccp_source_local_channel->owner;
	pbx_source_remote_channel = CS_AST_BRIDGED_CHANNEL(sccp_source_local_channel->owner);
	pbx_destination_remote_channel = CS_AST_BRIDGED_CHANNEL(sccp_destination_local_channel->owner);
	pbx_destination_local_channel = sccp_destination_local_channel->owner;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_local_channel       %s\n", d->id, pbx_source_local_channel ? pbx_source_local_channel->name : "NULL");
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_remote_channel      %s\n\n", d->id, pbx_source_remote_channel ? pbx_source_remote_channel->name : "NULL");

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_local_channel  %s\n", d->id, pbx_destination_local_channel ? pbx_destination_local_channel->name : "NULL");
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_remote_channel %s\n\n", d->id, pbx_destination_remote_channel ? pbx_destination_remote_channel->name : "NULL");

	if (!pbx_source_remote_channel || !pbx_destination_local_channel) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");

		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}
	sccp_channel_display_callInfo(sccp_destination_local_channel);
	sccp_channel_display_callInfo(sccp_source_remote_channel);

	if (sccp_destination_local_channel->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, pbx_source_remote_channel->name);
		pbx_indicate(pbx_source_remote_channel, AST_CONTROL_RINGING);	// Shouldn't this be ALERTING?

		/* update connected line */
		if (sccp_destination_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
			PBX(set_connected_line) (sccp_destination_local_channel, sccp_source_local_channel->callInfo.calledPartyNumber, sccp_source_local_channel->callInfo.calledPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER_ALERTING);
		} else {
			PBX(set_connected_line) (sccp_destination_local_channel, sccp_source_local_channel->callInfo.callingPartyNumber, sccp_source_local_channel->callInfo.callingPartyName, AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER_ALERTING);
		}

		/*
		   PBX(set_callerid_name)(sccp_destination_local_channel, sccp_destination_local_channel->callInfo.calledPartyName);
		   PBX(set_callerid_number)(sccp_destination_local_channel, sccp_destination_local_channel->callInfo.calledPartyNumber);

		   PBX(set_callerid_redirectingParty)(sccp_destination_local_channel, "30", "Test 30");
		   PBX(set_callerid_redirectedParty)(sccp_destination_local_channel, "18", "Test 18"); */

		/* starting the ringing thread */
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		struct sccp_dual_transfer *dual;

		dual = malloc(sizeof(struct sccp_dual_transfer));
		dual->transfered = pbx_source_remote_channel;
		dual->destination = pbx_destination_local_channel;

		if (pbx_pthread_create(&t, &attr, sccp_channel_transfer_ringing_thread, dual)) {
			pbx_log(LOG_WARNING, "%s: Unable to create thread for the blind transfer ring indication. %s\n", d->id, strerror(errno));
		}

		/* changing callerid for source part */
		sccp_source_remote_channel = CS_AST_CHANNEL_PVT(pbx_source_remote_channel);
		if (sccp_source_remote_channel) {
			if (CS_AST_CHANNEL_PVT_IS_SCCP(pbx_source_remote_channel)) {
				// SCCP CallerID Exchange
				if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
					/* copy old callerid */
					sccp_copy_string(sccp_source_remote_channel->callInfo.originalCalledPartyName, sccp_source_remote_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.originalCalledPartyName));
					sccp_copy_string(sccp_source_remote_channel->callInfo.originalCalledPartyNumber, sccp_source_remote_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.originalCalledPartyNumber));
					sccp_source_remote_channel->callInfo.originalCalledParty_valid = sccp_source_remote_channel->callInfo.calledParty_valid;

					sccp_copy_string(sccp_source_remote_channel->callInfo.calledPartyName, sccp_destination_local_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.calledPartyName));
					sccp_copy_string(sccp_source_remote_channel->callInfo.calledPartyNumber, sccp_destination_local_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.calledPartyNumber));
					sccp_source_remote_channel->callInfo.calledParty_valid = sccp_source_remote_channel->callInfo.calledParty_valid;

				} else if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
					/* copy old callerid */
					sccp_copy_string(sccp_source_remote_channel->callInfo.originalCallingPartyName, sccp_source_remote_channel->callInfo.callingPartyName, sizeof(sccp_source_remote_channel->callInfo.originalCallingPartyName));
					sccp_copy_string(sccp_source_remote_channel->callInfo.originalCallingPartyNumber, sccp_source_remote_channel->callInfo.callingPartyNumber, sizeof(sccp_source_remote_channel->callInfo.originalCallingPartyNumber));
					sccp_source_remote_channel->callInfo.originalCallingParty_valid = sccp_source_remote_channel->callInfo.callingParty_valid;

					sccp_copy_string(sccp_source_remote_channel->callInfo.callingPartyName, sccp_destination_local_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.callingPartyName));
					sccp_copy_string(sccp_source_remote_channel->callInfo.callingPartyNumber, sccp_destination_local_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.callingPartyNumber));
					sccp_source_remote_channel->callInfo.callingParty_valid = sccp_source_remote_channel->callInfo.calledParty_valid;
				}

				sccp_channel_send_callinfo(sccp_source_remote_channel->privateData->device, sccp_source_remote_channel);
			} else {
				// Other Tech->Type CallerID Exchange
				/*! \todo how about other types like SIP and IAX... How are we going to implement the callerid exchange for them. */
				sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Blind %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(pbx_source_remote_channel));
			}
		}

	}

	if (pbx_channel_masquerade(pbx_destination_local_channel, pbx_source_remote_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", pbx_destination_local_channel->name, pbx_source_remote_channel->name);

		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (!sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free ressources\n");
		return;
	}

	sccp_device_lock(d);
	d->transfer_channel = NULL;
	sccp_device_unlock(d);

	if (!pbx_destination_remote_channel) {
		/* the channel was ringing not answered yet. BLIND TRANSFER */
// TEST
//              if(sccp_destination_local_channel->rtp)
//                      sccp_channel_destroy_rtp(sccp_destination_local_channel);
		return;
	}

	/* it's a SCCP channel destination on transfer */
	sccp_destination_remote_channel = CS_AST_CHANNEL_PVT(pbx_destination_remote_channel);

	/* change callInfo on our destination */
	if (sccp_destination_remote_channel) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Transfer for Channel Type %s\n", CS_AST_CHANNEL_PVT_TYPE(pbx_source_remote_channel));

		if (CS_AST_CHANNEL_PVT_IS_SCCP(pbx_destination_remote_channel)) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer confirmation destination on channel %s\n", d->id, pbx_destination_remote_channel->name);

			/* change callInfo on destination part */
			sccp_copy_string(sccp_destination_remote_channel->callInfo.originalCallingPartyName, sccp_destination_local_channel->callInfo.callingPartyName, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyName));
			sccp_copy_string(sccp_destination_remote_channel->callInfo.originalCallingPartyNumber, sccp_destination_local_channel->callInfo.callingPartyNumber, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyNumber));
			sccp_destination_remote_channel->callInfo.originalCallingParty_valid = sccp_source_local_channel->callInfo.callingParty_valid;

			if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				sccp_copy_string(sccp_destination_remote_channel->callInfo.callingPartyName, sccp_source_local_channel->callInfo.callingPartyName, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyName));
				sccp_copy_string(sccp_destination_remote_channel->callInfo.callingPartyNumber, sccp_source_local_channel->callInfo.callingPartyNumber, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyNumber));
				sccp_destination_remote_channel->callInfo.callingParty_valid = sccp_source_local_channel->callInfo.callingParty_valid;
			} else {
				sccp_copy_string(sccp_destination_remote_channel->callInfo.callingPartyName, sccp_source_local_channel->callInfo.calledPartyName, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyName));
				sccp_copy_string(sccp_destination_remote_channel->callInfo.callingPartyNumber, sccp_source_local_channel->callInfo.calledPartyNumber, sizeof(sccp_destination_remote_channel->callInfo.originalCallingPartyNumber));
				sccp_destination_remote_channel->callInfo.callingParty_valid = sccp_source_local_channel->callInfo.calledParty_valid;
			}
			sccp_channel_send_callinfo(sccp_destination_remote_channel->privateData->device, sccp_destination_remote_channel);

			sccp_copy_string(sccp_destination_remote_channel->callInfo.lastRedirectingPartyName, sccp_destination_local_channel->callInfo.calledPartyName, sizeof(sccp_destination_remote_channel->callInfo.lastRedirectingPartyName));
			sccp_copy_string(sccp_destination_remote_channel->callInfo.lastRedirectingPartyNumber, sccp_destination_local_channel->callInfo.calledPartyNumber, sizeof(sccp_destination_remote_channel->callInfo.lastRedirectingPartyNumber));
			sccp_destination_remote_channel->callInfo.lastRedirectingParty_valid=-1;

		} else {
			pbx_set_callerid(pbx_destination_remote_channel, sccp_source_local_channel->callInfo.callingPartyNumber, sccp_source_local_channel->callInfo.callingPartyName, NULL);
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(pbx_destination_remote_channel));
		}								// no sccp channel
	}
	/* change callInfo on our source */
	sccp_source_remote_channel = CS_AST_CHANNEL_PVT(pbx_source_remote_channel);
	if (sccp_source_remote_channel) {
		if (CS_AST_CHANNEL_PVT_IS_SCCP(pbx_source_remote_channel)) {	/* change callInfo on our source */
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer confirmation source on channel %s\n", DEV_ID_LOG(d), pbx_source_remote_channel->name);

			if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				/* copy old callerid */
				sccp_copy_string(sccp_source_remote_channel->callInfo.originalCalledPartyName, sccp_source_remote_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.originalCalledPartyName));
				sccp_copy_string(sccp_source_remote_channel->callInfo.originalCalledPartyNumber, sccp_source_remote_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.originalCalledPartyNumber));
				sccp_source_remote_channel->callInfo.originalCalledParty_valid = sccp_source_remote_channel->callInfo.calledParty_valid;
				
				sccp_copy_string(sccp_source_remote_channel->callInfo.calledPartyName, sccp_destination_local_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.calledPartyName));
				sccp_copy_string(sccp_source_remote_channel->callInfo.calledPartyNumber, sccp_destination_local_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.calledPartyNumber));
				sccp_source_remote_channel->callInfo.calledParty_valid = sccp_destination_local_channel->callInfo.calledParty_valid;

				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set originalCalledPartyNumber %s, calledPartyNumber %s\n", DEV_ID_LOG(sccp_source_remote_channel->privateData->device), sccp_source_remote_channel->callInfo.originalCalledPartyNumber, sccp_source_remote_channel->callInfo.calledPartyNumber);

			} else if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				/* copy old callerid */
				sccp_copy_string(sccp_source_remote_channel->callInfo.originalCallingPartyName, sccp_source_remote_channel->callInfo.callingPartyName, sizeof(sccp_source_remote_channel->callInfo.originalCallingPartyName));
				sccp_copy_string(sccp_source_remote_channel->callInfo.originalCallingPartyNumber, sccp_source_remote_channel->callInfo.callingPartyNumber, sizeof(sccp_source_remote_channel->callInfo.originalCallingPartyNumber));
				sccp_source_remote_channel->callInfo.originalCallingParty_valid = sccp_source_remote_channel->callInfo.callingParty_valid;

				sccp_copy_string(sccp_source_remote_channel->callInfo.callingPartyName, sccp_destination_local_channel->callInfo.calledPartyName, sizeof(sccp_source_remote_channel->callInfo.callingPartyName));
				sccp_copy_string(sccp_source_remote_channel->callInfo.callingPartyNumber, sccp_destination_local_channel->callInfo.calledPartyNumber, sizeof(sccp_source_remote_channel->callInfo.callingPartyNumber));
				sccp_source_remote_channel->callInfo.callingParty_valid = sccp_destination_local_channel->callInfo.calledParty_valid;

				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set originalCalledPartyNumber %s, calledPartyNumber %s\n", DEV_ID_LOG(sccp_source_remote_channel->privateData->device), sccp_source_remote_channel->callInfo.originalCalledPartyNumber, sccp_source_remote_channel->callInfo.calledPartyNumber);
			}

			sccp_channel_send_callinfo(sccp_source_remote_channel->privateData->device, sccp_source_remote_channel);
		} else {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(pbx_source_remote_channel));
			if (CS_AST_CHANNEL_PVT_CMP_TYPE(pbx_source_remote_channel, "SIP")) {
//                              sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "SCCP: %s Transfer, cid_num '%s', cid_name '%s'\n", CS_AST_CHANNEL_PVT_TYPE(pbx_source_remote_channel), pbx_source_remote_channel->cid_num, pbx_source_remote_channel->cid_name);
				;
			}
		}								// if (CS_AST_CHANNEL_PVT_IS_SCCP(pbx_source_remote_channel)) {
	}
	sccp_channel_display_callInfo(sccp_destination_local_channel);
//	sccp_channel_display_callInfo(sccp_source_remote_channel);
	if (GLOB(transfer_tone) && sccp_destination_local_channel->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* while connected not all the tones can be played */
		sccp_dev_starttone(sccp_destination_local_channel->privateData->device, GLOB(autoanswer_tone), instance, sccp_destination_local_channel->callid, 0);
	}
}

/*!
 * \brief Reset Caller Id Presentation
 * \param channel SCCP Channel
 */
void sccp_channel_reset_calleridPresenceParameter(sccp_channel_t * channel)
{
	channel->callInfo.presentation = CALLERID_PRESENCE_ALLOWED;
	if (PBX(set_callerid_presence))
		PBX(set_callerid_presence) (channel);
}

/*!
 * \brief Set Caller Id Presentation
 * \param channel SCCP Channel
 * \param presenceParameter SCCP CallerID Presence ENUM
 */
void sccp_channel_set_calleridPresenceParameter(sccp_channel_t * channel, sccp_calleridpresence_t presenceParameter)
{
	if (PBX(set_callerid_presence))
		PBX(set_callerid_presence) (channel);
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
 * 	- channel
 */
void sccp_channel_forward(sccp_channel_t * sccp_channel_parent, sccp_linedevices_t * lineDevice, char *fwdNumber)
{
	sccp_channel_t *sccp_forwarding_channel = NULL;
	char dialedNumber[256];

	if (!sccp_channel_parent) {
		pbx_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return;
	}

	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));

	sccp_forwarding_channel = sccp_channel_allocate_locked(sccp_channel_parent->line, NULL);

	if (!sccp_forwarding_channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", lineDevice->device->id);
		return;
	}

	sccp_forwarding_channel->parentChannel = sccp_channel_parent;
	sccp_forwarding_channel->ss_action = SCCP_SS_DIAL;			/* softswitch will catch the number to be dialed */
	sccp_forwarding_channel->ss_data = 0;					// nothing to pass to action
	sccp_forwarding_channel->calltype = SKINNY_CALLTYPE_OUTBOUND;

	/* copy the number to dial in the ast->exten */
	sccp_copy_string(sccp_forwarding_channel->dialedNumber, dialedNumber, sizeof(sccp_forwarding_channel->dialedNumber));
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Incoming: %s Forwarded By: %s Forwarded To: %s", sccp_channel_parent->callInfo.callingPartyName, lineDevice->line->cid_name, dialedNumber);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate_locked(sccp_forwarding_channel)) {
		pbx_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", lineDevice->device->id, sccp_forwarding_channel->line->name);
		sccp_line_removeChannel(sccp_forwarding_channel->line, sccp_forwarding_channel);
		sccp_channel_clean_locked(sccp_forwarding_channel);
		sccp_channel_destroy_locked(sccp_forwarding_channel);
		return;
	}

	/* setting callerid */
//      char fwd_from_name[254];
	//sprintf(fwd_from_name, "%s -> %s", lineDevice->line->cid_num, sccp_channel_parent->callInfo.callingPartyName);

	if (PBX(set_callerid_number))
		PBX(set_callerid_number) (sccp_forwarding_channel, sccp_channel_parent->callInfo.callingPartyNumber);

	if (PBX(set_callerid_name))
		PBX(set_callerid_name) (sccp_forwarding_channel, sccp_channel_parent->callInfo.callingPartyName);

	if (PBX(set_callerid_ani))
		PBX(set_callerid_ani) (sccp_forwarding_channel, dialedNumber);

	if (PBX(set_callerid_dnid))
		PBX(set_callerid_dnid) (sccp_forwarding_channel, dialedNumber);

	if (PBX(set_callerid_redirectedParty))
		PBX(set_callerid_redirectedParty) (sccp_forwarding_channel, sccp_channel_parent->callInfo.calledPartyNumber, sccp_channel_parent->callInfo.calledPartyName);

	if (PBX(set_callerid_redirectingParty))
		PBX(set_callerid_redirectingParty) (sccp_forwarding_channel, sccp_forwarding_channel->line->cid_num, sccp_forwarding_channel->line->cid_name);

	/* dial sccp_forwarding_channel */
	sccp_copy_string(sccp_forwarding_channel->owner->exten, dialedNumber, sizeof(sccp_forwarding_channel->owner->exten));

	/* \todo copy device line setvar variables from parent channel to forwarder->owner */

	//PBX(set_callstate)(sccp_forwarding_channel, AST_STATE_OFFHOOK);
	PBX(set_callstate) (sccp_forwarding_channel, AST_STATE_OFFHOOK);
	if (!sccp_strlen_zero(dialedNumber)
	    && PBX(checkhangup) (sccp_forwarding_channel)
	    && pbx_exists_extension(sccp_forwarding_channel->owner, sccp_forwarding_channel->line->context, dialedNumber, 1, sccp_forwarding_channel->line->cid_num)) {
		/* found an extension, let's dial it */
		pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x is dialing number %s\n", "SCCP", sccp_forwarding_channel->line->name, sccp_forwarding_channel->callid, strdup(dialedNumber));
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		PBX(set_callstate) (sccp_forwarding_channel, AST_STATE_RING);
		if (pbx_pbx_start(sccp_forwarding_channel->owner)) {
			pbx_log(LOG_WARNING, "%s: invalide number\n", "SCCP");
		}
	}

	sccp_channel_unlock(sccp_forwarding_channel);
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
		sccp_dev_displaynotify(channel->privateData->device, extstr, 10);
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

	if (!data || !c)
		return FALSE;

	strncpy(text, data, sizeof(text) - 1);

	/* save original preferences */
	memcpy(&tempCodecPreferences, c->preferences.audio, sizeof(c->preferences.audio));

	for (x = 0; x < ARRAY_LEN(skinny_codecs) && numFoundCodecs < ARRAY_LEN(c->preferences.audio); x++) {
		if (!strcasecmp(skinny_codecs[x].shortname, text)) {

			c->preferences.audio[numFoundCodecs] = skinny_codecs[x].codec;
			numFoundCodecs++;
			/* we can have multiple codec versions, so dont break on this step */

			//! \todo we should remove our prefs from original list -MC
		}
	}
	memcpy(&c->preferences.audio[numFoundCodecs], tempCodecPreferences, sizeof(skinny_codec_t) * (ARRAY_LEN(c->preferences.audio) - numFoundCodecs));

	return TRUE;
}
