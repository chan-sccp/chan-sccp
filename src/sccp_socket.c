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

#include <netinet/in.h>
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

boolean_t sccp_socket_is_IPv4(const struct sockaddr_storage *sockAddrStorage)
{
	return (sockAddrStorage->ss_family == AF_INET) ? TRUE : FALSE;
}

boolean_t sccp_socket_is_IPv6(const struct sockaddr_storage *sockAddrStorage)
{
	return (sockAddrStorage->ss_family == AF_INET6) ? TRUE : FALSE;
}

uint16_t sccp_socket_getPort(const struct sockaddr_storage *sockAddrStorage)
{
	if (sccp_socket_is_IPv4(sockAddrStorage)) {
		return ntohs(((struct sockaddr_in*)sockAddrStorage)->sin_port);
	} else if (sccp_socket_is_IPv6(sockAddrStorage)) {
		return ntohs(((struct sockaddr_in6*)sockAddrStorage)->sin6_port);
	}
	return 0;
}

void sccp_socket_setPort(const struct sockaddr_storage *sockAddrStorage, uint16_t port)
{
	if (sccp_socket_is_IPv4(sockAddrStorage)) {
		((struct sockaddr_in*)sockAddrStorage)->sin_port = htons(port);
	} else if (sccp_socket_is_IPv6(sockAddrStorage)) {
		((struct sockaddr_in6*)sockAddrStorage)->sin6_port =htons(port);
	}
}

int sccp_socket_is_any_addr(const struct sockaddr_storage *sockAddrStorage)
{
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} tmp_addr = {
		.ss = *sockAddrStorage,
	};

	return (sccp_socket_is_IPv4(sockAddrStorage) && (tmp_addr.sin.sin_addr.s_addr == INADDR_ANY)) ||
	    (sccp_socket_is_IPv6(sockAddrStorage) && IN6_IS_ADDR_UNSPECIFIED(&tmp_addr.sin6.sin6_addr));
}

boolean_t sccp_socket_getExternalAddr(struct sockaddr_storage *sockAddrStorage, sa_family_t type)
{    
	//! \todo handle type parameter
	memcpy(sockAddrStorage, &GLOB(externip), sizeof(struct sockaddr_storage));
	return TRUE;
}

size_t sccp_socket_sizeof(const struct sockaddr_storage *sockAddrStorage) 
{
	if (sccp_socket_is_IPv4(sockAddrStorage)) {
		return sizeof(struct sockaddr_in);
	} else if (sccp_socket_is_IPv6(sockAddrStorage)) {
		return sizeof(struct sockaddr_in6);
	}
	return 0;
}

static int sccp_socket_is_ipv6_link_local(const struct sockaddr_storage *sockAddrStorage)
{
	const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sockAddrStorage;
	return sccp_socket_is_IPv6(sockAddrStorage) && IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr);
}

boolean_t sccp_socket_is_mapped_IPv4(const struct sockaddr_storage *sockAddrStorage)
{
	if (sccp_socket_is_IPv6(sockAddrStorage)) {
		const struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sockAddrStorage;
		return IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr);
	} else {
		return FALSE;
	}
}

boolean_t sccp_socket_ipv4_mapped(const struct sockaddr_storage *sockAddrStorage, struct sockaddr_storage *sockAddrStorage_mapped)
{
	const struct sockaddr_in6 *sin6;
	struct sockaddr_in sin4;

	if (!sccp_socket_is_IPv6(sockAddrStorage)) {
		return FALSE;
	}
  
	if (!sccp_socket_is_mapped_IPv4(sockAddrStorage)) {
		return FALSE;
	}

	sin6 = (const struct sockaddr_in6*)sockAddrStorage;

	memset(&sin4, 0, sizeof(sin4));
	sin4.sin_family = AF_INET;
	sin4.sin_port = sin6->sin6_port;
	sin4.sin_addr.s_addr = ((uint32_t *)&sin6->sin6_addr)[3];

	memcpy(sockAddrStorage_mapped, &sin4, sizeof(sin4));
	return TRUE;
}

/*!
 * \brief
 * Compares the addresses of two ast_sockaddr structures.
 *
 * \retval -1 \a a is lexicographically smaller than \a b
 * \retval 0 \a a is equal to \a b
 * \retval 1 \a b is lexicographically smaller than \a a
 */
int sccp_socket_cmp_addr(const struct sockaddr_storage *a, const struct sockaddr_storage *b)
{
//	char *stra = ast_strdupa(sccp_socket_stringify_addr(a));
//	char *strb = ast_strdupa(sccp_socket_stringify_addr(b));
	
	const struct sockaddr_storage *a_tmp, *b_tmp;
	struct sockaddr_storage ipv4_mapped;
	size_t len_a = sccp_socket_sizeof(a);
	size_t len_b = sccp_socket_sizeof(b);
	int ret = -1;

	a_tmp = a;
	b_tmp = b;

	if (len_a != len_b) {
		if (sccp_socket_ipv4_mapped(a, &ipv4_mapped)) {
			a_tmp = &ipv4_mapped;
		} else if (sccp_socket_ipv4_mapped(b, &ipv4_mapped)) {
			b_tmp = &ipv4_mapped;
		}
	}
	if (len_a < len_b) {
		ret = -1;
		goto EXIT;
	} else if (len_a > len_b) {
		ret = 1;
		goto EXIT;
	}
	
	if (a_tmp->ss_family == b_tmp->ss_family) {
		if (a_tmp->ss_family == AF_INET) {
			ret = memcmp(     &(((struct sockaddr_in *) a_tmp)->sin_addr), 
					&(((struct sockaddr_in *) b_tmp)->sin_addr),
					sizeof(struct in_addr) );
		} else { // AF_INET6
			ret = memcmp(     &(((struct sockaddr_in6 *) a_tmp)->sin6_addr), 
					&(((struct sockaddr_in6 *) b_tmp)->sin6_addr),
					sizeof(struct in6_addr) );
		}
	}
EXIT:
//	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_2 "SCCP: sccp_socket_cmp_addr(%s, %s) returning %d\n", stra, strb, ret);
	return ret;  
}

/*!
 * \brief
 * Splits a string into its host and port components
 *
 * \param str[in]   The string to parse. May be modified by writing a NUL at the end of
 *                  the host part.
 * \param host[out] Pointer to the host component within \a str.
 * \param port[out] Pointer to the port component within \a str.
 * \param flags     If set to zero, a port MAY be present. If set to PARSE_PORT_IGNORE, a
 *                  port MAY be present but will be ignored. If set to PARSE_PORT_REQUIRE,
 *                  a port MUST be present. If set to PARSE_PORT_FORBID, a port MUST NOT  
 *                  be present.
 *
 * \retval 1 Success
 * \retval 0 Failure
 */
int sccp_socket_split_hostport(char *str, char **host, char **port, int flags)
{
	char *s = str;
	char *orig_str = str;/* Original string in case the port presence is incorrect. */
	char *host_end = NULL;/* Delay terminating the host in case the port presence is incorrect. */

	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_4 "Splitting '%s' into...\n", str);
	*host = NULL;
	*port = NULL;
	if (*s == '[') {
		*host = ++s;
		for (; *s && *s != ']'; ++s) {
		}
		if (*s == ']') {
			host_end = s;
			++s;
		}
		if (*s == ':') {
			*port = s + 1;
		}
	} else { 
		*host = s;
		for (; *s; ++s) {
			if (*s == ':') {
				if (*port) {
					*port = NULL;
					break;
				} else {
					*port = s;
				}
			}
		}
		if (*port) {
			host_end = *port;
			++*port;
		}
	}

	switch (flags & PARSE_PORT_MASK) {
	case PARSE_PORT_IGNORE:
		*port = NULL;  
		break;
	case PARSE_PORT_REQUIRE:
		if (*port == NULL) {
			pbx_log(LOG_WARNING, "Port missing in %s\n", orig_str);
			return 0;
		}
		break;
	case PARSE_PORT_FORBID:
		if (*port != NULL) {
			pbx_log(LOG_WARNING, "Port disallowed in %s\n", orig_str);
			return 0;
		}
		break;
	}
	/* Can terminate the host string now if needed. */
	if (host_end) {
		*host_end = '\0';
	}
	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_4 "...host '%s' and port '%s'.\n", *host, *port ? *port : "");
	return 1;
}


AST_THREADSTORAGE(sccp_socket_stringify_buf);
char *sccp_socket_stringify_fmt(const struct sockaddr_storage *sockAddrStorage, int format)
{
	const struct sockaddr_storage *sockAddrStorage_tmp;
	char host[NI_MAXHOST];
	char port[NI_MAXSERV];
	struct ast_str *str;
	int e;
	static const size_t size = sizeof(host) - 1 + sizeof(port) - 1 + 4;


	if (!sockAddrStorage) {
		return "(null)";
	}

	if (!(str = ast_str_thread_get(&sccp_socket_stringify_buf, size))) {
		return "";
	}

//	if (sccp_socket_ipv4_mapped(sockAddrStorage, &sockAddrStorage_tmp_ipv4)) {
//		struct sockaddr_storage sockAddrStorage_tmp_ipv4;
//		sockAddrStorage_tmp = &sockAddrStorage_tmp_ipv4;
//#if DEBUG
//		sccp_log(0)("SCCP: ipv4 mapped in ipv6 address\n");
//#endif
//	} else {
		sockAddrStorage_tmp = sockAddrStorage;
//	}

	if ((e = getnameinfo((struct sockaddr *)sockAddrStorage_tmp, sccp_socket_sizeof(sockAddrStorage_tmp),
			     format & SCCP_SOCKADDR_STR_ADDR ? host : NULL,
			     format & SCCP_SOCKADDR_STR_ADDR ? sizeof(host) : 0,
			     format & SCCP_SOCKADDR_STR_PORT ? port : 0,
			     format & SCCP_SOCKADDR_STR_PORT ? sizeof(port): 0,
			     NI_NUMERICHOST | NI_NUMERICSERV))) {
		pbx_log(LOG_ERROR, "getnameinfo(): %s \n", gai_strerror(e));
		return "";
	}

	if ((format & SCCP_SOCKADDR_STR_REMOTE) == SCCP_SOCKADDR_STR_REMOTE) {
		char *p;
		if (sccp_socket_is_ipv6_link_local(sockAddrStorage_tmp) && (p = strchr(host, '%'))) {
			*p = '\0';
		}
	}
	switch ((format & SCCP_SOCKADDR_STR_FORMAT_MASK))  {
	case SCCP_SOCKADDR_STR_DEFAULT:
		ast_str_set(&str, 0, sockAddrStorage_tmp->ss_family == AF_INET6 ?
				"[%s]:%s" : "%s:%s", host, port);
		break;
	case SCCP_SOCKADDR_STR_ADDR:
		ast_str_set(&str, 0, "%s", host);
		break;
	case SCCP_SOCKADDR_STR_HOST:
		ast_str_set(&str, 0,
			    sockAddrStorage_tmp->ss_family == AF_INET6 ? "[%s]" : "%s", host);
		break;
	case SCCP_SOCKADDR_STR_PORT:
		ast_str_set(&str, 0, "%s", port);
		break;
	default:
		ast_log(LOG_ERROR, "Invalid format\n");
		return "";
	}

	return ast_str_buffer(str);
}

/*!
 * \brief Exchange Socket Addres Information from them to us
 */
int sccp_socket_getOurAddressfor(const struct sockaddr_storage *them, struct sockaddr_storage *us)
{
	int sock;
	struct sockaddr_storage sin;
	socklen_t slen;

	sin.ss_family = them->ss_family;
	if (sccp_socket_is_IPv6(them)){
		((struct sockaddr_in6 *)&sin)->sin6_port = htons(sccp_socket_getPort( &GLOB(bindaddr)) );
		((struct sockaddr_in6 *)&sin)->sin6_addr = ((struct sockaddr_in6 *)them)->sin6_addr;
		slen = sizeof(struct sockaddr_in6);
	} else if (sccp_socket_is_IPv4(them)) {
		((struct sockaddr_in *)&sin)->sin_port = htons(sccp_socket_getPort( &GLOB(bindaddr)) );
		((struct sockaddr_in *)&sin)->sin_addr = ((struct sockaddr_in *)them)->sin_addr;
		slen = sizeof(struct sockaddr_in);
	} else {
		pbx_log(LOG_WARNING, "SCCP: getOurAddressfor Unspecified them format: %s\n", sccp_socket_stringify(them));
		return -1;
	}

	if ((sock = socket(sin.ss_family, SOCK_DGRAM, 0)) < 0) {
		return -1;
	}

	if (connect(sock, (const struct sockaddr *) &sin, sizeof(sin))) {
		pbx_log(LOG_WARNING, "SCCP: getOurAddressfor Failed to connect to %s\n", sccp_socket_stringify(&sin));
		return -1;
	}
	if (getsockname(sock, (struct sockaddr *) &sin, &slen)) {
		close(sock);
		return -1;
	}
	close(sock);
	memcpy(us, &sin, slen);
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
	unsigned int packetSize = header->length;
	unsigned int protocolVersion = letohl(header->lel_protocolVer);
	unsigned int messageId = letohl(header->lel_messageId);

	// dissecting header to see if we have a valid sccp message, that we can handle
	if (packetSize < 4 || packetSize > SCCP_MAX_PACKET - 8) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) Size of the data payload in the packet is out of bounds: %d < %u > %d, cancelling read.\n", 4, packetSize, (int)(SCCP_MAX_PACKET - 8));
		return -1;
	}
	if ( protocolVersion > 0 && !(sccp_protocol_isProtocolSupported(s->protocolType, protocolVersion)) ) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) protocolversion %u is unknown, cancelling read.\n", protocolVersion);
		return -1;
	}
	
	if (messageId < SCCP_MESSAGE_LOW_BOUNDARY || messageId > SCCP_MESSAGE_HIGH_BOUNDARY) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_read_data) messageId out of bounds: %d < %u > %d, cancelling read.\n", SCCP_MESSAGE_LOW_BOUNDARY, messageId, SCCP_MESSAGE_HIGH_BOUNDARY);
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
	unsigned char buffer[SCCP_MAX_PACKET] = {0};
	unsigned char *dataptr;
	errno = 0;
	int tries = 0;
	int max_retries = 30;											/* arbitrairy number of tries to read a message */

	// STAGE 1: read header
	memset(msg, 0, SCCP_MAX_PACKET);	
	readlen = read(s->fds[0].fd, (&msg->header), SCCP_PACKET_HEADER);
	if (readlen < 0 && (errno == EINTR || errno == EAGAIN)) { return TRUE;}					/* try again later, return TRUE with empty r */
	else if (readlen <= 0) {goto READ_ERROR;}								/* error || client closed socket */
	
	msg->header.length = letohl(msg->header.length);
	if ((msgDataSegmentSize = sccp_dissect_header(s, &msg->header)) < 0) {
		goto READ_ERROR;
	}
	
	// STAGE 2: read message data
	msgDataSegmentSize -= SCCP_PACKET_HEADER;
	UnreadBytesAccordingToPacket = msg->header.length - 4;							/* correction because we count messageId as part of the header */
	bytesToRead = (UnreadBytesAccordingToPacket > msgDataSegmentSize) ? msgDataSegmentSize : UnreadBytesAccordingToPacket;	/* take the smallest of the two */
	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s: Dissection %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, msg->header.length: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, msg->header.length, bytesToRead, bytesReadSoFar);
	dataptr = (unsigned char *) &msg->data;
	while (bytesToRead >0) {
		sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s: Reading %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, bytesToRead, bytesReadSoFar);
		readlen = read(s->fds[0].fd, buffer, bytesToRead);						// use bufferptr instead
		if ((readlen < 0) && (tries++ < max_retries) && (errno == EINTR || errno == EAGAIN)) {continue;}	/* try again, blocking */
		else if (readlen <= 0) {goto READ_ERROR;}							/* client closed socket, because read caused an error */
		bytesReadSoFar += readlen;
		bytesToRead -= readlen;										// if bytesToRead then more segments to read
		memcpy(dataptr, buffer, readlen);
		dataptr += readlen;
	};
	UnreadBytesAccordingToPacket -= bytesReadSoFar;
	tries = 0;
	
	sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_3 "%s: Finished Reading %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, bytesToRead, bytesReadSoFar);
	
	// STAGE 3: discard the rest of UnreadBytesAccordingToPacket if it was bigger then msgDataSegmentSize
	if (UnreadBytesAccordingToPacket > 0) { 								/* checking to prevent unneeded allocation of discardBuffer */
		pbx_log(LOG_NOTICE, "%s: Going to Discard %d bytes of data for '%s' (%d) (Needs developer attention)\n", DEV_ID_LOG(s->device), UnreadBytesAccordingToPacket, msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId);
		unsigned char discardBuffer[128];
		bytesToRead = UnreadBytesAccordingToPacket;
		while (bytesToRead > 0) {
			readlen= read(s->fds[0].fd, discardBuffer, (bytesToRead > sizeof(discardBuffer)) ? sizeof(discardBuffer) : bytesToRead);
			if ((readlen < 0) && (tries++ < max_retries) && (errno == EINTR || errno == EAGAIN)) {continue;}	/* try again, blocking */
			else if (readlen <= 0) {break;}								/* if we read EOF, just break reading (invalid packet information) */
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
		pbx_log(LOG_ERROR, "%s: stats: %s (%d), msgDataSegmentSize: %d, UnreadBytesAccordingToPacket: %d, bytesToRead: %d, bytesReadSoFar: %d\n", DEV_ID_LOG(s->device), msgtype2str(letohl(msg->header.lel_messageId)), msg->header.lel_messageId, msgDataSegmentSize, UnreadBytesAccordingToPacket, bytesToRead, bytesReadSoFar);
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
	char addrStr[INET6_ADDRSTRLEN];

	if (!s) {
		return;
	}

	sccp_copy_string(addrStr, sccp_socket_stringify_addr(&s->sin), sizeof(addrStr));
	
	found_in_list = sccp_session_removeFromGlobals(s);

	if (!found_in_list) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Session could not be found in GLOB(session) %s\n", DEV_ID_LOG(s->device), addrStr);
	}

	/* cleanup device if this session is not a crossover session */
	if (s->device && (d = sccp_device_retain(s->device))) {
		sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_3 "%s: Destroy Device Session %s\n", DEV_ID_LOG(s->device), addrStr);
		d->registrationState = SKINNY_DEVICE_RS_NONE;
		d->needcheckringback = 0;
		sccp_dev_clean(d, (d->realtime) ? TRUE : FALSE, cleanupTime);
		sccp_device_release(d);
	}

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
	char addrStr[INET6_ADDRSTRLEN];

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
				sccp_copy_string(addrStr, sccp_socket_stringify_addr(&s->sin), sizeof(addrStr));
				pbx_log(LOG_ERROR, "%s: poll() returned %d. errno: %s, (ip-address: %s)\n", DEV_ID_LOG(s->device), errno, strerror(errno), addrStr);
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
				break;
			}
		} else if (0 == res) {										/* poll timeout */
			sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Poll Timeout.\n", DEV_ID_LOG(s->device));
			if (((int) time(0) > ((int) s->lastKeepAlive + (int) maxWaitTime))) {
				sccp_copy_string(addrStr, sccp_socket_stringify_addr(&s->sin), sizeof(addrStr));
				ast_log(LOG_NOTICE, "%s: Closing session because connection timed out after %d seconds (timeout: %d) (ip-address: %s).\n", DEV_ID_LOG(s->device), (int) maxWaitTime, pollTimeout, addrStr);
				sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_TIMEOUT);
				break;
			}
		} else if (res > 0) {										/* poll data processing */
			if (s->fds[0].revents & POLLIN || s->fds[0].revents & POLLPRI) {			/* POLLIN | POLLPRI */
				/* we have new data -> continue */
				sccp_log((DEBUGCAT_HIGH)) (VERBOSE_PREFIX_2 "%s: Session New Data Arriving\n", DEV_ID_LOG(s->device));
				//while (sccp_read_data(s, &r)) {						/* according to poll specification we should empty out the read buffer completely.*/
														/* but that would give us trouble with timeout */
				if (sccp_read_data(s, &msg)) {
					if (sccp_handle_message(&msg, s) != 0) {
						if (s->device) {
							sccp_device_sendReset(s->device, SKINNY_DEVICE_RESTART);
						}
						sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_FAILED);
						break;
					}
					s->lastKeepAlive = time(0);
				} else {
					// pbx_log(LOG_NOTICE, "%s: Socket read failed\n", DEV_ID_LOG(s->device));	/* gives misleading information, could be just a close connection by the client side */
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
	struct sockaddr_storage incoming;
	sccp_session_t *s;
	int new_socket;
	char addrStr[INET6_ADDRSTRLEN];

	socklen_t length = (socklen_t) (sizeof(struct sockaddr_storage));

	int on = 1;

	if ((new_socket = accept(GLOB(descriptor), (struct sockaddr *) &incoming, &length)) < 0) {
		pbx_log(LOG_ERROR, "Error accepting new socket %s\n", strerror(errno));
		return;
	}
	//      if (setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	//	      pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
	//      }
	if (setsockopt(new_socket, IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
	}
	if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) {
		pbx_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
	}
#if defined(linux)
	struct timeval tv = {1,0};							/* timeout after one second when trying to read from a socket */
	if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {				
		pbx_log(LOG_WARNING, "Failed to set SCCP socket SO_RCVTIMEO: %s\n", strerror(errno));
	}
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
	
	sccp_copy_string(addrStr, sccp_socket_stringify(&s->sin), sizeof(addrStr));

	/* check ip address against global permit/deny ACL */
	if (GLOB(ha) && sccp_apply_ha(GLOB(ha), &s->sin) != AST_SENSE_ALLOW) {
		struct ast_str *buf = pbx_str_alloca(512);

		sccp_print_ha(buf, sizeof(buf), GLOB(ha));
		sccp_log(0) ("SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", addrStr, pbx_str_buffer(buf));
		pbx_log(LOG_WARNING, "SCCP: Rejecting Connection: Ip-address '%s' denied. Check general deny/permit settings (%s).\n", addrStr, pbx_str_buffer(buf));
		sccp_session_reject(s, "Device ip not authorized");
		sccp_session_unlock(s);
		destroy_session(s, 0);
		return;
	}
	sccp_session_addToGlobals(s);
	/** set default handler for registration to sccp */
	s->protocolType = SCCP_PROTOCOL;

	s->lastKeepAlive = time(0);
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Accepted Client Connection from %s\n", addrStr);

	if ( sccp_socket_is_any_addr( &GLOB(bindaddr)) ) {
		sccp_socket_getOurAddressfor(&incoming, &s->ourip);
	} else {
		memcpy(&s->ourip, &GLOB(bindaddr), sizeof(s->ourip));
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Connected on server via %s\n", sccp_socket_stringify(&s->ourip));

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
 *	- globals
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
		pbx_mutex_lock(&s->write_lock);									/* prevent two threads writing at the same time. That should happen in a synchronized way */
		res = write(s->fds[0].fd, bufAddr + bytesSent, bufLen - bytesSent);
		pbx_mutex_unlock(&s->write_lock);
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

/* defined but not used */
/*
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
*/

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




