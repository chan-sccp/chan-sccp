#ifndef __SCCP_SOCKET_H
#define __SCCP_SOCKET_H

void * sccp_socket_thread(void * ignore);
void sccp_session_close(sccp_session_t * s);
void sccp_session_sendmsg(sccp_device_t *device, sccp_message_t t);
int sccp_session_send(const sccp_device_t *device, sccp_moo_t * r) ;
void sccp_session_reject(sccp_session_t *session, char *message);
struct in_addr * sccp_session_getINaddr(sccp_device_t *device, int type);


#endif
