
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

#include <config.h>
#include "common.h"
#include <signal.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#include <sys/ioctl.h>
#ifdef SOLARIS
#    include <sys/filio.h>											// provides FIONREAD on SOLARIS
#endif
#ifndef CS_USE_POLL_COMPAT
#    include <poll.h>
#    include <sys/poll.h>
#else
#    define AST_POLL_COMPAT 1
#    include <asterisk/poll-compat.h>
#endif
#ifdef pbx_poll
#    define sccp_socket_poll pbx_poll
#else
#    define sccp_socket_poll poll
#endif
sccp_session_t *sccp_session_findByDevice(const sccp_device_t * device);

void destroy_session(sccp_session_t * s, uint8_t cleanupTime);

void sccp_session_close(sccp_session_t * s);

void sccp_socket_device_thread_exit(void *session);

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

void sccp_socket_stop_sessionthread(sccp_session_t * session, uint8_t newRegistrationState) 
{
        sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "%s: Stopping Session Thread\n", DEV_ID_LOG(session->device));
        if (!session) { 
                pbx_log(LOG_NOTICE, "SCCP: session already terminated\n");
                return;
        }

        if (!session->session_stop) {
                session->session_stop = 1;
		if (session->device)
			session->device->registrationState = newRegistrationState;
                if (AST_PTHREADT_NULL != session->session_thread) {
                        shutdown(session->fds[0].fd,SHUT_RD);          		// this will also wake up poll
										// which is waiting for a read event and close down the thread nicely
                } else {
                        sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: no thread -> just destroy session\n", DEV_ID_LOG(session->device));
                        sccp_session_close(session);
                        destroy_session(session, 0);
                }
        }
}

/*!
 * \brief Read Data From Socket
 * \param s SCCP Session
 *
 * \lock
 * 	- session
 */
static int sccp_read_data(sccp_session_t * s)
{
        if (!s || s->session_stop) {
                return 0;
        }

        int bytesAvailable = 0;
        int16_t readlen = 0;
        char input[SCCP_MAX_PACKET];
 
        /* implements a kind of non-blocking socket read on a blocking socket. Only reading as much as is available on the socket, without dissecting the packet. */
        if ((ioctl(s->fds[0].fd, FIONREAD, &bytesAvailable) == -1)) {
                if (errno == EAGAIN) {
                        pbx_log(LOG_WARNING, "SCCP: FIONREAD Come back later (EAGAIN): %s\n", strerror(errno));
                } else {
                        pbx_log(LOG_WARNING, "SCCP: FIONREAD ioctl failed: %s. closing connection\n", strerror(errno));
                        sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                }
                return 0;
        } else {
                readlen = read(s->fds[0].fd, input, (int16_t) bytesAvailable);
                if (readlen <= 0) {
                        if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) {
                                pbx_log(LOG_WARNING, "SCCP: Come back later (EAGAIN): %s\n", strerror(errno));
                        } else {        					/* (readlen==0 || errno == ECONNRESET || errno == ETIMEDOUT) */
                                sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "%s: device closed connection or network unreachable. closing connection.\n", DEV_ID_LOG(s->device));
                                sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                        }
                        return 0;
                } else {							/* move s->buffer content to the beginning */
			memcpy(s->buffer + s->buffer_size, input, readlen);
			s->buffer_size += readlen;
                }
        }
        return readlen;
}

/*!
 * \brief Find Session in Globals Lists
 * \param device SCCP Session
 * \return boolean
 * 
 * \lock
 * 	- session
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
 * \param device SCCP Session
 * \return boolean
 * 
 * \lock
 * 	- session
 */
static boolean_t sccp_session_addToGlobals(sccp_session_t * s)
{
	boolean_t res = FALSE;

	assert(s);

	if (!sccp_session_findBySession(s)) {;
		SCCP_RWLIST_WRLOCK(&GLOB(sessions));
		SCCP_LIST_INSERT_HEAD(&GLOB(sessions), s, list);
		res = TRUE;
		SCCP_RWLIST_UNLOCK(&GLOB(sessions));
	}
	return res;
}

/*!
 * \brief Removes a session from the global sccp_sessions list
 * \param device SCCP Session
 * \return boolean
 * 
 * \lock
 * 	- sessions
 */
static boolean_t sccp_session_removeFromGlobals(sccp_session_t * s)
{
	sccp_session_t *session;
	boolean_t res = FALSE;

	assert(s);

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
	return res;
}

/*!
 * \brief Retain device pointer in session. Replace existing pointer if necessary
 * \param session SCCP Session
 * \param device SCCP Device
 */
sccp_device_t *sccp_session_addDevice(sccp_session_t * session, sccp_device_t * device)
{
	assert(session);
	assert(device);
	if (session->device != device) {
		sccp_session_lock(session);
		if (session->device) {
			sccp_device_t *remdevice = sccp_device_retain(session->device);

			sccp_session_removeDevice(session);
			remdevice = sccp_device_release(remdevice);
		}
		if ((session->device = sccp_device_retain(device)))
			session->device->session = session;
		sccp_session_unlock(session);
	}
	return session->device;
}

/*!
 * \brief Release device pointer from session
 * \param session SCCP Session
 */
sccp_device_t *sccp_session_removeDevice(sccp_session_t * session)
{
	assert(session);
	assert(session->device);
	if (session->device) {
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
 * 	- see sccp_hint_eventListener() via sccp_event_fire()
 * 	- session
 */
void sccp_session_close(sccp_session_t * s)
{
	if (!s)
		return;

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
 * 	- sessions
 * 	- device
 */
void destroy_session(sccp_session_t * s, uint8_t cleanupTime)
{
	sccp_device_t *d = NULL;
	boolean_t found_in_list = FALSE;

	if (!s){
		return;
	}

	found_in_list = sccp_session_removeFromGlobals(s);

	if ( !found_in_list ) {
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
	/* freeing buffers */
	if (s->buffer) {
		sccp_free(s->buffer);
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
        sccp_moo_t *m;     

        pthread_cleanup_push(sccp_socket_device_thread_exit, session);

        /* we increase additionalTime for wireless/slower devices */
        if (s->device && (      
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7920 ||
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7921 ||
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7925 ||
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7975 ||
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO7970 ||
                                s->device->skinny_type == SKINNY_DEVICETYPE_CISCO6911)  
                        ) {
                keepaliveAdditionalTimePercent += 10;
        }

        while (s->fds[0].fd > 0 && !s->session_stop) {
                /* create cancellation point */
                pthread_testcancel();
#ifdef CS_DYNAMIC_CONFIG
                if (s->device) {
                        pbx_mutex_lock(&GLOB(lock));
                        if (GLOB(reload_in_progress) == FALSE && s && s->device && (!(s->device->pendingUpdate == FALSE && s->device->pendingDelete == FALSE))) {
                                sccp_device_check_update(s->device);
			}
                        pbx_mutex_unlock(&GLOB(lock));
                }
#endif
                /* calculate poll timout using keepalive interval */
                maxWaitTime = (s->device) ? s->device->keepalive : GLOB(keepalive);
                maxWaitTime += (maxWaitTime / 100) * keepaliveAdditionalTimePercent;
                pollTimeout = maxWaitTime * 1000;

                sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_4 "%s: set poll timeout %d/%d for session %d\n", DEV_ID_LOG(s->device), (int)maxWaitTime, pollTimeout / 1000, s->fds[0].fd);

                pthread_testcancel();									/* poll is also a cancellation point */
                res = sccp_socket_poll(s->fds, 1, pollTimeout);
                pthread_testcancel();
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                if (res > 0) {                                                                          /* poll data processing */
                        if (s->fds[0].revents & POLLIN || s->fds[0].revents & POLLPRI) {                /* POLLIN | POLLPRI*/
                                /* we have new data -> continue */
                                sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Session New Data Arriving\n", DEV_ID_LOG(s->device));
                                if (sccp_read_data(s)>8) {                                              /* size of header*/   
                                        while ((m = sccp_process_data(s))) {
                                                if (!sccp_handle_message(m, s)) {
                                                        if (s->device) {
                                                        	sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
                                                        }
                                                        sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                                                        sccp_free(m);
                                                        break;
                                                }
                                                sccp_free(m);
                                                s->lastKeepAlive = time(0);
                                        }
                                }
                        } else {                                                                        /* POLLHUP / POLLERR */
                                pbx_log(LOG_NOTICE,  "%s: Closing session because we received POLLPRI/POLLHUP/POLLERR\n", DEV_ID_LOG(s->device));
                                sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                                break;
                        }
                } else {                                                                                /* poll timeout */
			sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Poll Timeout.\n", DEV_ID_LOG(s->device));
			if (((int)time(0) > ((int)s->lastKeepAlive + (int)maxWaitTime))) {
				ast_log(LOG_NOTICE, "%s: Closing session because connection timed out after %d seconds (timeout: %d).\n", DEV_ID_LOG(s->device), (int)maxWaitTime, pollTimeout);
                                sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_TIMEOUT);
				break;
			}
                        if ((errno != EAGAIN) && (errno != EINTR)) {
                                pbx_log(LOG_ERROR, "SCCP poll() returned %d. errno: %s\n", errno, strerror(errno));
                                sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                                break;
                        }
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
 * 	- sessions
 */
static void sccp_accept_connection(void)
{
	/* called without GLOB(sessions_lock) */
	struct sockaddr_in incoming;

	sccp_session_t *s;

	int new_socket;

	socklen_t length = (socklen_t) (sizeof(struct sockaddr_in));

	int on = 1;

	if ((new_socket = accept(GLOB(descriptor), (struct sockaddr *)&incoming, &length)) < 0) {
		pbx_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		return;
	}
	if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	}
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

	sccp_session_addToGlobals(s);

	sccp_session_lock(s);
	s->fds[0].events = POLLIN | POLLPRI | POLLHUP | POLLERR;
	s->fds[0].revents = 0;
	s->fds[0].fd = new_socket;

	if (!GLOB(ha)) {
		pbx_log(LOG_NOTICE, "No global ha list\n");

	}

	/* check ip address against global permit/deny ACL */
	if (GLOB(ha) && sccp_apply_ha(GLOB(ha), &s->sin) != AST_SENSE_ALLOW) {
		pbx_log(LOG_NOTICE, "Reject Connection: Ip-address '%s' denied. Check general permit settings.\n", pbx_inet_ntoa(s->sin.sin_addr));
		s = sccp_session_reject(s, "Device ip not authorized");
		return;
	}

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));

	/** set default handler for registration to sccp */
	s->protocolType = SCCP_PROTOCOL;

	s->lastKeepAlive = time(0);
	s->buffer = calloc(1,SCCP_MAX_PACKET * 2);				// maximum buffer size has to be more than one maximum packet
	s->buffer_size = 0;
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted connection from %s\n", pbx_inet_ntoa(s->sin.sin_addr));

	if (GLOB(bindaddr.sin_addr.s_addr) == INADDR_ANY) {
		sccp_sockect_getOurAddressfor(&incoming.sin_addr, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr.sin_addr.s_addr), sizeof(s->ourip));
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Using ip %s\n", pbx_inet_ntoa(s->ourip));

	size_t stacksize = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	pbx_pthread_create(&s->session_thread, &attr, sccp_socket_device_thread, s);
	if (!pthread_attr_getstacksize(&attr, &stacksize)) {
		sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "SCCP: Using %d memory for this thread\n", (int)stacksize);
	}
	sccp_session_unlock(s);
}

/*!
 * \brief Socket Process Data
 * \param s SCCP Session
 * \note Called with mutex lock
 */
static sccp_moo_t *sccp_process_data(sccp_session_t * s)
{
	uint32_t packSize = 0;
	sccp_moo_t *msg = NULL;

	if (s->buffer_size <= 4) {
		return NULL;											/* Not enough data to even read the packet lenght */
	}

	memcpy(&packSize, s->buffer, 4);
	packSize = letohl(packSize);
	packSize += 8;

	if (s->buffer_size < 0 || (packSize) > (uint32_t) s->buffer_size) {
		return NULL;											/* Not enough data, yet. */
	} else {
		/* copy the first full message we can find out of s->buffer */
		msg = sccp_malloc((packSize < SCCP_MAX_PACKET ? packSize : SCCP_MAX_PACKET));			/* Only malloc what we need */
		if (!msg) {
			pbx_log(LOG_WARNING, "SCCP: unable to allocate %zd bytes for a new skinny packet (Expect Dissaster)\n", SCCP_MAX_PACKET);
			return NULL;
		}
		memset(msg, 0, (packSize < SCCP_MAX_PACKET ? packSize : SCCP_MAX_PACKET));	
		memcpy(msg, s->buffer, (packSize < SCCP_MAX_PACKET ? packSize : SCCP_MAX_PACKET));
		msg->length = letohl(msg->length);

		if (packSize > SCCP_MAX_PACKET) {
			pbx_log(LOG_WARNING, "SCCP: Oversize packet mid: %d, our packet size: %zd, phone packet size: %d\n", letohl(msg->lel_messageId), SCCP_MAX_PACKET, packSize);
		}	

		/* move s->buffer content to the beginning */
		s->buffer_size -= packSize;
		memmove(s->buffer, (s->buffer + packSize), s->buffer_size);

		/* return the message */
		return msg;
	}
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
void sccp_session_sendmsg(const sccp_device_t * device, sccp_message_t t)
{
	if (!device || !device->session) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "SCCP: (sccp_session_sendmsg) No device available to send message to\n");
		return;
	}

	sccp_moo_t *r = sccp_build_packet(t, 0);

	if (r) {
		sccp_session_send(device, r);
	}	
}

/*!
 * \brief Socket Send
 * \param device SCCP Device
 * \param r Message Data Structure (sccp_moo_t)
 * \return SCCP Session Send
 */
int sccp_session_send(const sccp_device_t * device, sccp_moo_t * r)
{
	sccp_session_t *s = sccp_session_findByDevice(device);
	if (s && !s->session_stop) {
		return sccp_session_send2(s, r);
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
 * 	- session
 */
int sccp_session_send2(sccp_session_t * s, sccp_moo_t * r)
{
        ssize_t res = 0;
        uint32_t msgid = letohl(r->lel_messageId);
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
                sccp_free(r);
                r = NULL; 
                return -1;
        }

        if (msgid == KeepAliveAckMessage || msgid == RegisterAckMessage || msgid == UnregisterAckMessage) {
                r->lel_reserved = 0;
        } else if (s->device && s->device->inuseprotocolversion >= 17) {
                r->lel_reserved = htolel(0x11);                                                                 // we should always send 0x11
        } else {
                r->lel_reserved = 0;
        }

        try = 0;
        maxTries = 500;                                                                                         // arbitrairy number of tries
        bytesSent = 0;
        bufAddr = ((uint8_t *) r);
        bufLen = (ssize_t) (letohl(r->length) + 8);
        do {
                try++;
                ast_mutex_lock(&s->write_lock);                                                                 // prevent two threads writing at the same time. That should happen in a synchronized way
                res = write(s->fds[0].fd, bufAddr + bytesSent, bufLen - bytesSent);
                ast_mutex_unlock(&s->write_lock);
                if (res < 0) {
                        if (errno == EINTR || errno == EAGAIN) {
/*                                sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_4 
                                                "%s: EAGAIN / EINTR: %d (error: '%s (%d)'). Sent %d of %d for Message: '%s' with total length %d. Try: %d/%d\n",
                                                DEV_ID_LOG(s->device), 
                                                (int)res, strerror(errno), errno,
                                                (int)bytesSent, (int)bufLen, 
                                                message2str(letohl(r->lel_messageId)), letohl(r->length),
                                                try, maxTries
                                        );*/
                                usleep(200);                                                                    // back off to give network/other threads some time
                                continue;
                        }
                        pbx_log(LOG_ERROR, 
                                        "%s: write returned %d (error: '%s (%d)'). Sent %d of %d for Message: '%s' with total length %d \n", 
                                        DEV_ID_LOG(s->device), 
                                        (int)res, strerror(errno), errno, 
                                        (int)bytesSent, (int)bufLen, 
                                        message2str(letohl(r->lel_messageId)), letohl(r->length)
                                );
                        if (s) {
                                sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
                        }
                        res = -1;
                        break;
                }
                bytesSent += res;
        } while (bytesSent < bufLen && try < maxTries && s && !s->session_stop && s->fds[0].fd > 0);

        sccp_free(r);
        r = NULL;

        if (bytesSent < bufLen) {
                ast_log(LOG_ERROR, "%s: Could only send %d of %d bytes!\n", DEV_ID_LOG(s->device), (int)bytesSent, (int)bufLen);
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
 * 	- sessions
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
	sccp_moo_t *r;

	REQ(r, RegisterRejectMessage);
	sccp_copy_string(r->msg.RegisterRejectMessage.text, message, sizeof(r->msg.RegisterRejectMessage.text));
	sccp_session_send2(session, r);

	/* if we reject the connection during accept connection, thread is not ready */
	sccp_socket_stop_sessionthread(session, SKINNY_DEVICE_RS_FAILED);
	return NULL;
}

/*!
 * \brief Send a Reject Message to Device.
 * \param session SCCP Session Pointer
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
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenRejectSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_moo_t *r;

	REQ(r, SPCPRegisterTokenReject);
	r->msg.SPCPRegisterTokenReject.lel_features = htolel(features);
	sccp_session_send2(session, r);
}

/*!
 * \brief Send a token acknowledgement to the SPCP Device.
 * \param session SCCP Session Pointer
 * \param features Phone Features as Uint32_t
 */
void sccp_session_tokenAckSPCP(sccp_session_t * session, uint32_t features)
{
	sccp_moo_t *r;

	REQ(r, SPCPRegisterTokenAck);
	r->msg.SPCPRegisterTokenAck.lel_features = htolel(features);
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
