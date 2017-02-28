/*!
 * \file 	gen_sccpconf.h
 * \brief 	SCCP Config Generator Header
 * \author      Diederik de Groot <ddegroot [at] sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#pragma once


#define ARRAY_LEN(a) (size_t) (sizeof(a) / sizeof(0[a]))
typedef enum { FALSE = 0, TRUE = 1 } boolean_t;

#undef offsetof
#define offsetof(T, F) 0
#undef offsize
#define offsize(T, F) 0

#include <sys/types.h>
#include <stdint.h>

#if !defined(__BEGIN_C_EXTERN__)
#  if defined(__cplusplus) || defined(c_plusplus) 
#    define __BEGIN_C_EXTERN__ 		\
extern "C" {
#    define __END_C_EXTERN__ 		\
}
#  else
#    define __BEGIN_C_EXTERN__ 
#    define __END_C_EXTERN__ 
#  endif
#endif

#if !defined(SCCP_API)
#if defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L && defined(__GNUC__)
#  if !defined(__clang__)
#    define gcc_inline __inline__
#  else
#    define gcc_inline
#  endif
#  define SCCP_API extern __attribute__((__visibility__("hidden")))
#  define SCCP_API_VISIBLE extern __attribute__((__visibility__("default")))
#  define SCCP_INLINE SCCP_API gcc_inline
#  define SCCP_CALL 
#else
#  define gcc_inline
#  define SCCP_API extern
#  define SCCP_API_VISIBLE extern
#  define SCCP_INLINE SCCP_API gcc_inline
#  define SCCP_CALL 
#endif
#endif

#include "../../src/sccp_enum.h"

/*
#define G_OBJ_REF(x) #x,offsize(struct sccp_global_vars,x)
#define D_OBJ_REF(x) #x,offsize(struct sccp_device,x)
#define L_OBJ_REF(x) #x,offsize(struct sccp_line,x)
#define S_OBJ_REF(x) #x,offsize(struct softKeySetConfiguration,x)
#define H_OBJ_REF(x) #x,offsize(struct sccp_hotline,x)
*/
#define G_OBJ_REF(x) 0,0
#define D_OBJ_REF(x) 0,0
#define L_OBJ_REF(x) 0,0
#define S_OBJ_REF(x) 0,0
#define H_OBJ_REF(x) 0,0

/* dyn config */
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
	SCCP_CONFIG_DATATYPE_PARSER			= 1 << 4,
	SCCP_CONFIG_DATATYPE_STRINGPTR			= 1 << 5,	/* pointer */
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
/* *INDENT-ON* */
	const char *name;							/*!< Configuration Parameter Name */
	const int offset;
	const size_t size;
	enum SCCPConfigOptionType type;						/*!< Data type */
	const char *generic_parser;
        sccp_enum_str2intval_t str2intval;
        sccp_enum_all_entries_t all_entries;
        const char *parsername;
	enum SCCPConfigOptionFlag flags;					/*!< Data type */
	sccp_configurationchange_t change;					/*!< Does a change of this value needs a device restart */
	const char *defaultValue;						/*!< Default value */
	const char *description;						/*!< Configuration description (config file) or warning message for deprecated or obsolete values */
/* *INDENT-OFF* */
} SCCPConfigOption;

//converter function prototypes 
#define sccp_config_parse_mailbox "(mailbox)=mailbox@context"
#define sccp_config_parse_tos "(tos)=[[value]]|lowdelay|throughput|reliability|mincost|none"
#define sccp_config_parse_cos "(cos)=[[value]]"
#define sccp_config_parse_amaflags "(amaflags)=[[string]]"
#define sccp_config_parse_secondaryDialtoneDigits "(secondaryDialtoneDigits)=[[value]]"
#define sccp_config_parse_variables "(variables)=[[string]]"
#define sccp_config_parse_group "(group)=[[fromto]],[[value]]"
#define sccp_config_parse_button "(button)=[[button]]"
#define sccp_config_parse_permithosts "(permithosts)=[[hostname]]"
#define sccp_config_parse_addons "(addons)=7914,7915,7916,SPA500S"
#define sccp_config_parse_privacyFeature "(privacyFeature)=full|on|off"
#define sccp_config_parse_debug "(debug)=all,none,core,sccp,hint,rtp,device,line,action,channel,cli,config,feature,feature_button,softkey,indicate,pbx,socket,mwi,event,adv_feature,conference,buttontemplate,speeddial,codec,realtime,lock,threadlock,message,newcode,high,myi,fixme,fyi"
#define sccp_config_parse_ipaddress "(ipaddress)=[[ipaddress]]"
#define sccp_config_parse_port "(port)=[[port]]"
#define sccp_config_parse_context "(context)=[[context]]"
#define sccp_config_parse_hotline_context "(hotline_context)=[[context]]"
#define sccp_config_parse_hotline_exten "(hotline_exten)=[[value]]"
#define sccp_config_parse_jbflags_enable "(jbflags_enable)=on|off"
#define sccp_config_parse_jbflags_force "(jbflags_force)=on|off"
#define sccp_config_parse_jbflags_log "(jbflags_log)=on|off"
#define sccp_config_parse_codec_preferences "(codec)=all|(alaw,ulaw,gsm,ilbc,g722,g723,g729,g726,slin,slin16)"
#define sccp_config_parse_deny_permit "(permit)=[[ipaddress]],internal | (deny)=[[ipaddress]],internal"
#define sccp_config_parse_hotline_label ""
#define sccp_config_parse_jbflags_maxsize "200"
#define sccp_config_parse_jbflags_jbresyncthreshold "1000"
#define sccp_config_parse_jbflags_impl "fixed"

uint32_t (sccp_enum_str2intval) (const char *lookup_str);
const char *(sccp_enum_all_entries)(void);

#define sccp_earlyrtp_str2intval sccp_enum_str2intval
#define skinny_lampmode_str2intval sccp_enum_str2intval
#define sccp_blindtransferindication_str2intval sccp_enum_str2intval
#define sccp_call_answer_order_str2intval sccp_enum_str2intval
#define sccp_dtmfmode_str2intval sccp_enum_str2intval
#define sccp_dndmode_str2intval sccp_enum_str2intval
#define sccp_nat_str2intval sccp_enum_str2intval
#define skinny_ringtype_str2intval sccp_enum_str2intval

#define sccp_earlyrtp_all_entries sccp_enum_all_entries
#define skinny_lampmode_all_entries sccp_enum_all_entries
#define sccp_blindtransferindication_all_entries sccp_enum_all_entries
#define sccp_call_answer_order_all_entries sccp_enum_all_entries
#define sccp_dtmfmode_all_entries sccp_enum_all_entries
#define sccp_dndmode_all_entries sccp_enum_all_entries
#define sccp_nat_all_entries sccp_enum_all_entries
#define skinny_ringtype_all_entries sccp_enum_all_entries

#include "../../src/sccp_config_entries.hh"

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
	short unsigned int i = 0;

	for (i = 0; i < ARRAY_LEN(sccpConfigSegments); i++) {
		if (sccpConfigSegments[i].segment == segment)
			return &sccpConfigSegments[i];
	}
	return NULL;
}
