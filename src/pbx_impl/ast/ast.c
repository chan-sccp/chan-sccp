
/*!
 * \file 	ast.c
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

SCCP_FILE_VERSION(__FILE__, "$Revision: 2269 $")

/*
 * \brief itterate through locked pbx channels
 * \note replacement for ast_channel_walk_locked
 * \param ast_chan Asterisk Channel
 * \return ast_chan Locked Asterisk Channel
 *
 * \todo implement pbx_channel_walk_locked or replace
 * \warning marcello is this the right implementation for pbx_channel_walk_locked. Not sure if i replaced the iterator correctly
 */
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target)
{
#if ASTERISK_VERSION_NUMBER >= 10800
	struct ast_channel_iterator *iter = ast_channel_iterator_all_new();
	PBX_CHANNEL_TYPE *res = NULL;

// 	if (!target) {
// 		if (!(iter = ast_channel_iterator_all_new())) {
// 			return NULL;
// 		}
// 	}
// 	target = ast_channel_iterator_next(iter);
// 	if (!ast_channel_unref(target)) {
// 		ast_channel_lock(target);
// 		return target;
// 	} else {
// 		if (iter) {
// 			ast_channel_iterator_destroy(iter);
// 		}
// 		return NULL;
// 	}
	
	/* no target given, so just start iteration */
	if(!target){
		res = ast_channel_iterator_next(iter);
	}else{
		/* search for our target channel and use the next iteration value */
		while ((res = ast_channel_iterator_next(iter)) != NULL) {
			if(res == target){
				res = ast_channel_iterator_next(iter);
				break;
			}
		}
	}
	
	if (res) {
		ast_channel_unref(res);
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
 */
PBX_CHANNEL_TYPE *pbx_channel_search_locked(int (*is_match) (PBX_CHANNEL_TYPE *, void *), void *data)
{
//#ifdef ast_channel_search_locked
#if ASTERISK_VERSION_NUMBER < 10800
	return ast_channel_search_locked(is_match, data);
#else
	boolean_t matched = FALSE;
	PBX_CHANNEL_TYPE *pbx_channel = NULL;

	struct ast_channel_iterator *iter = NULL;

	if (!(iter = ast_channel_iterator_all_new())) {
		return NULL;
	}

	for (; iter && (pbx_channel = ast_channel_iterator_next(iter)); ast_channel_unref(pbx_channel)) {
		pbx_channel_lock(pbx_channel);
		if (is_match(pbx_channel, data)) {
			matched = TRUE;
			break;
		}
		pbx_channel_unlock(pbx_channel);
	}

	if (iter) {
		ast_channel_iterator_destroy(iter);
	}

	if (matched)
		return pbx_channel;
	else
		return NULL;

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
 */
struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error)
{
#if ASTERISK_VERSION_NUMBER < 10600
	return ast_append_ha(sense, stuff, path);
#else
	return ast_append_ha(sense, stuff, path, error);
#endif										// ASTERISK_VERSION_NUMBER
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
#endif										// ASTERISK_VERSION_NUMBER
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
#endif										// ASTERISK_VERSION_NUMBER
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
#endif										// ASTERISK_VERSION_NUMBER
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
#endif										// ASTERISK_VERSION_NUMBER
}

#if ASTERISK_VERSION_NUMBER < 10400

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
#endif										// ASTERISK_VERSION_NUMBER

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
#endif										// ASTERISK_VERSION_NUMBER

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
#endif										// ASTERISK_VERSION_NUMBER
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
#endif										// ASTERISK_VERSION_NUMBER
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

/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 * \param ast_chan Asterisk Channel
 * \return SCCP Channel on Success or Null on Fail
 */
sccp_channel_t *get_sccp_channel_from_ast_channel(PBX_CHANNEL_TYPE * ast_chan)
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

int sccp_wrapper_asterisk_forceHangup(PBX_CHANNEL_TYPE * ast_channel, pbx_hangup_type_t pbx_hangup_type) {
        if (!ast_channel) {
                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "no channel to hangup provided. exiting hangup\n");
                return FALSE;
        }

        if (pbx_test_flag(ast_channel, AST_FLAG_ZOMBIE)) {
                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: channel is a zombie. exiting hangup\n", ast_channel->name ? ast_channel->name : "--");
                return FALSE;
        }

        if (ast_channel->_softhangup != 0 ) {
                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: channel is already being hungup. exiting hangup\n", ast_channel->name);
                return FALSE;
        }

        /* channel is not running */
/*
        if (!ast_channel->pbx) {
                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: channel is not running. forcing queued_hangup.\n", ast_channel->name);
                return FALSE;
        }
*/
        /* check for briged ast_channel */
        PBX_CHANNEL_TYPE *pbx_bridged_channel = NULL;
        if ((pbx_bridged_channel = CS_AST_BRIDGED_CHANNEL(ast_channel))) {
                if (pbx_bridged_channel->_softhangup != 0) {
                        sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: bridge peer: %s is already hanging up. exiting hangup.\n", ast_channel->name, pbx_bridged_channel->name);
                        return FALSE;
                }
        }

        switch (pbx_hangup_type) {
                case PBX_HARD_HANGUP:
                        sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send hanrd ast_hangup\n", ast_channel->name);
                        ast_hangup(ast_channel);
                        break;
                case PBX_SOFT_HANGUP:
                        sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send ast_softhangup_nolock\n", ast_channel->name);
                        ast_softhangup_nolock(ast_channel, AST_SOFTHANGUP_DEV);
                        break;
                case PBX_QUEUED_HANGUP:
                        sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send ast_queue_hangup\n", ast_channel->name);
                        ast_channel->whentohangup = ast_tvnow();
                        ast_channel->_state=AST_STATE_DOWN;
                        ast_queue_hangup(ast_channel);
                        break;
        }

        return TRUE;
}


int sccp_wrapper_asterisk_requestHangup(PBX_CHANNEL_TYPE * ast_channel)
{
	if (!ast_channel) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "channel to hangup is NULL\n");
		return FALSE;
	}

	sccp_channel_t *sccp_channel = get_sccp_channel_from_pbx_channel(ast_channel);

	if ((ast_channel->_softhangup & AST_SOFTHANGUP_APPUNLOAD) != 0) {
		ast_channel->hangupcause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
		ast_softhangup(ast_channel, AST_SOFTHANGUP_APPUNLOAD);
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send softhangup appunload\n", ast_channel->name);
		return TRUE;
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "hangup %s: hasPbx %s; ast state: %s, sccp state: %s, blocking: %s, already being hungup: %s, hangupcause: %d\n", 
		ast_channel->name, 
		ast_channel->pbx ? "yes" : "no", 
		pbx_state2str(ast_channel->_state), 
		sccp_channel ? sccp_indicate2str(sccp_channel->state) : "--", 
		ast_test_flag(ast_channel, AST_FLAG_BLOCKING) ? "yes" : "no", 
		ast_channel->_softhangup ? "yes" : "no", 
		ast_channel->hangupcause);

	// \todo possible nullpointer dereference ahead (sccp_channel might be null)
	if (AST_STATE_UP != ast_channel->_state) {
	        if ( NULL == sccp_channel) {	// prevent dereferecing null pointer
	                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send ast_softhangup_nolock\n", ast_channel->name);
                        ast_softhangup_nolock(ast_channel, AST_SOFTHANGUP_DEV);
	        } else {
                        if (	(AST_STATE_DIALING == ast_channel->_state && SCCP_CHANNELSTATE_PROGRESS != sccp_channel->state) || 
                                        SCCP_CHANNELSTATE_OFFHOOK == sccp_channel->state || 
                                        SCCP_CHANNELSTATE_INVALIDNUMBER == sccp_channel->state
                                ) {
                                // AST_STATE_DIALING == ast_channel->_state                -> use ast_hangup when still in dialing state
                                // SCCP_CHANNELSTATE_OFFHOOK == sccp_channel->state        -> use ast_hangup after callforward ss-switch
                                // SCCP_CHANNELSTATE_INVALIDNUMBER == sccp_channel->state  -> use ast_hangup before connection to pbx is established 
				sccp_wrapper_asterisk_forceHangup(ast_channel, PBX_HARD_HANGUP);
                                return TRUE;
                        } else if (
                                        ( (AST_STATE_RING == ast_channel->_state || AST_STATE_RINGING == ast_channel->_state) && SCCP_CHANNELSTATE_DIALING == sccp_channel->state ) || 
                                        SCCP_CHANNELSTATE_BUSY == sccp_channel->state || 
                                        SCCP_CHANNELSTATE_CONGESTION == sccp_channel->state
                                ) {
                                /* softhangup when ast_channel structure is still needed afterwards */
				sccp_wrapper_asterisk_forceHangup(ast_channel, PBX_SOFT_HANGUP);
                                return TRUE;
                        }
		}
	}
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: send ast_queue_hangup\n", ast_channel->name);
	sccp_wrapper_asterisk_forceHangup(ast_channel, PBX_QUEUED_HANGUP);
	return TRUE;
}


int sccp_asterisk_pbx_fktChannelWrite(struct ast_channel *ast, const char *funcname, char *args, const char *value)
{
	sccp_channel_t *c;
	boolean_t res = TRUE;

	c = get_sccp_channel_from_ast_channel(ast);
	if (!c) {
		ast_log(LOG_ERROR, "This function requires a valid SCCP channel\n");
		return -1;
	}

	if (!strcasecmp(args, "MaxCallBR")) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set max call bitrate to %s\n", DEV_ID_LOG(sccp_channel_getDevice(c)), value);

             if(sscanf(value, "%ud", &c->maxBitRate)){
                     pbx_builtin_setvar_helper(ast, "_MaxCallBR", value);
                     res = TRUE;
             }else{
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
		return -1;
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
 * \param chan The channel structure that will get music on hold
 * \param mclass The class to use if the musicclass is not currently set on
 *               the channel structure.
 * \param interpclass The class to use if the musicclass is not currently set on
 *                    the channel structure or in the mclass argument.
 *
 * \retval Zero on success
 * \retval non-zero on failure
 */
int sccp_asterisk_moh_start(const PBX_CHANNEL_TYPE * pbx_channel, const char *mclass, const char* interpclass)
{
	return ast_moh_start((PBX_CHANNEL_TYPE *)pbx_channel, mclass, interpclass);
}

void sccp_asterisk_moh_stop(const PBX_CHANNEL_TYPE * pbx_channel) 
{
	ast_moh_stop((PBX_CHANNEL_TYPE *)pbx_channel);
}

