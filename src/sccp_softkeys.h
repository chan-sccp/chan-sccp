/*!
 * \file 	sccp_softkeys.h
 * \brief 	SCCP SoftKeys Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */
#ifndef __SCCP_SOFTKEYS_H
#    define __SCCP_SOFTKEYS_H

#    ifdef CS_DYNAMIC_CONFIG
void sccp_softkey_pre_reload(void);
void sccp_softkey_post_reload(void);
#    endif

void sccp_sk_redial(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_newcall(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_hold(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_transfer(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_conference(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_cfwdall(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_cfwdbusy(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_cfwdnoanswer(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);

void sccp_sk_dnd(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_backspace(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_endcall(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);

void sccp_sk_answer(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);

void sccp_sk_conference(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_join(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_meetme(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_barge(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_cbarge(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);

void sccp_sk_dirtrfr(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_select(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);

void sccp_sk_resume(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_park(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_trnsfvm(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_private(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_pickup(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_gpickup(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c);
void sccp_sk_set_keystate(sccp_device_t * d, sccp_line_t * l, const uint32_t lineInstance, sccp_channel_t * c, unsigned int keymode, unsigned int softkeyindex, unsigned int status);

#endif
