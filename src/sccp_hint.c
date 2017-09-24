/*!
 * \file        sccp_hint.c
 * \brief       SCCP Hint Class
 * \author      Marcello Ceschia < marcello.ceschia@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note        For more information about how does hint update works, see \ref hint_update 
 * \since       2009-01-16
 * \remarks     Purpose:        SCCP Hint
 *              When to use:    Does the business of hint status
 *
 */

/*!
 * \section hint_update How does hint update work
 *
 * Getting hint information for display the various connected devices (e.g., 7960 or 7914) varies from PBX implementation to implementation.  
 * In pure Asterisk and its derivatives, hint processing is needed to provide the information for the button, and can be accomplished as simply 
 * as adding "exten => 581,hint,SCCP/581" in the default (possibly "from-internal") internal dial-plan.  
 * Monitoring non-SCCP devices is possible by reviewing the hint status in the Asterisk CLI using the "core show hints" command.  
 * Anything that generates a hint can be monitored using the buttons.  
 *
 * \todo (Page needs to be re-written)
 */

#include "config.h"
#include "common.h"
#include "sccp_hint.h"
SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_indicate.h"											// only for SCCP_CHANNELSTATE_Idling
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

#if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H) 	// ast_event_subscribe
#  include <asterisk/event.h>
#endif

/* ========================================================================================================================= Struct Definitions */
/*!
 *\brief SCCP Hint Subscribing Device Structure
 */
typedef struct sccp_hint_SubscribingDevice sccp_hint_SubscribingDevice_t;
typedef struct sccp_hint_list sccp_hint_list_t;
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
static char default_eid_str[32];
#endif

struct sccp_hint_SubscribingDevice 
{
	SCCP_LIST_ENTRY (sccp_hint_SubscribingDevice_t) list;							/*!< Hint Subscribing Device Linked List Entry */
	sccp_device_t *device;											/*!< SCCP Device */
	uint8_t instance;											/*!< Instance */
	uint8_t positionOnDevice;										/*!< Instance */
};														/*!< SCCP Hint Subscribing Device Structure */

/*!
 *\brief SCCP Hint Line State Structure
 */
struct sccp_hint_lineState 
{
	sccp_line_t *line;
	sccp_channelstate_t state;

	/*!
	 * \brief Call Information Structure
	 */
	struct {
		char partyName[StationMaxNameSize];								/*!< Party Name */
		char partyNumber[StationMaxNameSize];								/*!< Party Number */
		skinny_calltype_t calltype;									/*!< Skinny Call Type */
	} callInfo;												/*!< Call Information Structure */

	SCCP_LIST_ENTRY (struct sccp_hint_lineState) list;							/*!< Hint Type Linked List Entry */
};

/*!
 * \brief SCCP Hint List Structure
 */
struct sccp_hint_list {
	//pbx_mutex_t lock;                                                                                       /*!< Asterisk Lock */

	char exten[SCCP_MAX_EXTENSION];										/*!< Extension for Hint */
	char context[SCCP_MAX_CONTEXT];										/*!< Context for Hint */
	char hint_dialplan[256];										/*!< e.g. IAX2/station123 */

	sccp_channelstate_t currentState;									/*!< current State */
	sccp_channelstate_t previousState;									/*!< current State */

	/*!
	 * \brief Call Information Structure
	 */
	//struct {
	//	char partyNumber[StationMaxNameSize];								/*!< Calling Party Name */
	//	char partyName[StationMaxNameSize];								/*!< Called Party Name */
	//	skinny_calltype_t calltype;									/*!< Skinny Call Type */
	//} callInfo;												/*!< Call Information Structure */
	sccp_callinfo_t *callInfo;
	skinny_calltype_t calltype;										/*!< Skinny Call Type */

	int stateid;												/*!< subscription id in asterisk */
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	PBX_EVENT_SUBSCRIPTION *device_state_sub;									/*!< asterisk distributed device state subscription */
#endif

	SCCP_LIST_HEAD (, sccp_hint_SubscribingDevice_t) subscribers;						/*!< Hint Type Subscribers Linked List Entry */
	SCCP_LIST_ENTRY (sccp_hint_list_t) list;								/*!< Hint Type Linked List Entry */
};														/*!< SCCP Hint List Structure */

/* ========================================================================================================================= Declarations */
static void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForMultipleChannels(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForSingleChannel(struct sccp_hint_lineState *lineState);
static void sccp_hint_checkForDND(struct sccp_hint_lineState *lineState);
static sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context);
static void sccp_hint_notifySubscribers(sccp_hint_list_t * hint);			/* old */
static void sccp_hint_notifyLineStateUpdate(struct sccp_hint_lineState *linestate); 	/* new */
static void sccp_hint_deviceRegistered(const sccp_device_t * device);
static void sccp_hint_deviceUnRegistered(const char *deviceName);
static void sccp_hint_addSubscription4Device(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice);
static void sccp_hint_attachLine(sccp_line_t * line, sccp_device_t * device);
static void sccp_hint_detachLine(sccp_line_t * line, sccp_device_t * device);
static void sccp_hint_lineStatusChanged(sccp_line_t * line, sccp_device_t * device);
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event);
static void sccp_hint_eventListener(const sccp_event_t * event);
#ifdef CS_DYNAMIC_SPEEDDIAL
static gcc_inline boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice);
#endif

#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
#if ASTERISK_VERSION_GROUP >= 112
static void sccp_hint_distributed_devstate_cb(void *data, struct stasis_subscription *sub, struct stasis_message *msg)
#else
static void sccp_hint_distributed_devstate_cb(const pbx_event_t * event, void *data)
#endif
{
	sccp_hint_list_t *hint = (sccp_hint_list_t *) data;
	const char *cidName;
	const char *cidNumber;
	//enum ast_device_state state;		/* maybe we should store the last state */
	
#if ASTERISK_VERSION_GROUP >= 112
	struct ast_device_state_message *dev_state = stasis_message_data(msg);
	if (ast_device_state_message_type() != stasis_message_type(msg)) {
		return;
	}
	if (dev_state->eid) {
		return;
	}
	//eid = dev_state->eid;
	//state = dev_state->state;
	//cidName = dev_state->
	//cidNumber = dev_state->
	cidName = "";
	cidNumber = "";
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "Got new hint event %s, cidname: %s, cidnum: %s\n", hint->hint_dialplan, cidName ? cidName : "NULL", cidNumber ? cidNumber : "NULL");
#else
	const struct ast_eid *eid;
	eid = ast_event_get_ie_raw(event, AST_EVENT_IE_EID);
	//state = pbx_event_get_ie_uint(ast_event, AST_EVENT_IE_STATE);
#if ASTERISK_VERSION_GROUP >= 108
	cidName = pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNAME);
	cidNumber = pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNUM);
#else
	cidName = "";
	cidNumber = "";
#endif
	char eid_str[32] = "";
	ast_eid_to_str(eid_str, sizeof(eid_str), (struct ast_eid *) eid);
	if (!ast_eid_cmp(&ast_eid_default, eid)) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "Skipping distribute devstate update from EID:'%s', MYEID:'%s' (i.e. myself)\n", eid_str, default_eid_str);
		return;
	}
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "Got new hint event %s, cidname: %s, cidnum: %s, originated from EID:'%s'\n", hint->hint_dialplan, cidName ? cidName : "NULL", cidNumber ? cidNumber : "NULL", eid_str);
#endif
	
	if (hint->callInfo) {
		if (hint->calltype == SKINNY_CALLTYPE_INBOUND) {
			iCallInfo.Setter(hint->callInfo, SCCP_CALLINFO_CALLINGPARTY_NAME, cidName, SCCP_CALLINFO_CALLINGPARTY_NUMBER, cidNumber, SCCP_CALLINFO_KEY_SENTINEL);
		} else {
			iCallInfo.Setter(hint->callInfo, SCCP_CALLINFO_CALLEDPARTY_NAME, cidName, SCCP_CALLINFO_CALLEDPARTY_NUMBER, cidNumber, SCCP_CALLINFO_KEY_SENTINEL);
		}
	}

	return;
}
#endif

/* ========================================================================================================================= List Declarations */
static SCCP_LIST_HEAD (, struct sccp_hint_lineState) lineStates;
static SCCP_LIST_HEAD (, sccp_hint_list_t) sccp_hint_subscriptions;

/* ========================================================================================================================= Module Start/Stop */
/*!
 * \brief starting hint-module
 */
void sccp_hint_module_start(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Starting hint system\n");
	SCCP_LIST_HEAD_INIT(&lineStates);
	SCCP_LIST_HEAD_INIT(&sccp_hint_subscriptions);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED, sccp_hint_eventListener, TRUE);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_hint_handleFeatureChangeEvent, TRUE);
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	ast_eid_to_str(default_eid_str, sizeof(default_eid_str), &ast_eid_default);
#endif
}

/*!
 * \brief stop hint-module
 *
 */
void sccp_hint_module_stop(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Stopping hint system\n");
	{
		struct sccp_hint_lineState *lineState;

		SCCP_LIST_LOCK(&lineStates);
		while ((lineState = SCCP_LIST_REMOVE_HEAD(&lineStates, list))) {
			if (lineState->line) {
				sccp_line_release(&lineState->line);		/* explicit release*/
			}
			sccp_free(lineState);
		}
		SCCP_LIST_UNLOCK(&lineStates);
	}

	{
		sccp_hint_list_t *hint;
		sccp_hint_SubscribingDevice_t *subscriber;

		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		while ((hint = SCCP_LIST_REMOVE_HEAD(&sccp_hint_subscriptions, list))) {
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
			pbx_event_unsubscribe(hint->device_state_sub);
#endif
			ast_extension_state_del(hint->stateid, NULL);

			// All subscriptions that have this device should be removed, force cleanup 
			SCCP_LIST_LOCK(&hint->subscribers);
			while ((subscriber = SCCP_LIST_REMOVE_HEAD(&hint->subscribers, list))) {
				AUTO_RELEASE(sccp_device_t, device , sccp_device_retain((sccp_device_t *) subscriber->device));

				if (device) {
					sccp_device_release(&subscriber->device);		/* explicit release*/
					sccp_free(subscriber);
				}
			}
			SCCP_LIST_UNLOCK(&hint->subscribers);
			SCCP_LIST_HEAD_DESTROY(&hint->subscribers);
			iCallInfo.Destructor(&hint->callInfo);
			sccp_free(hint);
		}
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED, sccp_hint_eventListener);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_hint_handleFeatureChangeEvent);

	SCCP_LIST_HEAD_DESTROY(&lineStates);
	SCCP_LIST_HEAD_DESTROY(&sccp_hint_subscriptions);
}

/* ========================================================================================================================= PBX Callbacks */
/*!
 * \brief asterisk callback for extension state changes (we subscribed with ast_extension_state_add)
 */
#if ASTERISK_VERSION_GROUP >= 111
/*!
 * \param context extension context (char *)
 * \param id extension (char *)
 * \param info ast_state_cb_info
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
#ifdef CS_AST_HAS_EXTENSION_STATE_CB_TYPE_CONST_CHAR
static int sccp_hint_devstate_cb(const char *context, const char *id, struct ast_state_cb_info *info, void *data)
#else
static int sccp_hint_devstate_cb(char *context, char *id, struct ast_state_cb_info *info, void *data)
#endif
#elif ASTERISK_VERSION_GROUP >= 110
/*!
 * \param context extension context (const char *)
 * \param id extension (const char *)
 * \param state ast_extension_state (enum)
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
static int sccp_hint_devstate_cb(const char *context, const char *id, enum ast_extension_states state, void *data)
#else
/*!
 * \param context extension context (char *)
 * \param id extension (char *)
 * \param state ast_extension_state (enum)
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
static int sccp_hint_devstate_cb(char *context, char *id, enum ast_extension_states state, void *data)
#endif
{
	sccp_hint_list_t *hint;
	int extensionState;
	//char hintStr[SCCP_MAX_EXTENSION];
	//const char *cidName;
	//const char *cidNumber;
	char cidName[StationMaxNameSize] = "";
	char cidNumber[StationMaxDirnumSize] = "";

	hint = (sccp_hint_list_t *) data;

#if ASTERISK_VERSION_GROUP >= 111
	extensionState = info->exten_state;
#else
	extensionState = state;
#endif

	if (hint->callInfo) {
		if (hint->calltype == SKINNY_CALLTYPE_INBOUND) {
			iCallInfo.Getter(hint->callInfo, 
				SCCP_CALLINFO_CALLINGPARTY_NAME, &cidName, 
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cidNumber, 
				SCCP_CALLINFO_KEY_SENTINEL);
		} else {
			iCallInfo.Getter(hint->callInfo, 
				SCCP_CALLINFO_CALLEDPARTY_NAME, &cidName, 
				SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cidNumber, 
				SCCP_CALLINFO_KEY_SENTINEL);
		}
	}

	/* save previousState */
	hint->previousState = hint->currentState;

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_2 "%s (hint_devstate_cb) Got new hint event %s, state: %d (%s), cidname: %s, cidnum: %s\n", hint->exten, hint->hint_dialplan, extensionState, ast_extension_state2str(extensionState), cidName, cidNumber);
	switch (extensionState) {
		case AST_EXTENSION_REMOVED:
		case AST_EXTENSION_DEACTIVATED:
		case AST_EXTENSION_UNAVAILABLE:
			if (!strncasecmp(cidName, "DND", 3)) {
				hint->currentState = SCCP_CHANNELSTATE_DND;
			} else {
				hint->currentState = SCCP_CHANNELSTATE_CONGESTION;
			}
			break;
		case AST_EXTENSION_NOT_INUSE:
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			break;
		case AST_EXTENSION_INUSE:
			if (SCCP_CHANNELSTATE_ONHOOK == hint->previousState || SCCP_CHANNELSTATE_DOWN == hint->previousState) {
				hint->currentState = SCCP_CHANNELSTATE_DIALING;
			} else {
				hint->currentState = SCCP_CHANNELSTATE_CONNECTED;
			}
			break;
		case AST_EXTENSION_BUSY:
			if (!strncasecmp(cidName, "DND", 3)) {
				hint->currentState = SCCP_CHANNELSTATE_DND;
			} else {
				hint->currentState = SCCP_CHANNELSTATE_BUSY;
			}
			break;
		case AST_EXTENSION_INUSE + AST_EXTENSION_RINGING:
		case AST_EXTENSION_RINGING:
			hint->currentState = SCCP_CHANNELSTATE_RINGING;
			break;
		case AST_EXTENSION_INUSE + AST_EXTENSION_ONHOLD:
		case AST_EXTENSION_ONHOLD:
			hint->currentState = SCCP_CHANNELSTATE_HOLD;
			break;
	}

	sccp_hint_notifySubscribers(hint);
	return 0;
}

/* ===================================================================================================================== SCCP Event Dispatchers */
/*!
 * \brief Event Listener for Hints
 * \param event SCCP Event
 * 
 */
static void sccp_hint_eventListener(const sccp_event_t * event)
{
	sccp_device_t *device = NULL;

	if (!event) {
		return;
	}
	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->event.deviceRegistered.device;

			sccp_hint_deviceRegistered(device);
			break;
		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->event.deviceRegistered.device;

			if (device) {
				char *deviceName = pbx_strdupa(device->id);

				sccp_hint_deviceUnRegistered(deviceName);
			}

			break;
		case SCCP_EVENT_DEVICE_ATTACHED:
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_2 "%s (hint_eventListener) device %s attached on line %s\n", DEV_ID_LOG(event->event.deviceAttached.linedevice->device), event->event.deviceAttached.linedevice->device->id, event->event.deviceAttached.linedevice->line->name);
			sccp_hint_attachLine(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device);
			break;
		case SCCP_EVENT_DEVICE_DETACHED:
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_2 "%s (hint_eventListener) device %s detached from line %s\n", DEV_ID_LOG(event->event.deviceAttached.linedevice->device), event->event.deviceAttached.linedevice->device->id, event->event.deviceAttached.linedevice->line->name);
			sccp_hint_detachLine(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device);
			break;
		case SCCP_EVENT_LINESTATUS_CHANGED:
			pbx_rwlock_rdlock(&GLOB(lock));
			if (!GLOB(reload_in_progress)) {									/* skip processing hints when reloading */
				sccp_hint_lineStatusChanged(event->event.lineStatusChanged.line, event->event.lineStatusChanged.optional_device);
			}
			pbx_rwlock_unlock(&GLOB(lock));
			break;
		default:
			break;
	}
}

/* ========================================================================================================================= Event Handlers */

/* ========================================================================================================================= Event Handlers : Device */
/*!
 * \brief Handle Hints for Device Register
 * \param device SCCP Device
 * 
 * \note device locked by parent
 * 
 * \warning
 *  - device->buttonconfig is not always locked
 */
static void sccp_hint_deviceRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	uint8_t positionOnDevice = 0;

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) device));

	if (d) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			positionOnDevice++;

			if (config->type == SPEEDDIAL && !sccp_strlen_zero(config->button.speeddial.hint)) {
				sccp_hint_addSubscription4Device(device, config->button.speeddial.hint, config->instance, positionOnDevice);
			}
		}
	}
}

/*!
 * \brief Handle Hints for Device UnRegister
 * \param deviceName Device as Char *
 * \note device locked by parent
 *
 */
static void sccp_hint_deviceUnRegistered(const char *deviceName)
{
	sccp_hint_list_t *hint = NULL;
	sccp_hint_SubscribingDevice_t *subscriber;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {

		/* All subscriptions that have this device should be removed */
		SCCP_LIST_LOCK(&hint->subscribers);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
			if (subscriber->device && !strcasecmp(subscriber->device->id, deviceName)) {
				sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_2 "%s: Freeing subscriber from hint exten: %s in %s\n", deviceName, hint->exten, hint->context);
				SCCP_LIST_REMOVE_CURRENT(list);
				sccp_device_release(&subscriber->device);		/* explicit release*/
				sccp_free(subscriber);
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
		SCCP_LIST_UNLOCK(&hint->subscribers);
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

/*!
 * \brief Subscribe to a Hint
 * \param device SCCP Device
 * \param hintStr Asterisk Hint Name as char
 * \param instance Instance as int
 * \param positionOnDevice button index on device (used to detect devicetype)
 * 
 * \warning
 *  - sccp_hint_subscriptions is not always locked
 * 
 * \note called with retained device
 */
static void sccp_hint_addSubscription4Device(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice)
{
	sccp_hint_list_t *hint = NULL;

	char buffer[256] = "";
	char *splitter, *hint_exten, *hint_context;

	sccp_copy_string(buffer, hintStr, sizeof(buffer));

	/* get exten and context */
	splitter = buffer;
	hint_exten = strsep(&splitter, "@");
	if (hint_exten) {
		pbx_strip(hint_exten);
	}
	hint_context = splitter;
	if (hint_context) {
		pbx_strip(hint_context);
	} else {
		hint_context = GLOB(context);
	}

	/* skip subscribtions to already owned line */
	/*      sccp_buttonconfig_t *config;
	   SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
	   if (config->type == LINE && sccp_strcaseequals(config->button.line.name, hint_exten)) {
	   sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_addSubscription4Device) Skipping Hint Registration for Line we already have this connected (%s, %s).\n", DEV_ID_LOG(device), config->button.line.name, hint_exten);
	   return;
	   }
	   } */
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_addSubscription4Device) Dialplan %s for exten: %s and context: %s\n", DEV_ID_LOG(device), hintStr, hint_exten, hint_context);

	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (sccp_strlen(hint_exten) == sccp_strlen(hint->exten)
		    && sccp_strlen(hint_context) == sccp_strlen(hint->context)
		    && sccp_strequals(hint_exten, hint->exten)
		    && sccp_strequals(hint_context, hint->context)) {
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_addSubscription4Device) Hint found for exten '%s@%s'\n", DEV_ID_LOG(device), hint_exten, hint_context);
			break;
		}
	}

	/* we have no hint */
	if (!hint) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_addSubscription4Device) create new hint for %s@%s\n", DEV_ID_LOG(device), hint_exten, hint_context);
		hint = sccp_hint_create(hint_exten, hint_context);
		if (!hint) {
			pbx_log(LOG_NOTICE, "%s (hint_addSubscription4Device) hint create failed for %s@%s\n", DEV_ID_LOG(device), hint_exten, hint_context);
			return;
		}
		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_hint_subscriptions, hint, list);
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	/* add subscribing device */
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_addSubscription4Device) create subscriber or hint: %s in %s\n", DEV_ID_LOG(device), hint->exten, hint->context);
	sccp_hint_SubscribingDevice_t *subscriber;

	subscriber = sccp_calloc(sizeof *subscriber, 1);
	if (!subscriber) {
		pbx_log(LOG_ERROR, "%s (hint_addSubscription4Device) Memory Allocation Error while creating subscriber object\n", DEV_ID_LOG(device));
		return;
	}

	subscriber->device = sccp_device_retain((sccp_device_t *) device);
	subscriber->instance = instance;
	subscriber->positionOnDevice = positionOnDevice;

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_addSubscription4Device) Adding subscription for hint %s@%s\n", DEV_ID_LOG(device), hint->exten, hint->context);
	SCCP_LIST_INSERT_HEAD(&hint->subscribers, subscriber, list);

	sccp_dev_set_keyset(device, subscriber->instance, 0, KEYMODE_ONHOOK);

	sccp_hint_notifySubscribers(hint);
}

/*!
 * \brief create a hint structure
 * \param hint_exten Hint Extension as char
 * \param hint_context Hint Context as char
 * \return SCCP Hint Linked List
 */
static sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context)
{
	sccp_hint_list_t *hint = NULL;
	char hint_dialplan[256] = "";

	if (sccp_strlen_zero(hint_exten)) {
		return NULL;
	}
	if (sccp_strlen_zero(hint_context)) {
		hint_context = GLOB(context);
	}
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_create) Create hint for exten: %s context: %s\n", hint_exten, hint_context);

	int res = pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
	// CS_AST_HAS_NEW_HINT

	if (!res || sccp_strlen_zero(hint_dialplan)) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_create) No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
		return NULL;
	}

	/* remove the end after comma (i.e. cutting of the CustomPresence part from 'exten => 143,hint,SCCP/143,CustomPresence:143') */
	int i=0;
	while (hint_dialplan[i++]) {
		if (hint_dialplan[i] == ',') {
			hint_dialplan[i] = '\0';
			break;
		}
	}

	hint = sccp_calloc(sizeof *hint, 1);
	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_create) Memory Allocation Error while creating hint list for hint: %s@%s\n", hint_exten, hint_context);
		return NULL;
	}
	if (!(hint->callInfo = iCallInfo.Constructor(0))) {
		sccp_free(hint);
		return NULL;
	}
	hint->calltype = SKINNY_CALLTYPE_SENTINEL;

	SCCP_LIST_HEAD_INIT(&hint->subscribers);
	//sccp_mutex_init(&hint->lock);

	sccp_copy_string(hint->exten, hint_exten, sizeof(hint->exten));
	sccp_copy_string(hint->context, hint_context, sizeof(hint->context));
	sccp_copy_string(hint->hint_dialplan, hint_dialplan, sizeof(hint_dialplan));

	/* subscripbe to the hint */
	hint->stateid = pbx_extension_state_add(hint->context, hint->exten, sccp_hint_devstate_cb, hint);

#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	/* subscripbe to the distributed hint event */
#if ASTERISK_VERSION_GROUP >= 112
	struct stasis_topic *devstate_hint_dialplan = ast_device_state_topic(hint->hint_dialplan);
	hint->device_state_sub = stasis_subscribe(devstate_hint_dialplan, sccp_hint_distributed_devstate_cb, hint);
#else
	hint->device_state_sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_hint_distributed_devstate_cb, "sccp_hint_distributed_devstate_cb", hint, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan, AST_EVENT_IE_END);
#endif
#endif

	/* force hint update to get currentState */
#if ASTERISK_VERSION_GROUP >= 111
	struct ast_state_cb_info info;

	info.exten_state = pbx_extension_state(NULL, hint->context, hint->exten);
	sccp_hint_devstate_cb(hint->context, hint->exten, &info, hint);
#else
	enum ast_extension_states state = pbx_extension_state(NULL, hint->context, hint->exten);

	sccp_hint_devstate_cb(hint->context, hint->exten, state, hint);
#endif
	return hint;
}

/* ========================================================================================================================= Event Handlers : LineState */
static void sccp_hint_attachLine(sccp_line_t * line, sccp_device_t * device) 
{
	struct sccp_hint_lineState *lineState = NULL;

	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (lineState->line == line) {
			break;
		}
	}
	if (!lineState) {		/* create new lineState if necessary */
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_attachLine) Create new hint_lineState for line: %s\n", DEV_ID_LOG(device), line->name);
		lineState = sccp_calloc(sizeof *lineState, 1);
		if (!lineState) {
			pbx_log(LOG_ERROR, "%s (hint_attachLine) Memory Allocation Error while creating hint-lineState object for line %s\n", DEV_ID_LOG(device), line->name);
			SCCP_LIST_UNLOCK(&lineStates);
			return;
		}
		SCCP_LIST_INSERT_HEAD(&lineStates, lineState, list);
	}

	if (!lineState->line) {		/* retain one instance of line in lineState->line */
		//sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_attachLine) attaching line: %s\n", DEV_ID_LOG(device), line->name);
		lineState->line = sccp_line_retain(line);
	}
	SCCP_LIST_UNLOCK(&lineStates);
	
	sccp_hint_lineStatusChanged(line, device);
}

static void sccp_hint_detachLine(sccp_line_t * line, sccp_device_t * device) 
{
	sccp_hint_lineStatusChanged(line, device);
	struct sccp_hint_lineState *lineState = NULL;
	
	if (line->statistic.numberOfActiveDevices == 0) {		/* release last instance of lineState->line */
		//sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_detachLine) detaching line: %s, \n", DEV_ID_LOG(device), line->name);
		SCCP_LIST_LOCK(&lineStates);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&lineStates, lineState, list) {
			if (lineState->line == line) {
				//sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_detachLine) line: %s detached\n", DEV_ID_LOG(device), line->name);
				if (lineState->line) {
					sccp_line_release(&lineState->line);		/* explicit release*/
				}
				SCCP_LIST_REMOVE_CURRENT(list);
				sccp_free(lineState)
				break;
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
		SCCP_LIST_UNLOCK(&lineStates);
	}
}

/*!
 * \brief Handle line status change
 * \param line SCCP Line that was changed
 * \param device SCCP Device who initialied the change
 * 
 */
static void sccp_hint_lineStatusChanged(sccp_line_t * line, sccp_device_t * device)
{
	struct sccp_hint_lineState *lineState = NULL;

	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (lineState->line == line) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&lineStates);
	
	if (lineState && lineState->line) {
		sccp_hint_updateLineState(lineState);
	}
}

/*!
 * \brief Handle Hint Status Update
 * \param lineState SCCP LineState
 */
static void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState)
{
	AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(lineState->line));

	if (line) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineState) Update Line Channel State: %s(%d)\n", line->name, sccp_channelstate2str(lineState->state), lineState->state);

		/* no line, or line without devices */
		if (0 == SCCP_LIST_GETSIZE(&line->devices)) {
			lineState->state = SCCP_CHANNELSTATE_CONGESTION;
			lineState->callInfo.calltype = SKINNY_CALLTYPE_SENTINEL;

			sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_TEMP_FAIL, sizeof(lineState->callInfo.partyName));
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_updateLineState) 0 devices register on linename: %s\n", line->name);

		} else if (SCCP_LIST_GETSIZE(&line->channels) > 1) {
			/* line is currently shared between multiple device and has multiple concurrent calls active */
			sccp_hint_updateLineStateForMultipleChannels(lineState);
		} else {
			/* just one device per line */
			sccp_hint_updateLineStateForSingleChannel(lineState);
		}

		/* push changes to pbx */
		sccp_hint_notifyLineStateUpdate(lineState);
	}
}

/*!
 * \brief set hint status for a line with more then one channel
 * \param lineState SCCP LineState
 */
static void sccp_hint_updateLineStateForMultipleChannels(struct sccp_hint_lineState *lineState)
{
	if (!lineState || !lineState->line) {
		return;
	}
	sccp_line_t *line = lineState->line;

	memset(lineState->callInfo.partyName, 0, sizeof(lineState->callInfo.partyName));
	memset(lineState->callInfo.partyNumber, 0, sizeof(lineState->callInfo.partyNumber));

	/* set default calltype = SKINNY_CALLTYPE_OUTBOUND */
	lineState->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;

	if (SCCP_LIST_GETSIZE(&line->channels) > 0) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForMultipleChannels) number of active channels %d\n", line->name, SCCP_LIST_GETSIZE(&line->channels));
		if (SCCP_LIST_GETSIZE(&line->channels) == 1) {
			SCCP_LIST_LOCK(&line->channels);
			AUTO_RELEASE(sccp_channel_t, channel , SCCP_LIST_FIRST(&line->channels) ? sccp_channel_retain(SCCP_LIST_FIRST(&line->channels)) : NULL);

			SCCP_LIST_UNLOCK(&line->channels);
			lineState->callInfo.calltype = SKINNY_CALLTYPE_SENTINEL;

			if (channel) {
				lineState->callInfo.calltype = channel->calltype;

				if (channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN) {
					lineState->state = channel->state;

					sccp_callinfo_t *ci = sccp_channel_getCallInfo(channel);
					char cid_name[StationMaxNameSize] = {0};
					char cid_num[StationMaxDirnumSize] = {0};
					sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;

					/* set cid name/numbe information according to the call direction */
					if (SKINNY_CALLTYPE_INBOUND == channel->calltype) {
						iCallInfo.Getter(ci, 
							SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name, 
							SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num, 
							SCCP_CALLINFO_PRESENTATION, &presentation, 
							SCCP_CALLINFO_KEY_SENTINEL);
					} else {
						iCallInfo.Getter(ci, 
							SCCP_CALLINFO_CALLEDPARTY_NAME, &cid_name, 
							SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cid_num, 
							SCCP_CALLINFO_PRESENTATION, &presentation, 
							SCCP_CALLINFO_KEY_SENTINEL);
					}
					if (presentation == CALLERID_PRESENTATION_FORBIDDEN) {
						sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_PRIVATE, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_PRIVATE, sizeof(lineState->callInfo.partyNumber));
					} else {
						sccp_copy_string(lineState->callInfo.partyName, cid_name, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, cid_num, sizeof(lineState->callInfo.partyNumber));
					}
				} else {
					lineState->state = SCCP_CHANNELSTATE_ONHOOK;
				}
			} else {
				lineState->state = SCCP_CHANNELSTATE_ONHOOK;
			}
		} else if (SCCP_LIST_GETSIZE(&line->channels) > 1) {

			/** we have multiple channels, so do not set cid information */
			//sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyName));
			//sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyNumber));
			lineState->state = SCCP_CHANNELSTATE_CONNECTED;
		}
	} else {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForMultipleChannels) no active channels\n", line->name);
		lineState->state = SCCP_CHANNELSTATE_ONHOOK;
	}

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForMultipleChannels) Set sharedLineState to %s(%d)\n", line->name, sccp_channelstate2str(lineState->state), lineState->state);
}

/*!
 * \brief set hint status for a line with less or eq one channel
 * \param lineState SCCP LineState
 * 
 */
static void sccp_hint_updateLineStateForSingleChannel(struct sccp_hint_lineState *lineState)
{
	if (!lineState || !lineState->line) {
		return;
	}
	sccp_line_t *line = lineState->line;
	sccp_channelstate_t state;

	//boolean_t dev_privacy = FALSE;

	/** clear cid information */
	memset(lineState->callInfo.partyName, 0, sizeof(lineState->callInfo.partyName));
	memset(lineState->callInfo.partyNumber, 0, sizeof(lineState->callInfo.partyNumber));

	AUTO_RELEASE(sccp_channel_t, channel , NULL);

	if (SCCP_LIST_GETSIZE(&line->channels) > 0) {
		SCCP_LIST_LOCK(&line->channels);
		channel = SCCP_LIST_LAST(&line->channels) ? sccp_channel_retain(SCCP_LIST_LAST(&line->channels)) : NULL;
		SCCP_LIST_UNLOCK(&line->channels);
	}

	if (channel) {
		lineState->callInfo.calltype = channel->calltype;
		state = channel->state;

		SCCP_LIST_LOCK(&line->devices);
		AUTO_RELEASE(sccp_linedevices_t, lineDevice , SCCP_LIST_FIRST(&line->devices) ? sccp_linedevice_retain(SCCP_LIST_FIRST(&line->devices)) : NULL);

		SCCP_LIST_UNLOCK(&line->devices);
		if (lineDevice) {
			AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(lineDevice->device));

			if (device) {
				if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
					state = SCCP_CHANNELSTATE_DND;
				}
				//dev_privacy = device->privacyFeature.enabled;
			}
		}
		switch (state) {
			case SCCP_CHANNELSTATE_DOWN:
				state = SCCP_CHANNELSTATE_ONHOOK;
				lineState->callInfo.calltype = SKINNY_CALLTYPE_SENTINEL;
				break;
			case SCCP_CHANNELSTATE_SPEEDDIAL:
				break;
			case SCCP_CHANNELSTATE_ONHOOK:
				lineState->callInfo.calltype = SKINNY_CALLTYPE_SENTINEL;
				break;
			case SCCP_CHANNELSTATE_DND:
				lineState->callInfo.calltype = SKINNY_CALLTYPE_INBOUND;
				sccp_copy_string(lineState->callInfo.partyName, "DND", sizeof(lineState->callInfo.partyName));
				sccp_copy_string(lineState->callInfo.partyNumber, "DND", sizeof(lineState->callInfo.partyNumber));
				break;
			case SCCP_CHANNELSTATE_OFFHOOK:
			case SCCP_CHANNELSTATE_GETDIGITS:
			case SCCP_CHANNELSTATE_RINGOUT:
			case SCCP_CHANNELSTATE_RINGOUT_ALERTING:
			case SCCP_CHANNELSTATE_CONNECTED:
			case SCCP_CHANNELSTATE_PROGRESS:
			case SCCP_CHANNELSTATE_PROCEED:
			case SCCP_CHANNELSTATE_RINGING:
			case SCCP_CHANNELSTATE_DIALING:
			case SCCP_CHANNELSTATE_DIGITSFOLL:
			case SCCP_CHANNELSTATE_BUSY:
			case SCCP_CHANNELSTATE_HOLD:
			case SCCP_CHANNELSTATE_CONGESTION:
			case SCCP_CHANNELSTATE_CALLWAITING:
			case SCCP_CHANNELSTATE_CALLPARK:
			case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			case SCCP_CHANNELSTATE_INVALIDNUMBER:
			case SCCP_CHANNELSTATE_CALLCONFERENCE:
			case SCCP_CHANNELSTATE_CALLTRANSFER: 
			{
				sccp_callinfo_t *ci = sccp_channel_getCallInfo(channel);
				char cid_name[StationMaxNameSize] = {0};
				char cid_num[StationMaxDirnumSize] = {0};
				sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;
				//if (dev_privacy == 0 || (dev_privacy == 1 && channel->privacy == FALSE)) {

				/** set cid name/number information according to the call direction */
				switch (channel->calltype) {
					case SKINNY_CALLTYPE_INBOUND:
						iCallInfo.Getter(ci, 
							SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name, 
							SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num, 
							SCCP_CALLINFO_PRESENTATION, &presentation, 
							SCCP_CALLINFO_KEY_SENTINEL);
						sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s: get speeddial party: '%s <%s>' (callingParty)\n", line->name, cid_name, cid_num);
						break;
					case SKINNY_CALLTYPE_OUTBOUND:
						iCallInfo.Getter(ci, 
							SCCP_CALLINFO_CALLEDPARTY_NAME, &cid_name, 
							SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cid_num, 
							SCCP_CALLINFO_PRESENTATION, &presentation, 
							SCCP_CALLINFO_KEY_SENTINEL);
						sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s: get speeddial party: '%s <%s>' (calledParty)\n", line->name, cid_name, cid_num);
						break;
					case SKINNY_CALLTYPE_FORWARD:
						sccp_copy_string(cid_name, "cfwd", sizeof(cid_name));
						sccp_copy_string(cid_num, "cfwd", sizeof(cid_num));
						sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s: get speedial partyName: cfwd\n", line->name);
						break;
					case SKINNY_CALLTYPE_SENTINEL:
						break;
				}
				if (presentation == CALLERID_PRESENTATION_FORBIDDEN) {
					sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_PRIVATE, sizeof(lineState->callInfo.partyName));
					sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_PRIVATE, sizeof(lineState->callInfo.partyNumber));
				} else {
					sccp_copy_string(lineState->callInfo.partyName, cid_name, sizeof(lineState->callInfo.partyName));
					sccp_copy_string(lineState->callInfo.partyNumber, cid_num, sizeof(lineState->callInfo.partyNumber));
				}
			}
				break;
			case SCCP_CHANNELSTATE_BLINDTRANSFER:
			case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
			case SCCP_CHANNELSTATE_ZOMBIE:
			case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
			case SCCP_CHANNELSTATE_SENTINEL:
				/* unused states */
				break;
		}

		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForSingleChannel) partyName: %s, partyNumber: %s\n", line->name, lineState->callInfo.partyName, lineState->callInfo.partyNumber);

		lineState->state = state;
	} else {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForSingleChannel) NO CHANNEL\n", line->name);
		lineState->state = SCCP_CHANNELSTATE_ONHOOK;
		lineState->callInfo.calltype = SKINNY_CALLTYPE_SENTINEL;
		sccp_hint_checkForDND(lineState);
	}													// if(channel)

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_updateLineStateForSingleChannel) Set singleLineState to %s(%d)\n", line->name, sccp_channelstate2str(lineState->state), lineState->state);
}

/* ========================================================================================================================= Event Handlers : Feature Change */
/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 * 
 * \warning
 *  - device->buttonconfig is not always locked
 */
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event)
{
	sccp_buttonconfig_t *buttonconfig = NULL;

	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_DND:
			{
				AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(event->event.featureChanged.device));

				if (d) {
					SCCP_LIST_LOCK(&d->buttonconfig);
					SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
						if (buttonconfig->type == LINE) {
							AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(buttonconfig->button.line.name, FALSE));

							if (line) {
								sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s (hint_handleFeatureChangeEvent) Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dndFeature.status ? "on" : "off", line->name);
								sccp_hint_lineStatusChanged(line, d);
							}
						}
					}
					SCCP_LIST_UNLOCK(&d->buttonconfig);
				}
				break;
			}
		default:
			break;
	}
}

static enum ast_device_state sccp_hint_hint2DeviceState(sccp_channelstate_t state)
{
	enum ast_device_state newDeviceState = AST_DEVICE_UNKNOWN;

	switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
		case SCCP_CHANNELSTATE_ONHOOK:
			newDeviceState = AST_DEVICE_NOT_INUSE;
			break;
		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_RINGOUT_ALERTING:
#ifdef CS_EXPERIMENTAL
			newDeviceState = AST_DEVICE_RINGINUSE;
			break;
#endif
		case SCCP_CHANNELSTATE_RINGING:
			newDeviceState = AST_DEVICE_RINGING;
			break;
		case SCCP_CHANNELSTATE_HOLD:
			newDeviceState = AST_DEVICE_ONHOLD;
			break;
		case SCCP_CHANNELSTATE_BUSY:
#ifdef CS_EXPERIMENTAL
			newDeviceState = AST_DEVICE_BUSY;
			break;
#endif
		case SCCP_CHANNELSTATE_DND:
#ifndef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_PROCEED:
		case SCCP_CHANNELSTATE_PROGRESS:
		case SCCP_CHANNELSTATE_GETDIGITS:
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
#else
		        /* and this is wrong too, it should only be busy if DND <<busy>>, not in all cases */
#endif
			newDeviceState = AST_DEVICE_BUSY;
			break;
		case SCCP_CHANNELSTATE_ZOMBIE:
		case SCCP_CHANNELSTATE_CONGESTION:
#ifndef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
#endif
			newDeviceState = AST_DEVICE_UNAVAILABLE;
			break;
#ifdef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_PROCEED:
		case SCCP_CHANNELSTATE_PROGRESS:
		case SCCP_CHANNELSTATE_GETDIGITS:
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
#endif
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
		case SCCP_CHANNELSTATE_OFFHOOK:
		case SCCP_CHANNELSTATE_CONNECTED:
		case SCCP_CHANNELSTATE_BLINDTRANSFER:
		case SCCP_CHANNELSTATE_CALLWAITING:
		case SCCP_CHANNELSTATE_CALLTRANSFER:
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
		case SCCP_CHANNELSTATE_CALLPARK:
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			newDeviceState = AST_DEVICE_INUSE;
			break;
		case SCCP_CHANNELSTATE_SENTINEL:
#ifdef CS_EXPERIMENTAL
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
		        /* returning UNKNOWN */
#endif
			break;
	}
	return newDeviceState;
}

/* ========================================================================================================================= Subscriber Notify : Updates Speeddial */
/*!
 * \brief send hint status to subscriber
 * \param hint SCCP Hint Linked List Pointer
 *
 * \todo Check if the actual device still exists while going throughthe hint->subscribers and not pointing at rubish
 */
static void sccp_hint_notifySubscribers(sccp_hint_list_t * hint)
{
	sccp_hint_SubscribingDevice_t *subscriber = NULL;

	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_notifySubscribers) no hint provided to notifySubscribers about\n");
		return;
	}

	if (!GLOB(module_running) || SCCP_REF_RUNNING != sccp_refcount_isRunning()) {
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_notifySubscribers) Skip processing hint while we are shutting down.\n", hint->exten);
		return;
	}

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s (hint_notifySubscribers) notify %u subscriber(s) of %s's state %s\n", hint->exten, SCCP_LIST_GETSIZE(&hint->subscribers), hint->hint_dialplan, sccp_channelstate2str(hint->currentState));

	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE(&hint->subscribers, subscriber, list) {
		AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) subscriber->device));

		if (d) {
			//sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_notifySubscribers) notify subscriber %s of %s's state %s (%d)\n", DEV_ID_LOG(d), d->id, hint->hint_dialplan, sccp_channelstate2str(hint->currentState), hint->currentState);
#ifdef CS_DYNAMIC_SPEEDDIAL
			sccp_msg_t *msg = NULL;
			sccp_speed_t k;
			char displayMessage[80] = "";
			skinny_busylampfield_state_t status = SKINNY_BLF_STATUS_UNKNOWN;
			if (d->inuseprotocolversion >= 15) {
				sccp_dev_speed_find_byindex( d, subscriber->instance, TRUE, &k);

				char cidName[StationMaxNameSize] = "";
				char cidNumber[StationMaxDirnumSize] = "";

				switch (hint->currentState) {
				case SCCP_CHANNELSTATE_DOWN:
					snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
					status = SKINNY_BLF_STATUS_UNKNOWN;	/* default state */
					break;

				case SCCP_CHANNELSTATE_ONHOOK:
					snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
					status = SKINNY_BLF_STATUS_IDLE;
					break;

				case SCCP_CHANNELSTATE_DND:
					//snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
					snprintf(displayMessage, sizeof(displayMessage), "(DND) %s", k.name);
					status = SKINNY_BLF_STATUS_DND;	/* dnd */
					break;

				case SCCP_CHANNELSTATE_CONGESTION:
					snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
					status = SKINNY_BLF_STATUS_UNKNOWN;	/* device/line not found */
					break;

				case SCCP_CHANNELSTATE_RINGING:
					status = SKINNY_BLF_STATUS_ALERTING;	/* ringin */
					/* fall through */

				default:
#ifdef CS_DYNAMIC_SPEEDDIAL
					if (sccp_hint_isCIDavailabe(d, subscriber->positionOnDevice) == TRUE) {
						if (hint->calltype == SKINNY_CALLTYPE_INBOUND) {
							iCallInfo.Getter(hint->callInfo, 
								SCCP_CALLINFO_CALLINGPARTY_NAME, &cidName, 
								SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cidNumber, 
								SCCP_CALLINFO_KEY_SENTINEL);
						} else {
							iCallInfo.Getter(hint->callInfo, 
								SCCP_CALLINFO_CALLEDPARTY_NAME, &cidName, 
								SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cidNumber, 
								SCCP_CALLINFO_KEY_SENTINEL);
						}
						if (strlen(cidName) > 0) {
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", cidName, (SCCP_CHANNELSTATE_CONNECTED == hint->currentState) ? "<=>" : ((hint->calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->"), k.name);
						} else if (strlen(cidNumber) > 0) {
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", cidNumber, (SCCP_CHANNELSTATE_CONNECTED == hint->currentState) ? "<=>" : ((hint->calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->"), k.name);
						} else {
							snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
						}
					} else 
#endif
					{
						snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
					}
					if (status == SKINNY_BLF_STATUS_UNKNOWN) {	/* still default value --> set */
						status = SKINNY_BLF_STATUS_INUSE;
					}
					break;
				}

				sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_notifySubscribers) notify device: %s@%d, displayMessage:%s, state: %s ->  %s\n", hint->exten, DEV_ID_LOG(d), subscriber->instance, displayMessage, sccp_channelstate2str(hint->currentState), skinny_busylampfield_state2str(status)); 
				/*!
				* hack to fix the white text without shadow issue -MC
				*
				* first send a label which is 1-character shorter than the correct one. 
				* then send another message with a longer label (correct/final label) will force an update (in white over the back drop in black)
				*/
				REQ(msg, FeatureStatDynamicMessage);
				if (msg) {
					sccp_copy_string(msg->data.FeatureStatDynamicMessage.featureTextLabel, displayMessage, sizeof(msg->data.FeatureStatDynamicMessage.featureTextLabel));
					msg->data.FeatureStatDynamicMessage.lel_featureIndex = htolel(subscriber->instance);
					msg->data.FeatureStatDynamicMessage.lel_featureID = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
					msg->data.FeatureStatDynamicMessage.lel_featureStatus = htolel(status);
					msg->data.FeatureStatDynamicMessage.featureTextLabel[strlen(displayMessage)-1] = '\0';
					sccp_dev_send(d, msg);
				} else {
					sccp_free(msg);
				}

				/*!
				 * Send the actual message we wanted to send */
				REQ(msg, FeatureStatDynamicMessage);
				if (msg) {
					sccp_copy_string(msg->data.FeatureStatDynamicMessage.featureTextLabel, displayMessage, sizeof(msg->data.FeatureStatDynamicMessage.featureTextLabel));
					msg->data.FeatureStatDynamicMessage.lel_featureIndex = htolel(subscriber->instance);
					msg->data.FeatureStatDynamicMessage.lel_featureID = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
					msg->data.FeatureStatDynamicMessage.lel_featureStatus = htolel(status);
					sccp_dev_send(d, msg);
				} else {
					sccp_free(msg);
				}
			} else
#endif
			{
				/*
				   we have dynamic speeddial enabled, but subscriber can not handle this.
				   We have to switch back to old hint style and send old state.
				 */
				sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_notifySubscribers) can not handle dynamic speeddial, fall back to old behavior using state %s (%d)\n", DEV_ID_LOG(d), sccp_channelstate2str(hint->currentState), hint->currentState);

				/*
				   With the old hint style we should only use SCCP_CHANNELSTATE_ONHOOK and SCCP_CHANNELSTATE_CALLREMOTEMULTILINE as callstate,
				   otherwise we get a callplane on device -> set all states except onhook to SCCP_CHANNELSTATE_CALLREMOTEMULTILINE -MC
				 */
				skinny_callstate_t iconstate = SKINNY_CALLSTATE_CALLREMOTEMULTILINE;

				switch (hint->currentState) {
					case SCCP_CHANNELSTATE_DOWN:
					case SCCP_CHANNELSTATE_ONHOOK:
						iconstate = SKINNY_CALLSTATE_ONHOOK;
						break;
					case SCCP_CHANNELSTATE_RINGING:
						if (d->allowRinginNotification) {
							iconstate = SKINNY_CALLSTATE_RINGIN;
						}
						break;
					case SCCP_CHANNELSTATE_ZOMBIE:
					case SCCP_CHANNELSTATE_CONGESTION:
					case SCCP_CHANNELSTATE_CONNECTED:
					case SCCP_CHANNELSTATE_OFFHOOK:
					case SCCP_CHANNELSTATE_RINGOUT:
					case SCCP_CHANNELSTATE_RINGOUT_ALERTING:
					case SCCP_CHANNELSTATE_BUSY:
					case SCCP_CHANNELSTATE_HOLD:
					case SCCP_CHANNELSTATE_CALLWAITING:
					case SCCP_CHANNELSTATE_CALLPARK:
					case SCCP_CHANNELSTATE_PROCEED:
					case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
					case SCCP_CHANNELSTATE_INVALIDNUMBER:
					case SCCP_CHANNELSTATE_DIALING:
					case SCCP_CHANNELSTATE_PROGRESS:
					case SCCP_CHANNELSTATE_GETDIGITS:
					case SCCP_CHANNELSTATE_SPEEDDIAL:
					case SCCP_CHANNELSTATE_DIGITSFOLL:
					case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
					case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
					case SCCP_CHANNELSTATE_BLINDTRANSFER:
					case SCCP_CHANNELSTATE_DND:
					case SCCP_CHANNELSTATE_CALLTRANSFER:
					case SCCP_CHANNELSTATE_CALLCONFERENCE:
						iconstate = SKINNY_CALLSTATE_CALLREMOTEMULTILINE;
						break;
					case SCCP_CHANNELSTATE_SENTINEL:
						break;
				}
				sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "%s (hint_notifySubscribers) setting icon to state %s (%d)\n", DEV_ID_LOG(d), skinny_callstate2str(iconstate), iconstate);

				if (SCCP_CHANNELSTATE_RINGING == hint->previousState) {
					/* we send a congestion to the phone, so call will not be marked as missed call */
					sccp_device_sendcallstate(d, subscriber->instance, 0, SKINNY_CALLSTATE_CONGESTION, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
				}

				sccp_device_sendcallstate(d, subscriber->instance, 0, iconstate, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT); /** do not set visibility to COLLAPSED, this will hidde callInfo in state CALLREMOTEMULTILINE */

				if (hint->currentState == SCCP_CHANNELSTATE_ONHOOK || hint->currentState == SCCP_CHANNELSTATE_CONGESTION) {
					sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, subscriber->instance, SKINNY_LAMP_OFF);
					sccp_dev_set_keyset(d, subscriber->instance, 0, KEYMODE_ONHOOK);

				} else if (hint->currentState == SCCP_CHANNELSTATE_RINGING && d->allowRinginNotification) {
					sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, subscriber->instance, SKINNY_LAMP_BLINK);
					sccp_dev_set_keyset(d, subscriber->instance, 0, KEYMODE_INUSEHINT);

				} else {
					iCallInfo.Send(hint->callInfo, 0 /*callid*/, (hint->calltype == SKINNY_CALLTYPE_OUTBOUND) ? SKINNY_CALLTYPE_OUTBOUND : SKINNY_CALLTYPE_INBOUND, subscriber->instance, d, TRUE);
					sccp_device_setLamp(d, SKINNY_STIMULUS_LINE, subscriber->instance, SKINNY_LAMP_ON);
					sccp_dev_set_keyset(d, subscriber->instance, 0 /*callid*/, KEYMODE_INUSEHINT);
				}
			}
		} else {
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifySubscribers) device not found/retained\n");
		}
	}
	SCCP_LIST_UNLOCK(&hint->subscribers);
}

/* ========================================================================================================================= PBX Notify */
/*
 * \brief Notify LineState Change to Subscribers via PBX include distributed devstate
 * calls notifySubscribers via asterisk->callback (sccp_hint_distributed_devstate_cb)
 */
static void sccp_hint_notifySubscribersViaPbx(struct sccp_hint_lineState *lineState, char *lineName, enum ast_device_state newDeviceState)
{
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifySubscribersViaPbx) Notify asterisk to set state to sccp channelstate '%s' (%d) => asterisk: '%s' (%d) on channel SCCP/%s\n", sccp_channelstate2str(lineState->state), lineState->state, pbxsccp_devicestate2str(newDeviceState), newDeviceState, lineState->line->name);
#if defined(CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE) && ASTERISK_VERSION_GROUP < 112		/* no distributed devstate support for ast-12 and up (yet) */
	pbx_event_t *event = pbx_event_new(AST_EVENT_DEVICE_STATE_CHANGE,
		AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, lineName, 
		AST_EVENT_IE_STATE, AST_EVENT_IE_PLTYPE_UINT, newDeviceState, 
#if ASTERISK_VERSION_GROUP >= 108
		AST_EVENT_IE_CEL_CIDNAME, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyName, 
		AST_EVENT_IE_CEL_CIDNUM, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyNumber, 
		AST_EVENT_IE_CEL_USERFIELD, AST_EVENT_IE_PLTYPE_UINT, lineState->callInfo.calltype, 
		AST_EVENT_IE_CEL_EXTRA, AST_EVENT_IE_PLTYPE_STR, sccp_channelstate2str(lineState->state),
#endif
		/* AST_EVENT_IE_PRESENCE_STATE, AST_EVENT_IE_PLTYPE_UINT, ->privacy ?? */
		AST_EVENT_IE_END);
	if (!event) {
		pbx_log(LOG_ERROR, "SCCP: Could not create AST_EVENT_DEVICE_STATE_CHANGE event.\n");
	} else if (!pbx_event_queue_and_cache(event)) {
		pbx_event_destroy(event);
		pbx_log(LOG_ERROR, "SCCP: Could not queue AST_EVENT_DEVICE_STATE_CHANGE event.\n");
	}
#else  /* CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE && ASTERISK_VERSION_GROUP < 112 */
	pbx_devstate_changed_literal(newDeviceState, lineName);					/* callback via pbx callback and update subscribers */
#endif /* CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE && ASTERISK_VERSION_GROUP < 112 */
}

/* ========================================================================================================================= PBX Notify */
/*!
 * \brief helper function to parse aggegated hint_dialplan to search for a match with lineName
 *
 * \note: We need to be able to parse a hint like this:
 * exten => 112,hint, SIP/123&Meetme:444&SCCP/98011&SCCP/98031&Custom:DND112,CustomPresence:112,Meetme:444
 * and match on the lineName we are looking for, i.e.: SCCP/98011 and SCCP/98031
 */
static boolean_t sccp_match_dialplan2lineName(char *hint_app, char *lineName)
{
	char *rest = pbx_strdupa(hint_app);
        char *cur;
        char *tmp;

        // get the device portion of the hint string
        if ((tmp = strrchr(rest, ','))) {
                *tmp = '\0';
        }
        
        // check for match for aggregated entry
        while ((cur = strsep(&rest, "&"))) {
        	if (sccp_strcaseequals(cur, lineName)) {
			//sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (parse_hint_device) cur:%s matches lineName:%s\n", cur, lineName);
        		return TRUE;
        	}
        }
        return FALSE;
}

/*
 * \brief Notify Line Status Update either directly or via PBX(including distributed devstate)
 * \param lineState SCCP LineState
 * \nore:
 * - notifyLineStateUpdate
 *   -> number/name has changed, but state stayed the same -> notifySubscribers			(shortcut)
 *   -> state changed -> sccp_hint_notifySubscribersViaPbx -> PBX -> sccp_hint_distributed_devstate_cb -> notifySubscribers
 */
static void sccp_hint_notifyLineStateUpdate(struct sccp_hint_lineState *lineState)
{
	sccp_hint_list_t *hint = NULL;
	char lineName[StationMaxNameSize + 5];

	{
		AUTO_RELEASE(sccp_line_t, line , lineState->line ? sccp_line_retain(lineState->line) : NULL);
		if (line) {
			snprintf(lineName, sizeof(lineName), "SCCP/%s", lineState->line->name);
		} else {
			return;
		}
	}
	enum ast_device_state newDeviceState = sccp_hint_hint2DeviceState(lineState->state);
	enum ast_device_state oldDeviceState = AST_DEVICE_UNKNOWN;

	/* Local Update */
 	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (!sccp_strlen_zero(hint->hint_dialplan) && sccp_match_dialplan2lineName(hint->hint_dialplan, lineName)) {
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifyLineStateUpdate) matched lineName:%s to dialplan:%s\n", lineName, hint->hint_dialplan);

			hint->calltype = lineState->callInfo.calltype;
			if (hint->calltype == SKINNY_CALLTYPE_INBOUND) {
				iCallInfo.Setter(hint->callInfo, 
					SCCP_CALLINFO_CALLINGPARTY_NAME, lineState->callInfo.partyName,
					SCCP_CALLINFO_CALLINGPARTY_NUMBER, lineState->callInfo.partyNumber,
					SCCP_CALLINFO_KEY_SENTINEL);
			} else {
				iCallInfo.Setter(hint->callInfo, 
					SCCP_CALLINFO_CALLEDPARTY_NAME, lineState->callInfo.partyName,
					SCCP_CALLINFO_CALLEDPARTY_NUMBER, lineState->callInfo.partyNumber,
					SCCP_CALLINFO_KEY_SENTINEL);
			}
			oldDeviceState = sccp_hint_hint2DeviceState(hint->currentState);

			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifyLineStateUpdate) Notify asterisk to set state to sccp channelstate '%s' (%d) on line 'SCCP/%s'\n", sccp_channelstate2str(lineState->state), lineState->state, lineName);
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifyLineStateUpdate) => asterisk: '%s' (%d) => '%s' (%d) on line SCCP/%s\n", pbxsccp_devicestate2str(oldDeviceState), oldDeviceState, pbxsccp_devicestate2str(newDeviceState), newDeviceState, lineName);
			if (newDeviceState == oldDeviceState) {
				sccp_hint_notifySubscribers(hint);							/* shortcut to inform sccp subscribers about cid update changes only */
			}
		}
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);

	sccp_hint_notifySubscribersViaPbx(lineState, lineName, newDeviceState);						/* go through pbx to inform subscribers about both state and cid */
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifyLineStateUpdate) Notified asterisk to set state to sccp channelstate '%s' (%d) => asterisk: '%s' (%d) on channel SCCP/%s\n", sccp_channelstate2str(lineState->state), lineState->state, pbxsccp_devicestate2str(newDeviceState), newDeviceState, lineState->line->name);
}

/* ========================================================================================================================= Helper Functions */
#ifdef CS_DYNAMIC_SPEEDDIAL
/*
 * model information should be moved to sccp_dev_build_buttontemplate, or some other place
 */
static gcc_inline boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice)
{
#ifdef CS_DYNAMIC_SPEEDDIAL_CID
	if (positionOnDevice <= 8 && (device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7971 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7985 || device->skinny_type == SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR)) {
		return TRUE;
	}
	if (positionOnDevice <= 6 && (device->skinny_type == SKINNY_DEVICETYPE_CISCO7945 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7961 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7961GE || device->skinny_type == SKINNY_DEVICETYPE_CISCO7962 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7965)) {
		return TRUE;
	}
	if (positionOnDevice <= 2 && (device->skinny_type == SKINNY_DEVICETYPE_CISCO7911 ||
				      device->skinny_type == SKINNY_DEVICETYPE_CISCO7912 ||
				      device->skinny_type == SKINNY_DEVICETYPE_CISCO7931 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7935 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7936 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7937 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7941 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7941GE || device->skinny_type == SKINNY_DEVICETYPE_CISCO7942 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7945)) {
		return TRUE;
	}
#endif

	return FALSE;
}
#endif

static void sccp_hint_checkForDND(struct sccp_hint_lineState *lineState)
{
	sccp_linedevices_t *lineDevice = NULL;
	sccp_line_t *line = lineState->line;

	do {
		/* we have to check if all devices on this line are dnd=SCCP_DNDMODE_REJECT, otherwise do not propagate DND status */
		boolean_t allDevicesInDND = TRUE;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
			if (lineDevice->device && lineDevice->device->dndFeature.status != SCCP_DNDMODE_REJECT) {
				allDevicesInDND = FALSE;
				break;
			}
		}
		SCCP_LIST_UNLOCK(&line->devices);

		if (allDevicesInDND) {
			lineState->callInfo.calltype = SKINNY_CALLTYPE_INBOUND;
			lineState->state = SCCP_CHANNELSTATE_DND;
		}
	} while (0);

	if (lineState->state == SCCP_CHANNELSTATE_DND) {
		sccp_copy_string(lineState->callInfo.partyName, "DND", sizeof(lineState->callInfo.partyName));
		sccp_copy_string(lineState->callInfo.partyNumber, "DND", sizeof(lineState->callInfo.partyNumber));
	}
}

sccp_channelstate_t sccp_hint_getLinestate(const char *linename, const char *deviceId)
{
	struct sccp_hint_lineState *lineState = NULL;
	sccp_channelstate_t state = SCCP_CHANNELSTATE_CONGESTION;

	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (lineState->line && sccp_strcaseequals(lineState->line->name, linename)) {
                	sccp_log(DEBUGCAT_HINT)(VERBOSE_PREFIX_3 "%s (getLinestate) state:%s, party:%s/%s, calltype:%s\n", lineState->line->name, sccp_channelstate2str(lineState->state),
                	        lineState->callInfo.partyNumber,lineState->callInfo.partyName,
                	        (!SCCP_CHANNELSTATE_Idling(lineState->state) && lineState->callInfo.calltype) ? skinny_calltype2str(lineState->callInfo.calltype) : "INACTIVE");
                        state = lineState->state;
			break;
		}
	}
	SCCP_LIST_UNLOCK(&lineStates);
	return state;
}

/*!
 * \brief Show Hint LineStates
 * \param fd Fd as int   
 * \param total Total number of lines as int
 * \param s AMI Session 
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
#include <asterisk/cli.h>
int sccp_show_hint_lineStates(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;

#define CLI_AMI_TABLE_NAME HintLineStates
#define CLI_AMI_TABLE_PER_ENTRY_NAME HintLineState
#define CLI_AMI_TABLE_LIST_ITER_HEAD &lineStates
#define CLI_AMI_TABLE_LIST_ITER_TYPE struct sccp_hint_lineState
#define CLI_AMI_TABLE_LIST_ITER_VAR lineState
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_FIELDS 															\
 		CLI_AMI_TABLE_FIELD(LineName,		"-10.10",	s,	10,	lineState->line ? lineState->line->name : "")		\
 		CLI_AMI_TABLE_FIELD(State,		"-22.22",	s,	22,	sccp_channelstate2str(lineState->state))		\
 		CLI_AMI_TABLE_FIELD(CallInfoNumber,	"-15.15",	s,	15,	lineState->callInfo.partyNumber)			\
 		CLI_AMI_TABLE_FIELD(CallInfoName,	"-30.30",	s,	30,	lineState->callInfo.partyName)				\
 		CLI_AMI_TABLE_FIELD(Direction,		"-10.10",	s,	10,	(!SCCP_CHANNELSTATE_Idling(lineState->state) && lineState->callInfo.calltype) ? skinny_calltype2str(lineState->callInfo.calltype) : "INACTIVE")

#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

/*!
 * \brief Show Hint Subscriptions
 * \param fd Fd as int   
 * \param total Total number of lines as int
 * \param s AMI Session 
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_show_hint_subscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;
	

#define CLI_AMI_TABLE_NAME HintSubscriptions
#define CLI_AMI_TABLE_PER_ENTRY_NAME HintSubscription
#define CLI_AMI_TABLE_LIST_ITER_HEAD &sccp_hint_subscriptions
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_hint_list_t
#define CLI_AMI_TABLE_LIST_ITER_VAR subscription
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION														\
	{																	\
		char cidName[StationMaxNameSize];												\
		char cidNumber[StationMaxDirnumSize];												\
		if (subscription->calltype == SKINNY_CALLTYPE_INBOUND) {									\
			iCallInfo.Getter(subscription->callInfo, 										\
				SCCP_CALLINFO_CALLINGPARTY_NAME, &cidName, 									\
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cidNumber, 									\
				SCCP_CALLINFO_KEY_SENTINEL);											\
		} else {															\
			iCallInfo.Getter(subscription->callInfo, 											\
				SCCP_CALLINFO_CALLEDPARTY_NAME, &cidName, 									\
				SCCP_CALLINFO_CALLEDPARTY_NUMBER, &cidNumber, 									\
				SCCP_CALLINFO_KEY_SENTINEL);											\
		}
#define CLI_AMI_TABLE_AFTER_ITERATION 														\
	}
#define CLI_AMI_TABLE_FIELDS 															\
 		CLI_AMI_TABLE_FIELD(Exten,		"-10.10",	s,	10,	subscription->exten)					\
 		CLI_AMI_TABLE_FIELD(Context,		"-10.10",	s,	10,	subscription->context)					\
 		CLI_AMI_TABLE_FIELD(Hint,		"-15.15",	s,	15,	subscription->hint_dialplan)				\
 		CLI_AMI_TABLE_FIELD(State,		"-22.22",	s,	22,	sccp_channelstate2str(subscription->currentState))	\
 		CLI_AMI_TABLE_FIELD(CallInfoNumber,	"-15.15",	s,	15,	cidNumber)			\
 		CLI_AMI_TABLE_FIELD(CallInfoName,	"-30.30",	s,	30,	cidName)			\
 		CLI_AMI_TABLE_FIELD(Direction,		"-10.10",	s,	10,	(subscription->calltype && subscription->calltype != SKINNY_CALLTYPE_SENTINEL) ? skinny_calltype2str(subscription->calltype) : "") \
 		CLI_AMI_TABLE_FIELD(Subs,		"-4",		d,	4,	SCCP_LIST_GETSIZE(&subscription->subscribers))

#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
