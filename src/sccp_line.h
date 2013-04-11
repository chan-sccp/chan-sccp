/*!
 * \file        sccp_line.h
 * \brief       SCCP Line Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_LINE_H
#define __SCCP_LINE_H

#define sccp_linedevice_retain(x) 	sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_linedevice_release(x) 	sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_line_release(x) 		sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_line_retain(x) 		sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

void sccp_line_pre_reload(void);
void sccp_line_post_reload(void);

/* live cycle */
void *sccp_create_hotline(void);
sccp_line_t *sccp_line_create(const char *name);
void sccp_line_addToGlobals(sccp_line_t * line);
sccp_line_t *sccp_line_removeFromGlobals(sccp_line_t * line);
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t * device, uint8_t lineInstance, struct subscriptionId *subscriptionId);
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t * device);
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t * channel);
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t * c);
void sccp_line_clean(sccp_line_t * l, boolean_t destroy);
void sccp_line_kill_channels(sccp_line_t * l);

sccp_channelState_t sccp_line_getDNDChannelState(sccp_line_t * line);
void sccp_line_cfwd(sccp_line_t * l, sccp_device_t * device, uint8_t type, char *number);
#endif														/* __SCCP_LINE_H */
