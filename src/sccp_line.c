
/*!
 * \file 	sccp_line.c
 * \brief 	SCCP Line
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note		Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#ifdef CS_DYNAMIC_CONFIG

/*!
 * \brief run before reload is start on lines
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 */
void sccp_line_pre_reload(void)
{
	sccp_line_t *l;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		/* Don't want to include the hotline line */
		if (GLOB(hotline)->line != l
#    ifdef CS_SCCP_REALTIME
		    && l->realtime == FALSE
#    endif
		    ) {
			l->pendingDelete = 1;
			sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "%s: Setting Line to Pending Delete=1\n", l->name);
		}
		l->pendingUpdate = 0;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
}

/*!
 * \brief run after the new line config is loaded during the reload process
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	  - line
 * 	     - line->devices
 * 	       - device
 * 	  - see sccp_line_clean()
 */
void sccp_line_post_reload(void)
{
	sccp_line_t *l;

	sccp_linedevices_t *ld;

	SCCP_RWLIST_WRLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		if (!l->pendingDelete && !l->pendingUpdate)
			continue;

		sccp_line_lock(l);

		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
			if (!ld->device)
				continue;

			sccp_device_lock(ld->device);
			ld->device->pendingUpdate = 1;
			sccp_device_unlock(ld->device);
		}
		SCCP_LIST_UNLOCK(&l->devices);

		sccp_line_unlock(l);

		if (l->pendingDelete) {
			/* the mutex is destroyed by sccp_line_clean(), it has to be
			 * released before calling it. */
			sccp_line_clean(l, FALSE);
			SCCP_RWLIST_REMOVE_CURRENT(list);
		}
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
}
#endif										/* CS_DYNAMIC_CONFIG */

/*!
 * \brief Build Default SCCP Line.
 *
 * Creates an SCCP Line with default/global values
 *
 * \return Default SCCP Line
 *
 * \callgraph
 * \callergraph
 */
sccp_line_t *sccp_line_create(void)
{
	sccp_line_t *l = ast_malloc(sizeof(sccp_line_t));

	if (!l) {
		sccp_log(0) (VERBOSE_PREFIX_3 "Unable to allocate memory for a line\n");
		return NULL;
	}
	memset(l, 0, sizeof(sccp_line_t));
	ast_mutex_init(&l->lock);
	l = sccp_line_applyDefaults(l);
	SCCP_LIST_HEAD_INIT(&l->channels);
	SCCP_LIST_HEAD_INIT(&l->devices);
	SCCP_LIST_HEAD_INIT(&l->mailboxes);

	return l;
}

/*!
 * \brief Apply Default Configuration to SCCP Line
 * \param l SCCP Line
 * \return SCCP line
 *
 * \callgraph
 * \callergraph
 */
sccp_line_t *sccp_line_applyDefaults(sccp_line_t * l)
{
	if (!l)
		return NULL;

	l->incominglimit = 99;							/* default value */
	l->echocancel = GLOB(echocancel);					/* default value */
	l->silencesuppression = GLOB(silencesuppression);			/* default value */
	l->audio_tos = GLOB(audio_tos);						/* default value */
	l->video_tos = GLOB(video_tos);						/* default value */
	l->audio_cos = GLOB(audio_cos);						/* default value */
	l->video_cos = GLOB(video_cos);						/* default value */
	l->transfer = TRUE;							/* default value. on if the device transfer is on */
	l->secondary_dialtone_tone = SKINNY_TONE_OUTSIDEDIALTONE;
	l->dndmode = SCCP_DNDMODE_OFF;

	sccp_copy_string(l->context, GLOB(context), sizeof(l->context));
	sccp_copy_string(l->language, GLOB(language), sizeof(l->language));
	sccp_copy_string(l->accountcode, GLOB(accountcode), sizeof(l->accountcode));
	sccp_copy_string(l->musicclass, GLOB(musicclass), sizeof(l->musicclass));
	l->meetme = GLOB(meetme);
	sccp_copy_string(l->meetmeopts, GLOB(meetmeopts), sizeof(l->meetmeopts));
	l->amaflags = GLOB(amaflags);
	l->callgroup = GLOB(callgroup);
#ifdef CS_SCCP_PICKUP
	l->pickupgroup = GLOB(pickupgroup);
#endif
	return l;
}

/*!
 * Add a line to global line list.
 * \param line line pointer
 * \since 20091202 - MC
 * 
 * \lock
 * 	- lines
 * 	- see sccp_mwi_linecreatedEvent() via sccp_event_fire()
 */
sccp_line_t *sccp_line_addToGlobals(sccp_line_t * line)
{
	sccp_line_t *l = NULL;

	if (!line) {
		ast_log(LOG_ERROR, "Adding null to global line list is not allowed!\n");
		return NULL;
	}

	SCCP_RWLIST_WRLOCK(&GLOB(lines));
	/* does the line created by an other thread? */
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (!strcasecmp(l->name, line->name)) {
			break;
		}
	}

	if (l) {
		ast_log(LOG_NOTICE, "SCCP: line '%s' was created by an other thread\n", line->name);
		ast_free(line);
		SCCP_RWLIST_UNLOCK(&GLOB(lines));
		return l;
	}

	/* line was not created */
	SCCP_RWLIST_INSERT_HEAD(&GLOB(lines), line, list);
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "Added line '%s'\n", line->name);

	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

	memset(event, 0, sizeof(sccp_event_t));
	event->type = SCCP_EVENT_LINE_CREATED;
	event->event.lineCreated.line = line;
	sccp_event_fire((const sccp_event_t **)&event);

	return line;
}

/*!
 * \brief Kill all Channels of a specific Line
 * \param l SCCP Line
 * \note Should be Called with a lock on l->lock
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- line->channels
 * 	  - see sccp_channel_endcall();
 */
void sccp_line_kill(sccp_line_t * l)
{
	sccp_channel_t *c;

	if (!l)
		return;

	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, c, list) {
		sccp_channel_lock(c);
		sccp_channel_endcall_locked(c);
		sccp_channel_unlock(c);
	}
	SCCP_LIST_UNLOCK(&l->channels);
}

/*!
 * \brief Clean Line
 *
 *  clean up memory allocated by the line.
 *  if destroy is true, line will be removed from global device list
 *
 * \param l SCCP Line
 * \param remove_from_global as boolean_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- lines
 * 	- see sccp_line_kill()
 * 	- line->devices
 * 	- see sccp_line_destroy()
 */
void sccp_line_clean(sccp_line_t * l, boolean_t remove_from_global)
{
	sccp_linedevices_t *linedevice;

	if (!l)
		return;

	if (remove_from_global) {
		SCCP_RWLIST_WRLOCK(&GLOB(lines));
		SCCP_RWLIST_REMOVE(&GLOB(lines), l, list);
		SCCP_RWLIST_UNLOCK(&GLOB(lines));
	}

	sccp_line_kill(l);

	SCCP_LIST_LOCK(&l->devices);
	while ((linedevice = SCCP_LIST_REMOVE_HEAD(&l->devices, list))) {
		if (linedevice) {
			ast_free(linedevice);
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	sccp_line_destroy(l);
}

/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line
 * 	  - see sccp_mwi_unsubscribeMailbox()
 */
int sccp_line_destroy(const void *ptr)
{
	sccp_line_t *l = (sccp_line_t *) ptr;

	sccp_log((DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Line FREE\n", l->name);

	sccp_line_lock(l);
	if (l->trnsfvm)
		ast_free(l->trnsfvm);

	sccp_mailbox_t *mailbox = NULL;

	while ((mailbox = SCCP_LIST_REMOVE_HEAD(&l->mailboxes, list))) {
		if (!mailbox)
			break;

		sccp_mwi_unsubscribeMailbox(&mailbox);
		if (mailbox->mailbox)
			sccp_free(mailbox->mailbox);
		if (mailbox->context)
			sccp_free(mailbox->context);
		sccp_free(mailbox);
	}
	sccp_line_unlock(l);
	ast_mutex_destroy(&l->lock);
	ast_free(l);
	return 0;
}

/*!
 * \brief Delete an SCCP line
 * \param l SCCP Line
 * \note Should be Called without a lock on l->lock
 */
void sccp_line_delete_nolock(sccp_line_t * l)
{
	sccp_line_clean(l, TRUE);
}

/*!
 * \brief Set a Call Forward on a specific Line
 * \param l SCCP Line
 * \param device device that requested the forward
 * \param type Call Forward Type as uint8_t
 * \param number Number to which should be forwarded
 * \todo we should check, that extension is reachable on line
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line->devices
 * 	- see sccp_feat_changed()
 */
void sccp_line_cfwd(sccp_line_t * l, sccp_device_t * device, uint8_t type, char *number)
{
	sccp_linedevices_t *linedevice;

	if (!l)
		return;

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		if ((linedevice->device == device))
			break;
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if (!linedevice) {
		ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return;
	}

	if (type == SCCP_CFWD_NONE) {
		linedevice->cfwdAll.enabled = 0;
		linedevice->cfwdBusy.enabled = 0;
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", DEV_ID_LOG(device), l->name);
	} else {
		if (!number || sccp_strlen_zero(number)) {
			linedevice->cfwdAll.enabled = 0;
			linedevice->cfwdBusy.enabled = 0;
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid\n", DEV_ID_LOG(device));
		} else {
			switch (type) {
			case SCCP_CFWD_ALL:
				linedevice->cfwdAll.enabled = 1;
				sccp_copy_string(linedevice->cfwdAll.number, number, sizeof(linedevice->cfwdAll.number));
				break;
			case SCCP_CFWD_BUSY:
				linedevice->cfwdBusy.enabled = 1;
				sccp_copy_string(linedevice->cfwdBusy.number, number, sizeof(linedevice->cfwdBusy.number));
				break;
			default:
				linedevice->cfwdAll.enabled = 0;
				linedevice->cfwdBusy.enabled = 0;
			}
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Call Forward enabled on line %s to number %s\n", DEV_ID_LOG(device), l->name, number);
		}
	}
	if (linedevice && linedevice->device) {
		sccp_dev_starttone(linedevice->device, SKINNY_TONE_ZIPZIP, 0, 0, 0);
		switch (type) {
		case SCCP_CFWD_ALL:
			sccp_feat_changed(linedevice->device, SCCP_FEATURE_CFWDALL);
		case SCCP_CFWD_BUSY:
			sccp_feat_changed(linedevice->device, SCCP_FEATURE_CFWDBUSY);
		default:
			sccp_feat_changed(linedevice->device, SCCP_FEATURE_CFWDALL);
		}
		sccp_dev_forward_status(l, linedevice->lineInstance, device);
	}
}

/*!
 * \brief Attach a Device to a line
 * \param l SCCP Line
 * \param device SCCP Device
 * \param lineInstance lineInstance as uint8_t
 * \param subscriptionId Subscription ID for addressing individual devices on the line
 * 
 * \lock
 * 	- line->devices
 * 	  - see register_exten()
 * 	- line
 * 	- see sccp_feat_changed()
 * 	- see sccp_dev_forward_status() via sccp_event_fire()
 * 	- see sccp_mwi_deviceAttachedEvent() via sccp_event_fire
 */
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t * device, uint8_t lineInstance, struct subscriptionId *subscriptionId)
{
	sccp_linedevices_t *linedevice = NULL;

	if (!l || !device)
		return;

	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: addDevice for line '%s'\n", DEV_ID_LOG(device), l->name);
	linedevice = sccp_util_getDeviceConfiguration(device, l);
	if (linedevice) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: device already registered for line '%s'\n", DEV_ID_LOG(device), l->name);
		return;
	} else {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: add device to line %s\n", DEV_ID_LOG(device), l->name);
		linedevice = ast_malloc(sizeof(sccp_linedevices_t));
		memset(linedevice, 0, sizeof(sccp_linedevices_t));
	}

	linedevice->device = device;
	linedevice->lineInstance = lineInstance;
	linedevice->line = l;

	if (NULL != subscriptionId) {
		sccp_copy_string(linedevice->subscriptionId.name, subscriptionId->name, sizeof(linedevice->subscriptionId.name));
		sccp_copy_string(linedevice->subscriptionId.number, subscriptionId->number, sizeof(linedevice->subscriptionId.number));
		sccp_copy_string(linedevice->subscriptionId.aux, subscriptionId->aux, sizeof(linedevice->subscriptionId.aux));
	}

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_INSERT_HEAD(&l->devices, linedevice, list);
#ifdef CS_DYNAMIC_CONFIG
	register_exten(l, &(linedevice->subscriptionId));
#endif
	SCCP_LIST_UNLOCK(&l->devices);

	sccp_line_lock(l);
	l->statistic.numberOfActiveDevices++;
	sccp_line_unlock(l);

	/* read cfw status from db */
#ifndef ASTDB_FAMILY_KEY_LEN
#    define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#    define ASTDB_RESULT_LEN 80
#endif
	int res;

	char family[ASTDB_FAMILY_KEY_LEN];

	char buffer[ASTDB_RESULT_LEN];

	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: checking call forward status for line '%s'\n", DEV_ID_LOG(device), l->name);
        linedevice->cfwdAll.enabled = FALSE;
        linedevice->cfwdBusy.enabled = FALSE;
	memset(family, 0, ASTDB_FAMILY_KEY_LEN);
	sprintf(family, "SCCP/%s/%s", device->id, l->name);
	res = pbx_db_get(family, "cfwdAll", buffer, sizeof(buffer));
	if (!res) {
	        // Since upon deactivation of the feature the database entries are removed
	        // The default cfwd state should be FALSE.			
	        // Otherwise any prepared cfwd features activate unwantedly after device restart (-DD, Bug 3396965)			
	        linedevice->cfwdAll.enabled = TRUE;
		sccp_copy_string(linedevice->cfwdAll.number, buffer, sizeof(linedevice->cfwdAll.number));
		sccp_feat_changed(device, SCCP_FEATURE_CFWDALL);
	}

	res = pbx_db_get(family, "cfwdBusy", buffer, sizeof(buffer));
	if (!res) {
		linedevice->cfwdBusy.enabled = TRUE;
		sccp_copy_string(linedevice->cfwdBusy.number, buffer, sizeof(linedevice->cfwdAll.number));
		sccp_feat_changed(device, SCCP_FEATURE_CFWDBUSY);
	}

	if (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) {
		sccp_dev_forward_status(l, lineInstance, device);
	}
	// fire event for new device
	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

	memset(event, 0, sizeof(sccp_event_t));
	event->type = SCCP_EVENT_DEVICE_ATTACHED;
	event->event.deviceAttached.line = l;
	event->event.deviceAttached.device = device;
	sccp_event_fire((const sccp_event_t **)&event);
}

/*!
 * \brief Remove a Device from a Line
 *
 * Fire SCCP_EVENT_DEVICE_DETACHED event after removing device.
 *
 * \param l SCCP Line
 * \param device SCCP Device
 * 
 * \lock
 * 	- line
 * 	  - line->devices
 * 	    - see unregister_exten()
 * 	- see sccp_hint_eventListener() via sccp_event_fire()
 */
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t * device)
{
	sccp_linedevices_t *linedevice;

	if (!l || !device)
		return;

	sccp_log((DEBUGCAT_HIGH + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: remove device from line %s\n", DEV_ID_LOG(device), l->name);

	sccp_line_lock(l);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->devices, linedevice, list) {
		if (linedevice->device == device) {
			SCCP_LIST_REMOVE_CURRENT(list);
#ifdef CS_DYNAMIC_CONFIG
			if (l->devices.size == 0) {
				unregister_exten(l, &(linedevice->subscriptionId));
			}
#endif
			l->statistic.numberOfActiveDevices--;
			ast_free(linedevice);
		}
	}

	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&l->devices);

	sccp_line_unlock(l);

	sccp_hint_lineStatusChanged(l, device, NULL, SCCP_CHANNELSTATE_CONGESTION, SCCP_CHANNELSTATE_CONGESTION);

	/* the hint system uses the line->devices to check the state */
	sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

	memset(event, 0, sizeof(sccp_event_t));

	event->type = SCCP_EVENT_DEVICE_DETACHED;
	event->event.deviceAttached.line = l;
	event->event.deviceAttached.device = device;
	sccp_event_fire((const sccp_event_t **)&event);
}

/*!
 * \brief Add a Channel to a Line
 *
 * \param l SCCP Line
 * \param channel SCCP Channel
 * 
 * \warning
 * 	- line->channels is not always locked
 * 
 * \lock
 * 	- line
 */
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t * channel)
{
	if (!l || !channel)
		return;

	sccp_line_lock(l);
	l->statistic.numberOfActiveChannels++;
	sccp_line_unlock(l);

	if (GLOB(callAnswerOrder) == ANSWER_OLDEST_FIRST)
		SCCP_LIST_INSERT_TAIL(&l->channels, channel, list);
	else
		SCCP_LIST_INSERT_HEAD(&l->channels, channel, list);
}

/*!
 * \brief Remove a Channel from a Line
 *
 * \param l SCCP Line
 * \param channel SCCP Channel
 * 
 * \warning
 * 	- line->channels is not always locked
 * 
 * \lock
 * 	- line
 */
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t * channel)
{
	if (!l || !channel)
		return;

	sccp_line_lock(l);
	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_REMOVE(&l->channels, channel, list);
	SCCP_LIST_UNLOCK(&l->channels);

	sccp_line_unlock(l);
}

/*!
 * \brief Register Extension to Asterisk regextension
 * \param l SCCP Line
 * \param subscriptionId subscriptionId
 * \note used for DUNDi Discovery \ref DUNDi
 */
void register_exten(sccp_line_t * l, struct subscriptionId *subscriptionId)
{
	char multi[256];

	char name[256];

	char *stringp, *ext, *context;

	if (sccp_strlen_zero(GLOB(regcontext)))
		return;

	sccp_copy_string(multi, S_OR(l->regexten, l->name), sizeof(multi));
	stringp = multi;
	while ((ext = strsep(&stringp, "&"))) {
		if ((context = strchr(ext, '@'))) {
			*context++ = '\0';					/* split ext@context */
			if (!ast_context_find(context)) {
				ast_log(LOG_WARNING, "Context %s must exist in regcontext= in sccp.conf\n", context);
				continue;
			}
		} else {
			context = GLOB(regcontext);
		}
		if (!ast_canmatch_extension(NULL, context, ext, 1, NULL)) {
			sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE)) (VERBOSE_PREFIX_1 "Registering RegContext: %s, Extension, %s Line %s\n", context, ext, l->name);
			ast_add_extension(context, 1, ext, 1, NULL, NULL, "Noop", sccp_strdup(l->name), sccp_free_ptr, "SCCP");
		}
		// parse subscriptionId
		if (subscriptionId->number && (strcmp(subscriptionId->number, ""))) {
			strcat(ext, "@");
			strcat(ext, subscriptionId->number);
			sccp_copy_string(name, l->name, sizeof(name));
			strcat(name, subscriptionId->name);
			if (!ast_canmatch_extension(NULL, context, ext, 2, NULL)) {
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE)) (VERBOSE_PREFIX_1 "Registering RegContext: %s, Extension, %s Line %s Subscription number [%s]\n", context, ext, l->name, subscriptionId->number);
				ast_add_extension(context, 1, ext, 2, NULL, NULL, "Noop", sccp_strdup(name), sccp_free_ptr, "SCCP");
			}
		}
	}
}

/*!
 * \brief Unregister Extension to Asterisk regcontext
 * \param l SCCP Line
 * \param subscriptionId subscriptionId
 * \note used for DUNDi Discovery \ref DUNDi
 */
void unregister_exten(sccp_line_t * l, struct subscriptionId *subscriptionId)
{
	char multi[256];

	char *stringp, *ext, *context;

	if (sccp_strlen_zero(GLOB(regcontext)))
		return;

	sccp_copy_string(multi, S_OR(l->regexten, l->name), sizeof(multi));
	stringp = multi;
	while ((ext = strsep(&stringp, "&"))) {
		if ((context = strchr(ext, '@'))) {
			*context++ = '\0';					/* split ext@context */
			if (!ast_context_find(context)) {
				ast_log(LOG_WARNING, "Context %s must exist in regcontext= in sccp.conf!\n", context);
				continue;
			}
		} else {
			context = GLOB(regcontext);
		}
		if (ast_canmatch_extension(NULL, context, ext, 1, NULL)) {
			sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE)) (VERBOSE_PREFIX_1 "Unregistering RegContext: %s, Extension, %s Line %s\n", context, ext, l->name);
			ast_context_remove_extension(context, ext, 1, NULL);
		}
		// parse subscriptionId
		if (subscriptionId->number && (strcmp(subscriptionId->number, ""))) {
			strcat(ext, "@");
			strcat(ext, subscriptionId->number);
			if (ast_canmatch_extension(NULL, context, ext, 2, NULL)) {
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE)) (VERBOSE_PREFIX_1 "Unregistering RegContext: %s, Extension, %s Line %s Subscription number [%s]\n", context, ext, l->name, subscriptionId->number);
				ast_context_remove_extension(context, ext, 2, NULL);
			}
		}
	}
}

#ifdef CS_DYNAMIC_CONFIG

/*!
 * copy the structure content of one line to a new one
 * \param orig_line sccp line
 * \return new_line as sccp_line_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line
 * 	  - see sccp_duplicate_line_mailbox_list()
 * 	  - see sccp_duplicate_line_linedevices_list()
 */
sccp_line_t *sccp_clone_line(sccp_line_t * orig_line)
{
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: Creating Clone (from %p)\n", orig_line->name, (void *)orig_line);
	sccp_line_t *new_line = ast_calloc(1, sizeof(sccp_line_t));

	sccp_line_lock(orig_line);
	memcpy(new_line, orig_line, sizeof(*new_line));
	memset(&new_line->lock, 0, sizeof(new_line->lock));
	ast_mutex_init(&new_line->lock);

	/* remaining values to be copied */
	new_line->trnsfvm = sccp_strdup(orig_line->trnsfvm);
	sccp_copy_string(new_line->adhocNumber, orig_line->adhocNumber, sizeof(new_line->adhocNumber));

	struct ast_variable *v;

	new_line->variables = NULL;
	for (v = orig_line->variables; v; v = v->next) {
		struct ast_variable *new_v = pbx_variable_new(v);

		new_v->next = new_line->variables;
		new_line->variables = new_v;
	}

	/* copy list-items over */
	sccp_duplicate_line_mailbox_list(new_line, orig_line);
	sccp_duplicate_line_linedevices_list(new_line, orig_line);

	sccp_line_unlock(orig_line);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "%s: Clone Created (%p)\n", new_line->name, (void *)new_line);
	return new_line;
}

/*!
 * Copy the list of mailbox from another line
 * \param new_line original sccp line to which the list is copied
 * \param orig_line original sccp line from which to copy the list
 * 
 * \note	orig_line locked by parent
 *
 * \lock
 * 	- line->mailboxes
 */
void sccp_duplicate_line_mailbox_list(sccp_line_t * new_line, sccp_line_t * orig_line)
{
	sccp_mailbox_t *orig_mailbox = NULL;

	sccp_mailbox_t *new_mailbox = NULL;

	SCCP_LIST_HEAD_INIT(&new_line->mailboxes);
	SCCP_LIST_LOCK(&orig_line->mailboxes);
	SCCP_LIST_TRAVERSE(&orig_line->mailboxes, orig_mailbox, list) {
		new_mailbox = ast_calloc(1, sizeof(sccp_mailbox_t));
		if (orig_mailbox->mailbox)
			new_mailbox->mailbox = sccp_strdup(orig_mailbox->mailbox);
		if (orig_mailbox->context)
			new_mailbox->context = sccp_strdup(orig_mailbox->context);
		SCCP_LIST_INSERT_TAIL(&new_line->mailboxes, new_mailbox, list);
	}
	SCCP_LIST_UNLOCK(&orig_line->mailboxes);
}

/*!
 * Copy the list of linedevices from another line
 * \param new_line original sccp line to which the list is copied
 * \param orig_line original sccp line from which to copy the list
 * 
 * \note	orig_line locked by parent
 *
 * \lock
 * 	- line->devices
 * 	  - see sccp_device_find_byid()
 */
void sccp_duplicate_line_linedevices_list(sccp_line_t * new_line, sccp_line_t * orig_line)
{
	sccp_linedevices_t *orig_linedevices = NULL;

	sccp_linedevices_t *new_linedevices = NULL;

	SCCP_LIST_HEAD_INIT(&new_line->devices);
	SCCP_LIST_LOCK(&orig_line->devices);
	SCCP_LIST_TRAVERSE(&orig_line->devices, orig_linedevices, list) {
		new_linedevices = ast_calloc(1, sizeof(sccp_linedevices_t));
		memcpy(new_linedevices, orig_linedevices, sizeof(*new_linedevices));
		new_linedevices->device = sccp_device_find_byid(orig_linedevices->device->id, TRUE);
		SCCP_LIST_INSERT_TAIL(&new_line->devices, new_linedevices, list);
	}
	SCCP_LIST_UNLOCK(&orig_line->devices);
}

/*!
 * Checks two lines against one another and returns a sccp_diff_t if different
 * \param line_a SCCP Line
 * \param line_b SCCP Line
 * \return sccp_diff_t
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- line_a->mailboxes
 * 	- line_b->mailboxes
 */
sccp_diff_t sccp_line_changed(sccp_line_t * line_a, sccp_line_t * line_b)
{
	sccp_diff_t res = NO_CHANGES;

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "(sccp_line_changed) Checking line_a: %s against line_b: %s\n", line_a->id, line_b->id);
	if (									// check changes requiring reset
		   (strcmp(line_a->id, line_b->id)) || (strcmp(line_a->pin, line_b->pin)) || (strcmp(line_a->name, line_b->name)) || (strcmp(line_a->description, line_b->description)) || (strcmp(line_a->label, line_b->label)) ||
#    ifdef CS_SCCP_REALTIME
		   (line_a->realtime != line_b->realtime) ||
#    endif
		   (strcmp(line_a->adhocNumber, line_b->adhocNumber))
	    ) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Changes need reset\n");
		return CHANGES_NEED_RESET;
	} else if (								// check minor changes
			  (strcmp(line_a->vmnum, line_b->vmnum)) ||
			  (line_a->meetme != line_b->meetme) ||
			  (strcmp(line_a->meetmenum, line_b->id)) ||
			  (strcmp(line_a->meetmeopts, line_b->id)) ||
			  (strcmp(line_a->context, line_b->context)) ||
			  (strcmp(line_a->language, line_b->language)) ||
			  (strcmp(line_a->accountcode, line_b->accountcode)) ||
			  (strcmp(line_a->musicclass, line_b->musicclass)) ||
			  (line_a->amaflags != line_b->amaflags) ||
			  (strcmp(line_a->cid_name, line_b->cid_name)) ||
			  (strcmp(line_a->cid_num, line_b->cid_num)) ||
			  (line_a->incominglimit != line_b->incominglimit) ||
			  (line_a->audio_tos != line_b->audio_tos) ||
			  (line_a->video_tos != line_b->video_tos) ||
			  (line_a->audio_cos != line_b->audio_cos) ||
			  (line_a->video_cos != line_b->video_cos) ||
			  (SCCP_RWLIST_GETSIZE(line_a->channels) != SCCP_RWLIST_GETSIZE(line_b->channels)) ||
			  (strcmp(line_a->secondary_dialtone_digits, line_b->secondary_dialtone_digits)) ||
			  (line_a->secondary_dialtone_tone != line_b->secondary_dialtone_tone) ||
			  (line_a->echocancel != line_b->echocancel) ||
			  (line_a->silencesuppression != line_b->silencesuppression) ||
			  (line_a->transfer != line_b->transfer) ||
			  (line_a->spareBit4 != line_b->spareBit4) ||
			  (line_a->spareBit5 != line_b->spareBit5) ||
			  (line_a->spareBit6 != line_b->spareBit6) ||
			  (line_a->dnd != line_b->dnd) ||
			  (line_a->dndmode != line_b->dndmode) ||
			  (line_a->pendingDelete != line_b->pendingDelete) ||
			  (line_a->pendingUpdate != line_b->pendingUpdate) ||
			  (line_a->statistic.numberOfActiveDevices != line_b->statistic.numberOfActiveDevices) ||
			  (line_a->statistic.numberOfActiveChannels != line_b->statistic.numberOfActiveChannels) ||
			  (line_a->statistic.numberOfHoldChannels != line_b->statistic.numberOfHoldChannels) || (line_a->statistic.numberOfDNDDevices != line_b->statistic.numberOfDNDDevices) || (line_a->voicemailStatistic.newmsgs != line_b->voicemailStatistic.newmsgs) || (line_a->voicemailStatistic.oldmsgs != line_b->voicemailStatistic.oldmsgs) || (line_a->configurationStatus != line_b->configurationStatus) || (line_a->callgroup != line_b->callgroup) ||
#    ifdef CS_SCCP_PICKUP
			  (line_a->pickupgroup != line_b->pickupgroup) ||
#    endif
			  (strcmp(line_a->adhocNumber, line_b->adhocNumber)) || (strcmp(line_a->defaultSubscriptionId.number, line_b->defaultSubscriptionId.number)) || (strcmp(line_a->defaultSubscriptionId.name, line_b->defaultSubscriptionId.name))
	    ) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "Minor changes\n");
		res = MINOR_CHANGES;
	}
	// changes in sccp_mailbox_t *orig_mailbox
	SCCP_LIST_LOCK(&line_a->mailboxes);
	SCCP_LIST_LOCK(&line_b->mailboxes);
	if (line_a->mailboxes.size != line_b->mailboxes.size) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "mailboxes: Changes need reset\n");
		res = MINOR_CHANGES;
	} else {
		sccp_mailbox_t *mb_a = SCCP_LIST_FIRST(&line_a->mailboxes);

		sccp_mailbox_t *mb_b = SCCP_LIST_FIRST(&line_b->mailboxes);

		while (mb_a && mb_b) {
			/* First comparison is to know if the values are not both NULL */
			if ((mb_a->mailbox != mb_b->mailbox && (!mb_a->mailbox || !mb_b->mailbox || strcmp(mb_a->mailbox, mb_b->mailbox))) || (mb_a->context != mb_b->context && (!mb_a->context || !mb_b->context || strcmp(mb_a->context, mb_b->context)))) {
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "mailboxes: %s,%s\n", mb_a->mailbox, mb_b->mailbox);
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "mailboxes: %s,%s\n", mb_a->context, mb_b->context);
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_3 "mailboxes: Changes need reset\n");
				res = MINOR_CHANGES;
				break;
			}
			mb_a = SCCP_LIST_NEXT(mb_a, list);
			mb_b = SCCP_LIST_NEXT(mb_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&line_b->mailboxes);
	SCCP_LIST_UNLOCK(&line_a->mailboxes);

	/* \todo still to implement
	   a check for device->setvar (ast_variables *variables)
	   a check for device->trnsfvwm (char  *trnsfvm)
	 */
	sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_2 "(sccp_line_changed) Returning : %d\n", res);
	return res;
}
#endif
