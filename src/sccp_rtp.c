/*!
 * \file        sccp_rtp.c
 * \brief       SCCP RTP Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_rtp.h"
#include "sccp_socket.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");

/* 
 * we should use the new sccp_rtp_type enum to specify audio/video/text variety of functions below
 */
struct sccp_rtp_new {
	sccp_mutex_t		lock;						/* This lock should only be used to get / set local variables, not function calls outside this class are allowed when holding this lock */
	const sccp_channel_t 	*channel;
	PBX_RTP_TYPE		*pbxrtp;
	sccp_rtp_type_t		type;
	uint16_t		readState;
	uint16_t		writeState;
	boolean_t		directMedia;
	skinny_codec_t		readFormat;
	skinny_codec_t		writeFormat;
	struct sockaddr_storage	phone;
	struct sockaddr_storage	phone_remote;
};

/*!
 * \brief create a new rtp server for audio data
 * \param c SCCP Channel
 */
int sccp_rtp_createAudioServer(constChannelPtr c)
{
	boolean_t rtpResult = FALSE;
	boolean_t isMappedIPv4;

	if (!c) {
		return FALSE;
	}
	if (c->rtp.audio.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "we already have a rtp server, we use this one\n");
		return TRUE;
	}
	sccp_rtp_t *audio = (sccp_rtp_t *) &(c->rtp.audio);
	struct sockaddr_storage *phone_remote = &audio->phone_remote;

	if (iPbx.rtp_audio_create) {
		rtpResult = (boolean_t) iPbx.rtp_audio_create((sccp_channel_t *) c);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
		return FALSE;
	}

	if (!sccp_rtp_getUs(audio, phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
		return FALSE;
	}

	uint16_t port = sccp_rtp_getServerPort(&c->rtp.audio);
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createAudioServer) RTP Server Port: %d\n", c->currentDeviceId, port);

	/* depending on the client connection, we us ipv4 or ipv6 */
	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	if (device) {
		sccp_session_getOurIP(device->session, phone_remote, 0);
		sccp_socket_setPort(phone_remote, port);

		char buf[NI_MAXHOST + NI_MAXSERV];
		sccp_copy_string(buf, sccp_socket_stringify(phone_remote), sizeof(buf));
		isMappedIPv4 = sccp_socket_ipv4_mapped(phone_remote, (struct sockaddr_storage *) phone_remote);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createAudioServer) updated remote phone ip to : %s, family:%s, mapped: %s\n", device->id, buf, sccp_socket_is_IPv4(phone_remote) ? "IPv4" : "IPv6", isMappedIPv4 ? "True" : "False");
	}
	//struct sockaddr_in us;
	//iPbx.rtp_setPeer(&c->rtp.audio, &c->rtp.audio.phone, device ? device->nat : 0);

	return rtpResult;
}

/*!
 * \brief create a new rtp server for video data
 * \param c SCCP Channel
 */
int sccp_rtp_createVideoServer(constChannelPtr c)
{
	boolean_t rtpResult = FALSE;

	if (!c) {
		return FALSE;
	}
	if (c->rtp.video.rtp) {
		pbx_log(LOG_ERROR, "we already have a rtp server, why dont we use this?\n");
		return TRUE;
	}

	if (iPbx.rtp_video_create) {
		rtpResult = (boolean_t) iPbx.rtp_video_create((sccp_channel_t *) c);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
	}

	if (!sccp_rtp_getUs(&c->rtp.video, &((sccp_channel_t *) c)->rtp.video.phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
	}

	return rtpResult;
}

/*!
 * \brief Stop an RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_stop(constChannelPtr channel)
{
	if (!channel) {
		return;
	}
	if (iPbx.rtp_stop) {
		if (channel->rtp.audio.rtp) {
			PBX_RTP_TYPE *rtp = (PBX_RTP_TYPE *) channel->rtp.audio.rtp;		/* discard const */
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "%s: Stopping PBX audio rtp transmission on channel %08X\n", channel->currentDeviceId, channel->callid);
			iPbx.rtp_stop(rtp);
		}
		if (channel->rtp.video.rtp) {
			PBX_RTP_TYPE *rtp = (PBX_RTP_TYPE *) channel->rtp.video.rtp;		/* discard const */
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "%s: Stopping PBX video rtp transmission on channel %08X\n", channel->currentDeviceId, channel->callid);
			iPbx.rtp_stop(rtp);
		}
	} else {
		pbx_log(LOG_ERROR, "no pbx function to stop rtp\n");
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_destroy(constChannelPtr c)
{
	sccp_line_t *l = c->line;
	
	sccp_rtp_t *audio = (sccp_rtp_t *) &(c->rtp.audio);
	sccp_rtp_t *video = (sccp_rtp_t *) &(c->rtp.video);

	if (audio->rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX rtp server on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		iPbx.rtp_destroy(audio->rtp);
		audio->rtp = NULL;
	}

	if (video->rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX vrtp server on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		iPbx.rtp_destroy(video->rtp);
		video->rtp = NULL;
	}
}

/*!
 * \brief set the address the phone should send rtp media.
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_peer(constChannelPtr c, sccp_rtp_t * const rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_socket_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: ( sccp_rtp_set_peer ) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	/* check if we have new infos */
	if (socket_equals(new_peer, &rtp->phone_remote)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) remote information are equal to the current one, ignore change\n", c->currentDeviceId);
		return;
	}
	//memcpy(&c->rtp.audio.phone_remote, new_peer, sizeof(c->rtp.audio.phone_remote));
	memcpy(&rtp->phone_remote, new_peer, sizeof(rtp->phone_remote));

	// inet_ntop(rtp->phone_remote.ss_family, sccp_socket_getAddr(&rtp->phone_remote), addressString, sizeof(addressString));
	// pbx_log(LOG_NOTICE, "%s: ( sccp_rtp_set_peer ) Set remote address to %s:%d\n", c->currentDeviceId, addressString, sccp_socket_getPort(new_peer));
	pbx_log(LOG_NOTICE, "%s: ( sccp_rtp_set_peer ) Set remote address to %s\n", c->currentDeviceId, sccp_socket_stringify(&rtp->phone_remote));

	if (rtp->readState & SCCP_RTP_STATUS_ACTIVE) {
		/* Shutdown any early-media or previous media on re-invite */
		/*! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) Stop media transmission on channel %d\n", c->currentDeviceId, c->callid);

		/*! \todo we should check if this is a video or autio rtp */
		// sccp_channel_stopmediatransmission(c);
		// sccp_channel_startMediaTransmission(c);
		sccp_channel_updateMediaTransmission(c);
	}

}

/*!
 * \brief set the address where the phone should send rtp media.
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_phone(constChannelPtr c, sccp_rtp_t * const rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_socket_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_phone) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	if (device) {

		/* check if we have new infos */
		/*! \todo if we enable this, we get an audio issue when resume on the same device, so we need to force asterisk to update -MC */
		/*
		if (socket_equals(new_peer, &c->rtp.audio.phone)) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_phone) remote information are equal to the current one, ignore change\n", c->currentDeviceId);
			//return;
		} 
		*/

		memcpy(&rtp->phone, new_peer, sizeof(rtp->phone));

		//update pbx
		if (iPbx.rtp_setPhoneAddress) {
			iPbx.rtp_setPhoneAddress(rtp, new_peer, device->nat >= SCCP_NAT_ON ? 1 : 0);
		}

		char buf1[NI_MAXHOST + NI_MAXSERV];

		sccp_copy_string(buf1, sccp_socket_stringify(&rtp->phone_remote), sizeof(buf1));
		char buf2[NI_MAXHOST + NI_MAXSERV];

		sccp_copy_string(buf2, sccp_socket_stringify(&rtp->phone), sizeof(buf2));
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell PBX   to send RTP/UDP media from %s to %s (NAT: %s)\n", DEV_ID_LOG(device), buf1, buf2, sccp_nat2str(device->nat));
	}
}

int sccp_rtp_updateNatRemotePhone(constChannelPtr c, sccp_rtp_t *const rtp)
{
	int res = 0;
	//sccp_rtp_t *audio = (sccp_rtp_t *) &(channel->rtp.audio);
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);
	if (d) {
		struct sockaddr_storage sus = { 0 };
		sccp_session_getOurIP(d->session, &sus, 0);
		uint16_t usFamily = sccp_socket_is_IPv6(&sus) ? AF_INET6 : AF_INET;
		//sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) us: %s, usFamily: %s\n", d->id, sccp_socket_stringify(&sus), (usFamily == AF_INET6) ? "IPv6" : "IPv4");

		struct sockaddr_storage *phone_remote = &rtp->phone_remote;
		uint16_t remoteFamily = (rtp->phone_remote.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(phone_remote)) ? AF_INET6 : AF_INET;
		//sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) remote: %s, remoteFamily: %s\n", d->id, sccp_socket_stringify(phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");

		/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
		if (d->nat >= SCCP_NAT_ON) {
			if ((usFamily == AF_INET) != remoteFamily) {						/* device needs correction for ipv6 address in remote */
				uint16_t port = sccp_rtp_getServerPort(rtp);					/* get rtp server port */

				memcpy(phone_remote, &sus, sizeof(struct sockaddr_storage));			/* Not sure if this should not be the externip in case of nat */
				sccp_socket_ipv4_mapped(phone_remote, phone_remote);				/*!< we need this to convert mapped IPv4 to real IPv4 address */
				sccp_socket_setPort(phone_remote, port);

			} else if ((usFamily == AF_INET6) != remoteFamily) {					/* the device can do IPv6 but should send it to IPv4 address (directrtp possible) */
				struct sockaddr_storage sas;

				memcpy(&sas, phone_remote, sizeof(struct sockaddr_storage));
				sccp_socket_ipv4_mapped(&sas, &sas);
			}
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) new remote: %s, new remoteFamily: %s\n", d->id, sccp_socket_stringify(phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");
			res = 1;
		}
		
		char buf1[NI_MAXHOST + NI_MAXSERV];
		char buf2[NI_MAXHOST + NI_MAXSERV];

		sccp_copy_string(buf1, sccp_socket_stringify(&rtp->phone), sizeof(buf1));
		sccp_copy_string(buf2, sccp_socket_stringify(phone_remote), sizeof(buf2));

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell Phone to send RTP/UDP media from %s to %s (NAT: %s)\n", DEV_ID_LOG(d), buf1, buf2, sccp_nat2str(d->nat));
	}
	return res;
}


/*!
 * \brief Get Audio Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(constChannelPtr c, sccp_rtp_t **rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.audio);

	result = SCCP_RTP_INFO_AVAILABLE;
	// \todo add apply_ha(d->ha, &sin) check here instead
	if (device->directrtp && device->nat == SCCP_NAT_OFF && !c->conference) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}
	return result;
}

/*!
 * \brief Get Video Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(constChannelPtr c, sccp_rtp_t ** rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.video);

	result = SCCP_RTP_INFO_AVAILABLE;
	if (device->directrtp && device->nat == SCCP_NAT_OFF && !c->conference) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}
	return result;
}

/*!
 * \brief Get Audio Peer
 */
boolean_t sccp_rtp_getAudioPeer(constChannelPtr c, struct sockaddr_storage **new_peer)
{
	sccp_rtp_t *audio = (sccp_rtp_t *) &(c->rtp.audio);
	*new_peer = &audio->phone_remote;
	return TRUE;
}

/*!
 * \brief Get Video Peer
 */
boolean_t sccp_rtp_getVideoPeer(constChannelPtr c, struct sockaddr_storage **new_peer)
{
	sccp_rtp_t *video = (sccp_rtp_t *) &(c->rtp.video);
	*new_peer = &video->phone_remote;
	return TRUE;
}


/*!
 * \brief Get Payload Type
 */
uint8_t sccp_rtp_get_payloadType(const sccp_rtp_t * const rtp, skinny_codec_t codec)
{
	if (iPbx.rtp_get_payloadType) {
		return iPbx.rtp_get_payloadType(rtp, codec);
	} else {
		return 97;
	}
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getUs(const sccp_rtp_t *rtp, struct sockaddr_storage *us)
{
	if (rtp->rtp) {
		iPbx.rtp_getUs(rtp->rtp, us);
		return TRUE;
	} else {
		// us = &rtp->phone_remote;
		return FALSE;
	}
}

uint16_t sccp_rtp_getServerPort(const sccp_rtp_t * const rtp)
{
	uint16_t port = 0;
	struct sockaddr_storage sas;

	sccp_rtp_getUs(rtp, &sas);

	port = sccp_socket_getPort(&sas);
	return port;
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getPeer(const sccp_rtp_t * const rtp, struct sockaddr_storage *them)
{
	if (rtp->rtp) {
		iPbx.rtp_getPeer(rtp->rtp, them);
		return TRUE;
	} else {
		return FALSE;
	}
}

/*!
 * \brief Get Sample Rate
 */
int sccp_rtp_get_sampleRate(skinny_codec_t codec)
{
	if (iPbx.rtp_get_sampleRate) {
		return iPbx.rtp_get_sampleRate(codec);
	} else {
		return 3840;
	}
}

/* new : allowing to internalize sccp_rtp struct */
#define sccp_rtp_lock(x) sccp_mutex_lock(&((sccp_rtp_t * const)x)->lock)				/* discard const */
#define sccp_rtp_unlock(x) sccp_mutex_unlock(&((sccp_rtp_t * const)x)->lock)				/* discard const */

sccp_rtp_new_t __attribute__ ((malloc)) *const sccp_rtp_ctor(constChannelPtr channel, sccp_rtp_type_t type)
{
	assert(channel != NULL);
	
	sccp_rtp_new_t *const rtp = sccp_calloc(sizeof(sccp_rtp_new_t), 1);

	if (!rtp) {
		pbx_log(LOG_ERROR, "SCCP: No memory to allocate rtp object. Failing\n");
		return NULL;
	}
	sccp_mutex_init(&rtp->lock);

	// set defaults
	if (channel) {
		rtp->channel = sccp_channel_retain(channel);
	}
	if (SCCP_RTP_TYPE_SENTINEL != type) {						// skip createServer
		if (!sccp_rtp_createServer(rtp)) {
			return sccp_rtp_dtor(rtp);					// return NULL
		}
	}
	rtp->type = type;
	
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_1 "SCCP: rtp constructor: %p\n", rtp);
	return rtp;
}

sccp_rtp_new_t *const sccp_rtp_dtor(sccp_rtp_new_t *rtp)
{
	assert(rtp != NULL && rtp->channel != NULL);
	if (rtp->pbxrtp) {
		sccp_rtp_destroyServer(rtp);
	}

	sccp_rtp_lock(rtp);
	const sccp_channel_t *const channel = rtp->channel;
	sccp_mutex_destroy(&rtp->lock);
	sccp_rtp_unlock(rtp);
	sccp_free(rtp);

	sccp_channel_release(channel);
	
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_2 "SCCP: rtp destructor\n");
	return NULL;
}

boolean_t sccp_rtp_createServer(sccp_rtp_new_t *const rtp)
{
	assert(rtp != NULL && rtp->channel != NULL);
	
	if (rtp->pbxrtp) {
		pbx_log(LOG_ERROR, "%s: we already have a %s rtp server, we should this that one one\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return TRUE;
	}
	if (rtp->readState || rtp->writeState) {
		pbx_log(LOG_ERROR, "%s: %s rtp->state seems to be active already, cancelling create.\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return FALSE;
	}

	PBX_RTP_TYPE *pbxrtp = iPbx.rtp_createServer ? iPbx.rtp_createServer(rtp->channel, rtp->type) : NULL;
	if (!pbxrtp) {
		pbx_log(LOG_ERROR, "%s: we should start our own %s rtp server, but we dont have one\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return FALSE;
	}

	struct sockaddr_storage phone_remote = {0};	
	if (!sccp_rtp_getOurSas(rtp, &phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Was not able to retrieve ourip for %s rtp\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		pbxrtp = iPbx.rtp_destroy ? iPbx.rtp_destroyServer(pbxrtp) : NULL;
		return FALSE;
	}
	uint16_t port = sccp_socket_getPort(&phone_remote);
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createServer) RTP Server Port: %d\n", rtp->channel->designator, port);

	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(rtp->channel);
	if (device) {
		sccp_session_getOurIP(device->session, &phone_remote, 0);
		sccp_socket_setPort(&phone_remote, port);

		char buf[NI_MAXHOST + NI_MAXSERV];
		sccp_copy_string(buf, sccp_socket_stringify(&phone_remote), sizeof(buf));
		boolean_t isIPv4 = sccp_socket_is_IPv4(&phone_remote);
		boolean_t isMappedIPv4 = sccp_socket_ipv4_mapped(&phone_remote, &phone_remote);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createAudioServer) updated remote phone ip to : %s, family:%s, mapped: %s\n", rtp->channel->designator, buf, isIPv4 ? "IPv4" : "IPv6", isMappedIPv4 ? "True" : "False");
	}

	sccp_rtp_lock(rtp);
	rtp->pbxrtp = pbxrtp;
	memcpy(&rtp->phone_remote, &phone_remote, sizeof(struct sockaddr_storage));
	sccp_rtp_unlock(rtp);

	return TRUE;
}

boolean_t sccp_rtp_new_stop(sccp_rtp_new_t *const rtp)
{
	assert(rtp != NULL);
	if (!rtp->pbxrtp) {
		pbx_log(LOG_ERROR, "%s: we don't have a %s rtp server\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return FALSE;
	}
	if (!iPbx.rtp_stop) {
		pbx_log(LOG_ERROR, "no pbx function connected to pbx_impl to stop rtp\n");
		return FALSE;
	}
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "%s: Stopping PBX audio rtp transmission\n", rtp->channel->designator);
	#warning FIX
	//return iPbx.rtp_stop(rtp->pbxrtp);
	iPbx.rtp_stop(rtp->pbxrtp);
	return TRUE;
}

boolean_t sccp_rtp_destroyServer(sccp_rtp_new_t *const rtp)
{
	assert(rtp != NULL);

	if (!rtp->pbxrtp) {
		pbx_log(LOG_ERROR, "%s: we don't have a %s rtp server, nothing to destroy\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return TRUE;
	}
	if (rtp->readState || rtp->writeState) {
		pbx_log(LOG_ERROR, "%s: %s rtp->state seems to be active, cancelling destroy.\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
		return FALSE;
	}
	
	sccp_rtp_lock(rtp);
	PBX_RTP_TYPE *pbxrtp = rtp->pbxrtp;
	sccp_rtp_unlock(rtp);

	if (pbxrtp) {	
		iPbx.rtp_stop(pbxrtp);
		pbxrtp = iPbx.rtp_destroyServer ? iPbx.rtp_destroyServer(pbxrtp) : NULL;
		
		if (!pbxrtp) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroyed PBX %s rtp server\n", rtp->channel->designator, sccp_rtp_type2str(rtp->type));
			
			sccp_rtp_lock(rtp);
			rtp->pbxrtp = NULL;
			sccp_rtp_unlock(rtp);
		}
	}
	
	return TRUE;
}

boolean_t sccp_rtp_getOurSas(const sccp_rtp_new_t * const rtp, struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);
	
	if (rtp->pbxrtp) {
		// get locked
		sccp_rtp_lock(rtp);
		PBX_RTP_TYPE *pbxrtp = rtp->pbxrtp;
		sccp_rtp_unlock(rtp);

		return iPbx.rtp_getUs(pbxrtp, sas);
	}
	return FALSE;
}

boolean_t sccp_rtp_getPeerSas(const sccp_rtp_new_t *const rtp, struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);

	if (rtp->pbxrtp) {
		// get locked
		sccp_rtp_lock(rtp);
		PBX_RTP_TYPE *pbxrtp = rtp->pbxrtp;
		sccp_rtp_unlock(rtp);
		
		return iPbx.rtp_getPeer(pbxrtp, sas);
	}
	return FALSE;
}

boolean_t sccp_rtp_setPeer(sccp_rtp_new_t *const rtp, const struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);

	// validate socket
	if (sccp_socket_getPort(sas) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: ( sccp_rtp_set_peer ) new information is invalid, dont change anything\n", rtp->channel->designator);
		return FALSE;
	}

	// check if anything has changed, skip otherwise
	if (socket_equals(sas, &rtp->phone_remote)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_setPeer) remote information are equal to the current one, ignore change\n", rtp->channel->designator);
		return FALSE;
	}

	// get / set locked
	sccp_rtp_lock(rtp);
	memcpy(&rtp->phone_remote, sas, sizeof(struct sockaddr_storage));
	pbx_log(LOG_NOTICE, "%s: ( sccp_rtp_setPeer ) Set remote address to %s\n", rtp->channel->designator, sccp_socket_stringify(&rtp->phone_remote));
	constChannelPtr channel = rtp->channel;
	uint16_t readState = rtp->readState;
	sccp_rtp_unlock(rtp);

	if (readState & SCCP_RTP_STATUS_ACTIVE) {
		//! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait)
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_setPeer) Stop media transmission\n", channel->designator);
		sccp_channel_updateMediaTransmission(channel);
	}
	return TRUE;
}

boolean_t sccp_rtp_setPhone(sccp_rtp_new_t * const rtp, const struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);
	boolean_t res = FALSE;
	/* validate socket */
	if (sccp_socket_getPort(sas) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_setPhone) new information is invalid, dont change anything\n", rtp->channel->designator);
		return FALSE;
	}
	
	//! \note do not check if address has changed otherwise we get an audio issue when resume on the same device, so we need to force asterisk to update -MC

	char buf1[NI_MAXHOST + NI_MAXSERV];
	char buf2[NI_MAXHOST + NI_MAXSERV];

	// get / set locked
	sccp_rtp_lock(rtp);
	memcpy(&rtp->phone, sas, sizeof(rtp->phone));
	constChannelPtr c = rtp->channel;
	sccp_copy_string(buf1, sccp_socket_stringify(&rtp->phone_remote), sizeof(buf1));
	sccp_copy_string(buf2, sccp_socket_stringify(&rtp->phone), sizeof(buf2));
	sccp_rtp_unlock(rtp);
	
	// update pbx
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);
	if (d) {
		if (iPbx.rtp_setPhoneAddress) {
			#warning FIX
			//res = iPbx.rtp_setPhoneAddress(rtp->pbxrtp, sas, d->nat >= SCCP_NAT_ON ? 1 : 0);
			res = TRUE;
		}
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell PBX   to send RTP/UDP media from %s to %s (NAT: %s)\n", rtp->channel->designator, buf1, buf2, sccp_nat2str(d->nat));
	}
	return res;
}

boolean_t sccp_rtp_updateRemoteNat(sccp_rtp_new_t *const rtp)
{
	assert(rtp != NULL);

	boolean_t res = FALSE;

	// set locked
	sccp_rtp_lock(rtp);
	struct sockaddr_storage phone_remote = {0};
	memcpy(&phone_remote, &rtp->phone_remote, sizeof(struct sockaddr_storage));
	constChannelPtr c = rtp->channel;
	sccp_rtp_unlock(rtp);
	
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);
	if (d) {
		struct sockaddr_storage sus = { 0 };
		sccp_session_getOurIP(d->session, &sus, 0);
		uint16_t usFamily = sccp_socket_is_IPv6(&sus) ? AF_INET6 : AF_INET;
		//sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) us: %s, usFamily: %s\n", c->designator, sccp_socket_stringify(&sus), (usFamily == AF_INET6) ? "IPv6" : "IPv4");

		uint16_t remoteFamily = (phone_remote.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&phone_remote)) ? AF_INET6 : AF_INET;
		//sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) remote: %s, remoteFamily: %s\n", c->designator, sccp_socket_stringify(&phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");

		//! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup
		if (d->nat >= SCCP_NAT_ON) {
			if ((usFamily == AF_INET) != remoteFamily) {						// device needs correction for ipv6 address in remote
				uint16_t port = sccp_socket_getPort(&sus);					// get rtp server port
				memcpy(&phone_remote, &sus, sizeof(struct sockaddr_storage));			// Not sure if this should not be the externip in case of nat
				sccp_socket_ipv4_mapped(&phone_remote, &phone_remote);				//!< we need this to convert mapped IPv4 to real IPv4 address
				sccp_socket_setPort(&phone_remote, port);
			} else if ((usFamily == AF_INET6) != remoteFamily) {					// the device can do IPv6 but should send it to IPv4 address (directrtp possible)
 				struct sockaddr_storage sas;
				memcpy(&sas, &phone_remote, sizeof(struct sockaddr_storage));
				sccp_socket_ipv4_mapped(&sas, &sas);
			}
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (startMediaTransmission) new remote: %s, new remoteFamily: %s\n", c->designator, sccp_socket_stringify(&phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");
			//res = TRUE;
		}
		
		char buf1[NI_MAXHOST + NI_MAXSERV];
		char buf2[NI_MAXHOST + NI_MAXSERV];

		sccp_copy_string(buf1, sccp_socket_stringify(&rtp->phone), sizeof(buf1));
		sccp_copy_string(buf2, sccp_socket_stringify(&phone_remote), sizeof(buf2));

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell Phone to send RTP/UDP media from %s to %s (NAT: %s)\n", c->designator, buf1, buf2, sccp_nat2str(d->nat));
	}

	// set locked    
	if (!socket_equals(&phone_remote, &rtp->phone_remote)) {
		sccp_rtp_lock(rtp);
		memcpy(&rtp->phone_remote, &phone_remote, sizeof(struct sockaddr_storage));
		sccp_rtp_unlock(rtp);
		res = TRUE;
	}
	
	return res;
}

/*
uint16_t sccp_rtp_getReadState(const sccp_rtp_t * const rtp)
{
	assert(rtp != NULL);
	
	sccp_rtp_lock(rtp);
	uint16_t readState = rtp->readState;
	sccp_rtp_unlock(rtp);
	
	return readState;
}

uint16_t sccp_rtp_getWriteState(const sccp_rtp_t * const rtp)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	uint16_t writeState = rtp->writeState;
	sccp_rtp_unlock(rtp);

	return writeState;
}

boolean_t sccp_rtp_isDirectMedia(const sccp_rtp_t * const rtp)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	boolean_t directMedia = rtp->directMedia;
	sccp_rtp_unlock(rtp);
	
	return directMedia;
}

skinny_codec_t sccp_rtp_getReadFormat(const sccp_rtp_t * const rtp)
{
	assert(rtp != NULL);
	
	sccp_rtp_lock(rtp);
	skinny_codec_t readFormat = rtp->readFormat;
	sccp_rtp_unlock(rtp);

	return readFormat;
}
skinny_codec_t sccp_rtp_getWriteFormat(const sccp_rtp_t * const rtp)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	skinny_codec_t writeFormat = rtp->writeFormat;
	sccp_rtp_unlock(rtp);

	return writeFormat;
}

int sccp_rtp_getPhoneAddress(const sccp_rtp_t * const rtp, struct sockaddr_storage *const sas)
{
	assert(rtp != NULL && sas != NULL);
	int res = 0;

	sccp_rtp_lock(rtp);
	if (rtp->readState != SCCP_RTP_STATUS_INACTIVE) {
		memcpy(sas, &rtp->phone, sizeof(struct sockaddr_storage));
		res = 1;
	}
	sccp_rtp_unlock(rtp);

	return res;
}

int sccp_rtp_getRemotePhoneAddress(const sccp_rtp_t * const rtp, struct sockaddr_storage *const sas)
{
	assert(rtp != NULL && sas != NULL);
	int res = 0;

	sccp_rtp_lock(rtp);
	if (rtp->readState != SCCP_RTP_STATUS_INACTIVE) {
		memcpy(sas, &rtp->phone_remote, sizeof(struct sockaddr_storage));
		res = 1;
	}
	sccp_rtp_unlock(rtp);

	return res;
}

sccp_rtp_setReadState(sccp_rtp_t * const rtp, uint16_t value)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	rtp->readState = value;
	sccp_rtp_unlock(rtp);

	return 1;
}
sccp_rtp_setWriteState(sccp_rtp_t * const rtp, uint16_t value)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	rtp->writeState = value;
	sccp_rtp_unlock(rtp);

	return 1;
}

sccp_rtp_setDirectMedia(sccp_rtp_t * const rtp, boolean_t direct)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	rtp->directMedia = direct;
	sccp_rtp_unlock(rtp);

	return 1;
}

sccp_rtp_setReadFormat(sccp_rtp_t * const rtp, skinny_codec_t codec)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	rtp->readFormat = codec;
	sccp_rtp_unlock(rtp);

	return 1;
}
sccp_rtp_setWriteFormat(sccp_rtp_t * const rtp, skinny_codec_t codec)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	rtp->writeFormat = codec;
	sccp_rtp_unlock(rtp);

	return 1;
}

sccp_rtp_setPhoneAddress(sccp_rtp_t * const rtp, const struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	memcpy(&rtp->phone, sas, sizeof(struct sockaddr_storage));
	sccp_rtp_unlock(rtp);

	return 1;
}

sccp_rtp_setRemotePhoneAddress(sccp_rtp_t * const rtp, const struct sockaddr_storage *const sas)
{
	assert(rtp != NULL);

	sccp_rtp_lock(rtp);
	memcpy(&rtp->phone_remote, sas, sizeof(struct sockaddr_storage));
	sccp_rtp_unlock(rtp);

	return 1;
}
*/

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
