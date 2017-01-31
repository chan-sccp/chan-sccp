/*!
 * \file        sccp_globals.h
 * \brief       SCCP Globals Header
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#pragma once
#include "config.h"
#include "define.h"
#include "sccp_codec.h"
#include "sccp_netsock.h"

__BEGIN_C_EXTERN__
SCCP_API char SCCP_VERSIONSTR[300];
SCCP_API char SCCP_REVISIONSTR[30];

/*!
 * \brief SCCP device-line subscriptionId
 * \note for addressing individual devices on shared line
 * \todo current size if 176 bits, could/should be reduced by using charptr instead
 * \todo at the moment subscriptionId is being copied from lines to channel and linedevices
 */
struct subscriptionId {
	char number[SCCP_MAX_EXTENSION];									/*!< will be added to cid */
	char name[SCCP_MAX_EXTENSION];										/*!< will be added to cidName */
	char label[SCCP_MAX_LABEL];										/*!< will be added to cidName */
	char aux[SCCP_MAX_AUX];											/*!< auxiliary parameter. Allows for phone-specific behaviour on a line. */
	boolean_t replaceCid;											/*!< Should cidnumber be replaced instead of appended to, controled by the '=' subscription flag */
};

/*!
 * \brief SCCP Global Variable Structure
 */
struct sccp_global_vars {
	int descriptor;												/*!< Server Socket Descriptor */
	int keepalive;												/*!< KeepAlive */
	int32_t debug;												/*!< Debug */
	int module_running;
	pbx_rwlock_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */

#if ASTERISK_VERSION_GROUP < 110
	pthread_t monitor_thread;										/*!< Monitor Thread */
	sccp_mutex_t monitor_lock;										/*!< Monitor Asterisk Lock */
#endif

	sccp_threadpool_t *general_threadpool;									/*!< General Work Threadpool */

	SCCP_RWLIST_HEAD (, sccp_session_t) sessions;								/*!< SCCP Sessions */
	SCCP_RWLIST_HEAD (, sccp_device_t) devices;								/*!< SCCP Devices */
	SCCP_RWLIST_HEAD (, sccp_line_t) lines;									/*!< SCCP Lines */

	sccp_mutex_t socket_lock;										/*!< Socket Lock */
#ifndef SCCP_ATOMIC	
	sccp_mutex_t usecnt_lock;										/*!< Use Counter Asterisk Lock */
#endif
	int usecnt;												/*!< Keep track of when we're in use. */
	int amaflags;												/*!< AmaFlags */
	pthread_t socket_thread;										/*!< Socket Thread */
	pthread_t mwiMonitorThread;										/*!< MWI Monitor Thread */

	char dateformat[SCCP_MAX_DATE_FORMAT];									/*!< Date Format */

	struct sccp_ha *ha;											/*!< Permit or deny connections to the main socket */
	struct sockaddr_storage bindaddr;									/*!< Bind IP Address */
	struct sccp_ha *localaddr;										/*!< Localnet for Network Address Translation */

	struct sockaddr_storage externip;									/*!< External IP Address (\todo should change to an array of external ip's, because externhost could resolv to multiple ip-addresses (h_addr_list)) */
	time_t externexpire;											/*!< External Expire */
	uint16_t externrefresh;											/*!< External Refresh */
	
	boolean_t recorddigittimeoutchar;									/*!< Record Digit Time Out Char. Whether to include the digittimeoutchar in the call logs */
	uint8_t firstdigittimeout;										/*!< First Digit Timeout. Wait up to 16 seconds for first digit */
	
	uint8_t digittimeout;											/*!< Digit Timeout. How long to wait for following digits */
	char digittimeoutchar;											/*!< Digit End Character. What char will force the dial (Normally '#') */
	boolean_t simulate_enbloc;										/*!< Simulated Enbloc Dialing for older device to speed up dialing */
	uint8_t autoanswer_ring_time;										/*!< Auto Answer Ring Time */
	uint8_t autoanswer_tone;										/*!< Auto Answer Tone */
	uint8_t remotehangup_tone;										/*!< Remote Hangup Tone */
	uint8_t transfer_tone;											/*!< Transfer Tone */
	uint8_t dnd_tone;											/*!< DND Tone */
	uint8_t callwaiting_tone;										/*!< Call Waiting Tone */

	uint8_t callwaiting_interval;										/*!< Call Waiting Ring Interval */
	uint8_t sccp_tos;											/*!< SCCP Socket Type of Service (TOS) (QOS) (Signaling) */
	uint8_t audio_tos;											/*!< Audio Socket Type of Service (TOS) (QOS) (RTP) */
	uint8_t video_tos;											/*!< Video Socket Type of Service (TOS) (QOS) (VRTP) */
	uint8_t sccp_cos;											/*!< SCCP Socket Class of Service (COS) (QOS) (Signaling) */
	uint8_t audio_cos;											/*!< Audio Socket Class of Service (COS) (QOS) (RTP) */
	uint8_t video_cos;											/*!< Video Socket Class of Service (COS) (QOS) (VRTP) */
	boolean_t dndFeature;											/*!< Do Not Disturb (DND) Mode: \see SCCP_DNDMODE_* */

	boolean_t transfer_on_hangup;										/*!< Complete transfer on hangup */
#ifdef CS_MANAGER_EVENTS
	boolean_t callevents;											/*!< Call Events */
#endif
	boolean_t echocancel;											/*!< Echo Canel Support (Boolean, default=on) */
	boolean_t silencesuppression;										/*!< Silence Suppression Support (Boolean, default=on)  */
	boolean_t trustphoneip;											/*!< Trust Phone IP Support (Boolean, default=on) */
	boolean_t privacy;											/*!< Privacy Support (Length=2) */
	boolean_t mwioncall;											/*!< MWI On Call Support (Boolean, default=on) */
	boolean_t cfwdall;											/*!< Call Forward All Support (Boolean, default=on) */
	boolean_t cfwdbusy;											/*!< Call Forward on Busy Support (Boolean, default=on) */
	boolean_t cfwdnoanswer;											/*!< Call Forward on No-Answer Support (Boolean, default=on) */
	boolean_t directrtp;											/*!< Direct RTP */
	boolean_t useoverlap;											/*!< Overlap Dial Support */

	char *meetmeopts;											/*!< Meetme Options to be Used */
	sccp_group_t callgroup;											/*!< Call Group */
#ifdef CS_SCCP_PICKUP
	sccp_group_t pickupgroup;										/*!< PickUp Group */
	boolean_t directed_pickup_modeanswer;									/*!< Directed PickUp Mode Answer (boolean, default" on) */
#else
	uint8_t _padding1[1];
#endif
	uint8_t ringtype;											/*!< RingType for incoming calls */
	boolean_t meetme;											/*!< Meetme on/off */
	boolean_t allowAnonymous;										/*!< Allow Anonymous/Guest Devices */
	sccp_earlyrtp_t earlyrtp;										/*!< Channel State where to open the rtp media stream */

	skinny_lampmode_t mwilamp;										/*!< MWI/Lamp (Length:3) */
	sccp_blindtransferindication_t blindtransferindication;							/*!< Blind Transfer Indication Support (Boolean, default=on = SCCP_BLINDTRANSFER_MOH) */
	sccp_nat_t nat;												/*!< Network Address Translation */
	sccp_call_answer_order_t callanswerorder;								/*!< Call Answer Order */

#if ASTERISK_VERSION_NUMBER >= 10400
	struct ast_jb_conf *global_jbconf;									/*!< Global Jitter Buffer Configuration */
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
SCCP_API struct sccp_global_vars *sccp_globals;
SCCP_API struct softKeySetConfigList softKeySetConfig;								/*!< List of SoftKeySets */
#ifdef CS_DEVSTATE_FEATURE
SCCP_API const char devstate_db_family[];
#endif

/* Function Declarations */
//#if UNUSEDCODE // 2015-11-01
//SCCP_API int SCCP_CALL sccp_sched_free(void *ptr);
//#endif

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
