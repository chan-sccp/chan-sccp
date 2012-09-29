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
#include "common.h"

#ifdef CS_SCCP_CONFERENCE


#include "asterisk/bridging.h"
#include "asterisk/bridging_features.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")


sccp_conference_t *sccp_conference_create(sccp_channel_t * owner){
	return NULL;
	

}
void sccp_conference_addParticipant(sccp_conference_t * conference, sccp_channel_t * participant){}
void sccp_conference_removeParticipant(sccp_conference_t * conference, sccp_conference_participant_t * participant){}
void sccp_conference_retractParticipatingChannel(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_module_start(void){}
void sccp_conference_end(sccp_conference_t * conference){}
int sccp_conference_addAstChannelToConferenceBridge(sccp_conference_participant_t * participant, PBX_CHANNEL_TYPE * currentParticipantPeer){ return 0;}

void sccp_conference_readFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel){}
void sccp_conference_writeFrame(PBX_FRAME_TYPE * frame, sccp_channel_t * channel){}

/* conf list related */
void sccp_conference_show_list(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_handle_device_to_user(sccp_device_t * d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID){}
void sccp_conference_kick_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant){}
void sccp_conference_toggle_mute_participant(sccp_conference_t * conference, sccp_conference_participant_t * participant){}
void sccp_conference_promote_participant(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_demode_participant(sccp_conference_t * conference, sccp_channel_t * channel){}
void sccp_conference_invite_participant(sccp_conference_t * conference, sccp_channel_t * channel){}


#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets on;
 
