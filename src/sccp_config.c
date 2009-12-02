/**
 *
 */
#include "config.h"

#include "asterisk.h"
#include "asterisk/channel.h"

#include "chan_sccp.h"
#include "sccp_config.h"
#include "sccp_utils.h"
#include "sccp_event.h"
#include <asterisk/astdb.h>

#ifdef CS_AST_HAS_EVENT
#include "sccp_mwi.h"
#include "asterisk/event.h"
#endif



/**
 * Add Line to device.
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

void sccp_config_addEmpty(sccp_device_t *device, uint8_t index){
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
 *
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
 *
 */
void sccp_config_addService(sccp_device_t *device, char *label, char *url, uint8_t index){
	sccp_buttonconfig_t	*config;

	config = ast_malloc(sizeof(sccp_buttonconfig_t));
	memset(config, 0, sizeof(sccp_buttonconfig_t));


	//TODO check already existing instances
	config->instance = index;
	if (ast_strlen_zero(label) || ast_strlen_zero(url)) {
		config->type = EMPTY;
	}else{
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
	int				res;
	char 			message[256]="";							//device message

	// Try to find out if we have the device already on file.
	// However, do not look into realtime, since
	// we might have been asked to create a device for realtime addition,
	// thus causing an infinite loop / recursion.
	d = sccp_device_find_byid(deviceName, FALSE);
	if (d) {
		ast_log(LOG_WARNING, "SCCP: Device '%s' already exists\n", deviceName);
		return d;
	} 

	d = build_devices_wo(variable);
	memset(d->id, 0, sizeof(d->id));
	sccp_copy_string(d->id, deviceName, sizeof(d->id));
#ifdef CS_SCCP_REALTIME
	d->realtime = isRealtime;
#endif

	res = ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
	if (!res) {
		d->phonemessage=strdup(message);									//set message on device if we have a result
	}

	// TODO: Load status of feature (DND, CFwd, etc.) from astdb.

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_INSERT_HEAD(&GLOB(devices), d, list);
	SCCP_LIST_UNLOCK(&GLOB(devices));

	if(isRealtime) {
		sccp_log(1)(VERBOSE_PREFIX_3 "Added device '%s' (%s)\n", d->id, d->config_type);
	} else {
		sccp_log(1)(VERBOSE_PREFIX_3 "Added device '%s'\n", d->id);
	}

	return d;
}

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

	line = build_lines_wo(variable);

	sccp_copy_string(line->name, ast_strip((char *)lineName), sizeof(line->name));
#ifdef CS_SCCP_REALTIME
	line->realtime = isRealtime;
#endif

	// TODO: Load status of feature (DND, CFwd, etc.) from astdb.

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_INSERT_HEAD(&GLOB(lines), line, list);
	SCCP_LIST_UNLOCK(&GLOB(lines));
	sccp_log(1)(VERBOSE_PREFIX_3 "Added line '%s'\n", line->name);

	//ast_config_destroy(variable);
	/* registering mailbox events */
#ifdef CS_AST_HAS_EVENT
//	sccp_mailbox_t *mailbox = NULL;
//	SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list){
//		sccp_mwi_subscribeMailbox(&line, &mailbox);
//	}
#endif

	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));
	event->type=SCCP_EVENT_LINECREATED;
	event->event.lineCreated.line = line;
	sccp_event_fire((const sccp_event_t **)&event);
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
	int						tos		= 0;
	char					pref_buf[128];
	struct ast_hostent		ahp;
	struct hostent			*hp;
	struct ast_ha 			*na;
#ifdef ASTERISK_CONF_1_6
	struct ast_flags 		config_flags = { CONFIG_FLAG_WITHCOMMENTS };
#endif


#ifndef ASTERISK_CONF_1_6
	cfg = ast_config_load("sccp_v3.conf");
	if(!cfg)
		cfg = ast_config_load("sccp.conf");
#else
	cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
	if(!cfg)
		cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
#endif
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

#ifndef ASTERISK_CONF_1_6
	cfg = ast_config_load("sccp_v3.conf");
	if(!cfg)
		cfg = ast_config_load("sccp.conf");
#else
	struct ast_flags 		config_flags = { CONFIG_FLAG_WITHCOMMENTS };
	cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
	if(!cfg)
		cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
#endif
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
                        // check minimum requirements for a line
                        if ( !ast_strlen_zero( ast_variable_retrieve(cfg, cat, "label") ) ) {
                        	;
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
                }
        }
        ast_config_destroy(cfg);
}

void sccp_config_readLines(sccp_readingtype_t readingtype){
	struct ast_config		*cfg = NULL;
	char 					*cat = NULL;
	struct ast_variable		*v = NULL;

#ifndef ASTERISK_CONF_1_6
	cfg = ast_config_load("sccp_v3.conf");
	if(!cfg)
		cfg = ast_config_load("sccp.conf");
#else
	struct ast_flags 		config_flags = { CONFIG_FLAG_WITHCOMMENTS };
	cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
	if(!cfg)
		cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
#endif
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}

	/* get all categories */
	while ((cat = ast_category_browse(cfg, cat))) {
		if( (strncmp(cat, "lines",5) == 0) ){
		}else if( !(strncmp(cat, "devices",7) == 0) && !(strncmp(cat, "SEP",3) == 0) && !(strncmp(cat, "ATA",3) == 0) && !(strncmp(cat, "general",7) == 0) ) {
			v = ast_variable_browse(cfg, cat);
			sccp_config_buildLine(v, cat, FALSE);
		}
	}

	ast_config_destroy(cfg);
}
void sccp_config_readdevices(sccp_readingtype_t readingtype){
	struct ast_config		*cfg = NULL;
	struct ast_variable		*v = NULL;
	char 					*cat = NULL;

#ifndef ASTERISK_CONF_1_6
	cfg = ast_config_load("sccp_v3.conf");
	if(!cfg)
		cfg = ast_config_load("sccp.conf");
#else
	struct ast_flags 		config_flags = { CONFIG_FLAG_WITHCOMMENTS };
	cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
	if(!cfg)
		cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
#endif
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}


	/* get all categories */
	while ((cat = ast_category_browse(cfg, cat))) {
		if( (strncmp(cat, "SEP",3) == 0) || (strncmp(cat, "ATA",3) == 0) ){
			ast_verbose(VERBOSE_PREFIX_3 "found device %s\n", cat);
			v = ast_variable_browse(cfg, cat);
			sccp_config_buildDevice(v, cat, FALSE);
		}
		else if( (strncmp(cat, "devices",7) == 0) ){
			ast_verbose(VERBOSE_PREFIX_3 "using build_devices\n");
			v = ast_variable_browse(cfg, cat);
			build_devices_wo(v);
		}
	}
	ast_config_destroy(cfg);
}


