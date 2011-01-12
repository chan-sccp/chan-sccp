/*!
 * \file 	sccp_pbx_wrapper.h
 * \brief 	SCCP PBX Wrapper Header
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#ifndef __SCCP_PBX_WRAPPER_H
#    define __SCCP_PBX_WRAPPER_H

char *get_pbx_callerid_name(struct ast_channel *ast_chan);
char *get_pbx_callerid_number(struct ast_channel *ast_chan);
sccp_callinfo_t *get_pbx_callerid(struct ast_channel * ast_chan);
int set_pbx_callerid(struct ast_channel *ast_chan, sccp_callinfo_t * callInfo);
struct ast_channel *pbx_channel_walk_locked(struct ast_channel *target);
const char *pbx_inet_ntoa(struct in_addr ia);
int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *them);
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *them);
#endif
