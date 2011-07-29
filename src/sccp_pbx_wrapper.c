
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

int sccp_wrapper_asterisk_rtp_stop(sccp_channel_t * channel);

struct sccp_pbx_cb sccp_pbx = {
	.alloc_pbxChannel = sccp_wrapper_asterisk_allocPBXChannel,
	.set_callstate = sccp_wrapper_asterisk_setCallState,

	.rtp_stop = sccp_wrapper_asterisk_rtp_stop,
	.rtp_audio_create = sccp_wrapper_asterisk_create_audio_rtp,
	.rtp_video_create = sccp_wrapper_asterisk_create_video_rtp,
	.rtp_get_payloadType = sccp_wrapper_asterisk_get_payloadType,

	.pbx_get_callerid_name = sccp_wrapper_asterisk_get_callerid_name,
	.pbx_get_callerid_number = sccp_wrapper_asterisk_get_callerid_number,
	.pbx_get_callerid_ani = NULL,						/*! \todo implement callbacks */
	.pbx_get_callerid_dnid = NULL,						/*! implement callbacks */
	.pbx_get_callerid_rdnis = NULL,						/*! implement callbacks */

	.pbx_set_callerid_name = sccp_wrapper_asterisk_set_callerid_name,
	.pbx_set_callerid_number = sccp_wrapper_asterisk_set_callerid_number,
	.pbx_set_callerid_ani = sccp_wrapper_asterisk_set_callerid_ani,
	.pbx_set_callerid_dnid = sccp_wrapper_asterisk_set_callerid_dnid,
	.pbx_set_callerid_rdnis = sccp_wrapper_asterisk_set_callerid_rdnis,
};

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/************************************************************************************************************ CALLERID **/
struct ast_channel *sccp_asterisk_channel_search_locked(int (*is_match) (struct ast_channel *, void *), void *data)
{
#ifdef ast_channel_search_locked
	return ast_channel_search_locked(is_match, data);
#else
	struct ast_channel *ast = NULL;

	while ((ast = ast_channel_walk_locked(ast))) {
		if (is_match(ast, data)) {
			break;
		}
		ast_channel_unlock(ast);
	}
	return ast;
#endif
}

/*!
 * \brief get callerid_name from pbx
 * \param channel SCCP Channel
 * \return char * with the callername
 */
char *sccp_wrapper_asterisk_get_callerid_name(const sccp_channel_t * channel)
{
	static char result[StationMaxNameSize];

	PBX_CHANNEL_TYPE *ast_chan = channel->owner;

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

/*!
 * \brief get callerid_name from pbx
 * \param channel SCCP Channel
 * \return char * with the callername
 */
char *sccp_wrapper_asterisk_get_callerid_number(const sccp_channel_t * channel)
{
	static char result[StationMaxDirnumSize];

	PBX_CHANNEL_TYPE *ast_chan = channel->owner;

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
 * \brief get callerid from pbx
 * \param ast_chan Asterisk Channel
 * \return SCCP CallInfo Structure 
 */
sccp_callinfo_t *sccp_wrapper_asterisk_get_callerid(PBX_CHANNEL_TYPE * ast_chan)
{
	sccp_callinfo_t *callInfo = NULL;

	if (!ast_chan) {
		return callInfo;
	}
#if ASTERISK_VERSION_NUMBER < 10400
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

/*!
 * \brief set callerid on pbx channel
 * \param ast_chan Asterisk Channel
 * \param callInfo SCCP CallInfo Structure
 * \return Fail/Success as int
 */
int sccp_wrapper_asterisk_set_callerid(PBX_CHANNEL_TYPE * ast_chan, sccp_callinfo_t * callInfo)
{
	int RESULT = 0;

	if (!ast_chan) {
		return -1;
	}

	if (!callInfo) {
		return -1;
	}
#if ASTERISK_VERSION_NUMBER < 10400
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

/*!
 * \brief set callerid name on pbx channel
 * \param pbx_chan PBX Channel
 * \param name Name as char
 */
void sccp_wrapper_asterisk_set_callerid_name(const PBX_CHANNEL_TYPE * pbx_chan, const char *name)
{
	if (!pbx_chan || !name) {
		return;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	// Implement ast 1.2 version
#else
	if (!strcmp(pbx_chan->cid.cid_name, name)) {
		sccp_copy_string(pbx_chan->cid.cid_name, name, sizeof(pbx_chan->cid.cid_name) - 1);
	}
#endif
}

/*!
 * \brief set callerid number on pbx channel
 * \param pbx_chan PBX Channel
 * \param number Number as char
 */
void sccp_wrapper_asterisk_set_callerid_number(const PBX_CHANNEL_TYPE * pbx_chan, const char *number)
{
	if (!pbx_chan || !number) {
		return;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	// Implement ast 1.2 version
#else
	if (!strcmp(pbx_chan->cid.cid_num, number)) {
		sccp_copy_string(pbx_chan->cid.cid_num, number, sizeof(pbx_chan->cid.cid_num) - 1);
	}
#endif
}

/*!
 * \brief set callerid ani on pbx channel
 * \param pbx_chan PBX Channel
 * \param ani Number as char
 */
void sccp_wrapper_asterisk_set_callerid_ani(PBX_CHANNEL_TYPE * pbx_chan, const char *ani)
{
	if (!pbx_chan || !ani) {
		return;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	// Implement ast 1.2 version
#else
	if (pbx_chan->cid.cid_ani)
		ast_free(pbx_chan->cid.cid_ani);

	pbx_chan->cid.cid_ani = strdup(ani);

#endif
}

/*!
 * \brief set callerid dnid on pbx channel
 * \param pbx_chan PBX Channel
 * \param dnid Number as char
 */
void sccp_wrapper_asterisk_set_callerid_dnid(const PBX_CHANNEL_TYPE * pbx_chan, const char *dnid)
{
	if (!pbx_chan || !dnid) {
		return;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	// Implement ast 1.2 version
#else
	if (!strcmp(pbx_chan->cid.cid_dnid, dnid)) {
		sccp_copy_string(pbx_chan->cid.cid_dnid, dnid, sizeof(pbx_chan->cid.cid_dnid) - 1);
	}
#endif
}

/*!
 * \brief set callerid rdnis on pbx channel
 * \param pbx_chan PBX Channel
 * \param rdnis Number as char
 */
void sccp_wrapper_asterisk_set_callerid_rdnis(const PBX_CHANNEL_TYPE * pbx_chan, const char *rdnis)
{
	if (!pbx_chan || !rdnis) {
		return;
	}
#if ASTERISK_VERSION_NUMBER < 10400
	// Implement ast 1.2 version
#else
	if (!strcmp(pbx_chan->cid.cid_rdnis, rdnis)) {
		sccp_copy_string(pbx_chan->cid.cid_rdnis, rdnis, sizeof(pbx_chan->cid.cid_rdnis) - 1);
	}
#endif
}

/************************************************************************************************************* CHANNEL **/

/*!
 * \brief itterate through locked pbx channels
 * \note replacement for ast_channel_walk_locked
 * \param target Target Asterisk Channel
 * \return ast_chan Locked Asterisk Channel
 */
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target)
{
	return ast_channel_walk_locked(target);
}

/************************************************************************************************************** CONFIG **/

/*!
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
#if ASTERISK_VERSION_NUMBER < 10600
	return ast_append_ha(sense, (char *)stuff, path);
#else
	return ast_append_ha(sense, stuff, path, error);
#endif
}

/*!
 * \brief Register a new context
 * \note replacement for ast_context_find_or_create
 *
 * This will first search for a context with your name.  If it exists already, it will not
 * create a new one.  If it does not exist, it will create a new one with the given name
 * and registrar.
 *
 * \param extcontexts pointer to the ast_context structure pointer
 * \param exttable Asterisk Extension Hashtable
 * \param name Context Name
 * \param registrar registrar of the context
 * \return NULL on failure, and an ast_context structure on success
 */
struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar)
{
#if ASTERISK_VERSION_NUMBER < 10600
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
#if ASTERISK_VERSION_NUMBER < 10600
	return ast_config_load(filename);
#else
	return ast_config_load2(filename, who_asked, flags);
#endif
}

/*************************************************************************************************************** CODEC **/

/*! 
 * \brief Get the name of a format
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
#endif
}

/*!
 * \brief Get/Create new config variable
 *
 * \note replacement for ast_variable_new
 *
 * \param v V as Array ofStruct Ast Variable 
 * \return The return value is struct ast_variable.
 */
struct ast_variable *pbx_variable_new(struct ast_variable *v)
{
#if ASTERISK_VERSION_NUMBER >= 10600
	return ast_variable_new(v->name, v->value, v->file);
#else
	return ast_variable_new(v->name, v->value);
#endif
}

/******************************************************************************************************** NET / SOCKET **/

/*!
 * \brief thread-safe replacement for inet_ntoa()
 * \note replacement for ast_pbx_inet_ntoa
 * \param ia In Address / Source Address
 * \return Address as char
 */
const char *pbx_inet_ntoa(struct in_addr ia)
{
#if ASTERISK_VERSION_NUMBER < 10400
	char iabuf[INET_ADDRSTRLEN];

	return ast_inet_ntoa(iabuf, sizeof(iabuf), ia);
#else
	return ast_inet_ntoa(ia);
#endif
}

#if ASTERISK_VERSION_NUMBER < 10400

/*!
 * \brief BackPort of ast_str2cos & ast_str2cos for asterisk 1.2
 */
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

#if ASTERISK_VERSION_NUMBER < 10600
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
 * \param registrar registrar of the extension
 *
 * This function removes an extension from a given context.
 *
 * \retval 0 on success 
 * \retval -1 on failure
 * 
 */
int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar)
{
#if ASTERISK_VERSION_NUMBER >= 10600
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
 * \param s s as struct mansession
 * \param m m as const struct message
 * \param msg Message as char *
 * \param listflag listflag as char *
 */
#ifdef CS_MANAGER_EVENTS
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag)
{
#if ASTERISK_VERSION_NUMBER < 10600
	astman_send_ack(s, m, msg);
#else
	astman_send_listack(s, m, msg, listflag);
#endif
}
#endif

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
int pbx_moh_start(PBX_CHANNEL_TYPE * chan, const char *mclass, const char *interpclass)
{
#if ASTERISK_VERSION_NUMBER < 10400
	return ast_moh_start(chan, interpclass);
#else
	return ast_moh_start(chan, mclass, interpclass);
#endif
}

int sccp_wrapper_asterisk_rtp_stop(sccp_channel_t * channel)
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

/***************************************************************************************************************** RTP **/

/*!
 * \brief pbx rtp get peer
 * Copies from rtp to them and returns 1 if there was a change or 0 if it was already the same
 *
 * \note replacement for ast_rtp_get_peer
 *
 * \param rtp Asterisk RTP Struct
 * \param addr Socket Addr_in of the Peer
 * \return int
 */
int pbx_rtp_get_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr)
{
	return ast_rtp_get_peer(rtp, addr);
}

/*!
 * \brief pbx rtp get us
 *
 * \note replacement for ast_rtp_get_peer
 *
 * \param rtp Asterisk RTP Struct
 * \param addr Socket Addr_in of the Peer
 * \return int
 */
void pbx_rtp_get_us(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr)
{
	ast_rtp_get_us(rtp, addr);
}

/*!
 * \brief pbx rtp set peer
 * \note replacement for ast_rtp_set_peer
 * \param rtp Asterisk RTP Struct
 * \param addr Socket Addr_in of the Peer
 */
void pbx_rtp_set_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr)
{
	ast_rtp_set_peer(rtp, addr);
}

/*!
 * \brief Create a new RTP Source.
 * \param c SCCP Channel
 * \param new_rtp new rtp source as struct ast_rtp **
 */
boolean_t sccp_wrapper_asterisk_create_audio_rtp(const sccp_channel_t * c, struct ast_rtp **new_rtp)
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
		ast_queue_frame(c->owner, &sccp_null_frame);
	}

	if (c->rtp.audio.rtp) {
		ast_rtp_codec_setpref(*new_rtp, (struct ast_codec_pref *)&c->codecs);
		ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
		sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	}
#endif
	if (*new_rtp) {
#if ASTERISK_VERSION_NUMBER >= 10600
		ast_rtp_setqos(*new_rtp, c->line->audio_tos, c->line->audio_cos, "SCCP RTP");
#else
		ast_rtp_settos(*new_rtp, c->line->audio_tos);
#endif
		ast_rtp_setnat(*new_rtp, d->nat);
	}

	return TRUE;
}

/*!
 * \brief Create a new RTP Source.
 * \param c SCCP Channel
 * \param new_rtp new rtp source as struct ast_rtp **
 */
boolean_t sccp_wrapper_asterisk_create_video_rtp(const sccp_channel_t * c, struct ast_rtp ** new_rtp)
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
		ast_queue_frame(c->owner, &sccp_null_frame);
	}

	if (*new_rtp) {
		ast_rtp_codec_setpref(*new_rtp, (struct ast_codec_pref *)&c->codecs);
		ast_codec_pref_string((struct ast_codec_pref *)&c->codecs, pref_buf, sizeof(pref_buf) - 1);
		sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, set pef: %s\n", c->line->name, c->callid, pref_buf);
	}
#endif
	if (*new_rtp) {
#if ASTERISK_VERSION_NUMBER >= 10600
		ast_rtp_setqos(*new_rtp, c->line->video_tos, c->line->video_cos, "SCCP VRTP");
#else
		ast_rtp_settos(*new_rtp, c->line->video_tos);
#endif
		ast_rtp_setnat(*new_rtp, d->nat);
#if ASTERISK_VERSION_NUMBER >= 10600
		ast_rtp_codec_setpref(c->rtp.video.rtp, (struct ast_codec_pref *)&c->codecs);
#endif

	}

	return TRUE;
}

#if ASTERISK_VERSION_NUMBER >= 10400

/*!
 * \brief Get RTP Peer from Channel
 * \param ast Asterisk Channel
 * \param rtp Asterisk RTP
 * \return ENUM of RTP Result
 * 
 * \called_from_asterisk
 */
enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
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

#    ifdef CS_AST_HAS_RTP_ENGINE
	ao2_ref(*rtp, +1);
#    endif

	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		return AST_RTP_GET_FAILED;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (sccp_channel_get_rtp_peer) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		return AST_RTP_TRY_PARTIAL;
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
#    define _RTP_GET_FAILED AST_RTP_GET_FAILED
enum ast_rtp_get_result sccp_wrapper_asterisk_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
#else
#    define _RTP_GET_FAILED AST_RTP_GLUE_RESULT_FORBID
enum ast_rtp_glue_result sccp_wrapper_asterisk_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp)
#endif
{
	sccp_channel_t *c = NULL;
	sccp_rtp_info_t rtpInfo;
	struct sccp_rtp *videoRTP = NULL;

	enum ast_rtp_get_result res = AST_RTP_TRY_NATIVE;

	if (NULL == ast) {
		res = _RTP_GET_FAILED;
		goto finished;
	}

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		res = _RTP_GET_FAILED;
		goto finished;
	}

	rtpInfo = sccp_rtp_getVideoPeerInfo(c, &videoRTP);
	if (rtpInfo == SCCP_RTP_INFO_NORTP) {
		res = _RTP_GET_FAILED;
		goto finished;
	}

	*rtp = videoRTP->rtp;
	if (!*rtp) {
		res = AST_RTP_GET_FAILED;
		goto finished;
	}
#ifdef CS_AST_HAS_RTP_ENGINE
	ao2_ref(*rtp, +1);
#endif

	if (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk_get_vrtp_peer) JitterBuffer is Forced. AST_RTP_GET_FAILED\n");
		res = AST_RTP_GET_FAILED;
		goto finished;
	}

	if (!(rtpInfo & SCCP_RTP_INFO_ALLOW_DIRECTRTP)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk_get_vrtp_peer) Direct RTP disabled ->  Using AST_RTP_TRY_PARTIAL for channel %s\n", ast->name);
		res = AST_RTP_TRY_PARTIAL;
		goto finished;
	}
 finished:

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_1 "SCCP: (asterisk_get_vrtp_peer) %s result : %d\n", ast->name, res);
	return res;
}

#if ASTERISK_VERSION_NUMBER >= 10400 && ASTERISK_VERSION_NUMBER < 10600

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
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, int codecs, int nat_active)
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
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active)
#endif
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l = NULL;

	sccp_device_t *d = NULL;

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
		sccp_rtp_set_peer(c, &them);
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

uint8_t sccp_wrapper_asterisk_get_payloadType(const struct sccp_rtp * rtp, skinny_media_payload codec)
{
	uint32_t asteriskCodec = sccp_codec_skinny2ast(codec);

	return ast_rtp_lookup_code(rtp->rtp, 1, asteriskCodec);
}

boolean_t sccp_wrapper_asterisk_allocPBXChannel(const sccp_channel_t * channel, void **pbx_channel)
{
#if ASTERISK_VERSION_NUMBER < 10400
	*pbx_channel = ast_channel_alloc(0);
#else
	*pbx_channel = ast_channel_alloc(0, AST_STATE_DOWN, channel->line->cid_num, channel->line->cid_name, channel->line->accountcode, channel->dialedNumber, channel->line->context, channel->line->amaflags, "SCCP/%s-%08X", channel->line->name, channel->callid);
#endif

	if (*pbx_channel != NULL)
		return TRUE;

	return FALSE;
}

int sccp_wrapper_asterisk_setCallState(const sccp_channel_t * channel, int state)
{
	sccp_ast_setstate(channel, state);
	return 0;
}
