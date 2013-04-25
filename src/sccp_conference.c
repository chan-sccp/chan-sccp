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
#include "common.h"
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
static const uint32_t appID = APPID_CONFERENCE;

SCCP_LIST_HEAD (, sccp_conference_t) conferences;								/*!< our list of conferences */
static void *sccp_conference_thread(void *data);
void sccp_conference_update_callInfo(sccp_channel_t *channel, PBX_CHANNEL_TYPE * pbxChannel);
void __sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participantChannel);
int playback_to_channel(sccp_conference_participant_t * participant, const char *filename, int say_number);
int playback_to_conference(sccp_conference_t * conference, const char *filename, int say_number);
sccp_conference_t *sccp_conference_findByID(uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByID(sccp_conference_t * conference, uint32_t identifier);
sccp_conference_participant_t *sccp_conference_participant_findByChannel(sccp_conference_t * conference, sccp_channel_t * channel);
sccp_conference_participant_t *sccp_conference_participant_findByDevice(sccp_conference_t * conference, sccp_device_t * device);
sccp_conference_participant_t *sccp_conference_participant_findByPBXChannel(sccp_conference_t * conference, PBX_CHANNEL_TYPE * channel);
void sccp_conference_play_music_on_hold_to_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant, boolean_t start);
static sccp_conference_participant_t *sccp_conference_createParticipant(sccp_conference_t * conference);
static void sccp_conference_addParticipant_toList(sccp_conference_t * conference, sccp_conference_participant_t * participant);
void pbx_builtin_setvar_int_helper(PBX_CHANNEL_TYPE * channel, const char *var_name, int intvalue);
static void sccp_conference_connect_bridge_channels_to_participants(sccp_conference_t * conference);
static void sccp_conference_update_conflist(sccp_conference_t * conference);
void __sccp_conference_hide_list(sccp_conference_participant_t * participant);
void sccp_conference_promote_demote_participant(sccp_conference_t * conference, sccp_conference_participant_t *participant, sccp_conference_participant_t *moderator);
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_conference_participant_t *moderator);
 
/*!
 * \brief Start Conference Module
 */
void sccp_conference_module_start(void)
{
	SCCP_LIST_HEAD_INIT(&conferences);
}

/*
 * \brief Cleanup after conference refcount goes to zero (refcount destroy)
 */
static void __sccp_conference_destroy(sccp_conference_t * conference)
{
	if (!conference) {
		return;
	}

	if (conference->playback_channel) {
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Destroying conference playback channel\n", conference->id);
		PBX_CHANNEL_TYPE *underlying_channel = pbx_channel_tech(conference->playback_channel)->bridged_channel(conference->playback_channel, NULL);

		pbx_hangup(underlying_channel);
		pbx_hangup(conference->playback_channel);
		conference->playback_channel = NULL;
	}
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Destroying conference\n", conference->id);
	sccp_free(conference->linkedid);
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

/*
 * \brief Cleanup after participant refcount goes to zero (refcount destroy)
 */
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
		participant->channel->conference = participant->channel->conference ? sccp_conference_release(participant->channel->conference) : NULL;
		participant->channel = sccp_channel_release(participant->channel);
	}
	if (participant->device) {
		participant->device->conferencelist_active = FALSE;
		participant->device->conference = participant->device->conference ? sccp_conference_release(participant->device->conference) : NULL;
		participant->device = sccp_device_release(participant->device);
	}
	participant->conference = participant->conference ? sccp_conference_release(participant->conference) : NULL;
	return;
}

/* ============================================================================================================================ Conference Functions === */
/*!
 * \brief Create a new conference on sccp_channel_t 
 */
sccp_conference_t *sccp_conference_create(sccp_device_t * device, sccp_channel_t * channel)
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
	conference->isOnHold = FALSE;
	conference->linkedid = strdup(PBX(getChannelLinkedId) (channel));
	conference->mute_on_entry = device->conf_mute_on_entry;
	conference->playback_announcements = device->conf_play_general_announce;
	sccp_copy_string(conference->playback_language, pbx_channel_language(channel->owner), sizeof(conference->playback_language));
	SCCP_LIST_HEAD_INIT(&conference->participants);

	//      bridgeCapabilities = AST_BRIDGE_CAPABILITY_1TO1MIX;                             /* bridge_multiplexed */
	bridgeCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX;							/* bridge_softmix */
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_MULTITHREADED;						/* bridge_softmix */
#ifdef CS_SCCP_VIDEO
	bridgeCapabilities |= AST_BRIDGE_CAPABILITY_VIDEO;
#endif
	/* using the SMART flag results in issues when removing forgeign participant, because it try to create a new conference and merge into it. Which seems to be more complex then necessary */
	conference->bridge = pbx_bridge_new(bridgeCapabilities, 0);

	/*
	   pbx_bridge_set_internal_sample_rate(conference_bridge->bridge, auto);
	   pbx_bridge_set_mixing_interval(conference->bridge,40);
	 */

	SCCP_LIST_LOCK(&conferences);
	if ((conference = sccp_conference_retain(conference))) {
		SCCP_LIST_INSERT_HEAD(&conferences, conference, list);
	}
	SCCP_LIST_UNLOCK(&conferences);

	/* init playback lock */
	pbx_mutex_init(&conference->playback_lock);

	/* create new conference moderator channel */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCP: Adding moderator channel to SCCPCONF/%04d\n", conferenceID);

	sccp_conference_participant_t *participant = sccp_conference_createParticipant(conference);

	if (participant) {
		conference->num_moderators++;
		participant->channel = sccp_channel_retain(channel);
		participant->device = sccp_device_retain(device);
		participant->conferenceBridgePeer = channel->owner;
		if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_thread, participant) < 0) {
			PBX(requestHangup) (channel->owner);
			return NULL;
		}
		sccp_conference_addParticipant_toList(conference, participant);
		participant->channel->conference = sccp_conference_retain(conference);
		participant->channel->conference_id = conference->id;
		participant->channel->conference_participant_id = participant->id;
		participant->playback_announcements = device->conf_play_part_announce;
		
		PBX(setChannelLinkedId) (participant->channel, conference->linkedid);
		sccp_conference_update_callInfo(channel, participant->conferenceBridgePeer);
		participant->isModerator = TRUE;
		device->conferencelist_active = TRUE;				// Activate conflist
		sccp_softkey_setSoftkeyState(device, KEYMODE_CONNCONF, SKINNY_LBL_JOIN, TRUE);
		sccp_softkey_setSoftkeyState(device, KEYMODE_CONNTRANS, SKINNY_LBL_JOIN, TRUE);
		pbx_builtin_setvar_int_helper(channel->owner, "__SCCP_CONFERENCE_ID", conference->id);
		pbx_builtin_setvar_int_helper(channel->owner, "__SCCP_CONFERENCE_PARTICIPANT_ID", participant->id);
		sccp_indicate(device, channel, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Added Moderator %d (Channel: %s)\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));
	}

	/** we return the pointer, so do not release conference (should be retained in d->conference or rather l->conference/c->conference. d->conference limits us to one conference per phone */
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_USER, "SCCPConfStart", "ConfId: %d\r\n" "SCCPDevice: %s\r\n", conferenceID, DEV_ID_LOG(device));
	}
#endif
	conference = sccp_conference_retain(conference);							// return retained
	return conference;
}

/*! 
 * \brief Generic Function to create a new participant
 */
static sccp_conference_participant_t *sccp_conference_createParticipant(sccp_conference_t * conference)
{
	if (!conference) {
		pbx_log(LOG_ERROR, "SCCPCONF: no conference / participantChannel provided.\n");
		return NULL;
	}

	sccp_conference_participant_t *participant = NULL;
	int participantID = conference->participants.size + 1;
	char participantIdentifier[REFCOUNT_INDENTIFIER_SIZE];

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Creating new conference-participant %d\n", conference->id, participantID);

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
	participant->playback_announcements = conference->playback_announcements;	// default
	participant->onMusicOnHold = FALSE;

	return participant;
}

static void sccp_conference_connect_bridge_channels_to_participants(sccp_conference_t * conference)
{
	struct ast_bridge *bridge= conference->bridge;
 	struct ast_bridge_channel *bridge_channel = NULL;
	sccp_conference_participant_t *participant = NULL;
 
	sccp_log((DEBUGCAT_HIGH | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Searching Bridge Channel(num_channels: %d).\n",  conference->id, conference->bridge->num);
	ao2_lock(bridge);
 	AST_LIST_TRAVERSE(&bridge->channels, bridge_channel, entry) {
		sccp_log((DEBUGCAT_HIGH | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Bridge Channel %p.\n", conference->id, bridge_channel);
		if ((participant = sccp_conference_participant_findByPBXChannel(conference, bridge_channel->chan))) {
			if (conference->mute_on_entry) {
 				participant->features.mute = 1;
			}
 			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Connecting Bridge Channel %p to Participant %d.\n", conference->id, bridge_channel, participant->id);
 			participant->bridge_channel = bridge_channel;
			participant = sccp_participant_release(participant);
 		}
 	}
	ao2_unlock(bridge);
}

/*!
 * \brief Allocate a temp channel(participant->conferenceBridgePeer) to take the place of the participant_ast_channel in the old channel bridge (masquerade). 
 * The resulting "bridge-free" participant_ast_channel can then be inserted into the conference
 * The temp channel will be hungup
 */
static boolean_t sccp_conference_masqueradeChannel(PBX_CHANNEL_TYPE * participant_ast_channel, sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	if (participant) {
		if (!(PBX(allocTempPBXChannel) (participant_ast_channel, &participant->conferenceBridgePeer))) {
			pbx_log(LOG_ERROR, "SCCPCONF/%04d: Creation of Temp Channel Failed. Exiting.\n", conference->id);
			return FALSE;
		}
		if (!PBX(masqueradeHelper) (participant_ast_channel, participant->conferenceBridgePeer)) {
			pbx_log(LOG_ERROR, "SCCPCONF/%04d: Failed to Masquerade TempChannel.\n", conference->id);
			PBX(requestHangup) (participant->conferenceBridgePeer);
			//participant_ast_channel = pbx_channel_unref(participant_ast_channel);
			return FALSE;
		}
		if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_thread, participant) < 0) {
			PBX(requestHangup) (participant->conferenceBridgePeer);
			return FALSE;
		}
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Added Participant %d (Channel: %s)\n", conference->id, participant->id, pbx_channel_name(participant->conferenceBridgePeer));

		return TRUE;
	}
	return FALSE;
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
void sccp_conference_update_callInfo(sccp_channel_t *channel, PBX_CHANNEL_TYPE *pbxChannel)
{
	char confstr[StationMaxNameSize] = "";
	snprintf(confstr, StationMaxNameSize, "Conference %d", channel->conference_id);
	
	switch (channel->calltype) {	
		case SKINNY_CALLTYPE_INBOUND:
			sccp_copy_string(channel->callInfo.originalCallingPartyName, channel->callInfo.callingPartyName, sizeof(channel->callInfo.originalCallingPartyName));
			channel->callInfo.originalCallingParty_valid = 1;
			sccp_copy_string(channel->callInfo.callingPartyName, confstr, sizeof(channel->callInfo.callingPartyName));
			channel->callInfo.callingParty_valid = 1;
			break;
		case SKINNY_CALLTYPE_OUTBOUND:
		case SKINNY_CALLTYPE_FORWARD:
			sccp_copy_string(channel->callInfo.originalCalledPartyName, channel->callInfo.calledPartyName, sizeof(channel->callInfo.originalCallingPartyName));
			channel->callInfo.originalCalledParty_valid = 1;
			sccp_copy_string(channel->callInfo.calledPartyName, confstr, sizeof(channel->callInfo.calledPartyName));
			channel->callInfo.calledParty_valid = 1;
			break;
	}
	
	/* this is just a workaround to update sip and other channels also -MC */
	/** @todo we should fix this workaround -MC */
#if ASTERISK_VERSION_GROUP > 106
	struct ast_party_connected_line connected;
	struct ast_set_party_connected_line update_connected;
	memset(&update_connected, 0, sizeof(update_connected));
	ast_party_connected_line_init(&connected);
	
	update_connected.id.number = 1;
	connected.id.number.valid = 1;
	connected.id.number.str = confstr;
	connected.id.number.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
	
	update_connected.id.name = 1;
	connected.id.name.valid = 1;
	connected.id.name.str = confstr;
	connected.id.name.presentation = AST_PRES_ALLOWED_NETWORK_NUMBER;
#if ASTERISK_VERSION_GROUP > 110
	ast_set_party_id_all(&update_connected.priv);
#endif
	connected.source = AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER;
	ast_channel_set_connected_line(pbxChannel, &connected, &update_connected);
#endif
	PBX(set_connected_line) (channel, confstr, confstr, AST_CONNECTED_LINE_UPDATE_SOURCE_TRANSFER);
}

/*!
 * \brief Used to set the ConferenceId and ParticipantId on the pbx channel for outside use
 * \todo Has to be moved to pbx_impl 
 */
void pbx_builtin_setvar_int_helper(PBX_CHANNEL_TYPE * channel, const char *var_name, int intvalue)
{
	char valuestr[8] = "";

	snprintf(valuestr, 8, "%d", intvalue);
	pbx_builtin_setvar_helper(channel, var_name, valuestr);
}

/*!
 * \brief Take the channel bridge peer and add it to the conference. (used for the bridge peer channels which are on hold on the moderators phone)
 */
#if HAVE_PBX_CEL_H
#  include <asterisk/cel.h>
#endif
void sccp_conference_addParticipatingChannel(sccp_conference_t * conference, sccp_channel_t *originalSCCPChannel, PBX_CHANNEL_TYPE * pbxChannel)
{
	sccp_channel_t *channel = NULL;

	if (!conference->isLocked) {
		sccp_conference_participant_t *participant = sccp_conference_createParticipant(conference);

		if (participant) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Adding participant %d (Channel %s)\n", conference->id, participant->id, pbx_channel_name(pbxChannel));

			sccp_device_t *device = NULL;
			sccp_conference_update_callInfo(originalSCCPChannel, pbxChannel);					// Update CallerId on originalChannel before masquerade

			// if peer is sccp then retain peer_sccp_channel
			channel = get_sccp_channel_from_pbx_channel(pbxChannel);

			if (channel && (device = sccp_channel_getDevice_retained(channel))) {
				participant->playback_announcements = device->conf_play_part_announce;
				PBX(setChannelLinkedId) (channel, conference->linkedid);
			} else {
				participant->playback_announcements = conference->playback_announcements;
			}

			if (sccp_conference_masqueradeChannel(pbxChannel, conference, participant)) {
				sccp_conference_addParticipant_toList(conference, participant);
				if (channel && device) {							// SCCP Channel
					participant->channel = sccp_channel_retain(channel);
					participant->device = sccp_device_retain(device);
					participant->channel->conference = sccp_conference_retain(conference);
					participant->channel->conference_id = conference->id;
					participant->channel->conference_participant_id = participant->id;
					participant->playback_announcements = device->conf_play_part_announce;
					sccp_indicate(device, channel, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);
//					device->conferencelist_active = TRUE;					// Activate conflist on all sccp participants
				} else {									// PBX Channel
					PBX(setPBXChannelLinkedId)(participant->conferenceBridgePeer, conference->linkedid);
				}
				pbx_builtin_setvar_int_helper(participant->conferenceBridgePeer, "__SCCP_CONFERENCE_ID", conference->id);
				pbx_builtin_setvar_int_helper(participant->conferenceBridgePeer, "__SCCP_CONFERENCE_PARTICIPANT_ID", participant->id);
#if ASTERISK_VERSION_GROUP>106
				pbx_indicate(participant->conferenceBridgePeer, AST_CONTROL_CONNECTED_LINE);
#endif
			} else {
				// Masq Error
				channel = channel ? sccp_channel_release(channel) : NULL;
				participant = sccp_participant_release(participant);
			}
			channel = channel ? sccp_channel_release(channel) : NULL;
			device = device ? sccp_device_release(device) : NULL;
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Conference is locked. Participant Denied.\n", conference->id);
		if (pbxChannel) {
			pbx_stream_and_wait(pbxChannel, "conf-locked", "");
		}
	}
}

/*!
 * \brief Remove a specific participant from a conference
 */
static void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
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
	sccp_conference_update_conflist(conference);
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
			manager_event(EVENT_FLAG_CALL, "SCCPConfEntered", "ConfId: %d\r\n" "PartId: %d\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", participant->conference->id, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
		}
#endif
		// Join the bridge
		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Entering pbx_bridge_join: %s as %d\n", participant->conference->id, pbx_channel_name(participant->conferenceBridgePeer), participant->id);
		pbx_bridge_join(participant->conference->bridge, participant->conferenceBridgePeer, NULL, &participant->features, NULL);

		sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Leaving pbx_bridge_join: %s as %d\n", participant->conference->id, pbx_channel_name(participant->conferenceBridgePeer), participant->id);
#ifdef CS_MANAGER_EVENTS
		if (GLOB(callevents)) {
			manager_event(EVENT_FLAG_CALL, "SCCPConfLeft", "ConfId: %d\r\n" "PartId: %d\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", participant->conference->id, participant->id, participant->channel ? PBX(getChannelName) (participant->channel) : "NULL", participant->channel ? PBX(getChannelUniqueID) (participant->channel) : "NULL");
		}
#endif
		if (participant->channel && participant->device) {
			__sccp_conference_hide_list(participant);
		}

		participant->pendingRemoval = TRUE;
		sccp_conference_removeParticipant(participant->conference, participant);
		participant->joinThread = AST_PTHREADT_NULL;
		participant = sccp_participant_release(participant);
	}

	return NULL;
}

/*!
 * \brief This function is called when the minimal number of occupants of a confernce is reached or when the last moderator hangs-up
 */
void sccp_conference_start(sccp_conference_t * conference)
{
	sccp_conference_update_conflist(conference);
	//playback_to_conference(conference, "conf-enteringno", -1);
	//playback_to_conference(conference, NULL, conference->id);
	playback_to_conference(conference, "conf-placeintoconf", -1);
	sccp_conference_connect_bridge_channels_to_participants(conference);
#ifdef CS_MANAGER_EVENTS
		if (GLOB(callevents)) {
			manager_event(EVENT_FLAG_CALL, "SCCPConfStarted", "ConfId: %d\r\n", conference->id);
		}
#endif
}

void sccp_conference_update(sccp_conference_t * conference)
{
	sccp_conference_update_conflist(conference);
	sccp_conference_connect_bridge_channels_to_participants(conference);
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

/* ========================================================================================================================== Conference Hold/Resume === */
/*!
 * \brief Handle putting on hold a conference
 */
void sccp_conference_hold(sccp_conference_t * conference)
{
	sccp_conference_participant_t *participant = NULL;

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Putting conference on hold.\n", conference->id);
	if (!conference) {
		return;
	}

	/* play music on hold to participants, if there is no moderator, currently active to the conference */
	if (!conference->isOnHold && conference->num_moderators == 1) {
		SCCP_LIST_LOCK(&conference->participants);
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (participant->isModerator == FALSE) {
				sccp_conference_play_music_on_hold_to_participant(conference, participant, TRUE);
			}
		}
		SCCP_LIST_UNLOCK(&conference->participants);
		conference->isOnHold = TRUE;
	}
}

/*!
 * \brief Handle resuming a conference
 */
void sccp_conference_resume(sccp_conference_t * conference)
{
	sccp_conference_participant_t *participant = NULL;

	sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Resuming conference.\n", conference->id);
	if (!conference) {
		return;
	}

	/* stop play music on hold to participants. */
	if (conference->isOnHold) {
		SCCP_LIST_LOCK(&conference->participants);
		SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
			if (participant->isModerator == FALSE) {
				sccp_conference_play_music_on_hold_to_participant(conference, participant, FALSE);
			}
		}
		SCCP_LIST_UNLOCK(&conference->participants);
		conference->isOnHold = TRUE;
	}
}

/* =============================================================================================================== Playback to Conference/Participant === */
/*!
 * \brief This helper-function is used to playback either a file or number sequence
 */
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

/*!
 * \brief This function is used to playback either a file or number sequence to a specific participant only
 */
int playback_to_channel(sccp_conference_participant_t * participant, const char *filename, int say_number)
{
	int res = 0;

	if (!participant->playback_announcements) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Playback for participant %d suppressed\n", participant->conference->id, participant->id);
		return 1;
	}
	if (participant->bridge_channel) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Playback %s %d for participant %d\n", participant->conference->id, filename, say_number, participant->id);
		participant->bridge_channel->suspended = 1;
		ast_bridge_change_state(participant->bridge_channel, AST_BRIDGE_CHANNEL_STATE_WAIT);
		if (stream_and_wait(participant->bridge_channel->chan, filename, say_number)) {
			res = 1;
		} else {
			pbx_log(LOG_WARNING, "Failed to play %s or '%d'!\n", filename, say_number);
		}
		participant->bridge_channel->suspended = 0;
		ast_bridge_change_state(participant->bridge_channel, AST_BRIDGE_CHANNEL_STATE_WAIT);
	} else {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: No bridge channel for playback\n", participant->conference->id);
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
		pbx_bridge_impart(conference->bridge, underlying_channel, NULL, NULL, 0);
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

/* ============================================================================================================================= List Find Functions === */
/*!
 * \brief Find conference by ID
 */
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

/*!
 * \brief Find participant by ID
 */
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

/*!
 * \brief Find participant by sccp channel
 */
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
/*!
 * \brief Find participant by sccp device
 */
sccp_conference_participant_t *sccp_conference_participant_findByDevice(sccp_conference_t * conference, sccp_device_t * device)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference || !device) {
		return NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->device == device) {
			participant = sccp_participant_retain(participant);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
}

/*!
 * \brief Find participant by PBX channel
 */
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

/* ======================================================================================================================== ConfList (XML) Functions === */

/*!
 * \brief Show ConfList
 *
 * Cisco URL reference
 * UserData:INTEGER:STRING
 * UserDataSoftKey:STRING:INTEGER:STRING
 * UserCallDataSoftKey:STRING:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
 * UserCallData:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
 */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * c)
{
	int use_icon = 0;
	sccp_conference_participant_t *participant = NULL;
	sccp_channel_t *channel = NULL;

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
	if (participant->device) {
		participant->device->conferencelist_active = TRUE;
		if (!participant->callReference) {
			participant->callReference = channel->callid;
			participant->lineInstance = conference->id;
#if ASTERISK_VERSION_NUMBER >= 10400
			participant->transactionID = pbx_random() % 1000;
#else
			participant->transactionID = random() % 1000;
#endif
		}
		
		char xmlStr[2048] = "";
		char xmlTmp[512] = "";

	//	sprintf(xmlTmp, "<CiscoIPPhoneIconMenu appId=\"%d\" onAppFocusLost=\"\" onAppFocusGained=\"\" onAppClosed=\"\">", appID);
		if (participant->device->protocolversion >= 17) {
			sprintf(xmlTmp, "<CiscoIPPhoneIconFileMenu appId=\"%d\">", appID);
			strcat(xmlStr, xmlTmp);
			if (conference->isLocked) {
				sprintf(xmlTmp, "<Title IconIndex=\"5\">Conference %d</Title>\n", conference->id);
			} else {
				sprintf(xmlTmp, "<Title IconIndex=\"4\">Conference %d</Title>\n", conference->id);
			}
			strcat(xmlStr, xmlTmp);
		} else {
			sprintf(xmlTmp, "<CiscoIPPhoneIconMenu>");
			strcat(xmlStr, xmlTmp);
			sprintf(xmlTmp, "<Title>Conference %d</Title>\n", conference->id);
			strcat(xmlStr, xmlTmp);
		}	
		strcat(xmlStr, "<Prompt>Make Your Selection</Prompt>\n");

		// MenuItems

		sccp_conference_participant_t *part = NULL;

		SCCP_LIST_LOCK(&conference->participants);
		SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
			if (part->pendingRemoval)
				continue;

			strcat(xmlStr, "<MenuItem>");

			if (part->isModerator)
				use_icon = 0;
			else
				use_icon = 2;

			if (part->features.mute) {
				++use_icon;
			}
			strcat(xmlStr, "<IconIndex>");
			sprintf(xmlTmp, "%d", use_icon);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</IconIndex>");

			strcat(xmlStr, "<Name>");
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
#if ASTERISK_VERSION_GROUP > 110
				sprintf(xmlTmp, "%d:%s (%s)", part->id, ast_channel_caller(part->conferenceBridgePeer)->id.name.str, ast_channel_caller(part->conferenceBridgePeer)->id.number.str);
#elif ASTERISK_VERSION_GROUP > 106
				sprintf(xmlTmp, "%d:%s (%s)", part->id, part->conferenceBridgePeer->caller.id.name.str, part->conferenceBridgePeer->caller.id.number.str);
#else
				sprintf(xmlTmp, "%d:%s (%s)", part->id, part->conferenceBridgePeer->cid.cid_name, part->conferenceBridgePeer->cid.cid_num);
#endif
				strcat(xmlStr, xmlTmp);
			}
			strcat(xmlStr, "</Name>");
			sprintf(xmlTmp, "<URL>UserCallData:%d:%d:%d:%d:%d</URL>", appID, participant->lineInstance, participant->callReference, participant->transactionID, part->id);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</MenuItem>\n");
		}
		SCCP_LIST_UNLOCK(&conference->participants);

		// SoftKeys
		if (participant->isModerator) {
			strcat(xmlStr, "<SoftKeyItem>");
			strcat(xmlStr, "<Name>EndConf</Name>");
			strcat(xmlStr, "<Position>1</Position>");
//			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:ENDCONF/%d/%d/%d/</URL>", 1, appID, participant->lineInstance, participant->transactionID);
			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:ENDCONF/%d</URL>", 1, participant->transactionID);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</SoftKeyItem>\n");
			strcat(xmlStr, "<SoftKeyItem>");
			strcat(xmlStr, "<Name>Mute</Name>");
			strcat(xmlStr, "<Position>2</Position>");
//			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:MUTE/%d/%d/%d/</URL>", 2, appID, participant->lineInstance, participant->transactionID);
			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:MUTE/%d</URL>", 2, participant->transactionID);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</SoftKeyItem>\n");

			strcat(xmlStr, "<SoftKeyItem>");
			strcat(xmlStr, "<Name>Kick</Name>");
			strcat(xmlStr, "<Position>3</Position>");
//			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:KICK/%d/%d/%d/</URL>", 3, appID, participant->lineInstance, participant->transactionID);
			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:KICK/%d</URL>", 3, participant->transactionID);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</SoftKeyItem>\n");
		}
		strcat(xmlStr, "<SoftKeyItem>");
		strcat(xmlStr, "<Name>Exit</Name>");
		strcat(xmlStr, "<Position>4</Position>");
		strcat(xmlStr, "<URL>SoftKey:Exit</URL>");
		strcat(xmlStr, "</SoftKeyItem>\n");
		if (participant->isModerator) {
			strcat(xmlStr, "<SoftKeyItem>");
			strcat(xmlStr, "<Name>Moderate</Name>");
			strcat(xmlStr, "<Position>5</Position>");
			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:MODERATE/%d/%d/%d/</URL>", 1, appID, participant->lineInstance, participant->transactionID);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</SoftKeyItem>\n");
#if CS_EXPERIMENTAL
			strcat(xmlStr, "<SoftKeyItem>");
			strcat(xmlStr, "<Name>Invite</Name>");
			strcat(xmlStr, "<Position>6</Position>");
			sprintf(xmlTmp, "<URL>UserDataSoftKey:Select:%d:INVITE/%d/%d/%d/</URL>", 1, appID, participant->lineInstance, participant->transactionID);
			strcat(xmlStr, xmlTmp);
			strcat(xmlStr, "</SoftKeyItem>\n");
#endif
		}
		// CiscoIPPhoneIconMenu Icons
		if (participant->device->protocolversion >= 17) {
			strcat(xmlStr, "<IconItem><Index>0</Index><URL>Resource:Icon.Connected</URL></IconItem>");		// moderator
			strcat(xmlStr, "<IconItem><Index>1</Index><URL>Resource:AnimatedIcon.Hold</URL></IconItem>");		// muted moderator
			strcat(xmlStr, "<IconItem><Index>2</Index><URL>Resource:AnimatedIcon.StreamRxTx</URL></IconItem>");	// participant
			strcat(xmlStr, "<IconItem><Index>3</Index><URL>Resource:AnimatedIcon.Hold</URL></IconItem>");		// muted participant
			strcat(xmlStr, "<IconItem><Index>4</Index><URL>Resource:Icon.Speaker</URL></IconItem>");		// unlocked conference
			strcat(xmlStr, "<IconItem><Index>5</Index><URL>Resource:Icon.SecureCall</URL></IconItem>\n");		// locked conference
		} else {
			strcat(xmlStr, "<IconItem><Index>0</Index><Height>10</Height><Width>16</Width><Depth>2</Depth><Data>000F0000C03F3000C03FF000C03FF003000FF00FFCFFF30FFCFFF303CC3FF300CC3F330000000000</Data></IconItem>");// moderator
			strcat(xmlStr, "<IconItem><Index>1</Index><Height>10</Height><Width>16</Width><Depth>2</Depth><Data>000F0000C03FF03CC03FF03CC03FF03C000FF03CFCFFF33CFCFFF33CCC3FF33CCC3FF33C00000000</Data></IconItem>");// muted moderator
			strcat(xmlStr, "<IconItem><Index>2</Index><Height>10</Height><Width>16</Width><Depth>2</Depth><Data>000F0000C0303000C030F000C030F003000FF00FFCF0F30F0C00F303CC30F300CC30330000000000</Data></IconItem>");// participant
			strcat(xmlStr, "<IconItem><Index>3</Index><Height>10</Height><Width>16</Width><Depth>2</Depth><Data>000F0000C030F03CC030F03CC030F03C000FF03CFCF0F33C0C00F33CCC30F33CCC30F33C00000000</Data></IconItem>\n");// muted participant
		}

		if (participant->device->protocolversion >= 17) {
			strcat(xmlStr, "</CiscoIPPhoneIconFileMenu>\n");
		} else {	
			strcat(xmlStr, "</CiscoIPPhoneIconMenu>\n");
		}
		sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: ShowList appID %d, lineInstance %d, callReference %d, transactionID %d\n", conference->id, appID, participant->callReference, participant->lineInstance, participant->transactionID);
		sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: XML-message:\n%s\n", conference->id, xmlStr);

		participant->device->protocol->sendUserToDeviceDataVersionMessage(participant->device, appID, participant->callReference, participant->lineInstance, participant->transactionID, xmlStr, 2);
	}

exit_function:
	participant = participant ? sccp_participant_release(participant) : NULL;
	channel = channel ? sccp_channel_release(channel) : NULL;
}

/*!
 * \brief Hide ConfList
 */
void __sccp_conference_hide_list(sccp_conference_participant_t *participant)
{
	if (participant->channel && participant->device) {
		if (participant->device->conferencelist_active) {
			sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: Hide Conf List for participant: %d\n", participant->conference->id, participant->id);
			char *data = "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"App:Close:0\"/></CiscoIPPhoneExecute>";
			participant->device->protocol->sendUserToDeviceDataVersionMessage(participant->device, appID, participant->callReference, participant->lineInstance, participant->transactionID, data, 2);
			participant->device->conferencelist_active = FALSE;
		}
	}
}

void sccp_conference_hide_list_ByDevice(sccp_device_t *device) 
{
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;

	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_TRAVERSE(&conferences, conference, list) {
		if (device && (participant = sccp_conference_participant_findByDevice(conference, device))) {
			if (participant->channel && participant->device) {
				__sccp_conference_hide_list(participant);
			}
			participant = sccp_participant_release(participant);
		}
	}	
	SCCP_LIST_UNLOCK(&conferences);
}


/*!
 * \brief Update ConfList on all phones displaying the list
 */
static void sccp_conference_update_conflist(sccp_conference_t * conference)
{
	sccp_conference_participant_t *participant = NULL;

	if (!conference) {
		return;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->channel && participant->device && participant->device->conferencelist_active) {
			sccp_conference_show_list(conference, participant->channel);
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
}

/*!
 * \brief Handle ButtonPresses from ConfList
 */
void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;
	sccp_conference_participant_t *moderator = NULL;

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
		if (!(moderator = sccp_conference_participant_findByDevice(conference, d))) {
			pbx_log(LOG_WARNING, "SCCPCONF/%04d: %s: Moderator not found\n", conference->id, DEV_ID_LOG(d));
			goto EXIT;
		}
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: DTU Softkey Executing Action %s (%s)\n", conference->id, d->dtu_softkey.action, DEV_ID_LOG(d));
		if (!strcmp(d->dtu_softkey.action, "ENDCONF")) {
			sccp_conference_end(conference);
		} else if (!strcmp(d->dtu_softkey.action, "MUTE")) {
			sccp_conference_toggle_mute_participant(conference, participant);
		} else if (!strcmp(d->dtu_softkey.action, "KICK")) {
			if (participant->isModerator) {
				sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Since wenn du we kick ourselves ? You've got issues! (%s)\n", conference->id, DEV_ID_LOG(d));
				sccp_dev_set_message(d, "cannot kick yourself", 5, FALSE, FALSE);
			} else {
				sccp_conference_kick_participant(conference, participant);
			}
		} else if (!strcmp(d->dtu_softkey.action, "EXIT")) {
			d->conferencelist_active = FALSE;
#if CS_EXPERIMENTAL
		} else if (!strcmp(d->dtu_softkey.action, "INVITE")) {
			sccp_conference_invite_participant(conference, moderator);
#endif
		} else if (!strcmp(d->dtu_softkey.action, "MODERATE")) {
			sccp_conference_promote_demote_participant(conference, participant, moderator);
		}
	} else {
		pbx_log(LOG_WARNING, "%s: DTU TransactionID does not match or device not found (%d)\n", DEV_ID_LOG(d), transactionID);
	}
EXIT:
	/* reset softkey state for next button press */
	if (d) {
		if (d->dtu_softkey.action) {
			sccp_free(d->dtu_softkey.action);
		}
		d->dtu_softkey.transactionID = 0;
	}
	participant = participant ? sccp_participant_release(participant) : NULL;
	conference = conference ? sccp_conference_release(conference) : NULL;
	moderator = moderator ? sccp_participant_release(moderator) : NULL;
}

/*!
 * \brief Kick Participant out of the conference
 */
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Kick Participant %d\n", conference->id, participant->id);
	participant->pendingRemoval = TRUE;
	playback_to_channel(participant, "conf-kicked", -1);
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfParticipantKicked", "ConfId: %d\r\n" "PartId: %d\r\n", conference->id, participant->id);
	}
#endif
	pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
}

/*!
 * \brief Toggle Conference Lock
 */
void sccp_conference_toggle_lock_conference(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Toggle Conference Lock\n", conference->id);
	conference->isLocked = (!conference->isLocked ? 1 : 0);
	playback_to_channel(participant, (conference->isLocked ? "conf-lockednow" : "conf-unlockednow"), -1);
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfLock", "ConfId: %d\r\n" "Enabled: %s\r\n", conference->id, conference->isLocked ? "Yes" : "No");
	}
#endif
	sccp_conference_update_conflist(conference);
}

/*!
 * \brief Toggle Participant Mute Status
 */
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Mute Participant %d\n", conference->id, participant->id);
	if (!participant->features.mute) {
		participant->features.mute = 1;
		playback_to_channel(participant, "conf-muted", -1);
//		if (participant->channel) {
//			participant->channel->setMicrophone(participant->channel, FALSE);
//		}
	} else {
		participant->features.mute = 0;
		playback_to_channel(participant, "conf-unmuted", -1);
//		if (participant->channel) {
//			participant->channel->setMicrophone(participant->channel, TRUE);
//		}
	}
	if (participant->channel && participant->device) {
		sccp_dev_set_message(participant->device, participant->features.mute ? "You are muted" : "You are unmuted", 5, FALSE, FALSE);
	}	
#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents)) {
		manager_event(EVENT_FLAG_CALL, "SCCPConfParticipantMute", "ConfId: %d\r\n" "PartId: %d\r\n" "Mute: %s\r\n", conference->id, participant->id, participant->features.mute ? "Yes" : "No");
	}
#endif
	sccp_conference_update_conflist(conference);
}

/*!
 * \brief Toggle Music on Hold to a specific participant
 */
void sccp_conference_play_music_on_hold_to_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant, boolean_t start)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCPCONF/%04d: Play Music on hold to Participant %d\n", conference->id, participant->id);
	if (!participant->channel || !participant->device) {
		return;
	}
	if (start) {
		if (participant->onMusicOnHold == FALSE) {
			if (!sccp_strlen_zero(participant->device->conf_music_on_hold_class)) {
				PBX(moh_start) (participant->conferenceBridgePeer, participant->device->conf_music_on_hold_class, NULL);
				participant->onMusicOnHold = TRUE;
				//pbx_set_flag(participant->conferenceBridgePeer, AST_FLAG_MOH);
			} else {
				sccp_conference_toggle_mute_participant(conference, participant);
			}
		}
	} else {
		if (!sccp_strlen_zero(participant->device->conf_music_on_hold_class)) {
			if (!ast_bridge_suspend(conference->bridge, participant->conferenceBridgePeer)) {
				PBX(moh_stop) (participant->conferenceBridgePeer);
				//pbx_clear_flag(participant->conferenceBridgePeer, AST_FLAG_MOH);
				ast_bridge_unsuspend(conference->bridge, participant->conferenceBridgePeer);
				participant->onMusicOnHold = FALSE;
			}
		} else {
			sccp_conference_toggle_mute_participant(conference, participant);
		}
	}
	sccp_conference_update_conflist(conference);
}

/*!
 * \brief Promote Participant to Moderator
 *
 * paramater moderator can be provided as NULL (cli/ami actions)
 */
void sccp_conference_promote_demote_participant(sccp_conference_t * conference, sccp_conference_participant_t *participant, sccp_conference_participant_t *moderator)
{
	if (participant->device && participant->channel) {
		if (!participant->isModerator) {			// promote
			participant->isModerator = TRUE;
			conference->num_moderators++;
			participant->device->conferencelist_active = TRUE;
			participant->device->conference = sccp_conference_retain(conference);
			sccp_softkey_setSoftkeyState(participant->device, KEYMODE_CONNCONF, SKINNY_LBL_JOIN, TRUE);
			sccp_softkey_setSoftkeyState(participant->device, KEYMODE_CONNTRANS, SKINNY_LBL_JOIN, TRUE);
			sccp_indicate(participant->device, participant->channel, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);
		} else {
			if (conference->num_moderators > 1) {		// demote
				participant->isModerator = FALSE;
				conference->num_moderators++;
				participant->device->conference = sccp_conference_release(participant->device->conference);
				sccp_softkey_setSoftkeyState(participant->device, KEYMODE_CONNCONF, SKINNY_LBL_JOIN, FALSE);
				sccp_softkey_setSoftkeyState(participant->device, KEYMODE_CONNTRANS, SKINNY_LBL_JOIN, FALSE);
				sccp_indicate(participant->device, participant->channel, SCCP_CHANNELSTATE_CONNECTED);
			} else {
				sccp_log(DEBUGCAT_CONFERENCE)(VERBOSE_PREFIX_3 "SCCPCONF/%04d: Not enough moderators left in the conference. Promote someone else first.\n", conference->id);
				if (moderator) {
					sccp_dev_set_message(moderator->device, "Promote someone first", 5, FALSE, FALSE);
				}
			}
		}
		sccp_dev_set_message(participant->device, participant->isModerator ? "You have been Promoted" : "You have been Demoted", 5, FALSE, FALSE);
#ifdef CS_MANAGER_EVENTS
		if (GLOB(callevents)) {
			manager_event(EVENT_FLAG_CALL, "SCCPConfParticipantPromotion", "ConfId: %d\r\n" "PartId: %d\r\n" "Moderator: %s\r\n", conference->id, participant->id, participant->isModerator ? "Yes" : "No");
		}
#endif
	} else {
		sccp_log(DEBUGCAT_CONFERENCE)(VERBOSE_PREFIX_3 "SCCPCONF/%04d: Only SCCP Channels can be moderators\n", conference->id);
		if (moderator) {
			sccp_dev_set_message(moderator->device, "Only sccp phones can be moderator", 5, FALSE, FALSE);
		}
	}
	sccp_conference_update_conflist(conference);
}

/*!
 * \brief Invite a new participant (XML function)
 * usage: enter a phone number via conflist menu, number will be dialed and included into the conference without any further action
 *
 * \todo To Be Implemented
 * 
 * Cisco URL reference
 * UserData:INTEGER:STRING
 * UserDataSoftKey:STRING:INTEGER:STRING
 * UserCallDataSoftKey:STRING:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
 * UserCallData:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
 */
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_conference_participant_t *moderator)
{
	//sccp_channel_t *channel = NULL;
	char xmlStr[2048] = "";
	char xmlTmp[512] = "";

	if (!conference) {
		pbx_log(LOG_WARNING, "SCCPCONF: No conference\n");
		return;
	}
	if (!moderator) {
		pbx_log(LOG_WARNING, "SCCPCONF/%04d: No moderator\n", conference->id);
		return;
	}
	if (conference->isLocked) {
		pbx_log(LOG_WARNING, "SCCPCONF/%04d: Conference is currently locked\n", conference->id);
		if (moderator->device) {
			sccp_dev_set_message(moderator->device, "Conference is locked", 5, FALSE, FALSE);
		}
		return;
	}

	if (moderator->channel && moderator->device) {

		if (moderator->device->protocolversion >= 17) {
			sprintf(xmlTmp, "<CiscoIPPhoneInput appId=\"%d\">\n", appID);
		} else {
			sprintf(xmlTmp, "<CiscoIPPhoneInput>\n");
		}		
		strcat(xmlStr, xmlTmp);
		sprintf(xmlTmp, "<Title>Conference %d Invite</Title>\n", conference->id);
		strcat(xmlStr, "<Prompt>Enter the phone number to invite</Prompt>\n");
//		sprintf(xmlTmp, "<URL>UserCallData:%d:%d:%d:%d:%d</URL>\n", APPID_CONFERENCE_INVITE, moderator->lineInstance, moderator->callReference, moderator->transactionID, moderator->id);
//		sprintf(xmlTmp, "<URL>UserCallData:%d:%d:%d:%d</URL>\n", APPID_CONFERENCE_INVITE, moderator->lineInstance, moderator->callReference, moderator->transactionID);
		sprintf(xmlTmp, "<URL>UserData:%d:%s</URL>\n", appID, "invite");
		strcat(xmlStr, xmlTmp);
		
		strcat(xmlStr, "<InputItem>\n");
		strcat(xmlStr, "  <DisplayName>Phone Number</DisplayName>\n");
		strcat(xmlStr, "  <QueryStringParam>NUMBER</QueryStringParam>\n");
		strcat(xmlStr, "  <InputFlags>T</InputFlags>\n");
		strcat(xmlStr, "</InputItem>\n");

		// SoftKeys
//		strcat(xmlStr, "<SoftKeyItem>\n");
//		strcat(xmlStr, "  <Name>Exit</Name>\n");
//		strcat(xmlStr, "  <Position>4</Position>\n");
//		strcat(xmlStr, "  <URL>SoftKey:Exit</URL>\n");
//		strcat(xmlStr, "</SoftKeyItem>\n");

		strcat(xmlStr, "</CiscoIPPhoneInput>\n");
		
		sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: ShowList appID %d, lineInstance %d, callReference %d, transactionID %d\n", conference->id, appID, moderator->callReference, moderator->lineInstance, moderator->transactionID);
		sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "SCCPCONF/%04d: XML-message:\n%s\n", conference->id, xmlStr);

		moderator->device->protocol->sendUserToDeviceDataVersionMessage(moderator->device, APPID_CONFERENCE_INVITE, moderator->callReference, moderator->lineInstance, moderator->transactionID, xmlStr, 2);
	}
}

/* =================================================================================================================================== CLI Functions === */
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
	sccp_conference_participant_t *participant = NULL;
	int conference_id = 0;
	int wordlen = strlen(word), which = 0, i = 0;
	char *ret = NULL;
	char tmpname[20];
	char *actions[5] = { "EndConf", "Kick", "Mute", "Invite", "Moderate" };
	
	switch (pos) {
		case 2:		// action
			for (i = 0; i < ARRAY_LEN(actions); i++) {
				if (!strncasecmp(word, actions[i], wordlen) && ++which > state) {
					return strdup(actions[i]);
				}
			}
			break;
		case 3:		// conferenceid
			SCCP_LIST_LOCK(&conferences);
			SCCP_LIST_TRAVERSE(&conferences, conference, list) {
				snprintf(tmpname, sizeof(tmpname), "%d", conference->id);
				if (!strncasecmp(word, tmpname, wordlen) && ++which > state) {
					ret = strdup(tmpname);
					break;
				}
			}
			SCCP_LIST_UNLOCK(&conferences);
			break;
		case 4:		// participantid
			if (sscanf(line, "sccp conference %s %d", tmpname, &conference_id) > 0) {
				if ((conference = sccp_conference_findByID(conference_id))) {
					SCCP_LIST_LOCK(&conference->participants);
					SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
						snprintf(tmpname, sizeof(tmpname), "%d", participant->id);
						if (!strncasecmp(word, tmpname, wordlen) && ++which > state) {
							ret = strdup(tmpname);
							break;
						}
					}
					SCCP_LIST_UNLOCK(&conference->participants);
					
					conference = sccp_conference_release(conference);
				}
			}
			break;
		default:
			break;
	}
	return ret;
}

/*!
 * \brief List Conferences
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
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
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
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
			CLI_AMI_TABLE_FIELD(Muted,		s,	5,	participant->features.mute ? "Yes" : "No")			\
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
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 */
int sccp_cli_conference_action(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int confid = 0, partid=0;
	int local_total = 0;
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;
	int res = RESULT_SUCCESS;

	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "Conference Action:%s, Conference %s, Participant %s\n", argv[2], argv[3], argc >= 5 ? argv[4] : "");

	if (argc < 4 || argc > 5)
		return RESULT_SHOWUSAGE;

	if (sccp_strlen_zero(argv[2]) || sccp_strlen_zero(argv[3]))
		return RESULT_SHOWUSAGE;

	if (sccp_strIsNumeric(argv[3]) && (confid = atoi(argv[3])) > 0) {
		if ((conference = sccp_conference_findByID(confid))) {
			if (!strncasecmp(argv[2], "EndConf", 7)) {									// EndConf Action
				sccp_conference_end(conference);
			} else if (argc >= 5) {
				if (sccp_strIsNumeric(argv[4]) && (partid = atoi(argv[4])) > 0) {
					if ((participant = sccp_conference_participant_findByID(conference, partid))) {
						if (!strncasecmp(argv[2], "Kick", 4)) {								// Kick Action
							sccp_conference_kick_participant(conference, participant);
						} else if (!strncasecmp(argv[2], "Mute", 4)) {							// Mute Action
							sccp_conference_toggle_mute_participant(conference, participant);
						} else if (!strncasecmp(argv[2], "Invite", 5)) {						// Invite Action
							sccp_conference_invite_participant(conference, participant);
						} else if (!strncasecmp(argv[2], "Moderate", 8)) {						// Moderate Action
							sccp_conference_promote_demote_participant(conference, participant, NULL);
						} else {
							pbx_log(LOG_WARNING, "Unknown Action %s\n", argv[2]);
							CLI_AMI_ERROR(fd, s, m, "Unknown Action\n %s", argv[2]);
							res = RESULT_FAILURE;
						}
						participant = sccp_participant_release(participant);
					} else {
						pbx_log(LOG_WARNING, "Participant %s not found in conference %s\n", argv[4], argv[3]);
						CLI_AMI_ERROR(fd, s, m, "Participant %s not found in conference\n", argv[4]);
						res = RESULT_FAILURE;
					}
				} else {
					pbx_log(LOG_WARNING, "At least a valid ParticipantId needs to be supplied\n");
					CLI_AMI_ERROR(fd, s, m, "At least valid ParticipantId needs to be supplied\n %s", "");
					res = RESULT_FAILURE;
				}	
			} else {
				pbx_log(LOG_WARNING, "Not enough parameters provided for action %s\n", argv[2]);
				CLI_AMI_ERROR(fd, s, m, "Not enough parameters provided for action %s\n", argv[2]);
				res = RESULT_FAILURE;
			}	
			conference = sccp_conference_release(conference);
		} else {
			pbx_log(LOG_WARNING, "Conference %s not found\n", argv[3]);
			CLI_AMI_ERROR(fd, s, m, "Conference %s not found\n", argv[3]);
			res = RESULT_FAILURE;
		}
	} else {
		pbx_log(LOG_WARNING, "At least a valid ConferenceId needs to be supplied\n");
		CLI_AMI_ERROR(fd, s, m, "At least valid ConferenceId needs to be supplied\n %s", "");
		return RESULT_FAILURE;
	}

	if (s)
		*total = local_total;
	return res;
}

#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
