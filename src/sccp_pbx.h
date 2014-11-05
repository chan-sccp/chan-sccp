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
#define __SCCP_PBX_H

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c, const void *ids, const PBX_CHANNEL_TYPE * parentChannel);
int sccp_pbx_sched_dial(const void *data);
sccp_extension_status_t sccp_pbx_helper(sccp_channel_t * c);
void *sccp_pbx_softswitch(sccp_channel_t * c);
void start_rtp(sccp_channel_t * sub);
void sccp_pbx_needcheckringback(sccp_device_t * d);
void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, const char *digits);
int sccp_pbx_transfer(PBX_CHANNEL_TYPE * ast, const char *dest);

int sccp_pbx_hangup(sccp_channel_t * c);
int sccp_pbx_call(sccp_channel_t * c, char *dest, int timeout);
int sccp_pbx_answer(sccp_channel_t * c);

//! \todo do we need this?
// It is a function so we can intervene in the standard asterisk bridge method. 
// At this moment it provides a little logging and switches a couple of DTMF signals off when bridging SCCP<->SCCP calls - DdG
//! \todo move this to pbx impl
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
