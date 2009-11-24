/*
 * sccp_event.c
 *
 *  Created on: 03.09.2009
 *      Author: marcello
 */
#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_event.h"

#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#endif


sccp_event_t *sccp_event_create(sccp_event_type_t eventType){
	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type = eventType;
	return event;
}

void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb,
			     const char *file, const char *caller, int line){

	sccp_event_subscriber_t *subscription = NULL;

	sccp_log(1)(VERBOSE_PREFIX_1 "[SCCP] %s:%d %s register event listener for %d\n", file, line, caller, eventType);

	subscription = ast_malloc(sizeof(sccp_event_subscriber_t));
	if(!subscription){
		ast_log(LOG_ERROR, "fail to alloc mem for subscription\n");
		return;
	}

	subscription->callback_function = cb;
	subscription->eventType = eventType;

	SCCP_LIST_INSERT_TAIL(&sccp_event_listeners->subscriber, subscription, list);
}
/**
 *
 * \remarks event will be freed after event is fired
 */
void sccp_event_fire(sccp_event_t *event){
	if( event == NULL)
		return;

	
	sccp_event_type_t type = event->type;
	sccp_event_subscriber_t *subscriber;

	
	//SCCP_LIST_LOCK(&sccp_event_listeners->subscriber);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&sccp_event_listeners->subscriber, subscriber, list){
		if(subscriber->eventType & type){
			//pthread_t tfd;
			//ast_pthread_create_background(&tfd, NULL, subscriber->callback_function, event);
			subscriber->callback_function(event);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	//SCCP_LIST_UNLOCK(&sccp_event_listeners->subscriber);

	ast_free(event);
}
