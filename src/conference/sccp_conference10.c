/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference for asterisk 10
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sorceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *
 * $Date$
 * $Revision$
 */


#include <config.h>
#include "../common.h"

#ifdef CS_SCCP_CONFERENCE


#include "asterisk/bridging.h"
#include "asterisk/bridging_features.h"


#define sccp_conference_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_conference_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define sccp_participant_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_participant_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define SCCP_CONFERENCE_USE_IMPART 0

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;
SCCP_LIST_HEAD(, sccp_conference_t) conferences;								/*!< our list of conferences */

#if SCCP_CONFERENCE_USE_IMPART == 0
static void *sccp_conference_join_thread(void *data);
#endif


static void __sccp_conference_destroy(sccp_conference_t *conference){
	if (!conference)
		return;

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying conference %08x\n", conference->id);
	ast_bridge_destroy(conference->bridge);
	return;
}

static void __sccp_conference_participant_destroy(sccp_conference_participant_t *participant){
	
	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying participant %d %p\n", participant->id, participant);
	
	participant->conference = sccp_conference_release(participant->conference);
	participant->channel = participant->channel ? sccp_conference_retain(participant->channel) : NULL;
	ast_bridge_features_cleanup(&participant->features);

	
	return;
}





sccp_conference_t *sccp_conference_create(sccp_channel_t *conferenceCreatorChannel){
	sccp_conference_t 		*conference	= NULL;
	sccp_conference_participant_t 	*moderator 	= NULL;
	char 				conferenceIdentifier[CHANNEL_DESIGNATOR_SIZE];
	int 				conferenceID 	= ++lastConferenceID;
	uint32_t			bridgeCapabilities;


	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_create called.\n");
	
	/** create conference */
	snprintf(conferenceIdentifier, CHANNEL_DESIGNATOR_SIZE, "SCCP/conference/%d", conferenceID);
	conference = (sccp_conference_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_t), SCCP_REF_CHANNEL, conferenceIdentifier, __sccp_conference_destroy);
	if (NULL == conference) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference.\n");
		return NULL;
	}
	
	/** initialize new conference */
	memset(conference, 0, sizeof(sccp_conference_t));
	conference->id = conferenceID;
	SCCP_LIST_HEAD_INIT(&conference->participants);
	
	
	bridgeCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX;
#if SCCP_CONFERENCE_USE_IMPART == 0
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;
#endif
#ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#endif
	conference->bridge = ast_bridge_new(bridgeCapabilities, AST_BRIDGE_FLAG_SMART);
	
	
	/** create participant(moderator) */
	moderator = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_MODERATOR, "sccp::participant", __sccp_conference_participant_destroy);
	if (!moderator) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference moderator.\n");
		sccp_conference_release(conference);
		return NULL;
	}
	/** initialize new moderator */
	ast_bridge_features_init(&moderator->features);
	
	moderator->channel = sccp_channel_retain(conferenceCreatorChannel);
	moderator->conference = sccp_conference_retain(conference);
	conference->moderator = sccp_participant_retain(moderator);
	
	conferenceCreatorChannel->conference = sccp_conference_retain(conference);
	
	
	
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, (sccp_conference_t *)sccp_conference_retain(conference), list);
	SCCP_LIST_UNLOCK(&conferences);
	
	{
		PBX_CHANNEL_TYPE *astChannel = conferenceCreatorChannel->owner;	
		pbx_channel_lock(astChannel); /* this lock is nessesary to keep this channel after sccp_conference_addParticipant */
		
		/** add remote party in first place */
		sccp_conference_addParticipant(conference, conferenceCreatorChannel);
		
		moderator->conferenceBridgePeer = ast_channel_alloc(0, astChannel->_state, 0, 0, astChannel->accountcode, astChannel->exten, astChannel->context, astChannel->linkedid, astChannel->amaflags, "SCCP/CONFERENCE/%03X@%08X", moderator->id, conference->id);


		/** break current bridge and swap moderator->conferenceBridgePeer with astChannel */
		if (pbx_channel_masquerade(moderator->conferenceBridgePeer, astChannel)) {
			pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
			PBX(requestHangup) (moderator->conferenceBridgePeer);
			moderator->conferenceBridgePeer = NULL;
			pbx_channel_unlock(astChannel);
			return NULL;
		} else {
			pbx_channel_lock(moderator->conferenceBridgePeer);

			pbx_do_masquerade(moderator->conferenceBridgePeer);
			pbx_channel_unlock(moderator->conferenceBridgePeer);

			/** assign hangup cause to origChannel */
			astChannel->hangupcause = AST_CAUSE_NORMAL_UNSPECIFIED;
		}

#if SCCP_CONFERENCE_USE_IMPART == 1
		ast_bridge_impart(moderator->conference->bridge, moderator->conferenceBridgePeer, NULL, &moderator->features, 1);
#else
		if (pbx_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
			// \todo TODO: error handling
		}
#endif
		
		
		pbx_channel_unlock(astChannel);
	}
	
	
	/** update callinfo */
	sprintf(conferenceCreatorChannel->callInfo.callingPartyName, "Conference %d", conference->id);
	sprintf(conferenceCreatorChannel->callInfo.calledPartyName, "Conference %d", conference->id);
// 	sccp_channel_send_callinfo(conferenceCreatorChannel);
	
	/** add to participant list */
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	/** we return the pointer, so do not release conference */
	return conference;
}



void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *participantChannel){
	
	PBX_CHANNEL_TYPE 		*astChannel = NULL;
	sccp_conference_participant_t 	*participant; 
	
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");
	
	/** create participant(moderator) */
	participant = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_MODERATOR, "sccp::participant", __sccp_conference_participant_destroy);
	
	if (!participant) {
		pbx_log(LOG_ERROR, "SCCP Conference: cannot alloc memory for new conference participant.\n");
		return;
	}
	
	pbx_bridge_features_init(&participant->features);
	participant->channel = NULL;
	participant->conference = sccp_conference_retain(conference);
	participant->id = ++lastParticipantID;
	
	
	astChannel = CS_AST_BRIDGED_CHANNEL(participantChannel->owner);	
	
	participant->conferenceBridgePeer = ast_channel_alloc(0, astChannel->_state, 0, 0, astChannel->accountcode, astChannel->exten, astChannel->context, astChannel->linkedid, astChannel->amaflags, "SCCP/CONFERENCE/%08X@%08X", participant->id, conference->id);
	

	/** break current bridge and swap moderator->conferenceBridgePeer with astChannel */
	if (pbx_channel_masquerade(participant->conferenceBridgePeer, astChannel)) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
		PBX(requestHangup) (participant->conferenceBridgePeer);
		participant->conferenceBridgePeer = NULL;
		return;
	} else {
		pbx_channel_lock(participant->conferenceBridgePeer);

		pbx_do_masquerade(participant->conferenceBridgePeer);
		pbx_channel_unlock(participant->conferenceBridgePeer);

		/** assign hangup cause to origChannel */
		astChannel->hangupcause = AST_CAUSE_NORMAL_UNSPECIFIED;
	}
	
	
	/** add to participant list */
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	
#if SCCP_CONFERENCE_USE_IMPART == 1
	ast_bridge_impart(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, 1);
#else
	if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_join_thread, participant) < 0) {
		// \todo TODO: error handling
	}
#endif
	
	pbx_log(LOG_NOTICE, "SCCP-Conference::%d: Participant '%p' joined the conference.\n", participant->conference->id, participant);
	
	
	/** do not retain participant, because we added it to conferencelist */
	return;
}


void sccp_conference_removeParticipant(sccp_conference_t *conference, sccp_conference_participant_t *participant){
	
	/* Update the presence of the moderator */
	if (participant == conference->moderator) {
		sccp_conference_end(conference);
		return;
	}
	
	
	SCCP_LIST_LOCK(&conference->participants);
	participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
// 	ast_bridge_depart(participant->conference->bridge, participant->conferenceBridgePeer);
	ast_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
	sccp_participant_release(participant);
	
	
	if (conference->participants.size < 1) {
		sccp_conference_end(conference);
	}
}


static void *sccp_conference_join_thread(void *data) {
	sccp_conference_participant_t	*participant	= NULL;
  
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: entering join thread.\n");
	
	
	/** cast data to conference_participant */
	participant = (sccp_conference_participant_t *) data;

	

	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering pbx_bridge_join: %s\n", participant->conferenceBridgePeer->name);

	
#if ASTERISK_VERSION_NUMBER < 11010
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features);
#else
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, NULL);
#endif
	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving pbx_bridge_join: %s\n", participant->conferenceBridgePeer->name);

// 	if (pbx_pbx_start(astChannel)) {
// 		pbx_log(LOG_WARNING, "SCCP: Unable to start PBX on %s\n", pbx_channel_name(participant->conferenceBridgePeer));
// 		PBX(requestHangup) (astChannel);
// 		// return -1;
// 		return NULL;
// 	}

	sccp_conference_removeParticipant(participant->conference, participant);

	participant->joinThread = AST_PTHREADT_NULL;

	return NULL;
}



void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_module_start(void){
	SCCP_LIST_HEAD_INIT(&conferences);
}
void sccp_conference_end(sccp_conference_t * conference){
	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_LOCK(&conference->participants);
	while ((part = SCCP_LIST_REMOVE_HEAD(&conference->participants, list))) {
		sccp_conference_removeParticipant(conference, part);
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	SCCP_LIST_HEAD_DESTROY(&conference->participants);
	
	
	SCCP_LIST_REMOVE(&conferences, conference, list);
	conference = sccp_conference_release(conference);	
}


void sccp_conference_readFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel){}
void sccp_conference_writeFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel){}

/* conf list related */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID){}
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant){}
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant){}
void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel){}


#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
 
