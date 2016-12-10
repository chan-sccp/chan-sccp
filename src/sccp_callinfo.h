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

/*!
 * \remarks
 * Purpose:	SCCP CallInfo
 * When to use: To Allocate / Destruct / Get & Set Callinfo fields
 * Relations:   
 */

__BEGIN_C_EXTERN__

/* forward declaration */
struct sccp_callinfo;

/* Definition of the functions associated with this type. */
typedef struct tagCallInfo {
	sccp_callinfo_t * const (*Constructor)(uint8_t callInstance);
	sccp_callinfo_t * const (*Destructor)(sccp_callinfo_t * * const ci);
	sccp_callinfo_t * (*CopyConstructor)(const sccp_callinfo_t * const src_ci);
	
	#if UNUSEDCODE // 2015-11-01
	boolean_t (*Copy)(const sccp_callinfo_t * const src, sccp_callinfo_t * const dst);
	#endif
	/*
	 * \brief callinfo setter with variable number of arguments
	 * settting "" means to clear out a particular entry. provising a NULL pointer will skip updating the entry.
	 * iCallInfo.Callinfo_setter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, "test", SCCP_CALLINFO_LAST_REDIRECT_REASON, 4, SCCP_CALLINFO_KEY_SENTINEL);
	 * SENTINEL is required to stop processing
	 * \returns: number of changed fields
	 */
	int (*Setter)(sccp_callinfo_t * const ci, int key, ...);						// key is a va_arg of type sccp_callinfo_key_t
	int (*CopyByKey)(const sccp_callinfo_t * const src_ci, sccp_callinfo_t * const dst_ci, int key, ...);	// key is a va_arg of type sccp_callinfo_key_t
	/*
	 * \brief send callinfo to device
	 */
	int (*Send)(sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const sccp_device_t * const device, boolean_t force);

	/*
	 * \brief callinfo getter with variable number of arguments, destination parameter needs to be prodided by reference
	 * iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER:, &name, SCCP_CALLINFO_LAST_REDIRECT_REASON, &readon, SCCP_CALLINFO_KEY_SENTINEL);
	 * SENTINEL is required to stop processing
	 * \returns: number of fields
	 */
	int (*Getter)(const sccp_callinfo_t * const ci, int key, ...);						// key is a va_arg of type sccp_callinfo_key_t

	/* helpers */
	int (*SetCalledParty)(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
	int (*SetCallingParty)(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize]);
	int (*SetOrigCalledParty)(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);
	int (*SetOrigCallingParty)(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize]);
	int (*SetLastRedirectingParty)(sccp_callinfo_t * const ci, const char name[StationMaxDirnumSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason);

	/* debug */
	void (*Print2log)(const sccp_callinfo_t * const ci, const char *const header);
} CallInfoInterface;

extern const CallInfoInterface iCallInfo;

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
