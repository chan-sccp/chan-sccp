/*!
 * \file        sccp_management.h
 * \brief       SCCP Management Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Revision: 2130 $  
 */
#pragma once

#ifdef CS_SCCP_MANAGER
/*
 * sccp_management.h
 *
 *  Created on: 22.11.2008
 *      Author: marcello
 */
int sccp_register_management(void);
int sccp_unregister_management(void);
void sccp_manager_module_start(void);
void sccp_manager_module_stop(void);

boolean_t sccp_manager_action2str(const char *manager_command, char **outStr);
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
