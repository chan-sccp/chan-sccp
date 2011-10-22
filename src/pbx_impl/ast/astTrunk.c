/*!
 * \file 	sccp_astTrunk.c
 * \brief 	SCCP PBX Asterisk Wrapper Class
 * \author 	Marcello Ceshia
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */


#include "../../config.h"
#include "../../common.h"
#include "astTrunk.h"
#include <asterisk/sched.h>
#include <asterisk/netsock2.h>

struct ast_sched_context *sched = 0;
struct io_context *io = 0;

#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel_tech sccp_tech;
#endif

#define RTP_NEW_SOURCE(_c,_log) 								\
        if(c->rtp.audio.rtp) { 										\
                pbx_rtp_new_source(c->rtp.audio.rtp); 							\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }

#define RTP_CHANGE_SOURCE(_c,_log) 								\
        if(c->rtp.audio.rtp) {										\
                pbx_rtp_change_source(c->rtp.audio.rtp);						\
                sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE))(VERBOSE_PREFIX_3 "SCCP: " #_log "\n"); 	\
        }


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


static void get_skinnyFormats(struct ast_format_cap *format, skinny_codec_t codecs[], size_t size)
{
	unsigned int x;
	unsigned len = 0;
	
	size_t f_len;
	struct ast_format tmp_fmt;
	const struct ast_format_list *f_list = ast_format_list_get(&f_len);

	if (!size){
		f_list = ast_format_list_destroy(f_list);
		return;
	}

	for (x = 0; x < ARRAY_LEN(skinny2pbx_codec_maps) && len <= size; x++) {
		ast_format_copy(&tmp_fmt, &f_list[x].format);
		if (ast_format_cap_iscompatible(format, &tmp_fmt)) {
			if (skinny2pbx_codec_maps[x].pbx_codec == ((uint)tmp_fmt.id) ) {
				codecs[len++] = skinny2pbx_codec_maps[x].skinny_codec;
			}
		}
	}
	f_list = ast_format_list_destroy(f_list);
}

/*!
 * \brief 	start monitoring thread of chan_sccp
 * \param 	data
 *
 * \lock
 * 	- monitor_lock
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
		ast_mutex_lock(&GLOB(monitor_lock));
		if (res >= 0) {
			ast_sched_runq(sched);
		}
		ast_mutex_unlock(&GLOB(monitor_lock));
	}
	/* Never reached */
	return NULL;

}

/*!
 * \brief Read from an Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 *
 * \called_from_asterisk
 */
static PBX_FRAME_TYPE *sccp_wrapper_asterisk111_rtp_read(PBX_CHANNEL_TYPE * ast)
{
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	PBX_FRAME_TYPE *frame;

	frame = &ast_null_frame;
	// ASTERISK_VERSION_NUMBER >= 10400

	if (!c || !c->rtp.audio.rtp)
		return frame;

	switch (ast->fdno) {

	case 0:
		frame = ast_rtp_instance_read(c->rtp.audio.rtp, 0);		/* RTP Audio */
		break;
	case 1:
		frame = ast_rtp_instance_read(c->rtp.audio.rtp, 1);		/* RTCP Control Channel */
		break;
	case 2:
#ifdef CS_SCCP_VIDEO
		frame = ast_rtp_instance_read(c->rtp.video.rtp, 0);		/* RTP Video */
#endif
		break;
	case 3:
#ifdef CS_SCCP_VIDEO
		frame = ast_rtp_instance_read(c->rtp.video.rtp, 1);		/* RTCP Control Channel for video */
#endif
		break;

	default:
		return frame;
	}

	if (!frame) {
		ast_log(LOG_WARNING, "%s: error reading frame == NULL\n", DEV_ID_LOG(sccp_channel_getDevice(c)));
		return frame;
	}
	//sccp_log(1)(VERBOSE_PREFIX_3 "%s: read format: ast->fdno: %d, frametype: %d, %s(%d)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), ast->fdno, frame->frametype, pbx_getformatname(frame->subclass), frame->subclass);

	if (frame->frametype == AST_FRAME_VOICE) {

		if (!(ast_format_cap_iscompatible(ast->nativeformats, &frame->subclass.format))){
			ast_format_cap_remove_bytype(ast->nativeformats, AST_FORMAT_TYPE_AUDIO);
			ast_format_cap_add(ast->nativeformats, &frame->subclass.format);
			
			ast_set_read_format(ast, &ast->readformat);
			ast_set_write_format(ast, &ast->writeformat);
		}
	}
	return frame;
}

/*!
 * \brief Find Asterisk/PBX channel by linkid
 *
 * \param ast 	pbx channel
 * \param data	linkId as void *
 *
 * \return int
 *
 * \todo I don't understand what this functions returns
 */
static int pbx_find_channel_by_linkid(PBX_CHANNEL_TYPE *ast, void *data)
{
	char *linkId = data;

	if (!data)
		return 0;

	ast_log(LOG_NOTICE, "SCCP: peer name: %s, linkId: %s\n", ast->name ? ast->name : "(null)", ast->linkedid ? ast->linkedid : "(null)");

	return !ast->pbx && ast->linkedid && (!strcasecmp(ast->linkedid, linkId)) && !ast->masq;
}

/*!
 * \brief Update Connected Line
 * \param channel Asterisk Channel as ast_channel
 * \param data Asterisk Data
 * \param datalen Asterisk Data Length
 */
static void sccp_wrapper_asterisk111_connectedline(sccp_channel_t * channel, const void *data, size_t datalen)
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
}

static int sccp_wrapper_asterisk111_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
{
//      skinny_codec_t oldChannelFormat;

	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	int res = 0;

	int deadlockAvoidanceCounter = 0;

	if (!c)
		return -1;

	if (!sccp_channel_getDevice(c))
		return -1;


	// deadlock avoidance loop
	while (sccp_channel_trylock(c)) {
		if (deadlockAvoidanceCounter++ > 100) {
			ast_log(LOG_ERROR, "SCCP: We would create a deadlock in %s:%d, giving up\n", __FILE__, __LINE__);
			return -1;
		}
		usleep(100);
	}

	if (c->state == SCCP_CHANNELSTATE_DOWN) {
		sccp_channel_unlock(c);
		return -1;
	}

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Asterisk indicate '%d' condition on channel %s\n", DEV_ID_LOG(sccp_channel_getDevice(c)), ind, ast->name);

	/* when the rtp media stream is open we will let asterisk emulate the tones */
	res = (c->rtp.audio.rtp ? -1 : 0);

	switch (ind) {
	case AST_CONTROL_RINGING:
		if (SKINNY_CALLTYPE_OUTBOUND == c->calltype) {
			// Allow signalling of RINGOUT only on outbound calls.
			// Otherwise, there are some issues with late arrival of ringing
			// indications on ISDN calls (chan_lcr, chan_dahdi) (-DD).
			sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_RINGOUT);

			struct ast_channel_iterator *iterator = ast_channel_iterator_all_new();
			((struct ao2_iterator *)iterator)->flags |= AO2_ITERATOR_DONTLOCK;
// 			if(iterator->simple_iterator)
// 			  ast_log(LOG_WARNING, "remote caps:\n");
// 			  .flags |= AO2_ITERATOR_DONTLOCK;
			
			
			PBX_CHANNEL_TYPE *remotePeer;

			for (; (remotePeer = ast_channel_iterator_next(iterator)); ast_channel_unref(remotePeer)) {

				if (pbx_find_channel_by_linkid(remotePeer, (void *)ast->linkedid)) {

					get_skinnyFormats(remotePeer->nativeformats, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));

					char cap_buf[512];

					sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));

					ast_log(LOG_WARNING, "remote caps: %s\n", cap_buf);

					ast_channel_unref(remotePeer);

					break;
				}

			}
			ast_channel_iterator_destroy(iterator);

//                      PBX_CHANNEL_TYPE *remotePeer = ast_channel_search_locked(pbx_find_channel_by_linkid, c->owner->linkedid); 
//                      if(remotePeer){
//                              ast_log(LOG_NOTICE, "SCCP: Found remote peer %s\n", remotePeer->name);
//                              
//                              char cap_buf[512];
//                              sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
//              
//                              ast_log(LOG_WARNING, "remote caps: %s\n", cap_buf);
//                              pbx_channel_unlock(remotePeer);
//                      }else{
//                              ast_log(LOG_NOTICE, "SCCP: No remote peer\n");
//                      }

		}
		break;
	case AST_CONTROL_BUSY:
		sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_BUSY);
		break;
	case AST_CONTROL_CONGESTION:
		sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_CONGESTION);
		break;
	case AST_CONTROL_PROGRESS:
		sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_PROGRESS);
		res = -1;
		break;
	case AST_CONTROL_PROCEEDING:
		sccp_indicate_locked(sccp_channel_getDevice(c), c, SCCP_CHANNELSTATE_PROCEED);
		res = -1;
		break;

	case AST_CONTROL_SRCCHANGE:
		if (c->rtp.audio.rtp)
			ast_rtp_instance_change_source(c->rtp.audio.rtp);
		//RTP_NEW_SOURCE(c, "Source Change");
		res = 0;
		break;

	case AST_CONTROL_SRCUPDATE:
		/* Source media has changed. */
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Source UPDATE request\n");

		/* update channel format */
		//! \todo use skinny codecs
//              oldChannelFormat = c->format;
		//c->format = ast->rawreadformat & AST_FORMAT_AUDIO_MASK;

//              if (oldChannelFormat != c->format) {
//                      /* notify of changing format */
//                      char s1[512], s2[512];
// 
//                      ast_log(LOG_NOTICE, "SCCP: SCCP/%s-%08x, changing format from: %s(%lu) to: %s(%lu) \n", c->line->name, c->callid, pbx_getformatname_multiple(s1, sizeof(s1) - 1, c->format), (ULONG)ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->rawreadformat), (ULONG)ast->rawreadformat);
//                      ast_set_read_format(ast, c->format);
//                      ast_set_write_format(ast, c->format);
//              }

		//! \todo reenable it
//              ast_log(LOG_NOTICE, "SCCP: SCCP/%s-%08x, state: %s(%d) \n", c->line->name, c->callid, sccp_indicate2str(c->state), c->state);
//              if (c->rtp.audio.rtp) {
//                      if (oldChannelFormat != c->format) {
//                              if (c->mediaStatus.receive == TRUE || c->mediaStatus.transmit == TRUE) {
//                                      sccp_channel_closereceivechannel_locked(c);     /* close the already openend receivechannel */
//                                      sccp_channel_openreceivechannel_locked(c);      /* reopen it */
//                              }
//                      }
//              }

                //! \todo handle COLP
		if (c->rtp.audio.rtp)
			ast_rtp_instance_change_source(c->rtp.audio.rtp);
		res = 0;
		break;

		/* when the bridged channel hold/unhold the call we are notified here */
	case AST_CONTROL_HOLD:
		ast_moh_start(ast, data, c->musicclass);
		res = 0;
		break;
	case AST_CONTROL_UNHOLD:
		ast_moh_stop(ast);

		if (c->rtp.audio.rtp)
			ast_rtp_instance_update_source(c->rtp.audio.rtp);
		//RTP_NEW_SOURCE(c, "Source Update");
		res = 0;
		break;

	case AST_CONTROL_CONNECTED_LINE:
		sccp_wrapper_asterisk111_connectedline(c, data, datalen);
		sccp_indicate_locked(sccp_channel_getDevice(c), c, c->state);
		
		res = 0;
		break;

	case AST_CONTROL_VIDUPDATE:						/* Request a video frame update */
		if (c->rtp.video.rtp) {
			res = 0;
		} else
			res = -1;
		break;
	case AST_CONTROL_INCOMPLETE:						/*!< Indication that the extension dialed is incomplete */
	        /* \todo implement dial continuation by:
	         *  - display message incomplete number
	         *  - adding time to channel->scheduler.digittimeout
	         *  - rescheduling sccp_pbx_sched_dial 
                 */
		res = 0;
		break;
	case -1:								// Asterisk prod the channel
		res = -1;
		break;
	default:
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: Don't know how to indicate condition %d\n", ind);
		res = -1;
	}
	//ast_cond_signal(&c->astStateCond);
	sccp_channel_unlock(c);

	sccp_log((DEBUGCAT_PBX | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "SCCP: send asterisk result %d\n", res);
	return res;
}

/*!
 * \brief Write to an Asterisk Channel
 * \param ast Channel as ast_channel
 * \param frame Frame as ast_frame
 *
 * \called_from_asterisk
 */
static int sccp_wrapper_asterisk111_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE *frame)
{
	int res = 0;
//	skinny_codec_t codec;
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	if (c) {
		switch (frame->frametype) {
		case AST_FRAME_VOICE:
			// checking for samples to transmit
			if (!frame->samples) {
				if (strcasecmp(frame->src, "ast_prod")) {
					ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", DEV_ID_LOG(sccp_channel_getDevice(c)), (int)frame->frametype);
				} else {
					// frame->samples == 0  when frame_src is ast_prod
					sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", DEV_ID_LOG(sccp_channel_getDevice(c)), ast->name);
				}
// 			} else if (!(frame->subclass.codec & ast->nativeformats)) {
			} else if (!(ast_format_cap_iscompatible(ast->nativeformats, &frame->subclass.format))) {
			
//                              char s1[512], s2[512], s3[512];
//
//                              ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), frame->subclass, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (ULONG)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (ULONG)ast->writeformat), (ULONG)ast->writeformat);
				//! \todo correct debugging
//                              ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), (int)frame->frametype, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (ULONG)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (ULONG)ast->writeformat), (ULONG)ast->writeformat);
				//return -1;
			}
			if (c->rtp.audio.rtp) {
				res = ast_rtp_instance_write(c->rtp.audio.rtp, frame);
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if ((c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) == 0 && c->rtp.video.rtp && sccp_channel_getDevice(c)
			    //      && (sccp_channel_getDevice(c)->capability & frame->subclass)
			    ) {
				int codec = pbx_codec2skinny_codec( (frame->subclass.codec & AST_FORMAT_VIDEO_MASK));
				ast_log(LOG_NOTICE, "%s: got video frame %d\n", DEV_ID_LOG(sccp_channel_getDevice(c)), codec);
				if(0 != codec){
					c->rtp.video.writeFormat = codec;
					sccp_channel_openMultiMediaChannel(c);
				}
			}

			if (c->rtp.video.rtp && (c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) != 0) {
				res = ast_rtp_instance_write(c->rtp.video.rtp, frame);
			}
#endif
			break;
		case AST_FRAME_TEXT:
		case AST_FRAME_MODEM:
		default:
			ast_log(LOG_WARNING, "%s: Can't send %d type frames with SCCP write on channel %s\n", DEV_ID_LOG(sccp_channel_getDevice(c)), frame->frametype, ast->name);
			break;
		}
	}
	return res;
}

static int sccp_wrapper_asterisk111_sendDigits(const sccp_channel_t * channel, const char *digits)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int i;
	PBX_FRAME_TYPE f;

	f = ast_null_frame;
	
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digits %s\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
									// CS_AST_NEW_FRAME_STRUCT
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass.integer = digits[i];
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), digits[i]);
		
		f.frametype = AST_FRAME_DTMF_BEGIN;
		ast_queue_frame(pbx_channel, &f);
		
		f.frametype = AST_FRAME_DTMF_END;
		ast_queue_frame(pbx_channel, &f);
	}
	return 1;
}

static int sccp_wrapper_asterisk111_sendDigit(const sccp_channel_t * channel, const char digit)
{
	char digits[2] = "\0\0";
	digits[0] = digit;
	return sccp_wrapper_asterisk111_sendDigits(channel, digits);
}

static void sccp_wrapper_asterisk111_setCalleridPresence(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

	if (CALLERID_PRESENCE_FORBIDDEN == channel->callInfo.presentation) {
		pbx_channel->caller.id.name.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
		pbx_channel->caller.id.number.presentation |= AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED;
	}
}

static int sccp_wrapper_asterisk111_setNativeAudioFormats(const sccp_channel_t * channel, skinny_codec_t codec[], int length)
{
	struct ast_format fmt;
	
	
	ast_format_cap_remove_bytype(channel->owner->nativeformats, AST_FORMAT_TYPE_AUDIO);
	
	/* while setting nativeformats to multiple value, asterisk will not transcode it */
#ifdef CS_EXPERIMENTAL
	int i;

	for (i = 0; i < length; i++) {
// 		channel->owner->nativeformats |= skinny_codec2pbx_codec(codec[i]);
		ast_format_set(&fmt, skinny_codec2pbx_codec(codec[i]), 0);
		ast_format_cap_add(channel->owner->nativeformats, &fmt);
	}
#else
// 	channel->owner->nativeformats = skinny_codec2pbx_codec(codec[0]);
	ast_format_set(&fmt, skinny_codec2pbx_codec(codec[0]), 0);
	ast_format_cap_add(channel->owner->nativeformats, &fmt);
#endif

	char s1[512];
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: set native Formats to %s, skinny: %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), ast_getformatname_multiple(s1, sizeof(s1), channel->owner->nativeformats), codec[0]);
	return 1;
}

static int sccp_wrapper_asterisk111_setNativeVideoFormats(const sccp_channel_t * channel, uint32_t formats)
{

	return 1;
}

static boolean_t sccp_wrapper_asterisk111_allocPBXChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{
	*pbx_channel = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, channel->line->cid_num, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);
	
	if (*pbx_channel == NULL)
		return FALSE;
	
	if (!channel || !channel->line) {
		return FALSE;
	}
	sccp_line_t *line=channel->line;

	(*pbx_channel)->tech = &sccp_tech;
	(*pbx_channel)->tech_pvt = &channel;
	//! \todo we already did this during the channel allocation, except asterisk 1.2
	//! \todo this should be moved to pbx implementation
	sccp_copy_string((*pbx_channel)->context, line->context, sizeof((*pbx_channel)->context));

	if (!sccp_strlen_zero(line->language))
		ast_string_field_set(*pbx_channel, language, line->language);

	if (!sccp_strlen_zero(line->accountcode))
		ast_string_field_set(*pbx_channel, accountcode, line->accountcode);

	if (!sccp_strlen_zero(line->musicclass))
		ast_string_field_set(*pbx_channel, musicclass, line->musicclass);

	if (line->amaflags)
		(*pbx_channel)->amaflags = line->amaflags;
	if (line->callgroup)
		(*pbx_channel)->callgroup = line->callgroup;
	if (line->pickupgroup)
		(*pbx_channel)->pickupgroup = line->pickupgroup;
	(*pbx_channel)->priority = 1;

	return TRUE;
}

static int sccp_wrapper_asterisk111_hangup(PBX_CHANNEL_TYPE * ast_channel)
{
	sccp_channel_t *c;
	int res;

	c = get_sccp_channel_from_ast_channel(ast_channel);
	if (!c)
		return -1;

	if (ast_test_flag(ast_channel, AST_FLAG_ANSWERED_ELSEWHERE) || ast_channel->hangupcause == AST_CAUSE_ANSWERED_ELSEWHERE) {
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: This call was answered elsewhere");
		c->answered_elsewhere = TRUE;
	}

	CS_AST_CHANNEL_PVT(ast_channel) = NULL;

	sccp_channel_lock(c);
	res = sccp_pbx_hangup_locked(c);
	sccp_channel_unlock(c);

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
static void *sccp_wrapper_asterisk111_park_thread(void *data)
{
	struct parkingThreadArg *arg;
	PBX_FRAME_TYPE *f;

	char extstr[20];
	int ext;
	int res;

	arg = data;

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
static sccp_parkresult_t sccp_wrapper_asterisk111_park(const sccp_channel_t * hostChannel)
{

	pthread_t th;
	struct parkingThreadArg *arg;
	PBX_CHANNEL_TYPE *pbx_bridgedChannelClone, *pbx_hostChannelClone;
	PBX_CHANNEL_TYPE *bridgedChannel = NULL;

	bridgedChannel = ast_bridged_channel(hostChannel->owner);

	if (!bridgedChannel) {
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: No PBX channel for parking");
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
			ast_log(LOG_ERROR, "while doing masquerade\n");
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
	if (!(arg = ast_calloc(1, sizeof(struct parkingThreadArg)))) {
		return PARK_RESULT_FAIL;
	}

	arg->bridgedChannel = pbx_bridgedChannelClone;
	arg->hostChannel = pbx_hostChannelClone;
	arg->device = sccp_channel_getDevice(hostChannel);

	if (!ast_pthread_create_detached_background(&th, NULL, sccp_wrapper_asterisk111_park_thread, arg)) {
		return PARK_RESULT_SUCCESS;
	}
	return PARK_RESULT_FAIL;

}

static uint8_t sccp_wrapper_asterisk111_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	struct ast_format astCodec;
	int payload;

	ast_format_set(&astCodec, skinny_codec2pbx_codec(codec), 0);
// 	payload = ast_rtp_codecs_payload_code(ast_rtp_instance_get_codecs(rtp->rtp), 1, astCodec);
	payload = ast_rtp_codecs_payload_code(ast_rtp_instance_get_codecs(rtp->rtp), 1, &astCodec, 0);
	
	return payload;
}

static uint8_t sccp_wrapper_asterisk111_get_sampleRate(skinny_codec_t codec)
{
	struct ast_format astCodec;

	ast_format_set(&astCodec, skinny_codec2pbx_codec(codec), 0);
	return ast_rtp_lookup_sample_rate2(1, &astCodec, 0);
}

static sccp_extension_status_t sccp_wrapper_asterisk111_extensionStatus(const sccp_channel_t * channel)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int ignore_pat = ast_ignore_pattern(pbx_channel->context, channel->dialedNumber);
	int ext_exist = ast_exists_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_canmatch = ast_canmatch_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);
	int ext_matchmore = ast_matchmore_extension(pbx_channel, pbx_channel->context, channel->dialedNumber, 1, channel->line->cid_num);

	sccp_log(1) (VERBOSE_PREFIX_1 "SCCP: extension helper says that:\n" "ignore pattern  : %d\n" "exten_exists    : %d\n" "exten_canmatch  : %d\n" "exten_matchmore : %d\n", ignore_pat, ext_exist, ext_canmatch, ext_matchmore);
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

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk111_request(const char *type, struct ast_format_cap *format, const PBX_CHANNEL_TYPE * requestor, void *data, int *cause)
{
	PBX_CHANNEL_TYPE *pbx_channel = NULL;
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
		ast_log(LOG_NOTICE, "Attempt to call with unspecified type of channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	}
	
	if (!data) {
		ast_log(LOG_NOTICE, "Attempt to call SCCP/ failed\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	}
	/* we leave the data unchanged */
	lineName = strdup(data);
	/* parsing options string */
	char *options = NULL;
	int optc = 0;
	char *optv[2];
	int opti = 0;

	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked us to create a channel with type=%s, format=" UI64FMT ", lineName=%s, options=%s\n", type, (ULONG)format, lineName, (options) ? options : "");
	
	/* parse options */
	if (options && (optc = sccp_app_separate_args(options, '/', optv, sizeof(optv) / sizeof(optv[0])))) {
		ast_log(LOG_NOTICE, "parse options\n");
		for (opti = 0; opti < optc; opti++) {
			ast_log(LOG_NOTICE, "parse option '%s'\n", optv[opti]);
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
					ast_log(LOG_NOTICE, "parsing aa\n");
					if (!strncasecmp(optv[opti], "1w", 2)) {
						autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti] += 2;
					} else if (!strncasecmp(optv[opti], "2w", 2)) {
						autoanswer_type = SCCP_AUTOANSWER_2W;
						ast_log(LOG_NOTICE, "set aa to 2w\n");
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
				ast_log(LOG_WARNING, "Wrong option %s\n", optv[opti]);
			}
		}
	}
	
	/** getting remote capabilities */
	char cap_buf[512];
	
	/* audio capabilities */
	sccp_channel_t *remoteSccpChannel = get_sccp_channel_from_pbx_channel(requestor);
	if(remoteSccpChannel){
		uint8_t x,y,z;
		z = 0;
		/* shrink audioCapabilities to remote preferred/capable format */
		for(x=0; x < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->preferences.audio[x] != 0; x++){
			for(y=0; y < SKINNY_MAX_CAPABILITIES && remoteSccpChannel->capabilities.audio[y] != 0; y++){
				if(remoteSccpChannel->preferences.audio[x] == remoteSccpChannel->capabilities.audio[y] ){
					audioCapabilities[z++] = remoteSccpChannel->preferences.audio[x];
					break;
				}
			}
		}
	}else{
//                get_skinnyFormats(requestor->nativeformats & AST_FORMAT_AUDIO_MASK, audioCapabilities, ARRAY_LEN(audioCapabilities));
                get_skinnyFormats(requestor->nativeformats, audioCapabilities, ARRAY_LEN(audioCapabilities)); 	// replace AUDIO_MASK with AST_FORMAT_TYPE_AUDIO check
		// if no format match possible -> AST_CAUSE_BEARERCAPABILITY_NOTAVAIL
	}
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, audioCapabilities, ARRAY_LEN(audioCapabilities));
	ast_log(LOG_WARNING, "remote audio caps: %s\n", cap_buf);
	
	/* video capabilities */
//	get_skinnyFormats(requestor->nativeformats & AST_FORMAT_VIDEO_MASK, videoCapabilities, ARRAY_LEN(videoCapabilities));
	get_skinnyFormats(requestor->nativeformats, videoCapabilities, ARRAY_LEN(videoCapabilities));		//replace AUDIO_MASK with AST_FORMAT_TYPE_AUDIO check
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, videoCapabilities, ARRAY_LEN(videoCapabilities));
	ast_log(LOG_WARNING, "remote video caps: %s\n", cap_buf);
	/** done */
	
	/** get requested format */
	codec = pbx_codec2skinny_codec(ast_format_cap_to_old_bitfield(format));

	requestStatus = sccp_requestChannel(lineName, codec, audioCapabilities, ARRAY_LEN(audioCapabilities), autoanswer_type, autoanswer_cause, ringermode, &channel);
	if (requestStatus != SCCP_REQUEST_STATUS_SUCCESS) {
		//! \todo parse state
		ast_log(LOG_WARNING, "SCCP: sccp_requestChannel Status not Successfull\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	}
	
	if (!sccp_pbx_channel_allocate_locked(channel)) {
		//! \todo handle error in more detail
		ast_log(LOG_WARNING, "SCCP: Unable to allocate channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		goto EXITFUNC;
	} else {
		pbx_channel = channel->owner;
		/* set calling party */
		sccp_channel_set_callingparty(channel, requestor->caller.id.name.str, requestor->caller.id.number.str);
		sccp_channel_set_originalCalledparty(channel, requestor->redirecting.to.name.str, requestor->redirecting.to.number.str);
	}

	if (requestor->linkedid) {
		ast_string_field_set(channel->owner, linkedid, requestor->linkedid);
		ast_log(LOG_WARNING, "requestor->linkedid: %s\n", requestor->linkedid);
	}

	sccp_channel_unlock(channel);

EXITFUNC:
	if (lineName)
		sccp_free(lineName);
	sccp_restart_monitor();
	return (pbx_channel && channel && channel->owner && channel->owner==pbx_channel) ? pbx_channel : NULL;
}

static int sccp_wrapper_asterisk111_call(PBX_CHANNEL_TYPE * chan, char *addr, int timeout)
{
	//! \todo change this handling and split pbx and sccp handling -MC
	return sccp_pbx_call(chan, addr, timeout);
}

static int sccp_wrapper_asterisk111_answer(PBX_CHANNEL_TYPE * chan)
{
	sccp_channel_t *channel = get_sccp_channel_from_pbx_channel(chan);

	if (!channel)
		return -1;
	//! \todo change this handling and split pbx and sccp handling -MC
	return sccp_pbx_answer(channel);
}

/**
 * 
 * \todo update remote capabilities after fixup
 */
static int sccp_wrapper_asterisk111_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
{
	sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: we got a fixup request for %s\n", newchan->name);
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(newchan);

	if (!c) {
		ast_log(LOG_WARNING, "sccp_pbx_fixup(old: %s(%p), new: %s(%p)). no SCCP channel to fix\n", oldchan->name, (void *)oldchan, newchan->name, (void *)newchan);
		return -1;
	}

	c->owner = newchan;
	return 0;
}

static enum ast_bridge_result sccp_wrapper_asterisk111_rtpBridge(PBX_CHANNEL_TYPE *c0, PBX_CHANNEL_TYPE *c1, int flags, PBX_FRAME_TYPE **fo, PBX_CHANNEL_TYPE **rc, int timeoutms)
{
	enum ast_bridge_result res;
	int new_flags = flags;

	/* \note temporarily marked out until we figure out how to get directrtp back on track - DdG */
	sccp_channel_t *sc0, *sc1;

	if ((sc0 = get_sccp_channel_from_pbx_channel(c0)) && (sc1 = get_sccp_channel_from_pbx_channel(c1))) {
		// Switch off DTMF between SCCP phones
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_0;
		new_flags &= !AST_BRIDGE_DTMF_CHANNEL_1;
		if ((sccp_channel_getDevice(sc0)->directrtp && sccp_channel_getDevice(sc1)->directrtp) || GLOB(directrtp)) {
			ast_channel_defer_dtmf(c0);
			ast_channel_defer_dtmf(c1);
		}
		sc0->peerIsSCCP = 1;
		sc1->peerIsSCCP = 1;
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

static enum ast_rtp_glue_result sccp_wrapper_asterisk111_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
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

static enum ast_rtp_glue_result sccp_wrapper_asterisk111_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
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

//static int sccp_wrapper_asterisk111_set_rtp_peer(PBX_CHANNEL_TYPE *ast, PBX_RTP_TYPE *rtp, PBX_RTP_TYPE *vrtp, PBX_RTP_TYPE *trtp, int codecs, int nat_active)
// static int sccp_wrapper_asterisk111_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, format_t codecs, int nat_active)
static int sccp_wrapper_asterisk111_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, const struct ast_format_cap *codecs, int nat_active)
{
	sccp_channel_t *c = NULL;
//	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;
	struct ast_sockaddr them;
	char buff[512];

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: __FILE__\n");
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return -1;
	}
//	if (!(l = c->line)) {
	if (!c->line) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO LINE\n");
		return -1;
	}
	if (!(d = sccp_channel_getDevice(c))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO DEVICE\n");
		return -1;
	}

	if (!d->directrtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Direct RTP disabled\n");
		return -1;
	}

	if (rtp) {
		ast_rtp_instance_get_remote_address(rtp, &them);
		//sccp_rtp_set_peer(c, &them); /*! \todo convert struct ast_sockaddr -> struct sockaddr_in */
		return 0;
	} else {
		/** \todo check remote codecs */
		if (ast->_state != AST_STATE_UP) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%s, nat=%d\n", ast_getformatname_multiple(buff, sizeof(buff), (struct ast_format_cap *)codecs), d->nat);
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%s, nat=%d\n", ast_getformatname_multiple(buff, sizeof(buff), (struct ast_format_cap *)codecs), d->nat);
		}
		return 0;
	}

	/* Need a return here to break the bridge */
	return 0;
}

/*
 * \brief get callerid_name from pbx
 * \param sccp_channle Asterisk Channel
 * \param cid name result
 * \return parse result
 */
static int sccp_wrapper_asterisk111_callerid_name(const sccp_channel_t * channel, char **cid_name)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.name.str && strlen(pbx_chan->caller.id.name.str) > 0) {
		*cid_name = strdup(pbx_chan->caller.id.name.str);
		return 1;
	}

	return 0;
}

static void sccp_wrapper_asterisk111_getCodec(PBX_CHANNEL_TYPE *ast, struct ast_format_cap *result)
{
	uint8_t i;
	sccp_channel_t *channel = NULL;
	struct ast_format fmt;
	
	if (!(channel = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return;
	}
	
	
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "asterisk requests format for channel\n");
	for (i = 0; i < ARRAY_LEN(channel->preferences.audio); i++) {
		ast_format_set(&fmt, skinny_codec2pbx_codec(channel->preferences.audio[i]), 0);
		ast_format_cap_add(result, &fmt);
	}
	
	return;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the caller number
 */
static int sccp_wrapper_asterisk111_callerid_number(const sccp_channel_t * channel, char **cid_number)
{
	PBX_CHANNEL_TYPE *pbx_chan = channel->owner;

	if (pbx_chan->caller.id.number.str && strlen(pbx_chan->caller.id.number.str) > 0) {
		*cid_number = strdup(pbx_chan->caller.id.number.str);
		return 1;
	}

	return 0;
}

static int sccp_wrapper_asterisk111_rtp_stop(sccp_channel_t * channel)
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

static boolean_t sccp_wrapper_asterisk111_create_audio_rtp(const sccp_channel_t * c, void **new_rtp)
{
	sccp_session_t *s;
	sccp_device_t *d = NULL;
	struct ast_sockaddr sock;
	struct ast_rtp_instance *rtp_instance;
	char pref_buf[128];

	//uint32_t astCodecPref = 0;                                            /*! \todo get asterist codec pref */
	struct ast_codec_pref astCodecPref;

	if (!c)
		return FALSE;
	d = sccp_channel_getDevice(c);
	if (d)
		s = d->session;
	else
		return FALSE;
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->ourip));
	if (c->rtp.audio.rtp) {
		ast_log(LOG_ERROR, "we already have a rtp server, why dont we use this?\n");
		return TRUE;
	}

	ast_sockaddr_from_sin(&sock, &GLOB(bindaddr));
	*new_rtp = rtp_instance = ast_rtp_instance_new("asterisk", sched, &sock, NULL);
	if (!*new_rtp) {
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: rtp created at %s:%d\n", DEV_ID_LOG(d), ast_sockaddr_stringify_host(&sock), ast_sockaddr_port(&sock));
	if (c->owner) {
		ast_rtp_instance_set_prop(rtp_instance, AST_RTP_PROPERTY_RTCP, 1);
		ast_channel_set_fd(c->owner, 0, ast_rtp_instance_fd(*new_rtp, 0));
		ast_channel_set_fd(c->owner, 1, ast_rtp_instance_fd(*new_rtp, 1));
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	ast_rtp_codecs_packetization_set(ast_rtp_instance_get_codecs(*new_rtp), *new_rtp, &astCodecPref);
	//ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
	sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	ast_rtp_instance_set_qos(*new_rtp, d->audio_tos, d->audio_cos, "SCCP RTP");
	struct ast_rtp_codecs *codecs = ast_rtp_instance_get_codecs(*new_rtp);
	uint8_t i;

	/* add payload mapping for skinny codecs */
	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		/* add audio codecs only */
		if (skinny_codecs[i].mimesubtype && skinny_codecs[i].codec_type == SKINNY_CODEC_TYPE_AUDIO) {
			ast_rtp_codecs_payloads_set_rtpmap_type_rate(codecs, NULL, skinny_codecs[i].codec, "audio", (char *)skinny_codecs[i].mimesubtype, 0, skinny_codecs[i].sample_rate);
		}
	}

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk111_create_video_rtp(const sccp_channel_t * c, void **new_rtp)
{
	sccp_session_t *s;
	sccp_device_t *d = NULL;
	struct ast_sockaddr sock;
	struct ast_rtp_instance *rtp_instance;

//      char pref_buf[128];

	//uint32_t astCodecPref = 0;                                            //! \todo get asterist codec pref
	struct ast_codec_pref astCodecPref;

	if (!c)
		return FALSE;
	d = sccp_channel_getDevice(c);
	if (d)
		s = d->session;
	else
		return FALSE;
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating vrtp server connection at %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->ourip));

	ast_sockaddr_from_sin(&sock, &GLOB(bindaddr));
	*new_rtp = rtp_instance = ast_rtp_instance_new("asterisk", sched, &sock, NULL);
	if (!*new_rtp) {
		return FALSE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: vrtp created at %s:%d\n", DEV_ID_LOG(d), ast_sockaddr_stringify_host(&sock), ast_sockaddr_port(&sock));
	if (c->owner) {
		ast_rtp_instance_set_prop(rtp_instance, AST_RTP_PROPERTY_RTCP, 1);

		ast_channel_set_fd(c->owner, 2, ast_rtp_instance_fd(*new_rtp, 0));
		ast_channel_set_fd(c->owner, 3, ast_rtp_instance_fd(*new_rtp, 1));

		ast_queue_frame(c->owner, &ast_null_frame);
	}

	ast_rtp_codecs_packetization_set(ast_rtp_instance_get_codecs(*new_rtp), *new_rtp, &astCodecPref);
	//ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);

	ast_rtp_instance_set_qos(*new_rtp, d->video_tos, d->video_cos, "SCCP VRTP");

	/* add payload mapping for skinny codecs */
	uint8_t i;
	struct ast_rtp_codecs *codecs = ast_rtp_instance_get_codecs(*new_rtp);

	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		/* add audio codecs only */
		if (skinny_codecs[i].mimesubtype && skinny_codecs[i].codec_type == SKINNY_CODEC_TYPE_VIDEO) {
			ast_rtp_codecs_payloads_set_rtpmap_type_rate(codecs, NULL, skinny_codecs[i].codec, "video", (char *)skinny_codecs[i].mimesubtype, 0, skinny_codecs[i].sample_rate);
		}
	}

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk111_destroyRTP(PBX_RTP_TYPE * rtp)
{
	int res;

	res = ast_rtp_instance_destroy(rtp);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk111_checkHangup(const sccp_channel_t * channel)
{
	int res;

	res = ast_check_hangup(channel->owner);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk111_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	struct ast_sockaddr addr;

	ast_rtp_instance_get_local_address(rtp, &addr);
	ast_sockaddr_to_sin(&addr, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk111_getChannelByName(const char *name, PBX_CHANNEL_TYPE * pbx_channel)
{

	pbx_channel = ast_channel_get_by_name(name);
	if (!pbx_channel)
		return FALSE;
	return TRUE;
}

static int sccp_wrapper_asterisk111_rtp_set_peer(const struct sccp_rtp *rtp, const struct sockaddr_in *new_peer, int nat_active)
{
	struct ast_sockaddr ast_sockaddr;
	int res;

	ast_sockaddr_from_sin(&ast_sockaddr, new_peer);
	ast_sockaddr_set_port(&ast_sockaddr, ntohs(new_peer->sin_port));
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Tell asterisk to send rtp media to %s:%d\n", ast_sockaddr_stringify_host(&ast_sockaddr), ast_sockaddr_port(&ast_sockaddr));
	res = ast_rtp_instance_set_remote_address(rtp->rtp, &ast_sockaddr);
	if (nat_active)
		ast_rtp_instance_set_prop(rtp->rtp, AST_RTP_PROPERTY_NAT, 1);
	return res;
}

static boolean_t sccp_wrapper_asterisk111_setWriteFormat(const sccp_channel_t *channel, skinny_codec_t codec)
{
	struct ast_format fmt;
	
	
	ast_format_set(&fmt, skinny_codec2pbx_codec(codec), 0);
	ast_format_copy(&channel->owner->writeformat, &fmt);
	ast_format_copy(&channel->owner->rawwriteformat, &fmt);
	ast_rtp_instance_set_write_format(channel->rtp.audio.rtp, &fmt);
	
// 	channel->owner->rawwriteformat = skinny_codec2pbx_codec(codec);
// 	channel->owner->writeformat = skinny_codec2pbx_codec(codec);
// 	ast_set_write_format(channel->owner, skinny_codec2pbx_codec(codec));

// 	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write native: %d\n", (int)channel->owner->rawwriteformat);
// 	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write: %d\n", (int)channel->owner->writeformat);

	
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk111_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{
// 	channel->owner->rawreadformat = skinny_codec2pbx_codec(codec);
// 	channel->owner->readformat = skinny_codec2pbx_codec(codec);

// 	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read native: %d\n", (int)channel->owner->rawreadformat);
// 	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read: %d\n", (int)channel->owner->readformat);

// 	ast_set_read_format(channel->owner, skinny_codec2pbx_codec(codec));
	
	struct ast_format fmt;
	
	ast_format_set(&fmt, skinny_codec2pbx_codec(codec), 0);
	ast_format_copy(&channel->owner->readformat, &fmt);
	ast_format_copy(&channel->owner->rawreadformat, &fmt);
	ast_rtp_instance_set_read_format(channel->rtp.audio.rtp, &fmt);
	
	return TRUE;
}

static void sccp_wrapper_asterisk111_setCalleridName(const sccp_channel_t * channel, const char *name)
{
	if (name) {
		channel->owner->caller.id.name.valid = 1;
		ast_party_name_free(&channel->owner->caller.id.name);
		channel->owner->caller.id.name.str = ast_strdup(name);
	}
}

static void sccp_wrapper_asterisk111_setCalleridNumber(const sccp_channel_t * channel, const char *number)
{
	if (number) {
		channel->owner->caller.id.number.valid = 1;
		ast_party_number_free(&channel->owner->caller.id.number);
		channel->owner->caller.id.number.str = ast_strdup(number);
	}
}

static void sccp_wrapper_asterisk111_setRedirectingParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	if (number) {
		channel->owner->redirecting.from.number.valid = 1;
		ast_party_number_free(&channel->owner->redirecting.from.number);
		channel->owner->redirecting.from.number.str = ast_strdup(number);
	}

	if (name) {
		channel->owner->redirecting.from.name.valid = 1;
		ast_party_name_free(&channel->owner->redirecting.from.name);
		channel->owner->redirecting.from.name.str = ast_strdup(name);
	}
}

static void sccp_wrapper_asterisk111_setRedirectedParty(const sccp_channel_t * channel, const char *number, const char *name)
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

static void sccp_wrapper_asterisk111_updateConnectedLine(const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason)
{
	struct ast_party_connected_line connected;
	struct ast_set_party_connected_line update_connected;

	memset(&update_connected, 0, sizeof(update_connected));

	if (number) {
		update_connected.id.number = 1;
		connected.id.number.valid = 1;
		connected.id.number.str = ast_strdup(number);
		connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}

	if (name) {
		update_connected.id.name = 1;
		connected.id.name.valid = 1;
		connected.id.name.str = ast_strdup(name);
		connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	}
	connected.id.tag = NULL;
	connected.source = reason;

	ast_channel_queue_connected_line_update(channel->owner, &connected, &update_connected);
}

static int sccp_wrapper_asterisk111_sched_add(int when, sccp_sched_cb callback, const void *data)
{
	return ast_sched_add(sched, when, callback, data);
}

static long sccp_wrapper_asterisk111_sched_when(int id)
{
	return ast_sched_when(sched, id);
}

static int sccp_wrapper_asterisk111_sched_wait(int id)
{
	return ast_sched_wait(sched);
}

static int sccp_wrapper_asterisk111_sched_del(int id)
{
	return ast_sched_del(sched, id);
}

static int sccp_wrapper_asterisk111_setCallState(const sccp_channel_t * channel, int state)
{
	sccp_pbx_setcallstate((sccp_channel_t *) channel, state);
	//! \todo implement correct return value (take into account negative deadlock prevention)
	return 0;
}

static int sccp_wrapper_asterisk111_requestHangup(PBX_CHANNEL_TYPE * channel)
{

	/* Is there a blocker ? */
	uint8_t res = (channel->pbx || ast_test_flag(channel, AST_FLAG_BLOCKING));

	channel->hangupcause = AST_CAUSE_NORMAL_CLEARING;
	if ((channel->_softhangup & AST_SOFTHANGUP_APPUNLOAD) != 0) {
		channel->hangupcause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
	} else {
		if (channel->masq != NULL) {					// we are masquerading
			channel->hangupcause = AST_CAUSE_ANSWERED_ELSEWHERE;
		}
	}

	if (res) {
		channel->_softhangup |= AST_SOFTHANGUP_DEV;
		ast_queue_hangup(channel);
		return 1;
	} else {
		ast_hangup(channel);
		return 2;
	}
}

/*!
 * \brief Send Text to Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param text Text to be send as char
 * \return Succes as int
 * 
 * \called_from_asterisk
 */
static int sccp_pbx_sendtext(PBX_CHANNEL_TYPE *ast, const char *text){
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	sccp_device_t *d;

	uint8_t instance;

	if (!c || !sccp_channel_getDevice(c))
		return -1;

	d = sccp_channel_getDevice(c);
	if (!text || sccp_strlen_zero(text))
		return 0;

	sccp_log(1) (VERBOSE_PREFIX_3 "%s: Sending text %s on %s\n", d->id, text, ast->name);

	instance = sccp_device_find_index_for_line(d, c->line->name);
	sccp_dev_displayprompt(d, instance, c->callid, (char *)text, 10);
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
static int sccp_wrapper_recvdigit_begin(PBX_CHANNEL_TYPE *ast, char digit)
{
	return -1;
}

/*!
 * \brief Receive Last Digit from Asterisk Channel
 * \param ast Asterisk Channel as ast_channel
 * \param digit Last Digit as char
 * \param duration Duration as int
 * \return Always Return -1 as int
 * \todo FIXME Always returns -1
 * 
 * \called_from_asterisk
 */
static int sccp_wrapper_recvdigit_end(PBX_CHANNEL_TYPE *ast, char digit, unsigned int duration)
{
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	sccp_device_t *d = NULL;

	return -1;

	if (!c || !sccp_channel_getDevice(c))
		return -1;

	d = sccp_channel_getDevice(c);

	sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to send dtmf '%d' to channel %s. Trying to send it %s\n", digit, ast->name, (d->dtmfmode) ? "outofband" : "inband");

	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_log(1) (VERBOSE_PREFIX_3 "%s: Can't send the dtmf '%d' %c to a not connected channel %s\n", d->id, digit, digit, ast->name);
		return -1;
	}

	sccp_dev_keypadbutton(d, digit, sccp_device_find_index_for_line(d, c->line->name), c->callid);

	return 0;
}

/*!
 * \brief ACF Channel Read callback
 *
 * \param ast Asterisk Channel
 * \param funcname	functionname as const char *
 * \param args		arguments as char *
 * \param buf		buffer as char *
 * \param buflen 	bufferlenght as size_t
 * \return result as int
 *
 * \called_from_asterisk
 * 
 * \test ACF Channel Read Needs to be tested
 */
static int sccp_wrapper_asterisk111_channel_read(struct ast_channel *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen)
{
	sccp_channel_t *c;


	if (!ast || ast->tech != &sccp_tech) {
		ast_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}

	c = get_sccp_channel_from_ast_channel(ast);
	if (c == NULL)
		return -1;

	if (!strcasecmp(args, "peerip")) {
		sccp_copy_string(buf, c->rtp.audio.phone_remote.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr) : "", buflen);
	} else if (!strcasecmp(args, "recvip")) {
		sccp_copy_string(buf, c->rtp.audio.phone.sin_addr.s_addr ? pbx_inet_ntoa(c->rtp.audio.phone.sin_addr) : "", buflen);
	} else if (!strcasecmp(args, "from") && sccp_channel_getDevice(c)) {
		sccp_copy_string(buf, (char *)sccp_channel_getDevice(c)->id, buflen);
	} else {
		return -1;
	}
	return 0;
}

static int sccp_wrapper_asterisk111_channel_write(struct ast_channel *ast, const char *funcname, char *args, const char *value){
	sccp_channel_t *c;
	boolean_t	res;


	if (!ast || ast->tech != &sccp_tech) {
		ast_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}

	c = get_sccp_channel_from_ast_channel(ast);
	
	if (!strcasecmp(args, "MaxCallBR")) {
		sccp_log(1) (VERBOSE_PREFIX_3 "%s: set max call bitrate to %s\n", DEV_ID_LOG(sccp_channel_getDevice(c)), value);
		pbx_builtin_setvar_helper(ast, "_MaxCallBR", value);
		res = TRUE;
	} else if (!strcasecmp(args, "codec")) {
		res = sccp_channel_setPreferredCodec(c, value);
	} else {
		return -1;
	}
	
	return res ? 0 : -1;
}

/*! \brief Set an option on a asterisk channel */
static int sccp_wrapper_asterisk111_setOption(struct ast_channel *ast, int option, void *data, int datalen)
{
	int res = -1;
	sccp_channel_t *c = get_sccp_channel_from_ast_channel(ast);

        if (!c) {
        	return -1;
        }

	switch (option) {
	case AST_OPTION_FORMAT_READ:
		if (c->rtp.audio.rtp) {
			res = ast_rtp_instance_set_read_format(c->rtp.audio.rtp, (struct ast_format *) data);
		}
		break;
	case AST_OPTION_FORMAT_WRITE:
		if (c->rtp.audio.rtp) {
			res = ast_rtp_instance_set_write_format(c->rtp.audio.rtp, (struct ast_format *) data);
		}
		break;
	case AST_OPTION_MAKE_COMPATIBLE:
		if (c->rtp.audio.rtp) {
			res = ast_rtp_instance_make_compatible(ast, c->rtp.audio.rtp, (struct ast_channel *) data);
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

	return res;
}

static const char *sccp_wrapper_asterisk111_getChannelLinkId(const sccp_channel_t *channel){
	static const char *emptyLinkId = "--no-linkedid--";
	
	if(channel->owner){
		return channel->owner->linkedid;
	}
	return emptyLinkId;
}

/*!
 * \brief using RTP Glue Engine
 */
struct ast_rtp_glue sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk111_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk111_get_vrtp_peer,
	.update_peer = sccp_wrapper_asterisk111_set_rtp_peer,
	.get_codec = sccp_wrapper_asterisk111_getCodec,
};

/*!
 * \brief SCCP Tech Structure
 */
struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */
	.type 			= SCCP_TECHTYPE_STR,
	.description 		= "Skinny Client Control Protocol (SCCP)",

	//.capabilities = NULL, /** this is don in load_module *
	.properties 		= AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester 		= sccp_wrapper_asterisk111_request,
	.devicestate 		= sccp_devicestate,
	.call 			= sccp_wrapper_asterisk111_call,
	.hangup 		= sccp_wrapper_asterisk111_hangup,
	.answer 		= sccp_wrapper_asterisk111_answer,
	.read 			= sccp_wrapper_asterisk111_rtp_read,
	.write 			= sccp_wrapper_asterisk111_rtp_write,
	.write_video 		= sccp_wrapper_asterisk111_rtp_write,
	.indicate 		= sccp_wrapper_asterisk111_indicate,
	.fixup 			= sccp_wrapper_asterisk111_fixup,
	.transfer		= sccp_pbx_transfer,

	//! \todo implement asterisk extension for commented functions
	.bridge 		= sccp_wrapper_asterisk111_rtpBridge,
	//.early_bridge         = ast_rtp_early_bridge,
	//.bridged_channel	=
	
	.send_text 		= sccp_pbx_sendtext,
	//.send_html		=
	//.send_image		=

	.func_channel_read 	= sccp_wrapper_asterisk111_channel_read,
	.func_channel_write 	= sccp_wrapper_asterisk111_channel_write,

	.send_digit_begin	= sccp_wrapper_recvdigit_begin,
	.send_digit_end 	= sccp_wrapper_recvdigit_end,
	
	//.write_text           = 
	//.write_video		=
	//.cc_callback          =                                              // ccss, new >1.6.0
	//.exception            =                                              // new >1.6.0
	.setoption 		= sccp_wrapper_asterisk111_setOption,
	//.queryoption          =                                              // new >1.6.0
	//.get_pvt_uniqueid     = sccp_pbx_get_callid,                         // new >1.6.0
	//.get_base_channel	=
	//.set_base_channel	=
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP - PBX Callback Functions 
 * (Decoupling Tight Dependencies on Asterisk Functions)
 */
struct sccp_pbx_cb sccp_pbx = {
	/* *INDENT-OFF* */
  
        /* channel */
	.alloc_pbxChannel 		= sccp_wrapper_asterisk111_allocPBXChannel,
	.requestHangup 			= sccp_wrapper_asterisk111_requestHangup,
	.extension_status 		= sccp_wrapper_asterisk111_extensionStatus,
	.getChannelByName 		= sccp_wrapper_asterisk111_getChannelByName,
	.getChannelLinkId		= sccp_wrapper_asterisk111_getChannelLinkId,
	.checkhangup			= sccp_wrapper_asterisk111_checkHangup,
	
	/* digits */
	.send_digits 			= sccp_wrapper_asterisk111_sendDigits,
	.send_digit 			= sccp_wrapper_asterisk111_sendDigit,

	/* schedulers */
	.sched_add 			= sccp_wrapper_asterisk111_sched_add,
	.sched_del 			= sccp_wrapper_asterisk111_sched_del,
	.sched_when 			= sccp_wrapper_asterisk111_sched_when,
	.sched_wait 			= sccp_wrapper_asterisk111_sched_wait,
	
	/* callstate / indicate */
	.set_callstate 			= sccp_wrapper_asterisk111_setCallState,

	/* codecs */
	.set_nativeAudioFormats 	= sccp_wrapper_asterisk111_setNativeAudioFormats,
	.set_nativeVideoFormats 	= sccp_wrapper_asterisk111_setNativeVideoFormats,

	/* rtp */
	.rtp_getUs 			= sccp_wrapper_asterisk111_rtpGetUs,
	.rtp_stop 			= sccp_wrapper_asterisk111_rtp_stop,
	.rtp_audio_create 		= sccp_wrapper_asterisk111_create_audio_rtp,
	.rtp_video_create 		= sccp_wrapper_asterisk111_create_video_rtp,
	.rtp_get_payloadType 		= sccp_wrapper_asterisk111_get_payloadType,
	.rtp_get_sampleRate 		= sccp_wrapper_asterisk111_get_sampleRate,
	.rtp_destroy 			= sccp_wrapper_asterisk111_destroyRTP,
	.rtp_setWriteFormat 		= sccp_wrapper_asterisk111_setWriteFormat,
	.rtp_setReadFormat 		= sccp_wrapper_asterisk111_setReadFormat,
	.rtp_setPeer 			= sccp_wrapper_asterisk111_rtp_set_peer,

	/* callerid */
	.get_callerid_name 		= sccp_wrapper_asterisk111_callerid_name,
	.get_callerid_number 		= sccp_wrapper_asterisk111_callerid_number,
	.get_callerid_dnid 		= NULL,						//! \todo implement callback
	.get_callerid_rdnis 		= NULL,						//! \todo implement callback
	.get_callerid_presence 		= NULL,						//! \todo implement callback
	.set_callerid_name 		= sccp_wrapper_asterisk111_setCalleridName,	//! \todo implement callback
	.set_callerid_number 		= sccp_wrapper_asterisk111_setCalleridNumber,	//! \todo implement callback
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_wrapper_asterisk111_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_wrapper_asterisk111_setRedirectedParty,
	.set_callerid_presence 		= sccp_wrapper_asterisk111_setCalleridPresence,
	.set_connected_line		= sccp_wrapper_asterisk111_updateConnectedLine,
	
	
	/* database */
	.feature_addToDatabase 		= sccp_asterisk_addToDatabase,
	.feature_getFromDatabase 	= sccp_asterisk_getFromDatabase,
	.feature_removeFromDatabase     = sccp_asterisk_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_asterisk_removeTreeFromDatabase,
	
	
	.feature_park			= sccp_wrapper_asterisk111_park,
	.feature_pickup			= NULL, //! \todo implement callback feature_pickup
	
	/* *INDENT-ON* */
};

static int load_module(void)
{

	boolean_t res;

	/* check for existance of chan_skinny */
	if (ast_module_check("chan_skinny.so")) {
		pbx_log(LOG_ERROR, "Chan_skinny is loaded. Please check modules.conf and remove chan_skinny before loading chan_sccp.\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	res = sccp_prePBXLoad();
	/* make globals */
	if (!res) {
                return AST_MODULE_LOAD_DECLINE;
	}
	
	sccp_tech.capabilities = ast_format_cap_alloc();
	ast_format_cap_add_all_by_type(sccp_tech.capabilities, AST_FORMAT_TYPE_AUDIO);

	sched = ast_sched_context_create();
	if (!sched) {
		ast_log(LOG_WARNING, "Unable to create schedule context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}
	io = io_context_create();
	if (!io) {
		ast_log(LOG_WARNING, "Unable to create I/O context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}
	//! \todo how can we handle this in a pbx independent way?
	if (!load_config()) {
		if (ast_channel_register(&sccp_tech)) {
			ast_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			return -1;
		}
	}

	ast_rtp_glue_register(&sccp_rtp);
	sccp_register_management();
	sccp_register_cli();
	sccp_register_dialplan_functions();
	/* And start the monitor for the first time */
	sccp_restart_monitor();
	sccp_postPBX_load();
	return 0;
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
	ast_rtp_glue_unregister(&sccp_rtp);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP Channel Tech\n");
	ast_channel_unregister(&sccp_tech);
	sccp_unregister_dialplan_functions();
	sccp_unregister_cli();
	sccp_mwi_module_stop();
	sccp_hint_module_stop();
#ifdef CS_SCCP_MANAGER
	sccp_unregister_management();
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
	sccp_globals_unlock(monitor_lock);
	sccp_mutex_destroy(&GLOB(monitor_lock));
	
	if (io) {
		io_context_destroy(io);
		io = NULL;
	}
	if (sched) {
		ast_sched_context_destroy(sched);
		sched = NULL;
	}
	sccp_free(sccp_globals);
	ast_log(LOG_NOTICE, "Running Cleanup\n");
	ast_format_cap_destroy(sccp_tech.capabilities);
	
#ifdef HAVE_LIBGC
//	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Collect a little:%d\n",GC_collect_a_little());
//	CHECK_LEAKS();
//      GC_gcollect();
#endif
	ast_log(LOG_NOTICE, "Module chan_sccp unloaded\n");
	return 0;
}

static int reload(void)
{
	sccp_reload();
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "', NULL)",
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
	.load_pri = AST_MODPRI_CHANNEL_DRIVER,
	.nonoptreq = "res_rtp_asterisk,chan_local",
    );
