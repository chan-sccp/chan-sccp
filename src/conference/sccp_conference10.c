
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

#    include "asterisk/bridging.h"
#    include "asterisk/bridging_features.h"

#    define sccp_conference_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    define sccp_conference_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#    define sccp_participant_release(x)	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    define sccp_participant_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;

SCCP_LIST_HEAD(, sccp_conference_t) conferences;								/*!< our list of conferences */
static void *sccp_conference_thread(void *data);
void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator);

/*
 * refcount destroy functions 
 */
static void __sccp_conference_destroy(sccp_conference_t * conference)
{
	if (!conference)
		return;

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying conference %08x\n", conference->id);
	ast_bridge_destroy(conference->bridge);
	return;
}

static void __sccp_conference_participant_destroy(sccp_conference_participant_t * participant)
{

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying participant %d %p\n", participant->id, participant);

	if (participant->isModerator)
		participant->conference->num_moderators--;
	else
		participant->conference->num_participants--;
	participant->conference = sccp_conference_release(participant->conference);
	participant->channel = participant->channel ? sccp_channel_release(participant->channel) : NULL;
	ast_bridge_features_cleanup(&participant->features);
	return;
}

/*!
 * \brief Create a new conference on sccp_channel_t 
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t * conferenceCreatorChannel)
{
	sccp_conference_t *conference = NULL;
	char conferenceIdentifier[CHANNEL_DESIGNATOR_SIZE];
	int conferenceID = ++lastConferenceID;
	uint32_t bridgeCapabilities;

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
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;
#    ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#    endif
	conference->bridge = ast_bridge_new(bridgeCapabilities, AST_BRIDGE_FLAG_SMART);

	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, (sccp_conference_t *) sccp_conference_retain(conference), list);
	SCCP_LIST_UNLOCK(&conferences);

	/** we return the pointer, so do not release conference */
	pbx_log(LOG_NOTICE, "SCCP: Conference %d created\n", conference->id);
	return conference;
}

/*! 
 * \brief Generic Function to Add a new Participant to a conference. When moderator=TRUE this new participant will be a moderator
 */
void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator)
{
	PBX_CHANNEL_TYPE *astChannel = NULL;
	sccp_conference_participant_t *participant;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");

	/** create participant(moderator) */
	participant = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_MODERATOR, "sccp::participant", __sccp_conference_participant_destroy);

	if (!participant) {
		pbx_log(LOG_ERROR, "SCCP Conference: cannot alloc memory for new conference participant.\n");
		return;
	}

	pbx_bridge_features_init(&participant->features);
	participant->channel = sccp_channel_retain(participantChannel);;
	participant->conference = sccp_conference_retain(conference);
	participant->id = ++lastParticipantID;

	if (moderator) {
		participant->isModerator=TRUE;
		astChannel = participantChannel->owner;
		participant->origChannel = participantChannel->owner;
	} else {
		participant->isModerator=FALSE;
		astChannel = CS_AST_BRIDGED_CHANNEL(participantChannel->owner);
	}
	if (!(participant->conferenceBridgePeer = ast_channel_alloc(0, astChannel->_state, 0, 0, astChannel->accountcode, astChannel->exten, astChannel->context, astChannel->linkedid, astChannel->amaflags, "SCCP/CONFERENCE/%03X@%08X", participant->id, conference->id))) {
		pbx_log(LOG_ERROR, "SCCP Conference: creating conferenceBridgePeer failed.\n");
		return;
	}
	participant->conferenceBridgePeer->writeformat = astChannel->writeformat;
	participant->conferenceBridgePeer->readformat = astChannel->readformat;

	pbx_channel_lock(astChannel);

	/** break current bridge and swap participant->conferenceBridgePeer with astChannel */
	if (pbx_channel_masquerade(participant->conferenceBridgePeer, astChannel)) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
		PBX(requestHangup) (participant->conferenceBridgePeer);
		participant->conferenceBridgePeer = NULL;
		return;
	} else {
		pbx_log(LOG_NOTICE, "SCCP Conference: masquerading conferenceBridgePeer %d (%p) into bridge.\n", participant->id, participant);
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

	if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_thread, participant) < 0) {
		// \todo TODO: error handling
	}
	sccp_device_t *d = NULL;

	if ((d = sccp_channel_getDevice_retained(participant->channel))) {
		sprintf(participant->channel->callInfo.callingPartyName, "Conference %d", participant->conference->id);
		sprintf(participant->channel->callInfo.calledPartyName, "Conference %d", participant->conference->id);
		sccp_channel_send_callinfo(d, participant->channel);
		d = sccp_device_release(d);
	}
	pbx_log(LOG_NOTICE, "SCCP-Conference::%d: Participant %d (%p) joined the conference.\n", participant->conference->id, participant->id, participant);

	/** do not retain participant, because we added it to conferencelist */
	return;
}

/*!
 * \brief Add a simple participant to a conference
 */
void sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel)
{
	__sccp_conference_addParticipant(conference, participantChannel, FALSE);
	conference->num_participants++;
}

/*!
 * \brief Add a moderating participant to a conference
 */
void sccp_conference_addModerator(sccp_conference_t * conference, sccp_channel_t * moderatorChannel)
{
	// We should call add Participant for moderator as well, almost completely the same
	pbx_log(LOG_NOTICE, "SCCP: Adding moderator %s to conference %d\n", moderatorChannel->owner->name, conference->id);
	__sccp_conference_addParticipant(conference, moderatorChannel, TRUE);					// add moderator

	/** update callinfo */
	sccp_device_t *d = NULL;

	if ((d = sccp_channel_getDevice_retained(moderatorChannel))) {
		sprintf(moderatorChannel->callInfo.callingPartyName, "Conference %d", conference->id);
		sprintf(moderatorChannel->callInfo.calledPartyName, "Conference %d", conference->id);
		sccp_channel_send_callinfo(d, moderatorChannel);
		sccp_dev_set_keyset(d, 0, moderatorChannel->callid, KEYMODE_CONNCONF);
		d = sccp_device_release(d);
	}
	conference->num_moderators++;
}

/*!
 * \brief Remove a specific participant from a conference
 */
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: remove Participant %p from conference %d.\n", participant, conference->id);
	SCCP_LIST_LOCK(&conference->participants);
	participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: hanging up Participant %p, bridgePeer: %s.\n", participant, participant->conferenceBridgePeer->name);
	ast_clear_flag(participant->conferenceBridgePeer, AST_FLAG_BLOCKING);
	ast_hangup(participant->conferenceBridgePeer);
	sccp_participant_release(participant);

	/* Conference end if the number of participants+moderators == 1 */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Moderators: %d, Participants: %d\n", conference->num_moderators, conference->num_participants);
	if (conference->num_moderators + conference->num_participants == 1) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: No more conference participant, ending conference.\n");
		sccp_conference_end(conference);
	}
}

/*!
 * \brief Every participant is running one of the threads as long as they are joined to the conference
 * When the thread is cancelled they will clean-up after them selves using the removeParticipant function
 */
static void *sccp_conference_thread(void *data)
{
	sccp_conference_participant_t *participant = NULL;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: entering join thread.\n");

	/** cast data to conference_participant */
	participant = (sccp_conference_participant_t *) data;

	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering pbx_bridge_join: %s\n", participant->conferenceBridgePeer->name);

#        if ASTERISK_VERSION_NUMBER < 11010
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features);
#        else
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, NULL);
#        endif
	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving pbx_bridge_join: %s\n", participant->conferenceBridgePeer->name);

	sccp_conference_removeParticipant(participant->conference, participant);

	participant->joinThread = AST_PTHREADT_NULL;
	return NULL;
}

/*!
 * \brief Remote function to Remove a participantChannel from a conference
 */
void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

/*!
 * \brief
 */
void sccp_conference_module_start(void)
{
	SCCP_LIST_HEAD_INIT(&conferences);
}

/*!
 * \brief This function is called when the minimal number of occupants of a confernce is reached or when the last moderator hangs-up
 */
void sccp_conference_end(sccp_conference_t * conference)
{
	sccp_conference_participant_t *participant = NULL;
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Ending conference.\n");

	/* remove remaining participants / moderators */
	SCCP_LIST_LOCK(&conference->participants);
		if (SCCP_LIST_GETSIZE(conference->participants) > 0) {
			// remove the participants first
			SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
				if (!participant->isModerator) {
					sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Removing participant %p.\n", participant);
					ast_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
				}
			}
			// and then remove the moderators
			SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
				sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Removing moderator %p.\n", participant);
				ast_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
			}
		}
	SCCP_LIST_UNLOCK(&conference->participants);
	SCCP_LIST_HEAD_DESTROY(&conference->participants);

	/* remove conference */
	SCCP_LIST_REMOVE(&conferences, conference, list);
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Conference Ended.\n");
	conference = sccp_conference_release(conference);
}

void sccp_conference_readFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel)
{
}

void sccp_conference_writeFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel)
{
}

/* conf list related */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
}

void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
}

void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
}

void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
