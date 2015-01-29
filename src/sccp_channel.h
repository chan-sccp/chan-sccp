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
 * $Date: 2015-01-26 17:11:20 +0100 (ma, 26 jan 2015) $
 * $Revision: 5889 $  
 */

#ifndef __SCCP_CHANNEL_H
#define __SCCP_CHANNEL_H

#define sccp_channel_retain(_x) 	({ast_assert(_x != NULL);sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_channel_release(_x) 	({ast_assert(_x != NULL);sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_channel_refreplace(_x, _y)	sccp_refcount_replace((void **)&_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)

/* live cycle */
sccp_channel_t *sccp_channel_allocate(sccp_line_t * l, sccp_device_t * device);					// device is optional
sccp_channel_t *sccp_channel_new_feature_call(sccp_line_t * l, sccp_device_t * device, sccp_feature_type_t feature, PBX_CHANNEL_TYPE * parentChannel, const void *ids);
sccp_channel_t *sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, const char *dial, uint8_t calltype, PBX_CHANNEL_TYPE * parentChannel, const void *ids);

void sccp_channel_updateChannelDesignator(sccp_channel_t * c);
void sccp_channel_updateChannelCapability(sccp_channel_t * channel);
void sccp_channel_send_callinfo(const sccp_device_t * device, const sccp_channel_t * c);
void sccp_channel_send_callinfo2(sccp_channel_t * c);
void sccp_channel_setChannelstate(sccp_channel_t * channel, sccp_channelstate_t state);
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
void sccp_channel_closeReceiveChannel(sccp_channel_t * c, boolean_t KeepPortOpen);
void sccp_channel_updateReceiveChannel(sccp_channel_t * c);
void sccp_channel_openMultiMediaReceiveChannel(sccp_channel_t * channel);
void sccp_channel_closeMultiMediaReceiveChannel(sccp_channel_t * channel, boolean_t KeepPortOpen);
void sccp_channel_updateMultiMediaReceiveChannel(sccp_channel_t * channel);

void sccp_channel_startMediaTransmission(sccp_channel_t * c);
void sccp_channel_stopMediaTransmission(sccp_channel_t * c, boolean_t KeepPortOpen);
void sccp_channel_updateMediaTransmission(sccp_channel_t * channel);
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel);
void sccp_channel_stopMultiMediaTransmission(sccp_channel_t * channel, boolean_t KeepPortOpen);
void sccp_channel_updateMultiMediaTransmission(sccp_channel_t * channel);

void sccp_channel_closeAllMediaTransmitAndReceive(sccp_device_t * d, sccp_channel_t * channel);

boolean_t sccp_channel_transfer_on_hangup(sccp_channel_t * channel);
gcc_inline void sccp_channel_stop_schedule_digittimout(sccp_channel_t * channel);
gcc_inline void sccp_channel_schedule_hangup(sccp_channel_t * channel, uint timeout);
gcc_inline void sccp_channel_schedule_digittimout(sccp_channel_t * channel, uint timeout);
void sccp_channel_end_forwarding_channel(sccp_channel_t * channel);
void sccp_channel_endcall(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
void sccp_channel_answer(const sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_stop_and_deny_scheduled_tasks(sccp_channel_t * channel);
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
boolean_t sccp_channel_setVideoMode(sccp_channel_t * c, const void *data);
int sccp_channel_callwaiting_tone_interval(sccp_device_t * d, sccp_channel_t * c);
const char *sccp_channel_getLinkedId(const sccp_channel_t * channel);

// find channel
sccp_channel_t *sccp_channel_find_byid(uint32_t id);
sccp_channel_t *sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id);
sccp_channel_t *sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, sccp_channelstate_t state);
sccp_channel_t *sccp_channel_find_bystate_on_device(sccp_device_t * d, sccp_channelstate_t state);
sccp_channel_t *sccp_find_channel_by_lineInstance_and_callid(const sccp_device_t * d, const uint32_t lineInstance, const uint32_t callid);
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid);
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c);
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d);

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
