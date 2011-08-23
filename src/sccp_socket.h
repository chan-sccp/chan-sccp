
/*!
 * \file 	sccp_socket.h
 * \brief 	SCCP Socket Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2011-01-06 18:22:54 +0100 (Do, 06 Jan 2011) $
 * $Revision: 2219 $  
 */

#ifndef __SCCP_SOCKET_H
#    define __SCCP_SOCKET_H

void *sccp_socket_thread(void *ignore);
void sccp_session_sendmsg(const sccp_device_t * device, sccp_message_t t);
int sccp_session_send(const sccp_device_t * device, sccp_moo_t * r);
void sccp_session_reject(sccp_session_t * session, char *message);
void sccp_session_tokenReject(sccp_session_t * session, uint32_t backoff_time);
void sccp_session_tokenAck(sccp_session_t * session);
struct in_addr *sccp_session_getINaddr(sccp_device_t * device, int type);

#endif
