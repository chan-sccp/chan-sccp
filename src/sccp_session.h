/*!
 * \file        sccp_session.h
 * \brief       SCCP Session Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#include "sccp_cli.h"
#include "sccp_transport.h"
//#include "sccp_protocol.h"
struct sccp_session;

__BEGIN_C_EXTERN__
typedef struct sccp_servercontext sccp_servercontext_t;
typedef enum {
	SCCP_SERVERCONTEXT_TCP = 0,
#ifdef HAVE_OPENSSL
	SCCP_SERVERCONTEXT_TLS = 1,
#endif
} sccp_servercontexttype_t;

SCCP_API sccp_servercontext_t * SCCP_CALL sccp_servercontext_create(struct sockaddr_storage * bindaddr, sccp_servercontexttype_t type);
SCCP_API int SCCP_CALL sccp_servercontext_stopListening(sccp_servercontext_t * context);
SCCP_API int SCCP_CALL sccp_servercontext_destroy(sccp_servercontext_t * context);
SCCP_API int SCCP_CALL sccp_servercontext_reload(sccp_servercontext_t * context, struct sockaddr_storage * bindaddr);
SCCP_API int SCCP_CALL sccp_session_getFD(sccp_session_t * s);
SCCP_API void SCCP_CALL sccp_session_setFD(sccp_session_t * s, int fd);
SCCP_API ssl_t * SCCP_CALL sccp_session_getSSL(sccp_session_t * s);
SCCP_API void SCCP_CALL sccp_session_setSSL(sccp_session_t * s, ssl_t * ssl);
SCCP_API void SCCP_CALL sccp_session_terminateAll(void);
SCCP_API const char * const SCCP_CALL sccp_session_getDesignator(constSessionPtr session);
SCCP_API int SCCP_CALL sccp_session_waitForPendingRequests(sccp_session_t * s);
uint16_t sccp_session_getPendingRequests(sccp_session_t * s);
SCCP_API void SCCP_CALL sccp_session_sendmsg(constDevicePtr device, sccp_mid_t t);
SCCP_API int SCCP_CALL sccp_session_send(constDevicePtr device, const sccp_msg_t * msg_in);
SCCP_API int SCCP_CALL sccp_session_send2(constSessionPtr session, sccp_msg_t * msg);
SCCP_API int SCCP_CALL sccp_session_retainDevice(constSessionPtr session, constDevicePtr device);
SCCP_API void SCCP_CALL sccp_session_releaseDevice(constSessionPtr volatile session);
SCCP_API sccp_session_t * SCCP_CALL sccp_session_reject(constSessionPtr session, char *message);
SCCP_API void SCCP_CALL sccp_session_tokenReject(constSessionPtr session, uint32_t backoff_time);
SCCP_API void SCCP_CALL sccp_session_tokenAck(constSessionPtr session);
SCCP_API void SCCP_CALL sccp_session_tokenRejectSPCP(constSessionPtr session, uint32_t features);
SCCP_API void SCCP_CALL sccp_session_tokenAckSPCP(constSessionPtr session, uint32_t features);
SCCP_INLINE void sccp_session_stopthread(constSessionPtr session, skinny_registrationstate_t newRegistrationState);
SCCP_API void SCCP_CALL sccp_session_setProtocol(constSessionPtr session, uint16_t protocolType);
SCCP_API uint16_t SCCP_CALL sccp_session_getProtocol(constSessionPtr session);
SCCP_API boolean_t SCCP_CALL sccp_session_getOurIP(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage, int family);
SCCP_API boolean_t SCCP_CALL sccp_session_getSas(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage);
SCCP_INLINE int SCCP_CALL sccp_session_getClientPort(constSessionPtr session);
SCCP_API int SCCP_CALL sccp_session_setOurIP4Address(constSessionPtr session, const struct sockaddr_storage * them);
SCCP_API void SCCP_CALL sccp_session_resetLastKeepAlive(constSessionPtr session);
void sccp_session_crossdevice_cleanup(constSessionPtr current_session, sessionPtr previous_session);
SCCP_API boolean_t SCCP_CALL sccp_session_check_crossdevice(constSessionPtr session, constDevicePtr device);
SCCP_API devicePtr SCCP_CALL sccp_session_getDevice(constSessionPtr session, boolean_t required);
SCCP_API boolean_t SCCP_CALL sccp_session_isValid(constSessionPtr session);
SCCP_API int SCCP_CALL sccp_cli_show_sessions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);

SCCP_API boolean_t SCCP_CALL sccp_session_bind_and_listen(sccp_servercontext_t * context, struct sockaddr_storage * bindaddr);
SCCP_API void SCCP_CALL sccp_session_stop_accept_thread(sccp_servercontext_t * context);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
