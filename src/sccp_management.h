/*!
 * \file        sccp_management.h
 * \brief       SCCP Management Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

#ifdef CS_SCCP_MANAGER
__BEGIN_C_EXTERN__
/*
 * sccp_management.h
 *
 *  Created on: 22.11.2008
 *      Author: marcello
 */
SCCP_API int SCCP_CALL sccp_register_management(void);
SCCP_API int SCCP_CALL sccp_unregister_management(void);
SCCP_API void SCCP_CALL sccp_manager_module_start(void);
SCCP_API void SCCP_CALL sccp_manager_module_stop(void);

#if HAVE_PBX_MANAGER_HOOK_H
SCCP_API boolean_t SCCP_CALL sccp_manager_action2str(const char *manager_command, char **outStr);
#if defined(CS_EXPERIMENTAL)
SCCP_API char * SCCP_CALL sccp_manager_retrieve_parkedcalls_cxml(char ** out);
#endif
#endif

__END_C_EXTERN__
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
