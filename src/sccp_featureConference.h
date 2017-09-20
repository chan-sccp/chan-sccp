/*!
 * \file	sccp_featureConference.h
 * \brief	SCCP Conference Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2017-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#pragma once

#ifdef CS_SCCP_CONFERENCE
#endif

__BEGIN_C_EXTERN__
typedef struct {
	int (*startConference) (void);
	//int (*handleConfSoftKey) (void);
	//int (*handleConfListSoftKey) (void);
	//int (*showConfList) (const char *conference);
	//int (*hideConfList) (const char *conference);
	//int (*handleButtonPress) (const char *conference, constDevicePtr d);
	//int (*handleDevice2User) (const char *conference);
	//inviteToConference
	//int (*moveIntoConference) (const char *conference);
	int (*endConference)(const char *conference);
} ConferenceInterface;

extern const ConferenceInterface iConference;
__END_C_EXTERN__

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
