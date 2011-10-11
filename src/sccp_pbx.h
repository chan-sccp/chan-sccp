
/*!
 * \file        sccp_pbx.h
 * \brief       SCCP PBX Asterisk Wrapper Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */
#ifndef __SCCP_PBX_H
#    define __SCCP_PBX_H

uint8_t sccp_pbx_channel_allocate_locked(sccp_channel_t * c);
int sccp_pbx_sched_dial(const void *data);
sccp_extension_status_t sccp_pbx_helper(sccp_channel_t * c);
void *sccp_pbx_softswitch_locked(sccp_channel_t * c);
void start_rtp(sccp_channel_t * sub);
void sccp_pbx_needcheckringback(sccp_device_t * d);
void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char *digits);
void sccp_pbx_update_connectedline(sccp_channel_t * channel, const void *data, size_t datalen);
int sccp_pbx_transfer(PBX_CHANNEL_TYPE * ast, const char *dest);
sccp_channel_t *sccp_pbx_getPeer(sccp_channel_t * channel);
int sccp_pbx_getCodecCapabilities(sccp_channel_t * channel, void **capabilities);
int sccp_pbx_getPeerCodecCapabilities(sccp_channel_t * channel, void **capabilities);
int sccp_pbx_hangup_locked(sccp_channel_t * c);
int sccp_pbx_call(PBX_CHANNEL_TYPE *ast, char *dest, int timeout);
int sccp_pbx_answer(sccp_channel_t * c);

//! \todo do we need this?
// It is a function so we can intervene in the standard asterisk bridge method. 
// At this moment it provides a little logging and switches a couple of DTMF signals off when bridging SCCP<->SCCP calls - DdG
//! \todo move this to pbx impl
int sccp_pbx_queue_control(sccp_channel_t * c, uint8_t control);

#endif