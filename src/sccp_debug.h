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

#ifdef NO_FILE_LINE_FUNC_DEBUG
#undef NO_FILE_LINE_FUNC_DEBUG
#endif

#define _B_ "", 0, ""
#define __LOG_VERBOSE    2
#define NO_FILE_LINE_FUNC_DEBUG      __LOG_VERBOSE, _B_

#define sccp_log1(...) {									\
	if ((sccp_globals->debug & (DEBUGCAT_FILELINEFUNC)) == DEBUGCAT_FILELINEFUNC) {		\
		pbx_log(AST_LOG_NOTICE, __VA_ARGS__);						\
	} else {										\
		pbx_log(NO_FILE_LINE_FUNC_DEBUG, __VA_ARGS__);					\
	}											\
}
#define sccp_log(_x) if ((sccp_globals->debug & (_x))) sccp_log1
#define sccp_log_and(_x) if ((sccp_globals->debug & (_x)) == (_x)) sccp_log1

__BEGIN_C_EXTERN__
extern const char *SS_Memory_Allocation_Error;
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
	DEBUGCAT_CALLINFO		= 1 << 23,
	DEBUGCAT_REFCOUNT		= 1 << 24,
	DEBUGCAT_MESSAGE 		= 1 << 25,
	DEBUGCAT_NEWCODE 		= 1 << 26,
	DEBUGCAT_THPOOL			= 1 << 27,
	DEBUGCAT_FILELINEFUNC		= 1 << 28,
	DEBUGCAT_HIGH 			= 1 << 29,
	DEBUGCAT_ALL 			= 0xffffffff,
	/* *INDENT-ON* */
} sccp_debug_category_t;											/*!< SCCP Debug Category Enum (saved in global_vars:debug = uint32_t) */

struct sccp_debug_category {
	const char *const key;
	const char *const text;
	sccp_debug_category_t category;
};
extern const struct sccp_debug_category sccp_debug_categories[32];

SCCP_API int32_t SCCP_CALL sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug_value);
SCCP_API char * SCCP_CALL sccp_get_debugcategories(int32_t debugvalue);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
