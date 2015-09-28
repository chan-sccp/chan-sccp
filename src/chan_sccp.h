/*!
 * \file        chan_sccp.h
 * \brief       Chan SCCP Main Class
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \brief       Main chan_sccp Class
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \warning     File has been Lined up using 8 Space TABS
 *
 * $Date$
 * $Revision$
 */

#ifndef __CHAN_SCCP_H
#define __CHAN_SCCP_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
/* *INDENT-OFF* */
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define gcc_inline __inline__
#else
#define gcc_inline
#endif

#include <config.h>
//#include "common.h"

#define sccp_mutex_t ast_mutex_t

/* Add bswap function if necessary */
#if HAVE_BYTESWAP_H
#include <byteswap.h>
#elif HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#elif HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#ifndef HAVE_BSWAP_16
static inline unsigned short bswap_16(unsigned short x)
{
	return (x >> 8) | (x << 8);
}
#endif
#ifndef HAVE_BSWAP_32
static inline unsigned int bswap_32(unsigned int x)
{
	return (bswap_16(x & 0xffff) << 16) | (bswap_16(x >> 16));
}
#endif
#ifndef HAVE_BSWAP_64
static inline unsigned long long bswap_64(unsigned long long x)
{
	return (((unsigned long long) bswap_32(x & 0xffffffffull)) << 32) | (bswap_32(x >> 32));
}
#endif

/* Byte swap based on platform endianes */
#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
#define letohl(x) (x)
#define letohs(x) (x)
#define htolel(x) (x)
#define htoles(x) (x)
#else
#define letohs(x) bswap_16(x)
#define htoles(x) bswap_16(x)
#define letohl(x) bswap_32(x)
#define htolel(x) bswap_32(x)
#endif

#define SCCP_TECHTYPE_STR "SCCP"

#define RET_STRING(_a) #_a
#define STRINGIFY(_a) RET_STRING(_a)

/* Versioning */
#ifndef SCCP_VERSION
#define SCCP_VERSION "custom"
#endif

#ifndef SCCP_BRANCH
#define SCCP_BRANCH "trunk"
#endif
char SCCP_VERSIONSTR[300];
char SCCP_REVISIONSTR[30];

#define SCCP_FILENAME_MAX 80
#if defined(PATH_MAX)
#define SCCP_PATH_MAX PATH_MAX
#else
#define	SCCP_PATH_MAX 2048
#endif

#define SCCP_MIN_KEEPALIVE 30
#define SCCP_LOCK_TRIES 10
#define SCCP_LOCK_USLEEP 100
#define SCCP_MIN_DTMF_DURATION 80										// 80 ms
#define SCCP_FIRST_LINEINSTANCE 1										/* whats the instance of the first line */
#define SCCP_FIRST_SERVICEINSTANCE 1										/* whats the instance of the first service button */
#define SCCP_FIRST_SPEEDDIALINSTANCE 1										/* whats the instance of the first speeddial button */

#define SCCP_DISPLAYSTATUS_TIMEOUT 5

/* Simulated Enbloc Dialing */
#define SCCP_SIM_ENBLOC_DEVIATION 3.5
#define SCCP_SIM_ENBLOC_MAX_PER_DIGIT 400
#define SCCP_SIM_ENBLOC_MIN_DIGIT 3
#define SCCP_SIM_ENBLOC_TIMEOUT 2

#define SCCP_HANGUP_TIMEOUT 15000

#define THREADPOOL_MIN_SIZE 2
#define THREADPOOL_MAX_SIZE 10
#define THREADPOOL_RESIZE_INTERVAL 10

#define CHANNEL_DESIGNATOR_SIZE 32
#define SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT 2000									// ms
#define SCCP_BACKTRACE_SIZE 10

#define DEFAULT_PBX_STR_BUFFERSIZE 512

/*! \todo I don't like the -1 returned value */
#define sccp_true(x) (pbx_true(x) ? 1 : 0)
#define sccp_false(x) (pbx_false(x) ? 1 : 0)

// When DEBUGCAT_HIGH is set, we use ast_log instead of ast_verbose
#define sccp_log1(...) { if ((sccp_globals->debug & (DEBUGCAT_FILELINEFUNC)) == DEBUGCAT_FILELINEFUNC) { ast_log(AST_LOG_NOTICE, __VA_ARGS__); } else { ast_verbose(__VA_ARGS__); } }
//#define sccp_log1(...) { ast_log(AST_LOG_DEBUG, __VA_ARGS__); ast_verbose(__VA_ARGS__); }
//#define sccp_log1(...) { ast_verbose(__VA_ARGS__); }
#define sccp_log(_x) if ((sccp_globals->debug & (_x))) sccp_log1
#define sccp_log_and(_x) if ((sccp_globals->debug & (_x)) == (_x)) sccp_log1

#define GLOB(x) sccp_globals->x

#if defined(LOW_MEMORY)
#define SCCP_FILE_VERSION(file, version)
#else
#define SCCP_FILE_VERSION(file, version) \
static void __attribute__((constructor)) __register_file_version(void) \
{ \
	pbx_register_file_version(file, version); \
} \
static void __attribute__((destructor)) __unregister_file_version(void) \
{ \
	pbx_unregister_file_version(file); \
}
#endif

#define DEV_ID_LOG(x) (x && !sccp_strlen_zero(x->id)) ? x->id : "SCCP"

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->tech_pvt)
#else
#define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->pvt->pvt)
#endif

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT_TYPE(x) x->tech->type
#else
#define CS_AST_CHANNEL_PVT_TYPE(x) x->type
#endif

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->tech->type, y, strlen(y))
#else
#define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->type, y, strlen(y))
#endif

#define CS_AST_CHANNEL_PVT_IS_SCCP(x) CS_AST_CHANNEL_PVT_CMP_TYPE(x,"SCCP")

#ifdef AST_MAX_EXTENSION
#define SCCP_MAX_EXTENSION AST_MAX_EXTENSION
#else
#define SCCP_MAX_EXTENSION 80
#endif
#define SCCP_MAX_AUX 16

#ifdef AST_MAX_CONTEXT
#define SCCP_MAX_CONTEXT AST_MAX_CONTEXT
#else
#define SCCP_MAX_CONTEXT 80
#endif

#ifdef MAX_LANGUAGE
#define SCCP_MAX_LANGUAGE MAX_LANGUAGE
#else
#define SCCP_MAX_LANGUAGE 20
#endif

#undef SCCP_MAX_ACCOUNT_CODE
#ifdef AST_MAX_ACCOUNT_CODE
#define SCCP_MAX_ACCOUNT_CODE AST_MAX_ACCOUNT_CODE
#else
#define SCCP_MAX_ACCOUNT_CODE 50
#endif

#ifdef MAX_MUSICCLASS
#define SCCP_MAX_MUSICCLASS MAX_MUSICCLASS
#else
#define SCCP_MAX_MUSICCLASS 80
#endif

#define SCCP_MAX_HOSTNAME_LEN SCCP_MAX_EXTENSION 
#define SCCP_MAX_MESSAGESTACK 10
#define SCCP_MAX_SOFTKEYSET_NAME 48
#define SCCP_MAX_SOFTKEY_MASK 16
#define SCCP_MAX_SOFTKEY_MODES 16
#define SCCP_MAX_DEVICE_DESCRIPTION 40
#define SCCP_MAX_DEVICE_CONFIG_TYPE 16
#define SCCP_MAX_LABEL SCCP_MAX_EXTENSION
#define SCCP_MAX_BUTTON_OPTIONS 256
#define SCCP_MAX_DEVSTATE_SPECIFIER 256
#define SCCP_MAX_LINE_ID 8
#define SCCP_MAX_LINE_PIN 8
#define SCCP_MAX_SECONDARY_DIALTONE_DIGITS 10
#define SCCP_MAX_DATE_FORMAT 8
#define SCCP_MAX_REALTIME_TABLE_NAME 45

#ifdef HAVE_ASTERISK
#include "pbx_impl/ast/ast.h"
#endif

/*!
 * \brief SCCP Debug Category Enum
 */
typedef enum {
	/* *INDENT-OFF* */
	DEBUGCAT_CORE 			= 1 << 0,
	DEBUGCAT_SCCP 			= 1 << 1,
	DEBUGCAT_HINT 			= 1 << 2,
	DEBUGCAT_RTP 			= 1 << 3,
	DEBUGCAT_DEVICE 		= 1 << 4,
	DEBUGCAT_LINE 			= 1 << 5,
	DEBUGCAT_ACTION 		= 1 << 6,
	DEBUGCAT_CHANNEL 		= 1 << 7,
	DEBUGCAT_CLI 			= 1 << 8,
	DEBUGCAT_CONFIG 		= 1 << 9,
	DEBUGCAT_FEATURE 		= 1 << 10,
	DEBUGCAT_FEATURE_BUTTON		= 1 << 11,
	DEBUGCAT_SOFTKEY 		= 1 << 12,
	DEBUGCAT_INDICATE 		= 1 << 13,
	DEBUGCAT_PBX 			= 1 << 14,
	DEBUGCAT_SOCKET 		= 1 << 15,
	DEBUGCAT_MWI 			= 1 << 16,
	DEBUGCAT_EVENT 			= 1 << 17,
	DEBUGCAT_CONFERENCE 		= 1 << 18,
	DEBUGCAT_BUTTONTEMPLATE 	= 1 << 19,
	DEBUGCAT_SPEEDDIAL 		= 1 << 20,
	DEBUGCAT_CODEC 			= 1 << 21,
	DEBUGCAT_REALTIME 		= 1 << 22,
	DEBUGCAT_LOCK 			= 1 << 23,
	DEBUGCAT_REFCOUNT		= 1 << 24,
	DEBUGCAT_MESSAGE 		= 1 << 25,
	DEBUGCAT_NEWCODE 		= 1 << 26,
	DEBUGCAT_THPOOL			= 1 << 27,
	DEBUGCAT_FILELINEFUNC		= 1 << 28,
	DEBUGCAT_HIGH 			= 1 << 29,
	/* *INDENT-ON* */
} sccp_debug_category_t;											/*!< SCCP Debug Category Enum (saved in global_vars:debug = uint32_t) */

/*!
 * \brief SCCP Verbose Level Structure
 */
static const struct sccp_debug_category {
	const char *const key;
	const char *const text;
	sccp_debug_category_t category;
} sccp_debug_categories[] = {
	/* *INDENT-OFF* */
	{"all",			"all debug levels", 			0xffffffff,},
	{"none",		"all debug levels", 			0x00000000,},
	{"core",		"core debug level", 			DEBUGCAT_CORE},
	{"sccp",		"sccp debug level", 			DEBUGCAT_SCCP},
	{"hint",		"hint debug level", 			DEBUGCAT_HINT},
	{"rtp",			"rtp debug level", 			DEBUGCAT_RTP},
	{"device",		"device debug level", 			DEBUGCAT_DEVICE},
	{"line",		"line debug level", 			DEBUGCAT_LINE},
	{"action",		"action debug level", 			DEBUGCAT_ACTION},
	{"channel",		"channel debug level", 			DEBUGCAT_CHANNEL},
	{"cli",			"cli debug level", 			DEBUGCAT_CLI},
	{"config",		"config debug level", 			DEBUGCAT_CONFIG},
	{"feature",		"feature debug level", 			DEBUGCAT_FEATURE},
	{"feature_button",	"feature_button debug level",		DEBUGCAT_FEATURE_BUTTON},
	{"softkey",		"softkey debug level", 			DEBUGCAT_SOFTKEY},
	{"indicate",		"indicate debug level",	 		DEBUGCAT_INDICATE},
	{"pbx",			"pbx debug level", 			DEBUGCAT_PBX},
	{"socket",		"socket debug level", 			DEBUGCAT_SOCKET},
	{"mwi",			"mwi debug level", 			DEBUGCAT_MWI},
	{"event",		"event debug level", 			DEBUGCAT_EVENT},
	{"conference",		"conference debug level", 		DEBUGCAT_CONFERENCE},
	{"buttontemplate",	"buttontemplate debug level",		DEBUGCAT_BUTTONTEMPLATE},
	{"speeddial",		"speeddial debug level",		DEBUGCAT_SPEEDDIAL},
	{"codec",		"codec debug level", 			DEBUGCAT_CODEC},
	{"realtime",		"realtime debug level",	 		DEBUGCAT_REALTIME},
	{"lock",		"lock debug level", 			DEBUGCAT_LOCK},
	{"refcount",		"refcount lock debug level", 		DEBUGCAT_REFCOUNT},
	{"message",		"message debug level", 			DEBUGCAT_MESSAGE},
	{"newcode",		"newcode debug level", 			DEBUGCAT_NEWCODE},
	{"threadpool",		"threadpool debug level",	 	DEBUGCAT_THPOOL},
	{"filelinefunc",	"add line/file/function to debug output", DEBUGCAT_FILELINEFUNC},
	{"high",		"high debug level", 			DEBUGCAT_HIGH},
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP device-line subscriptionId
 * \note for addressing individual devices on shared line
 * \todo current size if 176 bits, could/should be reduced by using charptr instead
 * \todo at the moment subscriptionId is being copied from lines to channel and linedevices
 */
struct subscriptionId {
	char number[SCCP_MAX_EXTENSION];									/*!< will be added to cid */
	char name[SCCP_MAX_EXTENSION];										/*!< will be added to cidName */
	char aux[SCCP_MAX_AUX];											/*!< auxiliary parameter. Allows for phone-specific behaviour on a line. */
};

/*!
 * \brief SCCP device-line composedId
 * \note string identifier with additional subscription id
 */
struct composedId {
	char mainId[StationMaxServiceURLSize];
	sccp_subscription_id_t subscriptionId;
};

/*!
 * \brief SCCP Global Variable Structure
 */
struct sccp_global_vars {
	int descriptor;												/*!< Server Socket Descriptor */
	int keepalive;												/*!< KeepAlive */
	int32_t debug;												/*!< Debug */
	int module_running;
	sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */

#if ASTERISK_VERSION_GROUP < 110
	pthread_t monitor_thread;										/*!< Monitor Thread */
	sccp_mutex_t monitor_lock;										/*!< Monitor Asterisk Lock */
#endif

	sccp_threadpool_t *general_threadpool;									/*!< General Work Threadpool */

	SCCP_RWLIST_HEAD (, sccp_session_t) sessions;								/*!< SCCP Sessions */
	SCCP_RWLIST_HEAD (, sccp_device_t) devices;								/*!< SCCP Devices */
	SCCP_RWLIST_HEAD (, sccp_line_t) lines;									/*!< SCCP Lines */

	sccp_mutex_t socket_lock;										/*!< Socket Lock */
	sccp_mutex_t usecnt_lock;										/*!< Use Counter Asterisk Lock */
	int usecnt;												/*!< Keep track of when we're in use. */
	int amaflags;												/*!< AmaFlags */
	pthread_t socket_thread;										/*!< Socket Thread */
	pthread_t mwiMonitorThread;										/*!< MWI Monitor Thread */

	char dateformat[SCCP_MAX_DATE_FORMAT];									/*!< Date Format */

	struct sccp_ha *ha;											/*!< Permit or deny connections to the main socket */
	struct sockaddr_storage bindaddr;									/*!< Bind IP Address */
	struct sccp_ha *localaddr;										/*!< Localnet for Network Address Translation */
	struct sockaddr_storage externip;									/*!< External IP Address (\todo should change to an array of external ip's, because externhost could resolv to multiple ip-addresses (h_addr_list)) */
	boolean_t simulate_enbloc;										/*!< Simulated Enbloc Dialing for older device to speed up dialing */
	int externrefresh;											/*!< External Refresh */

	time_t externexpire;											/*!< External Expire */
	boolean_t recorddigittimeoutchar;									/*!< Record Digit Time Out Char. Whether to include the digittimeoutchar in the call logs */
	uint8_t firstdigittimeout;										/*!< First Digit Timeout. Wait up to 16 seconds for first digit */
	uint8_t digittimeout;											/*!< Digit Timeout. How long to wait for following digits */
	char digittimeoutchar;											/*!< Digit End Character. What char will force the dial (Normally '#') */

	uint8_t autoanswer_ring_time;										/*!< Auto Answer Ring Time */
	uint8_t autoanswer_tone;										/*!< Auto Answer Tone */
	uint8_t remotehangup_tone;										/*!< Remote Hangup Tone */
	uint8_t transfer_tone;											/*!< Transfer Tone */
	uint8_t callwaiting_tone;										/*!< Call Waiting Tone */
	uint8_t callwaiting_interval;										/*!< Call Waiting Ring Interval */
	
	uint8_t sccp_tos;											/*!< SCCP Socket Type of Service (TOS) (QOS) (Signaling) */
	uint8_t audio_tos;											/*!< Audio Socket Type of Service (TOS) (QOS) (RTP) */
	uint8_t video_tos;											/*!< Video Socket Type of Service (TOS) (QOS) (VRTP) */
	uint8_t sccp_cos;											/*!< SCCP Socket Class of Service (COS) (QOS) (Signaling) */
	uint8_t audio_cos;											/*!< Audio Socket Class of Service (COS) (QOS) (RTP) */
	uint8_t video_cos;											/*!< Video Socket Class of Service (COS) (QOS) (VRTP) */

	uint8_t __padding[1];
	#if 0	/* unused */
	uint16_t protocolversion;										/*!< Skinny Protocol Version */
	#endif
	
	boolean_t dndFeature;											/*!< Do Not Disturb (DND) Mode: \see SCCP_DNDMODE_* */
	boolean_t transfer_on_hangup;										/*!< Complete transfer on hangup */

#ifdef CS_MANAGER_EVENTS
	boolean_t callevents;											/*!< Call Events */
#endif

	boolean_t echocancel;											/*!< Echo Canel Support (Boolean, default=on) */
	boolean_t silencesuppression;										/*!< Silence Suppression Support (Boolean, default=on)  */
	boolean_t trustphoneip;											/*!< Trust Phone IP Support (Boolean, default=on) */
	sccp_earlyrtp_t earlyrtp;										/*!< Channel State where to open the rtp media stream */
	boolean_t privacy;											/*!< Privacy Support (Length=2) */
	skinny_lampmode_t mwilamp;										/*!< MWI/Lamp (Length:3) */
	boolean_t mwioncall;											/*!< MWI On Call Support (Boolean, default=on) */
	sccp_blindtransferindication_t blindtransferindication;							/*!< Blind Transfer Indication Support (Boolean, default=on = SCCP_BLINDTRANSFER_MOH) */
	boolean_t cfwdall;											/*!< Call Forward All Support (Boolean, default=on) */
	boolean_t cfwdbusy;											/*!< Call Forward on Busy Support (Boolean, default=on) */
	boolean_t cfwdnoanswer;											/*!< Call Forward on No-Answer Support (Boolean, default=on) */
	sccp_nat_t nat;												/*!< Network Address Translation */
	boolean_t directrtp;											/*!< Direct RTP */
	boolean_t useoverlap;											/*!< Overlap Dial Support */
	sccp_group_t callgroup;											/*!< Call Group */
#ifdef CS_SCCP_PICKUP
	sccp_group_t pickupgroup;										/*!< PickUp Group */
	boolean_t directed_pickup_modeanswer;									/*!< Directed PickUp Mode Answer (boolean, default" on) */
#endif
	sccp_call_answer_order_t callanswerorder;								/*!< Call Answer Order */
	boolean_t allowAnonymous;										/*!< Allow Anonymous/Guest Devices */
	boolean_t meetme;											/*!< Meetme on/off */
	char *meetmeopts;											/*!< Meetme Options to be Used */
#if ASTERISK_VERSION_NUMBER >= 10400
	struct ast_jb_conf global_jbconf;									/*!< Global Jitter Buffer Configuration */
#endif
	char *servername;											/*!< ServerName */
	char *context;												/*!< Global / General Context */
	skinny_codec_t global_preferences[SKINNY_MAX_CAPABILITIES];						/*!< Global Asterisk Codecs */
	char *externhost;											/*!< External HostName */
	char *musicclass;											/*!< Music Class */
	char *language;												/*!< Language */
	char *accountcode;											/*!< Account Code */
	char *regcontext;											/*!< Context for auto-extension (DUNDI) */
#ifdef CS_SCCP_REALTIME
	char *realtimedevicetable;										/*!< Database Table Name for SCCP Devices */
	char *realtimelinetable;											/*!< Database Table Name for SCCP Lines */
#endif
	char used_context[SCCP_MAX_EXTENSION];									/*!< placeholder to check if context are already used in regcontext (DUNDI) */

	char *config_file_name;											/*!< SCCP Config File Name in Use */
	struct ast_config *cfg;
	sccp_hotline_t *hotline;										/*!< HotLine */

	char *token_fallback;											/*!< Fall back immediatly on TokenReq (true/false/odd/even) */
	int token_backoff_time;											/*!< Backoff time on TokenReject */
	int server_priority;											/*!< Server Priority to fallback to */

	boolean_t reload_in_progress;										/*!< Reload in Progress */

	boolean_t pendingUpdate;
};														/*!< SCCP Global Varable Structure */

/*!
 * \brief Scheduler Tasks
 * \note (NEW) Scheduler Implementation (NEW)
 */
#define SCCP_SCHED_DEL(id) 												\
({															\
	int _count = 0; 												\
	int _sched_res = -1; 												\
	while (id > -1 && (_sched_res = iPbx.sched_del(id)) && ++_count < 10) 						\
		usleep(1); 												\
	if (_count == 10) { 												\
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP: Unable to cancel schedule ID %d.\n", id); 		\
	} 														\
	id = -1; 			/* this might be seen as a side effect */					\
	(_sched_res); 	 												\
})

/* Global Allocations */
SCCP_LIST_HEAD (softKeySetConfigList, sccp_softKeySetConfiguration_t);						/*!< SCCP LIST HEAD for softKeySetConfigList (Structure) */
extern struct sccp_global_vars *sccp_globals;
extern struct softKeySetConfigList softKeySetConfig;								/*!< List of SoftKeySets */
#ifdef CS_DEVSTATE_FEATURE
extern const char devstate_db_family[];
#endif

/* Function Declarations */
int sccp_sched_free(void *ptr);
sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel);
int sccp_handle_message(constMessagePtr msg, constSessionPtr s);
int32_t sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug);
char *sccp_get_debugcategories(int32_t debugvalue);
int load_config(void);
int sccp_preUnload(void);
int sccp_reload(void);
boolean_t sccp_prePBXLoad(void);
boolean_t sccp_postPBX_load(void);
int sccp_updateExternIp(void);

#if defined(__cplusplus) || defined(c_plusplus)
/* *INDENT-ON* */
}
#endif

#endif														/* __CHAN_SCCP_H */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
