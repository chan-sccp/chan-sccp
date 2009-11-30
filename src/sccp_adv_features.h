/* this is a dummy file */
#define DUMMY 1

void dummyproc1(void);
void dummyproc2(void);
void sccp_advfeat_conference(sccp_device_t *d, sccp_line_t *l, sccp_channel_t *c);
void sccp_advfeat_join(sccp_device_t *d, sccp_line_t *l, sccp_channel_t *c);
int sccp_advfeat_barge(sccp_channel_t * c, char * exten);
int sccp_advfeat_cbarge(sccp_channel_t * c, char * conferencenum);
