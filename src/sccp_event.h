/*!
 * \file        sccp_event.h
 * \brief       SCCP Event Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-09-02
 *
 * $Date$
 * $Revision$  
 */

#ifndef SCCP_EVENT_H_
#define SCCP_EVENT_H_

//#include <config.h>
//#include "common.h"
#include "chan_sccp.h"

#define NUMBER_OF_EVENT_TYPES 10

#define sccp_event_retain(_x) 		({ast_assert(_x != NULL);sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_event_release(_x) 		({ast_assert(_x != NULL);sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})

/*!
 * \brief SCCP Event Structure
 */
typedef struct sccp_event sccp_event_t;
typedef void (*sccp_event_callback_t) (const sccp_event_t * event);

extern struct sccp_event_subscriptions *sccp_event_listeners;

/*!
 * \brief SCCP Event Structure
 */
struct sccp_event {
	sccp_event_type_t type;											/*!< Event Type */

	/*! 
	 * \brief SCCP Event Data Union
	 */
	union sccp_event_data {
		struct {
			sccp_line_t *line;									/*!< SCCP Line (required) */
		} lineCreated;											/*!< Event Line Created Structure */
		struct {
			sccp_device_t *device;									/*!< SCCP Device (required) */
		} deviceRegistered;										/*!< Event Device Registered Structure */
		struct {
			sccp_linedevices_t *linedevice;								/*!< SCCP device line (required) */
		} deviceAttached;										/*!< Event Device Attached Structure */

		struct {
			sccp_device_t *device;									/*!< SCCP device (required) */
			sccp_feature_type_t featureType;							/*!< what feature is changed (required) */
			sccp_linedevices_t *optional_linedevice;						/*!< SCCP linedevice (optional) */
		} featureChanged;										/*!< Event feature changed Structure */

		struct {
			sccp_line_t *line;									/*!< SCCP line (required) */
			uint8_t state;										/*!< state (required) */
			sccp_device_t *optional_device;								/*!< SCCP device (optional) */
		} lineStatusChanged;										/*!< Event feature changed Structure */

	} event;												/*!< SCCP Event Data Union */
};														/*!< SCCP Event Structure */

typedef struct sccp_event_subscriber sccp_event_subscriber_t;

/*!
 * \brief SCCP Event Subscriber Structure
 */
struct sccp_event_subscriber {
	sccp_event_type_t eventType;
	sccp_event_callback_t callback_function;

	//SCCP_LIST_ENTRY(sccp_event_subscriber_t) list;
};

void sccp_event_unsubscribe(sccp_event_type_t eventType, sccp_event_callback_t cb);
void sccp_event_subscribe(sccp_event_type_t eventType, sccp_event_callback_t cb, boolean_t allowASyncExecution);

void sccp_event_module_start(void);
void sccp_event_module_stop(void);

void sccp_event_fire(const sccp_event_t * event);

#endif														/* SCCP_EVENT_H_ */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
