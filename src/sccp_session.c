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
#include "sccp_session.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_actions.h"
#include "sccp_cli.h"
#include "sccp_device.h"
#include "sccp_netsock.h"
#include "sccp_utils.h"
#include "sccp_transport.h"
#include <netinet/in.h>
#include <sys/un.h>

#ifndef CS_USE_POLL_COMPAT
#include <poll.h>
#include <sys/poll.h>
#else
#define AST_POLL_COMPAT 1
#include <asterisk/poll-compat.h>
#endif
#ifdef pbx_poll
#define sccp_netsock_poll pbx_poll
#else
#define sccp_netsock_poll poll
#endif
#ifdef HAVE_PBX_ACL_H				// AST_SENSE_ALLOW
#  include <asterisk/acl.h>
#endif
#include <asterisk/cli.h>
#include <signal.h>

/* global variables -> GLOBALS */
// static pthread_t accept_tid;
// static int accept_sock = -1;

#define WRITE_BACKOFF 500											/* backoff time in millisecs, doubled every write retry (150+300+600+1200+2400+4800 = 9450 millisecs = 9.5 sec) */
#define SESSION_DEVICE_CLEANUP_TIME 10										/* wait time before destroying a device on thread exit */
#define KEEPALIVE_ADDITIONAL_PERCENT_SESSION 1.05								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */
#define KEEPALIVE_ADDITIONAL_PERCENT_DEVICE 1.20								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */
#define KEEPALIVE_ADDITIONAL_PERCENT_ON_CALL 2.00								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */
#define SESSION_REQUEST_TIMEOUT              5

/* Lock Macro for Sessions */
#define sccp_session_lock(x)			pbx_mutex_lock(&(x)->lock)
#define sccp_session_unlock(x)			pbx_mutex_unlock(&(x)->lock)
#define sccp_session_trylock(x)			pbx_mutex_trylock(&(x)->lock)
#define SCOPED_SESSION(x)                       SCOPED_MUTEX(sessionlock, (ast_mutex_t *)&(x)->lock);
/* */

void sccp_session_device_thread_exit(void *session);
void *sccp_session_device_thread(void *session);
void __sccp_session_stopthread(sessionPtr session, skinny_registrationstate_t newRegistrationState);
gcc_inline void recalc_wait_time(sccp_session_t *s);
static struct ast_sockaddr internip;

struct sccp_servercontext {
	sccp_servercontexttype_t type;
	const sccp_transport_t * transport;
	struct sockaddr_storage boundaddr;
	pthread_t accept_tid;
	sccp_socket_connection_t sc;
	boolean_t (*bind_and_listen)(sccp_servercontext_t * context, struct sockaddr_storage * bindaddr);
	int (*stopListening)(sccp_servercontext_t * context);
};

sccp_servercontext_t * sccp_servercontext_create(struct sockaddr_storage * bindaddr, sccp_servercontexttype_t type)
{
	sccp_servercontext_t * context = NULL;
	if(!(context = (sccp_servercontext_t *)sccp_calloc(sizeof *context, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}
	context->type = type;
	switch(type) {
		case SCCP_SERVERCONTEXT_TCP:
			if((context->transport = tcp_init()) == NULL) {
				pbx_log(LOG_ERROR, "SCCP: (%s) could not initialize tcp context\n", __func__);
				sccp_free(context);
				return NULL;
			}
			break;
#ifdef HAVE_LIBSSL
		case SCCP_SERVERCONTEXT_TLS:
			if((context->transport = tls_init()) == NULL) {
				// pbx_log(LOG_NOTICE, "SCCP: (%s) could not initialize tls context\n", __func__);
				sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_2 "SCCP: (%s) could not initialize tls context\n", __func__);
				sccp_free(context);
				return NULL;
			}
			break;
#endif
	}
	context->bind_and_listen = sccp_session_bind_and_listen;
	context->stopListening = sccp_servercontext_stopListening;
	context->sc.fd = -1;
	context->accept_tid = AST_PTHREADT_NULL;
	return sccp_servercontext_reload(context, bindaddr) ? context : NULL;
}

int sccp_servercontext_stopListening(sccp_servercontext_t * context)
{
	if(context) {
		sccp_session_stop_accept_thread(context);
		return 0;
	}
	return 1;
}

int sccp_servercontext_destroy(sccp_servercontext_t * context)
{
	if(context) {
		sccp_session_stop_accept_thread(context);
		context->transport->destroy(1);
		sccp_free(context);
		return 0;
	}
	return 1;
}

int sccp_servercontext_reload(sccp_servercontext_t * context, struct sockaddr_storage * bindaddr)
{
	if(context->sc.fd > -1 && (sccp_netsock_getPort(&context->boundaddr) != sccp_netsock_getPort(bindaddr) || sccp_netsock_cmp_addr(&context->boundaddr, bindaddr))) {
		sccp_session_stop_accept_thread(context);
	}
	return context->bind_and_listen(context, bindaddr);
}

const struct sockaddr_storage * const sccp_servercontext_getBoundAddr(sccp_servercontext_t * context)
{
	return context ? &context->boundaddr : NULL;
}

/*!
 * \brief SCCP Session Structure
 * \note This contains the current session the phone is in
 */
struct sccp_session {
	sccp_servercontext_t * srvcontext;
	time_t lastKeepAlive;											/*!< Last KeepAlive Time */
	uint16_t keepAlive;
	uint16_t keepAliveInterval;
	SCCP_RWLIST_ENTRY (sccp_session_t) list;								/*!< Linked List Entry for this Session */
	sccp_device_t *device;											/*!< Associated Device */
	sccp_socket_connection_t sc;                                                                            /*!< session filedescription (and tls connection) */
	struct sockaddr_storage sin;										/*!< Incoming Socket Address */
	uint32_t protocolType;
	volatile boolean_t session_stop;									/*!< Signal Session Stop */
	sccp_mutex_t write_lock;										/*!< Prevent multiple threads writing to the socket at the same time */
	sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */
	pthread_t session_thread;										/*!< Session Thread */
	struct sockaddr_storage ourip;										/*!< Our IP is for rtp use */
	struct sockaddr_storage ourIPv4;
	char designator[40];
	uint16_t requestsInFlight;
	pbx_cond_t pendingRequest;
};														/*!< SCCP Session Structure */

int sccp_session_getFD(sccp_session_t * s)
{
	// SCOPED_MUTEX
	sccp_session_lock(s);
	int res = s->sc.fd;
	sccp_session_unlock(s);
	return res;
}

void sccp_session_setFD(sccp_session_t * s, int fd)
{
	sccp_session_lock(s);
	if(s->sc.fd > 0) {
		s->srvcontext->transport->shutdown(&s->sc, SHUT_RDWR);
		s->srvcontext->transport->close(&s->sc);
		s->sc.fd = -1;
	}
	s->sc.fd = fd;
	sccp_session_unlock(s);
}

ssl_t * sccp_session_getSSL(sccp_session_t * s)
{
	ssl_t * res = NULL;
	sccp_session_lock(s);
	res = s->sc.ssl;
	sccp_session_unlock(s);
	return res;
}

void sccp_session_setSSL(sccp_session_t * s, ssl_t * ssl)
{
	sccp_session_lock(s);
	s->sc.ssl = ssl;
	sccp_session_unlock(s);
}

boolean_t sccp_session_getOurIP(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage, int family)
{
	if (session && sockAddrStorage) {
		if (!sccp_netsock_is_any_addr(&session->ourip)) {
			switch (family) {
				case 0:
					memcpy(sockAddrStorage, &session->ourip, sizeof(struct sockaddr_storage));
					break;
				case AF_INET:
					((struct sockaddr_in *) sockAddrStorage)->sin_addr = ((struct sockaddr_in *) &session->ourip)->sin_addr;
					break;
				case AF_INET6:
					((struct sockaddr_in6 *) sockAddrStorage)->sin6_addr = ((struct sockaddr_in6 *) &session->ourip)->sin6_addr;
					break;
			}
			return TRUE;
		}
	}
	return FALSE;
}

boolean_t sccp_session_getSas(constSessionPtr session, struct sockaddr_storage * const sockAddrStorage)
{
	if (session && sockAddrStorage) {
		memcpy(sockAddrStorage, &session->sin, sizeof(struct sockaddr_storage));
		return TRUE;
	}
	return FALSE;
}

gcc_inline int sccp_session_getClientPort(constSessionPtr session)
{
	if(session) {
		return sccp_netsock_getPort(&session->sin);
	}
	return 0;
}

/*!
 * \brief Exchange Socket Addres Information from them to us
 */
int sccp_session_setOurIP4Address(constSessionPtr session, const struct sockaddr_storage * them)
{
	sessionPtr s = (sessionPtr)session;                                        // discard const
	struct sockaddr_storage us = { 0 };
	sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "SCCP: (setOurIP4Address) client %s\n", sccp_netsock_stringify(them));

	// starting guess for the internal address
	memcpy(&us, &internip.ss, sizeof(struct sockaddr_storage));

	// now ask the system what would it use to talk to 'them'
	if(s && sccp_netsock_ouraddrfor(them, &us)) {
		memcpy(&s->ourIPv4, &us, sizeof(struct sockaddr_storage));
		sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "SCCP: (setOurIP4Address) can be reached best via %s\n", sccp_netsock_stringify(&s->ourIPv4));
		return 0;
	}
	return -2;
}

int sccp_session_waitForPendingRequests(sccp_session_t * s)
{
	struct timeval relative_timeout = {
		SESSION_REQUEST_TIMEOUT,
	};
	struct timeval absolute_timeout = ast_tvadd(ast_tvnow(), relative_timeout);
	struct timespec timeout_spec = {
		.tv_sec = absolute_timeout.tv_sec,
		.tv_nsec = absolute_timeout.tv_usec * 1000,
	};

	SCOPED_SESSION(s);
	while(s->requestsInFlight) {
		sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: Waiting for %d Pending Requests!\n", s->designator, s->requestsInFlight);
		if(pbx_cond_timedwait(&s->pendingRequest, &s->lock, &timeout_spec) == ETIMEDOUT) {
			pbx_log(LOG_WARNING, "%s: waitForPendingRequests timed out!\n", s->designator);
			s->requestsInFlight = 0;
			return s->requestsInFlight;
		}
	}
	return 0;
}

uint16_t sccp_session_getPendingRequests(sccp_session_t * s)
{
	SCOPED_SESSION(s);
	return s->requestsInFlight;
}

static void request_pending(sccp_session_t * s)
{
	SCOPED_SESSION(s);
	s->requestsInFlight++;
}

static void response_received(sccp_session_t * s)
{
	SCOPED_SESSION(s);
	if(s->requestsInFlight) {
		s->requestsInFlight--;
	}
	pbx_cond_broadcast(&s->pendingRequest);
}

static void socket_get_error(constSessionPtr s, const char * file, int line, const char * function)
{
	if (errno) {
		if (errno == ECONNRESET) {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: Connection reset by peer\n", DEV_ID_LOG(s->device));
		} else {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s (%s:%d:%s) Socket returned error: '%s (%d)')\n", DEV_ID_LOG(s->device), file, line, function, strerror(errno), errno);
		}
	} else {
		if(!s || s->sc.fd <= 0) {
			return;
		}
		int mysocket = s->sc.fd;
		int error = 0;
		socklen_t error_len = sizeof(error);
		if ((mysocket && getsockopt(mysocket, SOL_SOCKET, SO_ERROR, &error, &error_len) == 0) && error != 0) {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: (%s:%d:%s) SO_ERROR: %s (%d)\n", DEV_ID_LOG(s->device), file, line, function, strerror(error), error);
		}
	}
}

static int session_dissect_header(sccp_session_t * s, sccp_header_t * header, struct messageinfo ** msgInfoPtr)
{
	int result = -1;
	struct messageinfo * msginfo = *msgInfoPtr;
	unsigned int packetSize = header->length = letohl(header->length);
	int protocolVersion = letohl(header->lel_protocolVer);
	sccp_mid_t messageId = letohl(header->lel_messageId);
	do {
		// dissecting header to see if we have a valid sccp message, that we can handle
		if (packetSize < 4 || packetSize > SCCP_MAX_PACKET - 8) {
			pbx_log(LOG_ERROR, "%s: (session_dissect_header) Size of the data payload in the packet (messageId: %u, protocolVersion: %u / 0x0%x) is out of bounds: %d < %u > %d, close connection !\n", DEV_ID_LOG(s->device), messageId, protocolVersion, protocolVersion, 4, packetSize, (int) (SCCP_MAX_PACKET - 8));
			return -2;
		}

		if (protocolVersion > 0 && !(sccp_protocol_isProtocolSupported(s->protocolType, protocolVersion))) {
			pbx_log(LOG_ERROR, "%s: (session_dissect_header) protocolversion %u is unknown, cancelling read.\n", DEV_ID_LOG(s->device), protocolVersion);
			break;
		}

		if((msginfo = lookupMsgInfoStruct(messageId))) {
			if(msginfo->messageId != messageId) {
				pbx_log(LOG_ERROR, "%s: (session_dissect_header) messageId %d (0x%x) unknown. matched:0x%x discarding message.\n", DEV_ID_LOG(s->device), messageId, messageId, msginfo->messageId);
				break;
			}
			result = msginfo->size + SCCP_PACKET_HEADER;
			*msgInfoPtr = msginfo;
		}
	} while (0);

	return result;
}

static gcc_inline int session_buffer2msg(sccp_session_t * s, const unsigned char * const buffer, int lenAccordingToPacketHeader, sccp_msg_t * msg)
{
	int res = -5;
	sccp_header_t msg_header = {0};
	struct messageinfo * msginfo = NULL;
	memcpy(&msg_header, buffer, SCCP_PACKET_HEADER);

	// dissect the message header
	int lenAccordingToOurProtocolSpec = session_dissect_header(s, &msg_header, &msginfo);
	if (dont_expect(lenAccordingToOurProtocolSpec < 0)) {
		if (lenAccordingToOurProtocolSpec == -2) {
			return 0;
		}
		lenAccordingToOurProtocolSpec = 0;									// unknown message, read it and discard content completely
	}
	if (dont_expect(lenAccordingToPacketHeader > lenAccordingToOurProtocolSpec)) {					// show out discarded bytes
		pbx_log(LOG_WARNING, "%s: (session_dissect_msg) Incoming message is bigger(%d) than known size(%d). Packet looks like!\n", DEV_ID_LOG(s->device), lenAccordingToPacketHeader, lenAccordingToOurProtocolSpec);
		// buffer[lenAccordingToPacketHeader + 1] = '\0';								// terminate buffer
		sccp_dump_packet(buffer, lenAccordingToPacketHeader);
	}
	
	if (((unsigned int)lenAccordingToPacketHeader) < ((unsigned int)lenAccordingToOurProtocolSpec)){
		sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: (session_dissect_msg) Incoming message is smaller(%d) than known size(%d).\n", DEV_ID_LOG(s->device), lenAccordingToPacketHeader, lenAccordingToOurProtocolSpec);
		lenAccordingToOurProtocolSpec = lenAccordingToPacketHeader;
	}

	memset(msg, 0, SCCP_MAX_PACKET);
	memcpy(msg, buffer, lenAccordingToOurProtocolSpec);
	msg->header.length = lenAccordingToOurProtocolSpec;								// patch up msg->header.length to new size

	// handle the message
	res = sccp_handle_message(msg, s);

	// check response after handling message
	if(msginfo && msginfo->type == SKINNY_MSGTYPE_RESPONSE) {
		response_received(s);
		sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: Response '%s' Received\n", DEV_ID_LOG(s->device), msginfo->text);
	}
	return res;
}

static gcc_inline int process_buffer(sccp_session_t * s, sccp_msg_t * msg, unsigned char * const buffer, size_t * len)
{
	int res = 0;
	while (*len >= SCCP_PACKET_HEADER && *len <= SCCP_MAX_PACKET * 2) {										// We have at least SCCP_PACKET_HEADER, so we have the payload length
		uint32_t header_len;
		memcpy(&header_len, buffer, 4);
		uint32_t payload_len = letohl(header_len) + (SCCP_PACKET_HEADER - 4);
		if (*len < payload_len) {
			break;												// Too short - haven't received whole payload yet, go poll for more
		}

		if (dont_expect(payload_len < SCCP_PACKET_HEADER || payload_len > SCCP_MAX_PACKET)) {
			pbx_log(LOG_ERROR, "%s: (process_buffer) Size of the data payload in the packet is bigger than max packet, close connection !\n", DEV_ID_LOG(s->device));
			res = -1;
			break;
		}
		if (dont_expect(session_buffer2msg(s, buffer, payload_len, msg) != 0)) {
			res = -2;
			break;
		}

		*len -= payload_len;
		if (*len > 0) {												// Now shuffle the remaining data in the buffer back to the start
			memmove(buffer + 0, buffer + payload_len, *len);
		}
	}
	return res;
}

/*!
 * \brief Find Session in Globals Lists
 * \param s SCCP Session
 * \return boolean
 *
 * \lock
 *      - session
 */
static boolean_t sccp_session_findBySession(sccp_session_t * s)
{
	sccp_session_t * session = NULL;
	boolean_t res = FALSE;

	SCCP_RWLIST_RDLOCK(&GLOB(sessions));
	SCCP_RWLIST_TRAVERSE(&GLOB(sessions), session, list) {
		if (session == s) {
			res = TRUE;
			break;
		}
	}
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));
	return res;
}

/*!
 * \brief Add a session to the global sccp_sessions list
 * \param s SCCP Session
 * \return boolean
 *
 * \lock
 *      - session
 */
static boolean_t sccp_session_addToGlobals(sccp_session_t * s)
{
	boolean_t res = FALSE;

	if (s) {
		if (!sccp_session_findBySession(s)) {;
			SCCP_RWLIST_WRLOCK(&GLOB(sessions));
			SCCP_LIST_INSERT_HEAD(&GLOB(sessions), s, list);
			res = TRUE;
			SCCP_RWLIST_UNLOCK(&GLOB(sessions));
		}
	}
	return res;
}

/*!
 * \brief Removes a session from the global sccp_sessions list
 * \param s SCCP Session
 * \return boolean
 *
 * \lock
 *      - sessions
 */
static boolean_t sccp_session_removeFromGlobals(sccp_session_t * s)
{
	sccp_session_t * session = NULL;
	boolean_t res = FALSE;

	if (s) {
		SCCP_RWLIST_WRLOCK(&GLOB(sessions));
		SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), session, list) {
			if (session == s) {
				SCCP_LIST_REMOVE_CURRENT(list);
				res = TRUE;
				break;
			}
		}
		SCCP_RWLIST_TRAVERSE_SAFE_END;
		SCCP_RWLIST_UNLOCK(&GLOB(sessions));
	}
	return res;
}


/*!
 * \brief Terminate all session
 *
 * \lock
 *      - socket_lock
 *      - Glob(sessions)
 */
void sccp_session_terminateAll()
{
	sccp_session_t *s = NULL;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Sessions\n");
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), s, list) {
		sccp_session_stopthread(s, SKINNY_DEVICE_RS_NONE);
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	
	/* give remote phone a time to close the socket */
	int waitloop = 10;
	while (!SCCP_LIST_EMPTY(&GLOB(sessions)) && waitloop-- > 0) {
		usleep(100);
	}

	if (SCCP_LIST_EMPTY(&GLOB(sessions))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(sessions));
	}
}

/*!
 * \brief Release device pointer from session
 * \param session SCCP Session
 */
static sccp_device_t *__sccp_session_removeDevice(sessionPtr session)
{
	sccp_device_t *return_device = NULL;

	if (session && (return_device = session->device)) {
		if (return_device->session && return_device->session != session) {
			sccp_session_removeFromGlobals(return_device->session);
		}
		sccp_device_setRegistrationState(return_device, SKINNY_DEVICE_RS_NONE);
	}
	sccp_session_lock(session);
	sccp_copy_string(session->designator, sccp_netsock_stringify(&session->ourip), sizeof(session->designator));
	session->device = NULL;
	sccp_session_unlock(session);
	return return_device;
}

/*!
 * \brief Retain device pointer in session. Replace existing pointer if necessary
 * \param session SCCP Session
 * \param device SCCP Device
 * \returns -1 when error happend, 0 if no new ref was taken and 1 if new device ref
 */
static int __sccp_session_addDevice(sessionPtr session, constDevicePtr device)
{
	int res = 0;
	sccp_device_t *new_device = NULL;
	if (session && (!device || (device && session->device != device))) {
		sccp_session_lock(session);
		new_device = sccp_device_retain(device);				/* do this before releasing anything, to prevent device cleanup if the same */
		if (session->device) {
			AUTO_RELEASE(sccp_device_t, remDevice, __sccp_session_removeDevice(session)); /* implicit release */
		}
		if (device) {
			if (new_device) {
				session->device = new_device;				/* keep newly retained device */
				session->device->session = session;			/* update device session pointer */

				char buf[16] = "";
				snprintf(buf, 16, "%s:%d", device->id, session->sc.fd);
				sccp_copy_string(session->designator, buf, sizeof(session->designator));
				res = 1;
			} else {
				res = -1;
			}
		}
		sccp_session_unlock(session);
	}
	return res;
}

/*!
 * \brief Retain device pointer in session. Replace existing pointer if necessary (ConstWrapper)
 * \param session SCCP Session
 * \param device SCCP Device
 */
int sccp_session_retainDevice(constSessionPtr session, constDevicePtr device)
{
	if (session && (!device || (device && session->device != device))) {
		sessionPtr s = (sessionPtr)session;									/* discard const */
		sccp_log((DEBUGCAT_DEVICE))(VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", DEV_ID_LOG(device), s->sc.fd, sccp_netsock_stringify_addr(&s->sin));
		return __sccp_session_addDevice(s, device);
	}
	return 0;
}


void sccp_session_releaseDevice(constSessionPtr volatile session)
{
	sessionPtr s = (sessionPtr)session;										/* discard const */
	if (s) {
		AUTO_RELEASE(sccp_device_t, device, __sccp_session_removeDevice(s));                                        // implicit release
	}
}

/*!
 * \brief Destroy Socket Session
 * \param s SCCP Session
 * \param cleanupTime Cleanup Time as uint8_t, Max time before device cleanup starts
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *      - sessions
 *      - device
 */
static void destroy_session(sccp_session_t * s)
{
	if (!s) {
		return;
	}

	char addrStr[INET6_ADDRSTRLEN];
	sccp_copy_string(addrStr, sccp_netsock_stringify_addr(&s->sin), sizeof(addrStr));
	AUTO_RELEASE(sccp_device_t, d , s->device ? sccp_device_retain(s->device) : NULL);
	if (d) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Destroy Device Session %s\n", DEV_ID_LOG(s->device), addrStr);
		d->session = NULL;
		sccp_dev_clean(d, (d->realtime) ? TRUE : FALSE);
	}
	sccp_session_releaseDevice(s);

	if (!sccp_session_removeFromGlobals(s)) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session could not be found in GLOB(session) %s\n", DEV_ID_LOG(s->device), addrStr);
	}
	
	if (s) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Destroy Session %s\n", addrStr);
		/* closing fd's */
		sccp_session_lock(s);
		if(s->sc.fd > 0) {
			sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: Shutdown socket %d\n", s->sc.fd);
			s->srvcontext->transport->shutdown(&s->sc, SHUT_RDWR);
			sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: Closing socket %d\n", s->sc.fd);
			s->srvcontext->transport->close(&s->sc);
			s->sc.fd = -1;
		}
		sccp_session_unlock(s);

		/* destroying mutex and cleaning the session */
		sccp_mutex_destroy(&s->lock);
		sccp_mutex_destroy(&s->write_lock);
		pbx_cond_destroy(&s->pendingRequest);
		sccp_free(s);
		s = NULL;
	}
}

/*!
 * \brief Socket Device Thread Exit
 * \param session SCCP Session
 *
 * \callgraph
 * \callergraph
 */
void sccp_session_device_thread_exit(void *session)
{
	sccp_session_t *s = (sccp_session_t *) session;

	if (!s->device) {
		sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_3 "SCCP: Session without a device attached !\n");
	}

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: cleanup session\n", DEV_ID_LOG(s->device));
	sccp_session_lock(s);
	s->session_stop = TRUE;
	/*	if (s->sc.fd > 0) {
			s->srvcontext->transport->close(&s->sc);
			s->sc.fd = -1;
		}*/
	sccp_session_unlock(s);
	s->session_thread = AST_PTHREADT_NULL;
	destroy_session(s);
}

gcc_inline void recalc_wait_time(sccp_session_t *s)
{
	float keepaliveAdditionalTimePercent = KEEPALIVE_ADDITIONAL_PERCENT_SESSION;
	float keepAlive = GLOB(keepalive);
	float keepAliveInterval = GLOB(keepalive);
	sccp_device_t *d = s->device;
	if (d) {
		keepAlive = d->keepalive;
		keepAliveInterval = d->keepaliveinterval;
		if (
			d->skinny_type == SKINNY_DEVICETYPE_CISCO7920 || d->skinny_type == SKINNY_DEVICETYPE_CISCO7921 ||
			d->skinny_type == SKINNY_DEVICETYPE_CISCO7925 || d->skinny_type == SKINNY_DEVICETYPE_CISCO7926 ||
			d->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || d->skinny_type == SKINNY_DEVICETYPE_CISCO7975 ||
			d->skinny_type == SKINNY_DEVICETYPE_CISCO6911
		) {
			keepaliveAdditionalTimePercent = KEEPALIVE_ADDITIONAL_PERCENT_DEVICE;
		}
		if (d->active_channel) {
			keepaliveAdditionalTimePercent = KEEPALIVE_ADDITIONAL_PERCENT_ON_CALL;
		}
	}
       s->keepAlive = (uint16_t)(keepAlive * keepaliveAdditionalTimePercent);
       //s->keepAliveInterval = (uint16_t)(keepAliveInterval * KEEPALIVE_ADDITIONAL_PERCENT_SESSION);
       s->keepAliveInterval = (uint16_t)keepAliveInterval;

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_4 "%s: keepalive:%d, keepaliveinterval:%d\n", s->designator, s->keepAlive, s->keepAliveInterval);
	if (!s->keepAlive || !s->keepAliveInterval) {	/* temporary */
		pbx_log(LOG_NOTICE, "SCCP: keepalive interval calculation failed!\n");
		s->keepAlive = GLOB(keepalive);
		s->keepAliveInterval = GLOB(keepalive);
	}
}

/*!
 * \brief Socket Device Thread
 * \param session SCCP Session
 *
 * \callgraph
 * \callergraph
 */
void *sccp_session_device_thread(void *session)
{
	int res = 0;
	sccp_session_t *s = (sccp_session_t *) session;

	if (!s) {
		return NULL;
	}

	boolean_t oncall = TRUE;
	boolean_t tokenThread = FALSE;
	unsigned char recv_buffer[SCCP_MAX_PACKET * 2] = "";
	size_t recv_len = 0;
	sccp_msg_t msg = { {0,} };

	pthread_cleanup_push(sccp_session_device_thread_exit, session);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	struct pollfd fds[1] = { { 0 } };
	fds[0].events = POLLIN | POLLPRI;
	fds[0].revents = 0;
	fds[0].fd = s->sc.fd;

	while(s->sc.fd > 0 && !s->session_stop) {
		if (s->device) {
			sccp_device_t *d = s->device;
			if (d->pendingUpdate || d->pendingDelete) {
				pbx_rwlock_rdlock(&GLOB(lock));
				boolean_t reload_in_progress = GLOB(reload_in_progress);
				pbx_rwlock_unlock(&GLOB(lock));
				if(reload_in_progress == FALSE && sccp_device_check_update(d)) {
					continue;
				}
				sccp_safe_sleep(100);
				if(!s->device) {
					continue;
				}
			}
			if ((d->active_channel ? TRUE : FALSE) != oncall) {
				recalc_wait_time(s);
				oncall = (d->active_channel) ? TRUE : FALSE;
			}
			if (d->status.token == SCCP_TOKEN_STATE_ACK) {
				tokenThread = TRUE;								// only does TCP-Keepalive
			}
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_HIGH))(VERBOSE_PREFIX_4 "%s: set poll timeout %d for session %d\n", DEV_ID_LOG(s->device), (int)s->keepAliveInterval, fds[0].fd);

		res = sccp_netsock_poll(fds, 1, s->keepAliveInterval * 1000);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (-1 == res) {										/* poll data processing */
			if (errno > 0 && (errno != EAGAIN) && (errno != EINTR)) {
				pbx_log(LOG_ERROR, "%s: poll() returned %d. errno: %s, (ip-address: %s)\n", DEV_ID_LOG(s->device), errno, strerror(errno), s->designator);
				socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__);
				__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
				break;
			}
		} else if (0 == res) {										/* poll timeout */
			uintmax_t timediff = (uintmax_t)time(0) - (uintmax_t)s->lastKeepAlive;
			if (!tokenThread && timediff >= s->keepAlive) {
				pbx_log(LOG_NOTICE, "%s: Closing session because connection timed out after %ju seconds (ip-address: %s).\n", DEV_ID_LOG(s->device), timediff, s->designator);
				__sccp_session_stopthread(s, SKINNY_DEVICE_RS_TIMEOUT);
				break;
			}
		} else if (res > 0) {										/* poll data processing */
			if(fds[0].revents & POLLIN || fds[0].revents & POLLPRI) {                               /* POLLIN | POLLPRI */
				// sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Session New Data Arriving at buffer position:%lu\n", DEV_ID_LOG(s->device), recv_len);
				int result       = s->srvcontext->transport->recv(&s->sc, recv_buffer + recv_len, (ARRAY_LEN(recv_buffer) * sizeof(unsigned char)) - recv_len, 0);
				s->lastKeepAlive = time(0);
				if (result <= 0) {
					if (result < 0 || (errno != EINTR || errno != EAGAIN)) {
						socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__);
						break;
					}
				} else if (!((recv_len += result) && ((ARRAY_LEN(recv_buffer) * sizeof(unsigned char)) - recv_len) && process_buffer(s, &msg, recv_buffer, &recv_len) == 0)) {
					pbx_log(LOG_ERROR, "%s: (netsock_device_thread) Received a packet or message (with result:%d) which we could not handle, giving up session: %p!\n", s->designator, result, s);
					sccp_dump_msg(&msg);
					if (s->device) {
						sccp_device_sendReset(s->device, SKINNY_RESETTYPE_RESTART);
					}
					__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
					break;
				}
				s->lastKeepAlive = time(0);
			} else { /* POLLHUP / POLLERR */
				pbx_log(LOG_NOTICE, "%s: Closing session because we received POLLPRI/POLLHUP/POLLERR\n", s->designator);
				__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
				break;
			}
		} else {											/* poll returned invalid res */
			pbx_log(LOG_NOTICE, "%s: Poll Returned invalid result: %d.\n", DEV_ID_LOG(s->device), res);
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	}
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Exiting sccp_socket device thread\n", DEV_ID_LOG(s->device));
	pthread_cleanup_pop(1);

	return NULL;
}

/* stop session device thread from the same thread */
void __sccp_session_stopthread(sessionPtr s, skinny_registrationstate_t newRegistrationState)
{
	if(!s) {
		pbx_log(LOG_NOTICE, "SCCP: session already terminated\n");
		return;
	}
	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_2 "%s: Stopping Session Thread\n", DEV_ID_LOG(s->device));

	s->session_stop = TRUE;
	if(s->device) {
		sccp_device_setRegistrationState(s->device, newRegistrationState);
	}
	if(AST_PTHREADT_NULL != s->session_thread) {
		s->srvcontext->transport->shutdown(&s->sc, SHUT_RD);                                        // this will also wake up poll
													    // which is waiting for a read event and close down the thread nicely
	}
}

/* cleanup session device thread from another thread */
static void __sccp_netsock_end_device_thread(sccp_session_t *session)
{
	pthread_t session_thread = session->session_thread;
	if (session_thread == AST_PTHREADT_NULL) {
		return;
	}

	/* send thread cancellation (will interrupt poll if necessary) */
	int s = pthread_cancel(session_thread);
	if (s != 0) {
		pbx_log(LOG_NOTICE, "SCCP: (sccp_netsock_end_device_thread) pthread_cancel error\n");
	}

	/* join previous session thread, wait for device cleanup */
	void * res = NULL;
	if (pthread_join(session_thread, &res) == 0) {
		if (res != PTHREAD_CANCELED) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_netsock_end_device_thread) pthread join failed\n");
		}
	}
}

/* check if same or different thread, choose thread cancel method accordingly */
gcc_inline void sccp_session_stopthread(constSessionPtr session, skinny_registrationstate_t newRegistrationState)
{
	sessionPtr s = (sessionPtr)session;										/* discard const */
	if (s) {
		pthread_t ptid = pthread_self();
		if (ptid == s->session_thread) {
			__sccp_session_stopthread(s, newRegistrationState);
		} else {
			__sccp_netsock_end_device_thread(s);
		}
	}
}

static boolean_t sccp_session_new_socket_allowed(struct sockaddr_storage *sin)
{
	char addrStr[INET6_ADDRSTRLEN];
	sccp_copy_string(addrStr, sccp_netsock_stringify(sin), sizeof(addrStr));
	if (GLOB(ha) && sccp_apply_ha(GLOB(ha), sin) != AST_SENSE_ALLOW) {
		struct ast_str *buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		if (buf) {
			sccp_print_ha(buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(ha));
			pbx_log(LOG_NOTICE, "SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", addrStr, pbx_str_buffer(buf));
		} else {
			pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		}
		//sccp_session_reject(s, "Device ip not authorized");
		return FALSE;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted Client Connection from %s\n", addrStr);
	return TRUE;
}

static sccp_session_t * sccp_create_session(sccp_servercontext_t * context, sccp_socket_connection_t * sc)
{
	sccp_session_t * s = NULL;

	if (!(s = (sccp_session_t *)sccp_calloc(sizeof *s, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return NULL;
	}

	sccp_mutex_init(&s->lock);
	pbx_cond_init(&s->pendingRequest, NULL);
	sccp_mutex_init(&s->write_lock);

	s->sc.fd = sc->fd;
	s->sc.ssl = sc->ssl;
	s->protocolType = SCCP_PROTOCOL;
	s->srvcontext = context;

	s->lastKeepAlive = time(0);
	
	return s;
}

static boolean_t sccp_session_set_ourip(sccp_session_t * s)
{
	/** set default handler for registration to sccp */
	if (sccp_netsock_is_any_addr(&GLOB(bindaddr))) {
		struct sockaddr_storage them = { 0 };

		if(sccp_netsock_is_mapped_IPv4(&s->sin)) {
			sccp_netsock_ipv4_mapped(&s->sin, &them);
		} else {
			memcpy(&them, &s->sin, sizeof(struct sockaddr_storage));
		}

		// starting guess for the internal address
		memcpy(&s->ourip, &internip.ss, sizeof(struct sockaddr_storage));

		// now ask the system what would it use to talk to 'them'
		if(!sccp_netsock_ouraddrfor(&them, &s->ourip)) {
			pbx_log(LOG_ERROR, "SCCP: Could not retrieve a valid ip-address to use to communicate with client '%s'\n", sccp_netsock_stringify(&s->sin));
			// return FALSE
		}
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr), sizeof(s->ourip));
	}
	sccp_copy_string(s->designator, sccp_netsock_stringify(&s->ourip), sizeof(s->designator));
	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: Connected on server via %s\n", s->designator);
	return TRUE;
}

/*!
 * Accept Thread
 * continuesly waits for devices trying to connect, when they do it
 * - checks if the incoming ip-address is within the global deny/permit range
 * - creates a new session struct
 * - adds the new session struct to the global sessions list
 * - starts a new sccp_session_device_thread
 */
static void * accept_thread(void * data)
{
	sccp_servercontext_t * context = (sccp_servercontext_t *)data;
	sccp_socket_connection_t new_sc = { 0, 0 };
	struct sockaddr_storage incoming;
	sccp_session_t *s = NULL;
	socklen_t length = (socklen_t)(sizeof(struct sockaddr_storage));

	while (GLOB(module_running)) {
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel();
		memset(&new_sc, 0, sizeof(new_sc));
		context->transport->accept(&context->sc, (struct sockaddr *)&incoming, &length, &new_sc);
		if(new_sc.fd < 0) {
			pbx_log(LOG_ERROR, "Error accepting new socket %s on acceptFD:%d\n", strerror(errno), context->sc.fd);
			usleep(1000);
			continue;
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		sccp_netsock_setoptions(new_sc.fd, /*reuse*/ -1, /*linger*/ 0, /*keepalive*/ -1, /*sndtimeout*/ -1, /*rcvtimeout*/ 0);

		if (!sccp_session_new_socket_allowed(&incoming)) {
			context->transport->close(&new_sc);
			continue;
		}

		s = sccp_create_session(context, &new_sc);
		if(s == NULL) {
			context->transport->close(&new_sc);
			continue;
		}
		memcpy(&s->sin, &incoming, sizeof(s->sin));
		sccp_session_set_ourip(s);
		sccp_session_addToGlobals(s);
		recalc_wait_time(s);
	
		// Create a detached thread, since the sccp_session_device_thread will not be joined from another thread
		// Only detached threads free their stack and control structures after termination, otherwise a pthread_join is mandatory for this to take place (davidded).
		if (pbx_pthread_create_detached(&s->session_thread, NULL, sccp_session_device_thread, s)) {
			destroy_session(s);
		}
	}
	context->transport->close(&new_sc);
	if(context->sc.fd > -1) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "Closing Listening Port:%d\n", context->sc.fd);
		context->transport->close(&context->sc);
		context->sc.fd = -1;
	}
	return 0;
}

/*!
 * Start the session accept thread
 */
static void sccp_session_start_accept_thread(sccp_servercontext_t * context)
{
	ast_pthread_create_background(&context->accept_tid, NULL, accept_thread, (void *)context);
}

/*!
 * Stops the session accept thread
 * Closes the listening socket
 */
void sccp_session_stop_accept_thread(sccp_servercontext_t * context)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Stopping Accepting Thread\n");
	pbx_rwlock_wrlock(&GLOB(lock));
	if(context->accept_tid && (context->accept_tid != AST_PTHREADT_STOP)) {
		if (pthread_cancel(context->accept_tid) != 0) {
			pthread_kill(context->accept_tid, SIGURG);
		}
		pthread_join(context->accept_tid, NULL);
	}
	context->accept_tid = AST_PTHREADT_STOP;
	if(context->sc.fd > -1) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "Closing Listening Port:%d\n", context->sc.fd);
		context->transport->close(&context->sc);
		context->sc.fd = -1;
	}
	pbx_rwlock_unlock(&GLOB(lock));
}

/*!
 * Bind and Listen
 * Binds to the provided bindaddress (and port)
 * If the socket was already bound and listening, it is stopped and cleaned up first
 * If successfull it will start the listening/accepting thread
 *
 * The bound accepting socket is stored in a static global variable (see at top)
 * The thread id (tid) is stored in a static global variable (see at top)
 *
 * param bindaddr SockAddr Storage
 * returns TRUE on success
 */
// boolean_t sccp_session_bind_and_listen(constTransportPtr transport, struct sockaddr_storage * bindaddr)
boolean_t sccp_session_bind_and_listen(sccp_servercontext_t * context, struct sockaddr_storage * bindaddr)
{
	int result = FALSE;
	// static struct sockaddr_storage boundaddr = {0};
	static int port = -1;
	char addrStr[INET6_ADDRSTRLEN];
	sccp_copy_string(addrStr, sccp_netsock_stringify_addr(bindaddr), sizeof(addrStr));

	/*
	if (context->sc.fd > -1 && ( sccp_netsock_getPort(&boundaddr) != sccp_netsock_getPort(bindaddr) || sccp_netsock_cmp_addr(&boundaddr, bindaddr) ) ) {
		sccp_session_stop_accept_thread();
	}
	*/

	sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "Running bind and listen '%s'\n", addrStr);
	if(context->sc.fd < 0) {
		int status = 0;
		port = sccp_netsock_getPort(bindaddr);
		memcpy(&context->boundaddr, bindaddr, sizeof(struct sockaddr_storage));
		char port_str[15] = "cisco-sccp";

		struct addrinfo hints;

		struct addrinfo * res = NULL;
		memset(&hints, 0, sizeof hints);								// make sure the struct is empty
		hints.ai_family = AF_UNSPEC;									// don't care IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM;								// TCP stream sockets
		hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;					// fill in my IP for me
		if (port) {
			snprintf(port_str, sizeof(port_str), "%d", port);
		}

		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "Checking /etc/services for '%s:%s'!\n", addrStr, port_str);
		status = getaddrinfo(sccp_netsock_stringify_addr(bindaddr), port_str, &hints, &res);
		if(status != 0) {
			pbx_log(LOG_ERROR, "Failed to get addressinfo for %s:%s, error: %s!\n", sccp_netsock_stringify_addr(bindaddr), port_str, gai_strerror(status));
			return FALSE;
		}
		do {
			context->sc.fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if(context->sc.fd < 0) {
				pbx_log(LOG_ERROR, "Unable to create SCCP socket: %s\n", strerror(errno));
				break;
			}
			sccp_netsock_setoptions(context->sc.fd, /*reuse*/ 1, /*linger*/ -1, /*keepalive*/ -1, /*sndtimeout*/ 0, /*rcvtimeout*/ 0);
			if(context->transport->bind(&context->sc, res->ai_addr, res->ai_addrlen) < 0) {
				pbx_log(LOG_ERROR, "Failed to bind to %s:%d: %s!\n", addrStr, port, strerror(errno));
				context->transport->close(&context->sc);
				context->sc.fd = -1;
				break;
			}

			struct ast_sockaddr tmp_sa;
			ast_sockaddr_copy(&internip, storage2ast_sockaddr(bindaddr, &tmp_sa));
			if(ast_find_ourip(&internip, &tmp_sa, 0)) {
				ast_log(LOG_ERROR, "Unable to get own IP address\n");
				context->transport->close(&context->sc);
				context->sc.fd = -1;
				break;
			}

			if(listen(context->sc.fd, DEFAULT_SCCP_BACKLOG)) {
				pbx_log(LOG_ERROR, "Failed to start listening to %s:%d: %s\n", addrStr, port, strerror(errno));
				context->transport->close(&context->sc);
				context->sc.fd = -1;
				break;
			}
			sccp_session_start_accept_thread(context);
		} while(0);
		freeaddrinfo(res);
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Socket has not changed so we are reusing it\n");
	}

	if(context->sc.fd > -1) {
		sccp_log((DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "SCCP: Listening on %s:%d using socket:%d\n", addrStr, port, context->sc.fd);
		sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: using default ip:%s\n", ast_sockaddr_stringify_addr(&internip));
		result = TRUE;
	}
	return result;	
}

/*!
 * \brief Socket Send Message
 * \param device SCCP Device
 * \param t SCCP Message
 */
void sccp_session_sendmsg(const sccp_device_t * device, sccp_mid_t t)
{
	if (!device || !device->session) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: (sccp_session_sendmsg) No device available to send message to\n");
		return;
	}

	sccp_msg_t *msg = sccp_build_packet(t, 0);
	if (msg) {
		sccp_session_send(device, msg);
	}
}

/*!
 * \brief Socket Send
 * \param device SCCP Device
 * \param msg_in Message Data Structure (sccp_msg_t)
 * \return SCCP Session Send
 */
int sccp_session_send(constDevicePtr device, const sccp_msg_t * msg_in)
{
	//const sccp_session_t * const s = sccp_session_findByDevice(device);
	sccp_msg_t *msg = (sccp_msg_t *) msg_in;				/* discard const * const */
	const sccp_session_t * const s = device && device->session ? device->session : NULL;

	if (s && !s->session_stop) {
		return sccp_session_send2(s, msg);
	} 
	return -1;
}

/*!
 * \brief Socket Send Message
 * \param session Session SCCP Session (can't be null)
 * \param msg Message Data Structure (sccp_msg_t) (Will be freed automatically at the end)
 * \return Result as Int
 *
 * \lock
 *      - session
 */
int sccp_session_send2(constSessionPtr session, sccp_msg_t * msg)
{
	sessionPtr s = (sessionPtr)session;										/* discard const */
	ssize_t res = 0;
	uint32_t msgid = letohl(msg->header.lel_messageId);
	ssize_t bytesSent = 0;
	ssize_t bufLen = 0;
	uint8_t * bufAddr = NULL;

	if (s && s->session_stop) {
		return -2;
	}

	if(!s || s->sc.fd <= 0) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		if (s) {
			__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
		}
		sccp_free(msg);
		msg = NULL;
		return -3;
	}
	if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage || msgid == UnregisterAckMessage) {
		msg->header.lel_protocolVer = 0;
	} else if (s->device && s->device->protocol) {
		msg->header.lel_protocolVer = s->device->protocol->version < 10 ? 0 : htolel(s->device->protocol->version);
	}

	uint backoff = WRITE_BACKOFF;
	bytesSent = 0;
	bufAddr = ((uint8_t *) msg);
	bufLen = (ssize_t) (letohl(msg->header.length) + 8);

	struct messageinfo * msginfo = lookupMsgInfoStruct(msgid);
	if(msginfo) {
		if(msginfo->messageId != msgid) {
			pbx_log(LOG_ERROR, "%s: (session_send2) messageId %d (0x%x) unknown. matched:0x%x discarding message.\n", DEV_ID_LOG(s->device), msgid, msgid, msginfo->messageId);
			return -4;
		}
		if(msginfo->type == SKINNY_MSGTYPE_REQUEST) {
			request_pending(s);
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: Request '%s' to device Pending\n", DEV_ID_LOG(s->device), msginfo->text);
		}
		if((GLOB(debug) & DEBUGCAT_MESSAGE) != 0) {
			pbx_log(LOG_NOTICE, "%s: Sending Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(s->device), msginfo->text, msgid, msg->header.length);
			sccp_dump_msg(msg);
		}
	}
	do {
		pbx_mutex_lock(&s->write_lock);									/* prevent two threads writing at the same time. That should happen in a synchronized way */
		res = s->srvcontext->transport->send(&s->sc, bufAddr + bytesSent, bufLen - bytesSent, 0);
		pbx_mutex_unlock(&s->write_lock);
		if (res <= 0) {
			if (errno == EINTR) {
				usleep(backoff);								/* back off to give network/other threads some time */
				backoff *= 2;
				continue;
			}
			socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__);
			if (s) {
				__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
			}
			res = -1;
			break;
		}
		bytesSent += res;
	} while(bytesSent < bufLen && s && !s->session_stop && s->sc.fd > 0);

	sccp_free(msg);
	msg = NULL;

	if (bytesSent < bufLen) {
		pbx_log(LOG_ERROR, "%s: Could only send %d of %d bytes!\n", DEV_ID_LOG(s->device), (int) bytesSent, (int) bufLen);
		res = -1;
	}

	return res;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param message Message as char (reason of rejection)
 */
sccp_session_t *sccp_session_reject(constSessionPtr session, char *message)
{
	sccp_msg_t *msg = NULL;
	sessionPtr s = (sessionPtr)session;										/* discard const */

	REQ(msg, RegisterRejectMessage);
	if (!msg) {
		return NULL;
	}
	sccp_copy_string(msg->data.RegisterRejectMessage.text, message, sizeof(msg->data.RegisterRejectMessage.text));
	sccp_session_send2(s, msg);
	return NULL;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param current_session SCCP Session Pointer
 * \param previous_session SCCP Session Pointer
 * \param token Do we need to return a token reject or a session reject (as Boolean)
 */
void sccp_session_crossdevice_cleanup(constSessionPtr current_session, sessionPtr previous_session)
{
	if (!current_session || !previous_session) {
		return;
	}
	if (current_session != previous_session && previous_session->session_thread) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "%s: Session %p needs to be closed!\n", current_session->designator, previous_session->designator);
		__sccp_netsock_end_device_thread(previous_session);
	}
}

gcc_inline boolean_t sccp_session_check_crossdevice(constSessionPtr session, constDevicePtr device)
{
	if (session && device && ((session->device && session->device != device) || (device->session && device->session != session))) {
		pbx_log(LOG_WARNING, "Session(%p) and Device Session(%p) are out of sync.\n", session, device->session);
		return TRUE;
	}
	return FALSE;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param backoff_time Time to Backoff before retrying TokenSend
 */
void sccp_session_tokenReject(constSessionPtr session, uint32_t backoff_time)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, RegisterTokenReject);
	if (!msg) {
		return;
	}
	msg->data.RegisterTokenReject.lel_tokenRejWaitTime = htolel(backoff_time);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send a token acknowledgement.
 * \param session SCCP Session Pointer
 */
void sccp_session_tokenAck(constSessionPtr session)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, RegisterTokenAck);
	if (!msg) {
		return;
	}
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send an Reject Message to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenRejectSPCP(constSessionPtr session, uint32_t features)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, SPCPRegisterTokenReject);
	if (!msg) {
		return;
	}
	msg->data.SPCPRegisterTokenReject.lel_features = htolel(features);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send a token acknowledgement to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenAckSPCP(constSessionPtr session, uint32_t features)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, SPCPRegisterTokenAck);
	if (!msg) {
		return;
	}
	msg->data.SPCPRegisterTokenAck.lel_features = htolel(features);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Set Session Protocol
 * \param session SCCP Session
 * \param protocolType Protocol Type as uint16_t
 */
gcc_inline void sccp_session_setProtocol(constSessionPtr session, uint16_t protocolType)
{
	sessionPtr s = (sessionPtr)session;										/* discard const */
	if (s) {
		s->protocolType = protocolType;
	}
}

/*!
 * \brief Get Session Protocol
 * \param session SCCP Session
 */
gcc_inline uint16_t sccp_session_getProtocol(constSessionPtr session)
{
	if (session) {
		return session->protocolType;
	}
	return UNKNOWN_PROTOCOL;
}

/*!
 * \brief Reset Last KeepAlive
 * \param session SCCP Session
 */
gcc_inline void sccp_session_resetLastKeepAlive(constSessionPtr session)
{
	sessionPtr s = (sessionPtr)session;										/* discard const */
	if (s) {
		s->lastKeepAlive = time(0);
	}
}

gcc_inline const char * const sccp_session_getDesignator(constSessionPtr session)
{
	return session->designator;
}

/*!
 * \brief Get device connected to this session
 * \note returns retained device
 */
gcc_inline devicePtr sccp_session_getDevice(constSessionPtr session, boolean_t required)
{
	if (!session) {
		return NULL;
	}
	sccp_device_t *device = (session->device) ? sccp_device_retain(session->device) : NULL;
	if (required && !device) {
		pbx_log(LOG_WARNING, "No valid Session Device available\n");
		return NULL;
	}
	if (required && sccp_session_check_crossdevice(session, device)) {
		sccp_session_crossdevice_cleanup(session, device->session);
		sccp_device_release(&device);							/* explicit release after error */
		return NULL;
	}
	return device;
}

boolean_t sccp_session_isValid(constSessionPtr session)
{
	if(session && session->sc.fd > 0 && !session->session_stop && !sccp_netsock_is_any_addr(&session->ourip)) {
		return TRUE;
	}
	return FALSE;
}

/* -------------------------------------------------------------------------------------------------------SHOW SESSIONS- */
/*!
 * \brief Show Sessions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 *
 * \called_from_asterisk
 *
 */
int sccp_cli_show_sessions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;
	char clientAddress[INET6_ADDRSTRLEN] = "";

#define CLI_AMI_TABLE_NAME Sessions
#define CLI_AMI_TABLE_PER_ENTRY_NAME Session
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(sessions)
#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_session_t
#define CLI_AMI_TABLE_LIST_ITER_VAR session
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK
#define CLI_AMI_TABLE_BEFORE_ITERATION                                                                      \
	sccp_session_lock(session);                                                                         \
	sccp_copy_string(clientAddress, sccp_netsock_stringify_addr(&session->sin), sizeof(clientAddress)); \
	AUTO_RELEASE(sccp_device_t, d, session->device ? sccp_device_retain(session->device) : NULL);       \
	if(d || (argc == 4 && sccp_strcaseequals(argv[3], "all"))) {
#define CLI_AMI_TABLE_AFTER_ITERATION \
	}                             \
	sccp_session_unlock(session);
#define CLI_AMI_TABLE_FIELDS                                                                                                           \
	CLI_AMI_TABLE_FIELD(Socket, "-6", d, 6, session->sc.fd)                                                                        \
	CLI_AMI_TABLE_FIELD(IP, "40.40", s, 40, clientAddress)                                                                         \
	CLI_AMI_TABLE_FIELD(Trans, "5.5", s, 5, session->srvcontext->transport->name)                                                  \
	CLI_AMI_TABLE_FIELD(Port, "-5", d, 5, sccp_netsock_getPort(&session->sin))                                                     \
	CLI_AMI_TABLE_FIELD(KALST, "-5", d, 5, (uint32_t)(time(0) - session->lastKeepAlive))                                           \
	CLI_AMI_TABLE_FIELD(KAINT, "-5", d, 5, (d ? d->keepaliveinterval : session->keepAliveInterval))                                \
	CLI_AMI_TABLE_FIELD(KAMAX, "-5", d, 5, session->keepAlive)                                                                     \
	CLI_AMI_TABLE_FIELD(DeviceName, "15", s, 15, (d) ? d->id : "--")                                                               \
	CLI_AMI_TABLE_FIELD(State, "-14.14", s, 14, (d) ? sccp_devicestate2str(sccp_device_getDeviceState(d)) : "--")                  \
	CLI_AMI_TABLE_FIELD(Type, "-15.15", s, 15, (d) ? skinny_devicetype2str(d->skinny_type) : "--")                                 \
	CLI_AMI_TABLE_FIELD(RegState, "-10.10", s, 10, (d) ? skinny_registrationstate2str(sccp_device_getRegistrationState(d)) : "--") \
	CLI_AMI_TABLE_FIELD(Token, "-10.10", s, 10, d ? sccp_tokenstate2str(d->status.token) : "--")                                   \
	CLI_AMI_TABLE_FIELD(Req, "-3", d, 3, session->requestsInFlight)
#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
