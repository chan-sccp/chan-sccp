/*!
 * \file        sccp_conference.h
 * \brief       SCCP Conference Header
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

#ifndef SCCP_CONFERENCE_H_
#define SCCP_CONFERENCE_H_

#ifdef CS_SCCP_CONFERENCE

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
/* *INDENT-OFF* */
#endif

typedef struct sccp_conference_participant sccp_conference_participant_t;					/*!< SCCP Conference Participant Structure */

/* structures */
struct sccp_conference {
	ast_mutex_t lock;											/*!< mutex */
	uint32_t id;												/*!< conference id */
	struct ast_bridge *bridge;										/*!< Shared Ast_Bridge used by this conference */
	int num_moderators;											/*!< Number of moderators for this conference */
	boolean_t finishing;											/*!< Indicates the conference is closing down */
	boolean_t isLocked;											/*!< Indicates that no new participants are allowed */
	boolean_t isOnHold;
	PBX_CHANNEL_TYPE *playback_channel;									/*!< Channel to playback sound file on */
	ast_mutex_t playback_lock;										/*!< Mutex Lock for playing back sound files */
	char playback_language[SCCP_MAX_LANGUAGE];								/*!< Language to be used during playback */
	boolean_t mute_on_entry;										/*!< Mute new participant when they enter the conference */
	boolean_t playback_announcements;									/*!< general hear announcements */
	const char *linkedid;											/*!< Conference LinkedId */

	SCCP_LIST_HEAD (, sccp_conference_participant_t) participants;						/*!< participants in conference */
	SCCP_LIST_ENTRY (sccp_conference_t) list;								/*!< Linked List Entry */
};

struct sccp_conference_participant {
	boolean_t pendingRemoval;										/*!< Pending Removal */
	uint32_t id;												/*!< Numeric participant id. */
	sccp_channel_t *channel;										/*!< sccp channel, non-null if the participant resides on an SCCP device */
	sccp_device_t *device;											/*!< sccp device, non-null if the participant resides on an SCCP device */
	PBX_CHANNEL_TYPE *conferenceBridgePeer;									/*!< the asterisk channel which joins the conference bridge */
	struct ast_bridge_channel *bridge_channel;								/*!< Asterisk Conference Bridge Channel */
	struct ast_bridge_features features;									/*!< Enabled features information */
	pthread_t joinThread;											/*!< Running in this Thread */
	sccp_conference_t *conference;										/*!< Conference this participant belongs to */
	boolean_t isModerator;
	boolean_t onMusicOnHold;										/*!< Participant is listening to Music on Hold */
	boolean_t playback_announcements;									/*!< Does the Participant want to hear announcements */

	/* conflist */
	uint32_t callReference;
	uint32_t lineInstance;
	uint32_t transactionID;

	SCCP_LIST_ENTRY (sccp_conference_participant_t) list;							/*!< Linked List Entry */
};

/* prototype definition */
void sccp_conference_module_start(void);
void sccp_conference_module_stop(void);
sccp_conference_t *sccp_conference_create(sccp_device_t * device, sccp_channel_t * channel);
void sccp_conference_addParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * conferenceSCCPChannel, sccp_channel_t * originalSCCPChannel, PBX_CHANNEL_TYPE * pbxChannel);
void sccp_conference_resume(sccp_conference_t * conference);
void sccp_conference_start(sccp_conference_t * conference);
void sccp_conference_update(sccp_conference_t * conference);
void sccp_conference_end(sccp_conference_t * conference);
void sccp_conference_hold(sccp_conference_t * conference);

/* conf list related */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel);
void sccp_conference_hide_list_ByDevice(sccp_device_t * device);
void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID);
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant);
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant);
void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel);
void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel);

/* cli functions */
char *sccp_complete_conference(OLDCONST char *line, OLDCONST char *word, int pos, int state);
int sccp_cli_show_conferences(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[]);
int sccp_cli_show_conference(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[]);
int sccp_cli_conference_command(int fd, int *total, struct mansession *s, const struct message *m, int argc, char *argv[]);

#if defined(__cplusplus) || defined(c_plusplus)
/* *INDENT-ON* */
}
#endif
#endif														/* CS_SCCP_CONFERENCE */
#endif														/* SCCP_CONFERENCE_H_ */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
