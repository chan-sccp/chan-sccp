/*!
 * \file        sccp_sockaddr.c
 * \brief       SCCP Sockaddr Class
 * \note        Reworked, but based on asterisk netsock2.h code.
 *              The original chan_sccp driver that was made by Viagénie <asteriskv6@viagenie.ca>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-02-24 16:19:24 +0100 (Sun, 24 Feb 2013) $
 * $Revision: 4273 $
 */

/*! \file
 * \brief Network socket handling / sockaddr handling
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 369110 $")
#include "sccp_sockaddr.h"
int sccp_sockaddr_ipv4_mapped(const sccp_sockaddr_t *addr, sccp_sockaddr_t *sccp_mapped)
{
	const struct sockaddr_in6 *sin6;
	struct sockaddr_in sin4;

	if (!sccp_sockaddr_is_ipv6(addr)) {
		return 0;
	}

	if (!sccp_sockaddr_is_ipv4_mapped(addr)) {
		return 0;
	}

	sin6 = (const struct sockaddr_in6 *) &addr->ss;

	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_port = sin6->sin6_port;
	sin4.sin_addr.s_addr = ((uint32_t *) & sin6->sin6_addr)[3];

	sccp_sockaddr_from_sin(sccp_mapped, &sin4);

	return 1;
}

char *sccp_sockaddr_get_ip_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen)
{
	switch (sa->ss.ss_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *) sa)->sin_addr), s, maxlen);
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) sa)->sin6_addr), s, maxlen);
			break;

		default:
			strncpy(s, "Unknown AF", maxlen);
			return NULL;
	}

	return s;
}

char *sccp_sockaddr_get_port_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen)
{
	switch (sa->ss.ss_family) {
		case AF_INET:
			snprintf(s, maxlen, "%us", ntohs(((struct sockaddr_in *) sa)->sin_port));
			break;

		case AF_INET6:
			snprintf(s, maxlen, "%us", ntohs(((struct sockaddr_in6 *) sa)->sin6_port));
			break;

		default:
			strncpy(s, "Unknown AF", maxlen);
			return NULL;
	}

	return s;
}

char *sccp_sockaddr_get_both_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen)
{
	char tmpstr[INET6_ADDRSTRLEN];

	switch (sa->ss.ss_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *) sa)->sin_addr), tmpstr, INET_ADDRSTRLEN);
			snprintf(s, maxlen, "%s:%us", tmpstr, ntohs(((struct sockaddr_in *) sa)->sin_port));
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) sa)->sin6_addr), tmpstr, INET6_ADDRSTRLEN);
			snprintf(s, maxlen, "%s:%us", tmpstr, ntohs(((struct sockaddr_in6 *) sa)->sin6_port));
			break;

		default:
			strncpy(s, "Unknown AF", maxlen);
			return NULL;
	}

	return s;
}

socklen_t sccp_sockaddr_create_from_in_addr(const sccp_in_addr_t * addr, uint16_t port, sccp_sockaddr_t * sockaddr)
{
	if (addr->type == SCCP_AF_INET) {
		struct sockaddr_in sock4;

		memset(&sock4, 0, sizeof(sock4));
		sock4.sin_family = AF_INET;
		sock4.sin_addr.s_addr = addr->addr.addr4.s_addr;
//		sock4.sin_port = htons(port);
		sock4.sin_port = port;
		memcpy(sockaddr, &sock4, sizeof(sock4));
		return sizeof(struct sockaddr_in);
	} else {
		struct sockaddr_in6 sock6;

		memset(&sock6, 0, sizeof(sock6));
		sock6.sin6_family = AF_INET6;
//		sock6.sin6_port = htons(port);
		sock6.sin6_port = port;
		sock6.sin6_flowinfo = 0;
		sock6.sin6_addr = addr->addr.addr6;
		memcpy(sockaddr, &sock6, sizeof(sock6));
		return sizeof(struct sockaddr_in6);
	}
}

int sccp_sockaddr_split_hostport(char *str, char **host, char **port, int flags)
{
	char *s = str;
	char *orig_str = str;											/* Original string in case the port presence is incorrect. */
	char *host_end = NULL;											/* Delay terminating the host in case the port presence is incorrect. */

	sccp_log(DEBUGCAT_SOCKET) ("Splitting '%s' into...\n", str);
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
	sccp_log(DEBUGCAT_SOCKET) ("...host '%s' and port '%s'.\n", *host, *port ? *port : "");
	return 1;
}

int sccp_sockaddr_parse(sccp_sockaddr_t * addr, const char *str, int flags)
{
	struct addrinfo hints;
	struct addrinfo *res;
	char *s;
	char *host;
	char *port;
	int e;

	s = sccp_strdupa(str);
	if (!sccp_sockaddr_split_hostport(s, &host, &port, flags)) {
		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	/* Hint to get only one entry from getaddrinfo */
	hints.ai_socktype = SOCK_DGRAM;

#ifdef AI_NUMERICSERV
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
#else
	hints.ai_flags = AI_NUMERICHOST;
#endif
	if ((e = getaddrinfo(host, port, &hints, &res))) {
		if (e != EAI_NONAME) {										/* if this was just a host name rather than a ip address, don't print error */
			pbx_log(LOG_ERROR, "getaddrinfo(\"%s\", \"%s\", ...): %s\n", host, S_OR(port, "(null)"), gai_strerror(e));
		}
		return 0;
	}

	/*
	 * I don't see how this could be possible since we're not resolving host
	 * names. But let's be careful...
	 */
	if (res->ai_next != NULL) {
		pbx_log(LOG_WARNING, "getaddrinfo() returned multiple " "addresses. Ignoring all but the first.\n");
	}

	if (addr) {
		addr->len = res->ai_addrlen;
		memcpy(&addr->ss, res->ai_addr, addr->len);
	}

	freeaddrinfo(res);

	return 1;
}

int sccp_sockaddr_resolve(sccp_sockaddr_t ** addrs, const char *str, int flags, int family)
{
	struct addrinfo hints, *res, *ai;
	char *s, *host, *port;
	int e, i, res_cnt;

	if (!str) {
		return 0;
	}

	s = sccp_strdupa(str);
	if (!sccp_sockaddr_split_hostport(s, &host, &port, flags)) {
		return 0;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;

	if ((e = getaddrinfo(host, port, &hints, &res))) {
		pbx_log(LOG_ERROR, "getaddrinfo(\"%s\", \"%s\", ...): %s\n", host, S_OR(port, "(null)"), gai_strerror(e));
		return 0;
	}

	res_cnt = 0;
	for (ai = res; ai; ai = ai->ai_next) {
		res_cnt++;
	}

	if (res_cnt == 0) {
		goto cleanup;
	}

	if ((*addrs = ast_malloc(res_cnt * sizeof(sccp_sockaddr_t))) == NULL) {
		res_cnt = 0;
		goto cleanup;
	}

	i = 0;
	for (ai = res; ai; ai = ai->ai_next) {
		(*addrs)[i].len = ai->ai_addrlen;
		memcpy(&(*addrs)[i].ss, ai->ai_addr, ai->ai_addrlen);
		++i;
	}

cleanup:
	freeaddrinfo(res);
	return res_cnt;
}

int sccp_sockaddr_cmp(const sccp_sockaddr_t * a, const sccp_sockaddr_t * b)
{
	const sccp_sockaddr_t *a_tmp, *b_tmp;
	sccp_sockaddr_t ipv4_mapped;

	a_tmp = a;
	b_tmp = b;

	if (a_tmp->len != b_tmp->len) {
		if (sccp_sockaddr_ipv4_mapped(a, &ipv4_mapped)) {
			a_tmp = &ipv4_mapped;
		} else if (sccp_sockaddr_ipv4_mapped(b, &ipv4_mapped)) {
			b_tmp = &ipv4_mapped;
		}
	}

	if (a_tmp->len < b_tmp->len) {
		return -1;
	} else if (a_tmp->len > b_tmp->len) {
		return 1;
	}

	return memcmp(&a_tmp->ss, &b_tmp->ss, a_tmp->len);
}

int sccp_sockaddr_cmp_addr(const sccp_sockaddr_t * a, const sccp_sockaddr_t * b)
{
	const sccp_sockaddr_t *a_tmp, *b_tmp;
	sccp_sockaddr_t ipv4_mapped;
	const struct in_addr *ip4a, *ip4b;
	const struct in6_addr *ip6a, *ip6b;
	int ret = -1;

	a_tmp = a;
	b_tmp = b;

	if (a_tmp->len != b_tmp->len) {
		if (sccp_sockaddr_ipv4_mapped(a, &ipv4_mapped)) {
			a_tmp = &ipv4_mapped;
		} else if (sccp_sockaddr_ipv4_mapped(b, &ipv4_mapped)) {
			b_tmp = &ipv4_mapped;
		}
	}

	if (a->len < b->len) {
		ret = -1;
	} else if (a->len > b->len) {
		ret = 1;
	}

	switch (a_tmp->ss.ss_family) {
		case AF_INET:
			ip4a = &((const struct sockaddr_in *) &a_tmp->ss)->sin_addr;
			ip4b = &((const struct sockaddr_in *) &b_tmp->ss)->sin_addr;
			ret = memcmp(ip4a, ip4b, sizeof(*ip4a));
			break;
		case AF_INET6:
			ip6a = &((const struct sockaddr_in6 *) &a_tmp->ss)->sin6_addr;
			ip6b = &((const struct sockaddr_in6 *) &b_tmp->ss)->sin6_addr;
			ret = memcmp(ip6a, ip6b, sizeof(*ip6a));
			break;
	}
	return ret;
}

uint16_t _sccp_sockaddr_port(const sccp_sockaddr_t * addr, const char *file, int line, const char *func)
{
	if (addr->ss.ss_family == AF_INET && addr->len == sizeof(struct sockaddr_in)) {
		return ntohs(((struct sockaddr_in *) &addr->ss)->sin_port);
	} else if (addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6)) {
		return ntohs(((struct sockaddr_in6 *) &addr->ss)->sin6_port);
	}
	if (option_debug >= 1) {
		pbx_log(__LOG_DEBUG, file, line, func, "Not an IPv4 nor IPv6 address, cannot get port.\n");
	}
	return 0;
}

void _sccp_sockaddr_set_port(sccp_sockaddr_t * addr, uint16_t port, const char *file, int line, const char *func)
{
	if (addr->ss.ss_family == AF_INET && addr->len == sizeof(struct sockaddr_in)) {
		((struct sockaddr_in *) &addr->ss)->sin_port = htons(port);
	} else if (addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6)) {
		((struct sockaddr_in6 *) &addr->ss)->sin6_port = htons(port);
	} else if (option_debug >= 1) {
		pbx_log(__LOG_DEBUG, file, line, func, "Not an IPv4 nor IPv6 address, cannot set port.\n");
	}
}

uint32_t sccp_sockaddr_ipv4(const sccp_sockaddr_t * addr)
{
	const struct sockaddr_in *sin = (struct sockaddr_in *) &addr->ss;

	return ntohl(sin->sin_addr.s_addr);
}

int sccp_sockaddr_is_ipv4(const sccp_sockaddr_t * addr)
{
	return addr->ss.ss_family == AF_INET && addr->len == sizeof(struct sockaddr_in);
}

int sccp_sockaddr_is_ipv4_mapped(const sccp_sockaddr_t * addr)
{
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &addr->ss;

	return addr->len && IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr);
}

int sccp_sockaddr_is_ipv4_multicast(const sccp_sockaddr_t * addr)
{
	return ((sccp_sockaddr_ipv4(addr) & 0xf0000000) == 0xe0000000);
}

int sccp_sockaddr_is_ipv6_link_local(const sccp_sockaddr_t * addr)
{
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &addr->ss;

	return sccp_sockaddr_is_ipv6(addr) && IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr);
}

int sccp_sockaddr_is_ipv6(const sccp_sockaddr_t * addr)
{
	return addr->ss.ss_family == AF_INET6 && addr->len == sizeof(struct sockaddr_in6);
}

int sccp_sockaddr_is_any(const sccp_sockaddr_t * addr)
{
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} tmp_addr = {
	.ss = addr->ss,};

	return (sccp_sockaddr_is_ipv4(addr) && (tmp_addr.sin.sin_addr.s_addr == INADDR_ANY)) || (sccp_sockaddr_is_ipv6(addr) && IN6_IS_ADDR_UNSPECIFIED(&tmp_addr.sin6.sin6_addr));
}

int sccp_sockaddr_hash(const sccp_sockaddr_t * addr)
{
	/*
	 * For IPv4, return the IP address as-is. For IPv6, return the last 32
	 * bits.
	 */
	switch (addr->ss.ss_family) {
		case AF_INET:
			return ((const struct sockaddr_in *) &addr->ss)->sin_addr.s_addr;
		case AF_INET6:
			return ((uint32_t *) & ((const struct sockaddr_in6 *) &addr->ss)->sin6_addr)[3];
		default:
			pbx_log(LOG_ERROR, "Unknown address family '%d'.\n", addr->ss.ss_family);
			return 0;
	}
}

int sccp_socket_accept(int sockfd, sccp_sockaddr_t * addr)
{
	addr->len = sizeof(addr->ss);
	return accept(sockfd, (struct sockaddr *) &addr->ss, &addr->len);
}

int sccp_socket_bind(int sockfd, const sccp_sockaddr_t * addr)
{
	return bind(sockfd, (const struct sockaddr *) &addr->ss, addr->len);
}

int sccp_socket_connect(int sockfd, const sccp_sockaddr_t * addr)
{
	return connect(sockfd, (const struct sockaddr *) &addr->ss, addr->len);
}

int sccp_socket_getsockname(int sockfd, sccp_sockaddr_t * addr)
{
	addr->len = sizeof(addr->ss);
	return getsockname(sockfd, (struct sockaddr *) &addr->ss, &addr->len);
}

ssize_t sccp_socket_recvfrom(int sockfd, void *buf, size_t len, int flags, sccp_sockaddr_t * src_addr)
{
	src_addr->len = sizeof(src_addr->ss);
	return recvfrom(sockfd, buf, len, flags, (struct sockaddr *) &src_addr->ss, &src_addr->len);
}

ssize_t sccp_socket_sendto(int sockfd, const void *buf, size_t len, int flags, const sccp_sockaddr_t * dest_addr)
{
	return sendto(sockfd, buf, len, flags, (const struct sockaddr *) &dest_addr->ss, dest_addr->len);
}

int sccp_socket_set_qos(int sockfd, int tos, int cos, const char *desc)
{
	int res = 0;
	int set_tos;
	int set_tclass;
	sccp_sockaddr_t addr;

	/* If the sock address is IPv6, the TCLASS field must be set. */
	set_tclass = !sccp_socket_getsockname(sockfd, &addr) && sccp_sockaddr_is_ipv6(&addr) ? 1 : 0;

	/* If the the sock address is IPv4 or (IPv6 set to any address [::]) set TOS bits */
	set_tos = (!set_tclass || (set_tclass && sccp_sockaddr_is_any(&addr))) ? 1 : 0;

	if (set_tos) {
		if ((res = setsockopt(sockfd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)))) {
			pbx_log(LOG_WARNING, "Unable to set %s DSCP TOS value to %d (may be you have no " "root privileges): %s\n", desc, tos, strerror(errno));
		} else if (tos) {
			sccp_log(DEBUGCAT_SOCKET) ("Using %s TOS bits %d\n", desc, tos);
		}
	}
#if defined(IPV6_TCLASS) && defined(IPPROTO_IPV6)
	if (set_tclass) {
		if (!sccp_socket_getsockname(sockfd, &addr) && sccp_sockaddr_is_ipv6(&addr)) {
			if ((res = setsockopt(sockfd, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)))) {
				pbx_log(LOG_WARNING, "Unable to set %s DSCP TCLASS field to %d (may be you have no " "root privileges): %s\n", desc, tos, strerror(errno));
			} else if (tos) {
				sccp_log(DEBUGCAT_SOCKET) ("Using %s TOS bits %d in TCLASS field.\n", desc, tos);
			}
		}
	}
#endif

#ifdef linux
	if (setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &cos, sizeof(cos))) {
		pbx_log(LOG_WARNING, "Unable to set %s CoS to %d: %s\n", desc, cos, strerror(errno));
	} else if (cos) {
		sccp_log(DEBUGCAT_SOCKET) ("Using %s CoS mark %d\n", desc, cos);
	}
#endif

	return res;
}

/*!
 * These are backward compatibility functions that may be used by subsystems
 * that have not yet been converted to IPv6. They will be removed when all
 * subsystems are IPv6-ready.
 */

/*
 * \deprecated
 */
int _sccp_sockaddr_to_sin(const sccp_sockaddr_t * addr, struct sockaddr_in *sin, const char *file, int line, const char *func)
{
	if (sccp_sockaddr_isnull(addr)) {
		memset(sin, 0, sizeof(*sin));
		return 1;
	}

	if (addr->len != sizeof(*sin)) {
		pbx_log(__LOG_ERROR, file, line, func, "Bad address cast to IPv4\n");
		return 0;
	}

	if (addr->ss.ss_family != AF_INET && option_debug >= 1) {
		pbx_log(__LOG_DEBUG, file, line, func, "Address family is not AF_INET\n");
	}

	*sin = *(struct sockaddr_in *) &addr->ss;
	return 1;
}

/*
 * \deprecated
 */
void _sccp_sockaddr_from_sin(sccp_sockaddr_t * addr, const struct sockaddr_in *sin, const char *file, int line, const char *func)
{
	memcpy(&addr->ss, sin, sizeof(*sin));

	if (addr->ss.ss_family != AF_INET && option_debug >= 1) {
		pbx_log(__LOG_DEBUG, file, line, func, "Address family is not AF_INET\n");
	}

	addr->len = sizeof(*sin);
}
