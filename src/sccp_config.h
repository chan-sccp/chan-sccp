
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
#    ifndef __SCCP_CONFIG_H
#define __SCCP_CONFIG_H

/*!
 * \brief Enum for Config Value Change Status
 */
typedef enum {
/* *INDENT-OFF* */
	SCCP_CONFIG_CHANGE_NOCHANGE			= 0,
	SCCP_CONFIG_CHANGE_CHANGED,
	SCCP_CONFIG_CHANGE_INVALIDVALUE,
/* *INDENT-ON* */
} sccp_value_changed_t;

/*!
 * \brief Enum for Config Option Blocks
 */
typedef enum {
	SCCP_CONFIG_GLOBAL_SEGMENT = 0,
	SCCP_CONFIG_DEVICE_SEGMENT,
	SCCP_CONFIG_LINE_SEGMENT,
	SCCP_CONFIG_SOFTKEY_SEGMENT,
} sccp_config_segment_t;

void sccp_config_set_defaults(void *obj, const sccp_config_segment_t segment, const uint8_t alreadySetEntries[], uint8_t arraySize);
void sccp_copy_defaultValue(const char *name, void *obj, const sccp_device_t * device, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_dnd(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);

/*!
 * \brief Soft Key Configuration Template Structure
 */
typedef struct {
	const char configVar[50];						/*!< Config Variable as Character */
	const int softkey;							/*!< Softkey as Int */
} softkeyConfigurationTemplate;							/*!< Soft Key Configuration Template Structure */

static const softkeyConfigurationTemplate softKeyTemplate[] = {
/* *INDENT-OFF* */
	{"redial", 	SKINNY_LBL_REDIAL},
	{"newcall", 	SKINNY_LBL_NEWCALL},
	{"cfwdall", 	SKINNY_LBL_CFWDALL},
	{"cfwdbusy", 	SKINNY_LBL_CFWDBUSY},
	{"cfwdnoanswer",SKINNY_LBL_CFWDNOANSWER},
	{"pickup", 	SKINNY_LBL_PICKUP},
	{"gpickup", 	SKINNY_LBL_GPICKUP},
	{"conflist", 	SKINNY_LBL_CONFLIST},
	{"dnd", 	SKINNY_LBL_DND},
	{"hold", 	SKINNY_LBL_HOLD},
	{"endcall", 	SKINNY_LBL_ENDCALL},
	{"park", 	SKINNY_LBL_PARK},
	{"select", 	SKINNY_LBL_SELECT},
	{"idivert", 	SKINNY_LBL_IDIVERT},
	{"resume", 	SKINNY_LBL_RESUME},
	{"newcall", 	SKINNY_LBL_NEWCALL},
	{"transfer", 	SKINNY_LBL_TRANSFER},
	{"dirtrfr", 	SKINNY_LBL_DIRTRFR},
	{"answer", 	SKINNY_LBL_ANSWER},
	{"transvm", 	SKINNY_LBL_TRNSFVM},
	{"private", 	SKINNY_LBL_PRIVATE},
	{"meetme", 	SKINNY_LBL_MEETME},
	{"barge", 	SKINNY_LBL_BARGE},
	{"cbarge", 	SKINNY_LBL_CBARGE},
	{"conf", 	SKINNY_LBL_CONFRN},
	{"confrn",	SKINNY_LBL_CONFRN},
	{"back", 	SKINNY_LBL_BACKSPACE},
	{"join", 	SKINNY_LBL_JOIN},
	{"intrcpt", 	SKINNY_LBL_INTRCPT},
        {"monitor", 	SKINNY_LBL_MONITOR},  
	{"dial", 	SKINNY_LBL_DIAL},
	{"vidmode", 	SKINNY_LBL_VIDEO_MODE},
	{"empty", 	SKINNY_LBL_EMPTY},
/* *INDENT-ON* */
};

sccp_value_changed_t sccp_config_addButton(void *buttonconfig_head, int index, button_type_t type, const char *name, const char *option, const char *args);
sccp_line_t *sccp_config_buildLine(sccp_line_t * l, PBX_VARIABLE_TYPE * v, const char *lineName, boolean_t isRealtime);
sccp_device_t *sccp_config_buildDevice(sccp_device_t * d, PBX_VARIABLE_TYPE * v, const char *deviceName, boolean_t isRealtime);
boolean_t sccp_config_general(sccp_readingtype_t readingtype);
void cleanup_stale_contexts(char *newContext, char *oldContext);
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype);
int sccp_manager_config_metadata(struct mansession *s, const struct message *m);

#define CONFIG_STATUS_FILEOLD       (void *)-3
#define CONFIG_STATUS_FILE_NOT_SCCP (void *)-4
struct ast_config *sccp_config_getConfig(void);
sccp_configurationchange_t sccp_config_applyGlobalConfiguration(PBX_VARIABLE_TYPE * v);
sccp_configurationchange_t sccp_config_applyLineConfiguration(sccp_line_t * l, PBX_VARIABLE_TYPE * v);
sccp_configurationchange_t sccp_config_applyDeviceConfiguration(sccp_device_t * d, PBX_VARIABLE_TYPE * v);

void sccp_config_softKeySet(PBX_VARIABLE_TYPE * variable, const char *name);
uint8_t sccp_config_readSoftSet(uint8_t * softkeyset, const char *data);
int sccp_config_getSoftkeyLbl(char *key);
void sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device);

int sccp_config_generate(char *filename, int configType);

#    endif     /*__SCCP_CONFIG_H */
