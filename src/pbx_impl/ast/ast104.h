
/*!
 * \file 	ast104.h
 * \brief 	SCCP PBX Asterisk Header
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#ifndef SCCP_AST_MAJOR_H_
#    define SCCP_AST_MAJOR_H_

#    include "../../config.h"
#    include "../../common.h"

#    define sccp_sched_context_destroy sched_context_destroy

typedef int ast_format_t;

#    define NEWCONST								// old functions used without const
#    define OLDCONST const							// new function used with const

struct ast_hashtab {
};

/*! Options for ast_config_load()
 */
enum {
	/*! Load the configuration, including comments */
	CONFIG_FLAG_WITHCOMMENTS = (1 << 0),
	/*! On a reload, give us a -1 if the file hasn't changed. */
	CONFIG_FLAG_FILEUNCHANGED = (1 << 1),
	/*! Don't attempt to cache mtime on this config file. */
	CONFIG_FLAG_NOCACHE = (1 << 2),
};

int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, int codecs, int nat_active);
enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);
enum ast_rtp_get_result sccp_wrapper_asterisk_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);
boolean_t sccp_wrapper_asterisk_allocPBXChannel(const sccp_channel_t * channel, void **pbx_channel);
int pbx_rtp_get_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);
void pbx_rtp_get_us(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);
void pbx_rtp_set_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);
int sccp_wrapper_asterisk_sendDigit(const sccp_channel_t * channel, const char digit);
int sccp_wrapper_asterisk_sendDigits(const sccp_channel_t * channel, const char digits[]);
void sccp_wrapper_asterisk_setCalleridPresence(const sccp_channel_t * channel, uint8_t precence);
int sccp_wrapper_asterisk_hangup(const sccp_channel_t * channel);
uint8_t sccp_wrapper_asterisk_get_payloadType(const struct sccp_rtp *rtp, skinny_codec_t codec);

//void *sccp_do_monitor(void *data);
//int sccp_restart_monitor(void);
//int sccp_wrapper_asterisk16_hangup(PBX_CHANNEL_TYPE * ast_channel);
//boolean_t sccp_wrapper_asterisk16_allocPBXChannel(const sccp_channel_t * channel, PBX_CHANNEL_TYPE ** pbx_channel);

// CLI_ENTRY
//   param1=registration_name
//   param2=function to execute when called
//   param3=cli string to be types as array of strings
//   param4=registration description
//   param5=usage string
#    define CLI_ENTRY_COMPLETE(_FUNCTION_NAME,_CALLED_FUNCTION,_DESCR,_USAGE,_COMPLETER)	\
	static struct ast_cli_entry _FUNCTION_NAME = {						\
	  .cmda = { CLI_COMMAND },								\
	  .handler = _CALLED_FUNCTION,								\
	  .summary = _DESCR,									\
	  .usage = _USAGE,									\
	  .generator = _COMPLETER								\
	};
#    define CLI_ENTRY(_FUNCTION_NAME,_CALLED_FUNCTION,_DESCR,_USAGE)			\
	static struct ast_cli_entry _FUNCTION_NAME = {						\
	  .cmda = { CLI_COMMAND },								\
	  .handler = _CALLED_FUNCTION,								\
	  .summary = _DESCR,									\
	  .usage = _USAGE									\
	};
// END CLI_ENTRY

#endif										/* SCCP_ASTERISK19_PBX_H_ */
