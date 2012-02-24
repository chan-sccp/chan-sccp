
/*!
 * \file 	sccp_appfunctions.c
 * \brief 	SCCP application / dialplan functions Class
 * \author 	Diederik de Groot (ddegroot [at] sourceforge.net)
 * \date        18-03-2011
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date: 2011-01-12 02:42:50 +0100 (Mi, 12 Jan 2011) $
 * $Revision: 2235 $
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2235 $")

/*!
 * \brief  ${SCCPDEVICE()} Dialplan function - reads device data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param buf Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * \ref nf_sccp_dialplan_sccpdevice
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- device
 * 	  - device->buttonconfig
 */
static int sccp_func_sccpdevice(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *buf, size_t len)
{
	sccp_device_t *d;

	char *colname;

	char tmp[1024] = "";

	char lbuf[1024] = "";

	int first = 0;

	if ((colname = strchr(data, ':'))) {					/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0)
			pbx_log(LOG_WARNING, "SCCPDEVICE(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
	} else if ((colname = strchr(data, ',')))
		*colname++ = '\0';
	else
		colname = "ip";

	if (!strncasecmp(data, "current", 7)) {
		sccp_channel_t *c;

		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {

/*			pbx_log(LOG_WARNING, "SCCPDEVICE(): Not an SCCP channel\n");*/
			return -1;
		}

		if (!c || !sccp_channel_getDevice(c)) {
			pbx_log(LOG_WARNING, "SCCPDEVICE(): SCCP Device not available\n");
			return -1;
		}

		d = sccp_channel_getDevice(c);

	} else {
		if (!(d = sccp_device_find_byid(data, TRUE))) {
			pbx_log(LOG_WARNING, "SCCPDEVICE(): SCCP Device not available\n");
			return -1;
		}
	}

	sccp_device_lock(d);
	if (!strcasecmp(colname, "ip")) {
		sccp_session_t *s = d->session;

		if (s) {
			sccp_copy_string(buf, s->sin.sin_addr.s_addr ? pbx_inet_ntoa(s->sin.sin_addr) : "", len);
		}
	} else if (!strcasecmp(colname, "id")) {
		sccp_copy_string(buf, d->id, len);
	} else if (!strcasecmp(colname, "status")) {
		sccp_copy_string(buf, devicestatus2str(d->state), len);
	} else if (!strcasecmp(colname, "description")) {
		sccp_copy_string(buf, d->description, len);
	} else if (!strcasecmp(colname, "config_type")) {
		sccp_copy_string(buf, d->config_type, len);
	} else if (!strcasecmp(colname, "skinny_type")) {
		sccp_copy_string(buf, devicetype2str(d->skinny_type), len);
	} else if (!strcasecmp(colname, "tz_offset")) {
		snprintf(buf, len, "%d", d->tz_offset);
	} else if (!strcasecmp(colname, "image_version")) {
		sccp_copy_string(buf, d->imageversion, len);
	} else if (!strcasecmp(colname, "accessory_status")) {
		sccp_copy_string(buf, accessorystatus2str(d->accessorystatus), len);
	} else if (!strcasecmp(colname, "registration_state")) {
		sccp_copy_string(buf, deviceregistrationstatus2str(d->registrationState), len);
	} else if (!strcasecmp(colname, "codecs")) {
//              pbx_codec_pref_string(&d->codecs, buf, sizeof(buf) - 1);
		sccp_multiple_codecs2str(buf, sizeof(buf) - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
	} else if (!strcasecmp(colname, "capability")) {
//              pbx_getformatname_multiple(buf, len - 1, d->capability);
		sccp_multiple_codecs2str(buf, len - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
	} else if (!strcasecmp(colname, "state")) {
		sccp_copy_string(buf, accessorystatus2str(d->accessorystatus), len);
	} else if (!strcasecmp(colname, "lines_registered")) {
		sccp_copy_string(buf, d->linesRegistered ? "yes" : "no", len);
	} else if (!strcasecmp(colname, "lines_count")) {
		snprintf(buf, len, "%d", d->linesCount);
	} else if (!strcasecmp(colname, "last_number")) {
		sccp_copy_string(buf, d->lastNumber, len);
	} else if (!strcasecmp(colname, "early_rtp")) {
		snprintf(buf, len, "%d", d->earlyrtp);
	} else if (!strcasecmp(colname, "supported_protocol_version")) {
		snprintf(buf, len, "%d", d->protocolversion);
	} else if (!strcasecmp(colname, "used_protocol_version")) {
		snprintf(buf, len, "%d", d->inuseprotocolversion);
	} else if (!strcasecmp(colname, "mwi_light")) {
		sccp_copy_string(buf, d->mwilight ? "ON" : "OFF", len);
	} else if (!strcasecmp(colname, "dynamic") || !strcasecmp(colname, "realtime")) {
#ifdef CS_SCCP_REALTIME
		sccp_copy_string(buf, d->realtime ? "yes" : "no", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strcasecmp(colname, "active_channel")) {
		snprintf(buf, len, "%d", d->active_channel->callid);
	} else if (!strcasecmp(colname, "transfer_channel")) {
		snprintf(buf, len, "%d", d->transfer_channel->callid);
	} else if (!strcasecmp(colname, "conference_channel")) {
		snprintf(buf, len, "%d", d->conference_channel->callid);
	} else if (!strcasecmp(colname, "current_line")) {
		sccp_copy_string(buf, d->currentLine->id, len);
	} else if (!strcasecmp(colname, "button_config")) {
		sccp_buttonconfig_t *config;

		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			switch (config->type) {
			case LINE:
				snprintf(tmp, sizeof(tmp), "[%d,%s,%s]", config->instance, sccp_buttontype2str(config->type), config->button.line.name);
				break;
			case SPEEDDIAL:
				snprintf(tmp, sizeof(tmp), "[%d,%s,%s,%s]", config->instance, sccp_buttontype2str(config->type), config->label, config->button.speeddial.ext);
				break;
			case SERVICE:
				snprintf(tmp, sizeof(tmp), "[%d,%s,%s,%s]", config->instance, sccp_buttontype2str(config->type), config->label, config->button.service.url);
				break;
			case FEATURE:
				snprintf(tmp, sizeof(tmp), "[%d,%s,%s,%s]", config->instance, sccp_buttontype2str(config->type), config->label, config->button.feature.options);
				break;
			case EMPTY:
				snprintf(tmp, sizeof(tmp), "[%d,%s]", config->instance, sccp_buttontype2str(config->type));
				break;
			}
			if (first == 0) {
				first = 1;
				strcat(lbuf, tmp);
			} else {
				strcat(lbuf, ",");
				strcat(lbuf, tmp);
			}
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
		snprintf(buf, len, "[ %s ]", lbuf);
	} else if (!strcasecmp(colname, "pending_delete")) {
#ifdef CS_DYNAMIC_CONFIG
		sccp_copy_string(buf, d->pendingDelete ? "yes" : "no", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strcasecmp(colname, "pending_update")) {
#ifdef CS_DYNAMIC_CONFIG
		sccp_copy_string(buf, d->pendingUpdate ? "yes" : "no", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strncasecmp(colname, "chanvar[", 8)) {
		char *chanvar = colname + 8;

		PBX_VARIABLE_TYPE *v;

		chanvar = strsep(&chanvar, "]");
		for (v = d->variables; v; v = v->next) {
			if (!strcasecmp(v->name, chanvar)) {
				sccp_copy_string(buf, v->value, len);
			}
		}
	} else if (!strncasecmp(colname, "codec[", 6)) {
		char *codecnum;

//              int codec = 0;

		codecnum = colname + 6;						// move past the '[' 
		codecnum = strsep(&codecnum, "]");				// trim trailing ']' if any 
		if (skinny_codecs[atoi(codecnum)].key) {
//              if ((codec = pbx_codec_pref_index(&d->codecs, atoi(codecnum)))) {
//                      sccp_copy_string(buf, pbx_getformatname(codec), len);
			sccp_copy_string(buf, codec2name(atoi(codecnum)), len);
		} else {
			buf[0] = '\0';
		}
	} else {
		pbx_log(LOG_WARNING, "SCCPDEVICE(): unknown function option: %s", data);
		buf[0] = '\0';
	}
	sccp_device_unlock(d);
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPDEVICE */
static struct pbx_custom_function sccpdevice_function = {
	.name = "SCCPDEVICE",
	.synopsis = "Retrieves information about an SCCP Device",
	.syntax = "Usage: SCCPDEVICE(deviceId,<option>)\n",
	.read = sccp_func_sccpdevice,
	.desc = "DeviceId = Device Identifier (i.e. SEP0123456789)\n" "Option = One of the possible options mentioned in arguments\n",
	.arguments = "DeviceId = Device Identifier (i.e. SEP0123456789)\n"
	    "Option = One of these possible options:\n" "ip, id, status, description, config_type, skinny_type, tz_offset, image_version, \n" "accessory_status, registration_state, codecs, capability, state, lines_registered, \n" "lines_count, last_number, early_rtp, supported_protocol_version, used_protocol_version, \n" "mwi_light, dynamic, realtime, active_channel, transfer_channel, conference_channel, \n" "current_line, button_config, pending_delete, chanvar[], codec[]",
};

/*!
 * \brief  ${SCCPLINE()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param buf Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * \ref nf_sccp_dialplan_sccpline
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- line
 * 	  - line->devices
 */
static int sccp_func_sccpline(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *buf, size_t len)
{
	sccp_line_t *l;

	sccp_channel_t *c;

	char *colname;
	char tmp[1024] = "";

	char lbuf[1024] = "";

	int first = 0;

	if ((colname = strchr(data, ':'))) {					/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0)
			pbx_log(LOG_WARNING, "SCCPLINE(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
	} else if ((colname = strchr(data, ',')))
		*colname++ = '\0';
	else
		colname = "id";

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {

/*			pbx_log(LOG_WARNING, "SCCPLINE(): Not an SCCP Channel\n");*/
			return -1;
		}

		if (!c || !c->line) {
			pbx_log(LOG_WARNING, "SCCPLINE(): SCCP Line not available\n");
			return -1;
		}
		l = c->line;
	} else if (!strncasecmp(data, "parent", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {

/*			pbx_log(LOG_WARNING, "SCCPLINE(): Not an SCCP Channel\n");*/
			return -1;
		}

		if (!c || !c->parentChannel || !c->parentChannel->line) {
			pbx_log(LOG_WARNING, "SCCPLINE(): SCCP Line not available\n");
			return -1;
		}
		l = c->parentChannel->line;
	} else {
		if (!(l = sccp_line_find_byname_wo(data, TRUE))) {
			pbx_log(LOG_WARNING, "sccp_func_sccpdevice: SCCP Line not available\n");
			return -1;
		}
	}
	sccp_line_lock(l);

	if (!strcasecmp(colname, "id")) {
		sccp_copy_string(buf, l->id, len);
	} else if (!strcasecmp(colname, "name")) {
		sccp_copy_string(buf, l->name, len);
	} else if (!strcasecmp(colname, "description")) {
		sccp_copy_string(buf, l->description, len);
	} else if (!strcasecmp(colname, "label")) {
		sccp_copy_string(buf, l->label, len);
	} else if (!strcasecmp(colname, "vmnum")) {
		sccp_copy_string(buf, l->vmnum, len);
	} else if (!strcasecmp(colname, "trnsfvm")) {
		sccp_copy_string(buf, l->trnsfvm, len);
	} else if (!strcasecmp(colname, "meetme")) {
		sccp_copy_string(buf, l->meetme ? "on" : "off", len);
	} else if (!strcasecmp(colname, "meetmenum")) {
		sccp_copy_string(buf, l->meetmenum, len);
	} else if (!strcasecmp(colname, "meetmeopts")) {
		sccp_copy_string(buf, l->meetmeopts, len);
	} else if (!strcasecmp(colname, "context")) {
		sccp_copy_string(buf, l->context, len);
	} else if (!strcasecmp(colname, "language")) {
		sccp_copy_string(buf, l->language, len);
	} else if (!strcasecmp(colname, "accountcode")) {
		sccp_copy_string(buf, l->accountcode, len);
	} else if (!strcasecmp(colname, "musicclass")) {
		sccp_copy_string(buf, l->musicclass, len);
	} else if (!strcasecmp(colname, "amaflags")) {
		sccp_copy_string(buf, l->amaflags ? "yes" : "no", len);
	} else if (!strcasecmp(colname, "callgroup")) {
		pbx_print_group(buf, len, l->callgroup);
	} else if (!strcasecmp(colname, "pickupgroup")) {
#ifdef CS_SCCP_PICKUP
		pbx_print_group(buf, len, l->pickupgroup);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strcasecmp(colname, "cid_name")) {
		sccp_copy_string(buf, l->cid_name ? l->cid_name : "<not set>", len);
	} else if (!strcasecmp(colname, "cid_num")) {
		sccp_copy_string(buf, l->cid_num ? l->cid_num : "<not set>", len);
	} else if (!strcasecmp(colname, "incoming_limit")) {
		snprintf(buf, len, "%d", l->incominglimit);
	} else if (!strcasecmp(colname, "channel_count")) {
		snprintf(buf, len, "%d", SCCP_RWLIST_GETSIZE(l->channels));
	} else if (!strcasecmp(colname, "dynamic") || !strcasecmp(colname, "realtime")) {
#ifdef CS_SCCP_REALTIME
		sccp_copy_string(buf, l->realtime ? "Yes" : "No", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strcasecmp(colname, "pending_delete")) {

#ifdef CS_DYNAMIC_CONFIG
		sccp_copy_string(buf, l->pendingDelete ? "yes" : "no", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif
	} else if (!strcasecmp(colname, "pending_update")) {

#ifdef CS_DYNAMIC_CONFIG
		sccp_copy_string(buf, l->pendingUpdate ? "yes" : "no", len);
#else
		sccp_copy_string(buf, "not supported", len);
#endif

		/* regexten feature -- */
	} else if (!strcasecmp(colname, "regexten")) {
		sccp_copy_string(buf, l->regexten ? l->regexten : "Unset", len);
	} else if (!strcasecmp(colname, "regcontext")) {
		sccp_copy_string(buf, l->regcontext ? l->regcontext : "Unset", len);
		/* -- regexten feature */

	} else if (!strcasecmp(colname, "adhoc_number")) {
		sccp_copy_string(buf, l->adhocNumber ? l->adhocNumber : "No", len);
	} else if (!strcasecmp(colname, "newmsgs")) {
		snprintf(buf, len, "%d", l->voicemailStatistic.newmsgs);
	} else if (!strcasecmp(colname, "oldmsgs")) {
		snprintf(buf, len, "%d", l->voicemailStatistic.oldmsgs);
	} else if (!strcasecmp(colname, "num_lines")) {
		snprintf(buf, len, "%d", l->devices.size);
	} else if (!strcasecmp(colname, "cfwd")) {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice)
				snprintf(tmp, sizeof(tmp), "[id:%s,cfwdAll:%s,num:%s,cfwdBusy:%s,num:%s]", linedevice->device->id, linedevice->cfwdAll.enabled ? "on" : "off", linedevice->cfwdAll.number ? linedevice->cfwdAll.number : "<not set>", linedevice->cfwdBusy.enabled ? "on" : "off", linedevice->cfwdBusy.number ? linedevice->cfwdBusy.number : "<not set>");
			if (first == 0) {
				first = 1;
				strcat(lbuf, tmp);
			} else {
				strcat(lbuf, ",");
				strcat(lbuf, tmp);
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
		snprintf(buf, len, "%s", lbuf);
	} else if (!strcasecmp(colname, "devices")) {
		sccp_linedevices_t *linedevice;

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if (linedevice)
				snprintf(tmp, sizeof(tmp), "%s", linedevice->device->id);
			if (first == 0) {
				first = 1;
				strcat(lbuf, tmp);
			} else {
				strcat(lbuf, ",");
				strcat(lbuf, tmp);
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
		snprintf(buf, len, "%s", lbuf);
	} else if (!strncasecmp(colname, "chanvar[", 8)) {
		char *chanvar = colname + 8;

		PBX_VARIABLE_TYPE *v;

		chanvar = strsep(&chanvar, "]");
		for (v = l->variables; v; v = v->next) {
			if (!strcasecmp(v->name, chanvar)) {
				sccp_copy_string(buf, v->value, len);
			}
		}
	} else {
		pbx_log(LOG_WARNING, "SCCPLINE(): unknown function option: %s", data);
		buf[0] = '\0';
	}
	sccp_line_unlock(l);
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPLINE */
static struct pbx_custom_function sccpline_function = {
	.name = "SCCPLINE",
	.synopsis = "Retrieves information about an SCCP Line",
	.syntax = "Usage: SCCPLINE(lineName,<option>)",
	.read = sccp_func_sccpline,
	.desc = "LineName = Name of the line to be queried.\n" "Option = One of the possible options mentioned in arguments\n",
	.arguments = "LineName = use on off these: 'current', 'parent', actual linename\n" "Option = One of these possible options:\n" "id, name, description, label, vmnum, trnsfvm, meetme, meetmenum, meetmeopts, context, \n" "language, accountcode, musicclass, amaflags, callgroup, pickupgroup, cid_name, cid_num, \n" "incoming_limit, channel_count, dynamic, realtime, pending_delete, pending_update, \n" "regexten, regcontext, adhoc_number, newmsgs, oldmsgs, num_lines, cfwd, devices, chanvar[]"
};

/*!
 * \brief  ${SCCPCHANNEL()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param buf Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * \ref nf_sccp_dialplan_sccpchannel
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- channel
 */
static int sccp_func_sccpchannel(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *buf, size_t len)
{
	sccp_channel_t *c;

	char *colname;

	if ((colname = strchr(data, ':'))) {					/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0)
			pbx_log(LOG_WARNING, "SCCPCHANNEL(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
	} else if ((colname = strchr(data, ',')))
		*colname++ = '\0';
	else
		colname = "callid";

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan)))
			return -1;						/* Not a SCCP channel. */

		sccp_channel_lock(c);
	} else {
		uint32_t callid = atoi(data);

		if (!(c = sccp_channel_find_byid_locked(callid))) {
			pbx_log(LOG_WARNING, "SCCPCHANNEL(): SCCP Channel not available\n");
			return -1;
		}
	}

	if (!strcasecmp(colname, "callid") || !strcasecmp(colname, "id")) {
		snprintf(buf, len, "%d", c->callid);
	} else if (!strcasecmp(colname, "format")) {
		snprintf(buf, len, "%d", c->rtp.audio.readFormat);
	} else if (!strcasecmp(colname, "codecs")) {
//              pbx_codec_pref_string(&c->codecs, buf, sizeof(buf) - 1);
		sccp_copy_string(buf, codec2name(c->rtp.audio.readFormat), len);
	} else if (!strcasecmp(colname, "capability")) {
//              pbx_getformatname_multiple(buf, len - 1, c->capability);
		sccp_multiple_codecs2str(buf, sizeof(buf) - 1, c->capabilities.audio, ARRAY_LEN(c->capabilities.audio));
	} else if (!strcasecmp(colname, "calledPartyName")) {
		sccp_copy_string(buf, c->callInfo.calledPartyName, len);
	} else if (!strcasecmp(colname, "calledPartyNumber")) {
		sccp_copy_string(buf, c->callInfo.calledPartyNumber, len);
	} else if (!strcasecmp(colname, "callingPartyName")) {
		sccp_copy_string(buf, c->callInfo.callingPartyName, len);
	} else if (!strcasecmp(colname, "callingPartyNumber")) {
		sccp_copy_string(buf, c->callInfo.callingPartyNumber, len);
	} else if (!strcasecmp(colname, "originalCallingPartyName")) {
		sccp_copy_string(buf, c->callInfo.originalCallingPartyName, len);
	} else if (!strcasecmp(colname, "originalCallingPartyNumber")) {
		sccp_copy_string(buf, c->callInfo.originalCallingPartyNumber, len);
	} else if (!strcasecmp(colname, "originalCalledPartyName")) {
		sccp_copy_string(buf, c->callInfo.originalCalledPartyName, len);
	} else if (!strcasecmp(colname, "originalCalledPartyNumber")) {
		sccp_copy_string(buf, c->callInfo.originalCalledPartyNumber, len);
	} else if (!strcasecmp(colname, "lastRedirectingPartyName")) {
		sccp_copy_string(buf, c->callInfo.lastRedirectingPartyName, len);
	} else if (!strcasecmp(colname, "lastRedirectingPartyNumber")) {
		sccp_copy_string(buf, c->callInfo.lastRedirectingPartyNumber, len);
	} else if (!strcasecmp(colname, "cgpnVoiceMailbox")) {
		sccp_copy_string(buf, c->callInfo.cgpnVoiceMailbox, len);
	} else if (!strcasecmp(colname, "cdpnVoiceMailbox")) {
		sccp_copy_string(buf, c->callInfo.cdpnVoiceMailbox, len);
	} else if (!strcasecmp(colname, "originalCdpnVoiceMailbox")) {
		sccp_copy_string(buf, c->callInfo.originalCdpnVoiceMailbox, len);
	} else if (!strcasecmp(colname, "lastRedirectingVoiceMailbox")) {
		sccp_copy_string(buf, c->callInfo.lastRedirectingVoiceMailbox, len);
	} else if (!strcasecmp(colname, "passthrupartyid")) {
		snprintf(buf, len, "%d", c->passthrupartyid);
	} else if (!strcasecmp(colname, "state")) {
		sccp_copy_string(buf, channelstate2str(c->state), len);
	} else if (!strcasecmp(colname, "previous_state")) {
		sccp_copy_string(buf, channelstate2str(c->previousChannelState), len);
	} else if (!strcasecmp(colname, "calltype")) {
		sccp_copy_string(buf, calltype2str(c->calltype), len);
	} else if (!strcasecmp(colname, "dialed_number")) {
		sccp_copy_string(buf, c->dialedNumber, len);
	} else if (!strcasecmp(colname, "device")) {
		sccp_copy_string(buf, sccp_channel_getDevice(c)->id, len);
	} else if (!strcasecmp(colname, "line")) {
		sccp_copy_string(buf, c->line->name, len);
	} else if (!strcasecmp(colname, "answered_elsewhere")) {
		sccp_copy_string(buf, c->answered_elsewhere ? "yes" : "no", len);
	} else if (!strcasecmp(colname, "privacy")) {
		sccp_copy_string(buf, c->privacy ? "yes" : "no", len);
	} else if (!strcasecmp(colname, "ss_action")) {
		snprintf(buf, len, "%d", c->ss_action);
//      } else if (!strcasecmp(colname, "monitorEnabled")) {
//              sccp_copy_string(buf, c->monitorEnabled ? "yes" : "no", len);
	} else if (!strcasecmp(colname, "conference")) {
		/*! \todo needs to be implemented */
	} else if (!strcasecmp(colname, "parent")) {
		snprintf(buf, len, "%d", c->parentChannel->callid);
	} else if (!strcasecmp(colname, "peer")) {
		/*! \todo needs to be implemented */
	} else if (!strncasecmp(colname, "codec[", 6)) {
		char *codecnum;

//              int codec = 0;

		codecnum = colname + 6;						// move past the '[' 
		codecnum = strsep(&codecnum, "]");				// trim trailing ']' if any 
//              if ((codec = pbx_codec_pref_index(&c->codecs, atoi(codecnum)))) {
//                      sccp_copy_string(buf, pbx_getformatname(codec), len);
		if (skinny_codecs[atoi(codecnum)].key) {
			sccp_copy_string(buf, codec2name(atoi(codecnum)), len);
		} else {
			buf[0] = '\0';
		}
	} else {
		pbx_log(LOG_WARNING, "SCCPCHANNEL(): unknown function option: %s", data);
		buf[0] = '\0';
	}
	sccp_channel_unlock(c);
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPCHANNEL */
static struct pbx_custom_function sccpchannel_function = {
	.name = "SCCPCHANNEL",
	.synopsis = "Retrieves information about an SCCP Line",
	.syntax = "Usage: SCCPCHANNEL(channelId,<option>)",
	.read = sccp_func_sccpchannel,
	.desc = "ChannelId = Name of the line to be queried.\n" "Option = One of the possible options mentioned in arguments\n",
	.arguments = "ChannelId = use on off these: 'current', actual callid\n" "Option = One of these possible options:\n" "callid, id, format, codecs, capability, calledPartyName, calledPartyNumber, callingPartyName, \n" "callingPartyNumber, originalCallingPartyName, originalCallingPartyNumber, originalCalledPartyName, \n" "originalCalledPartyNumber, lastRedirectingPartyName, lastRedirectingPartyNumber, cgpnVoiceMailbox, \n" "cdpnVoiceMailbox, originalCdpnVoiceMailbox, lastRedirectingVoiceMailbox, passthrupartyid, state, \n" "previous_state, calltype, dialed_number, device, line, answered_elsewhere, privacy, ss_action, \n" "monitorEnabled, parent, codec[]"	// not implemented yet: "/*conference*/, /*peer*/"
};

/*!
 * \brief 	Set the Preferred Codec for a SCCP channel via the dialplan
 * \param 	chan Asterisk Channel
 * \param 	data single codec name
 * \return	Success as int
 * 
 * \called_from_asterisk
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{

	sccp_channel_t *c = NULL;
	int res;

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "sccp_app_prefcodec(): Not an SCCP channel\n");
		return -1;
	}

	res = sccp_channel_setPreferredCodec(c, data);
	return res ? 0 : -1;
}

/*! \brief Stucture to declare a dialplan function: SETSCCPCODEC */
static char *prefcodec_name = "SetSCCPCodec";
static char *prefcodec_synopsis = "Sets the preferred codec for the current sccp channel";
static char *prefcodec_descr = "Usage: SetSCCPCodec(codec)" "Sets the preferred codec for dialing out with the current chan_sccp channel\n";

/*!
 * \brief 	Set the Name and Number of the Called Party to the Calling Phone
 * \param 	chan Asterisk Channel
 * \param 	data CallerId in format "Name" \<number\>
 * \return	Success as int
 * 
 * \called_from_asterisk
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	char *text = (char *)data;

	char *num, *name;

	sccp_channel_t *c = NULL;

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPDEVICE(): Not an SCCP channel\n");
		return 0;
	}

	if (!text || !c)
		return 0;

	pbx_callerid_parse(text, &name, &num);
	sccp_channel_set_calledparty(c, name, num);

	return 0;
}

/*! \brief Stucture to declare a dialplan function: SETCALLEDPARTY */
static char *calledparty_name = "SetCalledParty";
static char *calledparty_synopsis = "Sets the callerid of the called party";
static char *calledparty_descr = "Usage: SetCalledParty(\"Name\" <ext>)" "Sets the name and number of the called party for use with chan_sccp\n";

/*!
 * \brief	It allows you to send a message to the calling device.
 * \author	Frank Segtrop <fs@matflow.net>
 * \param	chan asterisk channel
 * \param	data message to sent - if empty clear display
 * \version	20071112_1944
 *
 * \called_from_asterisk
 *
 * \lock
 * 	- device
 * 	  - see sccp_dev_displayprinotify()
 * 	  - see sccp_dev_displayprompt()
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d;

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPDEVICE(): Not an SCCP channel\n");
		return 0;
	}

	char *text;
	char *splitter = sccp_strdupa(data);
	int timeout = 0;

	text = strsep(&splitter, ",");
	if (splitter) {
		timeout = atoi(splitter);
	}

	if (!text || !c || !sccp_channel_getDevice(c))
		return 0;

	d = sccp_channel_getDevice(c);
	sccp_device_lock(d);
	if (text[0] != '\0') {
		sccp_dev_set_message(d, text, timeout, TRUE, FALSE);
	} else {
		sccp_dev_clear_message(d, TRUE);
	}

	sccp_device_unlock(d);

	return 0;
}

/*! \brief Stucture to declare a dialplan function: SETMESSAGE */
static char *setmessage_name = "SetMessage";
static char *setmessage_synopsis = "Send a Message to the current Phone";
static char *setmessage_descr = "Usage: SetMessage(\"Message\"[,timeout])\n" "       Send a Message to the Calling Device (and remove after timeout, if timeout is ommited will stay until next/empty message)\n";

int sccp_register_dialplan_functions(void)
{
	int result = 0;

	/* Register application functions */
	result = pbx_register_application(calledparty_name, sccp_app_calledparty, calledparty_synopsis, calledparty_descr, NULL);
	result |= pbx_register_application(setmessage_name, sccp_app_setmessage, setmessage_synopsis, setmessage_descr, NULL);
	result |= pbx_register_application(prefcodec_name, sccp_app_prefcodec, prefcodec_synopsis, prefcodec_descr, NULL);

	/* Register dialplan functions */
	result |= pbx_custom_function_register(&sccpdevice_function, NULL);
	result |= pbx_custom_function_register(&sccpline_function, NULL);
	result |= pbx_custom_function_register(&sccpchannel_function, NULL);

	return result;
}

int sccp_unregister_dialplan_functions(void)
{
	int result = 0;

	/* Unregister applications functions */
	result = pbx_unregister_application(calledparty_name);
	result |= pbx_unregister_application(setmessage_name);
	result |= pbx_unregister_application(prefcodec_name);

	/* Unregister dial plan functions */
	result |= pbx_custom_function_unregister(&sccpdevice_function);
	result |= pbx_custom_function_unregister(&sccpline_function);
	result |= pbx_custom_function_unregister(&sccpchannel_function);

	return result;
}
