
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
#define  REF_DEBUG 1

/*!
 * \brief Check device ipaddress against the ip ACL (permit/deny and permithosts entries)
 */
boolean_t sccp_device_checkACL(sccp_device_t *device, sccp_session_t *session)
{
	struct sockaddr_in sin;
	boolean_t matchesACL = FALSE;

        if (!device || !session)
                return FALSE;

	/* get current socket information */
	memcpy(&sin, &session->sin, sizeof(struct sockaddr_in));
	
	/* no permit deny information */
	if(!device->ha){
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: no deny/permit information for this device, allow all connections\n", device->id);
		return TRUE;
	}
  
	if (ast_apply_ha(device->ha, &sin) != AST_SENSE_ALLOW) {
	  
		// checking permithosts	
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: not allowed by deny/permit list. Checking permithost list...\n", device->id);

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
  
	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: checkACL returning %s\n", device->id, matchesACL ? "TRUE" : "FALSE");
	return matchesACL;
}

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

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
		sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "%s: Setting Device to Pending Delete=1\n", d->id);
		ast_free_ha(d->ha);
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

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "Device %s needs to be reset because of a change in sccp.conf\n", d->id);
	sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);

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
				ast_free(buttonconfig);
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

	SCCP_RWLIST_RDLOCK(&GLOB(devices));
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
	sccp_device_t *d = ast_calloc(1, sizeof(sccp_device_t));

	if (!d) {
		sccp_log(0) (VERBOSE_PREFIX_3 "Unable to allocate memory for a device\n");
		return NULL;
	}
	ast_mutex_init(&d->lock);

	d = sccp_device_applyDefaults(d);

	SCCP_LIST_HEAD_INIT(&d->buttonconfig);
	SCCP_LIST_HEAD_INIT(&d->selectedChannels);
	SCCP_LIST_HEAD_INIT(&d->addons);

#if ASTERISK_VERSION_NUMBER >= 10600 
	ast_append_ha("permit", "127.0.0.0/255.0.0.0", d->ha, NULL);
	ast_append_ha("permit", "10.0.0.0/255.0.0.0", d->ha, NULL);
	ast_append_ha("permit", "172.16.0.0/255.224.0.0", d->ha, NULL);
	ast_append_ha("permit", "192.168.0.0/255.255.0.0", d->ha, NULL);
#else 
	ast_append_ha("permit", "127.0.0.0/255.0.0.0", d->ha);
	ast_append_ha("permit", "10.0.0.0/255.0.0.0", d->ha);
	ast_append_ha("permit", "172.16.0.0/255.224.0.0", d->ha);
	ast_append_ha("permit", "192.168.0.0/255.255.0.0", d->ha);
#endif	
	return d;
}

/*!
 * \brief Apply Device Defaults
 * \param d SCCP Device
 * \return SCCP Device with default values
 *
 * \callgraph
 * \callergraph
 */
sccp_device_t *sccp_device_applyDefaults(sccp_device_t * d)
{
	if (!d)
		return NULL;

	d->linesCount = 0;
	d->accessoryused = 0;
	d->realtime = FALSE;
	d->accessorystatus = 0;
	d->keepalive = GLOB(keepalive);
	d->tz_offset = 0;
	d->defaultLineInstance = 0;
	d->capability = GLOB(global_capability);
	d->codecs = GLOB(global_codecs);
	d->transfer = 1;
	d->state = SCCP_DEVICESTATE_ONHOOK;
	d->trustphoneip = GLOB(trustphoneip);
	d->privacyFeature.enabled = TRUE;
	d->monitorFeature.enabled = TRUE;
	d->overlapFeature.enabled = GLOB(useoverlap);
	d->dndFeature.enabled = TRUE;
	d->earlyrtp = GLOB(earlyrtp);
	d->mwilamp = GLOB(mwilamp);
	d->mwioncall = GLOB(mwioncall);
	d->cfwdall = GLOB(cfwdall);
	d->cfwdbusy = GLOB(cfwdbusy);
	d->cfwdnoanswer = GLOB(cfwdnoanswer);
	d->postregistration_thread = AST_PTHREADT_STOP;
	d->nat = GLOB(nat);
	d->directrtp = GLOB(directrtp);
	d->digittimeout = GLOB(digittimeout);
#ifdef CS_SCCP_PICKUP
	d->pickupexten = 0;
	d->pickupcontext = NULL;
#endif

#ifdef CS_SCCP_PARK
	d->park = 1;
#else
	d->park = 0;
#endif
	d->meetme = GLOB(meetme);
	sccp_copy_string(d->meetmeopts, GLOB(meetmeopts), sizeof(d->meetmeopts));

	/* reset statistic */
	d->configurationStatistic.numberOfLines = 0;
	d->configurationStatistic.numberOfSpeeddials = 0;
	d->configurationStatistic.numberOfFeatures = 0;

	d->softKeyConfiguration.modes = (softkey_modes *) SoftKeyModes;
	d->softKeyConfiguration.size = sizeof(SoftKeyModes) / sizeof(softkey_modes);

#ifdef CS_ADV_FEATURES
	d->useRedialMenu = FALSE;
#endif

	return d;
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
 * \brief Get Codec for Device
 * \param ast Asterisk Channel
 * \return CodecCapability as int
 * 
 * \called_from_asterisk
 */
int sccp_device_get_codec(struct ast_channel *ast)
{
	sccp_channel_t *c = NULL;

#if 0										// (DD)
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Asterisk requested available codecs for channel %s\n", ast->name);
#endif

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Couldn't find a channel pvt struct. Returning global capabilities\n");
		return GLOB(global_capability);
	}

	if (!c->device) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Couldn't find a device associated to channel. Returning global capabilities\n");
	}

	char s1[512];

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) capabilities are %s (%d)\n", pbx_getformatname_multiple(s1, sizeof(s1) - 1, c->capability), c->capability);

	return c->capability;
}

/*!
 * \brief Create a template of Buttons as Definition for a Phonetype (d->skinny_type)
 * \param d device
 * \param btn buttonlist
 */
void sccp_dev_build_buttontemplate(sccp_device_t * d, btnlist * btn)
{
	uint8_t i;

	if (!d || !d->session)
		return;
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
		for (i = 0; i < 2 + sccp_addons_taps(d); i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7920:
	case SKINNY_DEVICETYPE_CISCO7921:
	case SKINNY_DEVICETYPE_CISCO7925:
	case SKINNY_DEVICETYPE_CISCO7985:
		for (i = 0; i < 6; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO8941:
	case SKINNY_DEVICETYPE_CISCO8945:
		for (i = 0; i < 20; i++)
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_CISCO7960:
	case SKINNY_DEVICETYPE_CISCO7961:
	case SKINNY_DEVICETYPE_CISCO7961GE:
	case SKINNY_DEVICETYPE_CISCO7962:
	case SKINNY_DEVICETYPE_CISCO7965:
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
		}
		break;
	case SKINNY_DEVICETYPE_NOKIA_ICC:
		(btn++)->type = SCCP_BUTTONTYPE_MULTI;
		break;
	case SKINNY_DEVICETYPE_NOKIA_E_SERIES:
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
	case SKINNY_DEVICETYPE_SPA_521S:
		break;
	case SKINNY_DEVICETYPE_SPA_525G:
		break;
	case SKINNY_DEVICETYPE_CISCO6901:
	case SKINNY_DEVICETYPE_CISCO6911:
                (btn++)->type = SCCP_BUTTONTYPE_MULTI;
                break;
	case SKINNY_DEVICETYPE_CISCO6921:
//		(btn++)->type = SKINNY_BUTTONTYPE_TRANSFER;
//		(btn++)->type = SKINNY_BUTTONTYPE_VOICEMAIL;
//		(btn++)->type = SKINNY_BUTTONTYPE_CONFERENCE;
		for (i = 0; i < 2; i++) {
                        (btn++)->type = SCCP_BUTTONTYPE_MULTI;
                }
		break;
	case SKINNY_DEVICETYPE_CISCO6941:
	case SKINNY_DEVICETYPE_CISCO6945:
		for (i = 0; i < 20; i++) {
                	(btn++)->type = SCCP_BUTTONTYPE_MULTI;
                }
		break;
	case SKINNY_DEVICETYPE_CISCO6961:
		for (i = 0; i < 12; i++) {
                        (btn++)->type = SCCP_BUTTONTYPE_MULTI;
                }
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
	sccp_moo_t *r = ast_malloc(sizeof(sccp_moo_t));

	if (!r) {
		ast_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
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
	} else {
		ast_log(LOG_WARNING, "%s: >> (sccp_dev_send) No Session available to send '%s'\n", d->id, message2str(letohl(r->lel_messageId)));
		return -1;
	}
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

	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: (sccp_dev_set_registered) Setting Registered Status for Device from %s to %s\n", DEV_ID_LOG(d), deviceregistrationstatus2str(d->registrationState), deviceregistrationstatus2str(opt));

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
	}
}

/*!
 * \brief Sets the SCCP Device's SoftKey Mode Specified by opt
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID as uint8_t
 * \param opt KEYMODE of KEYMODE_*
 * \todo Disable DirTrfr by Default
 */
void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt)
{
	sccp_moo_t *r;

	if (!d)
		return;

	if (!d->softkeysupport)
		return;								/* the device does not support softkeys */

	/*let's activate the transfer */
	if (opt == KEYMODE_CONNECTED)
		opt = ((d->conference_channel) ? KEYMODE_CONNCONF : (d->transfer) ? KEYMODE_CONNTRANS : KEYMODE_CONNECTED);

	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance = htolel(line);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(opt);

	r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF;		/* htolel(65535); */

	if ((opt == KEYMODE_ONHOOK || opt == KEYMODE_OFFHOOK || opt == KEYMODE_OFFHOOKFEAT)
	    && (sccp_strlen_zero(d->lastNumber)
#ifdef CS_ADV_FEATURES
		&& !d->useRedialMenu
#endif
	    )
	    ) {
		r->msg.SelectSoftKeysMessage.les_validKeyMask &= htolel(~(1 << 0));
	}

	sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, keymode2str(opt), opt, line, callid);
	sccp_dev_send(d, r);
}

/*!
 * \brief Set Ringer on Device
 * \param d SCCP Device
 * \param opt Option as uint8_t
 * \param lineInstance LineInstance as uint32_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_set_ringer(const sccp_device_t * d, sccp_ringermode_t opt, uint8_t lineInstance, uint32_t callid)
{
	sccp_moo_t *r;

	REQ(r, SetRingerMessage);
	r->msg.SetRingerMessage.lel_ringMode = htolel(opt);
	/* Note that for distinctive ringing to work with the higher protocol versions
	   the following actually needs to be set to 1 as the original comment says.
	   Curiously, the variable is not set to 1 ... */
	r->msg.SetRingerMessage.lel_ringerType = htolel(SKINNY_RINGERTYPE_RING_CONTINUOUS);	/* always 1 */
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
void sccp_dev_starttone(sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

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
 * \brief Send Clear Prompt to Device
 * \param d SCCP Device
 * \param lineInstance LineInstance as uint8_t
 * \param callid Call ID uint32_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_clearprompt(const sccp_device_t * d, uint8_t lineInstance, uint32_t callid)
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
 * \param line Line as uint8_t
 * \param callid Call ID uint32_t
 * \param msg Msg as char
 * \param timeout Timeout as int
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char *msg, int timeout)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

	if (d->inuseprotocolversion < 7) {
		REQ(r, DisplayPromptStatusMessage);
		r->msg.DisplayPromptStatusMessage.lel_messageTimeout = htolel(timeout);
		r->msg.DisplayPromptStatusMessage.lel_callReference = htolel(callid);
		r->msg.DisplayPromptStatusMessage.lel_lineInstance = htolel(line);
		sccp_copy_string(r->msg.DisplayPromptStatusMessage.promptMessage, msg, sizeof(r->msg.DisplayPromptStatusMessage.promptMessage));
	} else {
		int msg_len = strlen(msg);

		int hdr_len = sizeof(r->msg.DisplayDynamicPromptStatusMessage) - 3;

		int padding = ((msg_len + hdr_len) % 4);

		padding = (padding > 0) ? 4 - padding : 0;
		r = sccp_build_packet(DisplayDynamicPromptStatusMessage, hdr_len + msg_len + padding);
		r->msg.DisplayDynamicPromptStatusMessage.lel_messageTimeout = htolel(timeout);
		r->msg.DisplayDynamicPromptStatusMessage.lel_callReference = htolel(callid);
		r->msg.DisplayDynamicPromptStatusMessage.lel_lineInstance = htolel(line);
		memcpy(&r->msg.DisplayDynamicPromptStatusMessage.dummy, msg, msg_len);
	}
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", d->id, line, callid, timeout);
}

/*!
 * \brief Send Clear Display to Device
 * \param d SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_cleardisplay(sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearDisplay);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Clear the display\n", d->id);
}

/*!
 * \brief Send Display to Device
 * \param d SCCP Device
 * \param msg Msg as char
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_display(sccp_device_t * d, char *msg)
{
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
void sccp_dev_cleardisplaynotify(sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: Clear the display notify message\n", d->id);
}

/*!
 * \brief Send Display Notification to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param timeout Timeout as uint32_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_displaynotify(sccp_device_t * d, char *msg, uint32_t timeout)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;
	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

	REQ(r, DisplayNotifyMessage);
	r->msg.DisplayNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Display notify with timeout %d\n", d->id, timeout);
}

/*!
 * \brief Send Clear Display Notification to Device
 * \param d SCCP Device
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_cleardisplayprinotify(sccp_device_t * d)
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
 * \param priority Priority as uint32_t
 * \param timeout Timeout as uint32_t
 *
 * \callgraph
 * \callergraph
 */
void sccp_dev_displayprinotify(sccp_device_t * d, char *msg, uint32_t priority, uint32_t timeout)
{
	sccp_moo_t *r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type == SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type, "kirk")))
		return;								/* only for telecaster and new phones */

	if (!msg || sccp_strlen_zero(msg))
		return;

	REQ(r, DisplayPriNotifyMessage);
	r->msg.DisplayPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayPriNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayPriNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
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
			if (type == SCCP_BUTTONTYPE_HINT && !sccp_is_nonempty_string(config->button.speeddial.hint))
				continue;

			k = ast_malloc(sizeof(sccp_speed_t));
			memset(k, 0, sizeof(sccp_speed_t));

			k->instance = instance;
			k->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			sccp_copy_string(k->name, config->button.speeddial.label, sizeof(k->name));
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

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send the active line %s\n", DEV_ID_LOG(device), l->name);
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
	char tmp[256] = "";

	int timeout = 0;

	if (!d || !d->session)
		return;

	sccp_dev_clearprompt(d, 0, 0);

	if (d->phonemessage) {							// display message if set
		sccp_dev_displayprompt(d, 0, 0, d->phonemessage, 0);
		goto OUT;
	}

	/* check for forward to display */
	if (sccp_dev_display_cfwd(d, FALSE) == TRUE)
		goto OUT;

	if (d->dndFeature.enabled && d->dndFeature.status) {
		if (d->dndFeature.status == SCCP_DNDMODE_REJECT)
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_BUSY ") <<<", 0);

		else if (d->dndFeature.status == SCCP_DNDMODE_SILENT)
			/* no internal label for the silent string */
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (Silent) <<<", 0);
		else
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " <<<", 0);

		goto OUT;
	}

	if (!pbx_db_get("SCCP/message", "timeout", tmp, sizeof(tmp))) {
		sscanf(tmp, "%i", &timeout);
	}
	if (!pbx_db_get("SCCP/message", "text", tmp, sizeof(tmp))) {
		if (!sccp_strlen_zero(tmp)) {
			sccp_dev_displayprinotify(d, tmp, 5, timeout);
			goto OUT;
		}
	}

	if (d->mwilight) {
		char buffer[StationMaxDisplayTextSize];

		sprintf(buffer, "%s: (%d/%d)", SKINNY_DISP_YOU_HAVE_VOICEMAIL, d->voicemailStatistic.newmsgs, d->voicemailStatistic.oldmsgs);
		sccp_dev_displayprinotify(d, buffer, 5, 10);
		goto OUT;
	}

	/* when we are here, there's nothing to display */
	sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOUR_CURRENT_OPTIONS, 0);
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
			ast_log(LOG_ERROR, "%s: Failed to allocate SCCP channel.\n", d->id);
			return;
		}
		sccp_channel_unlock(chan);
	} else if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		// If the device is currently offhook, then we need to ...

		/*! \todo
	 	 * (1) Put current call on hold 
	 	 * (2) Stop transmitting/receiving 
		 */

		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Selecting line %s, while Line %s in-use\n", d->id, wanted->name, current->name);
	} else {
		// Otherwise, just select the callplane
		ast_log(LOG_WARNING, "%s: Unknown status while trying to select line %s.  Current line is %s\n", d->id, wanted->name, current->name);
	}
}

/*!
 * \brief Display Call Forward
 */
boolean_t sccp_dev_display_cfwd(sccp_device_t * device, boolean_t force)
{
	boolean_t ret = TRUE;
	char tmp[256] = { 0 };
	size_t len = sizeof(tmp);

	char *s = tmp;

	sccp_line_t *line = NULL;

	sccp_linedevices_t *ld = NULL;

	/* List every forwarded lines on the device prompt. */
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), line, list) {
		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, ld, list) {
			if (ld->device == device) {
				if (s != tmp)
					ast_build_string(&s, &len, ", ");
				if (ld->cfwdAll.enabled) {
					ast_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDALL, line->cid_num, SKINNY_DISP_FORWARDED_TO, ld->cfwdAll.number);
				} else if (ld->cfwdBusy.enabled) {
					ast_build_string(&s, &len, "%s:%s %s %s", SKINNY_DISP_CFWDBUSY, line->cid_num, SKINNY_DISP_FORWARDED_TO, ld->cfwdBusy.number);
				}
			}
		}
		SCCP_LIST_UNLOCK(&line->devices);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	/* There isn't any forward on device's lines. */
	if (s == tmp) {
		ret = FALSE;
		if (force) {
			/* Send an empty message to hide the message. */
			tmp[0] = ' ';
			tmp[1] = '\0';
		}
	}
	sccp_dev_displayprompt(device, 0, 0, tmp, 0);

	return ret;
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
	sccp_moo_t *r1 = NULL;

	sccp_linedevices_t *linedevice = NULL;

	if (!device || !device->session)
		return;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send Forward Status.  Line: %s\n", device->id, l->name);
	REQ(r1, ForwardStatMessage);
	r1->msg.ForwardStatMessage.lel_lineNumber = htolel(lineInstance);

	linedevice = sccp_util_getDeviceConfiguration(device, l);

	/*! \todo check for forward status during registration -MC */
	if (!linedevice) {
		if (device->registrationState == SKINNY_DEVICE_RS_OK) {
			ast_log(LOG_NOTICE, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		}
	} else {
		r1->msg.ForwardStatMessage.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;
		if (linedevice->cfwdAll.enabled) {
			r1->msg.ForwardStatMessage.lel_cfwdallstatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdallnumber, linedevice->cfwdAll.number, sizeof(r1->msg.ForwardStatMessage.cfwdallnumber));
		} else if (linedevice->cfwdBusy.enabled) {
			r1->msg.ForwardStatMessage.lel_cfwdbusystatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(r1->msg.ForwardStatMessage.cfwdbusynumber));
		}

		sccp_dev_display_cfwd(device, TRUE);
	}
	sccp_dev_send(device, r1);

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Sent Forward Status.  Line: %s\n", device->id, l->name);

	//! \todo What to do with this lineStatusChanges in sccp_dev_forward_status
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

	sccp_log(2) (VERBOSE_PREFIX_3 "%s: Device registered; performing post registration tasks...\n", d->id);

	sccp_dev_check_displayprompt(d);

	// Post event to interested listeners (hints, mwi) that device was registered.
	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

	memset(event, 0, sizeof(sccp_event_t));

	event->type = SCCP_EVENT_DEVICE_REGISTERED;
	event->event.deviceRegistered.device = d;
	sccp_event_fire((const sccp_event_t **)&event);

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
	ast_db_del(family, "lastDialedNumber");
	if (!sccp_strlen_zero(d->lastNumber))
		pbx_db_put(family, "lastDialedNumber", d->lastNumber);

	/* unsubscribe hints *//* prevent loop:sccp_dev_clean =>
	   sccp_line_removeDevice =>
	   sccp_event_fire =>
	   sccp_hint_eventListener =>
	   sccp_hint_lineStatusChanged =>
	   sccp_hint_hintStatusUpdate =>
	   sccp_hint_notifySubscribers =>
	   sccp_dev_speed_find_byindex */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_2 "SCCP: Unregister Device %s\n", d->id);
	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

	memset(event, 0, sizeof(sccp_event_t));
	event->type = SCCP_EVENT_DEVICE_UNREGISTERED;
	event->event.deviceRegistered.device = d;
	sccp_event_fire((const sccp_event_t **)&event);

	/* hang up open channels and remove device from line */
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if (config->type == LINE) {
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if (!line)
				continue;

			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, channel, list) {
				if (channel->device == d) {
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

	/* removing addons */
	if (remove_from_global) {
		sccp_addons_clear(d);
	}

	/* removing selected channels */
	SCCP_LIST_LOCK(&d->selectedChannels);
	while ((selectedChannel = SCCP_LIST_REMOVE_HEAD(&d->selectedChannels, list))) {
		ast_free(selectedChannel);
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	if (d->session) {
		sccp_session_lock(d->session);
		d->session->device = NULL;
		sccp_session_unlock(d->session);
	}

	d->session = NULL;
	if (d->buttonTemplate) {
		ast_free(d->buttonTemplate);
		d->buttonTemplate = NULL;
	}
#ifdef CS_DEVSTATE_FEATURE
	/* Unregister event subscriptions originating from devstate feature */
	if (remove_from_global) {
		/* only remove the subscription if we destroy the device */
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&d->devstateSpecifiers, devstateSpecifier, list) {
			sccp_log(DEBUGCAT_FEATURE_BUTTON) (VERBOSE_PREFIX_1 "%s: Removed Devicestate Subscription: %s\n", d->id, devstateSpecifier->specifier);
			devstateSpecifier->sub = ast_event_unsubscribe(devstateSpecifier->sub);
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
	}
#endif

	sccp_device_unlock(d);

	if (remove_from_global) {
		if (cleanupTime > 0) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "%s: Device planned to be free'd in %d secs.\n", d->id, cleanupTime);
			if ((d->scheduleTasks.free = sccp_sched_add(sched, cleanupTime * 1000, sccp_device_destroy, d)) < 0) {
				sleep(cleanupTime);
				sccp_device_destroy(d);
				d = NULL;
			}
		} else {
			sccp_device_destroy(d);
			d = NULL;
		}
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
int sccp_device_destroy(const void *ptr)
{
	sccp_device_t *d = (sccp_device_t *) ptr;

	sccp_buttonconfig_t *config = NULL;

	sccp_hostname_t *permithost = NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Destroy Device\n", d->id);
	sccp_device_lock(d);

	/* remove button config */
	/* only generated on read config, so do not remove on reset/restart */
	SCCP_LIST_LOCK(&d->buttonconfig);
	while ((config = SCCP_LIST_REMOVE_HEAD(&d->buttonconfig, list))) {
		ast_free(config);
		config = NULL;
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	SCCP_LIST_HEAD_DESTROY(&d->buttonconfig);

	/* removing permithosts */
	SCCP_LIST_LOCK(&d->permithosts);
	while ((permithost = SCCP_LIST_REMOVE_HEAD(&d->permithosts, list))) {
		ast_free(permithost);
	}
	SCCP_LIST_UNLOCK(&d->permithosts);
	SCCP_LIST_HEAD_DESTROY(&d->permithosts);

#ifdef CS_DEVSTATE_FEATURE
	/* removing devstate_specifier */
	sccp_devstate_specifier_t *devstateSpecifier;

	SCCP_LIST_LOCK(&d->devstateSpecifiers);
	while ((devstateSpecifier = SCCP_LIST_REMOVE_HEAD(&d->devstateSpecifiers, list))) {
		ast_free(devstateSpecifier);
	}
	SCCP_LIST_UNLOCK(&d->devstateSpecifiers);
#endif

	/* destroy selected channels list */
	SCCP_LIST_HEAD_DESTROY(&d->selectedChannels);
	if (d->ha) {
		ast_free_ha(d->ha);
	}

	d->ha = NULL;

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: Device Destroyed\n", d->id);

	sccp_device_unlock(d);
	ast_mutex_destroy(&d->lock);
	ast_free(d);

	return 0;
}

/*!
 * \brief is Video Support on a Device
 * \param device SCCP Device
 * \return result as boolean_t
 */
boolean_t sccp_device_isVideoSupported(const sccp_device_t * device)
{
#ifdef CS_SCCP_VIDEO
	if (device->capability & AST_FORMAT_VIDEO_MASK) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We have video support\n", DEV_ID_LOG(device));
		return TRUE;
	}
#endif
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We have no video support\n", DEV_ID_LOG(device));
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
sccp_service_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance)
{
	sccp_service_t *service = NULL;

	sccp_buttonconfig_t *config = NULL;

	if (!d || !d->session)
		return NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: searching for service with instance %d\n", d->id, instance);
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log(((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE) + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: instance: %d buttontype: %d\n", d->id, config->instance, config->type);

		if (config->type == SERVICE && config->instance == instance) {
			service = ast_malloc(sizeof(sccp_service_t));
			memset(service, 0, sizeof(sccp_service_t));
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: found service: %s\n", d->id, config->button.service.label);

			sccp_copy_string(service->label, config->button.service.label, sizeof(service->label));
			sccp_copy_string(service->url, config->button.service.url, sizeof(service->url));

		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);

	return service;
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
				if (c->device == device)
					numberOfChannels++;
			}
			SCCP_LIST_UNLOCK(&l->channels);
		}
	}

	return numberOfChannels;
}

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

void  sccp_device_set_time_date(sccp_device_t *d)
{
	sccp_moo_t *r1;
	time_t timer = 0;
	struct tm *cmtime = NULL;
	
	if (!d)
	        return;

	REQ(r1, DefineTimeDate);

	/* modulate the timezone by full hours only */
	timer = time(0) + (d->tz_offset * 3600);
	cmtime = localtime(&timer);
	r1->msg.DefineTimeDate.lel_year = htolel(cmtime->tm_year + 1900);
	r1->msg.DefineTimeDate.lel_month = htolel(cmtime->tm_mon + 1);
	r1->msg.DefineTimeDate.lel_dayOfWeek = htolel(cmtime->tm_wday);
	r1->msg.DefineTimeDate.lel_day = htolel(cmtime->tm_mday);
	r1->msg.DefineTimeDate.lel_hour = htolel(cmtime->tm_hour);
	r1->msg.DefineTimeDate.lel_minute = htolel(cmtime->tm_min);
	r1->msg.DefineTimeDate.lel_seconds = htolel(cmtime->tm_sec);
	r1->msg.DefineTimeDate.lel_milliseconds = htolel(0);
	r1->msg.DefineTimeDate.lel_systemTime = htolel(timer);
	sccp_dev_send(d, r1);
	sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Send date/time\n", DEV_ID_LOG(d));
}

#ifdef CS_DYNAMIC_CONFIG

/*!
 * \brief Copy the structure content of one device to a new one
 * \param orig_device sccp device
 * \return new_device as sccp_device_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_line_find_byinstance()
 * 	  - see sccp_duplicate_device_buttonconfig_list()
 *	  - see sccp_duplicate_device_hostname_list()
 *	  - see sccp_duplicate_device_selectedchannel_list()
 *	  - see sccp_duplicate_device_addon_list()
 */
sccp_device_t *sccp_clone_device(sccp_device_t * orig_device)
{
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: Creating Clone (from %p)\n", orig_device->id, (void *)orig_device);

	sccp_device_t *new_device = ast_calloc(1, sizeof(sccp_device_t));

	sccp_device_lock(orig_device);
	memcpy(new_device, orig_device, sizeof(*new_device));

	memset(&new_device->lock, 0, sizeof(new_device->lock));
	ast_mutex_init(&new_device->lock);
	new_device->session = NULL;

	// ast_ha ha
	struct ast_ha *hal;							// not sure this construction will help

	hal = ast_duplicate_ha_list(orig_device->ha);
	new_device->ha = hal;

	// ast_variable variables
	struct ast_variable *v;

	new_device->variables = NULL;
	for (v = orig_device->variables; v; v = v->next) {
		struct ast_variable *new_v = pbx_variable_new(v);

		new_v->next = new_device->variables;
		new_device->variables = new_v;
	}

	// sccp_line_t     *currentLine
	sccp_buttonconfig_t *orig_buttonconfig = NULL;

	SCCP_LIST_TRAVERSE(&orig_device->buttonconfig, orig_buttonconfig, list) {
		if (orig_buttonconfig->type == LINE) {
			new_device->currentLine = sccp_line_find_byinstance(new_device, orig_buttonconfig->instance);
			if (new_device->currentLine) {
				break;
			}
		}
	}
	// features
	new_device->privacyFeature = orig_device->privacyFeature;
	new_device->overlapFeature = orig_device->overlapFeature;
	new_device->monitorFeature = orig_device->monitorFeature;
	new_device->dndFeature = orig_device->dndFeature;
	new_device->priFeature = orig_device->priFeature;
	new_device->mobFeature = orig_device->mobFeature;
	new_device->accessoryStatus = orig_device->accessoryStatus;
	new_device->configurationStatistic = orig_device->configurationStatistic;
	new_device->voicemailStatistic = orig_device->voicemailStatistic;

	// softKeyConfiguration
	new_device->softKeyConfiguration = orig_device->softKeyConfiguration;
	softkey_modes *skm = orig_device->softKeyConfiguration.modes;

	new_device->softKeyConfiguration.modes = skm;
	new_device->softKeyConfiguration.size = orig_device->softKeyConfiguration.size;

	// scheduleTasks
	new_device->scheduleTasks = orig_device->scheduleTasks;

/*!	\todo produces a memcpy fault when compiled --with-conference. Copy Function of this structure has to be build. */

/*
#ifdef CS_SCCP_CONFERENCE
	// sccp_conference	*conference
	new_device->conference = ast_calloc(1,sizeof(sccp_conference_t));
	memcpy(new_device->conference,orig_device->conference,sizeof(sccp_conference_t));
#endif
*/

	// btnlist              *buttonTemplate
//      new_device->buttonTemplate = ast_calloc(1,sizeof(btnlist));
//      memcpy(new_device->btnTemplate,orig_device->btnTemplate,sizeof(btnlist));

	// char                 *pickupcontext
#    ifdef CS_SCCP_PICKUP
	if (orig_device->pickupcontext)
		new_device->pickupcontext = sccp_strdup(orig_device->pickupcontext);
#    endif

	// char                 *phonemessage
	if (orig_device->phonemessage)
		new_device->phonemessage = sccp_strdup(orig_device->phonemessage);

	// copy list-items over
	sccp_duplicate_device_buttonconfig_list(new_device, orig_device);
	sccp_duplicate_device_hostname_list(new_device, orig_device);
	sccp_duplicate_device_selectedchannel_list(new_device, orig_device);
	sccp_duplicate_device_addon_list(new_device, orig_device);

	sccp_device_unlock(orig_device);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: Clone Created (%p)\n", new_device->id, (void *)new_device);
	return new_device;
}

/*!
 * Copy the list of buttonconfig from another device
 * \param new_device original sccp device to which the list is copied
 * \param orig_device original sccp device from which to copy the list
 * 
 * \note	orig_device locked by parent function
 *
 * \lock
 * 	- device->buttonconfig
 */
void sccp_duplicate_device_buttonconfig_list(sccp_device_t * new_device, sccp_device_t * orig_device)
{
	sccp_buttonconfig_t *orig_buttonconfig = NULL;

	sccp_buttonconfig_t *new_buttonconfig = NULL;

	SCCP_LIST_HEAD_INIT(&new_device->buttonconfig);
	SCCP_LIST_LOCK(&orig_device->buttonconfig);
	SCCP_LIST_TRAVERSE(&orig_device->buttonconfig, orig_buttonconfig, list) {
		new_buttonconfig = ast_calloc(1, sizeof(sccp_buttonconfig_t));
		memcpy(new_buttonconfig, orig_buttonconfig, sizeof(*new_buttonconfig));
		SCCP_LIST_INSERT_TAIL(&new_device->buttonconfig, new_buttonconfig, list);
	}
	SCCP_LIST_UNLOCK(&orig_device->buttonconfig);
}

/*!
 * Copy the list of permitshosts from another device
 * \param new_device original sccp device to which the list is copied
 * \param orig_device original sccp device from which to copy the list
 * 
 * \note	orig_device locked by parent function
 *
 * \lock
 * 	- device->permithosts
 */
void sccp_duplicate_device_hostname_list(sccp_device_t * new_device, sccp_device_t * orig_device)
{
	sccp_hostname_t *orig_permithost = NULL;

	sccp_hostname_t *new_permithost = NULL;

	SCCP_LIST_HEAD_INIT(&new_device->permithosts);
	SCCP_LIST_LOCK(&orig_device->permithosts);
	SCCP_LIST_TRAVERSE(&orig_device->permithosts, orig_permithost, list) {
		new_permithost = ast_calloc(1, sizeof(sccp_hostname_t));
		memcpy(new_permithost, orig_permithost, sizeof(*new_permithost));
		SCCP_LIST_INSERT_TAIL(&new_device->permithosts, new_permithost, list);
	}
	SCCP_LIST_UNLOCK(&orig_device->permithosts);
}

/*!
 * Copy the list of selectchannels from another device
 * \param new_device original sccp device to which the list is copied
 * \param orig_device original sccp device from which to copy the list
 * 
 * \note	orig_device locked by parent function
 *
 * \lock
 * 	- device->selectedChannels
 */
void sccp_duplicate_device_selectedchannel_list(sccp_device_t * new_device, sccp_device_t * orig_device)
{
	sccp_selectedchannel_t *orig_selectedChannel = NULL;

	sccp_selectedchannel_t *new_selectedChannel = NULL;

	SCCP_LIST_HEAD_INIT(&new_device->selectedChannels);
	SCCP_LIST_LOCK(&orig_device->selectedChannels);
	SCCP_LIST_TRAVERSE(&orig_device->selectedChannels, orig_selectedChannel, list) {
		new_selectedChannel = ast_calloc(1, sizeof(sccp_selectedchannel_t));
		memcpy(new_selectedChannel, orig_selectedChannel, sizeof(*new_selectedChannel));
		SCCP_LIST_INSERT_TAIL(&new_device->selectedChannels, new_selectedChannel, list);
	}
	SCCP_LIST_UNLOCK(&orig_device->selectedChannels);
}

/*!
 * Copy the list of addons from another device
 * \param new_device new sccp device to which the list is copied
 * \param orig_device original sccp device from which to copy the list
 * 
 * \note	orig_device locked by parent function
 *
 * \lock
 * 	- device->addons
 */
void sccp_duplicate_device_addon_list(sccp_device_t * new_device, sccp_device_t * orig_device)
{
	sccp_addon_t *orig_addon = NULL;

	sccp_addon_t *new_addon = NULL;

	SCCP_LIST_HEAD_INIT(&new_device->addons);
	SCCP_LIST_LOCK(&orig_device->addons);
	SCCP_LIST_TRAVERSE(&orig_device->addons, orig_addon, list) {
		new_addon = ast_calloc(1, sizeof(sccp_addon_t));
		memcpy(new_addon, orig_addon, sizeof(*new_addon));
		SCCP_LIST_INSERT_TAIL(&new_device->addons, new_addon, list);
	}
	SCCP_LIST_UNLOCK(&orig_device->addons);
}

/*!
 * Checks two devices against one another and returns a sccp_diff_t if different
 * \param device_a sccp device
 * \param device_b sccp device
 * \return res as sccp_diff_t
 * \callgraph
 * \callergraph
 *
 * \note	device_a, device_b locked by parent function
 *
 * \lock
 * 	- device_a->buttonconfig
 * 	- device_b->buttonconfig
 * 	- device_a->permithosts
 * 	- device_b->permithosts
 * 	- device_a->addons
 * 	- device_b->addons
 * 	- device_a->selectedChannels
 * 	- device_b->selectedChannels
 */
sccp_diff_t sccp_device_changed(sccp_device_t * device_a, sccp_device_t * device_b)
{
	sccp_diff_t res = NO_CHANGES;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "(sccp_device_changed) Checking device_a: %s against device_b: %s\n", device_a->id, device_b->id);
	if (									// check changes requiring reset
		   (strcmp(device_a->description, device_b->description)) ||
		   (strcmp(device_a->imageversion, device_b->imageversion)) ||
		   (strcmp(device_a->softkeyDefinition, device_b->softkeyDefinition)) || (device_a->tz_offset != device_b->tz_offset) || (device_a->earlyrtp != device_b->earlyrtp) || (device_a->nat != device_b->nat) || (device_a->directrtp != device_b->directrtp) || (device_a->cfwdall != device_b->cfwdall) || (device_a->cfwdbusy != device_b->cfwdbusy) || (device_a->cfwdnoanswer != device_b->cfwdnoanswer) || (device_a->trustphoneip != device_b->trustphoneip)) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Changes need reset\n");
		return CHANGES_NEED_RESET;
	}
	// changes in buttonconfig
	SCCP_LIST_LOCK(&device_a->buttonconfig);
	SCCP_LIST_LOCK(&device_b->buttonconfig);
	if (device_a->buttonconfig.size != device_b->buttonconfig.size) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "buttonconfig: Changes need reset\n");
		res = CHANGES_NEED_RESET;
	} else {
		sccp_buttonconfig_t *bc_a = SCCP_LIST_FIRST(&device_a->buttonconfig);

		sccp_buttonconfig_t *bc_b = SCCP_LIST_FIRST(&device_b->buttonconfig);

		while (bc_a && bc_b) {
			if ((res = sccp_buttonconfig_changed(bc_a, bc_b)) != NO_CHANGES) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "buttonconfig: Changes need reset\n");
				break;
			}
			bc_a = SCCP_LIST_NEXT(bc_a, list);
			bc_b = SCCP_LIST_NEXT(bc_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&device_b->buttonconfig);
	SCCP_LIST_UNLOCK(&device_a->buttonconfig);

	if (res == CHANGES_NEED_RESET)
		return res;

	//sccp_hostname_t permithosts
	SCCP_LIST_LOCK(&device_a->permithosts);
	SCCP_LIST_LOCK(&device_b->permithosts);
	if (device_a->permithosts.size != device_b->permithosts.size) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "permithosts: Changes need reset\n");
		res = CHANGES_NEED_RESET;
	} else {
		sccp_hostname_t *ph_a = SCCP_LIST_FIRST(&device_a->permithosts);

		sccp_hostname_t *ph_b = SCCP_LIST_FIRST(&device_b->permithosts);

		while (ph_a && ph_b) {
			if (strcmp(ph_a->name, ph_b->name)) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "permithosts: Changes need reset\n");
				res = CHANGES_NEED_RESET;
				break;
			}
			ph_a = SCCP_LIST_NEXT(ph_a, list);
			ph_b = SCCP_LIST_NEXT(ph_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&device_b->permithosts);
	SCCP_LIST_UNLOCK(&device_a->permithosts);

	if (res == CHANGES_NEED_RESET)
		return res;

	//sccp_addon_t addons
	SCCP_LIST_LOCK(&device_a->addons);
	SCCP_LIST_LOCK(&device_b->addons);
	if (device_a->addons.size != device_b->addons.size) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "addons: Changes need reset: size_a %d, size_b %d\n", device_a->addons.size, device_b->addons.size);
		res = CHANGES_NEED_RESET;
	} else {
		sccp_addon_t *a_a = SCCP_LIST_FIRST(&device_a->addons);

		sccp_addon_t *a_b = SCCP_LIST_FIRST(&device_b->addons);

		while (a_a && a_b) {
			if (a_a->type != a_b->type || strcmp(a_a->device->id, a_b->device->id)) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "addons: Changes need reset: type_a %d, type_b %d\n", a_a->type, a_b->type);
				res = CHANGES_NEED_RESET;
				break;
			}
			a_a = SCCP_LIST_NEXT(a_a, list);
			a_b = SCCP_LIST_NEXT(a_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&device_b->addons);
	SCCP_LIST_UNLOCK(&device_a->addons);
	if (res == CHANGES_NEED_RESET)
		return res;

	if (									// check minor changes
		   (strcmp(device_a->meetmeopts, device_b->meetmeopts)) ||
		   (device_a->dtmfmode != device_b->dtmfmode) ||
		   (device_a->mwilamp != device_b->mwilamp) || (device_a->privacyFeature.status != device_b->privacyFeature.status) || (device_a->dndFeature.enabled != device_b->dndFeature.enabled) || (device_a->overlapFeature.enabled != device_b->overlapFeature.enabled) || (device_a->privacyFeature.enabled != device_b->privacyFeature.enabled) || (device_a->transfer != device_b->transfer) || (device_a->park != device_b->park) || (device_a->meetme != device_b->meetme) ||
#    ifdef CS_ADV_FEATURES
		   (device_a->useRedialMenu != device_b->useRedialMenu) ||
#    endif									/* CS_ADV_FEATURES */
		   (device_a->mwioncall != device_b->mwioncall)) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Minor changes\n");
		return MINOR_CHANGES;
	}
	//sccp_selectedchannel_t selectedChannels
	SCCP_LIST_LOCK(&device_a->selectedChannels);
	SCCP_LIST_LOCK(&device_b->selectedChannels);
	if (device_a->selectedChannels.size != device_b->selectedChannels.size) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))
		    (VERBOSE_PREFIX_3 "selectedChannels: Minor changes (%d vs %d)\n", device_a->selectedChannels.size, device_b->selectedChannels.size);
		res = MINOR_CHANGES;
	} else {
		sccp_selectedchannel_t *sc_a = SCCP_LIST_FIRST(&device_a->selectedChannels);

		sccp_selectedchannel_t *sc_b = SCCP_LIST_FIRST(&device_b->selectedChannels);

		while (sc_a && sc_b) {
			if (sc_a->channel->callid != sc_b->channel->callid) {
				sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "selectedChannels: Minor changes (diff)\n");
				res = MINOR_CHANGES;
				break;
			}
			sc_a = SCCP_LIST_NEXT(sc_a, list);
			sc_b = SCCP_LIST_NEXT(sc_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&device_b->selectedChannels);
	SCCP_LIST_UNLOCK(&device_a->selectedChannels);

	/*! \todo still to implement a check for device->setvar (ast_variables *variables) in sccp_device_changed */
	//device_a->setvar
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "(sccp_device_changed) Returning : %d\n", res);
	return res;
}

/*!
 * Checks two buttonconfig against one another and returns a sccp_diff_t if different
 * \param buttonconfig_a SCCP buttonconfig
 * \param buttonconfig_b SCCP buttonconfig
 * \return res as sccp_diff_t
 */
sccp_diff_t sccp_buttonconfig_changed(sccp_buttonconfig_t * buttonconfig_a, sccp_buttonconfig_t * buttonconfig_b)
{
	sccp_diff_t res = NO_CHANGES;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "(sccp_button_changed) Checking buttonconfigs: %d\n", buttonconfig_a->index);
	if (									// check changes requiring reset
		   !((buttonconfig_a->index == buttonconfig_b->index) && (buttonconfig_a->type == buttonconfig_b->type) && (buttonconfig_b->pendingDelete == 0)
		   )
	    ) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Changes need reset\n");
		res = CHANGES_NEED_RESET;
	} else if (								// check minor changes
			  !((buttonconfig_a->instance == buttonconfig_b->instance)
			  )
	    ) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Minor changes\n");
		res = MINOR_CHANGES;
	} else {								// check lower level changes
		switch (buttonconfig_a->type) {
		case LINE:
			{
				if ((strcmp(buttonconfig_a->button.line.name, buttonconfig_b->button.line.name)) ||
				    (strcmp(buttonconfig_a->button.line.subscriptionId.number, buttonconfig_b->button.line.subscriptionId.number)) || (strcmp(buttonconfig_a->button.line.subscriptionId.name, buttonconfig_b->button.line.subscriptionId.name)) || (strcmp(buttonconfig_a->button.line.subscriptionId.aux, buttonconfig_b->button.line.subscriptionId.aux)) || (strcmp(buttonconfig_a->button.line.options, buttonconfig_a->button.line.options))) {
					res = CHANGES_NEED_RESET;
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "line: changes need reset\n");
					break;
				}
			}
		case SPEEDDIAL:
			{
				if ((strcmp(buttonconfig_a->button.speeddial.label, buttonconfig_b->button.speeddial.label)) || (strcmp(buttonconfig_a->button.speeddial.ext, buttonconfig_b->button.speeddial.ext)) || (strcmp(buttonconfig_a->button.speeddial.hint, buttonconfig_b->button.speeddial.hint))) {
					res = CHANGES_NEED_RESET;
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "speeddial: changes need reset\n");
					break;
				}
			}
		case SERVICE:
			{
				if ((strcmp(buttonconfig_a->button.service.label, buttonconfig_b->button.service.label)) || (strcmp(buttonconfig_a->button.service.url, buttonconfig_b->button.service.url))) {
					res = CHANGES_NEED_RESET;
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "service: changes need reset\n");
					break;
				}
			}
		case FEATURE:
			{
				if ((strcmp(buttonconfig_a->button.feature.label, buttonconfig_b->button.feature.label)) || (buttonconfig_a->button.feature.id != buttonconfig_b->button.feature.id) || (strcmp(buttonconfig_a->button.feature.options, buttonconfig_b->button.feature.options))) {
					res = CHANGES_NEED_RESET;
					sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "feature: changes need reset\n");
					break;
				}
			}
		case EMPTY:
			{
				// nothing to check
			}
		}
	}

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "(sccp_buttonconfig_changed) Returning : %d\n", res);
	return res;
}

#endif
