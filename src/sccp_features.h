/*!
 * \file        sccp_features.h
 * \brief       SCCP Features Header
 * \author      Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 *
 * $Revision$  
 */
#pragma once
// callforward
void sccp_feat_handle_callforward(constLinePtr l, constDevicePtr device, sccp_callforward_t type);

#ifdef CS_SCCP_PICKUP
void sccp_feat_handle_directed_pickup(constDevicePtr d, constLinePtr l, channelPtr maybe_c);
int sccp_feat_directed_pickup(constDevicePtr d, channelPtr c, uint32_t lineInstance, const char *exten);
int sccp_feat_grouppickup(constDevicePtr d, constLinePtr l, uint32_t lineInstance, channelPtr maybe_c);
#endif
void sccp_feat_voicemail(constDevicePtr d, uint8_t line_instance);
void sccp_feat_idivert(constDevicePtr d, constLinePtr l, constChannelPtr c);
void sccp_feat_handle_conference(constDevicePtr d, constLinePtr l, uint8_t lineInstance, channelPtr c);
void sccp_feat_conference_start(constDevicePtr d, const uint32_t lineInstance, channelPtr c);
void sccp_feat_join(constDevicePtr d, constLinePtr l, uint8_t lineInstance, channelPtr c);
void sccp_feat_conflist(devicePtr d, uint8_t lineInstance, constChannelPtr c);
void sccp_feat_handle_meetme(constLinePtr l, uint8_t lineInstance, constDevicePtr d);
void sccp_feat_meetme_start(channelPtr c);
void sccp_feat_handle_barge(constLinePtr l, uint8_t lineInstance, constDevicePtr d);
int sccp_feat_barge(constChannelPtr c, const char *const exten);
void sccp_feat_handle_cbarge(constLinePtr l, uint8_t lineInstance, constDevicePtr d);
int sccp_feat_cbarge(constChannelPtr c, const char *const conferencenum);
void sccp_feat_adhocDial(constDevicePtr d, constLinePtr line);
void sccp_feat_changed(constDevicePtr device, const sccp_linedevices_t * const linedevice, sccp_feature_type_t featureType);
void sccp_feat_monitor(constDevicePtr device, constLinePtr no_line, uint32_t no_lineInstance, constChannelPtr maybe_channel);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
