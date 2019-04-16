/*!
 * \file        sccp_linedevice.c
 * \brief       SCCP LineDevice
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_linedevice.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_utils.h"

// forward declarations

// implementations
/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 * 
 */
static int __sccp_lineDevice_destroy(const void *ptr)
{
	sccp_linedevice_t *linedevice = (sccp_linedevice_t *) ptr;

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: LineDevice FREE %p\n", DEV_ID_LOG(linedevice->device), linedevice);
	if (linedevice->line) {
		sccp_line_release(&linedevice->line);						/* explicit release of line retained in linedevice */
	}
	if (linedevice->device) {
		sccp_device_release(&linedevice->device);					/* explicit release of device retained in linedevice */
	}
	return 0;
}

static void resetPickup(lineDevicePtr ld)
{
	ld->isPickupAllowed = &sccp_always_false;
#ifdef CS_SCCP_PICKUP
	if (ld->line->pickupgroup
#ifdef CS_AST_HAS_NAMEDGROUP
		|| !sccp_strlen_zero(ld->line->namedpickupgroup)
#endif
	) {
		sccp_log(DEBUGCAT_LINE)("%s: (allowPickup) on line:%s.\n", ld->device->id, ld->line->name);
		ld->isPickupAllowed = &sccp_always_true;
	}
#endif
}

static void disallowPickup(lineDevicePtr ld)
{
	sccp_log(DEBUGCAT_LINE)("%s: (disallowPickup) on line:%s.\n", ld->device->id, ld->line->name);
	ld->isPickupAllowed = &sccp_always_false;
}

static const sccp_cfwd_information_t * const getCallForward(constLineDevicePtr ld, sccp_callforward_t type)
{
	if (!ld) {
		return NULL;
	}
	const sccp_cfwd_information_t * res = NULL;
	switch(type) {
		case SCCP_CFWD_ALL:
			res = &ld->cfwdAll;
		case SCCP_CFWD_BUSY:
			res = &ld->cfwdBusy;
		case SCCP_CFWD_NONE:
		case SCCP_CFWD_NOANSWER:
		case SCCP_CALLFORWARD_SENTINEL:
			res = NULL;
	}
	return (const sccp_cfwd_information_t * const)res;
}

static void setCallForward(lineDevicePtr ld, sccp_callforward_t type, sccp_cfwd_information_t *value)
{
	if (!ld) {
		return;
	}
	sccp_cfwd_information_t *entry = NULL;

	switch(type) {
		case SCCP_CFWD_ALL:
			entry = &ld->cfwdAll;
			break;
		case SCCP_CFWD_BUSY:
			entry = &ld->cfwdBusy;
			break;
		case SCCP_CFWD_NONE:
		case SCCP_CFWD_NOANSWER:
		case SCCP_CALLFORWARD_SENTINEL:
			entry = NULL;
	}
	if (entry) {
		entry->enabled = value->enabled;
		sccp_copy_string(entry->number, value->number, sizeof(*entry->number));
	}
}
/*!
 * \brief Attach a Device to a line
 * \param line SCCP Line
 * \param d SCCP Device
 * \param lineInstance lineInstance as uint8_t
 * \param subscriptionId Subscription ID for addressing individual devices on the line
 * 
 */
void sccp_linedevice_allocate(constDevicePtr d, constLinePtr line, uint8_t lineInstance, sccp_subscription_id_t *subscriptionId)
{
	AUTO_RELEASE(sccp_line_t, l , sccp_line_retain(line));
	AUTO_RELEASE(sccp_device_t, device , sccp_device_retain(d));

	if (!device || !l) {
		pbx_log(LOG_ERROR, "SCCP: sccp_line_addDevice: No line or device provided\n");
		return;
	}
	sccp_linedevice_t *linedevice = NULL;

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
	linedevice = (sccp_linedevice_t *) sccp_refcount_object_alloc(sizeof(sccp_linedevice_t), SCCP_REF_LINEDEVICE, ld_id, __sccp_lineDevice_destroy);
	if (!linedevice) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, ld_id);
		return;
	}
	memset(linedevice, 0, sizeof(sccp_linedevice_t));
#if CS_REFCOUNT_DEBUG
	sccp_refcount_addWeakParent(linedevice, l);
	sccp_refcount_addWeakParent(linedevice, device);
#endif
	linedevice->device = sccp_device_retain(device);
	linedevice->line = sccp_line_retain(l);
	linedevice->lineInstance = lineInstance;

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
	
	linedevice->isPickupAllowed = &sccp_always_false;
	linedevice->resetPickup = &resetPickup;
	linedevice->disallowPickup = &disallowPickup;
	linedevice->getCallForward = &getCallForward;
	linedevice->setCallForward = &setCallForward;

	sccp_line_updatePreferencesFromDevicesToLine(l);

	// fire event for new device
	sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_DEVICE_ATTACHED);
	if (event) {
		event->deviceAttached.linedevice = sccp_linedevice_retain(linedevice);
		sccp_event_fire(event);
	}
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
void sccp_linedevice_destroy(constDevicePtr device, linePtr l)
{
	sccp_linedevice_t *linedevice = NULL;

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
			sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_DEVICE_DETACHED);
			if (event) {
				event->deviceAttached.linedevice = sccp_linedevice_retain(linedevice);
				sccp_event_fire(event);
			}
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

	sccp_line_updatePreferencesFromDevicesToLine(l);
	sccp_line_updateCapabilitiesFromDevicesToLine(l);
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
void sccp_line_cfwd(lineDevicePtr ld, sccp_callforward_t type, char *number)
{
	sccp_feature_type_t feature_type = SCCP_FEATURE_CFWDNONE;
	if (!ld) {
		return;
	}
	if (type == SCCP_CFWD_NONE) {
		if (ld->cfwdAll.enabled) {
			feature_type = SCCP_FEATURE_CFWDALL;
		}
		if (ld->cfwdBusy.enabled) {
			feature_type = SCCP_FEATURE_CFWDBUSY;
		}
		ld->cfwdAll.enabled = 0;
		ld->cfwdBusy.enabled = 0;
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", DEV_ID_LOG(ld->device), ld->line->name);
	} else {
		if (!number || sccp_strlen_zero(number)) {
			ld->cfwdAll.enabled = 0;
			ld->cfwdBusy.enabled = 0;
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid. Cfwd Disabled\n", DEV_ID_LOG(ld->device));
		} else {
			switch (type) {
				case SCCP_CFWD_ALL:
					feature_type = SCCP_FEATURE_CFWDALL;
					ld->cfwdAll.enabled = 1;
					sccp_copy_string(ld->cfwdAll.number, number, sizeof(ld->cfwdAll.number));
					break;
				case SCCP_CFWD_BUSY:
					feature_type = SCCP_FEATURE_CFWDBUSY;
					ld->cfwdBusy.enabled = 1;
					sccp_copy_string(ld->cfwdBusy.number, number, sizeof(ld->cfwdBusy.number));
					break;
				default:
					ld->cfwdAll.enabled = 0;
					ld->cfwdBusy.enabled = 0;
			}
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Call Forward %s enabled on line %s to number %s\n", DEV_ID_LOG(ld->device), sccp_callforward2str(type), ld->line->name, number);
		}
	}
	sccp_feat_changed(ld->device, ld, feature_type);
	sccp_dev_forward_status(ld);
}


/* create linebutton array */
void sccp_linedevice_createLineButtonsArray(devicePtr device)
{
	sccp_linedevice_t *linedevice = NULL;
	uint8_t lineInstances = 0;
	btnlist *btn = NULL;
	uint8_t i;

	if (device->lineButtons.size) {
		sccp_linedevice_deleteLineButtonsArray(device);
	}

	btn = device->buttonTemplate;

	for (i = 0; i < StationMaxButtonTemplateSize; i++) {
		if (btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].instance > lineInstances && btn[i].ptr) {
			lineInstances = btn[i].instance;
		}
	}

	device->lineButtons.instance = (sccp_linedevice_t **)sccp_calloc(lineInstances + SCCP_FIRST_LINEINSTANCE, sizeof(sccp_linedevice_t *));
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

void sccp_linedevice_deleteLineButtonsArray(devicePtr device)
{
	uint8_t i;

	if (device->lineButtons.instance) {
		for (i = SCCP_FIRST_LINEINSTANCE; i < device->lineButtons.size; i++) {
			if (device->lineButtons.instance[i]) {
				sccp_linedevice_t *tmpld = device->lineButtons.instance[i];			/* castless conversion */
				sccp_linedevice_release(&tmpld);						/* explicit release of retained linedevice */
				device->lineButtons.instance[i] = NULL;
			}
		}
		device->lineButtons.size = 0;
		sccp_free(device->lineButtons.instance);
	}
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
sccp_linedevice_t *__sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func)
{
	sccp_linedevice_t *linedevice = NULL;
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
	linedevice = SCCP_LIST_FIND(&l->devices, sccp_linedevice_t, tmplinedevice, list, (device == tmplinedevice->device), TRUE, filename, lineno, func);
	SCCP_LIST_UNLOCK(&l->devices);

	if (!linedevice) {
		sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for line %s could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, line->name);
	}
	return linedevice;
}

sccp_linedevice_t *__sccp_linedevice_findByLineInstance(const sccp_device_t * device, uint8_t instance, const char *filename, int lineno, const char *func)
{
	sccp_linedevice_t *linedevice = NULL;

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
		sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: linedevice for lineinstance %d could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, instance);
	}
	return linedevice;
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
