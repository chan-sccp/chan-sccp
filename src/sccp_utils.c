/*!
 * \file 	sccp_utils.c
 * \brief 	SCCP Utils Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \version 	$LastChangedDate$
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_config.h"




#include <asterisk/astdb.h>
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/devicestate.h>


/*!
 * \brief is Printable Character
 * \param c Character
 * \return true/false
 */
static int isPrintableChar(char c)
{
	if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z')
			&& (c < '0' || c > '9') && (c != ' ') && (c != '\'')
			&& (c != '(') && (c != ')') && (c != '+') && (c != ',')
			&& (c != '-') && (c != '.') && (c != '/') && (c != ':')
			&& (c != '=') && (c != '?') && (c != '_') && (c != '\\')
			&& (c != '@') && (c != '"') && (c != '%') && (c != '$')
			&& (c != '&') && (c != '#') && (c != ';')
			//&& (c != '£')
			&& (c != '<') && (c != '>') && (c != ']') && (c != '{')
			&& (c != '}') && (c != '*') && (c != '^')) {
		return 0;
	} else {
		return 1;
	}
}

/*!
 * \brief Print out a messagebuffer
 * \param messagebuffer Pointer to Message Buffer as char
 * \param len Lenght as Int
 */
void sccp_dump_packet(unsigned char * messagebuffer, int len)
{
	int cur, t, i;
	int rows, cols;
	int res = 16;
	char row[256];
	char temp[16];
	char temp2[32];
	cur = 0;

	cols = res;
	rows = len / cols + (len % cols ? 1 : 0);

	for(i = 0; i < rows; i++)
	{
		memset(row, 0, sizeof(row));
//		sprintf(row, "%04d: %08X - ", i, cur);
		sprintf(row, "%08X - ", cur);
		if((i == rows - 1) && (len % res > 0)) // FIXED after 354 -FS
			cols = len % res;

        memset(temp2, 0, sizeof(temp2));
		for(t = 0; t < cols; t++) {
			memset(temp, 0, sizeof(temp));
			sprintf(temp, "%02X ", messagebuffer[cur]);
			strcat(row, temp);

			if(isPrintableChar((char)messagebuffer[cur]))
				sprintf(temp, "%c", messagebuffer[cur]);
			else
				sprintf(temp, ".");
            strcat(temp2, temp);
			cur++;
		}

		if(cols < res) {
            for(t = 0; t < res - cols; t++) {
				strcat(row, "   ");
            }
        }
		strcat(row, temp2);
		sccp_log(10)(VERBOSE_PREFIX_1 "%s\n", row);
	}
}

/*!
 * \brief Add Host to the Permithost Linked List
 * \param d SCCP Device
 * \param config_string as Character
 */
void sccp_permithost_addnew(sccp_device_t * d, const char * config_string)
{
	sccp_hostname_t *permithost;

	if ((permithost = ast_malloc(sizeof(sccp_hostname_t)))) {
		sccp_copy_string(permithost->name, config_string, sizeof(permithost->name));
		SCCP_LIST_INSERT_HEAD(&d->permithosts, permithost, list);
	} else {
		ast_log(LOG_WARNING, "Error adding the permithost = %s to the list\n", config_string);
	}
}




/*!
 * \brief Add New AddOn/Sidecar to Device's AddOn Linked List
 * \param d SCCP Device
 * \param addon_config_type AddOn Type as Character
 */
void sccp_addon_addnew(sccp_device_t * d, const char * addon_config_type) {
	int addon_type;

	// check for device
	if(!d)
		return;

	// checking if devicetype is specified
	if(ast_strlen_zero(d->config_type)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Addon type (%s) must be specified after device type\n", addon_config_type);
		return;
	}

	if(!strcasecmp(addon_config_type, "7914"))
		addon_type = SKINNY_DEVICETYPE_CISCO7914;
	else if(!strcasecmp(addon_config_type, "7915"))
		addon_type = SKINNY_DEVICETYPE_CISCO7915;
	else if(!strcasecmp(addon_config_type, "7916"))
		addon_type = SKINNY_DEVICETYPE_CISCO7916;
	else {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Unknown addon type (%s) for device (%s)\n", addon_config_type, d->config_type);
		return;
	}

	if( !((addon_type == SKINNY_DEVICETYPE_CISCO7914) &&
		((!strcasecmp(d->config_type, "7960")) ||
		 (!strcasecmp(d->config_type, "7961")) ||
		 (!strcasecmp(d->config_type, "7962")) ||
		 (!strcasecmp(d->config_type, "7965")) ||
		 (!strcasecmp(d->config_type, "7970")) ||
		 (!strcasecmp(d->config_type, "7971")) ||
		 (!strcasecmp(d->config_type, "7975")))) &&
		!((addon_type == SKINNY_DEVICETYPE_CISCO7915) &&
		((!strcasecmp(d->config_type, "7962")) ||
		 (!strcasecmp(d->config_type, "7965")) ||
		 (!strcasecmp(d->config_type, "7975")))) &&
		!((addon_type == SKINNY_DEVICETYPE_CISCO7916) &&
		((!strcasecmp(d->config_type, "7962")) ||
		 (!strcasecmp(d->config_type, "7965")) ||
		 (!strcasecmp(d->config_type, "7975"))))) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Configured device (%s) does not support the specified addon type (%s)\n", d->config_type, addon_config_type);
		return;
	}

	sccp_addon_t * a = ast_malloc(sizeof(sccp_addon_t));
	if (!a) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Unable to allocate memory for a device addon\n");
		return;
	}
	memset(a, 0, sizeof(sccp_addon_t));

	a->type = addon_type;
	a->device = d;

	SCCP_LIST_INSERT_HEAD(&d->addons, a, list);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Added addon (%d) taps on device type (%s)\n", (d->id?d->id:"SCCP"), a->type, d->config_type);
}


/*!
 * \brief Return Number of Buttons on AddOn Device
 * \param d SCCP Device
 * \return taps (Number of Buttons on AddOn Device)
 */
int sccp_addons_taps(sccp_device_t * d)
{
	sccp_addon_t * cur = NULL;
	int taps = 0;

	if(!strcasecmp(d->config_type, "7914"))
		return 28; // on compatibility mode it returns 28 taps for a double 7914 addon

	SCCP_LIST_LOCK(&d->addons);
	SCCP_LIST_TRAVERSE(&d->addons, cur, list) {
		if(cur->type == SKINNY_DEVICETYPE_CISCO7914)
			taps += 14;
		if(cur->type == SKINNY_DEVICETYPE_CISCO7915 || cur->type == SKINNY_DEVICETYPE_CISCO7916)
			taps += 20;
		sccp_log(22)(VERBOSE_PREFIX_3 "%s: Found (%d) taps on device addon (%d)\n", (d->id?d->id:"SCCP"), taps, cur->type);
	}
	SCCP_LIST_UNLOCK(&d->addons);

	return taps;
}

/*!
 * \brief Clear all Addons from AddOn Linked List
 * \param d SCCP Device
 */
void sccp_addons_clear(sccp_device_t * d)
{
	if(!d)
		return;

	while ((AST_LIST_REMOVE_HEAD(&d->addons, list)));
}

/*!
 * \brief Return AddOn Linked List
 * \param d SCCP Device
 * \return addons_list
 */
char * sccp_addons_list(sccp_device_t * d)
{
	char * addons_list = NULL;

	return addons_list;
}

/*!
 * \brief Put SCCP into Safe Sleep for [num] milli_seconds
 * \param ms MilliSeconds
 */
void sccp_safe_sleep(int ms)
{
	struct timeval start = { 0 , 0 };

	start = ast_tvnow();
	usleep(1);
	while(ast_tvdiff_ms(ast_tvnow(), start) < ms) {
		usleep(1);
	}
}

/*!
 * \brief Create an asterisk variable
 * \param buf Variable Definition in format "[var]=[value]"
 */
struct ast_variable * sccp_create_variable(const char *buf) {
	struct ast_variable *tmpvar = NULL;
	char *varname = ast_strdupa(buf), *varval = NULL;

	if ((varval = strchr(varname,'='))) {
		*varval++ = '\0';
#ifndef ASTERISK_CONF_1_6
		if ((tmpvar = ast_variable_new(varname, varval))) {
#else
		if ((tmpvar = ast_variable_new(varname, varval, "" ))) {
#endif
			return tmpvar;
		}
	}
	return NULL;
}

/*!
 * \brief Find Device by ID
 * \param name Device ID (hostname)
 * \param useRealtime Use RealTime as Boolean
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t * sccp_device_find_byid(const char * name, boolean_t useRealtime)
{
	sccp_device_t * d;

  	SCCP_LIST_LOCK(&GLOB(devices));
  	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strcasecmp(d->id, name)) {
			break;
		}
  	}
  	SCCP_LIST_UNLOCK(&GLOB(devices));

#ifdef CS_SCCP_REALTIME
  	if (!d && useRealtime)
		d = sccp_device_find_realtime_byid(name);
#endif

  	return d;
}

#ifdef CS_SCCP_REALTIME
/*!
 * \brief Find Device via RealTime
 * \param name Device ID (hostname)
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t * sccp_device_find_realtime(const char * name) {
	sccp_device_t *d = NULL;
	struct ast_variable *v, *variable;

	/* No martini, no party ! */
	if(ast_strlen_zero(GLOB(realtimedevicetable)))
		return NULL;

	if ((variable = ast_load_realtime(GLOB(realtimedevicetable), "name", name, NULL))) {
		v = variable;
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Device '%s' found in realtime table '%s'\n", name, GLOB(realtimedevicetable));
		//d = sccp_config_buildDevice(v, name, TRUE);

		d = sccp_device_create();
		sccp_device_applyDefaults(d);
		sccp_copy_string(d->id, name, sizeof(d->id));
		d = sccp_config_applyDeviceConfiguration(d, variable);

#ifdef CS_SCCP_REALTIME
		d->realtime = TRUE;
#endif
		d = sccp_device_addToGlobals(d);
		ast_variables_destroy(v);

		if(!d) {
			ast_log(LOG_ERROR, "SCCP: Unable to build realtime device '%s'\n", name);
		}
		return d;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Device '%s' not found in realtime table '%s'\n", name, GLOB(realtimedevicetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Name
 * \param name Line Name
 * \param realtime Use Realtime as Boolean
 * \return SCCP Line
 */
sccp_line_t * sccp_line_find_byname_wo(const char * name, uint8_t realtime)
{
	sccp_line_t * l = NULL;

	sccp_log(98)(VERBOSE_PREFIX_3 "SCCP: Looking for line '%s'\n", name);

// 	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		if(!strcasecmp(l->name, name)) {
			break;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
// 	SCCP_LIST_UNLOCK(&GLOB(lines));

#ifdef CS_SCCP_REALTIME
	if (!l && realtime)
		l = sccp_line_find_realtime_byname(name);
#endif

	if (!l) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line '%s' not found.\n", name);
		return NULL;
	}

//	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found line '%s'\n", DEV_ID_LOG(l->device), l->name);
	sccp_log(98)(VERBOSE_PREFIX_3 "%s: Found line '%s'\n", "SCCP", l->name);

	return l;
}

#ifdef CS_SCCP_REALTIME
/*!
 * \brief Find Line via Realtime
 * \param name Line Name
 * \return SCCP Line
 */
sccp_line_t * sccp_line_find_realtime_byname(const char * name)
{
	sccp_line_t *l = NULL;
	struct ast_variable *v, *variable;

	/* No martini, no party ! */
	if(ast_strlen_zero(GLOB(realtimelinetable)))
		return NULL;

	if ((variable = ast_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));
		//l = build_lines_wo(v, 1);

		//l = sccp_config_buildLine(v, name, TRUE);
		l = sccp_line_create();
		l = sccp_config_applyLineConfiguration(l, variable);
		sccp_copy_string(l->name, name, sizeof(l->name));
#ifdef CS_SCCP_REALTIME
		l->realtime = TRUE;
#endif
		l = sccp_line_addToGlobals(l);
		ast_variables_destroy(v);


		if(!l) {
			ast_log(LOG_ERROR, "SCCP: Unable to build realtime line '%s'\n", name);
		}
		return l;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line '%s' not found in realtime table '%s'\n", name, GLOB(realtimelinetable));
	return NULL;
}
#endif


/*!
 * \brief Find Line by Instance on device
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 * \todo TODO No ID Specified only instance, should this function be renamed ?
 */
sccp_line_t * sccp_line_find_byid(sccp_device_t * d, uint8_t instance){
	sccp_line_t * l = NULL;
	int i=0;
	sccp_buttonconfig_t	*config;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if(++i == instance) {
			if(config->type == LINE && sccp_is_nonempty_string(config->button.line.name)){
				l = sccp_line_find_byname_wo(config->button.line.name, TRUE);
			}
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	/*
	SCCP_LIST_LOCK(&d->lines);
	SCCP_LIST_TRAVERSE(&d->lines, l, listperdevice) {
		if(l->instance == instance) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->lines);
	*/

	if (!l) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No line found with instance %d.\n", DEV_ID_LOG(d), instance);
		return NULL;
	}

//	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found line %s\n", DEV_ID_LOG(l->device), l->name);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found line %s\n", "SCCP", l->name);

	return l;
}

/*!
 * \brief Find Line by ID
 * \param id ID as int
 * \return SCCP Channel (can be null)
 */
sccp_channel_t * sccp_channel_find_byid(uint32_t id)
{
	sccp_channel_t * c = NULL;
	sccp_line_t * l;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			sccp_log(64)(VERBOSE_PREFIX_3 "Channel %u state: %d\n", c->callid, c->state);
			if (c->callid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%u)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if(c) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));

	return c;
}

/*!
 * We need this to start the correct rtp stream.
 * \brief Find Channel by Pass Through Party ID
 * \param id Party ID
 * \return SCCP Channel - cann bee NULL if no channel with this id was found
 */
sccp_channel_t * sccp_channel_find_bypassthrupartyid(uint32_t id)
{
	sccp_channel_t * c = NULL;
	sccp_line_t * l;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u\n", id);

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%u: Found channel partyID: %u state: %d\n", c->callid, c->passthrupartyid, c->state);
			if (c->passthrupartyid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%u)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if(c) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));

	return c;
}

/*!
 * \brief Find Channel by State on Line
 * \param l SCCP Line
 * \param state State
 * \return SCCP Channel
 */
sccp_channel_t * sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state) {
	sccp_channel_t * c = NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (c && c->callstate == state && c->state != SCCP_CHANNELSTATE_DOWN) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if(c)
			break;
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));

	return c;
}

/*!
 * \brief Find Selected Channel by Device
 * \param d SCCP Device
 * \param c channel
 * \return x SelectedChannel
 */
sccp_selectedchannel_t * sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c) {
	sccp_selectedchannel_t *x = NULL;

	if(!c || !d)
		return NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for selected channel (%d)\n", DEV_ID_LOG(d), c->callid);

	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, x, list) {
		if (x->channel == c) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(d), c->callid);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	return x;
}

/*!
 * \brief Count Selected Channel on Device
 * \param d SCCP Device
 * \return count Number of Selected Channels
 */
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d) {
	sccp_selectedchannel_t *x = NULL;
	uint8_t count = 0;

	if(!d)
		return 0;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Looking for selected channels count\n", DEV_ID_LOG(d));

	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, x, list) {
		count++;
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	return count;
}

/*!
 * \brief Find Channel by CallState on Line
 * \param l SCCP Line
 * \param state State
 * \return SCCP Channel
 */
sccp_channel_t * sccp_channel_find_bycallstate_on_line(sccp_line_t * l, uint8_t state) {
	sccp_channel_t * c = NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (c->callstate == state) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if(c)
			break;
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));

	return c;
}

/*!
 * \brief Find Channel by State on Device
 * \param d SCCP Device
 * \param state State as int
 * \return SCCP Channel
 */
sccp_channel_t * sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state)
{
	sccp_channel_t * c = NULL;
	sccp_line_t * l;
	sccp_buttonconfig_t	*buttonconfig = NULL;
	boolean_t channelFound = FALSE;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	if(!d)
		return NULL;


	sccp_device_lock(d);

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if(buttonconfig->type == LINE ){
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
			if(l){
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, c, list) {
					if (c->callstate == state && c->state != SCCP_CHANNELSTATE_DOWN) {
						sccp_log(10)(VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
						channelFound = TRUE;
						break;
					}
				}
				SCCP_LIST_UNLOCK(&l->channels);
				
				if(channelFound)
					break;
			}
		}
	}
	
	sccp_device_unlock(d);

	return c;
}

/*!
 * \brief Notify asterisk for new state
 * \param c Channel
 * \param state New State - type of AST_STATE_*
 */
void sccp_ast_setstate(sccp_channel_t * c, int state) {
	if (c && c->owner) {
		ast_setstate(c->owner, state);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set asterisk state %s (%d) for call %d\n", DEV_ID_LOG(c->device), ast_state2str(state), state, c->callid);
	}
}

/*!
 * \brief Safe current device state to astDB
 * \param d SCCP Device
 * \todo TODO we shoud do this on feature_state_changed
 */
void sccp_dev_dbput(sccp_device_t * d) {
	sccp_buttonconfig_t	*buttonconfig;
	sccp_line_t * l;
	char tmp[1024] = "", cfwdall[1024] = "", cfwdbusy[1024] = "", cfwdnoanswer[1024] = "";

	if (!d)
		return;
/*
	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if(buttonconfig->type == LINE ){
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
			if(l){
				if (l->cfwd_type == SCCP_CFWD_ALL) {
					snprintf(tmp, sizeof(tmp), "%d:%s;", buttonconfig->instance, l->cfwd_num);
					strncat(cfwdall, tmp, sizeof(cfwdall) - strlen(cfwdall));
				} else if (l->cfwd_type == SCCP_CFWD_BUSY) {
					snprintf(tmp, sizeof(tmp), "%d:%s;", buttonconfig->instance, l->cfwd_num);
					strncat(cfwdbusy, tmp, sizeof(cfwdbusy) - strlen(cfwdbusy));
				} else if (l->cfwd_type == SCCP_CFWD_NOANSWER) {
					snprintf(tmp, sizeof(tmp), "%d:%s;", buttonconfig->instance, l->cfwd_num);
					strncat(cfwdnoanswer, tmp, sizeof(cfwdnoanswer) - strlen(cfwdnoanswer));
				}
			}
		}
	}
	*/
//	SCCP_LIST_LOCK(&d->lines);
//	SCCP_LIST_TRAVERSE(&d->lines, l, listperdevice) {
//		instance = sccp_device_find_index_for_line(d, l->name);
//		if (l->cfwd_type == SCCP_CFWD_ALL) {
//			snprintf(tmp, sizeof(tmp), "%d:%s;", instance, l->cfwd_num);
//			strncat(cfwdall, tmp, sizeof(cfwdall) - strlen(cfwdall));
//		} else if (l->cfwd_type == SCCP_CFWD_BUSY) {
//			snprintf(tmp, sizeof(tmp), "%d:%s;", instance, l->cfwd_num);
//			strncat(cfwdbusy, tmp, sizeof(cfwdbusy) - strlen(cfwdbusy));
//		} else if (l->cfwd_type == SCCP_CFWD_NOANSWER) {
//			snprintf(tmp, sizeof(tmp), "%d:%s;", instance, l->cfwd_num);
//			strncat(cfwdnoanswer, tmp, sizeof(cfwdnoanswer) - strlen(cfwdnoanswer));
//		}
//	}
//	SCCP_LIST_UNLOCK(&d->lines);

	if (!ast_strlen_zero(cfwdall))
		cfwdall[strlen(cfwdall)-1] = '\0';
	if (!ast_strlen_zero(cfwdbusy))
		cfwdbusy[strlen(cfwdbusy)-1] = '\0';

	snprintf(tmp, sizeof(tmp), "dnd=%d,cfwdall=%s,cfwdbusy=%s,cfwdnoanswer=%s", d->dndFeature.status, cfwdall, cfwdbusy, cfwdnoanswer);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Storing device status (dnd, cfwd*) in the asterisk db\n", d->id);
	if (ast_db_put("SCCP", d->id, tmp))
		ast_log(LOG_NOTICE, "%s: Unable to store device status (dnd, cfwd*) in the asterisk db\n", d->id);
}

/*!
 * \brief read device state from astDB
 * \param d SCCP Device
 */
void sccp_dev_dbget(sccp_device_t * d) {
// 	char result[256]="", *tmp, *tmp1, *r;
// 	int i=0;
//
// 	if (!d)
// 		return;
// 	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Restoring device status (dnd, cfwd*) from the asterisk db\n", d->id);
// 	if (ast_db_get("SCCP", d->id, result, sizeof(result))) {
// 		return;
// 	}
// 	r = result;
// 	while ( (tmp = strsep(&r,",")) ) {
// 		tmp1 = strsep(&tmp,"=");
// 		if (tmp1) {
// 			if (!strcasecmp(tmp1, "dnd")) {
// 				if ( (tmp1 = strsep(&tmp,"")) ) {
// 					sscanf(tmp1, "%i", &i);
// 					d->dnd = (i) ? 1 : 0;
// 					sccp_log(10)(VERBOSE_PREFIX_3 "%s: dnd status is: %s\n", d->id, (d->dnd) ? "ON" : "OFF");
// 				}
// 			} else if (!strcasecmp(tmp1, "cfwdall")) {
// 				tmp1 = strsep(&tmp,"");
// 				sccp_cfwd_parse(d, tmp1, SCCP_CFWD_ALL);
// 			} else if (!strcasecmp(tmp1, "cfwdbusy")) {
// 				tmp1 = strsep(&tmp,"");
// 				sccp_cfwd_parse(d, tmp1, SCCP_CFWD_BUSY);
// 			}
// 		}
// 	}
}

/*!
 * \brief Clean Asterisk Database Entries in the "SCCP" Family
 */
void sccp_dev_dbclean() {
	struct ast_db_entry *entry;
	sccp_device_t * d;
	char key[256];

	entry = ast_db_gettree("SCCP", NULL);
	while (entry) {
		sscanf(entry->key,"/SCCP/%s", key);
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Looking for '%s' in the devices list\n", key);
			if ((strlen(key) == 15) && ((strncmp(key, "SEP",3) == 0) || (strncmp(key, "ATA",3)==0))) {

			SCCP_LIST_LOCK(&GLOB(devices));
			SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
				if (!strcasecmp(d->id, key)) {
					break;
				}
			}
			SCCP_LIST_UNLOCK(&GLOB(devices));

			if (!d) {
				ast_db_del("SCCP", key);
				sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: device '%s' removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry)
		ast_db_freetree(entry);
}

/*!
 * \brief Convert Asterisk Extension State to String
 * \param type Extension State
 * \return Extension String or "unknown"
 */
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
	case AST_EXTENSION_RINGING | AST_EXTENSION_INUSE:
		return "RingInUse";
#endif
#ifdef CS_AST_HAS_EXTENSION_ONHOLD
	case AST_EXTENSION_ONHOLD:
		return "OnHold";
	case AST_EXTENSION_ONHOLD | AST_EXTENSION_INUSE:
		return "HoldInUse";
#endif
	default:
		return "Unknown";
	}
}


/*!
 * \brief Convert Message to String
 * \param e Message
 * \return Message String or "unknown"
 */
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
	case 0x0048:
		return "DialedPhoneBookMessage";
	case 0x0049:
		return "AccessoryStatusMessage";

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
		return "unknown";
	}
}

/*!
 * \brief Convert Accessory 2 String
 * \param accessory SCCP_ACCESSORY_* as uint32_t
 * \return Accessory String or "unknown"
 */
const char * skinny_accessory2str(uint32_t accessory) {
	switch(accessory) {
	case SCCP_ACCESSORY_NONE:
		return "None";
	case SCCP_ACCESSORY_HEADSET:
		return "Headset";
	case SCCP_ACCESSORY_HANDSET:
		return "Handset";
	case SCCP_ACCESSORY_SPEAKER:
		return "Speaker";
	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Accessory State 2 String
 * \param state SCCP_ACCESSORYSTATE_* as uint32_t
 * \return Accessroy State String or "unknown"
 */
const char * skinny_accessorystate2str(uint32_t state) {
	switch(state) {
	case SCCP_ACCESSORYSTATE_NONE:
		return "None";
	case SCCP_ACCESSORYSTATE_ONHOOK:
		return "OnHook";
	case SCCP_ACCESSORYSTATE_OFFHOOK:
		return "OffHook";
	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Label 2 String
 * \param label SKINNY_LBL_* as uint8_t
 * \return Label String or "unknown"
 */
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

/*!
 * \brief Convert Tone 2 String
 * \param tone SKINNY_TONE_* as uint8_t
 * \return Tone String or "unknown"
 */
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

/*!
 * \brief Convert Alarm 2 String
 * \param alarm SKINNY_ALARM_* as uint8_t
 * \return Alarm String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert DeviceType 2 string values
 * \param type SKINNY_DEVICETYPE_* as uint32_t
 * \return DeviceType String or "unknown"
 * \todo TODO add values for new devices
 */
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
	case SKINNY_DEVICETYPE_CISCO7902:
		return "Cisco7902";
	case SKINNY_DEVICETYPE_CISCO7905:
		return "Cisco7905";
	case SKINNY_DEVICETYPE_CISCO7906:
		return "Cisco7906";
	case SKINNY_DEVICETYPE_CISCO7911:
		return "Cisco7911";
	case SKINNY_DEVICETYPE_CISCO7912:
		return "Cisco7912";
	case SKINNY_DEVICETYPE_CISCO7931:
		return "Cisco7931";
	case SKINNY_DEVICETYPE_CISCO7921:
		return "Cisco7921";
	case SKINNY_DEVICETYPE_CISCO7935:
		return "Cisco7935";
	case SKINNY_DEVICETYPE_CISCO7936:
		return "Cisco7936";
	case SKINNY_DEVICETYPE_CISCO7937:
		return "Cisco7937";
	case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
		return "Cisco_IP_Communicator";
	case SKINNY_DEVICETYPE_ATA186:
		return "Cisco Ata 186";
	case SKINNY_DEVICETYPE_CISCO7941:
		return "Cisco7941";
	case SKINNY_DEVICETYPE_CISCO7941GE:
		return "Cisco7941GE";
	case SKINNY_DEVICETYPE_CISCO7942:
		return "Cisco7942";
	case SKINNY_DEVICETYPE_CISCO7945:
		return "Cisco7945";
	case SKINNY_DEVICETYPE_CISCO7961:
		return "Cisco7961";
	case SKINNY_DEVICETYPE_CISCO7961GE:
		return "Cisco7961GE";
	case SKINNY_DEVICETYPE_CISCO7962:
		return "Cisco7962";
	case SKINNY_DEVICETYPE_CISCO7965:
		return "Cisco7965";
	case SKINNY_DEVICETYPE_CISCO7970:
		return "Cisco7970";
	case SKINNY_DEVICETYPE_CISCO7971:
		return "Cisco7971";
	case SKINNY_DEVICETYPE_CISCO7975:
		return "Cisco7975";
	case SKINNY_DEVICETYPE_CISCO7985:
		return "Cisco7985";

	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Stimulis 2 String
 * \param type SKINNY_STIMULUS_* as uint8_t
 * \return Stimulus String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert Button Type 2 String
 * \param type SKINNY_BUTTONTYPE_* as uint8_t
 * \return Button Type String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert Lamp Mode 2 String
 * \param type SKINNY_LAMP_* as uint8_t
 * \return Lamp Mode String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert Ringer Mode 2 String
 * \param type SKINNY_STATTION_* as uint8_t
 * \return Ringer Mode String or "unknown"
 */
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
	case SKINNY_STATION_SILENTRING:
			return "Silent";
	case SKINNY_STATION_URGENTRING:
			return "Urgent";
	default:
		return "unknown";
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
	case KEYMODE_INUSEHINT:
		return "InUseHint";
	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Device State 2 String
 * \param type SCCP_DEVICESTATE_* as uint8_t
 * \return Device State String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert Device Registration State 2 String
 * \param type SKINNY_DEVICE_RS_* as uint8_t
 * \return Device Registration State String or "unknown"
 */
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
		return "unknown";
	}
}

/*!
 * \brief Convert Call Type 2 String
 * \param type SKINNY_CALLTYPE_* as uint8_t
 * \return Call Type String or "unknown"
 */
const char * skinny_calltype2str(uint8_t type) {
	switch(type) {
	case SKINNY_CALLTYPE_INBOUND:
		return "Inbound";
	case SKINNY_CALLTYPE_OUTBOUND:
		return "Outbound";
	case SKINNY_CALLTYPE_FORWARD:
		return "Forward";
	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Codec 2 String
 * \param type Codec_* as uint8_t
 * \return Codec String or "unknown"
 */
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
	case 101:
		return "H.263";
	case 103:
		return "H.264";

	default:
		return "unknown";
	}
}

/*!
 * \brief Convert Asterisk Codec 2 Skinny Codec
 * \param fmt Asterisk Codec as type of AST_FORMAT_*
 * \return Skinny Codec
 */
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
#ifndef ASTERISK_CONF_1_2
	case AST_FORMAT_G726_AAL2:
		return 82;
#endif
	case AST_FORMAT_H261:
		return 100;
	case AST_FORMAT_H263:
		return 101;
	case AST_FORMAT_G722:
	        return 6;
	default:
		return 0;
	}
}


/*!
 * \version 20090708
 *  \author Federico
 */
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
		case 80:
			return AST_FORMAT_GSM;
#ifndef ASTERISK_CONF_1_2
		case 6:
			return AST_FORMAT_G722;
		case 82:
			return AST_FORMAT_G726_AAL2;
		case 103:
			return AST_FORMAT_H264;
#endif
#ifdef ASTERISK_CONF_1_6
/*
		case 25:
			return AST_FORMAT_CISCO_WIDEBAND;
 */
#endif
		case 100:
			return AST_FORMAT_H261;
		case 101:
			return AST_FORMAT_H263;
		case 102:
			return AST_FORMAT_H263_PLUS;
		default:
			return 0;
	}
}

/*!
 * \brief Convert Do Not Disturb 2 String
 * \param type DND as uint8_t
 * \return DND String or "unknown"
 */
const char * sccp_dndmode2str(uint8_t type) {
	switch(type) {
	case SCCP_DNDMODE_OFF:
		return "Off";
	case SCCP_DNDMODE_REJECT:
		return "Reject";
	case SCCP_DNDMODE_SILENT:
		return "Silent";
	default:
		return "unknown";
	}
}

#ifndef CS_AST_HAS_STRINGS
/*!
 * \brief Asterisk Skip Blanks
 * \param str as Character
 * \return String without Blanks
 */
char *ast_skip_blanks(char *str) {
        while (*str && *str < 33)
                str++;

        return str;
}

/*!
 * \brief Asterisk Trim Blanks
 * Remove Blanks from the begining and end of a string
 * \param str as Character
 * \return String without Beginning or Ending Blanks
 */
char *ast_trim_blanks(char *str) {
	char *work = str;
	if (work) {
		work += strlen(work) - 1;
		while ((work >= str) && *work < 33)
			*(work--) = '\0';
	}
	return str;
}

/*!
 * \brief Asterisk Non Blanks
 * \param str as Character
 * \return Only the Non Blank Characters
 */
char *ast_skip_nonblanks(char *str) {
        while (*str && *str > 32)
                str++;

        return str;
}

/*!
 * \brief Asterisk Strip
 * \param s as Character
 * \return String without all Blanks
 */
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

/*!
 * \brief get the SoftKeyIndex for a given SoftKeyLabel on specified keymode
 *
 *
 * \todo implement function for finding index of given SoftKey
 */
int sccp_softkeyindex_find_label(sccp_device_t * d, unsigned int keymode, unsigned int softkey) {

	return -1;
}

/*!
 * \brief This is used on device reconnect attempt
 * \param s_addr IP Address as unsigned long
 * \return SCCP Device
 */
sccp_device_t * sccp_device_find_byipaddress(unsigned long s_addr){
	sccp_device_t * d;

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
		if (d->session && d->session->sin.sin_addr.s_addr == s_addr) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));
	return d;
}


#if ASTERISK_VERSION_NUM >= 10600
#ifdef HAVE_ASTERISK_DEVICESTATE_H
/*!
 * \brief map states from sccp to ast_device_state
 * \param state SCCP Channel State
 * \return asterisk device state
 */
enum ast_device_state sccp_channelState2AstDeviceState(sccp_channelState_t state){
	switch(state){
	  case SCCP_CHANNELSTATE_ONHOOK:
	  case SCCP_CHANNELSTATE_DOWN:
		return AST_DEVICE_NOT_INUSE;
	  break;
	  case SCCP_CHANNELSTATE_OFFHOOK:
		  return AST_DEVICE_INUSE;
	  break;
	  case SCCP_CHANNELSTATE_CONGESTION:
		  return AST_DEVICE_UNAVAILABLE;
	  break;
	  case SCCP_CHANNELSTATE_DND:
		  return AST_DEVICE_UNAVAILABLE;
	  break;
	  default:
		  return AST_DEVICE_UNKNOWN;
	  break;
	}
}
#endif
#endif

/*!
 * \brief convert Feature String 2 Feature ID
 * \param str Feature Str as char
 * \return Feature Type
 */
sccp_feature_type_t sccp_featureStr2featureID(char *str){
	if(!str)
		return SCCP_FEATURE_UNKNOWN;
	if(!strcasecmp(str, "cfwdall")){
		return SCCP_FEATURE_CFWDALL;
	}else if(!strcasecmp(str, "privacy")){
		return SCCP_FEATURE_PRIVACY;
	}else if(!strcasecmp(str, "dnd")){
		return SCCP_FEATURE_DND;
	}else if(!strcasecmp(str, "monitor")){
		return SCCP_FEATURE_MONITOR;
	}else if(!strcasecmp(str, "test1")){
		return SCCP_FEATURE_TEST1;
	}else if(!strcasecmp(str, "test2")){
		return SCCP_FEATURE_TEST2;
	}else if(!strcasecmp(str, "test3")){
		return SCCP_FEATURE_TEST3;
	}else if(!strcasecmp(str, "test4")){
		return SCCP_FEATURE_TEST4;
	}else if(!strcasecmp(str, "test5")){
		return SCCP_FEATURE_TEST5;
	}else if(!strcasecmp(str, "test6")){
		return SCCP_FEATURE_TEST6;
	}else if(!strcasecmp(str, "test7")){
		return SCCP_FEATURE_TEST7;
	}else if(!strcasecmp(str, "test8")){
		return SCCP_FEATURE_TEST8;
	}else if(!strcasecmp(str, "test9")){
		return SCCP_FEATURE_TEST9;
	}else if(!strcasecmp(str, "testA")){
		return SCCP_FEATURE_TESTA;
	}else if(!strcasecmp(str, "testB")){
		return SCCP_FEATURE_TESTB;
	}else if(!strcasecmp(str, "testC")){
		return SCCP_FEATURE_TESTC;
	}else if(!strcasecmp(str, "testD")){
		return SCCP_FEATURE_TESTD;
	}else if(!strcasecmp(str, "testE")){
		return SCCP_FEATURE_TESTE;
	}else if(!strcasecmp(str, "testF")){
		return SCCP_FEATURE_TESTF;
	}else if(!strcasecmp(str, "testG")){
		return SCCP_FEATURE_TESTG;
	}else if(!strcasecmp(str, "testH")){
		return SCCP_FEATURE_TESTH;
	}else if(!strcasecmp(str, "testI")){
		return SCCP_FEATURE_TESTI;
	}else if(!strcasecmp(str, "testJ")){
		return SCCP_FEATURE_TESTJ;
	}


	return SCCP_FEATURE_UNKNOWN;
}

void sccp_util_handleFeatureChangeEvent(const sccp_event_t **event){
	char family[25];
	char cfwdLineStore[60];
	sccp_buttonconfig_t	*config;
	sccp_line_t		*line;
	sccp_linedevices_t 	*lineDevice;
	sccp_device_t *device = (*event)->event.featureChanged.device;


	if(!(*event) || !device )
		return;

	sccp_device_lock(device);
	sprintf(family, "SCCP/%s", device->id);
	sccp_device_unlock(device);

	switch((*event)->event.featureChanged.featureType) {
		case SCCP_FEATURE_CFWDALL:
			
			SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list){
				if(config->type == LINE ){
					line = sccp_line_find_byname_wo(config->button.line.name,FALSE);
					if(line){
						SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list){
							if(lineDevice->device != device)
								continue;
							
							sprintf(cfwdLineStore, "%s/%s", family,config->button.line.name);
							if(lineDevice->cfwdAll.enabled)
								ast_db_put(cfwdLineStore, "cfwdAll", lineDevice->cfwdAll.number);
							else
								ast_db_del(cfwdLineStore, "cfwdAll");
						}
					}
				}

			}
		  
			break;
		case SCCP_FEATURE_CFWDBUSY:
		  	break;
		case SCCP_FEATURE_DND:
			if(!device->dndFeature.status){
				ast_db_del(family, "dnd");
				//sccp_log(1)(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "dnd");
			}else{
				if(device->dndFeature.status == SCCP_DNDMODE_SILENT)
					ast_db_put(family, "dnd", "silent");
				else
					ast_db_put(family, "dnd", "reject");
			}
		  	break;
		case SCCP_FEATURE_PRIVACY:
			if(!device->privacyFeature.status){
				ast_db_del(family, "privacy");
				//sccp_log(1)(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "privacy");
			}else{
				char data[256];
				sprintf(data, "%d", device->privacyFeature.status );
				ast_db_put(family, "privacy", data);
			}
		  	break;
		case SCCP_FEATURE_MONITOR:
			if(!device->monitorFeature.status){
				ast_db_del(family, "monitor");
				//sccp_log(1)(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "monitor");
			}else{
				ast_db_put(family, "monitor", "on");
			}
			break;
		default:
			return;
	}

}

boolean_t sccp_util_matchSubscriberID(const sccp_channel_t *channel, const char *subscriberID){
	if(strlen(channel->subscriptionId.number) || !subscriberID)
		return TRUE;
	
	if( !strncasecmp(channel->subscriptionId.number, subscriberID, strlen(channel->subscriptionId.number)) )
		return FALSE;
	
	return TRUE;	
}
