/*!
 * \file        sccp_cli.c
 * \brief       SCCP CLI Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

/*!
 * \remarks
 * Purpose:     SCCP CLI
 * When to use: Only methods directly related to the asterisk cli interface should be stored in this source file.
 * Relations:   Calls ???
 *
 * how to use the cli macro's
 * /code
 * static char cli_message_device_usage[] = "Usage: sccp message device <deviceId> <message text> [beep] [timeout]\n" "Send a message to an SCCP Device + phone beep + timeout.\n";
 * static char ami_message_device_usage[] = "Usage: SCCPMessageDevices\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: DeviceId, MessageText, Beep, Timeout\n";
 * \#define CLI_COMMAND "sccp", "message", "device"                                      // defines the cli command line before parameters
 * \#define AMI_COMMAND "SCCPMessageDevice"                                              // defines the ami command line before parameters
 * \#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER                                       // defines on or more cli tab completion helpers (in order)
 * \#define CLI_AMI_PARAMS "DeviceId" "MessageText", "Beep", "Timeout"                   // defines the ami parameter conversion mapping to argc/argv, empty string if not defined
 * CLI_AMI_ENTRY(message_device, sccp_message_device, "Send a message to SCCP Device", cli_message_device_usage, FALSE)
 *                                                                                      // the actual macro call which will generate an cli function and an ami function to be called. CLI_AMI_ENTRY elements:
 *                                                                                      // - functionname (will be expanded to manager_functionname and cli_functionname)
 *                                                                                      // - function to be called upon execution
 *                                                                                      // - description
 *                                                                                      // - usage
 *                                                                                      // - completer repeats indefinitly (multi calls to the completer, for example for 'sccp debug')
 * \#undef CLI_AMI_PARAMS
 * \#undef AMI_COMMAND
 * \#undef CLI_COMPLETE
 * \#undef CLI_COMMAND                                                                   // cleanup / undefine everything before the next call to CLI_AMI_ENTRY
 * \#endif
 * /endcode
 * 
 * Inside the function that which is called on execution:
 *  - If s!=NULL we know it is an AMI calls, if m!=NULL it is a CLI call.
 *  - we need to add local_total which get's set to the number of lines returned (for ami calls).
 *  - We need to return RESULT_SUCCESS (for cli calls) at the end. If we set CLI_AMI_RETURN_ERROR, we will exit the function immediately and return RESULT_FAILURE. We need to make sure that all references are released before sending CLI_AMI_RETURN_ERROR.
 *  .
 */
#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_cli.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_session.h"
#include "sccp_conference.h"
#include "sccp_utils.h"
#include "sccp_config.h"
#include "sccp_features.h"
#include "sccp_mwi.h"
#include "sccp_hint.h"
#include "sys/stat.h"
#include <asterisk/cli.h>
#include <asterisk/paths.h>

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
 */
static char *sccp_complete_device(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_device_t *d = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strncasecmp(word, d->id, wordlen) && ++which > state) {
			ret = pbx_strdup(d->id);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	return ret;
}

static char *sccp_complete_connected_device(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_device_t *d = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!strncasecmp(word, d->id, wordlen) && sccp_device_getRegistrationState(d) != SKINNY_DEVICE_RS_NONE && ++which > state) {
			ret = pbx_strdup(d->id);
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
 */
static char *sccp_complete_line(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strncasecmp(word, l->name, wordlen) && ++which > state) {
			ret = pbx_strdup(l->name);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return ret;
}

static char *sccp_complete_connected_line(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strncasecmp(word, l->name, wordlen) && SCCP_LIST_GETSIZE(&l->devices) > 0 && ++which > state) {
			ret = pbx_strdup(l->name);
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
 */
static char *sccp_complete_channel(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_line_t *l = NULL;
	sccp_channel_t *c = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		SCCP_LIST_LOCK(&l->channels);
		SCCP_LIST_TRAVERSE(&l->channels, c, list) {
			if (!strncasecmp(word, c->designator, wordlen) && ++which > state) {
				ret = pbx_strdup(c->designator);
				break;
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
		if (ret) {
			break;								// break out of outer look, prevent leaking memory by strdup
		}
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
	char *extra_cmds[] = { "no", "none", "off", "all" };

	// check if the sccp debug line contains no before the categories
	if (!strncasecmp(line, "sccp debug no ", strlen("sccp debug no "))) {
		debugno = 1;
	}
	// check extra_cmd
	for (i = 0; i < ARRAY_LEN(extra_cmds); i++) {
		if (!strncasecmp(word, extra_cmds[i], wordlen)) {
			// skip "no" and "none" if in debugno mode
			if (debugno && !strncasecmp("no", extra_cmds[i], strlen("no"))) {
				continue;
			}
			if (++which > state) {
				return pbx_strdup(extra_cmds[i]);
			}
		}
	}
	// check categories
	for (i =0; i < ARRAY_LEN(sccp_debug_categories); i++) {
		// if in debugno mode
		if (debugno) {
			// then skip the categories which are not currently active
			if ((GLOB(debug) & sccp_debug_categories[i].category) != sccp_debug_categories[i].category) {
				continue;
			}
		} else {
			// not debugno then skip the categories which are already active
			if ((GLOB(debug) & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
				continue;
			}
		}
		// find a match with partial category
		if (!strncasecmp(word, sccp_debug_categories[i].key, wordlen)) {
			if (++which > state) {
				return pbx_strdup(sccp_debug_categories[i].key);
			}
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
	sccp_device_t *d = NULL;
	sccp_channel_t *c = NULL;
	sccp_line_t *l = NULL;

	int wordlen = strlen(word), which = 0;
	char tmpname[80];
	char *ret = NULL;

	char *types[] = { "device", "channel", "line", "fallback", "debug" };

	char *properties_channel[] = { "hold" };
	char *properties_device[] = { "ringtone", "backgroundImage" };
	char *properties_fallback[] = { "true", "false", "odd", "even", "path" };

	char *values_hold[] = { "on", "off" };

	switch (pos) {
		case 2:											// type
			for (i = 0; i < ARRAY_LEN(types); i++) {
				if (!strncasecmp(word, types[i], wordlen) && ++which > state) {
					return pbx_strdup(types[i]);
				}
			}
			break;
		case 3:											// device / channel / line / fallback
			if (strstr(line, "device") != NULL) {
				SCCP_RWLIST_RDLOCK(&GLOB(devices));
				SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
					if (!strncasecmp(word, d->id, wordlen) && ++which > state) {
						ret = pbx_strdup(d->id);
						break;
					}
				}
				SCCP_RWLIST_UNLOCK(&GLOB(devices));

			} else if (strstr(line, "channel") != NULL) {
				SCCP_RWLIST_RDLOCK(&GLOB(lines));
				SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
					SCCP_LIST_LOCK(&l->channels);
					SCCP_LIST_TRAVERSE(&l->channels, c, list) {
						snprintf(tmpname, sizeof(tmpname), "SCCP/%s", c->designator);
						if (!strncasecmp(word, tmpname, wordlen) && ++which > state) {
							ret = pbx_strdup(tmpname);
							break;
						}
					}
					SCCP_LIST_UNLOCK(&l->channels);
					if (ret) {
						break;							// break out of outer look, prevent leaking memory by strdup
					}
				}
				SCCP_RWLIST_UNLOCK(&GLOB(lines));
			} else if (strstr(line, "fallback") != NULL) {
				for (i = 0; i < ARRAY_LEN(properties_fallback); i++) {
					if (!strncasecmp(word, properties_fallback[i], wordlen) && ++which > state) {
						return pbx_strdup(properties_fallback[i]);
					}
				}
			} else if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
		case 4:											// properties

			if (strstr(line, "device") != NULL) {
				for (i = 0; i < ARRAY_LEN(properties_device); i++) {
					if (!strncasecmp(word, properties_device[i], wordlen) && ++which > state) {
						return pbx_strdup(properties_device[i]);
					}
				}

			} else if (strstr(line, "channel") != NULL) {
				for (i = 0; i < ARRAY_LEN(properties_channel); i++) {
					if (!strncasecmp(word, properties_channel[i], wordlen) && ++which > state) {
						return pbx_strdup(properties_channel[i]);
					}
				}
			} else if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
		case 5:											// values_hold

			if (strstr(line, "channel") != NULL && strstr(line, "hold") != NULL) {
				for (i = 0; i < ARRAY_LEN(values_hold); i++) {
					if (!strncasecmp(word, values_hold[i], wordlen) && ++which > state) {
						return pbx_strdup(values_hold[i]);
					}
				}
			} else if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
		case 6:											// values_hold off device
			if (strstr(line, "channel") != NULL && strstr(line, "hold off") != NULL) {
				if (!strncasecmp(word, "device", wordlen) && ++which > state) {
					return pbx_strdup("device");
				}
			} else if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
		case 7:											// values_hold off device
			if (strstr(line, "channel") != NULL && strstr(line, "hold off") != NULL && strstr(line, "device") != NULL) {
				SCCP_RWLIST_RDLOCK(&GLOB(devices));
				SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
					if (!strncasecmp(word, d->id, wordlen) && ++which > state) {
						ret = pbx_strdup(d->id);
						break;
					}
				}
				SCCP_RWLIST_UNLOCK(&GLOB(devices));
			} else if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
		default:
			if (strstr(line, "debug") != NULL) {
				return sccp_complete_debug(line, word, pos, state);
			}
			break;
	}
	return ret;
}

static char *sccp_exec_completer(sccp_cli_completer_t completer, OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	char *completerStr;

	completerStr = NULL;
	switch (completer) {
		case SCCP_CLI_NULL_COMPLETER:
			completerStr = NULL;
			break;
		case SCCP_CLI_DEVICE_COMPLETER:
			completerStr = sccp_complete_device(line, word, pos, state);
			break;
		case SCCP_CLI_CONNECTED_DEVICE_COMPLETER:
			completerStr = sccp_complete_connected_device(line, word, pos, state);
			break;
		case SCCP_CLI_LINE_COMPLETER:
			completerStr = sccp_complete_line(line, word, pos, state);
			break;
		case SCCP_CLI_CONNECTED_LINE_COMPLETER:
			completerStr = sccp_complete_connected_line(line, word, pos, state);
			break;
		case SCCP_CLI_CHANNEL_COMPLETER:
			completerStr = sccp_complete_channel(line, word, pos, state);
			break;
		case SCCP_CLI_CONFERENCE_COMPLETER:
#ifdef CS_SCCP_CONFERENCE
			completerStr = sccp_complete_conference(line, word, pos, state);
#endif
			break;
		case SCCP_CLI_DEBUG_COMPLETER:
			completerStr = sccp_complete_debug(line, word, pos, state);
			break;
		case SCCP_CLI_SET_COMPLETER:
			completerStr = sccp_complete_set(line, word, pos, state);
			break;
	}
	return completerStr;
}

/* --- Support Functions ---------------------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------------SHOW GLOBALS- */

/*!
 * \brief Show Globals
 * \param fd Fd as int
 * \param totals Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 */
//static int sccp_show_globals(int fd, int argc, char *argv[])
static int sccp_show_globals(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	char pref_buf[256];
	pbx_str_t *callgroup_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

#ifdef CS_SCCP_PICKUP
	pbx_str_t *pickupgroup_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
#endif
	pbx_str_t *ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	pbx_str_t *ha_localnet_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	char *debugcategories;
	int local_line_total = 0;
	const char *actionid = "";

	pbx_rwlock_rdlock(&GLOB(lock));

	sccp_codec_multiple2str(pref_buf, sizeof(pref_buf) - 1, GLOB(global_preferences), ARRAY_LEN(GLOB(global_preferences)));
	debugcategories = sccp_get_debugcategories(GLOB(debug));
	sccp_print_ha(ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(ha));
	sccp_print_ha(ha_localnet_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(localaddr));

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver global settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_append(s, "Response: Success\r\n");
		astman_append(s, "Message: SCCPGlobalSettings\r\n");
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		local_line_total++;
	}
	CLI_AMI_OUTPUT_PARAM("Config File", CLI_AMI_LIST_WIDTH, "%s", GLOB(config_file_name));
#if defined(SCCP_LITTLE_ENDIAN) && defined(SCCP_BIG_ENDIAN)
	CLI_AMI_OUTPUT_PARAM("Platform byte order", CLI_AMI_LIST_WIDTH, "%s", "LITTLE/BIG ENDIAN");
#elif defined(SCCP_LITTLE_ENDIAN)
	CLI_AMI_OUTPUT_PARAM("Platform byte order", CLI_AMI_LIST_WIDTH, "%s", "LITTLE ENDIAN");
#else
	CLI_AMI_OUTPUT_PARAM("Platform byte order", CLI_AMI_LIST_WIDTH, "%s", "BIG ENDIAN");
#endif
	CLI_AMI_OUTPUT_PARAM("Server Name", CLI_AMI_LIST_WIDTH, "%s", GLOB(servername));
	CLI_AMI_OUTPUT_PARAM("Bind Address", CLI_AMI_LIST_WIDTH, "%s", sccp_netsock_stringify(&GLOB(bindaddr)));
	CLI_AMI_OUTPUT_PARAM("Extern IP", CLI_AMI_LIST_WIDTH, "%s", !sccp_netsock_is_any_addr(&GLOB(externip)) ? sccp_netsock_stringify_addr(&GLOB(externip)) : (GLOB(externhost) ? "Not Set -> using externhost" : "Not Set -> falling back to Incoming Interface IP-addres (expect issue if running natted !)."));
	CLI_AMI_OUTPUT_PARAM("Localnet", CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(ha_localnet_buf));
	CLI_AMI_OUTPUT_PARAM("Deny/Permit", CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(ha_buf));
	CLI_AMI_OUTPUT_BOOL("Direct RTP", CLI_AMI_LIST_WIDTH, GLOB(directrtp));
	CLI_AMI_OUTPUT_PARAM("Nat", CLI_AMI_LIST_WIDTH, "%s", sccp_nat2str(GLOB(nat)));
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
	CLI_AMI_OUTPUT_PARAM("AMA flags", CLI_AMI_LIST_WIDTH, "%d (%s)", GLOB(amaflags), pbx_channel_amaflags2string(GLOB(amaflags)));
	sccp_print_group(callgroup_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(callgroup));
	CLI_AMI_OUTPUT_PARAM("Callgroup", CLI_AMI_LIST_WIDTH, "%s", callgroup_buf ? pbx_str_buffer(callgroup_buf) : "");
#ifdef CS_SCCP_PICKUP
	sccp_print_group(pickupgroup_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(pickupgroup));
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
	CLI_AMI_OUTPUT_BOOL("DND Feature enabled", CLI_AMI_LIST_WIDTH, GLOB(dndFeature));
#ifdef CS_SCCP_PARK
	CLI_AMI_OUTPUT_BOOL("Park", CLI_AMI_LIST_WIDTH, FALSE);
#else
	CLI_AMI_OUTPUT_BOOL("Park", CLI_AMI_LIST_WIDTH, FALSE);
#endif
	CLI_AMI_OUTPUT_BOOL("Private softkey", CLI_AMI_LIST_WIDTH, GLOB(privacy));
	CLI_AMI_OUTPUT_BOOL("Echo cancel", CLI_AMI_LIST_WIDTH, GLOB(echocancel));
	CLI_AMI_OUTPUT_BOOL("Silence suppression", CLI_AMI_LIST_WIDTH, GLOB(silencesuppression));
	CLI_AMI_OUTPUT_BOOL("Trust phone ip (deprecated)", CLI_AMI_LIST_WIDTH, GLOB(trustphoneip));
	CLI_AMI_OUTPUT_PARAM("Early RTP", CLI_AMI_LIST_WIDTH, "%s", sccp_earlyrtp2str(GLOB(earlyrtp)));
	CLI_AMI_OUTPUT_PARAM("Ringtype", CLI_AMI_LIST_WIDTH, "%s", skinny_ringtype2str(GLOB(ringtype)));
	CLI_AMI_OUTPUT_PARAM("AutoAnswer ringtime", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_ring_time));
	CLI_AMI_OUTPUT_PARAM("AutoAnswer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(autoanswer_tone));
	CLI_AMI_OUTPUT_PARAM("RemoteHangup tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(remotehangup_tone));
	CLI_AMI_OUTPUT_PARAM("Transfer tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(transfer_tone));
	CLI_AMI_OUTPUT_BOOL("Transfer on hangup", CLI_AMI_LIST_WIDTH, GLOB(transfer_on_hangup));
	CLI_AMI_OUTPUT_PARAM("Callwaiting tone", CLI_AMI_LIST_WIDTH, "%d", GLOB(callwaiting_tone));
	CLI_AMI_OUTPUT_PARAM("Callwaiting interval", CLI_AMI_LIST_WIDTH, "%d", GLOB(callwaiting_interval));
	CLI_AMI_OUTPUT_PARAM("Registration Context", CLI_AMI_LIST_WIDTH, "%s", GLOB(regcontext) ? GLOB(regcontext) : "Unset");
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer enabled ", CLI_AMI_LIST_WIDTH, pbx_test_flag(GLOB(global_jbconf), AST_JB_ENABLED));
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer forced ", CLI_AMI_LIST_WIDTH, pbx_test_flag(GLOB(global_jbconf), AST_JB_FORCED));
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer max size", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf)->max_size);
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer resync", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf)->resync_threshold);
	CLI_AMI_OUTPUT_PARAM("Jitterbuffer impl", CLI_AMI_LIST_WIDTH, "%s", GLOB(global_jbconf)->impl);
	CLI_AMI_OUTPUT_BOOL("Jitterbuffer log  ", CLI_AMI_LIST_WIDTH, pbx_test_flag(GLOB(global_jbconf), AST_JB_LOG));
#ifdef CS_AST_JB_TARGET_EXTRA
	CLI_AMI_OUTPUT_PARAM("Jitterbuf target extra", CLI_AMI_LIST_WIDTH, "%ld", GLOB(global_jbconf)->target_extra);
#endif
	CLI_AMI_OUTPUT_PARAM("Token FallBack", CLI_AMI_LIST_WIDTH, "%s", GLOB(token_fallback));
	CLI_AMI_OUTPUT_PARAM("Token Backoff-Time", CLI_AMI_LIST_WIDTH, "%d", GLOB(token_backoff_time));
	CLI_AMI_OUTPUT_BOOL("Hotline_Enabled", CLI_AMI_LIST_WIDTH, GLOB(allowAnonymous));
	CLI_AMI_OUTPUT_PARAM("Hotline_Context", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline)->line->context ? GLOB(hotline)->line->context : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Hotline_Exten", CLI_AMI_LIST_WIDTH, "%s", GLOB(hotline->exten));
	CLI_AMI_OUTPUT_PARAM("Threadpool Size", CLI_AMI_LIST_WIDTH, "%d/%d", sccp_threadpool_jobqueue_count(GLOB(general_threadpool)), sccp_threadpool_thread_count(GLOB(general_threadpool)));

	if (sccp_netsock_is_any_addr(&GLOB(externip)) && GLOB(externhost)) {
		struct sockaddr_storage externip;
		boolean_t lookup_success = sccp_netsock_getExternalAddr(&externip, sccp_netsock_is_IPv6(&GLOB(bindaddr)) ? AF_INET6 : AF_INET);
		CLI_AMI_OUTPUT_PARAM("Extern Host", CLI_AMI_LIST_WIDTH, "%s -> %s", GLOB(externhost), lookup_success ? sccp_netsock_stringify_addr(&externip) : "Resolve Failed!");
		CLI_AMI_OUTPUT_PARAM("Extern Refresh", CLI_AMI_LIST_WIDTH, "%d", GLOB(externrefresh));
	}

	sccp_free(debugcategories);
	pbx_rwlock_unlock(&GLOB(lock));

	if (s) {
		totals->lines = local_line_total;
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
CLI_AMI_ENTRY(show_globals, sccp_show_globals, "List defined SCCP global settings", cli_globals_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* --------------------------------------------------------------------------------------------------------SHOW DEVICES- */
    /*!
     * \brief Show Devices
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
    //static int sccp_show_devices(int fd, int argc, char *argv[])
static int sccp_show_devices(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	struct tm *timeinfo;
	char regtime[25];
	int local_line_total = 0;
	char addrStr[INET6_ADDRSTRLEN];

	// table definition
#define CLI_AMI_TABLE_NAME Devices
#define CLI_AMI_TABLE_PER_ENTRY_NAME Device

#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_device_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(devices)
#define CLI_AMI_TABLE_LIST_ITER_VAR list_dev
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 																\
	{																			\
		AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(list_dev));											\
		if (d) {																	\
			if(d->session) {															\
				struct sockaddr_storage sas = { 0 };												\
				timeinfo = localtime(&d->registrationTime); 											\
				strftime(regtime, sizeof(regtime), "%c ", timeinfo);										\
				sccp_session_getSas(d->session, &sas);											 	\
				sccp_copy_string(addrStr,sccp_netsock_stringify(&sas),sizeof(addrStr));								\
 			} else {addrStr[0] = '-'; addrStr[1] = '-';addrStr[2] = '\0';regtime[0] = '\0';}

#define CLI_AMI_TABLE_AFTER_ITERATION 																\
		}																		\
	}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 																	\
		CLI_AMI_TABLE_FIELD(Descr,		"-25.25",	s,	25,	d->description ? d->description : "<not set>")				\
		CLI_AMI_TABLE_FIELD(Address,		"44.44",	s,	44,	addrStr)								\
		CLI_AMI_TABLE_FIELD(Mac,		"-16.16",	s,	16,	d->id)									\
		CLI_AMI_TABLE_FIELD(RegState,		"-10.10",	s,	10, 	skinny_registrationstate2str(sccp_device_getRegistrationState(d)))	\
		CLI_AMI_TABLE_FIELD(Token,		"-5.5",		s,	5,	sccp_tokenstate2str(d->status.token)) 					\
		CLI_AMI_TABLE_FIELD(RegTime,		"25.25",	s,	25, 	regtime[0] ? regtime : "None")						\
		CLI_AMI_TABLE_FIELD(Act,		"3.3",		s,	3, 	(d->active_channel) ? "Yes" : "No")					\
		CLI_AMI_TABLE_FIELD(Lines, 		"-5",		d,	5, 	d->configurationStatistic.numberOfLines)				\
		CLI_AMI_TABLE_FIELD(Nat,		"9.9",		s, 	9,	sccp_nat2str(d->nat))
#include "sccp_cli_table.h"

	// end of table definition
	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

static char cli_devices_usage[] = "Usage: sccp show devices\n" "       Lists defined SCCP devices.\n";
static char ami_devices_usage[] = "Usage: SCCPShowDevices\n" "Lists defined SCCP devices.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "devices"
#define AMI_COMMAND "SCCPShowDevices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_devices, sccp_show_devices, "List defined SCCP devices", cli_devices_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* --------------------------------------------------------------------------------------------------------SHOW DEVICE- */
    /*!
     * \brief Show Device
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     * \warning
     *   - device->buttonconfig is not always locked
     */
static int sccp_show_device(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	char pref_buf[256];
	char cap_buf[512];
	pbx_str_t *ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	pbx_str_t *permithost_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	PBX_VARIABLE_TYPE *v = NULL;
	int local_line_total = 0;
	int local_table_total = 0;
	const char *actionid = "";
	char clientAddress[INET6_ADDRSTRLEN];
	char serverAddress[INET6_ADDRSTRLEN];

	const char *dev;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
		CLI_AMI_RETURN_ERROR(fd, s, m, "DeviceName needs to be supplied %s\n", "");		/* explicit return */
	}
	dev = pbx_strdupa(argv[3]);
	if (pbx_strlen_zero(dev)) {
		pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
		CLI_AMI_RETURN_ERROR(fd, s, m, "DeviceName needs to be supplied %s\n", "");		/* explicit return */
	}
	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(dev, FALSE));

	if (!d) {
		pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
		CLI_AMI_RETURN_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);		/* explicit return */
	}
	sccp_codec_multiple2str(pref_buf, sizeof(pref_buf) - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
	sccp_codec_multiple2str(cap_buf, sizeof(cap_buf) - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
	sccp_print_ha(ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, d->ha);

	if (d->session) {
		struct sockaddr_storage sas = { 0 };
		sccp_session_getSas(d->session, &sas);
		sccp_copy_string(clientAddress, sccp_netsock_stringify(&sas), sizeof(clientAddress));
		struct sockaddr_storage ourip = { 0 };
		sccp_session_getOurIP(d->session, &ourip, 0);
		sccp_copy_string(serverAddress, sccp_netsock_stringify(&ourip), sizeof(serverAddress));
	} else {
		snprintf(clientAddress, sizeof(clientAddress), "%s", "???.???.???.???");
		snprintf(serverAddress, sizeof(serverAddress), "%s", "???.???.???.???");
	}

	sccp_hostname_t *hostname;
	sccp_accessory_t activeAccessory = sccp_device_getActiveAccessory(d);
	sccp_accessorystate_t activeAccessoryState = sccp_device_getAccessoryStatus(d, activeAccessory);

	SCCP_LIST_TRAVERSE(&d->permithosts, hostname, list) {
		ast_str_append(&permithost_buf, DEFAULT_PBX_STR_BUFFERSIZE, "%s ", hostname->name);
	}

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver device settings ----------------------------------------------------------------------------------\n");
	} else {
		astman_append(s, "Event: SCCPShowDevice\r\n");
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		local_line_total++;
	}
	/* *INDENT-OFF* */
	CLI_AMI_OUTPUT_PARAM("MAC-Address",		CLI_AMI_LIST_WIDTH, "%s", d->id);
	CLI_AMI_OUTPUT_PARAM("Protocol Version",	CLI_AMI_LIST_WIDTH, "Supported '%d', In Use '%d'", d->protocolversion, d->inuseprotocolversion);
	CLI_AMI_OUTPUT_PARAM("Protocol In Use",		CLI_AMI_LIST_WIDTH, "%s Version %d", d->protocol ? (d->protocol->type == SCCP_PROTOCOL ? "SCCP" : "SPCP" ) : "NONE", d->protocol ? d->protocol->version : 0);
	char binstr[41] = "";
	CLI_AMI_OUTPUT_PARAM("Device Features",		CLI_AMI_LIST_WIDTH, "%#1x,%s", d->device_features, sccp_dec2binstr(binstr, 40, d->device_features));
	CLI_AMI_OUTPUT_PARAM("Tokenstate",		CLI_AMI_LIST_WIDTH, "%s", sccp_tokenstate2str(d->status.token));
	CLI_AMI_OUTPUT_PARAM("Keepalive",		CLI_AMI_LIST_WIDTH, "%d", d->keepalive);
	CLI_AMI_OUTPUT_PARAM("Registration state",	CLI_AMI_LIST_WIDTH, "%s", skinny_registrationstate2str(sccp_device_getRegistrationState(d)));
	CLI_AMI_OUTPUT_PARAM("State",			CLI_AMI_LIST_WIDTH, "%s", sccp_devicestate2str(sccp_device_getDeviceState(d)));
	CLI_AMI_OUTPUT_PARAM("MWI light",		CLI_AMI_LIST_WIDTH, "%s(%d)", skinny_lampmode2str(d->mwilamp), d->mwilamp);
	CLI_AMI_OUTPUT_PARAM("MWI handset light", 	CLI_AMI_LIST_WIDTH, "%s", sccp_dec2binstr(binstr, 40, d->mwilight));
	CLI_AMI_OUTPUT_PARAM("MWI During call",		CLI_AMI_LIST_WIDTH, "%s", d->mwioncall ? "keep on" : "turn off");
	CLI_AMI_OUTPUT_PARAM("Description",		CLI_AMI_LIST_WIDTH, "%s", d->description ? d->description : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Config Phone Type",	CLI_AMI_LIST_WIDTH, "%s", d->config_type);
	CLI_AMI_OUTPUT_PARAM("Skinny Phone Type",	CLI_AMI_LIST_WIDTH, "%s(%d)", skinny_devicetype2str(d->skinny_type), d->skinny_type);
	CLI_AMI_OUTPUT_YES_NO("Softkey support",	CLI_AMI_LIST_WIDTH, d->softkeysupport);
	CLI_AMI_OUTPUT_PARAM("Softkeyset",		CLI_AMI_LIST_WIDTH, "%s => %s (%p)", d->softkeyDefinition, d->softkeyset ? d->softkeyset->name : "NULL !", d->softkeyset);
	CLI_AMI_OUTPUT_YES_NO("BTemplate support",	CLI_AMI_LIST_WIDTH, d->buttonTemplate);
	CLI_AMI_OUTPUT_YES_NO("linesRegistered",	CLI_AMI_LIST_WIDTH, d->linesRegistered);
	CLI_AMI_OUTPUT_PARAM("Image Version",		CLI_AMI_LIST_WIDTH, "%s", d->loadedimageversion);
	CLI_AMI_OUTPUT_PARAM("Timezone Offset",		CLI_AMI_LIST_WIDTH, "%d", d->tz_offset);
	CLI_AMI_OUTPUT_PARAM("Capabilities",		CLI_AMI_LIST_WIDTH, "%s", cap_buf);
	CLI_AMI_OUTPUT_PARAM("Codecs preference",	CLI_AMI_LIST_WIDTH, "%s", pref_buf);
	CLI_AMI_OUTPUT_PARAM("Audio TOS",		CLI_AMI_LIST_WIDTH, "%d", d->audio_tos);
	CLI_AMI_OUTPUT_PARAM("Audio COS",		CLI_AMI_LIST_WIDTH, "%d", d->audio_cos);
	CLI_AMI_OUTPUT_PARAM("Video TOS",		CLI_AMI_LIST_WIDTH, "%d", d->video_tos);
	CLI_AMI_OUTPUT_PARAM("Video COS",		CLI_AMI_LIST_WIDTH, "%d", d->video_cos);
	CLI_AMI_OUTPUT_YES_NO("DND Feature enabled",	CLI_AMI_LIST_WIDTH, d->dndFeature.enabled);
	CLI_AMI_OUTPUT_PARAM("DND Status",		CLI_AMI_LIST_WIDTH, "%s", (d->dndFeature.status) ? sccp_dndmode2str(d->dndFeature.status) : "Disabled");
	CLI_AMI_OUTPUT_PARAM("DND Action", 		CLI_AMI_LIST_WIDTH, "%s", (d->dndmode) ? sccp_dndmode2str(d->dndmode) : "Disabled / Cycle");
	CLI_AMI_OUTPUT_BOOL("Can Transfer",		CLI_AMI_LIST_WIDTH, d->transfer);
	CLI_AMI_OUTPUT_BOOL("Can Park",			CLI_AMI_LIST_WIDTH, d->park);
	CLI_AMI_OUTPUT_BOOL("Can CFWDALL",		CLI_AMI_LIST_WIDTH, d->cfwdall);
	CLI_AMI_OUTPUT_BOOL("Can CFWBUSY",		CLI_AMI_LIST_WIDTH, d->cfwdbusy);
	CLI_AMI_OUTPUT_BOOL("Can CFWNOANSWER",		CLI_AMI_LIST_WIDTH, d->cfwdnoanswer);
	CLI_AMI_OUTPUT_YES_NO("Allow ringin notification", CLI_AMI_LIST_WIDTH, d->allowRinginNotification);
	CLI_AMI_OUTPUT_BOOL("Private softkey",		CLI_AMI_LIST_WIDTH, d->privacyFeature.enabled);
	CLI_AMI_OUTPUT_PARAM("Dtmf mode",		CLI_AMI_LIST_WIDTH, "%s", sccp_dtmfmode2str(d->getDtmfMode(d)));
//	CLI_AMI_OUTPUT_PARAM("digit timeout",		CLI_AMI_LIST_WIDTH, "%d", d->digittimeout);
	CLI_AMI_OUTPUT_PARAM("Nat",			CLI_AMI_LIST_WIDTH, "%s", sccp_nat2str(d->nat));
	CLI_AMI_OUTPUT_YES_NO("Videosupport?",		CLI_AMI_LIST_WIDTH, sccp_device_isVideoSupported(d));
	CLI_AMI_OUTPUT_BOOL("Direct RTP",		CLI_AMI_LIST_WIDTH, d->directrtp);
	CLI_AMI_OUTPUT_BOOL("Trust phone ip (deprecated)", CLI_AMI_LIST_WIDTH, d->trustphoneip);
	CLI_AMI_OUTPUT_PARAM("Bind Address",		CLI_AMI_LIST_WIDTH, "%s", clientAddress);
	CLI_AMI_OUTPUT_PARAM("Server Address",		CLI_AMI_LIST_WIDTH, "%s", serverAddress);
	CLI_AMI_OUTPUT_PARAM("Deny/Permit",		CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(ha_buf));
	CLI_AMI_OUTPUT_PARAM("PermitHosts",		CLI_AMI_LIST_WIDTH, "%s", pbx_str_buffer(permithost_buf));
	CLI_AMI_OUTPUT_PARAM("Early RTP",		CLI_AMI_LIST_WIDTH, "%s", sccp_earlyrtp2str(d->earlyrtp));
	CLI_AMI_OUTPUT_PARAM("Accessory State", 	CLI_AMI_LIST_WIDTH, "%s%s%s", activeAccessory ? sccp_accessory2str(activeAccessory) : "", activeAccessory ? ":" : "", sccp_accessorystate2str(activeAccessoryState));
	CLI_AMI_OUTPUT_PARAM("Last dialed number",	CLI_AMI_LIST_WIDTH, "%s (%d)", d->redialInformation.number, d->redialInformation.lineInstance);
	CLI_AMI_OUTPUT_PARAM("Default line instance",	CLI_AMI_LIST_WIDTH, "%d", d->defaultLineInstance);
	CLI_AMI_OUTPUT_PARAM("Custom Background Image",	CLI_AMI_LIST_WIDTH, "%s", d->backgroundImage ? d->backgroundImage : "---");
	CLI_AMI_OUTPUT_PARAM("Custom Ring Tone",	CLI_AMI_LIST_WIDTH, "%s", d->ringtone ? d->ringtone : "---");
	CLI_AMI_OUTPUT_BOOL("Use Placed Calls",		CLI_AMI_LIST_WIDTH, d->useRedialMenu);
	CLI_AMI_OUTPUT_BOOL("PendingUpdate",		CLI_AMI_LIST_WIDTH, d->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("PendingDelete",		CLI_AMI_LIST_WIDTH, d->pendingDelete);
#ifdef CS_SCCP_PICKUP
	CLI_AMI_OUTPUT_BOOL("Directed Pickup",		CLI_AMI_LIST_WIDTH, d->directed_pickup);
	CLI_AMI_OUTPUT_PARAM("Pickup Context",		CLI_AMI_LIST_WIDTH, "%s (%s)", d->directed_pickup_context, pbx_context_find(d->directed_pickup_context) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_BOOL("Pickup Mode Answer",	CLI_AMI_LIST_WIDTH, d->directed_pickup_modeanswer);
#endif
#ifdef CS_SCCP_CONFERENCE
	CLI_AMI_OUTPUT_BOOL("allow_conference",		CLI_AMI_LIST_WIDTH, d->allow_conference);
	CLI_AMI_OUTPUT_BOOL("conf_play_general_announce", CLI_AMI_LIST_WIDTH, d->conf_play_general_announce);
	CLI_AMI_OUTPUT_BOOL("conf_play_part_announce",	CLI_AMI_LIST_WIDTH, d->conf_play_part_announce);
	CLI_AMI_OUTPUT_BOOL("conf_mute_on_entry",	CLI_AMI_LIST_WIDTH, d->conf_mute_on_entry);
	CLI_AMI_OUTPUT_PARAM("conf_music_on_hold_class",CLI_AMI_LIST_WIDTH, "%s", d->conf_music_on_hold_class);
	CLI_AMI_OUTPUT_BOOL("conf_show_conflist",       CLI_AMI_LIST_WIDTH, d->conf_show_conflist);
	CLI_AMI_OUTPUT_BOOL("conflist_active",		CLI_AMI_LIST_WIDTH, d->conferencelist_active);
#endif
	if (s) {
		astman_append(s, "\r\n");
	}

	/* *INDENT-ON* */
	if (SCCP_LIST_FIRST(&d->buttonconfig)) {
		// BUTTONS
#define CLI_AMI_TABLE_NAME Buttons
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceButton
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_buttonconfig_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_FIELDS 																\
			CLI_AMI_TABLE_FIELD(Id,			"-4",	d,		4,	buttonconfig->index + 1)				\
			CLI_AMI_TABLE_FIELD(Inst,		"-4",	d,		4,	buttonconfig->instance)					\
			CLI_AMI_TABLE_FIELD(TypeStr,		"-40",	s,		40,	sccp_config_buttontype2str(buttonconfig->type))		\
			CLI_AMI_TABLE_FIELD(Type,		"-37",	d,		37,	buttonconfig->type)					\
			CLI_AMI_TABLE_FIELD(pendUpdt,		"-8",	s,		8, 	buttonconfig->pendingUpdate ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(pendDel,		"-8",	s,		8, 	buttonconfig->pendingUpdate ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(Default,		"-9",	s,		9,	(0!=buttonconfig->instance && d->defaultLineInstance == buttonconfig->instance && LINE==buttonconfig->type) ? "Yes" : "No")
#include "sccp_cli_table.h"
			local_table_total++;
		// LINES
#define CLI_AMI_TABLE_NAME LineButtons
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceLine
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 															\
			if (buttonconfig->type == LINE) {												\
				AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byname(buttonconfig->button.line.name, FALSE));				\
				char subscriptionIdBuf[21] = "";											\
				if (buttonconfig->button.line.subscriptionId) {										\
					snprintf(subscriptionIdBuf, 21, "(%s)%s:%s", buttonconfig->button.line.subscriptionId->replaceCid ? "=" : "+", buttonconfig->button.line.subscriptionId->number, buttonconfig->button.line.subscriptionId->name);	\
				}															\
				if (l) {														\
					AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(d, l));
					
#define CLI_AMI_TABLE_AFTER_ITERATION 															\
				}															\
			}

#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 																\
			CLI_AMI_TABLE_FIELD(Id,			"-4",		d,	4,	buttonconfig->index + 1)				\
			CLI_AMI_TABLE_FIELD(Name,		"-23.23",	s,	23,	l->name)						\
			CLI_AMI_TABLE_FIELD(SubCid,		"-21.21",	s,	21,	subscriptionIdBuf)					\
			CLI_AMI_TABLE_FIELD(Label,		"-37.37",	s,	37, 	buttonconfig->button.line.subscriptionId ? buttonconfig->button.line.subscriptionId->label : (l->label ? l->label : ""))	\
			CLI_AMI_TABLE_FIELD(CfwdType,		"-10",		s,	10, 	(linedevice && linedevice->cfwdAll.enabled ? "All" : (linedevice && linedevice->cfwdBusy.enabled ? "Busy" : "None")))	\
			CLI_AMI_TABLE_FIELD(CfwdNumber,		"16.16",	s,	16, 	(linedevice && linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : (linedevice && linedevice->cfwdBusy.enabled ? linedevice->cfwdBusy.number : "")))
#include "sccp_cli_table.h"
			local_table_total++;

		// SPEEDDIALS
#define CLI_AMI_TABLE_NAME SpeeddialButtons
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceSpeeddial
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 														\
			if (buttonconfig->type == SPEEDDIAL) {											
			
#define CLI_AMI_TABLE_AFTER_ITERATION 														\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 															\
			CLI_AMI_TABLE_FIELD(Id,			"-4",		d,	4,	buttonconfig->index + 1)			\
			CLI_AMI_TABLE_FIELD(Name,		"-23.23",	s,	23,	buttonconfig->label ? buttonconfig->label : "")				\
			CLI_AMI_TABLE_FIELD(Number,		"-22.22",	s,	22,	buttonconfig->button.speeddial.ext ? buttonconfig->button.speeddial.ext : "")		\
			CLI_AMI_TABLE_FIELD(Hint,		"-64.64",	s,	64, 	buttonconfig->button.speeddial.hint ? buttonconfig->button.speeddial.hint : "")
#include "sccp_cli_table.h"
			local_table_total++;

		// FEATURES
#define CLI_AMI_TABLE_NAME FeatureButtons
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceFeature
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 														\
			if (buttonconfig->type == FEATURE) {
#define CLI_AMI_TABLE_AFTER_ITERATION 														\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 															\
			CLI_AMI_TABLE_FIELD(Id,			"-4",		d,	4,	buttonconfig->index + 1)			\
			CLI_AMI_TABLE_FIELD(Name,		"-23.23",	s,	23,	buttonconfig->label ? buttonconfig->label : "")				\
			CLI_AMI_TABLE_FIELD(Status,		"-22",		d,	22, 	buttonconfig->button.feature.status)		\
			CLI_AMI_TABLE_FIELD(Options,		"-64.64",	s,	64,	buttonconfig->button.feature.options ? buttonconfig->button.feature.options : "")
#include "sccp_cli_table.h"
			local_table_total++;

		// SERVICEURL
#define CLI_AMI_TABLE_NAME ServiceURLButtons
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceServiceURL
#define CLI_AMI_TABLE_LIST_ITER_HEAD &d->buttonconfig
#define CLI_AMI_TABLE_LIST_ITER_VAR buttonconfig
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 														\
			if (buttonconfig->type == SERVICE) {
#define CLI_AMI_TABLE_AFTER_ITERATION 														\
			}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 															\
			CLI_AMI_TABLE_FIELD(Id,			"-4",		d,	4,	buttonconfig->index + 1)			\
			CLI_AMI_TABLE_FIELD(Name,		"-22.22",	s,	22,	buttonconfig->label ? buttonconfig->label : "")				\
			CLI_AMI_TABLE_FIELD(URL,		"-88.88",	s,	88,	buttonconfig->button.service.url ? buttonconfig->button.service.url : "")
#include "sccp_cli_table.h"
			local_table_total++;
	}

	if (d->variables) {
		// VARIABLES
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = d->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 															\
			CLI_AMI_TABLE_FIELD(Name,		"-28.28",	s,	28,	v->name)					\
			CLI_AMI_TABLE_FIELD(Value,		"-87.87",	s,	87,	v->value)
#include "sccp_cli_table.h"
			local_table_total++;
	}

	sccp_call_statistics_type_t callstattype;
	sccp_call_statistics_t *stats = NULL;

#define CLI_AMI_TABLE_NAME CallStatistics
#define CLI_AMI_TABLE_PER_ENTRY_NAME DeviceStatistics
#define CLI_AMI_TABLE_ITERATOR for(callstattype = SCCP_CALLSTATISTIC_LAST; callstattype <= SCCP_CALLSTATISTIC_AVG; callstattype++)
#define CLI_AMI_TABLE_BEFORE_ITERATION stats = &d->call_statistics[callstattype];
#define CLI_AMI_TABLE_FIELDS																	\
			CLI_AMI_TABLE_FIELD(Type,		"-8.8",		s,	8,	(callstattype == SCCP_CALLSTATISTIC_LAST) ? "LAST" : "AVG")	\
			CLI_AMI_TABLE_FIELD(Calls,		"-8",		d,	8,	stats->num)							\
			CLI_AMI_TABLE_FIELD(PcktSnt,		"-8",		d,	8,	stats->packets_sent)						\
			CLI_AMI_TABLE_FIELD(PcktRcvd,		"-8",		d,	8,	stats->packets_received)					\
			CLI_AMI_TABLE_FIELD(Lost,		"-8",		d,	8,	stats->packets_lost)						\
			CLI_AMI_TABLE_FIELD(Jitter,		"-8",		d,	8,	stats->jitter)							\
			CLI_AMI_TABLE_FIELD(Latency,		"-8",		d,	8,	stats->latency)							\
			CLI_AMI_TABLE_FIELD(Quality,		"1.6",		f,	8,	stats->opinion_score_listening_quality)				\
			CLI_AMI_TABLE_FIELD(avgQual,		"1.6",		f,	8,	stats->avg_opinion_score_listening_quality)			\
			CLI_AMI_TABLE_FIELD(meanQual,		"1.6",		f,	8,	stats->mean_opinion_score_listening_quality)			\
			CLI_AMI_TABLE_FIELD(maxQual,		"1.6",		f,	8,	stats->max_opinion_score_listening_quality)			\
			CLI_AMI_TABLE_FIELD(rConceal,		"1.6",		f,	8,	stats->cumulative_concealement_ratio)				\
			CLI_AMI_TABLE_FIELD(sConceal,		"-8",		d,	8,	stats->concealed_seconds)
#include "sccp_cli_table.h"
			local_table_total++;

	if (s) {
		totals->lines = local_line_total;
		totals->tables = local_table_total;
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
CLI_AMI_ENTRY(show_device, sccp_show_device, "Lists device settings", cli_device_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* ---------------------------------------------------------------------------------------------------------SHOW LINES- */
    /*!
     * \brief Show Lines
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
    //static int sccp_show_lines(int fd, int argc, char *argv[])
static int sccp_show_lines(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *l = NULL;
	boolean_t found_linedevice;
	sccp_linedevices_t *linedevice = NULL;
	sccp_channel_t *channel = NULL;
	char cap_buf[512] = {0};
	PBX_VARIABLE_TYPE *v = NULL;
	int local_line_total = 0;
	const char *actionid = "";

	if (!s) {
		pbx_cli(fd, "\n+--- Lines ------------------------------------------------------------------------------------------------------------------------------------------------------+\n");
		pbx_cli(fd, "| %-13s %-9s %-30s %-16s %-16s %-4s %-4s %-59s |\n", "Ext", "Suffix", "Label", "Description", "Device", "MWI", "Chs", "Active Channel");
		pbx_cli(fd, "+ ============= ========= ============================== ================ ================ ==== ==== =========================================================== +\n");
	} else {
		astman_append(s, "Event: TableStart\r\n");
		local_line_total++;
		astman_append(s, "TableName: Lines\r\n");
		local_line_total++;
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		astman_append(s, "\r\n");
		local_line_total++;
	}
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		found_linedevice = 0;
		channel = NULL;
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
			AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(linedevice->device));
			if (d) {
				memset(&cap_buf, 0, sizeof(cap_buf));
				char cid_name[StationMaxNameSize] = {0};
				skinny_calltype_t calltype = SKINNY_CALLTYPE_SENTINEL;
				sccp_channelstate_t state = SCCP_CHANNELSTATE_SENTINEL;
				
				SCCP_LIST_LOCK(&l->channels);
				SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
					//if (channel && (channel->state != SCCP_CHANNELSTATE_CONNECTED || sccp_strequals(channel->currentDeviceId, d->id))) {
					if (channel && (channel->state == SCCP_CHANNELSTATE_HOLD || sccp_strequals(channel->currentDeviceId, d->id))) {
						if (channel->owner) {
							pbx_getformatname_multiple(cap_buf, sizeof(cap_buf), pbx_channel_nativeformats(channel->owner));
						}
						if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
							iCallInfo.Getter(sccp_channel_getCallInfo(channel), 
								SCCP_CALLINFO_CALLEDPARTY_NAME, &cid_name,
								SCCP_CALLINFO_KEY_SENTINEL);
						} else {
							iCallInfo.Getter(sccp_channel_getCallInfo(channel), 
								SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name,
								SCCP_CALLINFO_KEY_SENTINEL);
						}
						calltype = channel->calltype;
						state = channel->state;
						break;
					}
				}
				SCCP_LIST_UNLOCK(&l->channels);
				if (!s) {
					pbx_cli(fd, "| %-13s %-3s%-6s %-30s %-16s %-16s %-4s %-4d %-10s %-10s %-26.26s %-10s |\n",
						!found_linedevice ? l->name : " +--", 
						linedevice->subscriptionId.replaceCid ? "(=)" : "(+)", linedevice->subscriptionId.number, 
						sccp_strlen_zero(linedevice->subscriptionId.label) ? (l->label ? l->label : "--") : linedevice->subscriptionId.label,
						l->description ? l->description : "--",
						d->id, 
						(l->voicemailStatistic.newmsgs) ? "ON" : "OFF", 
						SCCP_RWLIST_GETSIZE(&l->channels), 
						(state != SCCP_CHANNELSTATE_SENTINEL) ? sccp_channelstate2str(state) : "--",
						(calltype != SKINNY_CALLTYPE_SENTINEL) ? skinny_calltype2str(calltype) : "--",
						cid_name,
						cap_buf);
				} else {
					astman_append(s, "Event: SCCPLineEntry\r\n");
					astman_append(s, "ChannelType: SCCP\r\n");
					astman_append(s, "ChannelObjectType: Line\r\n");
					astman_append(s, "ActionId: %s\r\n", actionid);
					astman_append(s, "Exten: %s\r\n", l->name);
					astman_append(s, "SubscriptionNumber: %s\r\n", linedevice->subscriptionId.number);
					astman_append(s, "Label: %s\r\n", sccp_strlen_zero(linedevice->subscriptionId.label) ? l->label : linedevice->subscriptionId.label);
					astman_append(s, "Description: %s\r\n", l->description ? l->description : "<not set>");
					astman_append(s, "Device: %s\r\n", d->id);
					astman_append(s, "MWI: %s\r\n", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF");
					astman_append(s, "ActiveChannels: %d\r\n", SCCP_LIST_GETSIZE(&l->channels));
					astman_append(s, "ChannelState: %s\r\n", (state != SCCP_CHANNELSTATE_SENTINEL) ? sccp_channelstate2str(state) : "--");
					astman_append(s, "CallType: %s\r\n", (calltype != SKINNY_CALLTYPE_SENTINEL) ? skinny_calltype2str(calltype) : "--");
					astman_append(s, "PartyName: %s\r\n", cid_name);
					astman_append(s, "Capabilities: %s\r\n", cap_buf);
					astman_append(s, "\r\n");
				}
				found_linedevice = 1;
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);

		if (found_linedevice == 0) {
			char cid_name[StationMaxNameSize] = {0};
			if (!s) {
				pbx_cli(fd, "| %-13s %-3s%-6s %-30s %-16s %-16s %-4s %-4d %-10s %-10s %-26.26s %-10s |\n", 
					l->name, 
					"", "",
					l->label, 
					l->description,
					"--", 
					(l->voicemailStatistic.newmsgs) ? "ON" : "OFF", 
					SCCP_LIST_GETSIZE(&l->channels),
					"--", 
					"--", 
					cid_name,
					cap_buf);
			} else {
				astman_append(s, "Event: SCCPLineEntry\r\n");
				astman_append(s, "ChannelType: SCCP\r\n");
				astman_append(s, "ChannelObjectType: Line\r\n");
				astman_append(s, "ActionId: %s\r\n", actionid);
				astman_append(s, "Exten: %s\r\n", l->name);
				astman_append(s, "Label: %s\r\n", l->label ? l->label : "<not set>");
				astman_append(s, "Description: %s\r\n", l->description ? l->description : "<not set>");
				astman_append(s, "Device: %s\r\n", "(null)");
				astman_append(s, "MWI: %s\r\n", (l->voicemailStatistic.newmsgs) ? "ON" : "OFF");
				astman_append(s, "\r\n");
			}
		}
		if (!s) {
			for (v = l->variables; v; v = v->next) {
				pbx_cli(fd, "| %-13s %-9s %-30s = %-101.101s |\n", "", "Variable:", v->name, v->value);
			}
			if (!sccp_strlen_zero(l->defaultSubscriptionId.number) || !sccp_strlen_zero(l->defaultSubscriptionId.name)) {
				pbx_cli(fd, "| %-13s %-9s %-30s %-103.103s |\n", "", "SubscrId:", l->defaultSubscriptionId.number, l->defaultSubscriptionId.name);
			}
		}
	}
	if (!s) {
		pbx_cli(fd, "+----------------------------------------------------------------------------------------------------------------------------------------------------------------+\n");
	} else {
		astman_append(s, "Event: TableEnd\r\n");
		local_line_total++;
		astman_append(s, "TableName: Lines\r\n");
		local_line_total++;
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		} else {
			astman_append(s, "\r\n");
		}
		local_line_total++;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
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
CLI_AMI_ENTRY(show_lines, sccp_show_lines, "List defined SCCP Lines", cli_lines_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -----------------------------------------------------------------------------------------------------------SHOW LINE- */
    /*!
     * \brief Show Line
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
    //static int sccp_show_line(int fd, int argc, char *argv[])
static int sccp_show_line(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_linedevices_t *linedevice = NULL;
	sccp_mailbox_t *mailbox = NULL;
	PBX_VARIABLE_TYPE *v = NULL;
	pbx_str_t *callgroup_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
	const char *actionid = "";

#ifdef CS_SCCP_PICKUP
	pbx_str_t *pickupgroup_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
#endif
	int local_line_total = 0;
	int local_table_total = 0;

	const char *line;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "LineName needs to be supplied\n");
		CLI_AMI_RETURN_ERROR(fd, s, m, "LineName needs to be supplied %s\n", "");		/* explicit return */
	}
	line = pbx_strdupa(argv[3]);
	AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byname(line, FALSE));

	if (!l) {
		pbx_log(LOG_WARNING, "Failed to get line %s\n", line);
		CLI_AMI_RETURN_ERROR(fd, s, m, "Can't find settings for line %s\n", line);		/* explicit return */
	}

	if (!s) {
		CLI_AMI_OUTPUT(fd, s, "\n--- SCCP channel driver line settings ------------------------------------------------------------------------------------\n");
	} else {
		astman_append(s, "Event: SCCPShowLine\r\n");
		actionid = astman_get_header(m, "ActionID");
		if (!pbx_strlen_zero(actionid)) {
			astman_append(s, "ActionID: %s\r\n", actionid);
		}
		local_line_total++;
	}
	/* *INDENT-OFF* */
	CLI_AMI_OUTPUT_PARAM("Name", 			CLI_AMI_LIST_WIDTH, "%s", l->name);
	CLI_AMI_OUTPUT_PARAM("Description", 		CLI_AMI_LIST_WIDTH, "%s", l->description ? l->description : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Label", 			CLI_AMI_LIST_WIDTH, "%s", l->label ? l->label : "<not set>");
	CLI_AMI_OUTPUT_PARAM("ID", 			CLI_AMI_LIST_WIDTH, "%s", l->id);
	CLI_AMI_OUTPUT_PARAM("Pin", 			CLI_AMI_LIST_WIDTH, "%s", l->pin);
	CLI_AMI_OUTPUT_PARAM("VoiceMail number", 	CLI_AMI_LIST_WIDTH, "%s", l->vmnum ? l->vmnum : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Transfer to Voicemail",	CLI_AMI_LIST_WIDTH, "%s", l->trnsfvm ? l->trnsfvm : "No");
	CLI_AMI_OUTPUT_BOOL("MeetMe enabled",		CLI_AMI_LIST_WIDTH, l->meetme);
	CLI_AMI_OUTPUT_PARAM("MeetMe number",		CLI_AMI_LIST_WIDTH, "%s", l->meetmenum ? l->meetmenum : "<not set>");
	CLI_AMI_OUTPUT_PARAM("MeetMe Options",		CLI_AMI_LIST_WIDTH, "%s", l->meetmeopts ? l->meetmeopts : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Context",			CLI_AMI_LIST_WIDTH, "%s (%s)", l->context ? l->context : "<not set>", pbx_context_find(l->context) ? "exists" : "does not exist !!");
	CLI_AMI_OUTPUT_PARAM("Language",		CLI_AMI_LIST_WIDTH, "%s", l->language ? l->language : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Account Code",		CLI_AMI_LIST_WIDTH, "%s", l->accountcode ? l->accountcode : "<not set>");
	CLI_AMI_OUTPUT_PARAM("Musicclass",		CLI_AMI_LIST_WIDTH, "%s", l->musicclass ? l->musicclass : "<not set>");
	CLI_AMI_OUTPUT_PARAM("AmaFlags",		CLI_AMI_LIST_WIDTH, "%d", l->amaflags);
	
	sccp_print_group(callgroup_buf, DEFAULT_PBX_STR_BUFFERSIZE, l->callgroup);
	CLI_AMI_OUTPUT_PARAM("Call Group",		CLI_AMI_LIST_WIDTH, "%s", callgroup_buf ? pbx_str_buffer(callgroup_buf) : "");
#ifdef CS_SCCP_PICKUP
	sccp_print_group(pickupgroup_buf, DEFAULT_PBX_STR_BUFFERSIZE, l->pickupgroup);
	CLI_AMI_OUTPUT_PARAM("Pickup Group", 		CLI_AMI_LIST_WIDTH, "%s", pickupgroup_buf ? pbx_str_buffer(pickupgroup_buf) : "");
#endif
	
#ifdef CS_AST_HAS_NAMEDGROUP
	CLI_AMI_OUTPUT_PARAM("Named Call Group",	CLI_AMI_LIST_WIDTH, "%s", l->namedcallgroup ? l->namedcallgroup : "NONE");
	CLI_AMI_OUTPUT_PARAM("Named Pickup Group",	CLI_AMI_LIST_WIDTH, "%s", l->namedpickupgroup ? l->namedpickupgroup : "NONE");
#endif
	CLI_AMI_OUTPUT_PARAM("ParkingLot",		CLI_AMI_LIST_WIDTH, "%s", !sccp_strlen_zero(l->parkinglot) ? l->parkinglot : "default");
	CLI_AMI_OUTPUT_PARAM("Caller ID name",		CLI_AMI_LIST_WIDTH, "%s", l->cid_name);
	CLI_AMI_OUTPUT_PARAM("Caller ID number",	CLI_AMI_LIST_WIDTH, "%s", l->cid_num);
	CLI_AMI_OUTPUT_PARAM("Incoming Calls limit",	CLI_AMI_LIST_WIDTH, "%d", l->incominglimit);
	CLI_AMI_OUTPUT_PARAM("Active Channel Count",	CLI_AMI_LIST_WIDTH, "%d", SCCP_RWLIST_GETSIZE(&l->channels));
	CLI_AMI_OUTPUT_PARAM("Sec. Dialtone Digits",	CLI_AMI_LIST_WIDTH, "%s", l->secondary_dialtone_digits);
	CLI_AMI_OUTPUT_PARAM("Sec. Dialtone",		CLI_AMI_LIST_WIDTH, "0x%02x", l->secondary_dialtone_tone);
	CLI_AMI_OUTPUT_BOOL("Echo Cancellation",	CLI_AMI_LIST_WIDTH, l->echocancel);
	CLI_AMI_OUTPUT_BOOL("Silence Suppression",	CLI_AMI_LIST_WIDTH, l->silencesuppression);
	CLI_AMI_OUTPUT_BOOL("Can Transfer",		CLI_AMI_LIST_WIDTH, l->transfer);
	CLI_AMI_OUTPUT_PARAM("DND Action",		CLI_AMI_LIST_WIDTH, "%s", (l->dndmode) ? sccp_dndmode2str(l->dndmode) : "Disabled / Cycle");
#ifdef CS_SCCP_REALTIME
	CLI_AMI_OUTPUT_BOOL("Is Realtime Line",		CLI_AMI_LIST_WIDTH, l->realtime);
#endif
	CLI_AMI_OUTPUT_BOOL("Pending Delete",		CLI_AMI_LIST_WIDTH, l->pendingUpdate);
	CLI_AMI_OUTPUT_BOOL("Pending Update",		CLI_AMI_LIST_WIDTH, l->pendingDelete);

	CLI_AMI_OUTPUT_PARAM("Registration Extension",	CLI_AMI_LIST_WIDTH, "%s", l->regexten ? l->regexten : "Unset");
	CLI_AMI_OUTPUT_PARAM("Registration Context",	CLI_AMI_LIST_WIDTH, "%s", l->regcontext ? l->regcontext : "Unset");

	CLI_AMI_OUTPUT_BOOL("Adhoc Number Assigned",	CLI_AMI_LIST_WIDTH, l->adhocNumber ? l->adhocNumber : "No");
	CLI_AMI_OUTPUT_PARAM("Message Waiting New.",	CLI_AMI_LIST_WIDTH, "%i", l->voicemailStatistic.newmsgs);
	CLI_AMI_OUTPUT_PARAM("Message Waiting Old.",	CLI_AMI_LIST_WIDTH, "%i", l->voicemailStatistic.oldmsgs);
	CLI_AMI_OUTPUT_PARAM("Active Devices", 		CLI_AMI_LIST_WIDTH, "%i", l->statistic.numberOfActiveDevices);
	CLI_AMI_OUTPUT_PARAM("Active Channels",		CLI_AMI_LIST_WIDTH, "%i", l->statistic.numberOfActiveChannels);
	CLI_AMI_OUTPUT_PARAM("Held Channels",		CLI_AMI_LIST_WIDTH, "%i", l->statistic.numberOfHeldChannels);
	//CLI_AMI_OUTPUT_PARAM("DND Devices",		CLI_AMI_LIST_WIDTH, "%i", l->statistic.numberOfDNDDevices);
	/* *INDENT-ON* */
	if (s) {
		astman_append(s, "\r\n");
	}
	// Line attached to these devices
#define CLI_AMI_TABLE_NAME AttachedDevices
#define CLI_AMI_TABLE_PER_ENTRY_NAME AttachedDevice
#define CLI_AMI_TABLE_LIST_ITER_HEAD &l->devices
#define CLI_AMI_TABLE_LIST_ITER_VAR linedevice
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 												\
		CLI_AMI_TABLE_FIELD(DeviceName,		"-15.15",	s,	15,	linedevice->device->id)		\
		CLI_AMI_TABLE_FIELD(CfwdType,		"8",		s,	8,	linedevice->cfwdAll.enabled ? "All" : (linedevice->cfwdBusy.enabled ? "Busy" : ""))	\
		CLI_AMI_TABLE_FIELD(CfwdNumber,		"-20.20",	s,	20,	linedevice->cfwdAll.enabled ? linedevice->cfwdAll.number : (linedevice->cfwdBusy.enabled ? linedevice->cfwdBusy.number : ""))
#include "sccp_cli_table.h"
		local_table_total++;

	// Mailboxes connected to this line
#define CLI_AMI_TABLE_NAME Mailboxes
#define CLI_AMI_TABLE_PER_ENTRY_NAME Mailbox
#define CLI_AMI_TABLE_LIST_ITER_HEAD &l->mailboxes
#define CLI_AMI_TABLE_LIST_ITER_VAR mailbox
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 											\
		CLI_AMI_TABLE_FIELD(mailbox,		"15.15",	s,	15,	mailbox->mailbox)	\
		CLI_AMI_TABLE_FIELD(context,		"-15.15",	s,	15,	mailbox->context)
#include "sccp_cli_table.h"
		local_table_total++;

	if (l->variables) {
		// SERVICEURL
#define CLI_AMI_TABLE_NAME Variables
#define CLI_AMI_TABLE_PER_ENTRY_NAME Variable
#define CLI_AMI_TABLE_ITERATOR for(v = l->variables;v;v = v->next)
#define CLI_AMI_TABLE_FIELDS 											\
		CLI_AMI_TABLE_FIELD(Name,		"15.15",	s,	15,	v->name)		\
		CLI_AMI_TABLE_FIELD(Value,		"-29.29",	s,	29,	v->value)
#include "sccp_cli_table.h"
		local_table_total++;
	}
	if (s) {
		totals->lines = local_line_total;
		totals->tables = local_table_total;
	}
	return RESULT_SUCCESS;
}

static char cli_line_usage[] = "Usage: sccp show line <lineId>\n" "       List defined SCCP line settings.\n";
static char ami_line_usage[] = "Usage: SCCPShowLine\n" "List defined SCCP line settings.\n\n" "PARAMS: LineName\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "line"
#define CLI_COMPLETE SCCP_CLI_LINE_COMPLETER
#define AMI_COMMAND "SCCPShowLine"
#define CLI_AMI_PARAMS "LineName"
CLI_AMI_ENTRY(show_line, sccp_show_line, "List defined SCCP line settings", cli_line_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* --------------------------------------------------------------------------------------------------------SHOW CHANNELS- */
    /*!
     * \brief Show Channels
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
    //static int sccp_show_channels(int fd, int argc, char *argv[])
static int sccp_show_channels(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_channel_t *channel = NULL;
	sccp_line_t *line = NULL;
	int local_line_total = 0;
	char tmpname[25];
	char addrStr[INET6_ADDRSTRLEN] = "";

#define CLI_AMI_TABLE_NAME Channels
#define CLI_AMI_TABLE_PER_ENTRY_NAME Channel
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(lines)
#define CLI_AMI_TABLE_LIST_ITER_VAR line
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 												\
		AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));								\
		SCCP_LIST_LOCK(&l->channels);											\
		SCCP_LIST_TRAVERSE(&l->channels, channel, list) {								\
			if (channel->conference_id) {										\
				snprintf(tmpname, sizeof(tmpname), "SCCPCONF/%03d/%03d", channel->conference_id, channel->conference_participant_id);	\
			} else {												\
				snprintf(tmpname, sizeof(tmpname), "%s", channel->designator);					\
			}													\
			if (&channel->rtp) {											\
				sccp_copy_string(addrStr,sccp_netsock_stringify(&channel->rtp.audio.phone), sizeof(addrStr));	\
			}

#define CLI_AMI_TABLE_AFTER_ITERATION 												\
		}														\
		SCCP_LIST_UNLOCK(&l->channels);											\

#define CLI_AMI_TABLE_FIELDS 															\
		CLI_AMI_TABLE_FIELD(ID,			"-5",		d,	5,	channel->callid)					\
		CLI_AMI_TABLE_FIELD(Name,		"-25.25",	s,	25,	pbx_strdupa(tmpname))					\
		CLI_AMI_TABLE_FIELD(LineName,		"-10.10",	s,	10,	channel->line->name)					\
		CLI_AMI_TABLE_FIELD(DeviceName,		"-16",		s,	16,	channel->currentDeviceId)				\
		CLI_AMI_TABLE_FIELD(NumCalled,		"-10.10",	s,	10,	channel->dialedNumber)					\
		CLI_AMI_TABLE_FIELD(PBX State,		"-10.10",	s,	10,	(channel->owner) ? pbx_state2str(iPbx.getChannelState(channel)) : "(none)")	\
		CLI_AMI_TABLE_FIELD(SCCP State,		"-10.10",	s,	10,	sccp_channelstate2str(channel->state))			\
		CLI_AMI_TABLE_FIELD(ReadCodec,		"-10.10",	s,	10,	codec2name(channel->rtp.audio.readFormat))		\
		CLI_AMI_TABLE_FIELD(WriteCodec,		"-10.10",	s,	10,	codec2name(channel->rtp.audio.writeFormat))		\
		CLI_AMI_TABLE_FIELD(RTPPeer,		"22.22",	s,	22,	addrStr)						\
		CLI_AMI_TABLE_FIELD(Direct,		"-6.6",		s,	6,	channel->rtp.audio.directMedia ? "yes" : "no")		\
		CLI_AMI_TABLE_FIELD(DTMFmode,		"-8.8",		s,	8,	sccp_dtmfmode2str(channel->dtmfmode))
#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

static char cli_channels_usage[] = "Usage: sccp show channels\n" "       Lists active channels for the SCCP subsystem.\n";
static char ami_channels_usage[] = "Usage: SCCPShowChannels\n" "Lists active channels for the SCCP subsystem.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "channels"
#define AMI_COMMAND "SCCPShowChannels"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_channels, sccp_show_channels, "Lists active SCCP channels", cli_channels_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

/* -------------------------------------------------------------------------------------------------------SHOW SESSIONS- */
static char cli_sessions_usage[] = "Usage: sccp show sessions [all]\n" "	Show [All] SCCP Sessions.\n";
static char ami_sessions_usage[] = "Usage: SCCPShowSessions\n" "Show [All] SCCP Sessions.\n\n" "Optional PARAMS: all\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "sessions"
#define AMI_COMMAND "SCCPShowSessions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_sessions, sccp_cli_show_sessions, "Show all SCCP sessions", cli_sessions_usage, FALSE, TRUE)
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
CLI_AMI_ENTRY(show_mwi_subscriptions, sccp_show_mwi_subscriptions, "Show all SCCP MWI subscriptions", cli_mwi_subscriptions_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

    /* ---------------------------------------------------------------------------------------------CONFERENCE FUNCTIONS- */
#ifdef CS_SCCP_CONFERENCE
static char cli_conferences_usage[] = "Usage: sccp show conferences\n" "       Lists running SCCP conferences.\n";
static char ami_conferences_usage[] = "Usage: SCCPShowConferences\n" "Lists running SCCP conferences.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "conferences"
#define AMI_COMMAND "SCCPShowConferences"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_conferences, sccp_cli_show_conferences, "List running SCCP Conferences", cli_conferences_usage, FALSE, TRUE)
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
#define CLI_AMI_PARAMS "ConferenceId"
CLI_AMI_ENTRY(show_conference, sccp_cli_show_conference, "List running SCCP Conference", cli_conference_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif
    /* DOXYGEN_SHOULD_SKIP_THIS */
static char cli_conference_command_usage[] = "Usage: sccp conference [conference_id]\n" "	Conference [EndConf | Kick | Mute | Invite | Moderate] [conference_id] [participant_id].\n";
static char ami_conference_command_usage[] = "Usage: SCCPConference [conference id]\n" "Conference Command.\n\n" "PARAMS: \n" "  Command: [EndConf | Kick | Mute | Invite | Moderate]\n" "  ConferenceId\n" "  ParticipantId\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "conference"
#define CLI_COMPLETE SCCP_CLI_CONFERENCE_COMPLETER
#define AMI_COMMAND "SCCPConference"
#define CLI_AMI_PARAMS "Command","ConferenceId","ParticipantId"
CLI_AMI_ENTRY(conference_command, sccp_cli_conference_command, "Conference Action", cli_conference_command_usage, TRUE, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

#endif														/* CS_SCCP_CONFERENCE */
    /* ---------------------------------------------------------------------------------------------SHOW_HINT LINESTATES - */
static char cli_show_hint_lineStates_usage[] = "Usage: sccp show hint linestates\n" "	Show All SCCP HINT LineStates.\n";
static char ami_show_hint_lineStates_usage[] = "Usage: SCCPShowHintLineStates\n" "Show All SCCP Hint Line States.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "hint", "lineStates"
#define AMI_COMMAND "SCCPShowHintLineStates"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_hint_lineStates, sccp_show_hint_lineStates, "Show all SCCP Hint Line States", cli_show_hint_lineStates_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* ---------------------------------------------------------------------------------------------SHOW_HINT LINESTATES - */
static char cli_show_hint_subscriptions_usage[] = "Usage: sccp show hint linestates\n" "	Show All SCCP HINT LineStates.\n";
static char ami_show_hint_subscriptions_usage[] = "Usage: SCCPShowHintLineStates\n" "Show All SCCP Hint Line States.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "hint", "subscriptions"
#define AMI_COMMAND "SCCPShowHintSubscriptions"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_hint_subscriptions, sccp_show_hint_subscriptions, "Show all SCCP Hint Subscriptions", cli_show_hint_subscriptions_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -------------------------------------------------------------------------------------------------------TEST- */
#ifdef CS_EXPERIMENTAL
/*!
 * \brief Test Message
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
#include "sccp_actions.h"
static int sccp_test(int fd, int argc, char *argv[])
{
	if (argc < 3) {
		return RESULT_SHOWUSAGE;
	}
	if (sccp_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}
	// OpenReceiveChannel TEST
	if (!strcasecmp(argv[2], "openreceivechannel")) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Testing re-Sending OpenReceiveChannel to change Payloads on the fly!!\n");
		sccp_msg_t *msg1 = NULL;
		sccp_msg_t *msg2 = NULL;
		int packetSize = 20;										/*! \todo calculate packetSize */

		sccp_line_t *l = NULL;
		sccp_channel_t *channel = NULL;

		SCCP_RWLIST_RDLOCK(&GLOB(lines));
		SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
			SCCP_LIST_TRAVERSE(&l->channels, channel, list) {
				AUTO_RELEASE(sccp_channel_t, tmpChannel , sccp_channel_retain(channel));
				AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(tmpChannel));
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Sending OpenReceiveChannel and changing payloadType to 8\n");

				REQ(msg1, OpenReceiveChannel);
				msg1->data.OpenReceiveChannel.v17.lel_conferenceId = htolel(tmpChannel->callid);
				msg1->data.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(tmpChannel->passthrupartyid);
				msg1->data.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
				msg1->data.OpenReceiveChannel.v17.lel_payloadType = htolel(8);
				msg1->data.OpenReceiveChannel.v17.lel_vadValue = htolel(tmpChannel->line->echocancel);
				msg1->data.OpenReceiveChannel.v17.lel_callReference = htolel(tmpChannel->callid);
				msg1->data.OpenReceiveChannel.v17.lel_dtmfType = htolel(10);
				sccp_dev_send(d, msg1);
				// sleep(1);
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Sending OpenReceiveChannel and changing payloadType to 4\n");
				REQ(msg2, OpenReceiveChannel);
				msg2->data.OpenReceiveChannel.v17.lel_conferenceId = htolel(tmpChannel->callid);
				msg2->data.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(tmpChannel->passthrupartyid);
				msg2->data.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
				msg2->data.OpenReceiveChannel.v17.lel_payloadType = htolel(4);
				msg2->data.OpenReceiveChannel.v17.lel_vadValue = htolel(tmpChannel->line->echocancel);
				msg2->data.OpenReceiveChannel.v17.lel_callReference = htolel(tmpChannel->callid);
				msg2->data.OpenReceiveChannel.v17.lel_dtmfType = htolel(10);
				sccp_dev_send(d, msg2);
			}
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Testing re-Sending OpenReceiveChannel. It WORKS !\n");
		}
		SCCP_RWLIST_UNLOCK(&GLOB(lines));
		return RESULT_SUCCESS;
	}
	// SpeedDialStatDynamicMessage = 0x0149,
	if (!strcasecmp(argv[2], "speeddialstatdynamic")) {
		sccp_device_t *d = NULL;
		sccp_buttonconfig_t *buttonconfig = NULL;
		uint8_t instance = 0;
		sccp_msg_t *msg1 = NULL;

		if (argc < 5) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Device Not specified\n");
			return RESULT_FAILURE;
		}
		if ((d = sccp_device_find_byid(argv[3], FALSE))) {
			SCCP_LIST_LOCK(&d->buttonconfig);
			SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
				if (buttonconfig->type == SPEEDDIAL) {
					instance = buttonconfig->instance;
					REQ(msg1, SpeedDialStatDynamicMessage);
					msg1->data.SpeedDialStatDynamicMessage.lel_Number = htolel(instance);
					sccp_copy_string(msg1->data.SpeedDialStatDynamicMessage.DirNumber, argv[5], sizeof(msg1->data.SpeedDialStatDynamicMessage.DirNumber));
					sccp_copy_string(msg1->data.SpeedDialStatDynamicMessage.DisplayName, argv[6], sizeof(msg1->data.SpeedDialStatDynamicMessage.DisplayName));					
					sccp_dev_send(d, msg1);
				}
			}
			SCCP_LIST_UNLOCK(&d->buttonconfig);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "hint")) {
		int state = (argc == 6) ? sccp_atoi(argv[4], strlen(argv[4])) : 0;

		pbx_devstate_changed(state, "SCCP/%s", argv[3]);
		pbx_log(LOG_NOTICE, "Hint %s Set NewState: %d\n", argv[3], state);
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "permit")) {									/*  WIP */
		pbx_str_t *buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

		//struct sccp_ha *path; 
		//sccp_append_ha(const char *sense, const char *stuff, struct sccp_ha *path, int *error)

		sccp_print_ha(buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(ha));
		pbx_log(LOG_NOTICE, "%s: HA Buffer: %s\n", argv[3], pbx_str_buffer(buf));
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "retrieveDeviceCapabilities")) {						/*  WIP */
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
		if (d) {
			d->retrieveDeviceCapabilities(d);
			pbx_log(LOG_NOTICE, "%s: Done\n", d->id);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "xml")) {									/*  WIP */
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
		if (d) {
			char *xmlData1 = "<CiscoIPPhoneText><Title>Test 1 XML Message</Title><Text>abcdefghijklmnopqrstuvwxyz</Text></CiscoIPPhoneText>";

			d->protocol->sendUserToDeviceDataVersionMessage(d, 1, 1, 1, 1, xmlData1, 2);
			pbx_log(LOG_NOTICE, "%s: Done1\n", d->id);
			sleep(1);
			char *xmlData2 =
			    "<CiscoIPPhoneText><Title>Test 2 XML Message</Title><Text>abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_</Text></CiscoIPPhoneText>";
			d->protocol->sendUserToDeviceDataVersionMessage(d, 1, 1, 1, 1, xmlData2, 2);
			pbx_log(LOG_NOTICE, "%s: Done2\n", d->id);
			sleep(1);
			char *xmlData3 =
			    "<CiscoIPPhoneText><Title>Test 3 XML Message</Title><Text>abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_</Text></CiscoIPPhoneText>";
			d->protocol->sendUserToDeviceDataVersionMessage(d, 1, 1, 1, 1, xmlData3, 2);
			pbx_log(LOG_NOTICE, "%s: Done3\n", d->id);
			sleep(2);
			char *xmlData4 =
			    "<CiscoIPPhoneText><Title>Test 4 XML Message</Title><Text>abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_</Text></CiscoIPPhoneText>";
			d->protocol->sendUserToDeviceDataVersionMessage(d, 1, 1, 1, 1, xmlData4, 2);
			pbx_log(LOG_NOTICE, "%s: Done4\n", d->id);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "inputxml")) {									/*  WIP */
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
		if (d) {
			char xmlData1[] = {
				"<CiscoIPPhoneInput>" "<Title>Change Password</Title>"
				    //"<URL>:::10.15.15.195:2000:SESSION_NUMBER/Preferences/PinInput</URL>"
				"<URL>:%s:SESSION_NUMBER/Preferences/PinInput</URL>"
				    "<Prompt>PROMPT_TEXT</Prompt>"
				    "<InputItem>"
				    "<DisplayName>PIN</DisplayName>"
				    "<QueryStringParam>s</QueryStringParam>"
				    "<InputFlags>NP</InputFlags>"
				    "<DefaultValue></DefaultValue>"
				    "</InputItem>"
				    "<SoftKeyItem>"
				    "<Name>Accept</Name>"
				    "<URL>SoftKey:Submit</URL>"
				    "<Position>1</Position>"
				    "</SoftKeyItem>"
				    "<SoftKeyItem>"
				    "<Name>Accept</Name>"
				    "<URL>Key:Submit</URL>" "<Position>2</Position>" "</SoftKeyItem>" "<SoftKeyItem>" "<Name>Cancel</Name>" "<URL>:::SESSION_NUMBER/Preferences/Back</URL>" "<Position>2</Position>" "</SoftKeyItem>" "<SoftKeyItem>" "<Name>&lt;&lt;</Name>" "<URL>SoftKey:&lt;&lt;</URL>" "<Position>3</Position>" "</SoftKeyItem>" "<SoftKeyItem>" "<Name>Home</Name>" "<URL>:::SESSION_NUMBER/Preferences/Home</URL>" "<Position>4</Position>" "</SoftKeyItem>" "</CiscoIPPhoneInput>"
			};
			char xmlData2[2000];

			struct sockaddr_storage ourip = { 0 };
			sccp_session_getOurIP(d->session, &ourip, 0);
			snprintf(xmlData2, sizeof(xmlData2), xmlData1, sccp_netsock_stringify(&ourip));

			d->protocol->sendUserToDeviceDataVersionMessage(d, 1, 0, 0, 1, xmlData2, 1);
			pbx_log(LOG_NOTICE, "%s: Done1\n", d->id);
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "remove_reference")) {								/*  WIP */
		long findobj = 0;

		if (argc == 5 && sscanf(argv[3], "%lx", &findobj) == 1) {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_1 "SCCP: Reducing Refounct of 0x%lx, %s, %s by one\n", findobj, argv[3], argv[4]);
			if (sccp_refcount_force_release(findobj, argv[4])) {
				return RESULT_SUCCESS;
			}
		}
	}
	if (!strcasecmp(argv[2], "labels")) {
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
		uint8_t x, y, block;
		char clientAddress[INET6_ADDRSTRLEN];

		pbx_log(LOG_NOTICE, "%s: Running Labels\n", d->id);

		if (d) {
			if (sccp_device_getRegistrationState(d) == SKINNY_DEVICE_RS_OK) {
				if (argc < 5) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getSas(d->session, &sas);
					sccp_copy_string(clientAddress, sccp_netsock_stringify_addr(&sas), sizeof(clientAddress));
				} else {
					sccp_copy_string(clientAddress, argv[6], sizeof(clientAddress));
				}
				for (y = 0; y < 2; y++) {
					block = y == 0 ? 36 : 200;;
					for (x = 0; x < 255; x++) {
						char label[] = { y == 0 ? '\36' : '\200', x, '\0' };
						sccp_log(DEBUGCAT_CORE) ("%s: Sending \\%d\\%d\n", d->id, block, x);
						sccp_dev_displayprompt(d, 0, 0, label, 2);
						char command[512];

						usleep(50);
						snprintf(command, 512, "wget -q --user %s --password %s http://%s/CGI/Screenshot -O \"./screenshot_%s_%s_%d_%d.bmp\"", argv[4], argv[5], clientAddress, argv[3], skinny_devicetype2str(d->skinny_type), block, x);
						sccp_log(DEBUGCAT_CORE) ("%s: Taking snapshot using '%s'\n", d->id, command);
						int sysout = system(command);

						sccp_log(DEBUGCAT_CORE) ("%s: System Result '%d'\n", d->id, sysout);
						usleep(100);
					}
				}
			} else {
				sccp_log(DEBUGCAT_CORE) ("%s: Device not registered yet, try again later\n", d->id);
			}
			return RESULT_SUCCESS;
		}
		return RESULT_FAILURE;
	}
	if (!strcasecmp(argv[2], "callinfo") && argc > 4) {
		AUTO_RELEASE(sccp_channel_t, c , sccp_channel_find_byid(sccp_atoi(argv[3], strlen(argv[3]))));

		if (c) {
			pbx_log(LOG_NOTICE, "%s: Running CallInfo: %s\n", c->designator, argv[4]);
			if (sccp_strcaseequals(argv[4], "CalledParty")) {
				pbx_log(LOG_NOTICE, "%s: Setting Called Party, '%s', '%s'\n", c->designator, argv[5], argv[6]);
				sccp_channel_set_calledparty(c, argv[5], argv[6]);
			} else if (sccp_strcaseequals(argv[4], "CallingParty")) {
				pbx_log(LOG_NOTICE, "%s: Setting Calling Party, '%s', '%s'\n", c->designator, argv[5], argv[6]);
				sccp_channel_set_callingparty(c, argv[5], argv[6]);
			} else if (sccp_strcaseequals(argv[4], "OriginalCalledPartyName")) {
				pbx_log(LOG_NOTICE, "%s: Setting Original Called Party, '%s', '%s'\n", c->designator, argv[5], argv[6]);
				sccp_channel_set_originalCalledparty(c, argv[5], argv[6]);
			} else if (sccp_strcaseequals(argv[4], "OriginalCallingPartyName")) {
				pbx_log(LOG_NOTICE, "%s: Setting Original Calling Party, '%s', '%s'\n", c->designator, argv[5], argv[6]);
				sccp_channel_set_originalCalledparty(c, argv[5], argv[6]);
			}
			return RESULT_SUCCESS;
		} else {
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test Callinfo, callid %s not found\n", argv[3]);
		}
		return RESULT_FAILURE;
	}
	if (!strcasecmp(argv[2], "linestat") && argc > 2) {
		AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
		if (d) {
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat\n");
			sccp_msg_t *msg = NULL;
			msg = sccp_utils_buildLineStatDynamicMessage(1, 0x01 & 0x08, "1234", "name", "disp1");
			sccp_dev_send(d, msg);
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat1 0x01=OrigDialed & 0x08=CallingPartyName Sent\n");
			sleep(2);
			msg = sccp_utils_buildLineStatDynamicMessage(2, 2, "98011", "Diederik de Groot", "CfwdAll: 1234");
			sccp_dev_send(d, msg);
			if (d->protocolversion < 11) {
				sccp_dev_set_cplane(d, 2, 1);
				sleep(1);
				sccp_dev_deactivate_cplane(d);
			}
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat2 2=RedirDialed Sent\n");
			sleep(2);
			msg = sccp_utils_buildLineStatDynamicMessage(3, 4, "1234", "name", "disp4");
			sccp_dev_send(d, msg);
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat4 4=CallingPartyNumber Sent\n");
			sleep(2);
			msg = sccp_utils_buildLineStatDynamicMessage(4, 8, "1234", "name", "disp8");
			sccp_dev_send(d, msg);
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat8 8=CallinPartyName Sent\n");
			sleep(2);
			msg = sccp_utils_buildLineStatDynamicMessage(5, 0x0f, "1234", "name", "disp");
			sccp_dev_send(d, msg);
			sccp_log(DEBUGCAT_CORE) ("SCCP: Test LineStat 0x0f All of the Above Sent\n");		// This explaines the 0x15 we always send for these buttons
														// I think this type flag might be used to influence what the cplane may will show
			sleep(2);
			return RESULT_SUCCESS;
		}
		return RESULT_SUCCESS;
	}
	if (!strcasecmp(argv[2], "enum") && argc > 2) {
		pbx_cli(fd, "%s, %d\n", sccp_channelstate2str(SCCP_CHANNELSTATE_CONGESTION), SCCP_CHANNELSTATE_CONGESTION);
		pbx_cli(fd, "%d, %d\n", sccp_channelstate_str2val("CONGESTION"), SCCP_CHANNELSTATE_CONGESTION);
		pbx_cli(fd, "%d, %d\n", sccp_channelstate_str2intval("CONGESTION"), SCCP_CHANNELSTATE_CONGESTION);
		pbx_cli(fd, "%s\n", sccp_channelstate_all_entries());
 		char *all_entries = pbx_strdupa(sccp_channelstate_all_entries());
		pbx_cli(fd, "%s (%d)\n", all_entries, (int)strlen(all_entries));
		while (*all_entries) {
			if (*all_entries == ',') {
				*all_entries = '|';
			}
			all_entries++;
		}
		pbx_cli(fd, "%s\n", all_entries);
		#ifdef DEBUG
		sccp_do_backtrace();
		#endif
		return RESULT_SUCCESS;
	}
#if CS_REFCOUNT_DEBUG
	if (!strcasecmp(argv[2], "refreport") && argc > 2) {
		AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(argv[3], FALSE));
		if (d) {
			pbx_str_t *buf = pbx_str_create(DEFAULT_PBX_STR_BUFFERSIZE);
			sccp_refcount_gen_report(d, &buf);
			pbx_log(LOG_NOTICE, "%s (cli_test) refcount_report:\n%s\n", argv[3], pbx_str_buffer(buf));
			sccp_free(buf);
		}
		return RESULT_SUCCESS;
	}
#endif
	return RESULT_FAILURE;
}

static char cli_test_usage[] = "Usage: sccp test [test_name]\n" "	Test [test_name].\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "test"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
CLI_ENTRY(cli_test, sccp_test, "Test", cli_test_usage, FALSE)
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

#endif														// CS_EXPERIMENTAL

    /* ---------------------------------------------------------------------------------------------SHOW_REFCOUNT - */
static char cli_show_refcount_usage[] = "Usage: sccp show refcount [show|suppress]\n" "	Show All SCCP Refcount Entries.\n";
static char ami_show_refcount_usage[] = "Usage: SCCPShowRefcount\n" "Show All Refcount Entries.\n\n" "Optional PARAMS: inuse [show, suppress]\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "refcount"
#define AMI_COMMAND "SCCPShowRefcount"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS "inuse"
CLI_AMI_ENTRY(show_refcount, sccp_show_refcount, "Show all Refcount Entries", cli_show_refcount_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */

    /* --------------------------------------------------------------------------------------------------SHOW_SOKFTKEYSETS- */
    /*!
     * \brief Show Sessions
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
    //static int sccp_show_softkeysets(int fd, int argc, char *argv[])
static int sccp_show_softkeysets(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	uint8_t i = 0;
	uint8_t v_count = 0;
	uint8_t c = 0;
	int local_line_total = 0;

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
#define CLI_AMI_TABLE_FIELDS														\
				CLI_AMI_TABLE_FIELD(Set,		"-15.15",	s,	15,	softkeyset->name)		\
				CLI_AMI_TABLE_FIELD(Mode,		"-12.12",	s,	12,	skinny_keymode2str(i))		\
				CLI_AMI_TABLE_FIELD(Description,	"-40.40",	s,	40,	skinny_keymode2longstr(i))	\
				CLI_AMI_TABLE_FIELD(LblID,		"-5",		d,	5,	c)				\
				CLI_AMI_TABLE_FIELD(Label,	      "-15.15",	s,	15,     label2str(b[c]))
#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
	}
	return RESULT_SUCCESS;
}

static char cli_show_softkeysets_usage[] = "Usage: sccp show softkeysets\n" "	Show the configured SoftKeySets.\n";
static char ami_show_softkeysets_usage[] = "Usage: SCCPShowSoftKeySets\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: None\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "show", "softkeyssets"
#define AMI_COMMAND "SCCPShowSoftKeySets"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS ""
CLI_AMI_ENTRY(show_softkeysets, sccp_show_softkeysets, "Show configured SoftKeySets", cli_show_softkeysets_usage, FALSE, TRUE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -----------------------------------------------------------------------------------------------------MESSAGE DEVICES- */
    /*!
     * \brief Message Devices
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
static int sccp_message_devices(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d = NULL;
	int timeout = 10;
	boolean_t beep = FALSE;
	int local_line_total = 0;

	if (argc < 4) {
		pbx_log(LOG_WARNING, "More parameters needed for sccp_message_devices\n");
		return RESULT_SHOWUSAGE;
	}

	if (sccp_strlen_zero(argv[3])) {
		pbx_log(LOG_WARNING, "MessageText cannot be empty\n");
		CLI_AMI_RETURN_ERROR(fd, s, m, "messagetext cannot be empty, '%s'\n", argv[3]);		/* explicit return */
	}

	if (argc > 4) {
		if (!strcmp(argv[4], "beep")) {
			beep = TRUE;
			if (argc > 5) {
				sscanf(argv[5], "%d", &timeout);
			}
		} else {
			sscanf(argv[4], "%d", &timeout);
		}
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Sending message '%s' to all devices (beep: %d, timeout: %d)\n", argv[3], beep, timeout);
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, FALSE, beep);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	if (s) {
		totals->lines = local_line_total;
	}
	return RESULT_SUCCESS;
}

static char cli_message_devices_usage[] = "Usage: sccp message devices <message text> [beep] [timeout]\n" "       Send a message to all SCCP Devices + phone beep + timeout.\n";
static char ami_message_devices_usage[] = "Usage: SCCPMessageDevices\n" "Show All SCCP Softkey Sets.\n\n" "PARAMS: MessageText, Beep, Timeout\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "message", "devices"
#define AMI_COMMAND "SCCPMessageDevices"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS "MessageText", "Beep", "Timeout"
CLI_AMI_ENTRY(message_devices, sccp_message_devices, "Send a message to all SCCP devices", cli_message_devices_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef CLI_COMPLETE
#undef AMI_COMMAND
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -----------------------------------------------------------------------------------------------------MESSAGE DEVICE- */
    /*!
     * \brief Message Device
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     *
     * \todo TO BE IMPLEMENTED: sccp message device
     */
static int sccp_message_device(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int timeout = 10;
	boolean_t beep = FALSE;
	int local_line_total = 0;
	int res = RESULT_FAILURE;

	if (argc < 5) {
		pbx_log(LOG_WARNING, "More parameters needed for sccp_message_device\n");
		return RESULT_SHOWUSAGE;
	}
	if (sccp_strlen_zero(argv[4])) {
		pbx_log(LOG_WARNING, "MessageText cannot be empty\n");
		CLI_AMI_RETURN_ERROR(fd, s, m, "messagetext cannot be empty, '%s'\n", argv[4]);		/* explicit return */
	}
	if (argc > 5) {
		if (!strcmp(argv[5], "beep")) {
			beep = TRUE;
			if (argc > 6) {
				sscanf(argv[6], "%d", &timeout);
			}
		} else {
			sscanf(argv[5], "%d", &timeout);
		}
	}
	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
	if (d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Sending message '%s' to %s (beep: %d, timeout: %d)\n", argv[3], d->id, beep, timeout);
		sccp_dev_set_message(d, argv[4], timeout, FALSE, beep);
		res = RESULT_SUCCESS;
	} else {
		CLI_AMI_RETURN_ERROR(fd, s, m, "Device '%s' not found!\n", argv[3]);		/* explicit return */
	}

	if (s) {
		totals->lines = local_line_total;
	}
	return res;
}

static char cli_message_device_usage[] = "Usage: sccp message device <deviceId> <message text> [beep] [timeout]\n" "       Send a message to an SCCP Device + phone beep + timeout.\n";
static char ami_message_device_usage[] = "Usage: SCCPMessageDevice\n" "Send a message to an SCCP Device.\n\n" "PARAMS: DeviceId, MessageText, Beep, Timeout\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "message", "device"
#define AMI_COMMAND "SCCPMessageDevice"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
#define CLI_AMI_PARAMS "DeviceId", "MessageText", "Beep", "Timeout"
CLI_AMI_ENTRY(message_device, sccp_message_device, "Send a message to SCCP Device", cli_message_device_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef AMI_COMMAND
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* ------------------------------------------------------------------------------------------------------SYSTEM MESSAGE- */
    /*!
     * \brief System Message
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     */
static int sccp_system_message(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_device_t *d = NULL;
	int timeout = 0;
	char timeoutStr[5] = "";
	boolean_t beep = FALSE;
	int local_line_total = 0;
	int res = RESULT_FAILURE;

	if (argc == 3) {
		SCCP_RWLIST_RDLOCK(&GLOB(devices));
		SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
			sccp_dev_clear_message(d, TRUE);
		}
		SCCP_RWLIST_UNLOCK(&GLOB(devices));
		CLI_AMI_OUTPUT(fd, s, "Message Cleared\n");
		return RESULT_SUCCESS;
	}

	if (argc < 4 || argc > 6 || sccp_strlen_zero(argv[3])) {
		return RESULT_SHOWUSAGE;
	}

	if (!(iPbx.feature_addToDatabase("SCCP/message", "text", argv[3]))) {
		CLI_AMI_OUTPUT(fd, s, "Failed to store the SCCP system message text\n");
	} else {
		sccp_log((DEBUGCAT_CLI)) (VERBOSE_PREFIX_3 "SCCP system message text stored successfully\n");
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

	snprintf(timeoutStr, sizeof(timeoutStr), "%d", timeout);
	if (!(iPbx.feature_addToDatabase("SCCP/message", "timeout", timeoutStr))) {
		CLI_AMI_OUTPUT(fd, s, "Failed to store the SCCP system message timeout\n");
	} else {
		sccp_log((DEBUGCAT_CLI)) (VERBOSE_PREFIX_3 "SCCP system message timeout stored successfully\n");
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Sending system message '%s' to all devices (beep: %d, timeout: %d)\n", argv[3], beep, timeout);
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_dev_set_message(d, argv[3], timeout, TRUE, beep);
		res = RESULT_SUCCESS;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	if (s) {
		totals->lines = local_line_total;
	}
	return res;
}

static char cli_system_message_usage[] = "Usage: sccp system message <message text> [beep] [timeout]\n" "       The default optional timeout is 0 (forever)\n" "       Example: sccp system message \"The boss is gone. Let's have some fun!\"  10\n";
static char ami_system_message_usage[] = "Usage: SCCPSystemMessage\n" "Set a system wide message for all devices.\n\n" "PARAMS: MessageText, Beep, Timeout\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "system", "message"
#define AMI_COMMAND "SCCPSystemMessage"
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS "MessageText", "Beep", "Timeout"
CLI_AMI_ENTRY(system_message, sccp_system_message, "Send a system wide message to all SCCP Devices", cli_system_message_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef AMI_COMMAND
#undef CLI_COMPLETE
#undef CLI_COMMAND
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* -----------------------------------------------------------------------------------------------------DND DEVICE- */
    /*!
     * \brief Message Device
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     * 
     */
static int sccp_dnd_device(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int res = RESULT_FAILURE;

	int local_line_total = 0;

	if (3 > argc || argc > 5) {
		return RESULT_SHOWUSAGE;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], TRUE));
	if (d) {
		if (argc == 5) {
			if (sccp_strcaseequals(argv[4], "silent")) {
				d->dndFeature.status = SCCP_DNDMODE_SILENT;
				CLI_AMI_OUTPUT(fd, s, "Set DND SILENT\r\n");
			} else if (sccp_strcaseequals(argv[4], "reject")) {
				d->dndFeature.status = SCCP_DNDMODE_REJECT;
				CLI_AMI_OUTPUT(fd, s, "Set DND REJECT\r\n");
			} else if (sccp_strcaseequals(argv[4], "off")) {
				d->dndFeature.status = SCCP_DNDMODE_OFF;
				CLI_AMI_OUTPUT(fd, s, "Unset DND\r\n");
			} else {
				CLI_AMI_OUTPUT(fd, s, "Unknown DND State: %s\n", argv[3]);
				CLI_AMI_RETURN_ERROR(fd, s, m, "Can't find device %s\n", argv[3]);		/* explicit return */
			}
			sccp_feat_changed(d, NULL, SCCP_FEATURE_DND);
			sccp_dev_check_displayprompt(d);
		} else {
			//sccp_sk_dnd(d, NULL, 0, NULL);
			if (sccp_SoftkeyMap_execCallbackByEvent(d, NULL, 0, NULL, SKINNY_LBL_DND)) {
				CLI_AMI_OUTPUT(fd, s, "Set/Unset DND\r\n");
			} else {
				CLI_AMI_OUTPUT(fd, s, "Set/Unset DND Failed\r\n");
			}
		}
		res = RESULT_SUCCESS;
	} else {
		CLI_AMI_RETURN_ERROR(fd, s, m, "Can't find device %s\n", argv[3]);		/* explicit return */
	}

	if (s) {
		totals->lines = local_line_total;
	}
	return res;
}

static char cli_dnd_device_usage[] = "Usage: sccp dnd <deviceId> [off|reject|silent]\n" "       Send a dnd to an SCCP Device. Optionally specifying new DND state  [off|reject|silent]\n";
static char ami_dnd_device_usage[] = "Usage: SCCPDndDevice\n" "Set/Unset DND status on a SCCP Device.\n\n" "PARAMS: DeviceId, State= [off|reject|silent]\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "dnd", "device"
#define AMI_COMMAND "SCCPDndDevice"
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER, SCCP_CLI_NULL_COMPLETER
#define CLI_AMI_PARAMS "DeviceId, State"
CLI_AMI_ENTRY(dnd_device, sccp_dnd_device, "Set/Unset DND on an SCCP Device", cli_dnd_device_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef AMI_COMMAND
#undef CLI_COMPLETE
#undef CLI_COMMAND
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
	int res = RESULT_FAILURE;
	AUTO_RELEASE(sccp_device_t, d , NULL);
	AUTO_RELEASE(sccp_line_t, line , NULL);

	if (3 > argc || argc > 5) {
		return RESULT_SHOWUSAGE;
	}
	if ((d = sccp_device_find_byid(argv[3], FALSE))) {						// don't create new realtime devices by searching for them
		if ((line = sccp_line_find_byname(argv[4], FALSE))) {					// don't create new realtime lines by searching for them
			sccp_buttonconfig_t *config;

			d->pendingUpdate = 1;
			SCCP_LIST_LOCK(&d->buttonconfig);
			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&d->buttonconfig, config, list) {
				if (config->type == LINE && sccp_strequals(config->button.line.name,line->name)) {
					config->pendingDelete = 1;
					pbx_cli(fd, "Found at ButtonIndex %d => Line %s, removing...\n", config->index, line->name);
				}
			}
			SCCP_LIST_TRAVERSE_SAFE_END;
			SCCP_LIST_UNLOCK(&d->buttonconfig);
			
			pbx_cli(fd, "Line %s has been removed from device %s. Reloading Device...\n", line->name, d->id);
			sccp_device_check_update(d);
			res = RESULT_SUCCESS;
		} else {
			pbx_log(LOG_ERROR, "Error: Line %s not found\n", argv[4]);
		}
	} else {
		pbx_log(LOG_ERROR, "Error: Device %s not found\n", argv[3]);
	}
	return res;
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
	int res = RESULT_FAILURE;
	if (argc < 5) {
		return RESULT_SHOWUSAGE;
	}
	if (sccp_strlen_zero(argv[4])) {
		return RESULT_SHOWUSAGE;
	}

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
	if (d) {
		AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byname(argv[4], FALSE));
		if (!l) {
			pbx_log(LOG_ERROR, "Error: Line %s not found\n", argv[4]);
		}
 		d->pendingUpdate = 1;
		if (sccp_config_addButton(&d->buttonconfig, -1, LINE, l->name, NULL, NULL) == SCCP_CONFIG_CHANGE_CHANGED) {
			pbx_cli(fd, "Line %s has been added to device %s\n", l->name, d->id);
			sccp_device_check_update(d);
			res = RESULT_SUCCESS;
		} else {
	 		d->pendingUpdate = 0;;
		}
	} else {
		pbx_log(LOG_ERROR, "Error: Device %s not found\n", argv[3]);
	}

	return res;
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

	if (argc > 2) {
		pbx_cli(fd, "SCCP new debug status: (%d -> %d) %s\n", GLOB(debug), new_debug, debugcategories);
	} else {
		pbx_cli(fd, "SCCP debug status: (%d) %s\n", GLOB(debug), debugcategories);
	}
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
	if (argc < 3) {
		return RESULT_SHOWUSAGE;
	}
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
 * \note To find out more about the reload function see \ref sccp_config_reload
 */
static int sccp_cli_reload(int fd, int argc, char *argv[])
{
	boolean_t force_reload = FALSE;
	int returnval = RESULT_FAILURE;
	sccp_configurationchange_t change;
	sccp_buttonconfig_t *config = NULL;

	if (argc < 2 || argc > 4) {
		return RESULT_SHOWUSAGE;
	}
	pbx_rwlock_wrlock(&GLOB(lock));
	if (GLOB(reload_in_progress) == TRUE) {
		pbx_cli(fd, "SCCP reloading already in progress.\n");
		pbx_rwlock_unlock(&GLOB(lock));
		goto EXIT;
	}

	if (!GLOB(cfg)) {
		pbx_log(LOG_NOTICE, "GLOB(cfg) not available. Skip loading default setting.\n");
		pbx_rwlock_unlock(&GLOB(lock));
		goto EXIT;
	}
	GLOB(reload_in_progress) = TRUE;
	pbx_rwlock_unlock(&GLOB(lock));

	if (argc > 2) {
		if (sccp_strequals("device", argv[2])) {
			if (argc == 4) {
				AUTO_RELEASE(sccp_device_t, device , sccp_device_find_byid(argv[3], FALSE));
				PBX_VARIABLE_TYPE *v = NULL;

				if (!device) {
					pbx_cli(fd, "Could not find device %s\n", argv[3]);
					const char *utype;

					utype = pbx_variable_retrieve(GLOB(cfg), argv[3], "type");
					if (utype && !strcasecmp(utype, "device")) {
						device = sccp_device_create(argv[3]);
					} else {
						pbx_cli(fd, "Could not find device %s in config\n", argv[3]);
						goto EXIT;
					}
				}
#ifdef CS_SCCP_REALTIME
				if (device->realtime) {
					v = pbx_load_realtime(GLOB(realtimedevicetable), "name", argv[3], NULL);
				} else
#endif
				{
					if ((CONFIG_STATUS_FILE_OK == sccp_config_getConfig(TRUE)) && GLOB(cfg)) {
						v = ast_variable_browse(GLOB(cfg), argv[3]);
					}
				}
				if (v) {
					SCCP_LIST_LOCK(&device->buttonconfig);
					SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
						sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_4 "%s: Setting Button at Index:%d to pendingDelete\n", device->id, config->index);
						config->pendingDelete = 1;
					}
					SCCP_LIST_UNLOCK(&device->buttonconfig);

					change = sccp_config_applyDeviceConfiguration(device, v);
					sccp_log((DEBUGCAT_CORE)) ("%s: device has %s\n", device->id, change ? "major changes -> restarting device" : "no major changes -> skipping restart (minor changes applied)");
					pbx_cli(fd, "%s: device has %s\n", device->id, change ? "major changes -> restarting device" : "no major changes -> restart not required");
					if (change == SCCP_CONFIG_NEEDDEVICERESET) {
						device->pendingUpdate = 1;
						sccp_device_check_update(device);				// Will cleanup after reload and restart the device if necessary
					}
#ifdef CS_SCCP_REALTIME
					if (device->realtime) {
						pbx_variables_destroy(v);
					}
#endif
				} else {
					device->pendingDelete = 1;
				}

				returnval = RESULT_SUCCESS;
			} else {
				pbx_cli(fd, "Usage: sccp reload device [SEP00???????], device name required\n");
			}
			goto EXIT;
		} else if (sccp_strequals("line", argv[2])) {
			if (argc == 4) {
				AUTO_RELEASE(sccp_line_t, line , sccp_line_find_byname(argv[3], FALSE));
				PBX_VARIABLE_TYPE *v = NULL;

#ifdef CS_SCCP_REALTIME
				PBX_VARIABLE_TYPE *dv = NULL;
#endif
				if (!line) {
					pbx_cli(fd, "Could not find line %s\n", argv[3]);
					const char *utype;

					utype = pbx_variable_retrieve(GLOB(cfg), argv[3], "type");
					if (utype && !strcasecmp(utype, "line")) {
						line = sccp_line_create(argv[3]);
					} else {
						pbx_cli(fd, "Could not find line %s in config\n", argv[3]);
						goto EXIT;
					}
				}
				
				if (!line) {
					pbx_cli(fd, "Could not find/create line: '%s'\n", argv[3]);
					goto EXIT;
				}
#ifdef CS_SCCP_REALTIME
				if (line->realtime) {
					v = pbx_load_realtime(GLOB(realtimelinetable), "name", argv[3], NULL);
				} else
#endif
				{
					if ((CONFIG_STATUS_FILE_OK == sccp_config_getConfig(TRUE)) && GLOB(cfg)) {
						v = ast_variable_browse(GLOB(cfg), argv[3]);
					}
				}
				if (v) {
					change = sccp_config_applyLineConfiguration(line, v);
					sccp_log((DEBUGCAT_CORE)) ("%s: line has %s\n", line->name, change ? "major changes -> restarting attached devices" : "no major changes -> skipping restart (minor changes applied)");
					pbx_cli(fd, "%s: device has %s\n", line->name, change ? "major changes -> restarting attached devices" : "no major changes -> restart not required");
					if (change == SCCP_CONFIG_NEEDDEVICERESET) {
						sccp_linedevices_t *lineDevice = NULL;

						SCCP_LIST_LOCK(&line->devices);
						SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
							AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(lineDevice->device));
							if (device) {
								SCCP_LIST_LOCK(&device->buttonconfig);
								SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
									sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_4 "%s: Setting Button at Index:%d to pendingDelete\n", device->id, config->index);
									config->pendingDelete = 1;
								}
								SCCP_LIST_UNLOCK(&device->buttonconfig);
#ifdef CS_SCCP_REALTIME
								if (device->realtime) {
									if ((dv = pbx_load_realtime(GLOB(realtimedevicetable), "name", argv[3], NULL))) {
										change |= sccp_config_applyDeviceConfiguration(device, dv);
									}
								} else
#endif
								{
									if (GLOB(cfg)) {
										v = ast_variable_browse(GLOB(cfg), device->id);
										change |= sccp_config_applyDeviceConfiguration(device, v);
									}
								}
								device->pendingUpdate = 1;
								sccp_device_check_update(device);				// Will cleanup after reload and restart the device if necessary
#ifdef CS_SCCP_REALTIME
								if (device->realtime && dv) {
									pbx_variables_destroy(dv);
								}
#endif
							}
						}
						SCCP_LIST_UNLOCK(&line->devices);
					}
#ifdef CS_SCCP_REALTIME
					if (line->realtime) {
						pbx_variables_destroy(v);
					}
#endif
				} else {
					line->pendingDelete = 1;
				}

				returnval = RESULT_SUCCESS;
			} else {
				pbx_cli(fd, "Usage: sccp reload line [extension], line name required\n");
			}
			goto EXIT;
		} else if (sccp_strequals("force", argv[2]) && argc == 3) {
			pbx_cli(fd, "Force Reading Config file '%s'\n", GLOB(config_file_name));
			force_reload = TRUE;
		} else if (sccp_strequals("file", argv[2])) {
			if (argc == 4) {
				// build config file path
				char *buf;
				int buflen;
				if (argv[3][0] != '/') { 
					buflen = strlen(ast_config_AST_CONFIG_DIR) + strlen(argv[3]) + 2;
					buf = alloca(buflen);
					snprintf(buf, buflen, "%s/%s", ast_config_AST_CONFIG_DIR, argv[3]);
				} else {
					buf = pbx_strdupa(argv[3]);
				}
				// check file exists
				struct stat sb = { 0 };
				if (!(stat(buf, &sb) == 0 && S_ISREG(sb.st_mode))) {
					pbx_cli(fd, "The config file '%s' you requested to load could not be found at '%s' (check path/rights ?). Aborting reload\n", argv[3], buf);
					goto EXIT;
				}

				// load new config file
				pbx_cli(fd, "Using config file '%s' (previous config file: '%s')\n", argv[3], GLOB(config_file_name));
				if (!sccp_strequals(GLOB(config_file_name), buf)) {
					force_reload = TRUE;
				}
				if (GLOB(config_file_name)) {
					sccp_free(GLOB(config_file_name));
				}
				GLOB(config_file_name) = pbx_strdup(buf);
			} else {
				pbx_cli(fd, "Usage: sccp reload file [filename], filename is required\n");
				goto EXIT;
			}
		} else {
			returnval = RESULT_SHOWUSAGE;
			goto EXIT;
		}
	}
	sccp_config_file_status_t cfg = sccp_config_getConfig(force_reload);

	switch (cfg) {
		case CONFIG_STATUS_FILE_NOT_CHANGED:
			pbx_cli(fd, "config file '%s' has not changed, skipping reload.\n", GLOB(config_file_name));
			returnval = RESULT_SUCCESS;
			break;
		case CONFIG_STATUS_FILE_OK:
			if (GLOB(cfg)) {
				pbx_cli(fd, "SCCP reloading configuration. %p\n", GLOB(cfg));
				if (!sccp_config_general(SCCP_CONFIG_READRELOAD)) {
					pbx_cli(fd, "Unable to reload configuration.\n");
					returnval = RESULT_FAILURE;
					goto EXIT;
				}
				sccp_config_readDevicesLines(SCCP_CONFIG_READRELOAD);
				returnval = RESULT_SUCCESS;
			}
			break;
		case CONFIG_STATUS_FILE_OLD:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			break;
		case CONFIG_STATUS_FILE_NOT_SCCP:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "\n\n --> You are using an configuration file is not following the sccp format, please check '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			break;
		case CONFIG_STATUS_FILE_NOT_FOUND:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
			break;
		case CONFIG_STATUS_FILE_INVALID:
			pbx_cli(fd, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_cli(fd, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
			break;
	}
EXIT:
	pbx_rwlock_wrlock(&GLOB(lock));
	GLOB(reload_in_progress) = FALSE;
	pbx_rwlock_unlock(&GLOB(lock));
	return returnval;
}

static char reload_usage[] = "Usage: SCCP reload [force|file filename|device devicename|line linename]\n" "       Reloads SCCP configuration from sccp.conf or filename [force|file filename|device devicename|line linename]\n" "       (It will send a reset to all device which have changed (when they have an active channel reset will be postponed until device goes onhook))\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMPLETE SCCP_CLI_NULL_COMPLETER
#define CLI_COMMAND "sccp", "reload"
CLI_ENTRY(cli_reload, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#define CLI_COMMAND "sccp", "reload", "file"
CLI_ENTRY(cli_reload_file, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#define CLI_COMMAND "sccp", "reload", "force"
    CLI_ENTRY(cli_reload_force, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#define CLI_COMPLETE SCCP_CLI_DEVICE_COMPLETER
#define CLI_COMMAND "sccp", "reload", "device"
    CLI_ENTRY(cli_reload_device, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#define CLI_COMPLETE SCCP_CLI_LINE_COMPLETER
#define CLI_COMMAND "sccp", "reload", "line"
    CLI_ENTRY(cli_reload_line, sccp_cli_reload, "Reload the SCCP configuration", reload_usage, FALSE)
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
	int returnval = RESULT_FAILURE;
	char *config_file = "sccp.conf.new";

	if (argc < 2 || argc > 4) {
		return RESULT_SHOWUSAGE;
	}

	pbx_cli(fd, "SCCP: Generating new config file.\n");

	if (argc == 4) {
		config_file = pbx_strdupa(argv[3]);
	}
	if (!sccp_config_generate(config_file, 0)) {
		returnval = RESULT_SUCCESS;
	} else {
		pbx_cli(fd, "SCCP generation failed.\n");
	}

	return returnval;
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
	pbx_cli(fd, "%s", SCCP_VERSIONSTR);
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
     */
static int sccp_reset_restart(int fd, int argc, char *argv[])
{
	boolean_t restart = TRUE;

	if (argc < 3 || argc > 4) {
		return RESULT_SHOWUSAGE;
	}
	if (!strcasecmp(argv[1], "reset")) {
		if (argc == 4) {
			if (strcasecmp(argv[3], "restart")) {
				return RESULT_SHOWUSAGE;
			}
			restart = TRUE;
		} else {
			restart = FALSE;
		}
	} else if (argc != 3) {
		return RESULT_SHOWUSAGE;
	}
	pbx_cli(fd, VERBOSE_PREFIX_2 "%s: %s request sent to the device\n", argv[2], argv[1]);

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[2], FALSE));

	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	if (!d->session || sccp_device_getRegistrationState(d) != SKINNY_DEVICE_RS_OK) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		return RESULT_FAILURE;
	}
	
	/* sccp_device_clean will check active channels */
	if (d->active_channel) {
		pbx_cli(fd, "%s: unable to %s device with active channels. Hangup first\n", argv[2], (!strcasecmp(argv[1], "reset")) ? "reset" : "restart");
		return RESULT_FAILURE;
	}
	
	int res = 0;
	if (!restart) {
		res = sccp_device_sendReset(d, SKINNY_DEVICE_RESET);
	} else {
		res = sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
	}
	if (res <= 0) {
		sccp_session_stopthread(d->session, SKINNY_DEVICE_RS_NONE);
	}
	
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
     */
static int sccp_unregister(int fd, int argc, char *argv[])
{
	sccp_msg_t *msg = NULL;

	if (argc != 3) {
		return RESULT_SHOWUSAGE;
	}
	pbx_cli(fd, "%s: %s request sent to the device\n", argv[2], argv[1]);

	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[2], FALSE));
	if (!d) {
		pbx_cli(fd, "Can't find device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	if (!d->session) {
		pbx_cli(fd, "%s: device not registered\n", argv[2]);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "%s: Turn off the monitored line lamps to permit the %s\n", argv[2], argv[1]);

	REQ(msg, RegisterRejectMessage);
	sccp_copy_string(msg->data.RegisterRejectMessage.text, "Unregister user request", StationMaxDisplayTextSize);
	
	sccp_dev_send(d, msg);
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
	AUTO_RELEASE(sccp_channel_t, channel , NULL);
	if (argc < 3) {
		pbx_cli(fd, "argc is less then 2: %d\n", argc);
		return RESULT_SHOWUSAGE;
	}
	if (pbx_strlen_zero(argv[2])) {
		pbx_cli(fd, "string length of argv[2] is zero\n");
		return RESULT_SHOWUSAGE;
	}
	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[2], FALSE));
	if (!d) {
		pbx_cli(fd, "Can't find settings for device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	AUTO_RELEASE(sccp_line_t, line , NULL);
	if (d && d->defaultLineInstance > 0) {
		line = sccp_line_find_byid(d, d->defaultLineInstance);
	} else {
		line = sccp_dev_getActiveLine(d);
	}

	if (!line) {
		pbx_cli(fd, "Can't find line for device %s\n", argv[2]);
		return RESULT_FAILURE;
	}

	pbx_cli(fd, "Starting Call for Device: %s\n", argv[2]);
	channel = sccp_channel_newcall(line, d, argv[3], SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
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
     */
static int sccp_set_object(int fd, int argc, char *argv[])
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);
	PBX_VARIABLE_TYPE variable;
	int res;
	int cli_result = RESULT_SUCCESS;

	if (argc < 2) {
		return RESULT_SHOWUSAGE;
	}
	if (pbx_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}
	if (sccp_strcaseequals("channel", argv[2])) {
		if (argc < 5) {
			return RESULT_SHOWUSAGE;
		}
		if (pbx_strlen_zero(argv[3]) || pbx_strlen_zero(argv[4])) {
			return RESULT_SHOWUSAGE;
		}
		// sccp set channel SCCP/test123-123 hold on
		if (!strncasecmp("SCCP/", argv[3], 5)) {
			char line[80];
			int channel;

			sscanf(argv[3], "SCCP/%[^-]-%08x", line, &channel);
			// c = sccp_find_channel_on_line_byid(l, channeId);	// possible replacement, to also check if the line provided can be matched up.
			c = sccp_channel_find_byid(channel);
		} else {
			c = sccp_channel_find_byid(sccp_atoi(argv[3], strlen(argv[3])));
		}

		if (!c) {
			pbx_cli(fd, "Can't find channel for ID %s\n", argv[3]);
			return RESULT_FAILURE;
		}

		do {
			if (sccp_strcaseequals("hold", argv[4])) {
				if (argc < 6) {
					pbx_log(LOG_WARNING, "yes/no needs to be supplied\n");
					cli_result = RESULT_FAILURE;
					break;
				}
				if (sccp_strcaseequals("on", argv[5])) {					/* check to see if enable hold */
					pbx_cli(fd, "PLACING CHANNEL %s ON HOLD\n", argv[3]);
					sccp_channel_hold(c);
				} else if (!strcmp("off", argv[5])) {						/* check to see if disable hold */
					if (argc < 7) {
						pbx_cli(fd, "For resuming a channel from hold, you have to specify the resuming device\n%s %s %s %s %s %s <device>\n", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
						cli_result = RESULT_FAILURE;
						break;
					}
					char *dev;

					if (sccp_strcaseequals("device", argv[6])) {				/* 'device' str is optional during 'hold off' (to match old behaviour) */
						dev = pbx_strdupa(argv[7]);
					} else {
						dev = pbx_strdupa(argv[6]);
					}
					if (pbx_strlen_zero(dev)) {
						pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
						cli_result = RESULT_FAILURE;
						break;
					}
					AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(dev, FALSE));
					if (!d) {
						pbx_log(LOG_WARNING, "Device not found\n");
						cli_result = RESULT_FAILURE;
						break;
					}
					pbx_cli(fd, "PLACING CHANNEL %s OFF HOLD\n", argv[3]);

					sccp_channel_resume(d, c, FALSE);
				} else {
					/* wrong parameter value */
					return RESULT_SHOWUSAGE;
				}
			}
		} while (FALSE);

	} else if (sccp_strcaseequals("device", argv[2])) {
		if (argc < 6) {
			return RESULT_SHOWUSAGE;
		}
		if (pbx_strlen_zero(argv[3]) || pbx_strlen_zero(argv[4]) || pbx_strlen_zero(argv[5])) {
			return RESULT_SHOWUSAGE;
		}

		char *dev = pbx_strdupa(argv[3]);

		if (pbx_strlen_zero(dev)) {
			pbx_log(LOG_WARNING, "DeviceName needs to be supplied\n");
		}
		AUTO_RELEASE(sccp_device_t, device , sccp_device_find_byid(dev, FALSE));

		if (!device) {
			pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
			return RESULT_FAILURE;
		}

		if (!strcmp("ringtone", argv[4])) {
			device->setRingTone(device, argv[5]);

		} else if (!strcmp("backgroundImage", argv[4])) {
			device->setBackgroundImage(device, argv[5]);

		} else {
			variable.name = argv[4];
			variable.value = argv[5];
			variable.next = NULL;
			variable.file = "cli";
			variable.lineno = 0;

			res = sccp_config_applyDeviceConfiguration(device, &variable);

			if (res & SCCP_CONFIG_NEEDDEVICERESET) {
				device->pendingUpdate = 1;
			}
		}
	} else if (sccp_strcaseequals("fallback", argv[2])) {
		if (argc < 4) {
			return RESULT_SHOWUSAGE;
		}
		if (pbx_strlen_zero(argv[3])) {
			return RESULT_SHOWUSAGE;
		}
		char *fallback_option = pbx_strdupa(argv[3]);

		if (sccp_strcaseequals(fallback_option, "odd") || sccp_strcaseequals(fallback_option, "even") || sccp_true(fallback_option) || sccp_false(fallback_option)) {
			if (GLOB(token_fallback)) {
				sccp_free(GLOB(token_fallback));
			}
			GLOB(token_fallback) = pbx_strdup(fallback_option);
		} else if (strstr(fallback_option, "/") != NULL) {
			struct stat sb;

			if (stat(fallback_option, &sb) == 0 && sb.st_mode & S_IXUSR) {
				if (GLOB(token_fallback)) {
					sccp_free(GLOB(token_fallback));
				}
				GLOB(token_fallback) = pbx_strdup(fallback_option);
			} else {
				pbx_log(LOG_WARNING, "Script %s, either not found or not executable by this user\n", fallback_option);
				return RESULT_FAILURE;
			}
		} else if (sccp_strcaseequals(fallback_option, "path")) {
			pbx_log(LOG_WARNING, "Please specify a path to a script, using a fully qualified path (i.e. /etc/asterisk/tokenscript.sh)\n");
			return RESULT_FAILURE;
		} else {
			pbx_log(LOG_WARNING, "fallback option '%s' is unknown\n", fallback_option);
			return RESULT_FAILURE;
		}
		pbx_cli(fd, "New global fallback value: %s\n", GLOB(token_fallback));
	} else if (sccp_strcaseequals("debug", argv[2])) {
		int32_t new_debug = GLOB(debug);
		if (argc > 3) {
			new_debug = sccp_parse_debugline(argv, 3, argc, new_debug);
		}
		char *debugcategories = sccp_get_debugcategories(new_debug);

		if (argc > 3) {
			pbx_cli(fd, "SCCP new debug status: (%d -> %d) %s\n", GLOB(debug), new_debug, debugcategories);
		} else {
			pbx_cli(fd, "SCCP debug status: (%d) %s\n", GLOB(debug), debugcategories);
		}
		sccp_free(debugcategories);

		GLOB(debug) = new_debug;
		return RESULT_SUCCESS;
	} else {
		pbx_cli(fd, "ERROR: 'sccp set %s', Unknown argument '%s'\n\n", argv[2], argv[2]);
		return RESULT_SHOWUSAGE;
	}

	return cli_result;
}

static char set_object_usage[] = "Usage: sccp set channel|device|fallback|debug settings\n" \
				"sccp set " \
				"channel <channelId> hold <on/off>" \
				"|device <deviceId> [ringtone <ringtone>|backgroundImage <url>" \
				"|variable <variable>]" \
				"|fallback [true|false|odd|even|script path]" \
				"|debug [[no] <debugcategory>|none]\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "set"
#define CLI_COMPLETE SCCP_CLI_SET_COMPLETER
CLI_ENTRY(cli_set_object, sccp_set_object, "Set channel|device settings", set_object_usage, TRUE)
#undef CLI_COMMAND
#undef CLI_COMPLETE
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
    /* --------------------------------------------------------------------------------------------------------------REMOTE ANSWER- */
    /*!
     * \brief Answer a Remote Channel
     * \param fd Fd as int
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     */
static int sccp_answercall(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	int local_line_total = 0;
	int res = RESULT_SUCCESS;
	char error[100] = "";

	if (argc != 4 || pbx_strlen_zero(argv[2]) || pbx_strlen_zero(argv[3])) {
		return RESULT_SHOWUSAGE;
	}

	if (!strncasecmp("SCCP/", argv[2], 5)) {
		char line[80];
		int channelId;

		sscanf(argv[2], "SCCP/%[^-]-%08x", line, &channelId);
		// c = sccp_find_channel_on_line_byid(l, channeId);	// possible replacement, to also check if the line provided can be matched up.
		c = sccp_channel_find_byid(channelId);
	} else {
		c = sccp_channel_find_byid(sccp_atoi(argv[2], strlen(argv[2])));
	}

	if (c) {
		if (c->state == SCCP_CHANNELSTATE_RINGING) {
			AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(argv[3], FALSE));
			if (d) {
				sccp_channel_answer(d, c);
				if (c->owner) {
					iPbx.queue_control(c->owner, AST_CONTROL_ANSWER);
				}
				res = RESULT_SUCCESS;
			} else {
				pbx_log(LOG_WARNING, "SCCP: (sccp_answercall) Device %s not found\n", argv[3]);
				snprintf(error, sizeof(error), "SCCP: (sccp_answercall) Device %s not found\n", argv[3]);
				res = RESULT_FAILURE;
			}
		} else {
			pbx_log(LOG_WARNING, "SCCP: (sccp_answercall) Channel %s needs to be ringing and incoming, to be answered\n", c->designator);
			snprintf(error, sizeof(error), "SCCP: (sccp_answercall) Channel %s needs to be ringing and incoming, to be answered\n", c->designator);
			res = RESULT_FAILURE;
		}
	} else {
		pbx_log(LOG_WARNING, "SCCP: (sccp_answercall) Channel %s is not active\n", argv[2]);
		snprintf(error, sizeof(error), "SCCP: (sccp_answercall) Channel %s is not active\n", argv[2]);
		res = RESULT_FAILURE;
	}

	if (res == RESULT_FAILURE && !sccp_strlen_zero(error)) {
		CLI_AMI_RETURN_ERROR(fd, s, m, "%s\n", error);		/* explicit return */
	}

	if (s) {
		totals->lines = local_line_total;
	}

	return res;
}

static char cli_answercall_usage[] = "Usage: sccp answercall channelId deviceId\n" "       Answer a ringing incoming channel on device.\n";
static char ami_answercall_usage[] = "Usage: SCCPAsnwerCall1\n" "Answer a ringing incoming channel on device.\n\n" "PARAMS: ChannelId,DeviceId\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "answer"
#define AMI_COMMAND "SCCPAsnwerCall1"
#define CLI_COMPLETE SCCP_CLI_CHANNEL_COMPLETER, SCCP_CLI_CONNECTED_DEVICE_COMPLETER
#define CLI_AMI_PARAMS "ChannelId", "DeviceId"
CLI_AMI_ENTRY(answercall, sccp_answercall, "Answer a ringing incoming channel on device", cli_answercall_usage, FALSE, FALSE)
#undef CLI_AMI_PARAMS
#undef AMI_COMMAND
#undef CLI_COMPLETE
#undef CLI_COMMAND
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
     */
static int sccp_end_call(int fd, int argc, char *argv[])
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (argc < 3) {
		return RESULT_SHOWUSAGE;
	}
	if (pbx_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}
	if (!strncasecmp("SCCP/", argv[2], 5)) {
		char line[80];
		int channel;

		sscanf(argv[2], "SCCP/%[^-]-%08x", line, &channel);
		c = sccp_channel_find_byid(channel);
	} else {
		c = sccp_channel_find_byid(sccp_atoi(argv[2], strlen(argv[2])));
	}
	if (!c) {
		pbx_cli(fd, "Can't find channel for ID %s\n", argv[2]);
		return RESULT_FAILURE;
	}
	pbx_cli(fd, "ENDING CALL ON CHANNEL %s \n", argv[2]);
	sccp_channel_endcall(c);
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
     * \param totals Total number of lines as int
     * \param s AMI Session
     * \param m Message
     * \param argc Argc as int
     * \param argv[] Argv[] as char
     * \return Result as int
     * 
     * \called_from_asterisk
     */
static int sccp_tokenack(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;
	const char *dev;

	if (argc < 3 || sccp_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}
	dev = pbx_strdupa(argv[2]);
	
	AUTO_RELEASE(sccp_device_t, d , sccp_device_find_byid(dev, FALSE));
	if (!d) {
		pbx_log(LOG_WARNING, "Failed to get device %s\n", dev);
		CLI_AMI_RETURN_ERROR(fd, s, m, "Can't find settings for device %s\n", dev);		/* explicit return */
	}

	if (d->status.token != SCCP_TOKEN_STATE_REJ && d->session) {
		pbx_log(LOG_WARNING, "%s: We need to have received a token request before we can acknowledge it\n", dev);
		CLI_AMI_RETURN_ERROR(fd, s, m, "%s: We need to have received a token request before we can acknowledge it\n", dev);		/* explicit return */
	} 
	if (d->session) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Sending phone a token acknowledgement\n", dev);
		sccp_session_tokenAck(d->session);
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Phone not connected to this server (no valid session)\n", dev);
	}
	
	if (s) {
		totals->lines = local_line_total;
	}
	return RESULT_SUCCESS;
}

static char cli_tokenack_usage[] = "Usage: sccp tokenack <deviceId>\n" "Send Token Acknowlegde. Makes a phone switch servers on demand (used in clustering)\n";
static char ami_tokenack_usage[] = "Usage: SCCPTokenAck\n" "Send Token Acknowledge to device. Makes a phone switch servers on demand (used in clustering)\n\n" "PARAMS: DeviceId\n";

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_COMMAND "sccp", "tokenack"
#define CLI_COMPLETE SCCP_CLI_CONNECTED_DEVICE_COMPLETER
#define AMI_COMMAND "SCCPTokenAck"
#define CLI_AMI_PARAMS "DeviceId"
CLI_AMI_ENTRY(tokenack, sccp_tokenack, "Send TokenAck", cli_tokenack_usage, FALSE, FALSE)
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
	AST_CLI_DEFINE(cli_reload_file, "SCCP module reload file."),
	AST_CLI_DEFINE(cli_reload_force, "SCCP module reload force."),
	AST_CLI_DEFINE(cli_reload_device, "SCCP module reload device."),
	AST_CLI_DEFINE(cli_reload_line, "SCCP module reload line."),
	AST_CLI_DEFINE(cli_restart, "Restart an SCCP device"),
	AST_CLI_DEFINE(cli_reset, "Reset an SCCP Device"),
	AST_CLI_DEFINE(cli_start_call, "Start a Call."),
	AST_CLI_DEFINE(cli_end_call, "End a Call."),
	AST_CLI_DEFINE(cli_set_object, "Change channel/device settings."),
	AST_CLI_DEFINE(cli_answercall, "Remotely answer a call."),
#ifdef CS_EXPERIMENTAL
	AST_CLI_DEFINE(cli_test, "Test message."),
#endif
	AST_CLI_DEFINE(cli_show_refcount, "Test message."),
	AST_CLI_DEFINE(cli_tokenack, "Send Token Acknowledgement."),
#ifdef CS_SCCP_CONFERENCE
	AST_CLI_DEFINE(cli_show_conferences, "Show running SCCP Conferences."),
	AST_CLI_DEFINE(cli_show_conference, "Show SCCP Conference Info."),
	AST_CLI_DEFINE(cli_conference_command, "SCCP Conference Commands."),
#endif
	AST_CLI_DEFINE(cli_show_hint_lineStates, "Show all hint lineStates"),
	AST_CLI_DEFINE(cli_show_hint_subscriptions, "Show all hint subscriptions")
};

/*!
 * register CLI functions from asterisk
 */
int sccp_register_cli(void)
{
	uint i, res = 0;

	for (i = 0; i < ARRAY_LEN(cli_entries); i++) {
		sccp_log((DEBUGCAT_CLI)) (VERBOSE_PREFIX_2 "Cli registered action %s\n", (cli_entries + i)->_full_cmd);
		res |= pbx_cli_register(cli_entries + i);
	}

#if ASTERISK_VERSION_NUMBER < 10600
#define _MAN_COM_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND
#define _MAN_REP_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#else
#define _MAN_COM_FLAGS	(EVENT_FLAG_SYSTEM | EVENT_FLAG_COMMAND)
#define _MAN_REP_FLAGS	(EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING)
#endif
	res |= pbx_manager_register("SCCPShowGlobals", _MAN_REP_FLAGS, manager_show_globals, "show globals setting", ami_globals_usage);
	res |= pbx_manager_register("SCCPShowDevices", _MAN_REP_FLAGS, manager_show_devices, "show devices", ami_devices_usage);
	res |= pbx_manager_register("SCCPShowDevice", _MAN_REP_FLAGS, manager_show_device, "show device settings", ami_device_usage);
	res |= pbx_manager_register("SCCPShowLines", _MAN_REP_FLAGS, manager_show_lines, "show lines", ami_lines_usage);
	res |= pbx_manager_register("SCCPShowLine", _MAN_REP_FLAGS, manager_show_line, "show line", ami_line_usage);
	res |= pbx_manager_register("SCCPShowChannels", _MAN_REP_FLAGS, manager_show_channels, "show channels", ami_channels_usage);
	res |= pbx_manager_register("SCCPShowSessions", _MAN_REP_FLAGS, manager_show_sessions, "show sessions", ami_sessions_usage);
	res |= pbx_manager_register("SCCPShowMWISubscriptions", _MAN_REP_FLAGS, manager_show_mwi_subscriptions, "show mwi subscriptions", ami_mwi_subscriptions_usage);
	res |= pbx_manager_register("SCCPShowSoftkeySets", _MAN_REP_FLAGS, manager_show_softkeysets, "show softkey sets", ami_show_softkeysets_usage);
	res |= pbx_manager_register("SCCPMessageDevices", _MAN_REP_FLAGS, manager_message_devices, "message devices", ami_message_devices_usage);
	res |= pbx_manager_register("SCCPMessageDevice", _MAN_REP_FLAGS, manager_message_device, "message device", ami_message_device_usage);
	res |= pbx_manager_register("SCCPSystemMessage", _MAN_REP_FLAGS, manager_system_message, "system message", ami_system_message_usage);
	res |= pbx_manager_register("SCCPDndDevice", _MAN_REP_FLAGS, manager_dnd_device, "set/unset dnd on device", ami_dnd_device_usage);
	res |= pbx_manager_register("SCCPAnswerCall1", _MAN_REP_FLAGS, manager_answercall, "Answer Ringing Incoming Channel on Device", ami_answercall_usage);
	res |= pbx_manager_register("SCCPTokenAck", _MAN_REP_FLAGS, manager_tokenack, "send tokenack", ami_tokenack_usage);
#ifdef CS_SCCP_CONFERENCE
	res |= pbx_manager_register("SCCPShowConferences", _MAN_REP_FLAGS, manager_show_conferences, "show conferences", ami_conferences_usage);
	res |= pbx_manager_register("SCCPShowConference", _MAN_REP_FLAGS, manager_show_conference, "show conference", ami_conference_usage);
	res |= pbx_manager_register("SCCPConference", _MAN_REP_FLAGS, manager_conference_command, "conference commands", ami_conference_command_usage);
#endif
	res |= pbx_manager_register("SCCPShowHintLineStates", _MAN_REP_FLAGS, manager_show_hint_lineStates, "show hint lineStates", ami_show_hint_lineStates_usage);
	res |= pbx_manager_register("SCCPShowHintSubscriptions", _MAN_REP_FLAGS, manager_show_hint_subscriptions, "show hint subscriptions", ami_show_hint_subscriptions_usage);
	res |= pbx_manager_register("SCCPShowRefcount", _MAN_REP_FLAGS, manager_show_refcount, "show refcount", ami_show_refcount_usage);

	return res;
}

/*!
 * unregister CLI functions from asterisk
 */
int sccp_unregister_cli(void)
{

	uint i, res = 0;

	for (i = 0; i < ARRAY_LEN(cli_entries); i++) {
		sccp_log((DEBUGCAT_CLI)) (VERBOSE_PREFIX_2 "Cli unregistered action %s\n", (cli_entries + i)->_full_cmd);
		res |= pbx_cli_unregister(cli_entries + i);
	}
	res |= pbx_manager_unregister("SCCPShowGlobals");
	res |= pbx_manager_unregister("SCCPShowDevices");
	res |= pbx_manager_unregister("SCCPShowDevice");
	res |= pbx_manager_unregister("SCCPShowLines");
	res |= pbx_manager_unregister("SCCPShowLine");
	res |= pbx_manager_unregister("SCCPShowChannels");
	res |= pbx_manager_unregister("SCCPShowSessions");
	res |= pbx_manager_unregister("SCCPShowMWISubscriptions");
	res |= pbx_manager_unregister("SCCPShowSoftkeySets");
	res |= pbx_manager_unregister("SCCPMessageDevices");
	res |= pbx_manager_unregister("SCCPMessageDevice");
	res |= pbx_manager_unregister("SCCPSystemMessage");
	res |= pbx_manager_unregister("SCCPDndDevice");
	res |= pbx_manager_unregister("SCCPAnswerCall1");
	res |= pbx_manager_unregister("SCCPTokenAck");
#ifdef CS_SCCP_CONFERENCE
	res |= pbx_manager_unregister("SCCPShowConferences");
	res |= pbx_manager_unregister("SCCPShowConference");
	res |= pbx_manager_unregister("SCCPConference");
#endif
	res |= pbx_manager_unregister("SCCPShowHintLineStates");
	res |= pbx_manager_unregister("SCCPShowHintSubscriptions");
	res |= pbx_manager_unregister("SCCPShowRefcount");

	return res;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
