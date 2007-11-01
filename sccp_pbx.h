#ifndef __SCCP_PBX_H
#define __SCCP_PBX_H

uint8_t sccp_pbx_channel_allocate(sccp_channel_t * c);
void * sccp_pbx_startchannel (void *data);
void start_rtp(sccp_channel_t * sub);

#ifdef CS_AST_HAS_TECH_PVT
const struct ast_channel_tech sccp_tech;
#endif

void sccp_pbx_senddigit(sccp_channel_t * c, char digit);
void sccp_pbx_senddigits(sccp_channel_t * c, char digits[AST_MAX_EXTENSION]);

#endif
