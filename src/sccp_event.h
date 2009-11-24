/*!
 * \file 	sccp_event.c
 * \brief 	SCCP Event Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \since	2009-09-02
 */

#ifndef SCCP_EVENT_H_
#define SCCP_EVENT_H_

#include "chan_sccp.h"

#define EVENT(x) sccp_event_create(x)

/* structures */

/* event types to notify modular systems */

/*!
 * \brief SCCP Event Type ENUM
 */
typedef enum {
        SCCP_EVENT_LINECREATED 			= 1,
        SCCP_EVENT_LINECHANGED 			= 1 << 1,
        SCCP_EVENT_LINEDELETED 			= 1 << 2,
        SCCP_EVENT_DEVICEATTACHED 		= 1 << 3,
        SCCP_EVENT_DEVICEDETACHED 		= 1 << 4,
        SCCP_EVENT_DEVICEREGISTERED 		= 1 << 5,
        SCCP_EVENT_DEVICEUNREGISTERED  		= 1 << 6
} sccp_event_type_t;									/*!< SCCP Event Type ENUM */

typedef struct sccp_event 			sccp_event_t;							
typedef void* (*sccp_event_callback_t)(sccp_event_t *event);

/*!
 * \brief SCCP Event Structure
 */
struct sccp_event {
        sccp_event_type_t			type;					/*!< SCCP Event Type */
        union sccp_event_data {

                struct {
                        sccp_line_t 		* line;					/*!< SCCP Line */
                } lineCreated;								/*!< Created Line Structure */

                struct {
                        sccp_device_t 		* device;				/*!< SCCP Device */
                } deviceRegistered;							/*!< Registered Device Structure */

                struct {
                        sccp_line_t 		* line;					/*!< SCCP Line */
                        sccp_device_t		* device;				/*!< SCCP Device */
                } deviceAttached;							/*!< Attached Device Structure */
        } event;									/*!< SCCP Event Data Structure */
};											/*!< SCCP Event Structure */

typedef struct sccp_event_subscriber sccp_event_subscriber_t;

struct sccp_event_subscriber {
        sccp_event_type_t 			eventType;				/*!< SCCP Event Type */
        sccp_event_callback_t 			callback_function;			/*!< SCCP Event Call Back */
        SCCP_LIST_ENTRY(sccp_event_subscriber_t) list;					/*!< Event Subscriber Linked List Entry */
};											/*!< SCCP Event Subscriber Structure */

struct sccp_event_subscriptions {
        SCCP_LIST_HEAD(,sccp_event_subscriber_t) subscriber;				/*!< Event Subscriber Linked List Entry*/
};											/*!< SCCP Event Subscribtions Structure */

struct sccp_event_subscriptions 		* sccp_event_listeners;


/* functions */

#define 	sccp_subscribe_event(event, callback)		\
			sccp_event_subscribe(event, callback,	__FILE__, __FUNCTION__, __LINE__);

void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb,
                          const char *file, const char *caller, int line);

void sccp_event_fire(sccp_event_t *event);
sccp_event_t *sccp_event_create(sccp_event_type_t eventType);

#endif /* SCCP_EVENT_H_ */
