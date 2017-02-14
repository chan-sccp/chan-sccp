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
#include <netinet/in.h>

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

#define WRITE_BACKOFF 500											/* backoff time in millisecs, doubled every write retry (150+300+600+1200+2400+4800 = 9450 millisecs = 9.5 sec) */
#define SESSION_DEVICE_CLEANUP_TIME 10										/* wait time before destroying a device on thread exit */
#define KEEPALIVE_ADDITIONAL_PERCENT_SESSION 1.05								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */
#define KEEPALIVE_ADDITIONAL_PERCENT_DEVICE 1.20								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */
#define KEEPALIVE_ADDITIONAL_PERCENT_ON_CALL 2.00								/* extra time allowed for device keepalive overrun (percentage of GLOB(keepalive)) */

/* Lock Macro for Sessions */
#define sccp_session_lock(x)			pbx_mutex_lock(&(x)->lock)
#define sccp_session_unlock(x)			pbx_mutex_unlock(&(x)->lock)
#define sccp_session_trylock(x)			pbx_mutex_trylock(&(x)->lock)
/* */

void sccp_netsock_device_thread_exit(void *session);
void *sccp_netsock_device_thread(void *session);
void __sccp_session_stopthread(sessionPtr session, uint8_t newRegistrationState);
gcc_inline void recalc_wait_time(sccp_session_t *s);

/*!
 * \brief SCCP Session Structure
 * \note This contains the current session the phone is in
 */
struct sccp_session {
	time_t lastKeepAlive;											/*!< Last KeepAlive Time */
	uint16_t keepAlive;
	uint16_t keepAliveInterval;
	SCCP_RWLIST_ENTRY (sccp_session_t) list;								/*!< Linked List Entry for this Session */
	sccp_device_t *device;											/*!< Associated Device */
	struct pollfd fds[1];											/*!< File Descriptor */
	struct sockaddr_storage sin;										/*!< Incoming Socket Address */
	uint32_t protocolType;
	volatile boolean_t session_stop;									/*!< Signal Session Stop */
	sccp_mutex_t write_lock;										/*!< Prevent multiple threads writing to the socket at the same time */
	sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */
	pthread_t session_thread;										/*!< Session Thread */
	struct sockaddr_storage ourip;										/*!< Our IP is for rtp use */
	struct sockaddr_storage ourIPv4;
	char designator[40];
};														/*!< SCCP Session Structure */

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

/*!
 * \brief Exchange Socket Addres Information from them to us
 */
static int __sccp_session_setOurAddressFromTheirs(const struct sockaddr_storage *them, struct sockaddr_storage *us)
{
	int sock;
	socklen_t slen;

	union sockaddr_union {
		struct sockaddr sa;
		struct sockaddr_storage ss;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} tmp_addr = {
		.ss = *them,
	};

	if (sccp_netsock_is_IPv6(them)) {
		tmp_addr.sin6.sin6_port = htons(sccp_netsock_getPort(&GLOB(bindaddr)));
		slen = sizeof(struct sockaddr_in6);
	} else if (sccp_netsock_is_IPv4(them)) {
		tmp_addr.sin.sin_port = htons(sccp_netsock_getPort(&GLOB(bindaddr)));
		slen = sizeof(struct sockaddr_in);
	} else {
		pbx_log(LOG_WARNING, "SCCP: getOurAddressfor Unspecified them format: %s\n", sccp_netsock_stringify(them));
		return -1;
	}

	if ((sock = socket(tmp_addr.ss.ss_family, SOCK_DGRAM, 0)) < 0) {
		return -1;
	}

	if (connect(sock, &tmp_addr.sa, sizeof(tmp_addr))) {
		pbx_log(LOG_WARNING, "SCCP: getOurAddressfor Failed to connect to %s\n", sccp_netsock_stringify(them));
		close(sock);
		return -1;
	}
	if (getsockname(sock, &tmp_addr.sa, &slen)) {
		close(sock);
		return -1;
	}
	close(sock);
	memcpy(us, &tmp_addr, slen);
	return 0;
}

int sccp_session_setOurIP4Address(constSessionPtr session, const struct sockaddr_storage *addr)
{
	sccp_session_t * const s = (sccp_session_t * const)session;						/* discard const */
	if (s) {
		return __sccp_session_setOurAddressFromTheirs(addr, &s->ourIPv4);
	}
	return -2;
}

static void socket_get_error(constSessionPtr s, const char* file, int line, const char *function, int __errnum)
{
	if (errno) {
		if (errno == ECONNRESET) {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: Connection reset by peer\n", DEV_ID_LOG(s->device));
		} else {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s (%s:%d:%s) Socket returned error: '%s (%d)')\n", DEV_ID_LOG(s->device), file, line, function, strerror(errno), errno);
		}
	} else {
		if (!s || s->fds[0].fd <= 0) {
			return;
		}
		int mysocket = s->fds[0].fd;
		int error = 0;
		socklen_t error_len = sizeof(error);
		if ((mysocket && getsockopt(mysocket, SOL_SOCKET, SO_ERROR, &error, &error_len) == 0) && error != 0) {
			sccp_log(DEBUGCAT_SOCKET)(VERBOSE_PREFIX_3 "%s: (%s:%d:%s) SO_ERROR: %s (%d)\n", DEV_ID_LOG(s->device), file, line, function, strerror(error), error);
		}
	}
}

static int session_dissect_header(sccp_session_t * s, sccp_header_t * header)
{
	int result = -1;
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

		const struct messagetype *msgtype;
		if (messageId <= SCCP_MESSAGE_HIGH_BOUNDARY) {
			msgtype = &sccp_messagetypes[messageId];
			if (msgtype->messageId == messageId) {
				return msgtype->size + SCCP_PACKET_HEADER;
			}
			pbx_log(LOG_ERROR, "%s: (session_dissect_header) messageId %d (0x%x) unknown. discarding message.\n", DEV_ID_LOG(s->device), messageId, messageId);
			break;
		} else if (messageId >= SPCP_MESSAGE_LOW_BOUNDARY && messageId <= SPCP_MESSAGE_HIGH_BOUNDARY) {
			msgtype = &spcp_messagetypes[messageId - SPCP_MESSAGE_OFFSET];
			if (msgtype->messageId == messageId) {
				return msgtype->size + SCCP_PACKET_HEADER;
			}
			pbx_log(LOG_ERROR, "%s: (session_dissect_header) messageId %d (0x%x) unknown. discarding message.\n", DEV_ID_LOG(s->device), messageId, messageId);
			break;
		} else {
			pbx_log(LOG_ERROR, "%s: (session_dissect_header) messageId out of bounds: %d < %u > %d. Or messageId unknown. discarding message.\n", DEV_ID_LOG(s->device), SCCP_MESSAGE_LOW_BOUNDARY, messageId, SPCP_MESSAGE_HIGH_BOUNDARY);
			break;
		}
	} while (0);

	return result;
}

static gcc_inline int session_buffer2msg(sccp_session_t * s, unsigned char *buffer, int lenAccordingToPacketHeader, sccp_msg_t *msg) 
{
	sccp_header_t msg_header = {0};
	memcpy(&msg_header, buffer, SCCP_PACKET_HEADER);
	int lenAccordingToOurProtocolSpec = session_dissect_header(s, &msg_header);
	if (dont_expect(lenAccordingToOurProtocolSpec < 0)) {
		if (lenAccordingToOurProtocolSpec == -2) {
			return 0;
		}
		lenAccordingToOurProtocolSpec = 0;									// unknown message, read it and discard content completely
	}
	if (dont_expect(lenAccordingToPacketHeader > lenAccordingToOurProtocolSpec)) {					// show out discarded bytes
		pbx_log(LOG_WARNING, "%s: (session_dissect_msg) Incoming message is bigger(%d) than known size(%d). Packet looks like!\n", DEV_ID_LOG(s->device), lenAccordingToPacketHeader, lenAccordingToOurProtocolSpec);
		buffer[lenAccordingToPacketHeader + 1] = '\0';								// terminate buffer
		sccp_dump_packet(buffer, lenAccordingToPacketHeader);
	}
	
	if (((unsigned int)lenAccordingToPacketHeader) < ((unsigned int)lenAccordingToOurProtocolSpec)){
		sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: (session_dissect_msg) Incoming message is smaller(%d) than known size(%d).\n", DEV_ID_LOG(s->device), lenAccordingToPacketHeader, lenAccordingToOurProtocolSpec);
		lenAccordingToOurProtocolSpec = lenAccordingToPacketHeader;
	}

	memset(msg, 0, SCCP_MAX_PACKET);
	memcpy(msg, buffer, lenAccordingToOurProtocolSpec);
	msg->header.length = lenAccordingToOurProtocolSpec;								// patch up msg->header.length to new size
	return sccp_handle_message(msg, s);
}

static gcc_inline int process_buffer(sccp_session_t * s, sccp_msg_t *msg, unsigned char *buffer, size_t *len)
{
	int res = 0;
	while (*len >= SCCP_PACKET_HEADER && *len <= SCCP_MAX_PACKET * 2) {										// We have at least SCCP_PACKET_HEADER, so we have the payload length
		uint32_t hdr_len = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
		uint32_t payload_len = letohl(hdr_len) + (SCCP_PACKET_HEADER - 4);
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
	sccp_session_t *session;
	boolean_t res = FALSE;

	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
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
	sccp_session_t *session;
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
			AUTO_RELEASE(sccp_device_t, remDevice , NULL);
			remDevice = __sccp_session_removeDevice(session);		/* implicit release */
		}
		if (device) {
			if (new_device) {
				session->device = new_device;				/* keep newly retained device */
				session->device->session = session;			/* update device session pointer */

				char buf[16] = "";
				snprintf(buf,16, "%s:%d", device->id, session->fds[0].fd);
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
		sccp_session_t * s = (sccp_session_t *)session;								/* discard const */
		sccp_log((DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Allocating device to session (%d) %s\n", DEV_ID_LOG(device), s->fds[0].fd, sccp_netsock_stringify_addr(&s->sin));
		return __sccp_session_addDevice(s, device);
	}
	return 0;
}


void sccp_session_releaseDevice(constSessionPtr volatile session)
{
	sccp_session_t * s = (sccp_session_t *)session;									/* discard const */
	if (s) {
		AUTO_RELEASE(sccp_device_t, device , NULL);
		device = __sccp_session_removeDevice(s);
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
static void destroy_session(sccp_session_t * s, uint8_t cleanupTime)
{
	if (!s) {
		return;
	}

	char addrStr[INET6_ADDRSTRLEN];
	sccp_copy_string(addrStr, sccp_netsock_stringify_addr(&s->sin), sizeof(addrStr));
	AUTO_RELEASE(sccp_device_t, d , s->device ? sccp_device_retain(s->device) : NULL);
	if (d) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Destroy Device Session %s\n", DEV_ID_LOG(s->device), addrStr);
		if (d->session) {
			d->session = NULL;
			sccp_dev_clean(d, (d->realtime) ? TRUE : FALSE);
		} else {
			sccp_session_releaseDevice(s);
		}
	}

	if (!sccp_session_removeFromGlobals(s)) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session could not be found in GLOB(session) %s\n", DEV_ID_LOG(s->device), addrStr);
	}
	
	if (s) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Destroy Session %s\n", addrStr);
		/* closing fd's */
		sccp_session_lock(s);
		if (s->fds[0].fd > 0) {
			close(s->fds[0].fd);
			s->fds[0].fd = -1;
		}
		sccp_session_unlock(s);

		/* destroying mutex and cleaning the session */
		sccp_mutex_destroy(&s->lock);
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
void sccp_netsock_device_thread_exit(void *session)
{
	sccp_session_t *s = (sccp_session_t *) session;

	if (!s->device) {
		sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_3 "SCCP: Session without a device attached !\n");
	}

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: cleanup session\n", DEV_ID_LOG(s->device));
	sccp_session_lock(s);
	s->session_stop = TRUE;
	if (s->fds[0].fd > 0) {
		close(s->fds[0].fd);
		s->fds[0].fd = -1;
	}
	sccp_session_unlock(s);
	s->session_thread = AST_PTHREADT_NULL;
	destroy_session(s, SESSION_DEVICE_CLEANUP_TIME);
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
	s->keepAlive = (keepAlive * keepaliveAdditionalTimePercent);
	s->keepAliveInterval = (keepAliveInterval * KEEPALIVE_ADDITIONAL_PERCENT_SESSION);

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
void *sccp_netsock_device_thread(void *session)
{
	int res;
	sccp_session_t *s = (sccp_session_t *) session;

	if (!s) {
		return NULL;
	}

	boolean_t oncall = TRUE;
	boolean_t tokenThread = FALSE;
	unsigned char recv_buffer[SCCP_MAX_PACKET * 2] = "";
	size_t recv_len = 0;
	sccp_msg_t msg = { {0,} };

	pthread_cleanup_push(sccp_netsock_device_thread_exit, session);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (s->fds[0].fd > 0 && !s->session_stop) {
		if (s->device) {
			sccp_device_t *d = s->device;
			if (d->pendingUpdate || d->pendingDelete) {
				pbx_rwlock_rdlock(&GLOB(lock));
				boolean_t reload_in_progress = GLOB(reload_in_progress);
				pbx_rwlock_unlock(&GLOB(lock));
				if (reload_in_progress == FALSE) {
					sccp_device_check_update(d);
				}
				continue;									// make sure  s->device is still valid
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
		sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "%s: set poll timeout %d for session %d\n", DEV_ID_LOG(s->device), (int) s->keepAliveInterval, s->fds[0].fd);

		res = sccp_netsock_poll(s->fds, 1, s->keepAliveInterval * 1000);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (-1 == res) {										/* poll data processing */
			if (errno > 0 && (errno != EAGAIN) && (errno != EINTR)) {
				pbx_log(LOG_ERROR, "%s: poll() returned %d. errno: %s, (ip-address: %s)\n", DEV_ID_LOG(s->device), errno, strerror(errno), s->designator);
				socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__, errno);
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
			if (s->fds[0].revents & POLLIN || s->fds[0].revents & POLLPRI) {			/* POLLIN | POLLPRI */
				//sccp_log_and((DEBUGCAT_SOCKET + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Session New Data Arriving at buffer position:%lu\n", DEV_ID_LOG(s->device), recv_len);
				int result = recv(s->fds[0].fd, recv_buffer + recv_len, (SCCP_MAX_PACKET * 2) - recv_len, 0);
				s->lastKeepAlive = time(0);
				if (result <= 0) {
					if (result < 0 || (errno != EINTR || errno != EAGAIN)) {
						socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__, errno);
						break;
					}				
				} else if (!((recv_len += result) && ((SCCP_MAX_PACKET * 2) - recv_len) && process_buffer(s, &msg, recv_buffer, &recv_len) == 0)) {
					pbx_log(LOG_ERROR, "%s: (netsock_device_thread) Received a packet or message (with result:%d) which we could not handle, giving up session: %p!\n", s->designator, result, s);
					sccp_dump_msg(&msg);
					if (s->device) {
						sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
					}
					__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
					break;
				}
				s->lastKeepAlive = time(0);
			} else {										/* POLLHUP / POLLERR */
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
void __sccp_session_stopthread(sessionPtr session, uint8_t newRegistrationState)
{
	if (!session) {
		pbx_log(LOG_NOTICE, "SCCP: session already terminated\n");
		return;
	}
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "%s: Stopping Session Thread\n", DEV_ID_LOG(session->device));

	session->session_stop = TRUE;
	if (session->device) {
		sccp_device_setRegistrationState(session->device, newRegistrationState);
	}
	if (AST_PTHREADT_NULL != session->session_thread) {
		shutdown(session->fds[0].fd, SHUT_RD);								// this will also wake up poll
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
	void *res;
	if (pthread_join(session_thread, &res) == 0) {
		if (res != PTHREAD_CANCELED) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_netsock_end_device_thread) pthread join failed\n");
		}
	}
}

/* check if same or different thread, choose thread cancel method accordingly */
gcc_inline void sccp_session_stopthread(constSessionPtr session, uint8_t newRegistrationState)
{
	sccp_session_t * s = (sccp_session_t *)session;								/* discard const */
	if (s) {
		pthread_t ptid = pthread_self();
		if (ptid == s->session_thread) {
			__sccp_session_stopthread(s, newRegistrationState);
		} else {
			__sccp_netsock_end_device_thread(s);
		}
	}
}

/*!
 * \brief Socket Accept Connection
 *
 * \lock
 *      - sessions
 */
static void sccp_accept_connection(void)
{
	/* called without GLOB(sessions_lock) */
	struct sockaddr_storage incoming;
	sccp_session_t *s;
	int new_socket;
	char addrStr[INET6_ADDRSTRLEN];

	socklen_t length = (socklen_t) (sizeof(struct sockaddr_storage));

	if (!(s = sccp_calloc(sizeof *s, 1))) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}

	if ((new_socket = accept(GLOB(descriptor), (struct sockaddr *) &incoming, &length)) < 0) {
		pbx_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		sccp_free(s);
		return;
	}
	sccp_netsock_setoptions(new_socket, /*reuse*/ 0, /*linger*/ 0, GLOB(keepalive) * 2);
	
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	sccp_mutex_init(&s->lock);

	s->fds[0].events = POLLIN | POLLPRI;
	s->fds[0].revents = 0;
	s->fds[0].fd = new_socket;

	if (!GLOB(ha)) {
		pbx_log(LOG_NOTICE, "No global ha list\n");
	}

	sccp_copy_string(addrStr, sccp_netsock_stringify(&s->sin), sizeof(addrStr));

	/* check ip address against global permit/deny ACL */
	if (GLOB(ha) && sccp_apply_ha(GLOB(ha), &s->sin) != AST_SENSE_ALLOW) {
		struct ast_str *buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		if (buf) {
			sccp_print_ha(buf, DEFAULT_PBX_STR_BUFFERSIZE, GLOB(ha));
			sccp_log(0) ("SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", addrStr, pbx_str_buffer(buf));
			pbx_log(LOG_WARNING, "SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", addrStr, pbx_str_buffer(buf));
		} else {
			pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		}
		sccp_session_reject(s, "Device ip not authorized");
		destroy_session(s, 0);
		return;
	}
	sccp_session_addToGlobals(s);

	/** set default handler for registration to sccp */
	s->protocolType = SCCP_PROTOCOL;
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted Client Connection from %s\n", addrStr);

	if (sccp_netsock_is_any_addr(&GLOB(bindaddr))) {
		__sccp_session_setOurAddressFromTheirs(&incoming, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr), sizeof(s->ourip));
	}
	sccp_copy_string(s->designator, sccp_netsock_stringify(&s->ourip), sizeof(s->designator));
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Connected on server via %s\n", s->designator);
	s->lastKeepAlive = time(0);
	recalc_wait_time(s);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pbx_pthread_create(&s->session_thread, &attr, sccp_netsock_device_thread, s);

	/*
	size_t stacksize = 0;
	if (!pthread_attr_getstacksize(&attr, &stacksize)) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Using %d memory for this thread\n", (int) stacksize);
	}
	*/
}

static void sccp_netsock_cleanup_timed_out(void)
{
	sccp_session_t *session;

	pbx_rwlock_rdlock(&GLOB(lock));
	boolean_t reload_in_progress = GLOB(reload_in_progress);
	boolean_t module_running = GLOB(module_running);
	pbx_rwlock_unlock(&GLOB(lock));
	if (module_running && !reload_in_progress) {
		SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), session, list) {
			if ((time(0) - session->lastKeepAlive) > (5 * GLOB(keepalive)) && (session->session_thread != AST_PTHREADT_NULL)) {
				__sccp_netsock_end_device_thread(session);
			}
		}
		SCCP_LIST_TRAVERSE_SAFE_END;
	}
}


/*!
 * \brief Socket Thread
 * \param ignore None
 *
 * \lock
 *      - sessions
 *      - globals
 *	- see sccp_device_check_update()
 *	- see sccp_netsock_poll()
 *	- see destroy_session()
 *	- see sccp_read_data()
 *	- see sccp_process_data()
 *	- see sccp_handle_message()
 *	- see sccp_device_sendReset()
 */
void *sccp_netsock_thread(void * ignore)
{
	struct pollfd fds[1];
	fds[0].events = POLLIN | POLLPRI;
	fds[0].revents = 0;
	fds[0].fd = GLOB(descriptor);

	int res = 0;
	int pollTimeout = GLOB(keepalive) * 1000;
	boolean_t reload_in_progress = FALSE;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while (GLOB(descriptor) > -1) {
		res = sccp_netsock_poll(fds, 1, pollTimeout);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (res < 0) {
			if (!(errno == EINTR || errno == EAGAIN)) {
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s)\n", res, errno, strerror(errno));
				break;
			}
		} else if (res == 0) {
			sccp_netsock_cleanup_timed_out();
		} else {
			pbx_rwlock_rdlock(&GLOB(lock));
			reload_in_progress = GLOB(reload_in_progress);
			pollTimeout = GLOB(keepalive) * 1000;
			pbx_rwlock_unlock(&GLOB(lock));
			if (!reload_in_progress) {
				sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accept Connection\n");
				sccp_accept_connection();
			}
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel();
	}
	pbx_rwlock_wrlock(&GLOB(lock));
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	close(GLOB(descriptor));
	GLOB(descriptor) = -1;
	pbx_rwlock_unlock(&GLOB(lock));

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Exit from the socket thread\n");
	return NULL;
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
	sccp_session_t * const s = (sessionPtr) session;								/* discard const */
	ssize_t res = 0;
	uint32_t msgid = letohl(msg->header.lel_messageId);
	ssize_t bytesSent;
	ssize_t bufLen;
	uint8_t *bufAddr;

	if (s && s->session_stop) {
		return -1;
	}

	if (!s || s->fds[0].fd <= 0) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		if (s) {
			__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
		}
		sccp_free(msg);
		msg = NULL;
		return -1;
	}
	int mysocket = s->fds[0].fd;

	if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage || msgid == UnregisterAckMessage) {
		msg->header.lel_protocolVer = 0;
	} else if (s->device && s->device->protocol) {
		msg->header.lel_protocolVer = s->device->protocol->version < 10 ? 0 : htolel(s->device->protocol->version);
	} else {
		msg->header.lel_protocolVer = 0;
	}

	if (msg && (GLOB(debug) & DEBUGCAT_MESSAGE) != 0) {
		uint32_t mid = letohl(msg->header.lel_messageId);

		pbx_log(LOG_NOTICE, "%s: Send Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(s->device), msgtype2str(mid), mid, msg->header.length);
		sccp_dump_msg(msg);
	}

	uint backoff = WRITE_BACKOFF;
	bytesSent = 0;
	bufAddr = ((uint8_t *) msg);
	bufLen = (ssize_t) (letohl(msg->header.length) + 8);
	do {
		pbx_mutex_lock(&s->write_lock);									/* prevent two threads writing at the same time. That should happen in a synchronized way */
		res = send(mysocket, bufAddr + bytesSent, bufLen - bytesSent, 0);
		pbx_mutex_unlock(&s->write_lock);
		if (res <= 0) {
			if (errno == EINTR) {
				usleep(backoff);								/* back off to give network/other threads some time */
				backoff *= 2;
				continue;
			}
			socket_get_error(s, __FILE__, __LINE__, __PRETTY_FUNCTION__, errno);
			if (s) {
				__sccp_session_stopthread(s, SKINNY_DEVICE_RS_FAILED);
			}
			res = -1;
			break;
		}
		bytesSent += res;
	} while (bytesSent < bufLen && s && !s->session_stop && mysocket > 0);

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
	sccp_session_t * const s = (sccp_session_t * const) session;			/* discard const */

	REQ(msg, RegisterRejectMessage);
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
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_2 "%s: Previous session %p needs to be cleaned up and killed!\n", current_session->designator, previous_session->designator);
		__sccp_netsock_end_device_thread(previous_session);
	}
	return;
}

gcc_inline boolean_t sccp_session_check_crossdevice(constSessionPtr session, constDevicePtr device)
{
	if (session && device && ((session->device && session->device != device) || (device->session && device->session != session))) {
		pbx_log(LOG_WARNING, "Session(%p) and Device Session(%p) are of sync.\n", session, device->session);
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
	sccp_session_t * s = (sccp_session_t *)session;								/* discard const */

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
	sccp_session_t * s = (sccp_session_t *)session;								/* discard const */

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
gcc_inline sccp_device_t * const sccp_session_getDevice(constSessionPtr session, boolean_t required)
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
	if (session && session->fds[0].fd > 0 && !session->session_stop && !sccp_netsock_is_any_addr(&session->ourip)) {
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
#define CLI_AMI_TABLE_BEFORE_ITERATION 														\
		sccp_session_lock(session);													\
		sccp_copy_string(clientAddress, sccp_netsock_stringify_addr(&session->sin), sizeof(clientAddress));				\
		AUTO_RELEASE(sccp_device_t, d , session->device ? sccp_device_retain(session->device) : NULL);								\
		if (d || (argc == 4 && sccp_strcaseequals(argv[3],"all"))) {									\

#define CLI_AMI_TABLE_AFTER_ITERATION 														\
		}																\
		sccp_session_unlock(session);													\

#define CLI_AMI_TABLE_FIELDS 															\
		CLI_AMI_TABLE_FIELD(Socket,		"-6",		d,	6,	session->fds[0].fd)					\
		CLI_AMI_TABLE_FIELD(IP,			"40.40",	s,	40,	clientAddress)						\
		CLI_AMI_TABLE_FIELD(Port,		"-5",		d,	5,	sccp_netsock_getPort(&session->sin) )    		\
		CLI_AMI_TABLE_FIELD(KA,			"-4",		d,	4,	(uint32_t) (time(0) - session->lastKeepAlive))		\
		CLI_AMI_TABLE_FIELD(DKAI,		"-4",		d,	4,	(d ? d->keepaliveinterval : GLOB(keepalive)))		\
		CLI_AMI_TABLE_FIELD(KAMAX,		"-5",		d,	5,	session->keepAlive)					\
		CLI_AMI_TABLE_FIELD(DeviceName,		"15",		s,	15,	(d) ? d->id : "--")					\
		CLI_AMI_TABLE_FIELD(State,		"-14.14",	s,	14,	(d) ? sccp_devicestate2str(sccp_device_getDeviceState(d)) : "--")		\
		CLI_AMI_TABLE_FIELD(Type,		"-15.15",	s,	15,	(d) ? skinny_devicetype2str(d->skinny_type) : "--")	\
		CLI_AMI_TABLE_FIELD(RegState,		"-10.10",	s,	10,	(d) ? skinny_registrationstate2str(sccp_device_getRegistrationState(d)) : "--")	\
		CLI_AMI_TABLE_FIELD(Token,		"-10.10",	s,	10,	d ? sccp_tokenstate2str(d->status.token) : "--")
#include "sccp_cli_table.h"

	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
