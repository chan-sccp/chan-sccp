
/*!
 * \file 	sccp_indicate.h
 * \brief 	SCCP Indicate Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */
#ifndef __SCCP_INDICATE_H
#    define __SCCP_INDICATE_H

#    define SCCP_INDICATE_NOLOCK 	0
#    define SCCP_INDICATE_LOCK		1

void __sccp_indicate_locked(sccp_device_t * device, sccp_channel_t * c, uint8_t state, uint8_t debug, char *file, int line, const char *pretty_function);

const char *sccp_indicate2str(uint8_t state);
const char *sccp_callstate2str(uint8_t state);

#    ifdef CS_DEBUG_INDICATIONS
#        define sccp_indicate_locked(x, y, z)	__sccp_indicate_locked(x, y, z, 1, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#    else
#        define sccp_indicate_locked(x, y, z)	__sccp_indicate_locked(x, y, z, 0, NULL, 0, NULL)
#    endif

#endif										/* __SCCP_INDICATE_H */
