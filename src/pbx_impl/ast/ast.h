/*!
 * \file        ast.h
 * \brief       SCCP PBX Asterisk Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
//#define REF_DEBUG 1

#include <asterisk.h>
#include <asterisk/pbx.h>			// AST_EXTENSION_NOT_INUSE in mapping below
#ifdef HAVE_PBX_RTP_ENGINE_H			// sccp_callinfo, sccp_rtp
#  include <asterisk/rtp_engine.h>
#else
#  include <asterisk/rtp.h>
#endif
#  include <asterisk/manager.h>
#include <asterisk/causes.h>
#include <asterisk/buildopts.h>
#include "define.h"
#include "sccp_protocol.h"

#ifdef ASTERISK_CONF_1_6
#include "ast106.h"
#endif
#ifdef ASTERISK_CONF_1_8
#include "ast108.h"
#endif
#ifdef ASTERISK_CONF_1_10
#include "ast110.h"
#endif
#ifdef ASTERISK_CONF_1_11
#include "ast111.h"
#endif
#ifdef ASTERISK_CONF_1_12
#include "ast112.h"
#endif
#ifdef ASTERISK_CONF_1_13
#include "ast113.h"
#endif
#ifdef ASTERISK_CONF_1_14
#include "ast114.h"
#endif
#ifdef ASTERISK_CONF_1_15
#include "ast115.h"
#endif

/* only trunk version has AST_CAUSE_ANSWERED_ELSEWHERE */
#ifndef AST_CAUSE_ANSWERED_ELSEWHERE
#define AST_CAUSE_ANSWERED_ELSEWHERE 200
#endif

extern struct sccp_pbx_cb sccp_pbx;

#define AST_MODULE "chan_sccp"
#define PBX_HANGUP_CAUSE_UNKNOWN AST_CAUSE_NORMAL_UNSPECIFIED
#define PBX_HANGUP_CAUSE_NORMAL_CALL_CLEARING AST_CAUSE_NORMAL_CLEARING
#define PBX_HANGUP_CAUSE_CHANNEL_UNAVAILABLE AST_CAUSE_REQUESTED_CHAN_UNAVAIL
#define PBX_HANGUP_CAUSE_FACILITY_REJECTED AST_CAUSE_FACILITY_REJECTED
#define PBX_HANGUP_CAUSE_CALL_REJECTED AST_CAUSE_CALL_REJECTED

/*!
 * \brief PBX Hangup Types handled by sccp_wrapper_asterisk_forceHangup
 */
typedef enum {
	PBX_QUEUED_HANGUP = 0,
	PBX_SOFT_HANGUP = 1,
	PBX_HARD_HANGUP = 2,
} pbx_hangup_type_t;

SCCP_INLINE const char * SCCP_CALL pbxsccp_devicestate2str(uint32_t value);
#if UNUSEDCODE // 2015-11-01
SCCP_INLINE const char * SCCP_CALL extensionstatus2str(uint32_t value);
#endif

int set_pbx_callerid(PBX_CHANNEL_TYPE * ast_chan, sccp_callinfo_t * callInfo);
#if UNUSEDCODE // 2015-11-01
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target);
#endif
PBX_CHANNEL_TYPE *pbx_channel_search_locked(int (*is_match) (PBX_CHANNEL_TYPE *, void *), void *data);
#if UNUSEDCODE // 2015-11-01
struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error);
#endif
struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar);
struct ast_config *pbx_config_load(const char *filename, const char *who_asked, struct ast_flags flags);
const char *pbx_inet_ntoa(struct in_addr ia);
int pbx_str2cos(const char *value, uint8_t *cos);
int pbx_str2tos(const char *value, uint8_t *tos);
#if UNUSEDCODE // 2015-11-01
int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar);
#endif
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag);
int pbx_moh_start(PBX_CHANNEL_TYPE * chan, const char *mclass, const char *interpclass);
PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data);
const char *pbx_inet_ntoa(struct in_addr ia);

#define ast_format_type int
#define pbx_format_type int
skinny_codec_t __CONST__ pbx_codec2skinny_codec(ast_format_type fmt);
//ast_format_type skinny_codec2pbx_codec(skinny_codec_t codec);
uint64_t __CONST__ skinny_codec2pbx_codec(skinny_codec_t codec);
//int skinny_codecs2pbx_codecs(const skinny_codec_t * const codecs);
uint64_t __PURE__ skinny_codecs2pbx_codecs(const skinny_codec_t * const codecs);

// support for old uin32_t format (only temporarily
#define pbx_format2skinny_format (uint32_t)pbx_codec2skinny_codec
#define skinny_format2pbx_format(_x) skinny_codec2pbx_codec((skinny_codec_t)_x)

/* 
 * sccp_free_ptr should be used when a function pointer for free() needs to be 
 * passed as the argument to a function. Otherwise, astmm will cause seg faults.
 */
static void sccp_free_ptr(void *ptr) attribute_unused;
static void sccp_free_ptr(void *ptr)
{
	sccp_free(ptr);
}

#if DEBUG
#define get_sccp_channel_from_pbx_channel(_x) __get_sccp_channel_from_pbx_channel(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
sccp_channel_t *__get_sccp_channel_from_pbx_channel(const PBX_CHANNEL_TYPE * pbx_channel, const char *filename, int lineno, const char *func);
#else
sccp_channel_t *get_sccp_channel_from_pbx_channel(const PBX_CHANNEL_TYPE * pbx_channel);
#endif
int sccp_asterisk_pbx_fktChannelWrite(PBX_CHANNEL_TYPE * ast, const char *funcname, char *args, const char *value);
boolean_t sccp_wrapper_asterisk_requestQueueHangup(sccp_channel_t * c);
boolean_t sccp_wrapper_asterisk_requestHangup(sccp_channel_t * c);

/***** database *****/
boolean_t sccp_asterisk_addToDatabase(const char *family, const char *key, const char *value);
boolean_t sccp_asterisk_getFromDatabase(const char *family, const char *key, char *out, int outlen);
boolean_t sccp_asterisk_removeFromDatabase(const char *family, const char *key);
boolean_t sccp_asterisk_removeTreeFromDatabase(const char *family, const char *key);

/***** end - database *****/

int sccp_asterisk_moh_start(PBX_CHANNEL_TYPE * pbx_channel, const char *mclass, const char *interpclass);
void sccp_asterisk_moh_stop(PBX_CHANNEL_TYPE * pbx_channel);
void sccp_asterisk_connectedline(sccp_channel_t * channel, const void *data, size_t datalen);
void sccp_asterisk_redirectedUpdate(sccp_channel_t * channel, const void *data, size_t datalen);
void sccp_asterisk_sendRedirectedUpdate(const sccp_channel_t * channel, const char *fromNumber, const char *fromName, const char *toNumber, const char *toName, uint8_t reason);
int sccp_wrapper_asterisk_channel_read(PBX_CHANNEL_TYPE * ast, NEWCONST char *funcname, char *preparse, char *buf, size_t buflen);
int sccp_parse_alertinfo(PBX_CHANNEL_TYPE *pbx_channel, skinny_ringtype_t *ringermode);
int sccp_parse_dial_options(char *options, sccp_autoanswer_t *autoanswer_type, uint8_t *autoanswer_cause, skinny_ringtype_t *ringermode);
boolean_t sccp_wrapper_asterisk_featureMonitor(const sccp_channel_t * channel);

#if ASTERISK_VERSION_GROUP > 106
int sccp_wrapper_sendDigits(const sccp_channel_t * channel, const char *digits);
int sccp_wrapper_sendDigit(const sccp_channel_t * channel, const char digit);
#endif

void sccp_wrapper_asterisk_set_callgroup(sccp_channel_t *channel, ast_group_t value);
void sccp_wrapper_asterisk_set_pickupgroup(sccp_channel_t *channel, ast_group_t value);
#if CS_AST_HAS_NAMEDGROUP && ASTERISK_VERSION_GROUP >= 111
void sccp_wrapper_asterisk_set_named_callgroups(sccp_channel_t *channel, struct ast_namedgroups *value);
void sccp_wrapper_asterisk_set_named_pickupgroups(sccp_channel_t *channel, struct ast_namedgroups *value);
#endif

enum ast_pbx_result pbx_pbx_start(struct ast_channel *pbx_channel);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
