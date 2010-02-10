/*!
 * \file 	sccp_utils.c
 * \brief 	SCCP Utils Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \date	$Date$
 * \version 	$Revision$
 */

#include "config.h"
#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_config.h"
#include <assert.h>
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

	if(ast_strlen_zero(GLOB(realtimelinetable)))
		return NULL;

	if ((variable = ast_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));

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
 * \brief Convert SCCP Types to String
 * \param type 	SCCP Type
 * \param value SCCP Value
 * \return Converted String
 */
const char * sccp2str(uint8_t type, uint32_t value) {
	switch(type) {
                case SCCP_MESSAGE:
                        _ARR2STR(sccp_messagetypes, type , value , text);
                case SCCP_ACCESSORY:
                        _ARR2STR(sccp_accessories, accessory , value , text);
                case SCCP_ACCESSORY_STATE:
                        _ARR2STR(sccp_accessory_states, accessory_state , value , text);
                case SCCP_EXTENSION_STATE:
                        _ARR2STR(sccp_extension_states, extension_state , value , text);
                case SCCP_DNDMODE:
                        _ARR2STR(sccp_dndmodes, dndmode , value , text);
                default:
                        return "Not Defined";
        }  
}


/*!
 * \brief Convert Skinny Types 2 String
 * \param type 	Skinny Type
 * \param value Skinny Value
 * \return Converted String
 */
const char * skinny2str(uint8_t type, uint32_t value) {
	switch(type) {
                case SKINNY_TONE:
                        _ARR2STR(skinny_tones, tone , value , text);
                case SKINNY_ALARM:
                        _ARR2STR(skinny_alarms, alarm, value , text);
                case SKINNY_DEVICETYPE:
                        _ARR2STR(skinny_devicetypes, devicetype, value  , text);
                case SKINNY_STIMULUS:
                        _ARR2STR(skinny_stimuluses, stimulus , value, text);
                case SKINNY_BUTTONTYPE:
                        _ARR2STR(skinny_buttontypes, buttontype , value, text);
                case SKINNY_LAMP:
                        _ARR2STR(skinny_lampmodes, lampmode , value, text);
                case SKINNY_STATION:
                        _ARR2STR(skinny_stations, station , value, text);
                case SKINNY_LBL:
                        _ARR2STR(skinny_labels, label, value, text);
                case SKINNY_CALLTYPE:
                        _ARR2STR(skinny_calltypes, calltype , value, text);
                case SKINNY_KEYMODE:
                        _ARR2STR(skinny_keymodes, keymode , value, text);
                case SKINNY_DEVICE_RS:
                        _ARR2STR(skinny_device_registrationstates, device_registrationstate , value, text);
                case SKINNY_DEVICE_STATE:
                        _ARR2STR(skinny_device_states, device_state , value, text);
                case SKINNY_CODEC:
                        _ARR2STR(skinny_codecs, codec , value, text);
                default:
                        return "not defined yes";
        }
}

/*!
 * \brief Convert Asterisk Codec 2 Skinny Codec
 * \param fmt Asterisk Codec as type of AST_FORMAT_*
 * \return Skinny Codec
 */
uint8_t sccp_codec_ast2skinny(int fmt) {
/*	switch(fmt) {
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
	}*/
	int i;
	for (i = 1; i < ARRAY_LEN(skinny_codecs); i++) {
	        if (skinny_codecs[i].astcodec == fmt) {
	                return skinny_codecs[i].codec;
	        }
	}
	return 0;
}


/*!
 * \version 20090708
 *  \author Federico
 */
int sccp_codec_skinny2ast(uint8_t fmt) {
/*
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
//		case 25:
//			return AST_FORMAT_CISCO_WIDEBAND;
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
*/
	int i;
	for (i = 1; i < ARRAY_LEN(skinny_codecs); i++) {
	        if (skinny_codecs[i].codec == fmt) {
	                return skinny_codecs[i].astcodec;
	        }
	}
	return 0;
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

struct composedId sccp_parseComposedId(const char* labelString, unsigned int maxLength)
{
	const char *stringIterator = 0;
	int i = 0;
	boolean_t endDetected = FALSE;
	int state = 0;
	struct composedId id;

	assert(0 != labelString);

	memset (&id, 0, sizeof(id));
		

	for (stringIterator = labelString; stringIterator < labelString+maxLength && !endDetected; stringIterator++)
	{
		switch (state)
		{
			case 0: // parsing of main id
				assert (i < sizeof(id.mainId));
				switch (*stringIterator) {
					case '\0':
						endDetected = TRUE;
						id.mainId[i] = '\0';
						break;
					case '@':
						id.mainId[i] = '\0';
						i = 0;
						state = 1;
						break;
					default:
						id.mainId[i] = *stringIterator;
						i++;
						break;
				}
				break;
						
			case 1: // parsing of sub id number
				assert (i < sizeof(id.subscriptionId.number));
				switch (*stringIterator) {
					case '\0':
						endDetected = TRUE;
						id.subscriptionId.number[i] = '\0';
						break;
					case ':':
						id.subscriptionId.number[i] = '\0';
						i = 0;
						state = 2;
						break;
					default:
						id.subscriptionId.number[i] = *stringIterator;
						i++;
						break;
				}
				break;
				
			case 2: // parsing of sub id name
				assert (i < sizeof(id.subscriptionId.name));
				switch (*stringIterator) {
					case '\0':
						endDetected = TRUE;
						id.subscriptionId.name[i] = '\0';
						break;
					default:
						id.subscriptionId.name[i] = *stringIterator;
						i++;
						break;
				}
				break;

			default:
				assert(FALSE);
		}
	}
	return id;
}

boolean_t sccp_util_matchSubscriptionId(const sccp_channel_t *channel, const char *subscriptionIdNum){
	boolean_t result = TRUE;
	int compareId = 0;
	int compareDefId = 0;

	if(NULL != subscriptionIdNum) {
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: channel->subscriptionId.number=%s, SubscriptionId=%s, NULL!=channel->line %s\n", (channel->subscriptionId.number)?channel->subscriptionId.number:"NULL", (subscriptionIdNum)?subscriptionIdNum:"NULL", (NULL != channel->line)?"TRUE":"FALSE");
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: channel->line->defaultSubscriptionId.number=%s\n", (channel->line->defaultSubscriptionId.number)?channel->line->defaultSubscriptionId.number:"NULL");
	} else {
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: null subscriptionIdNum!\n");
	}
  
	if(    NULL != subscriptionIdNum 
		&& 0    != strlen(subscriptionIdNum)
	    && NULL != channel->line 
	    && 0    != (compareDefId = strncasecmp(channel->subscriptionId.number, 
					   channel->line->defaultSubscriptionId.number,
					   strlen(channel->subscriptionId.number)))  ) {
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: outer if: compare = %d\n", compareDefId);
			if( 0 != strncasecmp(channel->subscriptionId.number,
				subscriptionIdNum, 
				strlen(channel->subscriptionId.number)) 
				&& 0 != (compareId = strlen(channel->subscriptionId.number)) ) {
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: inner if: compare = %d\n", compareId);
					result = FALSE;
			}
	}
	
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId:  result: %s\n", (result)?"TRUE":"FALSE");
	
	return result;	
}
