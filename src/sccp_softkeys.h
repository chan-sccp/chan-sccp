/*!
 * \file	sccp_softkeys.h
 * \brief       SCCP SoftKeys Header
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
#ifndef __SCCP_SOFTKEYS_H
#define __SCCP_SOFTKEYS_H
typedef struct sccp_softkeyMap_cb sccp_softkeyMap_cb_t;

void sccp_softkey_pre_reload(void);
void sccp_softkey_post_reload(void);
void sccp_softkey_clear(void);
#ifdef CS_EXPERIMENTAL
sccp_softkeyMap_cb_t *sccp_softkeyMap_copyStaticallyMapped(void);
boolean_t sccp_softkeyMap_replaceCallBackByUriAction( sccp_softkeyMap_cb_t *softkeyMap, uint32_t event, char *hookstr);
#endif
boolean_t sccp_SoftkeyMap_execCallbackByEvent(sccp_device_t *d, sccp_line_t *l, uint32_t lineInstance, sccp_channel_t *c, uint32_t event);
void sccp_softkey_setSoftkeyState(sccp_device_t * device, uint8_t softKeySet, uint8_t softKey, boolean_t enable);
#endif
