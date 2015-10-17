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
#pragma once

#ifdef CS_SCCP_CONFERENCE

#include "sccp_cli.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
/* *INDENT-OFF* */
#endif

/* prototype definitions */
void sccp_conference_module_start(void);
void sccp_conference_module_stop(void);
sccp_conference_t *sccp_conference_create(devicePtr device, channelPtr channel);
boolean_t sccp_conference_addParticipatingChannel(conferencePtr conference, constChannelPtr conferenceSCCPChannel, constChannelPtr originalSCCPChannel, PBX_CHANNEL_TYPE * pbxChannel);
void sccp_conference_resume(conferencePtr conference);
void sccp_conference_start(conferencePtr conference);
void sccp_conference_update(constConferencePtr conference);
void sccp_conference_end(sccp_conference_t * conference);							/* explicit release */
void sccp_conference_hold(conferencePtr conference);

/* conf list related */
void sccp_conference_show_list(constConferencePtr conference, constChannelPtr channel);
void sccp_conference_hide_list_ByDevice(constDevicePtr device);
void sccp_conference_handle_device_to_user(devicePtr d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID);

/* cli functions */
char *sccp_complete_conference(OLDCONST char *line, OLDCONST char *word, int pos, int state);
int sccp_cli_show_conferences(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
int sccp_cli_show_conference(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
int sccp_cli_conference_command(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);

#if defined(__cplusplus) || defined(c_plusplus)
/* *INDENT-ON* */
}
#endif
#endif														/* CS_SCCP_CONFERENCE */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
