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
#include "sccp_linedevice.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

SCCP_FILE_VERSION(__FILE__, "");
#include <locale.h>
#if defined __has_include
#  if __has_include (<xlocale.h>)
#    include <xlocale.h>
#  endif
#elif defined(HAVE_XLOCALE_H)
#  include <xlocale.h>
#endif
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
	char * hexptr = NULL;
	char chrout[numcolumns + 1];
	char * chrptr = NULL;
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
 * \brief Clear all Addons from AddOn Linked List
 * \param d SCCP Device
 */
void sccp_addons_clear(devicePtr d)
{
	sccp_addon_t * addon = NULL;

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
	int argc = 0;
	char * scan = NULL;
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
void sccp_util_featureStorageBackend(const sccp_event_t * const event)
{
	char family[25];
	char cfwdDeviceLineStore[60];										/* backward compatibiliy SCCP/Device/Line */
	char cfwdLineDeviceStore[60];										/* new format cfwd: SCCP/Line/Device */
	sccp_linedevice_t * ld = NULL;
	sccp_device_t * device = NULL;

	if(!event || !(device = event->featureChanged.device)) {
		return;
	}

	sccp_log((DEBUGCAT_EVENT + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: StorageBackend got Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), sccp_feature_type2str(event->featureChanged.featureType), event->featureChanged.featureType);
	snprintf(family, sizeof(family), "SCCP/%s", device->id);

	switch (event->featureChanged.featureType) {
		case SCCP_FEATURE_CFWDNONE:
		case SCCP_FEATURE_CFWDBUSY:
		case SCCP_FEATURE_CFWDALL:
		case SCCP_FEATURE_CFWDNOANSWER:
			if((ld = event->featureChanged.optional_linedevice)) {
				constLinePtr line = ld->line;
				uint8_t instance = ld->lineInstance;
				int res = 0;

				sccp_dev_forward_status(line, instance, device);
				snprintf(cfwdDeviceLineStore, sizeof(cfwdDeviceLineStore), "SCCP/%s/%s", device->id, line->name);
				snprintf(cfwdLineDeviceStore, sizeof(cfwdLineDeviceStore), "SCCP/%s/%s", line->name, device->id);
				if(event->featureChanged.featureType == SCCP_FEATURE_CFWDNONE) {
					for(uint x = SCCP_CFWD_ALL; x < SCCP_CFWD_SENTINEL; x++) {
						char cfwdstr[15] = "";
						snprintf(cfwdstr, 14, "cfwd%s", sccp_cfwd2str((sccp_cfwd_t)x));
						res |= iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, cfwdstr);
						res |= iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, cfwdstr);
					}
					sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: all cfwd cleared from db (res:%d)\n", DEV_ID_LOG(device), res);
				} else {
					sccp_cfwd_t cfwd = sccp_feature2cfwd(event->featureChanged.featureType);
					// const char * cfwdstr = sccp_cfwd2str(cfwd);
					char cfwdstr[15] = "";
					snprintf(cfwdstr, 14, "cfwd%s", sccp_cfwd2str(cfwd));
					res |= iPbx.feature_removeFromDatabase(cfwdDeviceLineStore, cfwdstr);
					res |= iPbx.feature_removeFromDatabase(cfwdLineDeviceStore, cfwdstr);
					sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: db clear %s %s (res:%d))\n", DEV_ID_LOG(device), cfwdDeviceLineStore, cfwdstr, res);
					if(ld->cfwd[cfwd].enabled) {
						res |= iPbx.feature_addToDatabase(cfwdDeviceLineStore, cfwdstr, ld->cfwd[cfwd].number);
						res |= iPbx.feature_addToDatabase(cfwdLineDeviceStore, cfwdstr, ld->cfwd[cfwd].number);
						sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: db put %s %s (res:%d)\n", DEV_ID_LOG(device), cfwdDeviceLineStore, cfwdstr, res);
					}
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
 * \param subscription Subscription as sccp_subscription_t (ByRef) [out]
 * \param extension char array [SCCP_MAX_EXTENSION] [out]
 * \return int containing number of matched subcription elements
 *
 * \callgraph
 * \callergraph
 */
int sccp_parseComposedId(const char *buttonString, unsigned int maxLength, sccp_subscription_t *subscription, char extension[SCCP_MAX_EXTENSION])
{
	pbx_assert(NULL != buttonString && NULL != subscription && NULL != extension);
	int res = 0;
	const char *stringIterator = 0;
	uint32_t i = 0;
	boolean_t endDetected = FALSE;
	enum {EXTENSION, ID, CIDNUM, CIDNAME, LABEL, AUX} state = EXTENSION;
	memset(subscription, 0, sizeof(sccp_subscription_t));

	for (stringIterator = buttonString; stringIterator < buttonString + maxLength && !endDetected; stringIterator++) {
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

                                // button = line, 98099@1:=98041$cid_name#label !default

                                // 98099 is the linename
                                // @ starts a subscriptionid section
                                // : starts the callerid section
                                // +/= add to or replace the cid
                                // 98041 is the cid_num part
                                // $ splits the cid_num from the cid_name part
                                // cid_name is the cid_name part
                                // label is the new label to use
                                // ! starts the options / AUX
                                // default makes this the default line to dial out on
                                // silent is another AUX option

				pbx_assert(i < sizeof(subscription->id));
				switch (*stringIterator) {
					case '\0':
						subscription->id[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case ':':
						subscription->id[i] = '\0';
						i = 0;
						state = CIDNUM;
						res++;
						break;
					case '#':
						subscription->id[i] = '\0';
						i = 0;
						state = LABEL;
						res++;
						break;
					case '!':
						subscription->id[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						subscription->id[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case CIDNUM:										// parsing of cid_num

				pbx_assert(i < sizeof(subscription->cid_num));
				switch (*stringIterator) {
					case '\0':
						subscription->cid_num[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '+':
						if(i == 0) {
							subscription->replaceCid = 0;
						}
						break;
					case '=':
						if(i == 0) {
							subscription->replaceCid = 1;
						}
						break;
					case '$':
						subscription->cid_num[i] = '\0';				// assign cidnum
						i = 0;
						state = CIDNAME;
						res++;
						break;
					case '#':
						subscription->cid_num[i] = '\0';
						i = 0;
						state = LABEL;
						res++;
						break;
					case '!':
						subscription->cid_num[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						subscription->cid_num[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case CIDNAME:										// parsing of cid_name
				pbx_assert(i < sizeof(subscription->cid_name));
				switch (*stringIterator) {
					case '\0':
						subscription->cid_name[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '#':
						subscription->cid_name[i] = '\0';
						i = 0;
						state = LABEL;
						res++;
						break;
					case '!':
						subscription->cid_name[i] = '\0';
						i = 0;
						state = AUX ;
						res++;
						break;
					default:
						subscription->cid_name[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case LABEL:										// parsing of sub id name
				pbx_assert(i < sizeof(subscription->label));
				switch (*stringIterator) {
					case '\0':
						subscription->label[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					case '!':
						subscription->label[i] = '\0';
						i = 0;
						state = AUX;
						res++;
						break;
					default:
						subscription->label[i] = *stringIterator;
						i++;
						break;
				}
				break;

			case AUX:										// parsing of auxiliary parameter
				pbx_assert(i < sizeof(subscription->aux));
				switch (*stringIterator) {
					case '\0':
						subscription->aux[i] = '\0';
						endDetected = TRUE;
						res++;
						break;
					default:
						subscription->aux[i] = *stringIterator;
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
	sccp_log(DEBUGCAT_CONFIG)(VERBOSE_PREFIX_3 "buttonString: %s, exten:%s, subId:%s, subCidNum:%s, subCidName:%s, replace:%s, Label:%s, Aux:%s\n",
		buttonString, extension, subscription->id, subscription->cid_num, subscription->cid_name, subscription->replaceCid ? "Y" : "N", subscription->label, subscription->aux);
	
	return res;
}

/*!
 * \brief Match Subscription ID
 * \param channel SCCP Channel
 * \param subId Subscription ID Number for ld
 * \return result as boolean
 *
 * \callgraph * \callergraph
 */
boolean_t __PURE__ sccp_util_matchSubscription(constChannelPtr channel, const char * subId)
{
	boolean_t result = TRUE;

	boolean_t filterPhones = FALSE;

	/* Determine if the phones registered on the shared line shall be filtered at all:
	   only if a non-trivial subscription id is specified with the calling channel,
	   which is not the default subscription id of the shared line denoting all devices,
	   the phones are addressed individually. (-DD) */
	filterPhones = FALSE;											/* set the default to call all phones */

	/* First condition: Non-trivial subscription specified for matching in call. */
	if (sccp_strlen(channel->subscription.cid_num) != 0) {
		/* Second condition: Subscription does not match default subscription of line. */
		if (0 != strncasecmp(channel->subscription.cid_num, channel->line->defaultSubscription.cid_num, sccp_strlen(channel->subscription.cid_num))) {
			filterPhones = TRUE;
		}
	}

	if (FALSE == filterPhones) {
		/* Accept phone for calling if all phones shall be called. */
		result = TRUE;
	} else if (0 != sccp_strlen(subId) &&								/* We already know that we won't search for a trivial subscription. */
		   (0 != strncasecmp(channel->subscription.cid_num, subId, sccp_strlen(channel->subscription.cid_num)))) {	/* Do the match! */
		result = FALSE;
	}
//#if 0
	pbx_log(LOG_NOTICE, "sccp_channel->subscription.cid_num=%s, length=%ld\n", channel->subscription.cid_num, sccp_strlen(channel->subscription.cid_num));
	pbx_log(LOG_NOTICE, "subscriptionId=%s, length=%ld\n", subId ? subId : "<NULL>", sccp_strlen(subId));

	pbx_log(LOG_NOTICE, "sccp_util_matchSubscription: sccp_channel->subscription.cid_num=%s, SubscriptionId=%s\n", (channel->subscription.cid_num) ? channel->subscription.cid_num : "NULL", (subId) ? subId : "NULL");
	pbx_log(LOG_NOTICE, "sccp_util_matchSubscription: result: %d\n", result);
//#endif
	return result;
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
	if ((s0->ss_family == s1->ss_family && sccp_netsock_cmp_addr(s0, s1) == 0) && sccp_netsock_cmp_port(s0, s1) == 0) {
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

int __PURE__ sccp_strIsNumeric(const char *s)
{
	if (*s) {
		char c = 0;

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
	struct sccp_ha * hal = NULL;

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
		int i = 0;

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
	const struct sccp_ha * current_ha = NULL;

	for (current_ha = ha; current_ha; current_ha = current_ha->next) {

		struct sockaddr_storage result;
		struct sockaddr_storage mapped_addr;
		const struct sockaddr_storage * addr_to_use = NULL;

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
	struct addrinfo * res = NULL;
	char * s = NULL;
	char * host = NULL;
	char * port = NULL;
	int e = 0;

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
	int mask = 0;

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
		int i = 0;

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
	struct sccp_ha * ha = NULL;
	struct sccp_ha * prev = NULL;
	struct sccp_ha * ret = NULL;
	char *tmp = pbx_strdupa(stuff);
	char * address = NULL;

	char * mask = NULL;
	int addr_is_v4 = 0;

	ret = path;
	while (path) {
		prev = path;
		path = path->next;
	}

	if (!(ha = (struct sccp_ha *)sccp_calloc(sizeof *ha, 1))) {
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
		int mask_is_v4 = 0;

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

	sccp_log (DEBUGCAT_HIGH) (VERBOSE_PREFIX_2 "%s/%s sense %d appended to acl for peer\n", sccp_netsock_stringify_addr (&ha->netaddr), sccp_netsock_stringify_addr (&ha->netmask), ha->sense);

	return ret;
}

void sccp_print_ha(struct ast_str *buf, int buflen, struct sccp_ha *path)
{
	while (path) {
		pbx_str_append (&buf, buflen, "%s:%s/%s,", AST_SENSE_DENY == path->sense ? "deny" : "permit", sccp_netsock_stringify_addr (&path->netaddr), sccp_netsock_stringify_addr (&path->netmask));
		path = path->next;
	}
}

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(chan_sccp_acl_tests)
{
	struct sccp_ha *ha = NULL;
	struct sockaddr_storage sas10;

	struct sockaddr_storage sas1015;

	struct sockaddr_storage sas172;

	struct sockaddr_storage sas200;

	struct sockaddr_storage sasff;

	struct sockaddr_storage sasffff;
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
	uint8_t i = 0;
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
#endif

/*!
 * \brief Print Group
 * \param buf Buf as char
 * \param buflen Buffer Lendth as int
 * \param group Group as sccp_group_t
 * \return Result as char
 */
void sccp_print_group(struct ast_str *buf, int buflen, sccp_group_t group)
{
	unsigned int i = 0;
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

int __PURE__ sccp_strversioncmp(const char *s1, const char *s2)
{
	static const char *digits = "0123456789";
	int ret = 0;

	int lz1 = 0;

	int lz2 = 0;
	size_t p1 = 0;

	size_t p2 = 0;

	p1 = strcspn(s1, digits);
	p2 = strcspn(s2, digits);
	while (p1 == p2 && s1[p1] != '\0' && s2[p2] != '\0') {
		/* Different prefix */
		ret = strncmp(s1, s2, p1);
		if(ret != 0) {
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
		ret = strncmp(s1, s2, p1);
		if(ret != 0) {
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
	int pos = 0;
	long long z = 0;

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
	char * end = NULL;

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
		long temp = strtol (buf, &end, 10);
		if (end != buf && errno != ERANGE && temp >= INT_MIN && temp <= INT_MAX) {
			result = (int)temp;
		}
	}
	return result;
}

int sccp_random(void)
{
	/* potentially replace with our own implementation */
	return (int)pbx_random();
}

/*
 * splt comma separated string to PBX_VARIABLES
 * caller is responsible for freeing PBX_VARIABLE
 */
PBX_VARIABLE_TYPE *sccp_split_comma2variables(const char *inputstr, size_t inputLen)
{
	PBX_VARIABLE_TYPE *root = NULL;
	PBX_VARIABLE_TYPE *param = NULL;
	char delims[]=",";
	char buf[inputLen + 1];
	sccp_copy_string(buf, inputstr, sizeof(buf));
	char *tokenrest;
	char *token = strtok_r(buf, delims, &tokenrest);
	while (token != NULL) {
		char *name = token;
		if ((name = strsep(&token, "="))) {
			char *value = token;
			PBX_VARIABLE_TYPE *new_param = pbx_variable_new(name, value, "");
			if (!new_param) {
				pbx_log(LOG_ERROR, "SCCP: (sccp_config) Error while creating new var structure\n");
				if (root) {
					pbx_variables_destroy(root);
				}
				return NULL;
			}
			if (!root) {
				root = param = new_param;
			} else {
				param->next = new_param;
				param = param->next;
			}
		}
		token = strtok_r(NULL, delims, &tokenrest);
	}
	return root;
}

const char * sccp_retrieve_str_variable_byKey(PBX_VARIABLE_TYPE *params, const char *key)
{
	PBX_VARIABLE_TYPE * param = NULL;
	for(param = params;param;param = param->next) {
		if(strcasecmp(key, param->name) == 0) {
			return param->value;
			break;
		}
	}
	return NULL;
}

int sccp_retrieve_int_variable_byKey(PBX_VARIABLE_TYPE *params, const char *key)
{
	const char *value = sccp_retrieve_str_variable_byKey(params, key);
	if (value) {
		return sccp_atoi(value, strlen(value));
	}
	return -1;
}

boolean_t sccp_append_variable(PBX_VARIABLE_TYPE *params, const char *key, const char *value)
{
	boolean_t res = FALSE;
	PBX_VARIABLE_TYPE * newvar = NULL;
	if ((newvar = pbx_variable_new(key, value, ""))) {
		if (params) {
			while(params->next) {
				params = params->next;
			}
			params->next = newvar;
		} else {
			params = newvar;
		}
		res = TRUE;
	} else {
		pbx_log(LOG_ERROR, "SCCP: (append_variable) Error while creating newvar structure\n");
	}
	return res;
}


gcc_inline int sccp_utf8_columnwidth(int width, const char *const ms)
{
	// don't use setlocale() as that is global to the process
	int res = 0;
	locale_t locale = newlocale(LC_ALL_MASK, "", NULL);
	locale_t old_locale = uselocale(locale);

	if(ms) {
		res = width + (strlen(ms) - mbstowcs(NULL, ms, width));
	}

	uselocale(old_locale);
	if(locale != (locale_t)0) {
		freelocale(locale);
	}

	return res;
}

gcc_inline boolean_t sccp_always_false(void)
{
	return FALSE;
}

gcc_inline boolean_t sccp_always_true(void)
{
	return TRUE;
}

gcc_inline sccp_feature_type_t sccp_cfwd2feature(const sccp_cfwd_t type)
{
	switch(type) {
		case SCCP_CFWD_ALL:
			return SCCP_FEATURE_CFWDALL;
		case SCCP_CFWD_BUSY:
			return SCCP_FEATURE_CFWDBUSY;
		case SCCP_CFWD_NOANSWER:
			return SCCP_FEATURE_CFWDNOANSWER;
		case SCCP_CFWD_NONE:
			return SCCP_FEATURE_CFWDNONE;
		default:
			return SCCP_FEATURE_TYPE_SENTINEL;
	}
}

gcc_inline sccp_cfwd_t sccp_feature2cfwd(const sccp_feature_type_t type)
{
	switch(type) {
		case SCCP_FEATURE_CFWDALL:
			return SCCP_CFWD_ALL;
		case SCCP_FEATURE_CFWDBUSY:
			return SCCP_CFWD_BUSY;
		case SCCP_FEATURE_CFWDNOANSWER:
			return SCCP_CFWD_NOANSWER;
		case SCCP_FEATURE_CFWDNONE:
			return SCCP_CFWD_NONE;
		default:
			return SCCP_CFWD_SENTINEL;
	}
}

gcc_inline skinny_stimulus_t sccp_cfwd2stimulus(const sccp_cfwd_t type)
{
	switch(type) {
		case SCCP_CFWD_ALL:
			return SKINNY_STIMULUS_FORWARDALL;
		case SCCP_CFWD_BUSY:
			return SKINNY_STIMULUS_FORWARDBUSY;
		case SCCP_CFWD_NOANSWER:
			return SKINNY_STIMULUS_FORWARDNOANSWER;
		case SCCP_CFWD_NONE:
		default:
			return SKINNY_STIMULUS_SENTINEL;
	}
}

gcc_inline const char * const sccp_cfwd2disp(const sccp_cfwd_t type)
{
	switch(type) {
		case SCCP_CFWD_ALL:
			return SKINNY_DISP_CFWDALL;
		case SCCP_CFWD_BUSY:
			return SKINNY_DISP_CFWDBUSY;
		case SCCP_CFWD_NOANSWER:
			return SKINNY_DISP_NOANSWER;
		case SCCP_CFWD_NONE:
		case SCCP_CFWD_SENTINEL:
		default:
			return "";
	}
}

#if CS_TEST_FRAMEWORK
static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(chan_sccp_acl_tests);
	AST_TEST_REGISTER(chan_sccp_acl_invalid_tests);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(chan_sccp_acl_tests);
	AST_TEST_UNREGISTER(chan_sccp_acl_invalid_tests);
}
#endif

#ifdef DEBUG
#if ASTERISK_VERSION_GROUP < 112 
#if HAVE_EXECINFO_H
static char **__sccp_bt_get_symbols(void **addresses, size_t num_frames)
{
	char ** strings = NULL;
#			if defined(HAVE_DLADDR_H) && defined(HAVE_BFD_H)
	size_t stackfr;
	bfd * bfdobj = NULL;    /* bfd.h */
	Dl_info dli;		/* dlfcn.h */
	long allocsize;
	asymbol **syms = NULL;	/* bfd.h */
	bfd_vma offset;		/* bfd.h */
	const char * lastslash = NULL;
	asection * section = NULL;
	const char *file, *func;
	unsigned int line;
	char address_str[128];
	char msg[1024];
	size_t strings_size;
	size_t * eachlen = NULL;

	strings_size = num_frames * sizeof(*strings);

	eachlen = (size_t *) sccp_calloc(sizeof *eachlen, num_frames);
	strings = (char **) sccp_calloc(sizeof *strings, num_frames);
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
			(syms = (asymbol **)sccp_malloc(allocsize)) &&
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
					const char * prevslash = NULL;

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
			char ** tmp = NULL;

			eachlen[stackfr] = strlen(msg) + 1;
			if (!(tmp = (char **)sccp_realloc(strings, strings_size + eachlen[stackfr]))) {
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

#if defined(HAVE_EXECINFO_H) && defined(HAVE_BKTR)
	void	*addresses[SCCP_BACKTRACE_SIZE];
	size_t size = 0;

	size_t i = 0;
	bt_string_t * strings = NULL;
	struct ast_str * btbuf = NULL;
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
#ifdef CS_AST_BACKTRACE_VECTOR_STRING
			//struct ast_vector_string * strings;
			pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, " (bt) > %s\n", AST_VECTOR_GET(strings, i));
#else
			pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, " (bt) > %s\n", strings[i]);
#endif			
		}
		bt_free(strings);

		pbx_str_append(&btbuf, DEFAULT_PBX_STR_BUFFERSIZE, "================================================================================\n");
		pbx_log(LOG_WARNING, "SCCP: (backtrace) \n%s\n", pbx_str_buffer(btbuf));
	}
#endif	// HAVE_EXECINFO_H
}
#endif  // DEBUG
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
