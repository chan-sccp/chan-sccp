/*!
 * \file        sccp_hint.h
 * \brief       SCCP Hint Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 */
#pragma once
#include "sccp_cli.h"

__BEGIN_C_EXTERN__
SCCP_API sccp_channelstate_t SCCP_CALL sccp_hint_getLinestate(const char *linename, const char *deviceId);
SCCP_API void SCCP_CALL sccp_hint_module_start(void);
SCCP_API void SCCP_CALL sccp_hint_module_stop(void);

SCCP_API int SCCP_CALL sccp_show_hint_lineStates(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
SCCP_API int SCCP_CALL sccp_show_hint_subscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
