/*!
 * \file        sccp_devstate.h
 * \brief       SCCP device state Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2013-08-15
 */
#pragma once

__BEGIN_C_EXTERN__
/*!
 * \brief SCCP DevState Specifier Structure
 * Recording number of Device State Registrations Per Device
 */
#ifdef CS_DEVSTATE_FEATURE
struct sccp_devstate_specifier {
	char specifier[SCCP_MAX_DEVSTATE_SPECIFIER];								/*!< Name of the Custom  Devstate Extension */
	struct ast_event_sub *sub;										/* Asterisk event Subscription related to the devstate extension. */
	/* Note that the length of the specifier matches the length of "options" of the sccp_feature.options field,
	   to which it corresponds. */
	SCCP_LIST_ENTRY (sccp_devstate_specifier_t) list;							/*!< Specifier Linked List Entry */
};														/*!< SCCP Devstate Specifier Structure */

SCCP_API void SCCP_CALL sccp_devstate_module_start(void);
SCCP_API void SCCP_CALL sccp_devstate_module_stop(void);
#endif
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
