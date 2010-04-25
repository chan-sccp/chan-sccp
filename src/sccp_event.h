/*!
 * \file 	sccp_event.h
 * \brief 	SCCP Event Header
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License Version 3. 
 *		See the LICENSE file at the top of the source tree.
 * \since	2009-09-02
 * \date        $Date$
 * \version     $Revision$  
 */

#ifndef SCCP_EVENT_H_
#define SCCP_EVENT_H_

#include "config.h"
#include "chan_sccp.h"

/* structures */
/* event types to notify modular systems */

/*!
 * \brief SCCP Event Type ENUM
 */
typedef enum {
	SCCP_EVENT_LINECREATED = 1,
	SCCP_EVENT_LINECHANGED = 1 << 1,
	SCCP_EVENT_LINEDELETED = 1 << 2,
	SCCP_EVENT_DEVICEATTACHED = 1 << 3,
	SCCP_EVENT_DEVICEDETACHED = 1 << 4,
	SCCP_EVENT_DEVICEREGISTERED = 1 << 5,
	SCCP_EVENT_DEVICEUNREGISTERED  = 1 << 6,
	SCCP_EVENT_FEATURECHANGED  = 1 << 7
} sccp_event_type_t;									/*!< SCCP Event Type ENUM */

/*!
 * \brief SCCP Event Structure
 */
typedef struct sccp_event sccp_event_t;
typedef void (*sccp_event_callback_t)(const sccp_event_t **event);


extern struct sccp_event_subscriptions *sccp_event_listeners;

/*!
 * \brief SCCP Event Structure
 */
struct sccp_event {
	sccp_event_type_t	type;							/*!< Event Type */
	
	/*! 
	 * \brief SCCP Event Data Union
	 */
	union sccp_event_data{
		struct{
			sccp_line_t 	*line;						/*!< SCCP Line */
		} lineCreated;								/*!< Event Line Created Structure */
		struct{
			sccp_device_t 	*device;					/*!< SCCP Device */
		} deviceRegistered;							/*!< Event Device Registered Structure */
		struct{
			sccp_line_t 	*line;						/*!< SCCP Line */
			sccp_device_t	*device;					/*!< SCCP Device */
		} deviceAttached;							/*!< Event Device Attached Structure */
		
		struct{
			sccp_device_t 	*device;					/*!< device how initialized the change */
			sccp_feature_type_t featureType;				/*!< what feature is changed */
		} featureChanged;							/*!< Event feature changed Structure */
	} event;									/*!< SCCP Event Data Union */
};											/*!< SCCP Event Structure */

typedef struct sccp_event_subscriber sccp_event_subscriber_t;
/*!
 * \brief SCCP Event Subscriber Structure
 */
struct sccp_event_subscriber{
	sccp_event_type_t eventType;
	sccp_event_callback_t callback_function;

	SCCP_LIST_ENTRY(sccp_event_subscriber_t) list;
};

/*!
 * \brief SCCP Event Subscribtions Structure
 */
struct sccp_event_subscriptions{
	SCCP_LIST_HEAD(,sccp_event_subscriber_t)  subscriber;
};



void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb);

void sccp_event_fire(const sccp_event_t **event);

#endif /* SCCP_EVENT_H_ */
