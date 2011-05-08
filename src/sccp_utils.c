
/*!
 * \file 	sccp_utils.c
 * \brief 	SCCP Utils Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note		Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

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
void sccp_dump_packet(unsigned char *messagebuffer, int len)
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

	for (i = 0; i < rows; i++) {
		memset(row, 0, sizeof(row));
		sprintf(row, "%08X - ", cur);
		if ((i == rows - 1) && (len % res > 0))				// FIXED after 354 -FS
			cols = len % res;

		memset(temp2, 0, sizeof(temp2));
		for (t = 0; t < cols; t++) {
			memset(temp, 0, sizeof(temp));
			sprintf(temp, "%02X ", messagebuffer[cur]);
			strcat(row, temp);
			if (isPrintableChar((char)messagebuffer[cur]))
				sprintf(temp, "%c", messagebuffer[cur]);
			else
				sprintf(temp, ".");
			strcat(temp2, temp);
			cur++;
		}

		if (cols < res) {
			for (t = 0; t < res - cols; t++) {
				strcat(row, "   ");
			}
		}
		strcat(row, temp2);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%s\n", row);
	}
}

/*!
 * \brief Add Host to the Permithost Linked List
 * \param d SCCP Device
 * \param config_string as Character
 * 
 * \warning
 * 	- device->permithosts is not always locked
 */
void sccp_permithost_addnew(sccp_device_t * d, const char *config_string)
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
 * 
 * \warning
 * 	- device->addons is not always locked
 */
void sccp_addon_addnew(sccp_device_t * d, const char *addon_config_type)
{
	int addon_type;

	// check for device
	if (!d)
		return;

	// checking if devicetype is specified
	if (sccp_strlen_zero(d->config_type)) {
		sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "SCCP: Addon type (%s) must be specified after device type\n", addon_config_type);
		return;
	}

	if (!strcasecmp(addon_config_type, "7914"))
		addon_type = SKINNY_DEVICETYPE_CISCO7914;
	else if (!strcasecmp(addon_config_type, "7915"))
		addon_type = SKINNY_DEVICETYPE_CISCO7915;
	else if (!strcasecmp(addon_config_type, "7916"))
		addon_type = SKINNY_DEVICETYPE_CISCO7916;
	else {
		sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "SCCP: Unknown addon type (%s) for device (%s)\n", addon_config_type, d->config_type);
		return;
	}

	if (!((addon_type == SKINNY_DEVICETYPE_CISCO7914) &&
	      ((!strcasecmp(d->config_type, "7960")) ||
	       (!strcasecmp(d->config_type, "7961")) ||
	       (!strcasecmp(d->config_type, "7962")) ||
	       (!strcasecmp(d->config_type, "7965")) ||
	       (!strcasecmp(d->config_type, "7970")) || (!strcasecmp(d->config_type, "7971")) || (!strcasecmp(d->config_type, "7975")))) && !((addon_type == SKINNY_DEVICETYPE_CISCO7915) && ((!strcasecmp(d->config_type, "7962")) || (!strcasecmp(d->config_type, "7965")) || (!strcasecmp(d->config_type, "7975")))) && !((addon_type == SKINNY_DEVICETYPE_CISCO7916) && ((!strcasecmp(d->config_type, "7962")) || (!strcasecmp(d->config_type, "7965")) || (!strcasecmp(d->config_type, "7975"))))) {
		sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "SCCP: Configured device (%s) does not support the specified addon type (%s)\n", d->config_type, addon_config_type);
		return;
	}

	sccp_addon_t *a = ast_malloc(sizeof(sccp_addon_t));

	if (!a) {
		sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "SCCP: Unable to allocate memory for a device addon\n");
		return;
	}
	memset(a, 0, sizeof(sccp_addon_t));

	a->type = addon_type;
	a->device = d;

	SCCP_LIST_INSERT_HEAD(&d->addons, a, list);

	sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "%s: Added addon (%d) taps on device type (%s)\n", (d->id ? d->id : "SCCP"), a->type, d->config_type);
}

/*!
 * \brief Return Number of Buttons on AddOn Device
 * \param d SCCP Device
 * \return taps (Number of Buttons on AddOn Device)
 * 
 * \lock
 * 	- device->addons
 */
int sccp_addons_taps(sccp_device_t * d)
{
	sccp_addon_t *cur = NULL;

	int taps = 0;

	if (!strcasecmp(d->config_type, "7914"))
		return 28;							// on compatibility mode it returns 28 taps for a double 7914 addon

	SCCP_LIST_LOCK(&d->addons);
	SCCP_LIST_TRAVERSE(&d->addons, cur, list) {
		if (cur->type == SKINNY_DEVICETYPE_CISCO7914)
			taps += 14;
		if (cur->type == SKINNY_DEVICETYPE_CISCO7915 || cur->type == SKINNY_DEVICETYPE_CISCO7916)
			taps += 24;
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found (%d) taps on device addon (%d)\n", (d->id ? d->id : "SCCP"), taps, cur->type);
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
	if (!d)
		return;

	while ((AST_LIST_REMOVE_HEAD(&d->addons, list))) ;
}

/*!
 * \brief Return AddOn Linked List
 * \param d SCCP Device
 * \return addons_list
 */
char *sccp_addons_list(sccp_device_t * d)
{
	char *addons_list = NULL;

	return addons_list;
}

/*!
 * \brief Put SCCP into Safe Sleep for [num] milli_seconds
 * \param ms MilliSeconds
 */
void sccp_safe_sleep(int ms)
{
	struct timeval start = { 0, 0 };

	start = ast_tvnow();
	usleep(1);
	while (ast_tvdiff_ms(ast_tvnow(), start) < ms) {
		usleep(1);
	}
}

/*!
 * \brief Create an asterisk variable
 * \param buf Variable Definition in format "[var]=[value]"
 */
struct ast_variable *sccp_create_variable(const char *buf)
{
	struct ast_variable *tmpvar = NULL;

	char *varname = sccp_strdupa(buf), *varval = NULL;

	if ((varval = strchr(varname, '='))) {
		*varval++ = '\0';
#if ASTERISK_VERSION_NUM >= 10600
		if ((tmpvar = ast_variable_new(varname, varval, ""))) {
#else
		if ((tmpvar = ast_variable_new(varname, varval))) {
#endif
			return tmpvar;
		}
	}
	return NULL;
}

/*!
 * \brief Retrieve the SCCP Channel from an Asterisk Channel
 * \param ast_chan Asterisk Channel
 * \return SCCP Channel on Success or Null on Fail
 */
sccp_channel_t *get_sccp_channel_from_ast_channel(struct ast_channel * ast_chan)
{
#ifndef CS_AST_HAS_TECH_PVT
	if (!strncasecmp(ast_chan->type, "SCCP", 4)) {
#else
	if (!strncasecmp(ast_chan->tech->type, "SCCP", 4)) {
#endif
		return CS_AST_CHANNEL_PVT(ast_chan);
	} else {
		return NULL;
	}
}

/*!
 * \brief Find Device by ID
 * \param name Device ID (hostname)
 * \param useRealtime Use RealTime as Boolean
 * \return SCCP Device - can bee null if device is not found
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- devices
 */
sccp_device_t *sccp_device_find_byid(const char *name, boolean_t useRealtime)
{
	sccp_device_t *d;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strcasecmp(d->id, name)) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

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
 *
 * \callgraph
 * \callergraph
 */
sccp_device_t *sccp_device_find_realtime(const char *name)
{
	sccp_device_t *d = NULL;

	struct ast_variable *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimedevicetable)))
		return NULL;

	if ((variable = ast_load_realtime(GLOB(realtimedevicetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' found in realtime table '%s'\n", name, GLOB(realtimedevicetable));

		d = sccp_device_create();
		if (!d) {
			ast_log(LOG_ERROR, "SCCP: Unable to build realtime device '%s'\n", name);
			return NULL;
		}

		sccp_copy_string(d->id, name, sizeof(d->id));
		d->realtime = TRUE;
		d = sccp_config_applyDeviceConfiguration(d, variable);
		d = sccp_device_addToGlobals(d);
		ast_variables_destroy(v);

		return d;
	}

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' not found in realtime table '%s'\n", name, GLOB(realtimedevicetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Name
 * \param name Line Name
 * \param realtime Use Realtime as Boolean
 * \return SCCP Line
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 */
sccp_line_t *sccp_line_find_byname_wo(const char *name, uint8_t realtime)
{
	sccp_line_t *l = NULL;

	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "SCCP: Looking for line '%s'\n", name);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strcasecmp(l->name, name)) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

#ifdef CS_SCCP_REALTIME
	if (!l && realtime)
		l = sccp_line_find_realtime_byname(name);
#endif

	if (!l) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found.\n", name);
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found line '%s'\n", "SCCP", l->name);

	return l;
}

#ifdef CS_SCCP_REALTIME

/*!
 * \brief Find Line via Realtime
 * \param name Line Name
 * \return SCCP Line
 *
 * \callgraph
 * \callergraph
 */
sccp_line_t *sccp_line_find_realtime_byname(const char *name)
{
	sccp_line_t *l = NULL;

	struct ast_variable *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimelinetable)))
		return NULL;

	if ((variable = ast_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));

		ast_log(LOG_NOTICE, "SCCP: creating realtime line '%s'\n", name);
		l = sccp_line_create();
		sccp_config_applyLineConfiguration(l, variable);

		sccp_copy_string(l->name, name, sizeof(l->name));
		l->realtime = TRUE;
		l = sccp_line_addToGlobals(l);

		ast_variables_destroy(v);

		if (!l) {
			ast_log(LOG_ERROR, "SCCP: Unable to build realtime line '%s'\n", name);
		}
		return l;
	}

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found in realtime table '%s'\n", name, GLOB(realtimelinetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Instance on device
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 *
 * \todo TODO No ID Specified only instance, should this function be renamed ?
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->buttonconfig
 * 	  - see sccp_line_find_byname_wo()
 */
sccp_line_t *sccp_line_find_byid(sccp_device_t * d, uint16_t instance)
{
	sccp_line_t *l = NULL;

	sccp_buttonconfig_t *config;

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);

	if (instance == 0)
		return NULL;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: button instance %d, type: %d\n", DEV_ID_LOG(d), config->instance, config->type);

		if (config->type == LINE && config->instance == instance && sccp_is_nonempty_string(config->button.line.name)) {
			l = sccp_line_find_byname_wo(config->button.line.name, TRUE);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	if (!l) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No line found with instance %d.\n", DEV_ID_LOG(d), instance);
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found line %s\n", "SCCP", l->name);

	return l;
}

/*!
 * \brief Find Line by ID
 * \param id ID as int
 * \return *locked* SCCP Channel (can be null)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_channel_find_byid_locked(uint32_t id)
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Channel %u state: %d\n", c->callid, c->state);
			if (c->callid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%u)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);

		if (c)
			break;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (c)
		sccp_channel_lock(c);

	return c;
}


/*!
 * \brief Find Channel by ID, using a specific line
 * \param id channel ID as int
 * \return *locked* SCCP Channel (can be null)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_find_channel_on_line_byid_locked(sccp_line_t *l, uint32_t id){
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);


	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, c, list) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Channel %u state: %d\n", c->callid, c->state);
		if (c->callid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%u)\n", DEV_ID_LOG(c->device), c->callid);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&l->channels);




	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * We need this to start the correct rtp stream.
 * \brief Find Channel by Pass Through Party ID
 * \param id Party ID
 * \return *locked* SCCP Channel - cann bee NULL if no channel with this id was found
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_channel_find_bypassthrupartyid_locked(uint32_t id)
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u\n", id);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%u: Found channel partyID: %u state: %d\n", c->callid, c->passthrupartyid, c->state);
			if (c->passthrupartyid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%u)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (c) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * \brief Find Channel by State on Line
 * \param l SCCP Line
 * \param state State
 * \return *no locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 */
sccp_channel_t *sccp_channel_find_bystate_on_line_nolock(sccp_line_t * l, uint8_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (c && c->state == state) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (c)
			break;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return c;
}

/*!
 * \brief Find Channel by State on Line
 * \param l SCCP Line
 * \param state State
 * \return *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_line_locked(sccp_line_t * l, uint8_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (c && c->state == state) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (c)
			break;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * \brief Find Selected Channel by Device
 * \param d SCCP Device
 * \param c channel
 * \return x SelectedChannel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device->selectedChannels
 */
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c)
{
	sccp_selectedchannel_t *x = NULL;

	if (!c || !d)
		return NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channel (%d)\n", DEV_ID_LOG(d), c->callid);

	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, x, list) {
		if (x->channel == c) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(d), c->callid);
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
 * 
 * \lock
 * 	- device->selectedChannels
 */
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d)
{
	sccp_selectedchannel_t *x = NULL;

	uint8_t count = 0;

	if (!d)
		return 0;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channels count\n", DEV_ID_LOG(d));

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
 * \return *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_channel_find_bycallstate_on_line_locked(sccp_line_t * l, uint8_t state)
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (c->state == state) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(c->device), c->callid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (c)
			break;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * \brief Yields string representation from channel (for debug).
 * \param c SCCP channel
 * \return string constant (on the heap!)
 */
const char *sccp_channel_toString(sccp_channel_t *c)
{
	static char buf[256] = { '\0' };

	if(c && c->line && c->line->name)
		snprintf(buf, 255, "%s-%08x", c->line->name, c->callid);

	return (const char*) buf;
}

/*!
 * \brief Find Channel by State on Device
 * \param d SCCP Device
 * \param state State as int
 * \return *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 * 
 * \lock
 * 	- device
 * 	  - see sccp_line_find_byname_wo()
 * 	  - line->channels
 * 	- channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_device_locked(sccp_device_t * d, uint8_t state)
{
	sccp_channel_t *c = NULL;

	sccp_line_t *l;

	sccp_buttonconfig_t *buttonconfig = NULL;

	boolean_t channelFound = FALSE;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	if (!d)
		return NULL;

	sccp_device_lock(d);

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
			if (l) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, c, list) {
					if (c->state == state) {

						/* check that subscriptionId matches */
						if (sccp_util_matchSubscriptionId(c, buttonconfig->button.line.subscriptionId.number)) {
							sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(d), c->callid);
							channelFound = TRUE;
							break;
						} else {
							sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found channel (%d), but it does not match subscriptionId %s \n", DEV_ID_LOG(d), c->callid, buttonconfig->button.line.subscriptionId.number);
						}

					}
				}
				SCCP_LIST_UNLOCK(&l->channels);

				if (channelFound)
					break;
			}
		}
	}
	sccp_device_unlock(d);

	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * \brief Notify asterisk for new state
 * \param c Channel
 * \param state New State - type of AST_STATE_*
 */
void sccp_ast_setstate(const sccp_channel_t * c, int state)
{
	if (c && c->owner) {
		ast_setstate(c->owner, state);
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Set asterisk state %s (%d) for call %d\n", DEV_ID_LOG(c->device), ast_state2str(state), state, c->callid);
	}
}

/*!
 * \brief Safe current device state to astDB
 * \param d SCCP Device
 */
void sccp_dev_dbput(sccp_device_t * d)
{
	char tmp[1024] = "", cfwdall[1024] = "", cfwdbusy[1024] = "", cfwdnoanswer[1024] = "";

	if (!d)
		return;

	if (!sccp_strlen_zero(cfwdall))
		cfwdall[strlen(cfwdall) - 1] = '\0';
	if (!sccp_strlen_zero(cfwdbusy))
		cfwdbusy[strlen(cfwdbusy) - 1] = '\0';

	snprintf(tmp, sizeof(tmp), "dnd=%d,cfwdall=%s,cfwdbusy=%s,cfwdnoanswer=%s", d->dndFeature.status, cfwdall, cfwdbusy, cfwdnoanswer);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "%s: Storing device status (dnd, cfwd*) in the asterisk db\n", d->id);
	if (pbx_db_put("SCCP", d->id, tmp))
		ast_log(LOG_NOTICE, "%s: Unable to store device status (dnd, cfwd*) in the asterisk db\n", d->id);
}

/*!
 * \brief read device state from astDB
 * \param d SCCP Device
 *
 * \todo Reimplement/Remove code below
 */
void sccp_dev_dbget(sccp_device_t * d)
{
//      char result[256]="", *tmp, *tmp1, *r;
//      int i=0;
//
//      if (!d)
//              return;
//      sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME))(VERBOSE_PREFIX_3 "%s: Restoring device status (dnd, cfwd*) from the asterisk db\n", d->id);
//      if (pbx_db_get("SCCP", d->id, result, sizeof(result))) {
//              return;
//      }
//      r = result;
//      while ( (tmp = strsep(&r,",")) ) {
//              tmp1 = strsep(&tmp,"=");
//              if (tmp1) {
//                      if (!strcasecmp(tmp1, "dnd")) {
//                              if ( (tmp1 = strsep(&tmp,"")) ) {
//                                      sscanf(tmp1, "%i", &i);
//                                      d->dnd = (i) ? 1 : 0;
//                                      sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME))(VERBOSE_PREFIX_3 "%s: dnd status is: %s\n", d->id, (d->dnd) ? "ON" : "OFF");
//                              }
//                      } else if (!strcasecmp(tmp1, "cfwdall")) {
//                              tmp1 = strsep(&tmp,"");
//                              sccp_cfwd_parse(d, tmp1, SCCP_CFWD_ALL);
//                      } else if (!strcasecmp(tmp1, "cfwdbusy")) {
//                              tmp1 = strsep(&tmp,"");
//                              sccp_cfwd_parse(d, tmp1, SCCP_CFWD_BUSY);
//                      }
//              }
//      }
}

/*!
 * \brief Clean Asterisk Database Entries in the "SCCP" Family
 * 
 * \lock
 * 	- devices
 */
void sccp_dev_dbclean()
{
	struct ast_db_entry *entry;

	sccp_device_t *d;

	char key[256];

	entry = pbx_db_gettree("SCCP", NULL);
	while (entry) {
		sscanf(entry->key, "/SCCP/%s", key);
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Looking for '%s' in the devices list\n", key);
		if ((strlen(key) == 15) && (!strncmp(key, "SEP", 3) || !strncmp(key, "ATA", 3) || !strncmp(key, "VGC", 3) || !strncmp(key, "SKIGW", 5))) {

			SCCP_RWLIST_RDLOCK(&GLOB(devices));
			SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
				if (!strcasecmp(d->id, key)) {
					break;
				}
			}
			SCCP_RWLIST_UNLOCK(&GLOB(devices));

			if (!d) {
				ast_db_del("SCCP", key);
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: device '%s' removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry)
		ast_db_freetree(entry);
}

const char *message2str(uint32_t value)
{
	_ARR2STR(sccp_messagetypes, type, value, text);
}

const char *channelstate2str(uint32_t value)
{
	_ARR2STR(sccp_channelstates, channelstate, value, text);
}

const char *astdevicestate2str(uint32_t value)
{
	_ARR2STR(ast_devicestates, devicestate, value, text);
}

const char *accessory2str(uint32_t value)
{
	_ARR2STR(sccp_accessories, accessory, value, text);
}

const char *accessorystatus2str(uint32_t value)
{
	_ARR2STR(sccp_accessory_states, accessory_state, value, text);
}

const char *extensionstatus2str(uint32_t value)
{
	_ARR2STR(sccp_extension_states, extension_state, value, text);
}

const char *dndmode2str(uint32_t value)
{
	_ARR2STR(sccp_dndmodes, dndmode, value, text);
}

const char *sccp_buttontype2str(uint32_t value)
{
	_ARR2STR(sccp_buttontypes, buttontype, value, text);
}

const char *callforward2str(uint32_t value)
{
	_ARR2STR(sccp_callforwardstates, callforwardstate, value, text);
}

const char *callforward2longstr(uint32_t value)
{
	_ARR2STR(sccp_callforwardstates, callforwardstate, value, longtext);
}

const char *tone2str(uint32_t value)
{
	_ARR2STR(skinny_tones, tone, value, text);
}

const char *alarm2str(uint32_t value)
{
	_ARR2STR(skinny_alarms, alarm, value, text);
}

const char *devicetype2str(uint32_t value)
{
	_ARR2STR(skinny_devicetypes, devicetype, value, text);
}

const char *stimulus2str(uint32_t value)
{
	_ARR2STR(skinny_stimuluses, stimulus, value, text);
}

const char *buttontype2str(uint32_t value)
{
	_ARR2STR(skinny_buttontypes, buttontype, value, text);
}

const char *lampmode2str(uint32_t value)
{
	_ARR2STR(skinny_lampmodes, lampmode, value, text);
}

const char *station2str(uint32_t value)
{
	_ARR2STR(skinny_stations, station, value, text);
}

const char *label2str(uint32_t value)
{
	_ARR2STR(skinny_labels, label, value, text);
}

const char *calltype2str(uint32_t value)
{
	_ARR2STR(skinny_calltypes, calltype, value, text);
}

const char *keymode2str(uint32_t value)
{
	_ARR2STR(skinny_keymodes, keymode, value, text);
}

const char *keymode2shortname(uint32_t value)
{
	_ARR2STR(skinny_keymodes, keymode, value, shortname);
}

const char *deviceregistrationstatus2str(uint32_t value)
{
	_ARR2STR(skinny_device_registrationstates, device_registrationstate, value, text);
}

const char *devicestatus2str(uint32_t value)
{
	_ARR2STR(skinny_device_states, device_state, value, text);
}

const char *astcause2skinnycause(int value)
{
	_ARR2STR(ast_skinny_causes, ast_cause, value, skinny_disp);
}

const char *astcause2skinnycause_message(int value)
{
	_ARR2STR(ast_skinny_causes, ast_cause, value, message);
}

const char *codec2str(uint32_t value)
{
	_ARR2STR(skinny_codecs, codec, value, text);
}

/*!
 * \brief Retrieve the string of format numbers and names from an array of formats
 * Buffer needs to be declared and freed afterwards
 * \param buf Buffer as char array
 * \param size Size of the Buffer String (size_t)
 * \param formats Array of sccp/skinny formats (uint32_t)
 */
char *sccp_codecs2str(char *buf, size_t size, uint32_t * formats)
{
	uint32_t x;

	unsigned len;

	char *start, *end = buf;

	if (!size)
		return buf;
	len = strlen(end);
	end += len;
	size -= len;
	start = end;
	for (x = 0; x < ARRAY_LEN(formats); x++) {
		snprintf(end, size, "%d (%s)", formats[x], codec2str(formats[x]));
		len = strlen(end);
		end += len;
		size -= len;
	}
	if (start == end)
		ast_copy_string(start, "nothing)", size);
	return buf;
}

/*!
 * \brief Convert SCCP/Skinny Types 2 String
 * \param type 	SCCP/Skinny Type
 * \param value SCCP/Skinny Value
 * \return Converted String
 */
const char *array2str(uint8_t type, uint32_t value)
{
	switch (type) {
	case SCCP_MESSAGE:
		message2str(value);
	case SCCP_ACCESSORY:
		accessory2str(value);
	case SCCP_ACCESSORY_STATE:
		accessorystatus2str(value);
	case SCCP_EXTENSION_STATE:
		extensionstatus2str(value);
	case SCCP_DNDMODE:
		dndmode2str(value);
	case SKINNY_TONE:
		tone2str(value);
	case SKINNY_ALARM:
		alarm2str(value);
	case SKINNY_DEVICETYPE:
		devicetype2str(value);
	case SKINNY_STIMULUS:
		stimulus2str(value);
	case SKINNY_BUTTONTYPE:
		buttontype2str(value);
	case SKINNY_LAMPMODE:
		lampmode2str(value);
	case SKINNY_STATION:
		station2str(value);
	case SKINNY_LBL:
		label2str(value);
	case SKINNY_CALLTYPE:
		calltype2str(value);
	case SKINNY_KEYMODE:
		keymode2str(value);
	case SKINNY_DEVICE_RS:
		deviceregistrationstatus2str(value);
	case SKINNY_DEVICE_STATE:
		devicestatus2str(value);
	case SKINNY_CODEC:
		codec2str(value);
	default:
		return "array2str: Type Not Found";
	}
}

/*!
 * \brief Convert Asterisk Codec 2 Skinny Codec
 * \param fmt Asterisk Codec as type of AST_FORMAT_*
 * \return Skinny Codec
 */
uint32_t sccp_codec_ast2skinny(int fmt)
{
	uint32_t i;

	for (i = 1; i < ARRAY_LEN(skinny_codecs); i++) {
		if (skinny_codecs[i].astcodec == fmt) {
			return skinny_codecs[i].codec;
		}
	}
	return 0;
}

/*!
 * \brief Convert Skinny Codec 2 Asterisk Codec
 * \param fmt Skinny Codec as type of SKINNY_CODECS_*
 * \return Skinny Codec
 */
int sccp_codec_skinny2ast(uint32_t fmt)
{
	uint32_t i;

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
char *ast_skip_blanks(char *str)
{
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
char *ast_trim_blanks(char *str)
{
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
char *ast_skip_nonblanks(char *str)
{
	while (*str && *str > 32)
		str++;

	return str;
}

/*!
 * \brief Asterisk Strip
 * \param s as Character
 * \return String without all Blanks
 */
char *ast_strip(char *s)
{
	s = ast_skip_blanks(s);
	if (s)
		ast_trim_blanks(s);
	return s;
}
#endif

#ifndef CS_AST_HAS_APP_SEPARATE_ARGS

/*!
 * \brief Seperate App Args
 * \param buf Buffer as Char
 * \param delim Delimiter as Char
 * \param array Array as Char Array
 * \param arraylen Array Length as Int
 * \return argc Unsigned Int
 */
unsigned int sccp_app_separate_args(char *buf, char delim, char **array, int arraylen)
{
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
 * \param d SCCP Device
 * \param keymode KeyMode as Unsigned Int
 * \param softkey SoftKey as Unsigned Int
 * \return Result as int
 *
 * \todo implement function for finding index of given SoftKey
 */
int sccp_softkeyindex_find_label(sccp_device_t * d, unsigned int keymode, unsigned int softkey)
{

	return -1;
}

/*!
 * \brief This is used on device reconnect attempt
 * \param sin IP Address + Port as struct sockaddr_in
 * \return SCCP Device
 * 
 * \lock
 * 	- devices
 */
sccp_device_t *sccp_device_find_byipaddress(struct sockaddr_in sin)
{
	sccp_device_t *d;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (d->session && d->session->sin.sin_addr.s_addr == sin.sin_addr.s_addr && d->session->sin.sin_port == sin.sin_port) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return d;
}

#if ASTERISK_VERSION_NUM >= 10600
#    ifdef HAVE_PBX_DEVICESTATE_H

/*!
 * \brief map states from sccp to ast_device_state
 * \param state SCCP Channel State
 * \return asterisk device state
 */
enum ast_device_state sccp_channelState2AstDeviceState(sccp_channelState_t state)
{
	switch (state) {
	case SCCP_CHANNELSTATE_CALLWAITING:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_CALLTRANSFER:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_CALLPARK:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_PROCEED:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_INVALIDNUMBER:
		return AST_DEVICE_INVALID;
		break;
	case SCCP_CHANNELSTATE_BUSY:
		return AST_DEVICE_BUSY;
		break;
	case SCCP_CHANNELSTATE_RINGING:
		return AST_DEVICE_RINGING;
		break;
	case SCCP_CHANNELSTATE_RINGOUT:
		return AST_DEVICE_RINGINUSE;
		break;
	case SCCP_CHANNELSTATE_HOLD:
		return AST_DEVICE_ONHOLD;
		break;
	case SCCP_CHANNELSTATE_PROGRESS:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_DIALING:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_CONNECTED:
		return AST_DEVICE_INUSE;
		break;
	case SCCP_CHANNELSTATE_ONHOOK:
		return AST_DEVICE_NOT_INUSE;
		break;
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
#    endif
#endif

/*!
 * \brief convert Feature String 2 Feature ID
 * \param str Feature Str as char
 * \return Feature Type
 */
sccp_feature_type_t sccp_featureStr2featureID(const char *str)
{
	if (!str)
		return SCCP_FEATURE_UNKNOWN;
	if (!strcasecmp(str, "cfwdall")) {
		return SCCP_FEATURE_CFWDALL;
	} else if (!strcasecmp(str, "privacy")) {
		return SCCP_FEATURE_PRIVACY;
	} else if (!strcasecmp(str, "dnd")) {
		return SCCP_FEATURE_DND;
	} else if (!strcasecmp(str, "monitor")) {
		return SCCP_FEATURE_MONITOR;
#ifdef CS_DEVSTATE_FEATURE
	} else if (!strcasecmp(str, "devstate")) {
		return SCCP_FEATURE_DEVSTATE;
#endif
	} else if (!strcasecmp(str, "hold")) {
		return SCCP_FEATURE_HOLD;
	} else if (!strcasecmp(str, "transfer")) {
		return SCCP_FEATURE_TRANSFER;
	} else if (!strcasecmp(str, "multiblink")) {
		return SCCP_FEATURE_MULTIBLINK;
	} else if (!strcasecmp(str, "mobility")) {
		return SCCP_FEATURE_MOBILITY;
	} else if (!strcasecmp(str, "conference")) {
		return SCCP_FEATURE_CONFERENCE;
	} else if (!strcasecmp(str, "test6")) {
		return SCCP_FEATURE_TEST6;
	} else if (!strcasecmp(str, "test7")) {
		return SCCP_FEATURE_TEST7;
	} else if (!strcasecmp(str, "test8")) {
		return SCCP_FEATURE_TEST8;
	} else if (!strcasecmp(str, "test9")) {
		return SCCP_FEATURE_TEST9;
	} else if (!strcasecmp(str, "testA")) {
		return SCCP_FEATURE_TESTA;
	} else if (!strcasecmp(str, "testB")) {
		return SCCP_FEATURE_TESTB;
	} else if (!strcasecmp(str, "testC")) {
		return SCCP_FEATURE_TESTC;
	} else if (!strcasecmp(str, "testD")) {
		return SCCP_FEATURE_TESTD;
	} else if (!strcasecmp(str, "testE")) {
		return SCCP_FEATURE_TESTE;
	} else if (!strcasecmp(str, "testF")) {
		return SCCP_FEATURE_TESTF;
	} else if (!strcasecmp(str, "testG")) {
		return SCCP_FEATURE_TESTG;
	} else if (!strcasecmp(str, "testH")) {
		return SCCP_FEATURE_TESTH;
	} else if (!strcasecmp(str, "testI")) {
		return SCCP_FEATURE_TESTI;
	} else if (!strcasecmp(str, "testJ")) {
		return SCCP_FEATURE_TESTJ;
	} else if (!strcasecmp(str, "pickup")) {
		return SCCP_FEATURE_PICKUP;
	}

	return SCCP_FEATURE_UNKNOWN;
}

/*!
 * \brief Handle Feature Change Event
 * \param event SCCP Event
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 * 	- line->devices is not always locked
 * 
 * \lock
 * 	- device
 */
void sccp_util_handleFeatureChangeEvent(const sccp_event_t ** event)
{
	char family[25];

	char cfwdLineStore[60];

	sccp_buttonconfig_t *config;

	sccp_line_t *line;

	sccp_linedevices_t *lineDevice;

	sccp_device_t *device = (*event)->event.featureChanged.device;

	if (!(*event) || !device)
		return;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "%s: got FeatureChangeEvent %d\n", DEV_ID_LOG(device), (*event)->event.featureChanged.featureType);
	sccp_device_lock(device);
	sprintf(family, "SCCP/%s", device->id);
	sccp_device_unlock(device);

	switch ((*event)->event.featureChanged.featureType) {
	case SCCP_FEATURE_CFWDALL:

		SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
			if (config->type == LINE) {
				line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
				if (line) {
					SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
						if (lineDevice->device != device)
							continue;

						uint8_t instance = sccp_device_find_index_for_line(device, line->name);

						sccp_dev_forward_status(line, instance, device);
						sprintf(cfwdLineStore, "%s/%s", family, config->button.line.name);
						if (lineDevice->cfwdAll.enabled) {
							pbx_db_put(cfwdLineStore, "cfwdAll", lineDevice->cfwdAll.number);
							sccp_log((DEBUGCAT_UTILS)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdLineStore);
						} else {
							ast_db_del(cfwdLineStore, "cfwdAll");
						}
					}
				}
			}
		}

		break;
	case SCCP_FEATURE_CFWDBUSY:
		break;
	case SCCP_FEATURE_DND:
		if (!device->dndFeature.status) {
			ast_db_del(family, "dnd");
			//sccp_log((DEBUGCAT_UTILS))(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "dnd");
		} else {
			if (device->dndFeature.status == SCCP_DNDMODE_SILENT)
				pbx_db_put(family, "dnd", "silent");
			else
				pbx_db_put(family, "dnd", "reject");
		}
		break;
	case SCCP_FEATURE_PRIVACY:
		if (!device->privacyFeature.status) {
			ast_db_del(family, "privacy");
			//sccp_log((DEBUGCAT_UTILS))(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "privacy");
		} else {
			char data[256];

			sprintf(data, "%d", device->privacyFeature.status);
			pbx_db_put(family, "privacy", data);
		}
		break;
	case SCCP_FEATURE_MONITOR:
		if (!device->monitorFeature.status) {
			ast_db_del(family, "monitor");
			//sccp_log((DEBUGCAT_UTILS))(VERBOSE_PREFIX_3 "%s: delete %s/%s\n", device->id, family, "monitor");
		} else {
			pbx_db_put(family, "monitor", "on");
		}
		break;
	default:
		return;
	}

}

/*!
 * \brief Parse Composed ID
 * \param labelString LabelString as string
 * \param maxLength Maximum Length as unsigned int
 *
 * \callgraph
 * \callergraph
 */
struct composedId sccp_parseComposedId(const char *labelString, unsigned int maxLength)
{
	const char *stringIterator = 0;

	uint32_t i = 0;

	boolean_t endDetected = FALSE;

	int state = 0;

	struct composedId id;

	assert(0 != labelString);

	memset(&id, 0, sizeof(id));

	for (stringIterator = labelString; stringIterator < labelString + maxLength && !endDetected; stringIterator++) {
		switch (state) {
		case 0:							// parsing of main id
			assert(i < sizeof(id.mainId));
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
			case '!':
				id.mainId[i] = '\0';
				i = 0;
				state = 3;
				break;
			default:
				id.mainId[i] = *stringIterator;
				i++;
				break;
			}
			break;

		case 1:							// parsing of sub id number
			assert(i < sizeof(id.subscriptionId.number));
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
			case '!':
				id.subscriptionId.number[i] = '\0';
				i = 0;
				state = 3;
				break;
			default:
				id.subscriptionId.number[i] = *stringIterator;
				i++;
				break;
			}
			break;

		case 2:							// parsing of sub id name
			assert(i < sizeof(id.subscriptionId.name));
			switch (*stringIterator) {
			case '\0':
				endDetected = TRUE;
				id.subscriptionId.name[i] = '\0';
				break;
			case '!':
				id.subscriptionId.name[i] = '\0';
				i = 0;
				state = 3;
				break;
			default:
				id.subscriptionId.name[i] = *stringIterator;
				i++;
				break;
			}
			break;

		case 3:							// parsing of auxiliary parameter
			assert(i < sizeof(id.subscriptionId.name));
			switch (*stringIterator) {
			case '\0':
				endDetected = TRUE;
				id.subscriptionId.aux[i] = '\0';
				break;
			default:
				id.subscriptionId.aux[i] = *stringIterator;
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

/*!
 * \brief Match Subscription ID
 * \param channel SCCP Channel
 * \param subscriptionIdNum Subscription ID Number for linedevice
 * \return result as boolean
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_util_matchSubscriptionId(const sccp_channel_t * channel, const char *subscriptionIdNum)
{
	boolean_t result = TRUE;

	boolean_t filterPhones = FALSE;

	/*
	   int compareId = 0;
	   int compareDefId = 0;
	 */

	/* Determine if the phones registered on the shared line shall be filtered at all:
	   only if a non-trivial subscription id is specified with the calling channel,
	   which is not the default subscription id of the shared line denoting all devices,
	   the phones are addressed individually. (-DD) */

	filterPhones = FALSE;							/* set the default to call all phones */

	/* First condition: Non-trivial subscriptionId specified for matching in call. */
	if (strlen(channel->subscriptionId.number) != 0) {
		/* Second condition: SubscriptionId does not match default subscriptionId of line. */
		if (0 != strncasecmp(channel->subscriptionId.number, channel->line->defaultSubscriptionId.number, strlen(channel->subscriptionId.number))) {
			filterPhones = TRUE;
		}
	}

	if (FALSE == filterPhones) {
		/* Accept phone for calling if all phones shall be called. */
		result = TRUE;
	} else if (0 != strlen(subscriptionIdNum)				/* We already know that we won't search for a trivial subscriptionId. */
		   &&0 != strncasecmp(channel->subscriptionId.number, subscriptionIdNum, strlen(channel->subscriptionId.number))) {	/* Do the match! */
		result = FALSE;
	}
#if 0
	/* we are calling a line with no or default subscriptionIdNum -> let all devices get the call -> return true */
	if (strlen(channel->subscriptionId.number) == 0) {
		result = TRUE;
		goto DONE;
	}
#    if 0
	/* channel->line has a defaultSubscriptionId -> if we wish to ring all devices -> remove the #if */
	if (strlen(channel->line->defaultSubscriptionId.number) && 0 == strncasecmp(channel->subscriptionId.number, channel->line->defaultSubscriptionId.number, strlen(channel->subscriptionId.number))) {
		result = TRUE;
		goto DONE;
	}
#    endif

	/* we are calling a line with suffix, but device does not have a subscriptionIdNum -> skip it -> return false */
	else if (strlen(subscriptionIdNum) == 0 && strlen(channel->subscriptionId.number) != 0) {
		result = FALSE;
		goto DONE;
	}

	else if (NULL != subscriptionIdNum && 0 != strlen(subscriptionIdNum)
		 && NULL != channel->line && 0 != (compareDefId = strncasecmp(channel->subscriptionId.number, channel->line->defaultSubscriptionId.number, strlen(channel->subscriptionId.number)))) {

		if (0 != strncasecmp(channel->subscriptionId.number, subscriptionIdNum, strlen(channel->subscriptionId.number))
		    && 0 != (compareId = strlen(channel->subscriptionId.number))) {

			result = FALSE;
		}
	}
 DONE:
#endif

#if 0
	ast_log(LOG_NOTICE, "channel->subscriptionId.number=%s, length=%d\n", channel->subscriptionId.number, strlen(channel->subscriptionId.number));
	ast_log(LOG_NOTICE, "subscriptionIdNum=%s, length=%d\n", subscriptionIdNum ? subscriptionIdNum : "NULL", subscriptionIdNum ? strlen(subscriptionIdNum) : -1);

	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: channel->subscriptionId.number=%s, SubscriptionId=%s\n", (channel->subscriptionId.number) ? channel->subscriptionId.number : "NULL", (subscriptionIdNum) ? subscriptionIdNum : "NULL");
	ast_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: result: %d\n", result);
#endif
	return result;
}

/*!
 * \brief Get Device Configuration
 * \param device SCCP Device
 * \param line SCCP Line
 * \return SCCP Line Devices
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 * 	- line->devices is not always locked
 */
sccp_linedevices_t *sccp_util_getDeviceConfiguration(sccp_device_t * device, sccp_line_t * line)
{
	sccp_linedevices_t *linedevice;

	if (!line || !device)
		return NULL;

	SCCP_LIST_TRAVERSE(&line->devices, linedevice, list) {
		if (linedevice->device == device)
			return linedevice;
	}

	return NULL;
}

/*!
 * \brief Parse a debug categories line to debug int
 * \param arguments Array of Arguments
 * \param startat Start Point in the Arguments Array
 * \param argc Count of Arguments
 * \param new_debug_value as uint32_t
 * \return new_debug_value as uint32_t
 */
uint32_t sccp_parse_debugline(char *arguments[], int startat, int argc, uint32_t new_debug_value)
{
	int argi;

	uint32_t i;

	char *argument = "";

	char *token = "";

	const char delimiters[] = " ,\t";

	boolean_t subtract = 0;

	if (sscanf((char *)arguments[startat], "%d", &new_debug_value) != 1) {
		for (argi = startat; argi < argc; argi++) {
			argument = (char *)arguments[argi];
			if (!strncmp(argument, "none", 4)) {
				new_debug_value = 0;
				break;
			} else if (!strncmp(argument, "no", 2)) {
				subtract = 1;
			} else if (!strncmp(argument, "all", 3)) {
				new_debug_value = 0;
				for (i = 0; i < ARRAY_LEN(sccp_debug_categories); i++) {
					if (!subtract) {
						new_debug_value += sccp_debug_categories[i].category;
					}
				}
			} else {
				// parse comma separated debug_var
				token = strtok(argument, delimiters);
				while (token != NULL) {
					// match debug level name to enum
					for (i = 0; i < ARRAY_LEN(sccp_debug_categories); i++) {
						if (strcasecmp(token, sccp_debug_categories[i].short_name) == 0) {
							if (subtract) {
								if ((new_debug_value & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
									new_debug_value -= sccp_debug_categories[i].category;
								}
							} else {
								if ((new_debug_value & sccp_debug_categories[i].category) != sccp_debug_categories[i].category) {
									new_debug_value += sccp_debug_categories[i].category;
								}
							}
						}
					}
					token = strtok(NULL, delimiters);
				}
			}
		}
	}
	return new_debug_value;
}

/*!
 * \brief Write the current debug value to debug categories
 * \param debugvalue DebugValue as uint32_t
 * \return string containing list of categories comma seperated (you need to free it)
 */
char *sccp_get_debugcategories(uint32_t debugvalue)
{
	uint32_t i;

	char *res = NULL;

	const char *sep = ", ";

	size_t size = 0;

	for (i = 0; i < ARRAY_LEN(sccp_debug_categories); ++i) {
		if ((debugvalue & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
			size_t new_size = size;

			new_size += strlen(sccp_debug_categories[i].short_name) + sizeof(sep) + 1;
			res = ast_realloc(res, new_size);
			if (!res)
				return NULL;

			if (size == 0) {
				strcpy(res, sccp_debug_categories[i].short_name);
			} else {
				strcat(res, sep);
				strcat(res, sccp_debug_categories[i].short_name);
			}

			size = new_size;
		}
	}

	return res;
}

/*!
 * \brief create a LineStatDynamicMessage
 * \param lineInstance the line instance
 * \param dirNum the dirNum (e.g. line->cid_num)
 * \param fqdn line description (top right o the first line)
 * \param lineDisplayName label on the display
 * \return LineStatDynamicMessage as sccp_moo_t *
 *
 * \callgraph
 * \callergraph
 */
sccp_moo_t *sccp_utils_buildLineStatDynamicMessage(uint32_t lineInstance, const char *dirNum, const char *fqdn, const char *lineDisplayName)
{
	sccp_moo_t *r1 = NULL;

	int dirNum_len = (dirNum != NULL) ? strlen(dirNum) : 0;

	int FQDN_len = (fqdn != NULL) ? strlen(fqdn) : 0;

	int lineDisplayName_len = (lineDisplayName != NULL) ? strlen(lineDisplayName) : 0;

	int dummy_len = dirNum_len + FQDN_len + lineDisplayName_len;

	int hdr_len = 8 - 1;

	int padding = 4;							/* after each string + 1 */

	int size = hdr_len + dummy_len + padding;

	/* message size must be muliple of 4 */
	if ((size % 4) > 0) {
		size = size + (4 - (size % 4));
	}

	r1 = sccp_build_packet(LineStatDynamicMessage, size);
	r1->msg.LineStatDynamicMessage.lel_lineNumber = htolel(lineInstance);
	r1->msg.LineStatDynamicMessage.lel_lineType = htolel(0x0f);

	if (dummy_len) {
		char buffer[dummy_len + padding];

		memset(&buffer[0], 0, sizeof(buffer));

		if (dirNum_len)
			memcpy(&buffer[0], dirNum, dirNum_len);
		if (FQDN_len)
			memcpy(&buffer[dirNum_len + 1], fqdn, FQDN_len);
		if (lineDisplayName_len)
			memcpy(&buffer[dirNum_len + FQDN_len + 2], lineDisplayName, lineDisplayName_len);

		memcpy(&r1->msg.LineStatDynamicMessage.dummy, &buffer[0], sizeof(buffer));
	}

	return r1;
}

/*!
 * \brief explode string to string array
 * \param str String to explode
 * \param sep String to use as seperator
 * \return array of string (Needs to be freed afterwards)
 */
char **explode(char *str, char *sep)
{
	int nn = 0;

	char *tmp = "";

	char *ds = strdup(str);

	char **res = (char **)ast_malloc((strlen(str) / 2) * sizeof(char *));

	if (res != NULL) {
		tmp = strtok(ds, sep);
		for (nn = 0; tmp != NULL; ++nn) {
			res[nn] = strdup(tmp);
			tmp = strtok(NULL, sep);
		}
	} else {
		return NULL;
	}
	return res;
}

/*!
 * \brief implode string array to string
 * \param str Array of String to implode
 * \param sep String to use as seperator
 * \param res Result String
 * \return success as boolean
 */
boolean_t implode(char *str[], char *sep, char **res)
{
	int nn = 0;

	if (*res) {
		sccp_free(*res);
	}

	*res = ast_malloc((strlen(str[0]) * strlen(sep) + 1) * sizeof(char *));
	memset(*res, 0, (strlen(str[0]) * strlen(sep) + 1) * sizeof(char *));
	if (*res != NULL) {
		strcat(*res, str[nn]);
		for (nn = 1; str[nn] != NULL; nn++) {
			*res = ast_realloc(*res, (strlen(*res) + strlen(str[nn]) + strlen(sep) + 1) * sizeof(char *));
			if (*res != NULL) {
				strcat(*res, sep);
				strcat(*res, str[nn]);
			} else {
				return FALSE;
			}
		}
	} else {
		return FALSE;
	}
	return TRUE;
}

#ifdef HAVE_LIBGC
void gc_warn_handler(char *msg, GC_word p)
{
	ast_log(LOG_ERROR, "LIBGC: WARNING");
	ast_log(LOG_ERROR, msg, (unsigned long)p);
}
#endif

/*!
 * \brief Compare the information of two socket with one another
 * \param s0 Socket Information
 * \param s1 Socket Information
 * \return success as int
 */
int socket_equals(struct sockaddr_in *s0, struct sockaddr_in *s1)
{
	if (s0->sin_addr.s_addr != s1->sin_addr.s_addr || s0->sin_port != s1->sin_port || s0->sin_family != s1->sin_family) {
		return 0;
	}
	return 1;
}

/*!
 * \brief Send a xml message to the phone to invoke Cisco XML Response (for example show service menu)
 */
void sendUserToDeviceVersion1Message(sccp_device_t *d, uint32_t appID, uint32_t lineInstance,uint32_t callReference,uint32_t transactionID,char data[]) 
{
	sccp_moo_t *r1 = NULL;

	int data_len, msgSize, hdr_len, padding;

	data_len = strlen(data);
	hdr_len = 40 - 1;
	padding = ((data_len + hdr_len) % 4);
	padding = (padding > 0) ? 4 - padding : 0;
	msgSize = hdr_len + data_len + padding;

	r1 = sccp_build_packet(UserToDeviceDataVersion1Message, msgSize);
	r1->msg.UserToDeviceDataVersion1Message.lel_appID = htolel(appID);
	r1->msg.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(lineInstance);
	r1->msg.UserToDeviceDataVersion1Message.lel_callReference = htolel(callReference);
	r1->msg.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
	r1->msg.UserToDeviceDataVersion1Message.lel_sequenceFlag = 0x0002;
	r1->msg.UserToDeviceDataVersion1Message.lel_displayPriority = 0x0002;
	r1->msg.UserToDeviceDataVersion1Message.lel_dataLength = htolel(data_len);

	sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "Message Data:\n%s\n", data);
	if (data_len) {
		char buffer[data_len + 2];

		memset(&buffer[0], 0, sizeof(buffer));
		memcpy(&buffer[0], data, data_len);

		memcpy(&r1->msg.UserToDeviceDataVersion1Message.data, &buffer[0], sizeof(buffer));
		sccp_dev_send(d, r1);
	}
}

/*!
 * \brief SCCP version of strcasecmp
 * \note Takes into account zero length strings, if both strings are zero length returns TRUE
 * \param data1 String to be checked
 * \param data2 String to be checked
 * \return strcasecmp as int
 *
 * \retval int on strcasecmp
 * \retval TRUE on both zero length
 * \retval FALSE on one of the the parameters being zero length
 */
boolean_t sccp_strcasecmp(const char *data1,const char *data2){
	if (sccp_strlen_zero(data1) && sccp_strlen_zero(data2)) {
		return TRUE;
	} else if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2)) {
		return strcasecmp(data1,data2);
	}
	return FALSE;
}
