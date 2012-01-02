
/*!
 * \file 	sccp_management.h
 * \brief 	SCCP Management Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */

#    ifdef CS_SCCP_MANAGER

/*
 * sccp_management.h
 *
 *  Created on: 22.11.2008
 *      Author: marcello
 */

#ifndef __SCCP_MANAGEMENT_H
#    define __SCCP_MANAGEMENT_H

int sccp_register_management(void);
int sccp_unregister_management(void);
void sccp_manager_module_start(void);
void sccp_manager_module_stop(void);

#endif										/* SCCP_MANAGEMENT_H_ */
#    endif
