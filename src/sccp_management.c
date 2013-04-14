
/*!
 * \file        sccp_management.c
 * \brief       SCCP Management Class
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date: 2010-11-22 14:09:28 +0100 (Mon, 22 Nov 2010) $
 * $Revision: 2174 $  
 */
#include <config.h>
#ifdef CS_SCCP_MANAGER
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2174 $")

/*
 * Descriptions
 */
static char management_show_devices_desc[] = "Description: Lists SCCP devices in text format with details on current status.\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";
static char management_show_lines_desc[] = "Description: Lists SCCP lines in text format with details on current status.\n" "\n" "LinelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";
static char management_restart_devices_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device to restart\n";
static char management_show_device_add_line_desc[] = "Description: Lists SCCP devices in text format with details on current status.\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  Devicename: Name of device to restart.\n" "  Linename: Name of line";
static char management_device_update_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device\n";
static char management_device_set_dnd_desc[] = "Description: set dnd on device\n" "\n" "Variables:\n" "   Devicename: Name of device\n" "  DNDState: on (busy) / off / reject/ silent";
static char management_line_fwd_update_desc[] = "Description: update forward status for line\n" "\n" "Variables:\n" "  Devicename: Name of device\n" "  Linename: Name of line\n" "  Forwardtype: type of cfwd (all | busy | noAnswer)\n" "  Disable: yes Disable call forward (optional)\n" "  Number: number to forward calls (optional)";
static char management_fetch_config_metadata_desc[] = "Description: fetch configuration metadata\n" "\n" "Variables:\n" "  segment: Config Segment Name (if empty returns all segments).\n" "  option: OptionName (if empty returns all options in sement).";
static char management_startcall_desc[] = "Description: start a new call on a device/line\n" "\n" "Variables:\n" "  Devicename: Name of the Device\n"  "  Linename: Name of the line\n"  "  number: Number to call";
static char management_answercall_desc[] = "Description: answer a ringing channel\n" "\n" "Variables:\n" "  Devicename: Name of the Device\n"  "  channelId: Id of the channel to pickup\n";
static char management_hangupcall_desc[] = "Description: hangup a channel/call\n" "\n" "Variables:\n" "  channelId: Id of the Channel to hangup\n";
static char management_hold_desc[] = "Description: hold/resume a call\n" "\n" "Variables:\n" "  channelId: Id of the channel to hold/unhold\n" "  hold: hold=true / resume=false\n" "  Devicename: Name of the Device\n" "  SwapChannels: Swap channels when resuming and an active channel is present (true/false)\n";

void sccp_manager_eventListener(const sccp_event_t * event);

/*
 * Pre Declarations
 */
static int sccp_manager_show_devices(struct mansession *s, const struct message *m);
static int sccp_manager_show_lines(struct mansession *s, const struct message *m);
static int sccp_manager_restart_device(struct mansession *s, const struct message *m);
static int sccp_manager_device_add_line(struct mansession *s, const struct message *m);
static int sccp_manager_device_update(struct mansession *s, const struct message *m);
static int sccp_manager_device_set_dnd(struct mansession *s, const struct message *m);
static int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m);
static int sccp_manager_startCall(struct mansession *s, const struct message *m);
static int sccp_manager_answerCall(struct mansession *s, const struct message *m);
static int sccp_manager_hangupCall(struct mansession *s, const struct message *m);
static int sccp_manager_holdCall(struct mansession *s, const struct message *m);

#if HAVE_PBX_MANAGER_HOOK_H
static int sccp_asterisk_managerHookHelper(int category, const char *event, char *content);

static struct manager_custom_hook sccp_manager_hook = {
	.file = "chan_sccp",
	.helper = sccp_asterisk_managerHookHelper,
};
#endif

/*!
 * \brief Register management commands
 */
int sccp_register_management(void)
{
	int result = 0;

	/* Register manager commands */
#if ASTERISK_VERSION_NUMBER < 10600
#define _MAN_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#else
#define _MAN_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING
#endif

	result = pbx_manager_register("SCCPListDevices", _MAN_FLAGS, sccp_manager_show_devices, "List SCCP devices (text format)", management_show_devices_desc);
	result |= pbx_manager_register("SCCPListLines", _MAN_FLAGS, sccp_manager_show_lines, "List SCCP lines (text format)", management_show_lines_desc);
	result |= pbx_manager_register("SCCPDeviceRestart", _MAN_FLAGS, sccp_manager_restart_device, "Restart a given device", management_restart_devices_desc);
	result |= pbx_manager_register("SCCPDeviceAddLine", _MAN_FLAGS, sccp_manager_device_add_line, "add a line to device", management_show_device_add_line_desc);
	result |= pbx_manager_register("SCCPDeviceUpdate", _MAN_FLAGS, sccp_manager_device_update, "add a line to device", management_device_update_desc);
	result |= pbx_manager_register("SCCPDeviceSetDND", _MAN_FLAGS, sccp_manager_device_set_dnd, "set dnd on device", management_device_set_dnd_desc);
	result |= pbx_manager_register("SCCPLineForwardUpdate", _MAN_FLAGS, sccp_manager_line_fwd_update, "set call-forward on a line", management_line_fwd_update_desc);
	result |= pbx_manager_register("SCCPStartCall", _MAN_FLAGS, sccp_manager_startCall, "start a new call on device", management_startcall_desc);
	result |= pbx_manager_register("SCCPAnswerCall", _MAN_FLAGS, sccp_manager_answerCall, "answer a ringin channel", management_answercall_desc);
	result |= pbx_manager_register("SCCPHangupCall", _MAN_FLAGS, sccp_manager_hangupCall, "hangup a channel", management_hangupcall_desc);
	result |= pbx_manager_register("SCCPHoldCall", _MAN_FLAGS, sccp_manager_holdCall, "hold/unhold a call", management_hold_desc);
	result |= pbx_manager_register("SCCPConfigMetaData", _MAN_FLAGS, sccp_manager_config_metadata, "retrieve config metadata", management_fetch_config_metadata_desc);
#undef _MAN_FLAGS

#if HAVE_PBX_MANAGER_HOOK_H
	ast_manager_register_hook(&sccp_manager_hook);
#endif

	return result;
}

/*!
 * \brief Unregister management commands
 */
int sccp_unregister_management(void)
{
	int result = 0;

	result = pbx_manager_unregister("SCCPListDevices");
	result |= pbx_manager_unregister("SCCPDeviceRestart");
	result |= pbx_manager_unregister("SCCPDeviceAddLine");
	result |= pbx_manager_unregister("SCCPDeviceUpdate");
	result |= pbx_manager_unregister("SCCPDeviceSetDND");
	result |= pbx_manager_unregister("SCCPLineForwardUpdate");
	result |= pbx_manager_unregister("SCCPStartCall");
	result |= pbx_manager_unregister("SCCPAnswerCall");
	result |= pbx_manager_unregister("SCCPHangupCall");
	result |= pbx_manager_unregister("SCCPHoldCall");
	result |= pbx_manager_unregister("SCCPConfigMetaData");
#if HAVE_PBX_MANAGER_HOOK_H
	ast_manager_unregister_hook(&sccp_manager_hook);
#endif

	return result;
}

/*!
 * \brief starting manager-module
 */
void sccp_manager_module_start()
{
	sccp_event_subscribe(	SCCP_EVENT_DEVICE_ATTACHED | 
				SCCP_EVENT_DEVICE_DETACHED | 
				SCCP_EVENT_DEVICE_PREREGISTERED | 
				SCCP_EVENT_DEVICE_REGISTERED | 
				SCCP_EVENT_DEVICE_UNREGISTERED | 
				SCCP_EVENT_FEATURE_CHANGED, 
				sccp_manager_eventListener, TRUE
	);
}

/*!
 * \brief stop manager-module
 *
 * \lock
 *      - sccp_hint_subscriptions
 */
void sccp_manager_module_stop()
{
	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_PREREGISTERED | SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED, sccp_manager_eventListener);
}

/*!
 * \brief Event Listener
 *
 * Handles the manager events that need to be posted when an event happens
 */
void sccp_manager_eventListener(const sccp_event_t * event)
{
	sccp_device_t *device = NULL;
	sccp_linedevices_t *linedevice = NULL;

	if (!event)
		return;

	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(
					EVENT_FLAG_CALL, 
					"DeviceStatus", 
					"ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n", 
					"REGISTERED", 
					DEV_ID_LOG(device)
				);
			break;

		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(
					EVENT_FLAG_CALL,
					"DeviceStatus",
					"ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n",
					"UNREGISTERED",
					DEV_ID_LOG(device)
				);
			break;

		case SCCP_EVENT_DEVICE_PREREGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(
					EVENT_FLAG_CALL,
					"DeviceStatus",
					"ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n",
					"PREREGISTERED",
					DEV_ID_LOG(device)
				);
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
			device = event->event.deviceAttached.linedevice->device;				// already retained in the event
			linedevice = event->event.deviceAttached.linedevice;					// already retained in the event
			manager_event(
					EVENT_FLAG_CALL,
					"PeerStatus",
					"ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nPeerStatus: %s\r\nSCCPDevice: %s\r\nSCCPLine: %s\r\nSCCPLineName: %s\r\nSubscriptionId: %s\r\nSubscriptionName: %s\r\n",
					"ATTACHED",
					DEV_ID_LOG(device),
					linedevice && linedevice->line ? linedevice->line->name : "(null)",
					linedevice && linedevice->line ? linedevice->line->label : "(null)",
					linedevice->subscriptionId.number ? linedevice->subscriptionId.number : "(null)",
					linedevice->subscriptionId.name ? linedevice->subscriptionId.name : "(null)"
				);
			break;

		case SCCP_EVENT_DEVICE_DETACHED:
			device = event->event.deviceAttached.linedevice->device;				// already retained in the event
			linedevice = event->event.deviceAttached.linedevice;					// already retained in the event
			manager_event(
					EVENT_FLAG_CALL,
					"PeerStatus",
					"ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nPeerStatus: %s\r\nSCCPDevice: %s\r\nSCCPLine: %s\r\nSCCPLineName: %s\r\nSubscriptionId: %s\r\nSubscriptionName: %s\r\n",
					"DETACHED",
					DEV_ID_LOG(device),
					linedevice && linedevice->line ? linedevice->line->name : "(null)",
					linedevice && linedevice->line ? linedevice->line->label : "(null)",
					linedevice->subscriptionId.number ? linedevice->subscriptionId.number : "(null)",
					linedevice->subscriptionId.name ? linedevice->subscriptionId.name : "(null)"
				);
			break;

		case SCCP_EVENT_FEATURE_CHANGED:
			device = event->event.featureChanged.device;						// already retained in the event
			linedevice = event->event.featureChanged.linedevice;					// already retained in the event
			sccp_feature_type_t featureType = event->event.featureChanged.featureType;
			switch(featureType) {
				case SCCP_FEATURE_DND:
					manager_event(	
							EVENT_FLAG_CALL, 
							"DND", 
							"ChannelType: SCCP\r\nChannelObjectType: Device\r\nFeature: %s\r\nStatus: %s\r\nSCCPDevice: %s\r\n", 
							featureType2str(SCCP_FEATURE_DND), 
							dndmode2str(device->dndFeature.status), 
							DEV_ID_LOG(device)
						);
					break;
				case SCCP_FEATURE_CFWDALL:
				case SCCP_FEATURE_CFWDBUSY:
					if(linedevice) {
						manager_event(
								EVENT_FLAG_CALL,
								"CallForward",
								"ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nFeature: %s\r\nStatus: %s\r\nExtension: %s\r\nSCCPLine: %s\r\nSCCPDevice: %s\r\n",
								featureType2str(featureType),
								(SCCP_FEATURE_CFWDALL == featureType) ? ((linedevice->cfwdAll.enabled) ? "On" : "Off") : ((linedevice->cfwdBusy.enabled) ? "On" : "Off"),
								(SCCP_FEATURE_CFWDALL == featureType) ? ((linedevice->cfwdAll.number) ? linedevice->cfwdAll.number : "(null)") : ((linedevice->cfwdBusy.number) ? linedevice->cfwdBusy.number : "(null)"),
								(linedevice->line) ? linedevice->line->name : "(null)",
								DEV_ID_LOG(device)
						);
					}
					break;
				case SCCP_FEATURE_CFWDNONE:
					manager_event(
							EVENT_FLAG_CALL,
							"CallForward",
							"ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nFeature: %s\r\nStatus: Off\r\nSCCPLine: %s\r\nSCCPDevice: %s\r\n",
							featureType2str(featureType),
							(linedevice && linedevice->line) ? linedevice->line->name : "(null)",
							DEV_ID_LOG(device)
						);
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

/*!
 * \brief Show Devices Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 *      - devices
 */
int sccp_manager_show_devices(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	sccp_device_t *device = NULL;
	char idtext[256] = "";
	int total = 0;
	struct tm *timeinfo;
	char regtime[25];

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	pbxman_send_listack(s, m, "Device status list will follow", "start");
	// List the peers in separate manager events 
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), device, list) {
		timeinfo = localtime(&device->registrationTime);
		strftime(regtime, sizeof(regtime), "%c", timeinfo);
		astman_append(s, "Event: DeviceEntry\r\n%s", idtext);
		astman_append(s, "ChannelType: SCCP\r\n");
		astman_append(s, "ObjectId: %s\r\n", device->id);
		astman_append(s, "ObjectType: device\r\n");
		astman_append(s, "Description: %s\r\n", device->description);
		astman_append(s, "IPaddress: %s\r\n", (device->session) ? pbx_inet_ntoa(device->session->sin.sin_addr) : "--");
		astman_append(s, "Reg_Status: %s\r\n", deviceregistrationstatus2str(device->registrationState));
		astman_append(s, "Reg_Time: %s\r\n", regtime);
		astman_append(s, "Active: %s\r\n", (device->active_channel) ? "Yes" : "No");
		astman_append(s, "NumLines: %d\r\n", device->configurationStatistic.numberOfLines);
		total++;
	}

	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	// Send final confirmation 
	astman_append(s, "Event: SCCPListDevicesComplete\r\n" "EventList: Complete\r\n" "ListItems: %d\r\n" "\r\n", total);

	return 0;
}

/*!
 * \brief Show Lines Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 *      - lines
 */
int sccp_manager_show_lines(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	sccp_line_t *line;
	char idtext[256] = "";
	int total = 0;

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	pbxman_send_listack(s, m, "Device status list will follow", "start");
	/* List the peers in separate manager events */
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), line, list) {
		astman_append(s, "Event: LineEntry\r\n%s", idtext);
		astman_append(s, "ChannelType: SCCP\r\n");
		astman_append(s, "ObjectId: %s\r\n", line->id);
		astman_append(s, "ObjectType: line\r\n");
		astman_append(s, "Name: %s\r\n", line->name);
		astman_append(s, "Description: %s\r\n", line->description);
		astman_append(s, "Num_Channels: %d\r\n", SCCP_RWLIST_GETSIZE(line->channels));
		total++;
	}

	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	/* Send final confirmation */
	astman_append(s, "Event: SCCPListLinesComplete\r\n" "EventList: Complete\r\n" "ListItems: %d\r\n" "\r\n", total);
	return 0;
}

/*!
 * \brief Restart Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
int sccp_manager_restart_device(struct mansession *s, const struct message *m)
{
	//      sccp_list_t     *hintList = NULL;
	sccp_device_t *d = NULL;
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *type = astman_get_header(m, "Type");

	pbx_log(LOG_WARNING, "Attempt to get device %s\n", deviceName);
	if (sccp_strlen_zero(deviceName)) {		
		astman_send_error(s, m, "Please specify the name of device to be reset");
		return 0;
	}

	pbx_log(LOG_WARNING, "Type of Restart ([quick|reset] or [full|restart]) %s\n", deviceName);
	if (sccp_strlen_zero(deviceName)) {
		pbx_log(LOG_WARNING, "Type not specified, using quick");
		type = "quick";
	}

	d = sccp_device_find_byid(deviceName, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not registered");
		d = sccp_device_release(d);
		return 0;
	}

	if (!strncasecmp(type, "full", 4) || !strncasecmp(type, "reset", 5)) {
		sccp_device_sendReset(d, SKINNY_DEVICE_RESET);
	} else {
		sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
	}

	astman_send_ack(s, m, "Device restarted");
	//astman_append(s, "Send %s restart to device %s\r\n", type, deviceName);
	//astman_append(s, "\r\n");
	d = sccp_device_release(d);

	return 0;
}

/*!
 * \brief Add Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_device_add_line(struct mansession *s, const struct message *m)
{
	sccp_device_t *d = NULL;
	sccp_line_t *line = NULL;
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");

	pbx_log(LOG_WARNING, "Attempt to get device %s\n", deviceName);

	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Please specify the name of device");
		return 0;
	}

	if (sccp_strlen_zero(lineName)) {
		astman_send_error(s, m, "Please specify the name of line to be added");
		return 0;
	}

	d = sccp_device_find_byid(deviceName, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	line = sccp_line_find_byname(lineName, TRUE);
	if (!line) {
		astman_send_error(s, m, "Line not found");
		d = sccp_device_release(d);
		return 0;
	}
	sccp_config_addButton(d, -1, LINE, line->name, NULL, NULL);
	astman_append(s, "Done\r\n");
	astman_append(s, "\r\n");
	line = sccp_line_release(line);
	d = sccp_device_release(d);

	return 0;
}

/*!
 * \brief Update Line Forward Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m)
{
	sccp_line_t *line = NULL;
	sccp_device_t *d = NULL;
	sccp_linedevices_t *linedevice = NULL;

	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");
	const char *forwardType = astman_get_header(m, "Forwardtype");
	const char *Disable = astman_get_header(m, "Disable");
	const char *number = astman_get_header(m, "Number");
	uint8_t cfwd_type = SCCP_CFWD_NONE;
	char cbuf[64] = "";

	d = sccp_device_find_byid(deviceName, TRUE);
	if (!d) {
		pbx_log(LOG_WARNING, "%s: Device not found\n", deviceName);
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	line = sccp_line_find_byname(lineName, TRUE);
	if (!line) {
		pbx_log(LOG_WARNING, "%s: Line %s not found\n", deviceName, lineName);
		astman_send_error(s, m, "Line not found");
		d = sccp_device_release(d);
		return 0;
	}

	if (line->devices.size > 1) {
		pbx_log(LOG_WARNING, "%s: Callforwarding on shared lines is not supported at the moment\n", deviceName);
		astman_send_error(s, m, "Callforwarding on shared lines is not supported at the moment");
		line = sccp_line_release(line);
		d = sccp_device_release(d);
		return 0;
	}

	if (!forwardType) {
		pbx_log(LOG_WARNING, "%s: Forwardtype is not optional [all | busy]\n", deviceName);
		astman_send_error(s, m, "Forwardtype is not optional [all | busy]");				/* NoAnswer to be added later on */
		line = sccp_line_release(line);
		d = sccp_device_release(d);
		return 0;
	}

	if (!Disable) {
		Disable = "no";
	}

	if (line) {
		if ((linedevice  = sccp_linedevice_find(d, line))) {
			if (sccp_strcaseequals("all", forwardType)) {
				if (sccp_strcaseequals("yes", Disable)) {
					linedevice->cfwdAll.enabled = 0;
					number = "";
				} else {
					linedevice->cfwdAll.enabled = 1;
					cfwd_type = SCCP_CFWD_ALL;
				}
				sccp_copy_string(linedevice->cfwdAll.number, number, sizeof(linedevice->cfwdAll.number));
			} else if (sccp_strcaseequals("busy", forwardType)) {
				if (sccp_strcaseequals("yes", Disable)) {
					linedevice->cfwdBusy.enabled = 0;
					number = "";
				} else {
					linedevice->cfwdBusy.enabled = 1;
					cfwd_type = SCCP_CFWD_BUSY;
				}
				sccp_copy_string(linedevice->cfwdBusy.number, number, sizeof(linedevice->cfwdBusy.number));
			} else if (sccp_strcaseequals("yes", Disable)) {
				linedevice->cfwdAll.enabled = 0;
				linedevice->cfwdBusy.enabled = 0;
				number = "";
				sccp_copy_string(linedevice->cfwdAll.number, number, sizeof(linedevice->cfwdAll.number));
				sccp_copy_string(linedevice->cfwdBusy.number, number, sizeof(linedevice->cfwdBusy.number));
			}
			switch (cfwd_type) {
				case SCCP_CFWD_ALL:
					sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDALL);
					snprintf (cbuf, sizeof(cbuf), "Line %s CallForward ALL set to %s", lineName, linedevice->cfwdAll.number);
					break;
				case SCCP_CFWD_BUSY:
					sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDBUSY);
					snprintf (cbuf, sizeof(cbuf), "Line %s CallForward BUSY set to %s", lineName, linedevice->cfwdBusy.number);
					break;
				case SCCP_CFWD_NONE:
				default:
					sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDNONE);
					snprintf (cbuf, sizeof(cbuf), "Line %s Call Forward Disabled", lineName);
					break;
			}
			sccp_dev_forward_status(line, linedevice->lineInstance, linedevice->device);
			sccp_linedevice_release(linedevice);
		} else {
			pbx_log(LOG_WARNING, "%s: LineDevice not found for line %s (Device not registeed ?)\n", deviceName, lineName);
			astman_send_error(s, m, "LineDevice not found (Device not registered ?)");
			line = sccp_line_release(line);
			d = sccp_device_release(d);
			return 0;
		}
	}
	astman_send_ack(s, m, cbuf);

	line = sccp_line_release(line);
	d = sccp_device_release(d);
	return 0;
}

/*!
 * \brief Update Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_device_update(struct mansession *s, const struct message *m)
{
	sccp_device_t *d = NULL;
	const char *deviceName = astman_get_header(m, "Devicename");

	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Please specify the name of device");
		return 0;
	}

	d = sccp_device_find_byid(deviceName, FALSE);

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not active");
		d = sccp_device_release(d);
		return 0;
	}

	sccp_handle_soft_key_template_req(d->session, d, NULL);

	sccp_handle_button_template_req(d->session, d, NULL);

	astman_send_ack(s, m, "Done");

	d = sccp_device_release(d);
	return 0;
}

/*!
 * \brief Set DND State on Device
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_device_set_dnd(struct mansession *s, const struct message *m) 
{
	sccp_device_t *d = NULL;
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *DNDState = astman_get_header(m, "DNDState");
	int prevStatus = 0;
	char retValStr[64] = "";

	/** we need the device for resuming calls */
	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Devicename variable is required.");
		return 0;
	}
	if (sccp_strlen_zero(DNDState)) {
		astman_send_error(s, m, "DNDState variable is required.");
		return 0;
	}
	
	//astman_append(s, "remove channel '%s' from hold\n", channelId);
	if ((d = sccp_device_find_byid(deviceName, FALSE))) {
		if (d->dndFeature.enabled) {	
			prevStatus = d->dndFeature.status;	
			if (sccp_strcaseequals("on", DNDState) || sccp_strcaseequals("reject", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_REJECT;
			} else if (sccp_strcaseequals("silent", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_SILENT;
			} else if (sccp_strcaseequals("off", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_OFF;
			} else {
				astman_send_error(s, m, "DNDState Variable has to be one of (on/off/reject/silent).");
			}

			if (d->dndFeature.status != prevStatus) {
				snprintf(retValStr, sizeof(retValStr), "Device %s DND has been set to %s", d->id, dndmode2str(d->dndFeature.status));
				sccp_feat_changed(d, NULL, SCCP_FEATURE_DND);
				sccp_dev_check_displayprompt(d);
			} else {
				snprintf(retValStr, sizeof(retValStr), "Device %s DND state unchanged", d->id);
			}
		} else {
			astman_send_error(s, m, "DND Feature not enabled on this device.");
		}
		d = d ? sccp_device_release(d) : NULL;
	} else {
		astman_send_error(s, m, "Device could not be found.");
		return 0;
	}

	astman_send_ack(s, m, retValStr);
	return 0;
}


/*!
 * \brief Start Call on Device, Line to Number
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_startCall(struct mansession *s, const struct message *m)
{
	sccp_device_t *d = NULL;
	sccp_line_t *line = NULL;
	sccp_channel_t *channel = NULL;

	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");
	const char *number = astman_get_header(m, "number");

	d = sccp_device_find_byid(deviceName, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!lineName) {
		if (d && d->defaultLineInstance > 0) {
			line = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			line = sccp_dev_get_activeline(d);
		}
	} else {
		line = sccp_line_find_byname(lineName, FALSE);
	}

	if (!line) {
		astman_send_error(s, m, "Line not found");
		d = sccp_device_release(d);
		return 0;
	}

	channel = sccp_channel_newcall(line, d, sccp_strlen_zero(number) ? NULL : (char *) number, SKINNY_CALLTYPE_OUTBOUND, NULL);
	astman_send_ack(s, m, "Call Started");
	line = sccp_line_release(line);
	d = sccp_device_release(d);
	channel = channel ? sccp_channel_release(channel) : NULL;
	return 0;
}

/*!
 * \brief Answer Call of ChannelId on Device
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_answerCall(struct mansession *s, const struct message *m)
{
	sccp_device_t *d = NULL;
	sccp_channel_t *c = NULL;

	const char *deviceName = astman_get_header(m, "Devicename");
	const char *channelId = astman_get_header(m, "channelId");

	d = sccp_device_find_byid(deviceName, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	c = sccp_channel_find_byid(atoi(channelId));

	if (!c) {
		astman_send_error(s, m, "Call not found\r\n");
		d = sccp_device_release(d);
		return 0;
	}

	if (c->state != SCCP_CHANNELSTATE_RINGING) {
		astman_send_error(s, m, "Call is not ringin\r\n");
		d = sccp_device_release(d);
		return 0;
	}

	astman_append(s, "Answering channel '%s'\r\n", channelId);

	sccp_channel_answer(d, c);

	astman_send_ack(s, m, "Call was Answered");
	c = sccp_channel_release(c);
	d = sccp_device_release(d);
	return 0;
}

/*!
 * \brief Hangup Call of ChannelId
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_hangupCall(struct mansession *s, const struct message *m)
{
	sccp_channel_t *c = NULL;

	const char *channelId = astman_get_header(m, "channelId");

	c = sccp_channel_find_byid(atoi(channelId));
	if (!c) {
		astman_send_error(s, m, "Call not found.");
		return 0;
	}
	//astman_append(s, "Hangup call '%s'\r\n", channelId);
	sccp_channel_endcall(c);
	astman_send_ack(s, m, "Call was hungup");
	c = sccp_channel_release(c);
	return 0;
}

/*!
 * \brief Put ChannelId Call on Hold (on/off)
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_holdCall(struct mansession *s, const struct message *m)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;
	const char *channelId = astman_get_header(m, "channelId");
	const char *hold = astman_get_header(m, "hold");
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *swap = astman_get_header(m, "SwapChannels");
	char *retValStr = "Channel was resumed";
	boolean_t errorMessage = TRUE;
	

	c = sccp_channel_find_byid(atoi(channelId));
	if (!c) {
		astman_send_error(s, m, "Call not found\r\n");
		return 0;
	}
	if (sccp_strcaseequals("on", hold)) {									/* check to see if enable hold */
		sccp_channel_hold(c);
		retValStr = "Channel was put on hold";
		errorMessage = FALSE;
	
	} else if (sccp_strcaseequals("off", hold)) {								/* check to see if disable hold */

		/** we need the device for resuming calls */
		if (sccp_strlen_zero(deviceName)) {
			retValStr = "To resume a channel, you need to specify the device that resumes call using Devicename variable.";
			goto SEND_RESPONSE;
		}

		if ((d = sccp_device_find_byid(deviceName, FALSE))) {
			if (sccp_strcaseequals("yes", swap)) {
				sccp_channel_resume(d, c, TRUE);
			} else {
				sccp_channel_resume(d, c, FALSE);
			}
			retValStr = "Channel was resumed";
			errorMessage = FALSE;
		} else {
			retValStr = "Device to hold/resume could not be found.";
		}
	} else {
		retValStr = "Invalid value for hold, use 'on' or 'off' only.";
	}
	
SEND_RESPONSE:
	if (errorMessage){
		astman_send_error(s, m, retValStr);
	} else {
		astman_send_ack(s, m, retValStr);
	}
	
	d = d ? sccp_device_release(d) : NULL;
	c = c ? sccp_channel_release(c): NULL;
	return 0;
}

#if HAVE_PBX_MANAGER_HOOK_H

/**
 * parse string from management hook to struct message
 * 
 */
static void sccp_asterisk_parseStrToAstMessage(char *str, struct message *m)
{
	int x = 0;
	int curlen;

	curlen = strlen(str);
	for (x = 0; x < curlen; x++) {
		int cr;												/* set if we have \r */

		if (str[x] == '\r' && x + 1 < curlen && str[x + 1] == '\n')
			cr = 2;											/* Found. Update length to include \r\n */
		else if (str[x] == '\n')
			cr = 1;											/* also accept \n only */
		else
			continue;
		/* don't keep empty lines */
		if (x && m->hdrcount < ARRAY_LEN(m->headers)) {
			/* ... but trim \r\n and terminate the header string */
			str[x] = '\0';
			m->headers[m->hdrcount++] = str;
		}
		x += cr;
		curlen -= x;											/* remaining size */
		str += x;											/* update pointer */
		x = -1;												/* reset loop */
	}
}

/**
 * 
 * 
 * 
 */
static int sccp_asterisk_managerHookHelper(int category, const char *event, char *content)
{

	struct message m = { 0 };
	PBX_CHANNEL_TYPE *pbxchannel = NULL;
	PBX_CHANNEL_TYPE *pbxBridge = NULL;

	sccp_channel_t *channel = NULL;
	sccp_device_t *d = NULL;
	char *str, *dupStr;

	if (EVENT_FLAG_CALL == category) {
		if (!strcasecmp("MonitorStart", event) || !strcasecmp("MonitorStop", event)) {

			str = dupStr = sccp_strdupa(content); /** need a dup, because converter to message structure will modify the str */

			sccp_asterisk_parseStrToAstMessage(str, &m); /** convert to message structure to use the astman_get_header function */
			const char *channelName = astman_get_header(&m, "Channel");

			pbxchannel = pbx_channel_get_by_name(channelName);

			if (pbxchannel && (CS_AST_CHANNEL_PVT_IS_SCCP(pbxchannel))) {
				channel = get_sccp_channel_from_pbx_channel(pbxchannel);
			} else if (pbxchannel && ((pbxBridge = pbx_channel_get_by_name(pbx_builtin_getvar_helper(pbxchannel, "BRIDGEPEER"))) != NULL) && (CS_AST_CHANNEL_PVT_IS_SCCP(pbxBridge))) {
				channel = get_sccp_channel_from_pbx_channel(pbxBridge);
			}

			if (channel) {
				if ((d = sccp_channel_getDevice_retained(channel))) {
					if (strcasecmp("MonitorStart", event)) {
						d->monitorFeature.status |= SCCP_FEATURE_MONITOR_STATE_ACTIVE;
					} else {
						d->monitorFeature.status &= ~SCCP_FEATURE_MONITOR_STATE_ACTIVE;
					}
					sccp_feat_changed(d, NULL, SCCP_FEATURE_MONITOR);
					d = sccp_device_release(d);
				}
				channel = sccp_channel_release(channel);
			}
		}
	}
	return 0;
}
#endif														/* HAVE_PBX_MANAGER_HOOK_H */

#endif														/* CS_SCCP_MANAGER */
