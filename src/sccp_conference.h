/*!
 * \file 	sccp_conference.h
 * \brief 	SCCP Conference Header
 * \author
 * \date	$LastChangedDate$
 *
 */

#ifndef SCCP_CONFERENCE_H_
#define SCCP_CONFERENCE_H_


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "asterisk.h"
#include "chan_sccp.h"


typedef struct sccp_conference							sccp_conference_t;				/*!< SCCP Conference Structure */
typedef struct sccp_conference_participant				sccp_conference_participant_t;	/*!< SCCP Conference Participant Structure */


/* structures */


struct sccp_conference {
		ast_mutex_t 									lock;							/*!< mutex */
		int												id;								/*!< conference id*/
		sccp_conference_participant_t					*moderator;						/*!< how initializes the conference */
		SCCP_LIST_HEAD(, sccp_conference_participant_t) participants;					/*!< participants in conference */

		SCCP_LIST_ENTRY(sccp_conference_t) 				list;							/*!< Linked List Entry */
};



struct sccp_conference_participant {
		sccp_channel_t									*channel;						/*!< channel */

		SCCP_LIST_ENTRY(sccp_conference_participant_t) 	list;							/*!< Linked List Entry */
};




/* prototype definition */

sccp_conference_t *sccp_conference_create(sccp_channel_t *owner);
void sccp_conference_addParticipant(sccp_conference_t *conference, sccp_channel_t *participant);
void sccp_conference_removeParticipant(sccp_conference_t *conference, sccp_channel_t *participant);
void sccp_conference_module_start(void);




/* internal structure */

SCCP_LIST_HEAD(, sccp_conference_t) 					sccp_conferences;				/*!< our list of conferences */
static int lastConferenceID = 0;


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* SCCP_CONFERENCE_H_ */
