/*!
 * \file 	sccp_cli.c
 * \brief 	SCCP CLI Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$
 */

/*!
 * \remarks	Purpose: 	SCCP CLI
 * 		When to use:	Only methods directly related to the asterisk cli interface should be stored in this source file.
 *   		Relationships: 	Calls ???
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_cli.h"
#include "sccp_mwi.h"
#include "sccp_line.h"
#include "sccp_indicate.h"
#include "sccp_utils.h"
#include "sccp_hint.h"
#include "sccp_device.h"
#include "sccp_socket.h"
#include "sccp_config.h"
#include <asterisk/utils.h>
#include <asterisk/cli.h>
#include <asterisk/astdb.h>
#include <asterisk/pbx.h>

#ifdef CS_AST_HAS_EVENT
#include "asterisk/event.h"
#endif

/* ------------------------------------------------------------ */

#ifdef CS_NEW_AST_CLI
/*!
 * \brief Complete Device
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 */
static char * sccp_complete_device(const char *line, const char *word, int pos, int state)
{
#else
/*!
 * \brief Complete Device
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 */
static char * sccp_complete_device(char *line, char *word, int pos, int state) {
#endif
	sccp_device_t * d;
	int which = 0;
	char * ret;

	if (pos > 3)
	return NULL;

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strncasecmp(word, d->id, strlen(word))) {
			if (++which > state)
				break;
		}
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));

	ret = d ? strdup(d->id) : NULL;

	return ret;
}

/*!
 * \brief Reset/Restart
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_reset_restart(int fd, int argc, char * argv[]) {
	sccp_device_t * d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], FALSE);

	if (!d) {
		ast_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_SUCCESS;
	}


	sccp_device_lock(d);
	if (!d->session) {
		ast_cli(fd, "%s: device not registered\n", argv[2]);
		sccp_device_unlock(d);
		return RESULT_SUCCESS;
	}


	if(d->channelCount > 0) {
		/* sccp_device_clean will check active channels */
		//ast_cli(fd, "%s: unable to %s device with active channels. Hangup first\n", argv[2], (!strcasecmp(argv[1], "reset")) ? "reset" : "restart");
		//return RESULT_SUCCESS;
	}
	sccp_device_unlock(d);
	//ast_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);


	sccp_device_sendReset(d, (!strcasecmp(argv[1], "reset")) ? SKINNY_DEVICE_RESET : SKINNY_DEVICE_RESTART );
	//sccp_dev_clean(d, FALSE);
	sccp_session_close(d->session);


	return RESULT_SUCCESS;
}

/*!
 * \brief Unregister
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_unregister(int fd, int argc, char * argv[]) {
	sccp_moo_t * r;
	sccp_device_t * d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	ast_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], TRUE);

	if (!d) {
#ifdef CS_SCCP_REALTIME
		d = sccp_device_find_realtime_byid(argv[2]);
		if(!d)
			ast_cli(fd, "Can't find device %s\n", argv[2]);
#else
		ast_cli(fd, "Can't find device %s\n", argv[2]);
#endif
	}
	if(!d)
		return RESULT_SUCCESS;

	sccp_device_lock(d);

	if (!d->session) {
		ast_cli(fd, "%s: device not registered\n", argv[2]);
		sccp_device_unlock(d);
		return RESULT_SUCCESS;
	}

	ast_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);

	sccp_device_unlock(d);

	REQ(r, RegisterRejectMessage);
	strncpy(r->msg.RegisterRejectMessage.text, "Unregister user request", StationMaxDisplayTextSize);
	sccp_dev_send(d, r);

	return RESULT_SUCCESS;
}

/* ------------------------------------------------------------ */
/*!
 * \brief Print Group
 * \param buf Buf as char
 * \param buflen Buffer Lendth as int
 * \param group Group as ast_group_t
 * \return Result as char
 */
static char *sccp_print_group(char *buf, int buflen, ast_group_t group) {
	unsigned int i;
	int first=1;
	char num[3];
	uint8_t max = (sizeof(ast_group_t) * 8) - 1;

	buf[0] = '\0';

	if (!group)
		return(buf);

	for (i=0; i<=max; i++) {
		if (group & ((ast_group_t) 1 << i)) {
	   		if (!first) {
				strncat(buf, ", ", buflen);
			} else {
				first=0;
	  		}
			snprintf(num, sizeof(num), "%u", i);
			strncat(buf, num, buflen);
		}
	}
	return(buf);
}

/*!
 * \brief Show Globals
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_globals(int fd, int argc, char * argv[]) {
	char pref_buf[128];
	char cap_buf[512];
	char buf[256];
	char * debugcategories="";
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	sccp_globals_lock(lock);
	ast_codec_pref_string(&GLOB(global_codecs), pref_buf, sizeof(pref_buf) - 1);
	ast_getformatname_multiple(cap_buf, sizeof(cap_buf), GLOB(global_capability)),

	ast_cli(fd, "SCCP channel driver global settings\n");
	ast_cli(fd, "------------------------------------\n\n");
#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	ast_cli(fd, "Platform byte order   : LITTLE ENDIAN\n");
#else
	ast_cli(fd, "Platform byte order   : BIG ENDIAN\n");
#endif
	ast_cli(fd, "Protocol Version      : %d\n", GLOB(protocolversion));
	ast_cli(fd, "Server Name           : %s\n", GLOB(servername));
#ifdef ASTERISK_CONF_1_2
	ast_cli(fd, "Bind Address          : %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
	ast_cli(fd, "Bind Address          : %s:%d\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif
	ast_cli(fd, "Nat                   : %s\n", (GLOB(nat)) ? "Yes" : "No");
	ast_cli(fd, "Direct RTP            : %s\n", (GLOB(directrtp)) ? "Yes" : "No");
	ast_cli(fd, "Keepalive             : %d\n", GLOB(keepalive));
	ast_cli(fd, "Debug                 : (%d) %s\n", GLOB(debug),sccp_get_debugcategories(GLOB(debug),debugcategories));
	ast_cli(fd, "Date format           : %s\n", GLOB(date_format));
	ast_cli(fd, "First digit timeout   : %d\n", GLOB(firstdigittimeout));
	ast_cli(fd, "Digit timeout         : %d\n", GLOB(digittimeout));
	ast_cli(fd, "SCCP tos (signaling)  : %d\n", GLOB(sccp_tos));
	ast_cli(fd, "SCCP cos (signaling)  : %d\n", GLOB(sccp_cos));
	ast_cli(fd, "AUDIO tos (rtp)       : %d\n", GLOB(audio_tos));
	ast_cli(fd, "AUDIO cos (rtp)       : %d\n", GLOB(audio_cos));
	ast_cli(fd, "VIDEO tos (vrtp)      : %d\n", GLOB(video_tos));
	ast_cli(fd, "VIDEO cos (vrtp)      : %d\n", GLOB(video_cos));
	ast_cli(fd, "Context               : %s\n", GLOB(context));
	ast_cli(fd, "Language              : %s\n", GLOB(language));
	ast_cli(fd, "Accountcode           : %s\n", GLOB(accountcode));
	ast_cli(fd, "Musicclass            : %s\n", GLOB(musicclass));
	ast_cli(fd, "AMA flags             : %d - %s\n", GLOB(amaflags), ast_cdr_flags2str(GLOB(amaflags)));
	ast_cli(fd, "Callgroup             : %s\n", sccp_print_group(buf, sizeof(buf), GLOB(callgroup)));
#ifdef CS_SCCP_PICKUP
	ast_cli(fd, "Pickupgroup           : %s\n", sccp_print_group(buf, sizeof(buf), GLOB(pickupgroup)));
#endif
	ast_cli(fd, "Capabilities          : %s\n", cap_buf);
	ast_cli(fd, "Codecs preference     : %s\n", pref_buf);
	ast_cli(fd, "CFWDALL               : %s\n", (GLOB(cfwdall)) ? "Yes" : "No");
	ast_cli(fd, "CFWBUSY               : %s\n", (GLOB(cfwdbusy)) ? "Yes" : "No");
	ast_cli(fd, "CFWNOANSWER           : %s\n", (GLOB(cfwdnoanswer)) ? "Yes" : "No");
#ifdef CS_MANAGER_EVENTS
	ast_cli(fd, "Call Events:          : %s\n", (GLOB(callevents)) ? "Yes" : "No");
#else
	ast_cli(fd, "Call Events:          : Disabled\n");
#endif
	ast_cli(fd, "DND                   : %s\n", GLOB(dndmode) ? dndmode2str(GLOB(dndmode)) : "Disabled");
#ifdef CS_SCCP_PARK
	ast_cli(fd, "Park                  : Enabled\n");
#else
	ast_cli(fd, "Park                  : Disabled\n");
#endif
	ast_cli(fd, "Private softkey       : %s\n", GLOB(privacy) ? "Enabled" : "Disabled");
	ast_cli(fd, "Echo cancel           : %s\n", GLOB(echocancel) ? "Enabled" : "Disabled");
	ast_cli(fd, "Silence suppression   : %s\n", GLOB(silencesuppression) ? "Enabled" : "Disabled");
	ast_cli(fd, "Trust phone ip        : %s\n", GLOB(trustphoneip) ? "Yes" : "No");
	ast_cli(fd, "Early RTP             : %s\n", GLOB(earlyrtp) ? "Yes" : "No");
	ast_cli(fd, "AutoAnswer ringtime   : %d\n", GLOB(autoanswer_ring_time));
	ast_cli(fd, "AutoAnswer tone       : %d\n", GLOB(autoanswer_tone));
	ast_cli(fd, "RemoteHangup tone     : %d\n", GLOB(remotehangup_tone));
	ast_cli(fd, "Transfer tone         : %d\n", GLOB(transfer_tone));
	ast_cli(fd, "CallWaiting tone      : %d\n", GLOB(callwaiting_tone));
	ast_cli(fd, "Jitterbuffer enabled  : %s\n", (ast_test_flag(&GLOB(global_jbconf), AST_JB_ENABLED) ? "Yes" : "No"));
	ast_cli(fd, "Jitterbuffer forced   : %s\n", (ast_test_flag(&GLOB(global_jbconf), AST_JB_FORCED) ? "Yes" : "No"));
	ast_cli(fd, "Jitterbuffer max size : %ld\n", GLOB(global_jbconf).max_size);
	ast_cli(fd, "Jitterbuffer resync   : %ld\n", GLOB(global_jbconf).resync_threshold);
	ast_cli(fd, "Jitterbuffer impl     : %s\n",  GLOB(global_jbconf).impl);
	ast_cli(fd, "Jitterbuffer log      : %s\n", (ast_test_flag(&GLOB(global_jbconf), AST_JB_LOG) ? "Yes" : "No"));
#ifdef CS_AST_JB_TARGET_EXTRA
	ast_cli(fd, "Jitterbuf target extra: %ld\n",  GLOB(global_jbconf).target_extra);
#endif

	sccp_globals_unlock(lock);

	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Show Globals
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_globals(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
			e->command = "sccp show globals";
			e->usage =
				"Usage: sccp show globals\n";
			return NULL;
		} else if (cmd == CLI_GENERATE){
			return NULL;
		}
		if (a->argc != 3)
			return CLI_SHOWUSAGE;

		if(sccp_show_globals(a->fd, a->argc, a->argv) == RESULT_SUCCESS) {
			return CLI_SUCCESS;
		}else
			return CLI_FAILURE;

}

#else
/*
 * \brief CLI Show Global
 * \note Alias for Asterisk CLI ENTRY
 */
static struct ast_cli_entry cli_show_globals = {
  { "sccp", "show", "globals", NULL },
  sccp_show_globals,
  "Show SCCP global settings",
  "Usage: sccp show globals\n"
};
#endif

/* ------------------------------------------------------------ */
/*!
 * \brief Show Line
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_device(int fd, int argc, char * argv[]) {
	sccp_device_t 	* d;
	sccp_buttonconfig_t	*config = NULL;
	sccp_line_t 		* l;
	char pref_buf[128];
	char cap_buf[512];
	struct ast_variable *v = NULL;
	sccp_linedevices_t	*linedevice;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	d = sccp_device_find_byid(argv[3], TRUE);
	if (!d) {
		ast_cli(fd, "Can't find settings for device %s\n", argv[3]);
		return RESULT_SUCCESS;
	}
	sccp_device_lock(d);

	ast_codec_pref_string(&d->codecs, pref_buf, sizeof(pref_buf) - 1);
	ast_getformatname_multiple(cap_buf, sizeof(cap_buf), d->capability),

	ast_cli(fd, "Current settings for selected Device\n");
	ast_cli(fd, "------------------------------------\n\n");
	ast_cli(fd, "MAC-Address        : %s\n", d->id);
	ast_cli(fd, "Protocol Version   : Supported '%d', In Use '%d'\n", d->protocolversion, d->inuseprotocolversion);
	ast_cli(fd, "Keepalive          : %d\n", d->keepalive);
	ast_cli(fd, "Registration state : %s(%d)\n", deviceregistrationstatus2str(d->registrationState), d->registrationState);
	ast_cli(fd, "State              : %s(%d)\n", devicestatus2str(d->state), d->state);
	ast_cli(fd, "MWI handset light  : %s\n", (d->mwilight) ? "ON" : "OFF");
	ast_cli(fd, "Description        : %s\n", d->description);
	ast_cli(fd, "Config Phone Type  : %s\n", d->config_type);
	ast_cli(fd, "Skinny Phone Type  : %s(%d)\n", devicetype2str(d->skinny_type), d->skinny_type);
	ast_cli(fd, "Softkey support    : %s\n", (d->softkeysupport) ? "Yes" : "No");
	ast_cli(fd, "Image Version      : %s\n", d->imageversion);
	ast_cli(fd, "Timezone Offset    : %d\n", d->tz_offset);
	ast_cli(fd, "Capabilities       : %s\n", cap_buf);
	ast_cli(fd, "Codecs preference  : %s\n", pref_buf);
	ast_cli(fd, "DND Feature enabled: %s\n", (d->dndFeature.enabled) ? "YES" : "NO");
	ast_cli(fd, "DND Status         : %s\n", (d->dndFeature.status) ? dndmode2str(d->dndFeature.status) : "Disabled");
	ast_cli(fd, "Can Transfer       : %s\n", (d->transfer) ? "Yes" : "No");
	ast_cli(fd, "Can Park           : %s\n", (d->park) ? "Yes" : "No");
	ast_cli(fd, "Private softkey    : %s\n", d->privacyFeature.enabled ? "Enabled" : "Disabled");
	ast_cli(fd, "Can CFWDALL        : %s\n", (d->cfwdall) ? "Yes" : "No");
	ast_cli(fd, "Can CFWBUSY        : %s\n", (d->cfwdbusy) ? "Yes" : "No");
	ast_cli(fd, "Can CFWNOANSWER    : %s\n", (d->cfwdnoanswer) ? "Yes" : "No");
	ast_cli(fd, "Dtmf mode          : %s\n", (d->dtmfmode) ? "Out-of-Band" : "In-Band");
	ast_cli(fd, "Nat                : %s\n", (d->nat) ? "Yes" : "No");
	ast_cli(fd, "Direct RTP         : %s\n", (d->directrtp) ? "Yes" : "No");
	ast_cli(fd, "Trust phone ip     : %s\n", (d->trustphoneip) ? "Yes" : "No");
	ast_cli(fd, "Early RTP          : %s\n", (d->earlyrtp) ? "Yes" : "No");
	ast_cli(fd, "Device State (Acc.): %s\n", accessorystatus2str(d->accessorystatus));
	ast_cli(fd, "Last Used Accessory: %s\n", accessory2str(d->accessoryused));
	ast_cli(fd, "Last dialed number : %s\n", d->lastNumber);
	ast_cli(fd, "Use Placed Clalls  : %s\n", (d->useRedialMenu) ? "ON" : "OFF");

	if (SCCP_LIST_FIRST(&d->buttonconfig)) {
		ast_cli(fd, "\nButtonconfig\n");
		ast_cli(fd, "%-4s: %s\n", "id", "type");
		ast_cli(fd, "--------------------------------\n");

		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			ast_cli(fd, "%4d: %s(%d)\n", config->instance, sccp_buttontype2str(config->type), config->type);
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
	}

	ast_cli(fd, "\nLines\n");
	ast_cli(fd, "%-4s: %-20s %-5s %-20s %-10s %-20s\n", "id", "name", "suffix", "label", "cfwdType", "cfwdNumber");
	ast_cli(fd, "---------------------------------------------------------------------\n");

	sccp_buttonconfig_t *buttonconfig;
	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if(buttonconfig->type == LINE ){
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
			if(l){
				linedevice = sccp_util_getDeviceConfiguration(d, l);
				if(linedevice && linedevice->cfwdAll.enabled)
					ast_cli(fd, "%4d: %-20s %-5s %-20s %-10s %-20s\n", buttonconfig->instance, l->name , buttonconfig->button.line.subscriptionId.number, l->label, "all", linedevice->cfwdAll.number);
				else if(linedevice && linedevice->cfwdBusy.enabled)
					ast_cli(fd, "%4d: %-20s %-5s %-20s %-10s %-20s\n", buttonconfig->instance, l->name , buttonconfig->button.line.subscriptionId.number, l->label, "busy", linedevice->cfwdBusy.number);
				else
					ast_cli(fd, "%4d: %-20s %-5s %-20s %-10s %-20s\n", buttonconfig->instance, l->name , buttonconfig->button.line.subscriptionId.number, l->label, "", "");
			}
		}
	}

	if (SCCP_LIST_FIRST(&d->buttonconfig)) {
		ast_cli(fd, "\nSpeeddials\n");
		ast_cli(fd, "%-4s: %-24s %-24s %-20s\n", "id", "name" , "number", "hint");
		ast_cli(fd, "---------------------------------------------------------------------\n");

		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if(config->type == SPEEDDIAL)
				ast_cli(fd, "%4d: %-24s %-24s %-20s\n", config->instance, config->button.speeddial.label , config->button.speeddial.ext, config->button.speeddial.hint);
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
	}

	if (SCCP_LIST_FIRST(&d->buttonconfig)) {
		ast_cli(fd, "\nService URLs\n");
		ast_cli(fd, "%-4s: %-20s %-20s\n", "id", "label" , "URL");
		ast_cli(fd, "------------------------------------\n");

		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			if(config->type == SERVICE)
				ast_cli(fd, "%4d: %-20s %-20s\n", config->instance, config->button.service.label , config->button.service.url);
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);

	}

	if (d->variables) {
		ast_cli(fd, "\nDevice variables\n");
		ast_cli(fd, "%-4s: %-20s \n", "name" , "value");
		ast_cli(fd, "------------------------------------\n");

		for (v = d->variables ; v ; v = v->next) {
			ast_cli(fd, "%-20s : %-20s\n", v->name , v->value);
		}
	}
	sccp_device_unlock(d);
	

	int instance = 1;
	int status = 1;
	
	
	if (argc > 4){
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: argv[4]=%s\n", d->id, argv[4]);
		instance =  atoi(argv[4]);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: instance=%d(%s)\n", d->id, instance, argv[4]);
	}
	
	if (argc > 5){
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: argv[5]=%s\n", d->id, argv[5]);
		status =  atoi(argv[5]);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: status=%d(%s)\n", d->id, status, argv[5]);
	}


	
	
	
	

#if 0	
	sccp_moo_t 	*r1 = NULL;
// 	struct {
// 		uint32_t	lel_appID;
// 		uint32_t	lel_lineInstance;
// 		uint32_t	lel_callReference;
// 		uint32_t	lel_transactionId;
// 		uint32_t	lel_dataLength;
// 		uint32_t	lel_sequenceFlag;
// 		uint32_t	lel_displayPriority;
// 		uint32_t	lel_conferenceID;
// 		uint32_t	lel_appInstanceId;
// 		uint32_t	lel_routing;
// 		char		*data;
// 	} UserToDeviceDataVersion1Message;
	char *data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\"URL=\"Key:Directories\"/><ExecuteItem Priority=\"0\"URL=\"Key:KeyPad1\"/></CiscoIPPhoneExecute>";
	
	
	int dummy_len = strlen(data);

	int hdr_len = 40 - 1;
	int padding = ((dummy_len + hdr_len) % 4);
	padding = (padding > 0) ? 4 - padding : 0;
	int size = hdr_len + dummy_len + padding;
	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: dummy_len=%d\n", d->id, dummy_len);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: hdr_len=%d\n", d->id, hdr_len);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: padding=%d\n", d->id, padding);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: size=%d\n", d->id, size);

	r1 = sccp_build_packet(UserToDeviceDataVersion1Message, size);
	r1->msg.UserToDeviceDataVersion1Message.lel_callReference = htolel(1);
	r1->msg.UserToDeviceDataVersion1Message.lel_transactionId = htolel(1);
	r1->msg.UserToDeviceDataVersion1Message.lel_sequenceFlag = 0x0002;
	r1->msg.UserToDeviceDataVersion1Message.lel_displayPriority = 0x0002;
	r1->msg.UserToDeviceDataVersion1Message.lel_dataLength = htolel(dummy_len);

	if(dummy_len) {
		char buffer[dummy_len + 2];
		memset(&buffer[0], 0, dummy_len + 2);
		memcpy(&buffer[0], data, dummy_len);
		
		memcpy(&r1->msg.UserToDeviceDataVersion1Message.dummy, &buffer[0], dummy_len + 2);
		sccp_dev_send(d, r1);
	}

	sccp_moo_t 	*r1 = NULL, *r = NULL;
	REQ(r1, ForwardStatMessage);
	r1->msg.ForwardStatMessage.lel_status = htolel(status);
	r1->msg.ForwardStatMessage.lel_lineNumber = htolel(instance);
	
	
	r1->msg.ForwardStatMessage.lel_cfwdallstatus = htolel(status);
	if(status)
		sccp_copy_string(r1->msg.ForwardStatMessage.cfwdallnumber, "1234", sizeof(r1->msg.ForwardStatMessage.cfwdallnumber));
	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_status=%d\n", d->id, l->name, r1->msg.ForwardStatMessage.lel_status);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_lineNumber=%d\n", d->id, l->name, r1->msg.ForwardStatMessage.lel_lineNumber);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_cfwdallstatus=%d\n", d->id, l->name, r1->msg.ForwardStatMessage.lel_cfwdallstatus);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_cfwdallstatus=%s\n", d->id, l->name, r1->msg.ForwardStatMessage.cfwdallnumber);
	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_cfwdbusystatus=%d\n", d->id, l->name, r1->msg.ForwardStatMessage.lel_cfwdbusystatus);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Send Forward Status Line: %s, ForwardStatMessage.lel_cfwdallstatus=%s\n", d->id, l->name, r1->msg.ForwardStatMessage.cfwdbusynumber);
	sccp_dev_send(d, r1);
	
	
// 	usleep(300);
// 	REQ(r, SetLampMessage);
// 	r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_FORWARDALL);
// 	r->msg.SetLampMessage.lel_stimulusInstance = htolel(instance);
// 	r->msg.SetLampMessage.lel_lampMode = htolel(SKINNY_LAMP_ON);
// 	sccp_dev_send(d, r);


 	
//  	REQ(r1, ButtonTemplateMessage);
//  	r1->msg.ButtonTemplateMessage.lel_buttonOffset = htolel(instance-1);
//  	
//  	//r1->msg.ButtonTemplateMessage.definition[instance-1].instanceNumber = instance;
//  	//r1->msg.ButtonTemplateMessage.definition[instance-1].buttonDefinition = htolel(SKINNY_BUTTONTYPE_TRANSFER);
// 	
// 	r1->msg.ButtonTemplateMessage.definition[0].instanceNumber = instance;
//  	r1->msg.ButtonTemplateMessage.definition[0].buttonDefinition = htolel(SKINNY_BUTTONTYPE_TRANSFER);
//  	
//  	r1->msg.ButtonTemplateMessage.lel_buttonCount = htolel(1);
//  	r1->msg.ButtonTemplateMessage.lel_totalButtonCount = htolel(22);
// // 	sccp_dev_send(d, r1);
// 	
// 	sccp_dev_send(d, r1);
	
	
	char tmp[256] = "linename";
	strcat(tmp," " SKINNY_DISP_CFWDALL ":");
	strcat(tmp," 1234");
	
	REQ(r1, LineStatMessage);
	r1->msg.LineStatMessage.lel_lineNumber = htolel(instance);
	sccp_copy_string(r1->msg.LineStatMessage.lineDirNumber, tmp, sizeof(r1->msg.LineStatMessage.lineDirNumber));
	sccp_copy_string(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName, "test", sizeof(r1->msg.LineStatMessage.lineFullyQualifiedDisplayName));
	sccp_copy_string(r1->msg.LineStatMessage.lineDisplayName, "label", sizeof(r1->msg.LineStatMessage.lineDisplayName));
	sccp_dev_send(d, r1);
#endif
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6

/*!
 * \brief Show Device
 * \param e Asterisk CLI Entry
 * \param cmd Command as int
 * \param a Args as int
 * \return Result as int
 */
static char *cli_show_device(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
			e->command = "sccp show device";
			e->usage =
				"Usage:  sccp show device <deviceId>\n";
			return NULL;
		} else if (cmd == CLI_GENERATE){
			return sccp_complete_device(a->line, a->word, a->pos, a->n);
		}
		if (a->argc < 4)
			return CLI_SHOWUSAGE;

		if(sccp_show_device(a->fd, a->argc, a->argv) == RESULT_SUCCESS){
			return CLI_SUCCESS;
		}else
			return CLI_FAILURE;

}

#else
/*!
 * \brief Show Device
 * \return Result as Cli Entry Struct
 */
static struct ast_cli_entry cli_show_device = {
  { "sccp", "show", "device", NULL },
  sccp_show_device,
  "Show SCCP Device Information",
  "Usage: sccp show device <deviceId>\n",
  sccp_complete_device
};
#endif
/* ------------------------------------------------------------ */



#ifdef ASTERISK_CONF_1_6
/*!
 * \brief CLI Reset
 * \param e Asterisk Cli Entry
 * \param cmd Command as int
 * \param a Asterisk Cli Arguments
 * \note Result as Char
 */
static char *cli_reset(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
			e->command = "sccp reset";
			e->usage =
				"Usage: sccp reset <deviceId>\n";
			return NULL;
		} else if (cmd == CLI_GENERATE){
			return sccp_complete_device(a->line, a->word, a->pos, a->n);
		}
		if (a->argc != 3)
			return CLI_SHOWUSAGE;

		if(sccp_reset_restart(a->fd, a->argc, a->argv) == RESULT_SUCCESS){
			return CLI_SUCCESS;
		}else
			return CLI_FAILURE;

}

#else
/*!
 * \brief CLI Reset
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_reset = {
  { "sccp", "reset", NULL },
  sccp_reset_restart,
  "Reset an SCCP device",
  "Usage: sccp reset <deviceId>\n",
  sccp_complete_device
};
#endif


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Restart
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_restart(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
			e->command = "sccp restart";
			e->usage =
				"Usage: sccp restart <deviceId>\n";
			return NULL;
		} else if (cmd == CLI_GENERATE){
			return sccp_complete_device(a->line, a->word, a->pos, a->n);
		}
		if (a->argc != 3)
			return CLI_SHOWUSAGE;

		if(sccp_reset_restart(a->fd, a->argc, a->argv) == RESULT_SUCCESS){
			return CLI_SUCCESS;
		}else
			return CLI_FAILURE;

}

#else
/*!
 * \brief CLI Restart
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_restart = {
	{ "sccp", "restart", NULL },
	sccp_reset_restart,
	"Reset an SCCP device",
	"Usage: sccp restart <deviceId>\n",
	sccp_complete_device
};
#endif



#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Unregister
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Aterisk CLI Argements
 * \return Result as char
 */
static char *cli_unregister(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
			e->command = "sccp unregister";
			e->usage =
				"Usage: sccp unregister <deviceId>\n"
				"       Unregister an SCCP device\n";
			return NULL;
		} else if (cmd == CLI_GENERATE){
			return sccp_complete_device(a->line, a->word, a->pos, a->n);
		}
		if (a->argc != 3)
			return CLI_SHOWUSAGE;

		if(sccp_unregister(a->fd, a->argc, a->argv) == RESULT_SUCCESS){
			return CLI_SUCCESS;
		}else
			return CLI_FAILURE;

}

#else
/*!
 * \brief CLI Unregister
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_unregister = {
	{ "sccp", "unregister", NULL },
	sccp_unregister,
	"Unregister an SCCP device",
	"Usage: sccp unregister <deviceId>\n",
	sccp_complete_device
};
#endif


/* ------------------------------------------------------------ */
/*!
 * \brief Show Channels
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_channels(int fd, int argc, char * argv[]) {
	sccp_channel_t * c;
	sccp_line_t * l;

	ast_cli(fd, "\n%-5s %-10s %-16s %-16s %-16s %-10s %-10s\n", "ID","LINE","DEVICE","AST STATE","SCCP STATE","CALLED", "CODEC");
	ast_cli(fd, "===== ========== ================ ================ ================ ========== ==========\n");

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		sccp_line_lock(l);
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			ast_cli(fd, "%.5d %-10s %-16s %-16s %-16s %-10s %-10s\n",
				c->callid,
				c->line->name,
				(c->device)?c->device->description:"(unknown)",
				(c->owner) ? ast_state2str(c->owner->_state) : "(none)",
				sccp_indicate2str(c->state),
				c->calledPartyNumber,
				(c->format)?codec2str(sccp_codec_ast2skinny(c->format)):"(none)");
		}
		SCCP_LIST_UNLOCK(&l->channels);
		sccp_line_unlock(l);
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Show Channels
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_channels(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show channels";
		e->usage =
			"Usage: sccp show channels\n"
			"       Show all SCCP channels.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if(sccp_show_channels(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}
#endif

#ifndef ASTERISK_CONF_1_6
/*!
 * \brief CLI Show Channels
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_channels = {
	{ "sccp", "show", "channels", NULL },
	sccp_show_channels,
	"Show all SCCP channels",
	"Usage: sccp show channel\n",
};
#endif

/* ------------------------------------------------------------ */
/*!
 * \brief Show Devices
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_devices(int fd, int argc, char * argv[]) {
	sccp_device_t * d;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	ast_cli(fd, "\n%-40s %-20s %-16s %-10s\n", "NAME","ADDRESS","MAC","Reg. State");
	ast_cli(fd, "======================================== ==================== ================ ==========\n");

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
#ifdef ASTERISK_CONF_1_2
		ast_cli(fd, "%-40s %-20s %-16s %-10s\n",// %-10s %-16s %c%c %-10s\n",
			d->description,
			(d->session) ? ast_inet_ntoa(iabuf, sizeof(iabuf), d->session->sin.sin_addr) : "--",
			d->id,
			deviceregistrationstatus2str(d->registrationState)
		);
#else
		ast_cli(fd, "%-40s %-20s %-16s %-10s\n",// %-10s %-16s %c%c %-10s\n",
			d->description,
			(d->session) ? ast_inet_ntoa(d->session->sin.sin_addr) : "--",
			d->id,
			deviceregistrationstatus2str(d->registrationState)
		);
#endif
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));
	return RESULT_SUCCESS;
}




#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Show Devices
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_devices(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show devices";
		e->usage =
			"Usage: sccp show devices\n"
			"       Show all SCCP Devices.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if(sccp_show_devices(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Show Devices
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_devices = {
	{ "sccp", "show", "devices", NULL },
	sccp_show_devices,
	"Show all SCCP Devices",
	"Usage: sccp show devices\n"
};
#endif


/*!
 * \brief Message Devices
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_message_devices(int fd, int argc, char * argv[]) {
	sccp_device_t * d;
	int msgtimeout=10;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	if (ast_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	if (argc == 5 && sscanf(argv[4], "%d", &msgtimeout) != 1) {
		msgtimeout=10;
	}

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_displaynotify(d,argv[3],msgtimeout);
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Message Devices
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_message_devices(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp message devices";
		e->usage =
			"Usage: sccp messages devices <message text> [timeout]\n"
			"       Send a message to all SCCP Devices.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc < 4)
		return CLI_SHOWUSAGE;

	if(sccp_message_devices(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Mesage Devices
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_message_devices = {
  { "sccp", "message", "devices", NULL },
  sccp_message_devices,
  "Send a message to all SCCP Devices",
  "Usage: sccp messages devices <message text> <timeout>\n"
};
#endif


/* ------------------------------------------------------------ */
/*!
 * \brief Show Lines
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_lines(int fd, int argc, char * argv[]) {
	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	sccp_device_t * d = NULL;
	char cap_buf[512];
	struct ast_variable *v = NULL;

	ast_cli(fd, "\n%-16s %-16s %-5s %-4s %-4s %-16s\n", "NAME","DEVICE", "SUFFIX", "MWI","Chs","Active Channel");
	ast_cli(fd, "================ ================ ==== ==== =================================================\n");

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines),l,list) {

		c = NULL;
		//TODO handle shared line
		d = NULL;

		if (d) {
			sccp_device_lock(d);
			c = d->active_channel;
			sccp_device_unlock(d);
		}

		if (!c || (c->line != l))
			c = NULL;

		memset(&cap_buf, 0, sizeof(cap_buf));


		if (c && c->owner) {
			ast_getformatname_multiple(cap_buf, sizeof(cap_buf),  c->owner->nativeformats);
		}

		ast_cli(fd, "%-16s %-16s %-5s %-4s %-4d %-10s %-10s %-16s %-10s\n",
			l->name,
			"--",
			"",
			(l->voicemailStatistic.newmsgs) ? "ON" : "OFF",
			l->channelCount,
			(c) ? sccp_indicate2str(c->state) : "--",
			(c) ? calltype2str(c->calltype) : "",
			(c) ? ( (c->calltype == SKINNY_CALLTYPE_OUTBOUND) ? c->calledPartyName : c->callingPartyName ) : "",
			cap_buf);

		sccp_linedevices_t *linedevice;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
			if(!linedevice->device)
				continue;
			d=linedevice->device;
			ast_cli(fd, "%-16s %-16s %-5s %-4s %-4d %-10s %-10s %-16s %-10s\n",
				"",
				(d) ? d->id : "--",
				linedevice->subscriptionId.number,
				(l->voicemailStatistic.newmsgs) ? "ON" : "OFF",
				l->channelCount,
				(c) ? sccp_indicate2str(c->state) : "--",
				(c) ? calltype2str(c->calltype) : "",
				(c) ? ( (c->calltype == SKINNY_CALLTYPE_OUTBOUND) ? c->calledPartyName : c->callingPartyName ) : "",
				cap_buf);
		}
		SCCP_LIST_UNLOCK(&l->devices);
		for (v = l->variables ; v ; v = v->next)
			ast_cli(fd, "Variable: %-20s : %-20s\n", v->name , v->value);

		if(strcmp(l->defaultSubscriptionId.number, "") || strcmp(l->defaultSubscriptionId.name, ""))
			ast_cli(fd, "Default Subscription Id: Number=%s, Name=%s\n", l->defaultSubscriptionId.number,  l->defaultSubscriptionId.name);


	}
	SCCP_LIST_UNLOCK(&GLOB(lines));

	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Show Lines
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_lines(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show lines";
		e->usage =
			"Usage: sccp show lines\n"
			"       Show All SCCP Lines.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;


	if(sccp_show_lines(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Show Lines
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_lines = {
  { "sccp", "show", "lines", NULL },
  sccp_show_lines,
  "Show All SCCP Lines",
  "Usage: sccp show lines\n"
};
#endif



/* ------------------------------------------------------------ */
/*!
 * \brief Remove Line From Device
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_remove_line_from_device(int fd, int argc, char * argv[])
{
	return RESULT_FAILURE;
/*
	sccp_line_t * l = NULL;
	sccp_hint_t * h = NULL;
	sccp_device_t * d = NULL;



	d = sccp_device_find_byid(argv[3]);
	if(!d)
		ast_cli(fd, "Device %s not found\n",argv[3]);

	l = d->lines;
	while(l){
		sccp_line_lock(l);
		if(!strcasecmp(l->name, argv[4])){
			sccp_line_delete_nolock(l);
			while (l->hints) {
				h = l->hints;
				l->hints = l->hints->next;
				free(h);
			}
			if (l->cfwd_num)
				free(l->cfwd_num);
			if (l->trnsfvm)
				free(l->trnsfvm);
			free(l);
//			sccp_dev_set_registered(d, SKINNY_DEVICE_RS_PROGRESS);
//			sccp_dev_set_registered(d, SKINNY_DEVICE_RS_OK);
			break;
		}

		sccp_line_unlock(l);
		l = l->next_on_device;
	}

  return RESULT_SUCCESS;
*/
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Remove Line Device
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_remove_line_device(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp remove line";
		e->usage =
			"Usage: sccp remove line <deviceID> <lineID>\n"
			"       Remove a line from device.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc != 5)
		return CLI_SHOWUSAGE;

	if(sccp_remove_line_from_device(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Remove Line from Device
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_remove_line_device = {
  { "sccp", "remove", "line", NULL },
  sccp_remove_line_from_device,
  "Remove a line from device",
  "Usage: sccp remove line <deviceID> <lineID>\n"
};
#endif

/* ------------------------------------------------------------ */
/*!
 * \brief Show Sessions
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_sessions(int fd, int argc, char * argv[]) {
	sccp_session_t * s = NULL;
	sccp_device_t * d = NULL;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	ast_cli(fd, "%-10s %-15s %-4s %-15s %-15s %-15s\n", "Socket", "IP", "KA", "DEVICE", "STATE", "TYPE");
	ast_cli(fd, "========== =============== ==== =============== =============== ===============\n");

	SCCP_LIST_LOCK(&GLOB(sessions));
	SCCP_LIST_TRAVERSE(&GLOB(sessions), s, list) {
		sccp_session_lock(s);
		d = s->device;
		if (d) {
			sccp_device_lock(d);
#ifdef ASTERISK_CONF_1_2
			ast_cli(fd, "%-10d %-15s %-4d %-15s %-15s %-15s\n",
				s->fd,
				ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr),
				(uint32_t)(time(0) - s->lastKeepAlive),
				(d) ? d->id : "--",
				(d) ? devicestatus2str(d->state) : "--",
				(d) ? devicetype2str(d->skinny_type) : "--");
#else
			ast_cli(fd, "%-10d %-15s %-4d %-15s %-15s %-15s\n",
				s->fd,
				ast_inet_ntoa(s->sin.sin_addr),
				(uint32_t)(time(0) - s->lastKeepAlive),
				(d) ? d->id : "--",
				(d) ? devicestatus2str(d->state) : "--",
				(d) ? devicetype2str(d->skinny_type) : "--");
#endif
			sccp_device_unlock(d);
		}
		sccp_session_unlock(s);
	}
	SCCP_LIST_UNLOCK(&GLOB(sessions));
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Show Sessions
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_sessions(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show sessions";
		e->usage =
			"Usage: sccp show sessions\n"
			"       Show All SCCP Sessions.\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if(sccp_show_sessions(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Show Sessions
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_sessions = {
  { "sccp", "show", "sessions", NULL },
  sccp_show_sessions,
  "Show All SCCP Sessions",
  "Usage: sccp show sessions\n"
};
#endif

/* ------------------------------------------------------------ */
/*!
 * \brief System Message
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_system_message(int fd, int argc, char * argv[]) {
	int res;
	int timeout = 0;
	if ((argc < 3) || (argc > 5))
		return RESULT_SHOWUSAGE;

	if (argc == 3) {
		res = ast_db_deltree("SCCP", "message");
		if (res) {
			ast_cli(fd, "Failed to delete the SCCP system message!\n");
			return RESULT_FAILURE;
		}
		ast_cli(fd, "SCCP system message deleted!\n");
		return RESULT_SUCCESS;
	}

	if (ast_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	res = ast_db_put("SCCP/message", "text", argv[3]);
	if (res) {
		ast_cli(fd, "Failed to store the SCCP system message text\n");
	} else {
		ast_cli(fd, "SCCP system message text stored successfully\n");
	}
	if (argc == 5) {
		if (sscanf(argv[4], "%d", &timeout) != 1)
			return RESULT_SHOWUSAGE;
		res = ast_db_put("SCCP/message", "timeout", argv[4]);
		if (res) {
			ast_cli(fd, "Failed to store the SCCP system message timeout\n");
		} else {
			ast_cli(fd, "SCCP system message timeout stored successfully\n");
		}
	} else {
		ast_db_del("SCCP/message", "timeout");
	}
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli System Message
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_system_message(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp system message";
		e->usage =
			"Usage: sccp system message \"<message text>\" <timeout>\n"
			"		The default optional timeout is 0 (forever)\n"
			"		Example: sccp system message \"The boss is gone. Let's have some fun!\"  10\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc < 4)
		return CLI_SHOWUSAGE;

	if(sccp_system_message(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Systsem Message
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_system_message = {
   { "sccp", "system", "message", NULL },
   sccp_system_message,
   "Set the SCCP system message",
   "Usage: sccp system message \"<message text>\" <timeout>\n"
   "		The default optional timeout is 0 (forever)\n"
   "		Example: sccp system message \"The boss is gone. Let's have some fun!\"  10\n"
};
#endif

#ifdef ASTERISK_CONF_1_6
#ifdef CS_NEW_AST_CLI
/*!
 * \brief Complete Debug
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 */
static char * sccp_complete_debug(const char *line, const char *word, int pos, int state)
{
#else
/*!
 * \brief Complete Debug
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 */
static char * sccp_complete_debug(char *line, char *word, int pos, int state) {
#endif
	int i;
	int which = 0;
	char * ret=NULL;
	boolean_t debugno=0;
	char * extra_cmds[3]={"no","none","all"};

	if (pos < 2)
  	  return NULL;

	// check if the sccp debug line contains no before the categories
	if(!strncasecmp(line,"sccp debug no ",strlen("sccp debug no "))) {
		debugno=1;
	}

	// check extra_cmd
	for (i=0; i<ARRAY_LEN(extra_cmds); i++) {
		if (!strncasecmp(word, extra_cmds[i], strlen(word))) {
			// skip "no" and "none" if in debugno mode
			if (debugno && !strncasecmp("no",extra_cmds[i],strlen("no")))
				continue;
			if (++which > state)
				return strdup(extra_cmds[i]);
		}
	}
	// check categories
	for (i=0; i<ARRAY_LEN(sccp_debug_categories); i++) {
		// if in debugno mode
		if (debugno) {
			// then skip the categories which are not currently active
			if ((GLOB(debug) & sccp_debug_categories[i].category) != sccp_debug_categories[i].category)
				continue;
		} else {
			// not debugno then skip the categories which are already active
			if ((GLOB(debug) & sccp_debug_categories[i].category) == sccp_debug_categories[i].category)
				continue;
		}
		// find a match with partial category
		if (!strncasecmp(word, sccp_debug_categories[i].short_name, strlen(word))) {
			if (++which > state)
				return strdup(sccp_debug_categories[i].short_name);
		}
	}
	return ret;
}
#endif
/* ------------------------------------------------------------ */
/*!
 * \brief Do Debug
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_do_debug(int fd, int argc, char *argv[]) {
        uint32_t new_debug = GLOB(debug);

	if ((argc < 3))
		return RESULT_SHOWUSAGE;

	if (argc > 2) {
		new_debug=sccp_parse_debugline (argv,2,argc,new_debug);
	}
	char * debugcategories="";
	ast_cli(fd, "SCCP new debug status: (%d -> %d) %s\n", GLOB(debug), new_debug, sccp_get_debugcategories(new_debug,debugcategories));
	GLOB(debug) = new_debug;
	return RESULT_SUCCESS;
}

#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Do Debug
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_do_debug(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp debug";
		e->usage =
			"Usage: SCCP debug [no] <level or categories>\n"
			"		Where categories is one or more (separated by comma's) of:\n"
			"		core, sccp, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey,\n"
			"		indicate, pbx, socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime,\n"
			"		lock, newcode, high\n";
		return NULL;
	} else if (cmd == CLI_GENERATE) {
		return sccp_complete_debug(a->line, a->word, a->pos, a->n);
	}
	if ((a->argc < 3))
		return CLI_SHOWUSAGE;

	if(sccp_do_debug(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
	return CLI_FAILURE;
}
#else
/*!
 * \brief CLI Enable Debug
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_do_debug = {
  { "sccp", "debug", NULL },
  sccp_do_debug,
  "Enable SCCP debugging",
  "Usage: SCCP debug [no] <level or categories>\n"
  "		Where categories is one or more (separated by comma's) of:\n"
  "		core, sccp, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey,\n"
  "		indicate, pbx, socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime,\n"
  "		lock, newcode, high\n"
};
#endif

/*!
 * \brief No Debug
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_no_debug(int fd, int argc, char *argv[]) {
	if (argc < 3)
		return RESULT_SHOWUSAGE;

	GLOB(debug) = 0;
	ast_cli(fd, "SCCP Debugging Disabled\n");
	return RESULT_SUCCESS;
}



#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli No Debug
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_no_debug(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp no debug";
		e->usage =
			"Usage: SCCP no debug\n"
			"		Disables dumping of SCCP packets for debugging purposes\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc < 3)
		return CLI_SHOWUSAGE;

	if(sccp_no_debug(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI No Debug
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_no_debug = {
  { "sccp", "no", "debug", NULL },
  sccp_no_debug,
  "Disable SCCP debugging",
  "Usage: SCCP no debug\n"
  "		Disables dumping of SCCP packets for debugging purposes\n"
};
#endif

/*!
 * \brief Do Reload
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_do_reload(int fd, int argc, char *argv[]) {
#ifdef CS_DYNAMIC_CONFIG
	ast_cli(fd, "SCCP reloading configuration.\n");
	sccp_readingtype_t readingtype;

//(sccp_cli.c)        set global variable reloading=true
//                    > readingtype is not a global variable -romain
	readingtype=SCCP_CONFIG_READRELOAD;

//(sccp_config.c)        set all devices + lines to pendingDelete
	/* done by readDevicesLines */
	
// (sccp_config.c)     load global parameters
	if (!sccp_config_general()) {
		ast_cli(fd, "Unable to reload configuration.\n");
		return RESULT_FAILURE;
	}

	// (sccp_config.c)     load device parameters 
	sccp_config_readDevicesLines(readingtype);

/*
(sccp_config.c)       - if device already exists remove device.pendingDelete
(sccp_config.c)       - set device.pendingUpdate where device restart necessary
(sccp_config.c)     load line parameters set device.pendingUpdate where necessary {
(sccp_config.c)       - if line already exists remove line.pendingDelete
(sccp_config.c)       - set device.pendingUpdate where device restart necessary
                    }
(sccp_cli.c)        restart devices with pendingUpdate/pendingDelete {
(sccp_cli.c)          - skip device if it holds an active channel (device will restart on hangup 
(sccp_channel.c)            (hangup function needs to check pendingUpdate/pendingDelete and react accordingly)
(sccp_cli.c)          - if pendingDelete the remove buttonconfig, line, device
(sccp_cli.c)          - if pendingUpdate then set pendingUpdate to false and send restart to device
                    }
(sccp_cli.c)        print reload statistics (number of device restarted, number of devices with open channels)
*/

        return RESULT_SUCCESS;
#else
	ast_cli(fd, "SCCP configuration reload not implemented yet! use unload and load.\n");
	return RESULT_SUCCESS;
#endif
}

#ifdef ASTERISK_CONF_1_6
/*!
 * \brief Cli Reload
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_reload(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp reload";
		e->usage =
		"Usage: sccp reload\n"
		"	  Reloads SCCP configuration from sccp.conf (It will close all active connections)\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc != 2)
		return CLI_SHOWUSAGE;

	if(sccp_do_reload(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Reload
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_reload = {
  { "sccp", "reload", NULL },
  sccp_do_reload,
  "SCCP module reload",
  "Usage: sccp reload\n"
  "		Reloads SCCP configuration from sccp.conf (It will close all active connections)\n"
};
#endif

/*!
 * \brief Show Version
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 */
static int sccp_show_version(int fd, int argc, char *argv[]) {
	ast_cli(fd, "Skinny Client Control Protocol (SCCP). Release: %s %s - %s (built by '%s' on '%s')\n", SCCP_VERSION, SCCP_BRANCH, SCCP_REVISION, BUILD_USER, BUILD_DATE);
	return RESULT_SUCCESS;
}


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief CLI Show Version
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_version(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show version";
		e->usage =
			"Usage: SCCP show version\n"
			"		Show the SCCP channel version\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	if(sccp_show_version(a->fd, a->argc, a->argv) == RESULT_SUCCESS)
		return CLI_SUCCESS;
	else
		return CLI_FAILURE;
}

#else
/*!
 * \brief CLI Show Version
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_version = {
  { "sccp", "show", "version", NULL },
  sccp_show_version,
  "SCCP show version",
  "Usage: SCCP show version\n"
  "		Show the SCCP channel version\n"
};
#endif


#ifdef ASTERISK_CONF_1_6
/*!
 * \brief CLI MWI Subscriptions
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_mwi_subscriptions(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show subscriptions";
		e->usage =
			"Usage: sccp show subscriptions\n"
			"		Show the SCCP channel subscriptions\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	/*
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_mailboxLine_t	*mailboxLine = NULL;
	*/

	//TODO display mailbox subscriptions
	/*
	ast_cli(a->fd, "subscriptionsize: %d\n", sccp_mailbox_subscriptions.size);
	SCCP_LIST_TRAVERSE(&sccp_mailbox_subscriptions, subscription, list){
		ast_cli(a->fd, "mailbox: %s@%s\n", subscription->mailbox, subscription->context);

		SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list){
			ast_cli(a->fd, "---line: %s\n", (mailboxLine->line)->name );
			ast_cli(a->fd, "-----status\n");
			ast_cli(a->fd, "--------new: %d\n",(mailboxLine->line)->voicemailStatistic.newmsgs);
			ast_cli(a->fd, "--------old: %d\n",(mailboxLine->line)->voicemailStatistic.oldmsgs);

			sccp_linedevices_t *lineDevice = NULL;
			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&(mailboxLine->line)->devices, lineDevice, list){
				ast_cli(a->fd, "-----------device: %s light: %d\n", DEV_ID_LOG(lineDevice->device), lineDevice->device->mwilight);
			}
			SCCP_LIST_TRAVERSE_SAFE_END;
			ast_cli(a->fd, "\n");
		}
	}
	*/
	ast_cli(a->fd, "\n");
	return CLI_SUCCESS;
}
#else
/*!
 * \brief CLI MWI Subscriptions
 * \return Asterisk Cli Entry Structure
 * \note Alias for Asterisk CLI Entry
 */
static struct ast_cli_entry cli_show_mwi_subscriptions = {
  { "sccp", "show", "subscriptions", NULL },
  sccp_show_version,
  "SCCP show subscriptions",
  "Usage: SCCP show subscriptions\n"
  "		Show the SCCP channel subscriptions\n"
};
#endif



#ifdef ASTERISK_CONF_1_6
/*!
 * \brief CLI MWI Subscriptions
 * \param e Asterisk CLI Entry
 * \param cmd Cmd as int
 * \param a Asterisk CLI Arguments
 * \return Result as char
 */
static char *cli_show_softkeysets(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){
	if (cmd == CLI_INIT) {
		e->command = "sccp show softkeysets";
		e->usage =
			"Usage: sccp show softkeysets\n"
			"		Show the configured SoftKeySets\n";
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;

	if (a->argc != 3)
		return CLI_SHOWUSAGE;

	sccp_softKeySetConfiguration_t *softkeyset = NULL;

	ast_cli(a->fd, "SoftKeySets: %d\n", softKeySetConfig.size);
	uint8_t i = 0;
	uint8_t v_count = 0;
	uint8_t c=0;
	SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list){
		v_count = sizeof(softkeyset->modes)/sizeof(softkey_modes);

		ast_cli(a->fd, "name: %s\n", softkeyset->name);
		ast_cli(a->fd, "number of softkeysets: %d\n", v_count );



		for (i = 0; i < v_count; i++) {
			const uint8_t *b = softkeyset->modes[i].ptr;
			ast_cli(a->fd, "      Set[%-2d]= ", i);

			for ( c = 0; c < softkeyset->modes[i].count; c++) {
				ast_cli(a->fd, "%-2d:%-10s ", c, label2str(b[c]));
			}

			ast_cli(a->fd, "\n");
		}

		ast_cli(a->fd, "\n");

	}
	ast_cli(a->fd, "\n");
	return CLI_SUCCESS;
}
#endif


#ifdef ASTERISK_CONF_1_6

/*!
 * \brief Asterisk Cli Entry
 *
 * structure for cli functions including short description.
 *
 * \return Result as struct
 * \todo add short description
 */

static struct ast_cli_entry cli_entries[] = {
		AST_CLI_DEFINE(cli_show_globals, "Show SCCP global settings."),
		AST_CLI_DEFINE(cli_show_channels, "Show all SCCP channels."),
		AST_CLI_DEFINE(cli_unregister, "Unregister an SCCP device"),
		AST_CLI_DEFINE(cli_show_devices, "Show all SCCP Devices."),
		AST_CLI_DEFINE(cli_message_devices, "Send a message to all SCCP Devices."),
		AST_CLI_DEFINE(cli_show_lines, "Show All SCCP Lines."),
		AST_CLI_DEFINE(cli_remove_line_device, "Remove a line from device."),
		AST_CLI_DEFINE(cli_show_sessions, "Show All SCCP Sessions."),
		AST_CLI_DEFINE(cli_system_message, "Set the SCCP system message."),
		AST_CLI_DEFINE(cli_do_debug, "Enable SCCP debugging."),
		AST_CLI_DEFINE(cli_no_debug, "Disable SCCP debugging."),
		AST_CLI_DEFINE(cli_reload, "SCCP module reload."),
		AST_CLI_DEFINE(cli_show_version, "SCCP show version."),
		AST_CLI_DEFINE(cli_show_mwi_subscriptions, "Show all mwi subscriptions"),
		AST_CLI_DEFINE(cli_show_softkeysets, "Show all mwi configured SoftKeySets"),
		AST_CLI_DEFINE(cli_restart, ""),
		AST_CLI_DEFINE(cli_reset, ""),
		AST_CLI_DEFINE(cli_show_device, "")
};
#endif




/*!
 * register CLI functions from asterisk
 */
void sccp_register_cli(void) {

#ifdef ASTERISK_CONF_1_6
	/* register all CLI functions */
	ast_cli_register_multiple(cli_entries, sizeof(cli_entries)/ sizeof(struct ast_cli_entry));
#else
  ast_cli_register(&cli_show_channels);
  ast_cli_register(&cli_show_devices);
  ast_cli_register(&cli_show_lines);
  ast_cli_register(&cli_show_sessions);
  ast_cli_register(&cli_show_device);
  ast_cli_register(&cli_show_version);
  ast_cli_register(&cli_reload);
  ast_cli_register(&cli_restart);
  ast_cli_register(&cli_reset);
  ast_cli_register(&cli_unregister);
  ast_cli_register(&cli_do_debug);
  ast_cli_register(&cli_no_debug);
  ast_cli_register(&cli_system_message);
  ast_cli_register(&cli_show_globals);
  ast_cli_register(&cli_message_devices);
  ast_cli_register(&cli_remove_line_device);
  ast_cli_register(&cli_show_mwi_subscriptions);
#endif

}


/*!
 * unregister CLI functions from asterisk
 */
void sccp_unregister_cli(void) {

#ifdef ASTERISK_CONF_1_6
	/* unregister CLI functions */
	ast_cli_unregister_multiple(cli_entries, sizeof(cli_entries) / sizeof(struct ast_cli_entry));
#else
  ast_cli_unregister(&cli_show_channels);
  ast_cli_unregister(&cli_show_devices);
  ast_cli_unregister(&cli_show_lines);
  ast_cli_unregister(&cli_show_sessions);
  ast_cli_unregister(&cli_show_device);
  ast_cli_unregister(&cli_show_version);
  ast_cli_unregister(&cli_reload);
  ast_cli_unregister(&cli_restart);
  ast_cli_unregister(&cli_reset);
  ast_cli_unregister(&cli_unregister);
  ast_cli_unregister(&cli_do_debug);
  ast_cli_unregister(&cli_no_debug);
  ast_cli_unregister(&cli_system_message);
  ast_cli_unregister(&cli_show_globals);
  ast_cli_unregister(&cli_message_devices);
  ast_cli_unregister(&cli_remove_line_device);
  ast_cli_unregister(&cli_show_mwi_subscriptions);
#endif
}


