#ifndef __SCCP_INDICATE_H
#define __SCCP_INDICATE_H

void sccp_indicate_lock(sccp_channel_t * c, uint8_t state);
void sccp_indicate_nolock(sccp_channel_t * c, uint8_t state);
const char * sccp_indicate2str(uint8_t state);
const char * sccp_callstate2str(uint8_t state);

#endif
