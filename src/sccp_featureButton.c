/*
 * sccp_featureButton.c
 *
 *  Created on: 25.06.2009
 *      Author: marcello
 */
#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_protocol.h"
#include "sccp_lock.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_featureButton.h"

#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#endif


/* fetch the new sate, and send status to device */
void sccp_featButton_changed(sccp_device_t *device, sccp_feature_type_t featureType){
	sccp_moo_t 			*featureRequestMessage = NULL;
	sccp_buttonconfig_t		*config=NULL, *buttonconfig = NULL;
	sccp_line_t			*line;
	uint8_t				instance=0;


	if(!device){
		return;
	}



	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list){
		if(config->type == FEATURE && config->button.feature.id == featureType){
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s \n", DEV_ID_LOG(device), config->button.feature.id, (config->button.feature.options)?config->button.feature.options:"(none)");
			instance = config->instance;
			config->button.feature.status = 0;
			switch(config->button.feature.id){
				case SCCP_FEATURE_PRIVACY:
					if(!device->privacyFeature.enabled){
						config->button.feature.status = 0;
						//break;
					}

					sccp_log(10)(VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", DEV_ID_LOG(device), device->privacyFeature.status);
					if(!strcasecmp(config->button.feature.options, "callpresent")){
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result)?1:0;
					}
					if(!strcasecmp(config->button.feature.options, "hint")){
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result)?1:0;
					}
				break;
				case SCCP_FEATURE_CFWDALL:

					/* get current state */

					SCCP_LIST_TRAVERSE(&device->buttonconfig, buttonconfig, list) {
						if(buttonconfig->type == LINE ){
							line = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
							if(line){
								sccp_log(10)(VERBOSE_PREFIX_3 "%s: SCCP_CFWD_ALL on line: %s is %s\n", DEV_ID_LOG(device), line->name, (line->cfwd_type == SCCP_CFWD_ALL)?"on":"off");
								if(line->cfwd_type == SCCP_CFWD_ALL){
									config->button.feature.status = 1;
									break;
								}
							}
						}
					}
					buttonconfig = NULL;

				break;

				case SCCP_FEATURE_DND:
					if(!strcasecmp(config->button.feature.options, "silent")){
						if( (device->dnd && device->dndmode == SCCP_DNDMODE_SILENT) || (device->dnd == SCCP_DNDMODE_SILENT && device->dndmode == SCCP_DNDMODE_USERDEFINED) ){
							config->button.feature.status = 1;
						}
					}else if(!strcasecmp(config->button.feature.options, "busy")){
						if( (device->dnd && device->dndmode == SCCP_DNDMODE_REJECT) || (device->dnd == SCCP_DNDMODE_REJECT && device->dndmode == SCCP_DNDMODE_USERDEFINED) ){
							config->button.feature.status = 1;
						}
					}
				break;
				case SCCP_FEATURE_MONITOR:
					config->button.feature.status = device->monitorFeature.status;
				break;


				default:
				break;

			}

			/* send status */
			REQ(featureRequestMessage, FeatureStatMessage);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureInstance = htolel(instance);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureID = htolel(0x13);
			sccp_copy_string(featureRequestMessage->msg.FeatureStatMessage.featureTextLabel, config->button.feature.label, strlen(config->button.feature.label)+1);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureStatus = htolel(config->button.feature.status);
			sccp_dev_send(device, featureRequestMessage);
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got Feature Status Request. Instance = %d Status: %d\n", device->id, instance, config->button.feature.status);
		}

	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}
