
/*!
 * \file        sccp_cli.c
 * \brief       SCCP CLI Class
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

/*!
 * \remarks     Purpose:        SCCP CLI
 *              When to use:    Only methods directly related to the asterisk cli interface should be stored in this source file.
 *              Relationships:  Calls ???
 *
 * how to use the cli macro's
 * /code
 * static char cli_message_device_usage[] = "Usage: sccp message device <deviceId> <message text> [beep] [timeout]\n" "Send a message to an SCCP Device + phone beep + timeout.\n";
 * static char ami_message_device_usage[] = "Usage: SCCPMessageDevices\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: DeviceId, MessageText, Beep, Timeout\n";
 * #define CLI_COMMAND "sccp", "message", "device" 					// defines the cli command line before parameters
 * #define AMI_COMMAND "SCCPMessageDevice" 						// defines the ami command line before parameters
 * #define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER					// defines on or more cli tab completion helpers (in order)
 * #define CLI_AMI_PARAMS "DeviceId" "MessageText", "Beep", "Timeout"			// defines the ami parameter conversion mapping to argc/argv, empty string if not defined
 * CLI_AMI_ENTRY(message_device, sccp_message_device, "Send a message to SCCP Device", cli_message_device_usage, FALSE)
 * 											// the actual macro call which will generate an cli function and an ami function to be called. CLI_AMI_ENTRY elements:
 *											//  - functionname (will be expanded to manager_functionname and cli_functionname)
 *											//  - function to be called upon execution
 *											//  - description
 *											//  - usage
 *											//  - completer repeats indefinitly (multi calls to the completer, for example for 'sccp debug')
 * #undef CLI_AMI_PARAMS
 * #undef AMI_COMMAND
 * #undef CLI_COMPLETE
 * #undef CLI_COMMAND									// cleanup / undefine everything before the next call to CLI_AMI_ENTRY
 * #endif
 * /endcode
 * 
 * Inside the function that which is called on execution:
 *  - If s!=NULL we know it is an AMI calls, if m!=NULL it is a CLI call.
 *  - we need to add local_total which get's set to the number of lines returned (for ami calls).
 *  - We need to return RESULT_SUCCESS (for cli calls) at the end. If we set CLI_AMI_ERROR, we will exit the function immediately and return RESULT_FAILURE. We need to make sure that all references are released before sending CLI_AMI_ERROR.
 *  .
 */ 
#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#include <asterisk/cli.h>


typedef enum sccp_cli_completer {
	SCCP_CLI_NULL_COMPLETER,
	SCCP_CLI_DEVICE_COMPLETER,
	SCCP_CLI_CONNECTED_DEVICE_COMPLETER,
	SCCP_CLI_LINE_COMPLETER,
	SCCP_CLI_CONNECTED_LINE_COMPLETER,
	SCCP_CLI_CHANNEL_COMPLETER,
	SCCP_CLI_CONFERENCE_COMPLETER,
	SCCP_CLI_DEBUG_COMPLETER,
	SCCP_CLI_SET_COMPLETER,
} sccp_cli_completer_t;

static char *sccp_exec_completer(sccp_cli_completer_t completer, OLDCONST char *line, OLDCONST char *word, int pos, int state);

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
     *      - devices
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

static char *sccp_complete_connected_device(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_device_t *d;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strncasecmp(word, d->id, wordlen) && d->registrationState != SKINNY_DEVICE_RS_NONE && ++which > state) {
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
 *      - lines
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

static char *sccp_complete_connected_line(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strncasecmp(word, l->name, wordlen) && SCCP_LIST_GETSIZE(&l->devices) > 0 && ++which > state) {
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
 *      - lines
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
	int wordlen = strlen(word);
	int which = 0;
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
		if (!strncasecmp(word, sccp_debug_categories[i].key, wordlen)) {
			if (++which > state)
				return strdup(sccp_debug_categories[i].key);
		}
	}
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
static char *sccp_complete_set(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	uint8_t i;
	sccp_device_t *d;
	sccp_channel_t *c;
	sccp_line_t *l;
	
	int wordlen = strlen(word), which = 0;
	char tmpname[80];
	char *ret = NULL;
	
	char *types[] = { "device", "channel", "line"};
	
	char *properties_channel[] = { "hold"};
	char *properties_device[] = { "ringtone", "backgroundImage"};
	
	char *values_hold[] = { "on", "off"};
	
	
	switch (pos) {
		case 2:		// type
			for (i = 0; i < ARRAY_LEN(types); i++) {
				if (!strncasecmp(word, types[i], wordlen) && ++which > state) {
					return strdup(types[i]);
				}
			}
			break;
		case 3:		// device / channel / line
		  
			if( strstr(line, "device") != NULL ){
				SCCP_RWLIST_RDLOCK(&GLOB(devices));
				SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
					if (!strncasecmp(word, d->id, wordlen) && ++which > state) {
						ret = strdup(d->id);
						break;
					}
				}
				SCCP_RWLIST_UNLOCK(&GLOB(devices));
				
			} else if( strstr(line, "channel")  != NULL  ){
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
			}
			break;
		case 4:		// properties
		  
			if( strstr(line, "device") != NULL  ){
				for (i = 0; i < ARRAY_LEN(properties_device); i++) {
					if (!strncasecmp(word, properties_device[i], wordlen) && ++which > state) {
						return strdup(properties_device[i]);
					}
				}
				
			} else if( strstr(line, "channel") != NULL ){
				for (i = 0; i < ARRAY_LEN(properties_channel); i++) {
					if (!strncasecmp(word, properties_channel[i], wordlen) && ++which > state) {
						return strdup(properties_channel[i]);
					}
				}
			}
			break;
		case 5:		// values_hold
		  
			if( strstr(line, "channel") != NULL && strstr(line, "hold") != NULL ){
				for (i = 0; i < ARRAY_LEN(values_hold); i++) {
					if (!strncasecmp(word, values_hold[i], wordlen) && ++which > state) {
						return strdup(values_hold[i]);
					}
				}
			}
			break;
		default:
			break;
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
		case SCCP_CLI_CONNECTED_DEVICE_COMPLETER:
			return sccp_complete_connected_device(line, word, pos, state);
			break;
		case SCCP_CLI_LINE_COMPLETER:
			return sccp_complete_line(line, word, pos, state);
			break;
		case SCCP_CLI_CONNECTED_LINE_COMPLETER:
			return sccp_complete_connected_line(line, word, pos, state);
			break;
		case SCCP_CLI_CHANNEL_COMPLETER:
			return sccp_complete_channel(line, word, pos, state);
			break;
		case SCCP_CLI_CONFERENCE_COMPLETER:
#ifdef CS_SCCP_CONFERENCE
			return sccp_complete_conference(line, word, pos, state);
#endif
			break;
		case SCCP_CLI_DEBUG_COMPLETER:
			return sccp_complete_debug(line, word, pos, state);
			break;
		case SCCP_CLI_SET_COMPLETER:
			return sccp_complete_set(line, word, pos, state);
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
 *      - globals
 */
//static int sccp_show_globals(int fd, int argc, char *argv[])
static int sccp_show_globals(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	char pref_buf[256];
	struct ast_str *callgroup_buf = pbx_str_alloca(512);

#ifdef CS_SCCP_PICKUP
	struct ast_str *pickupgroup_buf = pbx_str_alloca(512);
#endif
	struct ast_str *ha_buf = pbx_str_alloca(512);
	char *debugcategories;
	int local_total = 0;
	const char *actionid = "";

	sccp_globals_lock(lock);

	sccp_multiple_codecs2str(pref_buf, sizeof(pref_buf) - 1, GLOB(global_preferences), ARRAY_LEN(GLOB(global_preferences)));
	debugcategories = sccp_get_debugcategories(GLOB(debug));
	sccp_print_ha(ha_buf, sizeof(ha_buf), GLOB(ha));

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver global settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_send_listack(s, m, argv[0], "start");
		CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", "SCCPGlobalSettings");
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		astman_append(s, "\r\n");
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
	CLI_AMI_OUTPUT_PARAM("Deny/Permit", CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(ha_buf));
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
	CLI_AMI_OUTPUT_BOOL("Pickup Mode Answer ", CLI_AMI_LIST_WIDTH, GLOB(directed_pickup_modeanswer));
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
	CLI_AMI_OUTPUT_BOOL("Trust phone ip (deprecated)", CLI_AMI_LIST_WIDTH, GLOB(trustphoneip));
	CLI_AMI_OUTPUT_PARAM("Early RTP", CLI_AMI_LIST_WIDTH, "%s (%s)", GLOB(earlyrtp) ? "Yes" : "No", GLOB(earlyrtp) ? sccp_indicate2str(GLOB(earlyrtp)) : "none");
	CLI_AMI_OUTPUT_PARAM("AutoAnswer ringtime", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_ring_time));
	CLI_AMI_OUTPUT_PARAM("AutoAnswer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_tone));
	CLI_AMI_OUTPUT_PARAM("RemoteHangup tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(remotehangup_tone));
	CLI_AMI_OUTPUT_PARAM("Transfer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(transfer_tone));
	CLI_AMI_OUTPUT_BOOL("Transfer on hangup", CLI_AMI_LIST_WIDTH, GLOB(transfer_on_hangup));
	CLI_AMI_OUTPUT_PARAM("Callwaiting tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(callwaiting_tone));
	CLI_AMI_OUTPUT_PARAM("Callwaiting interval", CLI_AMI_LIST_WIDTH, "%d", GLOB(callwaiting_interval));
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
	CLI_AMI_OUTPUT_PARAM("Hotline_Context", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline->line->context));
	CLI_AMI_OUTPUT_PARAM("Hotline_Exten", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline->exten));
	CLI_AMI_OUTPUT_PARAM("Threadpool Size", CLI_AMI_LIST_WIDTH, "%d/%d", sccp_threadpool_jobqueue_count(GLOB(general_threadpool)), sccp_threadpool_thread_count(GLOB(general_threadpool)));

	sccp_free(debugcategories);
	sccp_globals_unlock(lock);

	if (s) {
		*total = local_total;
		astman_append(s, "\r\n");
	}

	return RESULT_SUCCESS;
}

static char cli_globals_usage[] = "Usage: sccp show globals\n" "       Lists global settings for the SCCP subsystem.\n";
static char ami_globals_usage[] = "Usage: SCCPShowGlobals\n" "       Lists global settings for the SCCP subsystem.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "globals"
#define AMI_COMMAND "SCCPShowGlobals"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_globals, sccp_show_globals, "List defined SCCP global settings", cli_globals_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - devices
     */
    //static int sccp_show_devices(int fd, int argc, char *argv[])
static int sccp_show_devices(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	struct tm *timeinfo;
	char regtime[25];
	int local_total = 0;
	sccp_device_t *d;

	// table definition
#define CLI_AMI_TABLE_NAME Devices
#define CLI_AMI_TABLE_PER_ENTRY_NAME Device

#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_device_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(devices)
#define CLI_AMI_TABLE_LIST_ITER_VAR list_dev
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 																\
	if ((d = sccp_device_retain(list_dev))) {															\
		timeinfo = localtime(&d->registrationTime); 													\
		strftime(regtime, sizeof(regtime), "%c ", timeinfo);

#define CLI_AMI_TABLE_AFTER_ITERATION 																\
		sccp_device_release(d);																\
	}

	//#define CLI_AMI_TABLE_BEFORE_ITERATION timeinfo = localtime(&d->registrationTime); strftime(regtime, sizeof(regtime), "%c ", timeinfo);
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 																	\
		CLI_AMI_TABLE_FIELD(Name,		s,	40,	d->description)										\
		CLI_AMI_TABLE_FIELD(Address,		s,	20,	(d->session) ? pbx_inet_ntoa(d->session->sin.sin_addr) : "--")				\
		CLI_AMI_TABLE_FIELD(Mac,		s,	16,	d->id)											\
		CLI_AMI_TABLE_FIELD(RegState,		s,	10, 	registrationstate2str(d->registrationState))					\
		CLI_AMI_TABLE_FIELD(Token,		s,	5,	d->status.token ? ((d->status.token == SCCP_TOKEN_STATE_ACK) ? "Ack" : "Rej") : "None") \
		CLI_AMI_TABLE_FIELD(RegTime,		s, 	25, 	regtime)										\
		CLI_AMI_TABLE_FIELD(Act,		s,  	3, 	(d->active_channel) ? "Yes" : "No")							\
		CLI_AMI_TABLE_FIELD(Lines, 		d,  	5, 	d->configurationStatistic.numberOfLines)
#include "sccp_cli_table.h"

	// end of table definition
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_devices_usage[] = "Usage: sccp show devices\n" "       Lists defined SCCP devices.\n";
static char ami_devices_usage[] = "Usage: SCCPShowDevices\n" "Lists defined SCCP devices.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "devices"
#define AMI_COMMAND "SCCPShowDevices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_devices, sccp_show_devices, "List defined SCCP devices", cli_devices_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - device->buttonconfig is not always locked
     *
     * \lock
     *      - device
     *        - device->buttonconfig
     */
static int sccp_show_device(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d;
	sccp_line_t *l;
	char pref_buf[256];
	char cap_buf[512];
	struct ast_str *ha_buf = pbx_str_alloca(512);
	PBX_VARIABLE_TYPE *v = NULL;
	sccp_linedevices_t *linedevice = NULL;
	int local_total = 0;
	const char *actionid = "";

	const char *dev;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "DeviceName needs to be supplied %s\n", "");
	}
	dev = sccp_strdupa(argv[3]);
	if (pbx_strlen_zero(dev)) {
		pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "DeviceName needs to be supplied %s\n", "");
	}
	d = sccp_device_find_byid(dev, FALSE);

	if (!d) {
		pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
		CLI_AMI_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);
	}
	sccp_multiple_codecs2str(pref_buf, sizeof(pref_buf) - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
	sccp_multiple_codecs2str(cap_buf, sizeof(cap_buf) - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
	sccp_print_ha(ha_buf, sizeof(ha_buf), d->ha);

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver device settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_send_listack(s, m, argv[0], "start");
		CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", "SCCPShowDevice");
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		astman_append(s, "\r\n");
	}
	/* *INDENT-OFF* */
	CLI_AMI_OUTPUT_PARAM("MAC-Address",		CLI_AMI_LIST_WIDTH, "%s", d->id);
	CLI_AMI_OUTPUT_PARAM("Protocol Version",	CLI_AMI_LIST_WIDTH, "Supported '%d', In Use '%d'", d->protocolversion, d->inuseprotocolversion);
	CLI_AMI_OUTPUT_PARAM("Protocol In Use",		CLI_AMI_LIST_WIDTH, "%s Version %d", d->protocol ? d->protocol->name : "NONE", d->protocol ? d->protocol->version : 0);
	CLI_AMI_OUTPUT_PARAM("Tokenstate",		CLI_AMI_LIST_WIDTH, "%s", d->status.token ? ((d->status.token == SCCP_TOKEN_STATE_ACK) ? "Token acknowledged" : "Token rejected") : "no token requested");
	CLI_AMI_OUTPUT_PARAM("Keepalive",		CLI_AMI_LIST_WIDTH, "%d", d->keepalive);
	CLI_AMI_OUTPUT_PARAM("Registration state",	CLI_AMI_LIST_WIDTH, "%s(%d)", registrationstate2str(d->registrationState), d->registrationState);
	CLI_AMI_OUTPUT_PARAM("State",			CLI_AMI_LIST_WIDTH, "%s(%d)", devicestate2str(d->state), d->state);
	CLI_AMI_OUTPUT_PARAM("MWI light",		CLI_AMI_LIST_WIDTH, "%s(%d)", lampmode2str(d->mwilamp), d->mwilamp);
	CLI_AMI_OUTPUT_BOOL("MWI handset light", 	CLI_AMI_LIST_WIDTH, d->mwilight);
	CLI_AMI_OUTPUT_PARAM("Description",		CLI_AMI_LIST_WIDTH, "%s", d->description);
	CLI_AMI_OUTPUT_PARAM("Config Phone Type",	CLI_AMI_LIST_WIDTH, "%s", d->config_type);
	CLI_AMI_OUTPUT_PARAM("Skinny Phone Type",	CLI_AMI_LIST_WIDTH, "%s(%d)", devicetype2str(d->skinny_type), d->skinny_type);
	CLI_AMI_OUTPUT_YES_NO("Softkey support",	CLI_AMI_LIST_WIDTH, d->softkeysupport);
	CLI_AMI_OUTPUT_PARAM("Softkeyset",		CLI_AMI_LIST_WIDTH, "%s", d->softkeyDefinition);
	CLI_AMI_OUTPUT_YES_NO("BTemplate support",	CLI_AMI_LIST_WIDTH, d->buttonTemplate);
	CLI_AMI_OUTPUT_YES_NO("linesRegistered",	CLI_AMI_LIST_WIDTH, d->linesRegistered);
	CLI_AMI_OUTPUT_PARAM("Image Version",		CLI_AMI_LIST_WIDTH, "%s", d->imageversion);
	CLI_AMI_OUTPUT_PARAM("Timezone Offset",		CLI_AMI_LIST_WIDTH, "%d", d->tz_offset);
	CLI_AMI_OUTPUT_PARAM("Capabilities",		CLI_AMI_LIST_WIDTH, "%s", cap_buf);
	CLI_AMI_OUTPUT_PARAM("Codecs preference",	CLI_AMI_LIST_WIDTH, "%s", pref_buf);
	CLI_AMI_OUTPUT_PARAM("Audio TOS",		CLI_AMI_LIST_WIDTH, "%d", d->audio_tos);
	CLI_AMI_OUTPUT_PARAM("Audio COS",		CLI_AMI_LIST_WIDTH, "%d", d->audio_cos);
	CLI_AMI_OUTPUT_PARAM("Video TOS",		CLI_AMI_LIST_WIDTH, "%d", d->video_tos);
	CLI_AMI_OUTPUT_PARAM("Video COS",		CLI_AMI_LIST_WIDTH, "%d", d->video_cos);
	CLI_AMI_OUTPUT_YES_NO("DND Feature enabled",	CLI_AMI_LIST_WIDTH, d->dndFeature.enabled);
	CLI_AMI_OUTPUT_PARAM("DND Status",		CLI_AMI_LIST_WIDTH, "%s", (d->dndFeature.status) ? dndmode2str(d->dndFeature.status) : "Disabled");
	CLI_AMI_OUTPUT_BOOL("Can Transfer",		CLI_AMI_LIST_WIDTH, d->transfer);
	CLI_AMI_OUTPUT_BOOL("Can Park",			CLI_AMI_LIST_WIDTH, d->park);
	CLI_AMI_OUTPUT_BOOL("Can CFWDALL",		CLI_AMI_LIST_WIDTH, d->cfwdall);
	CLI_AMI_OUTPUT_BOOL("Can CFWBUSY",		CLI_AMI_LIST_WIDTH, d->cfwdbusy);
	CLI_AMI_OUTPUT_BOOL("Can CFWNOANSWER",		CLI_AMI_LIST_WIDTH, d->cfwdnoanswer);
	CLI_AMI_OUTPUT_YES_NO("Allow ringin notification (e)", CLI_AMI_LIST_WIDTH, d->allowRinginNotification);
	CLI_AMI_OUTPUT_BOOL("Private softkey",		CLI_AMI_LIST_WIDTH, d->privacyFeature.enabled);
	CLI_AMI_OUTPUT_PARAM("Dtmf mode",		CLI_AMI_LIST_WIDTH, "%s", (d->dtmfmode) ? "Out-of-Band" : "In-Band");
	CLI_AMI_OUTPUT_PARAM("digit timeout",		CLI_AMI_LIST_WIDTH, "%d", d->digittimeout);
	CLI_AMI_OUTPUT_BOOL("Nat",			CLI_AMI_LIST_WIDTH, d->nat);
	CLI_AMI_OUTPUT_YES_NO("Videosupport?",		CLI_AMI_LIST_WIDTH, sccp_device_isVideoSupported(d));
	CLI_AMI_OUTPUT_BOOL("Direct RTP",		CLI_AMI_LIST_WIDTH, d->directrtp);
	CLI_AMI_OUTPUT_BOOL("Trust phone ip (deprecated)", CLI_AMI_LIST_WIDTH, d->trustphoneip);
	CLI_AMI_OUTPUT_PARAM("Bind Address",		CLI_AMI_LIST_WIDTH, "%s:%d", (d->session) ? pbx_inet_ntoa(d->session->sin.sin_addr) : "???.???.???.???", (d->session) ? ntohs(d->session->sin.sin_port) : 0);
	CLI_AMI_OUTPUT_PARAM("Server Address",		CLI_AMI_LIST_WIDTH, "%s", (d->session) ? ast_inet_ntoa(d->session->ourip) : "???.???.???.???");
	CLI_AMI_OUTPUT_PARAM("Deny/Permit",		CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(ha_buf));
	CLI_AMI_OUTPUT_PARAM("Early RTP",		CLI_AMI_LIST_WIDTH, "%s (%s)", d->earlyrtp ? "Yes" : "No", d->earlyrtp ? sccp_indicate2str(d->earlyrtp) : "none");
	CLI_AMI_OUTPUT_PARAM("Device State (Acc.)",	CLI_AMI_LIST_WIDTH, "%s", accessorystate2str(d->accessorystatus));
	CLI_AMI_OUTPUT_PARAM("Last Used Accessory",	CLI_AMI_LIST_WIDTH, "%s", accessory2str(d->accessoryused));
	CLI_AMI_OUTPUT_PARAM("Last dialed number",	CLI_AMI_LIST_WIDTH, "%s", d->lastNumber);
	CLI_AMI_OUTPUT_PARAM("Default line instance",	CLI_AMI_LIST_WIDTH, "%d", d->defaultLineInstance);
	CLI_AMI_OUTPUT_PARAM("Custom Background Image",	CLI_AMI_LIST_WIDTH, "%s", d->backgroundImage ? d->backgroundImage : "---");
	CLI_AMI_OUTPUT_PARAM("Custom Ring Tone",	CLI_AMI_LIST_WIDTH, "%s", d->ringtone ? d->ringtone : "---");
#ifdef CS_ADV_FEATURES
	CLI_AMI_OUTPUT_BOOL("Use Placed Calls",		CLI_AMI_LIST_WIDTH, d->useRedialMenu);
#endif
	CLI_AMI_OUTPUT_BOOL("PendingUpdate",		CLI_AMI_LIST_WIDTH, d->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("PendingDelete",		CLI_AMI_LIST_WIDTH, d->pendingDelete);
#ifdef CS_SCCP_PICKUP
	CLI_AMI_OUTPUT_BOOL("Directed Pickup",		CLI_AMI_LIST_WIDTH, d->directed_pickup);
	CLI_AMI_OUTPUT_PARAM("Pickup Context",		CLI_AMI_LIST_WIDTH, "%s (%s)", d->directed_pickup_context, pbx_context_find(d->directed_pickup_context) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_BOOL("Pickup Mode Answer",	CLI_AMI_LIST_WIDTH, d->directed_pickup_modeanswer);
#endif
#ifdef CS_CONFERENCE
	CLI_AMI_OUTPUT_BOOL("allow_conference",		CLI_AMI_LIST_WIDTH, d->allow_conference);
	CLI_AMI_OUTPUT_BOOL("conf_play_general_announce", CLI_AMI_LIST_WIDTH, d->conf_play_general_announce);
	CLI_AMI_OUTPUT_BOOL("conf_play_part_announce",	CLI_AMI_LIST_WIDTH, d->conf_play_part_announce);
	CLI_AMI_OUTPUT_BOOL("conf_mute_on_entry",	CLI_AMI_LIST_WIDTH, d->conf_mute_on_entry);
	CLI_AMI_OUTPUT_PARAM("conf_music_on_hold_class",CLI_AMI_LIST_WIDTH, d->conf_music_on_hold_class);
#endif
	/* *INDENT-ON* */
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
#define CLI_AMI_TABLE_FIELDS 														\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(TypeStr,		s,	30,	config_buttontype2str(buttonconfig->type))		\
			CLI_AMI_TABLE_FIELD(Type,		d,	24,	buttonconfig->type)					\
			CLI_AMI_TABLE_FIELD(pendUpdt,		s,	8, 	buttonconfig->pendingUpdate ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(pendDel,		s, 	8, 	buttonconfig->pendingUpdate ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(Default,		s,	9,	(0!=buttonconfig->instance && d->defaultLineInstance == buttonconfig->instance && LINE==buttonconfig->type) ? "Yes" : "No")
#include "sccp_cli_table.h"

		// LINES
#define CLI_AMI_TABLE_NAME Lines
#define CLI_AMI_TABLE_PER_ENTRY_NAME Line
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 													\
			if (buttonconfig->type == LINE) {										\
				l = sccp_line_find_byname(buttonconfig->button.line.name, FALSE);					\
				if (l) {												\
					linedevice = sccp_linedevice_find(d, l);
#define CLI_AMI_TABLE_AFTER_ITERATION 													\
					linedevice = linedevice ? sccp_linedevice_release(linedevice) : NULL;				\
				sccp_line_release(l);											\
				}													\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 														\
			CLI_AMI_TABLE_FIELD(Id,			d,	4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(Name,		s,	23,	l->name)						\
			CLI_AMI_TABLE_FIELD(Suffix,		s,	6,	buttonconfig->button.line.subscriptionId.number)	\
			CLI_AMI_TABLE_FIELD(Label,		s,	24, 	l->label)						\
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
			CLI_AMI_TABLE_FIELD(Number,		s,	31,	buttonconfig->button.speeddial.ext)			\
			CLI_AMI_TABLE_FIELD(Hint,		s,	27, 	buttonconfig->button.speeddial.hint)
		//                    CLI_AMI_TABLE_FIELD(HintStatus,    s,      20,     ast_extension_state2str(ast_extension_state()))
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
			CLI_AMI_TABLE_FIELD(Options,		s,	31,	buttonconfig->button.feature.options)			\
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
			CLI_AMI_TABLE_FIELD(URL,		s,	59,	buttonconfig->button.service.url)
#include "sccp_cli_table.h"
	}

	if (d->variables) {
		// SERVICEURL
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = d->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Name,		s,	28,	v->name)				\
			CLI_AMI_TABLE_FIELD(Value,		s,	59,	v->value)
#include "sccp_cli_table.h"
	}

	sccp_call_statistics_type_t callstattype;
	sccp_call_statistics_t *stats = NULL;

#define CLI_AMI_TABLE_NAME CallStatistics
#define CLI_AMI_TABLE_PER_ENTRY_NAME Statistics
#define CLI_AMI_TABLE_ITERATOR for(callstattype = SCCP_CALLSTATISTIC_LAST; callstattype <= SCCP_CALLSTATISTIC_AVG; callstattype++)
#define CLI_AMI_TABLE_BEFORE_ITERATION stats = &d->call_statistics[callstattype];
#define CLI_AMI_TABLE_FIELDS														\
			CLI_AMI_TABLE_FIELD(Type,		s,	8,	stats->type ? stats->type : "UNDEF")			\
			CLI_AMI_TABLE_FIELD(Calls,		d,	8,	stats->num)					\
			CLI_AMI_TABLE_FIELD(PcktSnd,		d,	8,	stats->packets_sent)				\
			CLI_AMI_TABLE_FIELD(PcktRcvd,		d,	8,	stats->packets_received)				\
			CLI_AMI_TABLE_FIELD(Lost,		d,	8,	stats->packets_lost)				\
			CLI_AMI_TABLE_FIELD(Jitter,		d,	8,	stats->jitter)					\
			CLI_AMI_TABLE_FIELD(Latency,		d,	8,	stats->latency)					\
			CLI_AMI_TABLE_FIELD(Quality,		f,	8,	stats->opinion_score_listening_quality)		\
			CLI_AMI_TABLE_FIELD(avgQual,		f,	8,	stats->avg_opinion_score_listening_quality)	\
			CLI_AMI_TABLE_FIELD(meanQual,		f,	8,	stats->mean_opinion_score_listening_quality)	\
			CLI_AMI_TABLE_FIELD(maxQual,		f,	8,	stats->max_opinion_score_listening_quality)	\
			CLI_AMI_TABLE_FIELD(rConceal,		f,	8,	stats->cumulative_concealement_ratio)		\
			CLI_AMI_TABLE_FIELD(sConceal,		d,	8,	stats->concealed_seconds)
#include "sccp_cli_table.h"

	sccp_device_release(d);
	if (s) {
		*total = local_total;
		astman_append(s, "\r\n");
	}
	return RESULT_SUCCESS;
}

static char cli_device_usage[] = "Usage: sccp show device <deviceId>\n" "       Lists device settings for the SCCP subsystem.\n";
static char ami_device_usage[] = "Usage: SCCPShowDevice\n" "Lists device settings for the SCCP subsystem.\n\n" "PARAMS: DeviceName\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "device"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
#define AMI_COMMAND "SCCPShowDevice"
#define CLI_AMI_PARAMS "DeviceName"
CLI_AMI_ENTRY(show_device, sccp_show_device, "Lists device settings", cli_device_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - lines
     *        - devices
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
		pbx_cli(fd, "\n+--- Lines ---------------------------------------------------------------------------------------------------------------------------+\n");
		pbx_cli(fd, "| %-13s %-9s %-30s %-16s %-4s %-4s %-49s |\n", "Ext", "Suffix", "Label", "Device", "MWI", "Chs", "Active Channel");
		pbx_cli(fd, "+ ============= ========= ============================== ================ ==== ==== ================================================= +\n");
	} else {
		astman_append(s, "Event: TableStart\r\n");
		local_total++;
		astman_append(s, "TableName: Lines\r\n");
		local_total++;
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		astman_append(s, "\r\n");
	}
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		channel = NULL;
		// \todo handle shared line
		d = NULL;

#if 0														// never gonna happen because of line above
		if (d) {
			sccp_device_retain(d);
			channel = d->active_channel;
			sccp_device_release(d);
		}
#endif

		if (!channel || (channel->line != l))
			channel = NULL;

		memset(&cap_buf, 0, sizeof(cap_buf));

		if (channel && channel->owner) {
			pbx_getformatname_multiple(cap_buf, sizeof(cap_buf), pbx_channel_nativeformats(channel->owner));
		}

		sccp_linedevices_t *linedevice;

		found_linedevice = 0;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			if ((d = sccp_device_retain(linedevice->device))) {
				if (!s) {
					pbx_cli(fd, "| %-13s %-9s %-30s %-16s %-4s %-4d %-10s %-10s %-16s %-10s |\n",
						!found_linedevice ? l->name : " +--", linedevice->subscriptionId.number, l->label, (d) ? d->id : "--", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF", SCCP_RWLIST_GETSIZE(l->channels), (channel) ? sccp_indicate2str(channel->state) : "--", (channel) ? calltype2str(channel->calltype) : "", (channel) ? ((channel->calltype == SKINNY_CALLTYPE_OUTBOUND) ? channel->callInfo.calledPartyName : channel->callInfo.callingPartyName) : "", cap_buf);
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
				d = sccp_device_release(d);
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);

		if (found_linedevice == 0) {
			if (!s) {
				pbx_cli(fd, "| %-13s %-9s %-30s %-16s %-4s %-4d %-10s %-10s %-16s %-10s |\n", l->name, "", l->label, "--", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF", SCCP_RWLIST_GETSIZE(l->channels), (channel) ? sccp_indicate2str(channel->state) : "--", (channel) ? calltype2str(channel->calltype) : "", (channel) ? ((channel->calltype == SKINNY_CALLTYPE_OUTBOUND) ? channel->callInfo.calledPartyName : channel->callInfo.callingPartyName) : "", cap_buf);
			} else {
				astman_append(s, "Event: LineEntry\r\n");
				astman_append(s, "ChannelType: SCCP\r\n");
				astman_append(s, "ChannelObjectType: Line\r\n");
				astman_append(s, "Exten: %s\r\n", l->name);
				astman_append(s, "Label: %s\r\n", l->label);
				astman_append(s, "Device: %s\r\n", "(null)");
				astman_append(s, "MWI: %s\r\n", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF");
				astman_append(s, "\r\n");
			}
		}
		if (!s) {
			for (v = l->variables; v; v = v->next)
				pbx_cli(fd, "| %-13s %-9s %-30s = %-74.74s |\n", "", "Variable:", v->name, v->value);

			if (strcmp(l->defaultSubscriptionId.number, "") || strcmp(l->defaultSubscriptionId.name, ""))
				pbx_cli(fd, "| %-13s %-9s %-30s %-76.76s |\n", "", "SubscrId:", l->defaultSubscriptionId.number, l->defaultSubscriptionId.name);
		}
	}
	if (!s) {
		pbx_cli(fd, "+-------------------------------------------------------------------------------------------------------------------------------------+\n");
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
	if (s) {
		*total = local_total;
		astman_append(s, "\r\n");
	}

	return RESULT_SUCCESS;
}

static char cli_lines_usage[] = "Usage: sccp show lines\n" "       Lists all lines known to the SCCP subsystem.\n";
static char ami_lines_usage[] = "Usage: SCCPShowLines\n" "Lists all lines known to the SCCP subsystem\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "lines"
#define AMI_COMMAND "SCCPShowLines"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_lines, sccp_show_lines, "List defined SCCP Lines", cli_lines_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - line
     *        - line->devices
     */
    //static int sccp_show_line(int fd, int argc, char *argv[])
static int sccp_show_line(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *l;
	sccp_linedevices_t *linedevice;
	sccp_mailbox_t *mailbox;
	PBX_VARIABLE_TYPE *v = NULL;
	struct ast_str *callgroup_buf = pbx_str_alloca(512);

#ifdef CS_SCCP_PICKUP
	struct ast_str *pickupgroup_buf = pbx_str_alloca(512);
#endif
	int local_total = 0;

	const char *line;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "LineName needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "LineName needs to be supplied %s\n", "");
	}
	line = sccp_strdupa(argv[3]);
	l = sccp_line_find_byname(line, FALSE);

	if (!l) {
		pbx_log(LOG_WARNING, "Failed to get line %s\n", line);
		CLI_AMI_ERROR(fd, s, m, "Can't find settings for line %s\n", line);
	}

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
	CLI_AMI_OUTPUT_PARAM("MeetMe number", CLI_AMI_LIST_WIDTH, "%s", l->meetmenum);
	CLI_AMI_OUTPUT_PARAM("MeetMe Options", CLI_AMI_LIST_WIDTH, "%s", l->meetmeopts);
	CLI_AMI_OUTPUT_PARAM("Context", CLI_AMI_LIST_WIDTH, "%s (%s)", l->context ? l->context : "<not set>", pbx_context_find(l->context) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_PARAM("Language", CLI_AMI_LIST_WIDTH, "%s", l->language ? l->language : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Account Code", CLI_AMI_LIST_WIDTH, "%s", l->accountcode ? l->accountcode : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Musicclass", CLI_AMI_LIST_WIDTH, "%s", l->musicclass ? l->musicclass : "<not set>");
	CLI_AMI_OUTPUT_PARAM("AmaFlags", CLI_AMI_LIST_WIDTH, "%d", l->amaflags);
	sccp_print_group(callgroup_buf, sizeof(callgroup_buf), l->callgroup);
	CLI_AMI_OUTPUT_PARAM("Call Group", CLI_AMI_LIST_WIDTH, "%s", callgroup_buf ? pbx_str_buffer(callgroup_buf) : "");
#ifdef CS_SCCP_PICKUP
	sccp_print_group(pickupgroup_buf, sizeof(pickupgroup_buf), l->pickupgroup);
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
	CLI_AMI_OUTPUT_BOOL("Pending Delete", CLI_AMI_LIST_WIDTH, l->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("Pending Update", CLI_AMI_LIST_WIDTH, l->pendingDelete);

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

#define CLI_AMI_TABLE_FIELDS 											\
		CLI_AMI_TABLE_FIELD(Device,		s,	15,	linedevice->device->id)			\
		CLI_AMI_TABLE_FIELD(CfwdType,		s,	8,	linedevice->cfwdAll.enabled ? "All" : (linedevice->cfwdBusy.enabled ? "Busy" : ""))					\
		CLI_AMI_TABLE_FIELD(CfwdNumber,		s,	20,	linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : (linedevice->cfwdBusy.enabled ? linedevice->cfwdBusy.number : ""))
#include "sccp_cli_table.h"

	// Mailboxes connected to this line
#define CLI_AMI_TABLE_NAME Mailboxes
#define CLI_AMI_TABLE_PER_ENTRY_NAME Mailbox
#define CLI_AMI_TABLE_LIST_ITER_HEAD &l->mailboxes
#define CLI_AMI_TABLE_LIST_ITER_VAR mailbox
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 											\
		CLI_AMI_TABLE_FIELD(mailbox,		s,	15,	mailbox->mailbox)			\
		CLI_AMI_TABLE_FIELD(context,		s,	15,	mailbox->context)			
#include "sccp_cli_table.h"

	if (l->variables) {
		// SERVICEURL
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = l->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 											\
		CLI_AMI_TABLE_FIELD(Name,		s,	15,	v->name)				\
		CLI_AMI_TABLE_FIELD(Value,		s,	29,	v->value)
#include "sccp_cli_table.h"
	}
	sccp_line_release(l);
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_line_usage[] = "Usage: sccp show line <lineId>\n" "       List defined SCCP line settings.\n";
static char ami_line_usage[] = "Usage: SCCPShowLine\n" "List defined SCCP line settings.\n\n" "PARAMS: LineName\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "line"
#define CLI_COMPLETE SCCP_CLI_LINE_COMPLETER
#define AMI_COMMAND "SCCPShowLine"
#define CLI_AMI_PARAMS "LineName"
CLI_AMI_ENTRY(show_line, sccp_show_line, "List defined SCCP line settings", cli_line_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - lines
     *        - line
     *          - line->channels
     */
    //static int sccp_show_channels(int fd, int argc, char *argv[])
static int sccp_show_channels(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_channel_t *channel;
	sccp_line_t *l;
	sccp_device_t *d;
	int local_total = 0;
	char tmpname[20];

#define CLI_AMI_TABLE_NAME Channels
#define CLI_AMI_TABLE_PER_ENTRY_NAME Channel
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(lines)
#define CLI_AMI_TABLE_LIST_ITER_VAR l
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 												\
		l = sccp_line_retain(l);											\
		SCCP_LIST_LOCK(&l->channels);											\
		SCCP_LIST_TRAVERSE(&l->channels, channel, list) {								\
			d = sccp_channel_getDevice_retained(channel);								\
			if (channel->conference_id) {										\
				snprintf(tmpname, sizeof(tmpname), "SCCPCONF/%03d/%03d", channel->conference_id, channel->conference_participant_id);	\
			} else {												\
				snprintf(tmpname, sizeof(tmpname), "SCCP/%s-%08x", l->name, channel->callid);			\
			}

#define CLI_AMI_TABLE_AFTER_ITERATION 												\
			if (d)													\
				 sccp_device_release(d);									\
		}														\
		SCCP_LIST_UNLOCK(&l->channels);											\
		sccp_line_release(l);

#define CLI_AMI_TABLE_FIELDS 													\
		CLI_AMI_TABLE_FIELD(ID,			d,	5,	channel->callid)					\
		CLI_AMI_TABLE_FIELD(PBX,		s,	20,	strdupa(tmpname))					\
		CLI_AMI_TABLE_FIELD(Line,		s,	10,	channel->line->name)					\
		CLI_AMI_TABLE_FIELD(Device,		s,	16,	d ? d->id : "(unknown)")				\
		CLI_AMI_TABLE_FIELD(DeviceDescr,	s,	32,	d ? d->description : "(unknown)")			\
		CLI_AMI_TABLE_FIELD(NumCalled,		s,	10,	channel->callInfo.calledPartyNumber)			\
		CLI_AMI_TABLE_FIELD(PBX State,		s,	10,	(channel->owner) ? pbx_state2str(PBX(getChannelState)(channel)) : "(none)")	\
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "channels"
#define AMI_COMMAND "SCCPShowChannels"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_channels, sccp_show_channels, "Lists active SCCP channels", cli_channels_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - sessions
     *        - session
     *        - device
     *        - session
     */
    //static int sccp_show_sessions(int fd, int argc, char *argv[])
static int sccp_show_sessions(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d = NULL;
	int local_total = 0;

#define CLI_AMI_TABLE_NAME Sessions
#define CLI_AMI_TABLE_PER_ENTRY_NAME Session
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(sessions)
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_session_t
#define CLI_AMI_TABLE_LIST_ITER_VAR session
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 														\
		sccp_session_lock(session);													\
		if (session->device && (d = sccp_device_retain(session->device))) {								\

#define CLI_AMI_TABLE_AFTER_ITERATION 														\
			d = sccp_device_release(d);												\
		};																\
		sccp_session_unlock(session);													\

#define CLI_AMI_TABLE_FIELDS 															\
		CLI_AMI_TABLE_FIELD(Socket,			d,	10,	session->fds[0].fd)						\
		CLI_AMI_TABLE_FIELD(IP,				s,	CLI_AMI_LIST_WIDTH,	pbx_inet_ntoa(session->sin.sin_addr))		\
		CLI_AMI_TABLE_FIELD(Port,			d,	5,	session->sin.sin_port)						\
		CLI_AMI_TABLE_FIELD(KA,				d,	4,	(uint32_t) (time(0) - session->lastKeepAlive))			\
		CLI_AMI_TABLE_FIELD(KAI,			d,	4,	d->keepaliveinterval)						\
		CLI_AMI_TABLE_FIELD(Device,			s,	15,	(d) ? d->id : "--")						\
		CLI_AMI_TABLE_FIELD(State,			s,	14,	(d) ? devicestate2str(d->state) : "--")			\
		CLI_AMI_TABLE_FIELD(Type,			s,	15,	(d) ? devicetype2str(d->skinny_type) : "--")			\
		CLI_AMI_TABLE_FIELD(RegState,			s,	10,	(d) ? registrationstate2str(d->registrationState) : "--")	\
		CLI_AMI_TABLE_FIELD(Token,			s,	10,	(d && d->status.token ) ?  (SCCP_TOKEN_STATE_ACK == d->status.token ? "Ack" : (SCCP_TOKEN_STATE_REJ == d->status.token ? "Reject" : "--")) : "--")
#include "sccp_cli_table.h"

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_sessions_usage[] = "Usage: sccp show sessions\n" "	Show All SCCP Sessions.\n";
static char ami_sessions_usage[] = "Usage: SCCPShowSessions\n" "Show All SCCP Sessions.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "sessions"
#define AMI_COMMAND "SCCPShowSessions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_sessions, sccp_show_sessions, "Show all SCCP sessions", cli_sessions_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

/* ---------------------------------------------------------------------------------------------SHOW_MWI_SUBSCRIPTIONS- */
// sccp_show_mwi_subscriptions implementation moved to sccp_mwi.c, because of access to private struct
static char cli_mwi_subscriptions_usage[] = "Usage: sccp show mwi subscriptions\n" "	Show All SCCP MWI Subscriptions.\n";
static char ami_mwi_subscriptions_usage[] = "Usage: SCCPShowMWISubscriptions\n" "Show All SCCP MWI Subscriptions.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "mwi", "subscriptions"
#define AMI_COMMAND "SCCPShowMWISubscriptions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_mwi_subscriptions, sccp_show_mwi_subscriptions, "Show all SCCP MWI subscriptions", cli_mwi_subscriptions_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
#if defined(DEBUG) || defined(CS_EXPERIMENTAL)


/* ---------------------------------------------------------------------------------------------CONFERENCE FUNCTIONS- */
#ifdef CS_SCCP_CONFERENCE

static char cli_conferences_usage[] = "Usage: sccp show conferences\n" "       Lists running SCCP conferences.\n";
static char ami_conferences_usage[] = "Usage: SCCPShowConferences\n" "Lists running SCCP conferences.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "conferences"
#define AMI_COMMAND "SCCPShowConferences"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_conferences, sccp_cli_show_conferences, "List running SCCP Conferences", cli_conferences_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

static char cli_conference_usage[] = "Usage: sccp show conference\n" "       Lists running SCCP conference.\n";
static char ami_conference_usage[] = "Usage: SCCPShowConference\n" "Lists running SCCP conference.\n\n" "PARAMS: ConferenceId\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "conference"
#define AMI_COMMAND "SCCPShowConference"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_conference, sccp_cli_show_conference, "List running SCCP Conference", cli_conference_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif			

											/* DOXYGEN_SHOULD_SKIP_THIS */
static char cli_conference_action_usage[] = "Usage: sccp conference [conference_id]\n" "	Conference [EndConf | Kick | Mute | Invite | Moderate] [conference_id] [participant_id].\n";
static char ami_conference_action_usage[] = "Usage: SCCPConference [conference id]\n" "Conference Actions.\n\n" "PARAMS: \n" "  Action: [EndConf | Kick | Mute | Invite | Moderate]\n" "  ConferenceId\n" "  ParticipantId\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "conference"
#define CLI_COMPLETE SCCP_CLI_CONFERENCE_COMPLETER
#define AMI_COMMAND "SCCPConference"
#define CLI_AMI_PARAMS "Action","ConferenceId","ParticipantId"
CLI_AMI_ENTRY(conference_action, sccp_cli_conference_action, "Conference Action", cli_conference_action_usage, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
#endif														/* CS_SCCP_CONFERENCE */
    /* -------------------------------------------------------------------------------------------------------TEST MESSAGE- */
#define NUM_LOOPS 20
#define NUM_OBJECTS 100
#ifdef CS_EXPERIMENTAL
struct refcount_test {
	int id;
	int loop;
	unsigned int threadid;
	char *test;
} *object[NUM_OBJECTS];

static void sccp_cli_refcount_test_destroy(struct refcount_test *obj)
{
	sccp_log(0) ("TEST: Destroyed %d, thread: %d\n", obj->id, (unsigned int) pthread_self());
	sccp_free(object[obj->id]->test);
	object[obj->id]->test = NULL;
};

static void *sccp_cli_refcount_test_thread(void *data)
{
	boolean_t working = *(boolean_t *) data;
	struct refcount_test *obj = NULL, *obj1 = NULL;
	int test, loop;
	int random_object;

	if (working) {
		// CORRECT
		for (loop = 0; loop < NUM_LOOPS; loop++) {
			for (test = 0; test < NUM_OBJECTS; test++) {
				random_object = rand() % NUM_OBJECTS;
				sccp_log(0) ("TEST: retain/release %d, loop: %d, thread: %d\n", random_object, loop, (unsigned int) pthread_self());
				if ((obj = sccp_refcount_retain(object[random_object], __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
					usleep(random_object % 10);
					if ((obj1 = sccp_refcount_retain(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
						usleep(random_object % 10);
						obj1 = sccp_refcount_release(obj1, __FILE__, __LINE__, __PRETTY_FUNCTION__);
					}
					obj = sccp_refcount_release(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__);
				}
			}
		}
	} else {
		// FALSE
		for (loop = 0; loop < NUM_LOOPS; loop++) {
			for (test = 0; test < NUM_OBJECTS; test++) {
				random_object = rand() % NUM_OBJECTS;
				sccp_log(0) ("TEST: retain/release %d, loop: %d, thread: %d\n", random_object, loop, (unsigned int) pthread_self());
				if ((obj = sccp_refcount_retain(object[random_object], __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
					usleep(random_object % 10);
					if ((obj = sccp_refcount_retain(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
						usleep(random_object % 10);
						obj = sccp_refcount_release(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__);	// obj will be NULL after releasing
					}
					obj = sccp_refcount_release(obj, __FILE__, __LINE__, __PRETTY_FUNCTION__);
				}
			}
		}
	}

	return NULL;
}

static void *sccp_cli_threadpool_test_thread(void *data)
{
	int loop;

	//      int num_loops=rand();
	int num_loops = 1000;

	sccp_log(0) (VERBOSE_PREFIX_4 "Running work: %d, loops: %d\n", (unsigned int) pthread_self(), num_loops);
	for (loop = 0; loop < num_loops; loop++) {
		usleep(1);
	}
	sccp_log(0) (VERBOSE_PREFIX_4 "Thread: %d Done\n", (unsigned int) pthread_self());
	return 0;
}

static void sccp_update_statusbar(const sccp_device_t * d, const char *msg, const uint8_t priority, const uint8_t timeout, uint8_t level, boolean_t clear, const uint8_t lineInstance, const uint32_t callid)
{
	sccp_moo_t *r;
	
	if (!d || !d->protocol) {
		return;
	}
	
	switch (level) {
		case 0:
			if (!clear) {
				REQ(r, DisplayTextMessage);
				sccp_copy_string(r->msg.DisplayTextMessage.displayMessage, msg, sizeof(r->msg.DisplayTextMessage.displayMessage));
				sccp_dev_send(d, r);
			} else {
				sccp_dev_sendmsg(d, ClearDisplay);
			}
		case 1:
			if (!clear) {
				d->protocol->displayNotify(d, timeout, msg);
			} else {
				sccp_dev_sendmsg(d, ClearNotifyMessage);
			}
		case 2:
			if (!clear) {
				d->protocol->displayPriNotify(d, priority, timeout, msg);
			} else {
				REQ(r, ClearPriNotifyMessage);
				r->msg.ClearPriNotifyMessage.lel_priority = htolel(priority);
				sccp_dev_send(d, r);
			}
		case 3:
			if (!clear) {
				d->protocol->displayPrompt(d, lineInstance, callid, timeout, msg);
			} else {
				REQ(r, ClearPromptStatusMessage);
				r->msg.ClearPromptStatusMessage.lel_callReference = htolel(callid);
				r->msg.ClearPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
				sccp_dev_send(d, r);
			}
	}
}
#endif

/*!
 * \brief Test Message
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
static int sccp_test_message(int fd, int argc, char *argv[])
{
	if (argc < 4)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

#ifdef CS_EXPERIMENTAL
	// OpenReceiveChannel TEST
	if (!strcasecmp(argv[3], "openreceivechannel")) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Testing re-Sending OpenReceiveChannel to change Payloads on the fly!!\n");
		sccp_moo_t *r1;
		sccp_moo_t *r2;
		int packetSize = 20;										/*! \todo calculate packetSize */

		sccp_device_t *d = NULL;
		sccp_line_t *l = NULL;
		sccp_channel_t *channel = NULL;

		SCCP_RWLIST_RDLOCK(&GLOB(lines));
		SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
			SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
				d = sccp_channel_getDevice_retained(channel);
				sccp_channel_retain(channel);
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Sending OpenReceiveChannel and changing payloadType to 8\n");

				REQ(r1, OpenReceiveChannel);
				r1->msg.OpenReceiveChannel.v17.lel_conferenceId = htolel(channel->callid);
				r1->msg.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
				r1->msg.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
				r1->msg.OpenReceiveChannel.v17.lel_payloadType = htolel(8);
				r1->msg.OpenReceiveChannel.v17.lel_vadValue = htolel(channel->line->echocancel);
				r1->msg.OpenReceiveChannel.v17.lel_conferenceId1 = htolel(channel->callid);
				r1->msg.OpenReceiveChannel.v17.lel_rtptimeout = htolel(10);
				sccp_dev_send(d, r1);
				//                            sleep(1);
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Sending OpenReceiveChannel and changing payloadType to 4\n");
				REQ(r2, OpenReceiveChannel);
				r2->msg.OpenReceiveChannel.v17.lel_conferenceId = htolel(channel->callid);
				r2->msg.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
				r2->msg.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
				r2->msg.OpenReceiveChannel.v17.lel_payloadType = htolel(4);
				r2->msg.OpenReceiveChannel.v17.lel_vadValue = htolel(channel->line->echocancel);
				r2->msg.OpenReceiveChannel.v17.lel_conferenceId1 = htolel(channel->callid);
				r2->msg.OpenReceiveChannel.v17.lel_rtptimeout = htolel(10);
				sccp_dev_send(d, r2);

				sccp_channel_release(channel);
				sccp_device_release(d);
			}
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Testing re-Sending OpenReceiveChannel. It WORKS !\n");
		}
		SCCP_RWLIST_UNLOCK(&GLOB(lines));
		return RESULT_SUCCESS;
	}
	// SpeedDialStatDynamicMessage = 0x0149,
	if (!strcasecmp(argv[3], "speeddialstatdynamic")) {
		sccp_device_t *d = NULL;
		sccp_buttonconfig_t *buttonconfig = NULL;
		uint8_t instance = 0;
		uint8_t buttonID = SKINNY_BUTTONTYPE_SPEEDDIAL;
		uint32_t state = 66306;										// 0, 66306, 131589, 
		sccp_moo_t *r1;

		if (argc < 5) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Device Not specified\n");
			return RESULT_FAILURE;
		}
		if ((d = sccp_device_find_byid(argv[4], FALSE))) {
			instance = atoi(argv[5]);
			SCCP_LIST_LOCK(&d->buttonconfig);
			SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
				if (buttonconfig->type == SPEEDDIAL) {
					instance = buttonconfig->instance;
					REQ(r1, SpeedDialStatDynamicMessage);
					r1->msg.SpeedDialStatDynamicMessage.lel_instance = htolel(instance);
					r1->msg.SpeedDialStatDynamicMessage.lel_type = htolel(buttonID);
					r1->msg.SpeedDialStatDynamicMessage.lel_status = htolel(state ? state : 0);
					sccp_copy_string(r1->msg.SpeedDialStatDynamicMessage.DisplayName, "NEW TEXT", strlen("NEW_TEXT") + 1);
					sccp_dev_send(d, r1);
				}
			}
			SCCP_LIST_UNLOCK(&d->buttonconfig);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "refcount")) {
		int thread;
		boolean_t working = TRUE;
		int num_threads = (argc == 5) ? atoi(argv[4]) : 4;

		if (argc == 6 && !strcmp(argv[5], "fail")) {
			working = FALSE;
		}
		pthread_t t;
		int test;
		char id[23];

		for (test = 0; test < NUM_OBJECTS; test++) {
			snprintf(id, sizeof(id), "%d/%d", test, (unsigned int) pthread_self());
			object[test] = (struct refcount_test *) sccp_refcount_object_alloc(sizeof(struct refcount_test), SCCP_REF_TEST, id, sccp_cli_refcount_test_destroy);
			object[test]->id = test;
			object[test]->threadid = (unsigned int) pthread_self();
			object[test]->test = strdup(id);
			sccp_log(0) ("TEST: Created %d\n", object[test]->id);
		}
		sccp_refcount_print_hashtable(fd);
		sleep(3);

		for (thread = 0; thread < num_threads; thread++) {
			pbx_pthread_create(&t, NULL, sccp_cli_refcount_test_thread, &working);
		}
		pthread_join(t, NULL);
		sleep(3);

		for (test = 0; test < NUM_OBJECTS; test++) {
			if (object[test]) {
				sccp_log(0) ("TEST: Final Release %d, thread: %d\n", object[test]->id, (unsigned int) pthread_self());
				sccp_refcount_release(object[test], __FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		sleep(1);
		sccp_refcount_print_hashtable(fd);
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "threadpool")) {
		int work;
		int num_work = (argc == 5) ? atoi(argv[4]) : 4;

		for (work = 0; work < num_work; work++) {
			if (!sccp_threadpool_add_work(GLOB(general_threadpool), (void *) sccp_cli_threadpool_test_thread, (void *) &work)) {
				pbx_log(LOG_ERROR, "Could not add work to threadpool\n");
			}
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "hint")) {
		int state = (argc == 6) ? atoi(argv[5]) : 0;

		pbx_devstate_changed(state, "SCCP/%s", argv[4]);
		pbx_log(LOG_NOTICE, "Hint %s Set NewState: %d\n", argv[4], state);
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "permit")) {									/*  WIP */
		struct ast_str *buf = pbx_str_alloca(512);

		//struct sccp_ha *path; 
		//sccp_append_ha(const char *sense, const char *stuff, struct sccp_ha *path, int *error)

		sccp_print_ha(buf, sizeof(buf), GLOB(ha));
		pbx_log(LOG_NOTICE, "%s: HA Buffer: %s\n", argv[4], pbx_str_buffer(buf));
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "retrieveDeviceCapabilities")) {									/*  WIP */
		sccp_device_t *d = NULL;
		d = sccp_device_find_byid(argv[4], FALSE);
		if (d) {
			d->retrieveDeviceCapabilities(d);
			pbx_log(LOG_NOTICE, "%s: Done\n", d->id);
			d = sccp_device_release(d);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[3], "StatusBar")) {											/*  WIP */
		sccp_device_t *d = NULL;
		d = sccp_device_find_byid(argv[4], FALSE);
		if (d) {
			//sccp_update_statusbar(d, *msg, priority, timeout, level, clear, lineInstance, callid)
			pbx_log(LOG_NOTICE, "%s: Base\n", d->id);
			sccp_update_statusbar(d, "BASE", 0, 0, 0, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Clear Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 0, TRUE, 0, 0);
			sleep(1);

			/* Level 1 */
			pbx_log(LOG_NOTICE, "%s: Base\n", d->id);
			sccp_update_statusbar(d, "BASE", 0, 0, 0, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 1\n", d->id);
			sccp_update_statusbar(d, "LEVEL 1", 0, 0, 1, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to BASE\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 1, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Clear Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 0, TRUE, 0, 0);
			sleep(1);

			/* Level 2 */
			pbx_log(LOG_NOTICE, "%s: Base\n", d->id);
			sccp_update_statusbar(d, "BASE", 0, 0, 0, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 1\n", d->id);
			sccp_update_statusbar(d, "LEVEL 1", 0, 0, 1, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 2, Prio 1\n", d->id);
			sccp_update_statusbar(d, "Level 2, Prio 1", 1, 3, 2, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 2, Prio 4\n", d->id);
			sccp_update_statusbar(d, "Level 2, Prio 4", 4, 3, 2, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Level 2, Prio 4\n", d->id);
			sccp_update_statusbar(d, "", 4, 0, 2, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Level 1\n", d->id);
			sccp_update_statusbar(d, "", 1, 0, 2, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 1, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Clear Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 0, TRUE, 0, 0);
			sleep(1);
			
			/* Level 3 */
/*			pbx_log(LOG_NOTICE, "%s: Level 1\n", d->id);
			sccp_update_statusbar(d, "LEVEL 1", 0, 0, 1, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 2\n", d->id);
			sccp_update_statusbar(d, "Level 2", 0, 0, 2, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Level 3\n", d->id);
			sccp_update_statusbar(d, "Level 2", 0, 0, 3, FALSE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Level 2\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 3, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Level 1\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 2, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Back to Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 1, TRUE, 0, 0);
			sleep(1);

			pbx_log(LOG_NOTICE, "%s: Clear Base\n", d->id);
			sccp_update_statusbar(d, "", 0, 0, 0, TRUE, 0, 0);
			sleep(1);*/
			d = sccp_device_release(d);
		}
		return RESULT_SUCCESS;
	}
#endif
	return RESULT_FAILURE;
}

static char cli_test_message_usage[] = "Usage: sccp test message [message_name]\n" "	Test message [message_name].\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "test", "message"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_test_message, sccp_test_message, "Test a Message", cli_test_message_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* ------------------------------------------------------------------------------------------------------- REFCOUNT - */
    /*!
     * \brief Print Refcount Hash Table
     * \param fd Fd as int
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     */
static int sccp_show_refcount(int fd, int argc, char *argv[])
{
	if (argc < 3)
		return RESULT_SHOWUSAGE;

	sccp_refcount_print_hashtable(fd);
	return RESULT_SUCCESS;
}

static char cli_show_refcount_usage[] = "Usage: sccp show refcount [sortorder]\n" "	Show Refcount Hash Table. sortorder can be hash or type\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "refcount"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_show_refcount, sccp_show_refcount, "Test a Message", cli_show_refcount_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
#endif														//defined(DEBUG) || defined(CS_EXPERIMENTAL)
    /* --------------------------------------------------------------------------------------------------SHOW_SOKFTKEYSETS- */
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
     *      - softKeySetConfig
     */
    //static int sccp_show_softkeysets(int fd, int argc, char *argv[])
static int sccp_show_softkeysets(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	uint8_t i = 0;
	uint8_t v_count = 0;
	uint8_t c = 0;
	int local_total = 0;

#define CLI_AMI_TABLE_NAME SoftKeySets
#define CLI_AMI_TABLE_PER_ENTRY_NAME SoftKeySet
#define CLI_AMI_TABLE_LIST_ITER_HEAD &softKeySetConfig
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_softKeySetConfiguration_t
#define CLI_AMI_TABLE_LIST_ITER_VAR softkeyset
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION												\
		v_count = sizeof(softkeyset->modes) / sizeof(softkey_modes);							\
		for (i = 0; i < v_count; i++) {											\
			const uint8_t *b = softkeyset->modes[i].ptr;								\
			for (c = 0; c < softkeyset->modes[i].count; c++) {

#define CLI_AMI_TABLE_AFTER_ITERATION												\
			}													\
		}
#define CLI_AMI_TABLE_FIELDS													\
				CLI_AMI_TABLE_FIELD(Set,			s,	15,	softkeyset->name)		\
				CLI_AMI_TABLE_FIELD(Mode,			s,	12,	keymode2str(i))			\
				CLI_AMI_TABLE_FIELD(Description,		s,	40,	keymode2str(i))		\
				CLI_AMI_TABLE_FIELD(LblID,			d,	5,	c)				\
				CLI_AMI_TABLE_FIELD(Label,			s,	15,	label2str(b[c]))

#include "sccp_cli_table.h"

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_show_softkeysets_usage[] = "Usage: sccp show softkeysets\n" "	Show the configured SoftKeySets.\n";
static char ami_show_softkeysets_usage[] = "Usage: SCCPShowSoftkeySets\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "softkeyssets"
#define AMI_COMMAND "SCCPShowSoftkeySets"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_softkeysets, sccp_show_softkeysets, "Show configured SoftKeySets", cli_show_softkeysets_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -----------------------------------------------------------------------------------------------------MESSAGE DEVICES- */
    /*!
     * \brief Message Devices
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
     *      - devices
     *        - see sccp_dev_displaynotify()
     *        - see sccp_dev_starttone()
     */
    //static int sccp_message_devices(int fd, int argc, char *argv[])
static int sccp_message_devices(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d;
	int timeout = 0;
	boolean_t beep = FALSE;
	int local_total = 0;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "MessageText needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "MessageText needs to be supplied %s\n", "");
	}
	//      const char *messagetext = astman_get_header(m, "MessageText");
	//      if (sccp_strlen_zero(messagetext)) {

	if (sccp_strlen_zero(argv[3])) {
		pbx_log(LOG_WARNING, "MessageText cannot be empty\n");
		CLI_AMI_ERROR(fd, s, m, "messagetext cannot be empty, '%s'\n", argv[3]);
	}

	if (argc > 4) {
		if (!strcmp(argv[4], "beep")) {
			beep = TRUE;
			sscanf(argv[5], "%d", &timeout);
		}
		sscanf(argv[4], "%d", &timeout);
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Sending message '%s' to all devices (beep: %d, timeout: %d)\n", argv[3], beep, timeout);
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, FALSE, beep);
		//              sccp_dev_set_message(d, messagetext, timeout, FALSE, beep);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

static char cli_message_devices_usage[] = "Usage: sccp message devices <message text> [beep] [timeout]\n" "       Send a message to all SCCP Devices + phone beep + timeout.\n";
static char ami_message_devices_usage[] = "Usage: SCCPMessageDevices\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: MessageText, Beep, Timeout\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "message", "devices"
#define AMI_COMMAND "SCCPMessageDevices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS "MessageText", "Beep", "Timeout"
CLI_AMI_ENTRY(message_devices, sccp_message_devices, "Send a message to all SCCP devices", cli_message_devices_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
static int sccp_message_device(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d = NULL;
	int timeout = 10;
	boolean_t beep = FALSE;
	int local_total = 0;
	int res = RESULT_FAILURE;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "MessageText needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "MessageText needs to be supplied %s\n", "");
	}
	if (sccp_strlen_zero(argv[3])) {
		pbx_log(LOG_WARNING, "MessageText cannot be empty\n");
		CLI_AMI_ERROR(fd, s, m, "messagetext cannot be empty, '%s'\n", argv[3]);
	}
	if (argc > 5) {
		if (!strcmp(argv[5], "beep")) {
			beep = TRUE;
			sscanf(argv[6], "%d", &timeout);
		}
		sscanf(argv[5], "%d", &timeout);
	}
	if ((d = sccp_device_find_byid(argv[3], FALSE))) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Sending message '%s' to %s (beep: %d, timeout: %d)\n", argv[3], d->id, beep, timeout);
		sccp_dev_set_message(d, argv[4], timeout, FALSE, beep);
		res = RESULT_SUCCESS;
		d = sccp_device_release(d);
	} else {
		CLI_AMI_ERROR(fd, s, m, "Device '%s' not found!\n", argv[4]);
	}

	if (s)
		*total = local_total;
	return res;
}

static char cli_message_device_usage[] = "Usage: sccp message device <deviceId> <message text> [beep] [timeout]\n" "       Send a message to an SCCP Device + phone beep + timeout.\n";
static char ami_message_device_usage[] = "Usage: SCCPMessageDevices\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: DeviceId, MessageText, Beep, Timeout\n";
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "message", "device"
#define AMI_COMMAND "SCCPMessageDevice"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
#define CLI_AMI_PARAMS "DeviceId" "MessageText", "Beep", "Timeout"
CLI_AMI_ENTRY(message_device, sccp_message_device, "Send a message to SCCP Device", cli_message_device_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef AMI_COMMAND
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
			sscanf(argv[5], "%d", &timeout);
		}
		sscanf(argv[4], "%d", &timeout);
	} else {
		timeout = 0;
	}
	if (!res) {
		ast_cli(fd, "Failed to store the SCCP system message timeout\n");
	} else {
		sccp_log(DEBUGCAT_CLI) (VERBOSE_PREFIX_3 "SCCP system message timeout stored successfully\n");
	}

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "Sending system message '%s' to all devices (beep: %d, timeout: %d)\n", argv[3], beep, timeout);
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, TRUE, beep);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return RESULT_SUCCESS;
}

static char system_message_usage[] = "Usage: sccp system message <message text> [beep] [timeout]\n" "       The default optional timeout is 0 (forever)\n" "       Example: sccp system message \"The boss is gone. Let's have some fun!\"  10\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "system", "message"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_system_message, sccp_system_message, "Send a system wide message to all SCCP Devices", system_message_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - device
     *        - see sccp_sk_dnd()
     */
static int sccp_dnd_device(int fd, int argc, char *argv[])
{
	sccp_device_t *d = NULL;

	if (argc < 3)
		return RESULT_SHOWUSAGE;

	if ((d = sccp_device_find_byid(argv[3], TRUE))) {
		sccp_sk_dnd(d, NULL, 0, NULL);
		d = sccp_device_release(d);
	} else {
		pbx_cli(fd, "Can't find device %s\n", argv[3]);
		return RESULT_SUCCESS;
	}

	return RESULT_SUCCESS;
}

static char dnd_device_usage[] = "Usage: sccp dnd <deviceId>\n" "       Send a dnd to an SCCP Device.\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "dnd", "device"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
CLI_ENTRY(cli_dnd_device, sccp_dnd_device, "Send a dnd to SCCP Device", dnd_device_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "remove", "line"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER, SCCP_CLI_CONNECTED_LINE_COMPLETER
CLI_ENTRY(cli_remove_line_from_device, sccp_remove_line_from_device, "Remove a line from device", remove_line_from_device_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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

	if ((d = sccp_device_find_byid(argv[3], FALSE))) {
		l = sccp_line_find_byname(argv[4], FALSE);
		if (!l) {
			pbx_log(LOG_ERROR, "Error: Line %s not found\n", argv[4]);
			return RESULT_FAILURE;
		}
		sccp_config_addButton(d, -1, LINE, l->name, NULL, NULL);
		l = sccp_line_release(l);
		d = sccp_device_release(d);
	} else {
		pbx_log(LOG_ERROR, "Error: Device %s not found\n", argv[3]);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "Line %s has been added to device %s\n", l->name, d->id);
	return RESULT_SUCCESS;
}

static char add_line_to_device_usage[] = "Usage: sccp add line <deviceID> <lineID>\n" "       Add a line to a device.\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "add", "line"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER, SCCP_CLI_CONNECTED_LINE_COMPLETER
CLI_ENTRY(cli_add_line_to_device, sccp_add_line_to_device, "Add a line to a device", add_line_to_device_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
	int32_t new_debug = GLOB(debug);

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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "debug"
#define CLI_COMPLETE SCCP_CLI_DEBUG_COMPLETER
CLI_ENTRY(cli_do_debug, sccp_do_debug, "Set SCCP Debugging Types", do_debug_usage, TRUE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "no", "debug"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_no_debug, sccp_no_debug, "Set SCCP Debugging Types", no_debug_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - globals
     *
     * \note To find out more about the reload function see \ref sccp_config_reload
     */
static int sccp_cli_reload(int fd, int argc, char *argv[])
{
	sccp_readingtype_t readingtype;
	boolean_t force_reload = FALSE;
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
		if (sccp_strequals("force", argv[2])) {\
			pbx_cli(fd, "Force Reading Config file '%s'\n", GLOB(config_file_name));
			force_reload=TRUE;
		} else {
			pbx_cli(fd, "Using config file '%s' (previous config file: '%s')\n", argv[2], GLOB(config_file_name));
			if (!sccp_strequals(GLOB(config_file_name), argv[2])) {
				force_reload=TRUE;
			}
			if (GLOB(config_file_name)) {
				sccp_free(GLOB(config_file_name));
			}	
			GLOB(config_file_name) = sccp_strdup(argv[2]);
		}
	}
	sccp_config_file_status_t cfg = sccp_config_getConfig(force_reload);
	switch (cfg) {
		case CONFIG_STATUS_FILE_NOT_CHANGED:
//			if (!force_reload) {
				pbx_cli(fd, "config file '%s' has not change, skipping reload.\n", GLOB(config_file_name));
				returnval = RESULT_SUCCESS;
				break;
//			}
			/* fall through */
		case CONFIG_STATUS_FILE_OK:
			if (GLOB(cfg)) {
				pbx_cli(fd, "SCCP reloading configuration. %p\n", GLOB(cfg));
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
			}
			break;
		case CONFIG_STATUS_FILE_OLD:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = RESULT_FAILURE;
			break;
		case CONFIG_STATUS_FILE_NOT_SCCP:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "\n\n --> You are using an configuration file is not following the sccp format, please check '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = RESULT_FAILURE;
			break;
		case CONFIG_STATUS_FILE_NOT_FOUND:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
			returnval = RESULT_FAILURE;
			break;
		case CONFIG_STATUS_FILE_INVALID:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
			returnval = RESULT_FAILURE;
			break;
	}
	pbx_mutex_unlock(&GLOB(lock));
	return returnval;
}

static char reload_usage[] = "Usage: SCCP reload [force|filename]\n" "       Reloads SCCP configuration from sccp.conf or optional [force|filename]\n" "       (It will send a reset to all device which have changed (when they have an active channel reset will be postponed until device goes onhook))\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "reload"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_reload, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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

	if (argc < 2 || argc > 4)
		return RESULT_SHOWUSAGE;

	pbx_cli(fd, "SCCP: Creating config file.\n");

	if (argc > 3) {
		pbx_cli(fd, "Using config file '%s'\n", argv[3]);
		config_file = sccp_strdupa(argv[3]);
	}
	if (sccp_config_generate(config_file, 0)) {
		pbx_cli(fd, "SCCP generated. saving '%s'...\n", config_file);
	} else {
		pbx_cli(fd, "SCCP generation failed.\n");
		returnval = RESULT_FAILURE;
	}

	return returnval;
#else
	pbx_cli(fd, "online SCCP config generate not implemented yet! use gen_sccpconf from contrib for now.\n");
	return RESULT_FAILURE;
#endif
}

static char config_generate_usage[] = "Usage: SCCP config generate [filename]\n" "       Generates a new sccp.conf if none exists. Either creating sccp.conf or [filename] if specified\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "config", "generate"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_config_generate, sccp_cli_config_generate, "Generate a SCCP configuration file", config_generate_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "version"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_show_version, sccp_show_version, "Show SCCP version details", show_version_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - devices in sccp_device_find_byid()
     *      - device
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

	pbx_cli(fd, VERBOSE_PREFIX_2 "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], FALSE);

	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	if (!d->session || d->registrationState != SKINNY_DEVICE_RS_OK) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		d = sccp_device_release(d);
		return RESULT_FAILURE;
	}
	/* sccp_device_clean will check active channels */
	/* \todo implement a check for active channels before sending reset */
	//if (d->channelCount > 0) {
	//      pbx_cli(fd, "%s: unable to %s device with active channels. Hangup first\n", argv[2], (!strcasecmp(argv[1], "reset")) ? "reset" : "restart");
	//      return RESULT_SUCCESS;
	//}
	if (!restart)
		sccp_device_sendReset(d, SKINNY_DEVICE_RESET);
	else
		sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);

	pthread_cancel(d->session->session_thread);

	d = sccp_device_release(d);
	return RESULT_SUCCESS;
}

/* --------------------------------------------------------------------------------------------------------------RESET- */
static char reset_usage[] = "Usage: SCCP reset\n" "       sccp reset <deviceId> [restart]\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "reset"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
CLI_ENTRY(cli_reset, sccp_reset_restart, "Show SCCP version details", reset_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -------------------------------------------------------------------------------------------------------------RESTART- */
static char restart_usage[] = "Usage: SCCP restart\n" "       sccp restart <deviceId>\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "restart"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
CLI_ENTRY(cli_restart, sccp_reset_restart, "Restart an SCCP device", restart_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - device
     */
static int sccp_unregister(int fd, int argc, char *argv[])
{
	sccp_moo_t *r;
	sccp_device_t *d;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	pbx_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	d = sccp_device_find_byid(argv[2], FALSE);
	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	if (!d->session) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		sccp_device_release(d);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);

	REQ(r, RegisterRejectMessage);
	strncpy(r->msg.RegisterRejectMessage.text, "Unregister user request", StationMaxDisplayTextSize);
	sccp_dev_send(d, r);

	d = sccp_device_release(d);
	return RESULT_SUCCESS;
}

static char unregister_usage[] = "Usage: SCCP unregister <deviceId>\n" "       Unregister an SCCP device\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "unregister"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
CLI_ENTRY(cli_unregister, sccp_unregister, "Unregister an SCCP device", unregister_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
	sccp_channel_t *channel = NULL;

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
		d = sccp_device_release(d);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "Starting Call for Device: %s\n", argv[2]);
	channel = sccp_channel_newcall(line, d, argv[3], SKINNY_CALLTYPE_OUTBOUND, NULL);

	line = sccp_line_release(line);
	d = sccp_device_release(d);
	channel = channel ? sccp_channel_release(channel) : NULL;

	return RESULT_SUCCESS;
}

static char start_call_usage[] = "Usage: sccp call <deviceId> <phone_number>\n" "Call number <number> using device <deviceId>\nIf number is ommitted, device will go off-Hook.\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "call"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
CLI_ENTRY(cli_start_call, sccp_start_call, "Call Number via Device", start_call_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - channel
     */
static int sccp_set_object(int fd, int argc, char *argv[])
{
	sccp_channel_t		*c 	= NULL;
	sccp_device_t 		*device = NULL;
	PBX_VARIABLE_TYPE 	variable;
	int res;
	
	
	if (argc < 5)
		return RESULT_SHOWUSAGE;
	if (pbx_strlen_zero(argv[3]) || pbx_strlen_zero(argv[4]))
		return RESULT_SHOWUSAGE;

	
	
	if (!strcmp("channel", argv[2])) {
		// sccp set channel SCCP/test-123 hold on
		if (!strncasecmp("SCCP/", argv[3], 5)) {
			int line, channel;

			sscanf(argv[3], "SCCP/%d-%d", &line, &channel);
			c = sccp_channel_find_byid(channel);
		} else {
			c = sccp_channel_find_byid(atoi(argv[3]));
		}
		
		if (!c) {
			pbx_cli(fd, "Can't find channel for ID %s\n", argv[3]);
			return RESULT_FAILURE;
		}
		
		
		if (!strcmp("hold", argv[4]) ){
		
			if (!strcmp("on", argv[5])) {										/* check to see if enable hold */
				pbx_cli(fd, "PLACING CHANNEL %s ON HOLD\n", argv[3]);
				sccp_channel_hold(c);
			} else if (!strcmp("off", argv[5])) {									/* check to see if disable hold */
				pbx_cli(fd, "PLACING CHANNEL %s OFF HOLD\n", argv[3]);
				sccp_device_t *d = sccp_channel_getDevice_retained(c);

				sccp_channel_resume(d, c, FALSE);
				d = sccp_device_release(d);
			} else {
				/* wrong parameter value */
				c = sccp_channel_release(c);
				return RESULT_SHOWUSAGE;
			}
		}
		c = sccp_channel_release(c);
	  
	} else if (!strcmp("device", argv[2])) {
		// sccp set device SEP00000 ringtone http://1234
		if (argc < 6){
			return RESULT_SHOWUSAGE;
		} 
		
		if (pbx_strlen_zero(argv[3]) || pbx_strlen_zero(argv[4]) || pbx_strlen_zero(argv[5]))
			return RESULT_SHOWUSAGE;
		}
	
		char *dev = sccp_strdupa(argv[3]);
		if (pbx_strlen_zero(dev)) {
			pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
// 			CLI_AMI_ERROR(fd, s, m, "DeviceName needs to be supplied %s\n", "");
		}
		device = sccp_device_find_byid(dev, FALSE);

		if (!device) {
			pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
// 			CLI_AMI_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);
			return RESULT_FAILURE;
		}
	
		if(!strcmp("ringtone", argv[4])){
			device->setRingTone(device, argv[5]);

		} else if(!strcmp("backgroundImage", argv[4])){
			device->setBackgroundImage(device, argv[5]);

		} else {
			variable.name	= argv[4];
			variable.value	= argv[5];
			variable.next	= NULL;
			variable.file	= "cli";
			variable.lineno	= 0;
		    
			res = sccp_config_applyDeviceConfiguration(device, &variable);
			
			if (res & SCCP_CONFIG_NEEDDEVICERESET){
				device->pendingUpdate = 1; 
			}
		}
		
		device = device ? sccp_device_release(device) : NULL;
	
	return RESULT_SUCCESS;
}

static char set_hold_usage[] = "Usage: sccp set [hold|device] <channelId> <on/off>\n" "Set a channel to hold/unhold\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "set"
#define CLI_COMPLETE SCCP_CLI_SET_COMPLETER
CLI_ENTRY(cli_set_hold, sccp_set_object, "Set channel|device to hold/unhold", set_hold_usage, TRUE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - channel
     */
static int sccp_remote_answer(int fd, int argc, char *argv[])
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;

	if (argc < 3)
		return RESULT_SHOWUSAGE;
	if (pbx_strlen_zero(argv[2]))
		return RESULT_SHOWUSAGE;

	if (!strncasecmp("SCCP/", argv[2], 5)) {
		int line, channel;

		sscanf(argv[2], "SCCP/%d-%d", &line, &channel);
		c = sccp_channel_find_byid(channel);
	} else {
		c = sccp_channel_find_byid(atoi(argv[2]));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[2]);
		return RESULT_FAILURE;
	} else {
		pbx_cli(fd, "ANSWERING CHANNEL %s \n", argv[2]);
		if ((d = sccp_channel_getDevice_retained(c))) {
			sccp_channel_answer(d, c);
			d = sccp_device_release(d);
		}
		if (c->owner) {
			PBX(queue_control) (c->owner, AST_CONTROL_ANSWER);
		}
		c = sccp_channel_release(c);
		return RESULT_SUCCESS;
	}
}

static char remote_answer_usage[] = "Usage: sccp answer <channelId>\n" "Answer a ringing/incoming channel\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "answer"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER
CLI_ENTRY(cli_remote_answer, sccp_remote_answer, "Answer a ringing/incoming channel", remote_answer_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
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
     *      - channel
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
		c = sccp_channel_find_byid(channel);
	} else {
		c = sccp_channel_find_byid(atoi(argv[2]));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	pbx_cli(fd, "ENDING CALL ON CHANNEL %s \n", argv[2]);
	sccp_channel_endcall(c);
	c = sccp_channel_release(c);
	return RESULT_SUCCESS;
}

static char end_call_usage[] = "Usage: sccp onhook <channelId>\n" "Hangup a channel\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "onhook"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER
CLI_ENTRY(cli_end_call, sccp_end_call, "Hangup a channel", end_call_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* ---------------------------------------------------------------------------------------------------------------------- TOKEN - */
    /*!
     * \brief Send Token Ack to device(s)
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
     *      - channel
     */
static int sccp_tokenack(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d;
	int local_total = 0;
	const char *dev;

	if (argc < 3 || sccp_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}	
	dev = sccp_strdupa(argv[2]);
	d = sccp_device_find_byid(dev, FALSE);
	if (!d) {
		pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
		CLI_AMI_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);
	}

	if (d->status.token != SCCP_TOKEN_STATE_REJ && d->session) {
		pbx_log(LOG_WARNING, "%s: We need to have received a token request before we can acknowledge it\n", dev);
		CLI_AMI_ERROR(fd, s, m, "%s: We need to have received a token request before we can acknowledge it\n", dev);
	} else {
		if (d->session) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending phone a token acknowledgement\n", dev);
			sccp_session_tokenAck(d->session);
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Phone not connected to this server (no valid session)\n", dev);
		}
	}
	sccp_device_release(d);
	if (s)
		*total = local_total;
	return RESULT_SUCCESS;
}

static char cli_tokenack_usage[] = "Usage: sccp tokenack <deviceId>\n" "Send Token Acknowlegde. Makes a phone switch servers on demand (used in clustering)\n";
static char ami_tokenack_usage[] = "Usage: SCCPTokenAck\n" "Send Token Acknowledge to device. Makes a phone switch servers on demand (used in clustering)\n\n" "PARAMS: DeviceName\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "tokenack"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
#define AMI_COMMAND "SCCPTokenAck"
#define CLI_AMI_PARAMS "DeviceName"
CLI_AMI_ENTRY(tokenack, sccp_tokenack, "Send TokenAck", cli_tokenack_usage, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* --- Register Cli Entries-------------------------------------------------------------------------------------------- */
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
	AST_CLI_DEFINE(cli_reload, "SCCP module reload."),
	AST_CLI_DEFINE(cli_restart, "Restart an SCCP device"),
	AST_CLI_DEFINE(cli_reset, "Reset an SCCP Device"),
	AST_CLI_DEFINE(cli_start_call, "Start a Call."),
	AST_CLI_DEFINE(cli_end_call, "End a Call."),
	AST_CLI_DEFINE(cli_set_hold, "Place call on hold."),
	AST_CLI_DEFINE(cli_remote_answer, "Remotely answer a call."),
#if defined(DEBUG) || defined(CS_EXPERIMENTAL)
	AST_CLI_DEFINE(cli_test_message, "Test message."),
	AST_CLI_DEFINE(cli_show_refcount, "Test message."),
#endif
	AST_CLI_DEFINE(cli_tokenack, "Send Token Acknowledgement."),
#ifdef CS_SCCP_CONFERENCE
	AST_CLI_DEFINE(cli_show_conferences, "Show running SCCP Conferences."),
	AST_CLI_DEFINE(cli_show_conference, "Show SCCP Conference Info."),
	AST_CLI_DEFINE(cli_conference_action, "Conference Actions.")
#endif
};

/*!
 * register CLI functions from asterisk
 */
void sccp_register_cli(void)
{
	int i, res = 0;

	for (i = 0; i < ARRAY_LEN(cli_entries); i++) {
		sccp_log(DEBUGCAT_CLI) (VERBOSE_PREFIX_2 "Cli registered action %s\n", (cli_entries + i)->_full_cmd);
		res |= pbx_cli_register(cli_entries + i);
	}

#if ASTERISK_VERSION_NUMBER < 10600
#define _MAN_COM_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND
#define _MAN_REP_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#else
#define _MAN_COM_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND
#define _MAN_REP_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING
#endif
	pbx_manager_register("SCCPShowGlobals", _MAN_REP_FLAGS, manager_show_globals, "show globals setting", ami_globals_usage);
	pbx_manager_register("SCCPShowDevices", _MAN_REP_FLAGS, manager_show_devices, "show devices", ami_devices_usage);
	pbx_manager_register("SCCPShowDevice", _MAN_REP_FLAGS, manager_show_device, "show device settings", ami_device_usage);
	pbx_manager_register("SCCPShowLines", _MAN_REP_FLAGS, manager_show_lines, "show lines", ami_lines_usage);
	pbx_manager_register("SCCPShowLine", _MAN_REP_FLAGS, manager_show_line, "show line", ami_line_usage);
	pbx_manager_register("SCCPShowChannels", _MAN_REP_FLAGS, manager_show_channels, "show channels", ami_channels_usage);
	pbx_manager_register("SCCPShowSessions", _MAN_REP_FLAGS, manager_show_sessions, "show sessions", ami_sessions_usage);
	pbx_manager_register("SCCPShowMWISubscriptions", _MAN_REP_FLAGS, manager_show_mwi_subscriptions, "show sessions", ami_mwi_subscriptions_usage);
	pbx_manager_register("SCCPShowSoftkeySets", _MAN_REP_FLAGS, manager_show_softkeysets, "show softkey sets", ami_show_softkeysets_usage);
	pbx_manager_register("SCCPMessageDevices", _MAN_REP_FLAGS, manager_message_devices, "message devices", ami_message_devices_usage);
	pbx_manager_register("SCCPMessageDevice", _MAN_REP_FLAGS, manager_message_device, "message device", ami_message_device_usage);
	pbx_manager_register("SCCPTokenAck", _MAN_REP_FLAGS, manager_tokenack, "send tokenack", ami_tokenack_usage);
#ifdef CS_SCCP_CONFERENCE
	pbx_manager_register("SCCPShowConferences", _MAN_REP_FLAGS, manager_show_conferences, "show conferences", ami_conferences_usage);
	pbx_manager_register("SCCPShowConference", _MAN_REP_FLAGS, manager_show_conference, "show conference", ami_conference_usage);
	pbx_manager_register("SCCPConference", _MAN_REP_FLAGS, manager_conference_action, "conference actions", ami_conference_action_usage);
#endif
}

/*!
 * unregister CLI functions from asterisk
 */
void sccp_unregister_cli(void)
{

	int i, res = 0;

	for (i = 0; i < ARRAY_LEN(cli_entries); i++) {
		sccp_log(DEBUGCAT_CLI) (VERBOSE_PREFIX_2 "Cli unregistered action %s\n", (cli_entries + i)->_full_cmd);
		res |= pbx_cli_unregister(cli_entries + i);
	}
	pbx_manager_unregister("SCCPShowGlobals");
	pbx_manager_unregister("SCCPShowDevice");
	pbx_manager_unregister("SCCPShowDevices");
	pbx_manager_unregister("SCCPShowLines");
	pbx_manager_unregister("SCCPShowLine");
	pbx_manager_unregister("SCCPShowChannels");
	pbx_manager_unregister("SCCPShowSessions");
	pbx_manager_unregister("SCCPShowMWISubscriptions");
	pbx_manager_unregister("SCCPShowSoftkeySets");
	pbx_manager_unregister("SCCPMessageDevices");
	pbx_manager_unregister("SCCPMessageDevice");
	pbx_manager_unregister("SCCPTokenAck");
#ifdef CS_SCCP_CONFERENCE
	pbx_manager_unregister("SCCPShowConferences");
	pbx_manager_unregister("SCCPShowConference");
	pbx_manager_unregister("SCCPConference");
#endif
}
