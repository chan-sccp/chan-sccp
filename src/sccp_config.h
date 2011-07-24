
/*!
 * \file 	sccp_config.h
 * \brief 	SCCP Config Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */
#ifndef __SCCP_CONFIG_H
#    define __SCCP_CONFIG_H

/*!
 * \brief Soft Key Configuration Template Structure
 */
typedef struct {
	const char configVar[50];						/*!< Config Variable as Character */
	const int softkey;							/*!< Softkey as Int */
} softkeyConfigurationTemplate;							/*!< Soft Key Configuration Template Structure */

static const softkeyConfigurationTemplate softKeyTemplate[] = {
	{"redial", SKINNY_LBL_REDIAL},
	{"newcall", SKINNY_LBL_NEWCALL},
	{"cfwdall", SKINNY_LBL_CFWDALL},
	{"cfwdbusy", SKINNY_LBL_CFWDBUSY},
	{"cfwdnoanswer", SKINNY_LBL_CFWDNOANSWER},
	{"pickup", SKINNY_LBL_PICKUP},
	{"gpickup", SKINNY_LBL_GPICKUP},
	{"conflist", SKINNY_LBL_CONFLIST},
	{"dnd", SKINNY_LBL_DND},
	{"hold", SKINNY_LBL_HOLD},
	{"endcall", SKINNY_LBL_ENDCALL},
	{"park", SKINNY_LBL_PARK},
	{"select", SKINNY_LBL_SELECT},
	{"idivert", SKINNY_LBL_IDIVERT},
	{"resume", SKINNY_LBL_RESUME},
	{"newcall", SKINNY_LBL_NEWCALL},
	{"transfer", SKINNY_LBL_TRANSFER},
	{"dirtrfr", SKINNY_LBL_DIRTRFR},
	{"answer", SKINNY_LBL_ANSWER},
	{"transvm", SKINNY_LBL_TRNSFVM},
	{"private", SKINNY_LBL_PRIVATE},
	{"meetme", SKINNY_LBL_MEETME},
	{"barge", SKINNY_LBL_BARGE},
	{"cbarge", SKINNY_LBL_CBARGE},
	{"conf", SKINNY_LBL_CONFRN},
	{"confrn", SKINNY_LBL_CONFRN},
	{"back", SKINNY_LBL_BACKSPACE},
	{"join", SKINNY_LBL_JOIN},
	{"intrcpt", SKINNY_LBL_INTRCPT},
	{"newcallcustom", SKINNY_LBL_NEWCALLCUSTOM},
	{"monitor", SKINNY_LBL_MONITOR},
	{"dial", SKINNY_LBL_DIAL},
	{"empty", SKINNY_LBL_EMPTY},
};

#    ifdef CS_DYNAMIC_CONFIG
void sccp_config_addButton(sccp_device_t * device, int index, button_type_t type, const char *name, const char *option, const char *args);
#    else
void sccp_config_addLine(sccp_device_t * device, char *lineName, char *options, uint16_t index);
void sccp_config_addEmpty(sccp_device_t * device, uint16_t index);
void sccp_config_addSpeeddial(sccp_device_t * device, char *label, char *extension, char *hint, uint16_t index);
void sccp_config_addFeature(sccp_device_t * device, char *label, char *featureID, char *args, uint16_t index);
void sccp_config_addService(sccp_device_t * device, char *label, char *url, uint16_t index);
#    endif

sccp_device_t *sccp_config_buildDevice(struct ast_variable *variable, const char *deviceName, boolean_t isRealtime);
sccp_line_t *sccp_config_buildLine(struct ast_variable *variable, const char *lineName, boolean_t isRealtime);
boolean_t sccp_config_general(sccp_readingtype_t readingtype);
void cleanup_stale_contexts(char *new, char *old);
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype);

sccp_configurationchange_t sccp_config_applyLineConfiguration(sccp_line_t * l, struct ast_variable *v);
sccp_device_t *sccp_config_applyDeviceConfiguration(sccp_device_t * d, struct ast_variable *v);

void sccp_config_softKeySet(struct ast_variable *variable, const char *name);
uint8_t sccp_config_readSoftSet(uint8_t * softkeyset, const char *data);
int sccp_config_getSoftkeyLbl(char *key);
void sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device);

#endif /*__SCCP_CONFIG_H */
