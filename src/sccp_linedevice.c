/*!
 * \file        sccp_linedevice.c
 * \brief       SCCP LineDevice
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
/*
 * File:   sccp_linedevice.c
 * Author: dkgroot
 *
 * Created on September 22, 2019, 4:38 PM
 */

#include "config.h"
#include "common.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_linedevice.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \brief Free a Line as scheduled command
 * \param ptr SCCP Line Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 *
 */
static int __sccp_lineDevice_destroy(const void * ptr)
{
	sccp_linedevice_t * ld = (sccp_linedevice_t *)ptr;

	sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE + DEBUGCAT_CONFIG))(VERBOSE_PREFIX_1 "%s: LineDevice FREE %p\n", DEV_ID_LOG(ld->device), ld);
	if(ld->line) {
		sccp_line_release(&ld->line); /* explicit release of line retained in ld */
	}
	if(ld->device) {
		sccp_device_release(&ld->device); /* explicit release of device retained in ld */
	}
	return 0;
}

/*!
 * \brief Register Extension to Asterisk regextension
 * \param l SCCP Line
 * \param subscriptionId subscriptionId
 * \param onoff On/Off as int
 * \note used for DUNDi Discovery
 */
static void regcontext_exten(constLineDevicePtr ld, int onoff)
{
	char multi[256] = "";
	char * stringp = NULL;

	char * ext = "";

	char cntxt[SCCP_MAX_CONTEXT];
	char * context = cntxt;

	struct pbx_context * con = NULL;
	struct pbx_find_info q = { .stacklen = 0 };

	if(sccp_strlen_zero(GLOB(regcontext))) {
		return;
	}

	if(!ld || !ld->line) {
		return;
	}
	sccp_line_t * l = ld->line;
	// struct subscriptionId *subscriptionId = &(ld->subscriptionId);

	sccp_copy_string(multi, S_OR(l->regexten, l->name), sizeof(multi));
	stringp = multi;
	while((ext = strsep(&stringp, "&"))) {
		if((context = strchr(ext, '@'))) {
			*context++ = '\0'; /* split ext@context */
			if(!pbx_context_find(context)) {
				pbx_log(LOG_WARNING, "Context specified in regcontext=%s (sccp.conf) must exist\n", context);
				continue;
			}
		} else {
			sccp_copy_string(cntxt, GLOB(regcontext), sizeof(cntxt));
			context = cntxt;
		}
		con = pbx_context_find_or_create(NULL, NULL, context, "SCCP"); /* make sure the context exists */
		if(con) {
			if(onoff) {
				/* register */

				if(!pbx_exists_extension(NULL, context, ext, 1, NULL) && pbx_add_extension(context, 0, ext, 1, NULL, NULL, "Noop", pbx_strdup(l->name), sccp_free_ptr, "SCCP")) {
					sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG))(VERBOSE_PREFIX_1 "Registered RegContext: %s, Extension: %s, Line: %s\n", context, ext, l->name);
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

				if(SCCP_LIST_GETSIZE(&l->devices) == 1) {                                        // only remove entry if it is the last one (shared line)
					if(pbx_find_extension(NULL, NULL, &q, context, ext, 1, NULL, "", E_MATCH)) {
						ast_context_remove_extension(context, ext, 1, NULL);
						sccp_log((DEBUGCAT_LINE + DEBUGCAT_CONFIG))(VERBOSE_PREFIX_1 "Unregistered RegContext: %s, Extension: %s\n", context, ext);
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
void sccp_linedevice_cfwd(lineDevicePtr ld, sccp_cfwd_t type, char * number)
{
	if(!ld || !ld->line) {
		return;
	}

	if(type == SCCP_CFWD_NONE) {
		for(uint x = SCCP_CFWD_ALL; x < SCCP_CFWD_SENTINEL; x++) {
			ld->cfwd[x].enabled = FALSE;
			ld->cfwd[x].number[0] = '\0';
		}
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: all Call Forwards have been disabled on line %s\n", DEV_ID_LOG(ld->device), ld->line->name);
	} else {
		if(!number || sccp_strlen_zero(number)) {
			ld->cfwd[type].enabled = FALSE;
			ld->cfwd[type].number[0] = '\0';
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid. Cfwd Disabled\n", DEV_ID_LOG(ld->device));
		} else {
			ld->cfwd[type].enabled = TRUE;
			sccp_copy_string(ld->cfwd[type].number, number, sizeof(ld->cfwd[type].number));
			sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: Call Forward %s enabled on line %s to number %s\n", DEV_ID_LOG(ld->device), sccp_cfwd2str(type), ld->line->name, number);
		}
	}
	sccp_feat_changed(ld->device, ld, sccp_cfwd2feature(type));
	sccp_dev_forward_status(ld->line, ld->lineInstance, ld->device);
}

const char * const sccp_linedevice_get_cfwd_string(constLineDevicePtr ld, char * const buffer, size_t size)
{
	if(!ld) {
		buffer[0] = '\0';
		return NULL;
	}
	snprintf(buffer, size, "All:%s, Busy:%s, NoAnswer:%s", ld->cfwd[SCCP_CFWD_ALL].enabled ? ld->cfwd[SCCP_CFWD_ALL].number : "off", ld->cfwd[SCCP_CFWD_BUSY].enabled ? ld->cfwd[SCCP_CFWD_BUSY].number : "off",
		 ld->cfwd[SCCP_CFWD_NOANSWER].enabled ? ld->cfwd[SCCP_CFWD_NOANSWER].number : "off");
	return buffer;
}

/*!
 * \brief Attach a Device to a line
 * \param line SCCP Line
 * \param d SCCP Device
 * \param lineInstance lineInstance as uint8_t
 * \param subscriptionId Subscription ID for addressing individual devices on the line
 *
 */
void sccp_linedevice_create(constDevicePtr d, constLinePtr l, uint8_t lineInstance, sccp_subscription_id_t * subscriptionId)
{
	AUTO_RELEASE(sccp_line_t, line, sccp_line_retain(l));
	AUTO_RELEASE(sccp_device_t, device, sccp_device_retain(d));

	if(!device || !line) {
		pbx_log(LOG_ERROR, "SCCP: sccp_linedevice_create: No line or device provided\n");
		return;
	}
	sccp_linedevice_t * ld = NULL;

	if((ld = sccp_linedevice_find(device, l))) {
		sccp_log((DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: device already registered for line '%s'\n", DEV_ID_LOG(device), l->name);
		sccp_linedevice_release(&ld); /* explicit release of found ld */
		return;
	}

	sccp_log((DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: add device to line %s\n", DEV_ID_LOG(device), line->name);
#if CS_REFCOUNT_DEBUG
	sccp_refcount_addRelationship(device, line);
#endif
	char ld_id[REFCOUNT_INDENTIFIER_SIZE];

	snprintf(ld_id, REFCOUNT_INDENTIFIER_SIZE, "%s/%s", device->id, line->name);
	ld = (sccp_linedevice_t *)sccp_refcount_object_alloc(sizeof(sccp_linedevice_t), SCCP_REF_LINEDEVICE, ld_id, __sccp_lineDevice_destroy);
	if(!ld) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, ld_id);
		return;
	}
	memset(ld, 0, sizeof *ld);
#if CS_REFCOUNT_DEBUG
	sccp_refcount_addRelationship(l, ld);
	sccp_refcount_addRelationship(device, ld);
#endif
	*(sccp_device_t **)&(ld->device) = sccp_device_retain(device);                                        // const cast to emplace device
	*(sccp_line_t **)&(ld->line) = sccp_line_retain(line);                                                // const cast to emplace line
	ld->lineInstance = lineInstance;
	if(NULL != subscriptionId) {
		memcpy(&ld->subscriptionId, subscriptionId, sizeof(ld->subscriptionId));
	}

	SCCP_LIST_LOCK(&line->devices);
	SCCP_LIST_INSERT_HEAD(&line->devices, ld, list);
	SCCP_LIST_UNLOCK(&line->devices);

	ld->line->statistic.numberOfActiveDevices++;
	ld->device->configurationStatistic.numberOfLines++;

	sccp_line_updatePreferencesFromDevicesToLine(line);

	// fire event for new device
	sccp_event_t * event = sccp_event_allocate(SCCP_EVENT_DEVICE_ATTACHED);
	if(event) {
		event->deviceAttached.ld = sccp_linedevice_retain(ld);
		sccp_event_fire(event);
	}
	regcontext_exten(ld, 1);
	sccp_log((DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: added ld: %p with device: %s\n", line->name, ld, DEV_ID_LOG(device));
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
void sccp_linedevice_remove(constDevicePtr d, linePtr l)
{
	sccp_linedevice_t * ld = NULL;

	if(!l) {
		return;
	}
	sccp_log_and((DEBUGCAT_HIGH + DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: remove device from line %s\n", DEV_ID_LOG(d), l->name);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&l->devices, ld, list) {
		if(d == NULL || ld->device == d) {
#if CS_REFCOUNT_DEBUG
			sccp_refcount_removeRelationship(d ? d : ld->device, l);
#endif
			regcontext_exten(ld, 0);
			SCCP_LIST_REMOVE_CURRENT(list);
			l->statistic.numberOfActiveDevices--;
			sccp_event_t * event = sccp_event_allocate(SCCP_EVENT_DEVICE_DETACHED);
			if(event) {
				event->deviceAttached.ld = sccp_linedevice_retain(ld);
				sccp_event_fire(event);
			}
			sccp_linedevice_release(&ld); /* explicit release of list retained ld */
#ifdef CS_SCCP_REALTIME
			if(l->realtime && SCCP_LIST_GETSIZE(&l->devices) == 0 && SCCP_LIST_GETSIZE(&l->channels) == 0) {
				sccp_line_clean(l, TRUE);
			}
#endif
			if(d)
				break /*early*/;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&l->devices);

	if(GLOB(module_running) == TRUE && d) {
		sccp_line_updatePreferencesFromDevicesToLine(l);
		sccp_line_updateCapabilitiesFromDevicesToLine(l);
	}
}

void sccp_linedevice_indicateMWI(constLineDevicePtr ld)
{
	AUTO_RELEASE(sccp_device_t, d, sccp_device_retain(ld->device));
	AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(ld->line));
	if(l && d) {
		sccp_log((DEBUGCAT_MWI))(VERBOSE_PREFIX_3 "%s: (sccp_line_indicateMWI) Set voicemail lamp:%s on device:%s\n", l->name, l->voicemailStatistic.newmsgs ? "on" : "off", d->id);
		sccp_device_setLamp(d, SKINNY_STIMULUS_VOICEMAIL, ld->lineInstance, l->voicemailStatistic.newmsgs ? d->mwilamp : SKINNY_LAMP_OFF);
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
lineDevicePtr __sccp_linedevice_find(constDevicePtr device, constLinePtr line, const char * filename, int lineno, const char * func)
{
	sccp_linedevice_t * ld = NULL;
	sccp_line_t * l = NULL;                                        // loose const qualifier, to be able to lock the list;
	if(!line) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No line provided to search in\n", filename, lineno);
		return NULL;
	}
	l = (sccp_line_t *)line;                                        // loose const qualifier, to be able to lock the list;

	if(!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (line: %s)\n", filename, lineno, line->name);
		return NULL;
	}

	SCCP_LIST_LOCK(&l->devices);
	ld = SCCP_LIST_FIND(&l->devices, sccp_linedevice_t, tmplinedevice, list, (device == tmplinedevice->device), TRUE, filename, lineno, func);
	SCCP_LIST_UNLOCK(&l->devices);

	if(!ld) {
		sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: ld for line %s could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, line->name);
	}
	return ld;
}

lineDevicePtr __sccp_linedevice_findByLineinstance(constDevicePtr device, uint16_t instance, const char * filename, int lineno, const char * func)
{
	sccp_linedevice_t * ld = NULL;

	if(instance < 1) {
		pbx_log(LOG_NOTICE, "%s: [%s:%d]->linedevice_find: No line provided to search in\n", DEV_ID_LOG(device), filename, lineno);
		return NULL;
	}
	if(!device) {
		pbx_log(LOG_NOTICE, "SCCP: [%s:%d]->linedevice_find: No device provided to search for (lineinstance: %d)\n", filename, lineno, instance);
		return NULL;
	}

	if (instance < device->lineButtons.size && device->lineButtons.instance[instance]) { /* 0 < instance < lineButton.size */
		ld = sccp_linedevice_retain(device->lineButtons.instance[instance]);
	}

	if(!ld) {
		sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_HIGH))(VERBOSE_PREFIX_3 "%s: [%s:%d]->linedevice_find: ld for lineinstance %d could not be found. Returning NULL\n", DEV_ID_LOG(device), filename, lineno, instance);
	}
	return ld;
}

/* create linebutton array */
void sccp_linedevice_createButtonsArray(devicePtr device)
{
	sccp_linedevice_t * ld = NULL;
	uint8_t lineInstances = 0;
	btnlist * btn = NULL;
	uint8_t i = 0;

	if(device->lineButtons.size) {
		sccp_linedevice_deleteButtonsArray(device);
	}

	btn = device->buttonTemplate;

	for(i = 0; i < StationMaxButtonTemplateSize; i++) {
		if(btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].instance > lineInstances && btn[i].ptr) {
			lineInstances = btn[i].instance;
		}
	}

	device->lineButtons.instance = (sccp_linedevice_t **)sccp_calloc(lineInstances + SCCP_FIRST_LINEINSTANCE, sizeof(sccp_linedevice_t *));
	if(!device->lineButtons.instance) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, device->id);
		return;
	}
	device->lineButtons.size = lineInstances + SCCP_FIRST_LINEINSTANCE; /* add the offset of SCCP_FIRST_LINEINSTANCE for explicit access */

	for(i = 0; i < StationMaxButtonTemplateSize; i++) {
		if(btn[i].type == SKINNY_BUTTONTYPE_LINE && btn[i].ptr) {
			ld = sccp_linedevice_find(device, (sccp_line_t *)btn[i].ptr);
			if(!(device->lineButtons.instance[btn[i].instance] = ld)) {
				pbx_log(LOG_ERROR, "%s: ld could not be found or retained\n", device->id);
				device->lineButtons.size--;
				sccp_free(device->lineButtons.instance);
			}
		}
	}
}

void sccp_linedevice_deleteButtonsArray(devicePtr device)
{
	uint8_t i = 0;

	if(device->lineButtons.instance) {
		for(i = SCCP_FIRST_LINEINSTANCE; i < device->lineButtons.size; i++) {
			if(device->lineButtons.instance[i]) {
				sccp_linedevice_t * tmpld = device->lineButtons.instance[i]; /* castless conversion */
				sccp_linedevice_release(&tmpld);                             /* explicit release of retained ld */
				device->lineButtons.instance[i] = NULL;
			}
		}
		device->lineButtons.size = 0;
		sccp_free(device->lineButtons.instance);
	}
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
