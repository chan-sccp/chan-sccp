
/*!
 * \file 	sccp_actions.c
 * \brief 	SCCP Actions Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \author 	Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks	Purpose: 	SCCP Actions
 * 		When to use:	Only methods directly related to sccp actions should be stored in this source file.
 *                              Actions handle all the message interactions from and to SCCP phones
 *   		Relationships: 	Other Function wanting to communicate to phones
 *                              Phones Requesting function to be performed
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#if ASTERISK_VERSION_NUM < 10400

/*!
 * \brief Host Access Rule Structure
 */
struct ast_ha {
	/* Host access rule */
	struct in_addr netaddr;
	struct in_addr netmask;
	int sense;
	struct ast_ha *next;
};
#endif

/*
 * \brief Local Function to check for Valid Session, Message and Device
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 * \param s SCCP Device as sccp_device_t
 * \return -1 or Device;
 */
static sccp_device_t *check_session_message_device(sccp_session_t * s, sccp_moo_t * r, const char *msg)
{
	sccp_device_t *d = NULL;

	if (!s || (s->fds[0].fd < 0)) {
		ast_log(LOG_ERROR, "(%s) Session no longer valid\n", msg);
		return NULL;
	}

	if (!r) {
		ast_log(LOG_ERROR, "(%s) No Message Provided\n", msg);
		return NULL;
	}

	if (!(d = s->device)) {
		ast_log(LOG_ERROR, "No valid Device available to handle %s for", msg);
		return NULL;
	}

	if (s != s->device->session) {
		ast_log(LOG_WARNING, "(%s) Provided Session and Device Session are not the same!!\n", msg);
	}

	if ((GLOB(debug) & (DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) != 0) {
		uint32_t mid = letohl(r->lel_messageId);

		ast_log(LOG_NOTICE, "%s: SCCP Handle Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(d), message2str(mid), mid, r->length);
		sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET) ? r->length : SCCP_MAX_PACKET);
	}

	return d;
}

/*!
 * \brief Handle Alarm
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 */
void sccp_handle_alarm(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "SCCP: Alarm Message: Severity: %s (%d), %s [%d/%d]\n", alarm2str(letohl(r->msg.AlarmMessage.lel_alarmSeverity)), letohl(r->msg.AlarmMessage.lel_alarmSeverity), r->msg.AlarmMessage.text, letohl(r->msg.AlarmMessage.lel_parm1), letohl(r->msg.AlarmMessage.lel_parm2));
}

/*!
 * \brief Handle Unknown Message
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 */
void sccp_handle_unknown_message(sccp_session_t * s, sccp_moo_t * r)
{
	uint32_t mid = letohl(r->lel_messageId);

	if ((GLOB(debug) & (DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) != 0)				// only show when debugging messages
		ast_log(LOG_WARNING, "Unhandled SCCP Message: %s(0x%04X) %d bytes length\n", message2str(mid), mid, r->length);

	sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET) ? r->length : SCCP_MAX_PACKET);
}

#ifdef CS_ADV_FEATURES
/*!
 * \brief Handle Token Request
 *
 * If a fall-back server has been entered in the phones cnf.xml file and the phone has fallen back to a secundairy server
 * it will send a tokenreq to the primairy every so often (secundaity keep alive timeout ?). Once the primairy server sends 
 * a token acknowledgement the switches back.
 *
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_handle_tokenreq(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d;

	sccp_moo_t *r1;

	if (!s || (s->fds[0].fd < 0)) {
		ast_log(LOG_ERROR, "%s: No Valid Session\n", r->msg.RegisterMessage.sId.deviceName);
		return;
	}

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: is requesting a Token, Instance: %d, Type: %s (%d), Version: %d\n", r->msg.RegisterMessage.sId.deviceName, letohl(r->msg.RegisterMessage.sId.lel_instance), devicetype2str(letohl(r->msg.RegisterMessage.lel_deviceType)), letohl(r->msg.RegisterMessage.lel_deviceType), r->msg.RegisterMessage.protocolVer);

	// Search for already known devices -> Cleanup
	d = sccp_device_find_byid(r->msg.RegisterMessage.sId.deviceName, FALSE);
	if (d) {
		if (d->session && d->session != s) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_2 "%s: Device is registered on another server (TokenReq)!\n", d->id);
			d->session->session_stop = 1;				/* do not lock session, this will produce a deadlock, just stop the thread-> everything else will be done by thread it self */
			sccp_dev_clean(d, FALSE, 0);				/* we need to clean device configuration to set lines */
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "Previous Session for %s Closed!\n", d->id);
		}
	}

	/* ip address range check */
	if (GLOB(ha) && !pbx_apply_ha(GLOB(ha), &s->sin)) {
		ast_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.RegisterMessage.sId.deviceName);
		REQ(r1, RegisterTokenReject);
		memcpy(r1->msg.RegisterTokenReject.text, "Device ip not authorized", sizeof(r1->msg.RegisterTokenReject.text));
		sccp_session_send2(s, r1);
		return;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending phone a token acknowledgement\n", r->msg.RegisterMessage.sId.deviceName);
	REQ(r1, RegisterTokenAck);
	sccp_session_send2(s, r1);
}
#endif

/*!
 * \brief Handle Device Registration
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- devices
 * 	- device->permithosts
 * 	- device
 */
void sccp_handle_register(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d;

	sccp_moo_t *r1;

	uint8_t i = 0;

	struct ast_hostent ahp;

	struct hostent *hp;

	struct sockaddr_in sin;

	sccp_hostname_t *permithost;

	if (!s || (s->fds[0].fd < 0)) {
		ast_log(LOG_ERROR, "%s: No Valid Session\n", DEV_ID_LOG(s->device));
		return;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: is registering, Instance: %d, Type: %s (%d), Version: %d\n", r->msg.RegisterMessage.sId.deviceName, letohl(r->msg.RegisterMessage.sId.lel_instance), devicetype2str(letohl(r->msg.RegisterMessage.lel_deviceType)), letohl(r->msg.RegisterMessage.lel_deviceType), r->msg.RegisterMessage.protocolVer);

	/* ip address range check */
	if (GLOB(ha) && !pbx_apply_ha(GLOB(ha), &s->sin)) {
		ast_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.RegisterMessage.sId.deviceName);
		sccp_session_reject(s, "Device ip not authorized");
		return;
	}
	// Search for already known devices
	d = sccp_device_find_byid(r->msg.RegisterMessage.sId.deviceName, FALSE);
	if (d) {
		if (d->session && d->session != s) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_2 "%s: Device is doing a re-registration!\n", d->id);
			d->session->session_stop = 1;				/* do not lock session, this will produce a deadlock, just stop the thread-> everything else will be done by thread it self */
			sccp_dev_clean(d, FALSE, 0);				/* we need to clean device configuration to set lines */
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "Previous Session for %s Closed!\n", d->id);
		}
	}
	// search for all devices including realtime
	d = sccp_device_find_byid(r->msg.RegisterMessage.sId.deviceName, TRUE);
	if (!d) {
		if (GLOB(allowAnonymus)) {
			d = sccp_device_create();
			sccp_copy_string(d->id, r->msg.RegisterMessage.sId.deviceName, sizeof(d->id));
			d->realtime = TRUE;
			d->isAnonymous = TRUE;
#ifdef CS_DYNAMIC_CONFIG
			sccp_config_addButton(d, -1, LINE, GLOB(hotline)->line->name, NULL, NULL);
#else
			sccp_config_addLine(d, GLOB(hotline)->line->name, NULL, 0);
#endif
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", r->msg.RegisterMessage.sId.deviceName, GLOB(hotline)->line->name);
			d->defaultLineInstance = 1;
			SCCP_RWLIST_WRLOCK(&GLOB(devices));
			SCCP_RWLIST_INSERT_HEAD(&GLOB(devices), d, list);
			SCCP_RWLIST_UNLOCK(&GLOB(devices));
		} else {
			ast_log(LOG_NOTICE, "%s: Rejecting device: not found\n", r->msg.RegisterMessage.sId.deviceName);
			sccp_session_reject(s, "Unknown Device");
			return;
		}
	} else if (d->ha && !pbx_apply_ha(d->ha, &s->sin)) {

		// \todo TODO check anonymous devices for permit hosts
		SCCP_LIST_LOCK(&d->permithosts);
		SCCP_LIST_TRAVERSE(&d->permithosts, permithost, list) {
			if ((hp = pbx_gethostbyname(permithost->name, &ahp))) {
				memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
				if (s->sin.sin_addr.s_addr == sin.sin_addr.s_addr) {
					break;
				} else {
					ast_log(LOG_NOTICE, "%s: device ip address does not match the permithost = %s (%s)\n", r->msg.RegisterMessage.sId.deviceName, permithost->name, pbx_inet_ntoa(sin.sin_addr));
				}
			} else {
				ast_log(LOG_NOTICE, "%s: Invalid address resolution for permithost = %s\n", r->msg.RegisterMessage.sId.deviceName, permithost->name);
			}
		}
		SCCP_LIST_UNLOCK(&d->permithosts);

		if (i) {
			ast_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.RegisterMessage.sId.deviceName);
			sccp_session_reject(s, "Device ip not authorized");
			return;
		}
	}

	sccp_device_lock(d);
	d->linesRegistered = FALSE;
	/* test the localnet to understand if the device is behind NAT */
	if (GLOB(localaddr) && pbx_apply_ha(GLOB(localaddr), &s->sin)) {
		/* ok the device is natted */
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Device is behind NAT. We will set externip or externhost for the RTP stream \n", r->msg.RegisterMessage.sId.deviceName);
		d->nat = 1;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", d->id, s->fds[0].fd, pbx_inet_ntoa(s->sin.sin_addr));
	s->device = d;
	d->skinny_type = letohl(r->msg.RegisterMessage.lel_deviceType);

	d->session = s;
	s->lastKeepAlive = time(0);
	d->mwilight = 0;
	d->protocolversion = r->msg.RegisterMessage.protocolVer;

	sccp_device_unlock(d);

	/* we need some entropy for keepalive, to reduce the number of devices sending keepalive at one time */
	int keepAliveInterval = d->keepalive ? d->keepalive : GLOB(keepalive);

	keepAliveInterval = (keepAliveInterval / 2) + (rand() % (keepAliveInterval / 2)) + 1;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Ask the phone to send keepalive message every %d seconds\n", d->id, keepAliveInterval);
	REQ(r1, RegisterAckMessage);

//      sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, r->length);

	if (r->length < 56 && d->protocolversion == 0) {
		// registration request with protocol 0 version structure.
		d->inuseprotocolversion = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: asked our protocol capability (%d). We answered (%d).\n", DEV_ID_LOG(d), GLOB(protocolversion), d->inuseprotocolversion);
	} else if (r->msg.RegisterMessage.protocolVer > GLOB(protocolversion)) {
		d->inuseprotocolversion = GLOB(protocolversion);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: asked for protocol version (%d). We answered (%d) as our capability.\n", DEV_ID_LOG(d), r->msg.RegisterMessage.protocolVer, GLOB(protocolversion));
	} else if (r->msg.RegisterMessage.protocolVer <= GLOB(protocolversion)) {
		d->inuseprotocolversion = r->msg.RegisterMessage.protocolVer;
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: asked our protocol capability (%d). We answered (%d).\n", DEV_ID_LOG(d), GLOB(protocolversion), r->msg.RegisterMessage.protocolVer);
	}

	if (d->inuseprotocolversion <= 3) {
		// Our old flags for protocols from 0 to 3
		r1->msg.RegisterAckMessage.unknown1 = 0x00;
		r1->msg.RegisterAckMessage.unknown2 = 0x00;
		r1->msg.RegisterAckMessage.unknown3 = 0x00;
	} else if (d->inuseprotocolversion <= 10) {
		/* CCM 4.1.3 Sets this bytes this way Proto v6 */
		r1->msg.RegisterAckMessage.unknown1 = 0x20;			// 0x00;
		r1->msg.RegisterAckMessage.unknown2 = 0x00;			// 0x00;
		r1->msg.RegisterAckMessage.unknown3 = 0xFE;			// 0xFE;
	} else if (d->inuseprotocolversion >= 11) {
		/* CCM7 Sets this bytes this way Proto v11 */
		r1->msg.RegisterAckMessage.unknown1 = 0x20;			// 0x00;
		r1->msg.RegisterAckMessage.unknown2 = 0xF1;			// 0xF1;
		r1->msg.RegisterAckMessage.unknown3 = 0xFF;			// 0xFF;
	}

	r1->msg.RegisterAckMessage.protocolVer = d->inuseprotocolversion;
	r1->msg.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	r1->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel((d->keepalive ? d->keepalive : GLOB(keepalive)));

	memcpy(r1->msg.RegisterAckMessage.dateTemplate, GLOB(date_format), sizeof(r1->msg.RegisterAckMessage.dateTemplate));
	sccp_session_send(d, r1);
	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);

	// Ask for the capabilities of the device
	// to proceed with registration according to sccp protocol specification 3.0
	sccp_dev_sendmsg(d, CapabilitiesReqMessage);

}

void sccp_handle_SPAregister(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d;

	sccp_moo_t *r1;

	uint8_t i = 0;

	struct ast_hostent ahp;

	struct hostent *hp;

	struct sockaddr_in sin;

	sccp_hostname_t *permithost;

	if (!s || (s->fds[0].fd < 0)) {
		ast_log(LOG_ERROR, "%s: No Valid Session\n", DEV_ID_LOG(s->device));
		return;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: is registering, Instance: %d, Type: %s (%d)\n", r->msg.SPARegisterMessage.sId.deviceName, r->msg.SPARegisterMessage.sId.lel_instance, devicetype2str(letohl(r->msg.SPARegisterMessage.lel_deviceType)), letohl(r->msg.SPARegisterMessage.lel_deviceType));

	/* ip address range check */
	if (GLOB(ha) && !pbx_apply_ha(GLOB(ha), &s->sin)) {
		ast_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.SPARegisterMessage.sId.deviceName);
		sccp_session_reject(s, "Device ip not authorized");
		return;
	}
	// Search for already known devices
	d = sccp_device_find_byid(r->msg.SPARegisterMessage.sId.deviceName, FALSE);
	if (d) {
		if (d->session) {

			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_2 "%s: SPA-Device is doing a re-registration!\n", d->id);
			if (d->session != s) {
				pthread_cancel(s->session_thread);
			} else {
				return;
			}

		}
	}
	// search for all devices including realtime
	d = sccp_device_find_byid(r->msg.SPARegisterMessage.sId.deviceName, TRUE);
	if (!d) {
		if (GLOB(allowAnonymus)) {
			d = sccp_device_create();
			sccp_copy_string(d->id, r->msg.SPARegisterMessage.sId.deviceName, sizeof(d->id));
			d->realtime = TRUE;
			d->isAnonymous = TRUE;
#ifdef CS_DYNAMIC_CONFIG
			sccp_config_addButton(d, -1, LINE, GLOB(hotline)->line->name, NULL, NULL);
#else
			sccp_config_addLine(d, GLOB(hotline)->line->name, NULL, 0);
#endif
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: hotline name: %s\n", r->msg.SPARegisterMessage.sId.deviceName, GLOB(hotline)->line->name);
			d->defaultLineInstance = 1;
			SCCP_RWLIST_WRLOCK(&GLOB(devices));
			SCCP_RWLIST_INSERT_HEAD(&GLOB(devices), d, list);
			SCCP_RWLIST_UNLOCK(&GLOB(devices));
		} else {
			ast_log(LOG_NOTICE, "%s: Rejecting device: not found\n", r->msg.SPARegisterMessage.sId.deviceName);
			sccp_session_reject(s, "Unknown Device");
			return;
		}
	} else if (d->ha && !pbx_apply_ha(d->ha, &s->sin)) {

		// \todo TODO check anonymous devices for permit hosts
		SCCP_LIST_LOCK(&d->permithosts);
		SCCP_LIST_TRAVERSE(&d->permithosts, permithost, list) {
			if ((hp = pbx_gethostbyname(permithost->name, &ahp))) {
				memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
				if (s->sin.sin_addr.s_addr == sin.sin_addr.s_addr) {
					break;
				} else {
					ast_log(LOG_NOTICE, "%s: device ip address does not match the permithost = %s (%s)\n", r->msg.SPARegisterMessage.sId.deviceName, permithost->name, pbx_inet_ntoa(sin.sin_addr));
				}
			} else {
				ast_log(LOG_NOTICE, "%s: Invalid address resolution for permithost = %s\n", r->msg.SPARegisterMessage.sId.deviceName, permithost->name);
			}
		}
		SCCP_LIST_UNLOCK(&d->permithosts);

		if (i) {
			ast_log(LOG_NOTICE, "%s: Rejecting device: Ip address denied\n", r->msg.SPARegisterMessage.sId.deviceName);
			sccp_session_reject(s, "Device ip not authorized");
			return;
		}
	}

	sccp_device_lock(d);
	d->linesRegistered = FALSE;
	/* test the localnet to understand if the device is behind NAT */
	if (GLOB(localaddr) && pbx_apply_ha(GLOB(localaddr), &s->sin)) {
		/* ok the device is natted */
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Device is behind NAT. We will set externip or externhost for the RTP stream \n", r->msg.SPARegisterMessage.sId.deviceName);
		d->nat = 1;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", d->id, s->fds[0].fd, pbx_inet_ntoa(s->sin.sin_addr));
	s->device = d;
	d->skinny_type = letohl(r->msg.SPARegisterMessage.lel_deviceType);

	d->session = s;
	s->lastKeepAlive = time(0);
	d->mwilight = 0;
	//d->protocolversion = r->msg.SPARegisterMessage.protocolVer;
	d->protocolversion = 0;

	sccp_device_unlock(d);

	/* we need some entropy for keepalive, to reduce the number of devices sending keepalive at one time */
	int keepAliveInterval = d->keepalive ? d->keepalive : GLOB(keepalive);

	keepAliveInterval = (keepAliveInterval / 2) + (rand() % (keepAliveInterval / 2)) - 1;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Ask the phone to send keepalive message every %d seconds\n", d->id, keepAliveInterval);
	REQ(r1, RegisterAckMessage);

	d->inuseprotocolversion = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: asked our protocol capability (%d). We answered (%d).\n", DEV_ID_LOG(d), GLOB(protocolversion), d->inuseprotocolversion);

	r1->msg.RegisterAckMessage.protocolVer = d->inuseprotocolversion;
	r1->msg.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	r1->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel((d->keepalive ? d->keepalive : GLOB(keepalive)));

	memcpy(r1->msg.RegisterAckMessage.dateTemplate, GLOB(date_format), sizeof(r1->msg.RegisterAckMessage.dateTemplate));
	sccp_session_send(d, r1);
	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);

	// Ask for the capabilities of the device
	// to proceed with registration according to sccp protocol specification 3.0
	sccp_dev_sendmsg(d, CapabilitiesReqMessage);
}

/*!
 * \brief Make Button Template for Device
 * \param d SCCP Device as sccp_device_t
 * \return Linked List of ButtonDefinitions
 *
 * \lock
 * 	- device->buttonconfig
 * 	  - see sccp_line_find_byname()
 */
static btnlist *sccp_make_button_template(sccp_device_t * d)
{
	int i = 0;

	btnlist *btn;

	sccp_buttonconfig_t *buttonconfig;

	if (!(btn = ast_malloc(sizeof(btnlist) * StationMaxButtonTemplateSize))) {
		return NULL;
	}

	memset(btn, 0, sizeof(btnlist) * StationMaxButtonTemplateSize);
	sccp_dev_build_buttontemplate(d, btn);

	uint16_t speeddialInstance = 1;						/* starting instance for speeddial is 1 */

	uint16_t lineInstance = 1;

	uint16_t serviceInstance = 1;

	if (!d->isAnonymous) {
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for position for button type %d\n", DEV_ID_LOG(d), buttonconfig->type);
			if (buttonconfig->instance > 0)
				continue;

			if (buttonconfig->type == LINE) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for line position for line '%s'\n", DEV_ID_LOG(d), buttonconfig->button.line.name);
			}

			for (i = 0; i < StationMaxButtonTemplateSize; i++) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: btn[%.2d].type = %d\n", DEV_ID_LOG(d), i, btn[i].type);

				if (buttonconfig->type == LINE && sccp_is_nonempty_string(buttonconfig->button.line.name)
				    && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_LINE)) {

					btn[i].type = SKINNY_BUTTONTYPE_LINE;
					btn[i].ptr = sccp_line_find_byname(buttonconfig->button.line.name);

					/* check for existence */
					if (btn[i].ptr == NULL) {
						btn[i].type = SKINNY_BUTTONTYPE_UNDEFINED;
						buttonconfig->instance = btn[i].instance = 0;
						ast_log(LOG_WARNING, "%s: line %s does not exists\n", DEV_ID_LOG(d), buttonconfig->button.line.name);
					} else {
						buttonconfig->instance = btn[i].instance = lineInstance++;
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

				} else if (buttonconfig->type == SPEEDDIAL && sccp_is_nonempty_string(buttonconfig->button.speeddial.label)
					   && (btn[i].type == SCCP_BUTTONTYPE_MULTI || btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL)) {

					buttonconfig->instance = btn[i].instance = i + 1;
					if (sccp_is_nonempty_string(buttonconfig->button.speeddial.hint)
					    && btn[i].type == SCCP_BUTTONTYPE_MULTI	/* we can set our feature */
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

				} else if (buttonconfig->type == FEATURE && sccp_is_nonempty_string(buttonconfig->button.feature.label)
					   && (btn[i].type == SCCP_BUTTONTYPE_MULTI)) {

					buttonconfig->instance = btn[i].instance = speeddialInstance++;

					switch (buttonconfig->button.feature.id) {
					case SCCP_FEATURE_HOLD:
						btn[i].type = SKINNY_BUTTONTYPE_HOLD;
						break;

					case SCCP_FEATURE_TRANSFER:
						btn[i].type = SKINNY_BUTTONTYPE_TRANSFER;
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
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %s (%s)\n", d->id, buttonconfig->instance, "FEATURE", buttonconfig->button.feature.label);
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
 * \param d SCCP device
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- device
 * 	- device->buttonconfig
 * 	  - see sccp_line_addDevice()
 * 	  - see sccp_hint_lineStatusChanged()
 */
void sccp_handle_AvailableLines(sccp_device_t * d)
{
	uint8_t i = 0, line_count = 0;

	btnlist *btn;

	sccp_line_t *l;

	sccp_buttonconfig_t *buttonconfig = NULL;

	boolean_t defaultLineSet = FALSE;

	line_count = 0;

	/** \todo why do we get the message twice  */
	if (d->linesRegistered)
		return;

	sccp_device_lock(d);
	btn = d->buttonTemplate;
	sccp_device_unlock(d);

	if (!btn) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: no buttontemplate, reset device\n", DEV_ID_LOG(d));
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

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: Phone available lines %d\n", d->id, line_count);
	if (d->isAnonymous == TRUE) {

		sccp_device_lock(d);
		d->currentLine = GLOB(hotline)->line;
		sccp_device_unlock(d);

		sccp_line_addDevice(GLOB(hotline)->line, d, 1, NULL);
		sccp_hint_lineStatusChanged(GLOB(hotline)->line, d, NULL, SCCP_DEVICESTATE_UNAVAILABLE, SCCP_DEVICESTATE_ONHOOK);
	} else {

		for (i = 0; i < StationMaxButtonTemplateSize; i++) {
			if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
				l = btn[i].ptr;

				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Attaching line %s with instance %d to this device\n", d->id, l->name, btn[i].instance);

				sccp_device_lock(d);
				if (defaultLineSet == FALSE) {
					d->currentLine = l;
					defaultLineSet = TRUE;
				}
				sccp_device_unlock(d);

				SCCP_LIST_LOCK(&d->buttonconfig);
				SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
					if (btn[i].instance == buttonconfig->instance && buttonconfig->type == LINE) {
						sccp_line_addDevice(l, d, btn[i].instance, &(buttonconfig->button.line.subscriptionId));
						/* notify the line is on */
						sccp_hint_lineStatusChanged(l, d, NULL, SCCP_DEVICESTATE_UNAVAILABLE, SCCP_DEVICESTATE_ONHOOK);
						continue;
					}
				}
				SCCP_LIST_UNLOCK(&d->buttonconfig);
			}

		}
	}
	sccp_device_lock(d);
	d->linesRegistered = TRUE;
	sccp_device_unlock(d);
}

/*!
 * \brief Handle Accessory Status Message
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 */
void sccp_handle_accessorystatus_message(sccp_session_t * s, sccp_moo_t * r)
{
	/* this is from CCM7 dump -FS */
	uint8_t id;

	uint8_t status;

	uint32_t unknown = 0;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "accessory"))) {
		return;
	}

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

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s' (%u)\n", DEV_ID_LOG(d), accessory2str(d->accessoryused), accessorystatus2str(d->accessorystatus), unknown);
}

/*!
 * \brief Handle Device Unregister
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP MOO T as sccp_moo_t
 */
void sccp_handle_unregister(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "unregister"))) {
		return;
	}

	/* we don't need to look for active channels. the phone does send unregister only when there are no channels */
	REQ(r1, UnregisterAckMessage);
	r1->msg.UnregisterAckMessage.lel_status = SKINNY_UNREGISTERSTATUS_OK;
	sccp_session_send(d, r1);
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: unregister request sent\n", DEV_ID_LOG(d));

	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_NONE);
	pthread_cancel(s->session_thread);

}

/*!
 * \brief Handle Button Template Request for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \warning
 * 	- device->buttonconfig is not always locked
 * 
 * \lock
 * 	- device
 * 	  - see sccp_make_button_template()
 * 	  - see sccp_dev_send()
 */
void sccp_handle_button_template_req(sccp_session_t * s, sccp_moo_t * r)
{
	btnlist *btn;

	int i;

	uint8_t totalButtonCount = 0;

	sccp_moo_t *r1;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "button template"))) {
		return;
	}

	if (d->registrationState != SKINNY_DEVICE_RS_PROGRESS && d->registrationState != SKINNY_DEVICE_RS_OK) {
		ast_log(LOG_WARNING, "%s: Received a button template request from unregistered device\n", d->id);
		pthread_cancel(s->session_thread);
		return;
	}

	sccp_device_lock(d);

	/* pre-attach lines. We will wait for button template req if the phone does support it */
	if (d->buttonTemplate) {
		ast_free(d->buttonTemplate);
	}
	btn = d->buttonTemplate = sccp_make_button_template(d);

	if (!btn) {
		ast_log(LOG_ERROR, "%s: No memory allocated for button template\n", d->id);
		sccp_device_unlock(d);
		pthread_cancel(s->session_thread);
		return;
	}

	REQ(r1, ButtonTemplateMessage);
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;

		if (SKINNY_BUTTONTYPE_UNUSED != btn[i].type) {
			r1->msg.ButtonTemplateMessage.lel_buttonCount++;
			totalButtonCount++;
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
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_DISPLAY;
			break;

		case SKINNY_BUTTONTYPE_UNUSED:
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;

			break;

		default:
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = btn[i].type;
			break;
		}
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %d (%d)\n", d->id, i + 1, r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition, r1->msg.ButtonTemplateMessage.definition[i].instanceNumber);

	}

	r1->msg.ButtonTemplateMessage.lel_buttonOffset = 0;
	r1->msg.ButtonTemplateMessage.lel_buttonCount = htolel(r1->msg.ButtonTemplateMessage.lel_buttonCount);
	/* buttonCount is already in a little endian format so don't need to convert it now */
	r1->msg.ButtonTemplateMessage.lel_totalButtonCount = htolel(totalButtonCount);

	/* set speeddial for older devices like 7912 */
	uint32_t speeddialInstance = 0;

	sccp_buttonconfig_t *config;

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_SPEEDDIAL)) (VERBOSE_PREFIX_3 "%s: configure unconfigured speeddialbuttons \n", d->id);
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
	sccp_device_unlock(d);
}

/*!
 * \brief Handle Line Number for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- device->buttonconfig
 */
void sccp_handle_line_number(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_line_t *l = NULL;

	sccp_moo_t *r1;

	sccp_speed_t *k = NULL;

	sccp_buttonconfig_t *config;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "line number"))) {
		return;
	}
	uint8_t lineNumber = letohl(r->msg.LineStatReqMessage.lel_lineNumber);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Configuring line number %d\n", d->id, lineNumber);
	l = sccp_line_find_byid(d, lineNumber);

	/* if we find no regular line - it can be a speeddial with hint */
	if (!l)
		k = sccp_dev_speed_find_byindex(d, lineNumber, SCCP_BUTTONTYPE_HINT);

	REQ(r1, LineStatMessage);
	if (!l && !k) {
		ast_log(LOG_ERROR, "%s: requested a line configuration for unknown line %d\n", s->device->id, lineNumber);
		r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);
		sccp_dev_send(s->device, r1);
		return;
	}
	r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);

	sccp_copy_string(r1->msg.LineStatMessage.lineDirNumber, ((l) ? l->name : (k) ? k->name : ""), sizeof(r1->msg.LineStatMessage.lineDirNumber));

	/* lets set the device description for the first line, so it will be display on top of device -MC */
	if (lineNumber == 1) {
		sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, (d->description), sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	} else {
		sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, ((l) ? l->description : (k) ? k->name : ""), sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	}

	sccp_copy_string(r1->msg.LineStatMessage.lineDisplayName, ((l) ? l->label : (k) ? k->name : ""), sizeof(r1->msg.LineStatMessage.lineDisplayName));

	sccp_dev_send(d, r1);

	/* force the forward status message. Some phone does not request it registering */
	if (l) {
		sccp_dev_forward_status(l, lineNumber, d);

		/* set default line on device */
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->instance == lineNumber) {
				if (config->type == LINE) {
					if (config->button.line.options && !strcasecmp(config->button.line.options, "default")) {
						d->defaultLineInstance = lineNumber;
						sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", lineNumber);
					}
				}
				break;
			}
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);

	}
	/* remove speeddial if present */
	if (k) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_LINE | DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "%s: line is hint for %s\n", s->device->id, k->hint);
		ast_free(k);
	}
}

/*!
 * \brief Handle SpeedDial Status Request for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_speed_t *k;

	sccp_moo_t *r1;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "speeddial stat"))) {
		return;
	}

	int wanted = letohl(r->msg.SpeedDialStatReqMessage.lel_speedDialNumber);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_SPEEDDIAL)) (VERBOSE_PREFIX_3 "%s: Speed Dial Request for Button %d\n", s->device->id, wanted);

	REQ(r1, SpeedDialStatMessage);
	r1->msg.SpeedDialStatMessage.lel_speedDialNumber = htolel(wanted);

	k = sccp_dev_speed_find_byindex(s->device, wanted, SCCP_BUTTONTYPE_SPEEDDIAL);
	if (k) {
		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDirNumber, k->ext, sizeof(r1->msg.SpeedDialStatMessage.speedDialDirNumber));
		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDisplayName, k->name, sizeof(r1->msg.SpeedDialStatMessage.speedDialDisplayName));
		ast_free(k);
	} else {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_SPEEDDIAL)) (VERBOSE_PREFIX_3 "%s: speeddial %d not assigned\n", DEV_ID_LOG(s->device), wanted);
	}

	sccp_dev_send(d, r1);

}

/*!
 * \brief Handle Stimulus for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- channel
 * 	- device
 * 	  - see sccp_device_find_index_for_line()
 */
void sccp_handle_stimulus(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_line_t *l = NULL;

	sccp_speed_t *k;

	sccp_channel_t *c, *c1;

	uint8_t stimulus;

	uint8_t instance;

	sccp_channel_t *holdChannel, *ringChannel;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "stimulus"))) {
		return;
	}

	stimulus = letohl(r->msg.StimulusMessage.lel_stimulus);
	instance = letohl(r->msg.StimulusMessage.lel_stimulusInstance);

	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);			/* use adhoc dial feture with hotline */
		return;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got stimulus=%s (%d) for instance=%d\n", d->id, stimulus2str(stimulus), stimulus, instance);

	if (!instance) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Instance 0 is not a valid instance. Trying the active line %d\n", d->id, instance);
		l = sccp_dev_get_activeline(d);
		if (!l) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line found\n", d->id);
			return;
		}
		if (strlen(l->adhocNumber) > 0) {
			sccp_feat_adhocDial(d, l);
			return;
		}
		// \todo TODO set index
		instance = 1;
	}

	switch (stimulus) {

	case SKINNY_BUTTONTYPE_LASTNUMBERREDIAL:				// We got a Redial Request
		if (!sccp_is_nonempty_string(d->lastNumber))
			return;
		c = sccp_channel_get_active_locked(d);
		if (c) {
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
				sccp_copy_string(c->dialedNumber, d->lastNumber, sizeof(d->lastNumber));
				SCCP_SCHED_DEL(sched, c->digittimeout);
				sccp_pbx_softswitch_locked(c);

				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Redial the number %s\n", d->id, d->lastNumber);
			} else {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Redial ignored as call in progress\n", d->id);
			}
			sccp_channel_unlock(c);
		} else {
			l = d->currentLine;
			if (l) {
				sccp_channel_newcall(l, d, d->lastNumber, SKINNY_CALLTYPE_OUTBOUND);
			}
		}
		break;

	case SKINNY_BUTTONTYPE_LINE:						// We got a Line Request
		l = sccp_line_find_byid(d, instance);
		if (!l) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No line for instance %d. Looking for a speeddial with hint\n", d->id, instance);
			k = sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT);
			if (k)
				sccp_handle_speeddial(d, k);
			else
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
			return;
		}

		if (strlen(l->adhocNumber) > 0) {
			sccp_feat_adhocDial(d, l);
			return;
		}

		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Line Key press on line %s\n", d->id, (l) ? l->name : "(nil)");
		if ((c = sccp_channel_get_active_locked(d))) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: gotten active channel %d on line %s\n", d->id, c->callid, (l) ? l->name : "(nil)");
			if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Call not in progress. Closing line %s\n", d->id, (l) ? l->name : "(nil)");
				sccp_channel_endcall_locked(c);
				sccp_channel_unlock(c);
				sccp_dev_deactivate_cplane(d);
				return;
			} else {
				if (sccp_channel_hold_locked(c)) {
					sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: call (%d) put on hold on line %s\n", d->id, c->callid, l->name);
				} else {
					sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Hold failed for call (%d), line %s\n", d->id, c->callid, l->name);
				}
			}
			sccp_channel_unlock(c);
		} else {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: no activate channel on line %d\n", d->id, instance);
		}
		int num_channels_on_line=SCCP_RWLIST_GETSIZE(l->channels);
		if (0==num_channels_on_line) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: start new call on line %s\n", DEV_ID_LOG(d), (l) ? l->name : "(nil)");
			sccp_dev_set_activeline(d, l);
			sccp_dev_set_cplane(l, instance, d, 1);
			sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
		} else {
			ringChannel = sccp_channel_find_bystate_on_line_locked(l, SCCP_CHANNELSTATE_RINGING);
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Channel count on line %d = %d", d->id, instance, num_channels_on_line);
			if (ringChannel != NULL) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Answering ringing Channel %d on line %d\n", d->id, ringChannel->callid, instance);
				sccp_channel_answer_locked(d, ringChannel);
				sccp_channel_unlock(ringChannel);
			} else {
				holdChannel = sccp_channel_find_bystate_on_line_locked(l, SCCP_CHANNELSTATE_HOLD);
				if (holdChannel != NULL) {
					if (num_channels_on_line==1) {
						sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Resume channel %d on line %d\n", d->id, holdChannel->callid, instance);
						sccp_dev_set_activeline(d, l);
						sccp_channel_resume_locked(d, holdChannel, TRUE);
						sccp_dev_set_cplane(l, instance, d, 1);
					} else {
						sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Show call plane so user can select held (channel  %d on line %d)\n", d->id, holdChannel->callid, instance);
						sccp_dev_set_activeline(d, l);
						sccp_dev_set_cplane(l, instance, d, 1);
					}
					sccp_channel_unlock(holdChannel);
				}
			}
		}
		break;

	case SKINNY_BUTTONTYPE_SPEEDDIAL:
		k = sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_SPEEDDIAL);
		if (k)
			sccp_handle_speeddial(d, k);
		else
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
		break;

	case SKINNY_BUTTONTYPE_HOLD:
		/* this is the hard hold button. When we are here we are putting on hold the active_channel */
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Hold/Resume Button pressed on line (%d)\n", d->id, instance);
		l = sccp_line_find_byid(d, instance);
		if (!l) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
			l = sccp_dev_get_activeline(d);
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Trying the current line\n", d->id);
			if (!l) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				return;
			}
			sccp_device_lock(d);
			instance = sccp_device_find_index_for_line(d, l->name);
			sccp_device_unlock(d);
		}

		if ((c = sccp_channel_find_bystate_on_line_locked(l, SCCP_CHANNELSTATE_CONNECTED))) {
			sccp_channel_hold_locked(c);
			sccp_channel_unlock(c);
		} else if ((c = sccp_channel_find_bystate_on_line_locked(l, SCCP_CHANNELSTATE_HOLD))) {
			c1 = sccp_channel_get_active_locked(d);
			if (c1) {
				if (c1->state == SCCP_CHANNELSTATE_OFFHOOK)
					sccp_channel_endcall_locked(c1);
				sccp_channel_unlock(c1);
			}
			sccp_channel_resume_locked(d, c, TRUE);
			sccp_channel_unlock(c);
		} else {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No call to resume/hold found on line %d\n", d->id, instance);
		}
		break;

	case SKINNY_BUTTONTYPE_TRANSFER:
		if (!d->transfer) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device\n", d->id);
			break;
		}
		c = sccp_channel_get_active_locked(d);
		if (c) {
			sccp_channel_transfer_locked(c);
			sccp_channel_unlock(c);
		}
		break;

	case SKINNY_BUTTONTYPE_VOICEMAIL:					// Get a new Line and Dial the Voicemail.
		sccp_feat_voicemail(d, instance);
		break;
	case SKINNY_BUTTONTYPE_CONFERENCE:
		ast_log(LOG_NOTICE, "%s: Conference Button is not yet handled. working on implementation\n", d->id);
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

	case SKINNY_BUTTONTYPE_FORWARDALL:					// Call forward all
		if (!d->cfwdall) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: CFWDALL disabled on device\n", d->id);
			sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
			return;
		}
		l = d->currentLine;
		if (!l) {
			if (!instance)
				instance = 1;

			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				return;
			}
		}
		if (l) {
			sccp_feat_handle_callforward(l, d, SCCP_CFWD_ALL);
		}
		break;
	case SKINNY_BUTTONTYPE_FORWARDBUSY:
		if (!d->cfwdbusy) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY disabled on device\n", d->id);
			sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
			return;
		}
		l = d->currentLine;
		if (!l) {
			if (!instance)
				instance = 1;

			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				return;
			}
		}
		if (l) {
			sccp_feat_handle_callforward(l, d, SCCP_CFWD_BUSY);
		}
		break;
	case SKINNY_BUTTONTYPE_FORWARDNOANSWER:
		if (!d->cfwdnoanswer) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER disabled on device\n", d->id);
			sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
			return;
		}
		l = d->currentLine;
		if (!l) {
			if (!instance)
				instance = 1;

			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				return;
			}
		}
		if (l) {
			sccp_feat_handle_callforward(l, d, SCCP_CFWD_NOANSWER);
		}
		break;
	case SKINNY_BUTTONTYPE_CALLPARK:					// Call parking
#ifdef CS_SCCP_PARK
		c = sccp_channel_get_active_locked(d);
		if (!c) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Cannot park while no calls in progress\n", d->id);
			return;
		}
		sccp_channel_park(c);
		sccp_channel_unlock(c);
#else
		sccp_log((DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
		break;

	case SKINNY_BUTTONTYPE_BLFSPEEDDIAL:					//busy lamp field type speeddial
		k = sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT);
		if (k)
			sccp_handle_speeddial(d, k);
		else
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
		break;

	case SKINNY_STIMULUS_GROUPCALLPICKUP:					/*!< pickup feature button */

		if (d->defaultLineInstance > 0) {
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_FEATURE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);

			//sccp_feat_handle_directpickup(l, d->defaultLineInstance, d);
			sccp_channel_newcall(l, d, (char *)ast_pickup_ext(), SKINNY_CALLTYPE_OUTBOUND);	//handle *8
			return;
		}

		/* no default line set, use first line */
		if (!l) {
			l = sccp_line_find_byid(d, 1);
		}
		if (l) {
			//sccp_feat_handle_directpickup(l, 1, d);
			sccp_channel_newcall(d->currentLine, d, (char *)ast_pickup_ext(), SKINNY_CALLTYPE_OUTBOUND); //handle *8
		}

		break;

	default:
		ast_log(LOG_NOTICE, "%s: Don't know how to deal with stimulus %d with Phonetype %s(%d) \n", d->id, stimulus, devicetype2str(d->skinny_type), d->skinny_type);
		break;
	}
}

/*!
 * \brief Handle SpeedDial for Device
 * \param d SCCP Device as sccp_device_t
 * \param k SCCP SpeedDial as sccp_speed_t
 *
 * \lock
 * 	- channel
 */
void sccp_handle_speeddial(sccp_device_t * d, sccp_speed_t * k)
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l;

	int len;

	if (!k || !d || !d->session)
		return;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Speeddial Button (%d) pressed, configured number is (%s)\n", d->id, k->instance, k->ext);
	c = sccp_channel_get_active_locked(d);
	if (c) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: channel state %d\n", DEV_ID_LOG(d), c->state);

		// Channel already in use
		if ((c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK)) {
			len = strlen(c->dialedNumber);
			sccp_copy_string(c->dialedNumber + len, k->ext, sizeof(c->dialedNumber) - len);
			SCCP_SCHED_DEL(sched, c->digittimeout);
			sccp_pbx_softswitch_locked(c);
			sccp_channel_unlock(c);
			return;
		} else if (c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED) {
			// automatically put on hold
			sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: automatically put call %d on hold %d\n", DEV_ID_LOG(d), c->callid, c->state);
			sccp_channel_hold_locked(c);
			sccp_channel_unlock(c);

			l = d->currentLine;
			sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND);
			return;
		}
		// Channel not in use
		sccp_channel_unlock(c);
		sccp_pbx_senddigits(c, k->ext);
	} else {
		// Pull up a channel
		if (d->defaultLineInstance > 0) {
			sccp_log(((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION) + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = d->currentLine;
		}
		if (l) {
			sccp_channel_newcall(l, d, k->ext, SKINNY_CALLTYPE_OUTBOUND);
		}
	}
	ast_free(k);
}

/*!
 * \brief Handle Off Hook Event for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_offhook(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_line_t *l;

	sccp_channel_t *c;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "off-hook"))) {
		return;
	}

	if (d->isAnonymous) {
		sccp_feat_adhocDial(d, GLOB(hotline)->line);
		return;
	}

	if ((c = sccp_channel_get_active_locked(d))) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Taken Offhook with a call (%d) in progess. Skip it!\n", d->id, c->callid);
		sccp_channel_unlock(c);
		return;
	}

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_OFFHOOK;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Taken Offhook\n", d->id);

	/* checking for registerd lines */
	if (!d->configurationStatistic.numberOfLines) {
		ast_log(LOG_NOTICE, "No lines registered on %s for take OffHook\n", s->device->id);
		sccp_dev_displayprompt(d, 0, 0, "No lines registered!", 0);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}
	/* end line check */

	c = sccp_channel_find_bystate_on_device_locked(d, SKINNY_CALLSTATE_RINGIN);

	if (c) {
		/* Answer the ringing channel. */
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Answer channel\n", d->id);
		if (c->owner)
			pbx_channel_lock(c->owner);

		sccp_channel_answer_locked(d, c);

		if (c->owner)
			pbx_channel_unlock(c->owner);
		sccp_channel_unlock(c);
	} else {
		/* use default line if it is set */
		if (d && d->defaultLineInstance > 0) {
			sccp_log((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			l = sccp_dev_get_activeline(d);
		}
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Using line %s\n", d->id, l->name);

		if (l && !sccp_strlen_zero(l->adhocNumber)) {
			sccp_channel_newcall(l, d, l->adhocNumber, SKINNY_CALLTYPE_OUTBOUND);
		} else {
			/* make a new call with no number */
			sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
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
	sccp_moo_t *r;

	if (!d || !d->session)
		return;
	REQ(r, BackSpaceReqMessage);
	r->msg.BackSpaceReqMessage.lel_lineInstance = htolel(line);
	r->msg.BackSpaceReqMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Backspace request on line instance %u, call %u.\n", d->id, line, callid);
}

/*!
 * \brief Handle On Hook Event for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
void sccp_handle_onhook(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_channel_t *c;

	sccp_buttonconfig_t *buttonconfig = NULL;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "on-hook"))) {
		return;
	}

	if (!d->session)
		return;

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_ONHOOK;
	sccp_log((DEBUGCAT_CORE|DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s is Onhook\n", s->device->id);

	/* checking for registerd lines */
	uint8_t numberOfLines = 0;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE)
			numberOfLines++;
	}
	if (!numberOfLines) {
		ast_log(LOG_NOTICE, "No lines registered on %s to put OnHook\n", s->device->id);
		sccp_dev_displayprompt(d, 0, 0, "No lines registered!", 0);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
		return;
	}
	/* end line check */

	/* get the active channel */
	c = sccp_channel_get_active_locked(d);

	if (!c) {
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
		sccp_dev_stoptone(d, 0, 0);
	} else {
		sccp_channel_endcall_locked(c);
		sccp_channel_unlock(c);
	}

	return;
}

/*!
 * \brief Handle On Hook Event for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 * \note this is used just in protocol v3 stuff, it has been included in 0x004A AccessoryStatusMessage
 */
void sccp_handle_headset(sccp_session_t * s, sccp_moo_t * r)
{
	/*
	 * this is used just in protocol v3 stuff
	 * it has been included in 0x004A AccessoryStatusMessage
	 */

	if (!s || !s->device)
		return;

	uint32_t headsetmode = letohl(r->msg.HeadsetStatusMessage.lel_hsMode);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Accessory '%s' is '%s' (%u)\n", DEV_ID_LOG(s->device), accessory2str(SCCP_ACCESSORY_HEADSET), accessorystatus2str(headsetmode), 0);
}

/*!
 * \brief Handle Capabilities for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_moo_t * r)
{
	int i;

	uint8_t codec;

	int astcodec;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "capabilities"))) {
		return;
	}
	uint8_t n = letohl(r->msg.CapabilitiesResMessage.lel_count);

	d->capability = 0;
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Capabilities\n", d->id, n);
	for (i = 0; i < n; i++) {
		codec = letohl(r->msg.CapabilitiesResMessage.caps[i].lel_payloadCapability);
		astcodec = sccp_codec_skinny2ast(codec);
		d->capability |= astcodec;
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP:%6d %-25s AST:%6d %s\n", d->id, codec, codec2str(codec), astcodec, pbx_codec2str(astcodec));
	}
}

/*!
 * \brief Handle Soft Key Template Request Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- device
 */
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_moo_t * r)
{
	uint8_t i;

	sccp_moo_t *r1;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "softkey template"))) {
		return;
	}

	/* ok the device support the softkey map */
	sccp_device_lock(d);

	d->softkeysupport = 1;

	//REQ(r1, SoftKeyTemplateResMessage);
	int arrayLen = ARRAY_LEN(softkeysmap);

//      if(d->inuseprotocolversion < 15){
//              arrayLen = 31; /* fall back to old behaivour */
//      }

	int dummy_len = arrayLen * (sizeof(StationSoftKeyDefinition));

	int hdr_len = sizeof(r->msg.SoftKeyTemplateResMessage);

	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	/* create message */
	r1 = sccp_build_packet(SoftKeyTemplateResMessage, hdr_len + dummy_len + padding);
	r1->msg.SoftKeyTemplateResMessage.lel_softKeyOffset = 0;

	for (i = 0; i < arrayLen; i++) {
		r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;
		r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
		r1->msg.SoftKeyTemplateResMessage.definition[i].lel_softKeyEvent = htolel(i + 1);
		sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", d->id, i, i + 1, label2str(r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1]));
	}

	sccp_device_unlock(d);

	r1->msg.SoftKeyTemplateResMessage.lel_softKeyCount = htolel(arrayLen);
	r1->msg.SoftKeyTemplateResMessage.lel_totalSoftKeyCount = htolel(arrayLen);
	sccp_dev_send(s->device, r1);
}

/*!
 * \brief Handle Set Soft Key Request Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 *
 * \lock
 *	- softKeySetConfig
 */
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "softkeyset request"))) {
		return;
	}

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
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: searching for softkeyset: %s!\n", d->id, d->softkeyDefinition);
		SCCP_LIST_LOCK(&softKeySetConfig);
		SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
			if (!strcasecmp(d->softkeyDefinition, softkeyset->name)) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: using softkeyset: %s!\n", d->id, softkeyset->name);
				d->softKeyConfiguration.modes = softkeyset->modes;
				d->softKeyConfiguration.size = softkeyset->numberOfSoftKeySets;
			}
		}
		SCCP_LIST_UNLOCK(&softKeySetConfig);
	}
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: d->softkeyDefinition=%s!\n", d->id, d->softkeyDefinition);
	/* end softkey definition */

	const softkey_modes *v = d->softKeyConfiguration.modes;

	const uint8_t v_count = d->softKeyConfiguration.size;

	REQ(r1, SoftKeySetResMessage);
	r1->msg.SoftKeySetResMessage.lel_softKeySetOffset = htolel(0);

	/* look for line trnsvm */
	sccp_buttonconfig_t *buttonconfig;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
			if (l) {
				if (sccp_is_nonempty_string(l->trnsfvm))
					trnsfvm = 1;

				if (l->meetme)
					meetme = 1;

				if (sccp_is_nonempty_string(l->meetmenum))
					meetme = 1;

#ifdef CS_SCCP_PICKUP
				if (l->pickupgroup)
					pickupgroup = 1;
#endif
			}
		}
	}

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: softkey count: %d\n", d->id, v_count);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRANSFER        is %s\n", d->id, (d->transfer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: DND             is %s\n", d->id, d->dndFeature.status ? dndmode2str(d->dndFeature.status) : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PRIVATE         is %s\n", d->id, d->privacyFeature.enabled ? "enabled" : "disabled");
#ifdef CS_SCCP_PARK
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PARK            is  %s\n", d->id, (d->park) ? "enabled" : "disabled");
#endif
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDALL         is  %s\n", d->id, (d->cfwdall) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDBUSY        is  %s\n", d->id, (d->cfwdbusy) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: CFWDNOANSWER    is  %s\n", d->id, (d->cfwdnoanswer) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: TRNSFVM/IDIVERT is  %s\n", d->id, (trnsfvm) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: MEETME          is  %s\n", d->id, (meetme) ? "enabled" : "disabled");
#ifdef CS_SCCP_PICKUP
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPGROUP     is  %s\n", d->id, (pickupgroup) ? "enabled" : "disabled");
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: PICKUPEXTEN     is  %s\n", d->id, (d->pickupexten) ? "enabled" : "disabled");
#endif
	for (i = 0; i < v_count; i++) {
		const uint8_t *b = v->ptr;

		uint8_t c, j, cp = 0;

		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "%s: Set[%-2d]= ", d->id, v->id);

		for (c = 0, cp = 0; c < v->count; c++) {
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
			if ((b[c] == SKINNY_LBL_PICKUP) && (!d->pickupexten)) {
				continue;
			}
			if ((b[c] == SKINNY_LBL_GPICKUP) && (!pickupgroup)) {
				continue;
			}
#endif
			if ((b[c] == SKINNY_LBL_PRIVATE) && (!d->privacyFeature.enabled)) {
				continue;
			}
			for (j = 0; j < sizeof(softkeysmap); j++) {
				if (b[c] == softkeysmap[j]) {
					sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) ("%-2d:%-10s ", c, label2str(softkeysmap[j]));
					r1->msg.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[cp] = (j + 1);
					break;
				}
			}
			cp++;
		}

		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) ("\n");
		v++;
		iKeySetCount++;
	};

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE | DEBUGCAT_SOFTKEY)) (VERBOSE_PREFIX_3 "There are %d SoftKeySets.\n", iKeySetCount);

	r1->msg.SoftKeySetResMessage.lel_softKeySetCount = htolel(iKeySetCount);
	r1->msg.SoftKeySetResMessage.lel_totalSoftKeySetCount = htolel(iKeySetCount);	// <<-- for now, but should be: iTotalKeySetCount;

	sccp_dev_send(d, r1);
	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK);
}

/*!
 * \brief Handle Dialed PhoneBook Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_dialedphonebook_message(sccp_session_t * s, sccp_moo_t * r)
{
	/* this is from CCM7 dump */
	sccp_moo_t *r1 = NULL;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "Dialed Phonebook"))) {
		return;
	}

	uint32_t unknown1 = 0;							/* just 4 bits filled */

	uint32_t index = 0;							/* just 28 bits used */

	uint32_t unknown2 = 0;							/* all 32 bits used */

	uint32_t instance = 0;							/* */

	index = letohl(r->msg.DialedPhoneBookMessage.lel_NumberIndex);
	unknown1 = (index | 0xFFFFFFF0) ^ 0xFFFFFFF0;
	index = index >> 4;

	unknown2 = letohl(r->msg.DialedPhoneBookMessage.lel_unknown);		// i don't understand this :)
	instance = letohl(r->msg.DialedPhoneBookMessage.lel_lineinstance);

	// Sending 0x152 Ack Message. Still have to find out the meaning for 0x153
	REQ(r1, DialedPhoneBookAckMessage);
	r1->msg.DialedPhoneBookAckMessage.lel_NumberIndex = r->msg.DialedPhoneBookMessage.lel_NumberIndex;
	r1->msg.DialedPhoneBookAckMessage.lel_lineinstance = r->msg.DialedPhoneBookMessage.lel_lineinstance;
	r1->msg.DialedPhoneBookAckMessage.lel_unknown = r->msg.DialedPhoneBookMessage.lel_unknown;
	r1->msg.DialedPhoneBookAckMessage.lel_unknown2 = 0;
	sccp_dev_send(s->device, r1);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Device sent Dialed PhoneBook Rec.'%u' (%u) dn '%s' (0x%08X) line instance '%d'.\n", DEV_ID_LOG(d), index, unknown1, r->msg.DialedPhoneBookMessage.phonenumber, unknown2, instance);
}

/*!
 * \brief Handle Time/Date Request Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_time_date_req(sccp_session_t * s, sccp_moo_t * r)
{
	time_t timer = 0;

	struct tm *cmtime = NULL;

	sccp_device_t *d = NULL;

	if (!s || (s->fds[0].fd < 0)) {
		ast_log(LOG_ERROR, "(Time Date Request) Session no longer valid\n");
		return;
	}

	if (!(d = s->device)) {
		ast_log(LOG_ERROR, "No valid Device available to handle Time Date Request for");
		return;
	}

	if (s != s->device->session) {
		ast_log(LOG_WARNING, "(Time Date Request) Provided Session and Device Session are not the same!!\n");
		return;
	}

	sccp_moo_t *r1;

	REQ(r1, DefineTimeDate);

	/* modulate the timezone by full hours only */
	timer = time(0) + (s->device->tz_offset * 3600);
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
	sccp_dev_send(s->device, r1);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send date/time\n", s->device->id);

/*
	According to SCCP protocol since version 3,
	the first instance of asking for time and date
	concludes the device registration process.
	This is included even in the minimal subset of device registration commands.
*/
	if (d->registrationState != SKINNY_DEVICE_RS_OK) {
		sccp_dev_set_registered(s->device, SKINNY_DEVICE_RS_OK);
	}
}

/*!
 * \brief Handle KeyPad Button for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- channel
 * 	  - see sccp_device_find_index_for_line()
 * 	  - see sccp_dev_displayprompt()
 * 	  - see sccp_handle_dialtone_locked()
 */
void sccp_handle_keypad_button(sccp_session_t * s, sccp_moo_t * r)
{
	int event;

	uint8_t lineInstance;

	uint32_t callid;

	char resp = '\0';

	int len = 0;

	sccp_channel_t *c = NULL;

	sccp_line_t *l = NULL;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "keypad"))) {
		return;
	}

	event = letohl(r->msg.KeypadButtonMessage.lel_kpButton);
	lineInstance = letohl(r->msg.KeypadButtonMessage.lel_lineInstance);
	callid = letohl(r->msg.KeypadButtonMessage.lel_callReference);

	if (lineInstance)
		l = sccp_line_find_byid(s->device, lineInstance);

	if (l && callid)
		c = sccp_channel_find_byid_locked(callid);

	/* Old phones like 7912 never uses callid
	 * so here we don't have a channel, this way we
	 * should get the active channel on device
	 */
	if (!c) {
		c = sccp_channel_get_active_locked(d);
	}

	if (!c) {
		ast_log(LOG_NOTICE, "Device %s sent a Keypress, but there is no active channel!\n", DEV_ID_LOG(d));
		return;
	}

	l = c->line;
	d = c->device;

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Cisco Digit: %08x (%d) on line %s\n", d->id, event, event, l->name);

	if (event < 10)
		resp = '0' + event;
	else if (event == 14)
		resp = '*';
	else if (event == 15)
		resp = '#';

	/* added PROGRESS to make sending digits possible during progress state (Pavel Troller) */
	if (c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED || c->state == SCCP_CHANNELSTATE_PROGRESS) {
		/* we have to unlock 'cause the senddigit lock the channel */
		sccp_channel_unlock(c);
		sccp_pbx_senddigit(c, resp);
		return;
	}

	if ((c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) || (c->state == SCCP_CHANNELSTATE_GETDIGITS)) {
		len = strlen(c->dialedNumber);
		if (len >= (AST_MAX_EXTENSION - 1)) {
			uint8_t instance;

			instance = sccp_device_find_index_for_line(d, c->line->name);
			sccp_dev_displayprompt(d, instance, c->callid, "No more digits", 5);
		} else {
			c->dialedNumber[len++] = resp;
			c->dialedNumber[len] = '\0';

			/* removing scheduled dial */
			SCCP_SCHED_DEL(sched, c->digittimeout);

			// Overlap Dialing should set display too -FS
			if (c->state == SCCP_CHANNELSTATE_DIALING && c->owner && c->owner->pbx) {
				/* we shouldn't start pbx another time */
				sccp_channel_unlock(c);
				sccp_pbx_senddigit(c, resp);
				return;
			}

			/* as we're not in overlapped mode we should add timeout again */
#ifdef CS_ADV_FEATURES
			int found_match=sccp_pbx_helper(c);
			int digittimeout=GLOB(digittimeout);
			if (2==found_match) {
				digittimeout=GLOB(digittimeout)/2;
			}
			if ((c->digittimeout = sccp_sched_add(sched, digittimeout * 1000, sccp_pbx_sched_dial, c)) < 0) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: Unable to reschedule dialing in '%d' ms\n", digittimeout);
			}
#else
			if ((c->digittimeout = sccp_sched_add(sched, GLOB(digittimeout) * 1000, sccp_pbx_sched_dial, c)) < 0) {
				sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_1 "SCCP: Unable to reschedule dialing in '%d' ms\n", GLOB(digittimeout));
			}
#endif
#ifdef CS_SCCP_PICKUP
			if (!strcmp(c->dialedNumber, ast_pickup_ext()) && (c->state != SCCP_CHANNELSTATE_GETDIGITS)) {
				/* set it to offhook state because the sccp_sk_gpickup function look for an offhook channel */
				c->state = SCCP_CHANNELSTATE_OFFHOOK;
				sccp_channel_unlock(c);
				sccp_sk_gpickup(c->device, c->line, lineInstance, c);
				return;
			}
#endif

			if (GLOB(digittimeoutchar) && GLOB(digittimeoutchar) == resp) {
				c->dialedNumber[len] = '\0';

				SCCP_SCHED_DEL(sched, c->digittimeout);
				/* XXX to prevent keeping the channel locked during waiting, we
				 * unlock it here. But it's crappy to unlock it to relock it after
				 * because it can be removed meantime. */
				sccp_channel_unlock(c);
				// we would hear last keypad stroke before starting all
				sccp_safe_sleep(100);
				// we dial on digit timeout char !
				sccp_channel_lock(c);
				sccp_pbx_softswitch_locked(c);
				sccp_channel_unlock(c);
				return;
			}
			// we dial when helper says it's time to dial !
#ifdef CS_ADV_FEATURES
			if (1==found_match) {
#else
			if (sccp_pbx_helper(c)) {
#endif
				/* XXX to prevent keeping the channel locked during waiting, we
				 * unlock it here. But it's crappy to unlock it to relock it after
				 * because it can be removed meantime. */
				sccp_channel_unlock(c);
				// we would hear last keypad stroke before starting all
				sccp_safe_sleep(100);
				// we dialout if helper says it's time to dial
				sccp_channel_lock(c);
				sccp_pbx_softswitch_locked(c);
				sccp_channel_unlock(c);
				return;
			}
		}
	}
	sccp_handle_dialtone_locked(c);
	sccp_channel_unlock(c);
}

/*!
 * \brief Handle DialTone Without Lock
 * \param c SCCP Channel as sccp_channel_t
 */
void sccp_handle_dialtone_locked(sccp_channel_t * c)
{
	sccp_line_t *l = NULL;

	sccp_device_t *d = NULL;

	int len = 0, len1 = 0;

	uint8_t instance;

	if (!c)
		return;

	if (!(l = c->line))
		return;

	if (!(d = c->device))
		return;

	len = strlen(c->dialedNumber);
	instance = sccp_device_find_index_for_line(d, l->name);
	/* secondary dialtone check */
	len1 = strlen(l->secondary_dialtone_digits);

	/* we check dialtone just in DIALING action
	 * otherwise, you'll get secondary dialtone also
	 * when catching call forward number, meetme room,
	 * etc.
	 * */

	if (c->ss_action != SCCP_SS_DIAL)
		return;

	if (len == 0 && c->state != SCCP_CHANNELSTATE_OFFHOOK) {
		sccp_dev_stoptone(d, instance, c->callid);
		sccp_dev_starttone(d, SKINNY_TONE_INSIDEDIALTONE, instance, c->callid, 0);
	} else if (len == 1) {
		if (c->state != SCCP_CHANNELSTATE_DIALING) {
			sccp_dev_stoptone(d, instance, c->callid);
			sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_DIALING);
		} else {
			sccp_dev_stoptone(d, instance, c->callid);
		}
	}

	if (len1 && len == len1 && !strncmp(c->dialedNumber, l->secondary_dialtone_digits, len1)) {
		/* We have a secondary dialtone */
		sccp_safe_sleep(100);
		sccp_dev_starttone(d, l->secondary_dialtone_tone, instance, c->callid, 0);
	} else if ((len1) && (len == len1 + 1 || (len > 1 && len1 > 1 && len == len1 - 1))) {
		sccp_dev_stoptone(d, instance, c->callid);
	}
}

/*!
 * \brief Handle Soft Key Event for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l = NULL;

	sccp_speed_t *k = NULL;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "softkey event"))) {
		return;
	}

	uint32_t event = letohl(r->msg.SoftKeyEventMessage.lel_softKeyEvent);

	uint32_t lineInstance = letohl(r->msg.SoftKeyEventMessage.lel_lineInstance);

	uint32_t callid = letohl(r->msg.SoftKeyEventMessage.lel_callReference);

	if (!d)
		return;

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
	if (!lineInstance && !callid && event == SKINNY_LBL_NEWCALL) {
		if (d->defaultLineInstance > 0)
			lineInstance = d->defaultLineInstance;
		else
			l = d->currentLine;
	}

	if (lineInstance)
		l = sccp_line_find_byid(s->device, lineInstance);

	if (lineInstance && callid)
		c = sccp_channel_find_byid_locked(callid);

	/* XXX to prevent keeping lock during too long time, unlock it here to let
	 * sccp_sk_* functions relock it. */
	if (c)
		sccp_channel_unlock(c);

	switch (event) {
	case SKINNY_LBL_REDIAL:
		sccp_sk_redial(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_NEWCALL:
		if (d->isAnonymous) {
			sccp_feat_adhocDial(d, GLOB(hotline)->line);		/* with hotline as feature */
		} else if (l) {
			if (strlen(l->adhocNumber) == 0)
				sccp_sk_newcall(d, l, lineInstance, c);
			else {
				sccp_feat_adhocDial(d, l);
			}
		} else {
			k = sccp_dev_speed_find_byindex(d, lineInstance, SCCP_BUTTONTYPE_HINT);
			if (k) {
				sccp_handle_speeddial(d, k);
			} else
				sccp_sk_newcall(d, NULL, 0, NULL);
		}
		break;
	case SKINNY_LBL_CONFRN:
		sccp_sk_conference(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_MEETME:
		sccp_sk_meetme(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_JOIN:
		sccp_sk_join(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_BARGE:
		sccp_sk_barge(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_CBARGE:
		sccp_sk_cbarge(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_HOLD:
		sccp_sk_hold(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_TRANSFER:
		sccp_sk_transfer(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_CFWDALL:
		sccp_sk_cfwdall(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_CFWDBUSY:
		sccp_sk_cfwdbusy(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_CFWDNOANSWER:
		sccp_sk_cfwdnoanswer(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_BACKSPACE:
		sccp_sk_backspace(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_ENDCALL:
		sccp_sk_endcall(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_RESUME:
		sccp_sk_resume(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_ANSWER:
		sccp_sk_answer(d, l, lineInstance, c);
		break;
#ifdef CS_SCCP_PARK
	case SKINNY_LBL_PARK:
		sccp_sk_park(d, l, lineInstance, c);
		break;
#endif
	case SKINNY_LBL_TRNSFVM:
	case SKINNY_LBL_IDIVERT:
		sccp_sk_trnsfvm(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_DND:
		sccp_sk_dnd(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_DIRTRFR:
		sccp_sk_dirtrfr(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_SELECT:
		sccp_sk_select(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_PRIVATE:
		sccp_sk_private(d, l, lineInstance, c);
		break;
#ifdef CS_SCCP_PICKUP
	case SKINNY_LBL_PICKUP:
		sccp_sk_pickup(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_GPICKUP:
		sccp_sk_gpickup(d, l, lineInstance, c);
		break;
	case SKINNY_LBL_INTRCPT:
		sccp_sk_resume(d, l, lineInstance, c);
		break;
#endif
	default:
		ast_log(LOG_WARNING, "Don't know how to handle keypress %d\n", event);
	}
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- channel
 * 	  - see sccp_channel_startmediatransmission()
 * 	  - see sccp_channel_endcall()
 */
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_moo_t * r)
{
	struct sockaddr_in sin;

	struct sockaddr_in us;

	sccp_channel_t *c;

	sccp_device_t *d;

	char ipAddr[16];

	uint32_t status = 0, ipPort = 0, partyID = 0;

	if (!(d = check_session_message_device(s, r, "open receive channel acknowledgement"))) {
		return;
	}
	memset(ipAddr, 0, 16);
	if (d->inuseprotocolversion < 17) {
		ipPort = htons(htolel(r->msg.OpenReceiveChannelAck.lel_portNumber));
		partyID = letohl(r->msg.OpenReceiveChannelAck.lel_passThruPartyId);
		status = letohl(r->msg.OpenReceiveChannelAck.lel_orcStatus);
		memcpy(&ipAddr, &r->msg.OpenReceiveChannelAck.bel_ipAddr, 4);
	} else {
		ipPort = htons(htolel(r->msg.OpenReceiveChannelAck_v17.lel_portNumber));
		partyID = letohl(r->msg.OpenReceiveChannelAck_v17.lel_passThruPartyId);
		status = letohl(r->msg.OpenReceiveChannelAck_v17.lel_orcStatus);
		memcpy(&ipAddr, &r->msg.OpenReceiveChannelAck_v17.bel_ipAddr, 16);
	}
	if (status) {
		ast_log(LOG_ERROR, "Open Receive Channel Failure\n");
		return;
	}

	sin.sin_family = AF_INET;
	if ((d->trustphoneip || d->directrtp) && (!d->nat))
		memcpy(&sin.sin_addr, &ipAddr, sizeof(sin.sin_addr));
	else
		memcpy(&sin.sin_addr, &s->sin.sin_addr, sizeof(sin.sin_addr));
	sin.sin_port = ipPort;

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got OpenChannel ACK.  Status: %d, RemoteIP (%s): %s, Port: %d, PassThruId: %u, Trustphoneip: %s, Directrtp: %s, Natted: %s\n", d->id, status, (d->trustphoneip ? "Phone" : "Connection"), pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), partyID, d->trustphoneip ? "yes" : "no", d->directrtp ? "yes" : "no", d->nat ? "yes" : "no");
	if (status) {
		/* rtp error from the phone */
		ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device error (%d) ! No RTP media available\n", d->id, status);
		return;
	}

	c = sccp_channel_find_bypassthrupartyid_locked(partyID);

	/* prevent a segmentation fault on fast hangup after answer, failed voicemail for example */
	if (c) {								// && c->state != SCCP_CHANNELSTATE_DOWN) {
		if (status) {
			c->rtp.audio.status = 0;
			sccp_channel_unlock(c);
			ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device error (%d) ! No RTP media available\n", DEV_ID_LOG(d), status);
			return;
		}

		/* update status */
		c->rtp.audio.status |= SCCP_RTP_STATUS_RECEIVE;

		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			sccp_channel_unlock(c);
			ast_log(LOG_WARNING, "%s: (OpenReceiveChannelAck) Invalid Number (%d)\n", DEV_ID_LOG(d), c->state);
			return;
		}

		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: STARTING DEVICE RTP TRANSMISSION WITH STATE %s(%d)\n", d->id, sccp_indicate2str(c->state), c->state);

		if (c->rtp.audio.rtp) {
			ast_rtp_set_peer(c->rtp.audio.rtp, &sin);

			ast_rtp_get_us(c->rtp.audio.rtp, &us);
			memcpy(&c->rtp.audio.phone_remote, &us, sizeof(c->rtp.audio.phone_remote));

			sccp_channel_startmediatransmission(c);			/*!< Starting Media Transmission Earlier to fix 2 second delay - Copied from v2 - FS */
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s:%d\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

			/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
			if (c->state == SCCP_CHANNELSTATE_CONNECTED && (c->rtp.audio.status & SCCP_RTP_STATUS_TRANSMIT) && (c->rtp.audio.status & SCCP_RTP_STATUS_RECEIVE)) {
				sccp_ast_setstate(c, AST_STATE_UP);
			}

		} else {
			ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
			sccp_channel_endcall_locked(c);				// FS - 350
		}
		sccp_channel_unlock(c);
	} else {
		if (status) {
			ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device error (%d) ! No RTP media available\n", d->id, status);
		} else {
			ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) No channel with this PassThruId!\n", d->id);
		}
	}
}

/*!
 * \brief Handle Open Multi Media Receive Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- channel
 * 	  - see sccp_dev_send()
 */
void sccp_handle_OpenMultiMediaReceiveAck(sccp_session_t * s, sccp_moo_t * r)
{
	struct sockaddr_in sin;

	sccp_channel_t *c;

	sccp_device_t *d;

	char ipAddr[16];

	uint32_t status = 0, ipPort = 0, partyID = 0;

	if (!(d = check_session_message_device(s, r, "open multimedia receive acknowledgement"))) {
		return;
	}
	memset(ipAddr, 0, 16);
	if (d->inuseprotocolversion < 15) {
		ipPort = htons(htolel(r->msg.OpenMultiMediaReceiveChannelAckMessage.lel_portNumber));
		partyID = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.lel_passThruPartyId);
		status = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.lel_orcStatus);
		memcpy(&ipAddr, &r->msg.OpenMultiMediaReceiveChannelAckMessage.bel_ipAddr, 4);
		memcpy(&sin.sin_addr, &r->msg.OpenMultiMediaReceiveChannelAckMessage.bel_ipAddr, sizeof(sin.sin_addr));
	} else {
		ipPort = htons(htolel(r->msg.OpenMultiMediaReceiveChannelAckMessage_v17.lel_portNumber));
		partyID = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage_v17.lel_passThruPartyId);
		status = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage_v17.lel_orcStatus);
		memcpy(&ipAddr, &r->msg.OpenMultiMediaReceiveChannelAckMessage_v17.bel_ipAddr, 16);
		memcpy(&sin.sin_addr, &r->msg.OpenMultiMediaReceiveChannelAckMessage_v17.bel_ipAddr, sizeof(sin.sin_addr));
	}

	sin.sin_family = AF_INET;
	sin.sin_port = ipPort;

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got OpenMultiMediaReceiveChannelAck.  Status: %d, RemoteIP (%s): %s, Port: %d, PassThruId: %u\n", d->id, status, (d->trustphoneip ? "Phone" : "Connection"), pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), partyID);
	if (status) {
		/* rtp error from the phone */
		ast_log(LOG_ERROR, "%s: (OpenMultiMediaReceiveChannelAck) Device error (%d) ! No RTP media available\n", d->id, status);
		return;
	}

	c = sccp_channel_find_bypassthrupartyid_locked(partyID);
	/* prevent a segmentation fault on fast hangup after answer, failed voicemail for example */
	if (c) {								// && c->state != SCCP_CHANNELSTATE_DOWN) {
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER) {
			sccp_channel_unlock(c);
			return;
		}

		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: STARTING DEVICE RTP TRANSMISSION WITH STATE %s(%d)\n", d->id, sccp_indicate2str(c->state), c->state);
		memcpy(&c->rtp.video.phone, &sin, sizeof(sin));
		if (c->rtp.video.rtp || sccp_rtp_createVideoServer(c)) {
			sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s:%d\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
			ast_rtp_set_peer(c->rtp.video.rtp, &sin);
			if (c->state == SCCP_CHANNELSTATE_CONNECTED)
				sccp_ast_setstate(c, AST_STATE_UP);
		} else {
			ast_log(LOG_ERROR, "%s: Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		}

		sccp_moo_t *r1;

		r1 = sccp_build_packet(MiscellaneousCommandMessage, sizeof(r->msg.MiscellaneousCommandMessage));
		r1->msg.MiscellaneousCommandMessage.lel_conferenceId = htolel(c->callid);
		r1->msg.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(c->passthrupartyid);
		r1->msg.MiscellaneousCommandMessage.lel_callReference = htolel(c->callid);
		r1->msg.MiscellaneousCommandMessage.lel_miscCommandType = htolel(1);	/* videoFastUpdatePicture */
		sccp_dev_send(c->device, r1);

		r1 = sccp_build_packet(Unknown_0x0141_Message, sizeof(r->msg.Unknown_0x0141_Message));
		r1->msg.Unknown_0x0141_Message.lel_conferenceID = htolel(c->callid);
		r1->msg.Unknown_0x0141_Message.lel_passThruPartyId = htolel(c->passthrupartyid);
		r1->msg.Unknown_0x0141_Message.lel_callReference = htolel(c->callid);
		r1->msg.Unknown_0x0141_Message.lel_maxBitRate = htolel(0x00000c80);
		sccp_dev_send(c->device, r1);

		sccp_channel_unlock(c);
	} else {
		ast_log(LOG_ERROR, "%s: No channel with this PassThruId!\n", d->id);
	}
}

/*!
 * \brief Handle Version for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_version(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	sccp_device_t *d;

	if (!(d = check_session_message_device(s, r, "version"))) {
		return;
	}
	REQ(r1, VersionMessage);
	sccp_copy_string(r1->msg.VersionMessage.requiredVersion, d->imageversion, sizeof(r1->msg.VersionMessage.requiredVersion));
	sccp_dev_send(d, r1);
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Sending version number: %s\n", d->id, d->imageversion);
}

/*!
 * \brief Handle Connection Statistics for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "keypad"))) {
		return;
	}
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Statistics from %s callid: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", d->id, r->msg.ConnectionStatisticsRes.DirectoryNumber,
		     letohl(r->msg.ConnectionStatisticsRes.lel_CallIdentifier), letohl(r->msg.ConnectionStatisticsRes.lel_SentPackets), letohl(r->msg.ConnectionStatisticsRes.lel_RecvdPackets), letohl(r->msg.ConnectionStatisticsRes.lel_LostPkts), letohl(r->msg.ConnectionStatisticsRes.lel_Jitter), letohl(r->msg.ConnectionStatisticsRes.lel_latency)
	    );
}

/*!
 * \brief Handle Server Resource Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "server resource"))) {
		return;
	}

	if (!s->ourip.s_addr) {
		ast_log(LOG_ERROR, "%s: Session IP Changed mid flight\n", DEV_ID_LOG(d));
		return;
	}

	if (s->device->session != s) {
		ast_log(LOG_ERROR, "%s: Wrong Session or Session Changed mid flight\n", DEV_ID_LOG(d));
		return;
	}

	/* old protocol function replaced by the SEP file server addesses list */
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Sending servers message\n", DEV_ID_LOG(d));

	REQ(r1, ServerResMessage);
	sccp_copy_string(r1->msg.ServerResMessage.server[0].serverName, pbx_inet_ntoa(s->ourip), sizeof(r1->msg.ServerResMessage.server[0].serverName));
	r1->msg.ServerResMessage.serverListenPort[0] = GLOB(ourport);
	r1->msg.ServerResMessage.serverIpAddr[0] = s->ourip.s_addr;
	sccp_dev_send(d, r1);
}

/*!
 * \brief Handle Config Status Message for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- device
 * 	  - device->buttonconfig
 */
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_moo_t *r1;

	sccp_buttonconfig_t *config = NULL;

	uint8_t lines = 0;

	uint8_t speeddials = 0;

	sccp_device_t *d;

	if (!(d = check_session_message_device(s, r, "Config Status"))) {
		return;
	}

	sccp_device_lock(d);

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == SPEEDDIAL)
			speeddials++;
		else if (config->type == LINE)
			lines++;
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	REQ(r1, ConfigStatMessage);
	sccp_copy_string(r1->msg.ConfigStatMessage.station_identifier.deviceName, s->device->id, sizeof(r1->msg.ConfigStatMessage.station_identifier.deviceName));
	r1->msg.ConfigStatMessage.station_identifier.lel_stationUserId = htolel(0);
	//r1->msg.ConfigStatMessage.station_identifier.lel_stationInstance = htolel(1);
	r1->msg.ConfigStatMessage.station_identifier.lel_stationInstance = htolel(0);
	r1->msg.ConfigStatMessage.lel_numberLines = htolel(lines);
	r1->msg.ConfigStatMessage.lel_numberSpeedDials = htolel(speeddials);

	sccp_device_unlock(d);
	sccp_dev_send(s->device, r1);
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Sending ConfigStatMessage, lines %d, speeddials %d\n", d->id, lines, speeddials);
}

/*!
 * \brief Handle Enbloc Call Messsage (Dial in one block, instead of number by number)
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \lock
 * 	- channel
 */
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	sccp_channel_t *c = NULL;

	sccp_line_t *l = NULL;

	int len = 0;

	if (!(d = check_session_message_device(s, r, "Enbloc Call"))) {
		return;
	}

	if (r && sccp_is_nonempty_string(r->msg.EnblocCallMessage.calledParty)) {
		c = sccp_channel_get_active_locked(d);
		if (c) {
			if ((c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK)) {

				/* for anonymous devices we just want to call the extension defined in hotine->exten -> ignore dialed number -MC */
				if (d->isAnonymous) {
					sccp_channel_unlock(c);
					return;
				}

				len = strlen(c->dialedNumber);
				sccp_copy_string(c->dialedNumber + len, r->msg.EnblocCallMessage.calledParty, sizeof(c->dialedNumber) - len);
				SCCP_SCHED_DEL(sched, c->digittimeout);
				sccp_pbx_softswitch_locked(c);
				sccp_channel_unlock(c);
				return;
			}
			sccp_channel_unlock(c);
			sccp_pbx_senddigits(c, r->msg.EnblocCallMessage.calledParty);
		} else {
			// Pull up a channel
			l = d->currentLine;
			if (l) {
				sccp_channel_newcall(l, d, r->msg.EnblocCallMessage.calledParty, SKINNY_CALLTYPE_OUTBOUND);
			}
		}

	}
}

/*!
 * \brief Handle Forward Status Reques for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	sccp_line_t *l;

	sccp_moo_t *r1 = NULL;

	if (!(d = check_session_message_device(s, r, "Forward Status"))) {
		return;
	}

	uint32_t instance = letohl(r->msg.ForwardStatReqMessage.lel_lineNumber);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Got Forward Status Request.  Line: %d\n", d->id, instance);
	l = sccp_line_find_byid(d, instance);
	if (l)
		sccp_dev_forward_status(l, instance, d);
	else {
		/* speeddial with hint. Sending empty forward message */
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Send Forward Status.  Instance: %d\n", d->id, instance);
		REQ(r1, ForwardStatMessage);
		r1->msg.ForwardStatMessage.lel_status = 0;
		r1->msg.ForwardStatMessage.lel_lineNumber = r->msg.ForwardStatReqMessage.lel_lineNumber;
		sccp_dev_send(d, r1);
	}
}

/*!
 * \brief Handle Feature Status Reques for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	sccp_buttonconfig_t *config = NULL;

	if (!(d = check_session_message_device(s, r, "Feature Status Request"))) {
		return;
	}

	int instance = letohl(r->msg.FeatureStatReqMessage.lel_featureInstance);

	int unknown = letohl(r->msg.FeatureStatReqMessage.lel_unknown);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Unknown = %d \n", d->id, instance, unknown);

#ifdef CS_DYNAMIC_SPEEDDIAL
	/*
	 * the new speeddial style uses feature to display state
	 * unfortunately we dont know how to handle this on other way
	 */
	if ((unknown == 1 && d->inuseprotocolversion >= 15)) {
		sccp_speed_t *k = sccp_dev_speed_find_byindex(d, instance, SCCP_BUTTONTYPE_HINT);

		if (k) {
			sccp_moo_t *r1;

			REQ(r1, FeatureStatAdvancedMessage);
			r1->msg.FeatureStatAdvancedMessage.lel_instance = htolel(instance);
			r1->msg.FeatureStatAdvancedMessage.lel_type = htolel(SKINNY_BUTTONTYPE_BLFSPEEDDIAL);
			r1->msg.FeatureStatAdvancedMessage.lel_status = 0;

			sccp_copy_string(r1->msg.FeatureStatAdvancedMessage.DisplayName, k->name, sizeof(r1->msg.FeatureStatAdvancedMessage.DisplayName));
			sccp_dev_send(d, r1);

			ast_free(k);
			return;
		}
		return;
	}
#endif

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == instance && config->type == FEATURE) {
			sccp_feat_changed(d, config->button.feature.id);
		}
	}
}

/*!
 * \brief Handle Feature Status Request for Session
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 */
void sccp_handle_services_stat_req(sccp_session_t * s, sccp_moo_t * r)
{
	sccp_device_t *d = NULL;

	sccp_moo_t *r1 = NULL;

	sccp_service_t *service = NULL;

	if (!(d = check_session_message_device(s, r, "Service Status Request"))) {
		return;
	}

	int urlIndex = letohl(r->msg.ServiceURLStatReqMessage.lel_serviceURLIndex);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Got ServiceURL Status Request.  Index = %d\n", d->id, urlIndex);

	if ((service = sccp_dev_serviceURL_find_byindex(s->device, urlIndex))) {
		if (s->device->inuseprotocolversion < 7) {
			REQ(r1, ServiceURLStatMessage);
			r1->msg.ServiceURLStatMessage.lel_serviceURLIndex = htolel(urlIndex);
			sccp_copy_string(r1->msg.ServiceURLStatMessage.URL, service->url, strlen(service->url) + 1);
			sccp_copy_string(r1->msg.ServiceURLStatMessage.label, service->label, strlen(service->label) + 1);
		} else {
			int URL_len = strlen(service->url);

			int label_len = strlen(service->label);

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
					memcpy(&buffer[0], service->url, URL_len);
				if (label_len)
					memcpy(&buffer[URL_len + 1], service->label, label_len);
				memcpy(&r1->msg.ServiceURLStatDynamicMessage.dummy, &buffer[0], dummy_len + 2);
			}
		}
		ast_free(service);
		sccp_dev_send(s->device, r1);
	} else {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: serviceURL %d not assigned\n", DEV_ID_LOG(s->device), urlIndex);
	}
}

/*!
 * \brief Handle Feature Action for Device
 * \param d SCCP Device as sccp_device_t
 * \param instance Instance as int
 * \param toggleState as boolean
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
void sccp_handle_feature_action(sccp_device_t * d, int instance, boolean_t toggleState)
{
	sccp_buttonconfig_t *config = NULL;

	sccp_line_t *line = NULL;

	uint8_t status = 0;							/* state of cfwd */

	uint32_t featureStat1 = 0;

	uint32_t featureStat2 = 0;

	uint32_t featureStat3 = 0;

#ifdef CS_DEVSTATE_FEATURE
	int res = 0;

	char buf[254] = "";
#endif

	if (!d) {
		return;
	}

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: instance: %d, toggle: %s\n", d->id, instance, (toggleState) ? "yes" : "no");

	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->instance == instance && config->type == FEATURE) {
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: toggle status from %d", d->id, config->button.feature.status);
			config->button.feature.status = (config->button.feature.status == 0) ? 1 : 0;
			sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 " to %d\n", config->button.feature.status);
			break;
		}

	}

	if (!config || !config->type || config->type != FEATURE) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: Couldn find feature with ID = %d \n", d->id, instance);
		return;
	}

	/* notice: we use this function for request and changing status -> so just change state if toggleState==TRUE -MC */
	char featureOption[255];

	if (config->button.feature.options) {
		sccp_copy_string(featureOption, config->button.feature.options, sizeof(featureOption));
	}

	sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: FeatureID = %d, Option: %s \n", d->id, config->button.feature.id, featureOption);
	switch (config->button.feature.id) {
	case SCCP_FEATURE_PRIVACY:

		if (!d->privacyFeature.enabled)
			break;

		if (!strcasecmp(config->button.feature.options, "callpresent")) {
			uint32_t res = d->privacyFeature.status & SCCP_PRIVACYFEATURE_CALLPRESENT;

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
		}

		break;
	case SCCP_FEATURE_CFWDALL:
		status = SCCP_CFWD_ALL;

		if (!config->button.feature.options || sccp_strlen_zero(config->button.feature.options) || !config->button.feature.status)
			status = SCCP_CFWD_NONE;

		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if (config->type == LINE) {
				line = sccp_line_find_byname_wo(config->button.line.name, FALSE);

				if (line) {
					sccp_line_cfwd(line, d, status, featureOption);
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

		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) (VERBOSE_PREFIX_3 "%s: dndmode %d is %s\n", d->id, d->dndFeature.status, (d->dndFeature.status) ? "on" : "off");
		sccp_dev_check_displayprompt(d);
		sccp_feat_changed(d, SCCP_FEATURE_DND);
		break;
#ifdef CS_SCCP_FEATURE_MONITOR
	case SCCP_FEATURE_MONITOR:
		d->monitorFeature.status = (d->monitorFeature.status) ? 0 : 1;
		sccp_channel_t *channel = sccp_channel_get_active_locked(d);

		sccp_feat_monitor(d, channel);
		sccp_channel_unlock(channel);
		break;
#endif

#ifdef CS_DEVSTATE_FEATURE

		/**
		  * Handling of custom devicestate toggle buttons.
		  */
	case SCCP_FEATURE_DEVSTATE:
		/* Set the appropriate devicestate, toggle it and write to the devstate astdb.. */

		//config->button.feature.status = (config->button.feature.status)?0:1;
		config->button.feature.status = !(config->button.feature.status) ? 0 : 1;
		strncpy(buf, (config->button.feature.status) ? ("INUSE") : ("NOT_INUSE"), sizeof(buf));
		res = pbx_db_put(devstate_astdb_family, config->button.feature.options, buf);

		ast_devstate_changed(ast_devstate_val(buf), "Custom:%s", config->button.feature.options);
		sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: devstate feature change: %s state: %d res: %d\n", DEV_ID_LOG(d), config->button.feature.options, config->button.feature.status, res);
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
		sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_3 "%s: priority feature status: %d, %d, %d, total: %d\n", d->id, featureStat3, featureStat2, featureStat1, d->priFeature.status);
		break;

	default:
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: unknown feature\n", d->id);
		break;

	}

	if (config) {
		sccp_log((DEBUGCAT_FEATURE_BUTTON | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Status: %d\n", d->id, instance, config->button.feature.status);
		sccp_feat_changed(d, config->button.feature.id);
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
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 *
 * \since 20090708
 * \author Federico
 */
void sccp_handle_updatecapabilities_message(sccp_session_t * s, sccp_moo_t * r)
{
	int i;

	uint8_t codec, n;

	int astcodec;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "Update Capabilities"))) {
		return;
	}

	/* resetting capabilities */
	d->capability = 0;

	/* parsing audio caps */
	n = letohl(r->msg.UpdateCapabilitiesMessage.lel_audioCapCount);
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Audio Capabilities\n", DEV_ID_LOG(d), n);

	for (i = 0; i < n; i++) {
		codec = letohl(r->msg.UpdateCapabilitiesMessage.audioCaps[i].lel_payloadCapability);
		astcodec = sccp_codec_skinny2ast(codec);
		d->capability |= astcodec;
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP:%6d %-25s AST:%8d %s\n", DEV_ID_LOG(d), codec, codec2str(codec), astcodec, pbx_codec2str(astcodec));
	}
	/* store our audio capabilities */
//      memset(&d->capabilities.audio, 0, sizeof(audioCap_t) * DeviceMaxCapabilities);
//      memcpy(&d->capabilities.audio, &r->msg.UpdateCapabilitiesMessage.audioCaps, sizeof(audioCap_t) * n);

	/* parsing video caps */
	n = letohl(r->msg.UpdateCapabilitiesMessage.lel_videoCapCount);
	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_ACTION | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device has %d Video Capabilities\n", DEV_ID_LOG(d), n);
	for (i = 0; i < n; i++) {
		codec = letohl(r->msg.UpdateCapabilitiesMessage.videoCaps[i].lel_payloadCapability);
		astcodec = sccp_codec_skinny2ast(codec);
		d->capability |= astcodec;
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP:%6d %-25s AST:%8d %s\n", DEV_ID_LOG(d), codec, codec2str(codec), astcodec, pbx_codec2str(astcodec));
	}
	/* store our video capabilities */
//      memset(&d->capabilities.video, 0, sizeof(videoCap_t) * DeviceMaxCapabilities);
//      memcpy(&d->capabilities.video, &r->msg.UpdateCapabilitiesMessage.videoCaps, sizeof(videoCap_t) * n);
}

/*!
 * \brief Handle Start Media Transmission Acknowledgement
 * \param s SCCP Session as sccp_session_t
 * \param r SCCP Message as sccp_moo_t
 * \since 20090708
 * \author Federico
 */
void sccp_handle_startmediatransmission_ack(sccp_session_t * s, sccp_moo_t * r)
{
	struct sockaddr_in sin;

	sccp_channel_t *c;

	uint32_t status = 0, ipPort = 0, partyID = 0, callID = 0, callID1 = 0;

	sccp_device_t *d = NULL;

	if (!(d = check_session_message_device(s, r, "Start Media Transmission Acknowledgement"))) {
		return;
	}

	ipPort = htons(htolel(r->msg.StartMediaTransmissionAck.lel_portNumber));
	partyID = letohl(r->msg.StartMediaTransmissionAck.lel_passThruPartyId);
	status = letohl(r->msg.StartMediaTransmissionAck.lel_smtStatus);
	callID = letohl(r->msg.StartMediaTransmissionAck.lel_callReference);
	callID1 = letohl(r->msg.StartMediaTransmissionAck.lel_callReference1);

	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr, &r->msg.StartMediaTransmissionAck.bel_ipAddr, sizeof(sin.sin_addr));
	sin.sin_port = ipPort;

	c = sccp_channel_find_bypassthrupartyid_locked(partyID);
	if (!c) {
		ast_log(LOG_WARNING, "%s: Channel with passthrupartyid %u not found, please report this to developer\n", DEV_ID_LOG(d), partyID);
		return;
	}
	if (status) {
		ast_log(LOG_WARNING, "%s: Error while opening MediaTransmission. Ending call\n", DEV_ID_LOG(d));
		sccp_channel_endcall_locked(c);
		sccp_channel_unlock(c);
		return;
	}

	/* update status */
	c->rtp.audio.status &= ~SCCP_RTP_STATUS_PROGRESS_TRANSMIT;
	c->rtp.audio.status |= SCCP_RTP_STATUS_TRANSMIT;
	/* indicate up state only if both transmit and receive is done - this should fix the 1sek delay -MC */
	if (c->state == SCCP_CHANNELSTATE_CONNECTED && (c->rtp.audio.status & SCCP_RTP_STATUS_TRANSMIT) && (c->rtp.audio.status & SCCP_RTP_STATUS_RECEIVE)) {
		sccp_ast_setstate(c, AST_STATE_UP);
	}

	sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Got StartMediaTranmission ACK.  Status: %d, RemoteIP: %s, Port: %d, CallId %u (%u), PassThruId: %u\n", DEV_ID_LOG(d), status, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), callID, callID1, partyID);
	//ast_cond_signal(&c->rtp.audio.convar);
	sccp_channel_unlock(c);
}
