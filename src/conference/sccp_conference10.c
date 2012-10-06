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

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;
SCCP_LIST_HEAD(, sccp_conference_t) conferences;								/*!< our list of conferences */

static void __sccp_conference_destroy(sccp_conference_t *conference){
	if (!conference)
		return;

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying conference %08x\n", conference->id);

	return;
}

static void __sccp_conference_participant_destroy(sccp_conference_participant_t *participant){
	if (!participant)
		return;

	return;
}


#define sccp_conference_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_conference_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define sccp_participant_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_participant_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)


sccp_conference_t *sccp_conference_create(sccp_channel_t *conferenceCreatorChannel){
	sccp_conference_t 		*conference	= NULL;
	sccp_conference_participant_t 	*moderator 	= NULL;
	char 				conferenceIdentifier[CHANNEL_DESIGNATOR_SIZE];
	int 				conferenceID 	= ++lastConferenceID;


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
	conference->bridge = ast_bridge_new(AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_SMART);
	
	
	/** create participant(moderator) */
	moderator = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_MODERATOR, "sccp::participant", __sccp_conference_participant_destroy);
	if (!moderator) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference moderator.\n");
		sccp_conference_release(conference);
		return NULL;
	}
	/** initialize new moderator */
	memset(moderator, 0, sizeof(sccp_conference_participant_t));
	ast_bridge_features_init(&moderator->features);
	
	moderator->channel = conferenceCreatorChannel;
	moderator->conference = sccp_conference_retain(conference);
	conference->moderator = sccp_participant_retain(moderator);
	
	conferenceCreatorChannel->conference = sccp_conference_retain(conference);
	
	
	
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, (sccp_conference_t *)sccp_conference_retain(conference), list);
	SCCP_LIST_UNLOCK(&conferences);
	
	{
		PBX_CHANNEL_TYPE *astChannel = conferenceCreatorChannel->owner;	
		pbx_channel_lock(astChannel);
		
		/** add remote party */
		sccp_conference_addParticipant(conference, conferenceCreatorChannel);
		
		moderator->conferenceBridgePeer = ast_channel_alloc(0, astChannel->_state, 0, 0, astChannel->accountcode, astChannel->exten, astChannel->context, astChannel->linkedid, astChannel->amaflags, "SCCP/CONFERENCE/%08X@%08X", moderator->id, conference->id);
		
		
		if (pbx_channel_masquerade(moderator->conferenceBridgePeer, astChannel)) {
			pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
			PBX(requestHangup) (moderator->conferenceBridgePeer);
			moderator->conferenceBridgePeer = NULL;
			return NULL;
		} else {
			pbx_channel_lock(moderator->conferenceBridgePeer);

			pbx_do_masquerade(moderator->conferenceBridgePeer);
			pbx_channel_unlock(moderator->conferenceBridgePeer);

			/** assign hangup cause to origChannel */
			astChannel->hangupcause = AST_CAUSE_NORMAL_UNSPECIFIED;
		}
		ast_bridge_impart(moderator->conference->bridge, moderator->conferenceBridgePeer, NULL, &moderator->features, 1);
		
		
		pbx_channel_unlock(astChannel);
	}
	
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
	
	participant->conference = sccp_conference_retain(conference);
	participant->id = ++lastParticipantID;
	pbx_bridge_features_init(&participant->features);
	
	astChannel = CS_AST_BRIDGED_CHANNEL(participantChannel->owner);	
	
	participant->conferenceBridgePeer = ast_channel_alloc(0, astChannel->_state, 0, 0, astChannel->accountcode, astChannel->exten, astChannel->context, astChannel->linkedid, astChannel->amaflags, "SCCP/CONFERENCE/%08X@%08X", participant->id, conference->id);
	
	
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
	
	
	
	
	ast_bridge_impart(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, 1);
	
	pbx_log(LOG_NOTICE, "SCCP-Conference::%d: Participant '%p' joined the conference.\n", participant->conference->id, participant);
	
	
	/** do not retain participant, because we added it to conferencelist */
	return;
}


void sccp_conference_removeParticipant(sccp_conference_t *conference, sccp_conference_participant_t *participant){
	
	/* Update the presence of the moderator */
	if (participant == conference->moderator) {
		conference->moderator = NULL;
	}
	
	
	SCCP_LIST_LOCK(&conference->participants);
	participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);
	
	ast_bridge_depart(participant->conference->bridge, participant->conferenceBridgePeer);
	
	if (conference->participants.size < 1) {
		sccp_conference_end(conference);
	}
}


void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_module_start(void){}
void sccp_conference_end(sccp_conference_t * conference){}
int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t * participant, PBX_CHANNEL_TYPE * currentParticipantPeer){ return 0;}

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
 
