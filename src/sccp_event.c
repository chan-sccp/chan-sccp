/*!
 * \file 	sccp_event.c
 * \brief 	SCCP Event Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \since	2009-09-02
 * \date        $Date$
 * \version     $Revision$  
 */

#include "config.h"
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

/*!
 * \brief Subscribe to an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back
 * \param file File as char
 * \param caller Caller as char
 * \param line Line as int
 */
void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb,
			     const char *file, const char *caller, int line){

	sccp_event_subscriber_t *subscription = NULL;

	sccp_log(1)(VERBOSE_PREFIX_1 "[SCCP] %s:%d %s register event listener for %d\n", file, line, caller, eventType);
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

/*!
 * \brief Fire an Event
 * \param event SCCP Event
 * \note event will be freed after event is fired
 */
void sccp_event_fire(const sccp_event_t* *event){
	if( *event == NULL)
		return;

	sccp_event_type_t type = (*event)->type;
	sccp_event_subscriber_t *subscriber;

	sccp_log(1)(VERBOSE_PREFIX_1 "[SCCP] Fire event %d\n", type);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&sccp_event_listeners->subscriber, subscriber, list){
		sccp_log(1)(VERBOSE_PREFIX_1 "eventtype: %d listenerType: %d, -> result: %d %s\n", type, subscriber->eventType, (subscriber->eventType & type), (subscriber->eventType & type)?"true":"false" );
		if(subscriber->eventType & type){
			subscriber->callback_function(event);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;

	free((void *) *event);
}
