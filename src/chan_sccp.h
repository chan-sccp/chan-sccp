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
 * \date        $Date$
 * \version     $Revision$
 */

#ifndef __CHAN_SCCP_H
#define __CHAN_SCCP_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "config.h"
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <asterisk/pbx.h>
#include <asterisk/acl.h>
#include <asterisk/module.h>
#include <asterisk/rtp.h>
#include <asterisk/options.h>
#include <asterisk/logger.h>
#include <asterisk/config.h>
#include <asterisk/sched.h>
#include <asterisk/version.h>

#include "sccp_dllists.h"
//#include "sccp_conference.h"

#if ASTERISK_VERSION_NUM >= 10400
#include "asterisk/abstract_jb.h"
#endif

#ifdef CS_AST_HAS_ENDIAN
#include <asterisk/endian.h>
#endif


/* only trunk version has AST_CAUSE_ANSWERED_ELSEWHERE */
#ifndef AST_CAUSE_ANSWERED_ELSEWHERE
#define AST_CAUSE_ANSWERED_ELSEWHERE 200
#endif

#define SCCP_LITTLE_ENDIAN   1234 /* byte 0 is least significant (i386) */
#define SCCP_BIG_ENDIAN      4321 /* byte 0 is most significant (mc68k) */

#if defined( __alpha__ ) || defined( __alpha ) || defined( i386 )       ||   \
    defined( __i386__ )  || defined( _M_I86 )  || defined( _M_IX86 )    ||   \
    defined( __OS2__ )   || defined( sun386 )  || defined( __TURBOC__ ) ||   \
    defined( vax )       || defined( vms )     || defined( VMS )        ||   \
    defined( __VMS )

#define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN

#endif

#if defined( AMIGA )    || defined( applec )  || defined( __AS400__ )  ||   \
    defined( _CRAY )    || defined( __hppa )  || defined( __hp9000 )   ||   \
    defined( ibm370 )   || defined( mc68000 ) || defined( m68k )       ||   \
    defined( __MRC__ )  || defined( __MVS__ ) || defined( __MWERKS__ ) ||   \
    defined( sparc )    || defined( __sparc)  || defined( SYMANTEC_C ) ||   \
    defined( __TANDEM ) || defined( THINK_C ) || defined( __VMCMS__ )

#define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN

#endif

/*  if the platform is still not known, try to find its byte order  */
/*  from commonly used definitions in the headers included earlier  */

#if !defined(SCCP_PLATFORM_BYTE_ORDER)

#if defined(LITTLE_ENDIAN) || defined(BIG_ENDIAN)
#  if    defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif !defined(LITTLE_ENDIAN) &&  defined(BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  elif defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  endif


#elif defined(_LITTLE_ENDIAN) || defined(_BIG_ENDIAN)
#  if    defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif !defined(_LITTLE_ENDIAN) &&  defined(_BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  elif defined(_BYTE_ORDER) && (_BYTE_ORDER == _LITTLE_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif defined(_BYTE_ORDER) && (_BYTE_ORDER == _BIG_ENDIAN)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  endif

#elif defined(__LITTLE_ENDIAN__) || defined(__BIG_ENDIAN__)
#  if    defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif !defined(__LITTLE_ENDIAN__) &&  defined(__BIG_ENDIAN__)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __LITTLE_ENDIAN__)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN
#  elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __BIG_ENDIAN__)
#    define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN
#  endif
#endif

#endif

/* #define SCCP_PLATFORM_BYTE_ORDER SCCP_LITTLE_ENDIAN */
/* #define SCCP_PLATFORM_BYTE_ORDER SCCP_BIG_ENDIAN */

#if !defined(SCCP_PLATFORM_BYTE_ORDER)
#error Please edit chan_sccp.h (line 100 or 101) to set the platform byte order
#endif

#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
#define letohl(x) (x)
#define letohs(x) (x)
#define htolel(x) (x)
#define htoles(x) (x)
#else
static inline unsigned short bswap_16(unsigned short x) {
  return (x>>8) | (x<<8);
}

static inline unsigned int bswap_32(unsigned int x) {
  return (bswap_16(x&0xffff)<<16) | (bswap_16(x>>16));
}
/*
static inline unsigned long long bswap_64(unsigned long long x) {
  return (((unsigned long long)bswap_32(x&0xffffffffull))<<32) | (bswap_32(x>>32));
}
*/
#define letohl(x) bswap_32(x)
#define letohs(x) bswap_16(x)
#define htolel(x) bswap_32(x)
#define htoles(x) bswap_16(x)
#endif

#define SCCP_TECHTYPE_STR "SCCP"

/* Versioning */
#ifndef SCCP_VERSION
#define SCCP_VERSION "custom"
#endif

#ifndef SCCP_BRANCH
#define SCCP_BRANCH "trunk"
#endif

#define SCCP_LOCK_TRIES 10
#define SCCP_LOCK_USLEEP 100

/*! \todo I don't like the -1 returned value */
#define sccp_true(x) (ast_true(x) ? 1 : 0)
#define sccp_log(x) if ((sccp_globals->debug & x) == x)  ast_verbose
#define GLOB(x) sccp_globals->x

/* macro for memory alloc and free*/
#define sccp_alloc(x)	ast_alloc(x)
#define sccp_free(x){ \
	ast_free( x ); \
	(x) = NULL; \
}
/* */

#define SCCP_FILE_VERSION(file, version) \
	static void __attribute__((constructor)) __register_file_version(void) \
	{ \
		ast_register_file_version(file, version); \
	} \
	static void __attribute__((destructor)) __unregister_file_version(void) \
	{ \
		ast_unregister_file_version(file); \
	}

#define DEV_ID_LOG(x) x ? x->id : "SCCP"

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT(x) x->tech_pvt
#else
#define CS_AST_CHANNEL_PVT(x) x->pvt->pvt
#endif

#ifdef CS_AST_HAS_BRIDGED_CHANNEL
#define CS_AST_BRIDGED_CHANNEL(x) ast_bridged_channel(x)
#else
#define CS_AST_BRIDGED_CHANNEL(x) x->bridge
#endif

#ifndef CS_AST_HAS_AST_GROUP_T
typedef unsigned int ast_group_t;
#endif

extern struct ast_frame 			sccp_null_frame;			/*!< Asterisk Frame Structure */

typedef struct sccp_channel			sccp_channel_t;				/*!< SCCP Channel Structure */
typedef struct sccp_session			sccp_session_t;				/*!< SCCP Session Structure */
typedef struct sccp_line			sccp_line_t;				/*!< SCCP Line Structure */
typedef struct sccp_speed			sccp_speed_t;				/*!< SCCP Speed Structure */
typedef struct sccp_service			sccp_service_t;				/*!< SCCP Service Structure */
typedef struct sccp_device			sccp_device_t;				/*!< SCCP Device Structure */
typedef struct sccp_addon			sccp_addon_t; 				/*!< SCCP Add-On Structure */	// Added on SVN 327 -FS
typedef struct sccp_hint			sccp_hint_t;				/*!< SCCP Hint Structure */
typedef struct sccp_hostname			sccp_hostname_t;			/*!< SCCP HostName Structure */
typedef struct sccp_selectedchannel		sccp_selectedchannel_t;			/*!< SCCP Selected Channel Structure */
typedef struct sccp_ast_channel_name		sccp_ast_channel_name_t;		/*!< SCCP Asterisk Channel Name Structure */
typedef struct sccp_buttonconfig		sccp_buttonconfig_t;			/*!< SCCP Button Config Structure */
typedef struct sccp_hotline			sccp_hotline_t;				/*!< SCCP Hotline Structure */
typedef enum { FALSE=0, TRUE=1 } 		boolean_t;				/*!< Asterisk Reverses True and False; nice !! */
typedef enum {ON, OFF} 				light_t;				/*!< Enum Light Status */

typedef void sk_func (sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
typedef enum { LINE, SPEEDDIAL, SERVICE, FEATURE, EMPTY } button_type_t;		/*!< Enum Button Type */
typedef enum { ANSWER_LAST_FIRST=1, ANSWER_OLDEST_FIRST=2 } call_answer_order_t;	/*!< Enum Call Answer Order */


/*!
 * \brief SCCP ButtonType Structure
 */
static const struct sccp_buttontype {
	button_type_t buttontype;
	const char * const text;
} sccp_buttontypes[] = {
	{ LINE,		"LINE" },
	{ SPEEDDIAL,	"SPEEDDIAL" },
	{ SERVICE,	"SERVICE" },
	{ FEATURE,	"FEATURE" },
	{ EMPTY,	"EMPTY" }
};

struct sccp_conference;


/*!
 * \brief SCCP Debug Category Enum
 */
typedef enum {
	DEBUGCAT_CORE				= 1,
	DEBUGCAT_SCCP				= 1 << 1,
	DEBUGCAT_HINT 				= 1 << 2,
	DEBUGCAT_RTP 				= 1 << 3,
	DEBUGCAT_DEVICE				= 1 << 4,
	DEBUGCAT_LINE				= 1 << 5,
	DEBUGCAT_ACTION				= 1 << 6,
	DEBUGCAT_CHANNEL			= 1 << 7,
	DEBUGCAT_CLI				= 1 << 8,
	DEBUGCAT_CONFIG				= 1 << 9,
	DEBUGCAT_FEATURE			= 1 << 10,
	DEBUGCAT_FEATURE_BUTTON			= 1 << 11,
	DEBUGCAT_SOFTKEY			= 1 << 12,
	DEBUGCAT_INDICATE			= 1 << 13,
	DEBUGCAT_PBX				= 1 << 14,
	DEBUGCAT_SOCKET				= 1 << 15,
	DEBUGCAT_MWI 				= 1 << 16,
	DEBUGCAT_EVENT 				= 1 << 17,
	DEBUGCAT_ADV_FEATURE			= 1 << 18,
	DEBUGCAT_CONFERENCE			= 1 << 19,
	DEBUGCAT_BUTTONTEMPLATE			= 1 << 20,
	DEBUGCAT_SPEEDDIAL			= 1 << 21,
	DEBUGCAT_CODEC				= 1 << 22,
	DEBUGCAT_REALTIME			= 1 << 22,
	DEBUGCAT_LOCK				= 1 << 23,
	DEBUGCAT_MESSAGE			= 1 << 24,
	DEBUGCAT_NEWCODE			= 1 << 25,
	DEBUGCAT_HIGH				= 1 << 26,
} sccp_debug_category_t;									/*!< SCCP Debug Category Enum */

/*!
 * \brief SCCP Verbose Level Structure
 */
static const struct sccp_debug_category {
	const char * const short_name;
	sccp_debug_category_t category;
	const char * const text;
} sccp_debug_categories[] = {
  { "core",		DEBUGCAT_CORE,		"core debug level"		},
  { "sccp",		DEBUGCAT_SCCP,		"sccp debug level"		},
  { "hint", 		DEBUGCAT_HINT, 		"hint debug level"		},
  { "rtp",		DEBUGCAT_RTP,		"rtp debug level"		},
  { "device",		DEBUGCAT_DEVICE,	"device debug level"		},
  { "line",		DEBUGCAT_LINE,		"line debug level"		},
  { "action",		DEBUGCAT_ACTION,	"action debug level"		},
  { "channel",		DEBUGCAT_CHANNEL,	"channel debug level"		},
  { "cli",		DEBUGCAT_CLI,		"cli debug level"		},
  { "config",		DEBUGCAT_CONFIG,	"config debug level"		},
  { "feature",		DEBUGCAT_FEATURE,	"feature debug level"		},
  { "feature_button",	DEBUGCAT_FEATURE_BUTTON,"feature_button debug level"	},
  { "softkey",		DEBUGCAT_SOFTKEY,	"softkey debug level"		},
  { "indicate",		DEBUGCAT_INDICATE,	"indicate debug level"		},
  { "pbx",		DEBUGCAT_PBX,		"pbx debug level"		},
  { "socket",		DEBUGCAT_SOCKET,	"socket debug level"		},
  { "mwi",		DEBUGCAT_MWI,		"mwi debug level"		},
  { "event",		DEBUGCAT_EVENT,		"event debug level"		},
  { "adv_feature",	DEBUGCAT_ADV_FEATURE,	"adv_feature debug level"	},
  { "conference",	DEBUGCAT_CONFERENCE,	"conference debug level"	},
  { "buttontemplate",	DEBUGCAT_BUTTONTEMPLATE,"buttontemplate debug level"	},
  { "speeddial",	DEBUGCAT_SPEEDDIAL,	"speeddial debug level"		},
  { "codec",		DEBUGCAT_CODEC,		"codec debug level"		},
  { "realtime",		DEBUGCAT_REALTIME,	"realtime debug level"		},
  { "lock",		DEBUGCAT_LOCK,		"lock debug level"		},
  { "message",		DEBUGCAT_MESSAGE,	"message debug level"		},
  { "newcode",		DEBUGCAT_NEWCODE,	"newcode debug level"		},
  { "high",		DEBUGCAT_HIGH,		"high debug level"		},
};

/*!
 * \brief Feature Type Enum
 */
typedef enum {
	SCCP_FEATURE_UNKNOWN,
	SCCP_FEATURE_CFWDALL,
	SCCP_FEATURE_CFWDBUSY,
	SCCP_FEATURE_CFWDNOANSWER,
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
	SCCP_FEATURE_TESTJ
} sccp_feature_type_t;									/*!< Feature Type */

/*!
 * \brief SCCP Feature Configuration Structure
 */

typedef struct {
	boolean_t				enabled;				/*!< Feature Enabled */
	boolean_t				initialized;				/*!< Feature Enabled */
	uint32_t				status;					/*!< Feature State */
	char					configOptions[255];			/*!< space for config values */
} sccp_featureConfiguration_t;								/*!< SCCP Feature Configuration */


/*!
 * \brief Reading Type Enum
 */
typedef enum {
	SCCP_CONFIG_READINITIAL,
	SCCP_CONFIG_READRELOAD
}sccp_readingtype_t;									/*!< Reading Type */

/*!
 * \brief SCCP Mailbox Type Definition
 */
typedef struct sccp_mailbox sccp_mailbox_t;

/*!
 * \brief SCCP Mailbox Structure
 */
struct sccp_mailbox {
	char 					* mailbox;				/*!< Mailbox */
	char 					* context;				/*!< Context */
	SCCP_LIST_ENTRY(sccp_mailbox_t)		list;					/*!< Mailbox Linked List Entry */
};											/*!< SCCP Mailbox Structure */

#include "sccp_protocol.h"


/*!
 * \brief Privacy Definition
 */
#define SCCP_PRIVACYFEATURE_HINT 		1 << 1;
#define SCCP_PRIVACYFEATURE_CALLPRESENT		1 << 2;

/*!
 * \brief SCCP Currently Selected Channel Structure
 */

struct sccp_selectedchannel {
	sccp_channel_t 				* channel;				/*!< SCCP Channel */
	SCCP_LIST_ENTRY(sccp_selectedchannel_t) list;					/*!< Selected Channel Linked List Entry */
};											/*!< SCCP Selected Channel Structure */



/*!
 * \brief SCCP cfwd information
 */
typedef struct sccp_cfwd_information		sccp_cfwd_information_t;		/*!< cfwd information */

/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information{
	boolean_t				enabled;
	char 					number[AST_MAX_EXTENSION];
};



/*!
 * \brief for addressing individual devices on shared line
 */
struct  subscriptionId{
	char					number[AST_MAX_EXTENSION];		/*!< will be added to cid */
	char					name[AST_MAX_EXTENSION];		/*!< will be added to cidName */
};

/*!
 * \brief string identifier with additional subscription id
 */
struct  composedId{
	char   mainId[StationMaxServiceURLSize];
	struct subscriptionId subscriptionId;
};



/*!
 * \brief SCCP Line-Devices Structure
 */
typedef struct sccp_linedevices			sccp_linedevices_t;			/*!< SCCP Line Connected to Devices */

/*!
 * \brief SCCP Line-Devices Structure
 */
struct sccp_linedevices {
	sccp_device_t 				* device;				/*!< SCCP Device */
	sccp_cfwd_information_t			cfwdAll;				/*!< cfwd information */
	sccp_cfwd_information_t			cfwdBusy;				/*!< cfwd information */

	struct  subscriptionId			subscriptionId;				/*!< for addressing individual devices on shared line */
	char                                    label[80];                              /*!<  */
	SCCP_LIST_ENTRY(sccp_linedevices_t) 	list;					/*!< Device Linked List Entry */
};											/*!< SCCP Line-Device Structure */


/*!
 * \brief SCCP Button Configuration Structure
 */

struct sccp_buttonconfig {
	uint16_t				instance;				/*!< Instance on device */
	uint16_t				index;					/*!< buttonconfig index */
	button_type_t 				type;					/*!< Button type (e.g. line, speeddial, feature, empty) */
	SCCP_LIST_ENTRY(sccp_buttonconfig_t) 	list;					/*!< Button Linked List Entry */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;
	unsigned int				pendingUpdate:1;
#endif

	/*!
	 * \brief SCCP Button Structure
	 */
	union sccp_button {

		/*!
		 * \brief SCCP Button Line Structure
		 */

		struct {
			char 			name[80];				/*!< Button Name */
			struct subscriptionId 	subscriptionId;
			char			options[256];
		} line;									/*!< SCCP Button Line Structure */
		/*!
		 * \brief SCCP Button Speeddial Structure
		 */

		struct sccp_speeddial {
			char 			label[StationMaxNameSize];		/*!< SpeedDial Label */
			char 			ext[AST_MAX_EXTENSION];			/*!< SpeedDial Extension */
			char 			hint[AST_MAX_EXTENSION];		/*!< SpeedDIal Hint */
		} speeddial;								/*!< SCCP Button Speeddial Structure */

		/*!
		 * \brief SCCP Button Service Structure
		 */

		struct sccp_service {
			char 			label[StationMaxNameSize];		/*!< The label of the serviceURL button */
			char 			url[StationMaxServiceURLSize];		/*!< The number to dial when it's hit */
		} service;								/*!< SCCP Button Service Structure  */
		/*!
		 * \brief SCCP Button Feature Structure
		 */

		struct sccp_feature {
			uint8_t 		index;					/*!< Button Feature Index */
			sccp_feature_type_t 	id;					/*!< Button Feature ID */
			char 			label[StationMaxNameSize];		/*!< Button Feature Label */
			char 			options[254];				/*!< Button Feature Options */
			uint32_t 		status;					/*!< Button Feature Status */
		} feature;								/*!< SCCP Button Feature Structure */
	} button;									/*!< SCCP Button Structure */
};											/*!< SCCP Button Configuration Structure */

/*!
 * \brief SCCP Hostname Structure
 */
struct sccp_hostname {
	char 					name[MAXHOSTNAMELEN];			/*!< Name of the Host */
	SCCP_LIST_ENTRY(sccp_hostname_t) 	list;					/*!< Host Linked List Entry */
};											/*!< SCCP Hostname Structure */

/*!
 * \brief SCCP Line Structure
 * \note A line is the equivalent of a 'phone line' going to the phone.
 */
struct sccp_line {
	ast_mutex_t 				lock;					/*!< Asterisk: Lock Me Up and Tie me Down */
	char 					id[4];					/*!< This line's ID, used for logging into (for mobility) */
	char 					pin[8];					/*!< PIN number for mobility/roaming. */
	char 					name[80];				/*!< The name of the line, so use in asterisk (i.e SCCP/[name]) */
	char 					description[StationMaxNameSize];	/*!< A description for the line, displayed on in header (on7960/40) or on main  screen on 7910 */
	char 					label[StationMaxNameSize];		/*!< A name for the line, displayed next to the button (7960/40). */

	SCCP_LIST_HEAD(, sccp_mailbox_t) 	mailboxes;				/*!< Mailbox Linked List Entry. To check for messages */
	char 					vmnum[AST_MAX_EXTENSION];		/*!< Voicemail number to Dial */
	unsigned int 				meetme:1;				/*!< Meetme on/off */
	char 					meetmenum[AST_MAX_EXTENSION];		/*!< Meetme Extension to be Dialed (\todo TO BE REMOVED) */
	char 					meetmeopts[AST_MAX_CONTEXT];		/*!< Meetme Options to be Used*/
	char 					context[AST_MAX_CONTEXT];		/*!< The context we use for Outgoing Calls. */
	char 					language[MAX_LANGUAGE];			/*!< language we use for calls */
	char 					accountcode[SCCP_MAX_ACCOUNT_CODE];	/*!< accountcode used in cdr */
	char 					musicclass[MAX_MUSICCLASS];		/*!< musicclass assigned when getting moh */
	int					amaflags;				/*!< amaflags */
	ast_group_t				callgroup;				/*!< callgroups assigned (seperated by commas) to this lines */
#ifdef CS_SCCP_PICKUP
	ast_group_t				pickupgroup;				/*!< pickupgroup assigned to this line */
#endif
	char 					cid_name[AST_MAX_EXTENSION];		/*!< Caller(Name) to use on outgoing calls*/
	char 					cid_num[AST_MAX_EXTENSION];		/*!< Caller(ID) to use on outgoing calls  */
	uint8_t 				incominglimit;				/*!< max incoming calls limit */
	unsigned int				audio_tos;				/*!< audio stream type_of_service (TOS) (RTP) */
	unsigned int				video_tos;				/*!< video stream type_of_service (TOS) (VRTP) */
	unsigned int				audio_cos;				/*!< audio stream class_of_service (COS) (VRTP) */
	unsigned int				video_cos;				/*!< video stream class_of_service (COS) (VRTP) */
/* 	sccp_channel_t 				* activeChannel; */			/* The currently active channel. */
	SCCP_LIST_HEAD(, sccp_channel_t) 	channels;				/*!< Linked list of current channels for this line */
	uint8_t 				channelCount;				/*!< Number of currently active channels */
	SCCP_LIST_ENTRY(sccp_line_t) 		list;					/*!< global list entry */
/* 	sccp_device_t * 			device;*/				/* The device this line is currently registered to. */
	SCCP_LIST_HEAD(,sccp_linedevices_t)	devices;				/*!< The device this line is currently registered to. */
	//uint8_t 				cfwd_type;				/*!< Call Forward Type (SCCP_CFWD_ALL or SCCP_CFWD_BUSY0 */
	//char 					* cfwd_num;				/*!< call forward Number*/
	char 					* trnsfvm;				/*!< transfer to voicemail softkey. Basically a call forward */
	char 					secondary_dialtone_digits[10];		/*!< secondary dialtone digits*/
	uint8_t					secondary_dialtone_tone;		/*!< secondary dialtone tone */

	unsigned int				echocancel: 1;				/*!< echocancel phone support */
	unsigned int				silencesuppression:1;			/*!< Silence Suppression Phone Support */
	unsigned int				transfer: 1;				/*!< Transfer Phone Support */
	unsigned int				spareBit4: 1;				/*!< SpareBit4 */
	unsigned int				spareBit5: 1;				/*!< SpareBit5 */
	unsigned int				spareBit6: 1;				/*!< SpareBit6 */
#ifdef CS_SCCP_REALTIME
	boolean_t				realtime:1;				/*!< is it a realtimeconfiguration*/
#endif
	struct ast_variable			* variables;				/*!< Channel variables to set */
	unsigned int				dnd: 3;					/*!< dnd on line */
	uint8_t 				dndmode;				/*!< dnd mode: see SCCP_DNDMODE_* */
	struct  subscriptionId			defaultSubscriptionId;     /*!< default subscription id for shared lines */

#ifdef CS_DYNAMIC_CONFIG
	/* this is for reload routines */
	unsigned int				pendingDelete: 1;			/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int				pendingUpdate:1;			/*!< this bit will tell the scheduler to update this line when unused */
#endif


	/*!
	 * \brief Statistic for Line Structure
	 */
	struct {
		uint8_t				numberOfActiveDevices;			/*!< Number of Active Devices */
		uint8_t				numberOfActiveChannels;			/*!< Number of Active Channels */
		uint8_t				numberOfHoldChannels;			/*!< Number of Hold Channels */
		uint8_t 			numberOfDNDDevices;			/*!< Number of DND Devices */
	} statistic;									/*!< Statistics for Line Structure */

	/*!
	 * \brief VoiceMail Statistics Structure
	 */
	struct {
		int				newmsgs;				/*!< New Messages */
		int				oldmsgs;				/*!< Old Messages */
	} voicemailStatistic;								/*!< VoiceMail Statistics Structure */

	uint32_t				configurationStatus;			/*!< what is the current configuration status - @see sccp_config_status_t */
	char 					adhocNumber[AST_MAX_EXTENSION];		/*!< number that should be dialed when device offhocks this line */

};											/*!< SCCP Line Structure */

/*!
 * \brief SCCP SpeedDial Button Structure
 */
struct sccp_speed {
	uint8_t 				config_instance;			/*!< The instance of the speeddial in the sccp.conf */
	uint16_t 				instance;				/*!< The instance on the current device */
	uint8_t 				type;					/*!< SpeedDial Button Type (SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE (hint)) */
	char 					name[StationMaxNameSize];		/*!< The name of the speed dial button */
	char 					ext[AST_MAX_EXTENSION];			/*!< The number to dial when it's hit */
	char 					hint[AST_MAX_EXTENSION];		/*!< The HINT on this SpeedDial */

	SCCP_LIST_ENTRY(sccp_speed_t) 		list;					/*!< SpeedDial Linked List Entry */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;			/*!< this bit will tell the scheduler to delete this speed when unused */
	unsigned int				pendingUpdate:1;			/*!< this bit will tell the scheduler to update this speed when unused */
#endif

};

/*!
 * \brief SCCP Device Structure
 */
struct sccp_device {
	ast_mutex_t 				lock;					/*!< Asterisk: Lock Me Up and Tie me Down */
	char 					id[StationMaxDeviceNameSize];		/*!< SEP<macAddress> of the device. */
	char 					description[40];			/*!< Internal Description. Skinny protocol does not use it */
	char 					config_type[10];			/*!< Model of this Phone used for setting up features/softkeys/buttons etc. */
	uint32_t 				skinny_type;				/*!< Model of this Phone sent by the station, devicetype*/
	int 					tz_offset;				/*!< Timezone OffSet */
	char 					imageversion[StationMaxVersionSize];	/*!< Version to Send to the phone */
	uint8_t 				accessoryused;				/*!< Accessory Used. This are for support of message 0x0073 AccessoryStatusMessage - Protocol v.11 CCM7 -FS */
	uint8_t 				accessorystatus;			/*!< Accessory Status */
	uint8_t 				registrationState;			/*!< If the device has been fully registered yet */
	struct ast_codec_pref 			codecs;					/*!< Asterisk Codec Device Preference */
	sccp_devicestate_t 			state;					/*!< Device State (SCCP_DEVICE_ONHOOK or SCCP_DEVICE_OFFHOOK) */
/*      uint8_t 				ringermode;*/				/* Ringer Mode. Need it for the ringback */

	char 					lastNumber[AST_MAX_EXTENSION];		/*!< Last Dialed Number */
	int 					capability;				/*!< Asterisk Codec Capability */
	uint8_t 				earlyrtp;				/*!< RTP Channel State where to open the RTP Media Stream */
	uint8_t 				channelCount;				/*!< Number of Currently Active Channels */
	uint8_t 				protocolversion;			/*!< Skinny Supported Protocol Version */
	uint8_t 				inuseprotocolversion;			/*!< Skinny Used Protocol Version */
	int					keepalive;				/*!< Station Specific Keepalive Timeout */

	struct ast_ha				*ha;					/*!< Permit or Deny Connections to the Main Socket */
	uint32_t				conferenceid;				/*!< Conference ID */
	unsigned int 				meetme:1;				/*!< Meetme on/off */
	char 					meetmeopts[AST_MAX_CONTEXT];		/*!< Meetme Options to be Used*/

	unsigned int				mwilamp: 3;				/*!< MWI/Lamp to indicate MailBox Messages */
	unsigned int				mwioncall: 1;				/*!< MWI On Call Support (Boolean, default=on) */
	unsigned int				softkeysupport: 1;			/*!< Soft Key Support (Boolean, default=on) */
	unsigned int				mwilight: 1;				/*!< MWI/Light Support (Boolean, default=on) \todo MWI/Light or Lamp was soll es sein */

	unsigned int				transfer: 1;				/*!< Transfer Support (Boolean, default=on) */
	//unsigned int				conference: 1;				/*!< Conference Support (Boolean, default=on) */
	unsigned int				park: 1;				/*!< Park Support (Boolean, default=on) */
	unsigned int				cfwdall: 1;				/*!< Call Forward All Support (Boolean, default=on) */
	unsigned int				cfwdbusy: 1;				/*!< Call Forward on Busy Support (Boolean, default=on) */
	unsigned int				cfwdnoanswer: 1;			/*!< Call Forward on No-Answer Support (Boolean, default=on) */

#ifdef CS_SCCP_PICKUP
	unsigned int				pickupexten: 1;				/*!< Pickup Extension Support (Boolean, default=on) */
	char					* pickupcontext;			/*!< Pickup Context to Use in DialPlan */
	unsigned int				pickupmodeanswer: 1;			/*!< Pickup Mode Answer */
#endif
	unsigned int				dtmfmode: 1; 				/*!< DTMF Mode (0 inband - 1 outofband) */
	unsigned int				nat: 1;					/*!< Network Address Translation Support (Boolean, default=on) */
	unsigned int				directrtp: 1;				/*!< Direct RTP Support (Boolean, default=on) */
	unsigned int				trustphoneip: 1;			/*!< Trust Phone IP Support (Boolean, default=on) */
	unsigned int				needcheckringback: 1;			/*!< Need to Check Ring Back Support (Boolean, default=on) */

	boolean_t				realtime;				/*!< is it a realtime configuration*/
	sccp_channel_t   			* active_channel;			/*!< Active SCCP Channel */
	sccp_channel_t   			* transfer_channel; 			/*!< SCCP Channel being Transfered */
	sccp_channel_t				* conference_channel;			/*!< SCCP Channel which is going to be Conferenced */
	sccp_line_t      			* currentLine;				/*!< Current Line */
	sccp_session_t   			* session;				/*!< Current Session */
/*	SCCP_LIST_ENTRY(sccp_linedevices_t) 	linedevicelist;*/			/* Line-Device Linked List */
	SCCP_LIST_ENTRY(sccp_device_t) 		list;					/*!< Global Device Lined List */
	//SCCP_LIST_HEAD(,sccp_hint_t) 		hints;					/*!< list of hint pointers. Internal lines to notify the state */
	SCCP_LIST_HEAD(,sccp_buttonconfig_t) 	buttonconfig;				/*!< SCCP Button Config Attached to this Device */
	uint8_t					linesCount;				/*!< Number of Lines */
	SCCP_LIST_HEAD(,sccp_selectedchannel_t)	selectedChannels;			/*!< Selected Channel List */
	SCCP_LIST_HEAD(,sccp_addon_t) 		addons;					/*!< Add-Ons connect to this Device */
	SCCP_LIST_HEAD(,sccp_hostname_t)	permithosts;				/*!< Permit Registration to the Hostname/IP Address */

	pthread_t				postregistration_thread;		/*!< Post Registration Thread */

	struct ast_variable 			* variables;				/*!< Channel variables to set */
	char 					* phonemessage;				/*!< Message to display on device*/
	uint8_t					defaultLineInstance;			/*!< Default Line Instance */

	struct {
		boolean_t			headset;				/*!< HeadSet Support (Boolean) */
		boolean_t			handset;				/*!< HandSet Support (Boolean) */
		boolean_t			speaker;				/*!< Speaker Support (Boolean) */
	} accessoryStatus;								/*!< Accesory Status Structure */

	struct {
		uint8_t				numberOfLines;				/*!< Number of Lines */
		uint8_t				numberOfSpeeddials;			/*!< Number of SpeedDials */
		uint8_t				numberOfFeatures;			/*!< Number of Features */
		uint8_t				numberOfServices;			/*!< Number of Services */
	} configurationStatistic;							/*!< Configuration Statistic Structure */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;			/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int				pendingUpdate:1;			/*!< this will contain the updated line struct once reloaded from config to update the line when unused */

	//SCCP_LIST_ENTRY(sccp_device_t) 		list;
#endif

	boolean_t 				isAnonymous;				/*!< Device is connected Anonymously (Guest) */
	light_t					mwiLight;				/*!< MWI/Light \todo third MWI/light entry in device ? */

	struct {
		int				newmsgs;				/*!< New Messages */
		int				oldmsgs;				/*!< Old Messages */
	}voicemailStatistic;								/*!< VoiceMail Statistics */

	/* feature configurations */
	sccp_featureConfiguration_t 		privacyFeature;				/*!< Device Privacy Feature. \see SCCP_PRIVACYFEATURE_* */
	sccp_featureConfiguration_t 		overlapFeature;				/*!< Overlap Dial Feature */
	sccp_featureConfiguration_t 		monitorFeature;				/*!< Monitor (automon) Feature */
	sccp_featureConfiguration_t 		dndFeature;				/*!< dnd Feature */
	sccp_featureConfiguration_t 		priFeature;				/*!< priority Feature */
	sccp_featureConfiguration_t 		mobFeature;				/*!< priority Feature */


	char 					softkeyDefinition[50];			/*!< requested softKey configuration */

	struct{
		softkey_modes			*modes;					/*!< used softkeySet */
		uint8_t				size;					/*!< who many softkeysets are provided by modes */
	}softKeyConfiguration;								/*!< SoftKeySet configuration */


	struct{
		int 				free;
	}scheduleTasks;

	char 					videoSink[MAXHOSTNAMELEN];		/*!< sink to send video*/
	struct sccp_conference			*conference;				/*!< conference we are part of */

	btnlist 				*buttonTemplate;

#ifdef CS_ADV_FEATURES
	boolean_t				useRedialMenu;
#endif
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
	int 					type;					/*!< Add-On Type */
	SCCP_LIST_ENTRY(sccp_addon_t) 		list;					/*!< Linked List Entry for this Add-On */
	sccp_device_t * 			device;					/*!< Device Associated with this Add-On */

#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;			/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int				pendingUpdate:1;			/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
};

/*!
 * \brief SCCP Session Structure
 * \note This contains the current session the phone is in
 */
struct sccp_session {
	ast_mutex_t				lock;					/*!< Asterisk: Lock Me Up and Tie me Down */
	void					* buffer;				/*!< Session Buffer */
	int32_t					buffer_size;				/*!< Session Buffer Size */

	struct sockaddr_in			sin;					/*!< Socket In */

	struct in_addr				ourip;					/*!< Our IP is for rtp use */
	time_t					lastKeepAlive;				/*!< Last KeepAlive Time */
	int					fd;					/*!< File Descriptor */
	int					rtpPort;				/*!< RTP Port */
	sccp_device_t 				* device;				/*!< Associated Device */
	SCCP_LIST_ENTRY(sccp_session_t) 	list;					/*!< Linked List Entry for this Session */

	unsigned int				needcheckringback: 1;			/*!< Need Check Ring Back. (0/1) default 1 */
};											/*!< SCCP Sesson Structure */								/*!< SCCP Session Structure */

/*!
 * \brief SCCP Channel Structure
 * \note This contains the current channel information
 */
struct sccp_channel {
	ast_mutex_t				lock;					/*!< Asterisk: Lock Me Up and Tie me Down */
	int					format;					/*!< Codec requested by Asterisk */
	boolean_t				isCodecFix;				/*!< can we change codec */

	struct ast_codec_pref 			codecs;					/*!< Asterisk Codec Channel Preference */
	int 					capability;				/*!< channel Capability */

	char					calledPartyName[StationMaxNameSize];	/*!< Called Party Name */
	char					calledPartyNumber[StationMaxDirnumSize];/*!< Called Party Number */
	char					callingPartyName[StationMaxNameSize];	/*!< Calling Party Name */
	char					callingPartyNumber[StationMaxDirnumSize];/*!< Calling Party Number */

#ifdef CS_ADV_FEATURES
        char 					originalCalledPartyName[StationMaxNameSize];	/*!< Original Calling Party Name */
        char					originalCalledParty[StationMaxDirnumSize];	/*!< Original Calling Party ID */
        char					lastRedirectingPartyName[StationMaxNameSize];	/*!< Original Called Party Name */
        char					lastRedirectingParty[StationMaxDirnumSize];	/*!< Original Called Party ID */
        uint32_t				originalCdpnRedirectReason;			/*!< Original Called Party Redirect Reason */
        uint32_t				lastRedirectingReason;				/*!< Last Redirecting Reason */
        char					cgpnVoiceMailbox[StationMaxDirnumSize];		/*!< Calling Party Voicemail Box */
        char					cdpnVoiceMailbox[StationMaxDirnumSize];		/*!< Called Party Voicemail Box */
        char					originalCdpnVoiceMailbox[StationMaxDirnumSize];	/*!< Original Called Party VoiceMail Box */
        char					lastRedirectingVoiceMailbox[StationMaxDirnumSize];/*!< Last Redirecting VoiceMail Box */
#endif

	uint32_t				callid;					/*!< Call ID */
	uint32_t				passthrupartyid;			/*!< Pass Through ID */
	//uint32_t				conferenceid; 				/*!< Conference ID. This will be used in native conferencing mode and will differ from callid  -FS*/
	uint8_t					state;					/*!< Internal channel state SCCP_CHANNELSTATE_* */
	uint8_t					previousChannelState;			/*!< Previous channel state SCCP_CHANNELSTATE_* */
	skinny_calltype_t			calltype;				/*!< Skinny Call Type as SKINNY_CALLTYPE_* */
	int					digittimeout;				/*!< Scheduler Timeout on Dialing State */
	uint8_t					ringermode;				/*!< Ringer Mode */

	char 					dialedNumber[AST_MAX_EXTENSION];	/*!< Last Dialed Number */
	sccp_device_t 	 			* device;				/*!< SCCP Device */

	struct ast_channel 	 		* owner;				/*!< Asterisk Channel Owner */
	sccp_line_t		 		* line;					/*!< SCCP Line */

	struct{
		struct ast_rtp	 		*audio;					/*!< Asterisk RTP */
		struct ast_rtp 			*video;					/*!< Video RTP session */
	}rtp;

	struct sockaddr_in			rtp_addr;				/*!< RTP Socket Address */
	struct sockaddr_in			rtp_peer;				/*!< RTP Socket Address */
	struct sockaddr_in			vrtp_addr;				/*!< VRTP Socket Address */
	struct sockaddr_in			vrtp_peer;				/*!< VRTP Socket Address */
	SCCP_LIST_ENTRY(sccp_channel_t) 	list;					/*!< Channel Linked List List */
	uint8_t					autoanswer_type;			/*!< Auto Answer Type */
	uint8_t					autoanswer_cause;			/*!< Auto Answer Cause */
	boolean_t				answered_elsewhere;			/*!< Answered Elsewhere */

	/* don't allow sccp phones to monitor (hint) this call */
	boolean_t				privacy;				/*!< Private */
	char 					musicclass[MAX_MUSICCLASS];		/*!< Music Class */
	uint8_t					ss_action; 				/*!< Simple Switch Action. This is used in dial thread to collect numbers for callforward, pickup and so on -FS*/
	uint8_t					ss_data; 				/*!< Simple Switch Integer param */
#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;			/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int				pendingUpdate:1;			/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif

	struct {
		boolean_t transmit;							/*!< have we oppend transmitport already */
		boolean_t receive;							/*!< have we oppend receiveport already */
	} mediaStatus;

	/* feature sets */
	boolean_t				monitorEnabled;				/*!< Monitor Enabled Feature */
	//sccp_callReason_t			reason;					/*!< what is the reaso for this call */

	struct sccp_conference			*conference;				/*!< are we part of a conference? */
	sccp_channel_t				*parentChannel;				/*!< if we are a cfwd channel, our parent is this */


	struct  subscriptionId			subscriptionId;

};											/*!< SCCP Channel Structure */

/*!
 * \brief SCCP Global Variable Structure
 */
struct sccp_global_vars {
	ast_mutex_t				lock;					/*!< Asterisk: Lock Me Up and Tie me Down */

	pthread_t 				monitor_thread;				/*!< Monitor Thread */ // ADDED IN 414 -FS
	ast_mutex_t				monitor_lock;  				/*!< Monitor Asterisk Lock */ // ADDED IN 414 -FS

	SCCP_LIST_HEAD(, sccp_session_t) 	sessions;				/*!< SCCP Sessions */
	SCCP_LIST_HEAD(, sccp_device_t) 	devices;				/*!< SCCP Devices */
	SCCP_LIST_HEAD(, sccp_line_t) 		lines;					/*!< SCCP Lines */
	ast_mutex_t				socket_lock;				/*!< Socket Lock */
	pthread_t				socket_thread; 				/*!< Socket Thread */ // Moved her in v2 SVN 426 -FS
	int 					descriptor;				/*!< Descriptor */
	int					usecnt;					/*!< Keep track of when we're in use. */
	ast_mutex_t				usecnt_lock;				/*!< Use Counter Asterisk Lock */

	char					servername[StationMaxDisplayNotifySize];/*!< ServerName */

	struct sockaddr_in			bindaddr;				/*!< Bind IP Address */
	int					ourport;				/*!< \todo *ha ? */

	struct ast_ha				* ha;					/*!< Permit or deny connections to the main socket */

	struct ast_ha				* localaddr;				/*!< Localnet for Network Address Translation */
	struct sockaddr_in			externip;				/*!< External IP Address */
	char 					externhost[MAXHOSTNAMELEN];		/*!< External HostName */
	time_t 					externexpire;				/*!< External Expire */
	int 					externrefresh;				/*!< External Refresh */

	char					context[AST_MAX_CONTEXT];		/*!< Global / General Context */
	char					language[MAX_LANGUAGE];			/*!< Language */
	char					accountcode[SCCP_MAX_ACCOUNT_CODE];	/*!< Account Code */
	char					musicclass[MAX_MUSICCLASS];		/*!< Music Class */
	/*char					mohinterpret[MAX_MUSICCLASS];*/		/*!< Music On Hold Interpret */
	int					amaflags;				/*!< AmaFlags */
	ast_group_t				callgroup;				/*!< Call Group */
#ifdef CS_SCCP_PICKUP
	ast_group_t				pickupgroup;				/*!< Pick Up Group */

	int					pickupmodeanswer: 1;			/*!< Pick Up Mode Answer */
#endif
	int					global_capability;			/*!< Global Capability */

	struct					ast_codec_pref global_codecs;		/*!< Global Asterisk Codecs */
	int					keepalive;				/*!< KeepAlive */
	uint32_t				debug;					/*!< Debug */
	char 					date_format[7];				/*!< Date Format */

	uint8_t					firstdigittimeout;			/*< First Digit Timeout. Wait up to 16 seconds for first digit */
	uint8_t					digittimeout;				/*< Digit Timeout. How long to wait for following digits */
	char					digittimeoutchar;			/*< Digit End Character. What char will force the dial (Normally '#') */

	unsigned int				recorddigittimeoutchar: 1; 		/*< Record Digit Time Out Char. Whether to include the digittimeoutchar in the call logs */

	unsigned int				sccp_tos;				/*!< SCCP Socket Type of Service (TOS) (QOS) (Signaling) */
	unsigned int				audio_tos;				/*!< Audio Socket Type of Service (TOS) (QOS) (RTP) */
	unsigned int				video_tos;				/*!< Video Socket Type of Service (TOS) (QOS) (VRTP) */
	unsigned int				sccp_cos;				/*!< SCCP Socket Class of Service (COS) (QOS) (Signaling) */
	unsigned int				audio_cos;				/*!< Audio Socket Class of Service (COS) (QOS) (RTP) */
	unsigned int				video_cos;				/*!< Video Socket Class of Service (COS) (QOS) (VRTP) */

	uint8_t					earlyrtp;				/*!< Channel State where to open the rtp media stream */

	uint8_t 				dndmode;				/*!< Do Not Disturb (DND) Mode: \see SCCP_DNDMODE_* */
	uint8_t 				protocolversion;			/*!< Skinny Protocol Version */

	/* autoanswer stuff */
	uint8_t					autoanswer_ring_time;			/*!< Auto Answer Ring Time */
	uint8_t					autoanswer_tone;			/*!< Auto Answer Tone */
	uint8_t					remotehangup_tone;			/*!< Remote Hangup Tone */
	uint8_t					transfer_tone;				/*!< Transfer Tone */
	uint8_t					callwaiting_tone;			/*!< Call Waiting Tone */

	unsigned int				mwilamp:3;				/*!< MWI/Lamp (Default:3) */
	unsigned int				mwioncall:1;				/*!< MWI On Call Support (Boolean, default=on) */
	unsigned int				echocancel:1;				/*!< Echo Canel Support (Boolean, default=on) */
	unsigned int				silencesuppression: 1;			/*!< Silence Suppression Support (Boolean, default=on)  */
	unsigned int				trustphoneip: 1;				/*!< Trust Phone IP Support (Boolean, default=on) */
//	unsigned int				private: 1; 				/*!< Permit Private Function Support (Boolean, default=on) */
	unsigned int				privacy: 2;				/*!< Privacy Support (Default=2) */
	unsigned int				blindtransferindication: 1; 		/*!< Blind Transfer Indication Support (Boolean, default=on) */
	unsigned int				cfwdall: 1;				/*!< Call Forward All Support (Boolean, default=on) */
	unsigned int				cfwdbusy: 1;				/*!< Call Forward on Busy Support (Boolean, default=on) */
	unsigned int				cfwdnoanswer: 1;				/*!< Call Forward on No-Answer Support (Boolean, default=on) */
	unsigned int				nat: 1;					/*!< Network Address Translation */
	unsigned int				directrtp: 1;				/*!< Direct RTP */
	unsigned int				useoverlap: 1;				/*!< Overlap Dial Support */
	call_answer_order_t			callAnswerOrder;			/*!< Call Answer Order */
#ifdef CS_MANAGER_EVENTS
	boolean_t				callevents;				/*!< Call Events */
#endif
#ifdef CS_SCCP_REALTIME
	char 					realtimedevicetable[45];		/*!< Database Table Name for SCCP Devices*/
	char 					realtimelinetable[45];			/*!< Database Table Name for SCCP Lines*/
#endif
	unsigned int 				meetme: 1;				/*!< Meetme on/off */
	char 					meetmeopts[AST_MAX_CONTEXT];		/*!< Meetme Options to be Used*/
#if ASTERISK_VERSION_NUM >= 10400

	struct ast_jb_conf			global_jbconf;				/*!< Global Jitter Buffer Configuration */
#endif

#ifdef CS_DYNAMIC_CONFIG
	unsigned int				pendingDelete:1;			/*!< this bit will tell the scheduler to delete this line when unused */
	unsigned int				pendingUpdate:1;			/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif

	pthread_t				mwiMonitorThread; 			/*!< MWI Monitor Thread */ // MC
	boolean_t				allowAnonymus;				/*!< Allow Anonymous/Guest Devices */
	sccp_hotline_t				* hotline;				/*!< HotLine */
};											/*!< SCCP Global Varable Structure */



/*!
 * \brief SCCP Hotline Structure
 * \note This contains the new HotLine Feature
 */
struct sccp_hotline {
	sccp_line_t				* line;					/*!< Line */
	char					exten[50];				/*!< Extension */
};											/*!< SCCP Hotline Structure */


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
#define sccp_sched_add 				ast_sched_add
#define sccp_sched_del 				ast_sched_del
#define SCCP_SCHED_DEL(sched, id) \
({ \
	int _count = 0; \
	int _sched_res = -1; \
	while (id > -1 && (_sched_res = sccp_sched_del(sched, id)) && ++_count < 10) \
		usleep(1); \
	if (_count == 10) { \
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Unable to cancel schedule ID %d.\n", id); \
	} \
	id = -1; \
	(_sched_res); \
})

extern struct sccp_global_vars 			* sccp_globals;

uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s);

#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel 				* sccp_request(const char *type, int format, void *data, int *cause);
#else
struct ast_channel 				* sccp_request(char *type, int format, void *data);
#endif

int sccp_devicestate(void *data);

#if ASTERISK_VERSION_NUM >= 10400
extern struct sched_context 				* sched;
extern struct io_context 				* io;
void 						* sccp_do_monitor(void *data); 		// ADDED IN SVN 414 -FS
int sccp_restart_monitor(void); 							// ADDED IN SVN 414 -FS
enum ast_bridge_result 				sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms);
#endif


#if ASTERISK_VERSION_NUM >= 10400
int sccp_sched_free(void *ptr);
#endif

/*!
 * \todo sccp_is_nonempty_string never used (not used)
 */
static inline unsigned char sccp_is_nonempty_string(char *string)
{
	if (NULL != string) {
		if (!ast_strlen_zero(string)) {						/*!< \todo Unrecognized identifier: ast_strlen_zero. Identifier used in code has not been declared. */
			return 1;							/*!< \todo Return value type int does not match declared type unsigned char: 1 */
		}
	}
	return 0;									/*!< \todo Return value type int does not match declared type unsigned char: 1 */
}

typedef struct softKeySetConfiguration sccp_softKeySetConfiguration_t;			/*!< SoftKeySet configuration */


/*!
 * \brief SoftKeySet Configuration Structure
 */
struct softKeySetConfiguration{
	char					name[50];				/*!< Name for this configuration */
	softkey_modes 				modes[16];				/*!< SoftKeySet modes, see KEYMODE_* */
	uint8_t					numberOfSoftKeySets;			/*!< How many SoftKeySets we definde? */
	SCCP_LIST_ENTRY(sccp_softKeySetConfiguration_t) 	list;			/*!< Next list entry */
};											/*!< SoftKeySet Configuration Structure */

/*
   struct softKeySetConfigList{
	ast_mutex_t lock;
	sccp_softKeySetConfiguration_t *first;
	sccp_softKeySetConfiguration_t *last;
	uint16_t size;
} */

SCCP_LIST_HEAD(softKeySetConfigList, sccp_softKeySetConfiguration_t);
extern struct softKeySetConfigList softKeySetConfig;			/*!< List of SoftKeySets */

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* __CHAN_SCCP_H */

