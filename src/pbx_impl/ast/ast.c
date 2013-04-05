
/*!
 * \file        ast.c
 * \brief       SCCP PBX Asterisk Wrapper Class
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
#include <config.h>
#include "../../common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2269 $")

/*
 * \brief itterate through locked pbx channels
 * \note replacement for ast_channel_walk_locked
 * \param ast_chan Asterisk Channel
 * \return ast_chan Locked Asterisk Channel
 *
 * \todo implement pbx_channel_walk_locked or replace
 * \deprecated
 */
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target)
{
#if ASTERISK_VERSION_NUMBER >= 10800
	struct ast_channel_iterator *iter = ast_channel_iterator_all_new();
	PBX_CHANNEL_TYPE *res = NULL;
	PBX_CHANNEL_TYPE *tmp = NULL;

	/* no target given, so just start iteration */
	if (!target) {
		tmp = ast_channel_iterator_next(iter);
	} else {
		/* search for our target channel and use the next iteration value */
		while ((tmp = ast_channel_iterator_next(iter)) != NULL) {
			if (tmp == target) {
				tmp = ast_channel_iterator_next(iter);
				break;
			}
		}
	}

	if (tmp) {
		res = tmp;
		tmp = ast_channel_unref(tmp);
		ast_channel_lock(res);
	}
	ast_channel_iterator_destroy(iter);
	return res;
#else
	return ast_channel_walk_locked(target);
#endif
}

/*
 * \brief iterate through locked pbx channels and search using specified match function
 * \param is_match match function
 * \param data paremeter data for match function
 * \return pbx_channel Locked Asterisk Channel
 *
 * \deprecated
 */
PBX_CHANNEL_TYPE *pbx_channel_search_locked(int (*is_match) (PBX_CHANNEL_TYPE *, void *), void *data)
{
	//#ifdef ast_channel_search_locked
#if ASTERISK_VERSION_NUMBER < 10800
	return ast_channel_search_locked(is_match, data);
#else
	boolean_t matched = FALSE;
	PBX_CHANNEL_TYPE *pbx_channel = NULL;
	PBX_CHANNEL_TYPE *tmp = NULL;

	struct ast_channel_iterator *iter = NULL;

	if (!(iter = ast_channel_iterator_all_new())) {
		return NULL;
	}

	for (; iter && (tmp = ast_channel_iterator_next(iter)); tmp = ast_channel_unref(tmp)) {
		pbx_channel_lock(tmp);
		if (is_match(tmp, data)) {
			matched = TRUE;
			break;
		}
		pbx_channel_unlock(tmp);
	}

	if (iter) {
		ast_channel_iterator_destroy(iter);
	}

	if (matched) {
		pbx_channel = tmp;
		tmp = ast_channel_unref(tmp);
		return pbx_channel;
	} else {
		return NULL;
	}
#endif
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
 *
 * \deprecated
 */
struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error)
{
#if ASTERISK_VERSION_NUMBER < 10600
	return ast_append_ha(sense, stuff, path);
#else
	return ast_append_ha(sense, stuff, path, error);
#endif														// ASTERISK_VERSION_NUMBER
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
#if ASTERISK_VERSION_NUMBER < 10600
	return ast_context_find_or_create(extcontexts, name, registrar);
#else
	return ast_context_find_or_create(extcontexts, exttable, name, registrar);
#endif														// ASTERISK_VERSION_NUMBER
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
#endif														// ASTERISK_VERSION_NUMBER
}

/*!
 * \brief Get/Create new config variable
 * \note replacement for ast_variable_new
 * \param v Variable Name as char
 * \return The return value is struct ast_variable.
 */
PBX_VARIABLE_TYPE *pbx_variable_new(PBX_VARIABLE_TYPE * v)
{
#if ASTERISK_VERSION_NUMBER >= 10600
	return ast_variable_new(v->name, v->value, v->file);
#else
	return ast_variable_new(v->name, v->value);
#endif														// ASTERISK_VERSION_NUMBER
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
#if ASTERISK_VERSION_NUMBER < 10400
	char iabuf[INET_ADDRSTRLEN];

	return ast_inet_ntoa(iabuf, sizeof(iabuf), ia);
#else
	return ast_inet_ntoa(ia);
#endif														// ASTERISK_VERSION_NUMBER
}

#if ASTERISK_VERSION_NUMBER < 10400

/* BackPort of ast_str2cos & ast_str2cos for asterisk 1.2 */
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
#endif														// ASTERISK_VERSION_NUMBER

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
#endif														// ASTERISK_VERSION_NUMBER

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
 * @{
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
#endif														// ASTERISK_VERSION_NUMBER
}

/*!   
 * \brief Send ack in manager list transaction
 * \note replacement for astman_send_listack
 * \param s Management Session
 * \param m Management Message
 * \param msg Message
 * \param listflag List Flag
 */
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag)
{
#if ASTERISK_VERSION_NUMBER < 10600
	astman_send_ack(s, m, msg);
#else
	astman_send_listack(s, m, msg, listflag);
#endif														// ASTERISK_VERSION_NUMBER
}

/****************************************************************************************************** CODEC / FORMAT **/

/*!
 * \brief Convert a ast_codec (fmt) to a skinny codec (enum)
 *
 * \param fmt Format as ast_format_type
 *
 * \return skinny_codec 
 */
skinny_codec_t pbx_codec2skinny_codec(ast_format_type fmt)
{
	uint32_t i;

	for (i = 1; i < ARRAY_LEN(skinny2pbx_codec_maps); i++) {
		if (skinny2pbx_codec_maps[i].pbx_codec == (uint64_t) fmt) {
			return skinny2pbx_codec_maps[i].skinny_codec;
		}
	}
	return 0;
}

/*!
 * \brief Convert a skinny_codec (enum) to an ast_codec (fmt)
 *
 * \param codec Skinny Codec (enum)
 *
 * \return fmt Format as ast_format_type
 */
ast_format_type skinny_codec2pbx_codec(skinny_codec_t codec)
{
	uint32_t i;

	for (i = 1; i < ARRAY_LEN(skinny2pbx_codec_maps); i++) {
		if (skinny2pbx_codec_maps[i].skinny_codec == codec) {
			return skinny2pbx_codec_maps[i].pbx_codec;
		}
	}
	return 0;
}

/*!
 * \brief Convert an array of skinny_codecs (enum) to a bit array of ast_codecs (fmt)
 *
 * \param skinny_codecs Array of Skinny Codecs
 *
 * \return bit array fmt/Format of ast_format_type (int)
 *
 * \todo check bitwise operator (not sure) - DdG 
 */
int skinny_codecs2pbx_codecs(skinny_codec_t * skinny_codecs)
{
	uint32_t i;
	int res_codec = 0;

	for (i = 1; i < SKINNY_MAX_CAPABILITIES; i++) {
		res_codec |= skinny_codec2pbx_codec(skinny_codecs[i]);
	}
	return res_codec;
}

#if DEBUG
/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 *
 * \param pbx_channel PBX Channel
 * \param filename Debug Filename
 * \param lineno Debug Line Number
 * \param func Debug Function Name
 * \return SCCP Channel on Success or Null on Fail
 *
 * \todo this code is not pbx independent
 */
sccp_channel_t *__get_sccp_channel_from_pbx_channel(const PBX_CHANNEL_TYPE * pbx_channel, const char *filename, int lineno, const char *func)
#else
/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 *
 * \param pbx_channel PBX Channel
 * \return SCCP Channel on Success or Null on Fail
 *
 * \todo this code is not pbx independent
 */
sccp_channel_t *get_sccp_channel_from_pbx_channel(const PBX_CHANNEL_TYPE * pbx_channel)
#endif
{
	sccp_channel_t *c = NULL;

	if (pbx_channel && CS_AST_CHANNEL_PVT(pbx_channel) && CS_AST_CHANNEL_PVT_CMP_TYPE(pbx_channel, "SCCP")) {
		if ((c = CS_AST_CHANNEL_PVT(pbx_channel))) {
#if DEBUG
			return sccp_refcount_retain(c, filename, lineno, func);
#else
			return sccp_channel_retain(c);
#endif
		} else {
			pbx_log(LOG_ERROR, "Channel is not a valid SCCP Channel\n");
			return NULL;
		}
	} else {
		return NULL;
	}
}

int sccp_wrapper_asterisk_forceHangup(PBX_CHANNEL_TYPE * ast_channel, pbx_hangup_type_t pbx_hangup_type)
{
	int tries = 0;

	if (!ast_channel) {
		pbx_log(LOG_NOTICE, "no channel to hangup provided. exiting hangup\n");
		return FALSE;
	}

	if (pbx_test_flag(pbx_channel_flags(ast_channel), AST_FLAG_ZOMBIE)) {
		pbx_log(LOG_NOTICE, "%s: channel is a zombie. exiting hangup\n", pbx_channel_name(ast_channel) ? pbx_channel_name(ast_channel) : "--");
		return FALSE;
	}

	if (pbx_test_flag(pbx_channel_flags(ast_channel), AST_FLAG_BLOCKING)) {
		// wait for blocker before issuing softhangup
		while (!pbx_channel_blocker(ast_channel) && tries < 50) {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (requestHangup) Blocker set but no blocker found yet, waiting...!\n");
			usleep(50);
			tries++;
		}
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send ast_softhangup_nolock (blocker: %s)\n", pbx_channel_name(ast_channel), pbx_channel_blockproc(ast_channel));
		ast_softhangup_nolock(ast_channel, AST_SOFTHANGUP_DEV);
		return TRUE;
	}

	if (pbx_channel_softhangup(ast_channel) != 0) {
		if (AST_STATE_DOWN == pbx_channel_state(ast_channel)) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: channel is already being hungup. exiting hangup\n", pbx_channel_name(ast_channel));
			return FALSE;
		} else {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: channel is already being hungup. forcing queued_hangup.\n", pbx_channel_name(ast_channel));
			pbx_hangup_type = PBX_QUEUED_HANGUP;
			pbx_log(LOG_NOTICE, "set to PBX_QUEUED_HANGUP\n");
		}
	}

	/* check for briged pbx_channel */
	PBX_CHANNEL_TYPE *pbx_bridged_channel = NULL;

	if ((pbx_bridged_channel = CS_AST_BRIDGED_CHANNEL(ast_channel))) {
		if (pbx_channel_softhangup(pbx_bridged_channel) != 0) {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: bridge peer: %s is already hanging up. exiting hangup.\n", pbx_channel_name(ast_channel), pbx_channel_name(pbx_bridged_channel));
			return FALSE;
		}
	}

	switch (pbx_hangup_type) {
		case PBX_HARD_HANGUP:
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send hard ast_hangup\n", pbx_channel_name(ast_channel));
			ast_indicate(ast_channel, -1);
			ast_hangup(ast_channel);
			break;
		case PBX_SOFT_HANGUP:
			// wait for blocker before issuing softhangup
			while (!pbx_channel_blocker(ast_channel) && tries < 50) {
				sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (requestHangup) Blocker set but no blocker found yet, waiting...!\n");
				usleep(50);
				tries++;
			}
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send ast_softhangup_nolock (blocker: %s)\n", pbx_channel_name(ast_channel), pbx_channel_blockproc(ast_channel));
			ast_softhangup_nolock(ast_channel, AST_SOFTHANGUP_DEV);
			break;
		case PBX_QUEUED_HANGUP:
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send ast_queue_hangup\n", pbx_channel_name(ast_channel));
#if ASTERISK_VERSION_NUMBER < 10601
			pbx_channel_setwhentohangup_tv(ast_channel, 0);
#else
			pbx_channel_setwhentohangup_tv(ast_channel, ast_tvnow());
#endif
			//                      ast_channel->_state=AST_STATE_DOWN;
			ast_queue_hangup(ast_channel);
			break;
	}

	return TRUE;
}

int sccp_wrapper_asterisk_requestHangup(PBX_CHANNEL_TYPE * ast_channel)
{
	if (!ast_channel) {
		pbx_log(LOG_NOTICE, "channel to hangup is NULL\n");
		return FALSE;
	}

	if ((pbx_channel_softhangup(ast_channel) & AST_SOFTHANGUP_APPUNLOAD) != 0) {
		pbx_channel_set_hangupcause(ast_channel, AST_CAUSE_CHANNEL_UNACCEPTABLE);
		ast_softhangup(ast_channel, AST_SOFTHANGUP_APPUNLOAD);
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send softhangup appunload\n", pbx_channel_name(ast_channel));
		return TRUE;
	}

	sccp_channel_t *sccp_channel = get_sccp_channel_from_pbx_channel(ast_channel);

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "hangup %s: hasPbx %s; ast state: %s, sccp state: %s, blocking: %s, already being hungup: %s, hangupcause: %d\n",
				    pbx_channel_name(ast_channel), pbx_channel_pbx(ast_channel) ? "yes" : "no", pbx_state2str(pbx_channel_state(ast_channel)), sccp_channel ? sccp_indicate2str(sccp_channel->state) : "--", pbx_test_flag(pbx_channel_flags(ast_channel), AST_FLAG_BLOCKING) ? "yes" : "no", pbx_channel_softhangup(ast_channel) ? "yes" : "no", pbx_channel_hangupcause(ast_channel)
	    );

	if (pbx_test_flag(pbx_channel_flags(ast_channel), AST_FLAG_BLOCKING)) {
		// wait for blocker before issuing softhangup
		int tries = 0;

		while (!pbx_channel_blocker(ast_channel) && tries < 50) {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (requestHangup) Blocker set but no blocker found yet, waiting...!\n");
			usleep(50);
			tries++;
		}
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send ast_softhangup_nolock (blocker: %s)\n", pbx_channel_name(ast_channel), pbx_channel_blockproc(ast_channel));
		ast_softhangup_nolock(ast_channel, AST_SOFTHANGUP_DEV);
	} else if (AST_STATE_UP == pbx_channel_state(ast_channel) || pbx_channel_pbx(ast_channel)) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send ast_queue_hangup\n", pbx_channel_name(ast_channel));
#if ASTERISK_VERSION_NUMBER < 10601
		pbx_channel_setwhentohangup_tv(ast_channel, 0);
#else
		pbx_channel_setwhentohangup_tv(ast_channel, ast_tvnow());
#endif
		ast_queue_hangup(ast_channel);
	} else {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: send hard ast_hangup\n", pbx_channel_name(ast_channel));
		ast_hangup(ast_channel);
		//              ast_queue_hangup(ast_channel);
	}

	sccp_channel = sccp_channel ? sccp_channel_release(sccp_channel) : NULL;
	return TRUE;
}

int sccp_asterisk_pbx_fktChannelWrite(PBX_CHANNEL_TYPE * ast, const char *funcname, char *args, const char *value)
{
	sccp_channel_t *c;
	boolean_t res = TRUE;

	if (!(c = get_sccp_channel_from_pbx_channel(ast))) {
		pbx_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	} else {
		if (!strcasecmp(args, "MaxCallBR")) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set max call bitrate to %s\n", (char *) c->currentDeviceId, value);

			if (sscanf(value, "%ud", &c->maxBitRate)) {
				pbx_builtin_setvar_helper(ast, "_MaxCallBR", value);
				res = TRUE;
			} else {
				res = FALSE;
			}

		} else if (!strcasecmp(args, "codec")) {
			res = sccp_channel_setPreferredCodec(c, value);
		} else if (!strcasecmp(args, "microphone")) {
			if (!value || sccp_strlen_zero(value) || !sccp_true(value)) {
				c->setMicrophone(c, FALSE);
			} else {
				c->setMicrophone(c, TRUE);
			}
		} else {
			res = FALSE;
		}
		c = sccp_channel_release(c);
	}
	return res ? 0 : -1;
}

/***** database *****/
boolean_t sccp_asterisk_addToDatabase(const char *family, const char *key, const char *value)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key) || sccp_strlen_zero(value))
		return FALSE;
	res = ast_db_put(family, key, value);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_getFromDatabase(const char *family, const char *key, char *out, int outlen)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key))
		return FALSE;
	res = ast_db_get(family, key, out, outlen);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_removeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key))
		return FALSE;
	res = ast_db_del(family, key);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_removeTreeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key))
		return FALSE;
	res = ast_db_deltree(family, key);
	return (!res) ? TRUE : FALSE;
}

/* end - database */

/*!
 * \brief Turn on music on hold on a given channel 
 * \note replacement for ast_moh_start
 *
 * \param pbx_channel The channel structure that will get music on hold
 * \param mclass The class to use if the musicclass is not currently set on
 *               the channel structure.
 * \param interpclass The class to use if the musicclass is not currently set on
 *                    the channel structure or in the mclass argument.
 *
 * \retval Zero on success
 * \retval non-zero on failure
 */
int sccp_asterisk_moh_start(PBX_CHANNEL_TYPE * pbx_channel, const char *mclass, const char *interpclass)
{
	if (!ast_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_MOH)) {
		pbx_set_flag(pbx_channel_flags(pbx_channel), AST_FLAG_MOH);
		return ast_moh_start((PBX_CHANNEL_TYPE *) pbx_channel, mclass, interpclass);
	} else {
		return 0;
	}
}

void sccp_asterisk_moh_stop(PBX_CHANNEL_TYPE * pbx_channel)
{
	if (ast_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_MOH)) {
		pbx_clear_flag(pbx_channel_flags(pbx_channel), AST_FLAG_MOH);
		ast_moh_stop((PBX_CHANNEL_TYPE *) pbx_channel);
	}
}

void sccp_asterisk_redirectedUpdate(sccp_channel_t * channel, const void *data, size_t datalen)
{
	PBX_CHANNEL_TYPE *ast = channel->owner;

#if ASTERISK_VERSION_GROUP >106
	struct ast_party_id redirecting_from = pbx_channel_redirecting_effective_from(ast);
	struct ast_party_id redirecting_to = pbx_channel_redirecting_effective_to(ast);

	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Got redirecting update. From %s<%s>; To %s<%s>\n", pbx_channel_name(ast), redirecting_from.name.valid ? redirecting_from.name.str : "", redirecting_from.number.valid ? redirecting_from.number.str : "", redirecting_to.name.valid ? redirecting_to.name.str : "", redirecting_to.number.valid ? redirecting_to.number.str : "");

	if (redirecting_from.name.valid) {
		sccp_copy_string(channel->callInfo.lastRedirectingPartyName, redirecting_from.name.str, sizeof(channel->callInfo.callingPartyName));
	}

	sccp_copy_string(channel->callInfo.lastRedirectingPartyNumber, redirecting_from.number.valid ? redirecting_from.number.str : "", sizeof(channel->callInfo.lastRedirectingPartyNumber));
	channel->callInfo.lastRedirectingParty_valid = 1;
#else
	sccp_copy_string(channel->callInfo.lastRedirectingPartyNumber, ast->cid.cid_rdnis ? ast->cid.cid_rdnis : "", sizeof(channel->callInfo.lastRedirectingPartyNumber));
	channel->callInfo.lastRedirectingParty_valid = 1;
#endif

	sccp_channel_send_callinfo2(channel);
}

void sccp_asterisk_sendRedirectedUpdate(const sccp_channel_t * channel, const char *fromNumber, const char *fromName, const char *toNumber, const char *toName, uint8_t reason)
{
#if ASTERISK_VERSION_GROUP >106
	struct ast_party_redirecting redirecting;
	struct ast_set_party_redirecting update_redirecting;

	ast_party_redirecting_init(&redirecting);
	memset(&update_redirecting, 0, sizeof(update_redirecting));

	/* update redirecting info line for source part */
	if (fromNumber) {
		update_redirecting.from.number = 1;
		redirecting.from.number.valid = 1;
		redirecting.from.number.str = strdupa(fromNumber);
	}

	if (fromName) {
		update_redirecting.from.name = 1;
		redirecting.from.name.valid = 1;
		redirecting.from.name.str = strdupa(fromName);
	}

	if (toNumber) {
		update_redirecting.to.number = 1;
		redirecting.to.number.valid = 1;
		redirecting.to.number.str = strdupa(toNumber);
	}

	if (toName) {
		update_redirecting.to.name = 1;
		redirecting.to.name.valid = 1;
		redirecting.to.name.str = strdupa(toName);
	}

	ast_channel_queue_redirecting_update(channel->owner, &redirecting, &update_redirecting);
#else
	// set redirecting party (forwarder)
	if (fromNumber) {
		if (channel->owner->cid.cid_rdnis) {
			ast_free(channel->owner->cid.cid_rdnis);
		}
		channel->owner->cid.cid_rdnis = ast_strdup(fromNumber);
	}
	// where is the call going to now
	if (toNumber) {
		if (channel->owner->cid.cid_dnid) {
			ast_free(channel->owner->cid.cid_dnid);
		}
		channel->owner->cid.cid_dnid = ast_strdup(toNumber);
	}
#endif
}

/*!
 * \brief ACF Channel Read callback
 *
 * \param ast Asterisk Channel
 * \param funcname      functionname as const char *
 * \param args          arguments as char *
 * \param buf           buffer as char *
 * \param buflen        bufferlenght as size_t
 * \return result as int
 *
 * \called_from_asterisk
 * 
 * \test ACF Channel Read Needs to be tested
 */
int sccp_wrapper_asterisk_channel_read(PBX_CHANNEL_TYPE * ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	int res = 0;

	if (!ast || !CS_AST_CHANNEL_PVT_IS_SCCP(ast)) {
		pbx_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}

	if ((c = get_sccp_channel_from_pbx_channel(ast))) {
		if ((d = sccp_channel_getDevice_retained(c))) {
			if (!strcasecmp(args, "peerip")) {
				ast_copy_string(buf, pbx_inet_ntoa(d->session->sin.sin_addr), buflen);
			} else if (!strcasecmp(args, "recvip")) {
				ast_copy_string(buf, pbx_inet_ntoa(d->session->phone_sin.sin_addr), buflen);
			} else if (!strcasecmp(args, "useragent")) {
				sccp_copy_string(buf, devicetype2str(d->skinny_type), buflen);
			} else if (!strcasecmp(args, "from")) {
				sccp_copy_string(buf, (char *) d->id, buflen);
			} else {
				res = -1;
			}
			d = sccp_device_release(d);
		} else {
			res = -1;
		}
		c = sccp_channel_release(c);
	} else {
		res = -1;
	}
	return res;
}

static struct ast_frame *sccp_ast_kill_read(struct ast_channel *chan)
{
	/* Hangup channel. */
	return NULL;
}

static struct ast_frame *sccp_ast_kill_exception(struct ast_channel *chan)
{
	/* Hangup channel. */
	return NULL;
}

static int sccp_ast_kill_write(struct ast_channel *chan, struct ast_frame *frame)
{
	/* Hangup channel. */
	return -1;
}

static int sccp_ast_kill_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	/* No problem fixing up the channel. */
	return 0;
}

static int sccp_ast_kill_hangup(struct ast_channel *chan)
{
#if ASTERISK_VERSION_GROUP > 110
	ast_channel_tech_set(chan, NULL);
#else
	chan->tech_pvt = NULL;
#endif
	return 0;
}

/*!
 * \brief Kill the channel channel driver technology descriptor.
 *
 * \details
 * The purpose of this channel technology is to encourage the
 * channel to hangup as quickly as possible.
 *
 * \note Used by DTMF atxfer and zombie channels.
 */
const struct ast_channel_tech sccp_kill_tech = {
   .type = "SCCPKill",
   .description = "Kill channel (should not see this)",
#if ASTERISK_VERSION_GROUP < 100
   .capabilities = -1,
#endif
   .read = sccp_ast_kill_read,
   .exception = sccp_ast_kill_exception,
   .write = sccp_ast_kill_write,
   .fixup = sccp_ast_kill_fixup,
   .hangup = sccp_ast_kill_hangup,
};

// kate: indent-width 4; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off;
