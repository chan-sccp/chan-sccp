#ifndef __SCCP_LINE_H
#define __SCCP_LINE_H


sccp_line_t * sccp_line_create(void);
sccp_line_t *sccp_line_applayDefaults(sccp_line_t *l);
void sccp_line_kill(sccp_line_t * l);
void sccp_line_delete_nolock(sccp_line_t * l);
void sccp_line_cfwd(sccp_line_t * l, uint8_t type, char * number);
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t *device);
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t *device);
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t *channel);
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t *channel);

#endif
