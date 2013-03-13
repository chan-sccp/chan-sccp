
/*!
 * \file        sccp_event.c
 * \brief       SCCP Event Class
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-09-02
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \remarks     Purpose:        SCCP Event
 *              When to use:    Only methods directly related to sccp events should be stored in this source file.
 *              Relationships:  SCCP Hint
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

void sccp_event_destroy(sccp_event_t * event);

/*!
 * \brief SCCP Event Listeners Structure
 */
struct sccp_event_aSyncEventProcessorThreadArg {
	const struct sccp_event_subscriptions *subscribers;
	sccp_event_t *event;
};

struct sccp_event_subscriptions {
	int syncSize;
	sccp_event_subscriber_t *sync;

	int aSyncSize;
	sccp_event_subscriber_t *async;
};

struct sccp_event_subscriptions subscriptions[NUMBER_OF_EVENT_TYPES];

boolean_t sccp_event_running = FALSE;

void sccp_event_destroy(sccp_event_t * event)
{
	//      pbx_log(LOG_NOTICE, "destroy event - %p type: %d: releasing held object references\n", event, event->type);
	switch (event->type) {
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
			event->event.featureChanged.linedevice = event->event.featureChanged.linedevice ? sccp_linedevice_release(event->event.featureChanged.linedevice) : NULL;
			break;

		case SCCP_EVENT_LINESTATUS_CHANGED:
			event->event.lineStatusChanged.line = sccp_line_release(event->event.lineStatusChanged.line);
			event->event.lineStatusChanged.device = event->event.lineStatusChanged.device ? sccp_device_release(event->event.lineStatusChanged.device) : NULL;
			break;

		case SCCP_EVENT_LINE_CHANGED:
		case SCCP_EVENT_LINE_DELETED:
			break;
	}
	//      pbx_log(LOG_NOTICE, "Event destroyed- %p type: %d\n", event, event->type);
}

static void *sccp_event_processor(void *data)
{
	struct sccp_event_aSyncEventProcessorThreadArg *args;
	const struct sccp_event_subscriptions *subscribers;
	sccp_event_t *event;
	int n;

	args = data;

	subscribers = args->subscribers;
	event = args->event;

	sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Processing Asynchronous Event %p of Type %s\n", event, event2str(event->type));
	if ((event = sccp_event_retain(event))) {
		for (n = 0; n < subscribers->aSyncSize && sccp_event_running; n++) {
			if (subscribers->async[n].callback_function != NULL) {
				sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Processing Event %p of Type %s: %p (%d)\n", event, event2str(event->type), subscribers->async[n].callback_function, n);
				subscribers->async[n].callback_function((const sccp_event_t *) event);
			}
		}
		sccp_event_release(event);
	} else {
		sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Could not retain event\n");
	}
	sccp_event_release(args->event);
	sccp_free(data);
	return NULL;
}

void sccp_event_module_start(void)
{
	int i;

	if (!sccp_event_running) {
		for (i = 0; i < NUMBER_OF_EVENT_TYPES; i++) {
			subscriptions[i].async = (sccp_event_subscriber_t *) malloc(sizeof(sccp_event_subscriber_t));
			subscriptions[i].sync = (sccp_event_subscriber_t *) malloc(sizeof(sccp_event_subscriber_t));
		}
		sccp_event_running = TRUE;
	}
}

void sccp_event_module_stop(void)
{
	int i;

	if (sccp_event_running) {
		sccp_event_running = FALSE;
		for (i = 0; i < NUMBER_OF_EVENT_TYPES; i++) {
			subscriptions[i].aSyncSize = 0;
			subscriptions[i].syncSize = 0;
		}
		usleep(20);											// arbitairy time for other threads to finish their business
		for (i = 0; i < NUMBER_OF_EVENT_TYPES; i++) {
			free(subscriptions[i].async);
			free(subscriptions[i].sync);
		}
	}
}

/*!
 * \brief Subscribe to an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back Function
 * \param allowASyncExecution Handle Event Asynchronously (Boolean)
 * 
 * \warning
 *      - sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb, boolean_t allowASyncExecution)
{
	int i, n, size;

	for (i = 0, n = 1 << i; i < NUMBER_OF_EVENT_TYPES && sccp_event_running; i++, n = 1 << i) {
		if (eventType & n) {
			if (allowASyncExecution) {
				size = subscriptions[i].aSyncSize;
				if (size) {
					subscriptions[i].async = (sccp_event_subscriber_t *) realloc(subscriptions[i].async, (size + 1) * sizeof(sccp_event_subscriber_t));
				}
				subscriptions[i].async[size].callback_function = cb;
				subscriptions[i].async[size].eventType = eventType;
				subscriptions[i].aSyncSize++;
			} else {
				size = subscriptions[i].syncSize;
				if (size) {
					subscriptions[i].sync = (sccp_event_subscriber_t *) realloc(subscriptions[i].async, (size + 1) * sizeof(sccp_event_subscriber_t));
				}
				subscriptions[i].sync[size].callback_function = cb;
				subscriptions[i].sync[size].eventType = eventType;
				subscriptions[i].syncSize++;
			}
		}
	}
}

/*!
 * \brief unSubscribe from an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back Function
 */
void sccp_event_unsubscribe(sccp_event_type_t eventType, sccp_event_callback_t cb)
{
	int i, n, x;

	for (i = 0, n = 1 << i; i < NUMBER_OF_EVENT_TYPES; i++, n = 1 << i) {
		if (eventType & n) {
			for (x = 0; x < subscriptions[i].aSyncSize; x++) {
				if (subscriptions[i].async[x].callback_function == cb) {
					subscriptions[i].async[x].callback_function = (void *) NULL;
					subscriptions[i].async[x].eventType = 0;
				}
			}
			for (x = 0; x < subscriptions[i].syncSize; x++) {
				if (subscriptions[i].sync[x].callback_function == cb) {
					subscriptions[i].sync[x].callback_function = (void *) NULL;
					subscriptions[i].sync[x].eventType = 0;
				}
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
 *      - sccp_event_listeners->subscriber is not always locked
 */
void sccp_event_fire(const sccp_event_t * event)
{
	if (event == NULL || FALSE == sccp_refcount_isRunning() || !sccp_event_running)
		return;

	sccp_event_t *e = (sccp_event_t *) sccp_refcount_object_alloc(sizeof(sccp_event_t), SCCP_REF_EVENT, event2str(event->type), sccp_event_destroy);

	if (!e) {
		pbx_log(LOG_ERROR, "%p: Memory Allocation Error while creating sccp_event e. Exiting\n", event);
		return;
	}
	//      memcpy(e, event, sizeof(sccp_event_t));
	e->type = event->type;

	sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Handling Event %p of Type %s\n", event, event2str(e->type));
	/* update refcount */
	switch (e->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
		case SCCP_EVENT_DEVICE_UNREGISTERED:
		case SCCP_EVENT_DEVICE_PREREGISTERED:
			e->event.deviceRegistered.device = event->event.deviceRegistered.device;
			break;

		case SCCP_EVENT_LINE_CREATED:
			e->event.lineCreated.line = event->event.lineCreated.line;
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
		case SCCP_EVENT_DEVICE_DETACHED:
			e->event.deviceAttached.linedevice = event->event.deviceAttached.linedevice;
			break;

		case SCCP_EVENT_FEATURE_CHANGED:
			e->event.featureChanged.device = event->event.featureChanged.device;
			e->event.featureChanged.linedevice = event->event.featureChanged.linedevice;
			e->event.featureChanged.featureType = event->event.featureChanged.featureType;
			break;

		case SCCP_EVENT_LINESTATUS_CHANGED:
			e->event.lineStatusChanged.line = event->event.lineStatusChanged.line;
			e->event.lineStatusChanged.device = event->event.lineStatusChanged.device;
			e->event.lineStatusChanged.state = event->event.lineStatusChanged.state;
			break;

		case SCCP_EVENT_LINE_CHANGED:
		case SCCP_EVENT_LINE_DELETED:
			break;
	}

	/* search for position in array */
	int i, n;
	sccp_event_type_t eventType = event->type;

	for (i = 0, n = 1 << i; i < NUMBER_OF_EVENT_TYPES; i++, n = 1 << i) {
		if (eventType & n) {
			break;
		}
	}

	//      pthread_attr_t tattr;
	//      pthread_t tid;

	/* start async thread if nessesary */
	if (GLOB(module_running)) {
		if (subscriptions[i].aSyncSize > 0 && sccp_event_running) {
			/* create thread for async subscribers */
			struct sccp_event_aSyncEventProcessorThreadArg *arg = malloc(sizeof(struct sccp_event_aSyncEventProcessorThreadArg));

			if (!arg) {
				pbx_log(LOG_ERROR, "%p: Memory Allocation Error while creating sccp_event_aSyncEventProcessorThreadArg. Skipping\n", event);
			} else {
				arg->subscribers = &subscriptions[i];
				arg->event = sccp_event_retain(e);

				/* initialized with default attributes */
				if (arg->event != NULL) {
					sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Adding work to threadpool for event: %p, type: %s\n", event, event2str(event->type));
					if (!sccp_threadpool_add_work(GLOB(general_threadpool), (void *) sccp_event_processor, (void *) arg)) {
						pbx_log(LOG_ERROR, "Could not add work to threadpool for event: %p, type: %s for processing\n", event, event2str(event->type));
						arg->event = sccp_event_release(arg->event);
						sccp_free(arg);
					}
				} else {
					pbx_log(LOG_ERROR, "Could not retain e: %p, type: %s for processing\n", e, event2str(event->type));
					sccp_free(arg);
				}
			}
		}

		/* execute sync subscribers */
		if ((e = sccp_event_retain(e))) {
			for (n = 0; n < subscriptions[i].syncSize && sccp_event_running; n++) {
				if (subscriptions[i].sync[n].callback_function != NULL) {
					subscriptions[i].sync[n].callback_function((const sccp_event_t *) e);
				}
			}
			sccp_event_release(e);
		} else {
			pbx_log(LOG_ERROR, "Could not retain e: %p, type: %s for processing\n", e, event2str(event->type));
		}
	} else {
		// we are unloading. switching to synchonous mode for everything
		sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Handling Event %p of Type %s in Forced Synchronous Mode\n", event, event2str(e->type));
		if ((e = sccp_event_retain(e))) {
			for (n = 0; n < subscriptions[i].syncSize && sccp_event_running; n++) {
				if (subscriptions[i].sync[n].callback_function != NULL) {
					subscriptions[i].sync[n].callback_function((const sccp_event_t *) e);
				}
			}
			for (n = 0; n < subscriptions[i].aSyncSize && sccp_event_running; n++) {
				if (subscriptions[i].async[n].callback_function != NULL) {
					subscriptions[i].async[n].callback_function((const sccp_event_t *) e);
				}
			}
			sccp_event_release(e);
		} else {
			pbx_log(LOG_ERROR, "Could not retain e: %p, type: %s for processing\n", e, event2str(event->type));
		}
	}
	sccp_event_release(e);
}
