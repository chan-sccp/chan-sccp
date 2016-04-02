/*!
 * \file        sccp_conference.h
 * \brief       SCCP Conference Header
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

#include "sccp_cli.h"

#define sccp_conference_retain(_x)		sccp_refcount_retain_type(sccp_conference_t, _x)
#define sccp_conference_release(_x)		sccp_refcount_release_type(sccp_conference_t, _x)
#define sccp_conference_refreplace(_x, _y)	sccp_refcount_refreplace_type(sccp_conference_t, _x, _y)

__BEGIN_C_EXTERN__
/* prototype definitions */
SCCP_API void SCCP_CALL sccp_conference_module_start(void);
SCCP_API void SCCP_CALL sccp_conference_module_stop(void);
SCCP_API sccp_conference_t * SCCP_CALL sccp_conference_create(devicePtr device, channelPtr channel);
SCCP_API boolean_t SCCP_CALL sccp_conference_addParticipatingChannel(conferencePtr conference, constChannelPtr conferenceSCCPChannel, constChannelPtr originalSCCPChannel, PBX_CHANNEL_TYPE * pbxChannel);
SCCP_API void SCCP_CALL sccp_conference_resume(conferencePtr conference);
SCCP_API void SCCP_CALL sccp_conference_start(conferencePtr conference);
SCCP_API void SCCP_CALL sccp_conference_update(constConferencePtr conference);
SCCP_API void SCCP_CALL sccp_conference_end(sccp_conference_t * conference);							/* explicit release */
SCCP_API void SCCP_CALL sccp_conference_hold(conferencePtr conference);

/* conf list related */
SCCP_API void SCCP_CALL sccp_conference_show_list(constConferencePtr conference, constChannelPtr channel);
SCCP_API void SCCP_CALL sccp_conference_hide_list_ByDevice(constDevicePtr device);
SCCP_API void SCCP_CALL sccp_conference_handle_device_to_user(devicePtr d, uint32_t callReference, uint32_t transactionID, uint32_t conferenceID, uint32_t participantID);

/* cli functions */
SCCP_API char * SCCP_CALL sccp_complete_conference(OLDCONST char *line, OLDCONST char *word, int pos, int state);
SCCP_API int SCCP_CALL sccp_cli_show_conferences(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
SCCP_API int SCCP_CALL sccp_cli_show_conference(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
SCCP_API int SCCP_CALL sccp_cli_conference_command(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
