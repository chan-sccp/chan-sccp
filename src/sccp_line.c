
/*!
 * \file        sccp_line.c
 * \brief       SCCP Line
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
static void regcontext_exten(sccp_line_t * l, struct subscriptionId *subscriptionId, int onoff);
int __sccp_line_destroy(const void *ptr);
int __sccp_lineDevice_destroy(const void *ptr);
void sccp_line_delete_nolock(sccp_line_t * l);
int sccp_line_destroy(const void *ptr);

/*!
 * \brief run before reload is start on lines
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 */
void sccp_line_pre_reload(void)
{
	sccp_line_t *l;
	sccp_linedevices_t *linedevice;

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (GLOB(hotline)->line == l) {									/* always remove hotline from linedevice */
			SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->devices, linedevice, list) {
				sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Removing Hotline from Device\n", linedevice->device->id);
				linedevice->device->isAnonymous = FALSE;
				sccp_line_removeDevice(linedevice->line, linedevice->device);
			}
			SCCP_LIST_TRAVERSE_SAFE_END;
		} else {											/* Don't want to include the hotline line */
#ifdef CS_SCCP_REALTIME
			if (l->realtime == FALSE)
#endif
			{
				sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Setting Line to Pending Delete=1\n", l->name);
				l->pendingDelete = 1;
			}
		}
		l->pendingUpdate = 0;
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
}

/*!
 * \brief run after the new line config is loaded during the reload process
 * \note See \ref sccp_config_reload
 * \todo to be implemented correctly (***)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *        - line
 *           - line->devices
 *             - device
 *        - see sccp_line_clean()
 */
void sccp_line_post_reload(void)
{
	sccp_line_t *l;
	sccp_linedevices_t *linedevice;

	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		if (!l->pendingDelete && !l->pendingUpdate)
			continue;

		if ((l = sccp_line_retain(l))) {
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
				linedevice->device->pendingUpdate = 1;
			}
			SCCP_LIST_UNLOCK(&l->devices);

			if (l->pendingDelete) {
				sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Deleting Line (post_reload)\n", l->name);
				sccp_line_clean(l, TRUE);
			} else {
				sccp_log((DEBUGCAT_CONFIG | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Cleaning Line (post_reload)\n", l->name);
				sccp_line_clean(l, FALSE);
			}
			l = sccp_line_release(l);
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
	sccp_line_t *l = (sccp_line_t *) sccp_refcount_object_alloc(sizeof(sccp_line_t), SCCP_REF_LINE, name, __sccp_line_destroy);

	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Unable to allocate memory for a line\n");
		return NULL;
	}
	memset(l, 0, sizeof(sccp_line_t));
	pbx_mutex_init(&l->lock);
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
 * \lock
 *      - lines
 *      - see sccp_mwi_linecreatedEvent() via sccp_event_fire()
 */
void sccp_line_addToGlobals(sccp_line_t * line)
{
	sccp_line_t *l = NULL;

	while (!SCCP_RWLIST_TRYWRLOCK(&GLOB(lines))) {								/* Upgrading to wrlock if possible */
		usleep(100);											/* backoff on failure */
	}
	if ((l = sccp_line_retain(line))) {
		/* add to list */
		l = sccp_line_retain(l);									/* add retained line to the list */
		SCCP_RWLIST_INSERT_SORTALPHA(&GLOB(lines), l, list, cid_num);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Added line '%s' to Glob(lines)\n", l->name);

		/* emit event */
		sccp_event_t event;

		memset(&event, 0, sizeof(sccp_event_t));
		event.type = SCCP_EVENT_LINE_CREATED;
		event.event.lineCreated.line = sccp_line_retain(l);
		sccp_event_fire(&event);

		l = sccp_line_release(l);
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
 * \lock
 *      - lines
 *      - see sccp_mwi_linecreatedEvent() via sccp_event_fire()
 */
sccp_line_t *sccp_line_removeFromGlobals(sccp_line_t * line)
{
	sccp_line_t *removed_line = NULL;

	if (!line) {
		pbx_log(LOG_ERROR, "Removing null from global line list is not allowed!\n");
		return NULL;
	}
	//      SCCP_RWLIST_WRLOCK(&GLOB(lines));
	while (!SCCP_RWLIST_TRYWRLOCK(&GLOB(lines))) {								/* Upgrading to wrlock if possible */
		usleep(100);											/* backoff on failure */
	}
	removed_line = SCCP_RWLIST_REMOVE(&GLOB(lines), line, list);
	SCCP_RWLIST_UNLOCK(&GLOB(lines));
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Removed line '%s' from Glob(lines)\n", removed_line->name);

	/* not sure if we should fire an event like this ? */
	/*      
	   sccp_event_t event;
	   memset(&event, 0, sizeof(sccp_event_t));
	   event.type = SCCP_EVENT_LINE_DELETED;
	   event.event.lineCreated.line = sccp_line_retain(line);
	   sccp_event_fire(&event);
	 */
	if (removed_line) {
		sccp_line_release(removed_line);
	}

	return removed_line;
}

/*!
 * \brief       create a hotline
 * 
 * \lock
 *      - lines
 */
void *sccp_create_hotline(void)
{
	sccp_line_t *hotline;

	GLOB(hotline) = (sccp_hotline_t *) sccp_malloc(sizeof(sccp_hotline_t));
	if (!GLOB(hotline)) {
		pbx_log(LOG_ERROR, "Error allocating memory for GLOB(hotline)");
		return FALSE;
	}
	memset(GLOB(hotline), 0, sizeof(sccp_hotline_t));

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	hotline = sccp_line_create("Hotline");
#ifdef CS_SCCP_REALTIME
	hotline->realtime = TRUE;
#endif
	if (hotline) {
		sccp_copy_string(hotline->cid_name, "hotline", sizeof(hotline->cid_name));
		sccp_copy_string(hotline->cid_num, "hotline", sizeof(hotline->cid_name));
		sccp_copy_string(hotline->context, "default", sizeof(hotline->context));
		sccp_copy_string(hotline->label, "hotline", sizeof(hotline->label));
		sccp_copy_string(hotline->adhocNumber, "111", sizeof(hotline->adhocNumber));

		//sccp_copy_string(hotline->mailbox, "hotline", sizeof(hotline->mailbox));
		sccp_copy_string(GLOB(hotline)->exten, "111", sizeof(GLOB(hotline)->exten));

		GLOB(hotline)->line = sccp_line_retain(hotline);
		sccp_line_addToGlobals(hotline);								// can return previous line on doubles
		sccp_line_release(hotline);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	return NULL;
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
 *      - line->channels
 *        - see sccp_channel_endcall();
 */
void sccp_line_kill_channels(sccp_line_t * l)
{
	sccp_channel_t *c;

	if (!l)
		return;

	//      SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->channels, c, list) {
		if ((sccp_channel_retain(c))) {
			sccp_channel_endcall(c);
			sccp_channel_release(c);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	//      SCCP_LIST_UNLOCK(&l->channels);
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
 * \todo integrate sccp_line_clean and sccp_line_delete_nolock into sccp_line_delete
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 *      - see sccp_line_kill_channels()
 *      - line->devices
 *      - see sccp_line_destroy()
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
 * \lock
 *      - line
 *        - see sccp_mwi_unsubscribeMailbox()
 */
int __sccp_line_destroy(const void *ptr)
{
	sccp_line_t *l = (sccp_line_t *) ptr;

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: Line FREE\n", l->name);

	sccp_mutex_lock(&l->lock);

	// cleanup linedevices
	sccp_line_removeDevice(l, NULL);
	if (SCCP_LIST_EMPTY(&l->devices))
		SCCP_LIST_HEAD_DESTROY(&l->devices);

	// cleanup mailboxes
	if (l->trnsfvm) {
		sccp_free(l->trnsfvm);
	}
	sccp_mailbox_t *mailbox = NULL;

	SCCP_LIST_LOCK(&l->mailboxes);
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
	SCCP_LIST_UNLOCK(&l->mailboxes);
	if (SCCP_LIST_EMPTY(&l->mailboxes)) {
		SCCP_LIST_HEAD_DESTROY(&l->mailboxes);
	}
	// cleanup channels
	sccp_channel_t *c = NULL;

	SCCP_LIST_LOCK(&l->channels);
	while ((c = SCCP_LIST_REMOVE_HEAD(&l->channels, list))) {
		sccp_channel_endcall(c);
		sccp_channel_release(c);									// release channel retain in list
	}
	SCCP_LIST_UNLOCK(&l->channels);
	if (SCCP_LIST_EMPTY(&l->channels)) {
		SCCP_LIST_HEAD_DESTROY(&l->channels);
	}
	// cleanup variables
	if (l->variables) {
		pbx_variables_destroy(l->variables);
		l->variables = NULL;
	}

	sccp_mutex_unlock(&l->lock);
	pbx_mutex_destroy(&l->lock);
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
 * \lock
 *      - line
 *        - see sccp_mwi_unsubscribeMailbox()
 */
int __sccp_lineDevice_destroy(const void *ptr)
{
	sccp_linedevices_t *linedevice = (sccp_linedevices_t *) ptr;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: LineDevice FREE %p\n", DEV_ID_LOG(linedevice->device), linedevice);
	if (linedevice->line)
		linedevice->line = sccp_line_release(linedevice->line);
	if (linedevice->device)
		linedevice->device = sccp_device_release(linedevice->device);
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
 * \lock
 *      - line
 *        - see sccp_mwi_unsubscribeMailbox()
 */
int sccp_line_destroy(const void *ptr)
{
	sccp_line_t *l = (sccp_line_t *) ptr;

	sccp_line_removeFromGlobals(l);										// final release
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
 * \param line SCCP Line
 * \param device device that requested the forward
 * \param type Call Forward Type as uint8_t
 * \param number Number to which should be forwarded
 * \todo we should check, that extension is reachable on line
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - line->devices
 *      - see sccp_feat_changed()
 *
 * \todo implement cfwd_noanswer
 */
void sccp_line_cfwd(sccp_line_t * line, sccp_device_t * device, sccp_callforward_t type, char *number)
{
	sccp_linedevices_t *linedevice = NULL;

	if (!line || !device)
		return;

	if ((linedevice = sccp_linedevice_find(device, line))) {
		if (type == SCCP_CFWD_NONE) {
			linedevice->cfwdAll.enabled = 0;
			linedevice->cfwdBusy.enabled = 0;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", DEV_ID_LOG(device), line->name);
		} else {
			if (!number || sccp_strlen_zero(number)) {
				linedevice->cfwdAll.enabled = 0;
				linedevice->cfwdBusy.enabled = 0;
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid\n", DEV_ID_LOG(device));
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
				sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: Call Forward enabled on line %s to number %s\n", DEV_ID_LOG(device), line->name, number);
			}
		}
		sccp_dev_starttone(linedevice->device, SKINNY_TONE_ZIPZIP, 0, 0, 0);
		sccp_feat_changed(linedevice->device, linedevice, SCCP_FEATURE_CFWDALL);
		sccp_dev_forward_status(linedevice->line, linedevice->lineInstance, device);
		linedevice = sccp_linedevice_release(linedevice);
	} else {
		pbx_log(LOG_ERROR, "%s: Device does not have line configured (linedevice not found)\n", DEV_ID_LOG(device));
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
 *      - line->devices
 *        - see register_exten()
 *      - line
 *      - see sccp_feat_changed()
 *      - see sccp_dev_forward_status() via sccp_event_fire()
 *      - see sccp_mwi_deviceAttachedEvent() via sccp_event_fire
 */
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t * device, uint8_t lineInstance, struct subscriptionId *subscriptionId)
{
	sccp_linedevices_t *linedevice = NULL;

	if (!device || !l) {
		pbx_log(LOG_ERROR, "SCCP: sccp_line_addDevice: No line or device provided\n");
		return;
	}

	if ((linedevice = sccp_linedevice_find(device, l))) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: device already registered for line '%s'\n", DEV_ID_LOG(device), l->name);
		sccp_linedevice_release(linedevice);
		// early exit
		return;
	}

	if (!(device = sccp_device_retain(device))) {
		pbx_log(LOG_ERROR, "SCCP: sccp_line_addDevice: Device could not be retained for line : %s\n", l ? l->name : "UNDEF");
		return;
	}

	if (!(l = sccp_line_retain(l))) {
		pbx_log(LOG_ERROR, "%s: sccp_line_addDevice: Line could not be retained\n", DEV_ID_LOG(device));
		device = sccp_device_release(device);
		return;
	}
	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: add device to line %s\n", DEV_ID_LOG(device), l->name);

	char ld_id[REFCOUNT_INDENTIFIER_SIZE];

	snprintf(ld_id, REFCOUNT_INDENTIFIER_SIZE, "%s/%s", device->id, l->name);
	linedevice = (sccp_linedevices_t *) sccp_refcount_object_alloc(sizeof(sccp_linedevices_t), SCCP_REF_LINEDEVICE, ld_id, __sccp_lineDevice_destroy);
	memset(linedevice, 0, sizeof(sccp_linedevices_t));

	linedevice->device = sccp_device_retain(device);
	linedevice->line = sccp_line_retain(l);
	linedevice->lineInstance = lineInstance;

	if (NULL != subscriptionId) {
		sccp_copy_string(linedevice->subscriptionId.name, subscriptionId->name, sizeof(linedevice->subscriptionId.name));
		sccp_copy_string(linedevice->subscriptionId.number, subscriptionId->number, sizeof(linedevice->subscriptionId.number));
		sccp_copy_string(linedevice->subscriptionId.aux, subscriptionId->aux, sizeof(linedevice->subscriptionId.aux));
	}

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_INSERT_HEAD(&l->devices, linedevice, list);
	SCCP_LIST_UNLOCK(&l->devices);

	linedevice->line->statistic.numberOfActiveDevices++;
	linedevice->device->configurationStatistic.numberOfLines++;
	
	
// 	/* rebuild line button pointer array*/
// 	if( device->lineButtons.size < lineInstance){
// 		sccp_linedevices_t **instance = device->lineButtons.instance;
// 		uint8_t oldSize = device->lineButtons.size;
// 		
//   
// 		device->lineButtons.size = lineInstance + 1;									/* use one more, so we can explicit call this position */
// 		device->lineButtons.instance = calloc(device->lineButtons.size, sizeof(sccp_linedevices_t *) );
// 		memset(device->lineButtons.instance, 0, device->lineButtons.size * sizeof(sccp_linedevices_t *) );
// 		
// 		if(oldSize > 0){
// 			uint8_t i;
// 			for(i=0; i < oldSize;i++){
// 				device->lineButtons.instance[i] = instance[i];
// 			}
// 		  
// 			sccp_free(instance);
// 		}
// 	}
// 	device->lineButtons.instance[ lineInstance ] = sccp_linedevice_retain( linedevice );


	/* read cfwd status from db */
#ifndef ASTDB_FAMILY_KEY_LEN
#define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#define ASTDB_RESULT_LEN 80
#endif
	char family[ASTDB_FAMILY_KEY_LEN];
	char buffer[ASTDB_RESULT_LEN];

	memset(family, 0, ASTDB_FAMILY_KEY_LEN);
	sprintf(family, "SCCP/%s/%s", device->id, l->name);
	if (PBX(feature_getFromDatabase) (family, "cfwdAll", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
		linedevice->cfwdAll.enabled = TRUE;
		sccp_copy_string(linedevice->cfwdAll.number, buffer, sizeof(linedevice->cfwdAll.number));
		sccp_feat_changed(device, linedevice, SCCP_FEATURE_CFWDALL);
	}

	if (PBX(feature_getFromDatabase) (family, "cfwdBusy", buffer, sizeof(buffer)) && strcmp(buffer, "")) {
		linedevice->cfwdBusy.enabled = TRUE;
		sccp_copy_string(linedevice->cfwdBusy.number, buffer, sizeof(linedevice->cfwdAll.number));
		sccp_feat_changed(device, linedevice, SCCP_FEATURE_CFWDBUSY);
	}

	if (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) {
		sccp_dev_forward_status(l, lineInstance, device);
	}
	// fire event for new device
	sccp_event_t event;

	memset(&event, 0, sizeof(sccp_event_t));
	event.type = SCCP_EVENT_DEVICE_ATTACHED;
	event.event.deviceAttached.linedevice = sccp_linedevice_retain(linedevice);
	sccp_event_fire(&event);

	regcontext_exten(l, &(linedevice->subscriptionId), 1);
	sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: added linedevice: %p with device: %s\n", l->name, linedevice, DEV_ID_LOG(device));
	l = sccp_line_release(l);
	device = sccp_device_release(device);
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
 * \lock
 *      - line
 *        - line->devices
 *          - see unregister_exten()
 *      - see sccp_hint_eventListener() via sccp_event_fire()
 */
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t * device)
{
	sccp_linedevices_t *linedevice;

	if (!l) {
		return;
	}
	sccp_log((DEBUGCAT_HIGH + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: remove device from line %s\n", DEV_ID_LOG(device), l->name);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->devices, linedevice, list) {
		if (device == NULL || linedevice->device == device) {
			regcontext_exten(l, &(linedevice->subscriptionId), 0);
			SCCP_LIST_REMOVE_CURRENT(list);
			l->statistic.numberOfActiveDevices--;

			sccp_event_t event;

			memset(&event, 0, sizeof(sccp_event_t));

			event.type = SCCP_EVENT_DEVICE_DETACHED;
			event.event.deviceAttached.linedevice = sccp_linedevice_retain(linedevice);
			sccp_event_fire(&event);

			sccp_linedevice_release(linedevice);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&l->devices);
}

/*!
 * \brief Add a Channel to a Line
 *
 * \param l SCCP Line
 * \param channel SCCP Channel
 * 
 * \warning
 *      - line->channels is not always locked
 * 
 * \lock
 *      - line
 */
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t * channel)
{
	if (!l || !channel)
		return;

	if ((l = sccp_line_retain(l))) {
		l->statistic.numberOfActiveChannels++;
		SCCP_LIST_LOCK(&l->channels);
		if ((channel = sccp_channel_retain(channel))) {							// Add into list retained
			sccp_channel_updateChannelDesignator(channel);
			sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_1 "SCCP: Adding channel %d to line %s\n", channel->callid, l->name);
			if (GLOB(callanswerorder) == ANSWER_OLDEST_FIRST)
				SCCP_LIST_INSERT_TAIL(&l->channels, channel, list);
			else
				SCCP_LIST_INSERT_HEAD(&l->channels, channel, list);
		}
		SCCP_LIST_UNLOCK(&l->channels);
		l = sccp_line_release(l);
	}
}

/*!
 * \brief Remove a Channel from a Line
 *
 * \param l SCCP Line
 * \param c SCCP Channel
 * 
 * \warning
 *      - line->channels is not always locked
 * 
 * \lock
 *      - line
 */
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t * c)
{
	sccp_channel_t *channel;

	if (!l || !c)
		return;

	if ((l = sccp_line_retain(l))) {
		SCCP_LIST_LOCK(&l->channels);
		if ((channel = SCCP_LIST_REMOVE(&l->channels, c, list))) {
			sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_1 "SCCP: Removing channel %d from line %s\n", channel->callid, l->name);
			l->statistic.numberOfActiveChannels--;
			channel = sccp_channel_release(channel);						// Remove retain from list
		}
		SCCP_LIST_UNLOCK(&l->channels);
		l = sccp_line_release(l);
	}
}

/*!
 * \brief Register Extension to Asterisk regextension
 * \param l SCCP Line
 * \param subscriptionId subscriptionId
 * \param onoff On/Off as int
 * \note used for DUNDi Discovery \ref DUNDi
 */
static void regcontext_exten(sccp_line_t * l, struct subscriptionId *subscriptionId, int onoff)
{
	char multi[256] = "";
	char *stringp, *ext = "", *context = "";

	//      char extension[AST_MAX_CONTEXT]="";
	//      char name[AST_MAX_CONTEXT]="";

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
			context = GLOB(regcontext);
		}
		con = pbx_context_find_or_create(NULL, NULL, context, "SCCP");					/* make sure the context exists */
		if (con) {
			if (onoff) {
				/* register */

				if (!pbx_exists_extension(NULL, context, ext, 1, NULL) && pbx_add_extension(context, 0, ext, 1, NULL, NULL, "Noop", sccp_strdup(l->name), sccp_free_ptr, "SCCP")) {
					sccp_log((DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Registered RegContext: %s, Extension: %s, Line: %s\n", context, ext, l->name);
				}

				/* register extension + subscriptionId */
				/* if (subscriptionId && subscriptionId->number && !sccp_strlen_zero(subscriptionId->number) && !sccp_strlen_zero(subscriptionId->name)) {
				   snprintf(extension, sizeof(extension), "%s@%s", ext, subscriptionId->number);
				   snprintf(name, sizeof(name), "%s%s", l->name, subscriptionId->name);
				   if (!pbx_exists_extension(NULL, context, extension, 2, NULL) && pbx_add_extension(context, 0, extension, 2, NULL, NULL, "Noop", sccp_strdup(name), sccp_free_ptr, "SCCP")) {
				   sccp_log((DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Registered RegContext: %s, Extension: %s, Line: %s\n", context, extension, name);
				   }
				   } */
			} else {
				/* un-register */

				if (l->devices.size == 1) {							// only remove entry if it is the last one (shared line)
					if (pbx_find_extension(NULL, NULL, &q, context, ext, 1, NULL, "", E_MATCH)) {
						ast_context_remove_extension(context, ext, 1, NULL);
						sccp_log((DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Unregistered RegContext: %s, Extension: %s\n", context, ext);
					}
				}

				/* unregister extension + subscriptionId */
				/* if (subscriptionId && subscriptionId->number && !sccp_strlen_zero(subscriptionId->number) && !sccp_strlen_zero(subscriptionId->name)) {
				   snprintf(extension, sizeof(extension), "%s@%s", ext, subscriptionId->number);
				   // if (pbx_exists_extension(NULL, context, extension, 2, NULL)) {
				   if (pbx_find_extension(NULL, NULL, &q, context, extension, 2, NULL, "", E_MATCH)) {
				   ast_context_remove_extension(context, extension, 2, NULL);
				   sccp_log((DEBUGCAT_LINE | DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "Unregistered RegContext: %s, Extension: %s\n", context, extension);
				   }
				   } */
			}
		} else {
			pbx_log(LOG_ERROR, "SCCP: context '%s' does not exist and could not be created\n", context);
		}
	}
}

/*!
 * \brief check the DND status for single/shared lines
 * On shared line we will return dnd status if all devices in dnd only.
 * single line signaling dnd if device is set to dnd
 * 
 * \param line  SCCP Line (locked)
 */
sccp_channelstate_t sccp_line_getDNDChannelState(sccp_line_t * line)
{
	sccp_linedevices_t *lineDevice = NULL;
	sccp_channelstate_t state = SCCP_CHANNELSTATE_CONGESTION;

	if (!line) {
		pbx_log(LOG_WARNING, "SCCP: (sccp_hint_getDNDState) Either no hint or line provided\n");
		return state;
	}
	sccp_log(DEBUGCAT_HINT) (VERBOSE_PREFIX_4 "SCCP: (sccp_hint_getDNDState) line: %s\n", line->name);
	if (line->devices.size > 1) {
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
		sccp_linedevices_t *lineDevice = SCCP_LIST_FIRST(&line->devices);

		if (lineDevice) {
			if (lineDevice->device->dndFeature.enabled && lineDevice->device->dndFeature.status == SCCP_DNDMODE_REJECT) {
				state = SCCP_CHANNELSTATE_DND;
			}
		}
	}													// if(line->devices.size > 1)
	return state;
}

/*=================================================================================== FIND FUNCTIONS ==============*/

/*!
 * \brief Find Line by Name
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 *      - lines
 */
#if DEBUG
/*!
 * \param name Line Name
 * \param useRealtime Use Realtime as Boolean
 * \param filename Debug FileName
 * \param lineno Debug LineNumber
 * \param func Debug Function Name
 * \return SCCP Line
 */
sccp_line_t *__sccp_line_find_byname(const char *name, uint8_t useRealtime, const char *filename, int lineno, const char *func)
#else
/*!
 * \param name Line Name
 * \param useRealtime Use Realtime as Boolean
 * \return SCCP Line
 */
sccp_line_t *sccp_line_find_byname(const char *name, uint8_t useRealtime)
#endif
{
	sccp_line_t *l = NULL;

	//      sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "SCCP: Looking for line '%s'\n", name);
	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for line with name ''\n");
		return NULL;
	}

	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), l, list) {
		if (l && l->name && !strcasecmp(l->name, name)) {
#if DEBUG
			l = sccp_refcount_retain(l, filename, lineno, func);
#else
			l = sccp_line_retain(l);
#endif
			break;
		}
	}

#ifdef CS_SCCP_REALTIME
	if (!l && useRealtime) {
		l = sccp_line_find_realtime_byname(name);							/* already retained */
	}
#endif
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	if (!l) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found.\n", name);
		return NULL;
	}
	//      sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Found line '%s'\n", "SCCP", l->name);

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
	PBX_VARIABLE_TYPE *v, *variable;

	if (sccp_strlen_zero(GLOB(realtimelinetable)) || sccp_strlen_zero(name)) {
		return NULL;
	}

	if (sccp_strlen_zero(name)) {
		sccp_log((DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for line with name ''\n");
		return NULL;
	}

	if ((variable = pbx_load_realtime(GLOB(realtimelinetable), "name", name, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' found in realtime table '%s'\n", name, GLOB(realtimelinetable));

		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_4 "SCCP: creating realtime line '%s'\n", name);

		l = sccp_line_create(name);									/* already retained */
		sccp_config_applyLineConfiguration(l, variable);
		l->realtime = TRUE;
		sccp_line_addToGlobals(l);									// can return previous instance on doubles
		pbx_variables_destroy(v);

		if (!l) {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime line '%s'\n", name);
		}
		return l;
	}

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: Line '%s' not found in realtime table '%s'\n", name, GLOB(realtimelinetable));
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
 * \lock
 *      - device->buttonconfig
 *        - see sccp_line_find_byname_wo()
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
sccp_line_t *__sccp_line_find_byid(sccp_device_t * d, uint16_t instance, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 */
sccp_line_t *sccp_line_find_byid(sccp_device_t * d, uint16_t instance)
#endif
{
	sccp_line_t *l = NULL;
	

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);

	if (!d || instance == 0)
		return NULL;
#if 0
	sccp_buttonconfig_t *config;
	SCCP_LIST_LOCK(&d->buttonconfig);
	SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE | DEBUGCAT_BUTTONTEMPLATE)) (VERBOSE_PREFIX_3 "%s: button instance %d, type: %d\n", DEV_ID_LOG(d), config->instance, config->type);

		if (config && config->type == LINE && config->instance == instance && !sccp_strlen_zero(config->button.line.name)) {
#if DEBUG
			l = __sccp_line_find_byname(config->button.line.name, TRUE, filename, lineno, func);
#else
			l = sccp_line_find_byname(config->button.line.name, TRUE);
#endif
			break;
		}
	}
	SCCP_LIST_UNLOCK(&d->buttonconfig);
#else
	if (0 < instance && instance < d->lineButtons.size && d->lineButtons.instance[instance] && d->lineButtons.instance[instance]->line ){
		l = sccp_line_retain( d->lineButtons.instance[instance]->line );
	}
#endif
	


	if (!l) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: No line found with instance %d.\n", DEV_ID_LOG(d), instance);
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Found line %s\n", "SCCP", l->name);

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
 *      - line->devices is not always locked
 */
sccp_linedevices_t *__sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func)
{
	if (!line) {
		pbx_log(LOG_NOTICE, "%s: [%s:%d]->linedevice_find: No line provided to search in\n", DEV_ID_LOG(device), filename, lineno);
		return NULL;
	}
	if (!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (line: %s)\n", filename, lineno, line ? line->name : "UNDEF");
		return NULL;
	}

	sccp_linedevices_t *linedevice = NULL;
	sccp_linedevices_t *ld = NULL;

	SCCP_LIST_LOCK(&((sccp_line_t *) line)->devices);
	SCCP_LIST_TRAVERSE(&((sccp_line_t *) line)->devices, linedevice, list) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "linedevice %p for device %s line %s\n", linedevice, DEV_ID_LOG(linedevice->device), linedevice->line->name);
		if (device == linedevice->device) {
			ld = sccp_linedevice_retain(linedevice);
			sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: found linedevice for line %s. Returning linedevice %p\n", DEV_ID_LOG(device), ld->line->name, ld);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&((sccp_line_t *) line)->devices);

	if (!ld) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for line %s could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, line->name);
	}
	return ld;
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
	
	if (0 < instance && instance < device->lineButtons.size && device->lineButtons.instance[instance]) {		/* 0 < instance < lineButton.size */
		linedevice = sccp_linedevice_retain( device->lineButtons.instance[ instance ] ) ;
	}

	if (!linedevice) {
		sccp_log(DEBUGCAT_LINE) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for lineinstance %d could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, instance);
	}
	return linedevice;
}



/* create linebutton array */
void sccp_line_createLineButtonsArray(sccp_device_t *device) {
	sccp_linedevices_t *linedevice;
	uint8_t lineInstances = 0;
	btnlist *btn;
	uint8_t i;
	
	if(device->lineButtons.instance){
		sccp_line_deleteLineButtonsArray(device);
	}
	
	btn = device->buttonTemplate;
	
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].instance > lineInstances &&  btn[i].ptr) {
			lineInstances = btn[i].instance;
		} 
	}
	
	
	device->lineButtons.size = lineInstances + SCCP_FIRST_LINEINSTANCE;					/* add the offset of SCCP_FIRST_LINEINSTANCE for explicit access */
	device->lineButtons.instance = sccp_calloc(device->lineButtons.size, sizeof(sccp_line_t *) );
	
	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE  && btn[i].ptr ) {
			linedevice = sccp_linedevice_find(device, (sccp_line_t *)btn[i].ptr );
			device->lineButtons.instance[ btn[i].instance ] = linedevice;
		} 
	}
}

void sccp_line_deleteLineButtonsArray(sccp_device_t *device) {
	uint8_t i;
  
	if(device->lineButtons.instance){
		for (i = SCCP_FIRST_LINEINSTANCE; i < device->lineButtons.size; i++) {
			if(device->lineButtons.instance[i]){ 
				sccp_linedevice_release(device->lineButtons.instance[i]);
			}
		}
		sccp_free(device->lineButtons.instance);
		device->lineButtons.size = 0;
	}
}
