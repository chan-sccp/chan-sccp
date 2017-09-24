/*!
 * \file        sccp_netsock.h
 * \brief       SCCP NetSock Header
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#pragma once
#include "config.h"
#include "define.h"
#include <netinet/in.h>

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

__BEGIN_C_EXTERN__
SCCP_INLINE boolean_t SCCP_CALL sccp_netsock_is_IPv4(const struct sockaddr_storage *sockAddrStorage);
SCCP_INLINE boolean_t SCCP_CALL sccp_netsock_is_IPv6(const struct sockaddr_storage *sockAddrStorage);
SCCP_API uint16_t __PURE__ SCCP_CALL sccp_netsock_getPort(const struct sockaddr_storage *sockAddrStorage);
SCCP_API void SCCP_CALL sccp_netsock_setPort(const struct sockaddr_storage *sockAddrStorage, uint16_t port);
SCCP_API int __PURE__ SCCP_CALL sccp_netsock_is_any_addr(const struct sockaddr_storage *sockAddrStorage);
SCCP_API boolean_t SCCP_CALL sccp_netsock_getExternalAddr(struct sockaddr_storage *sockAddrStorage, int family);
SCCP_API void SCCP_CALL sccp_netsock_flush_externhost(void);
SCCP_API size_t __PURE__ SCCP_CALL sccp_netsock_sizeof(const struct sockaddr_storage *sockAddrStorage);
SCCP_API boolean_t __PURE__ SCCP_CALL sccp_netsock_is_mapped_IPv4(const struct sockaddr_storage *sockAddrStorage);
SCCP_API boolean_t SCCP_CALL sccp_netsock_ipv4_mapped(const struct sockaddr_storage *sockAddrStorage, struct sockaddr_storage *sockAddrStorage_mapped);
SCCP_API int SCCP_CALL sccp_netsock_cmp_addr(const struct sockaddr_storage *a, const struct sockaddr_storage *b);
SCCP_API int SCCP_CALL sccp_netsock_cmp_port(const struct sockaddr_storage *a, const struct sockaddr_storage *b);
SCCP_API int SCCP_CALL sccp_netsock_split_hostport(char *str, char **host, char **port, int flags);

/* helper: easy replacement for inet_ntop for use in sccp_log functions (threadsafe) */
char *__netsock_stringify_fmt(const struct sockaddr_storage *sockAddrStorage, int format);

/* begin sccp_netsock_stringify_fmt short cuts */
static inline char * SCCP_CALL sccp_netsock_stringify(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_DEFAULT);
}

static inline char * SCCP_CALL sccp_netsock_stringify_remote(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_DEFAULT_REMOTE);
}

static inline char * SCCP_CALL sccp_netsock_stringify_addr(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_ADDR);
}

static inline char * SCCP_CALL sccp_netsock_stringify_addr_remote(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_ADDR_REMOTE);
}

static inline char * SCCP_CALL sccp_netsock_stringify_host(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_HOST);
}

static inline char * SCCP_CALL sccp_netsock_stringify_host_remote(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_HOST_REMOTE);
}

static inline char * SCCP_CALL sccp_netsock_stringify_port(const struct sockaddr_storage *sockAddrStorage)
{
	return __netsock_stringify_fmt(sockAddrStorage, SCCP_SOCKADDR_STR_PORT);
}

/* end sccp_netsock_stringify_fmt short cuts */
SCCP_API void SCCP_CALL sccp_netsock_setoptions(int new_socket, int reuse, int linger, int keepalive);
SCCP_API void * SCCP_CALL sccp_netsock_thread(void *ignore);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
