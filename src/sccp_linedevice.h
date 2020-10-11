/*!
 * \file        sccp_linedevice.h
 * \brief       SCCP LineDevice Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
#pragma once

#define sccp_linedevice_retain(_x)         sccp_refcount_retain_type(sccp_linedevice_t, _x)
#define sccp_linedevice_release(_x)        sccp_refcount_release_type(sccp_linedevice_t, _x)
#define sccp_linedevice_refreplace(_x, _y) sccp_refcount_refreplace_type(sccp_linedevice_t, _x, _y)
#define sccp_line_refreplace(_x, _y)       sccp_refcount_refreplace_type(sccp_line_t, _x, _y)
/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information {
	boolean_t enabled;
	char number[SCCP_MAX_EXTENSION];
};

/*!
 * \brief SCCP Line-Devices Structure
 */
struct sccp_linedevice {
	devicePtr device;                                                               //!< SCCP Device
	linePtr line;                                                                   //!< SCCP Line
	SCCP_LIST_ENTRY(sccp_linedevice_t) list;                                        //!< Device Linked List Entry

	sccp_cfwd_information_t cfwd[SCCP_CFWD_SENTINEL];                                        //!< cfwd information

	sccp_subscription_t subscription;                                        //!< for addressing individual devices on shared line
	char label[SCCP_MAX_LABEL];                                                   //!<
	uint8_t lineInstance;                                                         //!< line instance of this->line on this->device
}; /*!< SCCP Line-Device Structure */

SCCP_API void SCCP_CALL sccp_linedevice_create(constDevicePtr d, constLinePtr line, uint8_t lineInstance, sccp_subscription_t * subscription);
SCCP_API void SCCP_CALL sccp_linedevice_remove(constDevicePtr device, linePtr l);
SCCP_API void SCCP_CALL sccp_linedevice_cfwd(lineDevicePtr ld, sccp_cfwd_t type, char * number);
SCCP_API const char * const SCCP_CALL sccp_linedevice_get_cfwd_string(constLineDevicePtr ld, char * const buffer, size_t size);
SCCP_API void SCCP_CALL sccp_linedevice_indicateMWI(constLineDevicePtr ld);

#define sccp_linedevice_find(_x, _y)               __sccp_linedevice_find(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_linedevice_findByLineinstance(_x, _y) __sccp_linedevice_findByLineinstance(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API lineDevicePtr SCCP_CALL __sccp_linedevice_find(constDevicePtr device, constLinePtr line, const char * filename, int lineno, const char * func);
SCCP_API lineDevicePtr SCCP_CALL __sccp_linedevice_findByLineinstance(constDevicePtr device, uint16_t instance, const char * filename, int lineno, const char * func);
SCCP_API void SCCP_CALL sccp_linedevice_createButtonsArray(devicePtr device);
SCCP_API void SCCP_CALL sccp_linedevice_deleteButtonsArray(devicePtr device);
