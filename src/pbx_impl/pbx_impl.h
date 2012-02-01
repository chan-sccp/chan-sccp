
/*!
 * \file        pbx_impl.h
 * \brief       SCCP PBX Asterisk Wrapper Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#ifndef __PBX_IMPL_H
#    define __PBX_IMPL_H

#    include "../config.h"
#    include "../common.h"

#    ifdef HAVE_ASTERISK
#        include "ast/ast.h"
#    endif
#    ifdef HAVE_CALLWEAVER
#        include "cw/cw.h"
#    endif

#    define PBX_BRIDGE_TYPE struct ast_bridge

/*!
 * \brief SCCP PBX Callback function
 *
 * Register Callback_Functions for a particular PBX Implementation and PBX Version. This will make it possible to make 
 * chan-sccp agnostic to the PBX Software we are connected to. And provides a way to cope with multiple different versions
 * of these PBX Implementation. There Callback functions are implemented under the pbx_impl directory
 */
struct sccp_pbx_cb {
	/* *INDENT-OFF* */
	/* channels */
	boolean_t(*const alloc_pbxChannel) (const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel);
	boolean_t(*const alloc_conferenceTempPBXChannel) (PBX_CHANNEL_TYPE * channel, PBX_CHANNEL_TYPE ** pbx_channel, uint32_t conf_id, uint32_t part_id);
	int (*const set_callstate) (const sccp_channel_t * ast_channel, int state);
	boolean_t(*const checkhangup) (const sccp_channel_t * channel);
	int (*const hangup) (PBX_CHANNEL_TYPE * channel);
	int (*const requestHangup) (PBX_CHANNEL_TYPE * channel);
	int (*const forceHangup) (PBX_CHANNEL_TYPE * ast_channel, pbx_hangup_type_t pbx_hangup_type);
	sccp_extension_status_t(*const extension_status) (const sccp_channel_t * channel);

	/** get channel by name */
	boolean_t(*const getChannelByName) (const char *name, PBX_CHANNEL_TYPE **pbx_channel);
	boolean_t(*const getRemoteChannel) (const sccp_channel_t *channel, PBX_CHANNEL_TYPE **pbx_channel);
	void *(*const getChannelByCallback) (int (*is_match)(PBX_CHANNEL_TYPE *, void *),void *data);
	const char *(*const getChannelLinkId) (const sccp_channel_t * channel);

	int (*const set_nativeAudioFormats) (const sccp_channel_t * channel, skinny_codec_t codec[], int length);
	int (*const set_nativeVideoFormats) (const sccp_channel_t * channel, uint32_t);
	int (*const getPeerCodecCapabilities) (const sccp_channel_t * channel, void **capabilities);
	int (*const send_digit) (const sccp_channel_t * channel, const char digit);
	int (*const send_digits) (const sccp_channel_t * channel, const char *digits);

	int (*const sched_add) (int when, sccp_sched_cb callback, const void *data);
	int (*const sched_del) (int id);
	long (*const sched_when) (int id);
	int (*const sched_wait) (int id);

	/* rtp */
	int (*const rtp_getPeer) (const sccp_channel_t * channel, struct sockaddr_in * address);
	boolean_t(*const rtp_getUs) (PBX_RTP_TYPE * rtp, struct sockaddr_in * address);
	int (*const rtp_setPeer) (const struct sccp_rtp * rtp, const struct sockaddr_in * new_peer, int nat_active);
	boolean_t(*const rtp_setWriteFormat) (const sccp_channel_t * channel, skinny_codec_t codec);
	boolean_t(*const rtp_setReadFormat) (const sccp_channel_t * channel, skinny_codec_t codec);
	boolean_t(*const rtp_destroy) (PBX_RTP_TYPE * tp);
	int (*const rtp_stop) (sccp_channel_t * channel);
	int (*const rtp_codec) (sccp_channel_t * channel);
	boolean_t(*const rtp_audio_create) (const sccp_channel_t * channel, void **new_rtp);
	boolean_t(*const rtp_video_create) (const sccp_channel_t * channel, void **new_rtp);
	uint8_t(*const rtp_get_payloadType) (const struct sccp_rtp * rtp, skinny_codec_t codec);
	uint8_t(*const rtp_get_sampleRate) (skinny_codec_t codec);
	uint8_t(*const rtp_bridgePeers) (PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, struct ast_frame ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms);

	/* callerid */
	int (*const get_callerid_name) (const sccp_channel_t * channel, char **cid_name);
	int (*const get_callerid_number) (const sccp_channel_t * channel, char **cid_number);
	int (*const get_callerid_ton) (const sccp_channel_t * channel, char **ton);
	int (*const get_callerid_ani) (const sccp_channel_t * channel, char **ani);
	int (*const get_callerid_subaddr) (const sccp_channel_t * channel, char **subaddr);
	int (*const get_callerid_dnid) (const sccp_channel_t * channel, char **dnid);
	int (*const get_callerid_rdnis) (const sccp_channel_t * channel, char **rdnis);
	int (*const get_callerid_presence) (const sccp_channel_t * channel);

	void (*const set_callerid_name) (const sccp_channel_t * channel, const char *name);
	void (*const set_callerid_number) (const sccp_channel_t * channel, const char *name);
	void (*const set_callerid_ani) (const sccp_channel_t * channel, const char *ani);
	void (*const set_callerid_dnid) (const sccp_channel_t * channel, const char *dnid);
	void (*const set_callerid_redirectingParty) (const sccp_channel_t * channel, const char *number, const char *name);
	void (*const set_callerid_redirectedParty) (const sccp_channel_t * channel, const char *number, const char *name);
	void (*const set_callerid_presence) (const sccp_channel_t * channel);
	void (*const set_connected_line) (const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason);

	/* feature section */
	sccp_parkresult_t(*const feature_park) (const sccp_channel_t * hostChannel);
	boolean_t(*const feature_stopMusicOnHold) (const sccp_channel_t * channel);
	boolean_t(*const feature_addToDatabase) (const char *family, const char *key, const char *value);
	boolean_t(*const feature_getFromDatabase) (const char *family, const char *key, char *out, int outlen);
	boolean_t(*const feature_removeFromDatabase) (const char *family, const char *key);
	boolean_t(*const feature_removeTreeFromDatabase) (const char *family, const char *key);
	boolean_t(*const getFeatureExtension) (const sccp_channel_t * channel, char **featureExtension);
	boolean_t(*const feature_pickup) (const sccp_channel_t *chan, struct ast_channel *target);

	void *(*const eventSubscribe)(const sccp_channel_t * channel, char **featureExtension);
	PBX_CHANNEL_TYPE *(*const findChannelByCallback)(int(*const found_cb)(PBX_CHANNEL_TYPE *c, void *data), void *data, boolean_t lock);

	int(*const moh_start) (const PBX_CHANNEL_TYPE * pbx_channel, const char *mclass, const char* interpclass);
	void(*const moh_stop) (const PBX_CHANNEL_TYPE * pbx_channel);
	int(*const queue_control) (const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control);
	int(*const queue_control_data) (const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen);

	/* *INDENT-ON* */
};

#endif
