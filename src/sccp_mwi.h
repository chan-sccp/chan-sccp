/*!
 * \file        sccp_mwi.h
 * \brief       SCCP Message Waiting Indicator Header
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef SCCP_MWI_H_
#define SCCP_MWI_H_

#include "sccp_cli.h"
#ifdef CS_AST_HAS_EVENT
#include "asterisk/event.h"
#endif


void sccp_mwi_module_start(void);
void sccp_mwi_module_stop(void);
void sccp_mwi_check(sccp_device_t * device);

void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t ** mailbox);

#if defined( CS_AST_HAS_EVENT )
void sccp_mwi_event(const struct ast_event *event, void *data);
#elif defined(CS_AST_HAS_STASIS)
void sccp_mwi_event(void *userdata, struct stasis_subscription *sub, struct stasis_message *msg);
#else
int sccp_mwi_checksubscription(const void *ptr);
#endif
int sccp_show_mwi_subscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);

#endif														/*SCCP_MWI_H_ */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
