/*!
 * \file        sccp_line.h
 * \brief       SCCP Line Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 */
#pragma once

#define sccp_linedevice_retain(_x)		sccp_refcount_retain_type(sccp_linedevices_t, _x)
#define sccp_linedevice_release(_x)		sccp_refcount_release_type(sccp_linedevices_t, _x)
#define sccp_linedevice_refreplace(_x, _y)	sccp_refcount_refreplace_type(sccp_linedevices_t, _x, _y)
#define sccp_line_retain(_x)			sccp_refcount_retain_type(sccp_line_t, _x)
#define sccp_line_release(_x)			sccp_refcount_release_type(sccp_line_t, _x)
#define sccp_line_refreplace(_x, _y)		sccp_refcount_refreplace_type(sccp_line_t, _x, _y)

__BEGIN_C_EXTERN__
/*!
 * \brief SCCP Line Structure
 * \note A line is the equivalent of a 'phone line' going to the phone.
 */
struct sccp_line {
	//sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */
	char id[SCCP_MAX_LINE_ID];										/*!< This line's ID, used for login (for mobility) */
	char name[StationMaxNameSize];										/*!< The name of the line, so use in asterisk (i.e SCCP/[name]) */
	uint32_t configurationStatus;										/*!< what is the current configuration status - @see sccp_config_status_t */
#ifdef CS_SCCP_REALTIME
	boolean_t realtime;											/*!< is it a realtimeconfiguration */
	uint8_t _padding1[3];
#endif
	SCCP_RWLIST_ENTRY (sccp_line_t) list;									/*!< global list entry */
	struct {
		uint8_t numberOfActiveDevices;									/*!< Number of Active Devices */
		uint8_t numberOfActiveChannels;									/*!< Number of Active Channels */
		uint8_t numberOfHeldChannels;									/*!< Number of Hold Channels */
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
	sccp_dndmode_t dndmode;											/*!< dnd mode: see SCCP_DNDMODE_* */
	
	SCCP_LIST_HEAD (, sccp_mailbox_t) mailboxes;								/*!< Mailbox Linked List Entry. To check for messages */
	SCCP_LIST_HEAD (, sccp_channel_t) channels;								/*!< Linked list of current channels for this line */
	SCCP_LIST_HEAD (, sccp_linedevices_t) devices;								/*!< The device this line is currently registered to. */

	PBX_VARIABLE_TYPE *variables;										/*!< Channel variables to set */

	char pin[SCCP_MAX_LINE_PIN];										/*!< PIN number for mobility/roaming. */
	char *adhocNumber;											/*!< number that should be dialed when device offhocks this line */
	char *regexten;												/*!< Extension for auto-extension (DUNDI) */
	char *regcontext;											/*!< Context for auto-extension (DUNDI) */
	char *description;											/*!< A description for the line, displayed on in header (on7960/40) or on main  screen on 7910 */
	char *label;												/*!< A name for the line, displayed next to the button (7960/40). */
	char *vmnum;												/*!< Voicemail number to Dial */
	char *meetmenum;											/*!< Meetme Extension to be Dialed (\todo TO BE REMOVED) */
	char *meetmeopts;											/*!< Meetme Options to be Used */
	char *context;												/*!< The context we use for Outgoing Calls. */
	char *language;												/*!< language we use for calls */
	char *accountcode;											/*!< accountcode used in cdr */
	char *musicclass;											/*!< musicclass assigned when getting moh */
	char *parkinglot;											/*!< parkinglot to use */

	sccp_subscription_id_t defaultSubscriptionId;								/*!< default subscription id for shared lines */
	boolean_t echocancel;											/*!< echocancel phone support */
	boolean_t silencesuppression;										/*!< Silence Suppression Phone Support */
	boolean_t meetme;											/*!< Meetme on/off */
	/*!
	 * \brief VoiceMail Statistics Structure
	 */
	struct {
		int newmsgs;											/*!< New Messages */
		int oldmsgs;											/*!< Old Messages */
	} voicemailStatistic;											/*!< VoiceMail Statistics Structure */
	boolean_t transfer;											/*!< Transfer Phone Support */

	/* this is for reload routines */
	boolean_t pendingDelete;										/*!< this bit will tell the scheduler to delete this line when unused */
	boolean_t pendingUpdate;										/*!< this bit will tell the scheduler to update this line when unused */
};														/*!< SCCP Line Structure */

/*!
 * \brief SCCP Hotline Structure
 * \note This contains the new HotLine Feature
 */
struct sccp_hotline {
	sccp_line_t *line;											/*!< Line */
	char exten[SCCP_MAX_EXTENSION];										/*!< Extension */
};														/*!< SCCP Hotline Structure */

/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information {
	boolean_t enabled;
	char number[SCCP_MAX_EXTENSION];
};

/*!
 * \brief SCCP Line-Devices Structure
 */
struct sccp_linedevices {
	sccp_device_t *device;											/*!< SCCP Device */
	sccp_line_t *line;											/*!< SCCP Line */
	SCCP_LIST_ENTRY (sccp_linedevices_t) list;								/*!< Device Linked List Entry */

	sccp_cfwd_information_t cfwdAll;									/*!< cfwdAll information */
	sccp_cfwd_information_t cfwdBusy;									/*!< cfwdBusy information */
	//sccp_cfwd_information_t cfwdNoAnswer;									/*!< cfwdNoAnswer information */

	sccp_subscription_id_t subscriptionId;									/*!< for addressing individual devices on shared line */
	char label[SCCP_MAX_LABEL];										/*!<  */

	uint8_t lineInstance;											/*!< line instance of this->line on this->device */
	boolean_t (*isPickupAllowed) (void);
};														/*!< SCCP Line-Device Structure */

SCCP_API void SCCP_CALL sccp_line_pre_reload(void);
SCCP_API void SCCP_CALL sccp_line_post_reload(void);

/* live cycle */
SCCP_API void * SCCP_CALL sccp_create_hotline(void);
SCCP_API sccp_line_t * SCCP_CALL sccp_line_create(const char *name);
SCCP_API void SCCP_CALL sccp_line_addToGlobals(sccp_line_t * line);
SCCP_API void SCCP_CALL sccp_line_removeFromGlobals(sccp_line_t * line);
SCCP_API void SCCP_CALL sccp_line_addDevice(sccp_line_t * line, sccp_device_t * d, uint8_t lineInstance, sccp_subscription_id_t *subscriptionId);
SCCP_API void SCCP_CALL sccp_line_removeDevice(sccp_line_t * l, sccp_device_t * device);
SCCP_API void SCCP_CALL sccp_line_addChannel(constLinePtr line, constChannelPtr channel);
SCCP_API void SCCP_CALL sccp_line_removeChannel(sccp_line_t * line, sccp_channel_t * channel);
SCCP_API void SCCP_CALL sccp_line_clean(sccp_line_t * l, boolean_t remove_from_global);
SCCP_API void SCCP_CALL sccp_line_kill_channels(sccp_line_t * l);

#if UNUSEDCODE // 2015-11-01
SCCP_API sccp_channelstate_t SCCP_CALL sccp_line_getDNDChannelState(sccp_line_t * line);
#endif
SCCP_API void SCCP_CALL sccp_line_copyCodecSetsFromLineToChannel(sccp_line_t *l, sccp_channel_t *c);
SCCP_API void SCCP_CALL sccp_line_cfwd(constLinePtr line, constDevicePtr device, sccp_callforward_t type, char *number);

SCCP_API void SCCP_CALL sccp_linedevice_resetPickup(sccp_linedevices_t * ld);
SCCP_API void SCCP_CALL sccp_linedevice_disallowPickup(sccp_linedevices_t * ld);

// find line
SCCP_API sccp_line_t * SCCP_CALL sccp_line_find_byname(const char *name, uint8_t useRealtime);

#if DEBUG
#define sccp_line_find_byid(_x,_y) __sccp_line_find_byid(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
SCCP_API sccp_line_t * SCCP_CALL __sccp_line_find_byid(constDevicePtr d, uint16_t instance, const char *filename, int lineno, const char *func);

#define sccp_line_find_byButtonIndex(_x, _y) __sccp_line_find_byButtonIndex(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
SCCP_API sccp_line_t * SCCP_CALL __sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex, const char *filename, int lineno, const char *func);
#ifdef CS_SCCP_REALTIME
#define sccp_line_find_realtime_byname(_x) __sccp_line_find_realtime_byname(_x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
SCCP_API sccp_line_t * SCCP_CALL __sccp_line_find_realtime_byname(const char *name, const char *filename, int lineno, const char *func);
#endif														// CS_SCCP_REALTIME
#else														// DEBUG
SCCP_API sccp_line_t * SCCP_CALL sccp_line_find_byid(constDevicePtr d, uint16_t instance);
SCCP_API sccp_line_t * SCCP_CALL sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex);

#ifdef CS_SCCP_REALTIME
SCCP_API sccp_line_t * SCCP_CALL sccp_line_find_realtime_byname(const char *name);
#endif														// CS_SCCP_REALTIME
#endif														// DEBUG

#define sccp_linedevice_find(_x,_y) __sccp_linedevice_find(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_linedevice_findByLineinstance(_x,_y) __sccp_linedevice_findByLineinstance(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API sccp_linedevices_t * SCCP_CALL __sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func);
SCCP_API sccp_linedevices_t * SCCP_CALL __sccp_linedevice_findByLineinstance(const sccp_device_t * device, uint16_t instance, const char *filename, int lineno, const char *func);
SCCP_API void SCCP_CALL sccp_line_createLineButtonsArray(sccp_device_t * device);
SCCP_API void SCCP_CALL sccp_line_deleteLineButtonsArray(sccp_device_t * device);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
