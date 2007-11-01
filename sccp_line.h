#ifndef __SCCP_LINE_H
#define __SCCP_LINE_H

void sccp_line_kill(sccp_line_t * l);
void sccp_line_delete_nolock(sccp_line_t * l);
void sccp_line_cfwd(sccp_line_t * l, uint8_t type, char * number);

#endif
