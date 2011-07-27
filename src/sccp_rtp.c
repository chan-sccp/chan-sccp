
/*!
 * \file 	sccp_rtp.c
 * \brief 	SCCP RTP Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note		Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/*!
 * \brief create a new rtp server for audio data
 * \param c SCCP Channel
 */
int sccp_rtp_createAudioServer(sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;

	if (!c)
		return -1;

	if (PBX(rtp_audio_create)) {
		rtpResult = PBX(rtp_audio_create) (c, &c->rtp.audio.rtp);
	} else {
		ast_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
	}
	return rtpResult;
}

/*!
 * \brief create a new rtp server for video data
 * \param c SCCP Channel
 */
int sccp_rtp_createVideoServer(sccp_channel_t * c)
{
	boolean_t rtpResult = FALSE;

	if (!c)
		return FALSE;

	if (PBX(rtp_video_create)) {
		rtpResult = PBX(rtp_video_create) (c, &c->rtp.video.rtp);
	} else {
		ast_log(LOG_ERROR, "we should start our own rtp server, but we dont have one\n");
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
		ast_log(LOG_ERROR, "no pbx function to stop rtp\n");
	}

}

/*!
 * \brief Stop an RTP Source.
 * \param c SCCP Channel
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_peer(sccp_channel_t * c, struct sockaddr_in *new_peer)
{

	/* validate socket */
	if (new_peer->sin_port == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (__PRETTY_FUNCTION__) remote information are invalid, dont change anything\n", DEV_ID_LOG(c->device));
		return;
	}

	/* check if we have new infos */
	if (socket_equals(new_peer, &c->rtp.audio.phone_remote)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (__PRETTY_FUNCTION__) remote information are equals with our curent one, ignore change\n", DEV_ID_LOG(c->device));
		return;
	}

	memcpy(&c->rtp.audio.phone_remote, new_peer, sizeof(c->rtp.audio.phone_remote));
	if (c->rtp.audio.status & SCCP_RTP_STATUS_TRANSMIT) {
		/* Shutdown any early-media or previous media on re-invite */
		/* \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (__PRETTY_FUNCTION__) Stop media transmission on channel %d\n", DEV_ID_LOG(c->device), c->callid);
		sccp_channel_stopmediatransmission_locked(c);
	}
	sccp_channel_startmediatransmission(c);
}

sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(const sccp_channel_t * c, struct sccp_rtp **rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	if (!c->device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.audio);

	result = SCCP_RTP_INFO_AVAILABLE;
	if (c->device->directrtp && !c->device->nat) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}
	return result;
}

sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(const sccp_channel_t * c, struct sccp_rtp ** rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	if (!c->device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.video);

	result = SCCP_RTP_INFO_AVAILABLE;
	return result;
}

uint8_t sccp_rtp_get_payloadType(const struct sccp_rtp * rtp, skinny_media_payload codec)
{
	if (PBX(rtp_get_payloadType)) {
		return PBX(rtp_get_payloadType) (rtp, codec);
	} else {
		return 97;
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_destroy(sccp_channel_t * c)
{
	sccp_device_t *d = NULL;

	sccp_line_t *l = NULL;

	if (c && c->line) {
		l = c->line;
		d = c->device;
	}

	if (c->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying phone media transmission on channel %s-%08X\n", (d && d->id) ? d->id : "SCCP", l ? l->name : "(null)", c->callid);
		/*! \todo use pbx callback */
		ast_rtp_destroy(c->rtp.audio.rtp);
		c->rtp.audio.rtp = NULL;
	}

	if (c->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying video media transmission on channel %s-%08X\n", (d && d->id) ? d->id : "SCCP", l ? l->name : "(null)", c->callid);
		/*! \todo use pbx callback */
		ast_rtp_destroy(c->rtp.video.rtp);
		c->rtp.video.rtp = NULL;
	}
}

boolean_t sccp_rtp_getAudioPeer(sccp_channel_t * c, struct sockaddr_in **new_peer)
{
	*new_peer = &c->rtp.audio.phone_remote;
	return TRUE;
}

boolean_t sccp_rtp_getVideoPeer(sccp_channel_t * c, struct sockaddr_in ** new_peer)
{
	*new_peer = &c->rtp.video.phone_remote;
	return TRUE;
}
