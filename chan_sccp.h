#ifndef __CHAN_SCCP_H
#define __CHAN_SCCP_H

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

#include "sccp_dllists.h"

#ifndef ASTERISK_CONF_1_2
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

/* Versioning */
#ifndef SCCP_VERSION
#define SCCP_VERSION "custom"
#endif

#ifndef SCCP_BRANCH
#define SCCP_BRANCH "trunk"
#endif

#define SCCP_LOCK_TRIES 10
#define SCCP_LOCK_USLEEP 100

/* I don't like the -1 returned value */
#define sccp_true(x) (ast_true(x) ? 1 : 0)
#define sccp_log(x) if ((!sccp_globals->fdebug && sccp_globals->debug >= x) || (sccp_globals->fdebug == x)) ast_verbose
#define GLOB(x) sccp_globals->x

/* macro for memory alloc and free*/
#define sccp_alloc(x)	ast_alloc(x)
#define sccp_free(x){ \
	ast_free( x ); \
	(x) = NULL; \
}
/* */

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

typedef struct sccp_channel			sccp_channel_t;
typedef struct sccp_session			sccp_session_t;
typedef struct sccp_line				sccp_line_t;
typedef struct sccp_speed			sccp_speed_t;
typedef struct sccp_service			sccp_service_t;
typedef struct sccp_selectedchannel	sccp_selectedchannel_t;
typedef struct sccp_device			sccp_device_t;
typedef struct sccp_addon			sccp_addon_t; // Added on SVN 327 -FS
typedef struct sccp_hint				sccp_hint_t;
typedef struct sccp_hostname			sccp_hostname_t;
typedef struct sccp_ast_channel_name	sccp_ast_channel_name_t;
typedef enum { FALSE=0, TRUE=1 } 		boolean_t;
typedef enum { LINE, SPEEDDIAL, SERVICE, FEATURE, EMPTY } button_type_t;
typedef enum { ANSWER_LAST_FIRST=1, ANSWER_OLDEST_FIRST=2 } 		call_answer_order_t;
typedef struct sccp_buttonconfig	sccp_buttonconfig_t;
typedef void sk_func (sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
typedef enum {ON, OFF} light_t;
typedef struct sccp_hotline			sccp_hotline_t;
typedef enum{
	SCCP_VERBOSE_LEVEL_HINT = 1,
	SCCP_VERBOSE_LEVEL_RTP = 2,
	SCCP_VERBOSE_LEVEL_MWI = 1<< 15,
	SCCP_VERBOSE_LEVEL_EVENT = 1 << 16
} sccp_verbose_level_t;
typedef enum {
	SCCP_FEATURE_UNKNOWN,
	SCCP_FEATURE_CFWDALL,
	SCCP_FEATURE_CFWDBUSY,
	SCCP_FEATURE_CFWDNOANSWER,
	SCCP_FEATURE_DND,
	SCCP_FEATURE_PRIVACY,
	SCCP_FEATURE_MONITOR
} sccp_feature_type_t;


typedef struct{
	boolean_t	enabled;
	uint8_t		status;
} sccp_featureConfiguration_t;

typedef enum{
	SCCP_CONFIG_READINITIAL,
	SCCP_CONFIG_READRELOAD
}sccp_readingtype_t;

typedef struct sccp_mailbox sccp_mailbox_t;
struct sccp_mailbox{
	char *mailbox;
	char *context;

	SCCP_LIST_ENTRY(sccp_mailbox_t) list;
};

#include "sccp_protocol.h"


/* privacy definition */
#define SCCP_PRIVACYFEATURE_HINT 			1 << 1;
#define SCCP_PRIVACYFEATURE_CALLPRESENT 	1 << 2;






struct sccp_selectedchannel {
	sccp_channel_t * channel;
	SCCP_LIST_ENTRY(sccp_selectedchannel_t) list;
};

typedef struct sccp_linedevices	sccp_linedevices_t;
struct sccp_linedevices {
	sccp_device_t 	*device;
	SCCP_LIST_ENTRY(sccp_linedevices_t) list;
};


struct sccp_buttonconfig {
	uint8_t			instance;		/*!< instance on device */
	button_type_t 	type;			/*!< button type (e.g. line, speeddial, feature, empty) */
	SCCP_LIST_ENTRY(sccp_buttonconfig_t) list;
	union sccp_button{
		struct {
			char 	name[80];
		} line;

		struct sccp_speeddial{
			char 	label[StationMaxNameSize];
			char 	ext[AST_MAX_EXTENSION];
			char 	hint[AST_MAX_EXTENSION];
		} speeddial;

		struct sccp_service {
			char 	label[StationMaxNameSize];			/*!< The label of the serviceURL button */
			char 	url[StationMaxServiceURLSize];		/*!< The number to dial when it's hit */
		} service;

		struct sccp_feature {
			uint8_t index;
			sccp_feature_type_t id;
			char 	label[StationMaxNameSize];
			char 	options[254];
			uint8_t status;
		} feature;
	} button;
};

struct sccp_hostname {
	char name[MAXHOSTNAMELEN];
	SCCP_LIST_ENTRY(sccp_hostname_t) list;
};



/* A line is a the equiv of a 'phone line' going to the phone. */
struct sccp_line {

	/* lockmeupandtiemedown */
	ast_mutex_t lock;

	/* This line's ID, used for logging into (for mobility) */
	char id[4];

	/* PIN number for mobility/roaming. */
	char pin[8];

	/* The lines position/instanceId on the current device*/
//	uint8_t instance;

	/* the name of the line, so use in asterisk (i.e SCCP/<name>) */
	char name[80];

	/* A description for the line, displayed on in header (on7960/40)
	* or on main  screen on 7910 */
	char description[StationMaxNameSize];

	/* A name for the line, displayed next to the button (7960/40). */
	char label[StationMaxNameSize];

	/* mainbox numbers (seperated by commas) to check for messages */
	//char mailbox[AST_MAX_EXTENSION];
	SCCP_LIST_HEAD(, sccp_mailbox_t) mailboxes;


	/* Voicemail number to dial */
	char vmnum[AST_MAX_EXTENSION];

	/* Meetme Extension to be dialed*/
	char meetmenum[AST_MAX_EXTENSION];

	/* The context we use for outgoing calls. */
	char context[AST_MAX_CONTEXT];
	char language[MAX_LANGUAGE];
	char accountcode[AST_MAX_ACCOUNT_CODE];
	char musicclass[MAX_MUSICCLASS];
	int					amaflags;
	ast_group_t				callgroup;
#ifdef CS_SCCP_PICKUP
	ast_group_t				pickupgroup;
#endif
	/* CallerId to use on outgoing calls*/
	char cid_name[AST_MAX_EXTENSION];
	char cid_num[AST_MAX_EXTENSION];

	/* max incoming calls limit */
	uint8_t incominglimit;

	/* rtp stream tos */
	uint32_t	rtptos;

	/* The currently active channel. */
	/* sccp_channel_t * activeChannel; */

	/* Linked list of current channels for this line */
	SCCP_LIST_HEAD(, sccp_channel_t) channels;

	/* Number of currently active channels */
	uint8_t channelCount;

	/* global list entry */
	SCCP_LIST_ENTRY(sccp_line_t) list;

	/* The device this line is currently registered to. */
	//sccp_device_t * device;
	SCCP_LIST_HEAD(, sccp_linedevices_t) devices;


	/* call forward SCCP_CFWD_ALL or SCCP_CFWD_BUSY */
	uint8_t cfwd_type;
	char * cfwd_num;

	/* transfer to voicemail softkey. Basically a call forward */
	char * trnsfvm;

	/* secondary dialtone */
	char secondary_dialtone_digits[10];
	uint8_t	secondary_dialtone_tone;

	/* echocancel phone support */
	unsigned int			echocancel:1;
	unsigned int			silencesuppression:1;
	unsigned int			transfer:1;
	unsigned int			spareBit4:1;
	unsigned int			spareBit5:1;
	unsigned int			spareBit6:1;
#ifdef CS_SCCP_REALTIME
	unsigned int			realtime:1;			/*!< is it a realtimeconfiguration*/
#endif
	struct ast_variable 	* variables;		/*!< Channel variables to set */
	unsigned int			dnd:3;				/*!< dnd on line */
	uint8_t 				dndmode;			/*!< dnd mode: see SCCP_DNDMODE_* */

#ifdef CS_DYNAMIC_CONFIG
// this is for reload routines
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_line_t				pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif

	int capability;

	/* statistic for line */
	struct{
		uint8_t	numberOfActiveDevices;
		uint8_t	numberOfActiveChannels;
		uint8_t	numberOfHoldChannels;
		uint8_t numberOfDNDDevices;
	}statistic;

	struct{
		int			newmsgs;
		int			oldmsgs;
	}voicemailStatistic;
};

/* This defines a speed dial button */
struct sccp_speed {
	/* The instance of the speeddial in the sccp.conf */
	uint8_t config_instance;
	/* The instance on the current device */
	uint8_t instance;
	/* SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE (hint) */
	uint8_t type;

	/* The name of the speed dial button */
	char name[StationMaxNameSize];

	/* The number to dial when it's hit */
	char ext[AST_MAX_EXTENSION];
	char hint[AST_MAX_EXTENSION];

	/* Pointer to speed dials list */
	SCCP_LIST_ENTRY(sccp_speed_t) list;
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_speed_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
};



struct sccp_device {
	ast_mutex_t lock;

	/* SEP<macAddress> of the device. */
	char id[StationMaxDeviceNameSize];

	/* internal description. Skinny protocol does not use it */
	char description[40];

	/* model of this phone used for setting up features/softkeys/buttons etc. */
	char config_type[10];

	/* model of this phone sent by the station, devicetype*/
	uint32_t skinny_type;

	/* timezone offset */
	int tz_offset;

	/* version to send to the phone */
	char imageversion[StationMaxVersionSize];

	/* this are for support of message 0x0073
	 * AccessoryStatusMessage - Protocol v.11
	 * CCM7 -FS */
	uint8_t accessoryused;
	uint8_t accessorystatus;

	/* If the device has been rully registered yet */
	uint8_t registrationState;

	/* asterisk codec device preference */
	struct ast_codec_pref codecs;

	/* SCCP_DEVICE_ONHOOK or SCCP_DEVICE_OFFHOOK */
	sccp_devicestate_t state;

	/* ringer mode. Need it for the ringback */
	//uint8_t ringermode;

	/* dnd mode: see SCCP_DNDMODE_* */
	uint8_t dndmode;

	/* last dialed number */
	char lastNumber[AST_MAX_EXTENSION];

	/* asterisk codec capability */
	int capability;

	/* channel state where to open the rtp media stream */
	uint8_t earlyrtp;

	/* Number of currently active channels */
	uint8_t channelCount;

	/* skinny supported protocol version */
	uint8_t protocolversion;

	/* skinny used protocol version */
	uint8_t inuseprotocolversion;

	/* station specific keepalive timeout */
	int						keepalive;

	/* permit or deny connections to the main socket */
	struct ast_ha			*ha;

	uint32_t				conferenceid;

	unsigned int			mwilamp:3;
	unsigned int			mwioncall:1;
	unsigned int			softkeysupport:1;
	unsigned int			mwilight:1;
	unsigned int			dnd:3;
	unsigned int			transfer:1;
	unsigned int			conference:1;
	unsigned int			park:1;
	unsigned int			cfwdall:1;
	unsigned int			cfwdbusy:1;
	unsigned int			cfwdnoanswer:1;
#ifdef CS_SCCP_PICKUP
	unsigned int			pickupexten:1;
	char					* pickupcontext;
	unsigned int			pickupmodeanswer:1;
#endif
	unsigned int			dtmfmode:1; 			/*!< 0 inband - 1 outofband */
	unsigned int			nat:1;
	unsigned int			directrtp:1;
	unsigned int			trustphoneip:1;
	unsigned int			needcheckringback:1;


	boolean_t				realtime;		/*!< is it a realtime configuration*/
	sccp_channel_t   		* active_channel;
	sccp_channel_t   		* transfer_channel; 		/*!< the channel under transfer */
	sccp_channel_t			* conference_channel;		/*!< the channel is going to be conferenced */
	sccp_line_t      		* currentLine;
	sccp_session_t   		* session;
	//SCCP_LIST_ENTRY(sccp_linedevices_t) linedevicelist;		/*!< line_device_list */
	SCCP_LIST_ENTRY(sccp_device_t) list;				/*!< global device list */
	SCCP_LIST_HEAD(,sccp_hint_t) hints;					/*!< list of hint pointers. Internal lines to notify the state */
	SCCP_LIST_HEAD(,sccp_buttonconfig_t) buttonconfig;
	uint8_t					linesCount;
	SCCP_LIST_HEAD(,sccp_selectedchannel_t) selectedChannels;
    SCCP_LIST_HEAD(,sccp_addon_t) addons;
    SCCP_LIST_HEAD(,sccp_hostname_t) permithosts;		/*!< permit registration to the hostname ip address */

	pthread_t        		postregistration_thread;
	struct ast_variable 	* variables;						/*!< Channel variables to set */
	char 					* phonemessage;						/*!< message to display on device*/
	uint8_t					defaultLineInstance;

	struct{
		boolean_t	headset;
		boolean_t	handset;
		boolean_t	speaker;
	} accessoryStatus;

	struct{
		uint8_t		numberOfLines;
		uint8_t		numberOfSpeeddials;
		uint8_t		numberOfFeatures;
		uint8_t		numberOfServices;
	} configurationStatistic;

#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_device_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */

	SCCP_LIST_ENTRY(sccp_device_t) list;
#endif

	boolean_t 		isAnonymous;
	light_t			mwiLight;

	struct{
		int			newmsgs;
		int			oldmsgs;
	}voicemailStatistic;

	/* feature configurations */
	sccp_featureConfiguration_t privacyFeature;				/*!< device privacy flag see SCCP_PRIVACYFEATURE_* */
	sccp_featureConfiguration_t overlapFeature;				/*!< overlap dial support */
	sccp_featureConfiguration_t monitorFeature;				/*!< monitor (automon) support */

};

// Number of additional keys per addon -FS
#define SCCP_ADDON_7914_TAPS		14
#define SCCP_ADDON_7915_TAPS		24
#define SCCP_ADDON_7916_TAPS		24

struct sccp_addon {
	int type;
	SCCP_LIST_ENTRY(sccp_addon_t) list;
	sccp_device_t * device;
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_addon_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif


};

struct sccp_session {
  	ast_mutex_t				lock;
  	void					*buffer;
  	int32_t					buffer_size;
  	struct sockaddr_in		sin;
  	struct in_addr			ourip;								/*< ourip is for rtp use */
  	time_t					lastKeepAlive;
  	int						fd;
  	int						rtpPort;
  	sccp_device_t 			*device;
  	SCCP_LIST_ENTRY(sccp_session_t) list;
  	unsigned int			needcheckringback:1;
};

struct sccp_channel {
	ast_mutex_t			lock;
	/* codec requested by asterisk */
	int				format;
	/* asterisk codec channel preference */
	struct ast_codec_pref 		codecs;
	char				calledPartyName[StationMaxNameSize];
	char				calledPartyNumber[StationMaxDirnumSize];
	char				callingPartyName[StationMaxNameSize];
	char				callingPartyNumber[StationMaxDirnumSize];

	uint32_t			callid;
	uint32_t			passthrupartyid;

	uint32_t			conferenceid; 	/* this will be used in native conferencing mode and will differ from callid  -FS*/

	uint8_t			state;				/*< internal channel state SCCP_CHANNELSTATE_* */
	uint8_t			previousChannelState;/*< previous channel state SCCP_CHANNELSTATE_* */
	uint8_t			callstate;			/*< skinny state */
	skinny_calltype_t	calltype;			/*< SKINNY_CALLTYPE_* */

	int				digittimeout;			/*< scheduler timeout on dialing state */
	uint8_t			ringermode;			/*< SCCPRingerMode application */

	char dialedNumber[AST_MAX_EXTENSION];			/*< last dialed number */

	sccp_device_t 	 	*device;
	struct ast_channel 	 	*owner;
	sccp_line_t		 	*line;
	struct ast_rtp	 	*rtp;
	struct sockaddr_in		rtp_addr;
	SCCP_LIST_ENTRY(sccp_channel_t) list;
	uint8_t				autoanswer_type;
	uint8_t				autoanswer_cause;
	boolean_t			answered_elsewhere;

	/* don't allow sccp phones to monitor (hint) this call */
	boolean_t			private;
	char 				musicclass[MAX_MUSICCLASS];

	uint8_t		ss_action; /* simple switch action, this is used in dial thread to collect numbers for callforward, pickup and so on -FS*/
	uint8_t		ss_data; /* simple switch integer param */
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_channel_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif

	/* feature sets */
	boolean_t			monitorEnabled;
};

struct sccp_global_vars {
	ast_mutex_t				lock;

	pthread_t 				monitor_thread;	// ADDED IN 414 -FS
	ast_mutex_t				monitor_lock;  // ADDED IN 414 -FS

	SCCP_LIST_HEAD(, sccp_session_t) sessions;
	SCCP_LIST_HEAD(, sccp_device_t) devices;
	SCCP_LIST_HEAD(, sccp_line_t) lines;
	ast_mutex_t				socket_lock;
	pthread_t				socket_thread; // Moved her in v2 SVN 426 -FS
	int 					descriptor;
	int						usecnt;	/* Keep track of when we're in use. */
	ast_mutex_t				usecnt_lock;

	char					servername[StationMaxDisplayNotifySize];
	struct sockaddr_in			bindaddr;
	int					ourport;
	/* permit or deny connections to the main socket */
	struct ast_ha				*ha;
	/* localnet for NAT */
	struct ast_ha				*localaddr;
	struct sockaddr_in			externip;
	char 					externhost[MAXHOSTNAMELEN];
	time_t 				externexpire;
	int 					externrefresh;

	char					context[AST_MAX_CONTEXT];
	char					language[MAX_LANGUAGE];
	char					accountcode[AST_MAX_ACCOUNT_CODE];
	char					musicclass[MAX_MUSICCLASS];
	/*char					mohinterpret[MAX_MUSICCLASS];*/
	int					amaflags;
	ast_group_t				callgroup;
#ifdef CS_SCCP_PICKUP
	ast_group_t				pickupgroup;
	int					pickupmodeanswer:1;
#endif
	int					global_capability;
	struct					ast_codec_pref global_codecs;
	int					keepalive;
	int					debug;
	int					fdebug;
	char 					date_format[7];

	uint8_t				firstdigittimeout;				/*< Wait up to 16 seconds for first digit */
	uint8_t				digittimeout;					/*< How long to wait for following digits */
	char					digittimeoutchar;				/*< what char will force the dial */
	unsigned int				recorddigittimeoutchar:1; /*< Whether to include the digittimeoutchar in the call logs */

	uint32_t				tos;
	uint32_t				rtptos;
	/* channel state where to open the rtp media stream */
	uint8_t				earlyrtp;

	uint8_t 				dndmode;					/*< dnd mode: see SCCP_DNDMODE_* */
	uint8_t 				protocolversion;				/*< skinny protocol version */

	/* autoanswer stuff */
	uint8_t				autoanswer_ring_time;
	uint8_t				autoanswer_tone;
	uint8_t				remotehangup_tone;
	uint8_t				transfer_tone;
	uint8_t				callwaiting_tone;

	unsigned int				mwilamp:3;
	unsigned int				mwioncall:1;
	unsigned int				echocancel:1;					/*< echocancel phone support */
	unsigned int				silencesuppression:1;
	unsigned int				trustphoneip:1;
	unsigned int				private:1; 					/*< permit private function */
	unsigned int				privacy:2;
	unsigned int				blindtransferindication:1; 			/*< SCCP_BLINDTRANSFER_* */
	unsigned int				cfwdall:1;
	unsigned int				cfwdbusy:1;
	unsigned int				cfwdnoanswer:1;
	unsigned int				nat:1;
	unsigned int				directrtp:1;
	unsigned int				useoverlap:1;	/* overlap dial support */
	call_answer_order_t			callAnswerOrder;
#ifdef CS_MANAGER_EVENTS
	boolean_t					callevents;
#endif
#ifdef CS_SCCP_REALTIME
	char 					realtimedevicetable[45];			/*< databasetable for sccp devices*/
	char 					realtimelinetable[45];			/*< databasetable for sccp lines*/
#endif
#ifndef ASTERISK_CONF_1_2
    struct ast_jb_conf             global_jbconf;
#endif
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	struct sccp_global_vars	pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif

	pthread_t				mwiMonitorThread; // MC
	boolean_t				allowAnonymus;		/*!< allow anonymous/unknown devices */
	sccp_hotline_t				*hotline;
};



struct sccp_hotline {
	sccp_line_t		*line;
	char			exten[50];
};


/* simple switch modes - Used in simple
 * switch tread to distinguish dial from
 * other number collects
 *
 * Moved here from protocol.h
 */
#define SCCP_SS_DIAL				0
#define SCCP_SS_GETFORWARDEXTEN		1
#define SCCP_SS_GETPICKUPEXTEN		2
#define SCCP_SS_GETMEETMEROOM		3
#define SCCP_SS_GETBARGEEXTEN		4
#define SCCP_SS_GETCBARGEROOM		5
/*
 * NEW NEW NEW NEW
 *
 * SCHEDULER TASKS !!!
 *
 */
#define sccp_sched_add ast_sched_add
#define sccp_sched_del ast_sched_del
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

struct sccp_global_vars *sccp_globals;

uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s);

#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause);
#else
struct ast_channel *sccp_request(char *type, int format, void *data);
#endif

int sccp_devicestate(void *data);


sccp_device_t *build_devices_wo(struct ast_variable *v, uint8_t realtime);
#define build_devices(x) build_devices_wo(x, 0)

sccp_line_t * build_lines_wo(struct ast_variable *v, uint8_t realtime);
#define build_lines(x) build_lines_wo(x, 0)


#ifndef ASTERISK_CONF_1_2
struct sched_context *sched;
struct io_context *io;

void * sccp_do_monitor(void *data); // ADDED IN SVN 414 -FS
int sccp_restart_monitor(void); // ADDED IN SVN 414 -FS
#endif
#ifndef ASTERISK_CONF_1_2
enum ast_bridge_result sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms);
#endif

static inline unsigned char sccp_is_nonempty_string(char *string) {
	if(NULL != string) {
		if(!ast_strlen_zero(string)) {
			return 1;
		}
	}

	return 0;
}




#endif /* __CHAN_SCCP_H */

