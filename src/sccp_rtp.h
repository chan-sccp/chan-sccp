/*!
 * \file        sccp_rtp.h
 * \brief       SCCP RTP Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */
#pragma once

/*!
 * \brief SCCP RTP Structure
 */
struct sccp_rtp {
	sccp_mutex_t lock;
	PBX_RTP_TYPE *rtp;											/*!< pbx rtp pointer */
	uint16_t readState;											/*!< current read state */
	uint16_t writeState;											/*!< current write state */
	boolean_t directMedia;											/*!< Show if we are running in directmedia mode (set in pbx_impl during rtp bridging) */
	skinny_codec_t readFormat;										/*!< current read format */
	skinny_codec_t writeFormat;										/*!< current write format */
	struct sockaddr_storage phone;										/*!< our phone information (openreceive) */
	struct sockaddr_storage phone_remote;									/*!< phone destination address (starttransmission) */
};														/*!< SCCP RTP Structure */

int sccp_rtp_createAudioServer(constChannelPtr c);
int sccp_rtp_createVideoServer(constChannelPtr c);
void sccp_rtp_stop(constChannelPtr c);
void sccp_rtp_destroy(constChannelPtr c);
void sccp_rtp_set_peer(constChannelPtr c, sccp_rtp_t *rtp, struct sockaddr_storage *new_peer);
void sccp_rtp_set_phone(constChannelPtr c, sccp_rtp_t *rtp, struct sockaddr_storage *new_peer);
int sccp_rtp_updateNatRemotePhone(constChannelPtr c, sccp_rtp_t *const rtp);

boolean_t sccp_rtp_getAudioPeer(constChannelPtr c, struct sockaddr_storage **new_peer);
boolean_t sccp_rtp_getVideoPeer(constChannelPtr c, struct sockaddr_storage **new_peer);
sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(constChannelPtr c, sccp_rtp_t **rtp);
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(constChannelPtr c, sccp_rtp_t **rtp);

uint8_t sccp_rtp_get_payloadType(const sccp_rtp_t *const rtp, skinny_codec_t codec);
boolean_t sccp_rtp_getUs(const sccp_rtp_t * const rtp, struct sockaddr_storage *us);
boolean_t sccp_rtp_getPeer(const sccp_rtp_t * const rtp, struct sockaddr_storage *us);
uint16_t sccp_rtp_getServerPort(const sccp_rtp_t * const rtp);
int sccp_rtp_get_sampleRate(skinny_codec_t codec);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
