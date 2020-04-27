/*!
 * \file	sccp_session.c
 * \brief       SCCP Session Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *		The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *		Modified by Jan Czmok and Julien Goodwin
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_transport.h"

#define REQUEST_RETRY_INTERVAL 5
#define REQUEST_RETRY_COUNT    2
#define DUPLICATE_INTERVAL     REQUEST_RETRY_INTERVAL * REQUEST_RETRY_COUNT

/* local variables */

/* forward declares */
const sccp_transport_t tcptransport;

#if 1 /*CS_SCCP_TCP*/
const sccp_transport_t * const tcp_init(void)
{
	return &tcptransport;
}

static int tcp_bind(sccp_socket_connection_t * sc, struct sockaddr * addr, socklen_t addrlen)
{
	return bind(sc->fd, addr, addrlen);
}

static int tcp_listen(sccp_socket_connection_t * sc, int backlog)
{
	return listen(sc->fd, backlog);
}

static sccp_socket_connection_t * tcp_accept(sccp_socket_connection_t * in_sc, struct sockaddr * addr, socklen_t * addrlen, sccp_socket_connection_t * out_sc)
{
	out_sc->fd = accept(in_sc->fd, addr, addrlen);
	return out_sc;
}

static int tcp_recv(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags)
{
	return recv(sc->fd, buf, buflen, flags);
}

static int tcp_send(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags)
{
	return send(sc->fd, buf, buflen, flags);
}

static int tcp_shutdown(sccp_socket_connection_t * sc, int how)
{
	return shutdown(sc->fd, how);
}

static int tcp_close(sccp_socket_connection_t * sc)
{
	return close(sc->fd);
}

static const sccp_transport_t * const tcp_destroy(uint8_t h)
{
	return NULL;
}

const sccp_transport_t tcptransport = {
	.name = "TCP",
	.secret_default = "",
	.socktype = SOCK_STREAM,
	.port_default = "2000",

	.retrycountdefault = 0,
	.retrycountmax = 0,
	.retryintervaldefault = REQUEST_RETRY_INTERVAL * REQUEST_RETRY_COUNT,
	.retryintervalmax = 60,
	.duplicateintervaldefault = DUPLICATE_INTERVAL,

	.init = tcp_init,
	.bind = tcp_bind,
	.listen = tcp_listen,
	.accept = tcp_accept,
	.recv = tcp_recv,
	.send = tcp_send,
	.shutdown = tcp_shutdown,
	.close = tcp_close,
	.destroy = tcp_destroy,
};

#else
const sccp_transport_t * const tcp_init(uint8_t h)
{
	return NULL;
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
