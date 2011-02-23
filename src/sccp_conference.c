
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
static int lastConferenceID = 99;

static void *sccp_conference_join_thread(void *data);
boolean_t isModerator(sccp_conference_participant_t * participant, sccp_channel_t * channel);
sccp_conference_t *sccp_conference_find_byid(uint32_t id);
sccp_conference_participant_t *sccp_conference_participant_find_byid(sccp_conference_t * conference, uint32_t id);

/*!
 * Create conference
 *
 * @param owner conference owner
 * @return conference
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t * owner)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_create called.\n");

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

#        if 0
	/* add moderator to participants list */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference: adding moderator to participant list.\n", owner->device->id);
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, moderator, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	/* Add moderator to conference. */
	if (0 != sccp_conference_addAstChannelToConferenceBridge(moderator, moderator->channel->owner)) {
		ast_log(LOG_ERROR, "SCCP: Conference: failed to add moderator channel (preparation phase).\n");
		/* TODO: Error handling. */
	}
#        endif

	/* Store conference in global list. */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference: adding conference to global conference list.\n", owner->device->id);
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, conference, list);
	SCCP_LIST_UNLOCK(&conferences);

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference: Conference with id %d created; Owner: %s \n", owner->device->id, conference->id, owner->device->id);

#        if 0
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_create.\n");
	if (ast_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
		ast_log(LOG_ERROR, "SCCP: Conference: failed to initiate join thread for moderator.\n");
		return conference;
	}
#        endif

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
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addAstChannelToConferenceBridge called.\n");

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
#        if 1
	//ast_channel_lock(chan);
	ast_set_flag(chan, AST_FLAG_BRIDGE_HANGUP_DONT);			/* don't let the after-bridge code run the h-exten */
	//ast_channel_unlock(chan);
#        endif
//      }

	struct ast_channel *bridge = CS_AST_BRIDGED_CHANNEL(chan);

/* Prevent also the bridge from hanging up. */
	if (NULL != bridge) {
#        if 0
		//ast_channel_lock(bridge);
		ast_set_flag(bridge, AST_FLAG_BRIDGE_HANGUP_DONT);		/* don't let the after-bridge code run the h-exten */
		//ast_channel_unlock(bridge);
#        endif
	}

	/* Allocate an asterisk channel structure as conference bridge peer for the participant */
	participant->conferenceBridgePeer = ast_channel_alloc(0, chan->_state, 0, 0, chan->accountcode, chan->exten, chan->context, chan->amaflags, "ConferenceBridge/%s", chan->name);

	if (!participant->conferenceBridgePeer) {
		ast_log(LOG_NOTICE, "Couldn't allocate participant peer.\n");
		//pbx_channel_unlock(chan);
		return -1;
	}

	participant->conferenceBridgePeer->readformat = chan->readformat;
	participant->conferenceBridgePeer->writeformat = chan->writeformat;
	participant->conferenceBridgePeer->nativeformats = chan->nativeformats;

	if (ast_channel_masquerade(participant->conferenceBridgePeer, chan)) {
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

	if (NULL == participant->channel) {
		participant->channel = get_sccp_channel_from_ast_channel(participant->conferenceBridgePeer);

		if (participant->channel && participant->channel->device && participant->channel->line) {	// Talking to an SCCP device
			ast_log(LOG_NOTICE, "SCCP: Conference: Member #%d (SCCP channel) being added to conference. Identifying sccp channel: %s\n", participant->id, sccp_channel_toString(participant->channel));
		} else {
			ast_log(LOG_NOTICE, "SCCP: Conference: Member #%d (Non SCCP channel) being added to conference.\n", participant->id);
			participant->channel = NULL;
		}
	} else {
		ast_log(LOG_NOTICE, "SCCP: Conference: We already have a sccp channel for this participant; assuming moderator.\n");
		/* In the future we may need to consider that / if the moderator changes the sccp channel due to creating new channel for the moderator */
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_addAstChannelToConferenceBridge.\n");
	if (ast_pthread_create_background(&participant->joinThread, NULL, sccp_conference_join_thread, participant) < 0) {
		// \todo TODO: error handling
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
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_addParticipant called.\n");

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
	if (NULL == remoteCallLeg) {
		ast_log(LOG_NOTICE, "%s: Conference: NULL remote ast call leg: %s-%08x\n", DEV_ID_LOG(channel->device), channel->line->name, channel->callid);
		return;
	}

	if (adding_moderator) {
		localParticipant = (sccp_conference_participant_t *) ast_malloc(sizeof(sccp_conference_participant_t));
		if (!localParticipant) {
			return;
		}
		memset(localParticipant, 0, sizeof(sccp_conference_participant_t));

		pbx_channel_lock(localCallLeg);
	}

	remoteParticipant = (sccp_conference_participant_t *) ast_malloc(sizeof(sccp_conference_participant_t));
	if (!remoteParticipant) {
		if (adding_moderator) {
			pbx_channel_unlock(localCallLeg);
			free(localParticipant);
		}
		return;
	}
	
	memset(remoteParticipant, 0, sizeof(sccp_conference_participant_t));

	pbx_channel_lock(remoteCallLeg);

	if (adding_moderator) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Adding remote party of moderator.\n");
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Adding remote party of ordinary participant call.\n");
	}

	remoteParticipant->conference = conference;
	remoteParticipant->id = SCCP_LIST_GETSIZE(conference->participants) + 1;
	ast_bridge_features_init(&remoteParticipant->features);

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, remoteParticipant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	if (0 != sccp_conference_addAstChannelToConferenceBridge(remoteParticipant, remoteCallLeg)) {
		// \todo TODO: error handling
	}

	if (adding_moderator) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Adding local party of moderator.\n");

		localParticipant->conference = conference;
		ast_bridge_features_init(&remoteParticipant->features);

		SCCP_LIST_LOCK(&conference->participants);
		SCCP_LIST_INSERT_TAIL(&conference->participants, localParticipant, list);
		SCCP_LIST_UNLOCK(&conference->participants);

		if (0 != sccp_conference_addAstChannelToConferenceBridge(localParticipant, localCallLeg)) {
			// \todo TODO: error handling
		}
	}

	pbx_channel_unlock(remoteCallLeg);

	if (adding_moderator) {
		pbx_channel_unlock(localCallLeg);
	}

	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: member #%d -- %s\n", conference->id, part->id, sccp_channel_toString(part->channel));
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
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_conference_participant_t *part = NULL;
	char leaveMessage[256] = { '\0' };

	/* Update the presence of the moderator */
	if (participant == conference->moderator) {
		conference->moderator = NULL;
	}

	/* Notify the moderator of the leaving party. */
	if (NULL != conference->moderator) {
		int instance;

		instance = sccp_device_find_index_for_line(conference->moderator->channel->device, conference->moderator->channel->line->name);
		ast_log(LOG_NOTICE, "Conference: Leave Notification for Participant #%d -- %s\n", participant->id, sccp_channel_toString(participant->channel));
		snprintf(leaveMessage, 255, "Member #%d left conference.", participant->id);
		sccp_dev_displayprompt(conference->moderator->channel->device, instance, conference->moderator->channel->callid, leaveMessage, 10);
	}
	/* \todo hangup channel / sccp_channel for removed participant when called via KICK action - DdG */
//	sccp_channel_lock(participant->channel);
//	sccp_channel_endcall_locked(participant->channel);
//	sccp_channel_unlock(participant->channel);
	/* \todo jump back to the EXITCONTEXT to resume their dialplan handling */

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		if (part == participant) {
			if (part->channel) {					// sccp device
				part->channel->conference = NULL;
				// TODO: Indicate SCCP device it is in conference no longer.
			}
			SCCP_LIST_REMOVE(&conference->participants, part, list);
			ast_free(part);
		}

	}
	SCCP_LIST_UNLOCK(&conference->participants);

	SCCP_LIST_TRAVERSE(&conference->participants, part, list) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference %d: member #%d -- %s\n", conference->id, participant->id, sccp_channel_toString(part->channel));
	}

	if (conference->participants.size < 1)
		sccp_conference_end(conference);
}

/*!
 * Retract participating channel from conference
 *
 * @param conference conference
 * @param channel SCCP Channel
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
void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel)
{
	sccp_conference_participant_t *part = NULL;

	SCCP_LIST_LOCK(&channel->conference->participants);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&channel->conference->participants, part, list) {
		if (part->channel == channel) {
			sccp_conference_removeParticipant(conference, part);
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END SCCP_LIST_UNLOCK(&channel->conference->participants);
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
	SCCP_LIST_HEAD_INIT(&conferences);
}

/*!
 * Join Conference Thread
 *
 * @param data Data
 */
static void *sccp_conference_join_thread(void *data)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: entering join thread.\n");

	sccp_conference_participant_t *participant = (sccp_conference_participant_t *) data;

	struct ast_channel *astChannel;

	if (NULL == participant) {
		ast_log(LOG_ERROR, "SCCP: Conference: Join thread: NULL participant.\n");
		return NULL;
	}

	astChannel = participant->conferenceBridgePeer;

	if (!astChannel) {
		ast_log(LOG_ERROR, "SCCP: Conference: Join thread: no ast channel for conference.\n");
		participant->joinThread = AST_PTHREADT_NULL;
		return NULL;
	} else {
		ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: adding channel to bridge: %s\n", sccp_channel_toString(participant->channel));
	}

	ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering ast_bridge_join: %s\n", sccp_channel_toString(participant->channel));

	/* set keyset and displayprompt for conference */
	/* Note (DD): This should be done by indicating conference call state to device. */
	if (participant->channel) {						// if talking to an SCCP device
		int instance;

		instance = sccp_device_find_index_for_line(participant->channel->device, participant->channel->line->name);
		/* if ((channel = participant->conference->moderator->channel)) { */
		if ((participant->channel == participant->conference->moderator->channel)) {	// Beware of accidental assignment!
			ast_log(LOG_NOTICE, "Conference: Set KeySet/Displayprompt for Moderator %s\n", sccp_channel_toString(participant->channel));
			sccp_dev_set_keyset(participant->channel->device, instance, participant->channel->callid, KEYMODE_CONNCONF);
			sccp_dev_displayprompt(participant->channel->device, instance, participant->channel->callid, "Started Conference", 10);
		} else {
			ast_log(LOG_NOTICE, "Conference: Set DisplayPrompt for Participant %s\n", sccp_channel_toString(participant->channel));
			sccp_dev_displayprompt(participant->channel->device, instance, participant->channel->callid, "Entered Conference", 10);
		}
	}

	ast_bridge_join(participant->conference->bridge, astChannel, NULL, &participant->features);
	ast_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving ast_bridge_join: %s\n", sccp_channel_toString(participant->channel));

	if (ast_pbx_start(astChannel)) {
		ast_log(LOG_WARNING, "SCCP: Unable to start PBX on %s\n", participant->conferenceBridgePeer->name);
		ast_hangup(astChannel);
		// return -1;
		return NULL;
	}

	sccp_conference_removeParticipant(participant->conference, participant);

	participant->joinThread = AST_PTHREADT_NULL;

	return NULL;
}

/*!
 * Is this participant a moderator on this conference
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 * 
 * \return Boolean
 */
boolean_t isModerator(sccp_conference_participant_t * participant, sccp_channel_t * channel)
{
	if (participant && channel && participant->channel && participant->channel == channel)
		return TRUE;
	else
		return FALSE;
}

/*!
 * \brief Show Conference List
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
//		UserData:INTEGER:STRING
//		UserDataSoftKey:STRING:INTEGER:STRING
//		UserCallDataSoftKey:STRING:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
//		UserCallData:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
 */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel)
{
	int use_icon = 0;
	uint32_t appID = APPID_CONFERENCE;

	if (!conference)
		return;
		
	if (!channel)
		return;

	if (conference->participants.size < 1)
		return;
		
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Sending ConferenceList to Channel %d\n", channel->device->id, channel->callid);

#        if ASTERISK_VERSION_NUM >= 10400
	unsigned int transactionID = ast_random();
#        else
	unsigned int transactionID = random();
#        endif

	char xmlStr[2048] = "";
	char xmlTemp[512] = "";
	sccp_conference_participant_t *participant;

	strcat(xmlStr, "<CiscoIPPhoneIconMenu>\n");				// Will be CiscoIPPhoneIconMenu with icons for participant, moderator, muted, unmuted
	strcat(xmlStr, "<Title>Conference List</Title>\n");
	strcat(xmlStr, "<Prompt>Make Your Selection</Prompt>\n");

	// MenuItems
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		strcat(xmlStr, "<MenuItem>\n");

		if (isModerator(participant, channel))
			use_icon = 0;
		else
			use_icon = 2;

		if (participant->muted) {
			++use_icon;
		}
		strcat(xmlStr, "  <IconIndex>");
		sprintf(xmlTemp, "%d", use_icon);
		strcat(xmlStr, xmlTemp);
		strcat(xmlStr, "</IconIndex>\n");

		strcat(xmlStr, "  <Name>");
		if (participant->channel) {
			//TODO check in-/outbound direction
			strcat(xmlStr, participant->channel->callInfo.callingPartyName);
			sprintf(xmlTemp, " (%d)", participant->id);
			strcat(xmlStr, xmlTemp);
		} else {
			// \todo get callerid info from asterisk channel
		}
		strcat(xmlStr, "</Name>\n");

		strcat(xmlStr, "  <URL>");
		sprintf(xmlTemp, "UserCallData:%d:%d:%d:%d:%d", appID, conference->id, channel->callid, transactionID, participant->id);
		strcat(xmlStr, xmlTemp);
		strcat(xmlStr, "</URL>\n");
		strcat(xmlStr, "</MenuItem>\n");
	}
	SCCP_LIST_UNLOCK(&conference->participants);

	// SoftKeys
	strcat(xmlStr, "<SoftKeyItem>\n");
	strcat(xmlStr, "  <Name>Update</Name>\n");
	strcat(xmlStr, "  <Position>1</Position>\n");
	strcat(xmlStr, "  <URL>QueryStringParam:action=update</URL>\n");
	strcat(xmlStr, "</SoftKeyItem>\n");

	if ((channel == conference->moderator->channel)) {
		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>ToggleMute</Name>\n");
		strcat(xmlStr, "  <Position>2</Position>\n");
		strcat(xmlStr, "  <URL>");
		sprintf(xmlTemp, "UserDataSoftKey:Select:1:MUTE:%d:%d:%d:%d", appID, conference->id, channel->callid, transactionID);
		strcat(xmlStr, xmlTemp);
		strcat(xmlStr, "</URL>\n");
		strcat(xmlStr, "</SoftKeyItem>\n");

		strcat(xmlStr, "<SoftKeyItem>\n");
		strcat(xmlStr, "  <Name>Kick</Name>\n");
		strcat(xmlStr, "  <Position>3</Position>\n");
		strcat(xmlStr, "  <URL>");
		sprintf(xmlTemp, "UserDataSoftKey:Select:2:KICK:%d:%d:%d", conference->id, channel->callid, transactionID);
		strcat(xmlStr, xmlTemp);
		strcat(xmlStr, "</URL>\n");
		strcat(xmlStr, "</SoftKeyItem>\n");
	}

	strcat(xmlStr, "<SoftKeyItem>\n");
	strcat(xmlStr, "  <Name>Exit</Name>\n");
	strcat(xmlStr, "  <Position>4</Position>\n");
	strcat(xmlStr, "  <URL>SoftKey:Exit</URL>\n");
	strcat(xmlStr, "</SoftKeyItem>\n");

	// CiscoIPPhoneIconMenu Icons
	strcat(xmlStr, "<IconItem>\n");						// moderator
	strcat(xmlStr, "  <Index>0</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C03F3000C03FF000C03FF003000FF00FFCFFF30FFCFFF303CC3FF300CC3F330000000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");						// muted moderator
	strcat(xmlStr, "  <Index>1</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C03FF03CC03FF03CC03FF03C000FF03CFCFFF33CFCFFF33CCC3FF33CCC3FF33C00000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");						// participant
	strcat(xmlStr, "  <Index>2</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C0303000C030F000C030F003000FF00FFCF0F30F0C00F303CC30F300CC30330000000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "<IconItem>\n");						// muted participant
	strcat(xmlStr, "  <Index>3</Index>\n");
	strcat(xmlStr, "  <Height>10</Height>\n");
	strcat(xmlStr, "  <Width>16</Width>\n");
	strcat(xmlStr, "  <Depth>2</Depth>\n");
	strcat(xmlStr, "  <Data>000F0000C030F03CC030F03CC030F03C000FF03CFCF0F33C0C00F33CCC30F33CCC30F33C00000000</Data>\n");
	strcat(xmlStr, "</IconItem>\n");

	strcat(xmlStr, "</CiscoIPPhoneIconMenu>\n");
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "xml-message:\n%s\n", xmlStr);

	sccp_moo_t *r1 = NULL;

	int xml_data_len, msgSize, hdr_len, padding;

	xml_data_len = strlen(xmlStr);
	hdr_len = 40 - 1;
	padding = ((xml_data_len + hdr_len) % 4);
	padding = (padding > 0) ? 4 - padding : 0;
	msgSize = hdr_len + xml_data_len + padding;

	r1 = sccp_build_packet(UserToDeviceDataVersion1Message, msgSize);
	r1->msg.UserToDeviceDataVersion1Message.lel_appID = appID;
	r1->msg.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(conference->id);
	r1->msg.UserToDeviceDataVersion1Message.lel_callReference = htolel(channel->callid);
	r1->msg.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
	r1->msg.UserToDeviceDataVersion1Message.lel_sequenceFlag = 0x0002;
	r1->msg.UserToDeviceDataVersion1Message.lel_displayPriority = 0x0002;
//	r1->msg.UserToDeviceDataVersion1Message.lel_conferenceID = htolel(conference->id);	// conferenceId send via lineInstance
	r1->msg.UserToDeviceDataVersion1Message.lel_dataLength = htolel(xml_data_len);

	if (xml_data_len) {
		char buffer[xml_data_len + 2];

		memset(&buffer[0], 0, sizeof(buffer));
		memcpy(&buffer[0], xmlStr, xml_data_len);

		memcpy(&r1->msg.UserToDeviceDataVersion1Message.xml_data, &buffer[0], sizeof(buffer));
		sccp_dev_send(channel->device, r1);
	}
}

void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle DTU SoftKey Button Press for CallID %d, Transaction %d, Conference %d, participant: %d\n", d->id, callReference, transactionID, conferenceID, participantID);

	if (!d)
		return;

	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *participant = NULL;

	/* lookup participant by id */
	if (!(conference = sccp_conference_find_byid(conferenceID))) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference not found", d->id);
		goto EXIT;
	}

	/* lookup participant by id */
	if (!(participant = sccp_conference_participant_find_byid(conference, participantID))) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference Participant not found", d->id);
		goto EXIT;
	}

	sccp_device_lock(d);
	if (d && d->dtu_softkey.transactionID == transactionID) {

		/* Handle SoftKey Events for selected conference / participant */
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: DTU Softkey Action %s", d->id, d->dtu_softkey.action);
		if (!strcmp(d->dtu_softkey.action, "MUTE")) {
			sccp_conference_toggle_mute_participant(conference, participant);
		} else if (!strcmp(d->dtu_softkey.action, "KICK")) {
			sccp_conference_kick_participant(conference,participant);
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: DTU TransactionID does not match or device not found (%d <> %d)", d->id, d->dtu_softkey.transactionID, transactionID);
	}

 EXIT:
	/* reset softkey state for next button press */
	d->dtu_softkey.action = "";
	d->dtu_softkey.appID = 0;
	d->dtu_softkey.payload = 0;
	d->dtu_softkey.transactionID = 0;

	sccp_device_unlock(d);
	if (conference) {
		sccp_conference_show_list(conference, conference->moderator->channel); 
	}
}

/*!
 * \brief Kick Participant from Conference
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 */
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle Kick for for ConferenceParticipant %d\n", participant->channel->device->id, participant->id);

	sccp_dev_displayprinotify(participant->channel->device, "You have been kicked out of the Conference", 5, 5);
	sccp_dev_displayprompt(participant->channel->device, 0, participant->channel->callid, "You have been kicked out of the Conference", 5);

	sccp_conference_removeParticipant(conference, participant);

	sccp_dev_displayprinotify(conference->moderator->channel->device, "Participant has been kicked out", 5, 2);
	sccp_dev_displayprompt(conference->moderator->channel->device, 0, conference->moderator->channel->callid, "Participant has been kicked out", 2);
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Participant %d(%d) Kicked from Conference %d", conference->moderator->channel->device->id, participant->id, participant->channel->callid, conference->id);
}

/*!
 * \brief Toggle Mute Conference Participant
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 */
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle Mute for for ConferenceParticipant %d\n", participant->channel->device->id, participant->id);
	if (participant->muted) {
		// enable audio for recipient
		sccp_dev_displayprinotify(participant->channel->device, "Unmuted", 5, 5);
		sccp_dev_displayprompt(participant->channel->device, 0, participant->channel->callid, "Unmuted", 5);

		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: enable audio for %d\n", participant->channel->device->id, participant->id);
		/* \todo implement confbridge unmute code */

		sccp_dev_displayprinotify(conference->moderator->channel->device, "Participant has been unmuted", 5, 2);
		sccp_dev_displayprompt(conference->moderator->channel->device, 0, conference->moderator->channel->callid, "Participant has been unmuted", 5);
	} else {
		// disable audio for recipient
		sccp_dev_displayprinotify(participant->channel->device, "Mute", 5, 0);	//no timeout
		sccp_dev_displayprompt(participant->channel->device, 0, participant->channel->callid, "Mute", 0);  //no timeout

		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: disable audio for %d\n", participant->channel->device->id, participant->id);
		/* \todo implement confbridge mute code */

		sccp_dev_displayprinotify(conference->moderator->channel->device, "Participant has been muted", 5, 2);
		sccp_dev_displayprompt(conference->moderator->channel->device, 0, conference->moderator->channel->callid, "Participant has been muted", 5);
	}
}

/*!
 * \brief Promote Participant to Moderator
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 *
 * \todo how to handle multiple moderators
 */
void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

/*!
 * \brief Demode Moderator to Participant
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 *
 * \todo how to handle multiple moderators
 */
void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

/*!
 * \brief Invite Remote Participant
 *
 * Allows us to enter a phone number and return to the conference immediatly. 
 * Participant is called in a seperate thread, played a message that he/she has been invited to join this conference
 *  and will be added to the conference upon accept.
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 *
 * \todo how to handle multiple moderators
 */
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
}

/*!
 * Find conference by id
 *
 * \param id ID as uint32_t
 * \returns sccp_conference_participant_t
 *
 * \warning
 * 	- conference->participants is not always locked
 */
sccp_conference_t *sccp_conference_find_byid(uint32_t id)
{
	sccp_conference_t *conference = NULL;

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCP: Looking for conference by id %u\n", id);
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_TRAVERSE(&conferences, conference, list) {
		if (conference->id == id) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "SCCP: Found conference (%d)\n", conference->id);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conferences);
	return conference;
}

/*!
 * Find participant in conference by id
 *
 * \param channel SCCP Channel
 * \param id ID as uint32_t
 * \returns sccp_conference_participant_t
 *
 * \warning
 * 	- conference->participants is not always locked
 */
sccp_conference_participant_t *sccp_conference_participant_find_byid(sccp_conference_t * conference, uint32_t id)
{
	sccp_conference_participant_t *participant = NULL;

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "SCCP: Looking for participant by id %u\n", id);

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->id == id) {
			sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found participant (%d)\n", DEV_ID_LOG(participant->channel->device), participant->id);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
}

#    endif									// ASTERISK_VERSION_NUM
#endif										// CS_SCCP_CONFERENCE
