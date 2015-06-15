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

extern struct sccp_pbx_cb sccp_pbx;

#define PBX(x) sccp_pbx.x

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

#if defined(HAVE_UNALIGNED_BUSERROR)										// for example sparc64
typedef unsigned long sccp_group_t;										/*!< SCCP callgroup / pickupgroup */
#else
typedef ULONG sccp_group_t;											/*!< SCCP callgroup / pickupgroup */
#endif

typedef struct sccp_channel sccp_channel_t;									/*!< SCCP Channel Structure */
typedef struct sccp_session sccp_session_t;									/*!< SCCP Session Structure */
typedef struct sccp_line sccp_line_t;										/*!< SCCP Line Structure */
typedef struct sccp_speed sccp_speed_t;										/*!< SCCP Speed Structure */
typedef struct sccp_service sccp_service_t;									/*!< SCCP Service Structure */
typedef struct sccp_device sccp_device_t;									/*!< SCCP Device Structure */
typedef struct sccp_addon sccp_addon_t;										/*!< SCCP Add-On Structure */
typedef struct sccp_hint sccp_hint_t;										/*!< SCCP Hint Structure */
typedef struct sccp_hostname sccp_hostname_t;									/*!< SCCP HostName Structure */

#ifdef CS_DEVSTATE_FEATURE
typedef struct sccp_devstate_specifier sccp_devstate_specifier_t;						/*!< SCCP Custom DeviceState Specifier Structure */
#endif
typedef struct sccp_selectedchannel sccp_selectedchannel_t;							/*!< SCCP Selected Channel Structure */
typedef struct sccp_ast_channel_name sccp_ast_channel_name_t;							/*!< SCCP Asterisk Channel Name Structure */
typedef struct sccp_buttonconfig sccp_buttonconfig_t;								/*!< SCCP Button Config Structure */
typedef struct sccp_hotline sccp_hotline_t;									/*!< SCCP Hotline Structure */
typedef struct sccp_callinfo sccp_callinfo_t;									/*!< SCCP Call Information Structure */
typedef struct sccp_linedevices sccp_linedevices_t;								/*!< SCCP Line Connected to Devices */
typedef struct softKeySetConfiguration sccp_softKeySetConfiguration_t;						/*!< SoftKeySet configuration */

#ifndef SOLARIS
typedef enum { FALSE = 0, TRUE = 1 } boolean_t;									/*!< Asterisk Reverses True and False; nice !! */
#else
// solaris already has a defintion for boolean_t, having B_FALSE and B_TRUE as members
#define FALSE B_FALSE
#define TRUE B_TRUE
#endif
typedef void sk_func(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

#ifdef HAVE_ASTERISK
#include "pbx_impl/ast/ast.h"
#endif

/*!
 * \brief SCCP Conference Structure
 */
typedef struct sccp_conference sccp_conference_t;

/*!
 * \brief SCCP Private Channel Data Structure
 */
struct sccp_private_channel_data;

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
	sccp_debug_category_t category;
	const char *const text;
} sccp_debug_categories[] = {
	/* *INDENT-OFF* */
	{"all",  			0xffffffff, 		"all debug levels"},
	{"none",  			0x00000000, 		"all debug levels"},
	{"core",  			DEBUGCAT_CORE, 		"core debug level"},
	{"sccp",  			DEBUGCAT_SCCP, 		"sccp debug level"},
	{"hint",  			DEBUGCAT_HINT, 		"hint debug level"},
	{"rtp",  			DEBUGCAT_RTP, 		"rtp debug level"},
	{"device",  			DEBUGCAT_DEVICE, 	"device debug level"},
	{"line",  			DEBUGCAT_LINE, 		"line debug level"},
	{"action",  			DEBUGCAT_ACTION, 	"action debug level"},
	{"channel",  			DEBUGCAT_CHANNEL, 	"channel debug level"},
	{"cli",  			DEBUGCAT_CLI, 		"cli debug level"},
	{"config",  			DEBUGCAT_CONFIG, 	"config debug level"},
	{"feature",  			DEBUGCAT_FEATURE, 	"feature debug level"},
	{"feature_button",  		DEBUGCAT_FEATURE_BUTTON,"feature_button debug level"},
	{"softkey",  			DEBUGCAT_SOFTKEY, 	"softkey debug level"},
	{"indicate",  			DEBUGCAT_INDICATE, 	"indicate debug level"},
	{"pbx",  			DEBUGCAT_PBX, 		"pbx debug level"},
	{"socket",  			DEBUGCAT_SOCKET, 	"socket debug level"},
	{"mwi",  			DEBUGCAT_MWI, 		"mwi debug level"},
	{"event",  			DEBUGCAT_EVENT, 	"event debug level"},
	{"conference",  		DEBUGCAT_CONFERENCE, 	"conference debug level"},
	{"buttontemplate",  		DEBUGCAT_BUTTONTEMPLATE,"buttontemplate debug level"},
	{"speeddial",  			DEBUGCAT_SPEEDDIAL, 	"speeddial debug level"},
	{"codec",  			DEBUGCAT_CODEC, 	"codec debug level"},
	{"realtime",  			DEBUGCAT_REALTIME, 	"realtime debug level"},
	{"lock",  			DEBUGCAT_LOCK, 		"lock debug level"},
	{"refcount",			DEBUGCAT_REFCOUNT, 	"refcount lock debug level"},
	{"message",  			DEBUGCAT_MESSAGE, 	"message debug level"},
	{"newcode",  			DEBUGCAT_NEWCODE, 	"newcode debug level"}, 
	{"threadpool", 			DEBUGCAT_THPOOL, 	"threadpool debug level"}, 
	{"filelinefunc",		DEBUGCAT_FILELINEFUNC, 	"add line/file/function to debug output"},
	{"high",  			DEBUGCAT_HIGH, 		"high debug level"},
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP Feature Type Enum
 */
typedef enum {
	SCCP_FEATURE_UNKNOWN,
	SCCP_FEATURE_CFWDNONE,
	SCCP_FEATURE_CFWDALL,
	SCCP_FEATURE_CFWDBUSY,
	SCCP_FEATURE_DND,
	SCCP_FEATURE_PRIVACY,
	SCCP_FEATURE_MONITOR,
	SCCP_FEATURE_HOLD,
	SCCP_FEATURE_TRANSFER,
	SCCP_FEATURE_MULTIBLINK,
	SCCP_FEATURE_MOBILITY,
	SCCP_FEATURE_CONFERENCE,
	SCCP_FEATURE_DO_NOT_DISTURB,
	SCCP_FEATURE_CONF_LIST,
	SCCP_FEATURE_REMOVE_LAST_PARTICIPANT,
	SCCP_FEATURE_HLOG,
	SCCP_FEATURE_QRT,
	SCCP_FEATURE_CALLBACK,
	SCCP_FEATURE_OTHER_PICKUP,
	SCCP_FEATURE_VIDEO_MODE,
	SCCP_FEATURE_NEW_CALL,
	SCCP_FEATURE_END_CALL,
	SCCP_FEATURE_TESTE,
	SCCP_FEATURE_TESTF,
	SCCP_FEATURE_TESTG,
	SCCP_FEATURE_TESTH,
	SCCP_FEATURE_TESTI,
	SCCP_FEATURE_TESTJ,
#ifdef CS_DEVSTATE_FEATURE
	SCCP_FEATURE_DEVSTATE,
#endif
	SCCP_FEATURE_PICKUP
} sccp_feature_type_t;												/*!< SCCP Feature Type Enum */

static const struct sccp_feature_type {
	sccp_feature_type_t featureType;
	const char *const text;
} sccp_feature_types[] = {
	/* *INDENT-OFF* */
	{SCCP_FEATURE_UNKNOWN, 			"FEATURE_UNKNOWN"},
	{SCCP_FEATURE_CFWDNONE,			"cfwd off"},
	{SCCP_FEATURE_CFWDALL, 			"cfwdall"},
	{SCCP_FEATURE_CFWDBUSY,			"cfwdbusy"},
	{SCCP_FEATURE_DND, 			"dnd"},
	{SCCP_FEATURE_PRIVACY, 			"privacy"},
	{SCCP_FEATURE_MONITOR, 			"monitor"},
	{SCCP_FEATURE_HOLD, 			"hold"},
	{SCCP_FEATURE_TRANSFER,			"transfer"},
	{SCCP_FEATURE_MULTIBLINK, 		"multiblink"},
	{SCCP_FEATURE_MOBILITY, 		"mobility"},
	{SCCP_FEATURE_CONFERENCE,		"conference"},
	{SCCP_FEATURE_DO_NOT_DISTURB,		"do not disturb"},
	{SCCP_FEATURE_CONF_LIST,		"ConfList"},
	{SCCP_FEATURE_REMOVE_LAST_PARTICIPANT,	"RemoveLastParticipant"},
	{SCCP_FEATURE_HLOG, 			"Hunt Group Log-in/out"},
	{SCCP_FEATURE_QRT, 			"QRT"},
	{SCCP_FEATURE_CALLBACK, 		"CallBack"},
	{SCCP_FEATURE_OTHER_PICKUP, 		"OtherPickup"},
	{SCCP_FEATURE_VIDEO_MODE, 		"VideoMode"},
	{SCCP_FEATURE_NEW_CALL, 		"NewCall"},
	{SCCP_FEATURE_END_CALL, 		"EndCall"},
	{SCCP_FEATURE_TESTE, 			"FEATURE_TESTE"},
	{SCCP_FEATURE_TESTF, 			"FEATURE_TESTF"},
	{SCCP_FEATURE_TESTI, 			"FEATURE_TESTI"},
	{SCCP_FEATURE_TESTG, 			"Messages"},
	{SCCP_FEATURE_TESTH, 			"Directory"},
	{SCCP_FEATURE_TESTJ, 			"Application"},
#    ifdef CS_DEVSTATE_FEATURE
	{SCCP_FEATURE_DEVSTATE, 		"devstate"},
#    endif
	{SCCP_FEATURE_PICKUP, 			"pickup"},
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP Feature Configuration Structure
 */
typedef struct {
	boolean_t enabled;											/*!< Feature Enabled */
	boolean_t initialized;											/*!< Feature Enabled */
	uint32_t previousStatus;										/*!< Feature Previous State */
	uint32_t status;											/*!< Feature State */
//	char configOptions[SCCP_MAX_EXTENSION];									/*!< space for config values */
} sccp_featureConfiguration_t;											/*!< SCCP Feature Configuration */

/*!
 * \brief SCCP Host Access Rule Structure
 *
 * internal representation of acl entries In principle user applications would have no need for this,
 * but there is sometimes a need to extract individual items, e.g. to print them, and rather than defining iterators to
 * navigate the list, and an externally visible 'struct ast_ha_entry', at least in the short term it is more convenient to make the whole
 * thing public and let users play with them.
 */
struct sccp_ha {
	struct sockaddr_storage netaddr;
	struct sockaddr_storage netmask;
	struct sccp_ha *next;
	int sense;
};


/*!
 * \brief SCCP Mailbox Type Definition
 */
typedef struct sccp_mailbox sccp_mailbox_t;

/*!
 * \brief SCCP Mailbox Structure
 */
struct sccp_mailbox {
	char *mailbox;												/*!< Mailbox */
	char *context;												/*!< Context */
	SCCP_LIST_ENTRY (sccp_mailbox_t) list;									/*!< Mailbox Linked List Entry */
};														/*!< SCCP Mailbox Structure */

/*!
 * \brief Privacy Definition
 */
#define SCCP_PRIVACYFEATURE_HINT 	1 << 1;
#define SCCP_PRIVACYFEATURE_CALLPRESENT	1 << 2;

/*!
 * \brief SCCP Currently Selected Channel Structure
 */
struct sccp_selectedchannel {
	sccp_channel_t *channel;										/*!< SCCP Channel */
	SCCP_LIST_ENTRY (sccp_selectedchannel_t) list;								/*!< Selected Channel Linked List Entry */
};														/*!< SCCP Selected Channel Structure */

/*!
 * \brief SCCP CallInfo Structure
 */
struct sccp_callinfo {
	char calledPartyName[StationMaxNameSize];								/*!< Called Party Name */
	char calledPartyNumber[StationMaxDirnumSize];								/*!< Called Party Number */
	char cdpnVoiceMailbox[StationMaxDirnumSize];								/*!< Called Party Voicemail Box */

	char callingPartyName[StationMaxNameSize];								/*!< Calling Party Name */
	char callingPartyNumber[StationMaxDirnumSize];								/*!< Calling Party Number */
	char cgpnVoiceMailbox[StationMaxDirnumSize];								/*!< Calling Party Voicemail Box */

	char originalCalledPartyName[StationMaxNameSize];							/*!< Original Calling Party Name */
	char originalCalledPartyNumber[StationMaxDirnumSize];							/*!< Original Calling Party ID */
	char originalCdpnVoiceMailbox[StationMaxDirnumSize];							/*!< Original Called Party VoiceMail Box */

	char originalCallingPartyName[StationMaxNameSize];							/*!< Original Calling Party Name */
	char originalCallingPartyNumber[StationMaxDirnumSize];							/*!< Original Calling Party ID */

	char lastRedirectingPartyName[StationMaxNameSize];							/*!< Original Called Party Name */
	char lastRedirectingPartyNumber[StationMaxDirnumSize];							/*!< Original Called Party ID */
	char lastRedirectingVoiceMailbox[StationMaxDirnumSize];							/*!< Last Redirecting VoiceMail Box */

	uint32_t originalCdpnRedirectReason;									/*!< Original Called Party Redirect Reason */
	uint32_t lastRedirectingReason;										/*!< Last Redirecting Reason */
	int presentation;											/*!< Should this callerinfo be shown (privacy) */

	unsigned int cdpnVoiceMailbox_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int calledParty_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int cgpnVoiceMailbox_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int callingParty_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int originalCdpnVoiceMailbox_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int originalCalledParty_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int originalCallingParty_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int lastRedirectingVoiceMailbox_valid:1;							/*!< TRUE if the name information is valid/present */
	unsigned int lastRedirectingParty_valid:1;								/*!< TRUE if the name information is valid/present */
};														/*!< SCCP CallInfo Structure */

typedef struct sccp_call_statistic {
	uint32_t num;
	uint32_t packets_sent;
	uint32_t packets_received;
	uint32_t packets_lost;
	uint32_t jitter;
	uint32_t latency;
	uint32_t discarded;
	float opinion_score_listening_quality;
	float avg_opinion_score_listening_quality;
	float mean_opinion_score_listening_quality;
	float max_opinion_score_listening_quality;
	float variance_opinion_score_listening_quality;
	float concealement_seconds;
	float cumulative_concealement_ratio;
	float interval_concealement_ratio;
	float max_concealement_ratio;
	uint32_t concealed_seconds;
	uint32_t severely_concealed_seconds;
} sccp_call_statistics_t;

/*!
 * \brief SCCP cfwd information
 */
typedef struct sccp_cfwd_information sccp_cfwd_information_t;							/*!< cfwd information */

/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information {
	boolean_t enabled;
	char number[SCCP_MAX_EXTENSION];
};

/*!
 * \brief SCCP device-line subscriptionId
 * \note for addressing individual devices on shared line
 */
struct subscriptionId {
	char number[SCCP_MAX_EXTENSION];									/*!< will be added to cid */
	char name[SCCP_MAX_EXTENSION];										/*!< will be added to cidName */
	char aux[SCCP_MAX_EXTENSION];										/*!< auxiliary parameter. Allows for phone-specific behaviour on a line. */
};

/*!
 * \brief SCCP device-line composedId
 * \note string identifier with additional subscription id
 */
struct composedId {
	char mainId[StationMaxServiceURLSize];
	struct subscriptionId subscriptionId;
};

/*!
 * \brief SCCP Line-Devices Structure
 */
struct sccp_linedevices {
	sccp_device_t *device;											/*!< SCCP Device */
	sccp_line_t *line;											/*!< SCCP Line */
	uint8_t lineInstance;											/*!< line instance of this->line on this->device */
	uint8_t __padding1[3];
	SCCP_LIST_ENTRY (sccp_linedevices_t) list;								/*!< Device Linked List Entry */

	sccp_cfwd_information_t cfwdAll;									/*!< cfwd information */
	sccp_cfwd_information_t cfwdBusy;									/*!< cfwd information */

	struct subscriptionId subscriptionId;									/*!< for addressing individual devices on shared line */
	char label[SCCP_MAX_LABEL];										/*!<  */
};														/*!< SCCP Line-Device Structure */

/*!
 * \brief SCCP Button Configuration Structure
 */
struct sccp_buttonconfig {
	uint16_t instance;											/*!< Instance on device */
	uint16_t index;												/*!< Button position on device */
	char label[StationMaxNameSize];										/*!< Button Name/Label */
	sccp_config_buttontype_t type;										/*!< Button type (e.g. line, speeddial, feature, empty) */
	SCCP_LIST_ENTRY (sccp_buttonconfig_t) list;								/*!< Button Linked List Entry */

	/*!
	 * \brief SCCP Button Structure
	 */
	union sccp_button {

		/*!
		 * \brief SCCP Button Line Structure
		 */
		struct {
			char name[StationMaxNameSize];								/*!< Button Name */
			struct subscriptionId subscriptionId;
			char options[SCCP_MAX_BUTTON_OPTIONS];
		} line;												/*!< SCCP Button Line Structure */

		/*!
		 * \brief SCCP Button Speeddial Structure
		 */
		struct sccp_speeddial {
			char ext[SCCP_MAX_EXTENSION];								/*!< SpeedDial Extension */
			char hint[SCCP_MAX_EXTENSION];								/*!< SpeedDIal Hint */
		} speeddial;											/*!< SCCP Button Speeddial Structure */

		/*!
		 * \brief SCCP Button Service Structure
		 */
		struct sccp_service {
			char url[StationMaxServiceURLSize];							/*!< The number to dial when it's hit */
		} service;											/*!< SCCP Button Service Structure  */

		/*!
		 * \brief SCCP Button Feature Structure
		 */
		struct sccp_feature {
			uint8_t index;										/*!< Button Feature Index */
			sccp_feature_type_t id;									/*!< Button Feature ID */
			char options[SCCP_MAX_BUTTON_OPTIONS];							/*!< Button Feature Options */
			uint32_t status;									/*!< Button Feature Status */
		} feature;											/*!< SCCP Button Feature Structure */
	} button;												/*!< SCCP Button Structure */

	boolean_t pendingDelete;
	boolean_t pendingUpdate;
};														/*!< SCCP Button Configuration Structure */

/*!
 * \brief SCCP Hostname Structure
 */
struct sccp_hostname {
	char name[SCCP_MAX_HOSTNAME_LEN];									/*!< Name of the Host */
	SCCP_LIST_ENTRY (sccp_hostname_t) list;									/*!< Host Linked List Entry */
};														/*!< SCCP Hostname Structure */

/*!
 * \brief SCCP DevState Specifier Structure
 * Recording number of Device State Registrations Per Device
 */
#ifdef CS_DEVSTATE_FEATURE
struct sccp_devstate_specifier {
	char specifier[SCCP_MAX_DEVSTATE_SPECIFIER];								/*!< Name of the Custom  Devstate Extension */
	struct ast_event_sub *sub;										/* Asterisk event Subscription related to the devstate extension. */
	/* Note that the length of the specifier matches the length of "options" of the sccp_feature.options field,
	   to which it corresponds. */
	SCCP_LIST_ENTRY (sccp_devstate_specifier_t) list;							/*!< Specifier Linked List Entry */
};														/*!< SCCP Devstate Specifier Structure */
#endif

/*!
 * \brief SCCP Line Structure
 * \note A line is the equivalent of a 'phone line' going to the phone.
 */
struct sccp_line {
	//sccp_mutex_t lock;                                                                                      /*!< Asterisk: Lock Me Up and Tie me Down */
	char id[SCCP_MAX_LINE_ID];										/*!< This line's ID, used for login (for mobility) */
	char name[StationMaxNameSize];										/*!< The name of the line, so use in asterisk (i.e SCCP/[name]) */
#ifdef CS_SCCP_REALTIME
	boolean_t realtime;											/*!< is it a realtimeconfiguration */
#endif
	uint32_t configurationStatus;										/*!< what is the current configuration status - @see sccp_config_status_t */
	SCCP_RWLIST_ENTRY (sccp_line_t) list;									/*!< global list entry */
	struct {
		uint8_t numberOfActiveDevices;									/*!< Number of Active Devices */
		uint8_t numberOfActiveChannels;									/*!< Number of Active Channels */
		uint8_t numberOfHoldChannels;									/*!< Number of Hold Channels */
		uint8_t numberOfDNDDevices;									/*!< Number of DND Devices */
	} statistic;												/*!< Statistics for Line Structure */

	uint8_t incominglimit;											/*!< max incoming calls limit */
	uint8_t secondary_dialtone_tone;									/*!< secondary dialtone tone */
	char secondary_dialtone_digits[SCCP_MAX_SECONDARY_DIALTONE_DIGITS];					/*!< secondary dialtone digits */

	char *trnsfvm;												/*!< transfer to voicemail softkey. Basically a call forward */
	sccp_group_t callgroup;											/*!< callgroups assigned (seperated by commas) to this lines */
#ifdef CS_SCCP_PICKUP
	sccp_group_t pickupgroup;										/*!< pickupgroup assigned to this line */
#endif
#ifdef CS_AST_HAS_NAMEDGROUP
	char *namedcallgroup;											/*!< Named Call Group */
	char *namedpickupgroup;											/*!< Named Pickup Group */
#endif

	char cid_num[SCCP_MAX_EXTENSION];									/* smaller would be better (i.e. 32) */ /*!< Caller(ID) to use on outgoing calls  */
	char cid_name[SCCP_MAX_EXTENSION];									/* smaller would be better (i.e. 32) */ /*!< Caller(Name) to use on outgoing calls */

	int amaflags;												/*!< amaflags */
	boolean_t echocancel;											/*!< echocancel phone support */
	boolean_t silencesuppression;										/*!< Silence Suppression Phone Support */
	boolean_t meetme;											/*!< Meetme on/off */
	boolean_t transfer;											/*!< Transfer Phone Support */
	uint16_t dndmode;											/*!< dnd mode: see SCCP_DNDMODE_* */

	SCCP_LIST_HEAD (, sccp_mailbox_t) mailboxes;								/*!< Mailbox Linked List Entry. To check for messages */
	SCCP_LIST_HEAD (, sccp_channel_t) channels;								/*!< Linked list of current channels for this line */
	SCCP_LIST_HEAD (, sccp_linedevices_t) devices;								/*!< The device this line is currently registered to. */

	PBX_VARIABLE_TYPE *variables;										/*!< Channel variables to set */
	struct subscriptionId defaultSubscriptionId;								/*!< default subscription id for shared lines */

	/*!
	 * \brief VoiceMail Statistics Structure
	 */
	struct {
		int newmsgs;											/*!< New Messages */
		int oldmsgs;											/*!< Old Messages */
	} voicemailStatistic;											/*!< VoiceMail Statistics Structure */

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Capabilities */
	} combined_capabilities;										/*!< Combined Capabilities of a Shared Line Participants */

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Preferences */
	} reduced_preferences;											/*!< Will eventually take over the role from device->preferences
														     For now it will either be automatically build based on reduced d->preferences of all shared line participants,
														     Or be user defined in sccp.conf per line */
	boolean_t reduced_preferences_auto_generated;								/*!< Temporary Variable to distinguish source of l->preferences */

	char adhocNumber[SCCP_MAX_EXTENSION];									/*!< number that should be dialed when device offhocks this line */
	/* this is for reload routines */
	boolean_t pendingDelete;										/*!< this bit will tell the scheduler to delete this line when unused */
	boolean_t pendingUpdate;										/*!< this bit will tell the scheduler to update this line when unused */

	char pin[SCCP_MAX_LINE_PIN];										/*!< PIN number for mobility/roaming. */

	/* \todo next entries are not used much, should be changed into pointers, to preserve space */
	char regexten[SCCP_MAX_EXTENSION];									/*!< Extension for auto-extension (DUNDI) */
	char regcontext[SCCP_MAX_CONTEXT];									/*!< Context for auto-extension (DUNDI) */
	char description[StationMaxNameSize];									/*!< A description for the line, displayed on in header (on7960/40) or on main  screen on 7910 */
	char label[StationMaxNameSize];										/*!< A name for the line, displayed next to the button (7960/40). */
	char vmnum[SCCP_MAX_EXTENSION];										/*!< Voicemail number to Dial */
	char meetmenum[SCCP_MAX_EXTENSION];									/*!< Meetme Extension to be Dialed (\todo TO BE REMOVED) */
	char meetmeopts[SCCP_MAX_CONTEXT];									/*!< Meetme Options to be Used */
	char context[SCCP_MAX_CONTEXT];										/*!< The context we use for Outgoing Calls. */
	char language[SCCP_MAX_LANGUAGE];									/*!< language we use for calls */
	char accountcode[SCCP_MAX_ACCOUNT_CODE];								/*!< accountcode used in cdr */
	char musicclass[SCCP_MAX_MUSICCLASS];									/*!< musicclass assigned when getting moh */
	char parkinglot[SCCP_MAX_CONTEXT];									/*!< parkinglot to use */
};														/*!< SCCP Line Structure */

/*!
 * \brief SCCP SpeedDial Button Structure
 */
struct sccp_speed {
	uint8_t config_instance;										/*!< The instance of the speeddial in the sccp.conf */
	uint16_t instance;											/*!< The instance on the current device */
	uint8_t type;												/*!< SpeedDial Button Type (SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE (hint)) */
	char name[StationMaxNameSize];										/*!< The name of the speed dial button */
	char ext[SCCP_MAX_EXTENSION];										/*!< The number to dial when it's hit */
	char hint[SCCP_MAX_EXTENSION];										/*!< The HINT on this SpeedDial */
	boolean_t valid;											/*!< Speeddial configuration is valid or not */

	SCCP_LIST_ENTRY (sccp_speed_t) list;									/*!< SpeedDial Linked List Entry */

};

/*!
 * \brief SCCP Device Structure
 */
struct sccp_device {
	char id[StationMaxDeviceNameSize];									/*!< SEP<macAddress> of the device. */
	const sccp_deviceProtocol_t *protocol;									/*!< protocol the devices uses */
	uint32_t skinny_type;											/*!< Model of this Phone sent by the station, devicetype */
	uint32_t device_features;										/*!< device features (contains protocolversion in 8bit first segement */
	sccp_devicestate_t state;										/*!< Device State (SCCP_DEVICE_ONHOOK or SCCP_DEVICE_OFFHOOK) */
	sccp_earlyrtp_t earlyrtp;										/*!< RTP Channel State where to open the RTP Media Stream */
	sccp_session_t *session;										/*!< Current Session */
	SCCP_RWLIST_ENTRY (sccp_device_t) list;									/*!< Global Device Linked List */
	uint16_t keepalive;											/*!< Station Specific Keepalive Timeout */
	uint16_t keepaliveinterval;										/*!< Currently set Keepalive Timeout */
	uint8_t protocolversion;										/*!< Skinny Supported Protocol Version */
	uint8_t inuseprotocolversion;										/*!< Skinny Used Protocol Version */
	uint16_t registrationState;										/*!< If the device has been fully registered yet */
	sccp_nat_t nat;												/*!< Network Address Translation Support (Boolean, default=on) */
	boolean_t directrtp;											/*!< Direct RTP Support (Boolean, default=on) */

	sccp_channel_t *active_channel;										/*!< Active SCCP Channel */
	sccp_line_t *currentLine;										/*!< Current Line */

	struct {
		sccp_linedevices_t **instance;									/*!< used softkeySet */
		uint8_t size;											/*!< how many softkeysets are provided by modes */
	} lineButtons;

	SCCP_LIST_HEAD (, sccp_buttonconfig_t) buttonconfig;							/*!< SCCP Button Config Attached to this Device */
	SCCP_LIST_HEAD (, sccp_selectedchannel_t) selectedChannels;						/*!< Selected Channel List */
	SCCP_LIST_HEAD (, sccp_addon_t) addons;									/*!< Add-Ons connect to this Device */
	SCCP_LIST_HEAD (, sccp_hostname_t) permithosts;								/*!< Permit Registration to the Hostname/IP Address */

	char description[SCCP_MAX_DEVICE_DESCRIPTION];								/*!< Internal Description. Skinny protocol does not use it */

	uint16_t accessoryused;											/*!< Accessory Used. This are for support of message 0x0073 AccessoryStatusMessage - Protocol v.11 CCM7 -FS */
	uint16_t accessorystatus;										/*!< Accessory Status */
	char imageversion[StationMaxImageVersionSize];								/*!< Version to Send to the phone */
	char loadedimageversion[StationMaxImageVersionSize];							/*!< Loaded version on the phone */
	char config_type[SCCP_MAX_DEVICE_CONFIG_TYPE];								/*!< Model of this Phone used for setting up features/softkeys/buttons etc. */
	int32_t tz_offset;											/*!< Timezone OffSet */
	boolean_t linesRegistered;										/*!< did we answer the RegisterAvailableLinesMessage */
	uint16_t linesCount;											/*!< Number of Lines */
	uint16_t defaultLineInstance;										/*!< Default Line Instance */
	uint16_t maxstreams;											/*!< Maximum number of Stream supported by the device */
										
	struct {
		char number[SCCP_MAX_EXTENSION];
		uint16_t lineInstance;
	} redialInformation;											/*!< Last Dialed Number */
	boolean_t realtime;											/*!< is it a realtime configuration */
	char *backgroundImage;											/*!< backgroundimage we will set after device registered */
	char *ringtone;												/*!< ringtone we will set after device registered */

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Capabilities */
	} capabilities;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Preferences */
	} preferences;

	//uint8_t earlyrtp;                                                                                     /*!< RTP Channel State where to open the RTP Media Stream */
	time_t registrationTime;

	struct sccp_ha *ha;											/*!< Permit or Deny Connections to the Main Socket */
	boolean_t meetme;											/*!< Meetme on/off */
	char meetmeopts[SCCP_MAX_CONTEXT];									/*!< Meetme Options to be Used */

	skinny_lampmode_t mwilamp;										/*!< MWI/Lamp to indicate MailBox Messages */
	boolean_t mwioncall;											/*!< MWI On Call Support (Boolean, default=on) */
	boolean_t softkeysupport;										/*!< Soft Key Support (Boolean, default=on) */
	uint32_t mwilight;											/*!< MWI/Light bit field to to store mwi light for each line and device (offset 0 is current device state) */

	boolean_t transfer;											/*!< Transfer Support (Boolean, default=on) */
	boolean_t park;												/*!< Park Support (Boolean, default=on) */
	boolean_t cfwdall;											/*!< Call Forward All Support (Boolean, default=on) */
	boolean_t cfwdbusy;											/*!< Call Forward on Busy Support (Boolean, default=on) */
	boolean_t cfwdnoanswer;											/*!< Call Forward on No-Answer Support (Boolean, default=on) */
	boolean_t allowRinginNotification;									/*!< allow ringin notification for hinted extensions (Boolean, default=on) */
	boolean_t trustphoneip;											/*!< Trust Phone IP Support (Boolean, default=off) DEPRECATED */
	boolean_t needcheckringback;										/*!< Need to Check Ring Back Support (Boolean, default=on) */

#ifdef CS_SCCP_PICKUP
	boolean_t directed_pickup;										/*!< Directed Pickup Extension Support (Boolean, default=on) */
	boolean_t directed_pickup_modeanswer;									/*!< Directed Pickup Mode Answer (Boolean, default on). Answer on directed pickup */
	char directed_pickup_context[SCCP_MAX_CONTEXT];								/*!< Directed Pickup Context to Use in DialPlan */
#endif
	sccp_dtmfmode_t dtmfmode;										/*!< DTMF Mode (0 inband - 1 outofband) */

	struct {
		sccp_channel_t *transferee;									/*!< SCCP Channel which will be transferred */
		sccp_channel_t *transferer;									/*!< SCCP Channel which initiated the transferee */
	} transferChannels;

	pthread_t postregistration_thread;									/*!< Post Registration Thread */

	PBX_VARIABLE_TYPE *variables;										/*!< Channel variables to set */

	struct {
		uint8_t numberOfLines;										/*!< Number of Lines */
		uint8_t numberOfSpeeddials;									/*!< Number of SpeedDials */
		uint8_t numberOfFeatures;									/*!< Number of Features */
		uint8_t numberOfServices;									/*!< Number of Services */
	} configurationStatistic;										/*!< Configuration Statistic Structure */

	struct {
		boolean_t headset;										/*!< HeadSet Support (Boolean) */
		boolean_t handset;										/*!< HandSet Support (Boolean) */
		boolean_t speaker;										/*!< Speaker Support (Boolean) */
	} accessoryStatus;											/*!< Accesory Status Structure */
	boolean_t isAnonymous;											/*!< Device is connected Anonymously (Guest) */
	boolean_t mwiLight;											/*!< MWI/Light \todo third MWI/light entry in device ? */

	struct {
		uint16_t newmsgs;										/*!< New Messages */
		uint16_t oldmsgs;										/*!< Old Messages */
	} voicemailStatistic;											/*!< VoiceMail Statistics */

	/* feature configurations */
	sccp_featureConfiguration_t privacyFeature;								/*!< Device Privacy Feature. \see SCCP_PRIVACYFEATURE_* */
	sccp_featureConfiguration_t overlapFeature;								/*!< Overlap Dial Feature */
	sccp_featureConfiguration_t monitorFeature;								/*!< Monitor (automon) Feature */
	sccp_featureConfiguration_t dndFeature;									/*!< dnd Feature */
	sccp_dndmode_t dndmode;											/*!< dnd mode: see SCCP_DNDMODE_* */
	sccp_featureConfiguration_t priFeature;									/*!< priority Feature */
	sccp_featureConfiguration_t mobFeature;									/*!< priority Feature */

#ifdef CS_DEVSTATE_FEATURE
	SCCP_LIST_HEAD (, sccp_devstate_specifier_t) devstateSpecifiers;					/*!< List of Custom DeviceState entries the phone monitors. */
#endif
	struct {
		softkey_modes *modes;										/*!< used softkeySet */
		uint32_t activeMask[SCCP_MAX_SOFTKEY_MASK];							/*!< enabled softkeys mask */
		uint8_t size;											/*!< how many softkeysets are provided by modes */
	} softKeyConfiguration;											/*!< SoftKeySet configuration */

	struct {
		btnlist *instance;										/*!< used softkeySet */
		uint8_t size;											/*!< how many softkeysets are provided by modes */
	} speeddialButtons;

	#if 0 /* unused */
	struct {
		int free;
	} scheduleTasks;
	#endif
	
	#if 0 /* unused */
	char videoSink[MAXHOSTNAMELEN];										/*!< sink to send video */
	#endif
	
	struct {
		sccp_tokenstate_t token;									/*!< token request state */
	} status;												/*!< Status Structure */
	boolean_t useRedialMenu;
	btnlist *buttonTemplate;

	struct {
		char *action;
		uint32_t transactionID;
	} dtu_softkey;

	boolean_t (*checkACL) (sccp_device_t * device);								/*!< check ACL callback function */
	sccp_push_result_t (*pushURL) (const sccp_device_t * device, const char *url, uint8_t priority, uint8_t tone);
	sccp_push_result_t (*pushTextMessage) (const sccp_device_t * device, const char *messageText, const char *from, uint8_t priority, uint8_t tone);
	boolean_t (*hasDisplayPrompt) (void);									/*!< has Display Prompt callback function (derived from devicetype and protocol) */
	boolean_t (*hasEnhancedIconMenuSupport) (void);								/*!< has Enhanced IconMenu Support (derived from devicetype and protocol) */
	void (*retrieveDeviceCapabilities) (const sccp_device_t * device);					/*!< set device background image */
	void (*setBackgroundImage) (const sccp_device_t * device, const char *url);				/*!< set device background image */
	void (*displayBackgroundImagePreview) (const sccp_device_t * device, const char *url);			/*!< display background image as preview */
	void (*setRingTone) (const sccp_device_t * device, const char *url);					/*!< set the default Ringtone */
	const struct sccp_device_indication_cb *indicate;
	
	sccp_dtmfmode_t(*getDtmfMode) (const sccp_device_t * device);
	
	struct { 
#ifndef SCCP_ATOMIC
		sccp_mutex_t lock;										/*!< Message Stack Lock */
#endif
		char *(messages[SCCP_MAX_MESSAGESTACK]);							/*!< Message Stack Array */
	} messageStack;
	
	sccp_call_statistics_t call_statistics[2];								/*!< Call statistics */
	char *softkeyDefinition;										/*!< requested softKey configuration */
	sccp_softKeySetConfiguration_t *softkeyset;								/*!< Allow for a copy of the softkeyset, if any of the softkeys needs to be redefined, for example for urihook/uriaction */
	void (*copyStr2Locale) (const sccp_device_t *d, char *dst, const char *src, size_t dst_size);		/*!< copy string to device converted to locale if necessary */

#ifdef CS_SCCP_CONFERENCE
	sccp_conference_t *conference;										/*!< conference we are part of */ /*! \todo to be removed in favor of conference_id */
	uint32_t conference_id;											/*!< Conference ID */
	boolean_t conferencelist_active;									/*!< ConfList is being displayed on this device */
	boolean_t allow_conference;										/*!< Allow use of conference */
	boolean_t conf_play_general_announce;									/*!< Playback General Announcements (Entering/Leaving) */
	boolean_t conf_play_part_announce;									/*!< Playback Personal Announcements (You have been Kicked/You are muted) */
	boolean_t conf_mute_on_entry;										/*!< Mute participants when they enter */
	char *conf_music_on_hold_class;										/*!< Play music on hold of this class when no moderator is listening on the conference. If set to an empty string, no music on hold will be played. */
	boolean_t conf_show_conflist;										/*!< Automatically show conference list to the moderator */
#endif
	uint8_t audio_tos;											/*!< audio stream type_of_service (TOS) (RTP) */
	uint8_t video_tos;											/*!< video stream type_of_service (TOS) (VRTP) */
	uint8_t audio_cos;											/*!< audio stream class_of_service (COS) (VRTP) */
	uint8_t video_cos;											/*!< video stream class_of_service (COS) (VRTP) */

	boolean_t pendingDelete;										/*!< this bit will tell the scheduler to delete this line when unused */
	boolean_t pendingUpdate;										/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
};

// Number of additional keys per addon -FS
#define SCCP_ADDON_7914_TAPS			14
#define SCCP_ADDON_7915_TAPS			24
#define SCCP_ADDON_7916_TAPS			24

/*!
 * \brief SCCP Add-On Structure
 * \note This defines the add-ons a.k.a sidecars
 */
struct sccp_addon {
	SCCP_LIST_ENTRY (sccp_addon_t) list;									/*!< Linked List Entry for this Add-On */
	sccp_device_t *device;											/*!< Device Associated with this Add-On */
	int type;												/*!< Add-On Type */
};

/*!
 * \brief SCCP Session Structure
 * \note This contains the current session the phone is in
 */
struct sccp_session {
	time_t lastKeepAlive;											/*!< Last KeepAlive Time */
	SCCP_RWLIST_ENTRY (sccp_session_t) list;								/*!< Linked List Entry for this Session */
	sccp_device_t *device;											/*!< Associated Device */
	struct pollfd fds[1];											/*!< File Descriptor */
	struct sockaddr_storage sin;										/*!< Incoming Socket Address */
	boolean_t needcheckringback;										/*!< Need Check Ring Back. (0/1) default 1 */
	uint16_t protocolType;
	uint8_t gone_missing;											/*!< KeepAlive received from an unregistered device */
	volatile uint8_t session_stop;										/*!< Signal Session Stop */
	sccp_mutex_t write_lock;										/*!< Prevent multiple threads writing to the socket at the same time */
	sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */
	pthread_t session_thread;										/*!< Session Thread */
	struct sockaddr_storage ourip;										/*!< Our IP is for rtp use */
	struct sockaddr_storage ourIPv4;
};														/*!< SCCP Session Structure */

/*!
 * \brief SCCP RTP Structure
 */
struct sccp_rtp {
	PBX_RTP_TYPE *rtp;											/*!< pbx rtp pointer */
	uint16_t readState;											/*!< current read state */
	uint16_t writeState;											/*!< current write state */
	boolean_t directMedia;											/*!< Show if we are running in directmedia mode (set in pbx_impl during rtp bridging) */
	skinny_codec_t readFormat;										/*!< current read format */
	skinny_codec_t writeFormat;										/*!< current write format */
	struct sockaddr_storage phone;										/*!< our phone information (openreceive) */
	struct sockaddr_storage phone_remote;									/*!< phone destination address (starttransmission) */
};														/*!< SCCP RTP Structure */

/*!
 * \brief SCCP Channel Structure
 * \note This contains the current channel information
 */
struct sccp_channel {
	uint32_t callid;											/*!< Call ID */
	uint32_t passthrupartyid;										/*!< Pass Through ID */
	sccp_channelstate_t state;										/*!< Internal channel state SCCP_CHANNELSTATE_* */
	sccp_channelstate_t previousChannelState;								/*!< Previous channel state SCCP_CHANNELSTATE_* */
	sccp_channelstatereason_t channelStateReason;								/*!< Reason the new/current state was set (for example to handle HOLD differently for transfer then normal) */
	skinny_calltype_t calltype;										/*!< Skinny Call Type as SKINNY_CALLTYPE_* */
	
	PBX_CHANNEL_TYPE *owner;										/*!< Asterisk Channel Owner */
	sccp_line_t *line;											/*!< SCCP Line */
	SCCP_LIST_ENTRY (sccp_channel_t) list;									/*!< Channel Linked List */
	char dialedNumber[SCCP_MAX_EXTENSION];									/*!< Last Dialed Number */
	char designator[CHANNEL_DESIGNATOR_SIZE];
	struct subscriptionId subscriptionId;
	boolean_t answered_elsewhere;										/*!< Answered Elsewhere */
	boolean_t privacy;											/*!< Private */

#if DEBUG
	sccp_device_t *(*getDevice_retained) (const sccp_channel_t * channel, const char *filename, int lineno, const char *func);	/*!< temporary function to retrieve refcounted device */
#else
	sccp_device_t *(*getDevice_retained) (const sccp_channel_t * channel);					/*!< temporary function to retrieve refcounted device */
#endif
	void (*setDevice) (sccp_channel_t * channel, const sccp_device_t * device);				/*!< set refcounted device connected to the channel */
	char currentDeviceId[StationMaxDeviceNameSize];								/*!< Returns a constant char of the Device Id if available */

	struct sccp_private_channel_data *privateData;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< our channel Capability in preference order */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];
	} capabilities;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Preferences */
	} preferences;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Capabilities */
	} remoteCapabilities;

	struct {
		uint32_t digittimeout;										/*!< Digit Timeout on Dialing State (Enbloc-Emu) */
		boolean_t deactivate;										/*!< Deactivate Enbloc-Emulation (Time Deviation Found) */
		uint32_t totaldigittime;									/*!< Total Time used to enter Number (Enbloc-Emu) */
		uint32_t totaldigittimesquared;									/*!< Total Time Squared used to enter Number (Enbloc-Emu) */
	} enbloc;

	struct {
#ifndef SCCP_ATOMIC
		sccp_mutex_t lock;
#endif
		volatile CAS32_TYPE deny;
		int digittimeout;										/*!< Schedule for Timeout on Dialing State */
		int hangup;											/*!< Automatic hangup after invalid/congested indication */
	} scheduler;


	struct {
		struct sccp_rtp audio;										/*!< Asterisk RTP */
		struct sccp_rtp video;										/*!< Video RTP session */
		//struct sccp_rtp text;                                                                         /*!< Video RTP session */
		uint8_t peer_dtmfmode;
	} rtp;

	uint16_t ringermode;											/*!< Ringer Mode */
	uint16_t autoanswer_cause;										/*!< Auto Answer Cause */
	sccp_autoanswer_t autoanswer_type;									/*!< Auto Answer Type */

	/* don't allow sccp phones to monitor (hint) this call */
	sccp_softswitch_t softswitch_action;									/*!< Simple Switch Action. This is used in dial thread to collect numbers for callforward, pickup and so on -FS */
	uint16_t ss_data;											/*!< Simple Switch Integer param */
	uint16_t subscribers;											/*!< Used to determine if a sharedline should be hungup immediately, if everybody declined the call */

	sccp_channel_t *parentChannel;										/*!< if we are a cfwd channel, our parent is this */

	sccp_conference_t *conference;										/*!< are we part of a conference? */ /*! \todo to be removed instead of conference_id */
	uint32_t conference_id;											/*!< Conference ID (might be safer to use instead of conference) */
	uint32_t conference_participant_id;									/*!< Conference Participant ID */

	int32_t maxBitRate;
	boolean_t peerIsSCCP;											/*!< Indicates that channel-peer is also SCCP */
	void (*setMicrophone) (sccp_channel_t * channel, boolean_t on);
	boolean_t (*hangupRequest) (sccp_channel_t * channel);
	boolean_t (*isMicrophoneEnabled) (void);

	/* next should be converted to pointers, to reduce size */
	char musicclass[SCCP_MAX_MUSICCLASS];									/*!< Music Class */
	sccp_callinfo_t callInfo;
	sccp_dtmfmode_t dtmfmode;										/*!< DTMF Mode (0 inband - 1 outofband) */
	sccp_video_mode_t videomode;										/*!< Video Mode (0 off - 1 user - 2 auto) */

#if ASTERISK_VERSION_GROUP >= 111
	int16_t pbx_callid_created;
#endif
};														/*!< SCCP Channel Structure */

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

	struct sockaddr_storage bindaddr;									/*!< Bind IP Address */
	struct sccp_ha *ha;											/*!< Permit or deny connections to the main socket */
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
	boolean_t meetme;											/*!< Meetme on/off */
	char meetmeopts[SCCP_MAX_CONTEXT];									/*!< Meetme Options to be Used */
	boolean_t allowAnonymous;										/*!< Allow Anonymous/Guest Devices */
#if ASTERISK_VERSION_NUMBER >= 10400
	struct ast_jb_conf global_jbconf;									/*!< Global Jitter Buffer Configuration */
#endif
	char servername[StationMaxDisplayNotifySize];								/*!< ServerName */
	char context[SCCP_MAX_CONTEXT];										/*!< Global / General Context */
	skinny_codec_t global_preferences[SKINNY_MAX_CAPABILITIES];						/*!< Global Asterisk Codecs */
	char externhost[MAXHOSTNAMELEN];									/*!< External HostName */
	char musicclass[SCCP_MAX_MUSICCLASS];									/*!< Music Class */
	char language[SCCP_MAX_LANGUAGE];									/*!< Language */
	char accountcode[SCCP_MAX_ACCOUNT_CODE];								/*!< Account Code */
	char regcontext[SCCP_MAX_CONTEXT];									/*!< Context for auto-extension (DUNDI) */
#ifdef CS_SCCP_REALTIME
	char realtimedevicetable[SCCP_MAX_REALTIME_TABLE_NAME];							/*!< Database Table Name for SCCP Devices */
	char realtimelinetable[SCCP_MAX_REALTIME_TABLE_NAME];							/*!< Database Table Name for SCCP Lines */
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
 * \brief SCCP Hotline Structure
 * \note This contains the new HotLine Feature
 */
struct sccp_hotline {
	sccp_line_t *line;											/*!< Line */
	char exten[AST_MAX_EXTENSION];										/*!< Extension */
};														/*!< SCCP Hotline Structure */

/*!
 * \brief Scheduler Tasks
 * \note (NEW) Scheduler Implementation (NEW)
 */
#define SCCP_SCHED_DEL(id) 												\
({															\
	int _count = 0; 												\
	int _sched_res = -1; 												\
	while (id > -1 && (_sched_res = PBX(sched_del)(id)) && ++_count < 10) 						\
		usleep(1); 												\
	if (_count == 10) { 												\
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP: Unable to cancel schedule ID %d.\n", id); 		\
	} 														\
	id = -1; 			/* this might be seen as a side effect */					\
	(_sched_res); 	 												\
})

extern struct sccp_global_vars *sccp_globals;

int sccp_handle_message(sccp_msg_t * msg, sccp_session_t * s);

sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel);

int sccp_sched_free(void *ptr);

/*!
 * \brief SCCP SoftKeySet Configuration Structure
 */
struct softKeySetConfiguration {
	char name[SCCP_MAX_SOFTKEYSET_NAME];									/*!< Name for this configuration */
	softkey_modes modes[SCCP_MAX_SOFTKEY_MODES];								/*!< SoftKeySet modes, see KEYMODE_ */
	sccp_softkeyMap_cb_t *softkeyCbMap;									/*!< Softkey Callback Map, ie handlers */
	SCCP_LIST_ENTRY (sccp_softKeySetConfiguration_t) list;							/*!< Next list entry */
	boolean_t pendingDelete;
	boolean_t pendingUpdate;
	uint8_t numberOfSoftKeySets;										/*!< Number of SoftKeySets we define */
};														/*!< SoftKeySet Configuration Structure */
SCCP_LIST_HEAD (softKeySetConfigList, sccp_softKeySetConfiguration_t);						/*!< SCCP LIST HEAD for softKeySetConfigList (Structure) */
extern struct softKeySetConfigList softKeySetConfig;								/*!< List of SoftKeySets */

int load_config(void);
int sccp_preUnload(void);
int sccp_reload(void);
boolean_t sccp_prePBXLoad(void);
boolean_t sccp_postPBX_load(void);

typedef int (*sccp_sched_cb) (const void *data);

int sccp_updateExternIp(void);

#if defined(__cplusplus) || defined(c_plusplus)
/* *INDENT-ON* */
}
#endif

#ifdef CS_DEVSTATE_FEATURE
extern const char devstate_db_family[];
#endif
#endif														/* __CHAN_SCCP_H */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
