
/*!
 * \file 	sccp_pbx.h
 * \brief 	SCCP PBX Header
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
#ifndef __SCCP_PBX_H
#    define __SCCP_PBX_H

#    ifndef CS_AST_HAS_RTP_ENGINE
#        define sccp_rtp_write	ast_rtp_write
#        	 if ASTERISK_VERSION_NUMBER >= 10400
extern struct ast_rtp_protocol sccp_rtp;					/*!< rtp definition, see sccp_pbx.c */
#        	  endif								// ASTERISK_VERSION_NUMBER >= 10400
#    else
#        define sccp_rtp_write	ast_rtp_instance_write
extern struct ast_rtp_glue sccp_rtp;
#    endif									// CS_AST_HAS_RTP_ENGINE

uint8_t sccp_pbx_channel_allocate_locked(sccp_channel_t * c);
int sccp_pbx_sched_dial(const void *data);
int sccp_pbx_helper(sccp_channel_t * c);
void *sccp_pbx_softswitch_locked(sccp_channel_t * c);
void start_rtp(sccp_channel_t * sub);

#    ifdef CS_AST_HAS_TECH_PVT
extern const struct ast_channel_tech sccp_tech;
#    endif									// CS_AST_HAS_TECH_PVT

void sccp_pbx_needcheckringback(sccp_device_t * d);

void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]);
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame *f);

#    if ASTERISK_VERSION_NUMBER >= 10400
int sccp_ast_queue_control(sccp_channel_t * c, enum ast_control_frame_type control);
#    else
int sccp_ast_queue_control(sccp_channel_t * c, uint8_t control);
#    endif									// ASTERISK_VERSION_NUMBER >= 10400

#    ifdef CS_AST_CONTROL_CONNECTED_LINE
static void sccp_pbx_update_connectedline(sccp_channel_t * channel, const void *data, size_t datalen);
#    endif									// CS_AST_CONTROL_CONNECTED_LINE

#    if ASTERISK_VERSION_NUMBER > 10400
enum ast_bridge_result sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms);
#    endif									// ASTERISK_VERSION_NUMBER > 10400

int sccp_pbx_transfer(struct ast_channel *ast, const char *dest);

int acf_channel_read(struct ast_channel *ast, NEWCONST char *funcname, char *args, char *buf, size_t buflen);

typedef enum {
		SCCP_COPYVARIABLE_NORMAL		= 1 << 0,
		SCCP_COPYVARIABLE_SOFTTRANSFERABLE	= 1 << 1,
		SCCP_COPYVARIABLE_HARDTRANSFERABLE	= 1 << 2,
} sccp_copyVariablesFlags_t;
/**
 * copy channel variables from sourceChannel to destinationChannel.
 * \param flags use flags to desicde what kind of variables to copy
 */
void sccp_pbx_copyChannelVariables(struct ast_channel *sourceChannel, struct ast_channel *destinationChannel, uint8_t flags);
#endif										// __SCCP_PBX_H
