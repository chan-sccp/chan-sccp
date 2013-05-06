
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

SCCP_FILE_VERSION(__FILE__, "$Revision$")

    /*!
     * \brief create a new rtp server for audio data
     * \param c SCCP Channel
     */
int sccp_rtp_createAudioServer(const sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;
	sccp_device_t *device = NULL;


	if (!c)
		return -1;

	if (c->rtp.audio.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "we already have a rtp server, we use this one\n");
		return TRUE;
	}

	if (PBX(rtp_audio_create)) {
		rtpResult = (boolean_t) PBX(rtp_audio_create) ((sccp_channel_t *) c);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
	}

	if (!sccp_rtp_getUs(&c->rtp.audio, &((sccp_channel_t *) c)->rtp.audio.phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
	}

	device = sccp_channel_getDevice_retained(c);
	//struct sockaddr_in us;
	//      PBX(rtp_setPeer) (&c->rtp.audio, &c->rtp.audio.phone, device ? device->nat : 0);
	device = device ? sccp_device_release(device) : NULL;

	return rtpResult;
}

/*!
 * \brief create a new rtp server for video data
 * \param c SCCP Channel
 */
int sccp_rtp_createVideoServer(const sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;

	if (!c)
		return FALSE;

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

	return rtpResult;
}

/*!
 * \brief Stop an RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_stop(sccp_channel_t * c)
{
	if (!c)
		return;

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
void sccp_rtp_set_peer(sccp_channel_t * c, struct sccp_rtp *rtp, struct sockaddr_in *new_peer)
{
	/* validate socket */
	if (new_peer->sin_port == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: ( sccp_rtp_set_peer ) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	/* check if we have new infos */
	if (socket_equals(new_peer, &c->rtp.audio.phone_remote)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) remote information are equal to the current one, ignore change\n", c->currentDeviceId);
		return;
	}

	memcpy(&c->rtp.audio.phone_remote, new_peer, sizeof(c->rtp.audio.phone_remote));
	pbx_log(LOG_NOTICE, "%s: ( sccp_rtp_set_peer ) Set remote address to %s:%d\n", c->currentDeviceId, pbx_inet_ntoa(new_peer->sin_addr), ntohs(new_peer->sin_port));

	if (c->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE) {
		/* Shutdown any early-media or previous media on re-invite */
		/*! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) Stop media transmission on channel %d\n", c->currentDeviceId, c->callid);
		sccp_channel_stopmediatransmission(c);
		sccp_channel_startmediatransmission(c);
	}

}

/*!
 * \brief set the address where the phone should send rtp media.
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_phone(sccp_channel_t *c, struct sccp_rtp *rtp, struct sockaddr_in *new_peer)
{
	sccp_device_t *device;

	/* validate socket */
	if (new_peer->sin_port == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_phone) remote information are invalid, dont change anything\n", c->currentDeviceId);
		return;
	}

	if ((device = sccp_channel_getDevice_retained(c))) {

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
		if (PBX(rtp_setPeer)) {
			PBX(rtp_setPeer) (rtp, new_peer, device->nat);
		}

		char cbuf1[128] = "";
		char cbuf2[128] = "";
		sprintf (cbuf1,"%15s:%d", pbx_inet_ntoa(rtp->phone_remote.sin_addr), ntohs(rtp->phone_remote.sin_port));
		sprintf (cbuf2,"%15s:%d", pbx_inet_ntoa(rtp->phone.sin_addr), ntohs(rtp->phone.sin_port));

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell PBX   to send RTP/UDP media from:%s to %s (NAT: %s)\n", DEV_ID_LOG(device), cbuf1, cbuf2, device->nat ? "yes" : "no");
		

		device = sccp_device_release(device);
	}
}

/*!
 * \brief Get Audio Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(const sccp_channel_t * c, struct sccp_rtp **rtp)
{
	sccp_device_t *device = NULL;
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	device = sccp_channel_getDevice_retained(c);
	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.audio);

	result = SCCP_RTP_INFO_AVAILABLE;
	if (device->directrtp && !device->nat) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}

	device = device ? sccp_device_release(device) : NULL;
	return result;
}

/*!
 * \brief Get Video Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(const sccp_channel_t * c, struct sccp_rtp ** rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;
	sccp_device_t *device = NULL;

	device = sccp_channel_getDevice_retained(c);
	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.video);

	result = SCCP_RTP_INFO_AVAILABLE;

	device = device ? sccp_device_release(device) : NULL;
	return result;
}

/*!
 * \brief Get Payload Type
 */
uint8_t sccp_rtp_get_payloadType(const struct sccp_rtp * rtp, skinny_codec_t codec)
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
uint8_t sccp_rtp_get_sampleRate(skinny_codec_t codec)
{
	if (PBX(rtp_get_sampleRate)) {
		return PBX(rtp_get_sampleRate) (codec);
	} else {
		return (uint8_t) 3840;
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_destroy(sccp_channel_t * c)
{
	sccp_line_t *l = NULL;

	l = c->line;

	if (c->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying phone media transmission on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		PBX(rtp_destroy) (c->rtp.audio.rtp);
		c->rtp.audio.rtp = NULL;
	}

	if (c->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying video media transmission on channel %s-%08X\n", c->currentDeviceId, l ? l->name : "(null)", c->callid);
		PBX(rtp_destroy) (c->rtp.video.rtp);
		c->rtp.video.rtp = NULL;
	}
}

/*!
 * \brief Get Audio Peer
 */
boolean_t sccp_rtp_getAudioPeer(sccp_channel_t * c, struct sockaddr_in **new_peer)
{
	*new_peer = &c->rtp.audio.phone_remote;
	return TRUE;
}

/*!
 * \brief Get Video Peer
 */
boolean_t sccp_rtp_getVideoPeer(sccp_channel_t * c, struct sockaddr_in **new_peer)
{
	*new_peer = &c->rtp.audio.phone_remote;
	return TRUE;
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getUs(const struct sccp_rtp *rtp, struct sockaddr_in *us)
{
	if (rtp->rtp) {
		PBX(rtp_getUs) (rtp->rtp, us);
		return TRUE;
	} else {
		us = (struct sockaddr_in *) &rtp->phone_remote;
		//              return FALSE;
		return TRUE;
	}
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getPeer(const struct sccp_rtp *rtp, struct sockaddr_in *them)
{
	if (rtp->rtp) {
		PBX(rtp_getPeer) (rtp->rtp, them);
		return TRUE;
	} else {
		return FALSE;
	}
}
