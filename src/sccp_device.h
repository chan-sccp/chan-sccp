/*!
 * \file	sccp_device.h
 * \brief       SCCP Device Header
 * \author	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *		The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *		Modified by Jan Czmok and Julien Goodwin
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_DEVICE_H
#define __SCCP_DEVICE_H

#define sccp_device_release(x) 		sccp_refcount_release(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_device_retain(x) 		sccp_refcount_retain(x, __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief SCCP Device Indication Callback Structure
 */
struct sccp_device_indication_cb {
	void (*const remoteHold) (const sccp_device_t * device, uint8_t lineInstance, uint8_t callid, uint8_t callpriority, uint8_t callPrivacy);
	void (*const remoteOffhook) (const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);
	void (*const remoteOnhook) (const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);
	void (*const offhook) (const sccp_device_t * device, sccp_linedevices_t * linedevice, uint8_t callid);
	void (*const onhook) (const sccp_device_t * device, const uint8_t lineInstance, uint8_t callid);
	void (*const dialing) (const sccp_device_t *device, const uint8_t lineInstance, const sccp_channel_t *channel);
	void (*const proceed) (const sccp_device_t *device, const uint8_t lineInstance, const sccp_channel_t *channel);
	void (*const connected) (const sccp_device_t * device, sccp_linedevices_t * linedevice, const sccp_channel_t * channel);
};

#define sccp_dev_display(p,q) sccp_dev_display_debug(p, q, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprompt(p, q, r, s, t) sccp_dev_displayprompt_debug(p, q, r, s, t, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displaynotify(p,q,r) sccp_dev_displaynotify_debug(p,q,r, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_dev_displayprinotify(p,q,r,s) sccp_dev_displayprinotify_debug(p,q,r,s,__FILE__, __LINE__, __PRETTY_FUNCTION__)

void sccp_device_pre_reload(void);
void sccp_device_post_reload(void);

/* live cycle */
sccp_device_t *sccp_device_create(const char *id);
sccp_device_t *sccp_device_createAnonymous(const char *name);
void sccp_device_addToGlobals(sccp_device_t * device);

sccp_line_t *sccp_dev_get_activeline(const sccp_device_t * d);
sccp_buttonconfig_t *sccp_dev_serviceURL_find_byindex(sccp_device_t * d, uint16_t instance);

#define REQ(x,y) x = sccp_build_packet(y, sizeof(x->data.y))
#define REQCMD(x,y) x = sccp_build_packet(y, 0)
sccp_msg_t *sccp_build_packet(sccp_mid_t t, size_t pkt_len);

void sccp_dev_check_displayprompt(sccp_device_t * d);
void sccp_device_setLastNumberDialed(sccp_device_t * device, const char *lastNumberDialed);
void sccp_device_setIndicationProtocol(sccp_device_t * device);
void sccp_dev_build_buttontemplate(sccp_device_t * d, btnlist * btn);
void sccp_dev_sendmsg(const sccp_device_t * d, sccp_mid_t t);
void sccp_dev_set_keyset(const sccp_device_t * d, uint8_t lineInstance, uint32_t callid, uint8_t softKeySetIndex);
void sccp_dev_set_ringer(const sccp_device_t * d, uint8_t opt, uint8_t lineInstance, uint32_t callid);
void sccp_dev_cleardisplay(const sccp_device_t * d);
void sccp_dev_set_registered(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_speaker(const sccp_device_t * d, uint8_t opt);
void sccp_dev_set_microphone(sccp_device_t * d, uint8_t opt);
void sccp_dev_set_cplane(const sccp_device_t * device, uint8_t lineInstance, int status);
void sccp_dev_deactivate_cplane(sccp_device_t * d);
void sccp_dev_starttone(const sccp_device_t * d, uint8_t tone, uint8_t line, uint32_t callid, uint32_t timeout);
void sccp_dev_stoptone(const sccp_device_t * d, uint8_t line, uint32_t callid);
void sccp_dev_clearprompt(const sccp_device_t * d, uint8_t lineInstance, uint32_t callid);
void sccp_dev_display_debug(const sccp_device_t * d, const char *msg, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprompt_debug(const sccp_device_t * d, const uint8_t lineInstance, const uint32_t callid, const char *msg, int timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displaynotify_debug(const sccp_device_t * d, const char *msg, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_displayprinotify_debug(const sccp_device_t * d, const char *msg, const uint8_t priority, const uint8_t timeout, const char *file, const int lineno, const char *pretty_function);
void sccp_dev_cleardisplaynotify(const sccp_device_t * d);
void sccp_dev_cleardisplayprinotify(const sccp_device_t * d);
void sccp_dev_speed_find_byindex(sccp_device_t * d, uint16_t instance, boolean_t withHint, sccp_speed_t * k);
void sccp_dev_set_activeline(sccp_device_t * device, const sccp_line_t * l);
void sccp_dev_forward_status(sccp_line_t * l, uint8_t lineInstance, sccp_device_t * device);
void sccp_dev_postregistration(void *data);
void sccp_dev_clean(sccp_device_t * d, boolean_t destroy, uint8_t cleanupTime);
void sccp_dev_keypadbutton(sccp_device_t * d, char digit, uint8_t line, uint32_t callid);
void sccp_dev_set_message(sccp_device_t * d, const char *msg, const int timeout, const boolean_t storedb, const boolean_t beep);
void sccp_dev_clear_message(sccp_device_t * d, const boolean_t cleardb);
void sccp_device_addMessageToStack(sccp_device_t * device, const uint8_t priority, const char *message);
void sccp_device_clearMessageFromStack(sccp_device_t * device, const uint8_t priority);
void sccp_device_featureChangedDisplay(const sccp_event_t * event);
void sccp_device_sendcallstate(const sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state, skinny_callpriority_t priority, skinny_callinfo_visibility_t visibility);

int sccp_dev_send(const sccp_device_t * d, sccp_msg_t * msg);
int sccp_device_check_ringback(sccp_device_t * d);
int sccp_device_sendReset(sccp_device_t * d, uint8_t reset_type);

uint8_t sccp_device_find_index_for_line(const sccp_device_t * d, const char *lineName);
uint8_t sccp_device_numberOfChannels(const sccp_device_t * device);

boolean_t sccp_device_isVideoSupported(const sccp_device_t * device);
boolean_t sccp_device_check_update(sccp_device_t * d);

// find device
#if DEBUG
#define sccp_device_find_byid(_x,_y) __sccp_device_find_byid(_x,_y,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_device_t *__sccp_device_find_byid(const char *name, boolean_t useRealtime, const char *filename, int lineno, const char *func);

#define sccp_device_find_byname(_x) __sccp_device_find_byid(_x)
#ifdef CS_SCCP_REALTIME
#define sccp_device_find_realtime(_x) __sccp_device_find_realtime(_x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_device_t *__sccp_device_find_realtime(const char *name, const char *filename, int lineno, const char *func);

#define sccp_device_find_realtime_byid(_x) sccp_device_find_realtime(_x)
#define sccp_device_find_realtime_byname(_x) sccp_device_find_realtime(_x)
#endif
#else
sccp_device_t *sccp_device_find_byid(const char *name, boolean_t useRealtime);

#define sccp_device_find_byname(x) sccp_device_find_byid(x)
#ifdef CS_SCCP_REALTIME
sccp_device_t *sccp_device_find_realtime(const char *name);

#define sccp_device_find_realtime_byid(x) sccp_device_find_realtime(x)
#define sccp_device_find_realtime_byname(x) sccp_device_find_realtime(x)
#endif
#endif

#endif														/* __SCCP_DEVICE_H */
