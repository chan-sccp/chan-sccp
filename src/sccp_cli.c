
/*!
 * \file 	sccp_cli.c
 * \brief 	SCCP CLI Class
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

/*!
 * \remarks	Purpose: 	SCCP CLI
 * 		When to use:	Only methods directly related to the asterisk cli interface should be stored in this source file.
 *   		Relationships: 	Calls ???
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#include <asterisk/cli.h>
#ifdef CS_IPV6
#    define CLI_AMI_LIST_WIDTH 46
#else
#    define CLI_AMI_LIST_WIDTH 21
#endif

/* --- CLI Tab Completion ---------------------------------------------------------------------------------------------- */

/*!
 * \brief Complete Device
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 *
 * \lock
 * 	- devices
 */
static char *sccp_complete_device(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_device_t *d;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strncasecmp(word, d->id, wordlen) && ++which > state) {
			ret = strdup(d->id);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	return ret;
}

/*!
 * \brief Complete Line
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- lines
 */
static char *sccp_complete_line(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strncasecmp(word, l->name, wordlen) && ++which > state) {
			ret = strdup(l->name);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return ret;
}

/*!
 * \brief Complete Channel
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- lines
 */
static char *sccp_complete_channel(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l;
	sccp_channel_t *c;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;
	char tmpname[20];

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			snprintf(tmpname, sizeof(tmpname), "SCCP/%s-%08x", l->name, c->callid);
			if (!strncasecmp(word, tmpname, wordlen) && ++which > state) {
				ret = strdup(tmpname);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return ret;
}

/*!
 * \brief Complete Debug
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 */
static char *sccp_complete_debug(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	uint8_t i;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;
	boolean_t debugno = 0;
	char *extra_cmds[3] = { "no", "none", "all" };

	// check if the sccp debug line contains no before the categories
	if (!strncasecmp(line, "sccp debug no ", strlen("sccp debug no "))) {
		debugno = 1;
	}
	// check extra_cmd
	for (i = 0; i < ARRAY_LEN(extra_cmds); i++) {
		if (!strncasecmp(word, extra_cmds[i], wordlen)) {
			// skip "no" and "none" if in debugno mode
			if (debugno && !strncasecmp("no", extra_cmds[i], strlen("no")))
				continue;
			if (++which > state)
				return strdup(extra_cmds[i]);
		}
	}
	// check categories
	for (i = 0; i < ARRAY_LEN(sccp_debug_categories); i++) {
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
		if (!strncasecmp(word, sccp_debug_categories[i].short_name, wordlen)) {
			if (++which > state)
				return strdup(sccp_debug_categories[i].short_name);
		}
	}
	return ret;
}

static char *sccp_exec_completer(sccp_cli_completer_t completer, OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	switch (completer) {
	case SCCP_CLI_NULL_COMPLETER:
		return NULL;
		break;
	case SCCP_CLI_DEVICE_COMPLETER:
		return sccp_complete_device(line, word, pos, state);
		break;
	case SCCP_CLI_LINE_COMPLETER:
		return sccp_complete_line(line, word, pos, state);
		break;
	case SCCP_CLI_CHANNEL_COMPLETER:
		return sccp_complete_channel(line, word, pos, state);
		break;
	case SCCP_CLI_DEBUG_COMPLETER:
		return sccp_complete_debug(line, word, pos, state);
		break;
	}
	return NULL;
}

/* --- Support Functions ---------------------------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------------------------------------SHOW GLOBALS- */

/*!
 * \brief Show Globals
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- globals
 */
//static int sccp_show_globals(int fd, int argc, char *argv[])
static int sccp_show_globals(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	char pref_buf[256];
	struct ast_str *callgroup_buf = pbx_str_alloca(512);
	struct ast_str *pickupgroup_buf = pbx_str_alloca(512);
	char *debugcategories;
	int local_total = 0;

	sccp_globals_lock(lock);

	sccp_multiple_codecs2str(pref_buf, sizeof(pref_buf) - 1, GLOB(global_preferences), ARRAY_LEN(GLOB(global_preferences)));
	debugcategories = sccp_get_debugcategories(GLOB(debug));

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver global settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_send_listack(s, m, argv[0], "start");
		CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", argv[0]);
	}
	CLI_AMI_OUTPUT_PARAM("Config File", CLI_AMI_LIST_WIDTH, "%s", GLOB(config_file_name));
#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	CLI_AMI_OUTPUT_PARAM("Platform byte order", CLI_AMI_LIST_WIDTH, "%s", "LITTLE ENDIAN");
#else
	CLI_AMI_OUTPUT_PARAM("Platform byte order", CLI_AMI_LIST_WIDTH, "%s", "BIG ENDIAN");
#endif
//      CLI_AMI_OUTPUT_PARAM("Protocol Version",        CLI_AMI_LIST_WIDTH,     "%d",           GLOB(protocolversion));
	CLI_AMI_OUTPUT_PARAM("Server Name", CLI_AMI_LIST_WIDTH, "%s", GLOB(servername));
	CLI_AMI_OUTPUT_PARAM("Bind Address", CLI_AMI_LIST_WIDTH, "%s:%d", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
	CLI_AMI_OUTPUT_BOOL("Nat", CLI_AMI_LIST_WIDTH, GLOB(nat));
	CLI_AMI_OUTPUT_PARAM("Extern Hostname", CLI_AMI_LIST_WIDTH, "%s", GLOB(externhost));
	CLI_AMI_OUTPUT_PARAM("Extern Host Refresh", CLI_AMI_LIST_WIDTH, "%d", GLOB(externrefresh));
	CLI_AMI_OUTPUT_PARAM("Extern IP", CLI_AMI_LIST_WIDTH, "%s:%d", pbx_inet_ntoa(GLOB(externip.sin_addr)), ntohs(GLOB(externip.sin_port)));
	CLI_AMI_OUTPUT_BOOL("Direct RTP", CLI_AMI_LIST_WIDTH, GLOB(directrtp));
	CLI_AMI_OUTPUT_PARAM("Keepalive", CLI_AMI_LIST_WIDTH, "%d", GLOB(keepalive));
	CLI_AMI_OUTPUT_PARAM("Debug", CLI_AMI_LIST_WIDTH, "(%d) %s", GLOB(debug), debugcategories);
	CLI_AMI_OUTPUT_PARAM("Date format", CLI_AMI_LIST_WIDTH, "%s", GLOB(dateformat));
	CLI_AMI_OUTPUT_PARAM("First digit timeout", CLI_AMI_LIST_WIDTH, "%d", GLOB(firstdigittimeout));
	CLI_AMI_OUTPUT_PARAM("Digit timeout", CLI_AMI_LIST_WIDTH, "%d", GLOB(digittimeout));
	CLI_AMI_OUTPUT_PARAM("Digit timeout char", CLI_AMI_LIST_WIDTH, "%c", GLOB(digittimeoutchar));
	CLI_AMI_OUTPUT_PARAM("SCCP tos (signaling)", CLI_AMI_LIST_WIDTH, "%d", GLOB(sccp_tos));
	CLI_AMI_OUTPUT_PARAM("SCCP cos (signaling)", CLI_AMI_LIST_WIDTH, "%d", GLOB(sccp_cos));
	CLI_AMI_OUTPUT_PARAM("AUDIO tos (rtp)", CLI_AMI_LIST_WIDTH, "%d", GLOB(audio_tos));
	CLI_AMI_OUTPUT_PARAM("AUDIO cos (rtp)", CLI_AMI_LIST_WIDTH, "%d", GLOB(audio_cos));
	CLI_AMI_OUTPUT_PARAM("VIDEO tos (vrtp)", CLI_AMI_LIST_WIDTH, "%d", GLOB(video_tos));
	CLI_AMI_OUTPUT_PARAM("VIDEO cos (vrtp)", CLI_AMI_LIST_WIDTH, "%d", GLOB(video_cos));
	CLI_AMI_OUTPUT_PARAM("Context", CLI_AMI_LIST_WIDTH, "%s (%s)", GLOB(context), pbx_context_find(GLOB(context)) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_PARAM("Language", CLI_AMI_LIST_WIDTH, "%s", GLOB(language));
	CLI_AMI_OUTPUT_PARAM("Accountcode", CLI_AMI_LIST_WIDTH, "%s", GLOB(accountcode));
	CLI_AMI_OUTPUT_PARAM("Musicclass", CLI_AMI_LIST_WIDTH, "%s", GLOB(musicclass));
	CLI_AMI_OUTPUT_PARAM("AMA flags", CLI_AMI_LIST_WIDTH, "%d (%s)", GLOB(amaflags), pbx_cdr_flags2str(GLOB(amaflags)));
	sccp_print_group(callgroup_buf, sizeof(callgroup_buf), GLOB(callgroup));
	CLI_AMI_OUTPUT_PARAM("Callgroup", CLI_AMI_LIST_WIDTH, "%s", callgroup_buf ? pbx_str_buffer(callgroup_buf) : "");
#ifdef CS_SCCP_PICKUP
	sccp_print_group(pickupgroup_buf, sizeof(pickupgroup_buf), GLOB(pickupgroup));
	CLI_AMI_OUTPUT_PARAM("Pickupgroup", CLI_AMI_LIST_WIDTH, "%s", pickupgroup_buf ? pbx_str_buffer(pickupgroup_buf) : "");
	CLI_AMI_OUTPUT_BOOL("Pickup Mode Answer ", CLI_AMI_LIST_WIDTH, GLOB(pickupmodeanswer));
#endif
	CLI_AMI_OUTPUT_PARAM("Codecs preference", CLI_AMI_LIST_WIDTH, "%s", pref_buf);
	CLI_AMI_OUTPUT_BOOL("CFWDALL    ", CLI_AMI_LIST_WIDTH, GLOB(cfwdall));
	CLI_AMI_OUTPUT_BOOL("CFWBUSY    ", CLI_AMI_LIST_WIDTH, GLOB(cfwdbusy));
	CLI_AMI_OUTPUT_BOOL("CFWNOANSWER   ", CLI_AMI_LIST_WIDTH, GLOB(cfwdnoanswer));
#ifdef CS_MANAGER_EVENTS
	CLI_AMI_OUTPUT_BOOL("Call Events", CLI_AMI_LIST_WIDTH, GLOB(callevents));
#else
	CLI_AMI_OUTPUT_BOOL("Call Events", CLI_AMI_LIST_WIDTH, FALSE);
#endif
	CLI_AMI_OUTPUT_PARAM("DND", CLI_AMI_LIST_WIDTH, "%s", GLOB(dndmode) ? dndmode2str(GLOB(dndmode)) : "Disabled");
#ifdef CS_SCCP_PARK
	CLI_AMI_OUTPUT_BOOL("Park", CLI_AMI_LIST_WIDTH, FALSE);
#else
	CLI_AMI_OUTPUT_BOOL("Park", CLI_AMI_LIST_WIDTH, FALSE);
#endif
	CLI_AMI_OUTPUT_BOOL("Private softkey", CLI_AMI_LIST_WIDTH, GLOB(privacy));
	CLI_AMI_OUTPUT_BOOL("Echo cancel", CLI_AMI_LIST_WIDTH, GLOB(echocancel));
	CLI_AMI_OUTPUT_BOOL("Silence suppression", CLI_AMI_LIST_WIDTH, GLOB(silencesuppression));
	CLI_AMI_OUTPUT_BOOL("Trust phone ip", CLI_AMI_LIST_WIDTH, GLOB(trustphoneip));
	CLI_AMI_OUTPUT_PARAM("Early RTP", CLI_AMI_LIST_WIDTH, "%s (%s)", GLOB(earlyrtp) ? "Yes" : "No", GLOB(earlyrtp) ? sccp_indicate2str(GLOB(earlyrtp)) : "none");
	CLI_AMI_OUTPUT_PARAM("AutoAnswer ringtime", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_ring_time));
	CLI_AMI_OUTPUT_PARAM("AutoAnswer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_tone));
	CLI_AMI_OUTPUT_PARAM("RemoteHangup tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(remotehangup_tone));
	CLI_AMI_OUTPUT_PARAM("Transfer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(transfer_tone));
	CLI_AMI_OUTPUT_PARAM("CallWaiting tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(callwaiting_tone));
	CLI_AMI_OUTPUT_PARAM("Registration Context", CLI_AMI_LIST_WIDTH, "%s", GLOB(regcontext) ? GLOB(regcontext) : "Unset");
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer enabled ", CLI_AMI_LIST_WIDTH, pbx_test_flag(&GLOB(global_jbconf), AST_JB_ENABLED));
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer forced ", CLI_AMI_LIST_WIDTH, pbx_test_flag(&GLOB(global_jbconf), AST_JB_FORCED));
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer max size", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf).max_size);
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer resync", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf).resync_threshold);
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer impl", CLI_AMI_LIST_WIDTH, "%s", GLOB(global_jbconf).impl);
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer log  ", CLI_AMI_LIST_WIDTH, pbx_test_flag(&GLOB(global_jbconf), AST_JB_LOG));
#ifdef CS_AST_JB_TARGET_EXTRA
	CLI_AMI_OUTPUT_PARAM("Jitterbuf target extra", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf).target_extra);
#endif
	CLI_AMI_OUTPUT_PARAM("Token FallBack", CLI_AMI_LIST_WIDTH, "%s", GLOB(token_fallback));
	CLI_AMI_OUTPUT_PARAM("Token Backoff-Time", CLI_AMI_LIST_WIDTH, "%d", GLOB(token_backoff_time));
	CLI_AMI_OUTPUT_BOOL("Hotline_Enabled", CLI_AMI_LIST_WIDTH, GLOB(allowAnonymous));
	CLI_AMI_OUTPUT_PARAM("Hotline_Exten", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline->line->context));
	CLI_AMI_OUTPUT_PARAM("Hotline_Context", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline->exten));

	sccp_free(debugcategories);
	sccp_globals_unlock(lock);

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_globals_usage[] = "Usage: sccp show globals\n" "       Lists global settings for the SCCP subsystem.\n";
static char ami_globals_usage[] = "Usage: SCCPShowGlobals\n" "       Lists global settings for the SCCP subsystem.\n\n" "PARAMS: None\n";

#define CLI_COMMAND "sccp", "show", "globals"
#define AMI_COMMAND "SCCPShowGlobals"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_globals, sccp_show_globals, "List defined SCCP global settings", cli_globals_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* --------------------------------------------------------------------------------------------------------SHOW DEVICES- */

/*!
 * \brief Show Devices
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- devices
 */
//static int sccp_show_devices(int fd, int argc, char *argv[])
static int sccp_show_devices(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	struct tm *timeinfo;
	char regtime[25];
	int local_total = 0;

	// table definition
#define CLI_AMI_TABLE_NAME Devices
#define CLI_AMI_TABLE_PER_ENTRY_NAME Device

#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_device_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(devices)
#define CLI_AMI_TABLE_LIST_ITER_VAR d
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION timeinfo = localtime(&d->registrationTime); strftime(regtime, sizeof(regtime), "%c ", timeinfo);
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 													\
		CLI_AMI_TABLE_FIELD(Name,		s,	40,	d->description)							\
		CLI_AMI_TABLE_FIELD(Address,		s,	20,	(d->session) ? pbx_inet_ntoa(d->session->sin.sin_addr) : "--")	\
		CLI_AMI_TABLE_FIELD(Mac,		s,	16,	d->id)								\
		CLI_AMI_TABLE_FIELD(RegState,		s,	10, 	deviceregistrationstatus2str(d->registrationState))		\
		CLI_AMI_TABLE_FIELD(RegTime,		s, 	25, 	regtime)							\
		CLI_AMI_TABLE_FIELD(Act,		s,  	3, 	(d->active_channel) ? "Yes" : "No")				\
		CLI_AMI_TABLE_FIELD(Lines, 		d,  	5, 	d->configurationStatistic.numberOfLines)
#include "sccp_cli_table.h"
	// end of table definition
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_devices_usage[] = "Usage: sccp show devices\n" "       Lists defined SCCP devices.\n";
static char ami_devices_usage[] = "Usage: SCCPShowDevices\n" "Lists defined SCCP devices.\n\n" "PARAMS: None\n";

#define CLI_COMMAND "sccp", "show", "devices"
#define AMI_COMMAND "SCCPShowDevices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_devices, sccp_show_devices, "List defined SCCP devices", cli_devices_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* --------------------------------------------------------------------------------------------------------SHOW DEVICE- */

/*!
 * \brief Show Device
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 *
 * \lock
 * 	- device
 * 	  - device->buttonconfig
 */
static int sccp_show_device(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d;
	sccp_line_t *l;
	char pref_buf[256];
	char cap_buf[512];
	PBX_VARIABLE_TYPE *v = NULL;
	sccp_linedevices_t *linedevice;
	int local_total = 0;

	const char *dev;

	if (s) {
		dev = strdupa(astman_get_header(m, "DeviceName"));
	} else {
		if (argc < 4)
			return RESULT_SHOWUSAGE;
		dev = strdupa(argv[3]);
	}
	d = sccp_device_find_byid(dev, TRUE);

	if (!d) {
		pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
		CLI_AMI_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);
	}
	sccp_device_lock(d);

	sccp_multiple_codecs2str(pref_buf, sizeof(pref_buf) - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver device settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_send_listack(s, m, argv[0], "start");
		CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", argv[0]);
	}
	CLI_AMI_OUTPUT_PARAM("Arguments", CLI_AMI_LIST_WIDTH, "%s/%s/%s/%s (%d)", argv[0], argv[1], argv[2], argv[3], argc);
	CLI_AMI_OUTPUT_PARAM("MAC-Address", CLI_AMI_LIST_WIDTH, "%s", d->id);
	CLI_AMI_OUTPUT_PARAM("Protocol Version", CLI_AMI_LIST_WIDTH, "Supported '%d', In Use '%d'", d->protocolversion, d->inuseprotocolversion);
	CLI_AMI_OUTPUT_PARAM("Protocol In Use", CLI_AMI_LIST_WIDTH, "%s Version %d", d->protocol ? d->protocol->name : "NONE", d->protocol ? d->protocol->version : 0);
	CLI_AMI_OUTPUT_PARAM("Tokenstate", CLI_AMI_LIST_WIDTH, "%s", d->status.token ? ((d->status.token == SCCP_TOKEN_STATE_ACK) ? "Token acknowledged" : "Token rejected") : "no toke requested");
	CLI_AMI_OUTPUT_PARAM("Keepalive", CLI_AMI_LIST_WIDTH, "%d", d->keepalive);
	CLI_AMI_OUTPUT_PARAM("Registration state", CLI_AMI_LIST_WIDTH, "%s(%d)", deviceregistrationstatus2str(d->registrationState), d->registrationState);
	CLI_AMI_OUTPUT_PARAM("State", CLI_AMI_LIST_WIDTH, "%s(%d)", devicestatus2str(d->state), d->state);
	CLI_AMI_OUTPUT_BOOL("MWI handset light", CLI_AMI_LIST_WIDTH, d->mwilight);
	CLI_AMI_OUTPUT_PARAM("Description", CLI_AMI_LIST_WIDTH, "%s", d->description);
	CLI_AMI_OUTPUT_PARAM("Config Phone Type", CLI_AMI_LIST_WIDTH, "%s", d->config_type);
	CLI_AMI_OUTPUT_PARAM("Skinny Phone Type", CLI_AMI_LIST_WIDTH, "%s(%d)", devicetype2str(d->skinny_type), d->skinny_type);
	CLI_AMI_OUTPUT_YES_NO("Softkey support", CLI_AMI_LIST_WIDTH, d->softkeysupport);
	CLI_AMI_OUTPUT_YES_NO("BTemplate support", CLI_AMI_LIST_WIDTH, d->buttonTemplate);
	CLI_AMI_OUTPUT_YES_NO("linesRegistered", CLI_AMI_LIST_WIDTH, d->linesRegistered);
	CLI_AMI_OUTPUT_PARAM("Image Version", CLI_AMI_LIST_WIDTH, "%s", d->imageversion);
	CLI_AMI_OUTPUT_PARAM("Timezone Offset", CLI_AMI_LIST_WIDTH, "%d", d->tz_offset);
	CLI_AMI_OUTPUT_PARAM("Capabilities", CLI_AMI_LIST_WIDTH, "%s", cap_buf);
	CLI_AMI_OUTPUT_PARAM("Codecs preference", CLI_AMI_LIST_WIDTH, "%s", pref_buf);
	CLI_AMI_OUTPUT_PARAM("Audio TOS", CLI_AMI_LIST_WIDTH, "%d", d->audio_tos);
	CLI_AMI_OUTPUT_PARAM("Audio COS", CLI_AMI_LIST_WIDTH, "%d", d->audio_cos);
	CLI_AMI_OUTPUT_PARAM("Video TOS", CLI_AMI_LIST_WIDTH, "%d", d->video_tos);
	CLI_AMI_OUTPUT_PARAM("Video COS", CLI_AMI_LIST_WIDTH, "%d", d->video_cos);
	CLI_AMI_OUTPUT_BOOL("DND Feature enabled", CLI_AMI_LIST_WIDTH, d->dndFeature.enabled);
	CLI_AMI_OUTPUT_PARAM("DND Status", CLI_AMI_LIST_WIDTH, "%s", (d->dndFeature.status) ? dndmode2str(d->dndFeature.status) : "Disabled");
	CLI_AMI_OUTPUT_BOOL("Can Transfer", CLI_AMI_LIST_WIDTH, d->transfer);
	CLI_AMI_OUTPUT_BOOL("Can Park", CLI_AMI_LIST_WIDTH, d->park);
	CLI_AMI_OUTPUT_BOOL("Private softkey", CLI_AMI_LIST_WIDTH, d->privacyFeature.enabled);
	CLI_AMI_OUTPUT_BOOL("Can CFWDALL", CLI_AMI_LIST_WIDTH, d->cfwdall);
	CLI_AMI_OUTPUT_BOOL("Can CFWBUSY", CLI_AMI_LIST_WIDTH, d->cfwdbusy);
	CLI_AMI_OUTPUT_BOOL("Can CFWNOANSWER", CLI_AMI_LIST_WIDTH, d->cfwdnoanswer);
	CLI_AMI_OUTPUT_PARAM("Dtmf mode", CLI_AMI_LIST_WIDTH, "%s", (d->dtmfmode) ? "Out-of-Band" : "In-Band");
	CLI_AMI_OUTPUT_PARAM("digit timeout", CLI_AMI_LIST_WIDTH, "%d", d->digittimeout);
	CLI_AMI_OUTPUT_BOOL("Nat", CLI_AMI_LIST_WIDTH, d->nat);
	CLI_AMI_OUTPUT_BOOL("Videosupport?", CLI_AMI_LIST_WIDTH, sccp_device_isVideoSupported(d));
	CLI_AMI_OUTPUT_BOOL("Direct RTP", CLI_AMI_LIST_WIDTH, d->directrtp);
	CLI_AMI_OUTPUT_BOOL("Trust phone ip", CLI_AMI_LIST_WIDTH, d->trustphoneip);
	CLI_AMI_OUTPUT_PARAM("Bind Address", CLI_AMI_LIST_WIDTH, "%s:%d", (d->session) ? pbx_inet_ntoa(d->session->sin.sin_addr) : "???.???.???.???", (d->session) ? ntohs(d->session->sin.sin_port) : 0);
	CLI_AMI_OUTPUT_PARAM("Our Address", CLI_AMI_LIST_WIDTH, "%s", (d->session) ? ast_inet_ntoa(d->session->ourip) : "???.???.???.???");
	CLI_AMI_OUTPUT_PARAM("Early RTP", CLI_AMI_LIST_WIDTH, "%s (%s)", d->earlyrtp ? "Yes" : "No", d->earlyrtp ? sccp_indicate2str(d->earlyrtp) : "none");
	CLI_AMI_OUTPUT_PARAM("Device State (Acc.)", CLI_AMI_LIST_WIDTH, "%s", accessorystatus2str(d->accessorystatus));
	CLI_AMI_OUTPUT_PARAM("Last Used Accessory", CLI_AMI_LIST_WIDTH, "%s", accessory2str(d->accessoryused));
	CLI_AMI_OUTPUT_PARAM("Last dialed number", CLI_AMI_LIST_WIDTH, "%s", d->lastNumber);
#ifdef CS_ADV_FEATURES
	CLI_AMI_OUTPUT_BOOL("Use Placed Calls", CLI_AMI_LIST_WIDTH, d->useRedialMenu);
#endif
#ifdef CS_DYNAMIC_CONFIG
	CLI_AMI_OUTPUT_BOOL("PendingUpdate", CLI_AMI_LIST_WIDTH, d->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("PendingDelete", CLI_AMI_LIST_WIDTH, d->pendingDelete);
#endif
#ifdef CS_SCCP_PICKUP
	CLI_AMI_OUTPUT_PARAM("Pickup Extension", CLI_AMI_LIST_WIDTH, "%d", d->pickupexten);
	CLI_AMI_OUTPUT_PARAM("Pickup Context", CLI_AMI_LIST_WIDTH, "%s (%s)", d->pickupcontext, pbx_context_find(d->pickupcontext) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_BOOL("Pickup Mode Answer", CLI_AMI_LIST_WIDTH, d->pickupmodeanswer);
#endif
	if (SCCP_LIST_FIRST(&d->buttonconfig)) {
		// BUTTONS
#define CLI_AMI_TABLE_NAME Buttons
#define CLI_AMI_TABLE_PER_ENTRY_NAME Button
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_buttonconfig_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(TypeStr,		s,	29,	sccp_buttontype2str(buttonconfig->type))		\
			CLI_AMI_TABLE_FIELD(Type,		d,	20,	buttonconfig->type)					\
			CLI_AMI_TABLE_FIELD(pendUpdt,		s,	13, 	buttonconfig->pendingUpdate ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(pendDel,		s, 	13, 	buttonconfig->pendingUpdate ? "Yes" : "No")
#include "sccp_cli_table.h"

		// LINES
#define CLI_AMI_TABLE_NAME Lines
#define CLI_AMI_TABLE_PER_ENTRY_NAME Line
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
			l = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);						\
			if (l) {													\
				linedevice = sccp_util_getDeviceConfiguration(d, l);
#define CLI_AMI_TABLE_AFTER_ITERATION 											\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(Name,		s,	23,	l->name)						\
			CLI_AMI_TABLE_FIELD(Suffix,		s,	6,	buttonconfig->button.line.subscriptionId.number)	\
			CLI_AMI_TABLE_FIELD(Label,		s,	19, 	l->label)						\
			CLI_AMI_TABLE_FIELD(CfwdType,		s, 	10, 	(linedevice && linedevice->cfwdAll.enabled ? "All" : (linedevice && linedevice->cfwdBusy.enabled ? "Busy" : "None")))	\
			CLI_AMI_TABLE_FIELD(CfwdNumber,		s, 	16, 	(linedevice && linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : (linedevice && linedevice->cfwdBusy.enabled ? linedevice->cfwdBusy.number : "")))
#include "sccp_cli_table.h"

		// SPEEDDIALS
#define CLI_AMI_TABLE_NAME Speeddials
#define CLI_AMI_TABLE_PER_ENTRY_NAME Speeddial
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
			if (buttonconfig->type == SPEEDDIAL) {
#define CLI_AMI_TABLE_AFTER_ITERATION 											\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(Name,		s,	23,	buttonconfig->label)					\
			CLI_AMI_TABLE_FIELD(Number,		s,	26,	buttonconfig->button.speeddial.ext)			\
			CLI_AMI_TABLE_FIELD(Hint,		s,	27, 	buttonconfig->button.speeddial.hint)
//                      CLI_AMI_TABLE_FIELD(HintStatus,         s,      20,     ast_extension_state2str(ast_extension_state()))
#include "sccp_cli_table.h"

		// FEATURES
#define CLI_AMI_TABLE_NAME Features
#define CLI_AMI_TABLE_PER_ENTRY_NAME Feature
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
			if (buttonconfig->type == FEATURE) {
#define CLI_AMI_TABLE_AFTER_ITERATION 											\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(Name,		s,	23,	buttonconfig->label)					\
			CLI_AMI_TABLE_FIELD(Options,		s,	26,	buttonconfig->button.feature.options)			\
			CLI_AMI_TABLE_FIELD(Status,		d,	27, 	buttonconfig->button.feature.status)
#include "sccp_cli_table.h"

		// SERVICEURL
#define CLI_AMI_TABLE_NAME ServiceURLs
#define CLI_AMI_TABLE_PER_ENTRY_NAME ServiceURL
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
			if (buttonconfig->type == SERVICE) {
#define CLI_AMI_TABLE_AFTER_ITERATION 											\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(Name,		s,	23,	buttonconfig->label)					\
			CLI_AMI_TABLE_FIELD(URL,		s,	54,	buttonconfig->button.service.url)
#include "sccp_cli_table.h"
	}

	if (d->variables) {
		// SERVICEURL
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = d->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Name,		s,	10,	v->name)						\
			CLI_AMI_TABLE_FIELD(Value,		s,	17,	v->value)
#include "sccp_cli_table.h"
	}
	sccp_device_unlock(d);
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_device_usage[] = "Usage: sccp show device <deviceId>\n" "       Lists device settings for the SCCP subsystem.\n";
static char ami_device_usage[] = "Usage: SCCPShowDevice\n" "Lists device settings for the SCCP subsystem.\n\n" "PARAMS: DeviceName\n";

#define CLI_COMMAND "sccp", "show", "device"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
#define AMI_COMMAND "SCCPShowDevice"
#define CLI_AMI_PARAMS "DeviceName"
CLI_AMI_ENTRY(show_device, sccp_show_device, "Lists device settings", cli_device_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* ---------------------------------------------------------------------------------------------------------SHOW LINES- */

/*!
 * \brief Show Lines
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- lines
 * 	  - devices
 */
//static int sccp_show_lines(int fd, int argc, char *argv[])
static int sccp_show_lines(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *l = NULL;
	sccp_channel_t *channel = NULL;
	sccp_device_t *d = NULL;
	boolean_t found_linedevice;
	char cap_buf[512];
	PBX_VARIABLE_TYPE *v = NULL;
	int local_total = 0;
	const char *actionid = "";

	if (!s) {
		pbx_cli(fd, "\n+--- Lines ----------------------------------------------------------------------------------------------+\n");
		pbx_cli(fd, "| %-16s %-8s %-16s %-4s %-4s %-49s |\n", "Ext", "Suffix", "Device", "MWI", "Chs", "Active Channel");
		pbx_cli(fd, "+ ================ ======== ================ ==== ==== ================================================= +\n");
	} else {
		astman_append(s, "Event: TableStart\r\n");
		local_total++;
		astman_append(s, "TableName: Lines\r\n");
		local_total++;
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n\r\n", actionid);
		}
	}
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		channel = NULL;
		// \todo handle shared line
		d = NULL;

		if (d) {
			sccp_device_lock(d);
			channel = d->active_channel;
			sccp_device_unlock(d);
		}

		if (!channel || (channel->line != l))
			channel = NULL;

		memset(&cap_buf, 0, sizeof(cap_buf));

		if (channel && channel->owner) {
			pbx_getformatname_multiple(cap_buf, sizeof(cap_buf), channel->owner->nativeformats);
		}

		sccp_linedevices_t *linedevice;

		found_linedevice = 0;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if ((d = linedevice->device)) {
				if (!s) {
					pbx_cli(fd, "| %-16s %-8s %-16s %-4s %-4d %-10s %-10s %-16s %-10s |\n",
						!found_linedevice ? l->name : " +--", linedevice->subscriptionId.number, (d) ? d->id : "--", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF", SCCP_RWLIST_GETSIZE(l->channels), (channel) ? sccp_indicate2str(channel->state) : "--", (channel) ? calltype2str(channel->calltype) : "", (channel) ? ((channel->calltype == SKINNY_CALLTYPE_OUTBOUND) ? channel->callInfo.calledPartyName : channel->callInfo.callingPartyName) : "", cap_buf);
				} else {
					astman_append(s, "Event: LineEntry\r\n");
					astman_append(s, "ChannelType: SCCP\r\n");
					astman_append(s, "ChannelObjectType: Line\r\n");
					astman_append(s, "Exten: %s\r\n", l->name);
					astman_append(s, "SubscriptionNumber: %s\r\n", linedevice->subscriptionId.number);
					astman_append(s, "Device: %s\r\n", (d) ? d->id : "--");
					astman_append(s, "MWI: %s\r\n", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF");
					astman_append(s, "ActiveChannels: %d\r\n", SCCP_RWLIST_GETSIZE(l->channels));
					astman_append(s, "ChannelState: %s\r\n", (channel) ? sccp_indicate2str(channel->state) : "--");
					astman_append(s, "CallType: %s\r\n", (channel) ? calltype2str(channel->calltype) : "");
					astman_append(s, "PartyName: %s\r\n", (channel) ? ((channel->calltype == SKINNY_CALLTYPE_OUTBOUND) ? channel->callInfo.calledPartyName : channel->callInfo.callingPartyName) : "");
					astman_append(s, "Capabilities: %s\r\n", cap_buf);
					astman_append(s, "\r\n");
				}
				found_linedevice = 1;
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);

		if (found_linedevice == 0) {
			if (!s) {
				pbx_cli(fd, "| %-16s %-8s %-16s %-4s %-4d %-10s %-10s %-16s %-10s |\n", l->name, "", "--", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF", SCCP_RWLIST_GETSIZE(l->channels), (channel) ? sccp_indicate2str(channel->state) : "--", (channel) ? calltype2str(channel->calltype) : "", (channel) ? ((channel->calltype == SKINNY_CALLTYPE_OUTBOUND) ? channel->callInfo.calledPartyName : channel->callInfo.callingPartyName) : "", cap_buf);
			} else {
				astman_append(s, "Event: LineEntry\r\n");
				astman_append(s, "ChannelType: SCCP\r\n");
				astman_append(s, "ChannelObjectType: Line\r\n");
				astman_append(s, "Exten: %s\r\n", l->name);
				astman_append(s, "Device: %s\r\n", "(null)");
				astman_append(s, "MWI: %s\r\n", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF");
				astman_append(s, "\r\n");
			}
		}
		if (!s) {
			for (v = l->variables; v; v = v->next)
				pbx_cli(fd, "| %-16s %-8s %-16s %-59.59s |\n", "", "Variable", v->name, v->value);

			if (strcmp(l->defaultSubscriptionId.number, "") || strcmp(l->defaultSubscriptionId.name, ""))
				pbx_cli(fd, "| %-16s %-8s %-16s %-59.59s |\n", "", "SubscrId", l->defaultSubscriptionId.number, l->defaultSubscriptionId.name);
		}
	}
	if (!s) {
		pbx_cli(fd, "+--------------------------------------------------------------------------------------------------------+\n");
	} else {
		astman_append(s, "Event: TableEnd\r\n");
		local_total++;
		astman_append(s, "TableName: Lines\r\n");
		local_total++;
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
			local_total++;
		}
		astman_append(s, "\r\n");
		local_total++;

	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_lines_usage[] = "Usage: sccp show lines\n" "       Lists all lines known to the SCCP subsystem.\n";
static char ami_lines_usage[] = "Usage: SCCPShowLines\n" "Lists all lines known to the SCCP subsystem\n" "PARAMS: None\n";

#define CLI_COMMAND "sccp", "show", "lines"
#define AMI_COMMAND "SCCPShowLines"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_lines, sccp_show_lines, "List defined SCCP Lines", cli_lines_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* -----------------------------------------------------------------------------------------------------------SHOW LINE- */

/*!
 * \brief Show Line
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- line
 * 	  - line->devices
 */
//static int sccp_show_line(int fd, int argc, char *argv[])
static int sccp_show_line(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *l;
	sccp_linedevices_t *linedevice;
	PBX_VARIABLE_TYPE *v = NULL;
	struct ast_str *callgroup_buf = pbx_str_alloca(512);
	struct ast_str *pickupgroup_buf = pbx_str_alloca(512);
	int local_total = 0;

	const char *line;

	if (s) {
		line = strdupa(astman_get_header(m, "LineName"));
	} else {
		if (argc < 4)
			return RESULT_SHOWUSAGE;
		line = strdupa(argv[3]);
	}
	l = sccp_line_find_byname(line);

	if (!l) {
		pbx_log(LOG_WARNING, "Failed to get line %s\n", line);
		CLI_AMI_ERROR(fd, s, m, "Can't find settings for line %s\n", line);
	}

	sccp_line_lock(l);

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver line settings ------------------------------------------------------------------------------------\n");
	} else {
		astman_send_listack(s, m, argv[0], "start");
		CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", argv[0]);
	}
	CLI_AMI_OUTPUT_PARAM("Name", CLI_AMI_LIST_WIDTH, "%s", l->name ? l->name : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Description", CLI_AMI_LIST_WIDTH, "%s", l->description ? l->description : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Label", CLI_AMI_LIST_WIDTH, "%s", l->label ? l->label : "<not set>");
	CLI_AMI_OUTPUT_PARAM("ID", CLI_AMI_LIST_WIDTH, "%s", l->id ? l->id : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Pin", CLI_AMI_LIST_WIDTH, "%s", l->pin ? l->pin : "<not set>");
	CLI_AMI_OUTPUT_PARAM("VoiceMail number", CLI_AMI_LIST_WIDTH, "%s", l->vmnum ? l->vmnum : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Transfer to Voicemail", CLI_AMI_LIST_WIDTH, "%s", l->trnsfvm ? l->trnsfvm : "No");
	CLI_AMI_OUTPUT_BOOL("MeetMe enabled", CLI_AMI_LIST_WIDTH, l->meetme);
	CLI_AMI_OUTPUT_BOOL("MeetMe number", CLI_AMI_LIST_WIDTH, l->meetmenum);
	CLI_AMI_OUTPUT_PARAM("MeetMe Options", CLI_AMI_LIST_WIDTH, "%s", l->meetmeopts);
	CLI_AMI_OUTPUT_PARAM("Context", CLI_AMI_LIST_WIDTH, "%s (%s)", l->context ? l->context : "<not set>", pbx_context_find(l->context) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_PARAM("Language", CLI_AMI_LIST_WIDTH, "%s", l->language ? l->language : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Account Code", CLI_AMI_LIST_WIDTH, "%s", l->accountcode ? l->accountcode : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Musicclass", CLI_AMI_LIST_WIDTH, "%s", l->musicclass ? l->musicclass : "<not set>");
	CLI_AMI_OUTPUT_PARAM("AmaFlags", CLI_AMI_LIST_WIDTH, "%d", l->amaflags);
	sccp_print_group(callgroup_buf, sizeof(callgroup_buf), GLOB(callgroup));
	CLI_AMI_OUTPUT_PARAM("Call Group", CLI_AMI_LIST_WIDTH, "%s", callgroup_buf ? pbx_str_buffer(callgroup_buf) : "");
#ifdef CS_SCCP_PICKUP
	sccp_print_group(pickupgroup_buf, sizeof(pickupgroup_buf), GLOB(pickupgroup));
	CLI_AMI_OUTPUT_PARAM("Pickup Group", CLI_AMI_LIST_WIDTH, "%s", pickupgroup_buf ? pbx_str_buffer(pickupgroup_buf) : "");
#endif
	CLI_AMI_OUTPUT_PARAM("Caller ID name", CLI_AMI_LIST_WIDTH, "%s", l->cid_name ? l->cid_name : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Caller ID number", CLI_AMI_LIST_WIDTH, "%s", l->cid_num ? l->cid_num : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Incoming Calls limit", CLI_AMI_LIST_WIDTH, "%d", l->incominglimit);
	CLI_AMI_OUTPUT_PARAM("Active Channel Count", CLI_AMI_LIST_WIDTH, "%d", SCCP_RWLIST_GETSIZE(l->channels));
	CLI_AMI_OUTPUT_PARAM("Sec. Dialtone Digits", CLI_AMI_LIST_WIDTH, "%s", l->secondary_dialtone_digits ? l->secondary_dialtone_digits : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Sec. Dialtone", CLI_AMI_LIST_WIDTH, "0x%02x", l->secondary_dialtone_tone);
	CLI_AMI_OUTPUT_BOOL("Echo Cancellation", CLI_AMI_LIST_WIDTH, l->echocancel);
	CLI_AMI_OUTPUT_BOOL("Silence Suppression", CLI_AMI_LIST_WIDTH, l->silencesuppression);
	CLI_AMI_OUTPUT_BOOL("Can Transfer", CLI_AMI_LIST_WIDTH, l->transfer);
	CLI_AMI_OUTPUT_PARAM("Can DND", CLI_AMI_LIST_WIDTH, "%s", (l->dndmode) ? dndmode2str(l->dndmode) : "Disabled");
#ifdef CS_SCCP_REALTIME
	CLI_AMI_OUTPUT_BOOL("Is Realtime Line", CLI_AMI_LIST_WIDTH, l->realtime);
#endif
#ifdef CS_DYNAMIC_CONFIG
	CLI_AMI_OUTPUT_BOOL("Pending Delete", CLI_AMI_LIST_WIDTH, l->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("Pending Update", CLI_AMI_LIST_WIDTH, l->pendingDelete);
#endif

	CLI_AMI_OUTPUT_PARAM("Registration Extension", CLI_AMI_LIST_WIDTH, "%s", l->regexten ? l->regexten : "Unset");
	CLI_AMI_OUTPUT_PARAM("Registration Context", CLI_AMI_LIST_WIDTH, "%s", l->regcontext ? l->regcontext : "Unset");

	CLI_AMI_OUTPUT_BOOL("Adhoc Number Assigned", CLI_AMI_LIST_WIDTH, l->adhocNumber ? l->adhocNumber : "No");
	CLI_AMI_OUTPUT_PARAM("Message Waiting New.", CLI_AMI_LIST_WIDTH, "%i", l->voicemailStatistic.newmsgs);
	CLI_AMI_OUTPUT_PARAM("Message Waiting Old.", CLI_AMI_LIST_WIDTH, "%i", l->voicemailStatistic.oldmsgs);

	// Line attached to these devices
#define CLI_AMI_TABLE_NAME AttachedDevices
#define CLI_AMI_TABLE_PER_ENTRY_NAME Device
#define CLI_AMI_TABLE_LIST_ITER_HEAD &l->devices
#define CLI_AMI_TABLE_LIST_ITER_VAR linedevice
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
		CLI_AMI_TABLE_FIELD(Device,		s,	15,	linedevice->device->id)					\
		CLI_AMI_TABLE_FIELD(CfwdType,		s,	8,	linedevice->cfwdAll.enabled ? "All" : (linedevice->cfwdBusy.enabled ? "Busy" : ""))					\
		CLI_AMI_TABLE_FIELD(CfwdNumber,		s,	20,	linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : (linedevice->cfwdBusy.enabled ? linedevice->cfwdBusy.number : ""))
#include "sccp_cli_table.h"

	if (l->variables) {
		// SERVICEURL
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = l->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 										\
			CLI_AMI_TABLE_FIELD(Name,		s,	10,	v->name)				\
			CLI_AMI_TABLE_FIELD(Value,		s,	17,	v->value)
#include "sccp_cli_table.h"
	}
	sccp_line_unlock(l);
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_line_usage[] = "Usage: sccp show line <lineId>\n" "       List defined SCCP line settings.\n";
static char ami_line_usage[] = "Usage: SCCPShowLine\n" "List defined SCCP line settings.\n\n" "PARAMS: LineName\n";

#define CLI_COMMAND "sccp", "show", "line"
#define CLI_COMPLETE SCCP_CLI_LINE_COMPLETER
#define AMI_COMMAND "SCCPShowLine"
#define CLI_AMI_PARAMS "LineName"
CLI_AMI_ENTRY(show_line, sccp_show_line, "List defined SCCP line settings", cli_line_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* --------------------------------------------------------------------------------------------------------SHOW CHANNELS- */

/*!
 * \brief Show Channels
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- lines
 * 	  - line
 * 	    - line->channels
 */
//static int sccp_show_channels(int fd, int argc, char *argv[])
static int sccp_show_channels(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_channel_t *channel;
	sccp_line_t *l;
	int local_total = 0;
	char tmpname[20];

	// Channels
#define CLI_AMI_TABLE_NAME Channels
#define CLI_AMI_TABLE_PER_ENTRY_NAME Channel
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(lines)
#define CLI_AMI_TABLE_LIST_ITER_VAR l
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
		sccp_line_lock(l);												\
		SCCP_LIST_LOCK(&l->channels);											\
		SCCP_LIST_TRAVERSE(&l->channels, channel, list) {								\
			snprintf(tmpname, sizeof(tmpname), "SCCP/%s-%08x", l->name, channel->callid);

#define CLI_AMI_TABLE_AFTER_ITERATION 											\
		}														\
		SCCP_LIST_UNLOCK(&l->channels);											\
		sccp_line_unlock(l);

#define CLI_AMI_TABLE_FIELDS 												\
		CLI_AMI_TABLE_FIELD(ID,			d,	5,	channel->callid)					\
		CLI_AMI_TABLE_FIELD(PBX,		s,	20,	strdupa(tmpname))						\
		CLI_AMI_TABLE_FIELD(Line,		s,	10,	channel->line->name)					\
		CLI_AMI_TABLE_FIELD(Device,		s,	16,	(sccp_channel_getDevice(channel)) ? sccp_channel_getDevice(channel)->id : "(unknown)")	\
		CLI_AMI_TABLE_FIELD(DeviceDescr,	s,	32,	(sccp_channel_getDevice(channel)) ? sccp_channel_getDevice(channel)->description : "(unknown)")	\
		CLI_AMI_TABLE_FIELD(NumCalled,		s,	10,	channel->callInfo.calledPartyNumber)			\
		CLI_AMI_TABLE_FIELD(PBX State,		s,	10,	(channel->owner) ? pbx_state2str(channel->owner->_state) : "(none)")	\
		CLI_AMI_TABLE_FIELD(SCCP State,		s,	10,	sccp_indicate2str(channel->state))			\
		CLI_AMI_TABLE_FIELD(ReadCodec,		s,	10,	codec2name(channel->rtp.audio.readFormat))		\
		CLI_AMI_TABLE_FIELD(WriteCodec,		s,	10,	codec2name(channel->rtp.audio.writeFormat))
#include "sccp_cli_table.h"

	if (s)
		*total = local_total;
	return RESULT_SUCCESS;
}

static char cli_channels_usage[] = "Usage: sccp show channels\n" "       Lists active channels for the SCCP subsystem.\n";
static char ami_channels_usage[] = "Usage: SCCPShowChannels\n" "Lists active channels for the SCCP subsystem.\n\n" "PARAMS: None\n";

#define CLI_COMMAND "sccp", "show", "channels"
#define AMI_COMMAND "SCCPShowChannels"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_channels, sccp_show_channels, "Lists active SCCP channels", cli_channels_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* -------------------------------------------------------------------------------------------------------SHOW SESSIONS- */

/*!
 * \brief Show Sessions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- sessions
 * 	  - session
 * 	  - device
 * 	  - session
 */
//static int sccp_show_sessions(int fd, int argc, char *argv[])
static int sccp_show_sessions(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
//      sccp_session_t *session=NULL;
	sccp_device_t *d = NULL;
	int local_total = 0;

	// Channels
#define CLI_AMI_TABLE_NAME Sessions
#define CLI_AMI_TABLE_PER_ENTRY_NAME Session
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(sessions)
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_session_t
#define CLI_AMI_TABLE_LIST_ITER_VAR session
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
		sccp_session_lock(session);											\
		d = session->device;												\
		if (d) {													\
			sccp_device_lock(d);

#define CLI_AMI_TABLE_AFTER_ITERATION 											\
			sccp_device_unlock(d);											\
		}														\
		sccp_session_unlock(session);

#define CLI_AMI_TABLE_FIELDS 												\
		CLI_AMI_TABLE_FIELD(Socket,			d,	10,	session->fds[0].fd)				\
		CLI_AMI_TABLE_FIELD(IP,				s,	CLI_AMI_LIST_WIDTH,	pbx_inet_ntoa(session->sin.sin_addr))		\
		CLI_AMI_TABLE_FIELD(Port,			d,	5,	session->sin.sin_port)				\
		CLI_AMI_TABLE_FIELD(KA,				d,	4,	(uint32_t) (time(0) - session->lastKeepAlive))	\
		CLI_AMI_TABLE_FIELD(Device,			s,	15,	(d) ? d->id : "--")				\
		CLI_AMI_TABLE_FIELD(State,			s,	14,	(d) ? devicestatus2str(d->state) : "--")	\
		CLI_AMI_TABLE_FIELD(Type,			s,	15,	(d) ? devicetype2str(d->skinny_type) : "--")
#include "sccp_cli_table.h"

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_sessions_usage[] = "Usage: sccp show sessions\n" "	Show All SCCP Sessions.\n";
static char ami_sessions_usage[] = "Usage: SCCPShowSessions\n" "Show All SCCP Sessions.\n\n" "PARAMS: None\n";

#define CLI_COMMAND "sccp", "show", "sessions"
#define AMI_COMMAND "SCCPShowSessions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_sessions, sccp_show_sessions, "Show all SCCP sessions", cli_sessions_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* ---------------------------------------------------------------------------------------------SHOW_MWI_SUBSCRIPTIONS- */

/*!
 * \brief Show MWI Subscriptions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \todo TO BE IMPLEMENTED: sccp show mwi subscriptions
 */
static int sccp_show_mwi_subscriptions(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{

/*
 	sccp_line_t *line=NULL;
 	sccp_mailboxLine_t *mailboxLine = NULL;
 	char linebuf[30]="";
 	int local_total=0;
 
 	 Channels
 	#define CLI_AMI_TABLE_NAME MWI_Subscriptions
 	#define CLI_AMI_TABLE_PER_ENTRY_NAME Mailbox_Subscriber
 	#define CLI_AMI_TABLE_LIST_ITER_HEAD &sccp_mailbox_subscriptions
 	#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_mailbox_subscriber_list_t
 	#define CLI_AMI_TABLE_LIST_ITER_VAR subscription
 	#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
 	#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
 	#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
 	#define CLI_AMI_TABLE_BEFORE_ITERATION 											\
 		SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {					\
 			line = mailboxLine->line;										\
 			sprintf(linebuf,"%s",line->name);									\
// 			if (line->name) {											\
// 				ast_join(linebuf, sizeof(linebuf), (const char * const *) line->name);				\
// 			}													\
 		}	
 
 	#define CLI_AMI_TABLE_FIELDS 												\
 		CLI_AMI_TABLE_FIELD(Mailbox,			s,	10,	subscription->mailbox)				\
 		CLI_AMI_TABLE_FIELD(LineName,			s,	30,	linebuf)					\
 		CLI_AMI_TABLE_FIELD(Context,			s,	15,	subscription->context)				\
 		CLI_AMI_TABLE_FIELD(New,			d,	3,	subscription->currentVoicemailStatistic.newmsgs)\
 		CLI_AMI_TABLE_FIELD(Old,			d,	3,	subscription->currentVoicemailStatistic.oldmsgs)
 	#include "sccp_cli_table.h"
 	
 	if (s) *total=local_total;

	return RESULT_SUCCESS;
*/

	pbx_cli(fd, "Command has not been fully implemented yet!\n");
	return RESULT_FAILURE;
}

static char cli_mwi_subscriptions_usage[] = "Usage: sccp show mwi subscriptions\n" "	Show All SCCP MWI Subscriptions.\n";
static char ami_mwi_subscriptions_usage[] = "Usage: SCCPShowMWISubscriptions\n" "	Show All SCCP MWI Subscriptions.\n";

#define CLI_COMMAND "sccp", "show", "mwi", "subscriptions"
#define AMI_COMMAND "SCCPShowMWISubscriptions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_mwi_subscriptions, sccp_show_mwi_subscriptions, "Show all SCCP MWI subscriptions", cli_mwi_subscriptions_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND

/* --------------------------------------------------------------------------------------------------SHOW_SOKFTKEYSETS- */

/*!
 * \brief Show Sessions
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- softKeySetConfig
 */
static int sccp_show_softkeysets(int fd, int argc, char *argv[])
{
	sccp_softKeySetConfiguration_t *softkeyset = NULL;

	pbx_cli(fd, "SoftKeySets: %d\n", softKeySetConfig.size);
	uint8_t i = 0;
	uint8_t v_count = 0;
	uint8_t c = 0;

	SCCP_LIST_LOCK(&softKeySetConfig);
	SCCP_LIST_TRAVERSE(&softKeySetConfig, softkeyset, list) {
		v_count = sizeof(softkeyset->modes) / sizeof(softkey_modes);

		pbx_cli(fd, "name: %s\n", softkeyset->name);
		pbx_cli(fd, "number of softkeysets: %d\n", v_count);

		for (i = 0; i < v_count; i++) {
			const uint8_t *b = softkeyset->modes[i].ptr;

			pbx_cli(fd, "      Set[%-2d]= ", i);

			for (c = 0; c < softkeyset->modes[i].count; c++) {
				pbx_cli(fd, "%-2d:%-10s ", c, label2str(b[c]));
			}

			pbx_cli(fd, "\n");
		}

		pbx_cli(fd, "\n");

	}
	SCCP_LIST_UNLOCK(&softKeySetConfig);
	pbx_cli(fd, "\n");
	return RESULT_SUCCESS;
}

static char show_softkeysets_usage[] = "Usage: sccp show softkeysets\n" "	Show the configured SoftKeySets.\n";

#define CLI_COMMAND "sccp", "show", "softkeyssets"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_show_softkeysets, sccp_show_softkeysets, "Show configured SoftKeySets", show_softkeysets_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND

/* -----------------------------------------------------------------------------------------------------MESSAGE DEVICES- */

/*!
 * \brief Message Devices
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- devices
 * 	  - see sccp_dev_displaynotify()
 * 	  - see sccp_dev_starttone()
 */
static int sccp_message_devices(int fd, int argc, char *argv[])
{
	sccp_device_t *d;
	int timeout = 10;
	boolean_t beep = FALSE;

	if (argc < 4)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	if (argc > 4) {
		if (!strcmp(argv[4], "beep")) {
			beep = TRUE;
			if (sscanf(argv[5], "%d", &timeout) != 1) {
				timeout = 10;
			}
		}
		if (sscanf(argv[4], "%d", &timeout) != 1) {
			timeout = 10;
		}
	}

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, FALSE, beep);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return RESULT_SUCCESS;
}

static char message_devices_usage[] = "Usage: sccp message devices <message text> [beep] [timeout]\n" "       Send a message to all SCCP Devices + phone beep + timeout.\n";

#define CLI_COMMAND "sccp", "message", "devices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_message_devices, sccp_message_devices, "Send a message to all SCCP Devices", message_devices_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND

/* -----------------------------------------------------------------------------------------------------MESSAGE DEVICE- */

/*!
 * \brief Message Device
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \todo TO BE IMPLEMENTED: sccp message device
 */
static int sccp_message_device(int fd, int argc, char *argv[])
{
	sccp_device_t *d;

	int timeout = 10;

	boolean_t beep = FALSE;

	if (argc < 4)
		return RESULT_SHOWUSAGE;
	if (sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;
	if (argc > 5) {
		if (!strcmp(argv[5], "beep")) {
			beep = TRUE;
			if (sscanf(argv[6], "%d", &timeout) != 1) {
				timeout = 10;
			}
		}
		if (sscanf(argv[5], "%d", &timeout) != 1) {
			timeout = 10;
		}
	}
	if ((d = sccp_device_find_byid(argv[3], FALSE))) {
		sccp_dev_set_message(d, argv[4], timeout, FALSE, beep);

		return RESULT_SUCCESS;
	} else {
		ast_cli(fd, "Device not found!\n");
		return RESULT_FAILURE;
	}

}

static char message_device_usage[] = "Usage: sccp message device <deviceId> <message text> [beep] [timeout]\n" "       Send a message to an SCCP Device + phone beep + timeout.\n";

#define CLI_COMMAND "sccp", "message", "device"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_message_device, sccp_message_device, "Send a message to SCCP Device", message_device_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND

/* ------------------------------------------------------------------------------------------------------SYSTEM MESSAGE- */

/*!
 * \brief System Message
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_system_message(int fd, int argc, char *argv[])
{
	int res;
	sccp_device_t *d;
	int timeout = 0;
	boolean_t beep = FALSE;

	if (argc == 3) {
		SCCP_RWLIST_RDLOCK(&GLOB(devices));
		SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
			sccp_dev_clear_message(d, TRUE);
		}
		SCCP_RWLIST_UNLOCK(&GLOB(devices));
		ast_cli(fd, "Message Cleared\n");
		return RESULT_SUCCESS;
	}

	if (argc < 4 || argc > 6)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	res = PBX(feature_addToDatabase) ("SCCP/message", "text", argv[3]);
	if (!res) {
		ast_cli(fd, "Failed to store the SCCP system message text\n");
	} else {
		sccp_log(DEBUGCAT_CLI) (VERBOSE_PREFIX_3 "SCCP system message text stored successfully\n");
	}

	if (argc > 4) {
		if (!strcmp(argv[4], "beep")) {
			beep = TRUE;
			if (sscanf(argv[5], "%d", &timeout) != 1)
				timeout = 10;
		}
		if (sscanf(argv[4], "%d", &timeout) != 1)
			timeout = 10;
	} else {
		timeout = 0;
	}
	if (!res) {
		ast_cli(fd, "Failed to store the SCCP system message timeout\n");
	} else {
		sccp_log(DEBUGCAT_CLI) (VERBOSE_PREFIX_3 "SCCP system message timeout stored successfully\n");
	}

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, TRUE, beep);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return RESULT_SUCCESS;
}

static char system_message_usage[] = "Usage: sccp system message <message text> [beep] [timeout]\n" "       The default optional timeout is 0 (forever)\n" "       Example: sccp system message \"The boss is gone. Let's have some fun!\"  10\n";

#define CLI_COMMAND "sccp", "system", "message"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_system_message, sccp_system_message, "Send a system wide message to all SCCP Devices", system_message_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND

/* -----------------------------------------------------------------------------------------------------DND DEVICE- */

/*!
 * \brief Message Device
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- device
 * 	  - see sccp_sk_dnd()
 */
static int sccp_dnd_device(int fd, int argc, char *argv[])
{
	sccp_device_t *d = NULL;

	if (argc < 3)
		return RESULT_SHOWUSAGE;

	d = sccp_device_find_byid(argv[3], TRUE);
	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[3]);
		return RESULT_SUCCESS;
	}
	sccp_device_lock(d);

	sccp_sk_dnd(d, NULL, 0, NULL);

	sccp_device_unlock(d);

	return RESULT_SUCCESS;
}

static char dnd_device_usage[] = "Usage: sccp dnd <deviceId>\n" "       Send a dnd to an SCCP Device.\n";

#define CLI_COMMAND "sccp", "dnd", "device"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_dnd_device, sccp_dnd_device, "Send a dnd to SCCP Device", dnd_device_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------REMOVE_LINE_FROM_DEVICE- */

/*!
 * \brief Remove Line From Device
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \todo TO BE IMPLEMENTED: sccp message device
 */
static int sccp_remove_line_from_device(int fd, int argc, char *argv[])
{
	pbx_cli(fd, "Command has not been fully implemented yet!\n");
	return RESULT_FAILURE;
}

static char remove_line_from_device_usage[] = "Usage: sccp remove line <deviceID> <lineID>\n" "       Remove a line from device.\n";

#define CLI_COMMAND "sccp", "remove", "line"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER, SCCP_CLI_LINE_COMPLETER
CLI_ENTRY(cli_remove_line_from_device, sccp_remove_line_from_device, "Remove a line from device", remove_line_from_device_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* -------------------------------------------------------------------------------------------------ADD_LINE_TO_DEVICE- */

/*!
 * \brief Add Line To Device
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_add_line_to_device(int fd, int argc, char *argv[])
{
	sccp_device_t *d;
	sccp_line_t *l;

	if (argc < 5)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[4]))
		return RESULT_SHOWUSAGE;

	d = sccp_device_find_byid(argv[3], FALSE);
	if (!d) {
		pbx_log(LOG_ERROR, "Error: Device %s not found", argv[3]);
		return RESULT_FAILURE;
	}

	l = sccp_line_find_byname(argv[4]);
	if (!l) {
		pbx_log(LOG_ERROR, "Error: Line %s not found", argv[4]);
		return RESULT_FAILURE;
	}
#ifdef CS_DYNAMIC_CONFIG
	sccp_config_addButton(d, -1, LINE, l->name, NULL, NULL);
#else
	sccp_config_addLine(d, l->name, 0, 0);
#endif

	pbx_cli(fd, "Line %s has been added to device %s\n", l->name, d->id);
	return RESULT_SUCCESS;
}

static char add_line_to_device_usage[] = "Usage: sccp add line <deviceID> <lineID>\n" "       Add a line to a device.\n";

#define CLI_COMMAND "sccp", "add", "line"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER, SCCP_CLI_LINE_COMPLETER
CLI_ENTRY(cli_add_line_to_device, sccp_add_line_to_device, "Add a line to a device", add_line_to_device_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* ------------------------------------------------------------------------------------------------------------DO DEBUG- */

/*!
 * \brief Do Debug
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_do_debug(int fd, int argc, char *argv[])
{
	uint32_t new_debug = GLOB(debug);

	if (argc > 2) {
		new_debug = sccp_parse_debugline(argv, 2, argc, new_debug);
	}

	char *debugcategories = sccp_get_debugcategories(new_debug);

	if (argc > 2)
		pbx_cli(fd, "SCCP new debug status: (%d -> %d) %s\n", GLOB(debug), new_debug, debugcategories);
	else
		pbx_cli(fd, "SCCP debug status: (%d) %s\n", GLOB(debug), debugcategories);
	sccp_free(debugcategories);

	GLOB(debug) = new_debug;
	return RESULT_SUCCESS;
}

static char do_debug_usage[] = "Usage: SCCP debug [no] <level or categories>\n" "       Where categories is one or more (separated by commas) of:\n" "       core, sccp, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey,\n" "       indicate, pbx, socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime,\n" "       lock, newcode, high\n";

#define CLI_COMMAND "sccp", "debug"
#define CLI_COMPLETE SCCP_CLI_DEBUG_COMPLETER
CLI_ENTRY(cli_do_debug, sccp_do_debug, "Set SCCP Debugging Types", do_debug_usage, TRUE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* ------------------------------------------------------------------------------------------------------------NO DEBUG- */

/*!
 * \brief No Debug
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_no_debug(int fd, int argc, char *argv[])
{
	if (argc < 3)
		return RESULT_SHOWUSAGE;

	GLOB(debug) = 0;
	pbx_cli(fd, "SCCP Debugging Disabled\n");
	return RESULT_SUCCESS;
}

static char no_debug_usage[] = "Usage: SCCP no debug\n" "       Disables dumping of SCCP packets for debugging purposes\n";

#define CLI_COMMAND "sccp", "no", "debug"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_no_debug, sccp_no_debug, "Set SCCP Debugging Types", no_debug_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------------------------RELOAD- */

/*!
 * \brief Do Reload
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- globals
 *
 * \note To find out more about the reload function see \ref sccp_config_reload
 */
static int sccp_cli_reload(int fd, int argc, char *argv[])
{
#ifdef CS_DYNAMIC_CONFIG
	sccp_readingtype_t readingtype;
	int returnval = RESULT_SUCCESS;

	if (argc < 2 || argc > 3)
		return RESULT_SHOWUSAGE;

	pbx_mutex_lock(&GLOB(lock));
	if (GLOB(reload_in_progress) == TRUE) {
		pbx_cli(fd, "SCCP reloading already in progress.\n");
		pbx_mutex_unlock(&GLOB(lock));
		return RESULT_FAILURE;
	}

	if (argc > 2) {
		pbx_cli(fd, "Using config file '%s'\n", argv[2]);
		GLOB(config_file_name) = strdupa(argv[2]);
	}

	struct ast_config *cfg = sccp_config_getConfig();

	if (cfg) {								// no errors
		pbx_cli(fd, "SCCP reloading configuration.\n");
		readingtype = SCCP_CONFIG_READRELOAD;
		GLOB(reload_in_progress) = TRUE;
		pbx_mutex_unlock(&GLOB(lock));
		if (!sccp_config_general(readingtype)) {
			pbx_cli(fd, "Unable to reload configuration.\n");
			GLOB(reload_in_progress) = FALSE;
			pbx_mutex_unlock(&GLOB(lock));
			return RESULT_FAILURE;
		}
		sccp_config_readDevicesLines(readingtype);
		pbx_mutex_lock(&GLOB(lock));
		GLOB(reload_in_progress) = FALSE;
		returnval = RESULT_SUCCESS;
	} else {
		pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
		if (CONFIG_STATUS_FILEMISSING == cfg) {
			pbx_cli(fd, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
		} else if (CONFIG_STATUS_FILEUNCHANGED == cfg) {
			pbx_cli(fd, "Config file '%s' has not changed, aborting reload.\n", GLOB(config_file_name));
		} else if (CONFIG_STATUS_FILEINVALID == cfg) {
			pbx_cli(fd, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
		} else if (CONFIG_STATUS_FILEOLD == cfg) {
			pbx_cli(fd, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
		} else if (CONFIG_STATUS_FILE_NOT_SCCP == cfg) {
			pbx_cli(fd, "\n\n --> You are using an configuration file is not following the sccp format, please check '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
		}
		returnval = RESULT_FAILURE;
	}
	pbx_mutex_unlock(&GLOB(lock));
	return returnval;
#else
	pbx_cli(fd, "SCCP configuration reload not implemented yet! use unload and load.\n");
	return RESULT_SUCCESS;
#endif
}

static char reload_usage[] = "Usage: SCCP reload [filename]\n" "       Reloads SCCP configuration from sccp.conf or optional [filename]\n" "       (It will send a reset to all device which have changed (when they have an active channel reset will be postponed until device goes onhook))\n";

#define CLI_COMMAND "sccp", "reload"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_reload, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/*
#define CLI_COMMAND "sccp", "config", "reload"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_config_reload, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
*/

/*!
 * \brief Generare sccp.conf
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_cli_config_generate(int fd, int argc, char *argv[])
{
#ifdef CS_EXPERIMENTAL
	int returnval = RESULT_SUCCESS;
	char *config_file = "sccp.conf.test";

	if (argc < 2 || argc > 3)
		return RESULT_SHOWUSAGE;

	pbx_cli(fd, "SCCP: Creating config file.\n");

	if (argc > 3) {
		pbx_cli(fd, "Using config file '%s'\n", argv[3]);
		config_file = strdupa(argv[3]);
	}
	if (sccp_config_generate(config_file, 0)) {
		pbx_cli(fd, "SCCP generated. saving '%s'...\n", config_file);
	} else {
		pbx_cli(fd, "SCCP generation failed.\n");
		returnval = RESULT_FAILURE;
	}

	return returnval;
#else
	pbx_cli(fd, "SCCP config generate not implemented yet! use unload and load.\n");
	return RESULT_FAILURE;
#endif
}

static char config_generate_usage[] = "Usage: SCCP config generate [filename]\n" "       Generates a new sccp.conf if none exists. Either creating sccp.conf or [filename] if specified\n";

#define CLI_COMMAND "sccp", "config", "generate"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_config_generate, sccp_cli_config_generate, "Generate a SCCP configuration file", config_generate_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* -------------------------------------------------------------------------------------------------------SHOW VERSION- */

/*!
 * \brief Show Version
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_show_version(int fd, int argc, char *argv[])
{
	pbx_cli(fd, "Skinny Client Control Protocol (SCCP). Release: %s %s - %s (built by '%s' on '%s')\n", SCCP_VERSION, SCCP_BRANCH, SCCP_REVISION, BUILD_USER, BUILD_DATE);
	return RESULT_SUCCESS;
}

static char show_version_usage[] = "Usage: SCCP show version\n" "       Show SCCP version details\n";

#define CLI_COMMAND "sccp", "show", "version"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_show_version, sccp_show_version, "Show SCCP version details", show_version_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND

/* -------------------------------------------------------------------------------------------------------RESET_RESTART- */

/*!
 * \brief Reset/Restart
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- devices in sccp_device_find_byid()
 * 	- device
 */
static int sccp_reset_restart(int fd, int argc, char *argv[])
{
	sccp_device_t *d;
	boolean_t restart = TRUE;

	if (argc < 3 || argc > 4)
		return RESULT_SHOWUSAGE;

	if (!strcasecmp(argv[1], "reset")) {
		if (argc == 4) {
			if (strcasecmp(argv[3], "restart"))
				return RESULT_SHOWUSAGE;
			restart = TRUE;
		} else
			restart = FALSE;
	} else if (argc != 3)
		return RESULT_SHOWUSAGE;

	pbx_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], FALSE);

	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	sccp_device_lock(d);
	if (!d->session) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		sccp_device_unlock(d);
		return RESULT_FAILURE;
	}

	/* sccp_device_clean will check active channels */
	/* \todo implement a check for active channels before sending reset */
//      if (d->channelCount > 0) {
	//pbx_cli(fd, "%s: unable to %s device with active channels. Hangup first\n", argv[2], (!strcasecmp(argv[1], "reset")) ? "reset" : "restart");
	//return RESULT_SUCCESS;
//      }
	sccp_device_unlock(d);

	if (!restart)
		sccp_device_sendReset(d, SKINNY_DEVICE_RESET);
	else
		sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
	pthread_cancel(d->session->session_thread);

	return RESULT_SUCCESS;
}

/* --------------------------------------------------------------------------------------------------------------RESET- */
static char reset_usage[] = "Usage: SCCP reset\n" "       sccp reset <deviceId> [restart]\n";

#define CLI_COMMAND "sccp", "reset"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_reset, sccp_reset_restart, "Show SCCP version details", reset_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* -------------------------------------------------------------------------------------------------------------RESTART- */
static char restart_usage[] = "Usage: SCCP restart\n" "       sccp restart <deviceId>\n";

#define CLI_COMMAND "sccp", "restart"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_restart, sccp_reset_restart, "Restart an SCCP device", restart_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* ----------------------------------------------------------------------------------------------------------UNREGISTER- */

/*!
 * \brief Unregister
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- device
 */
static int sccp_unregister(int fd, int argc, char *argv[])
{
	sccp_moo_t *r;
	sccp_device_t *d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	pbx_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], TRUE);
	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	sccp_device_lock(d);

	if (!d->session) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		sccp_device_unlock(d);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);

	sccp_device_unlock(d);

	REQ(r, RegisterRejectMessage);
	strncpy(r->msg.RegisterRejectMessage.text, "Unregister user request", StationMaxDisplayTextSize);
	sccp_dev_send(d, r);

	return RESULT_SUCCESS;
}

static char unregister_usage[] = "Usage: SCCP unregister <deviceId>\n" "       Unregister an SCCP device\n";

#define CLI_COMMAND "sccp", "unregister"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_unregister, sccp_unregister, "Unregister an SCCP device", unregister_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------------------------START CALL- */

/*!
 * \brief Start Call
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_start_call(int fd, int argc, char *argv[])
{
	sccp_device_t *d;

	sccp_line_t *line = NULL;

	if (argc < 3) {
		pbx_cli(fd, "argc is less then 2: %d\n", argc);
		return RESULT_SHOWUSAGE;
	}
	if (pbx_strlen_zero(argv[2])) {
		pbx_cli(fd, "string length of argv[2] is zero\n");
		return RESULT_SHOWUSAGE;
	}
	d = sccp_device_find_byid(argv[2], FALSE);
	if (!d) {
		pbx_cli(fd, "Can't find settings for device %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	if (d && d->defaultLineInstance > 0) {
		line = sccp_line_find_byid(d, d->defaultLineInstance);
	} else {
		line = sccp_dev_get_activeline(d);
	}
	if (!line) {
		pbx_cli(fd, "Can't find line for device %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	pbx_cli(fd, "Starting Call for Device: %s\n", argv[2]);
	sccp_channel_newcall(line, d, argv[3], SKINNY_CALLTYPE_OUTBOUND);
	return RESULT_SUCCESS;
}

static char start_call_usage[] = "Usage: sccp call <deviceId> <phone_number>\n" "Call number <number> using device <deviceId>\nIf number is ommitted, device will go off-Hook.\n";

#define CLI_COMMAND "sccp", "call"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
CLI_ENTRY(cli_start_call, sccp_start_call, "Call Number via Device", start_call_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------------------------SET HOLD- */

/*!
 * \brief Set Channel on Hold
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \lock
 * 	- channel
 */
static int sccp_set_hold(int fd, int argc, char *argv[])
{
	sccp_channel_t *c = NULL;

	if (argc < 5)
		return RESULT_SHOWUSAGE;
	if (pbx_strlen_zero(argv[3]) || pbx_strlen_zero(argv[4]))
		return RESULT_SHOWUSAGE;

	if (!strncasecmp("SCCP/", argv[3], 5)) {
		int line, channel;

		sscanf(argv[3], "SCCP/%d-%d", &line, &channel);
		c = sccp_channel_find_byid_locked(channel);
	} else {
		c = sccp_channel_find_byid_locked(atoi(argv[3]));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[3]);
		return RESULT_FAILURE;
	}
	if (!strcmp("on", argv[4])) {						/* check to see if enable hold */
		pbx_cli(fd, "PLACING CHANNEL %s ON HOLD\n", argv[3]);
		sccp_channel_hold_locked(c);
		sccp_channel_unlock(c);
	} else if (!strcmp("off", argv[4])) {					/* check to see if disable hold */
		pbx_cli(fd, "PLACING CHANNEL %s OFF HOLD\n", argv[3]);
		sccp_channel_resume_locked(sccp_channel_getDevice(c), c, FALSE);
		sccp_channel_unlock(c);
	} else
		/* wrong parameter value */
		return RESULT_SHOWUSAGE;
	return RESULT_SUCCESS;
}

static char set_hold_usage[] = "Usage: sccp set hold <channelId> <on/off>\n" "Set a channel to hold/unhold\n";

#define CLI_COMMAND "sccp", "set", "hold"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER
CLI_ENTRY(cli_set_hold, sccp_set_hold, "Set channel to hold/unhold", set_hold_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------------------------REMOTE ANSWER- */

/*!
 * \brief Answer a Remote Channel
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \lock
 * 	- channel
 */
static int sccp_remote_answer(int fd, int argc, char *argv[])
{
	sccp_channel_t *c = NULL;

	if (argc < 3)
		return RESULT_SHOWUSAGE;
	if (pbx_strlen_zero(argv[2]))
		return RESULT_SHOWUSAGE;

	if (!strncasecmp("SCCP/", argv[2], 5)) {
		int line, channel;

		sscanf(argv[2], "SCCP/%d-%d", &line, &channel);
		c = sccp_channel_find_byid_locked(channel);
	} else {
		c = sccp_channel_find_byid_locked(atoi(argv[2]));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	pbx_cli(fd, "ANSWERING CHANNEL %s \n", argv[2]);
	sccp_channel_answer_locked(sccp_channel_getDevice(c), c);
	sccp_channel_unlock(c);
	if (c->owner) {
		pbx_queue_control(c->owner, AST_CONTROL_ANSWER);
	}
	return RESULT_SUCCESS;
}

static char remote_answer_usage[] = "Usage: sccp answer <channelId>\n" "Answer a ringing/incoming channel\n";

#define CLI_COMMAND "sccp", "answer"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER
CLI_ENTRY(cli_remote_answer, sccp_remote_answer, "Answer a ringing/incoming channel", remote_answer_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --------------------------------------------------------------------------------------------------------------END CALL- */

/*!
 * \brief End a Call (on a Channel)
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 *
 * \lock
 * 	- channel
 */
static int sccp_end_call(int fd, int argc, char *argv[])
{
	sccp_channel_t *c = NULL;

	if (argc < 3)
		return RESULT_SHOWUSAGE;
	if (pbx_strlen_zero(argv[2]))
		return RESULT_SHOWUSAGE;

	if (!strncasecmp("SCCP/", argv[2], 5)) {
		int line, channel;

		sscanf(argv[2], "SCCP/%d-%d", &line, &channel);
		c = sccp_channel_find_byid_locked(channel);
	} else {
		c = sccp_channel_find_byid_locked(atoi(argv[2]));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	pbx_cli(fd, "ENDING CALL ON CHANNEL %s \n", argv[2]);
	sccp_channel_endcall_locked(c);
	sccp_channel_unlock(c);
	return RESULT_SUCCESS;
}

static char end_call_usage[] = "Usage: sccp onhook <channelId>\n" "Hangup a channel\n";

#define CLI_COMMAND "sccp", "onhook"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER
CLI_ENTRY(cli_end_call, sccp_end_call, "Hangup a channel", end_call_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE

/* --- Register Cli Entries-------------------------------------------------------------------------------------------- */
#if ASTERISK_VERSION_NUMBER >= 10600

/*!
 * \brief Asterisk Cli Entry
 *
 * structure for cli functions including short description.
 *
 * \return Result as struct
 * \todo add short description
 */
static struct pbx_cli_entry cli_entries[] = {
	AST_CLI_DEFINE(cli_show_globals, "Show SCCP global settings."),
	AST_CLI_DEFINE(cli_show_devices, "Show all SCCP Devices."),
	AST_CLI_DEFINE(cli_show_device, "Show an SCCP Device"),
	AST_CLI_DEFINE(cli_show_lines, "Show All SCCP Lines."),
	AST_CLI_DEFINE(cli_show_line, "Show an SCCP Line."),
	AST_CLI_DEFINE(cli_show_channels, "Show all SCCP channels."),
	AST_CLI_DEFINE(cli_show_version, "SCCP show version."),
	AST_CLI_DEFINE(cli_show_mwi_subscriptions, "Show all mwi subscriptions"),
	AST_CLI_DEFINE(cli_show_softkeysets, "Show all mwi configured SoftKeySets"),
	AST_CLI_DEFINE(cli_unregister, "Unregister an SCCP device"),
	AST_CLI_DEFINE(cli_system_message, "Set the SCCP system message."),
	AST_CLI_DEFINE(cli_message_devices, "Send a message to all SCCP Devices."),
	AST_CLI_DEFINE(cli_message_device, "Send a message to an SCCP Device."),
	AST_CLI_DEFINE(cli_remove_line_from_device, "Remove a line from a device."),
	AST_CLI_DEFINE(cli_add_line_to_device, "Add a line to a device."),
	AST_CLI_DEFINE(cli_show_sessions, "Show All SCCP Sessions."),
	AST_CLI_DEFINE(cli_dnd_device, "Set DND on a device"),
	AST_CLI_DEFINE(cli_do_debug, "Enable SCCP debugging."),
	AST_CLI_DEFINE(cli_no_debug, "Disable SCCP debugging."),
	AST_CLI_DEFINE(cli_config_generate, "SCCP generate config file."),
//      AST_CLI_DEFINE(cli_config_reload, "SCCP module reload."),
	AST_CLI_DEFINE(cli_reload, "SCCP module reload."),
	AST_CLI_DEFINE(cli_restart, "Restart an SCCP device"),
	AST_CLI_DEFINE(cli_reset, "Reset an SCCP Device"),
	AST_CLI_DEFINE(cli_start_call, "Start a Call."),
	AST_CLI_DEFINE(cli_end_call, "End a Call."),
	AST_CLI_DEFINE(cli_set_hold, "Place call on hold."),
	AST_CLI_DEFINE(cli_remote_answer, "Remotely answer a call.")
};
#endif

/*!
 * register CLI functions from asterisk
 */
void sccp_register_cli(void)
{

#if ASTERISK_VERSION_NUMBER >= 10600
	/* register all CLI functions */
	pbx_cli_register_multiple(cli_entries, sizeof(cli_entries) / sizeof(struct pbx_cli_entry));
#else
	pbx_cli_register(&cli_show_channels);
	pbx_cli_register(&cli_show_devices);
	pbx_cli_register(&cli_show_device);
	pbx_cli_register(&cli_show_lines);
	pbx_cli_register(&cli_show_line);
	pbx_cli_register(&cli_show_sessions);
	pbx_cli_register(&cli_show_version);
	pbx_cli_register(&cli_show_softkeysets);
	pbx_cli_register(&cli_config_generate);
//      pbx_cli_register(&cli_config_reload);
	pbx_cli_register(&cli_reload);
	pbx_cli_register(&cli_restart);
	pbx_cli_register(&cli_reset);
	pbx_cli_register(&cli_unregister);
	pbx_cli_register(&cli_do_debug);
	pbx_cli_register(&cli_no_debug);
	pbx_cli_register(&cli_system_message);
	pbx_cli_register(&cli_show_globals);
	pbx_cli_register(&cli_message_devices);
	pbx_cli_register(&cli_message_device);
	pbx_cli_register(&cli_dnd_device);
	pbx_cli_register(&cli_remove_line_from_device);
	pbx_cli_register(&cli_add_line_to_device);
	pbx_cli_register(&cli_show_mwi_subscriptions);
	pbx_cli_register(&cli_set_hold);
	pbx_cli_register(&cli_remote_answer);
	pbx_cli_register(&cli_start_call);
	pbx_cli_register(&cli_end_call);
#endif
#if ASTERISK_VERSION_NUMBER < 10600
#    define _MAN_COM_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND
#    define _MAN_REP_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#else
#    define _MAN_COM_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND
#    define _MAN_REP_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING
#endif
	pbx_manager_register2("SCCPShowGlobals", _MAN_REP_FLAGS, manager_show_globals, "show globals setting", ami_globals_usage);
	pbx_manager_register2("SCCPShowDevices", _MAN_REP_FLAGS, manager_show_devices, "show devices", ami_devices_usage);
	pbx_manager_register2("SCCPShowDevice", _MAN_REP_FLAGS, manager_show_device, "show device settings", ami_device_usage);
	pbx_manager_register2("SCCPShowLines", _MAN_REP_FLAGS, manager_show_lines, "show lines", ami_lines_usage);
	pbx_manager_register2("SCCPShowLine", _MAN_REP_FLAGS, manager_show_line, "show line", ami_line_usage);
	pbx_manager_register2("SCCPShowChannels", _MAN_REP_FLAGS, manager_show_channels, "show channels", ami_channels_usage);
	pbx_manager_register2("SCCPShowSessions", _MAN_REP_FLAGS, manager_show_sessions, "show sessions", ami_sessions_usage);
	pbx_manager_register2("SCCPShowMWISubscriptions", _MAN_REP_FLAGS, manager_show_mwi_subscriptions, "show sessions", ami_mwi_subscriptions_usage);
}

/*!
 * unregister CLI functions from asterisk
 */
void sccp_unregister_cli(void)
{

#if ASTERISK_VERSION_NUMBER >= 10600
	/* unregister CLI functions */
	pbx_cli_unregister_multiple(cli_entries, sizeof(cli_entries) / sizeof(struct pbx_cli_entry));
#else
	pbx_cli_unregister(&cli_show_channels);
	pbx_cli_unregister(&cli_show_devices);
	pbx_cli_unregister(&cli_show_device);
	pbx_cli_unregister(&cli_show_lines);
	pbx_cli_unregister(&cli_show_line);
	pbx_cli_unregister(&cli_show_sessions);
	pbx_cli_unregister(&cli_show_version);
	pbx_cli_unregister(&cli_show_softkeysets);
	pbx_cli_unregister(&cli_config_generate);
//      pbx_cli_unregister(&cli_config_reload);
	pbx_cli_unregister(&cli_reload);
	pbx_cli_unregister(&cli_restart);
	pbx_cli_unregister(&cli_reset);
	pbx_cli_unregister(&cli_unregister);
	pbx_cli_unregister(&cli_do_debug);
	pbx_cli_unregister(&cli_no_debug);
	pbx_cli_unregister(&cli_system_message);
	pbx_cli_unregister(&cli_show_globals);
	pbx_cli_unregister(&cli_message_devices);
	pbx_cli_unregister(&cli_message_device);
	pbx_cli_unregister(&cli_dnd_device);
	pbx_cli_unregister(&cli_remove_line_from_device);
	pbx_cli_unregister(&cli_add_line_to_device);
	pbx_cli_unregister(&cli_show_mwi_subscriptions);
	pbx_cli_unregister(&cli_set_hold);
	pbx_cli_unregister(&cli_remote_answer);
	pbx_cli_unregister(&cli_start_call);
	pbx_cli_unregister(&cli_end_call);
#endif
	pbx_manager_unregister("SCCPShowGlobals");
	pbx_manager_unregister("SCCPShowDevice");
	pbx_manager_unregister("SCCPShowDevices");
	pbx_manager_unregister("SCCPShowLines");
	pbx_manager_unregister("SCCPShowLine");
	pbx_manager_unregister("SCCPShowChannels");
	pbx_manager_unregister("SCCPShowSessions");
	pbx_manager_unregister("SCCPShowMWISubscriptions");
}
