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
 * $Date$
 * $Revision$
 */
#pragma once
void sccp_handle_unknown_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_dialedphonebook_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_alarm(constSessionPtr s, devicePtr d, constMessagePtr msg_in)				/* __attribute__((nonnull(1,3))) */;
void sccp_handle_token_request(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,3))) */;
void sccp_handle_register(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,3))) */;
void sccp_handle_SPCPTokenReq(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,3))) */;
void sccp_handle_accessorystatus_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_unregister(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,3))) */;
void sccp_handle_line_number(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_speed_dial_stat_req(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_stimulus(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_AvailableLines(constSessionPtr s, devicePtr d, constMessagePtr none)			/* __attribute__((nonnull(1,2))) */;
void sccp_handle_backspace(constDevicePtr d, const uint8_t lineInstance, const uint32_t callid);	/* __attribute__((nonnull(1))) */;
void sccp_handle_dialtone(constDevicePtr d, constLinePtr l, constChannelPtr c)				/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_KeepAliveMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in) 		/* __attribute__((nonnull(1))) */;
void sccp_handle_offhook(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_onhook(constSessionPtr s, devicePtr d, constMessagePtr msg_in)				/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_headset(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_capabilities_res(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_button_template_req(constSessionPtr s, devicePtr d, constMessagePtr none)		/* __attribute__((nonnull(1,2))) */;
void sccp_handle_soft_key_template_req(constSessionPtr s, devicePtr d, constMessagePtr none)		/* __attribute__((nonnull(1,2))) */;
void sccp_handle_soft_key_set_req(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_time_date_req(constSessionPtr s, devicePtr d, constMessagePtr none)			/* __attribute__((nonnull(1,2))) */;
void sccp_handle_keypad_button(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_soft_key_event(constSessionPtr s, devicePtr d, constMessagePtr msg_in) 		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_port_response(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_open_receive_channel_ack(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_OpenMultiMediaReceiveAck(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_ConnectionStatistics(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_version(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_ServerResMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_ConfigStatMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_EnblocCallMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_forward_stat_req(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_feature_stat_req(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_services_stat_req(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_updatecapabilities_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_updatecapabilities_V2_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_updatecapabilities_V3_message(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_startmediatransmission_ack(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_device_to_user(constSessionPtr s, devicePtr d, constMessagePtr msg_in)			/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_device_to_user_response(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_XMLAlarmMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,3))) */;
void sccp_handle_LocationInfoMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)		/* __attribute__((nonnull(1,3))) */;
void sccp_handle_startmultimediatransmission_ack(constSessionPtr s, devicePtr d, constMessagePtr msg_in)/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_mediatransmissionfailure(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
void sccp_handle_miscellaneousCommandMessage(constSessionPtr s, devicePtr d, constMessagePtr msg_in)	/* __attribute__((nonnull(1,2,3))) */;
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
