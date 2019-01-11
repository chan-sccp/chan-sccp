/*!
 * \file	sccp_featureParkingLot.h
 * \brief	SCCP ParkingLot Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#pragma once

/*!
 * \brief SCCP Mailbox Structure
 */
struct sccp_mailbox {
	char uniqueid[SCCP_MAX_MAILBOX_UNIQUEID];
	SCCP_LIST_ENTRY (sccp_mailbox_t) list;									/*!< Mailbox Linked List Entry */
};														/*!< SCCP Mailbox Structure */

__BEGIN_C_EXTERN__
typedef struct {
	void (*startModule)(void);
	void (*stopModule)(void);
	int (*showSubscriptions) (int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
} VoicemailInterface;
extern const VoicemailInterface iVoicemail;

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
