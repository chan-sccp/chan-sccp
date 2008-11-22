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

#ifndef ASTERISK_CONF_1_2
#include "asterisk/abstract_jb.h"
#endif

#ifdef CS_AST_HAS_ENDIAN
#include <asterisk/endian.h>
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
#error Please edit chan_sccp.h (line 94 or 95) to set the platform byte order
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

#define __SCCP_VERSION "svn-trunk"
#define SCCP_LOCK_TRIES 10
#define SCCP_LOCK_USLEEP 100

/* I don't like the -1 returned value */
#define sccp_true(x) (ast_true(x) ? 1 : 0)
#define sccp_log(x) if ((!sccp_globals->fdebug && sccp_globals->debug >= x) || (sccp_globals->fdebug == x)) ast_verbose
#define GLOB(x) sccp_globals->x

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

typedef struct sccp_channel		sccp_channel_t;
typedef struct sccp_session		sccp_session_t;
typedef struct sccp_line		sccp_line_t;
typedef struct sccp_speed		sccp_speed_t;
typedef struct sccp_serviceURL		sccp_serviceURL_t;
typedef struct sccp_device		sccp_device_t;
typedef struct sccp_addon		sccp_addon_t; // Added on SVN 327 -FS
typedef struct sccp_hint		sccp_hint_t;
typedef struct sccp_hostname		sccp_hostname_t;
typedef struct sccp_ast_channel_name	sccp_ast_channel_name_t;
typedef struct sccp_buttonconfig	sccp_buttonconfig_t;
typedef struct sccp_softkeyTemplate	sccp_softkeyTemplate_t;
typedef struct sccp_softkeyTemplateSet	sccp_softkeyTemplateSet_t;
typedef struct sccp_list		sccp_list_t;
typedef enum { FALSE, TRUE } boolean;

typedef void sk_func (sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

#include "sccp_protocol.h"



struct sccp_hostname {
	char name[MAXHOSTNAMELEN];
	sccp_hostname_t *next;
};

#define SCCP_HINTSTATE_NOTINUSE		0
#define SCCP_HINTSTATE_INUSE		1
struct sccp_hint {
	int hintid;
	sccp_device_t *device;
	uint8_t	instance;
	char context[AST_MAX_CONTEXT];
	char exten[AST_MAX_EXTENSION];
	uint8_t state;
	uint32_t callid;
};

struct sccp_list{
	void 		*data;
	sccp_list_t	*next;
	sccp_list_t	*prev;
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
	uint8_t instance;

	/* the name of the line, so use in asterisk (i.e SCCP/<name>) */
	char name[80];

	/* A description for the line, displayed on in header (on7960/40)
	* or on main  screen on 7910 */
	char description[StationMaxNameSize];

	/* A name for the line, displayed next to the button (7960/40). */
	char label[StationMaxNameSize];

	/* mainbox numbers (seperated by commas) to check for messages */
	char mailbox[AST_MAX_EXTENSION];

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
	/*  sccp_channel_t * activeChannel; */

	/* Linked list of current channels for this line */
	sccp_channel_t * channels;

	/* Number of currently active channels */
	uint8_t channelCount;

	/* Prev and Next line in the global list */
	sccp_line_t * next, * prev;

	/* Prev and Next line on the current device. */
	sccp_line_t * next_on_device, * prev_on_device;

	/* The device this line is currently registered to. */
	sccp_device_t * device;

	/* list of hint pointers. Internal lines to notify the state */
	sccp_list_t 	*hints;

	/* call forward SCCP_CFWD_ALL or SCCP_CFWD_BUSY */
	uint8_t cfwd_type;
	char * cfwd_num;

	/* transfer to voicemail softkey. Basically a call forward */
	char * trnsfvm;

	/* secondary dialtone */
	char secondary_dialtone_digits[10];
	uint8_t	secondary_dialtone_tone;

	unsigned int			mwilight:1;

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

	/* Pointer to next speed dial */
	sccp_speed_t * next;
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_speed_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
};

/* This defines a serviceURL button */

//struct sccp_serviceURL {
//	uint8_t config_instance;									/*!< The instance in the sccp.conf */
//	uint8_t instance;											/*!< The instance on the current device */
//	char label[StationMaxNameSize];								/*!< The label of the serviceURL button */
//	char URL[StationMaxServiceURLSize];							/*!< The number to dial when it's hit */
//	sccp_serviceURL_t * next;									/*!< Pointer to next serviceURL */
//#ifdef CS_DYNAMIC_CONFIG
//	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
//	sccp_serviceURL_t		pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
//#endif
//};


struct sccp_softkeyTemplate{
	uint8_t					type;
	sccp_softkeyTemplate_t 	*next;
	sccp_softkeyTemplate_t  *prev;
};

struct sccp_softkeyTemplateSet{
	sccp_softkeyTemplate_t	*onhook;
	sccp_softkeyTemplate_t	*connected;
	sccp_softkeyTemplate_t	*onhold;
	sccp_softkeyTemplate_t	*ringin;
	sccp_softkeyTemplate_t	*offhook;
	sccp_softkeyTemplate_t	*conntrans;
	sccp_softkeyTemplate_t	*digitsfoll;
	sccp_softkeyTemplate_t	*connconf;
	sccp_softkeyTemplate_t	*ringout;
	sccp_softkeyTemplate_t	*offhookfeat;
	sccp_softkeyTemplate_t	*onhint;
};

struct sccp_device {
	ast_mutex_t lock;

	/* SEP<macAddress> of the device. */
	char id[StationMaxDeviceNameSize];

	/* internal description. Skinny protocol does not use it */
	char description[40];

	/* default line to use (name of line) */
	char defaultline;

	/* model of this phone used for setting up features/softkeys/buttons etc. */
	char config_type[10];

	/* model of this phone sent by the station, devicetype*/
	uint32_t skinny_type;

	/* timezone offset */
	int tz_offset;

	/* version to send to the phone */
	char imageversion[StationMaxVersionSize];

	/* If the device has been rully registered yet */
	uint8_t registrationState;

	/* asterisk codec device preference */
	struct ast_codec_pref codecs;

	/* SCCP_DEVICE_ONHOOK or SCCP_DEVICE_OFFHOOK */
	uint8_t state;

	/* ringer mode. Need it for the ringback */
	uint8_t ringermode;

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

	/* skinny protocol version */
	uint8_t protocolversion;

	/* station specific keepalive timeout */
	int						keepalive;

	/* permit or deny connections to the main socket */
	struct ast_ha			*ha;
	/* permit registration to the hostname ip address */
	sccp_hostname_t			*permithost;

	uint32_t			conferenceid;

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
	char				*pickupcontext;
	unsigned int			pickupmodeanswer:1;
#endif
	unsigned int			dtmfmode:1; 					/*!< 0 inband - 1 outofband */
	unsigned int			nat:1;
	unsigned int			trustphoneip:1;
	unsigned int			needcheckringback:1;
	unsigned int			private:1; 					/*!< permit private function on this device */
	unsigned int			privacy:2;                          		/*!< device privacy flag 0x02 = no hints, 0x01 = hints depending on private key, 0x00 = always hints */
#ifdef CS_SCCP_REALTIME
	unsigned int			realtime:1;					/*!< is it a realtimeconfiguration*/
#endif
	sccp_channel_t   		*active_channel;
	sccp_channel_t   		*transfer_channel; 				/*!< the channel under transfer */
	sccp_channel_t			*conference_channel;				/*!< the channel is going to be conferenced */
	sccp_serviceURL_t 		*serviceURLs;
	sccp_speed_t     		*speed_dials;
	sccp_line_t      		*lines;
	sccp_line_t      		*currentLine;
	sccp_session_t   		*session;
	sccp_device_t    		*next;
	sccp_list_t      		*hints;						/*!< list of hint pointers. Internal lines to notify the state */
	pthread_t        		postregistration_thread;
	struct ast_variable 		*variables;					/*!< Channel variables to set */
	char 				*phonemessage;					/*!< message to display on device*/
    	sccp_addon_t			*addons;
	sccp_buttonconfig_t		*buttonconfig;
	struct sccp_selected_channel    *selectedChannels;
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;				/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_device_t			pendingUpdate;					/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
};

// Number of additional keys per addon -FS
#define SCCP_ADDON_7914_TAPS		14
#define SCCP_ADDON_7915_TAPS		24
#define SCCP_ADDON_7916_TAPS		24

struct sccp_addon {
	ast_mutex_t	lock;
	int type;
//	sccp_addon_t * prev;
	sccp_addon_t * next;
	sccp_device_t * device;
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	sccp_addon_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
};

struct sccp_session {
  	ast_mutex_t			lock;
  	void				*buffer;
  	size_t				buffer_size;
  	struct sockaddr_in		sin;
  	struct in_addr			ourip;								/*< ourip is for rtp use */
  	time_t				lastKeepAlive;
  	int				fd;
  	int				rtpPort;
  	sccp_device_t 			*device;
  	sccp_session_t			*prev, *next;
  	unsigned int			needcheckringback:1;
};

struct sccp_channel {
	ast_mutex_t			lock;
	/* codec requested by asterisk */
	int				format;
	char				calledPartyName[StationMaxNameSize];
	char				calledPartyNumber[StationMaxDirnumSize];
	char				callingPartyName[StationMaxNameSize];
	char				callingPartyNumber[StationMaxDirnumSize];
	uint32_t			callid;

	uint32_t			conferenceid; 	/* this will be used in native conferencing mode and will differ from callid  -FS*/

	uint8_t			state;				/*< internal channel state SCCP_CHANNELSTATE_* */
	uint8_t			callstate;			/*< skinny state */
	uint8_t			calltype;			/*< SKINNY_CALLTYPE_* */

	time_t				digittimeout;			/*< timeout on dialing state */
	uint8_t			ringermode;			/*< SCCPRingerMode application */

	char dialedNumber[AST_MAX_EXTENSION];			/*< last dialed number */

	sccp_device_t 	 	*device;
	struct ast_channel 	 	*owner;
	sccp_line_t		 	*line;
	struct ast_rtp	 	*rtp;
	struct sockaddr_in		rtp_addr;
	sccp_channel_t	 	*prev, * next,
					*prev_on_line, * next_on_line;
	uint8_t			autoanswer_type;
	uint8_t			autoanswer_cause;

	/* don't allow sccp phones to monitor (hint) this call */
	unsigned int		private:1;
	unsigned int		hangupok:1;
	char musicclass[MAX_MUSICCLASS];

	uint8_t		ss_action; /* simple switch action, this is used in dial thread to collect numbers for callforward, pickup and so on -FS*/
	uint8_t		ss_data; /* simple switch integer param */
// #ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
//	sccp_channel_t			pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
//#endif
};

struct sccp_global_vars {
	ast_mutex_t				lock;

	sccp_session_t			*sessions;
	ast_mutex_t				sessions_lock;
	sccp_device_t			*devices;
	ast_mutex_t				devices_lock;
	sccp_line_t	  			*lines;
	ast_mutex_t				lines_lock;
	sccp_channel_t 			*channels;
	ast_mutex_t				channels_lock;
	ast_mutex_t				socket_lock;
	int 					descriptor;
	/* Keep track of when we're in use. */
	int					usecnt;
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

	unsigned int			mwilamp:3;
	unsigned int			mwioncall:1;
	unsigned int			echocancel:1;					/*< echocancel phone support */
	unsigned int			silencesuppression:1;
	unsigned int			trustphoneip:1;
	unsigned int			private:1; 					/*< permit private function */
	unsigned int			privacy:2;
	unsigned int			blindtransferindication:1; 			/*< SCCP_BLINDTRANSFER_* */
	unsigned int			cfwdall:1;
	unsigned int			cfwdbusy:1;
	unsigned int			cfwdnoanswer:1;
#ifdef CS_MANAGER_EVENTS
	unsigned int			callevents:1;
#endif
#ifdef CS_SCCP_REALTIME
	char 				realtimedevicetable[45];			/*< databasetable for sccp devices*/
	char 				realtimelinetable[45];			/*< databasetable for sccp lines*/
#endif
#ifndef ASTERISK_CONF_1_2
    struct ast_jb_conf             	global_jbconf;
#endif
#ifdef CS_DYNAMIC_CONFIG
	unsigned int			pendingDelete:1;	/*!< this bit will tell the scheduler to delete this line when unused */
	struct sccp_global_vars		pendingUpdate;		/*!< this will contain the updated line struct once reloaded from config to update the line when unused */
#endif
	sccp_softkeyTemplateSet_t	*softkeyTemplateSet;
};

struct sccp_selected_channel {
  sccp_channel_t *c;
  struct sccp_selected_channel *next;
};

struct sccp_global_vars *sccp_globals;

struct sccp_buttonconfig {
	uint8_t		instance;		/*!< instance on device */
	char 		type[15];		/*!< button type (e.g. line, speeddial, feature, empty) */
	union sccp_button{
		struct {
			char name[80];
		} line;

		struct sccp_speeddial{
			char label[StationMaxNameSize];
			char ext[AST_MAX_EXTENSION];
			char hint[AST_MAX_EXTENSION];
		} speeddial;

		struct sccp_serviceURL {
			char label[StationMaxNameSize];			/*!< The label of the serviceURL button */
			char URL[StationMaxServiceURLSize];		/*!< The number to dial when it's hit */
		} serviceurl;

		struct sccp_feature {
			uint8_t index;
			uint8_t id;
			char label[StationMaxNameSize];
			char options[254];
			uint8_t status:1;
		} feature;
	} button;
	sccp_buttonconfig_t 	* next;					/*!< next button on device*/
};








uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s);

#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause);
#else
struct ast_channel *sccp_request(char *type, int format, void *data);
#endif

int sccp_devicestate(void *data);
sccp_hint_t * sccp_hint_make(sccp_device_t *d, uint8_t instance);
void sccp_hint_notify_devicestate(sccp_device_t * d, uint8_t state);
void sccp_hint_notify_linestate(sccp_line_t * l, uint8_t state, sccp_device_t * onedevice);
void sccp_hint_notify_channelstate(sccp_device_t * d, sccp_hint_t * h, sccp_channel_t * c);
int sccp_hint_state(char *context, char* exten, enum ast_extension_states state, void *data);
void sccp_hint_notify(sccp_channel_t *c, sccp_device_t * onedevice);

sccp_device_t * build_devices(struct ast_variable *v);
sccp_device_t * buildDeviceTemplate(void);
sccp_device_t * build_device(struct ast_variable *v, char *devicename);

sccp_line_t * build_lines(struct ast_variable *v);
sccp_line_t * buildLineTemplate(void);
sccp_line_t * buildLine(struct ast_variable *v, char *linename);


void buildSoftkeyTemplate(struct ast_variable *astVar);
int buildGlobals(struct ast_variable *v);


#ifndef ASTERISK_CONF_1_2
struct sched_context *sched;
struct io_context *io;
#endif

#endif /* __CHAN_SCCP_H */

