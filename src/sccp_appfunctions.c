/*!
 * \file        sccp_appfunctions.c
 * \brief       SCCP application / dialplan functions Class
 * \author      Diederik de Groot (ddegroot [at] sourceforge.net)
 * \date        18-03-2011
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 */
#include "config.h"
#include "common.h"
#include "sccp_appfunctions.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_mwi.h"
#include "sccp_conference.h"
#include "sccp_session.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/callerid.h>
#include <asterisk/module.h>		// ast_register_application2
#ifdef HAVE_PBX_APP_H
#  include <asterisk/app.h>
#endif

PBX_THREADSTORAGE(coldata_buf);
PBX_THREADSTORAGE(colnames_buf);

/*!
 * \brief ${SCCPDEVICE()} Dialplan function - reads device data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 */
static int sccp_func_sccpdevice(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;												// we should make this a finite length
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;
	int addcomma = 0;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPDEVICE(): usage of ':' to separate arguments is deprecated. Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "ip");
	}

	AUTO_RELEASE(sccp_device_t, d , NULL);

	if (!strncasecmp(data, "current", 7)) {
		AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(chan));

		if (c) {
			if (!(d = sccp_channel_getDevice(c))) {
				pbx_log(LOG_WARNING, "SCCPDEVICE(): SCCP Device not available\n");
				return -1;
			}
		} else {
			/* pbx_log(LOG_WARNING, "SCCPDEVICE(): No current SCCP channel found\n"); */
			return -1;
		}
	} else {
		if (!(d = sccp_device_find_byid(data, FALSE))) {
			pbx_log(LOG_WARNING, "SCCPDEVICE(): SCCP Device not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (d) {
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			addcomma = 0;
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			
			if (!strcasecmp(token, "ip")) {
				sccp_session_t *s = d->session;

				if (s) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getOurIP(s, &sas, 0);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), buf_len);
				}
			} else if (!strcasecmp(token, "id")) {
				sccp_copy_string(buf, d->id, buf_len);
			} else if (!strcasecmp(token, "status")) {
				sccp_copy_string(buf, sccp_devicestate2str(sccp_device_getDeviceState(d)), buf_len);
			} else if (!strcasecmp(token, "description")) {
				sccp_copy_string(buf, d->description, buf_len);
			} else if (!strcasecmp(token, "config_type")) {
				sccp_copy_string(buf, d->config_type, buf_len);
			} else if (!strcasecmp(token, "skinny_type")) {
				sccp_copy_string(buf, skinny_devicetype2str(d->skinny_type), buf_len);
			} else if (!strcasecmp(token, "tz_offset")) {
				snprintf(buf, buf_len, "%d", d->tz_offset);
			} else if (!strcasecmp(token, "image_version")) {
				sccp_copy_string(buf, d->loadedimageversion, buf_len);
			} else if (!strcasecmp(token, "accessory_status")) {
				sccp_accessory_t activeAccessory = sccp_device_getActiveAccessory(d);
				snprintf(buf, buf_len, "%s:%s", sccp_accessory2str(activeAccessory), sccp_accessorystate2str(sccp_device_getAccessoryStatus(d, activeAccessory)));
			} else if (!strcasecmp(token, "registration_state")) {
				sccp_copy_string(buf, skinny_registrationstate2str(sccp_device_getRegistrationState(d)), buf_len);
			} else if (!strcasecmp(token, "codecs")) {
				sccp_codec_multiple2str(buf, buf_len - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
			} else if (!strcasecmp(token, "capability")) {
				sccp_codec_multiple2str(buf, buf_len - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
			} else if (!strcasecmp(token, "lines_registered")) {
				sccp_copy_string(buf, d->linesRegistered ? "yes" : "no", buf_len);
			} else if (!strcasecmp(token, "lines_count")) {
				snprintf(buf, buf_len, "%d", d->linesCount);
			} else if (!strcasecmp(token, "last_number")) {
				sccp_copy_string(buf, d->redialInformation.number, buf_len);
			} else if (!strcasecmp(token, "early_rtp")) {
				snprintf(buf, buf_len, "%s", sccp_earlyrtp2str(d->earlyrtp));
			} else if (!strcasecmp(token, "supported_protocol_version")) {
				snprintf(buf, buf_len, "%d", d->protocolversion);
			} else if (!strcasecmp(token, "used_protocol_version")) {
				snprintf(buf, buf_len, "%d", d->inuseprotocolversion);
			} else if (!strcasecmp(token, "mwi_light")) {
				sccp_copy_string(buf, d->mwilight ? "ON" : "OFF", buf_len);
			} else if (!strcasecmp(token, "dnd_feature")) {
				sccp_copy_string(buf, (d->dndFeature.enabled) ? "ON" : "OFF", buf_len);
			} else if (!strcasecmp(token, "dnd_state")) {
				sccp_copy_string(buf, sccp_dndmode2str(d->dndFeature.status), buf_len);
			} else if (!strcasecmp(token, "dnd_action")) {
				sccp_copy_string(buf, sccp_dndmode2str(d->dndmode), buf_len);
			} else if (!strcasecmp(token, "dynamic") || !strcasecmp(token, "realtime")) {
#ifdef CS_SCCP_REALTIME
				sccp_copy_string(buf, d->realtime ? "yes" : "no", buf_len);
#else
				sccp_copy_string(buf, "not supported", buf_len);
#endif
			} else if (!strcasecmp(token, "active_channel")) {
				snprintf(buf, buf_len, "%d", d->active_channel->callid);
			} else if (!strcasecmp(token, "transfer_channel")) {
				snprintf(buf, buf_len, "%d", d->transferChannels.transferee->callid);
#ifdef CS_SCCP_CONFERENCE
//			} else if (!strcasecmp(token, "conference_id")) {
//				snprintf(buf, buf_len, "%d", d->conference->id);
			} else if (!strcasecmp(token, "allow_conference")) {
				snprintf(buf, buf_len, "%s", d->allow_conference ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_play_general_announce")) {
				snprintf(buf, buf_len, "%s", d->conf_play_general_announce ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_play_part_announce")) {
				snprintf(buf, buf_len, "%s", d->conf_play_part_announce ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_mute_on_entry")) {
				snprintf(buf, buf_len, "%s", d->conf_mute_on_entry ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_music_on_hold_class")) {
				snprintf(buf, buf_len, "%s", d->conf_music_on_hold_class);
			} else if (!strcasecmp(token, "conf_show_conflist")) {
				snprintf(buf, buf_len, "%s", d->conf_show_conflist ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conflist_active")) {
				snprintf(buf, buf_len, "%s", d->conferencelist_active ? "ON" : "OFF");
#endif
			} else if (!strcasecmp(token, "current_line")) {
				sccp_copy_string(buf, d->currentLine->id, buf_len);
			} else if (!strcasecmp(token, "button_config")) {
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
				sccp_buttonconfig_t *config;

				SCCP_LIST_LOCK(&d->buttonconfig);
				SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
					switch (config->type) {
						case LINE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->button.line.name ? config->button.line.name : "");
							break;
						case SPEEDDIAL:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.speeddial.ext ? config->button.speeddial.ext : "");
							break;
						case SERVICE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.service.url ? config->button.service.url : "");
							break;
						case FEATURE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.feature.options ? config->button.feature.options : "");
							break;
						case EMPTY:
							pbx_str_append(&lbuf, 0, "%s[%d,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type));
							break;
						case SCCP_CONFIG_BUTTONTYPE_SENTINEL:
							break;
					}
				}
				SCCP_LIST_UNLOCK(&d->buttonconfig);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "pending_delete")) {
				sccp_copy_string(buf, d->pendingDelete ? "yes" : "no", buf_len);
			} else if (!strcasecmp(token, "pending_update")) {
				sccp_copy_string(buf, d->pendingUpdate ? "yes" : "no", buf_len);
			} else if (!strcasecmp(colname, "rtpqos")) {
				sccp_call_statistics_t *call_stats = d->call_statistics;
				snprintf(buf, buf_len, "Packets sent: %d;rcvd: %d;lost: %d;jitter: %d;latency: %d;MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d", call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_LAST].latency,
				       call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
				       call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio,
				       (int) call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);
			} else if (!strncasecmp(token, "chanvar[", 8)) {
				char *chanvar = token + 8;
				PBX_VARIABLE_TYPE *v;

				chanvar = strsep(&chanvar, "]");
				for (v = d->variables; v; v = v->next) {
					if (!strcasecmp(v->name, chanvar)) {
						sccp_copy_string(buf, v->value, buf_len);
					}
				}
			} else if (!strncasecmp(token, "codec[", 6)) {
				char *codecnum;

				codecnum = token + 6;									// move past the '[' 
				codecnum = strsep(&codecnum, "]");							// trim trailing ']' if any 
				int codec_int = sccp_atoi(codecnum, strlen(codecnum));
				if (skinny_codecs[codec_int].key) {
					sccp_copy_string(buf, codec2name(codec_int), buf_len);
				} else {
					buf[0] = '\0';
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPDEVICE(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}
			
			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPDEVICE */
static struct pbx_custom_function sccpdevice_function = {
	.name = "SCCPDEVICE",
	.synopsis = "Retrieves information about an SCCP Device",
	.syntax = "Usage: SCCPDEVICE(deviceId,<option>,<option>,...)\n",
	.read = sccp_func_sccpdevice,
	.desc = "DeviceId = Device Identifier (i.e. SEP0123456789)\n" "Option = One of the possible options mentioned in arguments\n",
#if ASTERISK_VERSION_NUMBER > 10601
	.arguments = "DeviceId = Device Identifier (i.e. SEP0123456789)\n"
	    "Option = One of these possible options:\n"
	    "ip, id, status, description, config_type, skinny_type, tz_offset, image_version, \n"
	    "accessory_status, registration_state, codecs, capability, state, lines_registered, \n" "lines_count, last_number, early_rtp, supported_protocol_version, used_protocol_version, \n" "mwi_light, dynamic, realtime, active_channel, transfer_channel, \n" "conference_id, allow_conference, conf_play_general_announce, allow_conference, \n" "conf_play_part_announce, conf_mute_on_entry, conf_music_on_hold_class, conf_show_conflist, conflist_active\n"
	    "current_line, button_config, pending_delete, chanvar[], codec[]",
#endif
};

/*!
 * \brief  ${SCCPLINE()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 * 
 */
static int sccp_func_sccpline(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;
	int addcomma = 0;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPLINE(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "id");
	}
	AUTO_RELEASE(sccp_line_t, l , NULL);
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
			/* pbx_log(LOG_WARNING, "SCCPLINE(): Not an SCCP Channel\n"); */
			return -1;
		}

		if (!c->line) {
			pbx_log(LOG_WARNING, "SCCPLINE(): SCCP Line not available\n");
			return -1;
		}
		l = sccp_line_retain(c->line);
	} else if (!strncasecmp(data, "parent", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {

			/* pbx_log(LOG_WARNING, "SCCPLINE(): Not an SCCP Channel\n"); */
			return -1;
		}

		if (!c->parentChannel || !c->parentChannel->line) {
			pbx_log(LOG_WARNING, "SCCPLINE(): SCCP Line not available\n");
			return -1;
		}
		l = sccp_line_retain(c->parentChannel->line);
	} else {
		if (!(l = sccp_line_find_byname(data, TRUE))) {
			pbx_log(LOG_WARNING, "SCCPLINE(): SCCP Line not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (l) {
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			addcomma = 0;
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			
			if (!strcasecmp(token, "id")) {
				sccp_copy_string(buf, l->id, len);
			} else if (!strcasecmp(token, "name")) {
				sccp_copy_string(buf, l->name, len);
			} else if (!strcasecmp(token, "description")) {
				sccp_copy_string(buf, l->description, len);
			} else if (!strcasecmp(token, "label")) {
				sccp_copy_string(buf, l->label, len);
			} else if (!strcasecmp(token, "vmnum")) {
				sccp_copy_string(buf, l->vmnum, len);
			} else if (!strcasecmp(token, "trnsfvm")) {
				sccp_copy_string(buf, l->trnsfvm, len);
			} else if (!strcasecmp(token, "meetme")) {
				sccp_copy_string(buf, l->meetme ? "on" : "off", len);
			} else if (!strcasecmp(token, "meetmenum")) {
				sccp_copy_string(buf, l->meetmenum, len);
			} else if (!strcasecmp(token, "meetmeopts")) {
				sccp_copy_string(buf, l->meetmeopts, len);
			} else if (!strcasecmp(token, "context")) {
				sccp_copy_string(buf, l->context, len);
			} else if (!strcasecmp(token, "language")) {
				sccp_copy_string(buf, l->language, len);
			} else if (!strcasecmp(token, "accountcode")) {
				sccp_copy_string(buf, l->accountcode, len);
			} else if (!strcasecmp(token, "musicclass")) {
				sccp_copy_string(buf, l->musicclass, len);
			} else if (!strcasecmp(token, "amaflags")) {
				sccp_copy_string(buf, l->amaflags ? "yes" : "no", len);
			} else if (!strcasecmp(token, "dnd_action")) {
				sccp_copy_string(buf, sccp_dndmode2str(l->dndmode), buf_len);
			} else if (!strcasecmp(token, "callgroup")) {
				pbx_print_group(buf, buf_len, l->callgroup);
			} else if (!strcasecmp(token, "pickupgroup")) {
#ifdef CS_SCCP_PICKUP
				pbx_print_group(buf, buf_len, l->pickupgroup);
#else
				sccp_copy_string(buf, "not supported", len);
#endif
			} else if (!strcasecmp(token, "cid_name")) {
				sccp_copy_string(buf, l->cid_name, len);
			} else if (!strcasecmp(token, "cid_num")) {
				sccp_copy_string(buf, l->cid_num, len);
			} else if (!strcasecmp(token, "incoming_limit")) {
				snprintf(buf, buf_len, "%d", l->incominglimit);
			} else if (!strcasecmp(token, "channel_count")) {
				snprintf(buf, buf_len, "%d", SCCP_RWLIST_GETSIZE(&l->channels));
			} else if (!strcasecmp(token, "dynamic") || !strcasecmp(token, "realtime")) {
#ifdef CS_SCCP_REALTIME
				sccp_copy_string(buf, l->realtime ? "Yes" : "No", len);
#else
				sccp_copy_string(buf, "not supported", len);
#endif
			} else if (!strcasecmp(token, "pending_delete")) {
				sccp_copy_string(buf, l->pendingDelete ? "yes" : "no", len);
			} else if (!strcasecmp(token, "pending_update")) {
				sccp_copy_string(buf, l->pendingUpdate ? "yes" : "no", len);
			} else if (!strcasecmp(token, "regexten")) {
				sccp_copy_string(buf, l->regexten ? l->regexten : "Unset", len);
			} else if (!strcasecmp(token, "regcontext")) {
				sccp_copy_string(buf, l->regcontext ? l->regcontext : "Unset", len);
			} else if (!strcasecmp(token, "adhoc_number")) {
				sccp_copy_string(buf, l->adhocNumber ? l->adhocNumber : "No", len);
			} else if (!strcasecmp(token, "newmsgs")) {
				snprintf(buf, buf_len, "%d", l->voicemailStatistic.newmsgs);
			} else if (!strcasecmp(token, "oldmsgs")) {
				snprintf(buf, buf_len, "%d", l->voicemailStatistic.oldmsgs);
			} else if (!strcasecmp(token, "num_devices")) {
				snprintf(buf, buf_len, "%d", SCCP_LIST_GETSIZE(&l->devices));
			} else if (!strcasecmp(token, "mailboxes")) {
				sccp_mailbox_t *mailbox;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
				SCCP_LIST_LOCK(&l->mailboxes);
				SCCP_LIST_TRAVERSE(&l->mailboxes, mailbox, list) {
					pbx_str_append(&lbuf, 0, "%s%s%s%s", addcomma++ ? "," : "", mailbox->mailbox, mailbox->context ? "@" : "", mailbox->context ? mailbox->context : "");
				}
				SCCP_LIST_UNLOCK(&l->mailboxes);
				snprintf(buf, buf_len, "%s", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "cfwd")) {
				sccp_linedevices_t *linedevice = NULL;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

				SCCP_LIST_LOCK(&l->devices);
				SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
					if (linedevice) {
						pbx_str_append(&lbuf, 0, "%s[id:%s,cfwdAll:%s,num:%s,cfwdBusy:%s,num:%s]", addcomma++ ? "," : "", linedevice->device->id, linedevice->cfwdAll.enabled ? "on" : "off", linedevice->cfwdAll.number, linedevice->cfwdBusy.enabled ? "on" : "off", linedevice->cfwdBusy.number);
					}
				}
				SCCP_LIST_UNLOCK(&l->devices);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "devices")) {
				sccp_linedevices_t *linedevice = NULL;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

				SCCP_LIST_LOCK(&l->devices);
				SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
					if (linedevice) {
						pbx_str_append(&lbuf, 0, "%s%s", addcomma++ ? "," : "", linedevice->device->id);
					}
				}
				SCCP_LIST_UNLOCK(&l->devices);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strncasecmp(token, "chanvar[", 8)) {
				char *chanvar = token + 8;

				PBX_VARIABLE_TYPE *v;

				chanvar = strsep(&chanvar, "]");
				for (v = l->variables; v; v = v->next) {
					if (!strcasecmp(v->name, chanvar)) {
						sccp_copy_string(buf, v->value, len);
					}
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPLINE(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}

			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPLINE */
static struct pbx_custom_function sccpline_function = {
	.name = "SCCPLINE",
	.synopsis = "Retrieves information about an SCCP Line",
	.syntax = "Usage: SCCPLINE(lineName,<option>,...)\n",
	.read = sccp_func_sccpline,
	.desc = "LineName = Name of the line to be queried.\n" "Option = One of the possible options mentioned in arguments\n",
#if ASTERISK_VERSION_NUMBER > 10601
	.arguments = "LineName = use on off these: 'current', 'parent', actual linename\n" "Option = One of these possible options:\n" "id, name, description, label, vmnum, trnsfvm, meetme, meetmenum, meetmeopts, context, \n" "language, accountcode, musicclass, amaflags, callgroup, pickupgroup, cid_name, cid_num, \n" "incoming_limit, channel_count, dynamic, realtime, pending_delete, pending_update, \n" "regexten, regcontext, adhoc_number, newmsgs, oldmsgs, num_lines, cfwd, devices, chanvar[]"
#endif
};

/*!
 * \brief  ${SCCPCHANNEL()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 */
static int sccp_func_sccpchannel(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	PBX_CHANNEL_TYPE *ast;
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPCHANNEL(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "callid");
	}

	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
			return -1;										/* Not a SCCP channel. */
		}
	} else if (iPbx.getChannelByName(data, &ast) && (c = get_sccp_channel_from_pbx_channel(ast)) != NULL) {
		/* continue with sccp channel */

	} else {
		uint32_t callid = sccp_atoi(data, strlen(data));

		if (!(c = sccp_channel_find_byid(callid))) {
			pbx_log(LOG_WARNING, "SCCPCHANNEL(): SCCP Channel not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (c) {
		sccp_callinfo_t *ci = sccp_channel_getCallInfo(c);
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			

			if (!strcasecmp(token, "callid") || !strcasecmp(token, "id")) {
				snprintf(buf, buf_len, "%d", c->callid);
			} else if (!strcasecmp(token, "format")) {
				snprintf(buf, buf_len, "%d", c->rtp.audio.readFormat);
			} else if (!strcasecmp(token, "codecs")) {
				sccp_copy_string(buf, codec2name(c->rtp.audio.readFormat), len);
			} else if (!strcasecmp(token, "capability")) {
				sccp_codec_multiple2str(buf, buf_len - 1, c->capabilities.audio, ARRAY_LEN(c->capabilities.audio));
			} else if (!strcasecmp(token, "calledPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "calledPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "callingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "callingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCallingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCallingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCalledPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCalledPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "cgpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "cdpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCdpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "passthrupartyid")) {
				snprintf(buf, buf_len, "%d", c->passthrupartyid);
			} else if (!strcasecmp(token, "state")) {
				sccp_copy_string(buf, sccp_channelstate2str(c->state), len);
			} else if (!strcasecmp(token, "previous_state")) {
				sccp_copy_string(buf, sccp_channelstate2str(c->previousChannelState), len);
			} else if (!strcasecmp(token, "calltype")) {
				sccp_copy_string(buf, skinny_calltype2str(c->calltype), len);
			} else if (!strcasecmp(token, "ringtype")) {
				sccp_copy_string(buf, skinny_ringtype2str(c->ringermode), len);
			} else if (!strcasecmp(token, "dialed_number")) {
				sccp_copy_string(buf, c->dialedNumber, len);
			} else if (!strcasecmp(token, "device")) {
				sccp_copy_string(buf, c->currentDeviceId, len);
			} else if (!strcasecmp(token, "line")) {
				sccp_copy_string(buf, c->line->name, len);
			} else if (!strcasecmp(token, "answered_elsewhere")) {
				sccp_copy_string(buf, c->answered_elsewhere ? "yes" : "no", len);
			} else if (!strcasecmp(token, "privacy")) {
				sccp_copy_string(buf, c->privacy ? "yes" : "no", len);
			} else if (!strcasecmp(token, "softswitch_action")) {
				snprintf(buf, buf_len, "%s (%d)", sccp_softswitch2str(c->softswitch_action), c->softswitch_action);
			// } else if (!strcasecmp(token, "monitorEnabled")) {
				//sccp_copy_string(buf, c->monitorEnabled ? "yes" : "no", len);
#ifdef CS_SCCP_CONFERENCE
			} else if (!strcasecmp(token, "conference_id")) {
				snprintf(buf, buf_len, "%d", c->conference_id);
			} else if (!strcasecmp(token, "conference_participant_id")) {
				snprintf(buf, buf_len, "%d", c->conference_participant_id);
#endif
			} else if (!strcasecmp(token, "parent")) {
				snprintf(buf, buf_len, "%d", c->parentChannel->callid);
			} else if (!strcasecmp(token, "bridgepeer")) {
				PBX_CHANNEL_TYPE *bridgechannel;
				if (c->owner && (bridgechannel = iPbx.get_bridged_channel(c->owner))) {
					snprintf(buf, buf_len, "%s", pbx_channel_name(bridgechannel));
					pbx_channel_unref(bridgechannel);
				} else {
					snprintf(buf, buf_len, "<unknown>");
				}
			} else if (!strcasecmp(token, "peerip")) {							// NO-NAT (Ip-Address Associated with the Session->sin)
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getOurIP(d->session, &sas, 0);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(token, "recvip")) {							// NAT (Actual Source IP-Address Reported by the phone upon registration)
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getSas(d->session, &sas);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(colname, "rtpqos")) {
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					sccp_call_statistics_t *call_stats = d->call_statistics;
					snprintf(buf, buf_len, "Packets sent: %d;rcvd: %d;lost: %d;jitter: %d;latency: %d;MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d", call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_LAST].latency,
					       call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
					       call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio,
					       (int) call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);
				}
			} else if (!strncasecmp(token, "codec[", 6)) {
				char *codecnum;

				codecnum = token + 6;									// move past the '[' 
				codecnum = strsep(&codecnum, "]");							// trim trailing ']' if any 
				int codec_int = sccp_atoi(codecnum, strlen(codecnum));
				if (skinny_codecs[codec_int].key) {
					sccp_copy_string(buf, codec2name(codec_int), buf_len);
				} else {
					buf[0] = '\0';
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPCHANNEL(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}

			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPCHANNEL */
static struct pbx_custom_function sccpchannel_function = {
	.name = "SCCPCHANNEL",
	.synopsis = "Retrieves information about an SCCP Line",
	.syntax = "Usage: SCCPCHANNEL(channelId,<option>,<option>,...)\n",
	.read = sccp_func_sccpchannel,
	.desc = "ChannelId = Name of the line to be queried.\n" "Option = One of the possible options mentioned in arguments\n",
#if ASTERISK_VERSION_NUMBER > 10601
	.arguments = "ChannelId = use on off these: 'current', actual callid\n"
	    "Option = One of these possible options:\n"
	    "callid, id, format, codecs, capability, calledPartyName, calledPartyNumber, callingPartyName, \n"
	    "callingPartyNumber, originalCallingPartyName, originalCallingPartyNumber, originalCalledPartyName, \n" "originalCalledPartyNumber, lastRedirectingPartyName, lastRedirectingPartyNumber, cgpnVoiceMailbox, \n" "cdpnVoiceMailbox, originalCdpnVoiceMailbox, lastRedirectingVoiceMailbox, passthrupartyid, state, \n" "previous_state, calltype, dialed_number, device, line, answered_elsewhere, privacy, ss_action, \n" "monitorEnabled, conference_id, conference_participant_id, parent, bridgepeer, peerip, recvip, codec[]"
#endif
};

/*!
 * \brief       Set the Preferred Codec for a SCCP channel via the dialplan
 * \param       chan Asterisk Channel
 * \param       data single codec name
 * \return      Success as int
 * 
 * \called_from_asterisk
 * \deprecated
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);
	int res;

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetCodec: Not an SCCP channel\n");
		return -1;
	}

	res = sccp_channel_setPreferredCodec(c, data);
	pbx_log(LOG_WARNING, "SCCPSetCodec: Is now deprecated. Please use 'Set(CHANNEL(codec)=%s)' insteadl.\n", (char *) data);
	return res ? 0 : -1;
}

/*! \brief Stucture to declare a dialplan function: SETSCCPCODEC */
static char *prefcodec_name = "SCCPSetCodec";
static char *old_prefcodec_name = "SetSCCPCodec";
static char *prefcodec_synopsis = "Sets the preferred codec for the current sccp channel (DEPRECATED use generic 'Set(CHANNEL(codec)=alaw)' instead)";
static char *prefcodec_descr = "Usage: SCCPSetCodec(codec)" "Sets the preferred codec for dialing out with the current chan_sccp channel\nDEPRECATED use generic 'Set(CHANNEL(codec)=alaw)' instead\n";

/*!
 * \brief       Set the Name and Number of the Called Party to the Calling Phone
 * \param       chan Asterisk Channel
 * \param       data CallerId in format "Name" \<number\>
 * \return      Success as int
 * 
 * \called_from_asterisk
 * \deprecated
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	char *text = (char *) data;
	char *num, *name;
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetCalledParty: Not an SCCP channel\n");
		return 0;
	}

	if (!text) {
		pbx_log(LOG_WARNING, "SCCPSetCalledParty: No CalledParty Information Provided\n");
		return 0;
	}

	pbx_callerid_parse(text, &name, &num);
	sccp_channel_set_calledparty(c, name, num);
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SETCALLEDPARTY */
static char *calledparty_name = "SCCPSetCalledParty";
static char *old_calledparty_name = "SetCalledParty";
static char *calledparty_synopsis = "Sets the callerid of the called party (DEPRECATED use generic 'Set(CHANNEL(calledparty)=\"name <exten>\")' instead)";
static char *calledparty_descr = "Usage: SCCPSetCalledParty(\"Name\" <ext>)" "Sets the name and number of the called party for use with chan_sccp\n";

/*!
 * \brief       It allows you to send a message to the calling device.
 * \author      Frank Segtrop <fs@matflow.net>
 * \param       chan asterisk channel
 * \param       data message to sent - if empty clear display
 * \version     20071112_1944
 *
 * \called_from_asterisk
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetMessage: Not an SCCP channel\n");
		return 0;
	}

	int timeout = 0;
	int priority = -1;

        char *parse = pbx_strdupa(data);
        AST_DECLARE_APP_ARGS(args,
                AST_APP_ARG(text);
                AST_APP_ARG(timeout);
                AST_APP_ARG(priority);
        );
        AST_STANDARD_APP_ARGS(args, parse);
        
        if (!sccp_strlen_zero(args.timeout)) {
        	timeout = sccp_atoi(args.timeout, strlen(args.timeout));
        }
        if (!sccp_strlen_zero(args.priority)) {
        	priority = sccp_atoi(args.priority, strlen(args.priority));
        }

	AUTO_RELEASE(sccp_device_t, d , NULL);
	if (!(d = sccp_channel_getDevice(c))) {
		pbx_log(LOG_WARNING, "SCCPSetMessage: Not an SCCP device provided\n");
		return 0;
	}
	
	pbx_log(LOG_WARNING, "SCCPSetMessage: text:'%s', prio:%d, timeout:%d\n", args.text, priority, timeout);
	if (!sccp_strlen_zero(args.text)) {
		if (priority > -1) {
			sccp_dev_displayprinotify(d, args.text, priority, timeout);
		} else {
			sccp_dev_set_message(d, args.text, timeout, TRUE, FALSE);
		}
	} else {
		if (priority > -1) {
			sccp_dev_cleardisplayprinotify(d, priority);
		} else {
			sccp_dev_clear_message(d, TRUE);
		}
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SETMESSAGE */
static char *setmessage_name = "SCCPSetMessage";
static char *old_setmessage_name = "SetMessage";
static char *setmessage_synopsis = "Send a Message to the current Phone";
static char *setmessage_descr = "Usage: SCCPSetMessage(\"Message\"[,timeout][,priority])\n" "       Send a Message to the Calling Device (and remove after timeout, if timeout is ommited will stay until next/empty message). Use priority to set/clear priority notifications. Higher priority levels overrule lower ones.\n";

int sccp_register_dialplan_functions(void)
{
	int result = 0;

	/* Register application functions */
	result = pbx_register_application(calledparty_name, sccp_app_calledparty, calledparty_synopsis, calledparty_descr, NULL);
	result |= pbx_register_application(setmessage_name, sccp_app_setmessage, setmessage_synopsis, setmessage_descr, NULL);
	result |= pbx_register_application(prefcodec_name, sccp_app_prefcodec, prefcodec_synopsis, prefcodec_descr, NULL);
	/* old names */
	result |= pbx_register_application(old_calledparty_name, sccp_app_calledparty, calledparty_synopsis, calledparty_descr, NULL);
	result |= pbx_register_application(old_setmessage_name, sccp_app_setmessage, setmessage_synopsis, setmessage_descr, NULL);
	result |= pbx_register_application(old_prefcodec_name, sccp_app_prefcodec, prefcodec_synopsis, prefcodec_descr, NULL);

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
	/* old names */
	result |= pbx_unregister_application(old_calledparty_name);
	result |= pbx_unregister_application(old_setmessage_name);
	result |= pbx_unregister_application(old_prefcodec_name);

	/* Unregister dial plan functions */
	result |= pbx_custom_function_unregister(&sccpdevice_function);
	result |= pbx_custom_function_unregister(&sccpline_function);
	result |= pbx_custom_function_unregister(&sccpchannel_function);

	return result;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
