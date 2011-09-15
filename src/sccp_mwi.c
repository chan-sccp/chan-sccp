
/*!
 * \file 	sccp_mwi.c
 * \brief 	SCCP Message Waiting Indicator Class
 * \author 	Marcello Ceschia <marcello.ceschia [at] users.sourceforge.net>
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#ifndef CS_AST_HAS_EVENT
#    define SCCP_MWI_CHECK_INTERVAL 30
#endif
void sccp_mwi_checkLine(sccp_line_t * line);
void sccp_mwi_setMWILineStatus(sccp_device_t * d, sccp_line_t * l);
void sccp_mwi_linecreatedEvent(const sccp_event_t ** event);
void sccp_mwi_deviceAttachedEvent(const sccp_event_t ** event);
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t * line);
void sccp_mwi_lineStatusChangedEvent(const sccp_event_t ** event);

/*!
 * start mwi module.
 */
void sccp_mwi_module_start(void)
{
	/* */
	SCCP_LIST_HEAD_INIT(&sccp_mailbox_subscriptions);

	sccp_event_subscribe(SCCP_EVENT_LINE_CREATED, sccp_mwi_linecreatedEvent);
	sccp_event_subscribe(SCCP_EVENT_DEVICE_ATTACHED, sccp_mwi_deviceAttachedEvent);
	sccp_event_subscribe(SCCP_EVENT_LINESTATUS_CHANGED, sccp_mwi_lineStatusChangedEvent);
}

/*!
 * \brief Stop MWI Monitor
 * 
 * \lock
 * 	- sccp_mailbox_subscriptions
 */
void sccp_mwi_module_stop()
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

#ifdef CS_AST_HAS_EVENT
		/* unsubscribe asterisk event */
		if (subscription->event_sub) {
			pbx_event_unsubscribe(subscription->event_sub);
		}
#else
//              SCCP_SCHED_DEL(sched, subscription->schedUpdate);
		SCCP_SCHED_DEL(subscription->schedUpdate);
#endif

		sccp_free(subscription);
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_HEAD_DESTROY(&sccp_mailbox_subscriptions);
}

#ifdef CS_AST_HAS_EVENT

/*!
 * \brief Receive MWI Event from Asterisk
 * \param event Asterisk Event
 * \param data Asterisk Data
 * 
 * \warning
 * 	- line->devices is not always locked
 * 
 * \lock
 * 	- subscription->sccp_mailboxLine
 * 	  - line
 * 	    - see sccp_mwi_setMWILineStatus()
 */
void sccp_mwi_event(const struct ast_event *event, void *data)
{
	sccp_mailbox_subscriber_list_t *subscription = data;
	sccp_mailboxLine_t *mailboxLine = NULL;
	sccp_line_t *line = NULL;

	pbx_log(LOG_NOTICE, "Got mwi-event\n");
	if (!subscription || !event)
		return;

	sccp_log(DEBUGCAT_EVENT) (VERBOSE_PREFIX_3 "Got mwi event for %s@%s\n", (subscription->mailbox) ? subscription->mailbox : "NULL", (subscription->context) ? subscription->context : "NULL");

	/* for calculation store previous voicemail counts */
	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	subscription->currentVoicemailStatistic.newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	subscription->currentVoicemailStatistic.oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);

	SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		line = mailboxLine->line;
		if (line) {

			sccp_line_lock(line);
			sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_4 "line: %s\n", line->name);
			sccp_linedevices_t *lineDevice = NULL;

			/* update statistics for line  */
			line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

			line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
			line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
			/* done */

			/* notify each device on line */
			SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
				if (NULL != lineDevice && NULL != lineDevice->device) {
					sccp_mwi_setMWILineStatus(lineDevice->device, line);
				} else {
					sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_4 "error: null line device.\n");
				}
			}
			sccp_line_unlock(line);
		}
	}
	SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
}
#else

/*!
 * \brief MWI Progress
 * \param ptr Pointer to Mailbox Subscriber list Entry
 * \note only used for asterisk version without mwi event (scheduled check)
 * 
 * \warning
 * 	- line->devices is not always locked
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- subscription->sccp_mailboxLine
 * 	  - line
 * 	    - see sccp_mwi_setMWILineStatus()
 */
int sccp_mwi_checksubscription(const void *ptr)
{
	sccp_mailbox_subscriber_list_t *subscription = (sccp_mailbox_subscriber_list_t *) ptr;
	sccp_line_t *line = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;

	if (!subscription)
		return -1;

	subscription->previousVoicemailStatistic.newmsgs = subscription->currentVoicemailStatistic.newmsgs;
	subscription->previousVoicemailStatistic.oldmsgs = subscription->currentVoicemailStatistic.oldmsgs;

	char buffer[512];

	sprintf(buffer, "%s@%s", subscription->mailbox, (subscription->context) ? subscription->context : "default");
	sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_4 "SCCP: ckecking mailbox: %s\n", buffer);
	pbx_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);

	/* update devices if something changed */
	if (subscription->previousVoicemailStatistic.newmsgs != subscription->currentVoicemailStatistic.newmsgs) {
		SCCP_LIST_LOCK(&subscription->sccp_mailboxLine);
		SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
			line = mailboxLine->line;
			if (line) {

				sccp_line_lock(line);
				sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_4 "line: %s\n", line->name);
				sccp_linedevices_t *lineDevice = NULL;

				/* update statistics for line  */
				line->voicemailStatistic.oldmsgs -= subscription->previousVoicemailStatistic.oldmsgs;
				line->voicemailStatistic.newmsgs -= subscription->previousVoicemailStatistic.newmsgs;

				line->voicemailStatistic.oldmsgs += subscription->currentVoicemailStatistic.oldmsgs;
				line->voicemailStatistic.newmsgs += subscription->currentVoicemailStatistic.newmsgs;
				/* done */

				/* notify each device on line */
				SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
					sccp_mwi_setMWILineStatus(lineDevice->device, line);
				}
				sccp_line_unlock(line);
			}
		}
		SCCP_LIST_UNLOCK(&subscription->sccp_mailboxLine);
	}

	/* reschedule my self */
	if ((subscription->schedUpdate = sccp_sched_add(sched, SCCP_MWI_CHECK_INTERVAL * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
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
 * 
 * \lock
 * 	- device
 */
void sccp_mwi_deviceAttachedEvent(const sccp_event_t ** event)
{
	if (!(*event))
		return;

	sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_1 "Get deviceAttachedEvent\n");
	sccp_line_t *line = (*event)->event.deviceAttached.linedevice->line;
	sccp_device_t *device = (*event)->event.deviceAttached.linedevice->device;

	if (!line || !device) {
		pbx_log(LOG_ERROR, "get deviceAttachedEvent where one parameter is missing. device: %s, line: %s\n", DEV_ID_LOG(device), (line) ? line->name : "null");
		return;
	}

	sccp_device_lock(device);
	device->voicemailStatistic.oldmsgs += line->voicemailStatistic.oldmsgs;
	device->voicemailStatistic.newmsgs += line->voicemailStatistic.newmsgs;
	sccp_device_unlock(device);
	sccp_mwi_setMWILineStatus(device, line);				/* set mwi-line-status */
}

/*!
 * \brief Line Status Changed Event 
 * \param event SCCP Event
 * 
 * \lock
 * 	- see sccp_mwi_check()
 */
void sccp_mwi_lineStatusChangedEvent(const sccp_event_t ** event)
{
	if (!(*event))
		return;

	sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_1 "Get lineStatusChangedEvent\n");
	sccp_device_t *device = (*event)->event.lineStatusChanged.device;

	if (!device)
		return;

	sccp_mwi_check(device);
}

/*!
 * \brief Line Created Event 
 * \param event SCCP Event
 * 
 * \warning
 * 	- line->mailboxes is not always locked
 */
void sccp_mwi_linecreatedEvent(const sccp_event_t ** event)
{
	if (!(*event))
		return;

	sccp_mailbox_t *mailbox;
	sccp_line_t *line = (*event)->event.lineCreated.line;

	sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_1 "Get linecreatedEvent\n");

	if (!line) {
		pbx_log(LOG_ERROR, "Get linecreatedEvent, but line not set\n");
		return;
	}

	if (line && (&line->mailboxes) != NULL) {
		SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
			sccp_mwi_addMailboxSubscription(mailbox->mailbox, (mailbox->context) ? mailbox->context : "default", line);
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
 * 	- subscription->sccp_mailboxLine is not always locked
 * 
 * \lock
 * 	- sccp_mailbox_subscriptions
 * 	- subscription->sccp_mailboxLine
 */
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t * line)
{
	sccp_mailbox_subscriber_list_t *subscription = NULL;
	sccp_mailboxLine_t *mailboxLine = NULL;

	SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
	SCCP_LIST_TRAVERSE(&sccp_mailbox_subscriptions, subscription, list) {
		if (strlen(mailbox) == strlen(subscription->mailbox)
		    && strlen(context) == strlen(subscription->context)
		    && !strcmp(mailbox, subscription->mailbox)
		    && !strcmp(context, subscription->context)) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);

	if (!subscription) {
		subscription = sccp_malloc(sizeof(sccp_mailbox_subscriber_list_t));
		memset(subscription, 0, sizeof(sccp_mailbox_subscriber_list_t));

		SCCP_LIST_HEAD_INIT(&subscription->sccp_mailboxLine);

		sccp_copy_string(subscription->mailbox, mailbox, sizeof(subscription->mailbox));
		sccp_copy_string(subscription->context, context, sizeof(subscription->context));
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "create subscription for: %s@%s\n", subscription->mailbox, subscription->context);

		SCCP_LIST_LOCK(&sccp_mailbox_subscriptions);
		SCCP_LIST_INSERT_HEAD(&sccp_mailbox_subscriptions, subscription, list);
		SCCP_LIST_UNLOCK(&sccp_mailbox_subscriptions);

		/* get initial value */
		char buffer[512];

		sprintf(buffer, "%s@%s", subscription->mailbox, (subscription->context) ? subscription->context : "default");
		pbx_app_inboxcount(buffer, &subscription->currentVoicemailStatistic.newmsgs, &subscription->currentVoicemailStatistic.oldmsgs);

#ifdef CS_AST_HAS_EVENT
		/* register asterisk event */
		//struct pbx_event_sub *pbx_event_subscribe(enum ast_event_type event_type, ast_event_cb_t cb, char *description, void *userdata, ...);
#    if ASTERISK_VERSION_NUMBER >= 10800
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, "mailbox subscription", subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, S_OR(subscription->context, "default"), AST_EVENT_IE_END);
#    else
		subscription->event_sub = pbx_event_subscribe(AST_EVENT_MWI, sccp_mwi_event, subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, subscription->mailbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, S_OR(subscription->context, "default"), AST_EVENT_IE_END);
#    endif

#else
		if ((subscription->schedUpdate = sccp_sched_add(sched, SCCP_MWI_CHECK_INTERVAL * 1000, sccp_mwi_checksubscription, subscription)) < 0) {
			pbx_log(LOG_ERROR, "Error creating mailbox subscription.\n");
		}
#endif
	}

	/* we already have this subscription */
	SCCP_LIST_TRAVERSE(&subscription->sccp_mailboxLine, mailboxLine, list) {
		if (line == mailboxLine->line)
			break;
	}

	if (!mailboxLine) {
		mailboxLine = sccp_malloc(sizeof(sccp_mailboxLine_t));
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
 * 
 * \lock
 * 	- line->mailboxes
 */
void sccp_mwi_checkLine(sccp_line_t * line)
{
	sccp_mailbox_t *mailbox = NULL;
	char buffer[512];

	SCCP_LIST_LOCK(&line->mailboxes);
	SCCP_LIST_TRAVERSE(&line->mailboxes, mailbox, list) {
		sprintf(buffer, "%s@%s", mailbox->mailbox, (mailbox->context) ? mailbox->context : "default");
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "Line: %s, Mailbox: %s\n", line->name, buffer);
		if (!sccp_strlen_zero(buffer)) {

#ifdef CS_AST_HAS_NEW_VOICEMAIL
			pbx_app_inboxcount(buffer, &line->voicemailStatistic.newmsgs, &line->voicemailStatistic.oldmsgs);
#else
			if (pbx_app_has_voicemail(buffer)) {
				line->voicemailStatistic.newmsgs = 1;
			}
#endif

			sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "Line: %s, Mailbox: %s inbox: %d\n", line->name, buffer, line->voicemailStatistic.newmsgs);
		}
	}
	SCCP_LIST_UNLOCK(&line->mailboxes);
}

/*!
 * \brief Set MWI Line Status
 * \param d SCCP Device
 * \param l SCCP Line
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_find_index_for_line()
 * 	  - see sccp_dev_send()
 * 	  - see sccp_mwi_check()
 */
void sccp_mwi_setMWILineStatus(sccp_device_t * d, sccp_line_t * l)
{
	sccp_moo_t *r;
	int instance = 0;
	uint8_t status = 0;
	uint32_t mask;
	uint32_t newState = 0;

	if (!d)
		return;

	int retry = 0;

	while (sccp_device_trylock(d)) {
		retry++;
		sccp_log((DEBUGCAT_MWI + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s), retry: %d\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, retry);
		usleep(100);

		if (retry > 100) {
			return;
		}
	}

	/* when l is defined we are switching on/off the button icon */
	if (l) {
		instance = sccp_device_find_index_for_line(d, l->name);
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

		REQ(r, SetLampMessage);
		r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		r->msg.SetLampMessage.lel_stimulusInstance = htolel(instance);
		//r->msg.SetLampMessage.lel_lampMode = htolel( (l->voicemailStatistic.newmsgs) ? SKINNY_LAMP_ON :  SKINNY_LAMP_OFF);
		r->msg.SetLampMessage.lel_lampMode = htolel((status) ? d->mwilamp : SKINNY_LAMP_OFF);

		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s)%d\n", DEV_ID_LOG(d), (mask > 0) ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	} else {
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: Device already know status %s on line %s (%d)\n", DEV_ID_LOG(d), (mask > 0) ? "ON" : "OFF", (l ? l->name : "unknown"), instance);
	}

	sccp_mwi_check(d);
	sccp_device_unlock(d);
}

/*!
 * \brief Check MWI Status for Device
 * \param device SCCP Device
 * \note called by lineStatusChange
 * 
 * \lock
 * 	- device->buttonconfig
 * 	  - see sccp_line_find_byname_wo()
 * 	  - line->channels
 * 	- device
 * 	  - device->buttonconfig
 * 	    - see sccp_line_find_byname_wo()
 * 	  - see sccp_dev_send()
 * 	  - see sccp_dev_check_displayprompt()
 */
void sccp_mwi_check(sccp_device_t * device)
{
	sccp_buttonconfig_t *config = NULL;

	sccp_line_t *line = NULL;
	sccp_channel_t *c = NULL;

	sccp_moo_t *r = NULL;

	uint8_t status;
	uint32_t mask;

	uint32_t oldmsgs = 0, newmsgs = 0;

	/* check if we have an active channel */
	boolean_t hasActiveChannel = FALSE, hasRinginChannel = FALSE;

	if (!device) {
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "sccp_mwi_check called with NULL device!\n");
		return;
	}

	/* for each line, check if there is an active call */
	SCCP_LIST_LOCK(&device->buttonconfig);
	SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
		if (config->type == LINE) {
			line = sccp_line_find_byname_wo(config->button.line.name, FALSE);
			if (!line) {
				sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: NULL line retrieved from buttonconfig!\n", DEV_ID_LOG(device));
				continue;
			}

			SCCP_LIST_LOCK(&line->channels);
			SCCP_LIST_TRAVERSE(&line->channels, c, list) {
				if (sccp_channel_getDevice(c) == device) {			// We have a channel belonging to our device (no remote shared line channel)
					if (c->state != SCCP_CHANNELSTATE_ONHOOK && c->state != SCCP_CHANNELSTATE_DOWN) {
						hasActiveChannel = TRUE;
					}
					if (c->state == SCCP_CHANNELSTATE_RINGING) {
						hasRinginChannel = TRUE;
					}

					/* pre-collect number of voicemails on device to be set later */
					oldmsgs += line->voicemailStatistic.oldmsgs;
					newmsgs += line->voicemailStatistic.newmsgs;
				}
			}
			SCCP_LIST_UNLOCK(&line->channels);
		}
	}
	SCCP_LIST_UNLOCK(&device->buttonconfig);

	/* disable mwi light if we have an active channel, but no ringin */
	if (hasActiveChannel && !hasRinginChannel && !device->mwioncall) {
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: we have an active channel, disable mwi light\n", DEV_ID_LOG(device));

		if (device->mwilight & (1 << 0)) {				// Set the MWI light to off only if it is already on.
			device->mwilight &= ~(1 << 0);				/* set mwi light for device to off */

			REQ(r, SetLampMessage);
			r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
			r->msg.SetLampMessage.lel_stimulusInstance = 0;
			r->msg.SetLampMessage.lel_lampMode = htolel(SKINNY_LAMP_OFF);
			sccp_dev_send(device, r);
			sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: Turn %s the MWI on line (%s) %d\n", DEV_ID_LOG(device), "OFF", "unknown", 0);
		} else {
			sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: MWI already %s on line (%s) %d\n", DEV_ID_LOG(device), "OFF", "unknown", 0);
		}
		return;								// <---- This return must be outside the inner if
	}

	/* Note: We must return the function before this point unless we want to turn the MWI on during a call! */
	/*       This is taken care of by the previous block of code. */

	sccp_device_lock(device);
	device->voicemailStatistic.newmsgs = oldmsgs;
	device->voicemailStatistic.oldmsgs = newmsgs;

	/* set mwi light */
	mask = device->mwilight & ~(1 << 0);					/* status without mwi light for device (1<<0) */
	status = (mask > 0) ? 1 : 0;

	if ((device->mwilight & (1 << 0)) != status) {				/* status needs update */
		if (status) {
			device->mwilight |= (1 << 0);				/* activate */
		} else {
			device->mwilight &= ~(1 << 0);				/* deactivate */
		}

		REQ(r, SetLampMessage);
		r->msg.SetLampMessage.lel_stimulus = htolel(SKINNY_STIMULUS_VOICEMAIL);
		//r->msg.SetLampMessage.lel_stimulusInstance = 0;
		r->msg.SetLampMessage.lel_lampMode = htolel((device->mwilight) ? device->mwilamp : SKINNY_LAMP_OFF);
		sccp_dev_send(device, r);
		sccp_log(DEBUGCAT_MWI) (VERBOSE_PREFIX_3 "%s: Turn %s the MWI light\n", DEV_ID_LOG(device), (device->mwilight > 0) ? "ON" : "OFF");

		/* we should check the display only once, maybe we need a priority stack -MC */
		char buffer[StationMaxDisplayTextSize];
		sprintf(buffer, "%s: (%d/%d)", SKINNY_DISP_YOU_HAVE_VOICEMAIL, device->voicemailStatistic.newmsgs, device->voicemailStatistic.oldmsgs);
		
		sccp_device_addMessageToStack(device, SCCP_MESSAGE_PRIORITY_VOICEMAIL, buffer);
		//sccp_dev_check_displayprompt(device);
	}

	sccp_device_unlock(device);
}
