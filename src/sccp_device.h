/*!
 * \file        sccp_device.h
 * \brief       SCCP Device Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_DEVICE_H
#define __SCCP_DEVICE_H

#ifdef DEBUG
#define sccp_device_retain(_x) 		({sccp_device_t const __attribute__((unused)) *tmp_##__LINE__##X = _x;ast_assert(tmp_##__LINE__##X != NULL);sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_device_release(_x) 	({sccp_device_t const __attribute__((unused)) *tmp_##__LINE__##X = _x;ast_assert(tmp_##__LINE__##X != NULL);sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#else
#define sccp_device_retain(_x) 		({ast_assert(_x != NULL);sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_device_release(_x) 	({ast_assert(_x != NULL);sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#endif
#define sccp_device_refreplace(_x, _y) 	({sccp_refcount_replace((const void **)&_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
/*!
 * \brief SCCP Button Configuration Structure
 */
struct sccp_buttonconfig {
	uint16_t instance;											/*!< Instance on device */
	uint16_t index;												/*!< Button position on device */
	sccp_config_buttontype_t type;										/*!< Button type (e.g. line, speeddial, feature, empty) */
	char *label;												/*!< Button Name/Label */
	SCCP_LIST_ENTRY (sccp_buttonconfig_t) list;								/*!< Button Linked List Entry */

	/*!
	 * \brief SCCP Button Structure
	 */
	union sccp_button {

		/*!
		 * \brief SCCP Button Line Structure
		 */
		struct /* sccp_line */ {
			char *name;										/*!< Button Name */
			sccp_subscription_id_t *subscriptionId;
			char *options;
		} line;												/*!< SCCP Button Line Structure */

		/*!
		 * \brief SCCP Button Speeddial Structure
		 */
		struct /* sccp_speeddial */ {
			char *ext;										/*!< SpeedDial Extension */
			char *hint;										/*!< SpeedDIal Hint */
		} speeddial;											/*!< SCCP Button Speeddial Structure */

		/*!
		 * \brief SCCP Button Service Structure
		 */
		struct /* sccp_service */ {
			char *url;										/*!< The number to dial when it's hit */
		} service;											/*!< SCCP Button Service Structure  */

		/*!
		 * \brief SCCP Button Feature Structure
		 */
		struct /* sccp_feature */ {
			uint8_t index;										/*!< Button Feature Index */
			sccp_feature_type_t id;									/*!< Button Feature ID */
			char *options;										/*!< Button Feature Options */
			uint32_t status;									/*!< Button Feature Status */
		} feature;											/*!< SCCP Button Feature Structure */
	} button;												/*!< SCCP Button Structure */

	boolean_t pendingDelete;
	boolean_t pendingUpdate;
};														/*!< SCCP Button Configuration Structure */

/*!
 * \brief SCCP SpeedDial Button Structure
 * \todo replace ext/hint with charptr (save 80)
 */
struct sccp_speed {
	uint16_t instance;											/*!< The instance on the current device */
	uint8_t config_instance;										/*!< The instance of the speeddial in the sccp.conf */
	uint8_t type;												/*!< SpeedDial Button Type (SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE (hint)) */
	char name[StationMaxNameSize];										/*!< The name of the speed dial button */
	char ext[SCCP_MAX_EXTENSION];										/*!< The number to dial when it's hit */
	char hint[SCCP_MAX_EXTENSION];										/*!< The HINT on this SpeedDial */
	boolean_t valid;											/*!< Speeddial configuration is valid or not */
	SCCP_LIST_ENTRY (sccp_speed_t) list;									/*!< SpeedDial Linked List Entry */
};

/*!
 * \brief SCCP Feature Configuration Structure
 */
struct sccp_feature_configuration {
	boolean_t enabled;											/*!< Feature Enabled */
	boolean_t initialized;											/*!< Feature Enabled */
	uint32_t previousStatus;										/*!< Feature Previous State */
	uint32_t status;											/*!< Feature State */
};

/*!
 * \brief SCCP Call Statistics Structure
 */
struct sccp_call_statistics {
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
};

/*!
 * \brief SCCP Hostname Structure
 */
struct sccp_hostname {
	char name[SCCP_MAX_HOSTNAME_LEN];									/*!< Name of the Host */
	SCCP_LIST_ENTRY (sccp_hostname_t) list;									/*!< Host Linked List Entry */
};														/*!< SCCP Hostname Structure */

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

/*!
 * \brief SCCP Device Structure
 */
struct sccp_device {
	char id[StationMaxDeviceNameSize];									/*!< SEP<macAddress> of the device. */
	const sccp_deviceProtocol_t *protocol;									/*!< protocol the devices uses */
	uint32_t skinny_type;											/*!< Model of this Phone sent by the station, devicetype */
	uint32_t device_features;										/*!< device features (contains protocolversion in 8bit first segement */
	sccp_earlyrtp_t earlyrtp;										/*!< RTP Channel State where to open the RTP Media Stream */
	sccp_session_t *session;										/*!< Current Session */
	SCCP_RWLIST_ENTRY (sccp_device_t) list;									/*!< Global Device Linked List */
	uint16_t keepalive;											/*!< Station Specific Keepalive Timeout */
	uint16_t keepaliveinterval;										/*!< Currently set Keepalive Timeout */
	uint8_t protocolversion;										/*!< Skinny Supported Protocol Version */
	uint8_t inuseprotocolversion;										/*!< Skinny Used Protocol Version */
	sccp_nat_t nat;												/*!< Network Address Translation Support (Boolean, default=on) */
	boolean_t directrtp;											/*!< Direct RTP Support (Boolean, default=on) */

	sccp_private_device_data_t *privateData;
	
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

	char *description;											/*!< Internal Description. Skinny protocol does not use it */
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

	//uint8_t earlyrtp;											/*!< RTP Channel State where to open the RTP Media Stream */
	time_t registrationTime;

	struct sccp_ha *ha;											/*!< Permit or Deny Connections to the Main Socket */

	sccp_dtmfmode_t dtmfmode;										/*!< DTMF Mode (0 inband - 1 outofband) */
	boolean_t meetme;											/*!< Meetme on/off */
	char *meetmeopts;											/*!< Meetme Options to be Used */
	skinny_lampmode_t mwilamp;										/*!< MWI/Lamp to indicate MailBox Messages */
	boolean_t mwioncall;											/*!< MWI On Call Support (Boolean, default=on) */
	boolean_t softkeysupport;										/*!< Soft Key Support (Boolean, default=on) */
	uint32_t mwilight;											/*!< MWI/Light bit field to to store mwi light for each line and device (offset 0 is current device state) */

	boolean_t realtime;											/*!< is it a realtime configuration */
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
	sccp_dndmode_t dndmode;											/*!< dnd mode: see SCCP_DNDMODE_* */
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

	boolean_t (*checkACL) (constDevicePtr device);								/*!< check ACL callback function */
	sccp_push_result_t (*pushURL) (constDevicePtr device, const char *url, uint8_t priority, uint8_t tone);
	sccp_push_result_t (*pushTextMessage) (constDevicePtr device, const char *messageText, const char *from, uint8_t priority, uint8_t tone);
	boolean_t (*hasDisplayPrompt) (void);									/*!< has Display Prompt callback function (derived from devicetype and protocol) */
	boolean_t (*hasEnhancedIconMenuSupport) (void);								/*!< has Enhanced IconMenu Support (derived from devicetype and protocol) */
	void (*retrieveDeviceCapabilities) (constDevicePtr device);				/*!< set device background image */
	void (*setBackgroundImage) (constDevicePtr device, const char *url);			/*!< set device background image */
	void (*displayBackgroundImagePreview) (constDevicePtr device, const char *url);		/*!< display background image as preview */
	void (*setRingTone) (constDevicePtr device, const char *url);					/*!< set the default Ringtone */
	const struct sccp_device_indication_cb *indicate;
	
	sccp_dtmfmode_t(*getDtmfMode) (constDevicePtr device);
	
	struct { 
#ifndef SCCP_ATOMIC
		sccp_mutex_t lock;										/*!< Message Stack Lock */
#endif
		char *(messages[SCCP_MAX_MESSAGESTACK]);							/*!< Message Stack Array */
	} messageStack;
	
	sccp_call_statistics_t call_statistics[2];								/*!< Call statistics */
	char *softkeyDefinition;										/*!< requested softKey configuration */
	sccp_softKeySetConfiguration_t *softkeyset;								/*!< Allow for a copy of the softkeyset, if any of the softkeys needs to be redefined, for example for urihook/uriaction */
	void (*copyStr2Locale) (constDevicePtr d, char *dst, const char *src, size_t dst_size);		/*!< copy string to device converted to locale if necessary */

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
 * \brief SCCP Device Indication Callback Structure
 */
struct sccp_device_indication_cb {
	void (*const remoteHold) (constDevicePtr device, uint8_t lineInstance, uint32_t callid, uint8_t callpriority, uint8_t callPrivacy);
	void (*const remoteOffhook) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid);
	void (*const remoteOnhook) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid);
	void (*const offhook) (constDevicePtr device, sccp_linedevices_t * linedevice, uint32_t callid);
	void (*const onhook) (constDevicePtr device, const uint8_t lineInstance, uint32_t callid);
	void (*const dialing) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const skinny_calltype_t calltype, sccp_callinfo_t * const callinfo, char dialedNumber[SCCP_MAX_EXTENSION]);
	void (*const proceed) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const skinny_calltype_t calltype, sccp_callinfo_t * const callinfo);
	void (*const connected) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const skinny_calltype_t calltype, sccp_callinfo_t * const callinfo);
};

#define sccp_dev_display(p,q) sccp_dev_display_debug(p, q, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprompt(p, q, r, s, t) sccp_dev_displayprompt_debug(p, q, r, s, t, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displaynotify(p,q,r) sccp_dev_displaynotify_debug(p,q,r, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprinotify(p,q,r,s) sccp_dev_displayprinotify_debug(p,q,r,s,__FILE__, __LINE__, __PRETTY_FUNCTION__)

void sccp_device_pre_reload(void);
void sccp_device_post_reload(void);

/* ====================================================================================================== start getters / setters for privateData */
const sccp_accessorystate_t sccp_device_getAccessoryStatus(constDevicePtr d, const sccp_accessory_t accessory);
const sccp_accessory_t sccp_device_getActiveAccessory(constDevicePtr d);
int sccp_device_setAccessoryStatus(constDevicePtr d, const sccp_accessory_t accessory, const sccp_accessorystate_t state);
const sccp_devicestate_t sccp_device_getDeviceState(constDevicePtr d);
int sccp_device_setDeviceState(constDevicePtr d, const sccp_devicestate_t state);
const skinny_registrationstate_t sccp_device_getRegistrationState(constDevicePtr d);
int sccp_device_setRegistrationState(constDevicePtr d, const skinny_registrationstate_t state);
/* ======================================================================================================== end getters / setters for privateData */

/* live cycle */
sccp_device_t *sccp_device_create(const char *id);
sccp_device_t *sccp_device_createAnonymous(const char *name);
void sccp_device_addToGlobals(constDevicePtr device);

sccp_line_t *sccp_dev_getActiveLine(constDevicePtr device);
void sccp_dev_setActiveLine(devicePtr device, constLinePtr l);
sccp_channel_t *sccp_device_getActiveChannel(constDevicePtr device);
void sccp_device_setActiveChannel(devicePtr device, sccp_channel_t * c);

sccp_buttonconfig_t *sccp_dev_serviceURL_find_byindex(devicePtr device, uint16_t instance);

#define REQ(x,y) x = sccp_build_packet(y, sizeof(x->data.y))
#define REQCMD(x,y) x = sccp_build_packet(y, 0)
sccp_msg_t *sccp_build_packet(sccp_mid_t t, size_t pkt_len);

void sccp_dev_check_displayprompt(constDevicePtr d);
void sccp_device_setLastNumberDialed(devicePtr device, const char *lastNumberDialed, const sccp_linedevices_t *linedevice);
void sccp_device_preregistration(devicePtr device);
void sccp_dev_build_buttontemplate(devicePtr d, btnlist * btn);
void sccp_dev_sendmsg(constDevicePtr d, sccp_mid_t t);
void sccp_dev_set_keyset(constDevicePtr d, uint8_t lineInstance, uint32_t callid, uint8_t softKeySetIndex);
void sccp_dev_set_ringer(constDevicePtr d, uint8_t opt, uint8_t lineInstance, uint32_t callid);
void sccp_dev_cleardisplay(constDevicePtr d);
void sccp_dev_set_registered(devicePtr d, skinny_registrationstate_t state);
void sccp_dev_set_speaker(constDevicePtr d, uint8_t opt);
void sccp_dev_set_microphone(devicePtr d, uint8_t opt);
void sccp_dev_set_cplane(constDevicePtr device, uint8_t lineInstance, int status);
void sccp_dev_deactivate_cplane(constDevicePtr d);
void sccp_dev_starttone(constDevicePtr d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout);
void sccp_dev_stoptone(constDevicePtr d, uint8_t line, uint32_t callid);
void sccp_dev_clearprompt(constDevicePtr d, uint8_t lineInstance, uint32_t callid);
void sccp_dev_display_debug(constDevicePtr d, const char *msg, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprompt_debug(constDevicePtr d, const uint8_t lineInstance, const uint32_t callid, const char *msg, int timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displaynotify_debug(constDevicePtr d, const char *msg, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprinotify_debug(constDevicePtr d, const char *msg, const uint8_t priority, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_cleardisplaynotify(constDevicePtr d);
void sccp_dev_cleardisplayprinotify(constDevicePtr d, const uint8_t priority);
void sccp_dev_speed_find_byindex(constDevicePtr d, const uint16_t instance, boolean_t withHint, sccp_speed_t * const k);
void sccp_dev_forward_status(constLinePtr l, uint8_t lineInstance, constDevicePtr device);
void sccp_dev_postregistration(void *data);
void sccp_dev_clean(devicePtr d, boolean_t destroy, uint8_t cleanupTime);
void sccp_dev_keypadbutton(devicePtr d, char digit, uint8_t line, uint32_t callid);
void sccp_dev_set_message(devicePtr d, const char *msg, const int timeout, const boolean_t storedb, const boolean_t beep);
void sccp_dev_clear_message(devicePtr d, const boolean_t cleardb);
void sccp_device_addMessageToStack(devicePtr device, const uint8_t priority, const char *message);
void sccp_device_clearMessageFromStack(devicePtr device, const uint8_t priority);
void sccp_device_featureChangedDisplay(const sccp_event_t * event);
void sccp_device_sendcallstate(constDevicePtr d, uint8_t instance, uint32_t callid, skinny_callstate_t state, skinny_callpriority_t precedence_level, skinny_callinfo_visibility_t visibility);

int sccp_dev_send(constDevicePtr d, sccp_msg_t * msg);
int sccp_device_check_ringback(devicePtr d);
int sccp_device_sendReset(devicePtr d, uint8_t reset_type);

uint8_t sccp_device_find_index_for_line(constDevicePtr d, const char *lineName);
uint8_t sccp_device_numberOfChannels(constDevicePtr device);

boolean_t sccp_device_isVideoSupported(constDevicePtr device);
boolean_t sccp_device_check_update(devicePtr d);

// find device
sccp_device_t *sccp_device_find_byid(const char *id, boolean_t useRealtime);

#if DEBUG
#define sccp_device_find_byname(_x) __sccp_device_find_byid(_x)
#ifdef CS_SCCP_REALTIME
#define sccp_device_find_realtime(_x) __sccp_device_find_realtime(_x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_device_t *__sccp_device_find_realtime(const char *name, const char *filename, int lineno, const char *func);

#define sccp_device_find_realtime_byid(_x) sccp_device_find_realtime(_x)
#define sccp_device_find_realtime_byname(_x) sccp_device_find_realtime(_x)
#endif
#else

#define sccp_device_find_byname(x) sccp_device_find_byid(x)
#ifdef CS_SCCP_REALTIME
sccp_device_t *sccp_device_find_realtime(const char *name);

#define sccp_device_find_realtime_byid(x) sccp_device_find_realtime(x)
#define sccp_device_find_realtime_byname(x) sccp_device_find_realtime(x)
#endif
#endif

void sccp_device_setLamp(constDevicePtr device, skinny_stimulus_t stimulus, uint8_t instance, skinny_lampmode_t mode);

#endif														/* __SCCP_DEVICE_H */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
