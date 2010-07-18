/*!
 * \file 	sccp_pbx.h
 * \brief 	SCCP PBX Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$  
 */
#ifndef __SCCP_PBX_H
#define __SCCP_PBX_H

#include <asterisk/pbx.h>

#ifndef CS_AST_HAS_RTP_ENGINE
#define sccp_rtp_read	ast_rtp_write
#else
#define sccp_rtp_read	ast_rtp_instance_write
#endif

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c);
int sccp_pbx_sched_dial(const void *data);
int sccp_pbx_helper(sccp_channel_t * c);
void * sccp_pbx_softswitch(sccp_channel_t * c);
void start_rtp(sccp_channel_t * sub);

#ifdef CS_AST_HAS_TECH_PVT
extern const struct ast_channel_tech sccp_tech;
#endif

void sccp_pbx_needcheckringback(sccp_device_t * d);

void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]);
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame * f);
#if ASTERISK_VERSION_NUM >= 10400
int sccp_ast_queue_control(sccp_channel_t * c, enum ast_control_frame_type control);
#else
int sccp_ast_queue_control(sccp_channel_t * c, uint8_t control);
#endif
#ifdef CS_AST_CONTROL_CONNECTED_LINE
static void sccp_pbx_update_connectedline(sccp_channel_t *channel, const void *data, size_t datalen);
#endif
#endif
