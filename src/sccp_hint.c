
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
 * \section hint_update How does hint update work

 \dot
 digraph updateHint {
	asteriskEvent[ label="asterisk event" shape=rect fontsize=8];
	sccp_hint_state[ label="sccp_hint_state" shape=rect style=rounded URL="\ref sccp_hint_state" fontsize=8];
	and[label="and" shape=circle fontsize=8]
	sccp_hint_list_t[ label="sccp_hint_list_t" shape=circle URL="\ref sccp_hint_list_t" fontsize=8];
	sccp_hint_remoteNotification_thread[ label="sccp_hint_remoteNotification_thread" shape=rect style=rounded  URL="\ref sccp_hint_remoteNotification_thread" fontsize=8];
	sccp_hint_notifySubscribers[ label="sccp_hint_notifySubscribers" shape=rect style=rounded URL="\ref sccp_hint_notifySubscribers" fontsize=8];

	lineStatusChanged[label="line status changed" shape=rect fontsize=8];
	sccp_hint_lineStatusChanged[label="sccp_hint_lineStatusChanged" shape=rect style=rounded URL="\ref sccp_hint_lineStatusChanged" fontsize=8];
	sccp_hint_hintStatusUpdate[label="sccp_hint_hintStatusUpdate" shape=rect style=rounded URL="\ref sccp_hint_hintStatusUpdate" fontsize=8];
	checkShared[label="is shared line?" shape=diamond fontsize=8];
	sccp_hint_notificationForSharedLine[label="sccp_hint_notificationForSharedLine" shape=rect style=rounded URL="\ref sccp_hint_notificationForSharedLine" fontsize=8];
	sccp_hint_notificationForSingleLine[label="sccp_hint_notificationForSingleLine" shape=rect style=rounded URL="\ref sccp_hint_notificationForSingleLine" fontsize=8];

	end[shape=point];
	asteriskEvent -> sccp_hint_state;
	sccp_hint_state -> and;
	and -> sccp_hint_list_t[label="update" fontsize=7];
	and -> sccp_hint_remoteNotification_thread;
	sccp_hint_remoteNotification_thread -> sccp_hint_list_t[label="update" fontsize=7];

	lineStatusChanged -> sccp_hint_lineStatusChanged;
	sccp_hint_lineStatusChanged -> sccp_hint_hintStatusUpdate;
	sccp_hint_hintStatusUpdate -> checkShared;
	checkShared -> sccp_hint_notificationForSingleLine[label="no" fontsize=7];
	checkShared -> sccp_hint_notificationForSharedLine[label="yes" fontsize=7];
	sccp_hint_notificationForSingleLine -> sccp_hint_list_t[label="update" fontsize=7];
	sccp_hint_notificationForSharedLine -> sccp_hint_list_t[label="update" fontsize=7];

	sccp_hint_list_t -> sccp_hint_notifySubscribers;
	sccp_hint_notifySubscribers -> end;
 }
 \enddot
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2215 $")

void sccp_hint_notifyAsterisk(sccp_line_t * line, sccp_channelState_t state);
static void *sccp_hint_remoteNotification_thread(void *data);
static void sccp_hint_checkForDND(sccp_hint_list_t * hint, sccp_line_t * line);

sccp_hint_list_t *sccp_hint_create(char *hint_context, char *hint_exten);
void sccp_hint_subscribeHint(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice);
void sccp_hint_unSubscribeHint(const sccp_device_t * device, const char *hintStr, uint8_t instance);

void sccp_hint_eventListener(const sccp_event_t ** event);
void sccp_hint_deviceRegistered(const sccp_device_t * device);
void sccp_hint_deviceUnRegistered(const char *deviceName);

void sccp_hint_notifySubscribers(sccp_hint_list_t * hint);
void sccp_hint_hintStatusUpdate(sccp_hint_list_t * hint);
void sccp_hint_notificationForSharedLine(sccp_hint_list_t * hint);
void sccp_hint_notificationForSingleLine(sccp_hint_list_t * hint);
void sccp_hint_handleFeatureChangeEvent(const sccp_event_t ** event);

#ifdef AST_EVENT_IE_CIDNAME
static void sccp_hint_devicestate_cb(const struct ast_event *ast_event, void *data);
#endif										// AST_EVENT_IE_CIDNAME

SCCP_LIST_HEAD(, sccp_hint_list_t) sccp_hint_subscriptions;

/*!
 * \brief starting hint-module
 */
void sccp_hint_module_start()
{
	/* */
	SCCP_LIST_HEAD_INIT(&sccp_hint_subscriptions);

	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED, sccp_hint_eventListener);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_hint_handleFeatureChangeEvent);
}

/*!
 * \brief stop hint-module
 *
 * \lock
 * 	- sccp_hint_subscriptions
 */
void sccp_hint_module_stop()
{
	sccp_hint_list_t *hint;
	sccp_hint_SubscribingDevice_t *subscriber = NULL;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	while ((hint = SCCP_LIST_REMOVE_HEAD(&sccp_hint_subscriptions, list))) {
		if (hint->hintType == ASTERISK) {
			pbx_extension_state_del(hint->type.asterisk.hintid, NULL);
		}
		while ((subscriber = SCCP_LIST_REMOVE_HEAD(&hint->subscribers, list))) {
			sccp_free(subscriber);
		}
		sccp_free(hint);
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

#ifdef CS_DYNAMIC_SPEEDDIAL

/*! 
 * \brief can we us cid for device?
 */
static boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice)
{

#ifdef CS_DYNAMIC_SPEEDDIAL_CID
	if ((device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7971 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7985)
	    && positionOnDevice <= 8)

		return TRUE;
#endif

	return FALSE;
}
#endif										// CS_DYNAMIC_SPEEDDIAL

/*!
 * \brief Event Listener for Hints
 * \param event SCCP Event
 * 
 * \lock
 * 	- device
 * 	  - see sccp_hint_deviceRegistered()
 * 	  - see sccp_hint_deviceUnRegistered()
 */
void sccp_hint_eventListener(const sccp_event_t ** event)
{
	const sccp_event_t *e = *event;
	sccp_device_t *device;

	if (!e)
		return;

	switch (e->type) {
	case SCCP_EVENT_DEVICE_REGISTERED:
		device = e->event.deviceRegistered.device;

		if (!device) {
			pbx_log(LOG_ERROR, "Error posting deviceRegistered event (null device)\n");
			return;
		}

		sccp_device_lock(device);
		sccp_hint_deviceRegistered(device);
		sccp_device_unlock(device);

		break;

	case SCCP_EVENT_DEVICE_UNREGISTERED:
		device = e->event.deviceRegistered.device;

		if (!device) {
			pbx_log(LOG_ERROR, "Error posting deviceUnregistered event (null device)\n");
			return;
		}

		sccp_device_lock(device);
		char *deviceName = strdup(device->id);

		sccp_device_unlock(device);

		/* remove device from list */
		sccp_hint_deviceUnRegistered(deviceName);

		sccp_free(deviceName);

		break;
	case SCCP_EVENT_DEVICE_DETACHED:
		sccp_hint_lineStatusChanged(e->event.deviceAttached.linedevice->line, e->event.deviceAttached.linedevice->device, NULL, 0, 0);
		break;
	default:
		break;
	}
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
void sccp_hint_deviceRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	uint8_t positionOnDevice = 0;

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		positionOnDevice++;

		if (config->type == SPEEDDIAL) {
			if (sccp_strlen_zero(config->button.speeddial.hint)) {
				continue;
			}
			sccp_hint_subscribeHint(device, config->button.speeddial.hint, config->instance, positionOnDevice);

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
void sccp_hint_deviceUnRegistered(const char *deviceName)
{
	sccp_hint_list_t *hint = NULL;
	sccp_hint_SubscribingDevice_t *subscriber;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {

		/* All subscriptions that have this device should be removed */
		SCCP_LIST_TRYLOCK(&hint->subscribers);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
			if (!strcasecmp(subscriber->device->id, deviceName))
				SCCP_LIST_REMOVE_CURRENT(list);
		}
		SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&hint->subscribers);
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

/*!
 * \brief Asterisk Hint Wrapper
 * \param context Context as char
 * \param exten Extension as char
 * \param state Asterisk Extension State
 * \param data Asterisk Data
 * 
 * \called_from_asterisk
 */
#if ASTERISK_VERSION_NUMBER >= 11001
int sccp_hint_state(const char *context, const char *exten, enum ast_extension_states state, void *data)
#else
int sccp_hint_state(char *context, char *exten, enum ast_extension_states state, void *data)
#endif
{
	sccp_hint_list_t *hint = NULL;

	hint = data;

	if (state == -1 || !hint) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: get new hint, but no hint param exists\n");
		return 0;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: get new hint state %s(%d) for %s\n", extensionstatus2str(state), state, hint->hint_dialplan);
	hint->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;
	/* converting asterisk state -> sccp state */
	switch (state) {
	case AST_EXTENSION_REMOVED:
		hint->currentState = SCCP_CHANNELSTATE_ZOMBIE;
		
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		break;
	case AST_EXTENSION_DEACTIVATED:
		hint->currentState = SCCP_CHANNELSTATE_ZOMBIE;
		
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		break;
	case AST_EXTENSION_NOT_INUSE:
		hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
		
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_ON_HOOK, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_ON_HOOK, sizeof(hint->callInfo.calledPartyName));
		break;
	case AST_EXTENSION_INUSE:
		hint->currentState = SCCP_CHANNELSTATE_PROCEED;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.calledPartyName));

		break;
	case AST_EXTENSION_BUSY:
		hint->currentState = SCCP_CHANNELSTATE_BUSY;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.calledPartyName));

		break;
	case AST_EXTENSION_UNAVAILABLE:
		hint->currentState = SCCP_CHANNELSTATE_DOWN;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));

		break;
#ifdef CS_AST_HAS_EXTENSION_ONHOLD
	case AST_EXTENSION_ONHOLD:
		hint->currentState = SCCP_CHANNELSTATE_HOLD;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.calledPartyName));
		break;
#endif										// CS_AST_HAS_EXTENSION_ONHOLD
#ifdef CS_AST_HAS_EXTENSION_RINGING
	case AST_EXTENSION_RINGING:
		hint->currentState = SCCP_CHANNELSTATE_RINGING;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
		break;
#endif										// CS_AST_HAS_EXTENSION_RINGING
	default:
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: Unmapped hint state %d for %s\n", state, hint->hint_dialplan);
		hint->currentState = SCCP_CHANNELSTATE_DOWN;

		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
	}

	/* push to subscribers */
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: Notifying Subscribers for %s\n", hint->hint_dialplan);
	sccp_hint_notifySubscribers(hint);

#ifndef AST_EVENT_IE_CIDNAME
	if (state == AST_EXTENSION_INUSE || state == AST_EXTENSION_BUSY) {
		if (hint->type.asterisk.notificationThread != AST_PTHREADT_NULL) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: Thread Kill %s\n", hint->hint_dialplan);
			pthread_kill(hint->type.asterisk.notificationThread, SIGURG);
			hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
		}
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: Thread Background %s\n", hint->hint_dialplan);
		if (pbx_pthread_create_background(&hint->type.asterisk.notificationThread, NULL, sccp_hint_remoteNotification_thread, hint) < 0) {
			return -1;
		}

	}
#endif										// AST_EVENT_IE_CIDNAME
	return 0;
}

#ifdef AST_EVENT_IE_CIDNAME

/*!
 * \brief handle AST_EVENT_DEVICE_STATE_CHANGE events subscribed by us
 * \param ast_event event
 * \param data hint
 */
static void sccp_hint_devicestate_cb(const struct ast_event *ast_event, void *data)
{
	sccp_hint_list_t *hint = data;
	enum ast_device_state state;
	const char *cidnum;
	const char *cidname;
	const char *channelStr;

	if (!hint || !ast_event)
		return;

	channelStr = pbx_event_get_ie_str(ast_event, AST_EVENT_IE_DEVICE);
	state = pbx_event_get_ie_uint(ast_event, AST_EVENT_IE_STATE);
	cidnum = pbx_event_get_ie_str(ast_event, AST_EVENT_IE_CIDNUM);
	cidname = pbx_event_get_ie_str(ast_event, AST_EVENT_IE_CIDNAME);

	pbx_log(LOG_NOTICE, "got device state change event from asterisk channel: %s, cidname: %s, cidnum %s\n", (channelStr) ? channelStr : "NULL", (cidname) ? cidname : "NULL", (cidnum) ? cidnum : "NULL");

}
#endif										// AST_EVENT_IE_CIDNAME

/*!
 * \brief send hint status to subscriber
 * \param hint SCCP Hint Linked List Pointer
 *
 * \todo Check if the actual device still exists while going throughthe hint->subscribers and not pointing at rubish
 */
void sccp_hint_notifySubscribers(sccp_hint_list_t * hint)
{
	sccp_hint_SubscribingDevice_t *subscriber = NULL;
	sccp_moo_t *r;
	uint32_t state;								/* used to fall back to old behavior */
#ifdef CS_DYNAMIC_SPEEDDIAL
	char displayMessage[80];
#endif

	if (!hint)
		return;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "notify subscriber of %s\n", (hint->hint_dialplan) ? hint->hint_dialplan : "null");

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
		if (!subscriber->device) {
			SCCP_LIST_REMOVE_CURRENT(list);
			continue;
		}

		state = hint->currentState;

#ifdef CS_DYNAMIC_SPEEDDIAL
		if (subscriber->device->inuseprotocolversion >= 15) {
			sccp_speed_t *k = sccp_dev_speed_find_byindex((sccp_device_t *) subscriber->device, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL);

			REQ(r, FeatureStatDynamicMessage);
			r->msg.FeatureStatDynamicMessage.lel_instance = htolel(subscriber->instance);
			r->msg.FeatureStatDynamicMessage.lel_type = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
			
			
			switch (hint->currentState) {
			case SCCP_CHANNELSTATE_ONHOOK:
				snprintf(displayMessage, sizeof(displayMessage), (k) ? k->name : "unknown speeddial", sizeof(displayMessage));
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_IDLE);
				break;

			case SCCP_CHANNELSTATE_DOWN:
				snprintf(displayMessage, sizeof(displayMessage), (k) ? k->name : "unknown speeddial", sizeof(displayMessage));
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_UNKNOWN);	/* default state */
				break;

			case SCCP_CHANNELSTATE_RINGING:
				if (sccp_hint_isCIDavailabe(subscriber->device, subscriber->positionOnDevice) == TRUE) {
					snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? hint->callInfo.calledPartyName : hint->callInfo.callingPartyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? " <- " : " -> ", (k) ? k->name : "unknown speeddial");
				} else {
					snprintf(displayMessage, sizeof(displayMessage), "%s", (k) ? k->name : "unknown speeddial");
				}
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_ALERTING);	/* ringin */
				break;

			case SCCP_CHANNELSTATE_DND:
				snprintf(displayMessage, sizeof(displayMessage), (k) ? k->name : "unknown speeddial", sizeof(displayMessage));
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_DND);	/* dnd */
				break;

			case SCCP_CHANNELSTATE_CONGESTION:
				snprintf(displayMessage, sizeof(displayMessage), (k) ? k->name : "unknown speeddial", sizeof(displayMessage));
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_UNKNOWN);	/* device/line not found */
				break;

			default:
				if (sccp_hint_isCIDavailabe(subscriber->device, subscriber->positionOnDevice) == TRUE) {
					snprintf(displayMessage, sizeof(displayMessage), "%s %s %s", (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? hint->callInfo.calledPartyName : hint->callInfo.callingPartyName, (hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND) ? " <- " : " -> ", (k) ? k->name : "unknown speeddial");
				} else {
					snprintf(displayMessage, sizeof(displayMessage), "%s", (k) ? k->name : "unknown speeddial");
				}
				r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_INUSE);	/* connected */
				break;
			}
			

			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "set display name to: \"%s\"\n", displayMessage);
			sccp_copy_string(r->msg.FeatureStatDynamicMessage.DisplayName, displayMessage, sizeof(r->msg.FeatureStatDynamicMessage.DisplayName));
			sccp_dev_send(subscriber->device, r);

			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "notify device: %s@%d state: %d(%d)\n", DEV_ID_LOG(subscriber->device), subscriber->instance, hint->currentState, r->msg.FeatureStatDynamicMessage.lel_status);

			if (k)
				sccp_free(k);

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
// 		if (hint->currentState != SCCP_CHANNELSTATE_ONHOOK) {
// 			state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
// 		} else {
// 			state = SCCP_CHANNELSTATE_ONHOOK;
// 		}
		switch(hint->currentState){
			case SCCP_CHANNELSTATE_ONHOOK:
			case SCCP_CHANNELSTATE_RINGING:
				state = hint->currentState;
				break;
			default:
				state = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
		}
		
		
		if(SCCP_CHANNELSTATE_RINGING == hint->previousState){
			/* we send a congestion to the phone, so call will not be marked as missed call */
			sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, SCCP_CHANNELSTATE_CONGESTION, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
		}

		sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, state, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_COLLAPSED);

		/* create CallInfoMessage */
		REQ(r, CallInfoMessage);

		/* set callInfo */
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, hint->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, hint->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting callingPartyName: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.callingPartyName);
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: setting calledPartyName: '%s'\n", DEV_ID_LOG(subscriber->device), r->msg.CallInfoMessage.calledPartyName);

		sccp_copy_string(r->msg.CallInfoMessage.callingParty, hint->callInfo.callingParty, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, hint->callInfo.calledParty, sizeof(r->msg.CallInfoMessage.calledParty));
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
	}
	SCCP_LIST_TRAVERSE_SAFE_END
  
}

/*!
 * \brief Handle line status change
 * \param line		SCCP Line
 * \param device	SCCP Device
 * \param channel	SCCP Channel
 * \param previousState Previous Channel State
 * \param state		New Channel State
 * \param file		Source File Name as char
 * \param callerLine 	Source Line Number as int
 * 
 * \lock
 * 	- sccp_hint_subscriptions
 * 	  - see sccp_hint_hintStatusUpdate()
 * 	  - see sccp_hint_notifySubscribers()
 */
void sccp_hint_lineStatusChangedDebug(sccp_line_t * line, sccp_device_t * device, sccp_channel_t * channel, sccp_channelState_t previousState, sccp_channelState_t state, char *file, int callerLine)
{
	sccp_hint_list_t *hint = NULL;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "sccp_hint_lineStatusChanged: from %s:%d\n", file, callerLine);
	if (!line)
		return;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (strlen(line->name) == strlen(hint->type.internal.lineName)
		    && !strcmp(line->name, hint->type.internal.lineName)) {

			/* update hint */
			sccp_hint_hintStatusUpdate(hint);
			/* send to subscriber */
			sccp_hint_notifySubscribers(hint);
		}
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

/*!
 * \brief Handle Hint Status Update
 * \param hint SCCP Hint Linked List Pointer
 */
void sccp_hint_hintStatusUpdate(sccp_hint_list_t * hint)
{
	sccp_line_t *line = NULL;

	if (!hint)
		return;

	line = sccp_line_find_byname(hint->type.internal.lineName);
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "hint %s@%s has changed\n", hint->exten, hint->context);
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "line %s has %d device%s --> notify %s\n", line->name, line->devices.size, (line->devices.size > 1) ? "s" : "", (line->devices.size > 1) ? "shared line change" : "single line change");

	if ((line->devices.size > 1 && line->channels.size > 1) || line->channels.size > 1) {
		/* line is currently shared */
		sccp_hint_notificationForSharedLine(hint);
	} else {
		/* just one device per line */
		sccp_hint_notificationForSingleLine(hint);
	}

	/* notify asterisk */
	sccp_hint_notifySubscribers(hint);
	sccp_hint_notifyAsterisk(line, hint->currentState);
	
	hint->previousState = hint->currentState;
}

/*!
 * \brief Notify Asterisk of Hint State Change
 * \param line	SCCP Line
 * \param state SCCP Channel State
 */
void sccp_hint_notifyAsterisk(sccp_line_t * line, sccp_channelState_t state)
{
	if (!line)
		return;

#ifdef CS_NEW_DEVICESTATE

#    ifndef AST_EVENT_IE_CIDNAME
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", channelstate2str(state), state, pbxdevicestate2str(sccp_channelState2AstDeviceState(state)), sccp_channelState2AstDeviceState(state), line->name);
	pbx_devstate_changed(sccp_channelState2AstDeviceState(state), "SCCP/%s", line->name);
#    else
	struct ast_event *event;
	char channelName[100];

	sprintf(channelName, "SCCP/%s", line->name);

	if (!(event = pbx_event_new(AST_EVENT_DEVICE_STATE_CHANGE, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, channelName, AST_EVENT_IE_STATE, AST_EVENT_IE_PLTYPE_UINT, sccp_channelState2AstDeviceState(state), AST_EVENT_IE_CIDNAME, AST_EVENT_IE_PLTYPE_STR, strdup(l->cid_name), AST_EVENT_IE_CIDNUM, AST_EVENT_IE_PLTYPE_STR, strdup(l->cid_num), AST_EVENT_IE_END))) {

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", channelstate2str(state), state, pbxdevicestate2str(sccp_channelState2AstDeviceState(state)), sccp_channelState2AstDeviceState(state), line->name);
		pbx_devstate_changed(sccp_channelState2AstDeviceState(state), "%s", channelName);
		return;
	}
#    endif									// AST_EVENT_IE_CIDNAME

#else
	pbx_device_state_changed("SCCP/%s", line->name);
#endif										// CS_NEW_DEVICESTATE
}

/* private functions */

/*!
 * \brief set hint status for a line with more then one channel
 * \param hint	SCCP Hint Linked List Pointer
 */
void sccp_hint_notificationForSharedLine(sccp_hint_list_t * hint)
{
	sccp_line_t *line = sccp_line_find_byname_wo(hint->type.internal.lineName, FALSE);
	sccp_channel_t *channel = NULL;

	memset(hint->callInfo.callingPartyName, 0, sizeof(hint->callInfo.callingPartyName));
	memset(hint->callInfo.callingParty, 0, sizeof(hint->callInfo.callingParty));

	memset(hint->callInfo.calledPartyName, 0, sizeof(hint->callInfo.calledPartyName));
	memset(hint->callInfo.calledParty, 0, sizeof(hint->callInfo.calledParty));

	hint->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;

	if (!line) {
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
		return;
	}

	if (line->channels.size > 0) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: number of active channels %d\n", line->name, line->statistic.numberOfActiveChannels);
		if (line->channels.size == 1) {
			channel = SCCP_LIST_FIRST(&line->channels);
			if (!channel) {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				return;
			}

			hint->callInfo.calltype = channel->calltype;
			if (channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN) {
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));

			} else {
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
			}
		} else if (line->channels.size > 1) {
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.calledPartyName));
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;

		}
	} else {
		if (line->devices.size == 0) {
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));

			hint->currentState = SCCP_CHANNELSTATE_CONGESTION;	// CS_DYNAMIC_SPEEDDIAL
		} else {
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
		}
	}
}

/*!
 * \brief set hint status for a line with less or eq one channel
 * \param hint	SCCP Hint Linked List Pointer
 * 
 * \lock
 * 	- hint
 * 	  - see sccp_line_find_byname_wo()
 */
void sccp_hint_notificationForSingleLine(sccp_hint_list_t * hint)
{
	sccp_line_t *line = NULL;
	sccp_channel_t *channel = NULL;
	uint8_t state;

	if (!hint)
		return;

	sccp_mutex_lock(&hint->lock);
	line = sccp_line_find_byname_wo(hint->type.internal.lineName, FALSE);

	/* no line, or line without devices */
	if (!line || (line && line->devices.size == 0)) {
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));

		
		hint->currentState = SCCP_CHANNELSTATE_CONGESTION;

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "no line or no device; line: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	memset(hint->callInfo.callingPartyName, 0, sizeof(hint->callInfo.callingPartyName));
	memset(hint->callInfo.callingParty, 0, sizeof(hint->callInfo.callingParty));

	memset(hint->callInfo.calledPartyName, 0, sizeof(hint->callInfo.calledPartyName));
	memset(hint->callInfo.calledParty, 0, sizeof(hint->callInfo.calledParty));

	channel = SCCP_LIST_FIRST(&line->channels);
	if (channel) {
		hint->callInfo.calltype = channel->calltype;

		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;	/* not a good idea to set this to channel->currentState -MC */

		sccp_linedevices_t *lineDevice = SCCP_LIST_FIRST(&line->devices);
		sccp_device_t *device = NULL;

		state = channel->state;
		if (lineDevice) {
			device = lineDevice->device;
			if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				state = SCCP_CHANNELSTATE_DND;
			}
		}

		switch (state) {
		case SCCP_CHANNELSTATE_DOWN:
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			break;
		case SCCP_CHANNELSTATE_OFFHOOK:
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.calledPartyName));
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		case SCCP_CHANNELSTATE_DND:
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.calledPartyName));

			hint->currentState = SCCP_CHANNELSTATE_DND;
			break;
		case SCCP_CHANNELSTATE_GETDIGITS:
			sccp_copy_string(hint->callInfo.callingPartyName, channel->dialedNumber, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, channel->dialedNumber, sizeof(hint->callInfo.calledPartyName));

			sccp_copy_string(hint->callInfo.callingParty, channel->dialedNumber, sizeof(hint->callInfo.callingParty));
			sccp_copy_string(hint->callInfo.calledParty, channel->dialedNumber, sizeof(hint->callInfo.calledParty));
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		case SCCP_CHANNELSTATE_SPEEDDIAL:
			break;
		case SCCP_CHANNELSTATE_ONHOOK:
			break;
		case SCCP_CHANNELSTATE_RINGOUT:
		case SCCP_CHANNELSTATE_CONNECTED:
		case SCCP_CHANNELSTATE_PROCEED:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			if (!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {

				sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));

				sccp_copy_string(hint->callInfo.callingParty, channel->callInfo.callingPartyNumber, sizeof(hint->callInfo.callingParty));
				sccp_copy_string(hint->callInfo.calledParty, channel->callInfo.calledPartyNumber, sizeof(hint->callInfo.calledParty));

			} else {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
			}
			break;

		case SCCP_CHANNELSTATE_RINGING:
			hint->currentState = SCCP_CHANNELSTATE_RINGING;

			if (!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {

				sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));

				sccp_copy_string(hint->callInfo.callingParty, channel->callInfo.callingPartyNumber, sizeof(hint->callInfo.callingParty));
				sccp_copy_string(hint->callInfo.calledParty, channel->callInfo.calledPartyNumber, sizeof(hint->callInfo.calledParty));

			} else {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
			}
			break;

		case SCCP_CHANNELSTATE_DIALING:
		case SCCP_CHANNELSTATE_DIGITSFOLL:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			if (!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {

				sccp_copy_string(hint->callInfo.callingPartyName, channel->dialedNumber, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, channel->dialedNumber, sizeof(hint->callInfo.calledPartyName));

				sccp_copy_string(hint->callInfo.callingParty, channel->dialedNumber, sizeof(hint->callInfo.callingParty));
				sccp_copy_string(hint->callInfo.calledParty, channel->dialedNumber, sizeof(hint->callInfo.calledParty));

			} else {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
			}
			break;
		case SCCP_CHANNELSTATE_BUSY:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.calledPartyName));
			break;
		case SCCP_CHANNELSTATE_HOLD:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.calledPartyName));
			break;
		case SCCP_CHANNELSTATE_CONGESTION:
		case SCCP_CHANNELSTATE_CALLWAITING:
		case SCCP_CHANNELSTATE_CALLTRANSFER:
		case SCCP_CHANNELSTATE_CALLCONFERENCE:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		case SCCP_CHANNELSTATE_CALLPARK:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.calledPartyName));
			break;
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
		case SCCP_CHANNELSTATE_INVALIDNUMBER:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			break;
		default:
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			break;
		}
	} else {
		sccp_line_lock(line);
		sccp_hint_checkForDND(hint, line);
		sccp_line_unlock(line);
	}									// if(channel)
 DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "set singleLineState to %d\n", hint->currentState);
	sccp_mutex_unlock(&hint->lock);
}

/*!
 * \brief check if we can set DND status
 * On shared line we will send dnd status if all devices in dnd only.
 * single line signaling dnd if device is in dnd
 * 
 * \param hint	SCCP Hint Linked List Pointer (locked)
 * \param line	SCCP Line (locked)
 */
static void sccp_hint_checkForDND(sccp_hint_list_t * hint, sccp_line_t * line)
{
	sccp_linedevices_t *lineDevice;

	if (line->devices.size > 1) {
		/* we have to check if all devices on this line are dnd=SCCP_DNDMODE_REJECT, otherwise do not propagate DND status */
		boolean_t allDevicesInDND = TRUE;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
			if (lineDevice->device->dndFeature.status != SCCP_DNDMODE_REJECT) {
				allDevicesInDND = FALSE;
				break;
			}
		}
		SCCP_LIST_UNLOCK(&line->devices);

		if (allDevicesInDND) {
			hint->currentState = SCCP_CHANNELSTATE_DND;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.calledPartyName));
		} else {
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
		}

	} else {
		sccp_linedevices_t *lineDevice = SCCP_LIST_FIRST(&line->devices);

		if (lineDevice) {
			if (lineDevice->device->dndFeature.enabled && lineDevice->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				hint->currentState = SCCP_CHANNELSTATE_DND;
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.calledPartyName));
			} else {
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			}
		} else {
			/* no dnd -> on hook */
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
		}
	}									// if(line->devices.size > 1)
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
void sccp_hint_subscribeHint(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice)
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
	memset(subscriber, 0, sizeof(sccp_hint_SubscribingDevice_t));

	subscriber->device = device;
	subscriber->instance = instance;
	subscriber->positionOnDevice = positionOnDevice;

	SCCP_LIST_INSERT_HEAD(&hint->subscribers, subscriber, list);

	sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
	sccp_hint_notifySubscribers(hint);
}

/*!
 * \brief Unsubscribe from a Hint
 * \param device SCCP Device
 * \param hintStr Hint String (Extension at Context) as char
 * \param instance Instance as int
 * 
 * \lock
 * 	- sccp_hint_subscriptions
 * 	- hint->subscribers
 */
void sccp_hint_unSubscribeHint(const sccp_device_t * device, const char *hintStr, uint8_t instance)
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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Remove device %s from hint %s for exten: %s and context: %s\n", DEV_ID_LOG(device), hintStr, hint_exten, hint_context);

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (strlen(hint_exten) == strlen(hint->exten)
		    && strlen(hint_context) == strlen(hint->context)
		    && !strcmp(hint_exten, hint->exten)
		    && !strcmp(hint_context, hint->context)) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);

	if (!hint)
		return;

	sccp_hint_SubscribingDevice_t *subscriber;

	/* All subscriptions that have this device should be removed */
	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
		if (subscriber->device == device)
			SCCP_LIST_REMOVE_CURRENT(list);
	}
	SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&hint->subscribers);

}

/*!
 * \brief create a hint structure
 * \param hint_exten Hint Extension as char
 * \param hint_context Hint Context as char
 * \return SCCP Hint Linked List
 */
sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context)
{
	sccp_hint_list_t *hint = NULL;
	char hint_dialplan[256] = "";
	char *splitter;

	if (sccp_strlen_zero(hint_exten))
		return NULL;

	if (sccp_strlen_zero(hint_context))
		hint_context = GLOB(context);

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Create hint for exten: %s context: %s\n", hint_exten, hint_context);

#ifdef CS_AST_HAS_NEW_HINT
	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
#else
	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, hint_context, hint_exten);
#endif										// CS_AST_HAS_NEW_HINT

	if (sccp_strlen_zero(hint_dialplan)) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
		return NULL;
	}

	hint = sccp_malloc(sizeof(sccp_hint_list_t));
	memset(hint, 0, sizeof(sccp_hint_list_t));

	SCCP_LIST_HEAD_INIT(&hint->subscribers);
	sccp_mutex_init(&hint->lock);

	sccp_copy_string(hint->exten, hint_exten, sizeof(hint->exten));
	sccp_copy_string(hint->context, hint_context, sizeof(hint->context));

	sccp_copy_string(hint->hint_dialplan, hint_dialplan, sizeof(hint_dialplan));

	/* check if we have an internal hint or have to use asterisk hint system */
	if (strchr(hint_dialplan, '&') || strncasecmp(hint_dialplan, "SCCP", 4)) {
		/* asterisk style hint system */
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Configuring asterisk (no sccp features) hint %s for exten: %s and context: %s\n", hint_dialplan, hint_exten, hint_context);

		hint->hintType = ASTERISK;
		hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
		hint->type.asterisk.hintid = pbx_extension_state_add(hint_context, hint_exten, sccp_hint_state, hint);
#if 0
		hint->type.asterisk.device_state_sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_hint_devicestate_cb, hint, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, strdup(hint->hint_dialplan), AST_EVENT_IE_END);
#endif
		if (hint->type.asterisk.hintid > -1) {
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Added hint (ASTERISK), extension %s@%s, device %s\n", hint_exten, hint_context, hint_dialplan);

			int state = ast_extension_state(NULL, hint_context, hint_exten);

			sccp_hint_state(hint_context, hint_exten, state, hint);

		} else {
			/* error */
			sccp_free(hint);
			pbx_log(LOG_ERROR, "Error adding hint (ASTERISK) for extension %s@%s and device %s\n", hint_exten, hint_context, hint_dialplan);
			return NULL;
		}
	} else {
		/* SCCP channels hint system. Push */
		hint->hintType = INTERNAL;
		char lineName[256];

		/* what line do we have */
		splitter = hint_dialplan;
		strsep(&splitter, "/");
		sccp_copy_string(lineName, splitter, sizeof(lineName));
		pbx_strip(lineName);

		/* save lineName */
		sccp_copy_string(hint->type.internal.lineName, lineName, sizeof(hint->type.internal.lineName));

		/* set initial state */
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;

		sccp_line_t *line = sccp_line_find_byname(lineName);

		if (!line) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "Error adding hint (SCCP) for line: %s. The line does not exist!\n", hint_dialplan);
		} else {
			sccp_hint_hintStatusUpdate(hint);
		}
	}

	return hint;
}

/*!
 * \brief thread for searching callinfo on none sccp channels
 * \param data Data
 * 
 * \deprecated
 * \lock
 * 	- asterisk channel
 * 	  - see sccp_hint_notifySubscribers()
 */
static void *sccp_hint_remoteNotification_thread(void *data)
{
	//! \todo implement in pbx wrapper as getCallInfo(char *channelName, callInfo *callinfo)

//      sccp_hint_list_t *hint = data;
//
//      PBX_CHANNEL_TYPE *astChannel = NULL;
//      PBX_CHANNEL_TYPE *bridgedChannel = NULL;
//      PBX_CHANNEL_TYPE *foundChannel = NULL;
//      uint i = 0;
//
//      if (!hint)
//              goto CLEANUP;
//
//      sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "searching for callInfos for asterisk channel %s\n", hint->hint_dialplan ? hint->hint_dialplan : "null");
//      while ((astChannel = pbx_channel_walk_locked(astChannel)) != NULL) {
//              sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) searching for channel on %s\n", hint->hint_dialplan);
//              sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) asterisk channels %s, cid_num: %s, cid_name: %s\n", astChannel->name, astChannel->cid.cid_num, astChannel->cid.cid_name);
//              if (strlen(astChannel->name) > strlen(hint->hint_dialplan) && !strncmp(astChannel->name, hint->hint_dialplan, strlen(hint->hint_dialplan))) {
//                      foundChannel = astChannel;
//                      break;
//              }
//              pbx_channel_unlock(astChannel);
//      }
//
//      if (foundChannel) {
//              sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) found remote channel %s\n", foundChannel->name);
//              i = 0;
//              while (((bridgedChannel = ast_bridged_channel(foundChannel)) == NULL) && (i < 30)) {
//                      sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) waiting for bridge dChannel\n");
//                      AST_CHANNEL_DEADLOCK_AVOIDANCE(foundChannel);
//                      sleep(1);
//                      i++;
//              }
//
//              if (bridgedChannel) {
//                      sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) asterisk bridgedChannel %s, cid_num: %s, cid_name: %s\n", bridgedChannel->name, bridgedChannel->cid.cid_num, bridgedChannel->cid.cid_name);
//                      sccp_copy_string(hint->callInfo.callingParty, foundChannel->cid.cid_num, sizeof(hint->callInfo.callingParty));
//                      if (foundChannel->cid.cid_name)
//                              sccp_copy_string(hint->callInfo.callingPartyName, foundChannel->cid.cid_name, sizeof(hint->callInfo.callingPartyName));
//                      else
//                              sccp_copy_string(hint->callInfo.callingPartyName, foundChannel->cid.cid_num, sizeof(hint->callInfo.callingPartyName));
//
//                      sccp_copy_string(hint->callInfo.calledPartyName, bridgedChannel->cid.cid_name, sizeof(hint->callInfo.calledPartyName));
//                      sccp_copy_string(hint->callInfo.calledParty, bridgedChannel->cid.cid_num, sizeof(hint->callInfo.calledParty));
//                      sccp_hint_notifySubscribers(hint);
//
//              } else
//                      sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_state) no bridgedChannel channels for %s\n", foundChannel->name);
//              pbx_channel_unlock(foundChannel);
//      }
//
// CLEANUP:
//      if (hint)
//              hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
	return NULL;
}

/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
void sccp_hint_handleFeatureChangeEvent(const sccp_event_t ** event)
{
	sccp_buttonconfig_t *buttonconfig;
	sccp_device_t *d;
	sccp_line_t *line = NULL;

	switch ((*event)->event.featureChanged.featureType) {
	case SCCP_FEATURE_DND:
		d = (*event)->event.featureChanged.device;

		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			if (buttonconfig->type == LINE) {
				line = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
				if (line) {
					sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dndFeature.status ? "on" : "off", line->name);
					if (d->dndFeature.status == SCCP_DNDMODE_REJECT) {
						sccp_hint_lineStatusChanged(line, d, NULL, SCCP_CHANNELSTATE_ONHOOK, SCCP_CHANNELSTATE_DND);
					} else {
						sccp_hint_lineStatusChanged(line, d, NULL, SCCP_CHANNELSTATE_DND, SCCP_CHANNELSTATE_ONHOOK);
					}
				}
			}
		}
		break;
	default:
		break;
	}
}
