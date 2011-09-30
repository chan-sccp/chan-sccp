
/*!
 * \file 	sccp_config_entries.h
 * \brief 	SCCP Config Entries Header
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
#define ARRAY_LEN(a) (size_t) (sizeof(a) / sizeof(0[a]))
typedef enum { FALSE = 0, TRUE = 1 } boolean_t;

#define offsetof(T, F) 0
#define offsize(T, F) 0

#define G_OBJ_REF(x) 0, 0
#define D_OBJ_REF(x) 0, 0
#define L_OBJ_REF(x) 0, 0
#define S_OBJ_REF(x) 0, 0
#define H_OBJ_REF(x) 0, 0

/* dyn config */
typedef enum {
	SCCP_CONFIG_NOUPDATENEEDED = 0,
	SCCP_CONFIG_NEEDDEVICERESET = 1 << 1
} sccp_configurationchange_t;						/*!< configuration state change */

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
	SCCP_CONFIG_GLOBAL_SEGMENT			= 0,
	SCCP_CONFIG_DEVICE_SEGMENT,
	SCCP_CONFIG_LINE_SEGMENT,
	SCCP_CONFIG_SOFTKEY_SEGMENT,
} sccp_config_segment_t;

/*!
 * \brief Enum for Config Option Types
 */
enum SCCPConfigOptionType {
/* *INDENT-OFF* */
	SCCP_CONFIG_DATATYPE_BOOLEAN			= 1 << 0,
	SCCP_CONFIG_DATATYPE_INT			= 1 << 1,
	SCCP_CONFIG_DATATYPE_UINT			= 1 << 2,
	SCCP_CONFIG_DATATYPE_STRING			= 1 << 3,
	SCCP_CONFIG_DATATYPE_GENERIC			= 1 << 4,
	SCCP_CONFIG_DATATYPE_STRINGPTR			= 1 << 5,	/* pointer */
	SCCP_CONFIG_DATATYPE_CHAR			= 1 << 6,
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
/* *INDENT-ON* */
};


/*!
 * \brief SCCP Config Option Struct
 */
typedef struct SCCPConfigOption {

/* *INDENT-ON* */
	const char *name;							/*!< Configuration Parameter Name */
	const int offset;							/*!< The offset relative to the context structure where the option value is stored. */
	const int size;								/*!< Structure size */
	enum SCCPConfigOptionType type;						/*!< Data type */
	enum SCCPConfigOptionFlag flags;					/*!< Data type */
	sccp_configurationchange_t change;					/*!< Does a change of this value needs a device restart */
	const char *defaultValue;						/*!< Default value */
//	sccp_value_changed_t(*converter_f) (void *dest, const size_t size, const char *value, const sccp_config_segment_t segment);	/*!< Conversion function */
	const char *cb;
	const char *description;						/*!< Configuration description (config file) or warning message for deprecated or obsolete values */
/* *INDENT-OFF* */
} SCCPConfigOption;

//converter function prototypes 
#define sccp_config_parse_codec_preferences "sccp_config_parse_codec_preferences"
#define sccp_config_parse_allow_codec "sccp_config_parse_allow_codec"
#define sccp_config_parse_disallow_codec "sccp_config_parse_disallow_codec"
#define sccp_config_parse_mailbox "sccp_config_parse_mailbox"
#define sccp_config_parse_tos "sccp_config_parse_tos"
#define sccp_config_parse_cos "sccp_config_parse_cos"
#define sccp_config_parse_amaflags "sccp_config_parse_amaflags"
#define sccp_config_parse_secondaryDialtoneDigits "sccp_config_parse_secondaryDialtoneDigits"
#define sccp_config_parse_variables "sccp_config_parse_variables"
#define sccp_config_parse_group "sccp_config_parse_group"
#define sccp_config_parse_permit "sccp_config_parse_permit"
#define sccp_config_parse_deny "sccp_config_parse_deny"
#define sccp_config_parse_button "sccp_config_parse_button"
#define sccp_config_parse_permithosts "sccp_config_parse_permithosts"
#define sccp_config_parse_addons "sccp_config_parse_addons"
#define sccp_config_parse_privacyFeature "sccp_config_parse_privacyFeature"
#define sccp_config_parse_earlyrtp "sccp_config_parse_earlyrtp"
#define sccp_config_parse_dtmfmode "sccp_config_parse_dtmfmode"
#define sccp_config_parse_mwilamp "sccp_config_parse_mwilamp"
#define sccp_config_parse_debug "sccp_config_parse_debug"
#define sccp_config_parse_ipaddress "sccp_config_parse_ipaddress"
#define sccp_config_parse_port "sccp_config_parse_port"
#define sccp_config_parse_blindtransferindication "sccp_config_parse_blindtransferindication"
#define sccp_config_parse_callanswerorder "sccp_config_parse_callanswerorder"
#define sccp_config_parse_regcontext "sccp_config_parse_regcontext"
#define sccp_config_parse_context "sccp_config_parse_context"
#define sccp_config_parse_hotline_context "sccp_config_parse_hotline_context"
#define sccp_config_parse_hotline_exten "sccp_config_parse_hotline_exten"
#define sccp_config_parse_dnd "sccp_config_parse_dnd"

#include "../../src/sccp_config_entries.h"

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
	{"global", SCCP_CONFIG_GLOBAL_SEGMENT, sccpGlobalConfigOptions, ARRAY_LEN(sccpGlobalConfigOptions)},
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
	short unsigned int i = 0;

	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
		if (sccpConfigSegments[i].segment == segment)
			return &sccpConfigSegments[i];
	}
	return NULL;
}

