/*!
 * \file 	sccp_config.c
 * \brief 	SCCP Config Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note 	To find out more about the reload function see \ref sccp_config_reload
 * \remarks     Only methods directly related to chan-sccp configuration should be stored in this source file.
 *
 * $Date: 2010-11-17 18:10:34 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2154 $
 */

/*!
 * \file
 * \ref sccp_config
 *
 * \page sccp_config Loading sccp.conf/realtime configuration implementation
 *
* \section sccp_config_reload How was the new cli command "sccp reload" implemented
 *
 * \code
 * sccp_cli.c
 * 	new implementation of cli reload command
 * 		checks if no other reload command is currently running
 * 		starts loading global settings from sccp.conf (sccp_config_general)
 * 		starts loading devices and lines from sccp.conf(sccp_config_readDevicesLines)
 *
 * sccp_config.c
 * 	modified sccp_config_general
 *
 * 	modified sccp_config_readDevicesLines
 * 		sets pendingDelete for
 * 			devices (via sccp_device_pre_reload),
 * 			lines (via sccp_line_pre_reload)
 * 			softkey (via sccp_softkey_pre_reload)
 *
 * 		calls sccp_config_buildDevice as usual
 * 			calls sccp_config_buildDevice as usual
 * 				find device
 * 				or create new device
 * 				parses sccp.conf for device
 *				set defaults for device if necessary using the default from globals using the same parameter name
 * 				set pendingUpdate on device for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 * 			calls sccp_config_buildLine as usual
 * 				find line
 * 				or create new line
 * 				parses sccp.conf for line
 *				set defaults for line if necessary using the default from globals using the same parameter name
 *			 	set pendingUpdate on line for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 * 			calls sccp_config_softKeySet as usual ***
 * 				find softKeySet
 *				or create new softKeySet
 *			 	parses sccp.conf for softKeySet
 * 				set pendingUpdate on softKetSet for parameters marked with SCCP_CONFIG_NEEDDEVICERESET (remove pendingDelete)
 *
 * 		checks pendingDelete and pendingUpdate for
 *			skip when call in progress
 * 			devices (via sccp_device_post_reload),
 * 				resets GLOB(device) if pendingUpdate
 * 				removes GLOB(devices) with pendingDelete
 * 			lines (via sccp_line_post_reload)
 * 				resets GLOB(lines) if pendingUpdate
 * 				removes GLOB(lines) with pendingDelete
 * 			softkey (via sccp_softkey_post_reload) ***
 * 				resets GLOB(softkeyset) if pendingUpdate ***
 * 				removes GLOB(softkeyset) with pendingDelete ***
 *
 * channel.c
 * 	sccp_channel_endcall ***
 *		reset device if still device->pendingUpdate,line->pendingUpdate or softkeyset->pendingUpdate
 *
 * \endcode
 *
 * lines marked with "***" still need be implemented
 *
 */

#include "config.h"
#include "common.h"
#include <asterisk/paths.h>

SCCP_FILE_VERSION(__FILE__, "$Revision: 2154 $")
#ifndef offsetof
#if defined(__GNUC__) && __GNUC__ > 3
#    define offsetof(type, member)  __builtin_offsetof (type, member)
#else
#    define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif
#endif
#define offsize(T, F) sizeof(((T *)0)->F)
#define G_OBJ_REF(x) offsetof(struct sccp_global_vars,x), offsize(struct sccp_global_vars,x)
#define D_OBJ_REF(x) offsetof(struct sccp_device,x), offsize(struct sccp_device,x)
#define L_OBJ_REF(x) offsetof(struct sccp_line,x), offsize(struct sccp_line,x)
#define S_OBJ_REF(x) offsetof(struct softKeySetConfiguration,x), offsize(struct softKeySetConfiguration,x)
#define H_OBJ_REF(x) offsetof(struct sccp_hotline,x), offsize(struct sccp_hotline,x)

/* dyn config */

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
	SCCP_CONFIG_DATATYPE_STRINGPTR			= 1 << 5,	/* pointer */
	SCCP_CONFIG_DATATYPE_CHAR			= 1 << 6,
	SCCP_CONFIG_DATATYPE_ENUM2INT			= 1 << 7,
	SCCP_CONFIG_DATATYPE_ENUM2STR			= 1 << 8,
	SCCP_CONFIG_DATATYPE_CSV2STR			= 1 << 9,
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
        SCCP_CONFIG_FLAG_MULTI_ENTRY                    = 1 << 8,               /*< multi entries allowed */
/* *INDENT-ON* */
};

/*!
 * \brief SCCP Config Option Struct
 */
typedef struct SCCPConfigOption {

/* *INDENT-ON* */
	const char *name;							/*!< Configuration Parameter Name */
	const int offset;							/*!< The offset relative to the context structure where the option value is stored. */
	const size_t size;							/*!< Structure size */
	enum SCCPConfigOptionType type;						/*!< Data type */
	sccp_value_changed_t(*converter_f) (void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);	/*!< Conversion function */
	const uint32_t (*str2enumval)(const char *str);				/*!< generic convertor used for parsing OptionType: SCCP_CONFIG_DATATYPE_ENUM */
	const char *(*enumkeys)(void);						/*!< reverse convertor used for parsing OptionType: SCCP_CONFIG_DATATYPE_ENUM, to retrieve all possible values allowed */
	enum SCCPConfigOptionFlag flags;					/*!< Data type */
	sccp_configurationchange_t change;					/*!< Does a change of this value needs a device restart */
	const char *defaultValue;						/*!< Default value */
	const char *description;						/*!< Configuration description (config file) or warning message for deprecated or obsolete values */
/* *INDENT-OFF* */
} SCCPConfigOption;

//converter function prototypes 
sccp_value_changed_t sccp_config_parse_codec_preferences(void *dest, const size_t size, const char *value, const boolean_t allow, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_allow_codec (void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_disallow_codec (void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_mailbox(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_tos(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_cos(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_amaflags(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_variables(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_group(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_permit(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_deny(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_button(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_permithosts(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_addons(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_privacyFeature(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_earlyrtp(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_dtmfmode(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_mwilamp(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_debug(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_ipaddress(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_port(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_blindtransferindication(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_callanswerorder(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_context(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_context(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_exten(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_enable(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_force(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_log(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);

#include "sccp_config_entries.h"

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
		if (sccpConfigSegments[i].segment == segment)
			return &sccpConfigSegments[i];
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

	for (i = 0; i < sccpConfigSegment->config_size; i++) {
		if (!strcasecmp(config[i].name, name))
			return &config[i];
	}

	return NULL;
}

/*!
 * \brief Parse SCCP Config Option Value
 *
 * \todo add errormsg return to sccpConfigOption->converter_f: so we can have a fixed format the returned errors to the user
 */
static sccp_configurationchange_t sccp_config_object_setValue(void *obj, const char *name, const char *value, uint8_t lineno, const sccp_config_segment_t segment)
{
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *sccpConfigOption = sccp_find_config(segment, name);
	void *dst;
	int type;								/* enum wrapper */
	int flags;								/* enum wrapper */

	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;		/* indicates config value is changed or not */
	sccp_configurationchange_t changes = SCCP_CONFIG_NOUPDATENEEDED;

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
	
	char **multi_value, *value_part;
	int x;

	if (!sccpConfigOption) {
		pbx_log(LOG_WARNING, "Unknown param at %s:%d:%s='%s'\n", sccpConfigSegment->name, lineno, name, value);
		return SCCP_CONFIG_NOUPDATENEEDED;
	}

	if (sccpConfigOption->offset <= 0)
		return SCCP_CONFIG_NOUPDATENEEDED;

	dst = ((uint8_t *) obj) + sccpConfigOption->offset;
	type = sccpConfigOption->type;
	flags = sccpConfigOption->flags;

	if ((flags & SCCP_CONFIG_FLAG_IGNORE) == SCCP_CONFIG_FLAG_IGNORE) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "config parameter %s='%s' in line %d ignored\n", name, value, lineno);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} else if ((flags & SCCP_CONFIG_FLAG_CHANGED) == SCCP_CONFIG_FLAG_CHANGED) {	
		pbx_log(LOG_NOTICE, "changed config param at %s='%s' in line %d\n - %s -> please check sccp.conf file\n", name, value, lineno, sccpConfigOption->description);
	} else if ((flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED) {	
		pbx_log(LOG_NOTICE, "deprecated config param at %s='%s' in line %d\n - %s -> using old implementation\n", name, value, lineno, sccpConfigOption->description);
	} else if ((flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE) {
		pbx_log(LOG_WARNING, "obsolete config param at %s='%s' in line %d\n - %s -> param skipped\n", name, value, lineno, sccpConfigOption->description);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} else if ((flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) {
		if (NULL == value) {
			pbx_log(LOG_WARNING, "required config param at %s='%s' - %s\n", name, value, sccpConfigOption->description);
			changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
                        return SCCP_CONFIG_ERROR;
		}
	}

	/* warn user that value is being overwritten */
	switch (type) {
	  
	case SCCP_CONFIG_DATATYPE_CHAR:
		oldChar = *(char *)dst;

		if (!sccp_strlen_zero(value)) {
			if ( oldChar != value[0] ) {
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				*(char *)dst = value[0];
			}
		}
		break;  
	
	case SCCP_CONFIG_DATATYPE_STRING:
		str = (char *)dst;

		if (strcasecmp(str, value) ) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "config parameter %s '%s' != '%s'\n", name, str, value);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			pbx_copy_string(dst, value, sccpConfigOption->size);
//		}else{
//			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "config parameter %s '%s' == '%s'\n", name, str, value);
		}
		break;

	case SCCP_CONFIG_DATATYPE_STRINGPTR:
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
		str = *(void **)dst;

		if (!sccp_strlen_zero(value)) {
			if (str) {
				if (strcasecmp(str, value)) {
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					/* there is a value already, free it */
					free(str);
					*(void **)dst = strdup(value);
				}
			} else {
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				*(void **)dst = strdup(value);
			}

		} else if (!sccp_strlen_zero(str)) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			/* there is a value already, free it */
			free(str);
			*(void **)dst = NULL;
		}
		break;

	case SCCP_CONFIG_DATATYPE_INT:
		if (!sccp_strlen_zero(value)) {
//			pbx_log(LOG_NOTICE, "name: %s, size %d, cmp %d, sscan %d\n", name, (int)sccpConfigOption->size, strncmp("0x",value, 2), sscanf(value, "%hx", &int8num));
			switch (sccpConfigOption->size)	 {
				case 1:
					if (sscanf(value, "%hd", &int8num) == 1) {
						if ((*(int8_t *)dst) != int8num) {
							*(int8_t *)dst = int8num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 2:
					if (sscanf(value, "%d", &int16num) == 1) {
						if ((*(int16_t *)dst) != int16num) {
							*(int16_t *)dst = int16num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 4:
					if (sscanf(value, "%ld", &int32num) == 1) {
						if ((*(int32_t *)dst) != int32num) {
							*(int32_t *)dst = int32num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 8:
					if (sscanf(value, "%lld", &int64num) == 1) {
						if ((*(int64_t *)dst) != int64num) {
							*(int64_t *)dst = int64num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
			}
		}
		break;
	case SCCP_CONFIG_DATATYPE_UINT:
		if (!sccp_strlen_zero(value)) {
//			pbx_log(LOG_NOTICE, "name: %s, size %d, cmp %d, sscan %d\n", name, (int)sccpConfigOption->size, strncmp("0x",value, 2), sscanf(value, "%hx", &int8num));
			switch (sccpConfigOption->size)	 {
				case 1:
					if ( (!strncmp("0x",value, 2) && sscanf(value, "%hx", &uint8num)) || (sscanf(value, "%hu", &uint8num) == 1) ) {
						if ((*(uint8_t *)dst) != uint8num) {
							*(uint8_t *)dst = uint8num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 2:
					if (sscanf(value, "%u", &uint16num) == 1) {
						if ((*(uint16_t *)dst) != uint16num) {
							*(uint16_t *)dst = uint16num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 4:
					if (sscanf(value, "%lu", &uint32num) == 1) {
						if ((*(uint32_t *)dst) != uint32num) {
							*(uint32_t *)dst = uint32num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 8:
					if (sscanf(value, "%llu", &uint64num) == 1) {
						if ((*(uint64_t *)dst) != uint64num) {
							*(uint64_t *)dst = uint64num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
			}
		}
		break;

	case SCCP_CONFIG_DATATYPE_BOOLEAN:
		boolean = sccp_true(value);

		if (*(boolean_t *) dst != boolean) {
			*(boolean_t *) dst = sccp_true(value);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
		break;

	case SCCP_CONFIG_DATATYPE_PARSER:
		if (sccpConfigOption->converter_f) {
			changed = sccpConfigOption->converter_f(dst, sccpConfigOption->size, value, segment);
		}
		break;

	case SCCP_CONFIG_DATATYPE_ENUM2INT:
                if (sccpConfigOption->str2enumval) {
        	        multi_value = explode(value,",");
	                for(x=0; x<=ARRAY_LEN(multi_value); x++) {
                                value_part = pbx_skip_blanks(multi_value[x]);
	                        pbx_trim_blanks(value_part);
                                switch (sccpConfigOption->size)	 {
                                        case 1:
                                                uint8num=(uint8_t)sccpConfigOption->str2enumval(value_part);
                                                if ((*(uint8_t *)dst | uint8num) != uint8num) {
                                                        *(uint32_t *)dst |= uint8num;
                                                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                                                }
                                        case 2:
                                                uint16num=(uint16_t)sccpConfigOption->str2enumval(value_part);
                                                if ((*(uint16_t *)dst | uint16num) != uint16num) {
                                                        *(uint16_t *)dst |= uint16num; 
                                                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                                                }
                                        case 4:
                                                uint32num=(uint32_t)sccpConfigOption->str2enumval(value_part);
                                                if ((*(uint32_t *)dst | uint32num) != uint32num) {
                                                        *(uint32_t *)dst |= uint32num; 
                                                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                                                }
                                        case 8:
                                                uint64num=(uint64_t)sccpConfigOption->str2enumval(value_part);
                                                if ((*(uint64_t *)dst | uint64num) != uint64num) {
                                                        *(uint64_t *)dst |= uint64num; 
                                                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                                                }
                                }	
                        }        
                        sccp_free(multi_value);
                } else {
        		pbx_log(LOG_ERROR, "No parser available to parse %s='%s'\n", name, value);
	        	return SCCP_CONFIG_NOUPDATENEEDED;
                }
		break;

	case SCCP_CONFIG_DATATYPE_ENUM2STR:
/*                if (sccpConfigOption->str2enumstrl) {
        	        multi_value = explode(value,",");
	                for(x=0; x<=ARRAY_LEN(multi_value); x++) {
                                value_part = pbx_skip_blanks(multi_value[x]);
	                        pbx_trim_blanks(value_part);
                                if (!strcasecmp(&(*(char *)dst), value_part)) {
                                        if (!strcasecmp(value,sccpConfigOption->str2enumstr(value_part)) {
                                                *(char **)dest = strdup(value_part);
                                                changed = SCCP_CONFIG_CHANGE_CHANGED;
                                        }
                                }
                        }
                        sccp_free(multi_value);
                } */
		break;

	case SCCP_CONFIG_DATATYPE_CSV2STR:
/*                if (sccpConfigOption->str2enumval) {
        	        multi_value = explode(value,",");
	                for(x=0; x<=ARRAY_LEN(multi_value); x++) {
                                value_part = pbx_skip_blanks(multi_value[x]);
	                        pbx_trim_blanks(value_part);
                                if (!strcasecmp(&(*(char *)dst), value_part)) {
                                        *(char **)dest = strdup(value_part);
                                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                                }
                        }
                        sccp_free(multi_value);
                }*/
		break;
	default:
		pbx_log(LOG_WARNING, "Unknown param at %s='%s'\n", name, value);
		return SCCP_CONFIG_NOUPDATENEEDED;
	}

	/* set changed value if changed */
	if (SCCP_CONFIG_CHANGE_CHANGED == changed) {
		sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_2 "config parameter %s='%s' in line %d changed. %s\n", name, value, lineno, SCCP_CONFIG_NEEDDEVICERESET==sccpConfigOption->change ? "(causes device reset)": "");
		changes = sccpConfigOption->change;
	}

	return changes;
}

/*!
 * \brief Set SCCP obj defaults from predecessor (device / global)
 *
 * check if we can find the param name in the segment specified and retrieving its value or default value
 * copy the string from the defaultSegment and run through the converter again
 */
void sccp_config_set_defaults(void *obj, const sccp_config_segment_t segment, const uint8_t alreadySetEntries[], uint8_t arraySize)
{
	uint8_t i = 0;
	const SCCPConfigOption *sccpDstConfig = sccp_find_segment(segment)->config;
	const SCCPConfigOption *sccpDefaultConfigOption;
	sccp_device_t *my_device = NULL;
	sccp_line_t *my_line = NULL;
	sccp_device_t *ref_device = NULL;						/* need to find a way to find the default device to copy */
	int flags;									/* enum wrapper */
	int type;									/* enum wrapper */
	const char *value="";								/*! \todo retrieve value from correct segment */
	boolean_t valueFound=FALSE;
	PBX_VARIABLE_TYPE *v;
	
	/* check if not already set using it's own parameter in the sccp.conf file */
	switch (segment) {
		case SCCP_CONFIG_GLOBAL_SEGMENT:
			arraySize = ARRAY_LEN(sccpGlobalConfigOptions);
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "setting [general] defaults\n");
			break;
		case SCCP_CONFIG_DEVICE_SEGMENT:
			my_device = &(*(sccp_device_t *)obj);
			arraySize = ARRAY_LEN(sccpDeviceConfigOptions);
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "setting device[%s] defaults\n", my_device ? my_device->id : "NULL");
			break;
		case SCCP_CONFIG_LINE_SEGMENT:			
			my_line = &(*(sccp_line_t *)obj);			
			arraySize = ARRAY_LEN(sccpLineConfigOptions);
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "setting line[%s] defaults\n", my_line ? my_line->name : "NULL" );
			break;
		case SCCP_CONFIG_SOFTKEY_SEGMENT:
		        ast_log(LOG_ERROR, "softkey default loading not implemented yet\n");
			break;
	}

	/* find the defaultValue, first check the reference, if no reference is specified, us the local defaultValue */
	for (i = 0; i < arraySize; i++) {			
		flags = sccpDstConfig[i].flags;
		type = sccpDstConfig[i].type;

		// if (flags) { 
		if (alreadySetEntries[i]==0 && !(flags & SCCP_CONFIG_FLAG_OBSOLETE) ) {		// has not been set already
                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s looking for default (flags: %d, type: %d)\n", sccpDstConfig[i].name, flags, type);
                        if ((flags & SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT) == SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT) {
                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s refering to device default\n", sccpDstConfig[i].name);
                                ref_device = &(*(sccp_device_t *) obj);
                                
                                // get value from cfg->device category
                                valueFound=FALSE;
                                for (v = ast_variable_browse(GLOB(cfg), (const char *)ref_device->id); v; v = v->next) {
                                        if (!strcasecmp((const char *)sccpDstConfig[i].name, v->name)) {
                                                value = v->value;
                                                if (!sccp_strlen_zero(value)) {
                                                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using default %s\n", sccpDstConfig[i].name, value);
                                                        // get value from the config file and run through parser
                                                        sccp_config_object_setValue(obj, sccpDstConfig[i].name, value, 0, segment);
                                                }
                                                valueFound=TRUE;
                                        }
                                }
                                if (!valueFound) {
                                        // get defaultValue from default_segment
                                        sccpDefaultConfigOption = sccp_find_config(SCCP_CONFIG_DEVICE_SEGMENT, sccpDstConfig[i].name);
                                        if(sccpDefaultConfigOption) {
                                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s found value:%s in device source segment\n", sccpDstConfig[i].name, value);
                                                value = sccpDefaultConfigOption->defaultValue;
                                                if (!sccp_strlen_zero(value)) {
                                                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using device default %s\n", sccpDstConfig[i].name, value);
                                                        // get value from the config file and run through parser
                                                        sccp_config_object_setValue(obj, sccpDstConfig[i].name, value, 0, segment);
                                                }
                                        } else {
                                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s not found in device source segment\n", sccpDstConfig[i].name);
                                        }
                                }
                        } else if ((flags & SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT) == SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT) {
                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s refering to global default\n", sccpDstConfig[i].name);
                                // get value from cfg->general category

                                valueFound=FALSE;
                                for (v = ast_variable_browse(GLOB(cfg), "general"); v; v = v->next) {
                                        if (!strcasecmp(sccpDstConfig[i].name, v->name)) {
                                                value = v->value;
                                                if (!sccp_strlen_zero(value)) {
                                                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using global default %s\n", sccpDstConfig[i].name, value);
                                                        // get value from the config file and run through parser
                                                        sccp_config_object_setValue(obj, sccpDstConfig[i].name, value, 0, segment);
                                                }
                                                valueFound=TRUE;
                                        }
                                }
                                if (!valueFound) {
                                        // get defaultValue from default_segment
                                        sccpDefaultConfigOption = sccp_find_config(SCCP_CONFIG_GLOBAL_SEGMENT, sccpDstConfig[i].name);
                                        if(sccpDefaultConfigOption) {
                                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s found value:%s in global source segment\n", sccpDstConfig[i].name, value);
                                                value = sccpDefaultConfigOption->defaultValue;
                                                if (!sccp_strlen_zero(value)) {
                                                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using default %s\n", sccpDstConfig[i].name, value);
                                                        // get value from the config file and run through parser
                                                        sccp_config_object_setValue(obj, sccpDstConfig[i].name, value, 0, segment);
                                                }
                                        } else {
                                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s not found in global source segment\n", sccpDstConfig[i].name);
                                        }
                                }
                                value = (char *)pbx_variable_retrieve(GLOB(cfg), "general", sccpDstConfig[i].name);
                        } else {
                                // get defaultValue from self
                                sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using local source segment default: %s -> %s\n", sccpDstConfig[i].name, value, sccpDstConfig[i].defaultValue);
                                value = sccpDstConfig[i].defaultValue;
                                if (!sccp_strlen_zero(value)) {
                                        sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "config parameter %s using default %s\n", sccpDstConfig[i].name, value);
                                        // get value from the config file and run through parser
                                        sccp_config_object_setValue(obj, sccpDstConfig[i].name, value, 0, segment);
                                }
                        }
		}
	}
}

/*!
 * \brief Config Converter/Parser for Debug
 */
sccp_value_changed_t sccp_config_parse_debug(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	uint32_t debug_new = 0;

	char *debug_arr[1];

	debug_arr[0] = strdup(value);

	debug_new = sccp_parse_debugline(debug_arr, 0, 1, debug_new);
	if (*(uint32_t *) dest != debug_new) {
		*(uint32_t *) dest = debug_new;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	sccp_free(debug_arr[0]);
	return changed;
}

/*!
 * \brief Config Converter/Parser for Bind Address
 */
sccp_value_changed_t sccp_config_parse_ipaddress(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

#ifdef CS_EXPERIMENTAL_NEWIP
	struct sockaddr_in *bindaddr_prev = &(*(struct sockaddr_in *)dest);

	char curval[256], *port=NULL, *host=NULL, *splitter;
	sccp_copy_string(curval, value, sizeof(curval));
	splitter=curval;
	host = strsep(&splitter, ":");
 	port = splitter;

	int status;
	struct addrinfo hints, *res, *p;
	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
		pbx_log(LOG_WARNING, "Invalid address: %s. error: %s. SCCP disabled!\n", value, gai_strerror(status));
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

//	for(p = res;p != NULL; p = p->ai_next) {		// use sockaddr_storage to store multiple results
	p=res;
		if (p != NULL) {
			if (p->ai_family == AF_INET) { 	// IPv4
				if (bindaddr_prev->sin_addr.s_addr != (((struct sockaddr_in *)p->ai_addr)->sin_addr).s_addr || bindaddr_prev->sin_port != ((struct sockaddr_in *)p->ai_addr)->sin_port) {
					bindaddr_prev->sin_family=((struct sockaddr_in *)p->ai_addr)->sin_family;
					memcpy(&bindaddr_prev->sin_addr, &(((struct sockaddr_in *)p->ai_addr)->sin_addr), sizeof(struct in_addr));
					bindaddr_prev->sin_port=((struct sockaddr_in *)p->ai_addr)->sin_port;
					changed = SCCP_CONFIG_CHANGE_CHANGED;
				}
//			else 				// IPv6
//				if (bindaddr_prev->sin_addr.s_addr != ((struct sockaddr_in6 *)p->ai_addr).s_addr || bindaddr_prev->sin_port != ((struct sockaddr_in6 *)p->ai_addr)->sin_port) {
//					bindaddr_prev->sin_family=((struct sockaddr_in6 *)p->ai_addr)->sin_family;
//					memcpy(&bindaddr_prev->sin_addr, &(((struct sockaddr_in6 *)p->ai_addr)->sin_addr), sizeof(struct in_addr));
//					bindaddr_prev->sin_port=((struct sockaddr_in6 *)p->ai_addr)->sin_port;
//					changed = SCCP_CONFIG_CHANGE_CHANGED;
//				}
			}
		}
//		char ipstr[INET6_ADDRSTRLEN];
//		inet_ntop(p->ai_family, &(((struct sockaddr_in *)p->ai_addr)->sin_addr), ipstr, sizeof ipstr);
//		pbx_log(LOG_WARNING, "host: %s, port: %d\n", ipstr, htons((((struct sockaddr_in *)p->ai_addr)->sin_port)));
//	}
        freeaddrinfo(res); // free the linked-list
#else
	struct ast_hostent ahp;
	struct hostent *hp;

	struct sockaddr_in *bindaddr_prev = &(*(struct sockaddr_in *)dest);
	struct sockaddr_in *bindaddr_new = NULL;
	
	if (!(hp = pbx_gethostbyname(value, &ahp))) {
		pbx_log(LOG_WARNING, "Invalid address: %s. SCCP disabled\n", value);
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	
	if (&bindaddr_prev->sin_addr != NULL && hp != NULL) {
		if ((bindaddr_new = sccp_malloc(sizeof(struct sockaddr_in)))) {
			memcpy(&bindaddr_new->sin_addr, hp->h_addr, sizeof(struct in_addr));
			if (bindaddr_prev->sin_addr.s_addr != bindaddr_new->sin_addr.s_addr) {
				memcpy(&bindaddr_prev->sin_addr, hp->h_addr, sizeof(struct in_addr));
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
			sccp_free(bindaddr_new);
		}
	} else {
		memcpy(&bindaddr_prev->sin_addr, hp->h_addr, sizeof(struct in_addr));
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
#endif
	return changed;
}

/*!
 * \brief Config Converter/Parser for Port
 */
sccp_value_changed_t sccp_config_parse_port(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	int new_port;
	struct sockaddr_in *bindaddr_prev = &(*(struct sockaddr_in *)dest);

	if (sscanf(value, "%i", &new_port) == 1) {
		if (&bindaddr_prev->sin_port != NULL) {				// reload
			if (bindaddr_prev->sin_port != htons(new_port)) {
				bindaddr_prev->sin_port = htons(new_port);
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
		} else {
			bindaddr_prev->sin_port = htons(new_port);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
	} else {
		pbx_log(LOG_WARNING, "Invalid port number '%s'\n", value);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for BlindTransferIndication
 */
sccp_value_changed_t sccp_config_parse_blindtransferindication(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	boolean_t blindtransferindication = *(boolean_t *) dest;

	if (!strcasecmp(value, "moh")) {
		blindtransferindication = SCCP_BLINDTRANSFER_MOH;
	} else if (!strcasecmp(value, "ring")) {
		blindtransferindication = SCCP_BLINDTRANSFER_RING;
	} else {
		pbx_log(LOG_WARNING, "Invalid blindtransferindication value, should be 'moh' or 'ring'\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (*(boolean_t *) dest != blindtransferindication) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		*(boolean_t *) dest = blindtransferindication;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for ProtocolVersion
 *
 * \todo Can we completely loose the option to set the protocol version and just embed the max_protocol version in the source
 */
/*
sccp_value_changed_t sccp_config_parse_protocolversion(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int protocolversion = *(int *)dest;
	int new_value;

	if (sscanf(value, "%i", &new_value) == 1) {
		if (new_value < SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW) {
			new_value = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
		} else if (new_value > SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH) {
			new_value = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
		}
		if (protocolversion != new_value) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			*(int *)dest = new_value;
		}
	} else {
		pbx_log(LOG_WARNING, "Invalid protocol version value, has to be a number between '%d' and '%d'\n", SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW, SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	return changed;
}
*/

/*!
 * \brief Config Converter/Parser for Call Answer Order
 */
sccp_value_changed_t sccp_config_parse_callanswerorder(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	call_answer_order_t callanswerorder = *(call_answer_order_t *) dest;
	call_answer_order_t new_value;

	if (!strcasecmp(value, "oldestfirst")) {
		new_value = ANSWER_OLDEST_FIRST;
	} else if (!strcasecmp(value, "lastfirst")) {
		new_value = ANSWER_LAST_FIRST;
	} else {
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (callanswerorder != new_value) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		*(call_answer_order_t *) dest = new_value;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Codec Preferences
 *
 * \todo need check to see if preferred_codecs has changed
 * \todo do we need to reduce the preferences by the pbx -> skinny codec mapping ?
 */
sccp_value_changed_t sccp_config_parse_codec_preferences(void *dest, const size_t size, const char *value, const boolean_t allow, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	skinny_codec_t *preferred_codecs = &(*(skinny_codec_t *) dest);

//	skinny_codec_t preferred_codecs_prev[SKINNY_MAX_CAPABILITIES];
//	memcpy(preferred_codecs_prev, preferred_codecs, sizeof(preferred_codecs));

	if (!sccp_parse_allow_disallow(preferred_codecs, NULL, value, allow)) {
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else if (1==1){		/*\todo implement check */
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Allow Codec Preferences
 */
sccp_value_changed_t sccp_config_parse_allow_codec(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	return sccp_config_parse_codec_preferences(dest, size, value, TRUE, segment);
}

/*!
 * \brief Config Converter/Parser for Disallow Codec Preferences
 */
sccp_value_changed_t sccp_config_parse_disallow_codec(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	return sccp_config_parse_codec_preferences(dest, size, value, FALSE, segment);
}

/*!
 * \brief Config Converter/Parser for Permit IP
 *
 * \todo need check to see if ha has changed
 */
sccp_value_changed_t sccp_config_parse_permit(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int error=0;

	struct sccp_ha *ha = *(struct sccp_ha **)dest;

	if (!strcasecmp(value,"internal")) {
		ha = sccp_append_ha("permit", "127.0.0.0/255.0.0.0", ha, &error);
		ha = sccp_append_ha("permit", "10.0.0.0/255.0.0.0", ha, &error);
		ha = sccp_append_ha("permit", "172.16.0.0/255.224.0.0", ha, &error);
		ha = sccp_append_ha("permit", "192.168.0.0/255.255.0.0", ha, &error);
	} else {
		ha = sccp_append_ha("permit", value, ha, &error);
	}
	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Permit: %s\n", value);

        if (!error)
        	*(struct sccp_ha**)dest = ha;
	
	return changed;
}

/*!
 * \brief Config Converter/Parser for Deny IP
 *
 * \todo need check to see if ha has changed
 */
sccp_value_changed_t sccp_config_parse_deny(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int error=0;

	struct sccp_ha *ha = *(struct sccp_ha **)dest;

	ha = sccp_append_ha("deny", value, ha, &error);

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Deny: %s\n", value);

        if (!error)
        	*(struct sccp_ha **)dest = ha;

	return changed;
}

/*!
 * \brief Config Converter/Parser for Buttons
 *
 * \todo Build a check to see if the button has changed 
 */
sccp_value_changed_t sccp_config_parse_button(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	char *buttonType = NULL, *buttonName = NULL, *buttonOption = NULL, *buttonArgs = NULL;
	char k_button[256];
	char *splitter;

#ifdef CS_DYNAMIC_CONFIG
	button_type_t type;
	unsigned i;
#endif
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Found buttonconfig: %s\n", value);
	sccp_copy_string(k_button, value, sizeof(k_button));
	splitter = k_button;
	buttonType = strsep(&splitter, ",");
	buttonName = strsep(&splitter, ",");
	buttonOption = strsep(&splitter, ",");
	buttonArgs = splitter;
#ifdef CS_DYNAMIC_CONFIG
	for (i = 0; i < ARRAY_LEN(sccp_buttontypes) && strcasecmp(buttonType, sccp_buttontypes[i].text); ++i) ;
	if (i >= ARRAY_LEN(sccp_buttontypes)) {
		pbx_log(LOG_WARNING, "Unknown button type '%s'.\n", buttonType);
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	type = sccp_buttontypes[i].buttontype;
#endif
//	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonType: %s\n", buttonType);
//	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonName: %s\n", buttonName);
//	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ButtonOption: %s\n", buttonOption);

	changed = sccp_config_addButton(dest, 0, type, buttonName ? pbx_strip(buttonName) : buttonType, buttonOption ? pbx_strip(buttonOption) : NULL, buttonArgs ? pbx_strip(buttonArgs) : NULL);

	return changed;
}

/*!
 * \brief Config Converter/Parser for Permit Hosts
 *
 * \todo maybe add new DATATYPE_LIST to add string param value to LIST_HEAD to make a generic parser
 * \todo need check to see if  has changed
 */
sccp_value_changed_t sccp_config_parse_permithosts(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
//	sccp_hostname_t *permithost = &(*(sccp_hostname_t *) dest);
	sccp_hostname_t *permithost = NULL;

	if ((permithost = sccp_malloc(sizeof(sccp_hostname_t)))) {
		if (strcasecmp(permithost->name, value)) {
			sccp_copy_string(permithost->name, value, sizeof(permithost->name));
			SCCP_LIST_INSERT_HEAD(&(*(SCCP_LIST_HEAD(, sccp_hostname_t) *) dest), permithost, list);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
	} else {
		pbx_log(LOG_WARNING, "Error getting memory to assign hostname '%s' (malloc)\n", value);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for addons
 * 
 * \todo make more generic
 * \todo cleanup original implementation in sccp_utils.c
 */
sccp_value_changed_t sccp_config_parse_addons(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	int addon_type;
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;

	SCCP_LIST_HEAD(, sccp_addon_t) * addonList = dest;

	// checking if addontype is known
	if (!strcasecmp(value, "7914"))
		addon_type = SKINNY_DEVICETYPE_CISCO7914;
	else if (!strcasecmp(value, "7915"))
		addon_type = SKINNY_DEVICETYPE_CISCO7915;
	else if (!strcasecmp(value, "7916"))
		addon_type = SKINNY_DEVICETYPE_CISCO7916;
	else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Unknown addon type (%s)\n", value);
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	/*!\todo check allowed addons during the registration process, so we can use skinny device type instead of user defined type -MC */
	// checking if addontype is supported by devicetype
//      if (!((addon_type == SKINNY_DEVICETYPE_CISCO7914) &&
//            ((!strcasecmp(d->config_type, "7960")) ||
//             (!strcasecmp(d->config_type, "7961")) ||
//             (!strcasecmp(d->config_type, "7962")) ||
//             (!strcasecmp(d->config_type, "7965")) ||
//             (!strcasecmp(d->config_type, "7970")) || (!strcasecmp(d->config_type, "7971")) || (!strcasecmp(d->config_type, "7975")))) && !((addon_type == SKINNY_DEVICETYPE_CISCO7915) && ((!strcasecmp(d->config_type, "7962")) || (!strcasecmp(d->config_type, "7965")) || (!strcasecmp(d->config_type, "7975")))) && !((addon_type == SKINNY_DEVICETYPE_CISCO7916) && ((!strcasecmp(d->config_type, "7962")) || (!strcasecmp(d->config_type, "7965")) || (!strcasecmp(d->config_type, "7975"))))) {
//              sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Configured device (%s) does not support the specified addon type (%s)\n", d->config_type, value);
//              return changed;
//      }

	// add addon_type to list head
	sccp_addon_t *addon = sccp_malloc(sizeof(sccp_addon_t));

	if (!addon) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Unable to allocate memory for a device addon\n");
		return changed;
	}
	memset(addon, 0, sizeof(sccp_addon_t));

	addon->type = addon_type;

	SCCP_LIST_INSERT_HEAD(addonList, addon, list);

	return SCCP_CONFIG_CHANGE_CHANGED;
}

/*!
 * \brief Config Converter/Parser for privacyFeature
 *
 * \todo malloc/calloc of privacyFeature necessary ?
 */
sccp_value_changed_t sccp_config_parse_privacyFeature(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_featureConfiguration_t privacyFeature;				// = malloc(sizeof(sccp_featureConfiguration_t));

	if (!strcasecmp(value, "full")) {
		privacyFeature.status = ~0;
		privacyFeature.enabled = TRUE;
	} else if (sccp_true(value) || !sccp_true(value)) {
		privacyFeature.status = 0;
		privacyFeature.enabled = sccp_true(value);
	} else {
		pbx_log(LOG_WARNING, "Invalid privacy value, should be 'full', 'on' or 'off'\n");
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (privacyFeature.status != (*(sccp_featureConfiguration_t *)dest).status || privacyFeature.enabled != (*(sccp_featureConfiguration_t *)dest).enabled) {
		*(sccp_featureConfiguration_t *)dest = privacyFeature;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for early RTP
 */
sccp_value_changed_t sccp_config_parse_earlyrtp(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_channelState_t earlyrtp = 0;

	if (!strcasecmp(value, "none")) {
		earlyrtp = 0;
	} else if (!strcasecmp(value, "offhook")) {
		earlyrtp = SCCP_CHANNELSTATE_OFFHOOK;
	} else if (!strcasecmp(value, "dial")) {
		earlyrtp = SCCP_CHANNELSTATE_DIALING;
	} else if (!strcasecmp(value, "ringout")) {
		earlyrtp = SCCP_CHANNELSTATE_RINGOUT;
	} else if (!strcasecmp(value, "progress")) {
		earlyrtp = SCCP_CHANNELSTATE_PROGRESS;
	} else {
		pbx_log(LOG_WARNING, "Invalid earlyrtp state value, should be 'none', 'offhook', 'dial', 'ringout', 'progress'\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (*(sccp_channelState_t *)dest != earlyrtp ) {
		*(sccp_channelState_t *)dest = earlyrtp;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for dtmfmode
 */
sccp_value_changed_t sccp_config_parse_dtmfmode(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	boolean_t dtmfmode = 0;

	if (!strcasecmp(value, "outofband")) {
		dtmfmode = SCCP_DTMFMODE_OUTOFBAND;
	} else if (!strcasecmp(value, "inband")) {
		dtmfmode = SCCP_DTMFMODE_INBAND;
	} else {
		pbx_log(LOG_WARNING, "Invalid dtmfmode value, should be either 'inband' or 'outofband'\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (*(boolean_t *) dest != dtmfmode) {
		*(boolean_t *) dest = dtmfmode;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for mwilamp
 */
sccp_value_changed_t sccp_config_parse_mwilamp(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_lampMode_t mwilamp = 0;

	if (!sccp_true(value)) {
		mwilamp = SKINNY_LAMP_OFF;
	} else if (sccp_true(value)) {
		mwilamp = SKINNY_LAMP_ON;
	} else if (!strcasecmp(value, "wink")) {
		mwilamp = SKINNY_LAMP_WINK;
	} else if (!strcasecmp(value, "flash")) {
		mwilamp = SKINNY_LAMP_FLASH;
	} else if (!strcasecmp(value, "blink")) {
		mwilamp = SKINNY_LAMP_BLINK;
	} else {
		pbx_log(LOG_WARNING, "Invalid mwilamp value, should be one of 'off', 'on', 'wink', 'flash' or 'blink'\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	if (*(sccp_lampMode_t *) dest != mwilamp) {
		*(sccp_lampMode_t *) dest = mwilamp;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Mailbox Value
 */
sccp_value_changed_t sccp_config_parse_mailbox(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_mailbox_t *mailbox = NULL;
	char *context, *mbox = NULL;

	SCCP_LIST_HEAD(, sccp_mailbox_t) * mailboxList = dest;

	mbox = context = sccp_strdupa(value);
	boolean_t mailbox_exists = FALSE;

	strsep(&context, "@");

	// Check mailboxes list
	SCCP_LIST_TRAVERSE(mailboxList, mailbox, list) {
		if (sccp_strequals(mailbox->mailbox, mbox)) {
			mailbox_exists = TRUE;
		}
	}
	if ((!mailbox_exists) && (!sccp_strlen_zero(mbox))) {
		// Add mailbox entry
		mailbox = sccp_calloc(1, sizeof(*mailbox));
		if (NULL != mailbox) {
			mailbox->mailbox = sccp_strdup(mbox);
			mailbox->context = sccp_strdup(context);

			SCCP_LIST_INSERT_TAIL(mailboxList, mailbox, list);
		}
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Tos Value
 */
sccp_value_changed_t sccp_config_parse_tos(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	unsigned int tos;

	if (!pbx_str2tos(value, &tos)) {
		/* value is tos */
	} else if (sscanf(value, "%i", &tos) == 1) {
		tos = tos & 0xff;
	} else if (!strcasecmp(value, "lowdelay")) {
	 	tos = IPTOS_LOWDELAY;
	} else if (!strcasecmp(value, "throughput")) {
		tos = IPTOS_THROUGHPUT;
	} else if (!strcasecmp(value, "reliability")) {
		tos = IPTOS_RELIABILITY;

#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
	} else if (!strcasecmp(value, "mincost")) {
		tos = IPTOS_MINCOST;
#endif
	} else if (!strcasecmp(value, "none")) {
		tos = 0;
	} else {
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
#else
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
#endif
		tos = 0x68 & 0xff;
	}

	if ((*(unsigned int *)dest) != tos) {
		*(unsigned int *)dest = tos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Cos Value
 */
sccp_value_changed_t sccp_config_parse_cos(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	unsigned int cos;

	if (sscanf(value, "%d", &cos) == 1) {
		if (cos > 7) {
			pbx_log(LOG_WARNING, "Invalid cos %d value, refer to QoS documentation\n", cos);
			return SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	}

	if ((*(unsigned int *)dest) != cos) {
		*(unsigned int *)dest = cos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for AmaFlags Value
 */
sccp_value_changed_t sccp_config_parse_amaflags(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int amaflags;

	amaflags = pbx_cdr_amaflags2int(value);

	if (amaflags < 0) {
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if ((*(int *)dest) != amaflags) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			*(int *)dest = amaflags;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Secundairy Dialtone Digits
 */
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *str = (char *)dest;
	
	if (strlen(value) <= 9 ) {
		if (strcasecmp(str, value)) {
			sccp_copy_string(str, value, 9);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
	} else {
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	
	return changed;
}

/*!
 * \brief Config Converter/Parser for Setvar Value
 */
sccp_value_changed_t sccp_config_parse_variables(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_CHANGED;

	PBX_VARIABLE_TYPE *newvar = NULL;
	PBX_VARIABLE_TYPE *prevVar = *(PBX_VARIABLE_TYPE **)dest;

	newvar = sccp_create_variable(value);
	if (newvar) {
		newvar->next = prevVar;
		*(PBX_VARIABLE_TYPE **)dest = newvar;
	} else {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for Callgroup/Pickupgroup Values
 *
 * \todo check changes to make the function more generic
 */
sccp_value_changed_t sccp_config_parse_group(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	char *piece;
	char *c;
	int start=0, finish=0, x;
	sccp_group_t group = 0;

	if (!sccp_strlen_zero(value)){
		c = sccp_strdupa(value);
		
		while ((piece = strsep(&c, ","))) {
			if (sscanf(piece, "%30d-%30d", &start, &finish) == 2) {
				/* Range */
			} else if (sscanf(piece, "%30d", &start)) {
				/* Just one */
				finish = start;
			} else {
				ast_log(LOG_ERROR, "Syntax error parsing group configuration '%s' at '%s'. Ignoring.\n", value, piece);
				continue;
			}
			for (x = start; x <= finish; x++) {
				if ((x > 63) || (x < 0)) {
					ast_log(LOG_WARNING, "Ignoring invalid group %d (maximum group is 63)\n", x);
				} else{
					group |= ((ast_group_t) 1 << x);
				}
			}
		}
	}
	
	if( (*(sccp_group_t *) dest) != group){
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		
		*(sccp_group_t *) dest = group;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Context
 */
sccp_value_changed_t sccp_config_parse_context(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *str = (char *)dest;
	if (strcasecmp(str, value) ) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(dest, value, size);
                if (!pbx_context_find((const char *)dest)) {
                        ast_log(LOG_WARNING,"The context '%s' you specified might not be available in the dialplan. Please check the sccp.conf\n", (char *)dest);
                }
	}else{
	        changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Context
 */
sccp_value_changed_t sccp_config_parse_hotline_context(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_hotline_t *hotline = *(sccp_hotline_t **)dest;

	if (strcasecmp(hotline->line->context, value) ) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(hotline->line->context, value, size);
	}else{
	        changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}


/*!
 * \brief Config Converter/Parser for Hotline Extension
 */
sccp_value_changed_t sccp_config_parse_hotline_exten(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_hotline_t *hotline = *(sccp_hotline_t **)dest;

	if (strcasecmp(hotline->exten, value) ) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(hotline->exten, value, size);
		if (hotline->line) {
			sccp_copy_string(hotline->line->adhocNumber, value, sizeof(hotline));
		}
	}else{
	        changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	}
	return changed;
}
/*!
 * \brief Config Converter/Parser for DND Values
 * \note 0/off is allow and 1/on is reject
 */
sccp_value_changed_t sccp_config_parse_dnd(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	uint8_t dndmode;

	if (!strcasecmp(value, "reject")) {
		dndmode = SCCP_DNDMODE_REJECT;
	} else if (!strcasecmp(value, "silent")) {
		dndmode = SCCP_DNDMODE_SILENT;
	} else if (!strcasecmp(value, "user")) {
		dndmode = SCCP_DNDMODE_USERDEFINED;
	} else if (!strcasecmp(value, "")){
		dndmode = SCCP_DNDMODE_OFF;
	} else {
		dndmode = sccp_true(value);
	}

	if ((*(uint8_t *)dest) != dndmode) {
		(*(uint8_t *)dest) = dndmode;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for Jitter Buffer Flags
 */
static sccp_value_changed_t sccp_config_parse_jbflags(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment, const unsigned int flag)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	struct ast_jb_conf jb = *(struct ast_jb_conf *)dest;
	
//        ast_log(LOG_WARNING,"Checking JITTERBUFFER: %d to %s\n", flag, value);
        if (pbx_test_flag(&jb, flag) != (unsigned)ast_true(value)) {
//		 ast_log(LOG_WARNING,"Setting JITTERBUFFER: %d to %s\n", flag, value);
//		 pbx_set2_flag(&jb, ast_true(value), flag);
		 pbx_set2_flag(&GLOB(global_jbconf), ast_true(value), flag);
		 changed = SCCP_CONFIG_CHANGE_CHANGED;
        }
	return changed;
}
sccp_value_changed_t sccp_config_parse_jbflags_enable(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment){
	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_ENABLED);	
}
sccp_value_changed_t sccp_config_parse_jbflags_force(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment){
	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_FORCED);	
}
sccp_value_changed_t sccp_config_parse_jbflags_log(void *dest, const size_t size, const char *value, const sccp_config_segment_t segment){
	return sccp_config_parse_jbflags(dest, size, value, segment, AST_JB_LOG);	
}

/* end dyn config */

/*!
 * \brief add a Button to a device
 * \param buttonconfig_head pointer to the device->buttonconfig list
 * \param index		button index
 * \param type          type of button
 * \param name          name
 * \param options       options
 * \param args          args
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->buttonconfig
 * 	   - globals
 *
 * \todo Build a check to see if the button has changed
 */
sccp_value_changed_t sccp_config_addButton(void *buttonconfig_head, int index, button_type_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;
//	boolean_t is_new = FALSE;
	int highest_index = 0;
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
//	sccp_configurationchange_t changes = SCCP_CONFIG_NOUPDATENEEDED;

	SCCP_LIST_HEAD(, sccp_buttonconfig_t) * buttonconfigList = buttonconfig_head;

	sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "SCCP: Loading/Checking Button Config\n");
	SCCP_LIST_LOCK(buttonconfigList);
	SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
		// check if the button is to be deleted to see if we need to replace it
		if (index==0 && config->pendingDelete && config->type == type) {
			if (config->type == EMPTY || !strcmp(config->label, name)) {
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Found Existing button at %d (Being Replaced)\n", config->index);
				index = config->index;
				break;
			}
		}
		highest_index = config->index;
	}

	if (index < 0) {
		index = highest_index + 1;
		config = NULL;
	}

	/* If a config at this position is not found, recreate one. */
	if (!config || config->index != index) {
		config = sccp_calloc(1, sizeof(*config));
		if (!config) {
			pbx_log(LOG_WARNING, "SCCP: sccp_config_addButton, memory allocation failed (calloc) failed\n");
			return SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}

		config->index = index;
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "New %s Button %s at : %d:%d\n", sccp_buttontype2str(type), name, index, config->index);
		SCCP_LIST_INSERT_TAIL(buttonconfigList, config, list);
//		is_new = TRUE;
	} else {
		config->pendingDelete = 0;
		config->pendingUpdate = 1;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	SCCP_LIST_UNLOCK(buttonconfigList);

        if ( type != EMPTY && (sccp_strlen_zero(name) || (type != LINE && !options))) {
                sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Faulty Button Configuration found at index: %d, type: %s, name: %s\n", config->index, sccp_buttontype2str(type), name);
		type = EMPTY;
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	switch (type) {
	case LINE:
	{
		struct composedId composedLineRegistrationId;
		memset(&composedLineRegistrationId, 0, sizeof(struct composedId));
		composedLineRegistrationId = sccp_parseComposedId(name, 80);

		if (	LINE==config->type && 
			sccp_strequals(config->label, name) &&
			sccp_strequals(config->button.line.name, composedLineRegistrationId.mainId) &&
			sccp_strcaseequals(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number) &&	
			sccp_strequals(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name) &&	
			sccp_strequals(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux)
			) {
			if (options && sccp_strequals(config->button.line.options, options)) {
				changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				break;
			} else {
				changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				break;
			}
		}
		config->type = LINE;

		sccp_copy_string(config->label, name, sizeof(config->label));
		sccp_copy_string(config->button.line.name, composedLineRegistrationId.mainId, sizeof(config->button.line.name));
		sccp_copy_string(config->button.line.subscriptionId.number, composedLineRegistrationId.subscriptionId.number, sizeof(config->button.line.subscriptionId.number));
		sccp_copy_string(config->button.line.subscriptionId.name, composedLineRegistrationId.subscriptionId.name, sizeof(config->button.line.subscriptionId.name));
		sccp_copy_string(config->button.line.subscriptionId.aux, composedLineRegistrationId.subscriptionId.aux, sizeof(config->button.line.subscriptionId.aux));
		if (options) {	
			sccp_copy_string(config->button.line.options, options, sizeof(config->button.line.options));
		}
		break;
	}
	case SPEEDDIAL:
		/* \todo check if values change */
		if (	SPEEDDIAL==config->type && 
			sccp_strequals(config->label, name) &&
			sccp_strequals(config->button.speeddial.ext, options)
			) {
			if (args && sccp_strequals(config->button.speeddial.hint, args)) {
				changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				break;
			} else {
				changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				break;
			}
		}
		config->type = SPEEDDIAL;

		sccp_copy_string(config->label, name, sizeof(config->label));
		sccp_copy_string(config->button.speeddial.ext, options, sizeof(config->button.speeddial.ext));
		if (args) {
			sccp_copy_string(config->button.speeddial.hint, args, sizeof(config->button.speeddial.hint));
		}
		break;
	case SERVICE:
		if (	SERVICE==config->type && 
			sccp_strequals(config->label, name) &&
			sccp_strequals(config->button.service.url, options)
			) {
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
			break;
		}
		/* \todo check if values change */
		config->type = SERVICE;

		sccp_copy_string(config->label, name, sizeof(config->label));
		sccp_copy_string(config->button.service.url, options, sizeof(config->button.service.url));
		break;
	case FEATURE:
		if (	FEATURE==config->type && 
			sccp_strequals(config->label, name) &&
			config->button.feature.id == sccp_featureStr2featureID(options)
			) {
			if (args && sccp_strequals(config->button.feature.options, args)) {
				changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				break;
			} else {
			}
		}
		/* \todo check if values change */
		config->type = FEATURE;
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_FEATURE | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "featureID: %s\n", options);

		sccp_copy_string(config->label, name, sizeof(config->label));
		config->button.feature.id = sccp_featureStr2featureID(options);

		if (args) {
			sccp_copy_string(config->button.feature.options, args, sizeof(config->button.feature.options));
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Arguments present on feature button: %d\n", config->instance);
		}

		sccp_log((DEBUGCAT_FEATURE | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "Configured feature button with featureID: %s args: %s\n", options, args);

		break;
	case EMPTY:
		if (	EMPTY==config->type ) {
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
			break;
		}
		/* \todo check if values change */
		config->type = EMPTY;
		break;
	}
	return changed;
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
 * \lock
 * 	- line
 * 	  - see sccp_line_changed()
 */
sccp_line_t *sccp_config_buildLine(sccp_line_t *l, PBX_VARIABLE_TYPE *v, const char *lineName, boolean_t isRealtime)
{
	sccp_configurationchange_t res = sccp_config_applyLineConfiguration(l, v);
#ifdef CS_SCCP_REALTIME
	l->realtime = isRealtime;
#endif
#ifdef CS_DYNAMIC_CONFIG
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET && l && l->pendingDelete) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: major changes for line '%s' detected, device reset required -> pendingUpdate=1\n", l->id);
		l->pendingUpdate = 1;
	} else {
	        l->pendingUpdate = 0;
        }
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Removing pendingDelete\n", l->name);
	l->pendingDelete = 0;
#endif	/* CS_DYNAMIC_CONFIG */
	return l;
}

/*!
 * \brief Build Device
 * \param d SCCP Device
 * \param v Asterisk Variable
 * \param deviceName Name of device as char
 * \param isRealtime is Realtime as Boolean
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_changed()
 */
sccp_device_t *sccp_config_buildDevice(sccp_device_t *d, PBX_VARIABLE_TYPE *v, const char *deviceName, boolean_t isRealtime)
{
	sccp_configurationchange_t res = sccp_config_applyDeviceConfiguration(d, v);
#ifdef CS_SCCP_REALTIME
	d->realtime = isRealtime;
#endif
#ifdef CS_DYNAMIC_CONFIG
//	if (res == SCCP_CONFIG_NEEDDEVICERESET && d && d->pendingDelete) {
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET && d) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s: major changes for device detected, device reset required -> pendingUpdate=1\n", d->id);
		d->pendingUpdate = 1;
	} else {
	        d->pendingUpdate = 0;
        }
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Removing pendingDelete\n", d->id);
	d->pendingDelete = 0;
#endif	/* CS_DYNAMIC_CONFIG */

	return d;
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
 * \lock
 * 	- line->mailboxes
 */
sccp_configurationchange_t sccp_config_applyGlobalConfiguration(PBX_VARIABLE_TYPE *v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	uint8_t alreadySetEntries[ARRAY_LEN(sccpGlobalConfigOptions)];
	uint8_t i=0;
	
	memset(alreadySetEntries, 0, sizeof(alreadySetEntries));

	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(sccp_globals, v->name, v->value, v->lineno, SCCP_CONFIG_GLOBAL_SEGMENT);
		// mark entries as already set
		for (i=0;i<ARRAY_LEN(sccpGlobalConfigOptions);i++) {
			if (!strcasecmp(sccpGlobalConfigOptions[i].name,v->name)) {
				alreadySetEntries[i]=1;
			}
		}
	}
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Update Needed (%d)\n", res);
	
	sccp_config_set_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT, alreadySetEntries, ARRAY_LEN(sccpGlobalConfigOptions));	
	
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
        if(GLOB(ha)) {
		sccp_free_ha(GLOB(ha));
		GLOB(ha) = NULL;
        }
        if(GLOB(localaddr)){
		sccp_free_ha(GLOB(localaddr));
		GLOB(localaddr) = NULL;
        }

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

	sccp_configurationchange_t res = sccp_config_applyGlobalConfiguration(v);
//	sccp_config_set_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT);
	
#ifdef CS_DYNAMIC_CONFIG
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "SCCP: major changes detected in globals, reset required -> pendingUpdate=1\n");
		GLOB(pendingUpdate = 1);
	} else {
	        GLOB(pendingUpdate = 0);
	}
#endif

	/* setup bindaddress */
	if (!ntohs(GLOB(bindaddr.sin_port))) {
		GLOB(bindaddr.sin_port) = ntohs(DEFAULT_SCCP_PORT);
	}
	GLOB(bindaddr.sin_family) = AF_INET;

	/* setup hostname -> externip */
	/* \todo change using singular h_addr to h_addr_list (name may resolve to multiple ip-addresses */
	struct ast_hostent ahp;
	struct hostent *hp;

	if (!sccp_strlen_zero(GLOB(externhost))) {
		if (!(hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
			pbx_log(LOG_WARNING, "Invalid address resolution for externhost keyword: %s\n", GLOB(externhost));
		} else {
			memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
			time(&GLOB(externexpire));
		}
	}

	/* setup regcontext */
	char newcontexts[SCCP_MAX_CONTEXT];
	char oldcontexts[SCCP_MAX_CONTEXT];
	char *stringp, *context, *oldregcontext;

	sccp_copy_string(newcontexts, GLOB(regcontext), sizeof(newcontexts));
	stringp = newcontexts;

	sccp_copy_string(oldcontexts, GLOB(used_context), sizeof(oldcontexts));	// Initialize copy of current regcontext for later use in removing stale contexts
	oldregcontext = oldcontexts;

	cleanup_stale_contexts(stringp, oldregcontext);				// Let's remove any contexts that are no longer defined in regcontext

	while ((context = strsep(&stringp, "&"))) {				// Create contexts if they don't exist already
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
		stalecontext = '\0';
		sccp_copy_string(newlist, new, sizeof(newlist));
		stringp = newlist;
		while ((newcontext = strsep(&stringp, "&"))) {
			if (sccp_strequals(newcontext, oldcontext)) {
				/* This is not the context you're looking for */
				stalecontext = '\0';
				break;
			} else if (!sccp_strequals(newcontext, oldcontext)) {
				stalecontext = oldcontext;
			}

		}
		if (stalecontext)
			ast_context_destroy(ast_context_find(stalecontext), "SCCP");
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
 * \lock
 * 	- lines
 * 	  - see sccp_config_applyLineConfiguration()
 */
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype)
{
//      struct ast_config *cfg = NULL;

	char *cat = NULL;
	PBX_VARIABLE_TYPE *v = NULL;
	uint8_t device_count = 0;
	uint8_t line_count = 0;
	
	sccp_line_t *l;
	sccp_device_t *d;
	boolean_t is_new=FALSE;

	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Loading Devices and Lines from config\n");

#ifdef CS_DYNAMIC_CONFIG
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking Reading Type\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Pre Reload\n");
		sccp_device_pre_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Pre Reload\n");
		sccp_line_pre_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Pre Reload\n");
		sccp_softkey_pre_reload();
	}
#endif

	if (!GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}
	
	while ((cat = pbx_category_browse(GLOB(cfg), cat))) {

		const char *utype;

		if (!strcasecmp(cat, "general"))
			continue;

		utype = pbx_variable_retrieve(GLOB(cfg), cat, "type");

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
					d = sccp_device_create();
					sccp_copy_string(d->id, cat, sizeof(d->id));		/* set device name */
					sccp_device_addToGlobals(d);
					is_new = TRUE;
					device_count++;
				}else{
#ifdef CS_DYNAMIC_CONFIG
					if(d->pendingDelete){
						d->pendingDelete = 0;
					}
#endif
				}
//				sccp_config_applyDeviceConfiguration(d, v);		/** load configuration and set defaults */
				sccp_config_buildDevice(d, v, cat, FALSE);
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found device %d: %s\n", device_count, cat);
				/* load saved settings from ast db */
				sccp_config_restoreDeviceFeatureStatus(d);
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
			l = sccp_line_find_byname_wo(cat, FALSE);
			if (!l) {
				l = sccp_line_create();
				is_new = TRUE;
				sccp_copy_string(l->name, cat, sizeof(l->name));
			} else {
				is_new = FALSE;
			}
						
			sccp_config_buildLine(l, v, cat, FALSE);
			if (is_new)
				sccp_line_addToGlobals(l);
			
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found line %d: %s\n", line_count, cat);
		} else if (!strcasecmp(utype, "softkeyset")) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "read set %s\n", cat);
			v = ast_variable_browse(GLOB(cfg), cat);
			sccp_config_softKeySet(v, cat);
		}
	}

#ifdef CS_SCCP_REALTIME
	/* reload realtime lines */
	sccp_configurationchange_t res;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (l->realtime == TRUE && l != GLOB(hotline)->line) {
			sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "%s: reload realtime line\n", l->name);
			v = pbx_load_realtime(GLOB(realtimelinetable), "name", l->name, NULL);
#    ifdef CS_DYNAMIC_CONFIG
			/* we did not find this line, mark it for deletion */
			if (!v) {
				sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "%s: realtime line not found - set pendingDelete=1\n", l->name);
				l->pendingDelete = 1;
				continue;
			}
#    endif

			res = sccp_config_applyLineConfiguration(l, v);
			/* check if we did some changes that needs a device update */
#    ifdef CS_DYNAMIC_CONFIG
			if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
				l->pendingUpdate = 1;
			} else {
			        l->pendingUpdate = 0;
		        } 
#    endif
			pbx_variables_destroy(v);
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	/* finished realtime line reload */
#endif

	if (GLOB(reload_in_progress) && GLOB(pendingUpdate)) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Global param changed needing restart ->  Restart all device\n");
		sccp_device_t *device;
		SCCP_RWLIST_WRLOCK(&GLOB(devices));
		SCCP_RWLIST_TRAVERSE(&GLOB(devices), device, list) {
			if (!device->pendingDelete && !device->pendingUpdate) {
				device->pendingUpdate=1;
			}
		}	
		SCCP_RWLIST_UNLOCK(&GLOB(devices));
	}
	GLOB(pendingUpdate)=0;


#ifdef CS_DYNAMIC_CONFIG
	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking Reading Type\n");
	if (readingtype == SCCP_CONFIG_READRELOAD) {
		/* IMPORTANT: The line_post_reload function may change the pendingUpdate field of
		 * devices, so it's really important to call it *before* calling device_post_real().
		 */
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Line Post Reload\n");
		sccp_line_post_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Device Post Reload\n");
		sccp_device_post_reload();
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Softkey Post Reload\n");
		sccp_softkey_post_reload();
	}
#endif
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
 * \lock
 * 	- line->mailboxes
 */
sccp_configurationchange_t sccp_config_applyLineConfiguration(sccp_line_t * l, PBX_VARIABLE_TYPE *v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	uint8_t alreadySetEntries[ARRAY_LEN(sccpLineConfigOptions)];
	uint8_t i=0;

	memset(alreadySetEntries, 0, sizeof(alreadySetEntries));
#ifdef CS_DYNAMIC_CONFIG
	if (l->pendingDelete) {
		/* removing variables */
		if (l->variables) {
			pbx_variables_destroy(l->variables);
			l->variables = NULL;
		}
	}
#endif
	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(l, v->name, v->value, v->lineno, SCCP_CONFIG_LINE_SEGMENT);
		// mark entries as already set
//		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Update Needed (%d)\n", l->name, res);
		for (i=0;i<ARRAY_LEN(sccpLineConfigOptions);i++) {
			if (!strcasecmp(sccpLineConfigOptions[i].name,v->name)) {
				alreadySetEntries[i]=1;
			}
		}

	}
	sccp_config_set_defaults(l, SCCP_CONFIG_LINE_SEGMENT, alreadySetEntries, ARRAY_LEN(alreadySetEntries));
	
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
 * \lock
 * 	- device->addons
 * 	- device->permithosts
 */
sccp_configurationchange_t sccp_config_applyDeviceConfiguration(sccp_device_t * d, PBX_VARIABLE_TYPE * v)
{
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	char *prev_var_name = NULL;
	uint8_t alreadySetEntries[ARRAY_LEN(sccpDeviceConfigOptions)];
	uint8_t i=0;

	memset(alreadySetEntries, 0, sizeof(alreadySetEntries));
#ifdef CS_DYNAMIC_CONFIG
	if (d->pendingDelete) {
		// remove all addons before adding them again (to find differences later on in sccp_device_change)
		sccp_addon_t *addon;

		SCCP_LIST_LOCK(&d->addons);
		while ((addon = SCCP_LIST_REMOVE_HEAD(&d->addons, list))) {
			sccp_free(addon);
			addon = NULL;
		}
		SCCP_LIST_UNLOCK(&d->addons);
		SCCP_LIST_HEAD_DESTROY(&d->addons);
		SCCP_LIST_HEAD_INIT(&d->addons);

		/* removing variables */
		if (d->variables) {
			pbx_variables_destroy(d->variables);
			d->variables = NULL;
		}

		/* removing permithosts */
		sccp_hostname_t *permithost;

		SCCP_LIST_LOCK(&d->permithosts);
		while ((permithost = SCCP_LIST_REMOVE_HEAD(&d->permithosts, list))) {
			sccp_free(permithost);
			permithost = NULL;
		}
		SCCP_LIST_UNLOCK(&d->permithosts);
		SCCP_LIST_HEAD_DESTROY(&d->permithosts);
		SCCP_LIST_HEAD_INIT(&d->permithosts);

		sccp_free_ha(d->ha);
		d->ha = NULL;
	}
#endif
	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(d, v->name, v->value, v->lineno, SCCP_CONFIG_DEVICE_SEGMENT);
//		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Update Needed (%d)\n", d->id, res);

		// mark entries as already set
		for (i=0;i<ARRAY_LEN(sccpDeviceConfigOptions);i++) {
			if (!strcasecmp(sccpDeviceConfigOptions[i].name,v->name)) {
				alreadySetEntries[i]=1;
			}
		}
	}
	if(prev_var_name)
		sccp_free(prev_var_name);

	sccp_config_set_defaults(d, SCCP_CONFIG_DEVICE_SEGMENT, alreadySetEntries, ARRAY_LEN(alreadySetEntries));

#ifdef CS_DEVSTATE_FEATURE
	sccp_device_lock(d);
	sccp_buttonconfig_t *config = NULL;
	sccp_devstate_specifier_t *dspec;
	
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == FEATURE) {
			/* Check for the presence of a devicestate specifier and register in device list. */
			if ((SCCP_FEATURE_DEVSTATE == config->button.feature.id) && (strncmp("", config->button.feature.options, 254))) {
				dspec = sccp_calloc(1, sizeof(sccp_devstate_specifier_t));
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Recognized devstate feature button: %d\n", config->instance);
				SCCP_LIST_LOCK(&d->devstateSpecifiers);
				sccp_copy_string(dspec->specifier, config->button.feature.options, sizeof(config->button.feature.options));
				SCCP_LIST_INSERT_TAIL(&d->devstateSpecifiers, dspec, list);
				SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
			}
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	sccp_device_unlock(d);
#endif

	return res;
}

/*!
 * \brief Find the Correct Config File
 * \return Asterisk Config Object as ast_config
 */
struct ast_config *sccp_config_getConfig()
{
	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS & CONFIG_FLAG_FILEUNCHANGED };

	if (sccp_strlen_zero(GLOB(config_file_name))) {
		GLOB(config_file_name) = "sccp.conf";
	}
	if (GLOB(cfg) != NULL) {
		pbx_config_destroy(GLOB(cfg));
	}

	GLOB(cfg) = pbx_config_load(GLOB(config_file_name), "chan_sccp", config_flags);

	if (CONFIG_STATUS_FILEMISSING == GLOB(cfg)) {
		pbx_log(LOG_WARNING, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
	} else if (CONFIG_STATUS_FILEUNCHANGED == GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "Config file '%s' has not changed, aborting reload.\n", GLOB(config_file_name));
	} else if (CONFIG_STATUS_FILEINVALID == GLOB(cfg)) {
		pbx_log(LOG_WARNING, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
	} else if (GLOB(cfg) && ast_variable_browse(GLOB(cfg), "devices")) {	/* Warn user when old entries exist in sccp.conf */
		pbx_log(LOG_WARNING, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
		pbx_config_destroy(GLOB(cfg));
		return CONFIG_STATUS_FILEOLD;
	} else if (!ast_variable_browse(GLOB(cfg), "general")) {
		pbx_log(LOG_WARNING, "Missing [general] section, SCCP disabled\n");
		pbx_config_destroy(GLOB(cfg));
		return CONFIG_STATUS_FILE_NOT_SCCP;
	}
	return GLOB(cfg);
}

/*!
 * \brief Read a SoftKey configuration context
 * \param variable list of configuration variables
 * \param name name of this configuration (context)
 * 
 * \callgraph
 * \callergraph
 *
 * \lock
 *	- softKeySetConfig
 */
void sccp_config_softKeySet(PBX_VARIABLE_TYPE *variable, const char *name)
{
	int keySetSize;
	sccp_softKeySetConfiguration_t *softKeySetConfiguration = NULL;
	int keyMode = -1;
	unsigned int i = 0;

	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "start reading softkeyset: %s\n", name);

	
	SCCP_LIST_LOCK(&softKeySetConfig);
	SCCP_LIST_TRAVERSE(&softKeySetConfig, softKeySetConfiguration, list) {
		if(!strcasecmp(softKeySetConfiguration->name, name))
			break;
	}
	SCCP_LIST_UNLOCK(&softKeySetConfig);
	
	if(!softKeySetConfiguration){
		softKeySetConfiguration = sccp_calloc(1, sizeof(sccp_softKeySetConfiguration_t));
		memset(softKeySetConfiguration, 0, sizeof(sccp_softKeySetConfiguration_t));

		sccp_copy_string(softKeySetConfiguration->name, name, sizeof(softKeySetConfiguration->name));
		softKeySetConfiguration->numberOfSoftKeySets = 0;
	
		/* add new softkexSet to list */
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_INSERT_HEAD(&softKeySetConfig, softKeySetConfiguration, list);
		SCCP_LIST_UNLOCK(&softKeySetConfig);
	}

	while (variable) {
		keyMode = -1;
		sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeyset: %s \n", variable->name);
		if (!strcasecmp(variable->name, "type")) {

		} else if (!strcasecmp(variable->name, "onhook")) {
			keyMode = KEYMODE_ONHOOK;
		} else if (!strcasecmp(variable->name, "connected")) {
			keyMode = KEYMODE_CONNECTED;
		} else if (!strcasecmp(variable->name, "onhold")) {
			keyMode = KEYMODE_ONHOLD;
		} else if (!strcasecmp(variable->name, "ringin")) {
			keyMode = KEYMODE_RINGIN;
		} else if (!strcasecmp(variable->name, "offhook")) {
			keyMode = KEYMODE_OFFHOOK;
		} else if (!strcasecmp(variable->name, "conntrans")) {
			keyMode = KEYMODE_CONNTRANS;
		} else if (!strcasecmp(variable->name, "digitsfoll")) {
			keyMode = KEYMODE_DIGITSFOLL;
		} else if (!strcasecmp(variable->name, "connconf")) {
			keyMode = KEYMODE_CONNCONF;
		} else if (!strcasecmp(variable->name, "ringout")) {
			keyMode = KEYMODE_RINGOUT;
		} else if (!strcasecmp(variable->name, "offhookfeat")) {
			keyMode = KEYMODE_OFFHOOKFEAT;
		} else if (!strcasecmp(variable->name, "onhint")) {
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
				if(softKeySetConfiguration->modes[i].ptr)
					sccp_free(softKeySetConfiguration->modes[i].ptr);
			  
				uint8_t *softkeyset = sccp_calloc(StationMaxSoftKeySetDefinition, sizeof(uint8_t));

				keySetSize = sccp_config_readSoftSet(softkeyset, variable->value);

				if (keySetSize > 0) {			  
					softKeySetConfiguration->modes[i].id = keyMode;
					softKeySetConfiguration->modes[i].ptr = softkeyset;
					softKeySetConfiguration->modes[i].count = keySetSize;
				} else {
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

	if (!data)
		return 0;

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
		if (!strcasecmp(softKeyTemplate[i].configVar, key)) {
			return softKeyTemplate[i].softkey;
		}
	}
	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeybutton: %s not defined", key);
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
 * \lock
 * 	- device->devstateSpecifiers
 */
void sccp_config_restoreDeviceFeatureStatus(sccp_device_t * device)
{
	if (!device)
		return;

#ifdef CS_DEVSTATE_FEATURE
	char buf[256] = "";
	sccp_devstate_specifier_t *specifier;
#endif

#ifndef ASTDB_FAMILY_KEY_LEN
#    define ASTDB_FAMILY_KEY_LEN 256
#endif

#ifndef ASTDB_RESULT_LEN
#    define ASTDB_RESULT_LEN 256
#endif

	char buffer[ASTDB_RESULT_LEN];
	char timebuffer[ASTDB_RESULT_LEN];
	int timeout;
	char family[ASTDB_FAMILY_KEY_LEN];

	sprintf(family, "SCCP/%s", device->id);

	/* dndFeature */
	if (PBX(feature_getFromDatabase)(family, "dnd", buffer, sizeof(buffer))) {
		if (!strcasecmp(buffer, "silent"))
			device->dndFeature.status = SCCP_DNDMODE_SILENT;
		else
			device->dndFeature.status = SCCP_DNDMODE_REJECT;
	} else {
		device->dndFeature.status = SCCP_DNDMODE_OFF;
	}

// 	/* monitorFeature */
// 	if (PBX(feature_getFromDatabase)(family, "monitor", buffer, sizeof(buffer))) {
// 		device->monitorFeature.status |= SCCP_FEATURE_MONITOR_STATE_REQUESTED;
// 	} else {
// 		device->monitorFeature.status = 0;
// 	}

	/* privacyFeature */
	if (PBX(feature_getFromDatabase)(family, "privacy", buffer, sizeof(buffer))) {
		sscanf(buffer, "%u", (unsigned int *)&device->privacyFeature.status);
	} else {
		device->privacyFeature.status = 0;
	}

	/* Message */
	if (PBX(feature_getFromDatabase) && PBX(feature_getFromDatabase) ("SCCP/message", "text", buffer, sizeof(buffer))) {
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

	/* lastDialedNumber */
	char lastNumber[SCCP_MAX_EXTENSION] = "";

	if (PBX(feature_getFromDatabase)(family, "lastDialedNumber", lastNumber, sizeof(lastNumber))) {
		sccp_copy_string(device->lastNumber, lastNumber, sizeof(device->lastNumber));
	}

	/* initialize so called priority feature */
	device->priFeature.status = 0x010101;
	device->priFeature.initialized = 0;

#ifdef CS_DEVSTATE_FEATURE
	/* Read and initialize custom devicestate entries */
	SCCP_LIST_LOCK(&device->devstateSpecifiers);
	SCCP_LIST_TRAVERSE(&device->devstateSpecifiers, specifier, list) {
		/* Check if there is already a devicestate entry */
		if (PBX(feature_getFromDatabase)(devstate_db_family, specifier->specifier, buf, sizeof(buf))) {
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "%s: Found Existing Custom Devicestate Entry: %s, state: %s\n", device->id, specifier->specifier, buf);
		} else {
			/* If not present, add a new devicestate entry. Default: NOT_INUSE */
			PBX(feature_addToDatabase)(devstate_db_family, specifier->specifier, "NOT_INUSE");
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_1 "%s: Initialized Devicestate Entry: %s\n", device->id, specifier->specifier);
		}
		/* Register as generic hint watcher */
		/*! \todo Add some filtering in order to reduce number of unnecessarily triggered events.
		   Have to work out whether filtering with AST_EVENT_IE_DEVICE matches extension or hint device name. */
		snprintf(buf, 254, "Custom:%s", specifier->specifier);
		/* When registering for devstate events we wish to know if a single asterisk box has contributed
		   a change even in a rig of multiple asterisk with distributed devstate. This is to enable toggling
		   even then when otherwise the aggregate devicestate would obscure the change.
		   However, we need to force distributed devstate even on single asterisk boxes so to get the desired events. (-DD) */
#ifdef CS_NEW_DEVICESTATE
		ast_enable_distributed_devstate();
		specifier->sub = pbx_event_subscribe(AST_EVENT_DEVICE_STATE, sccp_devstateFeatureState_cb, "devstate subscription", device, AST_EVENT_IE_DEVICE, AST_EVENT_IE_PLTYPE_STR, strdup(buf), AST_EVENT_IE_END);
#endif		
	}
	SCCP_LIST_UNLOCK(&device->devstateSpecifiers);
#endif
}

int sccp_manager_config_metadata(struct mansession *s, const struct message *m) 
{
        const SCCPConfigSegment *sccpConfigSegment = NULL;
        const SCCPConfigOption 	*config=NULL;
        long unsigned int 	sccp_option;
        char 			idtext[256];
        int  			total = 0;
        int 			i;
	const char 		*id = astman_get_header(m, "ActionID");					
	const char 		*req_segment= astman_get_header(m, "Segment");
	const char 		*req_option = astman_get_header(m, "Option");
	char *description;
	char *description_part;

	if (strlen(req_segment) == 0) {		// return all segments
	        astman_send_listack(s, m, "List of segments will follow", "start");
        	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
               		astman_append(s, "Event: SegmentEntry\r\n");
        		astman_append(s, "Segment: %s\r\n\r\n", sccpConfigSegments[i].name);
			total++;
	        }
                astman_append(s, "Event: SegmentlistComplete\r\n\r\n");
	} else if (strlen(req_option) == 0) { 	// return all options for segment
	        astman_send_listack(s, m, "List of SegmentOptions will follow", "start");
        	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
	                if (!strcasecmp(sccpConfigSegments[i].name, req_segment)) {
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
	} else {				// return metadata for option in segmnet
	        astman_send_listack(s, m, "List of Option Attributes will follow", "start");
        	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
	                if (!strcasecmp(sccpConfigSegments[i].name, req_segment)) {
                                sccpConfigSegment = &sccpConfigSegments[i];
                                config = sccp_find_config(sccpConfigSegments[i].segment, req_option);
                                if (config) {
                                        if ((config->flags & SCCP_CONFIG_FLAG_IGNORE) != SCCP_CONFIG_FLAG_IGNORE) {
                                                const char *possible_values = NULL;
                                                astman_append(s, "Event: AttributeEntry\r\n");
                                                astman_append(s, "Segment: %s\r\n", sccpConfigSegment->name);
                                                astman_append(s, "Option: %s\r\n", config->name);
                                                astman_append(s, "Size: %d\r\n", (int)config->size);
                                                astman_append(s, "Required: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) ? "true" : "false"); 
                                                astman_append(s, "Deprecated: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED) ? "true" : "false");
                                                astman_append(s, "Obsolete: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE) ? "true" : "false"); 
                                                astman_append(s, "Multientry: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "true" : "false");
                                                astman_append(s, "RestartRequiredOnUpdate: %s\r\n", ((config->change & SCCP_CONFIG_NEEDDEVICERESET) == SCCP_CONFIG_NEEDDEVICERESET) ? "true" : "false");

                                                switch (config->type) {
                                                        case SCCP_CONFIG_DATATYPE_BOOLEAN:
                                                                astman_append(s, "Type: BOOLEAN\r\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_INT:
                                                                astman_append(s, "Type: INT\r\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_UINT:
                                                                astman_append(s, "Type: UNSIGNED INT\r\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_STRINGPTR:
                                                        case SCCP_CONFIG_DATATYPE_STRING:
                                                                astman_append(s, "Type: STRING\r\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_PARSER:
                                                                astman_append(s, "Type: PARSER\r\n");
                                                                astman_append(s, "possible_values: %s\r\n", "");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_CHAR:
                                                                astman_append(s, "Type: CHAR\r\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_ENUM2INT:
                                                                possible_values = config->enumkeys();
                                                                astman_append(s, "Type: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "SET" : "ENUM");; 
                                                                astman_append(s, "PossibleValues: %s\r\n", possible_values);
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_ENUM2STR:
                                                                possible_values = config->enumkeys();
                                                                astman_append(s, "Type: %s\r\n", ((config->flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "SET" : "ENUM");; 
        //                                                        astman_append(s, "PossibleValues: %s\r\n", possible_values);
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_CSV2STR:
                                                                possible_values = config->enumkeys();
                                                                astman_append(s, "Type: SET\r\n");
        //                                                        astman_append(s, "PossibleValues: %s\r\n", possible_values);
                                                                break;
                                                }
                                                if (config->defaultValue && !strlen(config->defaultValue)==0) {
                                                        astman_append(s, "DefaultValue: %s\r\n", config->defaultValue);
                                                }
                                                if (strlen(config->description)!=0) {
                                                description=malloc(sizeof(char) * strlen(config->description));	
                                                        description=strdup(config->description);	
                                                        astman_append(s, "Description: ");
                                                        while ((description_part=strsep(&description, "\n"))) {
                                                                if (description_part && strlen(description_part)!=0) {
                                                                        astman_append(s, "%s", description_part);
                                                                }
                                                        }
                                                        astman_append(s, "%s%s\r\n", !sccp_strlen_zero(possible_values) ? "(POSSIBLE VALUES: " : "", possible_values);
                                                }
                                                astman_append(s, "\r\n");
                                                if (possible_values)
                                                        sccp_free((char *)possible_values);
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
	astman_append(s,							
		"EventList: Complete\r\n"					
		"ListItems: %d\r\n"						
		"%s"								
		"\r\n", total, idtext);  					
        return 0;
}

/*!
 * \brief Generate default sccp.conf file
 * \param filename Filename
 * \param configType Config Type:
 *  - 0 = templated + registered devices
 *  - 1 = required params only
 *  - 2 = all params
 * \todo Add functionality
 */
int sccp_config_generate(char *filename, int configType) 
{
        const SCCPConfigSegment *sccpConfigSegment = NULL;
        const SCCPConfigOption *config=NULL;
        long unsigned int sccp_option;
        long unsigned int segment;
        char *description;
        char *description_part;
        char name_and_value[100];
        int linelen=0;

	char fn[PATH_MAX];
	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, "sccp.conf.test");
        ast_log(LOG_NOTICE, "Creating new config file '%s'\n", fn);
	
        FILE *f;
        if (!(f = fopen(fn, "w"))) {
                ast_log(LOG_ERROR, "Error creating new config file \n");
                return 1;
        }

        char date[256]="";
        time_t t;
        time(&t);
        sccp_copy_string(date, ctime(&t), sizeof(date));
                                
        fprintf(f,";!\n");
        fprintf(f,";! Automatically generated configuration file\n");
        fprintf(f,";! Filename: %s (%s)\n", filename, fn);
        fprintf(f,";! Generator: sccp config generate\n");
        fprintf(f,";! Creation Date: %s", date);
        fprintf(f,";!\n");
        fprintf(f,"\n");

	for (segment=SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {			
		sccpConfigSegment = sccp_find_segment(segment);
		if (configType==0 && (segment==SCCP_CONFIG_DEVICE_SEGMENT || segment==SCCP_CONFIG_LINE_SEGMENT)) {
        		sccp_log(DEBUGCAT_CONFIG)(VERBOSE_PREFIX_2 "adding [%s] template section\n", sccpConfigSegment->name);
	        	fprintf(f, "\n;\n; %s section\n;\n[default_%s](!)\n", sccpConfigSegment->name, sccpConfigSegment->name);
                } else {
        		sccp_log(DEBUGCAT_CONFIG)(VERBOSE_PREFIX_2 "adding [%s] section\n", sccpConfigSegment->name);
	        	fprintf(f, "\n;\n; %s section\n;\n[%s]\n", sccpConfigSegment->name, sccpConfigSegment->name);
                }

		config = sccpConfigSegment->config;
		for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
			if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
				sccp_log(DEBUGCAT_CONFIG)(VERBOSE_PREFIX_2 "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);
				
				if (!sccp_strlen_zero(config[sccp_option].name)) {
					if (!sccp_strlen_zero(config[sccp_option].defaultValue)		// non empty
						|| (configType!=2 && ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != SCCP_CONFIG_FLAG_REQUIRED && sccp_strlen_zero(config[sccp_option].defaultValue))) // empty but required
					) {
						snprintf(name_and_value, sizeof(name_and_value),"%s = %s", config[sccp_option].name, sccp_strlen_zero(config[sccp_option].defaultValue) ? "\"\"" : config[sccp_option].defaultValue);
						linelen=(int)strlen(name_and_value);
						fprintf(f, "%s", name_and_value);
						if (!sccp_strlen_zero(config[sccp_option].description)) {
							description = sccp_strdupa(config[sccp_option].description);	
							while ((description_part=strsep(&description, "\n"))) {
								if (!sccp_strlen_zero(description_part)) {
									fprintf(f, "%*.s ; %s%s%s\n", 81-linelen, " ", (config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED ? "(REQUIRED) " : "", ((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "(MULTI-ENTRY)" : "", description_part);
									linelen=0;
								}
							}	
						} else {
							fprintf(f, "\n");
						}
					}
				} else {
					ast_log(LOG_ERROR, "Error creating new variable structure for %s='%s'\n", config[sccp_option].name, config[sccp_option].defaultValue);
					return 2;
				}
			}
		}
		sccp_log(DEBUGCAT_CONFIG)("\n");
	}
        fclose(f);

	return 0;
}
