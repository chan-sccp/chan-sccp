/*!
 * \file        sccp_line.c
 * \brief       SCCP Line
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_config.h"
#include "sccp_features.h"
#include "sccp_mwi.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

static void regcontext_exten(sccp_line_t * l, struct subscriptionId *subscriptionId, int onoff);
int __sccp_line_destroy(const void *ptr);
int __sccp_lineDevice_destroy(const void *ptr);
int sccp_line_destroy(const void *ptr);

/*!
 * \brief run before reload is start on lines
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_line_pre_reload(void)
{
	sccp_line_t *l = NULL;
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		if (GLOB(hotline)->line == l) {									/* always remove hotline from linedevice */
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Removing Hotline from Device\n", l->name);
			sccp_line_removeDevice(l, NULL);
		} else {											/* Don't want to include the hotline line */
#ifdef CS_SCCP_REALTIME
			if (l->realtime == FALSE)
#endif
			{
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Setting Line to Pending Delete=1\n", l->name);
				l->pendingDelete = 1;
			}
		}
		l->pendingUpdate = 0;
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

/*!
 * \brief run after the new line config is loaded during the reload process
 * \note See \ref sccp_config_reload
 * \todo to be implemented correctly (***)
 *
 * \callgraph
 * \callergraph
 * 
 */
void sccp_line_post_reload(void)
{
	sccp_line_t *line = NULL;

	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), line, list) {
		if (!line->pendingDelete && !line->pendingUpdate) {
			continue;
		}
		AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));

		if (l) {
			// existing lines
			sccp_linedevices_t *linedevice = NULL;
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
				linedevice->device->pendingUpdate = 1;
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: LineDevice (line_post_reload) update:%d, delete:%d\n", l->name, linedevice->device->pendingUpdate, linedevice->device->pendingDelete);
			}
			SCCP_LIST_UNLOCK(&l->devices);
			
			if (l->pendingDelete) {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Deleting Line (post_reload)\n", l->name);
				sccp_line_clean(l, TRUE);
			} else {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Cleaning Line (post_reload)\n", l->name);
				sccp_line_clean(l, FALSE);
			}
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Line (line_post_reload) update:%d, delete:%d\n", l->name, l->pendingUpdate, l->pendingDelete);
		}
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
}

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
sccp_line_t *sccp_line_create(const char *name)
{
	sccp_line_t *l = NULL;

	if ((l = sccp_line_find_byname(name, FALSE))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' already exists.\n", name);
		sccp_line_release(&l);						/* explicit release of found line */
		return NULL;
	}
	
	l = (sccp_line_t *) sccp_refcount_object_alloc(sizeof(sccp_line_t), SCCP_REF_LINE, name, __sccp_line_destroy);
	if (!l) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, name);
		return NULL;
	}
	memset(l, 0, sizeof(sccp_line_t));
	sccp_copy_string(l->name, name, sizeof(l->name));

	SCCP_LIST_HEAD_INIT(&l->channels);
	SCCP_LIST_HEAD_INIT(&l->devices);
	SCCP_LIST_HEAD_INIT(&l->mailboxes);
	return l;
}

/*!
 * Add a line to global line list.
 * \param line line pointer
 * \since 20091202 - MC
 * 
 * \note needs to be called with a retained line
 * \note adds a retained line to the list (refcount + 1)
 */
void sccp_line_addToGlobals(sccp_line_t * line)
{
	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));

	SCCP_RWLIST_WRLOCK(&GLOB(lines));
	if (l) {
		/* add to list */
		sccp_line_retain(l);										/* add retained line to the list */
		SCCP_RWLIST_INSERT_SORTALPHA(&GLOB(lines), l, list, cid_num);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Added line '%s' to Glob(lines)\n", l->name);

		/* emit event */
		sccp_event_t event = {{{0}}};
		event.type = SCCP_EVENT_LINE_CREATED;
		event.event.lineCreated.line = sccp_line_retain(l);
		sccp_event_fire(&event);
	} else {
		pbx_log(LOG_ERROR, "Adding null to global line list is not allowed!\n");
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
}

/*!
 * Remove a line from the global line list.
 * \param line SCCP line pointer
 * 
 * \note needs to be called with a retained line
 * \note removes the retained line withing the list (refcount - 1)
 */
void sccp_line_removeFromGlobals(sccp_line_t * line)
{
	sccp_line_t *removed_line = NULL;
	if (line) {
		SCCP_RWLIST_WRLOCK(&GLOB(lines));
		removed_line = SCCP_RWLIST_REMOVE(&GLOB(lines), line, list);
		SCCP_RWLIST_UNLOCK(&GLOB(lines));

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Removed line '%s' from Glob(lines)\n", removed_line->name);

		//sccp_event_t event = {{{0}}};
		//event.type = SCCP_EVENT_LINE_DELETED;
		//event.event.lineCreated.line = sccp_line_retain(removed_line);
		//sccp_event_fire(&event);
		
		sccp_line_release(&removed_line);								/* explicit release */
	} else {
		pbx_log(LOG_ERROR, "Removing null from global line list is not allowed!\n");
	}
}

/*!
 * \brief create a hotline
 * 
 */
void *sccp_create_hotline(void)
{

	GLOB(hotline) = (sccp_hotline_t *) sccp_malloc(sizeof(sccp_hotline_t));
	if (!GLOB(hotline)) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}
	memset(GLOB(hotline), 0, sizeof(sccp_hotline_t));

	//SCCP_RWLIST_WRLOCK(&GLOB(lines));
	AUTO_RELEASE(sccp_line_t, hotline , sccp_line_create("Hotline"));

	if (hotline) {
#ifdef CS_SCCP_REALTIME
		hotline->realtime = TRUE;
#endif
		hotline->label = pbx_strdup("Hotline");
		hotline->context = pbx_strdup("default");
		sccp_copy_string(hotline->cid_name, "hotline", sizeof(hotline->cid_name));
		sccp_copy_string(hotline->cid_num, "hotline", sizeof(hotline->cid_name));
		GLOB(hotline)->line = sccp_line_retain(hotline);						// retain line inside hotline
		sccp_line_addToGlobals(hotline);								// retain line inside GlobalsList
	}
	//SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return NULL;
}

/*!
 * \brief Kill all Channels of a specific Line
 * \param l SCCP Line
 *
 * \callgraph
 * \callergraph
 *
 */
void sccp_line_kill_channels(sccp_line_t * l)
{
	sccp_channel_t *c = NULL;

	if (!l) {
		return;
	}
	SCCP_LIST_LOCK(&l->channels);
	while ((c = SCCP_LIST_REMOVE_HEAD(&l->channels, list))) {
		sccp_channel_endcall(c);
		sccp_channel_release(&c);									// explicit release channel retain in list
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
 */
void sccp_line_clean(sccp_line_t * l, boolean_t remove_from_global)
{
	sccp_line_kill_channels(l);
	sccp_line_removeDevice(l, NULL);									// removing all devices from this line.
	if (remove_from_global) {
		sccp_line_destroy(l);
	}
}

/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 * 
 */
int __sccp_line_destroy(const void *ptr)
{
	sccp_mailbox_t *mailbox = NULL;
	sccp_line_t *l = (sccp_line_t *) ptr;
	if (!l) {
		return -1;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Line FREE\n", l->name);

	// cleaning l->channels
	sccp_line_kill_channels(l);

	// cleaning l->devices
	sccp_line_removeDevice(l, NULL);

	// cleanup mailboxes (l->mailboxes)
	{
		SCCP_LIST_LOCK(&l->mailboxes);
		while ((mailbox = SCCP_LIST_REMOVE_HEAD(&l->mailboxes, list))) {
			sccp_mwi_unsubscribeMailbox(mailbox);
			if (mailbox->mailbox) {
				sccp_free(mailbox->mailbox);
			}
			if (mailbox->context) {
				sccp_free(mailbox->context);
			}
			sccp_free(mailbox);
		}
		SCCP_LIST_UNLOCK(&l->mailboxes);
		if (!SCCP_LIST_EMPTY(&l->mailboxes)) {
			pbx_log(LOG_WARNING, "%s: (line_destroy) there are connected mailboxes left during line destroy\n", l->name);
		}
		SCCP_LIST_HEAD_DESTROY(&l->mailboxes);
	}

	// cleanup variables
	if (l->variables) {
		pbx_variables_destroy(l->variables);
		l->variables = NULL;
	}

	// cleanup dynamically allocated memory by sccp_config
	sccp_config_cleanup_dynamically_allocated_memory(l, SCCP_CONFIG_LINE_SEGMENT);

	// cleanup regcontext
	if (l->regcontext) {
		sccp_free(l->regcontext);
	}

	// destroy lists
	if (!SCCP_LIST_EMPTY(&l->channels)) {
		pbx_log(LOG_WARNING, "%s: (line_destroy) there are connected channels left during line destroy\n", l->name);
	}
	SCCP_LIST_HEAD_DESTROY(&l->channels);

	if (!SCCP_LIST_EMPTY(&l->devices)) {
		pbx_log(LOG_WARNING, "%s: (line_destroy) there are connected device left during line destroy\n", l->name);
	}
	SCCP_LIST_HEAD_DESTROY(&l->devices);

	return 0;
}

/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 * 
 */
int __sccp_lineDevice_destroy(const void *ptr)
{
	sccp_linedevices_t *linedevice = (sccp_linedevices_t *) ptr;

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: LineDevice FREE %p\n", DEV_ID_LOG(linedevice->device), linedevice);
	if (linedevice->line) {
		sccp_line_release(&linedevice->line);						/* explicit release of line retained in linedevice */
	}
	if (linedevice->device) {
		sccp_device_release(&linedevice->device);					/* explicit release of device retained in linedevice */
	}
	return 0;
}

/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 * 
 */
int sccp_line_destroy(const void *ptr)
{
	sccp_line_t *l = (sccp_line_t *) ptr;

	sccp_line_removeFromGlobals(l);										// final release
	return 0;
}

void sccp_line_copyCodecSetsFromLineToChannel(sccp_line_t *l, sccp_channel_t *c)
{
	sccp_linedevices_t *linedevice = NULL;
	boolean_t first=TRUE;
	if (!l || !c) {
		return;
	}
	/* combine all capabilities */
	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		if (first) {
			memcpy(&c->capabilities.audio, &linedevice->device->capabilities.audio, sizeof(c->capabilities.audio));
			memcpy(&c->capabilities.video, &linedevice->device->capabilities.video, sizeof(c->capabilities.video));
			memcpy(&c->preferences.audio , &linedevice->device->preferences.audio , sizeof(c->preferences.audio));
			memcpy(&c->preferences.video , &linedevice->device->preferences.video , sizeof(c->preferences.video));
			first = FALSE;
		} else {
			sccp_codec_combineSets(c->capabilities.audio, linedevice->device->capabilities.audio);
			sccp_codec_combineSets(c->capabilities.video, linedevice->device->capabilities.video);
			sccp_codec_reduceSet(c->preferences.audio , linedevice->device->preferences.audio);
			sccp_codec_reduceSet(c->preferences.video , linedevice->device->preferences.video);
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);
}

/*!
 * \brief Set a Call Forward on a specific Line
 * \param line SCCP Line
 * \param device device that requested the forward
 * \param type Call Forward Type as uint8_t
 * \param number Number to which should be forwarded
 * \todo we should check, that extension is reachable on line
 *
 * \callgraph
 * \callergraph
 * 
 * \todo implement cfwd_noanswer
 */
void sccp_line_cfwd(constLinePtr line, constDevicePtr device, sccp_callforward_t type, char *number)
{
	AUTO_RELEASE(sccp_linedevices_t, linedevice , NULL);
	sccp_feature_type_t feature_type = SCCP_FEATURE_CFWDNONE;

	if (!line || !device) {
		return;
	}
	if ((linedevice = sccp_linedevice_find(device, line))) {
		if (type == SCCP_CFWD_NONE) {
			if (linedevice->cfwdAll.enabled) {
				feature_type = SCCP_FEATURE_CFWDALL;
			}
			if (linedevice->cfwdBusy.enabled) {
				feature_type = SCCP_FEATURE_CFWDBUSY;
			}
			linedevice->cfwdAll.enabled = 0;
			linedevice->cfwdBusy.enabled = 0;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", DEV_ID_LOG(device), line->name);
		} else {
			if (!number || sccp_strlen_zero(number)) {
				linedevice->cfwdAll.enabled = 0;
				linedevice->cfwdBusy.enabled = 0;
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid. Cfwd Disabled\n", DEV_ID_LOG(device));
			} else {
				switch (type) {
					case SCCP_CFWD_ALL:
						feature_type = SCCP_FEATURE_CFWDALL;
						linedevice->cfwdAll.enabled = 1;
						sccp_copy_string(linedevice->cfwdAll.number, number, sizeof(linedevice->cfwdAll.number));
						break;
					case SCCP_CFWD_BUSY:
						feature_type = SCCP_FEATURE_CFWDBUSY;
						linedevice->cfwdBusy.enabled = 1;
						sccp_copy_string(linedevice->cfwdBusy.number, number, sizeof(linedevice->cfwdBusy.number));
						break;
					default:
						linedevice->cfwdAll.enabled = 0;
						linedevice->cfwdBusy.enabled = 0;
				}
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward %s enabled on line %s to number %s\n", DEV_ID_LOG(device), sccp_callforward2str(type), line->name, number);
			}
		}
		sccp_feat_changed(linedevice->device, linedevice, feature_type);
		sccp_dev_forward_status(linedevice->line, linedevice->lineInstance, device);
	} else {
		pbx_log(LOG_ERROR, "%s: Device does not have line configured (linedevice not found)\n", DEV_ID_LOG(device));
	}
}

void sccp_linedevice_resetPickup(sccp_linedevices_t * ld) {

	ld->isPickupAllowed = sccp_always_false;
#ifdef CS_SCCP_PICKUP
	if (ld->line->pickupgroup
#ifdef CS_AST_HAS_NAMEDGROUP
		|| !sccp_strlen_zero(ld->line->namedpickupgroup)
#endif
	) {
		sccp_log(DEBUGCAT_LINE)("%s: (allowPickup) on line:%s.\n", ld->device->id, ld->line->name);
		ld->isPickupAllowed = sccp_always_true;
	}
#endif
}

void sccp_linedevice_disallowPickup(sccp_linedevices_t * ld) {
	sccp_log(DEBUGCAT_LINE)("%s: (disallowPickup) on line:%s.\n", ld->device->id, ld->line->name);
	ld->isPickupAllowed = sccp_always_false;
}

/*!
 * \brief Attach a Device to a line
 * \param line SCCP Line
 * \param d SCCP Device
 * \param lineInstance lineInstance as uint8_t
 * \param subscriptionId Subscription ID for addressing individual devices on the line
 * 
 */
void sccp_line_addDevice(sccp_line_t * line, sccp_device_t * d, uint8_t lineInstance, sccp_subscription_id_t *subscriptionId)
{
	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));
	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));

	if (!device || !l) {
		pbx_log(LOG_ERROR, "SCCP: sccp_line_addDevice: No line or device provided\n");
		return;
	}
	sccp_linedevices_t *linedevice = NULL;

	if ((linedevice = sccp_linedevice_find(device, l))) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: device already registered for line '%s'\n", DEV_ID_LOG(device), l->name);
		sccp_linedevice_release(&linedevice);			/* explicit release of found linedevice */
		return;
	}
	
	sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: add device to line %s\n", DEV_ID_LOG(device), l->name);
#if CS_REFCOUNT_DEBUG
	sccp_refcount_addWeakParent(line, device);
#endif
	char ld_id[REFCOUNT_INDENTIFIER_SIZE];

	snprintf(ld_id, REFCOUNT_INDENTIFIER_SIZE, "%s/%s", device->id, l->name);
	linedevice = (sccp_linedevices_t *) sccp_refcount_object_alloc(sizeof(sccp_linedevices_t), SCCP_REF_LINEDEVICE, ld_id, __sccp_lineDevice_destroy);
	if (!linedevice) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, ld_id);
		return;
	}
	memset(linedevice, 0, sizeof(sccp_linedevices_t));
#if CS_REFCOUNT_DEBUG
	sccp_refcount_addWeakParent(linedevice, l);
	sccp_refcount_addWeakParent(linedevice, device);
#endif
	linedevice->device = sccp_device_retain(device);
	linedevice->line = sccp_line_retain(l);
	linedevice->lineInstance = lineInstance;
	
	sccp_linedevice_resetPickup(linedevice);
	if (NULL != subscriptionId) {
		sccp_copy_string(linedevice->subscriptionId.name, subscriptionId->name, sizeof(linedevice->subscriptionId.name));
		sccp_copy_string(linedevice->subscriptionId.number, subscriptionId->number, sizeof(linedevice->subscriptionId.number));
		sccp_copy_string(linedevice->subscriptionId.label, subscriptionId->label, sizeof(linedevice->subscriptionId.label));
		sccp_copy_string(linedevice->subscriptionId.aux, subscriptionId->aux, sizeof(linedevice->subscriptionId.aux));
		linedevice->subscriptionId.replaceCid = subscriptionId->replaceCid;
	}

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_INSERT_HEAD(&l->devices, linedevice, list);
	SCCP_LIST_UNLOCK(&l->devices);

	linedevice->line->statistic.numberOfActiveDevices++;
	linedevice->device->configurationStatistic.numberOfLines++;

	// fire event for new device
	sccp_event_t event = {{{0}}};
	event.type = SCCP_EVENT_DEVICE_ATTACHED;
	event.event.deviceAttached.linedevice = sccp_linedevice_retain(linedevice);
	sccp_event_fire(&event);

	regcontext_exten(l, &(linedevice->subscriptionId), 1);
	sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: added linedevice: %p with device: %s\n", l->name, linedevice, DEV_ID_LOG(device));
}

/*!
 * \brief Remove a Device from a Line
 *
 * Fire SCCP_EVENT_DEVICE_DETACHED event after removing device.
 *
 * \param l SCCP Line
 * \param device SCCP Device
 *
 * \note device can be NULL, mening remove all device from this line
 * 
 */
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t * device)
{
	sccp_linedevices_t *linedevice = NULL;

	if (!l) {
		return;
	}
	sccp_log_and((DEBUGCAT_HIGH + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: remove device from line %s\n", DEV_ID_LOG(device), l->name);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->devices, linedevice, list) {
		if (device == NULL || linedevice->device == device) {
#if CS_REFCOUNT_DEBUG
			sccp_refcount_removeWeakParent(l, device ? device : linedevice->device);
#endif
			regcontext_exten(l, &(linedevice->subscriptionId), 0);
			SCCP_LIST_REMOVE_CURRENT(list);
			l->statistic.numberOfActiveDevices--;

			sccp_event_t event = {{{0}}};
			event.type = SCCP_EVENT_DEVICE_DETACHED;
			event.event.deviceAttached.linedevice = sccp_linedevice_retain(linedevice);	/* after processing this event the linedevice will be cleaned up */
			sccp_event_fire(&event);

			sccp_linedevice_release(&linedevice);						/* explicit release of list retained linedevice */
#ifdef CS_SCCP_REALTIME
			if (l->realtime && SCCP_LIST_GETSIZE(&l->devices) == 0 && SCCP_LIST_GETSIZE(&l->channels) == 0 ) {
				sccp_line_removeFromGlobals(l);
			}
#endif
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&l->devices);
}

/*!
 * \brief Add a Channel to a Line
 *
 * \param line SCCP Line
 * \param channel SCCP Channel
 * 
 * \warning
 *  - line->channels is not always locked
 * 
 */
void sccp_line_addChannel(constLinePtr line, constChannelPtr channel)
{
	if (!line || !channel) {
		return;
	}
	sccp_channel_t *c = NULL;

	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));

	if (l) {
		//l->statistic.numberOfActiveChannels++;
		SCCP_LIST_LOCK(&l->channels);
		if ((c = sccp_channel_retain(channel))) {							// Add into list retained
#if CS_REFCOUNT_DEBUG
			sccp_refcount_addWeakParent(l, c);
#endif
			sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_1 "SCCP: Adding channel %d to line %s\n", c->callid, l->name);
			if (GLOB(callanswerorder) == SCCP_ANSWER_OLDEST_FIRST) {
				SCCP_LIST_INSERT_TAIL(&l->channels, c, list);					// add to list
			} else {
				SCCP_LIST_INSERT_HEAD(&l->channels, c, list);					// add to list
			}
		}
		SCCP_LIST_UNLOCK(&l->channels);
	}
}

/*!
 * \brief Remove a Channel from a Line
 *
 * \param line SCCP Line
 * \param channel SCCP Channel
 * 
 * \warning
 *  - line->channels is not always locked
 * 
 */
void sccp_line_removeChannel(sccp_line_t * line, sccp_channel_t * channel)
{
	if (!line || !channel) {
		return;
	}
	sccp_channel_t *c = NULL;
	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));

	if (l) {
		SCCP_LIST_LOCK(&l->channels);
		if ((c = SCCP_LIST_REMOVE(&l->channels, channel, list))) {
#if CS_REFCOUNT_DEBUG
			sccp_refcount_removeWeakParent(l, c);
#endif
			if (c->state == SCCP_CHANNELSTATE_HOLD) {
				c->line->statistic.numberOfHeldChannels--;
			}
			sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_1 "SCCP: Removing channel %d from line %s\n", c->callid, l->name);
			sccp_channel_release(&c);					/* explicit release of channel from list */
		}
		SCCP_LIST_UNLOCK(&l->channels);
	}
}

/*!
 * \brief Register Extension to Asterisk regextension
 * \param l SCCP Line
 * \param subscriptionId subscriptionId
 * \param onoff On/Off as int
 * \note used for DUNDi Discovery
 */
static void regcontext_exten(sccp_line_t * l, struct subscriptionId *subscriptionId, int onoff)
{
	char multi[256] = "";
	char *stringp, *ext = "", *context = "";

	// char extension[SCCP_MAX_CONTEXT]="";
	// char name[SCCP_MAX_CONTEXT]="";

	struct pbx_context *con;
	struct pbx_find_info q = {.stacklen = 0 };

	if (sccp_strlen_zero(GLOB(regcontext))) {
		return;
	}

	sccp_copy_string(multi, S_OR(l->regexten, l->name), sizeof(multi));
	stringp = multi;
	while ((ext = strsep(&stringp, "&"))) {
		if ((context = strchr(ext, '@'))) {
			*context++ = '\0';									/* split ext@context */
			if (!pbx_context_find(context)) {
				pbx_log(LOG_WARNING, "Context specified in regcontext=%s (sccp.conf) must exist\n", context);
				continue;
			}
		} else {
			context = pbx_strdupa(GLOB(regcontext));
		}
		con = pbx_context_find_or_create(NULL, NULL, context, "SCCP");					/* make sure the context exists */
		if (con) {
			if (onoff) {
				/* register */

				if (!pbx_exists_extension(NULL, context, ext, 1, NULL) && pbx_add_extension(context, 0, ext, 1, NULL, NULL, "Noop", pbx_strdup(l->name), sccp_free_ptr, "SCCP")) {
					sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Registered RegContext: %s, Extension: %s, Line: %s\n", context, ext, l->name);
				}

				/* register extension + subscriptionId */
				/* if (subscriptionId && subscriptionId->number && !sccp_strlen_zero(subscriptionId->number) && !sccp_strlen_zero(subscriptionId->name)) {
				   snprintf(extension, sizeof(extension), "%s@%s", ext, subscriptionId->number);
				   snprintf(name, sizeof(name), "%s%s", l->name, subscriptionId->name);
				   if (!pbx_exists_extension(NULL, context, extension, 2, NULL) && pbx_add_extension(context, 0, extension, 2, NULL, NULL, "Noop", pbx_strdup(name), sccp_free_ptr, "SCCP")) {
				   sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Registered RegContext: %s, Extension: %s, Line: %s\n", context, extension, name);
				   }
				   } */
			} else {
				/* un-register */

				if (SCCP_LIST_GETSIZE(&l->devices) == 1) {			// only remove entry if it is the last one (shared line)
					if (pbx_find_extension(NULL, NULL, &q, context, ext, 1, NULL, "", E_MATCH)) {
						ast_context_remove_extension(context, ext, 1, NULL);
						sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Unregistered RegContext: %s, Extension: %s\n", context, ext);
					}
				}

				/* unregister extension + subscriptionId */
				/* if (subscriptionId && subscriptionId->number && !sccp_strlen_zero(subscriptionId->number) && !sccp_strlen_zero(subscriptionId->name)) {
				   snprintf(extension, sizeof(extension), "%s@%s", ext, subscriptionId->number);
				   // if (pbx_exists_extension(NULL, context, extension, 2, NULL)) {
				   if (pbx_find_extension(NULL, NULL, &q, context, extension, 2, NULL, "", E_MATCH)) {
				   ast_context_remove_extension(context, extension, 2, NULL);
				   sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Unregistered RegContext: %s, Extension: %s\n", context, extension);
				   }
				   } */
			}
		} else {
			pbx_log(LOG_ERROR, "SCCP: context '%s' does not exist and could not be created\n", context);
		}
	}
}

#if UNUSEDCODE // 2015-11-01
/*!
 * \brief check the DND status for single/shared lines
 * On shared line we will return dnd status if all devices in dnd only.
 * single line signaling dnd if device is set to dnd
 */
sccp_channelstate_t sccp_line_getDNDChannelState(sccp_line_t * line)
{
	sccp_linedevices_t *lineDevice = NULL;
	sccp_channelstate_t state = SCCP_CHANNELSTATE_CONGESTION;

	if (!line) {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_getDNDState) Either no hint or line provided\n");
		return state;
	}
	sccp_log((DEBUGCAT_HINT)) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_getDNDState) line: %s\n", line->name);
	if (SCCP_LIST_GETSIZE(&line->devices) > 1) {
		/* we have to check if all devices on this line are dnd=SCCP_DNDMODE_REJECT, otherwise do not propagate DND status */
		boolean_t allDevicesInDND = TRUE;

		SCCP_LIST_LOCK(&line->devices);
		SCCP_LIST_TRAVERSE(&line->devices, lineDevice, list) {
			if (lineDevice->device->dndFeature.status != SCCP_DNDMODE_REJECT) {
				allDevicesInDND = FALSE;
				break;
			}
		}
		SCCP_LIST_UNLOCK(&line->devices);

		if (allDevicesInDND) {
			state = SCCP_CHANNELSTATE_DND;
		}

	} else {
		SCCP_LIST_LOCK(&line->devices);
		lineDevice = SCCP_LIST_FIRST(&line->devices);
		SCCP_LIST_UNLOCK(&line->devices);

		if (lineDevice) {
			if (lineDevice->device->dndFeature.enabled && lineDevice->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				state = SCCP_CHANNELSTATE_DND;
			}
		}
	}													// if(SCCP_LIST_GETSIZE(&line->devices) > 1)
	return state;
}
#endif

/*=================================================================================== FIND FUNCTIONS ==============*/

/*!
 * \brief Find Line by Name
 *
 * \callgraph
 * \callergraph
 * 
 */
sccp_line_t *sccp_line_find_byname(const char *name, uint8_t useRealtime)
{
	sccp_line_t *l = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	l = SCCP_RWLIST_FIND(&GLOB(lines), sccp_line_t, tmpl, list, (sccp_strcaseequals(tmpl->name, name)), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
#ifdef CS_SCCP_REALTIME
	if (!l && useRealtime) {
		l = sccp_line_find_realtime_byname(name);
	}
#endif

	if (!l) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found.\n", name);
		return NULL;
	}
	return l;
}

#ifdef CS_SCCP_REALTIME

/*!
 * \brief Find Line via Realtime
 *
 * \callgraph
 * \callergraph
 */
#if DEBUG
/*!
 * \param name Line Name
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line
 */
sccp_line_t *__sccp_line_find_realtime_byname(const char *name, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Line Name
 * \return SCCP Line
 */
sccp_line_t *sccp_line_find_realtime_byname(const char *name)
#endif
{
	sccp_line_t *l = NULL;
	PBX_VARIABLE_TYPE *v = NULL, *variable = NULL;

	if (sccp_strlen_zero(GLOB(realtimelinetable)) || sccp_strlen_zero(name)) {
		return NULL;
	}

	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for line with name ''\n");
		return NULL;
	}

	if ((variable = pbx_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_LINE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));

		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_4 "SCCP: creating realtime line '%s'\n", name);

		// SCCP_RWLIST_WRLOCK(&GLOB(lines));
		if ((l = sccp_line_create(name))) {								/* already retained */
			sccp_config_applyLineConfiguration(l, variable);
			l->realtime = TRUE;
			sccp_line_addToGlobals(l);								// can return previous instance on doubles
			pbx_variables_destroy(v);
		} else {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime line '%s'\n", name);
		}
		// SCCP_RWLIST_UNLOCK(&GLOB(lines));
		return l;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found in realtime table '%s'\n", name, GLOB(realtimelinetable));
	return NULL;
}
#endif

/*!
 * \brief Find Line by Instance on device
 *
 * \todo No ID Specified only instance, should this function be renamed ?
 *
 * \callgraph
 * \callergraph
 * 
 */
#if DEBUG
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line (can be null)
 */
sccp_line_t *__sccp_line_find_byid(constDevicePtr d, uint16_t instance, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 */
sccp_line_t *sccp_line_find_byid(constDevicePtr d, uint16_t instance)
#endif
{
	sccp_line_t *l = NULL;

	if (!d || instance == 0) {
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);

	if (0 < instance && instance < d->lineButtons.size && d->lineButtons.instance[instance] && d->lineButtons.instance[instance]->line) {
#if DEBUG
		l = sccp_refcount_retain(d->lineButtons.instance[instance]->line, filename, lineno, func);
#else
		l = sccp_line_retain(d->lineButtons.instance[instance]->line);
#endif
	}
	if (!l) {
		sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No line found with instance %d.\n", DEV_ID_LOG(d), instance);
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found line %s\n", "SCCP", l->name);

	return l;
}

/*!
 * \brief Find Line by ButtonIndex on device
 *
 * \todo No ID Specified only instance, should this function be renamed ?
 *
 * \callgraph
 * \callergraph
 * 
 */
#if DEBUG
/*!
 * \param d SCCP Device
 * \param buttonIndex Button Index as uint16_t
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line (can be null)
 */
sccp_line_t *__sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 */
sccp_line_t *sccp_line_find_byButtonIndex(constDevicePtr d, uint16_t buttonIndex)
#endif
{
	sccp_line_t *l = NULL;

	if (!d || buttonIndex == 0) {
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with buttonIndex %d.\n", DEV_ID_LOG(d), buttonIndex);
	
	if (buttonIndex > 0 && buttonIndex < StationMaxButtonTemplateSize && d->buttonTemplate[buttonIndex - 1].type == SKINNY_BUTTONTYPE_LINE && d->buttonTemplate[buttonIndex - 1].ptr ) {
#if DEBUG
		l = sccp_refcount_retain(d->buttonTemplate[buttonIndex - 1].ptr, filename, lineno, func);
#else
		l = sccp_line_retain(d->buttonTemplate[buttonIndex - 1].ptr);
#endif
	}
	if (!l) {
		sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No line found with buttonIndex %d.\n", DEV_ID_LOG(d), buttonIndex);
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found line %s\n", "SCCP", l->name);

	return l;
}

/*!
 * \brief Get Device Configuration
 * \param device SCCP Device
 * \param line SCCP Line
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line Devices
 *
 * \callgraph
 * \callergraph
 * 
 * \warning
 *  - line->devices is not always locked
 */
sccp_linedevices_t *__sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func)
{
	sccp_linedevices_t *linedevice = NULL;
	sccp_line_t *l = NULL;									// loose const qualifier, to be able to lock the list;
	if (!line) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No line provided to search in\n", filename, lineno);
		return NULL;
	}
	l = (sccp_line_t *) line;									// loose const qualifier, to be able to lock the list;
	
	if (!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (line: %s)\n", filename, lineno, line->name);
		return NULL;
	}

	SCCP_LIST_LOCK(&l->devices);
	linedevice = SCCP_LIST_FIND(&l->devices, sccp_linedevices_t, tmplinedevice, list, (device == tmplinedevice->device), TRUE, filename, lineno, func);
	SCCP_LIST_UNLOCK(&l->devices);

	if (!linedevice) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for line %s could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, line->name);
	}
	return linedevice;
}

sccp_linedevices_t *__sccp_linedevice_findByLineinstance(const sccp_device_t * device, uint16_t instance, const char *filename, int lineno, const char *func)
{
	sccp_linedevices_t *linedevice = NULL;

	if (instance < 1) {
		pbx_log(LOG_NOTICE, "%s: [%s:%d]->linedevice_find: No line provided to search in\n", DEV_ID_LOG(device), filename, lineno);
		return NULL;
	}
	if (!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (lineinstance: %d)\n", filename, lineno, instance);
		return NULL;
	}

	if (0 < instance && instance < device->lineButtons.size && device->lineButtons.instance[instance]) {	/* 0 < instance < lineButton.size */
		linedevice = sccp_linedevice_retain(device->lineButtons.instance[instance]);
	}

	if (!linedevice) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for lineinstance %d could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, instance);
	}
	return linedevice;
}

/* create linebutton array */
void sccp_line_createLineButtonsArray(sccp_device_t * device)
{
	sccp_linedevices_t *linedevice = NULL;
	uint8_t lineInstances = 0;
	btnlist *btn = NULL;
	uint8_t i;

	if (device->lineButtons.size) {
		sccp_line_deleteLineButtonsArray(device);
	}

	btn = device->buttonTemplate;

	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].instance > lineInstances && btn[i].ptr) {
			lineInstances = btn[i].instance;
		}
	}

	device->lineButtons.instance = sccp_calloc(lineInstances + SCCP_FIRST_LINEINSTANCE, sizeof(sccp_linedevices_t *));
	if (!device->lineButtons.instance) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, device->id);
		return;
	}
	device->lineButtons.size = lineInstances + SCCP_FIRST_LINEINSTANCE;					/* add the offset of SCCP_FIRST_LINEINSTANCE for explicit access */

	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
			linedevice = sccp_linedevice_find(device, (sccp_line_t *) btn[i].ptr);
			if (!(device->lineButtons.instance[btn[i].instance] = linedevice)) {
				pbx_log(LOG_ERROR, "%s: linedevice could not be found or retained\n", device->id);
				device->lineButtons.size--;
				sccp_free(device->lineButtons.instance);
			}
		}
	}
}

void sccp_line_deleteLineButtonsArray(sccp_device_t * device)
{
	uint8_t i;

	if (device->lineButtons.instance) {
		for (i = SCCP_FIRST_LINEINSTANCE; i < device->lineButtons.size; i++) {
			if (device->lineButtons.instance[i]) {
				sccp_linedevices_t *tmpld = device->lineButtons.instance[i];			/* castless conversion */
				sccp_linedevice_release(&tmpld);						/* explicit release of retained linedevice */
				device->lineButtons.instance[i] = NULL;
			}
		}
		device->lineButtons.size = 0;
		sccp_free(device->lineButtons.instance);
	}
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
