
/*!
 * \file        sccp_actions.c
 * \brief       SCCP Actions Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \author      Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks
 * Purpose:     SCCP Actions
 * When to use: Only methods directly related to sccp actions should be stored in this source file.
 *              Actions handle all the message interactions from and to SCCP phones
 * Relations:   Other Function wanting to communicate to phones
 *              Phones Requesting function to be performed
 */

#include <config.h>
#include "common.h"
#include "sccp_actions.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_utils.h"
#include "sccp_channel.h"
#include "sccp_line.h"
#include "sccp_features.h"
#include "sccp_config.h"
#include "sccp_conference.h"
#include "sccp_socket.h"
#include "sccp_indicate.h"
#include "sccp_rtp.h"
#include <asterisk/unaligned.h>
#include <sys/stat.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$");
#include <math.h>
#if ASTERISK_VERSION_NUMBER < 10400

/* !
 *\brief Host Access Rule Structure
 */
struct ast_ha {
	/* Host access rule */
	struct in_addr netaddr;
	struct in_addr netmask;
	int sense;
	struct ast_ha *next;
};
#endif

/*!
 * \brief Handle Alarm
 * \param no_s SCCP Session = NULL
 * \param no_d SCCP Device = NULL
 * \param msg_in SCCP Message
 */
void sccp_handle_alarm(sccp_session_t * no_s, sccp_device_t * no_d, sccp_msg_t * msg_in)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Alarm Message: Severity: %s (%d), %s [%d/%d]\n", skinny_alarm2str(letohl(msg_in->data.AlarmMessage.lel_alarmSeverity)), letohl(msg_in->data.AlarmMessage.lel_alarmSeverity), msg_in->data.AlarmMessage.text, letohl(msg_in->data.AlarmMessage.lel_parm1), letohl(msg_in->data.AlarmMessage.lel_parm2));
}

/*!
 * \brief Handle Unknown Message
 * \param no_s SCCP Session = NULL
 * \param no_d SCCP Device = NULL
 * \param msg_in SCCP Message
 *
 * Interesting values for Last =
 * 0 Phone Load Is Rejected
 * 1 Phone Load TFTP Size Error
 * 2 Phone Load Compressor Error
 * 3 Phone Load Version Error
 * 4 Disk Full Error
 * 5 Checksum Error
 * 6 Phone Load Not Found in TFTP Server
 * 7 TFTP Timeout
 * 8 TFTP Access Error
 * 9 TFTP Error
 * 10 CCM TCP Connection timeout
 * 11 CCM TCP Connection Close because of bad Ack
 * 12 CCM Resets TCP Connection
 * 13 CCM Aborts TCP Connection
 * 14 CCM TCP Connection Closed
 * 15 CCM TCP Connection Closed because ICMP Unreachable
 * 16 CCM Rejects TCP Connection
 * 17 Keepalive Time Out
 * 18 Fail Back to Primary CCM
 * 20 User Resets Phone By Keypad
 * 21 Phone Resets because IP configuration
 * 22 CCM Resets Phone
 * 23 CCM Restarts Phone
 * 24 CCM Rejects Phone Registration
 * 25 Phone Initializes
 * 26 CCM TCP Connection Closed With Unknown Reason
 * 27 Waiting For State From CCM
 * 28 Waiting For Response From CCM
 * 29 DSP Alarm
 * 30 Phone Abort CCM TCP Connection
 * 31 File Authorization Failed
 */
void sccp_handle_unknown_message(sccp_session_t * no_s, sccp_device_t * no_d, sccp_msg_t * msg_in)
{
	uint32_t mid = letohl(msg_in->header.lel_messageId);

	if ((GLOB(debug) & DEBUGCAT_MESSAGE) != 0) {								// only show when debugging messages
		pbx_log(LOG_WARNING, "Unhandled SCCP Message: %s(0x%04X) %d bytes length\n", msgtype2str(mid), mid, msg_in->header.length);
		sccp_dump_msg(msg_in);
	}
}

/*!
 * \brief Handle Unknown Message
 * \param no_s SCCP Session = NULL
 * \param no_d SCCP Device = NULL
 * \param msg_in SCCP Message
 */
void sccp_handle_XMLAlarmMessage(sccp_session_t * no_s, sccp_device_t * no_d, sccp_msg_t * msg_in)
{
	uint32_t mid = letohl(msg_in->header.lel_messageId);
	char alarmName[101];
	int reasonEnum;
	char lastProtocolEventSent[101];
	char lastProtocolEventReceived[101];

	/*
	   char *deviceName = "";
	   char neighborIpv4Address[15];
	   char neighborIpv6Address[41];
	   char neighborDeviceID[StationMaxNameSize];
	   char neighborPortID[StationMaxNameSize];
	 */

	char *xmlData = sccp_strdupa((char *) &msg_in->data.XMLAlarmMessage);
	char *state = "";
	char *line = "";

	for (line = strtok_r(xmlData, "\n", &state); line != NULL; line = strtok_r(NULL, "\n", &state)) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s\n", line);

		/*
		   if (sscanf(line, "<String name=\"%[a-zA-Z]\">", deviceName) == 1) {
		   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Device Name: %s\n", deviceName);
		   }
		 */
		if (sscanf(line, "<Alarm Name=\"%[a-zA-Z]\">", alarmName) == 1) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Alarm Type: %s\n", alarmName);
		}
		if (sscanf(line, "<Enum name=\"ReasonForOutOfService\">%d</Enum>>", &reasonEnum) == 1) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Reason Enum: %d\n", reasonEnum);
		}
		if (sscanf(line, "<String name=\"LastProtocolEventSent\">%[^<]</String>", lastProtocolEventSent) == 1) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Last Event Sent: %s\n", lastProtocolEventSent);
		}
		if (sscanf(line, "<String name=\"LastProtocolEventReceived\">%[^<]</String>", lastProtocolEventReceived) == 1) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Last Event Received: %s\n", lastProtocolEventReceived);
		}

		/* We might want to capture this information for later use (For example in a VideoAdvantage like project) */

		/*
		   if (sscanf(line, "<String name=\"NeighborIPv4Address\">%[^<]</String>", neighborIpv4Address) == 1) {
		   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "neighborIpv4Address: %s\n", neighborIpv4Address);
		   }
		   if (sscanf(line, "<String name=\"NeighborIPv6Address\">%[^<]</String>", neighborIpv6Address) == 1) {
		   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "neighborIpv6Address: %s\n", neighborIpv6Address);
		   }
		   if (sscanf(line, "<String name=\"NeighborDeviceID\">%[^<]</String>", neighborDeviceID) == 1) {
		   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "neighborDeviceID: %s\n", neighborDeviceID);
		   }
		   if (sscanf(line, "<String name=\"NeighborPortID\">%[^<]</String>", neighborPortID) == 1) {
		   sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "neighborPortID: %s\n", neighborPortID);
		   }
		 */
	}
	if ((GLOB(debug) & DEBUGCAT_MESSAGE) != 0) {								// only show when debugging messages
		pbx_log(LOG_WARNING, "SCCP XMLAlarm Message: %s(0x%04X) %d bytes length\n", msgtype2str(mid), mid, msg_in->header.length);
		sccp_dump_msg(msg_in);
	}
}

/*!
 * \brief Handle Token Request
 *
 * If a fall-back server has been entered in the phones cnf.xml file and the phone has fallen back to a secundairy server
 * it will send a tokenreq to the primairy every so often (secundaity keep alive timeout ?). Once the primairy server sends
 * a token acknowledgement the switches back.
 *
 * \param s SCCP Session
 * \param no_d SCCP Device = NULL
 * \param msg_in SCCP Message
 *
 * \callgraph
 * \callergraph
 *
 * \todo Implement a decision when to send RegisterTokenAck and when to send RegisterTokenReject
 *       If sending RegisterTokenReject what should the lel_tokenRejWaitTime (BackOff time) be
 */
void sccp_handle_token_request(sccp_session_t * s, sccp_device_t * no_d, sccp_msg_t * msg_in)
{
	AUTO_RELEASE sccp_device_t *device = NULL;
	char *deviceName = "";
	uint32_t serverPriority = GLOB(server_priority);
	uint32_t deviceInstance = 0;
	uint32_t deviceType = 0;

	deviceName = sccp_strdupa(msg_in->data.RegisterTokenRequest.sId.deviceName);
	deviceInstance = letohl(msg_in->data.RegisterTokenRequest.sId.lel_instance);
	deviceType = letohl(msg_in->data.RegisterTokenRequest.lel_deviceType);

	if (GLOB(reload_in_progress)) {
		pbx_log(LOG_NOTICE, "SCCP: Reload in progress. Come back later.\n");
		sccp_session_tokenReject(s, 5);
		return;
	}
	// sccp_dump_msg(msg_in);
	if (!skinny_devicetype_exists(deviceType)) {
		pbx_log(LOG_NOTICE, "%s: We currently do not (fully) support this device type (%d).\n" "Please send this device type number plus the information about the phone model you are using to one of our developers.\n" "Be Warned you should Expect Trouble Ahead\nWe will try to go ahead (Without any guarantees)\n", deviceName, deviceType);
	}
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "%s: is requesting a Token, Device Instance: %d, Type: %s (%d)\n", deviceName, deviceInstance, skinny_devicetype2str(deviceType), deviceType);

	{
		// Search for already known device-sessions
		AUTO_RELEASE sccp_device_t *tmpdevice = sccp_device_find_byid(deviceName, TRUE);

		if (tmpdevice) {
			if (tmpdevice->session && tmpdevice->session != s) {
				sccp_session_crossdevice_cleanup(s, tmpdevice->session, TRUE);
				return;
			}
		}
	}

	// Search for the device (including realtime), if does not exist and hotline is requested create one.
	device = sccp_device_find_byid(deviceName, TRUE);
	if (!device && GLOB(allowAnonymous)) {
		device = sccp_device_createAnonymous(msg_in->data.RegisterTokenRequest.sId.deviceName);
		sccp_config_applyDeviceConfiguration(device, NULL);
		sccp_config_addButton(&device->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", deviceName, GLOB(hotline)->line->name);
		device->defaultLineInstance = SCCP_FIRST_LINEINSTANCE;
		sccp_device_addToGlobals(device);
	}

	/* no configuation for this device and no anonymous devices allowed */
	if (!device) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: not found\n", deviceName);
		s = sccp_session_reject(s, "Unknown Device");
		return;
	}

	s->device = sccp_session_addDevice(s, device);								// retained in session
	device->status.token = SCCP_TOKEN_STATE_REJ;
	device->skinny_type = deviceType;

	if (device->checkACL(device) == FALSE) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", msg_in->data.RegisterTokenRequest.sId.deviceName, sccp_socket_stringify_addr(&s->sin));
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		s = sccp_session_reject(s, "IP Not Authorized");
		return;
	}

	/* accepting token by default */
	boolean_t sendAck = TRUE;
	int last_digit = deviceName[strlen(deviceName)];
	int token_backoff_time = GLOB(token_backoff_time) >= 30 ? GLOB(token_backoff_time) : 60;

	if (!sccp_strlen_zero(GLOB(token_fallback))) {
		if (sccp_false(GLOB(token_fallback))) {
			sendAck = FALSE;
		} else if (sccp_true(GLOB(token_fallback))) {
			/* we are the primary server */
			if (serverPriority == 1) {
				sendAck = TRUE;
			}
		} else if (!strcasecmp("odd", GLOB(token_fallback))) {
			if (last_digit % 2 != 0) {
				sendAck = TRUE;
			}
		} else if (!strcasecmp("even", GLOB(token_fallback))) {
			if (last_digit % 2 == 0) {
				sendAck = TRUE;
			}
		} else if (strstr(GLOB(token_fallback), "/") != NULL) {
			struct stat sb = { 0 };
			if (stat(GLOB(token_fallback), &sb) == 0 && sb.st_mode & S_IXUSR) {
				char command[SCCP_PATH_MAX];
				char buff[19] = "";
				char output[20] = "";

				snprintf(command, SCCP_PATH_MAX, "%s %s %s %s", GLOB(token_fallback), deviceName, sccp_socket_stringify_host(&s->sin), skinny_devicetype2str(deviceType));
				FILE *pp;

				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: (token_request), executing '%s'\n", deviceName, (char *) command);
				pp = popen(command, "r");
				if (pp != NULL) {
					while (fgets(buff, sizeof(buff), pp)) {
						strncat(output, buff, 19 - strlen(output));
					}
					pclose(pp);
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (token_request), script result='%s'\n", deviceName, (char *) output);
					if (sccp_strcaseequals(output, "ACK\n")) {
						sendAck = TRUE;
					} else if (sscanf(output, "%d\n", &token_backoff_time) == 1 && token_backoff_time > 30) {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (token_request), sets new token_backoff_time=%d\n", deviceName, token_backoff_time);
						sendAck = FALSE;
					} else {
						pbx_log(LOG_WARNING, "%s: (token_request) script '%s' return unknown result: '%s'\n", deviceName, GLOB(token_fallback), (char *) output);
					}
				} else {
					pbx_log(LOG_WARNING, "%s: (token_request) Unable to execute '%s'\n", deviceName, (char *) command);
				}
			} else {
				pbx_log(LOG_WARNING, "Script %s, either not found or not executable by this user\n", GLOB(token_fallback));
			}
		} else {
			pbx_log(LOG_WARNING, "%s: did not understand global fallback value: '%s'... sending default value 'ACK'\n", deviceName, GLOB(token_fallback));
		}
	} else {
		pbx_log(LOG_WARNING, "%s: global fallback value is empty... sending default value 'ACK'\n", deviceName);
	}

	/* some test to detect active calls */
	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: serverPriority: %d, unknown: %d, active call? %s\n", deviceName, serverPriority, letohl(msg_in->data.RegisterTokenRequest.unknown), (letohl(msg_in->data.RegisterTokenRequest.unknown) & 0x6) ? "yes" : "no");

	device->registrationState = SKINNY_DEVICE_RS_TOKEN;
	if (sendAck) {
		sccp_log_and((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Acknowledging phone token request\n", deviceName);
		sccp_session_tokenAck(s);
	} else {
		sccp_log_and((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Sending phone a token rejection (sccp.conf:fallback=%s, serverPriority=%d), ask again in '%d' seconds\n", deviceName, GLOB(token_fallback), serverPriority, GLOB(token_backoff_time));
		sccp_session_tokenReject(s, token_backoff_time);
	}

	device->status.token = (sendAck) ? SCCP_TOKEN_STATE_ACK : SCCP_TOKEN_STATE_REJ;
	device->registrationTime = time(0);									// last time device tried sending token
}

/*!
 * \brief Handle Token Request for SPCP phones
 *
 * If a fall-back server has been entered in the phones cnf.xml file and the phone has fallen back to a secundairy server
 * it will send a tokenreq to the primairy every so often (secundaity keep alive timeout ?). Once the primairy server sends
 * a token acknowledgement the switches back.
 *
 * \param s SCCP Session
 * \param no_d SCCP Device = NULL
 * \param msg_in SCCP Message
 *
 * \callgraph
 * \callergraph
 *
 * \todo Implement a decision when to send RegisterTokenAck and when to send RegisterTokenReject
 *       If sending RegisterTokenReject what should the lel_tokenRejWaitTime (BackOff time) be
 */
void sccp_handle_SPCPTokenReq(sccp_session_t * s, sccp_device_t * no_d, sccp_msg_t * msg_in)
{
	AUTO_RELEASE sccp_device_t *device = NULL;
	char *deviceName = "";
	uint32_t deviceInstance = 0;
	uint32_t deviceType = 0;

	deviceInstance = letohl(msg_in->data.SPCPRegisterTokenRequest.sId.lel_instance);
	deviceName = sccp_strdupa(msg_in->data.RegisterTokenRequest.sId.deviceName);
	deviceType = letohl(msg_in->data.SPCPRegisterTokenRequest.lel_deviceType);

	if (GLOB(reload_in_progress)) {
		pbx_log(LOG_NOTICE, "SCCP: Reload in progress. Come back later.\n");
		sccp_session_tokenReject(s, 5);
		return;
	}

	if (!skinny_devicetype_exists(deviceType)) {
		pbx_log(LOG_NOTICE, "%s: We currently do not (fully) support this device type (%d).\n" "Please send this device type number plus the information about the phone model you are using to one of our developers.\n" "Be Warned you should Expect Trouble Ahead\nWe will try to go ahead (Without any guarantees)\n", deviceName, deviceType);
	}
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "%s: is requesting a token, Instance: %d, Type: %s (%d)\n", msg_in->data.SPCPRegisterTokenRequest.sId.deviceName, deviceInstance, skinny_devicetype2str(deviceType), deviceType);

	/* ip address range check */
	if (GLOB(ha) && !sccp_apply_ha(GLOB(ha), &s->sin)) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", msg_in->data.SPCPRegisterTokenRequest.sId.deviceName);
		s = sccp_session_reject(s, "IP not authorized");
		return;
	}

	{
		// Search for already known device-sessions
		AUTO_RELEASE sccp_device_t *tmpdevice = sccp_device_find_byid(deviceName, TRUE);

		if (tmpdevice) {
			if (tmpdevice->session && tmpdevice->session != s) {
				sccp_session_crossdevice_cleanup(s, tmpdevice->session, TRUE);
				return;
			}
		}
	}

	// search for all devices including realtime
	device = sccp_device_find_byid(msg_in->data.SPCPRegisterTokenRequest.sId.deviceName, TRUE);
	if (!device && GLOB(allowAnonymous)) {
		device = sccp_device_createAnonymous(msg_in->data.SPCPRegisterTokenRequest.sId.deviceName);

		sccp_config_applyDeviceConfiguration(device, NULL);
		sccp_config_addButton(&device->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", msg_in->data.SPCPRegisterTokenRequest.sId.deviceName, GLOB(hotline)->line->name);
		device->defaultLineInstance = SCCP_FIRST_LINEINSTANCE;
		sccp_device_addToGlobals(device);
	}

	/* no configuation for this device and no anonymous devices allowed */
	if (!device) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: not found\n", msg_in->data.SPCPRegisterTokenRequest.sId.deviceName);
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "Device not Accepted");
		return;
	}
	s->protocolType = SPCP_PROTOCOL;
	s->device = sccp_session_addDevice(s, device);								// retained in session
	device->status.token = SCCP_TOKEN_STATE_REJ;
	device->skinny_type = deviceType;

	if (device->checkACL(device) == FALSE) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", msg_in->data.SPCPRegisterTokenRequest.sId.deviceName, sccp_socket_stringify_addr(&s->sin));
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "IP Not Authorized");
		return;
	}

	if (device->session && device->session != s) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Crossover device registration!\n", device->id);
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "Crossover session not allowed");
		device->session = sccp_session_reject(device->session, "Crossover session not allowed");
		return;
	}

	/* all checks passed, assign session to device */
	// device->session = s;
	device->registrationState = SKINNY_DEVICE_RS_TOKEN;
	device->status.token = SCCP_TOKEN_STATE_ACK;

	sccp_session_tokenAckSPCP(s, 65535);
	device->registrationTime = time(0);									// last time device tried sending token
}

/*!
 * \brief Handle Device Registration
 * \param s SCCP Session
 * \param maybe_d SCCP Device
 * \param msg_in SCCP Message
 *
 * \callgraph
 * \callergraph
 */
void sccp_handle_register(sccp_session_t * s, sccp_device_t * maybe_d, sccp_msg_t * msg_in)
{
	AUTO_RELEASE sccp_device_t *device = NULL;
	char *phone_ipv4 = NULL, *phone_ipv6 = NULL;

	uint32_t deviceInstance = letohl(msg_in->data.RegisterMessage.sId.lel_instance);
	uint32_t userid = letohl(msg_in->data.RegisterMessage.sId.lel_userid);
	char deviceName[StationMaxDeviceNameSize];

	sccp_copy_string(deviceName, msg_in->data.RegisterMessage.sId.deviceName, StationMaxDeviceNameSize);
	uint32_t deviceType = letohl(msg_in->data.RegisterMessage.lel_deviceType);
	uint32_t maxStreams = letohl(msg_in->data.RegisterMessage.lel_maxStreams);
	uint32_t activeStreams = letohl(msg_in->data.RegisterMessage.lel_activeStreams);
	uint8_t protocolVer = letohl(msg_in->data.RegisterMessage.phone_features) & SKINNY_PHONE_FEATURES_PROTOCOLVERSION;
	uint32_t maxConferences = letohl(msg_in->data.RegisterMessage.lel_maxConferences);
	uint32_t activeConferences = letohl(msg_in->data.RegisterMessage.lel_activeConferences);
	uint8_t macAddress[12];

	memcpy(macAddress, msg_in->data.RegisterMessage.macAddress, 12);
	uint32_t ipV4AddressScope = letohl(msg_in->data.RegisterMessage.lel_ipV4AddressScope);
	uint32_t maxNumberOfLines = letohl(msg_in->data.RegisterMessage.lel_maxNumberOfLines);
	uint32_t ipV6AddressScope = letohl(msg_in->data.RegisterMessage.lel_ipV6AddressScope);

	if (GLOB(reload_in_progress)) {
		pbx_log(LOG_NOTICE, "SCCP: Reload in progress. Come back later.\n");
		s = sccp_session_reject(s, "Reload in progress");
		return;
	}

	if (!skinny_devicetype_exists(deviceType)) {
		pbx_log(LOG_NOTICE, "%s: We currently do not (fully) support this device type (%d).\n" "Please send this device type number plus the information about the phone model you are using to one of our developers.\n" "Be Warned you should Expect Trouble Ahead\nWe will try to go ahead (Without any guarantees)\n", deviceName, deviceType);
	}
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: is registering, Instance: %d, UserId: %d, Type: %s (%d), Version: %d (loadinfo '%s')\n", deviceName, deviceInstance, userid, skinny_devicetype2str(deviceType), deviceType, protocolVer, msg_in->data.RegisterMessage.loadInfo);

	// search for all devices including realtime
	if (!maybe_d) {
		device = sccp_device_find_byid(deviceName, TRUE);
	} else {
		device = sccp_device_retain(maybe_d);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s: cached device configuration (state: %s)\n", DEV_ID_LOG(device), device ? skinny_registrationstate2str(device->registrationState) : "UNKNOWN");
	}

	if (device && device->session && device->session != s) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Crossover device registration (state: %s)! Fixing up to new session\n", DEV_ID_LOG(device), skinny_registrationstate2str(device->registrationState));
		sccp_session_crossdevice_cleanup(s, device->session, FALSE);
		return;												// come back later
	}

	/*! \todo We need a fix here. If deviceName was provided and specified in sccp.conf we should not continue to anonymous,
	 * but we cannot depend on one of the standard find functions for this, because they return null in two different cases (non-existent and refcount<0).
	 */
	if (!device && GLOB(allowAnonymous)) {
		if (!(device = sccp_device_createAnonymous(deviceName))) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline device could not be created: %s\n", deviceName, GLOB(hotline)->line->name);
		} else {
			sccp_config_applyDeviceConfiguration(device, NULL);
			sccp_config_addButton(&device->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", deviceName, GLOB(hotline)->line->name);
			device->defaultLineInstance = SCCP_FIRST_LINEINSTANCE;
			sccp_device_addToGlobals(device);
		}
	}

	if (device) {
		if (!s->device || s->device != device) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", DEV_ID_LOG(device), s->fds[0].fd, sccp_socket_stringify_addr(&s->sin));
			s->device = sccp_session_addDevice(s, device);						// replace retained in session (already connected via tokenReq before)
		}

		if (!device || !device->session || !s->device) {
			pbx_log(LOG_WARNING, "%s: Signing over the session to new device failed. Giving up.\n", DEV_ID_LOG(device));
			return;
		}

		/* check ACLs for this device */
		if (device->checkACL(device) == FALSE) {
			pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", deviceName, sccp_socket_stringify_addr(&s->sin));
			device->registrationState = SKINNY_DEVICE_RS_FAILED;
			s = sccp_session_reject(s, "IP Not Authorized");
			return;
		}

	} else {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Device Unknown \n", deviceName);
		s = sccp_session_reject(s, "Device Unknown");
		return;
	}

	device->device_features = letohl(msg_in->data.RegisterMessage.phone_features);
	device->linesRegistered = FALSE;

	struct sockaddr_storage register_sas = { 0 };
	if (!sccp_strlen_zero(msg_in->data.RegisterMessage.ipv6Address)) {
		register_sas.ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &register_sas;

		memcpy(&sin6->sin6_addr, &msg_in->data.RegisterMessage.ipv6Address, sizeof(sin6->sin6_addr));
		phone_ipv6 = strdupa(sccp_socket_stringify_host(&register_sas));
	}
	register_sas.ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) &register_sas;

	memcpy(&sin->sin_addr, &msg_in->data.RegisterMessage.stationIpAddr, sizeof(sin->sin_addr));
	phone_ipv4 = strdupa(sccp_socket_stringify_host(&register_sas));

	/* get our IPv4 address */
	{
		sccp_socket_getOurAddressfor(&register_sas, &s->ourIPv4);
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Our IPv4 Address %s\n", deviceName, sccp_socket_stringify(&s->ourIPv4));
	}
	/* */
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: device load_info='%s', maxNumberOfLines='%d', supports dynamic_messages='%s', supports abbr_dial='%s'\n", deviceName, msg_in->data.RegisterMessage.loadInfo, maxNumberOfLines, (device->device_features & SKINNY_PHONE_FEATURES_DYNAMIC_MESSAGES) == 0 ? "no" : "yes", (device->device_features & SKINNY_PHONE_FEATURES_ABBRDIAL) == 0 ? "no" : "yes");
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: ipv4Address: %s, ipV4AddressScope: %d, ipv6Address: %s, ipV6AddressScope: %d\n", deviceName, phone_ipv4, ipV4AddressScope, phone_ipv6, ipV6AddressScope);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: maxStreams: %d, activeStreams: %d, maxConferences: %d, activeConferences: %d\n", deviceName, maxStreams, activeStreams, maxConferences, activeConferences);

	/* auto NAT detection if NAT is not set as device configuration */
	if (device->nat == SCCP_NAT_AUTO && GLOB(localaddr)) {
		device->nat = SCCP_NAT_AUTO_OFF;
		struct sockaddr_storage session_sas;

		memcpy(&session_sas, &s->sin, sizeof(struct sockaddr_storage));
		sccp_socket_ipv4_mapped(&session_sas, &session_sas);
		char *session_ipv4 = strdupa(sccp_socket_stringify_host(&session_sas));

		struct ast_str *ha_localnet_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

		sccp_print_ha(ha_localnet_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(localaddr));

		if (session_sas.ss_family == AF_INET) {
			if (sccp_apply_ha_default(GLOB(localaddr), &session_sas, AST_SENSE_DENY) != AST_SENSE_ALLOW) {	// if device->sin falls in localnet scope
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Auto Detected NAT. Session IP '%s' (Phone: '%s') is outside of localnet('%s') scope. We will use externip or externhost for the RTP stream\n", deviceName, session_ipv4, phone_ipv4, pbx_str_buffer(ha_localnet_buf));
				device->nat = SCCP_NAT_AUTO_ON;
			} else if (sccp_socket_cmp_addr(&session_sas, &register_sas)) {				// compare device->sin to the phones reported ipaddr
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Auto Detected Remote NAT. Session IP '%s' does not match IpAddr '%s' Reported by Device.  We will use externip or externhost for the RTP stream\n", deviceName, session_ipv4, phone_ipv4);
				device->nat = SCCP_NAT_AUTO_ON;
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device Not NATTED. Device IP '%s' falls in localnet scope\n", deviceName, session_ipv4);
			}
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: NAT Autodetection Skipped for IPv6.\n", deviceName);
		}
	}

	device->skinny_type = deviceType;

	// device->session = s;
	s->lastKeepAlive = time(0);
	device->mwilight = 0;
	device->protocolversion = protocolVer;
	device->status.token = SCCP_TOKEN_STATE_NOTOKEN;
	sccp_copy_string(device->loadedimageversion, msg_in->data.RegisterMessage.loadInfo, StationMaxImageVersionSize);

	/** workaround to fix the protocol version issue for ata devices */
	/*
	 * MAC-Address        : ATA00215504e821
	 * Protocol Version   : Supported '33', In Use '17'
	 */
	if (device->skinny_type == SKINNY_DEVICETYPE_ATA188 || device->skinny_type == SKINNY_DEVICETYPE_ATA186) {
		device->protocolversion = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
	}

	device->protocol = sccp_protocol_getDeviceProtocol(device, s->protocolType);
	uint8_t ourMaxProtocolCapability = sccp_protocol_getMaxSupportedVersionNumber(s->protocolType);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "%s: Asked for our protocol capability (%d).\n", DEV_ID_LOG(device), ourMaxProtocolCapability);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "%s: Phone protocol capability : %d\n", DEV_ID_LOG(device), protocolVer);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "%s: Our protocol capability   : %d\n", DEV_ID_LOG(device), ourMaxProtocolCapability);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Joint protocol capability : %d\n", DEV_ID_LOG(device), device->protocol->version);

	/* we need some entropy for keepalive, to reduce the number of devices sending keepalive at one time */
	device->keepaliveinterval = device->keepalive ? device->keepalive : GLOB(keepalive);
	device->keepaliveinterval = ((device->keepaliveinterval / 4) * 3) + (rand() % (device->keepaliveinterval / 4)) + 1;	// smaller random segment, keeping keepalive toward the upperbound
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Ask the phone to send keepalive message every %d seconds\n", DEV_ID_LOG(device), device->keepaliveinterval);

	device->inuseprotocolversion = device->protocol->version;
	sccp_device_preregistration(device);
	device->protocol->sendRegisterAck(device, device->keepaliveinterval, device->keepaliveinterval, GLOB(dateformat));

	sccp_dev_set_registered(device, SKINNY_DEVICE_RS_PROGRESS);

	/*
	   Ask for the capabilities of the device
	   to proceed with registration according to sccp protocol specification 3.0
	 */
	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: (sccp_handle_register) asking for capabilities\n", DEV_ID_LOG(device));
	sccp_dev_sendmsg(device, CapabilitiesReqMessage);
}

/*!
 * \brief Make Button Template for Device
 * \param d SCCP Device as sccp_device_t
 * \return Linked List of ButtonDefinitions
 */
static btnlist *sccp_make_button_template(sccp_device_t * d)
{
	int i = 0;
	btnlist *btn;
	sccp_buttonconfig_t *buttonconfig;

	if (!d) {
		return NULL;
	}
	if (!(btn = sccp_malloc(sizeof(btnlist) * StationMaxButtonTemplateSize))) {
		return NULL;
	}

	memset(btn, 0, sizeof(btnlist) * StationMaxButtonTemplateSize);
	sccp_dev_build_buttontemplate(d, btn);

	uint16_t speeddialInstance = SCCP_FIRST_SPEEDDIALINSTANCE;						/* starting instance for speeddial is 1 */
	uint16_t lineInstance = SCCP_FIRST_LINEINSTANCE;
	uint16_t serviceInstance = SCCP_FIRST_SERVICEINSTANCE;
	boolean_t defaultLineSet = FALSE;

	if (!d->isAnonymous) {
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "\n%s: searching for position of button type %d\n", DEV_ID_LOG(d), buttonconfig->type);
			if (buttonconfig->instance > 0) {
				continue;
			}
			if (buttonconfig->type == LINE) {
				sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for line position for line '%s'\n", DEV_ID_LOG(d), buttonconfig->button.line.name);
			}

			for (i = 0; i < StationMaxButtonTemplateSize; i++) {
				sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: btn[%.2d].type = %d\n", DEV_ID_LOG(d), i, btn[i].type);

				if (buttonconfig->type == LINE && !sccp_strlen_zero(buttonconfig->button.line.name)
				    && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_LINE)) {

					btn[i].type = SKINNY_BUTTONTYPE_LINE;

					/* search line (create new line, if necessary (realtime)) */
					/*! retains new line in btn[i].ptr, finally released in sccp_dev_clean */
					if ((btn[i].ptr = sccp_line_find_byname(buttonconfig->button.line.name, TRUE))) {
						buttonconfig->instance = btn[i].instance = lineInstance++;
						sccp_line_addDevice((sccp_line_t *) btn[i].ptr, d, btn[i].instance, &(buttonconfig->button.line.subscriptionId));
						if (FALSE == defaultLineSet && !d->defaultLineInstance) {
							d->defaultLineInstance = buttonconfig->instance;
							defaultLineSet = TRUE;
						}
					} else {
						btn[i].type = SKINNY_BUTTONTYPE_UNUSED;
						buttonconfig->instance = btn[i].instance = 0;
						pbx_log(LOG_WARNING, "%s: line %s does not exists\n", DEV_ID_LOG(d), buttonconfig->button.line.name);
					}

					sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: add line %s on position %d\n", DEV_ID_LOG(d), buttonconfig->button.line.name, buttonconfig->instance);
					break;

				} else if (buttonconfig->type == EMPTY && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_LINE || btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL)) {
					btn[i].type = SKINNY_BUTTONTYPE_UNUSED;
					buttonconfig->instance = btn[i].instance = 0;
					break;

				} else if (buttonconfig->type == SERVICE && (btn[i].type == SCCP_BUTTONTYPE_MULTI)) {
					btn[i].type = SKINNY_BUTTONTYPE_SERVICEURL;
					buttonconfig->instance = btn[i].instance = serviceInstance++;
					break;

				} else if (buttonconfig->type == SPEEDDIAL && !sccp_strlen_zero(buttonconfig->label)
					   && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL)) {

//					buttonconfig->instance = btn[i].instance = i + 1;
					if (!sccp_strlen_zero(buttonconfig->button.speeddial.hint)
					    && btn[i].type == SCCP_BUTTONTYPE_MULTI				/* we can set our feature */
					    ) {
#ifdef CS_DYNAMIC_SPEEDDIAL
						if (d->inuseprotocolversion >= 15) {
							btn[i].type = 0x15;
							buttonconfig->instance = btn[i].instance = speeddialInstance++;
						} else 
#endif
						{
							btn[i].type = SKINNY_BUTTONTYPE_LINE;
							buttonconfig->instance = btn[i].instance = lineInstance++;
						}
					} else {
						btn[i].type = SKINNY_BUTTONTYPE_SPEEDDIAL;
						buttonconfig->instance = btn[i].instance = speeddialInstance++;

					}
					break;

				} else if (buttonconfig->type == FEATURE && !sccp_strlen_zero(buttonconfig->label)
					   && (btn[i].type == SCCP_BUTTONTYPE_MULTI)) {

					buttonconfig->instance = btn[i].instance = speeddialInstance++;

					switch (buttonconfig->button.feature.id) {
						case SCCP_FEATURE_HOLD:
							btn[i].type = SKINNY_BUTTONTYPE_HOLD;
							break;
#ifdef CS_DEVSTATE_FEATURE
							// case SCCP_FEATURE_DEVSTATE:
							//btn[i].type = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
							//break;
#endif
						case SCCP_FEATURE_TRANSFER:
							btn[i].type = SKINNY_BUTTONTYPE_TRANSFER;
							break;

						case SCCP_FEATURE_MONITOR:
							if (d->inuseprotocolversion > 15) {
								btn[i].type = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
							} else {
								btn[i].type = SKINNY_BUTTONTYPE_FEATURE;
							}
							break;

						case SCCP_FEATURE_MULTIBLINK:
							if (d->inuseprotocolversion >= 15) {
								btn[i].type = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
							} else {
								btn[i].type = SKINNY_BUTTONTYPE_FEATURE;
							}
							break;

						case SCCP_FEATURE_MOBILITY:
							btn[i].type = SKINNY_BUTTONTYPE_MOBILITY;
							break;

						case SCCP_FEATURE_CONFERENCE:
							btn[i].type = SKINNY_BUTTONTYPE_CONFERENCE;
							break;

						case SCCP_FEATURE_PICKUP:
							btn[i].type = SKINNY_STIMULUS_GROUPCALLPICKUP;
							break;

						case SCCP_FEATURE_DO_NOT_DISTURB:
							btn[i].type = SKINNY_BUTTONTYPE_DO_NOT_DISTURB;
							break;

						case SCCP_FEATURE_CONF_LIST:
							btn[i].type = SKINNY_BUTTONTYPE_CONF_LIST;
							break;

						case SCCP_FEATURE_REMOVE_LAST_PARTICIPANT:
							btn[i].type = SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT;
							break;

						case SCCP_FEATURE_HLOG:
							btn[i].type = SKINNY_BUTTONTYPE_HLOG;
							break;

						case SCCP_FEATURE_QRT:
							btn[i].type = SKINNY_BUTTONTYPE_QRT;
							break;

						case SCCP_FEATURE_CALLBACK:
							btn[i].type = SKINNY_BUTTONTYPE_CALLBACK;
							break;

						case SCCP_FEATURE_OTHER_PICKUP:
							btn[i].type = SKINNY_BUTTONTYPE_OTHER_PICKUP;
							break;

						case SCCP_FEATURE_VIDEO_MODE:
							btn[i].type = SKINNY_BUTTONTYPE_VIDEO_MODE;
							break;

						case SCCP_FEATURE_NEW_CALL:
							btn[i].type = SKINNY_BUTTONTYPE_NEW_CALL;
							break;

						case SCCP_FEATURE_END_CALL:
							btn[i].type = SKINNY_BUTTONTYPE_END_CALL;
							break;

						case SCCP_FEATURE_TESTE:
							btn[i].type = SKINNY_BUTTONTYPE_TESTE;
							break;

						case SCCP_FEATURE_TESTF:
							btn[i].type = SKINNY_BUTTONTYPE_TESTF;
							break;

						case SCCP_FEATURE_TESTI:
							btn[i].type = SKINNY_BUTTONTYPE_TESTI;
							break;

						case SCCP_FEATURE_TESTG:
							btn[i].type = SKINNY_BUTTONTYPE_MESSAGES;
							break;

						case SCCP_FEATURE_TESTH:
							btn[i].type = SKINNY_BUTTONTYPE_DIRECTORY;
							break;

						case SCCP_FEATURE_TESTJ:
							btn[i].type = SKINNY_BUTTONTYPE_APPLICATION;
							break;

						default:
							btn[i].type = SKINNY_BUTTONTYPE_FEATURE;
							break;

					}
					break;
				}
			}
			sccp_log((DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured %d Phone Button [%.2d] = %s (%s)\n", d->id, buttonconfig->index + 1, buttonconfig->instance, skinny_buttontype2str(btn[i].type), buttonconfig->label);
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);

		// all non defined buttons are set to UNUSED
		for (i = 0; i < StationMaxButtonTemplateSize; i++) {
			if (btn[i].type == SCCP_BUTTONTYPE_MULTI) {
				btn[i].type = SKINNY_BUTTONTYPE_UNUSED;
			}
		}

	} else {
		/* reserve one line as hotline */
		buttonconfig = SCCP_LIST_FIRST(&d->buttonconfig);
		btn[i].type = SKINNY_BUTTONTYPE_LINE;
		btn[i].ptr = sccp_line_retain(GLOB(hotline)->line);
		buttonconfig->instance = btn[i].instance = SCCP_FIRST_LINEINSTANCE;
		sccp_line_addDevice((sccp_line_t *) btn[i].ptr, d, btn[i].instance, &(buttonconfig->button.line.subscriptionId));

	}

	return btn;
}

/*!
 * \brief Handle Available Lines
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \callgraph
 * \callergraph
 */
void sccp_handle_AvailableLines(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint8_t i = 0, line_count = 0;
	btnlist *btn;

	line_count = 0;

	/** \todo why do we get the message twice  */
	if (d->linesRegistered) {
		return;
	}
	btn = d->buttonTemplate;

	if (!btn) {
		sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: no buttontemplate, reset device\n", DEV_ID_LOG(d));
		sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
		return;
	}

	/* count the available lines on the phone */
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if ((btn[i].type == SKINNY_BUTTONTYPE_LINE) || (btn[i].type == SCCP_BUTTONTYPE_MULTI))
			line_count++;
		else if (btn[i].type == SKINNY_BUTTONTYPE_UNUSED)
			break;
	}

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: Phone available lines %d\n", d->id, line_count);
	/*
	   if (d->isAnonymous == TRUE) {
	   l = GLOB(hotline)->line;
	   sccp_line_addDevice(l, d, 1, NULL);
	   } else {
	   sccp_buttonconfig_t *buttonconfig = NULL;
	   boolean_t defaultLineSet = FALSE;
	   for (i = 0; i < StationMaxButtonTemplateSize; i++) {
	   if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
	   AUTO_RELEASE sccp_line_t *l = sccp_line_retain(btn[i].ptr);
	   if (l) {
	   sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Attaching line %s with instance %d to this device\n", d->id, l->name, btn[i].instance);
	   SCCP_LIST_LOCK(&d->buttonconfig);
	   SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
	   if (btn[i].instance == buttonconfig->instance && buttonconfig->type == LINE) {
	   sccp_line_addDevice(l, d, btn[i].instance, &(buttonconfig->button.line.subscriptionId));
	   if (FALSE == defaultLineSet && !d->defaultLineInstance) {
	   d->defaultLineInstance = buttonconfig->instance;
	   defaultLineSet = TRUE;
	   }
	   }
	   }
	   SCCP_LIST_UNLOCK(&d->buttonconfig);
	   }
	   }
	   }
	   }
	 */
	d->linesRegistered = TRUE;
}

/*!
 * \brief Handle Accessory Status Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_accessorystatus_message(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint8_t id;
	uint8_t status;

	id = letohl(msg_in->data.AccessoryStatusMessage.lel_AccessoryID);
	status = letohl(msg_in->data.AccessoryStatusMessage.lel_AccessoryStatus);

	d->accessoryused = id;
	d->accessorystatus = status;
	switch (id) {
		case 1:
			d->accessoryStatus.headset = (status) ? TRUE : FALSE;
			break;
		case 2:
			d->accessoryStatus.handset = (status) ? TRUE : FALSE;
			break;
		case 3:
			d->accessoryStatus.speaker = (status) ? TRUE : FALSE;
			// should we not also set d->state = SCCP_DEVICESTATE_OFFHOOK / SCCP_DEVICESTATE_ONHOOK
			break;
	}

	sccp_log((DEBUGCAT_MESSAGE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s'\n", DEV_ID_LOG(d), sccp_accessory2str(d->accessoryused), sccp_accessorystate2str(d->accessorystatus));
}

/*!
 * \brief Handle Device Unregister
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_unregister(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out;
	int reason = letohl(msg_in->data.UnregisterMessage.lel_UnregisterReason);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Unregister request Received (Reason: %s)\n", DEV_ID_LOG(d), reason ? "Unknown" : "Powersave");

	/* we don't need to look for active channels. the phone does send unregister only when there are no channels */
	REQ(msg_out, UnregisterAckMessage);
	msg_out->data.UnregisterAckMessage.lel_status = SKINNY_UNREGISTERSTATUS_OK;
	sccp_session_send2(s, msg_out);										// send directly to session, skipping device check
	sccp_log((DEBUGCAT_MESSAGE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: unregister request sent\n", DEV_ID_LOG(d));
	sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_NONE);
}

/*!
 * \brief Handle Button Template Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
void sccp_handle_button_template_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	btnlist *btn;
	int i;
	uint8_t buttonCount = 0, lastUsedButtonPosition = 0;

	sccp_msg_t *msg_out;

	if (d->registrationState != SKINNY_DEVICE_RS_PROGRESS && d->registrationState != SKINNY_DEVICE_RS_OK) {
		pbx_log(LOG_WARNING, "%s: Received a button template request from unregistered device\n", d->id);
		sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
		return;
	}

	/* pre-attach lines. We will wait for button template req if the phone does support it */
	if (d->buttonTemplate) {
		sccp_free(d->buttonTemplate);
	}
	btn = d->buttonTemplate = sccp_make_button_template(d);

	/* update lineButtons array */
	sccp_line_createLineButtonsArray(d);

	if (!btn) {
		pbx_log(LOG_ERROR, "%s: No memory allocated for button template\n", d->id);
		sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
		return;
	}

	REQ(msg_out, ButtonTemplateMessage);
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		msg_out->data.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;

		if (SKINNY_BUTTONTYPE_UNUSED != btn[i].type) {
			//msg_out->data.ButtonTemplateMessage.lel_buttonCount = i+1;
			buttonCount = i + 1;
			lastUsedButtonPosition = i;
		}

		switch (btn[i].type) {
			case SCCP_BUTTONTYPE_HINT:
			case SCCP_BUTTONTYPE_LINE:
				/* we do not need a line if it is not configured */
				if (msg_out->data.ButtonTemplateMessage.definition[i].instanceNumber == 0) {
					msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;
				} else {
					msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_LINE;
				}
				break;

			case SCCP_BUTTONTYPE_SPEEDDIAL:
				msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SPEEDDIAL;
				break;

			case SKINNY_BUTTONTYPE_SERVICEURL:
				msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SERVICEURL;
				break;

			case SKINNY_BUTTONTYPE_FEATURE:
				msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_FEATURE;
				break;

			case SCCP_BUTTONTYPE_MULTI:
				//msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_DISPLAY;
				//break;

			case SKINNY_BUTTONTYPE_UNUSED:
				msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;

				break;

			default:
				msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition = btn[i].type;
				break;
		}
		sccp_log((DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %d (%d)\n", d->id, i + 1, msg_out->data.ButtonTemplateMessage.definition[i].buttonDefinition, msg_out->data.ButtonTemplateMessage.definition[i].instanceNumber);

	}

	msg_out->data.ButtonTemplateMessage.lel_buttonOffset = 0;
	//msg_out->data.ButtonTemplateMessage.lel_buttonCount = htolel(msg_out->data.ButtonTemplateMessage.lel_buttonCount);
	msg_out->data.ButtonTemplateMessage.lel_buttonCount = htolel(buttonCount);
	/* buttonCount is already in a little endian format so don't need to convert it now */
	msg_out->data.ButtonTemplateMessage.lel_totalButtonCount = htolel(lastUsedButtonPosition + 1);

	/* set speeddial for older devices like 7912 */
	uint32_t speeddialInstance = 0;
	sccp_buttonconfig_t *config;

	sccp_log((DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_SPEEDDIAL)) (VERBOSE_PREFIX_3 "%s: configure unconfigured speeddialbuttons \n", d->id);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		/* we found an unconfigured speeddial */
		if (config->type == SPEEDDIAL && config->instance == 0) {
			config->instance = speeddialInstance++;
		} else if (config->type == SPEEDDIAL && config->instance != 0) {
			speeddialInstance = config->instance + 1;
		}
	}
	/* done */

	sccp_dev_send(d, msg_out);
	/*
	   sccp_msg_t *dynamicR;

	   int hdr_len = sizeof(dynamicR->data.ButtonTemplateMessageDynamic) - sizeof(dynamicR->data.ButtonTemplateMessageDynamic.dummy);
	   int dummy_len = (lastUsedButtonPosition + 1) * sizeof(StationButtonDefinition);
	   int padding = ((dummy_len + hdr_len) % 4);

	   padding = (padding > 0) ? 4 - padding : 4;

	   dynamicR = sccp_build_packet(ButtonTemplateMessage, hdr_len + dummy_len + padding);
	   dynamicR->data.ButtonTemplateMessageDynamic.lel_buttonOffset = 0;
	   dynamicR->data.ButtonTemplateMessageDynamic.lel_buttonCount = htolel(buttonCount);
	   dynamicR->data.ButtonTemplateMessageDynamic.lel_totalButtonCount = htolel(lastUsedButtonPosition + 1);
	   memcpy(&dynamicR->data.ButtonTemplateMessageDynamic.dummy, &msg_out->data.ButtonTemplateMessage.definition, dummy_len);

	   sccp_dev_send(d, dynamicR);
	   sccp_free(msg_out);
	 */
}

/*!
 * \brief Handle Line Number for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_line_number(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out;
	sccp_speed_t k;
	sccp_buttonconfig_t *config;

	uint8_t lineNumber = letohl(msg_in->data.LineStatReqMessage.lel_lineNumber);

	sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Configuring line number %d\n", d->id, lineNumber);

	/* if we find no regular line - it can be a speeddial with hint */
	AUTO_RELEASE sccp_line_t *l = sccp_line_find_byid(d, lineNumber);

	if (!l) {
		sccp_dev_speed_find_byindex(d, lineNumber, TRUE, &k);
	}

	REQ(msg_out, LineStatMessage);
	if (!l && !k.valid) {
		pbx_log(LOG_ERROR, "%s: requested a line configuration for unknown line/speeddial %d\n", DEV_ID_LOG(s->device), lineNumber);
		msg_out->data.LineStatMessage.lel_lineNumber = htolel(lineNumber);
		sccp_dev_send(s->device, msg_out);
		return;
	}
	msg_out->data.LineStatMessage.lel_lineNumber = htolel(lineNumber);

	//sccp_copy_string(msg_out->data.LineStatMessage.lineDirNumber, ((l) ? l->name : k.name), sizeof(msg_out->data.LineStatMessage.lineDirNumber));
	d->copyStr2Locale(d, msg_out->data.LineStatMessage.lineDirNumber, ((l) ? l->name : k.name), sizeof(msg_out->data.LineStatMessage.lineDirNumber));

	/* lets set the device description for the first line, so it will be display on top of device -MC */
	if (lineNumber == 1) {
		//sccp_copy_string(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName, (d->description), sizeof(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName));
		d->copyStr2Locale(d, msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName, (d->description), sizeof(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName));
	} else {
		//sccp_copy_string(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName, ((l) ? l->description : k.name), sizeof(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName));
		d->copyStr2Locale(d, msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName, ((l) ? l->description : k.name), sizeof(msg_out->data.LineStatMessage.lineFullyQualifiedDisplayName));
	}
	//sccp_copy_string(msg_out->data.LineStatMessage.lineDisplayName, ((l) ? l->label : k.name), sizeof(msg_out->data.LineStatMessage.lineDisplayName));
	d->copyStr2Locale(d, msg_out->data.LineStatMessage.lineDisplayName, ((l) ? l->label : k.name), sizeof(msg_out->data.LineStatMessage.lineDisplayName));

	sccp_dev_send(d, msg_out);

	if (l) {
		/* set default line on device if based on "default" config option */
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == LINE && config->instance == lineNumber) {
				if (strcasestr(config->button.line.options, "default")) {
					d->defaultLineInstance = lineNumber;
					sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", lineNumber);
				}
				break;
			}
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
	}
}

/*!
 * \brief Handle SpeedDial Status Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_speed_t k;
	sccp_msg_t *msg_out;

	int wanted = letohl(msg_in->data.SpeedDialStatReqMessage.lel_speedDialNumber);

	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Speed Dial Request for Button %d\n", DEV_ID_LOG(s->device), wanted);

	REQ(msg_out, SpeedDialStatMessage);
	msg_out->data.SpeedDialStatMessage.lel_speedDialNumber = htolel(wanted);

	sccp_dev_speed_find_byindex(s->device, wanted, FALSE, &k);
	if (k.valid) {
		d->copyStr2Locale(d, msg_out->data.SpeedDialStatMessage.speedDialDirNumber, k.ext, sizeof(msg_out->data.SpeedDialStatMessage.speedDialDirNumber));
		d->copyStr2Locale(d, msg_out->data.SpeedDialStatMessage.speedDialDisplayName, k.name, sizeof(msg_out->data.SpeedDialStatMessage.speedDialDisplayName));
	} else {
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: speeddial %d unknown\n", DEV_ID_LOG(s->device), wanted);
	}

	sccp_dev_send(d, msg_out);
}

/*!
 * \brief Handle LastNumberRedial Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_lastnumberredial(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle LastNumber Redial Stimulus\n", d->id);

	if (sccp_strlen_zero(d->redialInformation.number)) {
		pbx_log(LOG_NOTICE, "%s: (lastnumberredial) No last number stored to dial\n", d->id);
		return;
	}
	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	if (channel) {
		if (channel->state == SCCP_CHANNELSTATE_OFFHOOK) {
			sccp_copy_string(channel->dialedNumber, d->redialInformation.number, sizeof(d->redialInformation.number));
			sccp_pbx_softswitch(channel);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Redial the number %s\n", d->id, d->redialInformation.number);
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Redial ignored as call in progress\n", d->id);
		}
	} else {
		channel = sccp_channel_newcall(l, d, d->redialInformation.number, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
	}
}

/*!
 * \brief Handle Speeddial Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_speeddial(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Speeddial Stimulus\n", d->id);

	sccp_speed_t k;

	sccp_dev_speed_find_byindex(d, instance, FALSE, &k);
	if (k.valid) {
		sccp_handle_speeddial(d, &k);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Speeddial Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_blfspeeddial(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle BlfSpeeddial Stimulus\n", d->id);

	sccp_speed_t k;

	sccp_dev_speed_find_byindex(d, instance, TRUE, &k);
	if (k.valid) {
		sccp_handle_speeddial(d, &k);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to BlfSpeeddial %d\n", d->id, instance);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Line Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_line(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Line Button Stimulus\n", d->id);

	/* Mandatory adhoc for Anonymous Phones */
	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);							/* use adhoc dial feature with hotline */
		return;
	}

	/* for 7960's we use line keys to display hinted speeddials (Trick), without a hint it would have been a speeddial */
	if (!l) {
		sccp_speed_t k;

		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Handle (BLF) Speeddial Stimulus. Looking for a speeddial-instance:%d with hint\n", d->id, instance);
//		sccp_handle_stimulus_speeddial(d, l, instance, callId, stimulusstatus);
		sccp_dev_speed_find_byindex(d, instance, TRUE, &k);
		if (k.valid) {
			sccp_handle_speeddial(d, &k);
			return;
		}
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);

		return;
	}

	/*
	 * \note Check speeddial before handling adHoc allows speeddials to be used and makes adHoc Non-Mandatory (This is a personal Preference - DdG). 
	 * To make adhoc mandatory you can close it down in the dialplan. 
	 */
	if (strlen(l->adhocNumber) > 0) {
		sccp_feat_adhocDial(d, l);
		return;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Line Key press on line %s\n", d->id, (l) ? l->name : "(nil)");

	// Handle Local Line
	{
		AUTO_RELEASE sccp_channel_t *channel = NULL;

		if (instance && callId) {
			channel = sccp_find_channel_by_lineInstance_and_callid(d, instance, callId);		/* newer phones */
		} else {
			channel = sccp_device_getActiveChannel(d);						/* older phones don't provide instance or callid */
		}
		if (channel) {
			AUTO_RELEASE sccp_device_t *check_device = sccp_channel_getDevice_retained(channel);

			if (channel && check_device == d) {							// check to see if we own the channel (otherwise it would be a shared line owned by another device)
				sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: gotten active channel %d on line %s\n", d->id, channel->callid, (l) ? l->name : "(nil)");
				if (channel->state == SCCP_CHANNELSTATE_CONNECTED) {				/* incoming call on other line */
					if (sccp_channel_hold(channel)) {
						sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: call (%d) put on hold on line %s\n", d->id, channel->callid, l->name);
					} else {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold failed for call (%d), line %s\n", d->id, channel->callid, l->name);
						return;
					}
				} else {									/* ??? No idea when this is supposed to happen */
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call not in progress. Closing line %s\n", d->id, (l) ? l->name : "(nil)");
					sccp_channel_endcall(channel);
					sccp_dev_deactivate_cplane(d);
					return;
				}
			}
		}
	}
	// Handle Shared Line
	{
		AUTO_RELEASE sccp_channel_t *tmpChannel = NULL;

		if (!SCCP_RWLIST_GETSIZE(&l->channels)) {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: no activate channel on line %s\n -> New Call", DEV_ID_LOG(d), (l) ? l->name : "(nil)");
			sccp_dev_set_activeline(d, l);
			sccp_dev_set_cplane(d, instance, 1);
			tmpChannel = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		} else if ((tmpChannel = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGING))) {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Answering incoming/ringing line %d", d->id, instance);
			sccp_channel_answer(d, tmpChannel);
		} else if ((tmpChannel = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))) {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Channel count on line %d = %d", d->id, instance, SCCP_RWLIST_GETSIZE(&l->channels));
			if (SCCP_RWLIST_GETSIZE(&l->channels) == 1) {						/* only one call on hold, so resume that one */
				//channel = SCCP_LIST_FIRST(&l->channels);
				sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Resume channel %d on line %d", d->id, tmpChannel->callid, instance);
				sccp_dev_set_activeline(d, l);
				sccp_channel_resume(d, tmpChannel, TRUE);
				sccp_dev_set_cplane(d, instance, 1);
			} else {										/* not sure which channel to make activem let the user decide */
				sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Switch to line %d", d->id, instance);
				sccp_dev_set_activeline(d, l);
				sccp_dev_set_cplane(d, instance, 1);
			}
		} else {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: no activate channel on line %s for this phone\n -> New Call", DEV_ID_LOG(d), (l) ? l->name : "(nil)");
			sccp_dev_set_activeline(d, l);
			sccp_dev_set_cplane(d, instance, 1);
			tmpChannel = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		}
	}
}

/*!
 * \brief Handle Hold Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_hold(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	/* this is the hard hold button. When we are here we are putting on hold the active_channel */
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Hold/Resume Stimulus on  line %d\n", d->id, instance);

	AUTO_RELEASE sccp_channel_t *channel1 = NULL;

	if ((channel1 = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED))) {
		sccp_channel_hold(channel1);
		return;
	} else if ((channel1 = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))) {
		AUTO_RELEASE sccp_channel_t *channel2 = sccp_device_getActiveChannel(d);

		if (channel2 && channel2->state == SCCP_CHANNELSTATE_OFFHOOK) {
			if (channel2->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				sccp_channel_endcall(channel2);
			} else {
				return;										/* new since 2014-6-5: Prevent accidental resume when we have in inbound call we are trying to answeer */
			}
		}
		sccp_channel_resume(d, channel1, TRUE);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No call to resume/hold found on line %d\n", d->id, instance);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Transfer Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_transfer(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Transfer Stimulus\n", d->id);
	if (!d->transfer) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device\n", d->id);
		return;
	}
	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	if (channel) {
		sccp_channel_transfer(channel, d);
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No call to transfer found on line %d\n", d->id, instance);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Voicemail Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId Call ID as uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_voicemail(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Voicemail Stimulus\n", d->id);
	sccp_feat_voicemail(d, instance);
}

/*!
 * \brief Handle Conference Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_conference(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Conference Stimulus\n", d->id);
	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	if (channel) {
		sccp_feat_handle_conference(d, l, instance, channel);
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No call to handle conference for on line %d\n", d->id, instance);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Forward All Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_forwardAll(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Forward All Stimulus\n", d->id);
	if (d->cfwdall) {
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_ALL);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDALL disabled on device\n", d->id);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Forward Busy Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_forwardBusy(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Forward Busy Stimulus\n", d->id);
	if (d->cfwdbusy) {
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_BUSY);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY disabled on device\n", d->id);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Forward NoAnswer Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_forwardNoAnswer(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Forward NoAnswer Stimulus\n", d->id);
	if (d->cfwdnoanswer) {
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_NOANSWER);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDNoAnswer disabled on device\n", d->id);
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Call Park Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_callpark(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Call Park Stimulus\n", d->id);
#ifdef CS_SCCP_PARK
	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	if (channel) {
		sccp_channel_park(channel);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Cannot park while no calls in progress\n", d->id);
#else
	sccp_log((DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
	sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
}

/*!
 * \brief Handle Group Call Pickup Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_groupcallpickup(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Group Call Pickup Stimulus\n", d->id);
#ifdef CS_SCCP_PICKUP
	/*! \todo use feature map or sccp_feat_handle_directed_pickup */
	//sccp_feat_handle_directed_pickup(l, 1, d);
	AUTO_RELEASE sccp_channel_t *new_channel = NULL;

	if (!(new_channel = sccp_channel_newcall(l, d, "pickupexten", SKINNY_CALLTYPE_OUTBOUND, NULL, NULL))) {
	        pbx_log(LOG_ERROR, "%s: (grouppickup) Cannot start a new channel\n", d->id);
	}
#else
	sccp_log((DEBUGCAT_FEATURE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "### Native GROUP PICKUP was not compiled in\n");
#endif
}

/*!
 * \brief Handle Featere Action Stimulus
 * \param d SCCP Device
 * \param l SCCP Line
 * \param instance uint8_t
 * \param callId uint32_t
 * \param stimulusstatus uint32_t
 */
static void sccp_handle_stimulus_feature(sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus)
{
	sccp_log_and((DEBUGCAT_CORE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle Feature Button Stimulus (status: %d)\n", d->id, stimulusstatus);
	sccp_handle_feature_action(d, instance, TRUE);
}

static const struct _skinny_stimulusMap_cb {
	void (*const handler_cb) (sccp_device_t * d, sccp_line_t * l, uint8_t instance, uint32_t callId, uint32_t stimulusstatus);
	boolean_t lineRequired;
} skinny_stimulusMap_cb[] = {
	/* *INDENT-OFF* */
	[SKINNY_STIMULUS_UNUSED] 			= {NULL, TRUE},
	[SKINNY_STIMULUS_LASTNUMBERREDIAL] 		= {sccp_handle_stimulus_lastnumberredial, TRUE},
	[SKINNY_STIMULUS_SPEEDDIAL] 			= {sccp_handle_stimulus_speeddial, FALSE},
	[SKINNY_STIMULUS_BLFSPEEDDIAL] 			= {sccp_handle_stimulus_blfspeeddial, FALSE},
	[SKINNY_STIMULUS_LINE] 				= {sccp_handle_stimulus_line, FALSE},
	[SKINNY_STIMULUS_HOLD] 				= {sccp_handle_stimulus_hold, TRUE},
	[SKINNY_STIMULUS_TRANSFER] 			= {sccp_handle_stimulus_transfer, TRUE},
	[SKINNY_STIMULUS_VOICEMAIL] 			= {sccp_handle_stimulus_voicemail, TRUE},
	[SKINNY_STIMULUS_CONFERENCE] 			= {sccp_handle_stimulus_conference, TRUE},
	[SKINNY_STIMULUS_FORWARDALL] 			= {sccp_handle_stimulus_forwardAll, TRUE},
	[SKINNY_STIMULUS_FORWARDBUSY] 			= {sccp_handle_stimulus_forwardBusy, TRUE},
	[SKINNY_STIMULUS_FORWARDNOANSWER] 		= {sccp_handle_stimulus_forwardNoAnswer, TRUE},
	[SKINNY_STIMULUS_CALLPARK] 			= {sccp_handle_stimulus_callpark, TRUE},
	[SKINNY_STIMULUS_GROUPCALLPICKUP] 		= {sccp_handle_stimulus_groupcallpickup, TRUE},
	[SKINNY_STIMULUS_FEATURE] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_MOBILITY] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_MULTIBLINKFEATURE] 		= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_DO_NOT_DISTURB] 		= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_QRT] 				= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_CALLBACK] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_OTHER_PICKUP] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_VIDEO_MODE] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_NEW_CALL] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_END_CALL] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_HLOG] 				= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_TESTE] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_TESTF] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_TESTI] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_MESSAGES] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_DIRECTORY] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_APPLICATION] 			= {sccp_handle_stimulus_feature, FALSE},
	[SKINNY_STIMULUS_DISPLAY] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_T120CHAT] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_T120WHITEBOARD] 		= {NULL, FALSE},
	[SKINNY_STIMULUS_T120APPLICATIONSHARING]	= {NULL, FALSE},
	[SKINNY_STIMULUS_T120FILETRANSFER] 		= {NULL, FALSE},
	[SKINNY_STIMULUS_VIDEO] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_ANSWERRELEASE] 		= {NULL, FALSE},
	[SKINNY_STIMULUS_AUTOANSWER] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_SELECT] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_SERVICEURL] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_MALICIOUSCALL] 		= {NULL, FALSE},
	[SKINNY_STIMULUS_GENERICAPPB1] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_GENERICAPPB2] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_GENERICAPPB3] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_GENERICAPPB4] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_GENERICAPPB5] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_MEETMECONFERENCE] 		= {NULL, FALSE},
	[SKINNY_STIMULUS_CALLPICKUP] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_CONF_LIST] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_REMOVE_LAST_PARTICIPANT]	= {NULL, FALSE},
	[SKINNY_STIMULUS_QUEUING] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_HEADSET] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_KEYPAD] 			= {NULL, FALSE},
	[SKINNY_STIMULUS_AEC] 				= {NULL, FALSE},
	[SKINNY_STIMULUS_UNDEFINED] 			= {NULL, FALSE},
	/* *INDENT-ON* */
};

/*!
 * \brief Handle Stimulus for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \callgraph
 * \callergraph
 */
void sccp_handle_stimulus(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	AUTO_RELEASE sccp_line_t *l = NULL;
	uint32_t callId = 0;
	uint32_t stimulusStatus = 0;

	skinny_stimulus_t stimulus = letohl(msg_in->data.StimulusMessage.lel_stimulus);
	uint8_t instance = letohl(msg_in->data.StimulusMessage.lel_stimulusInstance);

	if (msg_in->header.length > 12) {
		callId = letohl(msg_in->data.StimulusMessage.lel_callReference);
		stimulusStatus = letohl(msg_in->data.StimulusMessage.lel_stimulusStatus);
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got stimulus=%s (%d) for instance=%d, callreference=%d, status=%d\n", d->id, skinny_stimulus2str(stimulus), stimulus, instance, callId, stimulusStatus);
	if (!instance) {											/*! \todo also use the callReference if available */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Instance 0 is not a valid instance. Trying the active line %d\n", d->id, instance);
		if ((l = sccp_dev_get_activeline(d))) {
			instance = sccp_device_find_index_for_line(d, l->name);
		} else {
			instance = (d->defaultLineInstance > 0) ? d->defaultLineInstance : SCCP_FIRST_LINEINSTANCE;
		}
	}
	if (!l) {
		// \todo ADD Use of CallReference !!!!
		l = sccp_line_find_byid(d, instance);
	}

	if (stimulus > SKINNY_STIMULUS_UNUSED && stimulus < SKINNY_STIMULUS_UNDEFINED && skinny_stimulusMap_cb[stimulus].handler_cb) {
		if (!skinny_stimulusMap_cb[stimulus].lineRequired || (skinny_stimulusMap_cb[stimulus].lineRequired && l)) {
			skinny_stimulusMap_cb[stimulus].handler_cb(d, l, instance, callId, stimulusStatus);
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line found to handle stimulus\n", d->id);
			return;
		}
	} else {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Got stimulus=%s (%d), which does not have a handling function. Not Handled\n", d->id, skinny_stimulus2str(stimulus), stimulus);
	}
}

/*!
 * \brief Handle SpeedDial for Device
 * \param d SCCP Device as sccp_device_t
 * \param k SCCP SpeedDial as sccp_speed_t
 */
void sccp_handle_speeddial(sccp_device_t * d, const sccp_speed_t * k)
{
	int len;

	if (!k || !d || !d->session) {
		return;
	}
	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Speeddial Button (%d) pressed, configured number is (%s)\n", d->id, k->instance, k->ext);
	if (channel) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: channel state %d\n", DEV_ID_LOG(d), channel->state);

		// Channel already in use
		if ((channel->state == SCCP_CHANNELSTATE_DIALING) || (channel->state == SCCP_CHANNELSTATE_GETDIGITS) || (channel->state == SCCP_CHANNELSTATE_DIGITSFOLL) || (channel->state == SCCP_CHANNELSTATE_OFFHOOK)) {
			len = strlen(channel->dialedNumber);
			sccp_copy_string(channel->dialedNumber + len, k->ext, sizeof(channel->dialedNumber) - len);
			sccp_pbx_softswitch(channel);
			return;
		} else if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_PROCEED) {
			// automatically put on hold
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: automatically put call %d on hold %d\n", DEV_ID_LOG(d), channel->callid, channel->state);
			sccp_channel_hold(channel);
			AUTO_RELEASE sccp_line_t *l = sccp_dev_get_activeline(d);

			if (l) {
				AUTO_RELEASE sccp_channel_t *new_channel = NULL;

				new_channel = sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
			}
			return;
		}
		// Channel not in use
		if (PBX(send_digits)) {
			PBX(send_digits) (channel, k->ext);
		}
	} else {
		/* check Remote RINGING + gpickup */
		AUTO_RELEASE sccp_line_t *l = NULL;

		if (d->defaultLineInstance > 0) {
			sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = sccp_dev_get_activeline(d);
		}
		if (!l) {
			sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using first line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, SCCP_FIRST_LINEINSTANCE);
		}
		if (l) {
			AUTO_RELEASE sccp_channel_t *new_channel = NULL;

			new_channel = sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		}
	}
}

/*!
 * \brief Handle Off Hook Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_offhook(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);
		return;
	}

	AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

	if (channel) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Taken Offhook with a call (%d) in progess. Skip it!\n", d->id, channel->callid);
		return;
	}

	/* we need this for callwaiting, hold, answer and stuff */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Taken Offhook\n", d->id);
	d->state = SCCP_DEVICESTATE_OFFHOOK;

	/* checking for registered lines */
	if (!d->configurationStatistic.numberOfLines) {
		pbx_log(LOG_NOTICE, "No lines registered on %s for take OffHook\n", DEV_ID_LOG(s->device));
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_NO_LINES_REGISTERED, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}
	/* end line check */

	/* \todo This should be changed, to handle and atomic version of sccp_channel_answer if it would return Success/Failed
	 * (think of two phones on a shared line, picking up at the same time) 
	 */
	if ((channel = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_RINGING))) {
		/* Answer the ringing channel. */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer channel\n", d->id);
		sccp_channel_answer(d, channel);
	} else {
		/* use default line if it is set */
		AUTO_RELEASE sccp_line_t *l = NULL;

		if (d->defaultLineInstance > 0) {
			sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = sccp_dev_get_activeline(d);
		}
		if (!l) {
			sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using first line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, SCCP_FIRST_LINEINSTANCE);
		}

		if (l) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Using line %s\n", d->id, l->name);
			AUTO_RELEASE sccp_channel_t *new_channel = NULL;

			new_channel = sccp_channel_newcall(l, d, !sccp_strlen_zero(l->adhocNumber) ? l->adhocNumber : NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		}
	}
}

/*!
 * \brief Handle BackSpace Event for Device
 * \param d SCCP Device as sccp_device_t
 * \param line Line Number as uint8_t
 * \param callid Call ID as uint32_t
 */
void sccp_handle_backspace(sccp_device_t * d, uint8_t line, uint32_t callid)
{
	sccp_msg_t *msg_out;

	if (!d || !d->session) {
		return;
	}
	REQ(msg_out, BackSpaceReqMessage);
	msg_out->data.BackSpaceReqMessage.lel_lineInstance = htolel(line);
	msg_out->data.BackSpaceReqMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, msg_out);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Backspace request on line instance %u, call %u.\n", d->id, line, callid);
}

/*!
 * \brief Handle On Hook Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
void sccp_handle_onhook(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{

	uint32_t lineInstance = letohl(msg_in->data.OnHookMessage.lel_lineInstance);
	uint32_t callid = letohl(msg_in->data.OnHookMessage.lel_callReference);

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_ONHOOK;
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: is Onhook\n", DEV_ID_LOG(d));

	if (!(d->lineButtons.size > SCCP_FIRST_LINEINSTANCE)) {
		pbx_log(LOG_NOTICE, "No lines registered on %s to put OnHook\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_NO_LINES_REGISTERED, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}

	AUTO_RELEASE sccp_channel_t *channel = NULL;

	if (lineInstance && callid) {
		channel = sccp_find_channel_by_lineInstance_and_callid(d, lineInstance, callid);
	} else {
		channel = sccp_device_getActiveChannel(d);
	}
	if (channel) {
		if (!GLOB(transfer_on_hangup) || !sccp_channel_transfer_on_hangup(channel)) {
			sccp_channel_endcall(channel);
		}
	} else {
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
		sccp_dev_stoptone(d, 0, 0);
	}

	return;
}

/*!
 * \brief Handle On Hook Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 * \note this is used just in protocol v3 stuff, it has been included in 0x004A AccessoryStatusMessage
 */
void sccp_handle_headset(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	/*
	 * this is used just in protocol v3 stuff
	 * it has been included in 0x004A AccessoryStatusMessage
	 */
	uint32_t headsetmode = letohl(msg_in->data.HeadsetStatusMessage.lel_hsMode);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s' (%u)\n", DEV_ID_LOG(s->device), sccp_accessory2str(SCCP_ACCESSORY_HEADSET), sccp_accessorystate2str(headsetmode), 0);
}

/*!
 * \brief Handle Capabilities for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	int i;
	skinny_codec_t codec;

	if (!d) {
		return;
	}

	uint8_t n = letohl(msg_in->data.CapabilitiesResMessage.lel_count);

	sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Capabilities\n", DEV_ID_LOG(d), n);
	for (i = 0; i < n; i++) {
		codec = letohl(msg_in->data.CapabilitiesResMessage.caps[i].lel_payloadCapability);
		d->capabilities.audio[i] = codec;
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP:%6d %-25s\n", d->id, codec, codec2str(codec));
	}

	if ((SKINNY_CODEC_NONE == d->preferences.audio[0])) {
		/* we have no preferred codec, use capabilities -MC */
		memcpy(&d->preferences.audio, &d->capabilities.audio, sizeof(d->preferences.audio));
	}
	
	char cap_buf[512];
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: num of codecs %d, capabilities: %s\n", DEV_ID_LOG(d), (int) ARRAY_LEN(d->capabilities.audio), cap_buf);
}

/*!
 * \brief Handle Soft Key Template Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint8_t i;
	sccp_msg_t *msg_out;

	/* ok the device support the softkey map */
	d->softkeysupport = 1;

	int arrayLen = ARRAY_LEN(softkeysmap);
	int dummy_len = arrayLen * (sizeof(StationSoftKeyDefinition));
	int hdr_len = sizeof(msg_in->data.SoftKeyTemplateResMessage);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	/* create message */
	msg_out = sccp_build_packet(SoftKeyTemplateResMessage, hdr_len + dummy_len + padding);
	msg_out->data.SoftKeyTemplateResMessage.lel_softKeyOffset = 0;

	for (i = 0; i < arrayLen; i++) {
		switch (softkeysmap[i]) {
			case SKINNY_LBL_EMPTY:
				// msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 0;
				// msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = 0;
			case SKINNY_LBL_DIAL:
				sccp_copy_string(msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel, label2str(softkeysmap[i]), StationMaxSoftKeyLabelSize);
				sccp_log((DEBUGCAT_SOFTKEY + DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel);
				break;
			case SKINNY_LBL_MONITOR:
				sccp_copy_string(msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel, label2str(softkeysmap[i]), StationMaxSoftKeyLabelSize);
				sccp_log((DEBUGCAT_SOFTKEY + DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel);
				break;
#ifdef CS_SCCP_CONFERENCE
			case SKINNY_LBL_CONFRN:
			case SKINNY_LBL_JOIN:
			case SKINNY_LBL_CONFLIST:
				if (d->allow_conference) {
					msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;	/* adding "\200" upfront to indicate that we are using an embedded/xml label */
					msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
				}
				break;
#endif
			default:
				msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;	/* adding "\200" upfront to indicate that we are using an embedded/xml label */
				msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
				sccp_log((DEBUGCAT_SOFTKEY + DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, label2str(msg_out->data.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1]));
		}
		msg_out->data.SoftKeyTemplateResMessage.definition[i].lel_softKeyEvent = htolel(i + 1);
	}

	msg_out->data.SoftKeyTemplateResMessage.lel_softKeyCount = htolel(arrayLen);
	msg_out->data.SoftKeyTemplateResMessage.lel_totalSoftKeyCount = htolel(arrayLen);
	sccp_dev_send(d, msg_out);
}

/*!
 * \brief Handle Set Soft Key Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{

	int iKeySetCount = 0;
	sccp_msg_t *msg_out;
	uint8_t i = 0;
	uint8_t trnsfvm = 0;
	uint8_t meetme = 0;

#ifdef CS_SCCP_PICKUP
	uint8_t pickupgroup = 0;
#endif

	/* set softkey definition */
	sccp_softKeySetConfiguration_t *softkeyset;
	d->softkeyset = NULL;

	if (!sccp_strlen_zero(d->softkeyDefinition)) {
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: searching for softkeyset: %s!\n", d->id, d->softkeyDefinition);
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
			if (sccp_strcaseequals(d->softkeyDefinition, softkeyset->name)) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: using softkeyset: %s (%p)!\n", d->id, softkeyset->name, softkeyset);
				d->softkeyset = softkeyset;
				d->softKeyConfiguration.modes = softkeyset->modes;
				d->softKeyConfiguration.size = softkeyset->numberOfSoftKeySets;
			}
		}
		SCCP_LIST_UNLOCK(&softKeySetConfig);
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: d->softkeyDefinition=%s!\n", d->id, d->softkeyDefinition);
	}
	
	if (!d->softkeyset) {
		pbx_log(LOG_WARNING, "SCCP: Defined softkeyset: '%s' could not be found. Falling back to 'default' instead !\n", d->softkeyDefinition);
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
			if (sccp_strcaseequals("default", softkeyset->name)) {
				d->softkeyset = softkeyset;
				d->softKeyConfiguration.modes = softkeyset->modes;
				d->softKeyConfiguration.size = softkeyset->numberOfSoftKeySets;
			}
		}
		SCCP_LIST_UNLOCK(&softKeySetConfig);
	}

	/* end softkey definition */
	const softkey_modes *v = d->softKeyConfiguration.modes;
	const uint8_t v_count = d->softKeyConfiguration.size;
	const uint8_t *b;

	REQ(msg_out, SoftKeySetResMessage);
	msg_out->data.SoftKeySetResMessage.lel_softKeySetOffset = htolel(0);

	/* look for line trnsvm */
	sccp_buttonconfig_t *buttonconfig;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			AUTO_RELEASE sccp_line_t *l = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);

			if (l) {
				if (!sccp_strlen_zero(l->trnsfvm)) {
					trnsfvm = 1;
				}
				if (l->meetme) {
					meetme = 1;
				}
				if (!sccp_strlen_zero(l->meetmenum)) {
					meetme = 1;
				}
#ifdef CS_SCCP_PICKUP
				if (l->pickupgroup) {
					pickupgroup = 1;
				}
#ifdef CS_AST_HAS_NAMEDGROUP
				if (!sccp_strlen_zero(l->namedpickupgroup)) {
					pickupgroup = 1;
				}
#endif
#endif
			}
		}
	}

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: softkey count: %d\n", d->id, v_count);

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRANSFER        is %s\n", d->id, (d->transfer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: DND             is %s\n", d->id, (d->dndFeature.status) ? sccp_dndmode2str(d->dndFeature.status) : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PRIVATE         is %s\n", d->id, (d->privacyFeature.enabled) ? "enabled" : "disabled");
#ifdef CS_SCCP_PARK
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PARK            is  %s\n", d->id, (d->park) ? "enabled" : "disabled");
#endif
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDALL         is  %s\n", d->id, (d->cfwdall) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY        is  %s\n", d->id, (d->cfwdbusy) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER    is  %s\n", d->id, (d->cfwdnoanswer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRNSFVM/IDIVERT is  %s\n", d->id, (trnsfvm) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: MEETME          is  %s\n", d->id, (meetme) ? "enabled" : "disabled");
#ifdef CS_SCCP_PICKUP
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPGROUP     is  %s\n", d->id, (pickupgroup) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPEXTEN     is  %s\n", d->id, (d->directed_pickup) ? "enabled" : "disabled");
#endif
	size_t buffersize = 20 + (15 * sizeof(softkeysmap));
	struct ast_str *outputStr = ast_str_create(buffersize);

	for (i = 0; i < v_count; i++) {
		b = v->ptr;
		uint8_t c, j, cp = 0;

		ast_str_append(&outputStr, buffersize, "%-15s => |", skinny_keymode2str(v->id));

		for (c = 0, cp = 0; c < v->count; c++, cp++) {
			msg_out->data.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[cp] = 0;
			/* look for the SKINNY_LBL_ number in the softkeysmap */
			if ((b[c] == SKINNY_LBL_PARK) && (!d->park)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_TRANSFER) && (!d->transfer)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_DND) && (!d->dndFeature.enabled)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_CFWDALL) && (!d->cfwdall)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_CFWDBUSY) && (!d->cfwdbusy)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_CFWDNOANSWER) && (!d->cfwdnoanswer)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_TRNSFVM) && (!trnsfvm)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_IDIVERT) && (!trnsfvm)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_MEETME) && (!meetme)) {
				continue;
			}
#ifndef CS_ADV_FEATURES
			if ((b[c] == SKINNY_LBL_BARGE)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_CBARGE)) {
				continue;
			}
#endif
#ifndef CS_SCCP_CONFERENCE
			if ((b[c] == SKINNY_LBL_JOIN)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_CONFRN)) {
				continue;
			}
#endif
#ifdef CS_SCCP_PICKUP
			if ((b[c] == SKINNY_LBL_PICKUP) && (!d->directed_pickup)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_GPICKUP) && (!pickupgroup)) {
				continue;
			}
#endif
			if ((b[c] == SKINNY_LBL_PRIVATE) && (!d->privacyFeature.enabled)) {
				continue;
			}
			if (b[c] == SKINNY_LBL_EMPTY) {
				continue;
			}
			for (j = 0; j < sizeof(softkeysmap); j++) {
				if (b[c] == softkeysmap[j]) {
					ast_str_append(&outputStr, buffersize, "%-2d:%-9s|", c, label2str(softkeysmap[j]));
					msg_out->data.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[cp] = (j + 1);
					msg_out->data.SoftKeySetResMessage.definition[v->id].les_softKeyInfoIndex[cp] = htoles(j + 301);
					break;
				}
			}

		}

		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: %s\n", d->id, ast_str_buffer(outputStr));
		ast_str_reset(outputStr);
		v++;
		iKeySetCount++;
	};
	sccp_free(outputStr);

	/* disable videomode and join softkey for all softkeysets */
	for (i = 0; i < KEYMODE_ONHOOKSTEALABLE; i++) {
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_JOIN, FALSE);
	}

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "There are %d SoftKeySets.\n", iKeySetCount);

	msg_out->data.SoftKeySetResMessage.lel_softKeySetCount = htolel(iKeySetCount);
	msg_out->data.SoftKeySetResMessage.lel_totalSoftKeySetCount = htolel(iKeySetCount);			// <<-- for now, but should be: iTotalKeySetCount;

	sccp_dev_send(d, msg_out);
	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK);
}

/*!
 * \brief Handle Dialed PhoneBook Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_dialedphonebook_message(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	/* this is from CCM7 dump */
	sccp_msg_t *msg_out = NULL;

	// sccp_BFLState_t state;

	uint32_t transactionID = letohl(msg_in->data.SubscriptionStatReqMessage.lel_transactionID);										
	uint32_t featureID = letohl(msg_in->data.SubscriptionStatReqMessage.lel_featureID);		/* LineInstance / BLF: 0x01 */
	uint32_t timer = letohl(msg_in->data.SubscriptionStatReqMessage.lel_timer);			/* all 32 bits used */
	char *subscriptionID = msg_in->data.SubscriptionStatReqMessage.subscriptionID;

	/* take transactionID apart */
	uint32_t index = transactionID >> 4;								/* just 28 bits filled */
	uint32_t unknown1 = (transactionID | 0xFFFFFFF0) ^ 0xFFFFFFF0;					/* just 4 bits filled */

	// Sending 0x152 Ack Message.
	REQ(msg_out, SubscriptionStatMessage);
	msg_out->data.SubscriptionStatMessage.lel_transactionID = htolel(transactionID);
	msg_out->data.SubscriptionStatMessage.lel_featureID = htolel(featureID);
	msg_out->data.SubscriptionStatMessage.lel_timer = htolel(timer);
	/* we could actually run a match against the pbx dialplan if we had a context (default line ?), and return OK / RouteFail depending on the outcome */
	msg_out->data.SubscriptionStatMessage.lel_cause = 0;						/*!< Cause (Enum): 
														OK: 0x00, 
														RouteFail:0x01, 
														AuthFail:0x02, 
														Timeout:0x03, 
														TrunkTerm:0x04, 
														TrunkForbidden:0x05, 
														Throttle:0x06
													*/
	sccp_dev_send(d, msg_out);

	/* sometimes a phone sends an ' ' entry, I think we can ignore this one */
	if (strlen(msg_in->data.SubscriptionStatReqMessage.subscriptionID) <= 1) {
		return;
	}

	AUTO_RELEASE sccp_line_t *line = sccp_line_find_byid(d, featureID);				

	if (line) {
		REQ(msg_out, NotificationMessage);
		uint32_t status = PBX(getExtensionState) (subscriptionID, line->context);

		msg_out->data.NotificationMessage.lel_transactionID = htolel(transactionID);
		msg_out->data.NotificationMessage.lel_featureID = htolel(featureID);				/* lineInstance */
		msg_out->data.NotificationMessage.lel_status = htolel(status);
		sccp_dev_send(d, msg_out);
		sccp_log((DEBUGCAT_HINT + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: send NotificationMessage for extension '%s', context '%s', state %d\n", DEV_ID_LOG(d), subscriptionID, line->context, status);
		sccp_log((DEBUGCAT_HINT + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Device sent Dialed PhoneBook Rec.'%u' (%u) dn '%s' (timer:0x%08X) line instance '%d'.\n", DEV_ID_LOG(d), index, unknown1, subscriptionID, timer, featureID);
	}
}

/*!
 * \brief Handle Time/Date Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_time_date_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	time_t timer = 0;
	struct tm *cmtime = NULL;

	// char servername[StationMaxDisplayNotifySize];
	sccp_msg_t *msg_out;

	if (!s) {
		return;
	}
	REQ(msg_out, DefineTimeDate);

	/* modulate the timezone by full hours only */
	timer = time(0) + (d->tz_offset * 3600);
	cmtime = localtime(&timer);
	msg_out->data.DefineTimeDate.lel_year = htolel(cmtime->tm_year + 1900);
	msg_out->data.DefineTimeDate.lel_month = htolel(cmtime->tm_mon + 1);
	msg_out->data.DefineTimeDate.lel_dayOfWeek = htolel(cmtime->tm_wday);
	msg_out->data.DefineTimeDate.lel_day = htolel(cmtime->tm_mday);
	msg_out->data.DefineTimeDate.lel_hour = htolel(cmtime->tm_hour);
	msg_out->data.DefineTimeDate.lel_minute = htolel(cmtime->tm_min);
	msg_out->data.DefineTimeDate.lel_seconds = htolel(cmtime->tm_sec);
	msg_out->data.DefineTimeDate.lel_milliseconds = htolel(0);
	msg_out->data.DefineTimeDate.lel_systemTime = htolel(timer);
	sccp_dev_send(d, msg_out);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send date/time\n", DEV_ID_LOG(d));

}

/*!
 * \brief Handle KeyPad Button for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_keypad_button(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	int digit;
	uint8_t lineInstance;
	uint32_t callid;
	char resp = '\0';
	int len = 0;

	digit 		= letohl(msg_in->data.KeypadButtonMessage.lel_kpButton);
	callid 		= letohl(msg_in->data.KeypadButtonMessage.lel_callReference);
	lineInstance 	= letohl(msg_in->data.KeypadButtonMessage.lel_lineInstance);
	
	//pbx_log(LOG_NOTICE, "%s: lineInstance %d\n", DEV_ID_LOG(s->device), lineInstance);
	//pbx_log(LOG_NOTICE, "%s: callid %d\n", DEV_ID_LOG(s->device), callid);

	if (!d) {												// should never be possible, d should have been retained in calling function
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but device is not specified! Exiting\n", DEV_ID_LOG(s->device));
		return;
	}

	/* Old phones like 7912 never uses callid
	 * so we would have trouble finding the right channel
	 */
	AUTO_RELEASE sccp_channel_t *channel = NULL;
	AUTO_RELEASE sccp_line_t *l = NULL;
	AUTO_RELEASE sccp_linedevices_t *linedevice;
	
	
	if ((channel = sccp_device_getActiveChannel(d)) && channel->callid == callid) {
		l = sccp_line_retain(channel->line);
		linedevice = sccp_linedevice_find(d, l);
		
		/* 
		 * older devices like 7960 are sending button index instead of lineInstance 
		 * so we can not trust lineInstance in this case
		 * 
		 */
		if(linedevice->lineInstance != lineInstance){
		    pbx_log(LOG_NOTICE, "%s: linedevice->lineInstance != lineInstance (%d != %d)\n", DEV_ID_LOG(s->device), linedevice->lineInstance, lineInstance);
		}
	}
	

	if (!channel && lineInstance) {
		if (callid) {
			if ((channel = sccp_find_channel_by_lineInstance_and_callid(d, lineInstance, callid)) && channel->line) {
				l = sccp_line_retain(channel->line);
			}
		} else {
			if ((l = sccp_line_find_byid(d, lineInstance))) {
				if (d->active_channel && d->active_channel->line == l) {
					channel = sccp_device_getActiveChannel(d);
				} else {
					sccp_channel_t *c = NULL;

					SCCP_LIST_LOCK(&l->channels);
					channel = SCCP_LIST_FIND(&l->channels, c, list, (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_GETDIGITS || c->state == SCCP_CHANNELSTATE_DIGITSFOLL), TRUE);
					SCCP_LIST_UNLOCK(&l->channels);
				}
			}
		}
	} else if(!l) {
		if (callid) {
			if ((channel = sccp_channel_find_byid(callid)) && channel->line) {
				l = sccp_line_retain(channel->line);
			}
		} else {
			if ((channel = sccp_device_getActiveChannel(d)) && channel->line) {
				l = sccp_line_retain(channel->line);
			}
		}
	}

	if (!channel) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no (active) channel! Exiting\n", DEV_ID_LOG(d));
		return;
	}

	if (!channel->owner) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no (active) pbx channel! Exiting\n", DEV_ID_LOG(d));
		sccp_channel_endcall(channel);
		return;
	}

	if (!l) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no line specified! Exiting\n", DEV_ID_LOG(d));
		return;
	}

	if (channel->scheduler.hangup) {
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "%s: Channel to be hungup shortly, giving up on sending more digits %d\n", DEV_ID_LOG(d), digit);
		return;
	}

	if (channel->state == SCCP_CHANNELSTATE_INVALIDNUMBER || channel->state == SCCP_CHANNELSTATE_CONGESTION || channel->state == SCCP_CHANNELSTATE_BUSY || channel->state == SCCP_CHANNELSTATE_ZOMBIE || channel->state == SCCP_CHANNELSTATE_DND) {
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "%s: Channel already ended, giving up on sending more digits %d\n", DEV_ID_LOG(d), digit);
		return;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: SCCP Digit: %08x (%d) on line %s, channel %d with state: %d (Using: %s)\n", DEV_ID_LOG(d), digit, digit, l->name, channel->callid, channel->state, sccp_dtmfmode2str(channel->dtmfmode));

	if (digit == 14) {
		resp = '*';
	} else if (digit == 15) {
		resp = '#';
	} else if (digit == 16) {
		resp = '+';
	} else if (digit >= 0 && digit <= 9) {
		resp = '0' + digit;
	} else {
		// resp = '0' + digit;
		pbx_log(LOG_WARNING, "Unsupported digit %d\n", digit);
	}

	/* added PROGRESS to make sending digits possible during progress state (Pavel Troller) */
	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE || channel->state == SCCP_CHANNELSTATE_PROCEED || channel->state == SCCP_CHANNELSTATE_RINGOUT) {
		/* we have to unlock 'cause the senddigit lock the channel */
		if (channel->dtmfmode == SCCP_DTMFMODE_SKINNY && PBX(send_digit)) {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "%s: Sending Emulated DTMF Digit %c to %s (using pbx frame)\n", DEV_ID_LOG(d), resp, l->name);
			PBX(send_digit) (channel, resp);
		} else {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "%s: Phone has sent DTMF Digit %c to %s (RFC2833)\n", DEV_ID_LOG(d), resp, l->name);
		}
		return;
	}

	len = strlen(channel->dialedNumber);
	if (len + 1 >= (SCCP_MAX_EXTENSION)) {
		/*! \todo Shouldn't we only skip displaying the number to the phone (Maybe even showing '...' at the end), but still dial it ? */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Maximum Length of Extension reached. Skipping Digit\n", channel->designator);
		sccp_dev_displayprompt(d, lineInstance, channel->callid, SKINNY_DISP_NO_MORE_DIGITS, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (((channel->state == SCCP_CHANNELSTATE_OFFHOOK) || (channel->state == SCCP_CHANNELSTATE_GETDIGITS) || (channel->state == SCCP_CHANNELSTATE_DIGITSFOLL)) && !PBX(getChannelPbx) (channel)) {
		/* enbloc emulation */
		double max_deviation = SCCP_SIM_ENBLOC_DEVIATION;
		int max_time_per_digit = SCCP_SIM_ENBLOC_MAX_PER_DIGIT;
		double variance = 0;
		double std_deviation = 0;
		int minimum_digit_before_check = SCCP_SIM_ENBLOC_MIN_DIGIT;
		int lpbx_digit_usecs = 0;
		int number_of_digits = len;
		int timeout_if_enbloc = SCCP_SIM_ENBLOC_TIMEOUT;						// new timeout if we have established we should enbloc dialing

		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC_EMU digittimeout '%d' ms, sched_wait '%d' ms\n", channel->enbloc.digittimeout, PBX(sched_wait) (channel->scheduler.digittimeout));
		if (GLOB(simulate_enbloc) && !channel->enbloc.deactivate && number_of_digits >= 1) {		// skip the first digit (first digit had longer delay than the rest)
			if ((channel->enbloc.digittimeout) < (PBX(sched_wait) (channel->scheduler.digittimeout))) {
				lpbx_digit_usecs = (channel->enbloc.digittimeout) - (PBX(sched_wait) (channel->scheduler.digittimeout));
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU Cancelled (past digittimeout)\n");
				channel->enbloc.deactivate = 1;
			}
			channel->enbloc.totaldigittime += lpbx_digit_usecs;
			channel->enbloc.totaldigittimesquared += pow(lpbx_digit_usecs, 2);
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC_EMU digit entry time '%d' ms, total dial time '%d' ms, number of digits: %d\n", lpbx_digit_usecs, channel->enbloc.totaldigittime, number_of_digits);
			if (number_of_digits >= 2) {								// prevent div/0
				if (number_of_digits >= minimum_digit_before_check) {				// minimal number of digits before checking
					if (lpbx_digit_usecs < max_time_per_digit) {
						variance = ((double) channel->enbloc.totaldigittimesquared - (pow((double) channel->enbloc.totaldigittime, 2) / (double) number_of_digits)) / ((double) number_of_digits - 1);
						std_deviation = sqrt(variance);
						sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU sqrt((%d-((pow(%d, 2))/%d))/%d)='%2.2f'\n", channel->enbloc.totaldigittimesquared, channel->enbloc.totaldigittime, number_of_digits, number_of_digits - 1, std_deviation);
						sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU totaldigittimesquared '%d', totaldigittime '%d', number_of_digits '%d', std_deviation '%2.2f', variance '%2.2f'\n", channel->enbloc.totaldigittimesquared, channel->enbloc.totaldigittime, number_of_digits, std_deviation, variance);
						if (std_deviation < max_deviation) {
							if (channel->enbloc.digittimeout > timeout_if_enbloc) {	// only display message and change timeout once
								sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU FAST DIAL (new timeout=2 sec)\n");
								channel->enbloc.digittimeout = timeout_if_enbloc;	// set new digittimeout
							}
						} else {
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU Cancelled (deviation from mean '%2.2f' > maximum '%2.2f')\n", std_deviation, max_deviation);
							channel->enbloc.deactivate = 1;
						}
					} else {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU Cancelled (time per digit '%d' > maximum '%d')\n", lpbx_digit_usecs, max_time_per_digit);
						channel->enbloc.deactivate = 1;
					}
				}
			}
		}

		/* add digit to dialed number */
		channel->dialedNumber[len++] = resp;
		channel->dialedNumber[len] = '\0';
		sccp_channel_schedule_digittimout(channel, channel->enbloc.digittimeout);

		if (GLOB(digittimeoutchar) == resp) {								// we dial on digit timeout char !
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Got digit timeout char '%c', dial immediately\n", GLOB(digittimeoutchar));
			channel->dialedNumber[len] = '\0';
			sccp_channel_stop_schedule_digittimout(channel);
			sccp_safe_sleep(100);									// we would hear last keypad stroke before starting all
			sccp_pbx_softswitch(channel);
		}
		if (sccp_pbx_helper(channel) == SCCP_EXTENSION_EXACTMATCH) {					// we dial when helper says we have a match
			sccp_channel_stop_schedule_digittimout(channel);
			sccp_safe_sleep(100);									// we would hear last keypad stroke before starting all
			sccp_pbx_softswitch(channel);								// channel will be released by hangup
		}
		sccp_handle_dialtone(channel);

	} else if (PBX(getChannelPbx) (channel) || channel->state == SCCP_CHANNELSTATE_DIALING) {		/* Overlap Dialing (\todo should we check &GLOB(allowoverlap) here ? */
		/* add digit to dialed number */
		channel->dialedNumber[len++] = resp;
		channel->dialedNumber[len] = '\0';

		if (d->earlyrtp == SCCP_EARLYRTP_IMMEDIATE) {
			sccp_channel_set_calledparty(channel, NULL, channel->dialedNumber);
			if (len==1) sccp_dev_set_keyset(d, lineInstance, channel->callid, KEYMODE_DIGITSFOLL);
		}
		if (channel->dtmfmode == SCCP_DTMFMODE_SKINNY && PBX(send_digit)) {
			sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "%s: Force Sending Emulated DTMF Digit %c to %s (using pbx frame)\n", DEV_ID_LOG(d), resp, l->name);
			PBX(send_digit) (channel, resp);
		}
	} else {
		pbx_log(LOG_WARNING, "%s: keypad_button could not be handled correctly because of invalid state on line %s, channel: %d, state: %d\n", DEV_ID_LOG(d), l->name, channel->callid, channel->state);
	}
}

/*!
 * \brief Handle DialTone Without Lock
 * \param channel SCCP Channel as sccp_channel_t
 */
void sccp_handle_dialtone(sccp_channel_t * channel)
{
	uint8_t instance;

	if (!channel) {
		return;
	}

	if (channel->softswitch_action != SCCP_SOFTSWITCH_DIAL || channel->scheduler.hangup) {
		return;
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_retain(channel->line);

	if (!l) {
		return;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(channel);

	if (!d) {
		return;
	}

	instance = sccp_device_find_index_for_line(d, l->name);

	/* we check dialtone just in DIALING action
	 * otherwise, you'll get secondary dialtone also
	 * when catching call forward number, meetme room,
	 * etc.
	 * */
	if (strlen(channel->dialedNumber) == 0 && channel->state != SCCP_CHANNELSTATE_OFFHOOK) {
		sccp_dev_stoptone(d, instance, channel->callid);
		sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, channel->callid, 0);
	} else if (strlen(channel->dialedNumber) > 0) {
		sccp_indicate(d, channel, SCCP_CHANNELSTATE_DIGITSFOLL);
	}
}

/*!
 * \brief Handle Soft Key Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_log((DEBUGCAT_MESSAGE + DEBUGCAT_ACTION + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Got Softkey\n", DEV_ID_LOG(d));

	uint32_t event = letohl(msg_in->data.SoftKeyEventMessage.lel_softKeyEvent);
	uint32_t lineInstance = letohl(msg_in->data.SoftKeyEventMessage.lel_lineInstance);
	uint32_t callid = letohl(msg_in->data.SoftKeyEventMessage.lel_callReference);

	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: Received Softkey Event but no device to connect it to. Exiting\n");
		return;
	}

	event = softkeysmap[event - 1];

	/* correct events for nokia icc client (Legacy Support -FS) */
	if (!strcasecmp(d->config_type, "nokia-icc")) {
		switch (event) {
			case SKINNY_LBL_DIRTRFR:
				event = SKINNY_LBL_ENDCALL;
				break;
		}
	}

	sccp_log((DEBUGCAT_MESSAGE + DEBUGCAT_ACTION + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Got Softkey: %s (%d) line=%d callid=%d\n", d->id, label2str(event), event, lineInstance, callid);

	/* we have no line and call information -> use default line */
	AUTO_RELEASE sccp_line_t *l = NULL;

	if (!lineInstance && !callid && (event == SKINNY_LBL_NEWCALL || event == SKINNY_LBL_REDIAL)) {
		if (d->defaultLineInstance > 0) {
			lineInstance = d->defaultLineInstance;
		} else {
			l = sccp_dev_get_activeline(d);
		}
	}

	if (lineInstance) {
		l = sccp_line_find_byid(d, lineInstance);
	}

	AUTO_RELEASE sccp_channel_t *c = NULL;

	if (l && callid) {
		c = sccp_find_channel_by_lineInstance_and_callid(d, lineInstance, callid);
	}

	if (!sccp_SoftkeyMap_execCallbackByEvent(d, l, lineInstance, c, event)) {
		char buf[100];

		/* skipping message if event is endcall, because they can coincide when both parties hangup around the same time */
		if (event != SKINNY_LBL_ENDCALL) {
			snprintf(buf, 100, SKINNY_DISP_NO_CHANNEL_TO_PERFORM_XXXXXXX_ON " " SKINNY_GIVING_UP, label2str(event));
			sccp_dev_displayprinotify(d, buf, 5, 5);
			sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, lineInstance, 0, 0);
			pbx_log(LOG_WARNING, "%s: Skip handling of Softkey %s (%d) line=%d callid=%d, because a channel is required, but not provided. Exiting\n", d->id, label2str(event), event, lineInstance, callid);
		}

		/* disable callplane for this device */
		if (d && d->indicate && d->indicate->onhook) {
			d->indicate->onhook(d, lineInstance, callid);
		}
	}
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	skinny_mediastatus_t mediastatus = SKINNY_MEDIASTATUS_Unknown;
	uint32_t callReference = 0, passThruPartyId = 0;

	struct sockaddr_storage sas = { 0 };
	d->protocol->parseOpenReceiveChannelAck((const sccp_msg_t *) msg_in, &mediastatus, &sas, &passThruPartyId, &callReference);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Got OpenChannel ACK.  Status: '%s' (%d), Remote RTP/UDP '%s', Type: %s, PassThruPartyId: %u, CallID: %u\n", d->id, skinny_mediastatus2str(mediastatus), mediastatus, sccp_socket_stringify(&sas), (d->directrtp ? "DirectRTP" : "Indirect RTP"), passThruPartyId, callReference);

	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO6911 && 0 == passThruPartyId) {
		passThruPartyId = 0xFFFFFFFF - callReference;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Dealing with 6911 which does not return a passThruPartyId, using callid: %u -> passThruPartyId %u\n", d->id, callReference, passThruPartyId);
	}

	AUTO_RELEASE sccp_channel_t *channel = NULL;

	if ((d->active_channel && d->active_channel->passthrupartyid == passThruPartyId) || !passThruPartyId) {	// reduce the amount of searching by first checking active_channel
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passThruPartyId);
	}
	if (mediastatus) {
		pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device returned: '%s' (%d) !. Giving up.\n", d->id, skinny_mediastatus2str(mediastatus), mediastatus);
		if (channel) {
			sccp_channel_endcall(channel);
		}
		return;
	}
	if (channel && channel->state != SCCP_CHANNELSTATE_ONHOOK) {
		if (channel->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			pbx_log(LOG_WARNING, "%s: (OpenReceiveChannelAck) Invalid Number (%d)\n", DEV_ID_LOG(d), channel->state);
			return;
		}
		if (channel->state == SCCP_CHANNELSTATE_DOWN) {
			pbx_log(LOG_WARNING, "%s: (OpenReceiveChannelAck) Channel is down. Giving up... (%d)\n", DEV_ID_LOG(d), channel->state);
			sccp_msg_t *r;

			REQ(r, CloseReceiveChannel);
			msg_in->data.CloseReceiveChannel.lel_conferenceId = htolel(callReference);
			msg_in->data.CloseReceiveChannel.lel_passThruPartyId = htolel(passThruPartyId);
			msg_in->data.CloseReceiveChannel.lel_callReference = htolel(callReference);
			sccp_dev_send(d, r);
			return;
		}

		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Starting Phone RTP/UDP Transmission (State: %s[%d])\n", d->id, sccp_channelstate2str(channel->state), channel->state);
		sccp_channel_setDevice(channel, d);
		if (channel->rtp.audio.rtp) {
			if (d->nat >= SCCP_NAT_ON) {
				/* Rewrite ip-addres to the outside source address via which the phone connection (device->sin) */
				uint16_t port = sccp_socket_getPort(&sas);

				memcpy(&sas, &d->session->sin, sizeof(struct sockaddr_storage));
				sccp_socket_ipv4_mapped(&sas, &sas);
				sccp_socket_setPort(&sas, port);

			}
			sccp_rtp_set_phone(channel, &channel->rtp.audio, &sas);
			sccp_channel_updateMediaTransmission(channel);
			//sccp_channel_startMediaTransmission(channel);

			/* update status */
			channel->rtp.audio.writeState = SCCP_RTP_STATUS_ACTIVE;
			/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
			if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				PBX(queue_control) (channel->owner, AST_CONTROL_ANSWER);
			}
			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE) && ((channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE))) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
		} else {
			pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Can't set the RTP media address to %s, no asterisk rtp channel!\n", d->id, sccp_socket_stringify(&sas));
			sccp_channel_endcall(channel);								// FS - 350
		}
	} else {
		/* we successfully opened receive channel, but have no channel active -> close receive */
		int32_t callId = passThruPartyId ^ 0xFFFFFFFF;

		pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) No channel with this PassThruPartyId %u (callReference: %d, callid: %d)!\n", d->id, passThruPartyId, callReference, callId);
		if (channel) {
			sccp_channel_closeReceiveChannel(channel, FALSE);
		}
	}
}

/*!
 * \brief Handle Open Multi Media Receive Acknowledgement
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_OpenMultiMediaReceiveAck(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	char addrStr[INET6_ADDRSTRLEN + 6];
	struct sockaddr_storage sas = { 0 };
	skinny_mediastatus_t mediastatus = SKINNY_MEDIASTATUS_Unknown;
	uint32_t partyID = 0, passThruPartyId = 0, callReference;

	d->protocol->parseOpenMultiMediaReceiveChannelAck((const sccp_msg_t *) msg_in, &mediastatus, &sas, &passThruPartyId, &callReference);
	sccp_copy_string(addrStr, sccp_socket_stringify(&sas), sizeof(addrStr));

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Got OpenMultiMediaReceiveChannelAck.  Status: '%s' (%d), Remote RTP/UDP '%s', Type: %s, PassThruId: %u, CallID: %u\n", d->id, skinny_mediastatus2str(mediastatus), mediastatus, addrStr, (d->directrtp ? "DirectRTP" : "Indirect RTP"), partyID, callReference);
	if (mediastatus) {
		/* rtp error from the phone */
		pbx_log(LOG_WARNING, "%s: Error while opening MediaTransmission, '%s' (%d).\n", DEV_ID_LOG(d), skinny_mediastatus2str(mediastatus), mediastatus);
		if (mediastatus == SKINNY_MEDIASTATUS_OutOfChannels || mediastatus == SKINNY_MEDIASTATUS_OutOfSockets) {
			pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Please Reset this Device. It ran out of Channels and/or Sockets\n", d->id);
		}
		sccp_dump_msg(msg_in);
		return;
	}

	AUTO_RELEASE sccp_channel_t *channel;

	if ((d->active_channel && d->active_channel->passthrupartyid == passThruPartyId) || !passThruPartyId) {	// reduce the amount of searching by first checking active_channel
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passThruPartyId);
	}
	if (channel) {												// && sccp_channel->state != SCCP_CHANNELSTATE_DOWN) {
		if (channel->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			return;
		}

		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Starting device rtp transmission with state %s(%d)\n", d->id, sccp_channelstate2str(channel->state), channel->state);
		if (channel->rtp.video.rtp || sccp_rtp_createVideoServer(channel)) {
			if (d->nat >= SCCP_NAT_ON) {
				uint16_t port = sccp_socket_getPort(&sas);

				memcpy(&sas, &d->session->sin, sizeof(struct sockaddr_storage));
				sccp_socket_ipv4_mapped(&sas, &sas);
				sccp_socket_setPort(&sas, port);
			}

			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s\n", d->id, sccp_socket_stringify(&sas));

			sccp_rtp_set_phone(channel, &channel->rtp.video, &sas);
			channel->rtp.video.writeState = SCCP_RTP_STATUS_ACTIVE;

			if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				PBX(queue_control) (channel->owner, AST_CONTROL_ANSWER);
			}
			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE) && ((channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE))) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
		} else {
			pbx_log(LOG_ERROR, "%s: Can't set the RTP media address to %s, no asterisk rtp channel!\n", d->id, addrStr);
		}

		sccp_msg_t *msg_out;

		msg_out = sccp_build_packet(MiscellaneousCommandMessage, sizeof(msg_in->data.MiscellaneousCommandMessage));
		msg_out->data.MiscellaneousCommandMessage.lel_conferenceId = htolel(channel->callid);
		msg_out->data.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg_out->data.MiscellaneousCommandMessage.lel_callReference = htolel(channel->callid);
		msg_out->data.MiscellaneousCommandMessage.lel_miscCommandType = htolel(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);	/* videoFastUpdatePicture */
		sccp_dev_send(d, msg_out);

		// msg_out = sccp_build_packet(FlowControlNotifyMessage, sizeof(msg_in->data.FlowControlNotifyMessage));
		// msg_out->data.FlowControlNotifyMessage.lel_conferenceID         = htolel(channel->callid);
		// msg_out->data.FlowControlNotifyMessage.lel_passThruPartyId      = htolel(channel->passthrupartyid);
		// msg_out->data.FlowControlNotifyMessage.lel_callReference        = htolel(channel->callid);
		// msg_out->data.FlowControlNotifyMessage.lel_maxBitRate           = htolel(500000);
		// sccp_dev_send(d, msg_out);

		PBX(queue_control) (channel->owner, AST_CONTROL_VIDUPDATE);
	} else {
		pbx_log(LOG_ERROR, "%s: No channel with this PassThruId %u!\n", d->id, partyID);
	}
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \since 20090708
 * \author Federico
 */
void sccp_handle_startmediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	struct sockaddr_storage sas = { 0 };
	skinny_mediastatus_t mediastatus = SKINNY_MEDIASTATUS_Unknown;
	uint32_t partyID = 0, callID = 0, callID1 = 0, passthrupartyid = 0;

	d->protocol->parseStartMediaTransmissionAck((const sccp_msg_t *) msg_in, &partyID, &callID, &callID1, &mediastatus, &sas);

	if (partyID) {
		passthrupartyid = partyID;
	}

	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO6911 && 0 == passthrupartyid) {
		passthrupartyid = 0xFFFFFFFF - callID1;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Dealing with 6911 which does not return a passthrupartyid, using callid: %u -> passthrupartyid %u\n", d->id, callID1, passthrupartyid);
	}

	AUTO_RELEASE sccp_channel_t *channel = NULL;

	if ((d->active_channel && d->active_channel->passthrupartyid == passthrupartyid) || !passthrupartyid) {
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passthrupartyid);
	}

	if (!channel) {
		pbx_log(LOG_WARNING, "%s: Channel with passthrupartyid %u / callid %u / callid1 %u not found, please report this to developer\n", DEV_ID_LOG(d), partyID, callID, callID1);
		return;
	}
	if (mediastatus) {
		pbx_log(LOG_WARNING, "%s: Error while opening MediaTransmission. Ending call. '%s' (%d))\n", DEV_ID_LOG(d), skinny_mediastatus2str(mediastatus), mediastatus);
		if (mediastatus == SKINNY_MEDIASTATUS_OutOfChannels || mediastatus == SKINNY_MEDIASTATUS_OutOfSockets) {
			pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Please Reset this Device. It ran out of Channels and/or Sockets\n", d->id);
		}
		sccp_dump_msg(msg_in);
		sccp_channel_closeAllMediaTransmitAndReceive(d, channel);
		sccp_channel_endcall(channel);
	} else {
		if (channel->state != SCCP_CHANNELSTATE_DOWN) {
			/* update status */
			channel->rtp.audio.readState = SCCP_RTP_STATUS_ACTIVE;

			/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
			if (channel->calltype == SKINNY_CALLTYPE_INBOUND) {
				PBX(queue_control) (channel->owner, AST_CONTROL_ANSWER);
			}
			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE) && ((channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE))) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got StartMediaTranmission ACK.  Status: '%s' (%d), Remote TCP/IP: '%s', CallId %u (%u), PassThruId: %u\n", DEV_ID_LOG(d), skinny_mediastatus2str(mediastatus), mediastatus, sccp_socket_stringify(&sas), callID, callID1, partyID);
		} else {
			pbx_log(LOG_WARNING, "%s: (sccp_handle_startmediatransmission_ack) Channel already down (%d). Hanging up\n", DEV_ID_LOG(d), channel->state);
			sccp_channel_closeAllMediaTransmitAndReceive(d, channel);
			sccp_channel_endcall(channel);
		}
	}
}

/*!
 * \brief Handle Start Multi Media Transmission Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param msg_in SCCP Message
 */
void sccp_handle_startmultimediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	struct sockaddr_storage ss = { 0 };

	skinny_mediastatus_t mediastatus = SKINNY_MEDIASTATUS_Unknown;
	uint32_t partyID = 0, callID = 0, callID1 = 0;

	d->protocol->parseStartMultiMediaTransmissionAck((const sccp_msg_t *) msg_in, &partyID, &callID, &callID1, &mediastatus, &ss);
	if (ss.ss_family == AF_INET6) {
		pbx_log(LOG_ERROR, "SCCP: IPv6 not supported at this moment\n");
		return;
	}

	AUTO_RELEASE sccp_channel_t *c = sccp_channel_find_bypassthrupartyid(partyID);
	if (mediastatus) {
		pbx_log(LOG_ERROR, "%s: (StartMultiMediaTransmissionAck) Device returned: '%s' (%d) !. Ending Call.\n", DEV_ID_LOG(d), skinny_mediastatus2str(mediastatus), mediastatus);
		if (c) {
			sccp_channel_endcall(c);
		}
		sccp_dump_msg(msg_in);
		c->rtp.video.readState = SCCP_RTP_STATUS_INACTIVE;
		return;
	}

	if (c) {
		/* update status */
		c->rtp.video.readState = SCCP_RTP_STATUS_ACTIVE;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got StartMultiMediaTranmission ACK. Remote TCP/IP '%s', CallId %u (%u), PassThruId: %u\n", DEV_ID_LOG(d), sccp_socket_stringify(&ss), callID, callID1, partyID);
		return;
	}
	pbx_log(LOG_WARNING, "%s: Channel with passthrupartyid %u could not be found, please report this to developer\n", DEV_ID_LOG(d), partyID);
	return;
}

/*!
 * \brief Handle Start Multi Media Transmission Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param msg_in SCCP Message
 */
void sccp_handle_mediatransmissionfailure(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_dump_msg(msg_in);
	/*
	
	struct sockaddr_storage ss = { 0 };
	uint32_t confID = 0, partyID = 0, callRef = 0;
	d->protocol->parseMediaTransmissionFailure((const sccp_msg_t *) msg_in, &confID, &partyID, &ss, &callRef);
	
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_find_bypassthrupartyid(partyID);

	if (c) {
		pbx_log(LOG_ERROR, "%s: MediaFailure on Channel '%s'!. Ending Call.\n", d->id, c->designator);
		// if directrtp: switch back to indirect rtp
		// else
		// 	hangup
			sccp_channel_endcall(c);
	}
	*/

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Received a MediaTranmissionFailure (not being handled fully at this moment)\n", DEV_ID_LOG(d));
}

/*!
 * \brief Handle Version for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_version(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out;

	REQ(msg_out, VersionMessage);
	sccp_copy_string(msg_out->data.VersionMessage.requiredVersion, d->imageversion, sizeof(msg_out->data.VersionMessage.requiredVersion));
	sccp_dev_send(d, msg_out);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending version number: %s\n", d->id, d->imageversion);
}

/*!
 * \brief Handle Connection Statistics for Session
 * \param s SCCP Session
 * \param device SCCP Device
 * \param msg_in SCCP Message
 *
 * \section sccp_statistics Phone Audio Statistics
 *
 * MOS LQK = Mean Opinion Score for listening Quality (5=Excellent -> 1=BAD)
 * Max MOS LQK = Baseline or highest MOS LQK score observed from start of the voice stream. These codecs provide the following maximum MOS LQK score under normal conditions with no frame loss: (G.711 gives 4.5, G.729 A /AB gives 3.7)
 * MOS LQK Version = Version of the Cisco proprietary algorithm used to calculate MOS LQK scores.
 * CS = Concealement seconds
 * Cumulative Conceal Ratio = Total number of concealment frames divided by total number of speech frames received from start of the voice stream.
 * Interval Conceal Ratio = Ratio of concealment frames to speech frames in preceding 3-second interval of active speech. If using voice activity detection (VAD), a longer interval might be required to accumulate 3 seconds of active speech.
 * Max Conceal Ratio = Highest interval concealment ratio from start of the voice stream.
 * Conceal Secs = Number of seconds that have concealment events (lost frames) from the start of the voice stream (includes severely concealed seconds).
 * Severely Conceal Secs = Number of seconds that have more than 5 percent concealment events (lost frames) from the start of the voice stream.
 * Latency1 = Estimate of the network latency, expressed in milliseconds. Represents a running average of the round-trip delay, measured when RTCP receiver report blocks are received.
 * Max Jitter = Maximum value of instantaneous jitter, in milliseconds.
 * Sender Size = RTP packet size, in milliseconds, for the transmitted stream.
 * Rcvr Size = RTP packet size, in milliseconds, for the received stream.
 * Rcvr Discarded = RTP packets received from network but discarded from jitter buffers.
 */
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_device_t * device, sccp_msg_t * msg_in)
{
#define CALC_AVG(_newval, _mean, _numval) ( ( (_mean * (_numval) ) + _newval ) / (_numval + 1))

	size_t buffersize = 2048;
	struct ast_str *output_buf = pbx_str_alloca(buffersize);
	char QualityStats[600] = "";
	uint32_t QualityStatsSize = 0;

	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (d) {
		// sccp_dump_msg(msg_in);
		// update last_call_statistics
		sccp_call_statistics_t *call_stats = d->call_statistics;

		if (letohl(msg_in->header.lel_protocolVer < 20)) {
			call_stats[SCCP_CALLSTATISTIC_LAST].num = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_CallIdentifier);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_SentPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_received = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_RecvdPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_LostPkts);
			call_stats[SCCP_CALLSTATISTIC_LAST].jitter = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_Jitter);
			call_stats[SCCP_CALLSTATISTIC_LAST].latency = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_latency);
			QualityStatsSize = letohl(msg_in->data.ConnectionStatisticsRes.v3.lel_QualityStatsSize) + 1;
			QualityStatsSize = QualityStatsSize < sizeof(QualityStats) ? QualityStatsSize : sizeof(QualityStats);
			if (QualityStatsSize) {
				sccp_copy_string(QualityStats, msg_in->data.ConnectionStatisticsRes.v3.QualityStats, QualityStatsSize);
			}
		} else if (letohl(msg_in->header.lel_protocolVer < 22)) {
			call_stats[SCCP_CALLSTATISTIC_LAST].num = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_CallIdentifier);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_SentPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_received = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_RecvdPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_LostPkts);
			call_stats[SCCP_CALLSTATISTIC_LAST].jitter = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_Jitter);
			call_stats[SCCP_CALLSTATISTIC_LAST].latency = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_latency);
			QualityStatsSize = letohl(msg_in->data.ConnectionStatisticsRes.v20.lel_QualityStatsSize) + 1;
			QualityStatsSize = QualityStatsSize < sizeof(QualityStats) ? QualityStatsSize : sizeof(QualityStats);
			if (QualityStatsSize) {
				sccp_copy_string(QualityStats, msg_in->data.ConnectionStatisticsRes.v20.QualityStats, QualityStatsSize);
			}
		} else {											// odd
			// ConnectionStatisticsRes_V22 has irregular packing (single byte packing), need to access unaligned data (using get_unaligned_uint32 for sparc62 / buserror machines
#if defined(HAVE_UNALIGNED_BUSERROR)
			call_stats[SCCP_CALLSTATISTIC_LAST].num = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_CallIdentifier));
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_SentPackets));
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_received = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_RecvdPackets));
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_LostPkts));
			call_stats[SCCP_CALLSTATISTIC_LAST].jitter = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_Jitter));
			call_stats[SCCP_CALLSTATISTIC_LAST].latency = letohl(get_unaligned_uint32((const void *) &msg_in->data.ConnectionStatisticsRes.v22.lel_latency));
			QualityStatsSize = letohl(get_unaligned_uint32(msg_in->data.ConnectionStatisticsRes.v22.lel_QualityStatsSize));
#else
			call_stats[SCCP_CALLSTATISTIC_LAST].num = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_CallIdentifier);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_SentPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_received = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_RecvdPackets);
			call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_LostPkts);
			call_stats[SCCP_CALLSTATISTIC_LAST].jitter = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_Jitter);
			call_stats[SCCP_CALLSTATISTIC_LAST].latency = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_latency);
			QualityStatsSize = letohl(msg_in->data.ConnectionStatisticsRes.v22.lel_QualityStatsSize) + 1;
#endif
			QualityStatsSize = QualityStatsSize < sizeof(QualityStats) ? QualityStatsSize : sizeof(QualityStats);
			if (QualityStatsSize) {
				sccp_copy_string(QualityStats, msg_in->data.ConnectionStatisticsRes.v22.QualityStats, QualityStatsSize);
			}
		}
		pbx_str_append(&output_buf, buffersize, "%s: Call Statistics:\n", d->id);
		pbx_str_append(&output_buf, buffersize, "       [\n");

		pbx_str_append(&output_buf, buffersize, "         Last Call        : CallID: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", call_stats[SCCP_CALLSTATISTIC_LAST].num, call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_LAST].latency);
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "QualityStats: %s\n", QualityStats);
		if (!sccp_strlen_zero(QualityStats)) {
			if (letohl(msg_in->header.lel_protocolVer < 20)) {
				sscanf(QualityStats, "MLQK=%f;MLQKav=%f;MLQKmn=%f;MLQKmx=%f;MLQKvr=%f;CCR=%f;ICR=%f;ICRmx=%f;CS=%d;SCS=%d",
				       &call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, &call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);
			} else if (letohl(msg_in->header.lel_protocolVer < 22)) {
				int Log = 0;

				sscanf(QualityStats, "Log %d: mos %f, avgMos %f, maxMos %f, minMos %f, CS %d, SCS %d, CCR %f, ICR %f, maxCR %f",
				       &Log,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, &call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds, &call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio);
			} else {
				sscanf(QualityStats, "MLQK=%f;MLQKav=%f;MLQKmn=%f;MLQKmx=%f;ICR=%f;CCR=%f;ICRmx=%f;CS=%d;SCS=%d;MLQKvr=%f",
				       &call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, &call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality,
				       &call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio, &call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, &call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds, &call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality);
			}
		}
		pbx_str_append(&output_buf, buffersize, "         Last Quality     : MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d\n",
			       call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
			       call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio,
			       (int) call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);

		// update avg_call_statistics
		call_stats[SCCP_CALLSTATISTIC_AVG].packets_sent = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_AVG].packets_sent, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].packets_received = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_AVG].packets_received, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].packets_lost = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_AVG].packets_lost, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].jitter = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_AVG].jitter, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].latency = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].latency, call_stats[SCCP_CALLSTATISTIC_AVG].latency, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].opinion_score_listening_quality = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].avg_opinion_score_listening_quality = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].avg_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].mean_opinion_score_listening_quality = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		if (call_stats[SCCP_CALLSTATISTIC_AVG].max_opinion_score_listening_quality < call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality) {
			call_stats[SCCP_CALLSTATISTIC_AVG].max_opinion_score_listening_quality = call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality;
		}
		call_stats[SCCP_CALLSTATISTIC_AVG].interval_concealement_ratio = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].cumulative_concealement_ratio = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		if (call_stats[SCCP_CALLSTATISTIC_AVG].max_concealement_ratio < call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio) {
			call_stats[SCCP_CALLSTATISTIC_AVG].max_concealement_ratio = call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio;
		}
		call_stats[SCCP_CALLSTATISTIC_AVG].concealed_seconds = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, call_stats[SCCP_CALLSTATISTIC_AVG].concealed_seconds, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].severely_concealed_seconds = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds, call_stats[SCCP_CALLSTATISTIC_AVG].severely_concealed_seconds, call_stats[SCCP_CALLSTATISTIC_AVG].num);
		call_stats[SCCP_CALLSTATISTIC_AVG].variance_opinion_score_listening_quality = CALC_AVG(call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].num);

		call_stats[SCCP_CALLSTATISTIC_AVG].num++;
		pbx_str_append(&output_buf, buffersize, "         Mean Statistics  : #Calls: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", call_stats[SCCP_CALLSTATISTIC_AVG].num, call_stats[SCCP_CALLSTATISTIC_AVG].packets_sent, call_stats[SCCP_CALLSTATISTIC_AVG].packets_received, call_stats[SCCP_CALLSTATISTIC_AVG].packets_lost, call_stats[SCCP_CALLSTATISTIC_AVG].jitter, call_stats[SCCP_CALLSTATISTIC_AVG].latency);

		pbx_str_append(&output_buf, buffersize, "         Mean Quality     : MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d\n",
			       call_stats[SCCP_CALLSTATISTIC_AVG].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].avg_opinion_score_listening_quality,
			       call_stats[SCCP_CALLSTATISTIC_AVG].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_AVG].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_AVG].max_concealement_ratio,
			       (int) call_stats[SCCP_CALLSTATISTIC_AVG].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_AVG].severely_concealed_seconds);
		pbx_str_append(&output_buf, buffersize, "       ]\n");
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s", pbx_str_buffer(output_buf));
	}
}

/*!
 * \brief Handle Server Resource Message for Session
 * old protocol function replaced by the SEP file server addesses list
 *
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \todo extend ServerResMessage to be able to return multiple servers (cluster)
 * \todo Handle IPv6
 */
void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out;

	if (sccp_socket_is_any_addr(&s->ourip)) {
		pbx_log(LOG_ERROR, "%s: Session IP Unspecified\n", DEV_ID_LOG(d));
		return;
	}

	if (s->device && s->device->session != s) {
		pbx_log(LOG_ERROR, "%s: Wrong Session or Session Changed mid flight (%s)\n", DEV_ID_LOG(d), sccp_socket_stringify(&s->ourip));
		return;
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Sending servers message (%s)\n", DEV_ID_LOG(d), sccp_socket_stringify(&s->ourip));

	REQ(msg_out, ServerResMessage);
	sccp_copy_string(msg_out->data.ServerResMessage.server[0].serverName, sccp_socket_stringify_addr(&s->ourip), sizeof(msg_out->data.ServerResMessage.server[0].serverName));
	msg_out->data.ServerResMessage.serverListenPort[0] = sccp_socket_getPort(&GLOB(bindaddr));

	if (s->ourip.ss_family == AF_INET) {
		msg_out->data.ServerResMessage.serverIpAddr[0] = ((struct sockaddr_in *) &s->ourip)->sin_addr.s_addr;
	}

	sccp_dev_send(d, msg_out);
}

/*!
 * \brief Handle Config Status Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out;
	sccp_buttonconfig_t *config = NULL;
	uint8_t lines = 0;
	uint8_t speeddials = 0;
	
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == SPEEDDIAL) {
			speeddials++;
		} else if (config->type == LINE) {
			lines++;
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	REQ(msg_out, ConfigStatMessage);
	sccp_copy_string(msg_out->data.ConfigStatMessage.station_identifier.deviceName, d->id, sizeof(msg_out->data.ConfigStatMessage.station_identifier.deviceName));
	msg_out->data.ConfigStatMessage.station_identifier.lel_stationUserId = htolel(0);
	msg_out->data.ConfigStatMessage.station_identifier.lel_stationInstance = htolel(1);
	msg_out->data.ConfigStatMessage.lel_numberLines = htolel(lines);
	msg_out->data.ConfigStatMessage.lel_numberSpeedDials = htolel(speeddials);

	sccp_dev_send(d, msg_out);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending ConfigStatMessage, lines %d, speeddials %d\n", DEV_ID_LOG(d), lines, speeddials);
}

/*!
 * \brief Handle Enbloc Call Messsage (Dial in one block, instead of number by number)
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	int len = 0;

	char calledParty[25] = { 0 };
	uint32_t lineInstance = 0;

	if (d->protocol->parseEnblocCall) {
		d->protocol->parseEnblocCall((const sccp_msg_t *) msg_in, calledParty, &lineInstance);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: EnblocCall, party: %s, lineInstance %d\n", DEV_ID_LOG(d), calledParty, lineInstance);

		if (!sccp_strlen_zero(calledParty)) {
			AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

			if (channel) {
				if ((channel->state == SCCP_CHANNELSTATE_DIALING) || (channel->state == SCCP_CHANNELSTATE_OFFHOOK)) {
					/* for anonymous devices we just want to call the extension defined in hotine->exten -> ignore dialed number -MC */
					if (d->isAnonymous) {
						return;
					}

					len = strlen(channel->dialedNumber);
					sccp_copy_string(channel->dialedNumber + len, calledParty, sizeof(channel->dialedNumber) - len);
					sccp_pbx_softswitch(channel);
					return;
				}
				if (PBX(send_digits)) {
					PBX(send_digits) (channel, calledParty);
				}
				return;
			}
			if (!lineInstance) {									// v3 - v16 don't provide lineinstance during enbloc
				lineInstance = d->defaultLineInstance ? d->defaultLineInstance : SCCP_FIRST_LINEINSTANCE;
			}
			AUTO_RELEASE sccp_linedevices_t *linedevice = sccp_linedevice_findByLineinstance(d, lineInstance);

			if (linedevice) {
				AUTO_RELEASE sccp_channel_t *new_channel = NULL;

				new_channel = sccp_channel_newcall(linedevice->line, d, calledParty, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
			}

		}
	}
}

/*!
 * \brief Handle Forward Status Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out = NULL;

	uint32_t instance = letohl(msg_in->data.ForwardStatReqMessage.lel_lineNumber);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got Forward Status Request.  Line: %d\n", d->id, instance);

	AUTO_RELEASE sccp_line_t *l = sccp_line_find_byid(d, instance);

	if (l) {
		sccp_dev_forward_status(l, instance, d);
		return;
	}

	/* speeddial with hint. Sending empty forward message */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Send Forward Status. Instance: %d\n", d->id, instance);
	REQ(msg_out, ForwardStatMessage);
	msg_out->data.ForwardStatMessage.v3.lel_lineNumber = msg_in->data.ForwardStatReqMessage.lel_lineNumber;
	sccp_dev_send(d, msg_out);
}

/*!
 * \brief Handle Feature Status Reques for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_buttonconfig_t *config = NULL;

	int index = letohl(msg_in->data.FeatureStatReqMessage.lel_featureIndex);
	int capabilities = letohl(msg_in->data.FeatureStatReqMessage.lel_featureCapabilities);

	sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Unknown = %d \n", d->id, index, capabilities);

#ifdef CS_DYNAMIC_SPEEDDIAL
	/*
	 * the new speeddial style uses feature to display state
	 * unfortunately we dont know how to handle this on other way
	 */
	sccp_speed_t k;

	if ((capabilities == 1 && d->inuseprotocolversion >= 15)) {
		sccp_dev_speed_find_byindex(d, index, TRUE, &k);

		if (k.valid) {
			sccp_msg_t *msg_out;

			REQ(msg_out, FeatureStatDynamicMessage);
			msg_out->data.FeatureStatDynamicMessage.lel_featureIndex = htolel(index);
			msg_out->data.FeatureStatDynamicMessage.lel_featureID = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
			msg_out->data.FeatureStatDynamicMessage.lel_featureStatus = 0;

			//sccp_copy_string(msg_out->data.FeatureStatDynamicMessage.DisplayName, k.name, sizeof(msg_out->data.FeatureStatDynamicMessage.DisplayName));
			d->copyStr2Locale(d, msg_out->data.FeatureStatDynamicMessage.featureTextLabel, k.name, sizeof(msg_out->data.FeatureStatDynamicMessage.featureTextLabel));
			sccp_dev_send(d, msg_out);
			return;
		}
	}
#endif

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == index && config->type == FEATURE) {
			sccp_feat_changed(d, NULL, config->button.feature.id);
		}
	}
}

/*!
 * \brief Handle Feature Status Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_services_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out = NULL;
	sccp_buttonconfig_t *config = NULL;

	int urlIndex = letohl(msg_in->data.ServiceURLStatReqMessage.lel_serviceURLIndex);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got ServiceURL Status Request.  Index = %d\n", d->id, urlIndex);

	if ((config = sccp_dev_serviceURL_find_byindex(s->device, urlIndex))) {
		/* \todo move ServiceURLStatMessage impl to sccp_protocol.c */
		if (d->inuseprotocolversion < 7) {
			REQ(msg_out, ServiceURLStatMessage);
			msg_out->data.ServiceURLStatMessage.lel_serviceURLIndex = htolel(urlIndex);
			sccp_copy_string(msg_out->data.ServiceURLStatMessage.URL, config->button.service.url, strlen(config->button.service.url) + 1);
			//sccp_copy_string(msg_out->data.ServiceURLStatMessage.label, config->label, strlen(config->label) + 1);
			d->copyStr2Locale(d, msg_out->data.ServiceURLStatMessage.label, config->label, strlen(config->label) + 1);
		} else {
			int URL_len = strlen(config->button.service.url);
			int label_len = strlen(config->label);
			int dummy_len = URL_len + label_len;

			int hdr_len = sizeof(msg_in->data.ServiceURLStatDynamicMessage) - 1;
			int padding = ((dummy_len + hdr_len) % 4);

			padding = (padding > 0) ? 4 - padding : 0;

			msg_out = sccp_build_packet(ServiceURLStatDynamicMessage, hdr_len + dummy_len + padding);
			msg_out->data.ServiceURLStatDynamicMessage.lel_serviceURLIndex = htolel(urlIndex);

			if (dummy_len) {
				char buffer[dummy_len + 2];

				memset(&buffer[0], 0, dummy_len + 2);
				if (URL_len) {
					memcpy(&buffer[0], config->button.service.url, URL_len);
				}
				if (label_len) {
					memcpy(&buffer[URL_len + 1], config->label, label_len);
				}
				memcpy(&msg_out->data.ServiceURLStatDynamicMessage.dummy, &buffer[0], dummy_len + 2);
			}
		}
		sccp_dev_send(d, msg_out);
	} else {
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: serviceURL %d not assigned\n", DEV_ID_LOG(s->device), urlIndex);
	}
}

/*!
 * \brief Handle Feature Action for Device
 * \param d SCCP Device as sccp_device_t
 * \param instance Instance as int
 * \param toggleState as boolean
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
void sccp_handle_feature_action(sccp_device_t * d, int instance, boolean_t toggleState)
{
	sccp_buttonconfig_t *config = NULL;
	sccp_callforward_t status = 0;										/* state of cfwd */
	uint32_t featureStat1 = 0;
	uint32_t featureStat2 = 0;
	uint32_t featureStat3 = 0;
	uint32_t res = 0;

	if (!d) {
		return;
	}

	sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: instance: %d, toggle: %s\n", d->id, instance, (toggleState) ? "yes" : "no");

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == instance && config->type == FEATURE) {
			// sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: toggle status from %d", d->id, config->button.feature.status);
			// config->button.feature.status = (config->button.feature.status == 0) ? 1 : 0;
			// sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 " to %d\n", config->button.feature.status);
			break;
		}

	}

	if (!config || !config->type || config->type != FEATURE) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Couldn find feature with ID = %d \n", d->id, instance);
		return;
	}

	/* notice: we use this function for request and changing status -> so just change state if toggleState==TRUE -MC */
	char featureOption[255];

	if (!sccp_strlen_zero(config->button.feature.options)) {
		sccp_copy_string(featureOption, config->button.feature.options, sizeof(featureOption));
	}

	sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s\n", d->id, config->button.feature.id, featureOption);
	switch (config->button.feature.id) {

		case SCCP_FEATURE_PRIVACY:

			if (!d->privacyFeature.enabled) {
				sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: privacy feature is disabled, ignore this change\n", d->id);
				break;
			}

			if (!strcasecmp(config->button.feature.options, "callpresent")) {
				res = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

				sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", d->id, d->privacyFeature.status);
				sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result=%d\n", d->id, res);
				if (res) {
					/* switch off */
					d->privacyFeature.status &= ~SCCP_PRIVACYFEATURE_CALLPRESENT;
					config->button.feature.status = 0;
				} else {
					d->privacyFeature.status |= SCCP_PRIVACYFEATURE_CALLPRESENT;
					config->button.feature.status = 1;
				}
				sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", d->id, d->privacyFeature.status);
			} else {
				sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: do not know how to handle %s\n", d->id, config->button.feature.options);
			}

			break;
		case SCCP_FEATURE_CFWDALL:
			status = SCCP_CFWD_NONE;
			if (TRUE == toggleState) {
				config->button.feature.status = (config->button.feature.status == 0) ? 1 : 0;
			}
			// Ask for activation of the feature.
			if (!sccp_strlen_zero(config->button.feature.options)) {
				// Now set the feature status. Note that the button status has already been toggled above.
				if (config->button.feature.status) {
					status = SCCP_CFWD_ALL;
				}
			}

			SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
				if (config->type == LINE) {
					AUTO_RELEASE sccp_line_t *line = sccp_line_find_byname(config->button.line.name, FALSE);

					if (line) {
						sccp_line_cfwd(line, d, status, featureOption);
					}
				}
			}

			break;

		case SCCP_FEATURE_DND:
			if (TRUE == toggleState) {
				config->button.feature.status = (config->button.feature.status == 0) ? 1 : 0;
			}

			if (!strcasecmp(config->button.feature.options, "silent")) {
				d->dndFeature.status = (config->button.feature.status) ? SCCP_DNDMODE_SILENT : SCCP_DNDMODE_OFF;
			} else if (!strcasecmp(config->button.feature.options, "busy")) {
				d->dndFeature.status = (config->button.feature.status) ? SCCP_DNDMODE_REJECT : SCCP_DNDMODE_OFF;
			}

			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: dndmode %d is %s\n", d->id, d->dndFeature.status, (d->dndFeature.status) ? "on" : "off");
			sccp_dev_check_displayprompt(d);
			sccp_feat_changed(d, NULL, SCCP_FEATURE_DND);
			break;
#ifdef CS_SCCP_FEATURE_MONITOR
		case SCCP_FEATURE_MONITOR:
			if (TRUE == toggleState) {
				AUTO_RELEASE sccp_channel_t *channel = sccp_device_getActiveChannel(d);

				if (channel) {
					sccp_feat_monitor(d, NULL, 0, channel);
				}
			}

			break;
#endif

#ifdef CS_DEVSTATE_FEATURE

		/**
		  * Handling of custom devicestate toggle buttons.
		  */
		case SCCP_FEATURE_DEVSTATE:
			sccp_log((DEBUGCAT_CORE + DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Feature Change DevState: '%s', State: '%s'\n", DEV_ID_LOG(d), config->button.feature.options, config->button.feature.status ? "On" : "Off");

			if (TRUE == toggleState) {
				// char devstateName[100];
				// sprintf(devstateName, "Custom:%s", config->button.feature.options);
				enum ast_device_state newDeviceState = config->button.feature.status ? AST_DEVICE_NOT_INUSE : AST_DEVICE_INUSE;

				//pbx_devstate_changed_literal(newDeviceState, devstateName);
				ast_db_put("CustomDevstate", config->button.feature.options, ast_devstate_str(newDeviceState));
				pbx_devstate_changed(newDeviceState, "Custom:%s", config->button.feature.options);
			}

			break;
#endif
		case SCCP_FEATURE_MULTIBLINK:
			featureStat1 = (d->priFeature.status & 0xf) - 1;
			featureStat2 = ((d->priFeature.status & 0xf00) >> 8) - 1;
			featureStat3 = ((d->priFeature.status & 0xf0000) >> 16) - 1;

			if (2 == featureStat2 && 6 == featureStat1) {
				featureStat3 = (featureStat3 + 1) % 2;
			}
			if (6 == featureStat1) {
				featureStat2 = (featureStat2 + 1) % 3;
			}
			featureStat1 = (featureStat1 + 1) % 7;

			d->priFeature.status = ((featureStat3 + 1) << 16) | ((featureStat2 + 1) << 8) | (featureStat1 + 1);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: priority feature status: %d, %d, %d, total: %d\n", d->id, featureStat3, featureStat2, featureStat1, d->priFeature.status);
			break;

		default:
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: unknown feature\n", d->id);
			break;

	}

	if (config) {
		sccp_log((DEBUGCAT_FEATURE_BUTTON + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Status: %d\n", d->id, instance, config->button.feature.status);
		sccp_feat_changed(d, NULL, config->button.feature.id);
	}

	return;
}

#if defined(CS_SCCP_VIDEO) && defined(DEBUG)
static void sccp_handle_updatecapabilities_dissect_customPictureFormat(sccp_device_t *d, uint32_t customPictureFormatCount, customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES]) {
	uint8_t video_customPictureFormat = 0;
	if (customPictureFormatCount <= MAX_CUSTOM_PICTURES) {
		for (video_customPictureFormat = 0; video_customPictureFormat < customPictureFormatCount; video_customPictureFormat++) {
			int width = letohl(customPictureFormat[video_customPictureFormat].lel_width);
			int height = letohl(customPictureFormat[video_customPictureFormat].lel_height);
			int pixelAspectRatio = letohl(customPictureFormat[video_customPictureFormat].lel_pixelAspectRatio);
			int pixelClockConversion = letohl(customPictureFormat[video_customPictureFormat].lel_pixelclockConversionCode);
			int pixelClockDivisor = letohl(customPictureFormat[video_customPictureFormat].lel_pixelclockDivisor);

			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7s %-5s customPictureFormat %d: width=%d, height=%d, pixelAspectRatio=%d, pixelClockConversion=%d, pixelClockDivisor=%d\n", DEV_ID_LOG(d), "", "", video_customPictureFormat, width, height, pixelAspectRatio, pixelClockConversion, pixelClockDivisor);
		}
	} else {
		pbx_log(LOG_ERROR, "%s: Received customPictureFormatCount: %d out of bounds (%d)\n", DEV_ID_LOG(d), customPictureFormatCount, MAX_CUSTOM_PICTURES);
	}
}

static void sccp_handle_updatecapabilities_dissect_levelPreference(sccp_device_t *d, uint32_t levelPreferenceCount, levelPreference_t levelPreference[MAX_LEVEL_PREFERENCE]) {
	uint8_t level = 0;
	if (levelPreferenceCount <= MAX_LEVEL_PREFERENCE) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7s Codec has %d levelPreferences:\n", DEV_ID_LOG(d), "", levelPreferenceCount);
		for (level = 0; level < levelPreferenceCount; level++) {
			int transmitPreference = letohl(levelPreference[level].lel_transmitPreference);
			skinny_videoformat_t video_format = letohl(levelPreference[level].lel_format);
			int maxBitRate = letohl(levelPreference[level].lel_maxBitRate);
			int minBitRate = letohl(levelPreference[level].lel_minBitRate);
			int MPI = letohl(levelPreference[level].lel_MPI);
			int serviceNumber = letohl(levelPreference[level].lel_serviceNumber);

			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %6s %2d: %-3s transmitPreference: %d\n", DEV_ID_LOG(d), "", level, "", transmitPreference);
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %14s format: %d: %s\n", DEV_ID_LOG(d), "", video_format, skinny_videoformat2str(video_format));
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %14s maxBitRate: %d\n", DEV_ID_LOG(d), "", maxBitRate);
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %14s minBitRate: %d\n", DEV_ID_LOG(d), "", minBitRate);
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %14s MPI: %d\n", DEV_ID_LOG(d), "", MPI);
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %14s serviceNumber: %d\n", DEV_ID_LOG(d), "", serviceNumber);
		}
	} else {
		pbx_log(LOG_ERROR, "%s: Received levelPreferenceCount: %d out of bounds (%d)\n", DEV_ID_LOG(d), levelPreferenceCount, MAX_LEVEL_PREFERENCE);
	}
}

static void sccp_handle_updatecapabilities_dissect_videocapabiltyunion(sccp_device_t *d, uint32_t video_codec, videoCapabilityUnionV2_t *capability) {
	switch (video_codec) {
		case SKINNY_CODEC_H261:
			{
				int temporalSpatialTradeOffCapability = letohl(capability->h261.lel_temporalSpatialTradeOffCapability);
				int stillImageTransmission = letohl(capability->h261.lel_stillImageTransmission);

				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s temporalSpatialTradeOff: %d\n", DEV_ID_LOG(d), "", temporalSpatialTradeOffCapability);
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s stillImageTransmission: %d\n", DEV_ID_LOG(d), "", stillImageTransmission);
			}
			break;
		case SKINNY_CODEC_H263:
			{
				int capabilityBitfield = letohl(capability->h263.lel_capabilityBitfield);
				int annexNandW = letohl(capability->h263.lel_annexNandWFutureUse);

				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s capabilityBitfield: %d\n", DEV_ID_LOG(d), "", capabilityBitfield);
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s annexNandW: %d\n", DEV_ID_LOG(d), "", annexNandW);
			}
			break;
		case SKINNY_CODEC_H263P:		/* Vieo */
			{
				int modelNumber= letohl(capability->h263P.lel_modelNumber);
				int bandwidth = letohl(capability->h263P.lel_bandwidth);

				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s model: %d\n", DEV_ID_LOG(d), "", modelNumber);
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s bandwidth: %d\n", DEV_ID_LOG(d), "", bandwidth);
			}
			break;
		case SKINNY_CODEC_H264:
			{
				int level = letohl(capability->h264.lel_level);
				int profile = letohl(capability->h264.lel_profile);

				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s level: %d\n", DEV_ID_LOG(d), "", level);
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-7s profile: %d\n", DEV_ID_LOG(d), "", profile);
			}
			break;
	}
}
#endif

/*!
 * \brief Handle Update Capabilities Message
 *
 * This message is often used to add video and data capabilities to client. Atm we just use it for audio and video caps.
 * Will be better to store audio codec max packet size and video bandwidth and size.
 * In future we will parse also data caps to support T.38 and NSE with ATA186/188 devices.
 *
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 *
 * \since 20090708
 * \author Federico
 */
void sccp_handle_updatecapabilities_message(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	if (letohl(msg_in->header.lel_protocolVer) >= 16) {
		sccp_handle_updatecapabilities_V2_message(s, d, msg_in);
	} else {
		uint8_t audio_capability = 0, audio_codec = 0, audio_capabilities = 0;
		uint32_t maxFramesPerPacket = 0;
		/* parsing audio caps */
		audio_capabilities = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.lel_audioCapCount);
		int RTPPayloadFormat = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.lel_RTPPayloadFormat);
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Audio Capabilities, RTPPayloadFormat=%d\n", DEV_ID_LOG(d), audio_capabilities, RTPPayloadFormat);

		if (audio_capabilities > 0 && audio_capabilities <= SKINNY_MAX_CAPABILITIES) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7s %-25s %-9s\n", DEV_ID_LOG(d), "#", "codec", "maxFrames");
			for (audio_capability = 0; audio_capability < audio_capabilities; audio_capability++) {
				audio_codec = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].lel_payloadCapability);
				maxFramesPerPacket = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].lel_maxFramesPerPacket);

				d->capabilities.audio[audio_capability] = audio_codec;		/** store our audio capabilities */
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7d %-25s %-6d\n", DEV_ID_LOG(d), audio_codec, codec2str(audio_codec), maxFramesPerPacket);
				
				if (audio_codec == SKINNY_CODEC_G723_1) {
					sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s bitRate: %d\n", DEV_ID_LOG(d), "", letohl(msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].payloads.lel_g723BitRate));
				} else {
					sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s codecMode: %d, dynamicPayload: %d, codecParam1: %d, codecParam2: %d\n", DEV_ID_LOG(d), "", msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].payloads.codecParams.codecMode, msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].payloads.codecParams.dynamicPayload, msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].payloads.codecParams.codecParam1, msg_in->data.UpdateCapabilitiesMessage.v3.audioCaps[audio_capability].payloads.codecParams.codecParam2);
				}
			}
		}
	#ifdef CS_SCCP_VIDEO
		uint8_t video_customPictureFormat = 0, video_customPictureFormats = 0;
		video_customPictureFormats = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.lel_customPictureFormatCount);
		for (video_customPictureFormat = 0; video_customPictureFormat < video_customPictureFormats; video_customPictureFormat++) {
			int width = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.customPictureFormat[video_customPictureFormat].lel_width);
			int height = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.customPictureFormat[video_customPictureFormat].lel_height);
			int pixelAspectRatio = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.customPictureFormat[video_customPictureFormat].lel_pixelAspectRatio);
			int pixelClockConversion = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.customPictureFormat[video_customPictureFormat].lel_pixelclockConversionCode);
			int pixelClockDivisor = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.customPictureFormat[video_customPictureFormat].lel_pixelclockDivisor);

			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %6s %-5s customPictureFormat %d: width=%d, height=%d, pixelAspectRatio=%d, pixelClockConversion=%d, pixelClockDivisor=%d\n", DEV_ID_LOG(d), "", "", video_customPictureFormat, width, height, pixelAspectRatio, pixelClockConversion, pixelClockDivisor);
		}
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %6s %-5s %s\n", DEV_ID_LOG(d), "", "", "--");
		uint8_t video_capabilities = 0, video_capability = 0;
		uint8_t video_codec = 0;
		boolean_t previousVideoSupport = sccp_device_isVideoSupported(d);					/* to check if this update changes the video capabilities */

		/* parsing video caps */
		video_capabilities = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.lel_videoCapCount);

		/* enable video mode button if device has video capability */
		if (video_capabilities > 0 && video_capabilities <= SKINNY_MAX_VIDEO_CAPABILITIES) {
			sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
			sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: enable video mode softkey\n", DEV_ID_LOG(d));


			sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Video Capabilities\n", DEV_ID_LOG(d), video_capabilities);
			for (video_capability = 0; video_capability < video_capabilities; video_capability++) {
				video_codec = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_payloadCapability);

				d->capabilities.video[video_capability] = video_codec;		/** store our video capabilities */
#if DEBUG
//				char transmitReceiveStr[5];
//				sprintf(transmitReceiveStr, "%c-%c", (letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_RECEIVE) ? '<' : ' ', (letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_TRANSMIT) ? '>' : ' ');
//				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %-3s %3d %-25s\n", DEV_ID_LOG(d), transmitReceiveStr, video_codec, codec2str(video_codec));

//				int protocolDependentData = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_protocolDependentData);
//				int maxBitRate = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_maxBitRate);

//				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %6s %-5s protocolDependentData: %d\n", DEV_ID_LOG(d), "", "", protocolDependentData);
//				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %6s %-5s maxBitRate: %d\n", DEV_ID_LOG(d), "", "", maxBitRate);
				char transmitReceiveStr[5];
				sprintf(transmitReceiveStr, "%c-%c", (letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_RECEIVE) ? '<' : ' ', (letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_TRANSMIT) ? '>' : ' ');
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %2d: %-3s %3d %-25s\n", DEV_ID_LOG(d), video_capability, transmitReceiveStr, video_codec, codec2str(video_codec));
				sccp_handle_updatecapabilities_dissect_videocapabiltyunion(d, video_codec, (videoCapabilityUnionV2_t *)&msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].capability);

				uint8_t levelPreferences = letohl(msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].lel_levelPreferenceCount);
				sccp_handle_updatecapabilities_dissect_levelPreference(d, levelPreferences, msg_in->data.UpdateCapabilitiesMessage.v3.videoCaps[video_capability].levelPreference);
#endif
			}
			if (previousVideoSupport == FALSE) {
				sccp_dev_set_message(d, "Video support enabled", 5, FALSE, TRUE);
			}
		} else {
			d->capabilities.video[0] = SKINNY_CODEC_NONE;
			sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, FALSE);
			sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: disable video mode softkey\n", DEV_ID_LOG(d));
			if (previousVideoSupport == TRUE) {
				sccp_dev_set_message(d, "Video support disabled", 5, FALSE, TRUE);
			}
		}
#endif
	}
}
/*!
 * \brief Handle Update Capabilities Message
 *
 * This message is often used to add video and data capabilities to client. Atm we just use it for audio and video caps.
 * Will be better to store audio codec max packet size and video bandwidth and size.
 * In future we will parse also data caps to support T.38 and NSE with ATA186/188 devices.
 *
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_updatecapabilities_V2_message(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint8_t audio_capability = 0, audio_codec = 0, audio_capabilities = 0;
	uint32_t maxFramesPerPacket = 0;

	/* parsing audio caps */
	audio_capabilities = letohl(msg_in->data.UpdateCapabilitiesV2Message.lel_audioCapCount);
	int RTPPayloadFormat = letohl(msg_in->data.UpdateCapabilitiesV2Message.lel_RTPPayloadFormat);
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Audio Capabilities, RTPPayloadFormat=%d (V2)\n", DEV_ID_LOG(d), audio_capabilities, RTPPayloadFormat);

	if (audio_capabilities > 0 && audio_capabilities <= SKINNY_MAX_CAPABILITIES) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7s %-25s %-9s\n", DEV_ID_LOG(d), "#", "codec", "maxFrames");
		for (audio_capability = 0; audio_capability < audio_capabilities; audio_capability++) {
			audio_codec = letohl(msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].lel_payloadCapability);
			maxFramesPerPacket = letohl(msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].lel_maxFramesPerPacket);

			d->capabilities.audio[audio_capability] = audio_codec;		/** store our audio capabilities */
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7d %-25s %-6d\n", DEV_ID_LOG(d), audio_codec, codec2str(audio_codec), maxFramesPerPacket);
			
			if (audio_codec == SKINNY_CODEC_G723_1) {
				sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s bitRate: %d\n", DEV_ID_LOG(d), "", letohl(msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].payloads.lel_g723BitRate));
			} else {
				sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s codecMode: %d, dynamicPayload: %d, codecParam1: %d, codecParam2: %d\n", DEV_ID_LOG(d), "", msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].payloads.codecParams.codecMode, msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].payloads.codecParams.dynamicPayload, msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].payloads.codecParams.codecParam1, msg_in->data.UpdateCapabilitiesV2Message.audioCaps[audio_capability].payloads.codecParams.codecParam2);
			}
		}
	}
#ifdef CS_SCCP_VIDEO
#if DEBUG
	uint8_t video_customPictureFormats = letohl(msg_in->data.UpdateCapabilitiesV2Message.lel_customPictureFormatCount);
	sccp_handle_updatecapabilities_dissect_customPictureFormat(d, video_customPictureFormats, msg_in->data.UpdateCapabilitiesV2Message.customPictureFormat);
#endif

	uint8_t video_capabilities = 0, video_capability = 0;
	uint8_t video_codec = 0;
	boolean_t previousVideoSupport = sccp_device_isVideoSupported(d);					/* to check if this update changes the video capabilities */

	/* parsing video caps */
	video_capabilities = letohl(msg_in->data.UpdateCapabilitiesV2Message.lel_videoCapCount);

	/* enable video mode button if device has video capability */
	if (video_capabilities > 0 && video_capabilities <= SKINNY_MAX_VIDEO_CAPABILITIES) {
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: enable video mode softkey\n", DEV_ID_LOG(d));

		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Video Capabilities\n", DEV_ID_LOG(d), video_capabilities);
		for (video_capability = 0; video_capability < video_capabilities; video_capability++) {
			video_codec = letohl(msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].lel_payloadCapability);

			d->capabilities.video[video_capability] = video_codec;		/** store our video capabilities */
#if DEBUG
			char transmitReceiveStr[5];
			sprintf(transmitReceiveStr, "%c-%c", (letohl(msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_RECEIVE) ? '<' : ' ', (letohl(msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_TRANSMIT) ? '>' : ' ');
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %2d: %-3s %3d %-25s\n", DEV_ID_LOG(d), video_capability, transmitReceiveStr, video_codec, codec2str(video_codec));
			sccp_handle_updatecapabilities_dissect_videocapabiltyunion(d, video_codec, &msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].capability);

			uint8_t levelPreferences = letohl(msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].lel_levelPreferenceCount);
			sccp_handle_updatecapabilities_dissect_levelPreference(d, levelPreferences, msg_in->data.UpdateCapabilitiesV2Message.videoCaps[video_capability].levelPreference);
#endif
		}
		if (previousVideoSupport == FALSE) {
			sccp_dev_set_message(d, "Video support enabled", 5, FALSE, TRUE);
		}
	} else {
		d->capabilities.video[0] = SKINNY_CODEC_NONE;
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: disable video mode softkey\n", DEV_ID_LOG(d));
		if (previousVideoSupport == TRUE) {
			sccp_dev_set_message(d, "Video support disabled", 5, FALSE, TRUE);
		}
	}
#endif
}

/*!
 * \brief Handle Update Capabilities Message
 *
 * This message is often used to add video and data capabilities to client. Atm we just use it for audio and video caps.
 * Will be better to store audio codec max packet size and video bandwidth and size.
 * In future we will parse also data caps to support T.38 and NSE with ATA186/188 devices.
 *
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_updatecapabilities_V3_message(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint8_t audio_capability = 0, audio_codec = 0, audio_capabilities = 0;
	uint32_t maxFramesPerPacket = 0;

	/* parsing audio caps */
	audio_capabilities = letohl(msg_in->data.UpdateCapabilitiesV3Message.lel_audioCapCount);
	int RTPPayloadFormat = letohl(msg_in->data.UpdateCapabilitiesV3Message.lel_RTPPayloadFormat);
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Audio Capabilities, RTPPayloadFormat=%d (V3)\n", DEV_ID_LOG(d), audio_capabilities, RTPPayloadFormat);

	if (audio_capabilities > 0 && audio_capabilities <= SKINNY_MAX_CAPABILITIES) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7s %-25s %-9s\n", DEV_ID_LOG(d), "#", "codec", "maxFrames");
		for (audio_capability = 0; audio_capability < audio_capabilities; audio_capability++) {
			audio_codec = letohl(msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].lel_payloadCapability);
			maxFramesPerPacket = letohl(msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].lel_maxFramesPerPacket);

			d->capabilities.audio[audio_capability] = audio_codec;		/** store our audio capabilities */
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %7d %-25s %-6d\n", DEV_ID_LOG(d), audio_codec, codec2str(audio_codec), maxFramesPerPacket);
			
			if (audio_codec == SKINNY_CODEC_G723_1) {
				sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s bitRate: %d\n", DEV_ID_LOG(d), "", letohl(msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].payloads.lel_g723BitRate));
			} else {
				sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: %7s codecMode: %d, dynamicPayload: %d, codecParam1: %d, codecParam2: %d\n", DEV_ID_LOG(d), "", msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].payloads.codecParams.codecMode, msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].payloads.codecParams.dynamicPayload, msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].payloads.codecParams.codecParam1, msg_in->data.UpdateCapabilitiesV3Message.audioCaps[audio_capability].payloads.codecParams.codecParam2);
			}
		}
	}
	
#ifdef CS_SCCP_VIDEO
#if DEBUG
	uint8_t video_customPictureFormats = letohl(msg_in->data.UpdateCapabilitiesV2Message.lel_customPictureFormatCount);
	sccp_handle_updatecapabilities_dissect_customPictureFormat(d, video_customPictureFormats, msg_in->data.UpdateCapabilitiesV3Message.customPictureFormat);
#endif

	uint8_t video_capabilities = 0, video_capability = 0;
	uint8_t video_codec = 0;
	boolean_t previousVideoSupport = sccp_device_isVideoSupported(d);					/* to check if this update changes the video capabilities */

	/* parsing video caps */
	video_capabilities = letohl(msg_in->data.UpdateCapabilitiesV3Message.lel_videoCapCount);

	/* enable video mode button if device has video capability */
	if (video_capabilities > 0 && video_capabilities <= SKINNY_MAX_VIDEO_CAPABILITIES) {
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: enable video mode softkey\n", DEV_ID_LOG(d));


		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Video Capabilities\n", DEV_ID_LOG(d), video_capabilities);
		for (video_capability = 0; video_capability < video_capabilities; video_capability++) {
			video_codec = letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_payloadCapability);

			d->capabilities.video[video_capability] = video_codec;		/** store our video capabilities */
#if DEBUG
			char transmitReceiveStr[5];
			sprintf(transmitReceiveStr, "%c-%c", (letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_RECEIVE) ? '<' : ' ', (letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_TRANSMITRECEIVE_TRANSMIT) ? '>' : ' ');
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: %2d: %-3s %3d %-25s\n", DEV_ID_LOG(d), video_capability, transmitReceiveStr, video_codec, codec2str(video_codec));
			sccp_handle_updatecapabilities_dissect_videocapabiltyunion(d, video_codec, &msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].capability);

			uint8_t levelPreferences = letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_levelPreferenceCount);
			sccp_handle_updatecapabilities_dissect_levelPreference(d, levelPreferences, msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].levelPreference);

			int encryptionCapability = letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_encryptionCapability);
			sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: EncryptionCapability: %s\n", DEV_ID_LOG(d), encryptionCapability ? "Yes" : "No");

			int ipv46 = letohl(msg_in->data.UpdateCapabilitiesV3Message.videoCaps[video_capability].lel_ipv46);
			sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: IPV46 Setting: %s\n", DEV_ID_LOG(d), ipv46 == 0 ? "IPv4" : ipv46 == 1 ? "IPv6" : "Mixed Mode");
#endif
		}
		if (previousVideoSupport == FALSE) {
			sccp_dev_set_message(d, "Video support enabled", 5, FALSE, TRUE);
		}
	} else {
		d->capabilities.video[0] = SKINNY_CODEC_NONE;
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: disable video mode softkey\n", DEV_ID_LOG(d));
		if (previousVideoSupport == TRUE) {
			sccp_dev_set_message(d, "Video support disabled", 5, FALSE, TRUE);
		}
	}
#endif

}

/*!
 * \brief Handle Keep Alive Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_KeepAliveMessage(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	sccp_msg_t *msg_out = sccp_build_packet(KeepAliveAckMessage, 0);

	sccp_session_send2(s, msg_out);
}

/*!
 * \brief Handle Device to User Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_device_to_user(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint32_t appID;
	uint32_t callReference;
	uint32_t transactionID;
	uint32_t dataLength;
	char data[StationMaxXMLMessage] = "";

#ifdef CS_SCCP_CONFERENCE
	uint32_t lineInstance;
	uint32_t conferenceID;
	uint32_t participantID;
#endif

	appID = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_appID);
#ifdef CS_SCCP_CONFERENCE
	lineInstance = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_lineInstance);
#endif
	callReference = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_callReference);
	transactionID = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_transactionID);
	dataLength = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_dataLength);

	if (dataLength) {
		memset(data, 0, dataLength);
		memcpy(data, msg_in->data.DeviceToUserDataVersion1Message.data, dataLength);
	}

	sccp_log((DEBUGCAT_ACTION + DEBUGCAT_MESSAGE + DEBUGCAT_DEVICE + DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle DTU for AppID:%d, data:'%s', length:%d\n", d->id, appID, data, dataLength);
	if (0 != appID && 0 != callReference && 0 != transactionID) {
		switch (appID) {
			case APPID_CONFERENCE:									// Handle Conference App
#ifdef CS_SCCP_CONFERENCE
				conferenceID = lineInstance;							// conferenceId is passed on via lineInstance
				participantID = atoi(data);							// participantId is passed on in the data segment
				sccp_log((DEBUGCAT_ACTION + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Handle ConferenceList Info for AppID %d , CallID %d, Transaction %d, Conference %d, Participant: %d\n", d->id, appID, callReference, transactionID, conferenceID, participantID);
				sccp_conference_handle_device_to_user(d, callReference, transactionID, conferenceID, participantID);
#endif
				break;
			case APPID_CONFERENCE_INVITE:								// Handle Conference Invite
#ifdef CS_SCCP_CONFERENCE
				conferenceID = lineInstance;							// conferenceId is passed on via lineInstance
				participantID = atoi(data);							// participantId is passed on in the data segment
				//phonenumber = atoi(data);
				sccp_log((DEBUGCAT_ACTION + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Handle ConferenceList Info for AppID %d , CallID %d, Transaction %d, Conference %d, Participant: %d\n", d->id, appID, callReference, transactionID, conferenceID, participantID);
				//sccp_conference_handle_device_to_user(d, callReference, transactionID, conferenceID, participantID);
#endif
				break;
			case APPID_PROVISION:
				break;
		}
	} else {
		// It has data -> must be a softkey
		if (dataLength) {
			/* split data by "/" */
			char str_action[10] = "", str_transactionID[10] = "";

			if (sscanf(data, "%[^/]/%s", str_action, str_transactionID) > 0) {
				sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_MESSAGE + DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle DTU Softkey Button:%s, %s\n", d->id, str_action, str_transactionID);
				d->dtu_softkey.action = strdup(str_action);
				d->dtu_softkey.transactionID = atoi(str_transactionID);
			} else {
				pbx_log(LOG_NOTICE, "%s: Failure parsing DTU Softkey Button: %s\n", d->id, data);
			}
		}
	}

}

/*!
 * \brief Handle Device to User Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param msg_in SCCP Message
 */
void sccp_handle_device_to_user_response(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	uint32_t appID;
	uint32_t lineInstance;
	uint32_t callReference;
	uint32_t transactionID;
	uint32_t dataLength;
	char data[StationMaxXMLMessage] = { 0 };

	appID = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_appID);
	lineInstance = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_lineInstance);
	callReference = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_callReference);
	transactionID = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_transactionID);
	dataLength = letohl(msg_in->data.DeviceToUserDataVersion1Message.lel_dataLength);

	if (dataLength) {
		sccp_copy_string(data, msg_in->data.DeviceToUserDataVersion1Message.data, dataLength);
	}

	sccp_log((DEBUGCAT_ACTION + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: DTU Response: AppID %d , LineInstance %d, CallID %d, Transaction %d\n", d->id, appID, lineInstance, callReference, transactionID);
	sccp_log_and((DEBUGCAT_MESSAGE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: DTU Response: Data %s\n", d->id, data);

	if (appID == APPID_DEVICECAPABILITIES) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device Capabilities Response '%s'\n", d->id, data);
	}
}

/*!
 * \brief Handle Miscellaneous Command Message
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param msg_in SCCP Message
 */
void sccp_handle_miscellaneousCommandMessage(sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg_in)
{
	skinny_miscCommandType_t commandType;
	struct sockaddr_in sin = { 0 };
	uint32_t conferenceId = letohl(msg_in->data.MiscellaneousCommandMessage.lel_conferenceId);
	uint32_t callReference = letohl(msg_in->data.MiscellaneousCommandMessage.lel_callReference);
	uint32_t passThruPartyId = letohl(msg_in->data.MiscellaneousCommandMessage.lel_passThruPartyId);
	commandType = letohl(msg_in->data.MiscellaneousCommandMessage.lel_miscCommandType);

//	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO8941 && 0 == passThruPartyId) {
	if (0 == passThruPartyId) {
		passThruPartyId = 0xFFFFFFFF - callReference;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Dealing with 8941 which does not return a passThruPartyId, using callid: %u -> passThruPartyId %u\n", d->id, callReference, passThruPartyId);
	}
	
	AUTO_RELEASE sccp_channel_t *channel = NULL;
	if ((d->active_channel && d->active_channel->passthrupartyid == passThruPartyId) || !passThruPartyId) {	// reduce the amount of searching by first checking active_channel
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passThruPartyId);
	}
	
	if (channel) {
		switch (commandType) {
			case SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE:
				break;
			case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE:
				memcpy(&sin.sin_addr, &msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdatePicture.bel_remoteIpAddr, sizeof(sin.sin_addr));
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: media statistic for %s, value1: %u, value2: %u, value3: %u, value4: %u\n",
							  channel ? channel->currentDeviceId : "--", pbx_inet_ntoa(sin.sin_addr), letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value1), letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value2), letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value3), letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value4)
				    );
				break;
			case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEGOB:
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: VideoFastUpdateGob, firstGOB: %d, numberOfGOBs: %d\n",
							  channel ? channel->currentDeviceId : "--", 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdateGOB.lel_firstGOB),
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdateGOB.lel_numberOfGOBs)
				    );
				break;
			case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEMB:
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: VideoFastUpdateMB, firstGOB: %d, firstMB: %d, numberOfMBs: %d\n",
							  channel ? channel->currentDeviceId : "--", 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdateMB.lel_firstGOB), 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdateMB.lel_firstMB), 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.videoFastUpdateMB.lel_numberOfMBs)
				    );
				break;
			case SKINNY_MISCCOMMANDTYPE_LOSTPICTURE:
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: LostPicture, pictureNumber %d, longTermPictureIndex %d\n",
							  channel ? channel->currentDeviceId : "--", 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPicture.lel_pictureNumber), 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPicture.lel_longTermPictureIndex)
				    );
				break;
			case SKINNY_MISCCOMMANDTYPE_LOSTPARTIALPICTURE:
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: LostPartialPicture, picRef:pictureNumber %d, picRef:longTermPictureIndex %d, firstMB: %d, numberOfMBs: %d\n",
							  channel ? channel->currentDeviceId : "--", 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPartialPicture.pictureReference.lel_pictureNumber), 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPartialPicture.pictureReference.lel_longTermPictureIndex),
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPartialPicture.lel_firstMB), 
							  letohl(msg_in->data.MiscellaneousCommandMessage.data.lostPartialPicture.lel_numberOfMBs)
				    );
				break;
			case SKINNY_MISCCOMMANDTYPE_RECOVERYREFERENCEPICTURE:
				{
					int x = 0;
					int pictureCount = letohl(msg_in->data.MiscellaneousCommandMessage.data.recoveryReferencePicture.lel_PictureCount);
					sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: recoveryReferencePicture, pictureCount:%d\n",
								  channel ? channel->currentDeviceId : "--", 
								  pictureCount);
					for (x = 0; x < pictureCount; x++) {
						sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: recoveryReferencePicture[%d], pictureNumber %d, longTermPictureIndex %d\n",
									channel ? channel->currentDeviceId : "--", 
									x,
									letohl(msg_in->data.MiscellaneousCommandMessage.data.recoveryReferencePicture.pictureReference[x].lel_pictureNumber), 
									letohl(msg_in->data.MiscellaneousCommandMessage.data.recoveryReferencePicture.pictureReference[x].lel_longTermPictureIndex)
						    );
					}
				}
				break;
			case SKINNY_MISCCOMMANDTYPE_TEMPORALSPATIALTRADEOFF:
				sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: recoveryReferencePicture, TemporalSpatialTradeOff:%d\n",
							  channel ? channel->currentDeviceId : "--", 
							   letohl(msg_in->data.MiscellaneousCommandMessage.data.lel_temporalSpatialTradeOff));
			default:
				break;
		}
		if (channel->owner) {
			PBX(queue_control) (channel->owner, AST_CONTROL_VIDUPDATE);
		}
		return;
	}
	pbx_log(LOG_WARNING, "%s: Channel with passthrupartyid %u could not be found (callRef: %u/ confId: %u)\n", DEV_ID_LOG(d), passThruPartyId, callReference, conferenceId);
	sccp_dump_msg(msg_in);
	return;
}

// kate: indent-width 4; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets on;
