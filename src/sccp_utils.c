/*!
 * \file        sccp_utils.c
 * \brief       SCCP Utils Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_device.h"
#include "sccp_utils.h"
#include "sccp_socket.h"
#if HAVE_ICONV_H
#include <iconv.h>
#endif
SCCP_FILE_VERSION(__FILE__, "$Revision$");

/*!
 * \brief Print out a messagebuffer
 * \param messagebuffer Pointer to Message Buffer as char
 * \param len Lenght as Int
 */
void sccp_dump_packet(unsigned char *messagebuffer, int len)
{
	static const int numcolumns = 16;									// number output columns

	if (len <= 0 || !messagebuffer || !strlen((const char *) messagebuffer)) {				// safe quard
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
		// memset(hexout, 0, sizeof(hexout));
		memset(hexout, 0, (numcolumns * 3) + (numcolumns / 8) + 1);
		// memset(chrout, 0, sizeof(chrout));
		memset(chrout, 0, numcolumns + 1);
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

void sccp_dump_msg(sccp_msg_t * msg)
{
	sccp_dump_packet((unsigned char *) msg, letohl(msg->header.length) + 8);
}

/*!
 * \brief Return Number of Buttons on AddOn Device
 * \param d SCCP Device
 * \return taps (Number of Buttons on AddOn Device)
 * 
 */
int sccp_addons_taps(sccp_device_t * d)
{
	sccp_addon_t *cur = NULL;
	int taps = 0;

	if (SCCP_LIST_GETSIZE(&d->addons) > 0 && (d->skinny_type == SKINNY_DEVICETYPE_CISCO7941 || d->skinny_type == SKINNY_DEVICETYPE_CISCO7941GE)) {
		pbx_log(LOG_WARNING, "%s: %s devices do no support AddOns/Expansion Modules of any kind. Skipping !\n", DEV_ID_LOG(d), skinny_devicetype2str(d->skinny_type));
	}

	if (!strcasecmp(d->config_type, "7914")) {
		pbx_log(LOG_WARNING, "%s: config_type '%s' forces addon compatibily mode. Possibly faulty config file.\n", DEV_ID_LOG(d), d->config_type);
		return 28;											// in compatibility mode it returns 28 taps for a double 7914 addon
	}
	SCCP_LIST_LOCK(&d->addons);
	SCCP_LIST_TRAVERSE(&d->addons, cur, list) {
		if (cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_7914) {
			taps += 14;
		}
		if (cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_7915_12BUTTON || cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_7916_12BUTTON) {
			taps += 12;
		}
		if (cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON || cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON) {
			taps += 24;
		}
		/* should maybe check if connected via SPCS */
		if (cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S || cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS || cur->type == SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS) {
			taps += 32;
		}
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

	if (!d) {
		return;
	}
	// while ((AST_LIST_REMOVE_HEAD(&d->addons, list))) ;
	while ((addon = SCCP_LIST_REMOVE_HEAD(&d->addons, list))) {
		sccp_free(addon);
	}
	d->addons.first = NULL;
	d->addons.last = NULL;
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
 * \brief Notify asterisk for new state
 * \param channel SCCP Channel
 * \param state New State - type of AST_STATE_*
 */
void sccp_pbx_setcallstate(sccp_channel_t * channel, int state)
{
	if (channel) {
		if (channel->owner) {
			pbx_setstate(channel->owner, state);
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Set asterisk state %s (%d) for call %d\n", channel->currentDeviceId, pbx_state2str(state), state, channel->callid);
		}
	}
}

/*!
 * \brief Clean Asterisk Database Entries in the "SCCP" Family
 * 
 */
void sccp_dev_dbclean(void)
{
	struct ast_db_entry *entry = NULL;
	sccp_device_t *d;
	char key[256];

	//! \todo write an pbx implementation for that
	//entry = PBX(feature_getFromDatabase)tree("SCCP", NULL);
	while (entry) {
		sscanf(entry->key, "/SCCP/%s", key);
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Looking for '%s' in the devices list\n", key);
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
				sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: device '%s' removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry) {
		pbx_db_freetree(entry);
	}
}

gcc_inline const char *msgtype2str(sccp_mid_t type)
{														/* sccp_protocol.h */
	return sccp_messagetypes[type].text;
}

gcc_inline size_t msgtype2size(sccp_mid_t type)
{														/* sccp_protocol.h */
	return sccp_messagetypes[type].size + SCCP_PACKET_HEADER;
}

gcc_inline const char *pbxsccp_devicestate2str(uint32_t value)
{														/* pbx_impl/ast/ast.h */
	_ARR2STR(pbx_devicestates, devicestate, value, text);
}

gcc_inline const char *extensionstatus2str(uint32_t value)
{														/* pbx_impl/ast/ast.h */
	_ARR2STR(sccp_extension_states, extension_state, value, text);
}

gcc_inline const char *label2str(uint16_t value)
{														/* sccp_labels.h */
	_ARR2STR(skinny_labels, label, value, text);
}

gcc_inline const char *codec2str(skinny_codec_t value)
{														/* sccp_protocol.h */
	_ARR2STR(skinny_codecs, codec, value, text);
}

gcc_inline int codec2payload(skinny_codec_t value)
{														/* sccp_protocol.h */
	_ARR2INT(skinny_codecs, codec, value, rtp_payload_type);
}

gcc_inline const char *codec2key(skinny_codec_t value)
{														/* sccp_protocol.h */
	_ARR2STR(skinny_codecs, codec, value, key);
}

gcc_inline const char *codec2name(skinny_codec_t value)
{														/* sccp_protocol.h */
	_ARR2STR(skinny_codecs, codec, value, name);
}

gcc_inline const char *featureType2str(sccp_feature_type_t value)
{														/* chan_sccp.h */
	_ARR2STR(sccp_feature_types, featureType, value, text);
}

gcc_inline uint32_t debugcat2int(const char *str)
{														/* chan_sccp.h */
	_STRARR2INT(sccp_debug_categories, key, str, category);
}

gcc_inline uint32_t labelstr2int(const char *str)
{														/* chan_sccp.h */
	_STRARR2INT(skinny_labels, text, str, label);
}

/*!
 * \brief Retrieve the string of format numbers and names from an array of formats
 * Buffer needs to be declared and freed afterwards
 * \param buf   Buffer as char array
 * \param size  Size of Buffer
 * \param codecs Array of Skinny Codecs
 * \param length Max Length
 */
char *sccp_multiple_codecs2str(char *buf, size_t size, const skinny_codec_t * codecs, const int length)
{
	int x;
	unsigned len;
	char *start, *end = buf;

	if (!size) {
		return buf;
	}
	snprintf(end, size, "(");
	len = strlen(end);
	end += len;
	size -= len;
	start = end;
	for (x = 0; x < length; x++) {
		if (codecs[x] == 0) {
			break;
		}

		snprintf(end, size, "%s (%d), ", codec2name(codecs[x]), codecs[x]);
		len = strlen(end);
		end += len;
		size -= len;
	}
	if (start == end) {
		pbx_copy_string(start, "nothing)", size);
	} else if (size > 2) {
		*(end - 2) = ')';
		*(end - 1) = '\0';
	}
	return buf;
}

/*!
 * \brief Remove Codec from Skinny Codec Preferences
 */
static void skinny_codec_pref_remove(skinny_codec_t * skinny_codec_prefs, skinny_codec_t skinny_codec)
{
	int x = 0;
	int found = 0;

	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		if (!found) {
			if (skinny_codec_prefs[x] == SKINNY_CODEC_NONE) {					// exit early if the rest can only be NONE
				return;
			}
			if (skinny_codec_prefs[x] == skinny_codec) {
				//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "found codec '%s (%d)' at pos %d\n", codec2name(skinny_codec), skinny_codec, x);
				found = 1;
			}
		} else {
			if (x + 1 < SKINNY_MAX_CAPABILITIES) {							// move all remaining member left one, deleting the found one
				//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "moving %d to %d\n", x+1, x);
				skinny_codec_prefs[x] = skinny_codec_prefs[x + 1];
			}
			if (skinny_codec_prefs[x + 1] == SKINNY_CODEC_NONE) {					// exit early if the rest can only be NONE
				return;
			}
		}
	}
}

/*!
 * \brief Append Codec to Skinny Codec Preferences
 */
static int skinny_codec_pref_append(skinny_codec_t * skinny_codec_pref, skinny_codec_t skinny_codec)
{
	int x = 0;

	// append behaviour: remove old entry, move all other entries left, append 
	skinny_codec_pref_remove(skinny_codec_pref, skinny_codec);

	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		// insert behaviour: skip if already there (cheaper)
		/*
		if (skinny_codec_pref[x] == skinny_codec) {
			return x;
		}
		*/
		if (SKINNY_CODEC_NONE == skinny_codec_pref[x]) {
			sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "inserting codec '%s (%d)' at pos %d\n", codec2name(skinny_codec), skinny_codec, x);
			skinny_codec_pref[x] = skinny_codec;
			return x;
		}
	}
	return -1;
}

/*!
 * \brief Parse Skinny Codec Allow / Disallow Config Lines
 */
int sccp_parse_allow_disallow(skinny_codec_t * skinny_codec_prefs, const char *list, int allowing)
{
	int errors = 0;

	if (!skinny_codec_prefs) {
		return -1;
	}
	unsigned int x;
	boolean_t all = FALSE;
	boolean_t found = FALSE;
	char *parse = NULL, *token = NULL;
	skinny_codec_t codec;

	parse = sccp_strdupa(list);
	while ((token = strsep(&parse, ","))) {
		if (!sccp_strlen_zero(token)) {
			all = sccp_strcaseequals(token, "all") ? TRUE : FALSE;
			if (all && !allowing) {									// disallowing all
				memset(skinny_codec_prefs, 0, SKINNY_MAX_CAPABILITIES);
				sccp_log((DEBUGCAT_CODEC)) ("SCCP: disallow=all => reset codecs\n");
				break;
			}
			for (x = 0; x < ARRAY_LEN(skinny_codecs); x++) {
				if (all || sccp_strcaseequals(skinny_codecs[x].key, token)) {
					codec = skinny_codecs[x].codec;
					found = TRUE;
					if (allowing) {
						//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "appending codec '%s'\n", codec2name(codec));
						skinny_codec_pref_append(skinny_codec_prefs, codec);
					} else {
						//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "removing codec '%s'\n", codec2name(codec));
						skinny_codec_pref_remove(skinny_codec_prefs, codec);
					}
				}
			}
			if (!found) {
				pbx_log(LOG_WARNING, "Cannot %s unknown codec '%s'\n", allowing ? "allow" : "disallow", token);
				errors++;
				continue;
			}
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

#ifndef HAVE_PBX_STRINGS_H

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
	if (s) {
		pbx_trim_blanks(s);
	}
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

	if (!buf || !array || !arraylen) {
		return 0;
	}
	memset(array, 0, arraylen * sizeof(*array));

	scan = buf;

	for (argc = 0; *scan && (argc < arraylen - 1); argc++) {
		array[argc] = scan;
		for (; *scan; scan++) {
			if (*scan == '(') {
				paren++;
			} else if (*scan == ')') {
				if (paren) {
					paren--;
				}
			} else if ((*scan == delim) && !paren) {
				*scan++ = '\0';
				break;
			}
		}
	}

	if (*scan) {
		array[argc++] = scan;
	}
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
 * \param sas   Socket Address In
 * \return SCCP Device
 * 
 */
//sccp_device_t *sccp_device_find_byipaddress(unsigned long s_addr)
sccp_device_t *sccp_device_find_byipaddress(struct sockaddr_storage * sas)
{
	sccp_device_t *d = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (d->session && sccp_socket_cmp_addr(&d->session->sin, sas) == 0) {
			d = sccp_device_retain(d);
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	return d;
}

/*!
 * \brief convert Feature String 2 Feature ID
 * \param str Feature Str as char
 * \return Feature Type
 */
sccp_feature_type_t sccp_featureStr2featureID(const char *const str)
{
	if (!str) {
		return SCCP_FEATURE_UNKNOWN;
	}
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
 *  - device->buttonconfig is not always locked
 *  - line->devices is not always locked
 */
void sccp_util_featureStorageBackend(const sccp_event_t * event)
{
	char family[25];
	char cfwdDeviceLineStore[60];										/* backward compatibiliy SCCP/Device/Line */
	char cfwdLineDeviceStore[60];										/* new format cfwd: SCCP/Line/Device */
	sccp_linedevices_t *linedevice = NULL;
	sccp_device_t *device = NULL;

	if (!event || !(device = event->event.featureChanged.device)) {
		return;
	}

	sccp_log((DEBUGCAT_EVENT + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: StorageBackend got Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), featureType2str(event->event.featureChanged.featureType), event->event.featureChanged.featureType);
	sprintf(family, "SCCP/%s", device->id);

	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_CFWDNONE:
		case SCCP_FEATURE_CFWDBUSY:
		case SCCP_FEATURE_CFWDALL:
			if ((linedevice = event->event.featureChanged.optional_linedevice)) {
				sccp_line_t *line = linedevice->line;
				uint8_t instance = linedevice->lineInstance;

				sccp_dev_forward_status(line, instance, device);
				sprintf(cfwdDeviceLineStore, "SCCP/%s/%s", device->id, line->name);
				sprintf(cfwdLineDeviceStore, "SCCP/%s/%s", line->name, device->id);
				switch (event->event.featureChanged.featureType) {
					case SCCP_FEATURE_CFWDALL:
						if (linedevice->cfwdAll.enabled) {
							PBX(feature_addToDatabase) (cfwdDeviceLineStore, "cfwdAll", linedevice->cfwdAll.number);
							PBX(feature_addToDatabase) (cfwdLineDeviceStore, "cfwdAll", linedevice->cfwdAll.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						} else {
							PBX(feature_removeFromDatabase) (cfwdDeviceLineStore, "cfwdAll");
							PBX(feature_removeFromDatabase) (cfwdLineDeviceStore, "cfwdAll");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDBUSY:
						if (linedevice->cfwdBusy.enabled) {
							PBX(feature_addToDatabase) (cfwdDeviceLineStore, "cfwdBusy", linedevice->cfwdBusy.number);
							PBX(feature_addToDatabase) (cfwdLineDeviceStore, "cfwdBusy", linedevice->cfwdBusy.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						} else {
							PBX(feature_removeFromDatabase) (cfwdDeviceLineStore, "cfwdBusy");
							PBX(feature_removeFromDatabase) (cfwdLineDeviceStore, "cfwdBusy");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDNONE:
						PBX(feature_removeFromDatabase) (cfwdDeviceLineStore, "cfwdAll");
						PBX(feature_removeFromDatabase) (cfwdDeviceLineStore, "cfwdBusy");
						PBX(feature_removeFromDatabase) (cfwdLineDeviceStore, "cfwdAll");
						PBX(feature_removeFromDatabase) (cfwdLineDeviceStore, "cfwdBusy");
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: cfwd cleared from db\n", DEV_ID_LOG(device));
					default:
						break;
				}
			}
			break;
		case SCCP_FEATURE_DND:
			// sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to %s\n", DEV_ID_LOG(device), device->dndFeature.status ? "on" : "off");
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
				device->privacyFeature.previousStatus = device->privacyFeature.status;
			}
			break;
		case SCCP_FEATURE_MONITOR:
			if (device->monitorFeature.previousStatus != device->monitorFeature.status) {
				if (device->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) {
					PBX(feature_addToDatabase) (family, "monitor", "on");
				} else {
					PBX(feature_removeFromDatabase) (family, "monitor");
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

	assert(NULL != labelString);

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
	filterPhones = FALSE;											/* set the default to call all phones */

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
	} else if (0 != strlen(subscriptionIdNum) &&								/* We already know that we won't search for a trivial subscriptionId. */
		   (0 != strncasecmp(channel->subscriptionId.number, subscriptionIdNum, strlen(channel->subscriptionId.number)))) {	/* Do the match! */
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

			new_size += strlen(sccp_debug_categories[i].key) + 1 /*sizeof(sep) */  + 1;
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
 * \param type LineType or LineOptions as uint32_t
 * \param dirNum the dirNum (e.g. line->cid_num)
 * \param fqdn line description (top right o the first line)
 * \param lineDisplayName label on the display
 * \return LineStatDynamicMessage as sccp_msg_t *
 *
 * \callgraph
 * \callergraph
 */
sccp_msg_t *sccp_utils_buildLineStatDynamicMessage(uint32_t lineInstance, uint32_t type, const char *dirNum, const char *fqdn, const char *lineDisplayName)
{
	sccp_msg_t *msg = NULL;
	int dirNum_len = (dirNum != NULL) ? strlen(dirNum) : 0;
	int FQDN_len = (fqdn != NULL) ? strlen(fqdn) : 0;
	int lineDisplayName_len = (lineDisplayName != NULL) ? strlen(lineDisplayName) : 0;
	int dummy_len = dirNum_len + FQDN_len + lineDisplayName_len;

	int hdr_len = 8 - 1;
	int padding = 4;											/* after each string + 1 */
	int size = hdr_len + dummy_len + padding;

	/* message size must be multiple of 4 */
	if ((size % 4) > 0) {
		size = size + (4 - (size % 4));
	}

	msg = sccp_build_packet(LineStatDynamicMessage, size);
	msg->data.LineStatDynamicMessage.lel_lineNumber = htolel(lineInstance);
	//msg->data.LineStatDynamicMessage.lel_lineType = htolel(0x0f);
	msg->data.LineStatDynamicMessage.lel_lineType = htolel(type);

	if (dummy_len) {
		char buffer[dummy_len + padding];

		// memset(&buffer[0], 0, sizeof(buffer));
		memset(&buffer[0], 0, dummy_len + padding);

		if (dirNum_len) {
			memcpy(&buffer[0], dirNum, dirNum_len);
		}
		if (FQDN_len) {
			memcpy(&buffer[dirNum_len + 1], fqdn, FQDN_len);
		}
		if (lineDisplayName_len) {
			memcpy(&buffer[dirNum_len + FQDN_len + 2], lineDisplayName, lineDisplayName_len);
		}
		// memcpy(&msg->data.LineStatDynamicMessage.dummy, &buffer[0], sizeof(buffer));
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "LineStatDynamicMessage.dummy: %s\n", buffer);
		memcpy(&msg->data.LineStatDynamicMessage.dummy, &buffer[0], dummy_len + padding);
	}

	return msg;
}

/*!
 * \brief Compare the information of two socket with one another
 * \param s0 Socket Information
 * \param s1 Socket Information
 * \return success as int
 *
 * \retval 0 on diff
 * \retval 1 on equal
 */
int socket_equals(struct sockaddr_storage *s0, struct sockaddr_storage *s1)
{
	/*
	if (s0->sin_addr.s_addr != s1->sin_addr.s_addr || s0->sin_port != s1->sin_port || s0->sin_family != s1->sin_family) {
		return 0;
	}
	*/

	//if (IN6_ARE_ADDR_EQUAL(s0, s1) && s0->ss_family == s1->ss_family ){
	if (sccp_socket_cmp_addr(s0, s1) && s0->ss_family == s1->ss_family) {
		return 1;
	}

	return 0;
}

/*!
 * \brief SCCP version of strlen_zero
 * \param data String to be checked
 * \return zerolength as boolean
 *
 * \retval FALSE on non zero length
 * \retval TRUE on zero length
 */
gcc_inline boolean_t sccp_strlen_zero(const char *data)
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
gcc_inline size_t sccp_strlen(const char *data)
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
gcc_inline boolean_t sccp_strequals(const char *data1, const char *data2)
{
	if (sccp_strlen_zero(data1) && sccp_strlen_zero(data2)) {
		return TRUE;
	} else if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2) && (sccp_strlen(data1) == sccp_strlen(data2))) {
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
gcc_inline boolean_t sccp_strcaseequals(const char *data1, const char *data2)
{
	if (sccp_strlen_zero(data1) && sccp_strlen_zero(data2)) {
		return TRUE;
	} else if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2) && (sccp_strlen(data1) == sccp_strlen(data2))) {
		return !strcasecmp(data1, data2);
	}
	return FALSE;
}

int sccp_strIsNumeric(const char *s)
{
	if (*s) {
		char c;

		while ((c = *s++)) {
			if (!isdigit(c)) {
				return 0;
			}
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

	sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "pLength %d, cLength: %d, rLength: %d\n", pLength, cLength, rLength);

	/*
	   char pref_buf[256];
	   sccp_multiple_codecs2str(pref_buf, sizeof(pref_buf) - 1, ourPreferences, (int)pLength);
	   char cap_buf[256];
	   sccp_multiple_codecs2str(cap_buf, sizeof(pref_buf) - 1, ourCapabilities, cLength);
	   char remote_cap_buf[256];
	   sccp_multiple_codecs2str(remote_cap_buf, sizeof(remote_cap_buf) - 1, remotePeerCapabilities, rLength);
	   sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ourPref %s\nourCap: %s\nremoteCap: %s\n", pref_buf, cap_buf, remote_cap_buf);
	 */

	/** check if we have a preference codec list */
	if (pLength == 0 || ourPreferences[0] == SKINNY_CODEC_NONE) {
		/* using remote capabilities to */
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "We got an empty preference codec list (exiting)\n");
		return SKINNY_CODEC_NONE;

	}

	/* iterate over our codec preferences */
	for (p = 0; p < pLength; p++) {
		if (ourPreferences[p] == SKINNY_CODEC_NONE) {
			sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "no more preferences at position %d\n", p);
			break;
		}
		/* no more preferences */
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "preference: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));

		/* check if we are capable to handle this codec */
		for (c = 0; c < cLength; c++) {
			if (ourCapabilities[c] == SKINNY_CODEC_NONE) {
				/* we reached the end of valide codecs, because we found the first NONE codec */
				sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) ("Breaking at capability: %d\n", c);
				break;
			}

			sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]));

			/* we have no capabilities from the remote party, use the best codec from ourPreferences */
			if (ourPreferences[p] == ourCapabilities[c]) {
				if (firstJointCapability == SKINNY_CODEC_NONE) {
					firstJointCapability = ourPreferences[p];
					sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "found first firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
				}

				if (rLength == 0 || remotePeerCapabilities[0] == SKINNY_CODEC_NONE) {
					sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "Empty remote Capabilities, using bestCodec from firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
					return firstJointCapability;
				} else {
					/* using capabilities from remote party, that matches our preferences & capabilities */
					for (r = 0; r < rLength; r++) {
						if (remotePeerCapabilities[r] == SKINNY_CODEC_NONE) {
							sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) ("Breaking at remotePeerCapability: %d\n", c);
							break;
						}
						sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s), remoteCapability: " UI64FMT "(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]), (ULONG) remotePeerCapabilities[r], codec2name(remotePeerCapabilities[r]));
						if (ourPreferences[p] == remotePeerCapabilities[r]) {
							sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "found bestCodec as joint capability with remote peer %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));
							return ourPreferences[p];
						}
					}
				}
			}
		}
	}

	if (firstJointCapability != SKINNY_CODEC_NONE) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "did not find joint capability with remote device, using first joint capability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
		return firstJointCapability;
	}

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "no joint capability with preference codec list\n");
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

/* Helper functions for ipv6 / apply_ha / append_ha */
/*!
 * \brief
 * Isolate a 32-bit section of an IPv6 address
 *
 * An IPv6 address can be divided into 4 32-bit chunks. This gives
 * easy access to one of these chunks.
 *
 * \param sin6 A pointer to a struct sockaddr_in6
 * \param index Which 32-bit chunk to operate on. Must be in the range 0-3.
 */
#define V6_WORD(sin6, index) ((uint32_t *)&((sin6)->sin6_addr))[(index)]

/*!
 * \brief
 * Apply a netmask to an address and store the result in a separate structure.
 *
 * When dealing with IPv6 addresses, one cannot apply a netmask with a simple
 * logical and operation. Furthermore, the incoming address may be an IPv4 address
 * and need to be mapped properly before attempting to apply a rule.
 *
 * \param netaddr The IP address to apply the mask to.
 * \param netmask The netmask configured in the host access rule.
 * \param result [out] The resultant address after applying the netmask to the given address
 *
 * \retval 0 Successfully applied netmask
 * \retval -1 Failed to apply netmask
 */
static int apply_netmask(const struct sockaddr_storage *netaddr, const struct sockaddr_storage *netmask, struct sockaddr_storage *result)
{
	int res = 0;

	char *straddr = ast_strdupa(sccp_socket_stringify_addr(netaddr));
	char *strmask = ast_strdupa(sccp_socket_stringify_addr(netmask));

	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (apply_netmask) applying netmask to %s/%s\n", straddr, strmask);

	if (netaddr->ss_family == AF_INET) {
		struct sockaddr_in result4 = { 0, };
		struct sockaddr_in *addr4 = (struct sockaddr_in *) netaddr;
		struct sockaddr_in *mask4 = (struct sockaddr_in *) netmask;

		result4.sin_family = AF_INET;
		result4.sin_addr.s_addr = addr4->sin_addr.s_addr & mask4->sin_addr.s_addr;
		memcpy(result, &result4, sizeof(result4));
	} else if (netaddr->ss_family == AF_INET6) {
		struct sockaddr_in6 result6 = { 0, };
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) netaddr;
		struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *) netmask;
		int i;

		result6.sin6_family = AF_INET6;
		for (i = 0; i < 4; ++i) {
			V6_WORD(&result6, i) = V6_WORD(addr6, i) & V6_WORD(mask6, i);
		}
		memcpy(result, &result6, sizeof(result6));
	} else {
		pbx_log(LOG_NOTICE, "SCCP: (apply_netmask) Unsupported address scheme\n");
		/* Unsupported address scheme */
		res = -1;
	}
	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (apply_netmask) result applied netmask %s\n", sccp_socket_stringify_addr(result));

	return res;
}

/*!
 * \brief Apply a set of rules to a given IP address
 *
 * \param ha The head of the list of host access rules to follow
 * \param addr A sockaddr_in whose address is considered when matching rules
 * \retval AST_SENSE_ALLOW The IP address passes our ACL
 * \retval AST_SENSE_DENY The IP address fails our ACL  
 */
int sccp_apply_ha(const struct sccp_ha *ha, const struct sockaddr_storage *addr)
{
	return sccp_apply_ha_default(ha, addr, AST_SENSE_ALLOW);
}

/*!
 * \brief Apply a set of rules to a given IP address
 *
 * \details
 * The list of host access rules is traversed, beginning with the
 * input rule. If the IP address given matches a rule, the "sense"
 * of that rule is used as the return value. Note that if an IP
 * address matches multiple rules that the last one matched will be
 * the one whose sense will be returned.
 *
 * \param ha The head of the list of host access rules to follow
 * \param addr An sockaddr_storage whose address is considered when matching rules
 * \param defaultValue int value
 * \retval AST_SENSE_ALLOW The IP address passes our ACL
 * \retval AST_SENSE_DENY The IP address fails our ACL
 */
int sccp_apply_ha_default(const struct sccp_ha *ha, const struct sockaddr_storage *addr, int defaultValue)
{
	/* Start optimistic */
	int res = defaultValue;
	const struct sccp_ha *current_ha;

	for (current_ha = ha; current_ha; current_ha = current_ha->next) {

		struct sockaddr_storage result;
		struct sockaddr_storage mapped_addr;
		const struct sockaddr_storage *addr_to_use;

		if (sccp_socket_is_IPv4(&ha->netaddr)) {
			if (sccp_socket_is_IPv6(addr)) {
				if (sccp_socket_is_mapped_IPv4(addr)) {
					if (!sccp_socket_ipv4_mapped(addr, &mapped_addr)) {
						pbx_log(LOG_ERROR, "%s provided to ast_sockaddr_ipv4_mapped could not be converted. That shouldn't be possible\n", sccp_socket_stringify_addr(addr));
						continue;
					}
					addr_to_use = &mapped_addr;
				} else {
					/* An IPv4 ACL does not apply to an IPv6 address */
					continue;
				}
			} else {
				/* Address is IPv4 and ACL is IPv4. No biggie */
				addr_to_use = addr;
			}
		} else {
			if (sccp_socket_is_IPv6(addr) && !sccp_socket_is_mapped_IPv4(addr)) {
				addr_to_use = addr;
			} else {
				/* Address is IPv4 or IPv4 mapped but ACL is IPv6. Skip */
				continue;
			}
		}
		// char *straddr = ast_strdupa(sccp_socket_stringify_addr(&current_ha->netaddr));
		// char *strmask = ast_strdupa(sccp_socket_stringify_addr(&current_ha->netmask));
		// sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s:%s/%s\n", AST_SENSE_DENY == current_ha->sense ? "deny" : "permit", straddr, strmask);

		/* For each rule, if this address and the netmask = the net address
		   apply the current rule */
		if (apply_netmask(addr_to_use, &current_ha->netmask, &result)) {
			/* Unlikely to happen since we know the address to be IPv4 or IPv6 */
			continue;
		}
		if (sccp_socket_cmp_addr(&result, &current_ha->netaddr) == 0) {
			//sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "SCCP: apply_ha_default: result: %s\n", sccp_socket_stringify_addr(&result));
			//sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "SCCP: apply_ha_default: current_ha->netaddr: %s\n", sccp_socket_stringify_addr(&current_ha->netaddr));
			res = current_ha->sense;
		}
	}
	return res;
}

/*!
 * \brief
 * Parse an IPv4 or IPv6 address string.
 *
 * \details
 * Parses a string containing an IPv4 or IPv6 address followed by an optional
 * port (separated by a colon) into a struct ast_sockaddr. The allowed formats
 * are the following:
 *
 * a.b.c.d
 * a.b.c.d:port
 * a:b:c:...:d 
 * [a:b:c:...:d]
 * [a:b:c:...:d]:port
 *
 * Host names are NOT allowed.
 *
 * \param[out] addr The resulting ast_sockaddr. This MAY be NULL from 
 * functions that are performing validity checks only, e.g. ast_parse_arg().
 * \param str The string to parse
 * \param flags If set to zero, a port MAY be present. If set to
 * PARSE_PORT_IGNORE, a port MAY be present but will be ignored. If set to
 * PARSE_PORT_REQUIRE, a port MUST be present. If set to PARSE_PORT_FORBID, a
 * port MUST NOT be present.
 *
 * \retval 1 Success
 * \retval 0 Failure
 */
int sccp_sockaddr_storage_parse(struct sockaddr_storage *addr, const char *str, int flags)
{
	struct addrinfo hints;
	struct addrinfo *res;
	char *s;
	char *host;
	char *port;
	int e;

	s = sccp_strdupa(str);
	if (!sccp_socket_split_hostport(s, &host, &port, flags)) {
		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	/* Hint to get only one entry from getaddrinfo */
	hints.ai_socktype = SOCK_DGRAM;

#ifdef AI_NUMERICSERV
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
#else
	hints.ai_flags = AI_NUMERICHOST;
#endif
	if ((e = getaddrinfo(host, port, &hints, &res))) {
		if (e != EAI_NONAME) {										/* if this was just a host name rather than a ip address, don't print error */
			pbx_log(LOG_ERROR, "getaddrinfo(\"%s\", \"%s\", ...): %s\n", host, S_OR(port, "(null)"), gai_strerror(e));
		}
		return 0;
	}

	/*
	 * I don't see how this could be possible since we're not resolving host
	 * names. But let's be careful...
	 */
	if (res->ai_next != NULL) {
		pbx_log(LOG_WARNING, "getaddrinfo() returned multiple " "addresses. Ignoring all but the first.\n");
	}

	if (addr) {
		memcpy(addr, res->ai_addr, (res->ai_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (sccp_sockaddr_storage_parse) addr:%s\n", sccp_socket_stringify_addr(addr));
	}

	freeaddrinfo(res);
	return 1;
}

/*!
 * \brief
 * Parse a netmask in CIDR notation
 *
 * \details
 * For a mask of an IPv4 address, this should be a number between 0 and 32. For
 * a mask of an IPv6 address, this should be a number between 0 and 128. This
 * function creates an IPv6 sockaddr_storage from the given netmask. For masks of
 * IPv4 addresses, this is accomplished by adding 96 to the original netmask.   
 *
 * \param[out] addr The sockaddr_stroage produced from the CIDR netmask
 * \param is_v4 Tells if the address we are masking is IPv4.
 * \param mask_str The CIDR mask to convert
 * \retval -1 Failure
 * \retval 0 Success
 */
static int parse_cidr_mask(struct sockaddr_storage *addr, int is_v4, const char *mask_str)
{
	int mask;

	if (sscanf(mask_str, "%30d", &mask) != 1) {
		return -1;
	}
	if (is_v4) {
		struct sockaddr_in sin = { 0, };
		if (mask < 0 || mask > 32) {
			return -1;
		}
		sin.sin_family = AF_INET;
		/* If mask is 0, then we already have the
		 * appropriate all 0s address in sin from
		 * the above memset.
		 */
		if (mask != 0) {
			sin.sin_addr.s_addr = htonl(0xFFFFFFFF << (32 - mask));
		}
		memcpy(addr, &sin, sizeof(sin));
	} else {
		struct sockaddr_in6 sin6 = { 0, };
		int i;

		if (mask < 0 || mask > 128) {
			return -1;
		}
		sin6.sin6_family = AF_INET6;
		for (i = 0; i < 4; ++i) {
			/* Once mask reaches 0, we don't have
			 * to explicitly set anything anymore
			 * since sin6 was zeroed out already
			 */
			if (mask > 0) {
				V6_WORD(&sin6, i) = htonl(0xFFFFFFFF << (mask < 32 ? (32 - mask) : 0));
				mask -= mask < 32 ? mask : 32;
			}
		}
		memcpy(addr, &sin6, sizeof(sin6));
	}
	return 0;
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
	struct sccp_ha *prev = NULL;
	struct sccp_ha *ret;
	char *tmp = ast_strdupa(stuff);
	char *address = NULL, *mask = NULL;
	int addr_is_v4;

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

	address = strsep(&tmp, "/");
	if (!address) {
		address = tmp;
	} else {
		mask = tmp;
	}
	if (!sccp_sockaddr_storage_parse(&ha->netaddr, address, PARSE_PORT_FORBID)) {
		pbx_log(LOG_WARNING, "Invalid IP address: %s\n", address);
		sccp_free_ha(ha);
		if (error) {
			*error = 1;
		}
		return ret;
	}
	/*
	   sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_2 "SCCP: (sccp_append_ha) netaddr:%s\n", sccp_socket_stringify_addr(&ha->netaddr));
	 */
	/* If someone specifies an IPv4-mapped IPv6 address,
	 * we just convert this to an IPv4 ACL
	 */
	if (sccp_socket_ipv4_mapped(&ha->netaddr, &ha->netaddr)) {
		pbx_log(LOG_NOTICE, "IPv4-mapped ACL network address specified. " "Converting to an IPv4 ACL network address.\n");
	}

	addr_is_v4 = sccp_socket_is_IPv4(&ha->netaddr);

	if (!mask) {
		parse_cidr_mask(&ha->netmask, addr_is_v4, addr_is_v4 ? "32" : "128");
	} else if (strchr(mask, ':') || strchr(mask, '.')) {
		int mask_is_v4;

		/* Mask is of x.x.x.x or x:x:x:x:x:x:x:x variety */
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (sccp_append_ha) mask:%s\n", mask);
		if (!sccp_sockaddr_storage_parse(&ha->netmask, mask, PARSE_PORT_FORBID)) {
			pbx_log(LOG_WARNING, "Invalid netmask: %s\n", mask);
			sccp_free_ha(ha);
			if (error) {
				*error = 1;
			}
			return ret;
		}
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (sccp_append_ha) strmask:%s, netmask:%s\n", mask, sccp_socket_stringify_addr(&ha->netmask));
		/* If someone specifies an IPv4-mapped IPv6 netmask,
		 * we just convert this to an IPv4 ACL
		 */
		if (sccp_socket_ipv4_mapped(&ha->netmask, &ha->netmask)) {
			ast_log(LOG_NOTICE, "IPv4-mapped ACL netmask specified. " "Converting to an IPv4 ACL netmask.\n");
		}
		mask_is_v4 = sccp_socket_is_IPv4(&ha->netmask);
		if (addr_is_v4 ^ mask_is_v4) {
			pbx_log(LOG_WARNING, "Address and mask are not using same address scheme (%d / %d)\n", addr_is_v4, mask_is_v4);
			sccp_free_ha(ha);
			if (error) {
				*error = 1;
			}
			return ret;
		}
	} else if (parse_cidr_mask(&ha->netmask, addr_is_v4, mask)) {
		pbx_log(LOG_WARNING, "Invalid CIDR netmask: %s\n", mask);
		sccp_free_ha(ha);
		if (error) {
			*error = 1;
		}
		return ret;
	}
	if (apply_netmask(&ha->netaddr, &ha->netmask, &ha->netaddr)) {
		/* This shouldn't happen because ast_sockaddr_parse would
		 * have failed much earlier on an unsupported address scheme
		 */
		char *failaddr = ast_strdupa(sccp_socket_stringify_addr(&ha->netaddr));
		char *failmask = ast_strdupa(sccp_socket_stringify_addr(&ha->netmask));

		pbx_log(LOG_WARNING, "Unable to apply netmask %s to address %s\n", failaddr, failmask);
		sccp_free_ha(ha);
		if (error) {
			*error = 1;
		}
		return ret;
	}

	ha->sense = strncasecmp(sense, "p", 1) ? AST_SENSE_DENY : AST_SENSE_ALLOW;

	ha->next = NULL;
	if (prev) {
		prev->next = ha;
	} else {
		ret = ha;
	}

	{
		char *straddr = ast_strdupa(sccp_socket_stringify_addr(&ha->netaddr));
		char *strmask = ast_strdupa(sccp_socket_stringify_addr(&ha->netmask));

		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "%s/%s sense %d appended to acl for peer\n", straddr, strmask, ha->sense);
	}

	return ret;
}

void sccp_print_ha(struct ast_str *buf, int buflen, struct sccp_ha *path)
{
	while (path) {
		char *straddr = ast_strdupa(sccp_socket_stringify_addr(&path->netaddr));
		char *strmask = ast_strdupa(sccp_socket_stringify_addr(&path->netmask));

		pbx_str_append(&buf, buflen, "%s:%s/%s,", AST_SENSE_DENY == path->sense ? "deny" : "permit", straddr, strmask);
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
	if (c && c->designator) {
		return (const char *) c->designator;
	} else {
		return "";
	}
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

	if (!group) {
		return;
	}
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

#if 0
/*!
 * \brief Compare two socket addressed with each other
 *
 * \note not used
 */
int sockaddr_cmp_addr(struct sockaddr_storage *addr1, socklen_t len1, struct sockaddr_storage *addr2, socklen_t len2)
{
	struct sockaddr_in *p1_in = (struct sockaddr_in *) addr1;
	struct sockaddr_in *p2_in = (struct sockaddr_in *) addr2;
	struct sockaddr_in6 *p1_in6 = (struct sockaddr_in6 *) addr1;
	struct sockaddr_in6 *p2_in6 = (struct sockaddr_in6 *) addr2;

	if (len1 < len2) {
		return -1;
	}
	if (len1 > len2) {
		return 1;
	}
	if (p1_in->sin_family < p2_in->sin_family) {
		return -1;
	}
	if (p1_in->sin_family > p2_in->sin_family) {
		return 1;
	}
	/* compare ip4 */
	if (p1_in->sin_family == AF_INET) {
		return memcmp(&p1_in->sin_addr, &p2_in->sin_addr, sizeof(p1_in->sin_addr));
	} else if (p1_in6->sin6_family == AF_INET6) {
		return memcmp(&p1_in6->sin6_addr, &p2_in6->sin6_addr, sizeof(p1_in6->sin6_addr));
	} else {
		/* unknown type, compare for sanity. */
		return memcmp(addr1, addr2, len1);
	}
}
#endif

int sccp_strversioncmp(const char *s1, const char *s2)
{
	static const char *digits = "0123456789";
	int ret, lz1, lz2;
	size_t p1, p2;

	p1 = strcspn(s1, digits);
	p2 = strcspn(s2, digits);
	while (p1 == p2 && s1[p1] != '\0' && s2[p2] != '\0') {
		/* Different prefix */
		if ((ret = strncmp(s1, s2, p1)) != 0) {
			return ret;
		}
		s1 += p1;
		s2 += p2;

		lz1 = lz2 = 0;
		if (*s1 == '0') {
			lz1 = 1;
		}
		if (*s2 == '0') {
			lz2 = 1;
		}
		if (lz1 > lz2) {
			return -1;
		} else if (lz1 < lz2) {
			return 1;
		} else if (lz1 == 1) {
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
			if (p1 == 0 && p2 > 0) {
				return 1;
			} else if (p2 == 0 && p1 > 0) {
				return -1;
			}
			/* Prefixes are not same */
			if (*s1 != *s2 && *s1 != '0' && *s2 != '0') {
				if (p1 < p2) {
					return 1;
				} else if (p1 > p2) {
					return -1;
				}
			} else {
				if (p1 < p2) {
					ret = strncmp(s1, s2, p1);
				} else if (p1 > p2) {
					ret = strncmp(s1, s2, p2);
				}
				if (ret != 0) {
					return ret;
				}
			}
		}

		p1 = strspn(s1, digits);
		p2 = strspn(s2, digits);

		if (p1 < p2) {
			return -1;
		} else if (p1 > p2) {
			return 1;
		} else if ((ret = strncmp(s1, s2, p1)) != 0) {
			return ret;
		}
		/* Numbers are equal or not present, try with next ones. */
		s1 += p1;
		s2 += p2;
		p1 = strcspn(s1, digits);
		p2 = strcspn(s2, digits);
	}

	return strcmp(s1, s2);
}

char *sccp_dec2binstr(char *buf, size_t size, int value)
{
	char b[33] = { 0 };
	int pos;
	long long z;

	for (z = 1LL << 31, pos = 0; z > 0; z >>= 1, pos++) {
		b[pos] = (((value & z) == z) ? '1' : '0');
	}
	snprintf(buf, size, "%s", b);
	return buf;
}

gcc_inline void sccp_copy_string(char *dst, const char *src, size_t size)
{
#ifdef CS_EXPERIMENTAL
	/*alternative implementation */
	/*
	if (size > 1) {
		*((char *)mempcpy(dst, src, size-1)) = '\0';
	} else {
		*dst = '\0';
	}
	*/
	if (size != 0) {
		while (--size != 0) {
			if ((*dst++ = *src++) == '\0') {
				break;
			}
		}
	}
	// always end string with \0
	*dst = '\0';

#else
	if (size != 0) {
		ast_copy_string(dst, src, size);
	}
#endif
}

/* 
 * \brief trim white space from beginning and ending of string
 * \note This function returns a pointer to a substring of the original string.
 * If the given string was allocated dynamically, the caller must not overwrite
 * that pointer with the returned value, since the original pointer must be
 * deallocated using the same allocator with which it was allocated.  The return
 * value must NOT be deallocated using free() etc.
 */
char *sccp_trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace(*str)) {
		str++;
	}
	if (*str == 0) {											// All spaces
		return str;
	}
	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace(*end)) {
		end--;
	}
	// Write new null terminator
	*(end + 1) = 0;
	return str;
}

#if HAVE_ICONV_H
gcc_inline boolean_t sccp_utils_convUtf8toLatin1(const char *utf8str, char *buf, size_t len) {
	iconv_t cd;
	size_t incount, outcount = len;
	incount = strlen(utf8str);
	if (incount) {
		cd = iconv_open("ISO8859-1", "UTF-8");
		if (cd == (iconv_t) -1) {
			pbx_log(LOG_ERROR, "conversion from 'UTF-8' to 'ISO8859-1' not available.\n");
			return FALSE;
		}
		if (iconv(cd, (void *) &utf8str, &incount, &buf, &outcount) == (size_t) -1) {
			if (errno == E2BIG)
				pbx_log(LOG_WARNING, "SCCP: Iconv: output buffer too small.\n");
			else if (errno == EILSEQ)
				pbx_log(LOG_WARNING,  "SCCP: Iconv: illegal character.\n");
			else if (errno == EINVAL)
				pbx_log(LOG_WARNING,  "SCCP: Iconv: incomplete character sequence.\n");
			else
				pbx_log(LOG_WARNING,  "SCCP: Iconv: error %d: %s.\n", errno, strerror(errno));
		}
		iconv_close(cd);
	}
	return TRUE;
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
