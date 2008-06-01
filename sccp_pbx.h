#ifndef __SCCP_PBX_H
#define __SCCP_PBX_H

#include <asterisk/pbx.h>

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c);
static void * sccp_pbx_startchannel (void *data);
void start_rtp(sccp_channel_t * sub);

#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
#endif

void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]);
void sccp_queue_frame(sccp_channel_t * c, struct ast_frame * f);
#endif
