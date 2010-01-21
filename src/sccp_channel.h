/*!
 * \file 	sccp_channel.h
 * \brief 	SCCP Channel Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *
 */

#ifndef __SCCP_CHANNEL_H
#define __SCCP_CHANNEL_H

#include "config.h"

sccp_channel_t * sccp_channel_allocate(sccp_line_t * l, sccp_device_t *device);
sccp_channel_t * sccp_channel_get_active(sccp_device_t * d);
void sccp_channel_updateChannelCapability(sccp_channel_t *channel);
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_send_callinfo(sccp_device_t *device, sccp_channel_t * c);
void sccp_channel_send_dialednumber(sccp_channel_t * c);
void sccp_channel_set_callstate(sccp_device_t * d, sccp_channel_t * c, uint8_t state);
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_connect(sccp_channel_t * c);
void sccp_channel_disconnect(sccp_channel_t * c);
#ifndef ASTERISK_CONF_1_2
enum ast_rtp_get_result sccp_channel_get_rtp_peer(struct ast_channel *ast, struct ast_rtp **rtp);
#ifdef ASTERISK_CONF_1_4
int sccp_channel_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, int codecs, int nat_active);
#else
int sccp_channel_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp, int codecs, int nat_active);
#endif
#endif
#ifndef CS_AST_HAS_RTP_ENGINE
enum ast_rtp_get_result sccp_channel_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp **rtp);
#else
enum ast_rtp_glue_result sccp_channel_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp_instance **rtp);
#endif
void sccp_channel_openreceivechannel(sccp_channel_t * c);
void sccp_channel_startmediatransmission(sccp_channel_t * c);
void sccp_channel_closereceivechannel(sccp_channel_t * c);
void sccp_channel_stopmediatransmission(sccp_channel_t * c);
void sccp_channel_openMultiMediaChannel(sccp_channel_t *channel);
void sccp_channel_startMultiMediaTransmission(sccp_channel_t *channel);
void sccp_channel_updatemediatype(sccp_channel_t * c);
void sccp_channel_endcall(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
sccp_channel_t * sccp_channel_newcall(sccp_line_t * l, sccp_device_t *device, char * dial, uint8_t calltype);
void sccp_channel_answer(sccp_device_t *d,sccp_channel_t * c);
void sccp_channel_delete_wo(sccp_channel_t * c, uint8_t list_lock, uint8_t channel_lock);
#define sccp_channel_delete(x) sccp_channel_delete_wo(x, 1, 1)
#define sccp_channel_delete_no_lock(x) sccp_channel_delete_wo(x, 0, 1)
void sccp_channel_cleanbeforedelete(sccp_channel_t *c);
int sccp_channel_hold(sccp_channel_t * c);
int sccp_channel_resume(sccp_device_t *device, sccp_channel_t * c);
void sccp_channel_start_rtp(sccp_channel_t * c);
void sccp_channel_stop_rtp(sccp_channel_t * c);
void sccp_channel_destroy_rtp(sccp_channel_t * c);
void sccp_channel_transfer(sccp_channel_t * c);
void sccp_channel_transfer_complete(sccp_channel_t * c);
void sccp_channel_forward(sccp_channel_t *parent, sccp_linedevices_t *lineDevice, char *fwdNumber);
#ifdef CS_SCCP_PARK
void sccp_channel_park(sccp_channel_t * c);
#endif

void sccp_channel_transfer2(sccp_channel_t * cSourceLocal, sccp_channel_t * cDestinationLocal);

#endif

