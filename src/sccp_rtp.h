/*!
 * \file 	sccp_rtp.h
 * \brief 	SCCP RTP Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */
#ifndef __SCCP_RTP_H
#define __SCCP_RTP_H

int sccp_rtp_createAudioServer(const sccp_channel_t *c);
int sccp_rtp_createVideoServer(const sccp_channel_t *c);
void sccp_rtp_stop(sccp_channel_t * c);
void sccp_rtp_destroy(sccp_channel_t * c);
void sccp_rtp_set_peer(sccp_channel_t *c, struct sockaddr_in *new_peer);


#endif// __SCCP_RTP_H
