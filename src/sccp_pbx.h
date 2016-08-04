/*!
 * \file        sccp_pbx.h
 * \brief       SCCP PBX Asterisk Wrapper Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#include "sccp_codec.h"		/* can be removed if we pass the sccp_codec capabilities by ref */
__BEGIN_C_EXTERN__
SCCP_API sccp_channel_request_status_t SCCP_CALL sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel);
SCCP_API uint8_t SCCP_CALL sccp_pbx_channel_allocate(sccp_channel_t * channel, const void *ids, const PBX_CHANNEL_TYPE * parentChannel);
SCCP_API int SCCP_CALL sccp_pbx_sched_dial(const void *data);
SCCP_API sccp_extension_status_t SCCP_CALL sccp_pbx_helper(sccp_channel_t * c);
SCCP_API void * SCCP_CALL sccp_pbx_softswitch(sccp_channel_t * channel);
SCCP_API void SCCP_CALL start_rtp(sccp_channel_t * sub);
SCCP_API void SCCP_CALL sccp_pbx_needcheckringback(sccp_device_t * d);
SCCP_API void SCCP_CALL sccp_pbx_senddigit(sccp_channel_t * c, char digit);
SCCP_API void SCCP_CALL sccp_pbx_senddigits(sccp_channel_t * c, const char *digits);
SCCP_API int SCCP_CALL sccp_pbx_transfer(PBX_CHANNEL_TYPE * ast, const char *dest);
SCCP_API sccp_channel_t *SCCP_CALL sccp_pbx_hangup(sccp_channel_t * channel);
SCCP_API int SCCP_CALL sccp_pbx_call(sccp_channel_t * c, char *dest, int timeout);
SCCP_API int SCCP_CALL sccp_pbx_answered(sccp_channel_t * channel);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
