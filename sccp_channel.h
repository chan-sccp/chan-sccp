sccp_channel_t * sccp_channel_allocate(sccp_line_t * l);
sccp_channel_t * sccp_channel_get_active(sccp_device_t * d);
void sccp_channel_set_active(sccp_channel_t * c);
void sccp_channel_send_callinfo(sccp_channel_t * c);
void sccp_channel_send_dialednumber(sccp_channel_t * c);
void sccp_channel_set_callstate(sccp_channel_t * c, uint8_t state);
void sccp_channel_set_callstate_full(sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state);
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_connect(sccp_channel_t * c);
void sccp_channel_disconnect(sccp_channel_t * c);
void sccp_channel_openreceivechannel(sccp_channel_t * c);
void sccp_channel_startmediatransmission(sccp_channel_t * c);
void sccp_channel_closereceivechannel(sccp_channel_t * c);
void sccp_channel_stopmediatransmission(sccp_channel_t * c);
void sccp_channel_endcall(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
sccp_channel_t * sccp_channel_newcall(sccp_line_t * l, char * dial);
void sccp_channel_answer(sccp_channel_t * c);
void sccp_channel_delete(sccp_channel_t * c);
void sccp_channel_delete_no_lock(sccp_channel_t * c);
int sccp_channel_hold(sccp_channel_t * c);
int sccp_channel_resume(sccp_channel_t * c);
void sccp_channel_start_rtp(sccp_channel_t * c);
void sccp_channel_stop_rtp(sccp_channel_t * c);
void sccp_channel_transfer(sccp_channel_t * c);
void sccp_channel_transfer_complete(sccp_channel_t * c);
#ifdef CS_SCCP_PARK
void sccp_channel_park(sccp_channel_t * c);
#endif

