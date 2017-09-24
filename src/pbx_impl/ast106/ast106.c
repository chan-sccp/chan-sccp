/*!
 * \file        ast106.c
 * \brief       SCCP PBX Asterisk Wrapper Class
 * \author      Marcello Ceshia
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */

#include "config.h"
#include "common.h"
#include "chan_sccp.h"
#include "sccp_pbx.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_cli.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_hint.h"
#include "sccp_mwi.h"
#include "sccp_appfunctions.h"
#include "sccp_management.h"
#include "sccp_netsock.h"
#include "sccp_rtp.h"
#include "sccp_session.h"		// required for sccp_session_getOurIP
#include "sccp_labels.h"
#include "ast106.h"

SCCP_FILE_VERSION(__FILE__, "");

__BEGIN_C_EXTERN__
#ifdef HAVE_PBX_ACL_H
#  include <asterisk/acl.h>
#endif
#include <asterisk/module.h>
#include <asterisk/callerid.h>
#include <asterisk/causes.h>
#include <asterisk/musiconhold.h>
#ifdef HAVE_PBX_FEATURES_H
#  include <asterisk/features.h>
#endif
#include <asterisk/indications.h>
#include <asterisk/netsock.h>
#include <asterisk/translate.h>

#define new avoid_cxx_new_keyword
#include <asterisk/rtp.h>
#undef new
#include <asterisk/timing.h>

#if HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif

#ifndef CS_AST_RTP_INSTANCE_NEW
#define ast_rtp_instance_read(_x, _y) ast_rtp_read(_x)
#endif

__END_C_EXTERN__
static struct sched_context *sched = 0;
static struct io_context *io = 0;

#define SCCP_AST_LINKEDID_HELPER "LINKEDID"

//static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, format_t format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause);
static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, int format, void *data, int *cause);
static int sccp_wrapper_asterisk16_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout);
static int sccp_wrapper_asterisk16_answer(PBX_CHANNEL_TYPE * chan);
static PBX_FRAME_TYPE *sccp_wrapper_asterisk16_rtp_read(PBX_CHANNEL_TYPE * ast);
static int sccp_wrapper_asterisk16_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame);
static int sccp_wrapper_asterisk16_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen);
static int sccp_wrapper_asterisk16_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan);

//static enum ast_bridge_result sccp_wrapper_asterisk16_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms);
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE * ast, const char *text);
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE * ast, char digit);
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE * ast, char digit, unsigned int duration);
static int sccp_pbx_sendHTML(PBX_CHANNEL_TYPE * ast, int subclass, const char *data, int datalen);
int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel);
int sccp_asterisk_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control);
int sccp_asterisk_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen);
boolean_t sccp_asterisk_getForwarderPeerChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel);
static int sccp_wrapper_asterisk16_devicestate(void *data);
PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE * chan, const char *exten, const char *context);
PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByGroupLocked(PBX_CHANNEL_TYPE * chan);
#if defined(__cplusplus) || defined(c_plusplus)

/*!
 * \brief SCCP Tech Structure
 */
static struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */
	type:			SCCP_TECHTYPE_STR,
	description:		"Skinny Client Control Protocol (SCCP)",
	capabilities:		AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_SLINEAR16 | AST_FORMAT_GSM | AST_FORMAT_G723_1 | AST_FORMAT_G729A | AST_FORMAT_H264 | AST_FORMAT_H263_PLUS,
	properties:		AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	requester:		sccp_wrapper_asterisk16_request,
	devicestate:		sccp_wrapper_asterisk16_devicestate,
	send_digit_begin:	sccp_wrapper_recvdigit_begin,
	send_digit_end:		sccp_wrapper_recvdigit_end,
	call:			sccp_wrapper_asterisk16_call,
	hangup:			sccp_wrapper_asterisk16_hangup,
	answer:			sccp_wrapper_asterisk16_answer,
	read:			sccp_wrapper_asterisk16_rtp_read,
	write:			sccp_wrapper_asterisk16_rtp_write,
	send_text:		sccp_pbx_sendtext,
	send_image:		NULL,
	send_html:		sccp_pbx_sendHTML,
	exception:		NULL,
//	bridge:			sccp_wrapper_asterisk16_rtpBridge,
	bridge:			ast_rtp_bridge,
	early_bridge:		NULL,
	indicate:		sccp_wrapper_asterisk16_indicate,
	fixup:			sccp_wrapper_asterisk16_fixup,
	setoption:		NULL,
	queryoption:		NULL,
	transfer:		NULL,
	write_video:		sccp_wrapper_asterisk16_rtp_write,
	write_text:		NULL,
	bridged_channel:	NULL,
	func_channel_read:	sccp_wrapper_asterisk_channel_read,
	func_channel_write:	sccp_asterisk_pbx_fktChannelWrite,
	get_base_channel:	NULL,
	set_base_channel:	NULL,
	/* *INDENT-ON* */ 
};

#else

/*!
 * \brief SCCP Tech Structure
 */
const struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */
	.type 			= SCCP_TECHTYPE_STR,
	.description 		= "Skinny Client Control Protocol (SCCP)",
	// we could use the skinny_codec = ast_codec mapping here to generate the list of capabilities
	.capabilities 		= AST_FORMAT_SLINEAR16 | AST_FORMAT_SLINEAR | AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_GSM | AST_FORMAT_G723_1 | AST_FORMAT_G729A,
	.properties 		= AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester 		= sccp_wrapper_asterisk16_request,
	.devicestate 		= sccp_wrapper_asterisk16_devicestate,
	.call 			= sccp_wrapper_asterisk16_call,
	.hangup 		= sccp_wrapper_asterisk16_hangup,
	.answer 		= sccp_wrapper_asterisk16_answer,
	.read 			= sccp_wrapper_asterisk16_rtp_read,
	.write 			= sccp_wrapper_asterisk16_rtp_write,
	.write_video 		= sccp_wrapper_asterisk16_rtp_write,
	.indicate 		= sccp_wrapper_asterisk16_indicate,
	.fixup 			= sccp_wrapper_asterisk16_fixup,
	//.transfer 		= sccp_pbx_transfer,
//	.bridge 		= sccp_wrapper_asterisk16_rtpBridge,
	.bridge			= ast_rtp_bridge,
	//.early_bridge         = ast_rtp_early_bridge,
	//.bridged_channel      =

	.send_text 		= sccp_pbx_sendtext,
	.send_html 		= sccp_pbx_sendHTML,
	//.send_html            =
	//.send_image           =

	.func_channel_read 	= sccp_wrapper_asterisk_channel_read,
	.func_channel_write 	= sccp_asterisk_pbx_fktChannelWrite,

	.send_digit_begin 	= sccp_wrapper_recvdigit_begin,
	.send_digit_end 	= sccp_wrapper_recvdigit_end,

	//.write_text           = 
	//.write_video          =
	//.cc_callback          =                                              // ccss, new >1.6.0
	//.exception            =                                              // new >1.6.0
//      .setoption              = sccp_wrapper_asterisk16_setOption,
	//.queryoption          =                                              // new >1.6.0
	//.get_pvt_uniqueid     = sccp_pbx_get_callid,                         // new >1.6.0
	//.get_base_channel     =
	//.set_base_channel     =
	/* *INDENT-ON* */
};

#endif

static int sccp_wrapper_asterisk16_devicestate(void *data)
{
	int res = AST_DEVICE_UNKNOWN;
	char *lineName = (char *) data;
	char *deviceId = NULL;
	sccp_channelstate_t state;

	if ((deviceId = strchr(lineName, '@'))) {
		*deviceId = '\0';
		deviceId++;
	}

	state = sccp_hint_getLinestate(lineName, deviceId);
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_asterisk_devicestate) sccp_hint returned state:%s for '%s'\n", sccp_channelstate2str(state), lineName);
	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
		case SCCP_CHANNELSTATE_ONHOOK:
			res = AST_DEVICE_NOT_INUSE;
			break;
		case SCCP_CHANNELSTATE_RINGING:
#ifdef CS_AST_DEVICE_RINGING
			res = AST_DEVICE_RINGING;
			break;
#endif
		case SCCP_CHANNELSTATE_HOLD:
			res = AST_DEVICE_INUSE;
			break;
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
			res = AST_DEVICE_INVALID;
			break;
		case SCCP_CHANNELSTATE_BUSY:
			res = AST_DEVICE_BUSY;
			break;
		case SCCP_CHANNELSTATE_DND:
			res = AST_DEVICE_BUSY;
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
#ifndef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_ZOMBIE:
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
#endif		
			res = AST_DEVICE_UNAVAILABLE;
			break;

		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_RINGOUT_ALERTING:
#ifdef CS_EXPERIMENTAL
			res = AST_DEVICE_RINGINUSE;
			break;
#endif
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
		case SCCP_CHANNELSTATE_PROGRESS:
#ifndef CS_EXPERIMENTAL
#ifdef CS_AST_DEVICE_RINGING
			res = AST_DEVICE_RINGING;
			break;
#endif
#endif
		case SCCP_CHANNELSTATE_CALLWAITING:
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
		case SCCP_CHANNELSTATE_OFFHOOK:
		case SCCP_CHANNELSTATE_GETDIGITS:
		case SCCP_CHANNELSTATE_CONNECTED:
		case SCCP_CHANNELSTATE_PROCEED:
		case SCCP_CHANNELSTATE_BLINDTRANSFER:
		case SCCP_CHANNELSTATE_CALLTRANSFER:
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
		case SCCP_CHANNELSTATE_CALLPARK:
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			res = AST_DEVICE_INUSE;
			break;
		case SCCP_CHANNELSTATE_SENTINEL:
#ifdef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
		case SCCP_CHANNELSTATE_ZOMBIE:
			res = AST_DEVICE_UNKNOWN;
#endif
			break;
	}

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_asterisk_devicestate) PBX  requests state for '%s' - state %s\n", lineName, ast_devstate2str(res));
	return res;
}

/*!
 * \brief Convert an array of skinny_codecs (enum) to ast_codec_prefs
 *
 * \param skinny_codecs Array of Skinny Codecs
 * \param astCodecPref Array of PBX Codec Preferences
 *
 * \return bit array fmt/Format of ast_format_type (int)
 */
int skinny_codecs2pbx_codec_pref(const skinny_codec_t * const codecs, struct ast_codec_pref *astCodecPref)
{
	uint32_t i;
	int res_codec = 0;

	for (i = 1; i < SKINNY_MAX_CAPABILITIES; i++) {
		if (codecs[i]) {
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "adding codec to ast_codec_pref\n");
			res_codec |= ast_codec_pref_append(astCodecPref, skinny_codec2pbx_codec(codecs[i]));
		}
	}
	return res_codec;
}

static boolean_t sccp_wrapper_asterisk16_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec);

#define RTP_NEW_SOURCE(_c,_log) 								\
        if(c->rtp.audio.instance) { 										\
                ast_rtp_new_source(c->rtp.audio.instance); 							\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }

#define RTP_CHANGE_SOURCE(_c,_log) 								\
        if(c->rtp.audio.instance) {										\
                ast_rtp_change_source(c->rtp.audio.instance);						\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }

static void get_skinnyFormats(format_t format, skinny_codec_t codecs[], size_t size)
{
/*
	unsigned int x;
	unsigned len = 0;

	if (!size) {
		return;
	}
	for (x = 0; x < ARRAY_LEN(pbx2skinny_codec_maps) && len <= size; x++) {
		if (pbx2skinny_codec_maps[x].pbx_codec & format) {
			codecs[len++] = pbx2skinny_codec_maps[x].skinny_codec;
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "map ast codec " UI64FMT " to %d\n", (ULONG) (pbx2skinny_codec_maps[x].pbx_codec & format), pbx2skinny_codec_maps[x].skinny_codec);
		}
	}
*/
        if (!size) {
                return;
        }
	uint8_t position = 0;
	skinny_codec_t found = pbx_codec2skinny_codec(format);
	if((found = pbx_codec2skinny_codec(format)) != SKINNY_CODEC_NONE) {
		codecs[position++] = found;
	}
}

/*************************************************************************************************************** CODEC **/

/*! \brief Get the name of a format
 * \note replacement for ast_getformatname
 * \param format id of format
 * \return A static string containing the name of the format or "unknown" if unknown.
 */
char *pbx_getformatname(format_t format)
{
	return ast_getformatname(format);
}

/*!
 * \brief Get the names of a set of formats
 * \note replacement for ast_getformatname_multiple
 * \param buf a buffer for the output string
 * \param size size of buf (bytes)
 * \param format the format (combined IDs of codecs)
 * Prints a list of readable codec names corresponding to "format".
 * ex: for format=AST_FORMAT_GSM|AST_FORMAT_SPEEX|AST_FORMAT_ILBC it will return "0x602 (GSM|SPEEX|ILBC)"
 * \return The return value is buf.
 */
char *pbx_getformatname_multiple(char *buf, size_t size, format_t format)
{
	return ast_getformatname_multiple(buf, size, format & AST_FORMAT_AUDIO_MASK);
}

/*!
 * \brief       start monitoring thread of chan_sccp
 * \param       data
 *
 * \lock
 *      - monitor_lock
 */
static void *sccp_do_monitor(void *data)
{
	int res;

	/* This thread monitors all the interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for (;;) {
		pthread_testcancel();
		/* Wait for sched or io */
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000)) {
			res = 1000;
		}
		res = ast_io_wait(io, res);
		if (res > 20) {
			ast_debug(1, "SCCP: ast_io_wait ran %d all at once\n", res);
		}
		pbx_mutex_lock(&GLOB(monitor_lock));

		res = ast_sched_runq(sched);
		if (res >= 20) {
			ast_debug(1, "SCCP: ast_sched_runq ran %d all at once\n", res);
		}
		pbx_mutex_unlock(&GLOB(monitor_lock));

		if (GLOB(monitor_thread) == AST_PTHREADT_STOP) {
			return 0;
		}
	}
	/* Never reached */
	return NULL;
}

static int sccp_restart_monitor()
{
	/* If we're supposed to be stopped -- stay stopped */
	if (GLOB(monitor_thread) == AST_PTHREADT_STOP) {
		return 0;
	}
	pbx_mutex_lock(&GLOB(monitor_lock));
	if (GLOB(monitor_thread) == pthread_self()) {
		pbx_mutex_unlock(&GLOB(monitor_lock));
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Cannot kill myself\n");
		return -1;
	}
	if (GLOB(monitor_thread) != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(GLOB(monitor_thread), SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&GLOB(monitor_thread), NULL, sccp_do_monitor, NULL) < 0) {
			pbx_mutex_unlock(&GLOB(monitor_lock));
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Unable to start monitor thread.\n");
			return -1;
		}
	}
	pbx_mutex_unlock(&GLOB(monitor_lock));
	return 0;
}

/*!
 * \brief Read from an Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 *
 * \called_from_asterisk
 *
 * \note not following the refcount rules... channel is already retained
 */
static PBX_FRAME_TYPE *sccp_wrapper_asterisk16_rtp_read(PBX_CHANNEL_TYPE * ast)
{
	//AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));				// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	PBX_FRAME_TYPE *frame = NULL;

	//if (!(c) {
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {									// not following the refcount rules... channel is already retained
		return &ast_null_frame;
	}

	if (!c->rtp.audio.instance) {
		return &ast_null_frame;
	}

	switch (ast->fdno) {

		case 0:
			frame = ast_rtp_instance_read(c->rtp.audio.instance, 0);					/* RTP Audio */
			break;
		case 1:
			frame = ast_rtp_instance_read(c->rtp.audio.instance, 1);					/* RTCP Control Channel */
			break;
#ifdef CS_SCCP_VIDEO
		case 2:
			frame = ast_rtp_instance_read(c->rtp.video.instance, 0);					/* RTP Video */
			break;
		case 3:
			frame = ast_rtp_instance_read(c->rtp.video.instance, 1);					/* RTCP Control Channel for video */
			break;
#endif
		default:
			break;
	}

	if (!frame) {
		pbx_log(LOG_WARNING, "%s: error reading frame == NULL\n", c->currentDeviceId);
		return &ast_null_frame;
	} 
	//sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: read format: ast->fdno: %d, frametype: %d, %s(%d)\n", DEV_ID_LOG(c->device), ast->fdno, frame->frametype, pbx_getformatname(frame->subclass), frame->subclass);
	if (frame->frametype == AST_FRAME_VOICE) {
#ifdef CS_SCCP_CONFERENCE
		if (c->conference && ast->readformat == AST_FORMAT_SLINEAR) {
			ast_set_read_format(ast, AST_FORMAT_SLINEAR);
		}
#endif
	}
	
	return frame;
}

static const char *asterisk_indication2str(int ind)
{
	switch (ind) {
		case AST_CONTROL_HANGUP: return "AST_CONTROL_HANGUP: Other end has hungup";
		case AST_CONTROL_RING: return "AST_CONTROL_RING: Local ring";
		case AST_CONTROL_RINGING: return "AST_CONTROL_RINGING: Remote end is ringing";
		case AST_CONTROL_ANSWER: return "AST_CONTROL_ANSWER: Remote end has answered";
		case AST_CONTROL_BUSY: return "AST_CONTROL_BUSY: Remote end is busy";
		case AST_CONTROL_TAKEOFFHOOK: return "AST_CONTROL_TAKEOFFHOOK: Make it go off hook";
		case AST_CONTROL_OFFHOOK: return "AST_CONTROL_OFFHOOK: Line is off hook";
		case AST_CONTROL_CONGESTION: return "AST_CONTROL_CONGESTION: Congestion (circuits busy)";
		case AST_CONTROL_FLASH: return "AST_CONTROL_FLASH: Flash hook";
		case AST_CONTROL_WINK: return "AST_CONTROL_WINK: Wink";
		case AST_CONTROL_OPTION: return "AST_CONTROL_OPTION: Set a low-level option";
		case AST_CONTROL_RADIO_KEY: return "AST_CONTROL_RADIO_KEY: Key Radio";
		case AST_CONTROL_RADIO_UNKEY: return "AST_CONTROL_RADIO_UNKEY: Un-Key Radio";
		case AST_CONTROL_PROGRESS: return "AST_CONTROL_PROGRESS: Indicate PROGRESS";
		case AST_CONTROL_PROCEEDING: return "AST_CONTROL_PROCEEDING: Indicate CALL PROCEEDING";
		case AST_CONTROL_HOLD: return "AST_CONTROL_HOLD: Indicate call is placed on hold";
		case AST_CONTROL_UNHOLD: return "AST_CONTROL_UNHOLD: Indicate call left hold";
		case AST_CONTROL_VIDUPDATE: return "AST_CONTROL_VIDUPDATE: Indicate video frame update";
		case _XXX_AST_CONTROL_T38: return "_XXX_AST_CONTROL_T38: T38 state change request/notification. Deprecated This is no longer supported. Use AST_CONTROL_T38_PARAMETERS instead.";
		case AST_CONTROL_SRCUPDATE: return "AST_CONTROL_SRCUPDATE: Indicate source of media has changed";
		case AST_CONTROL_T38_PARAMETERS: return "AST_CONTROL_T38_PARAMETERS: T38 state change request/notification with parameters";
		case AST_CONTROL_SRCCHANGE: return "AST_CONTROL_SRCCHANGE: Media source has changed and requires a new RTP SSRC";
		case -1: return "AST_CONTROL_PROD: Kick remote channel";
	}
	return "Unknown/Unhandled/IAX Indication";
}

static int sccp_wrapper_asterisk16_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
{
	int res = 0;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (!c) {
		return -1;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (!d) {
		/* handle forwarded channel indications */
		if (c->parentChannel) {
			PBX_CHANNEL_TYPE *bridgePeer;

			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: forwarding indication %d to parentChannel %s (FORWARDED_FOR: %s)\n", ind, c->parentChannel->owner->name, pbx_builtin_getvar_helper(c->parentChannel->owner, "FORWARDER_FOR"));
			if (sccp_asterisk_getForwarderPeerChannel(c->parentChannel, &bridgePeer)) {
				sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: forwarding indication %d to caller %s\n", ind, bridgePeer->name);
				res = sccp_wrapper_asterisk16_indicate(bridgePeer, ind, data, datalen);
				ast_channel_unlock(bridgePeer);
			} else {
				sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: cannot forward indication %d. bridgePeer not found\n", ind);
			}
		} else {
			ast_log(LOG_WARNING, "SCCP: (sccp_wrapper_asterisk16_indicate) Could not find device to handle indication, giving up.\n");
			res = -1;
		}
		return res;
	}

	if (c->state == SCCP_CHANNELSTATE_DOWN) {
		return -1;											/* Tell asterisk to provide inband signalling */
	}

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = (((c->rtp.audio.receiveChannelState != SCCP_RTP_STATUS_INACTIVE) || (d && d->earlyrtp)) ? -1 : 0);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: (pbx_indicate) start indicate '%s' (%d) condition on channel %s (readStat:%d, receiveChannelState:%d, rtp:%s, default res:%s (%d))\n", DEV_ID_LOG(d), asterisk_indication2str(ind), ind, pbx_channel_name(ast), c->rtp.audio.receiveChannelState, c->rtp.audio.receiveChannelState, (c->rtp.audio.instance) ? "yes" : "no", res ? "inband signaling" : "outofband signaling", res);

	switch (ind) {
		case AST_CONTROL_RINGING:
			if (SKINNY_CALLTYPE_OUTBOUND == c->calltype) {
				// Allow signalling of RINGOUT only on outbound calls.
				// Otherwise, there are some issues with late arrival of ringing
				// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
				sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGOUT);
				if (d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE) {
					/* 
					 * Redial button isnt't working properly in immediate mode, because the
					 * last dialed number was being remembered too early. This fix
					 * remembers the last dialed number in the same cases, where the dialed number
					 * is being sent - after receiving of RINGOUT -Pavel Troller
					 */
					AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, c->line));
					if(linedevice){ 
						sccp_device_setLastNumberDialed(d, c->dialedNumber, linedevice);
					}
				}
				iPbx.set_callstate(c, AST_STATE_RING);
			}
			break;
		case AST_CONTROL_BUSY:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_BUSY);
			iPbx.set_callstate(c, AST_STATE_BUSY);
			break;
		case AST_CONTROL_CONGESTION:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
			break;
		case AST_CONTROL_PROGRESS:
			if (c->state != SCCP_CHANNELSTATE_CONNECTED && c->previousChannelState != SCCP_CHANNELSTATE_CONNECTED) {
				sccp_indicate(d, c, SCCP_CHANNELSTATE_PROGRESS);
			} else {
				// ORIGINATE() to SIP indicates PROGRESS after CONNECTED, causing issues with transfer
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
			}
			res = -1;
			break;
		case AST_CONTROL_PROCEEDING:
			if (d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE) {
				/* 
					* Redial button isnt't working properly in immediate mode, because the
					* last dialed number was being remembered too early. This fix
					* remembers the last dialed number in the same cases, where the dialed number
					* is being sent - after receiving of PROCEEDING -Pavel Troller
					*/
				AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, c->line));
				if(linedevice){ 
					sccp_device_setLastNumberDialed(d, c->dialedNumber, linedevice);
				}
			}
			sccp_indicate(d, c, SCCP_CHANNELSTATE_PROCEED);
			res = -1;
			break;
		case AST_CONTROL_SRCCHANGE:
			if (c->rtp.audio.instance) {
				ast_rtp_new_source(c->rtp.audio.instance);
			}
			if (c->calltype == SKINNY_CALLTYPE_OUTBOUND && c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE && c->state > SCCP_CHANNELSTATE_DIALING) {
				sccp_channel_openReceiveChannel(c);
			}
			res = 0;
			break;

		case AST_CONTROL_SRCUPDATE:
			/* Source media has changed. */
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

			if (c->rtp.audio.instance) {
				ast_rtp_change_source(c->rtp.audio.instance);
			}
			res = 0;
			break;

			/* when the bridged channel hold/unhold the call we are notified here */
		case AST_CONTROL_HOLD:
			sccp_asterisk_moh_start(ast, (const char *) data, c->musicclass);
			res = 0;
			break;
		case AST_CONTROL_UNHOLD:
			sccp_asterisk_moh_stop(ast);
			if (c->rtp.audio.instance) {
				ast_rtp_new_source(c->rtp.audio.instance);
			}
			res = 0;
			break;

		case AST_CONTROL_VIDUPDATE:									/* Request a video frame update */
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.instance && d && sccp_device_isVideoSupported(d) && c->videomode != SCCP_VIDEO_MODE_OFF) {
				d->protocol->sendFastPictureUpdate(d, c);
				res = 0;
			} else 
#endif
			{
				res = -1;
			}
			break;
#ifdef CS_AST_CONTROL_INCOMPLETE
		case AST_CONTROL_INCOMPLETE:									/*!< Indication that the extension dialed is incomplete */
			/* \todo implement dial continuation by:
			 *  - display message incomplete number
			 *  - adding time to channel->scheduler.digittimeout
			 *  - rescheduling sccp_pbx_sched_dial 
			 */
			if (d->earlyrtp != SCCP_EARLYRTP_IMMEDIATE) {
				if (!c->scheduler.deny) {
					sccp_indicate(d, c, SCCP_CHANNELSTATE_DIGITSFOLL);
					sccp_channel_schedule_digittimout(c, c->enbloc.digittimeout);
				} else {
					sccp_channel_stop_schedule_digittimout(c);
					sccp_indicate(d, c, SCCP_CHANNELSTATE_ONHOOK);
				}
			}
			res = 0;
			break;
#endif
		case -1:											// Asterisk prod the channel
			if (	c->line && 
				c->state > SCCP_GROUPED_CHANNELSTATE_DIALING && 
				c->calltype == SKINNY_CALLTYPE_OUTBOUND && 
				c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE &&
				!ast->hangupcause
			) {
				sccp_channel_openReceiveChannel(c);
				uint8_t instance = sccp_device_find_index_for_line(d, c->line->name);
				sccp_dev_stoptone(d, instance, c->callid);
			}
			res = -1;
			break;
		default:
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: (pbx_indicate) Don't know how to indicate condition '%s' (%d)\n", DEV_ID_LOG(d), asterisk_indication2str(ind), ind);
			break;
	}
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_2 "%s: (pbx_indicate) finish: send indication '%s' (%d)\n", DEV_ID_LOG(d), res ? "inband signaling" : "outofband signaling", res);
	return res;
}

/*!
 * \brief Write to an Asterisk Channel
 * \param ast Channel as ast_channel
 * \param frame Frame as ast_frame
 *
 * \called_from_asterisk
 *
 * \note not following the refcount rules... channel is already retained
 */
static int sccp_wrapper_asterisk16_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame)
{
	int res = 0;
	//AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast)); 				// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		return -1;
	}

	switch (frame->frametype) {
		case AST_FRAME_VOICE:
			// checking for samples to transmit
			if (!frame->samples) {
				if (strcasecmp(frame->src, "ast_prod")) {
					pbx_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", c->currentDeviceId, (int) frame->frametype);
				} else {
					// frame->samples == 0  when frame_src is ast_prod
					sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", c->currentDeviceId, ast->name);
					if (c->parentChannel) {
						sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: forwarding prodding to parentChannel %s\n", c->parentChannel->owner->name);
						sccp_wrapper_asterisk16_rtp_write(c->parentChannel->owner, frame);
					}
				}
			} else if (!(frame->subclass & ast->nativeformats)) {
				// char s1[512], s2[512], s3[512];
				// sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", c->currentDeviceId, frame->subclass, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (uint64_t)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (uint64_t)ast->writeformat), (uint64_t)ast->writeformat);
				// //! \todo correct debugging
				// sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", c->currentDeviceId, (int)frame->frametype, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (uint64_t)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (uint64_t)ast->writeformat), (uint64_t)ast->writeformat);
				//return -1;
			}
#if 0
			if ((ast->rawwriteformat = ast->writeformat) && ast->writetrans) {
				ast_translator_free_path(ast->writetrans);
				ast->writetrans = NULL;

				ast_set_write_format(ast, frame->subclass);
			}
#endif
			if (c->rtp.audio.instance) {
				res = ast_rtp_write(c->rtp.audio.instance, frame);
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.receiveChannelState == SCCP_RTP_STATUS_INACTIVE && c->rtp.video.instance && c->state != SCCP_CHANNELSTATE_HOLD) {
				int codec = pbx_codec2skinny_codec((frame->subclass & AST_FORMAT_VIDEO_MASK));

				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: got video frame %d\n", c->currentDeviceId, codec);
				if (0 != codec) {
					c->rtp.video.writeFormat = codec;
					sccp_channel_openMultiMediaReceiveChannel(c);
				}
			}

			if (c->rtp.video.instance && (c->rtp.video.receiveChannelState & SCCP_RTP_STATUS_ACTIVE) != 0) {
				res = ast_rtp_write(c->rtp.video.instance, frame);
			}
#endif
			break;
		case AST_FRAME_TEXT:
		case AST_FRAME_MODEM:
		default:
			pbx_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %s\n", c->currentDeviceId, frame->frametype, ast->name);
			break;
	}
	return res;
}

static int sccp_wrapper_asterisk16_sendDigits(const sccp_channel_t * channel, const char *digits)
{
	if (!channel || !channel->owner) {
		pbx_log(LOG_WARNING, "No channel to send digits to\n");
		return 0;
	}
	// ast_channel_undefer_dtmf(channel->owner);
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int i;
	PBX_FRAME_TYPE f;

	f = ast_null_frame;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digits '%s'\n", (char *) channel->currentDeviceId, digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	for (i = 0; digits[i] != '\0'; i++) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", (char *) channel->currentDeviceId, digits[i]);

		f.frametype = AST_FRAME_DTMF_END;								// Sending only the dmtf will force asterisk to start with DTMF_BEGIN and schedule the DTMF_END
		f.subclass = digits[i];
		f.len = SCCP_MIN_DTMF_DURATION;
		f.src = "SEND DIGIT";
		ast_queue_frame(pbx_channel, &f);
	}
	return 1;
}

static int sccp_wrapper_asterisk16_sendDigit(const sccp_channel_t * channel, const char digit)
{
	char digits[3] = "\0\0";

	digits[0] = digit;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: got a single digit '%c' -> '%s'\n", channel->currentDeviceId, digit, digits);
	return sccp_wrapper_asterisk16_sendDigits(channel, digits);
}

static void sccp_wrapper_asterisk16_setCalleridPresentation(PBX_CHANNEL_TYPE * pbxChannel, sccp_callerid_presentation_t presentation)
{
	if (pbxChannel && CALLERID_PRESENTATION_FORBIDDEN == presentation) {
		pbxChannel->cid.cid_pres |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
	}
}

static int sccp_wrapper_asterisk16_setNativeAudioFormats(const sccp_channel_t * channel, skinny_codec_t codec[], int length)
{

	format_t new_nativeformats = 0;
	int i;
	if (!channel || !channel->owner || !channel->owner->nativeformats) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_wrapper_asterisk111_setNativeAudioFormats) no channel provided!\n");
		return 0;
	}

	ast_debug(10, "%s: set native Formats length: %d\n", (char *) channel->currentDeviceId, length);

	for (i = 0; i < length; i++) {
		new_nativeformats |= skinny_codec2pbx_codec(codec[i]);
		ast_debug(10, "%s: set native Formats to %d, skinny: %d\n", (char *) channel->currentDeviceId, (int) channel->owner->nativeformats, codec[i]);
	}
	if (channel->owner->nativeformats != new_nativeformats) {
		channel->owner->nativeformats = new_nativeformats;
		char codecs[512];

		sccp_codec_multiple2str(codecs, sizeof(codecs) - 1, codec, length);
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_2 "%s: updated native Formats to %d, length: %d, skinny: [%s]\n", (char *) channel->currentDeviceId, (int) channel->owner->nativeformats, length, codecs);
	}
	return 1;
}

static int sccp_wrapper_asterisk16_setNativeVideoFormats(const sccp_channel_t * channel, uint32_t formats)
{
	return 1;
}

static void sccp_wrapper_asterisk106_removeTimingFD(PBX_CHANNEL_TYPE *ast)
{
	if (ast) {
		struct ast_timer *timer=ast->timer;
		if (timer) {
			//ast_log(LOG_NOTICE, "%s: (clean_timer_fds) timername: %s, fd:%d\n", ast->name, ast_timer_get_name(timer), ast_timer_fd(timer));
			ast_timer_disable_continuous(timer);
			ast_timer_close(timer);
			ast_channel_set_fd(ast, AST_TIMING_FD, -1);
		}
	}
}

static void sccp_wrapper_asterisk106_setOwner(sccp_channel_t * channel, PBX_CHANNEL_TYPE * pbx_channel)
{
	channel->owner = pbx_channel;
}


static boolean_t sccp_wrapper_asterisk16_allocPBXChannel(sccp_channel_t * channel, const void *ids, const PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** _pbxDstChannel)
{
	// const char *linkedId = ids ? pbx_strdupa(ids) : NULL;
	PBX_CHANNEL_TYPE *pbxDstChannel = NULL;
	if (!channel || !channel->line) {
		return FALSE;
	}
	AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(channel->line));
	if (!line) {
		return FALSE;
	}

	// if (ids) {
	// 	linkedId = pbx_builtin_getvar_helper(pbxSrcChannel, SCCP_AST_LINKEDID_HELPER));
	// }

	pbxDstChannel = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);

	if (pbxDstChannel == NULL) {
		return FALSE;
	}

	pbxDstChannel->tech = &sccp_tech;
	pbxDstChannel->tech_pvt = sccp_channel_retain(channel);

	/* Copy Codec from SrcChannel */
	if (pbxSrcChannel) {
		pbxDstChannel->nativeformats = pbxSrcChannel->nativeformats;
		pbxDstChannel->rawwriteformat = pbxSrcChannel->rawwriteformat;
		pbxDstChannel->writeformat = pbxSrcChannel->writeformat;
		pbxDstChannel->readformat = pbxSrcChannel->readformat;
		pbxDstChannel->writetrans = pbxSrcChannel->writetrans;
		ast_set_write_format(pbxDstChannel, pbxSrcChannel->rawwriteformat);
	}
	/* EndCodec */

	memset(pbxDstChannel->exten, 0, sizeof(pbxDstChannel->exten));

	sccp_copy_string(pbxDstChannel->context, line->context, sizeof(pbxDstChannel->context));

	if (!sccp_strlen_zero(line->language)) {
		ast_string_field_set(pbxDstChannel, language, line->language);
	}
	if (!sccp_strlen_zero(line->accountcode)) {
		ast_string_field_set(pbxDstChannel, accountcode, line->accountcode);
	}
	if (!sccp_strlen_zero(line->musicclass)) {
		ast_string_field_set(pbxDstChannel, musicclass, line->musicclass);
	}
	if (line->amaflags) {
		pbxDstChannel->amaflags = line->amaflags;
	}
	if (line->callgroup) {
		pbxDstChannel->callgroup = line->callgroup;
	}
#if CS_SCCP_PICKUP
	if (line->pickupgroup) {
		pbxDstChannel->pickupgroup = line->pickupgroup;
	}
#endif
	if (!sccp_strlen_zero(line->parkinglot)) {
		ast_string_field_set(pbxDstChannel, parkinglot, line->parkinglot);
	}
	pbxDstChannel->priority = 1;
	pbxDstChannel->adsicpe = AST_ADSI_UNAVAILABLE;

	/** the the tonezone using language information */
	if (!sccp_strlen_zero(line->language) && ast_get_indication_zone(line->language)) {
		pbxDstChannel->zone = ast_get_indication_zone(line->language);					/* this will core asterisk on hangup */
	}

	/* create/set linkid */
	char linkedid[50];

	snprintf(linkedid, sizeof(linkedid), "SCCP::%-10d", channel->callid);
	if (iPbx.setChannelLinkedId) {
		iPbx.setChannelLinkedId(channel, linkedid);
	}
	/* done */

	sccp_wrapper_asterisk106_setOwner(channel, pbxDstChannel);

	(*_pbxDstChannel) = pbxDstChannel;

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_masqueradeHelper(PBX_CHANNEL_TYPE * pbxChannel, PBX_CHANNEL_TYPE * pbxTmpChannel)
{
	pbx_moh_stop(pbxChannel);
	const char *context;
	const char *exten;
	int priority;

	pbx_moh_stop(pbxChannel);
	ast_channel_lock(pbxChannel);
	context = pbx_strdupa(pbxChannel->context);
	exten = pbx_strdupa(pbxChannel->exten);
	priority = pbxChannel->priority;
	pbx_channel_unlock(pbxChannel);

	/* in older versions we need explicit locking, around the masqueration */
	pbx_channel_lock(pbxChannel);
	if (pbx_channel_masquerade(pbxTmpChannel, pbxChannel)) {
		return FALSE;
	}
	pbx_channel_lock(pbxTmpChannel);
	pbx_do_masquerade(pbxTmpChannel);
	pbx_channel_unlock(pbxTmpChannel);
	pbx_channel_set_hangupcause(pbxChannel, AST_CAUSE_NORMAL_UNSPECIFIED);
	pbx_channel_unlock(pbxChannel);

	/* when returning from bridge, the channel will continue at the next priority */
	ast_explicit_goto(pbxTmpChannel, context, exten, priority + 1);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_allocTempPBXChannel(PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** _pbxDstChannel)
{
	PBX_CHANNEL_TYPE *pbxDstChannel = NULL;

	if (!pbxSrcChannel) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) no pbx channel provided\n");
		return FALSE;
	}
	pbxDstChannel = ast_channel_alloc(0, pbxSrcChannel->_state, 0, 0, pbxSrcChannel->accountcode, pbxSrcChannel->exten, pbxSrcChannel->context, pbxSrcChannel->amaflags, "TMP/%s", pbxSrcChannel->name);
	if (pbxDstChannel == NULL) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) create pbx channel failed\n");
		return FALSE;
	}

	pbxDstChannel->writeformat = pbxSrcChannel->writeformat;
	pbxDstChannel->readformat = pbxSrcChannel->readformat;
	pbx_builtin_setvar_helper(pbxDstChannel, "__" SCCP_AST_LINKEDID_HELPER, pbx_builtin_getvar_helper(pbxSrcChannel, SCCP_AST_LINKEDID_HELPER));

	(*_pbxDstChannel) = pbxDstChannel;
	return TRUE;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_requestAnnouncementChannel(pbx_format_type format, const PBX_CHANNEL_TYPE * requestor, void *data)
{
	PBX_CHANNEL_TYPE *chan;
	int cause;

	if (!(chan = ast_request("Bridge", format, data, &cause))) {
		pbx_log(LOG_ERROR, "SCCP: Requested channel could not be created, cause: %d", cause);
		return NULL;
	}
	return chan;
}

int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel)
{
	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast_channel));
	int res = -1;

	if (c) {
		if (ast_test_flag(ast_channel, AST_FLAG_ANSWERED_ELSEWHERE) || ast_channel->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere\n");
			c->answered_elsewhere = TRUE;
		}
		/* postponing ast_channel_unref to sccp_channel destructor */
		AUTO_RELEASE(sccp_channel_t, channel , sccp_pbx_hangup(c));					/* explicit release from unretained channel returned by sccp_pbx_hangup */
		ast_channel->tech_pvt = NULL;
		(void) channel;											// suppress unused variable warning
	} else {												// after this moment c might have gone already
		ast_channel->tech_pvt = NULL;
		pbx_channel_unref(ast_channel);									// strange unknown channel, why did we get called to hang it up ?
	}
	return res;
}

/*!
 * \brief Parking Thread Arguments Structure
 */
struct parkingThreadArg {
	PBX_CHANNEL_TYPE *bridgedChannel;
	PBX_CHANNEL_TYPE *hostChannel;
	sccp_device_t *device;
};

/*!
 * \brief Channel Park Thread
 * This thread doing the park magic and sends an displaynotify with the park position to the initial device (@see struct parkingThreadArg)
 * 
 * \param data struct parkingThreadArg with host and bridged channel together with initial device
 */
static void *sccp_wrapper_asterisk16_park_thread(void *data)
{
	struct parkingThreadArg *arg;
	PBX_FRAME_TYPE *f;

	char extstr[20];
	int ext;
	int res;

	arg = (struct parkingThreadArg *) data;

	f = ast_read(arg->bridgedChannel);
	if (f) {
		ast_frfree(f);
	}
	res = ast_park_call(arg->bridgedChannel, arg->hostChannel, 0, &ext);

	if (!res) {
		snprintf(extstr, sizeof(extstr), "%c%c %d" , 128, SKINNY_LBL_CALL_PARK_AT, ext);
		

		sccp_dev_displaynotify((sccp_device_t *) arg->device, extstr, 10);
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Parked channel %s on %d\n", DEV_ID_LOG(arg->device), arg->bridgedChannel->name, ext);
	}

	ast_hangup(arg->hostChannel);
	ast_free(arg);

	return NULL;
}

/*!
 * \brief Park the bridge channel of hostChannel
 * This function prepares the host and the bridged channel to be ready for parking.
 * It clones the pbx channel of both sides forward them to the park_thread
 * 
 * \param hostChannel initial channel that request the parking
 * \todo we have a codec issue after unpark a call
 * \todo copy connected line info
 * 
 */
static sccp_parkresult_t sccp_wrapper_asterisk16_park(const sccp_channel_t * hostChannel)
{
	pthread_t th;
	struct parkingThreadArg *arg;
	PBX_CHANNEL_TYPE *pbx_bridgedChannelClone, *pbx_hostChannelClone;
	PBX_CHANNEL_TYPE *bridgedChannel = NULL;

	bridgedChannel = ast_bridged_channel(hostChannel->owner);

	if (!bridgedChannel) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No PBX channel for parking");
		return PARK_RESULT_FAIL;
	}

	pbx_bridgedChannelClone = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, hostChannel->owner->accountcode, bridgedChannel->exten, bridgedChannel->context, bridgedChannel->amaflags, "Parking/%s", bridgedChannel->name);
	pbx_hostChannelClone = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, hostChannel->owner->accountcode, hostChannel->owner->exten, hostChannel->owner->context, hostChannel->owner->amaflags, "SCCP/%s", hostChannel->owner->name);

	if (pbx_hostChannelClone && pbx_bridgedChannelClone) {

		/* restore codecs for bridged channel */
		pbx_bridgedChannelClone->readformat = bridgedChannel->readformat;
		pbx_bridgedChannelClone->writeformat = bridgedChannel->writeformat;
		pbx_bridgedChannelClone->nativeformats = bridgedChannel->nativeformats;
		ast_channel_masquerade(pbx_bridgedChannelClone, bridgedChannel);

		/* restore context data */
		ast_copy_string(pbx_bridgedChannelClone->context, bridgedChannel->context, sizeof(pbx_bridgedChannelClone->context));
		ast_copy_string(pbx_bridgedChannelClone->exten, bridgedChannel->exten, sizeof(pbx_bridgedChannelClone->exten));
		pbx_bridgedChannelClone->priority = bridgedChannel->priority;

		/* restore codecs for host channel */
		/* We need to clone the host part, for playing back the announcement */
		pbx_hostChannelClone->readformat = hostChannel->owner->readformat;
		pbx_hostChannelClone->writeformat = hostChannel->owner->writeformat;
		pbx_hostChannelClone->nativeformats = hostChannel->owner->nativeformats;
		ast_channel_masquerade(pbx_hostChannelClone, hostChannel->owner);

		/* restore context data */
		ast_copy_string(pbx_hostChannelClone->context, hostChannel->owner->context, sizeof(pbx_hostChannelClone->context));
		ast_copy_string(pbx_hostChannelClone->exten, hostChannel->owner->exten, sizeof(pbx_hostChannelClone->exten));
		pbx_hostChannelClone->priority = hostChannel->owner->priority;

		/* start cloning */
		if (ast_do_masquerade(pbx_hostChannelClone)) {
			pbx_log(LOG_ERROR, "while doing masquerade\n");
			ast_hangup(pbx_hostChannelClone);
			return PARK_RESULT_FAIL;
		}
	} else {
		if (pbx_bridgedChannelClone) {
			ast_hangup(pbx_bridgedChannelClone);
		}
		if (pbx_hostChannelClone) {
			ast_hangup(pbx_hostChannelClone);
		}
		return PARK_RESULT_FAIL;
	}
	if (!(arg = (struct parkingThreadArg *) ast_calloc(1, sizeof(struct parkingThreadArg)))) {
		return PARK_RESULT_FAIL;
	}

	arg->bridgedChannel = pbx_bridgedChannelClone;
	arg->hostChannel = pbx_hostChannelClone;
	if ((arg->device = sccp_channel_getDevice(hostChannel))) {
		if (!ast_pthread_create_detached_background(&th, NULL, sccp_wrapper_asterisk16_park_thread, arg)) {
			return PARK_RESULT_SUCCESS;
		}
		sccp_device_release(&arg->device);							/* explicit release */
	}
	return PARK_RESULT_FAIL;

}

#if !CS_AST_DO_PICKUP
static const struct ast_datastore_info pickup_active = {
	.type = "pickup-active",
};

static int ast_do_pickup(PBX_CHANNEL_TYPE * chan, PBX_CHANNEL_TYPE * target)
{
	const char *chan_name;											/*!< A masquerade changes channel names. */
	const char *target_name;										/*!< A masquerade changes channel names. */
	char *target_cid_name = NULL, *target_cid_number = NULL, *target_cid_ani = NULL;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(target));

	int res = -1;

	target_name = pbx_strdupa(target->name);
	ast_debug(1, "Call pickup on '%s' by '%s'\n", target_name, chan->name);

	/* Mark the target to block any call pickup race. */
#if ASTERISK_VERSION_NUMBER > 10600
	struct ast_datastore *ds_pickup;

	ds_pickup = ast_datastore_alloc(&pickup_active, NULL);
	if (!ds_pickup) {
		pbx_log(LOG_WARNING, "Unable to create channel datastore on '%s' for call pickup\n", target_name);
		return -1;
	}
	ast_channel_datastore_add(target, ds_pickup);
#endif

	/* store callerid info to local variables */
	if (c) {
		if (target->cid.cid_name) {
			target_cid_name = pbx_strdup(target->cid.cid_name);
		}
		if (target->cid.cid_num) {
			target_cid_number = pbx_strdup(target->cid.cid_num);
		}
		if (target->cid.cid_ani) {
			target_cid_ani = pbx_strdup(target->cid.cid_ani);
		}
	}
	// copy codec information
	target->nativeformats = chan->nativeformats;
	target->rawwriteformat = chan->rawwriteformat;
	target->writeformat = chan->writeformat;
	target->readformat = chan->readformat;
	target->writetrans = chan->writetrans;
	ast_set_write_format(target, chan->rawwriteformat);
	ast_channel_unlock(target);										/* The pickup race is avoided so we do not need the lock anymore. */

	ast_channel_lock(chan);
	chan_name = pbx_strdupa(chan->name);
	/* exchange callerid info */
	if (c) {
		if (chan && !sccp_strlen_zero(chan->cid.cid_name)) {
			sccp_copy_string(c->oldCallInfo.originalCalledPartyName, chan->cid.cid_name, sizeof(c->oldCallInfo.originalCalledPartyName));
		}
		if (chan && !sccp_strlen_zero(chan->cid.cid_num)) {
			sccp_copy_string(c->oldCallInfo.originalCalledPartyNumber, chan->cid.cid_num, sizeof(c->oldCallInfo.originalCalledPartyNumber));
		}
		if (!sccp_strlen_zero(target_cid_name)) {
			sccp_copy_string(c->oldCallInfo.calledPartyName, target_cid_name, sizeof(c->oldCallInfo.callingPartyName));
			sccp_free(target_cid_name);
		}
		if (!sccp_strlen_zero(target_cid_number)) {
			sccp_copy_string(c->oldCallInfo.calledPartyNumber, target_cid_number, sizeof(c->oldCallInfo.callingPartyNumber));
			sccp_free(target_cid_number);
		}

		/* we use the chan->cid.cid_name to do the magic */
		if (!sccp_strlen_zero(target_cid_ani)) {
			sccp_copy_string(c->oldCallInfo.callingPartyNumber, target_cid_ani, sizeof(c->oldCallInfo.callingPartyNumber));
			sccp_copy_string(c->oldCallInfo.callingPartyName, target_cid_ani, sizeof(c->oldCallInfo.callingPartyName));
			sccp_free(target_cid_ani);
		}
	}
	ast_channel_unlock(chan);

	if (ast_answer(chan)) {
		pbx_log(LOG_WARNING, "Unable to answer '%s'\n", chan_name);
		goto pickup_failed;
	}

	if (sccp_asterisk_queue_control(chan, AST_CONTROL_ANSWER)) {
		pbx_log(LOG_WARNING, "Unable to queue answer on '%s'\n", chan_name);
		goto pickup_failed;
	}

	/* setting this flag to generate a reason header in the cancel message to the ringing channel */
	ast_set_flag(chan, AST_FLAG_ANSWERED_ELSEWHERE);

	if (ast_channel_masquerade(target, chan)) {
		pbx_log(LOG_WARNING, "Unable to masquerade '%s' into '%s'\n", chan_name, target_name);
		goto pickup_failed;
	}

	/* Do the masquerade manually to make sure that it is completed. */
	ast_do_masquerade(target);
	res = 0;

pickup_failed:
	ast_channel_lock(target);
#if ASTERISK_VERSION_NUMBER > 10600
	if (!ast_channel_datastore_remove(target, ds_pickup)) {
		ast_datastore_free(ds_pickup);
	}
#endif

	return res;
}
#endif

static boolean_t sccp_wrapper_asterisk16_getFeatureExtension(const sccp_channel_t * channel, const char *featureName, char extension[SCCP_MAX_EXTENSION])
{
	struct ast_call_feature *feat;

	ast_rdlock_call_features();
	feat = ast_find_call_feature(featureName);
	if (feat) {
		sccp_copy_string(extension, feat->exten, SCCP_MAX_EXTENSION);
	}
	ast_unlock_call_features();

	return feat ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk16_getPickupExtension(const sccp_channel_t * channel, char extension[SCCP_MAX_EXTENSION])
{
	boolean_t res = FALSE;

	if (!sccp_strlen_zero(ast_pickup_ext())) {
		sccp_copy_string(extension, ast_pickup_ext(), SCCP_MAX_EXTENSION);
		res = TRUE;
	}
	return res;
}

static uint8_t sccp_wrapper_asterisk16_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	uint32_t astCodec;
	int payload;

	astCodec = skinny_codec2pbx_codec(codec);
	payload = ast_rtp_lookup_code(rtp->instance, 1, astCodec);

	return payload;
}

static int sccp_wrapper_asterisk16_get_sampleRate(skinny_codec_t codec)
{
#if ASTERISK_VERSION_NUMBER > 10601
	uint32_t astCodec;

	astCodec = skinny_codec2pbx_codec(codec);
	return ast_rtp_lookup_sample_rate(1, astCodec);
#else
	uint32_t i;

	for (i = 1; i < sccp_codec_getArrayLen(); i++) {
		if (skinny_codecs[i].codec == codec) {
			return skinny_codecs[i].sample_rate;
		}
	}
	return 8000;
#endif
}

static sccp_extension_status_t sccp_wrapper_asterisk16_extensionStatus(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

	if (!pbx_channel || sccp_strlen_zero(pbx_channel->context)) {
		pbx_log(LOG_ERROR, "%s: (extension_status) Either no pbx_channel or no valid context provided to lookup number\n", channel->designator);
		return SCCP_EXTENSION_NOTEXISTS;
	}
	int ignore_pat = ast_ignore_pattern(pbx_channel->context, channel->dialedNumber);
	int ext_exist = ast_exists_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_canmatch = ast_canmatch_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_matchmore = ast_matchmore_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);

	/* if we dialed the pickup extention, mark this as exact match */
	const char *pickupexten = ast_pickup_ext();

	if (!sccp_strlen_zero(pickupexten) && sccp_strcaseequals(pickupexten, channel->dialedNumber)) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: pbx extension matcher found pickup extension %s matches dialed number %s\n", channel->dialedNumber, pickupexten);
		ext_exist = 1;
		ext_canmatch = 1;
		ext_matchmore = 0;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "+- pbx extension matcher (%-15s): ---+\n" VERBOSE_PREFIX_3 "|ignore     |exists     |can match  |match more|\n" VERBOSE_PREFIX_3 "|%3s        |%3s        |%3s        |%3s       |\n" VERBOSE_PREFIX_3 "+----------------------------------------------+\n", channel->dialedNumber, ignore_pat ? "yes" : "no", ext_exist ? "yes" : "no", ext_canmatch ? "yes" : "no", ext_matchmore ? "yes" : "no");
	if (ignore_pat) {
		return SCCP_EXTENSION_NOTEXISTS;
	}
	if (ext_exist) {
		if (ext_canmatch && !ext_matchmore) {
			return SCCP_EXTENSION_EXACTMATCH;
		}
		return SCCP_EXTENSION_MATCHMORE;
	}

	return SCCP_EXTENSION_NOTEXISTS;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, int format, void *data, int *cause)
{
	sccp_channel_request_status_t requestStatus;
	PBX_CHANNEL_TYPE *requestor = NULL;

	skinny_codec_t audioCapabilities[SKINNY_MAX_CAPABILITIES];
	skinny_codec_t videoCapabilities[SKINNY_MAX_CAPABILITIES];

	memset(&audioCapabilities, 0, sizeof(audioCapabilities));
	memset(&videoCapabilities, 0, sizeof(videoCapabilities));

	//! \todo parse request
	char *lineName;
	skinny_codec_t codec = SKINNY_CODEC_G711_ULAW_64K;
	sccp_autoanswer_t autoanswer_type = SCCP_AUTOANSWER_NONE;
	uint8_t autoanswer_cause = AST_CAUSE_NOTDEFINED;
	skinny_ringtype_t ringermode = GLOB(ringtype);

	*cause = AST_CAUSE_NOTDEFINED;
	if (!type) {
		pbx_log(LOG_NOTICE, "Attempt to call with unspecified type of channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	}

	if (!data) {
		pbx_log(LOG_NOTICE, "Attempt to call SCCP/ failed\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	}
	/* we leave the data unchanged */
	lineName = pbx_strdup((const char *) data);
	/* parsing options string */
	char *options = NULL;
	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked us to create a channel with type=%s, format=" UI64FMT ", lineName=%s, options=%s\n", type, (uint64_t) format, lineName, (options) ? options : "");
	if (requestor) {
		sccp_parse_alertinfo((PBX_CHANNEL_TYPE *)requestor, &ringermode);
	}
	sccp_parse_dial_options(options, &autoanswer_type, &autoanswer_cause, &ringermode);
	if (autoanswer_cause) {
		*cause = autoanswer_cause;
	}

	/** getting remote capabilities */
	char cap_buf[512];

	/* audio capabilities */
	const struct ast_channel_tech *tech_pvt = ast_get_channel_tech(type);

	if (format) {
		get_skinnyFormats(format & AST_FORMAT_AUDIO_MASK, audioCapabilities, ARRAY_LEN(audioCapabilities));
		get_skinnyFormats(format & AST_FORMAT_VIDEO_MASK, videoCapabilities, ARRAY_LEN(videoCapabilities));
	} else {
		get_skinnyFormats(tech_pvt->capabilities & AST_FORMAT_AUDIO_MASK, audioCapabilities, ARRAY_LEN(audioCapabilities));
		get_skinnyFormats(tech_pvt->capabilities & AST_FORMAT_VIDEO_MASK, videoCapabilities, ARRAY_LEN(videoCapabilities));
	}

	sccp_codec_multiple2str(cap_buf, sizeof(cap_buf) - 1, audioCapabilities, ARRAY_LEN(audioCapabilities));
	// sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote audio caps: %s\n", cap_buf);

	sccp_codec_multiple2str(cap_buf, sizeof(cap_buf) - 1, videoCapabilities, ARRAY_LEN(videoCapabilities));
	// sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote video caps: %s\n", cap_buf);

	/** done */

	/** get requested format */
	codec = pbx_codec2skinny_codec(format);

	AUTO_RELEASE(sccp_channel_t, channel , NULL);
	requestStatus = sccp_requestChannel(lineName, codec, audioCapabilities, ARRAY_LEN(audioCapabilities), autoanswer_type, autoanswer_cause, ringermode, &channel);
	switch (requestStatus) {
		case SCCP_REQUEST_STATUS_SUCCESS:								// everything is fine
			break;
		case SCCP_REQUEST_STATUS_LINEUNKNOWN:
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_4 "SCCP: sccp_requestChannel returned Line %s Unknown -> Not Successfull\n", lineName);
			*cause = AST_CAUSE_DESTINATION_OUT_OF_ORDER;
			goto EXITFUNC;
		case SCCP_REQUEST_STATUS_LINEUNAVAIL:
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_4 "SCCP: sccp_requestChannel returned Line %s not currently registered -> Try again later\n", lineName);
			*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
			goto EXITFUNC;
		case SCCP_REQUEST_STATUS_ERROR:
			pbx_log(LOG_ERROR, "SCCP: sccp_requestChannel returned Status Error for lineName: %s\n", lineName);
			*cause = AST_CAUSE_UNALLOCATED;
			goto EXITFUNC;
		default:
			pbx_log(LOG_ERROR, "SCCP: sccp_requestChannel returned Status Error for lineName: %s\n", lineName);
			*cause = AST_CAUSE_UNALLOCATED;
			goto EXITFUNC;
	}

	if (!sccp_pbx_channel_allocate(channel, NULL, NULL)) {
		//! \todo handle error in more detail, cleanup sccp channel
		pbx_log(LOG_WARNING, "SCCP: Unable to allocate channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	}
	requestor = (channel && channel->owner) ? channel->owner : NULL;

	/* set initial connected line information, to be exchange with remove party during first CONNECTED_LINE update */
	ast_set_callerid(channel->owner, channel->line->cid_num, channel->line->cid_name, channel->line->cid_num);
	/* end */

	// set calling party 
	sccp_callinfo_t *ci = sccp_channel_getCallInfo(channel);
	iCallInfo.Setter(ci, 
			SCCP_CALLINFO_CALLINGPARTY_NAME, requestor->cid.cid_name,
			SCCP_CALLINFO_CALLINGPARTY_NUMBER, requestor->cid.cid_num,
			SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, requestor->cid.cid_dnid,
			SCCP_CALLINFO_KEY_SENTINEL);

EXITFUNC:
	if (lineName) {
		sccp_free(lineName);
	}
	sccp_restart_monitor();
	return requestor;
}

/*
 * \brief get callerid_name from pbx
 * \param sccp_channle Asterisk Channel
 * \param cid name result
 * \return parse result
 */
static int sccp_wrapper_asterisk16_callerid_name(PBX_CHANNEL_TYPE *pbx_chan, char **cid_name)
{
	if (pbx_chan && pbx_chan->cid.cid_name && strlen(pbx_chan->cid.cid_name) > 0) {
		*cid_name = pbx_strdup(pbx_chan->cid.cid_name);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_number(PBX_CHANNEL_TYPE *pbx_chan, char **cid_number)
{
	if (pbx_chan && pbx_chan->cid.cid_num && strlen(pbx_chan->cid.cid_num) > 0) {
		*cid_number = pbx_strdup(pbx_chan->cid.cid_num);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_ton from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_ton(PBX_CHANNEL_TYPE *pbx_chan, int *cid_ton)
{
	if (pbx_chan && pbx_chan->cid.cid_ton) {
		*cid_ton = pbx_chan->cid.cid_ton;
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_ani from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_ani(PBX_CHANNEL_TYPE *pbx_chan, char **cid_ani)
{
	if (pbx_chan && pbx_chan->cid.cid_ani && strlen(pbx_chan->cid.cid_ani) > 0) {
		*cid_ani = pbx_strdup(pbx_chan->cid.cid_ani);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_dnid from pbx (Destination ID)
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_dnid(PBX_CHANNEL_TYPE *pbx_chan, char **cid_dnid)
{
	if (pbx_chan && pbx_chan->cid.cid_dnid && strlen(pbx_chan->cid.cid_dnid) > 0) {
		*cid_dnid = pbx_strdup(pbx_chan->cid.cid_dnid);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_rdnis from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_rdnis(PBX_CHANNEL_TYPE *pbx_chan, char **cid_rdnis)
{
	if (pbx_chan && pbx_chan->cid.cid_rdnis && strlen(pbx_chan->cid.cid_rdnis) > 0) {
		*cid_rdnis = pbx_strdup(pbx_chan->cid.cid_rdnis);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_presence from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_presentation(PBX_CHANNEL_TYPE *pbx_chan)
{
	if (pbx_chan && pbx_chan->cid.cid_pres) {
		return CALLERID_PRESENTATION_ALLOWED;
	}
	return CALLERID_PRESENTATION_FORBIDDEN;
}

static int sccp_wrapper_asterisk16_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout)
{
	struct varshead *headp;
	struct ast_var_t *current;
	int res = 0;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to call %s (dest:%s, timeout: %d)\n", pbx_channel_name(ast), dest, timeout);

	if (!sccp_strlen_zero(pbx_channel_call_forward(ast))) {
		iPbx.queue_control(ast, -1);									/* Prod Channel if in the middle of a call_forward instead of proceed */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Forwarding Call to '%s'\n", pbx_channel_call_forward(ast));
		return 0;
	}

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (!c) {
		pbx_log(LOG_WARNING, "SCCP: Asterisk request to call %s on channel: %s, but we don't have this channel!\n", dest, pbx_channel_name(ast));
		return -1;
	}

	/* Check whether there is MaxCallBR variables */
	headp = &ast->varshead;
	//ast_log(LOG_NOTICE, "SCCP: search for varibles!\n");

	AST_LIST_TRAVERSE(headp, current, entries) {
		//ast_log(LOG_NOTICE, "var: name: %s, value: %s\n", ast_var_name(current), ast_var_value(current));
		if (!strcasecmp(ast_var_name(current), "__MaxCallBR")) {
			sccp_asterisk_pbx_fktChannelWrite(ast, "CHANNEL", "MaxCallBR", ast_var_value(current));
		} else if (!strcasecmp(ast_var_name(current), "MaxCallBR")) {
			sccp_asterisk_pbx_fktChannelWrite(ast, "CHANNEL", "MaxCallBR", ast_var_value(current));
#if CS_SCCP_VIDEO
		} else if (!strcasecmp(ast_var_name(current), "SCCP_VIDEO_MODE")) {
			sccp_channel_setVideoMode(c, ast_var_value(current));
#endif
		}
	}
	char *cid_name = NULL;
	char *cid_number = NULL;

	sccp_wrapper_asterisk16_callerid_name(c->owner, &cid_name);
	sccp_wrapper_asterisk16_callerid_number(c->owner, &cid_number);

	sccp_channel_set_callingparty(c, cid_name, cid_number);

	if (cid_name) {
		free(cid_name);
	}

	if (cid_number) {
		free(cid_number);
	}

	res = sccp_pbx_call(c, dest, timeout);
	return res;
}

static int sccp_wrapper_asterisk16_answer(PBX_CHANNEL_TYPE * chan)
{
	//! \todo change this handling and split pbx and sccp handling -MC
	int res = -1;
	AUTO_RELEASE(sccp_channel_t, channel , get_sccp_channel_from_pbx_channel(chan));
	if (channel) {
		res = sccp_pbx_answered(channel);
	}
	return res;
}

/**
 * 
 * \todo update remote capabilities after fixup
 */
static int sccp_wrapper_asterisk16_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s and %s\n", pbx_channel_name(oldchan), pbx_channel_name(newchan));
	int res = 0;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(newchan));
	if (!c) {
		pbx_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", pbx_channel_name(oldchan), (void *) oldchan, pbx_channel_name(newchan), (void *) newchan);
		res = -1;
	} else {
		if (c->owner != oldchan) {
			ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, c->owner);
			res = -1;
		} else {
			/* during a masquerade, fixup gets called twice, The Zombie channel name will have been changed to include '<ZOMBI>' */
			/* using test_flag for ZOMBIE cannot be used, as it is only set after the fixup call */
			if (!strstr(pbx_channel_name(newchan), "<ZOMBIE>")) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set c->hangupRequest = requestQueueHangup\n", c->designator);

				// set channel requestHangup to use ast_queue_hangup (as it is now part of a __ast_pbx_run, after masquerade completes) 
				c->hangupRequest = sccp_wrapper_asterisk_requestQueueHangup;
				if (!sccp_strlen_zero(c->line->language)) {
					ast_string_field_set(newchan, language, c->line->language);
				}
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set c->hangupRequest = requestHangup\n", c->designator);
				// set channel requestHangup to use ast_hangup (as it will not be part of __ast_pbx_run anymore, upon returning from masquerade) 
				c->hangupRequest = sccp_wrapper_asterisk_requestHangup;
			}
			//! \todo force update of rtp peer for directrtp
			// sccp_wrapper_asterisk111_update_rtp_peer(newchan, NULL, NULL, 0, 0, 0);

			//! \todo update remote capabilities after fixup
			sccp_wrapper_asterisk106_setOwner(c, newchan);
		}
	}
	return res;
}

#if 0
static enum ast_bridge_result sccp_wrapper_asterisk16_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms)
{
	enum ast_bridge_result res;
	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	AUTO_RELEASE(sccp_channel_t, sc0 , get_sccp_channel_from_pbx_channel(c0));
	AUTO_RELEASE(sccp_channel_t, sc1 , get_sccp_channel_from_pbx_channel(c1));

	if ((sc0 && sc1) {
		// Switch off DTMF between SCCP phones
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_0;
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_1;
		if (GLOB(directrtp)) {
			ast_channel_defer_dtmf(c0);
			ast_channel_defer_dtmf(c1);
		} else {
			AUTO_RELEASE(sccp_device_t, d0 , sccp_channel_getDevice(sc0));
			AUTO_RELEASE(sccp_device_t, d1 , sccp_channel_getDevice(sc1));

			if ((d0 && d1) && (d0->directrtp && d1->directrtp)) {
				ast_channel_defer_dtmf(c0);
				ast_channel_defer_dtmf(c1);
			}
		}
		sc0->peerIsSCCP = TRUE;
		sc1->peerIsSCCP = TRUE;
		// SCCP Key handle direction to asterisk is still to be implemented here
		// sccp_pbx_senddigit
	} else {
		// Switch on DTMF between differing channels
		ast_channel_undefer_dtmf(c0);
		ast_channel_undefer_dtmf(c1);
	}

	res = ast_rtp_bridge(c0, c1, new_flags, fo, rc, timeoutms);
	switch (res) {
		case AST_BRIDGE_COMPLETE:
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Complete\n", c0->name, c1->name);
			break;
		case AST_BRIDGE_FAILED:
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed\n", c0->name, c1->name);
			break;
		case AST_BRIDGE_FAILED_NOWARN:
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed NoWarn\n", c0->name, c1->name);
			break;
		case AST_BRIDGE_RETRY:
			sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridge chan %s and chan %s: Failed Retry\n", c0->name, c1->name);
			break;
	}
	/*! \todo Implement callback function queue upon completion */
	return res;
}
#endif

static enum ast_rtp_get_result sccp_wrapper_asterisk16_get_rtp_info(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	//AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));				// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo = SCCP_RTP_INFO_NORTP;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk16_get_rtp_info) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}
	if (pbx_channel_state(ast) != AST_STATE_UP) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_rtp_info) Asterisk requested EarlyRTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_rtp_info) Asterisk requested RTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GET_FAILED;
	}

	*rtp = audioRTP->instance;
	if (!*rtp) {
		return AST_RTP_GET_FAILED;
	}
	// struct ast_sockaddr ast_sockaddr_tmp;
	// ast_rtp_instance_get_remote_address(*rtp, &ast_sockaddr_tmp);
	// sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (asterisk16_get_rtp_info) remote address:'%s:%d'\n", ast_sockaddr_stringify_host(&ast_sockaddr_tmp), ast_sockaddr_port(&ast_sockaddr_tmp));

#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_rtp_info) JitterBuffer is Forced. AST_RTP_GET_FAILED\n", c->currentDeviceId);
		return AST_RTP_GET_FAILED;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_rtp_info) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", c->currentDeviceId, ast->name);
		return AST_RTP_TRY_PARTIAL;
	}
	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "%s: (asterisk111_get_rtp_info) Channel %s Returning res: %s\n", c->currentDeviceId, pbx_channel_name(ast), ((res == 2) ? "indirect-rtp" : ((res == 1) ? "direct-rtp" : "forbid")));
	return res;
}

static enum ast_rtp_get_result sccp_wrapper_asterisk16_get_vrtp_info(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	//AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));				// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo = SCCP_RTP_INFO_NORTP;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk16_get_vrtp_info) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}
	if (pbx_channel_state(ast) != AST_STATE_UP) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_vrtp_info) Asterisk requested EarlyRTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_vrtp_info) Asterisk requested RTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	}

#ifdef CS_SCCP_VIDEO
	rtpInfo = sccp_rtp_getVideoPeerInfo(c, &audioRTP);							//! \todo should this not be getVideoPeerInfo
#endif
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GET_FAILED;
	}

	*rtp = audioRTP->instance;
	if (!*rtp) {
		return AST_RTP_GET_FAILED;
	}
	// struct ast_sockaddr ast_sockaddr_tmp;
	// ast_rtp_instance_get_remote_address(*rtp, &ast_sockaddr_tmp);
	// sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (sccp_asterisk16_get_vrtp_info) remote address:'%s:%d'\n", c->currentDeviceId, ast_sockaddr_stringify_host(&ast_sockaddr_tmp), ast_sockaddr_port(&ast_sockaddr_tmp));

#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_vrtp_info) JitterBuffer is Forced. AST_RTP_GET_FAILED\n", c->currentDeviceId);
		return AST_RTP_TRY_PARTIAL;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_get_vrtp_info) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", c->currentDeviceId, ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "%s: (asterisk111_get_vrtp_info) Channel %s Returning res: %s\n", c->currentDeviceId, pbx_channel_name(ast), ((res == 2) ? "indirect-rtp" : ((res == 1) ? "direct-rtp" : "forbid")));
	return res;
}

static int sccp_wrapper_asterisk16_update_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);				// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	AUTO_RELEASE(sccp_device_t, d , NULL);
	int result = 0;

	do {
		if (!(c = CS_AST_CHANNEL_PVT(ast))) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk16_update_rtp_peer) NO PVT\n");
			result = -1;
			break;
		}
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (asterisk111_update_rtp_peer) stage: %s, remote codecs capabilty: %lu, nat_active: %d\n", c->currentDeviceId, S_COR(AST_STATE_UP == pbx_channel_state(ast), "RTP", "EarlyRTP"), (long unsigned int) codecs, nat_active);
		if (!c->line) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_update_rtp_peer) NO LINE\n", c->currentDeviceId);
			result = -1;
			break;
		}
		if (c->state == SCCP_CHANNELSTATE_HOLD) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_update_rtp_peer) On Hold -> No Update\n", c->currentDeviceId);
			result = 0;
			break;
		}
		if (!(d = sccp_channel_getDevice(c))) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_update_rtp_peer) NO DEVICE\n", c->currentDeviceId);
			result = -1;
			break;
		}
		if (!rtp && !vrtp && !trtp) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk16_update_rtp_peer) RTP not ready\n", c->currentDeviceId);
			result = 0;
			break;
		}
		PBX_RTP_TYPE *instance = { 0, };
		struct sockaddr_storage sas = { 0, };
		struct sockaddr_in sin = { 0, };
		boolean_t directmedia = FALSE;

		if (rtp) {											// generalize input
			instance = rtp;
		} else if (vrtp) {
			instance = vrtp;
		} else {
			instance = trtp;
		}

		if (d->directrtp && d->nat < SCCP_NAT_ON && !nat_active && !c->conference) {			// asume directrtp
			ast_rtp_get_peer(instance, &sin);
			memcpy(&sas, &sin, sizeof(struct sockaddr_storage));
			if (d->nat == SCCP_NAT_OFF) {								// forced nat off to circumvent autodetection + direcrtp, requires checking both phone_ip and external session ip address against devices permit/deny
				struct sockaddr_in sin_local = { 0, };
				struct sockaddr_storage localsas = { 0, };
				ast_rtp_get_us(instance, &sin_local);
				memcpy(&localsas, &sin_local, sizeof(struct sockaddr_storage));
				if (sccp_apply_ha(d->ha, &sas) == AST_SENSE_ALLOW && sccp_apply_ha(d->ha, &localsas) == AST_SENSE_ALLOW) {
					directmedia = TRUE;
				}
			} else if (sccp_apply_ha(d->ha, &sas) == AST_SENSE_ALLOW) {					// check remote sin against local device acl (to match netmask)
					directmedia = TRUE;
			}
		}
		if (!directmedia) {										// fallback to indirectrtp
			ast_rtp_get_us(instance, &sin);
			// sin.sin_addr.s_addr = sin.sin_addr.s_addr ? sin.sin_addr.s_addr : d->session->ourip.s_addr;
			memcpy(&sas, &sin, sizeof(struct sockaddr_storage));
			sccp_session_getOurIP(d->session, &sas, sccp_netsock_is_IPv4(&sas) ? AF_INET : AF_INET6);
		}

		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk111_update_rtp_peer) new remote rtp ip = '%s'\n (d->directrtp: %s && !d->nat: %s && !remote->nat_active: %s && d->acl_allow: %s) => directmedia=%s\n", c->currentDeviceId, sccp_netsock_stringify(&sas), S_COR(d->directrtp, "yes", "no"),
					  sccp_nat2str(d->nat),
					  S_COR(!nat_active, "yes", "no"), S_COR(directmedia, "yes", "no"), S_COR(directmedia, "yes", "no")
		    );

		if (rtp) {											// send peer info to phone
			sccp_rtp_set_peer(c, &c->rtp.audio, &sas);
			c->rtp.audio.directMedia = directmedia;
		} else if (vrtp) {
			sccp_rtp_set_peer(c, &c->rtp.video, &sas);
			c->rtp.video.directMedia = directmedia;
			//} else {
			//sccp_rtp_set_peer(c, &c->rtp.text, &sas);
			//c->rtp.text.directMedia = directmedia;
		}
	} while (0);

	/* Need a return here to break the bridge */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (asterisk16_update_rtp_peer) Result: %d\n", c ? c->currentDeviceId : NULL, result);
	return result;
}

static int sccp_wrapper_asterisk16_getCodec(PBX_CHANNEL_TYPE * ast)
{
	format_t format = AST_FORMAT_ULAW;
	AUTO_RELEASE(sccp_channel_t, channel , get_sccp_channel_from_pbx_channel(ast));

	if (!channel) {
		sccp_log((DEBUGCAT_RTP | DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (getCodec) NO PVT\n");
		return format;
	}

	ast_debug(10, "asterisk requests format for channel %s, readFormat: %s(%d)\n", ast->name, codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat);
	if (channel->remoteCapabilities.audio[0] != SKINNY_CODEC_NONE) {
		return skinny_codecs2pbx_codecs(channel->remoteCapabilities.audio);
	} 
	return skinny_codecs2pbx_codecs(channel->capabilities.audio);
}

static boolean_t sccp_wrapper_asterisk16_createRtpInstance(constDevicePtr d, constChannelPtr c, sccp_rtp_t *rtp)
{
	struct sockaddr_storage sock = { 0, };
	struct sockaddr_in *sin;
	uint32_t tos = 0, cos = 0;
	
	if (!c || !d) {
		return FALSE;
	}

	if (GLOB(bindaddr).ss_family == AF_INET6) {
		pbx_log(LOG_ERROR, "asterisk 1.6 does not support ipv6, returning FALSE\n");
		return FALSE;
	} 
	memcpy(&sock, &GLOB(bindaddr), sizeof(struct in_addr));
	sin = (struct sockaddr_in *) &sock;

	if (!(rtp->instance = ast_rtp_new_with_bindaddr(sched, io, 1, 0, sin->sin_addr))) {
		return FALSE;
	}

	/* rest below should be moved out of here (refactoring required) */
	PBX_RTP_TYPE *instance = rtp->instance;
	switch(rtp->type) {
		case SCCP_RTP_AUDIO:
			tos = d->audio_tos;
			cos = d->audio_cos;
			break;
		default:
			pbx_log(LOG_ERROR, "%s: (wrapper_create_rtp) unknown/unhandled rtp type, returning instance for now\n", c->designator);
			return TRUE;
	}

	if (c->owner) {
		ast_channel_set_fd(c->owner, 0, ast_rtp_fd(instance));		// RTP
		ast_channel_set_fd(c->owner, 1, ast_rtcp_fd(instance));		// RTCP
	}
	ast_rtp_setqos(instance, tos, cos, "SCCP RTP");
	ast_queue_frame(c->owner, &ast_null_frame); // this prevent a warning about unknown codec, when rtp traffic starts */
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_destroyRTP(PBX_RTP_TYPE * rtp)
{
	ast_rtp_destroy(rtp);
	return (!rtp) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk16_checkHangup(const sccp_channel_t * channel)
{
	int res;

	res = ast_check_hangup(channel->owner);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk16_rtpGetPeer(PBX_RTP_TYPE * rtp, struct sockaddr_storage *address)
{
	struct sockaddr_in tmp = {0};
	ast_rtp_get_peer(rtp, &tmp);
	memcpy(address, &tmp, sizeof(struct sockaddr_in));
	address->ss_family = AF_INET;
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_storage *address)
{
	struct sockaddr_in tmp = {0};
	ast_rtp_get_us(rtp, &tmp);
	memcpy(address, &tmp, sizeof(struct sockaddr_in));
	address->ss_family = AF_INET;
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_getChannelByName(const char *name, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *ast_channel = NULL;

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(iPbx.getChannelByName) searching for channel with identification %s\n", name);
	while ((ast_channel = ast_channel_walk_locked(ast_channel)) != NULL) {
		if (strlen(ast_channel->name) == strlen(name) && !strncmp(ast_channel->name, name, strlen(ast_channel->name))) {
			*pbx_channel = ast_channel;
			pbx_channel_unlock(ast_channel);
			return TRUE;
		}
		pbx_channel_unlock(ast_channel);
	}
	return FALSE;

	/*
	   PBX_CHANNEL_TYPE *ast = ast_get_channel_by_name_locked(name);

	   if (!ast) {
	   return FALSE;
	   }

	   *pbx_channel = ast;
	   ast_channel_unlock(ast);
	   return TRUE;
	 */
}

static int sccp_wrapper_asterisk16_setPhoneRTPAddress(const struct sccp_rtp *rtp, const struct sockaddr_storage *new_peer, int nat_active)
{
	struct sockaddr_in *tmpaddress = (struct sockaddr_in *) new_peer;
	struct sockaddr_in peer;

	memcpy(&peer.sin_addr, &tmpaddress->sin_addr, sizeof(peer.sin_addr));
	peer.sin_port = tmpaddress->sin_port;

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (asterisk16_setPhoneRTPAddress) Update PBX to send RTP/UDP media to '%s:%d' (new remote) (NAT: %s)\n", ast_inet_ntoa(peer.sin_addr), ntohs(peer.sin_port), S_COR(nat_active, "yes", "no"));
	ast_rtp_set_peer(rtp->instance, &peer);

	// sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (asterisk16_setPhoneRTPAddress) Set rtp_instance NAT Property to %s\n", S_COR(nat_active, "True", "False"));
	if (nat_active) {
		ast_rtp_setnat(rtp->instance, 1);
	} else {
		ast_rtp_setnat(rtp->instance, 0);
	}
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_setWriteFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{

	channel->owner->rawwriteformat = skinny_codec2pbx_codec(codec);
	channel->owner->nativeformats |= channel->owner->rawwriteformat;

	if (!channel->owner->writeformat) {
		channel->owner->writeformat = channel->owner->rawwriteformat;
	}

	if (channel->owner->writetrans) {
		ast_translator_free_path(channel->owner->writetrans);
		channel->owner->writetrans = NULL;
	}
	ast_set_write_format(channel->owner, channel->owner->rawwriteformat);

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write native: %d\n", (int) channel->owner->rawwriteformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write: %d\n", (int) channel->owner->writeformat);

	/*
	   PBX_CHANNEL_TYPE *bridge;

	   if (iPbx.getRemoteChannel(channel, &bridge)) {
	   channel->owner->writeformat = 0;

	   bridge->readformat = 0;
	   ast_channel_make_compatible(bridge, channel->owner);
	   } else {
	   ast_set_write_format(channel->owner, channel->owner->rawwriteformat);
	   }
	 */
	channel->owner->nativeformats = channel->owner->rawwriteformat;
	ast_set_write_format(channel->owner, channel->owner->writeformat);

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{

	channel->owner->rawreadformat = skinny_codec2pbx_codec(codec);
	channel->owner->nativeformats = channel->owner->rawreadformat;

	if (!channel->owner->readformat) {
		channel->owner->readformat = channel->owner->rawreadformat;
	}

	if (channel->owner->readtrans) {
		ast_translator_free_path(channel->owner->readtrans);
		channel->owner->readtrans = NULL;
	}
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read native: %d\n", (int) channel->owner->rawreadformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read: %d\n", (int) channel->owner->readformat);

	/*
	   PBX_CHANNEL_TYPE *bridge;

	   if (iPbx.getRemoteChannel(channel, &bridge)) {
	   channel->owner->readformat = 0;

	   bridge->writeformat = 0;
	   ast_channel_make_compatible(channel->owner, bridge);

	   } else {
	   ast_set_read_format(channel->owner, channel->owner->rawreadformat);
	   }
	 */
	channel->owner->nativeformats = channel->owner->rawreadformat;
	ast_set_read_format(channel->owner, channel->owner->readformat);

	return TRUE;
}

static void sccp_wrapper_asterisk16_setCalleridName(PBX_CHANNEL_TYPE * pbxChannel, const char *name)
{
	if (pbxChannel) {
		if (pbxChannel->cid.cid_name) {
			ast_free(pbxChannel->cid.cid_name);
		}
		pbxChannel->cid.cid_name = pbx_strdup(name);
	}
}

static void sccp_wrapper_asterisk16_setCalleridNumber(PBX_CHANNEL_TYPE * pbxChannel, const char *number)
{
	if (pbxChannel) {
		if (pbxChannel->cid.cid_num) {
			ast_free(pbxChannel->cid.cid_num);
		}
		pbxChannel->cid.cid_num = pbx_strdup(number);
	}
}

static void sccp_wrapper_asterisk16_setCalleridAni(PBX_CHANNEL_TYPE * pbxChannel, const char *number)
{
	if (pbxChannel) {
		if (pbxChannel->cid.cid_ani) {
			ast_free(pbxChannel->cid.cid_ani);
		}
		if (number) {
			pbxChannel->cid.cid_ani = pbx_strdup(number);
		}
	}
}

/*
   ANI=Automatic Number Identification => Calling Party
   DNIS/DNID=Dialed Number Identification Service
   RDNIS=Redirected Dialed Number Identification Service
 */
static void sccp_wrapper_asterisk16_setRedirectingParty(PBX_CHANNEL_TYPE * pbxChannel, const char *number, const char *name)
{
	if (pbxChannel) {
		// set redirecting party
		if (pbxChannel->cid.cid_rdnis) {
			ast_free(pbxChannel->cid.cid_rdnis);
		}
		pbxChannel->cid.cid_rdnis = pbx_strdup(number);

		// set number dialed originaly
		if (pbxChannel->cid.cid_dnid) {
			ast_free(pbxChannel->cid.cid_dnid);
		}

		if (pbxChannel->cid.cid_num) {
			pbxChannel->cid.cid_dnid = pbx_strdup(pbxChannel->cid.cid_num);
		}
	}
}

static void sccp_wrapper_asterisk16_setRedirectedParty(PBX_CHANNEL_TYPE * pbxChannel, const char *number, const char *name)
{
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "sccp_wrapper_asterisk16_setRedirectedParty Not Implemented\n");
	/*!< \todo set RedirectedParty using ast_callerid */

	/*      if (number) {
	if (pbxChannel) {
	   pbxChannel->redirecting.to.number.valid = 1;
	   ast_party_number_free(&pbxChannel->redirecting.to.number);
	   pbxChannel->redirecting.to.number.str = pbx_strdup(number);
	   }

	   if (name) {
	   pbxChannel->redirecting.to.name.valid = 1;
	   ast_party_name_free(&pbxChannel->redirecting.to.name);
	   pbxChannel->redirecting.to.name.str = pbx_strdup(name);
	   }
	}
	 */
}

static void sccp_wrapper_asterisk16_updateConnectedLine(const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason)
{
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "sccp_wrapper_asterisk16_updateConnectedLine Not Implemented\n");

	/*
	   struct ast_party_connected_line connected;
	   struct ast_set_party_connected_line update_connected;

	   memset(&update_connected, 0, sizeof(update_connected));

	   if (number) {
	   update_connected.id.number = 1;
	   connected.id.number.valid = 1;
	   connected.id.number.str = (char *)number;
	   connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	   }

	   if (name) {
	   update_connected.id.name = 1;
	   connected.id.name.valid = 1;
	   connected.id.name.str = (char *)name;
	   connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	   }
	   connected.id.tag = NULL;
	   connected.source = reason;

	   ast_channel_queue_connected_line_update(channel->owner, &connected, &update_connected);
	 */
}

static int sccp_wrapper_asterisk16_sched_add(int when, sccp_sched_cb callback, const void *data)
{
	if (sched) {
		return ast_sched_add(sched, when, callback, data);
	}
	return -1;
}

static int sccp_wrapper_asterisk16_sched_del(int id)
{
	if (sched) {
		return AST_SCHED_DEL(sched, id);
	}
	return -1;
}

static int sccp_wrapper_asterisk16_sched_add_ref(int *id, int when, sccp_sched_cb callback, sccp_channel_t * channel)
{
	if (sched && channel) {
		sccp_channel_t *c = sccp_channel_retain(channel);

		if (c) {
			if ((*id  = ast_sched_add(sched, when, callback, c)) < 0) {
				sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: sched add id:%d, when:%d, failed\n", c->designator, *id, when);
				sccp_channel_release(&channel);				/* explicit release on failure */
			}
			return *id;
		}
	}
	return -2;
}

static int sccp_wrapper_asterisk16_sched_del_ref(int *id, sccp_channel_t * channel)
{
	if (sched) {
		AST_SCHED_DEL_UNREF(sched, *id, sccp_channel_release(&channel));
		return *id;
	}
	return -2;
}

static int sccp_wrapper_asterisk16_sched_replace_ref(int *id, int when, ast_sched_cb callback, sccp_channel_t * channel)
{
	if (sched) {
		// sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: (sched_replace_ref) replacing id: %d\n", channel->designator, *id);
		AST_SCHED_REPLACE_UNREF(*id, sched, when, callback, channel, sccp_channel_release((sccp_channel_t **)&_data), sccp_channel_release(&channel), sccp_channel_retain(channel));	/* explicit retain / release */
		// sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: (sched_replace_ref) returning id: %d\n", channel->designator, *id);
		return *id;
	}
	return -2;
}

static long sccp_wrapper_asterisk16_sched_when(int id)
{
	if (sched) {
		return ast_sched_when(sched, id);
	}
	return FALSE;
}

static int sccp_wrapper_asterisk16_sched_wait(int id)
{
	if (sched) {
		return ast_sched_wait(sched);
	}
	return FALSE;
}

static int sccp_wrapper_asterisk16_setCallState(const sccp_channel_t * channel, int state)
{
	sccp_pbx_setcallstate((sccp_channel_t *) channel, state);
	//! \todo implement correct return value (take into account negative deadlock prevention)
	return 0;
}

static void sccp_wrapper_asterisk_set_pbxchannel_linkedid(PBX_CHANNEL_TYPE * pbx_channel, const char *new_linkedid)
{
	if (pbx_channel) {
		pbx_builtin_setvar_helper(pbx_channel, "__" SCCP_AST_LINKEDID_HELPER, new_linkedid);
	}
}

#define DECLARE_PBX_CHANNEL_STRGET(_field) 									\
static const char *sccp_wrapper_asterisk_get_channel_##_field(const sccp_channel_t * channel)	 		\
{														\
	static const char *empty_channel_##_field = "--no-channel-" #_field "--";				\
	if (channel && channel->owner) {											\
		return channel->owner->_field;									\
	}													\
	return empty_channel_##_field;										\
};

#define DECLARE_PBX_CHANNEL_STRSET(_field)									\
static void sccp_wrapper_asterisk_set_channel_##_field(const sccp_channel_t * channel, const char * (_field))	\
{ 														\
        if (channel && channel->owner) {											\
        	sccp_copy_string(channel->owner->_field, _field, sizeof(channel->owner->_field));		\
        }													\
};

DECLARE_PBX_CHANNEL_STRGET(name)
    DECLARE_PBX_CHANNEL_STRGET(uniqueid)
    DECLARE_PBX_CHANNEL_STRGET(appl)
    DECLARE_PBX_CHANNEL_STRGET(exten)
    DECLARE_PBX_CHANNEL_STRSET(exten)
    DECLARE_PBX_CHANNEL_STRGET(context)
    DECLARE_PBX_CHANNEL_STRSET(context)
    DECLARE_PBX_CHANNEL_STRGET(macroexten)
    DECLARE_PBX_CHANNEL_STRSET(macroexten)
    DECLARE_PBX_CHANNEL_STRGET(macrocontext)
    DECLARE_PBX_CHANNEL_STRSET(macrocontext)
    DECLARE_PBX_CHANNEL_STRGET(call_forward)

static const char *sccp_wrapper_asterisk_get_channel_linkedid(const sccp_channel_t * channel)
{
	static const char *emptyLinkedId = "--no-linkedid--";

	if (channel && channel->owner) {
		if (pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKEDID_HELPER)) {
			return pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKEDID_HELPER);
		}
	}
	return emptyLinkedId;
}

static void sccp_wrapper_asterisk_set_channel_linkedid(const sccp_channel_t * channel, const char *new_linkedid)
{
	if (channel && channel->owner) {
		sccp_wrapper_asterisk_set_pbxchannel_linkedid(channel->owner, new_linkedid);
	}
}

static void sccp_wrapper_asterisk_set_channel_name(const sccp_channel_t * channel, const char *new_name)
{
	if (channel && channel->owner) {
		pbx_string_field_set(channel->owner, name, new_name);
	}
}

static void sccp_wrapper_asterisk_set_channel_call_forward(const sccp_channel_t * channel, const char *new_call_forward)
{
	if (channel && channel->owner) {
		pbx_string_field_set(channel->owner, call_forward, new_call_forward);
	}
}

static enum ast_channel_state sccp_wrapper_asterisk_get_channel_state(const sccp_channel_t * channel)
{
	if (channel && channel->owner) {
		return channel->owner->_state;
	}
	return 0;
}

static const struct ast_pbx *sccp_wrapper_asterisk_get_channel_pbx(const sccp_channel_t * channel)
{
	if (channel && channel->owner) {
		return channel->owner->pbx;
	}
	return NULL;
}

/*!
 * \brief Find Asterisk/PBX channel by linkedid
 *
 * \param ast   pbx channel
 * \param data  linkId as void *
 *
 * \return int
 */
static int pbx_find_channel_by_linkedid(PBX_CHANNEL_TYPE * ast, void *data)
{
	if (!data) {
		return 0;
	}
	char *linkedId = data;

	if (!ast->masq && sccp_strequals(pbx_builtin_getvar_helper(ast, SCCP_AST_LINKEDID_HELPER), linkedId)) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: peer name: %s, linkId: %s\n", ast->name ? ast->name : "(null)", linkedId);
		// return !ast->pbx && (!strcasecmp(refLinkId, linkId)) && !ast->masq;
		return 1;
	} 
	return 0;
}

static boolean_t sccp_asterisk_getRemoteChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *remotePeer = ast_channel_search_locked(pbx_find_channel_by_linkedid, (void *) sccp_wrapper_asterisk_get_channel_linkedid(channel));

	if (remotePeer) {
		*pbx_channel = remotePeer;
		return TRUE;
	}
	return FALSE;
}

static int pbx_find_forward_originator_channel_by_linkedid(PBX_CHANNEL_TYPE * ast, void *data)
{
	if (!data) {
		return 0;
	}
	PBX_CHANNEL_TYPE *refAstChannel = data;

	if (ast == refAstChannel) {										// skip own channel
		return 0;
	}
	if (!ast->masq && sccp_strequals(pbx_builtin_getvar_helper(refAstChannel, SCCP_AST_LINKEDID_HELPER), pbx_builtin_getvar_helper(ast, SCCP_AST_LINKEDID_HELPER))) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: peer name: %s\n", ast->name ? ast->name : "(null)");
		return 1;
	} 
	return 0;
}

boolean_t sccp_asterisk_getForwarderPeerChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *remotePeer = ast_channel_search_locked(pbx_find_forward_originator_channel_by_linkedid, (void *) channel->owner);

	if (remotePeer) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: found peer name: %s\n", remotePeer->name ? remotePeer->name : "(null)");
		*pbx_channel = remotePeer;
		return TRUE;
	}
	return FALSE;
}

/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE * ast, const char *text)
{
	uint8_t instance;

	if (!ast) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No PBX CHANNEL to send text to\n");
		return -1;
	}

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (!c) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send text to (%s)\n", ast->name);
		return -1;
	}
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (!d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send text to (%s)\n", ast->name);
		return -1;
	}

	if (!text || sccp_strlen_zero(text)) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No text to send to %s\n", d->id, ast->name);
		return 0;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_displayprompt(d, instance, c->callid, (char *) text, 10);
	return 0;
}

/*!
 * \brief Receive First Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit First Digit as char
 * \return Always Return -1 as int
 * 
 * \called_from_asterisk
 */
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE * ast, char digit)
{
	return -1;
}

/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \param duration Duration as int
 * \return boolean
 * 
 * \called_from_asterisk
 */
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE * ast, char digit, unsigned int duration)
{
	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (!c) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send digit to (%s)\n", ast->name);
		return -1;
	}

	if (c->dtmfmode == SCCP_DTMFMODE_RFC2833) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Channel(%s) DTMF Mode is RFC2833. Skipping...\n", c->designator, pbx_channel_name(ast));
		return -1;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (!d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send digit to (%s)\n", ast->name);
		return -1;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, sccp_dtmfmode2str(d->dtmfmode));

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		return -1;
	}

	sccp_dev_keypadbutton(d, digit, sccp_device_find_index_for_line(d, c->line->name), c->callid);
	return -1;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findChannelWithCallback(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data, boolean_t lock)
{
	PBX_CHANNEL_TYPE *remotePeer;

	if (!lock) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "requesting channel search without intermediate locking, but no implementation for this (yet)\n");
	}
	remotePeer = ast_channel_search_locked(found_cb, data);
	return remotePeer;
}

static int sccp_pbx_sendHTML(PBX_CHANNEL_TYPE * ast, int subclass, const char *data, int datalen)
{
	int res = -1;
	if (!datalen || sccp_strlen_zero(data) || !(!strncmp(data, "http://", 7) || !strncmp(data, "file://", 7) || !strncmp(data, "ftp://", 6))) {
		pbx_log(LOG_NOTICE, "SCCP: Received a non valid URL\n");
		return res;
	}
	struct ast_frame fr;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (c) {
		AUTO_RELEASE(sccp_device_t, d , c->getDevice(c));
 		if (d) {
			memset(&fr, 0, sizeof(fr));
			fr.frametype = AST_FRAME_HTML;
#if CS_AST_NEW_FRAME_STRUCT
			fr.data.ptr = (char *) data;
#else
			fr.data = (char *) data;
#endif
			fr.src = "SCCP Send URL";
			fr.datalen = datalen;

			sccp_push_result_t pushResult = d->pushURL(d, data, 1, SKINNY_TONE_ZIP);

			if (SCCP_PUSH_RESULT_SUCCESS == pushResult) {
				fr.subclass = AST_HTML_LDCOMPLETE;
			} else {
				fr.subclass = AST_HTML_NOSUPPORT;
			}
			ast_queue_frame(ast, ast_frisolate(&fr));
			res = 0;
		}
	}
	return res;
}

/*!
 * \brief Queue a control frame
 * \param pbx_channel PBX Channel
 * \param control as Asterisk Control Frame Type
 */
int sccp_asterisk_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control)
{
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass = control };
	return ast_queue_frame((PBX_CHANNEL_TYPE *) pbx_channel, &f);
}

/*!
 * \brief Queue a control frame with payload
 * \param pbx_channel PBX Channel
 * \param control as Asterisk Control Frame Type
 * \param data Payload
 * \param datalen Payload Length
 */
int sccp_asterisk_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen)
{
#if CS_AST_NEW_FRAME_STRUCT
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass = control,.data.ptr = (void *) data,.datalen = datalen };
#else
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass = control,.data = (void *) data,.datalen = datalen };
#endif
	return ast_queue_frame((PBX_CHANNEL_TYPE *) pbx_channel, &f);
}

/*!
 * \brief Get Hint Extension State and return the matching Busy Lamp Field State 
 */
static skinny_busylampfield_state_t sccp_wrapper_asterisk106_getExtensionState(const char *extension, const char *context)
{
	skinny_busylampfield_state_t result = SKINNY_BLF_STATUS_UNKNOWN;

	if (sccp_strlen_zero(extension) || sccp_strlen_zero(context)) {
		pbx_log(LOG_ERROR, "SCCP: (iPbx.getExtensionState): Either extension:'%s' or context:;%s' provided is empty\n", extension, context);
		return result;
	}

	int state = ast_extension_state(NULL, context, extension);

	switch (state) {
		case AST_EXTENSION_REMOVED:
		case AST_EXTENSION_DEACTIVATED:
		case AST_EXTENSION_UNAVAILABLE:
			result = SKINNY_BLF_STATUS_UNKNOWN;
			break;
		case AST_EXTENSION_NOT_INUSE:
			result = SKINNY_BLF_STATUS_IDLE;
			break;
		case AST_EXTENSION_INUSE:
		case AST_EXTENSION_ONHOLD:
		case AST_EXTENSION_ONHOLD + AST_EXTENSION_INUSE:
		case AST_EXTENSION_BUSY:
			result = SKINNY_BLF_STATUS_INUSE;
			break;
		case AST_EXTENSION_RINGING + AST_EXTENSION_INUSE:
		case AST_EXTENSION_RINGING:
			result = SKINNY_BLF_STATUS_ALERTING;
			break;
	}
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (getExtensionState) extension: %s@%s, extension_state: '%s (%d)' -> blf state '%d'\n", extension, context, ast_extension_state2str(state), state, result);
	return result;
}

static boolean_t sccp_wrapper_asterisk106_channelIsBridged(sccp_channel_t * channel)
{
	boolean_t res = FALSE;

	if (!channel || !channel->owner) {
		return res;
	}
	ast_channel_lock(channel->owner);
	if (ast_bridged_channel(channel->owner)) {
		res = TRUE;
	}
	ast_channel_unlock(channel->owner);
	return res;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk106_getBridgeChannel(PBX_CHANNEL_TYPE * pbx_channel)
{
	PBX_CHANNEL_TYPE *bridgePeer = NULL;
	if (pbx_channel && (bridgePeer = ast_bridged_channel(pbx_channel))) {
		return pbx_channel_ref(bridgePeer);
	};
	return NULL;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk106_getUnderlyingChannel(PBX_CHANNEL_TYPE * pbx_channel)
{
	PBX_CHANNEL_TYPE *bridgePeer = NULL;
	if (pbx_channel && (bridgePeer = pbx_channel->tech->bridged_channel(pbx_channel, NULL))) {
		return pbx_channel_ref(bridgePeer);
	}
	return NULL;
}

static boolean_t sccp_wrapper_asterisk106_attended_transfer(sccp_channel_t * destination_channel, sccp_channel_t * source_channel)
{
	// possibly move transfer related callinfo updates here
	if (!destination_channel || !source_channel || !destination_channel->owner || !source_channel->owner) {
		return FALSE;
	}
	PBX_CHANNEL_TYPE *pbx_destination_local_channel = destination_channel->owner;
	PBX_CHANNEL_TYPE *pbx_source_remote_channel = sccp_wrapper_asterisk106_getBridgeChannel(source_channel->owner);

	if (!pbx_destination_local_channel || !pbx_source_remote_channel) {
		return FALSE;
	}
	return !pbx_channel_masquerade(pbx_destination_local_channel, pbx_source_remote_channel);
}

/*!
 * \brief using RTP Glue Engine
 */
#if defined(__cplusplus) || defined(c_plusplus)
static struct ast_rtp_protocol sccp_rtp = {
	/* *INDENT-OFF* */
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk16_get_rtp_info,
	.get_vrtp_info = sccp_wrapper_asterisk16_get_vrtp_info,
	.set_rtp_peer = sccp_wrapper_asterisk16_update_rtp_peer,
	.get_codec = sccp_wrapper_asterisk16_getCodec,
	/* *INDENT-ON* */
};
#else
static struct ast_rtp_protocol sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk16_get_rtp_info,
	.get_vrtp_info = sccp_wrapper_asterisk16_get_vrtp_info,
	.set_rtp_peer = sccp_wrapper_asterisk16_update_rtp_peer,
	.get_codec = sccp_wrapper_asterisk16_getCodec,
};
#endif

#ifdef HAVE_PBX_MESSAGE_H
#include <asterisk/message.h>
static int sccp_asterisk_message_send(const struct ast_msg *msg, const char *to, const char *from)
{
	char *lineName;
	const char *messageText = ast_msg_get_body(msg);
	int res = -1;

	lineName = pbx_strdupa(to);
	if (strchr(lineName, '@')) {
		strsep(&lineName, "@");
	} else {
		strsep(&lineName, ":");
	}
	if (sccp_strlen_zero(lineName)) {
		pbx_log(LOG_WARNING, "MESSAGE(to) is invalid for SCCP - '%s'\n", to);
		return -1;
	}

	AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(lineName, FALSE));
	if (!line) {
		pbx_log(LOG_WARNING, "line '%s' not found\n", lineName);
		return -1;
	}

	/** \todo move this to line implementation */
	sccp_linedevices_t *linedevice = NULL;
	sccp_push_result_t pushResult;

	SCCP_LIST_LOCK(&line->devices);
	SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
		pushResult = linedevice->device->pushTextMessage(linedevice->device, messageText, from, 1, SKINNY_TONE_ZIP);
		if (SCCP_PUSH_RESULT_SUCCESS == pushResult) {
			res = 0;
		}
	}
	SCCP_LIST_UNLOCK(&line->devices);

	return res;
}

#if defined(__cplusplus) || defined(c_plusplus)
static const struct ast_msg_tech sccp_msg_tech = {
	/* *INDENT-OFF* */
	name:	"sccp",
	msg_send:sccp_asterisk_message_send,
	/* *INDENT-ON* */
};
#else
static const struct ast_msg_tech sccp_msg_tech = {
	/* *INDENT-OFF* */
	.name = "sccp",
	.msg_send = sccp_asterisk_message_send,
	/* *INDENT-ON* */
};
#endif

#endif

static boolean_t sccp_wrapper_asterisk_setLanguage(PBX_CHANNEL_TYPE * pbxChannel, const char *newLanguage)
{

	ast_string_field_set(pbxChannel, language, newLanguage);
	return TRUE;
}

#if defined(__cplusplus) || defined(c_plusplus)
const PbxInterface iPbx = {
	/* *INDENT-OFF* */
	alloc_pbxChannel:		sccp_wrapper_asterisk16_allocPBXChannel,
	set_callstate:			sccp_wrapper_asterisk16_setCallState,
	checkhangup:			sccp_wrapper_asterisk16_checkHangup,
	hangup:				NULL,
	extension_status:		sccp_wrapper_asterisk16_extensionStatus,

	setPBXChannelLinkedId:		sccp_wrapper_asterisk_set_pbxchannel_linkedid,
	/** get channel by name */
	getChannelByName:		sccp_wrapper_asterisk16_getChannelByName,
	getRemoteChannel:		sccp_asterisk_getRemoteChannel,
	getChannelByCallback:		NULL,

	getChannelLinkedId:		sccp_wrapper_asterisk_get_channel_linkedid,
	setChannelLinkedId:		sccp_wrapper_asterisk_set_channel_linkedid,
	getChannelName:			sccp_wrapper_asterisk_get_channel_name,
	setChannelName:			sccp_wrapper_asterisk_set_channel_name,
	getChannelUniqueID:		sccp_wrapper_asterisk_get_channel_uniqueid,
	getChannelExten:		sccp_wrapper_asterisk_get_channel_exten,
	setChannelExten:		sccp_wrapper_asterisk_set_channel_exten,
	getChannelContext:		sccp_wrapper_asterisk_get_channel_context,
	setChannelContext:		sccp_wrapper_asterisk_set_channel_context,
	getChannelMacroExten:		sccp_wrapper_asterisk_get_channel_macroexten,
	setChannelMacroExten:		sccp_wrapper_asterisk_set_channel_macroexten,
	getChannelMacroContext:		sccp_wrapper_asterisk_get_channel_macrocontext,
	setChannelMacroContext:		sccp_wrapper_asterisk_set_channel_macrocontext,
	getChannelCallForward:		sccp_wrapper_asterisk_get_channel_call_forward,
	setChannelCallForward:		sccp_wrapper_asterisk_set_channel_call_forward,

	getChannelAppl:			sccp_wrapper_asterisk_get_channel_appl,
	getChannelState:		sccp_wrapper_asterisk_get_channel_state,
	getChannelPbx:			sccp_wrapper_asterisk_get_channel_pbx,

	set_nativeAudioFormats:		sccp_wrapper_asterisk16_setNativeAudioFormats,
	set_nativeVideoFormats:		sccp_wrapper_asterisk16_setNativeVideoFormats,

	getPeerCodecCapabilities:	NULL,
	send_digit:			sccp_wrapper_asterisk16_sendDigit,
	send_digits:			sccp_wrapper_asterisk16_sendDigits,

	sched_add:			sccp_wrapper_asterisk16_sched_add,
	sched_del:			sccp_wrapper_asterisk16_sched_del,
	sched_add_ref:			sccp_wrapper_asterisk16_sched_add_ref,
	sched_del_ref:			sccp_wrapper_asterisk16_sched_del_ref,
	sched_replace_ref:		sccp_wrapper_asterisk16_sched_replace_ref,
	sched_when:			sccp_wrapper_asterisk16_sched_when,
	sched_wait:			sccp_wrapper_asterisk16_sched_wait,

	/* rtp */
	rtp_getPeer:			sccp_wrapper_asterisk16_rtpGetPeer,
	rtp_getUs:			sccp_wrapper_asterisk16_rtpGetUs,
	rtp_setPhoneAddress:		sccp_wrapper_asterisk16_setPhoneRTPAddress,
	rtp_setWriteFormat:		sccp_wrapper_asterisk16_setWriteFormat,
	rtp_setReadFormat:		sccp_wrapper_asterisk16_setReadFormat,
	rtp_destroy:			sccp_wrapper_asterisk16_destroyRTP,
	rtp_stop:			ast_rtp_stop,
	rtp_codec:			NULL,
	rtp_create_instance:		sccp_wrapper_asterisk16_createRtpInstance,
	rtp_get_payloadType:		sccp_wrapper_asterisk16_get_payloadType,
	rtp_get_sampleRate:		sccp_wrapper_asterisk16_get_sampleRate,
	rtp_bridgePeers:		NULL,

	/* callerid */
	get_callerid_name:		sccp_wrapper_asterisk16_callerid_name,
	get_callerid_number:		sccp_wrapper_asterisk16_callerid_number,
	get_callerid_ton:		sccp_wrapper_asterisk16_callerid_ton,
	get_callerid_ani:		sccp_wrapper_asterisk16_callerid_ani,
	get_callerid_subaddr:		NULL,
	get_callerid_dnid:		sccp_wrapper_asterisk16_callerid_dnid,
	get_callerid_rdnis:		sccp_wrapper_asterisk16_callerid_rdnis,
	get_callerid_presentation:	sccp_wrapper_asterisk16_callerid_presentation,

	set_callerid_name:		sccp_wrapper_asterisk16_setCalleridName,
	set_callerid_number:		sccp_wrapper_asterisk16_setCalleridNumber,
	set_callerid_ani:		sccp_wrapper_asterisk16_setCalleridAni,
	set_callerid_dnid:		NULL,
	set_callerid_redirectingParty:	sccp_wrapper_asterisk16_setRedirectingParty,
	set_callerid_redirectedParty:	sccp_wrapper_asterisk16_setRedirectedParty,
	set_callerid_presentation:	sccp_wrapper_asterisk16_setCalleridPresentation,
	set_connected_line:		sccp_wrapper_asterisk16_updateConnectedLine,
	sendRedirectedUpdate:		sccp_asterisk_sendRedirectedUpdate,

	/* feature section */
	feature_park:			sccp_wrapper_asterisk16_park,
	feature_stopMusicOnHold:	NULL,
	feature_addToDatabase:		sccp_asterisk_addToDatabase,
	feature_getFromDatabase:	sccp_asterisk_getFromDatabase,
	feature_removeFromDatabase:	sccp_asterisk_removeFromDatabase,
	feature_removeTreeFromDatabase:	sccp_asterisk_removeTreeFromDatabase,
	feature_monitor:		sccp_wrapper_asterisk_featureMonitor,
	getFeatureExtension:		sccp_wrapper_asterisk16_getFeatureExtension,
	getPickupExtension:		sccp_wrapper_asterisk16_getPickupExtension,

	eventSubscribe:			NULL,
	findChannelByCallback:		sccp_wrapper_asterisk16_findChannelWithCallback,

	moh_start:			sccp_asterisk_moh_start,
	moh_stop:			sccp_asterisk_moh_stop,
	queue_control:			sccp_asterisk_queue_control,
	queue_control_data:		sccp_asterisk_queue_control_data,

	allocTempPBXChannel:		sccp_wrapper_asterisk16_allocTempPBXChannel,
	masqueradeHelper:		sccp_wrapper_asterisk16_masqueradeHelper,
	requestAnnouncementChannel:	sccp_wrapper_asterisk16_requestAnnouncementChannel,
	
	set_language:			sccp_wrapper_asterisk_setLanguage,

	getExtensionState:		sccp_wrapper_asterisk106_getExtensionState,
	findPickupChannelByExtenLocked: sccp_wrapper_asterisk16_findPickupChannelByExtenLocked,
	findPickupChannelByGroupLocked: sccp_wrapper_asterisk16_findPickupChannelByGroupLocked,

	set_owner:			sccp_wrapper_asterisk106_setOwner,
	removeTimingFD:			sccp_wrapper_asterisk106_removeTimingFD,
	dumpchan:			NULL,
	channel_is_bridged:		sccp_wrapper_asterisk106_channelIsBridged,
	get_bridged_channel:		sccp_wrapper_asterisk106_getBridgeChannel,
	get_underlying_channel:		sccp_wrapper_asterisk106_getUnderlyingChannel,
	attended_transfer:              sccp_wrapper_asterisk106_attended_transfer,

	set_callgroup:			sccp_wrapper_asterisk_set_callgroup,
	set_pickupgroup:		sccp_wrapper_asterisk_set_pickupgroup,
	set_named_callgroups:		NULL,
	set_named_pickupgroups:		NULL,
	/* *INDENT-ON* */
};

#else

/*!
 * \brief SCCP - PBX Callback Functions 
 * (Decoupling Tight Dependencies on Asterisk Functions)
 */
const PbxInterface iPbx = {
	/* *INDENT-OFF* */
  
        /* channel */
	.alloc_pbxChannel 		= sccp_wrapper_asterisk16_allocPBXChannel,
	.extension_status 		= sccp_wrapper_asterisk16_extensionStatus,
	.setPBXChannelLinkedId		= sccp_wrapper_asterisk_set_pbxchannel_linkedid,
	.getChannelByName 		= sccp_wrapper_asterisk16_getChannelByName,

	.getChannelLinkedId		= sccp_wrapper_asterisk_get_channel_linkedid,
	.setChannelLinkedId		= sccp_wrapper_asterisk_set_channel_linkedid,
	.getChannelName			= sccp_wrapper_asterisk_get_channel_name,
	.setChannelName			= sccp_wrapper_asterisk_set_channel_name,
	.getChannelUniqueID		= sccp_wrapper_asterisk_get_channel_uniqueid,
	.getChannelExten		= sccp_wrapper_asterisk_get_channel_exten,
	.setChannelExten		= sccp_wrapper_asterisk_set_channel_exten,
	.getChannelContext		= sccp_wrapper_asterisk_get_channel_context,
	.setChannelContext		= sccp_wrapper_asterisk_set_channel_context,
	.getChannelMacroExten		= sccp_wrapper_asterisk_get_channel_macroexten,
	.setChannelMacroExten		= sccp_wrapper_asterisk_set_channel_macroexten,
	.getChannelMacroContext		= sccp_wrapper_asterisk_get_channel_macrocontext,
	.setChannelMacroContext		= sccp_wrapper_asterisk_set_channel_macrocontext,
	.getChannelCallForward		= sccp_wrapper_asterisk_get_channel_call_forward,
	.setChannelCallForward		= sccp_wrapper_asterisk_set_channel_call_forward,

	.getChannelAppl			= sccp_wrapper_asterisk_get_channel_appl,
	.getChannelState		= sccp_wrapper_asterisk_get_channel_state,
	.getChannelPbx			= sccp_wrapper_asterisk_get_channel_pbx,

	.getRemoteChannel		= sccp_asterisk_getRemoteChannel,
	.checkhangup			= sccp_wrapper_asterisk16_checkHangup,
	/* digits */
	.send_digits 			= sccp_wrapper_asterisk16_sendDigits,
	.send_digit 			= sccp_wrapper_asterisk16_sendDigit,

	/* schedulers */
	.sched_add			= sccp_wrapper_asterisk16_sched_add,
	.sched_del			= sccp_wrapper_asterisk16_sched_del,
	.sched_add_ref			= sccp_wrapper_asterisk16_sched_add_ref,
	.sched_del_ref			= sccp_wrapper_asterisk16_sched_del_ref,
	.sched_replace_ref		= sccp_wrapper_asterisk16_sched_replace_ref,
	.sched_when 			= sccp_wrapper_asterisk16_sched_when,
	.sched_wait 			= sccp_wrapper_asterisk16_sched_wait,
	/* callstate / indicate */
	.set_callstate 			= sccp_wrapper_asterisk16_setCallState,

	/* codecs */
	.set_nativeAudioFormats 	= sccp_wrapper_asterisk16_setNativeAudioFormats,
	.set_nativeVideoFormats 	= sccp_wrapper_asterisk16_setNativeVideoFormats,

	/* rtp */
	.rtp_getPeer			= sccp_wrapper_asterisk16_rtpGetPeer,
	.rtp_getUs 			= sccp_wrapper_asterisk16_rtpGetUs,
	.rtp_stop			= ast_rtp_stop,
	.rtp_create_instance		= sccp_wrapper_asterisk16_createRtpInstance,
	.rtp_get_payloadType 		= sccp_wrapper_asterisk16_get_payloadType,
	.rtp_get_sampleRate 		= sccp_wrapper_asterisk16_get_sampleRate,
	.rtp_destroy 			= sccp_wrapper_asterisk16_destroyRTP,
	.rtp_setWriteFormat 		= sccp_wrapper_asterisk16_setWriteFormat,
	.rtp_setReadFormat 		= sccp_wrapper_asterisk16_setReadFormat,
	.rtp_setPhoneAddress		= sccp_wrapper_asterisk16_setPhoneRTPAddress,

	/* callerid */
	.get_callerid_name 		= sccp_wrapper_asterisk16_callerid_name,
	.get_callerid_number 		= sccp_wrapper_asterisk16_callerid_number,
	.get_callerid_ton 		= sccp_wrapper_asterisk16_callerid_ton,
	.get_callerid_ani 		= sccp_wrapper_asterisk16_callerid_ani,
	.get_callerid_subaddr 		= NULL,
	.get_callerid_dnid 		= sccp_wrapper_asterisk16_callerid_dnid,
	.get_callerid_rdnis 		= sccp_wrapper_asterisk16_callerid_rdnis,
	.get_callerid_presentation 	= sccp_wrapper_asterisk16_callerid_presentation,
	.set_callerid_name 		= sccp_wrapper_asterisk16_setCalleridName,
	.set_callerid_number 		= sccp_wrapper_asterisk16_setCalleridNumber,
	.set_callerid_ani		= sccp_wrapper_asterisk16_setCalleridAni,
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_wrapper_asterisk16_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_wrapper_asterisk16_setRedirectedParty,
	.set_callerid_presentation 	= sccp_wrapper_asterisk16_setCalleridPresentation,
	.set_connected_line		= sccp_wrapper_asterisk16_updateConnectedLine,
	.sendRedirectedUpdate		= sccp_asterisk_sendRedirectedUpdate,
	/* database */
	.feature_addToDatabase 		= sccp_asterisk_addToDatabase,
	.feature_getFromDatabase 	= sccp_asterisk_getFromDatabase,
	.feature_removeFromDatabase     = sccp_asterisk_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_asterisk_removeTreeFromDatabase,
	.feature_monitor		= sccp_wrapper_asterisk_featureMonitor,
	
	.feature_park			= sccp_wrapper_asterisk16_park,
	.getFeatureExtension		= sccp_wrapper_asterisk16_getFeatureExtension,
	.getPickupExtension		= sccp_wrapper_asterisk16_getPickupExtension,
	.findChannelByCallback		= sccp_wrapper_asterisk16_findChannelWithCallback,

	.moh_start			= sccp_asterisk_moh_start,
	.moh_stop			= sccp_asterisk_moh_stop,
	.queue_control			= sccp_asterisk_queue_control,
	.queue_control_data		= sccp_asterisk_queue_control_data,
	.allocTempPBXChannel		= sccp_wrapper_asterisk16_allocTempPBXChannel,
	.masqueradeHelper		= sccp_wrapper_asterisk16_masqueradeHelper,
	.requestAnnouncementChannel	= sccp_wrapper_asterisk16_requestAnnouncementChannel,
	.set_language			= sccp_wrapper_asterisk_setLanguage,

	.getExtensionState		= sccp_wrapper_asterisk106_getExtensionState,
	.findPickupChannelByExtenLocked	= sccp_wrapper_asterisk16_findPickupChannelByExtenLocked,
	.findPickupChannelByGroupLocked	= sccp_wrapper_asterisk16_findPickupChannelByGroupLocked,

	.set_owner			= sccp_wrapper_asterisk106_setOwner,
	.removeTimingFD			= sccp_wrapper_asterisk106_removeTimingFD,
	.dumpchan			= NULL,
	.channel_is_bridged		= sccp_wrapper_asterisk106_channelIsBridged,
	.get_bridged_channel		= sccp_wrapper_asterisk106_getBridgeChannel,
	.get_underlying_channel		= sccp_wrapper_asterisk106_getUnderlyingChannel,
	.attended_transfer              = sccp_wrapper_asterisk106_attended_transfer,

	.set_callgroup			= sccp_wrapper_asterisk_set_callgroup,
	.set_pickupgroup		= sccp_wrapper_asterisk_set_pickupgroup,
	.set_named_callgroups		= NULL,
	.set_named_pickupgroups		= NULL,
	/* *INDENT-ON* */
};
#endif

static int unload_module(void)
{
	sccp_preUnload();
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP RTP protocol\n");
	ast_rtp_proto_unregister(&sccp_rtp);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP Channel Tech\n");
	ast_channel_unregister(&sccp_tech);
	sccp_unregister_dialplan_functions();
	sccp_unregister_cli();
	sccp_mwi_module_stop();
#ifdef CS_SCCP_MANAGER
	sccp_unregister_management();
#endif
#ifdef HAVE_PBX_MESSAGE_H
	ast_msg_tech_unregister(&sccp_msg_tech);
#endif

	sccp_globals_lock(monitor_lock);
	if ((GLOB(monitor_thread) != AST_PTHREADT_NULL) && (GLOB(monitor_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(monitor_thread));
		pthread_kill(GLOB(monitor_thread), SIGURG);
		pthread_join(GLOB(monitor_thread), NULL);
	}
	GLOB(monitor_thread) = AST_PTHREADT_STOP;

	if (io) {
		io_context_destroy(io);
		io = NULL;
	}

	while (SCCP_REF_DESTROYED != sccp_refcount_isRunning()) {
		usleep(SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT);							// give enough time for all schedules to end and refcounted object to be cleanup completely
	}

	if (sched) {
		pbx_log(LOG_NOTICE, "Cleaning up scheduled items:\n");
		int scheduled_items = 0;

		ast_sched_dump(sched);
		while ((scheduled_items = ast_sched_runq(sched))) {
			pbx_log(LOG_NOTICE, "Cleaning up %d scheduled items... please wait\n", scheduled_items);
			usleep(ast_sched_wait(sched));
		}
		sched = NULL;
	}
	sccp_globals_unlock(monitor_lock);
	sccp_mutex_destroy(&GLOB(monitor_lock));

	sccp_free(sccp_globals);
	pbx_log(LOG_NOTICE, "Running Cleanup\n");
#ifdef HAVE_LIBGC
	// sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Collect a little:%d\n",GC_collect_a_little());
	// CHECK_LEAKS();
	// GC_gcollect();
#endif
	pbx_log(LOG_NOTICE, "Module chan_sccp unloaded\n");
	return 0;
}

#if defined(__cplusplus) || defined(c_plusplus)
static ast_module_load_result load_module(void)
#else
static int load_module(void)
#endif
{
	int res = AST_MODULE_LOAD_DECLINE;
	do {
		if (ast_module_check("chan_skinny.so")) {
			pbx_log(LOG_ERROR, "Chan_skinny is loaded. Please check modules.conf and remove chan_skinny before loading chan_sccp.\n");
			break;
		}
		if (!(sched = sched_context_create())) {
			pbx_log(LOG_WARNING, "Unable to create schedule context. SCCP channel type disabled\n");
			break;
		}
#if defined(CS_DEVSTATE_FEATURE) || defined(CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE)
		ast_enable_distributed_devstate();
#endif
		if (!sccp_prePBXLoad()) {
			pbx_log(LOG_ERROR, "SCCP: prePBXLoad Failed\n");
			break;
		}
		if (!(io = io_context_create())) {
			pbx_log(LOG_ERROR, "Unable to create I/O context. SCCP channel type disabled\n");
			break;
		}
		if (!load_config()) {
			pbx_log(LOG_ERROR, "SCCP: config file could not be parsed\n");
			res = AST_MODULE_LOAD_DECLINE;
			break;
		}

		if (ast_channel_register(&sccp_tech)) {
			pbx_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			break;
		}
#ifdef HAVE_PBX_MESSAGE_H
		if (ast_msg_tech_register(&sccp_msg_tech)) {
			pbx_log(LOG_ERROR, "Unable to register message interface\n");
			break;
		}
#endif
		if (ast_rtp_proto_register(&sccp_rtp)) {
			pbx_log(LOG_ERROR, "Unable to register RTP Glue/Proto\n");
			break;
		}
		if (sccp_register_management()) {
			pbx_log(LOG_ERROR, "Unable to register management functions");
			break;
		}
		if (sccp_register_cli()) {
			pbx_log(LOG_ERROR, "Unable to register CLI functions");
			break;
		}
		if (sccp_register_dialplan_functions()) {
			pbx_log(LOG_ERROR, "Unable to register dialplan functions");
			break;
		}
		if (sccp_restart_monitor()) {
			pbx_log(LOG_ERROR, "Could not restart the monitor thread");
			break;
		}
		if (!sccp_postPBX_load()) {
			pbx_log(LOG_ERROR, "SCCP: postPBXLoad Failed\n");
			break;
		}
		res = AST_MODULE_LOAD_SUCCESS;
	} while (0);

	if (res != AST_MODULE_LOAD_SUCCESS) {
		pbx_log(LOG_ERROR, "SCCP: Module Load Failed, unloading...\n");
		unload_module();
	}
	return res;
}

#if ASTERSIK_VERSION_NUMBER > 10601
static int module_reload(void)
{
	return sccp_reload();
}
#endif

#if defined(__cplusplus) || defined(c_plusplus)

static struct ast_module_info __mod_info = {
	NULL,
	load_module,
	module_reload,
	unload_module,
	NULL,
	NULL,
	AST_MODULE,
	SCCP_VERSIONSTR,
	ASTERISK_GPL_KEY,
	AST_MODFLAG_LOAD_ORDER,
	AST_BUILDOPT_SUM,
	AST_MODPRI_CHANNEL_DRIVER,
	NULL,
};

static void __attribute__ ((constructor)) __reg_module(void)
{
	ast_module_register(&__mod_info);
}

static void __attribute__ ((destructor)) __unreg_module(void)
{
	ast_module_unregister(&__mod_info);
}

static const __attribute__ ((unused))
struct ast_module_info *ast_module_info = &__mod_info;
#else
#if ASTERISK_VERSION_NUMBER > 10601
AST_MODULE_INFO(ASTERISK_GPL_KEY, SCCP_VERSIONSTR,.load = load_module,.unload = unload_module,.reload = module_reload,.load_pri = AST_MODPRI_DEFAULT,.description = "Skinny Client Control Protocol (SCCP)",);
#else
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, SCCP_VERSIONSTR);
#endif
#endif

#if UNUSEDCODE // 2015-11-01
PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data)
{
	PBX_CHANNEL_TYPE *remotePeer;

	remotePeer = ast_channel_search_locked(found_cb, data);
	return remotePeer;
}
#endif

/* sccp_wrapper_asterisk16_findPickupChannelByExtenLocked helper functions */
/* Helper function that determines whether a channel is capable of being picked up */
static int can_pickup(struct ast_channel *chan)
{
	if (!chan->pbx && !chan->masq && !ast_test_flag(chan, AST_FLAG_ZOMBIE) && (chan->_state == AST_STATE_RINGING || chan->_state == AST_STATE_RING || chan->_state == AST_STATE_DOWN)) {
		return 1;
	}
	return 0;
}

struct pickup_criteria {
	const char *exten;
	const char *context;
	struct ast_channel *chan;
};

static int sccp_find_pbxchannel_by_exten(PBX_CHANNEL_TYPE * c, void *data)
{
	struct pickup_criteria *info = data;

	return (!strcasecmp(c->macroexten, info->exten) || !strcasecmp(c->exten, info->exten)) && !strcasecmp(c->dialcontext, info->context) && (info->chan != c) && can_pickup(c);
}

/* end sccp_wrapper_asterisk16_findPickupChannelByExtenLocked helper functions */

PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE * chan, const char *exten, const char *context)
{
	struct ast_channel *target = NULL;									/*!< Potential pickup target */

	struct pickup_criteria search = {
		.exten = exten,
		.context = context,
		.chan = chan,
	};
	target = ast_channel_search_locked(sccp_find_pbxchannel_by_exten, &search);
	return target;
}

static int find_channel_by_group(struct ast_channel *c, void *data)
{
	struct ast_channel *chan = data;

	return !c->pbx &&
		/* Accessing 'chan' here is safe without locking, because there is no way for
		   the channel to disappear from under us at this point.  pickupgroup *could*
		   change while we're here, but that isn't a problem. */
		(c != chan) &&
		(chan->pickupgroup & c->callgroup) &&
		((c->_state == AST_STATE_RINGING) || (c->_state == AST_STATE_RING)) &&
		!c->masq &&
		!ast_test_flag(c, AST_FLAG_ZOMBIE);
}

PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByGroupLocked(PBX_CHANNEL_TYPE * chan)
{
	struct ast_channel *target = NULL;									/*!< Potential pickup target */
	if ((target = ast_channel_search_locked(find_channel_by_group, chan))) {
		ast_channel_lock(target);
		pbx_channel_ref(target);
	}
	return target;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
