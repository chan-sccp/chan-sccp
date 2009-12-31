/*!
 * \file 	sccp_conference.c
 * \brief 	SCCP Conference Class
 * \author
 * \date	$LastChangedDate$
 *
 */
#include "config.h"
#include "sccp_conference.h"



/*!
 * Create conference
 *
 * @param channel conference owner
 * @return conference
 */
sccp_conference_t *sccp_conference_create(sccp_channel_t *owner){
	sccp_conference_t *conference = NULL;
	sccp_conference_participant_t *moderator = NULL;

	if(!owner)
		return NULL;

	/* create conference */
	conference = (sccp_conference_t *)ast_malloc(sizeof(sccp_conference_t));
	if(!conference)
		return NULL;

	memset(conference, 0, sizeof(sccp_conference_t));
	conference->id = ++lastConferenceID;

	/* create moderator */
	moderator = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!moderator){
			ast_free(conference);
			return NULL;
	}
	moderator->channel = owner;
	conference->moderator = moderator;

	SCCP_LIST_LOCK(&sccp_conferences);
	SCCP_LIST_INSERT_HEAD(&sccp_conferences, conference, list);
	SCCP_LIST_UNLOCK(&sccp_conferences);

	return conference;
}

/*!
 * Add participant to conference
 *
 * @param conference conference
 * @param participant participant to remove
 */
void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *participant){
	sccp_conference_participant_t *part = NULL;

	if(!participant || !conference)
		return;

	part = (sccp_conference_participant_t*)ast_malloc(sizeof(sccp_conference_participant_t));
	if(!part)
		return;

	part->channel = participant;

	SCCP_LIST_INSERT_TAIL(&conference->participants, part, list);
}


/*!
 * Remove participant from conference
 *
 * @param conference conference
 * @param participant participant to remove
 *
 * \todo implement this
 * \todo check if the are enough participants in conference
 */
void sccp_conference_removeParticipant(sccp_conference_t *conference, sccp_channel_t *participant){

}

/*!
 * doing initial actions
 */
void sccp_conference_module_start(){
	SCCP_LIST_HEAD_INIT(&sccp_conferences);
}

