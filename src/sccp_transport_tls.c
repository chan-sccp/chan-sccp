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

#ifdef HAVE_LIBSSL
#	include <openssl/err.h> /* for ERR_print_errors_fp */
#	include <openssl/ssl.h> /* for SSL_CTX_free, SSL_get_error, ... */
#	ifdef HAVE_CRYPTO
#		include <openssl/crypto.h> /* for OPENSSL_free */
#	endif
#	define PBX_CERTFILE           "asterisk.pem"                                        // move to config.h (copy from tcptls.h)
#	define REQUEST_RETRY_INTERVAL 5
#	define REQUEST_RETRY_COUNT    2
#	define DUPLICATE_INTERVAL     REQUEST_RETRY_INTERVAL * REQUEST_RETRY_COUNT

/* local variables */
static SSL_CTX * sslctx = NULL;

/* forward declares */
const sccp_transport_t tlstransport;

static void write_openssl_error_to_log(void)
{
	char * buffer = NULL;
	size_t length = 0;

	FILE * fp = open_memstream(&buffer, &length);
	if (!fp) {
		pbx_log(LOG_ERROR, "SCCP: error opening memstream for openssl_error_to_log\n");
		return;
	}

	ERR_print_errors_fp(fp);
	fclose(fp);

	if (length) {
		pbx_log(LOG_ERROR, "%.*s\n", (int)length, buffer);
	}

	ast_free(buffer);
}

static void InitializeSSL()
{
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
}

static void DestroySSL()
{
	ERR_free_strings();
	EVP_cleanup();
}

static void ShutdownSSL(SSL * ssl)
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

static SSL_CTX * create_context()
{
	sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport create context...\n");
	// const SSL_METHOD * method = TLS_server_method();
	const SSL_METHOD * method = SSLv23_method();
	SSL_CTX *          ctx    = SSL_CTX_new(method);
	if (!ctx) {
		pbx_log(LOG_WARNING, "Unable to create SSL context\n");
		write_openssl_error_to_log();
		return NULL;
	}
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2);

	return ctx;
}

static boolean_t configure_context(SSL_CTX * ctx)
{
	SSL_CTX_set_ecdh_auto(ctx, 1);
	sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport configure context...\n");

	char * cert_file = NULL;
	if (GLOB(cert_file)) {
		cert_file = ast_strdupa(GLOB(cert_file));
	} else {
		cert_file = ast_strdupa(PBX_CERTFILE);
	}

	if (access(cert_file, F_OK) != 0) {
		sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "TLS/SSL no certificate file not found\n");
		return FALSE;
	} else {
		if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
			pbx_log(LOG_WARNING, "TLS/SSL error loading public key (certificate) from <%s>.\n", cert_file);
			write_openssl_error_to_log();
			return FALSE;
		} else if (SSL_CTX_use_PrivateKey_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
			ast_log(LOG_WARNING, "TLS/SSL error loading private key from <%s>.\n", cert_file);
			write_openssl_error_to_log();
			return FALSE;
		} else if (SSL_CTX_check_private_key(ctx) == 0) {
			ast_log(LOG_WARNING, "TLS/SSL error matching private key and certificate in <%s>.\n", cert_file);
			write_openssl_error_to_log();
			return FALSE;
		}
	}

	return TRUE;
}
const sccp_transport_t * const tls_init(void)
{
	sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport Initializing...\n");
	sslctx = create_context();
	if (sslctx && configure_context(sslctx)) {
		InitializeSSL();
		return &tlstransport;
	}
	return NULL;
}

static int tls_bind(sccp_socket_connection_t * sc, struct sockaddr * addr, socklen_t addrlen)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport bind...\n");
	return bind(sc->fd, addr, addrlen);
}

static int tls_listen(sccp_socket_connection_t * sc, int backlog)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport listen...\n");
	return listen(sc->fd, backlog);
}

static sccp_socket_connection_t * tls_accept(sccp_socket_connection_t * in_sc, struct sockaddr * addr, socklen_t * addrlen, sccp_socket_connection_t * out_sc)
{
	unsigned long ssl_err;
	int           newfd = 0;
	SSL *         ssl   = NULL;
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport accept...\n");
	do {
		newfd = accept(in_sc->fd, addr, addrlen);
		if (newfd < 0) {
			pbx_log(LOG_ERROR, "Error accepting new socket %s on fd:%d\n", strerror(errno), in_sc->fd);
			break;
		}

		ssl = SSL_new(sslctx);
		if (!ssl) {
			pbx_log(LOG_ERROR, "Error creating new SSL structure\n");
			break;
		}

		SSL_set_fd(ssl, newfd);
		ssl_err = SSL_accept(ssl);
		if (ssl_err <= 0) {
			pbx_log(LOG_ERROR, "SSL Error occured: %lu '%s'.\n", ssl_err, ERR_reason_error_string(ssl_err));
			break;
		}
		out_sc->fd  = newfd;
		out_sc->ssl = ssl;
		sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport accept returning:%d...\n", newfd);
		return out_sc;
	} while (0);

	// cleanup:
	if (ssl) {
		write_openssl_error_to_log();
		ShutdownSSL(ssl);
	}
	if (newfd >= 0) {
		close(newfd);
	}
	return NULL;
}

static int tls_recv(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport recv...\n");
	return SSL_read(sc->ssl, buf, buflen);
}

static int tls_send(sccp_socket_connection_t * sc, void * buf, size_t buflen, int flags)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport send...\n");
	return SSL_write(sc->ssl, buf, buflen);
}

static int tls_shutdown(sccp_socket_connection_t * sc, int how)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport shutdown...\n");
	SSL_shutdown(sc->ssl);
	return shutdown(sc->fd, how);
}

static int tls_close(sccp_socket_connection_t * sc)
{
	// sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport close...\n");
	int res = 0;
	if (sc->fd) {
		res = close(sc->fd);
	}
	if (sc->ssl) {
		SSL_free(sc->ssl);
	}
	return res;
}

static const sccp_transport_t * const tls_destroy(uint8_t h)
{
	sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_1 "TLS Transport destroy...\n");
	DestroySSL();
	return NULL;
}

const sccp_transport_t tlstransport = {
	.name           = "TLS",
	.secret_default = "",
	.socktype       = SOCK_STREAM,
	.port_default   = "2443",

	.retrycountdefault        = 0,
	.retrycountmax            = 0,
	.retryintervaldefault     = REQUEST_RETRY_INTERVAL * REQUEST_RETRY_COUNT,
	.retryintervalmax         = 60,
	.duplicateintervaldefault = DUPLICATE_INTERVAL,

	.init     = tls_init,
	.bind     = tls_bind,
	.listen   = tls_listen,
	.accept   = tls_accept,
	.recv     = tls_recv,
	.send     = tls_send,
	.shutdown = tls_shutdown,
	.close    = tls_close,
	.destroy  = tls_destroy,
};
#endif /* HAVE_LIBSSL */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
