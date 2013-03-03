/*!
 * \file        sccp_sockaddr.c
 * \brief       SCCP Sockaddr Header
 * \note        Reworked, but partially based on asterisk netsock2.h code.
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

#ifndef _SCCP_SOCKADDR_H
#define _SCCP_SOCKADDR_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <sys/socket.h>
#include <netinet/in.h>

	/*!
	 * Values for address families that we support. This is reproduced from socket.h
	 * because we do not want users to include that file. Only sccp_sockaddr.c should ever include socket.h.
	 */
	typedef enum sccp_addr_type {
		SCCP_AF_UNSPEC = 0,
		SCCP_AF_INET = 2,
		SCCP_AF_INET6 = 10,
	} sccp_addr_t;

	/*!
	 * \since 4.1
	 *
	 * \brief Socket address structure.
	 *
	 * \details
	 * The first member is big enough to contain addresses of any
	 * family. The second member contains the length (in bytes) used
	 * in the first member.
	 *
	 * \note
	 * Some BSDs have the length embedded in sockaddr structs. We
	 * ignore them. (This is the right thing to do.)
	 *
	 * \note
	 * It is important to always initialize ast_sockaddr before use
	 * -- even if they are passed to ast_sockaddr_copy() as the
	 * underlying storage could be bigger than what ends up being
	 * copied -- leaving part of the data unitialized.
	 */
	typedef struct sccp_sockaddr {
		union {
			struct sockaddr sa;
			struct sockaddr_in sin;
			struct sockaddr_in6 sin6;
			struct sockaddr_storage ss;
		};
		socklen_t len;
	} sccp_sockaddr_t;

	typedef struct sccp_in_addr_type {
		sccp_addr_t type;
		union {
			/* The order here is important for tr_in{,6}addr_any initialization,
			 * since we can't use C99 designated initializers */
			struct in6_addr addr6;
			struct in_addr addr4;
		} addr;
	} sccp_in_addr_t;

	/*!
	 * \brief
	 * Convert an IPv4-mapped IPv6 address into an IPv4 address.
	 *
	 * \warning You should rarely need this function. Only call this
	 * if you know what you're doing.
	 * 
	 * \param addr The IPv4-mapped address to convert
	 * \param ast_mapped The resulting IPv4 address
	 * \retval 0 Unable to make the conversion
	 * \retval 1 Successful conversion
	 */
	int sccp_sockaddr_ipv4_mapped(const sccp_sockaddr_t * addr, sccp_sockaddr_t * sccp_mapped);

	/*!
	 * \brief
	 * Get the correct in_addr/in6_addr from sccp_socketaddr independent of IPV4/IPV6 type.
	 * 
	 * \param sa sccp_sockaddr_t 
	 * \retval in_addr/in6_addr as void
	 */
	static inline void inline *sccp_sockaddr_get_in_addr(sccp_sockaddr_t * addr) {
		if (addr->ss.ss_family == AF_INET) {
			return &(addr->sin.sin_addr);
		} else {
			return &(addr->sin6.sin6_addr);
		}
	}

	/*!
	 * \brief
	 * Convert a struct sockaddr ip-address to a string (either IPv4 or IPv6)
	 *
	 * \param addr Pointer to the sccp_sockaddr we return the string for
	 * \param s Pointer to the char array, which needs to be free-ed after use
	 * \param maxlen Maximum length of s
	 * \retval Pointer to s
	 */
	char *sccp_sockaddr_get_ip_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen);

	/*!
	 * \brief
	 * Convert a struct sockaddr ip-port to a string (either IPv4 or IPv6)
	 *
	 * \param addr Pointer to the sccp_sockaddr we return the string for
	 * \param s Pointer to the char array, which needs to be free-ed after use
	 * \param maxlen Maximum length of s
	 * \retval Pointer to s
	 */
	char *sccp_sockaddr_get_port_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen);

	/*!
	 * \brief
	 * Convert a struct sockaddr ip-address and ip-port to a string (either IPv4 or IPv6), seperated by a colon
	 *
	 * \param addr Pointer to the sccp_sockaddr we return the string for
	 * \param s Pointer to the char array, which needs to be free-ed after use
	 * \param maxlen Maximum length of s
	 * \retval Pointer to s
	 */
	char *sccp_sockaddr_get_both_str(const sccp_sockaddr_t * sa, char *s, size_t maxlen);

	/*!
	 * \brief
	 * Create a new sccp_sockaddr from generic in_addr/in6_addr
	 *
	 * \param addr Const Pointer to sccp_in_addr_t
	 * \param port Port as uint16_t
	 * \param sockaddr Filled out sockaddr by ref
	 * \retval Size of sockaddr
	 */
	socklen_t sccp_sockaddr_create_from_in_addr(const sccp_in_addr_t * addr, uint16_t port, sccp_sockaddr_t * sockaddr);

	/*!
	 * \brief
	 * Checks if the sccp_sockaddr is null. "null" in this sense essentially
	 * means uninitialized, or having a 0 length.
	 *
	 * \param addr Pointer to the sccp_sockaddr we wish to check
	 * \retval 1 \a addr is null
	 * \retval 0 \a addr is non-null.
	 */
	static inline int sccp_sockaddr_isnull(const sccp_sockaddr_t * addr) {
		return !addr || addr->len == 0;
	}

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Sets address \a addr to null.
	 *
	 * \retval void
	 */
	static inline void sccp_sockaddr_setnull(sccp_sockaddr_t * addr) {
		addr->len = 0;
	}

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Copies the data from one sccp_sockaddr to another
	 *
	 * \param dst The destination sccp_sockaddr
	 * \param src The source sccp_sockaddr
	 * \retval void
	 */
	static inline void sccp_sockaddr_copy(sccp_sockaddr_t * dst, const sccp_sockaddr_t * src) {
		memcpy(dst, src, src->len);
		dst->len = src->len;
	};

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Compares two sccp_sockaddr structures
	 *
	 * \retval -1 \a a is lexicographically smaller than \a b
	 * \retval 0 \a a is equal to \a b
	 * \retval 1 \a b is lexicographically smaller than \a a
	 */
	int sccp_sockaddr_cmp(const sccp_sockaddr_t * a, const sccp_sockaddr_t * b);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Compares the addresses of two sccp_sockaddr structures.
	 *
	 * \retval -1 \a a is lexicographically smaller than \a b
	 * \retval 0 \a a is equal to \a b
	 * \retval 1 \a b is lexicographically smaller than \a a
	 */
	int sccp_sockaddr_cmp_addr(const sccp_sockaddr_t * a, const sccp_sockaddr_t * b);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Splits a string into its host and port components
	 *
	 * \param[in] str The string to parse. May be modified by writing a NUL at the end of
	 *                  the host part.
	 * \param[out] host Pointer to the host component within \a str.
	 * \param[out] port Pointer to the port component within \a str.
	 * \param flags     If set to zero, a port MAY be present. If set to PARSE_PORT_IGNORE, a
	 *                  port MAY be present but will be ignored. If set to PARSE_PORT_REQUIRE,
	 *                  a port MUST be present. If set to PARSE_PORT_FORBID, a port MUST NOT
	 *                  be present.
	 *
	 * \retval 1 Success
	 * \retval 0 Failure
	 */
	int sccp_sockaddr_split_hostport(char *str, char **host, char **port, int flags);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Parse an IPv4 or IPv6 address string.
	 *
	 * \details
	 * Parses a string containing an IPv4 or IPv6 address followed by an optional
	 * port (separated by a colon) into a sccp_sockaddr_t. The allowed formats
	 * are the following:
	 *
	 * a.b.c.d
	 * a.b.c.d:port
	 * a:b:c:...:d
	 * [a:b:c:...:d]
	 * [a:b:c:...:d]:port
	 *
	 * Host names are NOT allowed.
	 *
	 * \param[out] addr The resulting sccp_sockaddr. This MAY be NULL from 
	 * functions that are performing validity checks only, e.g. ast_parse_arg().
	 * \param str The string to parse
	 * \param flags If set to zero, a port MAY be present. If set to
	 * PARSE_PORT_IGNORE, a port MAY be present but will be ignored. If set to
	 * PARSE_PORT_REQUIRE, a port MUST be present. If set to PARSE_PORT_FORBID, a
	 * port MUST NOT be present.
	 *
	 * \retval 1 Success
	 * \retval 0 Failure
	 */
	int sccp_sockaddr_parse(sccp_sockaddr_t * addr, const char *str, int flags);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Parses a string with an IPv4 or IPv6 address and place results into an array
	 *
	 * \details
	 * Parses a string containing a host name or an IPv4 or IPv6 address followed
	 * by an optional port (separated by a colon).  The result is returned into a
	 * array of sccp_sockaddr_t. Allowed formats for str are the following:
	 *
	 * hostname:port
	 * host.example.com:port
	 * a.b.c.d
	 * a.b.c.d:port
	 * a:b:c:...:d
	 * [a:b:c:...:d]
	 * [a:b:c:...:d]:port
	 *
	 * \param[out] addrs The resulting array of sccp_sockaddrs
	 * \param str The string to parse
	 * \param flags If set to zero, a port MAY be present. If set to
	 * PARSE_PORT_IGNORE, a port MAY be present but will be ignored. If set to
	 * PARSE_PORT_REQUIRE, a port MUST be present. If set to PARSE_PORT_FORBID, a
	 * port MUST NOT be present.
	 *
	 * \param family Only addresses of the given family will be returned. Use 0 or
	 * AST_AF_UNSPEC to get addresses of all families.
	 *
	 * \retval 0 Failure
	 * \retval non-zero The number of elements in addrs array.
	 */
	int sccp_sockaddr_resolve(sccp_sockaddr_t ** addrs, const char *str, int flags, int family);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Get the port number of a socket address.
	 *
	 * \warning Do not use this function unless you really know what you are doing.
	 * And "I want the port number" is not knowing what you are doing.
	 *
	 * \retval 0 Address is null
	 * \retval non-zero The port number of the sccp_sockaddr
	 */
#define sccp_sockaddr_port(addr)	_sccp_sockaddr_port(addr, __FILE__, __LINE__, __PRETTY_FUNCTION__)
	uint16_t _sccp_sockaddr_port(const sccp_sockaddr_t * addr, const char *file, int line, const char *func);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Sets the port number of a socket address.
	 *
	 * \warning Do not use this function unless you really know what you are doing.
	 * And "I want the port number" is not knowing what you are doing.
	 *
	 * \param addr Address on which to set the port
	 * \param port The port you wish to set the address to use
	 * \retval void
	 */
#define sccp_sockaddr_set_port(addr,port)	_sccp_sockaddr_set_port(addr,port,__FILE__,__LINE__,__PRETTY_FUNCTION__)
	void _sccp_sockaddr_set_port(sccp_sockaddr_t * addr, uint16_t port, const char *file, int line, const char *func);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Get an IPv4 address of an sccp_sockaddr
	 *
	 * \warning You should rarely need this function. Only use if you know what you're doing.
	 * \return IPv4 address in network byte order
	 */
	uint32_t sccp_sockaddr_ipv4(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if the address is an IPv4 address
	 *
	 * \warning You should rarely need this function. Only use if you know what you're doing.
	 * \retval 1 This is an IPv4 address
	 * \retval 0 This is an IPv6 or IPv4-mapped IPv6 address
	 */
	int sccp_sockaddr_is_ipv4(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if this is an IPv4-mapped IPv6 address
	 *
	 * \warning You should rarely need this function. Only use if you know what you're doing.
	 *
	 * \retval 1 This is an IPv4-mapped IPv6 address.
	 * \retval 0 This is not an IPv4-mapped IPv6 address.
	 */
	int sccp_sockaddr_is_ipv4_mapped(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if an IPv4 address is a multicast address
	 *
	 * \param addr the address to check
	 *
	 * This function checks if an address is in the 224.0.0.0/4 network block.
	 *
	 * \return non-zero if this is a multicast address
	 */
	int sccp_sockaddr_is_ipv4_multicast(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if this is a link-local IPv6 address
	 *
	 * \warning You should rarely need this function. Only use if you know what you're doing.
	 *
	 * \retval 1 This is a link-local IPv6 address.
	 * \retval 0 This is link-local IPv6 address.
	 */
	int sccp_sockaddr_is_ipv6_link_local(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if this is an IPv6 address
	 *
	 * \warning You should rarely need this function. Only use if you know what you're doing.
	 *
	 * \retval 1 This is an IPv6 or IPv4-mapped IPv6 address.
	 * \retval 0 This is an IPv4 address.
	 */
	int sccp_sockaddr_is_ipv6(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Determine if the address type is unspecified, or "any" address.
	 *
	 * \details
	 * For IPv4, this would be the address 0.0.0.0, and for IPv6,
	 * this would be the address ::. The port number is ignored.
	 *
	 * \retval 1 This is an "any" address
	 * \retval 0 This is not an "any" address
	 */
	int sccp_sockaddr_is_any(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Computes a hash value from the address. The port is ignored.
	 *
	 * \retval 0 Unknown address family
	 * \retval other A 32-bit hash derived from the address
	 */
	int sccp_sockaddr_hash(const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around accept(2) that uses sccp_sockaddr_t.
	 *
	 * \details
	 * For parameter and return information, see the man page for
	 * accept(2).
	 */
	int sccp_socket_accept(int sockfd, sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around bind(2) that uses sccp_sockaddr_t.
	 *
	 * \details
	 * For parameter and return information, see the man page for
	 * bind(2).
	 */
	int sccp_socket_bind(int sockfd, const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around connect(2) that uses sccp_sockaddr_t.
	 *
	 * \details
	 * For parameter and return information, see the man page for
	 * connect(2).
	 */
	int sccp_socket_connect(int sockfd, const sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around getsockname(2) that uses sccp_sockaddr_t.
	 *
	 * \details
	 * For parameter and return information, see the man page for
	 * getsockname(2).
	 */
	int sccp_socket_getsockname(int sockfd, sccp_sockaddr_t * addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around recvfrom(2) that uses sccp_sockaddr_t.
	 *
	 * \details
	 * For parameter and return information, see the man page for
	 * recvfrom(2).
	 */
	ssize_t sccp_socket_recvfrom(int sockfd, void *buf, size_t len, int flags, sccp_sockaddr_t * src_addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Wrapper around sendto(2) that uses sccp_sockaddr.
	 *
	 * \details
	 * For parameter and
	 * return information, see the man page for sendto(2)
	 */
	ssize_t sccp_socket_sendto(int sockfd, const void *buf, size_t len, int flags, const sccp_sockaddr_t * dest_addr);

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Set type of service
	 *
	 * \details
	 * Set ToS ("Type of Service for IPv4 and "Traffic Class for IPv6) and
	 * CoS (Linux's SO_PRIORITY)
	 *
	 * \param sockfd File descriptor for socket on which to set the parameters
	 * \param tos The type of service for the socket
	 * \param cos The cost of service for the socket
	 * \param desc A text description of the socket in question.
	 * \retval 0 Success
	 * \retval -1 Error, with errno set to an appropriate value
	 */
	int sccp_socket_set_qos(int sockfd, int tos, int cos, const char *desc);

	/*!
	 * These are backward compatibility functions that may be used by subsystems
	 * that have not yet been converted to IPv6. They will be removed when all
	 * subsystems are IPv6-ready.
	 */
	/*@{ */

	/*!
	 * \since 4.1
	 *
	 * \brief
	 * Converts a sccp_sockaddr_t to a struct sockaddr_in.
	 *
	 * \param addr The sccp_sockaddr to convert
	 * \param[out] sin The resulting sockaddr_in struct
	 * \retval nonzero Success
	 * \retval zero Failure
	 */
#define sccp_sockaddr_to_sin(addr,sin)	_sccp_sockaddr_to_sin(addr,sin, __FILE__, __LINE__, __PRETTY_FUNCTION__)
	int _sccp_sockaddr_to_sin(const sccp_sockaddr_t * addr, struct sockaddr_in *sin, const char *file, int line, const char *func);

	/*!
	 * \since 4.1
	 *
	 * \brief Converts a struct sockaddr_in to a sccp_sockaddr_t.
	 *
	 * \param addr
	 * \param sin The sockaddr_in to convert
	 * \return an sccp_sockaddr structure
	 */
#define sccp_sockaddr_from_sin(addr,sin)	_sccp_sockaddr_from_sin(addr,sin, __FILE__, __LINE__, __PRETTY_FUNCTION__)
	void _sccp_sockaddr_from_sin(sccp_sockaddr_t * addr, const struct sockaddr_in *sin, const char *file, int line, const char *func);

	/*@} */

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif														/* _SCCP_SOCKADDR_H */
