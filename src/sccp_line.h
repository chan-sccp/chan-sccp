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
	skinny_tone_t initial_dialtone_tone;                                                                    /*!< initial dialtone tone */
	skinny_tone_t secondary_dialtone_tone;									/*!< secondary dialtone tone */
	char secondary_dialtone_digits[SCCP_MAX_SECONDARY_DIALTONE_DIGITS];					/*!< secondary dialtone digits */

	char *trnsfvm;												/*!< transfer to voicemail softkey. Basically a call forward */
	sccp_group_t callgroup;											/*!< callgroups assigned (seperated by commas) to this lines */
#ifdef CS_SCCP_PICKUP
	sccp_group_t pickupgroup;										/*!< pickupgroup assigned to this line */

	boolean_t directed_pickup;										/*!< Directed Pickup Extension Support (Boolean, default=on) */
	char directed_pickup_context[SCCP_MAX_CONTEXT];								/*!< Directed Pickup Context to Use in DialPlan */
	boolean_t pickup_modeanswer;										/*!< Directed PickUp Mode Answer (boolean, default" on) */
#ifdef CS_AST_HAS_NAMEDGROUP
	char *namedcallgroup;											/*!< Named Call Group */
	char *namedpickupgroup;											/*!< Named Pickup Group */
#endif
#endif
	skinny_capabilities_t capabilities;									/*!< (shared)line level preferences (overrules device level) */
	skinny_capabilities_t preferences;									/*!< (shared)line level preferences (overrules device level) */
	boolean_t preferences_set_on_line_level;								/*!< (Temp) if above was set manually or automatically copied */

	char cid_num[SCCP_MAX_EXTENSION];									/* smaller would be better (i.e. 32) */ /*!< Caller(ID) to use on outgoing calls  */
	char cid_name[SCCP_MAX_EXTENSION];									/* smaller would be better (i.e. 32) */ /*!< Caller(Name) to use on outgoing calls */

	pbx_ama_flags_type amaflags;
	sccp_dndmode_t dndmode;											/*!< dnd mode: see SCCP_DNDMODE_* */
	
	SCCP_LIST_HEAD (, sccp_mailbox_t) mailboxes;								/*!< Mailbox Linked List Entry. To check for messages */
	SCCP_LIST_HEAD (, sccp_channel_t) channels;								/*!< Linked list of current channels for this line */
	SCCP_LIST_HEAD(, sccp_linedevice_t) devices;                                                            /*!< The device this line is currently registered to. */

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

	sccp_subscription_t defaultSubscription;								/*!< default subscription id for shared lines */
	boolean_t echocancel;											/*!< echocancel phone support */
	boolean_t silencesuppression;										/*!< Silence Suppression Phone Support */
	boolean_t meetme;											/*!< Meetme on/off */
	boolean_t isShared;
	/*!
	 * \brief VoiceMail Statistics Structure
	 */
	struct {
		int newmsgs;											/*!< New Messages */
		int oldmsgs;											/*!< Old Messages */
	} voicemailStatistic;											/*!< VoiceMail Statistics Structure */
	boolean_t transfer;											/*!< Transfer Phone Support */

	sccp_video_mode_t videomode;
	/* this is for reload routines */
	boolean_t pendingDelete;										/*!< this bit will tell the scheduler to delete this line when unused */
	boolean_t pendingUpdate;										/*!< this bit will tell the scheduler to update this line when unused */
};														/*!< SCCP Line Structure */

/*!
 * \brief SCCP Hotline Structure
 * \note This contains the new HotLine Feature
 */
struct sccp_hotline {
	linePtr line;                                                                                           /*!< Line */
	char exten[SCCP_MAX_EXTENSION];										/*!< Extension */
};														/*!< SCCP Hotline Structure */

SCCP_API void SCCP_CALL sccp_line_pre_reload(void);
SCCP_API void SCCP_CALL sccp_line_post_reload(void);
/* live cycle */
SCCP_API void * SCCP_CALL sccp_create_hotline(void);
SCCP_API linePtr SCCP_CALL sccp_line_create(const char * name);
SCCP_API void SCCP_CALL sccp_line_addToGlobals(constLinePtr line);
SCCP_API void SCCP_CALL sccp_line_removeFromGlobals(sccp_line_t * line);
SCCP_API void SCCP_CALL sccp_line_addChannel(constLinePtr line, constChannelPtr channel);
SCCP_API void SCCP_CALL sccp_line_removeChannel(constLinePtr line, sccp_channel_t * channel);
SCCP_API void SCCP_CALL sccp_line_clean(linePtr l, boolean_t remove_from_global);
SCCP_API void SCCP_CALL sccp_line_kill_channels(linePtr l);
SCCP_API void SCCP_CALL sccp_line_copyCodecSetsFromLineToChannel(constLinePtr l, constDevicePtr maybe_d, channelPtr c);
SCCP_API void SCCP_CALL sccp_line_updatePreferencesFromDevicesToLine(linePtr l);
SCCP_API void SCCP_CALL sccp_line_updateCapabilitiesFromDevicesToLine(linePtr l);
SCCP_API void SCCP_CALL sccp_line_updateLineCapabilitiesByDevice(constDevicePtr d);
SCCP_API void SCCP_CALL sccp_line_cfwd(constLinePtr line, constDevicePtr device, sccp_cfwd_t type, char * number);
SCCP_API void SCCP_CALL sccp_line_setMWI(constLinePtr l, int newlinemsgs, int oldlinemsgs);

// find line
SCCP_API linePtr SCCP_CALL sccp_line_find_byname(const char * name, uint8_t useRealtime);
#if DEBUG
#	define sccp_line_find_byid(_x, _y) __sccp_line_find_byid(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API linePtr SCCP_CALL __sccp_line_find_byid(constDevicePtr d, uint16_t instance, const char * filename, int lineno, const char * func);

#	define sccp_line_find_byButtonIndex(_x, _y) __sccp_line_find_byButtonIndex(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API linePtr SCCP_CALL __sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex, const char * filename, int lineno, const char * func);
#	ifdef CS_SCCP_REALTIME
#		define sccp_line_find_realtime_byname(_x) __sccp_line_find_realtime_byname(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API linePtr SCCP_CALL __sccp_line_find_realtime_byname(const char * name, const char * filename, int lineno, const char * func);
#	endif                                                                                                  // CS_SCCP_REALTIME
#else														// DEBUG
SCCP_API linePtr SCCP_CALL sccp_line_find_byid(constDevicePtr d, uint16_t instance);
SCCP_API linePtr SCCP_CALL sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex);

#	ifdef CS_SCCP_REALTIME
SCCP_API linePtr SCCP_CALL sccp_line_find_realtime_byname(const char * name);
#	endif                                                                                                  // CS_SCCP_REALTIME
#endif														// DEBUG

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
