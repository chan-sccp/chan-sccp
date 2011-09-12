
/*!
 * \file 	sccp_ast106.c
 * \brief 	SCCP PBX Asterisk Wrapper Class
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */


#include "../../config.h"
#include "../../common.h"
#include "ast106.h"
#include <asterisk/sched.h>
#include <asterisk/netsock.h>

struct sched_context *sched = 0;
struct io_context *io = 0;

#define SCCP_AST_LINKID_HELPER "__LINKID"

#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
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

static void get_skinnyFormats(format_t format, skinny_codec_t codecs[], size_t size)
{
	unsigned int x;
	unsigned len = 0;

	if (!size)
		return;

	for (x = 0; x < ARRAY_LEN(skinny2pbx_codec_maps) && len <= size; x++) {
		if (skinny2pbx_codec_maps[x].pbx_codec & format) {
			codecs[len++] = skinny2pbx_codec_maps[x].skinny_codec;
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "map ast codec %lu to %d\n", (ULONG)(skinny2pbx_codec_maps[x].pbx_codec & format), skinny2pbx_codec_maps[x].skinny_codec);
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
#if ASTERISK_VERSION_NUMBER >= 10400
	return ast_getformatname_multiple(buf, size, format & AST_FORMAT_AUDIO_MASK);
#else
	return ast_getformatname_multiple(buf, size, format);
#endif										// ASTERISK_VERSION_NUMBER
}

/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 * \param ast_chan Asterisk Channel
 * \return SCCP Channel on Success or Null on Fail
 */
static sccp_channel_t *get_sccp_channel_from_ast_channel(PBX_CHANNEL_TYPE *ast_chan)
{
#ifndef CS_AST_HAS_TECH_PVT
	if (!strncasecmp(ast_chan->type, "SCCP", 4)) {
#else
	if (!strncasecmp(ast_chan->tech->type, "SCCP", 4)) {
#endif
		return CS_AST_CHANNEL_PVT(ast_chan);
	} else {
		return NULL;
	}
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
static PBX_FRAME_TYPE *sccp_wrapper_asterisk16_rtp_read(PBX_CHANNEL_TYPE * ast)
{
	sccp_channel_t *c = get_sccp_channel_from_pbx_channel(ast);

	PBX_FRAME_TYPE *frame;

	frame = &ast_null_frame;
	// ASTERISK_VERSION_NUMBER >= 10400

	if (!c || !c->rtp.audio.rtp)
		return frame;

	switch (ast->fdno) {

	case 0:
		frame = ast_rtp_read(c->rtp.audio.rtp);				/* RTP Audio */
		break;
	case 1:
		frame = ast_rtcp_read(c->rtp.audio.rtp);			/* RTCP Control Channel */
		break;
	case 2:
#ifdef CS_SCCP_VIDEO
		frame = ast_rtp_read(c->rtp.video.rtp);				/* RTP Video */
#endif
		break;
	case 3:
#ifdef CS_SCCP_VIDEO
		frame = ast_rtcp_read(c->rtp.video.rtp);			/* RTCP Control Channel for video */
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

		if (!(frame->subclass & (ast->nativeformats & AST_FORMAT_AUDIO_MASK)))	
		{
			//sccp_log(1) (VERBOSE_PREFIX_3 "%s: Channel %s changed format from %s(%d) to %s(%d)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), ast->name, pbx_getformatname(ast->nativeformats), ast->nativeformats, pbx_getformatname(frame->subclass), frame->subclass);
			ast->nativeformats = frame->subclass;
			ast_set_read_format(ast, ast->readformat);
			ast_set_write_format(ast, ast->writeformat);
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
/*
static int pbx_find_channel_by_linkid(PBX_CHANNEL_TYPE *ast, void *data)
{
	char *linkId = data;

	if (!data)
		return 0;

	ast_log(LOG_NOTICE, "SCCP: peer name: %s, linkId: %s\n", ast->name ? ast->name : "(null)", ast->linkedid ? ast->linkedid : "(null)");

	return !ast->pbx && ast->linkedid && (!strcasecmp(ast->linkedid, linkId)) && !ast->masq;
}
*/


static int sccp_wrapper_asterisk16_indicate(PBX_CHANNEL_TYPE * ast, int ind, const void *data, size_t datalen)
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

/*
			target = sccp_search_remotepeer_locked(pbx_find_channel_by_linkedid, c->owner->linkedid);
			if (target) {
				get_skinnyFormats(remotePeer->nativeformats, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
				char cap_buf[512];
				sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
				ast_log(LOG_WARNING, "remote caps: %s\n", cap_buf);
				ast_channel_unref(remotePeer);
                        }else{
                                ast_log(LOG_NOTICE, "SCCP: No remote peer\n");
			}
*/

/*                        PBX_CHANNEL_TYPE *remotePeer = ast_channel_search_locked(pbx_find_channel_by_linkid, c->owner->linkedid); 
                        if(remotePeer){
				get_skinnyFormats(remotePeer->nativeformats, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
                                ast_log(LOG_NOTICE, "SCCP: Found remote peer %s\n", remotePeer->name);
                                char cap_buf[512];
                                sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, c->remoteCapabilities.audio, ARRAY_LEN(c->remoteCapabilities.audio));
                                ast_log(LOG_WARNING, "remote caps: %s\n", cap_buf);
                                pbx_channel_unlock(remotePeer);
                        }else{
                                ast_log(LOG_NOTICE, "SCCP: No remote peer\n");
                        }
*/

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
			ast_rtp_new_source(c->rtp.audio.rtp);
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
		        ast_rtp_change_source(c->rtp.audio.rtp);
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
			ast_rtp_new_source(c->rtp.audio.rtp);
		res = 0;
		break;

//	case AST_CONTROL_CONNECTED_LINE:
//		sccp_wrapper_asterisk16_connectedline(c, data, datalen);
//		sccp_indicate_locked(sccp_channel_getDevice(c), c, c->state);
//		break;

	case AST_CONTROL_VIDUPDATE:						/* Request a video frame update */
		if (c->rtp.video.rtp) {
			res = 0;
		} else
			res = -1;
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
static int sccp_wrapper_asterisk16_rtp_write(PBX_CHANNEL_TYPE * ast, PBX_FRAME_TYPE *frame)
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
//                                      ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", DEV_ID_LOG(sccp_channel_getDevice(c)), frame->subclass);
					ast_log(LOG_ERROR, "%s: Asked to transmit frame type %d with no samples.\n", DEV_ID_LOG(sccp_channel_getDevice(c)), (int)frame->frametype);
				} else {
					// frame->samples == 0  when frame_src is ast_prod
					sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Asterisk prodded channel %s.\n", DEV_ID_LOG(sccp_channel_getDevice(c)), ast->name);
				}
			} else if (!(frame->subclass & ast->nativeformats)) {
//                              char s1[512], s2[512], s3[512];
//
//                              ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), frame->subclass, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (ULONG)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (ULONG)ast->writeformat), (ULONG)ast->writeformat);
				//! \todo correct debugging
//                              ast_log(LOG_WARNING, "%s: Asked to transmit frame type %d, while native formats are %s(%lu) read/write = %s(%lu)/%s(%lu)\n", DEV_ID_LOG(sccp_channel_getDevice(c)), (int)frame->frametype, pbx_getformatname_multiple(s1, sizeof(s1) - 1, ast->nativeformats), ast->nativeformats, pbx_getformatname_multiple(s2, sizeof(s2) - 1, ast->readformat), (ULONG)ast->readformat, pbx_getformatname_multiple(s3, sizeof(s3) - 1, (ULONG)ast->writeformat), (ULONG)ast->writeformat);
				//return -1;
			}
			if (c->rtp.audio.rtp) {
				res = ast_rtp_write(c->rtp.audio.rtp, frame);
				
			}
			break;
		case AST_FRAME_IMAGE:
		case AST_FRAME_VIDEO:
#ifdef CS_SCCP_VIDEO
			if ((c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) == 0 && c->rtp.video.rtp && sccp_channel_getDevice(c)
			    //      && (sccp_channel_getDevice(c)->capability & frame->subclass)
			    ) {
				int codec = pbx_codec2skinny_codec( (frame->subclass & AST_FORMAT_VIDEO_MASK));
				ast_log(LOG_NOTICE, "%s: got video frame %d\n", DEV_ID_LOG(sccp_channel_getDevice(c)), codec);
				if(0 != codec){
					c->rtp.video.writeFormat = codec;
					sccp_channel_openMultiMediaChannel(c);
				}
			}

			if (c->rtp.video.rtp && (c->rtp.video.status & SCCP_RTP_STATUS_RECEIVE) != 0) {
				res = ast_rtp_write(c->rtp.video.rtp, frame);
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

static int sccp_wrapper_asterisk16_sendDigits(const sccp_channel_t * channel, const char *digits)
{
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int i;
	PBX_FRAME_TYPE f;

	f = ast_null_frame;
	
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digits '%s'\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
									// CS_AST_NEW_FRAME_STRUCT
	for (i = 0; digits[i] != '\0'; i++) {
		f.subclass = digits[i];
		sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), digits[i]);
		
		f.frametype = AST_FRAME_DTMF_BEGIN;
		ast_queue_frame(pbx_channel, &f);
		
		f.frametype = AST_FRAME_DTMF_END;
		ast_queue_frame(pbx_channel, &f);
	}
	return 1;
}

static int sccp_wrapper_asterisk16_sendDigit(const sccp_channel_t * channel, const char digit)
{
	char digits[2] = "\0\0";
	digits[0] = digit;
	
	sccp_log((DEBUGCAT_PBX | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: got a single digit '%c' -> '%s'\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), digit, digits);
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

	/* while setting nativeformats to multiple value, asterisk will not transcode it */
#ifdef CS_EXPERIMENTAL
	int i;

	for (i = 0; i < length; i++) {
		channel->owner->nativeformats |= skinny_codec2pbx_codec(codec[i]);
	}
#else
	channel->owner->nativeformats = skinny_codec2pbx_codec(codec[0]);
#endif

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: set native Formats to %d, skinny: %d\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), (int)channel->owner->nativeformats, codec[0]);
	return 1;
}

static int sccp_wrapper_asterisk16_setNativeVideoFormats(const sccp_channel_t * channel, uint32_t formats)
{

	return 1;
}

boolean_t sccp_wrapper_asterisk16_allocPBXChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel)
{
	*pbx_channel = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);
	
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
	
	char linkId[25];
	
	sprintf(linkId, "SCCP::%d", channel->callid);
	pbx_builtin_setvar_helper(*pbx_channel, SCCP_AST_LINKID_HELPER, linkId);

	return TRUE;
}

int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel)
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
static void *sccp_wrapper_asterisk16_park_thread(void *data)
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
		sccp_log(1) (VERBOSE_PREFIX_3 "SCCP: No PBX channel for parking");
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

	if (!ast_pthread_create_detached_background(&th, NULL, sccp_wrapper_asterisk16_park_thread, arg)) {
		return PARK_RESULT_SUCCESS;
	}
	return PARK_RESULT_FAIL;

}


/*!
 * \brief Pickup asterisk channel target using chan
 * 
 * \param hostChannel initial channel that request the parking
 * 
 */
static boolean_t sccp_wrapper_asterisk16_pickupChannel(const sccp_channel_t *chan, struct ast_channel *target){
	int res = -1;
        struct ast_channel *pbx_channel=chan->owner;
	if (target) {
		ast_debug(1, "Call pickup on chan '%s' by '%s'\n",target->name, pbx_channel->name);
		res = ast_answer(pbx_channel);
		if (res)
			ast_log(LOG_WARNING, "Unable to answer '%s'\n", pbx_channel->name);
		res = ast_queue_control(pbx_channel, AST_CONTROL_ANSWER);
		if (res)
			ast_log(LOG_WARNING, "Unable to queue answer on '%s'\n", pbx_channel->name);
		res = ast_channel_masquerade(target, pbx_channel);
		if (res)
			ast_log(LOG_WARNING, "Unable to masquerade '%s' into '%s'\n", pbx_channel->name, target->name);		/* Done */
		ast_channel_unlock(target);
	} else	{
		ast_debug(1, "No call pickup possible...\n");
	}
	return res==-1 ? FALSE : TRUE;
}

static uint8_t sccp_wrapper_asterisk16_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec)
{
	uint32_t astCodec;
	int payload;

	astCodec = skinny_codec2pbx_codec(codec);
        payload = ast_rtp_lookup_code(rtp->rtp, 1, astCodec);
        	
	return payload;
}

static uint8_t sccp_wrapper_asterisk16_get_sampleRate(skinny_codec_t codec)
{
	uint32_t astCodec;

	astCodec = skinny_codec2pbx_codec(codec);
	return ast_rtp_lookup_sample_rate(1, astCodec);
}

static sccp_extension_status_t sccp_wrapper_asterisk16_extensionStatus(const sccp_channel_t * channel)
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

static PBX_CHANNEL_TYPE *sccp_wrapper_asterisk16_request(const char *type, int format, void *data, int *cause)
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
	const struct ast_channel_tech *tech_pvt=NULL;
	skinny_codec_t codec = SKINNY_CODEC_G711_ULAW_64K;
	sccp_autoanswer_type_t autoanswer_type = SCCP_AUTOANSWER_NONE;
	uint8_t autoanswer_cause = AST_CAUSE_NOTDEFINED;
	int ringermode = 0;

	*cause = AST_CAUSE_NOTDEFINED;
	if (!type) {
		ast_log(LOG_NOTICE, "Attempt to call with unspecified type of channel\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
		return NULL;
	} else {
		tech_pvt=ast_get_channel_tech(type);
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

	sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked us to create a channel with type=%s, format=%lu, lineName=%s, options=%s\n", type, (uint64_t)format, lineName, (options) ? options : "");

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

	/* check format */
	if (format) {
		codec = pbx_codec2skinny_codec(format);
	}
	
	/** getting remote capabilities */
	char cap_buf[512];
	
	/* audio capabilities */
//	get_skinnyFormats(format, audioCapabilities & AST_FORMAT_AUDIO_MASK, ARRAY_LEN(audioCapabilities));
	get_skinnyFormats(tech_pvt->capabilities & AST_FORMAT_AUDIO_MASK, audioCapabilities, ARRAY_LEN(audioCapabilities));
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, audioCapabilities, ARRAY_LEN(audioCapabilities));
	ast_log(LOG_WARNING, "remote audio caps: %s\n", cap_buf);

#if 0
	/* video capabilities */
//	get_skinnyFormats(format, videoCapabilties & AST_FORMAT_VIDEO_MASK, ARRAY_LEN(videoCapabilities));
	get_skinnyFormats(tech_pvt->capabilities & AST_FORMAT_VIDEO_MASK, videoCapabilities, ARRAY_LEN(videoCapabilities));
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, videoCapabilities, ARRAY_LEN(videoCapabilities));
	ast_log(LOG_WARNING, "remote video caps: %s\n", cap_buf);
#endif

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
		sccp_channel_set_callingparty(channel, pbx_channel->cid.cid_name, pbx_channel->cid.cid_num);
		sccp_channel_set_originalCalledparty(channel, NULL, pbx_channel->cid.cid_dnid);
	}
	
	sccp_channel_unlock(channel);

EXITFUNC:
	if (lineName)
		sccp_free(lineName);
	sccp_restart_monitor();
	return (pbx_channel && channel && channel->owner && channel->owner==pbx_channel) ? pbx_channel : NULL;
}

static int sccp_wrapper_asterisk16_call(PBX_CHANNEL_TYPE * chan, char *addr, int timeout)
{
	//! \todo change this handling and split pbx and sccp handling -MC
	return sccp_pbx_call(chan, addr, timeout);
}

static int sccp_wrapper_asterisk16_answer(PBX_CHANNEL_TYPE * chan)
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
static int sccp_wrapper_asterisk16_fixup(PBX_CHANNEL_TYPE * oldchan, PBX_CHANNEL_TYPE * newchan)
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

static enum ast_bridge_result sccp_wrapper_asterisk16_rtpBridge(PBX_CHANNEL_TYPE *c0, PBX_CHANNEL_TYPE *c1, int flags, PBX_FRAME_TYPE **fo, PBX_CHANNEL_TYPE **rc, int timeoutms)
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

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: ( __PRETTY_FUNCTION__ ) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	return res;
}

static int sccp_wrapper_asterisk16_set_rtp_peer(PBX_CHANNEL_TYPE *ast, PBX_RTP_TYPE *rtp, PBX_RTP_TYPE *vrtp, PBX_RTP_TYPE *trtp, int codecs, int nat_active)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	struct sockaddr_in them;
	

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: __FILE__\n");
	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return -1;
	}
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
//		ast_rtp_instance_get_remote_address(rtp, &them);
		ast_rtp_get_peer(rtp, &them);
//		sccp_rtp_set_peer(c, &them); 
		return 0;
	} else {
		if (ast->_state != AST_STATE_UP) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%lu, nat=%d\n", (ULONG)codecs, d->nat);
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%lu, nat=%d\n", (ULONG)codecs, d->nat);
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
 *
 * \todo user CallerInfo instead ?
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

static int sccp_wrapper_asterisk16_getCodec(PBX_CHANNEL_TYPE * chan)
{
	format_t format = 0;

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "asterisk requests format for channel\n");
	//! \todo implement sccp_wrapper_asterisk16_getCodec
	return format;
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

static boolean_t sccp_wrapper_asterisk16_create_audio_rtp(const sccp_channel_t * c, void **new_rtp)
{
	sccp_session_t 	*s;
	sccp_device_t 	*d = NULL;

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

	*new_rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);
	if (!*new_rtp) {
		return FALSE;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	if (*new_rtp && c->owner)
		c->owner->fds[0] = ast_rtp_fd(*new_rtp);
#endif

#if ASTERISK_VERSION_NUMBER >= 10400
#    if ASTERISK_VERSION_NUMBER < 10600
	if (c->owner) {
		c->owner->fds[0] = ast_rtp_fd(*new_rtp);
		c->owner->fds[1] = ast_rtcp_fd(*new_rtp);
	}
#    else
	if (c->owner) {
		ast_channel_set_fd(c->owner, 0, ast_rtp_fd(*new_rtp));
		ast_channel_set_fd(c->owner, 1, ast_rtcp_fd(*new_rtp));
	}
#    endif
	/* tell changes to asterisk */
	if (c->owner) {
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	if (c->rtp.audio.rtp) {
// 		ast_rtp_codec_setpref(*new_rtp, (struct ast_codec_pref *)&c->codecs);
// 		ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
// 		sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	}
#endif
	if (*new_rtp) {
#if ASTERISK_VERSION_NUMBER >= 10600
		ast_rtp_setqos(*new_rtp, sccp_channel_getDevice(c)->audio_tos, sccp_channel_getDevice(c)->audio_cos, "SCCP RTP");
#else
		ast_rtp_settos(*new_rtp, sccp_channel_getDevice(c)->audio_tos);
#endif
		ast_rtp_setnat(*new_rtp, d->nat);
	}

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_create_video_rtp(const sccp_channel_t * c, void **new_rtp)
{
	sccp_session_t *s;

	sccp_device_t *d = NULL;

	if (!c)
		return FALSE;

	d = sccp_channel_getDevice(c);
	if (d)
		s = d->session;
	else
		return FALSE;

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", d->id, pbx_inet_ntoa(s->ourip));

	if (c->rtp.video.rtp) {
		ast_log(LOG_ERROR, "we already have a rtp server, why dont we use this?\n");
		return TRUE;
	}

	*new_rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);
	if (!*new_rtp) {
		return FALSE;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	if (*new_rtp && c->owner)
		c->owner->fds[0] = ast_rtp_fd(*new_rtp);
#endif

#if ASTERISK_VERSION_NUMBER >= 10400
#    if ASTERISK_VERSION_NUMBER < 10600
	if (c->rtp.audio.rtp && c->owner) {
		c->owner->fds[2] = ast_rtp_fd(*new_rtp);
		c->owner->fds[3] = ast_rtcp_fd(*new_rtp);
	}
#    else
	if (c->rtp.audio.rtp && c->owner) {
		ast_channel_set_fd(c->owner, 2, ast_rtp_fd(*new_rtp));
		ast_channel_set_fd(c->owner, 3, ast_rtcp_fd(*new_rtp));
	}
#    endif
	/* tell changes to asterisk */
	if ((*new_rtp) && c->owner) {
		ast_queue_frame(c->owner, &ast_null_frame);
	}

	if (*new_rtp) {
// 		ast_rtp_codec_setpref(*new_rtp, (struct ast_codec_pref *)&c->codecs);
// 		ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
// 		sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	}
#endif
	if (*new_rtp) {
#if ASTERISK_VERSION_NUMBER >= 10600
		ast_rtp_setqos(*new_rtp, sccp_channel_getDevice(c)->video_tos, sccp_channel_getDevice(c)->video_cos, "SCCP VRTP");
#else
		ast_rtp_settos(*new_rtp, sccp_channel_getDevice(c)->video_tos);
#endif
		ast_rtp_setnat(*new_rtp, d->nat);
#if ASTERISK_VERSION_NUMBER >= 10600
// 		ast_rtp_codec_setpref(c->rtp.video.rtp, (struct ast_codec_pref *)&c->codecs);
#endif

	}

	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_destroyRTP(PBX_RTP_TYPE * rtp)
{
	ast_rtp_destroy(rtp);
	return (!rtp) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_addToDatabase(const char *family, const char *key, const char *value)
{
	int res;
	if (sccp_strlen_zero(family) || sccp_strlen_zero(key) || sccp_strlen_zero(value)) 
		return FALSE;
	res = ast_db_put(family, key, value);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_getFromDatabase(const char *family, const char *key, char *out, int outlen)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) 
		return FALSE;
	res = ast_db_get(family, key, out, outlen);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_removeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) 
		return FALSE;
	res = ast_db_del(family, key);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk18_removeTreeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) 
		return FALSE;
	res = ast_db_deltree(family, key);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk16checkhangup(const sccp_channel_t * channel)
{
	int res;

	res = ast_check_hangup(channel->owner);
	return (!res) ? TRUE : FALSE;
}

static boolean_t sccp_wrapper_asterisk16_rtpGetUs(PBX_RTP_TYPE * rtp, struct sockaddr_in *address)
{
	ast_rtp_get_us(rtp, address);
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_getChannelByName(const char *name, PBX_CHANNEL_TYPE * pbx_channel)
{

        pbx_channel = ast_get_channel_by_name_locked(name);
	if (!pbx_channel)
		return FALSE;
        pbx_channel_unlock(pbx_channel);
	return TRUE;
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
	channel->owner->writeformat = skinny_codec2pbx_codec(codec);

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write native: %d\n", (int)channel->owner->rawwriteformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "write: %d\n", (int)channel->owner->writeformat);

	ast_set_write_format(channel->owner, skinny_codec2pbx_codec(codec));
	return TRUE;
}

static boolean_t sccp_wrapper_asterisk16_setReadFormat(const sccp_channel_t * channel, skinny_codec_t codec)
{
	channel->owner->rawreadformat = skinny_codec2pbx_codec(codec);
	channel->owner->readformat = skinny_codec2pbx_codec(codec);

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read native: %d\n", (int)channel->owner->rawreadformat);
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "read: %d\n", (int)channel->owner->readformat);

	ast_set_read_format(channel->owner, skinny_codec2pbx_codec(codec));
	return TRUE;
}

static void sccp_wrapper_asterisk16_setCalleridName(const sccp_channel_t * channel, const char *name)
{
	if (!strcmp(channel->owner->cid.cid_name, name)) {
		sccp_copy_string(channel->owner->cid.cid_name, name, sizeof(channel->owner->cid.cid_name) - 1);
	}
}

static void sccp_wrapper_asterisk16_setCalleridNumber(const sccp_channel_t * channel, const char *number)
{
	if (!strcmp(channel->owner->cid.cid_num, number)) {
		sccp_copy_string(channel->owner->cid.cid_num, number, sizeof(channel->owner->cid.cid_num) - 1);
	}
}

static void sccp_wrapper_asterisk16_setRedirectingParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	/*!< \todo set RedirectedParty using ast_callerid */
}

static void sccp_wrapper_asterisk16_setRedirectedParty(const sccp_channel_t * channel, const char *number, const char *name)
{
	/*!< \todo set RedirectedParty using ast_callerid */
}

static void sccp_wrapper_asterisk16_updateConnectedLine(const sccp_channel_t * channel, const char *number, const char *name, uint8_t reason)
{
	
}

static int sccp_wrapper_asterisk16_sched_add(int when, sccp_sched_cb callback, const void *data)
{
	return ast_sched_add(sched, when, callback, data);
}

static long sccp_wrapper_asterisk16_sched_when(int id)
{
	return ast_sched_when(sched, id);
}

static unsigned int sccp_wrapper_asterisk16_sched_wait(int id)
{
	return ast_sched_wait(sched);
}

static int sccp_wrapper_asterisk16_sched_del(int id)
{
	return ast_sched_del(sched, id);
}

static int sccp_wrapper_asterisk16_setCallState(const sccp_channel_t * channel, int state)
{
	sccp_pbx_setcallstate((sccp_channel_t *) channel, state);
	//! \todo implement correct return value (take into account negative deadlock prevention)
	return 0;
}

int sccp_wrapper_asterisk16_requestHangup(PBX_CHANNEL_TYPE * channel)
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
 * \brief Search for channel with found_cb callback
 * \param found_cb callback function
 * \param data data
 * \param lock lock container
 * \return channel result
 */
static PBX_CHANNEL_TYPE * sccp_wrapper_asterisk16_findChannelWithCallback( int(*const found_cb)(PBX_CHANNEL_TYPE *c, void *data), void *data, boolean_t lock) { 
        PBX_CHANNEL_TYPE *remotePeer;
	
	if(!lock){
		sccp_log(1) (VERBOSE_PREFIX_3 "requesting channel search without lock, but no ipln for this\n");
	}
	remotePeer = ast_channel_search_locked(found_cb, data);
	return remotePeer;
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

	sccp_dev_keypadbutton(sccp_channel_getDevice(c), digit, sccp_device_find_index_for_line(sccp_channel_getDevice(c), c->line->name), c->callid);

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
static int sccp_wrapper_asterisk16_channel_read(struct ast_channel *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen)
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

static int sccp_wrapper_asterisk16_channel_write(struct ast_channel *ast, const char *funcname, char *args, const char *value){
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


static const char *sccp_wrapper_asterisk16_getChannelLinkId(const sccp_channel_t *channel){
	static const char *emptyLinkId = "--no-linkedid--";
	
	if(channel->owner){
		if(pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKID_HELPER)){
			return pbx_builtin_getvar_helper(channel->owner, SCCP_AST_LINKID_HELPER);
		}
	}
	return emptyLinkId;
}

/*!
 * \brief using RTP Glue Engine
 */
struct ast_rtp_protocol sccp_rtp = {
	.type = SCCP_TECHTYPE_STR,
	.get_rtp_info = sccp_wrapper_asterisk16_get_rtp_peer,
	.get_vrtp_info = sccp_wrapper_asterisk16_get_vrtp_peer,
	.set_rtp_peer = sccp_wrapper_asterisk16_set_rtp_peer,
	.get_codec = sccp_wrapper_asterisk16_getCodec,
};

/*!
 * \brief SCCP Tech Structure
 */
const struct ast_channel_tech sccp_tech = {
	/* *INDENT-OFF* */
	.type 			= SCCP_TECHTYPE_STR,
	.description 		= "Skinny Client Control Protocol (SCCP)",
	// we could use the skinny_codec = ast_codec mapping here to generate the list of capabilities
	.capabilities 		= AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_G723_1 | AST_FORMAT_G729A,
	.properties 		= AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER,
	.requester 		= sccp_wrapper_asterisk16_request,
	.devicestate 		= sccp_devicestate,
	.call 			= sccp_wrapper_asterisk16_call,
	.hangup 		= sccp_wrapper_asterisk16_hangup,
	.answer 		= sccp_wrapper_asterisk16_answer,
	.read 			= sccp_wrapper_asterisk16_rtp_read,
	.write 			= sccp_wrapper_asterisk16_rtp_write,
	.write_video 		= sccp_wrapper_asterisk16_rtp_write,
	.indicate 		= sccp_wrapper_asterisk16_indicate,
	.fixup 			= sccp_wrapper_asterisk16_fixup,
	.transfer		= sccp_pbx_transfer,

	//! \todo implement asterisk extension for commented functions
	.bridge 		= sccp_wrapper_asterisk16_rtpBridge,
	//.early_bridge         = ast_rtp_early_bridge,
	//.bridged_channel	=
	
	.send_text 		= sccp_pbx_sendtext,
	//.send_html		=
	//.send_image		=

	.func_channel_read 	= sccp_wrapper_asterisk16_channel_read,
	.func_channel_write 	= sccp_wrapper_asterisk16_channel_write,

	.send_digit_begin	= sccp_wrapper_recvdigit_begin,
	.send_digit_end 	= sccp_wrapper_recvdigit_end,
	
	//.write_text           = 
	//.write_video		=
	//.cc_callback          =                                              // ccss, new >1.6.0
	//.exception            =                                              // new >1.6.0
	//.setoption 		= sccp_wrapper_asterisk16_setOption,
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
	.alloc_pbxChannel 		= sccp_wrapper_asterisk16_allocPBXChannel,
	.requestHangup 			= sccp_wrapper_asterisk16_requestHangup,
	.extension_status 		= sccp_wrapper_asterisk16_extensionStatus,
	.getChannelByName 		= sccp_wrapper_asterisk16_getChannelByName,
	.getChannelLinkId		= sccp_wrapper_asterisk16_getChannelLinkId,
	.checkhangup			= sccp_wrapper_asterisk16checkhangup,
	
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
	.get_callerid_dnid 		= NULL,						//! \todo implement callback
	.get_callerid_rdnis 		= NULL,						//! \todo implement callback
	.get_callerid_presence 		= NULL,						//! \todo implement callback

	.set_callerid_name 		= sccp_wrapper_asterisk16_setCalleridName,	//! \todo implement callback
	.set_callerid_number 		= sccp_wrapper_asterisk16_setCalleridNumber,	//! \todo implement callback
	.set_callerid_dnid 		= NULL,						//! \todo implement callback
	.set_callerid_redirectingParty 	= sccp_wrapper_asterisk16_setRedirectingParty,
	.set_callerid_redirectedParty 	= sccp_wrapper_asterisk16_setRedirectedParty,
	.set_callerid_presence 		= sccp_wrapper_asterisk16_setCalleridPresence,
	.set_connected_line		= sccp_wrapper_asterisk16_updateConnectedLine,
	
	
	/* database */
	.feature_addToDatabase 		= sccp_wrapper_asterisk16_addToDatabase,
	.feature_getFromDatabase 	= sccp_wrapper_asterisk16_getFromDatabase,	//! \todo implement callback
	.feature_removeFromDatabase     = sccp_wrapper_asterisk16_removeFromDatabase,	
	.feature_removeTreeFromDatabase = sccp_wrapper_asterisk16_removeTreeFromDatabase,
	.feature_park			= sccp_wrapper_asterisk16_park,
	.feature_pickup			= sccp_wrapper_asterisk16_pickupChannel,
	
	.findChannelByCallback		= sccp_wrapper_asterisk16_findChannelWithCallback,
	
	/* *INDENT-ON* */
};

static int load_module(void)
{

	boolean_t res;

	res = sccp_prePBXLoad();
	/* make globals */
	if (!res) {
		return AST_MODULE_LOAD_FAILURE;
	}

	sched = sched_context_create();
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

	ast_rtp_proto_register(&sccp_rtp);
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
	ast_rtp_proto_unregister(&sccp_rtp);
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
		sched_context_destroy(sched);
		sched = NULL;
	}
	sccp_free(sccp_globals);
	ast_log(LOG_NOTICE, "Running Cleanup\n");
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
//	.load_pri = AST_MODPRI_CHANNEL_DRIVER,
//	.nonoptreq = "res_rtp_asterisk,chan_local",
    );

PBX_CHANNEL_TYPE * sccp_search_remotepeer_locked( int(*const found_cb)(PBX_CHANNEL_TYPE *c, void *data), void *data) { 
        PBX_CHANNEL_TYPE *remotePeer;
	remotePeer = ast_channel_search_locked(found_cb, data);
	return remotePeer;
}
