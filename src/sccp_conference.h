/*!
 * \file        sccp_conference.h
 * \brief       SCCP Conference Header
 * \author
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Revision$  
 */
#pragma once

#include "sccp_cli.h"

#ifdef DEBUG
#define sccp_conference_retain(_x) 	({sccp_conference_t const __attribute__((unused)) *tmp_##__LINE__##X = _x;pbx_assert(tmp_##__LINE__##X != NULL);(sccp_conference_t *)sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_conference_release(_x) 	({sccp_conference_t const __attribute__((unused)) *tmp_##__LINE__##X = _x;pbx_assert(tmp_##__LINE__##X != NULL);(sccp_conference_t *)sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#else
#define sccp_conference_retain(_x) 	({pbx_assert(_x != NULL);(sccp_conference_t *)sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_conference_release(_x) 	({pbx_assert(_x != NULL);(sccp_conference_t *)sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#endif

__BEGIN_EXTERN__
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
__END_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
