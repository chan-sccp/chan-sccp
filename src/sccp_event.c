/*!
 * \file	sccp_event.c
 * \brief       SCCP Event Class
 * \author      Marcello Ceschia <marcello [at] ceschia.de>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since       2009-09-02
 */

/*!
 * \remarks
 * Purpose:     SCCP Event
 * When to use: Only methods directly related to sccp events should be stored in this source file.
 * Relations:   SCCP Hint
 */

#include <config.h>
#include "common.h"
#include "sccp_device.h"
#include "sccp_event.h"
#include "sccp_line.h"
#include "sccp_vector.h"

SCCP_FILE_VERSION(__FILE__, "");

void sccp_event_destroy(sccp_event_t * event);
#define SCCP_EVENT_EXPECTED_SUBSCRIPTIONS 9			/* grep sccp_event_subscribe *.c */

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
#define NUMBER_OF_EVENT_TYPES 11				/* grep SCCP_EVENT sccp_enum.in */
#else
#define NUMBER_OF_EVENT_TYPES 10				/* grep SCCP_EVENT sccp_enum.in */
#endif
/* type declarations */
typedef struct sccp_event_subscriber sccp_event_subscriber_t;
typedef struct sccp_event_subscriptions sccp_event_subscriptions_t;
typedef SCCP_VECTOR_RW(, sccp_event_subscriber_t) sccp_event_vector_t;

/* vector compare functions */
#define SUBSCRIBER_CB_CMP(elem, value) ((elem).callback_function == (value))
#define SUBSCRIBER_EXEC_CMP(elem, value) ((elem).execution == (value))

/*!
 * \brief Execution Mode Enum
 */
typedef enum {
	SCCP_EVENT_ASYNC = 1,
	SCCP_EVENT_SYNC = 2,
} sccp_event_execution_mode_t;

/*!
 * \brief SCCP Event Subscriber Structure
 */
struct sccp_event_subscriber {
	sccp_event_type_t eventType;
	sccp_event_execution_mode_t execution;
	sccp_event_callback_t callback_function;
};

/*!
 * \brief SCCP Event Subscriptions Structure
 */
static struct sccp_event_subscriptions {
	sccp_event_vector_t subscribers;
							// same as: SCCP_VECTOR_RW(sccp_event_vector, sccp_event_subscriber_t) subscribers;
							// typedef struct sccp_event_vector sccp_event_vector_t;
							// but using predeclared type instead
} event_subscriptions[NUMBER_OF_EVENT_TYPES] = {{{0}}};

/*
 * \brief release held references when we are finished processing this event
 */
void sccp_event_destroy(sccp_event_t * event)
{
	//pbx_log(LOG_NOTICE, " - %p type: %d: releasing held object references\n", event, event->type);
	switch (event->type) {
		case SCCP_EVENT_DEVICE_REGISTERED:
		case SCCP_EVENT_DEVICE_UNREGISTERED:
		case SCCP_EVENT_DEVICE_PREREGISTERED:
			sccp_device_release(&event->event.deviceRegistered.device);							/* explicit release */
			break;

		case SCCP_EVENT_LINE_CREATED:
			sccp_line_release(&event->event.lineCreated.line);								/* explicit release */
			break;

		case SCCP_EVENT_DEVICE_ATTACHED:
		case SCCP_EVENT_DEVICE_DETACHED:
			sccp_linedevice_release(&event->event.deviceAttached.linedevice);						/* explicit release */
			break;

		case SCCP_EVENT_FEATURE_CHANGED:
			sccp_device_release(&event->event.featureChanged.device);							/* explicit release */
			if (event->event.featureChanged.optional_linedevice) {
				sccp_linedevice_release(&event->event.featureChanged.optional_linedevice);				/* explicit release */
			}
			break;

		case SCCP_EVENT_LINESTATUS_CHANGED:
			sccp_line_release(&event->event.lineStatusChanged.line);							/* explicit release */
			if (event->event.lineStatusChanged.optional_device) {
				sccp_device_release(&event->event.lineStatusChanged.optional_device);					/* explicit release */
			}
			break;

		case SCCP_EVENT_LINE_CHANGED:
		case SCCP_EVENT_LINE_DELETED:
			break;
#if CS_TEST_FRAMEWORK
		case SCCP_EVENT_TEST:
			pbx_log(LOG_NOTICE, "SCCP: TestEvent Destroy Event\n");
			if (event->event.TestEvent.str) {
				sccp_free(event->event.TestEvent.str);
			}
			break;
#endif
		case SCCP_EVENT_TYPE_SENTINEL:
		case SCCP_EVENT_NULL:
			break;
	}
	//pbx_log(LOG_NOTICE, "destroyed- %p type: %d\n", event, event->type);
}

static volatile boolean_t sccp_event_running = FALSE;

//static void __attribute__((constructor)) sccp_event_module_init(void)
void sccp_event_module_start(void)
{
	uint _idx = 0;
	if (!sccp_event_running) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Starting event system\n");
		for (_idx = 0; _idx < NUMBER_OF_EVENT_TYPES; _idx++) {
			if (SCCP_VECTOR_RW_INIT(&event_subscriptions[_idx].subscribers, SCCP_EVENT_EXPECTED_SUBSCRIPTIONS) != 0) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return;
			}
		}
		sccp_event_running = TRUE;
	}
}

//static void __attribute__((destructor)) sccp_event_module_destroy(void)
void sccp_event_module_stop(void)
{
	uint _idx = 0;
	if (sccp_event_running) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Stopping event system\n");
		sccp_event_running = FALSE;
		for (_idx = 0; _idx < NUMBER_OF_EVENT_TYPES; _idx++) {
			SCCP_VECTOR_RW_FREE(&event_subscriptions[_idx].subscribers);
		}
	}
}

/*!
 * \brief Subscribe to an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back Function
 * \param allowAsyncExecution Handle Event Asynchronously (Boolean)
 */
boolean_t sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb, boolean_t allowAsyncExecution)
{
	boolean_t res = FALSE;
	uint8_t _idx; 
	sccp_event_type_t _mask;
	for (_idx = 0, _mask = 1 << _idx; sccp_event_running && _idx < NUMBER_OF_EVENT_TYPES; _mask = 1 << ++_idx) {
		if(eventType & _mask) {
			//sccp_log(DEBUGCAT_EVENT)(VERBOSE_PREFIX_3 "SCCP: (sccp_event_subscribe) Adding %s with callback:%p to vector at idx:%d\n", sccp_event_type2str(eventType), cb, _idx);
			sccp_event_subscriber_t subscriber = {
				.callback_function = cb,
				.eventType = eventType,
				.execution = allowAsyncExecution ? SCCP_EVENT_ASYNC : SCCP_EVENT_SYNC,
			};
			
			sccp_event_vector_t *subscribers = &(event_subscriptions[_idx].subscribers);
			SCCP_VECTOR_RW_WRLOCK(subscribers);
			if (SCCP_VECTOR_APPEND(subscribers, subscriber) == 0) {
				res = TRUE;
			} else {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
			}
			SCCP_VECTOR_RW_UNLOCK(subscribers);
		}
	}
	return res;
}

/*!
 * \brief unSubscribe from an Event
 * \param eventType SCCP Event Type
 * \param cb SCCP Event Call Back Function
 */
boolean_t sccp_event_unsubscribe(sccp_event_type_t eventType, sccp_event_callback_t cb)
{
	boolean_t res = FALSE;
	uint8_t _idx; 
	sccp_event_type_t _mask;
	//sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "SCCP: (sccp_event_unsubscribe) Removing %s.\n", sccp_event_type2str(eventType))
	for (_idx = 0, _mask = 1 << _idx; sccp_event_running && _idx < NUMBER_OF_EVENT_TYPES; _mask = 1 << ++_idx) {
		if (eventType & _mask) {
			sccp_event_vector_t *subscribers = &(event_subscriptions[_idx].subscribers);
			{
				SCCP_VECTOR_RW_WRLOCK(subscribers);
				if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(subscribers, cb, SUBSCRIBER_CB_CMP, SCCP_VECTOR_ELEM_CLEANUP_NOOP) == 0) {
					res = TRUE;
				} else {
					pbx_log(LOG_ERROR, "SCCP: (sccp_event_subscribe) Failed to remove subscriber from subscribers vector\n");
				}
				SCCP_VECTOR_RW_UNLOCK(subscribers);
			}
		}
	}
	return res;
}

/* helpers */
/*!
 * \brief execute the callback off each subscriber in the subscribers array, for a particular event
 * \note should be handed an copied non-lockedable vector
 */
static gcc_inline boolean_t __execute_callback_helper(const sccp_event_t *event, sccp_event_vector_t *subs_vector) 
{
	boolean_t res = FALSE;
	if (subs_vector) {
		uint32_t n = 0;
		for (n = 0; n < SCCP_VECTOR_SIZE(subs_vector) && sccp_event_running; n++) {
			sccp_event_subscriber_t subscriber = SCCP_VECTOR_GET(subs_vector, n);
			if (subscriber.callback_function != NULL) {
				//sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "Processing Event %p of Type %s via %d callback:%p\n", event, sccp_event_type2str(event->type), n, subscriber.callback_function);
				subscriber.callback_function(event);
				res = TRUE;
			}
		}
		SCCP_VECTOR_PTR_FREE(subs_vector);
	}
	return res;
}

/*!
 * search for position in event_subscriptions[] array 
 */
static gcc_inline uint8_t __search_for_position_in_event_array(sccp_event_type_t eventType) {
	uint8_t _idx = 0;
	sccp_event_type_t _mask;
	for (_idx = 0, _mask = 1 << _idx; sccp_event_running && _idx < NUMBER_OF_EVENT_TYPES; _mask = 1 << ++_idx) {
		if (eventType & _mask) {
			//sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "SCCP: (__search_for_position_in_event_array) found index:%d\n", _idx);
			break;
		}
	}
	return _idx;
}
/* end helpers */

/*!
 * async thread arguments
 */
typedef struct __aSyncEventProcessorThreadArg
{
	uint8_t idx;
	sccp_event_t event;
	sccp_event_vector_t *async_subscribers;
} AsyncArgs_t;
/*!
 * async thread run within threadpool
 */
static void *sccp_event_processor(void *data)
{
	AsyncArgs_t *arg = data;
	if (arg) {
		//sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "Async Processing Event Callbacks Type %s\n", sccp_event_type2str(arg->event.type));
		__execute_callback_helper(&arg->event, arg->async_subscribers);
		sccp_event_destroy(&arg->event);
		sccp_free(arg);
	}
	return NULL;
}

/*!
 * \brief Fire an Event
 * \param event SCCP Event
 * \note event will be freed after event is fired
 * 
 * \warning
 *      - sccp_event_listeners->subscriber is not always locked
 */
boolean_t sccp_event_fire(sccp_event_t * event)
{
	boolean_t res = FALSE;
	if (event) {
		sccp_event_vector_t *sync_subscribers_cpy = NULL, *async_subscribers_cpy = NULL;
		size_t subsize = 0, syncsize = 0, asyncsize = 0;
		uint8_t _idx = __search_for_position_in_event_array(event->type);
		
		/* copy both vectors to a local copy while holding the rwlock */
		sccp_event_vector_t *subscribers = &event_subscriptions[_idx].subscribers;
		SCCP_VECTOR_RW_RDLOCK(subscribers);
		if ((subsize = SCCP_VECTOR_SIZE(subscribers))) {
			sync_subscribers_cpy = SCCP_VECTOR_CALLBACK_MULTIPLE(subscribers, SUBSCRIBER_EXEC_CMP, SCCP_EVENT_SYNC);
			async_subscribers_cpy = SCCP_VECTOR_CALLBACK_MULTIPLE(subscribers, SUBSCRIBER_EXEC_CMP, SCCP_EVENT_ASYNC);
			syncsize = sync_subscribers_cpy ? SCCP_VECTOR_SIZE(sync_subscribers_cpy) : 0;
			asyncsize = async_subscribers_cpy ? SCCP_VECTOR_SIZE(async_subscribers_cpy) : 0;
		}
		SCCP_VECTOR_RW_UNLOCK(subscribers);

		// handle synchronous events first (if any)
		if (sync_subscribers_cpy) {
			if (syncsize) {
				res |= __execute_callback_helper(event, sync_subscribers_cpy);
			} else {
				SCCP_VECTOR_PTR_FREE(sync_subscribers_cpy);
			}
		}

		// handle the others asynchonously via threadpool (if any)
		do {
			if (async_subscribers_cpy) {
				if (asyncsize) {
					AsyncArgs_t *arg = NULL;
					if (GLOB(general_threadpool) && sccp_event_running && (arg = sccp_malloc(sizeof *arg))) {
						arg->idx = _idx;
						memcpy(&arg->event, event, sizeof(sccp_event_t));
						arg->async_subscribers = async_subscribers_cpy;
						if (sccp_threadpool_add_work(GLOB(general_threadpool), (void *) sccp_event_processor, (void *) arg)) {
							//sccp_log((DEBUGCAT_EVENT)) (VERBOSE_PREFIX_3 "Work added to threadpool for event: %p, type: %s\n", event, sccp_event_type2str(event->type));
							event = NULL;					// set to NULL, thread will clean event up later.
							res |= true;
							break;						// break out of do/while loop, no further processing needed
						} else {
							pbx_log(LOG_ERROR, "Could not add work to threadpool for event: %s\n", sccp_event_type2str(event->type));
							sccp_free(arg);					// explicit failure release
						}
					}
					res |= __execute_callback_helper(event, async_subscribers_cpy);	// fallback to handling synchronously in case something prevented async
				} else {
					SCCP_VECTOR_PTR_FREE(async_subscribers_cpy);
				}
			}
		} while (0);

		/* cleanup */
		if (event) {
			sccp_event_destroy(event);
		}
	}
	return res;
}

#if CS_TEST_FRAMEWORK
#include "sccp_utils.h"
static uint32_t _sccp_event_TestValue = 25;
static char *_sccp_event_TestStr = "^YTHnjMK<MJHBgF";
static uint32_t _sccp_event_TestEventReceived = 0;

static void sccp_event_testListener(const sccp_event_t * event) {
	pbx_log(LOG_NOTICE, "SCCP: Test Event Listener, received event: %p, with type:%s, payload:[value:%d, str:%s]\n", event, sccp_event_type2str(event->type), event->event.TestEvent.value, event->event.TestEvent.str);
	if (event->event.TestEvent.value == _sccp_event_TestValue && sccp_strequals(event->event.TestEvent.str, _sccp_event_TestStr)) {
		pbx_log(LOG_NOTICE, "SCCP: Test Event Listener, Received Content validated: Returning: %d\n", ++_sccp_event_TestEventReceived);
		return;
	}
	pbx_log(LOG_NOTICE, "SCCP: Test Event Listener, Received Content incorrect\n");
}

AST_TEST_DEFINE(sccp_event_test_subscribe_single)
{
	int rc = AST_TEST_PASS;
	switch(cmd) {
		case TEST_INIT:
			info->name = "subscribe_single";
			info->category = "/channels/chan_sccp/event/";
			info->summary = "chan-sccp-b event subscribe to single event";
			info->description = "chan-sccp-b event subscribe to single event asynchonously and fire test event";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	pbx_test_status_update(test, "subscribe to event:0 fails.\n");
	pbx_test_validate(test, sccp_event_subscribe(0, sccp_event_testListener, TRUE) == FALSE);
	
	pbx_test_status_update(test, "subscribe to SCCP_EVENT_TYPE_SENTINEL fails.\n");
	pbx_test_validate(test, sccp_event_subscribe(SCCP_EVENT_TYPE_SENTINEL, sccp_event_testListener, TRUE) == FALSE);
	
	pbx_test_status_update(test, "subscribe to event:0 fails.\n");
	pbx_test_validate(test, sccp_event_unsubscribe(0, sccp_event_testListener) == FALSE);
	
	pbx_test_status_update(test, "subscribe to SCCP_EVENT_TYPE_SENTINEL fails.\n");
	pbx_test_validate(test, sccp_event_unsubscribe(SCCP_EVENT_TYPE_SENTINEL, sccp_event_testListener) == FALSE);

	pbx_test_status_update(test, "subscribe to SCCP_EVENT_TEST succeeds.\n");
	pbx_test_validate(test, sccp_event_subscribe(SCCP_EVENT_TEST, sccp_event_testListener, TRUE));

	uint32_t EventReceivedBeforeTest = _sccp_event_TestEventReceived;

	pbx_test_status_update(test, "fire SCCP_EVENT_TEST\n");
	sccp_event_t event = {{{0}}};
	event.type = SCCP_EVENT_TEST;
	event.event.TestEvent.value = _sccp_event_TestValue;
	event.event.TestEvent.str = pbx_strdup(_sccp_event_TestStr);
	sccp_event_fire(&event);

	/* wait for async result */
	int loopcount = 0;
	while (_sccp_event_TestEventReceived == EventReceivedBeforeTest && 100 > loopcount++) {
		sccp_safe_sleep(10);
	}
	pbx_test_status_update(test, "before test:%d, received:%d, expected:%d\n", EventReceivedBeforeTest, _sccp_event_TestEventReceived, EventReceivedBeforeTest + 1);
	pbx_test_validate_cleanup(test, _sccp_event_TestEventReceived == EventReceivedBeforeTest + 1, rc, cleanup);

cleanup:
	pbx_test_status_update(test, "unsubscribe from SCCP_EVENT_TEST\n");
	pbx_test_validate(test, sccp_event_unsubscribe(SCCP_EVENT_TEST, sccp_event_testListener));

	return rc;
}

AST_TEST_DEFINE(sccp_event_test_subscribe_multi)
{
	int rc = AST_TEST_PASS;
	switch(cmd) {
		case TEST_INIT:
			info->name = "subscribe_multi";
			info->category = "/channels/chan_sccp/event/";
			info->summary = "chan-sccp-b event subscribe to multiple event";
			info->description = "chan-sccp-b event subscribe to multiple events asynchonously and fire test event";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	int registration = 0, numregistrations = 10;

	pbx_test_status_update(test, "subscribe to SCCP_EVENT_TEST and SCCP_EVENT_LINESTATUS_CHANGED\n");
	for (registration = 0; registration < numregistrations; registration++) {
		pbx_test_status_update(test, "registrations:%d\n", registration);
		pbx_test_validate_cleanup(test, sccp_event_subscribe(SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_TEST, sccp_event_testListener, TRUE), rc, cleanup);
	}
	uint32_t EventReceivedBeforeTest = _sccp_event_TestEventReceived;

	pbx_test_status_update(test, "fire SCCP_EVENT_TEST\n");
	sccp_event_t event = {{{0}}};
	event.type = SCCP_EVENT_TEST;
	event.event.TestEvent.value = _sccp_event_TestValue;
	event.event.TestEvent.str = pbx_strdup(_sccp_event_TestStr);
	sccp_event_fire(&event);

	/* wait for async result */
	int loopcount = 0;
	while (_sccp_event_TestEventReceived == EventReceivedBeforeTest && 100 > loopcount++) {
		sccp_safe_sleep(10);
	}
	pbx_test_status_update(test, "registrations:%d, before test:%d, received:%d, expected:%d\n", registration, EventReceivedBeforeTest, _sccp_event_TestEventReceived, EventReceivedBeforeTest + registration);
	pbx_test_validate_cleanup(test, _sccp_event_TestEventReceived == EventReceivedBeforeTest + registration, rc, cleanup);

cleanup:
	pbx_test_status_update(test, "unsubscribe from SCCP_EVENT_TEST and SCCP_EVENT_LINESTATUS_CHANGED\n");
	while (registration > 0) {
		registration--;
		pbx_test_validate_cleanup(test, sccp_event_unsubscribe(SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_TEST, sccp_event_testListener), rc, cleanup);
	}
	return rc;
}

AST_TEST_DEFINE(sccp_event_test_subscribe_multi_sync)
{
	int rc = AST_TEST_PASS;
	switch(cmd) {
		case TEST_INIT:
			info->name = "subscribe_multi_sync";
			info->category = "/channels/chan_sccp/event/";
			info->summary = "chan-sccp-b event subscribe to multiple event synchonously";
			info->description = "chan-sccp-b event subscribe to multiple events synchonously and fire test event";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	int registration, numregistrations = 10;

	pbx_test_status_update(test, "subscribe to SCCP_EVENT_TEST and SCCP_EVENT_LINESTATUS_CHANGED\n");
	for (registration = 0; registration < numregistrations; registration++) {
		pbx_test_status_update(test, "registrations:%d\n", registration);
		pbx_test_validate_cleanup(test, sccp_event_subscribe(SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_TEST, sccp_event_testListener, FALSE), rc, cleanup);
	}

	uint32_t EventReceivedBeforeTest = _sccp_event_TestEventReceived;

	pbx_test_status_update(test, "fire SCCP_EVENT_TEST\n");
	sccp_event_t event = {{{0}}};
	event.type = SCCP_EVENT_TEST;
	event.event.TestEvent.value = _sccp_event_TestValue;
	event.event.TestEvent.str = pbx_strdup(_sccp_event_TestStr);
	sccp_event_fire(&event);

	pbx_test_status_update(test, "registrations:%d, before test:%d, received:%d, expected:%d\n", registration, EventReceivedBeforeTest, _sccp_event_TestEventReceived, EventReceivedBeforeTest + registration);
	pbx_test_validate_cleanup(test, _sccp_event_TestEventReceived == EventReceivedBeforeTest + registration, rc, cleanup);

cleanup:
	pbx_test_status_update(test, "unsubscribe from SCCP_EVENT_TEST and SCCP_EVENT_LINESTATUS_CHANGED\n");
	while (registration > 0) {
		registration--;
		pbx_test_validate_cleanup(test, sccp_event_unsubscribe(SCCP_EVENT_LINESTATUS_CHANGED | SCCP_EVENT_TEST, sccp_event_testListener), rc, cleanup);
	}
	return rc;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_event_test_subscribe_single);
	AST_TEST_REGISTER(sccp_event_test_subscribe_multi);
	AST_TEST_REGISTER(sccp_event_test_subscribe_multi_sync);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_event_test_subscribe_single);
	AST_TEST_UNREGISTER(sccp_event_test_subscribe_multi);
	AST_TEST_UNREGISTER(sccp_event_test_subscribe_multi_sync);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
