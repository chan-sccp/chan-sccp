/*!
 * \file 	sccp_device.c
 * \brief 	SCCP Device Class
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
 * \remarks	Purpose: 	SCCP Device
 * 		When to use:	Only methods directly related to sccp devices should be stored in this source file.
 *   		Relationships: 	SCCP Device -> SCCP DeviceLine -> SCCP Line
 *   			 	SCCP Line -> SCCP ButtonConfig -> SCCP Device
 */
 
#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_socket.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include "sccp_hint.h"
#include "sccp_line.h"
#include "sccp_mwi.h"
#include "sccp_config.h"

#include <asterisk/app.h>
#include <asterisk/pbx.h>
#include <asterisk/astdb.h>
#include <asterisk/utils.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif

#define  REF_DEBUG 1

void sccp_device_pre_reload(void)
{
	sccp_device_t * d;

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list){
		d->pendingDelete = 1;
		d->pendingUpdate = 0;
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));
}

void sccp_device_post_reload(void)
{

}

/*!
 * \brief create a device and adding default values.
 * \return device with default/global values
 */
sccp_device_t * sccp_device_create(void){
	sccp_device_t * d = ast_malloc(sizeof(sccp_device_t));
	if (!d) {
		sccp_log(0)(VERBOSE_PREFIX_3 "Unable to allocate memory for a device\n");
		return NULL;
	}
	memset(d, 0, sizeof(sccp_device_t));
	ast_mutex_init(&d->lock);

	d = sccp_device_applyDefaults(d);

	SCCP_LIST_HEAD_INIT(&d->buttonconfig);
	SCCP_LIST_HEAD_INIT(&d->selectedChannels);
	SCCP_LIST_HEAD_INIT(&d->addons);
	return d;
}

/*!
 * \brief Apply Device Defaults
 * \return SCCP Device with default values
 */
sccp_device_t *sccp_device_applyDefaults(sccp_device_t *d)
{
	if(!d)
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
//	d->dndmode = GLOB(dndmode);
	d->trustphoneip = GLOB(trustphoneip);
	//d->privacyFeature.enabled = GLOB(private);
	//TODO use global cnf
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
#ifdef CS_SCCP_PICKUP
	d->pickupexten = 0;
	d->pickupcontext = NULL;
#endif

#ifdef CS_SCCP_PARK
	d->park = 1;
#else
	d->park = 0;
#endif

	/* reset statistic */
	d->configurationStatistic.numberOfLines=0;
	d->configurationStatistic.numberOfSpeeddials=0;
	d->configurationStatistic.numberOfFeatures=0;
	/* */

	d->softKeyConfiguration.modes = (softkey_modes *)SoftKeyModes;
	d->softKeyConfiguration.size = sizeof(SoftKeyModes)/sizeof(softkey_modes);

	  return d;
}




sccp_device_t *sccp_device_addToGlobals(sccp_device_t *device){
	if(!device)
		return NULL;

	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_INSERT_HEAD(&GLOB(devices), device, list);
	SCCP_LIST_UNLOCK(&GLOB(devices));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "Added device '%s' (%s)\n", device->id, device->config_type);
	return device;
}

/*!
 * \brief Get Codec for Device
 * \param ast Asterisk Channel
 * \return CodecCapability as int
 */
int sccp_device_get_codec(struct ast_channel *ast)
{
	sccp_channel_t *c = NULL;
	sccp_device_t *d = NULL;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_CODEC))(VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Asterisk requested available codecs for channel %s\n", ast->name);

	if (!(c = CS_AST_CHANNEL_PVT(ast))) {
		sccp_log((DEBUGCAT_CODEC))(VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Couldn't find a channel pvt struct. Returning global capabilities\n");
		return GLOB(global_capability);
	}

	if (!(d = c->device)) {
		sccp_log((DEBUGCAT_CODEC))(VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) Couldn't find a device associated to channel. Returning global capabilities\n");
		//return GLOB(global_capability);
	}

	/* update channel capabilities */
	sccp_channel_updateChannelCapability(c);
	ast_set_read_format(ast, c->format);
	ast_set_write_format(ast, c->format);

	char s1[512];
	sccp_log((DEBUGCAT_CODEC))(VERBOSE_PREFIX_1 "SCCP: (sccp_device_get_codec) capabilities are %s (%d)\n",
#ifndef ASTERISK_CONF_1_2
		ast_getformatname_multiple(s1, sizeof(s1) -1, c->capability & AST_FORMAT_AUDIO_MASK),
#else
		ast_getformatname_multiple(s1, sizeof(s1) -1, c->capability),
#endif
		c->capability);

	return c->capability;
}

/*!
 * \brief Create a template of Buttons as Definition for a Phonetype (d->skinny_type)
 * \param d device
 * \param btn buttonlist
 */
void sccp_dev_build_buttontemplate(sccp_device_t *d, btnlist * btn) {
	uint8_t i;
	if (!d || !d->session)
		return;
	sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_BUTTONTEMPLATE | DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Building button template %s(%d), user config %s\n",	d->id, devicetype2str(d->skinny_type), d->skinny_type, d->config_type);

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
			for (i = 0; i < 20; i++){
				btn[i].type = SCCP_BUTTONTYPE_MULTI;
			}
			btn[20].type = SKINNY_BUTTONTYPE_MESSAGES;
			btn[21].type = SKINNY_BUTTONTYPE_DIRECTORY;
			btn[22].type = SKINNY_BUTTONTYPE_HEADSET;
			btn[23].type = SKINNY_BUTTONTYPE_APPLICATION;
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
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			break;
		case SKINNY_DEVICETYPE_CISCO7920:
		case SKINNY_DEVICETYPE_CISCO7921:
		case SKINNY_DEVICETYPE_CISCO7925:
			for (i = 0; i < 6; i++)
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
			/* the nokia icc client identifies it self as SKINNY_DEVICETYPE_CISCO7970, but it can have only one line  */
			if(!strcasecmp(d->config_type, "nokia-icc")) { // this is for nokia icc legacy support (Old releases) -FS
				(btn++)->type = SCCP_BUTTONTYPE_MULTI;
			} else {
				uint8_t addonsTaps = sccp_addons_taps(d);
				for (i = 0; i < 8 + addonsTaps; i++){
					(btn++)->type = SCCP_BUTTONTYPE_MULTI;
				}
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
sccp_moo_t * sccp_build_packet(sccp_message_t t, size_t pkt_len)
{
	sccp_moo_t * r = ast_malloc(sizeof(sccp_moo_t));
	if (!r) {
		ast_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
		return NULL;
	}
	memset(r, 0,  pkt_len + 12);
	r->length = htolel(pkt_len + 4);
	r->lel_messageId = htolel(t);
	return r;
}

/*!
 * \brief Send SCCP Message to Device
 * \param d SCCP Device
 * \param r SCCP MOO Message
 * \return Status as int
 */
int sccp_dev_send(const sccp_device_t * d, sccp_moo_t * r)
{
	if(d && d->session){
		sccp_log((DEBUGCAT_MESSAGE))(VERBOSE_PREFIX_3 "%s: >> Send message %s\n", d->id, message2str(letohl(r->lel_messageId)));
//		sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: >> Send message %s\n", d->id, message2str(letohl(r->lel_messageId)));
		return sccp_session_send(d, r);
	}else
		return -1;
}



void sccp_dev_sendmsg(sccp_device_t * d, sccp_message_t t) {
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

	if (d->registrationState == opt)
		return;


	d->registrationState = opt;

	/* Handle registration completion. */
	if (opt == SKINNY_DEVICE_RS_OK) {
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
void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt) {
	sccp_moo_t * r;
	if (!d)
		return;

	if (!d->softkeysupport)
		return; /* the device does not support softkeys */

	/*let's activate the transfer */
	if (opt == KEYMODE_CONNECTED)
		opt = ( (/* d->conference && */ d->conference_channel) ? KEYMODE_CONNCONF : (d->transfer) ? KEYMODE_CONNTRANS : KEYMODE_CONNECTED );

	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance  = htolel(line);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(opt);

	r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF; /* htolel(65535); */

	if ((opt == KEYMODE_ONHOOK || opt == KEYMODE_OFFHOOK || opt == KEYMODE_OFFHOOKFEAT) && (ast_strlen_zero(d->lastNumber)))
		r->msg.SelectSoftKeysMessage.les_validKeyMask &= htolel(~(1<<0));


	sccp_log((DEBUGCAT_SOFTKEY | DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, keymode2str(opt), opt, line, callid);
	sccp_dev_send(d, r);
}


/*!
 * \brief Set Message Waiting Indicator (MWI or LAMP) on Device
 * \param d SCCP Device
 * \param l SCCP Line
 * \param hasMail Mail Indicator Status as uint8_t
 */
void sccp_dev_set_mwi(sccp_device_t * d, sccp_line_t * l, uint8_t hasMail)
{
	sccp_moo_t * r;
	int instance;
	if (!d)
		return;

	
	int retry = 0;
	while(sccp_device_trylock(d)) {
		retry++;
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s), retry: %d\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__, retry);
		usleep(100);
		
		if(retry > 100){
		     return;
		}
	}

	if (l) {
		instance = sccp_device_find_index_for_line(d, l->name);
	} else {
		if (d->mwilight == hasMail) {
			sccp_device_unlock(d);
			return;
		}
		d->mwilight = hasMail;
		instance = 0;
	}
	sccp_device_unlock(d);


	REQ(r, SetLampMessage);
	r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
	r->msg.SetLampMessage.lel_stimulusInstance = (l ? htolel(instance) : 0);
	/* when l is defined we are switching on/off the button icon */
	r->msg.SetLampMessage.lel_lampMode = htolel( (hasMail) ? ( (l) ? SKINNY_LAMP_ON :  d->mwilamp) : SKINNY_LAMP_OFF);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MWI))(VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s)%d\n",DEV_ID_LOG(d), hasMail ? "ON" : "OFF", (l ? l->name : "unknown"),(l ? instance : 0));
}

/*!
 * \brief Set Ringer on Device
 * \param d SCCP Device
 * \param opt Option as uint8_t
 * \param line Line Number as uint32_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_set_ringer(sccp_device_t * d, uint8_t opt, uint32_t line, uint32_t callid)
{
        sccp_moo_t * r;

        if (!d || !d->session)
                return;
  // If we have multiple calls ringing at once, this has lead to
  // never ending rings even after termination of the alerting call.
  // Obviously the ringermode is no longer a per-device state but
  // rather per line/per call.
	//if (d->ringermode == opt)
	//	return;

	//d->ringermode = opt;
	REQ(r, SetRingerMessage);
	r->msg.SetRingerMessage.lel_ringMode = htolel(opt);
	/* Note that for distinctive ringing to work with the higher protocol versions
	   the following actually needs to be set to 1 as the original comment says.
	   Curiously, the variable is not set to 1 ... */
	r->msg.SetRingerMessage.lel_unknown1 = htolel(1);/* always 1 */
	r->msg.SetRingerMessage.lel_lineInstance = htolel(line);
	r->msg.SetRingerMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send ringer mode %s(%d) on device\n", d->id, station2str(opt), opt);

}

/*!
 * \brief Set Speaker Status on Device
 * \param d SCCP Device
 * \param mode Speaker Mode as uint8_t
 */
void sccp_dev_set_speaker(sccp_device_t * d, uint8_t mode)
{
        sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, SetSpeakerModeMessage);
	r->msg.SetSpeakerModeMessage.lel_speakerMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send speaker mode %d\n", d->id, mode);
}

/*!
 * \brief Set Microphone Status on Device
 * \param d SCCP Device
 * \param mode Microphone Mode as uint8_t
 */
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t mode)
{
        sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, SetMicroModeMessage);
	r->msg.SetMicroModeMessage.lel_micMode = htolel(mode);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send microphone mode %d\n", d->id, mode);
}

/*!
 * \brief Set Call Plane to Active on  Line on Device
 * \param l SCCP Line
 * \param device SCCP Device
 * \param status Status as int
 * \todo What does this function do exactly (ActivateCallPlaneMessage) ?
 */
void sccp_dev_set_cplane(sccp_line_t * l, sccp_device_t *device, int status)
{
	sccp_moo_t * r;
	int 	instance=0;
	if (!l)
		return;

	if (!device)
		return;

	instance = sccp_device_find_index_for_line(device, l->name);
	REQ(r, ActivateCallPlaneMessage);
	if (status)
		r->msg.ActivateCallPlaneMessage.lel_lineInstance = htolel(instance);
	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send activate call plane on line %d\n", device->id, (status) ? instance : 0 );
}

/*!
 * \brief Set Call Plane to In-Active on  Line on Device
 * \param d device
 * \todo What does this function do exactly (DeactivateCallPlaneMessage) ?
 */
void sccp_dev_deactivate_cplane(sccp_device_t * d)
{
	if (!d) {
		sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "Null device for deactivate callplane\n");
		return;
	}

	sccp_dev_sendmsg(d, DeactivateCallPlaneMessage);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send deactivate call plane\n", d->id);
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
        sccp_moo_t * r;
	if (!d || !d->session)
		return;

	REQ(r, StartToneMessage);
	r->msg.StartToneMessage.lel_tone = htolel(tone);
	r->msg.StartToneMessage.lel_toneTimeout = htolel(timeout);
	r->msg.StartToneMessage.lel_lineInstance = htolel(line);
	r->msg.StartToneMessage.lel_callReference = htolel(callid);

	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Sending tone %s (%d)\n", d->id, tone2str(tone), tone);
}

/*!
 * \brief Send Stop Tone to Device
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID as uint32_t
 */
void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid)
{
        sccp_moo_t * r;

        if (!d || !d->session)
                return;
	REQ(r, StopToneMessage);
	r->msg.StopToneMessage.lel_lineInstance = htolel(line);
	r->msg.StopToneMessage.lel_callReference = htolel(callid);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Stop tone on device\n", d->id);
}

/*!
 * \brief Send Clear Prompt to Device
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID uint32_t
 */
void sccp_dev_clearprompt(sccp_device_t * d, uint8_t line, uint32_t callid)
{
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	REQ(r, ClearPromptStatusMessage);
	r->msg.ClearPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.ClearPromptStatusMessage.lel_lineInstance  = htolel(line);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Clear the status prompt on line %d and callid %d\n", d->id, line, callid);
}



/*!
 * \brief Send Display Prompt to Device
 * \param d SCCP Device
 * \param line Line as uint8_t
 * \param callid Call ID uint32_t
 * \param msg Msg as char
 * \param timeout Timeout as int
 */
void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char * msg, int timeout)
{
        sccp_moo_t * r;

	if (!d || !d->session)
		return;
		
	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) 
	        return; 	/* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
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
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", d->id, line, callid, timeout);
}

/*!
 * \brief Send Clear Display to Device
 * \param d SCCP Device
 */
void sccp_dev_cleardisplay(sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearDisplay);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Clear the display\n", d->id);
}


/*!
 * \brief Send Display to Device
 * \param d SCCP Device
 * \param msg Msg as char
 */
void sccp_dev_display(sccp_device_t * d, char * msg)
{
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayTextMessage);
	sccp_copy_string(r->msg.DisplayTextMessage.displayMessage, msg, sizeof(r->msg.DisplayTextMessage.displayMessage));

	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Display text\n", d->id);
}

/*!
 * \brief Send Clear Display Notification to Device
 * \param d SCCP Device
 */
void sccp_dev_cleardisplaynotify(sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE))(VERBOSE_PREFIX_3 "%s: Clear the display notify message\n", d->id);
}

/*!
 * \brief Send Display Notification to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param timeout Timeout as uint32_t
 */
void sccp_dev_displaynotify(sccp_device_t * d, char * msg, uint32_t timeout)
{
        sccp_moo_t * r;

	if (!d || !d->session)
		return;
	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayNotifyMessage);
	r->msg.DisplayNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Display notify with timeout %d\n", d->id, timeout);
}

/*!
 * \brief Send Clear Display Notification to Device
 * \param d SCCP Device
 */
void sccp_dev_cleardisplayprinotify(sccp_device_t * d)
{
	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	sccp_dev_sendmsg(d, ClearPriNotifyMessage);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_MESSAGE))(VERBOSE_PREFIX_3 "%s: Clear the display priority notify message\n", d->id);
}

/*!
 * \brief Send Display Priority Notification to Device
 * \param d SCCP Device
 * \param msg Msg as char
 * \param priority Priority as uint32_t
 * \param timeout Timeout as uint32_t
 */
void sccp_dev_displayprinotify(sccp_device_t * d, char * msg, uint32_t priority, uint32_t timeout)
{
	sccp_moo_t * r;

	if (!d || !d->session)
		return;

	if (d->skinny_type < 6 || d->skinny_type ==  SKINNY_DEVICETYPE_ATA186 || (!strcasecmp(d->config_type,"kirk"))) return; /* only for telecaster and new phones */

	if (!msg || ast_strlen_zero(msg))
		return;

	REQ(r, DisplayPriNotifyMessage);
	r->msg.DisplayPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayPriNotifyMessage.displayMessage, msg, sizeof(r->msg.DisplayPriNotifyMessage.displayMessage));
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Display notify with timeout %d and priority %d\n", d->id, timeout, priority);
}

/*!
 * \brief Find SpeedDial by Index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \param type Type as uint8_t
 * \return SCCP Speed
 */
sccp_speed_t *sccp_dev_speed_find_byindex(sccp_device_t * d, uint8_t instance, uint8_t type)
{
	sccp_speed_t 		* k = NULL;
	sccp_buttonconfig_t	*config;


	if (!d || !d->session || instance == 0)
		return NULL;

	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {

		if(config->type == SPEEDDIAL && config->instance == instance){
		  
			/* we are searching for hinted speeddials */
			if(type == SCCP_BUTTONTYPE_HINT && !sccp_is_nonempty_string(config->button.speeddial.hint))
				continue;
		  
			k = ast_malloc(sizeof(sccp_speed_t));
			memset(k, 0, sizeof(sccp_speed_t));

			k->instance = instance;
			k->type = SCCP_BUTTONTYPE_SPEEDDIAL;
			sccp_copy_string(k->name, config->button.speeddial.label, sizeof(k->name));
			sccp_copy_string(k->ext, config->button.speeddial.ext, sizeof(k->ext));
			if (!ast_strlen_zero(config->button.speeddial.hint)){
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
 */
sccp_line_t * sccp_dev_get_activeline(sccp_device_t * d)
{
	sccp_buttonconfig_t *buttonconfig;

	/* What's up if the currentLine is NULL? let's do the check */
	if (!d || !d->session)
		return NULL;

	if (!d->currentLine) {
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			if(buttonconfig->type == LINE ){
				d->currentLine = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
				if(d->currentLine){
					break;
				}
			}
		}

		if (d->currentLine) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: Forcing the active line to %s from NULL\n", d->id, d->currentLine->name);
		} else {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: No lines\n", d->id);
		}
	} else {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: The active line is %s\n", d->id, d->currentLine->name);
	}
	return d->currentLine;
}

/*!
 * \brief Send Set Activeline to Device
 * \param device SCCP Device
 * \param l SCCP Line
 */
void sccp_dev_set_activeline(sccp_device_t *device, sccp_line_t * l)
{
	if (!l || !device || !device->session)
		return;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: Send the active line %s\n", device->id, l->name);
	sccp_device_lock(device);
	device->currentLine = l;
	sccp_device_unlock(device);
	return;
}

/*!
 * \brief Reschedule Display Prompt Check
 * \param d SCCP Device
 */
void sccp_dev_check_displayprompt(sccp_device_t * d)
{
	char tmp[256] = "";
	int timeout = 0;
	uint8_t res = 0;

	if (!d || !d->session)
		return;

	sccp_dev_clearprompt(d, 0, 0);
	sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_YOUR_CURRENT_OPTIONS, 0);

	if (d->phonemessage) 										// display message if set
		sccp_dev_displayprompt(d,0,0,d->phonemessage,0);

	sccp_dev_set_keyset(d, 0, 0, KEYMODE_ONHOOK); 				/* this is for redial softkey */

	/* check for forward to display */
	res = 0;
//	while(SCCP_LIST_TRYLOCK(&d->lines)) {
//		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "[SCCP LOOP] %s in file %s, line %d (%s)\n", d->id ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
//		usleep(5);
//	}


// 	SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
// 		if(buttonconfig->type == LINE ){
// 			l = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
// 			if (l && l->cfwd_type != SCCP_CFWD_NONE && l->cfwd_num) {
// 				res = 1;
// 				// tmp[0] = '\0';
// 				memset(tmp, 0, sizeof(tmp));
// 				instance = sccp_device_find_index_for_line(d, l->name);
//
// 				if (l->cfwd_type == SCCP_CFWD_ALL) {
// 					strcat(tmp, SKINNY_DISP_CFWDALL ":");
// 					sccp_dev_set_lamp(d, SKINNY_STIMULUS_FORWARDALL, instance, SKINNY_LAMP_ON);
// 				} else if (l->cfwd_type == SCCP_CFWD_BUSY) {
// 					strcat(tmp, SKINNY_DISP_CFWDBUSY ":");
// 					sccp_dev_set_lamp(d, SKINNY_STIMULUS_FORWARDBUSY, instance, SKINNY_LAMP_ON);
// 				}
// 				strcat(tmp, SKINNY_DISP_FORWARDED_TO " ");
// 				strcat(tmp, l->cfwd_num);
// 				sccp_dev_displayprompt(d, 0, 0, tmp, 0);
// 				sccp_dev_set_keyset(d, instance, 0, KEYMODE_ONHOOK); /* this is for redial softkey */
// 			}
//
// 		}
// 	}


	if (d->dndFeature.enabled && d->dndFeature.status ) {
		if (d->dndFeature.status == SCCP_DNDMODE_REJECT )
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (" SKINNY_DISP_BUSY ") <<<", 0);

		else if (d->dndFeature.status == SCCP_DNDMODE_SILENT )
			/* no internal label for the silent string */
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " (Silent) <<<", 0);
		else
			sccp_dev_displayprompt(d, 0, 0, ">>> " SKINNY_DISP_DND " <<<", 0);
	}

	if (!ast_db_get("SCCP/message", "timeout", tmp, sizeof(tmp))) {
		sscanf(tmp, "%i", &timeout);
	}
	if (!ast_db_get("SCCP/message", "text", tmp, sizeof(tmp))) {
		if (!ast_strlen_zero(tmp)) {
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
OUT:
	sccp_log((DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: Finish DisplayPrompt\n", d->id);
}

/*!
 * \brief Select Line on Device
 * \param d SCCP Device
 * \param wanted SCCP Line
 */
void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * wanted)
{
	/* XXX rework this */
	sccp_line_t * current;
	sccp_channel_t * chan = NULL;

	if (!d || !d->session)
		return;

	current = sccp_dev_get_activeline(d);

//	if (!wanted || !current || wanted->device != d || current == wanted)
	if (!wanted || !current ||  current == wanted)
		return;

	// If the current line isn't in a call, and
	// neither is the target.
	if (SCCP_LIST_FIRST(&current->channels) == NULL && SCCP_LIST_FIRST(&wanted->channels) == NULL) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: All lines seem to be inactive, SEIZEing selected line %s\n", d->id, wanted->name);
		sccp_dev_set_activeline(d, wanted);

		chan = sccp_channel_allocate(wanted, d);
		if (!chan) {
			ast_log(LOG_ERROR, "%s: Failed to allocate SCCP channel.\n", d->id);
			return;
		}
/*	} else if ( current->dnState > TsOnHook || wanted->dnState == TsOffHook) { */
	} else if ( d->state == SCCP_DEVICESTATE_OFFHOOK) {
	        // If the device is currently onhook, then we need to ...
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: Selecing line %s while using line %s\n", d->id, wanted->name, current->name);
        	// XXX (1) Put current call on hold
        	// (2) Stop transmitting/recievening
	} else {
        	// Otherwise, just select the callplane
        	ast_log(LOG_WARNING, "%s: Unknown status while trying to select line %s.  Current line is %s\n", d->id, wanted->name, current->name);
	}
}

/*!
 * \brief Set Message Waiting Indicator (MWI/Lamp)
 * \param d SCCP Device
 * \param stimulus Stimulus as uint16_t
 * \param instance Instance as uint8_t
 * \param lampMode LampMode as uint8_t
 */
void sccp_dev_set_lamp(const sccp_device_t * d, uint16_t stimulus, uint8_t instance, uint8_t lampMode)
{
/*
   sccp_moo_t * r;

	if (!d )
		return;

	REQ(r, SetLampMessage);
	r->msg.SetLampMessage.lel_stimulus = htolel(stimulus);
	r->msg.SetLampMessage.lel_stimulusInstance = htolel(instance);
	r->msg.SetLampMessage.lel_lampMode = htolel(lampMode);
	sccp_dev_send(d, r);
	*/
	//sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send lamp mode %s(%d) on line %d\n", d->id, lampmode2str(lampMode), lampMode, instance );
}

void sccp_dev_forward_status(sccp_line_t * l, sccp_device_t *device) {
	sccp_moo_t 				*r1 = NULL;
	sccp_linedevices_t 		*linedevice = NULL;
	int 					instance;

	if (!device || !device->session)
		return;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: Send Forward Status.  Line: %s\n", device->id, l->name);

	instance = sccp_device_find_index_for_line(device, l->name);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
		if(linedevice->device == device);
			break;
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if(!linedevice){
		ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return;
	}

	REQ(r1, ForwardStatMessage);
	r1->msg.ForwardStatMessage.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled)? htolel(1) : 0;
	r1->msg.ForwardStatMessage.lel_lineNumber = htolel(instance);
	if (linedevice->cfwdAll.enabled) {
			r1->msg.ForwardStatMessage.lel_cfwdallstatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdallnumber, linedevice->cfwdAll.number, sizeof(r1->msg.ForwardStatMessage.cfwdallnumber));
	}else if (linedevice->cfwdBusy.enabled) {
			r1->msg.ForwardStatMessage.lel_cfwdbusystatus = htolel(1);
			sccp_copy_string(r1->msg.ForwardStatMessage.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(r1->msg.ForwardStatMessage.cfwdbusynumber));
	}
	sccp_dev_send(device, r1);

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
 */
int sccp_device_check_ringback(sccp_device_t * d)
{
	sccp_channel_t * c;

	sccp_device_lock(d);
	d->needcheckringback = 0;
	if (d->state == SCCP_DEVICESTATE_OFFHOOK) {
		sccp_device_unlock(d);
		return 0;
	}
	sccp_device_unlock(d);
	c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLTRANSFER);
	if (!c)
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_RINGING);
	if (!c)
		c = sccp_channel_find_bystate_on_device(d, SCCP_CHANNELSTATE_CALLWAITING);

	if (c) {
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_RINGING);
		return 1;
	}
	return 0;
}

/*!
 * \brief Handle Post Device Registration
 * \param data Data
 */
void * sccp_dev_postregistration(void *data)
{
	sccp_device_t * d = data;

	if (!d)
		return NULL;

	sccp_log(2)(VERBOSE_PREFIX_3 "%s: Device registered; performing post registration tasks...\n", d->id);

	/* turn off the device MWI light. We need to force it off on some phone (7910 for example) */
	sccp_dev_set_mwi(d, NULL, 0);

	sccp_dev_check_displayprompt(d);

	// Post event to interested listeners (hints, mwi) that device was registered.
	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type=SCCP_EVENT_DEVICEREGISTERED;
	event->event.deviceRegistered.device = d;
	sccp_event_fire( (const sccp_event_t **)&event);

	sccp_mwi_check(d);

	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: Post registration process... done!\n", d->id);
	return NULL;
}

/*!
 * \brief Clean Device
 *
 *  clean up memory allocated by the device.
 *  if destroy is true, device will be removed from global device list
 *
 * \param d SCCP Device
 * \param destroy Destroy as boolean_t
 * \param cleanupTime Clean-up Time as uint8
 */
void sccp_dev_clean(sccp_device_t * d, boolean_t destroy, uint8_t cleanupTime) {
	sccp_buttonconfig_t	*config = NULL;
	sccp_selectedchannel_t 	*selectedChannel = NULL;
	sccp_line_t		*line =NULL;
	sccp_channel_t		*channel=NULL;

	char family[25];


	if (!d)
		return;

	sccp_device_lock(d);
	
	sprintf(family, "SCCP/%s", d->id);
	ast_db_del(family, "lastDialedNumber");
	if(!ast_strlen_zero(d->lastNumber))
		ast_db_put(family, "lastDialedNumber", d->lastNumber);

	if(destroy)
	{
		if(d->list.prev == NULL && d->list.next == NULL && GLOB(devices).size > 1 ){
			ast_log(LOG_ERROR, "%s: removing device from global device list. prev and next pointer ist not set while device list size is %d\n", DEV_ID_LOG(d), GLOB(devices).size);
		}else{
			SCCP_LIST_LOCK(&GLOB(devices));
			SCCP_LIST_REMOVE(&GLOB(devices), d, list);
			SCCP_LIST_UNLOCK(&GLOB(devices));
		}
	}

	

	/* hang up open channels and remove device from line */
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if(config->type == LINE ){
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if(!line)
				continue;

			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->channels, channel, list) {
				if(channel->device == d){
					sccp_channel_endcall(channel);
				}
			}
			SCCP_LIST_TRAVERSE_SAFE_END;
			
			/* remove devices from line */
			sccp_line_removeDevice(line, d);
		}
		
		
		config->instance = 0; /* reset button configuration to rebuild template on register */
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	/* */



	/* removing addons */
	if(destroy){
		sccp_addons_clear(d);
	}

	/* removing selected channels */
	SCCP_LIST_LOCK(&d->selectedChannels);
	while((selectedChannel = SCCP_LIST_REMOVE_HEAD(&d->selectedChannels, list))) {
		ast_free(selectedChannel);
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	d->session = NULL;
	sccp_device_unlock(d);
	
	if(destroy){
		if(cleanupTime > 0){
			if( (d->scheduleTasks.free = sccp_sched_add(sched, cleanupTime * 1000, sccp_device_free, d)) < 0 ) {
				sleep(cleanupTime);
				sccp_device_free(d);
				d=NULL;
			}
		}else{
			sccp_device_free(d);
			d=NULL;
		}
		return;
	}

	d->session = NULL;
}


/*!
 * \brief Free a Device as scheduled command
 * \param ptr SCCP Device Pointer
 * \return success as int
 */
int sccp_device_free(const void *ptr){
	sccp_device_t 		*d = (sccp_device_t *)ptr;
	sccp_buttonconfig_t	*config = NULL;
	sccp_hostname_t 	*permithost = NULL;

	
	
	sccp_device_lock(d);
	/* remove button config */
	/* only generated on read config, so do not remove on reset/restart*/
	SCCP_LIST_LOCK(&d->buttonconfig);
	while((config = SCCP_LIST_REMOVE_HEAD(&d->buttonconfig, list))) {
		ast_free(config);
		config = NULL;
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	SCCP_LIST_HEAD_DESTROY(&d->buttonconfig);
	/* */
	
	/* removing permithosts */
	SCCP_LIST_LOCK(&d->permithosts);
	while((permithost = SCCP_LIST_REMOVE_HEAD(&d->permithosts, list))) {
		ast_free(permithost);
	}
	SCCP_LIST_UNLOCK(&d->permithosts);
	SCCP_LIST_HEAD_DESTROY(&d->permithosts);

	/* destroy selected channels list */
	SCCP_LIST_HEAD_DESTROY(&d->selectedChannels);
	if (d->ha){
		ast_free_ha(d->ha);
	}

	d->ha = NULL;
	
	sccp_log(DEBUGCAT_DEVICE)(VERBOSE_PREFIX_3 "%s: device deleted\n", d->id);
	
	sccp_device_unlock(d);
	ast_mutex_destroy(&d->lock);
	ast_free(d);

	return 0;
}

boolean_t sccp_device_isVideoSupported(const sccp_device_t *device){
	//if(device->capability & AST_FORMAT_VIDEO_MASK)
	//	return TRUE;

	return FALSE;
}

/*!
 * \brief Find ServiceURL by index
 * \param d SCCP Device
 * \param instance Instance as uint8_t
 * \return SCCP Service
 */
sccp_service_t * sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint8_t instance)
{
	sccp_service_t 		* service = NULL;
	sccp_buttonconfig_t	* config=NULL;

	if (!d || !d->session)
		return NULL;


	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE))(VERBOSE_PREFIX_3 "%s: searching for service with instance %d\n", d->id, instance);
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log(((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE) + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: instance: %d buttontype: %d\n", d->id, config->instance, config->type);

		if(config->type == SERVICE && config->instance == instance){
			service = ast_malloc(sizeof(sccp_service_t));
			memset(service, 0, sizeof(sccp_service_t));
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE))(VERBOSE_PREFIX_3 "%s: found service: %s\n", d->id, config->button.service.label);

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
 */
int sccp_device_find_index_for_line(const sccp_device_t * d, char *lineName)
{
	sccp_buttonconfig_t	*config;

	if(!d || !lineName)
		return -1;

	/* device is already locked by parent function */
	//SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if(config->type == LINE && (config->button.line.name) && lineName && !strcasecmp(config->button.line.name, lineName)){
			break;
		}
	}
	//SCCP_LIST_UNLOCK(&d->buttonconfig);

	return (config)? config->instance : 0;
}

/*!
 * \brief Remove Line from Device
 * \param device SCCP Device
 * \param line SCCP Line
 */
void sccp_device_removeLine(sccp_device_t *device, sccp_line_t * line)
{
//	sccp_buttonconfig_t	*config;
//
//
//	SCCP_LIST_LOCK(&device->buttonconfig);
//	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
//		if(config->type == LINE && (config->button.line.name) && line && !strcasecmp(config->button.line.name, line->name)){
//			config->button.line.reference = NULL;
//
//			//TODO implement remove
//			//SCCP_LIST_REMOVE_CURRENT(config);
//		}
//	}
//	SCCP_LIST_UNLOCK(&device->buttonconfig);

}

/*!
 * \brief Send Reset to a Device
 * \param d SCCP Device
 * \param reset_type as int
 * \return Status as int
 */
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type)
{
	sccp_moo_t * r;


	if(!d)
		return 0;

	REQ(r, Reset);
	r->msg.Reset.lel_resetType = htolel(reset_type);
	sccp_session_send(d, r);
	return 1;
}

/* temporary function for hints */

/*!
 * \brief Send Call State to Device
 * \param d SCCP Device
 * \param instance Instance as int
 * \param callid Call ID as int
 * \param state Call State as int
 * \param priority Priority as skinny_callPriority_t
 * \param visibility Visibility as skinny_callinfo_visibility_t
 */
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state, skinny_callPriority_t priority, skinny_callinfo_visibility_t visibility)
{
	sccp_moo_t * r;

	if (!d)
		return;
	REQ(r, CallStateMessage);
	r->msg.CallStateMessage.lel_callState = htolel(state);
	r->msg.CallStateMessage.lel_lineInstance = htolel(instance);
	r->msg.CallStateMessage.lel_callReference = htolel(callid);
	r->msg.CallStateMessage.lel_visibility = htolel(visibility);
	r->msg.CallStateMessage.lel_priority = htolel(priority);
	/*r->msg.CallStateMessage.lel_unknown3 = htolel(2);*/
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Send and Set the call state %s(%d) on call %d\n", d->id, sccp_callstate2str(state), state, callid);
}


/**
 * get number of channels that the device owns
 * \param device sccp device
 * \note device should be locked by parent functions
 */
uint8_t sccp_device_numberOfChannels(const sccp_device_t *device){
	sccp_buttonconfig_t 	*config;
	sccp_channel_t 		*c;
	sccp_line_t 		*l;
	uint8_t			numberOfChannels = 0;

	if(!device){
		sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "device is null\n");
		return 0;
	}

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {

		if(config->type == LINE){
			l = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if(!l)
				continue;


			SCCP_LIST_LOCK(&l->channels);
			SCCP_LIST_TRAVERSE(&l->channels, c, list) {
				if(c->device == device)
					numberOfChannels++;
			}
			SCCP_LIST_UNLOCK(&l->channels);


		}
	}

	return numberOfChannels;
}

