
/*!
 * \file 	ast.h
 * \brief 	SCCP PBX Asterisk Header
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
#ifndef __SCCP_PBX_WRAPPER_H
#    define __SCCP_PBX_WRAPPER_H
#    include "../../common.h"

#    if ASTERISK_VERSION_NUMBER >= 10400
#        include <asterisk.h>
#        include <asterisk/abstract_jb.h>
#    endif
#    include <asterisk/compiler.h>
#    include <asterisk/utils.h>
#    include <asterisk/threadstorage.h>
#    include <asterisk/strings.h>
#    include <asterisk/pbx.h>
#    include <asterisk/acl.h>
#    include <asterisk/module.h>
#    include <asterisk/options.h>
#    include <asterisk/logger.h>
#    include <asterisk/config.h>
#    include <asterisk/sched.h>
#    include <asterisk/causes.h>
#    include <asterisk/frame.h>
#    include <asterisk/lock.h>
#    include <asterisk/channel.h>
#    include <asterisk/app.h>
#    include <asterisk/callerid.h>
#    include <asterisk/musiconhold.h>
#    include <asterisk/astdb.h>
#    ifdef CS_SCCP_PICKUP
#        include <asterisk/features.h>
#    endif
#    ifdef HAVE_PBX_EVENT
#        include <asterisk/event.h>
#    endif
#    ifndef CS_AST_HAS_TECH_PVT
#        include <asterisk/channel_pvt.h>
#    endif
#    ifdef HAVE_PBX_DEVICESTATE_H
#        include <asterisk/devicestate.h>
#    endif
#    ifdef AST_EVENT_IE_CIDNAME
#        include <asterisk/event.h>
#        include <asterisk/event_defs.h>
#    endif
#    ifdef CS_AST_HAS_AST_STRING_FIELD
#        include <asterisk/stringfields.h>
#    endif
#    ifdef CS_MANAGER_EVENTS
#        include <asterisk/manager.h>
#    endif
#    ifdef CS_AST_HAS_ENDIAN
#        include <asterisk/endian.h>
#    endif
#    include <asterisk/translate.h>
#    ifdef HAVE_PBX_RTP_ENGINE_H
#        include <asterisk/rtp_engine.h>
#    else
#        include <asterisk/rtp.h>
#    endif
#    ifdef CS_DEVSTATE_FEATURE
#        include <asterisk/event_defs.h>
#    endif
#    include <asterisk/indications.h>

#    include "define.h"
#    ifdef ASTERISK_CONF_1_2
#        include "ast102.h"
//#        include "ast10200.h"
#        define PBXVER 10200
#    endif
#    ifdef ASTERISK_CONF_1_4
#        include "ast104.h"
//#        include "ast10400.h"
#        define PBXVER 10400
#    endif
#    ifdef ASTERISK_CONF_1_6
#        include "ast106.h"

/*#        if ASTERISK_VERSION_NUMBER == 10600
#            include "ast10600.h"
#            define PBXVER 10600
#        elif ASTERISK_VERSION_NUMBER == 10601
#            include "ast10601.h"
#            define PBXVER 10601
#        elif ASTERISK_VERSION_NUMBER == 10602
#            include "ast10602.h"
#            define PBXVER 10602
#        endif
*/
#    endif
#    ifdef ASTERISK_CONF_1_8
#        include "ast108.h"
#    endif
#    ifdef ASTERISK_CONF_1_10
#        	ifdef ASTERISK_REPOS_LOCATION_TRUNK
#        include "astTrunk.h"
#        	else
#        include "ast110.h"
#        	endif

/*#        if ASTERISK_VERSION_NUMBER == 11000
#            include "ast11000.h"
#            define PBXVER 11000
#        endif*/
#    endif

/* only trunk version has AST_CAUSE_ANSWERED_ELSEWHERE */
#    ifndef AST_CAUSE_ANSWERED_ELSEWHERE
#        define AST_CAUSE_ANSWERED_ELSEWHERE 200
#    endif

extern struct sccp_pbx_cb sccp_pbx;

#    define PBX(x) sccp_pbx.x
#    define AST_MODULE "chan_sccp"

/*!
 * \brief Skinny Codec Mapping
 */
static const struct skinny2pbx_codec_map {
	skinny_codec_t skinny_codec;
	uint64_t pbx_codec;
} skinny2pbx_codec_maps[] = {
	/* *INDENT-OFF* */
	{SKINNY_CODEC_NONE, 		0},
	{SKINNY_CODEC_G711_ALAW_64K, 	AST_FORMAT_ALAW},
	{SKINNY_CODEC_G711_ALAW_56K, 	AST_FORMAT_ALAW},
	{SKINNY_CODEC_G711_ULAW_64K, 	AST_FORMAT_ULAW},
	{SKINNY_CODEC_G711_ULAW_56K, 	AST_FORMAT_ULAW},
	{SKINNY_CODEC_G723_1, 		AST_FORMAT_G723_1},
	{SKINNY_CODEC_G729, 		AST_FORMAT_G729A},
	{SKINNY_CODEC_G729_A, 		AST_FORMAT_G729A},
	{SKINNY_CODEC_GSM, 		AST_FORMAT_GSM},
	{SKINNY_CODEC_H261, 		AST_FORMAT_H261},
	{SKINNY_CODEC_H263, 		AST_FORMAT_H263},
	{SKINNY_CODEC_H263P,	 	AST_FORMAT_H263_PLUS},
	{SKINNY_CODEC_T120, 		AST_FORMAT_T140},
	{SKINNY_CODEC_WIDEBAND_256K,	AST_FORMAT_SLINEAR16},
#    if ASTERISK_VERSION_NUMBER >= 10400
	{SKINNY_CODEC_G726_32K, 	AST_FORMAT_G726_AAL2},
	{SKINNY_CODEC_G726_32K, 	AST_FORMAT_G726},
	{SKINNY_CODEC_G729_B_LOW, 	AST_FORMAT_ILBC},
	{SKINNY_CODEC_G722_64K, 	AST_FORMAT_G722},
	{SKINNY_CODEC_G722_56K, 	AST_FORMAT_G722},
	{SKINNY_CODEC_G722_48K, 	AST_FORMAT_G722},
	{SKINNY_CODEC_H264, 		AST_FORMAT_H264},
#    endif
#    ifdef AST_FORMAT_SIREN7
	{SKINNY_CODEC_G722_1_24K, 	AST_FORMAT_SIREN7},			// should this not be SKINNY_CODEC_G722_1_32K
	{SKINNY_CODEC_G722_1_32K, 	AST_FORMAT_SIREN14},			// should this not be SKINNY_CODEC_G722_1_48K
#    endif
	/* *INDENT-ON* */
};

#    define PBX_HANGUP_CAUSE_UNKNOWN AST_CAUSE_NORMAL_UNSPECIFIED
#    define PBX_HANGUP_CAUSE_NORMAL_CALL_CLEARING AST_CAUSE_NORMAL_CLEARING
#    define PBX_HANGUP_CAUSE_CHANNEL_UNAVAILABLE AST_CAUSE_REQUESTED_CHAN_UNAVAIL
#    define PBX_HANGUP_CAUSE_FACILITY_REJECTED AST_CAUSE_FACILITY_REJECTED
#    define PBX_HANGUP_CAUSE_CALL_REJECTED AST_CAUSE_CALL_REJECTED

/*!
 * \brief AST Device State Structure
 */
static const struct pbx_devicestate {
#    ifdef ENUM_AST_DEVICE
	enum ast_device_state devicestate;
#    else
	uint8_t devicestate;
#    endif
	const char *const text;
} pbx_devicestates[] = {
	/* *INDENT-OFF* */
	{AST_DEVICE_UNKNOWN, "Device is valid but channel doesn't know state"},
	{AST_DEVICE_NOT_INUSE, "Device is not in use"},
	{AST_DEVICE_INUSE, "Device is in use"},
	{AST_DEVICE_BUSY, "Device is busy"},
	{AST_DEVICE_INVALID, "Device is invalid"},
	{AST_DEVICE_UNAVAILABLE, "Device is unavailable"},
	{AST_DEVICE_RINGING, "Device is ringing"},
	{AST_DEVICE_RINGINUSE, "Device is ringing and in use"},
	{AST_DEVICE_ONHOLD, "Device is on hold"},
#    ifdef AST_DEVICE_TOTAL
	{AST_DEVICE_TOTAL, "Total num of device states, used for testing"}
#    endif
	/* *INDENT-ON* */
};

/*!
 * \brief PBX Hangup Types handled by sccp_wrapper_asterisk_forceHangup
 */
typedef enum {
        PBX_QUEUED_HANGUP = 0,
        PBX_SOFT_HANGUP = 1,
        PBX_HARD_HANGUP = 2,
} pbx_hangup_type_t;

/*!
 * \brief SCCP Extension State Structure
 */
static const struct sccp_extension_state {
	uint16_t extension_state;
	const char *const text;
} sccp_extension_states[] = {
	/* *INDENT-OFF* */
	{AST_EXTENSION_REMOVED, "Extension Removed"},
	{AST_EXTENSION_DEACTIVATED, "Extension Hint Removed"},
	{AST_EXTENSION_NOT_INUSE, "No device INUSE or BUSY"},
	{AST_EXTENSION_INUSE, "One or More devices In Use"},
	{AST_EXTENSION_BUSY, "All devices Busy"},
	{AST_EXTENSION_UNAVAILABLE, "All devices Unavailable/Unregistered"},
#    ifdef CS_AST_HAS_EXTENSION_RINGING
	{AST_EXTENSION_RINGING, "All Devices Ringing"},
	{AST_EXTENSION_INUSE | AST_EXTENSION_RINGING, "All Devices Ringing and In Use"},
#    endif
#    ifdef CS_AST_HAS_EXTENSION_ONHOLD
	{AST_EXTENSION_ONHOLD, "All Devices On Hold"},
#    endif
	/* *INDENT-ON* */
};

/*!
 * \brief Ast Cause - Skinny DISP Mapping
 */
static const struct ast_skinny_cause {
	int pbx_cause;
	const char *skinny_disp;
	const char *message;
} ast_skinny_causes[] = {
	/* *INDENT-OFF* */

        { AST_CAUSE_UNALLOCATED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED		, "Unallocated (unassigned) number" },
        { AST_CAUSE_NO_ROUTE_TRANSIT_NET,	SKINNY_DISP_UNKNOWN_NUMBER			, "No route to specified transmit network" },
        { AST_CAUSE_NO_ROUTE_DESTINATION,	SKINNY_DISP_UNKNOWN_NUMBER			, "No route to destination" },
        { AST_CAUSE_CHANNEL_UNACCEPTABLE,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Channel unacceptable" },   
	{ AST_CAUSE_MISDIALLED_TRUNK_PREFIX,   	SKINNY_DISP_UNKNOWN_NUMBER			, "Misdialled Trunk Prefix" },
        { AST_CAUSE_CALL_AWARDED_DELIVERED,	SKINNY_DISP_CONNECTED				, "Call awarded and being delivered in an established channel" },
  	{ AST_CAUSE_NORMAL_CLEARING,		SKINNY_DISP_ON_HOOK 				, "Normal Clearing" },
	{ AST_CAUSE_PRE_EMPTED,			SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Pre Empted" },
        { AST_CAUSE_USER_BUSY, 			SKINNY_DISP_BUSY				, "User busy" },
	{ AST_CAUSE_NUMBER_PORTED_NOT_HERE,	SKINNY_DISP_NUMBER_NOT_CONFIGURED		, "Number not configured" },
	{ AST_CAUSE_SUBSCRIBER_ABSENT,		SKINNY_DISP_TEMP_FAIL				, "Subscriber Absent, Try Again" },
	{ AST_CAUSE_ANSWERED_ELSEWHERE,		SKINNY_DISP_CONNECTED				, "Answered Elsewhere" },
	{ AST_CAUSE_NO_USER_RESPONSE,		SKINNY_DISP_EMPTY				, "No user responding" },
        { AST_CAUSE_NO_ANSWER,			SKINNY_DISP_EMPTY				, "User alerting, no answer" },
        { AST_CAUSE_CALL_REJECTED,		SKINNY_DISP_BUSY				, "Call Rejected" },   
	{ AST_CAUSE_NUMBER_CHANGED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED		, "Number changed" },
        { AST_CAUSE_DESTINATION_OUT_OF_ORDER,	SKINNY_DISP_TEMP_FAIL				, "Destination out of order" },
        { AST_CAUSE_INVALID_NUMBER_FORMAT, 	SKINNY_DISP_UNKNOWN_NUMBER 			, "Invalid number format" },
        { AST_CAUSE_FACILITY_REJECTED,		SKINNY_DISP_TEMP_FAIL				, "Facility rejected" },
	{ AST_CAUSE_RESPONSE_TO_STATUS_ENQUIRY,	SKINNY_DISP_TEMP_FAIL				, "Response to STATus ENQuiry" },
        { AST_CAUSE_NORMAL_UNSPECIFIED,		SKINNY_DISP_TEMP_FAIL				, "Normal, unspecified" },
  	{ AST_CAUSE_NORMAL_CIRCUIT_CONGESTION,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Circuit/channel congestion" },
        { AST_CAUSE_NETWORK_OUT_OF_ORDER, 	SKINNY_DISP_NETWORK_CONGESTION_REROUTING	, "Network out of order" },
  	{ AST_CAUSE_NORMAL_TEMPORARY_FAILURE, 	SKINNY_DISP_TEMP_FAIL				, "Temporary failure" },
        { AST_CAUSE_SWITCH_CONGESTION,		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Switching equipment congestion" }, 
	{ AST_CAUSE_ACCESS_INFO_DISCARDED,	SKINNY_DISP_SECURITY_ERROR			, "Access information discarded" },
        { AST_CAUSE_REQUESTED_CHAN_UNAVAIL,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Requested channel not available" },
	{ AST_CAUSE_PRE_EMPTED,			SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Pre-empted" },
        { AST_CAUSE_FACILITY_NOT_SUBSCRIBED,	SKINNY_DISP_TEMP_FAIL				, "Facility not subscribed" },
	{ AST_CAUSE_OUTGOING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR			, "Outgoing call barred" },
        { AST_CAUSE_INCOMING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR			, "Incoming call barred" },
	{ AST_CAUSE_BEARERCAPABILITY_NOTAUTH,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not authorized" },
        { AST_CAUSE_BEARERCAPABILITY_NOTAVAIL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not available" },
        { AST_CAUSE_BEARERCAPABILITY_NOTIMPL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not implemented" },
        { AST_CAUSE_CHAN_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Channel not implemented" },
	{ AST_CAUSE_FACILITY_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Facility not implemented" },
        { AST_CAUSE_INVALID_CALL_REFERENCE,	SKINNY_DISP_TEMP_FAIL				, "Invalid call reference value" },
	{ AST_CAUSE_INCOMPATIBLE_DESTINATION,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Incompatible destination" },
        { AST_CAUSE_INVALID_MSG_UNSPECIFIED,	SKINNY_DISP_ERROR_UNKNOWN			, "Invalid message unspecified" },
        { AST_CAUSE_MANDATORY_IE_MISSING,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL	, "Mandatory information element is missing" },
        { AST_CAUSE_MESSAGE_TYPE_NONEXIST,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL	, "Message type nonexist." },
  	{ AST_CAUSE_WRONG_MESSAGE,		SKINNY_DISP_ERROR_MISMATCH			, "Wrong message" },
        { AST_CAUSE_IE_NONEXIST,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Info. element nonexist or not implemented" },
	{ AST_CAUSE_INVALID_IE_CONTENTS,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Invalid information element contents" },
        { AST_CAUSE_WRONG_CALL_STATE,		SKINNY_DISP_TEMP_FAIL				, "Message not compatible with call state" },
	{ AST_CAUSE_RECOVERY_ON_TIMER_EXPIRE,	SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT		, "Recover on timer expiry" },
        { AST_CAUSE_MANDATORY_IE_LENGTH_ERROR,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Mandatory IE length error" },
	{ AST_CAUSE_PROTOCOL_ERROR,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Protocol error, unspecified" },
        { AST_CAUSE_INTERWORKING,		SKINNY_DISP_NETWORK_CONGESTION_REROUTING	, "Interworking, unspecified" },
        
        // aliases for backward compatibility reasons
	{ AST_CAUSE_BUSY,          		SKINNY_DISP_BUSY                                , "User busy" },
	{ AST_CAUSE_FAILURE,       		SKINNY_DISP_NETWORK_CONGESTION_REROUTING        , "Network out of order" },
	{ AST_CAUSE_NORMAL,        		SKINNY_DISP_ON_HOOK                             , "Normal Clearing" },
	{ AST_CAUSE_NOANSWER,      		SKINNY_DISP_EMPTY                               , "User alerting, no answer" },
	{ AST_CAUSE_CONGESTION,    		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Circuit/channel congestion" },
	{ AST_CAUSE_UNREGISTERED,  		SKINNY_DISP_TEMP_FAIL				, "Subscriber Absent, Try Again" },
	{ AST_CAUSE_NOTDEFINED,    		SKINNY_DISP_EMPTY				, "Not Defined" },
	{ AST_CAUSE_NOSUCHDRIVER,  		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE            , "Channel not implemented" },
	/* *INDENT-ON* */

};

int set_pbx_callerid(PBX_CHANNEL_TYPE * ast_chan, sccp_callinfo_t * callInfo);
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target);
PBX_CHANNEL_TYPE *pbx_channel_search_locked(int (*is_match) (PBX_CHANNEL_TYPE *, void *), void *data);
struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error);
struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar);
struct ast_config *pbx_config_load(const char *filename, const char *who_asked, struct ast_flags flags);
PBX_VARIABLE_TYPE *pbx_variable_new(PBX_VARIABLE_TYPE * v);
const char *pbx_inet_ntoa(struct in_addr ia);
int pbx_str2cos(const char *value, unsigned int *cos);
int pbx_str2tos(const char *value, unsigned int *tos);
int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar);
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag);
int pbx_moh_start(PBX_CHANNEL_TYPE * chan, const char *mclass, const char *interpclass);
PBX_CHANNEL_TYPE *sccp_search_remotepeer_locked(int (*const found_cb) (PBX_CHANNEL_TYPE * c, void *data), void *data);
const char *pbx_inet_ntoa(struct in_addr ia);

//#if ASTERISK_VERSION_NUMBER >=10900
//#define ast_format_type struct ast_format
//#else
#    define ast_format_type int
//#endif

skinny_codec_t pbx_codec2skinny_codec(ast_format_type fmt);
ast_format_type skinny_codec2pbx_codec(skinny_codec_t codec);

// support for old uin32_t format (only temporarily
#    define pbx_format2skinny_format (uint32_t)pbx_codec2skinny_codec
#    define skinny_format2pbx_format(_x) skinny_codec2pbx_codec((skinny_codec_t)_x)

//skinny_codec_t *pbx_codecs2skinny_codecs(int codecs);
int skinny_codecs2pbx_codecs(skinny_codec_t * skinny_codecs);

//skinny_codec_t *pbx_codec_pref2sccp_codec_pref(struct ast_codec_pref *prefs);
//struct ast_codec_pref *sccp_codec_pref2pbx_codec_pref(skinny_codec_t *skinny_codecs);

#    define sccp_strndup(str, len) \
	_sccp_strndup((str), (len), __FILE__, __LINE__, __PRETTY_FUNCTION__)

extern inline char *_sccp_strndup(const char *str, size_t len, const char *file, int lineno, const char *func);

extern inline char *_sccp_strndup(const char *str, size_t len, const char *file, int lineno, const char *func)
{
	char *newstr = NULL;

	if (str) {
		if (!(newstr = strndup(str, len)))
			ast_log(LOG_ERROR, "Memory Allocation Failure in function %s at line %d of %s\n", func, lineno, file);
	}
	return newstr;
}

/* 
 * sccp_free_ptr should be used when a function pointer for free() needs to be 
 * passed as the argument to a function. Otherwise, astmm will cause seg faults.
 */
static void sccp_free_ptr(void *ptr) attribute_unused;
static void sccp_free_ptr(void *ptr)
{
	sccp_free(ptr);
}

/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 * \param ast_chan Asterisk Channel
 * \return SCCP Channel on Success or Null on Fail
 */
sccp_channel_t *get_sccp_channel_from_ast_channel(PBX_CHANNEL_TYPE * ast_chan);
int sccp_asterisk_pbx_fktChannelWrite(struct ast_channel *ast, const char *funcname, char *args, const char *value);
int sccp_wrapper_asterisk_forceHangup(PBX_CHANNEL_TYPE * ast_channel, pbx_hangup_type_t pbx_hangup_type);
int sccp_wrapper_asterisk_requestHangup(PBX_CHANNEL_TYPE * ast_channel);

/***** database *****/
boolean_t sccp_asterisk_addToDatabase(const char *family, const char *key, const char *value);
boolean_t sccp_asterisk_getFromDatabase(const char *family, const char *key, char *out, int outlen);
boolean_t sccp_asterisk_removeFromDatabase(const char *family, const char *key);
boolean_t sccp_asterisk_removeTreeFromDatabase(const char *family, const char *key);
/***** end - database *****/

int sccp_asterisk_moh_start(const PBX_CHANNEL_TYPE * pbx_channel, const char *mclass, const char* interpclass);
void sccp_asterisk_moh_stop(const PBX_CHANNEL_TYPE * pbx_channel);
#endif

