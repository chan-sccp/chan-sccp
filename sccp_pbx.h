#ifndef __SCCP_PBX_H
#define __SCCP_PBX_H

#include <asterisk/pbx.h>

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c);
void * sccp_pbx_simpleswitch (sccp_channel_t * c);
void start_rtp(sccp_channel_t * sub);

#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
#endif

void sccp_pbx_needcheckringback(sccp_device_t * d);

void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]);
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame * f);
#ifndef ASTERISK_CONF_1_2
int sccp_ast_queue_control(sccp_channel_t * c, enum ast_control_frame_type control);
#else
int sccp_ast_queue_control(sccp_channel_t * c, uint8_t control);
#endif
#endif
