/*!
 * \file 	sccp_features.h
 * \brief 	SCCP Features Header
 * \author 	Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \since 	2009-01-16
 */

#ifndef __SCCP_FEATURES_H
#define __SCCP_FEATURES_H

// callforward
sccp_channel_t * sccp_feat_handle_callforward(sccp_line_t * l, sccp_device_t *device, uint8_t type);

#ifdef CS_SCCP_PICKUP
int sccp_feat_grouppickup(sccp_line_t * l, sccp_device_t *d);
int sccp_feat_directpickup(sccp_channel_t * c, char *exten);
sccp_channel_t * sccp_feat_handle_directpickup(sccp_line_t * l, sccp_device_t *d);
#endif

void sccp_feat_updatecid(sccp_channel_t * c);

// Voicemail wrapper
void sccp_feat_voicemail(sccp_device_t * d, uint8_t line_instance);
// Divert call to VoiceMail (was TRANSFVM)
void sccp_feat_idivert(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// Zaptel conference
void sccp_feat_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// Zaptel conference add user
void sccp_feat_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// MeetMe wrapper
sccp_channel_t * sccp_feat_handle_meetme(sccp_line_t * l, sccp_device_t *d);
// Call Barge
sccp_channel_t * sccp_feat_handle_barge(sccp_line_t * l, sccp_device_t *d);
int sccp_feat_barge(sccp_channel_t * c, char *exten);
// Conference Barge
sccp_channel_t * sccp_feat_handle_cbarge(sccp_line_t * l, sccp_device_t *d);
int sccp_feat_cbarge(sccp_channel_t * c, char *conferencenum);
void sccp_feat_hotline(sccp_device_t *d);
void sccp_feat_changed(sccp_device_t *device, sccp_feature_type_t featureType);
void sccp_feat_monitor(sccp_device_t *device, sccp_channel_t *channel);
void sccp_feat_channelStateChanged(sccp_device_t *device, sccp_channel_t * channel);
#endif /* __SCCP_FEATURES_H */

