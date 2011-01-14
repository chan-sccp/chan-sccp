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

//SCCP_FILE_VERSION(__FILE__, "$Revision: 2235 $")
/************************************************************************************************************ CALLERID **/

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char * get_pbx_callerid_name(struct ast_channel *ast_chan)
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
char * get_pbx_callerid_number(struct ast_channel *ast_chan)
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

#define strcp_callerid(_sccp,_ast)                                     \
        sccp_copy_string(_sccp,_ast,sizeof(_sccp)-1);       	\

        if (callInfo->callingParty_valid) {
		strcp_callerid(callInfo->callingPartyName, ast_chan->cid.cid_num);
		strcp_callerid(callInfo->callingPartyNumber, ast_chan->cid.cid_name);
	}

	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCallingPartyNumber, ast_chan->cid.cid_rdnis);		//RDNIS ? ANI
	}
	
	if (callInfo->lastRedirecting_valid) {
		strcp_callerid(callInfo->lastRedirectingPartyNumber, ast_chan->cid.cid_ani);		//RDNIS ? ANI
	}
	
	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCalledPartyNumber, ast_chan->cid.cid_dnid);		//DNID
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
#define strcp_callerid(_ast,_sccp)                              \
	 	sccp_copy_string(_ast,_sccp,sizeof(_ast)-1);    \

#if 0
	if (!strcmp(ast_chan->cid.cid_num, callInfo->callingPartyNumber) && !strcmp(ast_chan->cid.cid_name, callInfo->callingPartyName)) {
		strcp_callerid(ast_chan->cid.cid_num, callInfo->callingPartyNumber);
		strcp_callerid(ast_chan->cid.cid_name, callInfo->callingPartyName);
		callInfo->callingParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_ani, callInfo->calledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->calledPartyNumber);
		callInfo->calledParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber);		//RDNIS
		callInfo->originalCallingParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber);		//ANI
		callInfo->lastRedirectingParty_valid=1;
	}

	if (!strcmp(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber);		//DNID
		callInfo->originalCalledParty_valid;
	}

	ast_chan->cid.cid_pres = (int)callInfo->presentation;
#endif
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
	ast_channel_lock(target);
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
struct ast_ha *pbx_append_ha(OLDCONST char *sense, const char *stuff, struct ast_ha *path, int *error) {
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
struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar) {
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
struct ast_config *pbx_config_load(const char *filename, const char *who_asked, struct ast_flags flags) {
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
char* pbx_getformatname(format_t format) {
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
char * pbx_getformatname_multiple(char *buf, size_t size, format_t format) {
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
struct ast_variable *pbx_variable_new(struct ast_variable *v) {
#    if ASTERISK_VERSION_NUM >= 10600
		return ast_variable_new(v->name, v->value, v->file);
#    else
		return ast_variable_new(v->name, v->value);
#    endif
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
        { "CS0", 0x00 },
        { "CS1", 0x08 },
        { "CS2", 0x10 },
        { "CS3", 0x18 },
        { "CS4", 0x20 },
        { "CS5", 0x28 },
        { "CS6", 0x30 },
        { "CS7", 0x38 },
        { "AF11", 0x0A },
        { "AF12", 0x0C },
        { "AF13", 0x0E },
        { "AF21", 0x12 },
        { "AF22", 0x14 },
        { "AF23", 0x16 },
        { "AF31", 0x1A },
        { "AF32", 0x1C },
        { "AF33", 0x1E },
        { "AF41", 0x22 }, 
        { "AF42", 0x24 },
        { "AF43", 0x26 },
        { "EF", 0x2E },  
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
int pbx_str2tos(const char *value, unsigned int *tos) {
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
int pbx_str2cos(const char *value, unsigned int *cos) {
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
int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar) {
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
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag) {
#    if ASTERISK_VERSION_NUM < 10600
	astman_send_ack(s, m, msg);
#    else
	astman_send_listack(s, m, msg, listflag);
#    endif
}
/***************************************************************************************************************** RTP **/
/*!
 * \brief pbx rtp get peer
 * \note replacement for ast_rtp_get_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 * \return int
 */
int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *them) {
	return ast_rtp_get_peer(rtp, them);
}

/*!
 * \brief pbx rtp set peer
 * \note replacement for ast_rtp_set_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 */
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *them) {
	ast_rtp_set_peer(rtp, them);
}
