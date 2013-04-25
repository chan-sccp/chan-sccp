
/*!
 * \file        sccp_hint.c
 * \brief       SCCP Hint Class
 * \author      Marcello Ceschia < marcello.ceschia@users.sourceforge.net >
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note                For more information about how does hint update works, see \ref hint_update
 * \since       2009-01-16
 * \remarks     Purpose:        SCCP Hint
 *              When to use:    Does the business of hint status
 *
 * $Date: 2011-01-04 17:29:12 +0100 (Tue, 04 Jan 2011) $
 * $Revision: 2215 $
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

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2215 $")

    /* ========================================================================================================================= Struct Definitions */
    /*!
     *\brief SCCP Hint Subscribing Device Structure
     */
typedef struct sccp_hint_SubscribingDevice sccp_hint_SubscribingDevice_t;
typedef struct sccp_hint_list sccp_hint_list_t;

struct sccp_hint_SubscribingDevice {

	const sccp_device_t *device;										/*!< SCCP Device */
	uint8_t instance;											/*!< Instance */
	uint8_t positionOnDevice;										/*!< Instance */

	SCCP_LIST_ENTRY (sccp_hint_SubscribingDevice_t) list;							/*!< Hint Subscribing Device Linked List Entry */
};														/*!< SCCP Hint Subscribing Device Structure */

/*!
 *\brief SCCP Hint Line State Structure
 */
struct sccp_hint_lineState {
	sccp_line_t *line;
	sccp_channelState_t state;

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
	ast_mutex_t lock;											/*!< Asterisk Lock */

	char exten[SCCP_MAX_EXTENSION];										/*!< Extension for Hint */
	char context[SCCP_MAX_CONTEXT];										/*!< Context for Hint */
	char hint_dialplan[256];										/*!< e.g. IAX2/station123 */

	sccp_channelState_t currentState;									/*!< current State */
	sccp_channelState_t previousState;									/*!< current State */

	/*!
	 * \brief Call Information Structure
	 */
	struct {
		char partyNumber[StationMaxNameSize];								/*!< Calling Party Name */
		char partyName[StationMaxNameSize];								/*!< Called Party Name */
		skinny_calltype_t calltype;									/*!< Skinny Call Type */
	} callInfo;												/*!< Call Information Structure */

	int stateid;												/*!< subscription id in asterisk */
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	struct pbx_event_sub *device_state_sub;									/*!< asterisk distributed device state subscription */
#endif

	SCCP_LIST_HEAD (, sccp_hint_SubscribingDevice_t) subscribers;						/*!< Hint Type Subscribers Linked List Entry */
	SCCP_LIST_ENTRY (sccp_hint_list_t) list;								/*!< Hint Type Linked List Entry */
};														/*!< SCCP Hint List Structure */

/* ========================================================================================================================= Declarations */
static void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForSharedLine(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForSingleLine(struct sccp_hint_lineState *lineState);
static void sccp_hint_checkForDND(struct sccp_hint_lineState *lineState);
static void sccp_hint_notifyPBX(struct sccp_hint_lineState *linestate);
static sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context);
static void sccp_hint_notifySubscribers(sccp_hint_list_t * hint);
static void sccp_hint_deviceRegistered(const sccp_device_t * device);
static void sccp_hint_deviceUnRegistered(const char *deviceName);
static void sccp_hint_addSubscription4Device(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice);
static void sccp_hint_lineStatusChanged(sccp_line_t * line, sccp_device_t * device);
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event);
static void sccp_hint_eventListener(const sccp_event_t * event);
static inline boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice);

#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
static void sccp_hint_distributed_devstate_cb(const pbx_event_t * event, void *data)
{

	sccp_hint_list_t *hint;
	const char *cidName;
	const char *cidNumber;

	hint = (sccp_hint_list_t *) data;

	cidName = pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNAME);
	cidNumber = pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNUM);

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Got new hint event %s, cidname: %s, cidnum: %s\n", hint->hint_dialplan, cidName ? cidName : "NULL", cidNumber ? cidNumber : "NULL");

	if (cidName) {
		sccp_copy_string(hint->callInfo.partyName, cidName, sizeof(hint->callInfo.partyName));
	}

	if (cidNumber) {
		sccp_copy_string(hint->callInfo.partyNumber, cidNumber, sizeof(hint->callInfo.partyNumber));
	}

	return;
}
#endif

#if ASTERISK_VERSION_GROUP >= 112
int sccp_hint_devstate_cb(char *context, char *id, struct ast_state_cb_info *info, void *data);
#elif ASTERISK_VERSION_GROUP >= 110
int sccp_hint_devstate_cb(const char *context, const char *id, enum ast_extension_states state, void *data);
#else
int sccp_hint_devstate_cb(char *context, char *id, enum ast_extension_states state, void *data);
#endif
/* ========================================================================================================================= List Declarations */
SCCP_LIST_HEAD (, struct sccp_hint_lineState) lineStates;
SCCP_LIST_HEAD (, sccp_hint_list_t) sccp_hint_subscriptions;

/* ========================================================================================================================= Module Start/Stop */
/*!
 * \brief starting hint-module
 */
void sccp_hint_module_start()
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Starting hint system\n");
	SCCP_LIST_HEAD_INIT(&lineStates);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_FEATURE_CHANGED, sccp_hint_eventListener, TRUE);
}

/*!
 * \brief stop hint-module
 *
 * \lock
 *      - lineStates
 *      - sccp_hint_subscriptions
 *        - hint->subscribers
 */
void sccp_hint_module_stop()
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Stopping hint system\n");
	{
		struct sccp_hint_lineState *lineState;

		SCCP_LIST_LOCK(&lineStates);
		while ((lineState = SCCP_LIST_REMOVE_HEAD(&lineStates, list))) {
			lineState->line = lineState->line ? sccp_line_release(lineState->line) : NULL;
			sccp_free(lineState);
		}
		SCCP_LIST_UNLOCK(&lineStates);
	}

	{
		sccp_hint_list_t *hint;
		sccp_device_t *device;
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
				if ((device = sccp_device_retain((sccp_device_t *) subscriber->device))) {
					subscriber->device = sccp_device_release(subscriber->device);
					device = sccp_device_release(device);
					sccp_free(subscriber);
				}
			}
			SCCP_LIST_UNLOCK(&hint->subscribers);
			sccp_free(hint);
		}
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED, sccp_hint_eventListener);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_hint_handleFeatureChangeEvent);
}

/* ========================================================================================================================= PBX Callbacks */
/*!
 * \brief asterisk callback for extension state changes (we subscribed with ast_extension_state_add)
 */
#if ASTERISK_VERSION_GROUP >= 112
/*!
 * \param context extension context (char *)
 * \param id extension (char *)
 * \param info ast_state_cb_info
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
int sccp_hint_devstate_cb(char *context, char *id, struct ast_state_cb_info *info, void *data)
#elif ASTERISK_VERSION_GROUP >= 110
/*!
 * \param context extension context (const char *)
 * \param id extension (const char *)
 * \param state ast_extension_state (enum)
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
int sccp_hint_devstate_cb(const char *context, const char *id, enum ast_extension_states state, void *data)
#else
/*!
 * \param context extension context (char *)
 * \param id extension (char *)
 * \param state ast_extension_state (enum)
 * \param data private channel data (sccp_hint_list_t *hint) as void pointer
 */
int sccp_hint_devstate_cb(char *context, char *id, enum ast_extension_states state, void *data)
#endif
{
	sccp_hint_list_t *hint;
	int extensionState;
	char hintStr[AST_MAX_EXTENSION];
	const char *cidName;

	//      const char *cidNumber;

	hint = (sccp_hint_list_t *) data;
	ast_get_hint(hintStr, sizeof(hintStr), NULL, 0, NULL, hint->context, hint->exten);

#if ASTERISK_VERSION_GROUP >= 112
	extensionState = info->exten_state;
#else
	extensionState = state;
#endif

	cidName = hint->callInfo.partyName;
	//      cidNumber       = hint->callInfo.partyNumber;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "%s: (sccp_hint_devstate_cb) Got new hint event %s, state: %d (%s), cidname: %s, cidnum: %s\n", hint->exten, hint->hint_dialplan, extensionState, ast_extension_state2str(extensionState), hint->callInfo.partyName, hint->callInfo.partyNumber);
	switch (extensionState) {
		case AST_EXTENSION_REMOVED:
		case AST_EXTENSION_DEACTIVATED:
		case AST_EXTENSION_UNAVAILABLE:
			hint->currentState = SCCP_CHANNELSTATE_DOWN;
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
			if (cidName && !strcasecmp(cidName, "DND")) {
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
 * \lock
 *      - device
 *        - see sccp_hint_deviceRegistered()
 *        - see sccp_hint_deviceUnRegistered()
 */
void sccp_hint_eventListener(const sccp_event_t * event)
{
	sccp_device_t *device;

	if (!event)
		return;

	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->event.deviceRegistered.device;

			sccp_hint_deviceRegistered(device);
			break;
		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->event.deviceRegistered.device;

			if (device && device->id) {
				char *deviceName = strdupa(device->id);
				sccp_hint_deviceUnRegistered(deviceName);
			}

			break;
		case SCCP_EVENT_DEVICE_DETACHED:
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "%s: (sccp_hint_eventListener) device %s detached on line %s\n", DEV_ID_LOG(event->event.deviceAttached.linedevice->device), event->event.deviceAttached.linedevice->device->id, event->event.deviceAttached.linedevice->line->name);
			sccp_hint_lineStatusChanged(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device);
			break;
		case SCCP_EVENT_DEVICE_ATTACHED:
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "%s: (sccp_hint_eventListener) device %s attached on line %s\n", DEV_ID_LOG(event->event.deviceAttached.linedevice->device), event->event.deviceAttached.linedevice->device->id, event->event.deviceAttached.linedevice->line->name);
			sccp_hint_lineStatusChanged(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device);
			break;
		case SCCP_EVENT_LINESTATUS_CHANGED:
			sccp_hint_lineStatusChanged(event->event.lineStatusChanged.line, event->event.lineStatusChanged.device);
			break;
		case SCCP_EVENT_FEATURE_CHANGED:
			sccp_hint_handleFeatureChangeEvent(event);
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
 * \note        device locked by parent
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 */
static void sccp_hint_deviceRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	uint8_t positionOnDevice = 0;
	sccp_device_t *d = NULL;

	if ((d = sccp_device_retain((sccp_device_t *) device))) {
		SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
			positionOnDevice++;

			if (config->type == SPEEDDIAL) {
				if (sccp_strlen_zero(config->button.speeddial.hint)) {
					continue;
				}
				sccp_hint_addSubscription4Device(device, config->button.speeddial.hint, config->instance, positionOnDevice);
			}
		}
		sccp_device_release(d);
	}
}

/*!
 * \brief Handle Hints for Device UnRegister
 * \param deviceName Device as Char
 *
 * \note        device locked by parent
 *
 * \lock
 *      - device->buttonconfig
 *        - see sccp_hint_unSubscribeHint()
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
				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "%s: Freeing subscriber from hint exten: %s in %s\n", deviceName, hint->exten, hint->context);
				SCCP_LIST_REMOVE_CURRENT(list);
				subscriber->device = sccp_device_release(subscriber->device);
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
 *      - sccp_hint_subscriptions is not always locked
 * 
 * \lock
 *      - sccp_hint_subscriptions
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
	if (hint_exten)
		pbx_strip(hint_exten);

	hint_context = splitter;
	if (hint_context) {
		pbx_strip(hint_context);
	} else {
		hint_context = GLOB(context);
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "%s: (sccp_hint_addSubscription4Device) Dialplan %s for exten: %s and context: %s\n", DEV_ID_LOG(device), hintStr, hint_exten, hint_context);

	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (sccp_strlen(hint_exten) == sccp_strlen(hint->exten)
		    && sccp_strlen(hint_context) == sccp_strlen(hint->context)
		    && sccp_strequals(hint_exten, hint->exten)
		    && sccp_strequals(hint_context, hint->context)) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_addSubscription4Device) Hint found for exten '%s@%s'\n", DEV_ID_LOG(device), hint_exten, hint_context);
			break;
		}
	}

	/* we have no hint */
	if (!hint) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_addSubscription4Device) create new hint for %s@%s\n", DEV_ID_LOG(device), hint_exten, hint_context);
		hint = sccp_hint_create(hint_exten, hint_context);
		if (!hint) {
			pbx_log(LOG_ERROR, "%s: (sccp_hint_addSubscription4Device) hint create failed for %s@%s\n", DEV_ID_LOG(device), hint_exten, hint_context);
			return;
		}

		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_hint_subscriptions, hint, list);
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	/* add subscribing device */
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_addSubscription4Device) create subscriber or hint: %s in %s\n", DEV_ID_LOG(device), hint->exten, hint->context);
	sccp_hint_SubscribingDevice_t *subscriber;

	subscriber = sccp_malloc(sizeof(sccp_hint_SubscribingDevice_t));
	if (!subscriber) {
		pbx_log(LOG_ERROR, "%s: (sccp_hint_addSubscription4Device) Memory Allocation Error while creating subscriber object\n", DEV_ID_LOG(device));
		return;
	}
	memset(subscriber, 0, sizeof(sccp_hint_SubscribingDevice_t));

	subscriber->device = sccp_device_retain((sccp_device_t *) device);
	subscriber->instance = instance;
	subscriber->positionOnDevice = positionOnDevice;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_addSubscription4Device) Adding subscription for hint %s@%s\n", DEV_ID_LOG(device), hint->exten, hint->context);
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

	if (sccp_strlen_zero(hint_exten))
		return NULL;

	if (sccp_strlen_zero(hint_context))
		hint_context = GLOB(context);

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_create) Create hint for exten: %s context: %s\n", hint_exten, hint_context);

	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
	// CS_AST_HAS_NEW_HINT

	if (sccp_strlen_zero(hint_dialplan)) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_create) No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
		return NULL;
	}

	hint = sccp_malloc(sizeof(sccp_hint_list_t));
	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_create) Memory Allocation Error while creating hint list for hint: %s@%s\n", hint_exten, hint_context);
		return NULL;
	}
	memset(hint, 0, sizeof(sccp_hint_list_t));

	SCCP_LIST_HEAD_INIT(&hint->subscribers);
	sccp_mutex_init(&hint->lock);

	sccp_copy_string(hint->exten, hint_exten, sizeof(hint->exten));
	sccp_copy_string(hint->context, hint_context, sizeof(hint->context));
	sccp_copy_string(hint->hint_dialplan, hint_dialplan, sizeof(hint_dialplan));

	hint->stateid = pbx_extension_state_add(hint->context, hint->exten, sccp_hint_devstate_cb, hint);

#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	hint->device_state_sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_hint_distributed_devstate_cb, "sccp_hint_distributed_devstate_cb", hint, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan, AST_EVENT_IE_END);
#endif

#if ASTERISK_VERSION_GROUP >= 112
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
/*!
 * \brief Handle line status change
 * \param line          SCCP Line that was changed
 * \param device        SCCP Device who initialied the change
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

	/** create new state holder for line */
	if (!lineState) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "%s: (sccp_hint_lineStatusChanged) Create new hint_lineState for line: %s\n", DEV_ID_LOG(device), line->name);
		lineState = sccp_malloc(sizeof(struct sccp_hint_lineState));
		if (!lineState) {
			pbx_log(LOG_ERROR, "%s: (sccp_hint_lineStatusChanged) Memory Allocation Error while creating hint-lineState object for line %s\n", DEV_ID_LOG(device), line->name);
			return;
		}
		memset(lineState, 0, sizeof(struct sccp_hint_lineState));
		lineState->line = sccp_line_retain(line);

		SCCP_LIST_INSERT_HEAD(&lineStates, lineState, list);
	}
	SCCP_LIST_UNLOCK(&lineStates);

	if (lineState && lineState->line) {

		/** update line state */
		sccp_hint_updateLineState(lineState);
	}
}

/*!
 * \brief Handle Hint Status Update
 * \param lineState SCCP LineState
 */
void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState)
{
	sccp_line_t *line = NULL;

	if ((line = sccp_line_retain(lineState->line))) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineState) Update Line Channel State: %s(%d)\n", line->name, channelstate2str(lineState->state), lineState->state);
		if (line->channels.size > 1) {
			/* line is currently shared between multiple device and has multiple concurrent calls active */
			sccp_hint_updateLineStateForSharedLine(lineState);
		} else {
			/* just one device per line */
			sccp_hint_updateLineStateForSingleLine(lineState);
		}

		/* push chagnes to pbx */
		sccp_hint_notifyPBX(lineState);

		line = sccp_line_release(line);
	}
}

/*!
 * \brief set hint status for a line with more then one channel
 * \param lineState SCCP LineState
 */
void sccp_hint_updateLineStateForSharedLine(struct sccp_hint_lineState *lineState)
{
	sccp_line_t *line = lineState->line;
	sccp_channel_t *channel = NULL;

	memset(lineState->callInfo.partyName, 0, sizeof(lineState->callInfo.partyName));
	memset(lineState->callInfo.partyNumber, 0, sizeof(lineState->callInfo.partyNumber));

	/* no line, or line without devices */
	if (!line || (line && 0 == line->devices.size)) {
		lineState->state = SCCP_CHANNELSTATE_CONGESTION;
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_updateLineStateForSharedLine) no line or registered on 0 devices; linename: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	if (line->channels.size > 0) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSharedLine) number of active channels %d\n", line->name, line->channels.size);
		if (line->channels.size == 1) {
			SCCP_LIST_LOCK(&line->channels);
			channel = SCCP_LIST_FIRST(&line->channels);
			SCCP_LIST_UNLOCK(&line->channels);
			if (channel && (channel = sccp_channel_retain(channel))) {
				if (channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN) {
					lineState->state = channel->state;

					/* set cid name/numbe information according to the call direction */
					if (SKINNY_CALLTYPE_INBOUND == channel->calltype) {
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyNumber));
					} else {
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.calledPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.calledPartyNumber, sizeof(lineState->callInfo.partyNumber));
					}
				} else {
					lineState->state = SCCP_CHANNELSTATE_ONHOOK;
				}
				channel = sccp_channel_release(channel);
			} else {
				lineState->state = SCCP_CHANNELSTATE_ONHOOK;
			}
		} else if (line->channels.size > 1) {

			/** we have multiple channels, so do not set cid information */
			//                      sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyName));
			//                      sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyNumber));
			lineState->state = SCCP_CHANNELSTATE_CONNECTED;
		}
	} else {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSharedLine) no active channels\n", line->name);
		if (0 == line->devices.size) {

			/** the line does not have a device attached, mark them as unavailable (congestion) */
			//                      sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_TEMP_FAIL, sizeof(lineState->callInfo.partyName));
			//                      sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_TEMP_FAIL, sizeof(lineState->callInfo.partyNumber));

			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSharedLine) number of devices that have this line registered is 0\n", line->name);
			lineState->state = SCCP_CHANNELSTATE_ZOMBIE;						// CS_DYNAMIC_SPEEDDIAL
		} else {
			lineState->state = SCCP_CHANNELSTATE_ONHOOK;
		}
	}
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSharedLine) Set sharedLineState to %s(%d)\n", line->name, channelstate2str(lineState->state), lineState->state);
}

/*!
 * \brief set hint status for a line with less or eq one channel
 * \param lineState SCCP LineState
 * 
 * \lock
 *      - hint
 *        - see sccp_line_find_byname()
 */
void sccp_hint_updateLineStateForSingleLine(struct sccp_hint_lineState *lineState)
{
	sccp_line_t *line = lineState->line;
	sccp_channel_t *channel = NULL;
	sccp_device_t *device = NULL;
	sccp_linedevices_t *lineDevice = NULL;
	uint8_t state;

	//      boolean_t dev_privacy = FALSE;

	/** clear cid information */
	memset(lineState->callInfo.partyName, 0, sizeof(lineState->callInfo.partyName));
	memset(lineState->callInfo.partyNumber, 0, sizeof(lineState->callInfo.partyNumber));

	/* no line, or line without devices */
	if (!line || (line && 0 == line->devices.size)) {
		lineState->state = SCCP_CHANNELSTATE_CONGESTION;

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_updateLineStateForSingleLine) no line or register on 0 devices; linename: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	SCCP_LIST_LOCK(&line->channels);
	channel = SCCP_LIST_FIRST(&line->channels);
	SCCP_LIST_UNLOCK(&line->channels);
	if (channel && (channel = sccp_channel_retain(channel))) {
		lineState->callInfo.calltype = channel->calltype;
		state = channel->state;

		SCCP_LIST_LOCK(&line->devices);
		lineDevice = SCCP_LIST_FIRST(&line->devices);
		SCCP_LIST_UNLOCK(&line->devices);

		if ((lineDevice = sccp_linedevice_retain(lineDevice))) {
			if ((device = sccp_device_retain(lineDevice->device))) {
				if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
					state = SCCP_CHANNELSTATE_DND;
				}
				//                              dev_privacy = device->privacyFeature.enabled;
				device = device ? sccp_device_release(device) : NULL;
			}
			lineDevice = lineDevice ? sccp_linedevice_release(lineDevice) : NULL;
		}

		switch (state) {
			case SCCP_CHANNELSTATE_DOWN:
				state = SCCP_CHANNELSTATE_ONHOOK;
				break;
			case SCCP_CHANNELSTATE_SPEEDDIAL:
				break;
			case SCCP_CHANNELSTATE_ONHOOK:
				break;
			case SCCP_CHANNELSTATE_DND:
				sccp_copy_string(lineState->callInfo.partyName, "DND", sizeof(lineState->callInfo.partyName));
				sccp_copy_string(lineState->callInfo.partyNumber, "DND", sizeof(lineState->callInfo.partyNumber));
				break;
			case SCCP_CHANNELSTATE_OFFHOOK:
			case SCCP_CHANNELSTATE_GETDIGITS:
			case SCCP_CHANNELSTATE_RINGOUT:
			case SCCP_CHANNELSTATE_CONNECTED:
			case SCCP_CHANNELSTATE_PROCEED:
			case SCCP_CHANNELSTATE_RINGING:
			case SCCP_CHANNELSTATE_DIALING:
			case SCCP_CHANNELSTATE_DIGITSFOLL:
			case SCCP_CHANNELSTATE_BUSY:
			case SCCP_CHANNELSTATE_HOLD:
			case SCCP_CHANNELSTATE_CONGESTION:
			case SCCP_CHANNELSTATE_CALLWAITING:
			case SCCP_CHANNELSTATE_CALLTRANSFER:
			case SCCP_CHANNELSTATE_CALLCONFERENCE:
			case SCCP_CHANNELSTATE_CALLPARK:
			case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			case SCCP_CHANNELSTATE_INVALIDNUMBER:
				//                              if (dev_privacy == 0 || (dev_privacy == 1 && channel->privacy == FALSE)) {

				/** set cid name/number information according to the call direction */
				switch (channel->calltype) {
					case SKINNY_CALLTYPE_INBOUND:
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.callingPartyNumber, sizeof(lineState->callInfo.partyNumber));
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: set speeddial partyName: '%s' (callingParty)\n", line->name, channel->callInfo.callingPartyName);
						break;
					case SKINNY_CALLTYPE_OUTBOUND:
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.calledPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.calledPartyNumber, sizeof(lineState->callInfo.partyNumber));
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: set speeddial partyName: '%s' (calledParty)\n", line->name, channel->callInfo.calledPartyName);
						break;
					case SKINNY_CALLTYPE_FORWARD:
						sccp_copy_string(lineState->callInfo.partyName, "cfwd", sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, "cfwd", sizeof(lineState->callInfo.partyNumber));
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: set speedial partyName: cfwd\n", line->name);
						break;
				}
				//                              }
				break;
		}

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSingleLine) partyName: %s, partyNumber: %s\n", line->name, lineState->callInfo.partyName, lineState->callInfo.partyNumber);

		lineState->state = state;
		channel = sccp_channel_release(channel);
	} else {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSingleLine) NO CHANNEL\n", line->name);
		lineState->state = SCCP_CHANNELSTATE_ONHOOK;
		sccp_hint_checkForDND(lineState);
	}													// if(channel)
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_updateLineStateForSingleLine) Set singleLineState to %s(%d)\n", line->name, channelstate2str(lineState->state), lineState->state);
}

/* ========================================================================================================================= Event Handlers : Feature Change */
/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 */
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event)
{
	sccp_buttonconfig_t *buttonconfig = NULL;
	sccp_device_t *d = NULL;
	sccp_line_t *line = NULL;

	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_DND:
			if ((d = sccp_device_retain(event->event.featureChanged.device))) {
				SCCP_LIST_LOCK(&d->buttonconfig);
				SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
					if (buttonconfig->type == LINE) {
						line = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);
						if (line) {
							sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_hint_handleFeatureChangeEvent) Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dndFeature.status ? "on" : "off", line->name);
							sccp_hint_lineStatusChanged(line, d);
							line = sccp_line_release(line);
						}
					}
				}
				SCCP_LIST_UNLOCK(&d->buttonconfig);
				sccp_device_release(d);
			}
			break;
		default:
			break;
	}
}

/* ========================================================================================================================= PBX Notify */
/*!
 * \brief Notify Asterisk of Hint State Change
 * \param lineState SCCP LineState
 */
void sccp_hint_notifyPBX(struct sccp_hint_lineState *lineState)
{
	char channelName[100];
	sccp_hint_list_t *hint = NULL;

	sprintf(channelName, "SCCP/%s", lineState->line->name);
	enum ast_device_state newDeviceState = AST_DEVICE_UNKNOWN;

#ifndef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (!strncasecmp(channelName, hint->hint_dialplan, sizeof(channelName))) {
			sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifyPBX) %s <==> %s \n", channelName, hint->hint_dialplan);
			sccp_copy_string(hint->callInfo.partyName, lineState->callInfo.partyName, sizeof(hint->callInfo.partyName));
			sccp_copy_string(hint->callInfo.partyNumber, lineState->callInfo.partyNumber, sizeof(hint->callInfo.partyNumber));
			break;
		}
	}
#endif

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifyPBX) Notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", channelstate2str(lineState->state), lineState->state, pbxdevicestate2str(sccp_channelState2AstDeviceState(lineState->state)), sccp_channelState2AstDeviceState(lineState->state), lineState->line->name);

	switch (lineState->state) {
		case SCCP_CHANNELSTATE_DOWN:
		case SCCP_CHANNELSTATE_ONHOOK:
			newDeviceState = AST_DEVICE_NOT_INUSE;
			break;
		case SCCP_CHANNELSTATE_RINGING:
			newDeviceState = AST_DEVICE_RINGING;
			break;
		case SCCP_CHANNELSTATE_HOLD:
			newDeviceState = AST_DEVICE_ONHOLD;
			break;
		case SCCP_CHANNELSTATE_BUSY:
			newDeviceState = AST_DEVICE_BUSY;
			break;
		case SCCP_CHANNELSTATE_DND:
			newDeviceState = AST_DEVICE_BUSY;
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
		case SCCP_CHANNELSTATE_SPEEDDIAL:
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE:
		case SCCP_CHANNELSTATE_ZOMBIE:
			newDeviceState = AST_DEVICE_UNAVAILABLE;
			break;
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
		case SCCP_CHANNELSTATE_PROCEED:
		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
		case SCCP_CHANNELSTATE_OFFHOOK:
		case SCCP_CHANNELSTATE_GETDIGITS:
		case SCCP_CHANNELSTATE_CONNECTED:
		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
		case SCCP_CHANNELSTATE_PROGRESS:
		case SCCP_CHANNELSTATE_BLINDTRANSFER:
		case SCCP_CHANNELSTATE_CALLWAITING:
		case SCCP_CHANNELSTATE_CALLTRANSFER:
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
		case SCCP_CHANNELSTATE_CALLPARK:
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			newDeviceState = AST_DEVICE_INUSE;
			break;
	}

	// if pbx devicestate does not change, no need to inform asterisk */
	if (hint && lineState->state == hint->currentState) {
		sccp_hint_notifySubscribers(hint);								/* shortcut to inform sccp subscribers */
	} else {
#ifdef CS_USE_ASTERISK_DISTRIBUTED_DEVSTATE
		pbx_event_t *event;

		event = pbx_event_new(
					     AST_EVENT_DEVICE_STATE, 
					     AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, channelName, 
					     AST_EVENT_IE_STATE, AST_EVENT_IE_PLTYPE_UINT, newDeviceState, 
					     AST_EVENT_IE_CEL_CIDNAME, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyName, 
					     AST_EVENT_IE_CEL_CIDNUM, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyNumber, 
					     AST_EVENT_IE_CEL_USERFIELD, AST_EVENT_IE_PLTYPE_UINT, lineState->callInfo.calltype, 
					     AST_EVENT_IE_END
				     );
		pbx_event_queue_and_cache(event);
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: \n");
		sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_notifyPBX!distributed) Notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", channelstate2str(lineState->state), lineState->state, pbxdevicestate2str(newDeviceState), newDeviceState, lineState->line->name);
		pbx_devstate_changed_literal(newDeviceState, channelName);					/* come back via pbx callback and update subscribers */
#else
		pbx_devstate_changed_literal(newDeviceState, channelName);					/* come back via pbx callback and update subscribers */
#endif

	}
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
	sccp_device_t *d;
	sccp_hint_SubscribingDevice_t *subscriber = NULL;
	sccp_moo_t *r;
	uint32_t state;												/* used to fall back to old behavior */

#ifdef CS_DYNAMIC_SPEEDDIAL
	sccp_speed_t k;
#endif

#ifdef CS_DYNAMIC_SPEEDDIAL
	char displayMessage[80];
#endif

	sccp_channel_t tmpChannel;

	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_notifySubscribers) no hint provided to notifySubscribers about\n");
		return;
	}

	if (!&GLOB(module_running) || SCCP_REF_RUNNING != sccp_refcount_isRunning()) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "%s: (sccp_hint_notifySubscribers) Skip processing hint while we are shutting down.\n", hint->exten);
		return;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "%s: (sccp_hint_notifySubscribers) notify %u subscribers of %s's state %s\n", hint->exten, SCCP_LIST_GETSIZE(hint->subscribers), (hint->hint_dialplan) ? hint->hint_dialplan : "null", channelstate2str(hint->currentState));

	/* use a temporary channel as fallback for non dynamic speeddial devices */
	memset(&tmpChannel, 0, sizeof(sccp_channel_t));
	sccp_copy_string(tmpChannel.callInfo.callingPartyName, hint->callInfo.partyName, sizeof(tmpChannel.callInfo.callingPartyName));
	sccp_copy_string(tmpChannel.callInfo.calledPartyName, hint->callInfo.partyName, sizeof(tmpChannel.callInfo.calledPartyName));
	sccp_copy_string(tmpChannel.callInfo.callingPartyNumber, hint->callInfo.partyNumber, sizeof(tmpChannel.callInfo.callingPartyNumber));
	sccp_copy_string(tmpChannel.callInfo.calledPartyNumber, hint->callInfo.partyNumber, sizeof(tmpChannel.callInfo.calledPartyNumber));
	tmpChannel.calltype = hint->callInfo.calltype;
	/* done */

	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE(&hint->subscribers, subscriber, list) {
		if ((d = sccp_device_retain((sccp_device_t *) subscriber->device))) {
			state = hint->currentState;
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) notify subscriber %s of %s's state %s\n", DEV_ID_LOG(d), d->id, (hint->hint_dialplan) ? hint->hint_dialplan : "null", channelstate2str(state));

#ifdef CS_DYNAMIC_SPEEDDIAL
			if (d->inuseprotocolversion >= 15) {
				sccp_dev_speed_find_byindex((sccp_device_t *) d, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL, &k);

				REQ(r, FeatureStatDynamicMessage);
				if (r) {
					r->msg.FeatureStatDynamicMessage.lel_instance = htolel(subscriber->instance);
					r->msg.FeatureStatDynamicMessage.lel_type = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);

					switch (hint->currentState) {
						case SCCP_CHANNELSTATE_ONHOOK:
							snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_IDLE);
							break;

						case SCCP_CHANNELSTATE_DOWN:
							snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_UNKNOWN);	/* default state */
							break;

						case SCCP_CHANNELSTATE_RINGING:
							if (sccp_hint_isCIDavailabe(d, subscriber->positionOnDevice) == TRUE) {
								if (strlen(hint->callInfo.partyName) > 0) {
									snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->", k.name);
								} else if (strlen(hint->callInfo.partyNumber) > 0) {
									snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyNumber, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->", k.name);
								} else {
									snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
								}
							} else {
								snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
							}
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_ALERTING);	/* ringin */
							break;

						case SCCP_CHANNELSTATE_DND:
							snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_DND);	/* dnd */
							break;

						case SCCP_CHANNELSTATE_CONGESTION:
							snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_UNKNOWN);	/* device/line not found */
							break;

						default:
							if (sccp_hint_isCIDavailabe(d, subscriber->positionOnDevice) == TRUE) {
								if (strlen(hint->callInfo.partyName) > 0) {
									snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "<->", k.name);
								} else if (strlen(hint->callInfo.partyNumber) > 0) {
									snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyNumber, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "<->", k.name);
								} else {
									snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
								}
							} else {
								snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
							}
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_INUSE);	/* connected */
							break;
					}
					sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) set display name to: \"%s\"\n", DEV_ID_LOG(d), displayMessage);
					sccp_copy_string(r->msg.FeatureStatDynamicMessage.DisplayName, displayMessage, sizeof(r->msg.FeatureStatDynamicMessage.DisplayName));
					sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) notify device: %s@%d state: %d(%d)\n", DEV_ID_LOG(d), DEV_ID_LOG(d), subscriber->instance, hint->currentState, r->msg.FeatureStatDynamicMessage.lel_status);
					sccp_dev_send(d, r);
				} else {
					sccp_free(r);
				}
			} else
#endif
			{
				/*
				   we have dynamic speeddial enabled, but subscriber can not handle this.
				   We have to switch back to old hint style and send old state.
				 */
				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) can not handle dynamic speeddial, fall back to old behavior using state %d\n", DEV_ID_LOG(d), state);

				/*
				   With the old hint style we should only use SCCP_CHANNELSTATE_ONHOOK and SCCP_CHANNELSTATE_CALLREMOTEMULTILINE as callstate,
				   otherwise we get a callplane on device -> set all states except onhook to SCCP_CHANNELSTATE_CALLREMOTEMULTILINE -MC
				 */

				switch (hint->currentState) {
					case SCCP_CHANNELSTATE_ONHOOK:
						state = SCCP_CHANNELSTATE_ONHOOK;
						break;
					case SCCP_CHANNELSTATE_RINGING:
						if (d->allowRinginNotification) {
							state = SCCP_CHANNELSTATE_RINGING;
						} else {
							state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
						}
						break;
					default:
						state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
						break;
				}

				if (SCCP_CHANNELSTATE_RINGING == hint->previousState) {
					/* we send a congestion to the phone, so call will not be marked as missed call */
					sccp_device_sendcallstate(d, subscriber->instance, 0, SCCP_CHANNELSTATE_CONGESTION, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
				}

				sccp_device_sendcallstate(d, subscriber->instance, 0, state, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT); /** do not set visibility to COLLAPSED, this will hidde callInfo in state CALLREMOTEMULTILINE */
				d->protocol->sendCallInfo(d, &tmpChannel, subscriber->instance);

				if (state == SCCP_CHANNELSTATE_ONHOOK) {
					sccp_dev_set_keyset(d, subscriber->instance, 0, KEYMODE_ONHOOK);
				} else {
					sccp_dev_set_keyset(d, subscriber->instance, 0, KEYMODE_INUSEHINT);
				}

			}
			d = sccp_device_release(d);
		} else {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifySubscribers) device not found/retained\n");
		}
	}
	SCCP_LIST_UNLOCK(&hint->subscribers);
}

/* ========================================================================================================================= Helper Functions */
#ifdef CS_DYNAMIC_SPEEDDIAL
/*
 * model information should be moved to sccp_dev_build_buttontemplate, or some other place
 */
static inline boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice)
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
	sccp_linedevices_t *lineDevice;
	sccp_line_t *line = lineState->line;

	if (line->devices.size > 1) {
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
			lineState->state = SCCP_CHANNELSTATE_DND;
		}
	} else {
		SCCP_LIST_LOCK(&line->devices);
		sccp_linedevices_t *lineDevice = SCCP_LIST_FIRST(&line->devices);

		if (lineDevice && lineDevice->device) {
			if (lineDevice->device->dndFeature.enabled && lineDevice->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				lineState->state = SCCP_CHANNELSTATE_DND;
			}
		}
		SCCP_LIST_UNLOCK(&line->devices);
	}

	if (lineState->state == SCCP_CHANNELSTATE_DND) {
		sccp_copy_string(lineState->callInfo.partyName, "DND", sizeof(lineState->callInfo.partyName));
		sccp_copy_string(lineState->callInfo.partyNumber, "DND", sizeof(lineState->callInfo.partyNumber));
	}
}

sccp_channelState_t sccp_hint_getLinestate(const char *linename, const char *deviceId)
{
	struct sccp_hint_lineState *lineState = NULL;
	sccp_channelState_t state = SCCP_CHANNELSTATE_CONGESTION;

	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (sccp_strcaseequals(lineState->line->name, linename)) {
			state = lineState->state;
			break;
		}
	}
	SCCP_LIST_UNLOCK(&lineStates);
	return state;
}
