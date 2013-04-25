
/*!
 * \file        ast106.c
 * \brief       SCCP PBX Asterisk Wrapper Class
 * \author      Marcello Ceshia
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */

#include <config.h>
#include "../../common.h"
#include "ast106.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
#include <asterisk/sched.h>
#include <asterisk/netsock.h>

#define new avoid_cxx_new_keyword
#include <asterisk/rtp.h>
#undef new

#ifndef CS_AST_RTP_INSTANCE_NEW
#define ast_rtp_instance_read(_x, _y) ast_rtp_read(_x)
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
struct sched_context *sched = 0;
struct io_context *io = 0;

#define SCCP_AST_LINKEDID_HELPER "LINKEDID"

//static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, format_t format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause);
static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, int format, void *data, int *cause);
static int sccp_wrapper_asterisk16_call(PBX_CHANNEL_TYPE * chan, char *addr, int timeout);
static int sccp_wrapper_asterisk16_answer(PBX_CHANNEL_TYPE * chan);
static PBX_FRAME_TYPE *sccp_wrapper_asterisk16_rtp_read(PBX_CHANNEL_TYPE * ast);
static int sccp_wrapper_asterisk16_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame);
static int sccp_wrapper_asterisk16_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen);
static int sccp_wrapper_asterisk16_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan);
static enum ast_bridge_result sccp_wrapper_asterisk16_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms);
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE * ast, const char *text);
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE * ast, char digit);
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE * ast, char digit, unsigned int duration);
static int sccp_pbx_sendHTML(PBX_CHANNEL_TYPE * ast, int subclass, const char *data, int datalen);
int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel);
boolean_t sccp_wrapper_asterisk16_allocPBXChannel(sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel, const char *linkedId);
int sccp_asterisk_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control);
int sccp_asterisk_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen);
boolean_t sccp_asterisk_getForwarderPeerChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel);
static int sccp_wrapper_asterisk16_devicestate(void *data);
PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE *chan, const char *exten, const char *context);

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
	bridge:			sccp_wrapper_asterisk16_rtpBridge,
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
	.transfer 		= sccp_pbx_transfer,
	.bridge 		= sccp_wrapper_asterisk16_rtpBridge,
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
	sccp_channelState_t state;

	if ((deviceId = strchr(lineName, '@'))) {
		*deviceId = '\0';
		deviceId++;
	}

	state = sccp_hint_getLinestate(lineName, deviceId);
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
		case SCCP_CHANNELSTATE_ZOMBIE:
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			res = AST_DEVICE_UNAVAILABLE;
			break;

		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
		case SCCP_CHANNELSTATE_PROGRESS:
#ifdef CS_AST_DEVICE_RINGING
			res = AST_DEVICE_RINGING;
			break;
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
	}

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_asterisk_devicestate) PBX  requests state for '%s' - state %s\n", (char *) lineName, ast_devstate2str(res));
	return res;
}

/*!
 * \brief Convert an array of skinny_codecs (enum) to ast_codec_prefs
 *
 * \param skinny_codecs Array of Skinny Codecs
 * \param astCodecPref Array of PBX Codec Preferences
 *
 * \return bit array fmt/Format of ast_format_type (int)
 *
 * \todo check bitwise operator (not sure) - DdG 
 */
int skinny_codecs2pbx_codec_pref(skinny_codec_t * skinny_codecs, struct ast_codec_pref *astCodecPref)
{
	uint32_t i;
	int res_codec = 0;

	for (i = 1; i < SKINNY_MAX_CAPABILITIES; i++) {
		if (skinny_codecs[i]) {
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "adding codec to ast_codec_pref\n");
			res_codec |= ast_codec_pref_append(astCodecPref, skinny_codec2pbx_codec(skinny_codecs[i]));
		}
	}
	return res_codec;
}

static boolean_t sccp_wrapper_asterisk16_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec);

#define RTP_NEW_SOURCE(_c,_log) 								\
        if(c->rtp.audio.rtp) { 										\
                ast_rtp_new_source(c->rtp.audio.rtp); 							\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }

#define RTP_CHANGE_SOURCE(_c,_log) 								\
        if(c->rtp.audio.rtp) {										\
                ast_rtp_change_source(c->rtp.audio.rtp);						\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }

static void get_skinnyFormats(format_t format, skinny_codec_t codecs[], size_t size)
{
	unsigned int x;
	unsigned len = 0;

	if (!size)
		return;

	for (x = 0; x < ARRAY_LEN(skinny2pbx_codec_maps) && len <= size; x++) {
		if (skinny2pbx_codec_maps[x].pbx_codec & format) {
			codecs[len++] = skinny2pbx_codec_maps[x].skinny_codec;
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "map ast codec " UI64FMT " to %d\n", (ULONG) (skinny2pbx_codec_maps[x].pbx_codec & format), skinny2pbx_codec_maps[x].skinny_codec);
		}
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
void *sccp_do_monitor(void *data)
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
		ast_mutex_lock(&GLOB(monitor_lock));

		res = ast_sched_runq(sched);
		if (res >= 20) {
			ast_debug(1, "SCCP: ast_sched_runq ran %d all at once\n", res);
		}
		ast_mutex_unlock(&GLOB(monitor_lock));

		if (GLOB(monitor_thread) == AST_PTHREADT_STOP) {
			return 0;
		}
	}
	/* Never reached */
	return NULL;
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
	sccp_channel_t *c = NULL;
	PBX_FRAME_TYPE *frame = NULL;

	//      if (!(c = get_sccp_channel_from_pbx_channel(ast))) {            // not following the refcount rules... channel is already retained
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {									// not following the refcount rules... channel is already retained
		return &ast_null_frame;
	}

	if (!c->rtp.audio.rtp) {
		//              c = sccp_channel_release(c);
		return &ast_null_frame;
	}

	switch (ast->fdno) {

		case 0:
			frame = ast_rtp_instance_read(c->rtp.audio.rtp, 0);					/* RTP Audio */
			break;
		case 1:
			frame = ast_rtp_instance_read(c->rtp.audio.rtp, 1);					/* RTCP Control Channel */
			break;
#ifdef CS_SCCP_VIDEO
		case 2:
			frame = ast_rtp_instance_read(c->rtp.video.rtp, 0);					/* RTP Video */
			break;
		case 3:
			frame = ast_rtp_instance_read(c->rtp.video.rtp, 1);					/* RTCP Control Channel for video */
			break;
#endif
		default:
			break;
	}

	if (!frame) {
		pbx_log(LOG_WARNING, "%s: error reading frame == NULL\n", c->currentDeviceId);
		//              c = sccp_channel_release(c);
		return &ast_null_frame;
	} else {
		//sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: read format: ast->fdno: %d, frametype: %d, %s(%d)\n", DEV_ID_LOG(c->device), ast->fdno, frame->frametype, pbx_getformatname(frame->subclass), frame->subclass);
		if (frame->frametype == AST_FRAME_VOICE) {
#ifdef CS_SCCP_CONFERENCE
			if (c->conference && ast->readformat == AST_FORMAT_SLINEAR) {
				ast_set_read_format(ast, AST_FORMAT_SLINEAR);
			}
#endif
		}
	}
	//      c = sccp_channel_release(c);
	return frame;
}

static int sccp_wrapper_asterisk16_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	int res = 0;

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		return -1;
	}

	if (!(d = sccp_channel_getDevice_retained(c))) {
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
		d = sccp_device_release(d);
		c = sccp_channel_release(c);
		return res;
	}

	if (c->state == SCCP_CHANNELSTATE_DOWN) {
		c = sccp_channel_release(c);
		d = sccp_device_release(d);
		return -1;
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' condition on channel %s\n", (char *) c->currentDeviceId, ind, ast->name);

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = (((c->rtp.audio.readState != SCCP_RTP_STATUS_INACTIVE) || (d && d->earlyrtp)) ? -1 : 0);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: readStat: %d\n", (char *) c->currentDeviceId, c->rtp.audio.readState);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: res: %d\n", (char *) c->currentDeviceId, res);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: rtp?: %s\n", (char *) c->currentDeviceId, (c->rtp.audio.rtp) ? "yes" : "no");

	switch (ind) {
		case AST_CONTROL_RINGING:
			if (SKINNY_CALLTYPE_OUTBOUND == c->calltype) {
				// Allow signalling of RINGOUT only on outbound calls.
				// Otherwise, there are some issues with late arrival of ringing
				// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
				sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGOUT);
			}
			break;
		case AST_CONTROL_BUSY:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_BUSY);
			break;
		case AST_CONTROL_CONGESTION:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
			break;
		case AST_CONTROL_PROGRESS:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_PROGRESS);
			res = -1;
			break;
		case AST_CONTROL_PROCEEDING:
			sccp_indicate(d, c, SCCP_CHANNELSTATE_PROCEED);
			res = -1;
			break;
		case AST_CONTROL_SRCCHANGE:
			if (c->rtp.audio.rtp)
				ast_rtp_new_source(c->rtp.audio.rtp);

			res = 0;
			break;

		case AST_CONTROL_SRCUPDATE:
			/* Source media has changed. */
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

			if (c->rtp.audio.rtp) {
				ast_rtp_change_source(c->rtp.audio.rtp);
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
			if (c->rtp.audio.rtp) {
				ast_rtp_new_source(c->rtp.audio.rtp);
			}
			res = 0;
			break;

		case AST_CONTROL_VIDUPDATE:									/* Request a video frame update */
			if (c->rtp.video.rtp && d && sccp_device_isVideoSupported(d)) {
				d->protocol->sendFastPictureUpdate(d, c);
				res = 0;
			} else
				res = -1;
			break;
#ifdef CS_AST_CONTROL_INCOMPLETE
		case AST_CONTROL_INCOMPLETE:									/*!< Indication that the extension dialed is incomplete */
			/* \todo implement dial continuation by:
			 *  - display message incomplete number
			 *  - adding time to channel->scheduler.digittimeout
			 *  - rescheduling sccp_pbx_sched_dial 
			 */
#ifdef CS_EXPERIMENTAL
			if (c->scheduler.digittimeout) {
				SCCP_SCHED_DEL(c->scheduler.digittimeout);
			}

			sccp_indicate(d, c, SCCP_CHANNELSTATE_DIGITSFOLL);
			c->scheduler.digittimeout = PBX(sched_add) (c->enbloc.digittimeout, sccp_pbx_sched_dial, c);
#endif
			res = 0;
			break;
#endif
		case -1:											// Asterisk prod the channel
			res = -1;
			break;
		default:
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Don't know how to indicate condition %d\n", ind);
			res = -1;
	}
	//ast_cond_signal(&c->astStateCond);
	d = sccp_device_release(d);
	c = sccp_channel_release(c);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: send asterisk result %d\n", res);
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
	sccp_channel_t *c = NULL;

	//      if (!(c = get_sccp_channel_from_pbx_channel(ast))) {            // not following the refcount rules... channel is already retained
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
					if (c->parentChannel) {
						sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: forwarding prodding to parentChannel %s\n", c->parentChannel->owner->name);
						sccp_wrapper_asterisk16_rtp_write(c->parentChannel->owner, frame);
					}
				}
			} else if (!(frame->subclass & ast->nativeformats)) {
				//                      char s1[512], s2[512], s3[512];
				//                      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", c->currentDeviceId, frame->subclass, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (uint64_t)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (uint64_t)ast->writeformat), (uint64_t)ast->writeformat);
				//                      //! \todo correct debugging
				//                      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", c->currentDeviceId, (int)frame->frametype, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (uint64_t)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (uint64_t)ast->writeformat), (uint64_t)ast->writeformat);
				//return -1;
			}
#if 0
			if ((ast->rawwriteformat = ast->writeformat) && ast->writetrans) {
				ast_translator_free_path(ast->writetrans);
				ast->writetrans = NULL;

				ast_set_write_format(ast, frame->subclass);
			}
#endif
			if (c->rtp.audio.rtp) {
				res = ast_rtp_write(c->rtp.audio.rtp, frame);
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.writeState == SCCP_RTP_STATUS_INACTIVE && c->rtp.video.rtp && c->state != SCCP_CHANNELSTATE_HOLD) {
				int codec = pbx_codec2skinny_codec((frame->subclass & AST_FORMAT_VIDEO_MASK));

				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: got video frame %d\n", c->currentDeviceId, codec);
				if (0 != codec) {
					c->rtp.video.writeFormat = codec;
					sccp_channel_openMultiMediaChannel(c);
				}
			}

			if (c->rtp.video.rtp && (c->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE) != 0) {
				res = ast_rtp_write(c->rtp.video.rtp, frame);
			}
#endif
			break;
		case AST_FRAME_TEXT:
		case AST_FRAME_MODEM:
		default:
			pbx_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %s\n", c->currentDeviceId, frame->frametype, ast->name);
			break;
	}
	//      c = sccp_channel_release(c);
	return res;
}

static int sccp_wrapper_asterisk16_sendDigits(const sccp_channel_t * channel, const char *digits)
{
	if (!channel || !channel->owner) {
		pbx_log(LOG_WARNING, "No channel to send digits to\n");
		return 0;
	}
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int i;
	PBX_FRAME_TYPE f;

	f = ast_null_frame;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digits '%s'\n", (char *) channel->currentDeviceId, digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	// CS_AST_NEW_FRAME_STRUCT

	//      f.frametype = AST_FRAME_DTMF_BEGIN;
	//      ast_queue_frame(pbx_channel, &f);
	for (i = 0; digits[i] != '\0'; i++) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", (char *) channel->currentDeviceId, digits[i]);

		f.frametype = AST_FRAME_DTMF_END;								// Sending only the dmtf will force asterisk to start with DTMF_BEGIN and schedule the DTMF_END
		f.subclass = digits[i];
		//              f.samples = SCCP_MIN_DTMF_DURATION * 8;
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

static void sccp_wrapper_asterisk16_setCalleridPresence(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

	if (CALLERID_PRESENCE_FORBIDDEN == channel->callInfo.presentation) {
		pbx_channel->cid.cid_pres |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
	}
}

static int sccp_wrapper_asterisk16_setNativeAudioFormats(const sccp_channel_t * channel, skinny_codec_t codec[], int length)
{

	//#ifdef CS_EXPERIMENTAL
	format_t new_nativeformats = 0;
	int i;

	ast_debug(10, "%s: set native Formats length: %d\n", (char *) channel->currentDeviceId, length);

	for (i = 0; i < length; i++) {
		new_nativeformats |= skinny_codec2pbx_codec(codec[i]);
		ast_debug(10, "%s: set native Formats to %d, skinny: %d\n", (char *) channel->currentDeviceId, (int) channel->owner->nativeformats, codec[i]);
	}
	if (channel->owner->nativeformats != new_nativeformats) {
		channel->owner->nativeformats = new_nativeformats;
		char codecs[512];

		sccp_multiple_codecs2str(codecs, sizeof(codecs) - 1, codec, length);
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_2 "%s: updated native Formats to %d, length: %d, skinny: [%s]\n", (char *) channel->currentDeviceId, (int) channel->owner->nativeformats, length, codecs);
	}
	//#else
	//      channel->owner->nativeformats = skinny_codec2pbx_codec(codec[0]);
	//#endif
	return 1;
}

static int sccp_wrapper_asterisk16_setNativeVideoFormats(const sccp_channel_t * channel, uint32_t formats)
{
	return 1;
}

boolean_t sccp_wrapper_asterisk16_allocPBXChannel(sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel, const char *linkedId)
{
	sccp_line_t *line = NULL;

	(*pbx_channel) = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);

	if ((*pbx_channel) == NULL) {
		return FALSE;
	}

	if (!channel || !channel->line) {
		return FALSE;
	}
	line = channel->line;

	(*pbx_channel)->tech = &sccp_tech;
	(*pbx_channel)->tech_pvt = sccp_channel_retain(channel);

	memset((*pbx_channel)->exten, 0, sizeof((*pbx_channel)->exten));

	sccp_copy_string((*pbx_channel)->context, line->context, sizeof((*pbx_channel)->context));

	if (!sccp_strlen_zero(line->language))
		ast_string_field_set((*pbx_channel), language, line->language);

	if (!sccp_strlen_zero(line->accountcode))
		ast_string_field_set((*pbx_channel), accountcode, line->accountcode);

	if (!sccp_strlen_zero(line->musicclass))
		ast_string_field_set((*pbx_channel), musicclass, line->musicclass);

	if (line->amaflags)
		(*pbx_channel)->amaflags = line->amaflags;
	if (line->callgroup)
		(*pbx_channel)->callgroup = line->callgroup;
#if CS_SCCP_PICKUP
	if (line->pickupgroup)
		(*pbx_channel)->pickupgroup = line->pickupgroup;
#endif
	(*pbx_channel)->priority = 1;
	(*pbx_channel)->adsicpe = AST_ADSI_UNAVAILABLE;

	/** the the tonezone using language information */
	if (!sccp_strlen_zero(line->language) && ast_get_indication_zone(line->language)) {
		(*pbx_channel)->zone = ast_get_indication_zone(line->language);					/* this will core asterisk on hangup */
	}

	/* create/set linkid */
	char linkedid[50];

	sprintf(linkedid, "SCCP::%-10d", channel->callid);
	if (PBX(setChannelLinkedId))
		PBX(setChannelLinkedId) (channel, linkedid);
	/* done */

	channel->owner = (*pbx_channel);

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
	context = ast_strdupa(pbxChannel->context);
	exten = ast_strdupa(pbxChannel->exten);
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

static boolean_t sccp_wrapper_asterisk16_allocTempPBXChannel(PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** pbxDstChannel)
{
	if (!pbxSrcChannel) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) no pbx channel provided\n");
		return FALSE;
	}
	(*pbxDstChannel) = ast_channel_alloc(0, pbxSrcChannel->_state, 0, 0, pbxSrcChannel->accountcode, pbxSrcChannel->exten, pbxSrcChannel->context, pbxSrcChannel->amaflags, "TMP/%s", pbxSrcChannel->name);
	if ((*pbxDstChannel) == NULL) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_conferenceTempPBXChannel) create pbx channel failed\n");
		return FALSE;
	}

	(*pbxDstChannel)->writeformat = pbxSrcChannel->writeformat;
	(*pbxDstChannel)->readformat = pbxSrcChannel->readformat;
	pbx_builtin_setvar_helper(*pbxDstChannel, "__" SCCP_AST_LINKEDID_HELPER, pbx_builtin_getvar_helper(pbxSrcChannel, SCCP_AST_LINKEDID_HELPER));
	return TRUE;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_requestForeignChannel(const char *type, pbx_format_type format, const PBX_CHANNEL_TYPE * requestor, void *data)
{
	PBX_CHANNEL_TYPE *chan;
	int cause;

	if (!(chan = ast_request(type, format, data, &cause))) {
		pbx_log(LOG_ERROR, "SCCP: Requested channel could not be created, cause: %d", cause);
		return NULL;
	}
	return chan;
}

int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel)
{
	sccp_channel_t *c;
	int res = 1;

	if ((c = get_sccp_channel_from_pbx_channel(ast_channel))) {
		if (ast_test_flag(ast_channel, AST_FLAG_ANSWERED_ELSEWHERE) || ast_channel->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere\n");
			c->answered_elsewhere = TRUE;
		}
		res = sccp_pbx_hangup(c);
		sccp_channel_release(c);
	}
	ast_channel->tech_pvt = NULL;
	c = c ? sccp_channel_release(c) : NULL;
	ast_channel = pbx_channel_unref(ast_channel);
	return res;
}

/*!
 * \brief Parking Thread Arguments Structure
 */
struct parkingThreadArg {
	PBX_CHANNEL_TYPE *bridgedChannel;
	PBX_CHANNEL_TYPE *hostChannel;
	const sccp_device_t *device;
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
	if (f)
		ast_frfree(f);

	res = ast_park_call(arg->bridgedChannel, arg->hostChannel, 0, &ext);

	if (!res) {
		extstr[0] = 128;
		extstr[1] = SKINNY_LBL_CALL_PARK_AT;
		sprintf(&extstr[2], " %d", ext);

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
		if (pbx_bridgedChannelClone)
			ast_hangup(pbx_bridgedChannelClone);

		if (pbx_hostChannelClone)
			ast_hangup(pbx_hostChannelClone);

		return PARK_RESULT_FAIL;
	}
	if (!(arg = (struct parkingThreadArg *) ast_calloc(1, sizeof(struct parkingThreadArg)))) {
		return PARK_RESULT_FAIL;
	}

	arg->bridgedChannel = pbx_bridgedChannelClone;
	arg->hostChannel = pbx_hostChannelClone;
	if ((arg->device = sccp_channel_getDevice_retained(hostChannel))) {
		if (!ast_pthread_create_detached_background(&th, NULL, sccp_wrapper_asterisk16_park_thread, arg)) {
			return PARK_RESULT_SUCCESS;
		}
		arg->device = sccp_device_release(arg->device);
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
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(target);

	int res = -1;

	target_name = sccp_strdupa(target->name);
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
		if (target->cid.cid_name)
			target_cid_name = strdup(target->cid.cid_name);
		if (target->cid.cid_num)
			target_cid_number = strdup(target->cid.cid_num);
		if (target->cid.cid_ani)
			target_cid_ani = strdup(target->cid.cid_ani);
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
	chan_name = sccp_strdupa(chan->name);
	/* exchange callerid info */
	if (c) {
		if (chan && !sccp_strlen_zero(chan->cid.cid_name))
			sccp_copy_string(c->callInfo.originalCalledPartyName, chan->cid.cid_name, sizeof(c->callInfo.originalCalledPartyName));

		if (chan && !sccp_strlen_zero(chan->cid.cid_num))
			sccp_copy_string(c->callInfo.originalCalledPartyNumber, chan->cid.cid_num, sizeof(c->callInfo.originalCalledPartyNumber));

		if (!sccp_strlen_zero(target_cid_name)) {
			sccp_copy_string(c->callInfo.calledPartyName, target_cid_name, sizeof(c->callInfo.callingPartyName));
			sccp_free(target_cid_name);
		}
		if (!sccp_strlen_zero(target_cid_number)) {
			sccp_copy_string(c->callInfo.calledPartyNumber, target_cid_number, sizeof(c->callInfo.callingPartyNumber));
			sccp_free(target_cid_number);
		}

		/* we use the chan->cid.cid_name to do the magic */
		if (!sccp_strlen_zero(target_cid_ani)) {
			sccp_copy_string(c->callInfo.callingPartyNumber, target_cid_ani, sizeof(c->callInfo.callingPartyNumber));
			sccp_copy_string(c->callInfo.callingPartyName, target_cid_ani, sizeof(c->callInfo.callingPartyName));
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

/*!
 * \brief Pickup asterisk channel target using chan
 * 
 * \param chan initial channel that request the parking           
 * \param target Channel t park to
 */
static boolean_t sccp_wrapper_asterisk16_pickupChannel(const sccp_channel_t * chan, PBX_CHANNEL_TYPE * target)
{
	boolean_t result;

	result = ast_do_pickup(chan->owner, target) ? FALSE : TRUE;

	return result;
}

static uint8_t sccp_wrapper_asterisk16_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	uint32_t astCodec;
	int payload;

	astCodec = skinny_codec2pbx_codec(codec);
	payload = ast_rtp_lookup_code(rtp->rtp, 1, astCodec);

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

	for (i = 1; i < ARRAY_LEN(skinny_codecs); i++) {
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

	if (!pbx_channel || !pbx_channel->context) {
		pbx_log(LOG_ERROR, "%s: (extension_status) Either no pbx_channel or no valid context provided to lookup number\n", channel->designator);
		return SCCP_EXTENSION_NOTEXISTS;
	}
	int ignore_pat = ast_ignore_pattern(pbx_channel->context, channel->dialedNumber);
	int ext_exist = ast_exists_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_canmatch = ast_canmatch_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_matchmore = ast_matchmore_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "+=sccp extension matcher says==================+\n" VERBOSE_PREFIX_2 "|ignore     |exists     |can match  |match more|\n" VERBOSE_PREFIX_2 "|%3s        |%3s        |%3s        |%3s       |\n" VERBOSE_PREFIX_2 "+==============================================+\n", ignore_pat ? "yes" : "no", ext_exist ? "yes" : "no", ext_canmatch ? "yes" : "no", ext_matchmore ? "yes" : "no");
	if (ignore_pat) {
		return SCCP_EXTENSION_NOTEXISTS;
	} else if (ext_exist) {
		if (ext_canmatch && !ext_matchmore) {
			return SCCP_EXTENSION_EXACTMATCH;
		} else {
			return SCCP_EXTENSION_MATCHMORE;
		}
	}

	return SCCP_EXTENSION_NOTEXISTS;
}

//static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, format_t format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause)
static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, int format, void *data, int *cause)
{
	sccp_channel_request_status_t requestStatus;
	sccp_channel_t *channel = NULL;

	skinny_codec_t audioCapabilities[SKINNY_MAX_CAPABILITIES];
	skinny_codec_t videoCapabilities[SKINNY_MAX_CAPABILITIES];

	memset(&audioCapabilities, 0, sizeof(audioCapabilities));
	memset(&videoCapabilities, 0, sizeof(videoCapabilities));

	//! \todo parse request
	char *lineName;
	skinny_codec_t codec = SKINNY_CODEC_G711_ULAW_64K;
	sccp_autoanswer_type_t autoanswer_type = SCCP_AUTOANSWER_NONE;
	uint8_t autoanswer_cause = AST_CAUSE_NOTDEFINED;
	int ringermode = 0;

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
	lineName = strdup((const char *) data);
	/* parsing options string */
	char *options = NULL;
	int optc = 0;
	char *optv[2];
	int opti = 0;

	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked us to create a channel with type=%s, format=" UI64FMT ", lineName=%s, options=%s\n", type, (uint64_t) format, lineName, (options) ? options : "");

	/* get ringer mode from ALERT_INFO */
	/* requestor is not available in asterisk 1.6 */
	/*
	   const char *alert_info = NULL;
	   if(requestor){
	   alert_info = pbx_builtin_getvar_helper((PBX_CHANNEL_TYPE *)requestor, "_ALERT_INFO");
	   if(!alert_info){
	   alert_info = pbx_builtin_getvar_helper((PBX_CHANNEL_TYPE *)requestor, "__ALERT_INFO");
	   }
	   }    
	   if (alert_info && !sccp_strlen_zero(alert_info)) {
	   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Found ALERT_INFO=%s\n", alert_info);
	   if (strcasecmp(alert_info, "inside") == 0)
	   ringermode = SKINNY_STATION_INSIDERING;
	   else if (strcasecmp(alert_info, "feature") == 0)
	   ringermode = SKINNY_STATION_FEATURERING;
	   else if (strcasecmp(alert_info, "silent") == 0)
	   ringermode = SKINNY_STATION_SILENTRING;
	   else if (strcasecmp(alert_info, "urgent") == 0)
	   ringermode = SKINNY_STATION_URGENTRING;
	   }
	 */
	/* done ALERT_INFO parsing */

	/* parse options */
	if (options && (optc = sccp_app_separate_args(options, '/', optv, sizeof(optv) / sizeof(optv[0])))) {
		pbx_log(LOG_NOTICE, "parse options\n");
		for (opti = 0; opti < optc; opti++) {
			pbx_log(LOG_NOTICE, "parse option '%s'\n", optv[opti]);
			if (!strncasecmp(optv[opti], "aa", 2)) {
				/* let's use the old style auto answer aa1w and aa2w */
				if (!strncasecmp(optv[opti], "aa1w", 4)) {
					autoanswer_type = SCCP_AUTOANSWER_1W;
					optv[opti] += 4;
				} else if (!strncasecmp(optv[opti], "aa2w", 4)) {
					autoanswer_type = SCCP_AUTOANSWER_2W;
					optv[opti] += 4;
				} else if (!strncasecmp(optv[opti], "aa=", 3)) {
					optv[opti] += 3;
					pbx_log(LOG_NOTICE, "parsing aa\n");
					if (!strncasecmp(optv[opti], "1w", 2)) {
						autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti] += 2;
					} else if (!strncasecmp(optv[opti], "2w", 2)) {
						autoanswer_type = SCCP_AUTOANSWER_2W;
						pbx_log(LOG_NOTICE, "set aa to 2w\n");
						optv[opti] += 2;
					}
				}

				/* since the pbx ignores autoanswer_cause unless SCCP_RWLIST_GETSIZE(l->channels) > 1, it is safe to set it if provided */
				if (!sccp_strlen_zero(optv[opti]) && (autoanswer_cause)) {
					if (!strcasecmp(optv[opti], "b"))
						autoanswer_cause = AST_CAUSE_BUSY;
					else if (!strcasecmp(optv[opti], "u"))
						autoanswer_cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
					else if (!strcasecmp(optv[opti], "c"))
						autoanswer_cause = AST_CAUSE_CONGESTION;
				}
				if (autoanswer_cause)
					*cause = autoanswer_cause;
				/* check for ringer options */
			} else if (!strncasecmp(optv[opti], "ringer=", 7)) {
				optv[opti] += 7;
				if (!strcasecmp(optv[opti], "inside"))
					ringermode = SKINNY_STATION_INSIDERING;
				else if (!strcasecmp(optv[opti], "outside"))
					ringermode = SKINNY_STATION_OUTSIDERING;
				else if (!strcasecmp(optv[opti], "feature"))
					ringermode = SKINNY_STATION_FEATURERING;
				else if (!strcasecmp(optv[opti], "silent"))
					ringermode = SKINNY_STATION_SILENTRING;
				else if (!strcasecmp(optv[opti], "urgent"))
					ringermode = SKINNY_STATION_URGENTRING;
				else
					ringermode = SKINNY_STATION_OUTSIDERING;
			} else {
				pbx_log(LOG_WARNING, "Wrong option %s\n", optv[opti]);
			}
		}
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

	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, audioCapabilities, ARRAY_LEN(audioCapabilities));
	//      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote audio caps: %s\n", cap_buf);

	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, videoCapabilities, ARRAY_LEN(videoCapabilities));
	//      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote video caps: %s\n", cap_buf);

	/** done */

	/** get requested format */
	codec = pbx_codec2skinny_codec(format);

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

	if (!sccp_pbx_channel_allocate(channel, NULL)) {
		//! \todo handle error in more detail, cleanup sccp channel
		pbx_log(LOG_WARNING, "SCCP: Unable to allocate channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	}
	PBX_CHANNEL_TYPE *requestor = channel->owner;

	// set calling party 
	sccp_channel_set_callingparty(channel, requestor->cid.cid_name, requestor->cid.cid_num);
	sccp_channel_set_originalCalledparty(channel, NULL, requestor->cid.cid_dnid);

EXITFUNC:
	if (lineName)
		sccp_free(lineName);
	sccp_restart_monitor();
	sccp_channel_release(channel);
	return (channel && channel->owner) ? channel->owner : NULL;
}

/*
 * \brief get callerid_name from pbx
 * \param sccp_channle Asterisk Channel
 * \param cid name result
 * \return parse result
 */
static int sccp_wrapper_asterisk16_callerid_name(const sccp_channel_t * channel, char **cid_name)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->cid.cid_name && strlen(pbx_chan->cid.cid_name) > 0) {
		*cid_name = strdup(pbx_chan->cid.cid_name);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_number(const sccp_channel_t * channel, char **cid_number)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->cid.cid_num && strlen(pbx_chan->cid.cid_num) > 0) {
		*cid_number = strdup(pbx_chan->cid.cid_num);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_ton from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_ton(const sccp_channel_t * channel, char **cid_ton)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	return pbx_chan->cid.cid_ton;
}

/*
 * \brief get callerid_ani from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_ani(const sccp_channel_t * channel, char **cid_ani)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->cid.cid_ani && strlen(pbx_chan->cid.cid_ani) > 0) {
		*cid_ani = strdup(pbx_chan->cid.cid_ani);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_dnid from pbx (Destination ID)
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_dnid(const sccp_channel_t * channel, char **cid_dnid)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->cid.cid_dnid && strlen(pbx_chan->cid.cid_dnid) > 0) {
		*cid_dnid = strdup(pbx_chan->cid.cid_dnid);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_rdnis from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_rdnis(const sccp_channel_t * channel, char **cid_rdnis)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->cid.cid_rdnis && strlen(pbx_chan->cid.cid_rdnis) > 0) {
		*cid_rdnis = strdup(pbx_chan->cid.cid_rdnis);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_presence from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk16_callerid_presence(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	//      return pbx_chan->cid.cid_pres;
	if (pbx_chan->cid.cid_pres) {
		return CALLERID_PRESENCE_ALLOWED;
	}
	return CALLERID_PRESENCE_FORBIDDEN;
}

static int sccp_wrapper_asterisk16_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout)
{
	sccp_channel_t *c = NULL;
	struct varshead *headp;
	struct ast_var_t *current;
	int res = 0;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk request to call %s (dest:%s, timeout: %d)\n", pbx_channel_name(ast), dest, timeout);

	if (!sccp_strlen_zero(pbx_channel_call_forward(ast))) {
		PBX(queue_control) (ast, -1);									/* Prod Channel if in the middle of a call_forward instead of proceed */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Forwarding Call to '%s'\n", pbx_channel_call_forward(ast));
		return 0;
	}

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
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
		}
	}
	char *cid_name = NULL;
	char *cid_number = NULL;

	sccp_wrapper_asterisk16_callerid_name(c, &cid_name);
	sccp_wrapper_asterisk16_callerid_number(c, &cid_number);

	sccp_channel_set_callingparty(c, cid_name, cid_number);

	if (cid_name) {
		free(cid_name);
	}

	if (cid_number) {
		free(cid_number);
	}

	res = sccp_pbx_call(c, dest, timeout);
	c = sccp_channel_release(c);
	return res;

}

static int sccp_wrapper_asterisk16_answer(PBX_CHANNEL_TYPE * chan)
{
	//! \todo change this handling and split pbx and sccp handling -MC
	int res = -1;
	sccp_channel_t *channel = NULL;

	if ((channel = get_sccp_channel_from_pbx_channel(chan))) {
		res = sccp_pbx_answer(channel);
		channel = sccp_channel_release(channel);
	}
	return res;
}

/**
 * 
 * \todo update remote capabilities after fixup
 */
static int sccp_wrapper_asterisk16_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s\n", newchan->name);
	sccp_channel_t *c = NULL;

	if (!(c = get_sccp_channel_from_pbx_channel(newchan))) {
		pbx_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, (void *) oldchan, newchan->name, (void *) newchan);
		return -1;
	}

	c->owner = newchan;
	c = sccp_channel_release(c);
	return 0;
}

static enum ast_bridge_result sccp_wrapper_asterisk16_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms)
{
	enum ast_bridge_result res;
	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	sccp_channel_t *sc0, *sc1;

	if ((sc0 = get_sccp_channel_from_pbx_channel(c0)) && (sc1 = get_sccp_channel_from_pbx_channel(c1))) {
		// Switch off DTMF between SCCP phones
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_0;
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_1;
		if (GLOB(directrtp)) {
			ast_channel_defer_dtmf(c0);
			ast_channel_defer_dtmf(c1);
		} else {
			sccp_device_t *d0 = sccp_channel_getDevice_retained(sc0);

			if (d0) {
				sccp_device_t *d1 = sccp_channel_getDevice_retained(sc1);

				if (d1) {
					if (d0->directrtp && d1->directrtp) {
						ast_channel_defer_dtmf(c0);
						ast_channel_defer_dtmf(c1);
					}
					d1 = sccp_device_release(d1);
				}
				d0 = sccp_device_release(d0);
			}
		}
		sc0->peerIsSCCP = TRUE;
		sc1->peerIsSCCP = TRUE;
		// SCCP Key handle direction to asterisk is still to be implemented here
		// sccp_pbx_senddigit
		sc0 = sccp_channel_release(sc0);
		sc1 = sccp_channel_release(sc1);
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

static enum ast_rtp_get_result sccp_wrapper_asterisk16_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{

	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GET_FAILED;
	}

	*rtp = audioRTP->rtp;
	if (!*rtp)
		return AST_RTP_GET_FAILED;
#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GET_FAILED;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	return res;
}

static int sccp_wrapper_asterisk16_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	struct sockaddr_in them;

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) format: %d\n", (int) codecs);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: __FILE__\n");
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return -1;
	}
	if (!c->line) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO LINE\n");
		return -1;
	}

	if (!(d = sccp_channel_getDevice_retained(c))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO DEVICE\n");
		return -1;
	}

	if (!d->directrtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Direct RTP disabled\n");
		d = sccp_device_release(d);
		return -1;
	}

	if (rtp) {
		ast_rtp_get_peer(rtp, &them);
	} else {
		if (ast->_state != AST_STATE_UP) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%lu, nat=%d\n", (long unsigned int) codecs, d->nat);
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%lu, nat=%d\n", (long unsigned int) codecs, d->nat);
		}
	}
	/* Need a return here to break the bridge */
	d = sccp_device_release(d);
	return 0;
}

static enum ast_rtp_get_result sccp_wrapper_asterisk16_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);							//! \todo should this not be getVideoPeerInfo
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GET_FAILED;
	}

	*rtp = audioRTP->rtp;
	if (!*rtp)
		return AST_RTP_GET_FAILED;
#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_TRY_PARTIAL;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	return res;
}

static int sccp_wrapper_asterisk16_getCodec(PBX_CHANNEL_TYPE * ast)
{
	format_t format = AST_FORMAT_ULAW;
	sccp_channel_t *channel;

	if (!(channel = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP | DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (getCodec) NO PVT\n");
		return format;
	}

	ast_debug(10, "asterisk requests format for channel %s, readFormat: %s(%d)\n", ast->name, codec2str(channel->rtp.audio.readFormat), channel->rtp.audio.readFormat);
	if (channel->remoteCapabilities.audio)
		return skinny_codecs2pbx_codecs(channel->remoteCapabilities.audio);
	else
		return skinny_codecs2pbx_codecs(channel->capabilities.audio);
}

static int sccp_wrapper_asterisk16_rtp_stop(sccp_channel_t * channel)
{
	if (channel->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping phone media transmission on channel %08X\n", channel->callid);
		ast_rtp_stop(channel->rtp.audio.rtp);
	}

	if (channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping video media transmission on channel %08X\n", channel->callid);
		ast_rtp_stop(channel->rtp.video.rtp);
	}
	return 0;
}

static boolean_t sccp_wrapper_asterisk16_create_audio_rtp(sccp_channel_t * c)
{
	sccp_session_t *s = NULL;
	sccp_device_t *d = NULL;

	if (!c)
		return FALSE;
	if (!(d = sccp_channel_getDevice_retained(c)))
		return FALSE;

	s = d->session;
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating rtp server connection on %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->ourip));

	c->rtp.audio.rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);
	if (!c->rtp.audio.rtp) {
		d = sccp_device_release(d);
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: rtp server created\n", DEV_ID_LOG(d));
	if (c->owner) {
		ast_channel_set_fd(c->owner, 0, ast_rtp_fd(c->rtp.audio.rtp));
		ast_channel_set_fd(c->owner, 1, ast_rtcp_fd(c->rtp.audio.rtp));
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	ast_rtp_setqos(c->rtp.audio.rtp, d->audio_tos, d->audio_cos, "SCCP RTP");
	d = sccp_device_release(d);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_create_video_rtp(sccp_channel_t * c)
{
	sccp_session_t *s = NULL;
	sccp_device_t *d = NULL;

	if (!c)
		return FALSE;
	if (!(d = sccp_channel_getDevice_retained(c)))
		return FALSE;

	s = d->session;
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating vrtp server connection at %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->ourip));

	c->rtp.video.rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);
	if (!c->rtp.video.rtp) {
		d = sccp_device_release(d);
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: vrtp created\n", DEV_ID_LOG(d));
	if (c->owner) {
		ast_channel_set_fd(c->owner, 2, ast_rtp_fd(c->rtp.video.rtp));
		ast_channel_set_fd(c->owner, 3, ast_rtcp_fd(c->rtp.video.rtp));
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	ast_rtp_setqos(c->rtp.video.rtp, d->video_tos, d->video_cos, "SCCP VRTP");
	d = sccp_device_release(d);
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

static boolean_t sccp_wrapper_asterisk16_rtpGetPeer(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	//      ast_rtp_get_them(rtp, address);
	ast_rtp_get_peer(rtp, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	ast_rtp_get_us(rtp, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_getChannelByName(const char *name, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *ast_channel = NULL;

	sccp_log((DEBUGCAT_PBX + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "(PBX(getChannelByName)) searching for channel with identification %s\n", name);
	while ((ast_channel = pbx_channel_walk_locked(ast_channel)) != NULL) {
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

	   if (!ast)
	   return FALSE;

	   *pbx_channel = ast;
	   ast_channel_unlock(ast);
	   return TRUE;
	 */
}

static int sccp_wrapper_asterisk16_rtp_set_peer(const struct sccp_rtp *rtp, const struct sockaddr_in *new_peer, int nat_active)
{
	struct sockaddr_in peer;

	memcpy(&peer.sin_addr, &new_peer->sin_addr, sizeof(peer.sin_addr));
	peer.sin_port = new_peer->sin_port;

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Tell asterisk to send rtp media to %s:%d\n", ast_inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
	ast_rtp_set_peer(rtp->rtp, &peer);

	if (nat_active)
		ast_rtp_setnat(rtp->rtp, 1);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_setWriteFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{

	channel->owner->rawwriteformat = skinny_codec2pbx_codec(codec);
	channel->owner->nativeformats |= channel->owner->rawwriteformat;

#ifndef CS_EXPERIMENTAL_CODEC
	if (!channel->owner->writeformat) {
		channel->owner->writeformat = channel->owner->rawwriteformat;
	}

	if (channel->owner->writetrans) {
		ast_translator_free_path(channel->owner->writetrans);
		channel->owner->writetrans = NULL;
	}
#endif
	ast_set_write_format(channel->owner, channel->owner->rawwriteformat);

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write native: %d\n", (int) channel->owner->rawwriteformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write: %d\n", (int) channel->owner->writeformat);

#ifdef CS_EXPERIMENTAL_CODEC
	PBX_CHANNEL_TYPE *bridge;

	if (PBX(getRemoteChannel) (channel, &bridge)) {
		channel->owner->writeformat = 0;

		bridge->readformat = 0;
		ast_channel_make_compatible(bridge, channel->owner);
	} else {
		ast_set_write_format(channel->owner, channel->owner->rawwriteformat);
	}
#else
	channel->owner->nativeformats = channel->owner->rawwriteformat;
	ast_set_write_format(channel->owner, channel->owner->writeformat);
#endif

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{

	channel->owner->rawreadformat = skinny_codec2pbx_codec(codec);
	channel->owner->nativeformats = channel->owner->rawreadformat;

#ifndef CS_EXPERIMENTAL_CODEC
	if (!channel->owner->readformat) {
		channel->owner->readformat = channel->owner->rawreadformat;
	}

	if (channel->owner->readtrans) {
		ast_translator_free_path(channel->owner->readtrans);
		channel->owner->readtrans = NULL;
	}
#endif
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read native: %d\n", (int) channel->owner->rawreadformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read: %d\n", (int) channel->owner->readformat);

#ifdef CS_EXPERIMENTAL_CODEC
	PBX_CHANNEL_TYPE *bridge;

	if (PBX(getRemoteChannel) (channel, &bridge)) {
		channel->owner->readformat = 0;

		bridge->writeformat = 0;
		ast_channel_make_compatible(channel->owner, bridge);

	} else {
		ast_set_read_format(channel->owner, channel->owner->rawreadformat);
	}
#else
	channel->owner->nativeformats = channel->owner->rawreadformat;
	ast_set_read_format(channel->owner, channel->owner->readformat);
#endif

	return TRUE;
}

static void sccp_wrapper_asterisk16_setCalleridName(const sccp_channel_t * channel, const char *name)
{
	if (channel->owner->cid.cid_name) {
		ast_free(channel->owner->cid.cid_name);
	}
	channel->owner->cid.cid_name = ast_strdup(name);
}

static void sccp_wrapper_asterisk16_setCalleridNumber(const sccp_channel_t * channel, const char *number)
{
	if (channel->owner->cid.cid_num) {
		ast_free(channel->owner->cid.cid_num);
	}
	channel->owner->cid.cid_num = ast_strdup(number);
}

/*
   ANI=Automatic Number Identification => Calling Party
   DNIS/DNID=Dialed Number Identification Service
   RDNIS=Redirected Dialed Number Identification Service
 */
static void sccp_wrapper_asterisk16_setRedirectingParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	// set redirecting party
	if (channel->owner->cid.cid_rdnis) {
		ast_free(channel->owner->cid.cid_rdnis);
	}
	channel->owner->cid.cid_rdnis = ast_strdup(number);

	// set number dialed originaly
	if (channel->owner->cid.cid_dnid) {
		ast_free(channel->owner->cid.cid_dnid);
	}

	if (channel->owner->cid.cid_num) {
		channel->owner->cid.cid_dnid = ast_strdup(channel->owner->cid.cid_num);
	}
}

static void sccp_wrapper_asterisk16_setRedirectedParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "sccp_wrapper_asterisk16_setRedirectedParty Not Implemented\n");
	/*!< \todo set RedirectedParty using ast_callerid */

	/*      if (number) {
	   channel->owner->redirecting.to.number.valid = 1;
	   ast_party_number_free(&channel->owner->redirecting.to.number);
	   channel->owner->redirecting.to.number.str = ast_strdup(number);
	   }

	   if (name) {
	   channel->owner->redirecting.to.name.valid = 1;
	   ast_party_name_free(&channel->owner->redirecting.to.name);
	   channel->owner->redirecting.to.name.str = ast_strdup(name);
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
	if (sched)
		return ast_sched_add(sched, when, callback, data);
	return FALSE;
}

static long sccp_wrapper_asterisk16_sched_when(int id)
{
	if (sched)
		return ast_sched_when(sched, id);
	return FALSE;
}

static int sccp_wrapper_asterisk16_sched_wait(int id)
{
	if (sched)
		return ast_sched_wait(sched);
	return FALSE;
}

static int sccp_wrapper_asterisk16_sched_del(int id)
{
	if (sched)
		return ast_sched_del(sched, id);
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
	if (channel->owner) {											\
		return channel->owner->_field;									\
	}													\
	return empty_channel_##_field;										\
};

#define DECLARE_PBX_CHANNEL_STRSET(_field)									\
static void sccp_wrapper_asterisk_set_channel_##_field(const sccp_channel_t * channel, const char * _field)	\
{ 														\
        if (channel->owner) {											\
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

	if (channel->owner) {
		if (pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKEDID_HELPER)) {
			return pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKEDID_HELPER);
		}
	}
	return emptyLinkedId;
}

static void sccp_wrapper_asterisk_set_channel_linkedid(const sccp_channel_t * channel, const char *new_linkedid)
{
	if (channel->owner) {
		sccp_wrapper_asterisk_set_pbxchannel_linkedid(channel->owner, new_linkedid);
	}
}

static void sccp_wrapper_asterisk_set_channel_name(const sccp_channel_t * channel, const char *new_name)
{
	if (channel->owner) {
		pbx_string_field_set(channel->owner, name, new_name);
	}
}

static void sccp_wrapper_asterisk_set_channel_call_forward(const sccp_channel_t * channel, const char *new_call_forward)
{
	if (channel->owner) {
		pbx_string_field_set(channel->owner, call_forward, new_call_forward);
	}
}

static enum ast_channel_state sccp_wrapper_asterisk_get_channel_state(const sccp_channel_t * channel)
{
	if (channel->owner) {
		return channel->owner->_state;
	}
	return 0;
}

static const struct ast_pbx *sccp_wrapper_asterisk_get_channel_pbx(const sccp_channel_t * channel)
{
	if (channel->owner) {
		return channel->owner->pbx;
	}
	return NULL;
}

static void sccp_wrapper_asterisk_set_channel_tech_pvt(const sccp_channel_t * channel)
{
	if (channel->owner) {
		//              channel->owner->tech_pvt = sccp_channel_retain((sccp_channel_t * )channel);
	}
}

/*!
 * \brief Find Asterisk/PBX channel by linkedid
 *
 * \param ast   pbx channel
 * \param data  linkId as void *
 *
 * \return int
 *
 * \todo I don't understand what this functions returns
 */
static int pbx_find_channel_by_linkedid(PBX_CHANNEL_TYPE * ast, void *data)
{
	if (!data)
		return 0;
	char *linkedId = data;

	if (!ast->masq && sccp_strequals(pbx_builtin_getvar_helper(ast, SCCP_AST_LINKEDID_HELPER), linkedId)) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: peer name: %s, linkId: %s\n", ast->name ? ast->name : "(null)", linkedId);
		//              return !ast->pbx && (!strcasecmp(refLinkId, linkId)) && !ast->masq;
		return 1;
	} else {
		return 0;
	}
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
	if (!data)
		return 0;
	PBX_CHANNEL_TYPE *refAstChannel = data;

	if (ast == refAstChannel) {										// skip own channel
		return 0;
	}
	if (!ast->masq && sccp_strequals(pbx_builtin_getvar_helper(refAstChannel, SCCP_AST_LINKEDID_HELPER), pbx_builtin_getvar_helper(ast, SCCP_AST_LINKEDID_HELPER))) {
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: peer name: %s\n", ast->name ? ast->name : "(null)");
		return 1;
	} else {
		return 0;
	}
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
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	uint8_t instance;

	if (!ast) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No PBX CHANNEL to send text to\n");
		return -1;
	}

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send text to (%s)\n", ast->name);
		return -1;
	}
	if (!(d = sccp_channel_getDevice_retained(c))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send text to (%s)\n", ast->name);
		c = sccp_channel_release(c);
		return -1;
	}

	if (!text || sccp_strlen_zero(text)) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No text to send to %s\n", d->id, ast->name);
		d = sccp_device_release(d);
		c = sccp_channel_release(c);
		return 0;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_displayprompt(d, instance, c->callid, (char *) text, 10);
	d = sccp_device_release(d);
	c = sccp_channel_release(c);
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
	sccp_channel_t *c = NULL;

	sccp_device_t *d = NULL;

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP CHANNEL to send digit to (%s)\n", ast->name);
		return -1;
	}

	if (!(d = sccp_channel_getDevice_retained(c))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: No SCCP DEVICE to send digit to (%s)\n", ast->name);
		return -1;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, (d->dtmfmode) ? "outofband" : "inband");

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		d = sccp_device_release(d);
		return -1;
	}

	sccp_dev_keypadbutton(d, digit, sccp_device_find_index_for_line(d, c->line->name), c->callid);
	d = sccp_device_release(d);
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
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;

	if (!datalen || sccp_strlen_zero(data) || !(!strncmp(data, "http://", 7) || !strncmp(data, "file://", 7) || !strncmp(data, "ftp://", 6))) {
		pbx_log(LOG_NOTICE, "SCCP: Received a non valid URL\n");
		return -1;
	}
	struct ast_frame fr;

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		return -1;
	} else {
#if DEBUG
		if (!(d = c->getDevice_retained(c, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
#else
		if (!(d = c->getDevice_retained(c))) {
#endif
			c = sccp_channel_release(c);
			return -1;
		}

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
		d = sccp_device_release(d);
		c = sccp_channel_release(c);
	}
	return 0;
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
static sccp_BLFState_t sccp_wrapper_asterisk106_getExtensionState(const char *extension, const char *context)
{
	sccp_BLFState_t result = SCCP_BLF_STATUS_UNKNOWN;

	if (sccp_strlen_zero(extension) || sccp_strlen_zero(context)) {
		pbx_log(LOG_ERROR, "SCCP: PBX(getExtensionState): Either extension:'%s' or context:;%s' provided is empty\n", extension, context);
		return result;
	}

	int state = ast_extension_state(NULL, context, extension);

	switch (state) {
		case AST_EXTENSION_REMOVED:
		case AST_EXTENSION_DEACTIVATED:
		case AST_EXTENSION_UNAVAILABLE:
			result = SCCP_BLF_STATUS_UNKNOWN;
			break;
		case AST_EXTENSION_NOT_INUSE:
			result = SCCP_BLF_STATUS_IDLE;
			break;
		case AST_EXTENSION_INUSE:
		case AST_EXTENSION_ONHOLD:
		case AST_EXTENSION_ONHOLD + AST_EXTENSION_INUSE:
		case AST_EXTENSION_BUSY:
			result = SCCP_BLF_STATUS_INUSE;
			break;
		case AST_EXTENSION_RINGING + AST_EXTENSION_INUSE:
		case AST_EXTENSION_RINGING:
			result = SCCP_BLF_STATUS_ALERTING;
			break;
	}
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (getExtensionState) extension: %s@%s, extension_state: '%s (%d)' -> blf state '%d'\n", extension, context, ast_extension_state2str(state), state, result);
	return result;
}

/*!
 * \brief using RTP Glue Engine
 */
#if defined(__cplusplus) || defined(c_plusplus)
struct ast_rtp_protocol sccp_rtp = {
	/* *INDENT-OFF* */
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk16_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk16_get_vrtp_peer,
	.set_rtp_peer = sccp_wrapper_asterisk16_set_rtp_peer,
	.get_codec = sccp_wrapper_asterisk16_getCodec,
	/* *INDENT-ON* */
};
#else
struct ast_rtp_protocol sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk16_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk16_get_vrtp_peer,
	.set_rtp_peer = sccp_wrapper_asterisk16_set_rtp_peer,
	.get_codec = sccp_wrapper_asterisk16_getCodec,
};
#endif

#ifdef HAVE_PBX_MESSAGE_H
#include "asterisk/message.h"
static int sccp_asterisk_message_send(const struct ast_msg *msg, const char *to, const char *from)
{

	char *lineName;
	sccp_line_t *line;
	const char *messageText = ast_msg_get_body(msg);
	int res = -1;

	lineName = (char *) sccp_strdupa(to);
	if (strchr(lineName, '@')) {
		strsep(&lineName, "@");
	} else {
		strsep(&lineName, ":");
	}
	if (sccp_strlen_zero(lineName)) {
		pbx_log(LOG_WARNING, "MESSAGE(to) is invalid for SCCP - '%s'\n", to);
		return -1;
	}

	line = sccp_line_find_byname(lineName, FALSE);
	if (!line) {
		pbx_log(LOG_WARNING, "line '%s' not found\n", lineName);
		return -1;
	}

	/** \todo move this to line implementation */
	sccp_linedevices_t *linedevice;
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

boolean_t sccp_wrapper_asterisk_setLanguage(PBX_CHANNEL_TYPE * pbxChannel, const char *newLanguage)
{

	ast_string_field_set(pbxChannel, language, newLanguage);
	return TRUE;
}

#if defined(__cplusplus) || defined(c_plusplus)
sccp_pbx_cb sccp_pbx = {
	/* *INDENT-OFF* */
	alloc_pbxChannel:		sccp_wrapper_asterisk16_allocPBXChannel,
	set_callstate:			sccp_wrapper_asterisk16_setCallState,
	checkhangup:			sccp_wrapper_asterisk16_checkHangup,
	hangup:				NULL,
	requestHangup:			sccp_wrapper_asterisk_requestHangup,
	forceHangup:                    sccp_wrapper_asterisk_forceHangup,
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
	setChannelTechPVT:		sccp_wrapper_asterisk_set_channel_tech_pvt,

	set_nativeAudioFormats:		sccp_wrapper_asterisk16_setNativeAudioFormats,
	set_nativeVideoFormats:		sccp_wrapper_asterisk16_setNativeVideoFormats,

	getPeerCodecCapabilities:	NULL,
	send_digit:			sccp_wrapper_asterisk16_sendDigit,
	send_digits:			sccp_wrapper_asterisk16_sendDigits,

	sched_add:			sccp_wrapper_asterisk16_sched_add,
	sched_del:			sccp_wrapper_asterisk16_sched_del,
	sched_when:			sccp_wrapper_asterisk16_sched_when,
	sched_wait:			sccp_wrapper_asterisk16_sched_wait,

	/* rtp */
	rtp_getPeer:			sccp_wrapper_asterisk16_rtpGetPeer,
	rtp_getUs:			sccp_wrapper_asterisk16_rtpGetUs,
	rtp_setPeer:			sccp_wrapper_asterisk16_rtp_set_peer,
	rtp_setWriteFormat:		sccp_wrapper_asterisk16_setWriteFormat,
	rtp_setReadFormat:		sccp_wrapper_asterisk16_setReadFormat,
	rtp_destroy:			sccp_wrapper_asterisk16_destroyRTP,
	rtp_stop:			sccp_wrapper_asterisk16_rtp_stop,
	rtp_codec:			NULL,
	rtp_audio_create:		sccp_wrapper_asterisk16_create_audio_rtp,
	rtp_video_create:		sccp_wrapper_asterisk16_create_video_rtp,
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
	get_callerid_presence:		sccp_wrapper_asterisk16_callerid_presence,

	set_callerid_name:		sccp_wrapper_asterisk16_setCalleridName,
	set_callerid_number:		sccp_wrapper_asterisk16_setCalleridNumber,
	set_callerid_ani:		NULL,
	set_callerid_dnid:		NULL,
	
	set_callerid_redirectingParty:	sccp_wrapper_asterisk16_setRedirectingParty,
	set_callerid_redirectedParty:	sccp_wrapper_asterisk16_setRedirectedParty,
	set_callerid_presence:		sccp_wrapper_asterisk16_setCalleridPresence,
	set_connected_line:		sccp_wrapper_asterisk16_updateConnectedLine,
	sendRedirectedUpdate:		sccp_asterisk_sendRedirectedUpdate,

	/* feature section */
	feature_park:			sccp_wrapper_asterisk16_park,
	feature_stopMusicOnHold:	NULL,
	feature_addToDatabase:		sccp_asterisk_addToDatabase,
	feature_getFromDatabase:	sccp_asterisk_getFromDatabase,
	feature_removeFromDatabase:	sccp_asterisk_removeFromDatabase,
	feature_removeTreeFromDatabase:	sccp_asterisk_removeTreeFromDatabase,
	getFeatureExtension:		NULL,
	feature_pickup:			sccp_wrapper_asterisk16_pickupChannel,

	eventSubscribe:			NULL,
	findChannelByCallback:		sccp_wrapper_asterisk16_findChannelWithCallback,

	moh_start:			sccp_asterisk_moh_start,
	moh_stop:			sccp_asterisk_moh_stop,
	queue_control:			sccp_asterisk_queue_control,
	queue_control_data:		sccp_asterisk_queue_control_data,

	allocTempPBXChannel:		sccp_wrapper_asterisk16_allocTempPBXChannel,
	masqueradeHelper:		sccp_wrapper_asterisk16_masqueradeHelper,
	requestForeignChannel:		sccp_wrapper_asterisk16_requestForeignChannel,
	
	set_language:			sccp_wrapper_asterisk_setLanguage,

	getExtensionState:		sccp_wrapper_asterisk106_getExtensionState,
	findPickupChannelByExtenLocked: sccp_wrapper_asterisk16_findPickupChannelByExtenLocked,
	/* *INDENT-ON* */
};

#else

/*!
 * \brief SCCP - PBX Callback Functions 
 * (Decoupling Tight Dependencies on Asterisk Functions)
 */
struct sccp_pbx_cb sccp_pbx = {
	/* *INDENT-OFF* */
  
        /* channel */
	.alloc_pbxChannel 		= sccp_wrapper_asterisk16_allocPBXChannel,
	
	.requestHangup 			= sccp_wrapper_asterisk_requestHangup,
	.forceHangup                    = sccp_wrapper_asterisk_forceHangup,
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
	.setChannelTechPVT		= sccp_wrapper_asterisk_set_channel_tech_pvt,

	.getRemoteChannel		= sccp_asterisk_getRemoteChannel,
	.checkhangup			= sccp_wrapper_asterisk16_checkHangup,
	
	/* digits */
	.send_digits 			= sccp_wrapper_asterisk16_sendDigits,
	.send_digit 			= sccp_wrapper_asterisk16_sendDigit,

	/* schedulers */
	.sched_add 			= sccp_wrapper_asterisk16_sched_add,
	.sched_del 			= sccp_wrapper_asterisk16_sched_del,
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
	.rtp_stop 			= sccp_wrapper_asterisk16_rtp_stop,
	.rtp_audio_create 		= sccp_wrapper_asterisk16_create_audio_rtp,
	.rtp_video_create 		= sccp_wrapper_asterisk16_create_video_rtp,
	.rtp_get_payloadType 		= sccp_wrapper_asterisk16_get_payloadType,
	.rtp_get_sampleRate 		= sccp_wrapper_asterisk16_get_sampleRate,
	.rtp_destroy 			= sccp_wrapper_asterisk16_destroyRTP,
	.rtp_setWriteFormat 		= sccp_wrapper_asterisk16_setWriteFormat,
	.rtp_setReadFormat 		= sccp_wrapper_asterisk16_setReadFormat,
	.rtp_setPeer 			= sccp_wrapper_asterisk16_rtp_set_peer,

	/* callerid */
	.get_callerid_name 		= sccp_wrapper_asterisk16_callerid_name,
	.get_callerid_number 		= sccp_wrapper_asterisk16_callerid_number,
	.get_callerid_ton 		= sccp_wrapper_asterisk16_callerid_ton,
	.get_callerid_ani 		= sccp_wrapper_asterisk16_callerid_ani,
	.get_callerid_subaddr 		= NULL,
	.get_callerid_dnid 		= sccp_wrapper_asterisk16_callerid_dnid,
	.get_callerid_rdnis 		= sccp_wrapper_asterisk16_callerid_rdnis,
	.get_callerid_presence 		= sccp_wrapper_asterisk16_callerid_presence,
	.set_callerid_name 		= sccp_wrapper_asterisk16_setCalleridName,	//! \todo implement callback
	.set_callerid_number 		= sccp_wrapper_asterisk16_setCalleridNumber,	//! \todo implement callback
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_wrapper_asterisk16_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_wrapper_asterisk16_setRedirectedParty,
	.set_callerid_presence 		= sccp_wrapper_asterisk16_setCalleridPresence,
	.set_connected_line		= sccp_wrapper_asterisk16_updateConnectedLine,
	.sendRedirectedUpdate		= sccp_asterisk_sendRedirectedUpdate,
	
	/* database */
	.feature_addToDatabase 		= sccp_asterisk_addToDatabase,
	.feature_getFromDatabase 	= sccp_asterisk_getFromDatabase,
	.feature_removeFromDatabase     = sccp_asterisk_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_asterisk_removeTreeFromDatabase,
	
	
	.feature_park			= sccp_wrapper_asterisk16_park,
	.feature_pickup			= sccp_wrapper_asterisk16_pickupChannel,
	
	.findChannelByCallback		= sccp_wrapper_asterisk16_findChannelWithCallback,

	.moh_start			= sccp_asterisk_moh_start,
	.moh_stop			= sccp_asterisk_moh_stop,
	.queue_control			= sccp_asterisk_queue_control,
	.queue_control_data		= sccp_asterisk_queue_control_data,
	
	.allocTempPBXChannel		= sccp_wrapper_asterisk16_allocTempPBXChannel,
	.masqueradeHelper		= sccp_wrapper_asterisk16_masqueradeHelper,
	.requestForeignChannel		= sccp_wrapper_asterisk16_requestForeignChannel,
	
	.set_language			= sccp_wrapper_asterisk_setLanguage,

	.getExtensionState		= sccp_wrapper_asterisk106_getExtensionState,
	.findPickupChannelByExtenLocked	= sccp_wrapper_asterisk16_findPickupChannelByExtenLocked,
	/* *INDENT-ON* */
};
#endif

#if defined(__cplusplus) || defined(c_plusplus)
static ast_module_load_result load_module(void)
#else
static int load_module(void)
#endif
{

	boolean_t res;

	/* check for existance of chan_skinny */
	if (ast_module_check("chan_skinny.so")) {
		pbx_log(LOG_ERROR, "Chan_skinny is loaded. Please check modules.conf and remove chan_skinny before loading chan_sccp.\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	sched = sched_context_create();
	if (!sched) {
		pbx_log(LOG_WARNING, "Unable to create schedule context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}

	/* make globals */
	res = sccp_prePBXLoad();
	if (!res) {
		return AST_MODULE_LOAD_DECLINE;
	}

	io = io_context_create();
	if (!io) {
		pbx_log(LOG_WARNING, "Unable to create I/O context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}
	//! \todo how can we handle this in a pbx independent way?
	if (!load_config()) {
		if (ast_channel_register(&sccp_tech)) {
			pbx_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			return AST_MODULE_LOAD_FAILURE;
		}
	}
#ifdef HAVE_PBX_MESSAGE_H
	if (ast_msg_tech_register(&sccp_msg_tech)) {
		/* LOAD_FAILURE stops Asterisk, so cleanup is a moot point. */
		pbx_log(LOG_WARNING, "Unable to register message interface\n");
	}
#endif

	ast_rtp_proto_register(&sccp_rtp);
	sccp_register_management();
	sccp_register_cli();
	sccp_register_dialplan_functions();
	/* And start the monitor for the first time */
	sccp_restart_monitor();
	sccp_postPBX_load();
	return AST_MODULE_LOAD_SUCCESS;
}

int sccp_restart_monitor()
{
	/* If we're supposed to be stopped -- stay stopped */
	if (GLOB(monitor_thread) == AST_PTHREADT_STOP)
		return 0;
	ast_mutex_lock(&GLOB(monitor_lock));
	if (GLOB(monitor_thread) == pthread_self()) {
		ast_mutex_unlock(&GLOB(monitor_lock));
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Cannot kill myself\n");
		return -1;
	}
	if (GLOB(monitor_thread) != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(GLOB(monitor_thread), SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&GLOB(monitor_thread), NULL, sccp_do_monitor, NULL) < 0) {
			ast_mutex_unlock(&GLOB(monitor_lock));
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&GLOB(monitor_lock));
	return 0;
}

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
#ifndef HAVE_LIBGC
		pthread_join(GLOB(monitor_thread), NULL);
#endif
	}
	GLOB(monitor_thread) = AST_PTHREADT_STOP;

	if (io) {
		io_context_destroy(io);
		io = NULL;
	}

        while (SCCP_REF_DESTROYED != sccp_refcount_isRunning()) {
	        usleep(SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT);						// give enough time for all schedules to end and refcounted object to be cleanup completely
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
	sccp_globals_unlock(monitor_lock);
	sccp_mutex_destroy(&GLOB(monitor_lock));

	sccp_free(sccp_globals);
	pbx_log(LOG_NOTICE, "Running Cleanup\n");
#ifdef HAVE_LIBGC
	//      sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Collect a little:%d\n",GC_collect_a_little());
	//      CHECK_LEAKS();
	//      GC_gcollect();
#endif
	pbx_log(LOG_NOTICE, "Module chan_sccp unloaded\n");
	return 0;
}

#if ASTERSIK_VERSION_NUMBER > 10601
static int module_reload(void)
{
	sccp_reload();
	return 0;
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
	"Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "', NULL)",
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
AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, 
        "Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "', NULL)",
        .load = load_module,
        .unload = unload_module,
);
#else
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, 
        "Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "')"
);
#endif
#endif

PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data)
{
	PBX_CHANNEL_TYPE *remotePeer;

	remotePeer = ast_channel_search_locked(found_cb, data);
	return remotePeer;
}


/* sccp_wrapper_asterisk16_findPickupChannelByExtenLocked helper functions*/

/* Helper function that determines whether a channel is capable of being picked up */
static int can_pickup(struct ast_channel *chan)
{
        if (!chan->pbx && !chan->masq &&
                !ast_test_flag(chan, AST_FLAG_ZOMBIE) &&
                (chan->_state == AST_STATE_RINGING ||
                 chan->_state == AST_STATE_RING ||
                 chan->_state == AST_STATE_DOWN)) {
                return 1;
        }
        return 0;
}

struct pickup_criteria {
        const char *exten;
        const char *context;
        struct ast_channel *chan;
};
                        
static int sccp_find_pbxchannel_by_exten(PBX_CHANNEL_TYPE *c, void *data)
{
        struct pickup_criteria *info = data;

        return (!strcasecmp(c->macroexten, info->exten) || !strcasecmp(c->exten, info->exten)) &&
                !strcasecmp(c->dialcontext, info->context) &&
                (info->chan != c) && can_pickup(c);
}
/* end sccp_wrapper_asterisk16_findPickupChannelByExtenLocked helper functions*/

PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE *chan, const char *exten, const char *context)
{
        struct ast_channel *target = NULL;		/*!< Potential pickup target */
        struct pickup_criteria search = {
                .exten = exten,
                .context = context,
                .chan = chan,
        };

        target = ast_channel_search_locked(sccp_find_pbxchannel_by_exten, &search);
	return target;
}
