/*!
 * \file        sccp_mwi.h
 * \brief       SCCP Message Waiting Indicator Header
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
#pragma once

#include "sccp_cli.h"

__BEGIN_C_EXTERN__
//#ifdef CS_AST_HAS_EVENT
//#include <asterisk/event.h>
//#endif

/*!
 * \brief SCCP Mailbox Structure
 */
struct sccp_mailbox {
	char *mailbox;												/*!< Mailbox */
	char *context;												/*!< Context */
	SCCP_LIST_ENTRY (sccp_mailbox_t) list;									/*!< Mailbox Linked List Entry */
};														/*!< SCCP Mailbox Structure */

SCCP_API void SCCP_CALL sccp_mwi_module_start(void);
SCCP_API void SCCP_CALL sccp_mwi_module_stop(void);
SCCP_API void SCCP_CALL sccp_mwi_check(sccp_device_t * d);

SCCP_API void SCCP_CALL sccp_mwi_unsubscribeMailbox(sccp_mailbox_t *mailbox);

#if defined( CS_AST_HAS_EVENT )
SCCP_API void SCCP_CALL sccp_mwi_event(const struct ast_event *event, void *data);
#elif defined(CS_AST_HAS_STASIS)
SCCP_API void SCCP_CALL sccp_mwi_event(void *userdata, struct stasis_subscription *sub, struct stasis_message *msg);
#else
SCCP_API int SCCP_CALL sccp_mwi_checksubscription(const void *ptr);
#endif
SCCP_API void SCCP_CALL sccp_mwi_setMWILineStatus(sccp_linedevices_t * lineDevice);
SCCP_API int SCCP_CALL sccp_show_mwi_subscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
