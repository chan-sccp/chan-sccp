
/*!
 * \file        ast108.c
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
#include "ast108.h"
#include <signal.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
#include <asterisk/sched.h>
#include <asterisk/netsock2.h>
#if HAVE_PBX_CEL_H
#include <asterisk/cel.h>
#endif

#define new avoid_cxx_new_keyword
#include <asterisk/rtp_engine.h>
#undef new

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#undef HAVE_PBX_MESSAGE_H
struct sched_context *sched = 0;
struct io_context *io = 0;

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_request(const char *type, format_t format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause);
static int sccp_wrapper_asterisk18_call(PBX_CHANNEL_TYPE * chan, char *addr, int timeout);
static int sccp_wrapper_asterisk18_answer(PBX_CHANNEL_TYPE * chan);
static PBX_FRAME_TYPE *sccp_wrapper_asterisk18_rtp_read(PBX_CHANNEL_TYPE * ast);
static int sccp_wrapper_asterisk18_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame);
static int sccp_wrapper_asterisk18_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen);
static int sccp_wrapper_asterisk18_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan);
static enum ast_bridge_result sccp_wrapper_asterisk18_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms);
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE * ast, const char *text);
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE * ast, char digit);
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE * ast, char digit, unsigned int duration);
static int sccp_pbx_sendHTML(PBX_CHANNEL_TYPE * ast, int subclass, const char *data, int datalen);
int sccp_wrapper_asterisk18_hangup(PBX_CHANNEL_TYPE * ast_channel);
boolean_t sccp_wrapper_asterisk18_allocPBXChannel(sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel, const char *linkedId);
static boolean_t sccp_wrapper_asterisk18_setWriteFormat(const sccp_channel_t * channel, skinny_codec_t codec);

//static int sccp_wrapper_asterisk18_setOption(PBX_CHANNEL_TYPE *ast, int option, void *data, int datalen);
int sccp_asterisk_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control);
int sccp_asterisk_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen);
static int sccp_wrapper_asterisk18_devicestate(void *data);
PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE *chan, const char *exten, const char *context);

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
	requester:		sccp_wrapper_asterisk18_request,
	devicestate:		sccp_wrapper_asterisk18_devicestate,
	send_digit_begin:	sccp_wrapper_recvdigit_begin,
	send_digit_end:		sccp_wrapper_recvdigit_end,
	call:			sccp_wrapper_asterisk18_call,
	hangup:			sccp_wrapper_asterisk18_hangup,
	answer:			sccp_wrapper_asterisk18_answer,
	read:			sccp_wrapper_asterisk18_rtp_read,
	write:			sccp_wrapper_asterisk18_rtp_write,
	send_text:		sccp_pbx_sendtext,
	send_image:		NULL,
	send_html:		sccp_pbx_sendHTML,
	exception:		NULL,
	bridge:			sccp_wrapper_asterisk18_rtpBridge,
	early_bridge:		NULL,
	indicate:		sccp_wrapper_asterisk18_indicate,
	fixup:			sccp_wrapper_asterisk18_fixup,
	setoption:		NULL,
	queryoption:		NULL,
	transfer:		NULL,
	write_video:		sccp_wrapper_asterisk18_rtp_write,
	write_text:		NULL,
	bridged_channel:	NULL,
	func_channel_read:	sccp_wrapper_asterisk_channel_read,
	func_channel_write:	sccp_asterisk_pbx_fktChannelWrite,
	get_base_channel:	NULL,
	set_base_channel:	NULL,
//	setoption:		sccp_wrapper_asterisk18_setOption,
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
	.requester 		= sccp_wrapper_asterisk18_request,
	.devicestate 		= sccp_wrapper_asterisk18_devicestate,
	.call 			= sccp_wrapper_asterisk18_call,
	.hangup 		= sccp_wrapper_asterisk18_hangup,
	.answer 		= sccp_wrapper_asterisk18_answer,
	.read 			= sccp_wrapper_asterisk18_rtp_read,
	.write 			= sccp_wrapper_asterisk18_rtp_write,
	.write_video 		= sccp_wrapper_asterisk18_rtp_write,
	.indicate 		= sccp_wrapper_asterisk18_indicate,
	.fixup 			= sccp_wrapper_asterisk18_fixup,
	.transfer 		= sccp_pbx_transfer,
	.bridge 		= sccp_wrapper_asterisk18_rtpBridge,
	//.early_bridge         = ast_rtp_early_bridge,
	//.bridged_channel      =

	.send_text 		= sccp_pbx_sendtext,
	.send_html 		= sccp_pbx_sendHTML,
	//.send_image           =

	.func_channel_read 	= sccp_wrapper_asterisk_channel_read,
	.func_channel_write 	= sccp_asterisk_pbx_fktChannelWrite,

	.send_digit_begin 	= sccp_wrapper_recvdigit_begin,
	.send_digit_end 	= sccp_wrapper_recvdigit_end,

	//.write_text           = 
	//.write_video          =
	//.cc_callback          =                                              // ccss, new >1.6.0
	//.exception            =                                              // new >1.6.0
//	.setoption            = sccp_wrapper_asterisk18_setOption,
	//.queryoption          =                                              // new >1.6.0
	//.get_pvt_uniqueid     = sccp_pbx_get_callid,                         // new >1.6.0
	//.get_base_channel     =
	//.set_base_channel     =
	/* *INDENT-ON* */
};
#endif

static int sccp_wrapper_asterisk18_devicestate(void *data)
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
		case SCCP_CHANNELSTATE_ZOMBIE:
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			res = AST_DEVICE_UNAVAILABLE;
			break;

		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
		case SCCP_CHANNELSTATE_PROGRESS:
		case SCCP_CHANNELSTATE_CALLWAITING:
			res = AST_DEVICE_RINGINUSE;
			break;
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

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_asterisk_devicestate) PBX requests state for '%s' - state %s\n", (char *) lineName, ast_devstate2str(res));
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

static boolean_t sccp_wrapper_asterisk18_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec);

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
static PBX_FRAME_TYPE *sccp_wrapper_asterisk18_rtp_read(PBX_CHANNEL_TYPE * ast)
{
	sccp_channel_t *c = NULL;
	PBX_FRAME_TYPE *frame = NULL;

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {									// not following the refcount rules... channel is already retained
		return &ast_null_frame;
	}

	if (!c->rtp.audio.rtp) {
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
			frame = &ast_null_frame;
			break;
	}

	if (frame->frametype == AST_FRAME_VOICE) {
#ifdef CS_SCCP_CONFERENCE
		if (c->conference && (AST_FORMAT_SLINEAR != ast->readformat)) {
			ast_set_read_format(ast, AST_FORMAT_SLINEAR);
		} else 
#endif
		{	
			if (!(frame->subclass.codec & (ast->rawreadformat & AST_FORMAT_AUDIO_MASK))) {
				//sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Channel %s changed format from %s(%d) to %s(%d)\n", DEV_ID_LOG(c->device), ast->name, pbx_getformatname(ast->nativeformats), ast->nativeformats, pbx_getformatname(frame->subclass), frame->subclass);
#ifndef CS_EXPERIMENTAL_CODEC
				sccp_wrapper_asterisk18_setReadFormat(c, c->rtp.audio.readFormat);
#endif
			}
			if (frame->subclass.codec != (ast->nativeformats & AST_FORMAT_AUDIO_MASK)) {
				if (!(frame->subclass.codec & skinny_codecs2pbx_codecs(c->capabilities.audio))) {
					ast_debug(1, "Bogus frame of format '%s' received from '%s'!\n", ast_getformatname(frame->subclass.codec), ast->name);
					return &ast_null_frame;
				}
				ast_debug(1, "SCCP: format changed to %s\n", ast_getformatname(frame->subclass.codec));
				ast->nativeformats = (ast->nativeformats & (AST_FORMAT_VIDEO_MASK | AST_FORMAT_TEXT_MASK)) | frame->subclass.codec;
				ast_set_read_format(ast, ast->readformat);
				ast_set_write_format(ast, ast->writeformat);
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
 *
 * \todo I don't understand what this functions returns
 */
static int pbx_find_channel_by_linkid(PBX_CHANNEL_TYPE * ast, const void *data)
{
	const char *linkId = (char *) data;

	if (!data)
		return 0;

	return !ast->pbx && ast->linkedid && (!strcasecmp(ast->linkedid, linkId)) && !ast->masq;
}

/*!
 * \brief Update Connected Line
 * \param channel Asterisk Channel as ast_channel
 * \param data Asterisk Data
 * \param datalen Asterisk Data Length
 */
static void sccp_wrapper_asterisk18_connectedline(sccp_channel_t * channel, const void *data, size_t datalen)
{
	PBX_CHANNEL_TYPE *ast = channel->owner;

	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Got connected line update\nconnected.id.number=%s\nconnected.id.name=%s  reason=%d\n", ast->name, ast->connected.id.number.str ? ast->connected.id.number.str : "(nil)", ast->connected.id.name.str ? ast->connected.id.name.str : "(nil)", ast->connected.source);

	/* set the original calling/called party if the reason is a transfer */
	if (ast->connected.source == AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER || ast->connected.source == AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER_ALERTING) {
		if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {

			sccp_copy_string(channel->callInfo.originalCallingPartyNumber, channel->callInfo.callingPartyNumber, sizeof(channel->callInfo.originalCallingPartyNumber));
			sccp_copy_string(channel->callInfo.originalCallingPartyName, channel->callInfo.callingPartyName, sizeof(channel->callInfo.originalCallingPartyName));
		} else {
			sccp_copy_string(channel->callInfo.originalCalledPartyNumber, channel->callInfo.calledPartyNumber, sizeof(channel->callInfo.originalCalledPartyNumber));
			sccp_copy_string(channel->callInfo.originalCalledPartyName, channel->callInfo.calledPartyName, sizeof(channel->callInfo.originalCalledPartyName));
		}
	}

	if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {
		if (ast->connected.id.number.str && !sccp_strlen_zero(ast->connected.id.number.str))
			sccp_copy_string(channel->callInfo.callingPartyNumber, ast->connected.id.number.str, sizeof(channel->callInfo.callingPartyNumber));

		if (ast->connected.id.name.str && !sccp_strlen_zero(ast->connected.id.name.str))
			sccp_copy_string(channel->callInfo.callingPartyName, ast->connected.id.name.str, sizeof(channel->callInfo.callingPartyName));
	} else {
		if (ast->connected.id.number.str && !sccp_strlen_zero(ast->connected.id.number.str))
			sccp_copy_string(channel->callInfo.calledPartyNumber, ast->connected.id.number.str, sizeof(channel->callInfo.calledPartyNumber));

		if (ast->connected.id.name.str && !sccp_strlen_zero(ast->connected.id.name.str))
			sccp_copy_string(channel->callInfo.calledPartyName, ast->connected.id.name.str, sizeof(channel->callInfo.calledPartyName));
	}

	sccp_channel_send_callinfo2(channel);
}

static int sccp_wrapper_asterisk18_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	int res = 0;

	if (!(c = get_sccp_channel_from_pbx_channel(ast)))
		return -1;

	if (!(d = sccp_channel_getDevice_retained(c))) {

		switch (ind) {
			case AST_CONTROL_CONNECTED_LINE:
				sccp_wrapper_asterisk18_connectedline(c, data, datalen);

				res = 0;
				break;

			case AST_CONTROL_REDIRECTING:
				sccp_asterisk_redirectedUpdate(c, data, datalen);

				res = 0;
				break;
			default:
				res = -1;
				break;
		}

		c = sccp_channel_release(c);
		return res;
	}

	if (c->state == SCCP_CHANNELSTATE_DOWN) {
		c = sccp_channel_release(c);
		d = sccp_device_release(d);
		return -1;
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' condition on channel %s\n", DEV_ID_LOG(d), ind, ast->name);

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = (((c->rtp.audio.readState != SCCP_RTP_STATUS_INACTIVE) || (d && d->earlyrtp)) ? -1 : 0);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: readState: %d\n", DEV_ID_LOG(d), c->rtp.audio.readState);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: res: %d\n", DEV_ID_LOG(d), res);
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: rtp?: %s\n", DEV_ID_LOG(d), (c->rtp.audio.rtp) ? "yes" : "no");

	switch (ind) {
		case AST_CONTROL_RINGING:
			if (SKINNY_CALLTYPE_OUTBOUND == c->calltype) {
				// Allow signalling of RINGOUT only on outbound calls.
				// Otherwise, there are some issues with late arrival of ringing
				// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
				sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGOUT);

				struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

				((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

				/*! \todo handle multiple remotePeers i.e. DIAL(SCCP/400&SIP/300), find smallest common codecs, what order to use ? */
				PBX_CHANNEL_TYPE *remotePeer;

				for (; (remotePeer = ast_channel_iterator_next(iterator)); ast_channel_unref(remotePeer)) {
					if (pbx_find_channel_by_linkid(remotePeer, (void *) ast->linkedid)) {
						char buf[512];
						sccp_channel_t *remoteSccpChannel = get_sccp_channel_from_pbx_channel(remotePeer);

						if (remoteSccpChannel) {
							sccp_multiple_codecs2str(buf, sizeof(buf) - 1, remoteSccpChannel->preferences.audio, ARRAY_LEN(remoteSccpChannel->preferences.audio));
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
							remoteSccpChannel = sccp_channel_release(remoteSccpChannel);
						} else {
							sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote nativeformats: %s\n", pbx_getformatname_multiple(buf, sizeof(buf) - 1, remotePeer->nativeformats));
							get_skinnyFormats(remotePeer->nativeformats, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
						}

						sccp_multiple_codecs2str(buf, sizeof(buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
						sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_4 "remote caps: %s\n", buf);
						ast_channel_unref(remotePeer);
						break;
					}

				}
				ast_channel_iterator_destroy(iterator);
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
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source CHANGE request\n");
			if (c->rtp.audio.rtp)
				ast_rtp_instance_change_source(c->rtp.audio.rtp);

			res = 0;
			break;

		case AST_CONTROL_SRCUPDATE:
			/* Source media has changed. */
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

			if (c->rtp.audio.rtp)
				ast_rtp_instance_update_source(c->rtp.audio.rtp);

			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP:c->state: %d\n", c->state);
			if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
				sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: force CONNECT (AST_CONTROL_SRCUPDATE)\n");
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
			}
			res = 0;
			break;

			/* when the bridged channel hold/unhold the call we are notified here */
		case AST_CONTROL_HOLD:
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: HOLD request\n");
			sccp_asterisk_moh_start(ast, (const char *) data, c->musicclass);
			if (c->rtp.audio.rtp) {
				ast_rtp_instance_update_source(c->rtp.audio.rtp);
			}

			res = 0;
			break;
		case AST_CONTROL_UNHOLD:
			sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: UNHOLD request\n");
			sccp_asterisk_moh_stop(ast);
			if (c->rtp.audio.rtp) {
				ast_rtp_instance_update_source(c->rtp.audio.rtp);
			}
			res = 0;
			break;

		case AST_CONTROL_CONNECTED_LINE:
			sccp_wrapper_asterisk18_connectedline(c, data, datalen);
			sccp_indicate(d, c, c->state);

			res = 0;
			break;

		case AST_CONTROL_REDIRECTING:
			sccp_asterisk_redirectedUpdate(c, data, datalen);
			sccp_indicate(d, c, c->state);

			res = 0;
			break;

		case AST_CONTROL_VIDUPDATE:									/* Request a video frame update */
#ifdef CS_SCCP_VIDEO
			if (c->rtp.video.rtp && d && sccp_device_isVideoSupported(d)) {
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
#ifdef CS_EXPERIMENTAL
		case AST_CONTROL_UPDATE_RTP_PEER:								/* Absorb this since it is handled by the bridge */
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
 * \note // not following the refcount rules... channel is already retained
 */
static int sccp_wrapper_asterisk18_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE * frame)
{
	int res = 0;
	sccp_channel_t *c = NULL;

	//      if (!(c = get_sccp_channel_from_pbx_channel(ast)))      // not following the refcount rules... channel is already retained
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {									// not following the refcount rules... channel is already retained
		return -1;
	}

	if (c) {
		switch (frame->frametype) {
			case AST_FRAME_VOICE:
				// checking for samples to transmit
				if (!frame->samples) {
					if (strcasecmp(frame->src, "ast_prod")) {
						pbx_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", (char *) c->currentDeviceId, (int) frame->frametype);
					} else {
						sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", (char *) c->currentDeviceId, ast->name);
					}
				}
				//CODEC_TRANSLATION_FIX_AFTER_MOH
				if (frame->subclass.codec != ast->rawwriteformat) {
					/* asterisk channel.c:4911 temporary fix */
					if ((!(frame->subclass.codec & ast->nativeformats)) && (ast->writeformat != frame->subclass.codec)) {
						char s1[512], s2[512], s3[512];

						sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_2 "%s: Asked to transmit frame type %s, while native formats is %s read/write = %s/%s\n -> Forcing writeformat to %s to fix this issue.\n",
									  c->currentDeviceId, ast_getformatname(frame->subclass.codec), ast_getformatname_multiple(s1, sizeof(s1), ast->nativeformats & AST_FORMAT_AUDIO_MASK), ast_getformatname_multiple(s2, sizeof(s2), ast->readformat), ast_getformatname_multiple(s3, sizeof(s3), ast->writeformat), ast_getformatname(frame->subclass.codec));
						ast_set_write_format(ast, frame->subclass.codec);
					}

					frame = (ast->writetrans) ? ast_translate(ast->writetrans, frame, 0) : frame;
				}
				//CODEC_TRANSLATION_FIX_AFTER_MOH

				if (c->rtp.audio.rtp && frame) {
					res = ast_rtp_instance_write(c->rtp.audio.rtp, frame);
				}
				break;
			case AST_FRAME_IMAGE:
			case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
				if (c->rtp.video.writeState == SCCP_RTP_STATUS_INACTIVE && c->rtp.video.rtp && c->state != SCCP_CHANNELSTATE_HOLD
				    //      && (c->device->capability & frame->subclass)
				    ) {
					int codec = pbx_codec2skinny_codec((frame->subclass.codec & AST_FORMAT_VIDEO_MASK));

					sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: got video frame %d\n", (char *) c->currentDeviceId, codec);
					if (0 != codec) {
						c->rtp.video.writeFormat = codec;
						sccp_channel_openMultiMediaChannel(c);
					}
				}

				if (c->rtp.video.rtp && frame && (c->rtp.video.writeState & SCCP_RTP_STATUS_ACTIVE) != 0) {
					res = ast_rtp_instance_write(c->rtp.video.rtp, frame);
				}
#endif
				break;
			case AST_FRAME_TEXT:
			case AST_FRAME_MODEM:
			default:
				pbx_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %s\n", (char *) c->currentDeviceId, frame->frametype, ast->name);
				break;
		}
	}
	//      c = sccp_channel_release(c);
	return res;
}

static int sccp_wrapper_asterisk18_sendDigits(const sccp_channel_t * channel, const char *digits)
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
		f.subclass.integer = digits[i];
		//              f.samples = SCCP_MIN_DTMF_DURATION * 8;
		f.len = SCCP_MIN_DTMF_DURATION;
		f.src = "SEND DIGIT";
		ast_queue_frame(pbx_channel, &f);
	}
	return 1;
}

static int sccp_wrapper_asterisk18_sendDigit(const sccp_channel_t * channel, const char digit)
{
	char digits[3] = "\0\0";

	digits[0] = digit;

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: got a single digit '%c' -> '%s'\n", channel->currentDeviceId, digit, digits);
	return sccp_wrapper_asterisk18_sendDigits(channel, digits);
}

static void sccp_wrapper_asterisk18_setCalleridPresence(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

	if (CALLERID_PRESENCE_FORBIDDEN == channel->callInfo.presentation) {
		pbx_channel->caller.id.name.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
		pbx_channel->caller.id.number.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
	}
}

static int sccp_wrapper_asterisk18_setNativeAudioFormats(const sccp_channel_t * channel, skinny_codec_t codec[], int length)
{
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
	return 1;
}

static int sccp_wrapper_asterisk18_setNativeVideoFormats(const sccp_channel_t * channel, uint32_t formats)
{
	return 1;
}

boolean_t sccp_wrapper_asterisk18_allocPBXChannel(sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel, const char *linkedId)
{
	sccp_line_t *line = NULL;

	(*pbx_channel) = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, linkedId, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);

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
	ast_module_ref(ast_module_info->self);
	channel->owner = ast_channel_ref((*pbx_channel));

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk18_masqueradeHelper(PBX_CHANNEL_TYPE * pbxChannel, PBX_CHANNEL_TYPE * pbxTmpChannel)
{
	const char *context;
	const char *exten;
	int priority;

	pbx_moh_stop(pbxChannel);
	ast_channel_lock_both(pbxChannel, pbxTmpChannel);
	context = ast_strdupa(pbxChannel->context);
	exten = ast_strdupa(pbxChannel->exten);
	priority = pbxChannel->priority;
	pbx_channel_unlock(pbxChannel);
	pbx_channel_unlock(pbxTmpChannel);

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

static boolean_t sccp_wrapper_asterisk18_allocTempPBXChannel(PBX_CHANNEL_TYPE * pbxSrcChannel, PBX_CHANNEL_TYPE ** pbxDstChannel)
{
	if (!pbxSrcChannel) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_TempPBXChannel) no pbx channel provided\n");
		return FALSE;
	}
	(*pbxDstChannel) = ast_channel_alloc(0, pbxSrcChannel->_state, 0, 0, pbxSrcChannel->accountcode, pbxSrcChannel->exten, pbxSrcChannel->context, pbxSrcChannel->linkedid, pbxSrcChannel->amaflags, "TMP/%s", pbxSrcChannel->name);
	if ((*pbxDstChannel) == NULL) {
		pbx_log(LOG_ERROR, "SCCP: (alloc_TempPBXChannel) create pbx channel failed\n");
		return FALSE;
	}

	(*pbxDstChannel)->writeformat = pbxSrcChannel->writeformat;
	(*pbxDstChannel)->readformat = pbxSrcChannel->readformat;
	return TRUE;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_requestForeignChannel(const char *type, pbx_format_type format, const PBX_CHANNEL_TYPE * requestor, void *data)
{
	PBX_CHANNEL_TYPE *chan;
	int cause;

	if (!(chan = ast_request(type, format, requestor, data, &cause))) {
		pbx_log(LOG_ERROR, "SCCP: Requested channel could not be created, cause: %d", cause);
		return NULL;
	}
	return chan;
}

int sccp_wrapper_asterisk18_hangup(PBX_CHANNEL_TYPE * ast_channel)
{
	sccp_channel_t *c;
	PBX_CHANNEL_TYPE *channel_owner = NULL;
	int res = -1;

	if ((c = get_sccp_channel_from_pbx_channel(ast_channel))) {
		channel_owner = c->owner;
		if (ast_test_flag(ast_channel, AST_FLAG_ANSWERED_ELSEWHERE) || ast_channel->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere\n");
			c->answered_elsewhere = TRUE;
		}
		res = sccp_pbx_hangup(c);
		c->owner = NULL;
		if (0 == res) {
			sccp_channel_release(c);								//this only releases the get_sccp_channel_from_pbx_channel
		}
	}													//after this moment c might have gone already

	ast_channel->tech_pvt = NULL;
	c = c ? sccp_channel_release(c) : NULL;
	if (channel_owner) {
		channel_owner = ast_channel_unref(channel_owner);
	}
	ast_module_unref(ast_module_info->self);
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
static void *sccp_wrapper_asterisk18_park_thread(void *data)
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

	res = ast_park_call(arg->bridgedChannel, arg->hostChannel, 0, "", &ext);

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
static sccp_parkresult_t sccp_wrapper_asterisk18_park(const sccp_channel_t * hostChannel)
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

	pbx_bridgedChannelClone = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, hostChannel->owner->accountcode, bridgedChannel->exten, bridgedChannel->context, bridgedChannel->linkedid, bridgedChannel->amaflags, "Parking/%s", bridgedChannel->name);

	pbx_hostChannelClone = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, hostChannel->owner->accountcode, hostChannel->owner->exten, hostChannel->owner->context, hostChannel->owner->linkedid, hostChannel->owner->amaflags, "SCCP/%s", hostChannel->owner->name);

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
		if (!ast_pthread_create_detached_background(&th, NULL, sccp_wrapper_asterisk18_park_thread, arg)) {
			return PARK_RESULT_SUCCESS;
		}
		arg->device = sccp_device_release(arg->device);
	}
	sccp_free(arg);
	return PARK_RESULT_FAIL;

}

static boolean_t sccp_wrapper_asterisk18_getFeatureExtension(const sccp_channel_t * channel, char **extension)
{
	struct ast_call_feature *feat;

	ast_rdlock_call_features();
	feat = ast_find_call_feature("automon");

	if (feat) {
		*extension = strdup(feat->exten);
	}
	ast_unlock_call_features();

	return feat ? TRUE : FALSE;
}

#if !CS_AST_DO_PICKUP
static const struct ast_datastore_info pickup_active = {
	.type = "pickup-active",
};

static int ast_do_pickup(PBX_CHANNEL_TYPE * chan, PBX_CHANNEL_TYPE * target)
{
	struct ast_party_connected_line connected_caller;
	PBX_CHANNEL_TYPE *chans[2] = { chan, target };
	struct ast_datastore *ds_pickup;
	const char *chan_name;											/*!< A masquerade changes channel names. */
	const char *target_name;										/*!< A masquerade changes channel names. */
	int res = -1;

	target_name = sccp_strdupa(target->name);
	ast_debug(1, "Call pickup on '%s' by '%s'\n", target_name, chan->name);

	/* Mark the target to block any call pickup race. */
	ds_pickup = ast_datastore_alloc(&pickup_active, NULL);
	if (!ds_pickup) {
		pbx_log(LOG_WARNING, "Unable to create channel datastore on '%s' for call pickup\n", target_name);
		return -1;
	}
	ast_channel_datastore_add(target, ds_pickup);

	ast_party_connected_line_init(&connected_caller);
	ast_party_connected_line_copy(&connected_caller, &target->connected);
	ast_channel_unlock(target);										/* The pickup race is avoided so we do not need the lock anymore. */
	connected_caller.source = AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER;
	if (ast_channel_connected_line_macro(NULL, chan, &connected_caller, 0, 0)) {
		ast_channel_update_connected_line(chan, &connected_caller, NULL);
	}
	ast_party_connected_line_free(&connected_caller);

	ast_channel_lock(chan);
	chan_name = sccp_strdupa(chan->name);
	ast_connected_line_copy_from_caller(&connected_caller, &chan->caller);
	ast_channel_unlock(chan);
	connected_caller.source = AST_CONNECTED_LINE_UPDATE_SOURCE_ANSWER;
	ast_channel_queue_connected_line_update(chan, &connected_caller, NULL);
	ast_party_connected_line_free(&connected_caller);

	//      ast_cel_report_event(target, AST_CEL_PICKUP, NULL, NULL, chan);

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

	/* If you want UniqueIDs, set channelvars in manager.conf to CHANNEL(uniqueid) */
	ast_manager_event_multichan(EVENT_FLAG_CALL, "Pickup", 2, chans, "Channel: %s\r\n" "TargetChannel: %s\r\n", chan_name, target_name);

	/* Do the masquerade manually to make sure that it is completed. */
	ast_do_masquerade(target);
	res = 0;

pickup_failed:
	ast_channel_lock(target);
	if (!ast_channel_datastore_remove(target, ds_pickup)) {
		ast_datastore_free(ds_pickup);
	}

	return res;
}
#endif

/*!
 * \brief Pickup asterisk channel target using chan
 * 
 * \param chan initial channel that request the parking           
 * \param target Channel t park to
 */
static boolean_t sccp_wrapper_asterisk18_pickupChannel(const sccp_channel_t * chan, PBX_CHANNEL_TYPE * target)
{
	boolean_t result = FALSE;
	PBX_CHANNEL_TYPE *ref;

	ref = ast_channel_ref(chan->owner);
	result = ast_do_pickup(chan->owner, target) ? FALSE : TRUE;
	if (result) {
		((sccp_channel_t *) chan)->owner = ast_channel_ref(target);
		ast_hangup(ref);
	}
	ref = ast_channel_unref(chan->owner);

	return result;
}

static uint8_t sccp_wrapper_asterisk18_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	uint32_t astCodec;
	int payload;

	astCodec = skinny_codec2pbx_codec(codec);
	payload = ast_rtp_codecs_payload_code(ast_rtp_instance_get_codecs(rtp->rtp), 1, astCodec);

	return payload;
}

static int sccp_wrapper_asterisk18_get_sampleRate(skinny_codec_t codec)
{
	uint32_t astCodec;

	astCodec = skinny_codec2pbx_codec(codec);
	return ast_rtp_lookup_sample_rate2(1, astCodec);
}

static sccp_extension_status_t sccp_wrapper_asterisk18_extensionStatus(const sccp_channel_t * channel)
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

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_request(const char *type, format_t format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause)
{
	PBX_CHANNEL_TYPE *result_ast_channel = NULL;
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
	lineName = strdupa((const char *) data);
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
	const char *alert_info = NULL;

	if (requestor) {
		alert_info = pbx_builtin_getvar_helper((PBX_CHANNEL_TYPE *) requestor, "_ALERT_INFO");
		if (!alert_info) {
			alert_info = pbx_builtin_getvar_helper((PBX_CHANNEL_TYPE *) requestor, "__ALERT_INFO");
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
	if (requestor) {
		sccp_channel_t *remoteSccpChannel = get_sccp_channel_from_pbx_channel(requestor);

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
			remoteSccpChannel = sccp_channel_release(remoteSccpChannel);
		} else {
			get_skinnyFormats(requestor->nativeformats & AST_FORMAT_AUDIO_MASK, audioCapabilities, ARRAY_LEN(audioCapabilities));
		}

		/* video capabilities */
		get_skinnyFormats(requestor->nativeformats & AST_FORMAT_VIDEO_MASK, videoCapabilities, ARRAY_LEN(videoCapabilities));
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

	if (!sccp_pbx_channel_allocate(channel, requestor ? requestor->linkedid : NULL)) {
		//! \todo handle error in more detail, cleanup sccp channel
		pbx_log(LOG_WARNING, "SCCP: Unable to allocate channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	}

	if (requestor) {
		/* set calling party */
		sccp_channel_set_callingparty(channel, requestor->caller.id.name.str, requestor->caller.id.number.str);
		sccp_channel_set_originalCalledparty(channel, requestor->redirecting.from.name.str, requestor->redirecting.from.number.str);
	}

EXITFUNC:

	sccp_restart_monitor();

	if (channel) {
		result_ast_channel = channel->owner;
		sccp_channel_release(channel);
	}
	return result_ast_channel;
}

static int sccp_wrapper_asterisk18_call(PBX_CHANNEL_TYPE * ast, char *dest, int timeout)
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

	res = sccp_pbx_call(c, dest, timeout);
	c = sccp_channel_release(c);
	return res;

}

static int sccp_wrapper_asterisk18_answer(PBX_CHANNEL_TYPE * chan)
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
static int sccp_wrapper_asterisk18_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s\n", newchan->name);
	sccp_channel_t *c = NULL;
	int res = 0;

	if (!(c = get_sccp_channel_from_pbx_channel(newchan))) {
		pbx_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, (void *) oldchan, newchan->name, (void *) newchan);
		return -1;
	} else {
		if (c->owner != oldchan) {
			ast_log(LOG_WARNING, "old channel wasn't %p but was %p\n", oldchan, c->owner);
			res = -1;
		} else {
			c->owner = ast_channel_ref(newchan);
			if (!sccp_strlen_zero(c->line->language)) {
				ast_string_field_set(newchan, language, c->line->language);
			}
			ast_channel_unref(oldchan);
			//! \todo force update of rtp_peer for directrtp
			// sccp_wrapper_asterisk18_set_rtp_peer(newchan, NULL, NULL, 0, 0, 0);

			//! \todo update remote capabilities after fixup

		}
		c = sccp_channel_release(c);
	}

	return res;
}

static enum ast_bridge_result sccp_wrapper_asterisk18_rtpBridge(PBX_CHANNEL_TYPE * c0, PBX_CHANNEL_TYPE * c1, int flags, PBX_FRAME_TYPE ** fo, PBX_CHANNEL_TYPE ** rc, int timeoutms)
{
	enum ast_bridge_result res;
	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	sccp_channel_t *sc0, *sc1;

	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_1 "SCCP: Bridging chan %s and chan %s\n", c0->name, c1->name);
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

static enum ast_rtp_glue_result sccp_wrapper_asterisk18_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{

	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_glue_result res = AST_RTP_GLUE_RESULT_REMOTE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	*rtp = audioRTP->rtp;
	if (!*rtp)
		return AST_RTP_GLUE_RESULT_FORBID;
#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	return res;
}

static int sccp_wrapper_asterisk18_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, format_t codecs, int nat_active)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	int result = 0;

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) format: %d\n", (int) codecs);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: __FILE__\n");

	do {
		if (!(c = CS_AST_CHANNEL_PVT(ast))) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
			result = -1;
			break;
		}
		if (!c->line) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO LINE\n");
			result = -1;
			break;
		}
		if (!(d = sccp_channel_getDevice_retained(c))) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO DEVICE\n");
			result = -1;
			break;
		}

		if (!d->directrtp) {										// in-direct rtp 
			struct ast_sockaddr us_tmp;
			struct sockaddr_in us = { 0, };

			ast_rtp_instance_get_local_address(rtp, &us_tmp);
			ast_sockaddr_to_sin(&us_tmp, &us);
			us.sin_addr.s_addr = us.sin_addr.s_addr ? us.sin_addr.s_addr : d->session->ourip.s_addr;
			sccp_rtp_set_peer(c, &c->rtp.audio, &us);
			/*              } else {                                                                                                // direct rtp
			   struct ast_sockaddr them_tmp;
			   struct sockaddr_in them = { 0, };

			   ast_rtp_instance_get_remote_address(rtp, &them_tmp);
			   ast_sockaddr_to_sin(&them_tmp, &them);
			   sccp_rtp_set_peer(c, &c->rtp.audio, &them); */
		}

		if (ast->_state != AST_STATE_UP) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%lu, nat=%d\n", (long unsigned int) codecs, d->nat);
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%lu, nat=%d\n", (long unsigned int) codecs, d->nat);
		}
	} while (0);

	/* Need a return here to break the bridge */
	d = d ? sccp_device_release(d) : NULL;
	return result;
}

static enum ast_rtp_glue_result sccp_wrapper_asterisk18_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
{
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo;
	struct sccp_rtp *audioRTP = NULL;
	enum ast_rtp_glue_result res = AST_RTP_GLUE_RESULT_REMOTE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	rtpInfo = sccp_rtp_getAudioPeerInfo(c, &audioRTP);							//! \todo should this not be getVideoPeerInfo
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	*rtp = audioRTP->rtp;
	if (!*rtp)
		return AST_RTP_GLUE_RESULT_FORBID;
#ifdef HAVE_PBX_RTP_ENGINE_H
	ao2_ref(*rtp, +1);
#endif
	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_GLUE_RESULT_LOCAL;
	}

	return res;
}

static format_t sccp_wrapper_asterisk18_getCodec(PBX_CHANNEL_TYPE * ast)
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

/*
 * \brief get callerid_name from pbx
 * \param sccp_channle Asterisk Channel
 * \param cid name result
 * \return parse result
 *
 * caller.id.name.str
 * caller.id.name.char_set
 * caller.id.name.presentation
 * caller.id.name.valid
 * caller.id.number.str
 * caller.id.number.plan
 * caller.id.number.presentation
 * caller.id.number.valid
 * caller.id.subaddress.str
 * caller.id.subaddress.type 
 * caller.id.subaddress.odd_even_indicator
 * caller.id.subaddress.valid
 * caller.ani.*
 * caller.ani2
 */
static int sccp_wrapper_asterisk18_callerid_name(const sccp_channel_t * channel, char **cid_name)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.name.valid && pbx_chan->caller.id.name.str && strlen(pbx_chan->caller.id.name.str) > 0) {
		*cid_name = strdup(pbx_chan->caller.id.name.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 *
 * caller.id.name.str
 * caller.id.name.char_set
 * caller.id.name.presentation
 * caller.id.name.valid
 * caller.id.number.str
 * caller.id.number.plan
 * caller.id.number.presentation
 * caller.id.number.valid
 * caller.id.subaddress.str
 * caller.id.subaddress.type 
 * caller.id.subaddress.odd_even_indicator
 * caller.id.subaddress.valid
 * caller.ani.*
 * caller.ani2
 */
static int sccp_wrapper_asterisk18_callerid_number(const sccp_channel_t * channel, char **cid_number)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.number.valid && pbx_chan->caller.id.number.str && strlen(pbx_chan->caller.id.number.str) > 0) {
		*cid_number = strdup(pbx_chan->caller.id.number.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_ton from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk18_callerid_ton(const sccp_channel_t * channel, char **cid_ton)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.number.valid) {
		return pbx_chan->caller.ani.number.plan;
	}
	return 0;
}

/*
 * \brief get callerid_ani from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 *
 * caller.id.name.*
 * caller.id.number.*
 * caller.id.subaddress.*
 * caller.ani.name.str
 * caller.ani.name.char_set
 * caller.ani.name.presentation
 * caller.ani.name.valid
 * caller.ani.number.str
 * caller.ani.number.plan
 * caller.ani.number.presentation
 * caller.ani.number.valid
 * caller.ani.subaddr.str
 * caller.ani.subaddr.type
 * caller.ani.subaddr.odd_even_indicator
 * caller.ani.subaddr.valid
 * caller.ani2
 */
static int sccp_wrapper_asterisk18_callerid_ani(const sccp_channel_t * channel, char **cid_ani)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.ani.number.valid && pbx_chan->caller.ani.number.str && strlen(pbx_chan->caller.ani.number.str) > 0) {
		*cid_ani = strdup(pbx_chan->caller.ani.number.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_dnid from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 *
 * caller.id.subaddress.str
 * caller.id.subaddress.type 
 * caller.id.subaddress.odd_even_indicator
 * caller.id.subaddress.valid
 */
static int sccp_wrapper_asterisk18_callerid_subaddr(const sccp_channel_t * channel, char **cid_subaddr)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.subaddress.valid && pbx_chan->caller.id.subaddress.str && strlen(pbx_chan->caller.id.subaddress.str) > 0) {
		*cid_subaddr = strdup(pbx_chan->caller.id.subaddress.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_dnid from pbx (Destination ID)
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 *
 * dialed.number.str
 * dialed.number.plan
 * dialed.subaddress.str
 * dialed.subaddress.type 
 * dialed.subaddress.odd_even_indicator
 * dialed.subaddress.valid
 * dialed.transit_network_select
 */
static int sccp_wrapper_asterisk18_callerid_dnid(const sccp_channel_t * channel, char **cid_dnid)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->dialed.number.str && strlen(pbx_chan->dialed.number.str) > 0) {
		*cid_dnid = strdup(pbx_chan->dialed.number.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_rdnis from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 *
 * redirecting.from.name.str
 * redirecting.from.name.char_set
 * redirecting.from.name.presentation
 * redirecting.from.name.valid
 * redirecting.from.number.str
 * redirecting.from.number.char_set
 * redirecting.from.number.presentation
 * redirecting.from.number.valid
 * redirecting.from.subaddress.str
 * redirecting.from.subaddress.type 
 * redirecting.from.subaddress.odd_even_indicator
 * redirecting.from.subaddress.valid
 * redirecting.to.name.str
 * redirecting.to.name.char_set
 * redirecting.to.name.presentation
 * redirecting.to.name.valid
 * redirecting.to.number.str
 * redirecting.to.number.char_set
 * redirecting.to.number.presentation
 * redirecting.to.number.valid
 * redirecting.to.subaddress.str
 * redirecting.to.subaddress.type 
 * redirecting.to.subaddress.odd_even_indicator
 * redirecting.to.subaddress.valid
 */
static int sccp_wrapper_asterisk18_callerid_rdnis(const sccp_channel_t * channel, char **cid_rdnis)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->redirecting.from.number.valid && pbx_chan->redirecting.from.number.str && strlen(pbx_chan->redirecting.from.number.str) > 0) {
		*cid_rdnis = strdup(pbx_chan->redirecting.from.number.str);
		return 1;
	}

	return 0;
}

/*
 * \brief get callerid_presence from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk18_callerid_presence(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	//      if (pbx_chan->caller.id.number.presentation) {
	//              return pbx_chan->caller.id.number.presentation;
	//      }
	//      return 0;
	if ((ast_party_id_presentation(&pbx_chan->caller.id) & AST_PRES_RESTRICTION) == AST_PRES_ALLOWED) {
		return CALLERID_PRESENCE_ALLOWED;
	}
	return CALLERID_PRESENCE_FORBIDDEN;
}

static int sccp_wrapper_asterisk18_rtp_stop(sccp_channel_t * channel)
{
	if (channel->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping phone media transmission on channel %08X\n", channel->callid);
		ast_rtp_instance_stop(channel->rtp.audio.rtp);
	}

	if (channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping video media transmission on channel %08X\n", channel->callid);
		ast_rtp_instance_stop(channel->rtp.video.rtp);
	}
	return 0;
}

static boolean_t sccp_wrapper_asterisk18_create_audio_rtp(sccp_channel_t * c)
{
	sccp_session_t *s = NULL;
	sccp_device_t *d = NULL;
	struct ast_sockaddr sock;
	struct ast_codec_pref astCodecPref;

	if (!c)
		return FALSE;
	if (!(d = sccp_channel_getDevice_retained(c)))
		return FALSE;

	s = d->session;

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = GLOB(bindaddr.sin_port);
		sin.sin_addr = s->ourip;
/*		sccp_socket_getOurAddressfor(s->sin.sin_addr, sin.sin_addr);*/		// maybe we should use this opertunity to check the connected ip-address again
		ast_sockaddr_from_sin(&sock, &sin);
	} else {
		ast_sockaddr_from_sin(&sock, &GLOB(bindaddr));
	}
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating rtp server connection on %s\n", DEV_ID_LOG(d), ast_sockaddr_stringify_host(&sock));

	c->rtp.audio.rtp = ast_rtp_instance_new("asterisk", sched, &sock, NULL);
	if (!c->rtp.audio.rtp) {
		d = sccp_device_release(d);
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: rtp server created at %s:%d\n", DEV_ID_LOG(d), ast_sockaddr_stringify_host(&sock), ast_sockaddr_port(&sock));
	if (c->owner) {
		ast_rtp_instance_set_prop(c->rtp.audio.rtp, AST_RTP_PROPERTY_RTCP, 1);

		if (SCCP_DTMFMODE_INBAND == d->dtmfmode) {
			ast_rtp_instance_dtmf_mode_set(c->rtp.audio.rtp, AST_RTP_DTMF_MODE_INBAND);
		} else {
			ast_rtp_instance_set_prop(c->rtp.audio.rtp, AST_RTP_PROPERTY_DTMF, 1);
			ast_rtp_instance_set_prop(c->rtp.audio.rtp, AST_RTP_PROPERTY_DTMF_COMPENSATE, 1);
		}

		ast_channel_set_fd(c->owner, 0, ast_rtp_instance_fd(c->rtp.audio.rtp, 0));
		ast_channel_set_fd(c->owner, 1, ast_rtp_instance_fd(c->rtp.audio.rtp, 1));
		ast_queue_frame(c->owner, &ast_null_frame);
		ast_rtp_instance_set_prop(c->rtp.audio.rtp, AST_RTP_PROPERTY_NAT, d->nat);
	}

	memset(&astCodecPref, 0, sizeof(astCodecPref));
	if (skinny_codecs2pbx_codec_pref(c->preferences.audio, &astCodecPref)) {
		ast_rtp_codecs_packetization_set(ast_rtp_instance_get_codecs(c->rtp.audio.rtp), c->rtp.audio.rtp, &astCodecPref);
	}
	//char pref_buf[128];
	//      ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
	//      sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pref: %s\n", c->line->name, c->callid, pref_buf);
	ast_rtp_instance_set_qos(c->rtp.audio.rtp, d->audio_tos, d->audio_cos, "SCCP RTP");

	/* add payload mapping for skinny codecs */
	uint8_t i;
	struct ast_rtp_codecs *codecs = ast_rtp_instance_get_codecs(c->rtp.audio.rtp);

	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		/* add audio codecs only */
		if (skinny_codecs[i].mimesubtype && skinny_codecs[i].codec_type == SKINNY_CODEC_TYPE_AUDIO) {
			ast_rtp_codecs_payloads_set_rtpmap_type_rate(codecs, NULL, skinny_codecs[i].codec, "audio", (char *) skinny_codecs[i].mimesubtype, (enum ast_rtp_options) 0, skinny_codecs[i].sample_rate);
		}
	}
	d = sccp_device_release(d);

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk18_create_video_rtp(sccp_channel_t * c)
{
	sccp_session_t *s;
	sccp_device_t *d = NULL;
	struct ast_sockaddr sock;
	struct ast_codec_pref astCodecPref;

	if (!c)
		return FALSE;
	if (!(d = sccp_channel_getDevice_retained(c)))
		return FALSE;

	s = d->session;
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating vrtp server connection at %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->ourip));

	ast_sockaddr_from_sin(&sock, &GLOB(bindaddr));
	c->rtp.video.rtp = ast_rtp_instance_new("asterisk", sched, &sock, NULL);
	if (!c->rtp.video.rtp) {
		d = sccp_device_release(d);
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: vrtp created at %s:%d\n", DEV_ID_LOG(d), ast_sockaddr_stringify_host(&sock), ast_sockaddr_port(&sock));
	if (c->owner) {
		ast_rtp_instance_set_prop(c->rtp.video.rtp, AST_RTP_PROPERTY_RTCP, 1);

		ast_channel_set_fd(c->owner, 2, ast_rtp_instance_fd(c->rtp.video.rtp, 0));
		ast_channel_set_fd(c->owner, 3, ast_rtp_instance_fd(c->rtp.video.rtp, 1));

		ast_queue_frame(c->owner, &ast_null_frame);
	}

	memset(&astCodecPref, 0, sizeof(astCodecPref));
	if (skinny_codecs2pbx_codec_pref(c->preferences.video, &astCodecPref)) {
		ast_rtp_codecs_packetization_set(ast_rtp_instance_get_codecs(c->rtp.audio.rtp), c->rtp.audio.rtp, &astCodecPref);
	}
	//char pref_buf[128];
	//ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
	//sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);

	ast_rtp_instance_set_qos(c->rtp.video.rtp, d->video_tos, d->video_cos, "SCCP VRTP");

	/* add payload mapping for skinny codecs */
	uint8_t i;
	struct ast_rtp_codecs *codecs = ast_rtp_instance_get_codecs(c->rtp.video.rtp);

	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		/* add video codecs only */
		if (skinny_codecs[i].mimesubtype && skinny_codecs[i].codec_type == SKINNY_CODEC_TYPE_VIDEO) {
			ast_rtp_codecs_payloads_set_rtpmap_type_rate(codecs, NULL, skinny_codecs[i].codec, "video", (char *) skinny_codecs[i].mimesubtype, (enum ast_rtp_options) 0, skinny_codecs[i].sample_rate);
		}
	}

	d = sccp_device_release(d);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk18_destroyRTP(PBX_RTP_TYPE * rtp)
{
	int res;

	res = ast_rtp_instance_destroy(rtp);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_checkHangup(const sccp_channel_t * channel)
{
	int res;

	res = ast_check_hangup(channel->owner);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_rtpGetPeer(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	struct ast_sockaddr addr;

	ast_rtp_instance_get_remote_address(rtp, &addr);
	ast_sockaddr_to_sin(&addr, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk18_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	struct ast_sockaddr addr;

	ast_rtp_instance_get_local_address(rtp, &addr);
	ast_sockaddr_to_sin(&addr, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk18_getChannelByName(const char *name, PBX_CHANNEL_TYPE ** pbx_channel)
{
	PBX_CHANNEL_TYPE *ast = ast_channel_get_by_name(name);

	if (!ast)
		return FALSE;

	*pbx_channel = ast;
	return TRUE;
}

static int sccp_wrapper_asterisk18_rtp_set_peer(const struct sccp_rtp *rtp, const struct sockaddr_in *new_peer, int nat_active)
{
	struct ast_sockaddr ast_sockaddr_dest;
	struct ast_sockaddr ast_sockaddr_source;
	int res;

	((struct sockaddr_in *) new_peer)->sin_family = AF_INET;
	ast_sockaddr_from_sin(&ast_sockaddr_dest, new_peer);
	ast_sockaddr_set_port(&ast_sockaddr_dest, ntohs(new_peer->sin_port));

	res = ast_rtp_instance_set_remote_address(rtp->rtp, &ast_sockaddr_dest);

	ast_rtp_instance_get_local_address(rtp->rtp, &ast_sockaddr_source);
//	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: Tell PBX to send RTP/UDP media from '%s:%d' to '%s:%d' (NAT: %s)\n", ast_sockaddr_stringify_host(&ast_sockaddr_source), ast_sockaddr_port(&ast_sockaddr_source), ast_sockaddr_stringify_host(&ast_sockaddr_dest), ast_sockaddr_port(&ast_sockaddr_dest), nat_active ? "yes" : "no");
	if (nat_active) {
		ast_rtp_instance_set_prop(rtp->rtp, AST_RTP_PROPERTY_NAT, 1);
	}
	return res;
}

static boolean_t sccp_wrapper_asterisk18_setWriteFormat(const sccp_channel_t * channel, skinny_codec_t codec)
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
		//              pbx_log(LOG_NOTICE, "Bridge found\n");
		channel->owner->writeformat = 0;

		bridge->readformat = 0;
		//              if(bridge->readtrans){
		//                      ast_translator_free_path(bridge->readtrans);
		//                      bridge->readtrans = NULL;
		//              }
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

static boolean_t sccp_wrapper_asterisk18_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
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
		//              pbx_log(LOG_NOTICE, "Bridge found\n");
		channel->owner->readformat = 0;

		bridge->writeformat = 0;
		//              if(bridge->writetrans){
		//                      ast_translator_free_path(bridge->writetrans);
		//                      bridge->writetrans = NULL;
		//              }
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

static void sccp_wrapper_asterisk18_setCalleridName(const sccp_channel_t * channel, const char *name)
{
	if (name) {
		channel->owner->caller.id.name.valid = 1;
		ast_party_name_free(&channel->owner->caller.id.name);
		channel->owner->caller.id.name.str = ast_strdup(name);
	}
}

static void sccp_wrapper_asterisk18_setCalleridNumber(const sccp_channel_t * channel, const char *number)
{
	if (number) {
		channel->owner->caller.id.number.valid = 1;
		ast_party_number_free(&channel->owner->caller.id.number);
		channel->owner->caller.id.number.str = ast_strdup(number);
	}
}

static void sccp_wrapper_asterisk18_setRedirectingParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	if (number) {
		ast_party_number_free(&channel->owner->redirecting.from.number);
		channel->owner->redirecting.from.number.str = ast_strdup(number);
		channel->owner->redirecting.from.number.valid = 1;
	}

	if (name) {
		ast_party_name_free(&channel->owner->redirecting.from.name);
		channel->owner->redirecting.from.name.str = ast_strdup(name);
		channel->owner->redirecting.from.name.valid = 1;
	}
}

static void sccp_wrapper_asterisk18_setRedirectedParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	if (number) {
		channel->owner->redirecting.to.number.valid = 1;
		ast_party_number_free(&channel->owner->redirecting.to.number);
		channel->owner->redirecting.to.number.str = ast_strdup(number);
	}

	if (name) {
		channel->owner->redirecting.to.name.valid = 1;
		ast_party_name_free(&channel->owner->redirecting.to.name);
		channel->owner->redirecting.to.name.str = ast_strdup(name);
	}
}

static void sccp_wrapper_asterisk18_updateConnectedLine(const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason)
{
	struct ast_party_connected_line connected;
	struct ast_set_party_connected_line update_connected;

	memset(&update_connected, 0, sizeof(update_connected));

	if (number) {
		update_connected.id.number = 1;
		connected.id.number.valid = 1;
		connected.id.number.str = (char *) number;
		connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}

	if (name) {
		update_connected.id.name = 1;
		connected.id.name.valid = 1;
		connected.id.name.str = (char *) name;
		connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}
	connected.id.tag = NULL;
	connected.source = reason;

	ast_channel_queue_connected_line_update(channel->owner, &connected, &update_connected);
}

static int sccp_wrapper_asterisk18_sched_add(int when, sccp_sched_cb callback, const void *data)
{
	if (sched)
		return ast_sched_add(sched, when, callback, data);
	return FALSE;
}

static long sccp_wrapper_asterisk18_sched_when(int id)
{
	if (sched)
		return ast_sched_when(sched, id);
	return FALSE;
}

static int sccp_wrapper_asterisk18_sched_wait(int id)
{
	if (sched)
		return ast_sched_wait(sched);
	return FALSE;
}

static int sccp_wrapper_asterisk18_sched_del(int id)
{
	if (sched)
		return ast_sched_del(sched, id);
	return FALSE;
}

static int sccp_wrapper_asterisk18_setCallState(const sccp_channel_t * channel, int state)
{
	sccp_pbx_setcallstate((sccp_channel_t *) channel, state);
	//! \todo implement correct return value (take into account negative deadlock prevention)
	return 0;
}

static boolean_t sccp_asterisk_getRemoteChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{

	PBX_CHANNEL_TYPE *remotePeer;
	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

	for (; (remotePeer = ast_channel_iterator_next(iterator)); ast_channel_unref(remotePeer)) {
		if (pbx_find_channel_by_linkid(remotePeer, (void *) channel->owner->linkedid)) {
			break;
		}
	}
	ast_channel_iterator_destroy(iterator);

	if (remotePeer) {
		*pbx_channel = remotePeer;
		ast_channel_unref(remotePeer);
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
		c = sccp_channel_release(c);
		return -1;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, (d->dtmfmode) ? "outofband" : "inband");

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		d = sccp_device_release(d);
		c = sccp_channel_release(c);
		return -1;
	}

	sccp_dev_keypadbutton(d, digit, sccp_device_find_index_for_line(d, c->line->name), c->callid);

	d = sccp_device_release(d);
	c = sccp_channel_release(c);
	return -1;
}

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_findChannelWithCallback(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data, boolean_t lock)
{
	PBX_CHANNEL_TYPE *remotePeer;

	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	if (!lock) {
		((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;
	}

	for (; (remotePeer = ast_channel_iterator_next(iterator)); ast_channel_unref(remotePeer)) {

		if (found_cb(remotePeer, data)) {
			ast_channel_lock(remotePeer);
			ast_channel_unref(remotePeer);
			break;
		}

	}
	ast_channel_iterator_destroy(iterator);
	return remotePeer;
}

/*! \brief Set an option on a asterisk channel */
#if 0
static int sccp_wrapper_asterisk18_setOption(PBX_CHANNEL_TYPE * ast, int option, void *data, int datalen)
{
	int res = -1;
	sccp_channel_t *c = NULL;

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		return -1;
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: channel: %s(%s) setOption: %d\n", c->currentDeviceId, sccp_channel_toString(c), ast->name, option);

		//! if AST_OPTION_FORMAT_READ / AST_OPTION_FORMAT_WRITE are available we might be indication that we can do transcoding (channel.c:set_format). Correct ? */
		switch (option) {
			case AST_OPTION_FORMAT_READ:
				if (c->rtp.audio.rtp) {
					res = ast_rtp_instance_set_read_format(c->rtp.audio.rtp, *(int *) data);
				}
				break;
			case AST_OPTION_FORMAT_WRITE:
				if (c->rtp.audio.rtp) {
					res = ast_rtp_instance_set_write_format(c->rtp.audio.rtp, *(int *) data);
				}
				break;

			case AST_OPTION_MAKE_COMPATIBLE:
				if (c->rtp.audio.rtp) {
					res = ast_rtp_instance_make_compatible(ast, c->rtp.audio.rtp, (PBX_CHANNEL_TYPE *) data);
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
		c = sccp_channel_release(c);
	}
	return res;
}
#endif

static void sccp_wrapper_asterisk_set_pbxchannel_linkedid(PBX_CHANNEL_TYPE * pbx_channel, const char *new_linkedid)
{
	if (pbx_channel) {
		if (!strcmp(pbx_channel->linkedid, new_linkedid)) {
			return;
		}
#if HAVE_PBX_CEL_H
		ast_cel_check_retire_linkedid(pbx_channel);
#endif
		ast_string_field_set(pbx_channel, linkedid, new_linkedid);
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

static void sccp_wrapper_asterisk_set_channel_linkedid(const sccp_channel_t * channel, const char *new_linkedid)
{
	if (channel->owner) {
		sccp_wrapper_asterisk_set_pbxchannel_linkedid(channel->owner, new_linkedid);
	}
}

static void sccp_wrapper_asterisk_set_channel_call_forward(const sccp_channel_t * channel, const char *new_call_forward)
{
	if (channel->owner) {
		pbx_string_field_set(channel->owner, call_forward, new_call_forward);
	}
}

static void sccp_wrapper_asterisk_set_channel_name(const sccp_channel_t * channel, const char *new_name)
{
	if (channel->owner) {
		pbx_string_field_set(channel->owner, name, new_name);
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
		//              channel->owner->tech_pvt = channel;
	}
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
	}
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
	d = sccp_device_release(d);
	c = sccp_channel_release(c);

	return 0;
}

/*!
 * \brief Queue a control frame
 * \param pbx_channel PBX Channel
 * \param control as Asterisk Control Frame Type
 */
int sccp_asterisk_queue_control(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control)
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
int sccp_asterisk_queue_control_data(const PBX_CHANNEL_TYPE * pbx_channel, enum ast_control_frame_type control, const void *data, size_t datalen)
{
	struct ast_frame f = { AST_FRAME_CONTROL,.subclass.integer = control,.data.ptr = (void *) data,.datalen = datalen };
	return ast_queue_frame((PBX_CHANNEL_TYPE *) pbx_channel, &f);
}

/*!
 * \brief Get Hint Extension State and return the matching Busy Lamp Field State 
 */
static sccp_BLFState_t sccp_wrapper_asterisk108_getExtensionState(const char *extension, const char *context)
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

#if defined(__cplusplus) || defined(c_plusplus)
struct ast_rtp_glue sccp_rtp = {
	/* *INDENT-OFF* */
	type:		SCCP_TECHTYPE_STR,
	mod:		NULL,
	get_rtp_info:	sccp_wrapper_asterisk18_get_rtp_peer,
	get_vrtp_info:	sccp_wrapper_asterisk18_get_vrtp_peer,
	get_trtp_info:	NULL,
	update_peer:	sccp_wrapper_asterisk18_set_rtp_peer,
	get_codec:	sccp_wrapper_asterisk18_getCodec,
	/* *INDENT-ON* */
};
#else

/*!
 * \brief using RTP Glue Engine
 */
struct ast_rtp_glue sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk18_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk18_get_vrtp_peer,
	.update_peer = sccp_wrapper_asterisk18_set_rtp_peer,
	.get_codec = sccp_wrapper_asterisk18_getCodec,
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
	name:		"sccp",
	msg_send:	sccp_asterisk_message_send,
	/* *INDENT-ON* */
};
#else
static const struct ast_msg_tech sccp_msg_tech = {
	.name = "sccp",
	.msg_send = sccp_asterisk_message_send,
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
	alloc_pbxChannel:		sccp_wrapper_asterisk18_allocPBXChannel,
	set_callstate:			sccp_wrapper_asterisk18_setCallState,
	checkhangup:			sccp_wrapper_asterisk18_checkHangup,
	hangup:				NULL,
	requestHangup:			sccp_wrapper_asterisk_requestHangup,
	forceHangup:                    sccp_wrapper_asterisk_forceHangup,
	extension_status:		sccp_wrapper_asterisk18_extensionStatus,

	setPBXChannelLinkedId:		sccp_wrapper_asterisk_set_pbxchannel_linkedid,
	/** get channel by name */
	getChannelByName:		sccp_wrapper_asterisk18_getChannelByName,
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


	set_nativeAudioFormats:		sccp_wrapper_asterisk18_setNativeAudioFormats,
	set_nativeVideoFormats:		sccp_wrapper_asterisk18_setNativeVideoFormats,

	getPeerCodecCapabilities:	NULL,
	send_digit:			sccp_wrapper_asterisk18_sendDigit,
	send_digits:			sccp_wrapper_asterisk18_sendDigits,

	sched_add:			sccp_wrapper_asterisk18_sched_add,
	sched_del:			sccp_wrapper_asterisk18_sched_del,
	sched_when:			sccp_wrapper_asterisk18_sched_when,
	sched_wait:			sccp_wrapper_asterisk18_sched_wait,

	/* rtp */
	rtp_getPeer:			sccp_wrapper_asterisk18_rtpGetPeer,
	rtp_getUs:			sccp_wrapper_asterisk18_rtpGetUs,
	rtp_setPeer:			sccp_wrapper_asterisk18_rtp_set_peer,
	rtp_setWriteFormat:		sccp_wrapper_asterisk18_setWriteFormat,
	rtp_setReadFormat:		sccp_wrapper_asterisk18_setReadFormat,
	rtp_destroy:			sccp_wrapper_asterisk18_destroyRTP,
	rtp_stop:			sccp_wrapper_asterisk18_rtp_stop,
	rtp_codec:			NULL,
	rtp_audio_create:		sccp_wrapper_asterisk18_create_audio_rtp,
	rtp_video_create:		sccp_wrapper_asterisk18_create_video_rtp,
	rtp_get_payloadType:		sccp_wrapper_asterisk18_get_payloadType,
	rtp_get_sampleRate:		sccp_wrapper_asterisk18_get_sampleRate,
	rtp_bridgePeers:		NULL,

	/* callerid */
	get_callerid_name:		sccp_wrapper_asterisk18_callerid_name,
	get_callerid_number:		sccp_wrapper_asterisk18_callerid_number,
	get_callerid_ton:		sccp_wrapper_asterisk18_callerid_ton,
	get_callerid_ani:		sccp_wrapper_asterisk18_callerid_ani,
	get_callerid_subaddr:		sccp_wrapper_asterisk18_callerid_subaddr,
	get_callerid_dnid:		sccp_wrapper_asterisk18_callerid_dnid,
	get_callerid_rdnis:		sccp_wrapper_asterisk18_callerid_rdnis,
	get_callerid_presence:		sccp_wrapper_asterisk18_callerid_presence,

	set_callerid_name:		sccp_wrapper_asterisk18_setCalleridName,
	set_callerid_number:		sccp_wrapper_asterisk18_setCalleridNumber,
	set_callerid_ani:		NULL,
	set_callerid_dnid:		NULL,
	set_callerid_redirectingParty:	sccp_wrapper_asterisk18_setRedirectingParty,
	set_callerid_redirectedParty:	sccp_wrapper_asterisk18_setRedirectedParty,
	set_callerid_presence:		sccp_wrapper_asterisk18_setCalleridPresence,
	set_connected_line:		sccp_wrapper_asterisk18_updateConnectedLine,
	sendRedirectedUpdate:		sccp_asterisk_sendRedirectedUpdate,

	/* feature section */
	feature_park:			sccp_wrapper_asterisk18_park,
	feature_stopMusicOnHold:	NULL,
	feature_addToDatabase:		sccp_asterisk_addToDatabase,
	feature_getFromDatabase:	sccp_asterisk_getFromDatabase,
	feature_removeFromDatabase:	sccp_asterisk_removeFromDatabase,
	feature_removeTreeFromDatabase:	sccp_asterisk_removeTreeFromDatabase,
	getFeatureExtension:		sccp_wrapper_asterisk18_getFeatureExtension,
	feature_pickup:			sccp_wrapper_asterisk18_pickupChannel,

	eventSubscribe:			NULL,
	findChannelByCallback:		sccp_wrapper_asterisk18_findChannelWithCallback,

	moh_start:			sccp_asterisk_moh_start,
	moh_stop:			sccp_asterisk_moh_stop,
	queue_control:			sccp_asterisk_queue_control,
	queue_control_data:		sccp_asterisk_queue_control_data,

	allocTempPBXChannel:		sccp_wrapper_asterisk18_allocTempPBXChannel,
	masqueradeHelper:		sccp_wrapper_asterisk18_masqueradeHelper,
	requestForeignChannel:		sccp_wrapper_asterisk18_requestForeignChannel,
	
	set_language:			sccp_wrapper_asterisk_setLanguage,

	getExtensionState:		sccp_wrapper_asterisk108_getExtensionState,
	findPickupChannelByExtenLocked:	sccp_wrapper_asterisk18_findPickupChannelByExtenLocked,
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
	.alloc_pbxChannel 		= sccp_wrapper_asterisk18_allocPBXChannel,
	.requestHangup 			= sccp_wrapper_asterisk_requestHangup,
	.forceHangup                    = sccp_wrapper_asterisk_forceHangup,
	.extension_status 		= sccp_wrapper_asterisk18_extensionStatus,
	.setPBXChannelLinkedId		= sccp_wrapper_asterisk_set_pbxchannel_linkedid,
	.getChannelByName 		= sccp_wrapper_asterisk18_getChannelByName,

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
	.checkhangup			= sccp_wrapper_asterisk18_checkHangup,
	
	/* digits */
	.send_digits 			= sccp_wrapper_asterisk18_sendDigits,
	.send_digit 			= sccp_wrapper_asterisk18_sendDigit,

	/* schedulers */
	.sched_add 			= sccp_wrapper_asterisk18_sched_add,
	.sched_del 			= sccp_wrapper_asterisk18_sched_del,
	.sched_when 			= sccp_wrapper_asterisk18_sched_when,
	.sched_wait 			= sccp_wrapper_asterisk18_sched_wait,
	
	/* callstate / indicate */
	.set_callstate 			= sccp_wrapper_asterisk18_setCallState,

	/* codecs */
	.set_nativeAudioFormats 	= sccp_wrapper_asterisk18_setNativeAudioFormats,
	.set_nativeVideoFormats 	= sccp_wrapper_asterisk18_setNativeVideoFormats,

	/* rtp */
	.rtp_getPeer			= sccp_wrapper_asterisk18_rtpGetPeer,
	.rtp_getUs 			= sccp_wrapper_asterisk18_rtpGetUs,
	.rtp_stop 			= sccp_wrapper_asterisk18_rtp_stop,
	.rtp_audio_create 		= sccp_wrapper_asterisk18_create_audio_rtp,
	.rtp_video_create 		= sccp_wrapper_asterisk18_create_video_rtp,
	.rtp_get_payloadType 		= sccp_wrapper_asterisk18_get_payloadType,
	.rtp_get_sampleRate 		= sccp_wrapper_asterisk18_get_sampleRate,
	.rtp_destroy 			= sccp_wrapper_asterisk18_destroyRTP,
	.rtp_setWriteFormat 		= sccp_wrapper_asterisk18_setWriteFormat,
	.rtp_setReadFormat 		= sccp_wrapper_asterisk18_setReadFormat,
	.rtp_setPeer 			= sccp_wrapper_asterisk18_rtp_set_peer,

	/* callerid */
	.get_callerid_name 		= sccp_wrapper_asterisk18_callerid_name,
	.get_callerid_number 		= sccp_wrapper_asterisk18_callerid_number,
	.get_callerid_ton 		= sccp_wrapper_asterisk18_callerid_ton,
	.get_callerid_ani 		= sccp_wrapper_asterisk18_callerid_ani,
	.get_callerid_subaddr 		= sccp_wrapper_asterisk18_callerid_subaddr,
	.get_callerid_dnid 		= sccp_wrapper_asterisk18_callerid_dnid,
	.get_callerid_rdnis 		= sccp_wrapper_asterisk18_callerid_rdnis,
	.get_callerid_presence 		= sccp_wrapper_asterisk18_callerid_presence,
	.set_callerid_name 		= sccp_wrapper_asterisk18_setCalleridName,	//! \todo implement callback
	.set_callerid_number 		= sccp_wrapper_asterisk18_setCalleridNumber,	//! \todo implement callback
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_wrapper_asterisk18_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_wrapper_asterisk18_setRedirectedParty,
	.set_callerid_presence 		= sccp_wrapper_asterisk18_setCalleridPresence,
	.set_connected_line		= sccp_wrapper_asterisk18_updateConnectedLine,
	.sendRedirectedUpdate		= sccp_asterisk_sendRedirectedUpdate,
	
	
	/* database */
	.feature_addToDatabase 		= sccp_asterisk_addToDatabase,
	.feature_getFromDatabase 	= sccp_asterisk_getFromDatabase,
	.feature_removeFromDatabase     = sccp_asterisk_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_asterisk_removeTreeFromDatabase,
	
	
	.feature_park			= sccp_wrapper_asterisk18_park,
	.getFeatureExtension		= sccp_wrapper_asterisk18_getFeatureExtension,
	.feature_pickup			= sccp_wrapper_asterisk18_pickupChannel,
	
	.findChannelByCallback		= sccp_wrapper_asterisk18_findChannelWithCallback,

	.moh_start			= sccp_asterisk_moh_start,
	.moh_stop			= sccp_asterisk_moh_stop,
	.queue_control			= sccp_asterisk_queue_control,
	.queue_control_data		= sccp_asterisk_queue_control_data,
	
	.allocTempPBXChannel		= sccp_wrapper_asterisk18_allocTempPBXChannel,
	.masqueradeHelper		= sccp_wrapper_asterisk18_masqueradeHelper,
	.requestForeignChannel		= sccp_wrapper_asterisk18_requestForeignChannel,
	
	.set_language			= sccp_wrapper_asterisk_setLanguage,

	.getExtensionState		= sccp_wrapper_asterisk108_getExtensionState,
	.findPickupChannelByExtenLocked = sccp_wrapper_asterisk18_findPickupChannelByExtenLocked,
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

	//      ast_enable_distributed_devstate();

	//      ast_rtp_glue_register(&sccp_rtp);
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
	//      ast_rtp_glue_unregister(&sccp_rtp);
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
		sched_context_destroy(sched);
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

static int module_reload(void)
{
	sccp_reload();
	return 0;
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

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, 
        "Skinny Client Control Protocol (SCCP). SCCP-Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "', NULL)",
        .load = load_module,
        .unload = unload_module,
        .reload = module_reload,
        .load_pri = AST_MODPRI_DEFAULT,
        .nonoptreq = "res_rtp_asterisk,chan_local",);
#endif

PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data)
{
	PBX_CHANNEL_TYPE *remotePeer;

	struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();

	((struct ao2_iterator *) iterator)->flags |= AO2_ITERATOR_DONTLOCK;

	for (; (remotePeer = ast_channel_iterator_next(iterator)); ast_channel_unref(remotePeer)) {

		if (found_cb(remotePeer, data)) {
			ast_channel_lock(remotePeer);
			ast_channel_unref(remotePeer);
			break;
		}

	}
	ast_channel_iterator_destroy(iterator);
	return remotePeer;
}

PBX_CHANNEL_TYPE *sccp_wrapper_asterisk18_findPickupChannelByExtenLocked(PBX_CHANNEL_TYPE *chan, const char *exten, const char *context)
{
        struct ast_channel *target = NULL;		/*!< Potential pickup target */
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
                target = ast_channel_unref(target);
        }

        ast_channel_iterator_destroy(iter);
	return target;
}
