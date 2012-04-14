
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
#if !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(SOLARIS) && !defined(__Darwin__)
#    include <sys/poll.h>
#else
#    include <asterisk/poll-compat.h>
#endif
#ifdef ast_poll
#    define sccp_socket_poll ast_poll
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
			ast_log(LOG_WARNING, "SCCP: FIONREAD Come back later (EAGAIN): %s\n", strerror(errno));
		} else {
			ast_log(LOG_WARNING, "SCCP: FIONREAD ioctl failed: %s\n", strerror(errno));
			pthread_cancel(s->session_thread);
		}
		return;
	}

	length = (int16_t) bytesAvailable;

	if (0 == length)
		length = 1;

	input = ast_malloc(length + 1);

	readlen = read(s->fds[0].fd, input, length);
	if (readlen <= 0) {
		if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) {
			ast_log(LOG_WARNING, "SCCP: FIONREAD Come back later (EAGAIN): %s\n", strerror(errno));
		} else {
			/* probably a CLOSE_WAIT (readlen==0 || errno == ECONNRESET || errno == ETIMEDOUT) */
			ast_log(LOG_NOTICE, "SCCP: socket read returned zero length, either device disconnected or network disconnect. Closing Connection.\n");
			pthread_cancel(s->session_thread);
		}
		ast_free(input);
		return;
	}

	newptr = ast_realloc(s->buffer, (uint32_t) (s->buffer_size + readlen));
	if (newptr) {
		s->buffer = newptr;
		memcpy(s->buffer + s->buffer_size, input, readlen);
		s->buffer_size += readlen;
	} else {
		ast_log(LOG_WARNING, "SCCP: unable to reallocate %d bytes for skinny a packet\n", s->buffer_size + readlen);
		ast_free(s->buffer);
		s->buffer_size = 0;
	}

	ast_free(input);
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

	if (s->device) {
		sccp_event_t *event = ast_malloc(sizeof(sccp_event_t));

		memset(event, 0, sizeof(sccp_event_t));
		event->type = SCCP_EVENT_DEVICE_UNREGISTERED;
		event->event.deviceRegistered.device = s->device;
		sccp_event_fire((const sccp_event_t **)&event);
	}

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
 * \param cleanupTime Clean up time in uint8_t
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

	if (d) {
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
		ast_free(s->buffer);

	/* destroying mutex and cleaning the session */
	sccp_mutex_destroy(&s->lock);
	ast_free(s);
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

	sccp_moo_t *m;

	pthread_cleanup_push(sccp_socket_device_thread_exit, session);

	/* we increase additionalTime for wireless/slower devices */
	if (s->device && (s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7920 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7921 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7925 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 || s->device->skinny_type == SKINNY_DEVICETYPE_CISCO6911)) {
		keepaliveAdditionalTime += 10;
	}

	while (!s->session_stop) {
		/* create cancellation point */
		pthread_testcancel();

#ifdef CS_DYNAMIC_CONFIG
		if (s->device) {
			ast_mutex_lock(&GLOB(lock));
			if (GLOB(reload_in_progress) == FALSE)
				sccp_device_check_update(s->device);
			ast_mutex_unlock(&GLOB(lock));
		}
#endif
		if (s->fds[0].fd > 0) {
			/* calculate poll timout using keepalive interval */
			maxWaitTime = (s->device) ? s->device->keepalive : GLOB(keepalive);
			maxWaitTime += keepaliveAdditionalTime;
			pollTimeout = maxWaitTime * 1000;			// in ms

			sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: set poll timeout %d\n", DEV_ID_LOG(s->device), pollTimeout);

			res = sccp_socket_poll(s->fds, 1, pollTimeout);
			if (res < 0) {						/* poll error */
				/* Sparc64 + OpenBSD socket implementation can be interupted via system irq, ignore and try again */
				if ((errno != EAGAIN) && (errno != EINTR)) {
					ast_log(LOG_ERROR, "%s: socket_poll() returned %d. errno: %d (%s)\n", DEV_ID_LOG(s->device), res, errno, strerror(errno));
                                        if (s->device && s->device->registrationState)
                                                s->device->registrationState = SKINNY_DEVICE_RS_FAILED;
					s->session_stop = 1;
					break;
				}
				continue;
			} else if (res == 0) {					/* poll timeout */
				ast_log(LOG_NOTICE, "%s: Closing session because connection timed out after %d seconds (timeout: %d).\n", DEV_ID_LOG(s->device), (int)maxWaitTime, pollTimeout);
				if (s->device && s->device->registrationState)
				        s->device->registrationState = SKINNY_DEVICE_RS_TIMEOUT;
				s->session_stop = 1;
				break;
			}

			if (s->fds[0].revents) {				/* handle poll events a.k.a. data processing */
				sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "%s: Session New Data Arriving\n", DEV_ID_LOG(s->device));
				sccp_read_data(s);
				while ((m = sccp_process_data(s))) {
					if (!sccp_handle_message(m, s)) {
						sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
                                                if (s->device && s->device->registrationState)
                                                        s->device->registrationState = SKINNY_DEVICE_RS_FAILED;
						s->session_stop = 1;
						break;
					}
				}
			}
		} else {							/* session is gone */
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session is Gone\n", DEV_ID_LOG(s->device));
			if (s->device && s->device->registrationState)
			        s->device->registrationState = SKINNY_DEVICE_RS_TIMEOUT;
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
		ast_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		return;
	}

	if (ioctl(new_socket, FIONBIO, &dummy) < 0) {
		ast_log(LOG_ERROR, "Couldn't set socket to non-blocking\n");
		close(new_socket);
		return;
	}

	if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	if (setsockopt(new_socket, IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
#if defined(linux)
	if (setsockopt(new_socket, SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
#endif

	s = ast_malloc(sizeof(struct sccp_session));
	memset(s, 0, sizeof(sccp_session_t));
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	sccp_mutex_init(&s->lock);

	s->fds[0].events = POLLIN | POLLPRI;
	s->fds[0].revents = 0;
	s->fds[0].fd = new_socket;

	s->lastKeepAlive = time(0);

	/* check ip address against global permit/deny ACL*/
	if (GLOB(ha) && (AST_SENSE_ALLOW != pbx_apply_ha(GLOB(ha), &s->sin) )) {
		ast_log(LOG_NOTICE, "Rejecting device: Ip address '%s' denied\n", pbx_inet_ntoa(s->sin.sin_addr));
		sccp_session_reject(s, "Device ip not authorized");
		return;
	} else {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));
	}

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		ast_ouraddrfor(&incoming.sin_addr, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr.sin_addr.s_addr), sizeof(s->ourip));
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", pbx_inet_ntoa(s->ourip));

	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
	SCCP_LIST_INSERT_HEAD(&GLOB(sessions), s, list);
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));

	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	ast_pthread_create(&s->session_thread, &attr, sccp_socket_device_thread, s);
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

	if (packSize > SCCP_MAX_PACKET) {
		ast_log(LOG_WARNING, "SCCP: Oversize packet: our size: %zd, phone size: %d\n", SCCP_MAX_PACKET, packSize);
		return NULL;
	}

	m = ast_malloc(packSize);
	if (!m) {
		ast_log(LOG_WARNING, "SCCP: unable to allocate %zd bytes for skinny packet\n", SCCP_MAX_PACKET);
		return NULL;
	}

	memset(m, 0, packSize);

	memcpy(m, s->buffer, packSize);
	m->length = letohl(m->length);

	s->buffer_size -= packSize;

	if (s->buffer_size) {
		newptr = ast_malloc(s->buffer_size);
		if (newptr)
			memcpy(newptr, (s->buffer + packSize), s->buffer_size);
		else
			ast_log(LOG_WARNING, "SCCP: unable to allocate %d bytes for packets buffer\n", s->buffer_size);
	}

	if (s->buffer)
		ast_free(s->buffer);

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
		res = sccp_socket_poll(fds, 1, -1);

		if (res < 0) {
			/* Sparc64 + OpenBSD socket implementation can be interupted via system irq, ignore and try again */
			if ((errno != EAGAIN) && (errno != EINTR)) {
				ast_log(LOG_ERROR, "SCCP poll() returned %d. errno: %d (%s)\n", res, errno, strerror(errno));
				return NULL;
			}
			continue;
		} else if (res == 0) {
			/* we should not get here, polling timeout is -1, meaning don't timeout for as long as the socket exists */
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP (global sccp_socket_thread): Poll Timeout\n");
		}

		if (fds[0].revents) {
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accept New Device Connection\n");
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
	if (!device || !device->session) {
        	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: (sccp_session_sendmsg) No device available to send message to\n");
		return;
        }

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
	ssize_t res;
	uint32_t msgid = letohl(r->lel_messageId);
	ssize_t bytesSent;
	ssize_t bufLen;
	uint8_t *bufAddr;
	unsigned int try, maxTries;;

	if (!s || s->fds[0].fd <= 0 || s->fds[0].revents & POLLHUP || s->session_stop) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		ast_free(r);
		r = NULL;
		if (s) {
                        ast_log(LOG_WARNING, "%s. closing connection", DEV_ID_LOG(s->device));
                        if (s->device && s->device->registrationState)
                                s->device->registrationState = SKINNY_DEVICE_RS_FAILED;
                        s->session_stop = 1;
                        pthread_cancel(s->session_thread);
                        s->session_thread = AST_PTHREADT_NULL;
                }
		return -1;
	}
	sccp_session_lock(s);

	//sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET)?r->length:SCCP_MAX_PACKET);

	if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage || msgid == UnregisterAckMessage) {
		r->lel_reserved = 0;
	} else if (s->device && s->device->inuseprotocolversion >= 17) {
		r->lel_reserved = htolel(0x11);					// we should always send 0x11
	} else {
		r->lel_reserved = 0;
	}

	res = 0;
	try = 1;
	maxTries = 50;
	bytesSent = 0;
	bufAddr = ((uint8_t *) r);
	bufLen = (ssize_t) (letohl(r->length) + 8);
/*	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Sending Packet Type %s (%d bytes)\n", DEV_ID_LOG(s->device), message2str(letohl(r->lel_messageId)), letohl(r->length));*/
	do {
		res = write(s->fds[0].fd, bufAddr + bytesSent, bufLen - bytesSent);
                if (res < 0) { 
                        if (errno != EINTR && errno != EAGAIN) {
                                ast_log(LOG_WARNING, "%s: write returned %d (error: '%s'). Sent %d of %d for Message: '%s' with total length %d \n", DEV_ID_LOG(s->device), (int)res, strerror(errno), (int)bytesSent, (int)bufLen, message2str(letohl(r->lel_messageId)), letohl(r->length));
                                sccp_dump_packet((unsigned char *)&r->msg, (r->length < SCCP_MAX_PACKET)?r->length:SCCP_MAX_PACKET);
                                if (s) {
                                        ast_log(LOG_WARNING, "%s. closing connection", DEV_ID_LOG(s->device));
                                        if (s->device && s->device->registrationState)
                                                s->device->registrationState = SKINNY_DEVICE_RS_FAILED;
                                        s->session_stop = 1;
                                        pthread_cancel(s->session_thread);
                                        s->session_thread = AST_PTHREADT_NULL;
                                }
                                break;
                        } 
                        usleep(50);		// try sending more data
                        continue;
                }
		bytesSent += res;
		try++;
	} while (bytesSent < bufLen && try < maxTries && s && !s->session_stop && s->fds[0].fd > 0);

	sccp_session_unlock(s);
	ast_free(r);
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
	if (!device) {
        	sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: (sccp_session_find) No device available to find session\n");
		return NULL;
        }
        
	return device->session;
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

        /* if we reject the connction during accept connection, thread is not ready */ 
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
