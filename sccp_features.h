// callforward
sccp_channel_t * sccp_feat_handle_callforward(sccp_line_t * l, uint8_t type);

#ifdef CS_SCCP_PICKUP
int sccp_feat_grouppickup(sccp_line_t * l);
int sccp_feat_directpickup(sccp_channel_t * c, char *exten);
sccp_channel_t * sccp_feat_handle_directpickup(sccp_line_t * l);
#endif

void sccp_feat_updatecid(sccp_channel_t * c);

// Voicemail wrapper
void sccp_feat_voicemail(sccp_device_t * d, int line_instance);
// Divert call to VoiceMail (was TRANSFVM)
void sccp_feat_idivert(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// Zaptel conference
void sccp_feat_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// Zaptel conference add user
void sccp_feat_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
// MeetMe wrapper
sccp_channel_t * sccp_feat_handle_meetme(sccp_line_t * l);
// Call Barge
sccp_channel_t * sccp_feat_handle_barge(sccp_line_t * l);
int sccp_feat_barge(sccp_channel_t * c, char *exten);
// Conference Barge
sccp_channel_t * sccp_feat_handle_cbarge(sccp_line_t * l);
int sccp_feat_cbarge(sccp_channel_t * c, char *conferencenum);
