
/*!
 * \file        sccp_socket.c
 * \brief       SCCP Socket Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_socket.h"
#include "sccp_device.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#ifndef CS_USE_POLL_COMPAT
#include <poll.h>
#include <sys/poll.h>
#else
#define AST_POLL_COMPAT 1
#include <asterisk/poll-compat.h>
#endif
#ifdef pbx_poll
#define sccp_socket_poll pbx_poll
#else
#define sccp_socket_poll poll
#endif
sccp_session_t *sccp_session_findByDevice(const sccp_device_t * device);

void destroy_session(sccp_session_t * s, uint8_t cleanupTime);
void sccp_session_close(sccp_session_t * s);
void sccp_socket_device_thread_exit(void *session);
void *sccp_socket_device_thread(void *session);

boolean_t socket_is_IPv6(struct sockaddr_storage *socketStorage)
{
	return (socketStorage->ss_family == AF_INET6) ? TRUE : FALSE;
}

boolean_t socket_is_mapped_ipv4(struct sockaddr_storage *socketStorage)
{
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) socketStorage;

	return IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr);
}

/*!
 * \brief Exchange Socket Addres Information from them to us
 */
static int sccp_socket_getOurAddressfor(struct in_addr *them, struct in_addr *us)
{
	int s;
	struct sockaddr_in sin;
	socklen_t slen;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		return -1;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = GLOB(bindaddr.sin_port);
	sin.sin_addr = *them;
	if (connect(s, (struct sockaddr *) &sin, sizeof(sin))) {
		return -1;
	}
	slen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *) &sin, &slen)) {
		close(s);
		return -1;
	}
	close(s);
	//      *us = sin.sin_addr;
	memcpy(us, &sin.sin_addr, sizeof(struct in_addr));
	return 0;
}

void sccp_socket_stop_sessionthread(sccp_session_t * session, uint8_t newRegistrationState)
{
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "%s: Stopping Session Thread\n", DEV_ID_LOG(session->device));
	if (!session) {
		pbx_log(LOG_NOTICE, "SCCP: session already terminated\n");
		return;
	}

	session->session_stop = 1;
	if (session->device) {
		session->device->registrationState = newRegistrationState;
	}
	if (AST_PTHREADT_NULL != session->session_thread) {
		shutdown(session->fds[0].fd, SHUT_RD);								// this will also wake up poll
		// which is waiting for a read event and close down the thread nicely
	}
}

static int sccp_dissect_header(sccp_session_t * s, sccp_header_t *header) 
{
	int packetSize = header->length;
	int protocolVersion = letohl(header->lel_protocolVer);
	int messageId = letohl(header->lel_messageId);

	// dissecting header to see if we have a valid sccp message, that we can handle
	if (packetSize < 4 || packetSize > SCCP_MAX_PACKET - 8) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) Size of the data payload in the packet is out of bounds: %d < %d > %d, cancelling read.\n", 4, packetSize, (int)(SCCP_MAX_PACKET - 8));
		return -1;
	}
	if ( protocolVersion > 0 && !(sccp_protocol_isProtocolSupported(s->protocolType, protocolVersion)) ) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) protocolversion %d is unknown, cancelling read.\n", protocolVersion);
		return -1;
	}
	
	if (messageId < SCCP_MESSAGE_LOW_BOUNDARY || messageId > SCCP_MESSAGE_HIGH_BOUNDARY) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) messageId out of bounds: %d < %d > %d, cancelling read.\n", SCCP_MESSAGE_LOW_BOUNDARY, messageId, SCCP_MESSAGE_HIGH_BOUNDARY);
		return -1;
	}
	if (DEBUG) {
		boolean_t Found = FALSE;
		uint32_t x;
		for (x = 0; x < ARRAY_LEN(sccp_messagetypes); x++) {
			if (messageId == x) {
				Found = TRUE;
				break;
			}
		}
		if (!Found) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) messageId %d could not be found in the array of known messages, cancelling read.\n", messageId);
			//return -1;										/* not returning -1 in this case, so that we can see the message we would otherwise miss */
		}
	}

	return msgtype2size(messageId);
}

/*!
 * \brief Read Data From Socket
 * \param s SCCP Session
 *
 * \lock
 *      - session
 */
static boolean_t sccp_read_data(sccp_session_t * s, sccp_msg_t *msg)
{
	if (!s || s->session_stop) {
		return 0;
	}

	int msgDataSegmentSize = 0;										/* Size of sccp_data_t according to the sccp_msg_t */
	int UnreadBytesAccordingToPacket = 0;									/* Size of sccp_data_t according to the incomming Packet */
	int bytesToRead = 0;
	int bytesReadSoFar = 0;
	int readlen = 0;
	errno = 0;

	// STAGE 1: read header
	memset(msg, 0, SCCP_MAX_PACKET);	
	readlen = read(s->fds[0].fd, (&msg->header), SCCP_PACKET_HEADER);
	if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) { return TRUE;}					/* try again later, return TRUE with empty r */
	else if (readlen <= 0) {goto READ_ERROR;}								/* client closed socket */
	
	msg->header.length = letohl(msg->header.length);
	if ((msgDataSegmentSize = sccp_dissect_header(s, &msg->header)) < 0) {
		goto READ_ERROR;
	}
	
	// STAGE 2: read message data
	UnreadBytesAccordingToPacket = msg->header.length - 4;							/* correcttion because we count messageId as part of the header */
	bytesToRead = (UnreadBytesAccordingToPacket > msgDataSegmentSize) ? msgDataSegmentSize : UnreadBytesAccordingToPacket;	/* take the smallest of the two */
	while (bytesToRead >0) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Reading %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, bytesToRead, bytesReadSoFar);
//		readlen = read(s->fds[0].fd, &msg->data + bytesReadSoFar, bytesToRead);				/* msg->data pointer is advanced by read automatically. This will lead to Bad Address faults */
		readlen = read(s->fds[0].fd, &msg->data, bytesToRead);						
		if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) {continue;}				/* try again, blocking */
		else if (readlen <= 0) {goto READ_ERROR;}							/* client closed socket */
		bytesReadSoFar += readlen;
		bytesToRead -= readlen;
	};
	UnreadBytesAccordingToPacket -= bytesReadSoFar;
	// msg->header.length = bytesReadSoFar + 4;								/* Should we correct the header->length, to the length of our struct, which we know we can handle ?? */
	
	sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Finished Reading %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, bytesToRead, bytesReadSoFar);
	
	// STAGE 3: discard the rest of UnreadBytesAccordingToPacket if it was bigger then msgDataSegmentSize
	if (UnreadBytesAccordingToPacket > 0) { 								/* checking to prevent unneeded allocation of discardBuffer */
		pbx_log(LOG_NOTICE, "%s: Going to Discard %d bytes of data for '%s' (%d) (Needs developer attention)\n", DEV_ID_LOG(s->device), UnreadBytesAccordingToPacket, msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId);
		char discardBuffer[128];
		bytesToRead = UnreadBytesAccordingToPacket;
		while (bytesToRead > 0) {
			readlen= read(s->fds[0].fd, discardBuffer, (bytesToRead > sizeof(discardBuffer)) ? sizeof(discardBuffer) : bytesToRead);
			if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) {continue;}			/* try again, blocking */
// \todo //		else if (readlen == 0) {goto READ_ERROR;}						/* client closed socket, because read caused an error */
/* \todo */		else if (readlen <= 0) {break;}								/* if we read EOF, just break reading (invalid packet information) */
														/* would EOF not return <0 && errno, instead */
			bytesToRead -= readlen;
			sccp_dump_packet((unsigned char *)discardBuffer, readlen);				/* dump the discarded bytes, for analysis */
		};
		pbx_log(LOG_NOTICE, "%s: Discarded %d bytes of data for '%s' (%d) (Needs developer attention)\nReturning Only:\n", DEV_ID_LOG(s->device), UnreadBytesAccordingToPacket, msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId);
		sccp_dump_msg(msg);
	}

	return TRUE;
	
READ_ERROR:
	if (errno) {
		pbx_log(LOG_ERROR, "%s: error '%s (%d)' while reading from socket.\n", DEV_ID_LOG(s->device), strerror(errno), errno);
	}
	memset(msg, 0, SCCP_MAX_PACKET);	
	return FALSE;
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
 * \brief Retain device pointer in session. Replace existing pointer if necessary
 * \param session SCCP Session
 * \param device SCCP Device
 */
sccp_device_t *sccp_session_addDevice(sccp_session_t * session, sccp_device_t * device)
{
	if (session && device && session->device != device) {
		sccp_session_lock(session);
		if (session->device) {
			sccp_device_t *remdevice = sccp_device_retain(session->device);

			sccp_session_removeDevice(session);
			remdevice = sccp_device_release(remdevice);
		}
		if ((session->device = sccp_device_retain(device))) {
			session->device->session = session;
		}
		sccp_session_unlock(session);
	}
	return (session && session->device) ? session->device : NULL;
}

/*!
 * \brief Release device pointer from session
 * \param session SCCP Session
 */
sccp_device_t *sccp_session_removeDevice(sccp_session_t * session)
{
	if (session && session->device) {
		if (session->device->session && session->device->session != session) {
			// cleanup previous/crossover session
			sccp_session_removeFromGlobals(session->device->session);
		}
		sccp_session_lock(session);
		session->device->registrationState = SKINNY_DEVICE_RS_NONE;
		session->device->session = NULL;
		session->device = sccp_device_release(session->device);
		sccp_session_unlock(session);
	}
	return NULL;
}

/*!
 * \brief Socket Session Close
 * \param s SCCP Session
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 *      - see sccp_hint_eventListener() via sccp_event_fire()
 *      - session
 */
void sccp_session_close(sccp_session_t * s)
{

	sccp_session_lock(s);
	s->session_stop = 1;
	if (s->fds[0].fd > 0) {
		close(s->fds[0].fd);
		s->fds[0].fd = -1;
	}
	sccp_session_unlock(s);
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Old session marked down\n", DEV_ID_LOG(s->device));
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
void destroy_session(sccp_session_t * s, uint8_t cleanupTime)
{
	sccp_device_t *d = NULL;
	boolean_t found_in_list = FALSE;

	if (!s) {
		return;
	}

	found_in_list = sccp_session_removeFromGlobals(s);

	if (!found_in_list) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session could not be found in GLOB(session) %s\n", DEV_ID_LOG(s->device), pbx_inet_ntoa(s->sin.sin_addr));
	}

	/* cleanup device if this session is not a crossover session */
	if (s->device && (d = sccp_device_retain(s->device))) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Destroy Device Session %s\n", DEV_ID_LOG(s->device), pbx_inet_ntoa(s->sin.sin_addr));
		d->registrationState = SKINNY_DEVICE_RS_NONE;
		d->needcheckringback = 0;
		sccp_dev_clean(d, (d->realtime) ? TRUE : FALSE, cleanupTime);
		sccp_device_release(d);
	}

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Destroy Session %s\n", pbx_inet_ntoa(s->sin.sin_addr));
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

/*!
 * \brief Socket Device Thread Exit
 * \param session SCCP Session
 *
 * \callgraph
 * \callergraph
 */
void sccp_socket_device_thread_exit(void *session)
{
	sccp_session_t *s = (sccp_session_t *) session;

	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: cleanup session\n", DEV_ID_LOG(s->device));
	sccp_session_close(s);
	s->session_thread = AST_PTHREADT_NULL;
	destroy_session(s, 10);
}

/*!
 * \brief Socket Device Thread
 * \param session SCCP Session
 *
 * \callgraph
 * \callergraph
 */
void *sccp_socket_device_thread(void *session)
{
	sccp_session_t *s = (sccp_session_t *) session;
	uint8_t keepaliveAdditionalTimePercent = 10;
	int res;
	double maxWaitTime;
	int pollTimeout;
	sccp_msg_t msg = { {0,} };

	pthread_cleanup_push(sccp_socket_device_thread_exit, session);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	/* we increase additionalTime for wireless/slower devices */
	if (s->device && (s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7920 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7921 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7925 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO6911)
	    ) {
		keepaliveAdditionalTimePercent += 10;
	}

	while (s->fds[0].fd > 0 && !s->session_stop) {
		/* create cancellation point */
		pthread_testcancel();
		if (s->device) {
			pbx_mutex_lock(&GLOB(lock));
			if (GLOB(reload_in_progress) == FALSE && s && s->device && (!(s->device->pendingUpdate == FALSE && s->device->pendingDelete == FALSE))) {
				sccp_device_check_update(s->device);
			}
			pbx_mutex_unlock(&GLOB(lock));
		}
		/* calculate poll timout using keepalive interval */
		maxWaitTime = (s->device) ? s->device->keepalive : GLOB(keepalive);
		maxWaitTime += (maxWaitTime / 100) * keepaliveAdditionalTimePercent;
		pollTimeout = maxWaitTime * 1000;

		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "%s: set poll timeout %d/%d for session %d\n", DEV_ID_LOG(s->device), (int) maxWaitTime, pollTimeout / 1000, s->fds[0].fd);

		pthread_testcancel();										/* poll is also a cancellation point */
		res = sccp_socket_poll(s->fds, 1, pollTimeout);
		pthread_testcancel();
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if (-1 == res) {										/* poll data processing */
			if (errno > 0 && (errno != EAGAIN) && (errno != EINTR)) {
				pbx_log(LOG_ERROR, "%s: poll() returned %d. errno: %s, (ip-address: %s)\n", DEV_ID_LOG(s->device), errno, strerror(errno), pbx_inet_ntoa(s->sin.sin_addr));
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
				break;
			}
		} else if (0 == res) {										/* poll timeout */
			sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Poll Timeout.\n", DEV_ID_LOG(s->device));
			if (((int) time(0) > ((int) s->lastKeepAlive + (int) maxWaitTime))) {
				ast_log(LOG_NOTICE, "%s: Closing session because connection timed out after %d seconds (timeout: %d).\n", DEV_ID_LOG(s->device), (int) maxWaitTime, pollTimeout);
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_TIMEOUT);
				break;
			}
		} else if (res > 0) {										/* poll data processing */
			if (s->fds[0].revents & POLLIN || s->fds[0].revents & POLLPRI) {			/* POLLIN | POLLPRI */
				/* we have new data -> continue */
				sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Session New Data Arriving\n", DEV_ID_LOG(s->device));
				//while (sccp_read_data(s, &r)) {						/* according to poll specification we should empty out the read buffer completely.*/
														/* but that would give us trouble with timeout */
				if (sccp_read_data(s, &msg) && msg.header.length >= 4) {
					if (!sccp_handle_message(&msg, s)) {
						if (s->device) {
							sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
						}
						sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
						break;
					}
					s->lastKeepAlive = time(0);
				} else {
					pbx_log(LOG_NOTICE, "%s: Socket read failed\n", DEV_ID_LOG(s->device));
					sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
					break;
				}
				//}
			} else {										/* POLLHUP / POLLERR */
				pbx_log(LOG_NOTICE, "%s: Closing session because we received POLLPRI/POLLHUP/POLLERR\n", DEV_ID_LOG(s->device));
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
				break;
			}
		} else {											/* poll returned invalid res */
			sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Poll Returned invalid result: %d.\n", DEV_ID_LOG(s->device), res);
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Exiting sccp_socket device thread\n", DEV_ID_LOG(s->device));

	pthread_cleanup_pop(1);

	return NULL;
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
	struct sockaddr_in incoming;

	sccp_session_t *s;

	int new_socket;

	socklen_t length = (socklen_t) (sizeof(struct sockaddr_in));

	int on = 1;

	if ((new_socket = accept(GLOB(descriptor), (struct sockaddr *) &incoming, &length)) < 0) {
		pbx_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		return;
	}
	//      if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	//              pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	//      }
	if (setsockopt(new_socket, IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
	}
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
	}
#if defined(linux)
	if (setsockopt(new_socket, SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
	}
#endif

	s = sccp_malloc(sizeof(struct sccp_session));
	memset(s, 0, sizeof(sccp_session_t));
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	sccp_mutex_init(&s->lock);

	sccp_session_lock(s);
	s->fds[0].events = POLLIN | POLLPRI;
	s->fds[0].revents = 0;
	s->fds[0].fd = new_socket;

	if (!GLOB(ha)) {
		pbx_log(LOG_NOTICE, "No global ha list\n");

	}

	/* check ip address against global permit/deny ACL */
	if (GLOB(ha) && sccp_apply_ha(GLOB(ha), &s->sin) != AST_SENSE_ALLOW) {
		struct ast_str *buf = pbx_str_alloca(512);

		sccp_print_ha(buf, sizeof(buf), GLOB(ha));
		sccp_log((DEBUGCAT_CORE)) ("SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", pbx_inet_ntoa(s->sin.sin_addr), pbx_str_buffer(buf));
		pbx_log(LOG_WARNING, "SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", pbx_inet_ntoa(s->sin.sin_addr), pbx_str_buffer(buf));
		sccp_session_reject(s, "Device ip not authorized");
		sccp_session_unlock(s);
		destroy_session(s, 0);
		return;
	}
	sccp_session_addToGlobals(s);
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));

	/** set default handler for registration to sccp */
	s->protocolType = SCCP_PROTOCOL;

	s->lastKeepAlive = time(0);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		sccp_socket_getOurAddressfor(&incoming.sin_addr, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr.sin_addr.s_addr), sizeof(s->ourip));
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", pbx_inet_ntoa(s->ourip));

	size_t stacksize = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pbx_pthread_create(&s->session_thread, &attr, sccp_socket_device_thread, s);
	if (!pthread_attr_getstacksize(&attr, &stacksize)) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Using %d memory for this thread\n", (int) stacksize);
	}
	sccp_session_unlock(s);
}


/*!
 * \brief Socket Thread
 * \param ignore None
 *
 * \lock
 *      - sessions
 *        - globals
 *          - see sccp_device_check_update()
 *        - see sccp_socket_poll()
 *        - see sccp_session_close()
 *        - see destroy_session()
 *        - see sccp_read_data()
 *        - see sccp_process_data()
 *        - see sccp_handle_message()
 *        - see sccp_device_sendReset()
 */
void *sccp_socket_thread(void *ignore)
{
	struct pollfd fds[1];

	fds[0].events = POLLIN | POLLPRI;
	fds[0].revents = 0;

	int res;

	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	while (GLOB(descriptor) > -1) {
		fds[0].fd = GLOB(descriptor);
		res = sccp_socket_poll(fds, 1, SCCP_SOCKET_ACCEPT_TIMEOUT);

		if (res < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s) -- ignoring.\n", res, errno, strerror(errno));
			} else {
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s)\n", res, errno, strerror(errno));
			}

		} else if (res == 0) {
			// poll timeout
		} else {
			if (GLOB(module_running)) {
				sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accept Connection\n");
				sccp_accept_connection();
			}
		}
	}

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
 * \param r Message Data Structure (sccp_msg_t)
 * \return SCCP Session Send
 */
int sccp_session_send(const sccp_device_t * device, sccp_msg_t * msg)
{
	sccp_session_t *s = sccp_session_findByDevice(device);

	if (s && !s->session_stop) {
		return sccp_session_send2(s, msg);
	} else {
		return -1;
	}
}

/*!
 * \brief Socket Send Message
 * \param s Session SCCP Session (can't be null)
 * \param r Message SCCP Moo Message (will be freed)
 * \return Result as Int
 *
 * \lock
 *      - session
 */
int sccp_session_send2(sccp_session_t * s, sccp_msg_t * msg)
{
	ssize_t res = 0;
	uint32_t msgid = letohl(msg->header.lel_messageId);
	ssize_t bytesSent;
	ssize_t bufLen;
	uint8_t *bufAddr;
	unsigned int try, maxTries;;

	if (s && s->session_stop) {
		return -1;
	}

	if (!s || s->fds[0].fd <= 0) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		if (s) {
			sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
		}
		sccp_free(msg);
		msg = NULL;
		return -1;
	}

	if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage || msgid == UnregisterAckMessage) {
		msg->header.lel_protocolVer = 0;
	} else if (s->device && s->device->inuseprotocolversion >= 17) {
		msg->header.lel_protocolVer = htolel(0x11);							/* we should always send 0x11 */
	} else {
		msg->header.lel_protocolVer = 0;
	}

	try = 0;
	maxTries = 500;												/* arbitrairy number of tries */
	bytesSent = 0;
	bufAddr = ((uint8_t *) msg);
	bufLen = (ssize_t) (letohl(msg->header.length) + 8);
	do {
		try++;
		ast_mutex_lock(&s->write_lock);									/* prevent two threads writing at the same time. That should happen in a synchronized way */
		res = write(s->fds[0].fd, bufAddr + bytesSent, bufLen - bytesSent);
		ast_mutex_unlock(&s->write_lock);
		if (res < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				usleep(200);									/* back off to give network/other threads some time */
				continue;
			}
			pbx_log(LOG_ERROR, "%s: write returned %d (error: '%s (%d)'). Sent %d of %d for Message: '%s' with total length %d \n", DEV_ID_LOG(s->device), (int) res, strerror(errno), errno, (int) bytesSent, (int) bufLen, msgtype2str(letohl(msg->header.lel_messageId)), letohl(msg->header.length) );
			if (s) {
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
			}
			res = -1;
			break;
		}
		bytesSent += res;
	} while (bytesSent < bufLen && try < maxTries && s && !s->session_stop && s->fds[0].fd > 0);

	sccp_free(msg);
	msg = NULL;

	if (bytesSent < bufLen) {
		ast_log(LOG_ERROR, "%s: Could only send %d of %d bytes!\n", DEV_ID_LOG(s->device), (int) bytesSent, (int) bufLen);
		res = -1;
	}

	return res;
}

/*!
 * \brief Find session for device
 * \param device SCCP Device
 * \return SCCP Session
 *
 * \lock
 *      - sessions
 */
sccp_session_t *sccp_session_findByDevice(const sccp_device_t * device)
{
	if (!device) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: (sccp_session_find) No device available to find session\n");
		return NULL;
	}

	return device->session;
}

static sccp_session_t *sccp_session_findSessionForDevice(const sccp_device_t * device)
{
	sccp_session_t *session;

	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), session, list) {
		if (session->device == device) {
			break;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;

	return session;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param message Message as char (reason of rejection)
 */
sccp_session_t *sccp_session_reject(sccp_session_t * session, char *message)
{
	sccp_msg_t *msg;

	REQ(msg, RegisterRejectMessage);
	sccp_copy_string(msg->data.RegisterRejectMessage.text, message, sizeof(msg->data.RegisterRejectMessage.text));
	sccp_session_send2(session, msg);

	/* if we reject the connection during accept connection, thread is not ready */
	sccp_socket_stop_sessionthread(session, SKINNY_DEVICE_RS_FAILED);
	return NULL;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param device SCCP Device Pointer
 * \param message Message as char (reason of rejection)
 */
sccp_session_t *sccp_session_crossdevice_cleanup(sccp_session_t * session, sccp_device_t * device, char *message)
{
	sccp_dev_displayprinotify(device, message, 0, 2);
	sccp_device_sendReset(device, SKINNY_DEVICE_RESTART);

	/* if we reject the connection during accept connection, thread is not ready */
	sccp_socket_stop_sessionthread(session, SKINNY_DEVICE_RS_NONE);

	return NULL;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param backoff_time Time to Backoff before retrying TokenSend
 */
void sccp_session_tokenReject(sccp_session_t * session, uint32_t backoff_time)
{
	sccp_msg_t *msg;

	REQ(msg, RegisterTokenReject);
	msg->data.RegisterTokenReject.lel_tokenRejWaitTime = htolel(backoff_time);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send a token acknowledgement.
 * \param session SCCP Session Pointer
 */
void sccp_session_tokenAck(sccp_session_t * session)
{
	sccp_msg_t *msg;

	REQ(msg, RegisterTokenAck);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send an Reject Message to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenRejectSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_msg_t *msg;

	REQ(msg, SPCPRegisterTokenReject);
	msg->data.SPCPRegisterTokenReject.lel_features = htolel(features);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Send a token acknowledgement to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenAckSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_msg_t *msg;

	REQ(msg, SPCPRegisterTokenAck);
	msg->data.SPCPRegisterTokenAck.lel_features = htolel(features);
	sccp_session_send2(session, msg);
}

/*!
 * \brief Get the in_addr for Specific Device.
 * \param device SCCP Device Pointer (can be null)
 * \param type Type in {AF_INET, AF_INET6}
 * \return In Address as Structure
 */
struct in_addr *sccp_session_getINaddr(sccp_device_t * device, int type)
{
	sccp_session_t *s = sccp_session_findByDevice(device);

	if (!s) {
		return NULL;
	}

	switch (type) {
		case AF_INET:
			return &s->sin.sin_addr;
		case AF_INET6:
			//return &s->sin6.sin6_addr;
			return NULL;
		default:
			return NULL;
	}
}

void sccp_session_getSocketAddr(const sccp_device_t * device, struct sockaddr_in *sin)
{
	sccp_session_t *s = sccp_session_findSessionForDevice(device);

	if (!s) {
		return;
	}

	memcpy(sin, &s->sin, sizeof(struct sockaddr_in));
}
