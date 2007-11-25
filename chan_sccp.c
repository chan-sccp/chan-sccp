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



/**
 * \mainpage chan_sccp-b User documentation
 * 
 * \ref intro_sk The Softkeys
 * 
 * 
 */
#define AST_MODULE "chan_sccp"

#include "config.h"


#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_cli.h"
#include "sccp_line.h"
#include "sccp_socket.h"
#include "sccp_pbx.h"
#include "sccp_indicate.h"

#include <ctype.h>
#include <unistd.h>
#include <asterisk/pbx.h>
#ifdef CS_AST_HAS_APP_SEPARATE_ARGS
#include <asterisk/app.h>
#endif
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#include <asterisk/translate.h>
#ifdef CS_AST_HAS_AST_STRING_FIELD
#include <asterisk/stringfields.h>
#endif
#include <asterisk/astdb.h>

static pthread_t socket_thread;


/**
 * 
 * 
 * 
 * 
 */
#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause) {
#else
struct ast_channel *sccp_request(char *type, int format, void *data) {
#endif

	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	char *options = NULL, *datadup = NULL;
	int optc = 0;
	char *optv[2];
	int opti = 0;
	int res = 0;
	int oldformat = format;

#ifdef CS_AST_HAS_TECH_PVT
	*cause = AST_CAUSE_NOTDEFINED;
#endif

	if (!type) {
		ast_log(LOG_NOTICE, "Attempt to call the wrong type of channel\n");
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	if (!data) {
		ast_log(LOG_NOTICE, "Attempt to call SCCP/ failed\n");
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	/* we leave the data unchanged */
	datadup = strdup(data);
	if ((options = strchr(datadup, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked to create a channel type=%s, format=%d, data=%s, options=%s\n", type, format, datadup, (options) ? options : "");

	l = sccp_line_find_byname(datadup);

	if (!l) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", datadup);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	if (!l->device) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	if (!l->device->session) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP/%s device has no active session.\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}


	format &= l->device->capability;
	if (!format) {
		format = oldformat;
		res = ast_translator_best_choice(&format, &l->device->capability);
		if (res < 0) {
			ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format '%d'\n", oldformat);
#ifdef CS_AST_HAS_TECH_PVT
			*cause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
#endif
			goto OUT;
		}
	}
	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	 c = sccp_channel_allocate(l);
	 if (!c) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
	 	goto OUT;
	 }

	 c->format = oldformat;
	if (!sccp_pbx_channel_allocate(c)) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		sccp_channel_delete(c);
		c = NULL;
		goto OUT;
	}

	/* call forward check */
	ast_mutex_lock(&l->lock);
	if (l->cfwd_type == SCCP_CFWD_ALL) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call forward (all) to %s\n", l->device->id, l->cfwd_num);
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#else
		sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#endif
	} else if (l->cfwd_type == SCCP_CFWD_BUSY && l->channelCount > 1) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call forward (busy) to %s\n", l->device->id, l->cfwd_num);
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#else
		sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#endif
	}
	ast_mutex_unlock(&l->lock);
	
	/* we don't need to parse any options when we have a call forward status */
	if (!ast_strlen_zero(c->owner->call_forward))
		goto OUT;

	/* check for the channel params */
	if (options && (optc = sccp_app_separate_args(options, '/', optv, sizeof(optv) / sizeof(optv[0])))) {
		for (opti = 0; opti < optc; opti++) {
			if (!strncasecmp(optv[opti], "aa", 2)) {
				/* let's use the old style auto answer aa1w and aa2w */
				if (!strncasecmp(optv[opti], "aa1w", 4)) {
						c->autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti]+=4;
				} else if (!strncasecmp(optv[opti], "aa2w", 4)) {
						c->autoanswer_type = SCCP_AUTOANSWER_2W;
						optv[opti]+=4;
				} else if (!strncasecmp(optv[opti], "aa=", 3)) {
					optv[opti] += 3;
					if (!strncasecmp(optv[opti], "1w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti]+=2;
					} else if (!strncasecmp(optv[opti], "2w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_2W;
						optv[opti]+=2;
					}
				}

				/* since the pbx ignores autoanswer_cause unless channelCount > 1, it is safe to set it if provided */
				if (!ast_strlen_zero(optv[opti]) && (c->autoanswer_type)) {
					if (!strcasecmp(optv[opti], "b"))
						c->autoanswer_cause = AST_CAUSE_BUSY;
					else if (!strcasecmp(optv[opti], "u"))
						c->autoanswer_cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
					else if (!strcasecmp(optv[opti], "c"))
						c->autoanswer_cause = AST_CAUSE_CONGESTION;
				}
#ifdef CS_AST_HAS_TECH_PVT
				if (c->autoanswer_cause)
					*cause = c->autoanswer_cause;
#endif
			/* check for ringer options */
			} else if (!strncasecmp(optv[opti], "ringer=", 7)) {
				optv[opti] += 7;
				if (!strcasecmp(optv[opti], "inside"))
					c->ringermode = SKINNY_STATION_INSIDERING;
				else if (!strcasecmp(optv[opti], "outside"))
					c->ringermode = SKINNY_STATION_OUTSIDERING;
				else if (!strcasecmp(optv[opti], "feature"))
					c->ringermode = SKINNY_STATION_FEATURERING;
				else if (!strcasecmp(optv[opti], "silent"))
					c->ringermode = SKINNY_STATION_SILENTRING;
				else if (!strcasecmp(optv[opti], "urgent"))
					c->ringermode = SKINNY_STATION_URGENTRING;
				else
					c->ringermode = SKINNY_STATION_OUTSIDERING;
			} else {
				ast_log(LOG_WARNING, "%s: Wrong option %s\n", l->device->id, optv[opti]);
			}
		}
	}

OUT:
	if (datadup)
		free(datadup);

	return (c && c->owner ? c->owner : NULL);
}

/**
 * returns the state of device
 * 
 * \param data name of device
 * \return devicestate of AST_DEVICE_*
 */
int sccp_devicestate(void * data) {
	sccp_line_t * l =  NULL;
	int res = AST_DEVICE_UNKNOWN;

	l = sccp_line_find_byname((char*)data);
	if (!l)
		res = AST_DEVICE_INVALID;
	else if (!l->device)
		res = AST_DEVICE_UNAVAILABLE;
	else if ((l->device->dnd && l->device->dndmode == SCCP_DNDMODE_REJECT) || (l->dnd && (l->dndmode == SCCP_DNDMODE_REJECT || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT) )) )
			res = AST_DEVICE_BUSY;
	else if (!l->channelCount)
		res = AST_DEVICE_NOT_INUSE;
#ifdef  CS_AST_DEVICE_RINGING
	else if (sccp_channel_find_bystate_on_device(l->device, SCCP_CHANNELSTATE_RINGIN))
		res = AST_DEVICE_RINGING;
#endif
	else
		res = AST_DEVICE_INUSE;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked for the state (%d) of the line %s\n", res, (char *)data);

	return res;
}

/**
 * 
 * 
 */
sccp_hint_t * sccp_hint_make(sccp_device_t *d, uint8_t instance) {
	sccp_hint_t *h = NULL;
	if (!d)
		return NULL;
	h = malloc(sizeof(sccp_hint_t));

	if (!h)
		return NULL;
	h->device = d;
	h->instance = instance;
	return h;
}



/**
 * 
 * 
 * 
 * 
 */
void sccp_hint_notify_devicestate(sccp_device_t * d, uint8_t state) {
	sccp_line_t * l;
	if (!d || !d->session)
		return;
	ast_mutex_lock(&d->lock);
	l = d->lines;
	while (l) {
		sccp_hint_notify_linestate(l, state, NULL);
		l = l->next_on_device;
	}
	ast_mutex_unlock(&d->lock);
}



/**
 * 
 * 
 * 
 */
void sccp_hint_notify_linestate(sccp_line_t * l, uint8_t state, sccp_device_t * onedevice) {
	sccp_moo_t * r;
	sccp_hint_t * h;
	sccp_device_t * d;
	char tmp[256] = "";
	uint8_t lamp = SKINNY_LAMP_OFF;

	/* let's go for internal hint system */

	if (!l || !l->hints) {
		if (!onedevice)
			ast_device_state_changed("SCCP/%s", l->name);
		return;
	}

	h = l->hints;

	while (h) {
		d = h->device;
		if (!d || !d->session) {
			h = h->next;
			continue;
		}

		/* this is for polling line status after device registration */
		if (onedevice && d != onedevice) {
			h = h->next;
			continue;
		}

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: HINT notify state %d of the line '%s'\n", d->id, state, l->name);
		if (state == SCCP_DEVICESTATE_ONHOOK) {
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_OFF);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_ONHOOK);
			h = h->next;
			continue;
		}

		REQ(r, CallInfoMessage);
		switch (state) {
		case SCCP_DEVICESTATE_UNAVAILABLE:
			lamp = SKINNY_LAMP_ON;
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
		case SCCP_DEVICESTATE_DND:
			lamp = SKINNY_LAMP_ON;
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_DND, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_DND, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
		case SCCP_DEVICESTATE_FWDALL:
			lamp = SKINNY_LAMP_ON;
			if (l->cfwd_type == SCCP_CFWD_ALL) {
				strcat(tmp, SKINNY_DISP_FORWARDED_TO " ");
				strcat(tmp, l->cfwd_num);
				sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, tmp, sizeof(r->msg.CallInfoMessage.callingPartyName));
				sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, tmp, sizeof(r->msg.CallInfoMessage.calledPartyName));
			}
			break;
		default:
			/* nothing to send */
			free(r);
			h = h->next;
			continue;
		}
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, lamp);
		sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
		/* sending CallInfoMessage */
		r->msg.CallInfoMessage.lel_lineId = htolel(h->instance);
		r->msg.CallInfoMessage.lel_callType = htolel(SKINNY_CALLTYPE_OUTBOUND);
		sccp_dev_send(d, r);
		sccp_dev_set_keyset(d, h->instance, 0, KEYMODE_MYST);

		h = h->next;
	}

	/* notify the asterisk hint system when we are not in a postregistration state (onedevice) */
	if (!onedevice)
		ast_device_state_changed("SCCP/%s", l->name);
}


/**
 * 
 * 
 * 
 */
void sccp_hint_notify_channelstate(sccp_device_t *d, uint8_t instance, sccp_channel_t * c) {
	sccp_moo_t * r;
	uint8_t lamp = SKINNY_LAMP_OFF;

	if (!d)
		d = c->device;
	if (!d)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: HINT notify state %s (%d) of the channel %d \n", d->id, sccp_callstate2str(c->callstate), c->callstate, c->callid);

	if (c->callstate == SKINNY_CALLSTATE_ONHOOK) {
		sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_OFF);
		sccp_channel_set_callstate_full(d, instance, c->callid, SKINNY_CALLSTATE_ONHOOK);
		return;
	}

	REQ(r, CallInfoMessage);
	switch (c->callstate) {
	case SKINNY_CALLSTATE_CONNECTED:
		lamp = SKINNY_LAMP_ON;
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));
		break;
	case SKINNY_CALLSTATE_OFFHOOK:
		lamp = SKINNY_LAMP_ON;
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_OFF_HOOK, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_OFF_HOOK, sizeof(r->msg.CallInfoMessage.calledPartyName));
		break;
	case SKINNY_CALLSTATE_RINGOUT:
		lamp = SKINNY_LAMP_ON;
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));
		break;
	case SKINNY_CALLSTATE_RINGIN:
		lamp = SKINNY_LAMP_BLINK;
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));
		break;
	default:
		/* nothing to send */
		free(r);
		return;
	}
	sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_ON);
	sccp_channel_set_callstate_full(d, instance, c->callid, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
	/* sending CallInfoMessage */
	r->msg.CallInfoMessage.lel_lineId   = htolel(instance);
	r->msg.CallInfoMessage.lel_callRef  = htolel(c->callid);
	r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);

	sccp_dev_send(d, r);
//			sccp_dev_sendmsg(d, DeactivateCallPlaneMessage);
	sccp_dev_set_keyset(d, instance, c->callid, KEYMODE_MYST);
}

/**
 * 
 * 
 */
void sccp_hint_notify(sccp_channel_t * c, sccp_device_t * onedevice) {
	sccp_hint_t *h;
	sccp_line_t *l = c->line;
	sccp_device_t *d;

	h = l->hints;
	if (!h)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: HINT notify the state of the line %s \n", l->name);

	while (h) {
		d = h->device;
		if (!d || !d->session) {
			h = h->next;
			continue;
		}
		if (onedevice && d == onedevice) {
			sccp_hint_notify_channelstate(d, h->instance, c);
			break;
		} else
			sccp_hint_notify_channelstate(d, h->instance, c);
		h = h->next;
	}
}

/**
 * asterisk hint wrapper 
 */
int sccp_hint_state(char *context, char* exten, enum ast_extension_states state, void *data) {
	sccp_hint_t * h = data;
	sccp_device_t *d;
	sccp_moo_t * r;
	if (state == -1 || !h || !h->device || !h->device->session)
		return 0;

	d = h->device;
	REQ(r, CallInfoMessage);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: HINT notify state %s (%d), instance %d\n", d->id, sccp_extensionstate2str(state), state, h->instance);
	switch(state) {
		case AST_EXTENSION_NOT_INUSE:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_OFF);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_ONHOOK);
			return 0;
		case AST_EXTENSION_INUSE:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_ON);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
		case AST_EXTENSION_BUSY:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_ON);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_BUSY, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_BUSY, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
		case AST_EXTENSION_UNAVAILABLE:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_FLASH);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
#ifdef CS_AST_HAS_EXTENSION_RINGING
		case AST_EXTENSION_RINGING:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_FLASH);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(r->msg.CallInfoMessage.calledPartyName));
			break;
#endif
		default:
			sccp_dev_set_lamp(d, SKINNY_STIMULUS_LINE, h->instance, SKINNY_LAMP_FLASH);
			sccp_channel_set_callstate_full(d, h->instance, 0, SKINNY_CALLSTATE_CALLREMOTEMULTILINE);
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.callingPartyName));
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(r->msg.CallInfoMessage.calledPartyName));
	}

	sccp_copy_string(r->msg.CallInfoMessage.callingParty, "", sizeof(r->msg.CallInfoMessage.callingParty));
	sccp_copy_string(r->msg.CallInfoMessage.calledParty, "", sizeof(r->msg.CallInfoMessage.calledParty));

	r->msg.CallInfoMessage.lel_lineId   = htolel(h->instance);
	r->msg.CallInfoMessage.lel_callRef  = htolel(0);
	r->msg.CallInfoMessage.lel_callType = htolel(SKINNY_CALLTYPE_OUTBOUND);
	sccp_dev_send(d, r);
	sccp_dev_set_keyset(d, h->instance, 0, KEYMODE_MYST);
	return 0;
}


/**
 * 
 * \brief controller function to handle received messages
 * \param r message
 * \param s session
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s) {
  uint32_t  mid = letohl(r->lel_messageId);
  s->lastKeepAlive = time(0); /* always update keepalive */

  if ( (!s->device) && (mid != RegisterMessage && mid != UnregisterMessage && mid != RegisterTokenReq && mid != AlarmMessage && mid != KeepAliveMessage && mid != IpPortMessage)) {
	ast_log(LOG_WARNING, "Client sent %s without first registering.\n", sccpmsg2str(mid));
	free(r);
	return 0;
  }

  if (mid != KeepAliveMessage) {
	if (s && s->device) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: >> Got message %s\n", s->device->id, sccpmsg2str(mid));
	} else {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: >> Got message %s\n", sccpmsg2str(mid));
	}
  }

  switch (mid) {

  case AlarmMessage:
	sccp_handle_alarm(s, r);
	break;
	case RegisterMessage:
	case RegisterTokenReq:
	sccp_handle_register(s, r);
	break;
  case UnregisterMessage:
	sccp_handle_unregister(s, r);
	break;
  case VersionReqMessage:
	sccp_handle_version(s, r);
	break;
  case CapabilitiesResMessage:
	sccp_handle_capabilities_res(s, r);
	break;
  case ButtonTemplateReqMessage:
	sccp_handle_button_template_req(s, r);
	break;
  case SoftKeyTemplateReqMessage:
	sccp_handle_soft_key_template_req(s, r);
	break;
  case SoftKeySetReqMessage:
	sccp_handle_soft_key_set_req(s, r);
	break;
  case LineStatReqMessage:
	sccp_handle_line_number(s, r);
	break;
  case SpeedDialStatReqMessage:
	sccp_handle_speed_dial_stat_req(s, r);
	break;
  case StimulusMessage:
	sccp_handle_stimulus(s, r);
	break;
  case OffHookMessage:
	sccp_handle_offhook(s, r);
	break;
  case OnHookMessage:
	sccp_handle_onhook(s, r);
	break;
  case HeadsetStatusMessage:
	sccp_handle_headset(s, r);
	break;
  case TimeDateReqMessage:
	sccp_handle_time_date_req(s, r);
	break;
  case KeypadButtonMessage:
	sccp_handle_keypad_button(s, r);
	break;
  case SoftKeyEventMessage:
	sccp_handle_soft_key_event(s, r);
	break;
  case KeepAliveMessage:
	sccp_session_sendmsg(s, KeepAliveAckMessage);
	sccp_dev_check_mwi(s->device);
	break;
  case IpPortMessage:
  	/* obsolete message */
	s->rtpPort = letohs(r->msg.IpPortMessage.les_rtpMediaPort);
	break;
  case OpenReceiveChannelAck:
	sccp_handle_open_receive_channel_ack(s, r);
	break;
  case ConnectionStatisticsRes:
	sccp_handle_ConnectionStatistics(s,r);
	break;
  case ServerReqMessage:
	sccp_handle_ServerResMessage(s,r);
	break;
  case ConfigStatReqMessage:
	sccp_handle_ConfigStatMessage(s,r);
	break;
  case EnblocCallMessage:
	sccp_handle_EnblocCallMessage(s,r);
	break;
  case RegisterAvailableLinesMessage:
	if (s->device)
		sccp_dev_set_registered(s->device, SKINNY_DEVICE_RS_OK);
	break;
  case ForwardStatReqMessage:
	sccp_handle_forward_stat_req(s,r);
	break;
  case FeatureStatReqMessage:
	sccp_handle_feature_stat_req(s,r);
	break;
  case ServiceURLStatReqMessage:
	sccp_handle_servicesurl_stat_req(s,r);
  	break;
  default:
	if (GLOB(debug))
		ast_log(LOG_WARNING, "Unhandled SCCP Message: %d - %s with length %d\n", mid, sccpmsg2str(mid), r->length);
  }

  free(r);
  return 1;
}


/**
 * Creates a \link sccp_line_t line \endlink with default/global values 
 * 
 * \brief build default line.
 * \return default line
 * 
 * 
 */
sccp_line_t * build_line(void) {
	sccp_line_t * l = malloc(sizeof(sccp_line_t));
	if (!l) {
		sccp_log(0)(VERBOSE_PREFIX_3 "Unable to allocate memory for a line\n");
		return NULL;
	}
	memset(l, 0, sizeof(sccp_line_t));
	ast_mutex_init(&l->lock);
	l->instance = -1;
	l->incominglimit = 3; /* default value */
	l->echocancel = GLOB(echocancel); /* default value */
	l->silencesuppression = GLOB(silencesuppression); /* default value */
	l->rtptos = GLOB(rtptos); /* default value */
	l->transfer = 1; /* default value. on if the device transfer is on*/
	l->secondary_dialtone_tone = SKINNY_TONE_OUTSIDEDIALTONE;
	l->dndmode = SCCP_DNDMODE_OFF;

	sccp_copy_string(l->context, GLOB(context), sizeof(l->context));
	sccp_copy_string(l->language, GLOB(language), sizeof(l->language));
	sccp_copy_string(l->accountcode, GLOB(accountcode), sizeof(l->accountcode));
	sccp_copy_string(l->musicclass, GLOB(musicclass), sizeof(l->musicclass));
	l->amaflags = GLOB(amaflags);
	l->callgroup = GLOB(callgroup);
#ifdef CS_SCCP_PICKUP
	l->pickupgroup = GLOB(pickupgroup);
#endif

  return l;
}


/**
 * \brief creates configured line from \link ast_variable asterisk variable \endlink
 * \return configured line
 * \note also used by realtime functionality to line device from \link ast_variable asterisk variable \endlink
 */
sccp_line_t * build_lines(struct ast_variable *v) {
	sccp_line_t * l, *gl;
	int amaflags = 0;
	int secondary_dialtone_tone = 0;

 	l = build_line();
 	while(v) {
 			if (!strcasecmp(v->name, "line")) {
 				if ( !ast_strlen_zero(v->value) ) {
 					sccp_copy_string(l->name, ast_strip(v->value), sizeof(l->name));
 					
 					/* search for existing line */
					gl = GLOB(lines);
					while(gl && strcasecmp(gl->name, v->value) != 0) {
	 					gl = gl->next;
	 				}
 					if (gl && (strcasecmp(gl->name, v->value) == 0) ){					
 						ast_log(LOG_WARNING, "The line %s already exists\n", gl->name);
 						free(l);
 					}else {
 						ast_verbose(VERBOSE_PREFIX_3 "Added line '%s'\n", l->name);
 						ast_mutex_lock(&GLOB(lines_lock));
 						l->next = GLOB(lines);
 						if (l->next)
 							l->next->prev = l;
 						GLOB(lines) = l;
 						ast_mutex_unlock(&GLOB(lines_lock));
 					}
					free(gl);
 				} else {
 					ast_log(LOG_WARNING, "Wrong line param: %s => %s\n", v->name, v->value);
 					free(l);
 				}
 				l = build_line();
 			}
#ifdef CS_SCCP_REALTIME
			if (!strcasecmp(v->name, "name")) {
 				if ( !ast_strlen_zero(v->value) ) {
 					sccp_copy_string(l->name, ast_strip(v->value), sizeof(l->name));
 					
 					/* search for existing line */
					ast_mutex_lock(&GLOB(lines_lock));
					gl = GLOB(lines);
					while(gl && strcasecmp(gl->name, v->value) != 0) {
	 					gl = gl->next;
	 				}
					ast_mutex_unlock(&GLOB(lines_lock));
 					if (gl && (strcasecmp(gl->name, v->value) == 0) ){					
 						ast_log(LOG_WARNING, "The line %s already exists\n", gl->name);
 						free(l);
 					}else {
 						ast_verbose(VERBOSE_PREFIX_3 "Added line '%s'\n", l->name);
 						ast_mutex_lock(&GLOB(lines_lock));
 						l->next = GLOB(lines);
 						if (l->next)
 							l->next->prev = l;
 						GLOB(lines) = l;
 						ast_mutex_unlock(&GLOB(lines_lock));
 					}
					free(gl);
 				} else {
 					ast_log(LOG_WARNING, "Wrong line param: %s => %s\n", v->name, v->value);
 					free(l);
 				}
 			} 
#endif
			  else if (!strcasecmp(v->name, "id")) {
 				sccp_copy_string(l->id, v->value, sizeof(l->id));
 			} else if (!strcasecmp(v->name, "pin")) {
 				sccp_copy_string(l->pin, v->value, sizeof(l->pin));
 			} else if (!strcasecmp(v->name, "label")) {
 				sccp_copy_string(l->label, v->value, sizeof(l->label));
 			} else if (!strcasecmp(v->name, "description")) {
 				sccp_copy_string(l->description, v->value, sizeof(l->description));
 			} else if (!strcasecmp(v->name, "context")) {
 				sccp_copy_string(l->context, v->value, sizeof(l->context));
 			} else if (!strcasecmp(v->name, "cid_name")) {
 				sccp_copy_string(l->cid_name, v->value, sizeof(l->cid_name));
 			} else if (!strcasecmp(v->name, "cid_num")) {
 				sccp_copy_string(l->cid_num, v->value, sizeof(l->cid_num));
 			} else if (!strcasecmp(v->name, "callerid")) {
 				ast_log(LOG_WARNING, "obsolete callerid param. Use cid_num and cid_name\n");
 			} else if (!strcasecmp(v->name, "mailbox")) {
 				sccp_copy_string(l->mailbox, v->value, sizeof(l->mailbox));
 			} else if (!strcasecmp(v->name, "vmnum")) {
 				sccp_copy_string(l->vmnum, v->value, sizeof(l->vmnum));
 			} else if (!strcasecmp(v->name, "transfer")) {
 				l->transfer = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "incominglimit")) {
 				l->incominglimit = atoi(v->value);
 				if (l->incominglimit < 1)
 				l->incominglimit = 1;
 				/* this is the max call phone limits. Just a guess. It's not important */
 				if (l->incominglimit > 10)
 				l->incominglimit = 10;
 			} else if (!strcasecmp(v->name, "echocancel")) {
 					l->echocancel = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "silencesuppression")) {
 				l->silencesuppression = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "rtptos")) {
 				if (sscanf(v->value, "%d", &l->rtptos) == 1)
 					l->rtptos &= 0xff;
 			} else if (!strcasecmp(v->name, "language")) {
 				sccp_copy_string(l->language, v->value, sizeof(l->language));

 			} else if (!strcasecmp(v->name, "musicclass")) {
 				sccp_copy_string(l->musicclass, v->value, sizeof(l->musicclass));
 			} else if (!strcasecmp(v->name, "accountcode")) {
 				sccp_copy_string(l->accountcode, v->value, sizeof(l->accountcode));
 			} else if (!strcasecmp(v->name, "amaflags")) {
 				amaflags = ast_cdr_amaflags2int(v->value);
 				if (amaflags < 0) {
 					ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
 				} else {
 					l->amaflags = amaflags;
 				}
 			} else if (!strcasecmp(v->name, "callgroup")) {
 				l->callgroup = ast_get_group(v->value);
 	#ifdef CS_SCCP_PICKUP
 			} else if (!strcasecmp(v->name, "pickupgroup")) {
 				l->pickupgroup = ast_get_group(v->value);
 	#endif
 			} else if (!strcasecmp(v->name, "trnsfvm")) {
 				if (!ast_strlen_zero(v->value)) {
 					if (ast_exists_extension(NULL, l->context, v->value, 1, l->cid_num)) {
 						l->trnsfvm = strdup(v->value);
 					} else {
 						ast_log(LOG_WARNING, "trnsfvm: %s is not a valid extension. Disabled!\n", v->value);
 					}
 				}
 			} else if (!strcasecmp(v->name, "secondary_dialtone_digits")) {
 				if (strlen(v->value) > 9)
 					ast_log(LOG_WARNING, "secondary_dialtone_digits value '%s' is too long at line %d of SCCP.CONF. Max 9 digits\n", v->value, v->lineno);
 				sccp_copy_string(l->secondary_dialtone_digits, v->value, sizeof(l->secondary_dialtone_digits));
 			} else if (!strcasecmp(v->name, "secondary_dialtone_tone")) {
 				if (sscanf(v->value, "%i", &secondary_dialtone_tone) == 1) {
 					if (secondary_dialtone_tone >= 0 && secondary_dialtone_tone <= 255)
 						l->secondary_dialtone_tone = secondary_dialtone_tone;
 					else
 						l->secondary_dialtone_tone = SKINNY_TONE_OUTSIDEDIALTONE;
 				} else {
 					ast_log(LOG_WARNING, "Invalid secondary_dialtone_tone value '%s' at line %d of SCCP.CONF. Default is OutsideDialtone (0x22)\n", v->value, v->lineno);
 				}
 			} else if (!strcasecmp(v->name, "setvar")) {
				struct ast_variable *newvar = NULL;
				
				newvar = sccp_create_variable(v->value);
				if(newvar){
					sccp_log(10)(VERBOSE_PREFIX_3 "Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);				
					newvar->next = l->variables;
					l->variables = newvar;
				}
 			} else if (!strcasecmp(v->name, "dnd")) {
				if (!strcasecmp(v->value, "reject")) {
					l->dndmode = SCCP_DNDMODE_REJECT;
				} else if (!strcasecmp(v->value, "silent")) {
					l->dndmode = SCCP_DNDMODE_SILENT;
				} else if (!strcasecmp(v->value, "user")) {
					l->dndmode = SCCP_DNDMODE_USERDEFINED;
				} else {
					/* 0 is off and 1 (on) is reject */
					l->dndmode = sccp_true(v->value);
				}
			} else {
 				ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
 			}
 			v = v->next;
 		}

	return l;
}

/**
 * \brief Build default device.
 * \return device with default/global values
 * 
 */
sccp_device_t * build_device(void) {
	sccp_device_t * d = malloc(sizeof(sccp_device_t));
	if (!d) {
		sccp_log(0)(VERBOSE_PREFIX_3 "Unable to allocate memory for a device\n");
		return NULL;
	}
	memset(d, 0, sizeof(sccp_device_t));
	ast_mutex_init(&d->lock);

	d->keepalive = GLOB(keepalive);
	d->tz_offset = 0;
	d->capability = GLOB(global_capability);
	d->codecs = GLOB(global_codecs);
	d->transfer = 1;
	d->state = SCCP_DEVICESTATE_ONHOOK;
	d->ringermode = SKINNY_STATION_RINGOFF;
	d->dndmode = GLOB(dndmode);
	d->trustphoneip = GLOB(trustphoneip);
	d->private = GLOB(private);
	d->earlyrtp = GLOB(earlyrtp);
	d->mwilamp = GLOB(mwilamp);
	d->mwioncall = GLOB(mwioncall);
	d->cfwdall = GLOB(cfwdall);
	d->cfwdbusy = GLOB(cfwdbusy);
	d->postregistration_thread = AST_PTHREADT_STOP;

#ifdef CS_SCCP_PARK
	d->park = 1;
#else
	d->park = 0;
#endif
  
  d->selectedChannels = NULL;

  return d;
}

/**
 * \brief Create device from ast_variable
 * \return configured device 
 * \note also used by realtime functionality to build device from \link ast_variable asterisk variable \endlink
 */
sccp_device_t *build_devices(struct ast_variable *v) {
	sccp_hostname_t *permithost;
	sccp_device_t 	*d;
	sccp_speed_t 	*k = NULL, *k_last = NULL;
	sccp_serviceURL_t	*serviceURL = NULL, *serviceURL_last = NULL;
	char 			*splitter, *k_exten = NULL, *k_name = NULL, *k_hint = NULL;
	char 			k_speed[256];
	char 			serviceURLOptionString[1024];
	char 			*serviceURLLabel = NULL, *serviceURLURL = NULL;
	uint8_t 		speeddial_index = 1;
	uint8_t			serviceURLIndex = 1;
	char 			message[256]="";							//device message
	int				res;

	d = build_device();
	while (v) {	
			sccp_log(10)(VERBOSE_PREFIX_3 "%s = %s\n", v->name, v->value);
			if (!strcasecmp(v->name, "device")) {
				if ( (strlen(v->value) == 15) && ((strncmp(v->value, "SEP",3) == 0) || (strncmp(v->value, "ATA",3)==0)) ) {
					sccp_copy_string(d->id, v->value, sizeof(d->id));
					res=ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
					if (!res) 
						d->phonemessage=strdup(message);									//set message on device if we have a result
					strcpy(message,"");
					ast_verbose(VERBOSE_PREFIX_3 "Added device '%s' (%s) \n", d->id, d->config_type);
					ast_mutex_lock(&GLOB(devices_lock));
					d->next = GLOB(devices);
					GLOB(devices) = d;
					ast_mutex_unlock(&GLOB(devices_lock));
					d = build_device();
					speeddial_index = 1;
					serviceURLIndex = 1;
					serviceURL_last = NULL;
					k_last = NULL;
				} else {
					ast_log(LOG_WARNING, "Wrong device param: %s => %s\n", v->name, v->value);
					sccp_dev_clean(d);
					ast_mutex_destroy(&d->lock);
					free(d);
				}
				
			} 
#ifdef CS_SCCP_REALTIME
			else if (!strcasecmp(v->name, "name")) {
				if ( (strlen(v->value) == 15) && ((strncmp(v->value, "SEP",3) == 0) || (strncmp(v->value, "ATA",3)==0)) ) {
					sccp_copy_string(d->id, v->value, sizeof(d->id));
					res=ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
					if (!res) 
						d->phonemessage=strdup(message);									//set message on device if we have a result
					strcpy(message,"");
					ast_verbose(VERBOSE_PREFIX_3 "Added device '%s' (%s)\n", d->id, d->config_type);
					ast_mutex_lock(&GLOB(devices_lock));
					d->next = GLOB(devices);
					GLOB(devices) = d;
					ast_mutex_unlock(&GLOB(devices_lock));

				} else {
					ast_log(LOG_WARNING, "Wrong device param: %s => %sn", v->name, v->value);
					sccp_dev_clean(d);
					free(d);
				}
			}
#endif
			else if (!strcasecmp(v->name, "keepalive")) {
				d->keepalive = atoi(v->value);
			} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
				d->ha = ast_append_ha(v->name, v->value, d->ha);
			} else if (!strcasecmp(v->name, "permithost")) {
				if ((permithost = malloc(sizeof(sccp_hostname_t)))) {
					sccp_copy_string(permithost->name, v->value, sizeof(permithost->name));
					permithost->next = d->permithost;
					d->permithost = permithost;
				} else {
					ast_log(LOG_WARNING, "Error adding the permithost = %s to the list\n", v->value);
				}
			} else if (!strcasecmp(v->name, "type")) {
				sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
			} else if (!strcasecmp(v->name, "tzoffset")) {
				/* timezone offset */
				d->tz_offset = atoi(v->value);
			} else if (!strcasecmp(v->name, "autologin")) {
				sccp_copy_string(d->autologin, v->value, sizeof(d->autologin));
			} else if (!strcasecmp(v->name, "description")) {
				sccp_copy_string(d->description, v->value, sizeof(d->description));
			} else if (!strcasecmp(v->name, "imageversion")) {
				sccp_copy_string(d->imageversion, v->value, sizeof(d->imageversion));
			} else if (!strcasecmp(v->name, "allow")) {
				ast_parse_allow_disallow(&d->codecs, &d->capability, v->value, 1);
			} else if (!strcasecmp(v->name, "disallow")) {
				ast_parse_allow_disallow(&d->codecs, &d->capability, v->value, 0);
			} else if (!strcasecmp(v->name, "transfer")) {
				d->transfer = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "cfwdall")) {
				d->cfwdall = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "cfwdbusy")) {
				d->cfwdbusy = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "dnd")) {
				if (!strcasecmp(v->value, "reject")) {
					d->dndmode = SCCP_DNDMODE_REJECT;
				} else if (!strcasecmp(v->value, "silent")) {
					d->dndmode = SCCP_DNDMODE_SILENT;
				} else if (!strcasecmp(v->value, "user")) {
					d->dndmode = SCCP_DNDMODE_USERDEFINED;
				} else {
					/* 0 is off and 1 (on) is reject */
					d->dndmode = sccp_true(v->value);
				}
			} else if (!strcasecmp(v->name, "trustphoneip")) {
				d->trustphoneip = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "private")) {
				d->private = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "earlyrtp")) {
				if (!strcasecmp(v->value, "none"))
					d->earlyrtp = 0;
				else if (!strcasecmp(v->value, "offhook"))
					d->earlyrtp = SCCP_CHANNELSTATE_OFFHOOK;
				else if (!strcasecmp(v->value, "dial"))
					d->earlyrtp = SCCP_CHANNELSTATE_DIALING;
				else if (!strcasecmp(v->value, "ringout"))
					d->earlyrtp = SCCP_CHANNELSTATE_RINGOUT;
				else
					ast_log(LOG_WARNING, "Invalid earlyrtp state value at line %d, should be 'none', 'offhook', 'dial', 'ringout'\n", v->lineno);
			} else if (!strcasecmp(v->name, "dtmfmode")) {
				if (!strcasecmp(v->value, "outofband"))
					d->dtmfmode = SCCP_DTMFMODE_OUTOFBAND;
		#ifdef CS_SCCP_PARK
			} else if (!strcasecmp(v->name, "park")) {
				d->park = sccp_true(v->value);
		#endif
			} else if (!strcasecmp(v->name, "speeddial")) {
				if (ast_strlen_zero(v->value)) {
					speeddial_index++;
					ast_verbose(VERBOSE_PREFIX_3 "Added empty speeddial\n");
				} else {
					sccp_copy_string(k_speed, v->value, sizeof(k_speed));
					splitter = k_speed;
					k_exten = strsep(&splitter, ",");
					k_name = strsep(&splitter, ",");
					k_hint = splitter;
					if (k_exten)
						ast_strip(k_exten);
					if (k_name)
						ast_strip(k_name);
					if (k_hint)
						ast_strip(k_hint);
					if (k_exten && !ast_strlen_zero(k_exten)) {
						if (!k_name)
							k_name = k_exten;
						k = malloc(sizeof(sccp_speed_t));
						if (!k)
							ast_log(LOG_WARNING, "Error allocating speedial %s => %s\n", v->name, v->value);
						else {
							memset(k, 0, sizeof(sccp_speed_t));
							sccp_copy_string(k->name, k_name, sizeof(k->name));
							sccp_copy_string(k->ext, k_exten, sizeof(k->ext));
							if (k_hint)
								sccp_copy_string(k->hint, k_hint, sizeof(k->hint));
							k->config_instance = speeddial_index++;
							if (!d->speed_dials)
								d->speed_dials = k;
							if (!k_last)
								k_last = k;
							else {
								k_last->next = k;
								k_last = k;
							}
							ast_verbose(VERBOSE_PREFIX_3 "Added speeddial %d: %s (%s)\n", k->config_instance, k->name, k->ext);
						}
					} else
						ast_log(LOG_WARNING, "Wrong speedial syntax: %s => %s\n", v->name, v->value);
				}
			} else if (!strcasecmp(v->name, "mwilamp")) {
				if (!strcasecmp(v->value, "off"))
					d->mwilamp = SKINNY_LAMP_OFF;
				else if (!strcasecmp(v->value, "on"))
					d->mwilamp = SKINNY_LAMP_ON;
				else if (!strcasecmp(v->value, "wink"))
					d->mwilamp = SKINNY_LAMP_WINK;
				else if (!strcasecmp(v->value, "flash"))
					d->mwilamp = SKINNY_LAMP_FLASH;
				else if (!strcasecmp(v->value, "blink"))
					d->mwilamp = SKINNY_LAMP_BLINK;
				else
					ast_log(LOG_WARNING, "Invalid mwilamp value at line %d, should be 'off', 'on', 'wink', 'flash' or 'blink'\n", v->lineno);
			} else if (!strcasecmp(v->name, "mwioncall")) {
					d->mwioncall = sccp_true(v->value);
			}else if (!strcasecmp(v->name, "setvar")) {
				struct ast_variable *newvar = NULL;
				
				newvar = sccp_create_variable(v->value);
				if(newvar){
					sccp_log(10)(VERBOSE_PREFIX_3 "Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);				
					newvar->next = d->variables;
					d->variables = newvar;
				}
 			} else if (!strcasecmp(v->name, "serviceURL")) {
				if (ast_strlen_zero(v->value)) {
					serviceURLIndex++;
					ast_verbose(VERBOSE_PREFIX_3 "Added empty serviceURL\n");
				} else {
					sccp_copy_string(serviceURLOptionString, v->value, sizeof(serviceURLOptionString));
					splitter = serviceURLOptionString;
					serviceURLLabel = strsep(&splitter, ",");
					serviceURLURL = splitter;
					if(serviceURLLabel)
						ast_strip(serviceURLLabel);
					if(serviceURLURL)
						ast_strip(serviceURLURL);
					if((serviceURLURL && serviceURLLabel) && 	(!ast_strlen_zero(serviceURLURL) && !ast_strlen_zero(serviceURLLabel))) 
					{
						serviceURL = malloc(sizeof(sccp_serviceURL_t));
						if (!serviceURL)
						{
							ast_log(LOG_WARNING, "Error allocating serviceURL %s => %s\n", v->name, v->value);
						}
						else
						{
							memset(serviceURL, 0, sizeof(sccp_serviceURL_t));
							sccp_copy_string(serviceURL->label, serviceURLLabel, strlen(serviceURLLabel)+1);
							sccp_copy_string(serviceURL->URL, serviceURLURL, strlen(serviceURLURL)+1);
							serviceURL->config_instance = serviceURLIndex++;
							ast_verbose(VERBOSE_PREFIX_3 "Add serviceURL: %s as instance %d\n",serviceURLURL, serviceURL->config_instance);
							if (!d->serviceURLs)
								d->serviceURLs = serviceURL;
							if (!serviceURL_last)
								serviceURL_last = serviceURL;
							else {
								serviceURL_last->next = serviceURL;
								serviceURL_last = serviceURL;
							}
						}
					}
				}
			} else {
				ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
			}
			v = v->next;
		}

	return d;
}
/**
 * \brief reload the configuration from sccp.conf
 * 
 */
static int reload_config(void) {
	struct ast_config		*cfg;
	struct ast_variable	*v;
	int						oldport	= ntohs(GLOB(bindaddr.sin_port));
	int						on		= 1;
	int						tos		= 0;
	char					pref_buf[128];
	struct ast_hostent		ahp;
	struct hostent			*hp;
	struct ast_ha 			*na;
#ifdef ASTERISK_CONF_1_2
	char					iabuf[INET_ADDRSTRLEN];
#endif
	sccp_device_t			*d;
	sccp_line_t				*l = NULL;
	int firstdigittimeout = 0;
	int digittimeout = 0;
	int autoanswer_ring_time = 0;
	int autoanswer_tone = 0;
	int remotehangup_tone = 0;
	int transfer_tone = 0;
	int callwaiting_tone = 0;
	int amaflags = 0;
	int protocolversion = 0;
	

	memset(&GLOB(global_codecs), 0, sizeof(GLOB(global_codecs)));
	memset(&GLOB(bindaddr), 0, sizeof(GLOB(bindaddr)));

#ifdef CS_SCCP_REALTIME
	sccp_copy_string(GLOB(realtimedevicetable), "sccpdevice", sizeof(GLOB(realtimedevicetable)));
	sccp_copy_string(GLOB(realtimelinetable) , "sccpline", sizeof(GLOB(realtimelinetable)) );
#endif	

#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	sccp_log(0)(VERBOSE_PREFIX_2 "Platform byte order   : LITTLE ENDIAN\n");
#else
	sccp_log(0)(VERBOSE_PREFIX_2 "Platform byte order   : BIG ENDIAN\n");
#endif

	cfg = ast_config_load("sccp.conf");
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return 0;
	}
	/* read the general section */
	v = ast_variable_browse(cfg, "general");
	if (!v) {
		ast_log(LOG_WARNING, "Missing [general] section, SCCP disabled\n");
		return 0;
	}

	while (v) {
		if (!strcasecmp(v->name, "protocolversion")) {
			if (sscanf(v->value, "%i", &protocolversion) == 1) {
				if (protocolversion < 2 || protocolversion > 6)
					GLOB(protocolversion) = 3;
				else
					GLOB(protocolversion) = protocolversion;
			} else {
				ast_log(LOG_WARNING, "Invalid protocolversion number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "servername")) {
			sccp_copy_string(GLOB(servername), v->value, sizeof(GLOB(servername)));
		} else if (!strcasecmp(v->name, "bindaddr")) {
			if (!(hp = ast_gethostbyname(v->value, &ahp))) {
				ast_log(LOG_WARNING, "Invalid address: %s. SCCP disabled\n", v->value);
				return 0;
			} else {
				memcpy(&GLOB(bindaddr.sin_addr), hp->h_addr, sizeof(GLOB(bindaddr.sin_addr)));
			}
		} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
			GLOB(ha) = ast_append_ha(v->name, v->value, GLOB(ha));
		} else if (!strcasecmp(v->name, "localnet")) {
			if (!(na = ast_append_ha("d", v->value, GLOB(localaddr))))
				ast_log(LOG_WARNING, "Invalid localnet value: %s\n", v->value);
			else
				GLOB(localaddr) = na;
		} else if (!strcasecmp(v->name, "externip")) {
			if (!(hp = ast_gethostbyname(v->value, &ahp)))
				ast_log(LOG_WARNING, "Invalid address for externip keyword: %s\n", v->value);
			else
				memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
			GLOB(externexpire) = 0;
		} else if (!strcasecmp(v->name, "externhost")) {
			sccp_copy_string(GLOB(externhost), v->value, sizeof(GLOB(externhost)));
			if (!(hp = ast_gethostbyname(GLOB(externhost), &ahp)))
				ast_log(LOG_WARNING, "Invalid address resolution for externhost keyword: %s\n", GLOB(externhost));
			else
				memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
			time(&GLOB(externexpire));
		} else if (!strcasecmp(v->name, "externrefresh")) {
			if (sscanf(v->value, "%d", &GLOB(externrefresh)) != 1) {
				ast_log(LOG_WARNING, "Invalid externrefresh value '%s', must be an integer >0 at line %d\n", v->value, v->lineno);
				GLOB(externrefresh) = 10;
			}
		} else if (!strcasecmp(v->name, "keepalive")) {
			GLOB(keepalive) = atoi(v->value);
		} else if (!strcasecmp(v->name, "context")) {
			sccp_copy_string(GLOB(context), v->value, sizeof(GLOB(context)));
		} else if (!strcasecmp(v->name, "language")) {
			sccp_copy_string(GLOB(language), v->value, sizeof(GLOB(language)));
		} else if (!strcasecmp(v->name, "accountcode")) {
			sccp_copy_string(GLOB(accountcode), v->value, sizeof(GLOB(accountcode)));
		} else if (!strcasecmp(v->name, "amaflags")) {
			amaflags = ast_cdr_amaflags2int(v->value);
			if (amaflags < 0) {
				ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
			} else {
				GLOB(amaflags) = amaflags;
		}
/*
		} else if (!strcasecmp(v->name, "mohinterpret") || !strcasecmp(v->name, "mohinterpret") 	) {
			sccp_copy_string(GLOB(mohinterpret), v->value, sizeof(GLOB(mohinterpret)));
*/
		} else if (!strcasecmp(v->name, "musicclass")) {
			sccp_copy_string(GLOB(musicclass), v->value, sizeof(GLOB(musicclass)));
		} else if (!strcasecmp(v->name, "callgroup")) {
			GLOB(callgroup) = ast_get_group(v->value);
#ifdef CS_SCCP_PICKUP
		} else if (!strcasecmp(v->name, "pickupgroup")) {
			GLOB(pickupgroup) = ast_get_group(v->value);
#endif
		} else if (!strcasecmp(v->name, "dateformat")) {
			sccp_copy_string (GLOB(date_format), v->value, sizeof(GLOB(date_format)));
		} else if (!strcasecmp(v->name, "port")) {
			if (sscanf(v->value, "%i", &GLOB(ourport)) == 1) {
				GLOB(bindaddr.sin_port) = htons(GLOB(ourport));
			} else {
				ast_log(LOG_WARNING, "Invalid port number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "firstdigittimeout")) {
			if (sscanf(v->value, "%i", &firstdigittimeout) == 1) {
				if (firstdigittimeout > 0 && firstdigittimeout < 255)
					GLOB(firstdigittimeout) = firstdigittimeout;
			} else {
				ast_log(LOG_WARNING, "Invalid firstdigittimeout number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "digittimeout")) {
			if (sscanf(v->value, "%i", &digittimeout) == 1) {
				if (digittimeout > 0 && digittimeout < 255)
					GLOB(digittimeout) = digittimeout;
			} else {
				ast_log(LOG_WARNING, "Invalid firstdigittimeout number '%s' at line %d of SCCP.CONF\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "digittimeoutchar")) {
			GLOB(digittimeoutchar) = v->value[0];
		} else if (!strcasecmp(v->name, "recorddigittimeoutchar")) {
			GLOB(recorddigittimeoutchar) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "debug")) {
			GLOB(debug) = atoi(v->value);
		} else if (!strcasecmp(v->name, "allow")) {
			ast_parse_allow_disallow(&GLOB(global_codecs), &GLOB(global_capability), v->value, 1);
		} else if (!strcasecmp(v->name, "disallow")) {
			ast_parse_allow_disallow(&GLOB(global_codecs), &GLOB(global_capability), v->value, 0);
		} else if (!strcasecmp(v->name, "dnd")) {
			if (!strcasecmp(v->value, "reject")) {
				GLOB(dndmode) = SCCP_DNDMODE_REJECT;
			} else if (!strcasecmp(v->value, "silent")) {
				GLOB(dndmode) = SCCP_DNDMODE_SILENT;
			} else {
				/* 0 is off and 1 (on) is reject */
				GLOB(dndmode) = sccp_true(v->value);
			}
		} else if (!strcasecmp(v->name, "cfwdall")) {
			GLOB(cfwdall) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "cfwdbusy")) {
			GLOB(cfwdbusy) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "echocancel")) {
			GLOB(echocancel) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "silencesuppression")) {
			GLOB(silencesuppression) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "trustphoneip")) {
			GLOB(trustphoneip) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "private")) {
			GLOB(private) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "earlyrtp")) {
			if (!strcasecmp(v->value, "none"))
				GLOB(earlyrtp) = 0;
			else if (!strcasecmp(v->value, "offhook"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_OFFHOOK;
			else if (!strcasecmp(v->value, "dial"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_DIALING;
			else if (!strcasecmp(v->value, "ringout"))
				GLOB(earlyrtp) = SCCP_CHANNELSTATE_RINGOUT;
			else
				ast_log(LOG_WARNING, "Invalid earlyrtp state value at line %d, should be 'none', 'offhook', 'dial', 'ringout'\n", v->lineno);
		} else if (!strcasecmp(v->name, "tos")) {
			if (sscanf(v->value, "%d", &tos) == 1)
				GLOB(tos) = tos & 0xff;
			else if (!strcasecmp(v->value, "lowdelay"))
				GLOB(tos) = IPTOS_LOWDELAY;
			else if (!strcasecmp(v->value, "throughput"))
				GLOB(tos) = IPTOS_THROUGHPUT;
			else if (!strcasecmp(v->value, "reliability"))
				GLOB(tos) = IPTOS_RELIABILITY;
			#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
			else if (!strcasecmp(v->value, "mincost"))
				GLOB(tos) = IPTOS_MINCOST;
			#endif
			else if (!strcasecmp(v->value, "none"))
				GLOB(tos) = 0;
			else
			#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
				ast_log(LOG_WARNING, "Invalid tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', 'mincost', or 'none'\n", v->lineno);
			#else
				ast_log(LOG_WARNING, "Invalid tos value at line %d, should be 'lowdelay', 'throughput', 'reliability', or 'none'\n", v->lineno);
			#endif
		} else if (!strcasecmp(v->name, "rtptos")) {
			if (sscanf(v->value, "%d", &GLOB(rtptos)) == 1)
				GLOB(rtptos) &= 0xff;
		} else if (!strcasecmp(v->name, "autoanswer_ring_time")) {
			if (sscanf(v->value, "%i", &autoanswer_ring_time) == 1) {
				if (autoanswer_ring_time >= 0 && autoanswer_ring_time <= 255)
					GLOB(autoanswer_ring_time) = autoanswer_ring_time;
			} else {
				ast_log(LOG_WARNING, "Invalid autoanswer_ring_time value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "autoanswer_tone")) {
			if (sscanf(v->value, "%i", &autoanswer_tone) == 1) {
				if (autoanswer_tone >= 0 && autoanswer_tone <= 255)
					GLOB(autoanswer_tone) = autoanswer_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid autoanswer_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "remotehangup_tone")) {
			if (sscanf(v->value, "%i", &remotehangup_tone) == 1) {
				if (remotehangup_tone >= 0 && remotehangup_tone <= 255)
					GLOB(remotehangup_tone) = remotehangup_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid remotehangup_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "transfer_tone")) {
			if (sscanf(v->value, "%i", &transfer_tone) == 1) {
				if (transfer_tone >= 0 && transfer_tone <= 255)
					GLOB(transfer_tone) = transfer_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid transfer_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "callwaiting_tone")) {
			if (sscanf(v->value, "%i", &callwaiting_tone) == 1) {
				if (callwaiting_tone >= 0 && callwaiting_tone <= 255)
					GLOB(callwaiting_tone) = callwaiting_tone;
			} else {
				ast_log(LOG_WARNING, "Invalid callwaiting_tone value '%s' at line %d of SCCP.CONF. Default is 0\n", v->value, v->lineno);
			}
		} else if (!strcasecmp(v->name, "mwilamp")) {
			if (!strcasecmp(v->value, "off"))
				GLOB(mwilamp) = SKINNY_LAMP_OFF;
			else if (!strcasecmp(v->value, "on"))
				GLOB(mwilamp) = SKINNY_LAMP_ON;
			else if (!strcasecmp(v->value, "wink"))
				GLOB(mwilamp) = SKINNY_LAMP_WINK;
			else if (!strcasecmp(v->value, "flash"))
				GLOB(mwilamp) = SKINNY_LAMP_FLASH;
			else if (!strcasecmp(v->value, "blink"))
				GLOB(mwilamp) = SKINNY_LAMP_BLINK;
			else
				ast_log(LOG_WARNING, "Invalid mwilamp value at line %d, should be 'off', 'on', 'wink', 'flash' or 'blink'\n", v->lineno);
		} else if (!strcasecmp(v->name, "mwioncall")) {
				GLOB(mwioncall) = sccp_true(v->value);
		} else if (!strcasecmp(v->name, "blindtransferindication")) {
			if (!strcasecmp(v->value, "moh"))
				GLOB(blindtransferindication) = SCCP_BLINDTRANSFER_MOH;
			else if (!strcasecmp(v->value, "ring"))
				GLOB(blindtransferindication) = SCCP_BLINDTRANSFER_RING;
			else
				ast_log(LOG_WARNING, "Invalid blindtransferindication value at line %d, should be 'moh' or 'ring'\n", v->lineno);
#ifdef CS_SCCP_REALTIME
		}  else if (!strcasecmp(v->name, "devicetable")) {	
				sccp_copy_string(GLOB(realtimedevicetable), v->value, sizeof(GLOB(realtimedevicetable)));
		}  else if (!strcasecmp(v->name, "linetable")) {
				sccp_copy_string(GLOB(realtimelinetable) , v->value, sizeof(GLOB(realtimelinetable)) );
#endif
		} else {
			ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
		}
		v = v->next;
	}

	ast_codec_pref_string(&GLOB(global_codecs), pref_buf, sizeof(pref_buf) - 1);
	ast_verbose(VERBOSE_PREFIX_3 "GLOBAL: Preferred capability %s\n", pref_buf);

	if (!ntohs(GLOB(bindaddr.sin_port))) {
		GLOB(bindaddr.sin_port) = ntohs(DEFAULT_SCCP_PORT);
	}
	GLOB(bindaddr.sin_family) = AF_INET;

	v = ast_variable_browse(cfg, "devices");
	if (!v) {
#ifndef CS_SCCP_REALTIME
		ast_log(LOG_WARNING, "Missing [devices] section\n");
#endif		
	}else{
		d = build_devices(v);
	}

	v = ast_variable_browse(cfg, "lines");
	if(v)
		l = build_lines(v);
	
	if (l) {
		free(l);
	}

	/* ok the config parse is done */

	if ((GLOB(descriptor) > -1) && (ntohs(GLOB(bindaddr.sin_port)) != oldport)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

  if (GLOB(descriptor) < 0) {

	GLOB(descriptor) = socket(AF_INET, SOCK_STREAM, 0);

	on = 1;
	if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	if (setsockopt(GLOB(descriptor), IPPROTO_IP, IP_TOS, &GLOB(tos), sizeof(GLOB(tos))) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket TOS to IPTOS_LOWDELAY: %s\n", strerror(errno));
	if (setsockopt(GLOB(descriptor), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));

	if (GLOB(descriptor) < 0) {

		ast_log(LOG_WARNING, "Unable to create SCCP socket: %s\n", strerror(errno));

	} else {
		if (bind(GLOB(descriptor), (struct sockaddr *)&GLOB(bindaddr), sizeof(GLOB(bindaddr))) < 0) {
#ifdef ASTERISK_CONF_1_2
			ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n",	ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
#else
			ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n",	ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)),strerror(errno));
#endif
			close(GLOB(descriptor));
			GLOB(descriptor) = -1;
			return 0;
		}
#ifdef ASTERISK_CONF_1_2
		ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
		ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", 	ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif

		if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
#ifdef ASTERISK_CONF_1_2
			ast_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
#else
			ast_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)),	strerror(errno));
#endif			
			close(GLOB(descriptor));
			GLOB(descriptor) = -1;
			return 0;
		}
#ifdef ASTERISK_CONF_1_2
		sccp_log(0)(VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
		sccp_log(0)(VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif
		ast_pthread_create(&socket_thread, NULL, sccp_socket_thread, NULL);
	}
  }

  ast_config_destroy(cfg);
  sccp_dev_dbclean();
  return 0;
}

static char *sccp_setcalledparty_descrip = "  SetCalledParty(\"Name\" <ext>) sets the name and number of the called party for use with chan_sccp\n";
/**
 * \brief Set the name an number of the called party to the calling phone
 * \param chan asterisk channel
 * \param data callerid in format "Name" <number> 
 */
static int sccp_setcalledparty_exec(struct ast_channel *chan, void *data) {
	char tmp[256] = "";
	char * num, * name;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(chan);

#ifndef CS_AST_HAS_TECH_PVT
	if (strcasecmp(chan->type, "SCCP") != 0)
		return 0;
#else
	if (strcasecmp(chan->tech->type, "SCCP") != 0)
		return 0;
#endif

	if (!data || !c)
		return 0;

	sccp_copy_string(tmp, (char *)data, sizeof(tmp));
	ast_callerid_parse(tmp, &name, &num);
	sccp_channel_set_calledparty(c, name, num);
	
	return 0;
}


static char *sccp_setmessage_descrip = "  SetMessage(\"Message\") sets a display message for use with chan_sccp. Clear the display with empty message\n";
/**
 * \brief It allows you to send a message to the calling device.
 * \author Frank Segtrop <fs@matflow.net> 
 * \param chan 
 * \param data message to sent - if empty clear display
 * \version 20071112_1944
 */
static int sccp_setmessage_exec(struct ast_channel *chan, void *data) {
	char tmp[256] 		= "";
	sccp_channel_t 	* c = NULL;
	sccp_device_t 	* d;

	c = CS_AST_CHANNEL_PVT(chan);
#ifndef CS_AST_HAS_TECH_PVT
 	if (strcasecmp(chan->type, "SCCP") != 0)
 		return 0;
#else
 	if (strcasecmp(chan->tech->type, "SCCP") != 0)
 		return 0;
#endif

 	if (!data || !c ||!c->device)
 		return 0;

 	sccp_copy_string(tmp, (char *)data, sizeof(tmp));

	d = c->device;
	ast_mutex_lock(&d->lock);
	if (strlen(tmp)>0) {
		sccp_dev_displayprinotify(d,tmp,5,0);
		sccp_dev_displayprompt(d,0,0,tmp,0);
		d->phonemessage = strdup(tmp);
		ast_db_put("SCCPM", d->id, tmp);
	}
	else {
		sccp_dev_displayprinotify(d,"Message off",5,1);
		sccp_dev_displayprompt(d,0,0,"Message off",1);
		d->phonemessage = NULL;
		ast_db_del("SCCPM", d->id);
	}	
	ast_mutex_unlock(&d->lock);
	return 0;
}



#ifdef ASTERISK_CONF_1_2
int load_module() {
#else
static int load_module(void) {
#endif
	/* make globals */
	sccp_globals = malloc(sizeof(struct sccp_global_vars));
	if (!sccp_globals) {
		ast_log(LOG_ERROR, "No free mamory for SCCP global vars. SCCP channel type disabled\n");
#ifdef ASTERISK_CONF_1_2		
		return -1;
#else		
		return AST_MODULE_LOAD_FAILURE;
#endif
	}
	memset(sccp_globals,0,sizeof(struct sccp_global_vars));
	ast_mutex_init(&GLOB(lock));
	ast_mutex_init(&GLOB(sessions_lock));
	ast_mutex_init(&GLOB(devices_lock));
	ast_mutex_init(&GLOB(lines_lock));
	ast_mutex_init(&GLOB(channels_lock));
	ast_mutex_init(&GLOB(usecnt_lock));

	/* GLOB() is a macro for sccp_globals-> */
	GLOB(descriptor) = -1;
	GLOB(ourport) = 2000;
	GLOB(externrefresh) = 60;
	GLOB(keepalive)  = SCCP_KEEPALIVE;
	sccp_copy_string(GLOB(date_format), "D/M/YA", sizeof(GLOB(date_format)));
	sccp_copy_string(GLOB(context), "default", sizeof(GLOB(context)));
	sccp_copy_string(GLOB(servername), "Asterisk", sizeof(GLOB(servername)));

	/* Wait up to 16 seconds for first digit */
	GLOB(firstdigittimeout) = 16;
	/* How long to wait for following digits */
	GLOB(digittimeout) = 8;
	/* Yes, these are all that the phone supports (except it's own 'Wideband 256k') */
	GLOB(global_capability) = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A;

	GLOB(debug) = 1;
	GLOB(tos) = (0x68 & 0xff);
	GLOB(rtptos) = (184 & 0xff);
	GLOB(echocancel) = 1;
	GLOB(silencesuppression) = 0;
	GLOB(dndmode) = SCCP_DNDMODE_REJECT;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(private) = 1; /* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = 3;
	GLOB(amaflags) = ast_cdr_amaflags2int("documentation");

	if (!reload_config()) {
#ifdef CS_AST_HAS_TECH_PVT
		if (ast_channel_register(&sccp_tech)) {
#else
		if (ast_channel_register_ex("SCCP", "SCCP", GLOB(global_capability), sccp_request , sccp_devicestate)) {
#endif
			ast_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			return -1;
		}
	}

	sccp_register_cli();
	ast_register_application("SetCalledParty", sccp_setcalledparty_exec, "Sets the name of the called party", sccp_setcalledparty_descrip);
	ast_register_application("SetMessage", sccp_setmessage_exec, "Sets a message on the calling device", sccp_setmessage_descrip);
	return 0;
}
#ifdef ASTERISK_CONF_1_2
int unload_module() {
	char iabuf[INET_ADDRSTRLEN];
#else
static int unload_module(void) {
#endif
	sccp_line_t * l;
	sccp_device_t * d;
	sccp_session_t * s;
	sccp_hint_t *h;
	
#ifdef CS_AST_HAS_TECH_PVT
	ast_channel_unregister(&sccp_tech);
#else
	ast_channel_unregister("SCCP");
#endif
	ast_unregister_application("SetMessage");
	ast_unregister_application("SetCalledParty");
	sccp_unregister_cli();

	ast_mutex_lock(&GLOB(channels_lock));
	while (GLOB(channels))
		sccp_channel_delete_no_lock(GLOB(channels));
	ast_mutex_unlock(&GLOB(channels_lock));

	ast_mutex_lock(&GLOB(lines_lock));
	while (GLOB(lines)) {
		l = GLOB(lines);
		GLOB(lines) = l->next;
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing line %s\n", l->name);
		while (l->hints) {
			h = l->hints;
			l->hints = l->hints->next;
			free(h);
		}
		if (l->cfwd_num)
			free(l->cfwd_num);
		if (l->trnsfvm)
			free(l->trnsfvm);
		free(l);
	}
	ast_mutex_unlock(&GLOB(lines_lock));

	ast_mutex_lock(&GLOB(devices_lock));
	while (GLOB(devices)) {
		d = GLOB(devices);
		GLOB(devices) = d->next;
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		sccp_dev_clean(d);
		ast_mutex_destroy(&d->lock);
		free(d);
	}
	ast_mutex_unlock(&GLOB(devices_lock));

	ast_mutex_lock(&GLOB(sessions_lock));
	while (GLOB(sessions)) {
		s = GLOB(sessions);
		GLOB(sessions) = s->next;
#ifdef ASTERISK_CONF_1_2
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", ast_inet_ntoa(s->sin.sin_addr));
#endif
		
		if (s->fd > -1)
			close(s->fd);
		ast_mutex_destroy(&s->lock);
		free(s);
	}
	ast_mutex_unlock(&GLOB(sessions_lock));
	close(GLOB(descriptor));
	GLOB(descriptor) = -1;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Killing the socket thread\n");
	
	if (!ast_mutex_lock(&GLOB(socket_lock))) {
		if (socket_thread && (socket_thread != AST_PTHREADT_STOP)) {
			/* pthread_cancel(socket_thread); */
			pthread_kill(socket_thread, SIGURG);
			pthread_join(socket_thread, NULL);
		}
		socket_thread = AST_PTHREADT_STOP;
		ast_mutex_unlock(&GLOB(socket_lock));
	} else {
		ast_log(LOG_WARNING, "SCCP: Unable to lock the socket\n");
		return -1;
	}
	
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Killed the socket thread\n");
	
	if (GLOB(ha))
		ast_free_ha(GLOB(ha));

	if (GLOB(localaddr))
		ast_free_ha(GLOB(localaddr));

	ast_mutex_destroy(&GLOB(sessions_lock));
	ast_mutex_destroy(&GLOB(devices_lock));
	ast_mutex_destroy(&GLOB(lines_lock));
	ast_mutex_destroy(&GLOB(channels_lock));
	ast_mutex_destroy(&GLOB(usecnt_lock));
	ast_mutex_destroy(&GLOB(lock));

	free(sccp_globals);

	return 0;
}

#ifndef ASTERISK_CONF_1_2
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION);
#else
int usecount() {
	int res;
	ast_mutex_lock(&GLOB(usecnt_lock));
	res = GLOB(usecnt);
	ast_mutex_unlock(&GLOB(usecnt_lock));
	return res;
}

char *key() {
	return ASTERISK_GPL_KEY;
}

char *description() {
	return ("Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION);
}
#endif
