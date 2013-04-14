
/*!
 * \file        sccp_utils.c
 * \brief       SCCP Utils Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

    /*!
     * \brief Print out a messagebuffer
     * \param messagebuffer Pointer to Message Buffer as char
     * \param len Lenght as Int
     */
void sccp_dump_packet(unsigned char *messagebuffer, int len)
{
	static const int numcolumns = 16;									// number output columns

	if (len <= 0 || !messagebuffer) {									// safe quard
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: messagebuffer is not valid. exiting sccp_dump_packet\n");
		return;
	}
	int col = 0;
	int cur = 0;
	int hexcolumnlength = 0;
	const char *hex = "0123456789ABCDEF";
	char hexout[(numcolumns * 3) + (numcolumns / 8) + 1];							// 3 char per hex value + grouping + endofline
	char *hexptr;
	char chrout[numcolumns + 1];
	char *chrptr;

	do {
		memset(hexout, 0, sizeof(hexout));
		memset(chrout, 0, sizeof(chrout));
		hexptr = hexout;
		chrptr = chrout;
		for (col = 0; col < numcolumns && (cur + col) < len; col++) {
			*hexptr++ = hex[(*messagebuffer >> 4) & 0xF];						// lookup first part of hex value and add to hexptr
			*hexptr++ = hex[(*messagebuffer) & 0xF];						// lookup second part of a hex value and add to hexptr
			*hexptr++ = ' ';									// add space to hexptr
			if ((col + 1) % 8 == 0) {
				*hexptr++ = ' ';								// group by blocks of eight
			}
			*chrptr++ = isprint(*messagebuffer) ? *messagebuffer : '.';				// add character or . to chrptr
			messagebuffer += 1;									// instead *messagebuffer++ to circumvent unused warning
		}
		hexcolumnlength = (numcolumns * 3) + (numcolumns / 8) - 1;					// numcolums + block spaces - 1
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "%08X - %-*.*s - %s\n", cur, hexcolumnlength, hexcolumnlength, hexout, chrout);
		cur += col;
	} while (cur < (len - 1));
}

/*!
 * \brief Add Host to the Permithost Linked List
 * \param d SCCP Device
 * \param config_string as Character
 * 
 * \warning
 *      - device->permithosts is not always locked
 */
void sccp_permithost_addnew(sccp_device_t * d, const char *config_string)
{
	sccp_hostname_t *permithost;

	if ((permithost = sccp_malloc(sizeof(sccp_hostname_t)))) {
		sccp_copy_string(permithost->name, config_string, sizeof(permithost->name));
		SCCP_LIST_INSERT_HEAD(&d->permithosts, permithost, list);
	} else {
		pbx_log(LOG_WARNING, "Error adding the permithost = %s to the list\n", config_string);
	}
}

/*!
 * \brief Return Number of Buttons on AddOn Device
 * \param d SCCP Device
 * \return taps (Number of Buttons on AddOn Device)
 * 
 * \lock
 *      - device->addons
 */
int sccp_addons_taps(sccp_device_t * d)
{
	sccp_addon_t *cur = NULL;
	int taps = 0;

	if (!strcasecmp(d->config_type, "7914"))
		return 28;											// on compatibility mode it returns 28 taps for a double 7914 addon

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
	sccp_addon_t *addon;

	if (!d)
		return;

	//      while ((AST_LIST_REMOVE_HEAD(&d->addons, list))) ;
	while ((addon = SCCP_LIST_REMOVE_HEAD(&d->addons, list))) {
		sccp_free(addon);
	}
	d->addons.first = NULL;
	d->addons.last = NULL;
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
	struct timeval start = pbx_tvnow();

	usleep(1);
	while (ast_tvdiff_ms(pbx_tvnow(), start) < ms) {
		usleep(1);
	}
}

/*!
 * \brief Find Device by ID
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - devices
 */
#if DEBUG
/*!
 * \param name Device ID (hostname)
 * \param useRealtime Use RealTime as Boolean
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *__sccp_device_find_byid(const char *name, boolean_t useRealtime, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Device ID (hostname)
 * \param useRealtime Use RealTime as Boolean
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *sccp_device_find_byid(const char *name, boolean_t useRealtime)
#endif
{
	sccp_device_t *d = NULL;

	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for device with name ''\n");
		return NULL;
	}

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (d && d->id && !strcasecmp(d->id, name)) {
#if DEBUG
			d = sccp_refcount_retain(d, filename, lineno, func);
#else
			d = sccp_device_retain(d);
#endif
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
 *
 * \callgraph
 * \callergraph
 */
#if DEBUG
/*!
 * \param name Device ID (hostname)
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *__sccp_device_find_realtime(const char *name, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Device ID (hostname)
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *sccp_device_find_realtime(const char *name)
#endif
{
	sccp_device_t *d = NULL;
	PBX_VARIABLE_TYPE *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimedevicetable)) || sccp_strlen_zero(name))
		return NULL;

	if ((variable = pbx_load_realtime(GLOB(realtimedevicetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' found in realtime table '%s'\n", name, GLOB(realtimedevicetable));

		d = sccp_device_create(name);		/** create new device */
		if (!d) {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime device '%s'\n", name);
			return NULL;
		}
		//              sccp_copy_string(d->id, name, sizeof(d->id));

		sccp_config_applyDeviceConfiguration(d, v);		/** load configuration and set defaults */
		v = variable;
		sccp_config_applyDeviceDefaults(d, v);			/** set device defaults */

		sccp_config_restoreDeviceFeatureStatus(d);		/** load device status from database */

		sccp_device_addToGlobals(d);				/** add to device to global device list */

		d->realtime = TRUE;					/** set device as realtime device */
		pbx_variables_destroy(v);

		return d;
	}

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' not found in realtime table '%s'\n", name, GLOB(realtimedevicetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Name
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 */
#if DEBUG
/*!
 * \param name Line Name
 * \param useRealtime Use Realtime as Boolean
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line
 */
sccp_line_t *__sccp_line_find_byname(const char *name, uint8_t useRealtime, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Line Name
 * \param useRealtime Use Realtime as Boolean
 * \return SCCP Line
 */
sccp_line_t *sccp_line_find_byname(const char *name, uint8_t useRealtime)
#endif
{
	sccp_line_t *l = NULL;

	//      sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "SCCP: Looking for line '%s'\n", name);
	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for line with name ''\n");
		return NULL;
	}

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (l && l->name && !strcasecmp(l->name, name)) {
#if DEBUG
			l = sccp_refcount_retain(l, filename, lineno, func);
#else
			l = sccp_line_retain(l);
#endif
			break;
		}
	}

#ifdef CS_SCCP_REALTIME
	if (!l && useRealtime) {
		l = sccp_line_find_realtime_byname(name);							/* already retained */
	}	
#endif
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (!l) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found.\n", name);
		return NULL;
	}
	//      sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found line '%s'\n", "SCCP", l->name);

	return l;
}

#ifdef CS_SCCP_REALTIME

/*!
 * \brief Find Line via Realtime
 *
 * \callgraph
 * \callergraph
 */
#if DEBUG
/*!
 * \param name Line Name
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line
 */
sccp_line_t *__sccp_line_find_realtime_byname(const char *name, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Line Name
 * \return SCCP Line
 */
sccp_line_t *sccp_line_find_realtime_byname(const char *name)
#endif
{
	sccp_line_t *l = NULL;
	PBX_VARIABLE_TYPE *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimelinetable)) || sccp_strlen_zero(name)) {
		return NULL;
	}

	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for line with name ''\n");
		return NULL;
	}

	if ((variable = pbx_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));

		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_4 "SCCP: creating realtime line '%s'\n", name);

		l = sccp_line_create(name);									/* already retained */
		sccp_config_applyLineConfiguration(l, variable);
		l->realtime = TRUE;
		sccp_line_addToGlobals(l);									// can return previous instance on doubles
		pbx_variables_destroy(v);
		
		if (!l) {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime line '%s'\n", name);
		}
		return l;
	}

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found in realtime table '%s'\n", name, GLOB(realtimelinetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Instance on device
 *
 * \todo No ID Specified only instance, should this function be renamed ?
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device->buttonconfig
 *        - see sccp_line_find_byname_wo()
 */
#if DEBUG
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line (can be null)
 */
sccp_line_t *__sccp_line_find_byid(sccp_device_t * d, uint16_t instance, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 */
sccp_line_t *sccp_line_find_byid(sccp_device_t * d, uint16_t instance)
#endif
{
	sccp_line_t *l = NULL;
	sccp_buttonconfig_t *config;

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);

	if (!d || instance == 0)
		return NULL;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: button instance %d, type: %d\n", DEV_ID_LOG(d), config->instance, config->type);

		if (config && config->type == LINE && config->instance == instance && !sccp_strlen_zero(config->button.line.name)) {
#if DEBUG
			l = __sccp_line_find_byname(config->button.line.name, TRUE, filename, lineno, func);
#else
			l = sccp_line_find_byname(config->button.line.name, TRUE);
#endif
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
 * \brief Find Channel by ID, using a specific line
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line->channels
 *      - channel
 */
#if DEBUG
/*!
 * \param l     SCCP Line
 * \param id    channel ID as int
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return *refcounted* SCCP Channel (can be null)
 */
sccp_channel_t *__sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id, const char *filename, int lineno, const char *func)
#else
/*!
 * \param l     SCCP Line
 * \param id    channel ID as int
 * \return *refcounted* SCCP Channel (can be null)
 */
sccp_channel_t *sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id)
#endif
{
	sccp_channel_t *c = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);

	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, c, list) {
		sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Channel %u state: %d\n", c->callid, c->state);
		if (c && c->callid == id && c->state != SCCP_CHANNELSTATE_DOWN) {
#if DEBUG
			c = sccp_refcount_retain(c, filename, lineno, func);
#else
			c = sccp_channel_retain(c);
#endif
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel by callid: %u\n", c->currentDeviceId, c->callid);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&l->channels);

	return c;
}

/*!
 * \brief Find Line by ID
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line->channels
 *      - channel
 */
#if DEBUG
/*!
 * \param id ID as int
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return *refcounted* SCCP Channel (can be null)
 */
sccp_channel_t *__sccp_channel_find_byid(uint32_t id, const char *filename, int lineno, const char *func)
#else
/*!
 * \param id ID as int
 * \return *refcounted* SCCP Channel (can be null)
 */
sccp_channel_t *sccp_channel_find_byid(uint32_t id)
#endif
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by id %u\n", id);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
#if DEBUG
		channel = __sccp_find_channel_on_line_byid(l, id, filename, lineno, func);
#else
		channel = sccp_find_channel_on_line_byid(l, id);
#endif
		if (channel)
			break;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return channel;
}

/*!
 * \brief Find Channel by Pass Through Party ID
 * We need this to start the correct rtp stream.
 * \param passthrupartyid Party ID
 * \return *refcounted* SCCP Channel - cann bee NULL if no channel with this id was found
 *
 * \note Does check that channel state not is DOWN.
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line->channels
 *      - channel
 */
#if DEBUG
/*!
 * \param passthrupartyid Party ID
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return *refcounted* SCCP Channel - cann bee NULL if no channel with this id was found
 */
sccp_channel_t *__sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid, const char *filename, int lineno, const char *func)
#else
/*!
 * \param passthrupartyid Party ID
 * \return *refcounted* SCCP Channel - cann bee NULL if no channel with this id was found
 */
sccp_channel_t *sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid)
#endif
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u\n", passthrupartyid);

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%u: Found channel partyID: %u state: %d\n", channel->callid, channel->passthrupartyid, channel->state);
			if (channel->passthrupartyid == passthrupartyid && channel->state != SCCP_CHANNELSTATE_DOWN) {
#if DEBUG
				channel = sccp_refcount_retain(channel, filename, lineno, func);
#else
				channel = sccp_channel_retain(channel);
#endif
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel for callid %u by passthrupartyid %d\n", channel->currentDeviceId, channel->callid, channel->passthrupartyid);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (channel) {
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (!channel)
		ast_log(LOG_WARNING, "SCCP: Could not find active channel with Passthrupartyid %u\n", passthrupartyid);

	return channel;
}

/*!
 * \brief Find Channel by Pass Through Party ID on a line connected to device provided
 * We need this to start the correct rtp stream.
 *
 * \param d SCCP Device
 * \param passthrupartyid Party ID
 * \return *locked* SCCP Channel - can be NULL if no channel with this id was found. 
 *
 * \note does not take channel state into account, this need to be asserted in the calling function
 * \note this is different from the sccp_channel_find_bypassthrupartyid behaviour
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line->channels
 *      - channel
 */
#if !CS_EXPERIMENTAL	// Old Version
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid)
{
	if (!d) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "SCCP: No device provided to look for %u\n", passthrupartyid);
		return NULL;
	}
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;
	sccp_buttonconfig_t *buttonconfig = NULL;
	boolean_t channelFound = FALSE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u on device %s\n", passthrupartyid, d->id);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			l = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);
			if (l) {
				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, c, list) {
					//if (c->passthrupartyid == passthrupartyid && c->state != SCCP_CHANNELSTATE_DOWN) {
					sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Found channel passthrupartyid: %u, callid: %u,  state: %d on line %s\n", DEV_ID_LOG(d), c->passthrupartyid, c->callid, c->state, l->name);
					if (c->passthrupartyid == passthrupartyid) {
						sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Found channel (passthrupartyid: %u, callid: %u) on line %s with state %d\n", DEV_ID_LOG(d), c->passthrupartyid, c->callid, l->name, c->state);
						c = sccp_channel_retain(c);
						channelFound = TRUE;
						break;
					}
				}
				SCCP_LIST_UNLOCK(&l->channels);

				l = sccp_line_release(l);
				if (channelFound)
					break;
			}
		}
	}

	if (!c || !channelFound) {
		ast_log(LOG_WARNING, "SCCP: Could not find active channel with Passthrupartyid %u on device %s\n", passthrupartyid, DEV_ID_LOG(d));
	}	

	return c;
}
#else	// New Version
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid)
{
	if (!d) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "SCCP: No device provided to look for %u\n", passthrupartyid);
		return NULL;
	}
	btnlist * btn = d->buttonTemplate;
	if (!btn) {
		sccp_log(DEBUGCAT_BUTTONTEMPLATE) (VERBOSE_PREFIX_3 "%s: no buttontemplate, reset device\n", DEV_ID_LOG(d));
		return NULL;		
	}
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;
	int i = 0;
	boolean_t channelFound = FALSE;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by PassThruId %u on device %s\n", passthrupartyid, d->id);
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
			if ((l=sccp_line_retain(btn[i].ptr))) {
				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Found line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, c, list) {
					//if (c->passthrupartyid == passthrupartyid && c->state != SCCP_CHANNELSTATE_DOWN) {
					sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Found channel passthrupartyid: %u, callid: %u,  state: %d on line %s\n", DEV_ID_LOG(d), c->passthrupartyid, c->callid, c->state, l->name);
					if (c->passthrupartyid == passthrupartyid) {
						sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: Found channel (passthrupartyid: %u, callid: %u) on line %s with state %d\n", DEV_ID_LOG(d), c->passthrupartyid, c->callid, l->name, c->state);
						c = sccp_channel_retain(c);
						channelFound = TRUE;
						break;
					}
				}
				SCCP_LIST_UNLOCK(&l->channels);
				
				l = sccp_line_release(l);
				if (channelFound)
					break;
			}
		}
	}
	if (!c || !channelFound) {
		ast_log(LOG_WARNING, "SCCP: Could not find active channel with Passthrupartyid %u on device %s\n", passthrupartyid, DEV_ID_LOG(d));
	}	

	return c;
}
#endif
/*!
 * \brief Find Channel by State on Line
 * \return *refcounted* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line->channels
 *      - channel
 */
#if DEBUG
/*!
 * \param l SCCP Line
 * \param state State
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *__sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state, const char *filename, int lineno, const char *func)
#else
/*!
 * \param l SCCP Line
 * \param state State
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state)
#endif
{
	sccp_channel_t *channel = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
		if (channel && channel->state == state) {
#if DEBUG
			channel = sccp_refcount_retain(channel, filename, lineno, func);
#else
			channel = sccp_channel_retain(channel);
#endif
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel for callid %d by state %d\n", channel->currentDeviceId, channel->callid, channel->state);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&l->channels);

	return channel;
}

/*!
 * \brief Find Channel by State on Device
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 * 
 * \lock
 *      - device
 *        - see sccp_line_find_byname_wo()
 *        - line->channels
 *      - channel
 */
#if DEBUG
/*!
 * \param d SCCP Device
 * \param state State as int
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *__sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param state State as int
 * \return *refcounted* SCCP Channel
 */
sccp_channel_t *sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state)
#endif
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *l = NULL;
	sccp_buttonconfig_t *buttonconfig = NULL;
	boolean_t channelFound = FALSE;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Looking for channel by state '%d'\n", state);

	if (!(d = sccp_device_retain(d)))
		return NULL;

	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			l = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);
			if (l) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: line: '%s'\n", DEV_ID_LOG(d), l->name);
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
					if (channel->state == state) {
						/* check that subscriptionId matches */
						if (sccp_util_matchSubscriptionId(channel, buttonconfig->button.line.subscriptionId.number)) {
#if DEBUG
							channel = sccp_refcount_retain(channel, filename, lineno, func);
#else
							channel = sccp_channel_retain(channel);
#endif
							sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(d), channel->callid);
							channelFound = TRUE;
							break;
						} else {
							sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found channel (%d), but it does not match subscriptionId %s \n", DEV_ID_LOG(d), channel->callid, buttonconfig->button.line.subscriptionId.number);
						}

					}
				}
				SCCP_LIST_UNLOCK(&l->channels);
				l = sccp_line_release(l);

				if (channelFound)
					break;
			}
		}
	}
	d = sccp_device_release(d);

	return channel;
}

/*!
 * \brief Find Selected Channel by Device
 * \param d SCCP Device
 * \param channel channel
 * \return x SelectedChannel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - device->selectedChannels
 *
 * \todo Currently this returns the selectedchannel unretained !
 */
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * channel)
{
	if (!d)
		return NULL;

	sccp_selectedchannel_t *sc = NULL;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channel (%d)\n", DEV_ID_LOG(d), channel->callid);

	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, sc, list) {
		if (sc->channel == channel) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found channel (%d)\n", DEV_ID_LOG(d), channel->callid);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);
	return sc;
}

/*!
 * \brief Count Selected Channel on Device
 * \param d SCCP Device
 * \return count Number of Selected Channels
 * 
 * \lock
 *      - device->selectedChannels
 */
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d)
{
	sccp_selectedchannel_t *sc = NULL;
	uint8_t count = 0;

	if (!(d = sccp_device_retain(d)))
		return 0;

	sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Looking for selected channels count\n", DEV_ID_LOG(d));

	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, sc, list) {
		count++;
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	return count;
}

/*!
 * \brief Notify asterisk for new state
 * \param channel SCCP Channel
 * \param state New State - type of AST_STATE_*
 */
void sccp_pbx_setcallstate(sccp_channel_t * channel, int state)
{
	if (channel) {
		if (channel->owner) {
			pbx_setstate(channel->owner, state);
			//                      pbx_cond_wait(&channel->astStateCond, &channel->lock);
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Set asterisk state %s (%d) for call %d\n", channel->currentDeviceId, pbx_state2str(state), state, channel->callid);
		}
	}
}

/*!
 * \brief Clean Asterisk Database Entries in the "SCCP" Family
 * 
 * \lock
 *      - devices
 */
void sccp_dev_dbclean()
{
	struct ast_db_entry *entry = NULL;
	sccp_device_t *d;
	char key[256];

	//! \todo write an pbx implementation for that
	//entry = PBX(feature_getFromDatabase)tree("SCCP", NULL);
	while (entry) {
		sscanf(entry->key, "/SCCP/%s", key);
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Looking for '%s' in the devices list\n", key);
		if ((strlen(key) == 15) && (!strncmp(key, "SEP", 3) || !strncmp(key, "ATA", 3) || !strncmp(key, "VGC", 3) || !strncmp(key, "AN", 2) || !strncmp(key, "SKIGW", 5))) {

			SCCP_RWLIST_RDLOCK(&GLOB(devices));
			SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
				if (d->id && !strcasecmp(d->id, key)) {
					break;
				}
			}
			SCCP_RWLIST_UNLOCK(&GLOB(devices));

			if (!d) {
				PBX(feature_removeFromDatabase) ("SCCP", key);
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: device '%s' removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry)
		pbx_db_freetree(entry);
}

const char *message2str(uint32_t value)
{
	_ARR2STR(sccp_messagetypes, type, value, text);
}

size_t message2size(uint32_t value)
{
	uint32_t i;

	for (i = 0; i < ARRAY_LEN(sccp_messagetypes); i++) {
		if (sccp_messagetypes[i].type == value) {
			return sccp_messagetypes[i].size + SCCP_PACKET_HEADER;
		}
	}
	pbx_log(LOG_NOTICE, "SCCP: Unknown SCCP Message with %02X\n", value);
	return SCCP_MAX_PACKET;
}

const char *channelstate2str(uint32_t value)
{
	_ARR2STR(sccp_channelstates, channelstate, value, text);
}

const char *pbxdevicestate2str(uint32_t value)
{
	_ARR2STR(pbx_devicestates, devicestate, value, text);
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

const char *transmitModes2str(skinny_transmitOrReceive_t value)
{
	_ARR2STR(skinny_transmitOrReceiveModes, mode, value, text);
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

const char *event2str(sccp_event_type_t event_type)
{
	_ARR2STR(sccp_event_types, event_type, event_type, text);
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
	_ARR2STR(skinny_stimuly, stimulus, value, text);
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

const char *keymode2description(uint32_t value)
{
	_ARR2STR(skinny_keymodes, keymode, value, description);
}

const char *deviceregistrationstatus2str(uint32_t value)
{
	_ARR2STR(skinny_device_registrationstates, device_registrationstate, value, text);
}

const char *devicestatus2str(uint32_t value)
{
	_ARR2STR(skinny_device_states, device_state, value, text);
}

const char *codec2str(skinny_codec_t value)
{
	_ARR2STR(skinny_codecs, codec, value, text);
}

int codec2payload(skinny_codec_t value)
{
	_ARR2INT(skinny_codecs, codec, value, rtp_payload_type);
}

const char *codec2key(uint32_t value)
{
	_ARR2STR(skinny_codecs, codec, value, key);
}

const char *codec2name(uint32_t value)
{
	_ARR2STR(skinny_codecs, codec, value, name);
}

const char *featureType2str(uint32_t value)
{
	_ARR2STR(sccp_feature_types, featureType, value, text);
}

uint32_t debugcat2int(const char *str)
{
	_STRARR2INT(sccp_debug_categories, key, str, category);
}

const char *skinny_formatType2str(uint8_t value)
{
	_ARR2STR(skinny_formatTypes, id, value, text);
}

/*!
 * \brief Retrieve the string of format numbers and names from an array of formats
 * Buffer needs to be declared and freed afterwards
 * \param buf   Buffer as char array
 * \param size  Size of Buffer
 * \param codecs Array of Skinny Codecs
 * \param length Max Length
 */
char *sccp_multiple_codecs2str(char *buf, size_t size, skinny_codec_t * codecs, int length)
{
	int x;
	unsigned len;
	char *start, *end = buf;

	if (!size)
		return buf;

	snprintf(end, size, "(");
	len = strlen(end);
	end += len;
	size -= len;
	start = end;
	for (x = 0; x < length; x++) {
		if (codecs[x] == 0)
			continue;

		snprintf(end, size, "%s, ", codec2name(codecs[x]));
		len = strlen(end);
		end += len;
		size -= len;
	}
	if (start == end)
		pbx_copy_string(start, "nothing)", size);
	else if (size > 2) {
		*(end - 2) = ')';
		*(end - 1) = '\0';
	}
	return buf;
}

void skinny_codec_pref_remove(skinny_codec_t * skinny_codec_prefs, skinny_codec_t skinny_codec);

/*!
 * \brief Remove Codec from Skinny Codec Preferences
 */
void skinny_codec_pref_remove(skinny_codec_t * skinny_codec_prefs, skinny_codec_t skinny_codec)
{
	skinny_codec_t *old_skinny_codec_prefs = { 0 };
	int x, y = 0;

	if (ARRAY_LEN(skinny_codec_prefs))
		return;

	memcpy(old_skinny_codec_prefs, skinny_codec_prefs, sizeof(skinny_codec_t));
	memset(skinny_codec_prefs, 0, SKINNY_MAX_CAPABILITIES);

	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		// if not found element copy to new array
		if (SKINNY_CODEC_NONE == old_skinny_codec_prefs[x])
			break;
		if (old_skinny_codec_prefs[x] != skinny_codec) {
			//                        sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_1 "re-adding codec '%d' at pos %d\n", old_skinny_codec_prefs[x], y);
			skinny_codec_prefs[y++] = old_skinny_codec_prefs[x];
		} else {
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_1 "found codec '%d' at pos %d, skipping\n", skinny_codec, x);
		}
	}
}

/*!
 * \brief Append Codec to Skinny Codec Preferences
 */
static int skinny_codec_pref_append(skinny_codec_t * skinny_codec_pref, skinny_codec_t skinny_codec)
{
	int x = 0;

	skinny_codec_pref_remove(skinny_codec_pref, skinny_codec);

	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		if (SKINNY_CODEC_NONE == skinny_codec_pref[x]) {
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_1 "adding codec '%d' as pos %d\n", skinny_codec, x);
			skinny_codec_pref[x] = skinny_codec;
			return x;
		}
	}
	return -1;
}

/*!
 * \brief Parse Skinny Codec Allow / Disallow Config Lines
 */
int sccp_parse_allow_disallow(skinny_codec_t * skinny_codec_prefs, skinny_codec_t * skinny_codec_mask, const char *list, int allowing)
{
	int all;
	unsigned int x;

	//      unsigned int y;
	int errors = 0;
	char *parse = NULL, *this = NULL;
	boolean_t found = FALSE;

	//      boolean_t mapped = FALSE;
	skinny_codec_t codec;

	parse = sccp_strdupa(list);
	while ((this = strsep(&parse, ","))) {
		all = strcasecmp(this, "all") ? 0 : 1;

		if (all && !allowing) {										// disallowing all
			memset(skinny_codec_prefs, 0, SKINNY_MAX_CAPABILITIES);
			break;
		}
		for (x = 0; x < ARRAY_LEN(skinny_codecs); x++) {
			if (all || !strcasecmp(skinny_codecs[x].key, this)) {
				codec = skinny_codecs[x].codec;
				found = TRUE;
				if (skinny_codec_mask) {
					if (allowing)
						*skinny_codec_mask |= codec;
					else
						*skinny_codec_mask &= ~codec;
				}
				if (skinny_codec_prefs) {
					if (strcasecmp(this, "all")) {
						if (allowing) {
							//                                                        sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_1 "adding codec '%s'\n", this);
							skinny_codec_pref_append(skinny_codec_prefs, codec);
						} else {
							//                                                        sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_1 "removing codec '%s'\n", this);
							skinny_codec_pref_remove(skinny_codec_prefs, codec);
						}
					}
				}
			}
		}
		if (!found) {
			pbx_log(LOG_WARNING, "Cannot %s unknown codec '%s'\n", allowing ? "allow" : "disallow", this);
			errors++;
			continue;
		}
	}
	return errors;
}

/*!
 * \brief Check if Skinny Codec is compatible with Skinny Capabilities Array
 */
boolean_t sccp_utils_isCodecCompatible(skinny_codec_t codec, const skinny_codec_t capabilities[], uint8_t length)
{
	uint8_t i;

	for (i = 0; i < length; i++) {
		if (capabilities[i] == codec) {
			return TRUE;
		}
	}
	return FALSE;
}

#ifndef CS_AST_HAS_STRINGS

/*!
 * \brief Asterisk Skip Blanks
 * \param str as Character
 * \return String without Blanks
 */
char *pbx_skip_blanks(char *str)
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
char *pbx_trim_blanks(char *str)
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
char *pbx_skip_nonblanks(char *str)
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
char *pbx_strip(char *s)
{
	s = pbx_skip_blanks(s);
	if (s)
		pbx_trim_blanks(s);
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
 * \param sin   Socket Address In
 * \return SCCP Device
 * 
 * \lock
 *      - devices
 */
//sccp_device_t *sccp_device_find_byipaddress(unsigned long s_addr)
sccp_device_t *sccp_device_find_byipaddress(struct sockaddr_in sin)
{
	sccp_device_t *d = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (d->session && d->session->sin.sin_addr.s_addr == sin.sin_addr.s_addr && d->session->sin.sin_port == d->session->sin.sin_port) {
			d = sccp_device_retain(d);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return d;
}

#if ASTERISK_VERSION_NUMBER >= 10600
#ifdef HAVE_PBX_DEVICESTATE_H

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
#endif
#endif

/*!
 * \brief convert Feature String 2 Feature ID
 * \param str Feature Str as char
 * \return Feature Type
 */
sccp_feature_type_t sccp_featureStr2featureID(const char *const str)
{
	if (!str)
		return SCCP_FEATURE_UNKNOWN;

	uint32_t i;

	for (i = 0; i < ARRAY_LEN(sccp_feature_types); i++) {
		if (!strcasecmp(sccp_feature_types[i].text, str)) {
			return sccp_feature_types[i].featureType;
		}
	}
	return SCCP_FEATURE_UNKNOWN;
}

/*!
 * \brief Handle Feature Change Event for persistent feature storage
 * \param event SCCP Event
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 *      - device->buttonconfig is not always locked
 *      - line->devices is not always locked
 * 
 * \lock
 *      - device
 */
void sccp_util_featureStorageBackend(const sccp_event_t * event)
{
	char family[25];
	char cfwdLineStore[60];
	sccp_linedevices_t *linedevice = NULL;
	sccp_device_t *device = event->event.featureChanged.device;

	if (!event || !device) {
		return;
	}

	sccp_log((DEBUGCAT_EVENT | DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: StorageBackend got Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), featureType2str(event->event.featureChanged.featureType), event->event.featureChanged.featureType);
	sprintf(family, "SCCP/%s", device->id);

	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_CFWDNONE:
		case SCCP_FEATURE_CFWDBUSY:
		case SCCP_FEATURE_CFWDALL:
			if ((linedevice = event->event.featureChanged.linedevice)) {
				sccp_line_t *line = linedevice->line;	
				uint8_t instance = linedevice->lineInstance;
				sccp_dev_forward_status(line, instance, device);
				sprintf(cfwdLineStore, "%s/%s", family, line->name);
				switch (event->event.featureChanged.featureType) {
					case SCCP_FEATURE_CFWDALL:
						if (linedevice->cfwdAll.enabled) {
							PBX(feature_addToDatabase) (cfwdLineStore, "cfwdAll", linedevice->cfwdAll.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdLineStore);
						} else {
							PBX(feature_removeFromDatabase) (cfwdLineStore, "cfwdAll");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDBUSY:
						if (linedevice->cfwdBusy.enabled) {
							PBX(feature_addToDatabase) (cfwdLineStore, "cfwdBusy", linedevice->cfwdBusy.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdLineStore);
						} else {
							PBX(feature_removeFromDatabase) (cfwdLineStore, "cfwdBusy");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDNONE:
						PBX(feature_removeFromDatabase) (cfwdLineStore, "cfwdAll");
						PBX(feature_removeFromDatabase) (cfwdLineStore, "cfwdBusy");
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: cfwd cleared from db\n", DEV_ID_LOG(device));
					default:
						break;
				}
			}
			break;
		case SCCP_FEATURE_DND:
//			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to %s\n", DEV_ID_LOG(device), device->dndFeature.status ? "on" : "off");
			if (device->dndFeature.previousStatus != device->dndFeature.status) {
				if (!device->dndFeature.status) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to off\n", DEV_ID_LOG(device));
					PBX(feature_removeFromDatabase) (family, "dnd");
				} else {
					if (device->dndFeature.status == SCCP_DNDMODE_SILENT) {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to silent\n", DEV_ID_LOG(device));
						PBX(feature_addToDatabase) (family, "dnd", "silent");
					} else {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to reject\n", DEV_ID_LOG(device));
						PBX(feature_addToDatabase) (family, "dnd", "reject");
					}
				}
				device->dndFeature.previousStatus = device->dndFeature.status;
			}
			break;
		case SCCP_FEATURE_PRIVACY:
			if (device->privacyFeature.previousStatus != device->privacyFeature.status) {
				if (!device->privacyFeature.status) {
					PBX(feature_removeFromDatabase) (family, "privacy");
				} else {
					char data[256];

					sprintf(data, "%d", device->privacyFeature.status);
					PBX(feature_addToDatabase) (family, "privacy", data);
				}
				device->privacyFeature.status = device->privacyFeature.status;
			}
			break;
		case SCCP_FEATURE_MONITOR:
			if (device->monitorFeature.previousStatus != device->monitorFeature.status) {
				if (!device->monitorFeature.status) {
					PBX(feature_removeFromDatabase) (family, "monitor");
				} else {
					PBX(feature_addToDatabase) (family, "monitor", "on");
				}
				device->monitorFeature.previousStatus = device->monitorFeature.status;
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
			case 0:										// parsing of main id
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

			case 1:										// parsing of sub id number
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

			case 2:										// parsing of sub id name
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

			case 3:										// parsing of auxiliary parameter
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
	pbx_log(LOG_NOTICE, "sccp_channel->subscriptionId.number=%s, length=%d\n", channel->subscriptionId.number, strlen(channel->subscriptionId.number));
	pbx_log(LOG_NOTICE, "subscriptionIdNum=%s, length=%d\n", subscriptionIdNum ? subscriptionIdNum : "NULL", subscriptionIdNum ? strlen(subscriptionIdNum) : -1);

	pbx_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: sccp_channel->subscriptionId.number=%s, SubscriptionId=%s\n", (channel->subscriptionId.number) ? channel->subscriptionId.number : "NULL", (subscriptionIdNum) ? subscriptionIdNum : "NULL");
	pbx_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: result: %d\n", result);
#endif
	return result;
}

/*!
 * \brief Get Device Configuration
 * \param device SCCP Device
 * \param line SCCP Line
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line Devices
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 *      - line->devices is not always locked
 */
sccp_linedevices_t *__sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func)
{
	if (!line) {
		pbx_log(LOG_NOTICE, "%s: [%s:%d]->linedevice_find: No line provided to search in\n", DEV_ID_LOG(device), filename, lineno);
		return NULL;
	}
	if (!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (line: %s)\n", filename, lineno, line ? line->name : "UNDEF");
		return NULL;
	}

	sccp_linedevices_t *linedevice = NULL;
	sccp_linedevices_t *ld = NULL;

	SCCP_LIST_LOCK(&((sccp_line_t *) line)->devices);
	SCCP_LIST_TRAVERSE(&((sccp_line_t *) line)->devices, linedevice, list) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "linedevice %p for device %s line %s\n", linedevice, DEV_ID_LOG(linedevice->device), linedevice->line->name);
		if (device == linedevice->device) {
			ld = sccp_linedevice_retain(linedevice);
			sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: found linedevice for line %s. Returning linedevice %p\n", DEV_ID_LOG(device), ld->line->name, ld);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&((sccp_line_t *) line)->devices);

	if (!ld) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for line %s could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, line->name);
	}
	return ld;
}

/*!
 * \brief Parse a debug categories line to debug int
 * \param arguments Array of Arguments
 * \param startat Start Point in the Arguments Array
 * \param argc Count of Arguments
 * \param new_debug_value as uint32_t
 * \return new_debug_value as uint32_t
 */
int32_t sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug_value)
{
	int argi;
	int32_t i;
	char *argument = "";
	char *token = "";
	const char delimiters[] = " ,\t";
	boolean_t subtract = 0;

	if (sscanf((char *) arguments[startat], "%d", &new_debug_value) != 1) {
		for (argi = startat; argi < argc; argi++) {
			argument = (char *) arguments[argi];
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
						if (strcasecmp(token, sccp_debug_categories[i].key) == 0) {
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
char *sccp_get_debugcategories(int32_t debugvalue)
{
	int32_t i;
	char *res = NULL;
	char *tmpres = NULL;
	const char *sep = ",";
	size_t size = 0;

	for (i = 0; i < ARRAY_LEN(sccp_debug_categories); ++i) {
		if ((debugvalue & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
			size_t new_size = size;

			new_size += strlen(sccp_debug_categories[i].key) + sizeof(sep) + 1;
			tmpres = sccp_realloc(res, new_size);
			if (tmpres == NULL) {
				pbx_log(LOG_ERROR, "Memory Allocation Error\n");
				sccp_free(res);
				return NULL;
			}	
			res = tmpres;
			if (size == 0) {
				strcpy(res, sccp_debug_categories[i].key);
			} else {
				strcat(res, sep);
				strcat(res, sccp_debug_categories[i].key);
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
	int padding = 4;											/* after each string + 1 */
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

#ifdef HAVE_LIBGC

/*!
 * \brief Verbose Logging Hanler for the GC Garbage Collector
 */
void gc_warn_handler(char *msg, GC_word p)
{
	pbx_log(LOG_ERROR, "LIBGC: WARNING");
	pbx_log(LOG_ERROR, msg, (unsigned long) p);
}
#endif

/*!
 * \brief Compare the information of two socket with one another
 * \param s0 Socket Information
 * \param s1 Socket Information
 * \return success as int
 *
 * \retval 0 on diff
 * \retval 1 on equal
 */
int socket_equals(struct sockaddr_in *s0, struct sockaddr_in *s1)
{
	if (s0->sin_addr.s_addr != s1->sin_addr.s_addr || s0->sin_port != s1->sin_port || s0->sin_family != s1->sin_family) {
		return 0;
	}
	return 1;
}

/*!
 * \brief SCCP version of strlen_zero
 * \param data String to be checked
 * \return zerolength as boolean
 *
 * \retval FALSE on non zero length
 * \retval TRUE on zero length
 */
boolean_t sccp_strlen_zero(const char *data)
{
	if (!data || (*data == '\0')) {
		return TRUE;
	}

	return FALSE;
}

/*!
 * \brief SCCP version of strlen
 * \param data String to be checked
 * \return length as int
 */
size_t sccp_strlen(const char *data)
{
	if (!data || (*data == '\0')) {
		return 0;
	}
	return strlen(data);
}

/*!
 * \brief SCCP version of strequals
 * \note Takes into account zero length strings, if both strings are zero length returns TRUE
 * \param data1 String to be checked
 * \param data2 String to be checked
 * \return !strcmp as boolean_t
 *
 * \retval booleant_t on !strcmp
 * \retval TRUE on both zero length
 * \retval FALSE on one of the the parameters being zero length
 */
boolean_t sccp_strequals(const char *data1, const char *data2)
{
	if (sccp_strlen_zero(data1) && sccp_strlen_zero(data2)) {
		return TRUE;
	} else if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2)) {
		return !strcmp(data1, data2);
	}
	return FALSE;
}

/*!
 * \brief SCCP version of strcaseequals
 * \note Takes into account zero length strings, if both strings are zero length returns TRUE
 * \param data1 String to be checked
 * \param data2 String to be checked
 * \return !strcasecmp as boolean_t
 *
 * \retval boolean_t on strcaseequals
 * \retval TRUE on both zero length
 * \retval FALSE on one of the the parameters being zero length
 */
boolean_t sccp_strcaseequals(const char *data1, const char *data2)
{
	if (sccp_strlen_zero(data1) && sccp_strlen_zero(data2)) {
		return TRUE;
	} else if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2)) {
		return !strcasecmp(data1, data2);
	}
	return FALSE;
}

int sccp_strIsNumeric(const char *s)
{
	if (*s) {
		char c;

		while ((c = *s++)) {
			if (!isdigit(c))
				return 0;
		}
		return 1;
	}
	return 0;
}

/*!
 * \brief Find the best codec match Between Preferences, Capabilities and RemotePeerCapabilities
 * 
 * Returns:
 *  - Best Match If Found
 *  - If not it returns the first jointCapability
 *  - Else SKINNY_CODEC_NONE
 */
skinny_codec_t sccp_utils_findBestCodec(const skinny_codec_t ourPreferences[], int pLength, const skinny_codec_t ourCapabilities[], int cLength, const skinny_codec_t remotePeerCapabilities[], int rLength)
{
	uint8_t r, c, p;
	skinny_codec_t firstJointCapability = SKINNY_CODEC_NONE;						/*!< used to get a default value */

	sccp_log((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "pLength %d, cLength: %d, rLength: %d\n", pLength, cLength, rLength);

	/** check if we have a preference codec list */
	if ((pLength == 0 || ourPreferences[0] == SKINNY_CODEC_NONE)
	    && (rLength != 0 && remotePeerCapabilities[0] != SKINNY_CODEC_NONE)) {
		/* using remote capabilities to */
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "We got an empty preference codec list (exiting)\n");
		return SKINNY_CODEC_NONE;

	} else if (pLength == 0 || ourPreferences[0] == SKINNY_CODEC_NONE) {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "We got an empty preference codec list (exiting)\n");
		return 0;
	}

	/* iterate over our codec preferences */
	for (p = 0; p < pLength; p++) {
		if (ourPreferences[p] == SKINNY_CODEC_NONE) {
			sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "no more preferences at position %d\n", p);
			break;
		}
		/* no more preferences */
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "preference: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));

		/* check if we are capable to handle this codec */
		for (c = 0; c < cLength; c++) {
			if (ourCapabilities[c] == SKINNY_CODEC_NONE) {
				/* we reached the end of valide codecs, because we found the first NONE codec */
				break;
			}

			sccp_log((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]));

			/* we have no capabilities from the remote party, use the best codec from ourPreferences */
			if (ourPreferences[p] == ourCapabilities[c]) {
				if (firstJointCapability == SKINNY_CODEC_NONE) {
					firstJointCapability = ourPreferences[p];
					sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "found first firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
				}

				if (rLength == 0 || remotePeerCapabilities[0] == SKINNY_CODEC_NONE) {
					sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "Empty remote Capabilities, using bestCodec from firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
					return firstJointCapability;
				} else {
					/* using capabilities from remote party, that matches our preferences & capabilities */
					for (r = 0; r < rLength; r++) {
						if (remotePeerCapabilities[r] == SKINNY_CODEC_NONE) {
							break;
						}
						sccp_log((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s), remoteCapability: " UI64FMT "(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]), (ULONG) remotePeerCapabilities[r], codec2name(remotePeerCapabilities[r]));
						if (ourPreferences[p] == remotePeerCapabilities[r]) {
							sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "found bestCodec as joint capability with remote peer %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));
							return ourPreferences[p];
						}
					}
				}
			}
		}
	}

	if (firstJointCapability != SKINNY_CODEC_NONE) {
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "did not find joint capability with remote device, using first joint capability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
		return firstJointCapability;
	}

	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "no joint capability with preference codec list\n");
	return 0;
}

/*!
 * \brief Free a list of Host Access Rules
 * \param ha The head of the list of HAs to free
 * \retval void
 */
void sccp_free_ha(struct sccp_ha *ha)
{
	struct sccp_ha *hal;

	while (ha) {
		hal = ha;
		ha = ha->next;
		sccp_free(hal);
	}
}

/*!
 * \brief Apply a set of rules to a given IP address
 *
 * \param ha The head of the list of host access rules to follow
 * \param sin A sockaddr_in whose address is considered when matching rules
 * \retval AST_SENSE_ALLOW The IP address passes our ACL
 * \retval AST_SENSE_DENY The IP address fails our ACL  
 */
int sccp_apply_ha(struct sccp_ha *ha, struct sockaddr_in *sin)
{
	/* Start optimistic */
	int res = AST_SENSE_DENY;

	while (ha) {
		/* For each rule, if this address and the netmask = the net address
		   apply the current rule */
		if ((sin->sin_addr.s_addr & ha->netmask.s_addr) == ha->netaddr.s_addr) {
			res = ha->sense;
		}
		ha = ha->next;
	}
	return res;
}

/*!
 * \brief Add a new rule to a list of HAs
 *
 * \param sense Either "permit" or "deny" (Actually any 'p' word will result
 * in permission, and any other word will result in denial)
 * \param stuff The IP address and subnet mask, separated with a '/'. The subnet
 * mask can either be in dotted-decimal format or in CIDR notation (i.e. 0-32).
 * \param path The head of the HA list to which we wish to append our new rule. If
 * NULL is passed, then the new rule will become the head of the list
 * \param[out] error The integer error points to will be set non-zero if an error occurs
 * \return The head of the HA list
 */
struct sccp_ha *sccp_append_ha(const char *sense, const char *stuff, struct sccp_ha *path, int *error)
{
	struct sccp_ha *ha;
	char *nm;
	struct sccp_ha *prev = NULL;
	struct sccp_ha *ret;
	int x;
	char *tmp = sccp_strdupa(stuff);

	ret = path;
	while (path) {
		prev = path;
		path = path->next;
	}

	if (!(ha = sccp_calloc(1, sizeof(*ha)))) {
		if (error) {
			*error = 1;
		}
		return ret;
	}
	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "start parsing ha sense: %s, stuff: %s \n", sense, stuff);
	if (!(nm = strchr(tmp, '/'))) {
		/* assume /32. Yes, htonl does not do anything for this particular mask
		   but we better use it to show we remember about byte order */
		ha->netmask.s_addr = htonl(0xFFFFFFFF);
	} else {
		*nm = '\0';
		nm++;

		if (!strchr(nm, '.')) {
			if ((sscanf(nm, "%30d", &x) == 1) && (x >= 0) && (x <= 32)) {
				if (x == 0) {
					/* This is special-cased to prevent unpredictable
					 * behavior of shifting left 32 bits
					 */
					ha->netmask.s_addr = 0;
				} else {
					ha->netmask.s_addr = htonl(0xFFFFFFFF << (32 - x));
				}
			} else {
				sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "Invalid CIDR in %s\n", stuff);
				sccp_free(ha);
				if (error) {
					*error = 1;
				}
				return ret;
			}
		} else if (!inet_aton(nm, &ha->netmask)) {
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "Invalid mask in %s\n", stuff);
			sccp_free(ha);
			if (error) {
				*error = 1;
			}
			return ret;
		}
	}

	if (!inet_aton(tmp, &ha->netaddr)) {
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "Invalid IP address in %s\n", stuff);
		sccp_free(ha);
		if (error) {
			*error = 1;
		}
		return ret;
	}
	ha->netaddr.s_addr &= ha->netmask.s_addr;

	ha->sense = strncasecmp(sense, "p", 1) ? AST_SENSE_DENY : AST_SENSE_ALLOW;

	ha->next = NULL;
	if (prev) {
		prev->next = ha;
	} else {
		ret = ha;
	}

	return ret;
}

void sccp_print_ha(struct ast_str *buf, int buflen, struct sccp_ha *path)
{
	char str1[INET_ADDRSTRLEN];
	char str2[INET_ADDRSTRLEN];

	while (path) {
		inet_ntop(AF_INET, &path->netaddr.s_addr, str1, INET_ADDRSTRLEN);
		inet_ntop(AF_INET, &path->netmask.s_addr, str2, INET_ADDRSTRLEN);
		pbx_str_append(&buf, buflen, "%s:%s/%s,", AST_SENSE_DENY == path->sense ? "deny" : "permit", str1, str2);
		path = path->next;
	}
}

/*!
 * \brief Yields string representation from channel (for debug).
 * \param c SCCP channel
 * \return string constant (on the heap!)
 */
const char *sccp_channel_toString(sccp_channel_t * c)
{
	if (c && c->designator)
		return (const char *) c->designator;
	else
		return "";
}

/*!
 * \brief Print Group
 * \param buf Buf as char
 * \param buflen Buffer Lendth as int
 * \param group Group as sccp_group_t
 * \return Result as char
 */
void sccp_print_group(struct ast_str *buf, int buflen, sccp_group_t group)
{
	unsigned int i;
	int first = 1;
	uint8_t max = (sizeof(sccp_group_t) * 8) - 1;

	if (!group)
		return;

	for (i = 0; i <= max; i++) {
		if (group & ((sccp_group_t) 1 << i)) {
			if (!first) {
				pbx_str_append(&buf, buflen, ",");
			} else {
				first = 0;
			}
			pbx_str_append(&buf, buflen, "%d", i);
		}
	}
	return;
}

/*!
 * \brief Compare two socket addressed with each other
 */
int sockaddr_cmp_addr(struct sockaddr_storage *addr1, socklen_t len1, struct sockaddr_storage *addr2, socklen_t len2)
{
	struct sockaddr_in *p1_in = (struct sockaddr_in *) addr1;
	struct sockaddr_in *p2_in = (struct sockaddr_in *) addr2;
	struct sockaddr_in6 *p1_in6 = (struct sockaddr_in6 *) addr1;
	struct sockaddr_in6 *p2_in6 = (struct sockaddr_in6 *) addr2;

	if (len1 < len2)
		return -1;
	if (len1 > len2)
		return 1;
	if (p1_in->sin_family < p2_in->sin_family)
		return -1;
	if (p1_in->sin_family > p2_in->sin_family)
		return 1;
	/* compare ip4 */
	if (p1_in->sin_family == AF_INET) {
		return memcmp(&p1_in->sin_addr, &p2_in->sin_addr, INET_ADDRSTRLEN);
	} else if (p1_in6->sin6_family == AF_INET6) {
		return memcmp(&p1_in6->sin6_addr, &p2_in6->sin6_addr, INET6_ADDRSTRLEN);
	} else {
		/* unknown type, compare for sanity. */
		return memcmp(addr1, addr2, len1);
	}
}

int sccp_strversioncmp(const char *s1, const char *s2)
{
	static const char *digits = "0123456789";
	int ret, lz1, lz2;
	size_t p1, p2;

	p1 = strcspn(s1, digits);
	p2 = strcspn(s2, digits);
	while (p1 == p2 && s1[p1] != '\0' && s2[p2] != '\0') {
		/* Different prefix */
		if ((ret = strncmp(s1, s2, p1)) != 0)
			return ret;

		s1 += p1;
		s2 += p2;

		lz1 = lz2 = 0;
		if (*s1 == '0')
			lz1 = 1;
		if (*s2 == '0')
			lz2 = 1;

		if (lz1 > lz2)
			return -1;
		else if (lz1 < lz2)
			return 1;
		else if (lz1 == 1) {
			/*
			 * If the common prefix for s1 and s2 consists only of zeros, then the
			 * "longer" number has to compare less. Otherwise the comparison needs
			 * to be numerical (just fallthrough). See
			 */
			while (*s1 == '0' && *s2 == '0') {
				++s1;
				++s2;
			}

			p1 = strspn(s1, digits);
			p2 = strspn(s2, digits);

			/* Catch empty strings */
			if (p1 == 0 && p2 > 0)
				return 1;
			else if (p2 == 0 && p1 > 0)
				return -1;

			/* Prefixes are not same */
			if (*s1 != *s2 && *s1 != '0' && *s2 != '0') {
				if (p1 < p2)
					return 1;
				else if (p1 > p2)
					return -1;
			} else {
				if (p1 < p2)
					ret = strncmp(s1, s2, p1);
				else if (p1 > p2)
					ret = strncmp(s1, s2, p2);
				if (ret != 0)
					return ret;
			}
		}

		p1 = strspn(s1, digits);
		p2 = strspn(s2, digits);

		if (p1 < p2)
			return -1;
		else if (p1 > p2)
			return 1;
		else if ((ret = strncmp(s1, s2, p1)) != 0)
			return ret;

		/* Numbers are equal or not present, try with next ones. */
		s1 += p1;
		s2 += p2;
		p1 = strcspn(s1, digits);
		p2 = strcspn(s2, digits);
	}

	return strcmp(s1, s2);
}
