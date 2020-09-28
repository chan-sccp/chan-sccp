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

/*** DOCUMENTATION
	<manager name="SCCPConfigMetaData" language="en_US">
		<synopsis>Retrieve config metadata in json format</synopsis>
		<syntax>
			<xi:include href="../core-en_US.xml" parse="xml"
				xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])"/>
			<parameter name="segment" required="false">
				<para>The name of the segment you are interest, leaving it empty will list the segments available.</para>
				<enumlist>
					<enum name="general"/>
					<enum name="device"/>
					<enum name="line"/>
					<enum name="softkey"/>
				</enumlist>
			</parameter>
			<parameter name="ResultFormat">
				<para/>
				<enumlist>
					<enum name="list">
						<para>Will return Output will SCCPConfigMetaData managerevent</para>
					</enum>
					<enum name="command">
						<para>Will return json formatted string presented in the 'DataType' field.</para>
					</enum>
				</enumlist>
			</parameter>
		</syntax>
		<description>
			<para>Fetch configuration metadata</para>
		</description>
		<see-also>
			<ref type="managerEvent">SCCPConfigMetaData</ref>
			<ref type="managerEvent">SCCPConfigMetaDataComplete</ref>
		</see-also>
		<responses>
			<list-elements>
				<managerEvent language="en_US" name="SCCPConfigMetaData">
					<managerEventInstance class="EVENT_FLAG_COMMAND">
						<synopsis>Detailed field information for the requested segment.</synopsis>
						<synopsis>One</synopsis>
						<syntax>
							<parameter name="Name">
								<para>Name of the field.</para>
							</parameter>
							<parameter name="Type">
								<para>Name of the field.</para>
							</parameter>
							<parameter name="Size">
								<para>Size of the field</para>
							</parameter>
							<parameter name="Flags">
								<para/>
								<enumlist>
									<enum name="BOOLEAN"/>
									<enum name="INT"/>
									<enum name="UNSIGNED INT"/>
									<enum name="STRINGPTR">
										<para>Null terminated string.</para>
									</enum>
									<enum name="STRING">
										<para>Fixed length string.</para>
									</enum>
									<enum name="PARSER">
										<para>Custom parser/generator.</para>
									</enum>
									<enum name="CHAR">
										<para>Single Character.</para>
									</enum>
									<enum name="ENUM">
										<para>When 'ENUM' is used the list entry will also contain the <replaceable>Possible Values</replaceable> field</para>
									</enum>
								</enumlist>
							</parameter>
							<parameter name="Possible Values" required="false">
								<para>Contains a string of possible values, enclosed in '[' &amp; ']' and separated by ','.</para>
								<para>Only included for entries that have type:ENUM.</para>
							</parameter>
							<parameter name="DefaultValue">
								<para>Default value of the field.</para>
							</parameter>
							<parameter name="Description">
								<para>A detailed user description of the field.</para>
							</parameter>
							<xi:include href="../core-en_US.xml" parse="xml"
								xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])"/>
						</syntax>
						<description>
							<para>Detailed field information for the requested segment.</para>
						</description>
						<see-also>
							<ref type="managerEvent">SCCPConfigMetaDataComplete</ref>
						</see-also>
					</managerEventInstance>
				</managerEvent>
			</list-elements>
			<managerEvent language="en_US" name="SCCPConfigMetaDataComplete">
				<managerEventInstance class="EVENT_FLAG_AGENT">
					<synopsis>Final response event in a series of events to the Agents AMI action.</synopsis>
					<syntax>
						<xi:include href="../core-en_US.xml" parse="xml"
							xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])"/>
					</syntax>
					<see-also>
						<ref type="manager">SCCPConfigMetaData</ref>
					</see-also>
				</managerEventInstance>
			</managerEvent>
		</responses>
	</manager>
***/
#include "config.h"
#include "common.h"
#include "sccp_config.h"
#include "sccp_device.h"
#include "sccp_featureButton.h"
#include "sccp_line.h"
#include "sccp_linedevice.h"
#include "sccp_mwi.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include "sccp_labels.h"
#include "revision.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/paths.h>
#if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H) && (defined(CS_DEVICESTATE) || defined(CS_CACHEABLE_DEVICESTATE))	// ast_event_subscribe
#  include <asterisk/event.h>
#endif

#ifdef HAVE_PBX_APP_H
#	include <asterisk/app.h>                                        // AST_DECLARE_APP_ARGS / AST_STANDARD_APP_ARGS
#endif

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
#define G_OBJ_REF(x) offsize(struct sccp_global_vars,x), offsetof(struct sccp_global_vars,x)
#define D_OBJ_REF(x) offsize(struct sccp_device,x), offsetof(struct sccp_device,x)
#define L_OBJ_REF(x) offsize(struct sccp_line,x), offsetof(struct sccp_line,x)
#define S_OBJ_REF(x) offsize(struct softKeySetConfiguration,x), offsetof(struct softKeySetConfiguration,x)
#define H_OBJ_REF(x) offsize(struct sccp_hotline,x), offsetof(struct sccp_hotline,x)
//#define BITMASK(b) (1 << ((b) % CHAR_BIT))
//#define BITSLOT(b) ((b) / CHAR_BIT)
//#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
//#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
//#define BITTOGGLE(a, b) ((a)[BITSLOT(b)] ^= BITMASK(b))
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
	const size_t size;							/*!< The offsize of the element in the structure where the option value is stored */
	const int offset;							/*!< The offset relative to the context structure where the option value is stored. */
	int type; 								/*!< bitfield of enum SCCPConfigOptionType */
	sccp_value_changed_t (*converter_f)(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment); /*!< Conversion function */
	sccp_enum_str2intval_t str2intval;
        sccp_enum_all_entries_t all_entries;
        const char *parsername;
	int flags;								/*!< bitfield of enum SCCPConfigOptionFlag */
	sccp_configurationchange_t change;					/*!< Does a change of this value needs a device restart */
	const char *defaultValue;						/*!< Default value */
	const char *description;						/*!< Configuration description (config file) or warning message for deprecated or obsolete values */
/* *INDENT-ON* */
} SCCPConfigOption;

//converter function prototypes
sccp_value_changed_t sccp_config_parse_codec_preferences(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_mailbox(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_tos(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_cos(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_amaflags(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_variables(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_group(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_deny_permit(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_button(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vars, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_permithosts(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_addons(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_privacyFeature(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_debug(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_ipaddress(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_port(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_context(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_context(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_exten(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_hotline_label(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_enable(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_force(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_log(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_maxsize(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_impl(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_jbflags_jbresyncthreshold(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_checkButton(sccp_buttonconfig_list_t *buttonconfigList, int buttonindex, sccp_config_buttontype_t type, const char *name, const char *options, const char *args);
sccp_value_changed_t sccp_config_parse_webdir(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);
sccp_value_changed_t sccp_config_parse_earlyrtp(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment);

#include "sccp_config_entries.hh"

/*!
 * \brief SCCP Config Option Struct
 */
typedef struct SCCPConfigSegment {
	const char *name;
	const SCCPConfigOption *config;
	long unsigned int config_size;
	const sccp_config_segment_t segment;
} SCCPConfigSegment;

/*!
 * \brief SCCP Config Option Struct Initialization
 */
static const SCCPConfigSegment sccpConfigSegments[] = {
	{"general", sccpGlobalConfigOptions, ARRAY_LEN(sccpGlobalConfigOptions), SCCP_CONFIG_GLOBAL_SEGMENT},
	{"device", sccpDeviceConfigOptions, ARRAY_LEN(sccpDeviceConfigOptions), SCCP_CONFIG_DEVICE_SEGMENT},
	{"line", sccpLineConfigOptions, ARRAY_LEN(sccpLineConfigOptions), SCCP_CONFIG_LINE_SEGMENT},
	{"softkey", sccpSoftKeyConfigOptions, ARRAY_LEN(sccpSoftKeyConfigOptions), SCCP_CONFIG_SOFTKEY_SEGMENT},
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
	char * token = NULL;
	char * tokenrest = NULL;
	char * config_name = NULL;

	for (i = 0; i < sccpConfigSegment->config_size; i++) {
		if (strstr(config[i].name, delims) != NULL) {
			config_name = pbx_strdup(config[i].name);
			token = strtok_r(config_name, delims, &tokenrest);
			while (token != NULL) {
				if(strcasecmp(token, name) == 0) {
					sccp_free(config_name);
					return &config[i];
				}
				token = strtok_r(NULL, delims, &tokenrest);
			}
			sccp_free(config_name);
		}
		if(strcasecmp(config[i].name, name) == 0) {
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
	char option_name[strlen(configOptionName) + 2];
	char * token = NULL;
	char * tokenrest = NULL;

	snprintf(option_name, sizeof(option_name), "%s%s", configOptionName, delims);
	token = strtok_r(option_name, delims, &tokenrest);
	while (token != NULL) {
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Token %s/%s\n", option_name, token);
		for (v = cat_root; v; v = v->next) {
			if(strcasecmp(token, v->name) == 0) {
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
		token = strtok_r(NULL, delims, &tokenrest);
	}
EXIT:
	return out;
}

static PBX_VARIABLE_TYPE *createVariableSetForTokenizedDefault(const char *configOptionName, const char *defaultValue, PBX_VARIABLE_TYPE * out)
{
	PBX_VARIABLE_TYPE *tmp = NULL;

	char delims[] = "|";
	char *option_name_tokens = pbx_strdupa(configOptionName);
	char *option_value_tokens = pbx_strdupa(defaultValue);
	char * option_name_tokens_saveptr = NULL;
	char * option_value_tokens_saveptr = NULL;

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
static sccp_configurationchange_t sccp_config_object_setValue(void * const obj, PBX_VARIABLE_TYPE * cat_root, const char * name, const char * value, int lineno, const sccp_config_segment_t segment, boolean_t * SetEntries,
							      boolean_t default_run)
{
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *sccpConfigOption = sccp_find_config(segment, name);
	void * dst = NULL;
	enum SCCPConfigOptionType type = 0;  /* enum wrapper */
	enum SCCPConfigOptionFlag flags = 0; /* enum wrapper */
	const char * deprecated_obsolete_url = "https://github.com/chan-sccp/chan-sccp/wiki/Deprecated---Obsoleted-Parameters";

	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;						/* indicates config value is changed or not */
	sccp_configurationchange_t changes = SCCP_CONFIG_NOUPDATENEEDED;

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "SCCP: parsing %s parameter: %s %s%s%s (line: %d)\n", sccpConfigSegment->name, name, value ? "= '" : "", value ? value : "", value ? "' " : "", lineno);

	short int int8num = 0;
	int int16num = 0;
	long int int32num = 0;
	long long int int64num = 0;
	short unsigned int uint8num = 0;
	unsigned int uint16num = 0;
	long unsigned int uint32num = 0;
	long long unsigned int uint64num = 0;
	boolean_t boolean = 0;
	char * str = NULL;
	char oldChar = 0;
	char * tmp_value = NULL;

	if (!sccpConfigOption) {
		if (strlen(name) == 0 || name[0] != '_') {							/* skip generating error, when column name starts with '_' */
			pbx_log(LOG_WARNING, "SCCP: Unknown param at %s:%d:%s='%s'\n", sccpConfigSegment->name, lineno, name, value);
		}
		return SCCP_CONFIG_NOUPDATENEEDED;
	}

	dst = ((uint8_t *) obj) + sccpConfigOption->offset;
	type = (enum SCCPConfigOptionType) sccpConfigOption->type;
	flags = (enum SCCPConfigOptionFlag) sccpConfigOption->flags;
	
	// check if already set during first pass (multi_entry)
	if (SetEntries != NULL && ((flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)) {
		uint y = 0;

		for (y = 0; y < sccpConfigSegment->config_size; y++) {
			if (sccpConfigOption->offset == sccpConfigSegment->config[y].offset) {
				if (SetEntries[y] == TRUE) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_object_setValue) Set Entry[%d] = TRUE for MultiEntry %s -> SKIPPING\n", y, sccpConfigSegment->config[y].name);
					return SCCP_CONFIG_NOUPDATENEEDED;
				}
			}
		}
	}

	if ((flags & SCCP_CONFIG_FLAG_IGNORE) == SCCP_CONFIG_FLAG_IGNORE) {
		//sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s='%s' in line %d ignored\n", name, value, lineno);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} if ((flags & SCCP_CONFIG_FLAG_CHANGED) == SCCP_CONFIG_FLAG_CHANGED && !default_run) {
		pbx_log(LOG_NOTICE, "SCCP: changed config param at %s='%s' in line %d\n - %s -> please check sccp.conf file\n", name, value, lineno, sccpConfigOption->description);
	} else if ((flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED && lineno > 0 && !default_run) {
		pbx_log(LOG_WARNING, "SCCP: deprecated config param at %s='%s' in line %d\n - %s -> using old implementation.\nSee:%s\n", name, value, lineno, sccpConfigOption->description, deprecated_obsolete_url);
	} else if ((flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE && lineno > 0 && !default_run) {
		pbx_log(LOG_ERROR, "SCCP: obsolete config param at %s='%s' in line %d\n - %s -> param skipped\nSee: %s\n", name, value, lineno, sccpConfigOption->description, deprecated_obsolete_url);
		return SCCP_CONFIG_NOUPDATENEEDED;
	} else if ((flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) {
		if (NULL == value) {
			pbx_log(LOG_WARNING, "SCCP: required config param at %s='<NULL>' - %s\n", name, sccpConfigOption->description);
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
				if (sccp_strlen(value) > sccpConfigOption->size - 1) {
					pbx_log(LOG_NOTICE, "SCCP: config parameter %s:%s value '%s' is too long, only using the first %d characters\n", sccpConfigSegment->name, name, value, (int) sccpConfigOption->size - 1);
				}
				if(strncasecmp(str, value, sccpConfigOption->size - 1) != 0) {
					if (GLOB(reload_in_progress)) {
						sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s '%s' != '%s'\n", name, str, value);
					}
					changed = SCCP_CONFIG_CHANGE_CHANGED;
					pbx_copy_string((char *)dst, value, sccpConfigOption->size);
				}
			} else if (!sccp_strlen_zero(str)) {
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				pbx_copy_string((char *)dst, "", sccpConfigOption->size);
			}
			break;

		case SCCP_CONFIG_DATATYPE_STRINGPTR:
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
			str = *(char **) dst;

			if (!sccp_strequals(str, value)) {
				// pbx_log(LOG_NOTICE, "SCCP: Adding %s='%s' -> '%s'\n", name, str, value);
				changed = SCCP_CONFIG_CHANGE_CHANGED;
			}
			if (SCCP_CONFIG_CHANGE_CHANGED == changed) {
				if(*(void **)dst) {
					sccp_free(*(void **)dst);
				}
				if (value) {
					*(void **) dst = pbx_strdup(value);
				} else {
					*(void **) dst = NULL;
				}
			}
			break;

		case SCCP_CONFIG_DATATYPE_INT:
			if (sccp_strlen_zero(value)) {
				tmp_value = pbx_strdupa("0");
			} else {
				tmp_value = pbx_strdupa(value);
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
				tmp_value = pbx_strdupa("0");
			} else {
				tmp_value = pbx_strdupa(value);
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
					if ((!strncmp("0x", tmp_value, 2) && sscanf(tmp_value, "%ux", &uint16num)) || (sscanf(tmp_value, "%u", &uint16num) == 1)) {
						if ((*(uint16_t *) dst) != uint16num) {
							*(uint16_t *) dst = uint16num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 4:
					if ((!strncmp("0x", tmp_value, 2) && sscanf(tmp_value, "%lx", &uint32num)) || (sscanf(tmp_value, "%lu", &uint32num) == 1)) {
						if ((*(uint32_t *) dst) != uint32num) {
							*(uint32_t *) dst = uint32num;
							changed = SCCP_CONFIG_CHANGE_CHANGED;
						}
					}
					break;
				case 8:
					if ((!strncmp("0x", tmp_value, 2) && sscanf(tmp_value, "%llx", &uint64num)) || (sscanf(tmp_value, "%llu", &uint64num) == 1)) {
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
				if (sccp_true(value)) {
					boolean = TRUE;
				} else if (!sccp_true(value)) {
					boolean = FALSE;
				} else {
					pbx_log(LOG_NOTICE, "SCCP: Invalid value '%s' for [%s]->%s. Allowed: [TRUE/ON/YES or FALSE/OFF/NO]\n", value, sccpConfigSegment->name, name);
					changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
					break;
				}
			}

			if (*(boolean_t *) dst != boolean) {
				*(boolean_t *) dst = boolean;
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
				int enumValue = -1;
				if (!sccp_strlen_zero(value)) {
					const char *all_entries = sccpConfigOption->all_entries();
					if (!strncasecmp(value, "On,Yes,True,Off,No,False", strlen(value))) {
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
					} else if (!strncmp("0x", value, 2) && sscanf(value, "%x", &enumValue)) {
						sccp_log(DEBUGCAT_HIGH) ("SCCP: Parse Other Value: %s -> %d\n", value, enumValue);
					} else if (sscanf(value, "%d", &enumValue)) {
						sccp_log(DEBUGCAT_HIGH) ("SCCP: Parse Other Value: %s -> %d\n", value, enumValue);
					} else if ((enumValue = sccpConfigOption->str2intval(value)) != -1) {
						sccp_log(DEBUGCAT_HIGH) ("SCCP: Parse Other Value: %s -> %d\n", value, enumValue);
					}
					if (enumValue != -1) {
			                        switch (sccpConfigOption->size) {
						case 1:
							if (*(int8_t *) dst != (int8_t)enumValue) {
								*(int8_t *) dst = (int8_t)enumValue;
								changed = SCCP_CONFIG_CHANGE_CHANGED;
							}
							break;
						case 2:
							if ((*(uint16_t *) dst) != (int16_t)enumValue) {
								*(uint16_t *) dst = (int16_t)enumValue;
								changed = SCCP_CONFIG_CHANGE_CHANGED;
							}
							break;
						default:
							if (*(int *) dst != enumValue) {
								*(int *) dst = enumValue;
								changed = SCCP_CONFIG_CHANGE_CHANGED;
							}
							break;
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
	}

	if (SCCP_CONFIG_CHANGE_CHANGED == changed) {
		if (GLOB(reload_in_progress)) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "SCCP: config parameter %s='%s' in line %d changed. %s\n", name, value, lineno, SCCP_CONFIG_NEEDDEVICERESET == sccpConfigOption->change ? "(causes device reset)" : "");
		}
		changes = sccpConfigOption->change;
	}

	if ((SCCP_CONFIG_CHANGE_INVALIDVALUE != changed && SCCP_CONFIG_CHANGE_ERROR != changed) || 
		((flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)		/* Multi_Entry could give back invalid for one of it's values */
	) {
		/* if SetEntries is provided lookup the first offset of the struct variable we have set and note the index in SetEntries by changing the boolean_t to TRUE */
		if (SetEntries != NULL) {
			uint x = 0;

			for (x = 0; x < sccpConfigSegment->config_size; x++) {
				if (sccpConfigOption->offset == sccpConfigSegment->config[x].offset) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_object_setValue) Set Entry[%d] = TRUE for %s\n", x, sccpConfigSegment->config[x].name);
					SetEntries[x] = TRUE;
				}
			}
		}
	}
	if (SCCP_CONFIG_CHANGE_INVALIDVALUE == changed && !default_run) {
		pbx_log(LOG_NOTICE, "SCCP: Option Description: %s\n", sccpConfigOption->description);
	}
	if (SCCP_CONFIG_CHANGE_ERROR == changed) {
		pbx_log(LOG_NOTICE, "SCCP: Exception/Error during parsing of: %s\n", sccpConfigOption->description);
	}
	return changes;
}

/*!
 * \brief Set SCCP obj defaults from predecessor (device / global)
 *
 * check if we can find the param name in the segment specified and retrieving its value or default value
 * copy the string from the defaultSegment and run through the converter again
 */
static void sccp_config_set_defaults(void * const obj, const sccp_config_segment_t segment, boolean_t * SetEntries)
{
	if (!GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "GLOB(cfg) not available. Skip loading default setting.\n");
		return;
	}
	/* destination segment */
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);

	/* destination/source config */
	const SCCPConfigOption *sccpDstConfig = sccpConfigSegment->config;
	const SCCPConfigOption * sccpDefaultConfigOption = NULL;
	sccp_device_t * referral_device = NULL; /* need to find a way to find the default device to copy */
	char *referral_cat = "";
	sccp_config_segment_t search_segment_type = 0;
	boolean_t referralValueFound = FALSE;

	// already Set
	uint skip_elem = 0;
	boolean_t skip = FALSE;

	/* find the defaultValue, first check the reference, if no reference is specified, us the local defaultValue */
	long unsigned int cur_elem = 0;

	for (cur_elem = 0; cur_elem < sccpConfigSegment->config_size; cur_elem++) {
		/* Lookup the first offset of the struct variable we want to set default for, find the corresponding entry in the SetEntries array and check the boolean flag, skip if true */
		skip = FALSE;
		for (skip_elem = 0; skip_elem < sccpConfigSegment->config_size; skip_elem++) {
			//if (sccpDstConfig[cur_elem].offset == sccpConfigSegment->config[skip_elem].offset && (SetEntries[skip_elem] || sccpConfigSegment->config[cur_elem].flags & (SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_OBSOLETE))) {
			if (sccpDstConfig[cur_elem].offset == sccpConfigSegment->config[skip_elem].offset && (SetEntries[skip_elem] || sccpConfigSegment->config[cur_elem].flags & (SCCP_CONFIG_FLAG_OBSOLETE))) {
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
				referral_cat = referral_device->id;
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
				PBX_VARIABLE_TYPE * v = NULL;
				PBX_VARIABLE_TYPE * cat_root = NULL;
				char option_tokens[sccp_strlen(sccpDstConfig[cur_elem].name) + 2];
				referralValueFound = FALSE;

				/* tokenparsing */
				snprintf(option_tokens, sizeof(option_tokens), "%s|", sccpDstConfig[cur_elem].name);
				char *option_tokens_saveptr = NULL;
				char *option_name = strtok_r(option_tokens, "|", &option_tokens_saveptr);
				do {
					/* search for the default values in the referred segment, if found break so we can pass on the cat_root */
					for (cat_root = v = ast_variable_browse(GLOB(cfg), referral_cat); v; v = v->next) {
						if (sccp_strcaseequals((const char *) option_name, v->name)) {
							sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "Found name:'%s', value:'%s', use referred config-file value from segment '%s'\n", option_name, v->value, referral_cat);
							referralValueFound = TRUE;
							break;
						}
					}
				} while ((option_name = strtok_r(NULL, "|", &option_tokens_saveptr)) != NULL);

				if (referralValueFound && v) {							/* if referred to other segment and a value was found, pass the newly found cat_root directly to setValue */
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Refer default value lookup for parameter:'%s' through '%s' segment\n", sccpDstConfig[cur_elem].name, referral_cat);
					sccp_config_object_setValue(obj, cat_root, sccpDstConfig[cur_elem].name, v->value, __LINE__, segment, SetEntries, TRUE);
					continue;
				} else {									/* if referred but no default value was found, pass on the defaultValue of the referred segment in raw string form (including tokens) */
					sccpDefaultConfigOption = sccp_find_config(search_segment_type, sccpDstConfig[cur_elem].name);
					if (sccpDefaultConfigOption && !sccp_strlen_zero(sccpDefaultConfigOption->defaultValue)) {
						sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Set parameter '%s' to segment default, being:'%s'\n", sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue);
						sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, sccpDefaultConfigOption->defaultValue, __LINE__, segment, SetEntries, TRUE);
						continue;
					}
				}
				
			} else if (!sccp_strlen_zero(sccpDstConfig[cur_elem].defaultValue)) {			/* Non-Referral, pass defaultValue on in raw string format (including tokens) */
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Set parameter '%s' to own default, being:'%s'\n", sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue);
				sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, sccpDstConfig[cur_elem].defaultValue, __LINE__, segment, SetEntries, TRUE);
				continue;
			}

			if (type == SCCP_CONFIG_DATATYPE_STRINGPTR || type==SCCP_CONFIG_DATATYPE_PARSER) {	/* If nothing was found, clear variable, incase of a STRINGPTR */
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Clearing parameter %s\n", sccpDstConfig[cur_elem].name);
				sccp_config_object_setValue(obj, NULL, sccpDstConfig[cur_elem].name, "", __LINE__, segment, SetEntries, TRUE);
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
void sccp_config_cleanup_dynamically_allocated_memory(void * const obj, const sccp_config_segment_t segment)
{
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(segment);
	const SCCPConfigOption *sccpConfigOption = sccpConfigSegment->config;
	void * dst = NULL;
	char * str = NULL;

	long unsigned int i = 0;
	for (i = 0; i < sccpConfigSegment->config_size; i++) {
		if (sccpConfigOption[i].type == SCCP_CONFIG_DATATYPE_STRINGPTR) {
			dst = ((uint8_t *) obj) + sccpConfigOption[i].offset;
			str = *(char **) dst;
			if (str) {
				// pbx_log(LOG_NOTICE, "SCCP: Freeing %s='%s'\n", sccpConfigOption[i].name, str);
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
sccp_value_changed_t sccp_config_parse_ipaddress(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);

#if ASTERISK_VERSION_GROUP == 106
	if (sccp_strequals(value, "::")) {
		pbx_log(LOG_ERROR, "Asterisk 1.6, does not support ipv6, '::' has been replaced with '0.0.0.0'\n");
		value = pbx_strdupa("::");
	}
#endif
	if (sccp_strlen_zero(value)) {
		value = pbx_strdupa("0.0.0.0");
	}
	struct sockaddr_storage bindaddr_prev = (*(struct sockaddr_storage *) dest);
	struct sockaddr_storage bindaddr_new = { 0, };

	if (!sccp_sockaddr_storage_parse(&bindaddr_new, value, PARSE_PORT_FORBID)) {
		pbx_log(LOG_WARNING, "Invalid IP address: %s\n", value);
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if (sccp_netsock_cmp_addr(&bindaddr_prev, &bindaddr_new)) {					// 0 = equal
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
sccp_value_changed_t sccp_config_parse_port(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);

	int new_port = 0;
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
sccp_value_changed_t sccp_config_parse_privacyFeature(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	sccp_featureConfiguration_t privacyFeature = {0};

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
		//*(sccp_featureConfiguration_t *) dest = privacyFeature;
		memcpy(dest, &privacyFeature, sizeof(sccp_featureConfiguration_t));
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
   sccp_value_changed_t sccp_config_parse_mwilamp(void *const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
   {
   sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
   char *value = pbx_strdupa(v->value);
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
sccp_value_changed_t sccp_config_parse_tos(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	uint8_t tos = 0;

	if (pbx_str2tos(value, &tos)) {
		/* value is tos */
	} else if (sscanf(value, "%" SCNu8, &tos) == 1) {
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

	if ((*(uint8_t *) dest) != tos) {
		*(uint8_t *) dest = tos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Cos Value
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_cos(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	uint8_t cos = 0;

	if (pbx_str2cos(value, &cos)) {
		/* value is tos */
	} else if (sscanf(value, "%" SCNu8, &cos) == 1) {
		if (cos > 7) {
			pbx_log(LOG_WARNING, "Invalid cos %d value, refer to QoS documentation\n", cos);
			return SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	}

	if ((*(uint8_t *) dest) != cos) {
		*(uint8_t *) dest = cos;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}

	return changed;
}

/*!
 * \brief Config Converter/Parser for AmaFlags Value
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_amaflags(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	int amaflags = 0;

	if (!sccp_strlen_zero(value)) {
		amaflags = pbx_channel_string2amaflag(value);
		if ((*(int *) dest) != amaflags) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			*(int *) dest = amaflags;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Secondary Dialtone Digits
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_secondaryDialtoneDigits(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	char *str = (char *) dest;

	if (sccp_strlen(value) <= 9) {
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
sccp_value_changed_t sccp_config_parse_group(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);

	char * piece = NULL;
	char * c = NULL;
	int start = 0;

	int finish = 0;

	int x = 0;
	sccp_group_t group = 0;

	if (!sccp_strlen_zero(value)) {
		c = pbx_strdupa(value);

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
sccp_value_changed_t sccp_config_parse_context(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	if (v->value && !sccp_strlen_zero(v->value)) {
		char *value = pbx_strdupa(v->value);
		char *str = (char *) dest;
		if (!sccp_strcaseequals(str, value)) {
			changed = SCCP_CONFIG_CHANGE_CHANGED;
			sccp_copy_string((char *)dest, value, size);
			//if (!sccp_strlen_zero(value) && !pbx_context_find((const char *) dest)) {
			//	pbx_log(LOG_WARNING, "The context '%s' you specified might not be available in the dialplan. Please check the sccp.conf\n", (char *) dest);
			//}
		} else {
			changed = SCCP_CONFIG_CHANGE_NOCHANGE;
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Context
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_hotline_context(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	sccp_hotline_t *hotline = *(sccp_hotline_t **) dest;
	
	if (hotline->line && !sccp_strcaseequals(hotline->line->context, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		if (hotline->line->context) {
			sccp_free(hotline->line->context);
		}
		hotline->line->context = pbx_strdup(value);
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Extension
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_hotline_exten(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	sccp_hotline_t *hotline = *(sccp_hotline_t **) dest;
	
	if (!sccp_strcaseequals(hotline->exten, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		pbx_copy_string(hotline->exten, value, SCCP_MAX_EXTENSION);
		if (hotline->line) {
			if (hotline->line->adhocNumber) {
				sccp_free(hotline->line->adhocNumber);
			}
			hotline->line->adhocNumber = pbx_strdup(value);
		}
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Hotline Extension
 *
 * \note not multi_entry
 */
sccp_value_changed_t sccp_config_parse_hotline_label(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	sccp_hotline_t *hotline = *(sccp_hotline_t **) dest;

	if (hotline->line && !sccp_strcaseequals(hotline->line->label, value)) {
		changed = SCCP_CONFIG_CHANGE_CHANGED;
		if (hotline->line->label) {
			sccp_free(hotline->line->label);
		}
		hotline->line->label = pbx_strdup(value);
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Jitter Buffer Flags
 *
 * \note not multi_entry
 */
static sccp_value_changed_t sccp_config_parse_jbflags(void * const dest, const char * value, const unsigned int flag)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	struct ast_jb_conf *jb = *(struct ast_jb_conf **) dest;

	if (pbx_test_flag(jb, flag) != (unsigned) sccp_true(value)) {
		pbx_set2_flag(jb, sccp_true(value), flag);
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

sccp_value_changed_t sccp_config_parse_jbflags_enable(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = pbx_strdupa(v->value);

	return sccp_config_parse_jbflags(dest, value, AST_JB_ENABLED);
}

sccp_value_changed_t sccp_config_parse_jbflags_force(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = pbx_strdupa(v->value);

	return sccp_config_parse_jbflags(dest, value, AST_JB_FORCED);
}

sccp_value_changed_t sccp_config_parse_jbflags_log(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	char *value = pbx_strdupa(v->value);

	return sccp_config_parse_jbflags(dest, value, AST_JB_LOG);
}

sccp_value_changed_t sccp_config_parse_jbflags_maxsize(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int value = sccp_atoi(v->value, strlen(v->value));
	struct ast_jb_conf *jb = *(struct ast_jb_conf **) dest;
	
	if (jb->max_size != value) {
		jb->max_size = value;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

sccp_value_changed_t sccp_config_parse_jbflags_jbresyncthreshold(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int value = sccp_atoi(v->value, strlen(v->value));
	struct ast_jb_conf *jb = *(struct ast_jb_conf **) dest;
	
	if (jb->resync_threshold != value) {
		jb->resync_threshold = value;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

sccp_value_changed_t sccp_config_parse_jbflags_impl(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char * value = pbx_strdupa(v->value);
	struct ast_jb_conf *jb = *(struct ast_jb_conf **) dest;
	
	if (!sccp_strcaseequals(jb->impl,value)) {
		sccp_copy_string(jb->impl, value, sizeof jb->impl);
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for WebDir
 */
sccp_value_changed_t sccp_config_parse_webdir(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	char *value = pbx_strdupa(v->value);
	char *webdir = (char *) dest;
	char new_webdir[PATH_MAX] = "";

	if (sccp_strlen_zero(value)) {
	        snprintf(new_webdir, sizeof(new_webdir), "%s/%s", ast_config_AST_DATA_DIR, "static-http/");
	} else {
                snprintf(new_webdir, sizeof(new_webdir), "%s", value);
	}
	
        if (!sccp_strcaseequals(new_webdir, webdir)) {
	        if (access(new_webdir, F_OK ) != -1) {
                        changed = SCCP_CONFIG_CHANGE_CHANGED;
                        pbx_copy_string(webdir, new_webdir, size);
	        } else {
			pbx_log(LOG_WARNING, "The webdir '%s' specified could not be found.\n", new_webdir);
			pbx_copy_string(webdir, "", size);
	                changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	        }
        } else {
                changed = SCCP_CONFIG_CHANGE_NOCHANGE;
        }
	return changed;
}

/*!
 * \brief Config Converter/Parser for Debug
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_debug(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	uint32_t debug_new = 0;
	char * debug_arr[1] = { 0 };

	for (; v; v = v->next) {
		debug_arr[0] = pbx_strdup (v->value);
		debug_new = sccp_parse_debugline(debug_arr, 0, 1, debug_new);
		sccp_free (debug_arr[0]);
	}
	if (*(uint32_t *) dest != debug_new) {
		*(uint32_t *) dest = debug_new;
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

/*!
 * \brief Config Converter/Parser for Earlyrtp
 *
 * temporary "any string" to boolean for earlyrtp
 */
sccp_value_changed_t sccp_config_parse_earlyrtp(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	boolean_t old = *(boolean_t *)dest;
	boolean_t new = !sccp_false(v->value);

	if(sccp_strcaseequals(v->value, "none")) {
		new = FALSE;
	}

	if(new != old) {
		*(boolean_t *)dest = new;
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
sccp_value_changed_t sccp_config_parse_codec_preferences(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	skinny_capabilities_t *prefs = (skinny_capabilities_t *) dest;
	skinny_codec_t new_codecs[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE};
	int errors = 0;

	for (; v; v = v->next) {
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("sccp_config_parse_codec preference: name: %s, value:%s\n", v->name, v->value);
		if (sccp_strcaseequals(v->name, "disallow")) {
			errors += sccp_codec_parseAllowDisallow(new_codecs, v->value, 0);
		} else if (sccp_strcaseequals(v->name, "allow")) {
			errors += sccp_codec_parseAllowDisallow(new_codecs, v->value, 1);
		} else {
			errors += 1;
		}
	}
	
	skinny_codec_t audio_prefs[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONE};
	sccp_get_codecs_bytype(new_codecs, audio_prefs, SKINNY_CODEC_TYPE_AUDIO);
#if CS_SCCP_VIDEO
	skinny_codec_t video_prefs[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONE};
	sccp_get_codecs_bytype(new_codecs, video_prefs, SKINNY_CODEC_TYPE_VIDEO);
#endif
	if (errors) {
		pbx_log(LOG_NOTICE, "SCCP: (parse_codec preference) Error occured during parsing of the disallowed / allowed codecs\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	} else {
		if(memcmp(prefs->audio, audio_prefs, sizeof prefs->audio) != 0) {
			memcpy(prefs->audio, audio_prefs, sizeof prefs->audio);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
#if CS_SCCP_VIDEO
		if(memcmp(prefs->video, video_prefs, sizeof prefs->video) != 0) {
			memcpy(prefs->video, video_prefs, sizeof prefs->video);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
#endif
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
sccp_value_changed_t sccp_config_parse_deny_permit(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	int error = 0;
	int errors = 0;

	struct sccp_ha *prev_ha = *(struct sccp_ha **) dest;
	struct sccp_ha *ha = NULL;

	for (; v; v = v->next) {
		//sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("sccp_config_parse_deny_permit: name: %s, value:%s\n", v->name, v->value);
		if (sccp_strcaseequals(v->name, "deny")) {
			ha = sccp_append_ha("deny", v->value, ha, &error);
			//sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Deny: %s\n", v->value);
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
			//sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Permit: %s\n", v->value);
		}
		errors |= error;
	}
	if (!error) {
		// compare ha elements
		struct ast_str *ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		struct ast_str *prev_ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		if (ha_buf && prev_ha_buf) {
			sccp_print_ha(ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, ha);
			sccp_print_ha(prev_ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, prev_ha);
			if (!sccp_strequals(pbx_str_buffer(ha_buf), pbx_str_buffer(prev_ha_buf))) {
				//sccp_log_and(DEBUGCAT_CONFIG + DEBUGCAT_HIGH) ("hal: %s\nprev_ha: %s\n", pbx_str_buffer(ha_buf), pbx_str_buffer(prev_ha_buf));
				if (prev_ha) {
					sccp_free_ha(prev_ha);
				}
				*(struct sccp_ha **) dest = ha;
				changed = SCCP_CONFIG_CHANGE_CHANGED;
				ha = NULL;					// passed on to dest, will not be freed at exit
			}
		} else {
			pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
			changed = SCCP_CONFIG_CHANGE_ERROR;
		}
	} else {
		sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_parse_deny_permit) Invalid\n");
		changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}

	/* cleanup resources when not passed on to dest */
	if (ha) {
		sccp_free_ha(ha);
	}
	//sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: (sccp_config_parse_deny_permit) Return: %d\n", changed);
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
sccp_value_changed_t sccp_config_parse_permithosts(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_hostname_t *permithost = NULL;

	SCCP_LIST_HEAD (hostname, sccp_hostname_t) * permithostList = (struct hostname *)dest;

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
			if (!(permithost = (sccp_hostname_t *)sccp_calloc(1, sizeof(sccp_hostname_t)))) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return SCCP_CONFIG_CHANGE_ERROR;
			}
			sccp_copy_string(permithost->name, v->value, sizeof(permithost->name));
			SCCP_LIST_INSERT_TAIL(permithostList, permithost, list);
		}
		changed = SCCP_CONFIG_CHANGE_CHANGED;
	}
	return changed;
}

static skinny_devicetype_t addonstr2enum(const char *addonstr)
{
	if (sccp_strcaseequals(addonstr, "7914")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7914;
	}
	if (sccp_strcaseequals(addonstr, "7915")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON;
	}
	if (sccp_strcaseequals(addonstr, "7916")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON;
	}
	if (sccp_strcaseequals(addonstr, "500S")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S;
	}
	if (sccp_strcaseequals(addonstr, "500DS")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS;
	}
	if (sccp_strcaseequals(addonstr, "932DS")) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Unknown addon type (%s)\n", addonstr);
	return SKINNY_DEVICETYPE_SENTINEL;
}

/*!
 * \brief Config Converter/Parser for addons
 * 
 * \todo make more generic
 * \todo cleanup original implementation in sccp_utils.c
 *
 * \note multi_entry
 */
sccp_value_changed_t sccp_config_parse_addons(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	unsigned int changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_addon_t *addon = NULL;
	skinny_devicetype_t addon_type = 0;

	SCCP_LIST_HEAD (addon, sccp_addon_t) * addonList = (struct addon *)dest;

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(addonList, addon, list) {
		if (v) {
			if (!sccp_strlen_zero(v->value)) {
				if((addon_type = addonstr2enum(v->value)) && addon_type != SKINNY_DEVICETYPE_SENTINEL) {
					if (addon->type != addon_type) {					/* change/update */
						sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH))("change addon: %s(%d) => %s(%d)\n", skinny_devicetype2str(addon->type), addon->type, skinny_devicetype2str(addon_type),
												addon_type);
						addon->type = addon_type;
						changed |= SCCP_CONFIG_CHANGE_CHANGED;
					}
				} else {
					pbx_log(LOG_ERROR, "unknown addon type: %s, skipped\n", v->value);
					changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
				}
			}
			v = v->next;
		} else {											/* removal */
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("remove addon: %d\n", addon->type);
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
				if((addon_type = addonstr2enum(v->value)) && addon_type != SKINNY_DEVICETYPE_SENTINEL) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH))("add new addon: %s(%d)\n", skinny_devicetype2str(addon_type), addon_type);
					if (!(addon = (sccp_addon_t *)sccp_calloc(1, sizeof(sccp_addon_t)))) {
						pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
						return SCCP_CONFIG_CHANGE_ERROR;
					}
					addon->type = addon_type;
					SCCP_LIST_INSERT_TAIL(addonList, addon, list);
					changed |= SCCP_CONFIG_CHANGE_CHANGED;
				} else {
					pbx_log(LOG_ERROR, "unknown addon type: %s, skipped\n", v->value);
					changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
				}
			}
		} else {
			pbx_log(LOG_ERROR, "SCCP: maximum number(2) of addon's has been reached\n");
			changed |= SCCP_CONFIG_CHANGE_INVALIDVALUE;
		}
	}
	return (sccp_value_changed_t)changed;
}

/*!
 * \brief Config Converter/Parser for Mailbox Value
 *
 * \note multi_entry
 * \note order is irrelevant
 */
sccp_value_changed_t sccp_config_parse_mailbox(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vroot, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;
	sccp_mailbox_t *mailbox = NULL;

	SCCP_LIST_HEAD (mailbox, sccp_mailbox_t) * mailboxList = (struct mailbox *)dest;

	PBX_VARIABLE_TYPE *v = NULL;
	int varCount = 0;
	int listCount = 0;

	listCount = mailboxList->size;
	int notfound = 0;

	for (v = vroot; v; v = v->next) {
		if (!sccp_strlen_zero(v->value)) {
			varCount++;
		}
	}

	if (varCount == listCount) {										// list length equal
		SCCP_LIST_TRAVERSE(mailboxList, mailbox, list) {
			for (v = vroot; v; v = v->next) {
				if (!sccp_strlen_zero(v->value)) {
					char uniqueid[SCCP_MAX_MAILBOX_UNIQUEID];
					snprintf(uniqueid, sizeof(uniqueid), "%s%s", v->value, !strstr(v->value, "@") ? "@default" : "");
					if (sccp_strcaseequals(mailbox->uniqueid, uniqueid)) {
						continue;
					}
					notfound += 1;
				}
			}
		}
	}
	if (varCount != listCount || notfound) {								// build new list
		while ((mailbox = SCCP_LIST_REMOVE_HEAD(mailboxList, list))) {					// clear list
			sccp_free(mailbox);
		}
		for (v = vroot; v; v = v->next) {								// create new list
			if (!sccp_strlen_zero(v->value)) {
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "add new mailbox: '%s'\n", v->value);
				if (!(mailbox = (sccp_mailbox_t *)sccp_calloc(1, sizeof(sccp_mailbox_t)))) {
					pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
					return SCCP_CONFIG_CHANGE_ERROR;
				}
				snprintf(mailbox->uniqueid, sizeof(mailbox->uniqueid), "%s%s", v->value, !strstr(v->value, "@") ? "@default" : "");
				SCCP_LIST_INSERT_TAIL(mailboxList, mailbox, list);
			}
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
sccp_value_changed_t sccp_config_parse_variables(void * const dest, const size_t size, PBX_VARIABLE_TYPE * v, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	PBX_VARIABLE_TYPE *variableList = *(PBX_VARIABLE_TYPE **) dest;

	if (variableList) {
		pbx_variables_destroy(variableList);								/* destroy old list */
		variableList = NULL;
	}
	PBX_VARIABLE_TYPE *variable = variableList;
	char * var_name = NULL;
	char * var_value = NULL;

	for (; v; v = v->next) {										/* create new list */
		var_name = pbx_strdup (v->value);
		var_value = NULL;
		if ((var_value = strchr(var_name, '='))) {
			*var_value++ = '\0';
		}
		if (!sccp_strlen_zero(var_name) && !sccp_strlen_zero(var_value)) {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) ("add new variable: %s=%s\n", var_name, var_value);
			if (!variable) {
				if (!(variableList = pbx_variable_new(var_name, var_value, ""))) {
					pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
					variableList = NULL;
					break;
				}
				variable = variableList;
			} else {
				if (!(variable->next = pbx_variable_new(var_name, var_value, ""))) {
					pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
					pbx_variables_destroy(variableList);
					variableList = NULL;
					break;
				}
				variable = variable->next;
			}
		}
		sccp_free (var_name);
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
sccp_value_changed_t sccp_config_parse_button(void * const dest, const size_t size, PBX_VARIABLE_TYPE * vars, const sccp_config_segment_t segment)
{
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_CHANGED;

	/* parse all button definitions in one pass */
	char * buttonType = NULL;

	char * buttonName = NULL;

	char * buttonOption = NULL;

	char * buttonArgs = NULL;
	char k_button[256];
	char * splitter = NULL;
	sccp_config_buttontype_t type = EMPTY;									/* default to empty */
	uint buttonindex = 0;
	
	sccp_buttonconfig_list_t *buttonconfigList = (sccp_buttonconfig_list_t *)dest;
	sccp_buttonconfig_t *config = NULL;
	PBX_VARIABLE_TYPE * first_var = vars;
	PBX_VARIABLE_TYPE * v = NULL;

	/* temp current buttonconfiglist status*/
	{
		SCCP_LIST_LOCK(buttonconfigList);
		sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "buttonconfig status before check\n");
		SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
			sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "index:%d, type:%-10.10s (%d), pendingDelete:%s, pendingUpdate:%s\n",
			config->index, sccp_config_buttontype2str(config->type), config->type, config->pendingDelete ? "True" : "False", config->pendingUpdate ? "True" : "False");
		}
		SCCP_LIST_UNLOCK(buttonconfigList);
	}
	/* temp */

	if (GLOB(reload_in_progress)) {
		changed = SCCP_CONFIG_CHANGE_NOCHANGE;
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Checking Button Config\n");
		/* check if the number of buttons got reduced */
		for (v = first_var; v && !sccp_strlen_zero(v->value); v = v->next) {				/* check buttons against currently loaded set*/
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Checking button: %s\n", v->value);
			sccp_copy_string(k_button, v->value, sizeof(k_button));
			splitter = k_button;
			buttonType = strsep(&splitter, ",");
			buttonName = strsep(&splitter, ",");
			buttonOption = strsep(&splitter, ",");
			buttonArgs = splitter;

			type = sccp_config_buttontype_str2val(buttonType);
			if (type == SCCP_CONFIG_BUTTONTYPE_SENTINEL) {
				pbx_log(LOG_WARNING, "Unknown button type '%s'.\n", buttonType);
				changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
				type = EMPTY;
			}
			if ((changed = sccp_config_checkButton(buttonconfigList, buttonindex, type, buttonName ? pbx_strip(buttonName) : NULL, buttonOption ? pbx_strip(buttonOption) : NULL, buttonArgs ? pbx_strip(buttonArgs) : NULL))) {
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Button: %s changed. Giving up on checking buttonchanges, reloading all of them.\n", v->value);
				break;
			}
			buttonindex++;
		}
		if (!changed && SCCP_LIST_GETSIZE(buttonconfigList) != buttonindex) {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Number of Buttons changed (%d != %d). Reloading all of them.\n", SCCP_LIST_GETSIZE(buttonconfigList), buttonindex);
			changed = SCCP_CONFIG_CHANGE_CHANGED;
		}
		/* Clear/Set pendingDelete and PendingUpdate if button has changed or not
		 *
		 * \note Moved here from device_post_reload so that we will know the new state before line_post_reload. 
		 * That way adding/removind a line while accidentally keeping the button config for that line still works.
		 */
		if (!changed) {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Nothing changed, clear the pendingDelete and pendingUpdate settings\n");
			SCCP_LIST_LOCK(buttonconfigList);
			SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
				config->pendingDelete = 0;
				config->pendingUpdate = 0;
			}
			SCCP_LIST_UNLOCK(buttonconfigList);
		}
	}
	/* temp current buttonconfiglist status*/
	{
		SCCP_LIST_LOCK(buttonconfigList);
		sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "buttonconfig status after check\n");
		SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
			sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "index:%d, type:%-10.10s (%d), pendingDelete:%s, pendingUpdate:%s\n",
				config->index, sccp_config_buttontype2str(config->type), config->type, config->pendingDelete ? "True" : "False", config->pendingUpdate ? "True" : "False");
		}
		SCCP_LIST_UNLOCK(buttonconfigList);
	}
	/* temp */
	if (changed) {
		buttonindex = 0;										/* buttonconfig has changed. Load all buttons as new ones */
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Any Previous ButtonConfig will be discared during post-process\n");
		for (v = first_var; v && !sccp_strlen_zero(v->value); v = v->next) {
			sccp_copy_string(k_button, v->value, sizeof(k_button));
			splitter = k_button;
			buttonType = strsep(&splitter, ",");
			buttonName = strsep(&splitter, ",");
			buttonOption = strsep(&splitter, ",");
			buttonArgs = splitter;

			type = sccp_config_buttontype_str2val(buttonType);
			if (type == SCCP_CONFIG_BUTTONTYPE_SENTINEL) {
				pbx_log(LOG_WARNING, "Unknown button type '%s'. Will insert an Empty button instead.\n", buttonType);
				changed = SCCP_CONFIG_CHANGE_INVALIDVALUE;
				type = EMPTY;
			}
			sccp_config_addButton(buttonconfigList, buttonindex, type, buttonName ? pbx_strip(buttonName) : NULL, buttonOption ? pbx_strip(buttonOption) : NULL, buttonArgs ? pbx_strip(buttonArgs) : NULL);
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Added button: %s\n", v->value);
			buttonindex++;
		}
	}

	/* temp current buttonconfiglist status*/
	{
		SCCP_LIST_LOCK(buttonconfigList);
		sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "buttonconfig status after adding new buttons\n");
		SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
			sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "index:%d, type:%-10.10s (%d), pendingDelete:%s, pendingUpdate:%s\n",
				config->index, sccp_config_buttontype2str(config->type), config->type, config->pendingDelete ? "True" : "False", config->pendingUpdate ? "True" : "False");
		}
		SCCP_LIST_UNLOCK(buttonconfigList);
	}
	/* temp */

	/* return changed status */
	if (GLOB(reload_in_progress)) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "buttonconfig: %s\n", changed ? "changed" : "remained the same");
	}

	return changed;
}

/* end dyn config */
/*!
 * \brief check a Button against the current ButtonConfig on a device
 *
 * \param buttonconfigList pointer to the device->buttonconfig list
 * \param buttonindex button index
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
sccp_value_changed_t sccp_config_checkButton(sccp_buttonconfig_list_t *buttonconfigList, int buttonindex, sccp_config_buttontype_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;

	// boolean_t is_new = FALSE;
	sccp_value_changed_t changed = SCCP_CONFIG_CHANGE_NOCHANGE;

	SCCP_LIST_LOCK(buttonconfigList);
	SCCP_LIST_TRAVERSE(buttonconfigList, config, list) {
		if (config->index == buttonindex) {
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Found Button index at %d:%d\n", config->index, buttonindex);
			break;
		}
	}
	SCCP_LIST_UNLOCK(buttonconfigList);

	changed = SCCP_CONFIG_CHANGE_CHANGED;
	if (config) {
		switch (type) {
			case LINE:
			{
				char extension[SCCP_MAX_EXTENSION];
				sccp_subscription_id_t subscriptionId;
				int parseRes = sccp_parseComposedId(name, 80, &subscriptionId, extension);
				if (parseRes) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: ComposedId extension: %s, subscriptionId[number:%s, name:%s, label:%s, aux:%s]\n", extension, subscriptionId.number, subscriptionId.name, subscriptionId.label, subscriptionId.aux);
					if (LINE == config->type &&
						sccp_strequals(config->label, name) && 
						sccp_strequals(config->button.line.name, extension) && 
						((!config->button.line.subscriptionId && parseRes == 1) || 
						(config->button.line.subscriptionId &&
							(
								sccp_strcaseequals(config->button.line.subscriptionId->number, subscriptionId.number) &&
								sccp_strequals(config->button.line.subscriptionId->name, subscriptionId.name) && 
								sccp_strequals(config->button.line.subscriptionId->label, subscriptionId.label) && 
								sccp_strequals(config->button.line.subscriptionId->aux, subscriptionId.aux)
							)
						))
					) {
						if (!options || sccp_strequals(config->button.line.options, options)) {
							sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Line Button Definition remained the same\n");
							changed = SCCP_CONFIG_CHANGE_NOCHANGE;
						} else {
							sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "options: %s <-> %s  (%d)\n", config->button.line.options, options, sccp_strequals(config->button.line.options, options));
						}
					}
				} else {
					pbx_log(LOG_WARNING, "SCCP: button definition:'%s' could not be parsed\n", name);
				}
				break;
			}
			case SPEEDDIAL:
				/* \todo check if values change */
				if (SPEEDDIAL == config->type && sccp_strequals(config->label, name) && sccp_strequals(config->button.speeddial.ext, options)) {
					if (!args || sccp_strequals(config->button.speeddial.hint, args)) {
						sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Speeddial Button Definition remained the same\n");
						changed = SCCP_CONFIG_CHANGE_NOCHANGE;
					}
				}
				break;
			case SERVICE:
				if (SERVICE == config->type && sccp_strequals(config->label, name) && sccp_strequals(config->button.service.url, options)) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Service Button Definition remained the same\n");
					changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				}
				break;
			case FEATURE:
				if (	FEATURE == config->type 
					&& buttonindex == config->index
					&& sccp_strequals(config->label, name)
					&& config->button.feature.id == sccp_feature_type_str2val(options)
				) {
					char *default_option = "";
					char *default_arg = "";
					char combined_args[512] = "";
					char combined_current[512] = "";
					AST_DECLARE_APP_ARGS(elems, AST_APP_ARG(option); AST_APP_ARG(arg););
					if(args && !sccp_strlen_zero(args)) {
						char * parse = pbx_strdup (args);
						AST_STANDARD_APP_ARGS(elems, parse);
						sccp_free (parse);
					}
					snprintf(combined_current, sizeof(combined_current), "%s, %s",
						config->button.feature.options ? config->button.feature.options : "",
						config->button.feature.args ? config->button.feature.args : "");
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Feature Button Definition:%s,%s -> %s,%s\n", config->button.feature.options, config->button.feature.args, elems.option, elems.arg);
					if(SCCP_FEATURE_PARKINGLOT == config->button.feature.id) {
						default_option = "default";
						default_arg = "RetrieveSingle";
					} else
#ifdef CS_DEVSTATE_FEATURE
					if(SCCP_FEATURE_DEVSTATE == config->button.feature.id) {
						default_option = config->label;
						default_arg = "00001|10012|22321";
					}
#endif
					snprintf(combined_args, sizeof(combined_args), "%s, %s", elems.option ? elems.option : default_option, elems.arg ? elems.arg : default_arg);
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH))(VERBOSE_PREFIX_4 "old:'%s' / new:'%s'\n", combined_current, combined_args);
					if (
						(sccp_strequals(combined_current, combined_args))
					) {
						sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Feature Button Definition remained the same\n");
						changed = SCCP_CONFIG_CHANGE_NOCHANGE;
						break;
					}
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Feature Button Definition changed\n");
				}
				break;
			case EMPTY:
				if (EMPTY == config->type) {
					sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Button Definition remained the same\n");
					changed = SCCP_CONFIG_CHANGE_NOCHANGE;
				}
				break;
			default:
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_4 "SCCP: Unknown ButtonType: %d\n", type);
				break;
		}
	}
	if (changed) {	
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_4 "SCCP: ButtonTemplate has changed\n");
	} else {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_4 "SCCP: ButtonTemplate remained the same\n");
	}
	return changed;
}


/*!
 * \brief add a Button to a device
 *
 * \param buttonconfigList pointer to the device->buttonconfig list
 * \param buttonindex button index
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
sccp_value_changed_t sccp_config_addButton(sccp_buttonconfig_list_t *buttonconfigList, int buttonindex, sccp_config_buttontype_t type, const char *name, const char *options, const char *args)
{
	sccp_buttonconfig_t *config = NULL;
	sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Loading New Button Config\n");
	
	/*
	if (!sccp_config_buttontype_exists(type)) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_4 "SCCP: Unknown ButtonType. Skipping\n");
		return SCCP_CONFIG_CHANGE_INVALIDVALUE;
	}
	*/
	
	SCCP_LIST_LOCK(buttonconfigList);
	if (!(config = (sccp_buttonconfig_t *)sccp_calloc(1, sizeof(sccp_buttonconfig_t)))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return SCCP_CONFIG_CHANGE_ERROR;
	}
	config->index = buttonindex;
	config->type = type;
	sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "New %s Button '%s' at : %d:%d\n", sccp_config_buttontype2str(type), name, buttonindex, config->index);
	SCCP_LIST_INSERT_TAIL(buttonconfigList, config, list);
	SCCP_LIST_UNLOCK(buttonconfigList);

	/* replace faulty button declarations with an empty button */
	if (type != EMPTY && (sccp_strlen_zero(name) || (type != LINE && !options))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Faulty %s Button Configuration found at buttonindex: %d, name: %s, options: %s, args: %s. Substituted with  EMPTY button\n", sccp_config_buttontype2str(type), config->index, name, options, args);
		type = EMPTY;
	}

	switch (type) {
		case LINE:
		{
			char extension[SCCP_MAX_EXTENSION];
			sccp_subscription_id_t * subscriptionId = (sccp_subscription_id_t *)sccp_calloc(1, sizeof(sccp_subscription_id_t));
			if (!subscriptionId) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return SCCP_CONFIG_CHANGE_INVALIDVALUE;
 			}
			if (sccp_parseComposedId(name, 80, subscriptionId, extension)) {;
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Line Button Definition\n");
				sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: ComposedId extension: %s, subscriptionId[number:%s, name:%s, label:%s, aux:%s]\n", extension, subscriptionId->number, subscriptionId->name, subscriptionId->label, subscriptionId->aux);
				config->type = LINE;
				config->label = pbx_strdup(name);
				config->button.line.name = pbx_strdup(extension);
				
				if (!sccp_strlen_zero(subscriptionId->number) || !sccp_strlen_zero(subscriptionId->name) || !sccp_strlen_zero(subscriptionId->label) || !sccp_strlen_zero(subscriptionId->aux)) {
					config->button.line.subscriptionId = subscriptionId;
				} else {
					sccp_free(subscriptionId);
				}
			} else {
				pbx_log(LOG_WARNING, "SCCP: button definition:'%s' could not be parsed\n", name);
				sccp_free(subscriptionId);
				return SCCP_CONFIG_CHANGE_INVALIDVALUE;
			}
			if (options) {
				config->button.line.options = pbx_strdup(options);
			} else {
				config->button.line.options = NULL;
			}
			break;
		}
		case SPEEDDIAL:
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Speeddial Button Definition\n");
			config->type = SPEEDDIAL;
			config->label = pbx_strdup(name);
			config->button.speeddial.ext = pbx_strdup(options);
			if (args) {
				config->button.speeddial.hint = pbx_strdup(args);
			} else {
				config->button.speeddial.hint = NULL;
			}
			break;
		case SERVICE:
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Service Button Definition\n");
			config->type = SERVICE;
			config->label = pbx_strdup(name);
			config->button.service.url = pbx_strdup(options);
			break;
		case FEATURE:
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Feature Button Definition\n");
			sccp_log_and((DEBUGCAT_FEATURE + DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_4 "featureID: %s\n", options);
			config->type = FEATURE;
			config->label = pbx_strdup(name);
			config->button.feature.id = sccp_feature_type_str2val(options);

			config->button.feature.options = NULL;
			config->button.feature.args = NULL;
			AST_DECLARE_APP_ARGS(elems, AST_APP_ARG(option); AST_APP_ARG(arg););
			if(args && !sccp_strlen_zero(args)) {
				char * parse = pbx_strdup (args);
				AST_STANDARD_APP_ARGS(elems, parse);
				sccp_free (parse);
			}

			if(SCCP_FEATURE_PARKINGLOT == config->button.feature.id) {
				if(elems.option && !sccp_strlen_zero(elems.option)) {
					config->button.feature.options = pbx_strdup(elems.option);
				} else {
					config->button.feature.options = pbx_strdup("default");
				}
				if(elems.arg && !sccp_strlen_zero(elems.arg)) {
					config->button.feature.args = pbx_strdup(elems.arg);
				} else {
					config->button.feature.args = pbx_strdup("RetrieveSingle");
				}
			} else
#ifdef CS_DEVSTATE_FEATURE
			if(SCCP_FEATURE_DEVSTATE == config->button.feature.id) {
				// Value1:State:0:UNKNOWN|1:NOT_INUSE|2:INUSE|3:BUSY|4:INVALID|5:UNAVAILABLE|6:RINGING|7:RINGINUSE|8:ONHOLD
				// Value2:RythmCode:0-1:off,2:on,3-7:different_speeds,
				// Value3:ColorCode:0:off,1:green,2:red,3:orange
				// Value4:IconCode:0:off,1:open,2:closed,3:box
				// Value5:NextState:0:UNKNOWN|1:NOT_INUSE|2:INUSE|3:BUSY|4:INVALID|5:UNAVAILABLE|6:RINGING|7:RINGINUSE|8:ONHOLD

				// char *args_str = "devstname,[{"s":"NOT_INUSE","c":"off","r":"off","i":"open","n":"INUSE"},{"s":"INUSE","c":"orange","r":"slow","i":"closed","n":NOT_INUSE}]"
				// char *args_str = "devstname,[{"s":1,"c":0,"r":0,"i":1,"n":2},{"s":2,"c":3,"r":2,"i":2,"n":1}]"
				// char *args_str = "devstname,10012|22321"
				if(elems.option && !sccp_strlen_zero(elems.option)) {
					config->button.feature.options = pbx_strdup(elems.option);
				} else {
					config->button.feature.options = pbx_strdup(config->label);
				}
				if(elems.arg && !sccp_strlen_zero(elems.arg)) {
					config->button.feature.args = pbx_strdup(elems.arg);
				} else {
					config->button.feature.args = pbx_strdup("00001|10012|22321");
				}
			} else
#endif			
			{
				if(elems.option && !sccp_strlen_zero(elems.option)) {
					config->button.feature.options = pbx_strdup(elems.option);
				}
				if(elems.arg && !sccp_strlen_zero(elems.arg)) {
					config->button.feature.args = pbx_strdup(elems.arg);
				}
			}
			sccp_log_and((DEBUGCAT_FEATURE + DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_BUTTONTEMPLATE))(VERBOSE_PREFIX_4 "Configured feature button:%d with featureID: %s args: %s\n", config->instance,
													 config->button.feature.options, config->button.feature.args);
			break;
		case EMPTY:
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Empty Button Definition\n");
			config->type = EMPTY;
			config->label = NULL;
			break;
		default:
			sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCP: Unknown Button Type:%d\n", type);
			config->type = EMPTY;
			config->label = NULL;
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
static void sccp_config_buildLine(sccp_line_t * l, PBX_VARIABLE_TYPE * v, boolean_t isRealtime)
{
	sccp_configurationchange_t res = sccp_config_applyLineConfiguration(l, v);
	if(!l) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_config_buildLine) called without valid line ptr\n");
		return;
	}

#ifdef CS_SCCP_REALTIME
	l->realtime = isRealtime;
#endif
	// if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET && l && l->pendingDelete) {
	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s: major line changes detected, device reset required -> pendingUpdate=1\n", l->name);
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
static void sccp_config_buildDevice(sccp_device_t * d, PBX_VARIABLE_TYPE * variable, boolean_t isRealtime)
{
	PBX_VARIABLE_TYPE *v = variable;
	if(!d) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_config_buildDevice) called without valid device ptr\n");
		return;
	}

	/* apply configuration */
	sccp_configurationchange_t res = sccp_config_applyDeviceConfiguration(d, v);

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
	unsigned int  res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpGlobalConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;

	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(sccp_globals, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_GLOBAL_SEGMENT, SetEntries, FALSE);
	}
	if (res) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Update Needed (%d)\n", res);
	}
	sccp_config_set_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT, SetEntries);

	if (GLOB(keepalive) < SCCP_MIN_KEEPALIVE) {
		GLOB(keepalive) = SCCP_MIN_KEEPALIVE;
	}

	return (sccp_configurationchange_t)res;
}

/*!
 * \brief Add 'default' softkeyset
 */
static void sccp_config_add_default_softkeyset(void)
{
	// create tempory "default" variable set to create "default" softkeyset, if not defined in sccp.conf
	PBX_VARIABLE_TYPE * softkeyset_root = NULL;
	PBX_VARIABLE_TYPE * tmp = NULL;
	uint cur_elem = 0;
	const SCCPConfigOption *sccpConfigOption = sccpSoftKeyConfigOptions;
	for (cur_elem = 0; cur_elem < ARRAY_LEN(sccpSoftKeyConfigOptions); cur_elem++) {
		if (sccpConfigOption[cur_elem].defaultValue != NULL) {
			if (!softkeyset_root) {
				softkeyset_root = pbx_variable_new(sccpConfigOption[cur_elem].name, sccpConfigOption[cur_elem].defaultValue, "");
				tmp = softkeyset_root;
			} else {
				tmp->next = pbx_variable_new(sccpConfigOption[cur_elem].name, sccpConfigOption[cur_elem].defaultValue, "");
				tmp = tmp->next;
			}
		}
	}
	//pbx_log(LOG_NOTICE, "Adding 'default' softkeyset\n");
	sccp_config_softKeySet(softkeyset_root, "default");
	pbx_variables_destroy(softkeyset_root);
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
	PBX_VARIABLE_TYPE * v = NULL;

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
	if (!sccp_netsock_getPort(&GLOB(bindaddr))) {
		struct sockaddr_in *in = (struct sockaddr_in *) &GLOB(bindaddr);

		in->sin_port = ntohs(DEFAULT_SCCP_PORT);
		GLOB(bindaddr).ss_family = AF_INET;
	}
#ifdef HAVE_OPENSSL
	if(!sccp_netsock_getPort(&GLOB(secbindaddr))) {
		struct sockaddr_in * in = (struct sockaddr_in *)&GLOB(secbindaddr);

		in->sin_port = ntohs(DEFAULT_SCCP_SECURE_PORT);
		GLOB(secbindaddr).ss_family = AF_INET;
	}
#endif

	sccp_configurationchange_t res = sccp_config_applyGlobalConfiguration(v);

	/* setup bind-port */
	if (!sccp_netsock_getPort(&GLOB(bindaddr))) {
		sccp_netsock_setPort(&GLOB(bindaddr), DEFAULT_SCCP_PORT);
	}

	if (GLOB(reload_in_progress) && res == SCCP_CONFIG_NEEDDEVICERESET) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "SCCP: major changes detected in globals, reset required -> pendingUpdate=1\n");
		GLOB(pendingUpdate) = 1;
	} else {
		GLOB(pendingUpdate) = 0;
	}

	if (GLOB(regcontext)) {
                /* setup regcontext */
		char newcontexts[SCCP_MAX_CONTEXT]="";
		char oldcontexts[SCCP_MAX_CONTEXT]="";
		char * stringp = NULL;

		char * context = NULL;

		char * oldregcontext = NULL;

		sccp_copy_string(newcontexts, GLOB(regcontext), sizeof(newcontexts));
		//memcpy(newcontexts, GLOB(regcontext), sizeof(newcontexts));
		stringp = newcontexts;

		sccp_copy_string(oldcontexts, GLOB(used_context), sizeof(oldcontexts));				// Initialize copy of current regcontext for later use in removing stale contexts
		//memcpy(oldcontexts, GLOB(used_context), sizeof(oldcontexts));					// Initialize copy of current regcontext for later use in removing stale contexts
		oldregcontext = oldcontexts;

		cleanup_stale_contexts(stringp, oldregcontext);							// Let's remove any contexts that are no longer defined in regcontext

		while ((context = strsep(&stringp, "&"))) {							// Create contexts if they don't exist already
			sccp_copy_string(GLOB(used_context), context, sizeof(GLOB(used_context)));
			pbx_context_find_or_create(NULL, NULL, context, "SCCP");
		}
	}
	if (GLOB(externhost)) {
		sccp_netsock_flush_externhost();
	}
	
	return TRUE;
}


/*!
 * \brief Cleanup Stale Contexts (regcontext)
 * \param new New Context as Character
 * \param old Old Context as Character
 */
void cleanup_stale_contexts(char *new_contexts, char *old_contexts)
{
	char * oldcontext = NULL;

	char * newcontext = NULL;

	char * stalecontext = NULL;

	char * stringp = NULL;

	char newlist[SCCP_MAX_CONTEXT];

	while ((oldcontext = strsep(&old_contexts, "&"))) {
		stalecontext = NULL;
		sccp_copy_string(newlist, new_contexts, sizeof(newlist));
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
boolean_t sccp_config_readDevicesLines(sccp_readingtype_t readingtype)
{
	// struct ast_config *cfg = NULL;

	char *cat = NULL;
	PBX_VARIABLE_TYPE *v = NULL;
	uint8_t device_count = 0;
	uint8_t line_count = 0;
	sccp_device_t *d = NULL;

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Loading Devices and Lines from config\n");

	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Checking Reading Type:%s (%d)\n", readingtype == 0 ? "Module load" : "Reload", readingtype);
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
		return FALSE;
	}

	while ((cat = pbx_category_browse(GLOB(cfg), cat))) {
		const char * utype = NULL;

		if (!strcasecmp(cat, "general")) {
			continue;
		}
		utype = pbx_variable_retrieve(GLOB(cfg), cat, "type");
		sccp_log_and((DEBUGCAT_CONFIG + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "SCCP: (sccp_config_readDevicesLines) Reading Section Of Type %s\n", utype);

		if (!utype) {
			pbx_log(LOG_WARNING, "Section '%s' is missing a type parameter\n", cat);
			continue;
		} else if (!strcasecmp(utype, "device")) {
			// check minimum requirements for a device
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Parsing device [%s]\n", cat);
			v = ast_variable_browse(GLOB(cfg), cat);

			// Try to find out if we have the device already on file.
			// However, do not look into realtime, since
			// we might have been asked to create a device for realtime addition,
			// thus causing an infinite loop / recursion.
			AUTO_RELEASE(sccp_device_t, device, sccp_device_find_byid(cat, FALSE));
			sccp_nat_t nat = SCCP_NAT_AUTO;

			/* create new device with default values */
			if (!device) {
				device = sccp_device_create(cat) /*ref_replace*/;
				if(!device) {
					return FALSE;
				}
				// sccp_copy_string(d->id, cat, sizeof(d->id));         /* set device name */
				sccp_device_addToGlobals(device);
				device_count++;
			} else {
				if (device->pendingDelete) {
					nat = device->nat;
					device->pendingDelete = 0;
				}
			}
			sccp_config_buildDevice(device, v, FALSE);
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found device %d: %s\n", device_count, cat);
			/* load saved settings from ast db */
			// sccp_config_restoreDeviceFeatureStatus(device);

			/* restore current nat status, if device does not get restarted */
			if (0 == device->pendingDelete && sccp_device_getRegistrationState(device) != SKINNY_DEVICE_RS_NONE) {
				if (SCCP_NAT_AUTO == device->nat && (SCCP_NAT_AUTO == nat || SCCP_NAT_AUTO_OFF == nat || SCCP_NAT_AUTO_ON == nat)) {
					device->nat = nat;
				}
			}
		} else if (!strcasecmp(utype, "line")) {
			/* check minimum requirements for a line */
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Parsing line [%s]\n", cat);

			if ((!(!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "label"))) && (!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "cid_name"))) && (!sccp_strlen_zero(pbx_variable_retrieve(GLOB(cfg), cat, "cid_num"))))) {
				pbx_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
				continue;
			}
			line_count++;

			v = ast_variable_browse(GLOB(cfg), cat);
			AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byname(cat, FALSE));

			/* check if we have this line already */
			if (l) {
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "found line %d: %s, do update\n", line_count, cat);
				sccp_config_buildLine(l, v, FALSE);
			} else if((l = sccp_line_create(cat)) /*ref_replace*/) {
				sccp_config_buildLine(l, v, FALSE);
				sccp_line_addToGlobals(l);						/* may find another line instance create by another thread, in that case the newly created line is going to be dropped when l is released */
			} else {
				return FALSE;
			}
		} else if (!strcasecmp(utype, "softkeyset")) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "parsing softkey [%s]\n", cat);
			if (sccp_strcaseequals(cat, "default")) {
				pbx_log(LOG_WARNING, "SCCP: (sccp_config_readDevicesLines) The 'default' softkeyset cannot be overriden, please use another name\n");
			} else {
				v = ast_variable_browse(GLOB(cfg), cat);
				sccp_config_softKeySet(v, cat);
			}
		} else {
			pbx_log(LOG_WARNING, "SCCP: (sccp_config_readDevicesLines) UNKNOWN SECTION / UTYPE, type: %s\n", utype);
		}
	}
	sccp_config_add_default_softkeyset();

#ifdef CS_SCCP_REALTIME
	/* reload realtime lines */
	sccp_configurationchange_t res = SCCP_CONFIG_NOUPDATENEEDED;
	PBX_VARIABLE_TYPE *rv = NULL;
	
	sccp_line_t *l = NULL;
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(l));
		if (line) {
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
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	/* finished realtime line reload */

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));
		if (device) {
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
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
#endif

	if (GLOB(reload_in_progress) && GLOB(pendingUpdate)) {
		sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "Global param changed needing restart ->  Restart all device\n");

		SCCP_RWLIST_RDLOCK(&GLOB(devices));
		SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
			if (d->realtime) {
				d->pendingDelete = 1;
			} else if (!d->pendingDelete && !d->pendingUpdate) {
				d->pendingUpdate = 1;
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
	return TRUE;
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
sccp_configurationchange_t sccp_config_applyLineConfiguration(linePtr l, PBX_VARIABLE_TYPE * v)
{
	unsigned int res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpLineConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;
	if(!l) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_config_applyLineConfiguration) called without valid line ptr\n");
		return SCCP_CONFIG_ERROR;
	}

	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(l, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_LINE_SEGMENT, SetEntries, FALSE);
	}

	l->preferences_set_on_line_level = (l->preferences.audio[0] != SKINNY_CODEC_NONE) ? TRUE: FALSE;	

	sccp_config_set_defaults(l, SCCP_CONFIG_LINE_SEGMENT, SetEntries);

	if (sccp_strlen_zero(l->id)) {
		snprintf(l->id, sizeof(l->id), "%04d", SCCP_LIST_GETSIZE(&GLOB(lines)));
	}

	return (sccp_configurationchange_t)res;
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
sccp_configurationchange_t sccp_config_applyDeviceConfiguration(devicePtr d, PBX_VARIABLE_TYPE * v)
{
	unsigned int res = SCCP_CONFIG_NOUPDATENEEDED;
	boolean_t SetEntries[ARRAY_LEN(sccpDeviceConfigOptions)] = { FALSE };
	PBX_VARIABLE_TYPE *cat_root = v;
	if(!d) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_config_applyDeviceConfiguration) called without valid device ptr\n");
		return SCCP_CONFIG_ERROR;
	}

	if (d->pendingDelete) {
		sccp_dev_clean_restart(d, FALSE);
	}
	for (; v; v = v->next) {
		res |= sccp_config_object_setValue(d, cat_root, v->name, v->value, v->lineno, SCCP_CONFIG_DEVICE_SEGMENT, SetEntries, FALSE);
	}

	sccp_config_set_defaults(d, SCCP_CONFIG_DEVICE_SEGMENT, SetEntries);

	if (d->keepalive < SCCP_MIN_KEEPALIVE) {
		d->keepalive = SCCP_MIN_KEEPALIVE;
	}
	return (sccp_configurationchange_t)res;
}

/*!
 * \brief Find the Correct Config File
 * \return Asterisk Config Object as ast_config
 */
sccp_config_file_status_t sccp_config_getConfig(boolean_t force, const char * const filename)
{
	// struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS & CONFIG_FLAG_FILEUNCHANGED };
	sccp_config_file_status_t res = 0;
	char * newfilename = "sccp.conf";
	struct ast_flags config_flags = { CONFIG_FLAG_FILEUNCHANGED };
	if (force) {
		if (GLOB(cfg)) {
			pbx_config_destroy(GLOB(cfg));
			GLOB(cfg) = NULL;
		}
		pbx_clear_flag(&config_flags, CONFIG_FLAG_FILEUNCHANGED);
	}

	if(filename && !sccp_strlen_zero(filename)) {
		newfilename = (char *)filename;
	} else if(GLOB(config_file_name) && !sccp_strlen_zero(GLOB(config_file_name))) {
		newfilename = GLOB(config_file_name);
		sccp_free(GLOB(config_file_name));
	}
	GLOB(cfg) = pbx_config_load(newfilename, "chan_sccp", config_flags);
	if (GLOB(cfg) == CONFIG_STATUS_FILEMISSING) {
		pbx_log(LOG_ERROR, "Config file '%s' not found, aborting (re)load.\n", newfilename);
		GLOB(cfg) = NULL;
		res = CONFIG_STATUS_FILE_NOT_FOUND;
		goto FUNC_EXIT;
	} else if (GLOB(cfg) == CONFIG_STATUS_FILEINVALID) {
		pbx_log(LOG_ERROR, "Config file '%s' specified is not a valid config file, aborting (re)load.\n", newfilename);
		GLOB(cfg) = NULL;
		res = CONFIG_STATUS_FILE_INVALID;
		goto FUNC_EXIT;
	} else if (GLOB(cfg) == CONFIG_STATUS_FILEUNCHANGED) {
		// ugly solution, but we always need to have a valid config file loaded.
		pbx_clear_flag(&config_flags, CONFIG_FLAG_FILEUNCHANGED);
		GLOB(cfg) = pbx_config_load(newfilename, "chan_sccp", config_flags);
		if (!force) {
			pbx_log(LOG_NOTICE, "Config file '%s' has not changed, aborting (re)load.\n", newfilename);
			res = CONFIG_STATUS_FILE_NOT_CHANGED;
			goto FUNC_EXIT;
		} else {
			pbx_log(LOG_NOTICE, "Config file '%s' has not changed, forcing reload.\n", newfilename);
		}
	}
	if (GLOB(cfg)) {
		if (ast_variable_browse(GLOB(cfg), "devices")) {						/* Warn user when old entries exist in sccp.conf */
			pbx_log(LOG_ERROR,
				"\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check "
				"http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n",
				newfilename);
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
	pbx_log(LOG_NOTICE, "Config file '%s' loaded.\n", newfilename);
	res = CONFIG_STATUS_FILE_OK;
FUNC_EXIT:
	if (GLOB(config_file_name)) {sccp_free(GLOB(config_file_name));}
	GLOB(config_file_name) = pbx_strdup(newfilename);
	return res;
}

/*!
 * \brief Soft Key Str to Label Mapping
 */
static const struct softkeyConfigurationTemplate {
	const char configVar[16];										/*!< Config Variable as Character */
	const int softkey;											/*!< Softkey as Int */
} softKeyTemplate[] = {
	/* *INDENT-OFF* */
	{"redial", 			SKINNY_LBL_REDIAL},
	{"newcall", 			SKINNY_LBL_NEWCALL},
	{"cfwdall", 			SKINNY_LBL_CFWDALL},
	{"cfwdbusy", 			SKINNY_LBL_CFWDBUSY},
	{"cfwdnoanswer",		SKINNY_LBL_CFWDNOANSWER},
	{"dnd", 			SKINNY_LBL_DND},
	{"hold", 			SKINNY_LBL_HOLD},
	{"endcall", 			SKINNY_LBL_ENDCALL},
	{"idivert", 			SKINNY_LBL_IDIVERT},
	{"resume", 			SKINNY_LBL_RESUME},
	{"newcall", 			SKINNY_LBL_NEWCALL},
	{"transfer", 			SKINNY_LBL_TRANSFER},
	{"answer", 			SKINNY_LBL_ANSWER},
	{"transvm", 			SKINNY_LBL_TRNSFVM},
	{"private", 			SKINNY_LBL_PRIVATE},
	{"meetme", 			SKINNY_LBL_MEETME},
	{"barge", 			SKINNY_LBL_BARGE},
	{"back", 			SKINNY_LBL_BACKSPACE},
	{"intrcpt", 			SKINNY_LBL_INTRCPT},
	{"monitor", 			SKINNY_LBL_MONITOR},  
	{"dial", 			SKINNY_LBL_DIAL},
#ifndef CS_ADV_FEATURES
	{"callback",			SKINNY_LBL_CALLBACK},
	{"trnsfvm",			SKINNY_LBL_TRNSFVM},
	{"cbarge", 			SKINNY_LBL_CBARGE},
#endif
#ifdef CS_SCCP_VIDEO
	{"vidmode", 			SKINNY_LBL_VIDEO_MODE},
#else
	{"vidmode", 			-1},
#endif
#ifdef CS_SCCP_PICKUP
	{"pickup", 			SKINNY_LBL_PICKUP},
	{"gpickup", 			SKINNY_LBL_GPICKUP},
#else
	{"pickup", 			-1},
	{"gpickup", 			-1},
#endif	
#ifdef CS_SCCP_PARK
	{"park", 			SKINNY_LBL_PARK},
#else
	{"park", 			-1},
#endif
#ifdef CS_SCCP_DIRTRFR	
	{"select", 			SKINNY_LBL_SELECT},
	{"dirtrfr", 			SKINNY_LBL_DIRTRFR},
#else
	{"select", 			-1},
	{"dirtrfr", 			-1},
#endif	
#ifdef CS_SCCP_CONFERENCE
	{"conf", 			SKINNY_LBL_CONFRN},
	{"confrn",			SKINNY_LBL_CONFRN},
	{"join", 			SKINNY_LBL_JOIN},
	{"conflist", 			SKINNY_LBL_CONFLIST},
#else
	{"conf", 			-1},
	{"confrn",			-1},
	{"join",			-1},
	{"conflist", 			-1},
#endif	
	{"empty", 			SKINNY_LBL_EMPTY},
//	{"info", 			SKINNY_LBL_INFO},
	/* *INDENT-ON* */

};

/*!
 * \brief Get softkey label as int
 * \param key configuration value
 * \return SoftKey label as int of SKINNY_LBL_*. returns an empty button if nothing matched
 */
static int sccp_config_getSoftkeyLbl(char *key)
{
	size_t i = 0;
	for (i = 0; i < ARRAY_LEN(softKeyTemplate); i++) {
		if (sccp_strcaseequals(softKeyTemplate[i].configVar, key)) {
			return softKeyTemplate[i].softkey;
		}
	}
	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "softkeybutton: %s not defined\n", key);
	return SKINNY_LBL_EMPTY;
}

/*!
 * \brief Read a single SoftKeyMode (configuration values)
 * \param softkeyset SoftKeySet
 * \param data configuration values
 * \return number of parsed softkeys
 */
static uint8_t sccp_config_readSoftKeySet(uint8_t * softkeyset, const char * data)
{
	if (!data) {
		return 0;
	}
	int i = 0;

	int j = 0;
	int softkey = 0;

	char * labels = pbx_strdupa(data);
	char * labelrest = NULL;
	char delims[] = ",";

	char * label = strtok_r(labels, delims, &labelrest);
	while(label) {
		label = pbx_strip(label);
		if ((softkey = sccp_config_getSoftkeyLbl(label)) != -1 && (i + 1) < StationMaxSoftKeySetDefinition) {
			softkeyset[i++] = softkey;
		}
		label = strtok_r(NULL, delims, &labelrest);
	}
	/*! \todo we used calloc to create this structure, so setting EMPTY should not be required here */
	for (j = i; j < StationMaxSoftKeySetDefinition; j++) {
		softkeyset[j] = SKINNY_LBL_EMPTY;
	}
	return i;
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
	int keySetSize = 0;
	sccp_softKeySetConfiguration_t *softKeySetConfiguration = NULL;
	skinny_keymode_t keyMode = SKINNY_KEYMODE_SENTINEL;

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
		softKeySetConfiguration = (sccp_softKeySetConfiguration_t *)sccp_calloc(1, sizeof(sccp_softKeySetConfiguration_t));
		memset(softKeySetConfiguration, 0, sizeof(sccp_softKeySetConfiguration_t));

		sccp_copy_string(softKeySetConfiguration->name, name, sizeof(sccp_softKeySetConfiguration_t));
		softKeySetConfiguration->numberOfSoftKeySets = 0;
		softKeySetConfiguration->softkeyCbMap = NULL;							// defaults to static softkeyMapCb

		/* add new softkexSet to list */
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_INSERT_HEAD(&softKeySetConfig, softKeySetConfiguration, list);
		SCCP_LIST_UNLOCK(&softKeySetConfig);
	}

	while (variable) {
		keyMode = SKINNY_KEYMODE_SENTINEL;
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "softkeyset: %s = %s\n", variable->name, variable->value);
		if (sccp_strcaseequals(variable->name, "uriaction")) {
			sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: UriAction softkey (%s) found\n", variable->value);
			if (!softKeySetConfiguration->softkeyCbMap) {
				softKeySetConfiguration->softkeyCbMap = sccp_softkeyMap_copyStaticallyMapped();
			}

			char * uriactionstr = pbx_strdup (variable->value);
			char *event = strsep(&uriactionstr, ",");
			if (event && !sccp_strlen_zero(uriactionstr)) {
				sccp_softkeyMap_replaceCallBackByUriAction(softKeySetConfiguration->softkeyCbMap, labelstr2int(event), uriactionstr);
			} else {
				sccp_log(DEBUGCAT_CONFIG) (VERBOSE_PREFIX_3 "SCCP: UriAction softkey (%s) not found, or no uris (%s) specified\n", event, uriactionstr);
			}
			sccp_free (uriactionstr);
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
		} else if (sccp_strcaseequals(variable->name, "inusehint") || sccp_strcaseequals(variable->name, "onhint")) {
			keyMode = KEYMODE_INUSEHINT;
		} else if (sccp_strcaseequals(variable->name, "onhookstealable") || sccp_strcaseequals(variable->name, "onstealable")) {
			keyMode = KEYMODE_ONHOOKSTEALABLE;
		} else if (sccp_strcaseequals(variable->name, "holdconf")) {
			keyMode = KEYMODE_HOLDCONF;
		}

		if (keyMode != SKINNY_KEYMODE_SENTINEL) {
			if (softKeySetConfiguration->numberOfSoftKeySets < (keyMode + 1)) {
				softKeySetConfiguration->numberOfSoftKeySets = keyMode + 1;
			}

			/* cleanup old value */
			if (softKeySetConfiguration->modes[keyMode].ptr) {
				//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "KeyMode(%d) Ptr already defined in Softkeyset: %s. Freeing...\n", keyMode, name);
				sccp_free(softKeySetConfiguration->modes[keyMode].ptr);
			}

			/* add new value */
			uint8_t *softkeyset = (uint8_t *)sccp_calloc(StationMaxSoftKeySetDefinition, sizeof(uint8_t));
			keySetSize = sccp_config_readSoftKeySet(softkeyset, variable->value);
			//sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "Adding KeyMode(%d), with Size(%d), prt(%p) to Softkeyset: %s\n", keyMode, keySetSize, softkeyset, name);
			if (keySetSize > 0) {
				softKeySetConfiguration->modes[keyMode].id = keyMode;
				softKeySetConfiguration->modes[keyMode].ptr = softkeyset;
				softKeySetConfiguration->modes[keyMode].count = keySetSize;
			} else {
				softKeySetConfiguration->modes[keyMode].id = keyMode;
				softKeySetConfiguration->modes[keyMode].ptr = NULL;
				softKeySetConfiguration->modes[keyMode].count = 0;
				sccp_free(softkeyset);
			}
		}

		variable = variable->next;
	}

}


/* generate json output from now on */
int sccp_manager_config_metadata(struct mansession *s, const struct message *m)
{
	const SCCPConfigSegment *sccpConfigSegment = NULL;
	int total = 0;
	uint i = 0;
	const char *id = astman_get_header(m, "ActionID");
	const char *req_segment = astman_get_header(m, "Segment");
	const char *req_resultformat = astman_get_header(m, "ResultFormat");
	uint comma = 0;

	if (sccp_strlen_zero(req_segment)) {										// return all segments
		int sccp_config_revision = 0;

		sscanf(SCCP_CONFIG_REVISION, "$" "Revision: %i" "$", &sccp_config_revision);

		if (sccp_strcaseequals(req_resultformat, "list")) {
			astman_send_listack(s, m, "SCCPConfigMetaData Follows", "Start");
			astman_append(s, "Event: SCCPConfigMetaData\r\n");
		} else if (sccp_strcaseequals(req_resultformat, "command")) {
			astman_append(s, "Response: Follows\r\n");
			astman_append(s, "Priviledge: Command\r\n");
		} else {
			astman_append(s, "Response: Success\r\n");
		}
		if (!ast_strlen_zero(id)) {
			astman_append(s, "ActionID: %s\r\n", id);
		}

		astman_append(s, "JSON: {");
		astman_append(s, "\"Name\":\"Chan-sccp-b\",");
		astman_append(s, "\"Version\":\"%s\",", SCCP_VERSION);
#if defined(VCS_BRANCH) && defined(VCS_NUM) && defined(VCS_TAG) && defined(VCS_TAG) && defined(VCS_TYPE)
		astman_append(s, "\"Branch\":\"%s\",", VCS_BRANCH);
		astman_append(s, "\"RevisionHash\":\"%s\",", VCS_SHORT_HASH);
		astman_append(s, "\"RevisionNum\":\"%d\",", VCS_NUM);
		astman_append(s, "\"Tag\":\"%s\",", VCS_TAG);
		astman_append(s, "\"VersioningType\":\"%s\",", VCS_TYPE);
#else
		astman_append(s, "\"Branch\":\"%s\",", SCCP_BRANCH);
		astman_append(s, "\"RevisionHash\":\"%s\",", SCCP_REVISION);
		astman_append(s, "\"RevisionNum\":\"%d\",", 0);
		astman_append(s, "\"Tag\":\"%s\",", "");
		astman_append(s, "\"VersioningType\":\"%s\",", "archive");
#endif
		astman_append(s, "\"ConfigRevision\":\"%d\",", sccp_config_revision);
		char *conf_enabled_array[] = {
#ifdef CS_SCCP_PARK
			"park",
#endif
#ifdef CS_SCCP_PICKUP
			"pickup",
#endif 
#ifdef CS_SCCP_REALTIME
			"realtime",
#endif 
#ifdef CS_SCCP_VIDEO
			"video",
#endif 
#ifdef CS_SCCP_CONFERENCE
			"conferenence",
#endif 
#ifdef CS_SCCP_DIRTRFR
			"dirtrfr",
#endif 
#ifdef CS_SCCP_FEATURE_MONITOR
			"feature_monitor",
#endif
#ifdef CS_SCCP_FUNCTIONS
			"functions",
#endif
#ifdef CS_MANAGER_EVENTS
			"manager_events",
#endif
#ifdef CS_DEVICESTATE
			"devicestate",
#endif
#ifdef CS_DEVSTATE_FEATURE
			"devstate_feature",
#endif
#ifdef CS_DYNAMIC_SPEEDDIAL
			"dynamic_speeddial",
#endif
#ifdef CS_DYNAMIC_SPEEDDIAL_CID
			"dynamic_speeddial_cid",
#endif
#ifdef CS_EXPERIMENTAL
			"experimental",
#endif
#ifdef DEBUG
			"debug",
#endif
		};
		comma = 0;
		astman_append(s, "\"ConfigureEnabled\": [");
		for (i = 0; i < ARRAY_LEN(conf_enabled_array); i++) {
			astman_append(s, "%s\"%s\"", comma ? "," : "",conf_enabled_array[i]);
			comma = 1;
		}
		astman_append(s, "],");
		
		comma = 0;
		astman_append(s, "\"Segments\":[");
		for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
			astman_append(s, "%s", comma ? "," : "");
			astman_append(s, "\"%s\"", sccpConfigSegments[i].name);
			comma = 1;
		}
		astman_append(s, "]}\r\n");
		total++;
		if (sccp_strcaseequals(req_resultformat, "list")) {
			astman_append(s,
				"\r\nEvent: SCCPConfigMetaDataComplete\r\n"
				"EventList: Complete\r\n"
				"ListItems: %d\r\n\r\n", total);
		} else if (sccp_strcaseequals(req_resultformat, "command")) {
			astman_append(s, "--END COMMAND--\r\n");
		}
		astman_append(s, "\r\n");
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

				if (sccp_strcaseequals(req_resultformat, "list")) {
					astman_send_listack(s, m, "SCCPConfigMetaData Follows", "Start");
					astman_append(s, "Event: SCCPConfigMetaData\r\n");
				} else if (sccp_strcaseequals(req_resultformat, "command")) {
					astman_append(s, "Response: Follows\r\n");
				} else {
					astman_append(s, "Response: Success\r\n");
				}
				if (!ast_strlen_zero(id)) {
					astman_append(s, "ActionID: %s\r\n", id);
				}
				astman_append(s, "JSON: {");
				astman_append(s, "\"Segment\":\"%s\",", sccpConfigSegment->name);
				astman_append(s, "\"Options\":[");
				uint8_t cur_elem = 0;
				comma = 0;

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
									char * all_entries = pbx_strdup (config[cur_elem].all_entries());
									char *possible_entry = "";

									int subcomma = 0;
									astman_append(s, "\"Possible Values\": [");
									while (all_entries && (possible_entry = strsep(&all_entries, ","))) {
										astman_append(s, "%s\"%s\"", subcomma ? "," : "", possible_entry);
										subcomma = 1;
									}
									astman_append(s, "]");
									sccp_free (all_entries);
									break;
							}
							astman_append(s, ",");

							if ((config[cur_elem].flags & (SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_OBSOLETE | SCCP_CONFIG_FLAG_MULTI_ENTRY)) > 0 || (config[cur_elem].change & SCCP_CONFIG_NEEDDEVICERESET) == SCCP_CONFIG_NEEDDEVICERESET) {
								astman_append(s, "\"Flags\":[");
								{
									int comma1 = 0;

									if ((config[cur_elem].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) {
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
								astman_append(s, "],");
							}

							astman_append(s, "\"DefaultValue\":\"%s\"", config[cur_elem].defaultValue);

							if (!sccp_strlen_zero(config[cur_elem].description)) {
								char * description = pbx_strdup (config[cur_elem].description);
								char *description_part = "";
								int comma2 = 0;

								astman_append(s, ",\"Description\": [");
								while(description && (description_part = strsep(&description, "\n")) && !sccp_strlen_zero(description_part)) {
									astman_append(s, "%s\"%s\"", comma2++ ? "," : "", description_part);
								}
								astman_append(s, "]");
								sccp_free (description);
							}
						}
						astman_append(s, "}");
						comma = 1;
					}
				}
				astman_append(s, "]}\r\n");
				total++;
				if (sccp_strcaseequals(req_resultformat, "list")) {
					astman_append(s,
						"\r\nEvent: SCCPConfigMetaDataComplete\r\n"
						"EventList: Complete\r\n"
						"ListItems: %d\r\n\r\n", total);
				} else if (sccp_strcaseequals(req_resultformat, "command")) {
					astman_append(s, "--END COMMAND--\r\n"
							 "DataType: JSON\r\n"
							 "Priviledge: Command\r\n"
					);
				}
				astman_append(s, "\r\n");
			}
		}
	}
	return 0;
}

static int _config_generate_wiki(char * filename)
{
	const SCCPConfigSegment * sccpConfigSegment = NULL;
	const SCCPConfigOption * config = NULL;
	long unsigned int sccp_option = 0;
	long unsigned int segment = 0;
	char * description = "";
	char * description_part = "";
	char fn[PATH_MAX];

	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, filename);
	pbx_log(LOG_NOTICE, "Creating new wiki file '%s'\n", fn);

	FILE * f = NULL;

	if(!(f = fopen(fn, "w"))) {
		pbx_log(LOG_ERROR, "Error creating new config file \n");
		return 1;
	}

	char date[256] = "";
	time_t t = 0;

	time(&t);
	sccp_copy_string(date, ctime(&t), sizeof(date));

	fprintf(f, "*sccp.conf options*\n\n");
	for(segment = SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {
		sccpConfigSegment = sccp_find_segment((sccp_config_segment_t)segment);
		fprintf(f, "\n**[%s] section**\n\n", sccpConfigSegment->name);
		fprintf(f, "<table>\n");
		fprintf(f, "<tr><td><b>parameter</b></td><td><b>default</b></td><td><b>format</b></td><td><b>required</b></td><td><b>status</b></td></tr>\n");
		fprintf(f, "<tr><td></td><td colspan='4'><b>description</b></td></tr>\n");
		config = sccpConfigSegment->config;
		for(sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
			// if ((config[sccp_option].flags & (SCCP_CONFIG_FLAG_IGNORE | SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_OBSOLETE)) == 0) {
			sccp_log((DEBUGCAT_CONFIG))(VERBOSE_PREFIX_2 "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);
			if(!sccp_strlen_zero(config[sccp_option].name)) {
				char delims[] = "|";
				char * option_name_tokens = pbx_strdup (config[sccp_option].name);
				char * option_value_tokens = NULL;
				if(!sccp_strlen_zero(config[sccp_option].defaultValue)) {
					option_value_tokens = pbx_strdup (config[sccp_option].defaultValue);
				} else {
					option_value_tokens = pbx_strdup ("\"\"");
				}
				char * option_name_tokens_saveptr = NULL;
				char * option_value_tokens_saveptr = NULL;
				char * option_name = strtok_r(option_name_tokens, delims, &option_name_tokens_saveptr);
				char * option_value = strtok_r(option_value_tokens, delims, &option_value_tokens_saveptr);
				while(option_name != NULL) {
					fprintf(f, "<tr class=option_row id='%s'>\n", option_name);
					fprintf(f, "<td class='name'>%s</td><td class='default_value'>%s</td>\n", option_name, option_value);
					option_name = strtok_r(NULL, delims, &option_name_tokens_saveptr);
					option_value = strtok_r(NULL, delims, &option_value_tokens_saveptr);
					switch(config[sccp_option].type) {
						case SCCP_CONFIG_DATATYPE_STRING:
							fprintf(f, "<td class='format'>max length:%d</td>\n", (int)config[sccp_option].size - 1);
							break;
						case SCCP_CONFIG_DATATYPE_ENUM: {
							char * all_entries = pbx_strdupa(config[sccp_option].all_entries());
							char * possible_entry = "";
							int subcomma = 0;

							fprintf(f, "<td class='format'><small>potential values:[");
							while(all_entries && (possible_entry = strsep(&all_entries, ","))) {
								fprintf(f, "%s%s", subcomma ? ", " : "", possible_entry);
								subcomma = 1;
							}
							fprintf(f, "]</small></td>\n");
						} break;
						default:
							fprintf(f, "<td class='format'>-</td>\n");
							break;
					}
					fprintf(f, "<td class='required'>%s</td>\n", (config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED ? "yes" : "-");
					fprintf(f, "<td class='status'>%s</td> ",
						(config[sccp_option].flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED
						    ? "deprecated"
						    : (config[sccp_option].flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE ? "obsolete" : "-");
					fprintf(f, "</tr>\n");
					if(!sccp_strlen_zero(config[sccp_option].description)) {
						fprintf(f, "<tr class='descr_row'><td></td>\n");
						fprintf(f, "<td class='description' id='%s' colspan='4'><small>", config[sccp_option].name);
						description = pbx_strdup (config[sccp_option].description);
						while((description_part = strsep(&description, "\n"))) {
							if(!sccp_strlen_zero(description_part)) {
								fprintf(f, "%s.<br>", description_part);
							}
						}
						if(description_part) {
							sccp_free(description_part);
						}
						fprintf(f, "</small></td>\n");
						sccp_free (description);
					}
				}
				fprintf(f, "</tr>\n");
				sccp_free (option_name_tokens);
				sccp_free (option_value_tokens);
			} else {
				pbx_log(LOG_ERROR, "Error creating new variable structure for %s='%s'\n", config[sccp_option].name, config[sccp_option].defaultValue);
				fclose(f);
				return 2;
			}
		}
		fprintf(f, "</table><br>\n");
	}
	fclose(f);
	pbx_log(LOG_NOTICE, "Created new config file '%s'\n", fn);

	return 0;
};

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
	if(configType == 3) {
		return _config_generate_wiki(filename);
	}
	const SCCPConfigSegment * sccpConfigSegment = NULL;
	const SCCPConfigOption * config = NULL;
	long unsigned int sccp_option = 0;
	long unsigned int segment = 0;
	char *description = "";
	char *description_part = "";
	char name_and_value[100] = "";
	char size_str[15] = "";
	int linelen = 0;
	struct ast_str * extra_info = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE * 3);

	char fn[PATH_MAX];

	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, filename);
	pbx_log(LOG_NOTICE, "Creating new config file '%s'\n", fn);

	FILE * f = NULL;

	if (!(f = fopen(fn, "w"))) {
		pbx_log(LOG_ERROR, "Error creating new config file \n");
		return 1;
	}

	char date[256] = "";
	time_t t = 0;

	time(&t);
	sccp_copy_string(date, ctime(&t), sizeof(date));

	fprintf(f, ";!\n");
	fprintf(f, ";! Automatically generated configuration file\n");
	fprintf(f, ";! Filename: %s (%s)\n", filename, fn);
	fprintf(f, ";! Generator: sccp config generate\n");
	fprintf(f, ";! Creation Date: %s", date);
	fprintf(f, ";!\n");
	fprintf(f, "\n");
	fprintf(f, ";!\n");
	fprintf(f, ";! This file is only provided to show a possible parameters and their defaults.\n");
	fprintf(f, ";! Please do not use this file as a starting point for your sccp.conf file !\n");
	fprintf(f, ";! Parameters that have blanks or empty strings are just there as a placeholder, not to show a valid value.\n");
	fprintf(f, ";!\n");
	fprintf(f, "\n");

	for (segment = SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {
		sccpConfigSegment = sccp_find_segment((sccp_config_segment_t)segment);
		if (configType == 0 && (segment == SCCP_CONFIG_DEVICE_SEGMENT || segment == SCCP_CONFIG_LINE_SEGMENT)) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding [%s] template section\n", sccpConfigSegment->name);
			fprintf(f, "\n;\n; %s section\n;\n[default_%s](!)\n", sccpConfigSegment->name, sccpConfigSegment->name);
		} else if (configType == 0 && segment == SCCP_CONFIG_SOFTKEY_SEGMENT) {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding [%s] section\n", sccpConfigSegment->name);
			fprintf(f, "\n;\n; %s section\n;\n;[mysoftkeyset]\n", sccpConfigSegment->name);
		} else {
			sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding [%s] section\n", sccpConfigSegment->name);
			fprintf(f, "\n;\n; %s section\n;\n[%s]\n", sccpConfigSegment->name, sccpConfigSegment->name);
		}

		config = sccpConfigSegment->config;
		for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
			//if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
			if ((config[sccp_option].flags & (SCCP_CONFIG_FLAG_IGNORE | SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_OBSOLETE)) == 0) {
				sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);

				if (!sccp_strlen_zero(config[sccp_option].name)) {
					if (!sccp_strlen_zero(config[sccp_option].defaultValue)			// non empty
					    || (configType != 2 && ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != SCCP_CONFIG_FLAG_REQUIRED && sccp_strlen_zero(config[sccp_option].defaultValue)))	// empty but required
					    ) {
					    	if (strstr(config[sccp_option].name, "|")) {
					    		char delims[] = "|";
							char * option_name_tokens = pbx_strdup (config[sccp_option].name);
							char * option_value_tokens = NULL;
							if(!sccp_strlen_zero(config[sccp_option].defaultValue)) {
								option_value_tokens = pbx_strdup (config[sccp_option].defaultValue);
							} else {
								option_value_tokens = pbx_strdup ("\"\"");
							}
							char * option_name_tokens_saveptr = NULL;
							char * option_value_tokens_saveptr = NULL;
							char * option_name = strtok_r(option_name_tokens, delims, &option_name_tokens_saveptr);
							char *option_value = strtok_r(option_value_tokens, delims, &option_value_tokens_saveptr);
					    		while (option_name != NULL) {
								snprintf(name_and_value, sizeof(name_and_value), "%s = %s", option_name, option_value ? option_value : "\"\"");
								fprintf(f, "%s", name_and_value);
								option_name = strtok_r(NULL, delims, &option_name_tokens_saveptr);
								option_value = strtok_r(NULL, delims, &option_value_tokens_saveptr);
								if (option_name) {
									fprintf(f, "\n");
								}
							}
							sccp_free (option_name_tokens);
							sccp_free (option_value_tokens);
						} else {
							snprintf(name_and_value, sizeof(name_and_value), "%s%s = %s", !sccp_strlen_zero(config[sccp_option].defaultValue) ? ";" : "", config[sccp_option].name, sccp_strlen_zero(config[sccp_option].defaultValue) ? "\"\"" : config[sccp_option].defaultValue);
							fprintf(f, "%s", name_and_value);
					    	}
						linelen = (int) strlen(name_and_value);
						switch (config[sccp_option].type) {
							case SCCP_CONFIG_DATATYPE_STRING:
								snprintf(size_str, sizeof(size_str), "(SIZE: %d) ", (int) config[sccp_option].size - 1);
								break;
							case SCCP_CONFIG_DATATYPE_ENUM:
								{
								char * all_entries = pbx_strdup (config[sccp_option].all_entries());
								char * possible_entry = "";
								int subcomma = 0;

								pbx_str_append (&extra_info, 0, "(POSSIBLE VALUES: [");
								while (all_entries && (possible_entry = strsep (&all_entries, ","))) {
									pbx_str_append (&extra_info, 0, "%s\"%s\"", subcomma ? "," : "", possible_entry);
									subcomma = 1;
									}
									pbx_str_append(&extra_info, 0, "])");
									sccp_free (all_entries);
								}
								size_str[0] = '\0';
								break;
							default:
								size_str[0] = '\0';
								break;
						}
						fprintf(f, "%*.s ; %s%s%s%s%s", 81 - linelen, " ",
							((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) ? "(REQUIRED) " : "", 
							((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "(MULTI-ENTRY) " : "", 
							((config[sccp_option].flags & SCCP_CONFIG_FLAG_DEPRECATED) == SCCP_CONFIG_FLAG_DEPRECATED) ? "(DEPRECATED) " : "",
							((config[sccp_option].flags & SCCP_CONFIG_FLAG_OBSOLETE) == SCCP_CONFIG_FLAG_OBSOLETE) ? "(DEPRECATED) " : "",
							size_str
							);
						if (!sccp_strlen_zero(config[sccp_option].description)) {
							description = pbx_strdup (config[sccp_option].description);
							while ((description_part = strsep(&description, "\n"))) {
								if (!sccp_strlen_zero(description_part)) {
									if (linelen) {
										fprintf(f, "%s\n", description_part);
									} else {
										fprintf(f, "%*.s ; %s\n", 81, " ", description_part);
									}
									linelen = 0;
								}
							}
							if (description_part) {
								sccp_free(description_part);
							}
							sccp_free (description);
						} else {
							fprintf(f, "\n");
						}
						if (ast_str_strlen(extra_info)) {
							fprintf(f, "%*.s ; %s\n", 81, " ", pbx_str_buffer(extra_info));
							ast_str_reset(extra_info);
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
	pbx_log(LOG_NOTICE, "Created new wiki file '%s'\n", fn);

	return 0;
};

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(sccp_config_base_functions)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "base_functions";
			info->category = "/channels/chan_sccp/config/";
			info->summary = "chan-sccp-b config test";
			info->description = "chan-sccp-b config tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	pbx_test_status_update(test, "Executing chan-sccp-b config tests...\n");

	pbx_test_status_update(test, "sccp_find_segment...\n");
	pbx_test_validate(test, sccp_find_segment(SCCP_CONFIG_GLOBAL_SEGMENT) == &sccpConfigSegments[0]);

	pbx_test_status_update(test, "sccp_fine_config...\n");
	const SCCPConfigSegment *sccpConfigSegment = sccp_find_segment(SCCP_CONFIG_GLOBAL_SEGMENT);
	pbx_test_validate(test, sccp_find_config(SCCP_CONFIG_GLOBAL_SEGMENT, "debug") == &sccpConfigSegment->config[0]);
	pbx_test_validate(test, sccp_find_config(SCCP_CONFIG_GLOBAL_SEGMENT, "port") == &sccpConfigSegment->config[6]);

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(sccp_config_multientry)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "MultiEntryParameters";
			info->category = "/channels/chan_sccp/config/";
			info->summary = "chan-sccp-b config test";
			info->description = "chan-sccp-b config tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	pbx_test_status_update(test, "createVariableSetForMultiEntryParameters...\n");
	PBX_VARIABLE_TYPE *varset = NULL, *v = NULL,*root = NULL;
	root = ast_variable_new("disallow", "0.0.0.0/0.0.0.0", "");
	root->next = ast_variable_new("allow", "10.10.10.0/255.255.255.0", "");
	
	v = varset = createVariableSetForMultiEntryParameters(root, "disallow|allow", varset);
	pbx_test_validate(test, v != NULL);
	pbx_test_status_update(test, "Test disallow == 0.0.0.0/0.0.0.0\n");
	pbx_test_validate(test, (!strcasecmp((const char *) "disallow", v->name) && !strcasecmp((const char *) "0.0.0.0/0.0.0.0", v->value)));
	v = v->next;
	pbx_test_validate(test, v != NULL);
	pbx_test_status_update(test, "Test allow == 10.10.10.10/255.255.255.255\n");
	pbx_test_validate(test, (!strcasecmp((const char *) "allow", v->name) && !strcasecmp((const char *) "10.10.10.0/255.255.255.0", v->value)));
	v = v->next;
	pbx_test_validate(test, v == NULL);
	
	pbx_variables_destroy(varset);
	pbx_variables_destroy(root);

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(sccp_config_tokenized_default)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "TokenizedDefault";
			info->category = "/channels/chan_sccp/config/";
			info->summary = "chan-sccp-b config test";
			info->description = "chan-sccp-b config tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}

	pbx_test_status_update(test, "createVariableSetForTokenizedDefault...\n");
	PBX_VARIABLE_TYPE *varset = NULL, *v = NULL;
	v = varset = createVariableSetForTokenizedDefault("disallow|allow", "0.0.0.0/0.0.0.0|10.10.10.0/255.255.255.0", NULL);
	
	pbx_test_validate(test, v != NULL);
	pbx_test_status_update(test, "Test disallow == 0.0.0.0\n");
	pbx_test_validate(test, (!strcasecmp((const char *) "disallow", v->name) && !strcasecmp((const char *) "0.0.0.0/0.0.0.0", v->value)));
	v = v->next;
	pbx_test_validate(test, v != NULL);
	pbx_test_status_update(test, "Test allow == 10.10.10.0/255.255.255.0\n");
	pbx_test_validate(test, (!strcasecmp((const char *) "allow", v->name) && !strcasecmp((const char *) "10.10.10.0/255.255.255.0", v->value)));
	
	pbx_variables_destroy(varset);

	return AST_TEST_PASS;
}

/*
AST_TEST_DEFINE(sccp_config_setValue)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "setValue";
			info->category = "/channels/chan_sccp/config/";
			info->summary = "chan-sccp-b config test";
			info->description = "chan-sccp-b config tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	//pbx_test_status_update(test, "sccp_config_object_setValue...\n");
	//static sccp_configurationchange_t sccp_config_object_setValue(void *obj, PBX_VARIABLE_TYPE * cat_root, const char *name, const char *value, int lineno, const sccp_config_segment_t segment, boolean_t *SetEntries, FALSE)

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(sccp_config_setDefault)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "setDefault";
			info->category = "/channels/chan_sccp/config/";
			info->summary = "chan-sccp-b config test";
			info->description = "chan-sccp-b config tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	//pbx_test_status_update(test, "sccp_config_set_defaults...\n");
	//static void sccp_config_set_defaults(void *obj, const sccp_config_segment_t segment, boolean_t *SetEntries)

	return AST_TEST_PASS;
}
*/

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_config_base_functions);
	AST_TEST_REGISTER(sccp_config_multientry);
	AST_TEST_REGISTER(sccp_config_tokenized_default);
	//AST_TEST_REGISTER(sccp_config_setValue);
	//AST_TEST_REGISTER(sccp_config_setDefault);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_config_base_functions);
	AST_TEST_UNREGISTER(sccp_config_multientry);
	AST_TEST_UNREGISTER(sccp_config_tokenized_default);
	//AST_TEST_UNREGISTER(sccp_config_setValue);
	//AST_TEST_UNREGISTER(sccp_config_setDefault);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
