
/*!
 * \file 	chan_sccp.h
 * \brief 	Chan SCCP Main Class
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \brief 	Main chan_sccp Class
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \warning 	File has been Lined up using 8 Space TABS
 *
 * $Date$
 * $Revision$
 */

#ifndef __CHAN_SCCP_H
#    define __CHAN_SCCP_H

#    if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#    endif

#    include "config.h"
#    include "common.h"

#    define sccp_mutex_t ast_mutex_t

/* fix cast for (uint64_t) during printf */
#    if SIZEOF_LONG == SIZEOF_LONG_LONG
#        define ULONG long unsigned int
#        define UI64FMT "%lu"							// contributed by Stéphane Plantard
#    else
#        define ULONG long long unsigned int
#        define UI64FMT "%llu"							// contributed by Stéphane Plantard
#    endif

/* Add bswap function if necessary */
#    ifndef HAVE_BSWAP_16
static inline unsigned short bswap_16(unsigned short x) {
	return (x >> 8) | (x << 8);
}
#    endif
#    ifndef HAVE_BSWAP_32
static inline unsigned int bswap_32(unsigned int x) {
	return (bswap_16(x & 0xffff) << 16) | (bswap_16(x >> 16));
}
#    endif
#    ifndef HAVE_BSWAP_64
static inline unsigned long long bswap_64(unsigned long long x) {
	return (((unsigned long long)bswap_32(x & 0xffffffffull)) << 32) | (bswap_32(x >> 32));
}
#    endif

/* Byte swap based on platform endianes */
#    if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
#        define letohl(x) (x)
#        define letohs(x) (x)
#        define htolel(x) (x)
#        define htoles(x) (x)
#    else
#        define letohs(x) bswap_16(x)
#        define htoles(x) bswap_16(x)
#        define letohl(x) bswap_32(x)
#        define htolel(x) bswap_32(x)
#    endif

#    define SCCP_TECHTYPE_STR "SCCP"

/* Versioning */
#    ifndef SCCP_VERSION
#        define SCCP_VERSION "custom"
#    endif

#    ifndef SCCP_BRANCH
#        define SCCP_BRANCH "trunk"
#    endif

#    define SCCP_LOCK_TRIES 10
#    define SCCP_LOCK_USLEEP 100
#    define SCCP_MIN_DTMF_DURATION 80		// 80 ms

/*! \todo I don't like the -1 returned value */
#    define sccp_true(x) (pbx_true(x) ? 1 : 0)

// changed debug parameter to match any DEBUGCATegories given on an sccp_log line
#    define sccp_log(x) if ((sccp_globals->debug & x) != 0)  ast_verbose

#    define GLOB(x) sccp_globals->x

#    ifdef HAVE_LIBGC
#        if HAVE_LIBPTHREAD
#            define GC_PTHREADS
#            define GC_THREADS
#            undef _REENTRANT
#            define _REENTRANT
//#            define GC_REDIRECT_TO_LOCAL
#            include <gc/gc_local_alloc.h>
#            include <gc/gc_backptr.h>
#        endif
#        include <gc/gc.h>
#        define CHECK_LEAKS() GC_gcollect()
#    else
#        define CHECK_LEAKS()
#    endif

#    define SCCP_FILE_VERSION(file, version) \
static void __attribute__((constructor)) __register_file_version(void) \
{ \
	pbx_register_file_version(file, version); \
} \
static void __attribute__((destructor)) __unregister_file_version(void) \
{ \
	pbx_unregister_file_version(file); \
}

#    define DEV_ID_LOG(x) x ? x->id : "SCCP"

	extern struct sccp_pbx_cb sccp_pbx;

#    define PBX(x) sccp_pbx.x

#    ifdef CS_AST_HAS_TECH_PVT
#        define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->tech_pvt)
#    else
#        define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->pvt->pvt)
#    endif

#    ifdef CS_AST_HAS_TECH_PVT
#        define CS_AST_CHANNEL_PVT_TYPE(x) x->tech->type
#    else
#        define CS_AST_CHANNEL_PVT_TYPE(x) x->type
#    endif

#    ifdef CS_AST_HAS_TECH_PVT
#        define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->tech->type, y, strlen(y))
#    else
#        define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->type, y, strlen(y))
#    endif

#    define CS_AST_CHANNEL_PVT_IS_SCCP(x) CS_AST_CHANNEL_PVT_CMP_TYPE(x,"SCCP")

#    ifdef CS_AST_HAS_BRIDGED_CHANNEL
#        define CS_AST_BRIDGED_CHANNEL(x) ast_bridged_channel(x)
#    else
#        define CS_AST_BRIDGED_CHANNEL(x) x->bridge
#    endif

#    ifdef AST_MAX_EXTENSION
#        define SCCP_MAX_EXTENSION AST_MAX_EXTENSION
#    else
#        define SCCP_MAX_EXTENSION 80
#    endif

#    ifdef AST_MAX_CONTEXT
#        define SCCP_MAX_CONTEXT AST_MAX_CONTEXT
#    else
#        define SCCP_MAX_CONTEXT 80
#    endif

#    ifdef MAX_LANGUAGE
#        define SCCP_MAX_LANGUAGE MAX_LANGUAGE
#    else
#        define SCCP_MAX_LANGUAGE 20
#    endif

#    undef SCCP_MAX_ACCOUNT_CODE
#    ifdef AST_MAX_ACCOUNT_CODE
#        define SCCP_MAX_ACCOUNT_CODE AST_MAX_ACCOUNT_CODE
#    else
#        define SCCP_MAX_ACCOUNT_CODE 50
#    endif

#    ifdef MAX_MUSICCLASS
#        define SCCP_MAX_MUSICCLASS MAX_MUSICCLASS
#    else
#        define SCCP_MAX_MUSICCLASS 80
#    endif

#    define SCCP_MAX_HOSTNAME_LEN 100
#    define SCCP_MAX_MESSAGESTACK 10
typedef unsigned long long sccp_group_t;
typedef struct channel sccp_channel_t;					/*!< SCCP Channel Structure */
typedef struct sccp_session sccp_session_t;				/*!< SCCP Session Structure */
typedef struct sccp_line sccp_line_t;					/*!< SCCP Line Structure */
typedef struct sccp_speed sccp_speed_t;					/*!< SCCP Speed Structure */
typedef struct sccp_service sccp_service_t;				/*!< SCCP Service Structure */
typedef struct sccp_device sccp_device_t;				/*!< SCCP Device Structure */
typedef struct sccp_addon sccp_addon_t;					/*!< SCCP Add-On Structure */// Added on SVN 327 -FS
typedef struct sccp_hint sccp_hint_t;					/*!< SCCP Hint Structure */
typedef struct sccp_hostname sccp_hostname_t;				/*!< SCCP HostName Structure */

#    ifdef CS_DEVSTATE_FEATURE
typedef struct sccp_devstate_specifier sccp_devstate_specifier_t;	/*!< SCCP Custom DeviceState Specifier Structure */
#    endif
typedef struct sccp_selectedchannel sccp_selectedchannel_t;		/*!< SCCP Selected Channel Structure */
typedef struct sccp_ast_channel_name sccp_ast_channel_name_t;		/*!< SCCP Asterisk Channel Name Structure */
typedef struct sccp_buttonconfig sccp_buttonconfig_t;			/*!< SCCP Button Config Structure */
typedef struct sccp_hotline sccp_hotline_t;				/*!< SCCP Hotline Structure */
typedef struct sccp_callinfo sccp_callinfo_t;				/*!< SCCP Call Information Structure */
typedef enum { FALSE = 0, TRUE = 1 } boolean_t;				/*!< Asterisk Reverses True and False; nice !! */
typedef enum { ON, OFF } light_t;					/*!< Enum Light Status */
typedef enum { NO_CHANGES = 0, MINOR_CHANGES = 1, CHANGES_NEED_RESET = 2 } sccp_diff_t;	/*!< SCCP Diff Structure */

typedef void sk_func(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
typedef enum { LINE, SPEEDDIAL, SERVICE, FEATURE, EMPTY } button_type_t;	/*!< Enum Button Type */
typedef enum { ANSWER_LAST_FIRST = 1, ANSWER_OLDEST_FIRST = 2 } call_answer_order_t;	/*!< Enum Call Answer Order */
typedef enum { PARK_RESULT_FAIL = 0, PARK_RESULT_SUCCESS = 1 } sccp_parkresult_t;	/*!<  */
typedef enum { CALLERID_PRESENCE_FORBIDDEN = 0, CALLERID_PRESENCE_ALLOWED = 1 } sccp_calleridpresence_t;	/*!<  */
typedef enum {
	SCCP_RTP_STATUS_INACTIVE = 0,
	SCCP_RTP_STATUS_REQUESTED = 1 << 0,				/*!< rtp not started, but format was requested */
	SCCP_RTP_STATUS_PROGRESS = 1 << 1,
	SCCP_RTP_STATUS_ACTIVE = 1 << 2,
} sccp_rtp_status_t;							/*!< RTP status information */
typedef enum {
	SCCP_EXTENSION_NOTEXISTS = 0,
	SCCP_EXTENSION_MATCHMORE = 1,
	SCCP_EXTENSION_EXACTMATCH = 2,
} sccp_extension_status_t;						/*!< extension status information */

typedef enum {
	SCCP_REQUEST_STATUS_UNAVAIL = 0,
	SCCP_REQUEST_STATUS_SUCCESS,
} sccp_channel_request_status_t;					/*!< channel request status */

typedef enum {
	SCCP_MESSAGE_PRIORITY_IDLE = 0,
	SCCP_MESSAGE_PRIORITY_VOICEMAIL,
	SCCP_MESSAGE_PRIORITY_MONITOR,
	SCCP_MESSAGE_PRIORITY_PRIVACY,
	SCCP_MESSAGE_PRIORITY_DND,
	SCCP_MESSAGE_PRIORITY_CFWD,
} sccp_message_priority_t;

typedef enum {
	SCCP_PUSH_RESULT_FAIL = 0,
	SCCP_PUSH_RESULT_NOT_SUPPORTED,
	SCCP_PUSH_RESULT_SUCCESS,
} sccp_push_result_t;

typedef enum {
	SCCP_TOKEN_STATE_NOTOKEN = 0,
	SCCP_TOKEN_STATE_ACK,
	SCCP_TOKEN_STATE_REJ,
} sccp_tokenstate_t;

#    include "sccp_protocol.h"
#    if HAVE_ASTERISK
#        include "pbx_impl/ast/ast.h"
#    endif

/*!
 * \brief SCCP ButtonType Structure
 */
static const struct sccp_buttontype {
        button_type_t buttontype;
	const char *const text;
} sccp_buttontypes[] = {
        /* *INDENT-OFF* */
	{LINE, 		"LINE"},
	{SPEEDDIAL, 	"SPEEDDIAL"},
	{SERVICE, 	"SERVICE"},
	{FEATURE, 	"FEATURE"},
	{EMPTY, 	"EMPTY"}
	/* *INDENT-ON* */
};

/*!
 * \brief SCCP Conference Structure
 */
struct sccp_conference;

/*!
 * \brief SCCP Private Channel Data Structure
 */
struct sccp_private_channel_data;

/*!
 * \brief SCCP Debug Category Enum
 */
/* *INDENT-OFF* */
typedef enum {
	DEBUGCAT_CORE 		= 1 << 0,
	DEBUGCAT_SCCP 		= 1 << 1,
	DEBUGCAT_HINT 		= 1 << 2,
	DEBUGCAT_RTP 		= 1 << 3,
	DEBUGCAT_DEVICE 	= 1 << 4,
	DEBUGCAT_LINE 		= 1 << 5,
	DEBUGCAT_ACTION 	= 1 << 6,
	DEBUGCAT_CHANNEL 	= 1 << 7,
	DEBUGCAT_CLI 		= 1 << 8,
	DEBUGCAT_CONFIG 	= 1 << 9,
	DEBUGCAT_FEATURE 	= 1 << 10,
	DEBUGCAT_FEATURE_BUTTON = 1 << 11,
	DEBUGCAT_SOFTKEY 	= 1 << 12,
	DEBUGCAT_INDICATE 	= 1 << 13,
	DEBUGCAT_PBX 		= 1 << 14,
	DEBUGCAT_SOCKET 	= 1 << 15,
	DEBUGCAT_MWI 		= 1 << 16,
	DEBUGCAT_EVENT 		= 1 << 17,
	DEBUGCAT_ADV_FEATURE 	= 1 << 18,
	DEBUGCAT_CONFERENCE 	= 1 << 19,
	DEBUGCAT_BUTTONTEMPLATE = 1 << 20,
	DEBUGCAT_SPEEDDIAL 	= 1 << 21,
	DEBUGCAT_CODEC 		= 1 << 22,
	DEBUGCAT_REALTIME 	= 1 << 23,
	DEBUGCAT_LOCK 		= 1 << 24,
	DEBUGCAT_THREADLOCK 	= 1 << 25,
	DEBUGCAT_MESSAGE 	= 1 << 26,
	DEBUGCAT_NEWCODE 	= 1 << 27,
	DEBUGCAT_THPOOL		= 1 << 28,
	DEBUGCAT_HIGH 		= 1 << 29,
	DEBUGCAT_NYI		= 1 << 30,				// Not Yet Implemented
	DEBUGCAT_FIXME		= 1 << 31,				// Fix Me
	/* *INDENT-ON* */
} sccp_debug_category_t;						/*!< SCCP Debug Category Enum */

/*!
 * \brief SCCP Verbose Level Structure
 */
static const struct sccp_debug_category {
	const char *const key;
	sccp_debug_category_t category;
	const char *const text;
} sccp_debug_categories[] = {
	/* *INDENT-OFF* */
	{"all",  		0xffffffff, 		"all debug levels"},
	{"none",  		0x00000000, 		"all debug levels"},
	{"core",  		DEBUGCAT_CORE, 		"core debug level"},
	{"sccp",  		DEBUGCAT_SCCP, 		"sccp debug level"},
	{"hint",  		DEBUGCAT_HINT, 		"hint debug level"},
	{"rtp",  		DEBUGCAT_RTP, 		"rtp debug level"},
	{"device",  		DEBUGCAT_DEVICE, 	"device debug level"},
	{"line",  		DEBUGCAT_LINE, 		"line debug level"},
	{"action",  		DEBUGCAT_ACTION, 	"action debug level"},
	{"channel",  		DEBUGCAT_CHANNEL, 	"channel debug level"},
	{"cli",  		DEBUGCAT_CLI, 		"cli debug level"},
	{"config",  		DEBUGCAT_CONFIG, 	"config debug level"},
	{"feature",  		DEBUGCAT_FEATURE, 	"feature debug level"},
	{"feature_button",  	DEBUGCAT_FEATURE_BUTTON,"feature_button debug level"},
	{"softkey",  		DEBUGCAT_SOFTKEY, 	"softkey debug level"},
	{"indicate",  		DEBUGCAT_INDICATE, 	"indicate debug level"},
	{"pbx",  		DEBUGCAT_PBX, 		"pbx debug level"},
	{"socket",  		DEBUGCAT_SOCKET, 	"socket debug level"},
	{"mwi",  		DEBUGCAT_MWI, 		"mwi debug level"},
	{"event",  		DEBUGCAT_EVENT, 	"event debug level"},
	{"adv_feature",  	DEBUGCAT_ADV_FEATURE, 	"adv_feature debug level"},
	{"conference",  	DEBUGCAT_CONFERENCE, 	"conference debug level"},
	{"buttontemplate",  	DEBUGCAT_BUTTONTEMPLATE,"buttontemplate debug level"},
	{"speeddial",  		DEBUGCAT_SPEEDDIAL, 	"speeddial debug level"},
	{"codec",  		DEBUGCAT_CODEC, 	"codec debug level"},
	{"realtime",  		DEBUGCAT_REALTIME, 	"realtime debug level"},
	{"lock",  		DEBUGCAT_LOCK, 		"lock debug level"},
	{"threadlock",  	DEBUGCAT_THREADLOCK, 	"thread-lock debug level"},
	{"message",  		DEBUGCAT_MESSAGE, 	"message debug level"},
	{"newcode",  		DEBUGCAT_NEWCODE, 	"newcode debug level"}, 
	{"high",  		DEBUGCAT_HIGH, 		"high debug level"},
       	{"nyi",  		DEBUGCAT_NYI, 		"not yet implemented developer debug level"},
       	{"fixme",  		DEBUGCAT_FIXME, 	"fixme developer debug level"},
//     	{"fyi",  		DEBUGCAT_FYI, 		"for your information developer debug level"},
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
	SCCP_FEATURE_TEST6,
	SCCP_FEATURE_TEST7,
	SCCP_FEATURE_TEST8,
	SCCP_FEATURE_TEST9,
	SCCP_FEATURE_TESTA,
	SCCP_FEATURE_TESTB,
	SCCP_FEATURE_TESTC,
	SCCP_FEATURE_TESTD,
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
} sccp_feature_type_t;								/*!< SCCP Feature Type Enum */

static const struct sccp_feature_type {
	sccp_feature_type_t featureType;
	const char *const text;
} sccp_feature_types[] = {
	/* *INDENT-OFF* */
	{SCCP_FEATURE_UNKNOWN, 		"FEATURE_UNKNOWN"},
	{SCCP_FEATURE_CFWDNONE,	 	"cfwd off"},
	{SCCP_FEATURE_CFWDALL, 		"cfwdall"},
	{SCCP_FEATURE_CFWDBUSY,	 	"cfwdbusy"},
	{SCCP_FEATURE_DND, 		"dnd"},
	{SCCP_FEATURE_PRIVACY, 		"privacy"},
	{SCCP_FEATURE_MONITOR, 		"monitor"},
	{SCCP_FEATURE_HOLD, 		"hold"},
	{SCCP_FEATURE_TRANSFER,	 	"transfer"},
	{SCCP_FEATURE_MULTIBLINK, 	"multiblink"},
	{SCCP_FEATURE_MOBILITY, 	"mobility"},
	{SCCP_FEATURE_CONFERENCE,	"conference"},
	{SCCP_FEATURE_TEST6, 		"FEATURE_TEST6"},
	{SCCP_FEATURE_TEST7, 		"FEATURE_TEST7"},
	{SCCP_FEATURE_TEST8, 		"FEATURE_TEST8"},
	{SCCP_FEATURE_TEST9, 		"FEATURE_TEST9"},
	{SCCP_FEATURE_TESTA, 		"FEATURE_TESTA"},
	{SCCP_FEATURE_TESTB, 		"FEATURE_TESTB"},
	{SCCP_FEATURE_TESTC, 		"FEATURE_TESTC"},
	{SCCP_FEATURE_TESTD, 		"FEATURE_TESTD"},
	{SCCP_FEATURE_TESTE, 		"FEATURE_TESTE"},
	{SCCP_FEATURE_TESTF, 		"FEATURE_TESTF"},
	{SCCP_FEATURE_TESTG, 		"FEATURE_TESTG"},
	{SCCP_FEATURE_TESTH, 		"FEATURE_TESTH"},
	{SCCP_FEATURE_TESTI, 		"FEATURE_TESTI"},
	{SCCP_FEATURE_TESTJ, 		"FEATURE_TESTJ"},
#    ifdef CS_DEVSTATE_FEATURE
	{SCCP_FEATURE_DEVSTATE, 	"devstate"},
#    endif
	{SCCP_FEATURE_PICKUP, 		"pickup"},
	/* *INDENT-ON* */
};

typedef enum {
	SCCP_FEATURE_MONITOR_STATE_DISABLED = 0,
	SCCP_FEATURE_MONITOR_STATE_ENABLED_NOTACTIVE,
	SCCP_FEATURE_MONITOR_STATE_ACTIVE,
} sccp_feature_monitor_state_t;							/*!< monitor feature state */

/*!
 * \brief SCCP Feature Configuration Structure
 */
typedef struct {
	boolean_t enabled;							/*!< Feature Enabled */
	boolean_t initialized;							/*!< Feature Enabled */
	uint32_t previousStatus;						/*!< Feature Previous State */
	uint32_t status;							/*!< Feature State */
	char configOptions[255];						/*!< space for config values */
} sccp_featureConfiguration_t;							/*!< SCCP Feature Configuration */

/*!
 * \brief SCCP Host Access Rule Structure
 *
 * internal representation of acl entries In principle user applications would have no need for this,
 * but there is sometimes a need to extract individual items, e.g. to print them, and rather than defining iterators to
 * navigate the list, and an externally visible 'struct ast_ha_entry', at least in the short term it is more convenient to make the whole
 * thing public and let users play with them.
 */
struct sccp_ha {
	struct in_addr netaddr;
	struct in_addr netmask;
	int sense;
	struct sccp_ha *next;
};

/*!
 * \brief Reading Type Enum
 */
typedef enum {
	SCCP_CONFIG_READINITIAL,
	SCCP_CONFIG_READRELOAD
} sccp_readingtype_t;								/*!< Reading Type */

/*!
 * \brief Status of configuration change
 */
typedef enum {
	SCCP_CONFIG_NOUPDATENEEDED = 0,
	SCCP_CONFIG_NEEDDEVICERESET = 1 << 1,
	SCCP_CONFIG_ERROR = 1 << 2
} sccp_configurationchange_t;							/*!< configuration state change */

/*!
 * \brief SCCP Mailbox Type Definition
 */
typedef struct sccp_mailbox sccp_mailbox_t;

/*!
 * \brief SCCP Mailbox Structure
 */
struct sccp_mailbox {
	char *mailbox;								/*!< Mailbox */
	char *context;								/*!< Context */
	 SCCP_LIST_ENTRY(sccp_mailbox_t) list;					/*!< Mailbox Linked List Entry */
};										/*!< SCCP Mailbox Structure */

/*!
 * \brief Privacy Definition
 */
#define SCCP_PRIVACYFEATURE_HINT 		1 << 1;
#define SCCP_PRIVACYFEATURE_CALLPRESENT	1 << 2;

/*!
 * \brief SCCP Currently Selected Channel Structure
 */
struct sccp_selectedchannel {
	sccp_channel_t *channel;						/*!< SCCP Channel */
	 SCCP_LIST_ENTRY(sccp_selectedchannel_t) list;				/*!< Selected Channel Linked List Entry */
};										/*!< SCCP Selected Channel Structure */

/*!
 * \brief SCCP CallInfo Structure
 */
struct sccp_callinfo {
	char calledPartyName[StationMaxNameSize];				/*!< Called Party Name */
	char calledPartyNumber[StationMaxDirnumSize];				/*!< Called Party Number */
	char cdpnVoiceMailbox[StationMaxDirnumSize];				/*!< Called Party Voicemail Box */
	unsigned int cdpnVoiceMailbox_valid:1;					/*!< TRUE if the name information is valid/present */
	unsigned int calledParty_valid:1;					/*!< TRUE if the name information is valid/present */

	char callingPartyName[StationMaxNameSize];				/*!< Calling Party Name */
	char callingPartyNumber[StationMaxDirnumSize];				/*!< Calling Party Number */
	char cgpnVoiceMailbox[StationMaxDirnumSize];				/*!< Calling Party Voicemail Box */
	unsigned int cgpnVoiceMailbox_valid:1;					/*!< TRUE if the name information is valid/present */
	unsigned int callingParty_valid:1;					/*!< TRUE if the name information is valid/present */

	char originalCalledPartyName[StationMaxNameSize];			/*!< Original Calling Party Name */
	char originalCalledPartyNumber[StationMaxDirnumSize];			/*!< Original Calling Party ID */
	char originalCdpnVoiceMailbox[StationMaxDirnumSize];			/*!< Original Called Party VoiceMail Box */
	unsigned int originalCdpnVoiceMailbox_valid:1;				/*!< TRUE if the name information is valid/present */
	unsigned int originalCalledParty_valid:1;					/*!< TRUE if the name information is valid/present */

	char originalCallingPartyName[StationMaxNameSize];			/*!< Original Calling Party Name */
	char originalCallingPartyNumber[StationMaxDirnumSize];			/*!< Original Calling Party ID */
	unsigned int originalCallingParty_valid:1;					/*!< TRUE if the name information is valid/present */

	char lastRedirectingPartyName[StationMaxNameSize];			/*!< Original Called Party Name */
	char lastRedirectingPartyNumber[StationMaxDirnumSize];			/*!< Original Called Party ID */
	char lastRedirectingVoiceMailbox[StationMaxDirnumSize];			/*!< Last Redirecting VoiceMail Box */
	unsigned int lastRedirectingVoiceMailbox_valid:1;			/*!< TRUE if the name information is valid/present */
	unsigned int lastRedirectingParty_valid:1;					/*!< TRUE if the name information is valid/present */

	uint32_t originalCdpnRedirectReason;					/*!< Original Called Party Redirect Reason */
	uint32_t lastRedirectingReason;						/*!< Last Redirecting Reason */
	int presentation;							/*!< Should this callerinfo be shown (privacy) */
};										/*!< SCCP CallInfo Structure */

/*!
 * \brief SCCP cfwd information
 */
typedef struct sccp_cfwd_information sccp_cfwd_information_t;			/*!< cfwd information */

/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information {
	boolean_t enabled;
	char number[SCCP_MAX_EXTENSION];
};

/*!
 * \brief for addressing individual devices on shared line
 */
struct subscriptionId {
	char number[SCCP_MAX_EXTENSION];					/*!< will be added to cid */
	char name[SCCP_MAX_EXTENSION];						/*!< will be added to cidName */
	char aux[SCCP_MAX_EXTENSION];						/*!< auxiliary parameter. Allows for phone-specific behaviour on a line. */
};

/*!
 * \brief string identifier with additional subscription id
 */
struct composedId {
	char mainId[StationMaxServiceURLSize];
	struct subscriptionId subscriptionId;
};

/*!
 * \brief SCCP Line-Devices Structure
 */
typedef struct sccp_linedevices sccp_linedevices_t;				/*!< SCCP Line Connected to Devices */

/*!
 * \brief SCCP Line-Devices Structure
 */
struct sccp_linedevices {
	sccp_device_t *device;							/*!< SCCP Device */
	sccp_line_t *line;							/*!< SCCP Line */
	uint8_t lineInstance;							/*!< line instance of this->line on this->device */

	sccp_cfwd_information_t cfwdAll;					/*!< cfwd information */
	sccp_cfwd_information_t cfwdBusy;					/*!< cfwd information */

	struct subscriptionId subscriptionId;					/*!< for addressing individual devices on shared line */
	char label[80];								/*!<  */
	 SCCP_LIST_ENTRY(sccp_linedevices_t) list;				/*!< Device Linked List Entry */
};										/*!< SCCP Line-Device Structure */

/*!
 * \brief SCCP Button Configuration Structure
 */

struct sccp_buttonconfig {
	uint8_t instance;							/*!< Instance on device */
	uint16_t index;								/*!< Button position on device */
	char label[StationMaxNameSize];						/*!< Button Name/Label */
	button_type_t type;							/*!< Button type (e.g. line, speeddial, feature, empty) */
	 SCCP_LIST_ENTRY(sccp_buttonconfig_t) list;				/*!< Button Linked List Entry */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int pendingDelete:1;
	unsigned int pendingUpdate:1;
#endif

	/*!
	 * \brief SCCP Button Structure
	 */
	union sccp_button {

		/*!
		 * \brief SCCP Button Line Structure
		 */
		struct {
			char name[StationMaxNameSize];				/*!< Button Name */
			struct subscriptionId subscriptionId;
			char options[256];
		} line;								/*!< SCCP Button Line Structure */

		/*!
		 * \brief SCCP Button Speeddial Structure
		 */
		struct sccp_speeddial {
			char ext[SCCP_MAX_EXTENSION];				/*!< SpeedDial Extension */
			char hint[SCCP_MAX_EXTENSION];				/*!< SpeedDIal Hint */
		} speeddial;							/*!< SCCP Button Speeddial Structure */

		/*!
		 * \brief SCCP Button Service Structure
		 */
		struct sccp_service {
			char url[StationMaxServiceURLSize];			/*!< The number to dial when it's hit */
		} service;							/*!< SCCP Button Service Structure  */

		/*!
		 * \brief SCCP Button Feature Structure
		 */
		struct sccp_feature {
			uint8_t index;						/*!< Button Feature Index */
			sccp_feature_type_t id;					/*!< Button Feature ID */
			char options[254];					/*!< Button Feature Options */
			uint32_t status;					/*!< Button Feature Status */
		} feature;							/*!< SCCP Button Feature Structure */
	} button;								/*!< SCCP Button Structure */
};										/*!< SCCP Button Configuration Structure */

	/*!
	 * \brief SCCP Hostname Structure
	 */
struct sccp_hostname {
	char name[SCCP_MAX_HOSTNAME_LEN];					/*!< Name of the Host */
	 SCCP_LIST_ENTRY(sccp_hostname_t) list;					/*!< Host Linked List Entry */
};										/*!< SCCP Hostname Structure */

	/*
	 * \brief SCCP DevState Specifier Structure
	 * Recording number of Device State Registrations Per Device
	 */
#ifdef CS_DEVSTATE_FEATURE
struct sccp_devstate_specifier {
	char specifier[254];							/*!< Name of the Custom  Devstate Extension */
	struct ast_event_sub *sub;						/* Asterisk event Subscription related to the devstate extension. */
	/* Note that the length of the specifier matches the length of "options" of the sccp_feature.options field,
	   to which it corresponds. */
	 SCCP_LIST_ENTRY(sccp_devstate_specifier_t) list;			/*!< Specifier Linked List Entry */
};										/*!< SCCP Devstate Specifier Structure */
#endif

/*!
 * \brief SCCP Line Structure
 * \note A line is the equivalent of a 'phone line' going to the phone.
 */
struct sccp_line {
	sccp_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */
	char id[5];								/*!< This line's ID, used for logging into (for mobility) */
	char pin[8];								/*!< PIN number for mobility/roaming. */
	char name[80];								/*!< The name of the line, so use in asterisk (i.e SCCP/[name]) */
	char description[StationMaxNameSize];					/*!< A description for the line, displayed on in header (on7960/40) or on main  screen on 7910 */
	char label[StationMaxNameSize];						/*!< A name for the line, displayed next to the button (7960/40). */

	SCCP_LIST_HEAD(, sccp_mailbox_t) mailboxes;				/*!< Mailbox Linked List Entry. To check for messages */
	char vmnum[SCCP_MAX_EXTENSION];						/*!< Voicemail number to Dial */
	char meetmenum[SCCP_MAX_EXTENSION];					/*!< Meetme Extension to be Dialed (\todo TO BE REMOVED) */
	char meetmeopts[SCCP_MAX_CONTEXT];					/*!< Meetme Options to be Used */
	char context[SCCP_MAX_CONTEXT];						/*!< The context we use for Outgoing Calls. */
	char language[SCCP_MAX_LANGUAGE];					/*!< language we use for calls */
	char accountcode[SCCP_MAX_ACCOUNT_CODE];				/*!< accountcode used in cdr */
	char musicclass[SCCP_MAX_MUSICCLASS];					/*!< musicclass assigned when getting moh */
	int amaflags;								/*!< amaflags */
	sccp_group_t callgroup;							/*!< callgroups assigned (seperated by commas) to this lines */
#ifdef CS_SCCP_PICKUP
	sccp_group_t pickupgroup;						/*!< pickupgroup assigned to this line */
#endif
	char cid_name[SCCP_MAX_EXTENSION];					/*!< Caller(Name) to use on outgoing calls */
	char cid_num[SCCP_MAX_EXTENSION];					/*!< Caller(ID) to use on outgoing calls  */
	uint16_t incominglimit;							/*!< max incoming calls limit */
	SCCP_LIST_HEAD(, sccp_channel_t) channels;				/*!< Linked list of current channels for this line */
//              uint8_t channelCount;                                           /*!< Number of currently active channels */
	SCCP_RWLIST_ENTRY(sccp_line_t) list;					/*!< global list entry */
	SCCP_LIST_HEAD(, sccp_linedevices_t) devices;				/*!< The device this line is currently registered to. */

	char *trnsfvm;								/*!< transfer to voicemail softkey. Basically a call forward */
	char secondary_dialtone_digits[10];					/*!< secondary dialtone digits */
	uint8_t secondary_dialtone_tone;					/*!< secondary dialtone tone */

	boolean_t echocancel;							/*!< echocancel phone support */
	boolean_t silencesuppression;						/*!< Silence Suppression Phone Support */
	boolean_t meetme;							/*!< Meetme on/off */
	boolean_t transfer;							/*!< Transfer Phone Support */

#ifdef CS_SCCP_REALTIME
	boolean_t realtime;							/*!< is it a realtimeconfiguration */
#endif
	PBX_VARIABLE_TYPE *variables;						/*!< Channel variables to set */
	uint8_t dnd;								/*!< dnd on line */
	uint8_t dndmode;							/*!< dnd mode: see SCCP_DNDMODE_* */
	struct subscriptionId defaultSubscriptionId;				/*!< default subscription id for shared lines */

#ifdef CS_DYNAMIC_CONFIG
	/* this is for reload routines */
	unsigned int pendingDelete:1;						/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int pendingUpdate:1;						/*!< this bit will tell the scheduler to update this line when unused */
#endif

	/*!
	 * \brief Statistic for Line Structure
	 */
	struct {
		uint8_t numberOfActiveDevices;					/*!< Number of Active Devices */
		uint8_t numberOfActiveChannels;					/*!< Number of Active Channels */
		uint8_t numberOfHoldChannels;					/*!< Number of Hold Channels */
		uint8_t numberOfDNDDevices;					/*!< Number of DND Devices */
	} statistic;								/*!< Statistics for Line Structure */

	/*!
	 * \brief VoiceMail Statistics Structure
	 */
	struct {
		int newmsgs;							/*!< New Messages */
		int oldmsgs;							/*!< Old Messages */
	} voicemailStatistic;							/*!< VoiceMail Statistics Structure */

	uint32_t configurationStatus;						/*!< what is the current configuration status - @see sccp_config_status_t */
	char adhocNumber[SCCP_MAX_EXTENSION];					/*!< number that should be dialed when device offhocks this line */

	char regexten[SCCP_MAX_EXTENSION];					/*!< Extension for auto-extension (DUNDI) */
	char regcontext[SCCP_MAX_CONTEXT];					/*!< Context for auto-extension (DUNDI) */
};										/*!< SCCP Line Structure */

/*!
 * \brief SCCP SpeedDial Button Structure
 */
struct sccp_speed {
	uint8_t config_instance;						/*!< The instance of the speeddial in the sccp.conf */
	uint16_t instance;							/*!< The instance on the current device */
	uint8_t type;								/*!< SpeedDial Button Type (SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE (hint)) */
	char name[StationMaxNameSize];						/*!< The name of the speed dial button */
	char ext[SCCP_MAX_EXTENSION];						/*!< The number to dial when it's hit */
	char hint[SCCP_MAX_EXTENSION];						/*!< The HINT on this SpeedDial */

	SCCP_LIST_ENTRY(sccp_speed_t) list;					/*!< SpeedDial Linked List Entry */

};

/*!
 * \brief SCCP Device Structure
 */
struct sccp_device {
	sccp_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */
	char id[StationMaxDeviceNameSize];					/*!< SEP<macAddress> of the device. */
	char description[40];							/*!< Internal Description. Skinny protocol does not use it */
	char config_type[10];							/*!< Model of this Phone used for setting up features/softkeys/buttons etc. */
	uint32_t skinny_type;							/*!< Model of this Phone sent by the station, devicetype */
	const sccp_deviceProtocol_t *protocol;					/*!< protocol the devices uses */
	uint32_t device_features;						/*!< device features (contains protocolversion in 8bit first segement */
	uint32_t maxstreams;							/*!< Maximum number of Stream supported by the device */
	int tz_offset;								/*!< Timezone OffSet */
	char imageversion[StationMaxVersionSize];				/*!< Version to Send to the phone */
	uint8_t accessoryused;							/*!< Accessory Used. This are for support of message 0x0073 AccessoryStatusMessage - Protocol v.11 CCM7 -FS */
	uint8_t accessorystatus;						/*!< Accessory Status */
	uint8_t registrationState;						/*!< If the device has been fully registered yet */

	sccp_devicestate_t state;						/*!< Device State (SCCP_DEVICE_ONHOOK or SCCP_DEVICE_OFFHOOK) */
	boolean_t linesRegistered;						/*!< did we answer the RegisterAvailableLinesMessage */
	int digittimeout;							/*< Digit Timeout. How long to wait for following digits */
	const struct sccp_device_indication_cb *indicate;

	char lastNumber[SCCP_MAX_EXTENSION];					/*!< Last Dialed Number */

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Video Codec Capabilities */
	} capabilities;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Video Codec Preferences */
	} preferences;

//              uint8_t earlyrtp;                                               /*!< RTP Channel State where to open the RTP Media Stream */
	sccp_channelState_t earlyrtp;						/*!< RTP Channel State where to open the RTP Media Stream */
	uint8_t protocolversion;						/*!< Skinny Supported Protocol Version */
	uint8_t inuseprotocolversion;						/*!< Skinny Used Protocol Version */
	int keepalive;								/*!< Station Specific Keepalive Timeout */
	time_t registrationTime;

	struct sccp_ha *ha;							/*!< Permit or Deny Connections to the Main Socket */
	unsigned int audio_tos;							/*!< audio stream type_of_service (TOS) (RTP) */
	unsigned int video_tos;							/*!< video stream type_of_service (TOS) (VRTP) */
	unsigned int audio_cos;							/*!< audio stream class_of_service (COS) (VRTP) */
	unsigned int video_cos;							/*!< video stream class_of_service (COS) (VRTP) */
	uint32_t conferenceid;							/*!< Conference ID */
	boolean_t meetme;							/*!< Meetme on/off */
	char meetmeopts[SCCP_MAX_CONTEXT];					/*!< Meetme Options to be Used */

	sccp_lampMode_t mwilamp;						/*!< MWI/Lamp to indicate MailBox Messages */
	boolean_t mwioncall;							/*!< MWI On Call Support (Boolean, default=on) */
	boolean_t softkeysupport;						/*!< Soft Key Support (Boolean, default=on) */
	uint32_t mwilight;							/*!< MWI/Light bit field to to store mwi light for each line and device (offset 0 is current device state) */

	boolean_t transfer;							/*!< Transfer Support (Boolean, default=on) */
	//unsigned int conference: 1;                                   /*!< Conference Support (Boolean, default=on) */
	boolean_t park;								/*!< Park Support (Boolean, default=on) */
	boolean_t cfwdall;							/*!< Call Forward All Support (Boolean, default=on) */
	boolean_t cfwdbusy;							/*!< Call Forward on Busy Support (Boolean, default=on) */
	boolean_t cfwdnoanswer;							/*!< Call Forward on No-Answer Support (Boolean, default=on) */

#ifdef CS_SCCP_PICKUP
	boolean_t pickupexten;							/*!< Pickup Extension Support (Boolean, default=on) */
	char pickupcontext[SCCP_MAX_CONTEXT];					/*!< Pickup Context to Use in DialPlan */
	boolean_t pickupmodeanswer;						/*!< Pickup Mode Answer */
#endif
	boolean_t dtmfmode;							/*!< DTMF Mode (0 inband - 1 outofband) */
	boolean_t nat;								/*!< Network Address Translation Support (Boolean, default=on) */
	boolean_t directrtp;							/*!< Direct RTP Support (Boolean, default=on) */
	boolean_t trustphoneip;							/*!< Trust Phone IP Support (Boolean, default=on) */
	boolean_t needcheckringback;						/*!< Need to Check Ring Back Support (Boolean, default=on) */

	boolean_t realtime;							/*!< is it a realtime configuration */
	sccp_channel_t *active_channel;						/*!< Active SCCP Channel */
	sccp_channel_t *transfer_channel;					/*!< SCCP Channel being Transfered */
	sccp_channel_t *conference_channel;					/*!< SCCP Channel which is going to be Conferenced */
	sccp_line_t *currentLine;						/*!< Current Line */
	sccp_session_t *session;						/*!< Current Session */
	SCCP_RWLIST_ENTRY(sccp_device_t) list;					/*!< Global Device Linked List */
	SCCP_LIST_HEAD(, sccp_buttonconfig_t) buttonconfig;			/*!< SCCP Button Config Attached to this Device */
	uint8_t linesCount;							/*!< Number of Lines */
	SCCP_LIST_HEAD(, sccp_selectedchannel_t) selectedChannels;		/*!< Selected Channel List */
	SCCP_LIST_HEAD(, sccp_addon_t) addons;					/*!< Add-Ons connect to this Device */
	SCCP_LIST_HEAD(, sccp_hostname_t) permithosts;				/*!< Permit Registration to the Hostname/IP Address */

	pthread_t postregistration_thread;					/*!< Post Registration Thread */

	PBX_VARIABLE_TYPE *variables;						/*!< Channel variables to set */
	uint8_t defaultLineInstance;						/*!< Default Line Instance */

	struct {
		boolean_t headset;						/*!< HeadSet Support (Boolean) */
		boolean_t handset;						/*!< HandSet Support (Boolean) */
		boolean_t speaker;						/*!< Speaker Support (Boolean) */
	} accessoryStatus;							/*!< Accesory Status Structure */

	struct {
		uint8_t numberOfLines;						/*!< Number of Lines */
		uint8_t numberOfSpeeddials;					/*!< Number of SpeedDials */
		uint8_t numberOfFeatures;					/*!< Number of Features */
		uint8_t numberOfServices;					/*!< Number of Services */
	} configurationStatistic;						/*!< Configuration Statistic Structure */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int pendingDelete:1;						/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int pendingUpdate:1;						/*!< this will contain the updated line struct once reloaded from config to update the line when unused */

#endif

	boolean_t isAnonymous;							/*!< Device is connected Anonymously (Guest) */
	light_t mwiLight;							/*!< MWI/Light \todo third MWI/light entry in device ? */

	struct {
		int newmsgs;							/*!< New Messages */
		int oldmsgs;							/*!< Old Messages */
	} voicemailStatistic;							/*!< VoiceMail Statistics */

	/* feature configurations */
	sccp_featureConfiguration_t privacyFeature;				/*!< Device Privacy Feature. \see SCCP_PRIVACYFEATURE_* */
	sccp_featureConfiguration_t overlapFeature;				/*!< Overlap Dial Feature */
	sccp_featureConfiguration_t monitorFeature;				/*!< Monitor (automon) Feature */
	sccp_featureConfiguration_t dndFeature;					/*!< dnd Feature */
	sccp_featureConfiguration_t priFeature;					/*!< priority Feature */
	sccp_featureConfiguration_t mobFeature;					/*!< priority Feature */
#ifdef CS_DEVSTATE_FEATURE
	SCCP_LIST_HEAD(, sccp_devstate_specifier_t) devstateSpecifiers;	/*!< List of Custom DeviceState entries the phone monitors. */
#endif

	char softkeyDefinition[50];						/*!< requested softKey configuration */

	struct {
		softkey_modes *modes;						/*!< used softkeySet */
		uint8_t size;							/*!< how many softkeysets are provided by modes */
		uint32_t activeMask[16];					/*!< enabled softkeys mask */
	} softKeyConfiguration;							/*!< SoftKeySet configuration */

	struct {
		int free;
	} scheduleTasks;

	char videoSink[MAXHOSTNAMELEN];						/*!< sink to send video */
	struct sccp_conference *conference;					/*!< conference we are part of */

	btnlist *buttonTemplate;

#ifdef CS_ADV_FEATURES
	boolean_t useRedialMenu;
#endif

#ifdef CS_SCCP_CONFERENCE
	boolean_t conferencelist_active;
#endif
	struct {
		char *action;
		uint32_t appID;
		uint32_t payload;
		uint32_t transactionID;
	} dtu_softkey;

	struct {
		sccp_tokenstate_t token;					/*!< token request state */
//              char line[40];                                          	/*!< Status Text Line */
//              int priority;                                           	/*!< Priority From 10 to 0 */
//              char temp_line[40];                                     	/*!< Temporairy Status Text Line */
//              int timeout;                                            	/*!< Timeout for temporairy status text line */
//              boolean_t overwritable;                                 	/*!< Status is overwritable */
//              boolean_t updated;                                      	/*!< Status has changed, and needs to be refreshed */
	} status;								/*!< Status Structure */
	 sccp_push_result_t(*pushURL) (const sccp_device_t * device, const char *url, uint8_t priority, uint8_t tone);
	 sccp_push_result_t(*pushTextMessage) (const sccp_device_t * device, const char *messageText, const char *from, uint8_t priority, uint8_t tone);
	 boolean_t(*checkACL) (sccp_device_t * device);
	 boolean_t(*hasDisplayPrompt) (void);

	char *(messageStack[SCCP_MAX_MESSAGESTACK]);
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
	int type;								/*!< Add-On Type */
	SCCP_LIST_ENTRY(sccp_addon_t) list;					/*!< Linked List Entry for this Add-On */
	sccp_device_t *device;							/*!< Device Associated with this Add-On */
};

/*!
 * \brief SCCP Session Structure
 * \note This contains the current session the phone is in
 */
struct sccp_session {
	sccp_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */
	void *buffer;								/*!< Session Buffer */
	int32_t buffer_size;							/*!< Session Buffer Size */
#ifdef CS_EXPERIMENTAL_NEWIP
	struct sockaddr_storage *ss;						/*!< Incoming Socket Address Storage */
#endif
	struct sockaddr_in sin;							/*!< Incoming Socket Address */
	struct in_addr ourip;							/*!< Our IP is for rtp use */
	time_t lastKeepAlive;							/*!< Last KeepAlive Time */
	struct pollfd fds[1];							/*!< File Descriptor */
	sccp_device_t *device;							/*!< Associated Device */
	SCCP_RWLIST_ENTRY(sccp_session_t) list;				/*!< Linked List Entry for this Session */
	boolean_t needcheckringback;						/*!< Need Check Ring Back. (0/1) default 1 */
	pthread_t session_thread;						/*!< Session Thread */
	uint8_t session_stop;							/*!< Signal Session Stop */
	uint8_t protocolType;
};										/*!< SCCP Session Structure */

/*!
 * \brief SCCP RTP Structure
 */
struct sccp_rtp {
	PBX_RTP_TYPE *rtp;							/*!< pbx rtp pointer */
	boolean_t isStarted;							/*!< is rtp server started */
#ifdef CS_EXPERIMENTAL_NEWIP
	struct sockaddr_storage *phone_ss;					/*!< Our Phone Socket Address Storage (openreceive) */
#endif
	struct sockaddr_in phone;						/*!< our phone information (openreceive) */
#ifdef CS_EXPERIMENTAL_NEWIP
	struct sockaddr_storage *phone_remote_ss;				/*!< Phone Destination Socket Address Storage (starttransmission) */
#endif
	struct sockaddr_in phone_remote;					/*!< phone destination address (starttransmission) */
	skinny_codec_t readFormat;						/*!< current read format */
	uint8_t readState;							/*!< current read state */
	skinny_codec_t writeFormat;						/*!< current write format */
	uint8_t writeState;							/*!< current write state */
};										/*!< SCCP RTP Structure */

/*!
 * \brief SCCP Channel Structure
 * \note This contains the current channel information
 */
struct channel {
	sccp_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */
	ast_cond_t astStateCond;

	sccp_device_t *(*getDevice) (const sccp_channel_t * channel);
	void (*setDevice) (sccp_channel_t * channel, const sccp_device_t * device);

	struct sccp_private_channel_data *privateData;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];			/*!< our channel Capability in preference order */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];
	} capabilities;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Video Codec Preferences */
	} preferences;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];			/*!< SCCP Video Codec Capabilities */
	} remoteCapabilities;

	struct sccp_callinfo callInfo;

	uint32_t callid;							/*!< Call ID */
	uint32_t passthrupartyid;						/*!< Pass Through ID */

	uint8_t state;								/*!< Internal channel state SCCP_CHANNELSTATE_* */
	uint8_t previousChannelState;						/*!< Previous channel state SCCP_CHANNELSTATE_* */
	skinny_calltype_t calltype;						/*!< Skinny Call Type as SKINNY_CALLTYPE_* */

	struct {
		int digittimeout;						/*!< Digit Timeout on Dialing State (Enbloc-Emu) */
		unsigned int deactivate:1;					/*!< Deactivate Enbloc-Emulation (Time Deviation Found) */
		int totaldigittime;						/*!< Total Time used to enter Number (Enbloc-Emu) */
		int totaldigittimesquared;					/*!< Total Time Squared used to enter Number (Enbloc-Emu) */
	} enbloc;

	struct {
		int digittimeout;						/*!< Schedule for Timeout on Dialing State */
	} scheduler;

	uint8_t ringermode;							/*!< Ringer Mode */

	char dialedNumber[SCCP_MAX_EXTENSION];					/*!< Last Dialed Number */
//              sccp_device_t *device;                                          /*!< SCCP Device */

	PBX_CHANNEL_TYPE *owner;						/*!< Asterisk Channel Owner */
	sccp_line_t *line;							/*!< SCCP Line */

	struct {
		struct sccp_rtp audio;						/*!< Asterisk RTP */
		struct sccp_rtp video;						/*!< Video RTP session */
	} rtp;

	SCCP_LIST_ENTRY(sccp_channel_t) list;					/*!< Channel Linked List */
	sccp_autoanswer_type_t autoanswer_type;					/*!< Auto Answer Type */
	uint8_t autoanswer_cause;						/*!< Auto Answer Cause */
	boolean_t answered_elsewhere;						/*!< Answered Elsewhere */

	/* don't allow sccp phones to monitor (hint) this call */
	boolean_t privacy;							/*!< Private */
	char musicclass[SCCP_MAX_MUSICCLASS];					/*!< Music Class */
	uint8_t ss_action;							/*!< Simple Switch Action. This is used in dial thread to collect numbers for callforward, pickup and so on -FS */
	uint8_t ss_data;							/*!< Simple Switch Integer param */

	/* feature sets */
//              boolean_t monitorEnabled;                                       /*!< Monitor Enabled Feature */

	struct sccp_conference *conference;					/*!< are we part of a conference? */
	sccp_channel_t *parentChannel;						/*!< if we are a cfwd channel, our parent is this */

	struct subscriptionId subscriptionId;
	unsigned int maxBitRate;
	boolean_t peerIsSCCP;							/*!< Indicates that channel-peer is also SCCP */
	void (*setMicrophone) (const sccp_channel_t * channel, boolean_t on);
	boolean_t(*isMicrophoneEnabled) (void);
};										/*!< SCCP Channel Structure */

/*!
 * \brief SCCP Global Variable Structure
 */
struct sccp_global_vars {
	sccp_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */

	pthread_t monitor_thread;						/*!< Monitor Thread */// ADDED IN 414 -FS
	sccp_mutex_t monitor_lock;						/*!< Monitor Asterisk Lock */// ADDED IN 414 -FS

	SCCP_RWLIST_HEAD(, sccp_session_t) sessions;				/*!< SCCP Sessions */
	SCCP_RWLIST_HEAD(, sccp_device_t) devices;				/*!< SCCP Devices */
	SCCP_RWLIST_HEAD(, sccp_line_t) lines;					/*!< SCCP Lines */

	sccp_mutex_t socket_lock;						/*!< Socket Lock */
	pthread_t socket_thread;						/*!< Socket Thread */
	pthread_t mwiMonitorThread;						/*!< MWI Monitor Thread */
	int descriptor;								/*!< Server Socket Descriptor */
	int usecnt;								/*!< Keep track of when we're in use. */
	sccp_mutex_t usecnt_lock;						/*!< Use Counter Asterisk Lock */

	int keepalive;								/*!< KeepAlive */
	int32_t debug;								/*!< Debug */
	char servername[StationMaxDisplayNotifySize];				/*!< ServerName */
	char context[SCCP_MAX_CONTEXT];						/*!< Global / General Context */
	char dateformat[8];							/*!< Date Format */

#ifdef CS_EXPERIMENTAL_NEWIP
	struct sockaddr_storage bindaddr_ss;					/*!< Bind Socket Storage */
#endif
	struct sockaddr_in bindaddr;						/*!< Bind IP Address */
	skinny_codec_t global_preferences[SKINNY_MAX_CAPABILITIES];		/*!< Global Asterisk Codecs */
	boolean_t prefer_quality_over_size;					/*!< When deciding which codec to choose, prefer sound quality over packet size */
	struct sccp_ha *ha;							/*!< Permit or deny connections to the main socket */
	struct sccp_ha *localaddr;						/*!< Localnet for Network Address Translation */
	struct sockaddr_in externip;						/*!< External IP Address (\todo should change to an array of external ip's, because externhost could resolv to multiple ip-addresses (h_addr_list)) */
	char externhost[MAXHOSTNAMELEN];					/*!< External HostName */
	int externrefresh;							/*!< External Refresh */
	time_t externexpire;							/*!< External Expire */

	uint8_t firstdigittimeout;						/*!< First Digit Timeout. Wait up to 16 seconds for first digit */
	int digittimeout;							/*!< Digit Timeout. How long to wait for following digits */
	char digittimeoutchar;							/*!< Digit End Character. What char will force the dial (Normally '#') */
	boolean_t recorddigittimeoutchar;					/*!< Record Digit Time Out Char. Whether to include the digittimeoutchar in the call logs */
	boolean_t simulate_enbloc;						/*!< Simulated Enbloc Dialing for older device to speed up dialing */

	uint8_t autoanswer_ring_time;						/*!< Auto Answer Ring Time */
	uint8_t autoanswer_tone;						/*!< Auto Answer Tone */
	uint8_t remotehangup_tone;						/*!< Remote Hangup Tone */
	uint8_t transfer_tone;							/*!< Transfer Tone */
	uint8_t callwaiting_tone;						/*!< Call Waiting Tone */

	char musicclass[SCCP_MAX_MUSICCLASS];					/*!< Music Class */
	char language[SCCP_MAX_LANGUAGE];					/*!< Language */

#ifdef CS_MANAGER_EVENTS
	boolean_t callevents;							/*!< Call Events */
#endif
	char accountcode[SCCP_MAX_ACCOUNT_CODE];				/*!< Account Code */

	unsigned int sccp_tos;							/*!< SCCP Socket Type of Service (TOS) (QOS) (Signaling) */
	unsigned int audio_tos;							/*!< Audio Socket Type of Service (TOS) (QOS) (RTP) */
	unsigned int video_tos;							/*!< Video Socket Type of Service (TOS) (QOS) (VRTP) */
	unsigned int sccp_cos;							/*!< SCCP Socket Class of Service (COS) (QOS) (Signaling) */
	unsigned int audio_cos;							/*!< Audio Socket Class of Service (COS) (QOS) (RTP) */
	unsigned int video_cos;							/*!< Video Socket Class of Service (COS) (QOS) (VRTP) */

	boolean_t echocancel;							/*!< Echo Canel Support (Boolean, default=on) */
	boolean_t silencesuppression;						/*!< Silence Suppression Support (Boolean, default=on)  */
	boolean_t trustphoneip;							/*!< Trust Phone IP Support (Boolean, default=on) */
	sccp_channelState_t earlyrtp;						/*!< Channel State where to open the rtp media stream */
	uint8_t dndmode;							/*!< Do Not Disturb (DND) Mode: \see SCCP_DNDMODE_* */
	boolean_t privacy;							/*!< Privacy Support (Length=2) */
	sccp_lampMode_t mwilamp;						/*!< MWI/Lamp (Length:3) */
	boolean_t mwioncall;							/*!< MWI On Call Support (Boolean, default=on) */
	boolean_t blindtransferindication;					/*!< Blind Transfer Indication Support (Boolean, default=on = SCCP_BLINDTRANSFER_MOH) */
	boolean_t cfwdall;							/*!< Call Forward All Support (Boolean, default=on) */
	boolean_t cfwdbusy;							/*!< Call Forward on Busy Support (Boolean, default=on) */
	boolean_t cfwdnoanswer;							/*!< Call Forward on No-Answer Support (Boolean, default=on) */
	boolean_t nat;								/*!< Network Address Translation */
	boolean_t directrtp;							/*!< Direct RTP */
	boolean_t useoverlap;							/*!< Overlap Dial Support */
#ifdef CS_SCCP_PICKUP
	unsigned long pickupgroup;						/*!< Pick Up Group */
	boolean_t pickupmodeanswer;						/*!< Pick Up Mode Answer */
#endif
	int amaflags;								/*!< AmaFlags */
	uint8_t protocolversion;						/*!< Skinny Protocol Version */
	call_answer_order_t callanswerorder;					/*!< Call Answer Order */
	char regcontext[SCCP_MAX_CONTEXT];					/*!< Context for auto-extension (DUNDI) */
#ifdef CS_SCCP_REALTIME
	char realtimedevicetable[45];						/*!< Database Table Name for SCCP Devices */
	char realtimelinetable[45];						/*!< Database Table Name for SCCP Lines */
#endif
	boolean_t meetme;							/*!< Meetme on/off */
	char meetmeopts[SCCP_MAX_CONTEXT];					/*!< Meetme Options to be Used */
#if ASTERISK_VERSION_NUMBER >= 10400
	struct ast_jb_conf global_jbconf;					/*!< Global Jitter Buffer Configuration */
#endif

	char used_context[SCCP_MAX_EXTENSION];					/*!< placeholder to check if context are already used in regcontext (DUNDI) */
	unsigned long callgroup;						/*!< Call Group */

	boolean_t reload_in_progress;						/*!< Reload in Progress */

	char *config_file_name;							/*!< SCCP Config File Name in Use */
	struct ast_config *cfg;
	boolean_t allowAnonymous;						/*!< Allow Anonymous/Guest Devices */
	sccp_hotline_t *hotline;						/*!< HotLine */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int pendingUpdate:1;
#endif
	char token_fallback[7];							/*!< Fall back immediatly on TokenReq (true/false/odd/even) */
	int token_backoff_time;							/*!< Backoff time on TokenReject */

};										/*!< SCCP Global Varable Structure */

/*!
 * \brief SCCP Hotline Structure
 * \note This contains the new HotLine Feature
 */
struct sccp_hotline {
	sccp_line_t *line;							/*!< Line */
	char exten[50];								/*!< Extension */
};										/*!< SCCP Hotline Structure */

/*!
 * \brief Simple Switch Modes
 * \note Used in simple switch tread to distinguish dial from other number collects
 * \note (Moved here from protocol.h)
 */
#define SCCP_SS_DIAL				0
#define SCCP_SS_GETFORWARDEXTEN			1
#define SCCP_SS_GETPICKUPEXTEN			2
#define SCCP_SS_GETMEETMEROOM			3
#define SCCP_SS_GETBARGEEXTEN			4
#define SCCP_SS_GETCBARGEROOM			5

/*!
 * \brief Scheduler Tasks
 * \note (NEW) Scheduler Implementation (NEW)
 */

#define SCCP_SCHED_DEL(id) \
({ \
	int _count = 0; \
	int _sched_res = -1; \
	while (id > -1 && (_sched_res = sccp_sched_del(id)) && ++_count < 10) \
		usleep(1); \
	if (_count == 10) { \
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP: Unable to cancel schedule ID %d.\n", id); \
	} \
	id = -1; \
	(_sched_res); \
})

extern struct sccp_global_vars *sccp_globals;

uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s);

sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_type_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel);

int sccp_devicestate(void *data);

int sccp_sched_free(void *ptr);

typedef struct softKeySetConfiguration sccp_softKeySetConfiguration_t;		/*!< SoftKeySet configuration */

/*!
 * \brief SoftKeySet Configuration Structure
 */
struct softKeySetConfiguration {
	char name[50];								/*!< Name for this configuration */
	softkey_modes modes[16];						/*!< SoftKeySet modes, see KEYMODE_* */
	uint8_t numberOfSoftKeySets;						/*!< How many SoftKeySets we definde? */
	SCCP_LIST_ENTRY(sccp_softKeySetConfiguration_t) list;			/*!< Next list entry */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int pendingDelete:1;
	unsigned int pendingUpdate:1;
#endif
};										/*!< SoftKeySet Configuration Structure */

SCCP_LIST_HEAD(softKeySetConfigList, sccp_softKeySetConfiguration_t);		/*!< SCCP LIST HEAD for softKeySetConfigList (Structure) */
extern struct softKeySetConfigList softKeySetConfig;				/*!< List of SoftKeySets */

int load_config(void);
int sccp_preUnload(void);
int sccp_reload(void);
boolean_t sccp_prePBXLoad(void);
boolean_t sccp_postPBX_load(void);

typedef int (*sccp_sched_cb) (const void *data);

int sccp_sched_add(int when, sccp_sched_cb callback, const void *data);
int sccp_sched_del(int id);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#ifdef CS_DEVSTATE_FEATURE
extern const char devstate_db_family[];
#endif

#    endif										/* __CHAN_SCCP_H */
