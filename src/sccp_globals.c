/*!
 * \file        sccp_globals.c
 * \brief       SCCP Globals Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#include "config.h"
#include "common.h"
#include "sccp_globals.h"

SCCP_FILE_VERSION(__FILE__, "");

char SCCP_VERSIONSTR[300];
char SCCP_REVISIONSTR[30];

/*!
 * \brief       Global variables
 */
struct sccp_global_vars *sccp_globals = 0;

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
