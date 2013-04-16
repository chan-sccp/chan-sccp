
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
 * \remarks     Purpose:        SCCP Actions
 *              When to use:    Only methods directly related to sccp actions should be stored in this source file.
 *                              Actions handle all the message interactions from and to SCCP phones
 *              Relationships:  Other Function wanting to communicate to phones
 *                              Phones Requesting function to be performed
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#include <math.h>
#if ASTERISK_VERSION_NUMBER < 10400
* !*\brief Host Access Rule Structure * /struct ast_ha {
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
 * \param r SCCP MOO
 */
void sccp_handle_alarm(sccp_session_t * no_s, sccp_device_t * no_d, sccp_moo_t * r)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Alarm Message: Severity: %s (%d), %s [%d/%d]\n", alarm2str(letohl(r->msg.AlarmMessage.lel_alarmSeverity)), letohl(r->msg.AlarmMessage.lel_alarmSeverity), r->msg.AlarmMessage.text, letohl(r->msg.AlarmMessage.lel_parm1), letohl(r->msg.AlarmMessage.lel_parm2));
}

/*!
 * \brief Handle Unknown Message
 * \param no_s SCCP Session = NULL
 * \param no_d SCCP Device = NULL
 * \param r SCCP Moo
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
void sccp_handle_unknown_message(sccp_session_t * no_s, sccp_device_t * no_d, sccp_moo_t * r)
{
	uint32_t mid = letohl(r->header.lel_messageId);

	if ((GLOB(debug) & DEBUGCAT_MESSAGE) != 0) {								// only show when debugging messages
		pbx_log(LOG_WARNING, "Unhandled SCCP Message: %s(0x%04X) %d bytes length\n", message2str(mid), mid, r->header.length);
		sccp_dump_packet((unsigned char *) &r->msg, r->header.length);
	}
}

/*!
 * \brief Handle Unknown Message
 * \param no_s SCCP Session = NULL
 * \param no_d SCCP Device = NULL
 * \param r SCCP Moo
 */
void sccp_handle_XMLAlarmMessage(sccp_session_t * no_s, sccp_device_t * no_d, sccp_moo_t * r)
{
	uint32_t mid = letohl(r->header.lel_messageId);
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

	char *xmlData = sccp_strdupa((char *) &r->msg.RegisterMessage);
	char *state;
	char *line;

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
		pbx_log(LOG_WARNING, "SCCP XMLAlarm Message: %s(0x%04X) %d bytes length\n", message2str(mid), mid, r->header.length);
		sccp_dump_packet((unsigned char *) &r->msg.RegisterMessage, r->header.length);
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
 * \param r SCCP Moo
 *
 * \callgraph
 * \callergraph
 *
 * \todo Implement a decision when to send RegisterTokenAck and when to send RegisterTokenReject
 *       If sending RegisterTokenReject what should the lel_tokenRejWaitTime (BackOff time) be
 */
void sccp_handle_token_request(sccp_session_t * s, sccp_device_t * no_d, sccp_moo_t * r)
{
	sccp_device_t *device;
	char *deviceName = "";
	uint32_t serverInstance = 0;
	uint32_t deviceInstance = 0;
	uint32_t deviceType = 0;

	deviceName = sccp_strdupa(r->msg.RegisterTokenRequest.sId.deviceName);
	serverInstance = letohl(r->msg.RegisterTokenRequest.sId.lel_instance);					// should be named deviceInstance as in handle_register
	deviceInstance = letohl(r->msg.RegisterTokenRequest.sId.lel_instance);
	deviceType = letohl(r->msg.RegisterTokenRequest.lel_deviceType);

	//      sccp_dump_packet((unsigned char *)&r->msg.RegisterTokenRequest, r->header.length);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "%s: is requesting a Token, Instance: %d, Type: %s (%d)\n", deviceName, deviceInstance, devicetype2str(deviceType), deviceType);

	// Search for already known devices -> Cleanup
	device = sccp_device_find_byid(deviceName, TRUE);
	if (!device && GLOB(allowAnonymous)) {
		device = sccp_device_createAnonymous(r->msg.RegisterMessage.sId.deviceName);
		sccp_config_applyDeviceConfiguration(device, NULL);
		sccp_config_addButton(&device->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", deviceName, GLOB(hotline)->line->name);
		device->defaultLineInstance = 1;
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
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", r->msg.RegisterMessage.sId.deviceName, pbx_inet_ntoa(s->sin.sin_addr));
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		s = sccp_session_reject(s, "IP Not Authorized");
		device = sccp_device_release(device);
		return;
	}

	if (device->session && device->session != s) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Crossover device registration!\n", device->id);
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		sccp_session_tokenReject(s, GLOB(token_backoff_time));
		s = sccp_session_reject(s, "Crossover session not allowed");
		device->session = sccp_session_reject(device->session, "Crossover session not allowed");
		device = sccp_device_release(device);
		return;
	}

	/* all checks passed, assign session to device */
	//      device->session = s;

	/*Currently rejecting token until further notice */
	boolean_t sendAck = FALSE;
	int last_digit = deviceName[strlen(deviceName)];

	if (!strcasecmp("true", GLOB(token_fallback))) {
		/* we are the primary server */
		if (serverInstance == 1) {									// need to check if it gets increased by changing xml.cnf member priority ?
			sendAck = TRUE;
		}
	} else if (!strcasecmp("odd", GLOB(token_fallback))) {
		if (last_digit % 2 != 0)
			sendAck = TRUE;
	} else if (!strcasecmp("even", GLOB(token_fallback))) {
		if (last_digit % 2 == 0)
			sendAck = TRUE;
	}

	/* some test to detect active calls */
	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: serverInstance: %d, unknown: %d, active call? %s\n", deviceName, serverInstance, letohl(r->msg.RegisterTokenRequest.unknown), (letohl(r->msg.RegisterTokenRequest.unknown) & 0x6) ? "yes" : "no");

	device->registrationState = SKINNY_DEVICE_RS_TOKEN;
	if (sendAck) {
		sccp_log((DEBUGCAT_ACTION + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending phone a token acknowledgement\n", deviceName);
		sccp_session_tokenAck(s);
	} else {
		sccp_log((DEBUGCAT_ACTION + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending phone a token rejection (sccp.conf:fallback=%s, serverInstance=%d), ask again in '%d' seconds\n", deviceName, GLOB(token_fallback), serverInstance, GLOB(token_backoff_time));
		sccp_session_tokenReject(s, GLOB(token_backoff_time));
	}

	device->status.token = (sendAck) ? SCCP_TOKEN_STATE_ACK : SCCP_TOKEN_STATE_REJ;
	device->registrationTime = time(0);									// last time device tried sending token
	device = sccp_device_release(device);
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
 * \param r SCCP Moo
 *
 * \callgraph
 * \callergraph
 *
 * \todo Implement a decision when to send RegisterTokenAck and when to send RegisterTokenReject
 *       If sending RegisterTokenReject what should the lel_tokenRejWaitTime (BackOff time) be
 */
void sccp_handle_SPCPTokenReq(sccp_session_t * s, sccp_device_t * no_d, sccp_moo_t * r)
{
	sccp_device_t *device;
	uint32_t deviceInstance = 0;
	uint32_t deviceType = 0;

	deviceInstance = letohl(r->msg.SPCPRegisterTokenRequest.sId.lel_instance);
	deviceType = letohl(r->msg.SPCPRegisterTokenRequest.lel_deviceType);

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_2 "%s: is requestin a token, Instance: %d, Type: %s (%d)\n", r->msg.SPCPRegisterTokenRequest.sId.deviceName, deviceInstance, devicetype2str(deviceType), deviceType);

	/* ip address range check */
	if (GLOB(ha) && !sccp_apply_ha(GLOB(ha), &s->sin)) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.SPCPRegisterTokenRequest.sId.deviceName);
		s = sccp_session_reject(s, "IP not authorized");
		return;
	}
	// Search for already known devices
	device = sccp_device_find_byid(r->msg.SPCPRegisterTokenRequest.sId.deviceName, TRUE);
	if (device) {
		if (device->session && device->session != s) {
			device->registrationState = SKINNY_DEVICE_RS_TIMEOUT;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Device is doing a re-registration!\n", device->id);
			device->session->session_stop = 1;							/* do not lock session, this will produce a deadlock, just stop the thread-> everything else will be done by thread it self */
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Previous Session for %s Closed!\n", device->id);
		}
	}
	device = device ? sccp_device_release(device) : NULL;
	
	// search for all devices including realtime
	device = sccp_device_find_byid(r->msg.SPCPRegisterTokenRequest.sId.deviceName, TRUE);
	if (!device && GLOB(allowAnonymous)) {
		device = sccp_device_createAnonymous(r->msg.SPCPRegisterTokenRequest.sId.deviceName);

		sccp_config_applyDeviceConfiguration(device, NULL);
		sccp_config_addButton(&device->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", r->msg.SPCPRegisterTokenRequest.sId.deviceName, GLOB(hotline)->line->name);
		device->defaultLineInstance = 1;
		sccp_device_addToGlobals(device);
	}

	/* no configuation for this device and no anonymous devices allowed */
	if (!device) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: not found\n", r->msg.SPCPRegisterTokenRequest.sId.deviceName);
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "Device not Accepted");
		return;
	}
	s->protocolType = SPCP_PROTOCOL;
	//      s->device = sccp_device_retain(device);                                 //retained in session

	s->device = sccp_session_addDevice(s, device);								// retained in session
	device->status.token = SCCP_TOKEN_STATE_REJ;
	device->skinny_type = deviceType;

	if (device->checkACL(device) == FALSE) {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", r->msg.SPCPRegisterTokenRequest.sId.deviceName, pbx_inet_ntoa(s->sin.sin_addr));
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "IP Not Authorized");
		device = sccp_device_release(device);
		return;
	}

	if (device->session && device->session != s) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Crossover device registration!\n", device->id);
		device->registrationState = SKINNY_DEVICE_RS_FAILED;
		sccp_session_tokenRejectSPCP(s, 60);
		s = sccp_session_reject(s, "Crossover session not allowed");
		device->session = sccp_session_reject(device->session, "Crossover session not allowed");
		device = sccp_device_release(device);
		return;
	}

	/* all checks passed, assign session to device */
	//      device->session = s;
	device->registrationState = SKINNY_DEVICE_RS_TOKEN;
	device->status.token = SCCP_TOKEN_STATE_ACK;

	sccp_session_tokenAckSPCP(s, 65535);
	device->registrationTime = time(0);									// last time device tried sending token
	device = sccp_device_release(device);
}

/*!
 * \brief Handle Device Registration
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \callgraph
 * \callergraph
 */
void sccp_handle_register(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint8_t protocolVer = letohl(r->msg.RegisterMessage.phone_features) & SKINNY_PHONE_FEATURES_PROTOCOLVERSION;
	uint8_t ourMaxSupportedProtocolVersion = sccp_protocol_getMaxSupportedVersionNumber(s->protocolType);
	uint32_t deviceInstance = 0;
	uint32_t deviceType = 0;

	deviceInstance = letohl(r->msg.RegisterMessage.sId.lel_instance);
	deviceType = letohl(r->msg.RegisterMessage.lel_deviceType);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: is registering, Instance: %d, Type: %s (%d), Version: %d (loadinfo '%s')\n", r->msg.RegisterMessage.sId.deviceName, deviceInstance, devicetype2str(deviceType), deviceType, protocolVer, r->msg.RegisterMessage.loadInfo);
#ifdef CS_EXPERIMENTAL_NEWIP
	socklen_t addrlen = 0;
	struct sockaddr_storage *session_ss = { 0 };

	struct sockaddr_storage *ss = { 0 };
	char iabuf[INET6_ADDRSTRLEN];
	struct sockaddr_in sin = { 0 };

	if (0 != r->msg.RegisterMessage.lel_stationIpAddr) {
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, &r->msg.RegisterMessage.lel_stationIpAddr, 4);
		inet_ntop(AF_INET, &sin.sin_addr, iabuf, (socklen_t) INET_ADDRSTRLEN);
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: IPv4-Address: %s\n", r->msg.RegisterMessage.sId.deviceName, iabuf);
		ss = (struct sockaddr_storage *) &sin;
	}
#ifdef CS_IPV6
	struct sockaddr_in6 sin6 = { 0 };

	if (0 != r->msg.RegisterMessage.ipv6Address) {
		sin6.sin6_family = AF_INET6;
		memcpy(&sin6.sin6_addr, &r->msg.RegisterMessage.lel_stationIpAddr, 16);
		inet_ntop(AF_INET6, &sin6.sin6_addr, iabuf, (socklen_t) INET6_ADDRSTRLEN);
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: IPv6-Address: %s\n", r->msg.RegisterMessage.sId.deviceName, iabuf);
		ss = (struct sockaddr_storage *) &sin6;
	}
#endif
#else
        s->phone_sin.sin_family = AF_INET;
        memcpy(&s->phone_sin.sin_addr, &r->msg.RegisterMessage.lel_stationIpAddr, 4);
#endif

	// search for all devices including realtime
	if (!d) {
		d = sccp_device_find_byid(r->msg.RegisterMessage.sId.deviceName, TRUE);
	} else {
		d = sccp_device_retain(d);
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: cached device configuration\n", d->id);
	}

	/*! \todo We need a fix here. If deviceName was provided and specified in sccp.conf we should not continue to anonymous, but we cannot depend on one of the standard find functions for this, because they return null in two different cases (non-existent and refcount<0). */

	if (!d && GLOB(allowAnonymous)) {
		if (!(d = sccp_device_createAnonymous(r->msg.RegisterMessage.sId.deviceName))) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline device could not be created: %s\n", r->msg.RegisterMessage.sId.deviceName, GLOB(hotline)->line->name);
		}
		sccp_config_applyDeviceConfiguration(d, NULL);
		sccp_config_addButton(&d->buttonconfig, 1, LINE, GLOB(hotline)->line->name, NULL, NULL);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", r->msg.RegisterMessage.sId.deviceName, GLOB(hotline)->line->name);
		d->defaultLineInstance = 1;
		sccp_device_addToGlobals(d);
	}

	if (d) {
		if (d->session && d->session != s) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s: Crossover device registration! Fixing up to new session\n", d->id);
			sccp_socket_stop_sessionthread(d->session, SKINNY_DEVICE_RS_FAILED);
			//                      d->session->device->registrationState = SKINNY_DEVICE_RS_FAILED;
			//                      s->device = sccp_session_addDevice(s, d);
		}

		if (!s->device || s->device != d) {
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", d->id, s->fds[0].fd, pbx_inet_ntoa(s->sin.sin_addr));
			s->device = sccp_session_addDevice(s, d);						// replace retained in session (already connected via tokenReq before)
		}

		/* check ACLs for this device */
		if (d->checkACL(d) == FALSE) {
			pbx_log(LOG_NOTICE, "%s: Rejecting device: Ip address '%s' denied (deny + permit/permithosts).\n", r->msg.RegisterMessage.sId.deviceName, pbx_inet_ntoa(s->sin.sin_addr));
			d->registrationState = SKINNY_DEVICE_RS_FAILED;
			s = sccp_session_reject(s, "IP Not Authorized");
			d = sccp_device_release(d);
			return;
		}

	} else {
		pbx_log(LOG_NOTICE, "%s: Rejecting device: Device Unknown \n", r->msg.RegisterMessage.sId.deviceName);
		s = sccp_session_reject(s, "Device Unknown");
		return;
	}

	d->device_features = letohl(r->msg.RegisterMessage.phone_features);
	d->linesRegistered = FALSE;

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: device load_info='%s', maxbuttons='%d', supports dynamic_messages='%s', supports abbr_dial='%s'\n", r->msg.RegisterMessage.sId.deviceName, r->msg.RegisterMessage.loadInfo, r->msg.RegisterMessage.lel_maxButtons, (d->device_features & SKINNY_PHONE_FEATURES_DYNAMIC_MESSAGES) == 0 ? "no" : "yes", (d->device_features & SKINNY_PHONE_FEATURES_ABBRDIAL) == 0 ? "no" : "yes");

	if (GLOB(localaddr) && sccp_apply_ha(GLOB(localaddr), &s->sin) != AST_SENSE_ALLOW) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device is behind NAT. We will set externip or externhost for the RTP stream (%s does not fit permit/deny)\n", r->msg.RegisterMessage.sId.deviceName, pbx_inet_ntoa(s->sin.sin_addr));
		d->nat = 1;
	}

	/* We should be using sockaddr_storage in sccp_socket.c so this convertion would not be necessary here */
#ifdef CS_EXPERIMENTAL_NEWIP
	if (AF_INET == s->sin.sin_family) {
		session_ss = (struct sockaddr_storage *) &s->sin;
		addrlen = (socklen_t) INET_ADDRSTRLEN;
	}
#ifdef CS_IPV6

	/*      if (AF_INET6==s->sin6.sin6_family) {
	   session_ss=(struct sockaddr_storage *)&s->sin6;
	   addrlen=(socklen_t)INET6_ADDRSTRLEN;
	   } */
#endif
	if (sockaddr_cmp_addr(ss, addrlen, session_ss, addrlen)) {						// test to see if phone ip address differs from incoming socket ipaddress -> nat
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Auto Detected NAT. We will use externip or externhost for the RTP stream\n", r->msg.RegisterMessage.sId.deviceName);
		d->nat = 1;
	}
#endif
	d->skinny_type = deviceType;

	//      d->session = s;
	s->lastKeepAlive = time(0);
	d->mwilight = 0;
	d->protocolversion = protocolVer;
	d->status.token = SCCP_TOKEN_STATE_NOTOKEN;

	/** workaround to fix the protocol version issue for ata devices */
	/*
	 * MAC-Address        : ATA00215504e821
	 * Protocol Version   : Supported '33', In Use '17'
	 */
	if (d->skinny_type == SKINNY_DEVICETYPE_ATA188 || d->skinny_type == SKINNY_DEVICETYPE_ATA186) {
		d->protocolversion = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
	}

	d->protocol = sccp_protocol_getDeviceProtocol(d, s->protocolType);
	uint8_t ourProtocolCapability = sccp_protocol_getMaxSupportedVersionNumber(s->protocolType);

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: asked our protocol capability (%d).\n", DEV_ID_LOG(d), ourProtocolCapability);

	/* we need some entropy for keepalive, to reduce the number of devices sending keepalive at one time */
	d->keepaliveinterval = d->keepalive ? d->keepalive : GLOB(keepalive);
	d->keepaliveinterval = ((d->keepaliveinterval / 4) * 3) + (rand() % (d->keepaliveinterval / 4)) + 1;	// smaller random segment, keeping keepalive toward the upperbound

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Phone protocol capability : %d\n", DEV_ID_LOG(d), protocolVer);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Our protocol capability	 : %d\n", DEV_ID_LOG(d), ourMaxSupportedProtocolVersion);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Joint protocol capability : %d\n", DEV_ID_LOG(d), d->protocol->version);

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Ask the phone to send keepalive message every %d seconds\n", d->id, d->keepaliveinterval);

	d->inuseprotocolversion = d->protocol->version;
	sccp_device_setIndicationProtocol(d);
	d->protocol->sendRegisterAck(d, d->keepaliveinterval, d->keepaliveinterval, GLOB(dateformat));

	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);

	/* 
	   Ask for the capabilities of the device
	   to proceed with registration according to sccp protocol specification 3.0 
	 */
	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: (sccp_handle_register) asking for capabilities\n", DEV_ID_LOG(d));
	sccp_dev_sendmsg(d, CapabilitiesReqMessage);
	d = sccp_device_release(d);
}

/*!
 * \brief Make Button Template for Device
 * \param d SCCP Device as sccp_device_t
 * \return Linked List of ButtonDefinitions
 *
 * \lock
 *      - device->buttonconfig
 *        - see sccp_line_find_byname()
 */
static btnlist *sccp_make_button_template(sccp_device_t * d)
{
	int i = 0;
	btnlist *btn;
	sccp_buttonconfig_t *buttonconfig;

	if (!d || !&d->buttonconfig)
		return NULL;

	if (!(btn = sccp_malloc(sizeof(btnlist) * StationMaxButtonTemplateSize))) {
		return NULL;
	}

	memset(btn, 0, sizeof(btnlist) * StationMaxButtonTemplateSize);
	sccp_dev_build_buttontemplate(d, btn);

	uint16_t speeddialInstance = 1;										/* starting instance for speeddial is 1 */
	uint16_t lineInstance = 1;
	uint16_t serviceInstance = 1;

	if (!d->isAnonymous) {
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for position for button type %d\n", DEV_ID_LOG(d), buttonconfig->type);
			if (buttonconfig->instance > 0)
				continue;

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
					} else {
						btn[i].type = SKINNY_BUTTONTYPE_UNDEFINED;
						buttonconfig->instance = btn[i].instance = 0;
						pbx_log(LOG_WARNING, "%s: line %s does not exists\n", DEV_ID_LOG(d), buttonconfig->button.line.name);
					}

					sccp_log((DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: add line %s on position %d\n", DEV_ID_LOG(d), buttonconfig->button.line.name, buttonconfig->instance);
					break;

				} else if (buttonconfig->type == EMPTY && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_LINE || btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL)) {

					btn[i].type = SKINNY_BUTTONTYPE_UNDEFINED;
					buttonconfig->instance = btn[i].instance = 0;
					break;

				} else if (buttonconfig->type == SERVICE && (btn[i].type == SCCP_BUTTONTYPE_MULTI)) {

					btn[i].type = SKINNY_BUTTONTYPE_SERVICEURL;
					buttonconfig->instance = btn[i].instance = serviceInstance++;
					break;

				} else if (buttonconfig->type == SPEEDDIAL && !sccp_strlen_zero(buttonconfig->label)
					   && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL)) {

					buttonconfig->instance = btn[i].instance = i + 1;
					if (!sccp_strlen_zero(buttonconfig->button.speeddial.hint)
					    && btn[i].type == SCCP_BUTTONTYPE_MULTI				/* we can set our feature */
					    ) {
#ifdef CS_DYNAMIC_SPEEDDIAL
						if (d->inuseprotocolversion >= 15) {
							btn[i].type = 0x15;
							buttonconfig->instance = btn[i].instance = speeddialInstance++;
						} else {
							btn[i].type = SKINNY_BUTTONTYPE_LINE;
							buttonconfig->instance = btn[i].instance = lineInstance++;;

						}
#else
						btn[i].type = SKINNY_BUTTONTYPE_LINE;
						buttonconfig->instance = btn[i].instance = lineInstance++;;
#endif
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

						case SCCP_FEATURE_TRANSFER:
							btn[i].type = SKINNY_BUTTONTYPE_TRANSFER;
							break;

						case SCCP_FEATURE_MONITOR:
							btn[i].type = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
							break;

						case SCCP_FEATURE_MULTIBLINK:
							btn[i].type = SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
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

						case SCCP_FEATURE_TEST6:
							btn[i].type = SKINNY_BUTTONTYPE_TEST6;
							break;

						case SCCP_FEATURE_TEST7:
							btn[i].type = SKINNY_BUTTONTYPE_TEST7;
							break;

						case SCCP_FEATURE_TEST8:
							btn[i].type = SKINNY_BUTTONTYPE_TEST8;
							break;

						case SCCP_FEATURE_TEST9:
							btn[i].type = SKINNY_BUTTONTYPE_TEST9;
							break;

						case SCCP_FEATURE_TESTA:
							btn[i].type = SKINNY_BUTTONTYPE_TESTA;
							break;

						case SCCP_FEATURE_TESTB:
							btn[i].type = SKINNY_BUTTONTYPE_TESTB;
							break;

						case SCCP_FEATURE_TESTC:
							btn[i].type = SKINNY_BUTTONTYPE_TESTC;
							break;

						case SCCP_FEATURE_TESTD:
							btn[i].type = SKINNY_BUTTONTYPE_TESTD;
							break;

						case SCCP_FEATURE_TESTE:
							btn[i].type = SKINNY_BUTTONTYPE_TESTE;
							break;

						case SCCP_FEATURE_TESTF:
							btn[i].type = SKINNY_BUTTONTYPE_TESTF;
							break;

						case SCCP_FEATURE_TESTG:
							btn[i].type = SKINNY_BUTTONTYPE_MESSAGES;
							break;

						case SCCP_FEATURE_TESTH:
							btn[i].type = SKINNY_BUTTONTYPE_DIRECTORY;
							break;

						case SCCP_FEATURE_TESTI:
							btn[i].type = SKINNY_BUTTONTYPE_TESTI;
							break;

						case SCCP_FEATURE_TESTJ:
							btn[i].type = SKINNY_BUTTONTYPE_APPLICATION;
							break;

						default:
							btn[i].type = SKINNY_BUTTONTYPE_FEATURE;
							break;

					}
					break;
				} else {
					continue;
				}
				sccp_log((DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %s (%s)\n", d->id, buttonconfig->instance, "FEATURE", buttonconfig->label);
			}

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
		btn[i].type = SKINNY_BUTTONTYPE_LINE;
		SCCP_LIST_FIRST(&d->buttonconfig)->instance = btn[i].instance = 1;
	}

	return btn;
}

/*!
 * \brief Handle Available Lines
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *      - device->buttonconfig
 *        - see sccp_line_addDevice()
 *        - see sccp_hint_lineStatusChanged()
 */
void sccp_handle_AvailableLines(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint8_t i = 0, line_count = 0;
	btnlist *btn;
	sccp_line_t *l = NULL;
	sccp_buttonconfig_t *buttonconfig = NULL;

	boolean_t defaultLineSet = FALSE;

	line_count = 0;

	/** \todo why do we get the message twice  */
	if (d->linesRegistered)
		return;

	btn = d->buttonTemplate;

	if (!btn) {
		sccp_log(DEBUGCAT_BUTTONTEMPLATE) (VERBOSE_PREFIX_3 "%s: no buttontemplate, reset device\n", DEV_ID_LOG(d));
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

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: Phone available lines %d\n", d->id, line_count);
	if (d->isAnonymous == TRUE) {
		l = GLOB(hotline)->line;
		sccp_line_addDevice(l, d, 1, NULL);
	} else {
		for (i = 0; i < StationMaxButtonTemplateSize; i++) {
			if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
				if ((l = sccp_line_retain(btn[i].ptr))) {
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Attaching line %s with instance %d to this device\n", d->id, l->name, btn[i].instance);

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
					l = sccp_line_release(l);
				}
			}

		}
	}
	d->linesRegistered = TRUE;
}

/*!
 * \brief Handle Accessory Status Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_accessorystatus_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint8_t id;
	uint8_t status;
	uint32_t unknown = 0;

	id = letohl(r->msg.AccessoryStatusMessage.lel_AccessoryID);
	status = letohl(r->msg.AccessoryStatusMessage.lel_AccessoryStatus);

	d->accessoryused = id;
	d->accessorystatus = status;
	unknown = letohl(r->msg.AccessoryStatusMessage.lel_unknown);
	switch (id) {
		case 1:
			d->accessoryStatus.headset = (status) ? TRUE : FALSE;
			break;
		case 2:
			d->accessoryStatus.handset = (status) ? TRUE : FALSE;
			break;
		case 3:
			d->accessoryStatus.speaker = (status) ? TRUE : FALSE;
			break;
	}

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s' (%u)\n", DEV_ID_LOG(d), accessory2str(d->accessoryused), accessorystatus2str(d->accessorystatus), unknown);
}

/*!
 * \brief Handle Device Unregister
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_unregister(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	/* we don't need to look for active channels. the phone does send unregister only when there are no channels */
	REQ(r1, UnregisterAckMessage);
	r1->msg.UnregisterAckMessage.lel_status = SKINNY_UNREGISTERSTATUS_OK;
	sccp_session_send2(s, r1);										// send directly to session, skipping device check
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: unregister request sent\n", DEV_ID_LOG(d));
	sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_NONE);
}

/*!
 * \brief Handle Button Template Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \warning
 *      - device->buttonconfig is not always locked
 * 
 * \lock
 *      - device
 *        - see sccp_make_button_template()
 *        - see sccp_dev_send()
 */
void sccp_handle_button_template_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	btnlist *btn;
	int i;
	uint8_t buttonCount = 0, lastUsedButtonPosition = 0;

	sccp_moo_t *r1;

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

	if (!btn) {
		pbx_log(LOG_ERROR, "%s: No memory allocated for button template\n", d->id);
		sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
		return;
	}

	REQ(r1, ButtonTemplateMessage);
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;

		if (SKINNY_BUTTONTYPE_UNUSED != btn[i].type) {		
			//r1->msg.ButtonTemplateMessage.lel_buttonCount = i+1;
		        buttonCount = i+1;
			lastUsedButtonPosition = i;
		}

		switch (btn[i].type) {
			case SCCP_BUTTONTYPE_HINT:
			case SCCP_BUTTONTYPE_LINE:
				/* we do not need a line if it is not configured */
				if (r1->msg.ButtonTemplateMessage.definition[i].instanceNumber == 0) {
					r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;
				} else {
					r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_LINE;
				}
				break;

			case SCCP_BUTTONTYPE_SPEEDDIAL:
				r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SPEEDDIAL;
				break;

			case SKINNY_BUTTONTYPE_SERVICEURL:
				r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SERVICEURL;
				break;

			case SKINNY_BUTTONTYPE_FEATURE:
				r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_FEATURE;
				break;

			case SCCP_BUTTONTYPE_MULTI:
				//r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_DISPLAY;
				//break;

			case SKINNY_BUTTONTYPE_UNUSED:
				r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;

				break;

			default:
				r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = btn[i].type;
				break;
		}
		sccp_log((DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %d (%d)\n", d->id, i + 1, r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition, r1->msg.ButtonTemplateMessage.definition[i].instanceNumber);

	}

	r1->msg.ButtonTemplateMessage.lel_buttonOffset = 0;
	//r1->msg.ButtonTemplateMessage.lel_buttonCount = htolel(r1->msg.ButtonTemplateMessage.lel_buttonCount);
	r1->msg.ButtonTemplateMessage.lel_buttonCount = htolel(buttonCount);
	/* buttonCount is already in a little endian format so don't need to convert it now */
	r1->msg.ButtonTemplateMessage.lel_totalButtonCount = htolel(lastUsedButtonPosition+1);

	/* set speeddial for older devices like 7912 */
	uint32_t speeddialInstance = 0;
	sccp_buttonconfig_t *config;

	sccp_log((DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_SPEEDDIAL)) (VERBOSE_PREFIX_3 "%s: configure unconfigured speeddialbuttons \n", d->id);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		/* we found an unconfigured speeddial */
		if (config->type == SPEEDDIAL && config->instance == 0) {
			config->instance = speeddialInstance++;
		} else if (config->type == SPEEDDIAL && config->instance != 0) {
			speeddialInstance = config->instance;
		}
	}
	/* done */

	sccp_dev_send(d, r1);
}

/*!
 * \brief Handle Line Number for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - device->buttonconfig
 */
void sccp_handle_line_number(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_line_t *l = NULL;
	sccp_moo_t *r1;
	sccp_speed_t k;
	sccp_buttonconfig_t *config;

	uint8_t lineNumber = letohl(r->msg.LineStatReqMessage.lel_lineNumber);

	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: Configuring line number %d\n", d->id, lineNumber);
	l = sccp_line_find_byid(d, lineNumber);

	/* if we find no regular line - it can be a speeddial with hint */
	if (!l)
		sccp_dev_speed_find_byindex(d, lineNumber, SCCP_BUTTONTYPE_HINT, &k);

	REQ(r1, LineStatMessage);
	if (!l && !k.valid) {
		pbx_log(LOG_ERROR, "%s: requested a line configuration for unknown line/speeddial %d\n", DEV_ID_LOG(s->device), lineNumber);
		r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);
		sccp_dev_send(s->device, r1);
		return;
	}
	r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);

	sccp_copy_string(r1->msg.LineStatMessage.lineDirNumber, ((l) ? l->name : k.name), sizeof(r1->msg.LineStatMessage.lineDirNumber));

	/* lets set the device description for the first line, so it will be display on top of device -MC */
	if (lineNumber == 1) {
		sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, (d->description), sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	} else {
		sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, ((l) ? l->description : k.name), sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	}

	sccp_copy_string(r1->msg.LineStatMessage.lineDisplayName, ((l) ? l->label : k.name), sizeof(r1->msg.LineStatMessage.lineDisplayName));

	sccp_dev_send(d, r1);

	/* force the forward status message. Some phone does not request it registering */
	if (l) {
		sccp_dev_forward_status(l, lineNumber, d);

		/* set default line on device if based on "default" config option */
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->instance == lineNumber) {
				if (config->type == LINE) {
					if (config->button.line.options && strcasestr(config->button.line.options, "default")) {
						d->defaultLineInstance = lineNumber;
						sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", lineNumber);
					}
				}
				break;
			}
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
		l = sccp_line_release(l);
	}
}

/*!
 * \brief Handle SpeedDial Status Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_speed_t k;
	sccp_moo_t *r1;

	int wanted = letohl(r->msg.SpeedDialStatReqMessage.lel_speedDialNumber);

	sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Speed Dial Request for Button %d\n", DEV_ID_LOG(s->device), wanted);

	REQ(r1, SpeedDialStatMessage);
	r1->msg.SpeedDialStatMessage.lel_speedDialNumber = htolel(wanted);

	sccp_dev_speed_find_byindex(s->device, wanted, SCCP_BUTTONTYPE_SPEEDDIAL, &k);
	if (k.valid) {
		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDirNumber, k.ext, sizeof(r1->msg.SpeedDialStatMessage.speedDialDirNumber));
		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDisplayName, k.name, sizeof(r1->msg.SpeedDialStatMessage.speedDialDisplayName));
	} else {
		sccp_log((DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: speeddial %d unknown\n", DEV_ID_LOG(s->device), wanted);
	}

	sccp_dev_send(d, r1);
}

/*!
 * \brief Handle Stimulus for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *      - channel
 *      - device
 *        - see sccp_device_find_index_for_line()
 */
void sccp_handle_stimulus(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_line_t *l = NULL;
	sccp_line_t *al = NULL;
	sccp_speed_t k;
	sccp_channel_t *channel = NULL, *sccp_channel_1 = NULL;
	uint8_t stimulus;
	uint8_t instance;
	sccp_channel_t *sccp_channel_held;
	sccp_channel_t *sccp_channel_ringing;

	stimulus = letohl(r->msg.StimulusMessage.lel_stimulus);
	instance = letohl(r->msg.StimulusMessage.lel_stimulusInstance);

	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);							/* use adhoc dial feture with hotline */
		return;
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Got stimulus=%s (%d) for instance=%d\n", d->id, stimulus2str(stimulus), stimulus, instance);

	if (!instance) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Instance 0 is not a valid instance. Trying the active line %d\n", d->id, instance);
		al = sccp_dev_get_activeline(d);
		if (!al) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line found\n", d->id);
			goto func_exit;
		}
		if (strlen(al->adhocNumber) > 0) {
			sccp_feat_adhocDial(d, l);
			goto func_exit;
		}
		// \todo set index
		instance = 1;
	}

	switch (stimulus) {

		case SKINNY_BUTTONTYPE_LASTNUMBERREDIAL:							// We got a Redial Request
			if (sccp_strlen_zero(d->lastNumber))
				goto func_exit;
			channel = sccp_channel_get_active(d);
			if (channel) {
				if (channel->state == SCCP_CHANNELSTATE_OFFHOOK) {
					sccp_copy_string(channel->dialedNumber, d->lastNumber, sizeof(d->lastNumber));
					channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);
					sccp_pbx_softswitch(channel);

					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Redial the number %s\n", d->id, d->lastNumber);
				} else {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Redial ignored as call in progress\n", d->id);
				}
			} else {
				if ((l = sccp_dev_get_activeline(d))) {
					channel = sccp_channel_newcall(l, d, d->lastNumber, SKINNY_CALLTYPE_OUTBOUND, NULL);
					channel = channel ? sccp_channel_release(channel) : NULL;
				}
			}
			break;

		case SKINNY_BUTTONTYPE_LINE:									// We got a Line Request
			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No line for instance %d. Looking for a speeddial with hint\n", d->id, instance);
				sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT, &k);
				if (k.valid)
					sccp_handle_speeddial(d, &k);
				else
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
				goto func_exit;
			}

			if (strlen(l->adhocNumber) > 0) {
				sccp_feat_adhocDial(d, l);
				goto func_exit;
			}

			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Line Key press on line %s\n", d->id, (l) ? l->name : "(nil)");
			if ((channel = sccp_channel_get_active(d))) {
				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: gotten active channel %d on line %s\n", d->id, channel->callid, (l) ? l->name : "(nil)");
				if (channel->state != SCCP_CHANNELSTATE_CONNECTED) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call not in progress. Closing line %s\n", d->id, (l) ? l->name : "(nil)");
					sccp_channel_endcall(channel);
					sccp_dev_deactivate_cplane(d);
					goto func_exit;
				} else {
					if (sccp_channel_hold(channel)) {
						sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: call (%d) put on hold on line %s\n", d->id, channel->callid, l->name);
					} else {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold failed for call (%d), line %s\n", d->id, channel->callid, l->name);
					}
				}
			} else {
				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: no activate channel on line %d\n", d->id, instance);
			}
			if (!SCCP_RWLIST_GETSIZE(l->channels)) {
				sccp_channel_t *tmpChannel = NULL;

				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: no activate channel on line %s\n", DEV_ID_LOG(d), (l) ? l->name : "(nil)");
				sccp_dev_set_activeline(d, l);
				sccp_dev_set_cplane(l, instance, d, 1);
				tmpChannel = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL);
				tmpChannel = tmpChannel ? sccp_channel_release(tmpChannel) : NULL;

			} else if ((sccp_channel_ringing = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGING))) {
				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: Answering incoming/ringing line %d", d->id, instance);
				sccp_channel_answer(d, sccp_channel_ringing);
			} else if ((sccp_channel_held = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))) {
				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: Channel count on line %d = %d", d->id, instance, SCCP_RWLIST_GETSIZE(l->channels));
				if (SCCP_RWLIST_GETSIZE(l->channels) == 1) {
					/* \todo we should  lock the list here. */
					channel = SCCP_LIST_FIRST(&l->channels);
					sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: Resume channel %d on line %d", d->id, channel->callid, instance);
					sccp_dev_set_activeline(d, l);
					sccp_channel_resume(d, channel, TRUE);
					sccp_dev_set_cplane(l, instance, d, 1);
				} else {
					sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: Switch to line %d", d->id, instance);
					sccp_dev_set_activeline(d, l);
					sccp_dev_set_cplane(l, instance, d, 1);
				}
			}
			break;

		case SKINNY_BUTTONTYPE_SPEEDDIAL:
			sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_SPEEDDIAL, &k);
			if (k.valid)
				sccp_handle_speeddial(d, &k);
			else
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
			break;

		case SKINNY_BUTTONTYPE_HOLD:
			/* this is the hard hold button. When we are here we are putting on hold the active_channel */
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold/Resume Button pressed on line (%d)\n", d->id, instance);
			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				l = sccp_dev_get_activeline(d);
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Trying the current line\n", d->id);
				if (!l) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					goto func_exit;
				}
				instance = sccp_device_find_index_for_line(d, l->name);
			}

			if ((channel = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED))) {
				sccp_channel_hold(channel);
			} else if ((channel = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))) {
				sccp_channel_1 = sccp_channel_get_active(d);
				if (sccp_channel_1) {
					if (sccp_channel_1->state == SCCP_CHANNELSTATE_OFFHOOK)
						sccp_channel_endcall(sccp_channel_1);
				}
				sccp_channel_resume(d, channel, TRUE);
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No call to resume/hold found on line %d\n", d->id, instance);
			}
			break;

		case SKINNY_BUTTONTYPE_TRANSFER:
			if (!d->transfer) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device\n", d->id);
				break;
			}
			channel = sccp_channel_get_active(d);
			if (channel) {
				sccp_channel_transfer(channel, d);
			}
			break;

		case SKINNY_BUTTONTYPE_VOICEMAIL:								// Get a new Line and Dial the Voicemail.
			sccp_feat_voicemail(d, instance);
			break;
		case SKINNY_BUTTONTYPE_CONFERENCE:
			pbx_log(LOG_NOTICE, "%s: Conference Button is not yet handled. working on implementation\n", d->id);
			break;

		case SKINNY_BUTTONTYPE_FEATURE:
		case SKINNY_BUTTONTYPE_MOBILITY:
		case SKINNY_BUTTONTYPE_MULTIBLINKFEATURE:
		case SKINNY_BUTTONTYPE_TEST6:
		case SKINNY_BUTTONTYPE_TEST7:
		case SKINNY_BUTTONTYPE_TEST8:
		case SKINNY_BUTTONTYPE_TEST9:
		case SKINNY_BUTTONTYPE_TESTA:
		case SKINNY_BUTTONTYPE_TESTB:
		case SKINNY_BUTTONTYPE_TESTC:
		case SKINNY_BUTTONTYPE_TESTD:
		case SKINNY_BUTTONTYPE_TESTE:
		case SKINNY_BUTTONTYPE_TESTF:
		case SKINNY_BUTTONTYPE_MESSAGES:
		case SKINNY_BUTTONTYPE_DIRECTORY:
		case SKINNY_BUTTONTYPE_TESTI:
		case SKINNY_BUTTONTYPE_APPLICATION:
			sccp_handle_feature_action(d, instance, TRUE);
			break;

		case SKINNY_BUTTONTYPE_FORWARDALL:								// Call forward all
			if (!d->cfwdall) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDALL disabled on device\n", d->id);
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
				goto func_exit;
			}
			l = sccp_dev_get_activeline(d);
			if (!l) {
				if (!instance)
					instance = 1;

				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					goto func_exit;
				}
			}
			if (l) {
				sccp_feat_handle_callforward(l, d, SCCP_CFWD_ALL);
			}
			break;
		case SKINNY_BUTTONTYPE_FORWARDBUSY:
			if (!d->cfwdbusy) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY disabled on device\n", d->id);
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
				goto func_exit;
			}
			l = sccp_dev_get_activeline(d);
			if (!l) {
				if (!instance)
					instance = 1;

				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					goto func_exit;
				}
			}
			if (l) {
				sccp_feat_handle_callforward(l, d, SCCP_CFWD_BUSY);
			}
			break;
		case SKINNY_BUTTONTYPE_FORWARDNOANSWER:
			if (!d->cfwdnoanswer) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER disabled on device\n", d->id);
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
				goto func_exit;
			}
			l = sccp_dev_get_activeline(d);
			if (!l) {
				if (!instance)
					instance = 1;

				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					goto func_exit;
				}
			}
			if (l) {
				sccp_feat_handle_callforward(l, d, SCCP_CFWD_NOANSWER);
			}
			break;
		case SKINNY_BUTTONTYPE_CALLPARK:								// Call parking
#ifdef CS_SCCP_PARK
			channel = sccp_channel_get_active(d);
			if (!channel) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Cannot park while no calls in progress\n", d->id);
				goto func_exit;
			}
			sccp_channel_park(channel);
#else
			sccp_log((DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
			break;

		case SKINNY_BUTTONTYPE_BLFSPEEDDIAL:								//busy lamp field type speeddial
			sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT, &k);
			if (k.valid)
				sccp_handle_speeddial(d, &k);
			else
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
			break;

		case SKINNY_STIMULUS_GROUPCALLPICKUP:								/*!< pickup feature button */
#ifndef CS_SCCP_PICKUP
			sccp_log((DEBUGCAT_FEATURE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "### Native GROUP PICKUP was not compiled in\n");
#else
			if (d->defaultLineInstance > 0) {
				sccp_log((DEBUGCAT_FEATURE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "using default line with instance: %u\n", d->defaultLineInstance);
				/*! \todo use feature map or sccp_feat_handle_directed_pickup */
				if ((l = sccp_line_find_byid(d, d->defaultLineInstance))) {
					//sccp_feat_handle_directed_pickup(l, d->defaultLineInstance, d);
					channel = sccp_channel_newcall(l, d, (char *) pbx_pickup_ext(), SKINNY_CALLTYPE_OUTBOUND, NULL);
					channel = channel ? sccp_channel_release(channel) : NULL;
				}
				goto func_exit;
			}

			/* no default line set, use first line */
			if (!al) {
				al = sccp_line_find_byid(d, 1);
			}
			if (al) {
				/*! \todo use feature map or sccp_feat_handle_directed_pickup */
				//sccp_feat_handle_directed_pickup(l, 1, d);
				channel = sccp_channel_newcall(al, d, (char *) pbx_pickup_ext(), SKINNY_CALLTYPE_OUTBOUND, NULL);
				channel = channel ? sccp_channel_release(channel) : NULL;
			}
#endif
			break;

		default:
			pbx_log(LOG_NOTICE, "%s: Don't know how to deal with stimulus %d with Phonetype %s(%d) \n", d->id, stimulus, devicetype2str(d->skinny_type), d->skinny_type);
			break;
	}

func_exit:
	channel = channel ? sccp_channel_release(channel) : NULL;
	al = al ? sccp_line_release(al) : NULL;
	l = l ? sccp_line_release(l) : NULL;
}

/*!
 * \brief Handle SpeedDial for Device
 * \param d SCCP Device as sccp_device_t
 * \param k SCCP SpeedDial as sccp_speed_t
 *
 * \lock
 *      - channel
 */
void sccp_handle_speeddial(sccp_device_t * d, const sccp_speed_t * k)
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l;
	int len;

	if (!k || !d || !d->session)
		return;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Speeddial Button (%d) pressed, configured number is (%s)\n", d->id, k->instance, k->ext);
	if ((channel = sccp_channel_get_active(d))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: channel state %d\n", DEV_ID_LOG(d), channel->state);

		// Channel already in use
		if ((channel->state == SCCP_CHANNELSTATE_DIALING) || (channel->state == SCCP_CHANNELSTATE_OFFHOOK)) {
			len = strlen(channel->dialedNumber);
			sccp_copy_string(channel->dialedNumber + len, k->ext, sizeof(channel->dialedNumber) - len);
			channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);
			sccp_pbx_softswitch(channel);
			channel = sccp_channel_release(channel);
			return;
		} else if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_PROCEED) {
			// automatically put on hold
			sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_3 "%s: automatically put call %d on hold %d\n", DEV_ID_LOG(d), channel->callid, channel->state);
			sccp_channel_hold(channel);

			if ((l = sccp_dev_get_activeline(d))) {

				sccp_channel_t *tmpChannel = NULL;

				tmpChannel = sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND, NULL);
				tmpChannel = tmpChannel ? sccp_channel_release(tmpChannel) : NULL;

				l = sccp_line_release(l);
			}
			channel = sccp_channel_release(channel);
			return;
		}
		// Channel not in use
		sccp_pbx_senddigits(channel, k->ext);
		channel = sccp_channel_release(channel);
	} else {
		/* check Remote RINGING + gpickup */
		if (d->defaultLineInstance > 0) {
			sccp_log((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = sccp_dev_get_activeline(d);
		}
		if (l) {
			channel = sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND, NULL);
			channel = channel ? sccp_channel_release(channel) : NULL;
			l = sccp_line_release(l);
		}
	}
}

/*!
 * \brief Handle Off Hook Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_offhook(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_line_t *l;
	sccp_channel_t *channel;

	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);
		return;
	}

	if ((channel = sccp_channel_get_active(d))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Taken Offhook with a call (%d) in progess. Skip it!\n", d->id, channel->callid);
		channel = sccp_channel_release(channel);
		return;
	}

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_OFFHOOK;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Taken Offhook\n", d->id);

	/* checking for registered lines */
	if (!d->configurationStatistic.numberOfLines) {
		pbx_log(LOG_NOTICE, "No lines registered on %s for take OffHook\n", DEV_ID_LOG(s->device));
		sccp_dev_displayprompt(d, 0, 0, "No lines registered!", 0);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}
	/* end line check */

	if ((channel = sccp_channel_find_bystate_on_device(d, SKINNY_CALLSTATE_RINGIN))) {
		/* Answer the ringing channel. */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer channel\n", d->id);
		sccp_channel_answer(d, channel);
	} else {
		/* use default line if it is set */
		if (d && d->defaultLineInstance > 0) {
			sccp_log((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = sccp_dev_get_activeline(d);
		}
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Using line %s\n", d->id, l->name);

		if (l) {
			if (!sccp_strlen_zero(l->adhocNumber)) {
				channel = sccp_channel_newcall(l, d, l->adhocNumber, SKINNY_CALLTYPE_OUTBOUND, NULL);
			} else {
				/* make a new call with no number */
				channel = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL);
			}
			l = sccp_line_release(l);
		}
	}
	channel = channel ? sccp_channel_release(channel) : NULL;
}

/*!
 * \brief Handle BackSpace Event for Device
 * \param d SCCP Device as sccp_device_t
 * \param line Line Number as uint8_t
 * \param callid Call ID as uint32_t
 */
void sccp_handle_backspace(sccp_device_t * d, uint8_t line, uint32_t callid)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;
	REQ(r, BackSpaceReqMessage);
	r->msg.BackSpaceReqMessage.lel_lineInstance = htolel(line);
	r->msg.BackSpaceReqMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Backspace request on line instance %u, call %u.\n", d->id, line, callid);
}

/*!
 * \brief Handle On Hook Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 */
void sccp_handle_onhook(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_channel_t *channel;
	sccp_buttonconfig_t *buttonconfig = NULL;

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_ONHOOK;
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: is Onhook\n", DEV_ID_LOG(d));

	/* checking for registered lines */
	uint8_t numberOfLines = 0;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE)
			numberOfLines++;
	}
	if (!numberOfLines) {
		pbx_log(LOG_NOTICE, "No lines registered on %s to put OnHook\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, 0, 0, "No lines registered!", 0);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}

	/* get the active channel */
	if ((channel = sccp_channel_get_active(d))) {
		sccp_channel_endcall(channel);
		channel = sccp_channel_release(channel);
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
 * \param r SCCP Moo
 * \note this is used just in protocol v3 stuff, it has been included in 0x004A AccessoryStatusMessage
 */
void sccp_handle_headset(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	/*
	 * this is used just in protocol v3 stuff
	 * it has been included in 0x004A AccessoryStatusMessage
	 */

	uint32_t headsetmode = letohl(r->msg.HeadsetStatusMessage.lel_hsMode);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s' (%u)\n", DEV_ID_LOG(s->device), accessory2str(SCCP_ACCESSORY_HEADSET), accessorystatus2str(headsetmode), 0);
}

/*!
 * \brief Handle Capabilities for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 * 
 */
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	int i;
	skinny_codec_t codec;

	if (!d) {
		return;
	}

	uint8_t n = letohl(r->msg.CapabilitiesResMessage.lel_count);

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Capabilities\n", DEV_ID_LOG(d), n);
	for (i = 0; i < n; i++) {
		codec = letohl(r->msg.CapabilitiesResMessage.caps[i].lel_payloadCapability);
		d->capabilities.audio[i] = codec;
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6d %-25s\n", d->id, codec, codec2str(codec));
	}

	//      if((d->preferences.audio[0] == SKINNY_CODEC_NONE)){     // prevent assignment by reversing order
	if ((SKINNY_CODEC_NONE == d->preferences.audio[0])) {
		/* we have no preferred codec, use capabilities -MC */
		memcpy(&d->preferences.audio, &d->capabilities.audio, sizeof(d->preferences.audio));
	}

	char cap_buf[512];

	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_1 "%s: num of codecs %d, capabilities: %s\n", DEV_ID_LOG(d), (int) ARRAY_LEN(d->capabilities.audio), cap_buf);
}

/*!
 * \brief Handle Soft Key Template Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - device
 */
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint8_t i;
	sccp_moo_t *r1;

	/* ok the device support the softkey map */
	d->softkeysupport = 1;

	int arrayLen = ARRAY_LEN(softkeysmap);
	int dummy_len = arrayLen * (sizeof(StationSoftKeyDefinition));
	int hdr_len = sizeof(r->msg.SoftKeyTemplateResMessage);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	/* create message */
	r1 = sccp_build_packet(SoftKeyTemplateResMessage, hdr_len + dummy_len + padding);
	r1->msg.SoftKeyTemplateResMessage.lel_softKeyOffset = 0;

	for (i = 0; i < arrayLen; i++) {
		switch (softkeysmap[i]) {
			case SKINNY_LBL_EMPTY:
				//                              r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 0;
				//                              r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = 0;
			case SKINNY_LBL_DIAL:
				sccp_copy_string(r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel, label2str(softkeysmap[i]), StationMaxSoftKeyLabelSize);
				sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel);
				break;
#ifdef CS_SCCP_CONFERENCE
			case SKINNY_LBL_CONFRN:
			case SKINNY_LBL_JOIN:
			case SKINNY_LBL_CONFLIST:
				if (d->allow_conference) {
					r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;
					r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
				}
				break;
#endif
			default:
				r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;
				r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
				sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, label2str(r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1]));
		}
		r1->msg.SoftKeyTemplateResMessage.definition[i].lel_softKeyEvent = htolel(i + 1);
	}

	r1->msg.SoftKeyTemplateResMessage.lel_softKeyCount = htolel(arrayLen);
	r1->msg.SoftKeyTemplateResMessage.lel_totalSoftKeyCount = htolel(arrayLen);
	sccp_dev_send(s->device, r1);
}

/*!
 * \brief Handle Set Soft Key Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 *
 * \lock
 *      - softKeySetConfig
 */
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{

	int iKeySetCount = 0;
	sccp_moo_t *r1;
	uint8_t i = 0;
	sccp_line_t *l;
	uint8_t trnsfvm = 0;
	uint8_t meetme = 0;

#ifdef CS_SCCP_PICKUP
	uint8_t pickupgroup = 0;
#endif

	/* set softkey definition */
	sccp_softKeySetConfiguration_t *softkeyset;

	if (!sccp_strlen_zero(d->softkeyDefinition)) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: searching for softkeyset: %s!\n", d->id, d->softkeyDefinition);
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
			if (!strcasecmp(d->softkeyDefinition, softkeyset->name)) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: using softkeyset: %s!\n", d->id, softkeyset->name);
				d->softKeyConfiguration.modes = softkeyset->modes;
				d->softKeyConfiguration.size = softkeyset->numberOfSoftKeySets;
			}
		}
		SCCP_LIST_UNLOCK(&softKeySetConfig);
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: d->softkeyDefinition=%s!\n", d->id, d->softkeyDefinition);
	} else {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: d->softkeyDefinition is empty\n", d->id);
	}
	/* end softkey definition */

	const softkey_modes *v = d->softKeyConfiguration.modes;
	const uint8_t v_count = d->softKeyConfiguration.size;

	REQ(r1, SoftKeySetResMessage);
	r1->msg.SoftKeySetResMessage.lel_softKeySetOffset = htolel(0);

	/* look for line trnsvm */
	sccp_buttonconfig_t *buttonconfig;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			l = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);
			if (l) {
				if (!sccp_strlen_zero(l->trnsfvm))
					trnsfvm = 1;

				if (l->meetme)
					meetme = 1;

				if (!sccp_strlen_zero(l->meetmenum))
					meetme = 1;

#ifdef CS_SCCP_PICKUP
				if (l->pickupgroup)
					pickupgroup = 1;
#endif
				l = sccp_line_release(l);
			}
		}
	}

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: softkey count: %d\n", d->id, v_count);

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRANSFER        is %s\n", d->id, (d->transfer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: DND             is %s\n", d->id, d->dndFeature.status ? dndmode2str(d->dndFeature.status) : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PRIVATE         is %s\n", d->id, d->privacyFeature.enabled ? "enabled" : "disabled");
#ifdef CS_SCCP_PARK
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PARK            is  %s\n", d->id, (d->park) ? "enabled" : "disabled");
#endif
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDALL         is  %s\n", d->id, (d->cfwdall) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY        is  %s\n", d->id, (d->cfwdbusy) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER    is  %s\n", d->id, (d->cfwdnoanswer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRNSFVM/IDIVERT is  %s\n", d->id, (trnsfvm) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: MEETME          is  %s\n", d->id, (meetme) ? "enabled" : "disabled");
#ifdef CS_SCCP_PICKUP
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPGROUP     is  %s\n", d->id, (pickupgroup) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPEXTEN     is  %s\n", d->id, (d->directed_pickup) ? "enabled" : "disabled");
#endif
	for (i = 0; i < v_count; i++) {
		const uint8_t *b = v->ptr;
		uint8_t c, j, cp = 0;

		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Set[%-2d]= ", d->id, v->id);

		for (c = 0, cp = 0; c < v->count; c++, cp++) {
			r1->msg.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[cp] = 0;
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
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) ("%-2d:%-10s ", c, label2str(softkeysmap[j]));
					r1->msg.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[cp] = (j + 1);
					break;
				}
			}

		}

		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) ("\n");
		v++;
		iKeySetCount++;
	};
	
	/* disable videomode and join softkey for all softkeysets */
	for (i = 0; i < KEYMODE_ONHOOKSTEALABLE; i++) {
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_JOIN, FALSE);
	}

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "There are %d SoftKeySets.\n", iKeySetCount);

	r1->msg.SoftKeySetResMessage.lel_softKeySetCount = htolel(iKeySetCount);
	r1->msg.SoftKeySetResMessage.lel_totalSoftKeySetCount = htolel(iKeySetCount);				// <<-- for now, but should be: iTotalKeySetCount;

	sccp_dev_send(d, r1);
	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK);
}

/*!
 * \brief Handle Dialed PhoneBook Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_dialedphonebook_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	/* this is from CCM7 dump */
	sccp_moo_t *r1 = NULL;

	//      sccp_BFLState_t state;
	uint32_t state;
	sccp_line_t *line;

	uint32_t unknown1 = 0;											/* just 4 bits filled */
	uint32_t index = 0;											/* just 28 bits used */
	uint32_t unknown2 = 0;											/* all 32 bits used */
	uint32_t lineInstance = 0;										/* */
	char *number;

	index = letohl(r->msg.DialedPhoneBookMessage.lel_NumberIndex);
	unknown1 = (index | 0xFFFFFFF0) ^ 0xFFFFFFF0;
	index = index >> 4;

	unknown2 = letohl(r->msg.DialedPhoneBookMessage.lel_unknown);						// i don't understand this :)
	lineInstance = letohl(r->msg.DialedPhoneBookMessage.lel_lineinstance);

	number = r->msg.DialedPhoneBookMessage.phonenumber;

	// Sending 0x152 Ack Message.
	REQ(r1, DialedPhoneBookAckMessage);
	r1->msg.DialedPhoneBookAckMessage.lel_NumberIndex = r->msg.DialedPhoneBookMessage.lel_NumberIndex;
	r1->msg.DialedPhoneBookAckMessage.lel_lineinstance = r->msg.DialedPhoneBookMessage.lel_lineinstance;
	r1->msg.DialedPhoneBookAckMessage.lel_unknown = r->msg.DialedPhoneBookMessage.lel_unknown;
	r1->msg.DialedPhoneBookAckMessage.lel_unknown2 = 0;
	sccp_dev_send(d, r1);

	/* sometimes a phone sends an ' ' entry, I think we can ignore this one */
	if (strlen(r->msg.DialedPhoneBookMessage.phonenumber) <= 1) {
		return;
	}

	line = sccp_line_find_byid(d, lineInstance);
	if (!line) {
		return;
	}

	REQ(r1, CallListStateUpdate);
	state = PBX(getExtensionState) (number, line->context);

	r1->msg.CallListStateUpdate.lel_NumberIndex = htolel(r->msg.DialedPhoneBookMessage.lel_NumberIndex);
	r1->msg.CallListStateUpdate.lel_lineinstance = htolel(r->msg.DialedPhoneBookMessage.lel_lineinstance);
	r1->msg.CallListStateUpdate.lel_state = htolel(state);
	sccp_dev_send(d, r1);
	sccp_log((DEBUGCAT_HINT | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: send CallListStateUpdate for extension '%s', context '%s', state %d\n", DEV_ID_LOG(d), number, line->context, state);
	sccp_log((DEBUGCAT_HINT | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Device sent Dialed PhoneBook Rec.'%u' (%u) dn '%s' (0x%08X) line instance '%d'.\n", DEV_ID_LOG(d), index, unknown1, r->msg.DialedPhoneBookMessage.phonenumber, unknown2, lineInstance);
	
	line = sccp_line_release(line);
}

/*!
 * \brief Handle Time/Date Request Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_time_date_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	time_t timer = 0;
	struct tm *cmtime = NULL;
	char servername[StationMaxDisplayNotifySize];
	sccp_moo_t *r1;

	if (!s)
		return;

	REQ(r1, DefineTimeDate);

	/* modulate the timezone by full hours only */
	timer = time(0) + (d->tz_offset * 3600);
	cmtime = localtime(&timer);
	r1->msg.DefineTimeDate.lel_year = htolel(cmtime->tm_year + 1900);
	r1->msg.DefineTimeDate.lel_month = htolel(cmtime->tm_mon + 1);
	r1->msg.DefineTimeDate.lel_dayOfWeek = htolel(cmtime->tm_wday);
	r1->msg.DefineTimeDate.lel_day = htolel(cmtime->tm_mday);
	r1->msg.DefineTimeDate.lel_hour = htolel(cmtime->tm_hour);
	r1->msg.DefineTimeDate.lel_minute = htolel(cmtime->tm_min);
	r1->msg.DefineTimeDate.lel_seconds = htolel(cmtime->tm_sec);
	r1->msg.DefineTimeDate.lel_milliseconds = htolel(0);
	r1->msg.DefineTimeDate.lel_systemTime = htolel(timer);
	sccp_dev_send(d, r1);
	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Send date/time\n", DEV_ID_LOG(d));

	/*
	   According to SCCP protocol since version 3,
	   the first instance of asking for time and date
	   concludes the device registration process.
	   This is included even in the minimal subset of device registration commands.
	 */
	if (d->registrationState == SKINNY_DEVICE_RS_PROGRESS) {
		sccp_dev_set_registered(s->device, SKINNY_DEVICE_RS_OK);
		snprintf(servername, sizeof(servername), "%s %s", GLOB(servername), SKINNY_DISP_CONNECTED);
		sccp_dev_displaynotify(d, servername, 5);
	}
}

/*!
 * \brief Handle KeyPad Button for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - channel
 *        - see sccp_device_find_index_for_line()
 *        - see sccp_dev_displayprompt()
 *        - see sccp_handle_dialtone()
 */
void sccp_handle_keypad_button(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	int digit;
	uint8_t lineInstance;
	uint32_t callid;
	char resp = '\0';
	int len = 0;

	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;

	digit = letohl(r->msg.KeypadButtonMessage.lel_kpButton);
	lineInstance = letohl(r->msg.KeypadButtonMessage.lel_lineInstance);
	callid = letohl(r->msg.KeypadButtonMessage.lel_callReference);

	if (!d) {												// should never be possible, d should have been retained in calling function
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but device is not specified! Exiting\n", DEV_ID_LOG(s->device));
	}

	if (lineInstance) {
		l = sccp_line_find_byid(d, lineInstance);
	}
	if (callid) {
		if (d->active_channel && d->active_channel->callid == callid) {
			channel = sccp_channel_get_active(d);
			if (l && channel->line != l) {
				l = sccp_line_release(l);
			}
			if (!l) {
				l = channel ? sccp_line_retain(channel->line) : NULL;
			}
		} else {
			if (l) {
				channel = sccp_find_channel_on_line_byid(l, callid);
			} else {
				channel = sccp_channel_find_byid(callid);
				l = channel ? sccp_line_retain(channel->line) : NULL;
			}
		}
	}

	/* Old phones like 7912 never uses callid
	 * so here we don't have a channel, this way we
	 * should get the active channel on device
	 */
	if (!channel) {
		channel = sccp_channel_get_active(d);
		pbx_log(LOG_NOTICE, "%s: Could not find channel by callid %d on line %s with instance %d! Using active channel %d instead.\n", DEV_ID_LOG(d), callid, (l ? l->name : (channel ? channel->line->name : "Undef")), lineInstance, (channel ? channel->callid : 0));
		if (channel && !l) {
			l = sccp_line_retain(channel->line);
		}
	}

	if (!channel) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no (active) channel! Exiting\n", DEV_ID_LOG(d));
		goto EXIT_FUNC;
	}

	if (!channel->owner) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no (active) pbx channel! Exiting\n", DEV_ID_LOG(d));
		sccp_channel_endcall(channel);
		goto EXIT_FUNC;
	}

	if (!l) {
		pbx_log(LOG_NOTICE, "%s: Device sent a Keypress, but there is no line specified! Exiting\n", DEV_ID_LOG(d));
		goto EXIT_FUNC;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: SCCP Digit: %08x (%d) on line %s, channel %d with state: %d\n", DEV_ID_LOG(d), digit, digit, l->name, channel->callid, channel->state);

	if (digit == 14) {
		resp = '*';
	} else if (digit == 15) {
		resp = '#';
	} else if (digit >= 0 && digit <= 9) {
		resp = '0' + digit;
	} else {
		resp = '0' + digit;
		pbx_log(LOG_WARNING, "Unsupported digit %d\n", digit);
	}

	/* added PROGRESS to make sending digits possible during progress state (Pavel Troller) */
	if (channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE || channel->state == SCCP_CHANNELSTATE_PROCEED || channel->state == SCCP_CHANNELSTATE_PROGRESS || channel->state == SCCP_CHANNELSTATE_RINGOUT) {
		/* we have to unlock 'cause the senddigit lock the channel */
		sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_1 "%s: Sending DTMF Digit %c(%d) to %s\n", DEV_ID_LOG(d), digit, resp, l->name);
		sccp_pbx_senddigit(channel, resp);
		goto EXIT_FUNC;
	}

	if ((channel->state == SCCP_CHANNELSTATE_DIALING) || (channel->state == SCCP_CHANNELSTATE_OFFHOOK) || (channel->state == SCCP_CHANNELSTATE_GETDIGITS)) {
		len = strlen(channel->dialedNumber);
		if (len >= (SCCP_MAX_EXTENSION - 1)) {
			sccp_dev_displayprompt(d, lineInstance, channel->callid, "No more digits", 5);
		} else {
			//                      sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: else state\n");
			//                      sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: GLOB(digittimeoutchar) = '%c'\n",GLOB(digittimeoutchar));
			//                      sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: resp = '%c'\n", resp);
			//                      sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: GLOB(digittimeoutchar) %s resp\n", (GLOB(digittimeoutchar) == resp)?"==":"!=");

			/* enbloc emulation */
			double max_deviation = SCCP_SIM_ENBLOC_DEVIATION;
			int max_time_per_digit = SCCP_SIM_ENBLOC_MAX_PER_DIGIT;
			double variance = 0;
			double std_deviation = 0;
			int minimum_digit_before_check = SCCP_SIM_ENBLOC_MIN_DIGIT;
			int lpbx_digit_usecs = 0;
			int number_of_digits = len;
			int timeout_if_enbloc = SCCP_SIM_ENBLOC_TIMEOUT;								// new timeout if we have established we should enbloc dialing 

			sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_1 "SCCP: ENBLOC_EMU digittimeout '%d' ms, sched_wait '%d' ms\n", channel->enbloc.digittimeout, PBX(sched_wait) (channel->scheduler.digittimeout));
			if (GLOB(simulate_enbloc) && !channel->enbloc.deactivate && number_of_digits >= 1) {	// skip the first digit (first digit had longer delay than the rest)
				if ((channel->enbloc.digittimeout) < (PBX(sched_wait) (channel->scheduler.digittimeout) * 1000)) {
					lpbx_digit_usecs = (channel->enbloc.digittimeout) - (PBX(sched_wait) (channel->scheduler.digittimeout));
				} else {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU Cancelled (past digittimeout)\n");
					channel->enbloc.deactivate = 1;
				}
				channel->enbloc.totaldigittime += lpbx_digit_usecs;
				channel->enbloc.totaldigittimesquared += pow(lpbx_digit_usecs, 2);
				sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_1 "SCCP: ENBLOC_EMU digit entry time '%d' ms, total dial time '%d' ms, number of digits: %d\n", lpbx_digit_usecs, channel->enbloc.totaldigittime, number_of_digits);
				if (number_of_digits >= 2) {							// prevent div/0
					if (number_of_digits >= minimum_digit_before_check) {			// minimal number of digits before checking
						if (lpbx_digit_usecs < max_time_per_digit) {
							variance = ((double) channel->enbloc.totaldigittimesquared - (pow((double) channel->enbloc.totaldigittime, 2) / (double) number_of_digits)) / ((double) number_of_digits - 1);
							std_deviation = sqrt(variance);
							sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU sqrt((%d-((pow(%d, 2))/%d))/%d)='%2.2f'\n", channel->enbloc.totaldigittimesquared, channel->enbloc.totaldigittime, number_of_digits, number_of_digits - 1, std_deviation);
							sccp_log(DEBUGCAT_ACTION) (VERBOSE_PREFIX_1 "SCCP: ENBLOC EMU totaldigittimesquared '%d', totaldigittime '%d', number_of_digits '%d', std_deviation '%2.2f', variance '%2.2f'\n", channel->enbloc.totaldigittimesquared, channel->enbloc.totaldigittime, number_of_digits, std_deviation, variance);
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

			/* removing scheduled dial */
			channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);

			// Overlap Dialing should set display too -FS
			if (channel->state == SCCP_CHANNELSTATE_DIALING && PBX(getChannelPbx) (channel)) {
				/* we shouldn't start pbx another time */
				sccp_pbx_senddigit(channel, resp);
				goto EXIT_FUNC;
			}

			/* as we're not in overlapped mode we should add timeout again */
			if ((channel->scheduler.digittimeout = sccp_sched_add(channel->enbloc.digittimeout, sccp_pbx_sched_dial, channel)) < 0) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unable to reschedule dialing in '%d' ms\n", channel->enbloc.digittimeout);
			} else {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: reschedule dialing in '%d' ms\n", channel->enbloc.digittimeout);
			}
#ifdef CS_SCCP_PICKUP
			if (!strcmp(channel->dialedNumber, pbx_pickup_ext()) && (channel->state != SCCP_CHANNELSTATE_GETDIGITS)) {
				/* set it to offhook state because the sccp_sk_gpickup function look for an offhook channel */
				channel->state = SCCP_CHANNELSTATE_OFFHOOK;
				sccp_sk_gpickup(d, channel->line, lineInstance, channel);
				goto EXIT_FUNC;
			}
#endif

			if (GLOB(digittimeoutchar) == resp) {							// we dial on digit timeout char !
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Got digit timeout char '%c', dial immediately\n", GLOB(digittimeoutchar));
				channel->dialedNumber[len] = '\0';
				if (channel->scheduler.digittimeout) {
					channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);
				}
				sccp_safe_sleep(100);								// we would hear last keypad stroke before starting all
				sccp_pbx_softswitch(channel);
				goto EXIT_FUNC;
			}
			if (sccp_pbx_helper(channel) == SCCP_EXTENSION_EXACTMATCH) {				// we dial when helper says we have a match
				if (channel->scheduler.digittimeout) {
					channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);
				}
				sccp_safe_sleep(100);								// we would hear last keypad stroke before starting all
				sccp_pbx_softswitch(channel);							// channel will be released by hangup
				goto EXIT_FUNC;
			}
		}
	} else {
		pbx_log(LOG_WARNING, "%s: keypad_button could not be handled correctly because of invalid state on line %s, channel: %d, state: %d\n", DEV_ID_LOG(d), l->name, channel->callid, channel->state);
	}
	sccp_handle_dialtone(channel);
EXIT_FUNC:
	l = l ? sccp_line_release(l) : NULL;
	channel = channel ? sccp_channel_release(channel) : NULL;
}

/*!
 * \brief Handle DialTone Without Lock
 * \param channel SCCP Channel as sccp_channel_t
 */
void sccp_handle_dialtone(sccp_channel_t * channel)
{
	sccp_line_t *l = NULL;
	sccp_device_t *d = NULL;
	int lenDialed = 0, lenSecDialtoneDigits = 0;

	uint8_t instance;
	uint32_t callid = 0;
	uint32_t secondary_dialtone_tone = 0;

	if (!channel) {
		return;
	}	

	if (!(l = sccp_line_retain(channel->line))) {
		return;
	}

	if (!(d = sccp_channel_getDevice_retained(channel))) {
		return;
	}	

	callid = channel->callid;
	lenDialed = strlen(channel->dialedNumber);

	instance = sccp_device_find_index_for_line(d, l->name);
	/* secondary dialtone check */
	lenSecDialtoneDigits = strlen(l->secondary_dialtone_digits);
	secondary_dialtone_tone = l->secondary_dialtone_tone;

	/* we check dialtone just in DIALING action
	 * otherwise, you'll get secondary dialtone also
	 * when catching call forward number, meetme room,
	 * etc.
	 * */

	if (channel->ss_action != SCCP_SS_DIAL)  {
		l = sccp_line_release(l);	
		d = sccp_device_release(d);	
		return;
	}

	if (lenDialed == 0 && channel->state != SCCP_CHANNELSTATE_OFFHOOK) {
		sccp_dev_stoptone(d, instance, channel->callid);
		sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, channel->callid, 0);
	} else if (lenDialed == 1) {
		if (channel->state != SCCP_CHANNELSTATE_DIALING) {
			sccp_dev_stoptone(d, instance, channel->callid);
			sccp_indicate(d, channel, SCCP_CHANNELSTATE_DIALING);
		} else {
			sccp_dev_stoptone(d, instance, channel->callid);
		}
	}

	if (d && channel && l) {
		if (lenSecDialtoneDigits && lenDialed == lenSecDialtoneDigits && !strncmp(channel->dialedNumber, l->secondary_dialtone_digits, lenSecDialtoneDigits)) {
			/* We have a secondary dialtone */
			sccp_dev_starttone(d, secondary_dialtone_tone, instance, callid, 0);
		} else if ((lenSecDialtoneDigits) && (lenDialed == lenSecDialtoneDigits + 1 || (lenDialed > 1 && lenSecDialtoneDigits > 1 && lenDialed == lenSecDialtoneDigits - 1))) {
			sccp_dev_stoptone(d, instance, callid);
		}
	}
	l = sccp_line_release(l);
	d = sccp_device_release(d);
}

/*!
 * \brief Handle Soft Key Event for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;
	const sccp_softkeyMap_cb_t *softkeyMap_cb = NULL;

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Got Softkey\n", DEV_ID_LOG(d));

	uint32_t event = letohl(r->msg.SoftKeyEventMessage.lel_softKeyEvent);
	uint32_t lineInstance = letohl(r->msg.SoftKeyEventMessage.lel_lineInstance);
	uint32_t callid = letohl(r->msg.SoftKeyEventMessage.lel_callReference);

	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: Received Softkey Event but no device to connect it to. Exiting\n");
		return;
	}

	event = softkeysmap[event - 1];

	/* correct events for nokia icc client (Legacy Support -FS) */
	if (d->config_type && !strcasecmp(d->config_type, "nokia-icc")) {
		switch (event) {
			case SKINNY_LBL_DIRTRFR:
				event = SKINNY_LBL_ENDCALL;
				break;
		}
	}

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Got Softkey: %s (%d) line=%d callid=%d\n", d->id, label2str(event), event, lineInstance, callid);

	/* we have no line and call information -> use default line */
	if (!lineInstance && !callid && (event == SKINNY_LBL_NEWCALL || event == SKINNY_LBL_REDIAL)) {
		if (d->defaultLineInstance > 0)
			lineInstance = d->defaultLineInstance;
		else
			l = sccp_dev_get_activeline(d);
	}

	if (lineInstance) {
		l = sccp_line_find_byid(d, lineInstance);
	}

	if (l && callid) {
		c = sccp_find_channel_on_line_byid(l, callid);
	}
	do {
		softkeyMap_cb = sccp_getSoftkeyMap_by_SoftkeyEvent(event);

		if (!softkeyMap_cb) {
			pbx_log(LOG_WARNING, "Don't know how to handle keypress %d\n", event);
			break;
		}
		if (softkeyMap_cb->channelIsNecessary == TRUE && !c) {
			char buf[100];

			snprintf(buf, 100, "No channel for %s!", label2str(event));
			sccp_dev_displayprompt(d, lineInstance, 0, buf, 7);
			sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, lineInstance, 0, 0);
			pbx_log(LOG_WARNING, "%s: Skip handling of Softkey %s (%d) line=%d callid=%d, because a channel is required, but not provided. Exiting\n", d->id, label2str(event), event, lineInstance, callid);

			/* disable callplane for this device */
			sccp_device_sendcallstate(d, lineInstance, callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
			sccp_dev_set_cplane(l, lineInstance, d, 0);
			sccp_dev_set_keyset(d, lineInstance, callid, KEYMODE_ONHOOK);
			break;
		}
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Handling Softkey: %s on line: %s and channel: %s\n", d->id, label2str(event), l ? l->name : "UNDEF", c ? sccp_channel_toString(c) : "UNDEF");
		softkeyMap_cb->softkeyEvent_cb(d, l, lineInstance, c);

	} while (FALSE);

	c = c ? sccp_channel_release(c) : NULL;
	l = l ? sccp_line_release(l) : NULL;
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - channel
 *        - see sccp_channel_startmediatransmission()
 *        - see sccp_channel_endcall()
 */
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	struct sockaddr_in sin = { 0 };
	uint32_t status = 0, callReference = 0, passThruPartyId = 0;
	sccp_channel_t *channel = NULL;

	d->protocol->parseOpenReceiveChannelAck((const sccp_moo_t *) r, &status, &sin, &passThruPartyId, &callReference);

	//      if (d->trustphoneip || d->directrtp) {
	if (!d->directrtp) {
		memcpy(&sin.sin_addr, &s->sin.sin_addr, sizeof(sin.sin_addr));
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Got OpenChannel ACK.  Status: %d, Remote RTP/UDP '%s:%d', Type: %s, PassThruPartyId: %u, CallID: %u\n", d->id, status, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), (d->directrtp ? "DirectRTP" : "Indirect RTP"), passThruPartyId, callReference);

	if (status) {
		// rtp error from the phone 
		pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device returned error: %d ! No RTP stream available. Possibly all the rtp streams the phone supports have been used up. Giving up.\n", d->id, status);
		return;
	}

	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO6911 && 0 == passThruPartyId) {
		passThruPartyId = 0xFFFFFFFF - callReference;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Dealing with 6911 which does not return a passThruPartyId, using callid: %u -> passThruPartyId %u\n", d->id, callReference, passThruPartyId);
	}

	if ((d->active_channel && d->active_channel->passthrupartyid == passThruPartyId) || !passThruPartyId) {	// reduce the amount of searching by first checking active_channel
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passThruPartyId);
	}
	if (channel) {												// && sccp_channel->state != SCCP_CHANNELSTATE_DOWN) {
		if (channel->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			pbx_log(LOG_WARNING, "%s: (OpenReceiveChannelAck) Invalid Number (%d)\n", DEV_ID_LOG(d), channel->state);
			channel = sccp_channel_release(channel);
			return;
		}
		if (channel->state == SCCP_CHANNELSTATE_DOWN) {
			pbx_log(LOG_WARNING, "%s: (OpenReceiveChannelAck) Channel is down. Giving up... (%d)\n", DEV_ID_LOG(d), channel->state);
			sccp_moo_t *r;

			REQ(r, CloseReceiveChannel);
			r->msg.CloseReceiveChannel.lel_conferenceId = htolel(callReference);
			r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(passThruPartyId);
			r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(callReference);
			sccp_dev_send(d, r);
			channel = sccp_channel_release(channel);
			return;
		}

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Starting Phone RTP/UDP Transmission (State: %s[%d])\n", d->id, sccp_indicate2str(channel->state), channel->state);
		sccp_channel_setDevice(channel, d);
		if (channel->rtp.audio.rtp) {
			sccp_channel_set_active(d, channel);

			//ast_rtp_set_peer(channel->rtp.audio.rtp, &sin);
			sccp_rtp_set_phone(channel, &channel->rtp.audio, &sin);

			sccp_channel_startmediatransmission(channel);						/*!< Starting Media Transmission Earlier to fix 2 second delay - Copied from v2 - FS */

			/* update status */
			channel->rtp.audio.writeState |= SCCP_RTP_STATUS_ACTIVE;
			/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED || channel->state == SCCP_CHANNELSTATE_CONNECTEDCONFERENCE) && ((channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE)) ) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
		} else {
			pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
			sccp_channel_endcall(channel);								// FS - 350
		}
		channel = sccp_channel_release(channel);
	} else {
		/* we successfully opened receive channel, but have no channel active -> close receive */
		int callId = passThruPartyId ^ 0xFFFFFFFF;

		pbx_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) No channel with this PassThruPartyId %u (callReference: %d, callid: %d)!\n", d->id, passThruPartyId, callReference, callId);

		sccp_moo_t *r;

		REQ(r, CloseReceiveChannel);
		r->msg.CloseReceiveChannel.lel_conferenceId = htolel(callId);
		r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(passThruPartyId);
		r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(callId);
		sccp_dev_send(d, r);
	}
}

/*!
 * \brief Handle Open Multi Media Receive Acknowledgement
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - channel
 *        - see sccp_dev_send()
 */
void sccp_handle_OpenMultiMediaReceiveAck(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	struct sockaddr_in sin = { 0 };
	sccp_channel_t *channel;
	uint32_t status = 0, partyID = 0, passThruPartyId = 0, callReference;

	d->protocol->parseOpenMultiMediaReceiveChannelAck((const sccp_moo_t *) r, &status, &sin, &passThruPartyId, &callReference);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Got OpenMultiMediaReceiveChannelAck.  Status: %d, Remote RTP/UDP '%s:%d', Type: %s, PassThruId: %u, CallID: %u\n", d->id, status, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), (d->directrtp ? "DirectRTP" : "Indirect RTP"), partyID, callReference);
	if (status) {
		/* rtp error from the phone */
		pbx_log(LOG_WARNING, "%s: Error while opening MediaTransmission (%d). Ending call\n", DEV_ID_LOG(d), status);
		sccp_dump_packet((unsigned char *) &r->msg, r->header.length);
		return;
	}

	if ((d->active_channel && d->active_channel->passthrupartyid == passThruPartyId) || !passThruPartyId) {	// reduce the amount of searching by first checking active_channel
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passThruPartyId);
	}
	/* prevent a segmentation fault on fast hangup after answer, failed voicemail for example */
	if (channel) {												// && sccp_channel->state != SCCP_CHANNELSTATE_DOWN) {
		if (channel->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			channel = sccp_channel_release(channel);
			return;
		}

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: STARTING DEVICE RTP TRANSMISSION WITH STATE %s(%d)\n", d->id, sccp_indicate2str(channel->state), channel->state);
		memcpy(&channel->rtp.video.phone, &sin, sizeof(sin));
		if (channel->rtp.video.rtp || sccp_rtp_createVideoServer(channel)) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s:%d\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
			//                      ast_rtp_set_peer(channel->rtp.video.rtp, &sin);
			//sccp_rtp_set_peer(channel, &channel->rtp.video, &sin);
			sccp_rtp_set_phone(channel, &channel->rtp.video, &sin);
			channel->rtp.video.writeState |= SCCP_RTP_STATUS_ACTIVE;

			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE)
			    ) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
		} else {
			pbx_log(LOG_ERROR, "%s: Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		}

		sccp_moo_t *r1;

		r1 = sccp_build_packet(MiscellaneousCommandMessage, sizeof(r->msg.MiscellaneousCommandMessage));
		r1->msg.MiscellaneousCommandMessage.lel_conferenceId = htolel(channel->callid);
		r1->msg.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r1->msg.MiscellaneousCommandMessage.lel_callReference = htolel(channel->callid);
		r1->msg.MiscellaneousCommandMessage.lel_miscCommandType = htolel(1);				/* videoFastUpdatePicture */
		sccp_dev_send(d, r1);

		r1 = sccp_build_packet(Unknown_0x0141_Message, sizeof(r->msg.Unknown_0x0141_Message));
		r1->msg.Unknown_0x0141_Message.lel_conferenceID = htolel(channel->callid);
		r1->msg.Unknown_0x0141_Message.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r1->msg.Unknown_0x0141_Message.lel_callReference = htolel(channel->callid);
		r1->msg.Unknown_0x0141_Message.lel_maxBitRate = htolel(0x00000c80);
		sccp_dev_send(d, r1);

		PBX(queue_control) (channel->owner, AST_CONTROL_VIDUPDATE);
		channel = sccp_channel_release(channel);
	} else {
		pbx_log(LOG_ERROR, "%s: No channel with this PassThruId %u!\n", d->id, partyID);
	}
}

/*!
 * \brief Handle Version for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_version(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	REQ(r1, VersionMessage);
	sccp_copy_string(r1->msg.VersionMessage.requiredVersion, d->imageversion, sizeof(r1->msg.VersionMessage.requiredVersion));
	sccp_dev_send(d, r1);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Sending version number: %s\n", d->id, d->imageversion);
}


#define CALC_AVG(_newval, _mean, _numval) ( ( (_mean * (_numval) ) + _newval ) / (_numval + 1))
/*!
 * \brief Handle Connection Statistics for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
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
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{

	sccp_call_statistics_t *last_call_stats = NULL;
	sccp_call_statistics_t *avg_call_stats = NULL;
	char QualityStats[128] = "";

	if ((d = sccp_device_retain(d))) {
		// update last_call_statistics
		last_call_stats = &d->call_statistics[SCCP_CALLSTATISTIC_LAST];
		strncpy(last_call_stats->type, "LAST", sizeof(last_call_stats->type));
		if (letohl(r->header.lel_protocolVer < 19)) {
			last_call_stats->num = letohl(r->msg.ConnectionStatisticsRes.lel_CallIdentifier);
			last_call_stats->packets_sent = letohl(r->msg.ConnectionStatisticsRes.lel_SentPackets);
			last_call_stats->packets_received = letohl(r->msg.ConnectionStatisticsRes.lel_RecvdPackets);
			last_call_stats->packets_lost = letohl(r->msg.ConnectionStatisticsRes.lel_LostPkts);
			last_call_stats->jitter = letohl(r->msg.ConnectionStatisticsRes.lel_Jitter);
			last_call_stats->latency = letohl(r->msg.ConnectionStatisticsRes.lel_latency);
			sccp_copy_string(QualityStats, r->msg.ConnectionStatisticsRes.QualityStats, sizeof(QualityStats));
		} else {
			last_call_stats->num = letohl(r->msg.ConnectionStatisticsRes_V19.lel_CallIdentifier);
			last_call_stats->packets_sent = letohl(r->msg.ConnectionStatisticsRes_V19.lel_SentPackets);
			last_call_stats->packets_received = letohl(r->msg.ConnectionStatisticsRes_V19.lel_RecvdPackets);
			last_call_stats->packets_lost = letohl(r->msg.ConnectionStatisticsRes_V19.lel_LostPkts);
			last_call_stats->jitter = letohl(r->msg.ConnectionStatisticsRes_V19.lel_Jitter);
			last_call_stats->latency = letohl(r->msg.ConnectionStatisticsRes_V19.lel_latency);
			sccp_copy_string(QualityStats, r->msg.ConnectionStatisticsRes_V19.QualityStats, sizeof(QualityStats));
		}
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Statistics: CallID: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", d->id, last_call_stats->num, last_call_stats->packets_sent, last_call_stats->packets_received, last_call_stats->packets_lost, last_call_stats->jitter, last_call_stats->latency);

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Quality Statistics: %s\n", d->id, QualityStats);
		if (!sccp_strlen_zero(QualityStats)) {
			sscanf(QualityStats, "MLQK=%f;MLQKav=%f;MLQKmn=%f;MLQKmx=%f;ICR=%f;CCR=%f;ICRmx=%f;CS=%d;SCS=%d;MLQKvr=%f",
			       &last_call_stats->opinion_score_listening_quality,
			       &last_call_stats->avg_opinion_score_listening_quality, &last_call_stats->mean_opinion_score_listening_quality, &last_call_stats->max_opinion_score_listening_quality, &last_call_stats->interval_concealement_ratio, &last_call_stats->cumulative_concealement_ratio, &last_call_stats->max_concealement_ratio, &last_call_stats->concealed_seconds, &last_call_stats->severely_concealed_seconds, &last_call_stats->variance_opinion_score_listening_quality);
		}
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;ICR=%.4f;CCR=%.4f;ICRmx=%.4f;CS=%d;SCS=%d;MLQKvr=%.2f\n",
					   d->id, last_call_stats->opinion_score_listening_quality, last_call_stats->avg_opinion_score_listening_quality, last_call_stats->mean_opinion_score_listening_quality,
					   last_call_stats->max_opinion_score_listening_quality, last_call_stats->interval_concealement_ratio, last_call_stats->cumulative_concealement_ratio, last_call_stats->max_concealement_ratio, (int) last_call_stats->concealed_seconds, (int) last_call_stats->severely_concealed_seconds, last_call_stats->variance_opinion_score_listening_quality);

		// update avg_call_statistics
		avg_call_stats = &d->call_statistics[SCCP_CALLSTATISTIC_AVG];
		strncpy(avg_call_stats->type, "AVG", sizeof(last_call_stats->type));
		avg_call_stats->packets_sent = CALC_AVG(last_call_stats->packets_sent, avg_call_stats->packets_sent, avg_call_stats->num);
		avg_call_stats->packets_received = CALC_AVG(last_call_stats->packets_received, avg_call_stats->packets_received, avg_call_stats->num);
		avg_call_stats->packets_lost = CALC_AVG(last_call_stats->packets_lost, avg_call_stats->packets_lost, avg_call_stats->num);
		avg_call_stats->jitter = CALC_AVG(last_call_stats->jitter, avg_call_stats->jitter, avg_call_stats->num);
		avg_call_stats->latency = CALC_AVG(last_call_stats->latency, avg_call_stats->latency, avg_call_stats->num);
		avg_call_stats->opinion_score_listening_quality = CALC_AVG(last_call_stats->opinion_score_listening_quality, avg_call_stats->opinion_score_listening_quality, avg_call_stats->num);
		avg_call_stats->avg_opinion_score_listening_quality = CALC_AVG(last_call_stats->avg_opinion_score_listening_quality, avg_call_stats->avg_opinion_score_listening_quality, avg_call_stats->num);
		avg_call_stats->mean_opinion_score_listening_quality = CALC_AVG(last_call_stats->mean_opinion_score_listening_quality, avg_call_stats->mean_opinion_score_listening_quality, avg_call_stats->num);
		if (avg_call_stats->max_opinion_score_listening_quality < last_call_stats->max_opinion_score_listening_quality)
			avg_call_stats->max_opinion_score_listening_quality = last_call_stats->max_opinion_score_listening_quality;
		avg_call_stats->interval_concealement_ratio = CALC_AVG(last_call_stats->interval_concealement_ratio, avg_call_stats->interval_concealement_ratio, avg_call_stats->num);
		avg_call_stats->cumulative_concealement_ratio = CALC_AVG(last_call_stats->cumulative_concealement_ratio, avg_call_stats->cumulative_concealement_ratio, avg_call_stats->num);
		if (avg_call_stats->max_concealement_ratio < last_call_stats->max_concealement_ratio)
			avg_call_stats->max_concealement_ratio = last_call_stats->max_concealement_ratio;
		avg_call_stats->concealed_seconds = CALC_AVG(last_call_stats->concealed_seconds, avg_call_stats->concealed_seconds, avg_call_stats->num);
		avg_call_stats->severely_concealed_seconds = CALC_AVG(last_call_stats->severely_concealed_seconds, avg_call_stats->severely_concealed_seconds, avg_call_stats->num);
		avg_call_stats->variance_opinion_score_listening_quality = CALC_AVG(last_call_stats->variance_opinion_score_listening_quality, avg_call_stats->variance_opinion_score_listening_quality, avg_call_stats->num);

		avg_call_stats->num++;
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Mean Statistics: # Calls: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", d->id, avg_call_stats->num, avg_call_stats->packets_sent, avg_call_stats->packets_received, avg_call_stats->packets_lost, avg_call_stats->jitter, avg_call_stats->latency);

		// update global_call_statistics
		/*
		   avg_call_stats = &GLOB(avg_call_statistics);
		   avg_call_stats->packets_sent  = ((avg_call_stats->packets_sent * (avg_call_stats->num) ) + last_call_stats->packets_sent) / (avg_call_stats->num + 1);
		   avg_call_stats->packets_received  = ((avg_call_stats->packets_received * (avg_call_stats->num) ) + last_call_stats->packets_received) / (avg_call_stats->num + 1);
		   avg_call_stats->packets_lost  = ((avg_call_stats->packets_lost * (avg_call_stats->num) ) + last_call_stats->packets_lost) / (avg_call_stats->num + 1);
		   avg_call_stats->jitter  = ((avg_call_stats->jitter * (avg_call_stats->num) ) + last_call_stats->jitter) / (avg_call_stats->num + 1);
		   avg_call_stats->latency  = ((avg_call_stats->latency * (avg_call_stats->num) ) + last_call_stats->latency) / (avg_call_stats->num + 1);
		   avg_call_stats->num++;
		 */
		d = sccp_device_release(d);
	}

}

/*!
 * \brief Handle Server Resource Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \todo extend ServerResMessage to be able to return multiple servers (cluster)
 * \todo Handle IPv6
 */
void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	if (!s->ourip.s_addr) {
		pbx_log(LOG_ERROR, "%s: Session IP Changed mid flight\n", DEV_ID_LOG(d));
		return;
	}

	if (s->device && s->device->session != s) {
		pbx_log(LOG_ERROR, "%s: Wrong Session or Session Changed mid flight\n", DEV_ID_LOG(d));
		return;
	}

	/* old protocol function replaced by the SEP file server addesses list */
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Sending servers message\n", DEV_ID_LOG(d));

	REQ(r1, ServerResMessage);
	sccp_copy_string(r1->msg.ServerResMessage.server[0].serverName, pbx_inet_ntoa(s->ourip), sizeof(r1->msg.ServerResMessage.server[0].serverName));
	r1->msg.ServerResMessage.serverListenPort[0] = GLOB(bindaddr.sin_port);
	r1->msg.ServerResMessage.serverIpAddr[0] = s->ourip.s_addr;
	sccp_dev_send(d, r1);
}

/*!
 * \brief Handle Config Status Message for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - device
 *        - device->buttonconfig
 */
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_moo_t *r1;
	sccp_buttonconfig_t *config = NULL;
	uint8_t lines = 0;

	uint8_t speeddials = 0;

	if (!&d->buttonconfig)
		return;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == SPEEDDIAL)
			speeddials++;
		else if (config->type == LINE)
			lines++;
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	REQ(r1, ConfigStatMessage);
	sccp_copy_string(r1->msg.ConfigStatMessage.station_identifier.deviceName, d->id, sizeof(r1->msg.ConfigStatMessage.station_identifier.deviceName));
	r1->msg.ConfigStatMessage.station_identifier.lel_stationUserId = htolel(0);
	r1->msg.ConfigStatMessage.station_identifier.lel_stationInstance = htolel(1);
	r1->msg.ConfigStatMessage.lel_numberLines = htolel(lines);
	r1->msg.ConfigStatMessage.lel_numberSpeedDials = htolel(speeddials);

	sccp_dev_send(s->device, r1);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Sending ConfigStatMessage, lines %d, speeddials %d\n", DEV_ID_LOG(d), lines, speeddials);
}

/*!
 * \brief Handle Enbloc Call Messsage (Dial in one block, instead of number by number)
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \lock
 *      - channel
 */
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;
	int len = 0;

	if (r && !sccp_strlen_zero(r->msg.EnblocCallMessage.calledParty)) {
		channel = sccp_channel_get_active(d);
		if (channel) {
			if ((channel->state == SCCP_CHANNELSTATE_DIALING) || (channel->state == SCCP_CHANNELSTATE_OFFHOOK)) {

				/* for anonymous devices we just want to call the extension defined in hotine->exten -> ignore dialed number -MC */
				if (d->isAnonymous) {
					channel = sccp_channel_release(channel);
					return;
				}

				len = strlen(channel->dialedNumber);
				sccp_copy_string(channel->dialedNumber + len, r->msg.EnblocCallMessage.calledParty, sizeof(channel->dialedNumber) - len);
				channel->scheduler.digittimeout = SCCP_SCHED_DEL(channel->scheduler.digittimeout);
				sccp_pbx_softswitch(channel);
				channel = sccp_channel_release(channel);
				return;
			}
			sccp_pbx_senddigits(channel, r->msg.EnblocCallMessage.calledParty);
			channel = sccp_channel_release(channel);
		} else {
			// Pull up a channel
			l = sccp_dev_get_activeline(d);
			if (l) {
				channel = sccp_channel_newcall(l, d, r->msg.EnblocCallMessage.calledParty, SKINNY_CALLTYPE_OUTBOUND, NULL);
				channel = channel ? sccp_channel_release(channel) : NULL;
				l = sccp_line_release(l);
			}
		}

	}
}

/*!
 * \brief Handle Forward Status Reques for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_line_t *l;
	sccp_moo_t *r1 = NULL;

	uint32_t instance = letohl(r->msg.ForwardStatReqMessage.lel_lineNumber);

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Got Forward Status Request.  Line: %d\n", d->id, instance);
	l = sccp_line_find_byid(d, instance);
	if (l) {
		sccp_dev_forward_status(l, instance, d);
		l = sccp_line_release(l);
	} else {
		/* speeddial with hint. Sending empty forward message */
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Send Forward Status.  Instance: %d\n", d->id, instance);
		REQ(r1, ForwardStatMessage);
		r1->msg.ForwardStatMessage.lel_status = 0;
		r1->msg.ForwardStatMessage.lel_lineNumber = r->msg.ForwardStatReqMessage.lel_lineNumber;
		sccp_dev_send(d, r1);
	}
}

/*!
 * \brief Handle Feature Status Reques for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 */
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_buttonconfig_t *config = NULL;

	int instance = letohl(r->msg.FeatureStatReqMessage.lel_featureInstance);
	int unknown = letohl(r->msg.FeatureStatReqMessage.lel_unknown);

	sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Unknown = %d \n", d->id, instance, unknown);

#ifdef CS_DYNAMIC_SPEEDDIAL
	/*
	 * the new speeddial style uses feature to display state
	 * unfortunately we dont know how to handle this on other way
	 */
	sccp_speed_t k;

	if ((unknown == 1 && d->inuseprotocolversion >= 15)) {
		sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT, &k);

		if (k.valid) {
			sccp_moo_t *r1;

			REQ(r1, FeatureStatDynamicMessage);
			r1->msg.FeatureStatDynamicMessage.lel_instance = htolel(instance);
			r1->msg.FeatureStatDynamicMessage.lel_type = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
			r1->msg.FeatureStatDynamicMessage.lel_status = 0;

			sccp_copy_string(r1->msg.FeatureStatDynamicMessage.DisplayName, k.name, sizeof(r1->msg.FeatureStatDynamicMessage.DisplayName));
			sccp_dev_send(d, r1);
			return;
		}
	}
#endif

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == instance && config->type == FEATURE) {
			sccp_feat_changed(d, NULL, config->button.feature.id);
		}
	}
}

/*!
 * \brief Handle Feature Status Request for Session
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_services_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_moo_t *r1 = NULL;
	sccp_buttonconfig_t *config = NULL;

	int urlIndex = letohl(r->msg.ServiceURLStatReqMessage.lel_serviceURLIndex);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got ServiceURL Status Request.  Index = %d\n", d->id, urlIndex);

	if ((config = sccp_dev_serviceURL_find_byindex(s->device, urlIndex))) {
		if (d->inuseprotocolversion < 7) {
			REQ(r1, ServiceURLStatMessage);
			r1->msg.ServiceURLStatMessage.lel_serviceURLIndex = htolel(urlIndex);
			sccp_copy_string(r1->msg.ServiceURLStatMessage.URL, config->button.service.url, strlen(config->button.service.url) + 1);
			sccp_copy_string(r1->msg.ServiceURLStatMessage.label, config->label, strlen(config->label) + 1);
		} else {
			int URL_len = strlen(config->button.service.url);
			int label_len = strlen(config->label);
			int dummy_len = URL_len + label_len;

			int hdr_len = sizeof(r->msg.ServiceURLStatDynamicMessage) - 1;
			int padding = ((dummy_len + hdr_len) % 4);

			padding = (padding > 0) ? 4 - padding : 0;

			r1 = sccp_build_packet(ServiceURLStatDynamicMessage, hdr_len + dummy_len + padding);
			r1->msg.ServiceURLStatDynamicMessage.lel_serviceURLIndex = htolel(urlIndex);

			if (dummy_len) {
				char buffer[dummy_len + 2];

				memset(&buffer[0], 0, dummy_len + 2);
				if (URL_len)
					memcpy(&buffer[0], config->button.service.url, URL_len);
				if (label_len)
					memcpy(&buffer[URL_len + 1], config->label, label_len);
				memcpy(&r1->msg.ServiceURLStatDynamicMessage.dummy, &buffer[0], dummy_len + 2);
			}
		}
		sccp_dev_send(d, r1);
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
 *      - device->buttonconfig is not always locked
 */
void sccp_handle_feature_action(sccp_device_t * d, int instance, boolean_t toggleState)
{
	sccp_buttonconfig_t *config = NULL;
	sccp_line_t *line = NULL;
	uint8_t status = 0;											/* state of cfwd */
	uint32_t featureStat1 = 0;
	uint32_t featureStat2 = 0;
	uint32_t featureStat3 = 0;
	uint32_t res = 0;

#ifdef CS_DEVSTATE_FEATURE
	char buf[254] = "";
#endif

	if (!d) {
		return;
	}

	sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: instance: %d, toggle: %s\n", d->id, instance, (toggleState) ? "yes" : "no");

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == instance && config->type == FEATURE) {
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: toggle status from %d", d->id, config->button.feature.status);
			config->button.feature.status = (config->button.feature.status == 0) ? 1 : 0;
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 " to %d\n", config->button.feature.status);
			break;
		}

	}

	if (!config || !config->type || config->type != FEATURE) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Couldn find feature with ID = %d \n", d->id, instance);
		return;
	}

	/* notice: we use this function for request and changing status -> so just change state if toggleState==TRUE -MC */
	char featureOption[255];

	if (config->button.feature.options) {
		sccp_copy_string(featureOption, config->button.feature.options, sizeof(featureOption));
	}

	sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s\n", d->id, config->button.feature.id, featureOption);
	switch (config->button.feature.id) {

		case SCCP_FEATURE_PRIVACY:

			if (!d->privacyFeature.enabled) {
				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: privacy feature is disabled, ignore this change\n", d->id);
				break;
			}

			if (!strcasecmp(config->button.feature.options, "callpresent")) {
				res = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", d->id, d->privacyFeature.status);
				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: result=%d\n", d->id, res);
				if (res) {
					/* switch off */
					d->privacyFeature.status &= ~SCCP_PRIVACYFEATURE_CALLPRESENT;
					config->button.feature.status = 0;
				} else {
					d->privacyFeature.status |= SCCP_PRIVACYFEATURE_CALLPRESENT;
					config->button.feature.status = 1;
				}
				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: device->privacyFeature.status=%d\n", d->id, d->privacyFeature.status);
			} else {
				sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: do not know how to handle %s\n", d->id, config->button.feature.options);
			}

			break;
		case SCCP_FEATURE_CFWDALL:
			status = SCCP_CFWD_NONE;

			// Ask for activation of the feature.
			if (config->button.feature.options && !sccp_strlen_zero(config->button.feature.options)) {

				// Now set the feature status. Note that the button status has already been toggled above.
				if (config->button.feature.status) {
					status = SCCP_CFWD_ALL;
				}
			}

			SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
				if (config->type == LINE) {
					line = sccp_line_find_byname(config->button.line.name, FALSE);

					if (line) {
						sccp_line_cfwd(line, d, status, featureOption);
						line = sccp_line_release(line);
					}
				}
			}

			break;

		case SCCP_FEATURE_DND:
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
			d->monitorFeature.status = (d->monitorFeature.status) ? 0 : 1;
			if (TRUE == toggleState) {

				sccp_channel_t *channel = NULL;

				if (d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) {
					d->monitorFeature.status &= ~SCCP_FEATURE_MONITOR_STATE_REQUESTED;
				} else {
					d->monitorFeature.status |= SCCP_FEATURE_MONITOR_STATE_REQUESTED;
				}

				/* check if we need to start or stop monitor */
				if (((d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) >> 1) == (d->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
					sccp_log(1) (VERBOSE_PREFIX_3 "%s: no need to update monitor state\n", d->id);
				} else {
					channel = sccp_channel_get_active(d);
					if (channel) {
						sccp_feat_monitor(d, channel->line, 0, channel);
						channel = sccp_channel_release(channel);
					}
				}

				sccp_log(1) (VERBOSE_PREFIX_3 "%s: set monitor state to %d\n", d->id, d->monitorFeature.status);
			}

			break;
#endif

#ifdef CS_DEVSTATE_FEATURE

		/**
		  * Handling of custom devicestate toggle buttons.
		  */
		case SCCP_FEATURE_DEVSTATE:
			/* Set the appropriate devicestate, toggle it and write to the devstate astdb.. */
			strncpy(buf, (config->button.feature.status) ? ("INUSE") : ("NOT_INUSE"), sizeof(buf));
			res = PBX(feature_addToDatabase) (devstate_db_family, config->button.feature.options, buf);
			pbx_devstate_changed(pbx_devstate_val(buf), "Custom:%s", config->button.feature.options);
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Feature Change DevState: '%s', State: '%s'\n", DEV_ID_LOG(d), config->button.feature.options, config->button.feature.status ? "On" : "Off");
			break;
#endif
		case SCCP_FEATURE_MULTIBLINK:
			featureStat1 = (d->priFeature.status & 0xf) - 1;
			featureStat2 = ((d->priFeature.status & 0xf00) >> 8) - 1;
			featureStat3 = ((d->priFeature.status & 0xf0000) >> 16) - 1;

			if (2 == featureStat2 && 6 == featureStat1)
				featureStat3 = (featureStat3 + 1) % 2;

			if (6 == featureStat1)
				featureStat2 = (featureStat2 + 1) % 3;

			featureStat1 = (featureStat1 + 1) % 7;

			d->priFeature.status = ((featureStat3 + 1) << 16) | ((featureStat2 + 1) << 8) | (featureStat1 + 1);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: priority feature status: %d, %d, %d, total: %d\n", d->id, featureStat3, featureStat2, featureStat1, d->priFeature.status);
			break;

		default:
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: unknown feature\n", d->id);
			break;

	}

	if (config) {
		sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Status: %d\n", d->id, instance, config->button.feature.status);
		sccp_feat_changed(d, NULL, config->button.feature.id);
	}

	return;
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
 * \param r SCCP Moo
 *
 * \since 20090708
 * \author Federico
 */
void sccp_handle_updatecapabilities_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint8_t audio_capability = 0, audio_codec = 0, audio_capabilities = 0;

	/* parsing audio caps */
	audio_capabilities = letohl(r->msg.UpdateCapabilitiesMessage.lel_audioCapCount);
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Audio Capabilities\n", DEV_ID_LOG(d), audio_capabilities);

	if (audio_capabilities > 0 && audio_capabilities <= SKINNY_MAX_CAPABILITIES) {
		for (audio_capability = 0; audio_capability < audio_capabilities; audio_capability++) {
			audio_codec = letohl(r->msg.UpdateCapabilitiesMessage.audioCaps[audio_capability].lel_payloadCapability);

			d->capabilities.audio[audio_capability] = audio_codec;		/** store our audio capabilities */
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%7d %-25s\n", DEV_ID_LOG(d), audio_codec, codec2str(audio_codec));
		}
	}
#ifdef CS_SCCP_VIDEO
	uint8_t video_capabilities = 0, video_capability = 0;
	uint8_t video_codec = 0;

	/* parsing video caps */
	video_capabilities = letohl(r->msg.UpdateCapabilitiesMessage.lel_videoCapCount);

	/* enable video mode button if device has video capability */
	if (video_capabilities > 0 && video_capabilities <= SKINNY_MAX_VIDEO_CAPABILITIES) {
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: enable video mode softkey\n", DEV_ID_LOG(d));
		sccp_dev_set_message(d, "Video support enabled", 5, FALSE, TRUE);

		sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Video Capabilities\n", DEV_ID_LOG(d), video_capabilities);
		for (video_capability = 0; video_capability < video_capabilities; video_capability++) {
			video_codec = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].lel_payloadCapability);

			d->capabilities.video[video_capability] = video_codec;		/** store our video capabilities */
#if DEBUG
			char transmitReceiveStr[5];
			sprintf(transmitReceiveStr, "%c-%c", (letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_STATION_RECEIVE) ? '<' : ' ', (letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].lel_transmitOrReceive) & SKINNY_STATION_TRANSMIT) ? '>' : ' ');
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%-3s %3d %-25s\n", DEV_ID_LOG(d), transmitReceiveStr, video_codec, codec2str(video_codec));

			uint8_t video_levels = 0, video_level = 0;
			uint8_t video_format = 0;

			video_levels = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].lel_levelPreferenceCount);
			for (video_level = 0; video_level < video_levels; video_level++) {
				video_format = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].levelPreference[video_level].format);
				int transmitPreference = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].levelPreference[video_level].transmitPreference);
				int maxBitRate = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].levelPreference[video_level].maxBitRate);
				int minBitRate = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].levelPreference[video_level].minBitRate);

				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s transmitPreference: %d\n", DEV_ID_LOG(d), "", "", transmitPreference);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s format: %d: %s\n", DEV_ID_LOG(d), "", "", video_format, skinny_formatType2str(video_format));
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s maxBitRate: %d\n", DEV_ID_LOG(d), "", "", maxBitRate);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s minBitRate: %d\n", DEV_ID_LOG(d), "", "", minBitRate);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s %s\n", DEV_ID_LOG(d), "", "", "--");
			}

			if (d->capabilities.video[video_capability] == SKINNY_CODEC_H264) {
				int h264_level = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].codec_options.h264.level);
				int h264_profile = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[video_capability].codec_options.h264.profile);

				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s level: %d\n", DEV_ID_LOG(d), "", "", h264_level);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s profile: %d\n", DEV_ID_LOG(d), "", "", h264_profile);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s %s\n", DEV_ID_LOG(d), "", "", "--");
			}

			uint8_t video_customPictureFormat = 0, video_customPictureFormats = 0;

			video_customPictureFormats = letohl(r->msg.UpdateCapabilitiesMessage.customPictureFormatCount);
			for (video_customPictureFormat = 0; video_customPictureFormat < video_customPictureFormats; video_customPictureFormat++) {
				int width = letohl(r->msg.UpdateCapabilitiesMessage.customPictureFormat[video_customPictureFormat].customPictureFormatWidth);
				int height = letohl(r->msg.UpdateCapabilitiesMessage.customPictureFormat[video_customPictureFormat].customPictureFormatHeight);

				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s customPictureFormat %d: width=%d, height=%d\n", DEV_ID_LOG(d), "", "", video_customPictureFormat, width, height);
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP:%6s %-5s %s\n", DEV_ID_LOG(d), "", "", "--");
			}
#endif
		}
	} else {
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: disable video mode softkey\n", DEV_ID_LOG(d));
		sccp_dev_set_message(d, "Video support disabled", 5, FALSE, TRUE);
	}
#endif
}


/*!
 * \brief Handle Keep Alive Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_KeepAliveMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
        sccp_moo_t *r1 = sccp_build_packet(KeepAliveAckMessage, 0);
        sccp_session_send2(s, r1);
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 *
 * \since 20090708
 * \author Federico
 */
void sccp_handle_startmediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	struct sockaddr_in sin = { 0 };
	sccp_channel_t *channel = NULL;
	uint32_t status = 0, partyID = 0, callID = 0, callID1 = 0, passthrupartyid = 0;

	d->protocol->parseStartMediaTransmissionAck((const sccp_moo_t *) r, &partyID, &callID, &callID1, &status, &sin);

	if (partyID)
		passthrupartyid = partyID;

	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO6911 && 0 == passthrupartyid) {
		passthrupartyid = 0xFFFFFFFF - callID1;
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Dealing with 6911 which does not return a passthrupartyid, using callid: %u -> passthrupartyid %u\n", d->id, callID1, passthrupartyid);
	}

	if ((d->active_channel && d->active_channel->passthrupartyid == passthrupartyid) || !passthrupartyid) {
		channel = sccp_channel_retain(d->active_channel);
	} else {
		channel = sccp_channel_find_on_device_bypassthrupartyid(d, passthrupartyid);
	}

	if (!channel) {
		pbx_log(LOG_WARNING, "%s: Channel with passthrupartyid %u / callid %u / callid1 %u not found, please report this to developer\n", DEV_ID_LOG(d), partyID, callID, callID1);
		return;
	}
	if (status) {
		pbx_log(LOG_WARNING, "%s: Error while opening MediaTransmission. Ending call (status: %d)\n", DEV_ID_LOG(d), status);
		sccp_dump_packet((unsigned char *) &r->msg, r->header.length);
		if (channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) {
			sccp_channel_closereceivechannel(channel);
		}
		sccp_channel_endcall(channel);
	} else {
		if (channel->state != SCCP_CHANNELSTATE_DOWN) {
			/* update status */
			channel->rtp.audio.readState &= ~SCCP_RTP_STATUS_PROGRESS;
			channel->rtp.audio.readState |= SCCP_RTP_STATUS_ACTIVE;
			/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
			if ((channel->state == SCCP_CHANNELSTATE_CONNECTED) && (channel->rtp.audio.readState & SCCP_RTP_STATUS_ACTIVE) && (channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE)
			    ) {
				PBX(set_callstate) (channel, AST_STATE_UP);
			}
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Got StartMediaTranmission ACK.  Status: %d, Remote TCP/IP: '%s:%d', CallId %u (%u), PassThruId: %u\n", DEV_ID_LOG(d), status, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), callID, callID1, partyID);
		} else {
			pbx_log(LOG_WARNING, "%s: (sccp_handle_startmediatransmission_ack) Channel already down (%d). Hanging up\n", DEV_ID_LOG(d), channel->state);
			if (channel->rtp.audio.writeState & SCCP_RTP_STATUS_ACTIVE) {
				sccp_channel_closereceivechannel(channel);
			}
			sccp_channel_endcall(channel);
		}
	}
	channel = sccp_channel_release(channel);
}

/*!
 * \brief Handle Device to User Message
 * \param s SCCP Session
 * \param d SCCP Device
 * \param r SCCP Moo
 */
void sccp_handle_device_to_user(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint32_t appID;
	uint32_t callReference;
	uint32_t transactionID;
	uint32_t dataLength;
	char data[StationMaxXMLMessage];

#ifdef CS_SCCP_CONFERENCE
	uint32_t lineInstance;
	uint32_t conferenceID;
	uint32_t participantID;
#endif

	appID = letohl(r->msg.DeviceToUserDataVersion1Message.lel_appID);
#ifdef CS_SCCP_CONFERENCE
	lineInstance = letohl(r->msg.DeviceToUserDataVersion1Message.lel_lineInstance);
#endif
	callReference = letohl(r->msg.DeviceToUserDataVersion1Message.lel_callReference);
	transactionID = letohl(r->msg.DeviceToUserDataVersion1Message.lel_transactionID);
	dataLength = letohl(r->msg.DeviceToUserDataVersion1Message.lel_dataLength);

	if (dataLength) {
		memset(data, 0, dataLength);
		memcpy(data, r->msg.DeviceToUserDataVersion1Message.data, dataLength);
	}

	sccp_log((DEBUGCAT_ACTION | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Handle DTU for %d '%s'\n", d->id, appID, data);
	if (0 != appID && 0 != callReference && 0 != transactionID) {
		switch (appID) {
			case APPID_CONFERENCE:									// Handle Conference App
#ifdef CS_SCCP_CONFERENCE
				conferenceID = lineInstance;							// conferenceId is passed on via lineInstance
				participantID = atoi(data);							// participantId is passed on in the data segment
				sccp_log((DEBUGCAT_ACTION | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Handle ConferenceList Info for AppID %d , CallID %d, Transaction %d, Conference %d, Participant: %d\n", d->id, appID, callReference, transactionID, conferenceID, participantID);
				sccp_conference_handle_device_to_user(d, callReference, transactionID, conferenceID, participantID);
#endif
				break;
			case APPID_CONFERENCE_INVITE:								// Handle Conference Invite
#ifdef CS_SCCP_CONFERENCE
				conferenceID = lineInstance;							// conferenceId is passed on via lineInstance
				participantID = atoi(data);							// participantId is passed on in the data segment
				//phonenumber = atoi(data);
				sccp_log((DEBUGCAT_ACTION | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Handle ConferenceList Info for AppID %d , CallID %d, Transaction %d, Conference %d, Participant: %d\n", d->id, appID, callReference, transactionID, conferenceID, participantID);
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
                        char str_action[10] = "", str_appID[10] = "", str_payload[10] = "", str_transactionID[10] = "";
                        if (sscanf(data,"%[^/]/%[^/]/%[^/]/%[^/]/", str_action, str_appID, str_payload, str_transactionID) > 0) {
                                sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Handle DTU Softkey Button:%s, %s, %s, %s\n", d->id, str_action, str_appID, str_payload, str_transactionID);
                                d->dtu_softkey.action = strdup(str_action);
                                d->dtu_softkey.appID = atoi(str_appID);
                                d->dtu_softkey.payload = atoi(str_payload);						// For Conference Payload=Conference->ID
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
 * \param r SCCP Moo
 */
void sccp_handle_device_to_user_response(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	uint32_t appID;
	uint32_t lineInstance;
	uint32_t callReference;
	uint32_t transactionID;
	uint32_t dataLength;
	char data[StationMaxXMLMessage];

	appID = letohl(r->msg.DeviceToUserDataVersion1Message.lel_appID);
	lineInstance = letohl(r->msg.DeviceToUserDataVersion1Message.lel_lineInstance);
	callReference = letohl(r->msg.DeviceToUserDataVersion1Message.lel_callReference);
	transactionID = letohl(r->msg.DeviceToUserDataVersion1Message.lel_transactionID);
	dataLength = letohl(r->msg.DeviceToUserDataVersion1Message.lel_dataLength);

	if (dataLength) {
		memset(data, 0, dataLength);
		memcpy(data, r->msg.DeviceToUserDataVersion1Message.data, dataLength);
	}

	sccp_log((DEBUGCAT_ACTION | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: DTU Response: AppID %d , LineInstance %d, CallID %d, Transaction %d\n", d->id, appID, lineInstance, callReference, transactionID);
	sccp_log((DEBUGCAT_MESSAGE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: DTU Response: Data %s\n", d->id, data);
	
	if (appID == APPID_DEVICECAPABILITIES) {
	        sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device Capabilities Response '%s'\n", d->id, data);
	}
}

/*!
 * \brief Handle Start Multi Media Transmission Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_startmultimediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	struct sockaddr_in sin = { 0 };

	sccp_channel_t *c;

	uint32_t status = 0, partyID = 0, callID = 0, callID1 = 0;

	//      sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, r->header.length);
	d->protocol->parseStartMultiMediaTransmissionAck((const sccp_moo_t *) r, &partyID, &callID, &callID1, &status, &sin);

	c = sccp_channel_find_bypassthrupartyid(partyID);
	if (!c) {
		pbx_log(LOG_WARNING, "%s: Channel with passthrupartyid %u not found, please report this to developer\n", DEV_ID_LOG(d), partyID);
		return;
	}
	if (status) {
		pbx_log(LOG_WARNING, "%s: Error while opening MediaTransmission. Ending call\n", DEV_ID_LOG(d));
		sccp_channel_endcall(c);
		c = sccp_channel_release(c);
		return;
	}

	/* update status */
	c->rtp.video.readState &= ~SCCP_RTP_STATUS_PROGRESS;
	c->rtp.video.readState |= SCCP_RTP_STATUS_ACTIVE;

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got StartMultiMediaTranmission ACK.  Status: %d, Remote TCP/IP '%s:%d', CallId %u (%u), PassThruId: %u\n", DEV_ID_LOG(d), status, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), callID, callID1, partyID);

	c = sccp_channel_release(c);
}

/*!
 * \brief Handle Start Multi Media Transmission Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_mediatransmissionfailure(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_dump_packet((unsigned char *) &r->msg.RegisterMessage, r->header.length);
	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Received a MediaTranmissionFailure (not being handled fully at this moment)\n", DEV_ID_LOG(d));
}

/*!
 * \brief Handle Miscellaneous Command Message
 * \param s SCCP Session as sccp_session_t
 * \param d SCCP Device as sccp_device_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_miscellaneousCommandMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r)
{
	sccp_miscCommandType_t commandType;
	struct sockaddr_in sin = { 0 };
	uint32_t partyID;
	sccp_channel_t *channel;

	partyID = letohl(r->msg.MiscellaneousCommandMessage.lel_passThruPartyId);
	commandType = letohl(r->msg.MiscellaneousCommandMessage.lel_miscCommandType);

	channel = sccp_channel_find_bypassthrupartyid(partyID);

	switch (commandType) {
		case SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE:

			break;
		case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE:
			memcpy(&sin.sin_addr, &r->msg.MiscellaneousCommandMessage.data.videoFastUpdatePicture.bel_remoteIpAddr, sizeof(sin.sin_addr));
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: media statistic for %s, value1: %u, value2: %u, value3: %u, value4: %u\n",
						  channel ? channel->currentDeviceId : "--", pbx_inet_ntoa(sin.sin_addr), letohl(r->msg.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value1), letohl(r->msg.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value2), letohl(r->msg.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value3), letohl(r->msg.MiscellaneousCommandMessage.data.videoFastUpdatePicture.lel_value4)
			    );
			break;
//		case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEGOB:
//      	case SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEMB:
//		case SKINNY_MISCCOMMANDTYPE_LOSTPICTURE:
//		case SKINNY_MISCCOMMANDTYPE_LOSTPARTIALPICTURE:
//		case SKINNY_MISCCOMMANDTYPE_RECOVERYREFERENCEPICTURE:
//		case SKINNY_MISCCOMMANDTYPE_TEMPORALSPATIALTRADEOFF:
		default:

			break;
	}

	channel = channel ? sccp_channel_release(channel) : NULL;
}

// kate: indent-width 4; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets on;
