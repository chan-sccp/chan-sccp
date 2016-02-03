/*!
 * \file        sccp_debug.h
 * \brief       SCCP Debug Header
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#pragma once
#include "config.h"
#include "define.h"

static const char SS_Memory_Allocation_Error[] = "%s: Memory Allocation Error.\n";

/*!
 * \brief SCCP Debug Category Enum
 */
typedef enum {
	/* *INDENT-OFF* */
	DEBUGCAT_NONE			= 0,
	DEBUGCAT_CORE 			= 1 << 0,
	DEBUGCAT_SCCP 			= 1 << 1,
	DEBUGCAT_HINT 			= 1 << 2,
	DEBUGCAT_RTP 			= 1 << 3,
	DEBUGCAT_DEVICE 		= 1 << 4,
	DEBUGCAT_LINE 			= 1 << 5,
	DEBUGCAT_ACTION 		= 1 << 6,
	DEBUGCAT_CHANNEL 		= 1 << 7,
	DEBUGCAT_CLI 			= 1 << 8,
	DEBUGCAT_CONFIG 		= 1 << 9,
	DEBUGCAT_FEATURE 		= 1 << 10,
	DEBUGCAT_FEATURE_BUTTON		= 1 << 11,
	DEBUGCAT_SOFTKEY 		= 1 << 12,
	DEBUGCAT_INDICATE 		= 1 << 13,
	DEBUGCAT_PBX 			= 1 << 14,
	DEBUGCAT_SOCKET 		= 1 << 15,
	DEBUGCAT_MWI 			= 1 << 16,
	DEBUGCAT_EVENT 			= 1 << 17,
	DEBUGCAT_CONFERENCE 		= 1 << 18,
	DEBUGCAT_BUTTONTEMPLATE 	= 1 << 19,
	DEBUGCAT_SPEEDDIAL 		= 1 << 20,
	DEBUGCAT_CODEC 			= 1 << 21,
	DEBUGCAT_REALTIME 		= 1 << 22,
	DEBUGCAT_LOCK 			= 1 << 23,
	DEBUGCAT_REFCOUNT		= 1 << 24,
	DEBUGCAT_MESSAGE 		= 1 << 25,
	DEBUGCAT_NEWCODE 		= 1 << 26,
	DEBUGCAT_THPOOL			= 1 << 27,
	DEBUGCAT_FILELINEFUNC		= 1 << 28,
	DEBUGCAT_HIGH 			= 1 << 29,
	DEBUGCAT_ALL 			= 0xffffffff,
	/* *INDENT-ON* */
} sccp_debug_category_t;											/*!< SCCP Debug Category Enum (saved in global_vars:debug = uint32_t) */

/*!
 * \brief SCCP Verbose Level Structure
 */
static const struct sccp_debug_category {
	const char *const key;
	const char *const text;
	sccp_debug_category_t category;
} sccp_debug_categories[] = {
	/* *INDENT-OFF* */
	{"all",			"all debug levels", 			DEBUGCAT_ALL,},
	{"none",		"all debug levels", 			DEBUGCAT_NONE,},
	{"core",		"core debug level", 			DEBUGCAT_CORE},
	{"sccp",		"sccp debug level", 			DEBUGCAT_SCCP},
	{"hint",		"hint debug level", 			DEBUGCAT_HINT},
	{"rtp",			"rtp debug level", 			DEBUGCAT_RTP},
	{"device",		"device debug level", 			DEBUGCAT_DEVICE},
	{"line",		"line debug level", 			DEBUGCAT_LINE},
	{"action",		"action debug level", 			DEBUGCAT_ACTION},
	{"channel",		"channel debug level", 			DEBUGCAT_CHANNEL},
	{"cli",			"cli debug level", 			DEBUGCAT_CLI},
	{"config",		"config debug level", 			DEBUGCAT_CONFIG},
	{"feature",		"feature debug level", 			DEBUGCAT_FEATURE},
	{"feature_button",	"feature_button debug level",		DEBUGCAT_FEATURE_BUTTON},
	{"softkey",		"softkey debug level", 			DEBUGCAT_SOFTKEY},
	{"indicate",		"indicate debug level",	 		DEBUGCAT_INDICATE},
	{"pbx",			"pbx debug level", 			DEBUGCAT_PBX},
	{"socket",		"socket debug level", 			DEBUGCAT_SOCKET},
	{"mwi",			"mwi debug level", 			DEBUGCAT_MWI},
	{"event",		"event debug level", 			DEBUGCAT_EVENT},
	{"conference",		"conference debug level", 		DEBUGCAT_CONFERENCE},
	{"buttontemplate",	"buttontemplate debug level",		DEBUGCAT_BUTTONTEMPLATE},
	{"speeddial",		"speeddial debug level",		DEBUGCAT_SPEEDDIAL},
	{"codec",		"codec debug level", 			DEBUGCAT_CODEC},
	{"realtime",		"realtime debug level",	 		DEBUGCAT_REALTIME},
	{"lock",		"lock debug level", 			DEBUGCAT_LOCK},
	{"refcount",		"refcount lock debug level", 		DEBUGCAT_REFCOUNT},
	{"message",		"message debug level", 			DEBUGCAT_MESSAGE},
	{"newcode",		"newcode debug level", 			DEBUGCAT_NEWCODE},
	{"threadpool",		"threadpool debug level",	 	DEBUGCAT_THPOOL},
	{"filelinefunc",	"add line/file/function to debug output", DEBUGCAT_FILELINEFUNC},
	{"high",		"high debug level", 			DEBUGCAT_HIGH},
	/* *INDENT-ON* */
};

SCCP_API int32_t SCCP_CALL sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug);
SCCP_API char * SCCP_CALL sccp_get_debugcategories(int32_t debugvalue);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
