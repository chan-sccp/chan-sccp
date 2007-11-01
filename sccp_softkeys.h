#ifndef __SCCP_SOFTKEYS_H
#define __SCCP_SOFTKEYS_H

void sccp_sk_redial(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_newcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_hold(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_transfer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_cfwdall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_cfwdbusy(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_cfwdnoanswer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

void sccp_sk_dnd(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_backspace(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_endcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

void sccp_sk_answer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

void sccp_sk_dirtrfr(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

void sccp_sk_resume(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_park(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_trnsfvm(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_private(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
void sccp_sk_gpickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);

#endif
