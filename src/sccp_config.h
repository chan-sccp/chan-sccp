/*!
 * \file 	sccp_config.h
 * \brief 	SCCP Config Header
 * \author 	Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *
 */
#ifndef __SCCP_CONFIG_H
#define __SCCP_CONFIG_H

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

sccp_device_t *sccp_config_buildDevice(struct ast_variable *variable, const char *deviceName, boolean_t isRealtime);
sccp_line_t *sccp_config_buildLine(struct ast_variable *variable, const char *lineName, boolean_t isRealtime);
boolean_t sccp_config_general(void);
void sccp_config_readDevicesLines(sccp_readingtype_t readingtype);
void sccp_config_readLines(sccp_readingtype_t readingtype);
void sccp_config_readdevices(sccp_readingtype_t readingtype);

#endif /*__SCCP_CONFIG_H */
