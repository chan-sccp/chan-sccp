
/*!
 * \file 	sccp_conference.h
 * \brief 	SCCP Conference Header
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

#ifndef SCCP_CONFERENCE_H_
#    define SCCP_CONFERENCE_H_

#    ifdef CS_SCCP_CONFERENCE

#        if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#        endif

#        include "asterisk.h"
#        include "chan_sccp.h"

#        include "asterisk/bridging.h"
#        include "asterisk/bridging_features.h"

	typedef struct sccp_conference sccp_conference_t;			/*!< SCCP Conference Structure */
	typedef struct sccp_conference_participant sccp_conference_participant_t;	/*!< SCCP Conference Participant Structure */

/* structures */
	struct sccp_conference {
		ast_mutex_t lock;						/*!< mutex */
		uint32_t id;							/*!< conference id */
		sccp_conference_participant_t *moderator;			/*!< how initializes the conference */
		struct ast_bridge *bridge;					/*!< Shared Ast_Bridge used by this conference */
		SCCP_LIST_HEAD(, sccp_conference_participant_t) participants;	/*!< participants in conference */
		SCCP_LIST_ENTRY(sccp_conference_t) list;			/*!< Linked List Entry */
	};

	struct sccp_conference_participant {
		boolean_t pendingRemoval;				/*!< Numeric participant id. */
		uint32_t id;							/*!< Numeric participant id. */
		sccp_channel_t *channel;					/*!< sccp channel, non-null if the participant resides on an SCCP device */
		struct ast_channel *conferenceBridgePeer;			/*!< the asterisk channel which joins the conference bridge */
		struct ast_bridge_features features;				/*!< Enabled features information */
		pthread_t joinThread;						/*!< Running in this Thread */
		sccp_conference_t *conference;					/*!< Conference this participant belongs to */
		unsigned int muted;						/*!< Participant is Muted */

		char exitcontext[AST_MAX_CONTEXT];				/*!< Context to jump to after hangup */
		char exitexten[AST_MAX_CONTEXT];				/*!< Extension to jump to after hangup */
		int exitpriority;						/*!< Priority to jump to after hangup */

		SCCP_LIST_ENTRY(sccp_conference_participant_t) list;		/*!< Linked List Entry */
	};

/* prototype definition */

	sccp_conference_t *sccp_conference_create(sccp_channel_t * owner);
	void sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participant);
	void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant);
	void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel);
	void sccp_conference_module_start(void);
	void sccp_conference_end(sccp_conference_t * conference);
	int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t * participant, struct ast_channel *currentParticipantPeer);

	void sccp_conference_readFrame(struct ast_frame *frame, sccp_channel_t * channel);
	void sccp_conference_writeFrame(struct ast_frame *frame, sccp_channel_t * channel);

	/* conf list related */
	void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel);
	void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID);
	void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant);
	void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant);
	void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel);
	void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel);
	void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel);

/* internal structure */

	 SCCP_LIST_HEAD(, sccp_conference_t) conferences;			/*!< our list of conferences */

#        if defined(__cplusplus) || defined(c_plusplus)
}
#        endif
#    endif									/* CS_SCCP_CONFERENCE */
#endif										/* SCCP_CONFERENCE_H_ */
