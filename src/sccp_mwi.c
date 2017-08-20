/*!
 * \file        sccp_mwi.c
 * \brief       SCCP Message Waiting Indicator Class
 * \author      Marcello Ceschia <marcello.ceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_mwi.h"
#include "sccp_atomic.h"
#include "sccp_channel.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_labels.h"

SCCP_FILE_VERSION(__FILE__, "");

#ifdef HAVE_PBX_APP_H				// ast_mwi_state_type
#  include <asterisk/app.h>
#endif
#if defined(CS_AST_HAS_EVENT) && defined(HAVE_PBX_EVENT_H) 	// ast_event_subscribe
#  include <asterisk/event.h>
#endif

#ifndef CS_AST_HAS_EVENT
#define SCCP_MWI_CHECK_INTERVAL 30
#endif

/*!
 * \brief SCCP Mailbox Line Type Definition
 *
 * holding line information for mailbox subscription
 *
 */
typedef struct sccp_mailboxLine sccp_mailboxLine_t;

/*!
 * \brief SCCP Mailbox Line Type Structure
 *
 * holding line information for mailbox subscription
 *
 */
struct sccp_mailboxLine {
	sccp_line_t *line;
	SCCP_LIST_ENTRY (sccp_mailboxLine_t) list;
};

/*!
 * \brief SCCP Mailbox Subscriber List Type Definition
 */
typedef struct sccp_mailbox_subscriber_list sccp_mailbox_subscriber_list_t;

/*!
 * \brief SCCP Mailbox Subscriber List Structure
 *
 * we hold a mailbox event subscription in sccp_mailbox_subscription_t.
 * Each line that holds a subscription for this mailbox is listed in
 */
struct sccp_mailbox_subscriber_list {
	char mailbox[60];
	char context[60];

	SCCP_LIST_HEAD (, sccp_mailboxLine_t) sccp_mailboxLine;
	SCCP_LIST_ENTRY (sccp_mailbox_subscriber_list_t) list;

	/*!
	 * \brief Current Voicemail Statistic Structure
	 */
	struct {
		int newmsgs;											/*!< New Messages */
		int oldmsgs;											/*!< Old Messages */
	} currentVoicemailStatistic;								/*!< Current Voicemail Statistic Structure */

	/*!
	 * \brief Previous Voicemail Statistic Structure
	 */
	struct {
		int newmsgs;											/*!< New Messages */
		int oldmsgs;											/*!< Old Messages */
	} previousVoicemailStatistic;								/*!< Previous Voicemail Statistic Structure */

#if defined ( CS_AST_HAS_EVENT ) || (defined( CS_AST_HAS_STASIS ))
	/*!
	 * \brief Asterisk Event Subscribers Structure
	 */
	struct pbx_event_sub *event_sub;
#else
	int schedUpdate;
#endif
};																/*!< SCCP Mailbox Subscriber List Structure */

void sccp_mwi_setMWILineStatus(sccp_linedevices_t * lineDevice);
void sccp_mwi_destroySubscription(sccp_mailbox_subscriber_list_t *subscription);
void sccp_mwi_linecreatedEvent(const sccp_event_t * event);
void sccp_mwi_deviceAttachedEvent(const sccp_event_t * event);
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t * line);
void sccp_mwi_lineStatusChangedEvent(const sccp_event_t * event);

static SCCP_LIST_HEAD (, sccp_mailbox_subscriber_list_t) sccp_mailbox_subscriptions;

/*!
 * start mwi module.
 */
void sccp_mwi_module_start(void)
{
	SCCP_LIST_HEAD_INIT(&sccp_mailbox_subscriptions);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Starting MWI system\n");

	sccp_event_subscribe(SCCP_EVENT_LINE_CREATED, sccp_mwi_linecreatedEvent, TRUE);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_ATTACHED, sccp_mwi_deviceAttachedEvent, TRUE);
	sccp_event_subscribe(SCCP_EVENT_LINESTATUS_CHANGED, sccp_mwi_lineStatusChangedEvent, FALSE);
}

/*!
 * \brief Stop MWI Monitor
 */
void sccp_mwi_module_stop(void)
{
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Stopping MWI system\n");

	sccp_event_unsubscribe(SCCP_EVENT_LINE_CREATED, sccp_mwi_linecreatedEvent);
	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_ATTACHED, sccp_mwi_deviceAttachedEvent);
	sccp_event_unsubscribe(SCCP_EVENT_LINESTATUS_CHANGED, sccp_mwi_lineStatusChangedEvent);

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	while ((subscription = SCCP_LIST_REMOVE_HEAD(&sccp_mailbox_subscriptions, list))) {
		sccp_mwi_destroySubscription(subscription);
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_HEAD_DESTROY(&sccp_mailbox_subscriptions);
}

/*!
 * \brief Generic update mwi count
 * \param subscription Pointer to a mailbox subscription
 */
static void sccp_mwi_updatecount(sccp_mailbox_subscriber_list_t * subscription)
{
	sccp_mailboxLine_t *mailboxLine = NULL;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "(sccp_mwi_updatecount)\n");
	SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(mailboxLine->line));

		if (line) {
			sccp_linedevices_t *lineDevice = NULL;

			/* update statistics for line  */
			line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

			line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
			/* done */
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s:(sccp_mwi_updatecount) newmsgs:%d, oldmsgs:%d\n", line->name, line->voicemailStatistic.newmsgs, line->voicemailStatistic.oldmsgs);

			/* notify each device on line */
			SCCP_LIST_LOCK(&line->devices);
			SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
				if (lineDevice && lineDevice->device) {
					sccp_mwi_setMWILineStatus(lineDevice);
				} else {
					pbx_log(LOG_ERROR, "error: null line device.\n");
				}
			}
			SCCP_LIST_UNLOCK(&line->devices);
		}
	}
	SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
}

#if defined(CS_AST_HAS_EVENT)
/*!
 * \brief Receive MWI Event from Asterisk
 * \param event Asterisk Event
 * \param data Asterisk Data
 */
void sccp_mwi_event(const struct ast_event *event, void *data)
{
	sccp_mailbox_subscriber_list_t *subscription = data;

	if (!subscription || !event || !GLOB(module_running)) {
		pbx_log(LOG_ERROR, "SCCP: MWI Event received but not all requirements are fullfilled (%p, %p, %d)\n", subscription, event, GLOB(module_running));
		return;
	}
	int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: Received PBX mwi event (%s) for %s@%s, newmsgs:%d, oldmsgs:%d\n", ast_event_get_type_name(event), subscription->mailbox, subscription->context, newmsgs, oldmsgs);

	/* for calculation store previous voicemail counts */
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	if (newmsgs != -1 && oldmsgs != -1) {
		subscription->currentVoicemailStatistic.newmsgs = newmsgs;
		subscription->currentVoicemailStatistic.oldmsgs = oldmsgs;
		if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs && subscription->currentVoicemailStatistic.newmsgs != -1) {
			sccp_mwi_updatecount(subscription);
		}
	}
}

#elif defined(CS_AST_HAS_STASIS)
/*!
 * \brief Receive MWI Event from Asterisk
 * \param userdata Asterisk Event UserData
 * \param sub Asterisk Stasis Subscription
 * \param msg Asterisk Stasis Message
 */
void sccp_mwi_event(void *userdata, struct stasis_subscription *sub, struct stasis_message *msg)
{
	sccp_mailbox_subscriber_list_t *subscription = userdata;
	struct stasis_subscription_change *change = stasis_message_data(msg);

	if (!subscription || !GLOB(module_running)) {
		return;
	}

	if (msg && ast_mwi_state_type() == stasis_message_type(msg) && change->topic != ast_mwi_topic_all()) {
		struct ast_mwi_state *mwi_state = stasis_message_data(msg);
		
		int newmsgs = mwi_state->new_msgs, oldmsgs = mwi_state->old_msgs;

		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: Received PBX mwi event for %s@%s, newmsgs:%d, oldmsgs:%d\n", subscription->mailbox, subscription->context, newmsgs, oldmsgs);

		subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
		subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

		if (newmsgs != -1 && oldmsgs != -1) {
			subscription->currentVoicemailStatistic.newmsgs = newmsgs;
			subscription->currentVoicemailStatistic.oldmsgs = oldmsgs;
			if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
				sccp_mwi_updatecount(subscription);
			}
		}
	}
}

#else
/*!
 * \brief MWI Progress
 * \param ptr Pointer to Mailbox Subscriber list Entry
 * \note only used for asterisk version without mwi event (scheduled check)
 *
 * \warning
 *  - line->devices is not always locked
 *
 * \called_from_asterisk
 */
int sccp_mwi_checksubscription(const void *ptr)
{
	sccp_mailbox_subscriber_list_t *subscription = (sccp_mailbox_subscriber_list_t *) ptr;

	if (!subscription || !GLOB(module_running)) {
		return -1;
	}
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	char buffer[512];
	int newmsgs = 0, oldmsgs = 0;
	int interval = SCCP_MWI_CHECK_INTERVAL;

	snprintf(buffer, 512, "%s@%s", subscription->mailbox, subscription->context);
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_4 "SCCP: checking mailbox: %s\n", buffer);
	if (pbx_app_inboxcount(buffer, &newmsgs, &oldmsgs) == 0) {
		if (newmsgs != -1 && oldmsgs != -1) {
			subscription->currentVoicemailStatistic.newmsgs = newmsgs;
			subscription->currentVoicemailStatistic.oldmsgs = oldmsgs;

			/* update devices if something changed */
			if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
				sccp_mwi_updatecount(subscription);
			}
		}
	} else {
		interval = SCCP_MWI_CHECK_INTERVAL * 10;			/* if we failed, slow down polling */
	}	

	/* reschedule my self */
	if ((subscription->schedUpdate = iPbx.sched_add(interval * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
		pbx_log(LOG_ERROR, "Error creating mailbox subscription.\n");
	}

	return 0;
}
#endif

/*!
 * \brief Free Mailbox Subscription
 */
void sccp_mwi_destroySubscription(sccp_mailbox_subscriber_list_t *subscription)
{
	pbx_assert(subscription != NULL);
	sccp_mailboxLine_t *mailboxLine = NULL;
	
	SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
	while ((mailboxLine = SCCP_LIST_REMOVE_HEAD(&subscription->sccp_mailboxLine, list))) {
		sccp_free(mailboxLine);
	}
	SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	SCCP_LIST_HEAD_DESTROY(&subscription->sccp_mailboxLine);
			
#if defined(CS_AST_HAS_EVENT)
	if (subscription->event_sub) {
		pbx_event_unsubscribe(subscription->event_sub);
	}
#elif defined(CS_AST_HAS_STASIS)
	if (subscription->event_sub) {
		stasis_unsubscribe_and_join(subscription->event_sub);
	}
#else
	if (subscription->schedUpdate > -1) {
		subscription->schedUpdate = SCCP_SCHED_DEL(subscription->schedUpdate);
	}
#endif
	sccp_free(subscription);
}

/*!
 * \brief Remove Mailbox Subscription
 * \param mailbox SCCP Mailbox
 * \todo Implement sccp_mwi_unsubscribeMailbox ( \todo Marcello)
 */
void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t * mailbox)
{
	if (!mailbox) {
		pbx_log(LOG_ERROR, "(sccp_mwi_unsubscribeMailbox) mailbox not provided\n");
		return;
	}

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: unsubscribe mailbox: %s@%s\n", mailbox->mailbox, mailbox->context);
	sccp_mailbox_subscriber_list_t *subscription = NULL;

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&sccp_mailbox_subscriptions, subscription, list) {
		if (sccp_strequals(mailbox->mailbox, subscription->mailbox) && sccp_strequals(mailbox->context, subscription->context)) {
			SCCP_LIST_REMOVE_CURRENT(list);
			sccp_mwi_destroySubscription(subscription);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);
}

/*!
 * \brief Device Attached Event
 * \param event SCCP Event
 */
void sccp_mwi_deviceAttachedEvent(const sccp_event_t * event)
{
	if (!event || !event->event.deviceAttached.linedevice) {
		pbx_log(LOG_ERROR, "(deviceAttachedEvent) event or linedevice not provided\n");
		return;
	}

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: (mwi_deviceAttachedEvent) Get deviceAttachedEvent\n");

	sccp_linedevices_t *linedevice = event->event.deviceAttached.linedevice;
	sccp_line_t *line = linedevice->line;
	sccp_device_t *device = linedevice->device;

	if (line && device) {
		device->voicemailStatistic.oldmsgs += line->voicemailStatistic.oldmsgs;
		device->voicemailStatistic.newmsgs += line->voicemailStatistic.newmsgs;
		
		sccp_mwi_setMWILineStatus(linedevice);								/* set mwi-line-status */
	} else {
		pbx_log(LOG_ERROR, "get deviceAttachedEvent where one parameter is missing. device: %s, line: %s\n", DEV_ID_LOG(device), (line) ? line->name : "null");
	}
}

/*!
 * \brief Line Status Changed Event
 * \param event SCCP Event
 */
void sccp_mwi_lineStatusChangedEvent(const sccp_event_t * event)
{
	if (!event || !event->event.lineStatusChanged.optional_device) {
		pbx_log(LOG_ERROR, "(lineStatusChangedEvent) event or device not provided\n");
		return;
	}

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: (mwi_lineStatusChangedEvent) Get lineStatusChangedEvent\n");
	/* these are the only events we are interested in */
	if (	event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_DOWN || \
		event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_ONHOOK || \
		event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_RINGING || \
		event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_OFFHOOK 
	) {
		if (event->event.lineStatusChanged.line && event->event.lineStatusChanged.optional_device) {
			AUTO_RELEASE(sccp_linedevices_t, linedevice, sccp_linedevice_find(event->event.lineStatusChanged.optional_device, event->event.lineStatusChanged.line));
			if (linedevice) {
				sccp_mwi_setMWILineStatus(linedevice);
			}
		}
		//sccp_mwi_check(event->event.lineStatusChanged.optional_device);
	}
}

/*!
 * \brief Line Created Event
 * \param event SCCP Event
 *
 * \warning
 *  - line->mailboxes is not always locked
 */
void sccp_mwi_linecreatedEvent(const sccp_event_t * event)
{
	if (!event || !event->event.lineCreated.line) {
		pbx_log(LOG_ERROR, "(linecreatedEvent) event or line not provided\n");
		return;
	}

	sccp_mailbox_t *mailbox = NULL;
	sccp_line_t *line = event->event.lineCreated.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: (mwi_linecreatedEvent) Get linecreatedEvent\n");

	if (line && (&line->mailboxes) != NULL) {
		SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
			sccp_mwi_addMailboxSubscription(mailbox->mailbox, mailbox->context, line);
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (sccp_mwi_linecreatedEvent) subscribed mailbox: %s@%s\n", line->name, mailbox->mailbox, mailbox->context);
		}
	}
}

/*!
 * \brief Add Mailbox Subscription
 * \param mailbox Mailbox as char
 * \param context Mailbox Context
 * \param line SCCP Line
 *
 * \warning
 *  - subscription->sccp_mailboxLine is not always locked
 */
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t * line)
{
	if (sccp_strlen_zero(mailbox) || sccp_strlen_zero(context) || !line) {
		pbx_log(LOG_ERROR, "%s: (addMailboxSubscription) Not all parameter contain valid pointers, mailbox: %p, context: %p\n", line ? line->name : "SCCP", mailbox, context);
		return;
	}
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_mailbox_subscriptions, subscription, list) {
		if (sccp_strequals(mailbox, subscription->mailbox) && sccp_strequals(context, subscription->context)) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);

	if (!subscription) {
		subscription = sccp_calloc(sizeof *subscription, 1);
		if (!subscription) {
			pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, line->name);
			return;
		}
		SCCP_LIST_HEAD_INIT(&subscription->sccp_mailboxLine);

		sccp_copy_string(subscription->mailbox, mailbox, sizeof(subscription->mailbox));
		sccp_copy_string(subscription->context, context, sizeof(subscription->context));
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) creating subscription for: %s@%s\n", subscription->mailbox, subscription->context);

		SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_mailbox_subscriptions, subscription, list);
		SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);

		/* get initial value */

#ifdef CS_AST_HAS_EVENT
		struct ast_event *event = ast_event_get_cached(AST_EVENT_MWI,
							       AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox,
							       AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context,
							       AST_EVENT_IE_END);

		if (event) {
			int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
			int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
			if (newmsgs != -1 && oldmsgs != -1) {
				subscription->currentVoicemailStatistic.newmsgs = newmsgs;
				subscription->currentVoicemailStatistic.oldmsgs = oldmsgs;
			}
			ast_event_destroy(event);
		} else
#endif
		{												/* Fall back on checking the mailbox directly */
			char buffer[512];
			int newmsgs = 0, oldmsgs = 0;

			snprintf(buffer, 512, "%s@%s", subscription->mailbox, subscription->context);
			if (pbx_app_inboxcount(buffer, &newmsgs, &oldmsgs) == 0) {
				if (newmsgs != -1 && oldmsgs != -1) {
					subscription->currentVoicemailStatistic.newmsgs = newmsgs;
					subscription->currentVoicemailStatistic.oldmsgs = oldmsgs;
				}
			}
		}

		/* register asterisk event */
#if defined( CS_AST_HAS_EVENT)
#  if ASTERISK_VERSION_NUMBER >= 10800
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, "mailbox subscription", subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_NEWMSGS, AST_EVENT_IE_PLTYPE_EXISTS, AST_EVENT_IE_END);
#  else
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_END);
#  endif
		if (!subscription->event_sub) {
			pbx_log(LOG_ERROR, "SCCP: PBX MWI event could not be subscribed to for mailbox %s@%s\n", subscription->mailbox, subscription->context);
		}
#elif defined(CS_AST_HAS_STASIS)
//		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) Adding STASIS Subscription for mailbox %s\n", subscription->mailbox);
		char mailbox_context[512];

		snprintf(mailbox_context, SCCP_MAX_EXTENSION + SCCP_MAX_CONTEXT + 2, "%s@%s", subscription->mailbox, subscription->context);
		struct stasis_topic *mailbox_specific_topic = ast_mwi_topic(mailbox_context);
		if (mailbox_specific_topic) {
			subscription->event_sub = stasis_subscribe(mailbox_specific_topic, sccp_mwi_event, subscription);
		}
#else
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) Falling back to polling mailbox status\n");
		if ((subscription->schedUpdate = iPbx.sched_add(SCCP_MWI_CHECK_INTERVAL * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
			pbx_log(LOG_ERROR, "SCCP: (mwi_addMailboxSubscription) Error creating mailbox subscription.\n");
		}
#endif
		/* end register asterisk event */
	}

	/* we already have this subscription */
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		if (line == mailboxLine->line) {
			break;
		}
	}

	if (!mailboxLine) {
		mailboxLine = sccp_calloc(sizeof *mailboxLine, 1);
		if (!mailboxLine) {
			pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, line->name);
			return;
		}

		mailboxLine->line = line;

		line->voicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
		line->voicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_INSERT_HEAD(&subscription->sccp_mailboxLine, mailboxLine, list);
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	}
}

/*!
 * \brief Set MWI Line Status
 * \param lineDevice SCCP LineDevice
 */
void sccp_mwi_setMWILineStatus(sccp_linedevices_t * lineDevice)
{
	pbx_assert(lineDevice != NULL && lineDevice->device != NULL);
	
	sccp_msg_t *msg = NULL;
	sccp_line_t *l = lineDevice->line;
	sccp_device_t *d = lineDevice->device;
	uint32_t instance = 0;
	uint32_t status = 0, mask = 0, state = 0;

	/* when l is defined we are switching on/off the button icon, otherwise the main mwi light */
	if (l) {
		instance = lineDevice->lineInstance;
		state = l->voicemailStatistic.newmsgs ? 1 : 0;
	}

	mask = 1 << instance;			/* mask the bit field for this line instance */
	status = state << instance;

	/* check if we need to update line status */
	char binstr[41] = "";
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) instance: %d, mwilight:%d, mask:%s (%d)\n", DEV_ID_LOG(d), instance, d->mwilight, sccp_dec2binstr(binstr, 32, mask), mask);
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) state: %d. status:%s (%d) \n", DEV_ID_LOG(d), state, sccp_dec2binstr(binstr, 32, status), status);
	if ( (d->mwilight & mask) != status) {
		if (state) {			/* activate mwi line icon */
			d->mwilight |= mask;
		} else {			/* deactivate mwi line icon */
			d->mwilight &= ~mask;
		}
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) new mwilight:%s value: %d\n", DEV_ID_LOG(lineDevice->device), sccp_dec2binstr(binstr, 32, d->mwilight), d->mwilight);
		REQ(msg, SetLampMessage);
		msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		msg->data.SetLampMessage.lel_stimulusInstance = htolel(instance);
		msg->data.SetLampMessage.lel_lampMode = state ? htolel(SKINNY_LAMP_ON) : htolel(SKINNY_LAMP_OFF);
		sccp_dev_send(d, msg);
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) Turn %s the MWI on line %s (%d)\n", DEV_ID_LOG(d), state ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	} else {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) Device already knows this state %s on line %s (%d). skipping update\n", DEV_ID_LOG(d), status ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	}
	if (sccp_device_getRegistrationState(d) == SKINNY_DEVICE_RS_OK) {
		sccp_mwi_check(d); /* we need to check mwi status again, to enable/disable device mwi light */
	}
}

/*!
 * \brief Check MWI Status for Device
 * \param d SCCP Device
 * \note called by lineStatusChange
 */
void sccp_mwi_check(sccp_device_t * d)
{
	uint32_t oldmsgs = 0, newmsgs = 0;
	boolean_t suppress_lamp = FALSE;

	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));
	if (!device) {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_check) called with NULL device!\n");
		return;
	}

	uint32_t instance = SCCP_FIRST_LINEINSTANCE;
	for (instance = SCCP_FIRST_LINEINSTANCE; instance < device->lineButtons.size; instance++) {
		if (device->lineButtons.instance[instance] && device->lineButtons.instance[instance]->line) {
			sccp_line_t *line =  device->lineButtons.instance[instance]->line;
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: Checking Line Instance: %d = %s, mwioncall: %s\n", DEV_ID_LOG(device), instance, line->name, device->mwioncall ? "On" : "Off");
			if (!device->mwioncall) {									// check if mwi suppression is required
				sccp_channel_t *c = NULL;
				SCCP_LIST_LOCK(&line->channels);
				SCCP_LIST_TRAVERSE(&line->channels, c, list) {
					AUTO_RELEASE(sccp_device_t, tmpDevice , sccp_channel_getDevice(c));
					if (tmpDevice && tmpDevice == device) {						// We have a channel belonging to our device (no remote shared line channel)
						if ((c->state != SCCP_CHANNELSTATE_ONHOOK && c->state != SCCP_CHANNELSTATE_DOWN) || c->state == SCCP_CHANNELSTATE_RINGING) {
							sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: we have an active channel, suppress mwi light\n", DEV_ID_LOG(device));
							suppress_lamp = TRUE;
						}
					}
				}
				SCCP_LIST_UNLOCK(&line->channels);
			}
			/* pre-collect number of voicemails on device to be set later */
			oldmsgs += line->voicemailStatistic.oldmsgs;
			newmsgs += line->voicemailStatistic.newmsgs;
			
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) line %s voicemail count %d new/%d old (instance: %d)\n", DEV_ID_LOG(device), line->name, line->voicemailStatistic.newmsgs, line->voicemailStatistic.oldmsgs, instance);
		}
	}
	/* check current device mwi light status*/
	uint32_t devicemask = (1 << SCCP_DEVICE_MWILIGHT);
	uint32_t devicestate = device->mwilight;
	uint32_t devicenewstate = (((newmsgs && !suppress_lamp) ? 1 : 0) << SCCP_DEVICE_MWILIGHT);

	//char binstr1[41] = "", binstr2[41] = "";
	//sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) devicestate:%s, mask:%s\n", DEV_ID_LOG(device), sccp_dec2binstr(binstr1, 32, devicestate), sccp_dec2binstr(binstr2, 32, devicemask));
	//sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) devicenewstate:%s\n", DEV_ID_LOG(device), sccp_dec2binstr(binstr1, 32, devicenewstate));

	//if ((device->mwilight & newstate) != 0) {										/* needs update */
	if ((devicestate & devicemask) != devicenewstate) {
		//boolean_t devicelamp_active = FALSE;
		if (device->mwilight & (1 << SCCP_DEVICE_MWILIGHT)) {
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) De-activate\n", DEV_ID_LOG(device));
			device->mwilight &= ~(1 << SCCP_DEVICE_MWILIGHT);								/* deactivate */
		} else {
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Activate\n", DEV_ID_LOG(device));
			device->mwilight |= (1 << SCCP_DEVICE_MWILIGHT);								/* activate */
			//devicelamp_active = TRUE;
		}
		//sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) new device->mwilight:%s\n", DEV_ID_LOG(device), sccp_dec2binstr(binstr1, 32, device->mwilight));
		sccp_msg_t *msg = NULL;

		REQ(msg, SetLampMessage);
		msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		msg->data.SetLampMessage.lel_stimulusInstance = 0;
		msg->data.SetLampMessage.lel_lampMode = (device->mwilight & (1 << SCCP_DEVICE_MWILIGHT)) ? htolel(device->mwilamp) : htolel(SKINNY_LAMP_OFF);
		//msg->data.SetLampMessage.lel_lampMode = devicelamp_active ? htolel(device->mwilamp) : htolel(SKINNY_LAMP_OFF);
		sccp_dev_send(device, msg);
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Turn %s the MWI light (newmsgs: %d->%d)\n", DEV_ID_LOG(device), (device->mwilight & (1 << SCCP_DEVICE_MWILIGHT)) ? "ON" : "OFF", newmsgs,  device->voicemailStatistic.newmsgs);
	}
	/* we should check the display only once, maybe we need a priority stack -MC */
	if (newmsgs > 0) {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Set Have Voicemail on Display\n", DEV_ID_LOG(device));
		char buffer[StationMaxDisplayTextSize];
		snprintf(buffer, StationMaxDisplayTextSize, "%s: (%u/%u)", SKINNY_DISP_YOU_HAVE_VOICEMAIL, newmsgs, oldmsgs);
		sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_VOICEMAIL, buffer);
	} else {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) Remove Have Voicemail from Display\n", DEV_ID_LOG(device));
		sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_VOICEMAIL);
	}
	device->voicemailStatistic.oldmsgs = oldmsgs;
	device->voicemailStatistic.newmsgs = newmsgs;
}

/*!
 * \brief Show MWI Subscriptions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 *
 * \called_from_asterisk
 */
#include <asterisk/cli.h>
int sccp_show_mwi_subscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *line = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;
	char linebuf[31] = "";
	int local_line_total = 0;

#define CLI_AMI_TABLE_NAME MWISubscriptions
#define CLI_AMI_TABLE_PER_ENTRY_NAME MailboxSubscriber
#define CLI_AMI_TABLE_LIST_ITER_HEAD &sccp_mailbox_subscriptions
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_mailbox_subscriber_list_t
#define CLI_AMI_TABLE_LIST_ITER_VAR subscription
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION 															\
 		SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {								\
 			line = mailboxLine->line;													\
 			snprintf(linebuf,sizeof(linebuf),"%s",line->name);										\
 		}

#if defined ( CS_AST_HAS_EVENT ) || (defined( CS_AST_HAS_STASIS ))
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	subscription->mailbox)						\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	linebuf)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	subscription->context)						\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	subscription->currentVoicemailStatistic.newmsgs)		\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	subscription->currentVoicemailStatistic.oldmsgs)		\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->event_sub ? "YES" : "NO")
#include "sccp_cli_table.h"
#else
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	subscription->mailbox)						\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	linebuf)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	subscription->context)						\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	subscription->currentVoicemailStatistic.newmsgs)		\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	subscription->currentVoicemailStatistic.oldmsgs)
#include "sccp_cli_table.h"
#endif

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
