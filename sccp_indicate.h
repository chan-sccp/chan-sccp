#ifndef __SCCP_INDICATE_H
#define __SCCP_INDICATE_H

#define SCCP_INDICATE_NOLOCK 	0
#define SCCP_INDICATE_LOCK		1

void sccp_indicate_debug(int mode, char * file, int line, const char * pretty_function, sccp_channel_t * c, uint8_t state);
void __sccp_indicate_lock(sccp_channel_t * c, uint8_t state);
void __sccp_indicate_nolock(sccp_channel_t * c, uint8_t state);
const char * sccp_indicate2str(uint8_t state);
const char * sccp_callstate2str(uint8_t state);

#ifndef CS_DEBUG_INDICATIONS
#define sccp_indicate_lock(x, y)	__sccp_indicate_lock(x, y)
#define sccp_indicate_nolock(x, y)	__sccp_indicate_nolock(x, y)
#else
#define sccp_indicate_lock(x, y) 	sccp_indicate_debug(SCCP_INDICATE_LOCK, __FILE__, __LINE__, __PRETTY_FUNCTION__, x, y);
#define sccp_indicate_nolock(x, y) 	sccp_indicate_debug(SCCP_INDICATE_NOLOCK, __FILE__, __LINE__, __PRETTY_FUNCTION__, x, y);
#endif

#endif

