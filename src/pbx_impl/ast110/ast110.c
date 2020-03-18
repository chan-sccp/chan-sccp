/*!
 * \file        ast110.c
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
#include "sccp_linedevice.h"
#include "sccp_cli.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_hint.h"
#include "sccp_appfunctions.h"
#include "sccp_management.h"
#include "sccp_netsock.h"
#include "sccp_rtp.h"
#include "sccp_session.h"		// required for sccp_session_getOurIP
#include "sccp_labels.h"
#include "pbx_impl/ast110/ast110.h"

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
#include <asterisk/cel.h>
#include <asterisk/netsock2.h>
#define new avoid_cxx_new_keyword
#include <asterisk/rtp_engine.h>
#undef new
#include <asterisk/timing.h>
__END_C_EXTERN__

static struct ast_sched_context *sched = 0;
static struct io_context *io = 0;

#ifdef CS_SCCP_CONFERENCE
static struct ast_format slinFormat = { AST_FORMAT_SLINEAR, {{0}, 0} };
#endif
static PBX_CHANNEL_TYPE *sccp_astwrap_request(const char *type, struct ast_format_cap *format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause);
static int sccp_astwrap_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout);
static int sccp_astwrap_answer(PBX_CHANNEL_TYPE * chan);
static PBX_FRAME_TYPE *sccp_astwrap_rtp_read(PBX_CHANNEL_TYPE * ast);
static int sccp_astwrap_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame);
static int sccp_astwrap_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen);
static int sccp_astwrap_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan);
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE * ast, const char *text);
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE * ast, char digit);
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE * ast, char digit, unsigned int duration);
static int sccp_pbx_sendHTML(PBX_CHANNEL_TYPE * ast, int subclass, const char *data, int datalen);
int sccp_astwrap_hangup(PBX_CHANNEL_TYPE * ast_channel);
int sccp_astwrap_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control);
int sccp_astwrap_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen);
static int sccp_astwrap_devicestate(void *data);
PBX_CHANNEL_TYPE *sccp_astwrap_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE * chan, const char *exten, const char *context);
PBX_CHANNEL_TYPE *sccp_astwrap_findPickupChannelByGroupLocked(PBX_CHANNEL_TYPE * chan);

static skinny_codec_t sccp_astwrap_getSkinnyFormatSingle(struct ast_format_cap *ast_format_capability)
{
	struct ast_format tmp_fmt;
	skinny_codec_t codec = SKINNY_CODEC_NONE;

	ast_format_cap_iter_start(ast_format_capability);
	while (!ast_format_cap_iter_next(ast_format_capability, &tmp_fmt)) {
		if ((codec = pbx_codec2skinny_codec(tmp_fmt.id)) != SKINNY_CODEC_NONE) {
			break;
		}
	}
	ast_format_cap_iter_end(ast_format_capability);

	if (codec == SKINNY_CODEC_NONE) {
		ast_log(LOG_WARNING, "SCCP: (getSkinnyFormatSingle) No matching codec found");
	}

	return codec;
}

static uint8_t sccp_astwrap_getSkinnyFormatMultiple(struct ast_format_cap *ast_format_capability, skinny_codec_t codec[], int length)
{
	struct ast_format tmp_fmt;
	uint8_t position = 0;
	skinny_codec_t found = SKINNY_CODEC_NONE;

	ast_format_cap_iter_start(ast_format_capability);
	while (!ast_format_cap_iter_next(ast_format_capability, &tmp_fmt) && position < length) {
		if ((found = pbx_codec2skinny_codec(tmp_fmt.id)) != SKINNY_CODEC_NONE) {
			codec[position++] = found;
		}
	}
	ast_format_cap_iter_end(ast_format_capability);

	return position;
}

#if defined(__cplusplus) || defined(c_plusplus)

/*!
 * \brief SCCP Tech Structure
 */
static struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */	
	type:			SCCP_TECHTYPE_STR,
	description:		"Skinny Client Control Protocol (SCCP)",
//      capabilities:		AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_SLINEAR16 | AST_FORMAT_GSM | AST_FORMAT_G723_1 | AST_FORMAT_G729A | AST_FORMAT_H264 | AST_FORMAT_H263_PLUS,
	properties:		AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	requester:		sccp_astwrap_request,
	devicestate:		sccp_astwrap_devicestate,
	send_digit_begin:	sccp_wrapper_recvdigit_begin,
	send_digit_end:		sccp_wrapper_recvdigit_end,
	call:			sccp_astwrap_call,
	hangup:			sccp_astwrap_hangup,
	answer:			sccp_astwrap_answer,
	read:			sccp_astwrap_rtp_read,
	write:			sccp_astwrap_rtp_write,
	send_text:		sccp_pbx_sendtext,
	send_image:		NULL,
	send_html:		sccp_pbx_sendHTML,
	exception:		NULL,
//	bridge:			sccp_astwrap_rtpBridge,
	bridge:			ast_rtp_instance_bridge,
	early_bridge:		ast_rtp_instance_early_bridge,
	indicate:		sccp_astwrap_indicate,
	fixup:			sccp_astwrap_fixup,
	setoption:		NULL,
	queryoption:		NULL,
	transfer:		NULL,
	write_video:		sccp_astwrap_rtp_write,
	write_text:		NULL,
	bridged_channel:	NULL,
	func_channel_read:	sccp_astgenwrap_channel_read,
	func_channel_write:	sccp_astgenwrap_channel_write,
	get_base_channel:	NULL,
	set_base_channel:	NULL
	/* *INDENT-ON* */
};

#else

/*!
 * \brief SCCP Tech Structure
 */
struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */
	.type 			= SCCP_TECHTYPE_STR,
	.description 		= "Skinny Client Control Protocol (SCCP)",
	// we could use the skinny_codec = ast_codec mapping here to generate the list of capabilities
//      .capabilities           = AST_FORMAT_SLINEAR16 | AST_FORMAT_SLINEAR | AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_GSM | AST_FORMAT_G723_1 | AST_FORMAT_G729A,
	.properties 		= AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester 		= sccp_astwrap_request,
	.devicestate 		= sccp_astwrap_devicestate,
	.call 			= sccp_astwrap_call,
	.hangup 		= sccp_astwrap_hangup,
	.answer 		= sccp_astwrap_answer,
	.read 			= sccp_astwrap_rtp_read,
	.write 			= sccp_astwrap_rtp_write,
	.write_video 		= sccp_astwrap_rtp_write,
	.indicate 		= sccp_astwrap_indicate,
	.fixup 			= sccp_astwrap_fixup,
	//.transfer 		= sccp_pbx_transfer,
	//.bridge 		= sccp_astwrap_rtpBridge,
	.bridge 		= ast_rtp_instance_bridge,
	.early_bridge           = ast_rtp_instance_early_bridge,
	//.bridged_channel      =

	.send_text 		= sccp_pbx_sendtext,
	.send_html 		= sccp_pbx_sendHTML,
	//.send_html            =
	//.send_image           =

	.func_channel_read 	= sccp_astgenwrap_channel_read,
	.func_channel_write 	= sccp_astgenwrap_channel_write,

	.send_digit_begin 	= sccp_wrapper_recvdigit_begin,
	.send_digit_end 	= sccp_wrapper_recvdigit_end,

	//.write_text           = 
	//.write_video          =
	//.cc_callback          =                                              // ccss, new >1.6.0
	//.exception            =                                              // new >1.6.0
//      .setoption              = sccp_astwrap_setOption,
	//.queryoption          =                                              // new >1.6.0
	//.get_pvt_uniqueid     = sccp_pbx_get_callid,                         // new >1.6.0
	//.get_base_channel     =
	//.set_base_channel     =
	/* *INDENT-ON* */
};

#endif

static int sccp_astwrap_devicestate(void *data)
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
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (devicestate) sccp_hint returned state:%s for '%s'\n", sccp_channelstate2str(state), lineName);
	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
		case SCCP_CHANNELSTATE_ONHOOK:
			res = AST_DEVICE_NOT_INUSE;
			break;
		case SCCP_CHANNELSTATE_RINGING:
			res = AST_DEVICE_RINGING;
			break;
		case SCCP_CHANNELSTATE_HOLD:
			res = AST_DEVICE_ONHOLD;
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
		case SCCP_CHANNELSTATE_CALLWAITING:
#ifndef CS_EXPERIMENTAL
			res = AST_DEVICE_RINGINUSE;
			break;
#endif
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

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (devicestate) PBX requests state for '%s' - state %s\n", lineName, ast_devstate2str(res));
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
	struct ast_format *dst = NULL;
	uint32_t codec = skinny_codecs2pbx_codecs(codecs);							// convert to bitfield

	dst = ast_format_from_old_bitfield(dst, codec);								// convert bitfield to ast_format
	return ast_codec_pref_append(astCodecPref, dst);							// return ast_codec_pref
}

static boolean_t sccp_astwrap_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec);

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

// static void get_skinnyFormats(struct ast_format_cap *format, skinny_codec_t codecs[], size_t size)
// {
//      unsigned int x;
//      unsigned len = 0;
// 
//      size_t f_len;
//      struct ast_format tmp_fmt;
//      const struct ast_format_list *f_list = ast_format_list_get(&f_len);
// 
//      if (!size) {
//              f_list = ast_format_list_destroy(f_list);
//              return;
//      }
// 
//      for (x = 0; x < ARRAY_LEN(pbx2skinny_codec_maps) && len <= size; x++) {
//              ast_format_copy(&tmp_fmt, &f_list[x].format);
//              if (ast_format_cap_iscompatible(format, &tmp_fmt)) {
//                      if (pbx2skinny_codec_maps[x].pbx_codec == ((uint) tmp_fmt.id)) {
//                              codecs[len++] = pbx2skinny_codec_maps[x].skinny_codec;
//                      }
//              }
//      }
//      f_list = ast_format_list_destroy(f_list);
// }

/*************************************************************************************************************** CODEC **/

/*! \brief Get the name of a format
 * \note replacement for ast_getformatname
 * \param format id of format
 * \return A static string containing the name of the format or "unknown" if unknown.
 */
const char *pbx_getformatname(const struct ast_format *format)
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
char *pbx_getformatname_multiple(char *buf, size_t size, struct ast_format_cap *format)
{
	return ast_getformatname_multiple(buf, size, format);
}

/*!
 * \brief Read from an Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 *
 * \called_from_asterisk
 *
 * \note not following the refcount rules... channel is already retained
 */
static PBX_FRAME_TYPE *sccp_astwrap_rtp_read(PBX_CHANNEL_TYPE * ast)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);									// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	PBX_FRAME_TYPE *frame = NULL;

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
		if (c->conference && (!ast_format_is_slinear(&ast->readformat))) {
			ast_set_read_format(ast, &slinFormat);
		} else
#endif
		{
			if (ast_format_cmp(&frame->subclass.format, &ast->rawreadformat) == AST_FORMAT_CMP_NOT_EQUAL) {
				ast_format_cap_remove_bytype(ast->nativeformats, AST_FORMAT_TYPE_AUDIO);
				ast_format_cap_add(ast->nativeformats, &frame->subclass.format);
				ast_set_read_format(ast, &ast->readformat);
				ast_set_write_format(ast, &ast->writeformat);
				//ast_rtp_instance_set_read_format(channel->rtp.audio.instance, &ast->readformat);
				//ast_rtp_instance_set_write_format(channel->rtp.audio.instance, &ast->writeformat);
			}
		}
	}
	
	return frame;
}

/*!
 * \brief Find Asterisk/PBX channel by linkid
 *
 * \param ast   pbx channel
 * \param data  linkId as void *
 *
 * \return int
 */
static int pbx_find_channel_by_linkid(PBX_CHANNEL_TYPE * ast, const void *data)
{
	const char *linkId = (char *) data;

	if (!data) {
		return 0;
	}
	return !ast->pbx && ast->linkedid && (!strcasecmp(ast->linkedid, linkId)) && !ast->masq;
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
		case AST_CONTROL_TRANSFER: return "AST_CONTROL_TRANSFER: Indicate status of a transfer request";
		case AST_CONTROL_CONNECTED_LINE: return "AST_CONTROL_CONNECTED_LINE: Indicate connected line has changed";
		case AST_CONTROL_REDIRECTING: return "AST_CONTROL_REDIRECTING: Indicate redirecting id has changed";
		case AST_CONTROL_T38_PARAMETERS: return "AST_CONTROL_T38_PARAMETERS: T38 state change request/notification with parameters";
		case AST_CONTROL_CC: return "AST_CONTROL_CC: Indication that Call completion service is possible";
		case AST_CONTROL_SRCCHANGE: return "AST_CONTROL_SRCCHANGE: Media source has changed and requires a new RTP SSRC";
		case AST_CONTROL_READ_ACTION: return "AST_CONTROL_READ_ACTION: Tell ast_read to take a specific action";
		case AST_CONTROL_AOC: return "AST_CONTROL_AOC: Advice of Charge with encoded generic AOC payload";
		case AST_CONTROL_END_OF_Q: return "AST_CONTROL_END_OF_Q: Indicate that this position was the end of the channel queue for a softhangup.";
		case AST_CONTROL_INCOMPLETE: return "AST_CONTROL_INCOMPLETE: Indication that the extension dialed is incomplete";
		case AST_CONTROL_MCID: return "AST_CONTROL_MCID: Indicate that the caller is being malicious.";
		case AST_CONTROL_UPDATE_RTP_PEER: return "AST_CONTROL_UPDATE_RTP_PEER: Interrupt the bridge and have it update the peer";
		case -1: return "AST_CONTROL_PROD: Kick remote channel";
	}
	return "Unknown/Unhandled/IAX Indication";
}

static int sccp_astwrap_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
{
	int res = 0;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));
	if (!c) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: (pbx_indicate) no sccp channel yet\n");
		return -1;
	}
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: (pbx_indicate) start indicate '%s'\n", c->designator, asterisk_indication2str(ind));

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (!d || c->state == SCCP_CHANNELSTATE_DOWN) {
		//sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: (pbx_indicate) no sccp device yet\n");
		switch (ind) {
			case AST_CONTROL_CONNECTED_LINE:
				sccp_astwrap_connectedline(c, data, datalen);
				res = 0;
				break;
			case AST_CONTROL_REDIRECTING:
				sccp_astwrap_redirectedUpdate(c, data, datalen);
				res = 0;
				break;
			default:
				res = -1;
				break;
		}
		return res;
	}


	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = (((c->rtp.audio.reception.state != SCCP_RTP_STATUS_INACTIVE) || (d && d->earlyrtp)) ? -1 : 0);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: (pbx_indicate) start indicate '%s' (%d) condition on channel %s (readStat:%d, reception.state:%d, rtp:%s, default res:%s (%d))\n", DEV_ID_LOG(d), asterisk_indication2str(ind), ind, pbx_channel_name(ast), c->rtp.audio.reception.state, c->rtp.audio.reception.state, (c->rtp.audio.instance) ? "yes" : "no", res ? "inband signaling" : "outofband signaling", res);

	switch (ind) {
		case AST_CONTROL_RINGING:
			if (SKINNY_CALLTYPE_OUTBOUND == c->calltype && pbx_channel_state(c->owner) !=  AST_STATE_UP) {
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
					AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_find(d, c->line));
					if(ld) {
						sccp_device_setLastNumberDialed(d, c->dialedNumber, ld);
					}
				}
				iPbx.set_callstate(c, AST_STATE_RING);
				struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

				((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

				/*! \todo handle multiple remotePeers i.e. DIAL(SCCP/400&SIP/300), find smallest common codecs, what order to use ? */
				PBX_CHANNEL_TYPE *remotePeer;

				for (; (remotePeer = ast_channel_iterator_next(iterator)); remotePeer = pbx_channel_unref(remotePeer)) {
					if (pbx_find_channel_by_linkid(remotePeer, (void *) ast->linkedid)) {
						char buf[512];
						
						AUTO_RELEASE(sccp_channel_t, remoteSccpChannel , get_sccp_channel_from_pbx_channel(remotePeer));
						if (remoteSccpChannel) {
							sccp_codec_multiple2str(buf, sizeof(buf) - 1, remoteSccpChannel->preferences.audio, ARRAY_LEN(remoteSccpChannel->preferences.audio));
							sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote preferences: %s\n", buf);
							uint8_t x, y, z;

							z = 0;
							for (x = 0; x < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->preferences.audio[x] != 0; x++) {
								for (y = 0; y < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->capabilities.audio[y] != 0; y++) {
									if (remoteSccpChannel->preferences.audio[x] == remoteSccpChannel->capabilities.audio[y]) {
										c->remoteCapabilities.audio[z++] = remoteSccpChannel->preferences.audio[x];
										break;
									}
								}
							}
						} else {
							sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote nativeformats: %s\n", pbx_getformatname_multiple(buf, sizeof(buf) - 1, remotePeer->nativeformats));
							sccp_astwrap_getSkinnyFormatMultiple(remotePeer->nativeformats, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
						}

						sccp_codec_multiple2str(buf, sizeof(buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
						sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote caps: %s\n", buf);
						remotePeer = pbx_channel_unref(remotePeer);
						break;
					}

				}
				ast_channel_iterator_destroy(iterator);
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
			res = 0;
			break;
		case AST_CONTROL_PROCEEDING:
			if (d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE) {
				/* 
					* Redial button isnt't working properly in immediate mode, because the
					* last dialed number was being remembered too early. This fix
					* remembers the last dialed number in the same cases, where the dialed number
					* is being sent - after receiving of PROCEEDING -Pavel Troller
					*/
				AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_find(d, c->line));
				if(ld) {
					sccp_device_setLastNumberDialed(d, c->dialedNumber, ld);
				}
			}
			sccp_indicate(d, c, SCCP_CHANNELSTATE_PROCEED);
			res = 0;
			break;
		case AST_CONTROL_SRCCHANGE:
			if (c->rtp.audio.instance) {
				ast_rtp_instance_change_source(c->rtp.audio.instance);
			}
			res = 0;
			break;

		case AST_CONTROL_SRCUPDATE:
			/* Source media has changed. */
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

			if (c->rtp.audio.instance) {
				ast_rtp_instance_change_source(c->rtp.audio.instance);
			}

		/** this is a dirty workaround to fix audio issue while pickup a parked call
		 * reason: asterisk do not indicate connected if we dial to a parked extension
		 * -MC
		 */
			if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
				sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: force CONNECT\n");
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
			}
			res = 0;
			break;

			/* when the bridged channel hold/unhold the call we are notified here */
		case AST_CONTROL_HOLD:
			if (c->rtp.audio.instance) {
				ast_rtp_instance_update_source(c->rtp.audio.instance);
			}
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.instance && d && sccp_device_isVideoSupported(d) && sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF) {
				ast_rtp_instance_update_source(c->rtp.video.instance);
				d->protocol->sendMultiMediaCommand(d, c, SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE);
			}
#endif
			sccp_astwrap_moh_start(ast, (const char *) data, c->musicclass);
			res = 0;
			break;
		case AST_CONTROL_UNHOLD:
 			if (c->rtp.audio.instance) {
 				ast_rtp_instance_update_source(c->rtp.audio.instance);
 			}
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.instance && d && sccp_device_isVideoSupported(d) && sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF) {
				ast_rtp_instance_update_source(c->rtp.video.instance);
				d->protocol->sendMultiMediaCommand(d, c, SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);
			};
#endif
			sccp_astwrap_moh_stop(ast);
			res = 0;
			break;

		case AST_CONTROL_CONNECTED_LINE:
			/* remarking out this code, as it is causing issues with callforward + FREEPBX,  the calling party will not hear the remote end ringing
			 this patch was added to suppress 'double callwaiting tone', but channel PROD(-1) below is taking care of that already
			*/
			//if (c->calltype == SKINNY_CALLTYPE_OUTBOUND && c->rtp.audio.reception.state == SCCP_RTP_STATUS_INACTIVE && c->state > SCCP_CHANNELSTATE_DIALING) {
			//	sccp_channel_openReceiveChannel(c);
			//}
			sccp_astwrap_connectedline(c, data, datalen);
			res = 0;
			break;

		case AST_CONTROL_TRANSFER:
			ast_log(LOG_NOTICE, "%s: Ast Control Transfer: %d", c->designator, *(int *)data);
			//sccp_astwrap_connectedline(c, data, datalen);
			res = 0;
			break;

		case AST_CONTROL_REDIRECTING:
			sccp_astwrap_redirectedUpdate(c, data, datalen);
			sccp_indicate(d, c, c->state);
			res = 0;
			break;

		case AST_CONTROL_VIDUPDATE:									/* Request a video frame update */
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.instance && d && sccp_device_isVideoSupported(d) && sccp_channel_getVideoMode(c) != SCCP_VIDEO_MODE_OFF) {
				d->protocol->sendMultiMediaCommand(d, c, SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);
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
					sccp_channel_schedule_digittimeout(c, c->enbloc.digittimeout);
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
				c->rtp.audio.reception.state == SCCP_RTP_STATUS_INACTIVE &&
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
static int sccp_astwrap_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);									// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	int res = 0;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {									// not following the refcount rules... channel is already retained
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
				}
			}
			if (c->rtp.audio.instance) {
				res = ast_rtp_instance_write(c->rtp.audio.instance, frame);
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.reception.state == SCCP_RTP_STATUS_INACTIVE && c->rtp.video.instance && c->state != SCCP_CHANNELSTATE_HOLD
			    //      && (c->device->capability & frame->subclass)
			    ) {
				// int codec = pbx_codec2skinny_codec((frame->subclass.codec & AST_FORMAT_VIDEO_MASK));
				int codec = pbx_codec2skinny_codec((frame->frametype == AST_FRAME_VIDEO));

				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: got video frame %d\n", c->currentDeviceId, codec);
				if (0 != codec) {
					c->rtp.video.reception.format = codec;
					sccp_channel_openMultiMediaReceiveChannel(c);
				}
			}

			if (c->rtp.video.instance && (c->rtp.video.reception.state & SCCP_RTP_STATUS_ACTIVE) != 0) {
				res = ast_rtp_instance_write(c->rtp.video.instance, frame);
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

static void sccp_astwrap_setCalleridPresentation(PBX_CHANNEL_TYPE *pbx_channel, sccp_callerid_presentation_t presentation)
{
	if (pbx_channel && CALLERID_PRESENTATION_FORBIDDEN == presentation) {
		pbx_channel->caller.id.name.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
		pbx_channel->caller.id.number.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
	}
}

static int sccp_astwrap_setNativeAudioFormats(const sccp_channel_t * channel, skinny_codec_t codecs[])
{
	struct ast_format fmt;
	int i;
	if (!channel || !channel->owner || !channel->owner->nativeformats) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_wrapper_asterisk111_setNativeAudioFormats) no channel provided!\n");
		return 0;
	}

	int length = 1;
	ast_debug(10, "%s: set native Formats length: %d\n", (char *) channel->currentDeviceId, length);

	ast_format_cap_remove_bytype(channel->owner->nativeformats, AST_FORMAT_TYPE_AUDIO);
	for (i = 0; i < length; i++) {
		ast_format_set(&fmt, skinny_codec2pbx_codec(codecs[i]), 0);
		ast_format_cap_add(channel->owner->nativeformats, &fmt);
	}
	return 1;
}

static int sccp_astwrap_setNativeVideoFormats(const sccp_channel_t * channel, skinny_codec_t codecs[])
{
	return 1;
}

static void sccp_astwrap_removeTimingFD(PBX_CHANNEL_TYPE *ast)
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

static void sccp_astwrap_setOwner(sccp_channel_t * channel, PBX_CHANNEL_TYPE * pbx_channel)
{
	PBX_CHANNEL_TYPE *prev_owner = channel->owner;

	if (pbx_channel) {
		channel->owner = pbx_channel_ref(pbx_channel);
		ast_module_ref(ast_module_info->self);
	} else {
		channel->owner = NULL;
	}
	if (prev_owner) {
		pbx_channel_unref(prev_owner);
		ast_module_unref(ast_module_info->self);
	}
}

static void __sccp_astwrap_updateConnectedLine(PBX_CHANNEL_TYPE *pbx_channel, const char *number, const char *name, uint8_t reason)
{
	if (!pbx_channel) {
		return;
	}
	struct ast_party_connected_line connected;
	struct ast_set_party_connected_line update_connected = {{0}};

	ast_party_connected_line_init(&connected);
	if (number) {
		update_connected.id.number = 1;
		connected.id.number.valid = 1;
		connected.id.number.str = pbx_strdupa(number);
		connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}
	if (name) {
		update_connected.id.name = 1;
		connected.id.name.valid = 1;
		connected.id.name.str = pbx_strdupa(name);
		connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}
	if (update_connected.id.number || update_connected.id.name) {
		//ast_set_party_id_all(&update_connected.priv);
		// connected.id.tag = NULL;
		connected.source = reason;
		ast_channel_queue_connected_line_update(pbx_channel, &connected, &update_connected);
		sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "SCCP: do connected line for line '%s', name: %s ,num: %s\n", pbx_channel_name(pbx_channel), name ? name : "(NULL)", number ? number : "(NULL)");
	}
}

static boolean_t sccp_astwrap_allocPBXChannel(sccp_channel_t * channel, const void *ids, const PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** _pbxDstChannel)
{
	const char *linkedId = ids ? pbx_strdupa(ids) : NULL;
	PBX_CHANNEL_TYPE *pbxDstChannel = NULL;
	if (!channel || !channel->line) {
		return FALSE;
	}
	AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(channel->line));
	if (!line) {
		return FALSE;
	}

	pbxDstChannel = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, linkedId, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);

	if (pbxDstChannel == NULL) {
		return FALSE;
	}

	pbxDstChannel->tech = &sccp_tech;
	pbxDstChannel->tech_pvt = sccp_channel_retain(channel);

	/* Copy Codec from SrcChannel */
	struct ast_format tmpfmt;

	if (!pbxSrcChannel || ast_format_cap_is_empty(pbx_channel_nativeformats(pbxSrcChannel))) {
		ast_format_set(&tmpfmt, AST_FORMAT_ULAW, 0);
	} else {
		ast_best_codec(pbx_channel_nativeformats(pbxSrcChannel), &tmpfmt);
	}
	ast_format_cap_add(pbxDstChannel->nativeformats, &tmpfmt);
	ast_set_read_format(pbxDstChannel, &tmpfmt);
	ast_set_write_format(pbxDstChannel, &tmpfmt);
	/* EndCodec */

	memset(pbxDstChannel->exten, 0, sizeof(pbxDstChannel->exten));
	pbxDstChannel->priority = 1;
	pbxDstChannel->adsicpe = AST_ADSI_UNAVAILABLE;

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

	/** the the tonezone using language information */
	if (!sccp_strlen_zero(line->language) && ast_get_indication_zone(line->language)) {
		pbxDstChannel->zone = ast_get_indication_zone(line->language);					/* this will core asterisk on hangup */
	}
	sccp_astwrap_setOwner(channel, pbxDstChannel);

	(*_pbxDstChannel) = pbxDstChannel;

	return TRUE;
}

static boolean_t sccp_astwrap_masqueradeHelper(PBX_CHANNEL_TYPE * pbxChannel, PBX_CHANNEL_TYPE * pbxTmpChannel)
{
	pbx_moh_stop(pbxChannel);
	// Masquerade setup and execution must be done without any channel locks held 
	if (pbx_channel_masquerade(pbxTmpChannel, pbxChannel)) {
		return FALSE;
	}
	//pbxTmpChannel->_state = AST_STATE_UP;
	pbx_do_masquerade(pbxTmpChannel);

	// when returning from bridge, the channel will continue at the next priority
	// ast_explicit_goto(pbxTmpChannel, pbx_channel_context(pbxTmpChannel), pbx_channel_exten(pbxTmpChannel), pbx_channel_priority(pbxTmpChannel) + 1);
	return TRUE;
}

static boolean_t sccp_astwrap_allocTempPBXChannel(PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** _pbxDstChannel)
{
	struct ast_format tmpfmt;
	PBX_CHANNEL_TYPE *pbxDstChannel = NULL;

	if (!pbxSrcChannel) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) no pbx channel provided\n");
		return FALSE;
	}
	pbxDstChannel = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, pbxSrcChannel->accountcode, pbxSrcChannel->exten, pbxSrcChannel->context, pbxSrcChannel->linkedid, pbxSrcChannel->amaflags, "TMP/%s", pbxSrcChannel->name);
	if (pbxDstChannel == NULL) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) create pbx channel failed\n");
		return FALSE;
	}

	ast_channel_lock(pbxSrcChannel);

	if (ast_format_cap_is_empty(pbxSrcChannel->nativeformats)) {
		ast_format_set(&tmpfmt, AST_FORMAT_ULAW, 0);
	} else {
		ast_best_codec(pbxSrcChannel->nativeformats, &tmpfmt);
	}
	ast_format_cap_add(pbxDstChannel->nativeformats, &tmpfmt);
	ast_set_read_format(pbxDstChannel, &tmpfmt);
	ast_set_write_format(pbxDstChannel, &tmpfmt);

	sccp_copy_string(pbxDstChannel->context, pbxSrcChannel->context, sizeof(pbxDstChannel->context));
	sccp_copy_string(pbxDstChannel->exten, pbxSrcChannel->exten, sizeof(pbxDstChannel->exten));
	pbxDstChannel->priority = pbxSrcChannel->priority;
	ast_channel_unlock(pbxSrcChannel);

	(*_pbxDstChannel) = pbxDstChannel;
	return TRUE;
}

static PBX_CHANNEL_TYPE *sccp_astwrap_requestAnnouncementChannel(pbx_format_enum_type format, const PBX_CHANNEL_TYPE * requestor, void *data)
{
	PBX_CHANNEL_TYPE *chan;
	int cause;
	struct ast_format_cap *cap;
	struct ast_format tmpfmt;

	if (!(cap = ast_format_cap_alloc_nolock())) {
		return 0;
	}
	ast_format_cap_add(cap, ast_format_set(&tmpfmt, format, 0));
	if (!(chan = ast_request("Bridge", cap, NULL, "", &cause))) {
		pbx_log(LOG_ERROR, "SCCP: Requested channel could not be created, cause: %d\n", cause);
		cap = ast_format_cap_destroy(cap);
		return NULL;
	}
	cap = ast_format_cap_destroy(cap);
	return chan;
}

int sccp_astwrap_hangup(PBX_CHANNEL_TYPE * ast_channel)
{
	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast_channel));
	int res = -1;

	if (c) {
		if (pbx_channel_hangupcause(ast_channel) == AST_CAUSE_ANSWERED_ELSEWHERE) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere\n");
			c->answered_elsewhere = TRUE;
		}
		/* postponing pbx_channel_unref to sccp_channel destructor */
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
 * \brief Park the bridge channel of hostChannel
 * This function prepares the host and the bridged channel to be ready for parking.
 * It clones the pbx channel of both sides forward them to the park_thread
 * 
 * \param hostChannel initial channel that request the parking
 * \todo we have a codec issue after unpark a call
 * \todo copy connected line info
 * 
 */
static sccp_parkresult_t sccp_astwrap_park(const sccp_channel_t * hostChannel)
{
	int extout;
	char extstr[20];
	sccp_parkresult_t res = PARK_RESULT_FAIL;
	PBX_CHANNEL_TYPE *bridgedChannel = NULL;

	memset(extstr, 0, sizeof(extstr));

	if ((bridgedChannel = ast_bridged_channel(hostChannel->owner))) {
		AUTO_RELEASE(sccp_device_t, device , sccp_channel_getDevice(hostChannel));
		if (device) {
			ast_channel_lock(hostChannel->owner);							/* we have to lock our channel, otherwise asterisk crashes internally */
			if (!ast_masq_park_call(bridgedChannel, hostChannel->owner, 0, &extout)) {
				snprintf(extstr, sizeof(extstr), "%c%c %d" , 128, SKINNY_LBL_CALL_PARK_AT, extout);

				sccp_dev_displayprinotify(device, extstr, 1, 10);
				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Parked channel %s on %d\n", DEV_ID_LOG(device), bridgedChannel->name, extout);

				res = PARK_RESULT_SUCCESS;
			}
			ast_channel_unlock(hostChannel->owner);
		}
	}
	return res;
}

static boolean_t sccp_astwrap_getFeatureExtension(const sccp_channel_t * channel, const char *featureName, char extension[SCCP_MAX_EXTENSION])
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

static boolean_t sccp_astwrap_getPickupExtension(const sccp_channel_t * channel, char extension[SCCP_MAX_EXTENSION])
{
	boolean_t res = FALSE;

	if (!sccp_strlen_zero(ast_pickup_ext())) {
		sccp_copy_string(extension, ast_pickup_ext(), SCCP_MAX_EXTENSION);
		res = TRUE;
	}
	return res;
}

static uint8_t sccp_astwrap_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	struct ast_format astCodec;
	int payload;

	ast_format_set(&astCodec, skinny_codec2pbx_codec(codec), 0);
	payload = ast_rtp_codecs_payload_code(ast_rtp_instance_get_codecs(rtp->instance), 1, &astCodec, 0);

	return payload;
}

static int sccp_astwrap_get_sampleRate(skinny_codec_t codec)
{
	struct ast_format astCodec;

	ast_format_set(&astCodec, skinny_codec2pbx_codec(codec), 0);
	return ast_rtp_lookup_sample_rate2(1, &astCodec, 0);
}

static sccp_extension_status_t sccp_astwrap_extensionStatus(const sccp_channel_t * channel)
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

static PBX_CHANNEL_TYPE *sccp_astwrap_request(const char *type, struct ast_format_cap *format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause)
{
	PBX_CHANNEL_TYPE *result_ast_channel = NULL;
	sccp_channel_request_status_t requestStatus;

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
	lineName = pbx_strdupa((const char *) data);
	/* parsing options string */
	char *options = NULL;
	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked us to create a channel with type=%s, format=" UI64FMT " lineName=%s, options=%s\n", type, (ULONG) ast_format_cap_to_old_bitfield(format), lineName, (options) ? options : "");
	if (requestor) {							/* get ringer mode from ALERT_INFO */
		sccp_parse_alertinfo((PBX_CHANNEL_TYPE *)requestor, &ringermode);
	}
	sccp_parse_dial_options(options, &autoanswer_type, &autoanswer_cause, &ringermode);
	if (autoanswer_cause) {
		*cause = autoanswer_cause;
	}
	
	/** getting remote capabilities */
	char cap_buf[512];

	/* audio capabilities */
	if (requestor) {
		AUTO_RELEASE(sccp_channel_t, remoteSccpChannel , get_sccp_channel_from_pbx_channel(requestor));
		if (remoteSccpChannel) {
			uint8_t x, y, z;

			z = 0;
			/* shrink audioCapabilities to remote preferred/capable format */
			for (x = 0; x < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->preferences.audio[x] != 0; x++) {
				for (y = 0; y < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->capabilities.audio[y] != 0; y++) {
					if (remoteSccpChannel->preferences.audio[x] == remoteSccpChannel->capabilities.audio[y]) {
						audioCapabilities[z++] = remoteSccpChannel->preferences.audio[x];
						break;
					}
				}
			}
		} else {
			sccp_astwrap_getSkinnyFormatMultiple(requestor->nativeformats, audioCapabilities, ARRAY_LEN(audioCapabilities));	// replace AUDIO_MASK with AST_FORMAT_TYPE_AUDIO check
		}

		/* video capabilities */
		sccp_astwrap_getSkinnyFormatMultiple(requestor->nativeformats, videoCapabilities, ARRAY_LEN(videoCapabilities));	//replace AUDIO_MASK with AST_FORMAT_TYPE_AUDIO check
	}

	sccp_codec_multiple2str(cap_buf, sizeof(cap_buf) - 1, audioCapabilities, ARRAY_LEN(audioCapabilities));
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote audio caps: %s\n", cap_buf);

	sccp_codec_multiple2str(cap_buf, sizeof(cap_buf) - 1, videoCapabilities, ARRAY_LEN(videoCapabilities));
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote video caps: %s\n", cap_buf);

	/** done */

	/** get requested format */
	// codec = pbx_codec2skinny_codec(ast_format_cap_to_old_bitfield(format));
	codec = sccp_astwrap_getSkinnyFormatSingle(format);

	/* get requested format */
	{
		struct ast_format tmpfmt;

		ast_format_cap_iter_start(format);
		while (!(ast_format_cap_iter_next(format, &tmpfmt))) {
			if (AST_FORMAT_GET_TYPE(tmpfmt.id) == AST_FORMAT_TYPE_AUDIO) {
				codec = pbx_codec2skinny_codec(tmpfmt.id);
				break;
			}
		}
		ast_format_cap_iter_end(format);
	}

	AUTO_RELEASE(sccp_channel_t, channel , NULL);
	requestStatus = sccp_requestChannel(lineName, autoanswer_type, autoanswer_cause, ringermode, &channel);
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

	memcpy(&channel->remoteCapabilities.audio, audioCapabilities, sizeof(channel->remoteCapabilities.audio));
#ifdef CS_SCCP_VIDEO
	memset(&channel->remoteCapabilities.video, 0, sizeof(channel->remoteCapabilities.video));
	if (videoCapabilities[0] != SKINNY_CODEC_NONE) {
		memcpy(channel->remoteCapabilities.video, videoCapabilities, ARRAY_LEN(videoCapabilities));
	}
#endif

	if (!sccp_pbx_channel_allocate(channel, requestor ? requestor->linkedid : NULL, requestor)) {
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	}

	/* set initial connected line information, to be exchange with remove party during first CONNECTED_LINE update */
	ast_set_callerid(channel->owner, channel->line->cid_num, channel->line->cid_name, channel->line->cid_num);
	struct ast_party_connected_line connected;
	ast_party_connected_line_set_init(&connected, &channel->owner->connected);
	connected.id.number.valid = 1;
	connected.id.number.str = (char *)channel->line->cid_num;
	connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	connected.id.name.valid = 1;
	connected.id.name.str = (char *)channel->line->cid_name;
	connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	connected.source = AST_CONNECTED_LINE_UPDATE_SOURCE_UNKNOWN;
	ast_channel_set_connected_line(channel->owner, &connected, NULL);
	/* end */

	if (requestor) {
		/* set calling party */
		sccp_callinfo_t *ci = sccp_channel_getCallInfo(channel);
		iCallInfo.Setter(ci, 
				SCCP_CALLINFO_CALLINGPARTY_NAME, requestor->caller.id.name.str,
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, requestor->caller.id.number.str,
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, requestor->redirecting.from.name.str,
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, requestor->redirecting.from.number.str,
				SCCP_CALLINFO_KEY_SENTINEL);

		if (requestor->linkedid) {
			ast_string_field_set(channel->owner, linkedid, requestor->linkedid);
		}
	}

EXITFUNC:

	if (channel) {
		result_ast_channel = channel->owner;
	}
	return result_ast_channel;
}

static int sccp_astwrap_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout)
{
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

	res = sccp_pbx_call(c, dest, timeout);
	return res;

}

static int sccp_astwrap_answer(PBX_CHANNEL_TYPE * pbxchan)
{
	//! \todo change this handling and split pbx and sccp handling -MC
	int res = -1;
	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(pbxchan));
	if (c) {
		if (pbx_channel_state(pbxchan) != AST_STATE_UP && c->state < SCCP_GROUPED_CHANNELSTATE_CONNECTION) {
			pbx_indicate(pbxchan, AST_CONTROL_PROGRESS);
		}
		res = sccp_pbx_answer(c);
	}
	return res;
}

/**
 * 
 * \todo update remote capabilities after fixup
 */
static int sccp_astwrap_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s and %s\n", pbx_channel_name(oldchan), pbx_channel_name(newchan));
	int res = 0;

	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(newchan));
	if (c) {
		if (c->owner != oldchan) {
			ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, c->owner);
			res = -1;
		} else {
			/* during a masquerade, fixup gets called twice, The Zombie channel name will have been changed to include '<ZOMBI>' */
			/* using test_flag for ZOMBIE cannot be used, as it is only set after the fixup call */
			if (!strstr(pbx_channel_name(newchan), "<ZOMBIE>")) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set c->hangupRequest = requestQueueHangup\n", c->designator);
				if (pbx_channel_hangupcause(newchan) == AST_CAUSE_ANSWERED_ELSEWHERE) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Fixup Adding Redirecting Party from:%s\n", c->designator, pbx_channel_name(oldchan));
					iCallInfo.Setter(sccp_channel_getCallInfo(c),
						SCCP_CALLINFO_HUNT_PILOT_NAME, oldchan->caller.id.name.str,
						SCCP_CALLINFO_HUNT_PILOT_NUMBER, oldchan->caller.id.number.str,
						SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, oldchan->caller.id.name.str,
						SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, oldchan->caller.id.number.str,
						SCCP_CALLINFO_LAST_REDIRECT_REASON, 5,
						SCCP_CALLINFO_KEY_SENTINEL);
				}

				// set channel requestHangup to use ast_queue_hangup (as it is now part of a __ast_pbx_run, after masquerade completes) 
				c->hangupRequest = sccp_astgenwrap_requestQueueHangup;
				if (!sccp_strlen_zero(c->line->language)) {
					ast_string_field_set(newchan, language, c->line->language);
				}
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set c->hangupRequest = requestHangup\n", c->designator);
				// set channel requestHangup to use ast_hangup (as it will not be part of __ast_pbx_run anymore, upon returning from masquerade) 
				c->hangupRequest = sccp_astgenwrap_requestHangup;
			}
			sccp_astwrap_setOwner(c, newchan);
			//! \todo force update of rtp peer for directrtp
			// sccp_wrapper_asterisk111_update_rtp_peer(newchan, NULL, NULL, 0, 0, 0);

			//! \todo update remote capabilities after fixup

		}
	} else {
		pbx_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", pbx_channel_name(oldchan), (void *) oldchan, pbx_channel_name(newchan), (void *) newchan);
		res = -1;
	}
	return res;
}

#if 0
static enum ast_bridge_result sccp_astwrap_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms)
{
	enum ast_bridge_result res;
	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	AUTO_RELEASE(sccp_channel_t, sc0 , get_sccp_channel_from_pbx_channel(c0));
	AUTO_RELEASE(sccp_channel_t, sc1 , get_sccp_channel_from_pbx_channel(c1));

	if ((sc0 && sc1)) {
		// Switch off DTMF between SCCP phones
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_0;
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_1;
		if (GLOB(directrtp)) {
			ast_channel_defer_dtmf(c0);
			ast_channel_defer_dtmf(c1);
		} else {
			AUTO_RELEASE(sccp_device_t, d0 , sccp_channel_getDevice(sc0));
			AUTO_RELEASE(sccp_device_t, d1 , sccp_channel_getDevice(sc1));
			if ((d0 && d1) && ((d0->directrtp && d1->directrtp)) {
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

	//res = ast_rtp_bridge(c0, c1, new_flags, fo, rc, timeoutms);
	res = ast_rtp_instance_bridge(c0, c1, new_flags, fo, rc, timeoutms);
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

static enum ast_rtp_glue_result sccp_astwrap_get_rtp_info(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);									// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo = SCCP_RTP_INFO_NORTP;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_glue_result res = AST_RTP_GLUE_RESULT_REMOTE;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (get_rtp_info) NO PVT\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if (pbx_channel_state(ast) != AST_STATE_UP) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_rtp_info) Asterisk requested EarlyRTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_rtp_info) Asterisk requested RTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	*rtp = audioRTP->instance;
	if (!*rtp) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}
	// struct ast_sockaddr ast_sockaddr_tmp;
	// ast_rtp_instance_get_remote_address(*rtp, &ast_sockaddr_tmp);
	// sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (get_rtp_info) remote address:'%s:%d'\n", c->currentDeviceId, ast_sockaddr_stringify_host(&ast_sockaddr_tmp), ast_sockaddr_port(&ast_sockaddr_tmp));

	ao2_ref(*rtp, +1);
	if (ast_test_flag(GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_rtp_info) JitterBuffer is Forced. AST_RTP_GET_FAILED\n", c->currentDeviceId);
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_rtp_info) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "%s: (get_rtp_info) Channel %s Returning res: %s\n", c->currentDeviceId, pbx_channel_name(ast), ((res == 2) ? "indirect-rtp" : ((res == 1) ? "direct-rtp" : "forbid")));
	return res;
}

static enum ast_rtp_glue_result sccp_astwrap_get_vrtp_info(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);									// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo = SCCP_RTP_INFO_NORTP;
	struct sccp_rtp *videoRTP = NULL;
	enum ast_rtp_glue_result res = AST_RTP_GLUE_RESULT_REMOTE;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (get_vrtp_info) NO PVT\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if(sccp_channel_getVideoMode(c) == SCCP_VIDEO_MODE_OFF) {
		sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_2 "%s: (get_rtp_info) Video Not supported on this device\n", c->designator);
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	if (pbx_channel_state(ast) != AST_STATE_UP) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_vrtp_info) Asterisk requested EarlyRTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_vrtp_info) Asterisk requested RTP peer for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
	}

#ifdef CS_SCCP_VIDEO
	rtpInfo = sccp_rtp_getVideoPeerInfo(c, &videoRTP);
#endif
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	*rtp = videoRTP->instance;
	if (!*rtp) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}
	// struct ast_sockaddr ast_sockaddr_tmp;
	// ast_rtp_instance_get_remote_address(*rtp, &ast_sockaddr_tmp);
	// sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: (get_vrtp_info) remote address:'%s:%d'\n", c->currentDeviceId, ast_sockaddr_stringify_host(&ast_sockaddr_tmp), ast_sockaddr_port(&ast_sockaddr_tmp));
#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_vrtp_info) JitterBuffer is Forced. AST_RTP_GET_FAILED\n", c->currentDeviceId);
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (get_vrtp_info) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", c->currentDeviceId, pbx_channel_name(ast));
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "%s: (get_vrtp_info) Channel %s Returning res: %s\n", c->currentDeviceId, pbx_channel_name(ast), ((res == 2) ? "indirect-rtp" : ((res == 1) ? "direct-rtp" : "forbid")));
	return res;
}

static int sccp_astwrap_update_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, const struct ast_format_cap *codecs, int nat_active)
{
	//AUTO_RELEASE(sccp_channel_t, c , NULL);									// not following the refcount rules... channel is already retained
	sccp_channel_t *c = NULL;
	int result = 0;

	do {
		char codec_buf[512];

		if (!(c = CS_AST_CHANNEL_PVT(ast))) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (update_rtp_peer) NO PVT\n");
			result = -1;
			break;
		}

		if (!codecs) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (asterisk112_update_rtp_peer) NO Codecs\n", c->currentDeviceId);
			result = -1;
			break;
		}
		ast_getformatname_multiple(codec_buf, sizeof(codec_buf) - 1, (struct ast_format_cap *) codecs);
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (update_rtp_peer) stage: %s, remote codecs capabilty: %s (%lu), nat_active: %d\n", c->currentDeviceId, S_COR(AST_STATE_UP == pbx_channel_state(ast), "RTP", "EarlyRTP"), codec_buf, (long unsigned int) codecs, nat_active);
		if (!c->line) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (update_rtp_peer) NO LINE\n", c->currentDeviceId);
			result = -1;
			break;
		}
		if (c->state == SCCP_CHANNELSTATE_HOLD) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (update_rtp_peer) On Hold -> No Update\n", c->currentDeviceId);
			result = 0;
			break;
		}
		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
		if (!d) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (update_rtp_peer) NO DEVICE\n", c->currentDeviceId);
			result = -1;
			break;
		}
		if (!rtp && !vrtp && !trtp) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "%s: (update_rtp_peer) RTP not ready\n", c->currentDeviceId);
			result = 0;
			break;
		}

		PBX_RTP_TYPE *instance = { 0, };
		struct sockaddr_storage sas = { 0, };
		//struct sockaddr_in sin = { 0, };
		struct ast_sockaddr sin_tmp;
		boolean_t directmedia = FALSE;

		if (rtp) {											// generalize input
			instance = rtp;
		} else if (vrtp) {
			instance = vrtp;
		} else {
			instance = trtp;
		}

		if (d->directrtp && d->nat < SCCP_NAT_ON && !nat_active && !c->conference) {			// asume directrtp
			ast_rtp_instance_get_remote_address(instance, &sin_tmp);
			memcpy(&sas, &sin_tmp, sizeof(struct sockaddr_storage));
			//ast_sockaddr_to_sin(&sin_tmp, &sin);
			if (d->nat == SCCP_NAT_OFF) {								// forced nat off to circumvent autodetection + direcrtp, requires checking both phone_ip and external session ip address against devices permit/deny
				struct ast_sockaddr sin_local;
				struct sockaddr_storage localsas = { 0, };
				ast_rtp_instance_get_local_address(instance, &sin_local);
				memcpy(&localsas, &sin_local, sizeof(struct sockaddr_storage));
				if (sccp_apply_ha(d->ha, &sas) == AST_SENSE_ALLOW && sccp_apply_ha(d->ha, &localsas) == AST_SENSE_ALLOW) {
					directmedia = TRUE;
				}
			} else if (sccp_apply_ha(d->ha, &sas) == AST_SENSE_ALLOW) {					// check remote sin against local device acl (to match netmask)
				directmedia = TRUE;
			}
		}
		if (!directmedia) {										// fallback to indirectrtp
			ast_rtp_instance_get_local_address(instance, &sin_tmp);
			// ast_sockaddr_to_sin(&sin_tmp, &sin);
			// sin.sin_addr.s_addr = sin.sin_addr.s_addr ? sin.sin_addr.s_addr : d->session->ourip.s_addr;
			memcpy(&sas, &sin_tmp, sizeof(struct sockaddr_storage));
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
		} else {
			//sccp_rtp_set_peer(c, &c->rtp.text, &sas);
			//c->rtp.text.directMedia = directmedia;
		}
	} while (0);

	/* Need a return here to break the bridge */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (update_rtp_peer) Result: %d\n", c ? c->currentDeviceId : NULL, result);
	return result;
}

static void sccp_astwrap_getCodec(PBX_CHANNEL_TYPE * ast, struct ast_format_cap *result)
{
	uint8_t i;
	struct ast_format fmt;
	AUTO_RELEASE(sccp_channel_t, channel , get_sccp_channel_from_pbx_channel(ast));

	if (!channel) {
		sccp_log((DEBUGCAT_RTP | DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (getCodec) NO PVT\n");
		return;
	}

	ast_debug(10, "asterisk requests format for channel %s, readFormat: %s(%d)\n", ast->name, codec2str(channel->rtp.audio.transmission.format), channel->rtp.audio.transmission.format);
	for (i = 0; i < ARRAY_LEN(channel->preferences.audio); i++) {
		ast_format_set(&fmt, skinny_codec2pbx_codec(channel->preferences.audio[i]), 0);
		ast_format_cap_add(result, &fmt);
	}

	return;
}

/*
 * \brief get callerid_name from pbx
 * \param sccp_channle Asterisk Channel
 * \param cid name result
 * \return parse result
 */
static int sccp_astwrap_callerid_name(PBX_CHANNEL_TYPE *pbx_chan, char **cid_name)
{
	if (pbx_chan && pbx_chan->caller.id.name.valid && pbx_chan->caller.id.name.str && strlen(pbx_chan->caller.id.name.str) > 0) {
		*cid_name = pbx_strdup(pbx_chan->caller.id.name.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_number(PBX_CHANNEL_TYPE *pbx_chan, char **cid_number)
{
	if (pbx_chan && pbx_chan->caller.id.number.valid && pbx_chan->caller.id.number.str && strlen(pbx_chan->caller.id.number.str) > 0) {
		*cid_number = pbx_strdup(pbx_chan->caller.id.number.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_ton from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_ton(PBX_CHANNEL_TYPE *pbx_chan, int *cid_ton)
{
	if (pbx_chan && pbx_chan->caller.id.number.valid) {
		*cid_ton = pbx_chan->caller.ani.number.plan;
		return *cid_ton;
	}
	return 0;
}

/*
 * \brief get callerid_ani from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_ani(PBX_CHANNEL_TYPE *pbx_chan, char **cid_ani)
{
	if (pbx_chan && pbx_chan->caller.ani.number.valid && pbx_chan->caller.ani.number.str && strlen(pbx_chan->caller.ani.number.str) > 0) {
		*cid_ani = pbx_strdup(pbx_chan->caller.ani.number.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_dnid from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_subaddr(PBX_CHANNEL_TYPE *pbx_chan, char **cid_subaddr)
{
	if (pbx_chan && pbx_chan->caller.id.subaddress.valid && pbx_chan->caller.id.subaddress.str && strlen(pbx_chan->caller.id.subaddress.str) > 0) {
		*cid_subaddr = pbx_strdup(pbx_chan->caller.id.subaddress.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_dnid from pbx (Destination ID)
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_dnid(PBX_CHANNEL_TYPE *pbx_chan, char **cid_dnid)
{
	if (pbx_chan && pbx_chan->dialed.number.str && strlen(pbx_chan->dialed.number.str) > 0) {
		*cid_dnid = pbx_strdup(pbx_chan->dialed.number.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_rdnis from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_astwrap_callerid_rdnis(PBX_CHANNEL_TYPE *pbx_chan, char **cid_rdnis)
{
	if (pbx_chan && pbx_chan->redirecting.from.number.valid && pbx_chan->redirecting.from.number.str && strlen(pbx_chan->redirecting.from.number.str) > 0) {
		*cid_rdnis = pbx_strdup(pbx_chan->redirecting.from.number.str);
		return 1;
	}
	return 0;
}

/*
 * \brief get callerid_presence from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static sccp_callerid_presentation_t sccp_astwrap_callerid_presentation(PBX_CHANNEL_TYPE *pbx_chan)
{
	if (pbx_chan && (ast_party_id_presentation(&pbx_chan->caller.id) & AST_PRES_RESTRICTION) == AST_PRES_ALLOWED) {
		return CALLERID_PRESENTATION_ALLOWED;
	}
	return CALLERID_PRESENTATION_FORBIDDEN;
}

static boolean_t sccp_astwrap_createRtpInstance(constDevicePtr d, constChannelPtr c, sccp_rtp_t *rtp)
{
	uint32_t tos = 0, cos = 0;
	
	if (!c || !d) {
		return FALSE;
	}
	struct sockaddr_storage ourip = { 0 };
	sccp_session_getOurIP(d->session, &ourip, 0);
	struct ast_sockaddr sock = { {
	    0,
	} };
	storage2ast_sockaddr(&ourip, &sock);

	if ((rtp->instance = ast_rtp_instance_new("asterisk", sched, &sock, NULL))) {
		struct ast_sockaddr instance_addr = { {0,} };
		ast_rtp_instance_get_local_address(rtp->instance, &instance_addr);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: rtp server instance created at %s\n", c->designator, ast_sockaddr_stringify(&instance_addr));
	} else {
		return FALSE;
	}

	/* rest below should be moved out of here (refactoring required) */
	PBX_RTP_TYPE *instance = rtp->instance;
	//char *rtp_map_filter = NULL;
	//skinny_payload_type_t codec_type;
	int fd_offset = 0;
	switch(rtp->type) {
		case SCCP_RTP_AUDIO:
			tos = d->audio_tos;
			cos = d->audio_cos;
			//rtp_map_filter = "audio";
			//codec_type = SKINNY_CODEC_TYPE_AUDIO;
			break;
			
#if CS_SCCP_VIDEO
		case SCCP_RTP_VIDEO:
			tos = d->video_tos;
			cos = d->video_cos;
			//rtp_map_filter = "video";
			//codec_type = SKINNY_CODEC_TYPE_VIDEO;
			fd_offset = 2;
			break;
#endif			
		default:
			pbx_log(LOG_ERROR, "%s: (wrapper_create_rtp) unknown/unhandled rtp type, returning instance for now\n", c->designator);
			return TRUE;
	}

	if (c->owner) {
		ast_channel_set_fd(c->owner, fd_offset, ast_rtp_instance_fd(instance, 0));		// RTP
		ast_channel_set_fd(c->owner, fd_offset + 1, ast_rtp_instance_fd(instance, 1));		// RTCP
	}
	ast_rtp_instance_set_prop(instance, AST_RTP_PROPERTY_RTCP, 1);
	if (rtp->type == SCCP_RTP_AUDIO) {
		ast_rtp_instance_set_prop(instance, AST_RTP_PROPERTY_DTMF, 1);
		if (c->dtmfmode == SCCP_DTMFMODE_SKINNY) {
			ast_rtp_instance_set_prop(instance, AST_RTP_PROPERTY_DTMF_COMPENSATE, 1);
			ast_rtp_instance_dtmf_mode_set(instance, AST_RTP_DTMF_MODE_INBAND);
		}
	}
	ast_rtp_instance_set_qos(instance, tos, cos, "SCCP RTP");

	// Add CISCO DTMF SKINNY payload type
	if (rtp->type == SCCP_RTP_AUDIO && SCCP_DTMFMODE_SKINNY == d->dtmfmode) {
		ast_rtp_codecs_payloads_set_m_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 96);
		ast_rtp_codecs_payloads_set_rtpmap_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 96, "audio", "telephone-event", 0);
		ast_rtp_codecs_payloads_set_m_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 101);
		ast_rtp_codecs_payloads_set_rtpmap_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 101, "audio", "telephone-event", 0);
		ast_rtp_codecs_payloads_set_m_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 105);
		ast_rtp_codecs_payloads_set_rtpmap_type(ast_rtp_instance_get_codecs(c->rtp.audio.instance), c->rtp.audio.instance, 105, "audio", "cisco-telephone-event", 0);
	}
	if (c->owner) {
		// this prevent a warning about unknown codec, when rtp traffic starts */
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	return TRUE;
}

static uint sccp_wrapper_get_dtmf_payload_code(constChannelPtr c)
{
	int rtp_code = 0;
	if (SCCP_DTMFMODE_SKINNY != c->dtmfmode) {
		rtp_code = ast_rtp_codecs_payload_code(ast_rtp_instance_get_codecs(c->rtp.audio.instance), 0, NULL, AST_RTP_DTMF);
	}
	sccp_log(DEBUGCAT_RTP)(VERBOSE_PREFIX_3 "%s: Using dtmf rtp_code : %d\n", c->designator, rtp_code);
	return rtp_code != -1 ? rtp_code : 0;
}

static boolean_t sccp_astwrap_destroyRTP(PBX_RTP_TYPE * rtp)
{
	int res;

	res = ast_rtp_instance_destroy(rtp);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_astwrap_checkHangup(const sccp_channel_t * channel)
{
	int res;

	res = ast_check_hangup(channel->owner);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_astwrap_rtpGetPeer(PBX_RTP_TYPE * rtp, struct sockaddr_storage *address)
{
	struct ast_sockaddr addr;

	ast_rtp_instance_get_remote_address(rtp, &addr);
	memcpy(address, &addr.ss, sizeof(struct sockaddr_storage));
	return TRUE;
}

static boolean_t sccp_astwrap_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_storage *address)
{
	struct ast_sockaddr addr;

	ast_rtp_instance_get_local_address(rtp, &addr);
	memcpy(address, &addr.ss, sizeof(struct sockaddr_storage));
	return TRUE;
}

static boolean_t sccp_astwrap_getChannelByName(const char *name, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *ast = ast_channel_get_by_name(name);

	if (!ast) {
		return FALSE;
	}
	pbx_channel_lock(ast);
	*pbx_channel = pbx_channel_ref(ast);
	pbx_channel_unlock(ast);
	return TRUE;
}

//static int sccp_astwrap_rtp_set_peer(const struct sccp_rtp *rtp, const struct sockaddr_storage *new_peer, int nat_active)
static int sccp_astwrap_setPhoneRTPAddress(const struct sccp_rtp *rtp, const struct sockaddr_storage *new_peer, int nat_active)
{
	struct ast_sockaddr ast_sockaddr_dest;
	int res;

	memcpy(&ast_sockaddr_dest.ss, new_peer, sizeof(struct sockaddr_storage));
	ast_sockaddr_dest.len = (ast_sockaddr_dest.ss.ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	res = ast_rtp_instance_set_remote_address(rtp->instance, &ast_sockaddr_dest);

	sccp_log((DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (setPhoneRTPAddress) Update PBX to send RTP/UDP media to '%s:%d' (new remote) (NAT: %s)\n", ast_sockaddr_stringify_host(&ast_sockaddr_dest), ast_sockaddr_port(&ast_sockaddr_dest), S_COR(nat_active, "yes", "no"));
	if (nat_active) {
		ast_rtp_instance_set_prop(rtp->instance, AST_RTP_PROPERTY_NAT, 1);
	} else {
		ast_rtp_instance_set_prop(rtp->instance, AST_RTP_PROPERTY_NAT, 0);
	}
	return res;
}

static boolean_t sccp_astwrap_setWriteFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{
	//! \todo possibly needs to be synced to ast108
	if (!channel) {
		return FALSE;
	}
	struct ast_format fmt;

	ast_format_set(&fmt, skinny_codec2pbx_codec(codec), 0);
	ast_format_copy(&channel->owner->writeformat, &fmt);
	ast_format_copy(&channel->owner->rawwriteformat, &fmt);
	if (0 != channel->rtp.audio.instance) {
		ast_rtp_instance_set_write_format(channel->rtp.audio.instance, &fmt);
	}
	return TRUE;
}

static boolean_t sccp_astwrap_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{
	//! \todo possibly needs to be synced to ast108
	if (!channel) {
		return FALSE;
	}
	struct ast_format fmt;

	ast_format_set(&fmt, skinny_codec2pbx_codec(codec), 0);
	ast_format_copy(&channel->owner->readformat, &fmt);
	ast_format_copy(&channel->owner->rawreadformat, &fmt);
	if (0 != channel->rtp.audio.instance) {
		ast_rtp_instance_set_read_format(channel->rtp.audio.instance, &fmt);
	}
	return TRUE;
}

static void sccp_astwrap_setCalleridName(PBX_CHANNEL_TYPE *pbxChannel, const char *name)
{
	if (pbxChannel && name) {
		pbxChannel->caller.id.name.valid = 1;
		ast_party_name_free(&pbxChannel->caller.id.name);
		pbxChannel->caller.id.name.str = pbx_strdup(name);
	}
}

static void sccp_astwrap_setCalleridNumber(PBX_CHANNEL_TYPE *pbxChannel, const char *number)
{
	if (pbxChannel && number) {
		pbxChannel->caller.id.number.valid = 1;
		ast_party_number_free(&pbxChannel->caller.id.number);
		pbxChannel->caller.id.number.str = pbx_strdup(number);
	}
}

static void sccp_astwrap_setCalleridAni(PBX_CHANNEL_TYPE *pbxChannel, const char *number)
{
	if (pbxChannel && number) {
		pbxChannel->caller.ani.number.valid = 1;
		ast_party_number_free(&pbxChannel->caller.ani.number);
		pbxChannel->caller.ani.number.str = pbx_strdup(number);
	}
}

static void sccp_astwrap_setRedirectingParty(PBX_CHANNEL_TYPE *pbxChannel, const char *number, const char *name)
{
	if (pbxChannel && number) {
		ast_party_number_free(&pbxChannel->redirecting.from.number);
		pbxChannel->redirecting.from.number.str = pbx_strdup(number);
		pbxChannel->redirecting.from.number.valid = 1;
	}

	if (pbxChannel && name) {
		ast_party_name_free(&pbxChannel->redirecting.from.name);
		pbxChannel->redirecting.from.name.str = pbx_strdup(name);
		pbxChannel->redirecting.from.name.valid = 1;
	}
}

static void sccp_astwrap_setRedirectedParty(PBX_CHANNEL_TYPE *pbxChannel, const char *number, const char *name)
{
	if (pbxChannel && number) {
		pbxChannel->redirecting.to.number.valid = 1;
		ast_party_number_free(&pbxChannel->redirecting.to.number);
		pbxChannel->redirecting.to.number.str = pbx_strdup(number);
	}

	if (pbxChannel && name) {
		pbxChannel->redirecting.to.name.valid = 1;
		ast_party_name_free(&pbxChannel->redirecting.to.name);
		pbxChannel->redirecting.to.name.str = pbx_strdup(name);
	}
}

static void sccp_astwrap_updateConnectedLine(const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason)
{
	if (!channel || !channel->owner) {
		return;
	}
	__sccp_astwrap_updateConnectedLine(channel->owner, number, name, reason);
}

static int sccp_astwrap_sched_add(int when, sccp_sched_cb callback, const void *data)
{
	if (sched) {
		return ast_sched_add(sched, when, callback, data);
	}
	return -1;
}

static int sccp_astwrap_sched_del(int id)
{
	if (sched) {
		return AST_SCHED_DEL(sched, id);
	}
	return -1;
}

static int sccp_astwrap_sched_add_ref(int *id, int when, sccp_sched_cb callback, sccp_channel_t * channel)
{
	if (sched && channel) {
		sccp_channel_t *c = sccp_channel_retain(channel);

		if (c) {
			if ((*id  = ast_sched_add(sched, when, callback, c)) < 0) {
				sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: sched add id:%d, when:%d, failed\n", c->designator, *id, when);
				sccp_channel_release(&channel);			/* explicit release during failure */
			}
			return *id;
		}
	}
	return -2;
}

static int sccp_astwrap_sched_del_ref(int *id, sccp_channel_t * channel)
{
	if (sched) {
		AST_SCHED_DEL_UNREF(sched, *id, sccp_channel_release(&channel));
		return *id;
	}
	return -2;
}

static int sccp_astwrap_sched_replace_ref(int *id, int when, ast_sched_cb callback, sccp_channel_t * channel)
{
	if (sched) {
		// sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: (sched_replace_ref) replacing id: %d\n", channel->designator, *id);
		AST_SCHED_REPLACE_UNREF(*id, sched, when, callback, channel, sccp_channel_release((sccp_channel_t **)&_data), sccp_channel_release(&channel), sccp_channel_retain(channel));	/* explicit retain/release */
		// sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: (sched_replace_ref) returning id: %d\n", channel->designator, *id);
		return *id;
	}
	return -2;
}

static long sccp_astwrap_sched_when(int id)
{
	if (sched) {
		return ast_sched_when(sched, id);
	}
	return FALSE;
}

static int sccp_astwrap_sched_wait(int id)
{
	if (sched) {
		return ast_sched_wait(sched);
	}
	return FALSE;
}

static int sccp_astwrap_setCallState(const sccp_channel_t * channel, enum ast_channel_state state)
{
        if (channel && channel->owner) {
		pbx_setstate(channel->owner, state);
        }
	//! \todo implement correct return value (take into account negative deadlock prevention)
	return 0;
}

static boolean_t sccp_astwrap_getRemoteChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{

	PBX_CHANNEL_TYPE *remotePeer;
	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

	for (; (remotePeer = ast_channel_iterator_next(iterator)); remotePeer = pbx_channel_unref(remotePeer)) {
		if (pbx_find_channel_by_linkid(remotePeer, (void *) channel->owner->linkedid)) {
			break;
		}
	}
	ast_channel_iterator_destroy(iterator);

	if (remotePeer) {
		*pbx_channel = remotePeer;
		remotePeer = pbx_channel_unref(remotePeer);
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
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send text to (%s)\n", pbx_channel_name(ast));
		return -1;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (!d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send text to (%s)\n", pbx_channel_name(ast));
		return -1;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, pbx_channel_name(ast));

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
	if (c) {
		if (c->dtmfmode == SCCP_DTMFMODE_RFC2833) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Channel(%s) DTMF Mode is RFC2833. Skipping...\n", c->designator, pbx_channel_name(ast));
			return -1;
		}

		AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
		if (!d) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send digit to (%s)\n", pbx_channel_name(ast));
			return -1;
		}

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", d->id, digit, pbx_channel_name(ast), sccp_dtmfmode2str(d->dtmfmode));
		if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, pbx_channel_name(ast));
			return -1;
		}
		sccp_dev_keypadbutton(d, digit, sccp_device_find_index_for_line(d, c->line->name), c->callid);
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send digit to (%s)\n", pbx_channel_name(ast));
		return -1;
	}

	return -1;
}

static PBX_CHANNEL_TYPE *sccp_astwrap_findChannelWithCallback(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data, boolean_t lock)
{
	PBX_CHANNEL_TYPE *remotePeer;

	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	if (!lock) {
		((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;
	}

	for (; (remotePeer = ast_channel_iterator_next(iterator)); remotePeer = pbx_channel_unref(remotePeer)) {

		if (found_cb(remotePeer, data)) {
			ast_channel_lock(remotePeer);
			break;
		}

	}
	ast_channel_iterator_destroy(iterator);
	return remotePeer;
}

/*! \brief Set an option on a asterisk channel */
#if 0
static int sccp_astwrap_setOption(PBX_CHANNEL_TYPE * ast, int option, void *data, int datalen)
{
	int res = -1;
	AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(ast));

	if (c) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: channel: %s(%s) setOption: %d\n", c->currentDeviceId, sccp_channel_toString(c), ast->name, option);
		//! if AST_OPTION_FORMAT_READ / AST_OPTION_FORMAT_WRITE are available we might be indication that we can do transcoding (channel.c:set_format). Correct ? */
		switch (option) {
			case AST_OPTION_FORMAT_READ:
				if (c->rtp.audio.instance) {
					res = ast_rtp_instance_set_read_format(c->rtp.audio.instance, (struct ast_format *) data);
				}
				//sccp_astwrap_setReadFormat(c, (struct ast_format *) data);
				break;
			case AST_OPTION_FORMAT_WRITE:
				if (c->rtp.audio.instance) {
					//res = ast_rtp_instance_set_write_format(c->rtp.audio.instance, (struct ast_format *) data);
				}
				//sccp_astwrap_setWriteFormat(c, (struct ast_format *) data);
				break;

			case AST_OPTION_MAKE_COMPATIBLE:
				if (c->rtp.audio.instance) {
					//res = ast_rtp_instance_make_compatible(ast, c->rtp.audio.instance, (PBX_CHANNEL_TYPE *) data);
				}
				break;
			case AST_OPTION_DIGIT_DETECT:
			case AST_OPTION_SECURE_SIGNALING:
			case AST_OPTION_SECURE_MEDIA:
				res = -1;
				break;
			default:
				break;
		}
	}
	return res;
}
#endif

static void sccp_astwrap_set_pbxchannel_linkedid(PBX_CHANNEL_TYPE * pbx_channel, const char *new_linkedid)
{
	if (pbx_channel) {
		if (!strcmp(pbx_channel->linkedid, new_linkedid)) {
			return;
		}
#if HAVE_PBX_CEL_H
		ast_cel_check_retire_linkedid(pbx_channel);
#endif
		ast_string_field_set(pbx_channel, linkedid, new_linkedid);
#if CS_AST_CEL_LINKEDID_REF
		ast_cel_linkedid_ref(new_linkedid);
#endif
	}
};

#define DECLARE_PBX_CHANNEL_STRGET(_field) 									\
static const char *sccp_astwrap_get_channel_##_field(const sccp_channel_t * channel)	 		\
{														\
	static const char *empty_channel_##_field = "--no-channel-" #_field "--";				\
	if (channel && channel->owner) {											\
		return channel->owner->_field;									\
	}													\
	return empty_channel_##_field;										\
};

#define DECLARE_PBX_CHANNEL_STRSET(_field)									\
static void sccp_astwrap_set_channel_##_field(const sccp_channel_t * channel, const char * (_field))	\
{ 														\
        if (channel && channel->owner) {											\
        	sccp_copy_string(channel->owner->_field, _field, sizeof(channel->owner->_field));		\
        }													\
};

DECLARE_PBX_CHANNEL_STRGET(name)
    DECLARE_PBX_CHANNEL_STRGET(uniqueid)
    DECLARE_PBX_CHANNEL_STRGET(appl)
    DECLARE_PBX_CHANNEL_STRGET(linkedid)
    DECLARE_PBX_CHANNEL_STRGET(exten)
    DECLARE_PBX_CHANNEL_STRSET(exten)
    DECLARE_PBX_CHANNEL_STRGET(context)
    DECLARE_PBX_CHANNEL_STRSET(context)
    DECLARE_PBX_CHANNEL_STRGET(macroexten)
    DECLARE_PBX_CHANNEL_STRSET(macroexten)
    DECLARE_PBX_CHANNEL_STRGET(macrocontext)
    DECLARE_PBX_CHANNEL_STRSET(macrocontext)
    DECLARE_PBX_CHANNEL_STRGET(call_forward)

static void sccp_astwrap_set_channel_linkedid(const sccp_channel_t * channel, const char *new_linkedid)
{
	if (channel && channel->owner) {
		sccp_astwrap_set_pbxchannel_linkedid(channel->owner, new_linkedid);
	}
};

static void sccp_astwrap_set_channel_name(const sccp_channel_t * channel, const char *new_name)
{
	if (channel && channel->owner) {
		pbx_string_field_build(channel->owner, name, "%s", new_name);
	}
};

static void sccp_astwrap_set_channel_call_forward(const sccp_channel_t * channel, const char *new_call_forward)
{
	if (channel && channel->owner) {
		pbx_string_field_set(channel->owner, call_forward, new_call_forward);
	}
}

static enum ast_channel_state sccp_astwrap_get_channel_state(const sccp_channel_t * channel)
{
	if (channel && channel->owner) {
		return channel->owner->_state;
	}
	return 0;
}

static const struct ast_pbx *sccp_astwrap_get_channel_pbx(const sccp_channel_t * channel)
{
	if (channel && channel->owner) {
		return channel->owner->pbx;
	}
	return NULL;
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
			fr.data.ptr = (char *) data;
			fr.src = "SCCP Send URL";
			fr.datalen = datalen;

			sccp_push_result_t pushResult = d->pushURL(d, data, 1, SKINNY_TONE_ZIP);

			if (SCCP_PUSH_RESULT_SUCCESS == pushResult) {
				fr.subclass.integer = AST_HTML_LDCOMPLETE;
			} else {
				fr.subclass.integer = AST_HTML_NOSUPPORT;
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
int sccp_astwrap_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control)
{
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass.integer = control };
	return ast_queue_frame((PBX_CHANNEL_TYPE *) pbx_channel, &f);
}

/*!
 * \brief Queue a control frame with payload
 * \param pbx_channel PBX Channel
 * \param control as Asterisk Control Frame Type
 * \param data Payload
 * \param datalen Payload Length
 */
int sccp_astwrap_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen)
{
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass.integer = control,.data.ptr = (void *) data,.datalen = datalen };
	return ast_queue_frame((PBX_CHANNEL_TYPE *) pbx_channel, &f);
}

/*!
 * \brief Get Hint Extension State and return the matching Busy Lamp Field State 
 */
static skinny_busylampfield_state_t sccp_astwrap_getExtensionState(const char *extension, const char *context)
{
	skinny_busylampfield_state_t result = SKINNY_BLF_STATUS_UNKNOWN;

	if (sccp_strlen_zero(extension) || sccp_strlen_zero(context)) {
		pbx_log(LOG_ERROR, "SCCP: iPbx.getExtensionState: Either extension:'%s' or context:;%s' provided is empty\n", extension, context);
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

static boolean_t sccp_astwrap_channelIsBridged(sccp_channel_t * channel)
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

static PBX_CHANNEL_TYPE *sccp_astwrap_getBridgeChannel(PBX_CHANNEL_TYPE * pbx_channel)
{
	PBX_CHANNEL_TYPE *bridgePeer = NULL;
	if (pbx_channel && (bridgePeer = ast_bridged_channel(pbx_channel))) {
		return pbx_channel_ref(bridgePeer);
	};
	return NULL;
}

static PBX_CHANNEL_TYPE *sccp_astwrap_getUnderlyingChannel(PBX_CHANNEL_TYPE * pbx_channel)
{
	PBX_CHANNEL_TYPE *bridgePeer = NULL;
	if (pbx_channel && (bridgePeer = pbx_channel->tech->bridged_channel(pbx_channel, NULL))) {
		return pbx_channel_ref(bridgePeer);
	}
	return NULL;
}

static boolean_t sccp_astwrap_attended_transfer(sccp_channel_t * destination_channel, sccp_channel_t * source_channel)
{
	// possibly move transfer related callinfo updates here
	if (!destination_channel || !source_channel || !destination_channel->owner || !source_channel->owner) {
		return FALSE;
	}
	PBX_CHANNEL_TYPE *pbx_destination_local_channel = destination_channel->owner;
	PBX_CHANNEL_TYPE *pbx_source_remote_channel = sccp_astwrap_getBridgeChannel(source_channel->owner);

	if (!pbx_destination_local_channel || !pbx_source_remote_channel) {
		return FALSE;
	}
	return pbx_channel_masquerade(pbx_destination_local_channel, pbx_source_remote_channel);
}

/*!
 * \brief using RTP Glue Engine
 */
#if defined(__cplusplus) || defined(c_plusplus)
static struct ast_rtp_glue sccp_rtp = {
	/* *INDENT-OFF* */
	type:	SCCP_TECHTYPE_STR,
	mod:	NULL,
	get_rtp_info:sccp_astwrap_get_rtp_info,
	get_vrtp_info:sccp_astwrap_get_vrtp_info,
	get_trtp_info:NULL,
	update_peer:sccp_astwrap_update_rtp_peer,
	get_codec:sccp_astwrap_getCodec,
	/* *INDENT-ON* */
};
#else
static struct ast_rtp_glue sccp_rtp = {
	/* *INDENT-OFF* */
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_astwrap_get_rtp_info,
	.get_vrtp_info = sccp_astwrap_get_vrtp_info,
	.update_peer = sccp_astwrap_update_rtp_peer,
	.get_codec = sccp_astwrap_getCodec,
	/* *INDENT-ON* */
};
#endif

#ifdef HAVE_PBX_MESSAGE_H
#include <asterisk/message.h>
static int sccp_astwrap_message_send(const struct ast_msg *msg, const char *to, const char *from)
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
	sccp_linedevice_t * ld = NULL;
	sccp_push_result_t pushResult;

	SCCP_LIST_LOCK(&line->devices);
	SCCP_LIST_TRAVERSE(&line->devices, ld, list) {
		pushResult = ld->device->pushTextMessage(ld->device, messageText, from, 1, SKINNY_TONE_ZIP);
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
	msg_send:sccp_astwrap_message_send,
	/* *INDENT-ON* */
};
#else
static const struct ast_msg_tech sccp_msg_tech = {
	/* *INDENT-OFF* */
	.name = "sccp",
	.msg_send = sccp_astwrap_message_send,
	/* *INDENT-ON* */
};
#endif

#endif

static int sccp_wrapper_register_application(const char *app_name, int (*execute)(struct ast_channel *, const char *))
{
	return ast_register_application2(app_name, execute, NULL, NULL, ast_module_info->self);
}

static int sccp_wrapper_unregister_application(const char *app_name)
{
	return ast_unregister_application(app_name);
}

static int sccp_wrapper_register_function(struct pbx_custom_function *custom_function)
{
	return __ast_custom_function_register(custom_function, ast_module_info->self);
}

static int sccp_wrapper_unregister_function(struct pbx_custom_function *custom_function)
{
	return ast_custom_function_unregister(custom_function);
}

static boolean_t sccp_astwrap_setLanguage(PBX_CHANNEL_TYPE * pbxChannel, const char *newLanguage)
{

	ast_string_field_set(pbxChannel, language, newLanguage);
	return TRUE;
}

#if defined(__cplusplus) || defined(c_plusplus)
const PbxInterface iPbx = {
	/* *INDENT-OFF* */
	alloc_pbxChannel:		sccp_astwrap_allocPBXChannel,
	set_callstate:			sccp_astwrap_setCallState,
	checkhangup:			sccp_astwrap_checkHangup,
	hangup:				NULL,
	extension_status:		sccp_astwrap_extensionStatus,

	setPBXChannelLinkedId:		sccp_astwrap_set_pbxchannel_linkedid,
	/** get channel by name */
	getChannelByName:		sccp_astwrap_getChannelByName,
	getRemoteChannel:		sccp_astwrap_getRemoteChannel,
	getChannelByCallback:		NULL,

	getChannelLinkedId:		sccp_astwrap_get_channel_linkedid,
	setChannelLinkedId:		sccp_astwrap_set_channel_linkedid,
	getChannelName:			sccp_astwrap_get_channel_name,
	getChannelUniqueID:		sccp_astwrap_get_channel_uniqueid,
	getChannelExten:		sccp_astwrap_get_channel_exten,
	setChannelExten:		sccp_astwrap_set_channel_exten,
	getChannelContext:		sccp_astwrap_get_channel_context,
	setChannelContext:		sccp_astwrap_set_channel_context,
	getChannelMacroExten:		sccp_astwrap_get_channel_macroexten,
	setChannelMacroExten:		sccp_astwrap_set_channel_macroexten,
	getChannelMacroContext:		sccp_astwrap_get_channel_macrocontext,
	setChannelMacroContext:		sccp_astwrap_set_channel_macrocontext,
	getChannelCallForward:		sccp_astwrap_get_channel_call_forward,
	setChannelCallForward:		sccp_astwrap_set_channel_call_forward,

	getChannelAppl:			sccp_astwrap_get_channel_appl,
	getChannelState:		sccp_astwrap_get_channel_state,
	getChannelPbx:			sccp_astwrap_get_channel_pbx,

	set_nativeAudioFormats:		sccp_astwrap_setNativeAudioFormats,
	set_nativeVideoFormats:		sccp_astwrap_setNativeVideoFormats,

	getPeerCodecCapabilities:	NULL,
	send_digit:			sccp_wrapper_sendDigit,
	send_digits:			sccp_wrapper_sendDigits,

	sched_add:			sccp_astwrap_sched_add,
	sched_del:			sccp_astwrap_sched_del,
	sched_add_ref:			sccp_astwrap_sched_add_ref,
	sched_del_ref:			sccp_astwrap_sched_del_ref,
	sched_replace_ref:		sccp_astwrap_sched_replace_ref,
	sched_when:			sccp_astwrap_sched_when,
	sched_wait:			sccp_astwrap_sched_wait,

	/* rtp */
	rtp_getPeer:			sccp_astwrap_rtpGetPeer,
	rtp_getUs:			sccp_astwrap_rtpGetUs,
	rtp_setPhoneAddress:		sccp_astwrap_setPhoneRTPAddress,
	rtp_setWriteFormat:		sccp_astwrap_setWriteFormat,
	rtp_setReadFormat:		sccp_astwrap_setReadFormat,
	rtp_destroy:			sccp_astwrap_destroyRTP,
	rtp_stop:			ast_rtp_instance_stop,
	rtp_codec:			NULL,
	rtp_create_instance:		sccp_astwrap_createRtpInstance,
	rtp_get_payloadType:		sccp_astwrap_get_payloadType,
	rtp_get_sampleRate:		sccp_astwrap_get_sampleRate,
	rtp_bridgePeers:		NULL,

	/* callerid */
	get_callerid_name:		sccp_astwrap_callerid_name,
	get_callerid_number:		sccp_astwrap_callerid_number,
	get_callerid_ton:		sccp_astwrap_callerid_ton,
	get_callerid_ani:		sccp_astwrap_callerid_ani,
	get_callerid_subaddr:		sccp_astwrap_callerid_subaddr,
	get_callerid_dnid:		sccp_astwrap_callerid_dnid,
	get_callerid_rdnis:		sccp_astwrap_callerid_rdnis,
	get_callerid_presentation:	sccp_astwrap_callerid_presentation,

	set_callerid_name:		sccp_astwrap_setCalleridName,
	set_callerid_number:		sccp_astwrap_setCalleridNumber,
	set_callerid_ani:		sccp_astwrap_setCalleridAni,
	set_callerid_dnid:		NULL,
	set_callerid_redirectingParty:	sccp_astwrap_setRedirectingParty,
	set_callerid_redirectedParty:	sccp_astwrap_setRedirectedParty,
	set_callerid_presentation:	sccp_astwrap_setCalleridPresentation,
	set_connected_line:		sccp_astwrap_updateConnectedLine,
	sendRedirectedUpdate:		sccp_astwrap_sendRedirectedUpdate,

	/* feature section */
	feature_park:			sccp_astwrap_park,
	feature_stopMusicOnHold:	NULL,
	feature_addToDatabase:		sccp_astwrap_addToDatabase,
	feature_getFromDatabase:	sccp_astwrap_getFromDatabase,
	feature_removeFromDatabase:	sccp_astwrap_removeFromDatabase,
	feature_removeTreeFromDatabase:	sccp_astwrap_removeTreeFromDatabase,
	feature_monitor:		sccp_astgenwrap_featureMonitor,
	getFeatureExtension:		sccp_astwrap_getFeatureExtension,
	getPickupExtension:		sccp_astwrap_getPickupExtension,

	eventSubscribe:			NULL,
	findChannelByCallback:		sccp_astwrap_findChannelWithCallback,

	moh_start:			sccp_astwrap_moh_start,
	moh_stop:			sccp_astwrap_moh_stop,
	queue_control:			sccp_astwrap_queue_control,
	queue_control_data:		sccp_astwrap_queue_control_data,

	allocTempPBXChannel:		sccp_astwrap_allocTempPBXChannel,
	masqueradeHelper:		sccp_astwrap_masqueradeHelper,
	requestAnnouncementChannel:	sccp_astwrap_requestAnnouncementChannel,
	set_language:			sccp_astwrap_setLanguage,

	getExtensionState:		sccp_astwrap_getExtensionState,
	findPickupChannelByExtenLocked:	sccp_astwrap_findPickupChannelByExtenLocked,
	findPickupChannelByGroupLocked:	sccp_astwrap_findPickupChannelByGroupLocked,

	set_owner:			sccp_astwrap_setOwner,
	removeTimingFD:			sccp_astwrap_removeTimingFD,
	dumpchan:			NULL,
	channel_is_bridged:		sccp_astwrap_channelIsBridged,
	get_bridged_channel:		sccp_astwrap_getBridgeChannel,
	get_underlying_channel:		sccp_astwrap_getUnderlyingChannel,
	attended_transfer:		sccp_astwrap_attended_transfer,

	set_callgroup:			sccp_astgenwrap_set_callgroup,
	set_pickupgroup:		sccp_astgenwrap_set_pickupgroup,
	set_named_callgroups:		NULL,
	set_named_pickupgroups:		NULL,

	register_application:		sccp_wrapper_register_application,
	unregister_application:		sccp_wrapper_unregister_application,
	register_function:		sccp_wrapper_register_function,
	unregister_function:		sccp_wrapper_unregister_function,

	get_dtmf_payload_code:		sccp_wrapper_get_dtmf_payload_code,
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
	.alloc_pbxChannel 		= sccp_astwrap_allocPBXChannel,
	.extension_status 		= sccp_astwrap_extensionStatus,
	.setPBXChannelLinkedId		= sccp_astwrap_set_pbxchannel_linkedid,
	.getChannelByName 		= sccp_astwrap_getChannelByName,

	.getChannelLinkedId		= sccp_astwrap_get_channel_linkedid,
	.setChannelLinkedId		= sccp_astwrap_set_channel_linkedid,
	.getChannelName			= sccp_astwrap_get_channel_name,
	.setChannelName			= sccp_astwrap_set_channel_name,
	.getChannelUniqueID		= sccp_astwrap_get_channel_uniqueid,
	.getChannelExten		= sccp_astwrap_get_channel_exten,
	.setChannelExten		= sccp_astwrap_set_channel_exten,
	.getChannelContext		= sccp_astwrap_get_channel_context,
	.setChannelContext		= sccp_astwrap_set_channel_context,
	.getChannelMacroExten		= sccp_astwrap_get_channel_macroexten,
	.setChannelMacroExten		= sccp_astwrap_set_channel_macroexten,
	.getChannelMacroContext		= sccp_astwrap_get_channel_macrocontext,
	.setChannelMacroContext		= sccp_astwrap_set_channel_macrocontext,
	.getChannelCallForward		= sccp_astwrap_get_channel_call_forward,
	.setChannelCallForward		= sccp_astwrap_set_channel_call_forward,

	.getChannelAppl			= sccp_astwrap_get_channel_appl,
	.getChannelState		= sccp_astwrap_get_channel_state,
	.getChannelPbx			= sccp_astwrap_get_channel_pbx,
	.getRemoteChannel		= sccp_astwrap_getRemoteChannel,
	.checkhangup			= sccp_astwrap_checkHangup,
	/* digits */
	.send_digits 			= sccp_wrapper_sendDigits,
	.send_digit 			= sccp_wrapper_sendDigit,

	/* schedulers */
	.sched_add			= sccp_astwrap_sched_add,
	.sched_del			= sccp_astwrap_sched_del,
	.sched_add_ref			= sccp_astwrap_sched_add_ref,
	.sched_del_ref			= sccp_astwrap_sched_del_ref,
	.sched_replace_ref		= sccp_astwrap_sched_replace_ref,
	.sched_when 			= sccp_astwrap_sched_when,
	.sched_wait 			= sccp_astwrap_sched_wait,
	/* callstate / indicate */
	.set_callstate 			= sccp_astwrap_setCallState,

	/* codecs */
	.set_nativeAudioFormats 	= sccp_astwrap_setNativeAudioFormats,
	.set_nativeVideoFormats 	= sccp_astwrap_setNativeVideoFormats,

	/* rtp */
	.rtp_getPeer			= sccp_astwrap_rtpGetPeer,
	.rtp_getUs 			= sccp_astwrap_rtpGetUs,
	.rtp_stop 			= ast_rtp_instance_stop,
	.rtp_create_instance		= sccp_astwrap_createRtpInstance,
	.rtp_get_payloadType 		= sccp_astwrap_get_payloadType,
	.rtp_get_sampleRate 		= sccp_astwrap_get_sampleRate,
	.rtp_destroy 			= sccp_astwrap_destroyRTP,
	.rtp_setWriteFormat 		= sccp_astwrap_setWriteFormat,
	.rtp_setReadFormat 		= sccp_astwrap_setReadFormat,
	.rtp_setPhoneAddress		= sccp_astwrap_setPhoneRTPAddress,

	/* callerid */
	.get_callerid_name 		= sccp_astwrap_callerid_name,
	.get_callerid_number 		= sccp_astwrap_callerid_number,
	.get_callerid_ton 		= sccp_astwrap_callerid_ton,
	.get_callerid_ani 		= sccp_astwrap_callerid_ani,
	.get_callerid_subaddr 		= sccp_astwrap_callerid_subaddr,
	.get_callerid_dnid 		= sccp_astwrap_callerid_dnid,
	.get_callerid_rdnis 		= sccp_astwrap_callerid_rdnis,
	.get_callerid_presentation 	= sccp_astwrap_callerid_presentation,
	.set_callerid_name 		= sccp_astwrap_setCalleridName,
	.set_callerid_number 		= sccp_astwrap_setCalleridNumber,
	.set_callerid_ani 		= sccp_astwrap_setCalleridAni,
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_astwrap_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_astwrap_setRedirectedParty,
	.set_callerid_presentation 	= sccp_astwrap_setCalleridPresentation,
	.set_connected_line		= sccp_astwrap_updateConnectedLine,
	.sendRedirectedUpdate		= sccp_astwrap_sendRedirectedUpdate,
	/* database */
	.feature_addToDatabase 		= sccp_astwrap_addToDatabase,
	.feature_getFromDatabase 	= sccp_astwrap_getFromDatabase,
	.feature_removeFromDatabase     = sccp_astwrap_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_astwrap_removeTreeFromDatabase,
	.feature_monitor		= sccp_astgenwrap_featureMonitor,
	
	.feature_park			= sccp_astwrap_park,
	.getFeatureExtension		= sccp_astwrap_getFeatureExtension,
	.getPickupExtension		= sccp_astwrap_getPickupExtension,
	.findChannelByCallback		= sccp_astwrap_findChannelWithCallback,

	.moh_start			= sccp_astwrap_moh_start,
	.moh_stop			= sccp_astwrap_moh_stop,
	.queue_control			= sccp_astwrap_queue_control,
	.queue_control_data		= sccp_astwrap_queue_control_data,

	.allocTempPBXChannel		= sccp_astwrap_allocTempPBXChannel,
	.masqueradeHelper		= sccp_astwrap_masqueradeHelper,
	.requestAnnouncementChannel	= sccp_astwrap_requestAnnouncementChannel,
	.set_language			= sccp_astwrap_setLanguage,

	.getExtensionState		= sccp_astwrap_getExtensionState,
	.findPickupChannelByExtenLocked = sccp_astwrap_findPickupChannelByExtenLocked,
	.findPickupChannelByGroupLocked = sccp_astwrap_findPickupChannelByGroupLocked,

	.set_owner			= sccp_astwrap_setOwner,
	.removeTimingFD			= sccp_astwrap_removeTimingFD,
	.dumpchan			= NULL,
	.channel_is_bridged		= sccp_astwrap_channelIsBridged,
	.get_bridged_channel		= sccp_astwrap_getBridgeChannel,
	.get_underlying_channel		= sccp_astwrap_getUnderlyingChannel,
	.attended_transfer		= sccp_astwrap_attended_transfer,

	.set_callgroup			= sccp_astgenwrap_set_callgroup,
	.set_pickupgroup		= sccp_astgenwrap_set_pickupgroup,
	.set_named_callgroups		= NULL,
	.set_named_pickupgroups		= NULL,

	.register_application		= sccp_wrapper_register_application,
	.unregister_application		= sccp_wrapper_unregister_application,
	.register_function		= sccp_wrapper_register_function,
	.unregister_function		= sccp_wrapper_unregister_function,

	.get_dtmf_payload_code		= sccp_wrapper_get_dtmf_payload_code,
	/* *INDENT-ON* */
};
#endif

static int unload_module(void)
{
	sccp_preUnload();
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP RTP protocol\n");
	ast_rtp_glue_unregister(&sccp_rtp);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP Channel Tech\n");

	sccp_tech.capabilities = ast_format_cap_destroy(sccp_tech.capabilities);
	ast_channel_unregister(&sccp_tech);
	sccp_unregister_dialplan_functions();
	sccp_unregister_cli();
#ifdef CS_SCCP_MANAGER
	sccp_unregister_management();
#endif
#ifdef HAVE_PBX_MESSAGE_H
	ast_msg_tech_unregister(&sccp_msg_tech);
#endif

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
		ast_sched_context_destroy(sched);
		sched = NULL;
	}

	pbx_log(LOG_NOTICE, "Running Cleanup\n");
	sccp_free(sccp_globals);
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
		if (!(sched = ast_sched_context_create())) {
			pbx_log(LOG_WARNING, "Unable to create schedule context. SCCP channel type disabled\n");
			break;
		}
		if (ast_sched_start_thread(sched)) {
			pbx_log(LOG_ERROR, "Unable to start scheduler\n");
			ast_sched_context_destroy(sched);
			sched = NULL;
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

		sccp_tech.capabilities = ast_format_cap_alloc();
		ast_format_cap_add_all_by_type(sccp_tech.capabilities, AST_FORMAT_TYPE_AUDIO);
		ast_format_cap_add_all_by_type(sccp_tech.capabilities, AST_FORMAT_TYPE_VIDEO);

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
		if (ast_rtp_glue_register(&sccp_rtp)) {
			pbx_log(LOG_ERROR, "Unable to register RTP Glue\n");
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

static int module_reload(void)
{
	return sccp_reload();
}

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

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, SCCP_VERSIONSTR,.load = load_module,.unload = unload_module,.reload = module_reload,.load_pri = AST_MODPRI_DEFAULT,
		// .description = "Skinny Client Control Protocol (SCCP)",
		.nonoptreq = "chan_local");
#endif

#if UNUSEDCODE // 2015-11-01
PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data)
{
	PBX_CHANNEL_TYPE *remotePeer;

	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

	for (; (remotePeer = ast_channel_iterator_next(iterator)); remotePeer = pbx_channel_unref(remotePeer)) {

		if (found_cb(remotePeer, data)) {
			ast_channel_lock(remotePeer);
			break;
		}

	}
	ast_channel_iterator_destroy(iterator);
	return remotePeer;
}
#endif

PBX_CHANNEL_TYPE *sccp_astwrap_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE * chan, const char *exten, const char *context)
{
	struct ast_channel *target = NULL;									/*!< Potential pickup target */
	struct ast_channel_iterator *iter;

	if (!(iter = ast_channel_iterator_by_exten_new(exten, context))) {
		return NULL;
	}

	while ((target = ast_channel_iterator_next(iter))) {
		ast_channel_lock(target);
		if ((chan != target) && ast_can_pickup(target)) {
			ast_log(LOG_NOTICE, "%s pickup by %s\n", target->name, chan->name);
			break;
		}
		ast_channel_unlock(target);
		target = pbx_channel_unref(target);
	}

	ast_channel_iterator_destroy(iter);
	return target;
}

static int find_channel_by_group(void *obj, void *arg, void *data, int flags)
{
	struct ast_channel *target = obj;
	struct ast_channel *chan = data;

	ast_channel_lock(target);
	if (chan != target && (chan->pickupgroup & target->callgroup)
		&& ast_can_pickup(target)) {
		return CMP_MATCH | CMP_STOP;
	}
	ast_channel_unlock(target);

	return 0;
}

PBX_CHANNEL_TYPE *sccp_astwrap_findPickupChannelByGroupLocked(PBX_CHANNEL_TYPE * chan)
{
	struct ast_channel *target = NULL;									/*!< Potential pickup target */
	target = ast_channel_callback(find_channel_by_group, NULL, chan, 0);
	return target;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
