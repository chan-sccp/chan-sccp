/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference Class
 * \author
 * \date        $Date$
 * \version     $Revision$  
 */
#ifdef CS_SCCP_CONFERENCE


#include "config.h"
#include "sccp_conference.h"
#include "asterisk/bridging.h"
#include "asterisk/bridging_features.h"

static int lastConferenceID = 0;
static void * sccp_conference_join_thread(void *data);

/*!
 * Create conference
 *
 * @param channel conference owner
 * @return conference
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t *owner){

	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *moderator = NULL;
	int 	confCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX | AST_BRIDGE_CAPABILITY_THREAD | AST_BRIDGE_CAPABILITY_VIDEO;

	if(!owner)
		return NULL;

	/* create conference */
	conference = (sccp_conference_t *)ast_malloc(sizeof(sccp_conference_t));
	if(!conference)
		return NULL;

	memset(conference, 0, sizeof(sccp_conference_t));
	
	conference->id = ++lastConferenceID;
	SCCP_LIST_HEAD_INIT(&conference->participants);
	
	
	conference->bridge = ast_bridge_new(AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_SMART);
	
	

	/* create moderator */
	moderator = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!moderator){
			ast_free(conference);
			return NULL;
	}
	
	/* Always initialize the features structure, we are in most cases always going to need it. */
	ast_bridge_features_init(&moderator->features);
	
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: joining owner\n", owner->device->id, conference->id, owner->device->id);
	
	
	
	
	moderator->channel = owner;
	moderator->conference = conference;
	owner->conference = conference;
	conference->moderator = moderator;

		
	/* add moderator to participants list */
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: add owner\n", owner->device->id);
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	/* store conference */
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: store conference\n", owner->device->id);
	SCCP_LIST_LOCK(&sccp_conferences);
	SCCP_LIST_INSERT_HEAD(&sccp_conferences, conference, list);
	SCCP_LIST_UNLOCK(&sccp_conferences);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Creating %d created; Owner: %s \n", owner->device->id, conference->id, owner->device->id);
	
	
	//ast_bridge_join(conference->bridge, owner->owner, NULL, &moderator->features);
	if (ast_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
		//TODO throw error
		return conference;
	}
	
	return conference;
}

/*!
 * Add participant to conference
 *
 * @param conference conference
 * @param participant participant to remove
 */
void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *participant){

    sccp_conference_participant_t *part = NULL;

	if(!participant || !conference)
		return;

	/*if(participant->conference){
		ast_log(LOG_NOTICE, "%s: Already in conference\n", DEV_ID_LOG(participant->device));
		return;
	}*/
	
	part = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!part)
		return;

	
	/* Always initialize the features structure, we are in most cases always going to need it. */
	ast_bridge_features_init(&part->features);
	
	part->channel = participant;
	part->conference = conference;
	participant->conference = conference;

	/* Do we need both ? (-DD) */
	if(participant->state != SCCP_CHANNELSTATE_CONNECTED)
		sccp_indicate_nolock(selectedChannel->channel->device, selectedChannel->channel, SCCP_CHANNELSTATE_CONNECTED);
	if(channel->state == SCCP_CHANNELSTATE_HOLD)
		sccp_channel_resume(channel->device, channel);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Added participant to conference %d \n", participant->device->id, conference->id);
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, part, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	
	if (ast_pthread_create_background(&part->joinThread, NULL, sccp_conference_join_thread, part) < 0) {
		return;
	}
	
	SCCP_LIST_TRAVERSE(&conference->participants, part, list){
		sccp_log(1)(VERBOSE_PREFIX_3 "Conference %d: members %s-%08x\n", conference->id, part->channel->line->name, part->channel->callid);
			
	}
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
	
	SCCP_LIST_TRAVERSE(&conference->participants, part, list){
		sccp_log(1)(VERBOSE_PREFIX_3 "Conference %d: members %s-%08x\n", conference->id, part->channel->line->name, part->channel->callid);		
	}
	
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


static void * sccp_conference_join_thread(void *data){  
	sccp_conference_participant_t *participant = (sccp_conference_participant_t *)data;
	struct ast_channel	*astChannel;
	
	
	if(participant == participant->conference->moderator){
		astChannel = participant->channel->owner;
	}else{
		astChannel = CS_AST_BRIDGED_CHANNEL(participant->channel->owner);
	}
	
	if(!astChannel){
		ast_log(LOG_ERROR, "SCCP: no channel for conference\n");
		participant->joinThread = AST_PTHREADT_NULL;
		return NULL;
	}else{
		ast_log(LOG_NOTICE, "SCCP: add %s to bridge\n", astChannel->name);
	}
	  
	ast_bridge_join(participant->conference->bridge, astChannel, NULL, &participant->features);
	
	sccp_conference_removeParticipant(participant->conference, participant->channel);
	
	participant->joinThread = AST_PTHREADT_NULL;
}
#endif
