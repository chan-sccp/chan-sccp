
/*!
 * \file 	sccp_device.c
 * \brief 	SCCP Device Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \remarks	Purpose: 	SCCP Device
 * 		When to use:	Only methods directly related to sccp devices should be stored in this source file.
 *   		Relationships: 	SCCP Device -> SCCP DeviceLine -> SCCP Line
 *   			 	SCCP Line -> SCCP ButtonConfig -> SCCP Device
 *
 * \date        $Date$
 * \version     $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
int __sccp_device_destroy(const void *ptr);

static void sccp_device_old_indicate_remoteHold(const sccp_device_t *device, uint8_t lineInstance, uint8_t callid, uint8_t callPriority, uint8_t callPrivacy);

static void sccp_device_new_indicate_remoteHold(const sccp_device_t *device, uint8_t lineInstance, uint8_t callid, uint8_t callPriority, uint8_t callPrivacy);

static sccp_push_result_t sccp_device_pushURL(const sccp_device_t *device, const char *url, uint8_t priority, uint8_t tone);
static sccp_push_result_t sccp_device_pushURLNotSupported(const sccp_device_t *device, const char *url, uint8_t priority, uint8_t tone){
	return SCCP_PUSH_RESULT_NOT_SUPPORTED;
}

static sccp_push_result_t sccp_device_pushTextMessage(const sccp_device_t *device, const char *messageText, const char *from, uint8_t priority, uint8_t tone);
static sccp_push_result_t sccp_device_pushTextMessageNotSupported(const sccp_device_t *device, const char *messageText, const char *from, uint8_t priority, uint8_t tone){
	return SCCP_PUSH_RESULT_NOT_SUPPORTED;
}

static const struct sccp_device_indication_cb sccp_device_indication_newerDevices = {
	.remoteHold = sccp_device_new_indicate_remoteHold,
};

static const struct sccp_device_indication_cb sccp_device_indication_olderDevices = {
	.remoteHold = sccp_device_old_indicate_remoteHold,
};

static boolean_t sccp_device_checkACLTrue(sccp_device_t *device){
	return TRUE;
}

static boolean_t sccp_device_trueResult(void){
	return TRUE;
}

static boolean_t sccp_device_falseResult(void){
	return FALSE;
}

/*!
 * \brief Check device ipaddress against the ip ACL (permit/deny and permithosts entries)
 */
static boolean_t sccp_device_checkACL(sccp_device_t *device){
	
	struct sockaddr_in sin;
	boolean_t matchesACL = FALSE;
	
	/* get current socket information */
	sccp_session_getSocketAddr(device, &sin);
	
	
	/* no permit deny information */
	if(!device->ha){
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: no deny/permit information for this device, allow all connections", device->id);
		return TRUE;
	}
  
	if (sccp_apply_ha(device->ha, &sin) != AST_SENSE_ALLOW) {
	  
		// checking permithosts	
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: not allowed by deny/permit list. Checking permithost list...", device->id);

		struct ast_hostent ahp;
		struct hostent *hp;
		sccp_hostname_t *permithost;
		
		uint8_t i=0;
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&device->permithosts, permithost, list) {
			if ((hp = pbx_gethostbyname(permithost->name, &ahp))) {
				for(i=0; NULL != hp->h_addr_list[i]; i++ ) {	// walk resulting ip address
					if (sin.sin_addr.s_addr == (*(struct in_addr*)hp->h_addr_list[i]).s_addr) {
						sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: permithost = %s match found.\n", device->id, permithost->name);
						matchesACL = TRUE;
						continue;
					}
				}
			} else {
				sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Invalid address resolution for permithost = %s (skipping permithost).\n", device->id, permithost->name);
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
	}else{
		matchesACL = TRUE;
	}
  
	return matchesACL;
}

#define  REF_DEBUG 1
#ifdef CS_DYNAMIC_CONFIG

/*!
 * \brief run before reload is start on devices
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- devices
 * 	  - device->buttonconfig
 */
void sccp_device_pre_reload(void)
{
	sccp_device_t *d;
	sccp_buttonconfig_t *config;

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "%s: Setting Device to Pending Delete=1\n", d->id);
		sccp_free_ha(d->ha);
		d->ha = NULL;
		if (!d->realtime)						/* don't want to reset hotline devices. */
			d->pendingDelete = 1;
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
 * \param d SCCP Device
 * \return Result as Boolean
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_numberOfChannels()
 * 	  - see sccp_device_sendReset()
 * 	  - see sccp_session_close()
 * 	  - device->buttonconfig
 */
boolean_t sccp_device_check_update(sccp_device_t * d)
{
	if (!d->pendingUpdate && !d->pendingDelete)
		return FALSE;

	sccp_device_lock(d);

	if (sccp_device_numberOfChannels(d) > 0) {
		sccp_device_unlock(d);
		return FALSE;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "Device %s needs to be reset because of a change in sccp.conf\n", d->id);
	sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
	if (d->session)
		pthread_cancel(d->session->session_thread);
	d->pendingUpdate = 0;

	if (d->pendingDelete) {
		sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Remove Device from List\n", d->id);
		sccp_device_unlock(d);
		sccp_dev_clean(d, TRUE, 0);
	} else {
		sccp_buttonconfig_t *buttonconfig;

		SCCP_LIST_LOCK(&d->buttonconfig);
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&d->buttonconfig, buttonconfig, list) {
			if (!buttonconfig->pendingDelete && !buttonconfig->pendingUpdate)
				continue;

			if (buttonconfig->pendingDelete) {
				sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "Remove Buttonconfig for %s from List\n", d->id);
				SCCP_LIST_REMOVE_CURRENT(list);
				sccp_free(buttonconfig);
			} else {
				buttonconfig->pendingUpdate = 0;
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&d->buttonconfig);

		sccp_device_unlock(d);
	}

	return TRUE;
}

/*!
 * \brief run after the new device config is loaded during the reload process
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- devices
 * 	  - see sccp_device_check_update()
 */
void sccp_device_post_reload(void)
{
	sccp_device_t *d;

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		if (!d->pendingDelete && !d->pendingUpdate)
			continue;

		/* Because of the previous check, the only reason that the device hasn't
		 * been updated will be because it is currently engaged in a call.
		 */
		if (!sccp_device_check_update(d))
			sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "Device %s will receive reset after current call is completed\n", d->id);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(devices));
}
#endif										/* CS_DYNAMIC_CONFIG */

/*!
 * \brief create a device and adding default values.
 * \return device with default/global values
 *
 * \callgraph
 * \callergraph
 */
sccp_device_t *sccp_device_create(void)
{
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "DEVICE CREATE\n");
#if CS_EXPERIMENTAL	//refcount
	sccp_device_t *d = RefCountedObjectAlloc(sizeof(sccp_device_t), __sccp_device_destroy);
#else
	sccp_device_t *d = sccp_calloc(1, sizeof(sccp_device_t));
#endif
	if (!d) {
		sccp_log(0) (VERBOSE_PREFIX_3 "Unable to allocate memory for a device\n");
		return NULL;
	}
	
	memset(d, 0, sizeof(d));
	pbx_mutex_init(&d->lock);
	sccp_device_lock(d);

	SCCP_LIST_HEAD_INIT(&d->buttonconfig);
	SCCP_LIST_HEAD_INIT(&d->selectedChannels);
	SCCP_LIST_HEAD_INIT(&d->addons);
#ifdef CS_DEVSTATE_FEATURE
	SCCP_LIST_HEAD_INIT(&d->devstateSpecifiers);
#endif	
	memset(d->softKeyConfiguration.activeMask, 0xFF, sizeof(d->softKeyConfiguration.activeMask));
	
	d->softKeyConfiguration.modes = (softkey_modes *) SoftKeyModes;
	d->softKeyConfiguration.size = ARRAY_LEN(SoftKeyModes);
	d->state = SCCP_DEVICESTATE_ONHOOK;
	d->postregistration_thread = AST_PTHREADT_STOP;
	
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "!!! Init MessageStack\n");

	/* initialize messageStack */
	uint8_t i;
	for(i=0; i < ARRAY_LEN(d->messageStack); i++) {
		d->messageStack[i] = NULL;
	}

	/* disable videomode softkey for all softkeysets */
	for(i=0; i< KEYMODE_ONHOOKSTEALABLE; i++){
		sccp_softkey_setSoftkeyState(d, i, SKINNY_LBL_VIDEO_MODE, FALSE);
	}
	
	d->pushURL = sccp_device_pushURLNotSupported;
	d->pushTextMessage = sccp_device_pushTextMessageNotSupported;
	d->checkACL = sccp_device_checkACL;
	d->hasDisplayPrompt = sccp_device_trueResult;
	sccp_device_unlock(d);
	return d;
}

sccp_device_t *sccp_device_createAnonymous(const char *name){
	sccp_device_t *d = sccp_device_create();
	d->realtime = TRUE;
	d->isAnonymous = TRUE;
	d->checkACL = sccp_device_checkACLTrue;
	
	sccp_copy_string(d->id, name, sizeof(d->id));
	
	return d;
}

/*!
 * \brief set type of Indicate protocol by device type
 */
void sccp_device_setIndicationProtocol(sccp_device_t *device){
  
	switch (device->skinny_type) {
// 	case SKINNY_DEVICETYPE_30SPPLUS:
// 	case SKINNY_DEVICETYPE_30VIP:
// 	case SKINNY_DEVICETYPE_12SPPLUS:
// 	case SKINNY_DEVICETYPE_12SP:
// 	case SKINNY_DEVICETYPE_12:
// 	case SKINNY_DEVICETYPE_CISCO7902:
// 	case SKINNY_DEVICETYPE_CISCO7912:
// 	case SKINNY_DEVICETYPE_CISCO7911:
// 	case SKINNY_DEVICETYPE_CISCO7906:
// 	case SKINNY_DEVICETYPE_CISCO7905:
// 	case SKINNY_DEVICETYPE_CISCO7931:
// 	case SKINNY_DEVICETYPE_CISCO7935:
// 	case SKINNY_DEVICETYPE_CISCO7936:
// 	case SKINNY_DEVICETYPE_CISCO7937:
// 	case SKINNY_DEVICETYPE_CISCO7910:
// 	case SKINNY_DEVICETYPE_CISCO7940:
// 	case SKINNY_DEVICETYPE_CISCO7960:
	case SKINNY_DEVICETYPE_CISCO7941:
	case SKINNY_DEVICETYPE_CISCO7941GE:
	case SKINNY_DEVICETYPE_CISCO7942:
	case SKINNY_DEVICETYPE_CISCO7945:
// 	case SKINNY_DEVICETYPE_CISCO7920:
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
	return;
}

/*!
 * \brief Add a device to the global sccp_list
 * \param device SCCP Device
 * \return SCCP Device
 * 
 * \lock
 * 	- devices
 */
sccp_device_t *sccp_device_addToGlobals(sccp_device_t * device)
{
	if (!device)
		return NULL;

	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	SCCP_RWLIST_INSERT_HEAD(&GLOB(devices), device, list);
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "Added device '%s' (%s)\n", device->id, device->config_type);
	return device;
}

/*!
 * \brief Create a template of Buttons as Definition for a Phonetype (d->skinny_type)
 * \param d device
 * \param btn buttonlist
 */
void sccp_dev_build_buttontemplate(sccp_device_t * d, btnlist * btn)
{
	uint8_t i;

	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Building button template %s(%d), user config %s\n", d->id, devicetype2str(d->skinny_type), d->skinny_type, d->config_type);

	switch (d->skinny_type) {
	case SKINNY_DEVICETYPE_30SPPLUS:
	case SKINNY_DEVICETYPE_30VIP:
		for (i = 0; i < 4; i++)
			(btn++)->type = SCCP_BUTTONTYPE_LINE;
		for (i = 0; i < 9; i++)
			(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		/* Column 2 */
		(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
		(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
		(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
		(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
		for (i = 0; i < 9; i++)
			(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		break;
	case SKINNY_DEVICETYPE_12SPPLUS:
	case SKINNY_DEVICETYPE_12SP:
	case SKINNY_DEVICETYPE_12:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
		(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
		(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
		(btn++)->type = SKINNY_BUTTONTYPE_CALLPARK;
		(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
		(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
		break;
	case SKINNY_DEVICETYPE_CISCO7902:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
		(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
		(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
		(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
		(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
		(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
		break;
	case SKINNY_DEVICETYPE_CISCO7912:
	case SKINNY_DEVICETYPE_CISCO7911:
	case SKINNY_DEVICETYPE_CISCO7906:
	case SKINNY_DEVICETYPE_CISCO7905:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
		for (i = 0; i < 9; i++)
			(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
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
		break;
	case SKINNY_DEVICETYPE_CISCO7935:
	case SKINNY_DEVICETYPE_CISCO7936:
	case SKINNY_DEVICETYPE_CISCO7937:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		break;
	case SKINNY_DEVICETYPE_CISCO7910:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SKINNY_BUTTONTYPE_HOLD;
		(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
		(btn++)->type = SKINNY_BUTTONTYPE_DISPLAY;
		(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
		(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
		(btn++)->type = SKINNY_BUTTONTYPE_FORWARDALL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		(btn++)->type = SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
		break;
	case SKINNY_DEVICETYPE_CISCO7940:
	case SKINNY_DEVICETYPE_CISCO7941:
	case SKINNY_DEVICETYPE_CISCO7941GE:
	case SKINNY_DEVICETYPE_CISCO7942:
	case SKINNY_DEVICETYPE_CISCO7945:
		/* add text message support */
		d->pushTextMessage = sccp_device_pushTextMessage;
		d->pushURL = sccp_device_pushURL;
		
		
		for (i = 0; i < 2 + sccp_addons_taps(d); i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7920:
	case SKINNY_DEVICETYPE_CISCO7921:
	case SKINNY_DEVICETYPE_CISCO7925:
		for (i = 0; i < 6; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7985:
		d->capabilities.video[0] = SKINNY_CODEC_H264;
		d->capabilities.video[1] = SKINNY_CODEC_H263;
#ifdef CS_SCCP_VIDEO
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
#endif
		for (i = 0; i < 1; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7960:
	case SKINNY_DEVICETYPE_CISCO7961:
	case SKINNY_DEVICETYPE_CISCO7961GE:
	case SKINNY_DEVICETYPE_CISCO7962:
	case SKINNY_DEVICETYPE_CISCO7965:
		/* add text message support */
		d->pushTextMessage = sccp_device_pushTextMessage;
		d->pushURL = sccp_device_pushURL;
		
		for (i = 0; i < 6 + sccp_addons_taps(d); i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7970:
	case SKINNY_DEVICETYPE_CISCO7971:
	case SKINNY_DEVICETYPE_CISCO7975:
	case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR:
		/* the nokia icc client identifies it self as SKINNY_DEVICETYPE_CISCO7970, but it can only have one line  */
		if (!strcasecmp(d->config_type, "nokia-icc")) {			// this is for nokia icc legacy support (Old releases) -FS
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		} else {
			uint8_t addonsTaps = sccp_addons_taps(d);

			for (i = 0; i < 8 + addonsTaps; i++) {
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			}
			
			/* add text message support */
			d->pushTextMessage = sccp_device_pushTextMessage;
			d->pushURL = sccp_device_pushURL;
		}
		break;
	case SKINNY_DEVICETYPE_NOKIA_ICC:
		(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_NOKIA_E_SERIES:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		for (i = 0; i < 5; i++)
			(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		break;
	case SKINNY_DEVICETYPE_ATA186:
		//case SKINNY_DEVICETYPE_ATA188:
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		for (i = 0; i < 4; i++)
			(btn++)->type = SCCP_BUTTONTYPE_SPEEDDIAL;
		break;
	case SKINNY_DEVICETYPE_CISCO8941:
	case SKINNY_DEVICETYPE_CISCO8945:
		d->capabilities.video[0] = SKINNY_CODEC_H264;
		d->capabilities.video[1] = SKINNY_CODEC_H263;
#ifdef CS_SCCP_VIDEO
		sccp_softkey_setSoftkeyState(d, KEYMODE_CONNTRANS, SKINNY_LBL_VIDEO_MODE, TRUE);
#endif
		d->hasDisplayPrompt = sccp_device_falseResult;
		for (i = 0; i < 4; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_SPA_521S:
		for (i = 0; i < 1; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_SPA_525G2:
		for (i = 0; i < 8; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
     	case SKINNY_DEVICETYPE_CISCO6901:
     	case SKINNY_DEVICETYPE_CISCO6911:
                (btn++)->type = SCCP_BUTTONTYPE_MULTI;
                break;
    	case SKINNY_DEVICETYPE_CISCO6921:
    		for (i = 0; i < 2; i++) {
                	(btn++)->type = SCCP_BUTTONTYPE_MULTI;
                }
                d->hasDisplayPrompt = sccp_device_falseResult;
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
		/* at least one line */
		(btn++)->type = SCCP_BUTTONTYPE_LINE;
		break;
	}
	return;
}

/*!
 * \brief Build an SCCP Message Packet
 * \param[in] t SCCP Message Text
 * \param[out] pkt_len Packet Length
 * \return SCCP Message
 */
sccp_moo_t *sccp_build_packet(sccp_message_t t, size_t pkt_len)
{
	sccp_moo_t *r = sccp_malloc(sizeof(sccp_moo_t));

	if (!r) {
		pbx_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
		return NULL;
	}
	memset(r, 0, pkt_len + 12);
	r->length = htolel(pkt_len + 4);
	r->lel_messageId = htolel(t);
	return r;
}

/*!
 * \brief Send SCCP Message to Device
 * \param d SCCP Device
 * \param r SCCP MOO Message
 * \return Status as int
 *
 * \callgraph
 * \callergraph
 */
int sccp_dev_send(const sccp_device_t * d, sccp_moo_t * r)
{
	if (d && d->session) {
		sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: >> Send message %s\n", d->id, message2str(letohl(r->lel_messageId)));
		return sccp_session_send(d, r);
	} else
		return -1;
}

/*!
 * \brief Send an SCCP message to a device
 * \param d SCCP Device
 * \param t SCCP Message
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_sendmsg(const sccp_device_t * d, sccp_message_t t)
{
	if (d)
		sccp_session_sendmsg(d, t);
}

/*!
 * \brief Register a Device
 * \param d SCCP Device
 * \param opt Option/Registration State as int
 */
void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt)
{
	char servername[StationMaxDisplayNotifySize];
	sccp_moo_t *r;

	if (d->registrationState == opt)
		return;

	d->registrationState = opt;

	/* Handle registration completion. */
	if (opt == SKINNY_DEVICE_RS_OK) {

		/* this message is mandatory to finish process */
		REQ(r, SetLampMessage);
		r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		r->msg.SetLampMessage.lel_stimulusInstance = 0;
		r->msg.SetLampMessage.lel_lampMode = htolel(SKINNY_LAMP_OFF);
		d->mwilight &= ~(1 << 0);
		sccp_dev_send(d, r);

		if (!d->linesRegistered) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device does not support RegisterAvailableLinesMessage, force this\n", DEV_ID_LOG(d));
			sccp_handle_AvailableLines(d->session, d, NULL);
			d->linesRegistered = TRUE;
		}

		d->registrationTime = time(0);
		snprintf(servername, sizeof(servername), "%s %s", GLOB(servername), SKINNY_DISP_CONNECTED);
		sccp_dev_displaynotify(d, servername, 5);
		sccp_dev_postregistration(d);
	} else if (opt == SKINNY_DEVICE_RS_PROGRESS) {
		sccp_event_t *event = sccp_malloc(sizeof(sccp_event_t));
		memset(event, 0, sizeof(sccp_event_t));
		event->type = SCCP_EVENT_DEVICE_PREREGISTERED;
       		event->event.deviceRegistered.device = d;
		sccp_event_fire((const sccp_event_t **)&event);
	}
}

/*!
 * \brief Sets the SCCP Device's SoftKey Mode Specified by opt
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID as uint8_t
 * \param softKeySetIndex SoftKeySet Index
 * \todo Disable DirTrfr by Default
 */
void sccp_dev_set_keyset(const sccp_device_t *d, uint8_t line, uint32_t callid, uint8_t softKeySetIndex)
{
	sccp_moo_t *r;

	if (!d)
		return;

	if (!d->softkeysupport)
		return;								/* the device does not support softkeys */

	/*let's activate the transfer */
	if (softKeySetIndex == KEYMODE_CONNECTED)
		softKeySetIndex = (( /* d->conference && */ d->conference_channel) ? KEYMODE_CONNCONF : (d->transfer) ? KEYMODE_CONNTRANS : KEYMODE_CONNECTED);

	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance = htolel(line);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(softKeySetIndex);

	if ((softKeySetIndex == KEYMODE_ONHOOK || softKeySetIndex == KEYMODE_OFFHOOK || softKeySetIndex == KEYMODE_OFFHOOKFEAT)
	    && (sccp_strlen_zero(d->lastNumber)
#ifdef CS_ADV_FEATURES
		&& !d->useRedialMenu
#endif
	    )
	    ) {
		sccp_softkey_setSoftkeyState((sccp_device_t *)d, softKeySetIndex, SKINNY_LBL_REDIAL, FALSE);
	}
	
	//r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF;		/* htolel(65535); */
	r->msg.SelectSoftKeysMessage.les_validKeyMask = htolel( d->softKeyConfiguration.activeMask[softKeySetIndex] );

	sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, keymode2str(softKeySetIndex), softKeySetIndex, line, callid);
	sccp_dev_send(d, r);
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
	sccp_moo_t *r;

	REQ(r, SetRingerMessage);
	r->msg.SetRingerMessage.lel_ringMode = htolel(opt);
	/* Note that for distinctive ringing to work with the higher protocol versions
	   the following actually needs to be set to 1 as the original comment says.
	   Curiously, the variable is not set to 1 ... */
	r->msg.SetRingerMessage.lel_unknown1 = htolel(1);			/* always 1 */
	r->msg.SetRingerMessage.lel_lineInstance = htolel(lineInstance);
	r->msg.SetRingerMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send ringer mode %s(%d) on device\n", DEV_ID_LOG(d), station2str(opt), opt);

}

/*!
 * \brief Set Speaker Status on Device
 * \param d SCCP Device
 * \param mode Speaker Mode as uint8_t
 */
void sccp_dev_set_speaker(sccp_device_t * d, uint8_t mode)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	REQ(r, SetSpeakerModeMessage);
	r->msg.SetSpeakerModeMessage.lel_speakerMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send speaker mode %d\n", d->id, mode);
}

/*!
 * \brief Set Microphone Status on Device
 * \param d SCCP Device
 * \param mode Microphone Mode as uint8_t
 */
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t mode)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	REQ(r, SetMicroModeMessage);
	r->msg.SetMicroModeMessage.lel_micMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send microphone mode %d\n", d->id, mode);
}

/*!
 * \brief Set Call Plane to Active on  Line on Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as unint8_t
 * \param device SCCP Device
 * \param status Status as int
 * \todo What does this function do exactly (ActivateCallPlaneMessage) ?
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_set_cplane(sccp_line_t * l, uint8_t lineInstance, sccp_device_t * device, int status)
{
	sccp_moo_t *r;

	if (!l)
		return;

	if (!device)
		return;

	REQ(r, ActivateCallPlaneMessage);
	if (status)
		r->msg.ActivateCallPlaneMessage.lel_lineInstance = htolel(lineInstance);
	sccp_dev_send(device, r);

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
	sccp_moo_t *r;

	REQ(r, StartToneMessage);
	r->msg.StartToneMessage.lel_tone = htolel(tone);
	r->msg.StartToneMessage.lel_toneTimeout = htolel(timeout);
	r->msg.StartToneMessage.lel_lineInstance = htolel(line);
	r->msg.StartToneMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending tone %s (%d)\n", d->id, tone2str(tone), tone);
}

/*!
 * \brief Send Stop Tone to Device
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;
	REQ(r, StopToneMessage);
	r->msg.StopToneMessage.lel_lineInstance = htolel(line);
	r->msg.StopToneMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Stop tone on device\n", d->id);
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
		sprintf(msgtimeout,"%d",timeout);
		PBX(feature_addToDatabase)("SCCP/message", "timeout", strdup(msgtimeout));
		PBX(feature_addToDatabase)("SCCP/message", "text", msg);
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
		PBX(feature_removeTreeFromDatabase)("SCCP/message", "timeout");
		PBX(feature_removeTreeFromDatabase)("SCCP/message", "text");
	}	

	sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_IDLE);
//	sccp_dev_clearprompt(d, 0, 0);
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
	sccp_moo_t *r;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	REQ(r, ClearPromptStatusMessage);
	r->msg.ClearPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.ClearPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	sccp_dev_send(d, r);
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
	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_displayprompt '%s' for line %d (%d)\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg, lineInstance, timeout);

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

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
	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearDisplay);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Clear the display\n", d->id);
}

/*!
 * \brief Send Display to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param file Source File
 * \param lineno Source Line 
 * \param pretty_function CB Function to Print
 *
 * \callgraph
 * \callergraph
 */
//void sccp_dev_display(sccp_device_t * d, char *msg)
void sccp_dev_display_debug(const sccp_device_t * d, const char *msg, const char *file, const int lineno, const char *pretty_function)
{
	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_display '%s'\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg);
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

	REQ(r, DisplayTextMessage);
	sccp_copy_string(r->msg.DisplayTextMessage.displayMessage, msg, sizeof(r->msg.DisplayTextMessage.displayMessage));

	sccp_dev_send(d, r);
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
	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Clear the display notify message\n", d->id);
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
	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_displaynotify '%s' (%d)\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg, timeout);

	if (!d || !d->session)
		return;
	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

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
void sccp_dev_cleardisplayprinotify(const sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearPriNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Clear the display priority notify message\n", d->id);
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
	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: ( %s:%d:%s ) sccp_dev_displayprinotify '%s' (%d/%d)\n", DEV_ID_LOG(d), file, lineno, pretty_function, msg, timeout, priority);

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

	d->protocol->displayPriNotify(d, priority, timeout, msg);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Display notify with timeout %d and priority %d\n", d->id, timeout, priority);
}

/*!
 * \brief Find SpeedDial by Index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \param type Type as uint8_t
 * \return SCCP Speed
 * 
 * \lock
 * 	- device->buttonconfig
 */
sccp_speed_t *sccp_dev_speed_find_byindex(sccp_device_t * d, uint16_t instance, uint8_t type)
{
	sccp_speed_t *k = NULL;
	sccp_buttonconfig_t *config;

	if (!d || !d->session || instance == 0)
		return NULL;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {

		if (config->type == SPEEDDIAL && config->instance == instance) {

			/* we are searching for hinted speeddials */
			if (type == SCCP_BUTTONTYPE_HINT && sccp_strlen_zero(config->button.speeddial.hint))
				continue;

			k = sccp_malloc(sizeof(sccp_speed_t));
			memset(k, 0, sizeof(sccp_speed_t));

			k->instance = instance;
			k->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			sccp_copy_string(k->name, config->label, sizeof(k->name));
			sccp_copy_string(k->ext, config->button.speeddial.ext, sizeof(k->ext));
			if (!sccp_strlen_zero(config->button.speeddial.hint)) {
				sccp_copy_string(k->hint, config->button.speeddial.hint, sizeof(k->hint));
			}
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	return k;
}

/*!
 * \brief Send Get Activeline to Device
 * \param d SCCP Device
 * \return SCCP Line
 * 
 * \warning
 * 	- device->buttonconfig is not always locked
 */
sccp_line_t *sccp_dev_get_activeline(sccp_device_t * d)
{
	sccp_buttonconfig_t *buttonconfig;

	if (!d || !d->session)
		return NULL;

	if (!d->currentLine) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			if (buttonconfig->type == LINE) {
				d->currentLine = sccp_line_find_byname_wo(buttonconfig->button.line.name, FALSE);
				if (d->currentLine) {
					break;
				}
			}
		}

		if (d->currentLine) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Forcing the active line to %s from NULL\n", d->id, d->currentLine->name);
		} else {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No lines\n", d->id);
		}
	} else {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: The active line is %s\n", d->id, d->currentLine->name);
	}
	return d->currentLine;
}

/*!
 * \brief Send Set Activeline to Device
 * \param device SCCP Device
 * \param l SCCP Line
 * 
 * \lock
 * 	- device
 */
void sccp_dev_set_activeline(sccp_device_t * device, sccp_line_t * l)
{
	if (!l || !device || !device->session)
		return;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send the active line %s\n", device->id, l->name);
	device->currentLine = l;
	return;
}

/*!
 * \brief Reschedule Display Prompt Check
 * \param d SCCP Device
 *
 * \todo We have to decide on a standardized implementation of displayprompt to be used
 *	 For DND/Cfwd/Message/Voicemail/Private Status for Devices and Individual Lines
 *	 If necessary devicetypes could be deviced into 3-4 groups depending on their capability for displaying status the best way
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_check_displayprompt(sccp_device_t * d)
{
	//sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE))(VERBOSE_PREFIX_1 "%s: %s:%d %s: (sccp_dev_check_displayprompt) Send Current Options to Device\n", file, pretty_function, line, d->id);
	if (!d || !d->session)
		return;

	sccp_dev_clearprompt(d, 0, 0);

	int i;
//	for(i = ARRAY_LEN(d->messageStack); i >= 0; i--){
	for(i = SCCP_MAX_MESSAGESTACK-1; i>=0; i--) {
		if(d->messageStack[i] != NULL){
			sccp_dev_displayprompt(d, 0, 0, d->messageStack[i], 0);
			goto OUT;
		}
	}
	
	/* when we are here, there's nothing to display */
	if(d->hasDisplayPrompt()){
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOUR_CURRENT_OPTIONS, 0);
	}
	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK);				/* this is for redial softkey */

OUT:
	sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Finish DisplayPrompt\n", d->id);
}

/*!
 * \brief Select Line on Device
 * \param d SCCP Device
 * \param wanted SCCP Line
 */
void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * wanted)
{
	sccp_line_t *current;
	sccp_channel_t *chan = NULL;

	if (!d || !d->session)
		return;

	current = sccp_dev_get_activeline(d);

	if (!wanted || !current || current == wanted)
		return;

	// If the current line isn't in a call, and neither is the target.
	if (SCCP_LIST_FIRST(&current->channels) == NULL && SCCP_LIST_FIRST(&wanted->channels) == NULL) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: All lines seem to be inactive, SEIZEing selected line %s\n", d->id, wanted->name);
		sccp_dev_set_activeline(d, wanted);

		chan = sccp_channel_allocate_locked(wanted, d);
		if (!chan) {
			pbx_log(LOG_ERROR, "%s: Failed to allocate SCCP channel.\n", d->id);
			return;
		}
		sccp_channel_unlock(chan);
	} else if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		// If the device is currently onhook, then we need to ...
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Selecing line %s while using line %s\n", d->id, wanted->name, current->name);
		// \todo (1) Put current call on hold
		// (2) Stop transmitting/receiving
	} else {
		// Otherwise, just select the callplane
		pbx_log(LOG_WARNING, "%s: Unknown status while trying to select line %s.  Current line is %s\n", d->id, wanted->name, current->name);
	}
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
	sccp_linedevices_t *linedevice = NULL;

	if (!device || !device->session)
		return;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send Forward Status.  Line: %s\n", device->id, l->name);
	linedevice = sccp_util_getDeviceConfiguration(device, l);

// 	//! \todo check for forward status during registration -MC
	if (!linedevice) {
		if (device->registrationState == SKINNY_DEVICE_RS_OK) {
			pbx_log(LOG_NOTICE, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		}else{
			/** \todo this is the wrong place to do this hack, -MC*/
			if (!device->linesRegistered) {
				sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device does not support RegisterAvailableLinesMessage, force this\n", DEV_ID_LOG(device));
				sccp_handle_AvailableLines(device->session, device, NULL);
				device->linesRegistered = TRUE;
				
				linedevice = sccp_util_getDeviceConfiguration(device, l);
				if(linedevice){
					device->protocol->sendCallforwardMessage(device, linedevice);
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Sent Forward Status.  Line: %s (%d)\n", device->id, l->name, linedevice->lineInstance);
				}
			}
		}
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: no linedevice\n", device->id);
	} else {
		device->protocol->sendCallforwardMessage(device, linedevice);
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Sent Forward Status.  Line: %s (%d)\n", device->id, l->name, linedevice->lineInstance);
	}

	// \todo What to do with this lineStatusChanges in sccp_dev_forward_status
	/*
	   if (l->cfwd_type == SCCP_CFWD_ALL){
	   //sccp_hint_notify_linestate(l,device, SCCP_DEVICESTATE_ONHOOK, SCCP_DEVICESTATE_FWDALL);
	   sccp_hint_lineStatusChanged(l, device, NULL, SCCP_DEVICESTATE_ONHOOK, SCCP_DEVICESTATE_FWDALL);
	   }else{
	   sccp_hint_lineStatusChanged(l, device, NULL, SCCP_DEVICESTATE_FWDALL, SCCP_DEVICESTATE_ONHOOK);
	   }
	 */
}

/*!
 * \brief Check Ringback on Device
 * \param d SCCP Device
 * \return Result as int
 * 
 * \lock
 * 	- device
 */
int sccp_device_check_ringback(sccp_device_t * d)
{
	sccp_channel_t *c;

	sccp_device_lock(d);
	d->needcheckringback = 0;
	if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		sccp_device_unlock(d);
		return 0;
	}
	sccp_device_unlock(d);
	c = sccp_channel_find_bystate_on_device_locked(d, SCCP_CHANNELSTATE_CALLTRANSFER);
	if (!c)
		c = sccp_channel_find_bystate_on_device_locked(d, SCCP_CHANNELSTATE_RINGING);
	if (!c)
		c = sccp_channel_find_bystate_on_device_locked(d, SCCP_CHANNELSTATE_CALLWAITING);

	if (c) {
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_RINGING);
		sccp_channel_unlock(c);
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
 * \lock
 * 	-see sccp_hint_eventListener() via sccp_event_fire()
 */
void *sccp_dev_postregistration(void *data)
{
	sccp_device_t *d = data;

	if (!d)
		return NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Device registered; performing post registration tasks...\n", d->id);

	// Post event to interested listeners (hints, mwi) that device was registered.
	sccp_event_t *event = sccp_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type = SCCP_EVENT_DEVICE_REGISTERED;
	event->event.deviceRegistered.device = d;
	sccp_event_fire((const sccp_event_t **)&event);


	/* read status from db */
#ifndef ASTDB_FAMILY_KEY_LEN
#    define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#    define ASTDB_RESULT_LEN 80
#endif
	char family[ASTDB_FAMILY_KEY_LEN];
	char buffer[ASTDB_RESULT_LEN];

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Getting Database Settings...\n", d->id);
	memset(family, 0, ASTDB_FAMILY_KEY_LEN);
	sprintf(family, "SCCP/%s", d->id);
	if (!PBX(feature_getFromDatabase)(family, "dnd", buffer, sizeof(buffer))) {
		buffer[0]='\0';
	}
	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: buffer='%s'\n", d->id, buffer);
	if (sccp_config_parse_dnd(&d->dndFeature.status, sizeof(d->dndFeature.status),(const char *) buffer, SCCP_CONFIG_DEVICE_SEGMENT)) {
		sccp_feat_changed(d, SCCP_FEATURE_DND);
	}

	if (PBX(feature_getFromDatabase)(family, "privacy", buffer, sizeof(buffer)) && strcmp(buffer,"")) {
		d->privacyFeature.status=TRUE;
		sccp_feat_changed(d, SCCP_FEATURE_PRIVACY);
	}

	if (PBX(feature_getFromDatabase)(family, "monitor", buffer, sizeof(buffer)) && strcmp(buffer,"")) {
		d->monitorFeature.status=TRUE;
		sccp_feat_changed(d, SCCP_FEATURE_MONITOR);
	}

	sccp_dev_check_displayprompt(d);

	sccp_mwi_check(d);

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Post registration process... done!\n", d->id);
	return NULL;
}

/*!
 * \brief Clean Device
 *
 *  clean up memory allocated by the device.
 *  if destroy is true, device will be removed from global device list
 *
 * \param d SCCP Device
 * \param remove_from_global as boolean_t
 * \param cleanupTime Clean-up Time as uint8
 *
 * \todo integrate sccp_dev_clean and sccp_dev_free into sccp_device_delete -DdG
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 * 	- line->channels is not always locked
 * 
 * \lock
 * 	- devices
 * 	- device
 * 	  - see sccp_dev_set_registered()
 * 	  - see sccp_hint_eventListener() via sccp_event_fire()
 * 	  - device->buttonconfig
 * 	    - see sccp_line_find_byname_wo()
 * 	    - see sccp_channel_endcall()
 * 	    - see sccp_line_removeDevice()
 * 	  - device->selectedChannels
 * 	  - device->session
 * 	  - device->devstateSpecifiers
 */
void sccp_dev_clean(sccp_device_t * d, boolean_t remove_from_global, uint8_t cleanupTime)
{
	sccp_buttonconfig_t *config = NULL;
	sccp_selectedchannel_t *selectedChannel = NULL;
	sccp_line_t *line = NULL;
	sccp_channel_t *channel = NULL;

#ifdef CS_DEVSTATE_FEATURE
	sccp_devstate_specifier_t *devstateSpecifier;
#endif
	char family[25];

	if (!d)
		return;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "SCCP: Clean Device %s\n", d->id);

	if (remove_from_global) {
		SCCP_RWLIST_WRLOCK(&GLOB(devices));
		SCCP_RWLIST_REMOVE(&GLOB(devices), d, list);
		SCCP_RWLIST_UNLOCK(&GLOB(devices));
	}

	sccp_device_lock(d);
	sccp_dev_set_registered(d, SKINNY_DEVICE_RS_NONE);			/* set correct register state */

	d->mwilight = 0;							/* reset mwi light */
	d->linesRegistered = FALSE;
	sprintf(family, "SCCP/%s", d->id);
	PBX(feature_removeFromDatabase)(family, "lastDialedNumber");
	if (!sccp_strlen_zero(d->lastNumber))
		PBX(feature_addToDatabase) (family, "lastDialedNumber", d->lastNumber);

	/* unsubscribe hints *//* prevent loop:sccp_dev_clean =>
	   sccp_line_removeDevice =>
	   sccp_event_fire =>
	   sccp_hint_eventListener =>
	   sccp_hint_lineStatusChanged =>
	   sccp_hint_hintStatusUpdate =>
	   sccp_hint_notifySubscribers =>
	   sccp_dev_speed_find_byindex */

	/* hang up open channels and remove device from line */
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == LINE) {
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if (!line)
				continue;

			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, channel, list) {
				if ( sccp_channel_getDevice(channel) == d) {
					sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channel on line %s device %s\n", line->name, d->id);
					sccp_channel_lock(channel);
					sccp_channel_endcall_locked(channel);
					sccp_channel_unlock(channel);
				}
			}
			SCCP_LIST_UNLOCK(&line->channels);

			/* remove devices from line */
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Remove Line %s from device %s\n", line->name, d->id);
			sccp_line_removeDevice(line, d);
		}
		config->instance = 0;						/* reset button configuration to rebuild template on register */
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	d->linesRegistered = FALSE;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Unregister Device %s\n", d->id);
	sccp_event_t *event = sccp_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));
	event->type = SCCP_EVENT_DEVICE_UNREGISTERED;
	event->event.deviceRegistered.device = d;
	sccp_event_fire((const sccp_event_t **)&event);
	
	/* cleanup statistics */
	memset(&d->configurationStatistic, 0, sizeof(d->configurationStatistic));
	
	d->mwilight = 0; /* cleanup mwi status */

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

	if (d->session) {
		sccp_session_lock(d->session);
		d->session->device = NULL;
		sccp_session_unlock(d->session);
	}

	d->session = NULL;
	if (d->buttonTemplate) {
		sccp_free(d->buttonTemplate);
		d->buttonTemplate = NULL;
	}
#ifdef CS_DEVSTATE_FEATURE
	/* Unregister event subscriptions originating from devstate feature */
	SCCP_LIST_LOCK(&d->devstateSpecifiers);
	while ((devstateSpecifier = SCCP_LIST_REMOVE_HEAD(&d->devstateSpecifiers, list))) {
		if (devstateSpecifier->sub){
			pbx_event_unsubscribe(devstateSpecifier->sub);
		}
		sccp_log(DEBUGCAT_FEATURE_BUTTON) (VERBOSE_PREFIX_1 "%s: Removed Devicestate Subscription: %s\n", d->id, devstateSpecifier->specifier);
	}
	SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
#endif

	sccp_device_unlock(d);

	if (remove_from_global) {
#if CS_EXPERIMENTAL	//refcount
		// don't need scheduled destroy when using refcount
		sccp_device_destroy(d);
		d = NULL;
#else
		if (cleanupTime > 0) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Device planned to be free'd in %d secs.\n", d->id, cleanupTime);
			if ((d->scheduleTasks.free = sccp_sched_add(cleanupTime * 1000, sccp_device_destroy, d)) < 0) {
				sleep(cleanupTime);
				sccp_device_destroy(d);
				d = NULL;
			}
		} else {
			sccp_device_destroy(d);
			d = NULL;
		}
#endif
		return;
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
 * \lock
 * 	- device
 * 	  - device->buttonconfig
 * 	  - device->permithosts
 * 	  - device->devstateSpecifiers
 */
int __sccp_device_destroy(const void *ptr)
{
	sccp_device_t *d = (sccp_device_t *) ptr;
	sccp_buttonconfig_t *config = NULL;
	sccp_hostname_t *permithost = NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Destroy Device\n", d->id);
#if CS_EXPERIMENTAL
	sccp_mutex_lock(&d->lock);		// using real device lock while using refcount
#else
	sccp_device_lock(d);
#endif
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
		if (permithost)
			sccp_free(permithost);
	}
	SCCP_LIST_UNLOCK(&d->permithosts);
	SCCP_LIST_HEAD_DESTROY(&d->permithosts);

#ifdef CS_DEVSTATE_FEATURE
	/* removing devstate_specifier */
	sccp_devstate_specifier_t *devstateSpecifier;
	SCCP_LIST_LOCK(&d->devstateSpecifiers);
	while ((devstateSpecifier = SCCP_LIST_REMOVE_HEAD(&d->devstateSpecifiers, list))) {
		if (devstateSpecifier)
			sccp_free(devstateSpecifier);
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
	int i;
	for(i=0;i< SCCP_MAX_MESSAGESTACK;i++) { 
		if(d && d->messageStack && d->messageStack[i] != NULL){
			sccp_free(d->messageStack[i]);
		}
	}

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Device Destroyed\n", d->id);
	pbx_mutex_destroy(&d->lock);
#if CS_EXPERIMENTAL
	sccp_mutex_unlock(&d->lock);		// using real device lock while using refcount
#else
	sccp_device_unlock(d);
#endif
#if !CS_EXPERIMENTAL	// refcount
	sccp_free(d);	// moved to sccp_release
#endif
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
 * \lock
 * 	- device
 * 	  - device->buttonconfig
 * 	  - device->permithosts
 * 	  - device->devstateSpecifiers
 */
int sccp_device_destroy(const void *ptr)
{
#if CS_EXPERIMENTAL	// refcount
	sccp_device_t *d = (sccp_device_t *) ptr;
	sccp_device_release(d);
	return 0;
#else
	return __sccp_device_destroy(ptr);
#endif	
}

/*!
 * \brief is Video Support on a Device
 * \param device SCCP Device
 * \return result as boolean_t
 */
boolean_t sccp_device_isVideoSupported(const sccp_device_t * device)
{
  
	
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "%s: video support %d \n", device->id, device->capabilities.video[0]);
#ifdef CS_SCCP_VIDEO
	if (device->capabilities.video[0] != 0)
		return TRUE;
#endif
	return FALSE;
}

/*!
 * \brief Find ServiceURL by index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \return SCCP Service
 * 
 * \lock
 * 	- device->buttonconfig
 */
sccp_buttonconfig_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance)
{
	sccp_buttonconfig_t *config = NULL;

	if (!d || !d->session)
		return NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for service with instance %d\n", d->id, instance);
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log(((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE) + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: instance: %d buttontype: %d\n", d->id, config->instance, config->type);

		if (config->type == SERVICE && config->instance == instance) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: found service: %s\n", d->id, config->label);
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
 * 	- device->buttonconfig is not always locked
 */
uint8_t sccp_device_find_index_for_line(const sccp_device_t * d, const char *lineName)
{
	sccp_buttonconfig_t *config;

	if (!d || !lineName)
		return -1;

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: sccp_device_find_index_for_line searching for %s\n", DEV_ID_LOG(d), lineName);
	/* device is already locked by parent function */
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == LINE && (config->button.line.name) && !strcasecmp(config->button.line.name, lineName)) {
			sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: sccp_device_find_index_for_line found: %d\n", DEV_ID_LOG(d), config->instance);
			break;
		}
	}

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: sccp_device_find_index_for_line return: %d\n", DEV_ID_LOG(d), config ? config->instance : -2);
	return (config) ? config->instance : -2;
}

/*!
 * \brief Send Reset to a Device
 * \param d SCCP Device
 * \param reset_type as int
 * \return Status as int
 */
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type)
{
	sccp_moo_t *r;

	if (!d)
		return 0;

	REQ(r, Reset);
	r->msg.Reset.lel_resetType = htolel(reset_type);
	sccp_session_send(d, r);
	return 1;
}

/*!
 * \brief Send Call State to Device
 * \param d SCCP Device
 * \param instance Instance as int
 * \param callid Call ID as int
 * \param state Call State as int
 * \param priority Priority as skinny_callPriority_t
 * \param visibility Visibility as skinny_callinfo_visibility_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state, skinny_callPriority_t priority, skinny_callinfo_visibility_t visibility)
{
	sccp_moo_t *r;

	if (!d)
		return;
	REQ(r, CallStateMessage);
	r->msg.CallStateMessage.lel_callState = htolel(state);
	r->msg.CallStateMessage.lel_lineInstance = htolel(instance);
	r->msg.CallStateMessage.lel_callReference = htolel(callid);
	r->msg.CallStateMessage.lel_visibility = htolel(visibility);
	r->msg.CallStateMessage.lel_priority = htolel(priority);
	/*r->msg.CallStateMessage.lel_unknown3 = htolel(2); */
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send and Set the call state %s(%d) on call %d\n", d->id, sccp_callstate2str(state), state, callid);
}

/*!
 * \brief Get the number of channels that the device owns
 * \param device sccp device
 * \note device should be locked by parent functions
 * 
 * \warning
 * 	- device-buttonconfig is not always locked
 *
 * \lock
 * 	- line->channels
 */
uint8_t sccp_device_numberOfChannels(const sccp_device_t * device)
{
	sccp_buttonconfig_t *config;
	sccp_channel_t *c;
	sccp_line_t *l;
	uint8_t numberOfChannels = 0;

	if (!device) {
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "device is null\n");
		return 0;
	}

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == LINE) {
			l = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if (!l)
				continue;

			SCCP_LIST_LOCK(&l->channels);
			SCCP_LIST_TRAVERSE(&l->channels, c, list) {
				if ( sccp_channel_getDevice(c) == device)
					numberOfChannels++;
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
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	if (digit == '*') {
		digit = 0xe;							/* See the definition of tone_list in chan_protocol.h for more info */
	} else if (digit == '#') {
		digit = 0xf;
	} else if (digit == '0') {
		digit = 0xa;							/* 0 is not 0 for cisco :-) */
	} else {
		digit -= '0';
	}

	if (digit > 16) {
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: SCCP phones can't play this type of dtmf. Sending it inband\n", d->id);
		return;
	}

	REQ(r, KeypadButtonMessage);
	r->msg.KeypadButtonMessage.lel_kpButton = htolel(digit);
	r->msg.KeypadButtonMessage.lel_lineInstance = htolel(line);
	r->msg.KeypadButtonMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, r);

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: (sccp_dev_keypadbutton) Sending keypad '%02X'\n", DEV_ID_LOG(d), digit);
}


/*!
 * \brief Indicate to device that remote side has been put on hold (old).
 */
static void sccp_device_old_indicate_remoteHold(const sccp_device_t *device, uint8_t lineInstance, uint8_t callid, uint8_t callPriority, uint8_t callPrivacy){
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_HOLD, callPriority, callPrivacy);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOLD);
	sccp_dev_displayprompt(device, lineInstance, callid, SKINNY_DISP_HOLD, 0);
}
/*!
 * \brief Indicate to device that remote side has been put on hold (new).
 */
static void sccp_device_new_indicate_remoteHold(const sccp_device_t *device, uint8_t lineInstance, uint8_t callid, uint8_t callPriority, uint8_t callPrivacy){
	sccp_device_sendcallstate(device, lineInstance, callid, SKINNY_CALLSTATE_HOLDRED, callPriority, callPrivacy);
	sccp_dev_set_keyset(device, lineInstance, callid, KEYMODE_ONHOLD);
	sccp_dev_displayprompt(device, lineInstance, callid, SKINNY_DISP_HOLD, 0);
}


/*!
 * \brief Add message to the MessageStack to be shown on the Status Line of the SCCP Device
 */
void sccp_device_addMessageToStack(sccp_device_t *device, const uint8_t priority, const char *message){
	
	if(ARRAY_LEN(device->messageStack) <= priority)
		return;
	
	/** alredy a message for this priority */
	if(device->messageStack[priority]){
		/** message is not the same */
		if(strcasecmp(device->messageStack[priority], message)){
			sccp_free(device->messageStack[priority]);
		}else{
			return;
		}
	}
	
	device->messageStack[priority] = strdup(message);
	sccp_dev_check_displayprompt(device);
}

/*!
 * \brief Remove a message from the MessageStack to be shown on the Status Line of the SCCP Device
 */
void sccp_device_clearMessageFromStack(sccp_device_t *device, const uint8_t priority){
	if(ARRAY_LEN(device->messageStack) <= priority)
		return;
	
	pbx_log(LOG_NOTICE, "%s: clear message stack %d\n", DEV_ID_LOG(device), priority);
	if(device->messageStack[priority]){
		sccp_free(device->messageStack[priority]);
		device->messageStack[priority] = NULL;
		
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
 * 	- device->buttonconfig is not always locked
 * 	- line->devices is not always locked
 * 
 * \lock
 * 	- device
 *
 * \todo implement cfwd_noanswer
 */
void sccp_device_featureChangedDisplay(const sccp_event_t ** event)
{
	sccp_buttonconfig_t *config;
	sccp_line_t *line;
	sccp_linedevices_t *lineDevice;
	sccp_device_t *device = (*event)->event.featureChanged.device;
	
	char tmp[256] = { 0 };
	size_t len = sizeof(tmp);
	char *s = tmp;

	if (!(*event) || !device)
		return;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Received Feature Change Event: %s(%d)\n", DEV_ID_LOG(device), featureType2str((*event)->event.featureChanged.featureType), (*event)->event.featureChanged.featureType);

	switch ((*event)->event.featureChanged.featureType) {
	case SCCP_FEATURE_CFWDNONE:
		sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_CFWD);
		break;
	case SCCP_FEATURE_CFWDBUSY:
	case SCCP_FEATURE_CFWDALL:
		SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
			if (config->type == LINE) {
				line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
				if (line) {
					SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
						if (lineDevice->device != device)
							continue;

						uint8_t instance = sccp_device_find_index_for_line(device, line->name);

						sccp_dev_forward_status(line, instance, device);
						switch((*event)->event.featureChanged.featureType) {
							case SCCP_FEATURE_CFWDALL:
								if (lineDevice->cfwdAll.enabled) {								
									/* build disp message string */
									if (s != tmp)
										pbx_build_string(&s, &len, ", ");
									pbx_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDALL, line->cid_num, SKINNY_DISP_FORWARDED_TO, lineDevice->cfwdAll.number);
								}
								break;	
							case SCCP_FEATURE_CFWDBUSY:
								if (lineDevice->cfwdBusy.enabled) {								
									/* build disp message string */
									if (s != tmp)
										pbx_build_string(&s, &len, ", ");
									pbx_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDBUSY, line->cid_num, SKINNY_DISP_FORWARDED_TO, lineDevice->cfwdBusy.number);
								}
								break;
							default:
								break;
						}
					}
				}
			}
		}
		
		if(strlen(tmp) > 0){
			sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_CFWD, tmp);
		}else{
			sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_CFWD);
		}
		
		break;
	case SCCP_FEATURE_DND:
	  
		if (!device->dndFeature.status) {
			sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_DND);
		} else {
			if (device->dndFeature.status == SCCP_DNDMODE_SILENT){
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_DND, ">>> " SKINNY_DISP_DND " (Silent) <<<");
			}else{
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_DND, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_BUSY ") <<<");
			}
		}
		break;
	case SCCP_FEATURE_PRIVACY:
		if (TRUE==device->privacyFeature.status) {
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_PRIVACY,  SKINNY_DISP_PRIVATE);
		} else {
				sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_PRIVACY);
		}
		break;
	case SCCP_FEATURE_MONITOR:
		if (TRUE==device->monitorFeature.status) {
				sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_MONITOR,  SKINNY_DISP_MONITOR);
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
static sccp_push_result_t sccp_device_pushURL(const sccp_device_t *device, const char *url, uint8_t priority, uint8_t tone){
	char xmlData[512];
	sprintf(xmlData, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\"URL=\"%s\"/></CiscoIPPhoneExecute>", url);
	
	device->protocol->sendUserToDeviceDataVersionMessage(device, xmlData, priority);
	if(SKINNY_TONE_SILENCE != tone){
		sccp_dev_starttone(device, tone, 0, 0, 0);
	}
	return SCCP_PUSH_RESULT_SUCCESS;
}

/*!
 * \brief Push a Text Message to an SCCP device
 */
static sccp_push_result_t sccp_device_pushTextMessage(const sccp_device_t *device, const char *messageText, const char *from, uint8_t priority, uint8_t tone){
	char xmlData[1024];
	sprintf(xmlData, "<CiscoIPPhoneText><Title>%s</Title><Text>%s</Text></CiscoIPPhoneText>", from ? from : "", messageText);
	
	device->protocol->sendUserToDeviceDataVersionMessage(device, xmlData, priority);
	
	if(SKINNY_TONE_SILENCE != tone){
		sccp_dev_starttone(device, tone, 0, 0, 0);
	}
	return SCCP_PUSH_RESULT_SUCCESS;
}
