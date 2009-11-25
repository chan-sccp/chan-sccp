#ifndef __SCCP_ACTIONS_H
#define __SCCP_ACTIONS_H
void sccp_init_device(sccp_device_t * d);
void sccp_handle_unknown_message(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_dialedphonebook_message(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_alarm(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_register(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_accessorystatus_message(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_unregister(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_line_number(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_speed_dial_stat_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_stimulus(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_speeddial(sccp_device_t * d, sccp_speed_t * k);
void sccp_handle_backspace(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_handle_dialtone_nolock(sccp_channel_t * c);
void sccp_handle_offhook(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_onhook(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_headset(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_capabilities_res(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_button_template_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_soft_key_template_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_soft_key_set_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_time_date_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_keypad_button(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_soft_key_event(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_open_receive_channel_ack(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_ConnectionStatistics(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_version(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_ServerResMessage(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_ConfigStatMessage(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_EnblocCallMessage(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_forward_stat_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_feature_stat_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_services_stat_req(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_feature_action(sccp_device_t *d, int instance, boolean_t toggleState);
void sccp_handle_updatecapabilities_message(sccp_session_t * s, sccp_moo_t * r);
void sccp_handle_startmediatransmission_ack(sccp_session_t * s, sccp_moo_t * r);
#endif
