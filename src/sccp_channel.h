/*!
 * \file        sccp_channel.h
 * \brief       SCCP Channel Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_CHANNEL_H
#define __SCCP_CHANNEL_H

#define sccp_channel_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_channel_retain(x) 		sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

/* live cycle */
sccp_channel_t *sccp_channel_allocate(sccp_line_t * l, sccp_device_t * device);					// device is optional
sccp_channel_t *sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, const char *dial, uint8_t calltype, const char *linkedid);

sccp_channel_t *sccp_channel_get_active(const sccp_device_t * d);
void sccp_channel_updateChannelDesignator(sccp_channel_t * c);
void sccp_channel_updateChannelCapability(sccp_channel_t * channel);
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_send_callinfo(const sccp_device_t * device, const sccp_channel_t * c);
void sccp_channel_send_callinfo2(sccp_channel_t * c);
void sccp_channel_setSkinnyCallstate(sccp_channel_t * c, skinny_callstate_t state);
void sccp_channel_display_callInfo(sccp_channel_t * channel);
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number);
boolean_t sccp_channel_set_originalCallingparty(sccp_channel_t * channel, char *name, char *number);
boolean_t sccp_channel_set_originalCalledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_reset_calleridPresenceParameter(sccp_channel_t * c);
void sccp_channel_set_calleridPresenceParameter(sccp_channel_t * c, sccp_calleridpresence_t presenceParameter);
void sccp_channel_connect(sccp_channel_t * c);
void sccp_channel_disconnect(sccp_channel_t * c);

void sccp_channel_openReceiveChannel(sccp_channel_t * c);
void sccp_channel_closeReceiveChannel(sccp_channel_t * c);
void sccp_channel_updateReceiveChannel(sccp_channel_t * c);
void sccp_channel_openMultiMediaReceiveChannel(sccp_channel_t * channel);
void sccp_channel_closeMultiMediaReceiveChannel(sccp_channel_t * channel);
void sccp_channel_updateMultiMediaReceiveChannel(sccp_channel_t * channel);

void sccp_channel_startMediaTransmission(sccp_channel_t * c);
void sccp_channel_stopMediaTransmission(sccp_channel_t * c);
void sccp_channel_updateMediaTransmission(sccp_channel_t * channel);
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel);
void sccp_channel_stopMultiMediaTransmission(sccp_channel_t * channel);
void sccp_channel_updateMultiMediaTransmission(sccp_channel_t * channel);

void sccp_channel_closeAllMediaTransmitAndReceive (sccp_device_t *d, sccp_channel_t *channel);

void sccp_channel_endcall(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
void sccp_channel_answer(const sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_clean(sccp_channel_t * c);
void sccp_channel_transfer(sccp_channel_t * c, sccp_device_t * device);
void sccp_channel_transfer_release(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_transfer_cancel(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_transfer_complete(sccp_channel_t * c);
int sccp_channel_hold(sccp_channel_t * c);
int sccp_channel_resume(sccp_device_t * device, sccp_channel_t * c, boolean_t swap_channels);
int sccp_channel_forward(sccp_channel_t * parent, sccp_linedevices_t * lineDevice, char *fwdNumber);

#if DEBUG
#define sccp_channel_getDevice_retained(_x) __sccp_channel_getDevice_retained(_x, __FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_device_t *__sccp_channel_getDevice_retained(const sccp_channel_t * channel, const char *filename, int lineno, const char *func);
#else
sccp_device_t *sccp_channel_getDevice_retained(const sccp_channel_t * channel);
#endif
void sccp_channel_setDevice(sccp_channel_t * channel, const sccp_device_t * device);
const char *sccp_channel_device_id(const sccp_channel_t * channel);

#ifdef CS_SCCP_PARK
void sccp_channel_park(sccp_channel_t * c);
#endif

boolean_t sccp_channel_setPreferredCodec(sccp_channel_t * c, const void *data);
int sccp_channel_callwaiting_tone_interval(sccp_device_t * d, sccp_channel_t * c);
const char *sccp_channel_getLinkedId(const sccp_channel_t * channel);

// find channel
#if DEBUG
#define sccp_find_channel_on_line_byid(_x,_y) __sccp_find_channel_on_line_byid(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_channel_t *__sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id, const char *filename, int lineno, const char *func);

#define sccp_channel_find_byid(_x) __sccp_channel_find_byid(_x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_channel_t *__sccp_channel_find_byid(uint32_t id, const char *filename, int lineno, const char *func);

#define sccp_channel_find_bypassthrupartyid(_x) __sccp_channel_find_bypassthrupartyid(_x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_channel_t *__sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid, const char *filename, int lineno, const char *func);

#define sccp_channel_find_bystate_on_line(_x,_y) __sccp_channel_find_bystate_on_line(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_channel_t *__sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state, const char *filename, int lineno, const char *func);

#define sccp_channel_find_bystate_on_device(_x,_y) __sccp_channel_find_bystate_on_device(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_channel_t *__sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state, const char *filename, int lineno, const char *func);
#else
sccp_channel_t *sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id);
sccp_channel_t *sccp_channel_find_byid(uint32_t id);
sccp_channel_t *sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state);
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, uint8_t state);
sccp_channel_t *sccp_channel_find_bystate_on_device(sccp_device_t * d, uint8_t state);
#endif
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid);
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c);
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d);

#endif
