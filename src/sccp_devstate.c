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
SCCP_FILE_VERSION(__FILE__, "");

#if CS_DEVSTATE_FEATURE
#	include "sccp_device.h"
#	include "sccp_devstate.h"
#	include "sccp_utils.h"
#	include <asterisk/devicestate.h>

#	if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H)                                        // ast_event_subscribe
#		include <asterisk/event.h>
#	endif

typedef struct FeatureState feature_state_t;
struct FeatureState {
	union {
		struct FeatureStateValue strct;
		uint32_t lel_uint32;
	} value;
	enum ast_device_state nextstate;
};

typedef struct SubscribingDevice SubscribingDevice_t;
struct SubscribingDevice {
	SCCP_LIST_ENTRY(SubscribingDevice_t) list;
	sccp_device_t *device;											/*!< SCCP Device */
	sccp_buttonconfig_t *buttonConfig;
	char label[StationMaxNameSize];
	feature_state_t states[AST_DEVICE_TOTAL]; /*!< Array of Feature States */
};

typedef struct deviceState deviceState_t;
struct deviceState {
	SCCP_LIST_HEAD(, SubscribingDevice_t) subscribers;
	SCCP_LIST_ENTRY(struct deviceState) list;
	char devicestate[StationMaxNameSize];
	PBX_EVENT_SUBSCRIPTION *sub;
	enum ast_device_state featureState;
};
static SCCP_LIST_HEAD(, struct deviceState) deviceStates;

void deviceRegisterListener(const sccp_event_t * event);
deviceState_t * createDeviceStateHandler(const char * devstate);
deviceState_t * getDeviceStateHandler(const char * devstate);
#	if ASTERISK_VERSION_GROUP >= 112
void changed_cb(void * data, struct stasis_subscription * sub, struct stasis_message * msg);
#	else
void changed_cb(const struct ast_event * ast_event, void * data);
#	endif
void notifySubscriber(deviceState_t * deviceState, const SubscribingDevice_t * subscriber);

const char devstate_db_family[] = "CustomDevstate";
#	define ASTDB_RESULT_LEN 80
static void sccp_devstate_getASTDB(deviceState_t * deviceState)
{
	char buf[ASTDB_RESULT_LEN] = "";
	if(iPbx.feature_getFromDatabase(devstate_db_family, deviceState->devicestate, buf, sizeof(buf)) && !sccp_strlen_zero(buf)) {
		deviceState->featureState = ast_devstate_val(buf);
	}
}
static void sccp_devstate_setASTDB(deviceState_t * deviceState)
{
	if(iPbx.feature_addToDatabase) {
		iPbx.feature_addToDatabase(devstate_db_family, deviceState->devicestate, ast_devstate_str(deviceState->featureState));
	}
}

void sccp_devstate_module_start(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Starting devstate system\n");
	SCCP_LIST_HEAD_INIT(&deviceStates);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_REGISTERED, deviceRegisterListener, TRUE);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_UNREGISTERED, deviceRegisterListener, FALSE);
}

void sccp_devstate_module_stop(void)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Stopping devstate system\n");
	{
		deviceState_t * deviceState = NULL;
		SubscribingDevice_t * subscriber = NULL;

		SCCP_LIST_LOCK(&deviceStates);
		while ((deviceState = SCCP_LIST_REMOVE_HEAD(&deviceStates, list))) {
			if (deviceState->sub) {
				pbx_event_unsubscribe(deviceState->sub);
			}

			SCCP_LIST_LOCK(&deviceState->subscribers);
			while ((subscriber = SCCP_LIST_REMOVE_HEAD(&deviceState->subscribers, list))) {
				sccp_device_release(&subscriber->device);		/* explicit release */
			}
			SCCP_LIST_UNLOCK(&deviceState->subscribers);
			sccp_devstate_setASTDB(deviceState);
			SCCP_LIST_HEAD_DESTROY(&deviceState->subscribers);
			sccp_free(deviceState);
		}
		SCCP_LIST_UNLOCK(&deviceStates);
	}

	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED, deviceRegisterListener);
	SCCP_LIST_HEAD_DESTROY(&deviceStates);
}

/*
static void printStates(feature_state_t * states)
{
	for(uint x = 0; x < AST_DEVICE_TOTAL; x++) {
		feature_state_t state = states[x];
		sccp_log((DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "'%s'(%d): rythm:%d, color:%d, icon:%d, nextstate:%s(%d)\n", ast_devstate2str((enum ast_device_state)x), x, state.value.strct.rythm, state.value.strct.color, state.value.strct.icon,
			ast_devstate2str(state.nextstate), state.nextstate);
	}
}
*/

/*!
 * Parse devstate feature button arguments
 * returns allocated states which need to be freed after use
 */
static void parseButtonArgs(const char * args, feature_state_t * states)
{
	char * _args = pbx_strdupa(args);

	/* split arg string and assigned to states struct */
	char * arg = NULL;
	while((arg = strsep(&_args, "|")) != NULL) {
		unsigned short int state, rythm, color, icon, nextstate;
		if(sscanf(arg, "%1hd%1hd%1hd%1hd%1hd", &state, &rythm, &color, &icon, &nextstate) == 5) {
			states[state].value.strct.rythm = rythm;
			states[state].value.strct.color = color;
			states[state].value.strct.icon = icon;
			states[state].value.strct.oldval = 0;
			states[state].nextstate = (enum ast_device_state)nextstate;
			// sccp_log((DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "SCCP: parseButtonArgs(%p): added: '%s' -> '%s', %d, %d, %d, '%s'\n", (void *)&states[state], arg, ast_devstate2str(state), states[state].value.strct.rythm,
			// states[state].value.strct.color, states[state].value.strct.icon, ast_devstate2str(states[state].nextstate));
		} else {
			pbx_log(LOG_ERROR, "SCCP: (parseButtonArgs) could not parse '%s', failed segment:'%s'\n", args, args);
		}
	}
	// printStates(states);
}

static SubscribingDevice_t * addSubscriber(deviceState_t * deviceState, const sccp_device_t * device, sccp_buttonconfig_t * buttonConfig)
{
	SubscribingDevice_t * subscriber = (SubscribingDevice_t *)sccp_calloc(sizeof *subscriber, 1);
	if(!subscriber) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "devstate::addSubscriber");
		return NULL;
	}
	subscriber->device = sccp_device_retain((sccp_device_t *)device);
	subscriber->buttonConfig = buttonConfig;
	subscriber->buttonConfig->button.feature.status = deviceState->featureState;
	parseButtonArgs(subscriber->buttonConfig->button.feature.args, subscriber->states);
	sccp_copy_string(subscriber->label, buttonConfig->label, sizeof(subscriber->label));

	SCCP_LIST_INSERT_HEAD(&deviceState->subscribers, subscriber, list);
	return subscriber;
}

static void removeSubscriber(deviceState_t * deviceState, const sccp_device_t * device)
{
	SubscribingDevice_t * subscriber = NULL;

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&deviceState->subscribers, subscriber, list) {
		if(subscriber->device == device) {
			SCCP_LIST_REMOVE_CURRENT(list);
			sccp_device_release(&subscriber->device); /* explicit release */
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END
		;
}

static void deviceRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t * config = NULL;
	deviceState_t * deviceState = NULL;
	boolean_t initialize = FALSE;

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) device));

	if (d) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == FEATURE && config->button.feature.id == SCCP_FEATURE_DEVSTATE) {
				char * devStateStr = pbx_strdup (config->button.feature.options);
				if(devStateStr != NULL) {
					SCCP_LIST_LOCK(&deviceStates);
					deviceState = getDeviceStateHandler(devStateStr);
					if(!deviceState && devStateStr) {
						deviceState = createDeviceStateHandler(devStateStr);
						initialize = TRUE;
					}
					SCCP_LIST_UNLOCK(&deviceStates);

					if(deviceState) {
						SubscribingDevice_t * subscriber = addSubscriber(deviceState, device, config);
						if(initialize && deviceState->featureState == AST_DEVICE_UNKNOWN) {
							sccp_devstate_getASTDB(deviceState);
							deviceState->featureState = sccp_devstate_getNextDeviceState(d, config);
						} else {
							sccp_devstate_setASTDB(deviceState);
						}
						notifySubscriber(deviceState, subscriber); /* set initial state */
					}
				}
				sccp_free (devStateStr);
			}
		}
	}
}

static void deviceUnRegistered(const sccp_device_t * device)
{
	sccp_buttonconfig_t * config = NULL;
	deviceState_t * deviceState = NULL;

	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain((sccp_device_t *) device));

	if (d) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == FEATURE && config->button.feature.id == SCCP_FEATURE_DEVSTATE) {
				char * devStateStr = pbx_strdup (config->button.feature.options);
				if(devStateStr != NULL) {
					SCCP_LIST_LOCK(&deviceStates);
					deviceState = getDeviceStateHandler(devStateStr);
					if(deviceState) {
						removeSubscriber(deviceState, device);
					}
					SCCP_LIST_UNLOCK(&deviceStates);
				}
				sccp_free (devStateStr);
			}
		}
	}
}

void deviceRegisterListener(const sccp_event_t * event)
{
	sccp_device_t *device = NULL;

	if (!event) {
		return;
	}
	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->deviceRegistered.device;
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (devstate::deviceRegisterListener) device registered\n", DEV_ID_LOG(device));
			deviceRegistered(device);
			break;
		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->deviceRegistered.device;
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (devstate::deviceRegisterListener) device unregistered\n", DEV_ID_LOG(device));
			deviceUnRegistered(device);
			break;
		default:
			break;
	}
}

deviceState_t * __PURE__ getDeviceStateHandler(const char * devstate)
{
	if (!devstate) {
		return NULL;
	}

	deviceState_t * deviceState = NULL;
	SCCP_LIST_TRAVERSE(&deviceStates, deviceState, list) {
		if (!strncasecmp(devstate, deviceState->devicestate, sizeof(deviceState->devicestate))) {
			break;
		}
	}

	return deviceState;
}

deviceState_t * createDeviceStateHandler(const char * devstate)
{
	if (!devstate) {
		return NULL;
	}

	char buf[256] = "";
	snprintf(buf, 254, "Custom:%s", devstate);
	sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_4 "%s: (devstate::createDeviceStateHandler) create handler for %s/%s\n", "SCCP", devstate, buf);

	deviceState_t * deviceState = (deviceState_t *)sccp_calloc(sizeof *deviceState, 1);
	if (!deviceState) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "devstate::createDeviceStateHandler");
		return NULL;
	}
	SCCP_LIST_HEAD_INIT(&deviceState->subscribers);
	sccp_copy_string(deviceState->devicestate, devstate, sizeof(deviceState->devicestate));
#if CS_AST_HAS_STASIS
	struct stasis_topic *devstate_specific_topic = ast_device_state_topic((const char *)buf);
	if (devstate_specific_topic) {
		deviceState->sub = stasis_subscribe(devstate_specific_topic, changed_cb, deviceState);
	}
#elif CS_AST_HAS_EVENT
	deviceState->sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE_CHANGE, changed_cb, "sccp_devstate_changed_cb", deviceState, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, buf, AST_EVENT_IE_END);
#else
	pbx_log(LOG_ERROR, "SCCP: distributed devstate not supported\n");
#endif
	deviceState->featureState = ast_device_state(buf);

	SCCP_LIST_INSERT_HEAD(&deviceStates, deviceState, list);
	return deviceState;
}

enum ast_device_state sccp_devstate_getNextDeviceState(constDevicePtr d, sccp_buttonconfig_t * config)
{
	enum ast_device_state nextstate = AST_DEVICE_UNKNOWN;
	if(!d || !config) {
		return nextstate;
	}

	SCCP_LIST_LOCK(&deviceStates);
	deviceState_t * deviceState = getDeviceStateHandler(config->button.feature.options);
	SCCP_LIST_UNLOCK(&deviceStates);

	SubscribingDevice_t * subscriber = NULL;

	SCCP_LIST_LOCK(&deviceState->subscribers);
	SCCP_LIST_TRAVERSE(&deviceState->subscribers, subscriber, list) {
		if(subscriber->device == d) {
			nextstate = subscriber->states[deviceState->featureState].nextstate;
			// sccp_log((DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: getNextDeviceState: current:'%s';(%d), next:'%s'(%d)\n", d->id, ast_devstate2str(deviceState->featureState), (int)deviceState->featureState,
			// ast_devstate2str(nextstate),*(int)nextstate);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&deviceState->subscribers);
	return nextstate;
}

void notifySubscriber(deviceState_t * deviceState, const SubscribingDevice_t * subscriber)
{
	pbx_assert(subscriber != NULL && subscriber->device != NULL);
	sccp_msg_t *msg = NULL;

	const feature_state_t * curstate = &subscriber->states[deviceState->featureState];
	if (subscriber->device->inuseprotocolversion >= 15) {
		REQ(msg, FeatureStatDynamicMessage);
		if (!msg) {
			return;
		}
		msg->data.FeatureStatDynamicMessage.lel_lineInstance = htolel(subscriber->buttonConfig->instance);
		msg->data.FeatureStatDynamicMessage.lel_buttonType = htolel(SKINNY_BUTTONTYPE_MULTIBLINKFEATURE);
		msg->data.FeatureStatDynamicMessage.stateVal.strct = curstate->value.strct;
		sccp_copy_string(msg->data.FeatureStatDynamicMessage.textLabel, subscriber->label, sizeof(msg->data.FeatureStatDynamicMessage.textLabel));
	} else {
		REQ(msg, FeatureStatMessage);
		if (!msg) {
			return;
		}
		msg->data.FeatureStatMessage.lel_lineInstance = htolel(subscriber->buttonConfig->instance);
		msg->data.FeatureStatMessage.lel_buttonType = htolel(SKINNY_BUTTONTYPE_FEATURE);
		//msg->data.FeatureStatMessage.lel_stateValue = htolel((*(int *)&curstate->value) ? 1 : 0);
		msg->data.FeatureStatMessage.lel_stateValue = htolel(curstate->value.lel_uint32);
		sccp_copy_string(msg->data.FeatureStatMessage.textLabel, subscriber->label, sizeof(msg->data.FeatureStatMessage.textLabel));
	}
	sccp_dev_send(subscriber->device, msg);
}

// void changed_cb(const struct ast_event *ast_event, void *data)
#	if ASTERISK_VERSION_GROUP >= 112
void changed_cb(void * data, struct stasis_subscription * sub, struct stasis_message * msg)
#	else
void changed_cb(const struct ast_event * ast_event, void * data)
#	endif
{
	deviceState_t * deviceState = (deviceState_t *)data;
	SubscribingDevice_t * subscriber = NULL;
	enum ast_device_state newState = AST_DEVICE_UNKNOWN;

#	if ASTERISK_VERSION_GROUP >= 112
	struct ast_device_state_message *dev_state = (struct ast_device_state_message *)stasis_message_data(msg);
	if(ast_device_state_message_type() != stasis_message_type(msg) || !dev_state->eid) { /* ignore wrong message type or non-aggregate states */
		return;
	}
	newState = dev_state->state;
#	else
	newState = (enum ast_device_state)pbx_event_get_ie_uint(ast_event, AST_EVENT_IE_STATE);
#	endif
	if(deviceState) {
		deviceState->featureState = newState;
		SCCP_LIST_TRAVERSE(&deviceState->subscribers, subscriber, list) {
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: (devstate::changed_cb) notify subscriber of state:'%s'(%d) change\n", DEV_ID_LOG(subscriber->device), ast_devstate2str(deviceState->featureState),
						  deviceState->featureState);
			subscriber->buttonConfig->button.feature.status = (uint32_t)deviceState->featureState;
			notifySubscriber(deviceState, subscriber);
		}
		sccp_devstate_setASTDB(deviceState);
	}
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
