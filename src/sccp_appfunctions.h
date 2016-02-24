/*!
 * \file        sccp_appfunctions.h
 * \brief       SCCP application / dialplan functions Class 
 * \author      Diederik de Groot (ddegroot [at] sourceforge.net)
 * \date        18-03-2011
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 */
#pragma once
__BEGIN_C_EXTERN__
SCCP_API int SCCP_CALL sccp_register_dialplan_functions(void);
SCCP_API int SCCP_CALL sccp_unregister_dialplan_functions(void);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
