
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
#    if ASTERISK_VERSION_NUMBER >= 10602
#        include "asterisk/astobj2.h"
static int lastConferenceID = 99;

/* internal structure */

SCCP_LIST_HEAD(, sccp_conference_t) conferences;				/*!< our list of conferences */

static void *sccp_conference_join_thread(void *data);
boolean_t isModerator(sccp_conference_participant_t * participant, sccp_channel_t * channel);
sccp_conference_t *sccp_conference_find_byid(uint32_t id);
sccp_conference_participant_t *sccp_conference_participant_find_byid(sccp_conference_t * conference, uint32_t id);

/*!
 * \brief Create conference
 *
 * \param owner conference owner
 * \return conference
 *
 * \todo Implement SCCP:StartAnnouncementMessage SCCP:StopAnnouncementMessage
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t * owner)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: sccp_conference_create called.\n");

	sccp_conference_t *conference = NULL;

	sccp_conference_participant_t *moderator = NULL;

	//int   confCapabilities = AST_BRIDGE_CAPABILITY_MULTIMIX | AST_BRIDGE_CAPABILITY_THREAD | AST_BRIDGE_CAPABILITY_VIDEO;

	if (NULL == owner) {
		pbx_log(LOG_ERROR, "SCCP: Conference: NULL owner (sccp channel) while creating new conference.\n");
		return NULL;
	}

	if (NULL == owner->owner) {
		pbx_log(LOG_ERROR, "SCCP: Conference: NULL owner (ast channel) while creating new conference.\n");
		return NULL;
	}

	/* create conference */
	conference = (sccp_conference_t *) sccp_malloc(sizeof(sccp_conference_t));
	if (NULL == conference) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference.\n");
		return NULL;
	}

	memset(conference, 0, sizeof(sccp_conference_t));

	conference->id = ++lastConferenceID;
	SCCP_LIST_HEAD_INIT(&conference->participants);
	conference->bridge = pbx_bridge_new(AST_BRIDGE_CAPABILITY_1TO1MIX, AST_BRIDGE_FLAG_SMART);

	if (NULL == conference->bridge) {
		pbx_log(LOG_ERROR, "SCCP: Conference: Conference: conference bridge could not be created.\n");
		return NULL;
	}

	/* create moderator */
	moderator = (sccp_conference_participant_t *) sccp_malloc(sizeof(sccp_conference_participant_t));

	if (!moderator) {
		pbx_log(LOG_ERROR, "SCCP: Conference: cannot alloc memory for new conference moderator.\n");
		sccp_free(conference);
		return NULL;
	}

	/* Initialize data structures */

	memset(moderator, 0, sizeof(sccp_conference_participant_t));

	pbx_bridge_features_init(&moderator->features);

//      pbx_mutex_init(&moderator->cond_lock);
//      pbx_cond_init (&moderator->removed_cond_signal, NULL);

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
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to add moderator channel (preparation phase).\n");
		/* TODO: Error handling. */
	}
#        endif

	/* Store conference in global list. */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference: adding conference to global conference list.\n", sccp_channel_getDevice(owner)->id);
	SCCP_LIST_LOCK(&conferences);
	SCCP_LIST_INSERT_HEAD(&conferences, conference, list);
	SCCP_LIST_UNLOCK(&conferences);

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference: Conference with id %d created; Owner: %s \n", sccp_channel_getDevice(owner)->id, conference->id, sccp_channel_getDevice(owner)->id);

#        if 0
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_create.\n");
	if (pbx_pthread_create_background(&moderator->joinThread, NULL, sccp_conference_join_thread, moderator) < 0) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to initiate join thread for moderator.\n");
		return conference;
	}
#        endif

	return conference;
}

/*!
 * \brief Add channel to conference bridge
 * 
 * \lock
 * 	- asterisk channel
 */
int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t * participant, PBX_CHANNEL_TYPE * chan)
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

	if (chan->pbx) {
		//pbx_channel_lock(chan);
		pbx_set_flag(chan, AST_FLAG_BRIDGE_HANGUP_DONT);		/*! don't let the after-bridge code run the h-exten */
		pbx_set_flag(chan, AST_FLAG_NBRIDGE);				/*! This channel is in a native bridge */
		pbx_set_flag(chan, AST_FLAG_BLOCKING);				/*! a thread is blocking on this channel */
		//ast_channel_unlock(chan);
	}
	//PBX_CHANNEL_TYPE *bridge = CS_AST_BRIDGED_CHANNEL(chan);

	/* Allocate an asterisk channel structure as conference bridge peer for the participant */

	/** \todo make this pbx independent */

//      participant->conferenceBridgePeer = ast_channel_alloc(0, chan->_state, 0, 0, chan->accountcode, chan->exten, chan->context, "", chan->amaflags, "ConferenceBridge/%s", chan->name);
//      if (!participant->conferenceBridgePeer) {
//              pbx_log(LOG_NOTICE, "Couldn't allocate participant peer.\n");
//              return -1;
//      }

	if (!PBX(alloc_pbxChannel) (get_sccp_channel_from_pbx_channel(chan), &participant->conferenceBridgePeer)) {
		pbx_log(LOG_NOTICE, "Couldn't allocate participant peer.\n");
		//pbx_channel_unlock(chan);
		return -1;
	}
	participant->conferenceBridgePeer->_state = AST_STATE_DOWN;

	participant->conferenceBridgePeer->readformat = chan->readformat;
	participant->conferenceBridgePeer->writeformat = chan->writeformat;
	participant->conferenceBridgePeer->nativeformats = chan->nativeformats;
	
	/* save original channel */
	participant->origChannel = chan;

	if (pbx_channel_masquerade(participant->conferenceBridgePeer, participant->origChannel)) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade channel.\n");
		//pbx_channel_unlock(chan);
		PBX(requestHangup) (participant->conferenceBridgePeer);
		participant->conferenceBridgePeer = NULL;
		return -1;
	} else {
		pbx_channel_lock(participant->conferenceBridgePeer);
		pbx_do_masquerade(participant->conferenceBridgePeer);/** \todo make this pbx independent */
		pbx_channel_unlock(participant->conferenceBridgePeer);

		/* assign hangup cause to origChannel*/
		participant->origChannel->hangupcause = AST_CAUSE_NORMAL_UNSPECIFIED;
	}

	if (NULL == participant->channel) {
		participant->channel = get_sccp_channel_from_pbx_channel(participant->conferenceBridgePeer);

		if (participant->channel && sccp_channel_getDevice(participant->channel) && participant->channel->line) {	// Talking to an SCCP device
			pbx_log(LOG_NOTICE, "SCCP: Conference: Member #%d (SCCP channel) being added to conference. Identifying sccp channel: %s\n", participant->id, sccp_channel_toString(participant->channel));
		} else {
			pbx_log(LOG_NOTICE, "SCCP: Conference: Member #%d (Non SCCP channel) being added to conference.\n", participant->id);
			participant->channel = NULL;
		}
	} else {
		pbx_log(LOG_NOTICE, "SCCP: Conference: We already have a sccp channel for this participant; assuming moderator.\n");
		/* In the future we may need to consider that / if the moderator changes the sccp channel due to creating new channel for the moderator */
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Establishing Join thread via sccp_conference_addAstChannelToConferenceBridge.\n");
	if (pbx_pthread_create_background(&participant->joinThread, NULL, sccp_conference_join_thread, participant) < 0) {
		// \todo TODO: error handling
	}
	// Now done in add_participant
	// pbx_channel_unlock(chan);

	return 0;
}

/*!
 * \brief Add participant to conference
 *
 * \param conference SCCP conference
 * \param channel SCCP channel
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

	//PBX_CHANNEL_TYPE *currentParticipantPeer;

	PBX_CHANNEL_TYPE *localCallLeg = NULL, *remoteCallLeg = NULL;

	/* We need to handle adding the moderator in a special way: Both legs of the call
	   need to be added to the conference at the same time. (-DD) */
	boolean_t adding_moderator = (channel == conference->moderator->channel);

	if (NULL != channel->conference && !adding_moderator) {
		pbx_log(LOG_NOTICE, "%s: Conference: Already in conference: %s-%08x\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);
		return;
	}

	if (channel->state != SCCP_CHANNELSTATE_HOLD && channel->state != SCCP_CHANNELSTATE_CONNECTED) {
		pbx_log(LOG_NOTICE, "%s: Conference: Only connected or channel on hold eligible for conference: %s-%08x\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);
		return;
	}

	localCallLeg = channel->owner;
	if (NULL == localCallLeg) {
		pbx_log(LOG_NOTICE, "%s: Conference: NULL local ast call leg: %s-%08x\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);
		return;
	}

	remoteCallLeg = CS_AST_BRIDGED_CHANNEL(localCallLeg);
	if (NULL == remoteCallLeg) {
		pbx_log(LOG_NOTICE, "%s: Conference: NULL remote ast call leg: %s-%08x\n", DEV_ID_LOG(sccp_channel_getDevice(channel)), channel->line->name, channel->callid);
		return;
	}

	if (adding_moderator) {
		localParticipant = (sccp_conference_participant_t *) sccp_malloc(sizeof(sccp_conference_participant_t));
		if (!localParticipant) {
			return;
		}
		memset(localParticipant, 0, sizeof(sccp_conference_participant_t));

		pbx_channel_lock(localCallLeg);
	}
	remoteParticipant = (sccp_conference_participant_t *) sccp_malloc(sizeof(sccp_conference_participant_t));
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

//      pbx_mutex_init(&remoteParticipant->cond_lock);
//      pbx_cond_init (&remoteParticipant->removed_cond_signal, NULL);
	remoteParticipant->conference = conference;
	remoteParticipant->id = SCCP_LIST_GETSIZE(conference->participants) + 1;
	pbx_bridge_features_init(&remoteParticipant->features);

	/* get the exitcontext, to jump to after hangup */

	pbx_channel_lock(remoteCallLeg);
	if (!pbx_strlen_zero(remoteCallLeg->macrocontext)) {
		pbx_copy_string(remoteParticipant->exitcontext, remoteCallLeg->macrocontext, sizeof(remoteParticipant->exitcontext));
		pbx_copy_string(remoteParticipant->exitexten, remoteCallLeg->macroexten, sizeof(remoteParticipant->exitexten));
		remoteParticipant->exitpriority = remoteCallLeg->macropriority;
	} else {
		pbx_copy_string(remoteParticipant->exitcontext, remoteCallLeg->context, sizeof(remoteParticipant->exitcontext));
		pbx_copy_string(remoteParticipant->exitexten, remoteCallLeg->exten, sizeof(remoteParticipant->exitexten));
		remoteParticipant->exitpriority = remoteCallLeg->priority;
	}
	ast_channel_unlock(remoteCallLeg);

	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_INSERT_TAIL(&conference->participants, remoteParticipant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

	if (!adding_moderator) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Adding remote party.\n");
		if (0 != sccp_conference_addAstChannelToConferenceBridge(remoteParticipant, remoteCallLeg)) {
			// \todo TODO: error handling
		}
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: Adding local party of moderator.\n");

		localParticipant->conference = conference;
		pbx_bridge_features_init(&remoteParticipant->features);

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

	if (conference && sccp_channel_getDevice(conference->moderator->channel)->conferencelist_active) {
		sccp_conference_show_list(conference, conference->moderator->channel);
	}
}

/*!
 * \brief Remove participant from conference
 *
 * \param conference conference
 * \param participant participant to remove
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
	if (!conference || !participant->conferenceBridgePeer)
		return;

	PBX_CHANNEL_TYPE *ast_channel = participant->conferenceBridgePeer;

	char leaveMessage[256] = { '\0' };

	/* Update the presence of the moderator */
	if (participant == conference->moderator) {
		conference->moderator = NULL;
	}

	/* Notify the moderator of the leaving party. */

	/* \todo we need an sccp.conf conference option to specify the action that needs to be taken when the 
	 * moderator leaves the conference, i.e.:
	 *  - take down the conference
	 *  - moderator needs to promote a participant before being able to leave the conference
	 *  - continue conference and auto choose new moderator (using the first sccp phone with a local ip-address it finds )
	 */
	if (NULL != conference->moderator) {
		int instance;

		instance = sccp_device_find_index_for_line(sccp_channel_getDevice(conference->moderator->channel), conference->moderator->channel->line->name);
		pbx_log(LOG_NOTICE, "Conference: Leave Notification for Participant #%d -- %s\n", participant->id, sccp_channel_toString(participant->channel));
		snprintf(leaveMessage, 255, "Member #%d left conference.", participant->id);
		sccp_dev_displayprompt(sccp_channel_getDevice(conference->moderator->channel), instance, conference->moderator->channel->callid, leaveMessage, 10);
	}
	
	/* masquarade back to origChannel */
	if (pbx_channel_masquerade(participant->origChannel, ast_channel) {
		pbx_log(LOG_ERROR, "SCCP: Conference: failed to masquerade back to orig channel.\n");
		PBX(requestHangup) (participant->conferenceBridgePeer);
		PBX(requestHangup) (participant->origChannel);
		participant->conferenceBridgePeer = NULL;
		return -1;
	} else {
		pbx_channel_lock(participant->origChannel);
		pbx_do_masquerade(participant->origChannel);/** \todo make this pbx independent */
		pbx_channel_unlock(participant->origChannel);
	}

	/* \todo hangup channel / sccp_channel for removed participant when called via KICK action - DdG */
	if (ast_channel && !pbx_check_hangup(participant->conferenceBridgePeer)) {
		pbx_channel_lock(participant->conferenceBridgePeer);
		participant->conferenceBridgePeer->flasg |= AST_FLAG_ZOMBIE
		participant->conferenceBridgePeer->_softhangup |= AST_SOFTHANGUP_EXPLICIT;
		participant->conferenceBridgePeer->hangupcause = AST_CAUSE_REDIRECTED_TO_NEW_DESTINATION;
		participant->conferenceBridgePeer->_state = AST_STATE_DOWN;
		participant->conferenceBridgePeer_unlock(participant->conferenceBridgePeer);
		participant->conferenceBridgePeer = NULL;
	}

	/* \todo hangup channel / sccp_channel for removed participant when called via KICK action - DdG */
	if (ast_channel && !pbx_check_hangup(ast_channel)) {
		pbx_channel_lock(ast_channel);
		ast_channel->_softhangup |= AST_SOFTHANGUP_DEV;
//		ast_channel->_softhangup |= AST_SOFTHANGUP_ASYNCGOTO;
		ast_channel->hangupcause = AST_CAUSE_NORMAL_UNSPECIFIED;
		ast_channel->_state = AST_STATE_DOWN;
		ast_channel_unlock(ast_channel);
		ast_channel = NULL;
	}

	if (participant->channel) {						// sccp device
		participant->channel->conference = NULL;
	}
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_REMOVE(&conference->participants, participant, list);
	SCCP_LIST_UNLOCK(&conference->participants);

/*
	pbx_mutex_lock(&participant->cond_lock);
	participant->removed=TRUE;
	pbx_cond_broadcast(&participant->removed_cond_signal);
	pbx_mutex_unlock(&participant->cond_lock);
*/
	if (conference->participants.size < 1) {				// \todo should this not be 2 ?
		sccp_conference_end(conference);
	} else {
		if ((NULL != conference->moderator)
		    && (NULL != conference->moderator->channel)
		    && (NULL != sccp_channel_getDevice(conference->moderator->channel))
		    && sccp_channel_getDevice(conference->moderator->channel)->conferencelist_active) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "UPDATING CONFLIST\n");
			sccp_conference_show_list(conference, conference->moderator->channel);
		}
	}
}

/*!
 * \brief Retract participating channel from conference
 *
 * \param conference conference
 * \param channel SCCP Channel
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
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&channel->conference->participants);
}

/*!
 * \brief End conference
 *
 * \param conference conference
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
		sccp_free(part);
	}
	SCCP_LIST_UNLOCK(&conference->participants);

	SCCP_LIST_HEAD_DESTROY(&conference->participants);
	sccp_free(conference);
}

/*!
 * \brief doing initial actions
 */
void sccp_conference_module_start()
{
	SCCP_LIST_HEAD_INIT(&conferences);
}

/*!
 * \brief Join Conference Thread
 *
 * \param data Data
 */
static void *sccp_conference_join_thread(void *data)
{
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "Conference: entering join thread.\n");

	sccp_conference_participant_t *participant = (sccp_conference_participant_t *) data;

	PBX_CHANNEL_TYPE *astChannel;

	if (NULL == participant) {
		pbx_log(LOG_ERROR, "SCCP: Conference: Join thread: NULL participant.\n");
		return NULL;
	}

	astChannel = participant->conferenceBridgePeer;

	if (!astChannel) {
		pbx_log(LOG_ERROR, "SCCP: Conference: Join thread: no ast channel for conference.\n");
		participant->joinThread = AST_PTHREADT_NULL;
		return NULL;
	} else {
		pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: adding channel to bridge: %s\n", sccp_channel_toString(participant->channel));
	}

	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: entering pbx_bridge_join: %s\n", sccp_channel_toString(participant->channel));

	/* set keyset and displayprompt for conference */
	/* Note (DD): This should be done by indicating conference call state to device. */
	if (participant->channel) {						// if talking to an SCCP device
		int instance;

		instance = sccp_device_find_index_for_line(sccp_channel_getDevice(participant->channel), participant->channel->line->name);
		if (participant->channel == participant->conference->moderator->channel) {	// Beware of accidental assignment!
			pbx_log(LOG_NOTICE, "Conference: Set KeySet/Displayprompt for Moderator %s\n", sccp_channel_toString(participant->channel));
			sccp_dev_set_keyset(sccp_channel_getDevice(participant->channel), instance, participant->channel->callid, KEYMODE_CONNCONF);
			sccp_dev_displayprompt(sccp_channel_getDevice(participant->channel), instance, participant->channel->callid, "Started Conference", 10);
		} else {
			pbx_log(LOG_NOTICE, "Conference: Set DisplayPrompt for Participant %s\n", sccp_channel_toString(participant->channel));
			sccp_dev_displayprompt(sccp_channel_getDevice(participant->channel), instance, participant->channel->callid, "Entered Conference", 10);
		}
	}

	pbx_bridge_join(participant->conference->bridge, astChannel, NULL, &participant->features);
	pbx_log(LOG_NOTICE, "SCCP: Conference: Join thread: leaving pbx_bridge_join: %s\n", sccp_channel_toString(participant->channel));

	if (pbx_pbx_start(astChannel)) {
		pbx_log(LOG_WARNING, "SCCP: Unable to start PBX on %s\n", participant->conferenceBridgePeer->name);
		PBX(requestHangup) (astChannel);
		// return -1;
		return NULL;
	}

	sccp_conference_removeParticipant(participant->conference, participant);

	participant->joinThread = AST_PTHREADT_NULL;

	return NULL;
}

/*!
 * \brief Is this participant a moderator on this conference
 *
 * \param participant SCCP Conference Participant
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
 *
 */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel)
{
	int use_icon = 0;
	uint32_t appID = APPID_CONFERENCE;
	uint32_t callReference = 1;						// callreference should be asterisk_channel->unique identifier

	if (!conference)
		return;

	if (!channel)								// only send this list to sccp phones
		return;

	if (NULL != conference->moderator && conference->participants.size < 1)
		return;

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Sending ConferenceList to Channel %d\n", sccp_channel_getDevice(channel)->id, channel->callid);

#        if ASTERISK_VERSION_NUMBER >= 10400
	unsigned int transactionID = pbx_random();
#        else
	unsigned int transactionID = random();
#        endif

	char xmlStr[2048] = "";
	char xmlTmp[512] = "";
	sccp_conference_participant_t *participant;

	strcat(xmlStr, "<CiscoIPPhoneIconMenu>\n");				// Will be CiscoIPPhoneIconMenu with icons for participant, moderator, muted, unmuted
	strcat(xmlStr, "<Title>Conference List</Title>\n");
	strcat(xmlStr, "<Prompt>Make Your Selection</Prompt>\n");

	// MenuItems
	SCCP_LIST_LOCK(&conference->participants);
	SCCP_LIST_TRAVERSE(&conference->participants, participant, list) {
		if (participant->pendingRemoval)
			continue;

		if (participant->channel == channel && !sccp_channel_getDevice(participant->channel)->conferencelist_active) {
			sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: CONFLIST ACTIVED %d %d\n", sccp_channel_getDevice(channel)->id, channel->callid, participant->id);
			sccp_channel_getDevice(participant->channel)->conferencelist_active = TRUE;
		}

		strcat(xmlStr, "<MenuItem>\n");

		if (isModerator(participant, channel))
			use_icon = 0;
		else
			use_icon = 2;

		if (participant->muted) {
			++use_icon;
		}
		strcat(xmlStr, "  <IconIndex>");
		sprintf(xmlTmp, "%d", use_icon);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</IconIndex>\n");

		strcat(xmlStr, "  <Name>");
		if (participant->channel != NULL) {
			switch (participant->channel->calltype) {
			case SKINNY_CALLTYPE_INBOUND:
				sprintf(xmlTmp, "%d:%s (%s)", participant->id, participant->channel->callInfo.calledPartyName, participant->channel->callInfo.calledPartyNumber);
				break;
			case SKINNY_CALLTYPE_OUTBOUND:
				sprintf(xmlTmp, "%d:%s (%s)", participant->id, participant->channel->callInfo.callingPartyName, participant->channel->callInfo.callingPartyNumber);
				break;
			case SKINNY_CALLTYPE_FORWARD:
				sprintf(xmlTmp, "%d:%s (%s)", participant->id, participant->channel->callInfo.originalCallingPartyName, participant->channel->callInfo.originalCallingPartyName);
				break;
			}
			strcat(xmlStr, xmlTmp);
		} else {							// Asterisk Channel
			//sprintf(xmlTmp, "%d:%s (%s)", participant->id, participant->conferenceBridgePeer->cid.cid_name, participant->conferenceBridgePeer->cid.cid_num);

			/** \todo implement this in the right way */
			sprintf(xmlTmp, "%d:%s (%s)", participant->id, "Dummy CallerIdName", "Dummy CallerIdNumber");
			strcat(xmlStr, xmlTmp);
		}
		strcat(xmlStr, "</Name>\n");

//              UserData:INTEGER:STRING
//              UserDataSoftKey:STRING:INTEGER:STRING
//              UserCallDataSoftKey:STRING:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING
//              UserCallData:INTEGER0:INTEGER1:INTEGER2:INTEGER3:STRING

		sprintf(xmlTmp, "  <URL>UserCallData:%d:%d:%d:%d:%d</URL>", appID, conference->id, callReference, transactionID, participant->id);
		strcat(xmlStr, xmlTmp);
		strcat(xmlStr, "</MenuItem>\n");
	}
	SCCP_LIST_UNLOCK(&conference->participants);

	// SoftKeys
	if (channel && (channel == conference->moderator->channel)) {
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

	sendUserToDeviceVersion1Message(sccp_channel_getDevice(channel), appID, conference->id, callReference, transactionID, xmlStr);
}

/*!
 * \brief Handling ConferenceList Button Event
 *
 * \param d SCCP device
 * \param callReference as uint32_t
 * \param transactionID uint32_t
 * \param conferenceID as uint32_t
 * \param participantID as uint32_t
 */
void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID)
{
	if (!d)
		return;

	sccp_device_lock(d);
	if (d && d->dtu_softkey.transactionID == transactionID) {
		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle DTU SoftKey Button Press for CallID %d, Transaction %d, Conference %d, Participant:%d, Action %s\n", d->id, callReference, transactionID, conferenceID, participantID, d->dtu_softkey.action);
		sccp_conference_t *conference = NULL;
		sccp_conference_participant_t *participant = NULL;

		/* lookup conference by id */
		if (!(conference = sccp_conference_find_byid(conferenceID))) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference not found\n", d->id);
			goto EXIT;
		}

		sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: DTU Softkey Action %s\n", d->id, d->dtu_softkey.action);
		if (!strcmp(d->dtu_softkey.action, "INVITE")) {
			sccp_conference_show_list(conference, conference->moderator->channel);
			sccp_conference_invite_participant(conference, conference->moderator->channel);
		} else if (!strcmp(d->dtu_softkey.action, "EXIT")) {
			sccp_channel_getDevice(conference->moderator->channel)->conferencelist_active = FALSE;
		} else {
			/* lookup participant by id (only necessary for MUTE and KICK actions */
			if (!(participant = sccp_conference_participant_find_byid(conference, participantID))) {
				sccp_log((DEBUGCAT_CORE | DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Conference Participant not found\n", d->id);
				goto EXIT;
			}
			if (!strcmp(d->dtu_softkey.action, "MUTE")) {
				sccp_conference_toggle_mute_participant(conference, participant);
			} else if (!strcmp(d->dtu_softkey.action, "KICK")) {
				if (participant->channel && (conference->moderator->channel == participant->channel)) {
					sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Since wenn du we kick ourselves ? You've got issues!\n", d->id);
				} else {
					sccp_conference_kick_participant(conference, participant);
				}
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

	sccp_device_unlock(d);
}

/*!
 * \brief Kick Participant from Conference
 *
 * \param conference SCCP Conference
 * \param participant SCCP Conference Participant
 */
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
	if (NULL == participant || NULL == conference) {
		return;
	}

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle Kick for for ConferenceParticipant %d\n", DEV_ID_LOG(sccp_channel_getDevice(participant->channel)), participant->id);

	participant->pendingRemoval = 1;					// fakes removal

	if (participant->channel) {
		sccp_dev_displayprinotify(sccp_channel_getDevice(participant->channel), "You have been kicked out of the Conference", 5, 5);
		sccp_dev_displayprompt(sccp_channel_getDevice(participant->channel), 0, participant->channel->callid, "You have been kicked out of the Conference", 5);
	}

	ao2_lock(participant->conference->bridge);
	pbx_bridge_remove(participant->conference->bridge, participant->conferenceBridgePeer);
	ao2_unlock(participant->conference->bridge);

	/* \todo find a better methode to wait for the thread to signal removed participant (pthread_cond_timedwait ?); */

/*
	pbx_mutex_lock(&participant->cond_lock);
	while (participant->removed==FALSE) {
		pbx_cond_wait(&participant->removed_cond_signal, &participant->cond_lock);
	}
	pbx_mutex_unlock(&participant->cond_lock);
*/

	while (participant->joinThread != AST_PTHREADT_NULL) {
		usleep(100);
	}

	if (participant->channel) {
		sccp_dev_displayprinotify(sccp_channel_getDevice(conference->moderator->channel), "Participant has been kicked out", 5, 2);
		sccp_dev_displayprompt(sccp_channel_getDevice(conference->moderator->channel), 0, conference->moderator->channel->callid, "Participant has been kicked out", 2);
	}
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Participant %d Kicked from Conference %d", DEV_ID_LOG(sccp_channel_getDevice(conference->moderator->channel)), participant->id, conference->id);
}

/*!
 * \brief Toggle Mute Conference Participant
 *
 * \param conference SCCP Conference
 * \param participant SCCP Conference Participant
 */
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant)
{
//      char *audioFiles[]={"conf-unmuted" , "conf-muted"};
	char *textMessage[] = { "unmuted", "muted" };

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Handle Mute for for ConferenceParticipant %d\n", DEV_ID_LOG(sccp_channel_getDevice(participant->channel)), participant->id);

	if (participant->muted == 1)
		participant->muted = 0;
	else
		participant->muted = 1;

	if (participant->channel) {
		sccp_dev_displayprinotify(sccp_channel_getDevice(participant->channel), textMessage[participant->muted], 5, 5);
		sccp_dev_displayprompt(sccp_channel_getDevice(participant->channel), 0, participant->channel->callid, textMessage[participant->muted], 5);
	}
	participant->features.mute = participant->muted;
//      pbx_stream_and_wait(participant->channel, audioFiles[participant->muted], "");

	if (participant->channel) {
		char mesg[] = "Participant has been ";

		strcat(mesg, textMessage[participant->muted]);
		sccp_dev_displayprinotify(sccp_channel_getDevice(conference->moderator->channel), mesg, 5, 2);
		sccp_dev_displayprompt(sccp_channel_getDevice(conference->moderator->channel), 0, conference->moderator->channel->callid, mesg, 5);
	}
	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: ConferenceParticipant %d %s\n", DEV_ID_LOG(sccp_channel_getDevice(participant->channel)), participant->id, textMessage[participant->muted]);
	if (conference && sccp_channel_getDevice(conference->moderator->channel)->conferencelist_active) {
		sccp_conference_show_list(conference, conference->moderator->channel);
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
 * and will be added to the conference upon accept.
 *
 * \param conference SCCP Conference
 * \param channel SCCP Channel
 *
 * \todo how to handle multiple moderators
 */
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel)
{
	uint32_t appID = APPID_CONFERENCE;
	uint32_t callReference = 1;						// callreference should be asterisk_channel->unique identifier

	if (!conference)
		return;

	if (!channel)								// only send this list to sccp phones
		return;

	if (NULL != conference->moderator && conference->participants.size < 1 && conference->moderator->channel != channel)
		return;

	sccp_log((DEBUGCAT_CONFERENCE)) (VERBOSE_PREFIX_3 "%s: Sending InviteForm to Channel %d\n", sccp_channel_getDevice(channel)->id, channel->callid);

#        if ASTERISK_VERSION_NUMBER >= 10400
	unsigned int transactionID = pbx_random();
#        else
	unsigned int transactionID = random();
#        endif

	char xmlStr[2048] = "";
	char xmlTmp[512] = "";

	strcat(xmlStr, "<CiscoIPPhoneInput>\n");
	strcat(xmlStr, "    <Title>Invite to Conference</Title>\n");
	strcat(xmlStr, "    <Prompt>Enter the name/number to Dial</Prompt>\n");
	strcat(xmlStr, "    <InputItem>\n");
	strcat(xmlStr, "          <DisplayName>Name</DisplayName>\n");
	strcat(xmlStr, "          <QueryStringParam>Name</QueryStringParam>\n");
	strcat(xmlStr, "          <InputFlags>A</InputFlags>\n");
	strcat(xmlStr, "    </InputItem>\n");
	strcat(xmlStr, "    <InputItem>\n");
	strcat(xmlStr, "          <DisplayName>Number</DisplayName>\n");
	strcat(xmlStr, "          <QueryStringParam>Number</QueryStringParam>\n");
	strcat(xmlStr, "          <InputFlags>N</InputFlags>\n");
	strcat(xmlStr, "    </InputItem>\n");
	strcat(xmlStr, "<SoftKeyItem>\n");
	strcat(xmlStr, "  <Name>Submit</Name>\n");
	strcat(xmlStr, "  <Position>1</Position>\n");
	sprintf(xmlTmp, "  <URL>UserDataSoftKey:Submit:%d:INVITE1$%d$%d$%d$</URL>\n", 1, appID, conference->id, transactionID);
	strcat(xmlStr, xmlTmp);
	strcat(xmlStr, "</SoftKeyItem>\n");
	strcat(xmlStr, "<SoftKeyItem>\n");
	strcat(xmlStr, "  <Name>Cancel</Name>\n");
	strcat(xmlStr, "  <Position>3</Position>\n");
	strcat(xmlStr, "  <URL>SoftKey:Cancel</URL>\n");
	strcat(xmlStr, "</SoftKeyItem>\n");
	strcat(xmlStr, "</CiscoIPPhoneInput>\n");
	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "xml-message:\n%s\n", xmlStr);

	sendUserToDeviceVersion1Message(sccp_channel_getDevice(channel), appID, conference->id, callReference, transactionID, xmlStr);
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
 * \param conference SCCP Conference
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
			if (NULL == participant->channel) {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found participant (%d)\n", "non sccp", participant->id);
			} else {
				sccp_log((DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: Found participant (%d)\n", DEV_ID_LOG(sccp_channel_getDevice(participant->channel)), participant->id);
			}
			break;
		}
	}
	SCCP_LIST_UNLOCK(&conference->participants);
	return participant;
}

#    endif									// ASTERISK_VERSION_NUMBER
#endif										// CS_SCCP_CONFERENCE
