/*!
 * \file 	sccp_device.h
 * \brief 	SCCP Device Header
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

#ifndef __SCCP_DEVICE_H
#define __SCCP_DEVICE_H


#ifdef CS_DYNAMIC_CONFIG
void sccp_device_pre_reload(void);
void sccp_device_post_reload(void);
#endif

sccp_device_t * sccp_device_create(void);
sccp_device_t *sccp_device_applyDefaults(sccp_device_t *d);
sccp_device_t *sccp_device_addToGlobals(sccp_device_t *device);
int sccp_device_get_codec(struct ast_channel *ast);
void sccp_dev_build_buttontemplate(sccp_device_t *d, btnlist * btn);
sccp_moo_t * sccp_build_packet(sccp_message_t t, size_t pkt_len);
int  sccp_dev_send(const sccp_device_t * d, sccp_moo_t * r);
void sccp_dev_sendmsg(sccp_device_t * d, sccp_message_t t);

void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt);
void sccp_dev_set_ringer(sccp_device_t * d, uint8_t opt, uint32_t line, uint32_t callid);
void sccp_dev_cleardisplay(sccp_device_t * d);
void sccp_dev_display(sccp_device_t * d, char * msg);
void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt);

void sccp_dev_set_speaker(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_mwi(sccp_device_t * d, sccp_line_t * l, uint8_t hasMail);
void sccp_dev_set_cplane(sccp_line_t * l, sccp_device_t *device, int status);
void sccp_dev_deactivate_cplane(sccp_device_t * d);
void sccp_dev_starttone(sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout);
void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_clearprompt(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_displayprompt(sccp_device_t * d, uint8_t line, uint32_t callid, char * msg, int timeout);
void sccp_dev_cleardisplaynotify(sccp_device_t * d);
void sccp_dev_displaynotify(sccp_device_t * d, char * msg, uint32_t timeout);
void sccp_dev_cleardisplayprinotify(sccp_device_t * d);
void sccp_dev_displayprinotify(sccp_device_t * d, char * msg, uint32_t priority, uint32_t timeout);

sccp_speed_t *sccp_dev_speed_find_byindex(sccp_device_t * d, uint16_t instance, uint8_t type);
sccp_line_t * sccp_dev_get_activeline(sccp_device_t * d);
void sccp_dev_set_activeline(sccp_device_t *device, sccp_line_t * l);
void sccp_dev_check_displayprompt(sccp_device_t * d);
void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * l);
void sccp_dev_set_lamp(const sccp_device_t * d, uint16_t stimulus, uint16_t instance, uint8_t lampMode);
void sccp_dev_forward_status(sccp_line_t * l, sccp_device_t *device);
int sccp_device_check_ringback(sccp_device_t * d);
void * sccp_dev_postregistration(void *data);
void sccp_dev_clean(sccp_device_t * d, boolean_t destroy, uint8_t cleanupTime);
sccp_service_t * sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance);
uint16_t sccp_device_find_index_for_line(const sccp_device_t * d, char *lineName);

void sccp_device_removeLine(sccp_device_t *device, sccp_line_t * l);
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type);
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state, skinny_callPriority_t priority, skinny_callinfo_visibility_t visibility);
int sccp_device_free(const void *ptr);
boolean_t sccp_device_isVideoSupported(const sccp_device_t *device);

uint8_t sccp_device_numberOfChannels(const sccp_device_t *device);

#define REQ(x,y) x = sccp_build_packet(y, sizeof(x->msg.y))
#define REQCMD(x,y) x = sccp_build_packet(y, 0)

#ifdef CS_DYNAMIC_CONFIG
sccp_device_t * sccp_clone_device(sccp_device_t *orig_device);

void sccp_duplicate_device_buttonconfig_list(sccp_device_t *new_device, sccp_device_t *orig_device);
void sccp_duplicate_device_hostname_list(sccp_device_t *new_device, sccp_device_t *orig_device);
void sccp_duplicate_device_selectedchannel_list(sccp_device_t *new_device,sccp_device_t *orig_device);
void sccp_duplicate_device_addon_list(sccp_device_t *new_device, sccp_device_t *orig_device);
sccp_diff_t sccp_device_changed(sccp_device_t *device_a, sccp_device_t *device_b);
sccp_diff_t sccp_buttonconfig_changed(sccp_buttonconfig_t *buttonconfig_a, sccp_buttonconfig_t *buttonconfig_b);
#endif

#endif /* __SCCP_DEVICE_H */

