/*!
 * \file 	sccp_management.h
 * \brief 	SCCP Management Header
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifdef CS_SCCP_MANAGER
/*
 * sccp_management.h
 *
 *  Created on: 22.11.2008
 *      Author: marcello
 */

#ifndef __SCCP_MANAGEMENT_H
#define __SCCP_MANAGEMENT_H

#include "asterisk.h"

#include "asterisk/pbx.h"
#include "asterisk/manager.h"

void sccp_register_management(void);
void sccp_unregister_management(void);

#endif /* SCCP_MANAGEMENT_H_ */
#endif
