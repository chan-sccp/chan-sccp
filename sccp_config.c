/**
 *
 */
#include "config.h"

#include "asterisk.h"
#include "asterisk/channel.h"
#include "asterisk/astdb.h"

#include "chan_sccp.h"
#include "sccp_config.h"
#include "sccp_utils.h"
#include "sccp_line.h"
#include "sccp_device.h"

#ifdef CS_AST_HAS_EVENT
#include "sccp_mwi.h"
#include "asterisk/event.h"
#endif



static struct ast_config *cfg = NULL;



struct ast_config *sccp_config_getConfig(void);
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


	d = sccp_device_create();
	d = sccp_device_applyDefaults(d);
	d = sccp_config_applyDeviceConfiguration(d, variable);
	sccp_copy_string(d->id, deviceName, sizeof(d->id));

	memset(d->id, 0, sizeof(d->id));
	sccp_copy_string(d->id, deviceName, sizeof(d->id));

//	res=ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
//	if (!res)
//		d->phonemessage=strdup(message);									//set message on device if we have a result
//	strcpy(message,"");
//	ast_verbose(VERBOSE_PREFIX_3 "Added device '%s' (%s) \n", d->id, d->config_type);

#ifdef CS_SCCP_REALTIME
	d->realtime = isRealtime;
#endif
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

sccp_line_t *sccp_config_buildLine(struct ast_variable *variable,const char *lineName, boolean_t isRealtime){
	sccp_line_t 	*line = NULL;

	line = sccp_line_create();
	line = sccp_line_applyDefaults(line);
	sccp_copy_string(line->name, lineName, sizeof(line->name));
	line = sccp_config_applyLineConfiguration(line, variable);
	sccp_copy_string(line->name, lineName, sizeof(line->name));


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
	sccp_event_fire(event);
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


	//ast_config_destroy(cfg);
	return TRUE;
}
void sccp_config_readLines(sccp_readingtype_t readingtype){
	struct ast_config		*cfg = NULL;
	char 				*cat = NULL;
	struct ast_variable		*v = NULL;

	
	cfg = sccp_config_getConfig();
	if (!cfg) {
		ast_log(LOG_WARNING, "Unable to load config file sccp.conf, SCCP disabled\n");
		return;
	}

	/* get all categories */
	cat = NULL;
	while ((cat = ast_category_browse(cfg, cat))) {
		if( (strncmp(cat, "lines",5) == 0) ){
			//ast_verbose(VERBOSE_PREFIX_3 "using build_lines\n");
//			v = ast_variable_browse(cfg, cat);
//			build_lines(v);
		}else if( !(strncmp(cat, "SEP",3) == 0) && !(strncmp(cat, "ATA",3) == 0) && !(strncmp(cat, "general",7) == 0) ) {
			v = ast_variable_browse(cfg, cat);
			sccp_config_buildLine(v, cat, FALSE);
		}else{
			
		}
	}

	//ast_config_destroy(cfg);
}
void sccp_config_readDevices(sccp_readingtype_t readingtype){
	struct ast_config		*cfg = NULL;
	struct ast_variable		*v = NULL;
	char 					*cat = NULL;


	cfg = sccp_config_getConfig();
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
//		else if( (strncmp(cat, "devices",7) == 0) ){
//			ast_verbose(VERBOSE_PREFIX_3 "using build_devices\n");
//			v = ast_variable_browse(cfg, cat);
//			sccp_config_buildDevice()
//			build_devices(v);
//		}
	}
	ast_config_destroy(cfg);
}

/**
 * \brief configured line from \link ast_variable asterisk variable \endlink
 * \return configured line
 * \note also used by realtime functionality to line device from \link ast_variable asterisk variable \endlink
 */

sccp_line_t *sccp_config_applyLineConfiguration(sccp_line_t *l, struct ast_variable *v){
	int amaflags = 0;
	int secondary_dialtone_tone = 0;


	while(v) {
	 			if((!strcasecmp(v->name, "line") ) ){
					ast_log(LOG_WARNING, "obsolete line->name\n");
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
 * \brief apply device configuration from ast_variable
 * \return configured device
 * \note also used by realtime functionality to build device from \link ast_variable asterisk variable \endlink
 */

sccp_device_t *sccp_config_applyDeviceConfiguration(sccp_device_t *d, struct ast_variable *v){
		char 			message[256]="";							//device message
		int				res;

		/* for button config */
		char 			*buttonType = NULL, *buttonName = NULL, *buttonOption=NULL, *buttonArgs=NULL;
		char 			k_button[256];
	//	sccp_buttonconfig_t	* lastButton = NULL, * currentButton = NULL;
		uint8_t			instance=0;
		char 			*splitter;

		if(!v){
			sccp_log(10)(VERBOSE_PREFIX_3 "no variable given\n");
			return d;
		}
		while (v) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s = %s\n", v->name, v->value);
				if (!strcasecmp(v->name, "device")){
#ifdef CS_SCCP_REALTIME
				}else if (!strcasecmp(v->name, "name")) {
#endif
				}else if (!strcasecmp(v->name, "keepalive")) {
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
					if (!strcasecmp(buttonType, "line") && buttonName){
						if(!buttonName)
							continue;
						sccp_config_addLine(d, (buttonName)?ast_strip(buttonName):NULL, ++instance);
						if(buttonOption && !strcasecmp(buttonOption, "default")){
							d->defaultLineInstance = instance;
							sccp_log(99)(VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", instance);
						}
					} else if (!strcasecmp(buttonType, "empty")){
						sccp_config_addEmpty(d, ++instance);
					} else if (!strcasecmp(buttonType, "speeddial")){
						sccp_config_addSpeeddial(d, (buttonName)?ast_strip(buttonName):"speeddial", (buttonOption)?ast_strip(buttonOption):NULL, (buttonArgs)?ast_strip(buttonArgs):NULL, ++instance);
					} else if (!strcasecmp(buttonType, "feature") && buttonName){
						sccp_config_addFeature(d, (buttonName)?ast_strip(buttonName):"feature", (buttonOption)?ast_strip(buttonOption):NULL, buttonArgs, ++instance );
					} else if (!strcasecmp(buttonType, "service") && buttonName && !ast_strlen_zero(buttonName) ){
						sccp_config_addService(d, (buttonName)?ast_strip(buttonName):"service", (buttonOption)?ast_strip(buttonOption):NULL, ++instance);
					}
				} else if (!strcasecmp(v->name, "permithost")) {
					sccp_permithost_addnew(d, v->value);
				} else if (!strcasecmp(v->name, "type")) {
					sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
				} else if (!strcasecmp(v->name, "addon")) {
					sccp_addon_addnew(d, v->value);
				} else if (!strcasecmp(v->name, "tzoffset")) {
					/* timezone offset */
					d->tz_offset = atoi(v->value);
				} else if (!strcasecmp(v->name, "autologin")) {
					char *mb, *cur, tmp[256];

					sccp_copy_string(tmp, v->value, sizeof(tmp));
					mb = tmp;
					while((cur = strsep(&mb, ","))) {
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
					if(!ast_strlen_zero(v->value))
						d->pickupcontext = strdup(v->value);
				} else if (!strcasecmp(v->name, "pickupmodeanswer")) {
					d->pickupmodeanswer = sccp_true(v->value);
	#endif
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
					if(ast_strlen_zero(v->value)){
						sccp_config_addEmpty(d, ++instance);
					}else{
	//					//sccp_speeddial_addnew(d, v->value, speeddial_index);
						sccp_copy_string(k_button, v->value, sizeof(k_button));
						splitter = k_button;
						buttonType = strsep(&splitter, ",");
						buttonName = strsep(&splitter, ",");
						if(splitter)
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
				}else if (!strcasecmp(v->name, "setvar")) {
					struct ast_variable *newvar = NULL;
					newvar = sccp_create_variable(v->value);
					if(newvar){
						sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);
						newvar->next = d->variables;
						d->variables = newvar;
					}
	 			} else if (!strcasecmp(v->name, "serviceURL")) {
	 				//sccp_serviceURL_addnew(d, v->value, serviceURLIndex);
	 				ast_log(LOG_WARNING, "SCCP: Service: %s\n", v->value);
	 				if(ast_strlen_zero(v->value))
						sccp_config_addEmpty(d, ++instance);
					else{
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

struct ast_config *sccp_config_getConfig(){
// 	if(cfg)
// 		return cfg;
//	else{
#ifndef ASTERISK_CONF_1_6
		cfg = ast_config_load("sccp_v3.conf");
		if(!cfg)
			cfg = ast_config_load("sccp.conf");
#else
		struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS };
		cfg = ast_config_load2("sccp_v3.conf", "chan_sccp", config_flags);
		if(!cfg)
			cfg = ast_config_load2("sccp.conf", "chan_sccp", config_flags);
#endif
//	}
	return cfg;
}
