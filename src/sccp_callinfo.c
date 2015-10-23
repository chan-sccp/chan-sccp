/*!
 * \file	sccp_callinfo.c
 * \brief	SCCP CallInfo Class
 * \brief	SCCP CallInfo Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */

/*!
 * \remarks
 * Purpose:	CCP CallInfo
 * When to use: 
 * Relations:   
 */

#include <config.h>
#include "common.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include <stdarg.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$");

/* local definitions */
typedef struct callinfo_entry {
	char Name[StationMaxNameSize];
	char Number[StationMaxDirnumSize];
	char VoiceMailbox[StationMaxDirnumSize];
	uint16_t NumberValid;
	uint16_t VoiceMailboxValid;
} callinfo_entry_t;

enum callinfo_groups {
	CALLED_PARTY,
	CALLING_PARTY,
	ORIG_CALLED_PARTY,
	ORIG_CALLING_PARTY,
	LAST_REDIRECTING_PARTY,
	HUNT_PILOT,
};

enum callinfo_types {
	NAME,
	NUMBER,
	VOICEMAILBOX,
};

/*!
 * \brief SCCP CallInfo Structure
 */
struct sccp_callinfo {
	sccp_mutex_t lock;
	callinfo_entry_t entries[HUNT_PILOT + 1];
	uint32_t originalCdpnRedirectReason;									/*!< Original Called Party Redirect Reason */
	uint32_t lastRedirectingReason;										/*!< Last Redirecting Reason */
	sccp_callerid_presentation_t presentation;								/*!< Should this callerinfo be shown (privacy) */
	boolean_t changed;											/*! Changes since last send */
	uint8_t callInstance;
};														/*!< SCCP CallInfo Structure */

#define sccp_callinfo_lock(x) sccp_mutex_lock(&((sccp_callinfo_t * const)x)->lock)				/* discard const */
#define sccp_callinfo_unlock(x) sccp_mutex_unlock(&((sccp_callinfo_t * const)x)->lock)				/* discard const */

struct callinfo_lookup {
	const enum callinfo_groups group;
	const enum callinfo_types type;
} static const callinfo_lookup[] = {
	/* *INDENT-OFF* */
	[SCCP_CALLINFO_CALLEDPARTY_NAME]		= {CALLED_PARTY, NAME},
	[SCCP_CALLINFO_CALLEDPARTY_NUMBER]		= {CALLED_PARTY, NUMBER},
	[SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL]		= {CALLED_PARTY, VOICEMAILBOX},
	[SCCP_CALLINFO_CALLINGPARTY_NAME]		= {CALLING_PARTY, NAME},
	[SCCP_CALLINFO_CALLINGPARTY_NUMBER]		= {CALLING_PARTY, NUMBER},
	[SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL]		= {CALLING_PARTY, VOICEMAILBOX},
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME]		= {ORIG_CALLED_PARTY, NAME},
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER]		= {ORIG_CALLED_PARTY, NUMBER},
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL]	= {ORIG_CALLED_PARTY, VOICEMAILBOX},
	[SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME]		= {ORIG_CALLING_PARTY, NAME},
	[SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER]	= {ORIG_CALLING_PARTY, NUMBER},
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME]	= {LAST_REDIRECTING_PARTY, NAME},
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER]	= {LAST_REDIRECTING_PARTY, NUMBER},
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL]	= {LAST_REDIRECTING_PARTY, VOICEMAILBOX},
	[SCCP_CALLINFO_HUNT_PILOT_NAME]			= {HUNT_PILOT, NAME},
	[SCCP_CALLINFO_HUNT_PILOT_NUMBER]		= {HUNT_PILOT, NUMBER},
	/* *INDENT-ON* */
};

sccp_callinfo_t *const sccp_callinfo_ctor(uint8_t callInstance)
{
	sccp_callinfo_t *const ci = sccp_calloc(sizeof(sccp_callinfo_t), 1);

	if (!ci) {
		pbx_log(LOG_ERROR, "SCCP: No memory to allocate callinfo object. Failing\n");
		return NULL;
	}
	sccp_mutex_init(&ci->lock);

	/* by default we allow callerid presentation */
	ci->presentation = CALLERID_PRESENTATION_ALLOWED;
	ci->changed = TRUE;
	ci->callInstance = callInstance;

	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		#ifdef DEBUG
		//sccp_do_backtrace();
		#endif
	}
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_1 "SCCP: callinfo constructor: %p\n", ci);
	return ci;
}

sccp_callinfo_t *const sccp_callinfo_dtor(sccp_callinfo_t * ci)
{
	pbx_assert(ci != NULL);
	sccp_callinfo_lock(ci);
	sccp_mutex_destroy(&ci->lock);
	sccp_callinfo_unlock(ci);
	sccp_free(ci);
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: callinfo destructor\n");
	return NULL;
}

sccp_callinfo_t *sccp_callinfo_copyCtor(const sccp_callinfo_t * const src_ci)
{
	/* observing locking order. not locking both callinfo objects at the same time, using a tmp as go between */
	if (src_ci) {
		sccp_callinfo_t *tmp_ci = sccp_callinfo_ctor(0);
		if (!tmp_ci) {
			return NULL;
		}
		sccp_callinfo_lock(src_ci);
		memcpy(tmp_ci, src_ci, sizeof(sccp_callinfo_t));
		sccp_callinfo_unlock(src_ci);

		return tmp_ci;
	}
	return NULL;
}

boolean_t sccp_callinfo_copy(const sccp_callinfo_t * const src_ci, sccp_callinfo_t * const dst_ci)
{
	/* observing locking order. not locking both callinfo objects at the same time, using a tmp as go between */
	if (src_ci && dst_ci) {
		//sccp_callinfo_t tmp_ci = {{{{0}}}};
		sccp_callinfo_t tmp_ci;
		memset(&tmp_ci, 0, sizeof(sccp_callinfo_t));

		sccp_callinfo_lock(src_ci);
		memcpy(&tmp_ci, src_ci, sizeof(sccp_callinfo_t));
		sccp_callinfo_unlock(src_ci);

		sccp_callinfo_lock(dst_ci);
		memcpy(dst_ci, &tmp_ci, sizeof(sccp_callinfo_t));
		dst_ci->changed = TRUE;
		sccp_callinfo_unlock(dst_ci);

		return TRUE;
	}
	return FALSE;
}

int sccp_callinfo_setter(sccp_callinfo_t * const ci, sccp_callinfo_key_t key, ...)
{
	pbx_assert(ci != NULL);

	sccp_callinfo_key_t curkey = SCCP_CALLINFO_NONE;
	int changes = 0;

	sccp_callinfo_lock(ci);
	va_list ap;
	va_start(ap, key);

	/*
	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		#ifdef DEBUG
		sccp_do_backtrace();
		#endif
		sccp_callinfo_print2log(ci, "SCCP: (sccp_callinfo_setter) before:");
	}
	*/
	for (curkey = key; curkey > SCCP_CALLINFO_NONE && curkey < SCCP_CALLINFO_KEY_SENTINEL; curkey = va_arg(ap, sccp_callinfo_key_t)) {
		switch (curkey) {
			case SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON:
				{
					uint new_value = va_arg(ap, uint);
					if (new_value != ci->originalCdpnRedirectReason) {
						ci->originalCdpnRedirectReason = new_value;
						changes++;
					}
				}
				break;
			case SCCP_CALLINFO_LAST_REDIRECT_REASON:
				{
					uint new_value = va_arg(ap, uint);
					if (new_value != ci->lastRedirectingReason) {
						ci->lastRedirectingReason = new_value;
						changes++;
					}
				}
				break;
			case SCCP_CALLINFO_PRESENTATION:
				{
					sccp_callerid_presentation_t new_value = va_arg(ap, sccp_callerid_presentation_t);
					if (new_value != ci->presentation) {
						ci->presentation = new_value;
						changes++;
					}
				}
				break;
			default:
				{
					char *new_value = va_arg(ap, char *);
					if (new_value) {
						size_t size = 0;
						char *dstPtr = NULL;
						uint16_t *validPtr = NULL;
						struct callinfo_lookup entry = callinfo_lookup[curkey];
						callinfo_entry_t *callinfo = &ci->entries[entry.group];

						switch(entry.type) {
							case NAME:
								size = StationMaxNameSize;
								dstPtr = callinfo->Name;
								validPtr = NULL;
								break;
							case NUMBER:
								size = StationMaxDirnumSize;
								dstPtr = callinfo->Number;
								validPtr = &callinfo->NumberValid;
								break;
							case VOICEMAILBOX:
								size = StationMaxDirnumSize;
								dstPtr = callinfo->VoiceMailbox;
								validPtr = &callinfo->VoiceMailboxValid;
								break;
						}
						if (!sccp_strequals(dstPtr, new_value)) {
							sccp_copy_string(dstPtr, new_value, size);
							changes++;
							if (validPtr) {
								*validPtr = sccp_strlen_zero(new_value) ? 0 : 1;
							}
						}
					}
				}
				break;
		}
	}

	va_end(ap);
	if (changes) {
		ci->changed = TRUE;
	}
	sccp_callinfo_unlock(ci);

	/*
	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		sccp_callinfo_print2log(ci, "SCCP: (sccp_callinfo_setter) after:");
	}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "%p: (sccp_callinfo_setter) changes:%d\n", ci, changes);
	*/
	return changes;
}

int sccp_callinfo_copyByKey(const sccp_callinfo_t * const src_ci, sccp_callinfo_t * const dst_ci, sccp_callinfo_key_t key, ...)
{
	pbx_assert(src_ci != NULL && dst_ci != NULL);
	//sccp_callinfo_t tmp_ci = {{{{0}}}};
	sccp_callinfo_t tmp_ci;
	memset(&tmp_ci, 0, sizeof(sccp_callinfo_t));
	
	sccp_callinfo_key_t srckey = SCCP_CALLINFO_NONE;
	sccp_callinfo_key_t dstkey = SCCP_CALLINFO_NONE;
	int changes = 0;

	/* observing locking order. not locking both callinfo objects at the same time, using a tmp_ci as go between */
	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		sccp_callinfo_print2log(src_ci, "SCCP: (sccp_callinfo_copyByKey) orig src_ci");
		sccp_callinfo_print2log(dst_ci, "SCCP: (sccp_callinfo_copyByKey) orig dst_ci");
	}
	sccp_callinfo_lock(src_ci);
	va_list ap;
	va_start(ap, key);
	dstkey=va_arg(ap, sccp_callinfo_key_t);

	/* \todo function should also include copying the reasons and presentation */
	/*for (srckey = key; 	srckey > SCCP_CALLINFO_NONE && srckey <= SCCP_CALLINFO_KEY_SENTINEL &&
				dstkey > SCCP_CALLINFO_NONE && dstkey <= SCCP_CALLINFO_KEY_SENTINEL; 
				srckey = va_arg(ap, sccp_callinfo_key_t), 
				dstkey = va_arg(ap, sccp_callinfo_key_t)) {*/
	for (srckey = key; 	srckey > SCCP_CALLINFO_NONE && srckey <= SCCP_CALLINFO_HUNT_PILOT_NUMBER &&
				dstkey > SCCP_CALLINFO_NONE && dstkey <= SCCP_CALLINFO_HUNT_PILOT_NUMBER; 
				srckey = va_arg(ap, sccp_callinfo_key_t), 
				dstkey = va_arg(ap, sccp_callinfo_key_t)) {
		struct callinfo_lookup src_entry = callinfo_lookup[srckey];
		struct callinfo_lookup dst_entry = callinfo_lookup[dstkey];
		
		callinfo_entry_t *src_callinfo = (callinfo_entry_t *const) &src_ci->entries[src_entry.group];
		callinfo_entry_t *tmp_callinfo = &tmp_ci.entries[dst_entry.group];

		char *srcPtr = NULL;
		uint16_t *validPtr = NULL;
		switch(src_entry.type) {
			case NAME:
				srcPtr = src_callinfo->Name;
				validPtr = NULL;
				break;
			case NUMBER:
				srcPtr = src_callinfo->Number;
				validPtr = &src_callinfo->NumberValid;
				break;
			case VOICEMAILBOX:
				srcPtr = src_callinfo->VoiceMailbox;
				validPtr = &src_callinfo->VoiceMailboxValid;
				break;
		}
		char *tmpPtr = NULL;
		size_t size = 0;
		switch(dst_entry.type) {
			case NAME:
				size = StationMaxNameSize;
				tmpPtr = tmp_callinfo->Name;
				break;
			case NUMBER:
				size = StationMaxDirnumSize;
				tmpPtr = tmp_callinfo->Number;
				break;
			case VOICEMAILBOX:
				size = StationMaxDirnumSize;
				tmpPtr = tmp_callinfo->VoiceMailbox;
				break;
		}
		if (validPtr) {
			if (*validPtr) {
				sccp_copy_string(tmpPtr, srcPtr, size);
				changes++;
			} else {
				tmpPtr[0] = '\0';
			}
		} else {
			sccp_copy_string(tmpPtr, srcPtr, size);
		}
	}

	va_end(ap);
	sccp_callinfo_unlock(src_ci);
	
	sccp_callinfo_lock(dst_ci);
	memcpy(dst_ci, &tmp_ci, sizeof(sccp_callinfo_t));
	sccp_callinfo_unlock(dst_ci);

	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		sccp_callinfo_print2log(dst_ci, "SCCP: (sccp_callinfo_copyByKey) new dst_ci");
	}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "%p: (sccp_callinfo_copyByKey) changes:%d\n", dst_ci, changes);
	return changes;
}

int sccp_callinfo_getter(const sccp_callinfo_t * const ci, sccp_callinfo_key_t key, ...)
{
	pbx_assert(ci != NULL);

	sccp_callinfo_key_t curkey = SCCP_CALLINFO_NONE;
	int entries = 0;

	sccp_callinfo_lock(ci);
	va_list ap;
	va_start(ap, key);

	for (curkey = key; curkey > SCCP_CALLINFO_NONE && curkey < SCCP_CALLINFO_KEY_SENTINEL; curkey = va_arg(ap, sccp_callinfo_key_t)) {
		switch (curkey) {
			case SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON:
				{
					uint *dstPtr = va_arg(ap, uint *);
					if (*dstPtr != ci->originalCdpnRedirectReason) {
						*dstPtr = ci->originalCdpnRedirectReason;
						entries++;
					}
				}
				break;
			case SCCP_CALLINFO_LAST_REDIRECT_REASON:
				{
					uint *dstPtr = va_arg(ap, uint *);
					if (*dstPtr != ci->lastRedirectingReason) {
						*dstPtr = ci->lastRedirectingReason;
						entries++;
					}
				}
				break;
			case SCCP_CALLINFO_PRESENTATION:
				{
					sccp_callerid_presentation_t *dstPtr = va_arg(ap, sccp_callerid_presentation_t *);
					if (*dstPtr != ci->presentation) {
						*dstPtr = ci->presentation;
						entries++;
					}
				}
				break;
			default:
				{
					char *dstPtr = va_arg(ap, char *);
					if (dstPtr) {
						size_t size = 0;
						char *srcPtr = NULL;
						uint16_t *validPtr = NULL;
						struct callinfo_lookup entry = callinfo_lookup[curkey];
						callinfo_entry_t *callinfo = (callinfo_entry_t *const) &(ci->entries[entry.group]);

						switch(entry.type) {
							case NAME:
								size = StationMaxNameSize;
								srcPtr = callinfo->Name;
								validPtr = NULL;
								break;
							case NUMBER:
								size = StationMaxDirnumSize;
								srcPtr = callinfo->Number;
								validPtr = &callinfo->NumberValid;
								break;
							case VOICEMAILBOX:
								size = StationMaxDirnumSize;
								srcPtr = callinfo->VoiceMailbox;
								validPtr = &callinfo->VoiceMailboxValid;
								break;
						}
						if (validPtr) {
							if (!*validPtr) {
								if (dstPtr[0] != '\0') {
									dstPtr[0] = '\0';
									entries++;
								}
								break;
							}
						}
						if (!sccp_strequals(dstPtr, srcPtr)) {
							entries++;
							sccp_copy_string(dstPtr, srcPtr, size);
						}
					}
				}
				break;
		}
	}

	va_end(ap);
	sccp_callinfo_unlock(ci);

	/*
	if ((GLOB(debug) & (DEBUGCAT_NEWCODE)) != 0) {
		#ifdef DEBUG
		sccp_do_backtrace();
		#endif
		sccp_callinfo_print2log(ci, "SCCP: (sccp_callinfo_getter)");
	}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "%p: (sccp_callinfo_getter) entries:%d\n", ci, entries);
	*/
	return entries;
}

int sccp_callinfo_send(sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const sccp_device_t * const device, boolean_t force)
{
	if (ci->changed || force) {
		if (device->protocol && device->protocol->sendCallInfo) {
			device->protocol->sendCallInfo(ci, callid, calltype, lineInstance, ci->callInstance, device);
			sccp_callinfo_lock(ci);
			ci->changed = FALSE;
			sccp_callinfo_unlock(ci);
			return 1;
		}
	} else {
		sccp_log(DEBUGCAT_NEWCODE) ("%p: (sccp_callinfo_send) ci has not changed since last send. Skipped sending\n", ci);
	}
	
	return 0;
}


int sccp_callinfo_setCalledParty(sccp_callinfo_t * const ci, const char name[StationMaxNameSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize])
{
	pbx_assert(ci != NULL);
	return sccp_callinfo_setter(ci, SCCP_CALLINFO_CALLEDPARTY_NAME, name, SCCP_CALLINFO_CALLEDPARTY_NUMBER, number, SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, voicemail, SCCP_CALLINFO_KEY_SENTINEL);
}

int sccp_callinfo_setCallingParty(sccp_callinfo_t * const ci, const char name[StationMaxNameSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize])
{
	pbx_assert(ci != NULL);
	return sccp_callinfo_setter(ci, SCCP_CALLINFO_CALLINGPARTY_NAME, name, SCCP_CALLINFO_CALLINGPARTY_NUMBER, number, SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, voicemail, SCCP_CALLINFO_KEY_SENTINEL);
}

int sccp_callinfo_setOrigCalledParty(sccp_callinfo_t * const ci, const char name[StationMaxNameSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason)
{
	pbx_assert(ci != NULL);
	return sccp_callinfo_setter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, name, SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, number, SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, voicemail, SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, reason, SCCP_CALLINFO_KEY_SENTINEL);
}

int sccp_callinfo_setOrigCallingParty(sccp_callinfo_t * const ci, const char name[StationMaxNameSize], const char number[StationMaxDirnumSize])
{
	pbx_assert(ci != NULL);
	return sccp_callinfo_setter(ci, SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, name, SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, number, SCCP_CALLINFO_KEY_SENTINEL);
}

int sccp_callinfo_setLastRedirectingParty(sccp_callinfo_t * const ci, const char name[StationMaxNameSize], const char number[StationMaxDirnumSize], const char voicemail[StationMaxDirnumSize], const int reason)
{
	pbx_assert(ci != NULL);
	return sccp_callinfo_setter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, name, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, number, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, voicemail, SCCP_CALLINFO_LAST_REDIRECT_REASON, reason, SCCP_CALLINFO_KEY_SENTINEL);
}

boolean_t sccp_callinfo_getCallInfoStr(const sccp_callinfo_t * const ci, pbx_str_t ** const buf)
{
	pbx_assert(ci != NULL);
	sccp_callinfo_lock(ci);
	pbx_str_append(buf, 0, "%p: (getCallInfoStr):\n", ci);
	if (ci->entries[CALLED_PARTY].NumberValid || ci->entries[CALLED_PARTY].VoiceMailboxValid) {
		pbx_str_append(buf, 0, " - calledParty: %s <%s>%s%s%s\n", ci->entries[CALLED_PARTY].Name, ci->entries[CALLED_PARTY].Number, 
			(ci->entries[CALLED_PARTY].VoiceMailboxValid) ? " voicemail: " : "", ci->entries[CALLED_PARTY].VoiceMailbox, 
			(ci->entries[CALLED_PARTY].NumberValid) ? ", valid" : ", invalid");
	}
	if (ci->entries[CALLING_PARTY].NumberValid || ci->entries[CALLING_PARTY].VoiceMailboxValid) {
		pbx_str_append(buf, 0, " - callingParty: %s <%s>%s%s%s\n", ci->entries[CALLING_PARTY].Name, ci->entries[CALLING_PARTY].Number, 
			(ci->entries[CALLING_PARTY].VoiceMailboxValid) ? " voicemail: " : "", ci->entries[CALLING_PARTY].VoiceMailbox, 
			(ci->entries[CALLING_PARTY].NumberValid) ? ", valid" : ", invalid");
	}
	if (ci->entries[ORIG_CALLED_PARTY].NumberValid || ci->entries[ORIG_CALLED_PARTY].VoiceMailboxValid) {
		pbx_str_append(buf, 0, " - originalCalledParty: %s <%s>%s%s%s, reason: %d\n", ci->entries[ORIG_CALLED_PARTY].Name, ci->entries[ORIG_CALLED_PARTY].Number, 
			(ci->entries[ORIG_CALLED_PARTY].VoiceMailboxValid) ? " voicemail: " : "", ci->entries[ORIG_CALLED_PARTY].VoiceMailbox, 
			(ci->entries[ORIG_CALLED_PARTY].NumberValid) ? ", valid" : ", invalid",
			ci->originalCdpnRedirectReason);
	}
	if (ci->entries[ORIG_CALLING_PARTY].NumberValid) {
		pbx_str_append(buf, 0, " - originalCallingParty: %s <%s>, valid\n", ci->entries[ORIG_CALLING_PARTY].Name, ci->entries[ORIG_CALLING_PARTY].Number);
	}
	if (ci->entries[LAST_REDIRECTING_PARTY].NumberValid || ci->entries[LAST_REDIRECTING_PARTY].VoiceMailboxValid) {
		pbx_str_append(buf, 0, " - lastRedirectingParty: %s <%s>%s%s%s, reason: %d\n", ci->entries[LAST_REDIRECTING_PARTY].Name, ci->entries[LAST_REDIRECTING_PARTY].Number, 
			(ci->entries[LAST_REDIRECTING_PARTY].VoiceMailboxValid) ? " voicemail: " : "", ci->entries[LAST_REDIRECTING_PARTY].VoiceMailbox, 
			(ci->entries[LAST_REDIRECTING_PARTY].NumberValid) ? ", valid" : ", invalid",
			ci->lastRedirectingReason);
	}
	if (ci->entries[HUNT_PILOT].NumberValid) {
		pbx_str_append(buf, 0, " - huntPilot: %s <%s>, valid\n", ci->entries[HUNT_PILOT].Name, ci->entries[HUNT_PILOT].Number);
	}
	pbx_str_append(buf, 0, " - presentation: %s\n\n", sccp_callerid_presentation2str(ci->presentation));
	sccp_callinfo_unlock(ci);
	return TRUE;
}
void sccp_callinfo_print2log(const sccp_callinfo_t * const ci, const char *const header)
{
	pbx_assert(ci != NULL);
	pbx_str_t *buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

	sccp_callinfo_getCallInfoStr(ci, &buf);
	sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_1 "%s: %s\n", header, pbx_str_buffer(buf));
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
