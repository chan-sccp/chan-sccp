/*!
 * \file        sccp_device.c
 * \brief       SCCP Device Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * \remarks
 * Purpose:     SCCP Device
 * When to use: Only methods directly related to sccp devices should be stored in this source file.
 * Relations:   SCCP Device -> SCCP DeviceLine -> SCCP Line
 *              SCCP Line -> SCCP ButtonConfig -> SCCP Device
 *
 * \date        $Date$
 * \version     $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_config.h"
#include "sccp_actions.h"
#include "sccp_features.h"
#include "sccp_socket.h"
#include "sccp_indicate.h"
#include "sccp_mwi.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");
int __sccp_device_destroy(const void *ptr);
void sccp_device_removeFromGlobals(sccp_device_t * device);
int sccp_device_destroy(const void *ptr);

/* indicate definition */
static void sccp_device_old_indicate_remoteHold(const sccp_device_t * device, uint8_t lineInstance, uint32_t callid, uint8_t callpriority, uint8_t callPrivacy);
static void sccp_device_new_indicate_remoteHold(const sccp_device_t * device, uint8_t lineInstance, uint32_t callid, uint8_t callpriority, uint8_t callPrivacy);
static void sccp_device_indicate_offhook(const sccp_device_t * device, sccp_linedevices_t * linedevice, uint32_t callid);
static void sccp_device_indicate_onhook(const sccp_device_t * device, const uint8_t lineInstance, uint32_t callid);
static void sccp_device_indicate_dialing(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel);
static void sccp_device_indicate_proceed(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel);
static void sccp_device_indicate_connected(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);
static void sccp_device_indicate_offhook_remote(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);
static void sccp_device_indicate_onhook_remote(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);

/* end indicate */
static sccp_push_result_t sccp_device_pushURL(const sccp_device_t * device, const char *url, uint8_t priority, uint8_t tone);
static sccp_push_result_t sccp_device_pushURLNotSupported(const sccp_device_t * device, const char *url, uint8_t priority, uint8_t tone)
{
	return SCCP_PUSH_RESULT_NOT_SUPPORTED;
}

static sccp_push_result_t sccp_device_pushTextMessage(const sccp_device_t * device, const char *messageText, const char *from, uint8_t priority, uint8_t tone);
static sccp_push_result_t sccp_device_pushTextMessageNotSupported(const sccp_device_t * device, const char *messageText, const char *from, uint8_t priority, uint8_t tone)
{
	return SCCP_PUSH_RESULT_NOT_SUPPORTED;
}

static const struct sccp_device_indication_cb sccp_device_indication_newerDevices = {
	.remoteHold = sccp_device_new_indicate_remoteHold,
	.remoteOffhook = sccp_device_indicate_offhook_remote,
	.remoteOnhook = sccp_device_indicate_onhook_remote,
	.offhook = sccp_device_indicate_offhook,
	.onhook = sccp_device_indicate_onhook,
	.dialing = sccp_device_indicate_dialing,
	.proceed = sccp_device_indicate_proceed,
	.connected = sccp_device_indicate_connected,
};

static const struct sccp_device_indication_cb sccp_device_indication_olderDevices = {
	.remoteHold = sccp_device_old_indicate_remoteHold,
	.remoteOffhook = sccp_device_indicate_offhook_remote,
	.remoteOnhook = sccp_device_indicate_onhook_remote,
	.offhook = sccp_device_indicate_offhook,
	.onhook = sccp_device_indicate_onhook,
	.dialing = sccp_device_indicate_dialing,
	.proceed = sccp_device_indicate_proceed,
	.connected = sccp_device_indicate_connected,
};

static boolean_t sccp_device_checkACLTrue(sccp_device_t * device)
{
	return TRUE;
}

static boolean_t sccp_device_trueResult(void)
{
	return TRUE;
}

static boolean_t sccp_device_falseResult(void)
{
	return FALSE;
}

static void sccp_device_retrieveDeviceCapabilities(const sccp_device_t * device)
{
	char *xmlStr = "<getDeviceCaps></getDeviceCaps>";
	unsigned int transactionID = random();

	device->protocol->sendUserToDeviceDataVersionMessage(device, APPID_DEVICECAPABILITIES, 1, 0, transactionID, xmlStr, 2);
}

static void sccp_device_setBackgroundImage(const sccp_device_t * device, const char *url)
{
	char xmlStr[2048] = { 0 };
	unsigned int transactionID = random();

	if (strncasecmp("http://", url, strlen("http://")) != 0) {
		pbx_log(LOG_WARNING, "SCCP: '%s' needs to be a valid http url\n", url ? url : "");
		return;
	}
	strcat(xmlStr, "<setBackground>");
	strcat(xmlStr, "<background>");
	strcat(xmlStr, "<image>");
	strcat(xmlStr, url);
	strcat(xmlStr, "</image>");
	strcat(xmlStr, "<icon>");
	strcat(xmlStr, url);
	strcat(xmlStr, "</icon>");
	strcat(xmlStr, "</background>");
	strcat(xmlStr, "</setBackground>\n\0");

	device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 0, transactionID, xmlStr, 0);
	//sccp_log(DEBUGCAT_CORE)("%s: sent new background to device: %s, from d->backgroundImage: %s\n", device->id, url, device->backgroundImage);
}

static sccp_dtmfmode_t sccp_device_getDtfmMode(const sccp_device_t * device)
{
	sccp_dtmfmode_t res = device->dtmfmode;

	if (device->dtmfmode == SCCP_DTMFMODE_AUTO) {
		if ((device->device_features & SKINNY_PHONE_FEATURES_RFC2833) == SKINNY_PHONE_FEATURES_RFC2833) {
			res = SCCP_DTMFMODE_RFC2833;
		} else {
			res = SCCP_DTMFMODE_SKINNY;
		}
	}
	return res;
}

static void sccp_device_setBackgroundImageNotSupported(const sccp_device_t * device, const char *url)
{
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: does not support Background Image\n", device->id);
}

static void sccp_device_displayBackgroundImagePreview(const sccp_device_t * device, const char *url)
{
	char xmlStr[2048];
	unsigned int transactionID = random();

	if (strncmp("http://", url, strlen("http://")) != 0) {
		pbx_log(LOG_WARNING, "SCCP: '%s' needs to bee a valid http url\n", url ? url : "");
	}
	memset(xmlStr, 0, sizeof(xmlStr));

	strcat(xmlStr, "<setBackgroundPreview>");
	strcat(xmlStr, "<image>");
	strcat(xmlStr, url);
	strcat(xmlStr, "</image>");
	strcat(xmlStr, "</setBackgroundPreview>\n\0");

	device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 0, transactionID, xmlStr, 0);
}

static void sccp_device_displayBackgroundImagePreviewNotSupported(const sccp_device_t * device, const char *url)
{
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: does not support Background Image\n", device->id);
}

static void sccp_device_setRingtone(const sccp_device_t * device, const char *url)
{
	char xmlStr[2048];
	unsigned int transactionID = random();

	if (strncmp("http://", url, strlen("http://")) != 0) {
		pbx_log(LOG_WARNING, "SCCP: '%s' needs to bee a valid http url\n", url ? url : "");
	}

	memset(xmlStr, 0, sizeof(xmlStr));

	strcat(xmlStr, "<setRingTone>");
	strcat(xmlStr, "<ringTone>");
	strcat(xmlStr, url);
	strcat(xmlStr, "</ringTone>");
	strcat(xmlStr, "</setRingTone>\n\0");

	device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 0, transactionID, xmlStr, 0);
}

static void sccp_device_copyStr2Locale_UTF8(const sccp_device_t *d, char *dst, const char *src, size_t dst_size)
{
	sccp_copy_string(dst, src, dst_size);
}

#if HAVE_ICONV_H
static void sccp_device_copyStr2Locale_Convert(const sccp_device_t *d, char *dst, const char *src, size_t dst_size)
{
	char *buf = sccp_alloca(dst_size);
	size_t buf_len = dst_size;
	memset(buf, 0, dst_size);
	if (sccp_utils_convUtf8toLatin1(src, buf, buf_len)) {
		sccp_copy_string(dst, buf, dst_size);
		return;
	}
}
#endif

static void sccp_device_setRingtoneNotSupported(const sccp_device_t * device, const char *url)
{
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: does not support setting ringtone\n", device->id);
}

/*
   static void sccp_device_startStream(const sccp_device_t *device, const char *address, uint32_t port){
   char xmlStr[2048];
   unsigned int transactionID = random();

   strcat(xmlStr, "<startMedia>");
   strcat(xmlStr, "<mediaStream>");
   //strcat(xmlStr, "<onStopped></onStopped>"); url
   strcat(xmlStr, "<receiveVolume>50</receiveVolume>"); // 0-100
   strcat(xmlStr, "<type>audio</type>"); // send|receive|sendReceive
   strcat(xmlStr, "<mode>sendReceive</mode>"); // send|receive|sendReceive
   strcat(xmlStr, "<codec>Wideband</codec>"); // "G.711" "G.722" "G.723" "G.728" "G.729" "GSM" "Wideband" "iLBC"
   strcat(xmlStr, "<address>");
   strcat(xmlStr, address);
   strcat(xmlStr, "</address>");
   strcat(xmlStr, "<port>20480</port>");
   strcat(xmlStr, "</mediaStream>");
   strcat(xmlStr, "</startMedia>\n\0");

   device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 0, transactionID, xmlStr, 0);
   }
 */

/*!
 * \brief Check device ipaddress against the ip ACL (permit/deny and permithosts entries)
 */
static boolean_t sccp_device_checkACL(sccp_device_t * device)
{

	struct sockaddr_storage sas;
	boolean_t matchesACL = FALSE;

	if (!device || !device->session) {
		return FALSE;
	}

	/* get current socket information */
	memcpy(&sas, &device->session->sin, sizeof(struct sockaddr_storage));

	/* no permit deny information */
	if (!device->ha) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: no deny/permit information for this device, allow all connections\n", device->id);
		return TRUE;
	}

	if (sccp_apply_ha(device->ha, &sas) != AST_SENSE_ALLOW) {
		// checking permithosts
		struct ast_str *ha_buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

		sccp_print_ha(ha_buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(ha));

		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: not allowed by deny/permit list (%s). Checking permithost list...\n", device->id, pbx_str_buffer(ha_buf));

		/*! \todo check permithosts with IPv6 */
		/*
		   struct ast_hostent ahp;
		   struct hostent *hp;
		   sccp_hostname_t *permithost;

		   uint8_t i = 0;

		   SCCP_LIST_TRAVERSE_SAFE_BEGIN(&device->permithosts, permithost, list) {
		   if ((hp = pbx_gethostbyname(permithost->name, &ahp))) {
		   for (i = 0; NULL != hp->h_addr_list[i]; i++) {                                       // walk resulting ip address
		   if (sin.sin_addr.s_addr == (*(struct in_addr *) hp->h_addr_list[i]).s_addr) {
		   sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: permithost = %s match found.\n", device->id, permithost->name);
		   matchesACL = TRUE;
		   continue;
		   }
		   }
		   } else {
		   sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Invalid address resolution for permithost = %s (skipping permithost).\n", device->id, permithost->name);
		   }
		   }
		   SCCP_LIST_TRAVERSE_SAFE_END;
		 */
	} else {
		matchesACL = TRUE;
	}
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: checkACL returning %s\n", device->id, matchesACL ? "TRUE" : "FALSE");
	return matchesACL;
}

/*!
 * \brief run before reload is start on devices
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 */
void sccp_device_pre_reload(void)
{
	sccp_device_t *d;
	sccp_buttonconfig_t *config;

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "%s: Setting Device to Pending Delete=1\n", d->id);
		if (!d->realtime) {										/* don't want to reset hotline devices. */
			d->pendingDelete = 1;
		}
		d->pendingUpdate = 0;
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
			config->pendingDelete = 1;
			config->pendingUpdate = 0;
		}
		SCCP_LIST_UNLOCK(&d->buttonconfig);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
}

/*!
 * \brief Check Device Update Status
 * \note See \ref sccp_config_reload
 * \param device SCCP Device
 * \return Result as Boolean
 *
 * \callgraph
 * \callergraph
 */
boolean_t sccp_device_check_update(sccp_device_t * device)
{
	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);
	boolean_t res = FALSE;

	if (d) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "%s (check_update) pendingUpdate: %s, pendingDelete: %s\n", d->id, d->pendingUpdate ? "TRUE" : "FALSE", d->pendingDelete ? "TRUE" : "FALSE");

		if ((d->pendingUpdate || d->pendingDelete)) {
			do {
				if (sccp_device_numberOfChannels(d) > 0) {
					sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "device: %s check_update, openchannel: %d -> device restart pending.\n", d->id, sccp_device_numberOfChannels(d));
					break;
				}

				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "Device %s needs to be reset because of a change in sccp.conf (Update:%d, Delete:%d)\n", d->id, d->pendingUpdate, d->pendingDelete);

				d->pendingUpdate = 0;
				if (d->pendingDelete) {
					sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Remove Device from List\n", d->id);
					sccp_dev_clean(d, TRUE, 0);
				} else {
					sccp_dev_clean(d, FALSE, 0);
				}
				res = TRUE;
			} while (0);
		}
	}
	return res;
}

/*!
 * \brief run after the new device config is loaded during the reload process
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 *
 */
void sccp_device_post_reload(void)
{
	sccp_device_t *d;
	sccp_log((DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "SCCP: (post_reload)\n");

	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(devices), d, list) {
		if (!d->pendingDelete && !d->pendingUpdate) {
			continue;
		}
		/* Because of the previous check, the only reason that the device hasn't
		 * been updated will be because it is currently engaged in a call.
		 */
		if (!sccp_device_check_update(d)) {
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Device %s will receive reset after current call is completed\n", d->id);
		}
		/* should we re-check the device after hangup ? */
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

/*!
 * \brief create a device and adding default values.
 * \return retained device with default/global values
 *
 * \callgraph
 * \callergraph
 */
sccp_device_t *sccp_device_create(const char *id)
{
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Create Device\n");
	sccp_device_t *d = (sccp_device_t *) sccp_refcount_object_alloc(sizeof(sccp_device_t), SCCP_REF_DEVICE, id, __sccp_device_destroy);

	if (!d) {
		pbx_log(LOG_ERROR, "Unable to allocate memory for a device\n");
		return NULL;
	}

	memset(d, 0, sizeof(sccp_device_t));
	sccp_copy_string(d->id, id, sizeof(d->id));
	SCCP_LIST_HEAD_INIT(&d->buttonconfig);
	SCCP_LIST_HEAD_INIT(&d->selectedChannels);
	SCCP_LIST_HEAD_INIT(&d->addons);
#ifdef CS_DEVSTATE_FEATURE
	SCCP_LIST_HEAD_INIT(&d->devstateSpecifiers);
#endif
	/*
	if (PBX(endpoint_create)) {
		d->endpoint = PBX(endpoint_create)("sccp", id);
	}
	*/
	memset(d->softKeyConfiguration.activeMask, 0xFFFF, sizeof(d->softKeyConfiguration.activeMask));
	memset(d->call_statistics, 0, (sizeof(sccp_call_statistics_t) * 2));

	d->softKeyConfiguration.modes = (softkey_modes *) SoftKeyModes;
	d->softKeyConfiguration.size = ARRAY_LEN(SoftKeyModes);
	d->state = SCCP_DEVICESTATE_ONHOOK;
	d->postregistration_thread = AST_PTHREADT_STOP;
	d->registrationState = SKINNY_DEVICE_RS_NONE;

	// set minimum protocol levels
	// d->protocolversion = SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW;
	// d->protocol = sccp_protocol_getDeviceProtocol(d, SCCP_PROTOCOL);

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Init MessageStack\n");

	/* initialize messageStack */
#ifndef SCCP_ATOMIC
	pbx_mutex_init(&d->messageStack.lock);
	sccp_mutex_lock(&d->messageStack.lock);
#endif
	uint8_t i;

	for (i = 0; i < ARRAY_LEN(d->messageStack.messages); i++) {
		d->messageStack.messages[i] = NULL;
	}
#ifndef SCCP_ATOMIC
	sccp_mutex_unlock(&d->messageStack.lock);
#endif

	// /* disable videomode and join softkey for all softkeysets */
	/*
	for (i = 0; i < KEYMODE_ONHOOKSTEALABLE; i++) {
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_VIDEO_MODE, FALSE);
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_JOIN, FALSE);
	}
	*/

	d->pushURL = sccp_device_pushURLNotSupported;
	d->pushTextMessage = sccp_device_pushTextMessageNotSupported;
	d->checkACL = sccp_device_checkACL;
	d->hasDisplayPrompt = sccp_device_trueResult;
	d->hasEnhancedIconMenuSupport = sccp_device_falseResult;
	d->setBackgroundImage = sccp_device_setBackgroundImageNotSupported;
	d->displayBackgroundImagePreview = sccp_device_displayBackgroundImagePreviewNotSupported;
	d->retrieveDeviceCapabilities = sccp_device_retrieveDeviceCapabilities;
	d->setRingTone = sccp_device_setRingtoneNotSupported;
	d->getDtmfMode = sccp_device_getDtfmMode;
	d->copyStr2Locale = sccp_device_copyStr2Locale_UTF8;

	d->pendingUpdate = 0;
	d->pendingDelete = 0;
	return d;
}

/*!
 * \brief create an anonymous device and adding default values.
 * \return retained device with default/global values
 *
 * \callgraph
 * \callergraph
 */
sccp_device_t *sccp_device_createAnonymous(const char *name)
{
	sccp_device_t *d = sccp_device_create(name);

	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: sccp_device_create(%s) failed", name);
		return NULL;
	}

	d->realtime = TRUE;
	d->isAnonymous = TRUE;
	d->checkACL = sccp_device_checkACLTrue;
	return d;
}

void sccp_device_setLastNumberDialed(sccp_device_t * device, const char *lastNumberDialed, const sccp_linedevices_t *linedevice)
{
	boolean_t ResetNoneLineInstance = FALSE;

#ifdef CS_ADV_FEATURES
	if (device->useRedialMenu) {
		return;
	}
#endif
	boolean_t redial_active;

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Update last number dialed to %s.\n", DEV_ID_LOG(device), lastNumberDialed);
	if (lastNumberDialed && !sccp_strlen_zero(lastNumberDialed)) {
		if (sccp_strlen_zero(device->redialInformation.number)) {
			ResetNoneLineInstance = TRUE;
		}
		sccp_copy_string(device->redialInformation.number, lastNumberDialed, sizeof(device->redialInformation.number));
		device->redialInformation.lineInstance = linedevice->lineInstance;
		redial_active = TRUE;
	} else {
		sccp_copy_string(device->redialInformation.number, "", sizeof(device->redialInformation.number));
		device->redialInformation.lineInstance = 0;
		redial_active = FALSE;
	}
	sccp_softkey_setSoftkeyState(device, KEYMODE_ONHOOK, SKINNY_LBL_REDIAL, redial_active);
	sccp_softkey_setSoftkeyState(device, KEYMODE_OFFHOOK, SKINNY_LBL_REDIAL, redial_active);
	sccp_softkey_setSoftkeyState(device, KEYMODE_OFFHOOKFEAT, SKINNY_LBL_REDIAL, redial_active);
	sccp_softkey_setSoftkeyState(device, KEYMODE_ONHOOKSTEALABLE, SKINNY_LBL_REDIAL, redial_active);
	if (ResetNoneLineInstance) {
		sccp_dev_set_keyset(device, 0, 0, KEYMODE_ONHOOK);
	}
}

/*!
 * \brief set type of Indicate protocol by device type
 */
void sccp_device_preregistration(sccp_device_t * device)
{
	/*! \todo use device->device_features to detect devices capabilities, instead of hardcoded list of devices */
	switch (device->skinny_type) {
			// case SKINNY_DEVICETYPE_30SPPLUS:
			// case SKINNY_DEVICETYPE_30VIP:
			// case SKINNY_DEVICETYPE_12SPPLUS:
			// case SKINNY_DEVICETYPE_12SP:
			// case SKINNY_DEVICETYPE_12:
			// case SKINNY_DEVICETYPE_CISCO7902:
			// case SKINNY_DEVICETYPE_CISCO7912:
			// case SKINNY_DEVICETYPE_CISCO7911:
			// case SKINNY_DEVICETYPE_CISCO7906:
			// case SKINNY_DEVICETYPE_CISCO7905:
			// case SKINNY_DEVICETYPE_CISCO7931:
			// case SKINNY_DEVICETYPE_CISCO7935:
			// case SKINNY_DEVICETYPE_CISCO7936:
			// case SKINNY_DEVICETYPE_CISCO7937:
			// case SKINNY_DEVICETYPE_CISCO7910:
			// case SKINNY_DEVICETYPE_CISCO7940:
			// case SKINNY_DEVICETYPE_CISCO7960:
		case SKINNY_DEVICETYPE_CISCO7941:
		case SKINNY_DEVICETYPE_CISCO7941GE:
		case SKINNY_DEVICETYPE_CISCO7942:
		case SKINNY_DEVICETYPE_CISCO7945:
			// case SKINNY_DEVICETYPE_CISCO7920:
		case SKINNY_DEVICETYPE_CISCO7921:
		case SKINNY_DEVICETYPE_CISCO7925:
		case SKINNY_DEVICETYPE_CISCO7985:
		case SKINNY_DEVICETYPE_CISCO7961:
		case SKINNY_DEVICETYPE_CISCO7961GE:
		case SKINNY_DEVICETYPE_CISCO7962:
		case SKINNY_DEVICETYPE_CISCO7965:
		case SKINNY_DEVICETYPE_CISCO7970:
		case SKINNY_DEVICETYPE_CISCO7971:
		case SKINNY_DEVICETYPE_CISCO7975:
		case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
			device->indicate = &sccp_device_indication_newerDevices;
			break;
		default:
			device->indicate = &sccp_device_indication_olderDevices;
			break;
	}
#if HAVE_ICONV_H
	if (device && !(device->device_features & SKINNY_PHONE_FEATURES_UTF8)) {
		device->copyStr2Locale = sccp_device_copyStr2Locale_Convert;
	}
#endif
}

/*!
 * \brief Add a device to the global sccp_device list
 * \param device SCCP Device
 * \return SCCP Device
 *
 * \note needs to be called with a retained device
 * \note adds a retained device to the list (refcount + 1)
 */
void sccp_device_addToGlobals(sccp_device_t * device)
{
	if (!device) {
		pbx_log(LOG_ERROR, "Adding null to the global device list is not allowed!\n");
		return;
	}
	device = sccp_device_retain(device);
	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_INSERT_SORTALPHA(&GLOB(devices), device, list, id);
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Added device '%s' (%s) to Glob(devices)\n", device->id, device->config_type);
}

/*!
 * \brief Removes a device from the global sccp_device list
 * \param device SCCP Device
 * \return device or NULL
 *
 * \note needs to be called with a retained device
 * \note removes the retained device withing the list (refcount - 1)
 */
void sccp_device_removeFromGlobals(sccp_device_t * device)
{
	if (!device) {
		pbx_log(LOG_ERROR, "Removing null from the global device list is not allowed!\n");
		return;
	}

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	device = SCCP_RWLIST_REMOVE(&GLOB(devices), device, list);
	sccp_device_release(device);
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Removed device '%s' from Glob(devices)\n", DEV_ID_LOG(device));
}

/*!
 * \brief Create a template of Buttons as Definition for a Phonetype (d->skinny_type)
 * \param d device
 * \param btn buttonlist
 */
void sccp_dev_build_buttontemplate(sccp_device_t * d, btnlist * btn)
{
	uint8_t i;

	sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_BUTTONTEMPLATE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Building button template %s(%d), user config %s\n", d->id, skinny_devicetype2str(d->skinny_type), d->skinny_type, d->config_type);

	switch (d->skinny_type) {
		case SKINNY_DEVICETYPE_30SPPLUS:
		case SKINNY_DEVICETYPE_30VIP:
			/* 13 rows, 2 columns */
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			/* Column 2 */
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			for (i = 0; i < 4; i++) {
				(btn++)->type = SKINNY_BUTTONTYPE_UNDEFINED;
			}
			for (i = 0; i < 13; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_12SPPLUS:
		case SKINNY_DEVICETYPE_12SP:
		case SKINNY_DEVICETYPE_12:
			/* 6 rows, 2 columns */
			for (i = 0; i < 2; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_LINE;
			}
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			break;
		case SKINNY_DEVICETYPE_CISCO7902:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			break;
		case SKINNY_DEVICETYPE_CISCO7910:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
			for (i = 0; i < 2; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			break;
		case SKINNY_DEVICETYPE_CISCO7906:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			for (i = 0; i < 9; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7911:
		case SKINNY_DEVICETYPE_CISCO7905:
		case SKINNY_DEVICETYPE_CISCO7912:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			for (i = 0; i < 9; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			d->hasEnhancedIconMenuSupport = sccp_device_trueResult;
			break;
		case SKINNY_DEVICETYPE_CISCO7920:
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7931:
			for (i = 0; i < 20; i++) {
				btn[i].type = SCCP_BUTTONTYPE_MULTI;
			}
			btn[20].type = SKINNY_BUTTONTYPE_MESSAGES;
			btn[20].instance = 21;
			btn[21].type = SKINNY_BUTTONTYPE_DIRECTORY;
			btn[21].instance = 22;
			btn[22].type = SKINNY_BUTTONTYPE_HEADSET;
			btn[22].instance = 23;
			btn[23].type = SKINNY_BUTTONTYPE_APPLICATION;
			btn[23].instance = 24;
			d->hasEnhancedIconMenuSupport = sccp_device_trueResult;
			break;
		case SKINNY_DEVICETYPE_CISCO7935:
		case SKINNY_DEVICETYPE_CISCO7936:
		case SKINNY_DEVICETYPE_CISCO7937:
			for (i = 0; i < 2; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_LINE;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7921:
		case SKINNY_DEVICETYPE_CISCO7925:
			for (i = 0; i < 6; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7940:
			/* add text message support */
			d->pushTextMessage = sccp_device_pushTextMessage;
			d->pushURL = sccp_device_pushURL;

			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7941:
		case SKINNY_DEVICETYPE_CISCO7941GE:
		case SKINNY_DEVICETYPE_CISCO7942:
		case SKINNY_DEVICETYPE_CISCO7945:
			/* add text message support */
			d->pushTextMessage = sccp_device_pushTextMessage;
			d->pushURL = sccp_device_pushURL;
			d->hasEnhancedIconMenuSupport = sccp_device_trueResult;

			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7960:
			/* add text message support */
			d->pushTextMessage = sccp_device_pushTextMessage;
			d->pushURL = sccp_device_pushURL;
			d->setBackgroundImage = sccp_device_setBackgroundImage;
			d->displayBackgroundImagePreview = sccp_device_displayBackgroundImagePreview;
			d->setRingTone = sccp_device_setRingtone;

			for (i = 6 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7961:
		case SKINNY_DEVICETYPE_CISCO7961GE:
		case SKINNY_DEVICETYPE_CISCO7962:
		case SKINNY_DEVICETYPE_CISCO7965:
			/* add text message support */
			d->pushTextMessage = sccp_device_pushTextMessage;
			d->pushURL = sccp_device_pushURL;
			d->setBackgroundImage = sccp_device_setBackgroundImage;
			d->displayBackgroundImagePreview = sccp_device_displayBackgroundImagePreview;
			d->setRingTone = sccp_device_setRingtone;
			d->hasEnhancedIconMenuSupport = sccp_device_trueResult;

			for (i = 6 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7970:
		case SKINNY_DEVICETYPE_CISCO7971:
		case SKINNY_DEVICETYPE_CISCO7975:
			/* the nokia icc client identifies it self as SKINNY_DEVICETYPE_CISCO7970, but it can only have one line  */
			if (!strcasecmp(d->config_type, "nokia-icc")) {						// this is for nokia icc legacy support (Old releases) -FS
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			} else {
				for (i = 8 + sccp_addons_taps(d); i > 0; i--) {
					(btn++)->type = SCCP_BUTTONTYPE_MULTI;
				}

				/* add text message support */
				d->pushTextMessage = sccp_device_pushTextMessage;
				d->pushURL = sccp_device_pushURL;
				d->setBackgroundImage = sccp_device_setBackgroundImage;
				d->displayBackgroundImagePreview = sccp_device_displayBackgroundImagePreview;
				d->setRingTone = sccp_device_setRingtone;
				d->hasEnhancedIconMenuSupport = sccp_device_trueResult;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
			/* the nokia icc client identifies it self as SKINNY_DEVICETYPE_CISCO7970, but it can only have one line  */
			if (!strcasecmp(d->config_type, "nokia-icc")) {						// this is for nokia icc legacy support (Old releases) -FS
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			} else {
				for (i = 8 + sccp_addons_taps(d); i > 0; i--) {
					(btn++)->type = SCCP_BUTTONTYPE_MULTI;
				}

				/* add text message support */
				d->pushTextMessage = sccp_device_pushTextMessage;
				d->pushURL = sccp_device_pushURL;
				d->setBackgroundImage = sccp_device_setBackgroundImage;
				d->displayBackgroundImagePreview = sccp_device_displayBackgroundImagePreview;
				d->setRingTone = sccp_device_setRingtone;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO7985:
			d->capabilities.video[0] = SKINNY_CODEC_H264;
			d->capabilities.video[1] = SKINNY_CODEC_H263;
#ifdef CS_SCCP_VIDEO
			sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
#endif
			for (i = 0; i < 1; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_NOKIA_ICC:
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_NOKIA_E_SERIES:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			for (i = 0; i < 5; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			break;
		case SKINNY_DEVICETYPE_VGC:
		case SKINNY_DEVICETYPE_ANALOG_GATEWAY:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			d->hasDisplayPrompt = sccp_device_falseResult;
			break;
		case SKINNY_DEVICETYPE_ATA188:
		case SKINNY_DEVICETYPE_ATA186:
			//case SKINNY_DEVICETYPE_ATA188:
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			d->hasDisplayPrompt = sccp_device_falseResult;
			break;
		case SKINNY_DEVICETYPE_CISCO8941:
		case SKINNY_DEVICETYPE_CISCO8945:
#ifdef CS_SCCP_VIDEO
			d->capabilities.video[0] = SKINNY_CODEC_H264;
			d->capabilities.video[1] = SKINNY_CODEC_H263;
			sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
#endif
			d->hasDisplayPrompt = sccp_device_falseResult;
			for (i = 0; i < 10; i++) {								// 4 visible, 6 in dropdown
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			/*
			for (i = 5; i <= 10; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			*/
			(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
			(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
			break;

		case SKINNY_DEVICETYPE_SPA_502G:
		case SKINNY_DEVICETYPE_SPA_521S:
			for (i = 0; i < 1; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_SPA_303G:
			for (i = 0; i < 3; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_SPA_504G:
		case SKINNY_DEVICETYPE_SPA_524SG:
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_SPA_509G:
			for (i = 0; i < 12; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
			(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_SPA_525G2:
			for (i = 0; i < 8; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			for (i = 2 + sccp_addons_taps(d); i > 0; i--) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			break;
		case SKINNY_DEVICETYPE_CISCO6901:
		case SKINNY_DEVICETYPE_CISCO6911:
			d->hasDisplayPrompt = sccp_device_falseResult;
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_CISCO6921:
			d->hasDisplayPrompt = sccp_device_falseResult;
			for (i = 0; i < 2; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			/*
			for (i = 0; i < 6; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			}
			(btn++)->type = SKINNY_BUTTONTYPE_NONE;
			(btn++)->type = SKINNY_BUTTONTYPE_PRIVACY;
			(btn++)->type = SKINNY_BUTTONTYPE_DO_NOT_DISTURB;
			(btn++)->type = SKINNY_BUTTONTYPE_HLOG;                       // hunt group logout
			*/
			break;
		case SKINNY_DEVICETYPE_CISCO6941:
		case SKINNY_DEVICETYPE_CISCO6945:
			for (i = 0; i < 4; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			d->hasDisplayPrompt = sccp_device_falseResult;
			break;
		case SKINNY_DEVICETYPE_CISCO6961:
			for (i = 0; i < 12; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			d->hasDisplayPrompt = sccp_device_falseResult;
			break;
		default:
			pbx_log(LOG_WARNING, "Unknown device type '%d' found.\n", d->skinny_type);
			/* at least one line */
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
			break;
	}

	if (d->skinny_type < 6 || sccp_strcaseequals(d->config_type, "kirk")) {
		d->hasDisplayPrompt = sccp_device_falseResult;
	}

	return;
}

/*!
 * \brief Build an SCCP Message Packet
 * \param[in] t SCCP Message Text
 * \param[out] pkt_len Packet Length
 * \return SCCP Message
 */
sccp_msg_t __attribute__ ((malloc)) * sccp_build_packet(sccp_mid_t t, size_t pkt_len)
{
	sccp_msg_t *msg = sccp_calloc(1, pkt_len + SCCP_PACKET_HEADER);

	if (!msg) {
		pbx_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
		return NULL;
	}
	msg->header.length = htolel(pkt_len + 4);
	msg->header.lel_messageId = htolel(t);
	return msg;
}

/*!
 * \brief Send SCCP Message to Device
 * \param d SCCP Device
 * \param msg SCCP Message
 * \return Status as int
 *
 * \callgraph
 */
int sccp_dev_send(const sccp_device_t * d, sccp_msg_t * msg)
{
	int result = -1;

	if (d && d->session && msg) {
		sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: >> Send message %s\n", d->id, msgtype2str(letohl(msg->header.lel_messageId)));
		result = sccp_session_send(d, msg);
	} else {
		sccp_free(msg);
	}
	return result;
}

/*!
 * \brief Send an SCCP message to a device
 * \param d SCCP Device
 * \param t SCCP Message
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_sendmsg(const sccp_device_t * d, sccp_mid_t t)
{
	if (d) {
		sccp_session_sendmsg(d, t);
	}
}

/*!
 * \brief Register a Device
 * \param d SCCP Device
 * \param opt Option/Registration State as int
 *
 * \note adds a retained device to the event.deviceRegistered.device
 */
void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt)
{
	sccp_event_t event;
	sccp_msg_t *msg;

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (sccp_dev_set_registered) Setting Registered Status for Device from %s to %s\n", DEV_ID_LOG(d), skinny_registrationstate2str(d->registrationState), skinny_registrationstate2str(opt));

	if (d->registrationState == opt) {
		return;
	}
	d->registrationState = opt;

	/* Handle registration completion. */
	if (opt == SKINNY_DEVICE_RS_OK) {
		/* this message is mandatory to finish process */
		REQ(msg, SetLampMessage);

		if (msg) {
			msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
			msg->data.SetLampMessage.lel_stimulusInstance = 0;
			msg->data.SetLampMessage.lel_lampMode = (d->mwilight & ~(1 << 0)) ? htolel(d->mwilamp) : htolel(SKINNY_LAMP_OFF);
			// d->mwilight &= ~(1 << 0);
			sccp_dev_send(d, msg);
		}
		if (!d->linesRegistered) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device does not support RegisterAvailableLinesMessage, force this\n", DEV_ID_LOG(d));
			sccp_handle_AvailableLines(d->session, d, NULL);
		}

		sccp_dev_postregistration(d);
	} else if (opt == SKINNY_DEVICE_RS_PROGRESS) {
		memset(&event, 0, sizeof(sccp_event_t));
		event.type = SCCP_EVENT_DEVICE_PREREGISTERED;
		event.event.deviceRegistered.device = sccp_device_retain(d);
		sccp_event_fire(&event);
	}
	d->registrationTime = time(0);
}

/*!
 * \brief Sets the SCCP Device's SoftKey Mode Specified by opt
 * \param d SCCP Device
 * \param lineInstance LineInstance as uint8_t
 * \param callid Call ID as uint8_t
 * \param softKeySetIndex SoftKeySet Index
 * \todo Disable DirTrfr by Default
 */
void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t lineInstance, uint32_t callid, uint8_t softKeySetIndex)
{
	sccp_msg_t *msg;

	if (!d) {
		return;
	}
	if (!d->softkeysupport) {
		return;												/* the device does not support softkeys */
	}
	/* 69XX Exception SoftKeySet Mapping */
	if (d->skinny_type == SKINNY_DEVICETYPE_CISCO6901 || d->skinny_type == SKINNY_DEVICETYPE_CISCO6911 || d->skinny_type == SKINNY_DEVICETYPE_CISCO6921 || d->skinny_type == SKINNY_DEVICETYPE_CISCO6941 || d->skinny_type == SKINNY_DEVICETYPE_CISCO6945 || d->skinny_type == SKINNY_DEVICETYPE_CISCO6961) {

		/* 69XX does not like CONNCONF, so let's not set that and keep CONNECTED instead */
		/* while transfer in progress, they like OFFHOOKFEAT, after when we have connected to the destination we need to set CONNTRANS */
		/*! \todo Discuss if this behaviour should not be the general case for all devices */
		if (d->transfer && d->transferChannels.transferee) {
			/* first stage transfer */
			if (softKeySetIndex == KEYMODE_OFFHOOK && !d->transferChannels.transferer) {
				softKeySetIndex = KEYMODE_OFFHOOKFEAT;
			}
			/* second stage transfer (blind or not) */
			if ((softKeySetIndex == KEYMODE_RINGOUT || softKeySetIndex == KEYMODE_CONNECTED) && d->transferChannels.transferer) {
				softKeySetIndex = KEYMODE_CONNTRANS;
			}
		}
	} else {
		/*let's replace the CONNECTED with the transfer / conference states when allowed on this device */
		if (softKeySetIndex == KEYMODE_CONNECTED) {
			softKeySetIndex = (
#if CS_SCCP_CONFERENCE
						  (d->conference) ? KEYMODE_CONNCONF :
#endif
						  (d->transfer) ? KEYMODE_CONNTRANS : KEYMODE_CONNECTED);
		}
	}
	REQ(msg, SelectSoftKeysMessage);
	if (!msg) {
		return;
	}
	msg->data.SelectSoftKeysMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.SelectSoftKeysMessage.lel_callReference = htolel(callid);
	msg->data.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(softKeySetIndex);

	if (softKeySetIndex == KEYMODE_ONHOOK || softKeySetIndex == KEYMODE_OFFHOOK || softKeySetIndex == KEYMODE_OFFHOOKFEAT) {
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_REDIAL, (sccp_strlen_zero(d->redialInformation.number) && !d->useRedialMenu) ? FALSE : TRUE);
	}
#if CS_SCCP_CONFERENCE
	if (d->allow_conference) {
		if (d->conference) {
			sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_CONFRN, FALSE);
			sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_JOIN, TRUE);
		} else {
			sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_CONFRN, TRUE);
			sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_JOIN, FALSE);
		}
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_CONFLIST, TRUE);
	} else {
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_CONFRN, FALSE);
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_CONFLIST, FALSE);
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_JOIN, FALSE);
	}
#endif

	/* deactivate monitor softkey for all states excl. connected -MC */
	if (softKeySetIndex != KEYMODE_CONNTRANS && softKeySetIndex != KEYMODE_CONNECTED && softKeySetIndex != KEYMODE_EMPTY) {
		sccp_softkey_setSoftkeyState((sccp_device_t *) d, softKeySetIndex, SKINNY_LBL_MONITOR, FALSE);
	}
	//msg->data.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF;           /* htolel(65535); */
	msg->data.SelectSoftKeysMessage.les_validKeyMask = htolel(d->softKeyConfiguration.activeMask[softKeySetIndex]);

	sccp_log((DEBUGCAT_SOFTKEY + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Set softkeyset to %s(%d) on line %d  and call %d\n", d->id, skinny_keymode2str(softKeySetIndex), softKeySetIndex, lineInstance, callid);
	sccp_log((DEBUGCAT_SOFTKEY + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: validKeyMask %u\n", d->id, msg->data.SelectSoftKeysMessage.les_validKeyMask);
	sccp_dev_send(d, msg);
}

/*!
 * \brief Set Ringer on Device
 * \param d SCCP Device
 * \param opt Option as uint8_t
 * \param lineInstance LineInstance as uint32_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_set_ringer(const sccp_device_t * d, uint8_t opt, uint8_t lineInstance, uint32_t callid)
{
	sccp_msg_t *msg;

	REQ(msg, SetRingerMessage);
	if (!msg) {
		return;
	}
	msg->data.SetRingerMessage.lel_ringMode = htolel(opt);
	/* Note that for distinctive ringing to work with the higher protocol versions
 	   the following actually needs to be set to 1 as the original comment says.
	   Curiously, the variable is not set to 1 ... */
	msg->data.SetRingerMessage.lel_ringDuration = htolel(1);							/* Normal:1 / Single:2 */
	msg->data.SetRingerMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.SetRingerMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send ringer mode %s(%d) on device\n", DEV_ID_LOG(d), skinny_ringtype2str(opt), opt);
}

/*!
 * \brief Set Speaker Status on Device
 * \param d SCCP Device
 * \param mode Speaker Mode as uint8_t
 */
void sccp_dev_set_speaker(const sccp_device_t * d, uint8_t mode)
{
	sccp_msg_t *msg;

	if (!d || !d->session) {
		return;
	}
	REQ(msg, SetSpeakerModeMessage);
	if (!msg) {
		return;
	}
	msg->data.SetSpeakerModeMessage.lel_speakerMode = htolel(mode);
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send speaker mode %d\n", d->id, mode);
}

/*!
 * \brief Set Microphone Status on Device
 * \param d SCCP Device
 * \param mode Microphone Mode as uint8_t
 */
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t mode)
{
	sccp_msg_t *msg;

	if (!d || !d->session) {
		return;
	}
	REQ(msg, SetMicroModeMessage);
	if (!msg) {
		return;
	}
	msg->data.SetMicroModeMessage.lel_micMode = htolel(mode);
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send microphone mode %d\n", d->id, mode);
}

/*!
 * \brief Set Call Plane to Active on  Line on Device
 * \param device SCCP Device
 * \param lineInstance lineInstance as unint8_t
 * \param status Status as int
 * \todo What does this function do exactly (ActivateCallPlaneMessage) ?
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_set_cplane(const sccp_device_t * device, uint8_t lineInstance, int status)
{
	sccp_msg_t *msg;

	if (!device) {
		return;
	}
	REQ(msg, ActivateCallPlaneMessage);
	if (!msg) {
		return;
	}
	if (status) {
		msg->data.ActivateCallPlaneMessage.lel_lineInstance = htolel(lineInstance);
	}
	sccp_dev_send(device, msg);

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send activate call plane on line %d\n", device->id, (status) ? lineInstance : 0);
}

/*!
 * \brief Set Call Plane to In-Active on  Line on Device
 * \param d device
 * \todo What does this function do exactly (DeactivateCallPlaneMessage) ?
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_deactivate_cplane(sccp_device_t * d)
{
	if (!d) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Null device for deactivate callplane\n");
		return;
	}

	sccp_dev_sendmsg(d, DeactivateCallPlaneMessage);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send deactivate call plane\n", d->id);
}

/*!
 * \brief Send Start Tone to Device
 * \param d SCCP Device
 * \param tone Tone as uint8_t
 * \param line Line as uint8_t
 * \param callid Call ID as uint32_t
 * \param timeout Timeout as uint32_t
 */
void sccp_dev_starttone(const sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout)
{
	sccp_msg_t *msg;

	if (!d) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Null device for device starttone\n");
		return;
	}

	REQ(msg, StartToneMessage);
	if (!msg) {
		return;
	}
	msg->data.StartToneMessage.lel_tone = htolel(tone);
	msg->data.StartToneMessage.lel_toneTimeout = htolel(timeout);
	msg->data.StartToneMessage.lel_lineInstance = htolel(line);
	msg->data.StartToneMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending tone %s (%d) on line %d with callid %d\n", d->id, skinny_tone2str(tone), tone, line, callid);
}

/*!
 * \brief Send Stop Tone to Device
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_stoptone(const sccp_device_t * d, uint8_t line, uint32_t callid)
{
	sccp_msg_t *msg;

	if (!d || !d->session) {
		return;
	}
	REQ(msg, StopToneMessage);
	if (!msg) {
		return;
	}
	msg->data.StopToneMessage.lel_lineInstance = htolel(line);
	msg->data.StopToneMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Stop tone on line %d with callid %d\n", d->id, line, callid);
}

/*!
 * \brief Set Message on Display Prompt of Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param timeout Timeout as int
 * \param storedb Store in the pbx database
 * \param beep Beep on device when message is received
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_set_message(sccp_device_t * d, const char *msg, const int timeout, const boolean_t storedb, const boolean_t beep)
{
	if (storedb) {
		char msgtimeout[10];

		sprintf(msgtimeout, "%d", timeout);
		PBX(feature_addToDatabase) ("SCCP/message", "timeout", strdup(msgtimeout));
		PBX(feature_addToDatabase) ("SCCP/message", "text", msg);
	}

	if (timeout) {
		sccp_dev_displayprinotify(d, msg, 5, timeout);
	} else {
		sccp_device_addMessageToStack(d, SCCP_MESSAGE_PRIORITY_IDLE, msg);
	}
	if (beep) {
		sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, 0, 0, 0);
	}
}

/*!
 * \brief Clear Message from Display Prompt of Device
 * \param d SCCP Device
 * \param cleardb Clear from the pbx database
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_clear_message(sccp_device_t * d, const boolean_t cleardb)
{
	if (cleardb) {
		PBX(feature_removeTreeFromDatabase) ("SCCP/message", "timeout");
		PBX(feature_removeTreeFromDatabase) ("SCCP/message", "text");
	}

	sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_IDLE);
	// sccp_dev_clearprompt(d, 0, 0);
	sccp_dev_cleardisplay(d);
}

/*!
 * \brief Send Clear Prompt to Device
 * \param d SCCP Device
 * \param lineInstance LineInstance as uint8_t
 * \param callid Call ID uint32_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_clearprompt(const sccp_device_t * d, const uint8_t lineInstance, const uint32_t callid)
{
	sccp_msg_t *msg;

	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;												/* only for telecaster and new phones */
	}
	REQ(msg, ClearPromptStatusMessage);
	if (!msg) {
		return;
	}
	msg->data.ClearPromptStatusMessage.lel_callReference = htolel(callid);
	msg->data.ClearPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Clear the status prompt on line %d and callid %d\n", d->id, lineInstance, callid);
}

/*!
 * \brief Send Display Prompt to Device
 * \param d SCCP Device
 * \param lineInstance Line instance as uint8_t
 * \param callid Call ID uint32_t
 * \param msg Msg as char
 * \param timeout Timeout as int
 * \param file Source File
 * \param lineno Source Line
 * \param pretty_function CB Function to Print
 *
 * \callgraph
 * \callergraph
 */
//void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char *msg, int timeout)
void sccp_dev_displayprompt_debug(const sccp_device_t * d, const uint8_t lineInstance, const uint32_t callid, const char *msg, const int timeout, const char *file, int lineno, const char *pretty_function)
{
#if DEBUG
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_displayprompt '%s' for line %d (%d)\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg, lineInstance, timeout);
#endif
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	d->protocol->displayPrompt(d, lineInstance, callid, timeout, msg);
}

/*!
 * \brief Send Clear Display to Device
 * \param d SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_cleardisplay(const sccp_device_t * d)
{
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	sccp_dev_sendmsg(d, ClearDisplay);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Clear the display\n", d->id);
}

/*!
 * \brief Send Display to Device
 * \param d SCCP Device
 * \param msgstr Msg as char
 * \param file Source File
 * \param lineno Source Line
 * \param pretty_function CB Function to Print
 *
 * \callgraph
 * \callergraph
 */
//void sccp_dev_display(sccp_device_t * d, char *msg)
void sccp_dev_display_debug(const sccp_device_t * d, const char *msgstr, const char *file, const int lineno, const char *pretty_function)
{
#if DEBUG
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_display '%s'\n", DEV_ID_LOG(d), file, lineno, pretty_function, msgstr);
#endif
	sccp_msg_t *msg;

	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	if (!msgstr || sccp_strlen_zero(msgstr)) {
		return;
	}
	REQ(msg, DisplayTextMessage);
	if (!msg) {
		return;
	}
	sccp_copy_string(msg->data.DisplayTextMessage.displayMessage, msgstr, sizeof(msg->data.DisplayTextMessage.displayMessage));

	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Display text\n", d->id);
}

/*!
 * \brief Send Clear Display Notification to Device
 * \param d SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_cleardisplaynotify(const sccp_device_t * d)
{
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;												/* only for telecaster and new phones */
	}
	sccp_dev_sendmsg(d, ClearNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Clear the display notify message\n", d->id);
}

/*!
 * \brief Send Display Notification to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param timeout Timeout as uint8_t
 * \param file Source File
 * \param lineno Source Line
 * \param pretty_function CB Function to Print
 *
 * \callgraph
 * \callergraph
 */
//void sccp_dev_displaynotify(sccp_device_t * d, char *msg, uint32_t timeout)
void sccp_dev_displaynotify_debug(const sccp_device_t * d, const char *msg, uint8_t timeout, const char *file, const int lineno, const char *pretty_function)
{
	// #if DEBUG
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_displaynotify '%s' (%d)\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg, timeout);
	// #endif
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	if (!msg || sccp_strlen_zero(msg)) {
		return;
	}
	d->protocol->displayNotify(d, timeout, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Display notify with timeout %d\n", d->id, timeout);
}

/*!
 * \brief Send Clear Display Notification to Device
 * \param d SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_cleardisplayprinotify(const sccp_device_t * d, const uint8_t priority)
{
	sccp_msg_t *msg;
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	REQ(msg, ClearPriNotifyMessage);
	msg->data.ClearPriNotifyMessage.lel_priority = htolel(priority);

	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Clear the display priority notify message\n", d->id);
}

/*!
 * \brief Send Display Priority Notification to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param priority Priority as uint8_t
 * \param timeout Timeout as uint8_t
 * \param file Source File
 * \param lineno Source Line
 * \param pretty_function CB Function to Print
 *
 * \callgraph
 * \callergraph
 */
//void sccp_dev_displayprinotify(sccp_device_t * d, char *msg, uint32_t priority, uint32_t timeout)
void sccp_dev_displayprinotify_debug(const sccp_device_t * d, const char *msg, const uint8_t priority, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function)
{
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	if (!msg || sccp_strlen_zero(msg)) {
		sccp_dev_cleardisplayprinotify(d, priority);
		return;
	}
	d->protocol->displayPriNotify(d, priority, timeout, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Display notify with timeout %d and priority %d\n", d->id, timeout, priority);
}

/*!
 * \brief Find SpeedDial by Index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \param withHint With Hint as boolean_t
 * \param k SCCP Speeddial (Returned by Ref)
 * \return Void
 *
 */
void sccp_dev_speed_find_byindex(sccp_device_t * d, uint16_t instance, boolean_t withHint, sccp_speed_t * k)
{
	sccp_buttonconfig_t *config;

	if (!d || !d->session || instance == 0) {
		return;
	}
	memset(k, 0, sizeof(sccp_speed_t));
	sccp_copy_string(k->name, "unknown speeddial", sizeof(k->name));

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {

		if (config->type == SPEEDDIAL && config->instance == instance) {

			/* we are searching for hinted speeddials */
			if (TRUE == withHint && !sccp_strlen_zero(config->button.speeddial.hint)) {
				k->valid = TRUE;
				k->instance = instance;
				k->type = SCCP_BUTTONTYPE_SPEEDDIAL;
				sccp_copy_string(k->name, config->label, sizeof(k->name));
				sccp_copy_string(k->ext, config->button.speeddial.ext, sizeof(k->ext));
				sccp_copy_string(k->hint, config->button.speeddial.hint, sizeof(k->hint));
				
			} else if(FALSE == withHint && sccp_strlen_zero(config->button.speeddial.hint)) {
				k->valid = TRUE;
				k->instance = instance;
				k->type = SCCP_BUTTONTYPE_SPEEDDIAL;
				sccp_copy_string(k->name, config->label, sizeof(k->name));
				sccp_copy_string(k->ext, config->button.speeddial.ext, sizeof(k->ext));
			}
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
}

/*!
 * \brief Send Get Activeline to Device
 * \param d SCCP Device
 * \return Retained SCCP Line
 *
 * \warning
 *   - device->buttonconfig is not locked
 * \return_ref d->currentLine
 */
sccp_line_t *sccp_dev_get_activeline(const sccp_device_t * d)
{
	sccp_buttonconfig_t *buttonconfig;

	if (!d || !d->session) {
		return NULL;
	}
	if (d->currentLine) {
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: The active line is %s\n", d->id, d->currentLine->name);
		return sccp_line_retain(d->currentLine);
	}
	// else try to set an new currentLine
	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
		if (buttonconfig->type == LINE) {
			sccp_device_t *device = (sccp_device_t *) d;						// need non-const device

			if ((device->currentLine = sccp_line_find_byname(buttonconfig->button.line.name, FALSE))) {	// update d->currentLine, returns retained line
				sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Forcing the active line to %s from NULL\n", d->id, d->currentLine->name);
				return sccp_line_retain(device->currentLine);					// returning retained
			}
		}
	}

	// failed to find or set a currentLine
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No lines\n", d->id);
	return NULL;												// never reached
}

/*!
 * \brief Set Activeline to Device
 * \param device SCCP Device
 * \param l SCCP Line
 */
void sccp_dev_set_activeline(sccp_device_t * device, const sccp_line_t * l)
{
	if (!device || !device->session) {
		return;
	}
	sccp_line_refreplace(device->currentLine, (sccp_line_t *) l);

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Set the active line %s\n", device->id, l ? l->name : "(NULL)");
}

/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t *sccp_device_getActiveChannel(const sccp_device_t * d)
{
	sccp_channel_t *channel;

	if (!d || !d->active_channel) {
		return NULL;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Getting the active channel on device.\n", d->id);

	if (!(channel = sccp_channel_retain(d->active_channel))) {
		return NULL;
	}

	if (channel->state == SCCP_CHANNELSTATE_DOWN) {
		channel = sccp_channel_release(channel);
		return NULL;
	}

	return channel;
}

/*!
 * \brief Set SCCP Channel to Active
 * \param d SCCP Device
 * \param channel SCCP Channel
 */
void sccp_device_setActiveChannel(sccp_device_t * d, sccp_channel_t * channel)
{
	AUTO_RELEASE sccp_device_t *device = sccp_device_retain(d);

	if (device) {
		sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (channel) ? channel->callid : 0);
		if (device->active_channel && device->active_channel->line) {
			device->active_channel->line->statistic.numberOfActiveChannels--;
		}
		if (!channel) {
			sccp_dev_set_activeline(device, NULL);
		}
		sccp_channel_refreplace(device->active_channel, channel);
		if (device->active_channel) {
			sccp_channel_updateChannelDesignator(device->active_channel);
			sccp_dev_set_activeline(device, device->active_channel->line);
			if (device->active_channel->line) {
				device->active_channel->line->statistic.numberOfActiveChannels++;
			}
		}
	}
}

/*!
 * \brief Reschedule Display Prompt Check
 * \param d SCCP Device
 *
 * \todo We have to decide on a standardized implementation of displayprompt to be used
 *       For DND/Cfwd/Message/Voicemail/Private Status for Devices and Individual Lines
 *       If necessary devicetypes could be deviced into 3-4 groups depending on their capability for displaying status the best way
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_check_displayprompt(const sccp_device_t * d)
{
	//sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_1 "%s: (sccp_dev_check_displayprompt)\n", DEV_ID_LOG(d));
	if (!d || !d->session || !d->protocol || !d->hasDisplayPrompt()) {
		return;
	}
	boolean_t message_set = FALSE;
	int i;

	sccp_dev_clearprompt(d, 0, 0);
#ifndef SCCP_ATOMIC
	sccp_device_t *device = (sccp_device_t *) d;								/* discard const */

	sccp_mutex_lock(&device->messageStack.lock);
#endif
	for (i = SCCP_MAX_MESSAGESTACK - 1; i >= 0; i--) {
		if (d->messageStack.messages[i] != NULL && !sccp_strlen_zero(d->messageStack.messages[i])) {
			sccp_dev_displayprompt(d, 0, 0, d->messageStack.messages[i], 0);
			message_set = TRUE;
			break;
		}
	}
#ifndef SCCP_ATOMIC
	sccp_mutex_unlock(&device->messageStack.lock);
#endif
	if (!message_set) {
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOUR_CURRENT_OPTIONS, 0);
		sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK);							/* this is for redial softkey */
	}
	sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Finish DisplayPrompt\n", d->id);
}

/*!
 * \brief Send forward status to a line on a device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param device SCCP Device
 *
 * \todo integration this function correctly into check sccp_dev_check_displayprompt
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_forward_status(sccp_line_t * l, uint8_t lineInstance, sccp_device_t * device)
{
#ifndef ASTDB_FAMILY_KEY_LEN
#define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#define ASTDB_RESULT_LEN 80
#endif
	sccp_linedevices_t *linedevice = NULL;

	if (!l || !device || !device->session) {
		return;
	}
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send Forward Status.  Line: %s\n", device->id, l->name);

	//! \todo check for forward status during registration -MC
	//! \todo Needs to be revised. Does not make sense to call sccp_handle_AvailableLines from here
	if (device->registrationState != SKINNY_DEVICE_RS_OK) {
		if (!device->linesRegistered) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device does not support RegisterAvailableLinesMessage, forcing this\n", DEV_ID_LOG(device));
			sccp_handle_AvailableLines(device->session, device, NULL);
			device->linesRegistered = TRUE;
		}
	}

	if ((linedevice = sccp_linedevice_find(device, l))) {
		device->protocol->sendCallforwardMessage(device, linedevice);
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Sent Forward Status (%s). Line: %s (%d)\n", device->id, (linedevice->cfwdAll.enabled ? "All" : (linedevice->cfwdBusy.enabled ? "Busy" : "None")), l->name, linedevice->lineInstance);
		sccp_linedevice_release(linedevice);
	} else {
		pbx_log(LOG_NOTICE, "%s: Device does not have line configured (no linedevice found)\n", DEV_ID_LOG(device));
	}
}

/*!
 * \brief Check Ringback on Device
 * \param device SCCP Device
 * \return Result as int
 */
int sccp_device_check_ringback(sccp_device_t * device)
{
	AUTO_RELEASE sccp_channel_t *c;
	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (!d) {
		return 0;
	}
	d->needcheckringback = 0;
	if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		return 0;
	}
	c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLTRANSFER);
	if (!c) {
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_RINGING);
	}
	if (!c) {
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLWAITING);
	}
	if (c) {
		sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGING);
		return 1;
	}
	return 0;
}

/*!
 * \brief Handle Post Device Registration
 * \param data Data
 *
 * \callgraph
 * \callergraph
 *
 * \note adds a retained device to the event.deviceRegistered.device
 */
void sccp_dev_postregistration(void *data)
{
	sccp_device_t *d = data;
	sccp_event_t event = { 0 };

#ifndef ASTDB_FAMILY_KEY_LEN
#define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#define ASTDB_RESULT_LEN 80
#endif
	char family[ASTDB_FAMILY_KEY_LEN] = { 0 };
	char buffer[ASTDB_RESULT_LEN] = { 0 };
	int instance;

	if (!d) {
		return;
	}
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device registered; performing post registration tasks...\n", d->id);

	// Post event to interested listeners (hints, mwi) that device was registered.
	event.type = SCCP_EVENT_DEVICE_REGISTERED;
	event.event.deviceRegistered.device = sccp_device_retain(d);
	sccp_event_fire(&event);

	/* read last line/device states from db */
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Getting Database Settings...\n", d->id);
	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE sccp_linedevices_t *linedevice = sccp_linedevice_retain(d->lineButtons.instance[instance]);

			sprintf(family, "SCCP/%s/%s", d->id, linedevice->line->name);
			if (PBX(feature_getFromDatabase) (family, "cfwdAll", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
				linedevice->cfwdAll.enabled = TRUE;
				sccp_copy_string(linedevice->cfwdAll.number, buffer, sizeof(linedevice->cfwdAll.number));
				sccp_feat_changed(d, linedevice, SCCP_FEATURE_CFWDALL);
			}
			if (PBX(feature_getFromDatabase) (family, "cfwdBusy", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
				linedevice->cfwdBusy.enabled = TRUE;
				sccp_copy_string(linedevice->cfwdBusy.number, buffer, sizeof(linedevice->cfwdAll.number));
				sccp_feat_changed(d, linedevice, SCCP_FEATURE_CFWDBUSY);
			}
			/* combine device capabilities and add to (shared) line */
			sccp_line_copyCapabilitiesFromDeviceToLine(linedevice->line, linedevice->device, SCCP_LIST_GETSIZE(&linedevice->line->devices));
		}
	}
	sprintf(family, "SCCP/%s", d->id);
	if (PBX(feature_getFromDatabase) (family, "dnd", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
		d->dndFeature.status = sccp_dndmode_str2val(buffer);
		sccp_feat_changed(d, NULL, SCCP_FEATURE_DND);
	}

	if (PBX(feature_getFromDatabase) (family, "privacy", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
		d->privacyFeature.status = TRUE;
		sccp_feat_changed(d, NULL, SCCP_FEATURE_PRIVACY);
	}

	if (PBX(feature_getFromDatabase) (family, "monitor", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
		sccp_feat_monitor(d, NULL, 0, NULL);
		sccp_feat_changed(d, NULL, SCCP_FEATURE_MONITOR);
	}

	char lastNumber[SCCP_MAX_EXTENSION] = "";
	if (PBX(feature_getFromDatabase) (family, "lastDialedNumber", buffer, sizeof(buffer))) {
		sscanf(buffer,"%80[^;];lineInstance=%d", lastNumber, &instance);
		AUTO_RELEASE sccp_linedevices_t *linedevice = sccp_linedevice_findByLineinstance(d, instance);
		if(linedevice){ 
			sccp_device_setLastNumberDialed(d, lastNumber, linedevice);
		}
	}

	if (d->backgroundImage) {
		d->setBackgroundImage(d, d->backgroundImage);
	}

	if (d->ringtone) {
		d->setRingTone(d, d->ringtone);
	}

	if (d->useRedialMenu && (!d->hasDisplayPrompt)) {
		pbx_log(LOG_NOTICE, "%s: useRedialMenu is currently not supported on this devicetype. Reverting to old style redial\n", d->id);
		d->useRedialMenu = FALSE;
	}

	sccp_dev_check_displayprompt(d);

	sccp_mwi_check(d);

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Post registration process... done!\n", d->id);
	return;
}

/*!
 * \brief Clean Device
 *
 *  clean up memory allocated by the device.
 *  if destroy is true, device will be removed from global device list
 *
 * \param device SCCP Device
 * \param remove_from_global as boolean_t
 * \param cleanupTime Clean-up Time as uint8
 *
 * \callgraph
 * \callergraph
 *
 * \note adds a retained device to the event.deviceRegistered.device
 */
void sccp_dev_clean(sccp_device_t * device, boolean_t remove_from_global, uint8_t cleanupTime)
{
	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);
	sccp_buttonconfig_t *config = NULL;
	sccp_selectedchannel_t *selectedChannel = NULL;
	sccp_channel_t *c = NULL;
	sccp_event_t event = { 0 };
	int i = 0;

#if defined(CS_DEVSTATE_FEATURE) && defined(CS_AST_HAS_EVENT)
	sccp_devstate_specifier_t *devstateSpecifier;
#endif
	char family[25];

	if (d) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "SCCP: Clean Device %s\n", d->id);
		sccp_dev_set_registered(d, SKINNY_DEVICE_RS_NONE);						/* set correct register state */
		if (remove_from_global) {
			sccp_device_removeFromGlobals(d);
		}

		d->mwilight = 0;										/* reset mwi light */
		d->linesRegistered = FALSE;
		sprintf(family, "SCCP/%s", d->id);
		PBX(feature_removeFromDatabase) (family, "lastDialedNumber");
		char buffer[SCCP_MAX_EXTENSION+16] = "\0";
		if (!sccp_strlen_zero(d->redialInformation.number)) {
			sprintf (buffer, "%s;lineInstance=%d", d->redialInformation.number, d->redialInformation.lineInstance);
			PBX(feature_addToDatabase) (family, "lastDialedNumber", buffer);
		}

		/* hang up open channels and remove device from line */
		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&d->buttonconfig, config, list) {
			if (config->type == LINE) {
				AUTO_RELEASE sccp_line_t *line = sccp_line_find_byname(config->button.line.name, FALSE);

				if (!line) {
					continue;
				}
				SCCP_LIST_LOCK(&line->channels);
				SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->channels, c, list) {
					AUTO_RELEASE sccp_channel_t *channel = sccp_channel_retain(c);

					if (c) {
						AUTO_RELEASE sccp_device_t *tmpDevice = sccp_channel_getDevice_retained(channel);

						if (tmpDevice == d) {
							sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channel on line %s device %s\n", line->name, d->id);
							sccp_channel_endcall(channel);
						}
					}
				}
				SCCP_LIST_TRAVERSE_SAFE_END;
				SCCP_LIST_UNLOCK(&line->channels);

				/* remove devices from line */
				sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Remove Line %s from device %s\n", line->name, d->id);
				sccp_line_removeDevice(line, d);
			}
			config->instance = 0;									/* reset button configuration to rebuild template on register */
			if (config->pendingDelete) {
				SCCP_LIST_REMOVE_CURRENT(list);
				sccp_free(config);
				config = NULL;

			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
		SCCP_LIST_UNLOCK(&d->buttonconfig);
		d->linesRegistered = FALSE;

		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Unregister Device %s\n", d->id);

		memset(&event, 0, sizeof(sccp_event_t));
		event.type = SCCP_EVENT_DEVICE_UNREGISTERED;
		event.event.deviceRegistered.device = sccp_device_retain(d);
		sccp_event_fire(&event);

		/* cleanup statistics */
		memset(&d->configurationStatistic, 0, sizeof(d->configurationStatistic));

		d->mwilight = 0;										/* cleanup mwi status */
		d->status.token = SCCP_TOKEN_STATE_NOTOKEN;
		d->registrationTime = time(0);

		/* removing addons */
		if (remove_from_global) {
			sccp_addons_clear(d);
		}

		/* removing selected channels */
		SCCP_LIST_LOCK(&d->selectedChannels);
		while ((selectedChannel = SCCP_LIST_REMOVE_HEAD(&d->selectedChannels, list))) {
			sccp_free(selectedChannel);
		}
		SCCP_LIST_UNLOCK(&d->selectedChannels);

		if (d->session && d->session->device) {
			sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
			usleep(20);
			if (d->session) {
				sccp_device_t *previous_device = sccp_session_removeDevice(d->session);

				previous_device = previous_device ? sccp_device_release(previous_device) : NULL;
			}
			d->session = NULL;
		}

		/* release line references, refcounted in btnList */
		if (d->buttonTemplate) {
			btnlist *btn = d->buttonTemplate;

			for (i = 0; i < StationMaxButtonTemplateSize; i++) {
				if ((btn[i].type == SKINNY_BUTTONTYPE_LINE) && btn[i].ptr) {
					btn[i].ptr = sccp_line_release(btn[i].ptr);
				}
			}
			sccp_free(d->buttonTemplate);
			d->buttonTemplate = NULL;
		}

		sccp_line_deleteLineButtonsArray(d);

#if defined(CS_DEVSTATE_FEATURE) && defined(CS_AST_HAS_EVENT)
		/* Unregister event subscriptions originating from devstate feature */
		SCCP_LIST_LOCK(&d->devstateSpecifiers);
		while ((devstateSpecifier = SCCP_LIST_REMOVE_HEAD(&d->devstateSpecifiers, list))) {
			if (devstateSpecifier->sub) {
				pbx_event_unsubscribe(devstateSpecifier->sub);
			}
			sccp_log((DEBUGCAT_FEATURE_BUTTON)) (VERBOSE_PREFIX_1 "%s: Removed Devicestate Subscription: %s\n", d->id, devstateSpecifier->specifier);
		}
		SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
#endif
	}
}

/*!
 * \brief Free a Device as scheduled command
 * \param ptr SCCP Device Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 *
 */
int __sccp_device_destroy(const void *ptr)
{
	sccp_device_t *d = (sccp_device_t *) ptr;
	sccp_buttonconfig_t *config = NULL;
	sccp_hostname_t *permithost = NULL;
	int i;

	if (!d) {
		pbx_log(LOG_ERROR, "SCCP: Trying to destroy non-existend device\n");
		return -1;
	}

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Destroying Device\n", d->id);

	/* cleanup dynamic allocated during sccp_config (i.e. STRINGPTR) */
	sccp_config_cleanup_dynamically_allocated_memory(d, SCCP_CONFIG_DEVICE_SEGMENT);

	/* remove button config */
	/* only generated on read config, so do not remove on reset/restart */
	SCCP_LIST_LOCK(&d->buttonconfig);
	while ((config = SCCP_LIST_REMOVE_HEAD(&d->buttonconfig, list))) {
		sccp_free(config);
		config = NULL;
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	SCCP_LIST_HEAD_DESTROY(&d->buttonconfig);

	/* removing permithosts */
	SCCP_LIST_LOCK(&d->permithosts);
	while ((permithost = SCCP_LIST_REMOVE_HEAD(&d->permithosts, list))) {
		if (permithost) {
			sccp_free(permithost);
		}
	}
	SCCP_LIST_UNLOCK(&d->permithosts);
	SCCP_LIST_HEAD_DESTROY(&d->permithosts);

#ifdef CS_DEVSTATE_FEATURE
	/* removing devstate_specifier */
	sccp_devstate_specifier_t *devstateSpecifier;

	SCCP_LIST_LOCK(&d->devstateSpecifiers);
	while ((devstateSpecifier = SCCP_LIST_REMOVE_HEAD(&d->devstateSpecifiers, list))) {
		if (devstateSpecifier) {
			sccp_free(devstateSpecifier);
		}
	}
	SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
	SCCP_LIST_HEAD_DESTROY(&d->devstateSpecifiers);
#endif

	/* destroy selected channels list */
	SCCP_LIST_HEAD_DESTROY(&d->selectedChannels);

	if (d->ha) {
		sccp_free_ha(d->ha);
		d->ha = NULL;
	}

	/* cleanup message stack */
#ifndef SCCP_ATOMIC
	sccp_mutex_lock(&d->messageStack.lock);
#endif
	for (i = 0; i < SCCP_MAX_MESSAGESTACK; i++) {
		if (d->messageStack.messages[i] != NULL) {
			sccp_free(d->messageStack.messages[i]);
		}
	}
#ifndef SCCP_ATOMIC
	sccp_mutex_unlock(&d->messageStack.lock);
	pbx_mutex_destroy(&d->messageStack.lock);
#endif

	if (d->variables) {
		pbx_variables_destroy(d->variables);
		d->variables = NULL;
	}
	/*
	if (PBX(endpoint_shutdown)) {
		PBX(endpoint_shutdown)(d->endpoint);
	}
	*/

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device Destroyed\n", d->id);
	return 0;
}

/*!
 * \brief Free a Device as scheduled command
 * \param ptr SCCP Device Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 *
 * \called_from_asterisk
 *
 */
int sccp_device_destroy(const void *ptr)
{
	sccp_device_t *d = (sccp_device_t *) ptr;

	sccp_device_removeFromGlobals(d);
	return 0;
}

/*!
 * \brief is Video Support on a Device
 * \param device SCCP Device
 * \return result as boolean_t
 */
boolean_t sccp_device_isVideoSupported(const sccp_device_t * device)
{

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: video support %d \n", device->id, device->capabilities.video[0]);
#ifdef CS_SCCP_VIDEO
	if (device->capabilities.video[0] != 0) {
		return TRUE;
	}
#endif
	return FALSE;
}

/*!
 * \brief Find ServiceURL by index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \return SCCP Service
 *
 */
sccp_buttonconfig_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance)
{
	sccp_buttonconfig_t *config = NULL;

	if (!d || !d->session) {
		return NULL;
	}
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for service with instance %d\n", d->id, instance);
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log_and((DEBUGCAT_DEVICE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: instance: %d buttontype: %d\n", d->id, config->instance, config->type);

		if (config->type == SERVICE && config->instance == instance) {
			sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: found service: %s\n", d->id, config->label);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	return config;
}

/*!
 * \brief Find Device by Line Index
 * \param d SCCP Device
 * \param lineName Line Name as char
 * \return Status as int
 * \note device should be locked by parent fuction
 *
 * \warning
 *   - device->buttonconfig is not always locked
 */
uint8_t sccp_device_find_index_for_line(const sccp_device_t * d, const char *lineName)
{
	uint8_t instance;

	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance] && d->lineButtons.instance[instance]->line && !strcasecmp(d->lineButtons.instance[instance]->line->name, lineName)) {
			return instance;
		}
	}
	return 0;
}

/*!
 * \brief Send Reset to a Device
 * \param d SCCP Device
 * \param reset_type as int
 * \return Status as int
 */
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type)
{
	sccp_msg_t *msg;

	if (!d) {
		return 0;
	}

	REQ(msg, Reset);
	if (!msg) {
		return 0;
	}

	msg->data.Reset.lel_resetType = htolel(reset_type);
	sccp_session_send(d, msg);
	d->pendingUpdate = 0;
	return 1;
}

/*!
 * \brief Send Call State to Device
 * \param d SCCP Device
 * \param instance Instance as int
 * \param callid Call ID as int
 * \param state Call State as int
 * \param priority Priority as skinny_callpriority_t
 * \param visibility Visibility as skinny_callinfo_visibility_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, skinny_callstate_t state, skinny_callpriority_t precedence_level, skinny_callinfo_visibility_t visibility)
{
	sccp_msg_t *msg;

	if (!d) {
		return;
	}
	REQ(msg, CallStateMessage);
	if (!msg) {
		return;
	}
	msg->data.CallStateMessage.lel_callState = htolel(state);
	msg->data.CallStateMessage.lel_lineInstance = htolel(instance);
	msg->data.CallStateMessage.lel_callReference = htolel(callid);
	msg->data.CallStateMessage.lel_visibility = htolel(visibility);
	msg->data.CallStateMessage.precedence.lel_level = htolel(precedence_level);
	/*msg->data.CallStateMessage.precedency.lel_domain = htolel(2); */
	sccp_dev_send(d, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send and Set the call state %s(%d) on call %d\n", d->id, skinny_callstate2str(state), state, callid);
}

/*!
 * \brief Get the number of channels that the device owns
 * \param device sccp device
 * \note device should be locked by parent functions
 *
 * \warning
 *   - device-buttonconfig is not always locked
 */
uint8_t sccp_device_numberOfChannels(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	sccp_channel_t *c;
	uint8_t numberOfChannels = 0;

	if (!device) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "device is null\n");
		return 0;
	}

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == LINE) {
			AUTO_RELEASE sccp_line_t *l = sccp_line_find_byname(config->button.line.name, FALSE);

			if (!l) {
				continue;
			}
			SCCP_LIST_LOCK(&l->channels);
			SCCP_LIST_TRAVERSE(&l->channels, c, list) {
				AUTO_RELEASE sccp_device_t *tmpDevice = sccp_channel_getDevice_retained(c);

				if (tmpDevice == device) {
					numberOfChannels++;
				}
			}
			SCCP_LIST_UNLOCK(&l->channels);
		}
	}

	return numberOfChannels;
}

/*!
 * \brief Send DTMF Tone as KeyPadButton to SCCP Device
 */
void sccp_dev_keypadbutton(sccp_device_t * d, char digit, uint8_t line, uint32_t callid)
{
	sccp_msg_t *msg;

	if (!d || !d->session) {
		return;
	}
	if (digit == '*') {
		digit = 0xe;											/* See the definition of tone_list in chan_protocol.h for more info */
	} else if (digit == '#') {
		digit = 0xf;
	} else if (digit == '0') {
		digit = 0xa;											/* 0 is not 0 for cisco :-) */
	} else {
		digit -= '0';
	}

	if (digit > 16) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: SCCP phones can't play this type of dtmf. Sending it inband\n", d->id);
		return;
	}

	REQ(msg, KeypadButtonMessage);
	if (!msg) {
		return;
	}
	msg->data.KeypadButtonMessage.lel_kpButton = htolel(digit);
	msg->data.KeypadButtonMessage.lel_lineInstance = htolel(line);
	msg->data.KeypadButtonMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, msg);

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (sccp_dev_keypadbutton) Sending keypad '%02X'\n", DEV_ID_LOG(d), digit);
}

/*!
 * \brief Indicate to device that remote side has been put on hold (old).
 */
static void sccp_device_old_indicate_remoteHold(const sccp_device_t * device, uint8_t lineInstance, uint32_t callid, uint8_t callpriority, uint8_t callPrivacy)
{
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_HOLD, callpriority, callPrivacy);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOLD);
	sccp_dev_displayprompt(device, lineInstance, callid, SKINNY_DISP_HOLD, GLOB(digittimeout));
}

/*!
 * \brief Indicate to device that remote side has been put on hold (new).
 */
static void sccp_device_new_indicate_remoteHold(const sccp_device_t * device, uint8_t lineInstance, uint32_t callid, uint8_t callpriority, uint8_t callPrivacy)
{
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_HOLDRED, callpriority, callPrivacy);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOLD);
	sccp_dev_displayprompt(device, lineInstance, callid, SKINNY_DISP_HOLD, GLOB(digittimeout));
}

static void sccp_device_indicate_offhook(const sccp_device_t * device, sccp_linedevices_t * linedevice, uint32_t callid)
{

	sccp_dev_set_speaker(device, SKINNY_STATIONSPEAKER_ON);
	sccp_device_sendcallstate(device, linedevice->lineInstance, callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_dev_set_cplane(device, linedevice->lineInstance, 1);
	sccp_dev_displayprompt(device, linedevice->lineInstance, callid, SKINNY_DISP_ENTER_NUMBER, GLOB(digittimeout));
	sccp_dev_set_keyset(device, linedevice->lineInstance, callid, KEYMODE_OFFHOOK);
	sccp_dev_starttone(device, SKINNY_TONE_INSIDEDIALTONE, linedevice->lineInstance, callid, 0);
}

#if 0														/* new impl: does not seem to work (Reported by Sharan and Antonio: 01-07-2014), needs more work */
static void sccp_device_indicate_onhook(const sccp_device_t * device, const sccp_channel_t * channel, const uint8_t lineInstance, uint32_t callid)
{
	sccp_dev_stoptone(device, lineInstance, callid);
	sccp_device_setLamp(device, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_OFF);

	sccp_dev_cleardisplaynotify(device);
	sccp_dev_clearprompt(device, lineInstance, callid);
	sccp_dev_cleardisplay(device);

	if (channel && channel->answered_elsewhere) {
		sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);
	}

	sccp_dev_set_ringer(device, SKINNY_RINGTYPE_OFF, lineInstance, callid);
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_dev_set_cplane(device, lineInstance, 0);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOOK);

	sccp_handle_time_date_req(device->session, (sccp_device_t *) device, NULL);	/** we need datetime on hangup for 7936 */

	if (channel && channel == device->active_channel) {
		sccp_dev_set_speaker(device, SKINNY_STATIONSPEAKER_OFF);
	}
	sccp_device_clearMessageFromStack((sccp_device_t *) device, SCCP_MESSAGE_PRIORITY_PRIVACY);
	sccp_dev_check_displayprompt(device);									/* see if we need to display anything from the messageStack */
}
#endif
static void sccp_device_indicate_onhook(const sccp_device_t * device, const uint8_t lineInstance, uint32_t callid)
{
	sccp_dev_stoptone(device, lineInstance, callid);
	sccp_dev_cleardisplaynotify(device);
	sccp_dev_clearprompt(device, lineInstance, callid);
	sccp_dev_cleardisplay(device);
	//sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_HIDDEN);    /** if channel was answered somewhere, set state to connected before onhook -> no missedCalls entry */

	sccp_dev_set_ringer(device, SKINNY_RINGTYPE_OFF, lineInstance, callid);
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_dev_set_cplane(device, lineInstance, 0);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOOK);

	sccp_handle_time_date_req(device->session, (sccp_device_t *) device, NULL);	/** we need datetime on hangup for 7936 */
	sccp_device_clearMessageFromStack((sccp_device_t *) device, SCCP_MESSAGE_PRIORITY_PRIVACY);
	sccp_dev_check_displayprompt(device);									/* see if we need to display anything from the messageStack */
	sccp_dev_set_speaker(device, SKINNY_STATIONSPEAKER_OFF);
}

static void sccp_device_indicate_offhook_remote(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel)
{
	sccp_device_sendcallstate(device, linedevice->lineInstance, channel->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_dev_set_keyset(device, linedevice->lineInstance, channel->callid, KEYMODE_OFFHOOK);
}

static void sccp_device_indicate_onhook_remote(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel)
{

	sccp_device_setLamp(device, SKINNY_STIMULUS_LINE, linedevice->lineInstance, SKINNY_LAMP_OFF);
	sccp_dev_cleardisplaynotify(device);
	sccp_dev_clearprompt(device, linedevice->lineInstance, channel->callid);
	sccp_dev_set_ringer(device, SKINNY_RINGTYPE_OFF, linedevice->lineInstance, channel->callid);
	sccp_device_sendcallstate(device, linedevice->lineInstance, channel->callid, SKINNY_CALLSTATE_ONHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	sccp_dev_set_keyset(device, linedevice->lineInstance, channel->callid, KEYMODE_ONHOOK);
	sccp_dev_set_cplane(device, linedevice->lineInstance, 0);
	sccp_dev_set_keyset(device, linedevice->lineInstance, channel->callid, KEYMODE_ONHOOK);

	sccp_handle_time_date_req(device->session, (sccp_device_t *) device, NULL);	/** we need datetime on hangup for 7936 */
}

static void sccp_device_indicate_connected(const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel)
{
	sccp_dev_set_ringer(device, SKINNY_RINGTYPE_OFF, linedevice->lineInstance, channel->callid);
	sccp_dev_set_speaker(device, SKINNY_STATIONSPEAKER_ON);
	sccp_dev_stoptone(device, linedevice->lineInstance, channel->callid);
	sccp_device_setLamp(device, SKINNY_STIMULUS_LINE, linedevice->lineInstance, SKINNY_LAMP_ON);
	sccp_device_sendcallstate(device, linedevice->lineInstance, channel->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	if (device->protocol && device->protocol->sendCallInfo) {
		device->protocol->sendCallInfo(device, channel, linedevice->lineInstance);
	}
	sccp_dev_set_cplane(device, linedevice->lineInstance, 1);
	//sccp_dev_set_keyset(device, linedevice->lineInstance, channel->callid, KEYMODE_CONNECTED);              // moved back to sccp_indicate (re-used for connconf as well)
	sccp_dev_displayprompt(device, linedevice->lineInstance, channel->callid, SKINNY_DISP_CONNECTED, GLOB(digittimeout));
}

static void __sccp_device_indicate_immediate_dialing(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel)
{
	sccp_device_setLamp(device, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_BLINK);
	sccp_dev_set_keyset(device, lineInstance, channel->callid, KEYMODE_OFFHOOK);
}

static void __sccp_device_indicate_normal_dialing(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel)
{
	sccp_channel_t *c = (sccp_channel_t *) channel;

	sccp_dev_stoptone(device, lineInstance, channel->callid);
	sccp_device_setLamp(device, SKINNY_STIMULUS_LINE, lineInstance, SKINNY_LAMP_BLINK);
	sccp_channel_set_calledparty(c, NULL, c->dialedNumber);
	if (device->protocol) {
		if (device->protocol->sendDialedNumber) {
			device->protocol->sendDialedNumber(device, channel);
		}
		if (device->protocol->sendCallInfo) {
			device->protocol->sendCallInfo(device, channel, lineInstance);
		}
	}
	sccp_dev_set_keyset(device, lineInstance, channel->callid, KEYMODE_RINGOUT);
	sccp_device_sendcallstate(device, lineInstance, channel->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
}

/*!
 * \todo sccp_device_indicate_dialing should be split in a early_immediate version and a standard version, to make this function more readable. We can bind the correct indication function during device
 * registration / sccp_channel_setDevice
 */
static void sccp_device_indicate_dialing(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel)
{
	if (device->earlyrtp != SCCP_EARLYRTP_IMMEDIATE) {
		__sccp_device_indicate_normal_dialing(device, lineInstance, channel);
	} else {
		__sccp_device_indicate_immediate_dialing(device, lineInstance, channel);
	}
}

static void sccp_device_indicate_proceed(const sccp_device_t * device, const uint8_t lineInstance, const sccp_channel_t * channel)
{
	sccp_dev_stoptone(device, lineInstance, channel->callid);
	sccp_device_sendcallstate(device, lineInstance, channel->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
	if (device->protocol && device->protocol->sendCallInfo) {
		device->protocol->sendCallInfo(device, channel, lineInstance);
	}
	sccp_dev_displayprompt(device, lineInstance, channel->callid, SKINNY_DISP_CALL_PROCEED, GLOB(digittimeout));
}

/*!
 * \brief Add message to the MessageStack to be shown on the Status Line of the SCCP Device
 */
void sccp_device_addMessageToStack(sccp_device_t * device, const uint8_t priority, const char *message)
{
	// sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_1 "%s: (sccp_device_addMessageToStack), '%s' at priority %d \n", DEV_ID_LOG(device), message, priority);
	if (ARRAY_LEN(device->messageStack.messages) <= priority) {
		return;
	}
	char *newValue = NULL;
	char *oldValue = NULL;

	newValue = strdup(message);

	do {
		oldValue = device->messageStack.messages[priority];
	} while (!CAS_PTR(&device->messageStack.messages[priority], oldValue, newValue, &device->messageStack.lock));

	if (oldValue) {
		sccp_free(oldValue);
	}
	sccp_dev_check_displayprompt(device);
}

/*!
 * \brief Remove a message from the MessageStack to be shown on the Status Line of the SCCP Device
 */
void sccp_device_clearMessageFromStack(sccp_device_t * device, const uint8_t priority)
{
	if (ARRAY_LEN(device->messageStack.messages) <= priority) {
		return;
	}

	char *newValue = NULL;
	char *oldValue = NULL;

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_4 "%s: clear message stack %d\n", DEV_ID_LOG(device), priority);

	do {
		oldValue = device->messageStack.messages[priority];
	} while (!CAS_PTR(&device->messageStack.messages[priority], oldValue, newValue, &device->messageStack.lock));

	if (oldValue) {
		sccp_free(oldValue);
		sccp_dev_check_displayprompt(device);
	}
}

/*!
 * \brief Handle Feature Change Event for persistent feature storage
 * \param event SCCP Event
 *
 * \callgraph
 * \callergraph
 *
 * \warning
 *   - device->buttonconfig is not always locked
 *   - line->devices is not always locked
 *
 * \todo implement cfwd_noanswer
 */
void sccp_device_featureChangedDisplay(const sccp_event_t * event)
{
	sccp_linedevices_t *linedevice = NULL;
	sccp_device_t *device = NULL;

	char tmp[256] = { 0 };
	size_t len = sizeof(tmp);
	char *s = tmp;

	if (!event || !(device = event->event.featureChanged.device)) {
		return;
	}
	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_EVENT + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: Received Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), featureType2str(event->event.featureChanged.featureType), event->event.featureChanged.featureType);
	switch (event->event.featureChanged.featureType) {
		case SCCP_FEATURE_CFWDNONE:
			sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_CFWD);
			break;
		case SCCP_FEATURE_CFWDBUSY:
		case SCCP_FEATURE_CFWDALL:
			if ((linedevice = event->event.featureChanged.optional_linedevice)) {
				sccp_line_t *line = linedevice->line;
				uint8_t instance = linedevice->lineInstance;

				sccp_dev_forward_status(line, instance, device);
				switch (event->event.featureChanged.featureType) {
					case SCCP_FEATURE_CFWDALL:
						if (linedevice->cfwdAll.enabled) {
							/* build disp message string */
							if (s != tmp) {
								pbx_build_string(&s, &len, ", ");
							}
							if (strlen(line->cid_num) + strlen(linedevice->cfwdAll.number) > 15) {
								pbx_build_string(&s, &len, "%s:%s", SKINNY_DISP_CFWDALL, linedevice->cfwdAll.number);
							} else {
								pbx_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDALL, line->cid_num, SKINNY_DISP_FORWARDED_TO, linedevice->cfwdAll.number);
							}
						}
						break;
					case SCCP_FEATURE_CFWDBUSY:
						if (linedevice->cfwdBusy.enabled) {
							/* build disp message string */
							if (s != tmp) {
								pbx_build_string(&s, &len, ", ");
							}
							if (strlen(line->cid_num) + strlen(linedevice->cfwdAll.number) > 15) {
								pbx_build_string(&s, &len, "%s:%s", SKINNY_DISP_CFWDBUSY, linedevice->cfwdBusy.number);
							} else {
								pbx_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDBUSY, line->cid_num, SKINNY_DISP_FORWARDED_TO, linedevice->cfwdBusy.number);
							}
						}
						break;
					default:
						break;
				}
			}
			if (strlen(tmp) > 0) {
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_CFWD, tmp);
			} else {
				sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_CFWD);
			}

			break;
		case SCCP_FEATURE_DND:

			if (!device->dndFeature.status) {
				sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_DND);
			} else {
				if (device->dndFeature.status == SCCP_DNDMODE_SILENT) {
					sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_DND, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_SILENT ") <<<");
				} else {
					sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_DND, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_BUSY ") <<<");
				}
			}
			break;
		case SCCP_FEATURE_PRIVACY:
			if (TRUE == device->privacyFeature.status) {
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_PRIVACY, SKINNY_DISP_PRIVATE);
			} else {
				sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_PRIVACY);
			}
			break;
		case SCCP_FEATURE_MONITOR:
			if (device->monitorFeature.status) {
				//sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_MONITOR, SKINNY_DISP_MONITOR);
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_MONITOR, SKINNY_DISP_RECORDING);
			} else {
				sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_MONITOR);
			}
			break;
		default:
			return;
	}

}

/*!
 * \brief Push a URL to an SCCP device
 */
static sccp_push_result_t sccp_device_pushURL(const sccp_device_t * device, const char *url, uint8_t priority, uint8_t tone)
{
	const char *xmlFormat = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"%s\"/></CiscoIPPhoneExecute>";
	size_t msg_length = strlen(xmlFormat) + sccp_strlen(url) - 2 /* for %s */  + 1 /* for terminator */ ;

	if (sccp_strlen(url) > 256) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (pushURL) url is to long (max 256 char).\n", DEV_ID_LOG(device));
		return SCCP_PUSH_RESULT_FAIL;
	}
	char xmlData[msg_length];

	snprintf(xmlData, msg_length, xmlFormat, url);
	device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 1, 1, xmlData, priority);
	if (SKINNY_TONE_SILENCE != tone) {
		sccp_dev_starttone(device, tone, 0, 0, 0);
	}
	return SCCP_PUSH_RESULT_SUCCESS;
}

/*!
 * \brief Push a Text Message to an SCCP device
 *
 * \note
 * title field can be max 32 characters long
 * protocolversion < 17 allows for maximum of 1024 characters in the text block / maximum 2000 characted in overall message
 * protocolversion > 17 allows variable sized messages up to 4000 char in the text block (using multiple messages if necessary)
 */
static sccp_push_result_t sccp_device_pushTextMessage(const sccp_device_t * device, const char *messageText, const char *from, uint8_t priority, uint8_t tone)
{
	const char *xmlFormat = "<CiscoIPPhoneText>%s<Text>%s</Text></CiscoIPPhoneText>";
	size_t msg_length = strlen(xmlFormat) + sccp_strlen(messageText) - 4 /* for the %s' */  + 1 /* for terminator */ ;

	if (sccp_strlen(from) > 32) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (pushTextMessage) from is to long (max 32 char).\n", DEV_ID_LOG(device));
		return SCCP_PUSH_RESULT_FAIL;
	}

	if ((device->protocolversion < 17 && 1024 > msg_length) || sccp_strlen(messageText) > 4000) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (pushTextMessage) messageText is to long.\n", DEV_ID_LOG(device));
		return SCCP_PUSH_RESULT_FAIL;
	}

	const char *xmlTitleFormat = "<Title>%s</Title>";
	size_t title_length = strlen(xmlTitleFormat) + sccp_strlen(from) - 2 /* for the %s */  + 1 /* for terminator */ ;
	char title[title_length];

	if (!sccp_strlen_zero(from)) {
		msg_length += title_length;
		snprintf(title, title_length, xmlTitleFormat, from);
	}

	char xmlData[msg_length];

	snprintf(xmlData, msg_length, xmlFormat, title, messageText);
	device->protocol->sendUserToDeviceDataVersionMessage(device, 0, 0, 1, 1, xmlData, priority);

	if (SKINNY_TONE_SILENCE != tone) {
		sccp_dev_starttone(device, tone, 0, 0, 0);
	}
	return SCCP_PUSH_RESULT_SUCCESS;
}

/*=================================================================================== FIND FUNCTIONS ==============*/

/*!
 * \brief Find Device by ID
 *
 * \callgraph
 * \callergraph
 *
 * \param id Device ID (SEP.....)
 * \param useRealtime Use RealTime as Boolean
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *sccp_device_find_byid(const char *id, boolean_t useRealtime)
{
	sccp_device_t *d = NULL;

	if (sccp_strlen_zero(id)) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for device with name ''\n");
		return NULL;
	}

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	d = SCCP_RWLIST_FIND(&GLOB(devices), d, list, (sccp_strcaseequals(d->id, id)), TRUE);
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

#ifdef CS_SCCP_REALTIME
	if (!d && useRealtime) {
		d = sccp_device_find_realtime_byid(id);
	}
#endif

	return d;
}

#ifdef CS_SCCP_REALTIME
/*!
 * \brief Find Device via RealTime
 *
 * \callgraph
 * \callergraph
 */
#if DEBUG
/*!
 * \param name Device ID (hostname)
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *__sccp_device_find_realtime(const char *name, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Device ID (hostname)
 * \return SCCP Device - can bee null if device is not found
 */
sccp_device_t *sccp_device_find_realtime(const char *name)
#endif
{
	sccp_device_t *d = NULL;
	PBX_VARIABLE_TYPE *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimedevicetable)) || sccp_strlen_zero(name)) {
		return NULL;
	}
	if ((variable = pbx_load_realtime(GLOB(realtimedevicetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' found in realtime table '%s'\n", name, GLOB(realtimedevicetable));

		d = sccp_device_create(name);		/** create new device */
		if (!d) {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime device '%s'\n", name);
			return NULL;
		}
		// sccp_copy_string(d->id, name, sizeof(d->id));

		sccp_config_applyDeviceConfiguration(d, v);		/** load configuration and set defaults */

		sccp_config_restoreDeviceFeatureStatus(d);		/** load device status from database */

		sccp_device_addToGlobals(d);				/** add to device to global device list */

		d->realtime = TRUE;					/** set device as realtime device */
		pbx_variables_destroy(v);

		return d;
	}

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Device '%s' not found in realtime table '%s'\n", name, GLOB(realtimedevicetable));
	return NULL;
}
#endif

void sccp_device_setLamp(const sccp_device_t * device, skinny_stimulus_t stimulus, uint8_t instance, skinny_lampmode_t mode)
{
	sccp_msg_t *msg;

	REQ(msg, SetLampMessage);

	if (msg) {
		msg->data.SetLampMessage.lel_stimulus = htolel(stimulus);
		msg->data.SetLampMessage.lel_stimulusInstance = instance;
		msg->data.SetLampMessage.lel_lampMode = htolel(mode);
		sccp_dev_send(device, msg);
	}
}

// kate: indent-width 4; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets on;
