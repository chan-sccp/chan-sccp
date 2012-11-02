
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

#    define sccp_participant_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    define sccp_participant_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#    define SCCP_CONFERENCE_USE_IMPART 0

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;

SCCP_LIST_HEAD(, sccp_conference_t) conferences;								/*!< our list of conferences */

#    if SCCP_CONFERENCE_USE_IMPART == 0
static void *sccp_conference_join_thread(void *data);
#    endif

void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator);

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

	participant->conference = sccp_conference_release(participant->conference);
	participant->channel = participant->channel ? sccp_conference_retain(participant->channel) : NULL;
	ast_bridge_features_cleanup(&participant->features);
	return;
}

sccp_conference_t *sccp_conference_create(sccp_channel_t * conferenceCreatorChannel)
{
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *moderator = NULL;
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
#    if SCCP_CONFERENCE_USE_IMPART == 0
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;
#    endif
#    ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#    endif
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
	moderator->id = ++lastParticipantID;
	conference->moderator = sccp_participant_retain(moderator);

	conferenceCreatorChannel->conference = sccp_conference_retain(conference);

	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, (sccp_conference_t *) sccp_conference_retain(conference), list);
	SCCP_LIST_UNLOCK(&conferences);

	/** we return the pointer, so do not release conference */
	pbx_log(LOG_NOTICE, "SCCP: Conference %d created\n", conference->id);
	return conference;
}

void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator)
{
	PBX_CHANNEL_TYPE *astChannel = NULL;
	sccp_conference_participant_t *participant;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");

	/** create participant(moderator) */
	if (moderator) {
		participant = conference->moderator;

		astChannel = participantChannel->owner;
	} else {
		participant = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_MODERATOR, "sccp::participant", __sccp_conference_participant_destroy);

		if (!participant) {
			pbx_log(LOG_ERROR, "SCCP Conference: cannot alloc memory for new conference participant.\n");
			return;
		}

		pbx_bridge_features_init(&participant->features);
		participant->channel = sccp_channel_retain(participantChannel);;
		participant->conference = sccp_conference_retain(conference);
		participant->id = ++lastParticipantID;

		astChannel = CS_AST_BRIDGED_CHANNEL(participantChannel->owner);
	}

	pbx_channel_lock(astChannel);										/* this lock is nessesary to keep this channel after sccp_conference_addParticipant */

	// copy current channel state / dialplan location to temp location before masquerading this channel out
	participant->goto_after.accountcode = ast_strdupa(astChannel->accountcode);
	participant->goto_after.exten = ast_strdupa(astChannel->exten);
	participant->goto_after.context = ast_strdupa(astChannel->context);
	participant->goto_after.linkedid = ast_strdupa(astChannel->linkedid);
	participant->goto_after.name = ast_strdupa(astChannel->name);
	participant->goto_after.amaflags = astChannel->amaflags;
	participant->goto_after.state = astChannel->_state;
	participant->goto_after.writeformat = astChannel->writeformat;
	participant->goto_after.readformat = astChannel->readformat;
	participant->goto_after.cdr = astChannel->cdr ? ast_cdr_dup(astChannel->cdr) : NULL;
	participant->goto_after.priority = astChannel->priority + 1;						// jump to next entry in dialplan
	pbx_channel_unlock(astChannel);

	// do not hold channel lock while creating channel / setting explicit goto              
	// use copied channel state & dialplan location to create a new channel
	if (!(participant->conferenceBridgePeer = ast_channel_alloc(0, participant->goto_after.state, 0, 0, participant->goto_after.accountcode, participant->goto_after.exten, participant->goto_after.context, participant->goto_after.linkedid, participant->goto_after.amaflags, "SCCP/CONFERENCE/%03X@%08X", participant->id, conference->id))) {
		ast_cdr_discard(participant->goto_after.cdr);
		pbx_log(LOG_ERROR, "SCCP Conference: creating conferenceBridgePeer failed.\n");
		return;
	}
	if (participant->goto_after.cdr) {
		ast_cdr_discard(participant->goto_after.cdr);
		participant->conferenceBridgePeer->cdr = participant->goto_after.cdr;
		participant->goto_after.cdr = NULL;
	}
	participant->conferenceBridgePeer->readformat = participant->goto_after.readformat;
	participant->conferenceBridgePeer->writeformat = participant->goto_after.writeformat;

	ast_explicit_goto(participant->conferenceBridgePeer, participant->goto_after.context, participant->goto_after.exten, participant->goto_after.priority);

	pbx_channel_lock(astChannel);

	/** break current bridge and swap participant->conferenceBridgePeer with astChannel */
	if (pbx_channel_masquerade(participant->conferenceBridgePeer, astChannel)) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
		PBX(requestHangup) (participant->conferenceBridgePeer);
		participant->conferenceBridgePeer = NULL;
		return;
	} else {
		pbx_log(LOG_ERROR, "SCCP Conference: masquerading conferenceBridgePeer %d (%p) into bridge.\n", participant->id, participant);
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

#    if SCCP_CONFERENCE_USE_IMPART == 1
#        if ASTERISK_VERSION_NUMBER < 11010
	ast_bridge_impart(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features);
#        else
	ast_bridge_impart(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, 1);
#        endif
#    else
	if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_join_thread, participant) < 0) {
		// \todo TODO: error handling
	}
#    endif
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

void sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel)
{
	__sccp_conference_addParticipant(conference, participantChannel, FALSE);
}

void sccp_conference_addModerator(sccp_conference_t * conference)
{
	sccp_conference_participant_t *moderator = conference->moderator;

	// We should call add Participant for moderator as well, almost completely the same
	pbx_log(LOG_NOTICE, "SCCP: Adding moderator %d (%p) to conference %d\n", moderator->id, moderator, conference->id);
	__sccp_conference_addParticipant(conference, moderator->channel, TRUE);					// add moderator

	/** update callinfo */
	sccp_device_t *d = NULL;

	if ((d = sccp_channel_getDevice_retained(moderator->channel))) {
		sprintf(moderator->channel->callInfo.callingPartyName, "Conference %d", conference->id);
		sprintf(moderator->channel->callInfo.calledPartyName, "Conference %d", conference->id);
		sccp_channel_send_callinfo(d, moderator->channel);
		sccp_dev_set_keyset(d, 0, moderator->channel->callid, KEYMODE_CONNCONF);
		d = sccp_device_release(d);
	}

	/** add to participant list */
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	SCCP_LIST_UNLOCK(&conference->participants);
}

void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{

	/* Update the presence of the moderator */
	if (participant == conference->moderator) {
		sccp_conference_end(conference);
		return;
	}

	SCCP_LIST_LOCK(&conference->participants);
	participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

#    if SCCP_CONFERENCE_USE_IMPART == 1
	ast_bridge_depart(participant->conference->bridge, participant->conferenceBridgePeer);
#    else
	ast_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
#    endif
	// continue dialplan, using the copied data
	if (ast_pbx_start(participant->conferenceBridgePeer)) {
		ast_log(LOG_WARNING, "Unable to start PBX on %s\n", participant->conferenceBridgePeer->name);
		ast_hangup(participant->conferenceBridgePeer);
		return;
	}

	participant->channel = sccp_channel_retain(participant->channel);;
	sccp_participant_release(participant);

	if (conference->participants.size < 1) {
		sccp_conference_end(conference);
	}
}

#    if SCCP_CONFERENCE_USE_IMPART == 0
static void *sccp_conference_join_thread(void *data)
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

	/* do not block channel and force hangup */
	ast_clear_flag(participant->conferenceBridgePeer, AST_FLAG_BLOCKING);
	ast_hangup(participant->conferenceBridgePeer);

	sccp_conference_removeParticipant(participant->conference, participant);

	participant->joinThread = AST_PTHREADT_NULL;
	return NULL;
}
#    endif

void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

void sccp_conference_module_start(void)
{
	SCCP_LIST_HEAD_INIT(&conferences);
}

void sccp_conference_end(sccp_conference_t * conference)
{
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
