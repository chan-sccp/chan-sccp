/*!
 * \file	sccp_socket.h
 * \brief       SCCP Socket Header
 * \author	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *		The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *		Modified by Jan Czmok and Julien Goodwin
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_SOCKET_H
#define __SCCP_SOCKET_H

boolean_t sccp_socket_is_IPv4(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_is_IPv6(const struct sockaddr_storage *sockAddrStorage);
uint16_t sccp_socket_getPort(const struct sockaddr_storage *sockAddrStorage);
void sccp_socket_setPort(const struct sockaddr_storage *sockAddrStorage, uint16_t port);
int sccp_socket_is_any_addr(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_getExternalAddr(struct sockaddr_storage *sockAddrStorage);
int sccp_socket_getOurAddressfor(const struct sockaddr_storage *them, struct sockaddr_storage *us);
size_t sccp_socket_sizeof(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_is_mapped_IPv4(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_ipv4_mapped(const struct sockaddr_storage *sockAddrStorage, struct sockaddr_storage *sockAddrStorage_mapped);
int sccp_socket_cmp_addr(const struct sockaddr_storage *a, const struct sockaddr_storage *b);
int sccp_socket_split_hostport(char *str, char **host, char **port, int flags);

/* easy replacement for inet_ntop for use in sccp_log functions (threadsafe) */
#define SCCP_SOCKADDR_STR_ADDR           (1 << 0)
#define SCCP_SOCKADDR_STR_PORT           (1 << 1)
#define SCCP_SOCKADDR_STR_BRACKETS       (1 << 2)
#define SCCP_SOCKADDR_STR_REMOTE         (1 << 3)
#define SCCP_SOCKADDR_STR_HOST           (SCCP_SOCKADDR_STR_ADDR | SCCP_SOCKADDR_STR_BRACKETS)
#define SCCP_SOCKADDR_STR_DEFAULT        (SCCP_SOCKADDR_STR_ADDR | SCCP_SOCKADDR_STR_PORT)
#define SCCP_SOCKADDR_STR_ADDR_REMOTE     (SCCP_SOCKADDR_STR_ADDR | SCCP_SOCKADDR_STR_REMOTE)
#define SCCP_SOCKADDR_STR_HOST_REMOTE     (SCCP_SOCKADDR_STR_HOST | SCCP_SOCKADDR_STR_REMOTE)
#define SCCP_SOCKADDR_STR_DEFAULT_REMOTE  (SCCP_SOCKADDR_STR_DEFAULT | SCCP_SOCKADDR_STR_REMOTE)
#define SCCP_SOCKADDR_STR_FORMAT_MASK     (SCCP_SOCKADDR_STR_ADDR | SCCP_SOCKADDR_STR_PORT | SCCP_SOCKADDR_STR_BRACKETS)
char *sccp_socket_stringify_fmt(const struct sockaddr_storage *sockAddrStorage, int format);

/* begin sccp_socket_stringify_fmt short cuts */
static inline char *sccp_socket_stringify(const struct sockaddr_storage *sockAddrStorage)
{  
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_DEFAULT);
}
static inline char *sccp_socket_stringify_remote(const struct sockaddr_storage *sockAddrStorage)
{
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_DEFAULT_REMOTE);
}
static inline char *sccp_socket_stringify_addr(const struct sockaddr_storage *sockAddrStorage)
{
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_ADDR);
}
static inline char *sccp_socket_stringify_addr_remote(const struct sockaddr_storage *sockAddrStorage)
{
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_ADDR_REMOTE);
}
static inline char *sccp_socket_stringify_host(const struct sockaddr_storage *sockAddrStorage)
{
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_HOST);
}
static inline char *sccp_socket_stringify_host_remote(const struct sockaddr_storage *sockAddrStorage)
{ 
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_HOST_REMOTE);
}
static inline char *sccp_socket_stringify_port(const struct sockaddr_storage *sockAddrStorage)
{
        return sccp_socket_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_PORT);
}
/* end sccp_socket_stringify_fmt short cuts */


void *sccp_socket_thread(void *ignore);
void sccp_session_sendmsg(const sccp_device_t * device, sccp_mid_t t);
int sccp_session_send(const sccp_device_t * device, sccp_msg_t * msg);
int sccp_session_send2(sccp_session_t * s, sccp_msg_t * msg);
sccp_device_t *sccp_session_addDevice(sccp_session_t * session, sccp_device_t * device);
sccp_device_t *sccp_session_removeDevice(sccp_session_t * session);
sccp_session_t *sccp_session_reject(sccp_session_t * session, char *message);
sccp_session_t *sccp_session_crossdevice_cleanup(sccp_session_t * session, sccp_device_t * device, char *message);
void sccp_session_tokenReject(sccp_session_t * session, uint32_t backoff_time);
void sccp_session_tokenAck(sccp_session_t * session);
void sccp_session_tokenRejectSPCP(sccp_session_t * session, uint32_t features);
void sccp_session_tokenAckSPCP(sccp_session_t * session, uint32_t features);
void sccp_socket_stop_sessionthread(sccp_session_t * session, uint8_t newRegistrationState);
#endif
