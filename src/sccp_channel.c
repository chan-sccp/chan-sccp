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
 */
#include "config.h"
#include "common.h"
#include "sccp_channel.h"

SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \remarks
 * Purpose:     SCCP Channels
 * When to use: Only methods directly related to sccp channels should be stored in this source file.
 * Relations:   SCCP Channels connect Asterisk Channels to SCCP Lines
 */
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_conference.h"
#include "sccp_atomic.h"
#include "sccp_feature.h"
#include "sccp_indicate.h"
#include "sccp_line.h"
#include "sccp_linedevice.h"
#include "sccp_rtp.h"
#include "sccp_netsock.h"
#include "sccp_utils.h"
#include "sccp_labels.h"
#include "sccp_threadpool.h"
#include <asterisk/callerid.h>			// sccp_channel, sccp_callinfo
#include <asterisk/pbx.h>			// AST_EXTENSION_NOT_INUSE

static uint32_t callCount = 1;
int __sccp_channel_destroy(const void * data);

/* Lock Macro for Sessions */
#define sccp_channel_lock(x)    pbx_mutex_lock(&(x)->lock)
#define sccp_channel_unlock(x)  pbx_mutex_unlock(&(x)->lock)
#define sccp_channel_trylock(x) pbx_mutex_trylock(&(x)->lock)
//#define SCOPED_SESSION(x)       SCOPED_MUTEX(channellock, (ast_mutex_t *)&(x)->lock);
/* */

AST_MUTEX_DEFINE_STATIC(callCountLock);

/*!
 * \brief Private Channel Data Structure
 */
struct sccp_private_channel_data {
	devicePtr device;                                        //! \todo use lineDevicePtr instead;
	lineDevicePtr ld;
	sccp_callinfo_t * callInfo;
	SCCP_LIST_HEAD (, sccp_threadpool_job_t) cleanup_jobs;
	boolean_t microphone;					/*!< Flag to mute the microphone when calling a baby phone */
	boolean_t isAnswering;
	struct {
		skinny_tone_t active;
		skinny_toneDirection_t direction;
	} tone;
	boolean_t firewall_holepunch;
};

/*!
 * \brief Set Microphone State
 * \param channel SCCP Channel
 * \param enabled Enabled as Boolean
 */
static void setMicrophoneState(channelPtr c, boolean_t enabled)
{
	AUTO_RELEASE(sccp_device_t, d, sccp_channel_getDevice(c));
	if (!d) {
		return;
	}

	c->privateData->microphone = enabled;

	if (enabled) {
		c->isMicrophoneEnabled = sccp_always_true;
		if(sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_TRANSMISSION)) {
			sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
		}
	} else {
		c->isMicrophoneEnabled = sccp_always_false;
		if(sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_TRANSMISSION)) {
			sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
		}
	}
}

/*
 * \brief statemachine to Start/Stop device tone generation
 */
static void setTone(constChannelPtr c, skinny_tone_t tone, skinny_toneDirection_t direction)
{
	pbx_assert(c && c->privateData && c->privateData->ld);
	if(c->privateData->tone.active == tone && c->privateData->tone.direction == direction) {
		return;
	}
	sccp_linedevice_t * ld = c->privateData->ld;
	if(c->privateData->tone.active) {
		sccp_dev_stoptone(ld->device, ld->lineInstance, c->callid);
	}
	if(tone != SKINNY_TONE_SILENCE) {
		if(c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_DOWN) {
			sccp_dev_starttone(ld->device, tone, 0, 0, direction);
		} else {
			sccp_dev_starttone(ld->device, tone, ld->lineInstance, c->callid, direction);
		}
	}
	c->privateData->tone.active = tone;
	c->privateData->tone.direction = direction;
}

static void setEarlyRTP(channelPtr c, boolean_t state)
{
	pbx_assert(c != NULL);
	sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) %s\n", c->designator, __func__, state ? "ON" : "OFF");
	c->wantsEarlyRTP = state ? sccp_always_true : sccp_always_false;
}

static void makeProgress(channelPtr c)
{
	pbx_assert(c != NULL);
	if(c->wantsEarlyRTP() && c->progressSent == sccp_always_false) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s)\n", c->designator, __func__);
		if(!sccp_rtp_getState(&c->rtp.audio, SCCP_RTP_RECEPTION)) {
			sccp_channel_openReceiveChannel(c);
		}
#if CS_SCCP_VIDEO
		if(!sccp_rtp_getState(&c->rtp.video, SCCP_RTP_RECEPTION) && sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF) {
			sccp_channel_openMultiMediaReceiveChannel(c);
		}
#endif
		c->progressSent = sccp_always_true;
	}
}

/*!
 * \brief Allocate SCCP Channel on Device
 * \param l SCCP Line
 * \param device SCCP Device (optional)
 * \return a *retained* SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *  - callCountLock
 */
channelPtr sccp_channel_allocate(constLinePtr l, constDevicePtr device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t *channel = NULL;
	struct sccp_private_channel_data *private_data = NULL;
	sccp_line_t *refLine = sccp_line_retain(l);

	if (!refLine) {
		pbx_log(LOG_ERROR, "SCCP: Could not retain line to create a channel on it, giving up!\n");
		return NULL;
	}
	if (sccp_strlen_zero(refLine->name) || sccp_strlen_zero(refLine->context) || !pbx_context_find(refLine->context)) {
		pbx_log(LOG_ERROR, "SCCP: line with empty name, empty context or non-existent context provided, aborting creation of new channel\n");
		return NULL;
	}
	if (device && !device->session) {
		pbx_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", device->id);
		return NULL;
	}

	int32_t callid = 0;
	char designator[32];
	sccp_mutex_lock(&callCountLock);
	if (callCount < 0xFFFFFFFF) {						/* callcount limit should be reset at his upper limit :) */
		callid = callCount++;
	} else {
		pbx_log(LOG_NOTICE, "%s: CallId re-starting at 00000001\n", device->id);
		callCount = 1;
		callid = callCount;
	}
	snprintf(designator, 32, "SCCP/%s-%08X", refLine->name, callid);
	uint8_t callInstance = refLine->statistic.numberOfActiveChannels + refLine->statistic.numberOfHeldChannels + 1;
	sccp_mutex_unlock(&callCountLock);
	do {
		/* allocate new channel */
		channel = (sccp_channel_t *) sccp_refcount_object_alloc(sizeof(sccp_channel_t), SCCP_REF_CHANNEL, designator, __sccp_channel_destroy);
		if (!channel) {
			pbx_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n", l->id, l->name);
			break;
		}
		pbx_mutex_init(&channel->lock);
#if CS_REFCOUNT_DEBUG
		sccp_refcount_addRelationship(refLine, channel);
#endif
		/* allocate resources */
		private_data = (struct sccp_private_channel_data *)sccp_calloc(sizeof *private_data, 1);
		if (!private_data) {
			pbx_log(LOG_ERROR, "%s: No memory to allocate channel private data on line %s\n", l->id, l->name);
			break;
		}
		
		/* assign private_data default values */
		private_data->microphone = TRUE;
		private_data->callInfo = iCallInfo.Constructor(callInstance, designator);
		private_data->isAnswering = FALSE;
		SCCP_LIST_HEAD_INIT(&private_data->cleanup_jobs);
		if (!private_data->callInfo) {
			break;
		}
		
		/* assigning immutable values */
		*(struct sccp_private_channel_data **)&channel->privateData = private_data;
		*(uint32_t *)&channel->callid = callid;
		*(uint32_t *)&channel->passthrupartyid = callid ^ 0xFFFFFFFF;
		*(sccp_line_t **)&channel->line = refLine;
		*(char **)&channel->musicclass = pbx_strdup(
							!sccp_strlen_zero(refLine->musicclass) ? refLine->musicclass : 
							!sccp_strlen_zero(GLOB(musicclass)) ? GLOB(musicclass) : 
							"default"
						);
		*(char **)&channel->designator = pbx_strdup(designator);

		/* assign default values */
		channel->ringermode = GLOB(ringtype);
		channel->calltype = SKINNY_CALLTYPE_INBOUND;
		channel->answered_elsewhere = FALSE;
		channel->peerIsSCCP = 0;
		// channel->maxBitRate = 15000;
		channel->maxBitRate = 3200;
		iPbx.set_owner(channel, NULL);

		/* this is for dialing scheduler */
		channel->scheduler.digittimeout_id = -1;
		channel->scheduler.hangup_id = -1;
		channel->scheduler.cfwd_noanswer_id = -1;
		channel->enbloc.digittimeout = GLOB(digittimeout);
#ifndef SCCP_ATOMIC
		pbx_mutex_init(&channel->scheduler.lock);
#endif
		/* assign virtual functions */
		channel->getDevice = sccp_channel_getDevice;
		channel->getLineDevice = sccp_channel_getLineDevice;
		channel->setDevice = sccp_channel_setDevice;
		channel->isMicrophoneEnabled = sccp_always_true;
		channel->isHangingUp = FALSE;
		channel->isRunningPbxThread = FALSE;
		channel->wantsEarlyRTP = sccp_always_false;
		channel->setEarlyRTP = setEarlyRTP;
		channel->progressSent = sccp_always_false;
		channel->makeProgress = makeProgress;
		channel->setMicrophone = setMicrophoneState;
		channel->hangupRequest = sccp_astgenwrap_requestQueueHangup;
		//channel->privacy = (device && (device->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT)) ? TRUE : FALSE;
		if (device) {
			channel->dtmfmode = device->getDtmfMode(device);
			channel->setTone = setTone;
		} else {
			channel->dtmfmode = SCCP_DTMFMODE_RFC2833;
			channel->setTone = NULL;
		}

		if (l->preferences_set_on_line_level) {
			memcpy(&channel->preferences.audio, &l->preferences.audio, sizeof channel->preferences.audio);
			memcpy(&channel->preferences.video, &l->preferences.video, sizeof channel->preferences.video);
			if (device) {
				memcpy(&channel->capabilities.audio, &device->capabilities.audio, sizeof channel->capabilities.audio);
				memcpy(&channel->capabilities.video, &device->capabilities.video, sizeof channel->capabilities.video);
			}
		}
		channel->videomode = l->videomode;

		/* run setters */
		sccp_line_addChannel(l, channel);
		if (refLine->capabilities.audio[0] == SKINNY_CODEC_NONE) {
			sccp_line_updateCapabilitiesFromDevicesToLine(refLine);			// bit of a hack, UpdateCapabilties is done (long) after device registration
		}
		channel->setDevice(channel, device);

		/* return new channel */
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, channel->callid, l->name);
		return channel;
	} while (0);

	/* something went wrong, cleaning up */
	if (private_data) {
		if (private_data->callInfo) {
			iCallInfo.Destructor(&private_data->callInfo);
		}
		SCCP_LIST_HEAD_DESTROY(&(private_data->cleanup_jobs));
		sccp_free(private_data);
	}
	if (channel) {
		sccp_channel_release(&channel);							// explicit release
	}
	if (refLine) {
		sccp_line_release(&refLine);							// explicit release
	}
	return NULL;
}

/*!
 * \brief Retrieve Device from Channels->Private Channel Data
 * \param channel SCCP Channel
 * \return SCCP Device
 */
devicePtr sccp_channel_getDevice(constChannelPtr channel)
{
	pbx_assert(channel != NULL);
	if (channel->privateData && channel->privateData->device) {
		return sccp_device_retain(channel->privateData->device);
	} 
	return NULL;
}

/*!
 * \brief Retrieve LineDevice from Channels->Private Channel Data
 * \param channel SCCP Channel
 * \return SCCP LineDevice
 */
lineDevicePtr sccp_channel_getLineDevice(constChannelPtr channel)
{
	pbx_assert(channel != NULL);
	if(channel->privateData && channel->privateData->ld) {
		return sccp_linedevice_retain(channel->privateData->ld);
	}
	return NULL;
}
/*!
 * \brief Set Device in Channels->Private Channel Data
 * \param channel SCCP Channel
 * \param device SCCP Device
 */
void sccp_channel_setDevice(channelPtr channel, constDevicePtr device)
{
	if (!channel || !channel->privateData) {
		return;
	}

	/** for previous device,set active channel to null */
	if (!device) {
		sccp_linedevice_refreplace(&channel->privateData->ld, NULL);
		if (!channel->privateData->device) {
			/* channel->privateData->device was already set to NULL */
			goto EXIT;
		}
		sccp_device_setActiveChannel(channel->privateData->device, NULL);
	}
#if CS_REFCOUNT_DEBUG
	if (device || channel->privateData->device) {
		sccp_refcount_removeRelationship(channel->privateData->device ? channel->privateData->device : device, channel);
	}
#endif
	sccp_device_refreplace(&channel->privateData->device, (sccp_device_t *) device);

	if (device) {
		sccp_device_setActiveChannel(device, channel);
#if CS_REFCOUNT_DEBUG
		sccp_refcount_addRelationship(device, channel);
#endif
	}

	if (channel->privateData && channel->privateData->device) {
		{
			AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_find(channel->privateData->device, channel->line));
			sccp_linedevice_refreplace(&channel->privateData->ld, ld);
		}
		/*! \todo: Check/Fix codec selection on hold/resume */
		if(channel->preferences.audio[0] == SKINNY_CODEC_NONE || channel->capabilities.audio[0] == SKINNY_CODEC_NONE) {
			sccp_line_copyCodecSetsFromLineToChannel(channel->line, channel->privateData->device, channel);
		}
#if CS_SCCP_VIDEO
		if (!channel->line->preferences_set_on_line_level && channel->preferences.video[0] == SKINNY_CODEC_NONE) {
			memcpy(&channel->preferences.video, &channel->privateData->device->preferences.video, sizeof(channel->preferences.video));
		}
		if (channel->capabilities.video[0] == SKINNY_CODEC_NONE) {
			memcpy(&channel->capabilities.video, &channel->privateData->device->capabilities.video, sizeof(channel->capabilities.video));
		}
#endif
		sccp_copy_string(channel->currentDeviceId, channel->privateData->device->id, sizeof(char[StationMaxDeviceNameSize]));
		channel->dtmfmode = channel->privateData->device->getDtmfMode(channel->privateData->device);
		channel->setEarlyRTP(channel, channel->privateData->device->earlyrtp);
		channel->setTone = setTone;
		return;
	}
EXIT:
	/*! \todo: Check/Fix codec selection on hold/resume */
	if (channel->preferences.audio[0] == SKINNY_CODEC_NONE || channel->capabilities.audio[0] == SKINNY_CODEC_NONE) {
		sccp_line_copyCodecSetsFromLineToChannel(channel->line, NULL, channel);
	}

	sccp_linedevice_refreplace(&channel->privateData->ld, NULL);
	/* \todo we should use */
	// sccp_line_copyMinimumCodecSetFromLineToChannel(l, c); 
	sccp_copy_string(channel->currentDeviceId, "SCCP", sizeof(char[StationMaxDeviceNameSize]));
	channel->dtmfmode = SCCP_DTMFMODE_RFC2833;
	channel->setEarlyRTP(channel, FALSE);
	channel->setTone = NULL;
}

static void sccp_channel_recalculateAudioCodecFormat(channelPtr channel)
{
	char s1[512];

	char s2[512];

	char s3[512];

	char s4[512];
	skinny_codec_t joint = channel->rtp.audio.reception.format;
	skinny_capabilities_t *preferences = &(channel->preferences);
	sccp_rtp_t * audio = (sccp_rtp_t *)&(channel->rtp.audio);

	if(sccp_rtp_areBothInvalid(audio)) {
		if(channel->privateData->device && !channel->line->preferences_set_on_line_level) {
			preferences = &(channel->privateData->device->preferences);
			sccp_codec_reduceSet(preferences->audio, channel->privateData->device->capabilities.audio);
		}
		sccp_codec_reduceSet(preferences->audio, channel->capabilities.audio);
		joint = sccp_codec_findBestJoint(channel, preferences->audio, channel->remoteCapabilities.audio, TRUE);
		if (SKINNY_CODEC_NONE == joint) {
			joint = preferences->audio[0] ? preferences->audio[0] : SKINNY_CODEC_WIDEBAND_256K;
		}
		if (channel->rtp.audio.instance) {                      // Fix nativeAudioFormats
			skinny_codec_t codecs[SKINNY_MAX_CAPABILITIES] = { joint, SKINNY_CODEC_NONE};
			iPbx.set_nativeAudioFormats(channel, codecs);
		}
	}
	if (SKINNY_CODEC_NONE != joint) {
		if(!sccp_rtp_getState(audio, SCCP_RTP_RECEPTION)) {
			channel->rtp.audio.reception.format = joint;
			iPbx.rtp_setWriteFormat(channel, joint);
			channel->rtp.audio.transmission.format = joint;
			iPbx.rtp_setReadFormat(channel, joint);
		}
	}
	sccp_log((DEBUGCAT_CODEC + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3
		"%s - %s: (recalculateAudioCodecformat) \n\t"
		"channel capabilities: %s\n\t"
		"%s preferences %s\n\t"
		"%s preferences: %s\n\t"
		"remote caps: %s\n\t"
		"Format:%s\n",
		channel->currentDeviceId, channel->designator,
		sccp_codec_multiple2str(s1, sizeof(s1) - 1, channel->capabilities.audio, ARRAY_LEN(channel->capabilities.audio)),
		(SCCP_LIST_GETSIZE(&channel->line->devices) > 1) ? "shared" : "channel",
		sccp_codec_multiple2str(s2, sizeof(s2) - 1, channel->preferences.audio, ARRAY_LEN(channel->preferences.audio)),
		channel->line->preferences_set_on_line_level ? "line" : "device",
		channel->line->preferences_set_on_line_level ? sccp_codec_multiple2str(s3, sizeof(s3) - 1, channel->line->preferences.audio, ARRAY_LEN(channel->line->preferences.audio)) : ((channel->privateData->device) ? sccp_codec_multiple2str(s3, sizeof(s3) - 1, channel->privateData->device->preferences.audio, ARRAY_LEN(channel->privateData->device->preferences.audio)) : ""),
		sccp_codec_multiple2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.audio, ARRAY_LEN(channel->remoteCapabilities.audio)),
		codec2name(joint)
	);
}

static boolean_t sccp_channel_recalculateVideoCodecFormat(channelPtr channel)
{
	char s1[512];

	char s2[512];

	char s3[512];

	char s4[512];
	skinny_codec_t joint = channel->rtp.video.reception.format;
	skinny_capabilities_t *preferences = &(channel->preferences);
	sccp_rtp_t * video = (sccp_rtp_t *)&(channel->rtp.video);

	if(sccp_rtp_areBothInvalid(video)) {
		if(channel->privateData->device && !channel->line->preferences_set_on_line_level) {
			preferences = &(channel->privateData->device->preferences);
			sccp_codec_reduceSet(preferences->video, channel->privateData->device->capabilities.video);
		}
		sccp_codec_reduceSet(preferences->video, channel->capabilities.video);
		joint = sccp_codec_findBestJoint(channel, preferences->video, channel->remoteCapabilities.video, FALSE);
		if (joint == SKINNY_CODEC_NONE) {
			sccp_channel_setVideoMode(channel, "off");
			return FALSE;
		}
		//if (channel->rtp.video.instance) {
		skinny_codec_t codecs[SKINNY_MAX_CAPABILITIES] = { joint, SKINNY_CODEC_NONE};
		iPbx.set_nativeVideoFormats(channel, codecs);
		//}
		channel->rtp.video.reception.format = joint;
		channel->rtp.video.transmission.format = joint;
		iPbx.rtp_setWriteFormat(channel, joint);
		iPbx.rtp_setReadFormat(channel, joint);
	}
	sccp_log((DEBUGCAT_CODEC + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3
		"%s - %s: (recalculateVideoCodecformat) \n\t"
		"channel capabilities: %s\n\t"
		"%s preferences %s\n\t"
		"%s preferences: %s\n\t"
		"remote caps: %s\n\t"
		"Format:%s\n",
		channel->currentDeviceId, channel->designator,
		sccp_codec_multiple2str(s1, sizeof(s1) - 1, channel->capabilities.video, ARRAY_LEN(channel->capabilities.video)),
		(SCCP_LIST_GETSIZE(&channel->line->devices) > 1) ? "shared" : "channel",
		sccp_codec_multiple2str(s2, sizeof(s2) - 1, channel->preferences.video, ARRAY_LEN(channel->preferences.video)),
		channel->line->preferences_set_on_line_level ? "line" : "device",
		channel->line->preferences_set_on_line_level ? sccp_codec_multiple2str(s3, sizeof(s3) - 1, channel->line->preferences.video, ARRAY_LEN(channel->line->preferences.video)) : ((channel->privateData->device) ? sccp_codec_multiple2str(s3, sizeof(s3) - 1, channel->privateData->device->preferences.video, ARRAY_LEN(channel->privateData->device->preferences.video)) : ""),
		sccp_codec_multiple2str(s4, sizeof(s4) - 1, channel->remoteCapabilities.video, ARRAY_LEN(channel->remoteCapabilities.video)),
		codec2name(joint)
	);
	return TRUE;
}

/*!
 * \brief Update Channel Capability
 * \param channel a *retained* SCCP Channel
 */
void sccp_channel_updateChannelCapability(channelPtr channel)
{
	if (iPbx.retrieve_remote_capabilities && channel->remoteCapabilities.audio[0] == SKINNY_CODEC_NONE) {
		iPbx.retrieve_remote_capabilities(channel);
	}
	sccp_channel_recalculateAudioCodecFormat(channel);
#if CS_SCCP_VIDEO
	sccp_channel_recalculateVideoCodecFormat(channel);
#endif
}

/*!
 * \brief Get const pointer to channels private callinfo
 */
sccp_callinfo_t * const __PURE__ sccp_channel_getCallInfo(constChannelPtr channel)
{
	return channel->privateData->callInfo; /* discard const because callinfo has a private implementation anyway */
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
void sccp_channel_send_callinfo(constDevicePtr device, constChannelPtr channel)
{
	uint8_t lineInstance = 0;

	if (device && channel && channel->callid) {
		lineInstance = sccp_device_find_index_for_line(device, channel->line->name);
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: send callInfo on %s with lineInstance: %d\n", device->id, channel->designator, lineInstance);
		iCallInfo.Send(channel->privateData->callInfo, channel->callid, channel->calltype, lineInstance, device, FALSE);
	}

}

/*!
 * \brief Send Dialed Number to SCCP Channel device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_send_callinfo2(constChannelPtr channel)
{
	pbx_assert(channel != NULL);

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(channel->line));

	if (d) {
		sccp_channel_send_callinfo(d, channel);
	} else if (line) {
		sccp_linedevice_t * ld = NULL;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, ld, list) {
			AUTO_RELEASE(sccp_device_t, device, sccp_device_retain(ld->device));

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
void sccp_channel_setChannelstate(channelPtr channel, sccp_channelstate_t state)
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
void sccp_channel_display_callInfo(constChannelPtr channel)
{
	if ((GLOB(debug) & (DEBUGCAT_CHANNEL)) != 0) {
		iCallInfo.Print2log(channel->privateData->callInfo, channel->designator);
	}
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
void sccp_channel_set_callingparty(constChannelPtr channel, const char *name, const char *number)
{
	if (!channel) {
		return;
	}
	iCallInfo.SetCallingParty(channel->privateData->callInfo, name, number, NULL);
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_callingparty) Set callingParty Name '%s', Number '%s' on channel %s\n", channel->currentDeviceId, name, number, channel->designator);
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
boolean_t sccp_channel_set_originalCallingparty(constChannelPtr channel, char * name, char * number)
{
	boolean_t changed = FALSE;

	if (!channel) {
		return FALSE;
	}

	changed = iCallInfo.SetOrigCallingParty(channel->privateData->callInfo, name, number);
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_originalCallingparty) Set originalCallingparty Name '%s', Number '%s' on channel %s\n", channel->currentDeviceId, name, number, channel->designator);
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
void sccp_channel_set_calledparty(constChannelPtr channel, const char * name, const char * number)
{
	if (!channel || sccp_strequals(number, "s") /* skip update for immediate earlyrtp + s-extension */ ) {
		return;
	}
	iCallInfo.SetCalledParty(channel->privateData->callInfo, name, number, NULL);
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
boolean_t sccp_channel_set_originalCalledparty(constChannelPtr channel, char * name, char * number)
{
	boolean_t changed = FALSE;

	if (!channel) {
		return FALSE;
	}
	changed = iCallInfo.SetOrigCalledParty(channel->privateData->callInfo, name, number, NULL, 4);
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_set_originalCalledparty) Set originalCalledparty Name '%s', Number '%s' on channel %s\n", channel->currentDeviceId, name, number, channel->designator);
	return changed;

}

/*!
 * \brief Request Call Statistics for SCCP Channel
 * \param channel SCCP Channel
 */
void sccp_channel_StatisticsRequest(constChannelPtr channel)
{
	pbx_assert(channel != NULL);
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));

	if (!d) {
		return;
	}
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Device is Requesting Connections Statistics And Clear\n", d->id);
	d->protocol->sendConnectionStatisticsReq(d, channel, SKINNY_STATSPROCESSING_CLEAR);
}

/*
 * \brief a simple way to punch a whole in the firewall by sending a short burst of packets during progress
 * transmission will be stopped again as soon as the first packet has been received from the device in astwrap_rtp_read
 */
static void sccp_channel_startHolePunch(constChannelPtr c)
{
	pbx_assert(c != NULL && c->privateData && !c->privateData->firewall_holepunch);
	sccp_rtp_t * audio = (sccp_rtp_t *)&(c->rtp.audio);
	/*! \todo
		// punching is only necessary if the rtp-instance ip-address+mask differs from the rtp->phone+mask
		// ie: they are in different networks and there is a potential firewall in between
		sccp_rtp_t * audio = (sccp_rtp_t *)&(c->rtp.audio);
		apply_netmask()
		if (sccp_netsock_cmp_addr(&audio->phone, &audio->phone_remote)) {
	*/
	if(!sccp_rtp_getState(audio, SCCP_RTP_TRANSMISSION) && pbx_channel_state(c->owner) != AST_STATE_UP && c->wantsEarlyRTP()) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) start Punching a hole through the firewall (if necessary)\n", c->designator, __func__);
		c->privateData->firewall_holepunch = TRUE;
		sccp_channel_startMediaTransmission(c);
	}
}

boolean_t sccp_channel_finishHolePunch(constChannelPtr c)
{
	pbx_assert(c != NULL && c->privateData);
	if(pbx_channel_state(c->owner) == AST_STATE_UP) {
		c->privateData->firewall_holepunch = FALSE;
		return FALSE;
	}
	sccp_rtp_t * audio = (sccp_rtp_t *)&(c->rtp.audio);
	if(c->privateData->firewall_holepunch && ((sccp_rtp_getState(audio, SCCP_RTP_TRANSMISSION) & SCCP_RTP_STATUS_ACTIVE) == SCCP_RTP_STATUS_ACTIVE)) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) stop punching a hole through the firewall\n", c->designator, __func__);
		sccp_channel_stopMediaTransmission(c, TRUE);
		c->privateData->firewall_holepunch = FALSE;
	}
	return c->privateData->firewall_holepunch;
}

/*!
 * \brief Tell Device to Open a RTP Receive Channel
 *
 * At this point we choose the codec for receive channel and tell them to device.
 * We will get a OpenReceiveChannelAck message that includes all information.
 *
 */
void sccp_channel_openReceiveChannel(constChannelPtr channel)
{
	pbx_assert(channel != NULL);
	pbx_assert(channel->line != NULL); /* should not be possible, but received a backtrace / report */
	sccp_rtp_t * audio = (sccp_rtp_t *)&(channel->rtp.audio);

	if(channel->isHangingUp || !channel->owner || pbx_check_hangup_locked(channel->owner) || pbx_channel_hangupcause(channel->owner) == AST_CAUSE_ANSWERED_ELSEWHERE) {
		pbx_log(LOG_ERROR, "%s: (%s) Channel already hanging up\n", channel->designator, __func__);
		return;
	}

	if(sccp_rtp_getState(audio, SCCP_RTP_RECEPTION)) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) Already pending\n", channel->designator, __func__);
		return;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_ERROR, "%s: (%s) Could not retrieve device from channel\n", channel->designator, __func__);
		return;
	}

	/* Mute mic feature: If previously set, mute the microphone prior receiving media is already open. */
	/* This must be done in this exact order to work on popular phones like the 7975. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	if (!channel->rtp.audio.instance && !sccp_rtp_createServer(d, (channelPtr)channel, SCCP_RTP_AUDIO)) {			// discard const
		pbx_log(LOG_WARNING, "%s: Error opening RTP for channel %s\n", d->id, channel->designator);
		channel->setTone(channel, SKINNY_TONE_REORDERTONE, SKINNY_TONEDIRECTION_USER);
		return;
	}

	if (channel->owner && channel->rtp.audio.reception.format == SKINNY_CODEC_NONE) {
		sccp_channel_updateChannelCapability((channelPtr)channel);							// discard const
	}

	sccp_log((DEBUGCAT_RTP + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s, OpenReceiveChannel with format %s, payload %d, echocancel: %s, passthrupartyid: %u, pbx_channel_name: %s\n",
			channel->designator, codec2str(audio->reception.format), audio->reception.format,
			channel->line ? (channel->line->echocancel ? "YES" : "NO") : "(nil)>",
			channel->passthrupartyid, iPbx.getChannelName(channel)
		);

	sccp_rtp_setState(audio, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_PROGRESS);
	if (d->nat >= SCCP_NAT_ON) {												// device is natted
		sccp_rtp_updateNatRemotePhone(channel, audio);
	}

	d->protocol->sendOpenReceiveChannel(d, channel);                                        // extra channel retain, released when receive channel is closed
}

int sccp_channel_receiveChannelOpen(sccp_device_t *d, sccp_channel_t *c)
{
	pbx_assert(d != NULL && c != NULL);
	sccp_rtp_t * audio = &(c->rtp.audio);

	// check channel state
	if (!audio->instance) {
		pbx_log(LOG_ERROR, "%s: Channel has no rtp instance!\n", d->id);
		sccp_channel_endcall(c);											// FS - 350
		return SCCP_RTP_STATUS_INACTIVE;
	}

	c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
	if(c->isHangingUp || !c->owner || pbx_check_hangup_locked(c->owner) || pbx_channel_hangupcause(c->owner) == AST_CAUSE_ANSWERED_ELSEWHERE || SCCP_CHANNELSTATE_Idling(c->state)
	   || SCCP_CHANNELSTATE_IsTerminating(c->state)) {
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			return SCCP_RTP_STATUS_ACTIVE;
		}
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP))(VERBOSE_PREFIX_3 "%s: (receiveChannelOpen) Channel is already terminating. Giving up... (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
		return SCCP_RTP_STATUS_ACTIVE;
	}

	sccp_log((DEBUGCAT_RTP))(VERBOSE_PREFIX_3 "%s: Opened Receive Channel (State: %s[%d])\n", d->id, sccp_channelstate2str(c->state), c->state);
	//sccp_rtp_set_phone(c, &c->rtp.audio, &sas);
	sccp_channel_send_callinfo(d, c);
	sccp_rtp_appendState(audio, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_ACTIVE);

	if(c->owner && !pbx_check_hangup_locked(c->owner)) {
		sccp_rtp_runCallback(audio, SCCP_RTP_RECEPTION, c);
		if(c->calltype != SKINNY_CALLTYPE_INBOUND) {
			if(d->nat >= SCCP_NAT_ON) {
				sccp_channel_startHolePunch(c);
			}
			iPbx.queue_control(c->owner, (enum ast_control_frame_type)-1);						// 'PROD' the remote side to let them know
																// we can receive inband signalling from this
																// moment onwards -> inband signalling required
		}
	}
	return sccp_rtp_getState(audio, SCCP_RTP_RECEPTION);
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 * \note sccp_channel_stopMediaTransmission is explicit call within this function!
 * 
 */
void sccp_channel_closeReceiveChannel(constChannelPtr channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg = NULL;

	pbx_assert(channel != NULL);
	sccp_rtp_t *audio = (sccp_rtp_t *) &(channel->rtp.audio);
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));

	if (!d) {
		pbx_log(LOG_ERROR, "%s: (closeReceiveChannel) Could not retrieve device from channel\n", channel->designator);
		return;
	}
	// stop transmitting before closing receivechannel (\note maybe we should not be doing this here)
	//sccp_channel_stopMediaTransmission(channel, KeepPortOpen);
	//sccp_rtp_stop(channel);

	if(sccp_rtp_getState(audio, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Close receivechannel on device %s (KeepPortOpen: %s)\n", channel->designator, d->id, KeepPortOpen ? "YES" : "NO");
		REQ(msg, CloseReceiveChannel);
		msg->data.CloseReceiveChannel.lel_conferenceId = htolel(channel->callid);
		msg->data.CloseReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.CloseReceiveChannel.lel_callReference = htolel(channel->callid);
		msg->data.CloseReceiveChannel.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		sccp_rtp_setState(audio, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_INACTIVE);
	}
}

#if UNUSEDCODE // 2015-11-01
void sccp_channel_updateReceiveChannel(constChannelPtr channel)
{
	/* \todo possible to skip the closing of the receive channel (needs testing) */
	/* \todo if this works without closing, this would make changing codecs on the fly possible */
	if(sccp_rtp_getState(&channel->rtp.audio, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateReceiveChannel) Close Receive Channel on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_closeReceiveChannel(channel, TRUE);
	}
	if(!sccp_rtp_getState(&channel->rtp.audio, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateReceiveChannel) Open Receive Channel on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_openReceiveChannel(channel);
	}
}
#endif

/*!
 * \brief Tell a Device to Start Media Transmission.
 *
 * We choose codec according to sccp_channel->format.
 *
 * \param channel SCCP Channel
 * \note rtp should be started before, otherwise we do not start transmission
 */
void sccp_channel_startMediaTransmission(constChannelPtr channel)
{
	pbx_assert(channel != NULL);
	pbx_assert(channel->line != NULL); /* should not be possible, but received a backtrace / report */
	sccp_rtp_t *audio = (sccp_rtp_t *) &(channel->rtp.audio);

	if(channel->isHangingUp || !channel->owner || pbx_check_hangup_locked(channel->owner) || pbx_channel_hangupcause(channel->owner) == AST_CAUSE_ANSWERED_ELSEWHERE) {
		pbx_log(LOG_ERROR, "%s: (%s) Channel already hanging up\n", channel->designator, __func__);
		return;
	}

	if(sccp_rtp_getState(audio, SCCP_RTP_TRANSMISSION)) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) Already pending\n", channel->designator, __func__);
		return;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_ERROR, "%s: (%s) Could not retrieve device from channel\n", channel->designator, __func__);
		sccp_channel_closeReceiveChannel(channel, FALSE);
		return;
	}

	if(!audio->instance || !sccp_rtp_getState(audio, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s\n", channel->currentDeviceId, channel->designator);
		sccp_channel_closeReceiveChannel(channel, FALSE);
		return;
	}

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Starting Phone RTP/UDP Transmission (State: %s[%d])\n", d->id, sccp_channelstate2str(channel->state), channel->state);
	/* Mute mic feature: If previously set, mute the microphone after receiving of media is already open, but before starting to send to rtp. */
	/* This must be done in this exact order to work also on newer phones like the 8945. It must also be done in other places for other phones. */
	if (!channel->isMicrophoneEnabled()) {
		sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
	}

	if (d->nat >= SCCP_NAT_ON) {
		sccp_rtp_updateNatRemotePhone(channel, audio);
	}

	if (audio->transmission.format == SKINNY_CODEC_NONE) {
		if (audio->reception.format == SKINNY_CODEC_NONE) {
			sccp_channel_updateChannelCapability((sccp_channel_t *)channel);
		} else {
			audio->transmission.format = audio->reception.format;
		}
	}
	sccp_rtp_appendState(audio, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_PROGRESS);
	d->protocol->sendStartMediaTransmission(d, channel);                                        // extra channel retain, released when media transmission is closed

	char buf1[NI_MAXHOST + NI_MAXSERV];
	char buf2[NI_MAXHOST + NI_MAXSERV];
	sccp_copy_string(buf1, sccp_netsock_stringify(&audio->phone), sizeof(buf1));
	sccp_copy_string(buf2, sccp_netsock_stringify(&audio->phone_remote), sizeof(buf2));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell Phone to send RTP/UDP media from %s to %s (NAT: %s)\n", d->id, buf1, buf2, sccp_nat2str(d->nat));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Using codec:%s(%d), TOS:%d, Silence Suppression:%s for call with PassThruId:%u and CallID:%u\n", d->id, codec2str(audio->transmission.format), audio->transmission.format, d->audio_tos, channel->line->silencesuppression ? "ON" : "OFF", channel->passthrupartyid, channel->callid);
}

int sccp_channel_mediaTransmissionStarted(devicePtr d, channelPtr c)
{
	pbx_assert(d != NULL && c != NULL);
	sccp_rtp_t * audio = &(c->rtp.audio);
	// check channel state
	if (!audio->instance) {
		pbx_log(LOG_ERROR, "%s: Channel has no rtp instance!\n", d->id);
		sccp_channel_endcall(c);											// FS - 350
		return SCCP_RTP_STATUS_INACTIVE;
	}

	if(c->isHangingUp || !c->owner || pbx_check_hangup_locked(c->owner) || pbx_channel_hangupcause(c->owner) == AST_CAUSE_ANSWERED_ELSEWHERE || SCCP_CHANNELSTATE_Idling(c->state)
	   || SCCP_CHANNELSTATE_IsTerminating(c->state)) {
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop Tone %s\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
			c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
			return SCCP_RTP_STATUS_ACTIVE;
		}
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP))(VERBOSE_PREFIX_3 "%s: (mediaTransmissionStarted) Channel is already terminating. Giving up... (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
		return SCCP_RTP_STATUS_ACTIVE;
	}

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Media Transmission Started (State: %s[%d])\n", d->id, sccp_channelstate2str(c->state), c->state);
	sccp_rtp_appendState(audio, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_ACTIVE);
	return SCCP_RTP_STATUS_ACTIVE;
}

/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 * 
 */
void sccp_channel_stopMediaTransmission(constChannelPtr channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg = NULL;
	pbx_assert(channel != NULL);
	sccp_rtp_t *audio = (sccp_rtp_t *) &(channel->rtp.audio);

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_ERROR, "%s: (stopMediaTransmission) Could not retrieve device from channel\n", channel->designator);
		return;
	}
	// stopping phone rtp
	if(sccp_rtp_getState(audio, SCCP_RTP_TRANSMISSION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop mediatransmission on device %s (KeepPortOpen: %s)\n", channel->designator, d->id, KeepPortOpen ? "YES" : "NO");
		REQ(msg, StopMediaTransmission);
		msg->data.StopMediaTransmission.lel_conferenceId = htolel(channel->callid);
		msg->data.StopMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.StopMediaTransmission.lel_callReference = htolel(channel->callid);
		msg->data.StopMediaTransmission.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		sccp_rtp_setState(audio, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_INACTIVE);
#ifdef CS_EXPERIMENTAL
		if (!KeepPortOpen) {
			d->protocol->sendPortClose(d, channel, SKINNY_MEDIA_TYPE_AUDIO);
		}
#endif
	}
}

void sccp_channel_updateMediaTransmission(constChannelPtr channel)
{
	/* \note apparently startmediatransmission allows us to change the ip-information midflight without stopping mediatransmission beforehand */
	/* \note this would indicate that it should also be possible to change codecs midflight ! */
	/* \test should be able to do without this block to stopmediatransmission (Sometimes results in "OpenIngressChan: Potential buffer leak" (phone log) */
	if(sccp_rtp_getState(&channel->rtp.audio, SCCP_RTP_TRANSMISSION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMediaTransmission) Stop media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_stopMediaTransmission(channel, TRUE);
	}
	if(!sccp_rtp_getState(&channel->rtp.audio, SCCP_RTP_TRANSMISSION)) {
		/*! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMediaTransmission) Start/Update media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_startMediaTransmission(channel);
	}
}

/*!
 * \brief Open Multi Media Channel (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_openMultiMediaReceiveChannel(constChannelPtr channel)
{
	int payloadType = 0;
	uint8_t lineInstance = 0;
	// int bitRate = 1500;
	int bitRate = channel->maxBitRate;

	pbx_assert(channel != NULL);
	pbx_assert(channel->line != NULL); /* should not be possible, but received a backtrace / report */
	sccp_rtp_t *video = (sccp_rtp_t *) &(channel->rtp.video);

	if(channel->isHangingUp || !channel->owner || pbx_check_hangup_locked(channel->owner) || pbx_channel_hangupcause(channel->owner) == AST_CAUSE_ANSWERED_ELSEWHERE) {
		pbx_log(LOG_ERROR, "%s: (%s) Channel already hanging up\n", channel->designator, __func__);
		return;
	}

	if(sccp_rtp_getState(video, SCCP_RTP_RECEPTION)) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) Already pending\n", channel->designator, __func__);
		return;
	}

	AUTO_RELEASE(sccp_device_t, d, sccp_channel_getDevice(channel));
	if(!d) {
		pbx_log(LOG_ERROR, "%s: (%s) Could not retrieve device from channel\n", channel->designator, __func__);
		return;
	}

	if (sccp_channel_getVideoMode(channel) == SCCP_VIDEO_MODE_OFF || !sccp_device_isVideoSupported(d)) {
		pbx_log(LOG_WARNING, "%s: (openMultiMediaReceiveChannel) No video supported on device:%s or turning off. returning.\n", channel->designator, d->id);
		return;
	}

	if (!video->instance && !sccp_rtp_createServer(d, (channelPtr)channel, SCCP_RTP_VIDEO)) {				// discard const
		pbx_log(LOG_WARNING, "%s: (openMultiMediaReceiveChannel) Could not start vrtp on device:%s. returning\n", channel->designator, d->id);
		sccp_channel_setVideoMode((channelPtr)channel, "off");								// discard const
		return;
	}

	if (channel->owner && SKINNY_CODEC_NONE == video->reception.format && !sccp_channel_recalculateVideoCodecFormat((channelPtr)channel)) {
		return;
	}
	
	//if (d->nat >= SCCP_NAT_ON) {
	//	sccp_rtp_updateNatRemotePhone(channel, video);
	//}
	sccp_rtp_setState(video, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_PROGRESS);

	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, video->reception.format);
	lineInstance = sccp_device_find_index_for_line(d, channel->line->name);

	d->protocol->sendOpenMultiMediaChannel(d, channel, video->reception.format, payloadType, lineInstance, bitRate);                                        // extra receive channel retension

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Open receive multimedia channel with format %s[%d], payload %d\n", d->id,
		codec2str(video->reception.format), video->reception.format, payloadType);
}

int sccp_channel_receiveMultiMediaChannelOpen(constDevicePtr d, channelPtr c)
{
	pbx_assert(d != NULL && c != NULL);
	sccp_rtp_t * video = &(c->rtp.video);
	// check channel state
	if (!video->instance) {
		pbx_log(LOG_ERROR, "%s: Channel has no rtp instance!\n", d->id);
		sccp_channel_endcall(c);											// FS - 350
		return SCCP_RTP_STATUS_INACTIVE;
	}

	if(c->isHangingUp || !c->owner || pbx_check_hangup_locked(c->owner) || pbx_channel_hangupcause(c->owner) == AST_CAUSE_ANSWERED_ELSEWHERE || SCCP_CHANNELSTATE_Idling(c->state)
	   || SCCP_CHANNELSTATE_IsTerminating(c->state)) {
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop Tone %s\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
			c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
			return SCCP_RTP_STATUS_ACTIVE;
		}
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP))(VERBOSE_PREFIX_3 "%s: (receiveMultiMediaChannelOpen) Channel is already terminating. Giving up... (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
		return SCCP_RTP_STATUS_ACTIVE;
	}

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Opened MultiMedia Receive Channel (State: %s[%d])\n", d->id, sccp_channelstate2str(c->state), c->state);
	sccp_rtp_appendState(video, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_ACTIVE);

	if (c->owner && (c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE)) {
		if(sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION) & SCCP_RTP_STATUS_ACTIVE) {
			d->protocol->sendMultiMediaCommand(d, c, SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);
			//msg_out = sccp_build_packet(FlowControlNotifyMessage, sizeof(msg_out->data.FlowControlNotifyMessage));
			//msg_out->data.FlowControlNotifyMessage.lel_conferenceID         = htolel(c->callid);
			//msg_out->data.FlowControlNotifyMessage.lel_passThruPartyId      = htolel(c->passthrupartyid);
			//msg_out->data.FlowControlNotifyMessage.lel_callReference        = htolel(c->callid);
			//msg_out->data.FlowControlNotifyMessage.lel_maxBitRate           = htolel(500000);
		} else if(sccp_channel_getVideoMode(c) == SCCP_VIDEO_MODE_AUTO) {
			sccp_channel_startMultiMediaTransmission(c);
		}
		iPbx.queue_control(c->owner, AST_CONTROL_VIDUPDATE);
	}
	return SCCP_RTP_STATUS_ACTIVE;
}

/*!
 * \brief Open Multi Media Channel (Video) on Channel
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 */
void sccp_channel_closeMultiMediaReceiveChannel(constChannelPtr channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg = NULL;
	pbx_assert(channel != NULL);
	sccp_rtp_t *video = (sccp_rtp_t *) &(channel->rtp.video);

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_ERROR, "%s: (closeMultiMediaReceiveChannel) Could not retrieve device from channel\n", channel->designator);
		return;
	}
	// stop transmitting before closing receivechannel (\note maybe we should not be doing this here)
	sccp_channel_stopMediaTransmission(channel, KeepPortOpen);

	if(sccp_rtp_getState(video, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Close multimedia receive channel on device %s (KeepPortOpen: %s)\n", channel->designator, d->id, KeepPortOpen ? "YES" : "NO");
		REQ(msg, CloseMultiMediaReceiveChannel);
		msg->data.CloseMultiMediaReceiveChannel.lel_conferenceId = htolel(channel->callid);
		msg->data.CloseMultiMediaReceiveChannel.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.CloseMultiMediaReceiveChannel.lel_callReference = htolel(channel->callid);
		msg->data.CloseMultiMediaReceiveChannel.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		sccp_rtp_setState(video, SCCP_RTP_RECEPTION, SCCP_RTP_STATUS_INACTIVE);
#ifdef CS_EXPERIMENTAL
		if (!KeepPortOpen) {
			d->protocol->sendPortClose(d, channel, SKINNY_MEDIA_TYPE_MAIN_VIDEO);
		}
#endif
	}
	((channelPtr)channel)->videomode = channel->line->videomode;								// discard const
}

#if UNUSEDCODE // 2015-11-01
void sccp_channel_updateMultiMediaReceiveChannel(constChannelPtr channel)
{
	if(sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateMultiMediaReceiveChannel) Stop multimedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_closeMultiMediaReceiveChannel(channel, TRUE);
	}
	if(!sccp_rtp_getState(&channel.rtp.video, SCCP_RTP_RECEPTION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_updateMultiMediaReceiveChannel) Start media transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_openMultiMediaReceiveChannel(channel);
	}
}
#endif

/*!
 * \brief Start Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 */
void sccp_channel_startMultiMediaTransmission(constChannelPtr channel)
{
	int payloadType = 0;
	int bitRate = channel->maxBitRate;

	pbx_assert(channel != NULL);
	pbx_assert(channel->line != NULL); /* should not be possible, but received a backtrace / report */
	sccp_rtp_t *video = (sccp_rtp_t *) &(channel->rtp.video);

	if(channel->isHangingUp || !channel->owner || pbx_check_hangup_locked(channel->owner) || pbx_channel_hangupcause(channel->owner) == AST_CAUSE_ANSWERED_ELSEWHERE) {
		pbx_log(LOG_ERROR, "%s: (%s) Channel already hanging up\n", channel->designator, __func__);
		return;
	}

	if(sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION)) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: (%s) Already pending\n", channel->designator, __func__);
		return;
	}

	AUTO_RELEASE(sccp_device_t, d, sccp_channel_getDevice(channel));
	if(!d) {
		pbx_log(LOG_ERROR, "%s: (%s) Could not retrieve device from channel\n", channel->designator, __func__);
		sccp_channel_closeMultiMediaReceiveChannel(channel, FALSE);
		return;
	}

	if (sccp_channel_getVideoMode(channel) == SCCP_VIDEO_MODE_OFF || !sccp_device_isVideoSupported(d)) {
		pbx_log(LOG_WARNING, "%s: (openMultiMediaTransmission) No video supported on device:%s or turning off. returning.\n", channel->designator, d->id);
		return;
	}
	
	if (!video->instance) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: can't start vrtp media transmission, maybe channel is down %s\n", channel->currentDeviceId, channel->designator);
		sccp_channel_setVideoMode((channelPtr)channel, "off");								// discard const
		return;
	}
	//if (d->nat >= SCCP_NAT_ON) {												/* device is natted */
	//	sccp_rtp_updateNatRemotePhone(channel, video);
	//}

	sccp_rtp_setState(video, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_PROGRESS);

	/* lookup payloadType */
	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, video->transmission.format);

	d->protocol->sendStartMultiMediaTransmission(d, channel, payloadType, bitRate);                                        // extra mediatransmission channel retension

	char buf1[NI_MAXHOST + NI_MAXSERV];
	char buf2[NI_MAXHOST + NI_MAXSERV];
	sccp_copy_string(buf1, sccp_netsock_stringify(&video->phone), sizeof(buf1));
	sccp_copy_string(buf2, sccp_netsock_stringify(&video->phone_remote), sizeof(buf2));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMultiMediaTransmission) Tell Phone to send VRTP/UDP media from %s to %s (NAT: %s)\n", d->id, buf1, buf2, sccp_nat2str(d->nat));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (StartMultiMediaTransmission) Using format: %s(%d), payload:%d, TOS %d for call with PassThruId: %u and CallID: %u\n", d->id,
		codec2str(video->transmission.format), video->transmission.format, payloadType, d->video_tos, channel->passthrupartyid, channel->callid);

	iPbx.queue_control(channel->owner, AST_CONTROL_VIDUPDATE);
}

int sccp_channel_multiMediaTransmissionStarted(constDevicePtr d, channelPtr c)
{
	//pbx_builtin_setvar_helper(c->owner, "_SCCP_VIDEO_MODE", sccp_video_mode2str(sccp_channel_getVideoMode(c)));
	//iPbx.queue_control(c->owner, AST_CONTROL_VIDUPDATE);
	//return SCCP_RTP_STATUS_ACTIVE;
	pbx_assert(d != NULL && c != NULL);
	sccp_rtp_t * video = &(c->rtp.video);
	// check channel state
	if (!video->instance) {
		pbx_log(LOG_ERROR, "%s: Channel has no vrtp instance!\n", d->id);
		sccp_channel_endcall(c);											// FS - 350
		return SCCP_RTP_STATUS_INACTIVE;
	}

	if(c->isHangingUp || !c->owner || pbx_check_hangup_locked(c->owner) || pbx_channel_hangupcause(c->owner) == AST_CAUSE_ANSWERED_ELSEWHERE || SCCP_CHANNELSTATE_Idling(c->state)
	   || SCCP_CHANNELSTATE_IsTerminating(c->state)) {
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop Tone %s\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
			c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
			return SCCP_RTP_STATUS_ACTIVE;
		}
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP))(VERBOSE_PREFIX_3 "%s: (multiMediaTransmissionStarted) Channel is already terminating. Giving up... (%s)\n", DEV_ID_LOG(d), sccp_channelstate2str(c->state));
		return SCCP_RTP_STATUS_ACTIVE;
	}

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Multi Media Transmission Started (State: %s[%d])\n", d->id, sccp_channelstate2str(c->state), c->state);

	sccp_rtp_appendState(video, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_ACTIVE);
	if (c->owner) {
		iPbx.queue_control(c->owner, AST_CONTROL_VIDUPDATE);
	}
	return SCCP_RTP_STATUS_ACTIVE;
}

/*!
 * \brief Stop Multi Media Transmission (Video) on Channel
 * \param channel SCCP Channel
 * \param KeepPortOpen Boolean
 */
void sccp_channel_stopMultiMediaTransmission(constChannelPtr channel, boolean_t KeepPortOpen)
{
	sccp_msg_t *msg = NULL;

	pbx_assert(channel != NULL);
	sccp_rtp_t *video = (sccp_rtp_t *) &(channel->rtp.video);

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_ERROR, "%s: (stopMultiMediaReceiveChannel) Could not retrieve device from channel\n", channel->designator);
		return;
	}
	// stopping phone vrtp
	if(sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Stop multimediatransmission on device %s (KeepPortOpen: %s)\n", channel->designator, d->id, KeepPortOpen ? "YES" : "NO");
		REQ(msg, StopMultiMediaTransmission);
		msg->data.StopMultiMediaTransmission.lel_conferenceId = htolel(channel->callid);
		msg->data.StopMultiMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.StopMultiMediaTransmission.lel_callReference = htolel(channel->callid);
		msg->data.StopMultiMediaTransmission.lel_portHandlingFlag = htolel(KeepPortOpen);
		sccp_dev_send(d, msg);
		sccp_rtp_setState(video, SCCP_RTP_TRANSMISSION, SCCP_RTP_STATUS_INACTIVE);
	}
}

#if UNUSEDCODE // 2015-11-01
void sccp_channel_updateMultiMediaTransmission(constChannelPtr channel)
{
	if(sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_TRANSMISSION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMultiMediaTransmission) Stop multiemedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_stopMultiMediaTransmission(channel, TRUE);
	}
	if(!sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_TRANSMISSION)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (updateMultiMediaTransmission) Start multimedia transmission on channel %d\n", channel->currentDeviceId, channel->callid);
		sccp_channel_startMultiMediaTransmission(channel);
	}
}
#endif

sccp_rtp_status_t sccp_channel_closeAllMediaTransmitAndReceive(constChannelPtr channel)
{
	//! \todo This is what we should check, need to cover all calling paths though
	// pbx_assert(channel != NULL && d != NULL);
	// for now only check channel and skip otherwise (dangerous)
	pbx_assert(channel != NULL);
	sccp_rtp_status_t res = SCCP_RTP_STATUS_ACTIVE;
	sccp_rtp_t * audio = (sccp_rtp_t *)&(channel->rtp.audio);
	sccp_rtp_t * video = (sccp_rtp_t *)&(channel->rtp.video);

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_closeAllMediaTransmitAndReceive) Stop All Media Reception and Transmission on device %s\n", channel->designator, channel->currentDeviceId);
	if(sccp_rtp_getState(audio, SCCP_RTP_RECEPTION)) {
		sccp_channel_closeReceiveChannel(channel, FALSE);
	}
	if(sccp_rtp_getState(video, SCCP_RTP_RECEPTION)) {
		sccp_channel_closeMultiMediaReceiveChannel(channel, FALSE);
	}
	if(sccp_rtp_getState(audio, SCCP_RTP_TRANSMISSION)) {
		sccp_channel_stopMediaTransmission(channel, FALSE);
	}
	if(sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION)) {
		sccp_channel_stopMultiMediaTransmission(channel, FALSE);
	}
	res = SCCP_RTP_STATUS_INACTIVE;
	if (channel->rtp.audio.instance || channel->rtp.video.instance) {
		sccp_rtp_stop(channel);
	}
	return res;
}

/*
 * \brief Check if we are in the middle of a transfer and if transfer on hangup is wanted, function is only called by sccp_handle_onhook for now 
 */
boolean_t sccp_channel_transfer_on_hangup(constChannelPtr channel)
{
	boolean_t result = FALSE;
	if (!channel || !GLOB(transfer_on_hangup)) {
		return result;
	}
	AUTO_RELEASE(sccp_device_t, d , channel->privateData->device ? sccp_device_retain(channel->privateData->device) : NULL);

	if (d && (SCCP_CHANNELSTATE_IsSettingUp(channel->state) || SCCP_CHANNELSTATE_IsConnected(channel->state))) {	/* Complete transfer when one is in progress */
		sccp_channel_t *transferee = d->transferChannels.transferee;
		sccp_channel_t *transferer = d->transferChannels.transferer;

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
void sccp_channel_end_forwarding_channel(channelPtr orig_channel)
{
	sccp_channel_t *c = NULL;

	if (!orig_channel || !orig_channel->line) {
		return;
	}

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&orig_channel->line->channels, c, list) {
		if (c->parentChannel == orig_channel) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_end_forwarding_channel) Send Hangup to CallForwarding Channel:%s\n", orig_channel->designator, c->designator);
			sccp_channel_release(&c->parentChannel);				/* explicit release refcounted parentChannel */
			/* make sure a ZOMBIE channel is hungup using requestHangup if it is still available after the masquerade */
			c->hangupRequest = sccp_astgenwrap_requestHangup;
			/* need to use scheduled hangup, so that we clear any outstanding locks (during masquerade) before calling hangup */
			c->isHangingUp = TRUE;
			if (ATOMIC_FETCH(&c->scheduler.deny, &c->scheduler.lock) == 0) {
				sccp_channel_stop_and_deny_scheduled_tasks(c);
			}
			c->hangupRequest(c);
			//sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
			
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
	AUTO_RELEASE(sccp_channel_t, channel, sccp_channel_retain(data));
	if(channel) {
		channel->scheduler.hangup_id = -3;
		sccp_log(DEBUGCAT_CHANNEL) ("%s: Scheduled Hangup\n", channel->designator);
		if (ATOMIC_FETCH(&channel->scheduler.deny, &channel->scheduler.lock) == 0) {			/* we cancelled all scheduled tasks, so we should not be hanging up this channel anymore */
			sccp_channel_stop_and_deny_scheduled_tasks(channel);
			sccp_channel_endcall(channel);
		}
		sccp_channel_release((sccp_channel_t **)&data);							// release channel retained in scheduled event
	}
	return 0;												// return 0 to release schedule !
}

/* 
 * Remove Schedule digittimeout
 */
gcc_inline void sccp_channel_stop_schedule_digittimout(constChannelPtr channel)
{
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));

	if (c && c->scheduler.digittimeout_id > -1 && iPbx.sched_wait(c->scheduler.digittimeout_id) > 0) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: stop schedule digittimeout %d\n", c->designator, c->scheduler.digittimeout_id);
		iPbx.sched_del_ref(&c->scheduler.digittimeout_id, c);
	}
}

/* 
 * Schedule hangup if allowed and not already scheduled
 * \note needs to take retain on channel to pass it on the the scheduled hangup
 */
gcc_inline void sccp_channel_schedule_hangup(constChannelPtr channel, int timeout)
{
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	int res = 0;

	/* only schedule if allowed and not already scheduled */
	if (c && c->scheduler.hangup_id == -1 && !ATOMIC_FETCH(&c->scheduler.deny, &c->scheduler.lock)) {	
		res = iPbx.sched_add_ref(&c->scheduler.hangup_id, timeout, _sccp_channel_sched_endcall, c);
		if (res < 0) {
			pbx_log(LOG_NOTICE, "%s: Unable to schedule dialing in '%d' ms\n", c->designator, timeout);
		}
	}
}

/* 
 * Schedule digittimeout if allowed
 * Release any previously scheduled digittimeout
 */
gcc_inline void sccp_channel_schedule_digittimeout(constChannelPtr channel, int timeout)
{
	sccp_channel_t *c = sccp_channel_retain(channel);

	/* only schedule if allowed and not already scheduled */
	if (c && c->scheduler.hangup_id == -1 && !ATOMIC_FETCH(&c->scheduler.deny, &c->scheduler.lock)) {	
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: schedule digittimeout %d\n", c->designator, timeout);
		if (c->scheduler.digittimeout_id == -1) {
			iPbx.sched_add_ref(&c->scheduler.digittimeout_id, timeout * 1000, sccp_pbx_sched_dial, c);
		} else {
			iPbx.sched_replace_ref(&c->scheduler.digittimeout_id, timeout * 1000, sccp_pbx_sched_dial, c);
		}
		sccp_channel_release(&c);
	}
}

void sccp_channel_stop_and_deny_scheduled_tasks(constChannelPtr channel)
{
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));
	if (c) {
		(void) ATOMIC_INCR(&c->scheduler.deny, TRUE, &c->scheduler.lock);
		sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "%s: Disabling scheduler / Removing Scheduled tasks (digittimeout_id:%d) (hangup_id:%d) (cfwd_noanswer_id:%d)\n", c->designator, c->scheduler.digittimeout_id,
					   c->scheduler.hangup_id, c->scheduler.cfwd_noanswer_id);
		if (c->scheduler.digittimeout_id > -1) {
			iPbx.sched_del_ref(&c->scheduler.digittimeout_id, c);
		}
		if (c->scheduler.hangup_id > -1) {
			iPbx.sched_del_ref(&c->scheduler.hangup_id, c);
		}
		if(c->scheduler.cfwd_noanswer_id > -1) {
			iPbx.sched_del_ref(&c->scheduler.cfwd_noanswer_id, c);
		}
	}
}

gcc_inline void sccp_channel_schedule_cfwd_noanswer(constChannelPtr channel, int timeout)
{
	sccp_channel_t * c = sccp_channel_retain(channel);
	/* only schedule if allowed and not already scheduled */
	if(c && c->scheduler.cfwd_noanswer_id == -1 && !ATOMIC_FETCH(&c->scheduler.deny, &c->scheduler.lock)) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: schedule cfwd_noanswer %d\n", c->designator, timeout);
		if(c->scheduler.cfwd_noanswer_id == -1) {
			iPbx.sched_add_ref(&c->scheduler.cfwd_noanswer_id, timeout * 1000, sccp_pbx_cfwdnoanswer_cb, c);
		}
		sccp_channel_release(&c);
	}
}

gcc_inline void sccp_channel_stop_schedule_cfwd_noanswer(constChannelPtr channel)
{
	AUTO_RELEASE(sccp_channel_t, c, sccp_channel_retain(channel));

	if(c && c->scheduler.cfwd_noanswer_id > -1 && iPbx.sched_wait(c->scheduler.cfwd_noanswer_id) > 0) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: stop schedule cfwd_noanswer_id %d\n", c->designator, c->scheduler.cfwd_noanswer_id);
		iPbx.sched_del_ref(&c->scheduler.cfwd_noanswer_id, c);
	}
}

/*!
 * \brief Hangup this channel.
 * \param channel *retained* SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_endcall(channelPtr channel)
{
	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}
	channel->isHangingUp = TRUE;
	if (ATOMIC_FETCH(&channel->scheduler.deny, &channel->scheduler.lock) == 0) {
		sccp_channel_stop_and_deny_scheduled_tasks(channel);
	}
	/* end all call forwarded channels (our children) */
	sccp_channel_end_forwarding_channel(channel);

	/* this is a station active endcall or onhook */
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));

	if (d) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "%s: Ending call %s (state:%s)\n", d->id, channel->designator, sccp_channelstate2str(channel->state));
		if (d->transferChannels.transferee != channel) {
			sccp_channel_transfer_cancel(d, channel);
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
 * \brief get an SCCP Channel
 * Retrieve unused or allocate a new channel
 */
channelPtr sccp_channel_getEmptyChannel(constLinePtr l, constDevicePtr d, channelPtr maybe_c, skinny_calltype_t calltype, PBX_CHANNEL_TYPE * parentChannel, const void *ids)
{
	pbx_assert(l != NULL && d != NULL);
	sccp_log(DEBUGCAT_CORE)("%s: (getEmptyChannel) on line:%s, maybe_c:%s\n", d->id, l->name, maybe_c ? maybe_c->designator : "");
	sccp_channel_t *channel = NULL;
	{
		AUTO_RELEASE(sccp_channel_t, c, maybe_c ? sccp_channel_retain(maybe_c) : sccp_device_getActiveChannel(d));
		if (c) {
			sccp_log(DEBUGCAT_CORE)("%s: (getEmptyChannel) got channel already.\n", d->id);
			AUTO_RELEASE(const sccp_device_t, call_associated_device, c->getDevice(c));
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {		// reuse unused channel
				sccp_log(DEBUGCAT_CORE)("%s: (getEmptyChannel) channel not in use -> reuse it.\n", d->id);
				c->setTone(c, SKINNY_TONE_SILENCE, SKINNY_TONEDIRECTION_USER);
				channel = sccp_channel_retain(c);
				channel->calltype = calltype;
				return channel;
			} else if (call_associated_device && call_associated_device == d && !sccp_channel_hold(c)) {
				pbx_log(LOG_ERROR, "%s: Putting Active Channel %s OnHold failed -> Cancelling new CaLL\n", d->id, c->designator);
				return NULL;
			}
		}
	}
	if (!channel && !(channel = sccp_channel_allocate(l, d))) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	channel->calltype = calltype;
	if(sccp_pbx_channel_allocate(channel, ids, parentChannel)) {
		return channel;
	}
	return NULL;
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \param parentChannel SCCP Channel for which the channel was created
 * \param ids Optional Linked Channel ID's (> asterisk-1.8)
 * \return a *retained* SCCP Channel or NULL if something is wrong
 *
 * \callgraph
 * \callergraph
 * 
 */
channelPtr sccp_channel_newcall(constLinePtr l, constDevicePtr device, const char *dial, skinny_calltype_t calltype, PBX_CHANNEL_TYPE * parentChannel, const void *ids)
{
	/* handle outgoing calls */
	if (!l || !device) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if device or line is not defined!\n");
		return NULL;
	}

	sccp_channel_t * const channel = sccp_channel_getEmptyChannel(l, device, NULL, calltype, parentChannel, ids);
	if (!channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	channel->softswitch_action = SCCP_SOFTSWITCH_DIAL;							/* softswitch will catch the number to be dialed */
	channel->ss_data = 0;											/* nothing to pass to action */

	/* copy the number to dial in the ast->exten */
	iPbx.set_callstate(channel, AST_STATE_OFFHOOK);
	if (dial) {
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_SPEEDDIAL);
		sccp_copy_string(channel->dialedNumber, dial, sizeof(channel->dialedNumber));
		sccp_pbx_softswitch(channel);									/* we know the number to dial -> softswitch */
	} else {
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_OFFHOOK);
		sccp_channel_schedule_digittimeout(channel, GLOB(firstdigittimeout));
	}

	return channel;
}

/*! \internal
 *
 * \brief Locks both sccp_channel and sccp_channel owner if owner is present.
 *
 * \note This function gives a ref to sccp_channel->owner if it is present and locked.
 *       This reference must be decremented after sccp_channel->owner is unlocked.
 *
 * \note If the function exists early (ie: no c->owner set), the sccp_channel returned will be locked.
 *
 * \pre sccp_channel is not locked
 * \post sccp_channel is always locked
 * \post sccp_channel->owner is locked and its reference count is increased (if sccp_channel->owner is not NULL)
 *
 * \returns a pointer to the locked and reffed sccp_channel->owner channel if it exists.
 */
PBX_CHANNEL_TYPE * sccp_channel_lock_full(channelPtr c, boolean_t retry_indefinitely)
{
	PBX_CHANNEL_TYPE * pbx_channel = NULL;

	/* Locking is simple when it is done right.  If you see a deadlock resulting
	 * in this function, it is not this function's fault, Your problem exists elsewhere.
	 * This function is perfect... seriously. */
	do {
		/* First, get the pbx_channel and grab a reference to it */
		sccp_channel_lock(c);
		pbx_channel = c->owner;
		if(pbx_channel) {
			/* The pbx_channel can not go away while we hold the c lock.
			 * Give the pbx_channel a ref so it will not go away after we let
			 * the c lock go. */
			pbx_channel_ref(pbx_channel);
		} else {
			/* no pbx_channel, return c locked */
			return NULL;
		}
		/* We had to hold the c lock while getting a ref to the owner pbx_channel
		 * but now we have to let this lock go in order to preserve proper
		 * locking order when grabbing the pbx_channel lock */
		sccp_channel_unlock(c);

		/* Look, no deadlock avoidance, hooray! */
		pbx_channel_lock(pbx_channel);
		sccp_channel_lock(c);
		if(c->owner == pbx_channel) {
			/* done */
			break;
		}

		/* If the owner changed while everything was unlocked, no problem,
		 * just start over and everthing will work.  This is rare, do not be
		 * confused by this loop and think this it is an expensive operation.
		 * The majority of the calls to this function will never involve multiple
		 * executions of this loop. */
		sccp_channel_unlock(c);
		pbx_channel_unlock(pbx_channel);
		pbx_channel_unref(pbx_channel);
	} while(retry_indefinitely);

	/* If owner exists, it is locked and reffed */
	return pbx_channel;
}

/*!
 * \brief Complete Answer an Incoming Call via Callback when receiveChannelOpen finished.
 * \param channel incoming *retained* SCCP channel
 *
 * \callgraph
 * \callergraph
 *
 * Steps (sccp_channel_answer):
 * 1. Lock the pbx_channel and sccp_channel in orderly fashion
 * 2. Set callback to finish the answer sequence
 * 3. Start openReceiveChannel
 * 4. when receiveChannelOpen returns it will call the callback
 *
 * During callback (channel_answer_completion):
 * 1. Regaing the pbx_channel and sccp_channel lock
 * 2. send AST_CONTROL_ANSWER
 * 3. pbx::app_dial will terminate any other competetitors trying to answer this channel (astwrap_hangup -> sccp_pbx_remote_hangup)
 * 4. startMediaTransmission
 * 5. Indicate SCCP_CHANNELSTATE_CONNECTED
 */
static void channel_answer_completion(constChannelPtr channel)
{
	pbx_assert(channel && channel->owner);
	AUTO_RELEASE(sccp_channel_t, c, sccp_channel_retain(channel));
	AUTO_RELEASE(sccp_device_t, d, sccp_channel_getDevice(c));
	if(c && d && c->privateData->isAnswering) {
		PBX_CHANNEL_TYPE * pbx_channel = NULL;
		if((pbx_channel = sccp_channel_lock_full(c, TRUE))) {
			if(!pbx_check_hangup_locked(pbx_channel)) {
				sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (%s) Answering Call: %s (state:%s)\n", d->id, __func__, c->designator, ast_state2str(ast_channel_state(pbx_channel)));
				sccp_channel_startMediaTransmission(c);
				if(ast_channel_state(pbx_channel) != AST_STATE_UP) {
					iPbx.queue_control(pbx_channel, AST_CONTROL_ANSWER);
				}
#ifdef CS_SCCP_VIDEO
				if(sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF && sccp_device_isVideoSupported(d)) {
					sccp_channel_openMultiMediaReceiveChannel(c);
				}
#endif
				/*
								AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(c->line));
								if (l && SCCP_LIST_GETSIZE(&l->devices) > 1) {
									sccp_linedevice_t * ld = NULL;
									SCCP_LIST_LOCK(&l->devices);
									SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
										sccp_device_t *otherdevice = ld->device;
										if (otherdevice != d) {
											sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (%s) Hangingup up sharing subscribers\n", DEV_ID_LOG(otherdevice), __func__);
											otherdevice->indicate->callhistory(otherdevice, ld->lineInstance, c->callid, otherdevice->callhistory_answered_elsewhere);
											sccp_dev_displayprompt(otherdevice, ld->lineInstance, c->callid, SKINNY_DISP_IN_USE_REMOTE, GLOB(digittimeout));
											otherdevice->indicate->onhook(otherdevice, ld->lineInstance, c->callid);
										}
									}
									SCCP_LIST_UNLOCK(&l->devices);
								}
				*/
				/** check for monitor request */
				if((d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) && !(d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
					pbx_log(LOG_NOTICE, "%s: request monitor\n", d->id);
					sccp_feat_monitor(d, NULL, 0, c);
				}
				c->subscribers = 1;
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
#ifdef CS_MANAGER_EVENTS
				if(GLOB(callevents)) {
					char tmpCallingNumber[StationMaxDirnumSize] = { 0 };
					char tmpCallingName[StationMaxNameSize] = { 0 };
					char tmpOrigCallingName[StationMaxNameSize] = { 0 };
					char tmpLastRedirectingName[StationMaxNameSize] = { 0 };
					iCallInfo.Getter(channel->privateData->callInfo, SCCP_CALLINFO_CALLINGPARTY_NUMBER, &tmpCallingNumber, SCCP_CALLINFO_CALLINGPARTY_NAME, &tmpCallingName,
							 SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, &tmpOrigCallingName, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &tmpLastRedirectingName, SCCP_CALLINFO_KEY_SENTINEL);
					manager_event(EVENT_FLAG_CALL, "CallAnswered",
						      "Channel: %s\r\n"
						      "SCCPLine: %s\r\n"
						      "SCCPDevice: %s\r\n"
						      "Uniqueid: %s\r\n"
						      "CallingPartyNumber: %s\r\n"
						      "CallingPartyName: %s\r\n"
						      "originalCallingParty: %s\r\n"
						      "lastRedirectingParty: %s\r\n",
						      c->designator, c->line->name, d->id, iPbx.getChannelUniqueID(c), tmpCallingNumber, tmpCallingName, tmpOrigCallingName, tmpLastRedirectingName);
				}
#endif
				sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (%s) Answered channel %s\n", d->id, __func__, c->designator);
			} else {
				pbx_log(LOG_WARNING, "%s: (%s) Attempted to answer channel '%s' but someone else beat us to it (actual state:%s)\n", DEV_ID_LOG(d), __func__, c->designator,
					pbx_state2str(ast_channel_state(pbx_channel)));
			}
			pbx_channel_unref(pbx_channel);                                         // reffed by sccp_channel_lock_full
			pbx_channel_unlock(pbx_channel);                                        // locked by sccp_channel_lock_full
		}
		sccp_channel_unlock(c);                                        // locked by sccp_channel_lock_full
	}
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
 * Steps (sccp_channel_answer):
 * 1. Lock the pbx_channel and sccp_channel in orderly fashion
 * 2. Set callback to finish the answer sequence
 * 3. Start openReceiveChannel
 * 4. when receiveChannelOpen returns it will call the callback
 *
 * During callback (channel_answer_completion):
 * 1. Regaing the pbx_channel and sccp_channel lock
 * 2. send AST_CONTROL_ANSWER
 * 3. pbx::app_dial will terminate any other competetitors trying to answer this channel (astwrap_hangup -> sccp_pbx_remote_hangup)
 * 4. startMediaTransmission
 * 5. Indicate SCCP_CHANNELSTATE_CONNECTED
 */
void sccp_channel_answer(constDevicePtr device, channelPtr channel)
{
	if(!channel || !channel->line || !channel->owner || !device) {
		pbx_log(LOG_ERROR, "%s: (%s) Answering on unknown channel/device\n", (channel ? channel->designator : 0), __func__);
		return;
	}
	sccp_channel_end_forwarding_channel(channel);

	AUTO_RELEASE(sccp_channel_t, previous_channel, sccp_device_getActiveChannel(device));
	if(previous_channel && !sccp_channel_hold(previous_channel)) {
		pbx_log(LOG_ERROR, "%s: Putting Active Channel:%s OnHold failed -> While trying to answer incoming call:%s. Skipping answer!\n", device->id, previous_channel->designator, channel->designator);
		return;
	}

	PBX_CHANNEL_TYPE * pbx_channel = NULL;
	if((pbx_channel = sccp_channel_lock_full(channel, TRUE))) {
		if(pbx_channel_state(pbx_channel) == AST_STATE_RINGING && !pbx_check_hangup_locked(pbx_channel) && !channel->privateData->isAnswering) {
			channel->setDevice(channel, device);
			channel->privateData->isAnswering = TRUE;
			if (channel->state != SCCP_CHANNELSTATE_OFFHOOK) {	/* 7911 need to have callstate offhook, before connected, to transmit audio */
				sccp_indicate(device, channel, SCCP_CHANNELSTATE_OFFHOOK);
			}
			pbx_setstate(pbx_channel, AST_STATE_OFFHOOK);
			uint16_t lineInstance = sccp_device_find_index_for_line(device, channel->line->name);
			sccp_device_sendcallstate(device, lineInstance, channel->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW,
						  SKINNY_CALLINFO_VISIBILITY_DEFAULT);	// send connected, so it is not listed as missed call on device that fail the answer first
			sccp_rtp_setCallback(&channel->rtp.audio, SCCP_RTP_RECEPTION, channel_answer_completion);
			sccp_channel_openReceiveChannel(channel);

			AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(channel->line));
			if(l && SCCP_LIST_GETSIZE(&l->devices) > 1) {
				sccp_linedevice_t * ld = NULL;
				SCCP_LIST_LOCK(&l->devices);
				SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
					sccp_device_t * otherdevice = ld->device;
					if(otherdevice != device) {
						sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (%s) Hanging up sharing subscribers\n", DEV_ID_LOG(otherdevice), __func__);
						otherdevice->indicate->callhistory(otherdevice, ld->lineInstance, channel->callid, otherdevice->callhistory_answered_elsewhere);
						sccp_dev_displayprompt(otherdevice, ld->lineInstance, channel->callid, SKINNY_DISP_IN_USE_REMOTE, GLOB(digittimeout));
						otherdevice->indicate->onhook(otherdevice, ld->lineInstance, channel->callid);
					}
				}
				SCCP_LIST_UNLOCK(&l->devices);
			}
		} else {
			pbx_log(LOG_WARNING, "%s: (%s) Attempted to answer channel '%s' but someone else beat us to it (actual state:%s)\n", DEV_ID_LOG(device), __func__, channel->designator,
				pbx_state2str(ast_channel_state(pbx_channel)));
		}
		pbx_channel_unref(pbx_channel);                                         // reffed by sccp_channel_lock_full
		pbx_channel_unlock(pbx_channel);                                        // locked by sccp_channel_lock_full
	}
	sccp_channel_unlock(channel);							// locked by sccp_channel_lock_full
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
int sccp_channel_hold(channelPtr channel)
{
	uint16_t instance = 0;

	if (!channel || !channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to put on hold\n");
		return FALSE;
	}

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(channel->line));
	if (!l) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %s has no line attached to it\n", channel->designator);
		return FALSE;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel %s has no device attached to it\n", channel->designator);
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
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s can't put on hold an inactive channel %s with state %s (%d)... cancelling hold action.\n", d->id, channel->designator, sccp_channelstate2str(channel->state), channel->state);
		/* hard button phones need it */
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		return FALSE;
	}

	if (d->useHookFlash() && d->transfer && d->transferChannels.transferer == channel) {	// deal with single line phones like 6901, which do not have softkeys
												// 6901 is cancelling the transfer by pressing the hold key on the transferer
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: We are the middle of a transfer, pressed hold on the transferer channel(%s) -> cancel transfer\n", d->id, channel->designator);
		AUTO_RELEASE(sccp_channel_t, resumeChannel, sccp_channel_retain(d->transferChannels.transferee));
		if (resumeChannel) {
			sccp_channel_endcall(d->transferChannels.transferer);
			sccp_channel_resume(d, resumeChannel, FALSE);
		}
		return TRUE;
	} 

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold the channel %s\n", d->id, channel->designator);

#ifdef CS_SCCP_CONFERENCE
	if (channel->conference) {
		sccp_conference_hold(channel->conference);
	} else
#endif
	{
		if (channel->owner) {
			iPbx.queue_control_data(channel->owner, AST_CONTROL_HOLD, channel->musicclass, sccp_strlen(channel->musicclass) + 1);
		}
	}
	//sccp_rtp_stop(channel);
	sccp_dev_setActiveLine(d, NULL);
	sccp_indicate(d, channel, SCCP_CHANNELSTATE_HOLD);							// this will also close (but not destroy) the RTP stream
	sccp_channel_setDevice(channel, NULL);

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: On\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", iPbx.getChannelName(channel), iPbx.getChannelUniqueID(channel));
	}
#endif

	if (l) {
		l->statistic.numberOfHeldChannels++;
	}

	sccp_log_and((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", channel->passthrupartyid, channel->state);
	return TRUE;
}

/*!
 * \brief Actual Resume Implementation
 *
 * This will run while channel and pbx_channel are locked and we hold a reference, so that it cannot escape us
 *
 * \callgraph
 * \callergraph
 *
 */
static int channel_resume_locked(devicePtr d, linePtr l, channelPtr channel, boolean_t swap_channels)
{
	uint16_t instance = 0;

	/* look if we have a call to put on hold */
	if (swap_channels) {
		AUTO_RELEASE(sccp_channel_t, sccp_active_channel , sccp_device_getActiveChannel(d));

		/* there is an active call, if offhook channelstate then hangup else put it on hold */
		if (sccp_active_channel) {
			if (sccp_active_channel->state <= SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "%s: active channel is brand new and unused, hanging it up before resuming another\n", sccp_active_channel->designator);
				sccp_channel_endcall(sccp_active_channel);
			} else if (!(sccp_channel_hold(sccp_active_channel))) {				// hold failed, give up
				pbx_log(LOG_WARNING, "%s: swap_channels failed to put channel on hold. exiting\n", sccp_active_channel->designator);
				return FALSE;
			}
		}
	}

	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE || channel->state == SCCP_CHANNELSTATE_PROCEED) {
		if (!(sccp_channel_hold(channel))) {
			pbx_log(LOG_WARNING, "%s: channel still connected before resuming, put on hold failed. exiting\n", channel->designator);
			return FALSE;
		}
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* resume an active call */
	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER && channel->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong in the code let's notify it for a fix */
		pbx_log(LOG_ERROR, "%s can't resume the channel %s. Not on hold\n", d->id, channel->designator);
		sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_NO_ACTIVE_CALL_TO_PUT_ON_HOLD, SCCP_DISPLAYSTATUS_TIMEOUT);
		return FALSE;
	}

	if (d->transferChannels.transferee != channel) {
		sccp_channel_transfer_release(d, channel);			/* explicitly release transfer if we are in the middle of a transfer */
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume the channel %s\n", d->id, channel->designator);
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
	if (channel->conference) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume Conference on the channel %s\n", d->id, channel->designator);
		sccp_conference_resume(channel->conference);
		sccp_dev_set_keyset(d, instance, channel->callid, KEYMODE_CONNCONF);
	} else
#endif
	{
		if (channel->owner) {
			iPbx.queue_control(channel->owner, AST_CONTROL_UNHOLD);
		}
	}

	channel->state = SCCP_CHANNELSTATE_HOLD;
#ifdef CS_AST_CONTROL_SRCUPDATE
	iPbx.queue_control(channel->owner, AST_CONTROL_SRCUPDATE);						// notify changes e.g codec
#endif
#ifdef CS_SCCP_CONFERENCE
	if (channel->conference) {
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);				// this will also reopen the RTP stream
	} else
#endif
	{
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONNECTED);						// this will also reopen the RTP stream
	}

#ifdef CS_SCCP_VIDEO
	if(channel->rtp.video.instance && sccp_channel_getVideoMode(channel) != SCCP_VIDEO_MODE_OFF && sccp_device_isVideoSupported(d)) {
		if(!sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_RECEPTION)) {
			sccp_channel_openMultiMediaReceiveChannel(channel);
		} else if((sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_RECEPTION) & SCCP_RTP_STATUS_ACTIVE) && !sccp_rtp_getState(&channel->rtp.video, SCCP_RTP_TRANSMISSION)) {
			sccp_channel_startMultiMediaTransmission(channel);
		}
	}
#endif

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: Off\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", iPbx.getChannelName(channel), iPbx.getChannelUniqueID(channel));
	}
#endif

	/* state of channel is set down from the remoteDevices, so correct channel state */
	if (channel->conference) {
		channel->state = SCCP_CHANNELSTATE_CONNECTEDCONFERENCE;
	} else {
		channel->state = SCCP_CHANNELSTATE_CONNECTED;
	}
	l->statistic.numberOfHeldChannels--;

	/** set called party name */
	{
		AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_find(d, l));

		if(ld) {
			char tmpNumber[StationMaxDirnumSize] = {0};
			char tmpName[StationMaxNameSize] = {0};
			if(!sccp_strlen_zero(ld->subscriptionId.number)) {
				snprintf(tmpNumber, StationMaxDirnumSize, "%s%s", channel->line->cid_num, ld->subscriptionId.number);
			} else {
				snprintf(tmpNumber, StationMaxDirnumSize, "%s%s", channel->line->cid_num, channel->line->defaultSubscriptionId.number);
			}

			if(!sccp_strlen_zero(ld->subscriptionId.name)) {
				snprintf(tmpName, StationMaxNameSize, "%s%s", channel->line->cid_name, ld->subscriptionId.name);
			} else {
				snprintf(tmpName, StationMaxNameSize, "%s%s", channel->line->cid_name, channel->line->defaultSubscriptionId.name);
			}
			if(channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				iCallInfo.SetCallingParty(channel->privateData->callInfo, tmpNumber, tmpName, NULL);
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set callingPartyNumber '%s' callingPartyName '%s'\n", d->id, tmpNumber, tmpName);
			} else if(channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				iCallInfo.SetCalledParty(channel->privateData->callInfo, tmpNumber, tmpName, NULL);
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Set calledPartyNumber '%s' calledPartyName '%s'\n", d->id, tmpNumber, tmpName);
			}
			iPbx.set_connected_line(channel, tmpNumber, tmpName, AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER);
		}
	}
	/* */

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
int sccp_channel_resume(constDevicePtr device, channelPtr channel, boolean_t swap_channels)
{
	uint16_t instance = 0;
	PBX_CHANNEL_TYPE * pbx_channel = NULL;
	if(!channel || !channel->owner || !channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. No channel provided to resume\n");
		return FALSE;
	}
	AUTO_RELEASE(sccp_channel_t, c, sccp_channel_retain(channel));
	if(!c) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel could not be retained.\n");
		return FALSE;
	}
	AUTO_RELEASE(sccp_device_t, d, sccp_device_retain(device));
	AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(c->line));
	if(!d || !l) {
		pbx_log(LOG_WARNING, "%s: weird error. The channel has no line or device\n", c->designator);
		return FALSE;
	}

	if((pbx_channel = sccp_channel_lock_full(channel, FALSE))) {
		instance = channel_resume_locked(d, l, channel, swap_channels);
		pbx_channel_unref(pbx_channel);                                         // reffed by sccp_channel_lock_full
		pbx_channel_unlock(pbx_channel);                                        // locked by sccp_channel_lock_full
	} else {
		pbx_log(LOG_WARNING, "%s: weird error. We could not get a reference to the pbx_channel, skipping resume.\n", c->designator);
	}
	sccp_channel_unlock(channel);                                        // locked by sccp_channel_lock_full
	return instance;
}

void sccp_channel_addCleanupJob(channelPtr c, void *(*function_p) (void *), void *arg_p)
{
	if (!c) {
		return;
	}
	sccp_threadpool_job_t * newJob = NULL;
	if (!(newJob = (sccp_threadpool_job_t *) sccp_calloc(sizeof *newJob, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		exit(1);
	}

	/* add function and argument */
	newJob->function = function_p;
	newJob->arg = arg_p;

	/* add job to cleanup jobqueue */
	SCCP_LIST_LOCK(&(c->privateData->cleanup_jobs));
	SCCP_LIST_INSERT_TAIL(&(c->privateData->cleanup_jobs), newJob, list);
	SCCP_LIST_UNLOCK(&(c->privateData->cleanup_jobs));
}

/*!
 * \brief Cleanup Channel before Free.
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_channel_clean(channelPtr channel)
{
	sccp_selectedchannel_t * sccp_selected_channel = NULL;

	if (!channel) {
		pbx_log(LOG_ERROR, "SCCP:No channel provided to clean\n");
		return;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));

	// l = channel->line;
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Cleaning channel %s\n", channel->designator);

	if (ATOMIC_FETCH(&channel->scheduler.deny, &channel->scheduler.lock) == 0) {
		sccp_channel_stop_and_deny_scheduled_tasks(channel);
	}

	/* mark the channel DOWN so any pending thread will terminate */
	if (channel->owner) {
		pbx_setstate(channel->owner, AST_STATE_DOWN);
		/* postponing ast_channel_unref to sccp_channel destructor */
		//iPbx.set_owner(channel, NULL);
	}

	if (channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN) {
		iPbx.set_callstate(channel, AST_STATE_DOWN);
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
	}

	if (d) {
		/* make sure all rtp stuff is closed and destroyed */
		if (channel->rtp.audio.instance || channel->rtp.video.instance) {
			sccp_channel_closeAllMediaTransmitAndReceive(channel);
		}

		/* deactive the active call if needed */
		if (d->active_channel == channel) {
			sccp_device_setActiveChannel(d, NULL);
		}
		sccp_channel_transfer_release(d, channel);										/* explicitly release transfer when cleaning up channel */
#ifdef CS_SCCP_CONFERENCE
		if (d->conference && d->conference == channel->conference) {
			sccp_conference_release(&d->conference);									/* explicit release of conference */
		}
		if (channel->conference) {
			sccp_conference_release(&channel->conference);									/* explicit release of conference */
		}
#endif
		if (channel->privacy) {
			channel->privacy = FALSE;
			d->privacyFeature.status = SCCP_PRIVACYFEATURE_OFF;
			sccp_feat_changed(d, NULL, SCCP_FEATURE_PRIVACY);
		}

		if ((sccp_selected_channel = sccp_device_find_selectedchannel(d, channel))) {
			SCCP_LIST_LOCK(&d->selectedChannels);
			sccp_selected_channel = SCCP_LIST_REMOVE(&d->selectedChannels, sccp_selected_channel, list);
			SCCP_LIST_UNLOCK(&d->selectedChannels);
			sccp_channel_release(&sccp_selected_channel->channel);
			sccp_free(sccp_selected_channel);
		}
		sccp_dev_setActiveLine(d, NULL);
		sccp_dev_check_displayprompt(d);
	}
	if (channel->privateData) {
		if(channel->privateData->ld) {
			channel->privateData->ld->resetPickup(channel->privateData->ld);
		}

		if (channel->privateData->device) {
			sccp_channel_setDevice(channel, NULL);
		}

		if(channel->privateData->ld) {
			sccp_linedevice_release(&channel->privateData->ld);
		}

		sccp_threadpool_job_t * job = NULL;
		SCCP_LIST_LOCK(&channel->privateData->cleanup_jobs);
		while ((job = SCCP_LIST_REMOVE_HEAD(&channel->privateData->cleanup_jobs, list))) {
			SCCP_LIST_UNLOCK(&channel->privateData->cleanup_jobs);
			sccp_threadpool_jobqueue_add(GLOB(general_threadpool), job);
			SCCP_LIST_LOCK(&channel->privateData->cleanup_jobs);
		}
		SCCP_LIST_UNLOCK(&channel->privateData->cleanup_jobs);
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
int __sccp_channel_destroy(const void * data)
{
	sccp_channel_t * channel = (sccp_channel_t *) data;
	if (!channel) {
		pbx_log(LOG_NOTICE, "SCCP: channel destructor called with NULL pointer\n");
		return -1;
	}
	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Destroying channel %s\n", channel->designator);

	sccp_channel_lock(channel);

	if (channel->rtp.audio.instance || channel->rtp.video.instance) {
		sccp_channel_closeAllMediaTransmitAndReceive(channel);
		sccp_rtp_stop(channel);
		sccp_rtp_destroy(channel);
	}

	if (channel->privateData->callInfo) {
		iCallInfo.Destructor(&channel->privateData->callInfo);
	}

#if ASTERISK_VERSION_GROUP >= 113
	if (channel->caps) {
		ao2_t_cleanup(channel->caps, "sccp_channel_caps cleanup");
	}
#endif

	if (channel->owner) {
		if (iPbx.removeTimingFD) {
			iPbx.removeTimingFD(channel->owner); 
		}
		iPbx.set_owner(channel, NULL);
	}

	/* destroy immutables, by casting away const */
	sccp_free(*(char **)&channel->musicclass);
	sccp_free(*(char **)&channel->designator);
	SCCP_LIST_HEAD_DESTROY(&(channel->privateData->cleanup_jobs));
	sccp_free(*(struct sccp_private_channel_data **)&channel->privateData);
	sccp_line_release((sccp_line_t **)&channel->line);
	/* */

#ifndef SCCP_ATOMIC
	pbx_mutex_destroy(&channel->scheduler.lock);
#endif
	sccp_channel_unlock(channel);
	pbx_mutex_destroy(&channel->lock);
	return 0;
}

/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param channel *retained* SCCP Channel
 * \param device *retained* SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_transfer(channelPtr channel, constDevicePtr device)
{
	sccp_channelstate_t prev_channel_state = SCCP_CHANNELSTATE_ZOMBIE;
	uint32_t blindTransfer = 0;
	uint16_t instance = 0;
	PBX_CHANNEL_TYPE * pbx_channel_owner = NULL;
	PBX_CHANNEL_TYPE * pbx_channel_bridgepeer = NULL;

	if (!channel) {
		return;
	}

	if (!(channel->line)) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", channel->callid);
		sccp_dev_displayprompt(device, 0, channel->callid, SKINNY_DISP_NO_LINE_TO_TRANSFER, GLOB(digittimeout));
		return;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));

	if (!d) {
		/* transfer was pressed on first (transferee) channel, check if is our transferee channel and continue with d <= device */
		if (channel == device->transferChannels.transferee && device->transferChannels.transferer) {
			d = sccp_device_retain(device) /*ref_replace*/;
		} else if (channel->state == SCCP_CHANNELSTATE_HOLD) {
			if (SCCP_LIST_GETSIZE(&channel->line->devices) == 1) {
				d = sccp_device_retain(device) /*ref_replace*/;
			} else {
				pbx_log(LOG_WARNING, "%s: The channel %s is not attached to a particular device (hold on shared line, resume first)\n", DEV_ID_LOG(device), channel->designator);
				instance = sccp_device_find_index_for_line(device, channel->line->name);
				sccp_dev_displayprompt(device, instance, channel->callid, SKINNY_DISP_NO_LINE_TO_TRANSFER, GLOB(digittimeout));
				return;
			}
		} else {
			pbx_log(LOG_WARNING, "%s: The channel %s state is unclear. giving up\n", DEV_ID_LOG(device), channel->designator);
			instance = sccp_device_find_index_for_line(device, channel->line->name);
			sccp_dev_displayprompt(device, instance, channel->callid, SKINNY_DISP_NO_LINE_TO_TRANSFER, GLOB(digittimeout));
			return;
		}
	}
	instance = sccp_device_find_index_for_line(d, channel->line->name);

	if (!d->transfer || !channel->line->transfer) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", d->id);
		sccp_dev_displayprompt(device, instance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, GLOB(digittimeout));
		return;
	}

	/* are we in the middle of a transfer? */
	if (d->transferChannels.transferee && d->transferChannels.transferer) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", d->id);
		sccp_channel_transfer_complete(d->transferChannels.transferer);
		return;
	}
	/* exceptional case, we need to release half transfer before retaking, should never occur */
	/* \todo check out if this should be reactiveated or removed */
	// if (d->transferChannels.transferee && !d->transferChannels.transferer) {
	// sccp_channel_release(&d->transferChannels.transferee);						/* explicit release */
	// }
	if (!d->transferChannels.transferee && d->transferChannels.transferer) {
		sccp_channel_release(&d->transferChannels.transferer);						/* explicit release */
	}

	if ((d->transferChannels.transferee = sccp_channel_retain(channel))) {					/** channel to be transfered */
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s\n", d->id, channel->designator);

		prev_channel_state = channel->state;

		if (channel->state == SCCP_CHANNELSTATE_HOLD) {							/* already put on hold manually */
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_TRANSFER;
			// sccp_indicate(d, channel, SCCP_CHANNELSTATE_HOLD);                      		/* do we need to reindicate ? */
		}
		if ((channel->state != SCCP_CHANNELSTATE_OFFHOOK && channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CALLTRANSFER)) {
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_TRANSFER;

			if (!sccp_channel_hold(channel)) {							/* hold failed, restore */
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				sccp_channel_release(&d->transferChannels.transferee);				/* explicit release */
				return;
			}
		}

		if ((pbx_channel_owner = pbx_channel_ref(channel->owner))) {
			if (channel->state != SCCP_CHANNELSTATE_CALLTRANSFER) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CALLTRANSFER);
			}
			AUTO_RELEASE(sccp_channel_t, sccp_channel_new , sccp_channel_newcall(channel->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND, pbx_channel_owner, NULL));

			if (sccp_channel_new && (pbx_channel_bridgepeer = iPbx.get_bridged_channel(pbx_channel_owner))) {
				pbx_builtin_setvar_helper(sccp_channel_new->owner, "TRANSFEREE", pbx_channel_name(pbx_channel_bridgepeer));

				instance = sccp_device_find_index_for_line(d, sccp_channel_new->line->name);
				sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
				sccp_dev_set_keyset(d, instance, sccp_channel_new->callid, KEYMODE_OFFHOOKFEAT);
				sccp_dev_displayprompt(d, instance, sccp_channel_new->callid, SKINNY_DISP_ENTER_NUMBER, SCCP_DISPLAYSTATUS_TIMEOUT);
				sccp_device_setLamp(d, SKINNY_STIMULUS_TRANSFER, instance, SKINNY_LAMP_FLASH);

				/* set a var for BLINDTRANSFER. It will be removed if the user manually answers the call Otherwise it is a real BLINDTRANSFER */
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
				sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Cannot transfer a dialplan application, bridged channel is required on %s\n", d->id, channel->designator);
				sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONGESTION);
				sccp_channel_release(&d->transferChannels.transferee);				/* explicit release */
			} else {
				// giving up
				if (!sccp_channel_new) {
					sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: New channel could not be created to complete transfer for %s\n", d->id, channel->designator);
				} else {
					sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No bridged channel or application on %s\n", d->id, channel->designator);
				}
				sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
				channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_CONGESTION);
				sccp_channel_release(&d->transferChannels.transferee);				/* explicit release */
			}
			pbx_channel_owner = pbx_channel_unref(pbx_channel_owner);
		} else {
			// giving up
			sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No pbx channel owner to transfer %s\n", d->id, channel->designator);
			sccp_dev_displayprompt(d, instance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
			channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
			sccp_indicate(d, channel, prev_channel_state);
			sccp_channel_release(&d->transferChannels.transferee);					/* explicit release */
		}
	}
}

/*!
 * \brief Release Transfer Variables
 */
void sccp_channel_transfer_release(devicePtr d, channelPtr c)
{
	if (!d || !c) {
		return;
	}

	if ((d->transferChannels.transferee && c == d->transferChannels.transferee) || (d->transferChannels.transferer && c == d->transferChannels.transferer)) {
		if (d->transferChannels.transferee) {
			sccp_channel_release(&d->transferChannels.transferee);					/* explicit release */
		}
		if (d->transferChannels.transferer) {
			sccp_channel_release(&d->transferChannels.transferer);					/* explicit release */
		}
		sccp_log_and((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Transfer on the channel %s released\n", d->id, c->designator);
	}
	c->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
}

/*!
 * \brief Cancel Transfer
 */
void sccp_channel_transfer_cancel(devicePtr d, channelPtr c)
{
	if (!d || !c || !d->transferChannels.transferee) {
		return;
	}

	/**
	 * workaround to fix issue with 7960 and protocol version != 6
	 * 7960 loses callplane when cancel transfer (end call on other channel).
	 * This script sets the hold state for transfered channel explicitly -MC
	 */
	AUTO_RELEASE(sccp_channel_t, transferee , d->transferChannels.transferee ? sccp_channel_retain(d->transferChannels.transferee) : NULL);
	if (transferee && transferee != c) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_channel_transfer_cancel) Denied Receipt of Transferee %d %s by the Receiving Party. Cancelling Transfer and Putting transferee channel on Hold.\n", d->id, transferee->callid, transferee->line->name);
		transferee->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;
		sccp_channel_closeAllMediaTransmitAndReceive(c);
		sccp_dev_setActiveLine(d, NULL);
		sccp_indicate(d, transferee, SCCP_CHANNELSTATE_HOLD);
		sccp_channel_setDevice(transferee, NULL);
#if ASTERISK_VERSION_GROUP >= 108
		enum ast_control_transfer control_transfer_message = AST_TRANSFER_FAILED;
		iPbx.queue_control_data(c->owner, AST_CONTROL_TRANSFER, &control_transfer_message, sizeof(control_transfer_message));
#endif
		sccp_channel_transfer_release(d, transferee);			/* explicit release */
	} else {
		pbx_log(LOG_WARNING, "%s: (sccp_channel_transfer_cancel) Could not retain the transferee channel, giving up.\n", d->id);
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
void sccp_channel_transfer_complete(channelPtr sccp_destination_local_channel)
{
	PBX_CHANNEL_TYPE * pbx_source_local_channel = NULL;
	PBX_CHANNEL_TYPE * pbx_source_remote_channel = NULL;
	PBX_CHANNEL_TYPE * pbx_destination_local_channel = NULL;
	PBX_CHANNEL_TYPE * pbx_destination_remote_channel = NULL;
	boolean_t result = FALSE;
#if ASTERISK_VERSION_GROUP >= 108
	enum ast_control_transfer control_transfer_message = AST_TRANSFER_FAILED;
#endif
	uint16_t instance = 0;

	if (!sccp_destination_local_channel) {
		return;
	}
	// Obtain the device from which the transfer was initiated
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(sccp_destination_local_channel));

	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no device on channel %d\n", sccp_destination_local_channel->callid);
		return;
	}
	if (!sccp_destination_local_channel->line) {
		pbx_log(LOG_WARNING, "SCCP: weird error. The channel has no line on channel %d\n", sccp_destination_local_channel->callid);
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_NO_LINE_TO_TRANSFER, GLOB(digittimeout));
		return;
	}
	// Obtain the source channel on that device
	AUTO_RELEASE(sccp_channel_t, sccp_source_local_channel , sccp_channel_retain(d->transferChannels.transferee));

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Complete transfer from %s\n", d->id, sccp_destination_local_channel->designator);
	instance = sccp_device_find_index_for_line(d, sccp_destination_local_channel->line->name);

	if (sccp_destination_local_channel->state != SCCP_CHANNELSTATE_RINGOUT && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_CONNECTED && sccp_destination_local_channel->state != SCCP_CHANNELSTATE_PROGRESS) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected. ChannelState: %s (%d)\n", sccp_channelstate2str(sccp_destination_local_channel->state), sccp_destination_local_channel->state);
		goto EXIT;
	}

	if (!sccp_destination_local_channel->owner || !sccp_source_local_channel || !sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer error, no PBX channel for %s\n",
			d->id, 
			!sccp_destination_local_channel->owner ? sccp_destination_local_channel->designator : 
			sccp_source_local_channel ? sccp_source_local_channel->designator :
			"source_local == <null>");
		goto EXIT;
	}

	pbx_source_local_channel = sccp_source_local_channel->owner;
	pbx_source_remote_channel = iPbx.get_bridged_channel(sccp_source_local_channel->owner);
	pbx_destination_remote_channel = iPbx.get_bridged_channel(sccp_destination_local_channel->owner);
	pbx_destination_local_channel = sccp_destination_local_channel->owner;

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_local_channel       %s\n", d->id, pbx_source_local_channel ? pbx_channel_name(pbx_source_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_source_remote_channel      %s\n\n", d->id, pbx_source_remote_channel ? pbx_channel_name(pbx_source_remote_channel) : "NULL");

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_local_channel  %s\n", d->id, pbx_destination_local_channel ? pbx_channel_name(pbx_destination_local_channel) : "NULL");
	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: pbx_destination_remote_channel %s\n\n", d->id, pbx_destination_remote_channel ? pbx_channel_name(pbx_destination_remote_channel) : "NULL");

	sccp_source_local_channel->channelStateReason = SCCP_CHANNELSTATEREASON_NORMAL;

	if (!(pbx_source_remote_channel && pbx_destination_local_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");
		goto EXIT;
	}

	{
		int connectedLineUpdateReason = (sccp_destination_local_channel->state == SCCP_CHANNELSTATE_RINGOUT) ? AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER_ALERTING : AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER;

		char calling_number[StationMaxDirnumSize] = { 0 };

		char called_number[StationMaxDirnumSize] = { 0 };

		char orig_number[StationMaxDirnumSize] = { 0 };
		char calling_name[StationMaxNameSize] = { 0 };

		char called_name[StationMaxNameSize] = { 0 };

		char orig_name[StationMaxNameSize] = { 0 };

		iCallInfo.Getter(sccp_channel_getCallInfo(sccp_destination_local_channel), 
			SCCP_CALLINFO_CALLINGPARTY_NAME, &calling_name,
			SCCP_CALLINFO_CALLINGPARTY_NUMBER, &calling_number,
			SCCP_CALLINFO_CALLEDPARTY_NAME, &called_name,
			SCCP_CALLINFO_CALLEDPARTY_NUMBER, &called_number,
			SCCP_CALLINFO_KEY_SENTINEL);

		if (sccp_source_local_channel->calltype == SKINNY_CALLTYPE_INBOUND) {
			iCallInfo.Getter(sccp_channel_getCallInfo(sccp_source_local_channel), 
				SCCP_CALLINFO_CALLINGPARTY_NAME, &orig_name,
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, &orig_number,
				SCCP_CALLINFO_KEY_SENTINEL);
		} else {
			iCallInfo.Getter(sccp_channel_getCallInfo(sccp_source_local_channel), 
				SCCP_CALLINFO_CALLEDPARTY_NAME, &orig_name,
				SCCP_CALLINFO_CALLEDPARTY_NUMBER, &orig_number,
				SCCP_CALLINFO_KEY_SENTINEL);
		}

		/* update our source part */
		iCallInfo.Setter(sccp_channel_getCallInfo(sccp_source_local_channel), 
			SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, calling_name,
			SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, calling_number,
			SCCP_CALLINFO_KEY_SENTINEL);
		sccp_channel_display_callInfo(sccp_source_local_channel);

		/* update our destination part */
		iCallInfo.Setter(sccp_channel_getCallInfo(sccp_destination_local_channel), 
			SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, calling_name,
			SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, calling_number,
			SCCP_CALLINFO_KEY_SENTINEL);
		sccp_destination_local_channel->calltype = SKINNY_CALLTYPE_FORWARD;
		sccp_channel_display_callInfo(sccp_destination_local_channel);

		/* update transferee */
		iPbx.set_connected_line(sccp_source_local_channel, called_number, called_name, connectedLineUpdateReason);
#if ASTERISK_VERSION_GROUP > 106										/*! \todo change to SCCP_REASON Codes, using mapping table */
		if (iPbx.sendRedirectedUpdate) {
			iPbx.sendRedirectedUpdate(sccp_source_local_channel, calling_number, calling_name, called_number, called_name, AST_REDIRECTING_REASON_UNCONDITIONAL);
		}
#endif
		/* update ring-in channel directly */
		iPbx.set_connected_line(sccp_destination_local_channel, orig_number, orig_name, connectedLineUpdateReason);
#if ASTERISK_VERSION_GROUP > 106										/*! \todo change to SCCP_REASON Codes, using mapping table */
//		if (iPbx.sendRedirectedUpdate) {
//			iPbx.sendRedirectedUpdate(sccp_destination_local_channel, calling_number, calling_name, called_number, called_name, AST_REDIRECTING_REASON_UNCONDITIONAL);
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
			iPbx.moh_start(pbx_source_remote_channel, NULL, NULL);					//! \todo use pbx impl
		}
	}

	sccp_channel_transfer_release(d, d->transferChannels.transferee);					/* explicit release */
	if (!iPbx.attended_transfer(sccp_destination_local_channel, sccp_source_local_channel)) {
		pbx_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", pbx_channel_name(pbx_destination_local_channel), pbx_channel_name(pbx_source_remote_channel));
		goto EXIT;
	}

	if (GLOB(transfer_tone) && sccp_destination_local_channel->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* while connected not all the tones can be played */
		sccp_destination_local_channel->setTone(sccp_destination_local_channel, GLOB(autoanswer_tone), SKINNY_TONEDIRECTION_USER);
	}

#if ASTERISK_VERSION_GROUP >= 108
	control_transfer_message = AST_TRANSFER_SUCCESS;
#endif
	result = TRUE;
EXIT:
	if (pbx_source_remote_channel) {
		pbx_channel_unref(pbx_source_remote_channel);
	}
	if (pbx_destination_remote_channel) {
		pbx_channel_unref(pbx_destination_remote_channel);
	}
	if (result == FALSE) {
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, sccp_destination_local_channel->callid, SKINNY_TONEDIRECTION_USER);
		sccp_dev_displayprompt(d, instance, sccp_destination_local_channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else {
		sccp_source_local_channel->line->statistic.numberOfHeldChannels--;
	}
	if (!sccp_source_local_channel || !sccp_source_local_channel->owner) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free resources\n");
		return;
	}
#if ASTERISK_VERSION_GROUP >= 108
	iPbx.queue_control_data(sccp_source_local_channel->owner, AST_CONTROL_TRANSFER, &control_transfer_message, sizeof(control_transfer_message));
#endif
}

/*!
 * \brief Set Caller Id Presentation
 * \param channel SCCP Channel
 * \param presentation SCCP CallerID Presentation ENUM
 */
void sccp_channel_set_calleridPresentation(constChannelPtr channel, sccp_callerid_presentation_t presentation)
{
	iCallInfo.Setter(channel->privateData->callInfo, SCCP_CALLINFO_PRESENTATION, presentation, SCCP_CALLINFO_KEY_SENTINEL);
	if (iPbx.set_callerid_presentation) {
		iPbx.set_callerid_presentation(channel->owner, presentation);
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
int sccp_channel_forward(constChannelPtr sccp_channel_parent, constLineDevicePtr ld, const char * fwdNumber)
{
	char dialedNumber[256];
	if (!sccp_channel_parent) {
		pbx_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return -1;
	}

	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));
	AUTO_RELEASE(sccp_channel_t, sccp_forwarding_channel , sccp_channel_allocate(sccp_channel_parent->line, NULL));

	if (!sccp_forwarding_channel) {
		pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", ld->device->id);
		return -1;
	}
	sccp_forwarding_channel->parentChannel = sccp_channel_retain(sccp_channel_parent);
	sccp_forwarding_channel->softswitch_action = SCCP_SOFTSWITCH_DIAL;					/* softswitch will catch the number to be dialed */
	sccp_forwarding_channel->ss_data = 0;									// nothing to pass to action
	sccp_forwarding_channel->calltype = SKINNY_CALLTYPE_FORWARD;

	char calling_name[StationMaxNameSize] = {0};
	char calling_num[StationMaxDirnumSize] = {0};
	char called_name[StationMaxNameSize] = {0};
	char called_num[StationMaxDirnumSize] = {0};
	iCallInfo.Getter(sccp_channel_getCallInfo(sccp_channel_parent), 
		SCCP_CALLINFO_CALLINGPARTY_NAME, &calling_name,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &calling_num,
		SCCP_CALLINFO_CALLEDPARTY_NAME, &called_name,
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &called_num,
		SCCP_CALLINFO_KEY_SENTINEL);

	/* copy the number to dial in the ast->exten */
	sccp_copy_string(sccp_forwarding_channel->dialedNumber, dialedNumber, sizeof(sccp_forwarding_channel->dialedNumber));
	sccp_log((DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "Incoming: %s (%s) Forwarded By: %s (%s) Forwarded To: %s\n", calling_name, calling_num, ld->line->cid_name, ld->line->cid_num, dialedNumber);

	/* Copy Channel Capabilities From Predecessor */
	memset(&sccp_forwarding_channel->remoteCapabilities.audio, 0, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memcpy(&sccp_forwarding_channel->remoteCapabilities.audio, sccp_channel_parent->remoteCapabilities.audio, sizeof(sccp_forwarding_channel->remoteCapabilities.audio));
	memset(&sccp_forwarding_channel->preferences.audio, 0, sizeof(sccp_forwarding_channel->preferences.audio));
	memcpy(&sccp_forwarding_channel->preferences.audio, sccp_channel_parent->preferences.audio, sizeof(sccp_channel_parent->preferences.audio));

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(sccp_forwarding_channel, NULL, sccp_channel_parent->owner))
	{
		return -1;
	}
	/* Update rtp setting to match predecessor */
	skinny_codec_t codecs[] = { SKINNY_CODEC_WIDEBAND_256K, SKINNY_CODEC_NONE };
	iPbx.set_nativeAudioFormats(sccp_forwarding_channel, codecs);
	iPbx.rtp_setWriteFormat(sccp_forwarding_channel, SKINNY_CODEC_WIDEBAND_256K);
	iPbx.rtp_setReadFormat(sccp_forwarding_channel, SKINNY_CODEC_WIDEBAND_256K);
	sccp_channel_updateChannelCapability(sccp_forwarding_channel);

	char newcalling_name[StationMaxNameSize] = {0};
	char newcalling_num[StationMaxDirnumSize] = {0};
	iCallInfo.Getter(sccp_channel_getCallInfo(sccp_forwarding_channel),
		SCCP_CALLINFO_CALLINGPARTY_NAME, &newcalling_name,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &newcalling_num,
		SCCP_CALLINFO_KEY_SENTINEL);

	iCallInfo.Setter(sccp_channel_getCallInfo(sccp_forwarding_channel),
		SCCP_CALLINFO_CALLINGPARTY_NAME, &calling_name,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &calling_num,
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &dialedNumber,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &newcalling_name,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &newcalling_num,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, 4,
		SCCP_CALLINFO_KEY_SENTINEL);

	/* setting callerid */
	if (iPbx.set_callerid_number) {
		iPbx.set_callerid_number(sccp_forwarding_channel->owner, calling_num);
	}

	if (iPbx.set_callerid_name) {
		iPbx.set_callerid_name(sccp_forwarding_channel->owner, calling_name);
	}

	if (iPbx.set_callerid_ani) {
		iPbx.set_callerid_ani(sccp_forwarding_channel->owner, dialedNumber);
	}

	if (iPbx.set_callerid_dnid) {
		iPbx.set_callerid_dnid(sccp_forwarding_channel->owner, dialedNumber);
	}

	if (iPbx.set_callerid_redirectedParty) {
		iPbx.set_callerid_redirectedParty(sccp_forwarding_channel->owner, called_num, called_name);
	}

	if (iPbx.set_callerid_redirectingParty) {
		iPbx.set_callerid_redirectingParty(sccp_forwarding_channel->owner, newcalling_num, newcalling_name);
	}

	/* dial sccp_forwarding_channel */
	iPbx.setChannelExten(sccp_forwarding_channel, dialedNumber);

	/* \todo copy device line setvar variables from parent channel to forwarder->owner */
	iPbx.set_callstate(sccp_forwarding_channel, AST_STATE_OFFHOOK);
	if (!sccp_strlen_zero(dialedNumber)
	    && iPbx.checkhangup(sccp_forwarding_channel)
	    && pbx_exists_extension(sccp_forwarding_channel->owner, sccp_forwarding_channel->line->context ? sccp_forwarding_channel->line->context : "", dialedNumber, 1, sccp_forwarding_channel->line->cid_num)) {
		/* found an extension, let's dial it */
		pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s is dialing number %s\n", sccp_forwarding_channel->currentDeviceId, sccp_forwarding_channel->designator, dialedNumber);
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		iPbx.set_callstate(sccp_forwarding_channel, AST_STATE_RING);

		pbx_channel_call_forward_set(sccp_forwarding_channel->owner, dialedNumber);
#if CS_AST_CONTROL_REDIRECTING
		iPbx.queue_control(sccp_forwarding_channel->owner, AST_CONTROL_REDIRECTING);
#endif
		if (pbx_pbx_start(sccp_forwarding_channel->owner)) {
			pbx_log(LOG_WARNING, "%s: invalid number\n", "SCCP");
		}
		return 0;
	} 
	pbx_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s cannot dial this number %s\n", sccp_forwarding_channel->currentDeviceId, sccp_forwarding_channel->designator, dialedNumber);
	sccp_channel_release(&sccp_forwarding_channel->parentChannel);						/* explicit release */
	sccp_channel_endcall(sccp_forwarding_channel);
	return -1;
}

#ifdef CS_SCCP_PARK
/*!
 * \brief Park an SCCP Channel
 * \param channel SCCP Channel
 */
void sccp_channel_park(constChannelPtr channel)
{
	sccp_parkresult_t result = 0;

	if (!iPbx.feature_park) {
		pbx_log(LOG_WARNING, "SCCP, parking feature not implemented\n");
		return;
	}

	/* let the pbx implementation do the rest */
	result = iPbx.feature_park(channel);
	if (PARK_RESULT_SUCCESS != result) {
		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
		if (d) {
			sccp_dev_displaynotify(d, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 10);
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
boolean_t sccp_channel_setPreferredCodec(channelPtr c, const char * data)
{
	if (!data || !c) {
		return FALSE;
	}

	skinny_codec_t new_codecs[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE};
	skinny_codec_t audio_prefs[ARRAY_LEN(c->preferences.audio)] = {SKINNY_CODEC_NONE};
	skinny_codec_t video_prefs[ARRAY_LEN(c->preferences.video)] = {SKINNY_CODEC_NONE};

	char text[64] = { '\0' };
	sccp_copy_string(text, data, sizeof(text));
	sccp_codec_parseAllowDisallow(new_codecs, text, 1 /*allow*/);
	if (new_codecs[0] != SKINNY_CODEC_NONE) {
		sccp_get_codecs_bytype(new_codecs, audio_prefs, SKINNY_CODEC_TYPE_AUDIO);
		sccp_get_codecs_bytype(new_codecs, video_prefs, SKINNY_CODEC_TYPE_VIDEO);
	}
	if (audio_prefs[0] != SKINNY_CODEC_NONE) {
		memcpy(c->preferences.audio, audio_prefs, sizeof c->preferences.audio);
	}
	if (video_prefs[0] != SKINNY_CODEC_NONE) {
		memcpy(c->preferences.video, video_prefs, sizeof c->preferences.video);
	}

	if(c->line) {
		c->line->preferences_set_on_line_level = TRUE;
	}

	sccp_channel_updateChannelCapability(c);

	return TRUE;
}

sccp_video_mode_t sccp_channel_getVideoMode(constChannelPtr c)
{
#if CS_SCCP_VIDEO
	// sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "%s: (getVideoMode) current video mode:%s\n", c->designator, sccp_video_mode2str(c->videomode));
	return c->videomode;
#else
	return FALSE;
#endif
}

boolean_t sccp_channel_setVideoMode(channelPtr c, const char *data)
{
	boolean_t res = FALSE;
#if CS_SCCP_VIDEO
	if (c) {
		sccp_video_mode_t newval = c->videomode = sccp_video_mode_str2val(data);
		if (newval == SCCP_VIDEO_MODE_SENTINEL) {
			return res;
		}
		sccp_rtp_t * video = (sccp_rtp_t *)&(c->rtp.video);
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP))(VERBOSE_PREFIX_2 "%s: (setVideoMode) Setting Video Mode to %s\n", c->designator, sccp_video_mode2str(newval));
		if (c->state >= SCCP_GROUPED_CHANNELSTATE_SETUP && newval == SCCP_VIDEO_MODE_AUTO && !c->isHangingUp) {
			if(!c->rtp.video.instance || !sccp_rtp_getState(video, SCCP_RTP_RECEPTION)) {
				sccp_channel_openMultiMediaReceiveChannel(c);
			}
			if((sccp_rtp_getState(video, SCCP_RTP_RECEPTION) & SCCP_RTP_STATUS_ACTIVE) && !sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION)) {
				sccp_channel_startMultiMediaTransmission(c);
			}
		}
		if (newval == SCCP_VIDEO_MODE_OFF) {
			if (c->rtp.video.instance) {
				if(sccp_rtp_getState(video, SCCP_RTP_RECEPTION)) {
					sccp_channel_closeMultiMediaReceiveChannel(c, TRUE);
				}
				if(sccp_rtp_getState(video, SCCP_RTP_TRANSMISSION)) {
					sccp_channel_stopMultiMediaTransmission(c, TRUE);
				}
				if(video->instance && video->instance_active) {
					iPbx.rtp_stop(video->instance);
					video->instance_active = FALSE;
				}
			}
		}
		c->videomode = newval;
		if (c->owner) {
			pbx_builtin_setvar_helper(c->owner, "_SCCP_VIDEO_MODE", sccp_video_mode2str(c->videomode));
		}
		return TRUE;
	}
#endif
	{
		return res;
	}
}

/*!
 * \brief Send callwaiting tone to device multiple times
 */
int sccp_channel_callwaiting_tone_interval(constDevicePtr device, constChannelPtr channel)
{
	if (GLOB(callwaiting_tone)) {
		AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(device));

		if (d) {
			AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(channel));

			if (c) {
				pbx_assert(c->line != NULL);
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Handle Callwaiting Tone on channel %d\n", c->callid);
				if (c && c->owner && (SCCP_CHANNELSTATE_CALLWAITING == c->state || SCCP_CHANNELSTATE_RINGING == c->state)) {
					sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending Call Waiting Tone \n", d->id);
					c->setTone(c, GLOB(callwaiting_tone), SKINNY_TONEDIRECTION_USER);
					return 0;
				} 
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) channel has been hungup or pickuped up by another phone\n");
				return -1;
			}
		}
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No valid device/channel to handle callwaiting\n");
	} else {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_callwaiting_tone_interval) No callwaiting_tone set\n");
	}
	return -1;
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
channelPtr sccp_find_channel_on_line_byid(constLinePtr l, uint32_t id)
{
	sccp_channel_t *c = NULL;

	//sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel on line by id %u\n", id);

	SCCP_LIST_LOCK(&(((linePtr)l)->channels));
	c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->callid == id && tmpc->state != SCCP_CHANNELSTATE_DOWN), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_LIST_UNLOCK(&(((linePtr)l)->channels));
	return c;
}

/*!
 * Find channel by lineId and CallId, connected to a particular device;
 * \return *refcounted* SCCP Channel (can be null)
 */
channelPtr sccp_find_channel_by_buttonIndex_and_callid(constDevicePtr d, const uint32_t buttonIndex, const uint32_t callid)
{
	sccp_channel_t *c = NULL;

	if (!d || !buttonIndex || !callid) {
		return NULL;
	}

	AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byButtonIndex((sccp_device_t *) d, buttonIndex));
	if (l) {
		SCCP_LIST_LOCK(&l->channels);
		c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->callid == callid), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		SCCP_LIST_UNLOCK(&l->channels);
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find channel for lineInstance:%u and callid:%d on device\n", d->id, buttonIndex, callid);
	}
	return c;
}

/*!
 * Find channel by lineInstance and CallId, connected to a particular device;
 * \return *refcounted* SCCP Channel (can be null)
 */
channelPtr sccp_find_channel_by_lineInstance_and_callid(const sccp_device_t * d, const uint32_t lineInstance, const uint32_t callid)
{
	sccp_channel_t *c = NULL;

	if (!d || !lineInstance || !callid) {
		return NULL;
	}

	AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byid((sccp_device_t *) d, lineInstance));

	if (l) {
		SCCP_LIST_LOCK(&l->channels);
		c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->callid == callid), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		SCCP_LIST_UNLOCK(&l->channels);
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find channel for lineInstance:%u and callid:%d on device\n", d->id, lineInstance, callid);
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
channelPtr sccp_channel_find_byid(uint32_t callid)
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
channelPtr sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid)
{
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u\n", passthrupartyid);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->passthrupartyid == passthrupartyid && tmpc->state != SCCP_CHANNELSTATE_DOWN), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
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
channelPtr sccp_channel_find_on_device_bypassthrupartyid(constDevicePtr d, uint32_t passthrupartyid)
{
	sccp_channel_t *c = NULL;

	if (!d) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "SCCP: No device provided to look for %u\n", passthrupartyid);
		return NULL;
	}
	uint8_t instance = 0;

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel on device by PassThruId %u on device %s\n", passthrupartyid, d->id);
	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(d->lineButtons.instance[instance]->line));

			if (l) {
				sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_RTP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Found line: '%s'\n", d->id, l->name);
				SCCP_LIST_LOCK(&l->channels);
				c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->passthrupartyid == passthrupartyid), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
				SCCP_LIST_UNLOCK(&l->channels);

				if (c) {
					break;
				}
			}
		}
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find active channel with Passthrupartyid %u on device\n", d->id, passthrupartyid);
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
channelPtr sccp_channel_find_bystate_on_line(constLinePtr l, sccp_channelstate_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_LIST_LOCK(&(((linePtr)l)->channels));
	c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->state == state), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_LIST_UNLOCK(&(((linePtr)l)->channels));

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
channelPtr sccp_channel_find_bystate_on_device(constDevicePtr device, sccp_channelstate_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d' on device: %s\n", state, device->id);

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(device));

	if (!d) {
		return NULL;
	}
	uint8_t instance = 0;

	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(d->lineButtons.instance[instance]->line));

			if (l) {
				sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_CHANNEL + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: line: '%s'\n", d->id, l->name);
				SCCP_LIST_LOCK(&l->channels);
				c = SCCP_LIST_FIND(&l->channels, sccp_channel_t, tmpc, list, (tmpc->state == state && sccp_util_matchSubscriptionId(tmpc, d->lineButtons.instance[instance]->subscriptionId.number)), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
				SCCP_LIST_UNLOCK(&l->channels);
				if (c) {
					break;
				}
			}
		}
	}
	if (!c) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Could not find active channel with state %s(%u) on device\n", d->id, sccp_channelstate2str(state), state);
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
sccp_selectedchannel_t *sccp_device_find_selectedchannel(constDevicePtr d, constChannelPtr channel)
{
	if (!d) {
		return NULL;
	}
	sccp_selectedchannel_t *sc = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channel (%d)\n", d->id, channel->callid);

	SCCP_LIST_LOCK(&(((devicePtr)d)->selectedChannels));
	sc = SCCP_LIST_FIND(&d->selectedChannels, sccp_selectedchannel_t, tmpsc, list, (tmpsc->channel == channel), FALSE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_LIST_UNLOCK(&(((devicePtr)d)->selectedChannels));
	return sc;
}

/*!
 * \brief Count Selected Channel on Device
 * \param device SCCP Device
 * \return count Number of Selected Channels
 * 
 */
uint8_t sccp_device_selectedchannels_count(constDevicePtr device)
{
	uint8_t count = 0;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channels count\n", device->id);
	SCCP_LIST_LOCK(&(((devicePtr)device)->selectedChannels));
	count = SCCP_LIST_GETSIZE(&device->selectedChannels);
	SCCP_LIST_UNLOCK(&(((devicePtr)device)->selectedChannels));

	return count;
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
