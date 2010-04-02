/*!
 * \file 	sccp_features.c
 * \brief 	SCCP Features Class
 * \author 	Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \since 	2009-01-16
 * \date        $Date$
 * \version     $Revision$
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_features.h"
#ifdef CS_ADV_FEATURES
#include "sccp_adv_features.h"
#endif
#include "sccp_lock.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/musiconhold.h>
#ifdef CS_SCCP_PARK
#include <asterisk/features.h>
#endif
#ifndef ast_free
#define ast_free free
#endif


#include "sccp_featureButton.h"
#include "sccp_conference.h"

/*!
 * \brief Handle Call Forwarding
 * \param l SCCP Line
 * \param device SCCP Device
 * \param type CallForward Type (NONE, ALL, BUSY, NOANSWER) as SCCP_CFWD_*
 * \return SCCP Channel
 */
sccp_channel_t * sccp_feat_handle_callforward(sccp_line_t * l, sccp_device_t *device, uint8_t type)
{
	sccp_channel_t * c = NULL;
	struct ast_channel * bridge = NULL;
	sccp_linedevices_t	*linedevice;
	int instance;

	if (!l || !device || !device->id || ast_strlen_zero(device->id)){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
		if(linedevice->device == device);
			break;
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if(!linedevice){
		ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return NULL;
	}

	/* if call forward is active and you asked about the same call forward maybe you would disable */
	if( 	(linedevice->cfwdAll.enabled && type == SCCP_CFWD_ALL)
			|| (linedevice->cfwdBusy.enabled && type == SCCP_CFWD_BUSY)
			|| type == SCCP_CFWD_ALL ){

		sccp_line_cfwd(l, device, SCCP_CFWD_NONE, NULL);
		sccp_dev_sendmsg(device, DeactivateCallPlaneMessage);
		sccp_dev_check_displayprompt(device);
	}
	else{
		if(type == SCCP_CFWD_NOANSWER)
		{
			sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "### CFwdNoAnswer NOT SUPPORTED\n");
			sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
			return NULL;
		}
	}

	sccp_device_lock(device);
	instance = sccp_device_find_index_for_line(device, l->name);
	sccp_device_unlock(device);

	/* look if we have a call  */
	c = sccp_channel_get_active(device);

	if (c) {
		// we have a channel, checking if
		if (c->state == SCCP_CHANNELSTATE_RINGOUT || c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED || c->state == SCCP_CHANNELSTATE_BUSY || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			if(c->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				// if we have an outbound call, we can set callforward to dialed number -FS
				if(c->dialedNumber && !ast_strlen_zero(c->dialedNumber)) { // checking if we have a number !
					sccp_line_cfwd(l, device, type, c->dialedNumber);
					// we are on call, so no tone has been played until now :)
					//sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, instance, 0, 0);

					sccp_channel_endcall(c);
					return NULL;
				}
			}
			else if(c->owner && (bridge = ast_bridged_channel(c->owner))) { // check if we have an ast channel to get callerid from
				// if we have an incoming or forwarded call, let's get number from callerid :) -FS
#ifdef CS_AST_CHANNEL_HAS_CID
				char * number = NULL;
				if(bridge->cid.cid_num)
					number = strdup(bridge->cid.cid_num);
#else
				char * number = NULL, * name = NULL, * cidtmp =  NULL;
				if(bridge->callerid) {
					cidtmp = strdup(bridge->callerid);
					ast_callerid_parse(cidtmp, &name, &number);
				}
				// cleaning unused vars
				if(cidtmp)
					ast_free(cidtmp);

				if(name)
					ast_free(name);
#endif
				if(number) {
					sccp_line_cfwd(l, device, type, number);
					// we are on call, so no tone has been played until now :)
					sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, instance, 0, 0);

					sccp_channel_endcall(c);
					ast_free(number);
					return NULL;
				}
				// if we where here it's cause there is no number in callerid,, so put call on hold and ask for a call forward number :) -FS
				if (!sccp_channel_hold(c))
				{
					// if can't hold  it means there is no active call, so return as we're already waiting a number to dial
					sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
					return NULL;
				}
			}
		} else if(c->state == SCCP_CHANNELSTATE_OFFHOOK && ast_strlen_zero(c->dialedNumber)) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(device, instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			sccp_channel_lock(c);
			c->ss_action = SCCP_SS_GETFORWARDEXTEN; /* Simpleswitch will catch a number to be dialed */
			c->ss_data = type; /* this should be found in thread */
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(device, c, SCCP_CHANNELSTATE_GETDIGITS);
			sccp_channel_unlock(c);
			return c;
		} else {
			// we cannot allocate a channel, or ask an extension to pickup.
			sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
			return NULL;
		}
	}

	// if we where here there is no call in progress, so we should allocate a channel.
	c = sccp_channel_allocate(l, device);

	if(!c) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_GETFORWARDEXTEN; /* Simpleswitch will catch a number to be dialed */
	c->ss_data = type; /* this should be found in thread */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	sccp_channel_set_active(device, c);
	sccp_indicate_nolock(device, c, SCCP_CHANNELSTATE_GETDIGITS);

	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_callforward) Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate_lock(c->device, c, SCCP_CHANNELSTATE_CONGESTION);
		sccp_channel_endcall(c);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (device->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	return c;
}

#ifdef CS_SCCP_PICKUP
/*!
 * \brief Handle Direct Pickup of Line
 * \param l SCCP Line
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t * sccp_feat_handle_directpickup(sccp_line_t * l, sccp_device_t *d)
{
	sccp_channel_t * c;
	int instance;

	if (!l || !d || strlen(d->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {
		// we have a channel, checking if
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, instance, (c && c->callid)?c->callid:0);
			sccp_channel_lock(c);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETPICKUPEXTEN; /* Simpleswitch will catch a number to be dialed */
			c->ss_data = 0; /* this should be found in thread */
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);
			sccp_channel_unlock(c);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l, d);

	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_directpickup) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_GETPICKUPEXTEN; /* Simpleswitch will catch a number to be dialed */
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	sccp_channel_set_active(d, c);
	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);

	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_directpickup) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	return c;
}
#endif

#ifdef CS_SCCP_PICKUP
/*!
 * \brief Handle Direct Pickup of Extension
 * \param c SCCP Channel
 * \param exten Extension as char
 * \return Success as int
 */
int sccp_feat_directpickup(sccp_channel_t * c, char *exten)
{
	int res = 0;
	struct ast_channel *target = NULL;
	struct ast_channel *original = NULL;
	struct ast_channel *tmp = NULL;
	const char * ringermode = NULL;

	sccp_device_t * d;
	char * pickupexten;
	char * cidtmp = NULL, *name = NULL, *number = NULL;

	if(ast_strlen_zero(exten))
	{
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) zero exten\n");
		return -1;
	}

	if(!c || !c->owner)
	{
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) no channel\n");
		return -1;
	}

	original = c->owner;

	if(!c->line || !c->device || !c->device->id || ast_strlen_zero(c->device->id))
	{
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) no device\n");
		return -1;
	}

	d = c->device;

	/* copying extension into our buffer */
	pickupexten = strdup(exten);

	while ((target = ast_channel_walk_locked(target))) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE & SCCP_VERBOSE_LEVEL_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE & SCCP_VERBOSE_LEVEL_HIGH))(VERBOSE_PREFIX_3 "SCCP: (directpickup)\n"
					 "--------------------------------------------\n"
					 "(pickup target)\n"
					 "exten         = %s\n"
					 "context       = %s\n"
					 "pbx           = off\n"
					 "state		    = %d or %d\n"
					 "(current chan)\n"
					 "macro exten   = %s\n"
					 "exten         = %s\n"
					 "macro context = %s\n"
					 "context	    = %s\n"
#ifndef ASTERISK_CONF_1_2
					 "dialcontext   = %s\n"
#endif
					 "pbx		    = %s\n"
					 "state		    = %d\n"
					 "--------------------------------------------\n",
					 pickupexten,
					 !ast_strlen_zero(d->pickupcontext)?d->pickupcontext:"(NULL)",
					 AST_STATE_RINGING,
					 AST_STATE_RING,
					 target->macroexten?target->macroexten:"(NULL)",
					 target->exten?target->exten:"(NULL)",
					 target->macrocontext?target->macrocontext:"(NULL)",
					 target->context?target->context:"(NULL)",
#ifndef ASTERISK_CONF_1_2
					 target->dialcontext?target->dialcontext:"(NULL)",
#endif
					 target->pbx?"on":"off",
					 target->_state);

		if ((!strcasecmp(target->macroexten, pickupexten) || !strcasecmp(target->exten, pickupexten)) &&
#ifdef ASTERISK_CONF_1_2
			((!ast_strlen_zero(d->pickupcontext)?(!strcasecmp(target->context, d->pickupcontext)):1) ||
			 (!ast_strlen_zero(d->pickupcontext)?(!strcasecmp(target->macrocontext, d->pickupcontext)):1)) &&
#else
			((!ast_strlen_zero(d->pickupcontext)?(!strcasecmp(target->dialcontext, d->pickupcontext)):1) ||
			 (!ast_strlen_zero(d->pickupcontext)?(!strcasecmp(target->macrocontext, d->pickupcontext)):1)) &&

#endif
		    (!target->pbx && (target->_state == AST_STATE_RINGING || target->_state == AST_STATE_RING))) {

			tmp = (CS_AST_BRIDGED_CHANNEL(target) ? CS_AST_BRIDGED_CHANNEL(target) : target);

#ifdef CS_AST_CHANNEL_HAS_CID
			if(tmp->cid.cid_name)
				name = strdup(tmp->cid.cid_name);
			if(tmp->cid.cid_num)
				number = strdup(tmp->cid.cid_num);

			ast_log(LOG_NOTICE, "SCCP: %s callerid is ('%s'-'%s') vs copy ('%s'-'%s')\n", tmp->name, tmp->cid.cid_name, tmp->cid.cid_num, name, number);
#else
			if(tmp->callerid) {
				ast_log(LOG_NOTICE, "SCCP: Callerid found in channel %s is %s\n", tmp->name, tmp->callerid);
				cidtmp = strdup(tmp->callerid);
				ast_callerid_parse(cidtmp, &name, &number);
				ast_log(LOG_NOTICE, "SCCP: Callerid copy of channel %s is %s ('%s'-'%s')\n", tmp->name, cidtmp, name, number);
			}
#endif
			tmp = NULL;
			original->hangupcause = AST_CAUSE_CALL_REJECTED;

			res = 0;
			if(d->pickupmodeanswer) {
				if ((res = ast_answer(c->owner))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (directpickup) Unable to answer '%s'\n", c->owner->name);
					res = -1;
				}
				else if ((res = ast_queue_control(c->owner, AST_CONTROL_ANSWER))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (directpickup) Unable to queue answer on '%s'\n", c->owner->name);
					res = -1;
				}
			}

			if(res==0)
			{
				if ((res = ast_channel_masquerade(target, c->owner))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (directpickup) Unable to masquerade '%s' into '%s'\n", c->owner->name, target->name);
					res = -1;
				} else {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (directpickup) Pickup on '%s' by '%s'\n", target->name, c->owner->name);
					c->calltype = SKINNY_CALLTYPE_INBOUND;
					sccp_channel_set_callingparty(c, name, number);
					if(d->pickupmodeanswer) {
						sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONNECTED);
					} else {
						uint8_t	instance;
						instance = sccp_device_find_index_for_line(d, c->line->name);
						sccp_dev_stoptone(d, instance, c->callid);
						sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);

						sccp_device_lock(c);
						d->active_channel = NULL;
						sccp_device_unlock(c);

						sccp_channel_lock(c);
						c->ringermode = SKINNY_STATION_OUTSIDERING;	// default ring
						ringermode = pbx_builtin_getvar_helper(c->owner, "ALERT_INFO");
						if (ringermode && !ast_strlen_zero(ringermode)) {
							sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Found ALERT_INFO=%s\n", ringermode);
							if (strcasecmp(ringermode, "inside") == 0)
								c->ringermode = SKINNY_STATION_INSIDERING;
							else if (strcasecmp(ringermode, "feature") == 0)
								c->ringermode = SKINNY_STATION_FEATURERING;
							else if (strcasecmp(ringermode, "silent") == 0)
								c->ringermode = SKINNY_STATION_SILENTRING;
							else if (strcasecmp(ringermode, "urgent") == 0)
								c->ringermode = SKINNY_STATION_URGENTRING;
						}
						sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_RINGING);
						sccp_channel_unlock(c);
					}
					original->hangupcause = AST_CAUSE_NORMAL_CLEARING;
					ast_setstate(original, AST_STATE_DOWN);
				}
				sccp_ast_channel_unlock(target);
				ast_hangup(original);
			}

			if(name)
				ast_free(name);
			name = NULL;
			if(number)
				ast_free(number);
			number = NULL;
			if(cidtmp)
				ast_free(cidtmp);
			cidtmp = NULL;

			break;
		} else {
			res = -1;
		}
		sccp_ast_channel_unlock(target);
	}
	ast_free(pickupexten);
	pickupexten = NULL;
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) quit\n");
	return res;
}

/*!
 * \brief Handle Group Pickup Feature
 * \param l SCCP Line
 * \param d SCCP Device
 * \return Success as int
 */
int sccp_feat_grouppickup(sccp_line_t * l, sccp_device_t *d)
{
	int res = 0;
	struct ast_channel *target = NULL;
	struct ast_channel *original = NULL;
	const char * ringermode = NULL;

	sccp_channel_t * c;

	char * cidtmp = NULL, *name = NULL, *number = NULL;

	if(!l || !d || !d->id || ast_strlen_zero(d->id)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (grouppickup) no line or device\n");
		return -1;
	}


	if (!l->pickupgroup) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: (grouppickup) pickupgroup not configured in sccp.conf\n", d->id);
		return -1;
	}

	while ((target = ast_channel_walk_locked(target))) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE & SCCP_VERBOSE_LEVEL_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		if ((l->pickupgroup & target->callgroup) &&
		    (!target->pbx && (target->_state == AST_STATE_RINGING || target->_state == AST_STATE_RING))) {

			//  let's allocate a new channel if it's not already up
			sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: Device state is '%s'\n", d->id, devicestatus2str(d->state));
			if(!(c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_OFFHOOK))) {
				c = sccp_channel_allocate(l, d);
				if (!c) {
					ast_log(LOG_ERROR, "%s: (grouppickup) Can't allocate SCCP channel for line %s\n", d->id, l->name);
					sccp_ast_channel_unlock(target);
					return -1;
				}

				if (!sccp_pbx_channel_allocate(c)) {
					ast_log(LOG_WARNING, "%s: (grouppickup) Unable to allocate a new channel for line %s\n", d->id, l->name);
					sccp_ast_channel_unlock(target);
					sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONGESTION);
					return res;
				}

				sccp_channel_set_active(d, c);
				sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_OFFHOOK);

				if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
					sccp_channel_openreceivechannel(c);
				}
			}

			if(!c->owner) {
				sccp_ast_channel_unlock(target);
				res = -1;
				break;
			}

			original = c->owner;

#ifdef CS_AST_CHANNEL_HAS_CID
			if(target->cid.cid_name)
				name = strdup(target->cid.cid_name);
			if(target->cid.cid_num)
				number = strdup(target->cid.cid_num);
#else
			if(target->callerid) {
				cidtmp = strdup(target->callerid);
				ast_callerid_parse(cidtmp, &name, &number);
			}
#endif
#ifndef CS_AST_HAS_TECH_PVT
			if (!strcasecmp(target->type, "SCCP")){
#else
			if (!strcasecmp(target->tech->type, "SCCP")){
#endif
				sccp_channel_t *remote = CS_AST_CHANNEL_PVT(target);
				if(remote){
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (grouppickup) remote channel is SCCP -> correct cid\n");
					sccp_copy_string(name, remote->callingPartyName, sizeof(name));
					sccp_copy_string(number, remote->callingPartyNumber, sizeof(number));
				}
				remote = NULL;
			}
			original->hangupcause = AST_CAUSE_CALL_REJECTED;

			res = 0;
			if(d->pickupmodeanswer) {
				if ((res = ast_answer(c->owner))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (grouppickup) Unable to answer '%s'\n", c->owner->name);
					res = -1;
				}
				else if ((res = ast_queue_control(c->owner, AST_CONTROL_ANSWER))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (grouppickup) Unable to queue answer on '%s'\n", c->owner->name);
					res = -1;
				}
			}

			if(res==0)
			{
				if ((res = ast_channel_masquerade(target, c->owner))) {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (grouppickup) Unable to masquerade '%s' into '%s'\n", c->owner->name, target->name);
					res = -1;
				} else {
					sccp_log(1)(VERBOSE_PREFIX_3  "SCCP: (grouppickup) Pickup on '%s' by '%s'\n", target->name, c->owner->name);
					sccp_channel_lock(c);

					/* searching callerid */
					c->calltype = SKINNY_CALLTYPE_INBOUND;
					sccp_channel_set_callingparty(c, name, number);
					if(d->pickupmodeanswer) {
						sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_CONNECTED);
					} else {
						uint8_t	instance;
						instance = sccp_device_find_index_for_line(d, c->line->name);
						sccp_dev_stoptone(d, instance, c->callid);
						sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
						sccp_device_lock(d);
						d->active_channel = NULL;
						sccp_device_unlock(d);

						c->ringermode = SKINNY_STATION_OUTSIDERING;	// default ring
						ringermode = pbx_builtin_getvar_helper(c->owner, "ALERT_INFO");
						if ( ringermode && !ast_strlen_zero(ringermode) ) {
							sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Found ALERT_INFO=%s\n", ringermode);
							if (strcasecmp(ringermode, "inside") == 0)
								c->ringermode = SKINNY_STATION_INSIDERING;
							else if (strcasecmp(ringermode, "feature") == 0)
								c->ringermode = SKINNY_STATION_FEATURERING;
							else if (strcasecmp(ringermode, "silent") == 0)
								c->ringermode = SKINNY_STATION_SILENTRING;
							else if (strcasecmp(ringermode, "urgent") == 0)
								c->ringermode = SKINNY_STATION_URGENTRING;
						}
						sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_RINGING);
					}
					sccp_channel_unlock(c);
					original->hangupcause = AST_CAUSE_NORMAL_CLEARING;
					ast_setstate(original, AST_STATE_DOWN);
				}
				sccp_ast_channel_unlock(target);
				ast_hangup(original);
			}

			if(name)
				ast_free(name);
			name = NULL;
			if(number)
				ast_free(number);
			number = NULL;
			if(cidtmp)
				ast_free(cidtmp);
			cidtmp = NULL;

			res = 0;

			break;
		} else {
			res = -1;
		}
		sccp_ast_channel_unlock(target);
	}
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (grouppickup) quit\n");
	return res;
}

#endif

/*!
 * \brief Update Caller ID
 * \param c SCCP Channel
 */
void sccp_feat_updatecid(sccp_channel_t * c) {
	struct ast_channel * target = NULL;
	char * cidtmp = NULL, *name = NULL, *number = NULL;

	if(!c || !c->owner)
		return;

	if(c->calltype == SKINNY_CALLTYPE_OUTBOUND)
		target = c->owner;
	else if(!(target = ast_bridged_channel(c->owner))) {
		return;
	}

#ifdef CS_AST_CHANNEL_HAS_CID
	if(target->cid.cid_name)
		name = strdup(target->cid.cid_name);
	if(target->cid.cid_num)
		number = strdup(target->cid.cid_num);
#else
	if(target->callerid) {
		cidtmp = strdup(target->callerid);
		ast_callerid_parse(target, &name, &number);
	}
#endif

	sccp_channel_set_callingparty(c, name, number);

	ast_free(name);
	ast_free(number);
	ast_free(cidtmp);
}

/*!
 * \brief Handle VoiceMail
 * \param d SCCP Device
 * \param line_instance Line Instance as int
 */
void sccp_feat_voicemail(sccp_device_t * d, uint8_t line_instance) {

	sccp_channel_t * c;
	sccp_line_t * l;
	uint8_t		instance;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Voicemail Button pressed on line (%d)\n", d->id, line_instance);

	c = sccp_channel_get_active(d);
	if (c) {
		sccp_channel_lock(c);
		if (!c->line || ast_strlen_zero(c->line->vmnum)) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, line_instance);
			sccp_channel_unlock(c);
			return;
		}
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DIALING) {
			sccp_copy_string(c->dialedNumber, c->line->vmnum, sizeof(c->dialedNumber));
			SCCP_SCHED_DEL(sched, c->digittimeout);
			sccp_channel_unlock(c);
			sccp_pbx_softswitch(c);
			return;
		}
		instance = sccp_device_find_index_for_line(d, c->line->name);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		sccp_channel_unlock(c);
		return;
	}

	if (!line_instance)
		line_instance = 1;

	l = sccp_line_find_byid(d, line_instance);
	if (!l) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, line_instance);
		return;
	}
	if (!ast_strlen_zero(l->vmnum)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Dialing voicemail %s\n", d->id, l->vmnum);
		sccp_channel_newcall(l, d, l->vmnum, SKINNY_CALLTYPE_OUTBOUND);
	} else {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, line_instance);
	}
}

/*!
 * \brief Handle Divert/Transfer Call to VoiceMail
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_feat_idivert(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l){
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: TRANSVM pressed but no line found\n", d->id);
		sccp_dev_displayprompt(d, 0, 0, "No line found to transfer", 5);
		return;
	}
	if (!l->trnsfvm) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: TRANSVM pressed but not configured in sccp.conf\n", d->id);
		return;
	}
	if (!c || !c->owner) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: TRANSVM with no channel active\n", d->id);
		sccp_dev_displayprompt(d, 0, 0, "TRANSVM with no channel active", 5);
		return;
	}

	if (c->state != SCCP_CHANNELSTATE_RINGING && c->state != SCCP_CHANNELSTATE_CALLWAITING) {
		sccp_log((SCCP_VERBOSE_LEVEL_FEATURE))(VERBOSE_PREFIX_3 "%s: TRANSVM pressed in no ringing state\n", d->id);
		return;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: TRANSVM to %s\n", d->id, l->trnsfvm);
#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_set(c->owner, call_forward, l->trnsfvm);
#else
	sccp_copy_string(c->owner->call_forward, l->trnsfvm, sizeof(c->owner->call_forward));
#endif
	ast_setstate(c->owner, AST_STATE_BUSY);
	ast_queue_control(c->owner, AST_CONTROL_BUSY);
}

/*!
 * \brief Handle Conference
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 * \return Success as int
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_feat_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_SCCP_CONFERENCE
	sccp_buttonconfig_t 	*config = NULL;
	sccp_channel_t 		*channel = NULL;
	sccp_selectedchannel_t 	*selectedChannel = NULL;
	sccp_line_t 		*line = NULL;
	boolean_t			selectedFound = FALSE;


	if(!d || !c)
		return;

	sccp_device_lock(d);
	uint8_t num = sccp_device_numberOfChannels(d);
	sccp_device_unlock(d);
	if(num < 2){
		uint8_t instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		return;
	}

	if(!d->conference){
		d->conference = sccp_conference_create(c);
	}


	/* if we have selected channels, add this to conference */
	SCCP_LIST_LOCK(&d->selectedChannels);
	SCCP_LIST_TRAVERSE(&d->selectedChannels, selectedChannel, list) {

		sccp_conference_addParticipant(d->conference, selectedChannel->channel);
		selectedFound = TRUE;
	}
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	/* If no calls were selected, add all calls to the conference, across all lines. */
	if(FALSE == selectedFound) {
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		if(config->type == LINE){
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if(!line)
				continue;

			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, channel, list) {
				if(channel->device == d){
					sccp_conference_addParticipant(d->conference, channel);
				}
			}
			SCCP_LIST_UNLOCK(&line->channels);
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
	}

#else
	/* sorry but this is private code -FS */
	uint8_t instance = sccp_device_find_index_for_line(d, l->name);
	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
	ast_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
#endif
}

/*!
 * \brief Handle Join a Conference
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_feat_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_ADV_FEATURES
	sccp_advfeat_join(d, l, c);
#else
	/* sorry but this is private code -FS */
	uint8_t instance = sccp_device_find_index_for_line(d, l->name);
	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
#endif
}

/*!
 * \brief Handle 3-Way Phone Based Conferencing on a Device
 * \param l SCCP Line
 * \param d SCCP Device
 * \return SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
sccp_channel_t * sccp_feat_handle_meetme(sccp_line_t * l, sccp_device_t *d) {
	sccp_channel_t * c;
	int instance;

	if (!l || !d || !d->id || ast_strlen_zero(d->id)) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {
		// we have a channel, checking if
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, instance, (c && c->callid)?c->callid:0);
			sccp_channel_lock(c);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETMEETMEROOM; /* Simpleswitch will catch a number to be dialed */
			c->ss_data = 0; /* this should be found in thread */
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);
			sccp_channel_unlock(c);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l, d);

	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_meetme) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_GETMEETMEROOM; /* Simpleswitch will catch a number to be dialed */
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	sccp_channel_set_active(d, c);
	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);

	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_meetme) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	return c;
}

/*!
 * \brief Handle Barging into a Call
 * \param l SCCP Line
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t * sccp_feat_handle_barge(sccp_line_t * l, sccp_device_t *d) {
	sccp_channel_t * c;
	int instance;

	if (!l || !d || !d->id || ast_strlen_zero(d->id)) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}


	instance = sccp_device_find_index_for_line(d, l->name);
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {
		// we have a channel, checking if
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, instance, (c && c->callid)?c->callid:0);
			sccp_channel_lock(c);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETBARGEEXTEN; /* Simpleswitch will catch a number to be dialed */
			c->ss_data = 0; /* this should be found in thread */
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);
			sccp_channel_unlock(c);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l, d);

	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_barge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_GETBARGEEXTEN; /* Simpleswitch will catch a number to be dialed */
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	sccp_channel_set_active(d, c);
	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);

	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_barge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	return c;
}

/*!
 * \brief Barging into a Call Feature
 * \param c SCCP Channel
 * \param exten Extention as char
 * \return Success as int
 */
int sccp_feat_barge(sccp_channel_t * c, char *exten) {
#ifdef CS_ADV_FEATURES
	return sccp_advfeat_barge(c, exten);
#else
	/* sorry but this is private code -FS */
	uint8_t instance = sccp_device_find_index_for_line(c->device, c->line->name);
	sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
	return 1;
#endif
}

/*!
 * \brief Handle Barging into a Conference
 * \param l SCCP Line
 * \param d SCCP Device
 * \return SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
sccp_channel_t * sccp_feat_handle_cbarge(sccp_line_t * l, sccp_device_t *d) {
	sccp_channel_t * c;
	int instance;

	if (!l || !d || strlen(d->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {
		// we have a channel, checking if
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, instance, (c && c->callid)?c->callid:0);
			sccp_channel_lock(c);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETCBARGEROOM; /* Simpleswitch will catch a number to be dialed */
			c->ss_data = 0; /* this should be found in thread */
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);
			sccp_channel_unlock(c);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l, d);

	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_cbarge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}

	sccp_channel_lock(c);

	c->ss_action = SCCP_SS_GETCBARGEROOM; /* Simpleswitch will catch a number to be dialed */
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	sccp_channel_set_active(d, c);
	sccp_indicate_nolock(d, c, SCCP_CHANNELSTATE_GETDIGITS);

	sccp_channel_unlock(c);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_cbarge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio) {
		sccp_channel_openreceivechannel(c);
	}

	return c;
}

/*!
 * \brief Barging into a Conference Feature
 * \param c SCCP Channel
 * \param conferencenum Conference Number as char
 * \return Success as int
 */
int sccp_feat_cbarge(sccp_channel_t * c, char *conferencenum) {
#ifdef CS_ADV_FEATURES
	return sccp_advfeat_cbarge(c, conferencenum);
#else
	/* sorry but this is private code -FS */
	uint8_t instance = sccp_device_find_index_for_line(c->device, c->line->name);
	sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
	return 1;
#endif
}
/*!
 * \brief Hotline Feature
 *
 * Setting the hotline Feature on a device, will make it connect to a predefined extension as soon as the Receiver
 * is picked up or the "New Call" Button is pressed. No number has to be given.
 *
 * \param d SCCP Device
 */
void sccp_feat_hotline(sccp_device_t *d, sccp_line_t *line) {
	sccp_channel_t * c = NULL;

	if (!d || !d->session || !line)
		return;

	sccp_log((SCCP_VERBOSE_LEVEL_FEATURE | SCCP_VERBOSE_LEVEL_LINE))(VERBOSE_PREFIX_3 "%s: handling hotline\n", d->id);
	c = sccp_channel_get_active(d);
	if (c) {
		sccp_channel_lock(c);
		if ( (c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) ) {
			sccp_copy_string(c->dialedNumber, line->adhocNumber, sizeof(c->dialedNumber));
			sccp_channel_unlock(c);

			SCCP_SCHED_DEL(sched, c->digittimeout);
			sccp_pbx_softswitch(c);

			return;
		}
		sccp_channel_unlock(c);
		sccp_pbx_senddigits(c, line->adhocNumber);
	} else {
		// Pull up a channel
		if (GLOB(hotline)->line) {
			sccp_channel_newcall(line, d, line->adhocNumber, SKINNY_CALLTYPE_OUTBOUND);
		}
	}
}

/*!
 * \brief Handler to Notify Features have Changed
 * \param device SCCP Device
 * \param featureType SCCP Feature Type
 */
void sccp_feat_changed(sccp_device_t *device, sccp_feature_type_t featureType){
	if(device){


		sccp_featButton_changed(device, featureType);

		sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
		memset(event, 0, sizeof(sccp_event_t));

		event->type=SCCP_EVENT_FEATURECHANGED;
		event->event.featureChanged.device = device;
		event->event.featureChanged.featureType = featureType;
		sccp_event_fire((const sccp_event_t **)&event);

	}


}

/*!
 * \brief Handler to Notify Channel State has Changed
 * \param device SCCP Device
 * \param channel SCCP Channel
 */
void sccp_feat_channelStateChanged(sccp_device_t *device, sccp_channel_t * channel){
	uint8_t state;

	if(!channel || !device)
		return;

	state = channel->state;
	switch (state) {
		case SCCP_CHANNELSTATE_CONNECTED:
			if(device->monitorFeature.enabled && device->monitorFeature.status != channel->monitorEnabled){
				sccp_feat_monitor(device, channel);
			}
		break;

		default:
		break;
	}

}

/*!
 * \brief Feature Monitor
 * \param device SCCP Device
 * \param channel SCCP Channel
 */
void sccp_feat_monitor(sccp_device_t *device, sccp_channel_t *channel){
#if ASTERISK_VERSION_NUM >= 10600
#ifdef CS_SCCP_FEATURE_MONITOR
	struct ast_call_feature *feat;
	struct ast_frame f = { AST_FRAME_DTMF, };
	int j;

	if(!channel)
		return;

	ast_rdlock_call_features();
	feat = ast_find_call_feature("automon");
	if (!feat || ast_strlen_zero(feat->exten)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "Recording requested, but no One Touch Monitor registered. (See features.conf)\n");
		ast_unlock_call_features();
		return;
	}
	f.len = 100;
	for (j=0; j < strlen(feat->exten); j++) {
		f.subclass = feat->exten[j];
		ast_queue_frame(channel->owner, &f);
	}
	ast_unlock_call_features();
	channel->monitorEnabled = (channel->monitorEnabled)?FALSE:TRUE;

	sccp_feat_changed(device, SCCP_FEATURE_MONITOR);
#endif
#endif
}
