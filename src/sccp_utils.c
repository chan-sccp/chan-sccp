/*!
 * \file	sccp_utils.c
 * \brief       SCCP Utils Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *		The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *		Modified by Jan Czmok and Julien Goodwin
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_session.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

#if defined(DEBUG) && defined(HAVE_EXECINFO_H)
#  include <execinfo.h>
#  if defined(HAVE_DLADDR_H) && defined(HAVE_BFD_H)
#    include <dlfcn.h>
#    include <bfd.h>
#  endif
#  if ASTERISK_VERSION_GROUP >= 112 
#    include <asterisk/backtrace.h>
#  endif
#endif
#include <asterisk/ast_version.h>		// ast_get_version
#ifdef HAVE_PBX_ACL_H				// ast_ha, AST_SENSE_ALLOW
#  include <asterisk/acl.h>
#endif

#if HAVE_ICONV
#include <iconv.h>
#endif

/*!
 * \brief Print out a messagebuffer
 * \param messagebuffer Pointer to Message Buffer as char
 * \param len Lenght as Int
 */
void sccp_dump_packet(unsigned char *messagebuffer, int len)
{
	static const int numcolumns = 16;									// number output columns

	if (len <= 0 || !messagebuffer || !sccp_strlen((const char *) messagebuffer)) {				// safe quard
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
	pbx_str_t *output_buf = pbx_str_create(DEFAULT_PBX_STR_BUFFERSIZE);

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
		pbx_str_append(&output_buf, 0, VERBOSE_PREFIX_1 "%08X - %-*.*s - %s\n", cur, hexcolumnlength, hexcolumnlength, hexout, chrout);
		cur += col;
	} while (cur < (len - 1));
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: packet hex dump:\n%s", pbx_str_buffer(output_buf));
	sccp_free(output_buf)
}

void sccp_dump_msg(const sccp_msg_t * const msg)
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
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found (%d) taps on device addon (%d)\n", (d ? d->id : "SCCP"), taps, cur->type);
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

#if UNUSEDCODE // 2015-11-01
/*!
 * \brief Clean Asterisk Database Entries in the "SCCP" Family
 * 
 */
void sccp_dev_dbclean(void)
{
	struct ast_db_entry *entry = NULL;
	sccp_device_t *d = NULL;
	char key[SCCP_MAX_EXTENSION];
	char scanfmt[20]="";

	//! \todo write an pbx implementation for that
	//entry = PBX(feature_getFromDatabase)tree("SCCP", NULL);
	while (entry) {
		snprintf(scanfmt, sizeof(scanfmt), "/SCCP/%%%ds", SCCP_MAX_EXTENSION);
		sscanf(entry->key, scanfmt, key);
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Looking for '%s' in the devices list\n", key);
		if ((strlen(key) == 15) && (!strncmp(key, "SEP", 3) || !strncmp(key, "ATA", 3) || !strncmp(key, "VGC", 3) || !strncmp(key, "AN", 2) || !strncmp(key, "SKIGW", 5))) {

			SCCP_RWLIST_RDLOCK(&GLOB(devices));
			SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
				if (!strcasecmp(d->id, key)) {
					break;
				}
			}
			SCCP_RWLIST_UNLOCK(&GLOB(devices));

			if (!d) {
				iPbx.feature_removeFromDatabase("SCCP", key);
				sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: device '%s' removed from asterisk database\n", entry->key);
			}

		}
		entry = entry->next;
	}
	if (entry) {
		pbx_db_freetree(entry);
	}
}
#endif

gcc_inline const char *pbxsccp_devicestate2str(uint32_t value)
{														/* pbx_impl/ast/ast.h */
	_ARR2STR(sccp_pbx_devicestates, devicestate, value, text);
}

#if UNUSEDCODE // 2015-11-01
gcc_inline const char *extensionstatus2str(uint32_t value)
{														/* pbx_impl/ast/ast.h */
	_ARR2STR(sccp_extension_states, extension_state, value, text);
}
#endif

gcc_inline const char *label2str(uint16_t value)
{														/* sccp_labels.h */
	_ARR2STR(skinny_labels, label, value, text);
}

#if UNUSEDCODE // 2015-11-01
gcc_inline uint32_t debugcat2int(const char *str)
{														/* chan_sccp.h */
	_STRARR2INT(sccp_debug_categories, key, str, category);
}
#endif

gcc_inline uint32_t labelstr2int(const char *str)
{														/* chan_sccp.h */
	_STRARR2INT(skinny_labels, text, str, label);
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

#if 0 /* unused */
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
#endif

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

	sccp_log((DEBUGCAT_EVENT + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: StorageBackend got Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), sccp_feature_type2str(event->event.featureChanged.featureType), event->event.featureChanged.featureType);
	snprintf(family, sizeof(family), "SCCP/%s", device->id);

	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_CFWDNONE:
		case SCCP_FEATURE_CFWDBUSY:
		case SCCP_FEATURE_CFWDALL:
			if ((linedevice = event->event.featureChanged.optional_linedevice)) {
				sccp_line_t *line = linedevice->line;
				uint8_t instance = linedevice->lineInstance;

				sccp_dev_forward_status(line, instance, device);
				snprintf(cfwdDeviceLineStore, sizeof(cfwdDeviceLineStore), "SCCP/%s/%s", device->id, line->name);
				snprintf(cfwdLineDeviceStore, sizeof(cfwdLineDeviceStore), "SCCP/%s/%s", line->name, device->id);
				switch (event->event.featureChanged.featureType) {
					case SCCP_FEATURE_CFWDALL:
						if (linedevice->cfwdAll.enabled) {
							iPbx.feature_addToDatabase(cfwdDeviceLineStore, "cfwdAll", linedevice->cfwdAll.number);
							iPbx.feature_addToDatabase(cfwdLineDeviceStore, "cfwdAll", linedevice->cfwdAll.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						} else {
							iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, "cfwdAll");
							iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, "cfwdAll");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDBUSY:
						if (linedevice->cfwdBusy.enabled) {
							iPbx.feature_addToDatabase(cfwdDeviceLineStore, "cfwdBusy", linedevice->cfwdBusy.number);
							iPbx.feature_addToDatabase(cfwdLineDeviceStore, "cfwdBusy", linedevice->cfwdBusy.number);
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db put %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						} else {
							iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, "cfwdBusy");
							iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, "cfwdBusy");
							sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: db clear %s\n", DEV_ID_LOG(device), cfwdDeviceLineStore);
						}
						break;
					case SCCP_FEATURE_CFWDNONE:
						iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, "cfwdAll");
						iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, "cfwdBusy");
						iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, "cfwdAll");
						iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, "cfwdBusy");
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
					iPbx.feature_removeFromDatabase(family, "dnd");
				} else {
					if (device->dndFeature.status == SCCP_DNDMODE_SILENT) {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to silent\n", DEV_ID_LOG(device));
						iPbx.feature_addToDatabase(family, "dnd", "silent");
					} else {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: change dnd to reject\n", DEV_ID_LOG(device));
						iPbx.feature_addToDatabase(family, "dnd", "reject");
					}
				}
				device->dndFeature.previousStatus = device->dndFeature.status;
			}
			break;
		case SCCP_FEATURE_PRIVACY:
			if (device->privacyFeature.previousStatus != device->privacyFeature.status) {
				if (!device->privacyFeature.status) {
					iPbx.feature_removeFromDatabase(family, "privacy");
				} else {
					char data[256];

					snprintf(data, sizeof(data), "%d", device->privacyFeature.status);
					iPbx.feature_addToDatabase(family, "privacy", data);
				}
				device->privacyFeature.previousStatus = device->privacyFeature.status;
			}
			break;
		case SCCP_FEATURE_MONITOR:
			if (device->monitorFeature.previousStatus != device->monitorFeature.status) {
				if (device->monitorFeature.status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) {
					iPbx.feature_addToDatabase(family, "monitor", "on");
				} else {
					iPbx.feature_removeFromDatabase(family, "monitor");
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
 * \param subscriptionId SubscriptionId as sccp_subscription_id_t (ByRef) [out]
 * \param extension char array [SCCP_MAX_EXTENSION] [out]
 * \return int containing number of matched subcription elements
 *
 * \callgraph
 * \callergraph
 */
int sccp_parseComposedId(const char *labelString, unsigned int maxLength, sccp_subscription_id_t *subscriptionId, char extension[SCCP_MAX_EXTENSION])
{
	pbx_assert(NULL != labelString && NULL != subscriptionId && NULL != extension);
	int res = 0;
	const char *stringIterator = 0;
	uint32_t i = 0;
	boolean_t endDetected = FALSE;
	enum {EXTENSION, ID, CIDNAME, LABEL, AUX} state = EXTENSION;
	memset(subscriptionId, 0, sizeof(sccp_subscription_id_t));

	for (stringIterator = labelString; stringIterator < labelString + maxLength && !endDetected; stringIterator++) {
		switch (state) {
			case EXTENSION:										// parsing of main id
				pbx_assert(i < SCCP_MAX_EXTENSION);
				switch (*stringIterator) {
					case '\0':
						endDetected = TRUE;
						extension[i] = '\0';
						res++;
						break;
					case '@':
						extension[i] = '\0';
						i = 0;
						state = ID;
						res++;
						break;
					case '!':
						extension[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						extension[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case ID:										// parsing of sub id number
				pbx_assert(i < sizeof(subscriptionId->number));
				switch (*stringIterator) {
					case '\0':
						subscriptionId->number[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '+':
						if(i == 0) {
							subscriptionId->replaceCid = 0;
						}
						break;
					case '=':
						if(i == 0) {
							subscriptionId->replaceCid = 1;
						}
						break;
					case ':':
						subscriptionId->number[i] = '\0';
						i = 0;
						state = CIDNAME;
						res++;
						break;
					case '#':
						subscriptionId->name[i] = '\0';
						i = 0;
						state = LABEL;
						res++;
						break;
					case '!':
						subscriptionId->number[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						subscriptionId->number[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case CIDNAME:										// parsing of sub id name
				pbx_assert(i < sizeof(subscriptionId->name));
				switch (*stringIterator) {
					case '\0':
						subscriptionId->name[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '#':
						subscriptionId->name[i] = '\0';
						i = 0;
						state = LABEL;
						res++;
						break;
					case '!':
						subscriptionId->name[i] = '\0';
						i = 0;
						state = AUX ;
						res++;
						break;
					default:
						subscriptionId->name[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case LABEL:										// parsing of sub id name
				pbx_assert(i < sizeof(subscriptionId->label));
				switch (*stringIterator) {
					case '\0':
						subscriptionId->label[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '!':
						subscriptionId->label[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						subscriptionId->label[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case AUX:										// parsing of auxiliary parameter
				pbx_assert(i < sizeof(subscriptionId->aux));
				switch (*stringIterator) {
					case '\0':
						subscriptionId->aux[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					default:
						subscriptionId->aux[i] = *stringIterator;
						i++;
						break;
				}
				break;

			default:
				pbx_assert(FALSE);
				res = 0;
				break;
		}
	}
	return res;
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
	if (sccp_strlen(channel->subscriptionId.number) != 0) {
		/* Second condition: SubscriptionId does not match default subscriptionId of line. */
		if (0 != strncasecmp(channel->subscriptionId.number, channel->line->defaultSubscriptionId.number, sccp_strlen(channel->subscriptionId.number))) {
			filterPhones = TRUE;
		}
	}

	if (FALSE == filterPhones) {
		/* Accept phone for calling if all phones shall be called. */
		result = TRUE;
	} else if (0 != sccp_strlen(subscriptionIdNum) &&								/* We already know that we won't search for a trivial subscriptionId. */
		   (0 != strncasecmp(channel->subscriptionId.number, subscriptionIdNum, sccp_strlen(channel->subscriptionId.number)))) {	/* Do the match! */
		result = FALSE;
	}
#if 0
	pbx_log(LOG_NOTICE, "sccp_channel->subscriptionId.number=%s, length=%d\n", channel->subscriptionId.number, sccp_strlen(channel->subscriptionId.number));
	pbx_log(LOG_NOTICE, "subscriptionIdNum=%s, length=%d\n", subscriptionIdNum ? subscriptionIdNum : "NULL", subscriptionIdNum ? sccp_strlen(subscriptionIdNum) : -1);

	pbx_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: sccp_channel->subscriptionId.number=%s, SubscriptionId=%s\n", (channel->subscriptionId.number) ? channel->subscriptionId.number : "NULL", (subscriptionIdNum) ? subscriptionIdNum : "NULL");
	pbx_log(LOG_NOTICE, "sccp_util_matchSubscriptionId: result: %d\n", result);
#endif
	return result;
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
	int dirNumLen = dirNum ? sccp_strlen(dirNum) : 0;
	int fqdnLen = fqdn ? sccp_strlen(fqdn) : 0;
	int lineDisplayNameLen = lineDisplayName ? sccp_strlen(lineDisplayName) : 0;
	int dummyLen = dirNumLen + fqdnLen + lineDisplayNameLen;

 	int pktLen = SCCP_PACKET_HEADER + dummyLen;
	sccp_msg_t *msg = sccp_build_packet(LineStatDynamicMessage, pktLen);
	msg->data.LineStatDynamicMessage.lel_lineNumber = htolel(lineInstance);
	msg->data.LineStatDynamicMessage.lel_lineType = htolel(type);
	if (dummyLen) {
		char *dummyPtr = msg->data.LineStatDynamicMessage.dummy;
		if (dirNumLen) {
			memcpy(dummyPtr, dirNum, dirNumLen);
			dummyPtr += dirNumLen + 1;
		}
		if (fqdnLen) {
			memcpy(dummyPtr, fqdn, fqdnLen);
			dummyPtr += fqdnLen + 1;
		}
		if (lineDisplayNameLen) {
			memcpy(dummyPtr, lineDisplayName, lineDisplayNameLen);
			dummyPtr += lineDisplayNameLen + 1;
		}
	}

	return msg;
}

/*!
 * \brief Compare the information of two socket with one another
 * \param s0 Socket Information
 * \param s1 Socket Information
 * \return success as int
 *
 * \retval FALSE on diff
 * \retval TRUE on equal
 */
gcc_inline boolean_t sccp_netsock_equals(const struct sockaddr_storage * const s0, const struct sockaddr_storage *const s1)
{
	if (s0->ss_family == s1->ss_family && sccp_netsock_cmp_addr(s0, s1) == 0 ) {
		return TRUE;
	} 

	return FALSE;
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
	} if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2) && (sccp_strlen(data1) == sccp_strlen(data2))) {
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
	} if (!sccp_strlen_zero(data1) && !sccp_strlen_zero(data2) && (sccp_strlen(data1) == sccp_strlen(data2))) {
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

	char *straddr = pbx_strdupa(sccp_netsock_stringify_addr(netaddr));
	char *strmask = pbx_strdupa(sccp_netsock_stringify_addr(netmask));

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
	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (apply_netmask) result applied netmask %s\n", sccp_netsock_stringify_addr(result));

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

		if (sccp_netsock_is_IPv4(&ha->netaddr)) {
			if (sccp_netsock_is_IPv6(addr)) {
				if (sccp_netsock_is_mapped_IPv4(addr)) {
					if (!sccp_netsock_ipv4_mapped(addr, &mapped_addr)) {
						pbx_log(LOG_ERROR, "%s provided to ast_sockaddr_ipv4_mapped could not be converted. That shouldn't be possible\n", sccp_netsock_stringify_addr(addr));
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
			if (sccp_netsock_is_IPv6(addr) && !sccp_netsock_is_mapped_IPv4(addr)) {
				addr_to_use = addr;
			} else {
				/* Address is IPv4 or IPv4 mapped but ACL is IPv6. Skip */
				continue;
			}
		}
		// char *straddr = pbx_strdupa(sccp_netsock_stringify_addr(&current_ha->netaddr));
		// char *strmask = pbx_strdupa(sccp_netsock_stringify_addr(&current_ha->netmask));
		// sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s:%s/%s\n", AST_SENSE_DENY == current_ha->sense ? "deny" : "permit", straddr, strmask);

		/* For each rule, if this address and the netmask = the net address
		   apply the current rule */
		if (apply_netmask(addr_to_use, &current_ha->netmask, &result)) {
			/* Unlikely to happen since we know the address to be IPv4 or IPv6 */
			continue;
		}
		if (sccp_netsock_cmp_addr(&result, &current_ha->netaddr) == 0) {
			//sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "SCCP: apply_ha_default: result: %s\n", sccp_netsock_stringify_addr(&result));
			//sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "SCCP: apply_ha_default: current_ha->netaddr: %s\n", sccp_netsock_stringify_addr(&current_ha->netaddr));
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

	s = pbx_strdupa(str);
	if (!sccp_netsock_split_hostport(s, &host, &port, flags)) {
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
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (sccp_sockaddr_storage_parse) addr:%s\n", sccp_netsock_stringify_addr(addr));
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
	char *tmp = pbx_strdupa(stuff);
	char *address = NULL, *mask = NULL;
	int addr_is_v4;

	ret = path;
	while (path) {
		prev = path;
		path = path->next;
	}

	if (!(ha = sccp_calloc(sizeof *ha, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
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
	   sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_2 "SCCP: (sccp_append_ha) netaddr:%s\n", sccp_netsock_stringify_addr(&ha->netaddr));
	 */
	/* If someone specifies an IPv4-mapped IPv6 address,
	 * we just convert this to an IPv4 ACL
	 */
	if (sccp_netsock_ipv4_mapped(&ha->netaddr, &ha->netaddr)) {
		pbx_log(LOG_NOTICE, "IPv4-mapped ACL network address specified. " "Converting to an IPv4 ACL network address.\n");
	}

	addr_is_v4 = sccp_netsock_is_IPv4(&ha->netaddr);

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
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "SCCP: (sccp_append_ha) strmask:%s, netmask:%s\n", mask, sccp_netsock_stringify_addr(&ha->netmask));
		/* If someone specifies an IPv4-mapped IPv6 netmask,
		 * we just convert this to an IPv4 ACL
		 */
		if (sccp_netsock_ipv4_mapped(&ha->netmask, &ha->netmask)) {
			ast_log(LOG_NOTICE, "IPv4-mapped ACL netmask specified. " "Converting to an IPv4 ACL netmask.\n");
		}
		mask_is_v4 = sccp_netsock_is_IPv4(&ha->netmask);
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
		char *failaddr = pbx_strdupa(sccp_netsock_stringify_addr(&ha->netaddr));
		char *failmask = pbx_strdupa(sccp_netsock_stringify_addr(&ha->netmask));

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
		char *straddr = pbx_strdupa(sccp_netsock_stringify_addr(&ha->netaddr));
		char *strmask = pbx_strdupa(sccp_netsock_stringify_addr(&ha->netmask));

		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "%s/%s sense %d appended to acl for peer\n", straddr, strmask, ha->sense);
	}

	return ret;
}

void sccp_print_ha(struct ast_str *buf, int buflen, struct sccp_ha *path)
{
	while (path) {
		char *straddr = pbx_strdupa(sccp_netsock_stringify_addr(&path->netaddr));
		char *strmask = pbx_strdupa(sccp_netsock_stringify_addr(&path->netmask));

		pbx_str_append(&buf, buflen, "%s:%s/%s,", AST_SENSE_DENY == path->sense ? "deny" : "permit", straddr, strmask);
		path = path->next;
	}
}

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(chan_sccp_acl_tests)
{
	struct sccp_ha *ha = NULL;
	struct sockaddr_storage sas10, sas1015, sas172, sas200, sasff, sasffff;
	int error = 0;

	switch (cmd) {
	case TEST_INIT:
		info->name = "permit_deny";
		info->category = "/channels/chan_sccp/acl/";
		info->summary = "chan-sccp-b ha / permit / deny test";
		info->description = "chan-sccp-b ha / permit / deny parsing tests";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	pbx_test_status_update(test, "Executing chan-sccp-b ha path tests...\n");

	pbx_test_status_update(test, "Setting up sockaddr_storage...\n");
	sccp_sockaddr_storage_parse(&sas10, "10.0.0.1", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv4(&sas10));

	sccp_sockaddr_storage_parse(&sas1015, "10.15.15.1", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv4(&sas1015));

	sccp_sockaddr_storage_parse(&sas172, "172.16.0.1", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv4(&sas172));

	sccp_sockaddr_storage_parse(&sas200, "200.200.100.100", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv4(&sas200));

	sccp_sockaddr_storage_parse(&sasff, "fe80::ffff:0:0:0", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv6(&sasff));

	sccp_sockaddr_storage_parse(&sasffff, "fe80::ffff:0:ffff:0", PARSE_PORT_FORBID);
	pbx_test_validate(test, sccp_netsock_is_IPv6(&sasffff));

	// test 1
	pbx_test_status_update(test, "test 1: ha deny all\n");
	ha = sccp_append_ha("deny", "0.0.0.0/0.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);

	// test 2
	pbx_test_status_update(test, "test 2: previous + permit 10.15.15.0/255.255.255.0\n");
	ha = sccp_append_ha("permit", "10.15.15.0/255.255.255.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);

	// test 3
	pbx_test_status_update(test, "test 3: previous + second permit 10.15.15.0/255.255.255.0\n");
	ha = sccp_append_ha("permit", "10.15.15.0/255.255.255.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);
	sccp_free_ha(ha);
	ha = NULL;

	// test 4
	pbx_test_status_update(test, "test 4: deny all + permit 10.0.0.0/255.255.255.0\n");
	ha = sccp_append_ha("deny", "0.0.0.0/0.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	ha = sccp_append_ha("permit", "10.0.0.0/255.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);

	// test 5
	pbx_test_status_update(test, "test 5: previous + 172.16.0.0/255.255.0.0\n");
	ha = sccp_append_ha("permit", "172.16.0.0/255.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);

	// test 6
	pbx_test_status_update(test, "test 6: previous + deny_all at the end\n");
	ha = sccp_append_ha("deny", "0.0.0.0/0.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas1015) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas172) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);
	sccp_free_ha(ha);
	ha = NULL;

	// test 7: ipv6
	pbx_test_status_update(test, "test 7: IPv6: deny 0.0.0.0/0.0.0.0,::,::/0::\n");
	ha = sccp_append_ha("deny", "0.0.0.0/0.0.0.0", ha, &error);
	pbx_test_validate(test, error == 0);
	ha = sccp_append_ha("deny", "::", ha, &error);
	pbx_test_validate(test, error == 0);
	ha = sccp_append_ha("deny", "::/0", ha, &error);
	pbx_test_validate(test, error == 0);
	//pbx_test_status_update(test, "test 7: deny !fe80::/64\n");			/* we cannot parse this format yes (asterisk-13) */
	//ha = sccp_append_ha("deny", "!fe80::/64", ha, &error);
	//pbx_test_validate(test, error == 0);
	pbx_test_status_update(test, "      : previous + permit fe80::ffff:0:0:0/80\n");
	ha = sccp_append_ha("permit", "fe80::ffff:0:0:0/80", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_status_update(test, "      : previous + permit fe80::ffff:0:ffff:0/112\n");
	ha = sccp_append_ha("permit", "fe80::ffff:0:ffff:0/112", ha, &error);
	pbx_test_validate(test, error == 0);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas10) == AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sasff) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sasffff) != AST_SENSE_DENY);
	pbx_test_validate(test, sccp_apply_ha(ha, (struct sockaddr_storage *) &sas200) == AST_SENSE_DENY);
	sccp_free_ha(ha);
	ha = NULL;

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(chan_sccp_acl_invalid_tests)
{
	struct sccp_ha *ha = NULL;
	enum ast_test_result_state res = AST_TEST_PASS;

	switch (cmd) {
	case TEST_INIT:
		info->name = "invalid";
		info->category = "/channels/chan_sccp/acl/";
		info->summary = "Invalid ACL unit test";
		info->description = "Ensures that garbage ACL values are not accepted";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	pbx_test_status_update(test, "Executing invalid acl test tests...\n");

	// test invalid
	const char * invalid_acls[] = {
		/* Negative netmask */
		"1.3.3.7/-1",
		/* Netmask too large */
		"1.3.3.7/33",
		/* Netmask waaaay too large */
		"1.3.3.7/92342348927389492307420",
		/* Netmask non-numeric */
		"1.3.3.7/California",
		/* Too many octets in Netmask */
		"1.3.3.7/255.255.255.255.255",
		/* Octets in IP address exceed 255 */
		"57.60.278.900/31",
		/* Octets in IP address exceed 255 and are negative */
		"400.32.201029.-6/24",
		/* Invalidly formatted IP address */
		"EGGSOFDEATH/4000",
		/* Too many octets in IP address */
		"33.4.7.8.3/300030",
		/* Too many octets in Netmask */
		"1.2.3.4/6.7.8.9.0",
		/* Too many octets in IP address */
		"3.1.4.1.5.9/3",
		/* IPv6 address has multiple double colons */
		"ff::ff::ff/3",
		/* IPv6 address is too long */
		"1234:5678:90ab:cdef:1234:5678:90ab:cdef:1234/56",
		/* IPv6 netmask is too large */
		"::ffff/129",
		/* IPv4-mapped IPv6 address has too few octets */
		"::ffff:255.255.255/128",
		/* Leading and trailing colons for IPv6 address */
		":1234:/15",
		/* IPv6 address and IPv4 netmask */
		"fe80::1234/255.255.255.0",
	};
	uint8_t i;
	for (i = 0; i < ARRAY_LEN(invalid_acls); ++i) {
		int error = 0;
		ha = sccp_append_ha("permit", invalid_acls[i], ha, &error);
		if (ha || !error) {
			pbx_test_status_update(test, "ACL %s accepted even though it is total garbage.\n", invalid_acls[i]);
			if (ha) {
				sccp_free_ha(ha);
			}
			res = AST_TEST_FAIL;
		}
	}
	sccp_free_ha(ha);
	ha = NULL;

	return res;
}

AST_TEST_DEFINE(chan_sccp_reduce_codec_set)
{
	switch (cmd) {
	case TEST_INIT:
		info->name = "reduceCodecSet";
		info->category = "/channels/chan_sccp/codec/";
		info->summary = "reduceCodecSet unit test";
		info->description = "reduceCodecSet";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}
	
	const skinny_codec_t empty[SKINNY_MAX_CAPABILITIES] = {0};
	const skinny_codec_t short1[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONSTANDARD,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ULAW_56K,0};
	const skinny_codec_t short2[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G722_48K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_NONE};
	const skinny_codec_t long1[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G729_A,SKINNY_CODEC_G729,SKINNY_CODEC_G728,SKINNY_CODEC_G723_1,SKINNY_CODEC_G722_48K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_IS11172,SKINNY_CODEC_IS13818,SKINNY_CODEC_G729_B,SKINNY_CODEC_G729_AB,SKINNY_CODEC_GSM_FULLRATE,SKINNY_CODEC_GSM_HALFRATE,SKINNY_CODEC_WIDEBAND_256K};
	
	pbx_test_status_update(test, "Executing reduceCodecSet on two default codecArrays...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = {0};
		sccp_codec_reduceSet(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}
	pbx_test_status_update(test, "Executing reduceCodecSet on one partially filled and one empty codecArray...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_reduceSet(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}
	
	pbx_test_status_update(test, "Executing reduceCodecSet on two partially filled codecArrays...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_NONE,};
		sccp_codec_reduceSet(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	pbx_test_status_update(test, "Executing reduceCodecSet one fully filled and one partially filled codecArrays...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, long1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G722_48K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_NONE,};
		sccp_codec_reduceSet(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	return AST_TEST_PASS;
}

AST_TEST_DEFINE(chan_sccp_combine_codec_sets)
{
	switch (cmd) {
	case TEST_INIT:
		info->name = "combineCodecSets";
		info->category = "/channels/chan_sccp/codec/";
		info->summary = "combineCodecSets unit test";
		info->description = "combineCodecSets";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}
	
	const skinny_codec_t empty[SKINNY_MAX_CAPABILITIES] = {0};
	const skinny_codec_t short1[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONSTANDARD,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ULAW_56K,0};
	const skinny_codec_t short2[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G722_48K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_NONE};
	const skinny_codec_t long1[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_G729_A,SKINNY_CODEC_G729,SKINNY_CODEC_G728,SKINNY_CODEC_G723_1,SKINNY_CODEC_G722_48K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_IS11172,SKINNY_CODEC_IS13818,SKINNY_CODEC_G729_B,SKINNY_CODEC_G729_AB,SKINNY_CODEC_GSM_FULLRATE,SKINNY_CODEC_GSM_HALFRATE,SKINNY_CODEC_WIDEBAND_256K};
	
	pbx_test_status_update(test, "Executing combineCodecSet on two empty codecArrays...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = {0};
		sccp_codec_combineSets(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}

	pbx_test_status_update(test, "Executing combineCodecSet on one partially filled and one empty codecArray...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_combineSets(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(short1[x]));
			pbx_test_validate(test, baseCodecArray[x] == short1[x]);
		}
	}

	pbx_test_status_update(test, "Executing combineCodecSet on two partially filled codecArrays...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONSTANDARD,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G722_48K,SKINNY_CODEC_NONE,};
		sccp_codec_combineSets(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on one fully and one partially filled codecArray...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, long1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_combineSets(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(long1[x]));
			pbx_test_validate(test, baseCodecArray[x] == long1[x]);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on one partially and one fully filled codecArray...\n");
	{
		uint8_t x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONSTANDARD,SKINNY_CODEC_G711_ALAW_64K,SKINNY_CODEC_G711_ALAW_56K,SKINNY_CODEC_G711_ULAW_64K,SKINNY_CODEC_G711_ULAW_56K,SKINNY_CODEC_G729_A,SKINNY_CODEC_G729,SKINNY_CODEC_G728,SKINNY_CODEC_G723_1,SKINNY_CODEC_G722_48K,SKINNY_CODEC_G722_56K,SKINNY_CODEC_G722_64K,SKINNY_CODEC_IS11172,SKINNY_CODEC_IS13818,SKINNY_CODEC_G729_B,SKINNY_CODEC_G729_AB,SKINNY_CODEC_GSM_FULLRATE,SKINNY_CODEC_GSM_HALFRATE};
		sccp_codec_combineSets(baseCodecArray, long1);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			//pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	return AST_TEST_PASS;
}
#endif

/*!
 * \brief Yields string representation from channel (for debug).
 * \param c SCCP channel
 * \return string constant (on the heap!)
 */
const char *sccp_channel_toString(sccp_channel_t * c)
{
	if (c) {
		return (const char *) c->designator;
	} 
		return "";
	
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
		}
		if (lz1 < lz2) {
			return 1;
		}
		if (lz1 == 1) {
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
			} if (p2 == 0 && p1 > 0) {
				return -1;
			}
			/* Prefixes are not same */
			if (*s1 != *s2 && *s1 != '0' && *s2 != '0') {
				if (p1 < p2) {
					return 1;
				}
				if (p1 > p2) {
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
		}
		if (p1 > p2) {
			return 1;
		}
		if ((ret = strncmp(s1, s2, p1)) != 0) {
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
	pbx_assert(NULL != dst && NULL != src);
	if (do_expect(size != 0)) {
		while (do_expect(--size != 0)) {
			if (+(*dst++ = *src++) == '\0') {
				break;
			}
		}
	}
	*dst = '\0';
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
	end = str + sccp_strlen(str) - 1;
	while (end > str && isspace(*end)) {
		end--;
	}
	// Write new null terminator
	*(end + 1) = 0;
	return str;
}

gcc_inline int sccp_atoi(const char * const buf, size_t buflen)
{
	int result = 0;
	if (buf && buflen > 0) {
	        errno = 0;
		char *end = NULL;
        	long temp = strtol(buf, &end, 10);
	        if (end != buf && errno != ERANGE && (temp >= INT_MIN || temp <= INT_MAX)) {
        		result = (int)temp;
		}
	}
	return result;
}

long int sccp_random(void)
{
	/* potentially replace with our own implementation */
	return pbx_random();
}

#if HAVE_ICONV
static iconv_t __sccp_iconv = (iconv_t) -1;
static sccp_mutex_t __iconv_lock;

static void __attribute__((constructor)) __start_iconv(void)
{
	__sccp_iconv = iconv_open("ISO8859-1", "UTF-8");
	if (__sccp_iconv == (iconv_t) -1) {
		pbx_log(LOG_ERROR, "SCCP:conversion from 'UTF-8' to 'ISO8859-1' not available.\n");
	}
	pbx_mutex_init_notracking(&__iconv_lock);
}

static void __attribute__((destructor)) __stop_iconv(void)
{
	if (__sccp_iconv) {
		pbx_mutex_destroy(&__iconv_lock);
		iconv_close(__sccp_iconv);
	}
}

gcc_inline boolean_t sccp_utils_convUtf8toLatin1(ICONV_CONST char *utf8str, char *buf, size_t len) 
{
	if (__sccp_iconv == (iconv_t) -1) {
		// fallback to plain string copy
		sccp_copy_string(buf, utf8str, len);
		return TRUE;
	}
	size_t incount, outcount = len;
	incount = sccp_strlen(utf8str);
	if (incount) {
		pbx_mutex_lock(&__iconv_lock);
		if (iconv(__sccp_iconv, &utf8str, &incount, &buf, &outcount) == (size_t) -1) {
			if (errno == E2BIG) {
				pbx_log(LOG_WARNING, "SCCP: Iconv: output buffer too small.\n");
			} else if (errno == EILSEQ) {
				pbx_log(LOG_WARNING,  "SCCP: Iconv: illegal character.\n");
			} else if (errno == EINVAL) {
				pbx_log(LOG_WARNING,  "SCCP: Iconv: incomplete character sequence.\n");
			} else {
				pbx_log(LOG_WARNING,  "SCCP: Iconv: error %d: %s.\n", errno, strerror(errno));
}
		}
		pbx_mutex_unlock(&__iconv_lock);
	}
	return TRUE;
}
#endif

gcc_inline boolean_t sccp_always_false(void)
{
	return FALSE;
}

gcc_inline boolean_t sccp_always_true(void)
{
	return TRUE;
}


#if CS_TEST_FRAMEWORK
static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(chan_sccp_acl_tests);
	AST_TEST_REGISTER(chan_sccp_acl_invalid_tests);
	AST_TEST_REGISTER(chan_sccp_reduce_codec_set);
	AST_TEST_REGISTER(chan_sccp_combine_codec_sets);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(chan_sccp_acl_tests);
	AST_TEST_UNREGISTER(chan_sccp_acl_invalid_tests);
	AST_TEST_UNREGISTER(chan_sccp_reduce_codec_set);
	AST_TEST_UNREGISTER(chan_sccp_combine_codec_sets);
}
#endif

#ifdef DEBUG
#if ASTERISK_VERSION_GROUP < 112 
#if HAVE_EXECINFO_H
static char **__sccp_bt_get_symbols(void **addresses, size_t num_frames)
{
	char **strings;
#if defined(HAVE_DLADDR_H) && defined(HAVE_BFD_H)
	size_t stackfr;
	bfd *bfdobj;		/* bfd.h */
	Dl_info dli;		/* dlfcn.h */
	long allocsize;
	asymbol **syms = NULL;	/* bfd.h */
	bfd_vma offset;		/* bfd.h */
	const char *lastslash;
	asection *section;
	const char *file, *func;
	unsigned int line;
	char address_str[128];
	char msg[1024];
	size_t strings_size;
	size_t *eachlen;

	strings_size = num_frames * sizeof(*strings);

	eachlen = sccp_calloc(sizeof *eachlen, num_frames);
	strings = sccp_calloc(sizeof *strings, num_frames);
	if (!eachlen || !strings) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		sccp_free(eachlen);
		sccp_free(strings);
		return NULL;
	}

	for (stackfr = 0; stackfr < num_frames; stackfr++) {
		int found = 0, symbolcount;

		msg[0] = '\0';

		if (!dladdr(addresses[stackfr], &dli)) {
			continue;
		}

		if (strcmp(dli.dli_fname, "asterisk") == 0) {
			char asteriskpath[256];

			if (!(dli.dli_fname = ast_utils_which("asterisk", asteriskpath, sizeof(asteriskpath)))) {
				/* This will fail to find symbols */
				dli.dli_fname = "asterisk";
			}
		}

		lastslash = strrchr(dli.dli_fname, '/');
		if ((bfdobj = bfd_openr(dli.dli_fname, NULL)) &&
			bfd_check_format(bfdobj, bfd_object) &&
			(allocsize = bfd_get_symtab_upper_bound(bfdobj)) > 0 &&
			(syms = sccp_malloc(allocsize)) &&
			(symbolcount = bfd_canonicalize_symtab(bfdobj, syms))) {

			if (bfdobj->flags & DYNAMIC) {
				offset = addresses[stackfr] - dli.dli_fbase;
			} else {
				offset = addresses[stackfr] - (void *) 0;
			}

			for (section = bfdobj->sections; section; section = section->next) {
				if (!(bfd_get_section_flags(bfdobj, section) & SEC_ALLOC) ||
					section->vma > offset ||
					section->size + section->vma < offset) {
					continue;
				}

				if (!bfd_find_nearest_line(bfdobj, section, syms, offset - section->vma, &file, &func, &line)) {
					continue;
				}

				/* file can possibly be null even with a success result from bfd_find_nearest_line */
				file = file ? file : "";

				/* Stack trace output */
				found++;
				if ((lastslash = strrchr(file, '/'))) {
					const char *prevslash;

					for (prevslash = lastslash - 1; *prevslash != '/' && prevslash >= file; prevslash--) {
					}
					if (prevslash >= file) {
						lastslash = prevslash;
					}
				}
				if (dli.dli_saddr == NULL) {
					address_str[0] = '\0';
				} else {
					snprintf(address_str, sizeof(address_str), " (%p+%lX)",
						dli.dli_saddr,
						(unsigned long) (addresses[stackfr] - dli.dli_saddr));
				}
				snprintf(msg, sizeof(msg), "%s:%u %s()%s",
					lastslash ? lastslash + 1 : file, line,
					S_OR(func, "???"),
					address_str);

				break; /* out of section iteration */
			}
		}
		if (bfdobj) {
			bfd_close(bfdobj);
		}
		if (syms) {
			sccp_free(syms);
		}

		/* Default output, if we cannot find the information within BFD */
		if (!found) {
			if (dli.dli_saddr == NULL) {
				address_str[0] = '\0';
			} else {
				snprintf(address_str, sizeof(address_str), " (%p+%lX)",
					dli.dli_saddr,
					(unsigned long) (addresses[stackfr] - dli.dli_saddr));
			}
			snprintf(msg, sizeof(msg), "%s %s()%s",
				lastslash ? lastslash + 1 : dli.dli_fname,
				S_OR(dli.dli_sname, "<unknown>"),
				address_str);
		}

		if (!ast_strlen_zero(msg)) {
			char **tmp;

			eachlen[stackfr] = strlen(msg) + 1;
			if (!(tmp = sccp_realloc(strings, strings_size + eachlen[stackfr]))) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				sccp_free(strings);
				strings = NULL;
				break; /* out of stack frame iteration */
			}
			strings = tmp;
			strings[stackfr] = (char *) strings + strings_size;
			//__strcpy(strings[stackfr], msg);/* Safe since we just allocated the room. */
			sccp_copy_string(strings[stackfr], msg, strings_size + eachlen[stackfr]);
			strings_size += eachlen[stackfr];
		}
	}

	if (strings) {
		/* Recalculate the offset pointers because of the reallocs. */
		strings[0] = (char *) strings + num_frames * sizeof(*strings);
		for (stackfr = 1; stackfr < num_frames; stackfr++) {
			strings[stackfr] = strings[stackfr - 1] + eachlen[stackfr - 1];
		}
	}
	sccp_free(eachlen);
#else
	strings = backtrace_symbols(addresses, num_frames);
#endif  // defined(HAVE_DLADDR_H) && defined(HAVE_BFD_H)
	return strings;
}
#endif  // HAVE_EXECINFO_H
#endif	// ASTERISK_VERSION_GROUP

void sccp_do_backtrace()
{
	pbx_rwlock_rdlock(&GLOB(lock));
	boolean_t running = GLOB(module_running);
	pbx_rwlock_unlock(&GLOB(lock));
	if (!running) {
		return;
	}

#if HAVE_EXECINFO_H
	void	*addresses[SCCP_BACKTRACE_SIZE];
	size_t  size, i;
	char     **strings;
	struct ast_str *btbuf;
	if (!(btbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE * 2))) {
		return;
	}
	
	pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "================================================================================\n");
	pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "OPERATING SYSTEM: %s, ARCHITECTURE: %s, KERNEL: %s\nASTERISK: %s\nCHAN-SCCP-b: %s\n", BUILD_OS, BUILD_MACHINE, BUILD_KERNEL, pbx_get_version(), SCCP_VERSIONSTR);
#if !defined(HAVE_DLADDR_H) || !defined(HAVE_BFD_H)
	pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "To get a better backtrace you would need to install libbfd (package binutils devel package)\n");
#endif		
	pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "--------------------------------------------------------------------------(bt)--\n");
	size = backtrace(addresses, SCCP_BACKTRACE_SIZE);
#if ASTERISK_VERSION_GROUP >= 112 
	strings = ast_bt_get_symbols(addresses, size);
#else
	strings = __sccp_bt_get_symbols(addresses, size);
#endif

	if (strings) {
		for (i = 1; i < size; i++) {
			pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, " (bt) > %s\n", strings[i]);		
		}
		sccp_free(strings);	// malloced by backtrace_symbols

		pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "================================================================================\n");
		pbx_log(LOG_WARNING, "SCCP: (backtrace) \n%s\n", pbx_str_buffer(btbuf));
	}
#endif	// HAVE_EXECINFO_H
}
#endif  // DEBUG
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
