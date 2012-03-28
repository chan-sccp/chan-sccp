
/*!
 * \file 	sccp_event.c
 * \brief 	SCCP Event Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since	2009-09-02
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \remarks	Purpose: 	SCCP Event
 * 		When to use:	Only methods directly related to sccp events should be stored in this source file.
 *   		Relationships: 	SCCP Hint
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/*!
 * \brief SCCP Event Listeners Structure
 */
struct sccp_event_subscriptions *sccp_event_listeners = 0;

/*!
 * \brief Subscribe to an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back
 * 
 * \warning
 * 	- sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb)
{

	sccp_event_subscriber_t *subscription = NULL;

	sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_1 "[SCCP] register event listener for %d\n", eventType);

	subscription = ast_malloc(sizeof(sccp_event_subscriber_t));
	if (!subscription) {
		ast_log(LOG_ERROR, "Failed to allocate memory for subscription\n");
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
 * 
 * \warning
 * 	- sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_fire(const sccp_event_t * *event)
{
	if (*event == NULL)
		return;

	sccp_event_type_t type = (*event)->type;

	sccp_event_subscriber_t *subscriber;

	sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_2 "[SCCP] Fire event %d\n", type);
	SCCP_LIST_TRAVERSE(&sccp_event_listeners->subscriber, subscriber, list) {
		sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "eventtype: %d listenerType: %d, -> result: %d %s\n", type, subscriber->eventType, (subscriber->eventType & type), (subscriber->eventType & type) ? "true" : "false");
		if (subscriber->eventType & type) {
			subscriber->callback_function(event);
		}
	}

#ifdef HAVE_LIBGC
	*event = NULL;
#else
	ast_free((void *)*event);
#endif
}
