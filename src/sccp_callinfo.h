/*!
 * \file        sccp_callinfo.h
 * \brief       SCCP CallInfo Header
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$  
 */

#ifndef __SCCP_CALLINFO_H
#define __SCCP_CALLINFO_H

struct sccp_callinfo;
sccp_callinfo_t *const sccp_callinfo_ctor(void);
sccp_callinfo_t *const sccp_callinfo_dtor(sccp_callinfo_t *ci);
boolean_t sccp_callinfo_copy(const sccp_callinfo_t * const src, sccp_callinfo_t * const dst);

/*
 * \brief callinfo setter with variable number of arguments
 * sccp_callinfo_set(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, "test", SCCP_CALLINFO_LAST_REDIRECT_REASON, 4, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of changed fields
 */
//int sccp_callinfo_setStr(sccp_callinfo_t * ci, sccp_callinfo_key_t key, const char value[StationMaxDirnumSize]);
//int sccp_callinfo_setReason(sccp_callinfo_t * ci, sccp_callinfo_key_t key, const int reason);
//int sccp_callinfo_setPresentation(sccp_callinfo_t * ci, const sccp_calleridpresence_t presentation);
int sccp_callinfo_set(sccp_callinfo_t * ci, sccp_callinfo_key_t key, ...);

/*
 * \brief callinfo getter with variable number of arguments
 * sccp_callinfo_get(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, &name, SCCP_CALLINFO_LAST_REDIRECT_REASON, &readon, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of changed fields
 */
//boolean_t sccp_callinfo_getStr(sccp_callinfo_t * ci, sccp_callinfo_key_t key, char **const value);
//boolean_t sccp_callinfo_getReason(sccp_callinfo_t * ci, sccp_callinfo_key_t key, int *const reason);
//boolean_t sccp_callinfo_getPresentation(sccp_callinfo_t * ci, sccp_calleridpresence_t *const presentation);
int sccp_callinfo_get(sccp_callinfo_t * ci, sccp_callinfo_key_t key, ...);

/* helpers */
int sccp_callinfo_setCalledParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setCallingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setOrigCalledParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);
int sccp_callinfo_setOrigCallingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize]);
int sccp_callinfo_setLastRedirectingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);

//boolean_t sccp_callinfo_sendCallInfo(sccp_callinfo_t *ci, constDevicePtr d, constChannelPtr c);
boolean_t sccp_callinfo_getCallInfoStr(sccp_callinfo_t *ci, pbx_str_t ** const buf);
void sccp_callinfo_print2log(sccp_callinfo_t *ci);

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
