
/*!
 * \file 	sccp_actions.h
 * \brief 	SCCP Actions Header
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

#ifndef __SCCP_ACTIONS_H
#    define __SCCP_ACTIONS_H
void sccp_init_device(sccp_device_t * d);
void sccp_handle_unknown_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_dialedphonebook_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_alarm(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_token_request(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_register(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_SPCPTokenReq(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_accessorystatus_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_unregister(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_line_number(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_stimulus(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_AvailableLines(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_speeddial(sccp_device_t * d, sccp_speed_t * k);
void sccp_handle_backspace(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_handle_dialtone_locked(sccp_channel_t * c);
void sccp_handle_KeepAliveMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_offhook(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_onhook(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_headset(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_button_template_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_time_date_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_keypad_button(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_OpenMultiMediaReceiveAck(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_version(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_services_stat_req(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_feature_action(sccp_device_t * d, int instance, boolean_t toggleState);
void sccp_handle_updatecapabilities_message(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_startmediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_device_to_user(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_device_to_user_response(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_XMLAlarmMessage(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_startmultimediatransmission_ack(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
void sccp_handle_mediatransmissionfailure(sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
#endif
