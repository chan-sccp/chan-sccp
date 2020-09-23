/*!
 * \file	sccp_featureCallCompletion.h
 * \brief	SCCP CallCompletion Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2020-Sept-22
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#pragma once

__BEGIN_C_EXTERN__
typedef struct {
	int (*registerCcAgentCallback)(void);
	void (*unregisterCcAgentCallback)(void);
	void (*const handleDevice2User)(constDevicePtr d, uint32_t appID, uint8_t instance, uint32_t callReference, uint32_t transactionId, const char *data);
	int (*pushForm)(constDevicePtr d, constLinePtr l);
} CallCompletionInterface;

extern const CallCompletionInterface iCallCompletion;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
