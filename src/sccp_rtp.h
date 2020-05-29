/*!
 * \file        sccp_rtp.h
 * \brief       SCCP RTP Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

#include "sccp_codec.h"

/* can be removed in favor of forward declaration if we change phone and phone_remote to pointers instead */
#include <netinet/in.h>

__BEGIN_C_EXTERN__
typedef void (*scpp_rtp_direction_cb_t)(constChannelPtr c);

/* Note: We should maybe move the callback to sccp_rtp_t instead of sccp_rtp_direction_t because it get's a little confusing in sccp_indicate CONNECTED */
/* Note: In the process of making this private, using "_" to signal the has to be locked when reading/writing */
typedef struct sccp_rtp_direction {
	uint16_t _state;
	skinny_codec_t format;
	scpp_rtp_direction_cb_t cb;
} sccp_rtp_direction_t;

/*!
 * \brief SCCP RTP Structure
 */
struct sccp_rtp {
	sccp_mutex_t lock;
	PBX_RTP_TYPE *instance;											/*!< pbx rtp instance pointer */
	boolean_t instance_active;
	sccp_rtp_type_t type;											/* audio/video/data */
	sccp_rtp_direction_t reception;										/* receive rtp / ORC */
	sccp_rtp_direction_t transmission;									/* transmit rtp / SMT */
	struct sockaddr_storage phone;										/*!< our phone information (openreceive) */
	struct sockaddr_storage phone_remote;									/*!< phone destination address (starttransmission) */
	uint16_t RTCPPortNumber;										/*!< RTCP Port used by the phone */
 	boolean_t directMedia;											/*!< Show if we are running in directmedia mode (set in pbx_impl during rtp bridging) */
};														/*!< SCCP RTP Structure */

SCCP_API boolean_t SCCP_CALL sccp_rtp_createServer(constDevicePtr d, channelPtr c, sccp_rtp_type_t type);
SCCP_API int SCCP_CALL sccp_rtp_requestRTPPorts(constDevicePtr device, channelPtr channel);
SCCP_API void SCCP_CALL sccp_rtp_stop(constChannelPtr channel);
SCCP_API void SCCP_CALL sccp_rtp_destroy(constChannelPtr c);
SCCP_API void SCCP_CALL sccp_rtp_set_peer(constChannelPtr c, rtpPtr rtp, struct sockaddr_storage * new_peer);
SCCP_API void SCCP_CALL sccp_rtp_set_phone(constChannelPtr c, rtpPtr rtp, struct sockaddr_storage * new_peer);
SCCP_API int SCCP_CALL sccp_rtp_updateNatRemotePhone(constChannelPtr c, rtpPtr rtp);
SCCP_API void SCCP_CALL sccp_rtp_print(constChannelPtr c, sccp_rtp_type_t type, struct ast_str * buf, int buflen);

SCCP_API boolean_t SCCP_CALL sccp_rtp_getAudioPeer(constChannelPtr c, struct sockaddr_storage **new_peer);
SCCP_API sccp_rtp_info_t SCCP_CALL sccp_rtp_getAudioPeerInfo(constChannelPtr c, sccp_rtp_t **rtp);
#ifdef CS_SCCP_VIDEO
SCCP_API boolean_t SCCP_CALL sccp_rtp_getVideoPeer(constChannelPtr c, struct sockaddr_storage **new_peer);
SCCP_API sccp_rtp_info_t SCCP_CALL sccp_rtp_getVideoPeerInfo(constChannelPtr c, sccp_rtp_t **rtp);
#endif

SCCP_API sccp_rtp_status_t SCCP_CALL sccp_rtp_getState(constRtpPtr rtp, sccp_rtp_dir_t dir);
SCCP_API sccp_rtp_status_t SCCP_CALL sccp_rtp_areBothInvalid(constRtpPtr rtp);
SCCP_API void SCCP_CALL sccp_rtp_appendState(rtpPtr rtp, sccp_rtp_dir_t dir, sccp_rtp_status_t state);
SCCP_API void SCCP_CALL sccp_rtp_subtractState(rtpPtr rtp, sccp_rtp_dir_t dir, sccp_rtp_status_t state);
SCCP_API void SCCP_CALL sccp_rtp_setState(rtpPtr rtp, sccp_rtp_dir_t dir, sccp_rtp_status_t newstate);
SCCP_API void SCCP_CALL sccp_rtp_setCallback(rtpPtr rtp, sccp_rtp_dir_t dir, scpp_rtp_direction_cb_t cb);
SCCP_API boolean_t SCCP_CALL sccp_rtp_runCallback(rtpPtr rtp, sccp_rtp_dir_t dir, constChannelPtr c);

SCCP_API uint8_t SCCP_CALL sccp_rtp_get_payloadType(constRtpPtr rtp, skinny_codec_t codec);
SCCP_API boolean_t SCCP_CALL sccp_rtp_getUs(constRtpPtr rtp, struct sockaddr_storage * us);
SCCP_API boolean_t SCCP_CALL sccp_rtp_getPeer(constRtpPtr rtp, struct sockaddr_storage * them);
SCCP_API uint16_t SCCP_CALL sccp_rtp_getServerPort(constRtpPtr rtp);
SCCP_API int SCCP_CALL sccp_rtp_get_sampleRate(skinny_codec_t codec);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
