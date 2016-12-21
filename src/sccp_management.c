/*!
 * \file        sccp_management.c
 * \brief       SCCP Management Class
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#ifdef CS_SCCP_MANAGER
#include "common.h"
#include "sccp_channel.h"
#include "sccp_actions.h"
#include "sccp_config.h"
#include "sccp_device.h"
#include "sccp_features.h"
#include "sccp_line.h"
#include "sccp_management.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include "sccp_featureParkingLot.h"
#include <asterisk/threadstorage.h>

SCCP_FILE_VERSION(__FILE__, "");

/*
 * Descriptions
 */
static char management_show_devices_desc[] = "Description: Lists SCCP devices in text format with details on current status. (DEPRECATED in favor of SCCPShowDevices)\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";
static char management_show_lines_desc[] = "Description: Lists SCCP lines in text format with details on current status. (DEPRECATED in favor of SCCPShowLines)\n" "\n" "LinelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";
static char management_restart_devices_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device to restart\n";
static char management_show_device_add_line_desc[] = "Description: Lists SCCP devices in text format with details on current status.\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  Devicename: Name of device to restart.\n" "  Linename: Name of line";
static char management_device_update_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device\n";
static char management_device_set_dnd_desc[] = "Description: set dnd on device\n" "\n" "Variables:\n" "   Devicename: Name of device\n" "  DNDState: off / reject / silent";
static char management_line_fwd_update_desc[] = "Description: update forward status for line\n" "\n" "Variables:\n" "  Devicename: Name of device\n" "  Linename: Name of line\n" "  Forwardtype: type of cfwd (all | busy | noAnswer)\n" "  Disable: yes Disable call forward (optional)\n" "  Number: number to forward calls (optional)";
static char management_fetch_config_metadata_desc[] = "Description: fetch configuration metadata\n" "\n" "Variables:\n" "  segment: Config Segment Name (if empty returns all segments).\n";

#if ASTERISK_VERSION_GROUP >= 112
static char management_startcall_desc[] = "Description: start a new call on a device/line\n" "\n" "Variables:\n" "  Devicename: Name of the Device\n" "  Linename: Name of the line\n" "  number: Number to call\n" "  ChannelId: Channel UniqueId to be set on the channel\n";
#else
static char management_startcall_desc[] = "Description: start a new call on a device/line\n" "\n" "Variables:\n" "  Devicename: Name of the Device\n" "  Linename: Name of the line\n" "  number: Number to call";
#endif
static char management_answercall_desc[] = "Description: answer a ringing channel. (DEPRECATED in favor of SCCPAnswerCall1)\n" "\n" "Variables:\n" "  Devicename: Name of the Device\n" "  channelId: Id of the channel to pickup\n";
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
static int sccp_manager_answerCall2(struct mansession *s, const struct message *m);
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
 * \note deprecated
 */
int sccp_register_management(void)
{
	int result = 0;

	/* Register manager commands */
#if ASTERISK_VERSION_NUMBER < 10600
#define _MAN_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#else
#define _MAN_FLAGS	(EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING)
#endif

	result = pbx_manager_register("SCCPListDevices", _MAN_FLAGS, sccp_manager_show_devices, "List SCCP devices", management_show_devices_desc);
	result |= pbx_manager_register("SCCPListLines", _MAN_FLAGS, sccp_manager_show_lines, "List SCCP lines", management_show_lines_desc);
	result |= pbx_manager_register("SCCPDeviceRestart", _MAN_FLAGS, sccp_manager_restart_device, "Restart a given device", management_restart_devices_desc);
	result |= pbx_manager_register("SCCPDeviceAddLine", _MAN_FLAGS, sccp_manager_device_add_line, "add a line to device", management_show_device_add_line_desc);
	result |= pbx_manager_register("SCCPDeviceUpdate", _MAN_FLAGS, sccp_manager_device_update, "add a line to device", management_device_update_desc);
	result |= pbx_manager_register("SCCPDeviceSetDND", _MAN_FLAGS, sccp_manager_device_set_dnd, "set dnd on device", management_device_set_dnd_desc);
	result |= pbx_manager_register("SCCPLineForwardUpdate", _MAN_FLAGS, sccp_manager_line_fwd_update, "set call-forward on a line", management_line_fwd_update_desc);
	result |= pbx_manager_register("SCCPStartCall", _MAN_FLAGS, sccp_manager_startCall, "start a new call on device", management_startcall_desc);
	result |= pbx_manager_register("SCCPAnswerCall", _MAN_FLAGS, sccp_manager_answerCall2, "answer a ringin channel", management_answercall_desc);
	result |= pbx_manager_register("SCCPHangupCall", _MAN_FLAGS, sccp_manager_hangupCall, "hangup a channel", management_hangupcall_desc);
	result |= pbx_manager_register("SCCPHoldCall", _MAN_FLAGS, sccp_manager_holdCall, "hold/unhold a call", management_hold_desc);
	result |= pbx_manager_register("SCCPConfigMetaData", _MAN_FLAGS, sccp_manager_config_metadata, "retrieve config metadata in json format", management_fetch_config_metadata_desc);
#undef _MAN_FLAGS

#if HAVE_PBX_MANAGER_HOOK_H
	ast_manager_register_hook(&sccp_manager_hook);
#else
#warning "manager_custom_hook not found, monitor indication does not work properly"
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
	result |= pbx_manager_unregister("SCCPListLines");
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
void sccp_manager_module_start(void)
{
	sccp_event_subscribe(SCCP_EVENT_DEVICE_ATTACHED | SCCP_EVENT_DEVICE_DETACHED | SCCP_EVENT_DEVICE_PREREGISTERED | SCCP_EVENT_DEVICE_REGISTERED | SCCP_EVENT_DEVICE_UNREGISTERED | SCCP_EVENT_FEATURE_CHANGED, sccp_manager_eventListener, TRUE);
}

/*!
 * \brief stop manager-module
 *
 */
void sccp_manager_module_stop(void)
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

	if (!event) {
		return;
	}
	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(EVENT_FLAG_CALL, "DeviceStatus", "ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n", "REGISTERED", DEV_ID_LOG(device));
			break;

		case SCCP_EVENT_DEVICE_UNREGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(EVENT_FLAG_CALL, "DeviceStatus", "ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n", "UNREGISTERED", DEV_ID_LOG(device));
			break;

		case SCCP_EVENT_DEVICE_PREREGISTERED:
			device = event->event.deviceRegistered.device;						// already retained in the event
			manager_event(EVENT_FLAG_CALL, "DeviceStatus", "ChannelType: SCCP\r\nChannelObjectType: Device\r\nDeviceStatus: %s\r\nSCCPDevice: %s\r\n", "PREREGISTERED", DEV_ID_LOG(device));
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
			device = event->event.deviceAttached.linedevice->device;				// already retained in the event
			linedevice = event->event.deviceAttached.linedevice;					// already retained in the event
			manager_event(EVENT_FLAG_CALL,
				      "PeerStatus",
				      "ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nPeerStatus: %s\r\nSCCPDevice: %s\r\nSCCPLine: %s\r\nSCCPLineName: %s\r\nSubscriptionId: %s\r\nSubscriptionName: %s\r\n",
				      "ATTACHED", DEV_ID_LOG(device), linedevice && linedevice->line ? linedevice->line->name : "(null)", (linedevice && linedevice->line && linedevice->line->label) ? linedevice->line->label : "(null)", linedevice->subscriptionId.number, linedevice->subscriptionId.name);
			break;

		case SCCP_EVENT_DEVICE_DETACHED:
			device = event->event.deviceAttached.linedevice->device;				// already retained in the event
			linedevice = event->event.deviceAttached.linedevice;					// already retained in the event
			manager_event(EVENT_FLAG_CALL,
				      "PeerStatus",
				      "ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nPeerStatus: %s\r\nSCCPDevice: %s\r\nSCCPLine: %s\r\nSCCPLineName: %s\r\nSubscriptionId: %s\r\nSubscriptionName: %s\r\n",
				      "DETACHED", DEV_ID_LOG(device), linedevice && linedevice->line ? linedevice->line->name : "(null)", (linedevice && linedevice->line && linedevice->line->label) ? linedevice->line->label : "(null)", linedevice->subscriptionId.number, linedevice->subscriptionId.name);
			break;

		case SCCP_EVENT_FEATURE_CHANGED:
			device = event->event.featureChanged.device;						// already retained in the event
			linedevice = event->event.featureChanged.optional_linedevice;				// either NULL or already retained in the event
			sccp_feature_type_t featureType = event->event.featureChanged.featureType;

			switch (featureType) {
				case SCCP_FEATURE_DND:
					manager_event(EVENT_FLAG_CALL, "DND", "ChannelType: SCCP\r\nChannelObjectType: Device\r\nFeature: %s\r\nStatus: %s\r\nSCCPDevice: %s\r\n", sccp_feature_type2str(SCCP_FEATURE_DND), sccp_dndmode2str(device->dndFeature.status), DEV_ID_LOG(device));
					break;
				case SCCP_FEATURE_CFWDALL:
				case SCCP_FEATURE_CFWDBUSY:
					if (linedevice) {
						manager_event(EVENT_FLAG_CALL,
							      "CallForward",
							      "ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nFeature: %s\r\nStatus: %s\r\nExtension: %s\r\nSCCPLine: %s\r\nSCCPDevice: %s\r\n",
							      sccp_feature_type2str(featureType), (SCCP_FEATURE_CFWDALL == featureType) ? ((linedevice->cfwdAll.enabled) ? "On" : "Off") : ((linedevice->cfwdBusy.enabled) ? "On" : "Off"), (SCCP_FEATURE_CFWDALL == featureType) ? linedevice->cfwdAll.number : linedevice->cfwdBusy.number, (linedevice->line) ? linedevice->line->name : "(null)", DEV_ID_LOG(device)
						    );
					}
					break;
				case SCCP_FEATURE_CFWDNONE:
					manager_event(EVENT_FLAG_CALL, "CallForward", "ChannelType: SCCP\r\nChannelObjectType: DeviceLine\r\nFeature: %s\r\nStatus: Off\r\nSCCPLine: %s\r\nSCCPDevice: %s\r\n", sccp_feature_type2str(featureType), (linedevice && linedevice->line) ? linedevice->line->name : "(null)", DEV_ID_LOG(device));
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
 * \note deprecated
 * 
 */
static int sccp_manager_show_devices(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	sccp_device_t *device = NULL;
	char idtext[256] = "";
	int total = 0;
	struct tm *timeinfo;
	char regtime[25];
	char clientAddress[INET6_ADDRSTRLEN];

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	pbxman_send_listack(s, m, "Device status list will follow", "start");
	// List the peers in separate manager events 
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), device, list) {
		timeinfo = localtime(&device->registrationTime);

		struct sockaddr_storage sas = { 0 };
		if (sccp_session_getSas(device->session, &sas)) {
			sccp_copy_string(clientAddress, sccp_netsock_stringify(&sas), sizeof(clientAddress));
		} else {
			sccp_copy_string(clientAddress, "--", sizeof(clientAddress));
		}

		strftime(regtime, sizeof(regtime), "%c", timeinfo);
		astman_append(s, "Event: DeviceEntry\r\n%s", idtext);
		astman_append(s, "ChannelType: SCCP\r\n");
		astman_append(s, "ObjectId: %s\r\n", device->id);
		astman_append(s, "ObjectType: device\r\n");
		astman_append(s, "Description: %s\r\n", device->description  ? device->description : "<not set>");
		astman_append(s, "IPaddress: %s\r\n", clientAddress);
		astman_append(s, "Reg_Status: %s\r\n", skinny_registrationstate2str(sccp_device_getRegistrationState(device)));
		astman_append(s, "Reg_Time: %s\r\n", regtime);
		astman_append(s, "Active: %s\r\n", (device->active_channel) ? "Yes" : "No");
		astman_append(s, "NumLines: %d\r\n\r\n", device->configurationStatistic.numberOfLines);
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
 * \note deprecated
 * 
 */
static int sccp_manager_show_lines(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	sccp_line_t *line = NULL;
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
		astman_append(s, "Description: %s\r\n", line->description  ? line->description : "<not set>");
		astman_append(s, "Num_Channels: %d\r\n\r\n", SCCP_RWLIST_GETSIZE(&line->channels));
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
static int sccp_manager_restart_device(struct mansession *s, const struct message *m)
{
	// sccp_list_t *hintList = NULL;
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

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not registered");
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

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(lineName, TRUE));

	if (!line) {
		astman_send_error(s, m, "Line not found");
		return 0;
	}
	if (sccp_config_addButton(&d->buttonconfig, -1, LINE, line->name, NULL, NULL) == SCCP_CONFIG_CHANGE_CHANGED) {
		d->pendingUpdate = 1;
		sccp_config_addButton(&d->buttonconfig, -1, LINE, line->name, NULL, NULL);
		sccp_device_check_update(d);
		astman_append(s, "Done\r\n");
		astman_append(s, "\r\n");
	} else {
		astman_send_error(s, m, "Adding line button to device failed");
	}
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
static int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m)
{
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");
	const char *forwardType = astman_get_header(m, "Forwardtype");
	const char *Disable = astman_get_header(m, "Disable");
	const char *number = astman_get_header(m, "Number");
	sccp_callforward_t cfwd_type = SCCP_CFWD_NONE;
	char cbuf[64] = "";

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (!d) {
		pbx_log(LOG_WARNING, "%s: Device not found\n", deviceName);
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(lineName, TRUE));

	if (!line) {
		pbx_log(LOG_WARNING, "%s: Line %s not found\n", deviceName, lineName);
		astman_send_error(s, m, "Line not found");
		return 0;
	}

	if (SCCP_LIST_GETSIZE(&line->devices) > 1) {
		pbx_log(LOG_WARNING, "%s: Callforwarding on shared lines is not supported at the moment\n", deviceName);
		astman_send_error(s, m, "Callforwarding on shared lines is not supported at the moment");
		return 0;
	}

	if (!forwardType) {
		pbx_log(LOG_WARNING, "%s: Forwardtype is not optional [all | busy]\n", deviceName);
		astman_send_error(s, m, "Forwardtype is not optional [all | busy]");				/* NoAnswer to be added later on */
		return 0;
	}

	if (!Disable) {
		Disable = "no";
	}

	if (line) {
		AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, line));

		if (linedevice) {
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
					snprintf(cbuf, sizeof(cbuf), "Line %s CallForward ALL set to %s", lineName, linedevice->cfwdAll.number);
					break;
				case SCCP_CFWD_BUSY:
					sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDBUSY);
					snprintf(cbuf, sizeof(cbuf), "Line %s CallForward BUSY set to %s", lineName, linedevice->cfwdBusy.number);
					break;
				case SCCP_CFWD_NONE:
				default:
					sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDNONE);
					snprintf(cbuf, sizeof(cbuf), "Line %s Call Forward Disabled", lineName);
					break;
			}
			sccp_dev_forward_status(line, linedevice->lineInstance, linedevice->device);
		} else {
			pbx_log(LOG_WARNING, "%s: LineDevice not found for line %s (Device not registeed ?)\n", deviceName, lineName);
			astman_send_error(s, m, "LineDevice not found (Device not registered ?)");
			return 0;
		}
	}
	astman_send_ack(s, m, cbuf);
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
	const char *deviceName = astman_get_header(m, "Devicename");

	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Please specify the name of device");
		return 0;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not active");
		return 0;
	}

	sccp_handle_soft_key_template_req(d->session, d, NULL);

	sccp_handle_button_template_req(d->session, d, NULL);

	astman_send_ack(s, m, "Done");
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
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *DNDState = astman_get_header(m, "DNDState");
	uint prevStatus = 0;
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
	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (d) {
		if (d->dndFeature.enabled) {
			prevStatus = d->dndFeature.status;
			if (sccp_strcaseequals("reject", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_REJECT;
			} else if (sccp_strcaseequals("silent", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_SILENT;
			} else if (sccp_strcaseequals("off", DNDState)) {
				d->dndFeature.status = SCCP_DNDMODE_OFF;
			} else {
				astman_send_error(s, m, "DNDState Variable has to be one of (on/off/reject/silent).");
			}

			if (d->dndFeature.status != prevStatus) {
				snprintf(retValStr, sizeof(retValStr), "Device %s DND has been set to %s", d->id, sccp_dndmode2str(d->dndFeature.status));
				sccp_feat_changed(d, NULL, SCCP_FEATURE_DND);
				sccp_dev_check_displayprompt(d);
			} else {
				snprintf(retValStr, sizeof(retValStr), "Device %s DND state unchanged", d->id);
			}
		} else {
			astman_send_error(s, m, "DND Feature not enabled on this device.");
		}
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
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");
	const char *number = astman_get_header(m, "number");

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	AUTO_RELEASE(sccp_line_t, line , NULL);

	if (!lineName) {
		if (d && d->defaultLineInstance > 0) {
			line = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			line = sccp_dev_getActiveLine(d);
		}
	} else {
		line = sccp_line_find_byname(lineName, FALSE);
	}

	if (!line) {
		astman_send_error(s, m, "Line not found");
		return 0;
	}

	AUTO_RELEASE(sccp_channel_t, new_channel , NULL);

#if ASTERISK_VERSION_GROUP >= 112
	struct ast_assigned_ids ids = {
		.uniqueid = astman_get_header(m, "ChannelId"),
		//.uniqueid2 = astman_get_header(m, "OtherChannelId")
	};
	if ((ids.uniqueid && AST_MAX_PUBLIC_UNIQUEID < sccp_strlen(ids.uniqueid))
	    //|| (ids.uniqueid2 && AST_MAX_PUBLIC_UNIQUEID < sccp_strlen(ids.uniqueid2))
	    ) {
		astman_send_error_va(s, m, "Uniqueid length exceeds maximum of %d\n", AST_MAX_PUBLIC_UNIQUEID);
		return 0;
	}
	new_channel = sccp_channel_newcall(line, d, sccp_strlen_zero(number) ? NULL : (char *) number, SKINNY_CALLTYPE_OUTBOUND, NULL, (ids.uniqueid) ? &ids : NULL);
#else
	new_channel = sccp_channel_newcall(line, d, sccp_strlen_zero(number) ? NULL : (char *) number, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
#endif
	astman_send_ack(s, m, "Call Started");
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
static int sccp_manager_answerCall2(struct mansession *s, const struct message *m)
{
	char retValStr[64] = "";

	const char *deviceName = astman_get_header(m, "Devicename");
	const char *channelId = astman_get_header(m, "channelId");
	int channelIntId = sccp_atoi(channelId, strlen(channelId));

	if (channelIntId == 0) {
		snprintf(retValStr, sizeof(retValStr), "Channel Id has to be a number. You have provided: '%s'\r\n", channelId);
		astman_send_error(s, m, retValStr);
		return 0;
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(channelIntId));

	if (c) {
		AUTO_RELEASE(sccp_device_t, d , NULL);

		if (sccp_strlen_zero(deviceName)) {
			d = sccp_channel_getDevice(c);
		} else {
			d = sccp_device_find_byid(deviceName, FALSE);
		}
		if (d) {
			if (c->state == SCCP_CHANNELSTATE_RINGING) {
				sccp_channel_answer(d, c);
				if (c->owner) {
					iPbx.queue_control(c->owner, AST_CONTROL_ANSWER);
				}
				snprintf(retValStr, sizeof(retValStr), "Answered channel '%s' on device '%s'\r\n", channelId, deviceName);
				astman_send_ack(s, m, retValStr);
			} else {
				astman_send_error(s, m, "Call is not ringing\r\n");
			}
		} else {
			astman_send_error(s, m, "Device not found");
		}
	} else {
		astman_send_error(s, m, "Call not found\r\n");
	}
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
	const char *channelId = astman_get_header(m, "channelId");
	int channelIntId = sccp_atoi(channelId, strlen(channelId));

	if (channelIntId == 0) {
		astman_send_error(s, m, "Channel Id has to be a number.");
		return 0;
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(channelIntId));

	if (!c) {
		astman_send_error(s, m, "Call not found.");
		return 0;
	}
	//astman_append(s, "Hangup call '%s'\r\n", channelId);
	sccp_channel_endcall(c);
	astman_send_ack(s, m, "Call was hungup");
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
	const char *channelId = astman_get_header(m, "channelId");
	int channelIntId = sccp_atoi(channelId, strlen(channelId));
	const char *hold = astman_get_header(m, "hold");
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *swap = astman_get_header(m, "SwapChannels");
	static char *retValStr = "Channel was resumed";
	boolean_t errorMessage = TRUE;

	if (channelIntId == 0) {
		astman_send_error(s, m, "Channel Id has to be a number\r\n");
		return 0;
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(channelIntId));

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
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(deviceName, FALSE));

		if (d) {
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
	if (errorMessage) {
		astman_send_error(s, m, retValStr);
	} else {
		astman_send_ack(s, m, retValStr);
	}
	return 0;
}

#if HAVE_PBX_MANAGER_HOOK_H
/*!
 * \brief parse string from management hook to struct message
 * \note side effect: this function changes/consumes the str pointer
 */
static char * sccp_asterisk_parseStrToAstMessage(char *str, struct message *m)
{
	int x = 0;
	int curlen;

	curlen = sccp_strlen(str);
	for (x = 0; x < curlen; x++) {
		int cr;												/* set if we have \r */

		if (str[x] == '\r' && x + 1 < curlen && str[x + 1] == '\n') {
			cr = 2;											/* Found. Update length to include \r\n */
		} else if (str[x] == '\n') {
			cr = 1;											/* also accept \n only */
		} else {
			continue;
		}
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
	return str;
}

/*!
 * \brief HookHelper to parse AMI events
 * Used to check for Monitor Stop/Start events
 */
static int sccp_asterisk_managerHookHelper(int category, const char *event, char *content)
{
	char *str, *dupStr;

	if (EVENT_FLAG_CALL == category) {
		if (!strcasecmp("MonitorStart", event) || !strcasecmp("MonitorStop", event)) {
			AUTO_RELEASE(sccp_channel_t, channel , NULL);
			struct message m = { 0 };

			str = dupStr = pbx_strdupa(content); /** need a dup, because converter to message structure will modify the str */
			sccp_log(DEBUGCAT_CORE)("SCCP: (managerHookHelper) MonitorStart/MonitorStop Received\ncontent:[%s]\n", content);	/* temp */

			sccp_asterisk_parseStrToAstMessage(str, &m); /** convert to message structure to use the astman_get_header function */
			const char *channelName = astman_get_header(&m, "Channel");

			PBX_CHANNEL_TYPE *pbxchannel = pbx_channel_get_by_name(channelName);							/* returns reffed */
			if (pbxchannel) {
				PBX_CHANNEL_TYPE *pbxBridge = NULL;
				if ((CS_AST_CHANNEL_PVT_IS_SCCP(pbxchannel))) {
					channel = get_sccp_channel_from_pbx_channel(pbxchannel);
				} else if ( (pbxBridge = pbx_channel_get_by_name(pbx_builtin_getvar_helper(pbxchannel, "BRIDGEPEER"))) ) {
					if ((CS_AST_CHANNEL_PVT_IS_SCCP(pbxBridge))) {
						channel = get_sccp_channel_from_pbx_channel(pbxBridge);
					}
#if ASTERISK_VERSION_GROUP == 106
					pbx_channel_unlock(pbxBridge);
#else
					pbxBridge = ast_channel_unref(pbxBridge);
#endif
				}
#if ASTERISK_VERSION_GROUP == 106
				pbx_channel_unlock(pbxchannel);
#else
				pbxchannel = ast_channel_unref(pbxchannel);
#endif
			}

			if (channel) {
				sccp_log(DEBUGCAT_CORE)("%s: (managerHookHelper) MonitorStart/MonitorStop Received\n", channel->designator);	/* temp */
				AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(channel));
				if (d) {
					sccp_log(DEBUGCAT_CORE)("%s: (managerHookHelper) MonitorStart/MonitorStop on Device: %s\n", channel->designator, d->id);	/* temp */
					if (!strcasecmp("MonitorStart", event)) {
						d->monitorFeature.status |= SCCP_FEATURE_MONITOR_STATE_ACTIVE;
					} else {
						d->monitorFeature.status &= ~SCCP_FEATURE_MONITOR_STATE_ACTIVE;
					}
					sccp_msg_t *msg_out = NULL;
					REQ(msg_out, RecordingStatusMessage);
					msg_out->data.RecordingStatusMessage.lel_callReference = htolel(channel->callid);
					msg_out->data.RecordingStatusMessage.lel_status = (d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE) ? htolel(1) : htolel(0);
					sccp_dev_send(d, msg_out);

					sccp_feat_changed(d, NULL, SCCP_FEATURE_MONITOR);
				}
			}
#ifdef CS_SCCP_PARK
		} else if (sccp_strcaseequals("ParkedCall", event) || sccp_strcaseequals("UnParkedCall", event) || sccp_strcaseequals("ParkedCallGiveUp", event) || sccp_strcaseequals("ParkedCallTimeout", event)) {
			if (iParkingLot.addSlot && iParkingLot.removeSlot) {
				sccp_log(DEBUGCAT_CORE)("SCCP: (managerHookHelper) %s Received\ncontent:[%s]\n", event, content);

				str = dupStr = pbx_strdupa(content);
				struct message m = { 0 };
				sccp_asterisk_parseStrToAstMessage(str, &m);

				const char *parkinglot = astman_get_header(&m, "ParkingLot");
				const char *extension = astman_get_header(&m, PARKING_SLOT);
				int exten = sccp_atoi(extension, strlen(extension));
				if (parkinglot && exten) {
					if (sccp_strcaseequals("ParkedCall", event)) {
						iParkingLot.addSlot(parkinglot, exten, &m);
					} else {
						iParkingLot.removeSlot(parkinglot, exten);
					}
				}
			}
#endif
		//} else {
		//	sccp_log(DEBUGCAT_CORE)("SCCP: (managerHookHelper) %s Received\ncontent:[%s]\n", event, content);
		}
	}
	return 0;
}

AST_THREADSTORAGE(hookresult_threadbuf);
#define HOOKRESULT_INITSIZE DEFAULT_PBX_STR_BUFFERSIZE*2

#if ASTERISK_VERSION_GROUP >= 108
/*
 * \brief helper function to concatenate the result from a ami hook send action using a threadlocal buffer
 */
static int __sccp_manager_hookresult(int category, const char *event, char *content) {
        struct ast_str *buf = ast_str_thread_get(&hookresult_threadbuf, HOOKRESULT_INITSIZE);;
	if (buf) {
		pbx_str_append(&buf, 0, "%s", content);
	}
	return 0;
}
#endif
/*!
 * \brief Call an AMI/Manager Function and Wait for the Result
 * 
 * @param manager_command	const char * containing Something like "Action: ParkedCalls\r\n"
 * @param outStr		unallocated char * (will be allocated if successfull, must be freed after call)
 * @return int (-1 on failure | return value from called function)
 *
 * \todo implement using thread_local instead
 */
boolean_t sccp_manager_action2str(const char *manager_command, char **outStr) 
{
#if ASTERISK_VERSION_GROUP >= 108
        int failure = 0;
        struct ast_str *buf;
        
        if (!outStr || sccp_strlen_zero(manager_command) || !(buf = ast_str_thread_get(&hookresult_threadbuf, HOOKRESULT_INITSIZE))) {
		pbx_log(LOG_ERROR, "SCCP: No OutStr or Command Provided\n");
        	return -2;
        }

	struct manager_custom_hook hook = {__FILE__, __sccp_manager_hookresult};
        failure = ast_hook_send_action(&hook, manager_command);							/* "Action: ParkedCalls\r\n" */
        if (!failure) {
		sccp_log(DEBUGCAT_CORE)("SCCP: Sending AMI Result String: %s\n", pbx_str_buffer(buf));
        	*outStr = pbx_strdup(pbx_str_buffer(buf));
        }
       	ast_str_reset(buf);
        return !failure ? TRUE : FALSE;
#else
	sccp_log(DEBUGCAT_CORE)("SCCP: ast_hook_send_action is not available in asterisk-1.6\n");
	return FALSE;
#endif
}

/*
<response type='object' id='(null)'><(null) response='Success' message='Parked calls will follow' /></response>
<response type='object' id='(null)'><(null) event='ParkedCall' parkinglot='default' exten='701' channel='IAX2/iaxuser-2343' from='SCCP/98031-00000001' timeout='41' duration='4' calleridnum='100011' calleridname='Diederik de Groot (10001)' connectedlinenum='' connectedlinename='' /></response>
<response type='object' id='(null)'><(null) event='ParkedCallsComplete' total='1' /></response>
*/

#if defined(CS_EXPERIMENTAL)
char * sccp_manager_retrieve_parkedcalls_cxml(char ** out) 
{
	char *parkedcalls_messageStr = NULL;
	char *manager_command = "Action: ParkedCalls\r\n";
	
	if (sccp_manager_action2str(manager_command, &parkedcalls_messageStr) && parkedcalls_messageStr) {
		pbx_str_t *tmpPbxStr = ast_str_create(DEFAULT_PBX_STR_BUFFERSIZE);
		struct message m = {0};
		const char *event = "";

		sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "SCCP: (sccp_manager_retrieve_parkedcalls_cxml) content=%s\n",  parkedcalls_messageStr);
		pbx_str_append(&tmpPbxStr, 0, "<?xml version=\"1.0\"?>");
		pbx_str_append(&tmpPbxStr, 0, "<CiscoIPPhoneDirectory>");
		pbx_str_append(&tmpPbxStr, 0, "<Title>Parked Calls</Title>");
		pbx_str_append(&tmpPbxStr, 0, "<Prompt>Please Choose on of the parking lots</Prompt>");
		pbx_str_append(&tmpPbxStr, 0, "<DirectoryEntry>");
		char *strptr = parkedcalls_messageStr;
		char *token = NULL;
		char *rest = strptr;
		while (sscanf(strptr, "%[^\r\n]\r\n\r\n%s", token, rest) && token) {
			sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "SCCP: (sccp_manager_retrieve_parkedcalls_cxml) token='%s', rest='%s'\n", token, rest);
			usleep(500);

			token = sccp_asterisk_parseStrToAstMessage(token, &m);
		        event  = astman_get_header(&m, "Event");
			if (sccp_strcaseequals(event, "ParkedCallsComplete")) {
				break;
			} else if (sccp_strcaseequals(event, "ParkedCall")){
				pbx_str_append(&tmpPbxStr, 0, "<Name>%s (%s) by %s</Name><Telephone>%s</Telephone>", 
					astman_get_header((const struct message *)&m, "CallerIdName"), 
					astman_get_header((const struct message *)&m, "CallerIdNum"),
					astman_get_header((const struct message *)&m, "ConnectedLineName"),
					astman_get_header((const struct message *)&m, "Exten")
				);
				sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "SCCP: Found ParkedCall: %s on %s@%s\n", astman_get_header((const struct message *)&m, "Channel"), astman_get_header((const struct message *)&m, "Exten"), astman_get_header((const struct message *)&m, "ParkingLot"));
			}
		        memset(&m, 0, sizeof(m));
		        strptr = rest;
		}
		pbx_str_append(&tmpPbxStr, 0, "</DirectoryEntry>");
		pbx_str_append(&tmpPbxStr, 0, "</CiscoIPPhoneDirectory>");

		*out = pbx_strdup(pbx_str_buffer(tmpPbxStr));
		
		sccp_free(tmpPbxStr);
		sccp_free(parkedcalls_messageStr);
	}
	sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "SCCP: (sccp_manager_retrieve_parkedcalls_cxml) cxml=%s\n", *out);
	return *out;
}
#endif
#endif														// HAVE_PBX_MANAGER_HOOK_H
#endif														// CS_SCCP_MANAGER
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
