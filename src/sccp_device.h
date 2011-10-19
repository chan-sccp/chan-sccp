
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
#    define __SCCP_DEVICE_H

#    ifdef CS_DYNAMIC_CONFIG
void sccp_device_pre_reload(void);
void sccp_device_post_reload(void);
#    endif

/*!
 * \brief SCCP Device Indication Callback Structure
 */
struct sccp_device_indication_cb {
	void (*const remoteHold) (const sccp_device_t *device, uint8_t lineInstance, uint8_t callid, uint8_t callPriority, uint8_t callPrivacy);
};

//#define sccp_dev_check_displayprompt(x) sccp_dev_check_displayprompt_debug(x, __FILE__, __LINE__, __PRETTY_FUNCTION__);
void sccp_dev_check_displayprompt(sccp_device_t * d);

sccp_device_t *sccp_device_create(void);
sccp_device_t *sccp_device_createAnonymous(const char *name);
void sccp_device_setIndicationProtocol(sccp_device_t *device);
sccp_device_t *sccp_device_addToGlobals(sccp_device_t * device);
void sccp_dev_build_buttontemplate(sccp_device_t * d, btnlist * btn);
sccp_moo_t *sccp_build_packet(sccp_message_t t, size_t pkt_len);
int sccp_dev_send(const sccp_device_t * d, sccp_moo_t * r);
void sccp_dev_sendmsg(const sccp_device_t * d, sccp_message_t t);

void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t line, uint32_t callid, uint8_t opt);
void sccp_dev_set_ringer(const sccp_device_t * d, uint8_t opt, uint8_t lineInstance, uint32_t callid);
void sccp_dev_cleardisplay(const sccp_device_t * d);


void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt);

void sccp_dev_set_speaker(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_cplane(sccp_line_t * l, uint8_t lineInstance, sccp_device_t * device, int status);
void sccp_dev_deactivate_cplane(sccp_device_t * d);
void sccp_dev_starttone(const sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout);
void sccp_dev_stoptone(sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_clearprompt(const sccp_device_t * d, uint8_t lineInstance, uint32_t callid);

#define sccp_dev_display(p,q) sccp_dev_display_debug(p, q, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprompt(p, q, r, s, t) sccp_dev_displayprompt_debug(p, q, r, s, t, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displaynotify(p,q,r) sccp_dev_displaynotify_debug(p,q,r, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprinotify(p,q,r,s) sccp_dev_displayprinotify_debug(p,q,r,s,__FILE__, __LINE__, __PRETTY_FUNCTION__)
void sccp_dev_display_debug(const sccp_device_t * d, const char *msg, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprompt_debug(const sccp_device_t * d, const uint8_t lineInstance, const uint32_t callid, const char *msg, int timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displaynotify_debug(const sccp_device_t * d, const char *msg, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprinotify_debug(const sccp_device_t * d, const char *msg, const uint8_t priority, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_cleardisplaynotify(const sccp_device_t * d);
void sccp_dev_cleardisplayprinotify(const sccp_device_t * d);

sccp_speed_t *sccp_dev_speed_find_byindex(sccp_device_t * d, uint16_t instance, uint8_t type);
sccp_line_t *sccp_dev_get_activeline(sccp_device_t * d);
void sccp_dev_set_activeline(sccp_device_t * device, sccp_line_t * l);

void sccp_dev_select_line(sccp_device_t * d, sccp_line_t * l);
//void sccp_dev_set_lamp(const sccp_device_t * d, uint16_t stimulus, uint16_t instance, uint8_t lampMode);
void sccp_dev_forward_status(sccp_line_t * l, uint8_t lineInstance, sccp_device_t * device);
//boolean_t sccp_dev_display_cfwd(sccp_device_t * device, boolean_t force);
int sccp_device_check_ringback(sccp_device_t * d);
void *sccp_dev_postregistration(void *data);
void sccp_dev_clean(sccp_device_t * d, boolean_t destroy, uint8_t cleanupTime);
//sccp_service_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance);
sccp_buttonconfig_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance);
uint8_t sccp_device_find_index_for_line(const sccp_device_t * d, const char *lineName);

//void sccp_device_removeLine(sccp_device_t * device, sccp_line_t * l);
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type);
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state, skinny_callPriority_t priority, skinny_callinfo_visibility_t visibility);
int sccp_device_destroy(const void *ptr);
boolean_t sccp_device_isVideoSupported(const sccp_device_t * device);

uint8_t sccp_device_numberOfChannels(const sccp_device_t * device);

#    define REQ(x,y) x = sccp_build_packet(y, sizeof(x->msg.y))
#    define REQCMD(x,y) x = sccp_build_packet(y, 0)

void sccp_dev_keypadbutton(sccp_device_t * d, char digit, uint8_t line, uint32_t callid);
boolean_t sccp_device_check_update(sccp_device_t * d);

void sccp_dev_set_message(sccp_device_t * d, const char *msg, const int timeout, const boolean_t storedb, const boolean_t beep);
void sccp_dev_clear_message(sccp_device_t * d, const boolean_t cleardb);

void sccp_device_addMessageToStack(sccp_device_t *device, const uint8_t priority, const char *message);
void sccp_device_clearMessageFromStack(sccp_device_t *device, const uint8_t priority);
void sccp_device_featureChangedDisplay(const sccp_event_t ** event);
#endif										/* __SCCP_DEVICE_H */
