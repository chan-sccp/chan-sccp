/*!
 * \file        sccp_callinfo.h
 * \brief       SCCP CallInfo Header
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$  
 */

#ifndef __SCCP_CALLINFO_H
#define __SCCP_CALLINFO_H

struct sccp_callinfo;
sccp_callinfo_t *const sccp_callinfo_ctor(void) 		__attribute__((constructor));
sccp_callinfo_t *const sccp_callinfo_dtor(sccp_callinfo_t *ci) 	__attribute__((destructor));
boolean_t sccp_callinfo_copy(const sccp_callinfo_t * const src, sccp_callinfo_t * const dst);

/*
 * \brief callinfo setter with variable number of arguments
 * sccp_callinfo_set(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, "test", SCCP_CALLINFO_LAST_REDIRECT_REASON, 4, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of changed fields
 */
int sccp_callinfo_setter(sccp_callinfo_t * ci, sccp_callinfo_key_t key, ...);

/*
 * \brief callinfo getter with variable number of arguments
 * sccp_callinfo_get(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, &name, SCCP_CALLINFO_LAST_REDIRECT_REASON, &readon, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of changed fields
 */
int sccp_callinfo_getter(sccp_callinfo_t * ci, sccp_callinfo_key_t key, ...);
void sccp_callinfo_getStringArray(sccp_callinfo_t * const ci, char strArray[16][StationMaxDirnumSize]);

/* helpers */
int sccp_callinfo_setCalledParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setCallingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setOrigCalledParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);
int sccp_callinfo_setOrigCallingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize]);
int sccp_callinfo_setLastRedirectingParty(sccp_callinfo_t * ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);

/*!
 * \brief return all callinfo strings concatenated into one newly allocated string, null seperated
 * used by sccp_protocol.c sendCallInfo 
 * resulting buffer needs to be freed
 */
int sccp_callinfo_getLongString(sccp_callinfo_t * const ci, char *buf, int *buflen);		

/* debug */
boolean_t sccp_callinfo_getCallInfoStr(sccp_callinfo_t *ci, pbx_str_t ** const buf);
void sccp_callinfo_print2log(sccp_callinfo_t *ci);

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
