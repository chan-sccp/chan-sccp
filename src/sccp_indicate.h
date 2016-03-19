/*!
 * \file        sccp_indicate.h
 * \brief       SCCP Indicate Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#define SCCP_INDICATE_NOLOCK 	0
#define SCCP_INDICATE_LOCK		1

SCCP_API void SCCP_CALL __sccp_indicate(const sccp_device_t * const device, sccp_channel_t * const c, const sccp_channelstate_t state, const uint8_t debug, const char *file, const int line, const char *pretty_function);

#define SCCP_GROUPED_CHANNELSTATE_IDLE		9
#define SCCP_GROUPED_CHANNELSTATE_DIALING	19
#define SCCP_GROUPED_CHANNELSTATE_SETUP		29
#define SCCP_GROUPED_CHANNELSTATE_CONNECTION  	39
#define SCCP_GROUPED_CHANNELSTATE_TERMINATION	49

#define SCCP_CHANNELSTATE_Idling(_x) (( _x) <= SCCP_GROUPED_CHANNELSTATE_IDLE)
#define SCCP_CHANNELSTATE_IsDialing(_x) (( _x) > SCCP_GROUPED_CHANNELSTATE_IDLE && ( _x) <= SCCP_GROUPED_CHANNELSTATE_DIALING)
#define SCCP_CHANNELSTATE_IsSettingUp(_x) (( _x) > SCCP_GROUPED_CHANNELSTATE_DIALING && ( _x) <= SCCP_GROUPED_CHANNELSTATE_SETUP)
#define SCCP_CHANNELSTATE_IsConnected(_x) (( _x) > SCCP_GROUPED_CHANNELSTATE_SETUP && ( _x) <= SCCP_GROUPED_CHANNELSTATE_CONNECTION)
#define SCCP_CHANNELSTATE_IsTerminating(_x) (( _x) > SCCP_GROUPED_CHANNELSTATE_CONNECTION && ( _x) <= SCCP_GROUPED_CHANNELSTATE_TERMINATION)

#ifdef CS_DEBUG_INDICATIONS
#define sccp_indicate(x, y, z)	__sccp_indicate(x, (channelPtr) y, z, 1, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
#define sccp_indicate(x, y, z)	__sccp_indicate(x, (channelPtr) y, z, 0, NULL, 0, NULL)
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
