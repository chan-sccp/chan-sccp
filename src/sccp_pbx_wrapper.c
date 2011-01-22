
/*!
 * \file 	sccp_pbx_wrapper.c
 * \brief 	SCCP PBX Wrapper Class
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#include "config.h"
#include "common.h"
#include <asterisk/frame.h>

int sccp_wrapper_asterisk_rtp_stop(sccp_channel_t *channel);

struct sccp_pbx_cb sccp_pbx = {
	.rtp_stop 		= sccp_wrapper_asterisk_rtp_stop,
	.rtp_audio_create 	= sccp_wrapper_asterisk_create_audio_rtp,
};

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/************************************************************************************************************ CALLERID **/

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char *get_pbx_callerid_name(struct ast_channel *ast_chan)
{
	static char result[StationMaxNameSize];

#ifndef CS_AST_CHANNEL_HAS_CID
	char *name, *number, *cidtmp;

	ast_callerid_parse(cidtmp, &name, &number);
	sccp_copy_string(result, name, sizeof(result) - 1);
	if (cidtmp)
		ast_free(cidtmp);
	cidtmp = NULL;

	if (name)
		ast_free(name);
	name = NULL;

	if (number)
		ast_free(number);
	number = NULL;
#else
	sccp_copy_string(result, ast_chan->cid.cid_name, sizeof(result) - 1);
#endif
	return result;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char *get_pbx_callerid_number(struct ast_channel *ast_chan)
{
	static char result[StationMaxDirnumSize];

#ifndef CS_AST_CHANNEL_HAS_CID
	char *name, *number, *cidtmp;

	ast_callerid_parse(cidtmp, &name, &number);
	sccp_copy_string(result, number, sizeof(result) - 1);
	if (cidtmp)
		ast_free(cidtmp);
	cidtmp = NULL;

	if (name)
		ast_free(name);
	name = NULL;

	if (number)
		ast_free(number);
	number = NULL;
#else
	sccp_copy_string(result, ast_chan->cid.cid_num, sizeof(result) - 1);
#endif
	return result;
}


/*!
 * \brief Create a new RTP Source.
 * \param c SCCP Channel
 */
boolean_t sccp_wrapper_asterisk_create_audio_rtp(const sccp_channel_t * c)
{
	sccp_session_t *s;
	sccp_device_t *d = NULL;
	char pref_buf[128];

	if (!c)
		return FALSE;
	
	d = c->device;
	if (d)
		s = d->session;
	else
		return FALSE;

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", d->id, pbx_inet_ntoa(s->ourip));
	
	if(c->rtp.audio.rtp){
		ast_log(LOG_ERROR, "we already have a rtp server, why dont we use this?\n");
		return TRUE;
	}

	((sccp_channel_t *)c)->rtp.audio.rtp = ast_rtp_new_with_bindaddr(sched, io, 1, 0, s->ourip);
	if (!c->rtp.audio.rtp) {
		return FALSE;
	}
#if ASTERISK_VERSION_NUM < 10400
	if (c->rtp.audio.rtp && c->owner)
		c->owner->fds[0] = ast_rtp_fd(c->rtp.audio.rtp);
#endif

#if ASTERISK_VERSION_NUM >= 10400
#    if ASTERISK_VERSION_NUM < 10600
	if (c->rtp.audio.rtp && c->owner) {
		c->owner->fds[0] = ast_rtp_fd(c->rtp.audio.rtp);
		c->owner->fds[1] = ast_rtcp_fd(c->rtp.audio.rtp);
	}
#    else
	if (c->rtp.audio.rtp && c->owner) {
		ast_channel_set_fd(c->owner, 0, ast_rtp_fd(c->rtp.audio.rtp));
		ast_channel_set_fd(c->owner, 1, ast_rtcp_fd(c->rtp.audio.rtp));
	}
#    endif
	/* tell changes to asterisk */
	if ((c->rtp.audio.rtp) && c->owner) {
		ast_queue_frame(c->owner, &sccp_null_frame);
	}

	if (c->rtp.audio.rtp) {
		ast_rtp_codec_setpref((struct ast_rtp *)c->rtp.audio.rtp, &c->codecs);
		ast_codec_pref_string(&c->codecs, pref_buf, sizeof(pref_buf) - 1);
		sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	}
#endif

//        ast_set_flag(c->rtp.audio.rtp, FLAG_HAS_DTMF);
//        ast_set_flag(c->rtp.audio.rtp, FLAG_P2P_NEED_DTMF);

	if (c->rtp.audio.rtp) {
#if ASTERISK_VERSION_NUM >= 10600
		ast_rtp_setqos(c->rtp.audio.rtp, c->line->audio_tos, c->line->audio_cos, "SCCP RTP");
#else
		ast_rtp_settos(c->rtp.audio.rtp, c->line->audio_tos);
#endif
		ast_rtp_setnat(c->rtp.audio.rtp, d->nat);
#if ASTERISK_VERSION_NUM >= 10600
		ast_rtp_codec_setpref(c->rtp.audio.rtp, &c->codecs);
#endif
	}

	return TRUE;
}


#if ASTERISK_VERSION_NUM >= 10400
/*!
 * \brief Get RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \return ENUM of RTP Result
 * 
 * \called_from_asterisk
 */
enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(struct ast_channel *ast, struct ast_rtp **rtp)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Asterisk requested RTP peer for channel %s\n", ast->name);

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO PVT\n");
		return AST_RTP_GET_FAILED;
	}

	if (!c->rtp.audio.rtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO RTP\n");
		return AST_RTP_GET_FAILED;
	}

	*rtp = c->rtp.audio.rtp;
	
	if (!(d = c->device)) {
		sccp_log((DEBUGCAT_RTP | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) NO DEVICE\n");
		return AST_RTP_GET_FAILED;
	}

	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GET_FAILED;
	}

	if (!d->directrtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	if (d->nat) {
		res = AST_RTP_TRY_PARTIAL;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
	} else {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Using AST_RTP_TRY_NATIVE for channel %s\n", ast->name);
	}

	return res;
}
#endif

/*!
 * \brief Get VRTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \return ENUM of RTP Result
 * 
 * \called_from_asterisk
 */
#ifndef CS_AST_HAS_RTP_ENGINE
#define _RTP_GET_FAILED AST_RTP_GET_FAILED
enum ast_rtp_get_result sccp_wrapper_asterisk_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp **rtp)
#else
#define _RTP_GET_FAILED AST_RTP_GLUE_RESULT_FORBID
enum ast_rtp_glue_result sccp_wrapper_asterisk_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp_instance **rtp)
#endif
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	if (!(c = CS_AST_CHANNEL_PVT(ast)) || !(c->rtp.video.rtp)) {
	        return _RTP_GET_FAILED;
	}
#ifdef CS_AST_HAS_RTP_ENGINE
	ao2_ref(c->rtp.video, +1);
#endif
	*rtp = c->rtp.video.rtp;

	struct sockaddr_in them;
	ast_rtp_get_peer(*rtp, &them);

	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_vrtp_peer) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
	        return _RTP_GET_FAILED;
	}

	if (!d->directrtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
	}

	if (d->nat) {
		res = AST_RTP_TRY_PARTIAL;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_vrtp_peer) Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
	} else {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_vrtp_peer) Using AST_RTP_TRY_NATIVE for channel %s\n", ast->name);
	}
	
	return res;
}

#if ASTERISK_VERSION_NUM >= 10400 && ASTERISK_VERSION_NUM < 10600
/*!
 * \brief Set RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \param vrtp Asterisk RTP
 * \param codecs Codecs as int
 * \param nat_active Is NAT Active as int
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_wrapper_asterisk_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, int codecs, int nat_active)
#else
/*!
 * \brief Set RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \param vrtp Asterisk RTP
 * \param trtp Asterisk RTP
 * \param codecs Codecs as int
 * \param nat_active Is NAT Active as int
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_wrapper_asterisk_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp, int codecs, int nat_active)
#endif
{
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;
//	sccp_moo_t *r;

	struct ast_format_list fmt;
	struct sockaddr_in them;
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: __FILE__\n");

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO PVT\n");
		return -1;
	}
	if (!(l = c->line)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO LINE\n");
		return -1;
	}
	if (!(d = c->device)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) NO DEVICE\n");
		return -1;
	}

	if (!d->directrtp) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_set_rtp_peer) Direct RTP disabled\n");
		return -1;
	}

	if (rtp) {
        	ast_rtp_get_peer(rtp, &them);
            	
// 		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_set_rtp_peer) asterisk told us to set peer to '%s:%d'\n", DEV_ID_LOG(d), pbx_inet_ntoa(them.sin_addr), ntohs(them.sin_port));
//             	
// 		if(them.sin_port == 0){
// 			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_set_rtp_peer) remote information are invalid, us our own rtp\n", DEV_ID_LOG(d));
// 			ast_rtp_get_us(rtp, &c->rtp.audio.phone_remote);
// 		}else{
// 			c->rtp.audio.phone_remote=them;   // should be called bridge_peer
// 		}
                
                
                sccp_rtp_set_peer(c, &them);
// 		if (c->rtp.audio.status & SCCP_RTP_STATUS_TRANSMIT) {
//                         /* Shutdown any early-media or previous media on re-invite */
// 			/* \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
// 	                sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_set_rtp_peer) Stop media transmission on channel %d\n", DEV_ID_LOG(d), c->callid);
// 			sccp_channel_stopmediatransmission_locked(c);
// 		}
// 
//                 //fmt = pbx_codec_pref_getsize(&d->codecs, ast_best_codec(d->capability));
//                 int codec = ast_codec_choose(&c->codecs, codecs, 1);
//                 fmt = pbx_codec_pref_getsize(&d->codecs, codec);
//                 c->format = fmt.bits;						/* updating channel format */
//                 // replace by new function (something like: sccp_channel_setcodec / sccp_channel_choosecodec)
// 
//                 sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_channel_set_rtp_peer) Start media transmission on channel %d, codec='%d', payloadType='%d' (%d ms), peer_ip='%s:%d'\n", DEV_ID_LOG(d), c->callid, codec, fmt.bits, fmt.cur_ms, pbx_inet_ntoa(them.sin_addr), ntohs(them.sin_port));
//                 sccp_channel_startmediatransmission(c);

                c->mediaStatus.transmit = TRUE;
                return 0;
	} else {
		if (ast->_state != AST_STATE_UP) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Early RTP stage, codecs=%d, nat=%d\n", codecs, d->nat);
		} else {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "SCCP: (sccp_channel_set_rtp_peer) Native Bridge Break, codecs=%d, nat=%d\n", codecs, d->nat);
		}
		return 0;
	}

	/* Need a return here to break the bridge */
	return 0;
}

/*
 * \brief get callerid from pbx
 * \param ast_chan Asterisk Channel
 * \return SCCP CallInfo Structure 
 *
 * \todo need to be inspected and tested
 */
sccp_callinfo_t *get_pbx_callerid(struct ast_channel * ast_chan)
{
	sccp_callinfo_t *callInfo = NULL;

	if (!ast_chan) {
		return callInfo;
	}
#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else

#    define strcp_callerid(_sccp,_ast)                                     \
        sccp_copy_string(_sccp,_ast,sizeof(_sccp)-1);       	\

	if (callInfo->callingParty_valid) {
		strcp_callerid(callInfo->callingPartyName, ast_chan->cid.cid_num);
		strcp_callerid(callInfo->callingPartyNumber, ast_chan->cid.cid_name);
	}

	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCallingPartyNumber, ast_chan->cid.cid_rdnis);	//RDNIS ? ANI
	}

	if (callInfo->lastRedirecting_valid) {
		strcp_callerid(callInfo->lastRedirectingPartyNumber, ast_chan->cid.cid_ani);	//RDNIS ? ANI
	}

	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCalledPartyNumber, ast_chan->cid.cid_dnid);	//DNID
	}

	callInfo->presentation = (int)ast_chan->cid.cid_pres;
#endif

#undef strcp_callerid
	return callInfo;
}

/*
 * \brief set callerid on pbx channel
 * \param ast_chan Asterisk Channel
 * \param callInfo SCCP CallInfo Structure
 * \return Fail/Success as int
 *
 * \todo need to be inspected and tested
 */
int set_pbx_callerid(struct ast_channel *ast_chan, sccp_callinfo_t * callInfo)
{
	int RESULT = 0;

	if (!ast_chan) {
		return -1;
	}

	if (!callInfo) {
		return -1;
	}
#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else
#    define strcp_callerid(_ast,_sccp)                              \
	 	sccp_copy_string(_ast,_sccp,sizeof(_ast)-1);    \

#    if 0
	if (!strcmp(ast_chan->cid.cid_num, callInfo->callingPartyNumber) && !strcmp(ast_chan->cid.cid_name, callInfo->callingPartyName)) {
		strcp_callerid(ast_chan->cid.cid_num, callInfo->callingPartyNumber);
		strcp_callerid(ast_chan->cid.cid_name, callInfo->callingPartyName);
		callInfo->callingParty_valid = 1;
	}

	if (!strcmp(ast_chan->cid.cid_ani, callInfo->calledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->calledPartyNumber);
		callInfo->calledParty_valid = 1;
	}

	if (!strcmp(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber);	//RDNIS
		callInfo->originalCallingParty_valid = 1;
	}

	if (!strcmp(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber);	//ANI
		callInfo->lastRedirectingParty_valid = 1;
	}

	if (!strcmp(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber);	//DNID
		callInfo->originalCalledParty_valid;
	}

	ast_chan->cid.cid_pres = (int)callInfo->presentation;
#    endif
#endif

#undef strcp_callerid
	return RESULT;
}

/************************************************************************************************************* CHANNEL **/

/*
 * \brief itterate through locked pbx channels
 * \note replacement for ast_channel_walk_locked
 * \param ast_chan Asterisk Channel
 * \return ast_chan Locked Asterisk Channel
 */
struct ast_channel *pbx_channel_walk_locked(struct ast_channel *target)
{
	return ast_channel_walk_locked(target);
}

/************************************************************************************************************** CONFIG **/

/*
 * \brief Add a new rule to a list of HAs
 * \note replacement for ast_append_ha
 * \param sense Sense / Key
 * \param stuff Value / Stuff to Add
 * \param path list of HAs
 * \param error Error as int
 * \return The head of the HA list
 */
struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error)
{
#if ASTERISK_VERSION_NUM < 10600
	return ast_append_ha(sense, stuff, path);
#else
	return ast_append_ha(sense, stuff, path, error);
#endif
}

/*
 * \brief Register a new context
 * \note replacement for ast_context_find_or_create
 *
 * This will first search for a context with your name.  If it exists already, it will not
 * create a new one.  If it does not exist, it will create a new one with the given name
 * and registrar.
 *
 * \param extcontexts pointer to the ast_context structure pointer
 * \param name name of the new context
 * \param registrar registrar of the context
 * \return NULL on failure, and an ast_context structure on success
 */
struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar)
{
#if ASTERISK_VERSION_NUM < 10600
	return ast_context_find_or_create(extcontexts, name, registrar);
#else
	return ast_context_find_or_create(extcontexts, exttable, name, registrar);
#endif
}

/*!
 * \brief Load a config file
 *
 * \param filename path of file to open.  If no preceding '/' character,
 * path is considered relative to AST_CONFIG_DIR
 * \param who_asked The module which is making this request.
 * \param flags Optional flags:
 * CONFIG_FLAG_WITHCOMMENTS - load the file with comments intact;
 * CONFIG_FLAG_FILEUNCHANGED - check the file mtime and return CONFIG_STATUS_FILEUNCHANGED if the mtime is the same; or
 * CONFIG_FLAG_NOCACHE - don't cache file mtime (main purpose of this option is to save memory on temporary files).
 *
 * \details
 * Create a config structure from a given configuration file.
 *
 * \return an ast_config data structure on success
 * \retval NULL on error
 */
struct ast_config *pbx_config_load(const char *filename, const char *who_asked, struct ast_flags flags)
{
#if ASTERISK_VERSION_NUM < 10600
	return ast_config_load(filename);
#else
	return ast_config_load2(filename, who_asked, flags);
#endif
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
#if ASTERISK_VERSION_NUM >= 10400
	return ast_getformatname_multiple(buf, size, format & AST_FORMAT_AUDIO_MASK);
#else
	return ast_getformatname_multiple(buf, size, format);
#endif
}

/*!
 * \brief Get/Create new config variable
 * \note replacement for ast_variable_new
 * \param name Variable Name as char
 * \param value Variable Value as char
 * \param filename Filename
 * \return The return value is struct ast_variable.
 */
struct ast_variable *pbx_variable_new(struct ast_variable *v)
{
#if ASTERISK_VERSION_NUM >= 10600
	return ast_variable_new(v->name, v->value, v->file);
#else
	return ast_variable_new(v->name, v->value);
#endif
}

/******************************************************************************************************** NET / SOCKET **/

/*
 * \brief thread-safe replacement for inet_ntoa()
 * \note replacement for ast_pbx_inet_ntoa
 * \param ia In Address / Source Address
 * \return Address as char
 */
const char *pbx_inet_ntoa(struct in_addr ia)
{
#if ASTERISK_VERSION_NUM < 10400
	char iabuf[INET_ADDRSTRLEN];

	return ast_inet_ntoa(iabuf, sizeof(iabuf), ia);
#else
	return ast_inet_ntoa(ia);
#endif
}

#if ASTERISK_VERSION_NUM < 10400

/* BackPort of ast_str2cos & ast_str2cos for asterisk 1.2*/
struct dscp_codepoint {
	char *name;
	unsigned int space;
};

static const struct dscp_codepoint dscp_pool1[] = {
	{"CS0", 0x00},
	{"CS1", 0x08},
	{"CS2", 0x10},
	{"CS3", 0x18},
	{"CS4", 0x20},
	{"CS5", 0x28},
	{"CS6", 0x30},
	{"CS7", 0x38},
	{"AF11", 0x0A},
	{"AF12", 0x0C},
	{"AF13", 0x0E},
	{"AF21", 0x12},
	{"AF22", 0x14},
	{"AF23", 0x16},
	{"AF31", 0x1A},
	{"AF32", 0x1C},
	{"AF33", 0x1E},
	{"AF41", 0x22},
	{"AF42", 0x24},
	{"AF43", 0x26},
	{"EF", 0x2E},
};

int pbx_str2tos(const char *value, unsigned int *tos)
{
	int fval;

	unsigned int x;

	if (sscanf(value, "%30i", &fval) == 1) {
		*tos = fval & 0xFF;
		return 0;
	}

	for (x = 0; x < ARRAY_LEN(dscp_pool1); x++) {
		if (!strcasecmp(value, dscp_pool1[x].name)) {
			*tos = dscp_pool1[x].space << 2;
			return 0;
		}
	}

	return -1;
}
#else
int pbx_str2tos(const char *value, unsigned int *tos)
{
	return ast_str2tos(value, tos);
}
#endif

#if ASTERISK_VERSION_NUM < 10600
int pbx_str2cos(const char *value, unsigned int *cos)
{
	int fval;

	if (sscanf(value, "%30d", &fval) == 1) {
		if (fval < 8) {
			*cos = fval;
			return 0;
		}
	}

	return -1;
}
#else
int pbx_str2cos(const char *value, unsigned int *cos)
{
	return ast_str2cos(value, cos);
}
#endif

/************************************************************************************************************* GENERAL **/

/*! 
 * \brief Simply remove extension from context
 * \note replacement for ast_context_remove_extension
 * 
 * \param context context to remove extension from
 * \param extension which extension to remove
 * \param priority priority of extension to remove (0 to remove all)
 * \param callerid NULL to remove all; non-NULL to match a single record per priority
 * \param matchcid non-zero to match callerid element (if non-NULL); 0 to match default case
 * \param registrar registrar of the extension
 *
 * This function removes an extension from a given context.
 *
 * \retval 0 on success 
 * \retval -1 on failure
 * 
 * @{
 */
int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar)
{
#if ASTERISK_VERSION_NUM >= 10600
	struct pbx_find_info q = {.stacklen = 0 };
	if (pbx_find_extension(NULL, NULL, &q, context, extension, 1, NULL, "", E_MATCH)) {
		return ast_context_remove_extension(context, extension, priority, registrar);
	} else {
		return -1;
	}
#else
	return ast_context_remove_extension(context, extension, priority, registrar);
#endif
}

/*!   
 * \brief Send ack in manager list transaction
 * \note replacement for astman_send_listack
 * \param context which context to add the ignorpattern to
 * \param ignorepat ignorepattern to set up for the extension
 * \param registrar registrar of the ignore pattern
 */
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag)
{
#if ASTERISK_VERSION_NUM < 10600
	astman_send_ack(s, m, msg);
#else
	astman_send_listack(s, m, msg, listflag);
#endif
}

/*!
 * \brief Turn on music on hold on a given channel 
 * \note replacement for ast_moh_start
 *
 * \param chan The channel structure that will get music on hold
 * \param mclass The class to use if the musicclass is not currently set on
 *               the channel structure.
 * \param interpclass The class to use if the musicclass is not currently set on
 *                    the channel structure or in the mclass argument.
 *
 * \retval Zero on success
 * \retval non-zero on failure
 */
int pbx_moh_start(struct ast_channel *chan, const char *mclass, const char *interpclass)
{
#if ASTERISK_VERSION_NUM < 10400
	return ast_moh_start(chan, interpclass);
#else
	return ast_moh_start(chan, mclass, interpclass);
#endif
}

int sccp_wrapper_asterisk_rtp_stop(sccp_channel_t *channel){
	if (channel->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping phone media transmission on channel %08X\n",channel->callid);
		ast_rtp_stop(channel->rtp.audio.rtp);
	}

	if (channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "Stopping video media transmission on channel %08X\n", channel->callid);
		ast_rtp_stop(channel->rtp.video.rtp);
	}
	return 0;
}

/***************************************************************************************************************** RTP **/

/*!
 * \brief pbx rtp get peer
 * Copies from rtp to them and returns 1 if there was a change or 0 if it was already the same
 * \note replacement for ast_rtp_get_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 * \return int
 */
int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *addr)
{
// #if ASTERISK_VERSION_NUM >= 10800
// 	struct ast_sockaddr addr_tmp;
// 	ast_rtp_instance_get_remote_address(rtp, &addr_tmp);
// 	ast_sockaddr_to_sin(&addr_tmp, &addr);
// 	return addr.sin_addr.s_addr;
// #else
	return ast_rtp_get_peer(rtp, addr);
// #endif
}

/*!
 * \brief pbx rtp get us
 * \note replacement for ast_rtp_get_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 * \return int
 */
void pbx_rtp_get_us(struct ast_rtp *rtp, struct sockaddr_in *addr)
{
// #if ASTERISK_VERSION_NUM >= 10800
// 	struct ast_sockaddr addr_tmp;
// 	ast_rtp_instance_get_local_address(rtp, &addr_tmp);
// 	ast_sockaddr_to_sin(&addr_tmp, &addr);
// #else
	ast_rtp_get_us(rtp, addr);
// #endif
}

/*!
 * \brief pbx rtp set peer
 * \note replacement for ast_rtp_set_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 */
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *addr)
{
// #if ASTERISK_VERSION_NUM >= 10800
// 	struct ast_sockaddr addr_tmp;
// 	ast_sockaddr_from_sin(&addr_tmp, &addr);
// 	ast_rtp_instance_set_remote_address(rtp, &addr_tmp);
// #else
	ast_rtp_set_peer(rtp, addr);
// #endif
}


