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
