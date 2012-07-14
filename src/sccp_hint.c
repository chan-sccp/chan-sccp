
/*!
 * \file 	sccp_hint.c
 * \brief 	SCCP Hint Class
 * \author 	Marcello Ceschia < marcello.ceschia@users.sourceforge.net >
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note		For more information about how does hint update works, see \ref hint_update
 * \since 	2009-01-16
 * \remarks	Purpose: 	SCCP Hint
 * 		When to use:	Does the business of hint status
 *
 * $Date: 2011-01-04 17:29:12 +0100 (Tue, 04 Jan 2011) $
 * $Revision: 2215 $
 */

/*!
 * \file
 * \page sccp_hint How do hints work in chan-sccp-b
 *
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

SCCP_FILE_VERSION(__FILE__, "$Revision: 2215 $")

struct sccp_hint_lineState{
	sccp_line_t 		*line;
	sccp_channelState_t	state;
	
	/*!
	 * \brief Call Information Structure
	 */
	struct {
		char partyName[StationMaxNameSize];			/*!< Party Name */
		char partyNumber[StationMaxNameSize];			/*!< Party Number */
		skinny_calltype_t calltype;				/*!< Skinny Call Type */
	} callInfo;							/*!< Call Information Structure */
	
	SCCP_LIST_ENTRY(struct sccp_hint_lineState) list;		/*!< Hint Type Linked List Entry */
};
static void sccp_hint_eventListener(const sccp_event_t *event);
static void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForSharedLine(struct sccp_hint_lineState *lineState);
static void sccp_hint_updateLineStateForSingleLine(struct sccp_hint_lineState *lineState);
static void sccp_hint_checkForDND(struct sccp_hint_lineState *lineState);
static void sccp_hint_notifyPBX(struct sccp_hint_lineState *linestate);
static void sccp_hint_devstate_cb(const pbx_event_t *event, void *data);
static sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context);
static void sccp_hint_notifySubscribers(sccp_hint_list_t *hint);
static void sccp_hint_deviceRegistered(const sccp_device_t *device);
static void sccp_hint_deviceUnRegistered(const char *deviceName);
static void sccp_hint_addSubscription4Device(const sccp_device_t *device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice);
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t *event);


// SCCP_LIST_HEAD(, sccp_hint_list_t) sccp_hint_subscriptions;
SCCP_LIST_HEAD(, struct sccp_hint_lineState) lineStates;

SCCP_LIST_HEAD(, sccp_hint_list_t) sccp_hint_subscriptions;

/*!
 * \brief starting hint-module
 */
void sccp_hint_module_start()
{
	/* */
	SCCP_LIST_HEAD_INIT(&lineStates);

	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED, sccp_hint_eventListener, TRUE);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_hint_handleFeatureChangeEvent, TRUE);
}

/*!
 * \brief stop hint-module
 *
 * \lock
 * 	- sccp_hint_subscriptions
 */
void sccp_hint_module_stop()
{
  
	{
		sccp_hint_list_t *hint;
		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		while ((hint = SCCP_LIST_REMOVE_HEAD(&sccp_hint_subscriptions, list))) {
			pbx_event_unsubscribe(hint->device_state_sub);
			sccp_free(hint);
		}
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}
	
	{ 
		struct sccp_hint_lineState *lineState;
		SCCP_LIST_LOCK(&lineStates);
		while ((lineState = SCCP_LIST_REMOVE_HEAD(&lineStates, list))) {
			lineState->line = lineState->line ? sccp_line_release(lineState->line) : NULL;
			sccp_free(lineState);
		}
		SCCP_LIST_UNLOCK(&lineStates);
	}
	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED);
}


#ifdef CS_DYNAMIC_SPEEDDIAL
static inline boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice)
{
#ifdef CS_DYNAMIC_SPEEDDIAL_CID
	if ((device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7971 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7985)
	    && positionOnDevice <= 8)
		return TRUE;
#endif

	return FALSE;
}
#endif

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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Create hint for exten: %s context: %s\n", hint_exten, hint_context);

	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
									// CS_AST_HAS_NEW_HINT

	if (sccp_strlen_zero(hint_dialplan)) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
		return NULL;
	}

	hint = sccp_malloc(sizeof(sccp_hint_list_t));
	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: Memory Allocation Error while creating hint list for hint: %s@%s\n", hint_exten, hint_context);
		return NULL;
	}		
	memset(hint, 0, sizeof(sccp_hint_list_t));

	SCCP_LIST_HEAD_INIT(&hint->subscribers);
	sccp_mutex_init(&hint->lock);

	sccp_copy_string(hint->exten, hint_exten, sizeof(hint->exten));
	sccp_copy_string(hint->context, hint_context, sizeof(hint->context));
	sccp_copy_string(hint->hint_dialplan, hint_dialplan, sizeof(hint_dialplan));

	/*! \todo move pbx_event_subscribe to pbx_impl */
#if ASTERISK_VERSION_GROUP < 108
	hint->device_state_sub = pbx_event_subscribe(
				AST_EVENT_DEVICE_STATE_CHANGE, 
				sccp_hint_devstate_cb, hint, 
				AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan,
				AST_EVENT_IE_END);
#else
	hint->device_state_sub = pbx_event_subscribe(
				AST_EVENT_DEVICE_STATE_CHANGE, 
				sccp_hint_devstate_cb, "sccp_devstate_subscription", hint, 
				AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan,
				AST_EVENT_IE_END);
#endif
	/* get current state from pbx */
	struct ast_event *event = ast_event_get_cached(AST_EVENT_DEVICE_STATE,
				  AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan,
				  AST_EVENT_IE_END);
	
	if (event) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "restore state for '%s', state %d\n", hint->hint_dialplan, ast_event_get_ie_uint(event, AST_EVENT_IE_STATE));
	}else{
		/** this workaround restores the state by using ast_device_state function, may be it is also possible in an other way */
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "no state to restore for %s\n", hint->hint_dialplan);
		event = pbx_event_new(AST_EVENT_DEVICE_STATE,
			AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, hint->hint_dialplan, 
			AST_EVENT_IE_STATE, AST_EVENT_IE_PLTYPE_UINT, ast_device_state(hint->hint_dialplan), 
			AST_EVENT_IE_END
		);
	}
	
	if(event){
		  sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "restore2 state for '%s', state %d\n", hint->hint_dialplan, ast_event_get_ie_uint(event, AST_EVENT_IE_STATE));
		  sccp_hint_devstate_cb(event, hint);
		  ast_event_destroy(event);
	}

	return hint;
}

static void sccp_hint_devstate_cb(const pbx_event_t *event, void *data)
{
	int deviceState;
	sccp_hint_list_t *hint;
	
	hint = (sccp_hint_list_t *)data;
	
	deviceState	= pbx_event_get_ie_uint(event, AST_EVENT_IE_STATE);
#if CS_AST_HAS_EVENT_CIDNAME
	const char *cidName;
	const char *cidNumber;
	cidName		= pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNAME);
        cidNumber	= pbx_event_get_ie_str(event, AST_EVENT_IE_CEL_CIDNUM);
	memset(hint->callInfo.partyName, 0, sizeof(hint->callInfo.partyName));
	memset(hint->callInfo.partyNumber, 0, sizeof(hint->callInfo.partyNumber));
	
	if(cidName){
		sccp_copy_string(hint->callInfo.partyName, cidName, sizeof(hint->callInfo.partyName));
	}
	
	if(cidNumber){
		sccp_copy_string(hint->callInfo.partyNumber, cidNumber, sizeof(hint->callInfo.partyNumber));
	}
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "Got new hint event %s, state: %d, cidname: %s, cidnum: %s\n", hint->hint_dialplan, deviceState, cidName ? cidName : "NULL", cidNumber ? cidNumber : "NULL");
#else
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "Got new hint event %s, state: %d\n", hint->hint_dialplan, deviceState);
#endif	

	switch(deviceState){
	  case AST_DEVICE_UNKNOWN:
	    
	    break;
	  case AST_DEVICE_NOT_INUSE:
	    hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
	    break;
	  case AST_DEVICE_INUSE:
	    hint->currentState = SCCP_CHANNELSTATE_CONNECTED;
	    break;
	  case AST_DEVICE_BUSY:
#if CS_AST_HAS_EVENT_CIDNAME
		if(cidName && !strcasecmp(cidName, "DND")){
			hint->currentState = SCCP_CHANNELSTATE_DND;
		}else{
			hint->currentState = SCCP_CHANNELSTATE_BUSY;
		}
#else
		hint->currentState = SCCP_CHANNELSTATE_BUSY;
#endif
	    break;
	  case AST_DEVICE_INVALID:
	    hint->currentState = SCCP_CHANNELSTATE_INVALIDNUMBER;
	    break;
	  case AST_DEVICE_UNAVAILABLE:
	    hint->currentState = SCCP_CHANNELSTATE_CONGESTION;
	    break;
	  case AST_DEVICE_RINGING:
	  case AST_DEVICE_RINGINUSE:
	    hint->currentState = SCCP_CHANNELSTATE_RINGING;
	    break;
	  case AST_DEVICE_ONHOLD:
	    hint->currentState = SCCP_CHANNELSTATE_HOLD;
	    break;
	}
	
	sccp_hint_notifySubscribers(hint);
}


/*!
 * \brief Event Listener for Hints
 * \param event SCCP Event
 * 
 * \lock
 * 	- device
 * 	  - see sccp_hint_deviceRegistered()
 * 	  - see sccp_hint_deviceUnRegistered()
 */
void sccp_hint_eventListener(const sccp_event_t *event)
{
	sccp_device_t *device;

	if (!event)
		return;

	switch (event->type) {
	case SCCP_EVENT_DEVICE_REGISTERED:
		/* already retained by event */
		device = event->event.deviceRegistered.device;


		sccp_hint_deviceRegistered(device);
		break;
	case SCCP_EVENT_DEVICE_UNREGISTERED:
		device = event->event.deviceRegistered.device;

		if (device &&  device->id) {
			char *deviceName = strdupa(device->id);
			/* remove device from list */
			sccp_hint_deviceUnRegistered(deviceName);
		}

		break;
	case SCCP_EVENT_DEVICE_DETACHED:
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "device %s detached on line %s\n", event->event.deviceAttached.linedevice->device->id, event->event.deviceAttached.linedevice->line->name);
		sccp_hint_lineStatusChangedDebug(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device);
		break;
	default:
		break;
	}
}


/*!
 * \brief Handle line status change
 * \param line		SCCP Line that was changed
 * \param device	SCCP Device who initialied the change
 * 
 */
void sccp_hint_lineStatusChangedDebug(sccp_line_t *line, sccp_device_t *device)
{
	struct sccp_hint_lineState *lineState = NULL;

	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (lineState->line == line) {
			break;
		}
	}
	
	/** create new state holder for line */
	if(!lineState){
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: Create new hint_lineState for line: %s\n", DEV_ID_LOG(device), line->name);
		lineState = sccp_malloc(sizeof(struct sccp_hint_lineState));
		if (!lineState) {
			pbx_log(LOG_ERROR, "%s: Memory Allocation Error while creating hint-lineState object for line %s\n", DEV_ID_LOG(device), line->name);
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
 * \param hint SCCP Hint Linked List Pointer
 */
void sccp_hint_updateLineState(struct sccp_hint_lineState *lineState)
{
	sccp_line_t *line = NULL;
	
	if ((line = sccp_line_retain(lineState->line))) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: Update Line Channel State\n", line->name);
		if ((line->devices.size > 1 && line->channels.size > 1) || line->channels.size > 1) {
			/* line is currently shared */
			sccp_hint_updateLineStateForSharedLine(lineState);
		} else {
			/* just one device per line */
			sccp_hint_updateLineStateForSingleLine(lineState);
		}
		/** push chages to pbx */
		sccp_hint_notifyPBX(lineState);
		
		line = sccp_line_release(line);
	}
}

/*!
 * \brief set hint status for a line with more then one channel
 * \param hint	SCCP Hint Linked List Pointer
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

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "no line or no device; line: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	if (line->channels.size > 0) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: number of active channels %d\n", line->name, line->statistic.numberOfActiveChannels);
		if (line->channels.size == 1) {
			SCCP_LIST_LOCK(&line->channels);
			channel = SCCP_LIST_FIRST(&line->channels);
			SCCP_LIST_UNLOCK(&line->channels);
			if (channel && (channel = sccp_channel_retain(channel))) {
				if (channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN) {
					lineState->state = channel->state;
					
					/* set cid name/numbe information according to the call direction */
					if( SKINNY_CALLTYPE_INBOUND == channel->calltype ){
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyNumber));
					}else{
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
// 			sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyName));
// 			sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_IN_USE_REMOTE, sizeof(lineState->callInfo.partyNumber));
			lineState->state = SCCP_CHANNELSTATE_CONNECTED;
		}
	} else {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: no active channels\n", line->name);
		if (0 == line->devices.size ) {
			/** the line does not have a device attached, mark them as unavailable (congestion) */
// 			sccp_copy_string(lineState->callInfo.partyName, SKINNY_DISP_TEMP_FAIL, sizeof(lineState->callInfo.partyName));
// 			sccp_copy_string(lineState->callInfo.partyNumber, SKINNY_DISP_TEMP_FAIL, sizeof(lineState->callInfo.partyNumber));

			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: number of devices that have this line registered is 0\n", line->name);
			lineState->state = SCCP_CHANNELSTATE_ZOMBIE;	// CS_DYNAMIC_SPEEDDIAL
		} else {
			lineState->state = SCCP_CHANNELSTATE_ONHOOK;
		}
	}
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: Set sharedLineState to %s (%d)\n", line->name, channelstate2str(lineState->state), lineState->state);
}

/*!
 * \brief set hint status for a line with less or eq one channel
 * \param hint	SCCP Hint Linked List Pointer
 * 
 * \lock
 * 	- hint
 * 	  - see sccp_line_find_byname_wo()
 */
void sccp_hint_updateLineStateForSingleLine(struct sccp_hint_lineState *lineState)
{
	sccp_line_t *line = lineState->line;
	sccp_channel_t *channel = NULL;
	sccp_device_t *device = NULL;
	uint8_t state;
	
	/** clear cid information */
	memset(lineState->callInfo.partyName, 0, sizeof(lineState->callInfo.partyName));
	memset(lineState->callInfo.partyNumber, 0, sizeof(lineState->callInfo.partyNumber));

	/* no line, or line without devices */
	if (!line || (line && 0 == line->devices.size)) {
		lineState->state = SCCP_CHANNELSTATE_CONGESTION;

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "no line or no device; line: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	SCCP_LIST_LOCK(&line->channels);
	channel = SCCP_LIST_FIRST(&line->channels);
	SCCP_LIST_UNLOCK(&line->channels);
	if (channel && (channel = sccp_channel_retain(channel))) {
		lineState->callInfo.calltype = channel->calltype;

		SCCP_LIST_LOCK(&line->devices);
		sccp_linedevices_t *lineDevice = sccp_linedevice_retain(SCCP_LIST_FIRST(&line->devices));
		SCCP_LIST_UNLOCK(&line->devices);

		state = channel->state;
		if (lineDevice) {
			if ((device = sccp_device_retain(lineDevice->device))) {
				if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
					state = SCCP_CHANNELSTATE_DND;
				}
			}
		}

		switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
			lineState->state = SCCP_CHANNELSTATE_ONHOOK;
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

			if (!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {

				/** set cid name/number information according to the call direction */
				switch(channel->calltype){
					case SKINNY_CALLTYPE_INBOUND:
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.callingPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.callingPartyNumber, sizeof(lineState->callInfo.partyNumber));
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "set partyName: %s \n", channel->callInfo.callingPartyName);
					break;
					case SKINNY_CALLTYPE_OUTBOUND:
						sccp_copy_string(lineState->callInfo.partyName, channel->callInfo.calledPartyName, sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, channel->callInfo.calledPartyNumber, sizeof(lineState->callInfo.partyNumber));
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "set partyName: %s \n", channel->callInfo.calledPartyName);
					break;
					case SKINNY_CALLTYPE_FORWARD:
						sccp_copy_string(lineState->callInfo.partyName, "cfwd", sizeof(lineState->callInfo.partyName));
						sccp_copy_string(lineState->callInfo.partyNumber, "cfwd", sizeof(lineState->callInfo.partyNumber));
					break;
				}
			}
		break;
		}
		
		lineState->state = state;
		device = device ? sccp_device_release(device) : NULL;
		lineDevice = lineDevice ? sccp_linedevice_release(lineDevice) : NULL;
		channel = sccp_channel_release(channel);
	} else {
		lineState->state = SCCP_CHANNELSTATE_ONHOOK;
		sccp_hint_checkForDND(lineState);
	}// if(channel)
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: Set singleLineState to %s (%d)\n", line->name, channelstate2str(lineState->state), lineState->state);
}

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
	
	if(lineState->state == SCCP_CHANNELSTATE_DND){
		sccp_copy_string(lineState->callInfo.partyName, "DND", sizeof(lineState->callInfo.partyName));
		sccp_copy_string(lineState->callInfo.partyNumber, "DND", sizeof(lineState->callInfo.partyNumber));
	}
}

/*!
 * \brief Notify Asterisk of Hint State Change
 * \param line	SCCP Line
 * \param state SCCP Channel State
 */
void sccp_hint_notifyPBX(struct sccp_hint_lineState *lineState)
{
	char channelName[100];	// magic number
	sprintf(channelName, "SCCP/%s", lineState->line->name);
  
	enum ast_device_state newDeviceState = AST_DEVICE_UNKNOWN; 
	int event_signal_method = AST_EVENT_DEVICE_STATE_CHANGE;
	enum ast_device_state currentDeviceState = ast_device_state(channelName);

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "Notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", 
				   channelstate2str(lineState->state), 
				   lineState->state, 
				   pbxdevicestate2str(sccp_channelState2AstDeviceState(lineState->state)), 
				   sccp_channelState2AstDeviceState(lineState->state), 
				   lineState->line->name
	);
	
	if (currentDeviceState == AST_DEVICE_UNKNOWN || currentDeviceState == AST_DEVICE_UNAVAILABLE) {
		event_signal_method = AST_EVENT_DEVICE_STATE;							// new device registration
	}
	
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
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
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
			newDeviceState = AST_DEVICE_UNAVAILABLE;
			break;
		case SCCP_CHANNELSTATE_ZOMBIE:
			event_signal_method = AST_EVENT_DEVICE_STATE;						// device unregister
			newDeviceState = AST_DEVICE_UNAVAILABLE;
			break;
		  
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE:
		case SCCP_CHANNELSTATE_OFFHOOK:
		case SCCP_CHANNELSTATE_GETDIGITS:
		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_CONNECTED:
		case SCCP_CHANNELSTATE_PROCEED:
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
#if CS_AST_HAS_EVENT_CIDNAME
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "Set cidName to '%s' and cidNum to '%s' for hint\n", 
				   lineState->callInfo.partyName, 
				   lineState->callInfo.partyNumber
	);
#endif

	pbx_event_t 	 	*event;
	event = pbx_event_new(event_signal_method,
		  AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, channelName, 
		  AST_EVENT_IE_STATE, AST_EVENT_IE_PLTYPE_UINT, newDeviceState, 
#if CS_AST_HAS_EVENT_CIDNAME
		  AST_EVENT_IE_CEL_CIDNAME, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyName,
		  AST_EVENT_IE_CEL_CIDNUM, AST_EVENT_IE_PLTYPE_STR, lineState->callInfo.partyNumber,
#endif		  
		  AST_EVENT_IE_END);
	pbx_event_queue_and_cache(event);
}



/*!
 * \brief Handle Hints for Device Register
 * \param device SCCP Device
 * 
 * \note	device locked by parent
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
static void sccp_hint_deviceRegistered(const sccp_device_t *device)
{
	sccp_buttonconfig_t *config;
	uint8_t positionOnDevice = 0;

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		positionOnDevice++;

		if (config->type == SPEEDDIAL) {
			if (sccp_strlen_zero(config->button.speeddial.hint)) {
				continue;
			}
			sccp_hint_addSubscription4Device(device, config->button.speeddial.hint, config->instance, positionOnDevice);
		}
	}
}

/*!
 * \brief Handle Hints for Device UnRegister
 * \param deviceName Device as Char
 *
 * \note	device locked by parent
 *
 * \lock
 * 	- device->buttonconfig
 * 	  - see sccp_hint_unSubscribeHint()
 */
static void sccp_hint_deviceUnRegistered(const char *deviceName)
{
	sccp_hint_list_t *hint = NULL;
	sccp_hint_SubscribingDevice_t *subscriber;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {

		/* All subscriptions that have this device should be removed */
		SCCP_LIST_TRYLOCK(&hint->subscribers);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
			if (!strcasecmp(subscriber->device->id, deviceName)) {
				SCCP_LIST_REMOVE_CURRENT(list);
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
 * 	- sccp_hint_subscriptions is not always locked
 * 
 * \lock
 * 	- sccp_hint_subscriptions
 */
static void sccp_hint_addSubscription4Device(const sccp_device_t *device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice)
{
	sccp_hint_list_t *hint = NULL;

	char buffer[256] = "";
	char *splitter, *hint_exten, *hint_context;

	sccp_copy_string(buffer, hintStr, sizeof(buffer));

	if (!device) {
		pbx_log(LOG_ERROR, "adding hint to: %s without device is not allowed\n", hintStr);
		return;
	}

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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Dialplan %s for exten: %s and context: %s\n", hintStr, hint_exten, hint_context);

	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (sccp_strlen(hint_exten) == sccp_strlen(hint->exten)
		    && sccp_strlen(hint_context) == sccp_strlen(hint->context)
		    && sccp_strequals(hint_exten, hint->exten)
		    && sccp_strequals(hint_context, hint->context)) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "Hint found\n");
			break;
		}
	}

	/* we have no hint */
	if (!hint) {
		hint = sccp_hint_create(hint_exten, hint_context);
		if (!hint)
			return;

		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_hint_subscriptions, hint, list);
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	/* add subscribing device */
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "create subscriber\n");
	sccp_hint_SubscribingDevice_t *subscriber;

	subscriber = sccp_malloc(sizeof(sccp_hint_SubscribingDevice_t));
	if (!subscriber) {
		pbx_log(LOG_ERROR, "%s: Memory Allocation Error while creating subscritber object\n", device->id);
		return;
	}	
	memset(subscriber, 0, sizeof(sccp_hint_SubscribingDevice_t));

	subscriber->device = device;
	subscriber->instance = instance;
	subscriber->positionOnDevice = positionOnDevice;

	SCCP_LIST_INSERT_HEAD(&hint->subscribers, subscriber, list);

	sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
	sccp_hint_notifySubscribers(hint);
}

/*!
 * \brief send hint status to subscriber
 * \param hint SCCP Hint Linked List Pointer
 *
 * \todo Check if the actual device still exists while going throughthe hint->subscribers and not pointing at rubish
 */
static void sccp_hint_notifySubscribers(sccp_hint_list_t *hint)
{
	sccp_hint_SubscribingDevice_t *subscriber = NULL;
	sccp_moo_t *r;
	uint32_t state;								/* used to fall back to old behavior */
	sccp_speed_t k;

#ifdef CS_DYNAMIC_SPEEDDIAL
	char displayMessage[80];
#endif

	if (!&GLOB(module_running) || !sccp_refcount_isRunning()) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Skip processing hint while we are shutting down.\n");
		return;
	}

	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: no hint provided to notifySubscribers about\n");
		return;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "notify subscriber of %s\n", (hint->hint_dialplan) ? hint->hint_dialplan : "null");

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
		if (!subscriber->device) {
			SCCP_LIST_REMOVE_CURRENT(list);
			continue;
		}

		state = hint->currentState;

#ifdef CS_DYNAMIC_SPEEDDIAL
		if (subscriber->device->inuseprotocolversion >= 15) {
			sccp_dev_speed_find_byindex((sccp_device_t *) subscriber->device, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL, &k);

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
					if (sccp_hint_isCIDavailabe(subscriber->device, subscriber->positionOnDevice) == TRUE) {
						if(strlen(hint->callInfo.partyName) > 0){
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->", k.name);
						}else if(strlen(hint->callInfo.partyNumber) > 0){
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyNumber, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "->", k.name);
						}else{
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
					if (sccp_hint_isCIDavailabe(subscriber->device, subscriber->positionOnDevice) == TRUE) {
						if(strlen(hint->callInfo.partyName) > 0){
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "<->", k.name);
						}else if(strlen(hint->callInfo.partyNumber) > 0){
							snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", hint->callInfo.partyNumber, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? "<-" : "<->", k.name);
						}else{
							snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
						}
					} else {
						snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
					}
					r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_INUSE);	/* connected */
					break;
				}

				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "set display name to: \"%s\"\n", displayMessage);
				sccp_copy_string(r->msg.FeatureStatDynamicMessage.DisplayName, displayMessage, sizeof(r->msg.FeatureStatDynamicMessage.DisplayName));
				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "notify device: %s@%d state: %d(%d)\n", DEV_ID_LOG(subscriber->device), subscriber->instance, hint->currentState, r->msg.FeatureStatDynamicMessage.lel_status);
				sccp_dev_send(subscriber->device, r);
			} else {
				sccp_free(r);
			}

			continue;
		}

		/*
		   we have dynamic speeddial enabled, but subscriber can not handle this.
		   We have to switch back to old hint style and send old state.
		 */
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: can not handle dynamic speeddial, fall back to old behavior using state %d\n", DEV_ID_LOG(subscriber->device), state);
#endif										// CS_DYNAMIC_SPEEDDIAL
		/*
		   With the old hint style we should only use SCCP_CHANNELSTATE_ONHOOK and SCCP_CHANNELSTATE_CALLREMOTEMULTILINE as callstate,
		   otherwise we get a callplane on device -> set all states except onhook to SCCP_CHANNELSTATE_CALLREMOTEMULTILINE -MC
		 */
//              if (hint->currentState != SCCP_CHANNELSTATE_ONHOOK) {
//                      state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
//              } else {
//                      state = SCCP_CHANNELSTATE_ONHOOK;
//              }
#ifndef CS_EXPERIMENTAL_
		switch (hint->currentState) {
		case SCCP_CHANNELSTATE_ONHOOK:
		case SCCP_CHANNELSTATE_RINGING:
			state = hint->currentState;
			break;
		default:
			state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		}

		if (SCCP_CHANNELSTATE_RINGING == hint->previousState) {
			/* we send a congestion to the phone, so call will not be marked as missed call */
			sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, SCCP_CHANNELSTATE_CONGESTION, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
		}

		sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, state, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_COLLAPSED);

		/* create CallInfoMessage */
		REQ(r, CallInfoMessage);

		/* set callInfo */
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, hint->callInfo.partyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, hint->callInfo.partyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting callingPartyName: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.callingPartyName);
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting calledPartyName: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.calledPartyName);

		sccp_copy_string(r->msg.CallInfoMessage.callingParty, hint->callInfo.partyNumber, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, hint->callInfo.partyNumber, sizeof(r->msg.CallInfoMessage.calledParty));
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting callingParty: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.callingParty);
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting calledParty: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.calledParty);

		r->msg.CallInfoMessage.lel_lineId = htolel(subscriber->instance);
		r->msg.CallInfoMessage.lel_callRef = htolel(0);
		r->msg.CallInfoMessage.lel_callType = htolel(hint->callInfo.calltype);
		sccp_dev_send(subscriber->device, r);
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "notify device: %s@%d state: %d\n", DEV_ID_LOG(subscriber->device), subscriber->instance, hint->currentState);

		if (hint->currentState == SCCP_CHANNELSTATE_ONHOOK) {
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
		} else {
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_INUSEHINT);
		}
#else
		switch (hint->currentState) {
		case SCCP_CHANNELSTATE_ONHOOK:
			break;
		default:
			state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		}

		if (SCCP_CHANNELSTATE_RINGING == hint->previousState) {
			/* we send a congestion to the phone, so call will not be marked as missed call */
			sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, SCCP_CHANNELSTATE_CONGESTION, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
			// remove from messageStack
//			sccp_device_clearMessageFromStack((sccp_device_t *)subscriber->device, 8);
		}

		sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, state, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_COLLAPSED);

		if (SCCP_CHANNELSTATE_RINGING == hint->currentState) {
			char msg_buf[40];
			if (SKINNY_CALLTYPE_INBOUND == hint->callInfo.calltype) {
				if (!sccp_strlen_zero(hint->callInfo.partyName)) {
					sprintf(msg_buf, "%s -> %s", hint->callInfo.partyNumber, hint->exten);
				} else {
					sprintf(msg_buf, "%s -> %s", hint->callInfo.partyName, hint->exten);
				}
			} else {
				if (!sccp_strlen_zero(hint->callInfo.partyName)) {
					sprintf(msg_buf, "%s -> %s", hint->exten, hint->callInfo.partyNumber);
				} else {
					sprintf(msg_buf, "%s -> %s", hint->exten, hint->callInfo.partyName);
				}
			}
			sccp_dev_displaynotify(subscriber->device, msg_buf, 5);
			// possibly use messageStack instead
//			sccp_device_addMessageToStack((sccp_device_t *)subscriber->device, 8, msg_buf);
		}

		if (hint->currentState == SCCP_CHANNELSTATE_ONHOOK) {
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
		} else {
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_INUSEHINT);
		}
#endif
				
	}
	SCCP_LIST_TRAVERSE_SAFE_END
  
}

/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
static void sccp_hint_handleFeatureChangeEvent(const sccp_event_t *event)
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
					line = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
					if (line) {
						sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dndFeature.status ? "on" : "off", line->name);
						sccp_hint_lineStatusChangedDebug(line, d);
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

sccp_channelState_t sccp_hint_getLinestate(const char *linename, const char *deviceId){
	struct sccp_hint_lineState *lineState = NULL;
	sccp_channelState_t state = SCCP_CHANNELSTATE_CONGESTION;


	SCCP_LIST_LOCK(&lineStates);
	SCCP_LIST_TRAVERSE(&lineStates, lineState, list) {
		if (!strcasecmp(lineState->line->name, linename)) {
			state = lineState->state;
			break;
		}
	}
	SCCP_LIST_UNLOCK(&lineStates);
	
// 	pbx_log(LOG_WARNING, "LineState '%d'\n", state);
	return state;
}
