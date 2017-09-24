/*!
 * \file        sccp_netsock.c
 * \brief       SCCP NetSock Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#include "config.h"
#include "common.h"
#include "sccp_netsock.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_session.h"
#include <netinet/in.h>

/* arbitrary values */
//#define NETSOCK_TIMEOUT_SEC 0											/* timeout after seven seconds when trying to read/write from/to a socket */
//#define NETSOCK_TIMEOUT_MILLISEC 500										/* "       "     0 milli seconds "    "    */
#define NETSOCK_KEEPALIVE_CNT 3											/* The maximum number of keepalive probes TCP should send before dropping the connection. */
#define NETSOCK_LINGER_WAIT 0											/* but wait 0 milliseconds before closing socket and discard all outboung messages */
#define NETSOCK_RCVBUF SCCP_MAX_PACKET										/* SO_RCVBUF */
#define NETSOCK_SNDBUF (SCCP_MAX_PACKET * 5)									/* SO_SNDBUG */

union sockaddr_union {
	struct sockaddr sa;
	struct sockaddr_storage ss;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
};

gcc_inline boolean_t sccp_netsock_is_IPv4(const struct sockaddr_storage *sockAddrStorage)
{
	return (sockAddrStorage->ss_family == AF_INET) ? TRUE : FALSE;
}

gcc_inline boolean_t sccp_netsock_is_IPv6(const struct sockaddr_storage *sockAddrStorage)
{
	return (sockAddrStorage->ss_family == AF_INET6) ? TRUE : FALSE;
}

uint16_t __PURE__ sccp_netsock_getPort(const struct sockaddr_storage * sockAddrStorage)
{
	if (sccp_netsock_is_IPv4(sockAddrStorage)) {
		return ntohs(((struct sockaddr_in *) sockAddrStorage)->sin_port);
	}
	if (sccp_netsock_is_IPv6(sockAddrStorage)) {
		return ntohs(((struct sockaddr_in6 *) sockAddrStorage)->sin6_port);
	}
	return 0;
}

void sccp_netsock_setPort(const struct sockaddr_storage *sockAddrStorage, uint16_t port)
{
	if (sccp_netsock_is_IPv4(sockAddrStorage)) {
		((struct sockaddr_in *) sockAddrStorage)->sin_port = htons(port);
	} else if (sccp_netsock_is_IPv6(sockAddrStorage)) {
		((struct sockaddr_in6 *) sockAddrStorage)->sin6_port = htons(port);
	}
}

int __PURE__ sccp_netsock_is_any_addr(const struct sockaddr_storage *sockAddrStorage)
{
	union sockaddr_union tmp_addr = {
		.ss = *sockAddrStorage,
	};

	return (sccp_netsock_is_IPv4(sockAddrStorage) && (tmp_addr.sin.sin_addr.s_addr == INADDR_ANY)) || (sccp_netsock_is_IPv6(sockAddrStorage) && IN6_IS_ADDR_UNSPECIFIED(&tmp_addr.sin6.sin6_addr));
}

static boolean_t __netsock_resolve_first_af(struct sockaddr_storage *addr, const char *name, int family)
{
	struct addrinfo *res;
	int e;
	boolean_t result = FALSE;
	if (!name) {
		return FALSE;
	}

	struct addrinfo hints = {
		.ai_family = family,
		.ai_socktype = SOCK_STREAM,
	};
#if defined(AI_ADDRCONFIG)
       	hints.ai_flags |= AI_ADDRCONFIG;
#endif
#if defined(AI_V4MAPPED)
	hints.ai_flags |= AI_V4MAPPED;
#endif

	if ((e = getaddrinfo(name, NULL, &hints, &res)) == 0) {
		memcpy(addr, res->ai_addr, res->ai_addrlen);
		result = TRUE;
	} else {
		if (e == EAI_NONAME) {
			pbx_log(LOG_ERROR, "SCCP: name:%s could not be resolved\n", name);
		} else {
			pbx_log(LOG_ERROR, "getaddrinfo(\"%s\") failed: %s\n", name, gai_strerror(e));
		}
	}
	freeaddrinfo(res);
	return result;
}

static struct {
	time_t expire;
	struct sockaddr_storage ip;
} externhost[] = {
	[AF_INET]  = {0, {.ss_family = AF_INET}},
	[AF_INET6] = {0, {.ss_family = AF_INET6}},
};

boolean_t sccp_netsock_getExternalAddr(struct sockaddr_storage *sockAddrStorage, int family)
{
	boolean_t result = FALSE;
	if (sccp_netsock_is_any_addr(&GLOB(externip))) {
		if (GLOB(externhost) && strlen(GLOB(externhost)) != 0 && GLOB(externrefresh) > 0) {
			if (time(NULL) >= externhost[family].expire) {
				if (!__netsock_resolve_first_af(&externhost[family].ip, GLOB(externhost), family)) {
					pbx_log(LOG_NOTICE, "Warning: Resolving '%s' failed!\n", GLOB(externhost));
					return FALSE;
				}
				externhost[family].expire = time(NULL) + GLOB(externrefresh);
			}
			memcpy(sockAddrStorage, &externhost[family].ip, sizeof(struct sockaddr_storage));
			sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_3 "SCCP: %s resolved to %s\n", GLOB(externhost), sccp_netsock_stringify_addr(sockAddrStorage));
			result = TRUE;
		} else {
			sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP: No externip/externhost set in sccp.conf.\nWhen you are running your PBX on a seperate host behind a NAT-TING Firewall you need to set externip/externhost.\n");
		}
	} else {
		memcpy(sockAddrStorage, &GLOB(externip), sizeof(struct sockaddr_storage));
		result = TRUE;
	}
	return result;
}

void sccp_netsock_flush_externhost(void) 
{
	externhost[AF_INET].expire = 0;
	externhost[AF_INET6].expire = 0;
}

size_t __PURE__ sccp_netsock_sizeof(const struct sockaddr_storage * sockAddrStorage)
{
	if (sccp_netsock_is_IPv4(sockAddrStorage)) {
		return sizeof(struct sockaddr_in);
	}
	if (sccp_netsock_is_IPv6(sockAddrStorage)) {
		return sizeof(struct sockaddr_in6);
	}
	return 0;
}

static int sccp_netsock_is_ipv6_link_local(const struct sockaddr_storage *sockAddrStorage)
{
	union sockaddr_union tmp_addr = {
		.ss = *sockAddrStorage,
	};
	return sccp_netsock_is_IPv6(sockAddrStorage) && IN6_IS_ADDR_LINKLOCAL(&tmp_addr.sin6.sin6_addr);
}

boolean_t __PURE__ sccp_netsock_is_mapped_IPv4(const struct sockaddr_storage *sockAddrStorage)
{
	if (sccp_netsock_is_IPv6(sockAddrStorage)) {
		const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sockAddrStorage;
		return IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr);
	} 
	return FALSE;
}

boolean_t sccp_netsock_ipv4_mapped(const struct sockaddr_storage *sockAddrStorage, struct sockaddr_storage *sockAddrStorage_mapped)
{
	const struct sockaddr_in6 *sin6;
	struct sockaddr_in sin4;

	if (!sccp_netsock_is_IPv6(sockAddrStorage)) {
		return FALSE;
	}

	if (!sccp_netsock_is_mapped_IPv4(sockAddrStorage)) {
		return FALSE;
	}

	sin6 = (const struct sockaddr_in6 *) sockAddrStorage;

	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_port = sin6->sin6_port;
	sin4.sin_addr.s_addr = ((uint32_t *) & sin6->sin6_addr)[3];

	memcpy(sockAddrStorage_mapped, &sin4, sizeof(sin4));
	return TRUE;
}

/*!
 * \brief
 * Compares the addresses of two sockaddr structures.
 *
 * \retval -1 \a a is lexicographically smaller than \a b
 * \retval 0 \a a is equal to \a b
 * \retval 1 \a b is lexicographically smaller than \a a
 */
int sccp_netsock_cmp_addr(const struct sockaddr_storage *a, const struct sockaddr_storage *b)
{
	//char *stra = pbx_strdupa(sccp_netsock_stringpify_addr(a));
	//char *strb = pbx_strdupa(sccp_netsock_stringify_addr(b));

	const struct sockaddr_storage *a_tmp, *b_tmp;
	struct sockaddr_storage ipv4_mapped;
	size_t len_a = sccp_netsock_sizeof(a);
	size_t len_b = sccp_netsock_sizeof(b);
	int ret = -1;

	a_tmp = a;
	b_tmp = b;

	if (len_a != len_b) {
		if (sccp_netsock_ipv4_mapped(a, &ipv4_mapped)) {
			a_tmp = &ipv4_mapped;
		} else if (sccp_netsock_ipv4_mapped(b, &ipv4_mapped)) {
			b_tmp = &ipv4_mapped;
		}
	}
	if (len_a < len_b) {
		ret = -1;
		goto EXIT;
	} else if (len_a > len_b) {
		ret = 1;
		goto EXIT;
	}

	if (a_tmp->ss_family == b_tmp->ss_family) {
		if (a_tmp->ss_family == AF_INET) {
			ret = memcmp(&(((struct sockaddr_in *) a_tmp)->sin_addr), &(((struct sockaddr_in *) b_tmp)->sin_addr), sizeof(struct in_addr));
		} else {											// AF_INET6
			ret = memcmp(&(((struct sockaddr_in6 *) a_tmp)->sin6_addr), &(((struct sockaddr_in6 *) b_tmp)->sin6_addr), sizeof(struct in6_addr));
		}
	}
EXIT:
	//sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_2 "SCCP: sccp_netsock_cmp_addr(%s, %s) returning %d\n", stra, strb, ret);
	return ret;
}

/*!
 * \brief
 * Compares the port of two sockaddr structures.
 *
 * \retval -1 \a a is smaller than \a b
 * \retval 0 \a a is equal to \a b
 * \retval 1 \a b is smaller than \a a
 */
int sccp_netsock_cmp_port(const struct sockaddr_storage *a, const struct sockaddr_storage *b)
{
	uint16_t a_port = sccp_netsock_getPort(a);
	uint16_t b_port = sccp_netsock_getPort(b);

	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_2 "SCCP: sccp_netsock_cmp_port(%d, %d) returning %d\n", a_port, b_port, (a_port < b_port) ? -1 : (a_port == b_port) ? 0 : 1);

	return (a_port < b_port) ? -1 : (a_port == b_port) ? 0 : 1;
}

/*!
 * \brief
 * Splits a string into its host and port components
 *
 * \param str       [in] The string to parse. May be modified by writing a NUL at the end of
 *                  the host part.
 * \param host      [out] Pointer to the host component within \a str.
 * \param port      [out] Pointer to the port component within \a str.
 * \param flags     If set to zero, a port MAY be present. If set to PARSE_PORT_IGNORE, a
 *                  port MAY be present but will be ignored. If set to PARSE_PORT_REQUIRE,
 *                  a port MUST be present. If set to PARSE_PORT_FORBID, a port MUST NOT
 *                  be present.
 *
 * \retval 1 Success
 * \retval 0 Failure
 */
int sccp_netsock_split_hostport(char *str, char **host, char **port, int flags)
{
	char *s = str;
	char *orig_str = str;											/* Original string in case the port presence is incorrect. */
	char *host_end = NULL;											/* Delay terminating the host in case the port presence is incorrect. */

	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_4 "Splitting '%s' into...\n", str);
	*host = NULL;
	*port = NULL;
	if (*s == '[') {
		*host = ++s;
		for (; *s && *s != ']'; ++s) {
		}
		if (*s == ']') {
			host_end = s;
			++s;
		}
		if (*s == ':') {
			*port = s + 1;
		}
	} else {
		*host = s;
		for (; *s; ++s) {
			if (*s == ':') {
				if (*port) {
					*port = NULL;
					break;
				} else {
					*port = s;
				}
			}
		}
		if (*port) {
			host_end = *port;
			++*port;
		}
	}

	switch (flags & PARSE_PORT_MASK) {
		case PARSE_PORT_IGNORE:
			*port = NULL;
			break;
		case PARSE_PORT_REQUIRE:
			if (*port == NULL) {
				pbx_log(LOG_WARNING, "Port missing in %s\n", orig_str);
				return 0;
			}
			break;
		case PARSE_PORT_FORBID:
			if (*port != NULL) {
				pbx_log(LOG_WARNING, "Port disallowed in %s\n", orig_str);
				return 0;
			}
			break;
	}
	/* Can terminate the host string now if needed. */
	if (host_end) {
		*host_end = '\0';
	}
	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_4 "...host '%s' and port '%s'.\n", *host, *port ? *port : "");
	return 1;
}

AST_THREADSTORAGE(sccp_netsock_stringify_buf);
char *__netsock_stringify_fmt(const struct sockaddr_storage *sockAddrStorage, int format)
{
	const struct sockaddr_storage *sockAddrStorage_tmp;
	char host[NI_MAXHOST] = "";
	char port[NI_MAXSERV] = "";
	struct ast_str *str;
	int e;
	static const size_t size = sizeof(host) - 1 + sizeof(port) - 1 + 4;

	if (!sockAddrStorage) {
		return "(null)";
	}

	if (!(str = ast_str_thread_get(&sccp_netsock_stringify_buf, size))) {
		return "";
	}
	//if (sccp_netsock_ipv4_mapped(sockAddrStorage, &sockAddrStorage_tmp_ipv4)) {
	//	struct sockaddr_storage sockAddrStorage_tmp_ipv4;
	//	sockAddrStorage_tmp = &sockAddrStorage_tmp_ipv4;
	//#if DEBUG
	//	sccp_log(0)("SCCP: ipv4 mapped in ipv6 address\n");
	//#endif
	//} else {
	sockAddrStorage_tmp = sockAddrStorage;
	//}

	if ((e = getnameinfo((struct sockaddr *) sockAddrStorage_tmp, sccp_netsock_sizeof(sockAddrStorage_tmp), format & SCCP_SOCKADDR_STR_ADDR ? host : NULL, format & SCCP_SOCKADDR_STR_ADDR ? sizeof(host) : 0, format & SCCP_SOCKADDR_STR_PORT ? port : 0, format & SCCP_SOCKADDR_STR_PORT ? sizeof(port) : 0, NI_NUMERICHOST | NI_NUMERICSERV))) {
		sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_3 "SCCP: getnameinfo(): %s \n", gai_strerror(e));
		return "";
	}

	if ((format & SCCP_SOCKADDR_STR_REMOTE) == SCCP_SOCKADDR_STR_REMOTE) {
		char *p;

		if (sccp_netsock_is_ipv6_link_local(sockAddrStorage_tmp) && (p = strchr(host, '%'))) {
			*p = '\0';
		}
	}
	switch ((format & SCCP_SOCKADDR_STR_FORMAT_MASK)) {
		case SCCP_SOCKADDR_STR_DEFAULT:
			ast_str_set(&str, 0, sockAddrStorage_tmp->ss_family == AF_INET6 ? "[%s]:%s" : "%s:%s", host, port);
			break;
		case SCCP_SOCKADDR_STR_ADDR:
			ast_str_set(&str, 0, "%s", host);
			break;
		case SCCP_SOCKADDR_STR_HOST:
			ast_str_set(&str, 0, sockAddrStorage_tmp->ss_family == AF_INET6 ? "[%s]" : "%s", host);
			break;
		case SCCP_SOCKADDR_STR_PORT:
			ast_str_set(&str, 0, "%s", port);
			break;
		default:
			pbx_log(LOG_ERROR, "Invalid format\n");
			return "";
	}

	return ast_str_buffer(str);
}

#define SCCP_NETSOCK_SETOPTION(_SOCKET, _LEVEL,_OPTIONNAME, _OPTIONVAL, _OPTIONLEN) 							\
	if (setsockopt(_SOCKET, _LEVEL, _OPTIONNAME, (void*)(_OPTIONVAL), _OPTIONLEN) == -1) {						\
		if (errno != ENOTSUP) {													\
			pbx_log(LOG_WARNING, "Failed to set SCCP socket: " #_LEVEL ":" #_OPTIONNAME " error: '%s'\n", strerror(errno));	\
		}															\
	}

void sccp_netsock_setoptions(int new_socket, int reuse, int linger, int keepalive)
{
	int on = 1;

	/* reuse */
	if (reuse > -1) {
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#if defined(SO_REUSEPORT)
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
	}

	/* nodelay */
	// SCCP_NETSOCK_SETOPTION(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

	/* tos/cos */
	int value = (int) GLOB(sccp_tos);
	SCCP_NETSOCK_SETOPTION(new_socket, IPPROTO_IP, IP_TOS, &value, sizeof(value));
#if defined(linux)
	value = (int) GLOB(sccp_cos);
	SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_PRIORITY, &value, sizeof(value));

	/* rcvbuf / sndbug */
	int so_rcvbuf = NETSOCK_RCVBUF;
	int so_sndbuf = NETSOCK_SNDBUF;
	SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf, sizeof(int));
	SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_SNDBUF, &so_sndbuf, sizeof(int));

	/* linger */
	if (linger > -1) {
		struct linger so_linger = {linger, NETSOCK_LINGER_WAIT};						/* linger=on but wait NETSOCK_LINGER_WAIT milliseconds before closing socket and discard all outboung messages */
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
	}

	/* timeeo */
	//struct timeval mytv = { NETSOCK_TIMEOUT_SEC, NETSOCK_TIMEOUT_MILLISEC };				/* timeout after seven seconds when trying to read/write from/to a socket */
	//SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_RCVTIMEO, &mytv, sizeof(mytv));
	//SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_SNDTIMEO, &mytv, sizeof(mytv));

	/* keepalive */
	if (keepalive > -1) {
		int ip_keepidle  = keepalive;									/* The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes */
		int ip_keepintvl = keepalive;									/* The time (in seconds) between individual keepalive probes, once we have started to probe. */
		int ip_keepcnt   = NETSOCK_KEEPALIVE_CNT;							/* The maximum number of keepalive probes TCP should send before dropping the connection. */
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_TCP, TCP_KEEPIDLE, &ip_keepidle, sizeof(int));
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_TCP, TCP_KEEPINTVL, &ip_keepintvl, sizeof(int));
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_TCP, TCP_KEEPCNT, &ip_keepcnt, sizeof(int));
		SCCP_NETSOCK_SETOPTION(new_socket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
	}
	/* thin-tcp */
//#ifdef TCP_THIN_LINEAR_TIMEOUTS
//	SCCP_NETSOCK_SETOPTION(new_socket, IPPROTO_TCP, TCP_THIN_LINEAR_TIMEOUTS, &on, sizeof(on));
//	SCCP_NETSOCK_SETOPTION(new_socket, IPPROTO_TCP, TCP_THIN_DUPACK, &on, sizeof(on));
//#endif
#endif
}

#undef SCCP_NETSOCK_SETOPTION

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
