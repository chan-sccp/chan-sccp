/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */


#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_line.h"
#include "sccp_socket.h"
#include "sccp_device.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <asterisk/utils.h>

/* file descriptors of active sockets */
static fd_set	active_fd_set;

static void sccp_read_data(sccp_session_t * s) {
	ssize_t length, readlen;
	char * input, * newptr;

  /* called while we have GLOB(sessions_lock) */
	ast_mutex_lock(&s->lock);

	if (ioctl(s->fd, FIONREAD, &length) == -1) {
		ast_mutex_unlock(&s->lock);
		ast_log(LOG_WARNING, "SCCP: FIONREAD ioctl failed: %s\n", strerror(errno));
		sccp_session_close(s);
		return;
	}

	if (!length) {
		/* probably a CLOSE_WAIT */
		ast_mutex_unlock(&s->lock);
		sccp_session_close(s);
		return;
	}

	input = malloc(length + 1);
/*	memset(input, 0, length+1); */

	if ((readlen = read(s->fd, input, length)) < 0) {
		ast_log(LOG_WARNING, "SCCP: read() returned %s\n", strerror(errno));
		free(input);
		ast_mutex_unlock(&s->lock);
		sccp_session_close(s);
		return;
	}

	if (readlen != length) {
		ast_log(LOG_WARNING, "SCCP: read() returned %zd, wanted %zd: %s\n", readlen, length, strerror(errno));
		free(input);
		ast_mutex_unlock(&s->lock);
		sccp_session_close(s);
		return;
	}

	
	newptr = realloc(s->buffer, (size_t)(s->buffer_size + length));
	if (newptr) {
			s->buffer = newptr;
			memcpy(s->buffer + s->buffer_size, input, length);
			s->buffer_size += length;
	} else {
		ast_log(LOG_WARNING, "SCCP: unable to reallocate %zd bytes for skinny a packet\n", s->buffer_size + length);
		free(s->buffer);
		s->buffer_size = 0;
	}

	free(input);

	ast_mutex_unlock(&s->lock);
}

void sccp_session_close(sccp_session_t * s) {
	if (!s)
		return;
	ast_mutex_lock(&s->lock);
	if (s->fd > 0) {
		FD_CLR(s->fd, &active_fd_set);
		close(s->fd);
		s->fd = -1;
	}
	ast_mutex_unlock(&s->lock);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Old session marked down\n", (s->device) ? s->device->id : "SCCP");
}

static void destroy_session(sccp_session_t * s) {
	sccp_line_t * l, * l1 = NULL;
	sccp_device_t * d;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	if (!s)
		return;
	d = s->device;
	ast_mutex_lock(&GLOB(devices_lock));
#ifdef ASTERISK_CONF_1_2
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Killing Session %s\n", DEV_ID_LOG(d), ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Killing Session %s\n", DEV_ID_LOG(d), ast_inet_ntoa(s->sin.sin_addr));
#endif

	if (d) {
		ast_mutex_lock(&d->lock);
		l = d->lines;
		while (l) {
			sccp_hint_notify_linestate(l, SCCP_DEVICESTATE_UNAVAILABLE, NULL);
			sccp_line_kill(l);
			l->device = NULL;
			l1 = l->next_on_device;
			l->next_on_device = NULL;
			l = l1;
		}
		d->lines = NULL;
		d->session = NULL;
		d->registrationState = SKINNY_DEVICE_RS_NONE;
		ast_mutex_unlock(&d->lock);
    
#ifdef CS_SCCP_REALTIME
    sccp_device_t 	*devices, *dev, *prevDev, *delDev;

    if(d->realtime){
      sccp_log(10)(VERBOSE_PREFIX_3 "%s: Realtime device will be removed from the configuration.\n", d->id);
      devices = GLOB(devices);
      dev = devices;
      prevDev = NULL;
      delDev = NULL;
      
      while(NULL != dev) {
        if(d->id == dev->id) {
          delDev = dev;
          if(NULL == prevDev) {
            devices = dev->next;
            dev = devices;
          } else {
            prevDev->next = dev->next;
            dev = prevDev->next;
          }
          sccp_log(10)(VERBOSE_PREFIX_3 "%s: Removing realtime Device.\n", delDev->id);
          free(delDev);
        } else {
          prevDev = dev;
          dev = dev->next;
        }
      }
      GLOB(devices) = devices;
    }		
#endif
	}
	ast_mutex_unlock(&GLOB(devices_lock));

  /* remove the session */

	if (s->next) { /* not the last one */
		s->next->prev = s->prev;
	}
	if (s->prev) { /* not the first one */
		s->prev->next = s->next;
	}
	else { /* the first one */
		GLOB(sessions) = s->next;
	}

	if (s->fd > 0) {
		FD_CLR(s->fd, &active_fd_set);
		close(s->fd);
	}
	if (s->buffer)
		free(s->buffer);

	ast_mutex_destroy(&s->lock);
	free(s);
}

static void sccp_accept_connection(void) {
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
	if (setsockopt(new_socket, IPPROTO_IP, IP_TOS, &GLOB(tos), sizeof(GLOB(tos))) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket TOS to IPTOS_LOWDELAY: %s\n", strerror(errno));
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
/*
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_CORK, &on, sizeof(on)) < 0)
		ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_CORK: %s\n", strerror(errno));
*/
	s = malloc(sizeof(struct sccp_session));
	memset(s, 0, sizeof(sccp_session_t));
	memcpy(&s->sin, &incoming, sizeof(s->sin));
	ast_mutex_init(&s->lock);

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

	ast_mutex_lock(&GLOB(sessions_lock));
	if (GLOB(sessions)) {
		GLOB(sessions)->prev = s;
		s->next   = GLOB(sessions);
	}
	GLOB(sessions)  = s;
	ast_mutex_unlock(&GLOB(sessions_lock));
}

/* Called with mutex lock */
static sccp_moo_t * sccp_process_data(sccp_session_t * s) {
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

	m = malloc(SCCP_MAX_PACKET);
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
		newptr = malloc(s->buffer_size);
		if (newptr)
			memcpy(newptr, (s->buffer + packSize), s->buffer_size);
		else
			ast_log(LOG_WARNING, "SCCP: unable to allocate %zd bytes for packets buffer\n", s->buffer_size);
	}

	if (s->buffer)
		free(s->buffer);

	s->buffer = newptr;

	return m;
}

void * sccp_socket_thread(void * ignore) {
	fd_set fset;
	int res, maxfd = FD_SETSIZE;
	time_t now;
	sccp_session_t * s = NULL, * s1 = NULL;
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

	while (GLOB(descriptor) > -1) {

		fset = active_fd_set;
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		res = select(maxfd + 1, &fset, 0, 0, &tv);
		maxfd = GLOB(descriptor);
		if (res == -1) {
			ast_log(LOG_ERROR, "SCCP select() returned -1. errno: %s\n", strerror(errno));
			continue;
		}

		ast_mutex_lock(&GLOB(sessions_lock));
		s = GLOB(sessions);
		now = time(0);
	    while (s) {
			if (s->fd > 0) {
				if (s->fd > maxfd)
					maxfd = s->fd;
				/* we give the device a little delay after the TCP connection to start sending registration packets */
				if (!FD_ISSET(s->fd, &active_fd_set) && now > s->lastKeepAlive + 1)
					FD_SET(s->fd, &active_fd_set);
				if (FD_ISSET(s->fd, &fset)) {
					sccp_read_data(s);
					while ( (m = sccp_process_data(s)) ) {
	            		if (!sccp_handle_message(m, s))
							sccp_session_close(s);
					}
				}
			}
			if (s->fd > 0) {
				if (s->device && s->device->keepalive && (now > (s->lastKeepAlive + s->device->keepalive) + 10)) {
					ast_log(LOG_WARNING, "%s: Dead device does not send a keepalive message in %d seconds. Will be removed\n", (s->device) ? s->device->id : "SCCP", GLOB(keepalive));
					sccp_session_close(s);
				} else if (s->device) {
					if (s->needcheckringback) {
						s->needcheckringback = 0;
						if (!sccp_device_check_ringback(s->device)) {
							sccp_dev_check_mwi(s->device);
							sccp_dev_check_displayprompt(s->device);
						}
					}
				}
			} else {
				s1 = s;
			}
			s = s->next;
			if (s1) {
				destroy_session(s1);
				s1 = NULL;
			}
		}
		ast_mutex_unlock(&GLOB(sessions_lock));
		
		if ( (GLOB(descriptor)> -1) && FD_ISSET(GLOB(descriptor), &fset)) {
			sccp_accept_connection();
		}
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Exit from the socket thread\n");

	return NULL;
}
