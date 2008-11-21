/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */
#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_pbx.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_socket.h"
#include "sccp_features.h"

#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#endif

#ifndef ASTERISK_CONF_1_6
struct ast_ha {
        /* Host access rule */
        struct in_addr netaddr;
        struct in_addr netmask;
        int sense;
        struct ast_ha *next;
};
#endif

void sccp_handle_alarm(sccp_session_t * s, sccp_moo_t * r) {
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Alarm Message: Severity: %s (%d), %s [%d/%d]\n", skinny_alarm2str(letohl(r->msg.AlarmMessage.lel_alarmSeverity)), letohl(r->msg.AlarmMessage.lel_alarmSeverity), r->msg.AlarmMessage.text, letohl(r->msg.AlarmMessage.lel_parm1), letohl(r->msg.AlarmMessage.lel_parm2));
}

void sccp_handle_unknown_message(sccp_session_t * s, sccp_moo_t * r) {
	uint32_t mid = letohl(r->lel_messageId);
	
	if (GLOB(debug))
		ast_log(LOG_WARNING, "Unhandled SCCP Message: %d - %s with length %d\n", mid, sccpmsg2str(mid), r->length);
		
	sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET)?r->length:SCCP_MAX_PACKET);
}

void sccp_handle_register(sccp_session_t * s, sccp_moo_t * r) {
	pthread_attr_t 	attr;
	sccp_device_t 	* d;
	btnlist 		btn[StationMaxButtonTemplateSize];
	sccp_line_t 		*l, *lines_last = NULL;
	sccp_moo_t 		* r1;
	uint8_t i = 0, line_count = 0;
	struct ast_hostent	ahp;
	struct hostent		*hp;
	struct sockaddr_in sin;
	sccp_hostname_t		*permithost;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif
	sccp_buttonconfig_t	* buttonconfig;

	memset(&btn, 0 , sizeof(btn));

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: is registering, Instance: %d, Type: %s (%d), Version: %d\n",
		r->msg.RegisterMessage.sId.deviceName,
		letohl(r->msg.RegisterMessage.sId.lel_instance),
		skinny_devicetype2str(letohl(r->msg.RegisterMessage.lel_deviceType)),
		letohl(r->msg.RegisterMessage.lel_deviceType),
		r->msg.RegisterMessage.protocolVer);

	/* ip address range check */
	if (GLOB(ha) && !ast_apply_ha(GLOB(ha), &s->sin)) {
		REQ(r1, RegisterRejectMessage);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Rejecting device: Ip address denied\n", r->msg.RegisterMessage.sId.deviceName);
		sccp_copy_string(r1->msg.RegisterRejectMessage.text, "Device ip not authorized", sizeof(r1->msg.RegisterRejectMessage.text));
		sccp_session_send(s, r1);
		return;
	}

	d = sccp_device_find_byid(r->msg.RegisterMessage.sId.deviceName);

	if (!d) {
		REQ(r1, RegisterRejectMessage);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Rejecting device: not found\n", r->msg.RegisterMessage.sId.deviceName);
		sccp_copy_string(r1->msg.RegisterRejectMessage.text, "Unknown Device", sizeof(r1->msg.RegisterRejectMessage.text));
		sccp_session_send(s, r1);
		return;
	} else if (d->ha && !ast_apply_ha(d->ha, &s->sin)) {
		i = 1;
		permithost = d->permithost;
		while (i && permithost) {
			if ((hp = ast_gethostbyname(permithost->name, &ahp))) {
				memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
				if (s->sin.sin_addr.s_addr == sin.sin_addr.s_addr) {
					i = 0;
				} else {
#ifdef ASTERISK_CONF_1_2					
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: device ip address does not match the permithost = %s (%s)\n", r->msg.RegisterMessage.sId.deviceName, permithost->name, ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr));
#else					
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: device ip address does not match the permithost = %s (%s)\n", r->msg.RegisterMessage.sId.deviceName, permithost->name, ast_inet_ntoa(sin.sin_addr));
#endif
				}
			} else {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Invalid address resolution for permithost = %s\n", r->msg.RegisterMessage.sId.deviceName, permithost->name);
			}
			permithost = permithost->next;
		}
		if (i) {
			REQ(r1, RegisterRejectMessage);
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Rejecting device: Ip address denied\n", r->msg.RegisterMessage.sId.deviceName);
			sccp_copy_string(r1->msg.RegisterRejectMessage.text, "Device ip not authorized", sizeof(r1->msg.RegisterRejectMessage.text));
			sccp_session_send(s, r1);
			return;
		}
	}

	sccp_device_lock(d);

	/* test the localnet to understand if the device is behind NAT */
	if (GLOB(localaddr) && ast_apply_ha(GLOB(localaddr), &s->sin)) {
		/* ok the device is natted */
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Device is behind NAT. We will set externip or externhost for the RTP stream \n", r->msg.RegisterMessage.sId.deviceName);
		d->nat = 1;
	}

	if (d->session) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Device is doing a re-registration!\n", d->id);
	}
#ifdef ASTERISK_CONF_1_2	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", d->id, s->fd, ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", d->id, s->fd, ast_inet_ntoa(s->sin.sin_addr));
#endif
	s->device = d;
	d->skinny_type = letohl(r->msg.RegisterMessage.lel_deviceType);
	d->session = s;
	s->lastKeepAlive = time(0);
	d->mwilight = 0;
	d->protocolversion = r->msg.RegisterMessage.protocolVer;
	sccp_device_unlock(d);

	/* pre-attach lines. We will wait for button template req if the phone does support it */
	sccp_dev_build_buttontemplate(d, btn);

	line_count = 0;
	/* count the available lines on the phone */
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if ( (btn[i].type == SKINNY_BUTTONTYPE_LINE) || (btn[i].type == SKINNY_BUTTONTYPE_MULTI) )
			line_count++;
		else if (btn[i].type == SKINNY_BUTTONTYPE_UNUSED)
			break;
	}
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Phone available lines %d\n", d->id, line_count);
	i = 0;

	buttonconfig = d->buttonconfig;
	while(buttonconfig){
		ast_log(LOG_WARNING, "%s: Found buttontype: %s\n", d->id, buttonconfig->type);
		
		if(!strcasecmp(buttonconfig->type, "line") && !ast_strlen_zero(buttonconfig->button.line.name)){
			l = sccp_line_find_byname(buttonconfig->button.line.name);
			if (!l) {
				ast_log(LOG_ERROR, "%s: Failed to autolog into %s: Couldn't find line %s\n", d->id, buttonconfig->button.line.name, buttonconfig->button.line.name);
				i++;
				buttonconfig = buttonconfig->next;
				continue;
			}

			if (l->device) {
				ast_log(LOG_WARNING, "%s: Line %s aready attached to device %s\n", d->id, l->name, l->device->id);
				i++;
				buttonconfig = buttonconfig->next;
				continue;
			}

			ast_log(LOG_WARNING, "%s: Attaching line %s with instance %d to this device\n", d->id, l->name, buttonconfig->instance);
			if (buttonconfig->instance == line_count) {
				ast_log(LOG_WARNING, "%s: Failed to autolog into %s: Max available lines phone limit reached %s\n", d->id, buttonconfig->button.line.name, buttonconfig->button.line.name);
				buttonconfig = buttonconfig->next;
				continue;
			}
			ast_mutex_lock(&l->lock);
			ast_mutex_lock(&d->lock);
			
			l->device = d;
			l->instance = buttonconfig->instance;
			l->mwilight = 0;
			/* I want it in the right order */
			if (!d->lines)
				d->lines = l;
			if (!lines_last){
				lines_last = l;
				d->currentLine = l;				//set to default line
			}else {
				l->prev_on_device = lines_last;
				lines_last->next_on_device = l;
				lines_last = l;
			}
			ast_mutex_unlock(&l->lock);
			ast_mutex_unlock(&d->lock);
			/* notify the line is on */
			sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_ONHOOK, NULL);
		}
		buttonconfig = buttonconfig->next;

	}
	

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Ask the phone to send keepalive message every %d seconds\n", d->id, (d->keepalive ? d->keepalive : GLOB(keepalive)) );
	REQ(r1, RegisterAckMessage);

	sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, r->length);
	
	if(r->length < 68) {
		// registration request with protocol 0 version structure. 
		r1->msg.RegisterAckMessage.protocolVer = 0;
		// we should tell the device which is the highest protocol version we support but on some messages we support only version 0 :( -FS
		// r1->msg.RegisterAckMessage.protocolVer = GLOB(protocolversion);	
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: asked our protocol capability (%d) but we answered (0) for compatibility.\n", d->id, GLOB(protocolversion));		
	} else if(r->msg.RegisterMessage.protocolVer > GLOB(protocolversion)) {
		r1->msg.RegisterAckMessage.protocolVer = GLOB(protocolversion);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: asked for protocol version (%d): our capability is (%d).\n", d->id, r->msg.RegisterMessage.protocolVer, GLOB(protocolversion));
	} else {
		r1->msg.RegisterAckMessage.protocolVer = r->msg.RegisterMessage.protocolVer;
	}
	
	r1->msg.RegisterAckMessage.lel_keepAliveInterval = htolel( (d->keepalive ? d->keepalive : GLOB(keepalive)) );
	r1->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel( (d->keepalive ? d->keepalive : GLOB(keepalive)) );

	memcpy(r1->msg.RegisterAckMessage.dateTemplate, GLOB(date_format), sizeof(r1->msg.RegisterAckMessage.dateTemplate));
	sccp_session_send(s, r1);
	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);
	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_OK);

	/* clean up the device state */
	sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
	sccp_dev_clearprompt(d, 0, 0);

	/* check for dnd and cfwd status */
	sccp_dev_dbget(d);

	sccp_dev_sendmsg(d, CapabilitiesReqMessage);

	/* starting thread for monitored line status poll */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ast_pthread_create(&d->postregistration_thread, &attr, sccp_dev_postregistration, d)) {
		ast_log(LOG_WARNING, "%s: Unable to create thread for monitored status line poll. %s\n", d->id, strerror(errno));
	}
}

void sccp_handle_unregister(sccp_session_t * s, sccp_moo_t * r) {
	sccp_moo_t 		* r1;
	sccp_device_t 	* d = s->device;

	if (!s || (s->fd < 0) )
		return;

	/* we don't need to look for active channels. the phone does send unregister only when there are no channels */
	REQ(r1, UnregisterAckMessage);
  	r1->msg.UnregisterAckMessage.lel_status = SKINNY_UNREGISTERSTATUS_OK;
	sccp_session_send(s, r1);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: unregister request sent\n", DEV_ID_LOG(d));

	if (d)
		sccp_dev_set_registered(d, SKINNY_DEVICE_RS_NONE);
	sccp_session_close(s);

}

static uint8_t sccp_activate_hint(sccp_device_t *d, sccp_speed_t *k) {
	sccp_hint_t 	*h;
	sccp_line_t 	*l;
	sccp_list_t	*hintList;
	char 		hint[256] = "", hint_dialplan[256] = "";
	char 		*splitter, *hint_exten, *hint_context;

	if (ast_strlen_zero(k->hint))
		return 0;

	memset(&hint_dialplan, 0, sizeof(hint_dialplan));

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Parsing speedial hint %s for speedial: %s,%s\n", d->id, k->hint, k->ext, k->name);
	sccp_copy_string(hint, k->hint, sizeof(k->hint));
	splitter = hint;
	hint_exten = strsep(&splitter, "@");
	if (hint_exten)
		ast_strip(hint_exten);
	hint_context = splitter;
	if (hint_context)
		ast_strip(hint_context);
	else
		hint_context = GLOB(context);
#ifdef CS_AST_HAS_NEW_HINT
	ast_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
#else
	ast_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, hint_context, hint_exten);
#endif
	if (ast_strlen_zero(hint_dialplan)) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No hint configuration in the dialplan %s for exten: %s and context: %s\n", d->id, hint_dialplan, hint_exten, hint_context);
		return 0;
	}

	h = sccp_hint_make(d, k->instance);
	if (!h) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Error creating speeddial hint %s for extension %s and context: %s\n", d->id, hint_dialplan, hint_exten, hint_context);
		return 0;
	}

	if ( strchr(hint_dialplan,'&') || strncasecmp(hint_dialplan,"SCCP",4) ) {
		/* asterisk style hint system */
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Configuring asterisk (no sccp features) hint %s for exten: %s and context: %s\n", d->id, hint_dialplan, hint_exten, hint_context);
		h->hintid = ast_extension_state_add(hint_context, hint_exten, sccp_hint_state, h);
		if (h->hintid > -1) {
			sccp_copy_string(h->exten, hint_exten, sizeof(h->exten));
			sccp_copy_string(h->context, hint_context, sizeof(h->context));
			hintList = malloc(sizeof(sccp_list_t));
			hintList->data = h;
			hintList->next = d->hints;
			if(hintList->next)
				hintList->next->prev = hintList;
			d->hints = hintList;
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Added hint (ASTERISK), extension %s@%s, device %s\n", d->id, hint_exten, hint_context, hint_dialplan);
			return 1;
		}
		/* error */
		free(h);
		h = NULL;
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Error adding hint (ASTERISK) for extension %s@%s and device %s\n", d->id, hint_exten, hint_context, hint_dialplan);
		return 0;
	}

	/* SCCP channels hint system. Push */
	splitter = hint_dialplan;
	strsep(&splitter, "/");
	sccp_copy_string(hint_dialplan, splitter, sizeof(hint_dialplan));
	/* if (hint_dialplan){ */
		ast_strip(hint_dialplan);
		l = sccp_line_find_byname(hint_dialplan);
	/* } */
	if (!l) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Error adding hint (SCCP) for line: %s. The line does not exist!\n", d->id, hint_dialplan);
		free(h);
		return 0;
	}
	hintList = malloc(sizeof(sccp_list_t));
	hintList->data = h;
	hintList->next = l->hints;
	if(hintList->next)
		hintList->next->prev = hintList;
	
	l->hints = hintList;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Added hint (SCCP) on line %s, extension %s, context: %s\n", d->id, l->name, hint_exten, hint_context);

	return 1;
}

static btnlist *sccp_make_button_template(sccp_device_t * d) {
	int i;
	sccp_line_t 		*l = NULL;
	btnlist 		*btn;
	sccp_buttonconfig_t	* buttonconfig;



	btn = malloc(sizeof(btnlist)*StationMaxButtonTemplateSize);
	if (!btn)
		return NULL;

	memset(btn, 0 , sizeof(btnlist)*StationMaxButtonTemplateSize);
	sccp_dev_build_buttontemplate(d, btn);

	sccp_globals_lock(devices_lock);
	sccp_globals_lock(lines_lock);
	sccp_device_lock(d);

	buttonconfig = d->buttonconfig;
	
	while(buttonconfig){
		i = buttonconfig->instance-1;
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Button %d type: %s\n", d->id, buttonconfig->instance, buttonconfig->type);
		
		btn[i].instance = buttonconfig->instance;
		if(!strcasecmp(buttonconfig->type, "line") && !ast_strlen_zero(buttonconfig->button.line.name)){
			l = d->lines;
			while(l){
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: button instance %d\n", d->id, buttonconfig->instance);
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: line instance %d\n", d->id, l->instance);
				if(buttonconfig->instance == l->instance){
					l->device = d;	
					btn[i].type = SCCP_BUTTONTYPE_LINE;
					btn[i].ptr = l;
					sccp_log(10)(VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = LINE (%s), temporary instance (%d)\n", d->id, i+1, l->name, l->instance);
					break;	
				}
				l = l->next_on_device;
			}
			
		} else if(!strcasecmp(buttonconfig->type, "empty")){
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = empty\n", d->id, buttonconfig->instance);
			btn[i].type = SKINNY_BUTTONTYPE_UNUSED;
				
		} else if(!strcasecmp(buttonconfig->type, "serviceurl")) {
			btn[i].type = SKINNY_BUTTONTYPE_SERVICEURL;
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Configured Phone Button [%.2d] = %s (%s) URL: %s\n", d->id, buttonconfig->instance, "ServiceURL" ,buttonconfig->button.serviceurl.label, buttonconfig->button.serviceurl.URL);
			
		} else if(!strcasecmp(buttonconfig->type, "speeddial") && !ast_strlen_zero(buttonconfig->button.speeddial.label)){
			if (!ast_strlen_zero(buttonconfig->button.speeddial.hint))
				btn[i].type = SCCP_BUTTONTYPE_HINT;
				//btn[i].type = SCCP_BUTTONTYPE_SPEEDDIAL;
			else
				btn[i].type = SCCP_BUTTONTYPE_SPEEDDIAL;

		} else if(!strcasecmp(buttonconfig->type, "feature") && !ast_strlen_zero(buttonconfig->button.feature.label)){
			btn[i].type = SKINNY_BUTTONTYPE_FEATURE;
		}

		
		buttonconfig = buttonconfig->next;	
	}
		
		sccp_device_unlock(d);
	sccp_globals_unlock(lines_lock);
	sccp_globals_unlock(devices_lock);
	return btn;
}

void sccp_handle_button_template_req(sccp_session_t * s, sccp_moo_t * r) {
	btnlist *btn;
	sccp_device_t * d = s->device;
	int i;
	sccp_moo_t * r1;

	if (!d)
		return;

	if (d->registrationState != SKINNY_DEVICE_RS_OK) {
		ast_log(LOG_WARNING, "%s: Received a button template request from unregistered device\n", d->id);
		sccp_session_close(s);
		return;
	}

		btn = sccp_make_button_template(d);
		if (!btn) {
			ast_log(LOG_ERROR, "%s: No memory allocated for button template\n", d->id);
			sccp_session_close(s);
			return;
		}

	REQ(r1, ButtonTemplateMessage);
	for (i = 0; i < StationMaxButtonTemplateSize ; i++) {
		//if (btn[i].type == SCCP_BUTTONTYPE_HINT || btn[i].type == SCCP_BUTTONTYPE_LINE) {
		if (btn[i].type == SCCP_BUTTONTYPE_LINE) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_LINE;
			r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;
		} else if (btn[i].type == SCCP_BUTTONTYPE_SPEEDDIAL) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SPEEDDIAL;
			r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;
		} else if (btn[i].type == SCCP_BUTTONTYPE_HINT) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_LINE;
			r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;
		} else if (btn[i].type == SKINNY_BUTTONTYPE_SERVICEURL) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_SERVICEURL;
			r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;	
		} else if (btn[i].type == SKINNY_BUTTONTYPE_FEATURE) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_FEATURE;
			r1->msg.ButtonTemplateMessage.definition[i].instanceNumber = btn[i].instance;	
		} else if (btn[i].type == SKINNY_BUTTONTYPE_MULTI) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_DISPLAY;
		} else if (btn[i].type == SKINNY_BUTTONTYPE_UNUSED) {
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = SKINNY_BUTTONTYPE_UNDEFINED;
		} else
			r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition = btn[i].type;

		if (r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition != SKINNY_BUTTONTYPE_UNDEFINED) {
			r1->msg.ButtonTemplateMessage.lel_buttonCount++;
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Button Template [%.2d] = %s (%d), instance %d\n", d->id, i+1, skinny_buttontype2str(r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition), r1->msg.ButtonTemplateMessage.definition[i].buttonDefinition,r1->msg.ButtonTemplateMessage.definition[i].instanceNumber);
		}
	}

	r1->msg.ButtonTemplateMessage.lel_buttonOffset = htolel(0);
	r1->msg.ButtonTemplateMessage.lel_buttonCount = htolel(r1->msg.ButtonTemplateMessage.lel_buttonCount);
	/* buttonCount is already in a little endian format so don't need to convert it now */
	r1->msg.ButtonTemplateMessage.lel_totalButtonCount = r1->msg.ButtonTemplateMessage.lel_buttonCount;
	sccp_dev_send(d, r1);
	free(btn);
}

void sccp_handle_line_number(sccp_session_t * s, sccp_moo_t * r) {
	uint8_t lineNumber = letohs(r->msg.LineStatReqMessage.lel_lineNumber);
	sccp_line_t * l = NULL;
	sccp_moo_t * r1;
	sccp_device_t * d;
	sccp_speed_t * k = NULL;

	if (!s)
		return;
	d = s->device;
	if (!d)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Configuring line number %d\n", d->id, lineNumber);
	l = sccp_line_find_byid(d, lineNumber);
	/* if we find no regular line - it can be a speedial with hint */
	if (!l)
		k = sccp_dev_speed_find_byindex(d, lineNumber, SKINNY_BUTTONTYPE_LINE);


	REQ(r1, LineStatMessage);
	if (!l && !k) {
		ast_log(LOG_ERROR, "%s: requested a line configuration for unknown line %d\n", s->device->id, lineNumber);
		r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);
		sccp_dev_send(s->device, r1);
		return;
	}
	r1->msg.LineStatMessage.lel_lineNumber = htolel(lineNumber);

	sccp_copy_string(r1->msg.LineStatMessage.lineDirNumber, ((l) ? l->name : k->name), sizeof(r1->msg.LineStatMessage.lineDirNumber));
	sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, ((l) ? l->description : k->name), sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	sccp_copy_string(r1->msg.LineStatMessage.lineDisplayName, ((l) ? l->label : k->name), sizeof(r1->msg.LineStatMessage.lineDisplayName));
	sccp_dev_send(d, r1);

	/* force the forward status message. Some phone does not request it registering */
	if (l) {
		sccp_dev_forward_status(l);
	}
	/* remove speedial if present */
	if(k){
		sccp_log(3)(VERBOSE_PREFIX_3 "%s: line is hint for %s\n", s->device->id, k->hint);
		if(!ast_strlen_zero(k->hint)){
			sccp_log(3)(VERBOSE_PREFIX_3 "%s: activate hint for Button %d\n", s->device->id, lineNumber);
			sccp_activate_hint(d,k);
		}
		free(k);
	}	

}

void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_moo_t * r) {
	sccp_buttonconfig_t	*buttonconfig;
	//sccp_speed_t 		*k = s->device->speed_dials;
	sccp_moo_t 		*r1;
	int instance = letohl(r->msg.SpeedDialStatReqMessage.lel_speedDialNumber);

	sccp_log(3)(VERBOSE_PREFIX_3 "%s: Speed Dial Request for Button %d\n", s->device->id, instance);

	REQ(r1, SpeedDialStatMessage);
	r1->msg.SpeedDialStatMessage.lel_speedDialNumber = htolel(instance);

	//k = sccp_dev_speed_find_byindex(s->device, wanted, SKINNY_BUTTONTYPE_SPEEDDIAL);
	buttonconfig = s->device->buttonconfig;
	while (buttonconfig) {
		sccp_log(99)(VERBOSE_PREFIX_3 "%s: Button %d is %s\n", s->device->id, buttonconfig->instance, buttonconfig->type);
		if(buttonconfig->instance == instance && !strcasecmp(buttonconfig->type, "speeddial")){
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: found name: %s, ext: %s, index: %d\n", s->device->id, buttonconfig->button.speeddial.label, buttonconfig->button.speeddial.ext, buttonconfig->instance);
			sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDirNumber, buttonconfig->button.speeddial.ext, sizeof(r1->msg.SpeedDialStatMessage.speedDialDirNumber));
			sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDisplayName, buttonconfig->button.speeddial.label, sizeof(r1->msg.SpeedDialStatMessage.speedDialDisplayName));
			break;
		}
		buttonconfig = buttonconfig->next;
	}
	if(!buttonconfig)
		sccp_log(3)(VERBOSE_PREFIX_3 "%s: speeddial %d not assigned\n", DEV_ID_LOG(s->device), instance);
// 	if (k) {
// 		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDirNumber, k->ext, sizeof(r1->msg.SpeedDialStatMessage.speedDialDirNumber));
// 		sccp_copy_string(r1->msg.SpeedDialStatMessage.speedDialDisplayName, k->name, sizeof(r1->msg.SpeedDialStatMessage.speedDialDisplayName));
// 	} else {
// 		sccp_log(3)(VERBOSE_PREFIX_3 "%s: speeddial %d not assigned\n", DEV_ID_LOG(s->device), wanted);
// 	}

	sccp_dev_send(s->device, r1);
}

void sccp_handle_stimulus(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t 		*d = s->device;
	sccp_line_t * l;
	sccp_speed_t * k;
	sccp_channel_t * c, * c1;
	uint8_t stimulus;
	uint8_t instance;
	int len = 0;

	if (!d || !r)
		return;

	k = d->speed_dials;
	stimulus = letohl(r->msg.StimulusMessage.lel_stimulus);
	instance = letohl(r->msg.StimulusMessage.lel_stimulusInstance);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got stimulus=%s (%d) for instance=%d\n", d->id, skinny_stimulus2str(stimulus), stimulus, instance);

	if (!instance) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Instance 0 is not a valid instance. Trying the active line %d\n", d->id, instance);
		l = sccp_dev_get_activeline(d);
		if (!l) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line found\n", d->id);
			return;
		}
		instance = l->instance;
	}
	
	switch (stimulus) {

		case SKINNY_BUTTONTYPE_LASTNUMBERREDIAL: // We got a Redial Request
			if (ast_strlen_zero(d->lastNumber))
				return;
			c = sccp_channel_get_active(d);
			if (c) {
				sccp_channel_lock(c);
				if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
					sccp_copy_string(c->dialedNumber, d->lastNumber, sizeof(d->lastNumber));
					c->digittimeout = time(0)+1;
					sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
					sccp_channel_unlock(c);
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Redial the number %s\n", d->id, d->lastNumber);
				} else {
					sccp_channel_unlock(c);
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: Redial ignored as call in progress\n", d->id);
				}
			} else {
				l = d->currentLine;
				if (l) {
					sccp_channel_newcall(l, d->lastNumber, SKINNY_CALLTYPE_OUTBOUND);
				}
			}
			break;

		case SKINNY_BUTTONTYPE_LINE: // We got a Line Request
			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: No line for instance %d. Looking for a speedial with hint\n", d->id, instance);
				k = sccp_dev_speed_find_byindex(d, instance, SKINNY_BUTTONTYPE_LINE);
				if (k){
					sccp_handle_speeddial(d, k);
					free(k);
				}else
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
				return;
			}
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Line Key press on line %s\n", d->id, (l) ? l->name : "(nil)");
			if ( (c = sccp_channel_get_active(d)) ) {			
			    if(c->state != SCCP_CHANNELSTATE_CONNECTED)
			    {
				    sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call not in progress. Closing line %s\n", d->id, (l) ? l->name : "(nil)");
				    sccp_channel_endcall(c);
				    return;
			    }
			    else
			    {
				    sccp_log(1)(VERBOSE_PREFIX_3 "%s: Trying to put on hold the active call (%d) on line %s\n", d->id, c->callid, (l) ? l->name : "(nil)");
			    	    /* there is an active call, let's put it on hold first */
				    if (!sccp_channel_hold(c)) {
						sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hold failed on call (%d), line %s\n", d->id, c->callid, (l) ? l->name : "(nil)");
						return;
				    }
			    }
			
			}
			sccp_dev_set_activeline(l);
			sccp_dev_set_cplane(l,1);
			if (!l->channelCount)
				sccp_channel_newcall(l, NULL, SKINNY_CALLTYPE_OUTBOUND);
			break;

		case SKINNY_BUTTONTYPE_SPEEDDIAL:
			k = sccp_dev_speed_find_byindex(d, instance, SKINNY_BUTTONTYPE_SPEEDDIAL);
			sccp_handle_speeddial(d, k);

// 			buttonconfig = s->device->buttonconfig;
// 			while (buttonconfig) {
// 				sccp_log(99)(VERBOSE_PREFIX_3 "%s: Button %d is %s\n", s->device->id, buttonconfig->instance, buttonconfig->type);
// 				if(buttonconfig->instance == instance && !strcasecmp(buttonconfig->type, "speeddial")){
// 					k = malloc(sizeof(sccp_speed_t));
// 					k->instance = instance;
// 					k->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
// 					sccp_copy_string(k->name, buttonconfig->button.speeddial.label, sizeof(k->name));
// 					sccp_copy_string(k->ext, buttonconfig->button.speeddial.ext, sizeof(k->ext));
// 					
// 					sccp_handle_speeddial(d, k);
// 					free(k);
// 					break;
// 				}
// 				buttonconfig = buttonconfig->next;
// 			}
			if(!k){
				sccp_log(3)(VERBOSE_PREFIX_3 "%s: speeddial %d not assigned\n", DEV_ID_LOG(s->device), instance);
			}else{
				free(k);
			}
		// 	i
// 			if (k)
// 				sccp_handle_speeddial(d, k);
// 			else
// 				sccp_log(1)(VERBOSE_PREFIX_3 "%s: No number assigned to speeddial %d\n", d->id, instance);
			break;

		case SKINNY_BUTTONTYPE_HOLD:
			/* this is the hard hold button. When we are here we are putting on hold the active_channel */
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hold/Resume Button pressed on line (%d)\n", d->id, instance);
			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				l = sccp_dev_get_activeline(d);
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Trying the current line\n", d->id);
				if (!l) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					return;
				}
				instance = l->instance;
			}

			if ( (c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED)) ) {
				sccp_channel_hold(c);
			} else if ( (c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD)) ) {
				c1 = sccp_channel_get_active(d);
				if (c1 && c1->state == SCCP_CHANNELSTATE_OFFHOOK)
					sccp_channel_endcall(c1);
				sccp_channel_resume(c);
			} else {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: No call to resume/hold found on line %d\n", d->id, instance);
			}
			break;

		case SKINNY_BUTTONTYPE_TRANSFER:
			if (!d->transfer) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Tranfer disabled on device\n", d->id);
				break;
			}
			c = sccp_channel_get_active(d);
			if (c)
				sccp_channel_transfer(c);
			break;

		case SKINNY_BUTTONTYPE_VOICEMAIL: // Get a new Line and Dial the Voicemail.
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Voicemail Button pressed on line (%d)\n", d->id, instance);
			c = sccp_channel_get_active(d);
			if (c) {
				if (!c->line || ast_strlen_zero(c->line->vmnum)) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, instance);
					return;
				}
				sccp_channel_lock(c);
				if (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DIALING) {
					len = strlen(c->dialedNumber);
					sccp_copy_string(c->dialedNumber+len, c->line->vmnum, sizeof(c->dialedNumber-len));
					sccp_copy_string(c->dialedNumber, c->line->vmnum, sizeof(c->dialedNumber));
					c->digittimeout = time(0)+GLOB(digittimeout);
					sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
				} else {
					sccp_pbx_senddigits(c, c->line->vmnum);
//					ast_log(LOG_NOTICE, "Cannot send voicemail number. Call in progress with no rtp stream\n", c->callid);
				}
				sccp_channel_unlock(c);
				return;
			}
			if (!instance)
				instance = 1;
			l = sccp_line_find_byid(d, instance);
			if (!l) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
				return;
			}
			if (!ast_strlen_zero(l->vmnum)) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Dialing voicemail %s\n", d->id, l->vmnum);
				sccp_channel_newcall(l, l->vmnum, SKINNY_CALLTYPE_OUTBOUND);
			} else {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, instance);
			}
			break;

		case SKINNY_BUTTONTYPE_CONFERENCE:
			ast_log(LOG_NOTICE, "%s: Conference Button is not yet handled. working on implementation\n", d->id);
			break;

		
		
		case SKINNY_BUTTONTYPE_FEATURE:
			sccp_handle_feature_action(s, r, 1);
			break;
			
		case SKINNY_BUTTONTYPE_FORWARDALL: // Call forward all			
			l = d->currentLine;			
			if(!l) {
				if (!instance)
					instance = 1;
			
				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					return;
				}
			}			
			if (l) {
				sccp_feat_handle_callforward(l, SCCP_CFWD_ALL);
			}
			break;
			/*	
			if (!d->cfwdall) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: CFWDALL disabled on device\n", d->id);
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
				return;
			}
			c = sccp_channel_get_active(d);
			if (!c || !c->owner) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Call forward with no channel active\n", d->id);
				return;
			}
			if (c->state != SCCP_CHANNELSTATE_RINGOUT && c->state != SCCP_CHANNELSTATE_CONNECTED) {
				sccp_line_cfwd(c->line, SCCP_CFWD_NONE, NULL);
				return;
			}
			sccp_line_cfwd(c->line, SCCP_CFWD_ALL, c->dialedNumber);
			break;
			*/
		case SKINNY_BUTTONTYPE_FORWARDBUSY:
			l = d->currentLine;			
			if(!l) {
				if (!instance)
					instance = 1;
			
				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					return;
				}
			}			
			if (l) {
				sccp_feat_handle_callforward(l, SCCP_CFWD_BUSY);
			}
			break;
		case SKINNY_BUTTONTYPE_FORWARDNOANSWER:
			l = d->currentLine;			
			if(!l) {
				if (!instance)
					instance = 1;
			
				l = sccp_line_find_byid(d, instance);
				if (!l) {
					sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, instance);
					return;
				}
			}			
			if (l) {
				sccp_feat_handle_callforward(l, SCCP_CFWD_NOANSWER);
			}
			break;
		case SKINNY_BUTTONTYPE_CALLPARK: // Call parking
#ifdef CS_SCCP_PARK
			c = sccp_channel_get_active(d);
			if (!c) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: Cannot park while no calls in progress\n", d->id);
				return;
			}
			sccp_channel_park(c);
#else
    		sccp_log(10)(VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
			break;

		default:
			ast_log(LOG_NOTICE, "%s: Don't know how to deal with stimulus %d with Phonetype %s(%d) \n", d->id, stimulus, skinny_devicetype2str(d->skinny_type), d->skinny_type);
			break;
	}
}
/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_speeddial(sccp_device_t * d, sccp_speed_t * k) {
	sccp_channel_t * c = NULL;
	sccp_line_t * l;
	int len;

	if (!k || !d || !d->session)
		return;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Speeddial Button (%d) pressed, configured number is (%s)\n", d->id, k->instance, k->ext);
	c = sccp_channel_get_active(d);
	if (c) {
		sccp_channel_lock(c);
		if ( (c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) ) {
			len = strlen(c->dialedNumber);
			sccp_copy_string(c->dialedNumber+len, k->ext, sizeof(c->dialedNumber)-len);
				c->digittimeout = time(0)+1;
			/*
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK)
				sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
			*/
			sccp_channel_unlock(c);
			return;
		}
		sccp_channel_unlock(c);
		sccp_pbx_senddigits(c, k->ext);
	} else {
		// Pull up a channel
		l = d->currentLine;
		if (l) {
			sccp_channel_newcall(l, k->ext, SKINNY_CALLTYPE_OUTBOUND);
		}
	}
}

/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_offhook(sccp_session_t * s, sccp_moo_t * r) {
	sccp_line_t * l;
	sccp_channel_t * c;
	sccp_device_t * d = s->device;
	if (!d)
		return;

	if ( (c = sccp_channel_get_active(d)) ) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Taken Offhook with a call (%d) in progess. Skip it!\n", d->id, c->callid);
		return;
	}

	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_OFFHOOK;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Taken Offhook\n", d->id);

	if (!d->lines) {
    	ast_log(LOG_NOTICE, "No lines registered on %s for take OffHook\n", s->device->id);
    	// sccp_dev_displayprompt(d, 0, 0, "No lines registered!", 0);
    	// sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, 0);
    	return;
    }

	c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_RINGING);

	if (c) {
    	/* Answer the ringing channel. */
    	sccp_channel_answer(c);
	} else {
		l = sccp_dev_get_activeline(d);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Using line %s\n", d->id, l->name);
		/* make a new call with no number */
		sccp_channel_newcall(l, NULL, SKINNY_CALLTYPE_OUTBOUND);
	}
}

/**
 * 
 * 
 * 
 * 
 * 
 */
void sccp_handle_onhook(sccp_session_t * s, sccp_moo_t * r) {
	sccp_channel_t * c;
	sccp_device_t * d = s->device;

	if (!s || !d) {
		ast_log(LOG_NOTICE, "No device to put OnHook\n");
		return;
	}

	if (!d->session)
		return;
	
	/* we need this for callwaiting, hold, answer and stuff */
	d->state = SCCP_DEVICESTATE_ONHOOK;
	sccp_log(1)(VERBOSE_PREFIX_3 "%s is Onhook\n", s->device->id);

	if (!d->lines) {
		ast_log(LOG_NOTICE, "No lines registered on %s to put OnHook\n", s->device->id);
	return;
	}

	/* get the active channel */
	c = sccp_channel_get_active(d);

	if (!c) {
		sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
		sccp_dev_stoptone(d, 0, 0);
	} else {
		sccp_channel_endcall(c);
	}

	return;
}

void sccp_handle_headset(sccp_session_t * s, sccp_moo_t * r) {
	sccp_log(10)(VERBOSE_PREFIX_3 "handle headset\n");
}

/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_moo_t * r) {
  int i;
  uint8_t codec;
  int astcodec;
  uint8_t n = letohl(r->msg.CapabilitiesResMessage.lel_count);
  s->device->capability = 0;
  sccp_log(10)(VERBOSE_PREFIX_3 "%s: Device has %d Capabilities\n", s->device->id, n);
  for (i = 0 ; i < n; i++) {
    codec = letohl(r->msg.CapabilitiesResMessage.caps[i].lel_payloadCapability);
    astcodec = sccp_codec_skinny2ast(codec);
	s->device->capability |= astcodec;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: CISCO:%6d %-25s AST:%6d %s\n", s->device->id, codec, skinny_codec2str(codec), astcodec, ast_codec2str(astcodec));
  }
}

/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_moo_t * r) {
	uint8_t i;
	uint8_t 	size = sizeof(softkeysmap);
	sccp_moo_t * r1;
	sccp_device_t * d = s->device;
	
	

	if (!d)
		return;
	
	/* ok the device support the softkey map */
	
	sccp_device_lock(d);
	
	d->softkeysupport = 1;
	
	
	REQ(r1, SoftKeyTemplateResMessage);
	r1->msg.SoftKeyTemplateResMessage.lel_softKeyOffset = htolel(0);
	
	for (i = 0; i < size; i++) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Button(%d)[%2d] = %s\n", s->device->id, i, i+1, skinny_lbl2str(softkeysmap[i]));
		r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[0] = 128;
		r1->msg.SoftKeyTemplateResMessage.definition[i].softKeyLabel[1] = softkeysmap[i];
		r1->msg.SoftKeyTemplateResMessage.definition[i].lel_softKeyEvent = htolel(i+1);
	}
	
	sccp_device_unlock(d);
	
	r1->msg.SoftKeyTemplateResMessage.lel_softKeyCount = htolel(size);
	r1->msg.SoftKeyTemplateResMessage.lel_totalSoftKeyCount = htolel(size);
	sccp_dev_send(s->device, r1);
}

/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t * d = s->device;
	const softkey_modes * v = SoftKeyModes;
	const	uint8_t	v_count = ( sizeof(SoftKeyModes)/sizeof(softkey_modes) );
	int iKeySetCount = 0;
	sccp_moo_t * r1;
	uint8_t i = 0;
	sccp_line_t * l;
	uint8_t trnsfvm = 0;
	uint8_t meetme = 0;
#ifdef CS_SCCP_PICKUP
	uint8_t pickupgroup= 0;
#endif	
	if (!d)
		return;

	REQ(r1, SoftKeySetResMessage);
	r1->msg.SoftKeySetResMessage.lel_softKeySetOffset = htolel(0);

	/* look for line trnsvm */
	l = d->lines;
	while (l) {
		if (!ast_strlen_zero(l->trnsfvm))
			trnsfvm = 1;

		if (!ast_strlen_zero(l->meetmenum))
			meetme = 1;

#ifdef CS_SCCP_PICKUP
		if (l->pickupgroup)
			pickupgroup = 1;
#endif
		l = l->next_on_device;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSFER        is %s\n", d->id, (d->transfer) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: DND             is %s\n", d->id, d->dndmode ? sccp_dndmode2str(d->dndmode) : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: PRIVATE         is %s\n", d->id, d->private ? "enabled" : "disabled");
#ifdef CS_SCCP_PARK
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: PARK            is  %s\n", d->id, (d->park) ? "enabled" : "disabled");
#endif
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: CFWDALL         is  %s\n", d->id, (d->cfwdall) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: CFWDBUSY        is  %s\n", d->id, (d->cfwdbusy) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: CFWDNOANSWER    is  %s\n", d->id, (d->cfwdnoanswer) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRNSFVM/IDIVERT is  %s\n", d->id, (trnsfvm) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: MEETME          is  %s\n", d->id, (meetme) ? "enabled" : "disabled");
#ifdef CS_SCCP_PICKUP
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: PICKUPGROUP     is  %s\n", d->id, (pickupgroup) ? "enabled" : "disabled");
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: PICKUPEXTEN     is  %s\n", d->id, (d->pickupexten) ? "enabled" : "disabled");
#endif

	for (i = 0; i < v_count; i++) {
		const uint8_t * b = v->ptr;
		uint8_t c, i = 0;

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set[%-2d]= ", d->id, v->id);

		for ( c = 0; c < v->count; c++) {
		/* look for the SKINNY_LBL_ number in the softkeysmap */
			if ( (b[c] == SKINNY_LBL_PARK) && (!d->park) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_TRANSFER) && (!d->transfer) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_DND) && (!d->dndmode) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_CFWDALL) && (!d->cfwdall) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_CFWDBUSY) && (!d->cfwdbusy) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_CFWDNOANSWER) && (!d->cfwdnoanswer) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_TRNSFVM) && (!trnsfvm) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_IDIVERT) && (!trnsfvm) ) {
				continue;
			}			
			if ( (b[c] == SKINNY_LBL_MEETME) && (!meetme) ) {
				continue;
			}
#ifndef CS_ADV_FEATURES
			if ( (b[c] == SKINNY_LBL_BARGE) ) {
				continue;
			}			
			if ( (b[c] == SKINNY_LBL_CBARGE) ) {
				continue;
			}
#else
#ifndef CS_SCCP_CONFERENCE
			if ( (b[c] == SKINNY_LBL_JOIN) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_CONFRN) ) {
				continue;
			}			
#endif
#endif
#ifdef CS_SCCP_PICKUP
			if ( (b[c] == SKINNY_LBL_PICKUP) && (!d->pickupexten) ) {
				continue;
			}
			if ( (b[c] == SKINNY_LBL_GPICKUP) && (!pickupgroup) ) {
				continue;
			}
#endif			
			if ( (b[c] == SKINNY_LBL_PRIVATE) && (!d->private) ) {
				continue;
			}
			for (i = 0; i < sizeof(softkeysmap); i++) {
				if (b[c] == softkeysmap[i]) {
				sccp_log(10)("%-2d:%-10s ", c, skinny_lbl2str(softkeysmap[i]));
				r1->msg.SoftKeySetResMessage.definition[v->id].softKeyTemplateIndex[c] = (i+1);
				break;
			}
		}
	}

	sccp_log(10)("\n");
	v++;
	iKeySetCount++;
	};

	sccp_log(10)( VERBOSE_PREFIX_3 "There are %d SoftKeySets.\n", iKeySetCount);

	r1->msg.SoftKeySetResMessage.lel_softKeySetCount = htolel(iKeySetCount);
	r1->msg.SoftKeySetResMessage.lel_totalSoftKeySetCount = htolel(iKeySetCount); // <<-- for now, but should be: iTotalKeySetCount;

	sccp_dev_send(s->device, r1);
	sccp_dev_set_keyset(s->device, 0, 0, KEYMODE_ONHOOK);
}


/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_time_date_req(sccp_session_t * s, sccp_moo_t * req) {
  time_t timer = 0;
  struct tm * cmtime = NULL;
  sccp_moo_t * r1;
  REQ(r1, DefineTimeDate);

  if (!s || !s->device) {
       ast_log(LOG_WARNING,"Session no longer valid\n");
       return;
  }

  /* modulate the timezone by full hours only */
  timer = time(0) + (s->device->tz_offset * 3600);
  cmtime = localtime(&timer);

  r1->msg.DefineTimeDate.lel_year = htolel(cmtime->tm_year+1900);
  r1->msg.DefineTimeDate.lel_month = htolel(cmtime->tm_mon+1);
  r1->msg.DefineTimeDate.lel_dayOfWeek = htolel(cmtime->tm_wday);
  r1->msg.DefineTimeDate.lel_day = htolel(cmtime->tm_mday);
  r1->msg.DefineTimeDate.lel_hour = htolel(cmtime->tm_hour);
  r1->msg.DefineTimeDate.lel_minute = htolel(cmtime->tm_min);
  r1->msg.DefineTimeDate.lel_seconds = htolel(cmtime->tm_sec);
  r1->msg.DefineTimeDate.lel_milliseconds = htolel(0);
  r1->msg.DefineTimeDate.lel_systemTime = htolel(timer);
  sccp_dev_send(s->device, r1);
  sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send date/time\n", s->device->id);
}

void sccp_handle_keypad_button(sccp_session_t * s, sccp_moo_t * r) {
	int event;
	uint8_t line;
	uint32_t callid;
	char resp = '\0';
	int len = 0, len1 = 0;
	sccp_channel_t * c = NULL;
	sccp_device_t * d = NULL;
	sccp_line_t * l = NULL;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: sccp_handle_keypad_button \n", s->device->id);
	if (!s->device)
		return;

	d = s->device;

	event = letohl(r->msg.KeypadButtonMessage.lel_kpButton);
	line = letohl(r->msg.KeypadButtonMessage.lel_lineInstance);
	callid = letohl(r->msg.KeypadButtonMessage.lel_callReference);

	if (line)
		l = sccp_line_find_byid(s->device, line);

	if (l && callid)
		c = sccp_channel_find_byid(callid);

	if (!c) {
		c = sccp_channel_get_active(d);
	}

	if (!c) {
		ast_log(LOG_NOTICE, "Device %s sent a Keypress, but there is no active channel!\n", d->id);
		return;
	}

	sccp_channel_lock(c);
	
	l = c->line;
	d = c->device;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Cisco Digit: %08x (%d) on line %s\n", d->id, event, event, l->name);

	if (event < 10)
		resp = '0' + event;
	else if (event == 14)
		resp = '*';
	else if (event == 15)
		resp = '#';

	if (c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED) {
		/* we have to unlock 'cause the senddigit lock the channel */
		sccp_channel_unlock(c);
//		sccp_dev_starttone(d, (uint8_t) event);
    	sccp_pbx_senddigit(c, resp);
    	return;
	}

	if ( (c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) || (c->state == SCCP_CHANNELSTATE_GETDIGITS)) {
		len = strlen(c->dialedNumber);
		if (len >= (AST_MAX_EXTENSION - 1) ) {
			sccp_dev_displayprompt(d,c->line->instance, c->callid, "No more digits", 5);
		} else {
			c->dialedNumber[len++] = resp;
    		c->dialedNumber[len] = '\0';
    		c->digittimeout = time(0)+GLOB(digittimeout);

#ifdef CS_SCCP_PICKUP			
			if (!strcmp(c->dialedNumber, ast_pickup_ext()) && (c->state != SCCP_CHANNELSTATE_GETDIGITS)) {
				/* set it to offhook state because the sccp_sk_gpickup function look for an offhook channel */
				c->state = SCCP_CHANNELSTATE_OFFHOOK;
				sccp_channel_unlock(c);
				sccp_sk_gpickup(c->device, c->line, c);
				return;
			}
#endif

			if (GLOB(digittimeoutchar) && GLOB(digittimeoutchar) == resp) {
				if(GLOB(recorddigittimeoutchar)) {
					c->dialedNumber[len] = '\0';
				} else {
					c->dialedNumber[--len] = '\0';
				}
				c->digittimeout = time(0);
			}
		}
	}
	if (len == 1)
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
	/* secondary dialtone check */
	len1 = strlen(l->secondary_dialtone_digits);
	if (len1) {
		if (!strncmp(c->dialedNumber, l->secondary_dialtone_digits, len1)) {
			if (len == len1)
				 sccp_dev_starttone(d, l->secondary_dialtone_tone, l->instance, c->callid, 0);
			else if (len > len1)
				sccp_dev_stoptone(d, l->instance, c->callid);
		}
	}

	sccp_channel_unlock(c);
}

/**
 * 
 * 
 * 
 * 
 * 
 * 
 */
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t * d = s->device;
	sccp_channel_t * c = NULL;
	sccp_line_t * l = NULL;
	sccp_speed_t * k = NULL;
	uint32_t event = letohl(r->msg.SoftKeyEventMessage.lel_softKeyEvent);
	uint32_t line = letohl(r->msg.SoftKeyEventMessage.lel_lineInstance);
	uint32_t callid = letohl(r->msg.SoftKeyEventMessage.lel_callReference);

	if (!d)
		return;

		/*
	if ( event > sizeof(softkeysmap) )
		ast_log(LOG_ERROR, "%s: Out of range for Softkey: %s (%d) line=%d callid=%d\n", d->id, skinny_lbl2str(event), event, line, callid);
	*/
	event = softkeysmap[event-1];
	

	/* correct events for nokia icc client
	 */
	if(d->config_type && !strcasecmp(d->config_type, "nokia-icc")){
		switch (event) {
			case SKINNY_LBL_DIRTRFR:
				event = SKINNY_LBL_ENDCALL;
			break;
		}
	}




	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got Softkey: %s (%d) line=%d callid=%d\n", d->id, skinny_lbl2str(event), event, line, callid);

	if (line)
		l = sccp_line_find_byid(s->device, line);

	if (line && callid)
		c = sccp_channel_find_byid(callid);

	switch (event) {
	case SKINNY_LBL_REDIAL:
		sccp_sk_redial(d, l, c);
		break;
	case SKINNY_LBL_NEWCALL:
		if (l)
			sccp_sk_newcall(d, l, c);
		else {
			k = sccp_dev_speed_find_byindex(d, line, SKINNY_BUTTONTYPE_LINE);
			if (k){
				sccp_handle_speeddial(d, k);
				free(k);
			}else
				sccp_sk_newcall(d, NULL, NULL);
		}
		break;
	case SKINNY_LBL_CONFRN:
		sccp_sk_conference(d, l, c);
		break;
	case SKINNY_LBL_MEETME:
		sccp_sk_meetme(d, l, c);
		break;
	case SKINNY_LBL_JOIN:
		sccp_sk_join(d, l, c);
		break;
	case SKINNY_LBL_BARGE:
		sccp_sk_barge(d, l, c);
		break;
	case SKINNY_LBL_CBARGE:
		sccp_sk_cbarge(d, l, c);
		break;
	case SKINNY_LBL_HOLD:
		sccp_sk_hold(d, l, c);
		break;
	case SKINNY_LBL_TRANSFER:
		sccp_sk_transfer(d, l, c);
		break;
	case SKINNY_LBL_CFWDALL:
		sccp_sk_cfwdall(d, l, c);
		break;
	case SKINNY_LBL_CFWDBUSY:
		sccp_sk_cfwdbusy(d, l, c);
		break;
	case SKINNY_LBL_CFWDNOANSWER:
		sccp_sk_cfwdnoanswer(d, l, c);
		break;
	case SKINNY_LBL_BACKSPACE:
		sccp_sk_backspace(d, l, c);
		break;
	case SKINNY_LBL_ENDCALL:
		sccp_sk_endcall(d, l, c);
		break;
	case SKINNY_LBL_RESUME:
		sccp_sk_resume(d, l, c);
		break;
	case SKINNY_LBL_ANSWER:
		sccp_sk_answer(d, l, c);
		break;
#ifdef CS_SCCP_PARK
	case SKINNY_LBL_PARK:
		sccp_sk_park(d, l, c);
		break;
#endif
	case SKINNY_LBL_TRNSFVM:
	case SKINNY_LBL_IDIVERT:
		sccp_sk_trnsfvm(d, l, c);
		break;
	case SKINNY_LBL_DND:
		sccp_sk_dnd(d, l, c);
		break;
	case SKINNY_LBL_DIRTRFR:
		sccp_sk_dirtrfr(d, l, c);
		break;
	case SKINNY_LBL_SELECT:
		sccp_sk_select(d, l, c);
		break;
	case SKINNY_LBL_PRIVATE:
		sccp_sk_private(d, l, c);
		break;
#ifdef CS_SCCP_PICKUP
	case SKINNY_LBL_PICKUP:
		sccp_sk_pickup(d, l, c);
		break;
	case SKINNY_LBL_GPICKUP:
		sccp_sk_gpickup(d, l, c);
		break;
#endif
	default:
		ast_log(LOG_WARNING, "Don't know how to handle keypress %d\n", event);
	}
  /* This must not be done. As we didn't lock the channel ourselves,
   * we should not unlock it either. Hence, it could happen that 
   * the channel and with it the lock is already deallocated when we try to unlock it.
   * This actually happens sometimes when issueing a hangup handled by this function.
	if (c)
		sccp_channel_unlock(c);
   */
}


/**
 * 
 * 
 * 
 * 
 * 
 */
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_moo_t * r) {
	struct sockaddr_in sin;
	sccp_channel_t * c;
	sccp_device_t * d;
	int status = 0;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif	

	if (!s || !s->device)
		return;

	d = s->device;

	sin.sin_family = AF_INET;
	if (d->trustphoneip)
		memcpy(&sin.sin_addr, &r->msg.OpenReceiveChannelAck.bel_ipAddr, sizeof(sin.sin_addr));
	else
		memcpy(&sin.sin_addr, &s->sin.sin_addr, sizeof(sin.sin_addr));

	sin.sin_port = htons(htolel(r->msg.OpenReceiveChannelAck.lel_portNumber));

	status = letohl(r->msg.OpenReceiveChannelAck.lel_orcStatus);
#ifdef	ASTERISK_CONF_1_2
	sccp_log(8)(VERBOSE_PREFIX_3 "%s: Got OpenChannel ACK.  Status: %d, RemoteIP (%s): %s, Port: %d, PassThruId: %d\n",
			d->id,
			status, (d->trustphoneip ? "Phone" : "Connection"),
			ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr),
			ntohs(sin.sin_port),
			letohl(r->msg.OpenReceiveChannelAck.lel_passThruPartyId));
#else	
	sccp_log(8)(VERBOSE_PREFIX_3 "%s: Got OpenChannel ACK.  Status: %d, RemoteIP (%s): %s, Port: %d, PassThruId: %d\n",
		d->id,
		status, (d->trustphoneip ? "Phone" : "Connection"),
		ast_inet_ntoa(sin.sin_addr),
		ntohs(sin.sin_port),
		letohl(r->msg.OpenReceiveChannelAck.lel_passThruPartyId));
#endif
	if (status) {
		/* rtp error from the phone */
		ast_log(LOG_ERROR, "%s: (OpenReceiveChannelAck) Device error (%d) ! No RTP media available\n", d->id, status);
		return;
	}
	
	c = sccp_channel_find_byid(letohl(r->msg.OpenReceiveChannelAck.lel_passThruPartyId));
	/* prevent a segmentation fault on fast hangup after answer, failed voicemail for example */	
	if (c && c->state != SCCP_CHANNELSTATE_DOWN) {
		sccp_log(66)(VERBOSE_PREFIX_3 "%s: STARTING DEVICE RTP TRANSMISSION WITH STATE %s(%d)\n", d->id, sccp_indicate2str(c->state), c->state);
		sccp_channel_lock(c);
		memcpy(&c->rtp_addr, &sin, sizeof(sin));
		if (c->rtp) {
			sccp_channel_startmediatransmission(c);
#ifdef ASTERISK_CONF_1_2
			sccp_log(66)(VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s:%d\n", d->id, ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
				ast_rtp_set_peer(c->rtp, &sin);
#else
			sccp_log(66)(VERBOSE_PREFIX_3 "%s: Set the RTP media address to %s:%d\n", d->id, ast_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
			ast_rtp_set_peer(c->rtp, &sin);
			sccp_ast_setstate(c, AST_STATE_UP);
#endif
		} else {
#ifdef ASTERISK_CONF_1_2
			ast_log(LOG_ERROR,  "%s: Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port));
#else
			ast_log(LOG_ERROR,  "%s: Can't set the RTP media address to %s:%d, no asterisk rtp channel!\n", d->id, ast_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
#endif
			sccp_channel_endcall(c); // FS - 350
		}
		sccp_channel_unlock(c);
	} else {
		ast_log(LOG_ERROR, "%s: No channel with this PassThruId!\n", d->id);
	}	
}

/**
 * 
 * 
 * 
 */
void sccp_handle_version(sccp_session_t * s, sccp_moo_t * r) {
	sccp_moo_t * r1;

	if (!s || !s->device)
		return;

	REQ(r1, VersionMessage);
	sccp_copy_string(r1->msg.VersionMessage.requiredVersion, s->device->imageversion, sizeof(r1->msg.VersionMessage.requiredVersion));
	sccp_dev_send(s->device, r1);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending version number: %s\n", s->device->id, s->device->imageversion);
}


/**
 * 
 * 
 * 
 * 
 */
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_moo_t * r) {
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Statistics from %s callid: %d Packets sent: %d rcvd: %d lost: %d jitter: %d latency: %d\n", s->device->id, r->msg.ConnectionStatisticsRes.DirectoryNumber,
		letohl(r->msg.ConnectionStatisticsRes.lel_CallIdentifier),
		letohl(r->msg.ConnectionStatisticsRes.lel_SentPackets),
		letohl(r->msg.ConnectionStatisticsRes.lel_RecvdPackets),
		letohl(r->msg.ConnectionStatisticsRes.lel_LostPkts),
		letohl(r->msg.ConnectionStatisticsRes.lel_Jitter),
		letohl(r->msg.ConnectionStatisticsRes.lel_latency)
		);
}

void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_moo_t * r) {
	sccp_moo_t * r1;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	/* old protocol function replaced by the SEP file server addesses list */

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending servers message\n", DEV_ID_LOG(s->device));

	REQ(r1, ServerResMessage);
#ifdef ASTERISK_CONF_1_2
	sccp_copy_string(r1->msg.ServerResMessage.server[0].serverName, ast_inet_ntoa(iabuf, sizeof(iabuf), s->ourip), sizeof(r1->msg.ServerResMessage.server[0].serverName));
#else
	sccp_copy_string(r1->msg.ServerResMessage.server[0].serverName, ast_inet_ntoa(s->ourip), sizeof(r1->msg.ServerResMessage.server[0].serverName));
#endif
	r1->msg.ServerResMessage.serverListenPort[0] = GLOB(ourport);
	r1->msg.ServerResMessage.serverIpAddr[0] = s->ourip.s_addr;
	sccp_dev_send(s->device, r1);
}

/**
 * 
 * 
 */
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_moo_t * r) {
	sccp_moo_t * r1;
	uint8_t lines = 0;
	uint8_t speeddials = 0;
	sccp_device_t * d;
	sccp_line_t * l;
	sccp_speed_t * k;

	if (!s)
		return;
	d = s->device;
	if (!d)
		return;
	sccp_device_lock(d);
	sccp_globals_lock(lines_lock);
	l = d->lines;
	while (l) {
		lines++;
		l = l->next_on_device;
	}
	k = d->speed_dials;
	while (k) {
		speeddials++;
		k = k->next;
	}
	sccp_globals_unlock(lines_lock);
	sccp_device_unlock(d);


	REQ(r1, ConfigStatMessage);
	sccp_copy_string(r1->msg.ConfigStatMessage.deviceName, s->device->id, sizeof(r1->msg.ConfigStatMessage.deviceName));
	r1->msg.ConfigStatMessage.lel_stationInstance = htolel(1);
	r1->msg.ConfigStatMessage.lel_numberLines	     = htolel(lines);
	r1->msg.ConfigStatMessage.lel_numberSpeedDials 	     = htolel(speeddials);
	sccp_dev_send(s->device, r1);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending ConfigStatMessage, lines %d, speeddials %d\n", d->id, lines, speeddials);
}


/**
 *  
 * 
 * 
 */
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t * d = NULL;
	sccp_channel_t * c = NULL;
	sccp_line_t * l = NULL;
	
	int len = 0;
	// sccp_dump_packet((unsigned char *)&r->msg.EnblocCallMessage, r->length);
	
	if (!s || !s->device)
		return;
	
	d = s->device;
	
	if (r && !ast_strlen_zero(r->msg.EnblocCallMessage.calledParty)) {
		c = sccp_channel_get_active(d);
		if (c) {
			sccp_channel_lock(c);
			if ( (c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) ) {
				len = strlen(c->dialedNumber);
				sccp_copy_string(c->dialedNumber+len, r->msg.EnblocCallMessage.calledParty, sizeof(c->dialedNumber)-len);
					c->digittimeout = time(0)+1;
				/*
				if (c->state == SCCP_CHANNELSTATE_OFFHOOK)
					sccp_indicate_nolock(c, SCCP_CHANNELSTATE_DIALING);
				*/
				sccp_channel_unlock(c);
				return;
			}
			sccp_channel_unlock(c);
			sccp_pbx_senddigits(c, r->msg.EnblocCallMessage.calledParty);
		} else {
			// Pull up a channel
			l = d->currentLine;
			if (l) {
				sccp_channel_newcall(l, r->msg.EnblocCallMessage.calledParty, SKINNY_CALLTYPE_OUTBOUND);
			}
		}		
			
	}
}


/**
 * 
 * 
 * 
 */
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t * d = s->device;
	sccp_line_t * l;
	sccp_moo_t * r1 = NULL;

	if (!d)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got Forward Status Request.  Line: %d\n", d->id, letohl(r->msg.ForwardStatReqMessage.lel_lineNumber));

	l = sccp_line_find_byid(d, r->msg.ForwardStatReqMessage.lel_lineNumber);
	if (l)
		sccp_dev_forward_status(l);
	else {
		/* speeddial with hint. Sending empty forward message */
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send Forward Status.  Instance: %d\n", d->id, letohl(r->msg.ForwardStatReqMessage.lel_lineNumber));
		REQ(r1, ForwardStatMessage);
		r1->msg.ForwardStatMessage.lel_status = 0;
		r1->msg.ForwardStatMessage.lel_lineNumber = r->msg.ForwardStatReqMessage.lel_lineNumber;
		sccp_dev_send(d, r1);
	}
}

/**
 * 
 * 
 */
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t * d = s->device;

  
	if (!d)
		return;

  	int featureIndex = letohl(r->msg.FeatureStatReqMessage.lel_featureIndex);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d\n", d->id, featureIndex);
	sccp_handle_feature_action(s, r, 0);
}

/**
 * 
 * 
 *  
 */
void sccp_handle_servicesurl_stat_req(sccp_session_t * s, sccp_moo_t * r) {
	sccp_device_t 		* d = s->device;
	sccp_moo_t 			* r1;
	sccp_serviceURL_t 	* serviceURL;
	
	int urlIndex = letohl(r->msg.ServiceURLStatReqMessage.lel_serviceURLIndex);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Got ServiceURL Status Request.  Index = %d\n", d->id, urlIndex);
	serviceURL = sccp_dev_serviceURL_find_byindex(s->device, urlIndex);
	
	REQ(r1, ServiceURLStatMessage);
	r1->msg.ServiceURLStatMessage.lel_serviceURLIndex = htolel(urlIndex);
	
	if (serviceURL){
		sccp_copy_string(r1->msg.ServiceURLStatMessage.URL, serviceURL->URL, strlen(serviceURL->URL)+1);
		sccp_copy_string(r1->msg.ServiceURLStatMessage.label, serviceURL->label, strlen(serviceURL->label)+1);
	} 
	else{
		sccp_log(3)(VERBOSE_PREFIX_3 "%s: serviceURL %d not assigned\n", DEV_ID_LOG(s->device), urlIndex);
	}
	sccp_dev_send(s->device, r1);
}

void sccp_handle_feature_action(sccp_session_t * s, sccp_moo_t * r, uint8_t toggleState) {
	sccp_device_t 		* d = NULL;
	sccp_moo_t 		* featureRequestMessage = NULL;
	sccp_buttonconfig_t	* buttonconfig=NULL;
	int featureIndex	=0;
	sccp_line_t		* line;
	
	if(!s->device)
		return;
	else
		d = s->device;

	//TODO not a good solution to use toggleState
	if(toggleState)
		featureIndex = letohl(r->msg.StimulusMessage.lel_stimulusInstance);
	else
		featureIndex = letohl(r->msg.FeatureStatReqMessage.lel_featureIndex);

	
	
	buttonconfig = d->buttonconfig;	
	while(buttonconfig){
		if(buttonconfig->instance == featureIndex && !strcasecmp(buttonconfig->type, "feature")){
			//feature = buttonconfig->button.feature;
			if(toggleState){
				ast_mutex_lock(&d->lock);
				buttonconfig->button.feature.status = htolel((buttonconfig->button.feature.status==0)?1:0);
				ast_mutex_unlock(&d->lock);
			}
			break;
		}
		buttonconfig = buttonconfig->next;
	}
	
	if(!buttonconfig || !buttonconfig->type || strcasecmp(buttonconfig->type, "feature") ){
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Couldn find feature with ID = %d \n", d->id, featureIndex);	
		return;
	}

	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: feature ID = %d \n", d->id, buttonconfig->button.feature.id);
	if(buttonconfig->button.feature.id == 1 && toggleState){
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: set cfwd to  %d \n", d->id, buttonconfig->button.feature.status);
		line = d->lines;
		while(line){
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: line  %s \n", d->id, line->name);

			if(buttonconfig->button.feature.status)
				sccp_line_cfwd(line, SCCP_CFWD_ALL, buttonconfig->button.feature.options);
			else
				sccp_line_cfwd(line, SCCP_CFWD_NONE, NULL);
			line = line->next_on_device;
		}
	}
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Got Feature Status Request.  Index = %d Status: %d\n", d->id, featureIndex, buttonconfig->button.feature.status);	

	REQ(featureRequestMessage, FeatureStatMessage);
	featureRequestMessage->msg.FeatureStatMessage.lel_featureIndex = htolel(featureIndex);
	//featureRequestMessage->msg.FeatureStatMessage.lel_featureIndex = htolel(3);	//TODO just fpr testing
	featureRequestMessage->msg.FeatureStatMessage.lel_featureID = htolel(0x13);
	sccp_copy_string(featureRequestMessage->msg.FeatureStatMessage.featureTextLabel, buttonconfig->button.feature.label, strlen(buttonconfig->button.feature.label)+1);

	//featureRequestMessage->msg.FeatureStatMessage.lel_featureStatus = htolel(1);

	featureRequestMessage->msg.FeatureStatMessage.lel_featureStatus = htolel(buttonconfig->button.feature.status);
	sccp_dev_send(s->device, featureRequestMessage);
	return;
}
