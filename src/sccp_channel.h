/*!
 * \file 	sccp_channel.h
 * \brief 	SCCP Channel Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_CHANNEL_H
#    define __SCCP_CHANNEL_H

#    include "config.h"

sccp_channel_t *sccp_channel_allocate_locked(sccp_line_t * l, sccp_device_t * device);
sccp_channel_t *sccp_channel_get_active_locked(sccp_device_t * d);
sccp_channel_t *sccp_channel_get_active_nolock(sccp_device_t * d);
void sccp_channel_updateChannelCapability_locked(sccp_channel_t * channel);
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_send_callinfo(sccp_device_t * device, sccp_channel_t * c);
void sccp_channel_send_dialednumber(sccp_channel_t * c);
void sccp_channel_setSkinnyCallstate(sccp_channel_t * c, skinny_callstate_t state);
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_originalCalledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_connect(sccp_channel_t * c);
void sccp_channel_disconnect(sccp_channel_t * c);
void sccp_channel_openreceivechannel_locked(sccp_channel_t * c);
void sccp_channel_startmediatransmission(sccp_channel_t * c);
void sccp_channel_closereceivechannel_locked(sccp_channel_t * c);
void sccp_channel_stopmediatransmission_locked(sccp_channel_t * c);
void sccp_channel_openMultiMediaChannel(sccp_channel_t * channel);
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel);
void sccp_channel_updatemediatype_locked(sccp_channel_t * c);
void sccp_channel_endcall_locked(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
sccp_channel_t *sccp_channel_newcall_locked(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype);
boolean_t sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype);
void sccp_channel_answer_locked(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_destroy_locked(sccp_channel_t * c);
int sccp_channel_destroy_callback(const void* data);
void sccp_channel_clean_locked(sccp_channel_t * c);
int sccp_channel_hold_locked(sccp_channel_t * c);
int sccp_channel_resume_locked(sccp_device_t * device, sccp_channel_t * c);

void sccp_channel_transfer_locked(sccp_channel_t * c);
void sccp_channel_transfer_complete(sccp_channel_t * c);
void sccp_channel_forward(sccp_channel_t * parent, sccp_linedevices_t * lineDevice, char *fwdNumber);
#    ifdef CS_SCCP_PARK
void sccp_channel_park(sccp_channel_t * c);
#    endif


#endif
