/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference Class
 * \author
 * \date	$LastChangedDate$
 *
 */
#include "config.h"
#include "sccp_conference.h"

static int lastConferenceID = 0;

/*!
 * Create conference
 *
 * @param channel conference owner
 * @return conference
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t *owner){
#if 0
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *moderator = NULL;

	if(!owner)
		return NULL;

	/* create conference */
	conference = (sccp_conference_t *)ast_malloc(sizeof(sccp_conference_t));
	if(!conference)
		return NULL;

	memset(conference, 0, sizeof(sccp_conference_t));
	
	conference->id = ++lastConferenceID;
	SCCP_LIST_HEAD_INIT(&conference->participants);
	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Creating conference %d \n", owner->device->id, conference->id);

	/* create moderator */
	moderator = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!moderator){
			ast_free(conference);
			return NULL;
	}
	moderator->channel = owner;
	owner->conference = conference;
	conference->moderator = moderator;

	
	/* add moderator to participants list */
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	
	/* store conference */
	SCCP_LIST_LOCK(&sccp_conferences);
	SCCP_LIST_INSERT_HEAD(&sccp_conferences, conference, list);
	SCCP_LIST_UNLOCK(&sccp_conferences);

	return conference;
#endif
        return NULL;
}

/*!
 * Add participant to conference
 *
 * @param conference conference
 * @param participant participant to remove
 */
void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *participant){
#if 0
    sccp_conference_participant_t *part = NULL;

	if(!participant || !conference)
		return;

	if(participant->conference){
		ast_log(LOG_NOTICE, "%s: Already in conference\n", DEV_ID_LOG(participant->device));
		return;
	}
	
	part = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!part)
		return;

	part->channel = participant;
	participant->conference = conference;

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Added participant to conference %d \n", participant->device->id, conference->id);
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, part, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	
	SCCP_LIST_TRAVERSE(&conference->participants, part, list){
		sccp_log(1)(VERBOSE_PREFIX_3 "Conference %d: members %s-%08x\n", conference->id, part->channel->line->name, part->channel->callid);
			
	}
#endif
}


/*!
 * Remove participant from conference
 *
 * @param conference conference
 * @param participant participant to remove
 *
 * \todo implement this
 * \todo check if the are enough participants in conference
 */
void sccp_conference_removeParticipant(sccp_conference_t *conference, sccp_channel_t *participant){
	sccp_conference_participant_t *part = NULL;
	
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, part, list){
		if(part->channel == participant){
			part->channel->conference = NULL;
			SCCP_LIST_REMOVE(&conference->participants, part, list);
			ast_free(part);
		}
			
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	
	if(conference->participants.size < 1)
		sccp_conference_end(conference);
}

void sccp_conference_end(sccp_conference_t *conference){
	sccp_conference_participant_t *part = NULL;
  
	
	SCCP_LIST_LOCK(&conference->participants);
	while((part = SCCP_LIST_REMOVE_HEAD(&conference->participants, list))) {
		part->channel->conference = NULL;
		ast_free(part);
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	
	SCCP_LIST_HEAD_DESTROY(&conference->participants);
	ast_free(conference);
}

/*!
 * doing initial actions
 */
void sccp_conference_module_start(){
	SCCP_LIST_HEAD_INIT(&sccp_conferences);
}

