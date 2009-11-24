/**
 *
 */

#include "config.h"
#include <stdint.h>

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "asterisk/channel.h"

#include "chan_sccp.h"
#include "sccp_dllists.h"
#include "sccp_protocol.h"










void sccp_config_addLine(sccp_device_t *device, char *lineName, uint8_t index);
void sccp_config_addEmpty(sccp_device_t *device, uint8_t index);
void sccp_config_addSpeeddial(sccp_device_t *device, char *label, char *extension, char *hint, uint8_t index);
void sccp_config_addFeature(sccp_device_t *device, char *label, char *featureID, char *args, uint8_t index);
void sccp_config_addService(sccp_device_t *device, char *label, char *url, uint8_t index);

sccp_device_t *sccp_config_buildDevice(struct ast_variable *variable, char *deviceName, boolean_t isRealtime);
sccp_line_t *sccp_config_buildLine(struct ast_variable *variable, const char *lineName, boolean_t isRealtime);
boolean_t sccp_config_general(void);
void sccp_config_readLines(sccp_readingtype_t readingtype);
void sccp_config_readdevices(sccp_readingtype_t readingtype);

