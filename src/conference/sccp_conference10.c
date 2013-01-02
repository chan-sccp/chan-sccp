
/*!
 * \file        sccp_conference.c
 * \brief       SCCP Conference for asterisk 10
 * \author      Marcello Ceschia <marcelloceschia [at] users.sorceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "../common.h"
#include <asterisk/say.h>

#ifdef CS_SCCP_CONFERENCE

#include "asterisk/bridging.h"
#include "asterisk/bridging_features.h"

#define sccp_conference_release(x) 	(sccp_conference_t *)sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_conference_retain(x) 	(sccp_conference_t *)sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define sccp_participant_release(x)	(sccp_conference_participant_t *)sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_participant_retain(x) 	(sccp_conference_participant_t *)sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

SCCP_FILE_VERSION(__FILE__, "$Revision$")

static int lastConferenceID = 99;
static int lastParticipantID = 0;

SCCP_LIST_HEAD (, sccp_conference_t) conferences;								/*!< our list of conferences */
static void *sccp_conference_thread(void *data);
void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel, boolean_t moderator);
int playback_to_channel(sccp_conference_participant_t * participant, const char *filename, int say_number);
int playback_to_conference(sccp_conference_t * conference, const char *filename, int say_number);
sccp_conference_t *sccp_conference_findByID(uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByID(sccp_conference_t * conference, uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByChannel(sccp_conference_t * conference, sccp_channel_t * channel);
sccp_conference_participant_t *sccp_conference_participant_findByPBXChannel(sccp_conference_t * conference, PBX_CHANNEL_TYPE * channel);

/*
 * refcount destroy functions 
 */
static void __sccp_conference_destroy(sccp_conference_t * conference)
{
	if (!conference)
		return;

	if (conference->playback_channel) {
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Destroying conference playback channel\n", conference->id);
		PBX_CHANNEL_TYPE *underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);

		pbx_hangup(underlying_channel);
		pbx_hangup(conference->playback_channel);
		conference->playback_channel = NULL;
	}
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Destroying conference\n", conference->id);
	pbx_bridge_destroy(conference->bridge);
	SCCP_LIST_HEAD_DESTROY(&conference->participants);
	pbx_mutex_destroy(&conference->playback_lock);

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_USER, "SCCPConfEnd", "ConfId: %d\r\n", conference->id);
	}
#endif
	return;
}

static void __sccp_conference_participant_destroy(sccp_conference_participant_t * participant)
{

	sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Destroying participant %d %p\n", participant->conference->id, participant->id, participant);

	if (participant->isModerator && participant->conference) {
		participant->conference->num_moderators--;
	}
	pbx_bridge_features_cleanup(&participant->features);
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfLeave", "ConfId: %d\r\n" "PartId: %d\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", participant->conference ? participant->conference->id : -1, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
	}
#endif
	if (participant->channel) {
		participant->channel->conference_id = 0;
		participant->channel->conference_participant_id = 0;
		participant->channel = sccp_channel_release(participant->channel);
	}
	participant->conference = participant->conference ? sccp_conference_release(participant->conference) : NULL;
	return;
}

/*!
 * \brief Create a new conference on sccp_channel_t 
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t * conferenceCreatorChannel)
{
	sccp_conference_t *conference = NULL;
	char conferenceIdentifier[REFCOUNT_INDENTIFIER_SIZE];
	int conferenceID = ++lastConferenceID;
	uint32_t bridgeCapabilities;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCP: Creating new conference SCCPCONF/%04d\n", conferenceID);

	/** create conference */
	snprintf(conferenceIdentifier, REFCOUNT_INDENTIFIER_SIZE, "SCCPCONF/%04d", conferenceID);
	conference = (sccp_conference_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_t), SCCP_REF_CONFERENCE, conferenceIdentifier, __sccp_conference_destroy);
	if (NULL == conference) {
		pbx_log(LOG_ERROR, "SCCPCONF/%04d: cannot alloc memory for new conference.\n", conferenceID);
		return NULL;
	}

	/** initialize new conference */
	memset(conference, 0, sizeof(sccp_conference_t));
	conference->id = conferenceID;
	conference->finishing = FALSE;
	conference->isLocked = FALSE;
	SCCP_LIST_HEAD_INIT(&conference->participants);

	//      bridgeCapabilities = AST_BRIDGE_CAPABILITY_1TO1MIX;
	bridgeCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX;
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;
#ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#endif
	conference->bridge = pbx_bridge_new(bridgeCapabilities, AST_BRIDGE_FLAG_SMART);

	/*
	   pbx_bridge_set_internal_sample_rate(conference_bridge->bridge, auto);
	   pbx_bridge_set_mixing_interval(conference->bridge,40);
	 */

	SCCP_LIST_LOCK(&conferences);
	if ((conference = sccp_conference_retain(conference))) {
		SCCP_LIST_INSERT_HEAD(&conferences, conference, list);
	}
	SCCP_LIST_UNLOCK(&conferences);

	/* create playback channel */
	pbx_mutex_init(&conference->playback_lock);

	/** we return the pointer, so do not release conference (should be retained in d->conference or rather l->conference/c->conference. d->conference limits us to one conference per phone */
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		sccp_device_t *d = NULL;

		if ((d = sccp_channel_getDevice_retained(conferenceCreatorChannel))) {
			manager_event(EVENT_FLAG_USER, "SCCPConfStart", "ConfId: %d\r\n" "SCCPDevice: %s\r\n", conferenceID, DEV_ID_LOG(d)
			    );
			d = sccp_device_release(d);
		}
	}
#endif
	conference = sccp_conference_retain(conference);							// return retained
	return conference;
}

/*! 
 * \brief Generic Function to create a new participant
 */
static sccp_conference_participant_t *sccp_conference_createParticipant(sccp_conference_t * conference, boolean_t moderator)
{
	sccp_conference_participant_t *participant = NULL;
	int participantID = ++lastParticipantID;
	char participantIdentifier[REFCOUNT_INDENTIFIER_SIZE];

	if (!conference) {
		pbx_log(LOG_ERROR, "SCCPCONF: no conference / participantChannel provided.\n");
		return NULL;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Creating new conference-partcipant %d\n", conference->id, participantID);

	/** create participant(moderator) */
	snprintf(participantIdentifier, REFCOUNT_INDENTIFIER_SIZE, "SCCPCONF/%04d/PART/%04d", conference->id, participantID);
	participant = (sccp_conference_participant_t *) sccp_refcount_object_alloc(sizeof(sccp_conference_participant_t), SCCP_REF_PARTICIPANT, participantIdentifier, __sccp_conference_participant_destroy);

	if (!participant) {
		pbx_log(LOG_ERROR, "SCCPCONF/%04d: cannot alloc memory for new conference participant.\n", conference->id);
		participant = sccp_participant_release(participant);
		return NULL;
	}

	pbx_bridge_features_init(&participant->features);
	participant->id = participantID;
	participant->conference = sccp_conference_retain(conference);
	participant->conferenceBridgePeer = NULL;
	participant->isModerator = moderator;

	return participant;
}

/* Using a bit of a trick to get at the ast_bridge_channel, there is no remote function provided by asterisk */
/* should we use asterisk ref / unref for the bridge_channel reference */
static void sccp_conference_connect_bridge_channels_to_participants(sccp_conference_t * conference)
{
	struct ast_bridge_channel *bridge_channel = NULL;
	sccp_conference_participant_t *participant = NULL;

	AST_LIST_TRAVERSE(&conference->bridge->channels, bridge_channel, entry) {
		participant = sccp_conference_participant_findByPBXChannel(conference, bridge_channel->chan);
		if (conference->mute_on_entry)
			participant->features.mute = 1;
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Connecting Bridge Channel %p to Participant %d.\n", conference->id, bridge_channel, participant->id);
		participant->bridge_channel = bridge_channel;
	}
}

/*!
 * \brief Create a new channel and masquerade the bridge peer into the conference
 */
static int sccp_conference_masqueradeChannel(PBX_CHANNEL_TYPE * participant_ast_channel, sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	if (participant) {
		if (!(PBX(allocTempPBXChannel) (participant_ast_channel, &participant->conferenceBridgePeer))) {
			pbx_log(LOG_ERROR, "SCCPCONF/%04d: Creation of Temp Channel Failed. Exiting.\n", conference->id);
			return 0;
		}

		if (!PBX(masqueradeHelper) (participant_ast_channel, participant->conferenceBridgePeer)) {
			pbx_log(LOG_ERROR, "SCCPCONF/%04d: Failed to Masquerade TempChannel.\n", conference->id);
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
		//              usleep(200);
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Added Participant %d (Channel: %s)\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));

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
		// update callInfo for SCCP Channels
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
	} else {
		// update callinfo foreign channels
		/*! \todo Conference Handle CallInfo Foreign Channel Types" */
	}
}

/* has to be moved to pbx_impl */
void pbx_builtin_setvar_int_helper(PBX_CHANNEL_TYPE * channel, const char *var_name, int intvalue)
{
	char valuestr[8] = "";

	snprintf(valuestr, 8, "%d", intvalue);
	pbx_builtin_setvar_helper(channel, var_name, valuestr);

}

/*!
 * \brief Take the channel bridge peer and add it to the conference (used for the channels on hold)
 */
void sccp_conference_splitOffParticipant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	if (!conference->isLocked) {
		sccp_conference_participant_t *participant = sccp_conference_createParticipant(conference, FALSE);

		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Adding participant %d (Channel %s)\n", conference->id, participant->id, pbx_channel_name(channel->owner));

		if (participant) {
			PBX_CHANNEL_TYPE *participant_ast_channel = NULL;

			participant_ast_channel = CS_AST_BRIDGED_CHANNEL(channel->owner);

			sccp_device_t *d = NULL;

			if ((d = sccp_channel_getDevice_retained(channel))) {
				//! \todo We need to get the from a channel which is on hold. Need to figure this out.
				//                              participant->playback_announcements = d->conf_play_part_announce;
				participant->playback_announcements = TRUE;
			}
			// if peer is sccp then retain peer_sccp_channel
			participant->channel = get_sccp_channel_from_pbx_channel(participant_ast_channel);
			if (sccp_conference_masqueradeChannel(participant_ast_channel, conference, participant)) {
				sccp_conference_addParticipant_toList(conference, participant);
				if (participant->channel) {
					// re-assign channel->owner to new bridged channel
					participant->channel->owner = participant->conferenceBridgePeer;
					participant->channel->conference = sccp_conference_retain(conference);
					participant->channel->conference_id = conference->id;
					participant->channel->conference_participant_id = participant->id;
					sccp_conference_update_callInfo(participant);
				}
				pbx_builtin_setvar_int_helper(participant->conferenceBridgePeer, "__SCCP_CONFERENCE_ID", conference->id);
				pbx_builtin_setvar_int_helper(participant->conferenceBridgePeer, "__SCCP_CONFERENCE_PARTICIPANT_ID", participant->id);
			} else {
				// Masq Error
				participant->channel = participant->channel ? sccp_channel_release(participant->channel) : NULL;
				participant->conference = participant->conference ? sccp_conference_release(participant->conference) : NULL;
				participant = sccp_participant_release(participant);
			}
			d = d ? sccp_device_release(d) : NULL;
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Conference is locked. Participant Denied.\n", conference->id);
		pbx_stream_and_wait(channel->owner, "conf-locked", "");
	}
}

// Added to resume after using the "JOIN" Button.
/*
   void sccp_conference_resume(sccp_conference_t * conference) {
   sccp_conference_participant_t *participant = NULL;
   sccp_device_t *d = NULL;

   if (!conference) {
   return;
   }
   SCCP_LIST_LOCK(&conference->participants);
   SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
   if (ast_test_flag(participant->conferenceBridgePeer, AST_FLAG_MOH)) {
   if (participant->channel && (d = sccp_channel_getDevice_retained(participant->channel))) {
   sccp_channel_resume(d, participant->channel, FALSE);
   d = sccp_device_release(d);
   } else {
   PBX(queue_control) (participant->conferenceBridgePeer, AST_CONTROL_UNHOLD);
   }
   }

   }
   SCCP_LIST_UNLOCK(&conference->participants);
   }
 */
/*!
 * \brief Add both the channel->owner and the brige peer to the conference
 */
void sccp_conference_splitIntoModeratorAndParticipant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	sccp_device_t *d = NULL;

	if (!conference->isLocked) {
		sccp_conference_participant_t *moderator = sccp_conference_createParticipant(conference, TRUE);

		if (moderator) {
			// handle participant first (channel->owner->bridge side)
			sccp_conference_splitOffParticipant(conference, channel);

			// handle moderator (channel->owner side)
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Adding participant %d (%s) as Moderator\n", conference->id, moderator->id, pbx_channel_name(channel->owner));
			PBX_CHANNEL_TYPE *moderator_ast_channel = NULL;

			moderator_ast_channel = channel->owner;

			// if peer is sccp then retain peer_sccp_channel
			moderator->channel = get_sccp_channel_from_pbx_channel(moderator_ast_channel);
			// update callinfo
			if ((d = sccp_channel_getDevice_retained(channel))) {
				conference->mute_on_entry = d->conf_mute_on_entry;
				conference->playback_announcements = d->conf_play_general_announce;
				moderator->playback_announcements = d->conf_play_part_announce;
				if (sccp_conference_masqueradeChannel(moderator_ast_channel, conference, moderator)) {
					sccp_conference_addParticipant_toList(conference, moderator);
					if (moderator->channel) {
						// re-assign channel->owner to newly bridged channel
						moderator->channel->owner = moderator->conferenceBridgePeer;
						moderator->channel->conference = sccp_conference_retain(conference);
						moderator->channel->conference_id = conference->id;
						moderator->channel->conference_participant_id = moderator->id;
						sccp_conference_update_callInfo(moderator);
					}
					sccp_copy_string(conference->playback_language, pbx_channel_language(moderator->conferenceBridgePeer), sizeof(conference->playback_language));
					sprintf(channel->callInfo.callingPartyName, "Conference %d", conference->id);
					sprintf(channel->callInfo.calledPartyName, "Conference %d", conference->id);
					sccp_channel_send_callinfo(d, channel);
					sccp_dev_set_keyset(d, 0, channel->callid, KEYMODE_CONNCONF);
					conference->num_moderators++;
					playback_to_conference(conference, "conf-enteringno", -1);
					playback_to_conference(conference, NULL, conference->id);
					sccp_conference_connect_bridge_channels_to_participants(conference);
					//                                      sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Moderator Bridge_channel %p, ChannelName %s\n", conference->id, moderator->bridge_channel, moderator->bridge_channel ? moderator->bridge_channel->chan->name : "NULL");
					pbx_builtin_setvar_int_helper(moderator->conferenceBridgePeer, "__SCCP_CONFERENCE_ID", conference->id);
					pbx_builtin_setvar_int_helper(moderator->conferenceBridgePeer, "__SCCP_CONFERENCE_PARTICIPANT_ID", moderator->id);
				} else {
					// Masq Error
					moderator->channel = moderator->channel ? sccp_channel_release(moderator->channel) : NULL;
					moderator->conference = moderator->conference ? sccp_conference_release(moderator->conference) : NULL;
					moderator = sccp_participant_release(moderator);
				}
				d = sccp_device_release(d);
			}
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Conference is locked. Participant Denied.\n", conference->id);
		pbx_stream_and_wait(channel->owner, "conf-locked", "");
	}
}

/*!
 * \brief Remove a specific participant from a conference
 */
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_conference_participant_t *tmp_participant = NULL;

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Removing Participant %d.\n", conference->id, participant->id);
	SCCP_LIST_LOCK(&conference->participants);
	tmp_participant = SCCP_LIST_REMOVE(&conference->participants, participant, list);
	tmp_participant = sccp_participant_release(tmp_participant);
	SCCP_LIST_UNLOCK(&conference->participants);

	/* if last moderator is leaving, end conference */
	if (participant->isModerator && conference->num_moderators == 1 && !conference->finishing) {
		sccp_conference_end(conference);
	} else if (!participant->isModerator && !conference->finishing) {
		playback_to_conference(conference, NULL, participant->id);
		playback_to_conference(conference, "conf-hasleft", -1);
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Hanging up Participant %d (Channel: %s)\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));
	pbx_clear_flag(pbx_channel_flags(participant->conferenceBridgePeer), AST_FLAG_BLOCKING);
	pbx_hangup(participant->conferenceBridgePeer);
	participant = sccp_participant_release(participant);

	/* Conference end if the number of participants == 1 */
	if (SCCP_LIST_GETSIZE(conference->participants) == 1 && !conference->finishing) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: There are no conference participants left, Ending conference.\n", conference->id);
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

	/** cast data to conference_participant */
	participant = (sccp_conference_participant_t *) data;

	if ((participant = sccp_participant_retain(participant))) {
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: entering join thread.\n", participant->conference->id);
#ifdef CS_MANAGER_EVENTS
		if (GLOB(callevents)) {
			manager_event(EVENT_FLAG_CALL, "SCCPConfEnter", "ConfId: %d\r\n" "PartId: %d\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", participant->conference->id, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
		}
#endif
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Entering pbx_bridge_join: %s as %d\n", participant->conference->id, pbx_channel_name(participant->conferenceBridgePeer), participant->id);

#if ASTERISK_VERSION_NUMBER < 11010
		pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features);
#else
		pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, NULL);
#endif
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Leaving pbx_bridge_join: %s as %d\n", participant->conference->id, pbx_channel_name(participant->conferenceBridgePeer), participant->id);

		sccp_conference_removeParticipant(participant->conference, participant);
		participant->joinThread = AST_PTHREADT_NULL;
		participant = sccp_participant_release(participant);
	}

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

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Ending Conference.\n", conference->id);
	SCCP_LIST_LOCK(&conferences);
	conference->finishing = TRUE;
	SCCP_LIST_UNLOCK(&conferences);

	playback_to_conference(conference, "conf-leaderhasleft", -1);

	/* remove remaining participants / moderators */
	SCCP_LIST_LOCK(&conference->participants);
	if (SCCP_LIST_GETSIZE(conference->participants) > 0) {
		// remove the participants first
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (!participant->isModerator) {
				pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
			}
		}
		// and then remove the moderators
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (participant->isModerator) {
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
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Conference Ended.\n", conference->id);

	conference = sccp_conference_release(conference);
}

static int stream_and_wait(PBX_CHANNEL_TYPE * playback_channel, const char *filename, int say_number)
{
	if (!sccp_strlen_zero(filename) && !pbx_fileexists(filename, NULL, NULL)) {
		pbx_log(LOG_WARNING, "File %s does not exists in any format\n", !sccp_strlen_zero(filename) ? filename : "<unknown>");
		return 0;
	}
	if (playback_channel) {
		if (!sccp_strlen_zero(filename)) {
			sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Playing '%s' to Conference\n", filename);
			pbx_stream_and_wait(playback_channel, filename, "");
		} else if (say_number >= 0) {
			sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "Saying '%d' to Conference\n", say_number);
			pbx_say_number(playback_channel, say_number, "", pbx_channel_language(playback_channel), NULL);
		}
	}
	return 1;
}

int playback_to_channel(sccp_conference_participant_t * participant, const char *filename, int say_number)
{
	int res = 0;

	if (!participant->playback_announcements) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Playback for participant %d suppressed\n", participant->conference->id, participant->id);
		return 1;
	}
	if (participant->bridge_channel) {
		participant->bridge_channel->suspended = 1;
		ast_bridge_change_state(participant->bridge_channel, AST_BRIDGE_CHANNEL_STATE_WAIT);
		if (stream_and_wait(participant->bridge_channel->chan, filename, say_number)) {
			res = 1;
		} else {
			pbx_log(LOG_WARNING, "Failed to play %s or '%d'!\n", filename, say_number);
		}
		participant->bridge_channel->suspended = 0;
		ast_bridge_change_state(participant->bridge_channel, AST_BRIDGE_CHANNEL_STATE_WAIT);
	}
	return res;
}

/*!
 * \brief This function is used to playback either a file or number sequence to all conference participants. Used for announcing
 * The playback channel is created once, and imparted on the conference when necessary on follow-up calls
 */
int playback_to_conference(sccp_conference_t * conference, const char *filename, int say_number)
{
	PBX_CHANNEL_TYPE *underlying_channel;
	int res = 0;

	if (!conference->playback_announcements) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Playback on conference suppressed\n", conference->id);
		return 1;
	}

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
		if (!sccp_strlen_zero(conference->playback_language)) {
			PBX(set_language) (conference->playback_channel, conference->playback_language);
		}
		pbx_channel_set_bridge(conference->playback_channel, conference->bridge);

		if (ast_call(conference->playback_channel, "", 0)) {
			pbx_hangup(conference->playback_channel);
			conference->playback_channel = NULL;
			pbx_mutex_unlock(&conference->playback_lock);
			return 0;
		}

		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Created Playback Channel\n", conference->id);

		underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);

		// Update CDR to prevent nasty ast warning when hanging up this channel (confbridge does not set the cdr correctly)
		pbx_cdr_start(pbx_channel_cdr(conference->playback_channel));
#if ASTERISK_VERSION_GROUP < 110
		conference->playback_channel->cdr->answer = ast_tvnow();
		underlying_channel->cdr->answer = ast_tvnow();
#endif
		pbx_cdr_update(conference->playback_channel);

	} else {
		/* Channel was already available so we just need to add it back into the bridge */
		underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Attaching '%s' to Conference\n", conference->id, pbx_channel_name(underlying_channel));
#if ASTERISK_VERSION_GROUP < 110
		pbx_bridge_impart(conference->bridge, underlying_channel, NULL, NULL);
#else
		pbx_bridge_impart(conference->bridge, underlying_channel, NULL, NULL, 0);
#endif
	}

	if (!stream_and_wait(conference->playback_channel, filename, say_number)) {
		ast_log(LOG_WARNING, "Failed to play %s or '%d'!\n", filename, say_number);
	} else {
		res = 1;
	}
	sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Detaching '%s' from Conference\n", conference->id, pbx_channel_name(underlying_channel));
	pbx_bridge_depart(conference->bridge, underlying_channel);
	pbx_mutex_unlock(&conference->playback_lock);
	return res;
}

/* conf list related */
sccp_conference_t *sccp_conference_findByID(uint32_t identifier)
{
	sccp_conference_t *conference = NULL;

	if (identifier == 0) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_TRAVERSE(&conferences, conference, list) {
		if (conference->id == identifier) {
			conference = sccp_conference_retain(conference);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conferences);
	return conference;
}

sccp_conference_participant_t *sccp_conference_participant_findByID(sccp_conference_t * conference, uint32_t identifier)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference || identifier == 0) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->id == identifier) {
			participant = sccp_participant_retain(participant);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
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
			participant = sccp_participant_retain(participant);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
}

sccp_conference_participant_t *sccp_conference_participant_findByPBXChannel(sccp_conference_t * conference, PBX_CHANNEL_TYPE * channel)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference || !channel) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->conferenceBridgePeer == channel) {
			participant = sccp_participant_retain(participant);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
}

void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * c)
{
	int use_icon = 0;
	uint32_t appID = APPID_CONFERENCE;
	uint32_t callReference = 1;										// callreference should be asterisk_channel->unique identifier
	sccp_conference_participant_t *participant = NULL;
	sccp_channel_t *channel = NULL;
	sccp_device_t *device = NULL;

	if (!conference) {
		pbx_log(LOG_WARNING, "SCCPCONF: No conference available to display list for\n");
		goto exit_function;
	}

	if (!(channel = sccp_channel_retain(c))) {								// only send this list to sccp phones
		pbx_log(LOG_WARNING, "SCCPCONF/%04d: No channel available to display conferencelist for\n", conference->id);
		goto exit_function;
	}

	if (!(participant = sccp_conference_participant_findByChannel(conference, channel))) {
		pbx_log(LOG_WARNING, "SCCPCONF/%04d: Channel %s is not a participant in this conference\n", conference->id, pbx_channel_name(channel->owner));
		goto exit_function;
	}
	if (conference->participants.size < 1) {
		pbx_log(LOG_WARNING, "SCCPCONF/%04d: Conference does not have enough participants\n", conference->id);
		goto exit_function;
	}
#if ASTERISK_VERSION_NUMBER >= 10400
	unsigned int transactionID = pbx_random();
#else
	unsigned int transactionID = random();
#endif

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

		if (part->channel && part->channel == channel) {
			if ((device = sccp_channel_getDevice_retained(part->channel))) {
				if (!device->conferencelist_active) {
					sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: %s: CONFLIST ACTIVED %d %d\n", conference->id, channel->currentDeviceId, channel->callid, part->id);
					device->conferencelist_active = TRUE;
				}
				device = sccp_device_release(device);
			}
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
	if (participant->isModerator) {
#if CS_EXPERIMENTAL
		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>Invite</Name>\n");
		strcat(xmlStr, "  <Position>1</Position>\n");
		sprintf(xmlTmp, "  <URL>UserDataSoftKey:Select:%d:INVITE$%d$%d$%d$</URL>\n", 1, appID, conference->id, transactionID);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</SoftKeyItem>\n");
#endif

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
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: ShowList XML-message:\n%s\n", conference->id, xmlStr);

	if ((device = sccp_channel_getDevice_retained(channel))) {
		device->protocol->sendUserToDeviceDataVersionMessage(device, appID, conference->id, callReference, transactionID, xmlStr, 2);
		device = sccp_device_release(device);
	}

exit_function:
	participant = participant ? sccp_participant_release(participant) : NULL;
	channel = channel ? sccp_channel_release(channel) : NULL;
}

static void sccp_conference_update_conflist(sccp_conference_t * conference)
{
	sccp_conference_participant_t *participant = NULL;
	sccp_device_t *device = NULL;

	if (!conference) {
		return;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->channel) {
			if ((device = sccp_channel_getDevice_retained(participant->channel))) {
				if (device->conferencelist_active) {
					sccp_conference_show_list(conference, participant->channel);
				}
				device = sccp_device_release(device);
			}
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
}

void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;

	if (d && d->dtu_softkey.transactionID == transactionID) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "%s: Handle DTU SoftKey Button Press for CallID %d, Transaction %d, Conference %d, Participant:%d, Action %s\n", d->id, callReference, transactionID, conferenceID, participantID, d->dtu_softkey.action);

		if (!(conference = sccp_conference_findByID(conferenceID))) {
			pbx_log(LOG_WARNING, "%s: Conference not found\n", DEV_ID_LOG(d));
			goto EXIT;
		}
		if (!(participant = sccp_conference_participant_findByID(conference, participantID))) {
			pbx_log(LOG_WARNING, "SCCPCONF/%04d: %s: Participant not found\n", conference->id, DEV_ID_LOG(d));
			goto EXIT;
		}

		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: DTU Softkey Executing Action %s (%s)\n", conference->id, d->dtu_softkey.action, DEV_ID_LOG(d));
		if (!strcmp(d->dtu_softkey.action, "INVITE")) {
#if CS_EXPERIMENTAL
			sccp_conference_show_list(conference, participant->channel);
			sccp_conference_invite_participant(conference, participant->channel);
#endif
		} else if (!strcmp(d->dtu_softkey.action, "EXIT")) {
			d->conferencelist_active = FALSE;
		} else if (!strcmp(d->dtu_softkey.action, "MUTE")) {
			sccp_conference_toggle_mute_participant(conference, participant);
		} else if (!strcmp(d->dtu_softkey.action, "KICK")) {
			if (participant->isModerator) {
				sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Since wenn du we kick ourselves ? You've got issues! (%s)\n", conference->id, DEV_ID_LOG(d));
			} else {
				sccp_conference_kick_participant(conference, participant);
			}
		}
	} else {
		pbx_log(LOG_WARNING, "%s: DTU TransactionID does not match or device not found (%d)\n", DEV_ID_LOG(d), transactionID);
	}
EXIT:
	/* reset softkey state for next button press */
	if (d) {
		if (d->dtu_softkey.action) {
			d->dtu_softkey.action = "";
		}
		d->dtu_softkey.appID = 0;
		d->dtu_softkey.payload = 0;
		d->dtu_softkey.transactionID = 0;
	}
	participant = participant ? sccp_participant_release(participant) : NULL;
	conference = conference ? sccp_conference_release(conference) : NULL;
}

void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	participant->pendingRemoval = TRUE;
	pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
	stream_and_wait(participant->conferenceBridgePeer, "conf-kicked", -1);
	sccp_conference_update_conflist(conference);
}

void sccp_conference_toggle_lock_conference(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	conference->isLocked = (!conference->isLocked ? 1 : 0);
	playback_to_channel(participant, (conference->isLocked ? "conf-lockednow" : "conf-unlockednow"), -1);
	sccp_conference_update_conflist(conference);
}

void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_device_t *d = NULL;

	if (!participant->isMuted) {
		participant->isMuted = 1;
		participant->features.mute = 1;
		playback_to_channel(participant, "conf-muted", -1);
		if (participant->channel) {
			if ((d = sccp_channel_getDevice_retained(participant->channel))) {
				sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
				sccp_dev_set_message(d, "muted", 5, FALSE, FALSE);
				d = sccp_device_release(d);
			}
		}
	} else {
		participant->isMuted = 0;
		participant->features.mute = 0;
		playback_to_channel(participant, "conf-unmuted", -1);
		if (participant->channel) {
			if ((d = sccp_channel_getDevice_retained(participant->channel))) {
				sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
				sccp_dev_set_message(d, "unmuted", 5, FALSE, FALSE);
				d = sccp_device_release(d);
			}
		}
	}
	sccp_conference_update_conflist(conference);
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

/* CLI Functions */
#include <asterisk/cli.h>
/*!
 * \brief Complete Conference
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 * 
 * \lock
 *      - lines
 */
char *sccp_complete_conference(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	sccp_conference_t *conference = NULL;
	int wordlen = strlen(word), which = 0;
	char *ret = NULL;
	char tmpname[20];

	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_TRAVERSE(&conferences, conference, list) {
		snprintf(tmpname, sizeof(tmpname), "SCCPCONF/%d", conference->id);
		if (!strncasecmp(word, tmpname, wordlen) && ++which > state) {
			ret = strdup(tmpname);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conferences);
	return ret;
}

/*!
 * \brief List Conferences
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_cli_show_conferences(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_total = 0;
	sccp_conference_t *conference = NULL;

	// table definition
#define CLI_AMI_TABLE_NAME Conferences
#define CLI_AMI_TABLE_PER_ENTRY_NAME Conference

	//      #define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_conference_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &conferences
#define CLI_AMI_TABLE_LIST_ITER_VAR conference
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 																\
		CLI_AMI_TABLE_FIELD(Id,			d,	3,	conference->id)										\
		CLI_AMI_TABLE_FIELD(Participants,	d,	12,	conference->participants.size)								\
		CLI_AMI_TABLE_FIELD(Moderator,		d,	12,	conference->num_moderators)								\
		CLI_AMI_TABLE_FIELD(Announce,		s,	12,	conference->playback_announcements ? "Yes" : "No")					\
		CLI_AMI_TABLE_FIELD(MuteOnEntry,	s,	12,	conference->mute_on_entry ? "Yes" : "No")						\

#include "sccp_cli_table.h"

	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

/*!
 * \brief Show Conference Participants
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_cli_show_conference(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	sccp_conference_t *conference = NULL;
	int local_total = 0;
	int confid = 0;

	if (argc < 4 || argc > 5 || sccp_strlen_zero(argv[3])) {
		pbx_log(LOG_WARNING, "At least ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}
	if (!sccp_strIsNumeric(argv[3]) || (confid = atoi(argv[3])) <= 0) {
		pbx_log(LOG_WARNING, "At least a valid ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least valid ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}

	if ((conference = sccp_conference_findByID(confid))) {
		sccp_conference_participant_t *participant = NULL;

		if (!s) {
			CLI_AMI_OUTPUT(fd, s, "\n--- SCCP conference ----------------------------------------------------------------------------------\n");
		} else {
			astman_send_listack(s, m, argv[0], "start");
			CLI_AMI_OUTPUT_PARAM("Event", CLI_AMI_LIST_WIDTH, "%s", argv[0]);
		}
		CLI_AMI_OUTPUT_PARAM("ConfId", CLI_AMI_LIST_WIDTH, "%d", conference->id);
#define CLI_AMI_TABLE_NAME Participants
#define CLI_AMI_TABLE_PER_ENTRY_NAME Participant
#define CLI_AMI_TABLE_LIST_ITER_HEAD &conference->participants
#define CLI_AMI_TABLE_LIST_ITER_VAR participant
#define CLI_AMI_TABLE_LIST_LOCK SCCP_LIST_LOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_LIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_LIST_UNLOCK
#define CLI_AMI_TABLE_FIELDS 												\
			CLI_AMI_TABLE_FIELD(Id,			d,	3,	participant->id)					\
			CLI_AMI_TABLE_FIELD(ChannelName,	s,	20,	participant->conferenceBridgePeer ? pbx_channel_name(participant->conferenceBridgePeer) : "NULL")		\
			CLI_AMI_TABLE_FIELD(Moderator,		s,	11,	participant->isModerator ? "Yes" : "No")		\
			CLI_AMI_TABLE_FIELD(Muted,		s,	5,	participant->isMuted ? "Yes" : "No")			\
			CLI_AMI_TABLE_FIELD(Announce,		s,	8,	participant->playback_announcements ? "Yes" : "No")	\

#include "sccp_cli_table.h"
		conference = sccp_conference_release(conference);
	} else {
		pbx_log(LOG_WARNING, "At least a valid ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least valid ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}
	if (s)
		*total = local_total;

	return RESULT_SUCCESS;
}

/*!
 * \brief Conference End
 * \param fd Fd as int
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_cli_conference_end(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int confid = 0;
	int local_total = 0;
	sccp_conference_t *conference = NULL;

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Ending Conference, %d, %s,%s,%s,%s\n", argc, argv[0], argv[1], argv[2], argv[3]);

	if (argc < 4 || argc > 5)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	if (!sccp_strIsNumeric(argv[3]) || (confid = atoi(argv[3])) <= 0) {
		pbx_log(LOG_WARNING, "At least a valid ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least valid ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}
	if ((conference = sccp_conference_findByID(confid))) {
		sccp_conference_end(conference);
		conference = sccp_conference_release(conference);
	} else {
		pbx_log(LOG_WARNING, "At least a valid ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least valid ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}
	if (s)
		*total = local_total;
	return RESULT_SUCCESS;
	return RESULT_FAILURE;
}

/* To implement */
//sccp conference mute conf_id/participant_id
//sccp conference kick conf_id/participant_id

#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
