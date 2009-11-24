/*!
 * \file 	sccp_indicate.h
 * \brief 	SCCP Indicate Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * 
 */
 
#ifndef __SCCP_INDICATE_H
#define __SCCP_INDICATE_H

#define SCCP_INDICATE_NOLOCK 	0
#define SCCP_INDICATE_LOCK		1

void __sccp_indicate_nolock(sccp_device_t *device, sccp_channel_t * c, uint8_t state, uint8_t debug, char * file, int line, const char * pretty_function);

#define __sccp_indicate_lock(w, x, y, z) \
		while(x && sccp_channel_trylock(x)) { \
			sccp_log(99)(VERBOSE_PREFIX_1 "[SCCP LOOP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__); \
			usleep(200);  } \
		if (x) { \
			if (z) \
				sccp_log(93)(VERBOSE_PREFIX_1 "SCCP: [INDICATE] mode '%s' in file '%s', on line %d (%s)\n", "LOCK", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
			__sccp_indicate_nolock(w, x, y, 0, NULL, 0, NULL); \
			sccp_channel_unlock(x); \
		} else { \
			ast_log(LOG_ERROR, "SCCP: (sccp_indicate_lock) No channel to indicate.\n"); \
		} \
 
const char * sccp_indicate2str(uint8_t state);
const char * sccp_callstate2str(uint8_t state);
void __sccp_indicate_remote_device(sccp_device_t *device, const sccp_channel_t * c, uint8_t state, uint8_t debug, char * file, int line, const char * pretty_function);


#ifdef CS_DEBUG_INDICATIONS
#define sccp_indicate_lock(x, y, z)	__sccp_indicate_lock(x, y, z, 1)
#define sccp_indicate_nolock(x, y, z)	__sccp_indicate_nolock(x, y, z, 1, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
#define sccp_indicate_lock(x, y, z)	__sccp_indicate_lock(x, y, z, 0)
#define sccp_indicate_nolock(x, y, z)	__sccp_indicate_nolock(x, y, z, 0, NULL, 0, NULL)
#endif

#endif /* __SCCP_INDICATE_H */

