/*!
 * \file        sccp_features.h
 * \brief       SCCP Features Header
 * \author      Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_FEATURES_H
#define __SCCP_FEATURES_H

// callforward
void sccp_feat_handle_callforward(constLinePtr l, constDevicePtr device, sccp_callforward_t type);

#ifdef CS_SCCP_PICKUP
int sccp_feat_grouppickup(linePtr l, devicePtr d);
int sccp_feat_directed_pickup(channelPtr c, char *exten);
void sccp_feat_handle_directed_pickup(linePtr l, uint8_t lineInstance, devicePtr d);
#endif
void sccp_feat_conflist(devicePtr d, linePtr l, uint8_t lineInstance, channelPtr c);
void sccp_feat_updatecid(channelPtr c);
void sccp_feat_meetme_start(channelPtr c);
void sccp_feat_voicemail(constDevicePtr d, uint8_t line_instance);
void sccp_feat_idivert(devicePtr d, linePtr l, channelPtr c);
void sccp_feat_handle_conference(constDevicePtr d, constLinePtr l, uint8_t lineInstance, channelPtr c);
void sccp_feat_conference_start(devicePtr d, linePtr l, const uint32_t lineInstance, channelPtr c);
void sccp_feat_join(devicePtr d, linePtr l, uint8_t lineInstance, channelPtr c);
void sccp_feat_adhocDial(constDevicePtr d, constLinePtr line);
void sccp_feat_changed(constDevicePtr device, sccp_linedevices_t * linedevice, sccp_feature_type_t featureType);
void sccp_feat_monitor(constDevicePtr device, constLinePtr no_line, uint32_t no_lineInstance, channelPtr channel);
void sccp_feat_channelstateChanged(devicePtr device, channelPtr channel);
void sccp_feat_handle_meetme(linePtr l, uint8_t lineInstance, devicePtr d);
void sccp_feat_handle_barge(linePtr l, uint8_t lineInstance, devicePtr d);
int sccp_feat_barge(channelPtr c, char *exten);
void sccp_feat_handle_cbarge(linePtr l, uint8_t lineInstance, devicePtr d);
int sccp_feat_cbarge(channelPtr c, char *conferencenum);
#endif														/* __SCCP_FEATURES_H */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
