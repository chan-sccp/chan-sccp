/*!
 * \file 	sccp_socket.c
 * \brief 	SCCP Socket Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$  
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_event.h"
#include "sccp_lock.h"
#include "sccp_line.h"
#include "sccp_socket.h"
#include "sccp_device.h"
#include "sccp_utils.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <asterisk/utils.h>

sccp_session_t * sccp_session_find(const sccp_device_t *device);

int sccp_session_send2(sccp_session_t *s, sccp_moo_t * r);

/* file descriptors of active sockets */
static fd_set	active_fd_set;


/*!
 * \brief Read Data From Socket
 * \param s SCCP Session
 */
static void sccp_read_data(sccp_session_t * s)
{
	int64_t ioctl_len;
	int32_t length, readlen;
	char * input, * newptr;

	/* called while we have GLOB(sessions) list lock */
	sccp_session_lock(s);

	if (ioctl(s->fd, FIONREAD, &ioctl_len) == -1) {
		ast_log(LOG_WARNING, "SCCP: FIONREAD ioctl failed: %s\n", strerror(errno));
		sccp_session_unlock(s);
		sccp_session_close(s);
		return;
	}

	length = (int32_t)ioctl_len;

	if (!length) {
		/* probably a CLOSE_WAIT */
		sccp_session_unlock(s);
		sccp_session_close(s);
		return;
	}

	input = ast_malloc(length + 1);
/*	memset(input, 0, length+1); */

	if ((readlen = read(s->fd, input, length)) < 0) {
		ast_log(LOG_WARNING, "SCCP: read() returned %s\n", strerror(errno));
		ast_free(input);
		sccp_session_unlock(s);
		sccp_session_close(s);
		return;
	}

	/* It is not sensible to make the following, commented out check,
	   since the length of available data could have changed
	   since the previous FIONREAD ioctl anyway.
	   What matters is simply that there was data available,
	   and there probably will be still until we read it. */

	/* Suggestion: We should create some mechanism to assemble
	   sccp packets from several incomplete reads,
	   but this is rather a high level feature. (-DD) */

	/*
	if (readlen != length) {
		ast_log(LOG_WARNING, "SCCP: read() returned %d, wanted %d: %s\n", readlen, length, strerror(errno));
		ast_free(input);
		sccp_session_unlock(s);
		sccp_session_close(s);
		return;
	}*/

	newptr = realloc(s->buffer, (uint32_t)(s->buffer_size + readlen));
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

	sccp_session_unlock(s);
}


/*!
 * \brief Socket Session Close
 * \param s SCCP Session
 */
void sccp_session_close(sccp_session_t * s)
{
	if (!s)
		return;

	// fire event for new device
	sccp_event_t *event =ast_malloc(sizeof(sccp_event_t));
	memset(event, 0, sizeof(sccp_event_t));

	event->type=SCCP_EVENT_DEVICEUNREGISTERED;
	event->event.deviceRegistered.device = s->device;
	sccp_event_fire( (const sccp_event_t **)&event);

	sccp_session_lock(s);
	if (s->fd > 0) {
		FD_CLR(s->fd, &active_fd_set);
		close(s->fd);
		s->fd = -1;
	}
	sccp_session_unlock(s);

	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Old session marked down\n", DEV_ID_LOG(s->device));

}

/*!
 * \brief Destroy Socket Session
 * \param s SCCP Session
 */
static void destroy_session(sccp_session_t * s)
{
	sccp_device_t * d;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	if (!s)
		return;

	d = s->device;
	if (d)
	{
#ifdef ASTERISK_CONF_1_2
		sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Killing Session %s\n", DEV_ID_LOG(d), ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else
		sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Killing Session %s\n", DEV_ID_LOG(d), ast_inet_ntoa(s->sin.sin_addr));
#endif

		sccp_dev_clean(d, (d->realtime)?TRUE:FALSE, 10);
	}

	/* remove the session from global list*/
	SCCP_LIST_LOCK(&GLOB(sessions));
	SCCP_LIST_REMOVE(&GLOB(sessions), s, list);
	SCCP_LIST_UNLOCK(&GLOB(sessions));

	/* closing fd's */
	if (s->fd > 0) {
		FD_CLR(s->fd, &active_fd_set);
		close(s->fd);
	}
	/* freeing buffers */
	if (s->buffer)
		ast_free(s->buffer);

	/* destroying mutex and cleaning the session */
	sccp_mutex_destroy(&s->lock);
	ast_free(s);
	s=NULL;
}


/*!
 * \brief Socket Accept Connection
 */
static void sccp_accept_connection(void)
{
	/* called without GLOB(sessions_lock) */
	struct sockaddr_in incoming;
	sccp_session_t * s;
	int dummy = 1, new_socket;
	socklen_t length = (socklen_t)(sizeof(struct sockaddr_in));
	int on = 1;

#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

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
                                                
/*
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_CORK, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_CORK: %s\n", strerror(errno));
*/

	s = ast_malloc(sizeof(struct sccp_session));
	memset(s, 0, sizeof(sccp_session_t));
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	sccp_mutex_init(&s->lock);

	s->fd = new_socket;
	s->lastKeepAlive = time(0);
#ifdef ASTERISK_CONF_1_2
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", ast_inet_ntoa(s->sin.sin_addr));
#endif

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		ast_ouraddrfor(&incoming.sin_addr, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr.sin_addr.s_addr), sizeof(s->ourip));
	}

#ifdef ASTERISK_CONF_1_2
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), s->ourip));
#else
	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", ast_inet_ntoa(s->ourip));
#endif

	SCCP_LIST_LOCK(&GLOB(sessions));
	SCCP_LIST_INSERT_HEAD(&GLOB(sessions), s, list);
	SCCP_LIST_UNLOCK(&GLOB(sessions));
}

/*!
 * \brief Socket Process Data
 * \param s SCCP Session
 * \note Called with mutex lock
 */
static sccp_moo_t * sccp_process_data(sccp_session_t * s)
{
	uint32_t packSize;
	void * newptr = NULL;
	sccp_moo_t * m;

	if (s->buffer_size == 0)
		return NULL;

	memcpy(&packSize, s->buffer, 4);
	packSize = letohl(packSize);

	packSize += 8;

	if ((packSize) > s->buffer_size)
		return NULL; /* Not enough data, yet. */

	m = ast_malloc(SCCP_MAX_PACKET);
	if (!m) {
		ast_log(LOG_WARNING, "SCCP: unable to allocate %zd bytes for skinny packet\n", SCCP_MAX_PACKET);
		return NULL;
	}

	memset(m, 0, SCCP_MAX_PACKET);

	if (packSize > SCCP_MAX_PACKET)
		ast_log(LOG_WARNING, "SCCP: Oversize packet mid: %d, our packet size: %zd, phone packet size: %d\n", letohl(m->lel_messageId), SCCP_MAX_PACKET, packSize);

	memcpy(m, s->buffer, (packSize < SCCP_MAX_PACKET ? packSize : SCCP_MAX_PACKET) );

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
 */
void * sccp_socket_thread(void * ignore)
{
	fd_set fset;
	int res, maxfd = FD_SETSIZE;
	time_t now;
	sccp_session_t * s = NULL;
	sccp_moo_t * m;
	struct timeval tv;
	sigset_t sigs;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	sigemptyset(&sigs);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGPIPE);
	sigaddset(&sigs, SIGWINCH);
	sigaddset(&sigs, SIGURG);
//	pthread_sigmask(SIG_BLOCK, &sigs, NULL);

	FD_ZERO(&active_fd_set);
	FD_SET(GLOB(descriptor), &active_fd_set);
	maxfd = GLOB(descriptor);
	uint8_t keepaliveAdditionalTime = 0;

	while (GLOB(descriptor) > -1)
	{
		fset = active_fd_set;
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		res = select(maxfd + 1, &fset, 0, 0, &tv);
		maxfd = GLOB(descriptor);

		if (res == -1) {
			ast_log(LOG_ERROR, "SCCP select() returned -1. errno: %s\n", strerror(errno));
			continue;
		}

		now = time(0);
		SCCP_LIST_LOCK(&GLOB(sessions));
		SCCP_LIST_TRAVERSE(&GLOB(sessions), s, list) {
			keepaliveAdditionalTime = 10;
			if (s->fd > 0) {
				if (s->fd > maxfd)
					maxfd = s->fd;
				/* we give the device a little delay after the TCP connection to start sending registration packets */
				if (!FD_ISSET(s->fd, &active_fd_set) && now > s->lastKeepAlive + 1)
					FD_SET(s->fd, &active_fd_set);
				if (FD_ISSET(s->fd, &fset)) {
					sccp_read_data(s);
					while ((m = sccp_process_data(s))) {
						if (!sccp_handle_message(m, s)) {
							sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
							sccp_session_close(s);
						}
					}
				}
			}
			if (s->fd > 0) {
				/* we increase additionalTime for wireless devices */
				if(s->device && (s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7920
									|| s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7921
									|| s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7925)){
					keepaliveAdditionalTime = 30;

				}

				if (s->device && s->device->keepalive && (now > ((s->lastKeepAlive + s->device->keepalive) + keepaliveAdditionalTime) ) ) {
					//ast_log(LOG_WARNING, "%s: Device lastKeepAlive %s, now %s\n", (s->device) ? s->device->id : "SCCP", ctime(&s->lastKeepAlive), ctime(&now));
					ast_log(LOG_WARNING, "%s: Dead device does not send a keepalive message in %d+%d seconds. Will be removed\n", (s->device) ? s->device->id : "SCCP", GLOB(keepalive), keepaliveAdditionalTime);
					sccp_session_close(s);
					destroy_session(s);
				}
			} else {
				/* session is gone */
				sccp_session_close(s);
				destroy_session(s);
			}
		}
		SCCP_LIST_UNLOCK(&GLOB(sessions));

		if ( (GLOB(descriptor)> -1) && FD_ISSET(GLOB(descriptor), &fset)) {
			sccp_accept_connection();
		}
	}

	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: Exit from the socket thread\n");

	return NULL;
}


/*!
 * \brief Socket Send Message
 * \param device SCCP Device
 * \param t SCCP Message
 */
void sccp_session_sendmsg(sccp_device_t *device, sccp_message_t t)
{
	if(!device || !device->session)
		return;

	sccp_moo_t * r = sccp_build_packet(t, 0);
	if (r)
		sccp_session_send(device, r);
}

/*!
 * \brief Socket Send
 * \param device SCCP Device
 * \param r Message Data Structure (sccp_moo_t)
 * \return SCCP Session Send
 */
int sccp_session_send(const sccp_device_t *device, sccp_moo_t * r)
{
	sccp_session_t *s =sccp_session_find(device);
	return sccp_session_send2(s, r);
}

/*!
 * \brief Socket Send Message
 * \param s Session SCCP Session (can't be null)
 * \param r Message SCCP Moo Message (will be freed)
 * \return Result as Int
 */
int sccp_session_send2(sccp_session_t *s, sccp_moo_t * r){
	ssize_t res;
	uint32_t msgid = letohl(r->lel_messageId);
	
	ssize_t bytesSent;
	ssize_t bufLen;
	uint8_t *bufAddr;
	boolean_t finishSending;
	unsigned int try, maxTries;;

	if (!s || s->fd <= 0) {
		sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "SCCP: Tried to send packet over DOWN device.\n");
		ast_free(r);
		r = NULL;
		return -1;
	}
//	if(!s->device){
//		ast_free(r);
//		return -1;
//	}
	sccp_session_lock(s);

	//sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET)?r->length:SCCP_MAX_PACKET);
	
	/* This is a just a test */
	if(msgid == KeepAliveAckMessage || msgid == RegisterAckMessage) {
		r->lel_reserved = 0;
	} else if(s->device && s->device->inuseprotocolversion >= 17) {
//		r->lel_reserved = htolel(s->device->inuseprotocolversion);
		r->lel_reserved = htolel(0x11); // we should always send 0x11
	} else {
		r->lel_reserved = 0;
	}

	res = 0;
	finishSending = 0;
	try = 1;
	maxTries = 500;
	bytesSent = 0;
	bufAddr = ((uint8_t *) r);
	bufLen = (ssize_t)(letohl(r->length) + 8);
	/* sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Sending Packet Type %s (%d bytes)\n", s->device->id, message2str(letohl(r->lel_messageId)), letohl(r->length)); */
	do {
		res = write(s->fd, bufAddr+bytesSent, bufLen-bytesSent);
		if(res >= 0) {
			bytesSent += res;
		}
		if((bytesSent == bufLen) || (try >= maxTries)) {
			finishSending = 1;
		} else {
			usleep(10);
		}
		try++;
	} while(!finishSending);

	sccp_session_unlock(s);
	ast_free(r);

	if(bytesSent < bufLen) {
        	sccp_log((DEBUGCAT_SOCKET))(VERBOSE_PREFIX_3 "%s: Could only send %d of %d bytes!\n", s->device->id, (int)bytesSent, (int)bufLen);
		sccp_session_close(s);
        	return 0;
        }

	return res;
}

/*!
 * \brief Find session for device
 * \param device SCCP Device
 * \return SCCP Session
 */
sccp_session_t * sccp_session_find(const sccp_device_t *device)
{
	sccp_session_t *session = NULL;
	if(!device)
		return NULL;

	//SCCP_LIST_LOCK(&GLOB(sessions));
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), session, list) {
		if(session->device && session->device == device)
			break;
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	//SCCP_LIST_UNLOCK(&GLOB(sessions));
	return session;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
 * \param message Message as char (reason of rejection)
 */
void sccp_session_reject(sccp_session_t *session, char *message){
	sccp_moo_t 		* r;
	REQ(r, RegisterRejectMessage);
	sccp_copy_string(r->msg.RegisterRejectMessage.text, message, sizeof(r->msg.RegisterRejectMessage.text));
	sccp_session_send2(session, r);
}

/*!
 * \brief Get the in_addr for Specific Device.
 * \param device SCCP Device Pointer (can be null)
 * \param type Type in {AF_INET, AF_INET6}
 * \return In Address as Structure
 */
struct in_addr * sccp_session_getINaddr(sccp_device_t *device, int type){
	sccp_session_t *s = sccp_session_find(device);
	if(!s)
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

