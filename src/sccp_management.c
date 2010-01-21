/*!
 * \file 	sccp_management.c
 * \brief 	SCCP Management Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \date	2008-11-22
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 */

#include "config.h"
#ifdef CS_SCCP_MANAGER
#include "sccp_management.h"
#include "chan_sccp.h"
#include "sccp_utils.h"
#include "sccp_lock.h"
#include "sccp_device.h"
#include "sccp_config.h"
#include "sccp_actions.h"
#include "sccp_hint.h"
#include "sccp_line.h"

/*!
 * \brief Show Device Description
 */
static char management_show_devices_desc[] =
        "Description: Lists SCCP devices in text format with details on current status.\n"
        "test\n"
        "DevicelistComplete.\n"
        "Variables: \n"
        "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";

/*!
 * \brief Restart Device Description
 */
static char management_restart_devices_desc[] =
        "Description: restart a given device\n"
        "\n"
        "Variables:\n"
        "   Devicename: Name of device to restart\n";

/*!
 * \brief Add Device Line Description
 */
static char management_show_device_add_line_desc[] =
        "Description: Lists SCCP devices in text format with details on current status.\n"
        "test\n"
        "DevicelistComplete.\n"
        "Variables: \n"
        "  Devicename: Name of device to restart.\n"
        "  Linename: Name of line";

/*!
 * \brief Show Device Update Description
 */
static char management_device_update_desc[] =
        "Description: restart a given device\n"
        "\n"
        "Variables:\n"
        "   Devicename: Name of device\n";

/*!
 * \brief Show Like Forward Description
 */
static char management_line_fwd_update_desc[] =
        "Description: update forward status for line\n"
        "\n"
        "Variables:\n"
        "  Linename: Name of line\n"
        "  Forwardtype: type of cfwd (all | busy | noAnswer)\n"
        "  Number: number to forward calls (optional)";

static int sccp_manager_show_devices(struct mansession *s, const struct message *m);
static int sccp_manager_restart_device(struct mansession *s, const struct message *m);
static int sccp_manager_device_add_line(struct mansession *s, const struct message *m);
static int sccp_manager_device_update(struct mansession *s, const struct message *m);
static int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m);


/*!
 * \brief Register management commands
 */
void sccp_register_management(void)
{
        /* Register manager commands */
        ast_manager_register2(
                "SCCPListDevices",
#ifndef ASTERISK_CONF_1_6
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG,
#else
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
#endif
                sccp_manager_show_devices,
                "List SCCP devices (text format)",
                management_show_devices_desc);


        ast_manager_register2(
                "SCCPDeviceRestart",
#ifndef ASTERISK_CONF_1_6
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG,
#else
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
#endif
                sccp_manager_restart_device,
                "Restart a given device",
                management_restart_devices_desc);

        ast_manager_register2(
                "SCCPDeviceAddLine",
#ifndef ASTERISK_CONF_1_6
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG,
#else
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
#endif
                sccp_manager_device_add_line,
                "add a line to device",
                management_show_device_add_line_desc);

        ast_manager_register2(
                "SCCPDeviceUpdate",
#ifndef ASTERISK_CONF_1_6
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG,
#else
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
#endif
                sccp_manager_device_update,
                "add a line to device",
                management_device_update_desc);

        ast_manager_register2(
                "SCCPLineForwardUpdate",
#ifndef ASTERISK_CONF_1_6
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG,
#else
                EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING,
#endif
                sccp_manager_line_fwd_update,
                "add a line to device",
                management_line_fwd_update_desc);
}

/*!
 * \brief Unregister management commands
 */
void sccp_unregister_management(void)
{
        ast_manager_unregister("SCCPListDevices");
        ast_manager_unregister("SCCPDeviceRestart");
        ast_manager_unregister("SCCPDeviceAddLine");
        ast_manager_unregister("SCCPDeviceUpdate");
        ast_manager_unregister("SCCPLineForwardUpdate");
}

/*!
 * \brief Show Devices Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 */
int sccp_manager_show_devices(struct mansession *s, const struct message *m)
{
        const char 	*id = astman_get_header(m, "ActionID");
        sccp_device_t	*device;
        char 			idtext[256] = "";
        int 			total = 0;

//	if (!ast_strlen_zero(id))
        snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

#ifndef ASTERISK_CONF_1_6
        astman_send_ack(s, m, "Device status list will follow");
#else
        astman_send_listack(s, m, "Device status list will follow", "start");
#endif
        /* List the peers in separate manager events */

        SCCP_LIST_LOCK(&GLOB(devices));
        SCCP_LIST_TRAVERSE(&GLOB(devices), device, list) {
                astman_append(s,"Devicename: %s\r\n", device->id);
                total++;
        }

        SCCP_LIST_UNLOCK(&GLOB(devices));

        /* Send final confirmation */
        astman_append(s,
                      "Event: SCCPListDevicesComplete\r\n"
                      "EventList: Complete\r\n"
                      "ListItems: %d\r\n"
                      "\r\n",
                      total);
        return 0;
}

/*!
 * \brief Restart Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 */
int sccp_manager_restart_device(struct mansession *s, const struct message *m)
{
//	sccp_list_t		*hintList = NULL;
        sccp_moo_t 		*r;
        sccp_device_t	*d;
        const char 	*fn = astman_get_header(m, "Devicename");

        ast_log(LOG_WARNING, "Attempt to get device %s\n",fn);

        if (ast_strlen_zero(fn)) {
                astman_send_error(s, m, "Please specify the name of device to be reset");
                return 0;
        }

        /*	This is ok but Manager apps should be just wrappers of other
        	routines not implement their ones -FS 25/11/2008
         */
        d = sccp_device_find_byid(fn, FALSE);

        if (!d) {
                astman_send_error(s, m, "Device not found");
                return 0;
        }

        sccp_device_lock(d);

        if (!d->session) {
                astman_send_error(s, m, "Device not registered");
                sccp_device_unlock(d);
                return 0;
        }


        REQ(r, Reset);

        r->msg.Reset.lel_resetType = htolel(SKINNY_DEVICE_RESET);
        sccp_dev_send(d, r);

        //astman_start_ack(s, m);
        astman_append(s, "Send reset to device %s\r\n", fn);
        astman_append(s, "\r\n");

        return 0;
}


/*!
 * \brief Add Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 */
static int sccp_manager_device_add_line(struct mansession *s, const struct message *m)
{
        sccp_device_t	*d;
        sccp_line_t	*line;
        const char 	*deviceName = astman_get_header(m, "Devicename");
        const char 	*lineName = astman_get_header(m, "Linename");

        ast_log(LOG_WARNING, "Attempt to get device %s\n",deviceName);

        if (ast_strlen_zero(deviceName)) {
                astman_send_error(s, m, "Please specify the name of device");
                return 0;
        }

        if (ast_strlen_zero(lineName)) {
                astman_send_error(s, m, "Please specify the name of line to be added");
                return 0;
        }

        /*	This is ok but Manager apps should be just wrappers of other
        	routines not implement their ones -FS 25/11/2008
         */
        d = sccp_device_find_byid(deviceName, FALSE);

        line = sccp_line_find_byname_wo(lineName, TRUE);

        if (!d) {
                astman_send_error(s, m, "Device not found");
                return 0;
        }

        if (!line) {
                astman_send_error(s, m, "Line not found");
                return 0;
        }

        sccp_config_addLine(d, line->name, 0);

        astman_append(s, "Done\r\n");
        astman_append(s, "\r\n");

        return 0;
}


/*!
 * \brief Update Line Forward Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * //TODO
 */
int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m)
{
        sccp_line_t	*line;

        const char 	*lineName = astman_get_header(m, "Linename");
        const char 	*forwardType = astman_get_header(m, "Forwardtype");
        const char 	*number = astman_get_header(m, "Number");

        char *fwdNumber = (char *)number;


        line = sccp_line_find_byname_wo(lineName, TRUE);

        if (!line) {
                astman_send_error(s, m, "Line not found");
                return 0;
        }

        if (line->devices.size > 1) {
                astman_send_error(s, m, "Callforwarding on shared lines is not supported at the moment");
                return 0;
        }


        if (!forwardType) {
                astman_send_error(s, m, "Forwardtype is not optional");
                return 0;
        }

//        /* (cfwdAll | cfwdBusy | noAnswer) */
//        if (!strcasecmp(forwardType, "all")) {
//                sccp_line_cfwd(line, SCCP_CFWD_ALL, fwdNumber);
//        } else if (!strcasecmp(forwardType, "busy")) {
//                //sccp_line_cfwd(line, SCCP_CFWD_ALL, number);
//                astman_send_error(s,m, "cfwdBusy is not handled for the moment");
//        } else if (!strcasecmp(forwardType, "noAnswer")) {
//                //sccp_line_cfwd(line, SCCP_CFWD_ALL, number);
//                astman_send_error(s,m, "noAnswer is not handled for the moment");
//        }

        return 0;
}

/*!
 * \brief Update Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 */
static int sccp_manager_device_update(struct mansession *s, const struct message *m)
{
        sccp_device_t	*d;
        const char 	*deviceName = astman_get_header(m, "Devicename");

        ast_log(LOG_WARNING, "Attempt to get device %s\n",deviceName);

        if (ast_strlen_zero(deviceName)) {
                astman_send_error(s, m, "Please specify the name of device");
                return 0;
        }

        /*	This is ok but Manager apps should be just wrappers of other
        	routines not implement their ones -FS 25/11/2008
         */
        d = sccp_device_find_byid(deviceName, FALSE);

        if (!d) {
                astman_send_error(s, m, "Device not found");
                return 0;
        }

        if (!d->session) {
                astman_send_error(s, m, "Device not active");
                return 0;
        }

        sccp_handle_soft_key_template_req(d->session, NULL);

        sccp_handle_button_template_req(d->session, NULL);

        astman_append(s, "Done\r\n");
        astman_append(s, "\r\n");

        return 0;
}

#endif
