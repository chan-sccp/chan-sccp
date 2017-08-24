/*!
 * \file        sccp_devstate.c
 * \brief       SCCP device state Class
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2013-08-15
 */

#include "config.h"
#include "common.h"
#include "sccp_device.h"
#include "sccp_devstate.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

#if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H)							// ast_event_subscribe
#  include <asterisk/event.h>
#endif

#if CS_DEVSTATE_FEATURE
typedef struct sccp_devstate_SubscribingDevice sccp_devstate_SubscribingDevice_t;

struct sccp_devstate_SubscribingDevice 
{
	SCCP_LIST_ENTRY (sccp_devstate_SubscribingDevice_t) list;
	sccp_device_t *device;											/*!< SCCP Device */
	sccp_buttonconfig_t *buttonConfig;
	char label[StationMaxNameSize];
	uint8_t instance;											/*!< Instance */
};

typedef struct sccp_devstate_deviceState sccp_devstate_deviceState_t;
struct sccp_devstate_deviceState 
{
	SCCP_LIST_HEAD (, sccp_devstate_SubscribingDevice_t) subscribers;
	SCCP_LIST_ENTRY (struct sccp_devstate_deviceState) list;
	char devicestate[StationMaxNameSize];
	PBX_EVENT_SUBSCRIPTION *sub;
	uint32_t featureState;
};

static SCCP_LIST_HEAD (, struct sccp_devstate_deviceState) deviceStates;

void sccp_devstate_deviceRegisterListener(const sccp_event_t * event);
sccp_devstate_deviceState_t *sccp_devstate_createDeviceStateHandler(const char *devstate);
sccp_devstate_deviceState_t *sccp_devstate_getDeviceStateHandler(const char *devstate);

//void sccp_devstate_changed_cb(const struct ast_event *ast_event, void *data);
#if ASTERISK_VERSION_GROUP >= 112
void sccp_devstate_changed_cb(void *data, struct stasis_subscription *sub, struct stasis_message *msg);
#else
void sccp_devstate_changed_cb(const struct ast_event *ast_event, void *data);
#endif
void sccp_devstate_removeSubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_device_t * device);
void sccp_devstate_notifySubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_devstate_SubscribingDevice_t * subscriber);
void sccp_devstate_addSubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_device_t * device, sccp_buttonconfig_t * buttonConfig);

void sccp_devstate_module_start(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Starting devstate system\n");
	SCCP_LIST_HEAD_INIT(&deviceStates);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED, sccp_devstate_deviceRegisterListener, TRUE);
}

void sccp_devstate_module_stop(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Stopping devstate system\n");
	{
		sccp_devstate_deviceState_t *deviceState;
		sccp_devstate_SubscribingDevice_t *subscriber;

		SCCP_LIST_LOCK(&deviceStates);
		while ((deviceState = SCCP_LIST_REMOVE_HEAD(&deviceStates, list))) {
			pbx_event_unsubscribe(deviceState->sub);

			SCCP_LIST_LOCK(&deviceState->subscribers);
			while ((subscriber = SCCP_LIST_REMOVE_HEAD(&deviceState->subscribers, list))) {
				sccp_device_release(&subscriber->device);		/* explicit release */
			}
			SCCP_LIST_UNLOCK(&deviceState->subscribers);
			SCCP_LIST_HEAD_DESTROY(&deviceState->subscribers);
			sccp_free(deviceState);
		}
		SCCP_LIST_UNLOCK(&deviceStates);
	}

	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED, sccp_devstate_deviceRegisterListener);
	SCCP_LIST_HEAD_DESTROY(&deviceStates);
}

static void sccp_devstate_deviceRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	sccp_devstate_deviceState_t *deviceState;

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) device));

	if (d) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == FEATURE && config->button.feature.id == SCCP_FEATURE_DEVSTATE) {
				SCCP_LIST_LOCK(&deviceStates);
				deviceState = sccp_devstate_getDeviceStateHandler(config->button.feature.options);
				if (!deviceState && config->button.feature.options) {
					deviceState = sccp_devstate_createDeviceStateHandler(config->button.feature.options);
				}
				SCCP_LIST_UNLOCK(&deviceStates);

				if (deviceState) {
					sccp_devstate_addSubscriber(deviceState, device, config);
				}
			}
		}
	}
}

static void sccp_devstate_deviceUnRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	sccp_devstate_deviceState_t *deviceState;

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) device));

	if (d) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == FEATURE && config->button.feature.id == SCCP_FEATURE_DEVSTATE) {
				SCCP_LIST_LOCK(&deviceStates);
				deviceState = sccp_devstate_getDeviceStateHandler(config->button.feature.options);
				if (deviceState) {
					sccp_devstate_removeSubscriber(deviceState, device);
				}
				SCCP_LIST_UNLOCK(&deviceStates);
			}
		}
	}
}

void sccp_devstate_deviceRegisterListener(const sccp_event_t * event)
{
	sccp_device_t *device = NULL;

	if (!event) {
		return;
	}
	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->event.deviceRegistered.device;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_devstate_deviceRegisterListener) device registered\n", DEV_ID_LOG(device));
			sccp_devstate_deviceRegistered(device);
			break;
		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->event.deviceRegistered.device;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_devstate_deviceRegisterListener) device unregistered\n", DEV_ID_LOG(device));
			sccp_devstate_deviceUnRegistered(device);
			break;
		default:
			break;
	}
}

sccp_devstate_deviceState_t * __PURE__ sccp_devstate_getDeviceStateHandler(const char *devstate)
{
	if (!devstate) {
		return NULL;
	}

	sccp_devstate_deviceState_t *deviceState = NULL;

	SCCP_LIST_TRAVERSE(&deviceStates, deviceState, list) {
		if (!strncasecmp(devstate, deviceState->devicestate, sizeof(deviceState->devicestate))) {
			break;
		}
	}

	return deviceState;
}

sccp_devstate_deviceState_t *sccp_devstate_createDeviceStateHandler(const char *devstate)
{
	if (!devstate) {
		return NULL;
	}

	sccp_devstate_deviceState_t *deviceState = NULL;
	char buf[256] = "";

	snprintf(buf, 254, "Custom:%s", devstate);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "%s: (sccp_devstate_createDeviceStateHandler) create handler for %s/%s\n", "SCCP", devstate, buf);

	deviceState = sccp_calloc(sizeof *deviceState, 1);
	if (!deviceState) {
		pbx_log(LOG_ERROR, "Memory Allocation for deviceState failed!\n");
		return NULL;
	}
	SCCP_LIST_HEAD_INIT(&deviceState->subscribers);
	sccp_copy_string(deviceState->devicestate, devstate, sizeof(deviceState->devicestate));
#if ASTERISK_VERSION_GROUP >= 112
	struct stasis_topic *devstate_specific_topic = ast_device_state_topic((const char *)buf);
	deviceState->sub = stasis_subscribe(devstate_specific_topic, sccp_devstate_changed_cb, deviceState);
#else
	deviceState->sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, sccp_devstate_changed_cb, "sccp_devstate_changed_cb", deviceState, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, pbx_strdup(buf), AST_EVENT_IE_END);
#endif
	deviceState->featureState = (ast_device_state(buf) == AST_DEVICE_NOT_INUSE) ? 0 : 1;

	SCCP_LIST_INSERT_HEAD(&deviceStates, deviceState, list);
	return deviceState;
}

void sccp_devstate_addSubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_device_t * device, sccp_buttonconfig_t * buttonConfig)
{
	sccp_devstate_SubscribingDevice_t *subscriber;

	subscriber = sccp_calloc(sizeof *subscriber, 1);
	subscriber->device = sccp_device_retain((sccp_device_t *) device);
	subscriber->instance = buttonConfig->instance;
	subscriber->buttonConfig = buttonConfig;
	subscriber->buttonConfig->button.feature.status = deviceState->featureState;
	sccp_copy_string(subscriber->label, buttonConfig->label, sizeof(subscriber->label));

	SCCP_LIST_INSERT_HEAD(&deviceState->subscribers, subscriber, list);
	sccp_devstate_notifySubscriber(deviceState, subscriber);						/* set initial state */
}

void sccp_devstate_removeSubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_device_t * device)
{
	sccp_devstate_SubscribingDevice_t *subscriber = NULL;

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&deviceState->subscribers, subscriber, list) {
		if (subscriber->device == device) {
			SCCP_LIST_REMOVE_CURRENT(list);
			sccp_device_release(&subscriber->device);				/* explicit release */
		}

	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

void sccp_devstate_notifySubscriber(sccp_devstate_deviceState_t * deviceState, const sccp_devstate_SubscribingDevice_t * subscriber)
{
	pbx_assert(subscriber->device != NULL);
	sccp_msg_t *msg = NULL;

	if (subscriber->device->inuseprotocolversion >= 15) {
		REQ(msg, FeatureStatDynamicMessage);
		msg->data.FeatureStatDynamicMessage.lel_featureIndex = htolel(subscriber->instance);
		msg->data.FeatureStatDynamicMessage.lel_featureID = htolel(SKINNY_BUTTONTYPE_FEATURE);
		msg->data.FeatureStatDynamicMessage.lel_featureStatus = htolel(deviceState->featureState);
		sccp_copy_string(msg->data.FeatureStatDynamicMessage.featureTextLabel, subscriber->label, sizeof(msg->data.FeatureStatDynamicMessage.featureTextLabel));
	} else {
		REQ(msg, FeatureStatMessage);
		msg->data.FeatureStatMessage.lel_featureIndex = htolel(subscriber->instance);
		msg->data.FeatureStatMessage.lel_featureID = htolel(SKINNY_BUTTONTYPE_FEATURE);
		msg->data.FeatureStatMessage.lel_featureStatus = htolel(deviceState->featureState);
		sccp_copy_string(msg->data.FeatureStatMessage.featureTextLabel, subscriber->label, sizeof(msg->data.FeatureStatMessage.featureTextLabel));
	}

	sccp_dev_send(subscriber->device, msg);
}

//void sccp_devstate_changed_cb(const struct ast_event *ast_event, void *data)
#if ASTERISK_VERSION_GROUP >= 112
void sccp_devstate_changed_cb(void *data, struct stasis_subscription *sub, struct stasis_message *msg)
#else
void sccp_devstate_changed_cb(const struct ast_event *ast_event, void *data)
#endif
{
	sccp_devstate_deviceState_t *deviceState = NULL;
	sccp_devstate_SubscribingDevice_t *subscriber = NULL;
	enum ast_device_state state;

#if ASTERISK_VERSION_GROUP >= 112
	struct ast_device_state_message *dev_state = stasis_message_data(msg);

	if (ast_device_state_message_type() != stasis_message_type(msg)) {
		return;
	}
	if (dev_state->eid) {											/* ignore non-aggregate states */
		return;
	}
	state = dev_state->state;
#else
	state = pbx_event_get_ie_uint(ast_event, AST_EVENT_IE_STATE);
#endif
	deviceState = (sccp_devstate_deviceState_t *) data;
	deviceState->featureState = (state == AST_DEVICE_NOT_INUSE) ? 0 : 1;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_devstate_changed_cb) got new device state for %s, state: %d, deviceState->subscribers.count %d\n", "SCCP", deviceState->devicestate, state, deviceState->subscribers.size);
	SCCP_LIST_TRAVERSE(&deviceState->subscribers, subscriber, list) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (sccp_devstate_changed_cb) notify subscriber for state %d\n", DEV_ID_LOG(subscriber->device), deviceState->featureState);
		subscriber->buttonConfig->button.feature.status = deviceState->featureState;
		sccp_devstate_notifySubscriber(deviceState, subscriber);
	}
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
