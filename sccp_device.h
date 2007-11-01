void sccp_dev_build_buttontemplate(sccp_device_t *d, btnlist * btn);
sccp_moo_t * sccp_build_packet(sccp_message_t t, size_t pkt_len);
int  sccp_dev_send(sccp_device_t * d, sccp_moo_t * r);
int  sccp_session_send(sccp_session_t * s, sccp_moo_t * r);
void sccp_dev_sendmsg(sccp_device_t * d, sccp_message_t t);
void sccp_session_sendmsg(sccp_session_t * s, sccp_message_t t);

void sccp_dev_set_keyset(sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt);
void sccp_dev_set_ringer(sccp_device_t * d, uint8_t opt, uint32_t line, uint32_t callid);
void sccp_dev_cleardisplay(sccp_device_t * d);
void sccp_dev_display(sccp_device_t * d, char * msg);
void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt);

void sccp_dev_set_speaker(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_mwi(sccp_device_t * d, sccp_line_t * l, uint8_t hasMail);
void sccp_dev_set_cplane(sccp_line_t * l, int status);
void sccp_dev_set_deactivate_cplane(sccp_channel_t * c);
void sccp_dev_starttone(sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout);
void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_clearprompt(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char * msg, int timeout);
void sccp_dev_cleardisplaynotify(sccp_device_t * d);
void sccp_dev_displaynotify(sccp_device_t * d, char * msg, uint32_t timeout);
void sccp_dev_cleardisplayprinotify(sccp_device_t * d);
void sccp_dev_displayprinotify(sccp_device_t * d, char * msg, uint32_t priority, uint32_t timeout);

sccp_speed_t * sccp_dev_speed_find_byindex(sccp_device_t * d, uint8_t instance, uint8_t type);
sccp_line_t * sccp_dev_get_activeline(sccp_device_t * d);
void sccp_dev_set_activeline(sccp_line_t * l);
void sccp_dev_check_mwi(sccp_device_t * d);
void sccp_dev_check_displayprompt(sccp_device_t * d);
void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * l);
void sccp_dev_set_lamp(sccp_device_t * d, uint16_t stimulus, uint8_t instance, uint8_t lampMode);
void sccp_dev_forward_status(sccp_line_t * l);
int sccp_device_check_ringback(sccp_device_t * d);
void * sccp_dev_postregistration(void *data);
void sccp_dev_clean(sccp_device_t * d);
sccp_serviceURL_t * sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint8_t instance);

#define REQ(x,y) x = sccp_build_packet(y, sizeof(x->msg.y))
#define REQCMD(x,y) x = sccp_build_packet(y, 0)

