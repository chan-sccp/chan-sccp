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
#include "sccp_linedevice.h"
#include "sccp_config.h"
#include "sccp_feature.h"
#include "sccp_mwi.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

int __sccp_line_destroy(const void *ptr);

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
			sccp_linedevice_destroy(NULL, l);
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
			sccp_linedevice_t *linedevice = NULL;
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
		sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_LINEINSTANCE_CREATED);
		if (event) {
			event->lineInstance.line = sccp_line_retain(l);
			sccp_event_fire(event);
		}
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
	sccp_linedevice_destroy(NULL, l);									// removing all devices from this line.
	if (remove_from_global) {
		sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_LINEINSTANCE_DESTROYED);
		if (event) {
			event->lineInstance.line = sccp_line_retain(l);
			sccp_event_fire(event);
		}
		sccp_line_removeFromGlobals(l);									// final release
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
	sccp_linedevice_destroy(NULL, l);

	// cleanup mailboxes (l->mailboxes)
	{
		SCCP_LIST_LOCK(&l->mailboxes);
		while ((mailbox = SCCP_LIST_REMOVE_HEAD(&l->mailboxes, list))) {
			//sccp_mwi_unsubscribeMailbox(mailbox);
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

void sccp_line_copyCodecSetsFromLineToChannel(constLinePtr l, constDevicePtr maybe_d, sccp_channel_t *c)
{
	if (!l || !c) {
		return;
	}
	/* work already done in sccp_line_copyCodecSetsFromDeviceToLine during sccp_linedevice_allocate */
	if (!l->preferences_set_on_line_level && maybe_d) {
		memcpy(&c->preferences.audio, &maybe_d->preferences.audio, sizeof(c->preferences.audio));
		memcpy(&c->preferences.video, &maybe_d->preferences.video, sizeof(c->preferences.video));
	} else {
		memcpy(&c->preferences.audio, &l->preferences.audio, sizeof(c->preferences.audio));
		memcpy(&c->preferences.video, &l->preferences.video, sizeof(c->preferences.video));
	}
	if (maybe_d) {
		memcpy(&c->capabilities.audio, &maybe_d->capabilities.audio, sizeof(c->capabilities.audio));
		memcpy(&c->capabilities.video, &maybe_d->capabilities.video, sizeof(c->capabilities.video));
	} else {
		memcpy(&c->capabilities.audio, &l->capabilities.audio, sizeof(c->capabilities.audio));
		memcpy(&c->capabilities.video, &l->capabilities.video, sizeof(c->capabilities.video));
	}

	// use a minimal default set, all devices should be able to support (last resort)
	if (c->preferences.audio[0] == SKINNY_CODEC_NONE) {
		pbx_log(LOG_WARNING, "%s: (updatePreferencesFromDevicesToLine) Could not retrieve preferences from line or device. Using Fallback Preferences from Global\n", c->designator);
		memcpy(&c->preferences.audio, &GLOB(global_preferences), sizeof(c->preferences.audio));
		memcpy(&c->preferences.video, &GLOB(global_preferences), sizeof(c->preferences.video));
	}

	char s1[512], s2[512];
	sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: (copyCodecSetsFromLineToChannel) channel capabilities:%s\n", c->designator, sccp_codec_multiple2str(s1, sizeof(s1) - 1, c->capabilities.audio, SKINNY_MAX_CAPABILITIES));
	sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: (copyCodecSetsFromLineToChannel) channel preferences:%s\n", c->designator, sccp_codec_multiple2str(s2, sizeof(s2) - 1, c->preferences.audio, SKINNY_MAX_CAPABILITIES));
}

void sccp_line_updatePreferencesFromDevicesToLine(sccp_line_t *l)
{
	sccp_linedevice_t *linedevice = NULL;
	boolean_t first=TRUE;
	if (!l) {
		return;
	}
	// sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_3 "%s: Update line preferences\n", l->name);
	// combine all preferences
	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		if (first) {
			if (!l->preferences_set_on_line_level) {
				memcpy(&l->preferences.audio, &linedevice->device->preferences.audio , sizeof(l->preferences.audio));
				memcpy(&l->preferences.video, &linedevice->device->preferences.video , sizeof(l->preferences.video));
			}
			first = FALSE;
		} else {
			if (!l->preferences_set_on_line_level) {
				skinny_codec_t temp[SKINNY_MAX_CAPABILITIES] = {SKINNY_CODEC_NONE};
				if (sccp_codec_getReducedSet(l->preferences.audio , linedevice->device->preferences.audio, temp) == 0) {
					// zero matching codecs we have to combine
					sccp_codec_combineSets(l->preferences.audio , linedevice->device->preferences.audio);
				} else {
					memcpy(&l->preferences.audio, &temp, sizeof *temp);
				}
				memset(&temp, SKINNY_CODEC_NONE, sizeof *temp);
				if (sccp_codec_getReducedSet(l->preferences.video , linedevice->device->preferences.video, temp) == 0) {
					// zero matching codecs we have to combine
					sccp_codec_combineSets(l->preferences.video , linedevice->device->preferences.video);
				} else {
					memcpy(&l->preferences.video, &temp, sizeof *temp);
				}
			}
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	char s1[512];
	sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: (updatePreferencesFromDevicesToLine) line preferences:%s\n", l->name, sccp_codec_multiple2str(s1, sizeof(s1) - 1, l->preferences.audio, SKINNY_MAX_CAPABILITIES));
}

void sccp_line_updateCapabilitiesFromDevicesToLine(sccp_line_t *l)
{
	sccp_linedevice_t *linedevice = NULL;
	boolean_t first=TRUE;
	if (!l) {
		return;
	}
	// sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_3 "%s: Update line capabilities \n", l->name);
	// combine all capabilities
	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		if (first) {
			memcpy(&l->capabilities.audio, &linedevice->device->capabilities.audio, sizeof(l->capabilities.audio));
			memcpy(&l->capabilities.video, &linedevice->device->capabilities.video, sizeof(l->capabilities.video));
			first = FALSE;
		} else {
			sccp_codec_combineSets(l->capabilities.audio, linedevice->device->capabilities.audio);
			sccp_codec_combineSets(l->capabilities.video, linedevice->device->capabilities.video);
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	// use a minimal default set, all devices should be able to support (last resort)
	if (l->capabilities.audio[0] == SKINNY_CODEC_NONE) {
		pbx_log(LOG_WARNING, "%s: (updateCapabilitiesFromDevicesToLine) Could not retrieve capabilities from line or device. Using Fallback Codecs Alaw/Ulaw\n", l->name);
		l->capabilities.audio[0] = SKINNY_CODEC_G711_ALAW_64K;
		l->capabilities.audio[1] = SKINNY_CODEC_G711_ALAW_56K;
		l->capabilities.audio[2] = SKINNY_CODEC_G711_ULAW_64K;
		l->capabilities.audio[3] = SKINNY_CODEC_G711_ULAW_56K;
		l->capabilities.audio[4] = SKINNY_CODEC_NONE;
	}

	char s1[512];
	sccp_log_and((DEBUGCAT_LINE + DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "%s: (updateCapabilitiesFromDevicesToLine) line capabilities:%s\n", l->name, sccp_codec_multiple2str(s1, sizeof(s1) - 1, l->capabilities.audio, SKINNY_MAX_CAPABILITIES));
}

void sccp_line_updateLineCapabilitiesByDevice(constDevicePtr d)
{
	if (!d) {
		return;
	}
	int instance = 0;
	for (instance = SCCP_FIRST_LINEINSTANCE; instance < d->lineButtons.size; instance++) {
		if (d->lineButtons.instance[instance]) {
			AUTO_RELEASE(sccp_linedevice_t, linedevice , sccp_linedevice_retain(d->lineButtons.instance[instance]));
			if (linedevice && linedevice->line){
				sccp_line_updateCapabilitiesFromDevicesToLine(linedevice->line);
			}
		}
	}
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
void regcontext_exten(sccp_line_t * l, struct subscriptionId *subscriptionId, int onoff)
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
	sccp_linedevice_t *lineDevice = NULL;
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

/*=================================================================================== MWI EVENT HANDLING ==============*/
void sccp_line_setMWI(linePtr line, int newmsgs, int oldmsgs)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (sccp_line_setMWI), newmsgs:%d, oldmsgs:%d\n", line->name, newmsgs, oldmsgs);
	if (line->voicemailStatistic.newmsgs != newmsgs || line->voicemailStatistic.oldmsgs != oldmsgs) {
		line->voicemailStatistic.newmsgs = newmsgs;
		line->voicemailStatistic.oldmsgs = oldmsgs;
	}
}
void sccp_line_indicateMWI(constLineDevicePtr linedevice)
{
	AUTO_RELEASE(sccp_device_t, d, sccp_device_retain(linedevice->device));
	AUTO_RELEASE(sccp_line_t, l, sccp_line_retain(linedevice->line));
	if (l && d) {
		sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_3 "%s: (sccp_line_indicateMWI) Set voicemail lamp:%s on device:%s\n", l->name, 
			l->voicemailStatistic.newmsgs ? "on" : "off", d->id);
		sccp_device_setLamp(d, SKINNY_STIMULUS_VOICEMAIL, linedevice->lineInstance, 
			l->voicemailStatistic.newmsgs ? d->mwilamp : SKINNY_LAMP_OFF);
	}
}

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
	l = SCCP_RWLIST_FIND(&GLOB(lines), sccp_line_t, tmpl, list, (tmpl && sccp_strcaseequals(tmpl->name, name)), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
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
sccp_line_t *__sccp_line_find_byid(constDevicePtr d, uint8_t instance, const char *filename, int lineno, const char *func)
#else
/*!
 * \param d SCCP Device
 * \param instance line instance as int
 * \return SCCP Line (can be null)
 */
sccp_line_t *sccp_line_find_byid(constDevicePtr d, uint8_t instance)
#endif
{
	sccp_line_t *l = NULL;

	if (!d || instance == 0) {
		return NULL;
	}

	sccp_log((DEBUGCAT_LINE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Looking for line with instance %d.\n", DEV_ID_LOG(d), instance);

	if (0 < instance && instance < d->lineButtons.size && d->lineButtons.instance[instance] && d->lineButtons.instance[instance]->line) {
#if DEBUG
		l = (sccp_line_t *)sccp_refcount_retain(d->lineButtons.instance[instance]->line, filename, lineno, func);
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
		l = (sccp_line_t *)sccp_refcount_retain(d->buttonTemplate[buttonIndex - 1].ptr, filename, lineno, func);
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
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
