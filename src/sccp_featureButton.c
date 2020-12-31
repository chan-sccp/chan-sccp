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
#include "sccp_linedevice.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

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
	sccp_buttonconfig_t * config = NULL;

	sccp_buttonconfig_t * buttonconfig = NULL;
	uint8_t instance = 0;
	uint8_t buttonID = SKINNY_BUTTONTYPE_FEATURE;								// Default feature type.
	boolean_t lineFound = FALSE;
	char label_text[StationDynamicNameSize];

	if (!device) {
		return;
	}

	SCCP_LIST_LOCK(&(((devicePtr)device)->buttonconfig));
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == FEATURE && config->button.feature.id == featureType) {
			sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) FeatureID = %d, Option: %s\n", DEV_ID_LOG(device), config->button.feature.id, (config->button.feature.options) ? config->button.feature.options : "(none)");
			sccp_copy_string(label_text, config->label, sizeof(label_text));
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
								AUTO_RELEASE(sccp_linedevice_t, ld, sccp_linedevice_find(device, line));

								if(ld) {
									sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: SCCP_CFWD_ALL on line: %s is %s\n", DEV_ID_LOG(device), line->name,
															       (ld->cfwd[SCCP_CFWD_ALL].enabled) ? "on" : "off");

									/* set this button active, only if all lines are fwd -requesting issue #3081549 */
									// Upon finding the first existing line, we need to set the feature status									// to TRUE and subsequently AND
									// that value with the forward status of each line.
									if (FALSE == lineFound) {
										lineFound = TRUE;
										config->button.feature.status = 1;
									}
									// Set status of feature by logical and to comply with requirement above.
									config->button.feature.status &= ((ld->cfwd[SCCP_CFWD_ALL].enabled) ? 1 : 0);                                        // Logical and &= intended here.
								}
							}
						}
					}
					buttonconfig = NULL;

					break;

				case SCCP_FEATURE_DND:
					{
						// coverity[MIXED_ENUMS]
						sccp_dndmode_t status = (sccp_dndmode_t)device->dndFeature.status;
						if (sccp_strcaseequals(config->button.feature.options, "silent")) {
							if ((device->dndFeature.enabled && status == SCCP_DNDMODE_SILENT)) {
								config->button.feature.status = 1;
							} else {
								config->button.feature.status = 0;
							}
						} else if (sccp_strcaseequals(config->button.feature.options, "busy")) {
							if ((device->dndFeature.enabled && status == SCCP_DNDMODE_REJECT)) {
								config->button.feature.status = 1;
							} else {
								config->button.feature.status = 0;
							}
						} else {
							if (device->inuseprotocolversion > 15) {				// multiple States
								buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
								switch (status) {
									case SCCP_DNDMODE_OFF:
										config->button.feature.status = 0x010000;                                        // off
										break;
									case SCCP_DNDMODE_REJECT:
										config->button.feature.status = 0x020202;                                        // red/on/filled
										break;
									case SCCP_DNDMODE_SILENT:
										config->button.feature.status = 0x030302;                                        // amber/blink/filled
										break;
									case SCCP_DNDMODE_SENTINEL:
										/* fall through */
									case SCCP_DNDMODE_USERDEFINED:
										config->button.feature.status = 0x030303;                                        // amber/blink/boxes
										break;
								}
							} else {								// old non-java phones
								if (status == SCCP_DNDMODE_OFF) {
									config->button.feature.status = 0;
								} else {
									config->button.feature.status = 1;
								}
							}
						}
					}
					break;
				case SCCP_FEATURE_MONITOR:
					{
						sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) monitor featureButton new state:%s (%d)\n", DEV_ID_LOG(device), sccp_feature_monitor_state2str(device->monitorFeature.status), device->monitorFeature.status);
						// coverity[MIXED_ENUMS]
						uint8_t status = (sccp_feature_monitor_state_t) device->monitorFeature.status;
						if(device->inuseprotocolversion <= 15 || device->skinny_type == SKINNY_DEVICETYPE_CISCO8941
						   || device->skinny_type == SKINNY_DEVICETYPE_CISCO8945) {                                        // multiple States
							// firmware on the 89xx series is broken, showing the color of the lamp on the label of MultiBlinkButtons
							// so using old SKINNY_BUTTONTYPE_MONITOR instead
							switch (status) {
								case SCCP_FEATURE_MONITOR_STATE_DISABLED:
									config->button.feature.status = 0;
									break;
								case SCCP_FEATURE_MONITOR_STATE_REQUESTED:
									// fall through
								case SCCP_FEATURE_MONITOR_STATE_ACTIVE:
									// fall through
								case (SCCP_FEATURE_MONITOR_STATE_REQUESTED | SCCP_FEATURE_MONITOR_STATE_ACTIVE):
									config->button.feature.status = 1;
									break;
							}
						} else {
							buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
							switch (status) {
								case SCCP_FEATURE_MONITOR_STATE_DISABLED:
									config->button.feature.status = 0; /*off*/
									break;
								case SCCP_FEATURE_MONITOR_STATE_REQUESTED:
									// snprintf(label_text, sizeof(label_text), "%s (%s)", config->label, SKINNY_DISP_RECORDING_AWAITING_CALL_TO_BE_ACTIVE);
									config->button.feature.status = 0x020302; /*amber/on/filled */
									break;
								case SCCP_FEATURE_MONITOR_STATE_ACTIVE:
									snprintf(label_text, sizeof(label_text), "%s (%s)", config->label, SKINNY_DISP_RECORDING);
									config->button.feature.status = 0x030203; /*red/slowblink/filled */
									break;
								case (SCCP_FEATURE_MONITOR_STATE_REQUESTED | SCCP_FEATURE_MONITOR_STATE_ACTIVE):
									snprintf(label_text, sizeof(label_text), "%s (%s)", config->label, SKINNY_DISP_RECORDING);
									config->button.feature.status = 0x030205; /*red/slowblink/boxes */
									break;
							}
						}
					}
					break;
#ifdef CS_DEVSTATE_FEATURE
					case SCCP_FEATURE_DEVSTATE:
						/* handled by sccp_devstate.c */
						goto EXIT_FUNC;
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

					case SCCP_FEATURE_HUNT_GROUP_LOG_IN_OUT:
						buttonID = SKINNY_BUTTONTYPE_HUNT_GROUP_LOG_IN_OUT;
						break;

					case SCCP_FEATURE_QUALITY_REPORT_TOOL:
						buttonID = SKINNY_BUTTONTYPE_QUALITY_REPORT_TOOL;
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
				if (!msg) {
					return;
				}
				msg->data.FeatureStatDynamicMessage.lel_lineInstance = htolel(instance);
				msg->data.FeatureStatDynamicMessage.lel_buttonType = htolel(buttonID);
				msg->data.FeatureStatDynamicMessage.stateVal.lel_uint32 = htolel(config->button.feature.status);
				sccp_copy_string(msg->data.FeatureStatDynamicMessage.textLabel, label_text, sizeof(msg->data.FeatureStatDynamicMessage.textLabel));
			} else {
				REQ(msg, FeatureStatMessage);
				if (!msg) {
					return;
				}
				msg->data.FeatureStatMessage.lel_lineInstance = htolel(instance);
				msg->data.FeatureStatMessage.lel_buttonType = htolel(buttonID);
				msg->data.FeatureStatMessage.lel_stateValue = htolel(config->button.feature.status);
				sccp_copy_string(msg->data.FeatureStatMessage.textLabel, label_text, sizeof(msg->data.FeatureStatMessage.textLabel));
			}
			sccp_dev_send(device, msg);
			sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_featButton_changed) Got Feature Status Request. Instance = %d, Label: '%s', Status: %d\n", DEV_ID_LOG(device), instance, config->label, config->button.feature.status);
		}
	}
EXIT_FUNC:
	SCCP_LIST_UNLOCK(&(((devicePtr)device)->buttonconfig));
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
