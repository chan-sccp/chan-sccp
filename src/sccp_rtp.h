
/*!
 * \file 	sccp_rtp.h
 * \brief 	SCCP RTP Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */
#    ifndef __SCCP_RTP_H
#define __SCCP_RTP_H

typedef enum {
	SCCP_RTP_INFO_NORTP = 0,
	SCCP_RTP_INFO_AVAILABLE = 1 << 2,
	SCCP_RTP_INFO_ALLOW_DIRECTRTP = 1 << 2,
} sccp_rtp_info_t;								/*!< RTP status information */

int sccp_rtp_createAudioServer(const sccp_channel_t * c);
int sccp_rtp_createVideoServer(const sccp_channel_t * c);
void sccp_rtp_stop(sccp_channel_t * c);
void sccp_rtp_destroy(sccp_channel_t * c);
void sccp_rtp_set_peer(sccp_channel_t * c, struct sccp_rtp *rtp, struct sockaddr_in *new_peer);
void sccp_rtp_set_phone(sccp_channel_t * c, struct sccp_rtp *rtp, struct sockaddr_in *new_peer);
boolean_t sccp_rtp_getAudioPeer(sccp_channel_t * c, struct sockaddr_in **new_peer);
boolean_t sccp_rtp_getVideoPeer(sccp_channel_t * c, struct sockaddr_in **new_peer);
uint8_t sccp_rtp_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec);
uint8_t sccp_rtp_get_sampleRate(skinny_codec_t codec);

sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(const sccp_channel_t * c, struct sccp_rtp **rtp);
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(const sccp_channel_t * c, struct sccp_rtp **rtp);
boolean_t sccp_rtp_getUs(const struct sccp_rtp *rtp, struct sockaddr_in *us);

#    endif										// __SCCP_RTP_H
