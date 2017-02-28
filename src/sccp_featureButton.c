/*!
 * \file        sccp_featureButton.c
 * \brief       SCCP FeatureButton Class
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-06-15
 *
 */

/*!
 * \remarks
 * Purpose:     SCCP FeatureButton
 * When to use: Only methods directly related to the phone featureButtons should be stored in this source file.
 *              FeatureButtons are the ones at the bottom of the display, not to be confused with speeddial
 *              buttons on the right side of the display.
 * Relations:   Call SCCP Features
 */

#include "config.h"
#include "common.h"
#include "sccp_device.h"
#include "sccp_featureButton.h"
#include "sccp_line.h"
#include "sccp_utils.h"

#ifdef CS_DEVSTATE_FEATURE
#include "sccp_devstate.h"
#endif

SCCP_FILE_VERSION(__FILE__, "");

#if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H) 	// ast_event_subscribe
#  include <asterisk/event.h>
#endif

/*!
 * \brief Feature Button Changed
 *
 * fetch the new state, and send status to device
 *
 * \param device SCCP Device
 * \param featureType SCCP Feature Type
 * 
 * \warning
 *  - device->buttonconfig is not always locked
 */
void sccp_featButton_changed(constDevicePtr device, sccp_feature_type_t featureType)
{
	sccp_msg_t *msg = NULL;
	sccp_buttonconfig_t *config = NULL, *buttonconfig = NULL;
	uint8_t instance = 0;
	uint8_t buttonID = SKINNY_BUTTONTYPE_FEATURE;								// Default feature type.
	boolean_t lineFound = FALSE;

	if (!device) {
		return;
	}

	SCCP_LIST_LOCK(&((devicePtr)device)->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == FEATURE && config->button.feature.id == featureType) {
			sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) FeatureID = %d, Option: %s\n", DEV_ID_LOG(device), config->button.feature.id, (config->button.feature.options) ? config->button.feature.options : "(none)");
			instance = config->instance;

			switch (config->button.feature.id) {
				case SCCP_FEATURE_PRIVACY:
					if (!device->privacyFeature.enabled) {
						config->button.feature.status = 0;
					}

					sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", DEV_ID_LOG(device), device->privacyFeature.status);
					if (sccp_strcaseequals(config->button.feature.options, "callpresent")) {
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

						sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result) ? 1 : 0;
					}
					if (sccp_strcaseequals(config->button.feature.options, "hint")) {
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;

						sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result) ? 1 : 0;
					}
					break;
				case SCCP_FEATURE_CFWDALL:

					// This needs to default to FALSE so that the cfwd feature
					// is not being enabled unless we can ask the lines for their state.
					config->button.feature.status = 0;

					/* get current state */
					SCCP_LIST_TRAVERSE(&device->buttonconfig, buttonconfig, list) {
						if (buttonconfig->type == LINE) {
							// Check if line and line device exists and thus forward status on that device can be checked
							AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(buttonconfig->button.line.name, FALSE));

							if (line) {
								AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(device, line));

								if (linedevice) {
									sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: SCCP_CFWD_ALL on line: %s is %s\n", DEV_ID_LOG(device), line->name, (linedevice->cfwdAll.enabled) ? "on" : "off");

									/* set this button active, only if all lines are fwd -requesting issue #3081549 */
									// Upon finding the first existing line, we need to set the feature status
									// to TRUE and subsequently AND that value with the forward status of each line.
									if (FALSE == lineFound) {
										lineFound = TRUE;
										config->button.feature.status = 1;
									}
									// Set status of feature by logical and to comply with requirement above.
									config->button.feature.status &= ((linedevice->cfwdAll.enabled) ? 1 : 0);	// Logical and &= intended here.
								}
							}
						}
					}
					buttonconfig = NULL;

					break;

				case SCCP_FEATURE_DND:
					if (sccp_strcaseequals(config->button.feature.options, "silent")) {
						// coverity[MIXED_ENUMS]
						if ((device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_SILENT)) {
							config->button.feature.status = 1;
						} else {
							config->button.feature.status = 0;
						}
					} else if (sccp_strcaseequals(config->button.feature.options, "busy")) {
						// coverity[MIXED_ENUMS]
						if ((device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT)) {
							config->button.feature.status = 1;
						} else {
							config->button.feature.status = 0;
						}
					}
					break;
				case SCCP_FEATURE_MONITOR:
					sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) monitor featureButton new state:%s (%d)\n", DEV_ID_LOG(device), sccp_feature_monitor_state2str(device->monitorFeature.status), device->monitorFeature.status);
					if (device->inuseprotocolversion > 15) {				// multiple States
						buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
						switch (device->monitorFeature.status) {
							case SCCP_FEATURE_MONITOR_STATE_DISABLED:
								config->button.feature.status = 0;
								break;
							case SCCP_FEATURE_MONITOR_STATE_REQUESTED:
								config->button.feature.status = 0x020202;
								break;
							case SCCP_FEATURE_MONITOR_STATE_ACTIVE:
								config->button.feature.status = 0x020303;
								break;
							case (SCCP_FEATURE_MONITOR_STATE_REQUESTED | SCCP_FEATURE_MONITOR_STATE_ACTIVE):
								config->button.feature.status = 0x020205;
								break;
						}
					} else {
						switch (device->monitorFeature.status) {
							case SCCP_FEATURE_MONITOR_STATE_DISABLED:
								config->button.feature.status = 0;
								break;
							case SCCP_FEATURE_MONITOR_STATE_REQUESTED:
								if (device->active_channel) {
									config->button.feature.status = 0;
								} else {
									config->button.feature.status = 1;
									break;
								}
								break;
							case SCCP_FEATURE_MONITOR_STATE_ACTIVE:
								config->button.feature.status = 1;
								break;
							case (SCCP_FEATURE_MONITOR_STATE_REQUESTED | SCCP_FEATURE_MONITOR_STATE_ACTIVE):
								config->button.feature.status = 1;
								break;
						}
					}
					break;

#ifdef CS_DEVSTATE_FEATURE

				/**
				  Handling of custom devicestate toggle button feature
				  */
				case SCCP_FEATURE_DEVSTATE:
					/* see sccp_devstate.c */
					break;
#endif

				case SCCP_FEATURE_HOLD:
					buttonID = SKINNY_BUTTONTYPE_HOLD;
					break;

				case SCCP_FEATURE_TRANSFER:
					buttonID = SKINNY_BUTTONTYPE_TRANSFER;
					break;

				case SCCP_FEATURE_MULTIBLINK:
					buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
					config->button.feature.status = device->priFeature.status;
					break;

				case SCCP_FEATURE_MOBILITY:
					buttonID = SKINNY_BUTTONTYPE_MOBILITY;
					config->button.feature.status = device->mobFeature.status;
					break;

				case SCCP_FEATURE_CONFERENCE:
					buttonID = SKINNY_BUTTONTYPE_CONFERENCE;
					break;

				case SCCP_FEATURE_DO_NOT_DISTURB:
					buttonID = SKINNY_BUTTONTYPE_DO_NOT_DISTURB;
					break;

				case SCCP_FEATURE_CONF_LIST:
					buttonID = SKINNY_BUTTONTYPE_CONF_LIST;
					break;

				case SCCP_FEATURE_REMOVE_LAST_PARTICIPANT:
					buttonID = SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT;
					break;

				case SCCP_FEATURE_HLOG:
					buttonID = SKINNY_BUTTONTYPE_HLOG;
					break;

				case SCCP_FEATURE_QRT:
					buttonID = SKINNY_BUTTONTYPE_QRT;
					break;

				case SCCP_FEATURE_CALLBACK:
					buttonID = SKINNY_BUTTONTYPE_CALLBACK;
					break;

				case SCCP_FEATURE_OTHER_PICKUP:
					buttonID = SKINNY_BUTTONTYPE_OTHER_PICKUP;
					break;

				case SCCP_FEATURE_VIDEO_MODE:
					buttonID = SKINNY_BUTTONTYPE_VIDEO_MODE;
					break;

				case SCCP_FEATURE_NEW_CALL:
					buttonID = SKINNY_BUTTONTYPE_NEW_CALL;
					break;

				case SCCP_FEATURE_END_CALL:
					buttonID = SKINNY_BUTTONTYPE_END_CALL;
					break;

				case SCCP_FEATURE_PARKINGLOT:
#ifdef CS_SCCP_PARK
					sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) parkinglot state:%d\n", DEV_ID_LOG(device), config->button.feature.status);
					if (device->inuseprotocolversion > 15) {
						buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
					}
#endif
					break;

				case SCCP_FEATURE_TESTF:
					buttonID = SKINNY_BUTTONTYPE_TESTF;
					break;

				case SCCP_FEATURE_TESTG:
					buttonID = SKINNY_BUTTONTYPE_MESSAGES;
					break;

				case SCCP_FEATURE_TESTH:
					buttonID = SKINNY_BUTTONTYPE_DIRECTORY;
					break;

				case SCCP_FEATURE_TESTI:
					buttonID = SKINNY_BUTTONTYPE_TESTI;
					break;

				case SCCP_FEATURE_TESTJ:
					buttonID = SKINNY_BUTTONTYPE_APPLICATION;
					break;

				case SCCP_FEATURE_PICKUP:
					buttonID = SKINNY_BUTTONTYPE_GROUPCALLPICKUP;
					break;

				default:
					break;

			}

			/* send status using new message */
			if (device->inuseprotocolversion >= 15) {
				REQ(msg, FeatureStatDynamicMessage);
				msg->data.FeatureStatDynamicMessage.lel_featureIndex = htolel(instance);
				msg->data.FeatureStatDynamicMessage.lel_featureID = htolel(buttonID);
				msg->data.FeatureStatDynamicMessage.lel_featureStatus = htolel(config->button.feature.status);
				sccp_copy_string(msg->data.FeatureStatDynamicMessage.featureTextLabel, config->label, sizeof(msg->data.FeatureStatDynamicMessage.featureTextLabel));
			} else {
				REQ(msg, FeatureStatMessage);
				msg->data.FeatureStatMessage.lel_featureIndex = htolel(instance);
				msg->data.FeatureStatMessage.lel_featureID = htolel(buttonID);
				msg->data.FeatureStatMessage.lel_featureStatus = htolel(config->button.feature.status);
				sccp_copy_string(msg->data.FeatureStatMessage.featureTextLabel, config->label, sizeof(msg->data.FeatureStatDynamicMessage.featureTextLabel));
			}
			sccp_dev_send(device, msg);
			sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) Got Feature Status Request. Instance = %d, Label: '%s', Status: %d\n", DEV_ID_LOG(device), instance, config->label, config->button.feature.status);
		}
	}
	SCCP_LIST_UNLOCK(&((devicePtr)device)->buttonconfig);
}

/*!
 * \brief Device State Feature CallBack
 *
 * Called when we want to return a state change of a device
 */
#if defined(CS_DEVSTATE_FEATURE) && defined(CS_AST_HAS_EVENT)
void sccp_devstateFeatureState_cb(const struct ast_event *ast_event, void *data)
{
	/* parse the devstate string */
	/* If it is the custom family, isolate the specifier. */
	size_t len = strlen("Custom:");

	// char *sspecifier = 0;
	const char *dev;

	if (!data || !ast_event) {
		return;
	}
	dev = pbx_event_get_ie_str(ast_event, AST_EVENT_IE_DEVICE);

	sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "got device state change event from asterisk channel: %s\n", (dev) ? dev : "NULL");

	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain((sccp_device_t *) data));

	if (!device) {
		sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "NULL device in devstate event callback.\n");
		return;
	}
	if (!dev) {
		sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "NULL devstate string in devstate event callback.\n");
		return;
	}

	/* Note that we update all devstate feature buttons if we receive an event for one of them,
	   which we registered for. This will lead to unneccesary updates with multiple buttons.
	   In the future we might need a more elegant hint-registry for this type of notification,
	   which should be global to chan-sccp-b, not for each device. For now, this suffices. */
	if (!strncasecmp(dev, "Custom:", len)) {
		// sspecifier = (char *)(dev + len);
		sccp_featButton_changed(device, SCCP_FEATURE_DEVSTATE);
	}
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
