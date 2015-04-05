/*!
 * \file        sccp_mwi.c
 * \brief       SCCP Message Waiting Indicator Class
 * \author      Marcello Ceschia <marcello.ceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_mwi.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_line.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");
#ifndef CS_AST_HAS_EVENT
#define SCCP_MWI_CHECK_INTERVAL 30
#endif
void sccp_mwi_checkLine(sccp_line_t * line);
void sccp_mwi_setMWILineStatus(sccp_linedevices_t * lineDevice);
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
	/* */
	SCCP_LIST_HEAD_INIT(&sccp_mailbox_subscriptions);

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
	sccp_mailboxLine_t *sccp_mailboxLine = NULL;

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	while ((subscription = SCCP_LIST_REMOVE_HEAD(&sccp_mailbox_subscriptions, list))) {

		/* removing lines */
		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		while ((sccp_mailboxLine = SCCP_LIST_REMOVE_HEAD(&subscription->sccp_mailboxLine, list))) {
			sccp_free(sccp_mailboxLine);
		}
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_HEAD_DESTROY(&subscription->sccp_mailboxLine);

#if defined(CS_AST_HAS_EVENT)
		/* unsubscribe asterisk event */
		if (subscription->event_sub) {
			pbx_event_unsubscribe(subscription->event_sub);
		}
#elif defined(CS_AST_HAS_STASIS) && defined(CS_EXPERIMENTAL)
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (sccp_mwi_module_stop) STASIS Unsubscribe\n");
		if (subscription->event_sub) {
			stasis_unsubscribe(subscription->event_sub);
		}
#else
		subscription->schedUpdate = SCCP_SCHED_DEL(subscription->schedUpdate);
#endif

		sccp_free(subscription);
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_HEAD_DESTROY(&sccp_mailbox_subscriptions);

	sccp_event_unsubscribe(SCCP_EVENT_LINE_CREATED, sccp_mwi_linecreatedEvent);
	sccp_event_unsubscribe(SCCP_EVENT_DEVICE_ATTACHED, sccp_mwi_deviceAttachedEvent);
	sccp_event_unsubscribe(SCCP_EVENT_LINESTATUS_CHANGED, sccp_mwi_lineStatusChangedEvent);
}

/*!
 * \brief Generic update mwi count
 * \param subscription Pointer to a mailbox subscription
 */
static void sccp_mwi_updatecount(sccp_mailbox_subscriber_list_t * subscription)
{
	sccp_mailboxLine_t *mailboxLine = NULL;

	SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		AUTO_RELEASE sccp_line_t *line = sccp_line_retain(mailboxLine->line);

		if (line) {
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_4 "line: %s\n", line->name);
			sccp_linedevices_t *lineDevice = NULL;

			/* update statistics for line  */
			line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

			line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
			/* done */

			/* notify each device on line */
			SCCP_LIST_LOCK(&line->devices);
			SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
				if (lineDevice && lineDevice->device) {
					sccp_mwi_setMWILineStatus(lineDevice);
				} else {
					sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_4 "error: null line device.\n");
				}
			}
			SCCP_LIST_UNLOCK(&line->devices);
		}
	}
	SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
}

#if defined(CS_AST_HAS_STASIS) && defined(CS_EXPERIMENTAL)
/*!
 * \brief Receive MWI Event from Asterisk
 * \param event Asterisk Event
 * \param data Asterisk Data
 */

void sccp_mwi_event(void *userdata, struct stasis_subscription *sub, struct stasis_message *msg)
{
	sccp_mailbox_subscriber_list_t *subscription = userdata;

	sccp_log(DEBUGCAT_MWI)(VERBOSE_PREFIX_1 "Got mwi-event\n");
	if (!subscription || !sub) {
		return;
	}

	if (msg && ast_mwi_state_type() == stasis_message_type(msg)) {
		struct ast_mwi_state *mwi_state = stasis_message_data(msg);

		subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
		subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

		subscription->currentVoicemailStatistic.newmsgs = mwi_state->new_msgs;
		subscription->currentVoicemailStatistic.oldmsgs = mwi_state->old_msgs;

		if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
			sccp_mwi_updatecount(subscription);
		}

	} else {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "Received STASIS Message that did not contain mwi state\n");
	}

}

#elif defined(CS_AST_HAS_EVENT)
/*!
 * \brief Receive MWI Event from Asterisk
 * \param event Asterisk Event
 * \param data Asterisk Data
 */
void sccp_mwi_event(const struct ast_event *event, void *data)
{
	sccp_mailbox_subscriber_list_t *subscription = data;

	pbx_log(LOG_NOTICE, "Got mwi-event\n");
	if (!subscription || !event) {
		return;
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "Received PBX mwi event for %s@%s\n", (subscription->mailbox) ? subscription->mailbox : "NULL", (subscription->context) ? subscription->context : "NULL");

	/* for calculation store previous voicemail counts */
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	subscription->currentVoicemailStatistic.newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	subscription->currentVoicemailStatistic.oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);

	if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
		sccp_mwi_updatecount(subscription);
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

	if (!subscription) {
		return -1;
	}
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	char buffer[512];

	snprintf(buffer, 512, "%s@%s", subscription->mailbox, subscription->context);
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_4 "SCCP: ckecking mailbox: %s\n", buffer);
	pbx_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);

	/* update devices if something changed */
	if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
		sccp_mwi_updatecount(subscription);
	}

	/* reschedule my self */
	if ((subscription->schedUpdate = PBX(sched_add) (SCCP_MWI_CHECK_INTERVAL * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
		pbx_log(LOG_ERROR, "Error creating mailbox subscription.\n");
	}
	return 0;
}

#endif

/*!
 * \brief Remove Mailbox Subscription
 * \param mailbox SCCP Mailbox
 * \todo Implement sccp_mwi_unsubscribeMailbox ( \todo Marcello)
 */
void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t ** mailbox)
{
	// \todo implement sccp_mwi_unsubscribeMailbox
	return;
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

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "SCCP: (mwi_deviceAttachedEvent) Get deviceAttachedEvent\n");

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

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "SCCP: (mwi_lineStatusChangedEvent) Get lineStatusChangedEvent\n");
	if (event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_DOWN || event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_ONHOOK || event->event.lineStatusChanged.state == SCCP_CHANNELSTATE_RINGING) {	/* these are the only events we are interested in */
		sccp_mwi_check(event->event.lineStatusChanged.optional_device);
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

	sccp_mailbox_t *mailbox;
	sccp_line_t *line = event->event.lineCreated.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "SCCP: (mwi_linecreatedEvent) Get linecreatedEvent\n");

	if (line && (&line->mailboxes) != NULL) {
		SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "line: '%s' mailbox: %s@%s\n", line->name, mailbox->mailbox, mailbox->context);
			sccp_mwi_addMailboxSubscription(mailbox->mailbox, mailbox->context, line);
		}
	}
	return;
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
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_mailbox_subscriptions, subscription, list) {
		if (strlen(mailbox) == strlen(subscription->mailbox) && strlen(context) == strlen(subscription->context) && !strcmp(mailbox, subscription->mailbox) && !strcmp(context, subscription->context)) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);

	if (!subscription) {
		subscription = sccp_malloc(sizeof(sccp_mailbox_subscriber_list_t));
		if (!subscription) {
			pbx_log(LOG_ERROR, "SCCP: (mwi_addMailboxSubscription) Error allocating memory for sccp_mwi_addMailboxSubscription");
			return;
		}
		memset(subscription, 0, sizeof(sccp_mailbox_subscriber_list_t));

		SCCP_LIST_HEAD_INIT(&subscription->sccp_mailboxLine);

		sccp_copy_string(subscription->mailbox, mailbox, sizeof(subscription->mailbox));
		sccp_copy_string(subscription->context, context, sizeof(subscription->context));
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) create subscription for: %s@%s\n", subscription->mailbox, subscription->context);

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
			subscription->currentVoicemailStatistic.newmsgs = ast_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
			subscription->currentVoicemailStatistic.oldmsgs = ast_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
			ast_event_destroy(event);
		} else
#endif
		{												/* Fall back on checking the mailbox directly */
			char buffer[512];

			snprintf(buffer, 512, "%s@%s", subscription->mailbox, subscription->context);
			pbx_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);
		}

#if defined( CS_AST_HAS_EVENT)
		/* register asterisk event */
		//struct pbx_event_sub *pbx_event_subscribe(enum ast_event_type event_type, ast_event_cb_t cb, char *description, void *userdata, ...);
#if ASTERISK_VERSION_NUMBER >= 10800
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, "mailbox subscription", subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_NEWMSGS, AST_EVENT_IE_PLTYPE_EXISTS, AST_EVENT_IE_END);
#else
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, subscription->context, AST_EVENT_IE_END);
#endif
		if (!subscription->event_sub) {
			pbx_log(LOG_ERROR, "SCCP: PBX MWI event could not be subscribed to for mailbox %s@%s\n", subscription->mailbox, subscription->context);
		}
#elif defined(CS_AST_HAS_STASIS) && defined(CS_EXPERIMENTAL)
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) Adding STASIS Subscription for mailbox %s\n", subscription->mailbox);
		char mailbox_context[512];

		snprintf(mailbox_context, 512, "%s@%s", subscription->mailbox, subscription->context);

		struct stasis_topic *mailbox_specific_topic;

		mailbox_specific_topic = ast_mwi_topic(mailbox_context);
		if (mailbox_specific_topic) {
			subscription->event_sub = stasis_subscribe(mailbox_specific_topic, sccp_mwi_event, subscription);
		}
#else
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_addMailboxSubscription) Falling back to polling mailbox status\n");
		if ((subscription->schedUpdate = PBX(sched_add) (SCCP_MWI_CHECK_INTERVAL * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
			pbx_log(LOG_ERROR, "SCCP: (mwi_addMailboxSubscription) Error creating mailbox subscription.\n");
		}
#endif
	}

	/* we already have this subscription */
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		if (line == mailboxLine->line) {
			break;
		}
	}

	if (!mailboxLine) {
		mailboxLine = sccp_malloc(sizeof(sccp_mailboxLine_t));
		if (!mailboxLine) {
			pbx_log(LOG_ERROR, "SCCP: (mwi_addMailboxSubscription) Error allocating memory for mailboxLine");
			return;
		}
		memset(mailboxLine, 0, sizeof(sccp_mailboxLine_t));

		mailboxLine->line = line;

		line->voicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
		line->voicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_INSERT_HEAD(&subscription->sccp_mailboxLine, mailboxLine, list);
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	}
}

/*!
 * \brief Check Line for MWI Status
 * \param line SCCP Line
 */
void sccp_mwi_checkLine(sccp_line_t * line)
{
	sccp_mailbox_t *mailbox = NULL;
	char buffer[512];

	SCCP_LIST_LOCK(&line->mailboxes);
	SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
		snprintf(buffer, 512, "%s@%s", mailbox->mailbox, mailbox->context);
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_checkLine) Line: %s, Mailbox: %s\n", line->name, buffer);
		if (!sccp_strlen_zero(buffer)) {

#ifdef CS_AST_HAS_NEW_VOICEMAIL
			pbx_app_inboxcount(buffer, &line->voicemailStatistic.newmsgs, &line->voicemailStatistic.oldmsgs);
#else
			if (pbx_app_has_voicemail(buffer)) {
				line->voicemailStatistic.newmsgs = 1;
			}
#endif
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_checkLine) Line: %s, Mailbox: %s inbox: %d/%d\n", line->name, buffer, line->voicemailStatistic.newmsgs, line->voicemailStatistic.oldmsgs);
		}
	}
	SCCP_LIST_UNLOCK(&line->mailboxes);
}

/*!
 * \brief Set MWI Line Status
 * \param lineDevice SCCP LineDevice
 */
void sccp_mwi_setMWILineStatus(sccp_linedevices_t * lineDevice)
{
	sccp_msg_t *msg;
	sccp_line_t *l = lineDevice->line;
	sccp_device_t *d = lineDevice->device;
	int instance = 0;
	uint8_t status = 0;
	uint32_t mask;
	uint32_t newState = 0;

	/* when l is defined we are switching on/off the button icon */
	if (l) {
		instance = lineDevice->lineInstance;
		status = l->voicemailStatistic.newmsgs ? 1 : 0;
	}

	mask = 1 << instance;

	newState = d->mwilight;
	/* update status */
	if (status) {
		/* activate */
		newState |= mask;
	} else {
		/* deactivate */
		newState &= ~mask;
	}

	/* check if we need to update line status */
	if ((d->mwilight & ~(1 << 0)) != (newState & ~(1 << 0))) {

		d->mwilight = newState;

		REQ(msg, SetLampMessage);
		msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		msg->data.SetLampMessage.lel_stimulusInstance = htolel(instance);
		msg->data.SetLampMessage.lel_lampMode = (d->mwilight & ~(1 << 0)) ? htolel(d->mwilamp) : htolel(SKINNY_LAMP_OFF);

		sccp_dev_send(d, msg);
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) Turn %s the MWI on line (%s)%d\n", DEV_ID_LOG(d), (mask > 0) ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	} else {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_setMWILineStatus) Device already knows status %s on line %s (%d)\n", DEV_ID_LOG(d), (newState & ~(1 << 0)) ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	}

	sccp_mwi_check(d);
}

/*!
 * \brief Check MWI Status for Device
 * \param d SCCP Device
 * \note called by lineStatusChange
 */
void sccp_mwi_check(sccp_device_t * d)
{
	sccp_buttonconfig_t *config = NULL;

	sccp_msg_t *msg = NULL;

	uint8_t status;
	uint32_t mask;

	uint32_t oldmsgs = 0, newmsgs = 0;

	/* check if we have an active channel */
	boolean_t hasActiveChannel = FALSE, hasRinginChannel = FALSE;

	AUTO_RELEASE sccp_device_t *device = sccp_device_retain(d);

	if (!device) {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "SCCP: (mwi_check) called with NULL device!\n");
		return;
	}

	/* for each line, check if there is an active call */
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == LINE) {
			AUTO_RELEASE sccp_line_t *line = sccp_line_find_byname(config->button.line.name, FALSE);

			if (!line) {
				sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: NULL line retrieved from buttonconfig!\n", DEV_ID_LOG(device));
				continue;
			}
			sccp_channel_t *c = NULL;

			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, c, list) {
				AUTO_RELEASE sccp_device_t *tmpDevice = sccp_channel_getDevice_retained(c);

				if (tmpDevice && tmpDevice == device) {						// We have a channel belonging to our device (no remote shared line channel)
					if (c->state != SCCP_CHANNELSTATE_ONHOOK && c->state != SCCP_CHANNELSTATE_DOWN) {
						hasActiveChannel = TRUE;
					}
					if (c->state == SCCP_CHANNELSTATE_RINGING) {
						hasRinginChannel = TRUE;
					}
				}
			}
			/* pre-collect number of voicemails on device to be set later */
			oldmsgs += line->voicemailStatistic.oldmsgs;
			newmsgs += line->voicemailStatistic.newmsgs;
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (mwi_check) line %s voicemail count %d new/%d old\n", DEV_ID_LOG(device), line->name, line->voicemailStatistic.newmsgs, line->voicemailStatistic.oldmsgs);
			SCCP_LIST_UNLOCK(&line->channels);
		}
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);

	/* disable mwi light if we have an active channel, but no ringin */
	if (hasActiveChannel && !hasRinginChannel && !device->mwioncall) {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: we have an active channel, disable mwi light\n", DEV_ID_LOG(device));

		if (device->mwilight & (1 << 0)) {								// Set the MWI light to off only if it is already on.
			device->mwilight &= ~(1 << 0);								/* set mwi light for device to off */

			REQ(msg, SetLampMessage);
			msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
			msg->data.SetLampMessage.lel_stimulusInstance = 0;
			msg->data.SetLampMessage.lel_lampMode = htolel(SKINNY_LAMP_OFF);
			sccp_dev_send(device, msg);
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s) %d\n", DEV_ID_LOG(device), "OFF", "unknown", 0);
		} else {
			sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: MWI already %s on line (%s) %d\n", DEV_ID_LOG(device), "OFF", "unknown", 0);
		}
		return;												// <---- This return must be outside the inner if
	}

	/* Note: We must return the function before this point unless we want to turn the MWI on during a call! */
	/*       This is taken care of by the previous block of code. */
	device->voicemailStatistic.newmsgs = oldmsgs;
	device->voicemailStatistic.oldmsgs = newmsgs;

	/* set mwi light */
	mask = device->mwilight & ~(1 << 0);									/* status without mwi light for device (1<<0) */
	status = (mask > 0) ? 1 : 0;

	if ((device->mwilight & (1 << 0)) != status) {								/* status needs update */
		if (status) {
			device->mwilight |= (1 << 0);								/* activate */
		} else {
			device->mwilight &= ~(1 << 0);								/* deactivate */
		}

		REQ(msg, SetLampMessage);
		msg->data.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		//msg->data.SetLampMessage.lel_stimulusInstance = 0;
		msg->data.SetLampMessage.lel_lampMode = htolel((device->mwilight) ? device->mwilamp : SKINNY_LAMP_OFF);
		sccp_dev_send(device, msg);
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: Turn %s the MWI light (newmsgs: %d->%d)\n", DEV_ID_LOG(device), (device->mwilight & (1 << 0)) ? "ON" : "OFF", newmsgs, device->voicemailStatistic.newmsgs);

	}
	/* we should check the display only once, maybe we need a priority stack -MC */
	if (newmsgs > 0) {
		char buffer[StationMaxDisplayTextSize];

		snprintf(buffer, StationMaxDisplayTextSize, "%s: (%u/%u)", SKINNY_DISP_YOU_HAVE_VOICEMAIL, newmsgs, oldmsgs);
		sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_VOICEMAIL, buffer);
	} else {
		sccp_device_clearMessageFromStack(device, SCCP_MESSAGE_PRIORITY_VOICEMAIL);
	}
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
int sccp_show_mwi_subscriptions(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_line_t *line = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;
	char linebuf[31] = "";
	int local_total = 0;

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

#ifdef CS_AST_HAS_EVENT
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	subscription->mailbox)						\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	linebuf)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	subscription->context)						\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,		3,	subscription->currentVoicemailStatistic.newmsgs)	\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,		3,	subscription->currentVoicemailStatistic.oldmsgs)	\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->event_sub ? "YES" : "NO")
#include "sccp_cli_table.h"
#else
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-10.10",	s,	10,	subscription->mailbox)						\
 		CLI_AMI_TABLE_FIELD(LineName,		"-30.30",	s,	30,	linebuf)							\
 		CLI_AMI_TABLE_FIELD(Context,		"-15.15",	s,	15,	subscription->context)						\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,		3,	subscription->currentVoicemailStatistic.newmsgs)	\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,		3,	subscription->currentVoicemailStatistic.oldmsgs)
#include "sccp_cli_table.h"
#endif

	if (s) {
		*total = local_total;
	}
	return RESULT_SUCCESS;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
