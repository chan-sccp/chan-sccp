
/*!
 * \file 	sccp_hint.h
 * \brief 	SCCP Hint Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since 	2009-01-16
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */

#    ifndef SCCP_HINT_H_
#define SCCP_HINT_H_

typedef enum { ASTERISK = 0, INTERNAL = 1 } sccp_hinttype_t;

typedef struct sccp_hint_SubscribingDevice sccp_hint_SubscribingDevice_t;

/*!
 *\brief SCCP Hint Subscribing Device Structure
 */
struct sccp_hint_SubscribingDevice {

	const sccp_device_t *device;						/*!< SCCP Device */
	uint8_t instance;							/*!< Instance */
	uint8_t positionOnDevice;						/*!< Instance */

	 SCCP_LIST_ENTRY(sccp_hint_SubscribingDevice_t) list;			/*!< Hint Subscribing Device Linked List Entry */
};										/*!< SCCP Hint Subscribing Device Structure */

/*!
 * \brief SCCP Hint List Type Definition
 */
typedef struct sccp_hint_list sccp_hint_list_t;

/*!
 * \brief SCCP Hint List Structure
 *
 * we hold a mailbox event subscription in sccp_mailbox_subscription_t.
 * Each line that holds a subscription for this mailbox is listed in
 *
 */
struct sccp_hint_list {
	ast_mutex_t lock;							/*!< Asterisk Lock */

	char exten[SCCP_MAX_EXTENSION];						/*!< Extension for Hint */
	char context[SCCP_MAX_CONTEXT];						/*!< Context for Hint */
	char hint_dialplan[256];						/*!< e.g. IAX2/station123 */

	sccp_channelState_t currentState;					/*!< current State */
	sccp_channelState_t previousState;					/*!< current State */
	sccp_hinttype_t hintType;						/*!< Type of Hint */

	/*!
	 * \brief Call Information Structure
	 */
	struct {
		char callingPartyName[StationMaxNameSize];			/*!< Calling Party Name */
		char calledPartyName[StationMaxNameSize];			/*!< Called Party Name */
		char callingParty[StationMaxNameSize];				/*!< Calling Party Number */
		char calledParty[StationMaxNameSize];				/*!< Called Party Number */
		skinny_calltype_t calltype;					/*!< Skinny Call Type */
	} callInfo;								/*!< Call Information Structure */

	/*!
	 * \brief SCCP Hint Type Structure
	 */
	union sccp_hint_type {
		struct {
			char lineName[SCCP_MAX_EXTENSION];			/*!< Line Name */
		} internal;							/*!< Hint Type Internal Structure */

		struct {
			int hintid;						/*!< Hint ID */
#ifndef AST_EVENT_IE_CIDNAME
			pthread_t notificationThread;				/*!< Notification Thread */
#else
			struct pbx_event_sub *device_state_sub;
#endif
		} asterisk;							/*!< Hint Type Asterisk Structure */
	} type;									/*!< Hint Type Structure */

	 SCCP_LIST_HEAD(, sccp_hint_SubscribingDevice_t) subscribers;		/*!< Hint Type Subscribers Linked List Entry */
	 SCCP_LIST_ENTRY(sccp_hint_list_t) list;				/*!< Hint Type Linked List Entry */
};										/*!< SCCP Hint List Structure */

/*!
 * \brief Hint State for Device
 * \param context Context as char
 * \param exten Extension as char
 * \param state State as Asterisk Extension State
 * \param data Asterisk Data
 * \return Status as int
 */
#if ASTERISK_VERSION_NUMBER >= 11001
int sccp_hint_state(const char *context, const char *exten, enum ast_extension_states state, void *data);
#else
int sccp_hint_state(char *context, char *exten, enum ast_extension_states state, void *data);
#endif

#define sccp_hint_lineStatusChanged(a,b,c,d,e) sccp_hint_lineStatusChangedDebug(a,b,c,d,e, __FILE__, __LINE__)
void sccp_hint_lineStatusChangedDebug(sccp_line_t * line, sccp_device_t * device, sccp_channel_t * channel, sccp_channelState_t previousState, sccp_channelState_t state, char *callerFile, int callerLine);
void sccp_hint_module_start(void);
void sccp_hint_module_stop(void);

#    endif										/* SCCP_HINT_H_ */
