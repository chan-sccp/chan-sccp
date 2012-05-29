
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

void sccp_event_destroy(sccp_event_t *event);

/*!
 * \brief SCCP Event Listeners Structure
 */
struct sccp_event_aSyncEventProcessorThreadArg{
	const struct sccp_event_subscribtions *subscribers;
	sccp_event_t *event;
};

struct sccp_event_subscribtions{
	int syncSize;
	sccp_event_subscriber_t *sync;
	
	int aSyncSize;
	sccp_event_subscriber_t *async;
};

struct sccp_event_subscribtions subscribtions[NUMBER_OF_EVENT_TYPES];

void sccp_event_destroy(sccp_event_t *event)
{
// 	pbx_log(LOG_NOTICE, "destroy event - %p type: %d: releasing held object references\n", event, event->type);
	switch(event->type){
		case SCCP_EVENT_DEVICE_REGISTERED:
		case SCCP_EVENT_DEVICE_UNREGISTERED:
		case SCCP_EVENT_DEVICE_PREREGISTERED:
			event->event.deviceRegistered.device = sccp_device_release(event->event.deviceRegistered.device);
			break;

		case SCCP_EVENT_LINE_CREATED:
			event->event.lineCreated.line = sccp_line_release(event->event.lineCreated.line);
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
		case SCCP_EVENT_DEVICE_DETACHED:
			//event->event.deviceAttached.linedevice = (event->event.deviceAttached.linedevice);
			event->event.deviceAttached.linedevice = sccp_linedevice_release(event->event.deviceAttached.linedevice);
			break;
		
		case SCCP_EVENT_FEATURE_CHANGED:
			event->event.featureChanged.device = sccp_device_release(event->event.featureChanged.device);
			break;
		
		case SCCP_EVENT_LINESTATUS_CHANGED:
			event->event.lineStatusChanged.line = sccp_line_release(event->event.lineStatusChanged.line);
			event->event.lineStatusChanged.device = sccp_device_release(event->event.lineStatusChanged.device);
			break;
		
		case SCCP_EVENT_LINE_CHANGED:
		case SCCP_EVENT_LINE_DELETED:
			break;
	}
// 	pbx_log(LOG_NOTICE, "Event destroyed- %p type: %d\n", event, event->type);
}

static void *sccp_event_processor(void *data){
	struct sccp_event_aSyncEventProcessorThreadArg *args;
	const struct sccp_event_subscribtions *subscribers;
	sccp_event_t *event;
	int i;
	
	args = data;
	
	subscribers = args->subscribers;
	event = args->event;
	
	sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Processing Event %p of Type %s\n", event, event2str(event->type));
	if ((event = sccp_event_retain(event))) {
		for(i = 0; i < subscribers->aSyncSize; i++){
			subscribers->async[i].callback_function((const sccp_event_t *)event);
		}
		sccp_event_release(event);
	}
	sccp_event_release(args->event);
	sccp_free(data);
	return NULL;
}

/*!
 * \brief Subscribe to an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back
 * 
 * \warning
 * 	- sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb, boolean_t allowASyncExecution)
{
	int i, n, size;
	for(i = 0, n = 1<<i; i < NUMBER_OF_EVENT_TYPES; i++, n = 1<<i){
		if(eventType & n){
			if(allowASyncExecution){
				size = subscribtions[i].aSyncSize;
				subscribtions[i].async = (sccp_event_subscriber_t *) realloc(subscribtions[i].async, (size+1) * sizeof(sccp_event_subscriber_t));
				subscribtions[i].async[size].callback_function = cb;
				subscribtions[i].async[size].eventType = eventType;
				subscribtions[i].aSyncSize++;
			}else{
				size = subscribtions[i].syncSize;
				subscribtions[i].sync = (sccp_event_subscriber_t *) realloc(subscribtions[i].async, (size+1) * sizeof(sccp_event_subscriber_t));
				subscribtions[i].sync[size].callback_function = cb;
				subscribtions[i].sync[size].eventType = eventType;
				subscribtions[i].syncSize++;
			}
		}
	}
}

/*!
 * \brief Fire an Event
 * \param event SCCP Event
 * \note event will be freed after event is fired
 * 
 * \warning
 * 	- sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_fire(const sccp_event_t *event)
{
	if (event == NULL)
		return;

	sccp_event_t *e = (sccp_event_t *)sccp_refcount_object_alloc(sizeof(sccp_event_t),"event", event2str(event->type), sccp_event_destroy);
	if (!e) {
		pbx_log(LOG_ERROR, "%p: Memory Allocation Error while creating sccp_event e. Exiting\n", event);
		return;
	}
// 	memcpy(e, event, sizeof(sccp_event_t));
	e->type = event->type;

	sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Handling Event %p of Type %s\n", event, event2str(e->type));
	/* update refcount */
	switch(e->type){
		case SCCP_EVENT_DEVICE_REGISTERED:
		case SCCP_EVENT_DEVICE_UNREGISTERED:
		case SCCP_EVENT_DEVICE_PREREGISTERED:
			e->event.deviceRegistered.device = sccp_device_retain(event->event.deviceRegistered.device);
			break;

		case SCCP_EVENT_LINE_CREATED:
			e->event.lineCreated.line = sccp_line_retain(event->event.lineCreated.line);
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
		case SCCP_EVENT_DEVICE_DETACHED:
			e->event.deviceAttached.linedevice = sccp_linedevice_retain(event->event.deviceAttached.linedevice);
			break;
		
		case SCCP_EVENT_FEATURE_CHANGED:
			e->event.featureChanged.device = sccp_device_retain(event->event.featureChanged.device);
			e->event.featureChanged.featureType = event->event.featureChanged.featureType;
			break;
		
		case SCCP_EVENT_LINESTATUS_CHANGED:
			e->event.lineStatusChanged.line = sccp_line_retain(event->event.lineStatusChanged.line);
			e->event.lineStatusChanged.device = sccp_device_retain(event->event.lineStatusChanged.device);
			e->event.lineStatusChanged.state = event->event.lineStatusChanged.state;
			break;
		
		case SCCP_EVENT_LINE_CHANGED:
		case SCCP_EVENT_LINE_DELETED:
			break;
	}
	
	/* search for position in array */
	int i, n;
	sccp_event_type_t eventType = event->type;
	for(i = 0, n = 1<<i; i < NUMBER_OF_EVENT_TYPES; i++, n = 1<<i){
		if(eventType & n){
			break;
		}
	}
	
//	pthread_attr_t tattr;
//	pthread_t tid;

	/* start async thread if nessesary */
	if(subscribtions[i].aSyncSize > 0){
		/* create thread for async subscribers */
		struct sccp_event_aSyncEventProcessorThreadArg *arg = malloc(sizeof(struct sccp_event_aSyncEventProcessorThreadArg));
		if (!arg) {
			pbx_log(LOG_ERROR, "%p: Memory Allocation Error while creating sccp_event_aSyncEventProcessorThreadArg. Skipping\n", event);
		} else {
			arg->subscribers = &subscribtions[i];
			arg->event = sccp_event_retain(e);

			/* initialized with default attributes */
//			pthread_attr_init(&tattr);
//			pthread_create(&tid, &tattr, sccp_event_aSyncProviderThread, arg);
			if (arg->event != NULL) {
				if (!sccp_threadpool_add_work(GLOB(general_threadpool), (void*)sccp_event_processor, (void *)arg)) {
					arg->event=sccp_event_release(arg->event);
					sccp_free(arg);
				}
			} else {
				sccp_free(arg);
			}
		}
	}
	
	
	/* execute sync subscribers */
	if ((e = sccp_event_retain(e))) {
		for(n = 0; i < subscribtions[i].syncSize; n++){
			subscribtions[i].sync[n].callback_function( (const sccp_event_t *)e);
		}
		sccp_event_release(e);
	}	
	sccp_event_release(e);
}
