/*!
 * \file 	sccp_featureButton.c
 * \brief 	SCCP FeatureButton Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since 	2009-06-15
 * \date        $Date$
 * \version     $Revision$  
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

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$") 

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


/*!
 * \brief Feature Button Changed
 *
 * fetch the new sate, and send status to device
 * 
 * \param device SCCP Device
 * \param featureType SCCP Feature Type
 */
void sccp_featButton_changed(sccp_device_t *device, sccp_feature_type_t featureType)
{
	sccp_moo_t			*featureAdvancedMessage = NULL;
	sccp_buttonconfig_t		*config=NULL, *buttonconfig = NULL;
	sccp_linedevices_t		*linedevice =NULL;
	sccp_line_t			*line;
	uint8_t				instance=0;
	uint8_t				buttonID=SKINNY_BUTTONTYPE_FEATURE;  // Default feature type.


	if(!device){
		return;
	}



	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list){
		if(config->type == FEATURE && config->button.feature.id == featureType){
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s \n", DEV_ID_LOG(device), config->button.feature.id, (config->button.feature.options)?config->button.feature.options:"(none)");
			instance = config->instance;
			config->button.feature.status = 0;
			
			switch(config->button.feature.id){
				case SCCP_FEATURE_PRIVACY:
					if(!device->privacyFeature.enabled){
						config->button.feature.status = 0;
						//break;
					}

					sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", DEV_ID_LOG(device), device->privacyFeature.status);
					if(!strcasecmp(config->button.feature.options, "callpresent")){
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;
						sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result)?1:0;
					}
					if(!strcasecmp(config->button.feature.options, "hint")){
						uint32_t result = device->privacyFeature.status & SCCP_PRIVACYFEATURE_HINT;
						sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: result is %d\n", device->id, result);
						config->button.feature.status = (result)?1:0;
					}
				break;
				case SCCP_FEATURE_CFWDALL:

					/* get current state */

					SCCP_LIST_TRAVERSE(&device->buttonconfig, buttonconfig, list) {
						if(buttonconfig->type == LINE ){
							line = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
							if(line){

								SCCP_LIST_LOCK(&line->devices);
								SCCP_LIST_TRAVERSE(&line->devices, linedevice, list){
									if(linedevice->device == device);
										break;
								}
								SCCP_LIST_UNLOCK(&line->devices);

								if(!linedevice){
									ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
									continue;
								}

								sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: SCCP_CFWD_ALL on line: %s is %s\n", DEV_ID_LOG(device), line->name, (linedevice->cfwdAll.enabled)?"on":"off");
								if(linedevice->cfwdAll.enabled){
									config->button.feature.status = 1;
								}
							}
						}
					}
					buttonconfig = NULL;

				break;

				case SCCP_FEATURE_DND:
					if(!strcasecmp(config->button.feature.options, "silent")){
						if( (device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_SILENT) ){
							config->button.feature.status = 1;
						}
					}else if(!strcasecmp(config->button.feature.options, "busy")){
						if( ( device->dndFeature.enabled && device->dndFeature.status == SCCP_DNDMODE_REJECT) ){
							config->button.feature.status = 1;
						}
					}
				break;
				case SCCP_FEATURE_MONITOR:
					config->button.feature.status = (device->monitorFeature.status)?1:0;
				break;

				case SCCP_FEATURE_HOLD:
					buttonID = SKINNY_BUTTONTYPE_HOLD;
					break;

				case SCCP_FEATURE_TRANSFER:
					buttonID = SKINNY_BUTTONTYPE_TRANSFER;
					break;

				case SCCP_FEATURE_MULTIBLINK:
					buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
					config->button.feature.status = device->priFeature.status;
					buttonID = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
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

				default:
				break;

			}


			/* send status using old message */
#if 0
			REQ(featureRequestMessage, FeatureStatMessage);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureInstance = htolel(instance);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureID = htolel(buttonID);
			sccp_copy_string(featureRequestMessage->msg.FeatureStatMessage.featureTextLabel, config->button.feature.label, strlen(config->button.feature.label)+1);
			featureRequestMessage->msg.FeatureStatMessage.lel_featureStatus = htolel(config->button.feature.status);
			sccp_dev_send(device, featureRequestMessage);
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE))(VERBOSE_PREFIX_3 "%s: Got Feature Status Request. Instance = %d Status: %d\n", device->id, instance, config->button.feature.status);
#else
			/* send status using new message */
			REQ(featureAdvancedMessage, FeatureStatAdvancedMessage);
			featureAdvancedMessage->msg.FeatureStatAdvancedMessage.lel_instance = htolel(instance);
			featureAdvancedMessage->msg.FeatureStatAdvancedMessage.lel_type     = htolel(buttonID);
			featureAdvancedMessage->msg.FeatureStatAdvancedMessage.lel_status = htolel(config->button.feature.status);
			sccp_copy_string(featureAdvancedMessage->msg.FeatureStatAdvancedMessage.DisplayName, config->button.feature.label, strlen(config->button.feature.label)+1);
			sccp_dev_send(device, featureAdvancedMessage);
#endif
		} 
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}
