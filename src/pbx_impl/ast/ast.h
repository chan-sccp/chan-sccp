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

/* only trunk version has AST_CAUSE_ANSWERED_ELSEWHERE */
#ifndef AST_CAUSE_ANSWERED_ELSEWHERE
#define AST_CAUSE_ANSWERED_ELSEWHERE 200
#endif

extern struct sccp_pbx_cb sccp_pbx;

#define AST_MODULE "chan_sccp"

/*!
 * \brief Skinny Codec Mapping
 */
static const struct pbx2skinny_codec_map {
	uint64_t pbx_codec;
	skinny_codec_t skinny_codec;
} pbx2skinny_codec_maps[] = {
	/* *INDENT-OFF* */
	{0,		 	SKINNY_CODEC_NONE},
	{AST_FORMAT_ALAW,	SKINNY_CODEC_G711_ALAW_64K},
	{AST_FORMAT_ALAW,	SKINNY_CODEC_G711_ALAW_56K},
	{AST_FORMAT_ULAW,	SKINNY_CODEC_G711_ULAW_64K},
	{AST_FORMAT_ULAW,	SKINNY_CODEC_G711_ULAW_56K},
	{AST_FORMAT_GSM,	SKINNY_CODEC_GSM},
	{AST_FORMAT_H261,	SKINNY_CODEC_H261},
	{AST_FORMAT_H263,	SKINNY_CODEC_H263},
	{AST_FORMAT_T140,	SKINNY_CODEC_T120},
#    if ASTERISK_VERSION_GROUP >= 113
	{AST_FORMAT_G723,	SKINNY_CODEC_G723_1},
	{AST_FORMAT_SLIN16,	SKINNY_CODEC_WIDEBAND_256K},
	{AST_FORMAT_G729,	SKINNY_CODEC_G729},
	{AST_FORMAT_G729,	SKINNY_CODEC_G729_A},
	{AST_FORMAT_H263P,	SKINNY_CODEC_H263P},
#    else
	{AST_FORMAT_G723_1,	SKINNY_CODEC_G723_1},
	{AST_FORMAT_SLINEAR16,	SKINNY_CODEC_WIDEBAND_256K},
	{AST_FORMAT_G729A,	SKINNY_CODEC_G729},
	{AST_FORMAT_G729A,	SKINNY_CODEC_G729_A},
	{AST_FORMAT_H263_PLUS,	SKINNY_CODEC_H263P},
#    endif
#    if ASTERISK_VERSION_NUMBER >= 10400
	{AST_FORMAT_G726_AAL2,	SKINNY_CODEC_G726_32K},
	{AST_FORMAT_G726,	SKINNY_CODEC_G726_32K},
	{AST_FORMAT_ILBC,	SKINNY_CODEC_G729_B_LOW},
	{AST_FORMAT_G722,	SKINNY_CODEC_G722_64K},
	{AST_FORMAT_G722,	SKINNY_CODEC_G722_56K},
	{AST_FORMAT_G722,	SKINNY_CODEC_G722_48K},
	{AST_FORMAT_H264,	SKINNY_CODEC_H264},
#    endif
#    ifdef AST_FORMAT_SIREN7
	{AST_FORMAT_SIREN7, 	SKINNY_CODEC_G722_1_24K},			// should this not be SKINNY_CODEC_G722_1_32K
#    endif
#    ifdef AST_FORMAT_SIREN14
	{AST_FORMAT_SIREN14, 	SKINNY_CODEC_G722_1_32K},			// should this not be SKINNY_CODEC_G722_1_48K
#    endif
#    ifdef AST_FORMAT_OPUS
	{AST_FORMAT_OPUS, 	SKINNY_CODEC_OPUS},
#    endif
	/* *INDENT-ON* */
};

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

/*!
 * \brief AST Device State Structure
 */
static const struct sccp_pbx_devicestate {
	const char *const text;
#ifdef ENUM_AST_DEVICE
	enum ast_device_state devicestate;
#else
	uint8_t devicestate;
#endif
} sccp_pbx_devicestates[] = {
	/* *INDENT-OFF* */
	{"Device is valid but channel doesn't know state",	AST_DEVICE_UNKNOWN},
	{"Device is not in use",				AST_DEVICE_NOT_INUSE},
	{"Device is in use",					AST_DEVICE_INUSE},
	{"Device is busy",					AST_DEVICE_BUSY},
	{"Device is invalid",					AST_DEVICE_INVALID},
	{"Device is unavailable",				AST_DEVICE_UNAVAILABLE},
	{"Device is ringing",					AST_DEVICE_RINGING},
	{"Device is ringing and in use",			AST_DEVICE_RINGINUSE},
	{"Device is on hold",					AST_DEVICE_ONHOLD},
#    ifdef AST_DEVICE_TOTAL
	{"Total num of device states, used for testing",	AST_DEVICE_TOTAL},
#    endif
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP Extension State Structure
 */
static const struct sccp_extension_state {
	const char *const text;
	int extension_state;
} sccp_extension_states[] = {
	/* *INDENT-OFF* */
	{"Extension Removed",					AST_EXTENSION_REMOVED},
	{"Extension Hint Removed",				AST_EXTENSION_DEACTIVATED},
	{"No device INUSE or BUSY",				AST_EXTENSION_NOT_INUSE},
	{"One or More devices In Use",				AST_EXTENSION_INUSE},
	{"All devices Busy",					AST_EXTENSION_BUSY},
	{"All devices Unavailable/Unregistered",		AST_EXTENSION_UNAVAILABLE},
#    ifdef CS_AST_HAS_EXTENSION_RINGING
	{"All Devices Ringing",					AST_EXTENSION_RINGING},
	{"All Devices Ringing and In Use",			AST_EXTENSION_INUSE | AST_EXTENSION_RINGING},
#    endif
#    ifdef CS_AST_HAS_EXTENSION_ONHOLD
	{"All Devices On Hold",					AST_EXTENSION_ONHOLD},
#    endif
	/* *INDENT-ON* */
};

#if 0 /* unused */
/*!
 * \brief Ast Cause - Skinny DISP Mapping
 */
static const struct pbx_skinny_cause {
	const char *skinny_disp;
	const char *message;
	int pbx_cause;
} pbx_skinny_causes[] = {
	/* *INDENT-OFF* */
        { AST_CAUSE_UNALLOCATED,		, "Unallocated (unassigned) number", 		SKINNY_DISP_NUMBER_NOT_CONFIGURED},
        { AST_CAUSE_NO_ROUTE_TRANSIT_NET,	, "No route to specified transmit network", 	SKINNY_DISP_UNKNOWN_NUMBER},
        { AST_CAUSE_NO_ROUTE_DESTINATION,	, "No route to destination", 			SKINNY_DISP_UNKNOWN_NUMBER},
        { AST_CAUSE_CHANNEL_UNACCEPTABLE,	, "Channel unacceptable", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
#if ASTERISK_VERSION_GROUP >= 108
	{ AST_CAUSE_MISDIALLED_TRUNK_PREFIX,   	, "Misdialled Trunk Prefix", 			SKINNY_DISP_UNKNOWN_NUMBER},
#endif
        { AST_CAUSE_CALL_AWARDED_DELIVERED,	, "Call awarded and being delivered in an established channel", SKINNY_DISP_CONNECTED},
        { AST_CAUSE_NORMAL_CLEARING,		, "Normal Clearing",				SKINNY_DISP_ON_HOOK},
	{ AST_CAUSE_PRE_EMPTED,			, "Pre Empted", 				SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER},
        { AST_CAUSE_USER_BUSY, 			, "User busy", 					SKINNY_DISP_BUSY},
#if ASTERISK_VERSION_GROUP >= 108
	{ AST_CAUSE_NUMBER_PORTED_NOT_HERE,	, "Number not configured", 			SKINNY_DISP_NUMBER_NOT_CONFIGURED},
#endif
	{ AST_CAUSE_SUBSCRIBER_ABSENT,		, "Subscriber Absent, Try Again", 		SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_ANSWERED_ELSEWHERE,		, "Answered Elsewhere", 			SKINNY_DISP_CONNECTED},
	{ AST_CAUSE_NO_USER_RESPONSE,		, "No user responding", 			SKINNY_DISP_EMPTY},
        { AST_CAUSE_NO_ANSWER,			, "User alerting, no answer", 			SKINNY_DISP_EMPTY},
        { AST_CAUSE_CALL_REJECTED,		, "Call Rejected", 				SKINNY_DISP_BUSY},
	{ AST_CAUSE_NUMBER_CHANGED,		, "Number changed", 				SKINNY_DISP_NUMBER_NOT_CONFIGURED},
        { AST_CAUSE_DESTINATION_OUT_OF_ORDER,	, "Destination out of order", 			SKINNY_DISP_TEMP_FAIL},
        { AST_CAUSE_INVALID_NUMBER_FORMAT, 	, "Invalid number format", 			SKINNY_DISP_UNKNOWN_NUMBER },
        { AST_CAUSE_FACILITY_REJECTED,		, "Facility rejected", 				SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_RESPONSE_TO_STATUS_ENQUIRY,	, "Response to STATus ENQuiry", 		SKINNY_DISP_TEMP_FAIL},
        { AST_CAUSE_NORMAL_UNSPECIFIED,		, "Normal, unspecified", 			SKINNY_DISP_TEMP_FAIL},
  	{ AST_CAUSE_NORMAL_CIRCUIT_CONGESTION,	, "Circuit/channel congestion", 		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER},
        { AST_CAUSE_NETWORK_OUT_OF_ORDER, 	, "Network out of order", 			SKINNY_DISP_NETWORK_CONGESTION_REROUTING},
  	{ AST_CAUSE_NORMAL_TEMPORARY_FAILURE, 	, "Temporary failure", 				SKINNY_DISP_TEMP_FAIL},
        { AST_CAUSE_SWITCH_CONGESTION,		, "Switching equipment congestion",	 	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER}, 
	{ AST_CAUSE_ACCESS_INFO_DISCARDED,	, "Access information discarded", 		SKINNY_DISP_SECURITY_ERROR},
        { AST_CAUSE_REQUESTED_CHAN_UNAVAIL,	, "Requested channel not available", 		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER},
	{ AST_CAUSE_PRE_EMPTED,			, "Pre-empted", 				SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER},
        { AST_CAUSE_FACILITY_NOT_SUBSCRIBED,	, "Facility not subscribed", 			SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_OUTGOING_CALL_BARRED,	, "Outgoing call barred", 			SKINNY_DISP_SECURITY_ERROR},
        { AST_CAUSE_INCOMING_CALL_BARRED,	, "Incoming call barred", 			SKINNY_DISP_SECURITY_ERROR},
	{ AST_CAUSE_BEARERCAPABILITY_NOTAUTH,	, "Bearer capability not authorized", 		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_BEARERCAPABILITY_NOTAVAIL,	, "Bearer capability not available", 		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_BEARERCAPABILITY_NOTIMPL,	, "Bearer capability not implemented", 		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_CHAN_NOT_IMPLEMENTED,	, "Channel not implemented", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
	{ AST_CAUSE_FACILITY_NOT_IMPLEMENTED,	, "Facility not implemented", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_INVALID_CALL_REFERENCE,	, "Invalid call reference value", 		SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_INCOMPATIBLE_DESTINATION,	, "Incompatible destination", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_INVALID_MSG_UNSPECIFIED,	, "Invalid message unspecified", 		SKINNY_DISP_ERROR_UNKNOWN},
        { AST_CAUSE_MANDATORY_IE_MISSING,	, "Mandatory information element is missing", 	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL},
        { AST_CAUSE_MESSAGE_TYPE_NONEXIST,	, "Message type nonexist.", 			SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL},
  	{ AST_CAUSE_WRONG_MESSAGE,		, "Wrong message", 				SKINNY_DISP_ERROR_MISMATCH},
        { AST_CAUSE_IE_NONEXIST,		, "Info. element nonexist or not implemented", 	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
	{ AST_CAUSE_INVALID_IE_CONTENTS,	, "Invalid information element contents", 	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_WRONG_CALL_STATE,		, "Message not compatible with call state", 	SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_RECOVERY_ON_TIMER_EXPIRE,	, "Recover on timer expiry", 			SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT},
        { AST_CAUSE_MANDATORY_IE_LENGTH_ERROR,	, "Mandatory IE length error", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
	{ AST_CAUSE_PROTOCOL_ERROR,		, "Protocol error, unspecified", 		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
        { AST_CAUSE_INTERWORKING,		, "Interworking, unspecified", 			SKINNY_DISP_NETWORK_CONGESTION_REROUTING},
        
        // aliases for backward compatibility reasons
	{ AST_CAUSE_BUSY,			, "User busy", 					SKINNY_DISP_BUSY},
	{ AST_CAUSE_FAILURE,			, "Network out of order", 			SKINNY_DISP_NETWORK_CONGESTION_REROUTING},
	{ AST_CAUSE_NORMAL,			, "Normal Clearing", 				SKINNY_DISP_ON_HOOK},
	{ AST_CAUSE_NOANSWER,			, "User alerting, no answer", 			SKINNY_DISP_EMPTY},
	{ AST_CAUSE_CONGESTION,    		, "Circuit/channel congestion", 		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER},
	{ AST_CAUSE_UNREGISTERED,  		, "Subscriber Absent, Try Again", 		SKINNY_DISP_TEMP_FAIL},
	{ AST_CAUSE_NOTDEFINED,    		, "Not Defined", 				SKINNY_DISP_EMPTY},
	{ AST_CAUSE_NOSUCHDRIVER,		, "Channel not implemented", 			SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE},
	/* *INDENT-ON* */
};
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

skinny_codec_t pbx_codec2skinny_codec(ast_format_type fmt);
ast_format_type skinny_codec2pbx_codec(skinny_codec_t codec);

// support for old uin32_t format (only temporarily
#define pbx_format2skinny_format (uint32_t)pbx_codec2skinny_codec
#define skinny_format2pbx_format(_x) skinny_codec2pbx_codec((skinny_codec_t)_x)

int skinny_codecs2pbx_codecs(const skinny_codec_t * const codecs);

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
