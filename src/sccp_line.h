/*!
 * \file 	sccp_line.h
 * \brief 	SCCP Line Header
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

#ifndef __SCCP_LINE_H
#define __SCCP_LINE_H


#ifdef CS_DYNAMIC_CONFIG
void sccp_line_pre_reload(void);
void sccp_line_post_reload(void);
#endif

sccp_line_t * sccp_line_create(void);
sccp_line_t *sccp_line_applyDefaults(sccp_line_t *l);
sccp_line_t *sccp_line_addToGlobals(sccp_line_t *line);
void sccp_line_kill(sccp_line_t * l);
void sccp_line_clean(sccp_line_t * l,boolean_t destroy);
int sccp_line_destroy(const void *ptr);
void sccp_line_delete_nolock(sccp_line_t * l);
void sccp_line_cfwd(sccp_line_t * l, sccp_device_t *device, uint8_t type, char * number);
void sccp_line_addDevice(sccp_line_t * l, sccp_device_t *device, uint8_t lineInstance, struct subscriptionId *subscriptionId);
void sccp_line_removeDevice(sccp_line_t * l, sccp_device_t *device);
void sccp_line_addChannel(sccp_line_t * l, sccp_channel_t *channel);
void sccp_line_removeChannel(sccp_line_t * l, sccp_channel_t *channel);
#ifdef CS_DYNAMIC_CONFIG
void register_exten(sccp_line_t *l,struct subscriptionId *subscriptionId);
void unregister_exten(sccp_line_t *l,struct subscriptionId *subscriptionId);
sccp_line_t * sccp_clone_line(sccp_line_t *orig_line);
void sccp_duplicate_line_mailbox_list(sccp_line_t *new_line, sccp_line_t *orig_line);
void sccp_duplicate_line_linedevices_list(sccp_line_t *new_line, sccp_line_t *orig_line);
sccp_diff_t sccp_line_changed(sccp_line_t *line_a,sccp_line_t *line_b);
#endif /* CS_DYNAMIC_CONFIG */

#endif /* __SCCP_LINE_H */
