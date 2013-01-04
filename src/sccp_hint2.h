/*!
 * \file 	sccp_hint.h
 * \brief 	SCCP Hint Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \since 	2009-01-16
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */

#ifndef SCCP_HINT_H_
#    define SCCP_HINT_H_

#ifdef CS_EXPERIMENTAL
typedef enum { ASTERISK = 0, INTERNAL = 1 } sccp_hinttype_t;

typedef struct sccp_hint_SubscribingDevice sccp_hint_SubscribingDevice_t;
typedef struct sccp_hint_list sccp_hint_list_t;

/*!
 * \brief Hint State for Device
 * \param context Context as char
 * \param exten Extension as char
 * \param state State as Asterisk Extension State
 * \param data Asterisk Data
 * \return Status as int
 */
#    if ASTERISK_VERSION_NUMBER >= 11000
int sccp_hint_state(const char *context, const char *exten, enum ast_extension_states state, void *data);
#    else
int sccp_hint_state(char *context, char *exten, enum ast_extension_states state, void *data);
#    endif

sccp_channelState_t sccp_hint_getLinestate(const char *linename, const char *deviceId);

// void sccp_hint_lineStatusChanged(sccp_line_t * line, sccp_device_t * device, sccp_channelState_t state);
void sccp_hint_module_start(void);
void sccp_hint_module_stop(void);

#endif										/* CS_EXPERIMENTAL */
#endif										/* SCCP_HINT_H_ */
