/*!
 * \file	sccp_callinfo.h
 * \brief	SCCP CallInfo Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$  
 */
#pragma once

struct sccp_callinfo;
sccp_callinfo_t __attribute__ ((malloc)) *const sccp_callinfo_ctor(uint8_t callInstance);
sccp_callinfo_t *const sccp_callinfo_dtor(sccp_callinfo_t * ci);
sccp_callinfo_t *sccp_callinfo_copyCtor(const sccp_callinfo_t * const src_ci);
boolean_t sccp_callinfo_copy(const sccp_callinfo_t * const src, sccp_callinfo_t * const dst);

/*
 * \brief callinfo setter with variable number of arguments
 * settting "" means to clear out a particular entry. provising a NULL pointer will skip updating the entry.
 * sccp_callinfo_setter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, "test", SCCP_CALLINFO_LAST_REDIRECT_REASON, 4, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of changed fields
 */
int sccp_callinfo_setter(sccp_callinfo_t * const ci, sccp_callinfo_key_t key, ...);
int sccp_callinfo_copyByKey(const sccp_callinfo_t * const src_ci, sccp_callinfo_t * const dst_ci, sccp_callinfo_key_t key, ...);

/*
 * \brief send callinfo to device
 */
int sccp_callinfo_send(sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const sccp_device_t * const device, boolean_t force);

/*
 * \brief callinfo getter with variable number of arguments, destination parameter needs to be prodided by reference
 * sccp_callinfo_getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, &name, SCCP_CALLINFO_LAST_REDIRECT_REASON, &readon, SCCP_CALLINFO_KEY_SENTINEL);
 * SENTINEL is required to stop processing
 * \returns: number of fields
 */
int sccp_callinfo_getter(const sccp_callinfo_t * const ci, sccp_callinfo_key_t key, ...);


/* helpers */
int sccp_callinfo_setCalledParty(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setCallingParty(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
int sccp_callinfo_setOrigCalledParty(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);
int sccp_callinfo_setOrigCallingParty(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize]);
int sccp_callinfo_setLastRedirectingParty(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);

/* debug */
boolean_t sccp_callinfo_getCallInfoStr(const sccp_callinfo_t * const ci, pbx_str_t ** const buf);
void sccp_callinfo_print2log(const sccp_callinfo_t * const ci, const char *const header);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
