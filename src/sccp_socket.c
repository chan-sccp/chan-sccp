
/*!
 * \file 	sccp_socket.c
 * \brief 	SCCP Socket Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note		Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include <sys/ioctl.h>
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS)
#    include <sys/poll.h>
#else
#    include <asterisk/poll-compat.h>
#endif

#ifdef pbx_poll
#    define sccp_socket_poll pbx_poll
#else
#    define sccp_socket_poll poll
#endif
sccp_session_t *sccp_session_find(const sccp_device_t * device);

void destroy_session(sccp_session_t * s, uint8_t cleanupTime);

void sccp_session_close(sccp_session_t * s);

void sccp_socket_device_thread_exit(void *session);

int sccp_session_send2(sccp_session_t * s, sccp_moo_t * r);

void *sccp_socket_device_thread(void *session);

static sccp_moo_t *sccp_process_data(sccp_session_t * s);

/*!
 * \brief Exchange Socket Addres Information from them to us
 */
static int sccp_sockect_getOurAddressfor(struct in_addr *them, struct in_addr *us)
{
	int s;
	struct sockaddr_in sin;
	socklen_t slen;

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		return -1;
	}
	sin.sin_family = AF_INET;
//	sin.sin_port = htons(GLOB(ourport));
	sin.sin_port = GLOB(bindaddr.sin_port);
	sin.sin_addr = *them;
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin))) {
		return -1;
	}
	slen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *)&sin, &slen)) {
		close(s);
		return -1;
	}
	close(s);
	*us = sin.sin_addr;
	return 0;
}

/*!
 * \brief Read Data From Socket
 * \param s SCCP Session
 * 
 * \lock
 * 	- session
 */
static void sccp_read_data(sccp_session_t * s)
{
	int bytesAvailable;

	int16_t length, readlen;

	char *input, *newptr;

	if (ioctl(s->fds[0].fd, FIONREAD, &bytesAvailable) == -1) {
		if (errno == EAGAIN) {
			pbx_log(LOG_WARNING, "SCCP: FIONREAD Come back later (EAGAIN): %s\n", strerror(errno));
		} else {
			pbx_log(LOG_WARNING, "SCCP: FIONREAD ioctl failed: %s\n", strerror(errno));
			pthread_cancel(s->session_thread);
		}
		return;
	}

	length = (int16_t) bytesAvailable;

	input = sccp_malloc(length + 1);
	if (!input) {
		pbx_log(LOG_WARNING, "SCCP: unable to allocate %d bytes for skinny a packet\n", length+1);
		sccp_free(input);
		return;
	}

	readlen = read(s->fds[0].fd, input, length);
	if (readlen <= 0) {
		if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) {
			pbx_log(LOG_WARNING, "SCCP: FIONREAD Come back later (EAGAIN): %s\n", strerror(errno));
		} else {
			/* probably a CLOSE_WAIT (readlen==0 || errno == ECONNRESET || errno == ETIMEDOUT) */
			pbx_log(LOG_WARNING, "SCCP: read() returned zero length. Assuming closed connection. %d\n", s->fds[0].revents);
			pthread_cancel(s->session_thread);
		}
		sccp_free(input);
		return;
	}

	newptr = sccp_realloc(s->buffer, (uint32_t) (s->buffer_size + readlen));
	if (newptr) {
		s->buffer = newptr;
		memcpy(s->buffer + s->buffer_size, input, readlen);
		s->buffer_size += readlen;
	} else {
		pbx_log(LOG_WARNING, "SCCP: unable to reallocate %d bytes for skinny a packet\n", s->buffer_size + readlen);
		sccp_free(s->buffer);
		s->buffer_size = 0;
	}

	sccp_free(input);
}

/*!
 * \brief Socket Session Close
 * \param s SCCP Session
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- see sccp_hint_eventListener() via sccp_event_fire()
 * 	- session
 */
void sccp_session_close(sccp_session_t * s)
{
	if (!s)
		return;

	// fire event to set device unregistered
/*	if (s->device) {
		sccp_event_t *event = sccp_malloc(sizeof(sccp_event_t));

		memset(event, 0, sizeof(sccp_event_t));
		event->type = SCCP_EVENT_DEVICE_UNREGISTERED;
		event->event.deviceRegistered.device = s->device;
		sccp_event_fire((const sccp_event_t **)&event);
	}
*/
	sccp_session_lock(s);
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
 * 	- sessions
 * 	- device
 */
void destroy_session(sccp_session_t * s, uint8_t cleanupTime)
{
	sccp_device_t *d;

	if (!s)
		return;

	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
	SCCP_LIST_REMOVE(&GLOB(sessions), s, list);
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));

	d = s->device;

	/* cleanup device if this session is not a crossover session */
	if (d && (d->session == s || NULL == d->session) ) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Killing Session %s\n", DEV_ID_LOG(d), pbx_inet_ntoa(s->sin.sin_addr));
		sccp_device_lock(d);
		d->session = NULL;
		d->registrationState = SKINNY_DEVICE_RS_NONE;
		d->needcheckringback = 0;
		sccp_device_unlock(d);
		sccp_dev_clean(d, (d->realtime) ? TRUE : FALSE, cleanupTime);
	}

	/* closing fd's */
	if (s->fds[0].fd > 0) {
		close(s->fds[0].fd);
	}
	/* freeing buffers */
	if (s->buffer)
		sccp_free(s->buffer);

	/* destroying mutex and cleaning the session */
	sccp_mutex_destroy(&s->lock);
	sccp_free(s);
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

	s->session_thread = AST_PTHREADT_NULL;
	sccp_session_close(s);
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

	uint8_t keepaliveAdditionalTime = 10;

	int res;

	double maxWaitTime;

	int pollTimeout;

	time_t now;

	sccp_moo_t *m;

	pthread_cleanup_push(sccp_socket_device_thread_exit, session);

	/* we increase additionalTime for wireless/slower devices */
	if (s->device && (s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7920 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7921 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7925 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7970)) {
		keepaliveAdditionalTime += 10;
	}

	while (!s->session_stop) {
		/* create cancellation point */
		pthread_testcancel();

#ifdef CS_DYNAMIC_CONFIG
		if (s->device) {
			pbx_mutex_lock(&GLOB(lock));
			if (GLOB(reload_in_progress) == FALSE)
				sccp_device_check_update(s->device);
			pbx_mutex_unlock(&GLOB(lock));
		}
#endif
		if (s->fds[0].fd > 0) {
			/* calculate poll timout using keepalive interval */
			now = time(0);
			maxWaitTime = (s->device) ? s->device->keepalive : GLOB(keepalive);
			maxWaitTime += keepaliveAdditionalTime;

			/* our calculation was in seconds, we need msecs */
			pollTimeout = (int)(maxWaitTime - ((int)now - (int)s->lastKeepAlive)) * 1000;

			if (pollTimeout < 0) {
				s->session_stop = 1;
				break;
			}

			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: set poll timeout %d\n", DEV_ID_LOG(s->device), pollTimeout);

			res = sccp_socket_poll(s->fds, 1, pollTimeout);
			if (res > 0) {						/* poll data processing */
				/* we have new data -> continue */
				sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session New Data Arriving\n", DEV_ID_LOG(s->device));
				sccp_read_data(s);
				while ((m = sccp_process_data(s))) {
					if (!sccp_handle_message(m, s)) {
						sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
						s->session_stop = 1;
						break;
					}
					sccp_free(m);
				}
			} else if (res == 0) {					/* poll timeout */
				now = time(0);
				if (s->device && s->device->keepalive && (now > ((s->lastKeepAlive + s->device->keepalive) + keepaliveAdditionalTime))) {
					sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session Keepalive %s Expired, now %s\n", DEV_ID_LOG(s->device), ctime(&s->lastKeepAlive), ctime(&now));
					pbx_log(LOG_WARNING, "%s: Dead device does not send a keepalive message in %d+%d seconds. Will be removed\n", DEV_ID_LOG(s->device), GLOB(keepalive), keepaliveAdditionalTime);
					s->session_stop = 1;
					break;
				}
			} else {						/* poll error */
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %s\n", errno, strerror(errno));
				s->session_stop = 1;
				break;
			}
		} else {							/* session is gone */
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session is Gone\n", DEV_ID_LOG(s->device));
			s->session_stop = 1;
			break;
		}
	}
	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: exiting thread\n", DEV_ID_LOG(s->device));

	pthread_cleanup_pop(1);

	return NULL;
}

/*!
 * \brief Socket Accept Connection
 *
 * \lock
 * 	- sessions
 */
static void sccp_accept_connection(void)
{
	/* called without GLOB(sessions_lock) */
	struct sockaddr_in incoming;

	sccp_session_t *s;

	int dummy = 1, new_socket;

	socklen_t length = (socklen_t) (sizeof(struct sockaddr_in));

	int on = 1;

	if ((new_socket = accept(GLOB(descriptor), (struct sockaddr *)&incoming, &length)) < 0) {
		pbx_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		return;
	}

	if (ioctl(new_socket, FIONBIO, &dummy) < 0) {
		pbx_log(LOG_ERROR, "Couldn't set socket to non-blocking\n");
		close(new_socket);
		return;
	}

	if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	if (setsockopt(new_socket, IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0)
		pbx_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		pbx_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
#if defined(linux)
	if (setsockopt(new_socket, SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0)
		pbx_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
#endif

	s = sccp_malloc(sizeof(struct sccp_session));
	memset(s, 0, sizeof(sccp_session_t));
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	sccp_mutex_init(&s->lock);
	
	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
	SCCP_LIST_INSERT_HEAD(&GLOB(sessions), s, list);
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));
	

	s->fds[0].events = POLLIN | POLLPRI;
	s->fds[0].revents = 0;
	s->fds[0].fd = new_socket;
	
	if(!GLOB(ha)){
		ast_log(LOG_NOTICE, "No global ha list\n");
	  
	}
	
	/* check ip address against global permit/deny ACL*/
	if ( GLOB(ha) && sccp_apply_ha(GLOB(ha), &s->sin) != AST_SENSE_ALLOW ) {
		ast_log(LOG_NOTICE, "Reject Connection: Ip-address '%s' denied. Check general permit settings.\n", pbx_inet_ntoa(s->sin.sin_addr));
		s = sccp_session_reject(s, "Device ip not authorized");
		return;
	} 
	
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));
	/** set default handler for registration to sccp */
	s->protocolType = SCCP_PROTOCOL;

	s->lastKeepAlive = time(0);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		sccp_sockect_getOurAddressfor(&incoming.sin_addr, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr.sin_addr.s_addr), sizeof(s->ourip));
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", pbx_inet_ntoa(s->ourip));

	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	pbx_pthread_create(&s->session_thread, &attr, sccp_socket_device_thread, s);
}

/*!
 * \brief Socket Process Data
 * \param s SCCP Session
 * \note Called with mutex lock
 */
static sccp_moo_t *sccp_process_data(sccp_session_t * s)
{
	uint32_t packSize;

	void *newptr = NULL;

	sccp_moo_t *m;

	/* Notice: If the buffer length read so far
	   is smaller than the length field + some data of at least on byte,
	   we need and must not parse the packet length yet . (-DD) */
	if (s->buffer_size <= 4)
		return NULL;

	memcpy(&packSize, s->buffer, 4);
	packSize = letohl(packSize);

	packSize += 8;

	if (s->buffer_size < 0 || (packSize) > (uint32_t) s->buffer_size)
		return NULL;							/* Not enough data, yet. */

	m = sccp_malloc(SCCP_MAX_PACKET);
	if (!m) {
		pbx_log(LOG_WARNING, "SCCP: unable to allocate %zd bytes for skinny packet\n", SCCP_MAX_PACKET);
		return NULL;
	}

	memset(m, 0, SCCP_MAX_PACKET);

	memcpy(m, s->buffer, (packSize < SCCP_MAX_PACKET ? packSize : SCCP_MAX_PACKET));
	m->length = letohl(m->length);
	s->buffer_size -= packSize;
	
	if (packSize > SCCP_MAX_PACKET)
		pbx_log(LOG_WARNING, "SCCP: Oversize packet mid: %d, our packet size: %zd, phone packet size: %d\n", letohl(m->lel_messageId), SCCP_MAX_PACKET, packSize);


	if (s->buffer_size) {
		newptr = sccp_malloc(s->buffer_size);
		if (newptr)
			memcpy(newptr, (s->buffer + packSize), s->buffer_size);
		else
			pbx_log(LOG_WARNING, "SCCP: unable to allocate %d bytes for packets buffer\n", s->buffer_size);
	}

	if (s->buffer)
		sccp_free(s->buffer);

	s->buffer = newptr;

	return m;
}

/*!
 * \brief Socket Thread
 * \param ignore None
 * 
 * \lock
 * 	- sessions
 * 	  - globals
 * 	    - see sccp_device_check_update()
 * 	  - see sccp_socket_poll()
 * 	  - see sccp_session_close()
 * 	  - see destroy_session()
 * 	  - see sccp_read_data()
 * 	  - see sccp_process_data()
 * 	  - see sccp_handle_message()
 * 	  - see sccp_device_sendReset()
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
		res = sccp_socket_poll(fds, 1, 2000);

		if (res < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s) -- ignoring.\n", res, errno, strerror(errno));
			} else {
				pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s)\n", res, errno, strerror(errno));
//                      usleep(10000);
//                      return NULL;
			}

		} else if (res == 0) {
			// poll timeout
			//pbx_log(LOG_WARNING, "SCCP: Poll TimeOut\n");
		} else {
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accept Connection\n");
			sccp_accept_connection();
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
void sccp_session_sendmsg(const sccp_device_t * device, sccp_message_t t)
{
	if (!device || !device->session)
		return;

	sccp_moo_t *r = sccp_build_packet(t, 0);

	if (r)
		sccp_session_send(device, r);
}

/*!
 * \brief Socket Send
 * \param device SCCP Device
 * \param r Message Data Structure (sccp_moo_t)
 * \return SCCP Session Send
 */
int sccp_session_send(const sccp_device_t * device, sccp_moo_t * r)
{
	sccp_session_t *s = sccp_session_find(device);

	return sccp_session_send2(s, r);
}

/*!
 * \brief Socket Send Message
 * \param s Session SCCP Session (can't be null)
 * \param r Message SCCP Moo Message (will be freed)
 * \return Result as Int
 *
 * \lock
 * 	- session
 */
int sccp_session_send2(sccp_session_t * s, sccp_moo_t * r)
{
	ssize_t res=0;

	uint32_t msgid = letohl(r->lel_messageId);

	ssize_t bytesSent;

	ssize_t bufLen;

	uint8_t *bufAddr;

	boolean_t finishSending;

	unsigned int try, maxTries;;

	if (!s || s->fds[0].fd <= 0 || s->fds[0].revents & POLLHUP) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		sccp_free(r);
		r = NULL;
		return -1;
	}
	sccp_session_lock(s);

	//sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET)?r->length:SCCP_MAX_PACKET);

	if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage) {
		r->lel_reserved = 0;
	} else if (s->device && s->device->inuseprotocolversion >= 17) {
		r->lel_reserved = htolel(0x11);					// we should always send 0x11
	} else {
		r->lel_reserved = 0;
	}

//	res = 0;
	finishSending = 0;
	try = 1;
	maxTries = 500;
	bytesSent = 0;
	bufAddr = ((uint8_t *) r);
	bufLen = (ssize_t) (letohl(r->length) + 8);
	/* sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Sending Packet Type %s (%d bytes)\n", s->device->id, message2str(letohl(r->lel_messageId)), letohl(r->length)); */
	do {
		res = write(s->fds[0].fd, bufAddr + bytesSent, bufLen - bytesSent);
		if (res >= 0) {
			bytesSent += res;
		}
		if ((bytesSent == bufLen) || (try >= maxTries)) {
			finishSending = 1;
		} else {
			usleep(10);
		}
		try++;
	} while (!finishSending);

	sccp_session_unlock(s);
	sccp_free(r);

	if (bytesSent < bufLen) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Could only send %d of %d bytes!\n", s->device->id, (int)bytesSent, (int)bufLen);
		sccp_session_close(s);
		return 0;
	}

	return res;
}

/*!
 * \brief Find session for device
 * \param device SCCP Device
 * \return SCCP Session
 * 
 * \lock
 * 	- sessions
 */
sccp_session_t *sccp_session_find(const sccp_device_t * device)
{
	if (!device)
		return NULL;

	return device->session;
}


static sccp_session_t *sccp_session_findSessionForDevice(const sccp_device_t * device)
{
	sccp_session_t *session;
  
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), session, list) {
		if ( session->device == device) {
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
	sccp_moo_t *r;

	REQ(r, RegisterRejectMessage);
	sccp_copy_string(r->msg.RegisterRejectMessage.text, message, sizeof(r->msg.RegisterRejectMessage.text));
	sccp_session_send2(session, r);
	
	
	/* if we reject the connction during acceppt connection, thread is not ready */
	if(session->session_thread){
		pthread_cancel(session->session_thread);
		session->session_thread = AST_PTHREADT_NULL;
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: use thread cleanup\n", DEV_ID_LOG(session->device));
	}else{
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: no thread\n", DEV_ID_LOG(session->device));
		sccp_session_close(session);
		destroy_session(session, 0);
	}
	
	return NULL;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param backoff_time Time to Backoff before retrying TokenSend
 */
void sccp_session_tokenReject(sccp_session_t * session, uint32_t backoff_time)
{
	sccp_moo_t *r;

	REQ(r, RegisterTokenReject);
	r->msg.RegisterTokenReject.lel_tokenRejWaitTime = htolel(backoff_time);
	sccp_session_send2(session, r);
}

/*!
 * \brief Send a token acknowledgement.
 * \param session SCCP Session Pointer
 */
void sccp_session_tokenAck(sccp_session_t * session)
{
	sccp_moo_t *r;

	REQ(r, RegisterTokenAck);
	sccp_session_send2(session, r);
}

/*!
 * \brief Send an Reject Message to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features
 */
void sccp_session_tokenRejectSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_moo_t *r;

	REQ(r, SPCPRegisterTokenReject);
	r->msg.SPCPRegisterTokenReject.lel_features=htolel(features);
	sccp_session_send2(session, r);
}


/*!
 * \brief Send a token acknowledgement to the SPCP Device.
 * \param session SCCP Session Pointer
 */
void sccp_session_tokenAckSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_moo_t *r;
	REQ(r, SPCPRegisterTokenAck);
	r->msg.SPCPRegisterTokenAck.lel_features=htolel(features);
	sccp_session_send2(session, r);
}

/*!
 * \brief Get the in_addr for Specific Device.
 * \param device SCCP Device Pointer (can be null)
 * \param type Type in {AF_INET, AF_INET6}
 * \return In Address as Structure
 */
struct in_addr *sccp_session_getINaddr(sccp_device_t * device, int type)
{
	sccp_session_t *s = sccp_session_find(device);

	if (!s)
		return NULL;

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


void sccp_session_getSocketAddr(const sccp_device_t *device, struct sockaddr_in *sin)
{
	sccp_session_t *s = sccp_session_findSessionForDevice(device);

	if (!s)
		return;
	
	memcpy(sin, &s->sin, sizeof(struct sockaddr_in));
}
