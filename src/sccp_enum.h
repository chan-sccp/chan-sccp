/*
 * Auto-Generated File, do not modify.  Changes will be destroyed.
 * $Date: $
 * $Revision: $
 */
#pragma once
typedef int (*sccp_enum_str2intval_t)(const char *lookup_str);
typedef char *(*sccp_enum_all_entries_t)(void);

/* = Begin =======================================================================================       sparse sccp_channelstate === */

/*
 * \brief sparse enum sccp_channelstate
 */
typedef enum sccp_channelstate {
	SCCP_CHANNELSTATE_DOWN=0,
	SCCP_CHANNELSTATE_ONHOOK=1,
	SCCP_CHANNELSTATE_OFFHOOK=10,
	SCCP_CHANNELSTATE_GETDIGITS=11,
	SCCP_CHANNELSTATE_DIGITSFOLL=12,
	SCCP_CHANNELSTATE_SPEEDDIAL=13,
	SCCP_CHANNELSTATE_DIALING=14,
	SCCP_CHANNELSTATE_RINGOUT=20,
	SCCP_CHANNELSTATE_RINGING=21,
	SCCP_CHANNELSTATE_PROCEED=22,
	SCCP_CHANNELSTATE_PROGRESS=23,
	SCCP_CHANNELSTATE_CONNECTED=30,
	SCCP_CHANNELSTATE_CONNECTEDCONFERENCE=31,
	SCCP_CHANNELSTATE_HOLD=32,
	SCCP_CHANNELSTATE_CALLWAITING=34,
	SCCP_CHANNELSTATE_CALLPARK=35,
	SCCP_CHANNELSTATE_CALLREMOTEMULTILINE=36,
	SCCP_CHANNELSTATE_CALLCONFERENCE=37,
	SCCP_CHANNELSTATE_CALLTRANSFER=38,
	SCCP_CHANNELSTATE_BLINDTRANSFER=39,
	SCCP_CHANNELSTATE_DND=40,
	SCCP_CHANNELSTATE_BUSY=41,
	SCCP_CHANNELSTATE_CONGESTION=42,
	SCCP_CHANNELSTATE_INVALIDNUMBER=43,
	SCCP_CHANNELSTATE_INVALIDCONFERENCE=44,
	SCCP_CHANNELSTATE_ZOMBIE=45,
	SCCP_CHANNELSTATE_SENTINEL
} sccp_channelstate_t;
int sccp_channelstate_exists(int sccp_channelstate_int_value);
const char * sccp_channelstate2str(sccp_channelstate_t enum_value);
sccp_channelstate_t sccp_channelstate_str2val(const char *lookup_str);
int sccp_channelstate_str2intval(const char *lookup_str);
char *sccp_channelstate_all_entries(void);
/* = End =========================================================================================       sparse sccp_channelstate === */


/* = Begin =======================================================================================        sccp_channelstatereason === */

/*
 * \brief enum sccp_channelstatereason
 */
typedef enum sccp_channelstatereason {
	SCCP_CHANNELSTATEREASON_NORMAL=0,
	SCCP_CHANNELSTATEREASON_TRANSFER,
	SCCP_CHANNELSTATEREASON_CALLFORWARD,
	SCCP_CHANNELSTATEREASON_CONFERENCE,
	SCCP_CHANNELSTATEREASON_SENTINEL
} sccp_channelstatereason_t;
int sccp_channelstatereason_exists(int sccp_channelstatereason_int_value);
const char * sccp_channelstatereason2str(sccp_channelstatereason_t enum_value);
sccp_channelstatereason_t sccp_channelstatereason_str2val(const char *lookup_str);
int sccp_channelstatereason_str2intval(const char *lookup_str);
char *sccp_channelstatereason_all_entries(void);
/* = End =========================================================================================        sccp_channelstatereason === */


/* = Begin =======================================================================================                  sccp_earlyrtp === */

/*
 * \brief enum sccp_earlyrtp
 */
typedef enum sccp_earlyrtp {
	SCCP_EARLYRTP_IMMEDIATE,
	SCCP_EARLYRTP_OFFHOOK,
	SCCP_EARLYRTP_DIALING,
	SCCP_EARLYRTP_RINGOUT,
	SCCP_EARLYRTP_PROGRESS,
	SCCP_EARLYRTP_NONE,
	SCCP_EARLYRTP_SENTINEL
} sccp_earlyrtp_t;
int sccp_earlyrtp_exists(int sccp_earlyrtp_int_value);
const char * sccp_earlyrtp2str(sccp_earlyrtp_t enum_value);
sccp_earlyrtp_t sccp_earlyrtp_str2val(const char *lookup_str);
int sccp_earlyrtp_str2intval(const char *lookup_str);
char *sccp_earlyrtp_all_entries(void);
/* = End =========================================================================================                  sccp_earlyrtp === */


/* = Begin =======================================================================================               sccp_devicestate === */

/*
 * \brief enum sccp_devicestate
 */
typedef enum sccp_devicestate {
	SCCP_DEVICESTATE_ONHOOK,
	SCCP_DEVICESTATE_OFFHOOK,
	SCCP_DEVICESTATE_UNAVAILABLE,
	SCCP_DEVICESTATE_DND,
	SCCP_DEVICESTATE_FWDALL,
	SCCP_DEVICESTATE_SENTINEL
} sccp_devicestate_t;
int sccp_devicestate_exists(int sccp_devicestate_int_value);
const char * sccp_devicestate2str(sccp_devicestate_t enum_value);
sccp_devicestate_t sccp_devicestate_str2val(const char *lookup_str);
int sccp_devicestate_str2intval(const char *lookup_str);
char *sccp_devicestate_all_entries(void);
/* = End =========================================================================================               sccp_devicestate === */


/* = Begin =======================================================================================               sccp_callforward === */

/*
 * \brief enum sccp_callforward
 */
typedef enum sccp_callforward {
	SCCP_CFWD_NONE,
	SCCP_CFWD_ALL,
	SCCP_CFWD_BUSY,
	SCCP_CFWD_NOANSWER,
	SCCP_CALLFORWARD_SENTINEL
} sccp_callforward_t;
int sccp_callforward_exists(int sccp_callforward_int_value);
const char * sccp_callforward2str(sccp_callforward_t enum_value);
sccp_callforward_t sccp_callforward_str2val(const char *lookup_str);
int sccp_callforward_str2intval(const char *lookup_str);
char *sccp_callforward_all_entries(void);
/* = End =========================================================================================               sccp_callforward === */


/* = Begin =======================================================================================                  sccp_dtmfmode === */

/*
 * \brief enum sccp_dtmfmode
 */
typedef enum sccp_dtmfmode {
	SCCP_DTMFMODE_AUTO,
	SCCP_DTMFMODE_RFC2833,
	SCCP_DTMFMODE_SKINNY,
	SCCP_DTMFMODE_SENTINEL
} sccp_dtmfmode_t;
int sccp_dtmfmode_exists(int sccp_dtmfmode_int_value);
const char * sccp_dtmfmode2str(sccp_dtmfmode_t enum_value);
sccp_dtmfmode_t sccp_dtmfmode_str2val(const char *lookup_str);
int sccp_dtmfmode_str2intval(const char *lookup_str);
char *sccp_dtmfmode_all_entries(void);
/* = End =========================================================================================                  sccp_dtmfmode === */


/* = Begin =======================================================================================                sccp_autoanswer === */

/*
 * \brief enum sccp_autoanswer
 */
typedef enum sccp_autoanswer {
	SCCP_AUTOANSWER_NONE,
	SCCP_AUTOANSWER_1W,
	SCCP_AUTOANSWER_2W,
	SCCP_AUTOANSWER_SENTINEL
} sccp_autoanswer_t;
int sccp_autoanswer_exists(int sccp_autoanswer_int_value);
const char * sccp_autoanswer2str(sccp_autoanswer_t enum_value);
sccp_autoanswer_t sccp_autoanswer_str2val(const char *lookup_str);
int sccp_autoanswer_str2intval(const char *lookup_str);
char *sccp_autoanswer_all_entries(void);
/* = End =========================================================================================                sccp_autoanswer === */


/* = Begin =======================================================================================                   sccp_dndmode === */

/*
 * \brief enum sccp_dndmode
 */
typedef enum sccp_dndmode {
	SCCP_DNDMODE_OFF,
	SCCP_DNDMODE_REJECT,
	SCCP_DNDMODE_SILENT,
	SCCP_DNDMODE_USERDEFINED,
	SCCP_DNDMODE_SENTINEL
} sccp_dndmode_t;
int sccp_dndmode_exists(int sccp_dndmode_int_value);
const char * sccp_dndmode2str(sccp_dndmode_t enum_value);
sccp_dndmode_t sccp_dndmode_str2val(const char *lookup_str);
int sccp_dndmode_str2intval(const char *lookup_str);
char *sccp_dndmode_all_entries(void);
/* = End =========================================================================================                   sccp_dndmode === */


/* = Begin =======================================================================================                 sccp_accessory === */

/*
 * \brief enum sccp_accessory
 */
typedef enum sccp_accessory {
	SCCP_ACCESSORY_NONE,
	SCCP_ACCESSORY_HEADSET,
	SCCP_ACCESSORY_HANDSET,
	SCCP_ACCESSORY_SPEAKER,
	SCCP_ACCESSORY_SENTINEL
} sccp_accessory_t;
int sccp_accessory_exists(int sccp_accessory_int_value);
const char * sccp_accessory2str(sccp_accessory_t enum_value);
sccp_accessory_t sccp_accessory_str2val(const char *lookup_str);
int sccp_accessory_str2intval(const char *lookup_str);
char *sccp_accessory_all_entries(void);
/* = End =========================================================================================                 sccp_accessory === */


/* = Begin =======================================================================================            sccp_accessorystate === */

/*
 * \brief enum sccp_accessorystate
 */
typedef enum sccp_accessorystate {
	SCCP_ACCESSORYSTATE_NONE,
	SCCP_ACCESSORYSTATE_OFFHOOK,
	SCCP_ACCESSORYSTATE_ONHOOK,
	SCCP_ACCESSORYSTATE_SENTINEL
} sccp_accessorystate_t;
int sccp_accessorystate_exists(int sccp_accessorystate_int_value);
const char * sccp_accessorystate2str(sccp_accessorystate_t enum_value);
sccp_accessorystate_t sccp_accessorystate_str2val(const char *lookup_str);
int sccp_accessorystate_str2intval(const char *lookup_str);
char *sccp_accessorystate_all_entries(void);
/* = End =========================================================================================            sccp_accessorystate === */


/* = Begin =======================================================================================         sccp_config_buttontype === */

/*
 * \brief enum sccp_config_buttontype
 */
typedef enum sccp_config_buttontype {
	LINE,
	SPEEDDIAL,
	SERVICE,
	FEATURE,
	EMPTY,
	SCCP_CONFIG_BUTTONTYPE_SENTINEL
} sccp_config_buttontype_t;
int sccp_config_buttontype_exists(int sccp_config_buttontype_int_value);
const char * sccp_config_buttontype2str(sccp_config_buttontype_t enum_value);
sccp_config_buttontype_t sccp_config_buttontype_str2val(const char *lookup_str);
int sccp_config_buttontype_str2intval(const char *lookup_str);
char *sccp_config_buttontype_all_entries(void);
/* = End =========================================================================================         sccp_config_buttontype === */


/* = Begin =======================================================================================            sccp_devstate_state === */

/*
 * \brief enum sccp_devstate_state
 */
typedef enum sccp_devstate_state {
	SCCP_DEVSTATE_IDLE=0,
	SCCP_DEVSTATE_INUSE=1,
	SCCP_DEVSTATE_STATE_SENTINEL
} sccp_devstate_state_t;
int sccp_devstate_state_exists(int sccp_devstate_state_int_value);
const char * sccp_devstate_state2str(sccp_devstate_state_t enum_value);
sccp_devstate_state_t sccp_devstate_state_str2val(const char *lookup_str);
int sccp_devstate_state_str2intval(const char *lookup_str);
char *sccp_devstate_state_all_entries(void);
/* = End =========================================================================================            sccp_devstate_state === */


/* = Begin =======================================================================================   sccp_blindtransferindication === */

/*
 * \brief enum sccp_blindtransferindication
 */
typedef enum sccp_blindtransferindication {
	SCCP_BLINDTRANSFER_RING=0,
	SCCP_BLINDTRANSFER_MOH,
	SCCP_BLINDTRANSFERINDICATION_SENTINEL
} sccp_blindtransferindication_t;
int sccp_blindtransferindication_exists(int sccp_blindtransferindication_int_value);
const char * sccp_blindtransferindication2str(sccp_blindtransferindication_t enum_value);
sccp_blindtransferindication_t sccp_blindtransferindication_str2val(const char *lookup_str);
int sccp_blindtransferindication_str2intval(const char *lookup_str);
char *sccp_blindtransferindication_all_entries(void);
/* = End =========================================================================================   sccp_blindtransferindication === */


/* = Begin =======================================================================================         sccp_call_answer_order === */

/*
 * \brief enum sccp_call_answer_order
 */
typedef enum sccp_call_answer_order {
	SCCP_ANSWER_OLDEST_FIRST=0,
	SCCP_ANSWER_LAST_FIRST,
	SCCP_CALL_ANSWER_ORDER_SENTINEL
} sccp_call_answer_order_t;
int sccp_call_answer_order_exists(int sccp_call_answer_order_int_value);
const char * sccp_call_answer_order2str(sccp_call_answer_order_t enum_value);
sccp_call_answer_order_t sccp_call_answer_order_str2val(const char *lookup_str);
int sccp_call_answer_order_str2intval(const char *lookup_str);
char *sccp_call_answer_order_all_entries(void);
/* = End =========================================================================================         sccp_call_answer_order === */


/* = Begin =======================================================================================                       sccp_nat === */

/*
 * \brief enum sccp_nat
 */
typedef enum sccp_nat {
	SCCP_NAT_AUTO=0,
	SCCP_NAT_OFF,
	SCCP_NAT_AUTO_OFF,
	SCCP_NAT_ON,
	SCCP_NAT_AUTO_ON,
	SCCP_NAT_SENTINEL
} sccp_nat_t;
int sccp_nat_exists(int sccp_nat_int_value);
const char * sccp_nat2str(sccp_nat_t enum_value);
sccp_nat_t sccp_nat_str2val(const char *lookup_str);
int sccp_nat_str2intval(const char *lookup_str);
char *sccp_nat_all_entries(void);
/* = End =========================================================================================                       sccp_nat === */


/* = Begin =======================================================================================                sccp_video_mode === */

/*
 * \brief enum sccp_video_mode
 */
typedef enum sccp_video_mode {
	SCCP_VIDEO_MODE_OFF=0,
	SCCP_VIDEO_MODE_USER,
	SCCP_VIDEO_MODE_AUTO,
	SCCP_VIDEO_MODE_SENTINEL
} sccp_video_mode_t;
int sccp_video_mode_exists(int sccp_video_mode_int_value);
const char * sccp_video_mode2str(sccp_video_mode_t enum_value);
sccp_video_mode_t sccp_video_mode_str2val(const char *lookup_str);
int sccp_video_mode_str2intval(const char *lookup_str);
char *sccp_video_mode_all_entries(void);
/* = End =========================================================================================                sccp_video_mode === */


/* = Begin =======================================================================================                sccp_event_type === */

/*
 * \brief enum sccp_event_type
 */
typedef enum sccp_event_type {
	SCCP_EVENT_LINE_CREATED=1<<0,
	SCCP_EVENT_LINE_CHANGED=1<<1,
	SCCP_EVENT_LINE_DELETED=1<<2,
	SCCP_EVENT_DEVICE_ATTACHED=1<<3,
	SCCP_EVENT_DEVICE_DETACHED=1<<4,
	SCCP_EVENT_DEVICE_PREREGISTERED=1<<5,
	SCCP_EVENT_DEVICE_REGISTERED=1<<6,
	SCCP_EVENT_DEVICE_UNREGISTERED=1<<7,
	SCCP_EVENT_FEATURE_CHANGED=1<<8,
	SCCP_EVENT_LINESTATUS_CHANGED=1<<9,
	SCCP_EVENT_TYPE_SENTINEL = 1<<10
} sccp_event_type_t;
int sccp_event_type_exists(int sccp_event_type_int_value);
const char * sccp_event_type2str(int sccp_event_type_int_value);
sccp_event_type_t sccp_event_type_str2val(const char *lookup_str);
int sccp_event_type_str2intval(const char *lookup_str);
char *sccp_event_type_all_entries(void);
/* = End =========================================================================================                sccp_event_type === */


/* = Begin =======================================================================================                sccp_parkresult === */

/*
 * \brief enum sccp_parkresult
 */
typedef enum sccp_parkresult {
	PARK_RESULT_FAIL,
	PARK_RESULT_SUCCESS,
	SCCP_PARKRESULT_SENTINEL
} sccp_parkresult_t;
int sccp_parkresult_exists(int sccp_parkresult_int_value);
const char * sccp_parkresult2str(sccp_parkresult_t enum_value);
sccp_parkresult_t sccp_parkresult_str2val(const char *lookup_str);
int sccp_parkresult_str2intval(const char *lookup_str);
char *sccp_parkresult_all_entries(void);
/* = End =========================================================================================                sccp_parkresult === */


/* = Begin =======================================================================================     sccp_callerid_presentation === */

/*
 * \brief enum sccp_callerid_presentation
 */
typedef enum sccp_callerid_presentation {
	CALLERID_PRESENTATION_FORBIDDEN,
	CALLERID_PRESENTATION_ALLOWED,
	SCCP_CALLERID_PRESENTATION_SENTINEL
} sccp_callerid_presentation_t;
int sccp_callerid_presentation_exists(int sccp_callerid_presentation_int_value);
const char * sccp_callerid_presentation2str(sccp_callerid_presentation_t enum_value);
sccp_callerid_presentation_t sccp_callerid_presentation_str2val(const char *lookup_str);
int sccp_callerid_presentation_str2intval(const char *lookup_str);
char *sccp_callerid_presentation_all_entries(void);
/* = End =========================================================================================     sccp_callerid_presentation === */


/* = Begin =======================================================================================                sccp_rtp_status === */

/*
 * \brief enum sccp_rtp_status
 */
typedef enum sccp_rtp_status {
	SCCP_RTP_STATUS_INACTIVE=0,
	SCCP_RTP_STATUS_PROGRESS=1<<0,
	SCCP_RTP_STATUS_ACTIVE=1<<1,
	SCCP_RTP_STATUS_SENTINEL = 1<<2
} sccp_rtp_status_t;
int sccp_rtp_status_exists(int sccp_rtp_status_int_value);
const char * sccp_rtp_status2str(int sccp_rtp_status_int_value);
sccp_rtp_status_t sccp_rtp_status_str2val(const char *lookup_str);
int sccp_rtp_status_str2intval(const char *lookup_str);
char *sccp_rtp_status_all_entries(void);
/* = End =========================================================================================                sccp_rtp_status === */


/* = Begin =======================================================================================             sccp_sccp_rtp_type === */

/*
 * \brief enum sccp_sccp_rtp_type
 */
typedef enum sccp_sccp_rtp_type {
	SCCP_RTP_AUDIO=0,
	SCCP_RTP_VIDEO=1<<0,
	SCCP_RTP_TEXT=1<<2,
	SCCP_SCCP_RTP_TYPE_SENTINEL = 1<<3
} sccp_sccp_rtp_type_t;
int sccp_sccp_rtp_type_exists(int sccp_sccp_rtp_type_int_value);
const char * sccp_sccp_rtp_type2str(int sccp_sccp_rtp_type_int_value);
sccp_sccp_rtp_type_t sccp_sccp_rtp_type_str2val(const char *lookup_str);
int sccp_sccp_rtp_type_str2intval(const char *lookup_str);
char *sccp_sccp_rtp_type_all_entries(void);
/* = End =========================================================================================             sccp_sccp_rtp_type === */


/* = Begin =======================================================================================          sccp_extension_status === */

/*
 * \brief enum sccp_extension_status
 */
typedef enum sccp_extension_status {
	SCCP_EXTENSION_NOTEXISTS,
	SCCP_EXTENSION_MATCHMORE,
	SCCP_EXTENSION_EXACTMATCH,
	SCCP_EXTENSION_STATUS_SENTINEL
} sccp_extension_status_t;
int sccp_extension_status_exists(int sccp_extension_status_int_value);
const char * sccp_extension_status2str(sccp_extension_status_t enum_value);
sccp_extension_status_t sccp_extension_status_str2val(const char *lookup_str);
int sccp_extension_status_str2intval(const char *lookup_str);
char *sccp_extension_status_all_entries(void);
/* = End =========================================================================================          sccp_extension_status === */


/* = Begin =======================================================================================    sccp_channel_request_status === */

/*
 * \brief enum sccp_channel_request_status
 */
typedef enum sccp_channel_request_status {
	SCCP_REQUEST_STATUS_ERROR,
	SCCP_REQUEST_STATUS_LINEUNKNOWN,
	SCCP_REQUEST_STATUS_LINEUNAVAIL,
	SCCP_REQUEST_STATUS_SUCCESS,
	SCCP_CHANNEL_REQUEST_STATUS_SENTINEL
} sccp_channel_request_status_t;
int sccp_channel_request_status_exists(int sccp_channel_request_status_int_value);
const char * sccp_channel_request_status2str(sccp_channel_request_status_t enum_value);
sccp_channel_request_status_t sccp_channel_request_status_str2val(const char *lookup_str);
int sccp_channel_request_status_str2intval(const char *lookup_str);
char *sccp_channel_request_status_all_entries(void);
/* = End =========================================================================================    sccp_channel_request_status === */


/* = Begin =======================================================================================          sccp_message_priority === */

/*
 * \brief enum sccp_message_priority
 */
typedef enum sccp_message_priority {
	SCCP_MESSAGE_PRIORITY_IDLE,
	SCCP_MESSAGE_PRIORITY_VOICEMAIL,
	SCCP_MESSAGE_PRIORITY_MONITOR,
	SCCP_MESSAGE_PRIORITY_PRIVACY,
	SCCP_MESSAGE_PRIORITY_DND,
	SCCP_MESSAGE_PRIORITY_CFWD,
	SCCP_MESSAGE_PRIORITY_SENTINEL
} sccp_message_priority_t;
int sccp_message_priority_exists(int sccp_message_priority_int_value);
const char * sccp_message_priority2str(sccp_message_priority_t enum_value);
sccp_message_priority_t sccp_message_priority_str2val(const char *lookup_str);
int sccp_message_priority_str2intval(const char *lookup_str);
char *sccp_message_priority_all_entries(void);
/* = End =========================================================================================          sccp_message_priority === */


/* = Begin =======================================================================================               sccp_push_result === */

/*
 * \brief enum sccp_push_result
 */
typedef enum sccp_push_result {
	SCCP_PUSH_RESULT_FAIL,
	SCCP_PUSH_RESULT_NOT_SUPPORTED,
	SCCP_PUSH_RESULT_SUCCESS,
	SCCP_PUSH_RESULT_SENTINEL
} sccp_push_result_t;
int sccp_push_result_exists(int sccp_push_result_int_value);
const char * sccp_push_result2str(sccp_push_result_t enum_value);
sccp_push_result_t sccp_push_result_str2val(const char *lookup_str);
int sccp_push_result_str2intval(const char *lookup_str);
char *sccp_push_result_all_entries(void);
/* = End =========================================================================================               sccp_push_result === */


/* = Begin =======================================================================================                sccp_tokenstate === */

/*
 * \brief enum sccp_tokenstate
 */
typedef enum sccp_tokenstate {
	SCCP_TOKEN_STATE_NOTOKEN,
	SCCP_TOKEN_STATE_ACK,
	SCCP_TOKEN_STATE_REJ,
	SCCP_TOKENSTATE_SENTINEL
} sccp_tokenstate_t;
int sccp_tokenstate_exists(int sccp_tokenstate_int_value);
const char * sccp_tokenstate2str(sccp_tokenstate_t enum_value);
sccp_tokenstate_t sccp_tokenstate_str2val(const char *lookup_str);
int sccp_tokenstate_str2intval(const char *lookup_str);
char *sccp_tokenstate_all_entries(void);
/* = End =========================================================================================                sccp_tokenstate === */


/* = Begin =======================================================================================                sccp_softswitch === */

/*
 * \brief enum sccp_softswitch
 */
typedef enum sccp_softswitch {
	SCCP_SOFTSWITCH_DIAL,
	SCCP_SOFTSWITCH_GETFORWARDEXTEN,
#ifdef CS_SCCP_PICKUP
	SCCP_SOFTSWITCH_GETPICKUPEXTEN,
#endif
	SCCP_SOFTSWITCH_GETMEETMEROOM,
	SCCP_SOFTSWITCH_GETBARGEEXTEN,
	SCCP_SOFTSWITCH_GETCBARGEROOM,
#ifdef CS_SCCP_CONFERENCE
	SCCP_SOFTSWITCH_GETCONFERENCEROOM,
#endif
	SCCP_SOFTSWITCH_SENTINEL
} sccp_softswitch_t;
int sccp_softswitch_exists(int sccp_softswitch_int_value);
const char * sccp_softswitch2str(sccp_softswitch_t enum_value);
sccp_softswitch_t sccp_softswitch_str2val(const char *lookup_str);
int sccp_softswitch_str2intval(const char *lookup_str);
char *sccp_softswitch_all_entries(void);
/* = End =========================================================================================                sccp_softswitch === */


/* = Begin =======================================================================================                 sccp_phonebook === */

/*
 * \brief enum sccp_phonebook
 */
typedef enum sccp_phonebook {
	SCCP_PHONEBOOK_NONE,
	SCCP_PHONEBOOK_MISSED,
	SCCP_PHONEBOOK_RECEIVED,
	SCCP_PHONEBOOK_SENTINEL
} sccp_phonebook_t;
int sccp_phonebook_exists(int sccp_phonebook_int_value);
const char * sccp_phonebook2str(sccp_phonebook_t enum_value);
sccp_phonebook_t sccp_phonebook_str2val(const char *lookup_str);
int sccp_phonebook_str2intval(const char *lookup_str);
char *sccp_phonebook_all_entries(void);
/* = End =========================================================================================                 sccp_phonebook === */


/* = Begin =======================================================================================     sccp_feature_monitor_state === */

/*
 * \brief enum sccp_feature_monitor_state
 */
typedef enum sccp_feature_monitor_state {
	SCCP_FEATURE_MONITOR_STATE_DISABLED,
	SCCP_FEATURE_MONITOR_STATE_ACTIVE,
	SCCP_FEATURE_MONITOR_STATE_REQUESTED,
	SCCP_FEATURE_MONITOR_STATE_SENTINEL
} sccp_feature_monitor_state_t;
int sccp_feature_monitor_state_exists(int sccp_feature_monitor_state_int_value);
const char * sccp_feature_monitor_state2str(sccp_feature_monitor_state_t enum_value);
sccp_feature_monitor_state_t sccp_feature_monitor_state_str2val(const char *lookup_str);
int sccp_feature_monitor_state_str2intval(const char *lookup_str);
char *sccp_feature_monitor_state_all_entries(void);
/* = End =========================================================================================     sccp_feature_monitor_state === */


/* = Begin =======================================================================================               sccp_readingtype === */

/*
 * \brief enum sccp_readingtype
 */
typedef enum sccp_readingtype {
	SCCP_CONFIG_READINITIAL,
	SCCP_CONFIG_READRELOAD,
	SCCP_READINGTYPE_SENTINEL
} sccp_readingtype_t;
int sccp_readingtype_exists(int sccp_readingtype_int_value);
const char * sccp_readingtype2str(sccp_readingtype_t enum_value);
sccp_readingtype_t sccp_readingtype_str2val(const char *lookup_str);
int sccp_readingtype_str2intval(const char *lookup_str);
char *sccp_readingtype_all_entries(void);
/* = End =========================================================================================               sccp_readingtype === */


/* = Begin =======================================================================================       sccp_configurationchange === */

/*
 * \brief enum sccp_configurationchange
 */
typedef enum sccp_configurationchange {
	SCCP_CONFIG_NOUPDATENEEDED=0,
	SCCP_CONFIG_NEEDDEVICERESET=1<<1,
	SCCP_CONFIG_WARNING=1<<2,
	SCCP_CONFIG_ERROR=1<<3,
	SCCP_CONFIGURATIONCHANGE_SENTINEL = 1<<4
} sccp_configurationchange_t;
int sccp_configurationchange_exists(int sccp_configurationchange_int_value);
const char * sccp_configurationchange2str(int sccp_configurationchange_int_value);
sccp_configurationchange_t sccp_configurationchange_str2val(const char *lookup_str);
int sccp_configurationchange_str2intval(const char *lookup_str);
char *sccp_configurationchange_all_entries(void);
/* = End =========================================================================================       sccp_configurationchange === */


/* = Begin =======================================================================================      sccp_call_statistics_type === */

/*
 * \brief enum sccp_call_statistics_type
 */
typedef enum sccp_call_statistics_type {
	SCCP_CALLSTATISTIC_LAST,
	SCCP_CALLSTATISTIC_AVG,
	SCCP_CALL_STATISTICS_TYPE_SENTINEL
} sccp_call_statistics_type_t;
int sccp_call_statistics_type_exists(int sccp_call_statistics_type_int_value);
const char * sccp_call_statistics_type2str(sccp_call_statistics_type_t enum_value);
sccp_call_statistics_type_t sccp_call_statistics_type_str2val(const char *lookup_str);
int sccp_call_statistics_type_str2intval(const char *lookup_str);
char *sccp_call_statistics_type_all_entries(void);
/* = End =========================================================================================      sccp_call_statistics_type === */


/* = Begin =======================================================================================                  sccp_rtp_info === */

/*
 * \brief enum sccp_rtp_info
 */
typedef enum sccp_rtp_info {
	SCCP_RTP_INFO_NORTP=0,
	SCCP_RTP_INFO_AVAILABLE=1<<1,
	SCCP_RTP_INFO_ALLOW_DIRECTRTP=1<<2,
	SCCP_RTP_INFO_SENTINEL = 1<<3
} sccp_rtp_info_t;
int sccp_rtp_info_exists(int sccp_rtp_info_int_value);
const char * sccp_rtp_info2str(int sccp_rtp_info_int_value);
sccp_rtp_info_t sccp_rtp_info_str2val(const char *lookup_str);
int sccp_rtp_info_str2intval(const char *lookup_str);
char *sccp_rtp_info_all_entries(void);
/* = End =========================================================================================                  sccp_rtp_info === */


/* = Begin =======================================================================================              sccp_feature_type === */

/*
 * \brief enum sccp_feature_type
 */
typedef enum sccp_feature_type {
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
	SCCP_FEATURE_TESTI,
	SCCP_FEATURE_TESTG,
	SCCP_FEATURE_TESTH,
	SCCP_FEATURE_TESTJ,
#ifdef CS_DEVSTATE_FEATURE
	SCCP_FEATURE_DEVSTATE,
#endif
	SCCP_FEATURE_PICKUP,
	SCCP_FEATURE_TYPE_SENTINEL
} sccp_feature_type_t;
int sccp_feature_type_exists(int sccp_feature_type_int_value);
const char * sccp_feature_type2str(sccp_feature_type_t enum_value);
sccp_feature_type_t sccp_feature_type_str2val(const char *lookup_str);
int sccp_feature_type_str2intval(const char *lookup_str);
char *sccp_feature_type_all_entries(void);
/* = End =========================================================================================              sccp_feature_type === */


/* = Begin =======================================================================================              sccp_callinfo_key === */

/*
 * \brief enum sccp_callinfo_key
 */
typedef enum sccp_callinfo_key {
	SCCP_CALLINFO_NONE=0,
	SCCP_CALLINFO_CALLEDPARTY_NAME,
	SCCP_CALLINFO_CALLEDPARTY_NUMBER,
	SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL,
	SCCP_CALLINFO_CALLINGPARTY_NAME,
	SCCP_CALLINFO_CALLINGPARTY_NUMBER,
	SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL,
	SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME,
	SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER,
	SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL,
	SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME,
	SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER,
	SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME,
	SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER,
	SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL,
	SCCP_CALLINFO_HUNT_PILOT_NAME,
	SCCP_CALLINFO_HUNT_PILOT_NUMBER,
	SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON,
	SCCP_CALLINFO_LAST_REDIRECT_REASON,
	SCCP_CALLINFO_PRESENTATION,
	SCCP_CALLINFO_KEY_SENTINEL
} sccp_callinfo_key_t;
int sccp_callinfo_key_exists(int sccp_callinfo_key_int_value);
const char * sccp_callinfo_key2str(sccp_callinfo_key_t enum_value);
sccp_callinfo_key_t sccp_callinfo_key_str2val(const char *lookup_str);
int sccp_callinfo_key_str2intval(const char *lookup_str);
char *sccp_callinfo_key_all_entries(void);
/* = End =========================================================================================              sccp_callinfo_key === */


/* = Begin =======================================================================================                skinny_lampmode === */

/*
 * \brief enum skinny_lampmode
 */
typedef enum skinny_lampmode {
	SKINNY_LAMP_OFF=1,
	SKINNY_LAMP_ON,
	SKINNY_LAMP_WINK,
	SKINNY_LAMP_FLASH,
	SKINNY_LAMP_BLINK,
	SKINNY_LAMPMODE_SENTINEL
} skinny_lampmode_t;
int skinny_lampmode_exists(int skinny_lampmode_int_value);
const char * skinny_lampmode2str(skinny_lampmode_t enum_value);
skinny_lampmode_t skinny_lampmode_str2val(const char *lookup_str);
int skinny_lampmode_str2intval(const char *lookup_str);
char *skinny_lampmode_all_entries(void);
/* = End =========================================================================================                skinny_lampmode === */


/* = Begin =======================================================================================                skinny_calltype === */

/*
 * \brief enum skinny_calltype
 */
typedef enum skinny_calltype {
	SKINNY_CALLTYPE_INBOUND=1,
	SKINNY_CALLTYPE_OUTBOUND,
	SKINNY_CALLTYPE_FORWARD,
	SKINNY_CALLTYPE_SENTINEL
} skinny_calltype_t;
int skinny_calltype_exists(int skinny_calltype_int_value);
const char * skinny_calltype2str(skinny_calltype_t enum_value);
skinny_calltype_t skinny_calltype_str2val(const char *lookup_str);
int skinny_calltype_str2intval(const char *lookup_str);
char *skinny_calltype_all_entries(void);
/* = End =========================================================================================                skinny_calltype === */


/* = Begin =======================================================================================               skinny_callstate === */

/*
 * \brief enum skinny_callstate
 */
typedef enum skinny_callstate {
	SKINNY_CALLSTATE_OFFHOOK=1,
	SKINNY_CALLSTATE_ONHOOK,
	SKINNY_CALLSTATE_RINGOUT,
	SKINNY_CALLSTATE_RINGIN,
	SKINNY_CALLSTATE_CONNECTED,
	SKINNY_CALLSTATE_BUSY,
	SKINNY_CALLSTATE_CONGESTION,
	SKINNY_CALLSTATE_HOLD,
	SKINNY_CALLSTATE_CALLWAITING,
	SKINNY_CALLSTATE_CALLTRANSFER,
	SKINNY_CALLSTATE_CALLPARK,
	SKINNY_CALLSTATE_PROCEED,
	SKINNY_CALLSTATE_CALLREMOTEMULTILINE,
	SKINNY_CALLSTATE_INVALIDNUMBER,
	SKINNY_CALLSTATE_HOLDYELLOW,
	SKINNY_CALLSTATE_INTERCOMONEWAY,
	SKINNY_CALLSTATE_HOLDRED,
	SKINNY_CALLSTATE_SENTINEL
} skinny_callstate_t;
int skinny_callstate_exists(int skinny_callstate_int_value);
const char * skinny_callstate2str(skinny_callstate_t enum_value);
skinny_callstate_t skinny_callstate_str2val(const char *lookup_str);
int skinny_callstate_str2intval(const char *lookup_str);
char *skinny_callstate_all_entries(void);
/* = End =========================================================================================               skinny_callstate === */


/* = Begin =======================================================================================            skinny_callpriority === */

/*
 * \brief enum skinny_callpriority
 */
typedef enum skinny_callpriority {
	SKINNY_CALLPRIORITY_HIGHEST,
	SKINNY_CALLPRIORITY_HIGH,
	SKINNY_CALLPRIORITY_MEDIUM,
	SKINNY_CALLPRIORITY_LOW,
	SKINNY_CALLPRIORITY_NORMAL,
	SKINNY_CALLPRIORITY_SENTINEL
} skinny_callpriority_t;
int skinny_callpriority_exists(int skinny_callpriority_int_value);
const char * skinny_callpriority2str(skinny_callpriority_t enum_value);
skinny_callpriority_t skinny_callpriority_str2val(const char *lookup_str);
int skinny_callpriority_str2intval(const char *lookup_str);
char *skinny_callpriority_all_entries(void);
/* = End =========================================================================================            skinny_callpriority === */


/* = Begin =======================================================================================     skinny_callinfo_visibility === */

/*
 * \brief enum skinny_callinfo_visibility
 */
typedef enum skinny_callinfo_visibility {
	SKINNY_CALLINFO_VISIBILITY_DEFAULT,
	SKINNY_CALLINFO_VISIBILITY_COLLAPSED,
	SKINNY_CALLINFO_VISIBILITY_HIDDEN,
	SKINNY_CALLINFO_VISIBILITY_SENTINEL
} skinny_callinfo_visibility_t;
int skinny_callinfo_visibility_exists(int skinny_callinfo_visibility_int_value);
const char * skinny_callinfo_visibility2str(skinny_callinfo_visibility_t enum_value);
skinny_callinfo_visibility_t skinny_callinfo_visibility_str2val(const char *lookup_str);
int skinny_callinfo_visibility_str2intval(const char *lookup_str);
char *skinny_callinfo_visibility_all_entries(void);
/* = End =========================================================================================     skinny_callinfo_visibility === */


/* = Begin =======================================================================================       skinny_callsecuritystate === */

/*
 * \brief enum skinny_callsecuritystate
 */
typedef enum skinny_callsecuritystate {
	SKINNY_CALLSECURITYSTATE_UNKNOWN,
	SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED,
	SKINNY_CALLSECURITYSTATE_AUTHENTICATED,
	SKINNY_CALLSECURITYSTATE_SENTINEL
} skinny_callsecuritystate_t;
int skinny_callsecuritystate_exists(int skinny_callsecuritystate_int_value);
const char * skinny_callsecuritystate2str(skinny_callsecuritystate_t enum_value);
skinny_callsecuritystate_t skinny_callsecuritystate_str2val(const char *lookup_str);
int skinny_callsecuritystate_str2intval(const char *lookup_str);
char *skinny_callsecuritystate_all_entries(void);
/* = End =========================================================================================       skinny_callsecuritystate === */


/* = Begin =======================================================================================     skinny_busylampfield_state === */

/*
 * \brief enum skinny_busylampfield_state
 */
typedef enum skinny_busylampfield_state {
	SKINNY_BLF_STATUS_UNKNOWN,
	SKINNY_BLF_STATUS_IDLE,
	SKINNY_BLF_STATUS_INUSE,
	SKINNY_BLF_STATUS_DND,
	SKINNY_BLF_STATUS_ALERTING,
	SKINNY_BUSYLAMPFIELD_STATE_SENTINEL
} skinny_busylampfield_state_t;
int skinny_busylampfield_state_exists(int skinny_busylampfield_state_int_value);
const char * skinny_busylampfield_state2str(skinny_busylampfield_state_t enum_value);
skinny_busylampfield_state_t skinny_busylampfield_state_str2val(const char *lookup_str);
int skinny_busylampfield_state_str2intval(const char *lookup_str);
char *skinny_busylampfield_state_all_entries(void);
/* = End =========================================================================================     skinny_busylampfield_state === */


/* = Begin =======================================================================================            sparse skinny_alarm === */

/*
 * \brief sparse enum skinny_alarm
 */
typedef enum skinny_alarm {
	SKINNY_ALARM_CRITICAL=0,
	SKINNY_ALARM_WARNING=1,
	SKINNY_ALARM_INFORMATIONAL=2,
	SKINNY_ALARM_UNKNOWN=4,
	SKINNY_ALARM_MAJOR=7,
	SKINNY_ALARM_MINOR=8,
	SKINNY_ALARM_MARGINAL=10,
	SKINNY_ALARM_TRACEINFO=20,
	SKINNY_ALARM_SENTINEL
} skinny_alarm_t;
int skinny_alarm_exists(int skinny_alarm_int_value);
const char * skinny_alarm2str(skinny_alarm_t enum_value);
skinny_alarm_t skinny_alarm_str2val(const char *lookup_str);
int skinny_alarm_str2intval(const char *lookup_str);
char *skinny_alarm_all_entries(void);
/* = End =========================================================================================            sparse skinny_alarm === */


/* = Begin =======================================================================================             sparse skinny_tone === */

/*
 * \brief sparse enum skinny_tone
 */
typedef enum skinny_tone {
	SKINNY_TONE_SILENCE=0x00,
	SKINNY_TONE_DTMF1=0x01,
	SKINNY_TONE_DTMF2=0x02,
	SKINNY_TONE_DTMF3=0x03,
	SKINNY_TONE_DTMF4=0x04,
	SKINNY_TONE_DTMF5=0x05,
	SKINNY_TONE_DTMF6=0x06,
	SKINNY_TONE_DTMF7=0x07,
	SKINNY_TONE_DTMF8=0x08,
	SKINNY_TONE_DTMF9=0x09,
	SKINNY_TONE_DTMF0=0x0A,
	SKINNY_TONE_DTMFSTAR=0x0E,
	SKINNY_TONE_DTMFPOUND=0x0F,
	SKINNY_TONE_DTMFA=0x10,
	SKINNY_TONE_DTMFB=0x11,
	SKINNY_TONE_DTMFC=0x12,
	SKINNY_TONE_DTMFD=0x13,
	SKINNY_TONE_INSIDEDIALTONE=0x21,
	SKINNY_TONE_OUTSIDEDIALTONE=0x22,
	SKINNY_TONE_LINEBUSYTONE=0x23,
	SKINNY_TONE_ALERTINGTONE=0x24,
	SKINNY_TONE_REORDERTONE=0x25,
	SKINNY_TONE_RECORDERWARNINGTONE=0x26,
	SKINNY_TONE_RECORDERDETECTEDTONE=0x27,
	SKINNY_TONE_REVERTINGTONE=0x28,
	SKINNY_TONE_RECEIVEROFFHOOKTONE=0x29,
	SKINNY_TONE_PARTIALDIALTONE=0x2A,
	SKINNY_TONE_NOSUCHNUMBERTONE=0x2B,
	SKINNY_TONE_BUSYVERIFICATIONTONE=0x2C,
	SKINNY_TONE_CALLWAITINGTONE=0x2D,
	SKINNY_TONE_CONFIRMATIONTONE=0x2E,
	SKINNY_TONE_CAMPONINDICATIONTONE=0x2F,
	SKINNY_TONE_RECALLDIALTONE=0x30,
	SKINNY_TONE_ZIPZIP=0x31,
	SKINNY_TONE_ZIP=0x32,
	SKINNY_TONE_BEEPBONK=0x33,
	SKINNY_TONE_MUSICTONE=0x34,
	SKINNY_TONE_HOLDTONE=0x35,
	SKINNY_TONE_TESTTONE=0x36,
	SKINNY_TONE_DTMONITORWARNINGTONE=0x37,
	SKINNY_TONE_ADDCALLWAITING=0x40,
	SKINNY_TONE_PRIORITYCALLWAIT=0x41,
	SKINNY_TONE_RECALLDIAL=0x42,
	SKINNY_TONE_BARGIN=0x43,
	SKINNY_TONE_DISTINCTALERT=0x44,
	SKINNY_TONE_PRIORITYALERT=0x45,
	SKINNY_TONE_REMINDERRING=0x46,
	SKINNY_TONE_PRECEDENCE_RINGBACK=0x47,
	SKINNY_TONE_PREEMPTIONTONE=0x48,
	SKINNY_TONE_MF1=0x50,
	SKINNY_TONE_MF2=0x51,
	SKINNY_TONE_MF3=0x52,
	SKINNY_TONE_MF4=0x53,
	SKINNY_TONE_MF5=0x54,
	SKINNY_TONE_MF6=0x55,
	SKINNY_TONE_MF7=0x56,
	SKINNY_TONE_MF8=0x57,
	SKINNY_TONE_MF9=0x58,
	SKINNY_TONE_MF0=0x59,
	SKINNY_TONE_MFKP1=0x5A,
	SKINNY_TONE_MFST=0x5B,
	SKINNY_TONE_MFKP2=0x5C,
	SKINNY_TONE_MFSTP=0x5D,
	SKINNY_TONE_MFST3P=0x5E,
	SKINNY_TONE_MILLIWATT=0x5F,
	SKINNY_TONE_MILLIWATTTEST=0x60,
	SKINNY_TONE_HIGHTONE=0x61,
	SKINNY_TONE_FLASHOVERRIDE=0x62,
	SKINNY_TONE_FLASH=0x63,
	SKINNY_TONE_PRIORITY=0x64,
	SKINNY_TONE_IMMEDIATE=0x65,
	SKINNY_TONE_PREAMPWARN=0x66,
	SKINNY_TONE_2105HZ=0x67,
	SKINNY_TONE_2600HZ=0x68,
	SKINNY_TONE_440HZ=0x69,
	SKINNY_TONE_300HZ=0x6A,
	SKINNY_TONE_MLPP_PALA=0x77,
	SKINNY_TONE_MLPP_ICA=0x78,
	SKINNY_TONE_MLPP_VCA=0x79,
	SKINNY_TONE_MLPP_BPA=0x7A,
	SKINNY_TONE_MLPP_BNEA=0x7B,
	SKINNY_TONE_MLPP_UPA=0x7C,
	SKINNY_TONE_NOTONE=0x7F,
	SKINNY_TONE_MEETME_GREETING=0x80,
	SKINNY_TONE_MEETME_NUMBER_INVALID=0x81,
	SKINNY_TONE_MEETME_NUMBER_FAILED=0x82,
	SKINNY_TONE_MEETME_ENTER_PIN=0x83,
	SKINNY_TONE_MEETME_INVALID_PIN=0x84,
	SKINNY_TONE_MEETME_FAILED_PIN=0x85,
	SKINNY_TONE_MEETME_CFB_FAILED=0x86,
	SKINNY_TONE_MEETME_ENTER_ACCESS_CODE=0x87,
	SKINNY_TONE_MEETME_ACCESS_CODE_INVALID=0x88,
	SKINNY_TONE_MEETME_ACCESS_CODE_FAILED=0x89,
	SKINNY_TONE_SENTINEL
} skinny_tone_t;
int skinny_tone_exists(int skinny_tone_int_value);
const char * skinny_tone2str(skinny_tone_t enum_value);
skinny_tone_t skinny_tone_str2val(const char *lookup_str);
int skinny_tone_str2intval(const char *lookup_str);
char *skinny_tone_all_entries(void);
/* = End =========================================================================================             sparse skinny_tone === */


/* = Begin =======================================================================================      sparse skinny_videoformat === */

/*
 * \brief sparse enum skinny_videoformat
 */
typedef enum skinny_videoformat {
	SKINNY_VIDEOFORMAT_UNDEFINED=0,
	SKINNY_VIDEOFORMAT_SQCIF=1,
	SKINNY_VIDEOFORMAT_QCIF=2,
	SKINNY_VIDEOFORMAT_CIF=3,
	SKINNY_VIDEOFORMAT_4CIF=4,
	SKINNY_VIDEOFORMAT_16CIF=5,
	SKINNY_VIDEOFORMAT_CUSTOM=6,
	SKINNY_VIDEOFORMAT_UNKNOWN=232,
	SKINNY_VIDEOFORMAT_SENTINEL
} skinny_videoformat_t;
int skinny_videoformat_exists(int skinny_videoformat_int_value);
const char * skinny_videoformat2str(skinny_videoformat_t enum_value);
skinny_videoformat_t skinny_videoformat_str2val(const char *lookup_str);
int skinny_videoformat_str2intval(const char *lookup_str);
char *skinny_videoformat_all_entries(void);
/* = End =========================================================================================      sparse skinny_videoformat === */


/* = Begin =======================================================================================                skinny_ringtype === */

/*
 * \brief enum skinny_ringtype
 */
typedef enum skinny_ringtype {
	SKINNY_RINGTYPE_OFF=1,
	SKINNY_RINGTYPE_INSIDE,
	SKINNY_RINGTYPE_OUTSIDE,
	SKINNY_RINGTYPE_FEATURE,
	SKINNY_RINGTYPE_SILENT,
	SKINNY_RINGTYPE_URGENT,
	SKINNY_RINGTYPE_BELLCORE_1,
	SKINNY_RINGTYPE_BELLCORE_2,
	SKINNY_RINGTYPE_BELLCORE_3,
	SKINNY_RINGTYPE_BELLCORE_4,
	SKINNY_RINGTYPE_BELLCORE_5,
	SKINNY_RINGTYPE_SENTINEL
} skinny_ringtype_t;
int skinny_ringtype_exists(int skinny_ringtype_int_value);
const char * skinny_ringtype2str(skinny_ringtype_t enum_value);
skinny_ringtype_t skinny_ringtype_str2val(const char *lookup_str);
int skinny_ringtype_str2intval(const char *lookup_str);
char *skinny_ringtype_all_entries(void);
/* = End =========================================================================================                skinny_ringtype === */


/* = Begin =======================================================================================         skinny_receivetransmit === */

/*
 * \brief enum skinny_receivetransmit
 */
typedef enum skinny_receivetransmit {
	SKINNY_TRANSMITRECEIVE_NONE=0,
	SKINNY_TRANSMITRECEIVE_RECEIVE=1,
	SKINNY_TRANSMITRECEIVE_TRANSMIT=2,
	SKINNY_TRANSMITRECEIVE_BOTH=3,
	SKINNY_RECEIVETRANSMIT_SENTINEL
} skinny_receivetransmit_t;
int skinny_receivetransmit_exists(int skinny_receivetransmit_int_value);
const char * skinny_receivetransmit2str(skinny_receivetransmit_t enum_value);
skinny_receivetransmit_t skinny_receivetransmit_str2val(const char *lookup_str);
int skinny_receivetransmit_str2intval(const char *lookup_str);
char *skinny_receivetransmit_all_entries(void);
/* = End =========================================================================================         skinny_receivetransmit === */


/* = Begin =======================================================================================                 skinny_keymode === */

/*
 * \brief enum skinny_keymode
 */
typedef enum skinny_keymode {
	KEYMODE_ONHOOK,
	KEYMODE_CONNECTED,
	KEYMODE_ONHOLD,
	KEYMODE_RINGIN,
	KEYMODE_OFFHOOK,
	KEYMODE_CONNTRANS,
	KEYMODE_DIGITSFOLL,
	KEYMODE_CONNCONF,
	KEYMODE_RINGOUT,
	KEYMODE_OFFHOOKFEAT,
	KEYMODE_INUSEHINT,
	KEYMODE_ONHOOKSTEALABLE,
	KEYMODE_EMPTY,
	SKINNY_KEYMODE_SENTINEL
} skinny_keymode_t;
int skinny_keymode_exists(int skinny_keymode_int_value);
const char * skinny_keymode2str(skinny_keymode_t enum_value);
skinny_keymode_t skinny_keymode_str2val(const char *lookup_str);
int skinny_keymode_str2intval(const char *lookup_str);
char *skinny_keymode_all_entries(void);
/* = End =========================================================================================                 skinny_keymode === */


/* = Begin =======================================================================================       skinny_registrationstate === */

/*
 * \brief enum skinny_registrationstate
 */
typedef enum skinny_registrationstate {
	SKINNY_DEVICE_RS_FAILED,
	SKINNY_DEVICE_RS_TIMEOUT,
	SKINNY_DEVICE_RS_NONE,
	SKINNY_DEVICE_RS_TOKEN,
	SKINNY_DEVICE_RS_PROGRESS,
	SKINNY_DEVICE_RS_OK,
	SKINNY_REGISTRATIONSTATE_SENTINEL
} skinny_registrationstate_t;
int skinny_registrationstate_exists(int skinny_registrationstate_int_value);
const char * skinny_registrationstate2str(skinny_registrationstate_t enum_value);
skinny_registrationstate_t skinny_registrationstate_str2val(const char *lookup_str);
int skinny_registrationstate_str2intval(const char *lookup_str);
char *skinny_registrationstate_all_entries(void);
/* = End =========================================================================================       skinny_registrationstate === */


/* = Begin =======================================================================================             skinny_mediastatus === */

/*
 * \brief enum skinny_mediastatus
 */
typedef enum skinny_mediastatus {
	SKINNY_MEDIASTATUS_Ok,
	SKINNY_MEDIASTATUS_Unknown,
	SKINNY_MEDIASTATUS_OutOfChannels,
	SKINNY_MEDIASTATUS_CodecTooComplex,
	SKINNY_MEDIASTATUS_InvalidPartyId,
	SKINNY_MEDIASTATUS_InvalidCallReference,
	SKINNY_MEDIASTATUS_InvalidCodec,
	SKINNY_MEDIASTATUS_InvalidPacketSize,
	SKINNY_MEDIASTATUS_OutOfSockets,
	SKINNY_MEDIASTATUS_EncoderOrDecoderFailed,
	SKINNY_MEDIASTATUS_InvalidDynPayloadType,
	SKINNY_MEDIASTATUS_RequestedIpAddrTypeUnavailable,
	SKINNY_MEDIASTATUS_DeviceOnHook,
	SKINNY_MEDIASTATUS_SENTINEL
} skinny_mediastatus_t;
int skinny_mediastatus_exists(int skinny_mediastatus_int_value);
const char * skinny_mediastatus2str(skinny_mediastatus_t enum_value);
skinny_mediastatus_t skinny_mediastatus_str2val(const char *lookup_str);
int skinny_mediastatus_str2intval(const char *lookup_str);
char *skinny_mediastatus_all_entries(void);
/* = End =========================================================================================             skinny_mediastatus === */


/* = Begin =======================================================================================         sparse skinny_stimulus === */

/*
 * \brief sparse enum skinny_stimulus
 */
typedef enum skinny_stimulus {
	SKINNY_STIMULUS_UNUSED=0x00,
	SKINNY_STIMULUS_LASTNUMBERREDIAL=0x01,
	SKINNY_STIMULUS_SPEEDDIAL=0x02,
	SKINNY_STIMULUS_HOLD=0x03,
	SKINNY_STIMULUS_TRANSFER=0x04,
	SKINNY_STIMULUS_FORWARDALL=0x05,
	SKINNY_STIMULUS_FORWARDBUSY=0x06,
	SKINNY_STIMULUS_FORWARDNOANSWER=0x07,
	SKINNY_STIMULUS_DISPLAY=0x08,
	SKINNY_STIMULUS_LINE=0x09,
	SKINNY_STIMULUS_T120CHAT=0x0A,
	SKINNY_STIMULUS_T120WHITEBOARD=0x0B,
	SKINNY_STIMULUS_T120APPLICATIONSHARING=0x0C,
	SKINNY_STIMULUS_T120FILETRANSFER=0x0D,
	SKINNY_STIMULUS_VIDEO=0x0E,
	SKINNY_STIMULUS_VOICEMAIL=0x0F,
	SKINNY_STIMULUS_ANSWERRELEASE=0x10,
	SKINNY_STIMULUS_AUTOANSWER=0x11,
	SKINNY_STIMULUS_SELECT=0x12,
	SKINNY_STIMULUS_FEATURE=0x13,
	SKINNY_STIMULUS_SERVICEURL=0x14,
	SKINNY_STIMULUS_BLFSPEEDDIAL=0x15,
	SKINNY_STIMULUS_MALICIOUSCALL=0x1B,
	SKINNY_STIMULUS_GENERICAPPB1=0x21,
	SKINNY_STIMULUS_GENERICAPPB2=0x22,
	SKINNY_STIMULUS_GENERICAPPB3=0x23,
	SKINNY_STIMULUS_GENERICAPPB4=0x24,
	SKINNY_STIMULUS_GENERICAPPB5=0x25,
	SKINNY_STIMULUS_MULTIBLINKFEATURE=0x26,
	SKINNY_STIMULUS_MEETMECONFERENCE=0x7B,
	SKINNY_STIMULUS_CONFERENCE=0x7D,
	SKINNY_STIMULUS_CALLPARK=0x7E,
	SKINNY_STIMULUS_CALLPICKUP=0x7F,
	SKINNY_STIMULUS_GROUPCALLPICKUP=0x80,
	SKINNY_STIMULUS_MOBILITY=0x81,
	SKINNY_STIMULUS_DO_NOT_DISTURB=0x82,
	SKINNY_STIMULUS_CONF_LIST=0x83,
	SKINNY_STIMULUS_REMOVE_LAST_PARTICIPANT=0x84,
	SKINNY_STIMULUS_QRT=0x85,
	SKINNY_STIMULUS_CALLBACK=0x86,
	SKINNY_STIMULUS_OTHER_PICKUP=0x87,
	SKINNY_STIMULUS_VIDEO_MODE=0x88,
	SKINNY_STIMULUS_NEW_CALL=0x89,
	SKINNY_STIMULUS_END_CALL=0x8A,
	SKINNY_STIMULUS_HLOG=0x8B,
	SKINNY_STIMULUS_QUEUING=0x8F,
	SKINNY_STIMULUS_TESTE=0xC0,
	SKINNY_STIMULUS_TESTF=0xC1,
	SKINNY_STIMULUS_TESTI=0xC4,
	SKINNY_STIMULUS_MESSAGES=0xC2,
	SKINNY_STIMULUS_DIRECTORY=0xC3,
	SKINNY_STIMULUS_APPLICATION=0xC5,
	SKINNY_STIMULUS_HEADSET=0xC6,
	SKINNY_STIMULUS_KEYPAD=0xF0,
	SKINNY_STIMULUS_AEC=0xFD,
	SKINNY_STIMULUS_UNDEFINED=0xFF,
	SKINNY_STIMULUS_SENTINEL
} skinny_stimulus_t;
int skinny_stimulus_exists(int skinny_stimulus_int_value);
const char * skinny_stimulus2str(skinny_stimulus_t enum_value);
skinny_stimulus_t skinny_stimulus_str2val(const char *lookup_str);
int skinny_stimulus_str2intval(const char *lookup_str);
char *skinny_stimulus_all_entries(void);
/* = End =========================================================================================         sparse skinny_stimulus === */


/* = Begin =======================================================================================       sparse skinny_buttontype === */

/*
 * \brief sparse enum skinny_buttontype
 */
typedef enum skinny_buttontype {
	SKINNY_BUTTONTYPE_UNUSED=0x00,
	SKINNY_BUTTONTYPE_LASTNUMBERREDIAL=0x01,
	SKINNY_BUTTONTYPE_SPEEDDIAL=0x02,
	SKINNY_BUTTONTYPE_HOLD=0x03,
	SKINNY_BUTTONTYPE_TRANSFER=0x04,
	SKINNY_BUTTONTYPE_FORWARDALL=0x05,
	SKINNY_BUTTONTYPE_FORWARDBUSY=0x06,
	SKINNY_BUTTONTYPE_FORWARDNOANSWER=0x07,
	SKINNY_BUTTONTYPE_DISPLAY=0x08,
	SKINNY_BUTTONTYPE_LINE=0x09,
	SKINNY_BUTTONTYPE_T120CHAT=0x0A,
	SKINNY_BUTTONTYPE_T120WHITEBOARD=0x0B,
	SKINNY_BUTTONTYPE_T120APPLICATIONSHARING=0x0C,
	SKINNY_BUTTONTYPE_T120FILETRANSFER=0x0D,
	SKINNY_BUTTONTYPE_VIDEO=0x0E,
	SKINNY_BUTTONTYPE_VOICEMAIL=0x0F,
	SKINNY_BUTTONTYPE_ANSWERRELEASE=0x10,
	SKINNY_BUTTONTYPE_AUTOANSWER=0x11,
	SKINNY_BUTTONTYPE_FEATURE=0x13,
	SKINNY_BUTTONTYPE_SERVICEURL=0x14,
	SKINNY_BUTTONTYPE_BLFSPEEDDIAL=0x15,
	SKINNY_BUTTONTYPE_GENERICAPPB1=0x21,
	SKINNY_BUTTONTYPE_GENERICAPPB2=0x22,
	SKINNY_BUTTONTYPE_GENERICAPPB3=0x23,
	SKINNY_BUTTONTYPE_GENERICAPPB4=0x24,
	SKINNY_BUTTONTYPE_GENERICAPPB5=0x25,
	SKINNY_BUTTONTYPE_MULTIBLINKFEATURE=0x26,
	SKINNY_BUTTONTYPE_MEETMECONFERENCE=0x7B,
	SKINNY_BUTTONTYPE_CONFERENCE=0x7D,
	SKINNY_BUTTONTYPE_CALLPARK=0x7E,
	SKINNY_BUTTONTYPE_CALLPICKUP=0x7F,
	SKINNY_BUTTONTYPE_GROUPCALLPICKUP=0x80,
	SKINNY_BUTTONTYPE_MOBILITY=0x81,
	SKINNY_BUTTONTYPE_DO_NOT_DISTURB=0x82,
	SKINNY_BUTTONTYPE_CONF_LIST=0x83,
	SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT=0x84,
	SKINNY_BUTTONTYPE_QRT=0x85,
	SKINNY_BUTTONTYPE_CALLBACK=0x86,
	SKINNY_BUTTONTYPE_OTHER_PICKUP=0x87,
	SKINNY_BUTTONTYPE_VIDEO_MODE=0x88,
	SKINNY_BUTTONTYPE_NEW_CALL=0x89,
	SKINNY_BUTTONTYPE_END_CALL=0x8A,
	SKINNY_BUTTONTYPE_HLOG=0x8B,
	SKINNY_BUTTONTYPE_QUEUING=0x8F,
	SKINNY_BUTTONTYPE_TESTE=0xC0,
	SKINNY_BUTTONTYPE_TESTF=0xC1,
	SKINNY_BUTTONTYPE_TESTI=0xC4,
	SKINNY_BUTTONTYPE_MESSAGES=0xC2,
	SKINNY_BUTTONTYPE_DIRECTORY=0xC3,
	SKINNY_BUTTONTYPE_APPLICATION=0xC5,
	SKINNY_BUTTONTYPE_HEADSET=0xC6,
	SKINNY_BUTTONTYPE_KEYPAD=0xF0,
	SKINNY_BUTTONTYPE_AEC=0xFD,
	SKINNY_BUTTONTYPE_UNDEFINED=0xFF,
	SKINNY_BUTTONTYPE_SENTINEL
} skinny_buttontype_t;
int skinny_buttontype_exists(int skinny_buttontype_int_value);
const char * skinny_buttontype2str(skinny_buttontype_t enum_value);
skinny_buttontype_t skinny_buttontype_str2val(const char *lookup_str);
int skinny_buttontype_str2intval(const char *lookup_str);
char *skinny_buttontype_all_entries(void);
/* = End =========================================================================================       sparse skinny_buttontype === */


/* = Begin =======================================================================================       sparse skinny_devicetype === */

/*
 * \brief sparse enum skinny_devicetype
 */
typedef enum skinny_devicetype {
	SKINNY_DEVICETYPE_UNDEFINED=00,
	SKINNY_DEVICETYPE_VGC=10,
	SKINNY_DEVICETYPE_ATA186=12,
	SKINNY_DEVICETYPE_ATA188=13,
	SKINNY_DEVICETYPE_VIRTUAL30SPPLUS=20,
	SKINNY_DEVICETYPE_PHONEAPPLICATION=21,
	SKINNY_DEVICETYPE_ANALOGACCESS=30,
	SKINNY_DEVICETYPE_DIGITALACCESSPRI=40,
	SKINNY_DEVICETYPE_DIGITALACCESST1=41,
	SKINNY_DEVICETYPE_DIGITALACCESSTITAN2=42,
	SKINNY_DEVICETYPE_ANALOGACCESSELVIS=43,
	SKINNY_DEVICETYPE_DIGITALACCESSLENNON=47,
	SKINNY_DEVICETYPE_CONFERENCEBRIDGE=50,
	SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO=51,
	SKINNY_DEVICETYPE_CONFERENCEBRIDGEDIXIELAND=52,
	SKINNY_DEVICETYPE_CONFERENCEBRIDGESUMMIT=53,
	SKINNY_DEVICETYPE_H225=60,
	SKINNY_DEVICETYPE_H323PHONE=61,
	SKINNY_DEVICETYPE_H323TRUNK=62,
	SKINNY_DEVICETYPE_MUSICONHOLD=70,
	SKINNY_DEVICETYPE_PILOT=71,
	SKINNY_DEVICETYPE_TAPIPORT=72,
	SKINNY_DEVICETYPE_TAPIROUTEPOINT=73,
	SKINNY_DEVICETYPE_VOICEINBOX=80,
	SKINNY_DEVICETYPE_VOICEINBOXADMIN=81,
	SKINNY_DEVICETYPE_LINEANNUNCIATOR=82,
	SKINNY_DEVICETYPE_SOFTWAREMTPDIXIELAND=83,
	SKINNY_DEVICETYPE_CISCOMEDIASERVER=84,
	SKINNY_DEVICETYPE_CONFERENCEBRIDGEFLINT=85,
	SKINNY_DEVICETYPE_ROUTELIST=90,
	SKINNY_DEVICETYPE_LOADSIMULATOR=100,
	SKINNY_DEVICETYPE_MEDIA_TERM_POINT=110,
	SKINNY_DEVICETYPE_MEDIA_TERM_POINTYOKO=111,
	SKINNY_DEVICETYPE_MEDIA_TERM_POINTDIXIELAND=112,
	SKINNY_DEVICETYPE_MEDIA_TERM_POINTSUMMIT=113,
	SKINNY_DEVICETYPE_MGCPSTATION=120,
	SKINNY_DEVICETYPE_MGCPTRUNK=121,
	SKINNY_DEVICETYPE_RASPROXY=122,
	SKINNY_DEVICETYPE_TRUNK=125,
	SKINNY_DEVICETYPE_ANNUNCIATOR=126,
	SKINNY_DEVICETYPE_MONITORBRIDGE=127,
	SKINNY_DEVICETYPE_RECORDER=128,
	SKINNY_DEVICETYPE_MONITORBRIDGEYOKO=129,
	SKINNY_DEVICETYPE_SIPTRUNK=131,
	SKINNY_DEVICETYPE_ANALOG_GATEWAY=30027,
	SKINNY_DEVICETYPE_BRI_GATEWAY=30028,
	SKINNY_DEVICETYPE_30SPPLUS=1,
	SKINNY_DEVICETYPE_12SPPLUS=2,
	SKINNY_DEVICETYPE_12SP=3,
	SKINNY_DEVICETYPE_12=4,
	SKINNY_DEVICETYPE_30VIP=5,
	SKINNY_DEVICETYPE_CISCO7902=30008,
	SKINNY_DEVICETYPE_CISCO7905=20000,
	SKINNY_DEVICETYPE_CISCO7906=369,
	SKINNY_DEVICETYPE_CISCO7910=6,
	SKINNY_DEVICETYPE_CISCO7911=307,
	SKINNY_DEVICETYPE_CISCO7912=30007,
	SKINNY_DEVICETYPE_CISCO7920=30002,
	SKINNY_DEVICETYPE_CISCO7921=365,
	SKINNY_DEVICETYPE_CISCO7925=484,
	SKINNY_DEVICETYPE_CISCO7926=577,
	SKINNY_DEVICETYPE_CISCO7931=348,
	SKINNY_DEVICETYPE_CISCO7935=9,
	SKINNY_DEVICETYPE_CISCO7936=30019,
	SKINNY_DEVICETYPE_CISCO7937=431,
	SKINNY_DEVICETYPE_CISCO7940=8,
	SKINNY_DEVICETYPE_CISCO7941=115,
	SKINNY_DEVICETYPE_CISCO7941GE=309,
	SKINNY_DEVICETYPE_CISCO7942=434,
	SKINNY_DEVICETYPE_CISCO7945=435,
	SKINNY_DEVICETYPE_CISCO7960=7,
	SKINNY_DEVICETYPE_CISCO7961=30018,
	SKINNY_DEVICETYPE_CISCO7961GE=308,
	SKINNY_DEVICETYPE_CISCO7962=404,
	SKINNY_DEVICETYPE_CISCO7965=436,
	SKINNY_DEVICETYPE_CISCO7970=30006,
	SKINNY_DEVICETYPE_CISCO7971=119,
	SKINNY_DEVICETYPE_CISCO7975=437,
	SKINNY_DEVICETYPE_CISCO7985=302,
	SKINNY_DEVICETYPE_NOKIA_E_SERIES=275,
	SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR=30016,
	SKINNY_DEVICETYPE_NOKIA_ICC=376,
	SKINNY_DEVICETYPE_CISCO6901=547,
	SKINNY_DEVICETYPE_CISCO6911=548,
	SKINNY_DEVICETYPE_CISCO6921=495,
	SKINNY_DEVICETYPE_CISCO6941=496,
	SKINNY_DEVICETYPE_CISCO6945=564,
	SKINNY_DEVICETYPE_CISCO6961=497,
	SKINNY_DEVICETYPE_CISCO8941=586,
	SKINNY_DEVICETYPE_CISCO8945=585,
	SKINNY_DEVICETYPE_SPA_303G=80011,
	SKINNY_DEVICETYPE_SPA_502G=80003,
	SKINNY_DEVICETYPE_SPA_504G=80004,
	SKINNY_DEVICETYPE_SPA_509G=80007,
	SKINNY_DEVICETYPE_SPA_521S=80000,
	SKINNY_DEVICETYPE_SPA_524SG=80001,
	SKINNY_DEVICETYPE_SPA_525G=80005,
	SKINNY_DEVICETYPE_SPA_525G2=80009,
	SKINNY_DEVICETYPE_CISCO_ADDON_7914=124,
	SKINNY_DEVICETYPE_CISCO_ADDON_7915_12BUTTON=227,
	SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON=228,
	SKINNY_DEVICETYPE_CISCO_ADDON_7916_12BUTTON=229,
	SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON=230,
	SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S=99991,
	SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS=99992,
	SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS=99993,
	SKINNY_DEVICETYPE_SENTINEL
} skinny_devicetype_t;
int skinny_devicetype_exists(int skinny_devicetype_int_value);
const char * skinny_devicetype2str(skinny_devicetype_t enum_value);
skinny_devicetype_t skinny_devicetype_str2val(const char *lookup_str);
int skinny_devicetype_str2intval(const char *lookup_str);
char *skinny_devicetype_all_entries(void);
/* = End =========================================================================================       sparse skinny_devicetype === */


/* = Begin =======================================================================================        skinny_encryptionmethod === */

/*
 * \brief enum skinny_encryptionmethod
 */
typedef enum skinny_encryptionmethod {
	SKINNY_ENCRYPTIONMETHOD_NONE=0x0,
	SKINNY_ENCRYPTIONMETHOD_AES_128_HMAC_SHA1_32=0x1,
	SKINNY_ENCRYPTIONMETHOD_AES_128_HMAC_SHA1_80=0x2,
	SKINNY_ENCRYPTIONMETHOD_F8_128_HMAC_SHA1_32=0x3,
	SKINNY_ENCRYPTIONMETHOD_F8_128_HMAC_SHA1_80=0x4,
	SKINNY_ENCRYPTIONMETHOD_AEAD_AES_128_GCM=0x5,
	SKINNY_ENCRYPTIONMETHOD_AEAD_AES_256_GCM=0x6,
	SKINNY_ENCRYPTIONMETHOD_SENTINEL
} skinny_encryptionmethod_t;
int skinny_encryptionmethod_exists(int skinny_encryptionmethod_int_value);
const char * skinny_encryptionmethod2str(skinny_encryptionmethod_t enum_value);
skinny_encryptionmethod_t skinny_encryptionmethod_str2val(const char *lookup_str);
int skinny_encryptionmethod_str2intval(const char *lookup_str);
char *skinny_encryptionmethod_all_entries(void);
/* = End =========================================================================================        skinny_encryptionmethod === */


/* = Begin =======================================================================================         skinny_miscCommandType === */

/*
 * \brief enum skinny_miscCommandType
 */
typedef enum skinny_miscCommandType {
	SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE=0x0,
	SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE=0x1,
	SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEGOB=0x2,
	SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEMB=0x3,
	SKINNY_MISCCOMMANDTYPE_LOSTPICTURE=0x4,
	SKINNY_MISCCOMMANDTYPE_LOSTPARTIALPICTURE=0x5,
	SKINNY_MISCCOMMANDTYPE_RECOVERYREFERENCEPICTURE=0x6,
	SKINNY_MISCCOMMANDTYPE_TEMPORALSPATIALTRADEOFF=0x7,
	SKINNY_MISCCOMMANDTYPE_SENTINEL
} skinny_miscCommandType_t;
int skinny_miscCommandType_exists(int skinny_miscCommandType_int_value);
const char * skinny_miscCommandType2str(skinny_miscCommandType_t enum_value);
skinny_miscCommandType_t skinny_miscCommandType_str2val(const char *lookup_str);
int skinny_miscCommandType_str2intval(const char *lookup_str);
char *skinny_miscCommandType_all_entries(void);
/* = End =========================================================================================         skinny_miscCommandType === */

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
