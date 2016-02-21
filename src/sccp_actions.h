/*!
 * \file        sccp_actions.h
 * \brief       SCCP Actions Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 */
#pragma once
__BEGIN_C_EXTERN__

SCCP_API int SCCP_CALL sccp_handle_message(constMessagePtr msg, constSessionPtr s);

/* externally used handlers */
SCCP_API void SCCP_CALL sccp_handle_backspace(constDevicePtr d, const uint8_t lineInstance, const uint32_t callid)	__NONNULL(1);
SCCP_API void SCCP_CALL sccp_handle_dialtone(constDevicePtr d, constLinePtr l, constChannelPtr channel)			__NONNULL(1,2,3);
SCCP_API void SCCP_CALL sccp_handle_AvailableLines(constSessionPtr s, devicePtr d, constMessagePtr none)		__NONNULL(1,2);
SCCP_API void SCCP_CALL sccp_handle_soft_key_template_req(constSessionPtr s, devicePtr d, constMessagePtr none)		__NONNULL(1,2);
SCCP_API void SCCP_CALL sccp_handle_time_date_req(constSessionPtr s, devicePtr d, constMessagePtr none)			__NONNULL(1,2);
SCCP_API void SCCP_CALL sccp_handle_button_template_req(constSessionPtr s, devicePtr d, constMessagePtr none)		__NONNULL(1,2);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
