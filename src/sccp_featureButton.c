/*!
 * \file 	sccp_featureButton.c
 * \brief 	SCCP FeatureButton Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since 	2009-06-15
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks	Purpose: 	SCCP FeatureButton
 * 		When to use:	Only methods directly related to the phone featureButtons should be stored in this source file.
 *                              FeatureButtons are the ones at the bottom of the display, not to be confused with speeddial
 *                              buttons on the right side of the display.
 *   		Relationships: 	Call SCCP Features
 *
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/*!
 * \brief Feature Button Changed
 *
 * fetch the new state, and send status to device
 *
 * \param device SCCP Device
 * \param featureType SCCP Feature Type
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 *
 * \lock
 * 	- device->buttonconfig
 * 	  - see sccp_line_find_byname_wo()
 * 	  - see sccp_util_getDeviceConfiguration()
 * 	  - see sccp_dev_send()
 */
void sccp_featButton_changed(sccp_device_t * device, sccp_feature_type_t featureType)
{
	sccp_moo_t *featureMessage = NULL;
	sccp_buttonconfig_t *config = NULL, *buttonconfig = NULL;
	sccp_linedevices_t *linedevice = NULL;
	sccp_line_t *line;
	uint8_t instance = 0;
	uint8_t buttonID = SKINNY_BUTTONTYPE_FEATURE;				// Default feature type.
	boolean_t cfwdButtonEnabeld = TRUE;

#ifdef CS_DEVSTATE_FEATURE
	char buf[254] = "";
	int res = 0;
#endif

	if (!device) {
		return;
	}

	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == FEATURE && config->button.feature.id == featureType) {
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s \n", DEV_ID_LOG(device), config->button.feature.id, (config->button.feature.options) ? config->button.feature.options : "(none)");
			instance = config->instance;
			config->button.feature.status = 0;

			switch (config->button.feature.id) {
			case SCCP_FEATURE_PRIVACY:
				if (!device->privacyFeature.enabled) {
					config->button.feature.status = 0;
				}

				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", DEV_ID_LOG(device), device->privacyFeature.status);
				if (!strcasecmp(config->button.feature.options, "callpresent")) {
					uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;
					sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
					config->button.feature.status = (result) ? 1 : 0;
				}
				if (!strcasecmp(config->button.feature.options, "hint")) {
					uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;
					sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
					config->button.feature.status = (result) ? 1 : 0;
				}
				break;
			case SCCP_FEATURE_CFWDALL:

				/* get current state */
				SCCP_LIST_TRAVERSE(&device->buttonconfig, buttonconfig, list) {
					if (buttonconfig->type == LINE) {
						line = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
						if (line) {

							linedevice = sccp_util_getDeviceConfiguration(device, line);

							if (!linedevice) {
								if (device->registrationState == SKINNY_DEVICE_RS_OK) {
									ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
								}
								continue;
							}

							sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: SCCP_CFWD_ALL on line: %s is %s\n", DEV_ID_LOG(device), line->name, (linedevice->cfwdAll.enabled) ? "on" : "off");

							/* set this button active, only if all lines are fwd -requesting issue #3081549 */
							if (linedevice->cfwdAll.enabled == 0) {
								cfwdButtonEnabeld = FALSE;
							}
						}
					}
				}
				buttonconfig = NULL;

				if (cfwdButtonEnabeld)
					config->button.feature.status = 1;
				else
					config->button.feature.status = 0;

				break;

			case SCCP_FEATURE_DND:
				if (!strcasecmp(config->button.feature.options, "silent")) {
					if ((device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_SILENT)) {
						config->button.feature.status = 1;
					}
				} else if (!strcasecmp(config->button.feature.options, "busy")) {
					if ((device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT)) {
						config->button.feature.status = 1;
					}
				}
				break;
			case SCCP_FEATURE_MONITOR:
				config->button.feature.status = (device->monitorFeature.status) ? 1 : 0;
				break;

#ifdef CS_DEVSTATE_FEATURE
				/**
				  Handling of custom devicestate toggle button feature
				  */
			case SCCP_FEATURE_DEVSTATE:
				/* we check which devicestate this button is assigned to, and fetch the respective status from the astdb.
				   Note that this relies on the functionality of the asterisk custom devicestate module. */

				res = pbx_db_get(devstate_astdb_family, config->button.feature.options, buf, sizeof(buf));
				sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: devstate feature state: %s state: %s res: %d\n", DEV_ID_LOG(device), config->button.feature.options, buf, res);
				if (!res) {
					if (!strncmp("INUSE", buf, 254))
						config->button.feature.status = 1;
					else
						config->button.feature.status = 0;
				}
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

			case SCCP_FEATURE_TEST6:
				buttonID = SKINNY_BUTTONTYPE_TEST6;
				break;

			case SCCP_FEATURE_TEST7:
				buttonID = SKINNY_BUTTONTYPE_TEST7;
				break;

			case SCCP_FEATURE_TEST8:
				buttonID = SKINNY_BUTTONTYPE_TEST8;
				break;

			case SCCP_FEATURE_TEST9:
				buttonID = SKINNY_BUTTONTYPE_TEST9;
				break;

			case SCCP_FEATURE_TESTA:
				buttonID = SKINNY_BUTTONTYPE_TESTA;
				break;

			case SCCP_FEATURE_TESTB:
				buttonID = SKINNY_BUTTONTYPE_TESTB;
				break;

			case SCCP_FEATURE_TESTC:
				buttonID = SKINNY_BUTTONTYPE_TESTC;
				break;

			case SCCP_FEATURE_TESTD:
				buttonID = SKINNY_BUTTONTYPE_TESTD;
				break;

			case SCCP_FEATURE_TESTE:
				buttonID = SKINNY_BUTTONTYPE_TESTE;
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
				buttonID = SKINNY_STIMULUS_GROUPCALLPICKUP;
				break;

			default:
				break;

			}

			/* send status using new message */
			if (device->inuseprotocolversion >= 15) {
				REQ(featureMessage, FeatureStatAdvancedMessage);
				featureMessage->msg.FeatureStatAdvancedMessage.lel_instance = htolel(instance);
				featureMessage->msg.FeatureStatAdvancedMessage.lel_type = htolel(buttonID);
				featureMessage->msg.FeatureStatAdvancedMessage.lel_status = htolel(config->button.feature.status);
				sccp_copy_string(featureMessage->msg.FeatureStatAdvancedMessage.DisplayName, config->button.feature.label, strlen(config->button.feature.label) + 1);
			} else {
				REQ(featureMessage, FeatureStatMessage);
				featureMessage->msg.FeatureStatMessage.lel_featureInstance = htolel(instance);
				featureMessage->msg.FeatureStatMessage.lel_featureID = htolel(buttonID);
				featureMessage->msg.FeatureStatMessage.lel_featureStatus = htolel(config->button.feature.status);
				sccp_copy_string(featureMessage->msg.FeatureStatMessage.featureTextLabel, config->button.feature.label, strlen(config->button.feature.label) + 1);
			}
			sccp_dev_send(device, featureMessage);
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request. Instance = %d Label: %s Status: %d\n", DEV_ID_LOG(device), instance, config->button.feature.label, config->button.feature.status);
		}
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

#ifdef CS_DEVSTATE_FEATURE

void sccp_devstateFeatureState_cb(const struct ast_event *ast_event, void *data)
{
	/* parse the devstate string */
	/* If it is the custom family, isolate the specifier. */
	sccp_device_t *device;
	size_t len = strlen("Custom:");
	char *sspecifier = 0;
	const char *dev;

	if (!data || !ast_event)
		return;

	dev = ast_event_get_ie_str(ast_event, AST_EVENT_IE_DEVICE);

	sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "got device state change event from asterisk channel: %s\n", (dev) ? dev : "NULL");

	device = (sccp_device_t *) data;

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
		sspecifier = (char *)(dev + len);
		sccp_featButton_changed(device, SCCP_FEATURE_DEVSTATE);
	}
}
#endif
