/*!
 * \file 	sccp_mwi.c
 * \brief 	SCCP Message Waiting Indicator Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$  
 */
 
#include "config.h"
#include "sccp_mwi.h"
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_utils.h"
#include "sccp_lock.h"
#include "sccp_device.h"
#include "sccp_config.h"
#include "sccp_actions.h"
#include "sccp_event.h"

#ifdef CS_AST_HAS_EVENT
#include "asterisk/event.h"
#endif

#include <asterisk/app.h>

void sccp_mwi_checkLine(sccp_line_t *line);
void sccp_mwi_setMWILineStatus(sccp_device_t * d, sccp_line_t * l);
void sccp_mwi_linecreatedEvent(const sccp_event_t **event);
void sccp_mwi_deviceAttachedEvent(const sccp_event_t **event);
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t *line);
void sccp_mwi_lineStatusChangedEvent(const sccp_event_t **event);



SCCP_LIST_HEAD(, sccp_mailbox_subscriber_list_t) sccp_mailbox_subscriptions;


/*!
 * start mwi module.
 */
void sccp_mwi_module_start(void){
  /* */
	SCCP_LIST_HEAD_INIT(&sccp_mailbox_subscriptions);
  
	sccp_event_subscribe(SCCP_EVENT_LINECREATED, sccp_mwi_linecreatedEvent);
	sccp_event_subscribe(SCCP_EVENT_DEVICEATTACHED, sccp_mwi_deviceAttachedEvent);
	sccp_event_subscribe(SCCP_EVENT_LINESTATUSCHANGED, sccp_mwi_lineStatusChangedEvent);
}

/*!
 * \brief Stop MWI Monitor
 */
void sccp_mwi_module_stop(){
	sccp_mailbox_subscriber_list_t 	*subscription = NULL;
	sccp_mailboxLine_t		*sccp_mailboxLine = NULL;
  
	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	while ((subscription = SCCP_LIST_REMOVE_HEAD(&sccp_mailbox_subscriptions, list))) {

		/* removing lines */
		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		while ((sccp_mailboxLine = SCCP_LIST_REMOVE_HEAD(&subscription->sccp_mailboxLine, list))){
			ast_free(sccp_mailboxLine);
		}
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_HEAD_DESTROY(&subscription->sccp_mailboxLine);

#ifdef CS_AST_HAS_EVENT		
		/* unsubscribe asterisk event */
		if(subscription->event_sub){
			ast_event_unsubscribe(subscription->event_sub);
		}
#else
		SCCP_SCHED_DEL(sched, subscription->schedUpdate);
#endif


		ast_free(subscription);
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_HEAD_DESTROY(&sccp_mailbox_subscriptions);	
}





#ifdef CS_AST_HAS_EVENT
/*!
 * \brief Receive MWI Event from Asterisk
 * \param event Asterisk Event
 * \param data Asterisk Data
 */
void sccp_mwi_event(const struct ast_event *event, void *data){
	sccp_mailbox_subscriber_list_t *subscription = data;
	sccp_mailboxLine_t 	*mailboxLine = NULL;
	sccp_line_t		*line=NULL;

	ast_log(LOG_NOTICE, "Got mwi-event\n");
	if(!subscription || !event)
		return;

	sccp_log(DEBUGCAT_EVENT)(VERBOSE_PREFIX_3 "Got mwi event for %s@%s\n",(subscription->mailbox)?subscription->mailbox:"NULL", (subscription->context)?subscription->context:"NULL");


	/* for calculation store previous voicemail counts */
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	subscription->currentVoicemailStatistic.newmsgs = ast_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	subscription->currentVoicemailStatistic.oldmsgs = ast_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);

	SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list){
		line = mailboxLine->line;
		if(line){

			sccp_line_lock( line );
			sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_4 "line: %s\n", line->name);
			sccp_linedevices_t *lineDevice = NULL;

			/* update statistics for line  */
			line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

			line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
			/* done */

			/* notify each device on line */
			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->devices, lineDevice, list){
				if(0 != lineDevice && 0 != lineDevice->device) {
					sccp_mwi_setMWILineStatus(lineDevice->device, line);
				} else {
					sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_4 "error: null line device.\n");
				}

			}
			SCCP_LIST_TRAVERSE_SAFE_END;
			sccp_line_unlock( line );
		}
	}
	SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
}
#else

/*!
 * \brief MWI Progress
 * \param ptr Pointer to Mailbox Subscriber list Entry
 */
int sccp_mwi_checksubscription(const void *ptr){
	sccp_mailbox_subscriber_list_t 	*subscription = (sccp_mailbox_subscriber_list_t *)ptr;
	sccp_line_t			*line=NULL;
	sccp_mailboxLine_t 		*mailboxLine = NULL;
	if(!subscription)
		return -1;
	
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;
	
	char buffer[512];
	sprintf(buffer, "%s@%s", subscription->mailbox, (subscription->context)?subscription->context:"default");
	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_4 "SCCP: ckecking mailbox: %s\n", buffer);
	ast_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);

	/* update devices if something changed */
	if(subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs){
		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list){
			line = mailboxLine->line;
			if(line){

				sccp_line_lock( line );
				sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_4 "line: %s\n", line->name);
				sccp_linedevices_t *lineDevice = NULL;

				/* update statistics for line  */
				line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
				line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

				line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
				line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
				/* done */

				/* notify each device on line */
				SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->devices, lineDevice, list){
					sccp_mwi_setMWILineStatus(lineDevice->device, line);
				}
				SCCP_LIST_TRAVERSE_SAFE_END;
				sccp_line_unlock( line );
			}
		}
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	}
		
		
	/* reschedule my self */
	if( (subscription->schedUpdate = sccp_sched_add(sched, 30 * 1000, sccp_mwi_checksubscription, subscription)) < 0 ) {
		ast_log(LOG_ERROR, "Error creating mailbox subscription.\n");
	}
	return 0;
}
#endif



/*!
 * \brief Remove Mailbox Subscription
 * \param mailbox SCCP Mailbox
 * \todo Implement sccp_mwi_unsubscribeMailbox (TODO Marcello)
 */
void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t **mailbox){

	//TODO implement sccp_mwi_unsubscribeMailbox
	return;


}


void sccp_mwi_deviceAttachedEvent(const sccp_event_t **event){
	if(!(*event))
		return;

	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_1 "Get deviceAttachedEvent\n");
	sccp_line_t *line = (*event)->event.deviceAttached.line;
	sccp_device_t *device = (*event)->event.deviceAttached.device;

	if(!line || !device){
		ast_log(LOG_ERROR, "get deviceAttachedEvent where one parameter is missing. device: %s, line: %s\n", DEV_ID_LOG(device), (line)?line->name:"null");
		return;
	}

	sccp_device_lock(device);
	device->voicemailStatistic.oldmsgs += line->voicemailStatistic.oldmsgs;
	device->voicemailStatistic.newmsgs += line->voicemailStatistic.newmsgs;
	sccp_device_unlock(device);
	//sccp_mwi_setMWILineStatus(device, line); /* set mwi-line-status */
}


void sccp_mwi_lineStatusChangedEvent(const sccp_event_t **event){
	if(!(*event))
		return;

	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_1 "Get lineStatusChangedEvent\n");
	sccp_device_t *device = (*event)->event.lineStatusChanged.device;

	if(!device)
		return;
	
	sccp_mwi_check(device);
}


void sccp_mwi_linecreatedEvent(const sccp_event_t **event){
	if(!(*event))
		return;

	sccp_mailbox_t *mailbox;
	sccp_line_t *line = (*event)->event.lineCreated.line;
	
	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_1 "Get linecreatedEvent\n");

	if(!line){
		ast_log(LOG_ERROR, "Get linecreatedEvent, but line not set\n");
		return;
	}

	if(line && (&line->mailboxes) != NULL){
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&line->mailboxes, mailbox, list){
			sccp_mwi_addMailboxSubscription(mailbox->mailbox, (mailbox->context)?mailbox->context:"default", line);
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
	}
	return;
}



void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t *line){
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_mailboxLine_t	*mailboxLine = NULL;
	
	
	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_mailbox_subscriptions, subscription, list){
		if(		strlen(mailbox) == strlen(subscription->mailbox)
				&& strlen(context) == strlen(subscription->context)
				&& !strcmp(mailbox, subscription->mailbox)
				&& !strcmp(context, subscription->context)){
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);


	if(!subscription){
		subscription = ast_malloc(sizeof(sccp_mailbox_subscriber_list_t));
		memset(subscription, 0, sizeof(sccp_mailbox_subscriber_list_t));

		
		SCCP_LIST_HEAD_INIT(&subscription->sccp_mailboxLine);

		sccp_copy_string(subscription->mailbox, mailbox, sizeof(subscription->mailbox));
		sccp_copy_string(subscription->context, context, sizeof(subscription->context));
		sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "create subscription for: %s@%s\n", subscription->mailbox, subscription->context);
		
		SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_mailbox_subscriptions, subscription, list);
		SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);


		/* get initial value */
		char buffer[512];
		sprintf(buffer, "%s@%s", subscription->mailbox, (subscription->context)?subscription->context:"default");
		ast_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);

		
#ifdef CS_AST_HAS_EVENT
		/* register asterisk event */
		subscription->event_sub = ast_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, subscription,
										AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox,
										AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, S_OR(subscription->context, "default"),
										AST_EVENT_IE_END);
#else
		if( (subscription->schedUpdate = sccp_sched_add(sched, 30 * 1000, sccp_mwi_checksubscription, subscription)) < 0 ) {
			ast_log(LOG_ERROR, "Error creating mailbox subscription.\n");
		}
										
#endif
	}

	/* we already have this subscription */
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list){
		if(line == mailboxLine->line)
			break;
	}

	if(!mailboxLine){
		mailboxLine = ast_malloc(sizeof(sccp_mailboxLine_t));
		memset(mailboxLine, 0, sizeof(sccp_mailboxLine_t));
		
		
		mailboxLine->line = line;

		line->voicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
		line->voicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;


		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_INSERT_HEAD(&subscription->sccp_mailboxLine, mailboxLine, list);
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	}
}



/*!
 * \brief Check Line for MWI Status
 * \param line SCCP Line
 */
void sccp_mwi_checkLine(sccp_line_t *line){
	sccp_mailbox_t *mailbox=NULL;
	char buffer[512];

	SCCP_LIST_LOCK(&line->mailboxes);
	SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list){
		sprintf(buffer, "%s@%s", mailbox->mailbox, (mailbox->context)?mailbox->context:"default");
		sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "Line: %s, Mailbox: %s\n",line->name, buffer);
		if (!ast_strlen_zero(buffer)) {

#ifdef CS_AST_HAS_NEW_VOICEMAIL
			ast_app_inboxcount(buffer, &line->voicemailStatistic.newmsgs, &line->voicemailStatistic.oldmsgs);
#else
			if(ast_app_has_voicemail(buffer)){
				line->voicemailStatistic.newmsgs = 1;
			}
#endif

			sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "Line: %s, Mailbox: %s inbox: %d\n",line->name, buffer, line->voicemailStatistic.newmsgs);
		}
	}
	SCCP_LIST_UNLOCK(&line->mailboxes);
}

/*!
 * \brief Set MWI Line Status
 * \param d SCCP Device
 * \param l SCCP Line
 */
void sccp_mwi_setMWILineStatus(sccp_device_t * d, sccp_line_t * l)
{
	sccp_moo_t * r;
	int instance;
	boolean_t hasMail;

	if (!d)
		return;

	//sccp_device_lock(d);
	int retry = 0;
	while(sccp_device_trylock(d)) {
		retry++;
		sccp_log((DEBUGCAT_MWI + DEBUGCAT_HIGH))(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s), retry: %d\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__, retry);
		usleep(100);
		
		if(retry > 100){
		     return;
		}
	}
	
	/* when l is defined we are switching on/off the button icon */
	if(l)
		instance = sccp_device_find_index_for_line(d, l->name);
	else
		instance = 0;

	REQ(r, SetLampMessage);
	r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
	r->msg.SetLampMessage.lel_stimulusInstance = htolel(instance);

	if(l){
		r->msg.SetLampMessage.lel_lampMode = htolel( (l->voicemailStatistic.newmsgs) ? SKINNY_LAMP_ON :  SKINNY_LAMP_OFF);
		hasMail = l->voicemailStatistic.newmsgs?TRUE:FALSE;
	}else{
		r->msg.SetLampMessage.lel_lampMode = htolel( (d->mwilight) ? d->mwilamp :  SKINNY_LAMP_OFF);
		hasMail = d->mwilight?TRUE:FALSE;
	}
	sccp_dev_send(d, r);
	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s)%d\n",DEV_ID_LOG(d), hasMail ? "ON" : "OFF", (l ? l->name : "unknown"),(l ? instance : 0));
	
	
	/* set mwi light */
	if(d->mwilight != hasMail){
		d->mwilight = hasMail;
		REQ(r, SetLampMessage);
		r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		r->msg.SetLampMessage.lel_stimulusInstance = 0;
		r->msg.SetLampMessage.lel_lampMode = htolel( (d->mwilight) ? d->mwilamp :  SKINNY_LAMP_OFF);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s) %d\n",DEV_ID_LOG(d), hasMail ? "ON" : "OFF", "unknown",(l ? instance : 0));
	}	
	/* */ 
	
	sccp_dev_check_displayprompt(d);
	sccp_device_unlock(d);
}

/*!
 * \brief Check MWI Status for Device
 * \param device SCCP Device
 */
void sccp_mwi_check(sccp_device_t *device)
{
	sccp_buttonconfig_t	*buttonconfig = NULL;
	sccp_line_t		*line = NULL;
	uint hasNewMessage = 0;

	if(!device)
		return;

	
	
	
	/* check if we have an active channel */
	boolean_t hasActiveChannel = FALSE, hasRinginChannel = FALSE;
	sccp_buttonconfig_t	*config;
	sccp_channel_t		*c;
	
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if(config->type == LINE){
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if(!line)
				continue;


			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, c, list) {
				if(c->device == device && c->state != SCCP_CHANNELSTATE_ONHOOK && c->state != SCCP_CHANNELSTATE_DOWN && c->state != SCCP_CHANNELSTATE_RINGING){
					hasActiveChannel = TRUE;
				}
				if(c->device == device && c->state == SCCP_CHANNELSTATE_RINGING)
					hasRinginChannel = TRUE;
			}
			SCCP_LIST_UNLOCK(&line->channels);
			
			if(hasActiveChannel)
				break;
		}
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);
	
	/* disable mwi light if we have an active channel, but no ringin */
	if((hasActiveChannel == TRUE && hasRinginChannel != TRUE) && !device->mwioncall){
		sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: we have an active channel, disable mwi light\n",DEV_ID_LOG(device));
		if(device->mwilight){
			sccp_moo_t *r;
			REQ(r, SetLampMessage);
			r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
			r->msg.SetLampMessage.lel_stimulusInstance = 0;
			r->msg.SetLampMessage.lel_lampMode = htolel(SKINNY_LAMP_OFF);
			sccp_dev_send(device, r);
			sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s) %d\n",DEV_ID_LOG(device), "OFF", "unknown", 0);
			return;
		}
	}		
	/* */ 
	
	
	sccp_device_lock(device);
	device->voicemailStatistic.newmsgs = 0;
	device->voicemailStatistic.oldmsgs = 0;


	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, buttonconfig, list) {
		if(buttonconfig->type == LINE ){
			line = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
			if(line){

				sccp_mwi_setMWILineStatus(device, line); /* set mwi-line-status */
				device->voicemailStatistic.oldmsgs += line->voicemailStatistic.oldmsgs;
				device->voicemailStatistic.newmsgs += line->voicemailStatistic.newmsgs;

				sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: line %s mwi-status: %s (%d)\n",DEV_ID_LOG(device), line->name, line->voicemailStatistic.newmsgs ? "ON" : "OFF", line->voicemailStatistic.newmsgs);
				if(line->voicemailStatistic.newmsgs){
					hasNewMessage = 1;
					sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: device has voicemail: %d\n",DEV_ID_LOG(device), line->voicemailStatistic.newmsgs);
				}
				sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: current device mwi-state is: %s\n",DEV_ID_LOG(device), device->mwilight ? "ON" : "OFF");
				if(device->mwilight != hasNewMessage){
					sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_3 "%s: set device mwi-state to: %s\n",DEV_ID_LOG(device), hasNewMessage ? "ON" : "OFF");
					device->mwilight = hasNewMessage;
					sccp_mwi_setMWILineStatus(device, NULL);
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);
	sccp_device_unlock(device);
}

