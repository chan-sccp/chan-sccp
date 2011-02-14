
/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference Class
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \remarks	Purpose: 	SCCP Conference
 * 		When to use:	Only methods directly related to the sccp conference implementation should be stored in this source file.
 *   		Relationships: 	
 */
#include "config.h"
#include "common.h"

#ifdef CS_SCCP_CONFERENCE
#    include "asterisk/bridging.h"
#    include "asterisk/bridging_features.h"
SCCP_FILE_VERSION(__FILE__, "$Revision$")
#    if ASTERISK_VERSION_NUM >= 10602
static int lastConferenceID = 0;

static void *sccp_conference_join_thread(void *data);

/*!
 * Create conference
 *
 * @param channel conference owner
 * @return conference
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t * owner)
{
	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: sccp_conference_create called.\n");

	sccp_conference_t *conference = NULL;

	sccp_conference_participant_t *moderator = NULL;

	//int   confCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX | AST_BRIDGE_CAPABILITY_THREAD | AST_BRIDGE_CAPABILITY_VIDEO;

	if (NULL == owner) {
		ast_log(LOG_ERROR, "SCCP: Conference: NULL owner (sccp channel) while creating new conference.\n");
		return NULL;
	}

	if (NULL == owner->owner) {
		ast_log(LOG_ERROR, "SCCP: Conference: NULL owner (ast channel) while creating new conference.\n");
		return NULL;
	}

	/* create conference */
	conference = (sccp_conference_t *) ast_malloc(sizeof(sccp_conference_t));
	if (NULL == conference) {
		ast_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference.\n");
		return NULL;
	}

	memset(conference, 0, sizeof(sccp_conference_t));

	conference->id = ++lastConferenceID;
	SCCP_LIST_HEAD_INIT(&conference->participants);
	conference->bridge = ast_bridge_new(AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_SMART);

	if (NULL == conference->bridge) {
		ast_log(LOG_ERROR, "SCCP: Conference: Conference: conference bridge could not be created.\n");
		return NULL;
	}


	/* create moderator */
	moderator = (sccp_conference_participant_t *) ast_malloc(sizeof(sccp_conference_participant_t));
	if (!moderator) {
		ast_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference moderator.\n");
		ast_free(conference);
		return NULL;
	}

	/* Initialize data structures */

	memset(moderator, 0, sizeof(sccp_conference_participant_t));

	ast_bridge_features_init(&moderator->features);

	moderator->channel = owner;
	moderator->conference = conference;
	owner->conference = conference;
	conference->moderator = moderator;


#if 0
	/* add moderator to participants list */
	sccp_log(1) (VERBOSE_PREFIX_3 "%s: Conference: adding moderator to participant list.\n", owner->device->id);
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	SCCP_LIST_UNLOCK(&conference->participants);


	/* Add moderator to conference. */
	if (0 != sccp_conference_addAstChannelToConferenceBridge(moderator, moderator->channel->owner)) {
		ast_log(LOG_ERROR, "SCCP: Conference: failed to add moderator channel (preparation phase).\n");
		/* TODO: Error handling. */
	}
#endif

	/* Store conference in global list. */
	sccp_log(1) (VERBOSE_PREFIX_3 "%s: Conference: adding conference to global conference list.\n", owner->device->id);
	SCCP_LIST_LOCK(&sccp_conferences);
	SCCP_LIST_INSERT_HEAD(&sccp_conferences, conference, list);
	SCCP_LIST_UNLOCK(&sccp_conferences);

	sccp_log(1) (VERBOSE_PREFIX_3 "%s: Conference: Conference with id %d created; Owner: %s \n", owner->device->id, conference->id, owner->device->id);

#if 0
	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_create.\n");
	if (ast_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
		ast_log(LOG_ERROR, "SCCP: Conference: failed to initiate join thread for moderator.\n");
		return conference;
	}
#endif
	

	return conference;
}

/*!
 * Add channel to conference bridge
 * 
 * \lock
 * 	- asterisk channel
 */
int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t * participant, struct ast_channel *chan)
{
	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addAstChannelToConferenceBridge called.\n");

	if (NULL == participant)
		return -1;

	sccp_conference_t *conference = participant->conference;

	if (NULL == conference)
		return -1;

	if (NULL == chan)
		return -1;

	// Now done in add_participant
	// pbx_channel_lock(chan);
	
		//if (chan->pbx) {
		#if 1
		//ast_channel_lock(chan);
		ast_set_flag(chan, AST_FLAG_BRIDGE_HANGUP_DONT); /* don't let the after-bridge code run the h-exten */
		//ast_channel_unlock(chan);
		#endif
//	}

struct ast_channel *bridge = CS_AST_BRIDGED_CHANNEL(chan);

/* Prevent also the bridge from hanging up. */
if (NULL != bridge) {
#if 0
		//ast_channel_lock(bridge);
		ast_set_flag(bridge, AST_FLAG_BRIDGE_HANGUP_DONT); /* don't let the after-bridge code run the h-exten */
		//ast_channel_unlock(bridge);
	#endif
		}

	/* Allocate an asterisk channel structure as conference bridge peer for the participant */
	participant->conferenceBridgePeer = ast_channel_alloc(0, chan->_state, 0, 0, chan->accountcode, 
	   chan->exten, chan->context, chan->amaflags, "ConferenceBridge/%s", chan->name);
	   
	   if(!participant->conferenceBridgePeer) {
	   ast_log(LOG_NOTICE, "Couldn't allocate participant peer.\n");
	  	 //pbx_channel_unlock(chan);
	 	  return -1;
	   }

	   participant->conferenceBridgePeer->readformat = chan->readformat;
	   participant->conferenceBridgePeer->writeformat = chan->writeformat;
	   participant->conferenceBridgePeer->nativeformats = chan->nativeformats;

	   if(ast_channel_masquerade(participant->conferenceBridgePeer, chan)){
	   ast_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
	   //pbx_channel_unlock(chan);
	   ast_hangup(participant->conferenceBridgePeer);
	   participant->conferenceBridgePeer = NULL;
	   return -1;
	   } else {
	   			ast_channel_lock(participant->conferenceBridgePeer);
				ast_do_masquerade(participant->conferenceBridgePeer);
				ast_channel_unlock(participant->conferenceBridgePeer);
				
				

				
	   }
	   

	// Now done in add_participant
	// pbx_channel_unlock(chan);

	return 0;
}

/*!
 * Add participant to conference
 *
 * @param conference conference
 * @param participant participant to remove
 * 
 * \warning
 * 	- conference->participants is not always locked
 *
 * \lock
 * 	- conference->participants
 */
void sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");
	
	if (!channel || !conference) {
		// TODO: Log
		return;
	}
	

	sccp_conference_participant_t *localParticipant = NULL, *remoteParticipant = NULL;

	//struct ast_channel *currentParticipantPeer;
	
	struct ast_channel *localCallLeg = NULL, *remoteCallLeg = NULL;
	
	/* We need to handle adding the moderator in a special way: Both legs of the call
	need to be added to the conference at the same time. (-DD) */
	boolean_t adding_moderator = (channel == conference->moderator->channel);

	if (NULL != channel->conference && !adding_moderator) {
		ast_log(LOG_NOTICE, "%s: Conference: Already in conference: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
		return;
	}

	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CONNECTED) {
		ast_log(LOG_NOTICE, "%s: Conference: Only connected or channel on hold eligible for conference: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
		return;
	}
	
	localCallLeg = channel->owner;
	if (NULL == localCallLeg) {
			ast_log(LOG_NOTICE, "%s: Conference: NULL local ast call leg: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
			return;
	}
	
	remoteCallLeg = CS_AST_BRIDGED_CHANNEL(localCallLeg);
	if (NULL == localCallLeg) {
			ast_log(LOG_NOTICE, "%s: Conference: NULL remote ast call leg: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
			return;
	}

	if(adding_moderator) {
		localParticipant = (sccp_conference_participant_t *) ast_malloc(sizeof(sccp_conference_participant_t));
		if (!localParticipant) {
			return;
		}
		memset(localParticipant, 0, sizeof(sccp_conference_participant_t));
		
		pbx_channel_lock(localCallLeg);
	}
	
	remoteParticipant = (sccp_conference_participant_t *) ast_malloc(sizeof(sccp_conference_participant_t));
	if (!remoteParticipant) {
		if(adding_moderator) {
			pbx_channel_unlock(localCallLeg);
			free(localParticipant);
		}
		return;
	}
	memset(remoteParticipant, 0, sizeof(sccp_conference_participant_t));
	
	pbx_channel_lock(remoteCallLeg);
	

	if(adding_moderator) {
		sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Adding remote party of moderator.\n");
	} else {
		sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Adding remote party of ordinary participant call.\n");
	}
		
	remoteParticipant->channel 		= channel;
	remoteParticipant->conference 	= conference;
	ast_bridge_features_init(&remoteParticipant->features);

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, remoteParticipant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	if (0 != sccp_conference_addAstChannelToConferenceBridge(remoteParticipant, remoteCallLeg)) {
		// \todo TODO: error handling
	}

	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_addParticipant.\n");
	if (ast_pthread_create_background(&remoteParticipant->joinThread, NULL, sccp_conference_join_thread, remoteParticipant) < 0) {
		// \todo TODO: error handling
	}
	
	
	
	if(adding_moderator) {
		sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Adding local party of moderator.\n");
	
	localParticipant->channel 		= channel;
	localParticipant->conference 	= conference;
	ast_bridge_features_init(&remoteParticipant->features);

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, localParticipant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	if (0 != sccp_conference_addAstChannelToConferenceBridge(localParticipant, localCallLeg)) {
		// \todo TODO: error handling
	}

	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_addParticipant.\n");
	if (ast_pthread_create_background(&localParticipant->joinThread, NULL, sccp_conference_join_thread, localParticipant) < 0) {
		// \todo TODO: error handling
	}
	
	}

	pbx_channel_unlock(remoteCallLeg);
	
	if(adding_moderator) {
		pbx_channel_unlock(localCallLeg);
	}

	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		sccp_log(1) (VERBOSE_PREFIX_3 "Conference %d: members %s-%08x\n", conference->id, part->channel->line->name, part->channel->callid);
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
 * 
 * \warning
 * 	- conference->participants is not always locked
 *
 * \lock
 * 	- conference->participants
 */
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_channel_t * participant)
{
	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		if (part->channel == participant) {
			part->channel->conference = NULL;
			SCCP_LIST_REMOVE(&conference->participants, part, list);
			ast_free(part);
		}

	}
	SCCP_LIST_UNLOCK(&conference->participants);

	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		sccp_log(1) (VERBOSE_PREFIX_3 "Conference %d: members\n", conference->id);
	}

	if (conference->participants.size < 1)
		sccp_conference_end(conference);
}

/*!
 * End conference
 *
 * @param conference conference
 *
 * \lock
 * 	- conference->participants
 */
void sccp_conference_end(sccp_conference_t * conference)
{
	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_LOCK(&conference->participants);
	while ((part = SCCP_LIST_REMOVE_HEAD(&conference->participants, list))) {
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
void sccp_conference_module_start()
{
	SCCP_LIST_HEAD_INIT(&sccp_conferences);
}

static void *sccp_conference_join_thread(void *data)
{
	sccp_log(1) (VERBOSE_PREFIX_3 "Conference: entering join thread.\n");

	sccp_conference_participant_t *participant = (sccp_conference_participant_t *) data;

	struct ast_channel *astChannel;

	if (NULL == participant) {
		ast_log(LOG_ERROR, "SCCP: Conference: Join thread: NULL participant.\n");
		return NULL;
	}
	if (NULL == participant->channel) {
		ast_log(LOG_ERROR, "SCCP: Conference: Join thread: NULL participant sccp channel.\n");
		return NULL;
	}

	astChannel = participant->conferenceBridgePeer;

	if (!astChannel) {
		ast_log(LOG_ERROR, "SCCP: Conference: Join thread: no ast channel for conference.\n");
		participant->joinThread = AST_PTHREADT_NULL;
		return NULL;
	} else {
		ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: adding channel to bridge: %s-%08x\n", participant->channel->line->name, participant->channel->callid);
	}

	ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering ast_bridge_join: %s-%08x\n", participant->channel->line->name, participant->channel->callid);
	ast_bridge_join(participant->conference->bridge, astChannel, NULL, &participant->features);
	/* \todo TODO: We must fix the resolving of the channel lookup the other way round since the channel is not related to the actual participant and its present device. (-DD) */
	//ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving ast_bridge_join: %s-%08x\n", participant->channel->line->name, participant->channel->callid);

					if (ast_pbx_start(astChannel)) {
					ast_log(LOG_WARNING, "SCCP: Unable to start PBX on %s\n", participant->conferenceBridgePeer->name);
					ast_hangup(astChannel);
					return -1;
				}

	//sccp_conference_removeParticipant(participant->conference, participant->channel);

	participant->joinThread = AST_PTHREADT_NULL;

	return NULL;
}
#    endif									// ASTERISK_VERSION_NUM
#endif										// CS_SCCP_CONFERENCE
