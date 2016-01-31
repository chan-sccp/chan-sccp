/*!
 * \file        sccp_softkeys.h
 * \brief       SCCP SoftKeys Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Revision$  
 */
#pragma once
typedef struct sccp_softkeyMap_cb sccp_softkeyMap_cb_t;

void sccp_softkey_pre_reload(void);
void sccp_softkey_post_reload(void);
void sccp_softkey_clear(void);

sccp_softkeyMap_cb_t *sccp_softkeyMap_copyStaticallyMapped(void);
boolean_t sccp_softkeyMap_replaceCallBackByUriAction(sccp_softkeyMap_cb_t * const softkeyMap, uint32_t event, char *hookstr);
boolean_t sccp_SoftkeyMap_execCallbackByEvent(devicePtr d, linePtr l, uint32_t lineInstance, channelPtr c, uint32_t event);
void sccp_softkey_setSoftkeyState(devicePtr device, uint8_t softKeySet, uint8_t softKey, boolean_t enable);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
