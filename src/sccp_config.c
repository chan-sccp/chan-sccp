/*!
 * \file 	sccp_config.c
 * \brief 	SCCP Config Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \todo using generic function to configure structures, this can also be used to reconfigure structure on-line
 * \version $LastChangedDate$
 */
#include "config.h"

#include "asterisk.h"
#include "asterisk/channel.h"

#include "chan_sccp.h"
#include "sccp_config.h"
#include "sccp_utils.h"
#include "sccp_event.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include <asterisk/astdb.h>

#ifdef CS_AST_HAS_EVENT
#include "sccp_mwi.h"
#include "asterisk/event.h"
#endif



struct ast_config *sccp_config_getConfig(void);


/*!
 * \brief Add Line to device.
 * \param device - Device
 * \param lineName - Name of line
 * \param index - preferred button (position)
 */
void sccp_config_addLine(sccp_device_t *device, char *lineName, uint8_t index) {
	sccp_buttonconfig_t	*config;

	config = ast_malloc(sizeof(sccp_buttonconfig_t));
	memset(config, 0, sizeof(sccp_buttonconfig_t));
	if(!config)
		return;

	ast_strip(lineName);
	config->instance = index;
	sccp_log(0)(VERBOSE_PREFIX_3 "Add line button on position: %d\n", config->instance);
	//TODO check already existing instances
	if (ast_strlen_zero(lineName)) {
		config->type = EMPTY;
	}else{
		config->type = LINE;
		sccp_copy_string(config->button.line.name, lineName, sizeof(config->button.line.name));
	}
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfLines++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);

}

/**
 * Add an Empty Button to device.
 * \param device SCCP Device
 * \param index Index Preferred Button Position as int
 */
void sccp_config_addEmpty(sccp_device_t *device, uint8_t index)
{
        sccp_buttonconfig_t	*config;

	config = ast_malloc(sizeof(sccp_buttonconfig_t));
	memset(config, 0, sizeof(sccp_buttonconfig_t));
	if(!config)
		return;

	config->instance = index;
	sccp_log(0)(VERBOSE_PREFIX_3 "Add empty button on position: %d\n", config->instance);
	config->type = EMPTY;
	//TODO check already existing instances

	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/*!
 * Add an SpeedDial to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param extension Extension as char
 * \param hint Hint as char
 * \param index Index Preferred Button Position as int
 */
void sccp_config_addSpeeddial(sccp_device_t *device, char *label, char *extension, char *hint, uint8_t index){
	sccp_buttonconfig_t	*config;

	config = ast_malloc(sizeof(sccp_buttonconfig_t));
	memset(config, 0, sizeof(sccp_buttonconfig_t));


	//TODO check already existing instances
	config->instance = index;
	config->type = SPEEDDIAL;

	sccp_copy_string(config->button.speeddial.label, ast_strip(label), sizeof(config->button.speeddial.label));
	sccp_copy_string(config->button.speeddial.ext, ast_strip(extension), sizeof(config->button.speeddial.ext));
	if(hint){
		sccp_copy_string(config->button.speeddial.hint, ast_strip(hint), sizeof(config->button.speeddial.hint));
	}
	sccp_log(0)(VERBOSE_PREFIX_3 "Add SPEEDDIAL button on position: %d\n", config->instance);


	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfSpeeddials++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}

/**
 * Add an SpeedDial to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param extension Extension as char
 * \param hint Hint as char
 * \param index Index Preferred Button Position as int
 */
void sccp_config_addFeature(sccp_device_t *device, char *label, char *featureID, char *args, uint8_t index){
	sccp_buttonconfig_t	*config;

	config = ast_malloc(sizeof(sccp_buttonconfig_t));
	memset(config, 0, sizeof(sccp_buttonconfig_t));


	//TODO check already existing instances
	config->instance = index;
	if (ast_strlen_zero(label)) {
		config->type = EMPTY;
	}else{
		config->type = FEATURE;


		sccp_copy_string(config->button.feature.label, ast_strip(label), sizeof(config->button.feature.label));
		//sccp_copy_string(config->button.feature.id, (!ast_strlen_zero(featureID))?ast_strip(featureID):NULL, sizeof(config->button.feature.options));
		sccp_log(10)(VERBOSE_PREFIX_3 "featureID: %s\n", featureID);
		config->button.feature.id = sccp_featureStr2featureID(featureID);
		if(args)
			sccp_copy_string(config->button.feature.options, (args)?ast_strip(args):"", sizeof(config->button.feature.options));

		sccp_log(0)(VERBOSE_PREFIX_3 "Add FEATURE button on position: %d, featureID: %d, args: %s\n", config->instance, config->button.feature.id, (config->button.feature.options)?config->button.feature.options:"(none)");
	}

	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
	device->configurationStatistic.numberOfFeatures++;
	SCCP_LIST_UNLOCK(&device->buttonconfig);
}


/**
 * Add an Service to Device.
 * \param device SCCP Device
 * \param label Label as char
 * \param url URL as char
 * \param index Index Preferred Button Position as int
 */
void sccp_config_addService(sccp_device_t *device, char *label, char *url, uint8_t index)
{
        sccp_buttonconfig_t	*config;

        config = ast_malloc(sizeof(sccp_buttonconfig_t));
        memset(config, 0, sizeof(sccp_buttonconfig_t));


        //TODO check already existing instances
        config->instance = index;

        if (ast_strlen_zero(label) || ast_strlen_zero(url)) {
                config->type = EMPTY;
        } else {
                config->type = SERVICE;

                sccp_copy_string(config->button.service.label, ast_strip(label), sizeof(config->button.service.label));
                sccp_copy_string(config->button.service.url, ast_strip(url), sizeof(config->button.service.url));

                sccp_log(0)(VERBOSE_PREFIX_3 "Add SERVICE button on position: %d\n", config->instance);
        }

        SCCP_LIST_LOCK(&device->buttonconfig);

        SCCP_LIST_INSERT_TAIL(&device->buttonconfig, config, list);
        device->configurationStatistic.numberOfServices++;
        SCCP_LIST_UNLOCK(&device->buttonconfig);
}



/**
 * Create a device from configuration variable.
 *
 * \param variable asterisk configuration variable
 * \param deviceName name of device e.g. SEP0123455689
 * \param isRealtime configuration is generated by realtime
 * \since 10.01.2008 - branche V3
 * \author Marcello Ceschia
 */
sccp_device_t *sccp_config_buildDevice(struct ast_variable *variable, const char *deviceName, boolean_t isRealtime){
	sccp_device_t 	*d = NULL;
	int		res;
	char 		message[256]=""; /*!< device message */

	// Try to find out if we have the device already on file.
	// However, do not look into realtime, since
	// we might have been asked to create a device for realtime addition,
	// thus causing an infinite loop / recursion.
	d = sccp_device_find_byid(deviceName, FALSE);
	if (d) {
		ast_log(LOG_WARNING, "SCCP: Device '%s' already exists\n", deviceName);
		return d;
	} 

	
	//d = build_devices_wo(variable);
	
	/* create new device with default values */
	d= sccp_device_create();
	d= sccp_config_applyDeviceConfiguration(d, variable); /* apply configuration using variable */
	
	memset(d->id, 0, sizeof(d->id));
	sccp_copy_string(d->id, deviceName, sizeof(d->id));/* set device name */
#ifdef CS_SCCP_REALTIME
	d->realtime = isRealtime;
#endif

	res = ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
	if (!res) {
		d->phonemessage=strdup(message);						//set message on device if we have a result
	}

	// TODO: Load status of feature (DND, CFwd, etc.) from astdb.

	sccp_device_addToGlobals(d);

	return d;
}

/*!
 * \brief Add Line to device.
 * \param device - Device
 * \param lineName - Name of line
 * \param index - preferred button (position)
 */
sccp_line_t *sccp_config_buildLine(struct ast_variable *variable, const char *lineName, boolean_t isRealtime){
	sccp_line_t 	*line = NULL;

	// Try to find out if we have the line already on file.
	// However, do not look into realtime, since
	// we might have been asked to create a device for realtime addition,
	// thus causing an infinite loop / recursion.
	line = sccp_line_find_byname_wo(lineName, FALSE);


	/* search for existing line */
	if (line) {
		ast_log(LOG_WARNING, "SCCP: Line '%s' already exists\n", lineName);
		return line;
	} 

	//line = build_lines_wo(variable);
	line = sccp_line_create();
        line = sccp_config_applyLineConfiguration(line, variable);
	
	sccp_copy_string(line->name, lineName, sizeof(line->name));

	
	
	sccp_copy_string(line->name, ast_strip((char *)lineName), sizeof(line->name));
#ifdef CS_SCCP_REALTIME
	line->realtime = isRealtime;
#endif

	// TODO: Load status of feature (DND, CFwd, etc.) from astdb.

	sccp_line_addToGlobals(line);
	return line;
}

boolean_t sccp_config_general(void){
	struct ast_config		*cfg;
	struct ast_variable		*v;
	int firstdigittimeout = 0;
	int digittimeout = 0;
	int autoanswer_ring_time = 0;
	int autoanswer_tone = 0;
	int remotehangup_tone = 0;
	int transfer_tone = 0;
	int callwaiting_tone = 0;
	int amaflags = 0;
	int protocolversion = 0;
	char digittimeoutchar = '#';
	int				tos= 0;
	char				pref_buf[128];
	struct ast_hostent		ahp;
	struct hostent			*hp;
	struct ast_ha 			*na;



	cfg = sccp_config_getConfig();
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return FALSE;
	}

	/* read the general section */
	v = ast_variable_browse(cfg, "general");
	if (!v) {
		ast_log(LOG_WARNING, "Missing [general] section, SCCP disabled\n");
		return FALSE;
	}

	while (v) {
#ifndef ASTERISK_CONF_1_2
			/* handle jb in configuration just let asterisk do that */
			if (!ast_jb_read_conf(&GLOB(global_jbconf), v->name, v->value)){
				// Found a jb parameter
			}
#endif

			if (!strcasecmp(v->name, "protocolversion")) {
				if (sscanf(v->value, "%i", &protocolversion) == 1) {
					if (protocolversion < SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW) {
						GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
					} else if (protocolversion > SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH) {
						GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
					} else {
						GLOB(protocolversion) = protocolversion;
					}
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

#ifndef	ASTERISK_CONF_1_6
				GLOB(ha) = ast_append_ha(v->name, v->value, GLOB(ha));
#else
				GLOB(ha) = ast_append_ha(v->name, v->value, GLOB(ha), NULL);
#endif

			} else if (!strcasecmp(v->name, "localnet")) {

#ifndef	ASTERISK_CONF_1_6
				if (!(na = ast_append_ha("d", v->value, GLOB(localaddr))))
#else
				if (!(na = ast_append_ha("d", v->value, GLOB(localaddr), NULL)))
#endif
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
			} else if (!strcasecmp(v->name, "nat")) {
				GLOB(nat) = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "directrtp")) {
				GLOB(directrtp) = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "allowoverlap")) {
				GLOB(useoverlap) = sccp_true(v->value);
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
				if(!ast_strlen_zero(v->value))
					GLOB(digittimeoutchar) = v->value[0];
				else
					GLOB(digittimeoutchar) = digittimeoutchar;
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
			} else if (!strcasecmp(v->name, "cfwdnoanswer")) {
				GLOB(cfwdnoanswer) = sccp_true(v->value);
#ifdef CS_MANAGER_EVENTS
			} else if (!strcasecmp(v->name, "callevents")) {
				GLOB(callevents) = sccp_true(v->value);
#endif
			} else if (!strcasecmp(v->name, "echocancel")) {
				GLOB(echocancel) = sccp_true(v->value);
#ifdef CS_SCCP_PICKUP
			} else if (!strcasecmp(v->name, "pickupmodeanswer")) {
				GLOB(pickupmodeanswer) = sccp_true(v->value);
#endif
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
			} else if (!strcasecmp(v->name, "callanswerorder")) {
				if (!strcasecmp(v->value, "oldestfirst"))
					GLOB(callAnswerOrder) = ANSWER_OLDEST_FIRST;
				else
					GLOB(callAnswerOrder) = ANSWER_LAST_FIRST;
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
			} else if (!strcasecmp(v->name, "hotline_enabled")) {
					GLOB(allowAnonymus) = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "hotline_context")) {
					sccp_copy_string(GLOB(hotline)->line->context , v->value, sizeof(GLOB(hotline)->line->context) );
			} else if (!strcasecmp(v->name, "hotline_extension")) {
					sccp_copy_string(GLOB(hotline)->exten , v->value, sizeof(GLOB(hotline)->exten) );
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
		//GLOB(bindaddr.sin_family) = AF_UNSPEC;


	ast_config_destroy(cfg);
	return TRUE;
}
/**
 * \brief Read Lines from the Config File
 *
 * \param readingtype as SCCP Reading Type
 * \since 10.01.2008 - branche V3
 * \author Marcello Ceschia
 */
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype)
{
	struct ast_config		*cfg = NULL;
	char 					*cat = NULL;
	struct ast_variable		*v = NULL;
	uint8_t device_count=0;
	uint8_t line_count=0;

        ast_log(LOG_NOTICE, "Loading Devices and Lines from config\n");

	cfg = sccp_config_getConfig();
	if (!cfg) {
		ast_log(LOG_NOTICE, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}

        if (!cfg) {
                ast_log(LOG_NOTICE, "Unable to load config file sccp.conf, SCCP disabled\n");
                return;
        }
        
        while ( ( cat = ast_category_browse(cfg,cat)) ){
        
                const char *utype;
                if (!strcasecmp(cat, "general"))
                        continue;

	        ast_log(LOG_NOTICE, "Parsing category: %s\n", cat);
                utype = ast_variable_retrieve(cfg, cat, "type");
                
                if (!utype) {
                        ast_log(LOG_WARNING, "Section '%s' lacks type\n", cat);
                        continue;
                } else if ( !strcasecmp(utype,"device") ){
                        // check minimum requirements for a device
                        if ( ast_strlen_zero( ast_variable_retrieve(cfg, cat, "devicetype") ) ) {
                                ast_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
                                continue;
                        } else {
				v = ast_variable_browse(cfg, cat);
				sccp_config_buildDevice(v, cat, FALSE);
				device_count++;
				ast_verbose(VERBOSE_PREFIX_3 "found device %d: %s\n", device_count, cat);
                        }
                } else if ( !strcasecmp(utype,"line") ) {
                        /* check minimum requirements for a line */
                        if ( !ast_strlen_zero( ast_variable_retrieve(cfg, cat, "label") ) ) {
                        	;
				//TODO why are these params required? - MC
                        } else if ( !ast_strlen_zero( ast_variable_retrieve(cfg, cat, "pin") ) ) {
                        	;
                        } else if ( !ast_strlen_zero( ast_variable_retrieve(cfg, cat, "cid_name") ) ) {
                        	;
                        } else if ( !ast_strlen_zero( ast_variable_retrieve(cfg, cat, "cid_num") ) ) {
                        	;
                        } else {
                                ast_log(LOG_WARNING, "Unknown type '%s' for '%s' in %s\n", utype, cat, "sccp.conf");
                                continue;
			}
			line_count++;
			v = ast_variable_browse(cfg, cat);
			sccp_config_buildLine(v, cat, FALSE);
			ast_verbose(VERBOSE_PREFIX_3 "found line %d: %s\n", line_count, cat);
		} else if ( !strcasecmp(utype,"softkeyset") ) {
			ast_verbose(VERBOSE_PREFIX_3 "read set %s\n", cat);
			v = ast_variable_browse(cfg, cat);
			sccp_config_softKeySet(v, cat);
                }
        }
        ast_config_destroy(cfg);
}




/**
 * \brief Get Configured Line from Asterisk Variable
 * \param l SCCP Line
 * \param v Asterisk Variable
 * \return Configured SCCP Line
 * \note also used by realtime functionality to line device from Asterisk Variable
 */
sccp_line_t *sccp_config_applyLineConfiguration(sccp_line_t *l, struct ast_variable *v){
        int amaflags = 0;
        int secondary_dialtone_tone = 0;


        while (v) {
                if ((!strcasecmp(v->name, "line") )
#ifdef CS_SCCP_REALTIME
                                || (!strcasecmp(v->name, "name"))
#endif
                   ) {
                } else if (!strcasecmp(v->name, "type")) {
		  //skip;
                } else if (!strcasecmp(v->name, "id")) {
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
                        sccp_mailbox_t *mailbox = NULL;
                        char *context, *mbox = NULL;

                        mbox = context = ast_strdupa(v->value);

                        if (ast_strlen_zero(mbox))
                                continue;

                        if (!(mailbox = ast_calloc(1, sizeof(*mailbox))))
                                continue;


                        strsep(&context, "@");
                        mailbox->mailbox = ast_strdup(mbox);
                        mailbox->context = ast_strdup(context);

                        SCCP_LIST_INSERT_TAIL(&l->mailboxes, mailbox, list);
                        sccp_log(10)(VERBOSE_PREFIX_3 "%s: Added mailbox '%s@%s'\n", l->name, mailbox->mailbox, (mailbox->context)?mailbox->context:"default");

                        //sccp_copy_string(l->mailbox, v->value, sizeof(l->mailbox));
                } else if (!strcasecmp(v->name, "vmnum")) {
                        sccp_copy_string(l->vmnum, v->value, sizeof(l->vmnum));
                } else if (!strcasecmp(v->name, "meetmenum")) {
                        sccp_copy_string(l->meetmenum, v->value, sizeof(l->meetmenum));
                } else if (!strcasecmp(v->name, "transfer")) {
                        l->transfer = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "incominglimit")) {
                        l->incominglimit = atoi(v->value);

                        if (l->incominglimit < 1)
                                l->incominglimit = 1;

                        /* this is the max call phone limits. Just a guess. It's not important */
                        if (l->incominglimit > 99)
                                l->incominglimit = 99;
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
                                l->trnsfvm = strdup(v->value);
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

                        if (newvar) {
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
 * \brief Apply Device Configuration from Asterisk Variable
 * \param d SCCP Device
 * \param v Asterisk Variable
 * \return Configured SCCP Device
 * \note also used by realtime functionality to line device from Asterisk Variable
 * \todo this function should be called sccp_config_applyDeviceConfiguration
 */
sccp_device_t *sccp_config_applyDeviceConfiguration(sccp_device_t *d, struct ast_variable *v){
        char 			message[256]="";//device message
        int				res;

        /* for button config */
        char 			*buttonType = NULL, *buttonName = NULL, *buttonOption=NULL, *buttonArgs=NULL;
        char 			k_button[256];
        uint8_t			instance=0;
        char 			*splitter;

        if (!v) {
                sccp_log(10)(VERBOSE_PREFIX_3 "no variable given\n");
                return d;
        }

        while (v) {
                sccp_log(1)(VERBOSE_PREFIX_3 "%s = %s\n", v->name, v->value);

                if ((!strcasecmp(v->name, "device"))
#ifdef CS_SCCP_REALTIME
                                || (!strcasecmp(v->name, "name"))
#endif
                   ) {

                } else if (!strcasecmp(v->name, "keepalive")) {
                        d->keepalive = atoi(v->value);
                } else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
#ifndef ASTERISK_CONF_1_6
                        d->ha = ast_append_ha(v->name, v->value, d->ha);
#else
                        d->ha = ast_append_ha(v->name, v->value, d->ha, NULL );
#endif
                } else if (!strcasecmp(v->name, "button")) {
                        sccp_log(0)(VERBOSE_PREFIX_3 "Found buttonconfig: %s\n", v->value);
                        sccp_copy_string(k_button, v->value, sizeof(k_button));
                        splitter = k_button;
                        buttonType = strsep(&splitter, ",");
                        buttonName = strsep(&splitter, ",");
                        buttonOption = strsep(&splitter, ",");
                        buttonArgs = splitter;

                        sccp_log(99)(VERBOSE_PREFIX_3 "ButtonType: %s\n", buttonType);
                        sccp_log(99)(VERBOSE_PREFIX_3 "ButtonName: %s\n", buttonName);
                        sccp_log(99)(VERBOSE_PREFIX_3 "ButtonOption: %s\n", buttonOption);

                        if (!strcasecmp(buttonType, "line") && buttonName) {
                                if (!buttonName)
                                        continue;

                                sccp_config_addLine(d, (buttonName)?ast_strip(buttonName):NULL, ++instance);

                                if (buttonOption && !strcasecmp(buttonOption, "default")) {
                                        d->defaultLineInstance = instance;
                                        sccp_log(99)(VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", instance);
                                }
                        } else if (!strcasecmp(buttonType, "empty")) {
                                sccp_config_addEmpty(d, ++instance);
                        } else if (!strcasecmp(buttonType, "speeddial")) {
                                sccp_config_addSpeeddial(d, (buttonName)?ast_strip(buttonName):"speeddial", (buttonOption)?ast_strip(buttonOption):NULL, (buttonArgs)?ast_strip(buttonArgs):NULL, ++instance);
                        } else if (!strcasecmp(buttonType, "feature") && buttonName) {
                                sccp_config_addFeature(d, (buttonName)?ast_strip(buttonName):"feature", (buttonOption)?ast_strip(buttonOption):NULL, buttonArgs, ++instance );
                        } else if (!strcasecmp(buttonType, "service") && buttonName && !ast_strlen_zero(buttonName) ) {
                                sccp_config_addService(d, (buttonName)?ast_strip(buttonName):"service", (buttonOption)?ast_strip(buttonOption):NULL, ++instance);
                        }
                } else if (!strcasecmp(v->name, "permithost")) {
                        sccp_permithost_addnew(d, v->value);
		} else if ((!strcasecmp(v->name, "type")) || !strcasecmp(v->name, "devicetype")){
			if (!strcasecmp(v->value, "device")){
				sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
			}
                } else if (!strcasecmp(v->name, "type")) {
                        //sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
			//skip
                } else if (!strcasecmp(v->name, "addon")) {
                        sccp_addon_addnew(d, v->value);
                } else if (!strcasecmp(v->name, "tzoffset")) {
                        /* timezone offset */
                        d->tz_offset = atoi(v->value);
                } else if (!strcasecmp(v->name, "autologin")) {
                        char *mb, *cur, tmp[256];

                        sccp_copy_string(tmp, v->value, sizeof(tmp));
                        mb = tmp;

                        while ((cur = strsep(&mb, ","))) {
                                ast_strip(cur);

                                if (ast_strlen_zero(cur)) {
                                        sccp_config_addEmpty(d, ++instance);
                                        continue;
                                }

                                sccp_log(10)(VERBOSE_PREFIX_3 "%s: Auto logging into %s\n", d->id, cur);

                                sccp_config_addLine(d, cur, ++instance);
                        }

                        //sccp_copy_string(d->autologin, v->value, sizeof(d->autologin));
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
                } else if (!strcasecmp(v->name, "cfwdnoanswer")) {
                        d->cfwdnoanswer = sccp_true(v->value);
#ifdef CS_SCCP_PICKUP
                } else if (!strcasecmp(v->name, "pickupexten")) {
                        d->pickupexten = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "pickupcontext")) {
                        if (!ast_strlen_zero(v->value))
                                d->pickupcontext = strdup(v->value);
                } else if (!strcasecmp(v->name, "pickupmodeanswer")) {
                        d->pickupmodeanswer = sccp_true(v->value);
#endif
                } else if (!strcasecmp(v->name, "dnd")) {
                        /* 0 is off and 1 (on) is reject */
                        //d->dndFeature.enabled = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "nat")) {
                        d->nat = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "directrtp")) {
                        d->directrtp = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "allowoverlap")) {
                        d->overlapFeature.enabled = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "trustphoneip")) {
                        d->trustphoneip = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "private")) {
                        d->privacyFeature.enabled = sccp_true(v->value);
                } else if (!strcasecmp(v->name, "privacy")) {
                        if (!strcasecmp(v->value, "full")) {
                                d->privacyFeature.status = 0;
                                d->privacyFeature.status = ~d->privacyFeature.status;
                                //d->privacyFeature.status = 0xFFFFFFFF;
                        } else {
                                d->privacyFeature.status = 0;
                                //d->privacyFeature.enabled = sccp_true(v->value);
                        }
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
                                sccp_config_addEmpty(d, ++instance);
                        } else {
                                //					//sccp_speeddial_addnew(d, v->value, speeddial_index);
                                sccp_copy_string(k_button, v->value, sizeof(k_button));
                                splitter = k_button;
                                buttonType = strsep(&splitter, ",");
                                buttonName = strsep(&splitter, ",");

                                if (splitter)
                                        buttonOption = strsep(&splitter, ",");
                                else
                                        buttonOption = NULL;

                                //
                                sccp_config_addSpeeddial(d, ast_strip(buttonName), ast_strip(buttonType), (buttonOption)?ast_strip(buttonOption):NULL, ++instance);
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
                } else if (!strcasecmp(v->name, "setvar")) {

                        struct ast_variable *newvar = NULL;
                        newvar = sccp_create_variable(v->value);

                        if (newvar) {
                                sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);
                                newvar->next = d->variables;
                                d->variables = newvar;
                        }
                } else if (!strcasecmp(v->name, "serviceURL")) {
                        //sccp_serviceURL_addnew(d, v->value, serviceURLIndex);
                        ast_log(LOG_WARNING, "SCCP: Service: %s\n", v->value);

                        if (ast_strlen_zero(v->value))
                                sccp_config_addEmpty(d, ++instance);
                        else {
                                sccp_copy_string(k_button, v->value, sizeof(k_button));
                                splitter = k_button;
                                buttonType = strsep(&splitter, ",");
                                buttonName = strsep(&splitter, ",");


                                sccp_config_addService(d, ast_strip(buttonType), ast_strip(buttonName), ++instance);
                        }
                } else {
                        ast_log(LOG_WARNING, "SCCP: Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
                }

                v = v->next;
        }

        res=ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db

        if (!res)
                d->phonemessage=strdup(message);									//set message on device if we have a result

        strcpy(message,"");

        sccp_dev_dbget(d); /* load saved settings from ast db */

        return d;
}



/**
 * \brief Find the Correct Config File
 * \return Asterisk Config
 */

struct ast_config *sccp_config_getConfig() {
	struct ast_config *cfg = NULL;
	
#ifndef ASTERISK_CONF_1_6
	cfg = ast_config_load("sccp.conf");

	if (!cfg)
		cfg = ast_config_load("sccp_v3.conf");

#else
	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS };
	cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
	if (!cfg)
		cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
#endif

	return cfg;
}

void sccp_config_softKeySet(struct ast_variable *variable, const char *name){
	int keySetSize;
	uint8_t keyMode = 0;
	int i=0;
	sccp_log(1)(VERBOSE_PREFIX_3 "start reading softkeyset: %s\n", name);
	while(variable){
		sccp_log(1)(VERBOSE_PREFIX_3 "softkeyset: %s \n",variable->name);
		if (!strcasecmp(variable->name, "type")){
			
		}else if(!strcasecmp(variable->name, "onhook")){
			keyMode = KEYMODE_ONHOOK;
		}else if(!strcasecmp(variable->name, "connected")){
			keyMode = KEYMODE_CONNECTED;
		}else if(!strcasecmp(variable->name, "onhold")){
			keyMode = KEYMODE_ONHOLD;
		}else if(!strcasecmp(variable->name, "ringin")){
			keyMode = KEYMODE_RINGIN;
		}else if(!strcasecmp(variable->name, "offhook")){
			keyMode = KEYMODE_OFFHOOK;
		}else if(!strcasecmp(variable->name, "conntrans")){
			keyMode = KEYMODE_CONNTRANS;
		}else if(!strcasecmp(variable->name, "digitsfoll")){
			keyMode = KEYMODE_DIGITSFOLL;
		}else if(!strcasecmp(variable->name, "connconf")){
			keyMode = KEYMODE_CONNCONF;
		}else if(!strcasecmp(variable->name, "ringout")){
			keyMode = KEYMODE_RINGOUT;
		}else if(!strcasecmp(variable->name, "offhookfeat")){
			keyMode = KEYMODE_OFFHOOKFEAT;
		}else if(!strcasecmp(variable->name, "onhint")){
			keyMode = KEYMODE_INUSEHINT;
		}else{
			
		}
		
		if(keyMode == 0){
			variable = variable->next;
			continue;
		}
		
		for(i=0;i < ( sizeof(SoftKeyModes)/sizeof(softkey_modes) ); i++){
			if(SoftKeyModes[i].id == keyMode){
				uint8_t *softkeyset = ast_malloc(StationMaxSoftKeySetDefinition * sizeof(uint8_t));
				keySetSize = sccp_config_readSoftSet(softkeyset, variable->value);
				
				if(keySetSize > 0){
					SoftKeyModes[i].ptr = softkeyset;
					SoftKeyModes[i].count = keySetSize;
				}else{
					ast_free(softkeyset);
				}
			}
		}
		variable = variable->next;
	}
}

uint8_t sccp_config_readSoftSet(uint8_t *softkeyset, const char *data){
	int i = 0, j;

	char 			labels[256];
	char 			*splitter;
	char			*label;


	strcpy(labels, data);
	splitter = labels;
	while( (label = strsep(&splitter, ",")) != NULL && i < StationMaxSoftKeySetDefinition){
		softkeyset[i++] = sccp_config_getSoftkeyLbl(label);
	}

	for(j=i;j < StationMaxSoftKeySetDefinition;j++){
		softkeyset[j] = SKINNY_LBL_EMPTY;
	}
	return i;
}

int sccp_config_getSoftkeyLbl(char* key){
	int i=0;
	int size = sizeof(softKeyTemplate)/sizeof(softkeyConfigurationTemplate);


	for(i=0; i < size; i++){
		if(!strcasecmp(softKeyTemplate[i].configVar, key)){
			return softKeyTemplate[i].softkey;
		}
	}
	return SKINNY_LBL_EMPTY;
}

void sccp_config_restoreDeviceFeatureStatus(sccp_device_t *device){
	if(!device)
		return;
	
	char 	buffer[256];
	char 	family[25];
	int 	res;
	
	sprintf(family, "SCCP/%s", device->id);
	res = ast_db_get(family, "dnd", buffer, sizeof(buffer));
	if(!res){
	      if(!strcasecmp(buffer, "silent"))
			device->dndFeature.status = SCCP_DNDMODE_SILENT;
	      else
			device->dndFeature.status = SCCP_DNDMODE_REJECT;
	}else{
	      device->dndFeature.status = SCCP_DNDMODE_OFF;
	}
	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));
	
	event->type=SCCP_EVENT_FEATURECHANGED;
	event->event.featureChanged.device = device;
	event->event.featureChanged.featureType = SCCP_FEATURE_DND;
	sccp_event_fire((const sccp_event_t **)&event);
}

