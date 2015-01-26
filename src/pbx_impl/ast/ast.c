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
 * $Date$
 * $Revision$  
 */
#include <config.h>
#include "../../common.h"
#include "../../sccp_device.h"
#include "../../sccp_channel.h"
#include "../../sccp_utils.h"
#include "../../sccp_indicate.h"
#include "../../sccp_socket.h"
#include "../../sccp_pbx.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");

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
		tmp = pbx_channel_unref(tmp);
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

	for (; iter && (tmp = ast_channel_iterator_next(iter)); tmp = pbx_channel_unref(tmp)) {
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
		tmp = pbx_channel_unref(tmp);
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

//static boolean_t sccp_wrapper_asterisk_nullHangup(sccp_channel_t *channel)
//{
//      return FALSE;
//}

static boolean_t sccp_wrapper_asterisk_carefullHangup(sccp_channel_t * c)
{
	boolean_t res = FALSE;

	if (!c || !c->owner) {
		return FALSE;
	}
	AUTO_RELEASE sccp_channel_t *channel = sccp_channel_retain(c);

	if (channel && channel->owner) {
		PBX_CHANNEL_TYPE *pbx_channel = pbx_channel_ref(channel->owner);

		sccp_channel_stop_and_deny_scheduled_tasks(channel);

		/* let's wait for a bit, for the dust to settle */
		sched_yield();
		pbx_safe_sleep(pbx_channel, 1000);

		/* recheck everything before going forward */
		pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_carefullHangup) processing hangup request, using carefull version. Standby.\n", pbx_channel_name(pbx_channel));
		if (!pbx_check_hangup(pbx_channel)) {
			pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_carefullHangup) Channel still active.\n", pbx_channel_name(pbx_channel));
			if (pbx_channel_pbx(pbx_channel) || pbx_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_BLOCKING) || pbx_channel_state(pbx_channel) == AST_STATE_UP) {
				pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_carefullHangup) Has PBX -> ast_queue_hangup.\n", pbx_channel_name(pbx_channel));
				res = ast_queue_hangup(pbx_channel) ? FALSE : TRUE;
			} else {
				pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_carefullHangup) Has no PBX -> ast_hangup.\n", pbx_channel_name(pbx_channel));
				ast_hangup(pbx_channel);
				res = TRUE;
			}
		} else {
			pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_carefullHangup) Already Hungup. Forcing SCCP Remove Call.\n", pbx_channel_name(pbx_channel));
			AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

			if (d) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
			}
			res = TRUE;
		}
		pbx_channel_unref(pbx_channel);
	}
	return res;
}

boolean_t sccp_wrapper_asterisk_requestQueueHangup(sccp_channel_t * c)
{
	boolean_t res = FALSE;
	AUTO_RELEASE sccp_channel_t *channel = sccp_channel_retain(c);

	if (channel) {
		PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

		sccp_channel_stop_and_deny_scheduled_tasks(channel);

		channel->hangupRequest = sccp_wrapper_asterisk_carefullHangup;
		if (!pbx_check_hangup(pbx_channel)) {								/* if channel is not already been hungup */
			res = ast_queue_hangup(pbx_channel) ? FALSE : TRUE;
		} else {
			pbx_log(LOG_NOTICE, "%s: (sccp_wrapper_asterisk_requestQueueHangup) Already Hungup\n", channel->designator);
			AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

			if (d) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
			}
		}
	}
	return res;
}

boolean_t sccp_wrapper_asterisk_requestHangup(sccp_channel_t * c)
{
	boolean_t res = FALSE;
	AUTO_RELEASE sccp_channel_t *channel = sccp_channel_retain(c);

	if (channel) {
		PBX_CHANNEL_TYPE *pbx_channel = channel->owner;

		sccp_channel_stop_and_deny_scheduled_tasks(channel);

		channel->hangupRequest = sccp_wrapper_asterisk_carefullHangup;
		if (!pbx_check_hangup(pbx_channel)) {
			if (pbx_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_BLOCKING)) {
				return sccp_wrapper_asterisk_requestQueueHangup(channel);
			}
			ast_hangup(pbx_channel);
			res = TRUE;
		} else {
			AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

			if (d) {
				sccp_indicate(d, channel, SCCP_CHANNELSTATE_ONHOOK);
			}
		}
	}
	return res;
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
			
		} else if (!strcasecmp(args, "video")) {
			res = sccp_channel_setVideoMode(c, value);
			
		} else if (!strcasecmp(args, "CallingParty")) {
			char *num, *name;

			pbx_callerid_parse((char *) value, &name, &num);
			sccp_channel_set_callingparty(c, name, num);
			sccp_channel_display_callInfo(c);
		} else if (!strcasecmp(args, "CalledParty")) {
			char *num, *name;

			pbx_callerid_parse((char *) value, &name, &num);
			sccp_channel_set_calledparty(c, name, num);
			sccp_channel_display_callInfo(c);
		} else if (!strcasecmp(args, "OriginalCallingParty")) {
			char *num, *name;

			pbx_callerid_parse((char *) value, &name, &num);
			sccp_channel_set_originalCallingparty(c, name, num);
			sccp_channel_display_callInfo(c);
		} else if (!strcasecmp(args, "OriginalCalledParty")) {
			char *num, *name;

			pbx_callerid_parse((char *) value, &name, &num);
			sccp_channel_set_originalCalledparty(c, name, num);
			sccp_channel_display_callInfo(c);
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

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key) || sccp_strlen_zero(value)) {
		return FALSE;
	}
	res = ast_db_put(family, key, value);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_getFromDatabase(const char *family, const char *key, char *out, int outlen)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) {
		return FALSE;
	}
	res = ast_db_get(family, key, out, outlen);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_removeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) {
		return FALSE;
	}
	res = ast_db_del(family, key);
	return (!res) ? TRUE : FALSE;
}

boolean_t sccp_asterisk_removeTreeFromDatabase(const char *family, const char *key)
{
	int res;

	if (sccp_strlen_zero(family) || sccp_strlen_zero(key)) {
		return FALSE;
	}
	res = ast_db_deltree(family, key);
	return (!res) ? TRUE : FALSE;
}

/* end - database */

/*!
 * \brief Turn on music on hold on a given channel 
 * \note replacement for ast_moh_start
 *
 * \param pbx_channel The channel structure that will get music on hold
 * \param mclass The class to use if the musicclass is not currently set on the channel structure.
 * \param interpclass The class to use if the musicclass is not currently set on the channel structure or in the mclass argument.
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

	sccp_log((DEBUGCAT_PBX)) (VERBOSE_PREFIX_3 "%s: Got redirecting update. From %s<%s>; To %s<%s>\n", pbx_channel_name(ast), (redirecting_from.name.valid && redirecting_from.name.str) ? redirecting_from.name.str : "", (redirecting_from.number.valid && redirecting_from.number.str) ? redirecting_from.number.str : "", (redirecting_to.name.valid && redirecting_to.name.str) ? redirecting_to.name.str : "",
				  (redirecting_to.number.valid && redirecting_to.number.str) ? redirecting_to.number.str : "");

	if (redirecting_from.name.valid && redirecting_from.name.str) {
		sccp_copy_string(channel->callInfo.lastRedirectingPartyName, redirecting_from.name.str, sizeof(channel->callInfo.callingPartyName));
	}

	sccp_copy_string(channel->callInfo.lastRedirectingPartyNumber, (redirecting_from.number.valid && redirecting_from.number.str) ? redirecting_from.number.str : "", sizeof(channel->callInfo.lastRedirectingPartyNumber));
	channel->callInfo.lastRedirectingParty_valid = 1;
#else
	sccp_copy_string(channel->callInfo.lastRedirectingPartyNumber, ast->cid.cid_rdnis ? ast->cid.cid_rdnis : "", sizeof(channel->callInfo.lastRedirectingPartyNumber));
	channel->callInfo.lastRedirectingParty_valid = 1;
#endif
	channel->callInfo.originalCdpnRedirectReason = channel->callInfo.lastRedirectingReason;
	channel->callInfo.lastRedirectingReason = 4;								// need to figure out these codes

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
 */
int sccp_wrapper_asterisk_channel_read(PBX_CHANNEL_TYPE * ast, NEWCONST char *funcname, char *preparse, char *buf, size_t buflen)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	int res = 0;

	char *parse = sccp_strdupa(preparse);

	AST_DECLARE_APP_ARGS(args, AST_APP_ARG(param); AST_APP_ARG(type); AST_APP_ARG(field););
	AST_STANDARD_APP_ARGS(args, parse);

	if (!ast || !CS_AST_CHANNEL_PVT_IS_SCCP(ast)) {
		pbx_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}

	if ((c = get_sccp_channel_from_pbx_channel(ast))) {
		if ((d = sccp_channel_getDevice_retained(c))) {
			if (!strcasecmp(args.param, "peerip")) {
				sccp_copy_string(buf, sccp_socket_stringify(&d->session->sin), buflen);
			} else if (!strcasecmp(args.param, "recvip")) {
				ast_copy_string(buf, sccp_socket_stringify(&d->session->sin), buflen);
			} else if (!strcasecmp(args.param, "useragent")) {
				sccp_copy_string(buf, skinny_devicetype2str(d->skinny_type), buflen);
			} else if (!strcasecmp(args.param, "from")) {
				sccp_copy_string(buf, (char *) d->id, buflen);
#if ASTERISK_VERSION_GROUP >= 108
			} else if (!strcasecmp(args.param, "rtpqos")) {
				PBX_RTP_TYPE *rtp = NULL;

				if (sccp_strlen_zero(args.type)) {
					args.type = "audio";
				}

				if (sccp_strcaseequals(args.type, "audio")) {
					rtp = c->rtp.audio.rtp;
				} else if (sccp_strcaseequals(args.type, "video")) {
					rtp = c->rtp.video.rtp;
				/*
				} else if (sccp_strcaseequals(args.type, "text")) {
					rtp = c->rtp.text.rtp;
				*/
				} else {
					return -1;
				}

				if (sccp_strlen_zero(args.field) || sccp_strcaseequals(args.field, "all")) {
					char quality_buf[256 /*AST_MAX_USER_FIELD */ ];

					if (!ast_rtp_instance_get_quality(rtp, AST_RTP_INSTANCE_STAT_FIELD_QUALITY, quality_buf, sizeof(quality_buf))) {
						return -1;
					}

					sccp_copy_string(buf, quality_buf, buflen);
					return res;
				} else {
					struct ast_rtp_instance_stats stats;
					int i;
					struct {
						const char *name;
						enum { INT, DBL } type;
						union {
							unsigned int *i4;
							double *d8;
						};
					} lookup[] = {
						{
							"txcount", INT, {
						.i4 = &stats.txcount,},}, {
							"rxcount", INT, {
						.i4 = &stats.rxcount,},}, {
							"txjitter", DBL, {
						.d8 = &stats.txjitter,},}, {
							"rxjitter", DBL, {
						.d8 = &stats.rxjitter,},}, {
							"remote_maxjitter", DBL, {
						.d8 = &stats.remote_maxjitter,},}, {
							"remote_minjitter", DBL, {
						.d8 = &stats.remote_minjitter,},}, {
							"remote_normdevjitter", DBL, {
						.d8 = &stats.remote_normdevjitter,},}, {
							"remote_stdevjitter", DBL, {
						.d8 = &stats.remote_stdevjitter,},}, {
							"local_maxjitter", DBL, {
						.d8 = &stats.local_maxjitter,},}, {
							"local_minjitter", DBL, {
						.d8 = &stats.local_minjitter,},}, {
							"local_normdevjitter", DBL, {
						.d8 = &stats.local_normdevjitter,},}, {
							"local_stdevjitter", DBL, {
						.d8 = &stats.local_stdevjitter,},}, {
							"txploss", INT, {
						.i4 = &stats.txploss,},}, {
							"rxploss", INT, {
						.i4 = &stats.rxploss,},}, {
							"remote_maxrxploss", DBL, {
						.d8 = &stats.remote_maxrxploss,},}, {
							"remote_minrxploss", DBL, {
						.d8 = &stats.remote_minrxploss,},}, {
							"remote_normdevrxploss", DBL, {
						.d8 = &stats.remote_normdevrxploss,},}, {
							"remote_stdevrxploss", DBL, {
						.d8 = &stats.remote_stdevrxploss,},}, {
							"local_maxrxploss", DBL, {
						.d8 = &stats.local_maxrxploss,},}, {
							"local_minrxploss", DBL, {
						.d8 = &stats.local_minrxploss,},}, {
							"local_normdevrxploss", DBL, {
						.d8 = &stats.local_normdevrxploss,},}, {
							"local_stdevrxploss", DBL, {
						.d8 = &stats.local_stdevrxploss,},}, {
							"rtt", DBL, {
						.d8 = &stats.rtt,},}, {
							"maxrtt", DBL, {
						.d8 = &stats.maxrtt,},}, {
							"minrtt", DBL, {
						.d8 = &stats.minrtt,},}, {
							"normdevrtt", DBL, {
						.d8 = &stats.normdevrtt,},}, {
							"stdevrtt", DBL, {
						.d8 = &stats.stdevrtt,},}, {
							"local_ssrc", INT, {
						.i4 = &stats.local_ssrc,},}, {
							"remote_ssrc", INT, {
						.i4 = &stats.remote_ssrc,},}, {
					NULL,},};

					if (ast_rtp_instance_get_stats(rtp, &stats, AST_RTP_INSTANCE_STAT_ALL)) {
						return -1;
					}

					for (i = 0; !sccp_strlen_zero(lookup[i].name); i++) {
						if (sccp_strcaseequals(args.field, lookup[i].name)) {
							if (lookup[i].type == INT) {
								snprintf(buf, buflen, "%u", *lookup[i].i4);
							} else {
								snprintf(buf, buflen, "%f", *lookup[i].d8);
							}
							return 0;
						}
					}
					pbx_log(LOG_WARNING, "SCCP: (sccp_wrapper_asterisk_channel_read) Unrecognized argument '%s' to %s\n", preparse, funcname);
					return -1;
				}
#endif
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

boolean_t sccp_wrapper_asterisk_featureMonitor(const sccp_channel_t * channel)
{
#if ASTERISK_VERSION_GROUP >= 112
	char *featexten;

	if (PBX(getFeatureExtension) (channel, &featexten)) {
		struct ast_frame f = { AST_FRAME_DTMF, };
		int j;

		f.len = 100;
		for (j = 0; j < strlen(featexten); j++) {
			f.subclass.integer = featexten[j];
			ast_queue_frame(channel->owner, &f);
		}
		sccp_free(featexten);
		return TRUE;
	}
#else
#ifdef CS_EXPERIMENTAL		// Added 2015/01/24
	ast_rdlock_call_features();
	
	struct ast_call_feature *feature = ast_find_call_feature("automixmon");

	if (feature) {
		struct ast_call_feature feat;
		memcpy(&feat, feature, sizeof(feat));
		ast_unlock_call_features();
		
		feat.operation(channel->owner, CS_AST_BRIDGED_CHANNEL(channel->owner), NULL, "monitor button", FEATURE_SENSE_CHAN | FEATURE_SENSE_PEER, NULL);
		return TRUE;
	}
#else				// Old Impl
	struct ast_call_feature *feature = ast_find_call_feature("automon");

	if (feature) {
		feature->operation(channel->owner, channel->owner, NULL, "monitor button", 0, NULL);
		return TRUE;
	}
#endif
#endif
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Automon not available in features.conf/n", channel->designator);
	return FALSE;

}

#if !defined(AST_DEFAULT_EMULATE_DTMF_DURATION)
#define AST_DEFAULT_EMULATE_DTMF_DURATION 100
#endif
#if ASTERISK_VERSION_GROUP > 106
int sccp_wrapper_sendDigits(const sccp_channel_t * channel, const char *digits)
{
	if (!channel || !channel->owner) {
		pbx_log(LOG_WARNING, "No channel to send digits to\n");
		return 0;
	}
	if (!digits || sccp_strlen_zero(digits)) {
		pbx_log(LOG_WARNING, "No digits to send\n");
		return 0;
	}
	//ast_channel_undefer_dtmf(channel->owner);
	PBX_CHANNEL_TYPE *pbx_channel = channel->owner;
	int i;
	PBX_FRAME_TYPE f = ast_null_frame;

	sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Sending digits '%s'\n", (char *) channel->currentDeviceId, digits);
	// We don't just call sccp_pbx_senddigit due to potential overhead, and issues with locking
	f.src = "SCCP";
	for (i = 0; digits[i] != '\0'; i++) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Sending digit %c\n", (char *) channel->currentDeviceId, digits[i]);

		f.frametype = AST_FRAME_DTMF_END;								// Sending only the dmtf will force asterisk to start with DTMF_BEGIN and schedule the DTMF_END
		f.subclass.integer = digits[i];
		// f.len = SCCP_MIN_DTMF_DURATION;
		f.len = AST_DEFAULT_EMULATE_DTMF_DURATION;
		f.src = "SEND DIGIT";
		ast_queue_frame(pbx_channel, &f);
	}
	return 1;
}

int sccp_wrapper_sendDigit(const sccp_channel_t * channel, const char digit)
{
	const char digits[] = { digit, '\0' };
	sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: got a single digit '%c' -> '%s'\n", channel->currentDeviceId, digit, digits);
	return sccp_wrapper_sendDigits(channel, digits);
}
#endif

static void *sccp_asterisk_doPickupThread(void *data)
{
	PBX_CHANNEL_TYPE *pbx_channel = data;

	if (ast_pickup_call(pbx_channel)) {
		pbx_channel_set_hangupcause(pbx_channel, AST_CAUSE_CALL_REJECTED);
	} else {
		pbx_channel_set_hangupcause(pbx_channel, AST_CAUSE_NORMAL_CLEARING);
	}
	ast_hangup(pbx_channel);
	pbx_channel_unref(pbx_channel);
	pbx_channel = NULL;
	return NULL;
}

static int sccp_asterisk_doPickup(PBX_CHANNEL_TYPE * pbx_channel)
{
	pthread_t threadid;

	if (!pbx_channel || !(pbx_channel_ref(pbx_channel) > 0)) {
		return FALSE;
	}
	if (ast_pthread_create_detached_background(&threadid, NULL, sccp_asterisk_doPickupThread, pbx_channel)) {
		pbx_log(LOG_ERROR, "Unable to start Group pickup thread on channel %s\n", pbx_channel_name(pbx_channel));
		pbx_channel_unref(pbx_channel);
		return FALSE;
	}
	pbx_log(LOG_NOTICE, "SCCP: Started Group pickup thread on channel %s\n", pbx_channel_name(pbx_channel));
	return TRUE;
}

enum ast_pbx_result pbx_pbx_start(PBX_CHANNEL_TYPE * pbx_channel)
{
	enum ast_pbx_result res = AST_PBX_FAILED;
	sccp_channel_t *channel = NULL;

	if (!pbx_channel) {
		pbx_log(LOG_ERROR, "SCCP: (pbx_pbx_start) called without pbx channel\n");
		return res;
	}

	if ((channel = get_sccp_channel_from_pbx_channel(pbx_channel))) {
		ast_channel_lock(pbx_channel);
#if ASTERISK_VERSION_GROUP >= 111
		struct ast_callid *callid = NULL;

		channel->pbx_callid_created = ast_callid_threadstorage_auto(&callid);
		ast_channel_callid_set(pbx_channel, callid);
#endif
		// check if the pickup extension was entered
		const char *dialedNumber = PBX(getChannelExten) (channel);
		char *pickupexten;

		if (PBX(getPickupExtension) (channel, &pickupexten) && sccp_strequals(dialedNumber, pickupexten)) {
			if (sccp_asterisk_doPickup(pbx_channel)) {
				res = AST_PBX_SUCCESS;
			}
			ast_channel_unlock(pbx_channel);
			channel = sccp_channel_release(channel);
			sccp_free(pickupexten);
			goto EXIT;
		}
		// channel->hangupRequest = sccp_wrapper_asterisk_dummyHangup;
		channel->hangupRequest = sccp_wrapper_asterisk_carefullHangup;
		res = ast_pbx_start(pbx_channel);								// starting ast_pbx_start with a locked ast_channel so we know exactly where we end up when/if the __ast_pbx_run get started
		if (res == 0) {											// thread started successfully
			do {											// wait for thread to become ready
				pbx_safe_sleep(pbx_channel, 10);
			} while (!pbx_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_IN_AUTOLOOP) && !pbx_channel_pbx(pbx_channel) && pbx_check_hangup(pbx_channel));

			/* test if __ast_pbx_run got started correctly and if the channel has not already been hungup */
			// if (pbx_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_IN_AUTOLOOP) && pbx_channel_pbx(pbx_channel) && !pbx_check_hangup(pbx_channel)) {
			if (pbx_channel_pbx(pbx_channel) && !pbx_check_hangup(pbx_channel)) {
				sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "%s: (pbx_pbx_start) autoloop has started, set requestHangup = requestQueueHangup\n", channel->designator);
				channel->hangupRequest = sccp_wrapper_asterisk_requestQueueHangup;
			} else {
				pbx_log(LOG_NOTICE, "%s: (pbx_pbx_start) pbx_pbx_start thread is not running anymore, carefullHangup should remain. This channel will be hungup/being hungup soon\n", channel->designator);
				//pbx_log(LOG_NOTICE, "%s: (pbx_pbx_start) Reason: autoloop: %s, pbx_start: %s, already hungup: %s\n", channel->designator, pbx_test_flag(pbx_channel_flags(pbx_channel), AST_FLAG_IN_AUTOLOOP) ? "TRUE" : "FALSE", pbx_channel_pbx(pbx_channel) ? "TRUE" : "FALSE", !pbx_check_hangup(pbx_channel) ? "TRUE" : "FALSE");
				res = AST_PBX_FAILED;
			}
		}
		ast_channel_unlock(pbx_channel);
		channel = sccp_channel_release(channel);
	}
EXIT:
	return res;
}

// kate: indent-width 4; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off;
