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
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"

#include <asterisk/app.h>
#include <asterisk/pbx.h>
#include <asterisk/astdb.h>
#include <asterisk/utils.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif


void sccp_dev_build_buttontemplate(sccp_device_t *d, btnlist * btn) {
	uint8_t i;
	if (!d || !d->session)
		return;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Building button template %s(%d), user config %s\n",	d->id, skinny_devicetype2str(d->skinny_type), d->skinny_type, d->config_type);
	
	if (!strcasecmp(d->config_type,"7914")) {
		for (i = 0 ; i < 34 ; i++)
			(btn++)->type = SKINNY_BUTTONTYPE_MULTI;
		return;
	}

	switch (d->skinny_type) {
		case SKINNY_DEVICETYPE_30SPPLUS:
		case SKINNY_DEVICETYPE_30VIP:
			for (i = 0 ; i < 4 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			for (i = 0 ; i < 9 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
					/* Column 2 */
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
			for (i = 0 ; i < 9 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			break;
		case SKINNY_DEVICETYPE_12SPPLUS:
		case SKINNY_DEVICETYPE_12SP:
		case SKINNY_DEVICETYPE_12:
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			break;
		case SKINNY_DEVICETYPE_CISCO7902:
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			break;
		case SKINNY_DEVICETYPE_CISCO7912:
		case SKINNY_DEVICETYPE_CISCO7911:
		case SKINNY_DEVICETYPE_CISCO7905:
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			for (i = 0 ; i < 9 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;

			break;
		case SKINNY_DEVICETYPE_CISCO7935:
		case SKINNY_DEVICETYPE_CISCO7936:
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			break;
		case SKINNY_DEVICETYPE_CISCO7910:
				(btn++)->type = SKINNY_BUTTONTYPE_LINE;
				(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
				(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
				(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
				(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
				(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
				(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
				(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			break;
		case SKINNY_DEVICETYPE_CISCO7940:
		case SKINNY_DEVICETYPE_CISCO7941:
		case SKINNY_DEVICETYPE_CISCO7941GE:
				(btn++)->type = SKINNY_BUTTONTYPE_MULTI;
				(btn++)->type = SKINNY_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_CISCO7920:
		case SKINNY_DEVICETYPE_CISCO7960:
		case SKINNY_DEVICETYPE_CISCO7961:
		case SKINNY_DEVICETYPE_CISCO7961GE:
				for (i = 0 ; i < 6 ; i++)
					(btn++)->type = SKINNY_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_CISCO7970:
		case SKINNY_DEVICETYPE_CISCO7971:
		case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
			for (i = 0 ; i < 8 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_ATA186:
			(btn++)->type = SKINNY_BUTTONTYPE_LINE;
			for (i = 0 ; i < 4 ; i++)
				(btn++)->type = SKINNY_BUTTONTYPE_SPEEDDIAL;
			break;
		default:
		/* at least one line */
		(btn++)->type = SKINNY_BUTTONTYPE_LINE;
	}
	return;
}

sccp_moo_t * sccp_build_packet(sccp_message_t t, size_t pkt_len) {
	sccp_moo_t * r = malloc(sizeof(sccp_moo_t));
	if (!r) {
		ast_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
		return NULL;
	}
	memset(r, 0,  pkt_len + 12);
	r->length = htolel(pkt_len + 4);
	r->lel_messageId = htolel(t);
	return r;
}

int sccp_dev_send(sccp_device_t * d, sccp_moo_t * r) {
  return sccp_session_send(d->session, r);
}

void sccp_session_sendmsg(sccp_session_t * s, sccp_message_t t) {
	sccp_moo_t * r = sccp_build_packet(t, 0);
	if (r)
		sccp_session_send(s, r);
}

void sccp_dev_sendmsg(sccp_device_t * d, sccp_message_t t) {
	if (d)
		sccp_session_sendmsg(d->session, t);
}

int sccp_session_send(sccp_session_t * s, sccp_moo_t * r) {
	ssize_t res;
	if (!s || s->fd <= 0) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		free(r);
		return -1;
	}

	res = 1;
	/* sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending Packet Type %s (%d bytes)\n", s->device->id, sccpmsg2str(letohl(r->lel_messageId)), letohl(r->length)); */
	ast_mutex_lock(&s->lock);
	res = write(s->fd, r, (size_t)(letohl(r->length) + 8));
	ast_mutex_unlock(&s->lock);
	if (res != (ssize_t)(letohl(r->length) + 8)) {
/*		ast_log(LOG_WARNING, "SCCP: Only managed to send %d bytes (out of %d): %s\n", res, letohl(r->length) + 8, strerror(errno)); */
		res = 0;
	}

	free(r);
	return res;
}

void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt) {
	char servername[StationMaxDisplayNotifySize];

	if (d->registrationState == opt)
		return;

	d->registrationState = opt;
	if (opt == SKINNY_DEVICE_RS_OK) {
		snprintf(servername, sizeof(servername), "%s %s", GLOB(servername), SKINNY_DISP_CONNECTED);
		sccp_dev_displaynotify(d, servername, 5);
		/* the sccp_dev_check_displayprompt(d); can't stay here, IPC will not reboot correctly */
	}
}
/**
 * sets device d info the SoftKey mode specified by opt
 * 
 * \param d device
 * \param line line
 * \param callid callid
 * \param opt KEYMODE of KEYMODE_*
 * 
 * \todo disable DirTrfr on default
 * 
 */
void sccp_dev_set_keyset(sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;

	if (!d->softkeysupport) 
		return; /* the device does not support softkeys */

	/*let's activate the transfer */
	if (opt == KEYMODE_CONNECTED)
		opt = ( (d->transfer) ? KEYMODE_CONNTRANS : KEYMODE_CONNECTED );

	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance  = htolel(line);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(opt);

	r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF; /* htolel(65535); */

	if ((opt == KEYMODE_ONHOOK || opt == KEYMODE_OFFHOOK || opt == KEYMODE_OFFHOOKFEAT) && (ast_strlen_zero(d->lastNumber)))
		r->msg.SelectSoftKeysMessage.les_validKeyMask &= htolel(~(1<<0));
	

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, skinny_softkeyset2str(opt), opt, line, callid);
	sccp_dev_send(d, r);
}

void sccp_dev_set_mwi(sccp_device_t * d, sccp_line_t * l, uint8_t hasMail) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;

	if (l) {
		if (l->mwilight == hasMail) {
			return;
		}
		l->mwilight = hasMail;
	} else {
		if (d->mwilight == hasMail) {
			return;
		}
		d->mwilight = hasMail;
	}

	REQ(r, SetLampMessage);
	r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
	r->msg.SetLampMessage.lel_stimulusInstance = (l ? htolel(l->instance) : 0);
	/* when l is defined we are switching on/off the button icon */
	r->msg.SetLampMessage.lel_lampMode = htolel( (hasMail) ? ( (l) ? SKINNY_LAMP_ON :  d->mwilamp) : SKINNY_LAMP_OFF);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line %d\n",	d->id, hasMail ? "ON" : "OFF", (l ? l->instance : 0));
}

void sccp_dev_set_ringer(sccp_device_t * d, uint8_t opt, uint32_t line, uint32_t callid) {
	sccp_moo_t * r;
	if (!d->session)
		return;
  // If we have multiple calls ringing at once, this has lead to
  // never ending rings even after termination of the alerting call.
  // Obviously the ringermode is no longer a per-device state but
  // rather per line/per call.
	//if (d->ringermode == opt)
	//	return;

	d->ringermode = opt;
	REQ(r, SetRingerMessage);
	r->msg.SetRingerMessage.lel_ringMode = htolel(opt);
	/* Note that for distinctive ringing to work with the higher protocol versions
	   the following actually needs to be set to 1 as the original comment says.
	   Curiously, the variable is not set to 1 ... */
	//r->msg.SetRingerMessage.lel_unknown1 = htolel(opt); /* always 1 */
	r->msg.SetRingerMessage.lel_unknown1 = htolel(1);/* always 1 */
	r->msg.SetRingerMessage.lel_lineInstance = htolel(line);
	r->msg.SetRingerMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send ringer mode %s(%d) on device\n", d->id, skinny_ringermode2str(opt), opt);

}

void sccp_dev_set_speaker(sccp_device_t * d, uint8_t mode) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, SetSpeakerModeMessage);
	r->msg.SetSpeakerModeMessage.lel_speakerMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send speaker mode %d\n", d->id, mode);
}

void sccp_dev_set_microphone(sccp_device_t * d, uint8_t mode) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, SetMicroModeMessage);
	r->msg.SetMicroModeMessage.lel_micMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send microphone mode %d\n", d->id, mode);
}

void sccp_dev_set_cplane(sccp_line_t * l, int status) {
	sccp_moo_t * r;
	sccp_device_t * d;
	if (!l)
		return;
	d = l->device;
	if (!d)
		return;

	REQ(r, ActivateCallPlaneMessage);
	if (status)
		r->msg.ActivateCallPlaneMessage.lel_lineInstance = htolel(l->instance);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send activate call plane on line %d\n", d->id, (status) ? l->instance : 0 );
}

void sccp_dev_set_deactivate_cplane(sccp_channel_t * c) {
	if (!c || !c->device || !c->line)
		return;

	sccp_dev_sendmsg(c->device, DeactivateCallPlaneMessage);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send deactivate call plane on line %d\n", c->device->id, c->line->instance);
}

void sccp_dev_starttone(sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, StartToneMessage);
	r->msg.StartToneMessage.lel_tone = htolel(tone);
	r->msg.StartToneMessage.lel_toneTimeout = htolel(timeout);
	r->msg.StartToneMessage.lel_lineInstance = htolel(line);
	r->msg.StartToneMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending tone %s (%d)\n", d->id, skinny_tone2str(tone), tone);
}

void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid) {
	sccp_moo_t * r;
	if (!d || !d->session)
		return;
	REQ(r, StopToneMessage);
	r->msg.StopToneMessage.lel_lineInstance = htolel(line);
	r->msg.StopToneMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Stop tone on device\n", d->id);
}

void sccp_dev_clearprompt(sccp_device_t * d, uint8_t line, uint32_t callid) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	REQ(r, ClearPromptStatusMessage);
	r->msg.ClearPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.ClearPromptStatusMessage.lel_lineInstance  = htolel(line);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Clear the status prompt on line %d and callid %d\n", d->id, line, callid);
}



void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char * msg, int timeout) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;
	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayPromptStatusMessage);
	r->msg.DisplayPromptStatusMessage.lel_messageTimeout = htolel(timeout);
	r->msg.DisplayPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.DisplayPromptStatusMessage.lel_lineInstance = htolel(line);
	sccp_copy_string(r->msg.DisplayPromptStatusMessage.promptMessage, msg, sizeof(r->msg.DisplayPromptStatusMessage.promptMessage));
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", d->id, line, callid, timeout);
}

void sccp_dev_cleardisplay(sccp_device_t * d) {
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearDisplay);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Clear the display\n", d->id);
}

void sccp_dev_display(sccp_device_t * d, char * msg) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayTextMessage);
	sccp_copy_string(r->msg.DisplayTextMessage.displayMessage, msg, sizeof(r->msg.DisplayTextMessage.displayMessage));

	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Display text\n", d->id);
}

void sccp_dev_cleardisplaynotify(sccp_device_t * d) {
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearNotifyMessage);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Clear the display notify message\n", d->id);
}

void sccp_dev_displaynotify(sccp_device_t * d, char * msg, uint32_t timeout) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;
	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayNotifyMessage);
	r->msg.DisplayNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Display notify with timeout %d\n", d->id, timeout);
}

void sccp_dev_cleardisplayprinotify(sccp_device_t * d) {
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearPriNotifyMessage);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Clear the display priority notify message\n", d->id);
}

void sccp_dev_displayprinotify(sccp_device_t * d, char * msg, uint32_t priority, uint32_t timeout) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayPriNotifyMessage);
	r->msg.DisplayPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayPriNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayPriNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Display notify with timeout %d and priority %d\n", d->id, timeout, priority);
}

sccp_speed_t * sccp_dev_speed_find_byindex(sccp_device_t * d, uint8_t instance, uint8_t type) {
	/* type could be SKINNY_BUTTONTYPE_SPEEDDIAL or SKINNY_BUTTONTYPE_LINE. See the button template functions */
	sccp_speed_t * k = d->speed_dials;
	if (!d || !d->session)
		return NULL;


	sccp_log(10)(VERBOSE_PREFIX_3 "%s: searching for index %d and type %d\n", d->id, instance, type);
	while (k) {
		if (k->type == type && k->instance == instance) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: found name: %s, ext: %s, index: %d, type: %d\n", d->id, k->name, k->ext, k->instance, k->type);
			break;
		}
/*		sccp_log(10)(VERBOSE_PREFIX_3 "%s: skipped name: %s, ext: %s, index: %d, type: %d\n", d->id, k->name, k->ext, k->instance, k->type); */
		k = k->next;
	}
	return k;
}

sccp_line_t * sccp_dev_get_activeline(sccp_device_t * d) {
	/* XXX fixme */
	/* What's up if the currentLine is NULL? let's do the check */
	if (!d || !d->session)
		return NULL;

	if (!d->currentLine) {
		d->currentLine = d->lines;
		if (d->currentLine) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Forcing the active line to %s from NULL\n", d->id, d->currentLine->name);
		} else {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: No lines\n", d->id);
		}
	} else {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: The active line is %s\n", d->id, d->currentLine->name);
	}
	return d->currentLine;
}

void sccp_dev_set_activeline(sccp_line_t * l) {
	sccp_device_t * d = ( (l) ? l->device : NULL);
	if (!l || !d || !d->session)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send the active line %s\n", d->id, l->name);
	ast_mutex_lock(&d->lock);
	d->currentLine = l;
	ast_mutex_unlock(&d->lock);
	return;
}

void sccp_dev_check_mwi(sccp_device_t * d) {
	sccp_line_t * l;
	uint8_t linehasmsgs = 0;
	uint8_t devicehasmsgs = 0;
	uint8_t devicehaschannels = 0;

	if (!d || !d->session)
		return;

	ast_mutex_lock(&d->lock);
	l = d->lines;
	while (l) {
		linehasmsgs = 0;
		ast_mutex_lock(&l->lock);
		if (l->channels && !d->mwioncall)
			devicehaschannels = 1;
		else if (!ast_strlen_zero(l->mailbox)) {
#ifdef CS_AST_HAS_NEW_VOICEMAIL
			if (ast_app_has_voicemail(l->mailbox, NULL)) {
#else
			if (ast_app_has_voicemail(l->mailbox)) {
#endif
				linehasmsgs = 1;
			}
			if (linehasmsgs != l->mwilight) {
				if (linehasmsgs)
					sccp_log(2)(VERBOSE_PREFIX_3 "%s: New messages in the mailbox %s on line %s\n", d->id, l->mailbox, l->name);
				sccp_dev_set_mwi(d, l, linehasmsgs);
			}
		}
		if (linehasmsgs)
			devicehasmsgs = 1;
		ast_mutex_unlock(&l->lock);
		l = l->next_on_device;
	}

	if (!devicehaschannels) {
		if (devicehasmsgs != d->mwilight)
			sccp_dev_set_mwi(d, NULL, devicehasmsgs);
	}
	ast_mutex_unlock(&d->lock);
}

/*
uint8_t sccp_dev_check_idle(sccp_device_t * d) {
	sccp_line_t * l = NULL;
	uint8_t res = 0;
	if (!d || !d->session)
		return 0;
	ast_mutex_lock(&d->lock);
	if (d->state == SCCP_DEVICESTATE_OFFHOOK)
		goto OUT;
	if (d->active_channel)
		goto OUT;
	l = d->lines;
	if (!l)
		goto OUT;
	* let's check for channels on lines *
	res = 1;
	while (res && l) {
		if (l->channels)
			res = 0;
		l = l->next_on_device;
	}
OUT:
	ast_mutex_unlock(&d->lock);
	return res;
}
*/

void sccp_dev_check_displayprompt(sccp_device_t * d) {
	sccp_line_t * l;
	char tmp[256] = "";
	int timeout = 0;
	uint8_t res = 0;

	if (!d || !d->session)
		return;

	ast_mutex_lock(&d->lock);

	sccp_dev_clearprompt(d, 0, 0);
	sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOUR_CURRENT_OPTIONS, 0);

	if (d->phonemessage) 										// display message if set
		sccp_dev_displayprompt(d,0,0,d->phonemessage,0);

	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK); 				/* this is for redial softkey */

	/* check for forward to display */
	res = 0;
	l = d->lines;
	while (l) {
		if (l->cfwd_type != SCCP_CFWD_NONE && l->cfwd_num) {
			res = 1;
			tmp[0] = '\0';
			if (l->cfwd_type == SCCP_CFWD_ALL) {
				strcat(tmp, SKINNY_DISP_CFWDALL ":");
				sccp_dev_set_lamp(d, SKINNY_STIMULUS_FORWARDALL, l->instance, SKINNY_LAMP_ON);
			} else if (l->cfwd_type == SCCP_CFWD_BUSY) {
				strcat(tmp, SKINNY_DISP_CFWDBUSY ":");
				sccp_dev_set_lamp(d, SKINNY_STIMULUS_FORWARDBUSY, l->instance, SKINNY_LAMP_ON);
			}
			strcat(tmp, SKINNY_DISP_FORWARDED_TO " ");
			strcat(tmp, l->cfwd_num);
			sccp_dev_displayprompt(d, 0, 0, tmp, 0);
			sccp_dev_set_keyset(d, l->instance, 0, KEYMODE_ONHOOK); /* this is for redial softkey */
		}
		l = l->next_on_device;
	}
	if (res)
		goto OUT;

	if (d->dnd && d->dndmode ) {
		if (d->dndmode == SCCP_DNDMODE_REJECT || d->dnd == SCCP_DNDMODE_REJECT)
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_BUSY ") <<<", 0);

		else if (d->dndmode == SCCP_DNDMODE_SILENT || d->dnd == SCCP_DNDMODE_SILENT)
			/* no internal label for the silent string */
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (Silent) <<<", 0);

		else
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " <<<", 0);
		goto OUT;
	}

	if (!ast_db_get("SCCP/message", "timeout", tmp, sizeof(tmp))) {
		sscanf(tmp, "%i", &timeout);
	}
	if (!ast_db_get("SCCP/message", "text", tmp, sizeof(tmp))) {
		if (!ast_strlen_zero(tmp)) {
			/* sccp_dev_displayprompt(d, 0, 0, tmp, timeout); */
			sccp_dev_displayprinotify(d, tmp, 5, timeout);
			goto OUT;
		}
	}

	if (d->mwilight) {
		/* sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOU_HAVE_VOICEMAIL, 10); */
		sccp_dev_displayprinotify(d, SKINNY_DISP_YOU_HAVE_VOICEMAIL, 5, 10);
		goto OUT;
	}
	/* when we are here, there's nothing to display */
OUT:
	ast_mutex_unlock(&d->lock);
}

void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * wanted) {
	/* XXX rework this */
	sccp_line_t * current;
	sccp_channel_t * chan = NULL;

	if (!d || !d->session)
		return;

	current = sccp_dev_get_activeline(d);

	if (!wanted || !current || wanted->device != d || current == wanted)
		return;

	// If the current line isn't in a call, and
	// neither is the target.
	if (current->channels == NULL && wanted->channels == NULL) {

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: All lines seem to be inactive, SEIZEing selected line %s\n", d->id, wanted->name);
		sccp_dev_set_activeline(wanted);

		chan = sccp_channel_allocate(wanted);
		if (!chan) {
			ast_log(LOG_ERROR, "%s: Failed to allocate SCCP channel.\n", d->id);
			return;
		}

/*	} else if ( current->dnState > TsOnHook || wanted->dnState == TsOffHook) { */
	} else if ( d->state == SCCP_DEVICESTATE_OFFHOOK) {
	  // If the device is currently onhook, then we need to ...

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Selecing line %s while using line %s\n", d->id, wanted->name, current->name);
	// XXX (1) Put current call on hold
	// (2) Stop transmitting/recievening

	} else {

	// Otherwise, just select the callplane
	ast_log(LOG_WARNING, "%s: Unknown status while trying to select line %s.  Current line is %s\n", d->id, wanted->name, current->name);
	}
}

void sccp_dev_set_lamp(sccp_device_t * d, uint16_t stimulus, uint8_t instance, uint8_t lampMode) {
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	REQ(r, SetLampMessage);
	r->msg.SetLampMessage.lel_stimulus = htolel(stimulus);
	r->msg.SetLampMessage.lel_stimulusInstance = htolel(instance);
	r->msg.SetLampMessage.lel_lampMode = htolel(lampMode);
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send lamp mode %s(%d) on line %d\n", d->id, skinny_lampmode2str(lampMode), lampMode, instance );
}

void sccp_dev_forward_status(sccp_line_t * l) {
	sccp_device_t * d = l->device;
	sccp_moo_t * r1 = NULL;

	if (!d || !d->session)
		return;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send Forward Status.  Line: %s\n", d->id, l->name);

	REQ(r1, ForwardStatMessage);
	r1->msg.ForwardStatMessage.lel_status = (l->cfwd_type)? htolel(1) : 0;
	r1->msg.ForwardStatMessage.lel_lineNumber = htolel(l->instance);
	switch (l->cfwd_type) {
		case SCCP_CFWD_ALL:
			r1->msg.ForwardStatMessage.lel_cfwdallstatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdallnumber,l->cfwd_num, sizeof(r1->msg.ForwardStatMessage.cfwdallnumber));
			break;
		case SCCP_CFWD_BUSY:
			r1->msg.ForwardStatMessage.lel_cfwdbusystatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdbusynumber, l->cfwd_num, sizeof(r1->msg.ForwardStatMessage.cfwdbusynumber));
	}
	sccp_dev_send(d, r1);
	if (l->cfwd_type == SCCP_CFWD_ALL)
		sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_FWDALL, NULL);
	else
		sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_ONHOOK, NULL);
}

int sccp_device_check_ringback(sccp_device_t * d) {
	sccp_channel_t * c;

	ast_mutex_lock(&d->lock);
	d->needcheckringback = 0;
	if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		ast_mutex_unlock(&d->lock);
		return 0;
	}
	ast_mutex_unlock(&d->lock);
	c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLTRANSFER);
	if (!c)
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_RINGIN);
	if (!c)
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLWAITING);

	if (c) {
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_RINGIN);
		return 1;
	}
	return 0;
}

/* this thread will start 5 seconds after the device registration to display the status of the monitored extensions */
void * sccp_dev_postregistration(void *data) {
	sccp_device_t * d = data;
	sccp_hint_t * h = NULL;
	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	int state = AST_EXTENSION_NOT_INUSE;

	if (!d || !d->session)
		return NULL;

	sleep(5);
	pthread_testcancel();
	ast_mutex_lock(&d->lock);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Post registration process\n", d->id);

	/* turn off the device MWI light. We need to force it off on some phone (7910 for example) */
	sccp_dev_set_mwi(d, NULL, 0);

	/* poll the state of the asterisk hints extensions and notify the state to the phone */
	h = d->hints;
	while (h) {
		/* force the hint state for non SCCP (or mixed) devices */
		state = ast_extension_state(NULL, h->context, h->exten);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: %s@%s is on state %d\n", d->id, h->exten, h->context, state);
		if (state != AST_EXTENSION_NOT_INUSE)
			sccp_hint_state(NULL, NULL, state, h);
		h = h->next;
	}

	/* poll the state of the SCCP monitored lines */
	
	ast_mutex_lock(&GLOB(lines_lock));
	l = GLOB(lines);
	while(l) {
		/* turn off the device MWI light */
		sccp_dev_set_mwi(d, l, 0);
		h = l->hints;
		while (h) {
			if (h->device == d) {
				if (!l->device)
					sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_UNAVAILABLE, d);
				else if (l->device->dnd && l->device->dndmode == SCCP_DNDMODE_REJECT)
					sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_DND, d);
				else if (l->cfwd_type == SCCP_CFWD_ALL)
					sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_FWDALL, d);
				else {
					c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED);
					if (!c)
						c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGIN);
					if (!c)
						c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CALLWAITING);
					if (c)
						sccp_hint_notify(c, d);
				}
				break;
			}
			h = h->next;
		}
		l = l->next;
	}
	ast_mutex_unlock(&GLOB(lines_lock));
	sccp_dev_check_mwi(d);
	sccp_dev_check_displayprompt(d);
	d->postregistration_thread = AST_PTHREADT_STOP;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Post registration process... done!\n", d->id);
	ast_mutex_unlock(&d->lock);
	return NULL;
}

/* clean up memory allocated by the device */
void sccp_dev_clean(sccp_device_t * d) {
	sccp_speed_t * k, * k1 = NULL;
	sccp_hint_t * h;
	sccp_hostname_t *permithost;

	if (!d || !d->session)
		return;
	ast_mutex_lock(&d->lock);

	k = d->speed_dials;
	while (k) {
		k1 = k->next;
		free(k);
		k = k1;
	}
	while (d->hints) {
		h = d->hints;
		if (h->hintid > -1)
			ast_extension_state_del(h->hintid, NULL);
		d->hints = d->hints->next;
		free(h);
	}
	while (d->permithost) {
		permithost = d->permithost;
		d->permithost = d->permithost->next;
		free(permithost);
	}
	if (d->ha)
		ast_free_ha(d->ha);
		
	if (d->postregistration_thread && (d->postregistration_thread != AST_PTHREADT_STOP)) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Killing the device postregistration thread\n", d->id);
		pthread_cancel(d->postregistration_thread);
		pthread_kill(d->postregistration_thread, SIGURG);
		pthread_join(d->postregistration_thread, NULL);
		d->postregistration_thread = AST_PTHREADT_STOP;
	}
	ast_mutex_unlock(&d->lock);
}

sccp_serviceURL_t * sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint8_t instance) {
	sccp_serviceURL_t * s = d->serviceURLs;
	if (!d || !d->session)
		return NULL;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: searching for serviceURL index %d\n", d->id, instance);
	if(!s)
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: no serviceURLs defined for device\n", d->id);

	while (s) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: ServiceURL-instance: %d\n", d->id, s->instance);
		if (s->instance == instance) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: found serviceURL label: %s, URL: %s, index: %d\n", d->id, s->label, s->URL, s->instance);
			break;
		}
		s = s->next;
	}
	return s;
}
