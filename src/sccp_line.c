/*!
 * \file 	sccp_line.c
 * \brief 	SCCP Line
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */

#include "config.h"

#if ASTERISK_VERSION_NUM >= 10400
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_lock.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_actions.h"
#include "sccp_channel.h"
#include "sccp_features.h"
#include "sccp_mwi.h"
#include <asterisk/utils.h>

#ifdef CS_DYNAMIC_CONFIG
void sccp_line_pre_reload(void)
{
	sccp_line_t* l;

	SCCP_LIST_LOCK(&GLOB(lines));
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list){
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "%s: Setting Line to Pending Delete=1\n", l->name);
		l->pendingDelete = 1;
		l->pendingUpdate = 0;
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));
}

void sccp_line_post_reload(void)
{

}
#endif /* CS_DYNAMIC_CONFIG */

/*!
 * \brief Build Default SCCP Line.
 *
 * Creates an SCCP Line with default/global values
 *
 * \return Default SCCP Line
 */
sccp_line_t * sccp_line_create(void) {
	sccp_line_t * l = ast_malloc(sizeof(sccp_line_t));
	if (!l) {
		sccp_log(0)(VERBOSE_PREFIX_3 "Unable to allocate memory for a line\n");
		return NULL;
	}
	memset(l, 0, sizeof(sccp_line_t));
	ast_mutex_init(&l->lock);
	l =  sccp_line_applyDefaults(l);
	SCCP_LIST_HEAD_INIT(&l->channels);
	SCCP_LIST_HEAD_INIT(&l->devices);
	SCCP_LIST_HEAD_INIT(&l->mailboxes);

	return l;
}

/*!
 * \brief Apply Default Configuration to SCCP Line
 * \param l SCCP Line
 * \return SCCP line
 */
sccp_line_t *sccp_line_applyDefaults(sccp_line_t *l)
{
	if(!l)
		return NULL;


	l->incominglimit = 99; /* default value */
	l->echocancel = GLOB(echocancel); /* default value */
	l->silencesuppression = GLOB(silencesuppression); /* default value */
	l->audio_tos = GLOB(audio_tos); /* default value */
	l->video_tos = GLOB(video_tos); /* default value */
	l->audio_cos = GLOB(audio_cos); /* default value */
	l->video_cos = GLOB(video_cos); /* default value */
	l->transfer = TRUE; /* default value. on if the device transfer is on*/
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
 */
sccp_line_t *sccp_line_addToGlobals(sccp_line_t *line){
	sccp_line_t *l =NULL;

	if(!line){
		ast_log(LOG_ERROR, "Adding null to global line list is not allowed!\n");
		return NULL;
	}

	SCCP_LIST_LOCK(&GLOB(lines));
	/* does the line created by an other thread? */
	SCCP_LIST_TRAVERSE(&GLOB(lines), l, list) {
		if(!strcasecmp(l->name, line->name)) {
			break;
		}
	}

	if(l){
		ast_log(LOG_NOTICE, "SCCP: line '%s' was created by an other thread\n", line->name);
		ast_free(line);
		SCCP_LIST_UNLOCK(&GLOB(lines));
		return l;
	}

	/* line was not created */
	SCCP_LIST_INSERT_HEAD(&GLOB(lines), line, list);
	SCCP_LIST_UNLOCK(&GLOB(lines));
	sccp_log(1)(VERBOSE_PREFIX_3 "Added line '%s'\n", line->name);


	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));
	event->type=SCCP_EVENT_LINECREATED;
	event->event.lineCreated.line = line;
	sccp_event_fire((const sccp_event_t **)&event);

	return line;
}


/*!
 * \brief Kill all Channels of a specific Line
 * \param l SCCP Line
 * \note Should be Called with a lock on l->lock
 */
void sccp_line_kill(sccp_line_t * l) {
	sccp_channel_t * c;

	if (!l)
		return;

	SCCP_LIST_LOCK(&l->channels);
	SCCP_LIST_TRAVERSE(&l->channels, c, list) {
		sccp_channel_endcall(c);
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
 * \todo integrate sccp_line_clean and sccp_line_delete_nolock into sccp_line_delete
 */
void sccp_line_clean(sccp_line_t * l, boolean_t remove_from_global) {
	sccp_device_t 		*d;
	sccp_linedevices_t	*linedevice;

	if (!l)
		return;

	sccp_line_kill(l);

	/* remove from the global lines list */
	if (remove_from_global) {
		if(l->list.prev == NULL && l->list.next == NULL && GLOB(lines).first != l) {
			if (GLOB(lines).size > 1)
				ast_log(LOG_ERROR, "%s: removing line from global lines list. prev and next pointer ist not set while lines list size is %d\n", l->name, GLOB(lines).size);
		} else {
			SCCP_LIST_LOCK(&GLOB(lines));
			SCCP_LIST_REMOVE(&GLOB(lines), l, list);
			SCCP_LIST_UNLOCK(&GLOB(lines));
		}
	}

	SCCP_LIST_LOCK(&l->devices);
	while( (linedevice = SCCP_LIST_REMOVE_HEAD(&l->devices, list))){
		if(linedevice) {
			if(!linedevice->device)
				continue;
			d = linedevice->device;

			/* remove the line from the device lines list */
			//SCCP_LIST_LOCK(&d->lines);
			//SCCP_LIST_REMOVE(&d->lines, l, listperdevice);
			//SCCP_LIST_UNLOCK(&d->lines);
			sccp_device_lock(d);
			d->linesCount--;
			sccp_device_unlock(d);

			ast_free(linedevice);
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if (l->trnsfvm)
		ast_free(l->trnsfvm);

	if (remove_from_global) {
		sccp_mutex_destroy(&l->lock);
	}
	sccp_mailbox_t *mailbox = NULL;

	while( (mailbox = SCCP_LIST_REMOVE_HEAD(&l->mailboxes, list))){
		if(!mailbox)
			return;

		sccp_mwi_unsubscribeMailbox(&mailbox);
		if (mailbox->mailbox)
			sccp_free(mailbox->mailbox);
		if (mailbox->context)
			sccp_free(mailbox->context);
		sccp_free(mailbox);
	}
	ast_free(l);
}


/*!
 * \brief Delete an SCCP line
 * \param l SCCP Line
 * \note Should be Called without a lock on l->lock
 */
void sccp_line_delete_nolock(sccp_line_t * l) {
	sccp_line_clean(l, TRUE);
}

/*!
 * \brief Set a Call Forward on a specific Line
 * \param l SCCP Line
 * \param device device that requested the forward
 * \param type Call Forward Type as uint8_t
 * \param number Number to which should be forwarded
 * \todo we should check, that extension is reachable on line
 */
void sccp_line_cfwd(sccp_line_t * l, sccp_device_t *device, uint8_t type, char * number) {
	sccp_linedevices_t *linedevice;

	if (!l)
		return;


	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
		if((linedevice->device == device))
			break;
	}
	SCCP_LIST_UNLOCK(&l->devices);

	if(!linedevice){
		ast_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return;
	}

	if (type == SCCP_CFWD_NONE) {
		linedevice->cfwdAll.enabled = 0;
		linedevice->cfwdBusy.enabled = 0;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", DEV_ID_LOG(device), l->name);
	} else {
		if (!number || ast_strlen_zero(number)) {
			linedevice->cfwdAll.enabled = 0;
			linedevice->cfwdBusy.enabled = 0;
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid\n", DEV_ID_LOG(device));
		}else{
			switch(type){
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
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward enabled on line %s to number %s\n", DEV_ID_LOG(device), l->name, number);
		}
	}
	// sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, l->instance, 0, 0);

	//SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
		if(linedevice && linedevice->device){

			sccp_dev_starttone(linedevice->device, SKINNY_TONE_ZIPZIP, 0, 0, 0);
			sccp_feat_changed(linedevice->device, SCCP_FEATURE_CFWDALL);
		}
	//}
}

/*!
 * \brief Attach a Device to a line
 * \param l SCCP Line
 * \param device SCCP Device
 * \param subscriptionId Subscription ID for addressing individual devices on the line
 */
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t *device, struct subscriptionId *subscriptionId)
{

	sccp_linedevices_t *linedevice;
	if(!l || !device)
		return;

	sccp_log((DEBUGCAT_HIGH + DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: add device to line %s\n", DEV_ID_LOG(device), l->name);
	linedevice = ast_malloc(sizeof(sccp_linedevices_t));
	memset(linedevice, 0, sizeof(sccp_linedevices_t));
	linedevice->device = device;

	if(NULL != subscriptionId) {
		sccp_copy_string(linedevice->subscriptionId.name,  subscriptionId->name, sizeof(linedevice->subscriptionId.name));
		sccp_copy_string(linedevice->subscriptionId.number, subscriptionId->number, sizeof(linedevice->subscriptionId.number));
	}

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_INSERT_HEAD(&l->devices, linedevice, list);
	SCCP_LIST_UNLOCK(&l->devices);

	sccp_line_lock(l);
	l->statistic.numberOfActiveDevices++;
	sccp_line_unlock(l);


	// fire event for new device
	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type=SCCP_EVENT_DEVICEATTACHED;
	event->event.deviceAttached.line = l;
	event->event.deviceAttached.device = device;
	sccp_event_fire((const sccp_event_t**)&event);
}


/*!
 * \brief Remove a Device from a Line
 *
 * Fire SCCP_EVENT_DEVICEDETACHED event after removing device.
 *
 * \param l SCCP Line
 * \param device SCCP Device
 */
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t *device)
{
	sccp_linedevices_t *linedevice;

	if(!l || !device)
		return;

	sccp_log((DEBUGCAT_HIGH + DEBUGCAT_LINE))(VERBOSE_PREFIX_3 "%s: remove device from line %s\n", DEV_ID_LOG(device), l->name);


	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type=SCCP_EVENT_DEVICEDETACHED;
	event->event.deviceAttached.line = l;
	event->event.deviceAttached.device = device;
	sccp_event_fire((const sccp_event_t**)&event);

	SCCP_LIST_LOCK(&l->devices);
	SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
		if (linedevice->device == device) {
			SCCP_LIST_LOCK(&l->devices);
			SCCP_LIST_REMOVE(&l->devices, linedevice, list);
			SCCP_LIST_UNLOCK(&l->devices);

			sccp_line_lock(l);
			l->statistic.numberOfActiveDevices--;
			sccp_line_unlock(l);
			ast_free(linedevice);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&l->devices);

}


/*!
 * \brief Add a Channel to a Line
 *
 * \param l SCCP Line
 * \param channel SCCP Channel
 */
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t *channel)
{
	if(!l || !channel)
		return;

	sccp_line_lock(l);
	l->channelCount++;
	l->statistic.numberOfActiveChannels++;
	sccp_line_unlock(l);

	if(GLOB(callAnswerOrder) == ANSWER_OLDEST_FIRST)
		SCCP_LIST_INSERT_TAIL(&l->channels, channel, list);
	else
		SCCP_LIST_INSERT_HEAD(&l->channels, channel, list);
}

/*!
 * \brief Remove a Channel from a Line
 *
 * \param l SCCP Line
 * \param channel SCCP Channel
 */
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t *channel)
{
	if(!l || !channel)
		return;

	sccp_line_lock(l);
	l->channelCount--;
	sccp_line_unlock(l);

	SCCP_LIST_REMOVE(&l->channels, channel, list);
}

#ifdef CS_DYNAMIC_CONFIG
/*!
 * copy the structure content of one line to a new one
 * \param line sccp line
 * \return new_line as sccp_line_t
 */
sccp_line_t * sccp_clone_line(sccp_line_t *orig_line){
	sccp_line_t * new_line = NULL;

	new_line=ast_calloc(1, sizeof(sccp_line_t));

	sccp_device_lock(orig_line);
	memcpy(new_line, orig_line, sizeof(*new_line));

	/* remaining values to be copied */
	// char 		*trnsfvm;
	new_line->trnsfvm=ast_strdup(orig_line->trnsfvm);
	
	// struct ast_variable	* variables;				
	struct ast_variable *v;
	new_line->variables=NULL;
	for (v = orig_line->variables; v; v = v->next)
	{
#if ASTERISK_VERSION_NUM >= 10600
		struct ast_variable *new_v = ast_variable_new(v->name, v->value, v->file);
#else
		struct ast_variable *new_v = ast_variable_new(v->name, v->value);
#endif
		new_v->next = new_line->variables;
		new_line->variables = new_v;
	}

	/* copy list-items over */
	sccp_duplicate_line_mailbox_list(new_line,orig_line);
	sccp_duplicate_line_linedevices_list(new_line, orig_line);
	
	sccp_line_unlock(orig_line);
	return new_line;
}

/*!
 * Copy the list of mailbox from another line
 * \param device original sccp line from which to copy the list
 */
void sccp_duplicate_line_mailbox_list(sccp_line_t *new_line, sccp_line_t *orig_line) {
	sccp_mailbox_t *orig_mailbox=NULL;
	sccp_mailbox_t *new_mailbox=NULL;

	SCCP_LIST_HEAD_INIT(&new_line->mailboxes);
	SCCP_LIST_LOCK(&orig_line->mailboxes);
	SCCP_LIST_TRAVERSE(&orig_line->mailboxes, orig_mailbox, list){
		new_mailbox=ast_calloc(1,sizeof(sccp_mailbox_t));
		new_mailbox->mailbox=ast_strdup(orig_mailbox->mailbox);
		new_mailbox->context=ast_strdup(orig_mailbox->context);
		SCCP_LIST_INSERT_TAIL(&new_line->mailboxes, new_mailbox, list);
	}
	SCCP_LIST_UNLOCK(&orig_line->mailboxes);
}

/*!
 * Copy the list of linedevices from another line
 * \param device original sccp line from which to copy the list
 */
void sccp_duplicate_line_linedevices_list(sccp_line_t *new_line, sccp_line_t *orig_line) {
	sccp_linedevices_t *orig_linedevices=NULL;
	sccp_linedevices_t *new_linedevices=NULL;

	SCCP_LIST_HEAD_INIT(&new_line->devices);
	SCCP_LIST_LOCK(&orig_line->devices);
	SCCP_LIST_TRAVERSE(&orig_line->devices, orig_linedevices, list){
		new_linedevices=ast_calloc(1,sizeof(sccp_linedevices_t));
		memcpy(new_linedevices, orig_linedevices, sizeof(*new_linedevices));
		new_linedevices->device=sccp_device_find_byid(orig_linedevices->device->id, TRUE);
//		memcpy(new_linedevices->cfwdAll, orig_linedevices->cfwdAll, sizeof(sccp_cfwd_information_t));
//		memcpy(new_linedevices->cfwdBusy, orig_linedevices->cfwdBusy, sizeof(sccp_cfwd_information_t));
		SCCP_LIST_INSERT_TAIL(&new_line->devices, new_linedevices, list);
	}
	SCCP_LIST_UNLOCK(&orig_line->devices);
}

/*!
 * Checks two devices against one another and returns a sccp_diff_t if different
 * \param device_a sccp device
 * \param device_b sccp device
 * \return sccp_diff_t
 */
sccp_diff_t sccp_line_changed(sccp_line_t *line_a,sccp_line_t *line_b) {
	sccp_diff_t res=NO_CHANGES;
	
	sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_2 "(sccp_line_changed) Checking line_a: %s against line_b: %s\n", line_a->id, line_b->id);
	if (									// check changes requiring reset
		(strcmp(line_a->id, line_b->id)) ||
		(strcmp(line_a->pin, line_b->pin)) ||
		(strcmp(line_a->name, line_b->name)) ||
		(strcmp(line_a->description, line_b->description)) ||
		(strcmp(line_a->label, line_b->label)) ||
#ifdef CS_SCCP_REALTIME	
		(line_a->realtime != line_b->realtime) ||		
#endif
		(strcmp(line_a->adhocNumber, line_b->adhocNumber)) 
		) {
	        sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "Changes need reset\n");
		return CHANGES_NEED_RESET;
	} else if ( 								// check minor changes
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
		(line_a->channelCount != line_b->channelCount) ||
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
		(line_a->statistic.numberOfHoldChannels != line_b->statistic.numberOfHoldChannels) ||
		(line_a->statistic.numberOfDNDDevices != line_b->statistic.numberOfDNDDevices) ||
		(line_a->voicemailStatistic.newmsgs != line_b->voicemailStatistic.newmsgs) ||
		(line_a->voicemailStatistic.oldmsgs != line_b->voicemailStatistic.oldmsgs) ||
		(line_a->configurationStatus != line_b->configurationStatus) ||
		(line_a->callgroup != line_b->callgroup) ||
		#ifdef CS_SCCP_PICKUP
		(line_a->pickupgroup != line_b->pickupgroup) ||
		#endif
		(strcmp(line_a->defaultSubscriptionId.number, line_b->defaultSubscriptionId.number)) ||
		(strcmp(line_a->defaultSubscriptionId.name, line_b->defaultSubscriptionId.name))
		) {
	        sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "Minor changes\n");
	     	res=MINOR_CHANGES;
	}
	// changes in sccp_mailbox_t *orig_mailbox
	SCCP_LIST_LOCK(&line_a->mailboxes);
	SCCP_LIST_LOCK(&line_b->mailboxes);
	if (line_a->mailboxes.size != line_b->mailboxes.size) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "mailboxes: Changes need reset\n");
		res=MINOR_CHANGES;
	} else {
		sccp_mailbox_t *mb_a = SCCP_LIST_FIRST(&line_a->mailboxes);
		sccp_mailbox_t *mb_b = SCCP_LIST_FIRST(&line_b->mailboxes);
		while (mb_a && mb_b) {
			if ((strcmp(mb_a->mailbox,mb_b->mailbox)) || (strcmp(mb_a->context,mb_b->context))) {
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "mailboxes: Changes need reset\n");
				res=MINOR_CHANGES;
				break;
			}
			mb_a = SCCP_LIST_NEXT(mb_a, list);
			mb_b = SCCP_LIST_NEXT(mb_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&line_a->mailboxes);
	SCCP_LIST_UNLOCK(&line_b->mailboxes);

	// changes in sccp_linedevices_t *orig_linedevices
	SCCP_LIST_LOCK(&line_a->devices);
	SCCP_LIST_LOCK(&line_b->devices);
	if (line_a->devices.size != line_b->devices.size) {
		sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "devices: Changes need reset\n");
		res=CHANGES_NEED_RESET;
	} else {
		sccp_linedevices_t *dev_a = SCCP_LIST_FIRST(&line_a->devices);
		sccp_linedevices_t *dev_b = SCCP_LIST_FIRST(&line_b->devices);
		while (dev_a && dev_b) {
			if (strcmp(dev_a->device->id,dev_b->device->id)) {
				sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_3 "devices: Changes need reset\n");
				 res=CHANGES_NEED_RESET;
				 break;
			}
			dev_a = SCCP_LIST_NEXT(dev_a, list);
			dev_b = SCCP_LIST_NEXT(dev_b, list);
		}
	}
	SCCP_LIST_UNLOCK(&line_a->devices);
	SCCP_LIST_UNLOCK(&line_b->devices);

	/* \todo Still to implement */
/*
	char 					* trnsfvm;				
	struct ast_variable			* variables;				
*/
	
	sccp_log((DEBUGCAT_LINE | DEBUGCAT_NEWCODE | DEBUGCAT_CONFIG))(VERBOSE_PREFIX_2 "(sccp_line_changed) Returning : %d\n", res);
	return res;


}
#endif
