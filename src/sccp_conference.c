/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference Class
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$  
 */

/*!
 * \remarks	Purpose: 	SCCP Conference
 * 		When to use:	Only methods directly related to the sccp conference implementation should be stored in this source file.
 *   		Relationships: 	
 */
#include "config.h"

#ifdef CS_SCCP_CONFERENCE
#include "sccp_conference.h"
#include "sccp_indicate.h"
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
	memset(moderator, 0, sizeof(sccp_conference_participant_t));
	
	/* Always initialize the features structure, we are in most cases always going to need it. */
	ast_bridge_features_init(&moderator->features);
	
	moderator->channel = owner;
	moderator->conference = conference;
	owner->conference = conference;
	conference->moderator = moderator;

	if(NULL == owner->owner)
	{
		return NULL;
	}

	if (0 != sccp_conference_addAstChannelToConferenceBridge(moderator, owner->owner) ) {
		// Error handling
	}
		
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
	
	if (ast_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
		//TODO throw error
		return conference;
	}
	
	return conference;
}


/*!
 * Add channel to conference bridge
 */
int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t *participant, struct ast_channel *currentParticipantPeer)
{
	if(NULL == participant)
		return -1;

	sccp_conference_t *conference = participant->conference;

	if(NULL == conference)
		return -1;
	if(NULL == currentParticipantPeer)
		return -1;

	ast_channel_lock(currentParticipantPeer);
	/* Allocate an asterisk channel structure as conference bridge peer for the participant */
	participant->conferenceBridgePeer = currentParticipantPeer;
	/*
	   ast_channel_alloc(0, currentParticipantPeer->_state, 0, 0, currentParticipantPeer->accountcode, 
			currentParticipantPeer->exten, currentParticipantPeer->context, currentParticipantPeer->amaflags, "ConferenceBridge/%s", currentParticipantPeer->name);
	if(!participant->conferenceBridgePeer) {
		ast_log(LOG_NOTICE, "Couldn't allocate participant peer.\n");
		ast_channel_unlock(currentParticipantPeer);
		return -1;
	}

	participant->conferenceBridgePeer->readformat = currentParticipantPeer->readformat;
	participant->conferenceBridgePeer->writeformat = currentParticipantPeer->writeformat;
	participant->conferenceBridgePeer->nativeformats = currentParticipantPeer->nativeformats;

	if(ast_channel_masquerade(participant->conferenceBridgePeer, currentParticipantPeer)){
		ast_channel_unlock(currentParticipantPeer);
		return -1;
	}*/ 

	ast_channel_unlock(currentParticipantPeer);

	return 0;
}

/*!
 * Add participant to conference
 *
 * @param conference conference
 * @param participant participant to remove
 */
void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *channel){

    sccp_conference_participant_t *part = NULL;
	struct ast_channel *currentParticipantPeer;

	if(!channel || !conference)
		return;

	if(channel != conference->moderator->channel && channel->conference){
		ast_log(LOG_NOTICE, "%s: Already in conference\n", DEV_ID_LOG(channel->device));
		return;
	}
	if(channel->state != SCCP_CHANNELSTATE_HOLD &&  channel->state != SCCP_CHANNELSTATE_CONNECTED){
		ast_log(LOG_NOTICE, "%s: Only connected or channel on hold eligible for conference: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
		return;
	}

	if(! (channel->owner && (currentParticipantPeer = CS_AST_BRIDGED_CHANNEL(channel->owner))))
	{
		ast_log(LOG_NOTICE, "%s: Weird error: Participant has no channel on our side: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
		return;
	}

	part = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!part) {
		return;
	}
	memset(part, 0, sizeof(sccp_conference_participant_t));
	
	part->channel = channel;
	part->conference = conference;
	channel->conference = conference;
	
	/* Always initialize the features structure, we are in most cases always going to need it. */
	ast_bridge_features_init(&part->features);
	
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, part, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	if(0 != sccp_conference_addAstChannelToConferenceBridge(part, currentParticipantPeer) )
	{
		// Todo: error handling
	}

	if (ast_pthread_create_background(&part->joinThread, NULL, sccp_conference_join_thread, part) < 0) {
		return;
	}
	
	SCCP_LIST_TRAVERSE(&conference->participants, part, list){
		//sccp_log(1)(VERBOSE_PREFIX_3 "Conference %d: members %s-%08x\n", conference->id, part->channel->line->name, part->channel->callid);
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
		sccp_log(1)(VERBOSE_PREFIX_3 "Conference %d: members\n", conference->id);		
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

	if(NULL == participant)
	{
		ast_log(LOG_ERROR, "SCCP: conference join thread null participant\n");
		return NULL;
	}

	astChannel = participant->conferenceBridgePeer;

	if(NULL == data)
	{
		ast_log(LOG_ERROR, "SCCP: conference join thread null channel\n");
		return NULL;
	}

	
	if(!astChannel){
		ast_log(LOG_ERROR, "SCCP: no channel for conference\n");
		participant->joinThread = AST_PTHREADT_NULL;
		return NULL;
	}else{
		ast_log(LOG_NOTICE, "SCCP: add channel to bridge\n");
	}
	  
	ast_bridge_join(participant->conference->bridge, astChannel, NULL, &participant->features);
	
	//sccp_conference_removeParticipant(participant->conference, participant->channel);
	
	participant->joinThread = AST_PTHREADT_NULL;
}
#endif
