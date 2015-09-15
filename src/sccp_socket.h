/*!
 * \file        sccp_socket.h
 * \brief       SCCP Socket Header
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

#ifndef __SCCP_SOCKET_H
#define __SCCP_SOCKET_H

#include "sccp_cli.h"

/* ------------------------------------------------------------------------------------------------------- SOCKET FUNCTIONS - */

/*!
 * \brief SCCP Host Access Rule Structure
 *
 * internal representation of acl entries In principle user applications would have no need for this,
 * but there is sometimes a need to extract individual items, e.g. to print them, and rather than defining iterators to
 * navigate the list, and an externally visible 'struct ast_ha_entry', at least in the short term it is more convenient to make the whole
 * thing public and let users play with them.
 */
struct sccp_ha {
	struct sockaddr_storage netaddr;
	struct sockaddr_storage netmask;
	struct sccp_ha *next;
	int sense;
};

boolean_t sccp_socket_is_IPv4(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_is_IPv6(const struct sockaddr_storage *sockAddrStorage);
uint16_t sccp_socket_getPort(const struct sockaddr_storage *sockAddrStorage);
void sccp_socket_setPort(const struct sockaddr_storage *sockAddrStorage, uint16_t port);
int sccp_socket_is_any_addr(const struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_getExternalAddr(struct sockaddr_storage *sockAddrStorage);
boolean_t sccp_socket_getOurIP(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage, int family);
boolean_t sccp_socket_getSas(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage);
int sccp_socket_setOurIP4Address(constSessionPtr session, const struct sockaddr_storage *addr) ;
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
void sccp_socket_setoptions(int new_socket);
void *sccp_socket_thread(void *ignore);

/* ------------------------------------------------------------------------------------------------------- SESSION FUNCTIONS - */
struct sccp_session;

void sccp_session_terminateAll();
const char *const sccp_session_getDesignator(constSessionPtr session);
void sccp_session_sendmsg(constDevicePtr device, sccp_mid_t t);
int sccp_session_send(constDevicePtr device, const sccp_msg_t * msg);
int sccp_session_send2(constSessionPtr s, sccp_msg_t * msg);
int sccp_session_retainDevice(constSessionPtr session, constDevicePtr device);
void sccp_session_releaseDevice(constSessionPtr volatile session);
sccp_session_t *sccp_session_reject(constSessionPtr session, char *message);
void sccp_session_tokenReject(constSessionPtr session, uint32_t backoff_time);
void sccp_session_tokenAck(constSessionPtr session);
void sccp_session_tokenRejectSPCP(constSessionPtr session, uint32_t features);
void sccp_session_tokenAckSPCP(constSessionPtr session, uint32_t features);
void sccp_session_stopthread(constSessionPtr session, uint8_t newRegistrationState);
void sccp_session_setProtocol(constSessionPtr session, uint16_t protocolType);
uint16_t sccp_session_getProtocol(constSessionPtr session);
void sccp_session_resetLastKeepAlive(constSessionPtr session);
boolean_t sccp_session_check_crossdevice(constSessionPtr session, constDevicePtr device);
sccp_device_t * const sccp_session_getDevice(constSessionPtr session, boolean_t required);
boolean_t sccp_session_isValid(constSessionPtr session);
int sccp_cli_show_sessions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
