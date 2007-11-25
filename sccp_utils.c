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
#include "sccp_indicate.h"
#include "sccp_device.h"
#include <asterisk/astdb.h>
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif


void sccp_device_add_line(sccp_device_t * d, char * name) {
	uint8_t i = 0, line_count = 0;
	sccp_line_t *l, *lines_last = NULL;
	btnlist btn[StationMaxButtonTemplateSize];

	if(!d)
		return;

	memset(&btn, 0 , sizeof(btn));
	sccp_dev_build_buttontemplate(d, btn);
	
	line_count = 0;
	/* count the available lines on the phone */
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if ( (btn[i].type == SKINNY_BUTTONTYPE_LINE) || (btn[i].type == SKINNY_BUTTONTYPE_MULTI) )
			line_count++;
		else if (btn[i].type == SKINNY_BUTTONTYPE_UNUSED)
			break;
	}
	i=0;
	l=d->lines;
	lines_last=d->lines;
	while(l){
		i++;
		lines_last=l;
		l = l->next;		
	}

	ast_strip(name);
		if (ast_strlen_zero(name)) {
			/* this is useful to leave and empty button */
			ast_mutex_lock(&lines_last->lock);
			lines_last->next = build_line();
			ast_mutex_unlock(&lines_last->lock);
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Add an empty line\n", d->id);
			
		}

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Auto logging into %s\n", d->id, name);
		l = sccp_line_find_byname(name);
		if (!l) {
			ast_log(LOG_ERROR, "%s: Failed to autolog into %s: Couldn't find line %s\n", d->id, name, name);
			return;
		}

		if (l->device) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Line %s aready attached to device %s\n", d->id, l->name, l->device->id);
			return;
		}

		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Attaching line %s to this device\n", d->id, l->name);
		if (i == line_count) {
			ast_log(LOG_WARNING, "%s: Failed to autolog into %s: Max available lines phone limit reached %s\n", d->id, name, name);
			return;
		}

		i++;
		ast_mutex_lock(&l->lock);
		ast_mutex_lock(&d->lock);
		if (i == 1)
			d->currentLine = l;
		l->device = d;
		l->instance = i;
		l->mwilight = 0;
		/* I want it in the right order */
		if (!d->lines)
			d->lines = l;
		if (!lines_last)
			lines_last = l;
		else {
			l->prev_on_device = lines_last;
			lines_last->next_on_device = l;
			lines_last = l;
		}
		ast_mutex_unlock(&l->lock);
		ast_mutex_unlock(&d->lock);
		/* notify the line is on */
		sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_ONHOOK, NULL);
}

struct ast_variable * sccp_create_variable(const char *buf) {
	struct ast_variable *tmpvar = NULL;
	char *varname = ast_strdupa(buf), *varval = NULL;
	
	if ((varval = strchr(varname,'='))) {
		*varval++ = '\0';
		if ((tmpvar = ast_variable_new(varname, varval))) {
			return tmpvar;
		}
	}
	return NULL;
}

sccp_device_t * sccp_device_find_byid(const char * name) {
  	sccp_device_t * d;
	/*
  	if (!(d = GLOB(devices) ))
  		return NULL;
	*/
   	d = GLOB(devices);

  	ast_mutex_lock(&GLOB(devices_lock));
  	while (d) {
    		if (!strcasecmp(d->id, name))
      			break;
    		d = d->next;
  	}
  	ast_mutex_unlock(&GLOB(devices_lock));

#ifdef CS_SCCP_REALTIME
  	if (!d)
		d = sccp_device_find_realtime_byid(name);
#endif
  	return d;
}

sccp_device_t * sccp_device_find_byname(const char * name) {
  	sccp_device_t * d;
	/*
	if (!(d = GLOB(devices) ))
		return NULL;
	*/
  	d = GLOB(devices);
  
  	ast_mutex_lock(&GLOB(devices_lock));
  	while (d) {
    		if (!strcasecmp(d->id, name))
    			break;
    		d = d->next;
  	}
  	ast_mutex_unlock(&GLOB(devices_lock));
 
#ifdef CS_SCCP_REALTIME
	if (!d)
 		d = sccp_device_find_realtime_byname(name);
#endif
 
 return d;
}

#ifdef CS_SCCP_REALTIME
sccp_device_t * sccp_device_find_realtime_byid(const char * name) {
 	return sccp_device_find_realtime(name);
}

sccp_device_t * sccp_device_find_realtime_byname(const char * name) {
	return sccp_device_find_realtime(name);
}
#endif

#ifdef CS_SCCP_REALTIME

sccp_device_t * sccp_device_find_realtime(const char * name) {
 sccp_device_t *d=NULL;
 struct ast_variable *v;

 sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for device %s in realtime. look at table: %s\n", name, GLOB(realtimedevicetable));
 v = ast_load_realtime(GLOB(realtimedevicetable), "name", name, NULL);

 if (!v){
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Device %s not found\n", name);
 	return NULL;
 }

 d = build_devices(v);
 if(d)
	d->realtime=1;
 sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Device %s realtime is: %d\n", d->id, d->realtime);

 ast_variables_destroy(v);

 if (!d)
 return NULL;

  return d;
}
#endif


sccp_line_t * sccp_line_find_byname(const char * name) {
	sccp_line_t * l;
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for line %s\n", name);

	if(!name)
		return NULL;

#ifndef CS_SCCP_REALTIME
	if (!(l = GLOB(lines) ))
		return NULL;
#endif
	l = GLOB(lines);

	ast_mutex_lock(&GLOB(lines_lock));
	while(l && strcasecmp(l->name, name) != 0) {
		l = l->next;
	}
	if (l && l->device)
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: found line %s\n", DEV_ID_LOG(l->device), l->name);
	ast_mutex_unlock(&GLOB(lines_lock));

#ifdef CS_SCCP_REALTIME
	if (!l)
		l = sccp_line_find_realtime_byname(name);
#endif
	if(l){
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: found line %s\n", DEV_ID_LOG(l->device), l->name);
	}else{
		sccp_log(10)(VERBOSE_PREFIX_3 "no line");	
		return NULL;
	}
	return l;
}


#ifdef CS_SCCP_REALTIME
sccp_line_t * sccp_line_find_realtime_byname(const char * name) {
 sccp_line_t *l=NULL;
 struct ast_variable *v;


 sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for line %s in realtime. Look at table: %s\n", name, GLOB(realtimelinetable));

 v = ast_load_realtime(GLOB(realtimelinetable), "name", name, NULL);

 if (!v){
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line %s not found\n", name);
 	return NULL;
 }else{
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line %s found\n", name);
 }

 l = build_lines(v);
 if(l)
	l->realtime=1;
 sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line %s realtime is: %d\n", l->name, l->realtime);

 ast_variables_destroy(v);

 if (!l)
 	return NULL;

 return l;

}
#endif


sccp_line_t * sccp_line_find_byid(sccp_device_t * d, uint8_t instance) {
	sccp_line_t * l;
	if (!d)
		return NULL;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for line by instance %d\n", d->id, instance);
	ast_mutex_lock(&GLOB(lines_lock));
	ast_mutex_lock(&d->lock);
	l = d->lines;
	while (l) {
		if (l->instance == instance)
			break;
		l = l->next_on_device;
	}
	ast_mutex_unlock(&d->lock);
	ast_mutex_unlock(&GLOB(lines_lock));
	if (l)
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found line %s\n", d->id, l->name);
	return l;
}

sccp_channel_t * sccp_channel_find_byid(uint32_t id) {
	sccp_channel_t * c;
/*	uint8_t tries = SCCP_LOCK_TRIES; */
	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %d\n", id);

	ast_mutex_lock(&GLOB(channels_lock));
	/* with this lock the channels list will not change */
	c = GLOB(channels);
	while (c) {
		if (c->callid == id) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
/*
			while (tries && ast_mutex_trylock(&c->lock)) {
				tries--;
				usleep(SCCP_LOCK_USLEEP);
			}
			if (!tries) {
				ast_log(LOG_ERROR, "%s: Failed to lock channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				c = NULL;
			}
*/
			break;
		}
		c = c->next;
	}
	ast_mutex_unlock(&GLOB(channels_lock));
	return c;
}

sccp_channel_t * sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state) {
	sccp_channel_t * c;
	if (!l)
		return NULL;
//	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for a channel with state \"%s\" (%d) on line %s\n", DEV_ID_LOG(l->device), sccp_indicate2str(state), state, l->name);

	ast_mutex_lock(&GLOB(channels_lock));
	ast_mutex_lock(&l->lock);
	c = l->channels;
	while (c) {
		if (c->state == state)
			break;
		c = c->next_on_line;
	}
	ast_mutex_unlock(&l->lock);
	ast_mutex_unlock(&GLOB(channels_lock));
	if (c)
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d) with state \"%s\" (%d) on line %s\n", DEV_ID_LOG(l->device), c->callid, sccp_indicate2str(state), state, l->name);
	return c;
}

sccp_channel_t * sccp_channel_find_bycallstate_on_line(sccp_line_t * l, uint8_t state) {
	sccp_channel_t * c;
	if (!l)
		return NULL;
//	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for a channel with state \"%s\" (%d) on line %s\n", DEV_ID_LOG(l->device), sccp_callstate2str(state), state, l->name);

	ast_mutex_lock(&GLOB(channels_lock));
	ast_mutex_lock(&l->lock);
	c = l->channels;
	while (c) {
		if (c->callstate == state)
			break;
		c = c->next_on_line;
	}
	ast_mutex_unlock(&l->lock);
	ast_mutex_unlock(&GLOB(channels_lock));
	if (c)
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d) with state \"%s\" (%d) on line %s\n", DEV_ID_LOG(l->device), c->callid, sccp_callstate2str(state), state, l->name);
	return c;
}

sccp_channel_t * sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state) {
	sccp_channel_t * c = NULL;
	sccp_line_t * l;
	if (!d)
		return NULL;
//	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for a channel with state \"%s\" (%d) on device\n", d->id, sccp_indicate2str(state), state);
	
	ast_mutex_lock(&GLOB(channels_lock));
	ast_mutex_lock(&d->lock);
	l = d->lines;
	while (l) {
		c = sccp_channel_find_bystate_on_line(l, state);
		if (c)
			break;
		l = l->next_on_device;
	}
	ast_mutex_unlock(&d->lock);
	ast_mutex_unlock(&GLOB(channels_lock));
	if (c)
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d) with state \"%s\" (%d) on device\n", d->id, c->callid, sccp_indicate2str(state), state);
	return c;
}

void sccp_ast_setstate(sccp_channel_t * c, int state) {
	if (c && c->owner) {
		ast_setstate(c->owner, state);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set asterisk state %s (%d) for call %d\n", DEV_ID_LOG(c->device), ast_state2str(state), state, c->callid);
	}
}

void sccp_dev_dbput(sccp_device_t * d) {
	char tmp[1024] = "", cfwdall[1024] = "", cfwdbusy[1024] = "";
	sccp_line_t * l;
	if (!d)
		return;
	ast_mutex_lock(&d->lock);
	l = d->lines;
	while (l) {
		if (l->cfwd_type == SCCP_CFWD_ALL) {
			snprintf(tmp, sizeof(tmp), "%d:%s;", l->instance, l->cfwd_num);
			strncat(cfwdall, tmp, sizeof(cfwdall) - strlen(cfwdall));
		} else if (l->cfwd_type == SCCP_CFWD_BUSY) {
			snprintf(tmp, sizeof(tmp), "%d:%s;", l->instance, l->cfwd_num);
			strncat(cfwdbusy, tmp, sizeof(cfwdbusy) - strlen(cfwdbusy));
		}
		l = l->next_on_device;
	}

	if (!ast_strlen_zero(cfwdall))
		cfwdall[strlen(cfwdall)-1] = '\0';
	if (!ast_strlen_zero(cfwdbusy))
		cfwdbusy[strlen(cfwdbusy)-1] = '\0';

	snprintf(tmp, sizeof(tmp), "dnd=%d,cfwdall=%s,cfwdbusy=%s", d->dnd, cfwdall, cfwdbusy);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Storing device status (dnd, cfwd*) in the asterisk db\n", d->id);
	if (ast_db_put("SCCP", d->id, tmp))
		ast_log(LOG_NOTICE, "%s: Unable to store device status (dnd, cfwd*) in the asterisk db\n", d->id);
	ast_mutex_unlock(&d->lock);
}

static void sccp_cfwd_parse(sccp_device_t * d, char * tmp, uint8_t type) {
	char *tmp2, *tmp3;
	sccp_line_t * l = NULL;

	while ( tmp && (tmp2 = strsep(&tmp,";")) ) {
		if (tmp2 && !ast_strlen_zero(tmp2)) {
			tmp3 = strsep(&tmp2,":");
			if (tmp3 && !ast_strlen_zero(tmp3)) {
				l = sccp_line_find_byid(d, atoi(tmp3));
				if (!l) {
					sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found a call forward %s in the database, but no line available with instance %s\n", d->id, (type == SCCP_CFWD_ALL) ? "ALL" : "BUSY", tmp3);
				} else {
					tmp3 = strsep(&tmp2,"");
					if (tmp3 && !ast_strlen_zero(tmp3)) {
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: Restoring call forward %s to %s on line %s\n", d->id, (type == SCCP_CFWD_ALL) ? "ALL" : "BUSY", tmp3, l->name);
						l->cfwd_type = type;
						l->cfwd_num = strdup(tmp3);
					} else {
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found a call forward %s in the database, but no number available for line %s\n", d->id, (type == SCCP_CFWD_ALL) ? "ALL" : "BUSY", l->name);
					}
				}
			}
		}
	}
}

void sccp_dev_dbget(sccp_device_t * d) {
	char result[256]="", *tmp, *tmp1, *r;
	int i=0;

	if (!d)
		return;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Restoring device status (dnd, cfwd*) from the asterisk db\n", d->id);
	if (ast_db_get("SCCP", d->id, result, sizeof(result))) {
		return;
	}
	r = result;
	while ( (tmp = strsep(&r,",")) ) {
		tmp1 = strsep(&tmp,"=");
		if (tmp1) {
			if (!strcasecmp(tmp1, "dnd")) {
				if ( (tmp1 = strsep(&tmp,"")) ) {
					sscanf(tmp1, "%i", &i);
					d->dnd = (i) ? 1 : 0;
					sccp_log(10)(VERBOSE_PREFIX_3 "%s: dnd status is: %s\n", d->id, (d->dnd) ? "ON" : "OFF");
				}
			} else if (!strcasecmp(tmp1, "cfwdall")) {
				tmp1 = strsep(&tmp,"");
				sccp_cfwd_parse(d, tmp1, SCCP_CFWD_ALL);
			} else if (!strcasecmp(tmp1, "cfwdbusy")) {
				tmp1 = strsep(&tmp,"");
				sccp_cfwd_parse(d, tmp1, SCCP_CFWD_BUSY);
			}
		}
	}
}

void sccp_dev_dbclean() {
	struct ast_db_entry *entry;
	sccp_device_t * d;
	char key[256];

	ast_mutex_lock(&GLOB(devices_lock));
	entry = ast_db_gettree("SCCP", NULL);
	while (entry) {
		sscanf(entry->key,"/SCCP/%s", key);
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for %s in the devices list\n", key);
			if ( (strlen(key) == 15) && ((strncmp(key, "SEP",3) == 0) || (strncmp(key, "ATA",3)==0)) ) {
			d = GLOB(devices);
			while (d) {
				if (!strcasecmp(d->id, key))
					break;
				d = d->next;
			}
			if (!d) {
				ast_db_del("SCCP", key);
				sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: device %s removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry)
		ast_db_freetree(entry);
	ast_mutex_unlock(&GLOB(devices_lock));
}

const char * sccp_extensionstate2str(uint8_t type) {
	switch(type) {
	case AST_EXTENSION_NOT_INUSE:
		return "NotInUse";
	case AST_EXTENSION_BUSY:
		return "Busy";
	case AST_EXTENSION_UNAVAILABLE:
		return "Unavailable";
	case AST_EXTENSION_INUSE:
		return "InUse";
#ifdef CS_AST_HAS_EXTENSION_RINGING
	case AST_EXTENSION_RINGING:
		return "Ringing";
#endif
	default:
		return "Unknown";
	}
}

const char * sccpmsg2str(uint32_t e) {
	switch(e) {
/* Station -> Callmanager */
	case 0x0000:
		return "KeepAliveMessage";
	case 0x0001:
		return "RegisterMessage";
	case 0x0002:
		return "IpPortMessage";
	case 0x0003:
		return "KeypadButtonMessage";
	case 0x0004:
		return "EnblocCallMessage";
	case 0x0005:
		return "StimulusMessage";
	case 0x0006:
		return "OffHookMessage";
	case 0x0007:
		return "OnHookMessage";
	case 0x0008:
		return "HookFlashMessage";
	case 0x0009:
		return "ForwardStatReqMessage";
	case 0x000A:
		return "SpeedDialStatReqMessage";
	case 0x000B:
		return "LineStatReqMessage";
	case 0x000C:
		return "ConfigStatReqMessage";
	case 0x000D:
		return "TimeDateReqMessage";
	case 0x000E:
		return "ButtonTemplateReqMessage";
	case 0x000F:
		return "VersionReqMessage";
	case 0x0010:
		return "CapabilitiesResMessage";
	case 0x0011:
		return "MediaPortListMessage";
	case 0x0012:
		return "ServerReqMessage";
	case 0x0020:
		return "AlarmMessage";
	case 0x0021:
		return "MulticastMediaReceptionAck";
	case 0x0022:
		return "OpenReceiveChannelAck";
	case 0x0023:
		return "ConnectionStatisticsRes";
	case 0x0024:
		return "OffHookWithCgpnMessage";
	case 0x0025:
		return "SoftKeySetReqMessage";
	case 0x0026:
		return "SoftKeyEventMessage";
	case 0x0027:
		return "UnregisterMessage";
	case 0x0028:
		return "SoftKeyTemplateReqMessage";
	case 0x0029:
		return "RegisterTokenReq";
	case 0x002B:
		return "HeadsetStatusMessage";
	case 0x002C:
		return "MediaResourceNotification";
	case 0x002D:
		return "RegisterAvailableLinesMessage";
	case 0x002E:
		return "DeviceToUserDataMessage";
	case 0x002F:
		return "DeviceToUserDataResponseMessage";
	case 0x0030:
		return "UpdateCapabilitiesMessage";
	case 0x0031:
		return "OpenMultiMediaReceiveChannelAckMessage";
	case 0x0032:
		return "ClearConferenceMessage";
	case 0x0033:
		return "ServiceURLStatReqMessage";
	case 0x0034:
		return "FeatureStatReqMessage";
	case 0x0035:
		return "CreateConferenceResMessage";
	case 0x0036:
		return "DeleteConferenceResMessage";
	case 0x0037:
		return "ModifyConferenceResMessage";
	case 0x0038:
		return "AddParticipantResMessage";
	case 0x0039:
		return "AuditConferenceResMessage";
	case 0x0040:
		return "AuditParticipantResMessage";
	case 0x0041:
		return "DeviceToUserDataVersion1Message";
	case 0x0042:
		return "DeviceToUserDataResponseVersion1Message";

/* Callmanager -> Station */
	case 0x0081:
		return "RegisterAckMessage";
	case 0x0082:
		return "StartToneMessage";
	case 0x0083:
		return "StopToneMessage";
	case 0x0085:
		return "SetRingerMessage";
	case 0x0086:
		return "SetLampMessage";
	case 0x0087:
		return "SetHkFDetectMessage";
	case 0x0088:
		return "SetSpeakerModeMessage";
	case 0x0089:
		return "SetMicroModeMessage";
	case 0x008A:
		return "StartMediaTransmission";
	case 0x008B:
		return "StopMediaTransmission";
	case 0x008C:
		return "StartMediaReception";
	case 0x008D:
		return "StopMediaReception";
	case 0x008F:
		return "CallInfoMessage";
	case 0x0090:
		return "ForwardStatMessage";
	case 0x0091:
		return "SpeedDialStatMessage";
	case 0x0092:
		return "LineStatMessage";
	case 0x0093:
		return "ConfigStatMessage";
	case 0x0094:
		return "DefineTimeDate";
	case 0x0095:
		return "StartSessionTransmission";
	case 0x0096:
		return "StopSessionTransmission";
	case 0x0097:
		return "ButtonTemplateMessage";
	case 0x0098:
		return "VersionMessage";
	case 0x0099:
		return "DisplayTextMessage";
	case 0x009A:
		return "ClearDisplay";
	case 0x009B:
		return "CapabilitiesReqMessage";
	case 0x009C:
		return "EnunciatorCommandMessage";
	case 0x009D:
		return "RegisterRejectMessage";
	case 0x009E:
		return "ServerResMessage";
	case 0x009F:
		return "Reset";
	case 0x0100:
		return "KeepAliveAckMessage";
	case 0x0101:
		return "StartMulticastMediaReception";
	case 0x0102:
		return "StartMulticastMediaTransmission";
	case 0x0103:
		return "StopMulticastMediaReception";
	case 0x0104:
		return "StopMulticastMediaTransmission";
	case 0x0105:
		return "OpenReceiveChannel";
	case 0x0106:
		return "CloseReceiveChannel";
	case 0x0107:
		return "ConnectionStatisticsReq";
	case 0x0108:
		return "SoftKeyTemplateResMessage";
	case 0x0109:
		return "SoftKeySetResMessage";
	case 0x0110:
		return "SelectSoftKeysMessage";
	case 0x0111:
		return "CallStateMessage";
	case 0x0112:
		return "DisplayPromptStatusMessage";
	case 0x0113:
		return "ClearPromptStatusMessage";
	case 0x0114:
		return "DisplayNotifyMessage";
	case 0x0115:
		return "ClearNotifyMessage";
	case 0x0116:
		return "ActivateCallPlaneMessage";
	case 0x0117:
		return "DeactivateCallPlaneMessage";
	case 0x0118:
		return "UnregisterAckMessage";
	case 0x0119:
		return "BackSpaceReqMessage";
	case 0x011A:
		return "RegisterTokenAck";
	case 0x011B:
		return "RegisterTokenReject";
	case 0x011C:
		return "StartMediaFailureDetection";
	case 0x011D:
		return "DialedNumberMessage";
	case 0x011E:
		return "UserToDeviceDataMessage";
	case 0x011F:
		return "FeatureStatMessage";
	case 0x0120:
		return "DisplayPriNotifyMessage";
	case 0x0121:
		return "ClearPriNotifyMessage";
	case 0x0122:
		return "StartAnnouncementMessage";
	case 0x0123:
		return "StopAnnouncementMessage";
	case 0x0124:
		return "AnnouncementFinishMessage";
	case 0x0127:
		return "NotifyDtmfToneMessage";
	case 0x0128:
		return "SendDtmfToneMessage";
	case 0x0129:
		return "SubscribeDtmfPayloadReqMessage";
	case 0x012A:
		return "SubscribeDtmfPayloadResMessage";
	case 0x012B:
		return "SubscribeDtmfPayloadErrMessage";
	case 0x012C:
		return "UnSubscribeDtmfPayloadReqMessage";
	case 0x012D:
		return "UnSubscribeDtmfPayloadResMessage";
	case 0x012E:
		return "UnSubscribeDtmfPayloadErrMessage";
	case 0x012F:
		return "ServiceURLStatMessage";
	case 0x0130:
		return "CallSelectStatMessage";
	case 0x0131:
		return "OpenMultiMediaChannelMessage";
	case 0x0132:
		return "StartMultiMediaTransmission";
	case 0x0133:
		return "StopMultiMediaTransmission";
	case 0x0134:
		return "MiscellaneousCommandMessage";
	case 0x0135:
		return "FlowControlCommandMessage";
	case 0x0136:
		return "CloseMultiMediaReceiveChannel";
	case 0x0137:
		return "CreateConferenceReqMessage";
	case 0x0138:
		return "DeleteConferenceReqMessage";
	case 0x0139:
		return "ModifyConferenceReqMessage";
	case 0x013A:
		return "AddParticipantReqMessage";
	case 0x013B:
		return "DropParticipantReqMessage";
	case 0x013C:
		return "AuditConferenceReqMessage";
	case 0x013D:
		return "AuditParticipantReqMessage";
	case 0x013F:
		return "UserToDeviceDataVersion1Message";
	default:
		return "unknow";
	}
}

const char * skinny_lbl2str(uint8_t label) {
	switch(label) {
	case SKINNY_LBL_EMPTY:
		return "Empty";
	case SKINNY_LBL_REDIAL:
		return "Redial";
	case SKINNY_LBL_NEWCALL:
		return "NewCall";
	case SKINNY_LBL_HOLD:
		return "Hold";
	case SKINNY_LBL_TRANSFER:
		return "Transfer";
	case SKINNY_LBL_CFWDALL:
		return "CFwdALL";
	case SKINNY_LBL_CFWDBUSY:
		return "CFwdBusy";
	case SKINNY_LBL_CFWDNOANSWER:
		return "CFwdNoAnswer";
	case SKINNY_LBL_BACKSPACE:
		return "&lt;&lt;";
	case SKINNY_LBL_ENDCALL:
		return "EndCall";
	case SKINNY_LBL_RESUME:
		return "Resume";
	case SKINNY_LBL_ANSWER:
		return "Answer";
	case SKINNY_LBL_INFO:
		return "Info";
	case SKINNY_LBL_CONFRN:
		return "Confrn";
	case SKINNY_LBL_PARK:
		return "Park";
	case SKINNY_LBL_JOIN:
		return "Join";
	case SKINNY_LBL_MEETME:
		return "MeetMe";
	case SKINNY_LBL_PICKUP:
		return "PickUp";
	case SKINNY_LBL_GPICKUP:
		return "GPickUp";
	case SKINNY_LBL_YOUR_CURRENT_OPTIONS:
		return "Your current options";
	case SKINNY_LBL_OFF_HOOK:
		return "Off Hook";
	case SKINNY_LBL_ON_HOOK:
		return "On Hook";
	case SKINNY_LBL_RING_OUT:
		return "Ring out";
	case SKINNY_LBL_FROM:
		return "From ";
	case SKINNY_LBL_CONNECTED:
		return "Connected";
	case SKINNY_LBL_BUSY:
		return "Busy";
	case SKINNY_LBL_LINE_IN_USE:
		return "Line In Use";
	case SKINNY_LBL_CALL_WAITING:
		return "Call Waiting";
	case SKINNY_LBL_CALL_TRANSFER:
		return "Call Transfer";
	case SKINNY_LBL_CALL_PARK:
		return "Call Park";
	case SKINNY_LBL_CALL_PROCEED:
		return "Call Proceed";
	case SKINNY_LBL_IN_USE_REMOTE:
		return "In Use Remote";
	case SKINNY_LBL_ENTER_NUMBER:
		return "Enter number";
	case SKINNY_LBL_CALL_PARK_AT:
		return "Call park At";
	case SKINNY_LBL_PRIMARY_ONLY:
		return "Primary Only";
	case SKINNY_LBL_TEMP_FAIL:
		return "Temp Fail";
	case SKINNY_LBL_YOU_HAVE_VOICEMAIL:
		return "You Have VoiceMail";
	case SKINNY_LBL_FORWARDED_TO:
		return "Forwarded to";
	case SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE:
		return "Can Not Complete Conference";
	case SKINNY_LBL_NO_CONFERENCE_BRIDGE:
		return "No Conference Bridge";
	case SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL:
		return "Can Not Hold Primary Control";
	case SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT:
		return "Invalid Conference Participant";
	case SKINNY_LBL_IN_CONFERENCE_ALREADY:
		return "In Conference Already";
	case SKINNY_LBL_NO_PARTICIPANT_INFO:
		return "No Participant Info";
	case SKINNY_LBL_EXCEED_MAXIMUM_PARTIES:
		return "Exceed Maximum Parties";
	case SKINNY_LBL_KEY_IS_NOT_ACTIVE:
		return "Key Is Not Active";
	case SKINNY_LBL_ERROR_NO_LICENSE:
		return "Error No License";
	case SKINNY_LBL_ERROR_DBCONFIG:
		return "Error DBConfig";
	case SKINNY_LBL_ERROR_DATABASE:
		return "Error Database";
	case SKINNY_LBL_ERROR_PASS_LIMIT:
		return "Error Pass Limit";
	case SKINNY_LBL_ERROR_UNKNOWN:
		return "Error Unknown";
	case SKINNY_LBL_ERROR_MISMATCH:
		return "Error Mismatch";
	case SKINNY_LBL_CONFERENCE:
		return "Conference";
	case SKINNY_LBL_PARK_NUMBER:
		return "Park Number";
	case SKINNY_LBL_PRIVATE:
		return "Private";
	case SKINNY_LBL_NOT_ENOUGH_BANDWIDTH:
		return "Not Enough Bandwidth";
	case SKINNY_LBL_UNKNOWN_NUMBER:
		return "Unknown Number";
	case SKINNY_LBL_RMLSTC:
		return "RmLstC";
	case SKINNY_LBL_VOICEMAIL:
		return "Voicemail";
	case SKINNY_LBL_IMMDIV:
		return "ImmDiv";
	case SKINNY_LBL_INTRCPT:
		return "Intrcpt";
	case SKINNY_LBL_SETWTCH:
		return "SetWtch";
	case SKINNY_LBL_TRNSFVM:
		return "TrnsfVM";
	case SKINNY_LBL_DND:
		return "DND";
	case SKINNY_LBL_DIVALL:
		return "DivAll";
	case SKINNY_LBL_CALLBACK:
		return "CallBack";
	case SKINNY_LBL_NETWORK_CONGESTION_REROUTING:
		return "Network congestion,rerouting";
	case SKINNY_LBL_BARGE:
		return "Barge";
	case SKINNY_LBL_FAILED_TO_SETUP_BARGE:
		return "Failed to setup Barge";
	case SKINNY_LBL_ANOTHER_BARGE_EXISTS:
		return "Another Barge exists";
	case SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE:
		return "Incompatible device type";
	case SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE:
		return "No Park Number Available";
	case SKINNY_LBL_CALLPARK_REVERSION:
		return "CallPark Reversion";
	case SKINNY_LBL_SERVICE_IS_NOT_ACTIVE:
		return "Service is not Active";
	case SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER:
		return "High Traffic Try Again Later";
	case SKINNY_LBL_QRT:
		return "QRT";
	case SKINNY_LBL_MCID:
		return "MCID";
	case SKINNY_LBL_DIRTRFR:
		return "DirTrfr";
	case SKINNY_LBL_SELECT:
		return "Select";
	case SKINNY_LBL_CONFLIST:
		return "ConfList";
	case SKINNY_LBL_IDIVERT:
		return "iDivert";
	case SKINNY_LBL_CBARGE:
		return "cBarge";
	case SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER:
		return "Can Not Complete Transfer";
	case SKINNY_LBL_CAN_NOT_JOIN_CALLS:
		return "Can Not Join Calls";
	case SKINNY_LBL_MCID_SUCCESSFUL:
		return "Mcid Successful";
	case SKINNY_LBL_NUMBER_NOT_CONFIGURED:
		return "Number Not Configured";
	case SKINNY_LBL_SECURITY_ERROR:
		return "Security Error";
	case SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE:
		return "Video Bandwidth Unavailable";
	default:
		return "";
	}
}

const char * skinny_tone2str(uint8_t tone) {
	switch(tone) {
		case SKINNY_TONE_SILENCE:
		return "Silence";
	case SKINNY_TONE_DTMF1:
		return "Dtmf1";
	case SKINNY_TONE_DTMF2:
		return "Dtmf2";
	case SKINNY_TONE_DTMF3:
		return "Dtmf3";
	case SKINNY_TONE_DTMF4:
		return "Dtmf4";
	case SKINNY_TONE_DTMF5:
		return "Dtmf5";
	case SKINNY_TONE_DTMF6:
		return "Dtmf6";
	case SKINNY_TONE_DTMF7:
		return "Dtmf7";
	case SKINNY_TONE_DTMF8:
		return "Dtmf8";
	case SKINNY_TONE_DTMF9:
		return "Dtmf9";
	case SKINNY_TONE_DTMF0:
		return "Dtmf0";
	case SKINNY_TONE_DTMFSTAR:
		return "DtmfStar";
	case SKINNY_TONE_DTMFPOUND:
		return "DtmfPound";
	case SKINNY_TONE_DTMFA:
		return "DtmfA";
	case SKINNY_TONE_DTMFB:
		return "DtmfB";
	case SKINNY_TONE_DTMFC:
		return "DtmfC";
	case SKINNY_TONE_DTMFD:
		return "DtmfD";
	case SKINNY_TONE_INSIDEDIALTONE:
		return "InsideDialTone";
	case SKINNY_TONE_OUTSIDEDIALTONE:
		return "OutsideDialTone";
	case SKINNY_TONE_LINEBUSYTONE:
		return "LineBusyTone";
	case SKINNY_TONE_ALERTINGTONE:
		return "AlertingTone";
	case SKINNY_TONE_REORDERTONE:
		return "ReorderTone";
	case SKINNY_TONE_RECORDERWARNINGTONE:
		return "RecorderWarningTone";
	case SKINNY_TONE_RECORDERDETECTEDTONE:
		return "RecorderDetectedTone";
	case SKINNY_TONE_REVERTINGTONE:
		return "RevertingTone";
	case SKINNY_TONE_RECEIVEROFFHOOKTONE:
		return "ReceiverOffHookTone";
	case SKINNY_TONE_PARTIALDIALTONE:
		return "PartialDialTone";
	case SKINNY_TONE_NOSUCHNUMBERTONE:
		return "NoSuchNumberTone";
	case SKINNY_TONE_BUSYVERIFICATIONTONE:
		return "BusyVerificationTone";
	case SKINNY_TONE_CALLWAITINGTONE:
		return "CallWaitingTone";
	case SKINNY_TONE_CONFIRMATIONTONE:
		return "ConfirmationTone";
	case SKINNY_TONE_CAMPONINDICATIONTONE:
		return "CampOnIndicationTone";
	case SKINNY_TONE_RECALLDIALTONE:
		return "RecallDialTone";
	case SKINNY_TONE_ZIPZIP:
		return "ZipZip";
	case SKINNY_TONE_ZIP:
		return "Zip";
	case SKINNY_TONE_BEEPBONK:
		return "BeepBonk";
	case SKINNY_TONE_MUSICTONE:
		return "MusicTone";
	case SKINNY_TONE_HOLDTONE:
		return "HoldTone";
	case SKINNY_TONE_TESTTONE:
		return "TestTone";
	case SKINNY_TONE_ADDCALLWAITING:
		return "AddCallWaiting";
	case SKINNY_TONE_PRIORITYCALLWAIT:
		return "PriorityCallWait";
	case SKINNY_TONE_RECALLDIAL:
		return "RecallDial";
	case SKINNY_TONE_BARGIN:
		return "BargIn";
	case SKINNY_TONE_DISTINCTALERT:
		return "DistinctAlert";
	case SKINNY_TONE_PRIORITYALERT:
		return "PriorityAlert";
	case SKINNY_TONE_REMINDERRING:
		return "ReminderRing";
	case SKINNY_TONE_MF1:
		return "MF1";
	case SKINNY_TONE_MF2:
		return "MF2";
	case SKINNY_TONE_MF3:
		return "MF3";
	case SKINNY_TONE_MF4:
		return "MF4";
	case SKINNY_TONE_MF5:
		return "MF5";
	case SKINNY_TONE_MF6:
		return "MF6";
	case SKINNY_TONE_MF7:
		return "MF7";
	case SKINNY_TONE_MF8:
		return "MF8";
	case SKINNY_TONE_MF9:
		return "MF9";
	case SKINNY_TONE_MF0:
		return "MF0";
	case SKINNY_TONE_MFKP1:
		return "MFKP1";
	case SKINNY_TONE_MFST:
		return "MFST";
	case SKINNY_TONE_MFKP2:
		return "MFKP2";
	case SKINNY_TONE_MFSTP:
		return "MFSTP";
	case SKINNY_TONE_MFST3P:
		return "MFST3P";
	case SKINNY_TONE_MILLIWATT:
		return "MILLIWATT";
	case SKINNY_TONE_MILLIWATTTEST:
		return "MILLIWATTTEST";
	case SKINNY_TONE_HIGHTONE:
		return "HIGHTONE";
	case SKINNY_TONE_FLASHOVERRIDE:
		return "FLASHOVERRIDE";
	case SKINNY_TONE_FLASH:
		return "FLASH";
	case SKINNY_TONE_PRIORITY:
		return "PRIORITY";
	case SKINNY_TONE_IMMEDIATE:
		return "IMMEDIATE";
	case SKINNY_TONE_PREAMPWARN:
		return "PREAMPWARN";
	case SKINNY_TONE_2105HZ:
		return "2105HZ";
	case SKINNY_TONE_2600HZ:
		return "2600HZ";
	case SKINNY_TONE_440HZ:
		return "440HZ";
	case SKINNY_TONE_300HZ:
		return "300HZ";
	case SKINNY_TONE_NOTONE:
		return "NoTone";
	default:
		return "";
	}
}

const char * skinny_alarm2str(uint8_t alarm) {
	switch(alarm) {
	case SKINNY_ALARM_CRITICAL:
		return "Critical";
	case SKINNY_ALARM_WARNING:
		return "Warning";
	case SKINNY_ALARM_INFORMATIONAL:
		return "Informational";
	case SKINNY_ALARM_UNKNOWN:
		return "Unknown";
	case SKINNY_ALARM_MAJOR:
		return "Major";
	case SKINNY_ALARM_MINOR:
		return "Minor";
	case SKINNY_ALARM_MARGINAL:
		return "Marginal";
	case SKINNY_ALARM_TRACEINFO:
		return "TraceInfo";
	default:
		return "unknow";
	}
}

const char * skinny_devicetype2str(uint32_t type) {
	switch(type) {
	case SKINNY_DEVICETYPE_30SPPLUS:
		return "30SPplus";
	case SKINNY_DEVICETYPE_12SPPLUS:
		return "12SPplus";
	case SKINNY_DEVICETYPE_12SP:
		return "12SP";
	case SKINNY_DEVICETYPE_12:
		return "12";
	case SKINNY_DEVICETYPE_30VIP:
		return "30VIP";
	case SKINNY_DEVICETYPE_CISCO7910:
		return "Cisco7910";
	case SKINNY_DEVICETYPE_CISCO7960:
		return "Cisco7960";
	case SKINNY_DEVICETYPE_CISCO7940:
		return "Cisco7940";
	case SKINNY_DEVICETYPE_VIRTUAL30SPPLUS:
		return "Virtual30SPplus";
	case SKINNY_DEVICETYPE_PHONEAPPLICATION:
		return "PhoneApplication";
	case SKINNY_DEVICETYPE_ANALOGACCESS:
		return "AnalogAccess";
	case SKINNY_DEVICETYPE_DIGITALACCESSPRI:
		return "DigitalAccessPRI";
	case SKINNY_DEVICETYPE_DIGITALACCESST1:
		return "DigitalAccessT1";
	case SKINNY_DEVICETYPE_DIGITALACCESSTITAN2:
		return "DigitalAccessTitan2";
	case SKINNY_DEVICETYPE_ANALOGACCESSELVIS:
		return "AnalogAccessElvis";
	case SKINNY_DEVICETYPE_DIGITALACCESSLENNON:
		return "DigitalAccessLennon";
	case SKINNY_DEVICETYPE_CONFERENCEBRIDGE:
		return "ConferenceBridge";
	case SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO:
		return "ConferenceBridgeYoko";
	case SKINNY_DEVICETYPE_H225:
		return "H225";
	case SKINNY_DEVICETYPE_H323PHONE:
		return "H323Phone";
	case SKINNY_DEVICETYPE_H323TRUNK:
		return "H323Trunk";
	case SKINNY_DEVICETYPE_MUSICONHOLD:
		return "MusicOnHold";
	case SKINNY_DEVICETYPE_PILOT:
		return "Pilot";
	case SKINNY_DEVICETYPE_TAPIPORT:
		return "TapiPort";
	case SKINNY_DEVICETYPE_TAPIROUTEPOINT:
		return "TapiRoutePoint";
	case SKINNY_DEVICETYPE_VOICEINBOX:
		return "VoiceInBox";
	case SKINNY_DEVICETYPE_VOICEINBOXADMIN:
		return "VoiceInboxAdmin";
	case SKINNY_DEVICETYPE_LINEANNUNCIATOR:
		return "LineAnnunciator";
	case SKINNY_DEVICETYPE_ROUTELIST:
		return "RouteList";
	case SKINNY_DEVICETYPE_LOADSIMULATOR:
		return "LoadSimulator";
	case SKINNY_DEVICETYPE_MEDIATERMINATIONPOINT:
		return "MediaTerminationPoint";
	case SKINNY_DEVICETYPE_MEDIATERMINATIONPOINTYOKO:
		return "MediaTerminationPointYoko";
	case SKINNY_DEVICETYPE_MGCPSTATION:
		return "MGCPStation";
	case SKINNY_DEVICETYPE_MGCPTRUNK:
		return "MGCPTrunk";
	case SKINNY_DEVICETYPE_RASPROXY:
		return "RASProxy";
	case SKINNY_DEVICETYPE_NOTDEFINED:
		return "NotDefined";
	case SKINNY_DEVICETYPE_CISCO7920:
		return "Cisco7920";
	case SKINNY_DEVICETYPE_CISCO7970:
		return "Cisco7970";
	case SKINNY_DEVICETYPE_CISCO7902:
		return "Cisco7902";
	case SKINNY_DEVICETYPE_CISCO7905:
		return "Cisco7905";
	case SKINNY_DEVICETYPE_CISCO7911:
		return "Cisco7911";
	case SKINNY_DEVICETYPE_CISCO7912:
		return "Cisco7912";
	case SKINNY_DEVICETYPE_CISCO7935:
		return "Cisco7935";
	case SKINNY_DEVICETYPE_CISCO7936:
		return "Cisco7936";
	case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
		return "Cisco_IP_Communicator";
	case SKINNY_DEVICETYPE_ATA186:
		return "Cisco Ata 186";
	case SKINNY_DEVICETYPE_CISCO7941:
		return "Cisco7941";
	case SKINNY_DEVICETYPE_CISCO7941GE:
		return "Cisco7941GE";
	case SKINNY_DEVICETYPE_CISCO7961:
		return "Cisco7961";
	case SKINNY_DEVICETYPE_CISCO7961GE:
		return "Cisco7961GE";
	case SKINNY_DEVICETYPE_CISCO7971:
		return "Cisco7971";
	case SKINNY_DEVICETYPE_CISCO7985:
		return "Cisco7985";
	default:
		return "unknow";
	}
}

const char * skinny_stimulus2str(uint8_t type) {
	switch(type) {
	case SKINNY_STIMULUS_LASTNUMBERREDIAL:
		return "LastNumberRedial";
	case SKINNY_STIMULUS_SPEEDDIAL:
		return "SpeedDial";
	case SKINNY_STIMULUS_HOLD:
		return "Hold";
	case SKINNY_STIMULUS_TRANSFER:
		return "Transfer";
	case SKINNY_STIMULUS_FORWARDALL:
		return "ForwardAll";
	case SKINNY_STIMULUS_FORWARDBUSY:
		return "ForwardBusy";
	case SKINNY_STIMULUS_FORWARDNOANSWER:
		return "ForwardNoAnswer";
	case SKINNY_STIMULUS_DISPLAY:
		return "Display";
	case SKINNY_STIMULUS_LINE:
		return "Line";
	case SKINNY_STIMULUS_T120CHAT:
		return "T120Chat";
	case SKINNY_STIMULUS_T120WHITEBOARD:
		return "T120Whiteboard";
	case SKINNY_STIMULUS_T120APPLICATIONSHARING:
		return "T120ApplicationSharing";
	case SKINNY_STIMULUS_T120FILETRANSFER:
		return "T120FileTransfer";
	case SKINNY_STIMULUS_VIDEO:
		return "Video";
	case SKINNY_STIMULUS_VOICEMAIL:
		return "VoiceMail";
	case SKINNY_STIMULUS_AUTOANSWER:
		return "AutoAnswer";
	case SKINNY_STIMULUS_GENERICAPPB1:
		return "GenericAppB1";
	case SKINNY_STIMULUS_GENERICAPPB2:
		return "GenericAppB2";
	case SKINNY_STIMULUS_GENERICAPPB3:
		return "GenericAppB3";
	case SKINNY_STIMULUS_GENERICAPPB4:
		return "GenericAppB4";
	case SKINNY_STIMULUS_GENERICAPPB5:
		return "GenericAppB5";
	case SKINNY_STIMULUS_MEETMECONFERENCE:
		return "MeetMeConference";
	case SKINNY_STIMULUS_CONFERENCE:
		return "Conference";
	case SKINNY_STIMULUS_CALLPARK:
		return "CallPark";
	case SKINNY_STIMULUS_CALLPICKUP:
		return "CallPickup";
	case SKINNY_STIMULUS_GROUPCALLPICKUP:
		return "GroupCallPickup";
	default:
		return "unknow";
	}
}

const char * skinny_buttontype2str(uint8_t type) {
	switch(type) {
	case SKINNY_BUTTONTYPE_UNUSED:
		return "Unused";
	case SKINNY_BUTTONTYPE_LASTNUMBERREDIAL:
		return "LastNumberRedial";
	case SKINNY_BUTTONTYPE_SPEEDDIAL:
		return "SpeedDial";
	case SKINNY_BUTTONTYPE_HOLD:
		return "Hold";
	case SKINNY_BUTTONTYPE_TRANSFER:
		return "Transfer";
	case SKINNY_BUTTONTYPE_FORWARDALL:
		return "ForwardAll";
	case SKINNY_BUTTONTYPE_FORWARDBUSY:
		return "ForwardBusy";
	case SKINNY_BUTTONTYPE_FORWARDNOANSWER:
		return "ForwardNoAnswer";
	case SKINNY_BUTTONTYPE_DISPLAY:
		return "Display";
	case SKINNY_BUTTONTYPE_LINE:
		return "Line";
	case SKINNY_BUTTONTYPE_T120CHAT:
		return "T120Chat";
	case SKINNY_BUTTONTYPE_T120WHITEBOARD:
		return "T120Whiteboard";
	case SKINNY_BUTTONTYPE_T120APPLICATIONSHARING:
		return "T120ApplicationSharing";
	case SKINNY_BUTTONTYPE_T120FILETRANSFER:
		return "T120FileTransfer";
	case SKINNY_BUTTONTYPE_VIDEO:
		return "Video";
	case SKINNY_BUTTONTYPE_VOICEMAIL:
		return "Voicemail";
	case SKINNY_BUTTONTYPE_ANSWERRELEASE:
		return "AnswerRelease";
	case SKINNY_BUTTONTYPE_AUTOANSWER:
		return "AutoAnswer";
	case SKINNY_BUTTONTYPE_GENERICAPPB1:
		return "GenericAppB1";
	case SKINNY_BUTTONTYPE_GENERICAPPB2:
		return "GenericAppB2";
	case SKINNY_BUTTONTYPE_GENERICAPPB3:
		return "GenericAppB3";
	case SKINNY_BUTTONTYPE_GENERICAPPB4:
		return "GenericAppB4";
	case SKINNY_BUTTONTYPE_GENERICAPPB5:
		return "GenericAppB5";
	case SKINNY_BUTTONTYPE_MEETMECONFERENCE:
		return "MeetMeConference";
	case SKINNY_BUTTONTYPE_CONFERENCE:
		return "Conference";
	case SKINNY_BUTTONTYPE_CALLPARK:
		return "CallPark";
	case SKINNY_BUTTONTYPE_CALLPICKUP:
		return "CallPickup";
	case SKINNY_BUTTONTYPE_GROUPCALLPICKUP:
		return "GroupCallPickup";
	case SKINNY_BUTTONTYPE_KEYPAD:
		return "Keypad";
	case SKINNY_BUTTONTYPE_AEC:
		return "AEC";
	case SKINNY_BUTTONTYPE_UNDEFINED:
		return "Undefined";
	/* not a skinny definition */
	case SKINNY_BUTTONTYPE_MULTI:
		return "Line_or_Speeddial";
	default:
		return "unknow";
	}
}

const char * skinny_lampmode2str(uint8_t type) {
	switch(type) {
	case SKINNY_LAMP_OFF:
		return "LampOff";
	case SKINNY_LAMP_ON:
		return "LampOn";
	case SKINNY_LAMP_WINK:
		return "LampWink";
	case SKINNY_LAMP_FLASH:
		return "LampFlash";
	case SKINNY_LAMP_BLINK:
		return "LampBlink";
	default:
		return "unknow";
	}
}

const char * skinny_ringermode2str(uint8_t type) {
	switch(type) {
	case SKINNY_STATION_RINGOFF:
		return "RingOff";
	case SKINNY_STATION_INSIDERING:
		return "Inside";
	case SKINNY_STATION_OUTSIDERING:
		return "Outside";
	case SKINNY_STATION_FEATURERING:
		return "Feature";
	default:
		return "unknow";
	}
}

const char * skinny_softkeyset2str(uint8_t type) {
	switch(type) {
	case KEYMODE_ONHOOK:
		return "OnHook";
	case KEYMODE_CONNECTED:
		return "Connected";
	case KEYMODE_ONHOLD:
		return "OnHold";
	case KEYMODE_RINGIN:
		return "Ringin";
	case KEYMODE_OFFHOOK:
		return "OffHook";
	case KEYMODE_CONNTRANS:
		return "ConnTrans";
	case KEYMODE_DIGITSFOLL:
		return "DigitsFoll";
	case KEYMODE_CONNCONF:
		return "ConnConf";
	case KEYMODE_RINGOUT:
		return "RingOut";
	case KEYMODE_OFFHOOKFEAT:
		return "OffHookFeat";
	case KEYMODE_MYST:
		return "Myst";
	default:
		return "unknow";
	}
}

const char * skinny_devicestate2str(uint8_t type) {
	switch(type) {
	case SCCP_DEVICESTATE_ONHOOK:
		return "OnHook";
	case SCCP_DEVICESTATE_OFFHOOK:
		return "OffHook";
	case SCCP_DEVICESTATE_UNAVAILABLE:
		return "Unavailable";
	case SCCP_DEVICESTATE_DND:
		return "Do Not Disturb";
	case SCCP_DEVICESTATE_FWDALL:
		return "Forward All";
	default:
		return "unknow";
	}
}

const char * skinny_registrationstate2str(uint8_t type) {
	switch(type) {
	case SKINNY_DEVICE_RS_NONE:
		return "None";
	case SKINNY_DEVICE_RS_PROGRESS:
		return "Progress";
	case SKINNY_DEVICE_RS_FAILED:
		return "Failes";
	case SKINNY_DEVICE_RS_OK:
		return "Ok";
	case SKINNY_DEVICE_RS_TIMEOUT:
		return "Timeout";
	default:
		return "unknow";
	}
}

const char * skinny_calltype2str(uint8_t type) {
	switch(type) {
	case SKINNY_CALLTYPE_INBOUND:
		return "Inbound";
	case SKINNY_CALLTYPE_OUTBOUND:
		return "Outbound";
	case SKINNY_CALLTYPE_FORWARD:
		return "Forward";
	default:
		return "unknow";
	}
}

const char * skinny_codec2str(uint8_t type) {
	switch(type) {
	case 1:
		return "Non-standard codec";
	case 2:
		return "G.711 A-law 64k";
	case 3:
		return "G.711 A-law 56k";
	case 4:
		return "G.711 u-law 64k";
	case 5:
		return "G.711 u-law 56k";
	case 6:
		return "G.722 64k";
	case 7:
		return "G.722 56k";
	case 8:
		return "G.722 48k";
	case 9:
		return "G.723.1";
	case 10:
		return "G.728";
	case 11:
		return "G.729";
	case 12:
		return "G.729 Annex A";
	case 13:
		return "IS11172 AudioCap";
	case 14:
		return "IS13818 AudioCap";
	case 15:
		return "G.729 Annex B";
	case 16:
		return "G.729 Annex A + B";
	case 18:
		return "GSM Full Rate";
	case 19:
		return "GSM Half Rate";
	case 20:
		return "GSM Enhanced Full Rate";
	case 25:
		return "Wideband 256k";
	case 32:
		return "Data 64k";
	case 33:
		return "Data 56k";
	case 80:
		return "GSM";
	case 81:
		return "ActiveVoice";
	case 82:
		return "G.726 32K";
	case 83:
		return "G.726 24K";
	case 84:
		return "G.726 16K";
	case 85:
		return "G.729 Annex B";
	case 86:
		return "G.729B Low Complexity";
	default:
		return "unknow";
	}
}

uint8_t sccp_codec_ast2skinny(int fmt) {
	switch(fmt) {
	case AST_FORMAT_ALAW:
		return 2;
	case AST_FORMAT_ULAW:
		return 4;
	case AST_FORMAT_G723_1:
		return 9;
	case AST_FORMAT_G729A:
		return 12;
	default:
		return 0;
	}
}

int sccp_codec_skinny2ast(uint8_t fmt) {
	switch(fmt) {
	case 2:
		return AST_FORMAT_ALAW;
	case 4:
		return AST_FORMAT_ULAW;
	case 9:
		return AST_FORMAT_G723_1;
	case 12:
		return AST_FORMAT_G729A;
	default:
		return 0;
	}
}

const char * sccp_dndmode2str(uint8_t type) {
	switch(type) {
	case SCCP_DNDMODE_OFF:
		return "Off";
	case SCCP_DNDMODE_REJECT:
		return "Reject";
	case SCCP_DNDMODE_SILENT:
		return "Silent";
	default:
		return "unknow";
	}
}

#ifndef CS_AST_HAS_STRINGS
char *ast_skip_blanks(char *str) {
	while (*str && *str < 33)
		str++;
	return str;
}

char *ast_trim_blanks(char *str) {
	char *work = str;
	if (work) {
		work += strlen(work) - 1;
		while ((work >= str) && *work < 33)
			*(work--) = '\0';
	}
	return str;
}

char *ast_skip_nonblanks(char *str) {
	while (*str && *str > 32)
		str++;
	return str;
}

char *ast_strip(char *s) {
	s = ast_skip_blanks(s);
	if (s)
		ast_trim_blanks(s);
	return s;
}
#endif

#ifndef CS_AST_HAS_APP_SEPARATE_ARGS
unsigned int sccp_app_separate_args(char *buf, char delim, char **array, int arraylen) {
	int argc;
	char *scan;
	int paren = 0;

	if (!buf || !array || !arraylen)
		return 0;

	memset(array, 0, arraylen * sizeof(*array));

	scan = buf;

	for (argc = 0; *scan && (argc < arraylen - 1); argc++) {
		array[argc] = scan;
		for (; *scan; scan++) {
			if (*scan == '(')
				paren++;
			else if (*scan == ')') {
				if (paren)
					paren--;
			} else if ((*scan == delim) && !paren) {
				*scan++ = '\0';
				break;
			}
		}
	}

	if (*scan)
		array[argc++] = scan;

	return argc;
}
#endif

/**
 * \brief get the SoftKeyIndex for a given SoftKeyLabel on specified keymode
 * 
 * 
 * \todo implement function for finding index of given SoftKey
 */
int sccp_softkeyindex_find_label(sccp_device_t * d, unsigned int keymode, unsigned int softkey) {
	
	return -1;
}
