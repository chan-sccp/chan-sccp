/*!
 * \file        sccp_config.h
 * \brief       SCCP Config Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

__BEGIN_C_EXTERN__
// sccp_buttonconfig_list_t externally declared in sccp_device.h, required by sccp_config_addButton
extern struct sccp_buttonconfig_list sccp_buttonconfig_list;

/*!
 * \brief Enum for Config Value Change Status
 */
typedef enum {
	SCCP_CONFIG_CHANGE_NOCHANGE,
	SCCP_CONFIG_CHANGE_CHANGED,
	SCCP_CONFIG_CHANGE_INVALIDVALUE,
	SCCP_CONFIG_CHANGE_ERROR,
} sccp_value_changed_t;

/*!
 * \brief Enum for Config Option Blocks
 */
typedef enum {
	SCCP_CONFIG_GLOBAL_SEGMENT,
	SCCP_CONFIG_DEVICE_SEGMENT,
	SCCP_CONFIG_LINE_SEGMENT,
	SCCP_CONFIG_SOFTKEY_SEGMENT,
} sccp_config_segment_t;

SCCP_API void SCCP_CALL sccp_copy_defaultValue(const char *name, void *obj, const sccp_device_t * device, const sccp_config_segment_t segment);
SCCP_API int SCCP_CALL sccp_manager_config_metadata(struct mansession *s, const struct message *m);
SCCP_API void SCCP_CALL sccp_config_cleanup_dynamically_allocated_memory(void *obj, const sccp_config_segment_t segment);
SCCP_API sccp_value_changed_t SCCP_CALL sccp_config_addButton(sccp_buttonconfig_list_t *buttonconfigList, int buttonindex, sccp_config_buttontype_t type, const char *name, const char *options, const char *args);
SCCP_API boolean_t SCCP_CALL sccp_config_general(sccp_readingtype_t readingtype);
SCCP_API void SCCP_CALL cleanup_stale_contexts(char *new, char *old);
SCCP_API boolean_t SCCP_CALL sccp_config_readDevicesLines(sccp_readingtype_t readingtype);
SCCP_API int SCCP_CALL sccp_manager_config_metadata(struct mansession *s, const struct message *m);

/*!
 * \brief Enum for Config File Status (Return Values)
 */
typedef enum {
	/* *INDENT-OFF* */
	CONFIG_STATUS_FILE_NOT_CHANGED 	= -1,
	CONFIG_STATUS_FILE_OK 		= 0,
	CONFIG_STATUS_FILE_OLD 		= 1,
	CONFIG_STATUS_FILE_NOT_SCCP 	= 2,
	CONFIG_STATUS_FILE_NOT_FOUND 	= 3,
	CONFIG_STATUS_FILE_INVALID 	= 5,
	/* *INDENT-ON* */
} sccp_config_file_status_t;

SCCP_API sccp_config_file_status_t SCCP_CALL sccp_config_getConfig(boolean_t force);
SCCP_API sccp_configurationchange_t SCCP_CALL sccp_config_applyGlobalConfiguration(PBX_VARIABLE_TYPE * v);
SCCP_API sccp_configurationchange_t SCCP_CALL sccp_config_applyLineConfiguration(sccp_line_t * l, PBX_VARIABLE_TYPE * v);
SCCP_API sccp_configurationchange_t SCCP_CALL sccp_config_applyDeviceConfiguration(sccp_device_t * d, PBX_VARIABLE_TYPE * v);
SCCP_API sccp_configurationchange_t SCCP_CALL sccp_config_applyDeviceDefaults(sccp_device_t * device, PBX_VARIABLE_TYPE * variable);

SCCP_API void SCCP_CALL sccp_config_softKeySet(PBX_VARIABLE_TYPE * variable, const char *name);
SCCP_API void SCCP_CALL sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device);

SCCP_API int SCCP_CALL sccp_config_generate(char *filename, int configType);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
