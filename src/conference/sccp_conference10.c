
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
#include <asterisk/say.h>

#ifdef CS_SCCP_CONFERENCE

#    include "asterisk/bridging.h"
#    include "asterisk/bridging_features.h"

#    define sccp_conference_release(x) 	(sccp_conference_t *)sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    define sccp_conference_retain(x) 	(sccp_conference_t *)sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#    define sccp_participant_release(x)	(sccp_conference_participant_t *)sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    define sccp_participant_retain(x) 	(sccp_conference_participant_t *)sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;

SCCP_LIST_HEAD(, sccp_conference_t) conferences;								/*!< our list of conferences */
static void *sccp_conference_thread(void *data);
void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator);
int playback_sound_helper(sccp_conference_t * conference, const char *filename, int say_number);
sccp_conference_t *sccp_conference_findByID(uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByID(sccp_conference_t * conference, uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByChannel(sccp_conference_t * conference, sccp_channel_t * channel);

/*
 * refcount destroy functions 
 */
static void __sccp_conference_destroy(sccp_conference_t * conference)
{
	if (!conference)
		return;

#    ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_USER, "SCCPConfEnd", "ConfId: %d\r\n", conference->id);
	}
#    endif

	if (conference->playback_channel) {
		sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying conference playback channel %08x\n", conference->id);
		PBX_CHANNEL_TYPE *underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);

		pbx_hangup(underlying_channel);
		pbx_hangup(conference->playback_channel);
		conference->playback_channel = NULL;
	}

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying conference %08x\n", conference->id);
	pbx_bridge_destroy(conference->bridge);
	SCCP_LIST_HEAD_DESTROY(&conference->participants);
	pbx_mutex_destroy(&conference->playback_lock);
	return;
}

static void __sccp_conference_participant_destroy(sccp_conference_participant_t * participant)
{

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Destroying participant %d %p\n", participant->id, participant);

	if (participant->isModerator && participant->conference) {
		participant->conference->num_moderators--;
	}
	participant->conference = participant->conference ? sccp_conference_release(participant->conference) : NULL;
	participant->channel = participant->channel ? sccp_channel_release(participant->channel) : NULL;
	pbx_bridge_features_cleanup(&participant->features);
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
	conference = (sccp_conference_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_t), SCCP_REF_CONFERENCE, conferenceIdentifier, __sccp_conference_destroy);
	if (NULL == conference) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference.\n");
		return NULL;
	}

	/** initialize new conference */
	memset(conference, 0, sizeof(sccp_conference_t));
	conference->id = conferenceID;
	conference->finishing = FALSE;
	conference->isLocked = FALSE;
	SCCP_LIST_HEAD_INIT(&conference->participants);

	bridgeCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX;
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;
#    ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#    endif
	conference->bridge = pbx_bridge_new(bridgeCapabilities, AST_BRIDGE_FLAG_SMART);

	SCCP_LIST_LOCK(&conferences);
	if ((conference = sccp_conference_retain(conference))) {
		SCCP_LIST_INSERT_HEAD(&conferences, conference, list);
	}
	SCCP_LIST_UNLOCK(&conferences);

	/* create playback channel */
	pbx_mutex_init(&conference->playback_lock);

	/** we return the pointer, so do not release conference (should be retained in d->conference or rather l->conference/c->conference. d->conference limits us to one conference per phone */
	pbx_log(LOG_NOTICE, "SCCP: Conference %d created\n", conferenceID);
#    ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		sccp_device_t *d = sccp_channel_getDevice_retained(conferenceCreatorChannel);

		manager_event(EVENT_FLAG_USER, "SCCPConfStart", "ConfId: %d\r\nSCCPDevice: %s\r\n", conferenceID, DEV_ID_LOG(d));
		d = d ? sccp_device_release(d) : NULL;
	}
#    endif
	return sccp_conference_retain(conference);
}

/*! 
 * \brief Generic Function to create a new participant
 */
static sccp_conference_participant_t *sccp_conference_createParticipant(sccp_conference_t * conference, boolean_t moderator)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference) {
		pbx_log(LOG_ERROR, "SCCP Conference: no conference / participantChannel provided.\n");
		return NULL;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");

	/** create participant(moderator) */
	participant = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_PARTICIPANT, "sccp::participant", __sccp_conference_participant_destroy);

	if (!participant) {
		pbx_log(LOG_ERROR, "SCCP Conference: cannot alloc memory for new conference participant.\n");
		participant = sccp_participant_release(participant);
		return NULL;
	}

	pbx_bridge_features_init(&participant->features);
	participant->id = ++lastParticipantID;
	participant->conference = sccp_conference_retain(conference);
	participant->conferenceBridgePeer = NULL;
	participant->isModerator = moderator;

	return participant;
}

/*!
 * \brief Create a new channel and masquerade the bridge peer into the conference
 */
static int sccp_conference_masqueradeChannel(PBX_CHANNEL_TYPE * participant_ast_channel, sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	if (participant) {
		if (!(PBX(allocTempPBXChannel) (participant_ast_channel, &participant->conferenceBridgePeer))) {
			pbx_log(LOG_ERROR, "SCCP Conference: creating conferenceBridgePeer failed.\n");
			return 0;
		}

		if (!PBX(masqueradeHelper)(participant_ast_channel, participant->conferenceBridgePeer)) {
			pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
			PBX(requestHangup) (participant->conferenceBridgePeer);
#if ASTERISK_VERSION_GROUP > 106
			participant_ast_channel = ast_channel_unref(participant_ast_channel);
#endif			
			return 0;
		}

		if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_thread, participant) < 0) {
			PBX(requestHangup) (participant->conferenceBridgePeer);
			return 0;
		}
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Added Conference %d: Participant %d, bridgePeer: %s.\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));

		return 1;
	}
	return 0;
}

/*!
 * \brief Add the Participant to conference->participants
 */
static void sccp_conference_addParticipant_toList(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	// add to participant list 
	sccp_conference_participant_t *tmpParticipant = NULL;

	SCCP_LIST_LOCK(&conference->participants);
	if ((tmpParticipant = sccp_participant_retain(participant))) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Adding Participant: %d / Moderator: %s\n", tmpParticipant->id, tmpParticipant->isModerator ? "yes" : "no");
		SCCP_LIST_INSERT_TAIL(&conference->participants, tmpParticipant, list);
	}
	SCCP_LIST_UNLOCK(&conference->participants);
}

/*!
 * \brief Update the callinfo on the sccp channel
 */
static void sccp_conference_update_callInfo(sccp_conference_participant_t * participant)
{
	sccp_device_t *d = NULL;

	if (participant->channel && (d = sccp_channel_getDevice_retained(participant->channel))) {
		// update callInfo
		switch (participant->channel->calltype) {
			case SKINNY_CALLTYPE_INBOUND:
				sccp_copy_string(participant->channel->callInfo.originalCallingPartyName, participant->channel->callInfo.callingPartyName, sizeof(participant->channel->callInfo.originalCallingPartyName));
				participant->channel->callInfo.originalCallingParty_valid = 1;
				sprintf(participant->channel->callInfo.callingPartyName, "Conference %d", participant->conference->id);
				participant->channel->callInfo.callingParty_valid = 1;
				break;
			case SKINNY_CALLTYPE_OUTBOUND:
			case SKINNY_CALLTYPE_FORWARD:
				sccp_copy_string(participant->channel->callInfo.originalCalledPartyName, participant->channel->callInfo.calledPartyName, sizeof(participant->channel->callInfo.originalCallingPartyName));
				participant->channel->callInfo.originalCalledParty_valid = 1;
				sprintf(participant->channel->callInfo.calledPartyName, "Conference %d", participant->conference->id);
				participant->channel->callInfo.calledParty_valid = 1;
				break;
		}
		sccp_channel_send_callinfo(d, participant->channel);
		d = sccp_device_release(d);
	}
	pbx_log(LOG_NOTICE, "SCCP-Conference::%d: %s: %d (%p) joined the conference.\n", participant->conference->id, participant->isModerator ? "Moderator" : "Participant", participant->id, participant);
}

/*!
 * \brief Take the channel bridge peer and add it to the conference (used for the channels on hold)
 */
void sccp_conference_splitOffParticipant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	if (!conference->isLocked) {
		sccp_conference_participant_t *participant = sccp_conference_createParticipant(conference, FALSE);

		PBX_CHANNEL_TYPE *participant_ast_channel = NULL;

		participant_ast_channel = CS_AST_BRIDGED_CHANNEL(channel->owner);

		// if peer is sccp then retain peer_sccp_channel
		if ((participant->channel = get_sccp_channel_from_pbx_channel(participant_ast_channel))) {
			participant->channel = sccp_channel_retain(participant->channel);
		}
		if (sccp_conference_masqueradeChannel(participant_ast_channel, conference, participant)) {
			sccp_conference_addParticipant_toList(conference, participant);
			if (participant->channel) {
				// re-assign channel->owner to new bridged channel
				participant->channel->owner = participant->conferenceBridgePeer;
				sccp_conference_update_callInfo(participant);
			}
		} else {
			// Masq Error
			participant->channel = participant->channel ? sccp_channel_release(participant->channel) : NULL;
			participant->conference = participant->conference ? sccp_conference_release(participant->conference) : NULL;
			participant = sccp_participant_release(participant);
		}
	} else {
		pbx_stream_and_wait(channel->owner, "conf-locked", "");
	}
}

/*!
 * \brief Add both the channel->owner and the brige peer to the conference
 */
void sccp_conference_splitIntoModeratorAndParticipant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	if (!conference->isLocked) {
		sccp_conference_participant_t *participant = sccp_conference_createParticipant(conference, FALSE);
		sccp_conference_participant_t *moderator = sccp_conference_createParticipant(conference, TRUE);

		pbx_log(LOG_NOTICE, "SCCP: Adding moderator %s and participant %s to conference %d\n", pbx_channel_name(channel->owner), pbx_channel_name(CS_AST_BRIDGED_CHANNEL(channel->owner)), conference->id);

		if (moderator && participant) {
			// handle participant first (channel->owner->bridge side)
			sccp_conference_splitOffParticipant(conference, channel);

			// handle moderator (channel->owner side)
			PBX_CHANNEL_TYPE *moderator_ast_channel = NULL;

			moderator_ast_channel = channel->owner;

			// if peer is sccp then retain peer_sccp_channel
			if ((moderator->channel = get_sccp_channel_from_pbx_channel(moderator_ast_channel))) {
				moderator->channel = sccp_channel_retain(moderator->channel);
			}
			if (sccp_conference_masqueradeChannel(moderator_ast_channel, conference, moderator)) {
				sccp_conference_addParticipant_toList(conference, moderator);
				if (moderator->channel) {
					// re-assign channel->owner to newly bridged channel
					moderator->channel->owner = moderator->conferenceBridgePeer;
					sccp_conference_update_callInfo(moderator);
				}
				// update callinfo
				sccp_device_t *d = NULL;

				if ((d = sccp_channel_getDevice_retained(channel))) {
					sprintf(channel->callInfo.callingPartyName, "Conference %d", conference->id);
					sprintf(channel->callInfo.calledPartyName, "Conference %d", conference->id);
					sccp_channel_send_callinfo(d, channel);
					sccp_dev_set_keyset(d, 0, channel->callid, KEYMODE_CONNCONF);
					d = sccp_device_release(d);
				}
				conference->num_moderators++;
				playback_sound_helper(conference, "conf-enteringno", -1);
				playback_sound_helper(conference, NULL, conference->id);
			} else {
				// Masq Error
				moderator->channel = moderator->channel ? sccp_channel_release(moderator->channel) : NULL;
				moderator->conference = moderator->conference ? sccp_conference_release(moderator->conference) : NULL;
				moderator = sccp_participant_release(moderator);
			}
		}

	} else {
		pbx_stream_and_wait(channel->owner, "conf-locked", "");
	}
}

/*!
 * \brief Remove a specific participant from a conference
 */
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_conference_participant_t *tmp_participant = NULL;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: (sccp_conference_removeParticipant) Moderators: %d, Participants: %d, Participant: %d, Moderator: %s, Finishing: %s\n", conference->id, conference->num_moderators, SCCP_LIST_GETSIZE(conference->participants), participant->id, participant->isModerator ? "YES" : "NO", conference->finishing ? "YES" : "NO");

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: removing Participant %d.\n", conference->id, participant->id);
	SCCP_LIST_LOCK(&conference->participants);
	tmp_participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	tmp_participant = sccp_participant_release(tmp_participant);
	SCCP_LIST_UNLOCK(&conference->participants);

	/* if last moderator is leaving, end conference */
	if (participant->isModerator && conference->num_moderators == 1 && !conference->finishing) {
		playback_sound_helper(conference, "conf-leaderhasleft", -1);
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: Issue Conference End 1\n", conference->id);
		sccp_conference_end(conference);
	} else {
		playback_sound_helper(conference, NULL, participant->id);
		playback_sound_helper(conference, "conf-hasleft", -1);
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: hanging up Participant %d, bridgePeer: %s.\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));
#    ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfLeave", "ConfId: %d\r\nPartId: %d\r\nChannel: %s\r\nUniqueid: %s\r\n", conference->id, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
	}
#    endif
	pbx_clear_flag(pbx_channel_flags(participant->conferenceBridgePeer), AST_FLAG_BLOCKING);
	pbx_hangup(participant->conferenceBridgePeer);
	participant = sccp_participant_release(participant);

	/* Conference end if the number of participants == 1 */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: (sccp_conference_removeParticipant) Moderators: %d, Participants: %d, Finishing: %s\n", conference->id, conference->num_moderators, SCCP_LIST_GETSIZE(conference->participants), conference->finishing ? "YES" : "NO");
	if (SCCP_LIST_GETSIZE(conference->participants) == 1 && !conference->finishing) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: No more conference participant, ending conference.\n");
		playback_sound_helper(conference, "conf-leaderhasleft", -1);
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

#    	 ifdef CS_MANAGER_EVENTS
	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering pbx_bridge_join: %s as %d\n", pbx_channel_name(participant->conferenceBridgePeer), participant->id);
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfEnter", "ConfId: %d\r\nPartId: %d\r\nChannel: %s\r\nUniqueid: %s\r\n", participant->conference->id, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
	}
#    	 endif

#    if ASTERISK_VERSION_NUMBER < 11010
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features);
#    else
	pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, NULL);
#    endif
	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving pbx_bridge_join: %s as %d\n", pbx_channel_name(participant->conferenceBridgePeer), participant->id);

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

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: Ending conference.\n", conference->id);
	SCCP_LIST_LOCK(&conferences);
	conference->finishing = TRUE;
	SCCP_LIST_UNLOCK(&conferences);

	/* remove remaining participants / moderators */
	SCCP_LIST_LOCK(&conference->participants);
	if (SCCP_LIST_GETSIZE(conference->participants) > 0) {
		// remove the participants first
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (!participant->isModerator) {
				sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: Remove participant %d from bridge.\n", conference->id, participant->id);
				pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
			}
		}
		// and then remove the moderators
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (participant->isModerator) {
				sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: Remove moderator %d from bridge.\n", conference->id, participant->id);
				pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
			}
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);

	/* remove conference */
	sccp_conference_t *tmp_conference = NULL;

	SCCP_LIST_LOCK(&conferences);
	tmp_conference = SCCP_LIST_REMOVE(&conferences, conference, list);
	tmp_conference = sccp_conference_release(tmp_conference);
	SCCP_LIST_UNLOCK(&conferences);
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: Conference Ended.\n", conference->id);

	conference = sccp_conference_release(conference);
}

/*!
 * \brief This function is used to playback either a file or number sequence to all conference participants. Used for announcing
 * The playback channel is created once, and imparted on the conference when necessary on follow-up calls
 */
int playback_sound_helper(sccp_conference_t * conference, const char *filename, int say_number)
{
	PBX_CHANNEL_TYPE *underlying_channel;

	pbx_mutex_lock(&conference->playback_lock);

	if (!sccp_strlen_zero(filename) && !pbx_fileexists(filename, NULL, NULL)) {
		pbx_log(LOG_WARNING, "File %s does not exists in any format\n", !sccp_strlen_zero(filename) ? filename : "<unknown>");
		return 0;
	}

	if (!(conference->playback_channel)) {
		if (!(conference->playback_channel = PBX(requestForeignChannel) ("Bridge", AST_FORMAT_SLINEAR, NULL, ""))) {
			pbx_mutex_unlock(&conference->playback_lock);
			return 0;
		}

		pbx_channel_set_bridge(conference->playback_channel, conference->bridge);

		if (ast_call(conference->playback_channel, "", 0)) {
			pbx_hangup(conference->playback_channel);
			conference->playback_channel = NULL;
			pbx_mutex_unlock(&conference->playback_lock);
			return 0;
		}

		ast_debug(1, "Created a playback channel on conference '%d'\n", conference->id);

		underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);
	} else {
		/* Channel was already available so we just need to add it back into the bridge */
		underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);
#    if ASTERISK_VERSION_GROUP < 110
		pbx_bridge_impart(conference->bridge, underlying_channel, NULL, NULL);
#    else
		pbx_bridge_impart(conference->bridge, underlying_channel, NULL, NULL, 0);
#    endif
	}

	if (conference->playback_channel) {
		if (!sccp_strlen_zero(filename)) {
			sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_4 "CONFERENCE %d: Playing '%s'\n", conference->id, filename);
			pbx_stream_and_wait(conference->playback_channel, filename, "");
		} else if (say_number >= 0) {
			sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_4 "CONFERENCE %d: Say '%d'\n", conference->id, say_number);
			pbx_say_number(conference->playback_channel, say_number, "", pbx_channel_language(conference->playback_channel), NULL);
		}
	}

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "Departing underlying channel '%s' from conference '%d'\n", pbx_channel_name(underlying_channel), conference->id);
	pbx_bridge_depart(conference->bridge, underlying_channel);
	pbx_mutex_unlock(&conference->playback_lock);
	return 1;
}

/* conf list related */
sccp_conference_t *sccp_conference_findByID(uint32_t identifier)
{
	sccp_conference_t *conference = NULL;

	if (identifier < 0) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_TRAVERSE(&conferences, conference, list) {
		if (conference->id == identifier) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conferences);
	return sccp_conference_retain(conference);
}

sccp_conference_participant_t *sccp_conference_participant_findByID(sccp_conference_t * conference, uint32_t identifier)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference || identifier < 0) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->id == identifier) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return sccp_participant_retain(participant);
}

sccp_conference_participant_t *sccp_conference_participant_findByChannel(sccp_conference_t * conference, sccp_channel_t * channel)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference || !channel) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->channel == channel) {
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return sccp_participant_retain(participant);
}

void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * c)
{
	int use_icon = 0;
	uint32_t appID = APPID_CONFERENCE;
	uint32_t callReference = 1;						// callreference should be asterisk_channel->unique identifier
	sccp_conference_participant_t *participant = NULL;
	sccp_channel_t *channel = NULL;
	sccp_device_t *device = NULL;

	if (!conference) {
		pbx_log(LOG_WARNING, "SCCP: No conference available to display list for\n");
		goto exit_function;
	}

	if (!(channel = sccp_channel_retain(c))) {								// only send this list to sccp phones
		pbx_log(LOG_WARNING, "CONF-%d: No channel available to display conferencelist for\n", conference->id);
		goto exit_function;
	}

	if (!(participant = sccp_conference_participant_findByChannel(conference, channel))) {
		pbx_log(LOG_WARNING, "CONF-%d: Channel %s is not a participant in this conference\n", conference->id, pbx_channel_name(channel->owner));
		goto exit_function;
	}
	if (conference->participants.size < 1) {
		pbx_log(LOG_WARNING, "CONF-%d: Conference does not have enough participants\n", conference->id);
		goto exit_function;
	}
//      sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Sending ConferenceList to Channel: %d %s / Participant: %d / moderator: %s\n", channel->currentDeviceId, channel->callid, channel->owner->name,participant->id, participant->isModerator ? "yes" : "no");

#    if ASTERISK_VERSION_NUMBER >= 10400
	unsigned int transactionID = pbx_random();
#    else
	unsigned int transactionID = random();
#    endif

	char xmlStr[2048] = "";
	char xmlTmp[512] = "";

	strcat(xmlStr, "<CiscoIPPhoneIconMenu>\n");								// Will be CiscoIPPhoneIconMenu with icons for participant, moderator, muted, unmuted
	strcat(xmlStr, "<Title>Conference List</Title>\n");
	strcat(xmlStr, "<Prompt>Make Your Selection</Prompt>\n");

	// MenuItems

	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		if (part->pendingRemoval)
			continue;

		if (part->channel && (device = sccp_channel_getDevice_retained(part->channel)) && part->channel == channel && !device->conferencelist_active) {
			sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: CONFLIST ACTIVED %d %d\n", channel->currentDeviceId, channel->callid, part->id);
			device->conferencelist_active = TRUE;
			device = sccp_device_release(device);
		}
		strcat(xmlStr, "<MenuItem>\n");

		if (part->isModerator)
			use_icon = 0;
		else
			use_icon = 2;

		if (part->isMuted) {
			++use_icon;
		}
		strcat(xmlStr, "  <IconIndex>");
		sprintf(xmlTmp, "%d", use_icon);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</IconIndex>\n");

		strcat(xmlStr, "  <Name>");
		if (part->channel != NULL) {
			switch (part->channel->calltype) {
				case SKINNY_CALLTYPE_INBOUND:
					sprintf(xmlTmp, "%d:%s (%s)", part->id, part->channel->callInfo.calledPartyName, part->channel->callInfo.calledPartyNumber);
					break;
				case SKINNY_CALLTYPE_OUTBOUND:
					sprintf(xmlTmp, "%d:%s (%s)", part->id, part->channel->callInfo.callingPartyName, part->channel->callInfo.callingPartyNumber);
					break;
				case SKINNY_CALLTYPE_FORWARD:
					sprintf(xmlTmp, "%d:%s (%s)", part->id, part->channel->callInfo.originalCallingPartyName, part->channel->callInfo.originalCallingPartyName);
					break;
			}
			strcat(xmlStr, xmlTmp);
		} else {											// Asterisk Channel
			sprintf(xmlTmp, "%d:%s (%s)", part->id, "Dummy CallerIdName", "Dummy CallerIdNumber");
			strcat(xmlStr, xmlTmp);
		}
		strcat(xmlStr, "</Name>\n");
		sprintf(xmlTmp, "  <URL>UserCallData:%d:%d:%d:%d:%d</URL>", appID, conference->id, callReference, transactionID, part->id);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</MenuItem>\n");
	}
	SCCP_LIST_UNLOCK(&conference->participants);

	// SoftKeys
#    if CS_EXPERIMENTAL
	if (participant->isModerator) {
		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>Invite</Name>\n");
		strcat(xmlStr, "  <Position>1</Position>\n");
		sprintf(xmlTmp, "  <URL>UserDataSoftKey:Select:%d:INVITE$%d$%d$%d$</URL>\n", 1, appID, conference->id, transactionID);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</SoftKeyItem>\n");

		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>Mute</Name>");
		strcat(xmlStr, "  <Position>2</Position>\n");
		sprintf(xmlTmp, "  <URL>UserDataSoftKey:Select:%d:MUTE$%d$%d$%d$</URL>\n", 2, appID, conference->id, transactionID);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</SoftKeyItem>\n");

		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>Kick</Name>\n");
		strcat(xmlStr, "  <Position>3</Position>\n");
		sprintf(xmlTmp, "  <URL>UserDataSoftKey:Select:%d:KICK$%d$%d$%d$</URL>", 3, appID, conference->id, transactionID);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</SoftKeyItem>\n");
	}
#    endif
	strcat(xmlStr, "<SoftKeyItem>\n");
	strcat(xmlStr, "  <Name>Exit</Name>\n");
	strcat(xmlStr, "  <Position>4</Position>\n");
	strcat(xmlStr, "  <URL>SoftKey:Exit</URL>\n");
	strcat(xmlStr, "</SoftKeyItem>\n");

	// CiscoIPPhoneIconMenu Icons
	strcat(xmlStr, "<IconItem>\n");										// moderator
	strcat(xmlStr, "  <Index>0</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C03F3000C03FF000C03FF003000FF00FFCFFF30FFCFFF303CC3FF300CC3F330000000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");										// muted moderator
	strcat(xmlStr, "  <Index>1</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C03FF03CC03FF03CC03FF03C000FF03CFCFFF33CFCFFF33CCC3FF33CCC3FF33C00000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");										// participant
	strcat(xmlStr, "  <Index>2</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C0303000C030F000C030F003000FF00FFCF0F30F0C00F303CC30F300CC30330000000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");										// muted participant
	strcat(xmlStr, "  <Index>3</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C030F03CC030F03CC030F03C000FF03CFCF0F33C0C00F33CCC30F33CCC30F33C00000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "</CiscoIPPhoneIconMenu>\n");
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "xml-message:\n%s\n", xmlStr);

	if ((device = sccp_channel_getDevice_retained(channel))) {
	        device->protocol->sendUserToDeviceDataVersionMessage(device, appID, conference->id, callReference, transactionID, xmlStr, 2);
                device = sccp_device_release(device);
	}

 exit_function:
	participant = participant ? sccp_participant_release(participant) : NULL;
	channel = channel ? sccp_channel_release(channel) : NULL;
}

void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
#    if CS_EXPERIMENTAL
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;

	if (d && d->dtu_softkey.transactionID == transactionID) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle DTU SoftKey Button Press for CallID %d, Transaction %d, Conference %d, Participant:%d, Action %s\n", d->id, callReference, transactionID, conferenceID, participantID, d->dtu_softkey.action);

		if (!(conference = sccp_conference_findByID(conferenceID))) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference not found\n", d->id);
			goto EXIT;
		}
		if (!(participant = sccp_conference_participant_findByID(conference, participantID))) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Participant not found\n", d->id);
			goto EXIT;
		}

		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: DTU Softkey Action %s\n", d->id, d->dtu_softkey.action);
		if (!strcmp(d->dtu_softkey.action, "INVITE")) {
			sccp_conference_show_list(conference, participant->channel);
			sccp_conference_invite_participant(conference, participant->channel);
		} else if (!strcmp(d->dtu_softkey.action, "EXIT")) {
			d->conferencelist_active = FALSE;
		} else if (!strcmp(d->dtu_softkey.action, "MUTE")) {
			sccp_conference_toggle_mute_participant(conference, participant);
		} else if (!strcmp(d->dtu_softkey.action, "KICK")) {
			if (participant->isModerator) {
				sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Since wenn du we kick ourselves ? You've got issues!\n", d->id);
			} else {
				sccp_conference_kick_participant(conference, participant);
			}
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: DTU TransactionID does not match or device not found (%d <> %d)\n", d->id, d->dtu_softkey.transactionID, transactionID);
	}
 EXIT:
	/* reset softkey state for next button press */
	d->dtu_softkey.action = "";
	d->dtu_softkey.appID = 0;
	d->dtu_softkey.payload = 0;
	d->dtu_softkey.transactionID = 0;
	participant = participant ? sccp_participant_release(participant) : NULL;
	conference = conference ? sccp_conference_release(conference) : NULL;
#    endif
}

void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	ast_stopstream(participant->conferenceBridgePeer);
	pbx_stream_and_wait(participant->conferenceBridgePeer, "conf-kicked", "");
	pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
}

void sccp_conference_toggle_lock_conference(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	conference->isLocked = (!conference->isLocked ? 1 : 0);
	pbx_stream_and_wait(participant->conferenceBridgePeer, (conference->isLocked ? "conf-lockednow" : "conf-unlockednow"), "");
}

void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	participant->isMuted = (!participant->isMuted ? 1 : 0);
	pbx_stream_and_wait(participant->conferenceBridgePeer, (participant->isMuted ? "conf-muted" : "conf-unmuted"), "");
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
