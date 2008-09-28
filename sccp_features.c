/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * Features Code by Federico Santulli 
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 *
 * Part of this features code rests private due to free effort of implementing
 *
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
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

sccp_channel_t * sccp_feat_handle_callforward(sccp_line_t * l, uint8_t type) {
	sccp_channel_t * c = NULL;
	sccp_device_t * d;	
	pthread_attr_t attr;
	pthread_t t;
	struct ast_channel * bridge = NULL;
	
	if (!l || !l->device || strlen(l->device->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;
	
	if(l->cfwd_type != SCCP_CFWD_NONE && l->cfwd_type == type) /* if callforward is active and you asked about the same callforward maybe you would disable */
	{
		/* disable call_forward */
		sccp_line_cfwd(l, SCCP_CFWD_NONE, NULL);
		sccp_dev_check_displayprompt(d);
		return NULL;
	}
	else 
	{
		if(type == SCCP_CFWD_NOANSWER)
		{
			sccp_log(10)(VERBOSE_PREFIX_3 "### CFwdNoAnswer NOT SUPPORTED\n");
			sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
			return NULL;
		}
	}
	
	/* look if we have a call  */
	if ((c = sccp_channel_get_active(d))) {				
		// we have a channel, checking if 
		if (c->state == SCCP_CHANNELSTATE_RINGOUT || c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED || c->state == SCCP_CHANNELSTATE_BUSY || c->state == SCCP_CHANNELSTATE_CONGESTION) {
			if(c->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				// if we have an outbound call, we can set callforward to dialed number -FS				
				if(c->dialedNumber && !ast_strlen_zero(c->dialedNumber)) { // checking if we have a number !
					sccp_line_cfwd(l, type, c->dialedNumber);		
					// we are on call, so no tone has been played until now :)
					sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, l->instance, 0, 0);				
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
					free(cidtmp);
					
				if(name)
					free(name);
#endif			
				if(number) {
					sccp_line_cfwd(l, type, number);
					// we are on call, so no tone has been played until now :)	
					sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, l->instance, 0, 0);
					free(number);
					return NULL;
				}				
				// if we where here it's cause there is no number in callerid,, so put call on hold and ask for a call forward number :) -FS
				if (!sccp_channel_hold(c))
				{
					// if can't hold  it means there is no active call, so return as we're already waiting a number to dial
					sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);				
					return NULL; 
				}
			}		
		} else if(c->state == SCCP_CHANNELSTATE_OFFHOOK && ast_strlen_zero(c->dialedNumber)) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, l->instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETFORWARDEXTEN; /* Simpleswitch will catch a number to be dialed */	
			c->ss_data = type; /* this should be found in thread */					
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_GETDIGITS);
			return c;
		} else {
			// we cannot allocate a channel, or ask an extension to pickup.
			sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
			return NULL;
		}
	}
	
	// if we where here there is no call in progress, so we should allocate a channel.	
	c = sccp_channel_allocate(l);

	if(!c) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}

	c->ss_action = SCCP_SS_GETFORWARDEXTEN; /* Simpleswitch will catch a number to be dialed */	
	c->ss_data = type; /* this should be found in thread */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_GETDIGITS);
	
	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_callforward) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* let's wait for an extension */
	if (ast_pthread_create(&t, &attr, sccp_channel_simpleswitch_thread, c)) {
		ast_log(LOG_WARNING, "%s: (handle_callforward) Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}

	return c;
}

#ifdef CS_SCCP_PICKUP
sccp_channel_t * sccp_feat_handle_directpickup(sccp_line_t * l) {
	sccp_channel_t * c;
	sccp_device_t * d;
	pthread_attr_t attr;
	pthread_t t;

	if (!l || !l->device || strlen(l->device->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;
	
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {	
		// we have a channel, checking if 
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, l->instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETPICKUPEXTEN; /* Simpleswitch will catch a number to be dialed */	
			c->ss_data = 0; /* this should be found in thread */					
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_GETDIGITS);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l);
	
	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_directpickup) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	
	c->ss_action = SCCP_SS_GETPICKUPEXTEN; /* Simpleswitch will catch a number to be dialed */	
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_GETDIGITS);
	
	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_directpickup) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* let's call it */
	if (ast_pthread_create(&t, &attr, sccp_channel_simpleswitch_thread, c)) {
		ast_log(LOG_WARNING, "%s: (handle_directpickup) Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}

	return c;
}
#endif

#ifdef CS_SCCP_PICKUP
int sccp_feat_directpickup(sccp_channel_t * c, char *exten) {
	int res = 0;
	struct ast_channel *target = NULL;
	struct ast_channel *original = NULL;
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
	
	if(!c->device || !c->device->id || strlen(c->device->id) < 3)
	{
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) no device\n");
		return -1;
	}
	
	d = c->device;
	
	/* copying extension into our buffer */
	pickupexten = strdup(exten);
	
	while ((target = ast_channel_walk_locked(target))) {
		sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		sccp_log(73)(VERBOSE_PREFIX_3 "SCCP: (directpickup)\n"
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
						sccp_indicate_nolock(c, SCCP_CHANNELSTATE_CONNECTED);
					} else {
						sccp_dev_stoptone(d, c->line->instance, c->callid);
						sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
						d->active_channel = NULL;
					
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
						sccp_indicate_nolock(c, SCCP_CHANNELSTATE_RINGING);
					}
					original->hangupcause = AST_CAUSE_NORMAL_CLEARING;
					ast_setstate(original, AST_STATE_DOWN);
				}
				sccp_ast_channel_unlock(target);
				ast_hangup(original);
			}
			
			if(name)
				free(name);
			name = NULL;
			if(number)
				free(number);
			number = NULL;	
			if(cidtmp)
				free(cidtmp);								
			cidtmp = NULL;
			
			break;
		} else {
			res = -1;
		}		
		sccp_ast_channel_unlock(target);
	}	
	free(pickupexten);
	pickupexten = NULL;
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (directpickup) quit\n");
	return res;
}

int sccp_feat_grouppickup(sccp_line_t * l) {
	int res = 0;
	struct ast_channel *target = NULL;
	struct ast_channel *original = NULL;
	const char * ringermode = NULL;
	
	sccp_channel_t * c;
	sccp_device_t * d;
	
	char * cidtmp = NULL, *name = NULL, *number = NULL;
		
	if(!l || !l->device || strlen(l->device->id) < 3) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (grouppickup) no line or device\n");
		return -1;
	}

	d = l->device;	
			
	if (!l->pickupgroup) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: (grouppickup) pickupgroup not configured in sccp.conf\n", d->id);		
		return -1;
	}
	
	while ((target = ast_channel_walk_locked(target))) {
		sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
		if ((l->pickupgroup & target->callgroup) &&
		    (!target->pbx && (target->_state == AST_STATE_RINGING || target->_state == AST_STATE_RING))) {

			//  let's allocate a new channel if it's not already up
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Device state is '%s'\n", d->id, skinny_devicestate2str(d->state));			
			if(!(c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_OFFHOOK))) {
				c = sccp_channel_allocate(l);			
				if (!c) {
					ast_log(LOG_ERROR, "%s: (grouppickup) Can't allocate SCCP channel for line %s\n", d->id, l->name);
					sccp_ast_channel_unlock(target);
					return -1;
				}
				
				c->hangupok = 1;

				if (!sccp_pbx_channel_allocate(c)) {
					ast_log(LOG_WARNING, "%s: (grouppickup) Unable to allocate a new channel for line %s\n", d->id, l->name);
					sccp_ast_channel_unlock(target);
					sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
					return res;
				}
				
				sccp_channel_set_active(c);
				sccp_indicate_lock(c, SCCP_CHANNELSTATE_OFFHOOK);

				if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
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
					c->calltype = SKINNY_CALLTYPE_INBOUND;					
					sccp_channel_set_callingparty(c, name, number);
					if(d->pickupmodeanswer) {
						sccp_indicate_nolock(c, SCCP_CHANNELSTATE_CONNECTED);
					} else {
						sccp_dev_stoptone(d, c->line->instance, c->callid);
						sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
						d->active_channel = NULL;
											
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
						sccp_indicate_nolock(c, SCCP_CHANNELSTATE_RINGING);
					}
					original->hangupcause = AST_CAUSE_NORMAL_CLEARING;
					ast_setstate(original, AST_STATE_DOWN);
				}
				sccp_ast_channel_unlock(target);
				ast_hangup(original);
			}
			
			if(name)
				free(name);
			name = NULL;
			if(number)
				free(number);
			number = NULL;	
			if(cidtmp)
				free(cidtmp);								
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
	
	free(name);
	free(number);
	free(cidtmp);		
}

void sccp_feat_voicemail(sccp_device_t * d, int line_instance) {

}

void sccp_feat_idivert(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!l->trnsfvm) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM pressed but not configured in sccp.conf\n", d->id);
		return;
	}
	if (!c || !c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM with no channel active\n", d->id);
		return;
	}
	if (c->state != SCCP_CHANNELSTATE_RINGING && c->state != SCCP_CHANNELSTATE_CALLWAITING) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM pressed in no ringing state\n", d->id);
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

void sccp_feat_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_ADV_FEATURES
	sccp_adv_feat_conference(d, l, c);
#else
	/* sorry but this is private code -FS */
	sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
#endif
}

void sccp_feat_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_ADV_FEATURES
	sccp_adv_feat_join(d, l, c);
#else
	/* sorry but this is private code -FS */
	sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
#endif
}

sccp_channel_t * sccp_feat_handle_meetme(sccp_line_t * l) {
	sccp_channel_t * c;
	sccp_device_t * d;
	pthread_attr_t attr;
	pthread_t t;

	if (!l || !l->device || strlen(l->device->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;
	
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {	
		// we have a channel, checking if 
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, l->instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETMEETMEROOM; /* Simpleswitch will catch a number to be dialed */	
			c->ss_data = 0; /* this should be found in thread */					
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_GETDIGITS);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l);
	
	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_meetme) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	
	c->ss_action = SCCP_SS_GETMEETMEROOM; /* Simpleswitch will catch a number to be dialed */	
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_GETDIGITS);
	
	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_meetme) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* let's call it */
	if (ast_pthread_create(&t, &attr, sccp_channel_simpleswitch_thread, c)) {
		ast_log(LOG_WARNING, "%s: (handle_meetme) Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}

	return c;
}

sccp_channel_t * sccp_feat_handle_barge(sccp_line_t * l) {
	sccp_channel_t * c;
	sccp_device_t * d;
	pthread_attr_t attr;
	pthread_t t;

	if (!l || !l->device || strlen(l->device->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;
	
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {	
		// we have a channel, checking if 
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, l->instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETBARGEEXTEN; /* Simpleswitch will catch a number to be dialed */	
			c->ss_data = 0; /* this should be found in thread */					
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_GETDIGITS);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l);
	
	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_barge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	
	c->ss_action = SCCP_SS_GETBARGEEXTEN; /* Simpleswitch will catch a number to be dialed */	
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_GETDIGITS);
	
	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_barge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* let's call it */
	if (ast_pthread_create(&t, &attr, sccp_channel_simpleswitch_thread, c)) {
		ast_log(LOG_WARNING, "%s: (handle_barge) Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}

	return c;
}

int sccp_feat_barge(sccp_channel_t * c, char *exten) {
#ifdef CS_ADV_FEATURES
	return sccp_adv_feat_barge(c, exten);
#else
	/* sorry but this is private code -FS */
	sccp_dev_displayprompt(c->device, c->line->instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
	return 1;
#endif
}

sccp_channel_t * sccp_feat_handle_cbarge(sccp_line_t * l) {
	sccp_channel_t * c;
	sccp_device_t * d;
	pthread_attr_t attr;
	pthread_t t;

	if (!l || !l->device || strlen(l->device->id) < 3){
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;
	
	/* look if we have a call */
	if ( (c = sccp_channel_get_active(d)) ) {	
		// we have a channel, checking if 
		if(c->state == SCCP_CHANNELSTATE_OFFHOOK && (!c->dialedNumber || (c->dialedNumber && ast_strlen_zero(c->dialedNumber)))) {
 			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(d, l->instance, (c && c->callid)?c->callid:0);
			// changing SS_DIALING mode to SS_GETFORWARDEXTEN
			c->ss_action = SCCP_SS_GETCBARGEROOM; /* Simpleswitch will catch a number to be dialed */	
			c->ss_data = 0; /* this should be found in thread */					
			// changing channelstate to GETDIGITS
			sccp_indicate_nolock(c, SCCP_CHANNELSTATE_GETDIGITS);
			return c;
		} else {
			/* there is an active call, let's put it on hold first */
			if (!sccp_channel_hold(c))
				return NULL;
		}
	}

	c = sccp_channel_allocate(l);
	
	if (!c) {
		ast_log(LOG_ERROR, "%s: (handle_cbarge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	
	c->ss_action = SCCP_SS_GETCBARGEROOM; /* Simpleswitch will catch a number to be dialed */	
	c->ss_data = 0; /* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_GETDIGITS);
	
	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: (handle_cbarge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* let's call it */
	if (ast_pthread_create(&t, &attr, sccp_channel_simpleswitch_thread, c)) {
		ast_log(LOG_WARNING, "%s: (handle_cbarge) Unable to create switch thread for channel (%s-%08x) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}

	return c;
}

int sccp_feat_cbarge(sccp_channel_t * c, char *conferencenum) {
#ifdef CS_ADV_FEATURES
	return sccp_adv_feat_barge(c, conferencenum);
#else
	/* sorry but this is private code -FS */
	sccp_dev_displayprompt(c->device, c->line->instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
	return 1;
#endif
}

