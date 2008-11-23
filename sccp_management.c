#ifdef CS_SCCP_MANAGER
/*
 * sccp_management.c
 *
 *  Created on: 22.11.2008
 *      Author: marcello
 */
#include "config.h"
#include "sccp_management.h"
#include "chan_sccp.h"
#include "sccp_utils.h"
#include "sccp_lock.h"
#include "sccp_device.h"




/**
 * Register manager commands
 */
void sccp_register_management(void){
	/* Register manager commands */
	ast_manager_register2(
			"SccpDevices",
			EVENT_FLAG_SYSTEM | EVENT_FLAG_REPORTING,
			sccp_manager_show_devices,
			"List SCCP devices (text format)",
			management_show_devices_desc);
	ast_manager_register2(
				"SCCPRestartDevice",
				EVENT_FLAG_SYSTEM | EVENT_FLAG_REPORTING,
				sccp_manager_restart_device,
				"Restart a given device",
				management_restart_devices_desc);
}

void sccp_unregister_management(void){
	ast_manager_unregister("SccpDevices");
	ast_manager_unregister("SCCPRestartDevice");
}

static int sccp_manager_show_devices(struct mansession *s, const struct message *m){
	const char 	*id = astman_get_header(m, "ActionID");
	//const char 	*a[] = {"sccp", "show", "devices"};
	char 			idtext[256] = "";
	int 			total = 0;

//	if (!ast_strlen_zero(id))
	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	astman_send_listack(s, m, "Device status list will follow", "start");
	/* List the peers in separate manager events */


	/* Send final confirmation */
	astman_append(s,
			"Event: DevicelistComplete\r\n"
			"EventList: Complete\r\n"
			"ListItems: %d\r\n"
			"%s"
			"\r\n",
			total, idtext);
	return 0;
}

static int sccp_manager_restart_device(struct mansession *s, const struct message *m){
	sccp_list_t		*hintList = NULL;
	sccp_moo_t 		*r;
	sccp_device_t	*d;
	sccp_hint_t		*h;
	const char 	*fn = astman_get_header(m, "Devicename");

	ast_log(LOG_WARNING, "Attempt to get device %s\n",fn);
	if (ast_strlen_zero(fn)) {
		astman_send_error(s, m, "Devicename not specified");
		return 0;
	}

	d = sccp_device_find_byid(fn, FALSE);
	if(!d){
		astman_send_error(s, m, "Devicename not found");
		return 0;
	}
	sccp_device_lock(d);

	if (!d->session) {
		astman_send_error(s, m, "Device not registered");
		sccp_device_unlock(d);
		return 0;
	}

	hintList = d->hints;

	while (hintList) {
		h = (sccp_hint_t*)hintList->data;
		/* force the hint state for non SCCP (or mixed) devices */
		if(h)
			sccp_hint_state(NULL, NULL, AST_EXTENSION_NOT_INUSE, h);
		hintList = hintList->next;
	}
	sccp_device_unlock(d);

	REQ(r, Reset);
	r->msg.Reset.lel_resetType = htolel(SKINNY_DEVICE_RESET);
	sccp_dev_send(d, r);

	astman_start_ack(s, m);
	astman_append(s, "Send reset to device %s\r\n", fn);

	astman_append(s, "\r\n");

	return 0;
}
#endif
