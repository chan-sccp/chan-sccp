
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
static void sccp_hint_checkForDND(sccp_hint_list_t * hint, sccp_line_t * line);
sccp_hint_list_t *sccp_hint_create(char *hint_context, char *hint_exten);
void sccp_hint_subscribeHint(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice);
void sccp_hint_unSubscribeHint(const sccp_device_t * device, const char *hintStr, uint8_t instance);
void sccp_hint_eventListener(const sccp_event_t * event);
void sccp_hint_deviceRegistered(const sccp_device_t * device);
void sccp_hint_deviceUnRegistered(const char *deviceName);
void sccp_hint_notifySubscribers(sccp_hint_list_t * hint, boolean_t force);
void sccp_hint_hintStatusUpdate(sccp_hint_list_t * hint);
void sccp_hint_notificationForSharedLine(sccp_hint_list_t * hint);
void sccp_hint_notificationForSingleLine(sccp_hint_list_t * hint);
void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event);
boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice);

SCCP_LIST_HEAD (, sccp_hint_list_t) sccp_hint_subscriptions;

/* ================================================================================================================================ */
/*!
 * \brief starting hint-module
 */
void sccp_hint_module_start()
{
	/* */
	SCCP_LIST_HEAD_INIT(&sccp_hint_subscriptions);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_FEATURE_CHANGED, sccp_hint_eventListener, TRUE);
}

/*!
 * \brief stop hint-module
 *
 * \lock
 *      - sccp_hint_subscriptions
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
			subscriber->device = subscriber->device ? sccp_device_release(subscriber->device) : NULL;
			sccp_free(subscriber);
		}
		sccp_free(hint);
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_FEATURE_CHANGED, sccp_hint_eventListener);
}

/* ================================================================================================================================ SCCP EVENT Listener */
/*!
 * \brief Event Listener for Hints
 * \param event SCCP Event
 * 
 * \expects_ref
 *      - SCCP_EVENT_DEVICE_REGISTERED
 *              - device
 *      - SCCP_EVENT_DEVICE_UNREGISTERED
 *              - device
 *      - SCCP_EVENT_DEVICE_ATTACHED
 *              - device
 *              - line
 *      - SCCP_EVENT_DEVICE_DETACHED
 *              - device
 *              - line
 *      - SCCP_EVENT_LINESTATUS_CHANGED
 *              - device
 *              - line
 *      - SCCP_EVENT_FEATURE_CHANGED
 *              - device
 */
void sccp_hint_eventListener(const sccp_event_t * event)
{
	sccp_device_t *device;

	if (!event)
		return;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_1 "SCCP: (sccp_hint_eventListener) handling event %d\n", event->type);

	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			// subscribe to the hints used on this device   
			sccp_hint_deviceRegistered(event->event.deviceRegistered.device);
			break;
		case SCCP_EVENT_DEVICE_UNREGISTERED:
			// unsubscribe from the hints used on this device       
			device = event->event.deviceRegistered.device;
			if (device && device->id) {
				char *deviceName = strdupa(device->id);

				sccp_hint_deviceUnRegistered(deviceName);
			}
			break;
		case SCCP_EVENT_DEVICE_ATTACHED:
			// update initial state to onhook when the line is registered on a device       
			sccp_hint_lineStatusChanged(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device, SCCP_CHANNELSTATE_ONHOOK);
			break;
		case SCCP_EVENT_DEVICE_DETACHED:
			// switch line status for this particulat device to zombie
			sccp_hint_lineStatusChanged(event->event.deviceAttached.linedevice->line, event->event.deviceAttached.linedevice->device, SCCP_CHANNELSTATE_ZOMBIE);
			break;
		case SCCP_EVENT_LINESTATUS_CHANGED:
			// update hint status for every line-state change (sccp_indication)
			sccp_hint_lineStatusChanged(event->event.lineStatusChanged.line, event->event.lineStatusChanged.device, event->event.lineStatusChanged.state);
			break;
		case SCCP_EVENT_FEATURE_CHANGED:
			// update hint status when a feature changes
			sccp_hint_handleFeatureChangeEvent(event);
			break;
		default:
			// do nothing
			break;
	}
}

/* ================================================================================================================================ SCCP EVENT Handlers */

/*!
 * \brief Handle Hints for Device Register
 * \param device SCCP Device
 * 
 * \expects_ref
 *      - device
 * \warning
 *      - device->buttonconfig is not locked
 */
void sccp_hint_deviceRegistered(const sccp_device_t * device)
{
	sccp_device_t *d = NULL;
	sccp_buttonconfig_t *config;
	uint8_t positionOnDevice = 0;

	if ((d = sccp_device_retain((sccp_device_t *) device))) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "SCCP: (sccp_hint_deviceRegistered) device %s\n", DEV_ID_LOG(device));
		SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
			positionOnDevice++;

			if (config->type == SPEEDDIAL) {
				if (sccp_strlen_zero(config->button.speeddial.hint)) {
					continue;
				}
				sccp_hint_subscribeHint(device, config->button.speeddial.hint, config->instance, positionOnDevice);
			}
		}
		sccp_device_release(d);
	}
}

/*!
 * \brief Handle Hints for Device UnRegister
 * \param deviceName Device as Char
 *
 * \expects_ref
 *      - device
 * \lock
 *        - hint->subscribers
 * \warning
 *      - device->buttonconfig is not locked
 */
void sccp_hint_deviceUnRegistered(const char *deviceName)
{
	sccp_hint_list_t *hint = NULL;
	sccp_hint_SubscribingDevice_t *subscriber;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "SCCP: (sccp_hint_deviceUnRegistered) device %s\n", deviceName);
	//      SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		/* All subscriptions that have this device should be removed */
		SCCP_LIST_LOCK(&hint->subscribers);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
			if (subscriber->device && !strcasecmp(subscriber->device->id, deviceName)) {
				subscriber->device = subscriber->device ? sccp_device_release(subscriber->device) : NULL;
				SCCP_LIST_REMOVE_CURRENT(list);
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
		SCCP_LIST_UNLOCK(&hint->subscribers);
	}
	//      SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

/*!
 * \brief Handle line status change
 * \param line          SCCP Line
 * \param device        SCCP Device
 * \param previousState Previous Channel State
 * \param state         New Channel State
 * \param file          Source File Name as char
 * \param callerLine    Source Line Number as int
 * 
 * \expects_ref
 *      - device
 *      - line
 * \lock
 *      - sccp_hint_subscriptions
 *        - see sccp_hint_hintStatusUpdate()
 */
void sccp_hint_lineStatusChanged(sccp_line_t * line, sccp_device_t * device, sccp_channelState_t state)
{
	sccp_hint_list_t *hint = NULL;

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "SCCP: (sccp_hint_lineStatusChanged): to new state: %s(%d)\n", channelstate2str(state), state);
	if (!line) {
		return;
	}
	/*      if (!device) {
	   return;      
	   } */
	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (strlen(line->name) == strlen(hint->type.internal.lineName)
		    && !strcmp(line->name, hint->type.internal.lineName)) {

			if (hint->currentState != state && hint->previousState != state) {
				/* update hint */
				hint->currentState = state;
				sccp_hint_hintStatusUpdate(hint);
			}
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

	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_hintStatusUpdate) No hint provided\n");
		return;
	}
	if ((line = sccp_line_find_byname(hint->type.internal.lineName))) {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_lineStatusUpdate) hint %s@%s has changed, line %s has %d device%s --> notify %s\n", hint->exten, hint->context, line->name, line->devices.size, (line->devices.size > 1) ? "s" : "", (line->devices.size > 1) ? "shared line change" : "single line change");
		if ((line->devices.size > 1 && line->channels.size > 1) || line->channels.size > 1) {
			/* line is currently shared */
			sccp_hint_notificationForSharedLine(hint);
		} else {
			/* just one device per line */
			sccp_hint_notificationForSingleLine(hint);
		}
		/* notify asterisk */
		sccp_hint_notifySubscribers(hint, FALSE);
		sccp_hint_notifyAsterisk(line, hint->currentState);						// will this not also callback for all subscribers ?

		sccp_line_release(line);
	} else {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_hintStatusUpdate) Could not find line associated to this hint\n");
	}
}

/*!
 * \brief set hint status for a line with more then one channel
 * \param hint  SCCP Hint Linked List Pointer
 *
 * \lock
 *      - hint
 *      - linechannels
 *        - see sccp_line_find_byname_wo()
 */
void sccp_hint_notificationForSharedLine(sccp_hint_list_t * hint)
{
	sccp_line_t *line = NULL;
	sccp_channel_t *channel = NULL;
	boolean_t dev_privacy = FALSE;
	sccp_channelState_t state;

	sccp_mutex_lock(&hint->lock);
	if ((line = sccp_line_find_byname_wo(hint->type.internal.lineName, FALSE))) {
		memset(hint->callInfo.callingPartyName, 0, sizeof(hint->callInfo.callingPartyName));
		memset(hint->callInfo.callingPartyNumber, 0, sizeof(hint->callInfo.callingPartyNumber));
		memset(hint->callInfo.calledPartyName, 0, sizeof(hint->callInfo.calledPartyName));
		memset(hint->callInfo.calledPartyNumber, 0, sizeof(hint->callInfo.calledPartyNumber));
		hint->callInfo.presentation = 0;
		hint->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;

		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSharedLine)\n");

		if (line->devices.size > 0) {
			sccp_device_t *device = NULL;
			sccp_linedevices_t *lineDevice = NULL;
			SCCP_LIST_LOCK(&line->devices);
			lineDevice = SCCP_LIST_FIRST(&line->devices);
			SCCP_LIST_UNLOCK(&line->devices);

			if ((lineDevice = sccp_linedevice_retain(lineDevice))) {
				if ((device = sccp_device_retain(lineDevice->device))) {
					if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
						state = SCCP_CHANNELSTATE_DND;
					}
					dev_privacy = device->privacyFeature.enabled;
					device = device ? sccp_device_release(device) : NULL;
				}
				lineDevice = lineDevice ? sccp_linedevice_release(lineDevice) : NULL;
			}
		}	
		if (line->channels.size > 0) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSharedLine) %s: number of active channels %d\n", line->name, line->statistic.numberOfActiveChannels);
			if (line->channels.size == 1) {
				SCCP_LIST_LOCK(&line->channels);
				channel = SCCP_LIST_FIRST(&line->channels);
				SCCP_LIST_UNLOCK(&line->channels);
				if (channel && (channel = sccp_channel_retain(channel))) {
					state = (SCCP_CHANNELSTATE_DND==state) ? SCCP_CHANNELSTATE_DND : channel->state;
					if (dev_privacy && channel->privacy) {
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine) Suppressing Presentation\n");
						hint->callInfo.presentation = 0;
					} else {
						sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine) Allowing Presentation\n");
						hint->callInfo.presentation = 1;
					}
					hint->callInfo.calltype = channel->calltype;
					if (state != SCCP_CHANNELSTATE_ONHOOK && state != SCCP_CHANNELSTATE_DOWN) {
						hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
						sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
						sccp_copy_string(hint->callInfo.callingPartyNumber, channel->callInfo.callingPartyNumber, sizeof(hint->callInfo.callingPartyNumber));
						sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));	
						sccp_copy_string(hint->callInfo.calledPartyNumber, channel->callInfo.calledPartyNumber, sizeof(hint->callInfo.calledPartyNumber));
					} else {
						hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
						sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
						sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
					}
					channel = sccp_channel_release(channel);
				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
					hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
					goto DONE;
				}
			} else if (line->channels.size > 1) {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			}
		} else {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSharedLine) Number of channel on this shared line is zero\n");
			if (line->devices.size == 0) {
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_CONGESTION;				// CS_DYNAMIC_SPEEDDIAL
			} else {
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
			}
		}
	} else {
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
	}
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notificationForSharedLine) set sharedLineState to (%s) %d\n", line ? line->name : "NULL", channelstate2str(hint->currentState), hint->currentState);
	line = line ? sccp_line_release(line) : NULL;
	sccp_mutex_unlock(&hint->lock);
}

/*!
 * \brief set hint status for a line with less or eq one channel
 * \param hint  SCCP Hint Linked List Pointer
 * 
 * \lock
 *      - hint
 *      - linedevices
 *        - see sccp_line_find_byname_wo()
 */
void sccp_hint_notificationForSingleLine(sccp_hint_list_t * hint)
{
	sccp_line_t *line = NULL;
	sccp_channel_t *channel = NULL;
	boolean_t dev_privacy = FALSE;
	sccp_channelState_t state;

	if (!hint)
		return;

	sccp_mutex_lock(&hint->lock);
	line = sccp_line_find_byname_wo(hint->type.internal.lineName, FALSE);
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine)\n");

	/* no line, or line without devices */
	if (!line || (line && line->devices.size == 0)) {
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		hint->currentState = SCCP_CHANNELSTATE_CONGESTION;
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine) No line or no device associated to line: %s\n", (line) ? line->name : "null");
		goto DONE;
	}

	memset(hint->callInfo.callingPartyName, 0, sizeof(hint->callInfo.callingPartyName));
	memset(hint->callInfo.callingPartyNumber, 0, sizeof(hint->callInfo.callingPartyNumber));
	memset(hint->callInfo.calledPartyName, 0, sizeof(hint->callInfo.calledPartyName));
	memset(hint->callInfo.calledPartyNumber, 0, sizeof(hint->callInfo.calledPartyNumber));
	hint->callInfo.presentation = 0;

	if (line->devices.size > 0) {
		sccp_device_t *device = NULL;
		sccp_linedevices_t *lineDevice = NULL;
		boolean_t dev_privacy = FALSE;
		SCCP_LIST_LOCK(&line->devices);
		lineDevice = SCCP_LIST_FIRST(&line->devices);
		SCCP_LIST_UNLOCK(&line->devices);

		if ((lineDevice = sccp_linedevice_retain(lineDevice))) {
			if ((device = sccp_device_retain(lineDevice->device))) {
				if (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) {
					state = SCCP_CHANNELSTATE_DND;
				}
				dev_privacy = device->privacyFeature.enabled;
				device = device ? sccp_device_release(device) : NULL;
			}
			lineDevice = lineDevice ? sccp_linedevice_release(lineDevice) : NULL;
		}
	}
	SCCP_LIST_LOCK(&line->channels);
	channel = SCCP_LIST_FIRST(&line->channels);
	SCCP_LIST_UNLOCK(&line->channels);
	if (channel && (channel = sccp_channel_retain(channel))) {
		hint->callInfo.calltype = channel->calltype;
		state = (SCCP_CHANNELSTATE_DND==state) ? SCCP_CHANNELSTATE_DND : channel->state;
		if (dev_privacy && channel->privacy) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine) Suppressing Presentation\n");
			hint->callInfo.presentation = 0;
		} else {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notificationForSingleLine) Allowing Presentation\n");
			hint->callInfo.presentation = 1;
		}

		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;					/* not a good idea to set this to channel->currentState -MC */
		switch (state) {
			case SCCP_CHANNELSTATE_SPEEDDIAL:
				break;
			case SCCP_CHANNELSTATE_ONHOOK:
			case SCCP_CHANNELSTATE_DOWN:
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				break;
			case SCCP_CHANNELSTATE_OFFHOOK:
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_DIALING;
				break;
			case SCCP_CHANNELSTATE_DND:
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.calledPartyName));

				hint->currentState = SCCP_CHANNELSTATE_DND;
				break;
			case SCCP_CHANNELSTATE_GETDIGITS:
			case SCCP_CHANNELSTATE_DIALING:
			case SCCP_CHANNELSTATE_DIGITSFOLL:
				hint->currentState = SCCP_CHANNELSTATE_DIALING;
				if (hint->callInfo.presentation) {
					sccp_copy_string(hint->callInfo.callingPartyName, channel->dialedNumber, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.callingPartyNumber, channel->dialedNumber, sizeof(hint->callInfo.callingPartyNumber));
					sccp_copy_string(hint->callInfo.calledPartyName, channel->dialedNumber, sizeof(hint->callInfo.calledPartyName));
					sccp_copy_string(hint->callInfo.calledPartyNumber, channel->dialedNumber, sizeof(hint->callInfo.calledPartyNumber));
				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.calledPartyName));
				}
				break;
			case SCCP_CHANNELSTATE_RINGOUT:
			case SCCP_CHANNELSTATE_RINGING:
				hint->currentState = SCCP_CHANNELSTATE_RINGING;
				if (hint->callInfo.presentation) {
					sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.callingPartyNumber, channel->callInfo.callingPartyNumber, sizeof(hint->callInfo.callingPartyNumber));
					sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));
					sccp_copy_string(hint->callInfo.calledPartyNumber, channel->callInfo.calledPartyNumber, sizeof(hint->callInfo.calledPartyNumber));
				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
				}
				break;

			case SCCP_CHANNELSTATE_CONNECTED:
			case SCCP_CHANNELSTATE_PROCEED:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				if (hint->callInfo.presentation) {
					sccp_copy_string(hint->callInfo.callingPartyName, channel->callInfo.callingPartyName, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.callingPartyNumber, channel->callInfo.callingPartyNumber, sizeof(hint->callInfo.callingPartyNumber));
					sccp_copy_string(hint->callInfo.calledPartyName, channel->callInfo.calledPartyName, sizeof(hint->callInfo.calledPartyName));
					sccp_copy_string(hint->callInfo.calledPartyNumber, channel->callInfo.calledPartyNumber, sizeof(hint->callInfo.calledPartyNumber));
				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_CONNECTED, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_CONNECTED, sizeof(hint->callInfo.calledPartyName));
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
			case SCCP_CHANNELSTATE_CALLPARK:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.calledPartyName));
				break;
			case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			case SCCP_CHANNELSTATE_INVALIDNUMBER:
			case SCCP_CHANNELSTATE_CONGESTION:
			case SCCP_CHANNELSTATE_CALLWAITING:
			case SCCP_CHANNELSTATE_CALLTRANSFER:
			case SCCP_CHANNELSTATE_CALLCONFERENCE:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
			default:
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				break;
		}
		channel = sccp_channel_release(channel);
	} else {
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notificationForSingleLine) No Active Channel for this hint\n", line ? line->name : "NULL");
		sccp_hint_checkForDND(hint, line);
	}													// if(channel)
DONE:
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notificationForSingleLine) set singleLineState to (%s) %d\n", line ? line->name : "NULL", channelstate2str(hint->currentState), hint->currentState);
	line = line ? sccp_line_release(line) : NULL;
	sccp_mutex_unlock(&hint->lock);
}

/*!
 * \brief send hint status to subscriber
 * \param hint SCCP Hint Linked List Pointer
 * \param force Skip checking if state has changed and force update
 * 
 * \lock
 *      - hint->subscribers
 */
void sccp_hint_notifySubscribers(sccp_hint_list_t * hint, boolean_t force)
{
	sccp_device_t *d = NULL;
	sccp_hint_SubscribingDevice_t *subscriber = NULL;
	sccp_moo_t *r = NULL;
	uint32_t state;												/* used to fall back to old behavior */
	sccp_speed_t k;

#ifdef CS_DYNAMIC_SPEEDDIAL
	char displayMessage[80];
#endif

	if (!hint) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_notifySubscribers) No hint provided to notifySubscribers about\n");
		return;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifySubscribers) Notify subscriber %s\n", (hint->hint_dialplan) ? hint->hint_dialplan : "null");

	if (!force && hint->currentState == hint->previousState) {
		// nothing changed, skip
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifySubscribers) Old State '%s', New State '%s'. No Change -> Skipping.\n", channelstate2str(hint->previousState), channelstate2str(hint->currentState));
		return;
	}
/*
  == SCCP: (sccp_hint_lineStatusChanged): to new state: OFFHOOK(1)
       > SCCP: (sccp_hint_lineStatusUpdate) hint 98041@hints has changed, line 98041 has 1 device --> notify single line change
       > SCCP: (sccp_hint_notificationForSingleLine)
       > 98041: (sccp_hint_notificationForSingleLine) set singleLineState to (CALLREMOTEMULTILINE) 13
       > SCCP: (sccp_hint_notifySubscribers) Notify subscriber SCCP/98041
       > SEP0024C4446974: (sccp_hint_notifySubscribers) notify subscriber SCCP/98041 of state CALLREMOTEMULTILINE
       > SEP0024C4446974: (sccp_hint_notifySubscribers) set display name for Outbound call to: '�<- 98041'
       > SEP0024C4446974: (sccp_hint_notifySubscribers) notify device: SEP0024C4446974@3 state: 13(2)
       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) notify subscriber SCCP/98041 of state CALLREMOTEMULTILINE
       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) setting callingPartyName: '�       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) setting calledPartyName: '�       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) setting callingParty: ''
       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) setting calledParty: ''
       > SEP001B535CD3D6: (sccp_hint_notifySubscribers) notify device: SEP001B535CD3D6@4 state: 13
       > SCCP: (sccp_hint_notifyAsterisk) notify asterisk to set state to sccp channelstate CALLREMOTEMULTILINE (13) => asterisk: Device is in use (2) on cha
*/                                                 
	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
		if (!subscriber->device) {
			SCCP_LIST_REMOVE_CURRENT(list);
			pbx_log(LOG_ERROR, "SCCP: (sccp_hint_notifySubscribers) Subscriber Device is NULL. Removing and Skipping.\n");
			sccp_free(subscriber);
			continue;
		}
		if (subscriber->device && (d = sccp_device_retain((sccp_device_t *) subscriber->device))) {
			state = hint->currentState;
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) notify subscriber %s of state %s\n", DEV_ID_LOG(d), (hint->hint_dialplan) ? hint->hint_dialplan : "null", channelstate2str(state));
			sccp_dev_speed_find_byindex((sccp_device_t *) d, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL, &k);
#ifdef CS_DYNAMIC_SPEEDDIAL
			sccp_dev_speed_find_byindex((sccp_device_t *) d, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL, &k);
			if (subscriber->device->inuseprotocolversion >= 15) {
				REQ(r, FeatureStatDynamicMessage);
				if (!r) {
					pbx_log(LOG_ERROR, "%s: (sccp_hint_notifySubscribers) Failed to create FeatureStatDynamicMessage Message\n", DEV_ID_LOG(d));
					d = sccp_device_release(d);
					return;
				}
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

					case SCCP_CHANNELSTATE_DND:
						snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
						r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_DND);	/* dnd */
						break;

					case SCCP_CHANNELSTATE_CONGESTION:
						snprintf(displayMessage, sizeof(displayMessage), k.name, sizeof(displayMessage));
						r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_UNKNOWN);	/* device/line not found */
						break;

					case SCCP_CHANNELSTATE_RINGING:
					default:
						if (sccp_hint_isCIDavailabe(d, subscriber->positionOnDevice) == TRUE) {
							if (hint->callInfo.presentation) {
								switch(hint->callInfo.calltype)	{
									case SKINNY_CALLTYPE_OUTBOUND:
										if (strlen(hint->callInfo.calledPartyName) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s <- %s", hint->callInfo.calledPartyName, k.name);
										} else if (strlen(hint->callInfo.calledPartyNumber) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s <- %s", hint->callInfo.calledPartyNumber, k.name);
										} else {
											snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
										}
										break;
									//! \todo to be implemented
									case SKINNY_CALLTYPE_FORWARD:
	/*									if (strlen(hint->callInfo.callingPartyName) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s -> %s -> %s", hint->callInfo.callingPartyName, hint->callInfo.originalCalledPartyName, k.name);
										} else if (strlen(hint->callInfo.callingParty) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s -> %s -> %s", hint->callInfo.callingPartyNumber, hint->callInfo.originalCalledParty, k.name);
										} else {
											snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
										}
										break;*/
									case SKINNY_CALLTYPE_INBOUND:
										if (strlen(hint->callInfo.callingPartyName) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s -> %s", hint->callInfo.callingPartyName, k.name);
										} else if (strlen(hint->callInfo.callingPartyNumber) > 0) {
											snprintf(displayMessage, sizeof(displayMessage), "%s -> %s", hint->callInfo.callingPartyNumber, k.name);
										} else {
											snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
										}
										break;
								}
							} else {
								snprintf(displayMessage, sizeof(displayMessage), "%s -> %s", SKINNY_DISP_PRIVATE, k.name);
							}
						} else {
							snprintf(displayMessage, sizeof(displayMessage), "%s", k.name);
						}
						if (SCCP_CHANNELSTATE_RINGING == hint->currentState) {
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_ALERTING);	/* ringin */
						} else {
							r->msg.FeatureStatDynamicMessage.lel_status = htolel(SCCP_BLF_STATUS_INUSE);	/* connected */
						}	
						break;
				}

				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) set display name for %s call to: '%s'\n", DEV_ID_LOG(d), hint->callInfo.calltype ? calltype2str(hint->callInfo.calltype) : "NULL", displayMessage);
				sccp_copy_string(r->msg.FeatureStatDynamicMessage.DisplayName, displayMessage, sizeof(r->msg.FeatureStatDynamicMessage.DisplayName));
				sccp_dev_send(d, r);
				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) notify device: %s@%d state: %d(%d)\n", DEV_ID_LOG(d), DEV_ID_LOG(d), subscriber->instance, hint->currentState, r->msg.FeatureStatDynamicMessage.lel_status);
			} else
#endif														// CS_DYNAMIC_SPEEDDIAL
			{
				//                      sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) can not handle dynamic speeddial, fall back to old behavior using state %d\n", DEV_ID_LOG(subscriber->device), state);
				/*
				   With the old hint style we should only use SCCP_CHANNELSTATE_ONHOOK and SCCP_CHANNELSTATE_CALLREMOTEMULTILINE as callstate,
				   otherwise we get a callplane on device -> set all states except onhook to SCCP_CHANNELSTATE_CALLREMOTEMULTILINE -MC
				 */
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
				if (!r) {
					pbx_log(LOG_ERROR, "%s: (sccp_hint_notifySubscribers) Failed to create FeatureStatDynamicMessage Message\n", DEV_ID_LOG(d));
					d = sccp_device_release(d);
					return;
				}

				/* set callInfo */
				sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, hint->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
				sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, hint->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
//				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) setting callingPartyName: '%s'\n", DEV_ID_LOG(d), r->msg.CallInfoMessage.callingPartyName);
//				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) setting calledPartyName: '%s'\n", DEV_ID_LOG(d), r->msg.CallInfoMessage.calledPartyName);

				sccp_copy_string(r->msg.CallInfoMessage.callingParty, hint->callInfo.callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));
				sccp_copy_string(r->msg.CallInfoMessage.calledParty, hint->callInfo.calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));
//				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) setting callingParty: '%s'\n", DEV_ID_LOG(d), r->msg.CallInfoMessage.callingParty);
//				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) setting calledParty: '%s'\n", DEV_ID_LOG(d), r->msg.CallInfoMessage.calledParty);

				r->msg.CallInfoMessage.lel_lineId = htolel(subscriber->instance);
				r->msg.CallInfoMessage.lel_callRef = htolel(0);
				r->msg.CallInfoMessage.lel_callType = htolel(hint->callInfo.calltype);
				sccp_dev_send(subscriber->device, r);
				sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_notifySubscribers) notify device: %s@%d state: %d\n", DEV_ID_LOG(d), DEV_ID_LOG(d), subscriber->instance, hint->currentState);

				if (hint->currentState == SCCP_CHANNELSTATE_ONHOOK) {
					sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
				} else {
					sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_INUSEHINT);
				}
			}
			d = sccp_device_release(d);
		} else {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifySubscribers) No subscriber->device to send hint update to.\n");
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&hint->subscribers);

	hint->previousState = hint->currentState;
}

/*!
 * \brief Notify Asterisk of Hint State Change
 * \param line  SCCP Line
 * \param state SCCP Channel State
 */
void sccp_hint_notifyAsterisk(sccp_line_t * line, sccp_channelState_t state)
{
	if (!line)
		return;

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_notifyAsterisk) notify asterisk to set state to sccp channelstate %s (%d) => asterisk: %s (%d) on channel SCCP/%s\n", channelstate2str(state), state, pbxdevicestate2str(sccp_channelState2AstDeviceState(state)), sccp_channelState2AstDeviceState(state), line->name);
#ifdef CS_NEW_DEVICESTATE
	pbx_devstate_changed(sccp_channelState2AstDeviceState(state), "SCCP/%s", line->name);
#else
	pbx_device_state_changed("SCCP/%s", line->name);
#endif														// CS_NEW_DEVICESTATE
}

/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 * 
 * \lock
 *      - d->buttonconfig
 * \warning
 *      - device->buttonconfig is not always locked
 */
void sccp_hint_handleFeatureChangeEvent(const sccp_event_t * event)
{
	sccp_buttonconfig_t *buttonconfig;
	sccp_device_t *d;
	sccp_line_t *line = NULL;

	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_handleFeatureChangeEvent) featureType: %d\n", event->event.featureChanged.featureType);
	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_DND:
			if ((d = sccp_device_retain(event->event.featureChanged.device))) {
				SCCP_LIST_LOCK(&d->buttonconfig);
				SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
					if (buttonconfig->type == LINE) {
						if ((line = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE))) {
							sccp_log((DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: (sccp_hint_handleFeatureChangeEvent) Notify the dnd status (%s) to asterisk for line %s\n", DEV_ID_LOG(d), d->dndFeature.status ? "on" : "off", line->name);
							if (d->dndFeature.status == SCCP_DNDMODE_REJECT) {
								sccp_hint_lineStatusChanged(line, d, SCCP_CHANNELSTATE_DND);
							} else {
								sccp_hint_lineStatusChanged(line, d, SCCP_CHANNELSTATE_DOWN);
							}
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

/* ================================================================================================================================ PBX Hint EVENT Handler */
#if ASTERISK_VERSION_NUMBER >= 11200
/*!
 * \brief Asterisk Hint Wrapper
 * \param context Context as char
 * \param exten Extension as char
 * \param info Asterisk State Info (Contains more than just the state)
 * \param data Asterisk Misc Data (Pointer to Hint)
 * 
 * \called_from_asterisk
 */
int sccp_hint_state(char *context, char *exten, struct ast_state_cb_info *info, void *data)
#elif ASTERISK_VERSION_NUMBER >= 11001
/*!
 * \brief Asterisk Hint Wrapper
 * \param context Context as char
 * \param exten Extension as char
 * \param state Asterisk Extension State
 * \param data Asterisk Misc Data (Pointer to Hint)
 * 
 * \called_from_asterisk
 */
int sccp_hint_state(const char *context, const char *exten, enum ast_extension_states state, void *data)
#else
/*!
 * \brief Asterisk Hint Wrapper
 * \param context Context as char
 * \param exten Extension as char
 * \param state Asterisk Extension State
 * \param data Asterisk Misc Data (Pointer to Hint)
 * 
 * \called_from_asterisk
 */
int sccp_hint_state(char *context, char *exten, enum ast_extension_states state, void *data)
#endif
{
	sccp_hint_list_t *hint = (sccp_hint_list_t *) data;

#if ASTERISK_VERSION_NUMBER >= 11200
	enum ast_extension_states state = info->exten_state;

	/* other available info in asterisk 11, could potentially be used to transmit callinfo to hinted speeddials */
	/*
	   info->reason;
	   info->device_state_info;
	   info->presence_state;
	   info->presence_subtype;
	   info->presence_message;
	 */
#endif

	if (state == -1 || !hint) {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_state) Got new hint, but no hint param\n");
		return 0;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_2 "SCCP: (sccp_hint_state) get new hint state %s(%d) for %s\n", extensionstatus2str(state), state, hint->hint_dialplan);
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
#endif														// CS_AST_HAS_EXTENSION_ONHOLD
#ifdef CS_AST_HAS_EXTENSION_RINGING
		case AST_EXTENSION_RINGING:
			hint->currentState = SCCP_CHANNELSTATE_RINGING;

			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
			break;
#endif														// CS_AST_HAS_EXTENSION_RINGING
		default:
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_state) Unmapped hint state %d for %s\n", state, hint->hint_dialplan);
			hint->currentState = SCCP_CHANNELSTATE_DOWN;

			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
	}
	/* push to subscribers */
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_state) Notifying Subscribers for %s\n", hint->hint_dialplan);
	sccp_hint_notifySubscribers(hint, FALSE);								//! \todo check if called twice per notification

	return 0;
}

/* ================================================================================================================================ PBX Hint Subscribe/Unsubscribe */

/*!
 * \brief Subscribe to a Hint
 * \param device SCCP Device
 * \param hintStr Asterisk Hint Name as char
 * \param instance Instance as int
 * \param positionOnDevice button index on device (used to detect devicetype)
 * 
 * \lock
 *      - sccp_hint_subscriptions
 *      - hint->subscribers
 *
 * \note
 *      - added retained device to subscriber->device
 */
void sccp_hint_subscribeHint(const sccp_device_t * device, const char *hintStr, const uint8_t instance, const uint8_t positionOnDevice)
{
	sccp_hint_list_t *hint = NULL;

	char buffer[256] = "";
	char *splitter, *hint_exten, *hint_context;

	sccp_copy_string(buffer, hintStr, sizeof(buffer));

	if (!device) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_subscribeHint) adding hint to: %s without device is not allowed\n", hintStr);
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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "%s: (sccp_hint_subscribeHint) Dialplan %s for exten: %s and context: %s\n", DEV_ID_LOG(device), hintStr, hint_exten, hint_context);

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list) {
		if (sccp_strlen(hint_exten) == sccp_strlen(hint->exten)
		    && sccp_strlen(hint_context) == sccp_strlen(hint->context)
		    && sccp_strequals(hint_exten, hint->exten)
		    && sccp_strequals(hint_context, hint->context)) {
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_subscribeHint) Hint found\n", DEV_ID_LOG(device));
			break;
		}
	}
	/* we have no hint, add a new one */
	if (!hint) {
		hint = sccp_hint_create(hint_exten, hint_context);
		if (!hint)
			return;
		SCCP_LIST_INSERT_HEAD(&sccp_hint_subscriptions, hint, list);
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);

	/* add subscribing device */
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "%s: (sccp_hint_subscribeHint) Create new subscriber\n", DEV_ID_LOG(device));
	sccp_hint_SubscribingDevice_t *subscriber;

	subscriber = sccp_malloc(sizeof(sccp_hint_SubscribingDevice_t));
	memset(subscriber, 0, sizeof(sccp_hint_SubscribingDevice_t));

	if ((subscriber->device = sccp_device_retain((sccp_device_t *) device))) {				// add retained device, release in unSubscribe
		subscriber->instance = instance;
		subscriber->positionOnDevice = positionOnDevice;
	} else {
		pbx_log(LOG_ERROR, "SCCP: (sccp_hint_subscribeHint) Device could not be retained\n");
	}

	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_INSERT_HEAD(&hint->subscribers, subscriber, list);
	SCCP_LIST_UNLOCK(&hint->subscribers);
	//      sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);               // Is this necessary here ?
	sccp_hint_notifySubscribers(hint, TRUE);								// forcing update, event if channelstate remained the same
}

/*!
 * \brief Unsubscribe from a Hint
 * \param device SCCP Device
 * \param hintStr Hint String (Extension at Context) as char
 * \param instance Instance as int
 * 
 * \lock
 *      - sccp_hint_subscriptions
 *      - hint->subscribers
 *
 * \note
 *      - releases retained subscriber->device
 */
void sccp_hint_unSubscribeHint(const sccp_device_t * device, const char *hintStr, uint8_t instance)
{
	sccp_hint_list_t *hint = NULL;
	sccp_device_t *d = NULL;
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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_unSubscribeHint) Remove device %s from hint %s for exten: %s and context: %s\n", DEV_ID_LOG(device), hintStr, hint_exten, hint_context);
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

	if (hint) {
		if ((d = sccp_device_retain((sccp_device_t *) device))) {
			sccp_hint_SubscribingDevice_t *subscriber;

			/* All subscriptions that have this device should be removed */
			SCCP_LIST_LOCK(&hint->subscribers);
			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&hint->subscribers, subscriber, list) {
				if (subscriber->device == device) {
					SCCP_LIST_REMOVE_CURRENT(list);
					subscriber->device = sccp_device_release(subscriber->device);		// release subscriber->device
				}
			}
			SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&hint->subscribers);

			d = sccp_device_release(d);
		} else {
			pbx_log(LOG_ERROR, "SCCP: (sccp_hint_unSubscribeHint) Device could not be retained\n");
		}
	} else {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_unSubscribeHint)	hint could not be found, potentially loosing refcount for subscriber->device\n");
	}
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

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_create) Create hint for exten: %s context: %s\n", hint_exten, hint_context);

#ifdef CS_AST_HAS_NEW_HINT
	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
#else
	pbx_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, hint_context, hint_exten);
#endif														// CS_AST_HAS_NEW_HINT

	if (sccp_strlen_zero(hint_dialplan)) {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_create) No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
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
		sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_create) Configuring asterisk (no sccp features) hint %s for exten: %s and context: %s\n", hint_dialplan, hint_exten, hint_context);

		hint->hintType = ASTERISK;
		hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
		hint->type.asterisk.hintid = pbx_extension_state_add(hint_context, hint_exten, sccp_hint_state, hint);
#if 0
		hint->type.asterisk.device_state_sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_hint_devicestate_cb, hint, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, strdup(hint->hint_dialplan), AST_EVENT_IE_END);
#endif
		if (hint->type.asterisk.hintid > -1) {
			//                      hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			hint->currentState = SCCP_CHANNELSTATE_CONGESTION;
			sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_3 "SCCP: (sccp_hint_create) Added hint (ASTERISK), extension %s@%s, device %s\n", hint_exten, hint_context, hint_dialplan);

			int state = pbx_extension_state(NULL, hint_context, hint_exten);

#if ASTERISK_VERSION_NUMBER >= 11200
			struct ast_state_cb_info *info = { 0 };
			info->exten_state = state;
			sccp_hint_state(hint_context, hint_exten, info, hint);
#else
			sccp_hint_state(hint_context, hint_exten, state, hint);
#endif
		} else {
			/* error */
			sccp_free(hint);
			pbx_log(LOG_ERROR, "SCCP: (sccp_hint_create) Error adding hint (ASTERISK) for extension %s@%s and device %s\n", hint_exten, hint_context, hint_dialplan);
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
		//              hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
		hint->currentState = SCCP_CHANNELSTATE_CONGESTION;

		sccp_line_t *line = NULL;

		if ((line = sccp_line_find_byname(lineName))) {
			sccp_hint_hintStatusUpdate(hint);
			line = sccp_line_release(line);
		} else {
			pbx_log(LOG_WARNING, "SCCP: (sccp_hint_create) Error adding hint (SCCP) for line: %s. The line does not exist!\n", hint_dialplan);
		}
	}

	return hint;
}

/* ================================================================================================================================ Helper Functions */

#ifdef CS_DYNAMIC_SPEEDDIAL
/*! 
 * \brief can we us cid for device?
 */
boolean_t sccp_hint_isCIDavailabe(const sccp_device_t * device, const uint8_t positionOnDevice)
{
	if ((device->skinny_type == SKINNY_DEVICETYPE_CISCO7962 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7971 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || device->skinny_type == SKINNY_DEVICETYPE_CISCO7985)
	    && positionOnDevice <= 8)
		return TRUE;

	return FALSE;
}
#endif														// CS_DYNAMIC_SPEEDDIAL


/*!
 * \brief check if we need to set SCCP_CHANNELSTATE_DND status on hint
 * On shared line we will send dnd status if all devices in dnd only.
 * single line signaling dnd if device is set to dnd
 * 
 * \param hint  SCCP Hint Linked List Pointer
 * \param line  SCCP Line
 */
static void sccp_hint_checkForDND(sccp_hint_list_t * hint, sccp_line_t * line)
{
	if (!line || !hint) {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_checkForDND) Either no hint or line provided\n");
		return;
	}

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_checkForDND) line: %s\n", line->id);
	if (SCCP_CHANNELSTATE_DND == sccp_line_getDNDChannelState(line)) {
		hint->currentState = SCCP_CHANNELSTATE_DND;
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_DND, sizeof(hint->callInfo.calledPartyName));
	} else {
		hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
		sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.calledPartyName));
	}
}

/* ======================================================================================================================== getlinestate / getdevicestate */
/*!
 * \brief Temporary helper function to return devicestate in PBX format, using chan_sccp.c: sccp_devicestate for now
 * 
 * \param linename Line Name as const char
 * \param deviceId Device ID as const char (not used at this moment)
 */
sccp_channelState_t sccp_hint_getLinestate(const char *linename, const char *deviceId)
{
	sccp_channelState_t state = sccp_devicestate((char *) linename);

	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "(sccp_hint_getLinestate) Returning LineState '%d'\n", state);
	return state;
}
