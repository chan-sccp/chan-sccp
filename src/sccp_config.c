/*!
 * \file        sccp_config.c
 * \brief       SCCP Config Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \note        To find out more about the reload function see \ref sccp_config_reload
 * \remarks     Only methods directly related to chan-sccp configuration should be stored in this source file.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \section sccp_config Loading sccp.conf/realtime configuration implementation
 *
 * \subsection sccp_config_reload How was the new cli command "sccp reload" implemented
 *
 * sccp_cli.c
 * - new implementation of cli reload command
 *  - checks if no other reload command is currently running
 *  - starts loading global settings from sccp.conf (sccp_config_general)
 *  - starts loading devices and lines from sccp.conf(sccp_config_readDevicesLines)
 *  .
 * .
 *
 * sccp_config.c
 * - modified sccp_config_general
 *
 * - modified sccp_config_readDevicesLines
 *  - sets pendingDelete for
 *    - devices (via sccp_device_pre_reload),
 *    - lines (via sccp_line_pre_reload)
 *    - softkey (via sccp_softkey_pre_reload)
 *    .
 *  - calls sccp_config_buildDevice as usual
 *    - calls sccp_config_buildDevice as usual
 *      - find device
 *      - or create new device
 *      - parses sccp.conf for device
 *      - set defaults for device if necessary using the default from globals using the same parameter name
 *      - set pendingUpdate on device for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 *      .
 *    - calls sccp_config_buildLine as usual
 *      - find line
 *      - or create new line
 *      - parses sccp.conf for line
 *      - set defaults for line if necessary using the default from globals using the same parameter name
 *      - set pendingUpdate on line for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 *      .
 *    - calls sccp_config_softKeySet as usual ***
 *      - find softKeySet
 *      - or create new softKeySet
 *      - parses sccp.conf for softKeySet
 *      - set pendingUpdate on softKetSet for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 *      .
 *    .
 *  - checks pendingDelete and pendingUpdate for
 *    - skip when call in progress
 *    - devices (via sccp_device_post_reload),
 *      - resets GLOB(device) if pendingUpdate
 *      - removes GLOB(devices) with pendingDelete
 *      .
 *    - lines (via sccp_line_post_reload)
 *      - resets GLOB(lines) if pendingUpdate
 *      - removes GLOB(lines) with pendingDelete
 *      .
 *    - softkey (via sccp_softkey_post_reload) ***
 *      - resets GLOB(softkeyset) if pendingUpdate ***
 *      - removes GLOB(softkeyset) with pendingDelete ***
 *      .
 *    .
 *  .
 * channel.c
 * - sccp_channel_endcall ***
 *   - reset device if still device->pendingUpdate,line->pendingUpdate or softkeyset->pendingUpdate
 *   .
 * .
 *
 * lines marked with "***" still need be implemented
 *
 */

#include <config.h>
#include "common.h"
#include "sccp_config.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_featureButton.h"
#include "sccp_mwi.h"
#include "sccp_socket.h"
#include <asterisk/paths.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$");
#ifndef offsetof
#if defined(__GNUC__) && __GNUC__ > 3
#define offsetof(type, member)  __builtin_offsetof (type, member)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif
#ifndef offsetof
#endif
#define offsize(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)
#define G_OBJ_REF(x) offsetof(struct sccp_global_vars,x), offsize(struct sccp_global_vars,x)
#define D_OBJ_REF(x) offsetof(struct sccp_device,x), offsize(struct sccp_device,x)
#define L_OBJ_REF(x) offsetof(struct sccp_line,x), offsize(struct sccp_line,x)
#define S_OBJ_REF(x) offsetof(struct softKeySetConfiguration,x), offsize(struct softKeySetConfiguration,x)
#define H_OBJ_REF(x) offsetof(struct sccp_hotline,x), offsize(struct sccp_hotline,x)
#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITTOGGLE(a, b) ((a)[BITSLOT(b)] ^= BITMASK(b))
    /*!
     * \brief Enum for Config Option Types
     */
enum SCCPConfigOptionType {
/* *INDENT-OFF* */
	SCCP_CONFIG_DATATYPE_BOOLEAN			= 1 << 0,
	SCCP_CONFIG_DATATYPE_INT			= 1 << 1,
	SCCP_CONFIG_DATATYPE_UINT			= 1 << 2,
	SCCP_CONFIG_DATATYPE_STRING			= 1 << 3,
	SCCP_CONFIG_DATATYPE_PARSER			= 1 << 4,
	SCCP_CONFIG_DATATYPE_STRINGPTR			= 1 << 5,		/* string pointer */
	SCCP_CONFIG_DATATYPE_CHAR			= 1 << 6,
	SCCP_CONFIG_DATATYPE_ENUM			= 1 << 7,
/* *INDENT-ON* */
};

/*!
 * \brief Enum for Config Option Flags
 */
enum SCCPConfigOptionFlag {
/* *INDENT-OFF* */
	SCCP_CONFIG_FLAG_IGNORE 			= 1 << 0,		/*< ignore parameter */
	SCCP_CONFIG_FLAG_NONE	 			= 1 << 1,		/*< ignore parameter */
	SCCP_CONFIG_FLAG_DEPRECATED			= 1 << 2,		/*< parameter is deprecated and should not be used anymore, warn user and still set variable */
	SCCP_CONFIG_FLAG_OBSOLETE			= 1 << 3,		/*< parameter is now obsolete warn user and skip */
	SCCP_CONFIG_FLAG_CHANGED			= 1 << 4,		/*< parameter implementation has changed, warn user */
	SCCP_CONFIG_FLAG_REQUIRED			= 1 << 5,		/*< parameter is required */
	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT		= 1 << 6,		/*< retrieve default value from device */
	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT		= 1 << 7,		/*< retrieve default value from global */
	SCCP_CONFIG_FLAG_MULTI_ENTRY			= 1 << 8,		/*< multi entries allowed */
/* *INDENT-ON* */
};

/*!
 * \brief SCCP Config Option Struct
 */
typedef struct SCCPConfigOption {
/* *INDENT-OFF* */
	const char *name;							/*!< Configuration Parameter Name */
	const int offset;							/*!< The offset relative to the context structure where the option value is stored. */
	const size_t size;							/*!< Structure size */
	enum SCCPConfigOptionType type;						/*!< Data type */
	sccp_value_changed_t(*converter_f) (void *dest, const size_t size, PBX_VARIABLE_TYPE *v, const sccp_config_segment_t segment);	/*!< Conversion function */
        sccp_enum_str2intval_t str2intval;
        sccp_enum_all_entries_t all_entries;
        const char *parsername;
	enum SCCPConfigOptionFlag flags;					/*!< Data type */
	sccp_configurationchange_t change;					/*!< Does a change of this value needs a device restart */
	const char *defaultValue;						/*!< Default value */
	const char *description;						/*!< Configuration description (config file) or warning message for deprecated or obsolete values */
/* *INDENT-ON* */
} SCCPConfigOption;

//converter function prototypes 
sccp_value_changed_t sccp_config_parse_codec_preferences(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_mailbox(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_tos(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_cos(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_amaflags(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_variables(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_group(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_deny_permit(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_button(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_permithosts(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_addons(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_privacyFeature(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_debug(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_ipaddress(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_port(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_context(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_context(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_exten(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_enable(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_force(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_log(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_checkButton(void *buttonconfig_head, int index, sccp_config_buttontype_t type, const char *name, const char *options, const char *args);

#include "sccp_config_entries.hh"

/*!
 * \brief SCCP Config Option Struct
 */
typedef struct SCCPConfigSegment {
	const char *name;
	const sccp_config_segment_t segment;
	const SCCPConfigOption *config;
	long unsigned int config_size;
} SCCPConfigSegment;

/*!
 * \brief SCCP Config Option Struct Initialization
 */
static const SCCPConfigSegment sccpConfigSegments[] = {
	{"general", SCCP_CONFIG_GLOBAL_SEGMENT, sccpGlobalConfigOptions, ARRAY_LEN(sccpGlobalConfigOptions)},
	{"device", SCCP_CONFIG_DEVICE_SEGMENT, sccpDeviceConfigOptions, ARRAY_LEN(sccpDeviceConfigOptions)},
	{"line", SCCP_CONFIG_LINE_SEGMENT, sccpLineConfigOptions, ARRAY_LEN(sccpLineConfigOptions)},
	{"softkey", SCCP_CONFIG_SOFTKEY_SEGMENT, sccpSoftKeyConfigOptions, ARRAY_LEN(sccpSoftKeyConfigOptions)},
};

/*!
 * \brief Find of SCCP Config Options
 */
//static const SCCPConfigOption *sccp_find_segment(const sccp_config_segment_t segment)
static const SCCPConfigSegment *sccp_find_segment(const sccp_config_segment_t segment)
{
	uint8_t i = 0;

	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
		if (sccpConfigSegments[i].segment == segment) {
			return &sccpConfigSegments[i];
		}
	}
	return NULL;
}

/*!
 * \brief Find of SCCP Config Options
 */
static const SCCPConfigOption *sccp_find_config(const sccp_config_segment_t segment, const char *name)
{
	long unsigned int i = 0;
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *config = sccpConfigSegment->config;

	char delims[] = "|";
	char *token = NULL;
	char *config_name = NULL;

	for (i = 0; i < sccpConfigSegment->config_size; i++) {
		if (strstr(config[i].name, delims) != NULL) {
			config_name = strdupa(config[i].name);
			token = strtok(config_name, delims);
			while (token != NULL) {
				if (!strcasecmp(token, name)) {
					return &config[i];
				}
				token = strtok(NULL, delims);
			}
		}
		if (!strcasecmp(config[i].name, name)) {
			return &config[i];
		}
	}

	return NULL;
}

/* Create new variable structure for Multi Entry Parameters */
static PBX_VARIABLE_TYPE *createVariableSetForMultiEntryParameters(PBX_VARIABLE_TYPE * cat_root, const char *configOptionName, PBX_VARIABLE_TYPE * out)
{
	PBX_VARIABLE_TYPE *v = cat_root;
	PBX_VARIABLE_TYPE *tmp = NULL;

	char delims[] = "|";
	char *token = NULL;
	char *option_name = alloca(strlen(configOptionName) + 1);

	sprintf(option_name, "%s%s", configOptionName, delims);							// add delims to string
	token = strtok(option_name, delims);
	while (token != NULL) {
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Token %s/%s\n", option_name, token);
		for (v = cat_root; v; v = v->next) {
			if (!strcasecmp(token, v->name)) {
				if (!tmp) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Create new variable set (%s=%s)\n", v->name, v->value);
					if (!(out = pbx_variable_new(v->name, v->value, ""))) {
						pbx_log(LOG_ERROR, "SCCP: (sccp_config) Error while creating new var structure\n");
						goto EXIT;
					}
					tmp = out;
				} else {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Add to variable set (%s=%s)\n", v->name, v->value);
					if (!(tmp->next = pbx_variable_new(v->name, v->value, ""))) {
						pbx_log(LOG_ERROR, "SCCP: (sccp_config) Error while creating new var structure\n");
						pbx_variables_destroy(out);
						goto EXIT;
					}
					tmp = tmp->next;
				}
			}
		}
		token = strtok(NULL, delims);
	}
EXIT:
	return out;
}

static PBX_VARIABLE_TYPE *createVariableSetForTokenizedDefault(const char *configOptionName, const char *defaultValue, PBX_VARIABLE_TYPE * out)
{
	PBX_VARIABLE_TYPE *tmp = NULL;

	char delims[] = "|";
	char *option_name_tokens = strdupa(configOptionName);
	char *option_value_tokens = strdupa(defaultValue);
	char *option_name_tokens_saveptr;
	char *option_value_tokens_saveptr;

	char *option_name = strtok_r(option_name_tokens, "|", &option_name_tokens_saveptr);
	char *option_value = strtok_r(option_value_tokens, "|", &option_value_tokens_saveptr);

	while (option_name != NULL && option_value != NULL) {
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Token %s/%s\n", option_name, option_value);
		if (!tmp) {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Create new variable set (%s=%s)\n", option_name, option_value);
			if (!(out = pbx_variable_new(option_name, option_value, ""))) {
				pbx_log(LOG_ERROR, "SCCP: (sccp_config) Error while creating new var structure\n");
				goto EXIT;
			}
			tmp = out;
		} else {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Add to variable set (%s=%s)\n", option_name, option_value);
			if (!(tmp->next = pbx_variable_new(option_name, option_value, ""))) {
				pbx_log(LOG_ERROR, "SCCP: (sccp_config) Error while creating new var structure\n");
				pbx_variables_destroy(out);
				goto EXIT;
			}
			tmp = tmp->next;
		}
		option_name = strtok_r(NULL, delims, &option_name_tokens_saveptr);
		option_value = strtok_r(NULL, delims, &option_value_tokens_saveptr);
	}
EXIT:
	return out;
}

/*!
 * \brief Parse SCCP Config Option Value
 *
 * \todo add errormsg return to sccpConfigOption->converter_f: so we can have a fixed format the returned errors to the user
 */
static sccp_configurationchange_t sccp_config_object_setValue(void *obj, PBX_VARIABLE_TYPE * cat_root, const char *name, const char *value, int lineno, const sccp_config_segment_t segment, boolean_t *SetEntries)
{
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *sccpConfigOption = sccp_find_config(segment, name);
	void *dst;
	int type;												/* enum wrapper */
	int flags;												/* enum wrapper */

	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;						/* indicates config value is changed or not */
	sccp_configurationchange_t changes = SCCP_CONFIG_NOUPDATENEEDED;

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: parsing %s parameter: %s %s%s%s (line: %d)\n", sccpConfigSegment->name, name, value ? "= '" : "", value ? value : "", value ? "' " : "", lineno);

	short int int8num;
	int int16num;
	long int int32num;
	long long int int64num;
	short unsigned int uint8num;
	unsigned int uint16num;
	long unsigned int uint32num;
	long long unsigned int uint64num;
	boolean_t boolean;
	char *str;
	char oldChar;
	char *tmp_value;

	if (!sccpConfigOption) {
		pbx_log(LOG_WARNING, "SCCP: Unknown param at %s:%d:%s='%s'\n", sccpConfigSegment->name, lineno, name, value);
		return SCCP_CONFIG_NOUPDATENEEDED;
	}

	if (sccpConfigOption->offset <= 0) {
		return SCCP_CONFIG_NOUPDATENEEDED;
	}

	dst = ((uint8_t *) obj) + sccpConfigOption->offset;
	type = sccpConfigOption->type;
	flags = sccpConfigOption->flags;

	// check if already set during first pass (multi_entry)
	if (sccpConfigOption->offset > 0 && SetEntries != NULL && ((flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)) {
		int y;

		for (y = 0; y < sccpConfigSegment->config_size; y++) {
			if (sccpConfigOption->offset == sccpConfigSegment->config[y].offset) {
				if (SetEntries[y] == TRUE) {
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_object_setValue) Set Entry[%d] = TRUE for MultiEntry %s -> SKIPPING\n", y, sccpConfigSegment->config[y].name);
					return SCCP_CONFIG_NOUPDATENEEDED;
				}
			}
		}
	}

	if ((flags & SCCP_CONFIG_FLAG_IGNORE) == SCCP_CONFIG_FLAG_IGNORE) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s='%s' in line %d ignored\n", name, value, lineno);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} else if ((flags & SCCP_CONFIG_FLAG_CHANGED) == SCCP_CONFIG_FLAG_CHANGED) {
		pbx_log(LOG_NOTICE, "SCCP: changed config param at %s='%s' in line %d\n - %s -> please check sccp.conf file\n", name, value, lineno, sccpConfigOption->description);
	} else if ((flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED && lineno > 0) {
		pbx_log(LOG_NOTICE, "SCCP: deprecated config param at %s='%s' in line %d\n - %s -> using old implementation\n", name, value, lineno, sccpConfigOption->description);
	} else if ((flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE && lineno > 0) {
		pbx_log(LOG_WARNING, "SCCP: obsolete config param at %s='%s' in line %d\n - %s -> param skipped\n", name, value, lineno, sccpConfigOption->description);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} else if ((flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) {
		if (NULL == value) {
			pbx_log(LOG_WARNING, "SCCP: required config param at %s='%s' - %s\n", name, value, sccpConfigOption->description);
			return SCCP_CONFIG_WARNING;
		}
	}

	/* rewrite to check change at the end */
	switch (type) {
		case SCCP_CONFIG_DATATYPE_CHAR:
			oldChar = *(char *) dst;

			if (!sccp_strlen_zero(value)) {
				if (oldChar != value[0]) {
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					*(char *) dst = value[0];
				}
			} else {
				if (oldChar != '\0') {
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					*(char *) dst = '\0';
				}
			}
			break;

		case SCCP_CONFIG_DATATYPE_STRING:
			str = (char *) dst;

			if (!sccp_strlen_zero(value)) {
				if (strlen(value) > sccpConfigOption->size - 1) {
					pbx_log(LOG_NOTICE, "SCCP: config parameter %s:%s value '%s' is too long, only using the first %d characters\n", sccpConfigSegment->name, name, value, (int) sccpConfigOption->size - 1);
				}
				if (strncasecmp(str, value, sccpConfigOption->size - 1)) {
					if (GLOB(reload_in_progress)) {
						sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s '%s' != '%s'\n", name, str, value);
					}
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					pbx_copy_string(dst, value, sccpConfigOption->size);
				}
			} else if (!sccp_strlen_zero(str)) {
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				pbx_copy_string(dst, "", sccpConfigOption->size);
			}
			break;

		case SCCP_CONFIG_DATATYPE_STRINGPTR:
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
			str = *(void **) dst;

			if (!sccp_strlen_zero(value)) {
				if (str) {
					if (strcasecmp(str, value)) {
						changed = SCCP_CONFIG_CHANGE_CHANGED;
						/* there is a value already, free it */
						sccp_free(str);
						*(void **) dst = strdup(value);
					}
				} else {
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					*(void **) dst = strdup(value);
				}

			} else if (str) {
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				/* there is a value already, free it */
				sccp_free(str);
				if (value == NULL) {
					*(void **) dst = NULL;
				} else {
					*(void **) dst = strdup(value);
				}
			}
			break;

		case SCCP_CONFIG_DATATYPE_INT:
			if (sccp_strlen_zero(value)) {
				tmp_value = strdupa("0");
			} else {
				tmp_value = strdupa(value);
			}

			switch (sccpConfigOption->size) {
				case 1:
					if (sscanf(tmp_value, "%hd", &int8num) == 1) {
						if ((*(int8_t *) dst) != int8num) {
							*(int8_t *) dst = int8num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 2:
					if (sscanf(tmp_value, "%d", &int16num) == 1) {
						if ((*(int16_t *) dst) != int16num) {
							*(int16_t *) dst = int16num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 4:
					if (sscanf(tmp_value, "%ld", &int32num) == 1) {
						if ((*(int32_t *) dst) != int32num) {
							*(int32_t *) dst = int32num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 8:
					if (sscanf(tmp_value, "%lld", &int64num) == 1) {
						if ((*(int64_t *) dst) != int64num) {
							*(int64_t *) dst = int64num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
			}
			break;
		case SCCP_CONFIG_DATATYPE_UINT:
			if (sccp_strlen_zero(value)) {
				tmp_value = strdupa("0");
			} else {
				tmp_value = strdupa(value);
			}
			switch (sccpConfigOption->size) {
				case 1:
					if ((!strncmp("0x", tmp_value, 2) && sscanf(tmp_value, "%hx", &uint8num)) || (sscanf(tmp_value, "%hu", &uint8num) == 1)) {
						if ((*(uint8_t *) dst) != uint8num) {
							*(uint8_t *) dst = uint8num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 2:
					if (sscanf(tmp_value, "%u", &uint16num) == 1) {
						if ((*(uint16_t *) dst) != uint16num) {
							*(uint16_t *) dst = uint16num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 4:
					if (sscanf(tmp_value, "%lu", &uint32num) == 1) {
						if ((*(uint32_t *) dst) != uint32num) {
							*(uint32_t *) dst = uint32num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 8:
					if (sscanf(tmp_value, "%llu", &uint64num) == 1) {
						if ((*(uint64_t *) dst) != uint64num) {
							*(uint64_t *) dst = uint64num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
			}
			break;

		case SCCP_CONFIG_DATATYPE_BOOLEAN:
			if (sccp_strlen_zero(value)) {
				boolean = FALSE;
			} else {
				boolean = sccp_true(value);
			}

			if (*(boolean_t *) dst != boolean) {
				*(boolean_t *) dst = sccp_true(value);
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
			break;

		case SCCP_CONFIG_DATATYPE_PARSER:
			{
				/* MULTI_ENTRY can only be parsed by a specific datatype_parser at this moment */
				if (sccpConfigOption->converter_f) {
					PBX_VARIABLE_TYPE *new_var = NULL;

					if (cat_root) {								/* convert cat_root or referral to new PBX_VARIABLE_TYPE */
						new_var = createVariableSetForMultiEntryParameters(cat_root, sccpConfigOption->name, new_var);
					} else {
						if (strstr(value, "|")) {					/* convert multi-token default value to PBX_VARIABLE_TYPE */
							new_var = createVariableSetForTokenizedDefault(name, value, new_var);
						} else {							/* convert simple single value -> PBX_VARIABLE_TYPE */
							new_var = ast_variable_new(name, value, "");
						}
					}
					if (new_var) {
						changed = sccpConfigOption->converter_f(dst, sccpConfigOption->size, new_var, segment);
						pbx_variables_destroy(new_var);
					}
				}
			}
			break;
		case SCCP_CONFIG_DATATYPE_ENUM:
			{
				int enumValue = 0;
				if (!sccp_strlen_zero(value)) {
					if ((enumValue = sccpConfigOption->str2intval(value)) != -1) {
						sccp_log(DEBUGCAT_HIGH) ("SCCP: Parse Other Value: %s -> %d\n", value, enumValue);
					} else {
						char *all_entries = sccpConfigOption->all_entries();
						if (sccp_true(value)) {
							if (strcasestr(all_entries, "On")) {
								enumValue = sccpConfigOption->str2intval("On");
							} else if (strcasestr(all_entries, "Yes")) {
								enumValue = sccpConfigOption->str2intval("Yes");
							} else if (strcasestr(all_entries, "True")) {
								enumValue = sccpConfigOption->str2intval("True");
							}
						} else if (!sccp_true(value)) {
							if (strcasestr(all_entries, "Off")) {
								enumValue = sccpConfigOption->str2intval("Off");
							} else if (strcasestr(all_entries, "No")) {
								enumValue = sccpConfigOption->str2intval("No");
							} else if (strcasestr(all_entries, "False")) {
								enumValue = sccpConfigOption->str2intval("False");
							}
						}
					}
					if (enumValue != -1) {
						if (*(int *) dst != enumValue) {
							*(int *) dst = enumValue;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					} else {
						pbx_log(LOG_NOTICE, "SCCP: Invalid value '%s' for [%s]->%s. Allowed: [%s]\n", value, sccpConfigSegment->name, name, sccpConfigOption->all_entries());
						changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
					}
					break;
				}
				pbx_log(LOG_WARNING, "SCCP: [%s]=>%s cannot be ''. Allowed: [%s]\n", sccpConfigSegment->name, name, sccpConfigOption->all_entries());
				changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
			}
			break;

		case SCCP_CONFIG_BUTTONTYPE_SENTINEL:
			pbx_log(LOG_WARNING, "SCCP: Unknown param at %s='%s'\n", name, value);
			return SCCP_CONFIG_NOUPDATENEEDED;
	}

	if (SCCP_CONFIG_CHANGE_CHANGED == changed) {
		if (GLOB(reload_in_progress)) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s='%s' in line %d changed. %s\n", name, value, lineno, SCCP_CONFIG_NEEDDEVICERESET == sccpConfigOption->change ? "(causes device reset)" : "");
		}
		changes = sccpConfigOption->change;
	}

	if (SCCP_CONFIG_CHANGE_INVALIDVALUE != changed || ((flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)) {	/* Multi_Entry could give back invalid for one of it's values */
		/* if SetEntries is provided lookup the first offset of the struct variable we have set and note the index in SetEntries by changing the boolean_t to TRUE */
		if (sccpConfigOption->offset > 0 && SetEntries != NULL) {
			int x;

			for (x = 0; x < sccpConfigSegment->config_size; x++) {
				if (sccpConfigOption->offset == sccpConfigSegment->config[x].offset) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_object_setValue) Set Entry[%d] = TRUE for %s\n", x, sccpConfigSegment->config[x].name);
					SetEntries[x] = TRUE;
				}
			}
		}
	}
	if (SCCP_CONFIG_CHANGE_INVALIDVALUE == changed) {
		pbx_log(LOG_NOTICE, "SCCP: Option Description: %s\n", sccpConfigOption->description);
	}
	return changes;
}

/*!
 * \brief Set SCCP obj defaults from predecessor (device / global)
 *
 * check if we can find the param name in the segment specified and retrieving its value or default value
 * copy the string from the defaultSegment and run through the converter again
 */
static void sccp_config_set_defaults(void *obj, const sccp_config_segment_t segment, boolean_t *SetEntries)
{
	if (!GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "GLOB(cfg) not available. Skip loading default setting.\n");
		return;
	}
	/* destination segment */
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);

	/* destination/source config */
	const SCCPConfigOption *sccpDstConfig = sccpConfigSegment->config;
	const SCCPConfigOption *sccpDefaultConfigOption;

	/* segment referral for default values  */
	sccp_device_t *referral_device = NULL;									/* need to find a way to find the default device to copy */
	char *referral_cat = "";
	sccp_config_segment_t search_segment_type;
	boolean_t referralValueFound = FALSE;

	// already Set
	int skip_elem;
	boolean_t skip = FALSE;

	/* find the defaultValue, first check the reference, if no reference is specified, us the local defaultValue */
	uint8_t cur_elem = 0;

	for (cur_elem = 0; cur_elem < sccpConfigSegment->config_size; cur_elem++) {
		/* Lookup the first offset of the struct variable we want to set default for, find the corresponding entry in the SetEntries array and check the boolean flag, skip if true */
		skip = FALSE;
		for (skip_elem = 0; skip_elem < sccpConfigSegment->config_size; skip_elem++) {
			if (sccpDstConfig[cur_elem].offset == sccpConfigSegment->config[skip_elem].offset && (SetEntries[skip_elem] || sccpConfigSegment->config[cur_elem].flags & (SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_OBSOLETE))) {
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "SCCP: (sccp_config_set_defaults) skip setting default (SetEntry[%d] = TRUE for %s)\n", skip_elem, sccpConfigSegment->config[skip_elem].name);
				skip = TRUE;
				break;
			}
		}
		if (skip) {											// skip default value if already set
			continue;
		}
		int flags = sccpDstConfig[cur_elem].flags;
		int type = sccpDstConfig[cur_elem].type;

		if (((flags & SCCP_CONFIG_FLAG_OBSOLETE) != SCCP_CONFIG_FLAG_OBSOLETE)) {			// has not been set already and is not obsolete
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "parsing %s parameter %s looking for defaultValue (flags: %d, type: %d)\n", sccpConfigSegment->name, sccpDstConfig[cur_elem].name, flags, type);

			/* check if referring to another segment, or ourself */
			if ((flags & SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT) == SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT) {	/* get default value from device */
				referral_device = &(*(sccp_device_t *) obj);
				referral_cat = strdupa(referral_device->id);
				search_segment_type = SCCP_CONFIG_DEVICE_SEGMENT;

			} else if ((flags & SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT) == SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT) {	/* get default value from global */
				referral_cat = "general";
				search_segment_type = SCCP_CONFIG_GLOBAL_SEGMENT;

			} else {										/* get default value from our own segment */
				referral_cat = NULL;
				search_segment_type = segment;
			}

			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter:'%s' defaultValue %s lookup %s%s\n", sccpDstConfig[cur_elem].name, referral_cat ? "referred" : "direct", referral_cat ? "via " : "", referral_cat ? referral_cat : "");

			/* check to see if there is a default value to be found in the config file within referred segment */
			if (referral_cat) {
				/* tokenize all option parameters */
				PBX_VARIABLE_TYPE *v;
				PBX_VARIABLE_TYPE *cat_root;

				referralValueFound = FALSE;

				/* tokenparsing */
				char *option_tokens = alloca(strlen(sccpDstConfig[cur_elem].name) + 1);

				sprintf(option_tokens, "%s|", sccpDstConfig[cur_elem].name);
				char *option_tokens_saveptr = NULL;
				char *option_name = strtok_r(option_tokens, "|", &option_tokens_saveptr);

				do {
					/* search for the default values in the referred segment, if found break so we can pass on the cat_root */
					for (cat_root = v = ast_variable_browse(GLOB(cfg), referral_cat); v; v = v->next) {
						if (!strcasecmp((const char *) option_name, v->name)) {
							sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "Found name:'%s', value:'%s', use referred config-file value from segment '%s'\n", option_name, v->value, referral_cat);
							referralValueFound = TRUE;
							break;
						}
					}
				} while ((option_name = strtok_r(NULL, "|", &option_tokens_saveptr)) != NULL);

				if (referralValueFound && v) {							/* if referred to other segment and a value was found, pass the newly found cat_root directly to setValue */
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Refer default value lookup for parameter:'%s' through '%s' segment\n", sccpDstConfig[cur_elem].name, referral_cat);
					sccp_config_object_setValue(obj, cat_root, sccpDstConfig[cur_elem].name, v->value, __LINE__, segment, SetEntries);
					continue;
				} else {									/* if referred but no default value was found, pass on the defaultValue of the referred segment in raw string form (including tokens) */
					sccpDefaultConfigOption = sccp_find_config(search_segment_type, sccpDstConfig[cur_elem].name);
					if (sccpDefaultConfigOption && !sccp_strlen_zero(sccpDefaultConfigOption->defaultValue)) {
						sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Set parameter '%s' to segment default, being:'%s'\n", sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue);
						sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, sccpDefaultConfigOption->defaultValue, __LINE__, segment, SetEntries);
						continue;
					}
				}
			} else if (!sccp_strlen_zero(sccpDstConfig[cur_elem].defaultValue)) {			/* Non-Referral, pass defaultValue on in raw string format (including tokens) */
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Set parameter '%s' to own default, being:'%s'\n", sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue);
				sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue, __LINE__, segment, SetEntries);
				continue;
			}

			if (type == SCCP_CONFIG_DATATYPE_STRINGPTR) {						/* If nothing was found, clear variable, incase of a STRINGPTR */
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Clearing parameter %s\n", sccpDstConfig[cur_elem].name);
				sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, "", __LINE__, segment, SetEntries);
			}
		}
	}
}

/*
 * \brief automatically free all STRINGPTR objects allocated during config parsing and object initialization
 *
 * \note This makes it easier to change character arrays to stringptr in structs, without having to think about cleaning up after them.
 *
 */
void sccp_config_cleanup_dynamically_allocated_memory(void *obj, const sccp_config_segment_t segment)
{
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *sccpConfigOption = sccpConfigSegment->config;
	void *dst;
	char *str;

	uint8_t i = 0;

	for (i = 0; i < sccpConfigSegment->config_size; i++) {
		if (sccpConfigOption[i].type == SCCP_CONFIG_DATATYPE_STRINGPTR) {
			dst = ((uint8_t *) obj) + sccpConfigOption[i].offset;
			str = *(void **) dst;
			if (str) {
				//pbx_log(LOG_NOTICE, "SCCP: Freeing %s='%s'\n", sccpConfigOption[i].name, str);
				sccp_free(str);
				str = NULL;
			}
		}
	}
}

/*!
 * \brief Config Converter/Parser for Bind Address
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_ipaddress(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);

#if ASTERISK_VERSION_GROUP == 106
	if (sccp_strequals(value, "::")) {
		pbx_log(LOG_ERROR, "Asterisk 1.6, does not support ipv6, '::' has been replaced with '0.0.0.0'\n");
		value = strdupa("::");
	}
#endif
	if (sccp_strlen_zero(value)) {
		value = strdupa("0.0.0.0");
	}
	struct sockaddr_storage bindaddr_prev = (*(struct sockaddr_storage *) dest);
	struct sockaddr_storage bindaddr_new = { 0, };

	if (!sccp_sockaddr_storage_parse(&bindaddr_new, value, PARSE_PORT_FORBID)) {
		pbx_log(LOG_WARNING, "Invalid IP address: %s\n", value);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if (sccp_socket_cmp_addr(&bindaddr_prev, &bindaddr_new)) {					// 0 = equal
			memcpy(&(*(struct sockaddr_storage *) dest), &bindaddr_new, sizeof(bindaddr_new));
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Port
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_port(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);

	int new_port;
	struct sockaddr_storage bindaddr_storage_prev = (*(struct sockaddr_storage *) dest);

	if (sscanf(value, "%i", &new_port) == 1) {
		if (bindaddr_storage_prev.ss_family == AF_INET) {
			struct sockaddr_in bindaddr_prev = (*(struct sockaddr_in *) dest);

			if (bindaddr_prev.sin_port != 0) {
				if (bindaddr_prev.sin_port != htons(new_port)) {
					(*(struct sockaddr_in *) dest).sin_port = htons(new_port);
					changed = SCCP_CONFIG_CHANGE_CHANGED;
				}
			} else {
				(*(struct sockaddr_in *) dest).sin_port = htons(new_port);
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
		} else if (bindaddr_storage_prev.ss_family == AF_INET6) {
			struct sockaddr_in6 bindaddr_prev = (*(struct sockaddr_in6 *) dest);

			if (bindaddr_prev.sin6_port != 0) {
				if (bindaddr_prev.sin6_port != htons(new_port)) {
					(*(struct sockaddr_in6 *) dest).sin6_port = htons(new_port);
					changed = SCCP_CONFIG_CHANGE_CHANGED;
				}
			} else {
				(*(struct sockaddr_in6 *) dest).sin6_port = htons(new_port);
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
		} else {
			pbx_log(LOG_WARNING, "Invalid address in bindaddr to set port to '%s'\n", value);
			changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	} else {
		pbx_log(LOG_WARNING, "Invalid port number '%s'\n", value);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for privacyFeature
 *
 * \todo malloc/calloc of privacyFeature necessary ?
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_privacyFeature(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	sccp_featureConfiguration_t privacyFeature;								// = malloc(sizeof(sccp_featureConfiguration_t));

	if (sccp_strcaseequals(value, "full")) {
		privacyFeature.status = ~0;
		privacyFeature.enabled = TRUE;
	} else if (sccp_true(value) || !sccp_true(value)) {
		privacyFeature.status = 0;
		privacyFeature.enabled = sccp_true(value);
	} else {
		pbx_log(LOG_WARNING, "Invalid privacy value, should be 'full', 'on' or 'off'\n");
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (privacyFeature.status != (*(sccp_featureConfiguration_t *) dest).status || privacyFeature.enabled != (*(sccp_featureConfiguration_t *) dest).enabled) {
		*(sccp_featureConfiguration_t *) dest = privacyFeature;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for mwilamp
 *
 * \note not multi_entry
 */
/*
   sccp_value_changed_t sccp_config_parse_mwilamp(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
   {
   sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
   char *value = strdupa(v->value);
   skinny_lampmode_t mwilamp = SKINNY_LAMP_OFF;

   if (sccp_strcaseequals(value, "wink")) {
   mwilamp = SKINNY_LAMP_WINK;
   } else if (sccp_strcaseequals(value, "flash")) {
   mwilamp = SKINNY_LAMP_FLASH;
   } else if (sccp_strcaseequals(value, "blink")) {
   mwilamp = SKINNY_LAMP_BLINK;
   } else if (!sccp_true(value)) {
   mwilamp = SKINNY_LAMP_OFF;
   } else if (sccp_true(value)) {
   mwilamp = SKINNY_LAMP_ON;
   } else {
   pbx_log(LOG_WARNING, "Invalid mwilamp value, should be one of 'off', 'on', 'wink', 'flash' or 'blink'\n");
   changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
   }

   if (*(skinny_lampmode_t *) dest != mwilamp) {
   *(skinny_lampmode_t *) dest = mwilamp;
   changed = SCCP_CONFIG_CHANGE_CHANGED;
   }
   return changed;
   }
 */

/*!
 * \brief Config Converter/Parser for Tos Value
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_tos(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	unsigned int tos;

	if (!pbx_str2tos(value, &tos)) {
		/* value is tos */
	} else if (sscanf(value, "%i", &tos) == 1) {
		tos = tos & 0xff;
	} else if (sccp_strcaseequals(value, "lowdelay")) {
		tos = IPTOS_LOWDELAY;
	} else if (sccp_strcaseequals(value, "throughput")) {
		tos = IPTOS_THROUGHPUT;
	} else if (sccp_strcaseequals(value, "reliability")) {
		tos = IPTOS_RELIABILITY;

#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
	} else if (sccp_strcaseequals(value, "mincost")) {
		tos = IPTOS_MINCOST;
#endif
	} else if (sccp_strcaseequals(value, "none")) {
		tos = 0;
	} else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
#else
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
#endif
		tos = 0x68 & 0xff;
	}

	if ((*(unsigned int *) dest) != tos) {
		*(unsigned int *) dest = tos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Cos Value
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_cos(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	unsigned int cos;

	if (sscanf(value, "%d", &cos) == 1) {
		if (cos > 7) {
			pbx_log(LOG_WARNING, "Invalid cos %d value, refer to QoS documentation\n", cos);
			return SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	}

	if ((*(unsigned int *) dest) != cos) {
		*(unsigned int *) dest = cos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for AmaFlags Value
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_amaflags(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	int amaflags;

	amaflags = pbx_channel_string2amaflag(value);

	if (amaflags < 0) {
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if ((*(int *) dest) != amaflags) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			*(int *) dest = amaflags;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Secundairy Dialtone Digits
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	char *str = (char *) dest;

	if (strlen(value) <= 9) {
		if (!sccp_strcaseequals(str, value)) {
			sccp_copy_string(str, value, 9);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
	} else {
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for Callgroup/Pickupgroup Values
 *
 * \todo check changes to make the function more generic
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_group(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);

	char *piece;
	char *c;
	int start = 0, finish = 0, x;
	sccp_group_t group = 0;

	if (!sccp_strlen_zero(value)) {
		c = sccp_strdupa(value);

		while ((piece = strsep(&c, ","))) {
			if (sscanf(piece, "%30d-%30d", &start, &finish) == 2) {
				/* Range */
			} else if (sscanf(piece, "%30d", &start)) {
				/* Just one */
				finish = start;
			} else {
				pbx_log(LOG_ERROR, "Syntax error parsing group configuration '%s' at '%s'. Ignoring.\n", value, piece);
				continue;
			}
			for (x = start; x <= finish; x++) {
				if ((x > 63) || (x < 0)) {
					pbx_log(LOG_WARNING, "Ignoring invalid group %d (maximum group is 63)\n", x);
				} else {
					group |= ((ast_group_t) 1 << x);
				}
			}
		}
	}
#if defined(HAVE_UNALIGNED_BUSERROR)										// for example sparc64
	sccp_group_t group_orig = 0;

	memcpy(&group_orig, dest, sizeof(sccp_group_t));
	if (group_orig != group) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		memcpy(dest, &group, sizeof(sccp_group_t));
	}
#else
	if ((*(sccp_group_t *) dest) != group) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		*(sccp_group_t *) dest = group;
	}
#endif
	return changed;
}

/*!
 * \brief Config Converter/Parser for Context
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_context(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	char *str = (char *) dest;

	if (!sccp_strcaseequals(str, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(dest, value, size);
		if (!sccp_strlen_zero(value) && !pbx_context_find((const char *) dest)) {
			pbx_log(LOG_WARNING, "The context '%s' you specified might not be available in the dialplan. Please check the sccp.conf\n", (char *) dest);
		}
	} else {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Context
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_hotline_context(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	sccp_hotline_t *hotline = *(sccp_hotline_t **) dest;

	if (!sccp_strcaseequals(hotline->line->context, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(hotline->line->context, value, size);
	} else {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Extension
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_hotline_exten(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = strdupa(v->value);
	sccp_hotline_t *hotline = *(sccp_hotline_t **) dest;

	if (!sccp_strcaseequals(hotline->exten, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(hotline->exten, value, size);
		if (hotline->line) {
			sccp_copy_string(hotline->line->adhocNumber, value, sizeof(hotline->line->adhocNumber));
		}
	} else {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Jitter Buffer Flags
 *
 * \note not multi_entry
 */
static sccp_value_changed_t sccp_config_parse_jbflags(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment, const unsigned int flag)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	struct ast_jb_conf jb = *(struct ast_jb_conf *) dest;

	if (pbx_test_flag(&jb, flag) != (unsigned) ast_true(value)) {
		pbx_set2_flag(&GLOB(global_jbconf), ast_true(value), flag);
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

sccp_value_changed_t sccp_config_parse_jbflags_enable(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = strdupa(v->value);

	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_ENABLED);
}

sccp_value_changed_t sccp_config_parse_jbflags_force(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = strdupa(v->value);

	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_FORCED);
}

sccp_value_changed_t sccp_config_parse_jbflags_log(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = strdupa(v->value);

	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_LOG);
}

/*!
 * \brief Config Converter/Parser for Debug
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_debug(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	uint32_t debug_new = 0;

	char *debug_arr[1];

	for (; v; v = v->next) {
		debug_arr[0] = strdupa(v->value);
		debug_new = sccp_parse_debugline(debug_arr, 0, 1, debug_new);
	}
	if (*(uint32_t *) dest != debug_new) {
		*(uint32_t *) dest = debug_new;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Codec Preferences
 *
 * \todo need check to see if preferred_codecs has changed
 * \todo do we need to reduce the preferences by the pbx -> skinny codec mapping ?
 *
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_codec_preferences(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	skinny_codec_t *codecs = (skinny_codec_t *) dest;
	skinny_codec_t new_codecs[SKINNY_MAX_CAPABILITIES] = { 0 };
	int errors = 0;

	for (; v; v = v->next) {
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("sccp_config_parse_codec preference: name: %s, value:%s\n", v->name, v->value);
		if (sccp_strcaseequals(v->name, "disallow")) {
			errors += sccp_parse_allow_disallow(new_codecs, v->value, 0);
		} else if (sccp_strcaseequals(v->name, "allow")) {
			errors += sccp_parse_allow_disallow(new_codecs, v->value, 1);
		} else {
			errors += 1;
		}
	}
	if (errors) {
		pbx_log(LOG_NOTICE, "SCCP: (parse_codec preference) Error occured during parsing of the disallowed / allowed codecs\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if (memcmp(codecs, new_codecs, SKINNY_MAX_CAPABILITIES * sizeof(skinny_codec_t))) {
			memcpy(codecs, new_codecs, SKINNY_MAX_CAPABILITIES * sizeof(skinny_codec_t));
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		} else {
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
		}
	}
	return changed;

}

/*!
 * \brief Config Converter/Parser for Deny IP
 *
 * \todo need check to see if ha has changed
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_deny_permit(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int error = 0;
	int errors = 0;

	struct sccp_ha *prev_ha = *(struct sccp_ha **) dest;
	struct sccp_ha *ha = NULL;

	for (; v; v = v->next) {
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("sccp_config_parse_deny_permit: name: %s, value:%s\n", v->name, v->value);
		if (sccp_strcaseequals(v->name, "deny")) {
			ha = sccp_append_ha("deny", v->value, ha, &error);
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Deny: %s\n", v->value);
		} else if (sccp_strcaseequals(v->name, "permit") || sccp_strcaseequals(v->name, "localnet")) {
			if (sccp_strcaseequals(v->value, "internal")) {
				ha = sccp_append_ha("permit", "127.0.0.0/255.0.0.0", ha, &error);
				errors |= error;
				ha = sccp_append_ha("permit", "10.0.0.0/255.0.0.0", ha, &error);
				errors |= error;
				ha = sccp_append_ha("permit", "172.16.0.0/255.224.0.0", ha, &error);
				errors |= error;
				ha = sccp_append_ha("permit", "192.168.0.0/255.255.0.0", ha, &error);
			} else {
				ha = sccp_append_ha("permit", v->value, ha, &error);
			}
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Permit: %s\n", v->value);
		}
		errors |= error;
	}
	if (!error) {
		// compare ha elements
		struct ast_str *ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

		sccp_print_ha(ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, ha);
		struct ast_str *prev_ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

		sccp_print_ha(prev_ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, prev_ha);

		if (!sccp_strequals(pbx_str_buffer(ha_buf), pbx_str_buffer(prev_ha_buf))) {
			sccp_log_and(DEBUGCAT_CONFIG + DEBUGCAT_HIGH) ("hal: %s\nprev_ha: %s\n", pbx_str_buffer(ha_buf), pbx_str_buffer(prev_ha_buf));
			if (prev_ha) {
				sccp_free_ha(prev_ha);
			}
			*(struct sccp_ha **) dest = ha;
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		} else {
			if (ha) {
				sccp_free_ha(ha);
			}
		}
	} else {
		sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_parse_deny_permit) Invalid\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
		if (ha) {
			sccp_free_ha(ha);
		}
	}
	sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_parse_deny_permit) Return: %d\n", changed);
	return changed;
}

/*!
 * \brief Config Converter/Parser for Permit Hosts
 *
 * \todo maybe add new DATATYPE_LIST to add string param value to LIST_HEAD to make a generic parser
 * \todo need check to see if  has changed
 *
 * \note multi_entry
 * \note order is irrelevant
 */
sccp_value_changed_t sccp_config_parse_permithosts(void *dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_hostname_t *permithost = NULL;

	SCCP_LIST_HEAD (, sccp_hostname_t) * permithostList = dest;

	PBX_VARIABLE_TYPE *v = NULL;
	int listCount = SCCP_LIST_GETSIZE(permithostList);
	int varCount = 0;
	int found = 0;

	for (v = vroot; v; v = v->next) {
		SCCP_LIST_TRAVERSE(permithostList, permithost, list) {
			if (sccp_strcaseequals(permithost->name, v->value)) {					// variable found
				found++;
				break;
			}
		}
		varCount++;
	}
	if (listCount != varCount || listCount != found) {							// build new list
		while ((permithost = SCCP_LIST_REMOVE_HEAD(permithostList, list))) {				// clear list
			sccp_free(permithost);
		}
		for (v = vroot; v; v = v->next) {
			if (!(permithost = sccp_calloc(1, sizeof(sccp_hostname_t)))) {
				pbx_log(LOG_ERROR, "SCCP: Unable to allocate memory for a new permithost\n");
				break;
			}
			sccp_copy_string(permithost->name, v->value, sizeof(permithost->name));
			SCCP_LIST_INSERT_TAIL(permithostList, permithost, list);
		}
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

static int addonstr2enum(const char *addonstr)
{
	if (sccp_strcaseequals(addonstr, "7914")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7914;
	} else if (sccp_strcaseequals(addonstr, "7915")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON;
	} else if (sccp_strcaseequals(addonstr, "7916")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON;
	} else if (sccp_strcaseequals(addonstr, "500S")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S;
	} else if (sccp_strcaseequals(addonstr, "500DS")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS;
	} else if (sccp_strcaseequals(addonstr, "932DS")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS;
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Unknown addon type (%s)\n", addonstr);
		return 0;
	}
}

/*!
 * \brief Config Converter/Parser for addons
 * 
 * \todo make more generic
 * \todo cleanup original implementation in sccp_utils.c
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_addons(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_addon_t *addon = NULL;
	int addon_type;

	SCCP_LIST_HEAD (, sccp_addon_t) * addonList = dest;

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(addonList, addon, list) {
		if (v) {
			if (!sccp_strlen_zero(v->value)) {
				if ((addon_type = addonstr2enum(v->value))) {
					if (addon->type != addon_type) {					/* change/update */
						sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("change addon: %d => %d\n", addon->type, addon_type);
						addon->type = addon_type;
						changed |= SCCP_CONFIG_CHANGE_CHANGED;
					}
				} else {
					changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
				}
			}
			v = v->next;
		} else {											/* removal */
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("remove addon: %d\n", addon->type);
			SCCP_LIST_REMOVE_CURRENT(list);
			sccp_free(addon);
			changed |= SCCP_CONFIG_CHANGE_CHANGED;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;

	int addon_counter = 0;

	for (; v; v = v->next) {										/* addition */
		if (2 > addon_counter++) {
			if (!sccp_strlen_zero(v->value)) {
				if ((addon_type = addonstr2enum(v->value))) {
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("add new addon: %d\n", addon_type);
					if (!(addon = sccp_calloc(1, sizeof(sccp_addon_t)))) {
						pbx_log(LOG_ERROR, "SCCP: Unable to allocate memory for a new addon\n");
						break;
					}
					addon->type = addon_type;
					SCCP_LIST_INSERT_TAIL(addonList, addon, list);
					changed |= SCCP_CONFIG_CHANGE_CHANGED;
				} else {
					changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
				}
			}
		} else {
			pbx_log(LOG_ERROR, "SCCP: maximum number(2) of addon's has been reached\n");
			changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Mailbox Value
 *
 * \note multi_entry
 * \note order is irrelevant
 */
sccp_value_changed_t sccp_config_parse_mailbox(void *dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_mailbox_t *mailbox = NULL;
	char *context, *mbox = NULL;

	SCCP_LIST_HEAD (, sccp_mailbox_t) * mailboxList = dest;

	PBX_VARIABLE_TYPE *v = NULL;
	int varCount = 0;
	int listCount = 0;

	listCount = mailboxList->size;
	boolean_t notfound = FALSE;

	for (v = vroot; v; v = v->next) {
		varCount++;
	}
	if (varCount == listCount) {										// list length equal
		SCCP_LIST_TRAVERSE(mailboxList, mailbox, list) {
			for (v = vroot; v; v = v->next) {
				mbox = context = sccp_strdupa(v->value);
				strsep(&context, "@");
				if (sccp_strlen_zero(context)) {
					context = "default";
				}
				if (sccp_strcaseequals(mailbox->mailbox, mbox) && sccp_strcaseequals(mailbox->context, context)) {	// variable found
					continue;
				}
				notfound |= TRUE;
			}
		}
	}
	if (varCount != listCount || notfound) {								// build new list
		while ((mailbox = SCCP_LIST_REMOVE_HEAD(mailboxList, list))) {					// clear list
			sccp_free(mailbox->mailbox);
			sccp_free(mailbox->context);
			sccp_free(mailbox);
		}
		for (v = vroot; v; v = v->next) {								// create new list
			mbox = context = sccp_strdupa(v->value);
			strsep(&context, "@");
			if (sccp_strlen_zero(context)) {
				context = "default";
			}
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("add new mailbox: %s@%s\n", mbox, context);
			if (!(mailbox = sccp_calloc(1, sizeof(sccp_mailbox_t)))) {
				pbx_log(LOG_ERROR, "SCCP: Unable to allocate memory for a new mailbox\n");
				break;
			}
			mailbox->mailbox = strdup(mbox);
			mailbox->context = strdup(context);
			SCCP_LIST_INSERT_TAIL(mailboxList, mailbox, list);
		}
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for PBX Variables
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_variables(void *dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	PBX_VARIABLE_TYPE *variableList = *(PBX_VARIABLE_TYPE **) dest;

	if (variableList) {
		pbx_variables_destroy(variableList);								/* destroy old list */
		variableList = NULL;
	}
	PBX_VARIABLE_TYPE *variable = variableList;
	char *var_name = NULL;
	char *var_value = NULL;

	for (; v; v = v->next) {										/* create new list */
		var_name = sccp_strdupa(v->value);
		var_value = NULL;
		if ((var_value = strchr(var_name, '='))) {
			*var_value++ = '\0';
		}
		if (!sccp_strlen_zero(var_name)) {
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("add new variable: %s=%s\n", var_name, var_value);
			if (!variable) {
				if (!(variableList = pbx_variable_new(var_name, var_value, ""))) {
					pbx_log(LOG_ERROR, "SCCP: Unable to allocate memory for a new variable\n");
					variableList = NULL;
					break;
				}
				variable = variableList;
			} else {
				if (!(variable->next = pbx_variable_new(var_name, var_value, ""))) {
					pbx_log(LOG_ERROR, "SCCP: Unable to allocate memory for a new variable\n");
					pbx_variables_destroy(variableList);
					variableList = NULL;
					break;
				}
				variable = variable->next;
			}
		}
	}
	*(PBX_VARIABLE_TYPE **) dest = variableList;

	return changed;
}

/*!
 * \brief Config Converter/Parser for Buttons
 *
 * \todo Build a check to see if the button has changed 
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_button(void *dest, const size_t size, PBX_VARIABLE_TYPE * vars, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_CHANGED;

	/* parse all button definitions in one pass */
	char *buttonType = NULL, *buttonName = NULL, *buttonOption = NULL, *buttonArgs = NULL;
	char k_button[256];
	char *splitter;
	sccp_config_buttontype_t type = EMPTY;									/* default to empty */
	unsigned int i;
	int buttonindex = 0;
	
	SCCP_LIST_HEAD (, sccp_buttonconfig_t) * buttonconfigList = dest;
	sccp_buttonconfig_t *config = NULL;
	PBX_VARIABLE_TYPE * first_var = vars;
	PBX_VARIABLE_TYPE * v = NULL;

	if (GLOB(reload_in_progress)) {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "SCCP: Checking Button Config\n");
		/* check if the number of buttons got reduced */
		for (v = first_var; v; v = v->next) {										/* check buttons against currently loaded set*/
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Checking button: %s\n", v->value);
			sccp_copy_string(k_button, v->value, sizeof(k_button));
			splitter = k_button;
			buttonType = strsep(&splitter, ",");
			buttonName = strsep(&splitter, ",");
			buttonOption = strsep(&splitter, ",");
			buttonArgs = splitter;

			for (i = 0; i < ARRAY_LEN(sccp_buttontypes) && strcasecmp(buttonType, sccp_buttontypes[i].text); ++i);
			if (i >= ARRAY_LEN(sccp_buttontypes)) {
				pbx_log(LOG_WARNING, "Unknown button type '%s'.\n", buttonType);
				changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
				/* will cause an empty button to be inserted */
			} else {
				type = sccp_buttontypes[i].buttontype;
			}
			if ((changed = sccp_config_checkButton(dest, buttonindex, type, buttonName ? pbx_strip(buttonName) : NULL, buttonOption ? pbx_strip(buttonOption) : NULL, buttonArgs ? pbx_strip(buttonArgs) : NULL))) {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Button: %s changed. Giving up on checking buttonchanges, reloading all of them.\n", v->value);
				break;
			}
			buttonindex++;
		}
		if (!changed && SCCP_LIST_GETSIZE(buttonconfigList) != buttonindex) {
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Number of Buttons changed (%d != %d). Reloading all of them.\n", SCCP_LIST_GETSIZE(buttonconfigList), buttonindex);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
		/* Clear/Set pendingDelete and PendingUpdate if button has changed or not
		 *
		 * \note Moved here from device_post_reload so that we will know the new state before line_post_reload. 
		 * That way adding/removind a line while accidentally keeping the button config for that line still works.
		 */
		if (!changed) {
			SCCP_LIST_LOCK(buttonconfigList);
			SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
				config->pendingDelete = 0;
				config->pendingUpdate = 0;
			}
			SCCP_LIST_UNLOCK(buttonconfigList);
		}
	}
	if (changed) {
		buttonindex = 0;										/* buttonconfig has changed. Load all buttons as new ones */
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "Discarding Previous ButtonConfig Completely\n");
		for (v = first_var; v; v = v->next) {
			sccp_copy_string(k_button, v->value, sizeof(k_button));
			splitter = k_button;
			buttonType = strsep(&splitter, ",");
			buttonName = strsep(&splitter, ",");
			buttonOption = strsep(&splitter, ",");
			buttonArgs = splitter;

			for (i = 0; i < ARRAY_LEN(sccp_buttontypes) && strcasecmp(buttonType, sccp_buttontypes[i].text); ++i);
			if (i >= ARRAY_LEN(sccp_buttontypes)) {
				pbx_log(LOG_WARNING, "Unknown button type '%s'.\n", buttonType);
				changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
				/* will cause an empty button to be inserted */
			} else {
				type = sccp_buttontypes[i].buttontype;
			}
			sccp_config_addButton(dest, buttonindex, type, buttonName ? pbx_strip(buttonName) : NULL, buttonOption ? pbx_strip(buttonOption) : NULL, buttonArgs ? pbx_strip(buttonArgs) : NULL);
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Added button: %s\n", v->value);
			buttonindex++;
		}
	}

	/* return changed status */
	if (GLOB(reload_in_progress)) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "buttonconfig: %s\n", changed ? "changed" : "remained the same");
	}

	return changed;
}

/* end dyn config */
/*!
 * \brief check a Button against the current ButtonConfig on a device
 * \param buttonconfig_head pointer to the device->buttonconfig list
 * \param index button index
 * \param type type of button
 * \param name name
 * \param options options
 * \param args args
 *
 * \callgraph
 * \callergraph
 * 
 * \todo Build a check to see if the button has changed
 */
sccp_value_changed_t sccp_config_checkButton(void *buttonconfig_head, int index, sccp_config_buttontype_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;

	// boolean_t is_new = FALSE;
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	SCCP_LIST_HEAD (, sccp_buttonconfig_t) * buttonconfigList = buttonconfig_head;

	SCCP_LIST_LOCK(buttonconfigList);
	SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
		// check if the button is to be deleted to see if we need to replace it
		if (config->index == index) {
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Found Button index at %d:%d\n", config->index, index);
			break;
		}
	}
	SCCP_LIST_UNLOCK(buttonconfigList);

	changed = SCCP_CONFIG_CHANGE_CHANGED;
	if (config) {
		switch (type) {
			case LINE:
			{
				struct composedId composedLineRegistrationId;

				memset(&composedLineRegistrationId, 0, sizeof(struct composedId));
				composedLineRegistrationId = sccp_parseComposedId(name, 80);
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: ComposedId mainId: %s, subscriptionId.number: %s, subscriptionId.name: %s, subscriptionId.aux: %s\n", composedLineRegistrationId.mainId, composedLineRegistrationId.subscriptionId.number, composedLineRegistrationId.subscriptionId.name, composedLineRegistrationId.subscriptionId.aux);
				if (LINE == config->type &&
				    sccp_strequals(config->label, name) && sccp_strequals(config->button.line.name, composedLineRegistrationId.mainId) && sccp_strcaseequals(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number) && sccp_strequals(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name) && sccp_strequals(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux)
				) {
					if (!options || sccp_strequals(config->button.line.options, options)) {
						sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Line Button Definition remained the same\n");
						changed = SCCP_CONFIG_CHANGE_NOCHANGE;
					}
				}
				break;
			}
			case SPEEDDIAL:
				/* \todo check if values change */
				if (SPEEDDIAL == config->type && sccp_strequals(config->label, name) && sccp_strequals(config->button.speeddial.ext, options)) {
					if (!args || sccp_strequals(config->button.speeddial.hint, args)) {
						sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Speeddial Button Definition remained the same\n");
						changed = SCCP_CONFIG_CHANGE_NOCHANGE;
					}
				}
				break;
			case SERVICE:
				if (SERVICE == config->type && sccp_strequals(config->label, name) && sccp_strequals(config->button.service.url, options)) {
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Service Button Definition remained the same\n");
					changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				}
				break;
			case FEATURE:
				if (FEATURE == config->type && index == config->index && sccp_strequals(config->label, name) && config->button.feature.id == sccp_featureStr2featureID(options)) {
					if (!args || sccp_strequals(config->button.feature.options, args)) {
						sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Feature Button Definition remained the same\n");
						changed = SCCP_CONFIG_CHANGE_NOCHANGE;
					}
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Feature Button Definition changed\n");
				}
				break;
			case EMPTY:
				if (EMPTY == config->type) {
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Button Definition remained the same\n");
					changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				}
				break;
			default:
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: Unknown ButtonType\n");
				break;
		}
	}
	if (changed) {	
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: ButtonTemplate Has Changed\n");
	} else {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: ButtonTemplate Remained the same\n");
	}
	return changed;
}


/*!
 * \brief add a Button to a device
 * \param buttonconfig_head pointer to the device->buttonconfig list
 * \param index button index
 * \param type type of button
 * \param name name
 * \param options options
 * \param args args
 *
 * \callgraph
 * \callergraph
 * 
 * \todo Build a check to see if the button has changed
 */
sccp_value_changed_t sccp_config_addButton(void *buttonconfig_head, int index, sccp_config_buttontype_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;
	SCCP_LIST_HEAD (, sccp_buttonconfig_t) * buttonconfigList = buttonconfig_head;

	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Loading New Button Config\n");
	
	if (type != LINE && type != SPEEDDIAL && type != SERVICE && type != FEATURE && type != EMPTY) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: Unknown ButtonType. Skipping\n");
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	
	SCCP_LIST_LOCK(buttonconfigList);
	if (!(config = sccp_calloc(1, sizeof(sccp_buttonconfig_t)))) {
		pbx_log(LOG_WARNING, "SCCP: sccp_config_addButton, memory allocation failed (calloc) failed\n");
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	config->index = index;
	config->type = type;
	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "New %s Button '%s' at : %d:%d\n", sccp_config_buttontype2str(type), name, index, config->index);
	SCCP_LIST_INSERT_TAIL(buttonconfigList, config, list);
	SCCP_LIST_UNLOCK(buttonconfigList);

	/* replace faulty button declarations with an empty button */
	if (type != EMPTY && (sccp_strlen_zero(name) || (type != LINE && !options))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Faulty %s Button Configuration found at index: %d, name: %s, options: %s, args: %s. Substituted with  EMPTY button\n", sccp_config_buttontype2str(type), config->index, name, options, args);
		type = EMPTY;
	}

	switch (type) {
		case LINE:
		{
			struct composedId composedLineRegistrationId;

			memset(&composedLineRegistrationId, 0, sizeof(struct composedId));
			composedLineRegistrationId = sccp_parseComposedId(name, 80);
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Line Button Definition\n");
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: ComposedId mainId: %s, subscriptionId.number: %s, subscriptionId.name: %s, subscriptionId.aux: %s\n", composedLineRegistrationId.mainId, composedLineRegistrationId.subscriptionId.number, composedLineRegistrationId.subscriptionId.name, composedLineRegistrationId.subscriptionId.aux);
			config->type = LINE;
			config->index = index;

			sccp_copy_string(config->label, name, sizeof(config->label));
			sccp_copy_string(config->button.line.name, composedLineRegistrationId.mainId, sizeof(config->button.line.name));
			sccp_copy_string(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number, sizeof(config->button.line.subscriptionId.number));
			sccp_copy_string(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name, sizeof(config->button.line.subscriptionId.name));
			sccp_copy_string(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux, sizeof(config->button.line.subscriptionId.aux));
			if (options) {
				sccp_copy_string(config->button.line.options, options, sizeof(config->button.line.options));
			} else {
				sccp_copy_string(config->button.line.options, "", sizeof(config->button.line.options));
			}
			break;
		}
		case SPEEDDIAL:
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Speeddial Button Definition\n");
			config->type = SPEEDDIAL;
			config->index = index;

			sccp_copy_string(config->label, name, sizeof(config->label));
			sccp_copy_string(config->button.speeddial.ext, options, sizeof(config->button.speeddial.ext));
			if (args) {
				sccp_copy_string(config->button.speeddial.hint, args, sizeof(config->button.speeddial.hint));
			} else {
				sccp_copy_string(config->button.speeddial.hint, "", sizeof(config->button.speeddial.hint));
			}
			break;
		case SERVICE:
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Service Button Definition\n");
			/* \todo check if values change */
			config->type = SERVICE;
			config->index = index;

			sccp_copy_string(config->label, name, sizeof(config->label));
			sccp_copy_string(config->button.service.url, options, sizeof(config->button.service.url));
			break;
		case FEATURE:
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Feature Button Definition\n");
			sccp_log((DEBUGCAT_FEATURE + DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "featureID: %s\n", options);
			config->type = FEATURE;
			config->index = index;

			sccp_copy_string(config->label, name, sizeof(config->label));
			config->button.feature.id = sccp_featureStr2featureID(options);

			if (args) {
				sccp_copy_string(config->button.feature.options, args, sizeof(config->button.feature.options));
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Arguments present on feature button: %d\n", config->instance);
			} else {
				sccp_copy_string(config->button.feature.options, "", sizeof(config->button.feature.options));
			}
			sccp_log((DEBUGCAT_FEATURE + DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "Configured feature button with featureID: %s args: %s\n", options, args);

			break;
		case EMPTY:
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Empty Button Definition\n");
			config->type = EMPTY;
			config->index = index;
			break;
		case SCCP_CONFIG_BUTTONTYPE_SENTINEL:
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: Enum ButtonType SENTINEL\n");
			break;
	}
	return SCCP_CONFIG_CHANGE_CHANGED;
}

/*!
 * \brief Build Line
 * \param l SCCP Line
 * \param v Asterisk Variable
 * \param lineName Name of line as char
 * \param isRealtime is Realtime as Boolean
 *
 * \callgraph
 * \callergraph
 * 
 */
static void sccp_config_buildLine(sccp_line_t * l, PBX_VARIABLE_TYPE * v, const char *lineName, boolean_t isRealtime)
{
	sccp_configurationchange_t res = sccp_config_applyLineConfiguration(l, v);

#ifdef CS_SCCP_REALTIME
	l->realtime = isRealtime;
#endif
	// if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET && l && l->pendingDelete) {
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s: major changes for line '%s' detected, device reset required -> pendingUpdate=1\n", l->id, l->name);
		l->pendingUpdate = 1;
	} else {
		l->pendingUpdate = 0;
	}
	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Removing pendingDelete\n", l->name);
	l->pendingDelete = 0;
}

/*!
 * \brief Build Device
 * \param d SCCP Device
 * \param variable Asterisk Variable
 * \param deviceName Name of device as char
 * \param isRealtime is Realtime as Boolean
 *
 * \callgraph
 * \callergraph
 * 
 */
static void sccp_config_buildDevice(sccp_device_t * d, PBX_VARIABLE_TYPE * variable, const char *deviceName, boolean_t isRealtime)
{
	PBX_VARIABLE_TYPE *v = variable;

	/* apply configuration */
	sccp_configurationchange_t res = sccp_config_applyDeviceConfiguration(d, v);

#ifdef CS_DEVSTATE_FEATURE
	sccp_buttonconfig_t *config = NULL;
	sccp_devstate_specifier_t *dspec;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == FEATURE) {
			/* Check for the presence of a devicestate specifier and register in device list. */
			if ((SCCP_FEATURE_DEVSTATE == config->button.feature.id) && (strncmp("", config->button.feature.options, 254))) {
				dspec = sccp_calloc(1, sizeof(sccp_devstate_specifier_t));
				if (!dspec) {
					pbx_log(LOG_ERROR, "error while allocating memory for devicestate specifier");
				} else {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Recognized devstate feature button: %d\n", config->instance);
					SCCP_LIST_LOCK(&d->devstateSpecifiers);
					sccp_copy_string(dspec->specifier, config->button.feature.options, sizeof(config->button.feature.options));
					SCCP_LIST_INSERT_TAIL(&d->devstateSpecifiers, dspec, list);
					SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

#endif

#ifdef CS_SCCP_REALTIME
	d->realtime = isRealtime;
#endif
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET && d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s: major changes for device detected, device reset required -> pendingUpdate=1\n", d->id);
		d->pendingUpdate = 1;
	} else {
		d->pendingUpdate = 0;
	}
	d->pendingDelete = 0;
}

/*!
 * \brief Get Configured Line from Asterisk Variable
 * \param v Asterisk Variable
 * \return Configured SCCP Line
 * \note also used by realtime functionality to line device from Asterisk Variable
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_configurationchange_t sccp_config_applyGlobalConfiguration(PBX_VARIABLE_TYPE * v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpGlobalConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;

	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(sccp_globals, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_GLOBAL_SEGMENT, SetEntries);
	}
	if (res) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Update Needed (%d)\n", res);
	}

	sccp_config_set_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT, SetEntries);

	if (GLOB(keepalive) < SCCP_MIN_KEEPALIVE) {
		GLOB(keepalive) = SCCP_MIN_KEEPALIVE;
	}

	return res;
}

/*!
 * \brief Parse sccp.conf and Create General Configuration
 * \param readingtype SCCP Reading Type
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_config_general(sccp_readingtype_t readingtype)
{
	PBX_VARIABLE_TYPE *v;

	/* Cleanup for reload */
	if (!GLOB(cfg)) {
		pbx_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return FALSE;
	}

	/* read the general section */
	v = ast_variable_browse(GLOB(cfg), "general");
	if (!v) {
		pbx_log(LOG_WARNING, "Missing [general] section, SCCP disabled\n");
		return FALSE;
	}
	// sccp_config_set_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT);

	/* setup bindaddress */
	if (!sccp_socket_getPort(&GLOB(bindaddr))) {
		struct sockaddr_in *in = (struct sockaddr_in *) &GLOB(bindaddr);

		in->sin_port = ntohs(DEFAULT_SCCP_PORT);
		GLOB(bindaddr).ss_family = AF_INET;
	}

	sccp_configurationchange_t res = sccp_config_applyGlobalConfiguration(v);

	/* setup bind-port */
	if (!sccp_socket_getPort(&GLOB(bindaddr))) {
		sccp_socket_setPort(&GLOB(bindaddr), DEFAULT_SCCP_PORT);
	}

	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "SCCP: major changes detected in globals, reset required -> pendingUpdate=1\n");
		GLOB(pendingUpdate = 1);
	} else {
		GLOB(pendingUpdate = 0);
	}

	/* setup hostname -> externip */
	sccp_updateExternIp();											/* deprecated, not needed any more */

	/* setup regcontext */
	char newcontexts[SCCP_MAX_CONTEXT];
	char oldcontexts[SCCP_MAX_CONTEXT];
	char *stringp, *context, *oldregcontext;

	sccp_copy_string(newcontexts, GLOB(regcontext), sizeof(newcontexts));
	stringp = newcontexts;

	sccp_copy_string(oldcontexts, GLOB(used_context), sizeof(oldcontexts));					// Initialize copy of current regcontext for later use in removing stale contexts
	oldregcontext = oldcontexts;

	cleanup_stale_contexts(stringp, oldregcontext);								// Let's remove any contexts that are no longer defined in regcontext

	while ((context = strsep(&stringp, "&"))) {								// Create contexts if they don't exist already
		sccp_copy_string(GLOB(used_context), context, sizeof(GLOB(used_context)));
		pbx_context_find_or_create(NULL, NULL, context, "SCCP");
	}

	return TRUE;
}

/*!
 * \brief Cleanup Stale Contexts (regcontext)
 * \param new New Context as Character
 * \param old Old Context as Character
 */
void cleanup_stale_contexts(char *new, char *old)
{
	char *oldcontext, *newcontext, *stalecontext, *stringp, newlist[SCCP_MAX_CONTEXT];

	while ((oldcontext = strsep(&old, "&"))) {
		stalecontext = NULL;
		sccp_copy_string(newlist, new, sizeof(newlist));
		stringp = newlist;
		while ((newcontext = strsep(&stringp, "&"))) {
			if (sccp_strequals(newcontext, oldcontext)) {
				/* This is not the context you're looking for */
				stalecontext = NULL;
				break;
			} else {
				stalecontext = oldcontext;
			}

		}
		if (stalecontext) {
			ast_context_destroy(ast_context_find(stalecontext), "SCCP");
		}
	}
}

/*!
 * \brief Read Lines from the Config File
 *
 * \param readingtype as SCCP Reading Type
 * \since 10.01.2008 - branche V3
 * \author Marcello Ceschia
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype)
{
	// struct ast_config *cfg = NULL;

	char *cat = NULL;
	PBX_VARIABLE_TYPE *v = NULL;
	uint8_t device_count = 0;
	uint8_t line_count = 0;

	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Loading Devices and Lines from config\n");

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking Reading Type\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Pre Reload\n");
		sccp_device_pre_reload();
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Pre Reload\n");
		sccp_line_pre_reload();
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Pre Reload\n");
		sccp_softkey_pre_reload();
	}

	if (!GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "SCCP: (sccp_config_readDevicesLines) Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}

	while ((cat = pbx_category_browse(GLOB(cfg), cat))) {

		const char *utype;

		if (!strcasecmp(cat, "general")) {
			continue;
		}
		utype = pbx_variable_retrieve(GLOB(cfg), cat, "type");
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_readDevicesLines) Reading Section Of Type %s\n", utype);

		if (!utype) {
			pbx_log(LOG_WARNING, "Section '%s' is missing a type parameter\n", cat);
			continue;
		} else if (!strcasecmp(utype, "device")) {
			// check minimum requirements for a device
			if (sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "devicetype"))) {
				pbx_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
				continue;
			} else {
				v = ast_variable_browse(GLOB(cfg), cat);

				// Try to find out if we have the device already on file.
				// However, do not look into realtime, since
				// we might have been asked to create a device for realtime addition,
				// thus causing an infinite loop / recursion.
				d = sccp_device_find_byid(cat, FALSE);

				/* create new device with default values */
				if (!d) {
					d = sccp_device_create(cat);
					// sccp_copy_string(d->id, cat, sizeof(d->id));         /* set device name */
					sccp_device_addToGlobals(d);
					device_count++;
				} else {
					if (d->pendingDelete) {
						d->pendingDelete = 0;
					}
				}
				sccp_config_buildDevice(d, v, cat, FALSE);
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found device %d: %s\n", device_count, cat);
				/* load saved settings from ast db */
				sccp_config_restoreDeviceFeatureStatus(d);
				d = sccp_device_release(d);
			}
		} else if (!strcasecmp(utype, "line")) {
			/* check minimum requirements for a line */
			if ((!(!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "label"))) && (!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "cid_name"))) && (!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "cid_num"))))) {
				pbx_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
				continue;
			}
			line_count++;

			v = ast_variable_browse(GLOB(cfg), cat);

			/* check if we have this line already */
			//    SCCP_RWLIST_WRLOCK(&GLOB(lines));
			if ((l = sccp_line_find_byname(cat, FALSE))) {
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found line %d: %s, do update\n", line_count, cat);
				sccp_config_buildLine(l, v, cat, FALSE);
			} else {
				if ((l = sccp_line_create(cat))) {
					sccp_config_buildLine(l, v, cat, FALSE);
					sccp_line_addToGlobals(l);						/* may find another line instance create by another thread, in that case the newly created line is going to be dropped when l is released */
				}
			}
			l = l ? sccp_line_release(l) : NULL;							/* release either found / or newly created line. will remain retained in glob(lines) anyway. */
			//    SCCP_RWLIST_UNLOCK(&GLOB(lines));

		} else if (!strcasecmp(utype, "softkeyset")) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "read set %s\n", cat);
			v = ast_variable_browse(GLOB(cfg), cat);
			sccp_config_softKeySet(v, cat);
		} else {
			pbx_log(LOG_WARNING, "SCCP: (sccp_config_readDevicesLines) UNKNOWN SECTION / UTYPE, type: %s\n", utype);
		}
	}

#ifdef CS_SCCP_REALTIME
	/* reload realtime lines */
	sccp_configurationchange_t res;

	sccp_line_t *line = NULL;
	PBX_VARIABLE_TYPE *rv = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if ((line = sccp_line_retain(l))) {
			do {
				if (line->realtime == TRUE && line != GLOB(hotline)->line) {
					sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: reload realtime line\n", line->name);
					rv = pbx_load_realtime(GLOB(realtimelinetable), "name", line->name, NULL);
					/* we did not find this line, mark it for deletion */
					if (!rv) {
						sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: realtime line not found - set pendingDelete=1\n", line->name);
						line->pendingDelete = 1;
						break;
					}
					line->pendingDelete = 0;

					res = sccp_config_applyLineConfiguration(line, rv);
					/* check if we did some changes that needs a device update */
					if (GLOB(reload_in_progress) && res & SCCP_CONFIG_NEEDDEVICERESET) {
						line->pendingUpdate = 1;
					} else {
						line->pendingUpdate = 0;
					}
					pbx_variables_destroy(rv);
				}
			} while (0);
			line = sccp_line_release(line);
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	/* finished realtime line reload */

	sccp_device_t *device;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if ((device = sccp_device_retain(d))) {
			do {
				if (device->realtime == TRUE) {
					sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: reload realtime line\n", device->id);
					rv = pbx_load_realtime(GLOB(realtimedevicetable), "name", device->id, NULL);
					/* we did not find this line, mark it for deletion */
					if (!rv) {
						sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: realtime device not found - set pendingDelete=1\n", device->id);
						device->pendingDelete = 1;
						break;
					}
					device->pendingDelete = 0;

					res = sccp_config_applyDeviceConfiguration(device, rv);
					/* check if we did some changes that needs a device update */
					if (GLOB(reload_in_progress) && res & SCCP_CONFIG_NEEDDEVICERESET) {
						device->pendingUpdate = 1;
					} else {
						device->pendingUpdate = 0;
					}
					pbx_variables_destroy(rv);
				}
			} while (0);
			device = sccp_device_release(device);
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
#endif

	if (GLOB(reload_in_progress) && GLOB(pendingUpdate)) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Global param changed needing restart ->  Restart all device\n");
		sccp_device_t *device;

		SCCP_RWLIST_WRLOCK(&GLOB(devices));
		SCCP_RWLIST_TRAVERSE(&GLOB(devices), device, list) {
			if (!device->pendingDelete && !device->pendingUpdate) {
				device->pendingUpdate = 1;
			}
		}
		SCCP_RWLIST_UNLOCK(&GLOB(devices));
	} else {
		GLOB(pendingUpdate) = 0;
	}
	GLOB(pendingUpdate) = 0;

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking Reading Type\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		/* IMPORTANT: The line_post_reload function may change the pendingUpdate field of
		 * devices, so it's really important to call it *before* calling device_post_real().
		 */
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Post Reload\n");
		sccp_line_post_reload();
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Post Reload\n");
		sccp_device_post_reload();
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Post Reload\n");
		sccp_softkey_post_reload();
	}
}

/*!
 * \brief Get Configured Line from Asterisk Variable
 * \param l SCCP Line
 * \param v Asterisk Variable
 * \return Configured SCCP Line
 * \note also used by realtime functionality to line device from Asterisk Variable
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_configurationchange_t sccp_config_applyLineConfiguration(sccp_line_t * l, PBX_VARIABLE_TYPE * v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpLineConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;

	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(l, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_LINE_SEGMENT, SetEntries);
	}

	sccp_config_set_defaults(l, SCCP_CONFIG_LINE_SEGMENT, SetEntries);

	if (sccp_strlen_zero(l->id)) {
		sprintf(l->id, "%04d", SCCP_LIST_GETSIZE(&GLOB(lines)));
	}

	return res;
}

/*!
 * \brief Apply Device Configuration from Asterisk Variable
 * \param d SCCP Device
 * \param v Asterisk Variable
 * \return Configured SCCP Device
 * \note also used by realtime functionality to line device from Asterisk Variable
 * \todo this function should be called sccp_config_applyDeviceConfiguration
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_configurationchange_t sccp_config_applyDeviceConfiguration(sccp_device_t * d, PBX_VARIABLE_TYPE * v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpDeviceConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;

	if (d->pendingDelete) {
		sccp_dev_clean(d, FALSE, 0);
	}
	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(d, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_DEVICE_SEGMENT, SetEntries);
	}

	sccp_config_set_defaults(d, SCCP_CONFIG_DEVICE_SEGMENT, SetEntries);

	if (d->keepalive < SCCP_MIN_KEEPALIVE) {
		d->keepalive = SCCP_MIN_KEEPALIVE;
	}
	return res;
}

/*!
 * \brief Find the Correct Config File
 * \return Asterisk Config Object as ast_config
 */
sccp_config_file_status_t sccp_config_getConfig(boolean_t force)
{
	// struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS & CONFIG_FLAG_FILEUNCHANGED };
	int res = 0;
	struct ast_flags config_flags = { CONFIG_FLAG_FILEUNCHANGED };
	if (force) {
		if (GLOB(cfg)) {
			pbx_config_destroy(GLOB(cfg));
			GLOB(cfg) = NULL;
		}
		pbx_clear_flag(&config_flags, CONFIG_FLAG_FILEUNCHANGED);
	}

	if (sccp_strlen_zero(GLOB(config_file_name))) {
		GLOB(config_file_name) = strdup("sccp.conf");
	}
	GLOB(cfg) = pbx_config_load(GLOB(config_file_name), "chan_sccp", config_flags);
	if (GLOB(cfg) == CONFIG_STATUS_FILEMISSING) {
		pbx_log(LOG_ERROR, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
		GLOB(cfg) = NULL;
		if (GLOB(config_file_name)) {
			sccp_free(GLOB(config_file_name));
		}
		GLOB(config_file_name) = strdup("sccp.conf");
		res = CONFIG_STATUS_FILE_NOT_FOUND;
		goto FUNC_EXIT;
	} else if (GLOB(cfg) == CONFIG_STATUS_FILEINVALID) {
		pbx_log(LOG_ERROR, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
		GLOB(cfg) = NULL;
		if (GLOB(config_file_name)) {
			sccp_free(GLOB(config_file_name));
		}
		GLOB(config_file_name) = strdup("sccp.conf");
		res = CONFIG_STATUS_FILE_INVALID;
		goto FUNC_EXIT;
	} else if (GLOB(cfg) == CONFIG_STATUS_FILEUNCHANGED) {
		// ugly solution, but we always need to have a valid config file loaded.
		pbx_clear_flag(&config_flags, CONFIG_FLAG_FILEUNCHANGED);
		GLOB(cfg) = pbx_config_load(GLOB(config_file_name), "chan_sccp", config_flags);
		if (!force) {
			pbx_log(LOG_NOTICE, "Config file '%s' has not changed, aborting reload.\n", GLOB(config_file_name));
			res = CONFIG_STATUS_FILE_NOT_CHANGED;
			goto FUNC_EXIT;
		} else {
			pbx_log(LOG_NOTICE, "Config file '%s' has not changed, forcing reload.\n", GLOB(config_file_name));
		}
	}
	if (GLOB(cfg)) {
		if (ast_variable_browse(GLOB(cfg), "devices")) {						/* Warn user when old entries exist in sccp.conf */
			pbx_log(LOG_ERROR, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			pbx_config_destroy(GLOB(cfg));
			GLOB(cfg) = NULL;
			res = CONFIG_STATUS_FILE_OLD;
			goto FUNC_EXIT;
		} else if (!ast_variable_browse(GLOB(cfg), "general")) {
			pbx_log(LOG_ERROR, "Missing [general] section, SCCP disabled\n");
			pbx_config_destroy(GLOB(cfg));
			GLOB(cfg) = NULL;
			res = CONFIG_STATUS_FILE_NOT_SCCP;
			goto FUNC_EXIT;
		}
	} else {
		pbx_log(LOG_ERROR, "Missing Glob(cfg)\n");
		GLOB(cfg) = NULL;
		res = CONFIG_STATUS_FILE_NOT_FOUND;
		goto FUNC_EXIT;
	}
	pbx_log(LOG_NOTICE, "Config file '%s' loaded.\n", GLOB(config_file_name));
	res = CONFIG_STATUS_FILE_OK;
FUNC_EXIT:
	return res;
}

/*!
 * \brief Read a SoftKey configuration context
 * \param variable list of configuration variables
 * \param name name of this configuration (context)
 * 
 * \callgraph
 * \callergraph
 *
 */
void sccp_config_softKeySet(PBX_VARIABLE_TYPE * variable, const char *name)
{
	int keySetSize;
	sccp_softKeySetConfiguration_t *softKeySetConfiguration = NULL;
	int keyMode = -1;
	unsigned int i = 0;

	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "start reading softkeyset: %s\n", name);

	SCCP_LIST_LOCK(&softKeySetConfig);
	SCCP_LIST_TRAVERSE(&softKeySetConfig, softKeySetConfiguration, list) {
		if (sccp_strcaseequals(softKeySetConfiguration->name, name)) {
			//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Softkeyset: %s already defined\n", name);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&softKeySetConfig);

	if (!softKeySetConfiguration) {
		//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Adding Softkeyset: %s\n", name);
		softKeySetConfiguration = sccp_calloc(1, sizeof(sccp_softKeySetConfiguration_t));
		memset(softKeySetConfiguration, 0, sizeof(sccp_softKeySetConfiguration_t));

		sccp_copy_string(softKeySetConfiguration->name, name, sizeof(sccp_softKeySetConfiguration_t));
		softKeySetConfiguration->numberOfSoftKeySets = 0;
#ifdef CS_EXPERIMENTAL
		softKeySetConfiguration->softkeyCbMap = NULL;							// defaults to static softkeyMapCb
#endif

		/* add new softkexSet to list */
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_INSERT_HEAD(&softKeySetConfig, softKeySetConfiguration, list);
		SCCP_LIST_UNLOCK(&softKeySetConfig);
	}

	while (variable) {
		keyMode = -1;
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeyset: %s \n", variable->name);
		if (sccp_strcaseequals(variable->name, "type")) {

		} else if (sccp_strcaseequals(variable->name, "uriaction")) {
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: UriAction softkey (%s) found\n", variable->value);
			if (!softKeySetConfiguration->softkeyCbMap) {
				softKeySetConfiguration->softkeyCbMap = sccp_softkeyMap_copyStaticallyMapped();
			}
			
			char *uriactionstr = strdupa(variable->value);
			char *event = strsep(&uriactionstr, ",");
			if (event && !sccp_strlen_zero(uriactionstr)) {
				sccp_softkeyMap_replaceCallBackByUriAction(softKeySetConfiguration->softkeyCbMap, labelstr2int(event), uriactionstr);
			} else {
				sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: UriAction softkey (%s) not found, or no uris (%s) specified\n", event, uriactionstr);
			}
		} else if (sccp_strcaseequals(variable->name, "onhook")) {
			keyMode = KEYMODE_ONHOOK;
		} else if (sccp_strcaseequals(variable->name, "connected")) {
			keyMode = KEYMODE_CONNECTED;
		} else if (sccp_strcaseequals(variable->name, "onhold")) {
			keyMode = KEYMODE_ONHOLD;
		} else if (sccp_strcaseequals(variable->name, "ringin")) {
			keyMode = KEYMODE_RINGIN;
		} else if (sccp_strcaseequals(variable->name, "offhook")) {
			keyMode = KEYMODE_OFFHOOK;
		} else if (sccp_strcaseequals(variable->name, "conntrans")) {
			keyMode = KEYMODE_CONNTRANS;
		} else if (sccp_strcaseequals(variable->name, "digitsfoll")) {
			keyMode = KEYMODE_DIGITSFOLL;
		} else if (sccp_strcaseequals(variable->name, "connconf")) {
			keyMode = KEYMODE_CONNCONF;
		} else if (sccp_strcaseequals(variable->name, "ringout")) {
			keyMode = KEYMODE_RINGOUT;
		} else if (sccp_strcaseequals(variable->name, "offhookfeat")) {
			keyMode = KEYMODE_OFFHOOKFEAT;
		} else if (sccp_strcaseequals(variable->name, "onhint")) {
			keyMode = KEYMODE_INUSEHINT;
		} else {
			// do nothing
		}

		if (keyMode == -1) {
			variable = variable->next;
			continue;
		}

		if (softKeySetConfiguration->numberOfSoftKeySets < (keyMode + 1)) {
			softKeySetConfiguration->numberOfSoftKeySets = keyMode + 1;
		}

		for (i = 0; i < (sizeof(SoftKeyModes) / sizeof(softkey_modes)); i++) {
			if (SoftKeyModes[i].id == keyMode) {

				/* cleanup old value */
				if (softKeySetConfiguration->modes[i].ptr) {
					//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "KeyMode(%d) Ptr already defined in Softkeyset: %s. Freeing...\n", keyMode, name);
					sccp_free(softKeySetConfiguration->modes[i].ptr);
				}
				uint8_t *softkeyset = sccp_calloc(StationMaxSoftKeySetDefinition, sizeof(uint8_t));

				keySetSize = sccp_config_readSoftSet(softkeyset, variable->value);
				//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Adding KeyMode(%d), with Size(%d), prt(%p) to Softkeyset: %s\n", keyMode, keySetSize, softkeyset, name);

				if (keySetSize > 0) {
					softKeySetConfiguration->modes[i].id = keyMode;
					softKeySetConfiguration->modes[i].ptr = softkeyset;
					softKeySetConfiguration->modes[i].count = keySetSize;
				} else {
					softKeySetConfiguration->modes[i].id = keyMode;
					softKeySetConfiguration->modes[i].ptr = NULL;
					softKeySetConfiguration->modes[i].count = 0;
					sccp_free(softkeyset);
				}
			}
		}

		variable = variable->next;
	}

}

/*!
 * \brief Read a single SoftKeyMode (configuration values)
 * \param softkeyset SoftKeySet
 * \param data configuration values
 * \return number of parsed softkeys
 */
uint8_t sccp_config_readSoftSet(uint8_t * softkeyset, const char *data)
{
	int i = 0, j;

	char labels[256];
	char *splitter;
	char *label;

	if (!data) {
		return 0;
	}
	strcpy(labels, data);
	splitter = labels;
	while ((label = strsep(&splitter, ",")) != NULL && (i + 1) < StationMaxSoftKeySetDefinition) {
		softkeyset[i++] = sccp_config_getSoftkeyLbl(label);
	}

	for (j = i + 1; j < StationMaxSoftKeySetDefinition; j++) {
		softkeyset[j] = SKINNY_LBL_EMPTY;
	}
	return i;
}

/*!
 * \brief Get softkey label as int
 * \param key configuration value
 * \return SoftKey label as int of SKINNY_LBL_*. returns an empty button if nothing matched
 */
int sccp_config_getSoftkeyLbl(char *key)
{
	int i = 0;
	int size = sizeof(softKeyTemplate) / sizeof(softkeyConfigurationTemplate);

	for (i = 0; i < size; i++) {
		if (sccp_strcaseequals(softKeyTemplate[i].configVar, key)) {
			return softKeyTemplate[i].softkey;
		}
	}
	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeybutton: %s not defined", key);
	return SKINNY_LBL_EMPTY;
}

/*!
 * \brief Restore feature status from ast-db
 * \param device device to be restored
 * \todo restore cfwd feature
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device)
{
	if (!device) {
		return;
	}
#ifdef CS_DEVSTATE_FEATURE
	char buf[256] = "";
	sccp_devstate_specifier_t *specifier;
#endif

#ifndef ASTDB_FAMILY_KEY_LEN
#define ASTDB_FAMILY_KEY_LEN 256
#endif

#ifndef ASTDB_RESULT_LEN
#define ASTDB_RESULT_LEN 256
#endif
	char buffer[ASTDB_RESULT_LEN];
	char timebuffer[ASTDB_RESULT_LEN];
	int timeout = 0;

	/* Message */
	if (PBX(feature_getFromDatabase) ("SCCP/message", "text", buffer, sizeof(buffer))) {
		if (!sccp_strlen_zero(buffer)) {
			if (PBX(feature_getFromDatabase) && PBX(feature_getFromDatabase) ("SCCP/message", "timeout", timebuffer, sizeof(timebuffer))) {
				sscanf(timebuffer, "%i", &timeout);
			}
			if (timeout) {
				sccp_dev_displayprinotify(device, buffer, 5, timeout);
			} else {
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_IDLE, buffer);
			}
		}
	}

	/* initialize so called priority feature */
	device->priFeature.status = 0x010101;
	device->priFeature.initialized = 0;
#ifdef CS_DEVSTATE_FEATURE
	/* Read and initialize custom devicestate entries */
	SCCP_LIST_LOCK(&device->devstateSpecifiers);
	SCCP_LIST_TRAVERSE(&device->devstateSpecifiers, specifier, list) {
		/* Check if there is already a devicestate entry */
		if (PBX(feature_getFromDatabase) (devstate_db_family, specifier->specifier, buf, sizeof(buf))) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Found Existing Custom Devicestate Entry: %s, state: %s\n", device->id, specifier->specifier, buf);
		} else {
			/* If not present, add a new devicestate entry. Default: NOT_INUSE */
			PBX(feature_addToDatabase) (devstate_db_family, specifier->specifier, "NOT_INUSE");
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Initialized Devicestate Entry: %s\n", device->id, specifier->specifier);
		}
		/* Register as generic hint watcher */
		/*! \todo Add some filtering in order to reduce number of unnecessarily triggered events.
		   Have to work out whether filtering with AST_EVENT_IE_DEVICE matches extension or hint device name. */
		snprintf(buf, 254, "Custom:%s", specifier->specifier);
		/* When registering for devstate events we wish to know if a single asterisk box has contributed
		   a change even in a rig of multiple asterisk with distributed devstate. This is to enable toggling
		   even then when otherwise the aggregate devicestate would obscure the change.
		   However, we need to force distributed devstate even on single asterisk boxes so to get the desired events. (-DD) */
#if defined(CS_AST_HAS_EVENT) && (defined(CS_DEVICESTATE) || defined(CS_CACHEABLE_DEVICESTATE))
		pbx_enable_distributed_devstate();
		specifier->sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE, sccp_devstateFeatureState_cb, "devstate subscription", device, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, buf, AST_EVENT_IE_END);
#endif
	}
	SCCP_LIST_UNLOCK(&device->devstateSpecifiers);
#endif
}

#if 0
int sccp_manager_config_metadata(struct mansession *s, const struct message *m)
{
	const SCCPConfigSegment *sccpConfigSegment = NULL;
	const SCCPConfigOption *config = NULL;
	long unsigned int sccp_option;
	char idtext[256];
	int total = 0;
	int i;
	const char *id = astman_get_header(m, "ActionID");
	const char *req_segment = astman_get_header(m, "Segment");
	const char *req_option = astman_get_header(m, "Option");
	char *description = "";
	char *description_part = "";

	if (strlen(req_segment) == 0) {										// return all segments
		int sccp_config_revision = 0;

		sscanf(SCCP_CONFIG_REVISION, "$" "Revision: %i" "$", &sccp_config_revision);
		astman_append(s, "Name: Chan-sccp-b: \r\n");
		astman_append(s, "Branch: %s\r\n", SCCP_BRANCH);
		astman_append(s, "Version: %s\r\n", SCCP_VERSION);
		astman_append(s, "Revision: %s\r\n", SCCP_REVISIONSTR);
		astman_append(s, "ConfigRevision: %d\r\n\r\n", sccp_config_revision);
		astman_send_listack(s, m, "List of segments will follow", "start");

		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			astman_append(s, "Event: SegmentEntry\r\n");
			astman_append(s, "Segment: %s\r\n\r\n", sccpConfigSegments[i].name);
			total++;
		}
		astman_append(s, "Event: SegmentlistComplete\r\n\r\n");
	} else if (strlen(req_option) == 0) {									// return all options for segment
		astman_send_listack(s, m, "List of SegmentOptions will follow", "start");
		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			if (sccp_strcaseequals(sccpConfigSegments[i].name, req_segment)) {
				sccpConfigSegment = &sccpConfigSegments[i];
				config = sccpConfigSegment->config;
				for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
					astman_append(s, "Event: OptionEntry\r\n");
					astman_append(s, "Segment: %s\r\n", sccpConfigSegment->name);
					astman_append(s, "Option: %s\r\n\r\n", config[sccp_option].name);
					total++;
				}
				total++;
			}
		}
		if (0 == total) {
			astman_append(s, "error: segment %s not found\r\n", req_segment);
		} else {
			astman_append(s, "Event: SegmentOptionlistComplete\r\n\r\n");
		}
	} else {												// return metadata for option in segmnet
		astman_send_listack(s, m, "List of Option Attributes will follow", "start");
		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			if (sccp_strcaseequals(sccpConfigSegments[i].name, req_segment)) {
				sccpConfigSegment = &sccpConfigSegments[i];
				config = sccp_find_config(sccpConfigSegments[i].segment, req_option);
				if (config) {
					if ((config->flags & SCCP_CONFIG_FLAG_IGNORE) != SCCP_CONFIG_FLAG_IGNORE) {
						char *possible_values = NULL;

						astman_append(s, "Event: AttributeEntry\r\n");
						astman_append(s, "Segment: %s\r\n", sccpConfigSegment->name);
						astman_append(s, "Option: %s\r\n", config->name);
						astman_append(s, "Required: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) ? "true" : "false");
						astman_append(s, "Deprecated: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED) ? "true" : "false");
						astman_append(s, "Obsolete: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE) ? "true" : "false");
						astman_append(s, "Multientry: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "true" : "false");
						astman_append(s, "RestartRequiredOnUpdate: %s\r\n", ((config->change & SCCP_CONFIG_NEEDDEVICERESET) == SCCP_CONFIG_NEEDDEVICERESET) ? "true" : "false");

						switch (config->type) {
							case SCCP_CONFIG_DATATYPE_BOOLEAN:
								astman_append(s, "Type: BOOLEAN\r\n");
								astman_append(s, "Size: %d\r\n", (int) config->size - 1);
								break;
							case SCCP_CONFIG_DATATYPE_INT:
								astman_append(s, "Type: INT\r\n");
								astman_append(s, "Size: %d\r\n", (int) config->size - 1);
								break;
							case SCCP_CONFIG_DATATYPE_UINT:
								astman_append(s, "Type: UNSIGNED INT\r\n");
								astman_append(s, "Size: %d\r\n", (int) config->size - 1);
								break;
							case SCCP_CONFIG_DATATYPE_STRINGPTR:
								astman_append(s, "Type: STRING\r\n");
								astman_append(s, "Size: *\r\n");
								break;
							case SCCP_CONFIG_DATATYPE_STRING:
								astman_append(s, "Type: STRING\r\n");
								astman_append(s, "Size: %d\r\n", (int) config->size - 1);
								break;
							case SCCP_CONFIG_DATATYPE_PARSER:
								astman_append(s, "Type: PARSER\r\n");
								astman_append(s, "possible_values: %s\r\n", "");
								astman_append(s, "Parser:%s", config->parsername);
								break;
							case SCCP_CONFIG_DATATYPE_CHAR:
								astman_append(s, "Type: CHAR\r\n");
								astman_append(s, "Size: 1\r\n");
								break;
							case SCCP_CONFIG_DATATYPE_ENUM:
								astman_append(s, "Type: ENUM\r\n");
								astman_append(s, "Size: %d\r\n", (int) config->size - 1);
                                                                char *all_entries = strdupa(config->all_entries());
                                                                while (*all_entries) {
                                                                        if (*all_entries == ',') {
                                                                                *all_entries = '|';
                                                                        }
                                                                        all_entries++;
                                                                }
								astman_append(s, "Possible Values: [%s]\r\n", all_entries);
								break;
						}
						if (config->defaultValue && !strlen(config->defaultValue) == 0) {
							astman_append(s, "DefaultValue: %s\r\n", config->defaultValue);
						}
						if (strlen(config->description) != 0) {
							description = strdupa(config->description);
							astman_append(s, "Description: ");
							while (description && (description_part = strsep(&description, "\n"))) {
								astman_append(s, "%s ", description_part);
							}
							if (!sccp_strlen_zero(possible_values)) {
								astman_append(s, "(POSSIBLE VALUES: %s)\r\n", possible_values);
							}
						}
						astman_append(s, "\r\n");
						if (possible_values) {
							sccp_free(possible_values);
						}
						total++;
					}
				} else {
					astman_append(s, "error: option %s in segment %s not found\r\n", req_option, req_segment);
					total++;
				}
			}
		}
		if (0 == total) {
			astman_append(s, "error: segment %s not found\r\n", req_segment);
			total++;
		} else {
			astman_append(s, "Event: SegmentOptionAttributelistComplete\r\n\r\n");
		}
	}

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);
	astman_append(s, "EventList: Complete\r\n" "ListItems: %d\r\n" "%s" "\r\n", total, idtext);
	return 0;
}
#endif

/* generate json output from now on */
int sccp_manager_config_metadata(struct mansession *s, const struct message *m)
{
	const SCCPConfigSegment *sccpConfigSegment = NULL;
	int total = 0;
	int i;
	const char *id = astman_get_header(m, "ActionID");
	const char *req_segment = astman_get_header(m, "Segment");

	if (strlen(req_segment) == 0) {										// return all segments
		int sccp_config_revision = 0;

		sscanf(SCCP_CONFIG_REVISION, "$" "Revision: %i" "$", &sccp_config_revision);

		astman_append(s, "Response: Success\r\n");
		if (!ast_strlen_zero(id)) {
			astman_append(s, "ActionID: %s\r\n", id);
		}

		astman_append(s, "JSON: {");
		astman_append(s, "\"Name\":\"Chan-sccp-b\",");
		astman_append(s, "\"Branch\":\"%s\",", SCCP_BRANCH);
		astman_append(s, "\"Version\":\"%s\",", SCCP_VERSION);
		astman_append(s, "\"Revision\":\"%s\",", SCCP_REVISIONSTR);
		astman_append(s, "\"ConfigRevision\":\"%d\",", sccp_config_revision);
		astman_append(s, "\"Segments\":[");
		int comma = 0;

		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			astman_append(s, "%s", comma ? "," : "");
			astman_append(s, "\"%s\"", sccpConfigSegments[i].name);
			comma = 1;
		}
		astman_append(s, "]}\r\n\r\n");
		total++;
	} else {												// return metadata for option in segmnet
		/*
		   JSON:
		   {
		   "Segment": "general", 
		   "Options": [
		   {
		   Option: config->name,
		   Type: ....,
		   Flags : [Required, Deprecated, Obsolete, MultiEntry, RestartRequiredOnUpdate],
		   DefaultValue: ...,
		   Description: ....
		   },
		   {
		   ...
		   }
		   ] 

		   }
		 */
		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			if (sccp_strcaseequals(sccpConfigSegments[i].name, req_segment)) {
				sccpConfigSegment = &sccpConfigSegments[i];
				const SCCPConfigOption *config = sccpConfigSegment->config;

				astman_append(s, "Response: Success\r\n");
				if (!ast_strlen_zero(id)) {
					astman_append(s, "ActionID: %s\r\n", id);
				}

				astman_append(s, "JSON: {");
				astman_append(s, "\"Segment\":\"%s\",", sccpConfigSegment->name);
				astman_append(s, "\"Options\":[");
				uint8_t cur_elem = 0;
				int comma = 0;

				for (cur_elem = 0; cur_elem < sccpConfigSegment->config_size; cur_elem++) {
					if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_IGNORE) != SCCP_CONFIG_FLAG_IGNORE) {
						astman_append(s, "%s", comma ? "," : "");
						astman_append(s, "{");

						{
							astman_append(s, "\"Name\":\"%s\",", config[cur_elem].name);

							switch (config[cur_elem].type) {
								case SCCP_CONFIG_DATATYPE_BOOLEAN:
									astman_append(s, "\"Type\":\"BOOLEAN\",");
									astman_append(s, "\"Size\":%d", (int) config[cur_elem].size - 1);
									break;
								case SCCP_CONFIG_DATATYPE_INT:
									astman_append(s, "\"Type\":\"INT\",");
									astman_append(s, "\"Size\":%d", (int) config[cur_elem].size - 1);
									break;
								case SCCP_CONFIG_DATATYPE_UINT:
									astman_append(s, "\"Type\":\"UNSIGNED INT\",");
									astman_append(s, "\"Size\":%d", (int) config[cur_elem].size - 1);
									break;
								case SCCP_CONFIG_DATATYPE_STRINGPTR:
									astman_append(s, "\"Type\":\" STRING\",");
									astman_append(s, "\"Size\":0");
									break;
								case SCCP_CONFIG_DATATYPE_STRING:
									astman_append(s, "\"Type\":\"STRING\",");
									astman_append(s, "\"Size\":%d", (int) config[cur_elem].size - 1);
									break;
								case SCCP_CONFIG_DATATYPE_PARSER:
									astman_append(s, "\"Type\":\"PARSER\",");
									astman_append(s, "\"Size\":0,");
									astman_append(s, "\"Parser\":\"%s\"", config[cur_elem].parsername);
									break;
								case SCCP_CONFIG_DATATYPE_CHAR:
									astman_append(s, "\"Type\":\"CHAR\",");
									astman_append(s, "\"Size\":1");
									break;
								case SCCP_CONFIG_DATATYPE_ENUM:
									astman_append(s, "\"Type\":\"ENUM\",");
									astman_append(s, "\"Size\":%d,", (int) config[cur_elem].size - 1);
									astman_append(s, "Possible Values: [%s]\r\n", config[cur_elem].all_entries());
									break;
							}
							astman_append(s, ",");

							astman_append(s, "\"Flags\":[");
							{
								int comma1 = 0;

								if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) {
									astman_append(s, "%s", comma1 ? "," : "");
									astman_append(s, "\"Required\"");
									comma1 = 1;
								}
								if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED) {
									astman_append(s, "%s", comma1 ? "," : "");
									astman_append(s, "\"Deprecated\"");
									comma1 = 1;
								}
								if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE) {
									astman_append(s, "%s", comma1 ? "," : "");
									astman_append(s, "\"Obsolete\"");
									comma1 = 1;
								}
								if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) {
									astman_append(s, "%s", comma1 ? "," : "");
									astman_append(s, "\"MultiEntry\"");
									comma1 = 1;
								}
								if ((config[cur_elem].change & SCCP_CONFIG_NEEDDEVICERESET) == SCCP_CONFIG_NEEDDEVICERESET) {
									astman_append(s, "%s", comma1 ? "," : "");
									astman_append(s, "\"RestartRequiredOnUpdate\"");
									comma1 = 1;
								}
							}
							astman_append(s, "]");

							astman_append(s, ",\"DefaultValue\":\"%s\"", config[cur_elem].defaultValue);

							if (strlen(config[cur_elem].description) != 0) {
								char *description = strdupa(config[cur_elem].description);
								char *description_part = "";

								astman_append(s, ",\"Description\":\"");
								while (description && (description_part = strsep(&description, "\n"))) {
									astman_append(s, "%s ", description_part);
								}
								astman_append(s, "\"");
							}
						}
						astman_append(s, "}");
						comma = 1;
					}
				}
				astman_append(s, "]}\r\n\r\n");
				total++;
			}
		}
	}
	return 0;
}

/*!
 * \brief Generate default sccp.conf file
 * \param filename Filename
 * \param configType Config Type:
 *- 0 = templated + registered devices
 *- 1 = required params only
 *- 2 = all params
 * \todo Add functionality
 */
int sccp_config_generate(char *filename, int configType)
{
	const SCCPConfigSegment *sccpConfigSegment = NULL;
	const SCCPConfigOption *config = NULL;
	long unsigned int sccp_option;
	long unsigned int segment;
	char *description = "";
	char *description_part = "";
	char name_and_value[100];
	char size_str[15] = "";
	int linelen = 0;

	char fn[PATH_MAX];

	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, filename);
	pbx_log(LOG_NOTICE, "Creating new config file '%s'\n", fn);

	FILE *f;

	if (!(f = fopen(fn, "w"))) {
		pbx_log(LOG_ERROR, "Error creating new config file \n");
		return 1;
	}

	char date[256] = "";
	time_t t;

	time(&t);
	sccp_copy_string(date, ctime(&t), sizeof(date));

	fprintf(f, ";!\n");
	fprintf(f, ";! Automatically generated configuration file\n");
	fprintf(f, ";! Filename: %s (%s)\n", filename, fn);
	fprintf(f, ";! Generator: sccp config generate\n");
	fprintf(f, ";! Creation Date: %s", date);
	fprintf(f, ";!\n");
	fprintf(f, "\n");

	for (segment = SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {
		sccpConfigSegment = sccp_find_segment(segment);
		if (configType == 0 && (segment == SCCP_CONFIG_DEVICE_SEGMENT || segment == SCCP_CONFIG_LINE_SEGMENT)) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding [%s] template section\n", sccpConfigSegment->name);
			fprintf(f, "\n;\n; %s section\n;\n[default_%s](!)\n", sccpConfigSegment->name, sccpConfigSegment->name);
		} else {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding [%s] section\n", sccpConfigSegment->name);
			fprintf(f, "\n;\n; %s section\n;\n[%s]\n", sccpConfigSegment->name, sccpConfigSegment->name);
		}

		config = sccpConfigSegment->config;
		for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
			if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);

				if (!sccp_strlen_zero(config[sccp_option].name)) {
					if (!sccp_strlen_zero(config[sccp_option].defaultValue)			// non empty
					    || (configType != 2 && ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != SCCP_CONFIG_FLAG_REQUIRED && sccp_strlen_zero(config[sccp_option].defaultValue)))	// empty but required
					    ) {
						snprintf(name_and_value, sizeof(name_and_value), "%s = %s", config[sccp_option].name, sccp_strlen_zero(config[sccp_option].defaultValue) ? "\"\"" : config[sccp_option].defaultValue);
						linelen = (int) strlen(name_and_value);
						fprintf(f, "%s", name_and_value);
						if (!sccp_strlen_zero(config[sccp_option].description)) {
							description = sccp_strdupa(config[sccp_option].description);
							while ((description_part = strsep(&description, "\n"))) {
								if (!sccp_strlen_zero(description_part)) {
									if (config[sccp_option].type == SCCP_CONFIG_DATATYPE_STRING) {
										snprintf(size_str, sizeof(size_str), " (Size: %d)", (int) config[sccp_option].size - 1);
									} else {
										size_str[0] = '\0';
									}
									fprintf(f, "%*.s ; %s%s%s%s\n", 81 - linelen, " ", (config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED ? "(REQUIRED) " : "", ((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "(MULTI-ENTRY)" : "", description_part, size_str);
									linelen = 0;
								}
							}
							if (description_part) {
								sccp_free(description_part);
							}
						} else {
							fprintf(f, "\n");
						}
					}
				} else {
					pbx_log(LOG_ERROR, "Error creating new variable structure for %s='%s'\n", config[sccp_option].name, config[sccp_option].defaultValue);
					fclose(f);
					return 2;
				}
			}
		}
		sccp_log((DEBUGCAT_CONFIG)) ("\n");
	}
	fclose(f);
	pbx_log(LOG_NOTICE, "Created new config file '%s'\n", fn);

	return 0;
};

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
