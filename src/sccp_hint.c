/*!
 * \file 	sccp_hint.c
 * \brief 	SCCP Hint Class
 * \author 	Marcello Ceschia < marcello.ceschia@users.sourceforge.net >
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \since 	2009-01-16
 * \version 	$LastChangeDate$
 *
 how does hint update works:
 \dot
 digraph updateHint {
	asteriskEvent[ label="asterisk event" shape=rect];
	sccp_hint_state[ label="sccp_hint_state" shape=rect style=rounded];
	and[label="and" shape=circle]
	sccp_hint_list_t[ label="sccp_hint_list_t" shape=circle];
	sccp_hint_remoteNotification_thread[ label="sccp_hint_remoteNotification_thread" shape=rect style=rounded];
	sccp_hint_notifySubscribers[ label="sccp_hint_notifySubscribers" shape=rect style=rounded];
	
	lineStatusChanged[label="line status changed" shape=rect];
	sccp_hint_lineStatusChanged[label="sccp_hint_lineStatusChanged" shape=rect style=rounded];
	sccp_hint_hintStatusUpdate[label="sccp_hint_hintStatusUpdate" shape=rect style=rounded];
	checkShared[label="is shared line?" shape=diamond];
	sccp_hint_notificationForSharedLine[label="sccp_hint_notificationForSharedLine" shape=rect style=rounded];
	sccp_hint_notificationForSingleLine[label="sccp_hint_notificationForSingleLine" shape=rect style=rounded];
	
	
	end[shape=point];
	asteriskEvent -> sccp_hint_state;
	sccp_hint_state -> and;
	and -> sccp_hint_list_t[label="update"];
	and -> sccp_hint_remoteNotification_thread;
	sccp_hint_remoteNotification_thread -> sccp_hint_list_t[label="update"];
	
	lineStatusChanged -> sccp_hint_lineStatusChanged;
	sccp_hint_lineStatusChanged -> sccp_hint_hintStatusUpdate;
	sccp_hint_hintStatusUpdate -> checkShared;
	checkShared -> sccp_hint_notificationForSingleLine[label="no"];
	checkShared -> sccp_hint_notificationForSharedLine[label="yes"];
	sccp_hint_notificationForSingleLine -> sccp_hint_list_t[label="update"];
	sccp_hint_notificationForSharedLine -> sccp_hint_list_t[label="update"];
	
	sccp_hint_list_t -> sccp_hint_notifySubscribers;
	sccp_hint_notifySubscribers -> end;
 }
 \enddot
 */ 


#include "config.h"
#include "sccp_hint.h"
#include "sccp_event.h"
#include "chan_sccp.h"
#include "sccp_utils.h"

#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif

//sccp_hint_t *sccp_hint_create(sccp_device_t *d, uint8_t instance);
void sccp_hint_notifyAsterisk(sccp_line_t *line, sccp_channelState_t state);
static void * sccp_hint_remoteNotification_thread(void *data);


sccp_hint_list_t *sccp_hint_create(char *hint_context, char *hint_exten);
void sccp_hint_subscribeHint(const sccp_device_t *device, const char *hintStr, uint8_t instance);
void sccp_hint_unSubscribeHint(const sccp_device_t *device, const char *hintStr, uint8_t instance);

void sccp_hint_eventListener(const sccp_event_t **event);
void sccp_hint_deviceRegistered(const sccp_device_t *device);
void sccp_hint_deviceUnRegistered(const sccp_device_t *device);

void sccp_hint_notifySubscribers(sccp_hint_list_t *hint);
void sccp_hint_hintStatusUpdate(sccp_hint_list_t *hint);
void sccp_hint_notificationForSharedLine(sccp_hint_list_t *hint);
void sccp_hint_notificationForSingleLine(sccp_hint_list_t *hint);

/*!
 * \brief starting hint-module
 * \todo Handle Do Not Disturb (DND) (TODO MC)
 */
void sccp_hint_module_start(){
	/* */
	SCCP_LIST_HEAD_INIT(&sccp_hint_subscriptions);
	
	sccp_subscribe_event(SCCP_EVENT_DEVICEREGISTERED | SCCP_EVENT_DEVICEUNREGISTERED | SCCP_EVENT_DEVICEDETACHED, sccp_hint_eventListener);
}

/*!
 * \brief stop hint-module
 */
void sccp_hint_module_stop()
{
	sccp_hint_list_t *hint;
	sccp_hint_SubscribingDevice_t *subscriber = NULL;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	while((hint = SCCP_LIST_REMOVE_HEAD(&sccp_hint_subscriptions, list))) {
		if(hint->hintType == ASTERISK){
			ast_extension_state_del(hint->type.asterisk.hintid, NULL);
		}
		while((subscriber = SCCP_LIST_REMOVE_HEAD(&hint->subscribers, list))){
			ast_free(subscriber);
			subscriber = NULL;
		}
		ast_free(hint);
		hint = NULL;
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
}

/*!
 * \brief Event Listener for Hints
 * \param event SCCP Event
 */
void sccp_hint_eventListener(const sccp_event_t **event){
	const sccp_event_t *e = *event;
	sccp_device_t *device;

	if(!e)
		return;

	switch(e->type){
	case SCCP_EVENT_DEVICEREGISTERED:
		device = e->event.deviceRegistered.device;
		
		if(!device)
		{
			ast_log(LOG_ERROR, "Error posting deviceRegistered event (null device)\n");
			return;
		}

		sccp_device_lock(device);
		sccp_hint_deviceRegistered(device);
		sccp_device_unlock(device);
		
		break;

	case SCCP_EVENT_DEVICEUNREGISTERED:
		device = e->event.deviceRegistered.device;
		
		if(!device){
			ast_log(LOG_ERROR, "Error posting deviceUnregistered event (null device)\n");
			return;
		}
		
		sccp_device_lock(device);
		sccp_hint_deviceUnRegistered(device);
		sccp_device_unlock(device);
		
		break;
	case SCCP_EVENT_DEVICEDETACHED:
		sccp_hint_lineStatusChanged(e->event.deviceAttached.line, e->event.deviceAttached.device, NULL, 0, 0);
		break;
	default:
		break;
	}
}


/*!
 * \brief Handle Hints for Device Register
 * \param device SCCP Device
 */
void sccp_hint_deviceRegistered(const sccp_device_t *device){
	sccp_buttonconfig_t *config;

	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {

		if(config->type == SPEEDDIAL){
			if (ast_strlen_zero(config->button.speeddial.hint)){
				continue;
			}
			sccp_hint_subscribeHint(device, config->button.speeddial.hint, config->instance);

		}
	}
}

/*!
 * \brief Handle Hints for Device UnRegister
 * \param device SCCP Device
 */
void sccp_hint_deviceUnRegistered(const sccp_device_t *device){
	sccp_buttonconfig_t *config;

	if(!device)
		return;
	
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {

		if(config->type == SPEEDDIAL){
			if (ast_strlen_zero(config->button.speeddial.hint)){
				continue;
			}
			sccp_hint_unSubscribeHint(device, config->button.speeddial.hint, config->instance);

		}
	}
}


/*!
 * \brief Asterisk Hint Wrapper
 * \param context Context as char
 * \param exten Extension as char
 * \param state Asterisk Extension State
 * \param data Asterisk Data
 */
int sccp_hint_state(char *context, char* exten, enum ast_extension_states state, void *data) {
	sccp_hint_list_t 	*hint = NULL;
	//sccp_moo_t * r;

	hint = data;

	if (state == -1 || !hint ){
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "SCCP: get new hint, but no hint param exists\n");
		return 0;
	}

	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "SCCP: get new hint state %s for %s\n", sccp_extensionstate2str(state), hint->hint_dialplan);
	hint->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;
	/* converting asterisk state -> sccp state */
	switch(state) {
		case AST_EXTENSION_NOT_INUSE:
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			//sccp_copy_string(hint->callInfo.calledPartyName, "", sizeof(hint->callInfo.callingPartyName));
			//sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
			break;
		case AST_EXTENSION_INUSE:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.calledPartyName));

			break;
		case AST_EXTENSION_BUSY:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.calledPartyName));

			break;
		case AST_EXTENSION_UNAVAILABLE:
#ifndef CS_DYNAMIC_SPEEDDIAL
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#else
			hint->currentState = SCCP_CHANNELSTATE_DOWN;
#endif
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));

			break;
#ifdef CS_AST_HAS_EXTENSION_ONHOLD
		case AST_EXTENSION_ONHOLD:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.calledPartyName));

			break;
#endif
#ifdef CS_AST_HAS_EXTENSION_RINGING
		case AST_EXTENSION_RINGING:
#ifndef CS_DYNAMIC_SPEEDDIAL		  
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#else
			hint->currentState = SCCP_CHANNELSTATE_RINGING;
#endif
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));

			break;
		case AST_EXTENSION_RINGING | AST_EXTENSION_INUSE:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_LINE_IN_USE, sizeof(hint->callInfo.calledPartyName));

			break;
#endif
		default:
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));


	}
	//sccp_copy_string(hint->callInfo.calledParty, "", sizeof(hint->callInfo.callingPartyName));
	//sccp_copy_string(hint->callInfo.callingParty, "", sizeof(hint->callInfo.callingPartyName));

	/* push to subscribers */
	sccp_hint_notifySubscribers(hint);

	if(state == AST_EXTENSION_INUSE || state == AST_EXTENSION_BUSY){
		if (hint->type.asterisk.notificationThread  != AST_PTHREADT_NULL) {
			pthread_kill(hint->type.asterisk.notificationThread, SIGURG);
			hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
		}

		if(hint->type.asterisk.notificationThread == AST_PTHREADT_NULL){
			if (ast_pthread_create_background(&hint->type.asterisk.notificationThread, NULL, sccp_hint_remoteNotification_thread, hint) < 0) {
				return -1;
			}
		}
	}

	
	return 0;
}

/*!
 * \brief send hint status to subscriber
 * \param hint SCCP Hint Linked List Pointer
 */
void sccp_hint_notifySubscribers(sccp_hint_list_t *hint){
	sccp_hint_SubscribingDevice_t *subscriber = NULL;
	sccp_moo_t *r;
	
	if(!hint)
		return;

	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "notify subscriber of %s\n", (hint->hint_dialplan)?hint->hint_dialplan:"null");
	
	
	
	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE(&hint->subscribers, subscriber, list){
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "notify device: %s@%d state: %d\n", DEV_ID_LOG(subscriber->device), subscriber->instance, hint->currentState );
		if(!subscriber->device){
			SCCP_LIST_REMOVE(&hint->subscribers, subscriber, list);
			continue;
		}
		

		
//  		if(hint->currentState == SCCP_CHANNELSTATE_ONHOOK){
//  			sccp_dev_set_lamp(subscriber->device, SKINNY_STIMULUS_LINE, subscriber->instance, SKINNY_LAMP_ON);
// 		}else
//  			sccp_dev_set_lamp(subscriber->device, SKINNY_STIMULUS_LINE, subscriber->instance, SKINNY_LAMP_OFF);
		
		
#ifndef CS_DYNAMIC_SPEEDDIAL		
		sccp_device_sendcallstate(subscriber->device, subscriber->instance, 0, hint->currentState, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		
		/* create CallInfoMessage */
		REQ(r, CallInfoMessage);

		/* set callInfo */
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, hint->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, hint->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, hint->callInfo.callingParty, sizeof(r->msg.CallInfoMessage.callingParty));
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, hint->callInfo.calledParty, sizeof(r->msg.CallInfoMessage.calledParty));

		r->msg.CallInfoMessage.lel_lineId   = htolel(subscriber->instance);
		r->msg.CallInfoMessage.lel_callRef  = htolel(0);
		r->msg.CallInfoMessage.lel_callType = htolel(hint->callInfo.calltype);
		sccp_dev_send(subscriber->device, r);
		
#else
		if(subscriber->device->inuseprotocolversion >= 15){
			sccp_speed_t * k = sccp_dev_speed_find_byindex((sccp_device_t *)subscriber->device, subscriber->instance, SKINNY_BUTTONTYPE_SPEEDDIAL);
		  
		  
			REQ(r, SpeedDialStatDynamicMessage);
			r->msg.SpeedDialStatDynamicMessage.lel_instance = htolel(subscriber->instance);
			r->msg.SpeedDialStatDynamicMessage.lel_type = 0x15;
			
			switch(hint->currentState){
				case SCCP_CHANNELSTATE_ONHOOK:
					r->msg.SpeedDialStatDynamicMessage.lel_unknown1 = htolel(1);
				break;
				
				case SCCP_CHANNELSTATE_DOWN:
					r->msg.SpeedDialStatDynamicMessage.lel_unknown1 = htolel(0); /* default state */
				break;
				
				case SCCP_CHANNELSTATE_RINGING:
					r->msg.SpeedDialStatDynamicMessage.lel_unknown1 = htolel(4); /* ringin */
				break;
				
				default:
					r->msg.SpeedDialStatDynamicMessage.lel_unknown1 = htolel(2); /* connected */
				break;
			}
			char displayMessage[100];
			
			/* do not add name for TEMP_FAIL and ONHOOK */
			if(hint->currentState > 2 ){
				sprintf(displayMessage, "%s %s %s", 
					(hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND)?hint->callInfo.calledPartyName : hint->callInfo.callingPartyName,
					(hint->callInfo.calltype == SKINNY_CALLTYPE_OUTBOUND)? "\t<-\t" : "\t->\t",
					(k)?k->name:"unknown speeddial"
				);
			}else{
				sccp_copy_string(displayMessage, (k)?k->name:"unknown speeddial", sizeof(displayMessage));
			}
			
			sccp_copy_string(r->msg.SpeedDialStatDynamicMessage.DisplayName, displayMessage, sizeof(r->msg.SpeedDialStatDynamicMessage.DisplayName));
			sccp_dev_send(subscriber->device, r);
			
			if(k)
				ast_free(k);
		}
#endif
		/*  */
		if(hint->currentState == SCCP_CHANNELSTATE_ONHOOK) {
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_ONHOOK);
		}else{
			sccp_dev_set_keyset(subscriber->device, subscriber->instance, 0, KEYMODE_INUSEHINT);
		}
	}
	SCCP_LIST_UNLOCK(&hint->subscribers);
}

/*!
 * \brief Handle line status change
 * \param line		SCCP Line
 * \param device	SCCP Device
 * \param channel	SCCP Channel
 * \param previousState Previous Channel State
 * \param state		New Channel State
 */
void sccp_hint_lineStatusChanged(sccp_line_t *line, sccp_device_t *device, sccp_channel_t *channel, sccp_channelState_t previousState, sccp_channelState_t state){
	sccp_hint_list_t *hint = NULL;

	if(!line)
		return;

	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list){
		if(	strlen(line->name) == strlen(hint->type.internal.lineName)
			&& !strcmp(line->name, hint->type.internal.lineName)){
			
			/* update hint */
			sccp_hint_hintStatusUpdate(hint);
			/* send to subscriber */
			sccp_hint_notifySubscribers(hint);
		}
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);

	/* notify asterisk */
	sccp_hint_notifyAsterisk(line, state);
}

/*!
 * \brief Handle Hint Status Update
 * \param hint SCCP Hint Linked List Pointer
 */
void sccp_hint_hintStatusUpdate(sccp_hint_list_t *hint){
	sccp_line_t *line = NULL;
	if(!hint)
		return;

	line = sccp_line_find_byname(hint->type.internal.lineName);
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "hint %s@%s has changed\n", hint->exten, hint->context );
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "line %s has %d device%s --> notify %s\n", line->name, line->devices.size, (line->devices.size>1)?"s":"",  (line->devices.size>1)?"shared line change":"single line change");
	
	if( (line->devices.size >1 && line->channels.size > 1) || line->channels.size > 1 ){
		/* line is currently shared */
		sccp_hint_notificationForSharedLine(hint);
	}else{
		/* just one device per line */
		sccp_hint_notificationForSingleLine(hint);
	}

	/* notify asterisk */
	sccp_hint_notifySubscribers(hint);
	sccp_hint_notifyAsterisk(line, hint->currentState);
}

/*!
 * \brief Notify Asterisk of Hint State Change
 * \param line	SCCP Line
 * \param state SCCP Channel State
 */
void sccp_hint_notifyAsterisk(sccp_line_t *line, sccp_channelState_t state){
	if(!line)
		return;
	
#ifdef CS_NEW_DEVICESTATE
//	//TODO set device state
	ast_devstate_changed(sccp_channelState2AstDeviceState(state), "SCCP/%s", line->name);
#else
	ast_device_state_changed("SCCP/%s", line->name);
#endif
}


/* private functions */
/*!
 * \brief set hint status for a line with more then one channel
 * \param hint	SCCP Hint Linked List Pointer
 */
void sccp_hint_notificationForSharedLine(sccp_hint_list_t *hint){
	sccp_line_t *line = sccp_line_find_byname_wo(hint->type.internal.lineName,FALSE);
	sccp_channel_t *channel = NULL;
	
	
	hint->callInfo.calltype = SKINNY_CALLTYPE_OUTBOUND;

	if(!line){
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName,  SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
		return;
	}


	if(line->channels.size > 0){
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "%s: number of active channels %d\n", line->name, line->statistic.numberOfActiveChannels);
		if(line->channels.size == 1){
			channel = SCCP_LIST_FIRST(&line->channels);
			if(!channel){
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName,  SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				return;
			}

			//hint->currentState = channel->state;
			hint->callInfo.calltype = channel->calltype;
			if(channel->state != SCCP_CHANNELSTATE_ONHOOK && channel->state != SCCP_CHANNELSTATE_DOWN){
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				sccp_copy_string(hint->callInfo.callingPartyName, channel->callingPartyName, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, channel->calledPartyName, sizeof(hint->callInfo.calledPartyName));

			}else{
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName,  "", sizeof(hint->callInfo.calledPartyName));
			}
		}else if(line->channels.size > 1){
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_IN_USE_REMOTE, sizeof(hint->callInfo.calledPartyName));
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;

		}
	}else{
		if(line->devices.size == 0){
			sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName,  SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
			//hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#ifndef CS_DYNAMIC_SPEEDDIAL
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#else
			hint->currentState = SCCP_CHANNELSTATE_DOWN;
#endif
		}else{
			hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
			sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
			sccp_copy_string(hint->callInfo.calledPartyName,  "", sizeof(hint->callInfo.calledPartyName));
		}
	}
}


/*!
 * \brief set hint status for a line with less or eq one channel
 * \param hint	SCCP Hint Linked List Pointer
 */
void sccp_hint_notificationForSingleLine(sccp_hint_list_t *hint){
	sccp_line_t 	*line = NULL;
	sccp_channel_t 	*channel = NULL;

	if(!hint)
		return;

	sccp_mutex_lock(&hint->lock);
	line = sccp_line_find_byname_wo(hint->type.internal.lineName,FALSE);
	
	/* no line, or line without devices */
	if(!line || (line && line->devices.size == 0) ){
		sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.callingPartyName));
		sccp_copy_string(hint->callInfo.calledPartyName,  SKINNY_DISP_TEMP_FAIL, sizeof(hint->callInfo.calledPartyName));
		//hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#ifndef CS_DYNAMIC_SPEEDDIAL
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#else
		hint->currentState = SCCP_CHANNELSTATE_DOWN;
#endif

		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "no line or no device; line: %s\n", (line)?line->name:"null");
		goto DONE;
	}
	

	sccp_copy_string(hint->callInfo.callingPartyName, "", sizeof(hint->callInfo.callingPartyName));
	sccp_copy_string(hint->callInfo.calledPartyName,  "", sizeof(hint->callInfo.calledPartyName));

	channel = SCCP_LIST_FIRST(&line->channels);

	if(channel){
		hint->callInfo.calltype = channel->calltype;
		
		//hint->currentState = channel->state;
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE; /* not a good idea to set this to channel->currentState -MC */
		
		sccp_linedevices_t *lineDevice = SCCP_LIST_FIRST(&line->devices);
		sccp_device_t *device = NULL;
		if(lineDevice)
			device = lineDevice->device;

		switch (channel->state) {
			case SCCP_CHANNELSTATE_DOWN:
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				break;
			case SCCP_CHANNELSTATE_OFFHOOK:
				sccp_copy_string(hint->callInfo.callingPartyName,  SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName,  SKINNY_DISP_OFF_HOOK, sizeof(hint->callInfo.calledPartyName));
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
			case SCCP_CHANNELSTATE_GETDIGITS:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
			case SCCP_CHANNELSTATE_SPEEDDIAL:
				break;
			case SCCP_CHANNELSTATE_ONHOOK:
				break;
			case SCCP_CHANNELSTATE_RINGOUT:
			case SCCP_CHANNELSTATE_CONNECTED:
			case SCCP_CHANNELSTATE_PROCEED:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				if(!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {
					
					sccp_copy_string(hint->callInfo.callingPartyName,   channel->callingPartyName, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName,   channel->calledPartyName, sizeof(hint->callInfo.calledPartyName));

					sccp_copy_string(hint->callInfo.callingParty,  channel->callingPartyNumber, sizeof(hint->callInfo.callingParty));
					sccp_copy_string(hint->callInfo.calledParty,  channel->calledPartyNumber, sizeof(hint->callInfo.calledParty));
					

				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
				}
				break;
				
			case SCCP_CHANNELSTATE_RINGING:
#ifndef CS_DYNAMIC_SPEEDDIAL		  
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
#else
				hint->currentState = SCCP_CHANNELSTATE_RINGING;
#endif
				if(!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {
					
					sccp_copy_string(hint->callInfo.callingPartyName,   channel->callingPartyName, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName,   channel->calledPartyName, sizeof(hint->callInfo.calledPartyName));

					sccp_copy_string(hint->callInfo.callingParty,  channel->callingPartyNumber, sizeof(hint->callInfo.callingParty));
					sccp_copy_string(hint->callInfo.calledParty,  channel->calledPartyNumber, sizeof(hint->callInfo.calledParty));
					

				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
				}
				break;
				
			case SCCP_CHANNELSTATE_DIALING:
			case SCCP_CHANNELSTATE_DIGITSFOLL:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				if(!device || device->privacyFeature.enabled == 0 || (device->privacyFeature.enabled == 1 && channel->privacy == FALSE)) {
					
					sccp_copy_string(hint->callInfo.callingPartyName, channel->dialedNumber, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, channel->calledPartyName, sizeof(hint->callInfo.calledPartyName));

					sccp_copy_string(hint->callInfo.callingParty, channel->dialedNumber, sizeof(hint->callInfo.callingParty));
					sccp_copy_string(hint->callInfo.calledParty, channel->calledPartyNumber, sizeof(hint->callInfo.calledParty));
					

				} else {
					sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.callingPartyName));
					sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_RING_OUT, sizeof(hint->callInfo.calledPartyName));
				}
				break;	
			case SCCP_CHANNELSTATE_BUSY:
				//hint->currentState = channel->state;
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_BUSY, sizeof(hint->callInfo.calledPartyName));
				break;
			case SCCP_CHANNELSTATE_HOLD:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				//hint->currentState = channel->state;
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_HOLD, sizeof(hint->callInfo.calledPartyName));
				break;
			case SCCP_CHANNELSTATE_CONGESTION:
			case SCCP_CHANNELSTATE_CALLWAITING:
			case SCCP_CHANNELSTATE_CALLTRANSFER:
			case SCCP_CHANNELSTATE_CALLCONFERENCE:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
			case SCCP_CHANNELSTATE_CALLPARK:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				sccp_copy_string(hint->callInfo.callingPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.callingPartyName));
				sccp_copy_string(hint->callInfo.calledPartyName, SKINNY_DISP_PARK, sizeof(hint->callInfo.calledPartyName));
				break;
			case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE:
			case SCCP_CHANNELSTATE_INVALIDNUMBER:
				hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
				break;
			default:
				hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
				break;
			}


	}else{
		/* no channel -> on hook */
		hint->currentState = SCCP_CHANNELSTATE_ONHOOK;
	}
DONE:
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "set singleLineState to %d\n", hint->currentState);
	sccp_mutex_unlock(&hint->lock);
}
/*!
 * \brief Subscribe to a Hint
 * \param device SCCP Device
 * \param hintStr Asterisk Hint Name as char
 * \param instance Instance as int
 */
void sccp_hint_subscribeHint(const sccp_device_t *device, const char *hintStr, uint8_t instance){
	sccp_hint_list_t *hint = NULL;

	char buffer[256] = "";
	char *splitter, *hint_exten, *hint_context;
	sccp_copy_string(buffer, hintStr, sizeof(buffer));

	
	if(!device){
		ast_log(LOG_ERROR, "adding hint to: %s without device is not allowed\n", hintStr);
		return;
	}

	/* get exten and context */
	splitter = buffer;
	hint_exten = strsep(&splitter, "@");
	if(hint_exten)
		ast_strip(hint_exten);


	hint_context = splitter;
	if(hint_context){
		ast_strip(hint_context);
	}else{
		hint_context = GLOB(context);
	}
	/*  */
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Dialplan %s for exten: %s and context: %s\n", hintStr, hint_exten, hint_context);
	

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&sccp_hint_subscriptions, hint, list){
		if(		strlen(hint_exten) == strlen(hint->exten)
				&& strlen(hint_context) == strlen(hint->context)
				&& !strcmp(hint_exten, hint->exten)
				&& !strcmp(hint_context, hint->context)){
		  
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "Hint found\n");
			break;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;

	/* we have no hint */
	if(!hint){
		hint = sccp_hint_create(hint_exten, hint_context);
		if(!hint)
			return;

		SCCP_LIST_LOCK(&sccp_hint_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_hint_subscriptions, hint, list);
		SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);
	}

	/* add subscribing device */
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "create subscriber\n");
	sccp_hint_SubscribingDevice_t *subscriber;
	subscriber = ast_malloc(sizeof(sccp_hint_SubscribingDevice_t));
	memset(subscriber, 0, sizeof(sccp_hint_SubscribingDevice_t));
	
	subscriber->device = device;
	subscriber->instance = instance;

	SCCP_LIST_INSERT_HEAD(&hint->subscribers, subscriber, list);

	
	sccp_hint_notifySubscribers(hint);
}

/*!
 * \brief Unsubscribe from a Hint
 * \param device SCCP Device
 * \param hintStr Hint String (Extension at Context) as char
 * \param instance Instance as int
 */
void sccp_hint_unSubscribeHint(const sccp_device_t *device, const char *hintStr, uint8_t instance){
	sccp_hint_list_t *hint = NULL;

	char buffer[256] = "";
	char *splitter, *hint_exten, *hint_context;
	sccp_copy_string(buffer, hintStr, sizeof(buffer));


	/* get exten and context */
	splitter = buffer;
	hint_exten = strsep(&splitter, "@");
	if (hint_exten)
		ast_strip(hint_exten);


	hint_context = splitter;
	if (hint_context){
		ast_strip(hint_context);
	}else{
		hint_context = GLOB(context);
	}
	/*  */
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Remove device %s from hint %s for exten: %s and context: %s\n", DEV_ID_LOG(device),hintStr, hint_exten, hint_context);


	SCCP_LIST_LOCK(&sccp_hint_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_hint_subscriptions, hint, list){
		if(		strlen(hint_exten) == strlen(hint->exten)
				&& strlen(hint_context) == strlen(hint->context)
				&& !strcmp(hint_exten, hint->exten)
				&& !strcmp(hint_context, hint->context)){
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_hint_subscriptions);

	if(!hint)
		return;

	sccp_hint_SubscribingDevice_t *subscriber;
	
	SCCP_LIST_LOCK(&hint->subscribers);
	SCCP_LIST_TRAVERSE(&hint->subscribers, subscriber, list){
		if(subscriber->device == device)
			break;
	}
	
	if(subscriber){
		SCCP_LIST_REMOVE(&hint->subscribers, subscriber, list);
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Device removed.\n");
	}
	SCCP_LIST_UNLOCK(&hint->subscribers);
}


/*!
 * \brief create a hint structure
 * \param hint_exten Hint Extension as char
 * \param hint_context Hint Context as char
 * \return SCCP Hint Linked List
 */
sccp_hint_list_t *sccp_hint_create(char *hint_exten, char *hint_context){
	sccp_hint_list_t *hint = NULL;
	char hint_dialplan[256] = "";
	char *splitter;

	if(ast_strlen_zero(hint_exten))
		return NULL;
	
	if(ast_strlen_zero(hint_context))
		hint_context = GLOB(context);
	
	//memset(hint_dialplan, 0, sizeof(hint_dialplan));
	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Create hint for exten: %s context: %s\n", hint_exten, hint_context);

#ifdef CS_AST_HAS_NEW_HINT
	ast_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, 0, NULL, hint_context, hint_exten);
#else
	ast_get_hint(hint_dialplan, sizeof(hint_dialplan) - 1, NULL, hint_context, hint_exten);
#endif
	
	if (ast_strlen_zero(hint_dialplan)) {
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "No hint configuration in the dialplan exten: %s and context: %s\n", hint_exten, hint_context);
		return NULL;
	}
	
	hint = ast_malloc(sizeof(sccp_hint_list_t));
	memset(hint, 0, sizeof(sccp_hint_list_t));
	
	SCCP_LIST_HEAD_INIT(&hint->subscribers);
	sccp_mutex_init(&hint->lock);
	
	sccp_copy_string(hint->exten, hint_exten, sizeof(hint->exten));
	sccp_copy_string(hint->context, hint_context, sizeof(hint->context));
	
	sccp_copy_string(hint->hint_dialplan, hint_dialplan, sizeof(hint_dialplan));



	/* check if we have an internal hint or have to use asterisk hint system */
	if ( strchr(hint_dialplan,'&') || strncasecmp(hint_dialplan,"SCCP",4) ) {
		/* asterisk style hint system */
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Configuring asterisk (no sccp features) hint %s for exten: %s and context: %s\n", hint_dialplan, hint_exten, hint_context);
		
		hint->hintType = ASTERISK;
		hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
		hint->type.asterisk.hintid = ast_extension_state_add(hint_context, hint_exten, sccp_hint_state, hint);
		
		if (hint->type.asterisk.hintid > -1) {
			hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Added hint (ASTERISK), extension %s@%s, device %s\n", hint_exten, hint_context, hint_dialplan);
			
			int state = ast_extension_state(NULL, hint_context, hint_exten);
			sccp_hint_state(hint_context, hint_exten, state, hint);
			
		}else{
			/* error */
			ast_free(hint);
			hint = NULL;
			ast_log(LOG_ERROR, "Error adding hint (ASTERISK) for extension %s@%s and device %s\n", hint_exten, hint_context, hint_dialplan);
			return NULL;
		}
	}else{
		/* SCCP channels hint system. Push */
		hint->hintType = INTERNAL;
		char lineName[256];

		/* what line do we have */
		splitter = hint_dialplan;
		strsep(&splitter, "/");
		sccp_copy_string(lineName, splitter, sizeof(lineName));
		ast_strip(lineName);
		
		/* save lineName */
		sccp_copy_string(hint->type.internal.lineName, lineName, sizeof(hint->type.internal.lineName));
		
		/* set initial state */
		hint->currentState = SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
		
		sccp_line_t *line = sccp_line_find_byname(lineName);
		if (!line) {
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "Error adding hint (SCCP) for line: %s. The line does not exist!\n", hint_dialplan);
		}else{
			sccp_hint_hintStatusUpdate(hint);
		}	
	}

	return hint;
}

/*!
 * \brief thread for searching callinfo on none sccp channels
 * \param data Data
 */
static void * sccp_hint_remoteNotification_thread(void *data){
	sccp_hint_list_t  *hint = data;
	
	sccp_moo_t *r = NULL;
	struct ast_channel *astChannel = NULL;
	struct ast_channel *bridgedChannel = NULL;
	struct ast_channel *foundChannel = NULL;
	uint i = 0;

	if (!hint )
		goto CLEANUP;

	

	sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_3 "searching for callInfos for asterisk channel %s\n", hint->hint_dialplan ? hint->hint_dialplan : "null");
	while ((astChannel = ast_channel_walk_locked(astChannel)) != NULL) {
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) searching for channel on %s\n", hint->hint_dialplan);
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) asterisk channels %s, cid_num: %s, cid_name: %s\n", astChannel->name, astChannel->cid.cid_num, astChannel->cid.cid_name);
		if(strlen(astChannel->name) > strlen(hint->hint_dialplan) && !strncmp(astChannel->name, hint->hint_dialplan, strlen(hint->hint_dialplan))){
			ast_channel_unlock(astChannel);
			foundChannel = astChannel;
			break;
		}
		ast_channel_unlock(astChannel);
	}

	if(foundChannel){
		ast_channel_lock(foundChannel);
		sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) found remote channel %s\n", foundChannel->name);
		i = 0;
		while (foundChannel && (bridgedChannel = ast_bridged_channel(foundChannel)) == NULL && i < 30) {
			if(!ast_channel_unlock(foundChannel))
				goto CLEANUP;
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) searching for bridgedChannel\n");
			sleep(1);
			i++;
			if(!ast_channel_trylock(foundChannel))
				goto CLEANUP;
		}

		if(bridgedChannel){
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) asterisk bridgedChannel %s, cid_num: %s, cid_name: %s\n", bridgedChannel->name, bridgedChannel->cid.cid_num, bridgedChannel->cid.cid_name);

			
			sccp_copy_string(hint->callInfo.callingParty, foundChannel->cid.cid_num, sizeof(hint->callInfo.callingParty));
			if(foundChannel->cid.cid_name)
				sccp_copy_string(hint->callInfo.callingPartyName, foundChannel->cid.cid_name, sizeof(hint->callInfo.callingPartyName));
			else
				sccp_copy_string(hint->callInfo.callingPartyName, foundChannel->cid.cid_num, sizeof(hint->callInfo.callingPartyName));

			sccp_copy_string(hint->callInfo.calledPartyName, bridgedChannel->cid.cid_name, sizeof(hint->callInfo.calledPartyName));
			sccp_copy_string(hint->callInfo.calledParty, bridgedChannel->cid.cid_num, sizeof(hint->callInfo.calledParty));

			sccp_hint_notifySubscribers(hint);

		}else
			sccp_log(SCCP_VERBOSE_LEVEL_HINT)(VERBOSE_PREFIX_4 "(sccp_hint_state) no bridgedChannel channels for %s\n", foundChannel->name);
		ast_channel_unlock(foundChannel);
	}

CLEANUP:
	if(r){
		ast_free(r);
		r = NULL;
	}
	if(hint)
		hint->type.asterisk.notificationThread = AST_PTHREADT_NULL;
	return NULL;
}


