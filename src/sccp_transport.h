/*!
 * \file        sccp_transport.h
 * \brief       SCCP Transport Header
 * \author      Diederik de Groot <dkgroot [at] talon.nl>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

__BEGIN_C_EXTERN__

typedef struct sccp_transport sccp_transport_t;
typedef const sccp_transport_t * const constTransportPtr;

typedef struct ssl_st ssl_t;

typedef struct sccp_socket_connection {
	int fd;
	ssl_t * ssl;
} sccp_socket_connection_t;

struct sccp_transport {
	char * name;
	char * secret_default;
	int socktype;
	char * port_default;

	uint8_t retrycountdefault;
	uint8_t retrycountmax;
	uint8_t retryintervaldefault;
	uint8_t retryintervalmax;
	uint8_t duplicateintervaldefault;

	const sccp_transport_t * const (*const init)(void);
	int (*const bind)(sccp_socket_connection_t * sc, struct sockaddr * addr, socklen_t addrlen);
	int (*const listen)(sccp_socket_connection_t * sc, int backlog);
	sccp_socket_connection_t * (*const accept)(sccp_socket_connection_t * in_sc, struct sockaddr *, socklen_t * len, sccp_socket_connection_t * out_sc);
	int (*const recv)(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags);
	// int (*const recv_timeout)(int fd, void *buf, size_t buflen, int flags, int secs);
	int (*const send)(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags);
	// int (*const send_timeout)(int fd, void *buf, size_t buflen, int flags, int secs);
	int (*const shutdown)(sccp_socket_connection_t * sc, int how);
	int (*const close)(sccp_socket_connection_t * sc);
	const sccp_transport_t * const (*const destroy)(uint8_t h);
};

const sccp_transport_t * const tcp_init(void);
#ifdef HAVE_LIBSSL
const sccp_transport_t * const tls_init(void);
#endif

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
