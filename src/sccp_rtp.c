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

/*!
 * \brief create a new rtp server for audio data
 * \param c SCCP Channel
 */
int sccp_rtp_createAudioServer(const sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;
	boolean_t isMappedIPv4 = FALSE;

	if (!c) {
		return FALSE;
	}
	if (c->rtp.audio.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "we already have a rtp server, we use this one\n");
		return TRUE;
	}

	if (PBX(rtp_audio_create)) {
		rtpResult = (boolean_t) PBX(rtp_audio_create) ((sccp_channel_t *) c);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
		return FALSE;
	}

	if (!sccp_rtp_getUs(&c->rtp.audio, &((sccp_channel_t *) c)->rtp.audio.phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
		return FALSE;
	}

	uint16_t port = sccp_rtp_getServerPort(&c->rtp.audio);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "RTP Server Port: %d\n", port);

	/* depending on the clients connection, we us ipv4 or ipv6 */
	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	struct sockaddr_storage *phone_remote = (struct sockaddr_storage*) &c->rtp.audio.phone_remote;
	if (device) {
		memcpy(phone_remote, &device->session->ourip, sizeof(struct sockaddr_storage));		/* fallback */
		sccp_socket_setPort(phone_remote, port);
	}

	uint8_t isIPv4 = sccp_socket_is_IPv4(phone_remote) ? 1 : 0;
	uint8_t isIPv6 = sccp_socket_is_IPv6(phone_remote) ? 1 : 0;
	if (isIPv6) {
		isMappedIPv4 = sccp_socket_ipv4_mapped(phone_remote, (struct sockaddr_storage*) &c->rtp.audio.phone_remote);		/*!< this is absolute necessary */
	}
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createAudioServer) phone_remote: %s (IPv4: %d, IPv6: %d, Mapped: %d)\n", c->designator, sccp_socket_stringify(&c->rtp.audio.phone_remote), isIPv4, isIPv6, isMappedIPv4);
	return rtpResult;
}

/*!
 * \brief create a new rtp server for video data
 * \param c SCCP Channel
 */
int sccp_rtp_createVideoServer(const sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;
	boolean_t isMappedIPv4 = FALSE;

	if (!c) {
		return FALSE;
	}
	if (c->rtp.video.rtp) {
		pbx_log(LOG_ERROR, "we already have a rtp server, why dont we use this?\n");
		return TRUE;
	}

	if (PBX(rtp_video_create)) {
		rtpResult = (boolean_t) PBX(rtp_video_create) ((sccp_channel_t *) c);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
	}

	if (!sccp_rtp_getUs(&c->rtp.video, &((sccp_channel_t *) c)->rtp.video.phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
	}

	uint16_t port = sccp_rtp_getServerPort(&c->rtp.video);
	/* depending on the clients connection, we us ipv4 or ipv6 */
	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	struct sockaddr_storage *phone_remote = (struct sockaddr_storage*) &c->rtp.video.phone_remote;
	if (device) {
		memcpy(phone_remote, &device->session->ourip, sizeof(struct sockaddr_storage));		/* fallback */
		sccp_socket_setPort(phone_remote, port);
	}

	uint8_t isIPv4 = sccp_socket_is_IPv4(phone_remote) ? 1 : 0;
	uint8_t isIPv6 = sccp_socket_is_IPv6(phone_remote) ? 1 : 0;
	if (isIPv6) {
		isMappedIPv4 = sccp_socket_ipv4_mapped(phone_remote, (struct sockaddr_storage*) &c->rtp.video.phone_remote);		/*!< this is absolute necessary */
	}
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createVideoServer) phone_remote: %s (IPv4: %d, IPv6: %d, Mapped: %d)\n", c->designator, sccp_socket_stringify(&c->rtp.audio.phone_remote), isIPv4, isIPv6, isMappedIPv4);

	return rtpResult;
}

/*!
 * \brief Stop an RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_stop(sccp_channel_t * c)
{
	if (!c) {
		return;
	}
	if (PBX(rtp_stop)) {
		PBX(rtp_stop) (c);
	} else {
		pbx_log(LOG_ERROR, "no pbx function to stop rtp\n");
	}
}

/*!
 * \brief set the address the phone should send rtp media.
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_peer(sccp_channel_t * c, sccp_rtp_t *rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_socket_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: ( sccp_rtp_set_peer ) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	/* check if we have new infos */
	if (socket_equals(new_peer, &c->rtp.audio.phone_remote)) {
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
void sccp_rtp_set_phone(sccp_channel_t * c, sccp_rtp_t *rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_socket_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_phone) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	AUTO_RELEASE sccp_device_t *device = sccp_channel_getDevice_retained(c);

	if (device) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s\n", device->id, sccp_socket_stringify(new_peer));
		if (device->nat >= SCCP_NAT_ON) {								/* Use connected server socket address instead */
			uint16_t port = sccp_socket_getPort(new_peer);
			memcpy(&rtp->phone, &device->session->sin, sizeof(struct sockaddr_storage));
			sccp_socket_ipv4_mapped(&rtp->phone, &rtp->phone);
			sccp_socket_setPort(&rtp->phone, port);
		} else {
			memcpy(&rtp->phone, new_peer, sizeof(struct sockaddr_storage));
		}

		//update pbx
		if (PBX(rtp_setPhoneAddress)) {
			PBX(rtp_setPhoneAddress) (rtp, &rtp->phone, device->nat >= SCCP_NAT_ON ? 1 : 0);
		}

		char buf1[NI_MAXHOST + NI_MAXSERV];
		char buf2[NI_MAXHOST + NI_MAXSERV];
		sccp_copy_string(buf1, sccp_socket_stringify(&rtp->phone_remote), sizeof(buf1));
		sccp_copy_string(buf2, sccp_socket_stringify(&rtp->phone), sizeof(buf2));
		if (device->nat < SCCP_NAT_ON) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell PBX   to send RTP/UDP media from %s to %s (No NAT)\n", DEV_ID_LOG(device), buf1, buf2);
		} else {
			char buf3[NI_MAXHOST + NI_MAXSERV];
			sccp_copy_string(buf3, sccp_socket_stringify(new_peer), sizeof(buf3));
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell PBX   to send RTP/UDP media from %s to %s (NAT: %s)\n", DEV_ID_LOG(device), buf1, buf2, buf3);
		}
	}
}

void sccp_rtp_updateNatAddress(sccp_channel_t *channel) 
{
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);
	uint16_t usFamily = (d->session->ourip.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&d->session->ourip)) ? AF_INET6 : AF_INET;
	uint16_t remoteFamily = (channel->rtp.audio.phone_remote.ss_family == AF_INET6 && !sccp_socket_is_mapped_IPv4(&channel->rtp.audio.phone_remote)) ? AF_INET6 : AF_INET;

	/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
	if (d->nat >= SCCP_NAT_ON) {
		if ((usFamily == AF_INET) != remoteFamily) {							/* device needs correction for ipv6 address in remote */
			uint16_t port = sccp_rtp_getServerPort(&channel->rtp.audio);

			if (!sccp_socket_getExternalAddr(&channel->rtp.audio.phone_remote)) {				/* Use externip (PBX behind NAT Firewall */
				memcpy(&channel->rtp.audio.phone_remote, &d->session->ourip, sizeof(struct sockaddr_storage));	/* Fallback: use ip-address of incoming interface */
			}
			sccp_socket_ipv4_mapped(&channel->rtp.audio.phone_remote, &channel->rtp.audio.phone_remote);	/*!< we need this to convert mapped IPv4 to real IPv4 address */
			sccp_socket_setPort(&channel->rtp.audio.phone_remote, port);

		} else if ((usFamily == AF_INET6) != remoteFamily) {						/* the device can do IPv6 but should send it to IPv4 address (directrtp possible) */
			struct sockaddr_storage sas;

			memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
			sccp_socket_ipv4_mapped(&sas, &sas);
		}
	}
}

/*!
 * \brief Get Audio Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(const sccp_channel_t * c, sccp_rtp_t **rtp)
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
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(const sccp_channel_t * c, sccp_rtp_t ** rtp)
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
 * \brief Get Payload Type
 */
uint8_t sccp_rtp_get_payloadType(const sccp_rtp_t * rtp, skinny_codec_t codec)
{
	if (PBX(rtp_get_payloadType)) {
		return PBX(rtp_get_payloadType) (rtp, codec);
	} else {
		return 97;
	}
}

/*!
 * \brief Get Sample Rate
 */
int sccp_rtp_get_sampleRate(skinny_codec_t codec)
{
	if (PBX(rtp_get_sampleRate)) {
		return PBX(rtp_get_sampleRate) (codec);
	} else {
		return 3840;
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_destroy(sccp_channel_t * c)
{
	sccp_line_t *l = c->line;

	if (c->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX rtp server on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		PBX(rtp_destroy) (c->rtp.audio.rtp);
		c->rtp.audio.rtp = NULL;
	}

	if (c->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX vrtp server on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		PBX(rtp_destroy) (c->rtp.video.rtp);
		c->rtp.video.rtp = NULL;
	}
}

/*!
 * \brief Get Audio Peer
 */
boolean_t sccp_rtp_getAudioPeer(sccp_channel_t * c, struct sockaddr_storage **new_peer)
{
	*new_peer = &c->rtp.audio.phone_remote;
	return TRUE;
}

/*!
 * \brief Get Video Peer
 */
boolean_t sccp_rtp_getVideoPeer(sccp_channel_t * c, struct sockaddr_storage **new_peer)
{
	*new_peer = &c->rtp.video.phone_remote;
	return TRUE;
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getUs(const sccp_rtp_t *rtp, struct sockaddr_storage *us)
{
	if (rtp->rtp) {
		PBX(rtp_getUs) (rtp->rtp, us);
		return TRUE;
	} else {
		// us = &rtp->phone_remote;
		return FALSE;
	}
}

uint16_t sccp_rtp_getServerPort(const sccp_rtp_t * rtp)
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
boolean_t sccp_rtp_getPeer(const sccp_rtp_t *rtp, struct sockaddr_storage *them)
{
	if (rtp->rtp) {
		PBX(rtp_getPeer) (rtp->rtp, them);
		return TRUE;
	} else {
		return FALSE;
	}
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
