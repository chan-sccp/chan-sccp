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
#include "sccp_cli.h"
#include "sccp_line.h"
#include "sccp_indicate.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include <asterisk/utils.h>
#include <asterisk/cli.h>
#include <asterisk/astdb.h>
#include <asterisk/pbx.h>

/* ------------------------------------------------------------ */

#ifdef CS_NEW_AST_CLI
static char * sccp_complete_device(const char *line, const char *word, int pos, int state) {
#else
static char * sccp_complete_device(char *line, char *word, int pos, int state) {
#endif
	sccp_device_t * d;
	int which = 0;
	char * ret;

	if (pos > 3)
	return NULL;

	ast_mutex_lock(&GLOB(devices_lock));
	d = GLOB(devices);
	while(d) {
		if (!strncasecmp(word, d->id, strlen(word))) {
			if (++which > state)
				break;
		}
		d = d->next;
	}
	ret = d ? strdup(d->id) : NULL;

	ast_mutex_unlock(&GLOB(devices_lock));
	
	return ret;
}

static int sccp_reset_restart(int fd, int argc, char * argv[]) {
	sccp_moo_t * r;
	sccp_hint_t * h;
	sccp_device_t * d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2]);

	if (!d) {
#ifdef CS_SCCP_REALTIME
		d = sccp_device_find_realtime_byid(argv[2]);
		if(!d)
			ast_cli(fd, "Can't find device %s\n", argv[2]);
#else
		ast_cli(fd, "Can't find device %s\n", argv[2]);
#endif	
	}
	if(!d)
		return RESULT_SUCCESS;
	
	ast_mutex_lock(&d->lock);
	
	if (!d->session) {
		ast_cli(fd, "%s: device not registered\n", argv[2]);
		ast_mutex_unlock(&d->lock);
		return RESULT_SUCCESS;
	}
	
	ast_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);
	h = d->hints;
	while (h) {
		/* force the hint state for non SCCP (or mixed) devices */
		sccp_hint_state(NULL, NULL, AST_EXTENSION_NOT_INUSE, h);
		h = h->next;
	}
	ast_mutex_unlock(&d->lock);

	REQ(r, Reset);
	r->msg.Reset.lel_resetType = htolel((!strcasecmp(argv[1], "reset")) ? SKINNY_DEVICE_RESET : SKINNY_DEVICE_RESTART);
	sccp_dev_send(d, r);
	
	return RESULT_SUCCESS;
}

static int sccp_unregister(int fd, int argc, char * argv[]) {
	sccp_moo_t * r;
	sccp_hint_t * h;
	sccp_device_t * d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2]);

	if (!d) {
#ifdef CS_SCCP_REALTIME
		d = sccp_device_find_realtime_byid(argv[2]);
		if(!d)
			ast_cli(fd, "Can't find device %s\n", argv[2]);
#else
		ast_cli(fd, "Can't find device %s\n", argv[2]);
#endif	
	}
	if(!d)
		return RESULT_SUCCESS;
	
	ast_mutex_lock(&d->lock);
	
	if (!d->session) {
		ast_cli(fd, "%s: device not registered\n", argv[2]);
		ast_mutex_unlock(&d->lock);
		return RESULT_SUCCESS;
	}
	
	ast_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);
	h = d->hints;
	while (h) {
		/* force the hint state for non SCCP (or mixed) devices */
		sccp_hint_state(NULL, NULL, AST_EXTENSION_NOT_INUSE, h);
		h = h->next;
	}
	ast_mutex_unlock(&d->lock);

	REQ(r, RegisterRejectMessage);
	strncpy(r->msg.RegisterRejectMessage.text, "Unregister user request", StationMaxDisplayTextSize);
	sccp_dev_send(d, r);
	
	return RESULT_SUCCESS;
}

/* ------------------------------------------------------------ */

static char *sccp_print_group(char *buf, int buflen, ast_group_t group) {
	unsigned int i;
	int first=1;
	char num[3];
	uint8_t max = (sizeof(ast_group_t) * 8) - 1;

	buf[0] = '\0';
	
	if (!group)
		return(buf);

	for (i=0; i<=max; i++) {
		if (group & ((ast_group_t) 1 << i)) {
	   		if (!first) {
				strncat(buf, ", ", buflen);
			} else {
				first=0;
	  		}
			snprintf(num, sizeof(num), "%u", i);
			strncat(buf, num, buflen);
		}
	}
	return(buf);
}

static int sccp_show_globals(int fd, int argc, char * argv[]) {
	char pref_buf[128];
	char cap_buf[512];
	char buf[256];
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	ast_mutex_lock(&GLOB(lock));
	ast_codec_pref_string(&GLOB(global_codecs), pref_buf, sizeof(pref_buf) - 1);
	ast_getformatname_multiple(cap_buf, sizeof(cap_buf), GLOB(global_capability)),

	ast_cli(fd, "SCCP channel driver global settings\n");
	ast_cli(fd, "------------------------------------\n\n");
#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	ast_cli(fd, "Platform byte order   : LITTLE ENDIAN\n");
#else
	ast_cli(fd, "Platform byte order   : BIG ENDIAN\n");
#endif
	ast_cli(fd, "Protocol Version      : %d\n", GLOB(protocolversion));
	ast_cli(fd, "Server Name           : %s\n", GLOB(servername));
#ifdef ASTERISK_CONF_1_2
	ast_cli(fd, "Bind Address          : %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
	ast_cli(fd, "Bind Address          : %s:%d\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif
	ast_cli(fd, "Keepalive             : %d\n", GLOB(keepalive));
	ast_cli(fd, "Debug level           : %d\n", GLOB(debug));
	ast_cli(fd, "Date format           : %s\n", GLOB(date_format));
	ast_cli(fd, "First digit timeout   : %d\n", GLOB(firstdigittimeout));
	ast_cli(fd, "Digit timeout         : %d\n", GLOB(digittimeout));
	ast_cli(fd, "RTP tos               : %d\n", GLOB(rtptos));
	ast_cli(fd, "Context               : %s\n", GLOB(context));
	ast_cli(fd, "Language              : %s\n", GLOB(language));
	ast_cli(fd, "Accountcode           : %s\n", GLOB(accountcode));
	ast_cli(fd, "Musicclass            : %s\n", GLOB(musicclass));
	ast_cli(fd, "AMA flags             : %d - %s\n", GLOB(amaflags), ast_cdr_flags2str(GLOB(amaflags)));
	ast_cli(fd, "Callgroup             : %s\n", sccp_print_group(buf, sizeof(buf), GLOB(callgroup)));
#ifdef CS_SCCP_PICKUP
	ast_cli(fd, "Pickupgroup           : %s\n", sccp_print_group(buf, sizeof(buf), GLOB(pickupgroup)));
#endif
	ast_cli(fd, "Capabilities          : %s\n", cap_buf);
	ast_cli(fd, "Codecs preference     : %s\n", pref_buf);
	ast_cli(fd, "CFWDALL               : %s\n", (GLOB(cfwdall)) ? "Yes" : "No");
	ast_cli(fd, "CFWBUSY               : %s\n", (GLOB(cfwdbusy)) ? "Yes" : "No");
	ast_cli(fd, "DND                   : %s\n", GLOB(dndmode) ? sccp_dndmode2str(GLOB(dndmode)) : "Disabled");
#ifdef CS_SCCP_PARK
	ast_cli(fd, "Park                  : Enabled\n");
#else
	ast_cli(fd, "Park                  : Disabled\n");
#endif
	ast_cli(fd, "Private softkey       : %s\n", GLOB(private) ? "Enabled" : "Disabled");
	ast_cli(fd, "Echo cancel           : %s\n", GLOB(echocancel) ? "Enabled" : "Disabled");
	ast_cli(fd, "Silence suppression   : %s\n", GLOB(silencesuppression) ? "Enabled" : "Disabled");
	ast_cli(fd, "Trust phone ip        : %s\n", GLOB(trustphoneip) ? "Yes" : "No");
	ast_cli(fd, "Early RTP             : %s\n", GLOB(earlyrtp) ? "Yes" : "No");
	ast_cli(fd, "AutoAnswer ringtime   : %d\n", GLOB(autoanswer_ring_time));
	ast_cli(fd, "AutoAnswer tone       : %d\n", GLOB(autoanswer_tone));
	ast_cli(fd, "RemoteHangup tone     : %d\n", GLOB(remotehangup_tone));
	ast_cli(fd, "Transfer tone         : %d\n", GLOB(transfer_tone));
	ast_cli(fd, "CallWaiting tone      : %d\n", GLOB(callwaiting_tone));
	ast_mutex_unlock(&GLOB(lock));
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_globals = {
  { "sccp", "show", "globals", NULL },
  sccp_show_globals,
  "Show SCCP global settings",
  "Usage: sccp show globals\n"
};

/* ------------------------------------------------------------ */

static int sccp_show_device(int fd, int argc, char * argv[]) {
	sccp_device_t 	* d;
	sccp_speed_t 		* k;
	sccp_serviceURL_t 	* s;
	sccp_line_t 		* l;
	char pref_buf[128];
	char cap_buf[512];
	struct ast_variable *v = NULL;

	if (argc != 4)
		return RESULT_SHOWUSAGE;

	d = sccp_device_find_byid(argv[3]);
	if (!d) {
		ast_cli(fd, "Can't find settings for device %s\n", argv[3]);
		return RESULT_SUCCESS;
	}
	ast_mutex_lock(&d->lock);
	ast_codec_pref_string(&d->codecs, pref_buf, sizeof(pref_buf) - 1);
	ast_getformatname_multiple(cap_buf, sizeof(cap_buf), d->capability),

	ast_cli(fd, "Current settings for selected Device\n");
	ast_cli(fd, "------------------------------------\n\n");
	ast_cli(fd, "MAC-Address        : %s\n", d->id);
	ast_cli(fd, "Protocol Version   : phone=%d, channel=%d\n", d->protocolversion, GLOB(protocolversion));
	ast_cli(fd, "Keepalive          : %d\n", d->keepalive);
	ast_cli(fd, "Registration state : %s(%d)\n", skinny_registrationstate2str(d->registrationState), d->registrationState);
	ast_cli(fd, "State              : %s(%d)\n", skinny_devicestate2str(d->state), d->state);
	ast_cli(fd, "MWI handset light  : %s\n", (d->mwilight) ? "ON" : "OFF");
	ast_cli(fd, "Description        : %s\n", d->description);
	ast_cli(fd, "Config Phone Type  : %s\n", d->config_type);
	ast_cli(fd, "Skinny Phone Type  : %s(%d)\n", skinny_devicetype2str(d->skinny_type), d->skinny_type);
	ast_cli(fd, "Softkey support    : %s\n", (d->softkeysupport) ? "Yes" : "No");
	ast_cli(fd, "Autologin          : %s\n", d->autologin);
	ast_cli(fd, "Image Version      : %s\n", d->imageversion);
	ast_cli(fd, "Timezone Offset    : %d\n", d->tz_offset);
	ast_cli(fd, "Capabilities       : %s\n", cap_buf);
	ast_cli(fd, "Codecs preference  : %s\n", pref_buf);
	ast_cli(fd, "Can DND            : %s\n", (d->dndmode) ? sccp_dndmode2str(d->dndmode) : "Disabled");
	ast_cli(fd, "Can Transfer       : %s\n", (d->transfer) ? "Yes" : "No");
	ast_cli(fd, "Can Park           : %s\n", (d->park) ? "Yes" : "No");
	ast_cli(fd, "Private softkey    : %s\n", d->private ? "Enabled" : "Disabled");
	ast_cli(fd, "Can CFWDALL        : %s\n", (d->cfwdall) ? "Yes" : "No");
	ast_cli(fd, "Can CFWBUSY        : %s\n", (d->cfwdbusy) ? "Yes" : "No");
	ast_cli(fd, "Dtmf mode          : %s\n", (d->dtmfmode) ? "Out-of-Band" : "In-Band");
	ast_cli(fd, "Trust phone ip     : %s\n", (d->trustphoneip) ? "Yes" : "No");
	ast_cli(fd, "Early RTP          : %s\n", (d->earlyrtp) ? "Yes" : "No");

	l = d->lines;
	if (l) {
		ast_cli(fd, "\nLines\n");
		ast_cli(fd, "%-4s: %-20s %-20s\n", "id", "name" , "label");
		ast_cli(fd, "------------------------------------\n");
		while (l) {
			ast_cli(fd, "%4d: %-20s %-20s\n", l->instance, l->name , l->label);
			l = l->next_on_device;
		}
	}
	k = d->speed_dials;
	if (k) {
		ast_cli(fd, "\nSpeedials\n");
		ast_cli(fd, "%-4s: %-20s %-20s\n", "id", "name" , "number");
		ast_cli(fd, "------------------------------------\n");
		while (k) {
			ast_cli(fd, "%4d: %-20s %-20s\n", k->instance, k->name , k->ext);
			k = k->next;
		}
	}
	s = d->serviceURLs;
	if (s) {
		ast_cli(fd, "\nService URLs\n");
		ast_cli(fd, "%-4s: %-20s %-20s\n", "id", "label" , "URL");
		ast_cli(fd, "------------------------------------\n");
		while (s) {
			ast_cli(fd, "%4d: %-20s %-20s\n", s->instance, s->label , s->URL);
			s = s->next;
		}
	}
	
	if(d->variables){
		ast_cli(fd, "\nDevice variables\n");
		ast_cli(fd, "%-4s: %-20s \n", "name" , "value");
		ast_cli(fd, "------------------------------------\n");
		for (v = d->variables ; v ; v = v->next)
			ast_cli(fd, "%-20s : %-20s\n", v->name , v->value);
	}

	ast_mutex_unlock(&d->lock);
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_device = {
  { "sccp", "show", "device", NULL },
  sccp_show_device,
  "Show SCCP Device Information",
  "Usage: sccp show device <deviceId>\n",
  sccp_complete_device
};

/* ------------------------------------------------------------ */


static struct ast_cli_entry cli_reset = {
  { "sccp", "reset", NULL },
  sccp_reset_restart,
  "Reset an SCCP device",
  "Usage: sccp reset <deviceId>\n",
  sccp_complete_device
};

static struct ast_cli_entry cli_restart = {
	{ "sccp", "restart", NULL },
	sccp_reset_restart,
	"Reset an SCCP device",
	"Usage: sccp restart <deviceId>\n",
	sccp_complete_device
};

static struct ast_cli_entry cli_unregister = {
	{ "sccp", "unregister", NULL },
	sccp_unregister,
	"Unregister an SCCP device",
	"Usage: sccp unregister <deviceId>\n",
	sccp_complete_device
};



/* ------------------------------------------------------------ */

static int sccp_show_channels(int fd, int argc, char * argv[]) {
	sccp_channel_t * c;

	ast_cli(fd, "\n%-5s %-10s %-16s %-16s %-16s %-10s\n", "ID","LINE","DEVICE","AST STATE","SCCP STATE","CALLED");
	ast_cli(fd, "===== ========== ================ ================ ================ ========== \n");

	ast_mutex_lock(&GLOB(channels_lock));
	c = GLOB(channels);
	while(c) {
		ast_cli(fd, "%.5d %-10s %-16s %-16s %-16s %-10s\n",
			c->callid,
			c->line->name,
			c->line->device->description,
			(c->owner) ? ast_state2str(c->owner->_state) : "No channel",
			sccp_indicate2str(c->state),
			c->calledPartyNumber);
		c = c->next;
	}
	ast_mutex_unlock(&GLOB(channels_lock));
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_channels = {
	{ "sccp", "show", "channels", NULL },
	sccp_show_channels,
	"Show all SCCP channels",
	"Usage: sccp show channel\n",
};

/* ------------------------------------------------------------ */

static int sccp_show_devices(int fd, int argc, char * argv[]) {
	sccp_device_t * d;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	ast_cli(fd, "\n%-16s %-15s %-16s %-10s\n", "NAME","ADDRESS","MAC","Reg. State");
	ast_cli(fd, "================ =============== ================ ==========\n");

	ast_mutex_lock(&GLOB(devices_lock));
	d = GLOB(devices);
	while (d) {
		
#ifdef ASTERISK_CONF_1_2
		ast_cli(fd, "%-16s %-15s %-16s %-10s\n",// %-10s %-16s %c%c %-10s\n",
			d->description,
			(d->session) ? ast_inet_ntoa(iabuf, sizeof(iabuf), d->session->sin.sin_addr) : "--",
			d->id,
			skinny_registrationstate2str(d->registrationState)
		);
#else
		ast_cli(fd, "%-16s %-15s %-16s %-10s\n",// %-10s %-16s %c%c %-10s\n",
			d->description,
			(d->session) ? ast_inet_ntoa(d->session->sin.sin_addr) : "--",
			d->id,
			skinny_registrationstate2str(d->registrationState)
		);
#endif
		d = d->next;
	}
	ast_mutex_unlock(&GLOB(devices_lock));
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_devices = {
	{ "sccp", "show", "devices", NULL },
	sccp_show_devices,
	"Show all SCCP Devices",
	"Usage: sccp show devices\n"
};

static int sccp_message_devices(int fd, int argc, char * argv[]) {
	sccp_device_t * d;
	int msgtimeout=10;

	if (argc < 4)
		return RESULT_SHOWUSAGE;
		
	if (ast_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;
		
	if (argc == 5 && sscanf(argv[4], "%d", &msgtimeout) != 1) {
		msgtimeout=10;
	}

	ast_mutex_lock(&GLOB(devices_lock));
	d = GLOB(devices);
	while (d) {
		sccp_dev_displaynotify(d,argv[3],msgtimeout);
		d = d->next;
	}
	ast_mutex_unlock(&GLOB(devices_lock));
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_message_devices = {
  { "sccp", "message", "devices", NULL },
  sccp_message_devices,
  "Send a message to all SCCP Devices",
  "Usage: sccp messages devices <message text> <timeout>\n"
};



/* ------------------------------------------------------------ */

static int sccp_show_lines(int fd, int argc, char * argv[]) {
	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	sccp_device_t * d = NULL;
	char cap_buf[512];
	struct ast_variable *v = NULL;

	ast_cli(fd, "\n%-16s %-16s %-4s %-4s %-16s\n", "NAME","DEVICE","MWI","Chs","Active Channel");
	ast_cli(fd, "================ ================ ==== ==== =================================================\n");

	ast_mutex_lock(&GLOB(lines_lock));
	l = GLOB(lines);

	while (l) {
		ast_mutex_lock(&l->lock);
		c = NULL;
		d = l->device;
		if (d) {
			ast_mutex_lock(&d->lock);
			c = d->active_channel;
			ast_mutex_unlock(&d->lock);
		}

		if (!c || (c->line != l))
		c = NULL;
		memset(&cap_buf, 0, sizeof(cap_buf));
		if (c && c->owner)
			ast_getformatname_multiple(cap_buf, sizeof(cap_buf),  c->owner->nativeformats);

		ast_cli(fd, "%-16s %-16s %-4s %-4d %-10s %-10s %-16s %-10s\n",
			l->name,
			(l->device) ? l->device->id : "--",
			(l->mwilight) ? "ON" : "OFF",
			l->channelCount,
			(c) ? sccp_indicate2str(c->state) : "--",
			(c) ? skinny_calltype2str(c->calltype) : "",
			(c) ? ( (c->calltype == SKINNY_CALLTYPE_OUTBOUND) ? c->calledPartyName : c->callingPartyName ) : "",
			cap_buf);

		for (v = l->variables ; v ; v = v->next)
			ast_cli(fd, "Variable: %-20s : %-20s\n", v->name , v->value);

		ast_mutex_unlock(&l->lock);
		l = l->next;
  }

  ast_mutex_unlock(&GLOB(lines_lock));
  return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_lines = {
  { "sccp", "show", "lines", NULL },
  sccp_show_lines,
  "Show All SCCP Lines",
  "Usage: sccp show lines\n"
};



/* ------------------------------------------------------------ */

static int sccp_remove_line_from_device(int fd, int argc, char * argv[]) {
	sccp_line_t * l = NULL;
	sccp_hint_t * h = NULL;
	sccp_device_t * d = NULL;



	d = sccp_device_find_byid(argv[3]);
	if(!d)
		ast_cli(fd, "Device %s not found\n",argv[3]);

	l = d->lines;
	while(l){
		ast_mutex_lock(&l->lock);
		if(!strcasecmp(l->name, argv[4])){
			sccp_line_delete_nolock(l);
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
//			sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);
//			sccp_dev_set_registered(d, SKINNY_DEVICE_RS_OK);
			break;
		}
		
		ast_mutex_unlock(&l->lock);
		l = l->next_on_device;
	}

  return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_remove_line_device = {
  { "sccp", "remove", "line", NULL },
  sccp_remove_line_from_device,
  "Remove a line from device",
  "Usage: sccp remove line <deviceID> <lineID>\n"
};

/* ------------------------------------------------------------ */

static int sccp_show_sessions(int fd, int argc, char * argv[]) {
	sccp_session_t * s = NULL;
	sccp_device_t * d = NULL;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	ast_cli(fd, "%-10s %-15s %-4s %-15s %-15s %-15s\n", "Socket", "IP", "KA", "DEVICE", "STATE", "TYPE");
	ast_cli(fd, "========== =============== ==== =============== =============== ===============\n");

	ast_mutex_lock(&GLOB(sessions_lock));
	s = GLOB(sessions);

	while (s) {
		ast_mutex_lock(&s->lock);
		d = s->device;
		if (d)
			ast_mutex_lock(&d->lock);
#ifdef ASTERISK_CONF_1_2
		ast_cli(fd, "%-10d %-15s %-4d %-15s %-15s %-15s\n",
			s->fd,
			ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr),
			(uint32_t)(time(0) - s->lastKeepAlive),
			(d) ? d->id : "--",
			(d) ? skinny_devicestate2str(d->state) : "--",
			(d) ? skinny_devicetype2str(d->skinny_type) : "--");
#else
		ast_cli(fd, "%-10d %-15s %-4d %-15s %-15s %-15s\n",
			s->fd,
			ast_inet_ntoa(s->sin.sin_addr),
			(uint32_t)(time(0) - s->lastKeepAlive),
			(d) ? d->id : "--",
			(d) ? skinny_devicestate2str(d->state) : "--",
			(d) ? skinny_devicetype2str(d->skinny_type) : "--");
#endif
		if (d)
			ast_mutex_unlock(&d->lock);
		ast_mutex_unlock(&s->lock);
		s = s->next;
	}
	ast_mutex_unlock(&GLOB(sessions_lock));
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_sessions = {
  { "sccp", "show", "sessions", NULL },
  sccp_show_sessions,
  "Show All SCCP Sessions",
  "Usage: sccp show sessions\n"
};

/* ------------------------------------------------------------ */
static int sccp_system_message(int fd, int argc, char * argv[]) {
	int res;
	int timeout = 0;
	if ((argc < 3) || (argc > 5))
		return RESULT_SHOWUSAGE;

	if (argc == 3) {
		res = ast_db_deltree("SCCP", "message");
		if (res) {
			ast_cli(fd, "Failed to delete the SCCP system message!\n");
			return RESULT_FAILURE;
		}
		ast_cli(fd, "SCCP system message deleted!\n");
		return RESULT_SUCCESS;
	}

	if (ast_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	res = ast_db_put("SCCP/message", "text", argv[3]);
	if (res) {
		ast_cli(fd, "Failed to store the SCCP system message text\n");
	} else {
		ast_cli(fd, "SCCP system message text stored successfully\n");
	}
	if (argc == 5) {
		if (sscanf(argv[4], "%d", &timeout) != 1)
			return RESULT_SHOWUSAGE;
		res = ast_db_put("SCCP/message", "timeout", argv[4]);
		if (res) {
			ast_cli(fd, "Failed to store the SCCP system message timeout\n");
		} else {
			ast_cli(fd, "SCCP system message timeout stored successfully\n");
		}
	} else {
		ast_db_del("SCCP/message", "timeout");
	}
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_system_message = {
   { "sccp", "system", "message", NULL },
   sccp_system_message,
   "Set the SCCP system message",
   "Usage: sccp system message \"<message text>\" <timeout>\nThe default optional timeout is 0 (forever)\nExample: sccp system message \"The boss is gone. Let's have some fun!\"  10\n"
};

/* ------------------------------------------------------------ */

static char debug_usage[] =
"Usage: SCCP debug <level>\n"
"		Set the debug level of the sccp protocol from none (0) to high (10)\n";

static char no_debug_usage[] =
"Usage: SCCP no debug\n"
"		Disables dumping of SCCP packets for debugging purposes\n";

static int sccp_do_debug(int fd, int argc, char *argv[]) {
	int new_debug = 10;

	if ((argc < 2) || (argc > 3))
		return RESULT_SHOWUSAGE;

	if (argc == 3) {
		if (sscanf(argv[2], "%d", &new_debug) != 1)
			return RESULT_SHOWUSAGE;
		new_debug = (new_debug > 10) ? 10 : new_debug;
		new_debug = (new_debug < 0) ? 0 : new_debug;
	}

	ast_cli(fd, "SCCP debug level was %d now %d\n", GLOB(debug), new_debug);
	GLOB(debug) = new_debug;
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_do_debug = {
  { "sccp", "debug", NULL },
  sccp_do_debug,
  "Enable SCCP debugging",
  debug_usage };

static int sccp_no_debug(int fd, int argc, char *argv[]) {
	if (argc != 3)
		return RESULT_SHOWUSAGE;

	GLOB(debug) = 0;
	ast_cli(fd, "SCCP Debugging Disabled\n");
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_no_debug = {
  { "sccp", "no", "debug", NULL },
  sccp_no_debug,
  "Disable SCCP debugging",
  no_debug_usage };

static int sccp_do_reload(int fd, int argc, char *argv[]) {
	ast_cli(fd, "SCCP configuration reload not implemented yet! use unload and load.\n");
	return RESULT_SUCCESS;
}

static char reload_usage[] =
"Usage: sccp reload\n"
"		Reloads SCCP configuration from sccp.conf (It will close all active connections)\n";

static struct ast_cli_entry cli_reload = {
  { "sccp", "reload", NULL },
  sccp_do_reload,
  "SCCP module reload",
  reload_usage };

static char version_usage[] =
"Usage: SCCP show version\n"
"		Show the SCCP channel version\n";

static int sccp_show_version(int fd, int argc, char *argv[]) {
	ast_cli(fd, "SCCP channel version: %s\n", SCCP_VERSION);
	return RESULT_SUCCESS;
}

static struct ast_cli_entry cli_show_version = {
  { "sccp", "show", "version", NULL },
  sccp_show_version,
  "SCCP show version",
  version_usage };

void sccp_register_cli(void) {
  ast_cli_register(&cli_show_channels);
  ast_cli_register(&cli_show_devices);
  ast_cli_register(&cli_show_lines);
  ast_cli_register(&cli_show_sessions);
  ast_cli_register(&cli_show_device);
  ast_cli_register(&cli_show_version);
  ast_cli_register(&cli_reload);
  ast_cli_register(&cli_restart);
  ast_cli_register(&cli_reset);
  ast_cli_register(&cli_unregister);
  ast_cli_register(&cli_do_debug);
  ast_cli_register(&cli_no_debug);
  ast_cli_register(&cli_system_message);
  ast_cli_register(&cli_show_globals);
  ast_cli_register(&cli_message_devices);
  ast_cli_register(&cli_remove_line_device);

}

void sccp_unregister_cli(void) {
  ast_cli_unregister(&cli_show_channels);
  ast_cli_unregister(&cli_show_devices);
  ast_cli_unregister(&cli_show_lines);
  ast_cli_unregister(&cli_show_sessions);
  ast_cli_unregister(&cli_show_device);
  ast_cli_unregister(&cli_show_version);
  ast_cli_unregister(&cli_reload);
  ast_cli_unregister(&cli_restart);
  ast_cli_unregister(&cli_reset);
  ast_cli_unregister(&cli_unregister);
  ast_cli_unregister(&cli_do_debug);
  ast_cli_unregister(&cli_no_debug);
  ast_cli_unregister(&cli_system_message);
  ast_cli_unregister(&cli_show_globals);
  ast_cli_unregister(&cli_message_devices);
  ast_cli_unregister(&cli_remove_line_device);
}
