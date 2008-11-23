#ifdef  CS_SCCP_MANAGER
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



static char management_show_devices_desc[] =
	"Description: Lists SCCP devices in text format with details on current status.\n"
	"test\n"
	"DevicelistComplete.\n"
	"Variables: \n"
	"  ActionID: <id>	Action ID for this transaction. Will be returned.\n";

static char management_restart_devices_desc[] =
	"Description: restart a given device\n"
	"\n"
	"Variables:\n"
	"   Devicename: Name of device to restart\n";


void sccp_register_management(void);
void sccp_unregister_management(void);
static int sccp_manager_show_devices(struct mansession *s, const struct message *m);
static int sccp_manager_restart_device(struct mansession *s, const struct message *m);

#endif /* SCCP_MANAGEMENT_H_ */
#endif
