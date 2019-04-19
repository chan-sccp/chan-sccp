/*!
 * \file        sccp_linedevice.h
 * \brief       SCCP LineDevice Header
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        Reworked, but based on chan_sccp code.
 *               The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *               Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *               See the LICENSE file at the top of the source tree.
 */
#pragma once
#include "config.h"
__BEGIN_C_EXTERN__

#define sccp_linedevice_retain(_x)		sccp_refcount_retain_type(sccp_linedevice_t, _x)
#define sccp_linedevice_release(_x)		sccp_refcount_release_type(sccp_linedevice_t, _x)
#define sccp_linedevice_refreplace(_x, _y)	sccp_refcount_refreplace_type(sccp_linedevice_t, _x, _y)

/*!
 * \brief SCCP cfwd information
 */
struct sccp_cfwd_information {
	boolean_t enabled;
	char number[SCCP_MAX_EXTENSION];
};
typedef struct sccp_cfwd_information sccp_cfwd_information_t;

/*!
 * \brief SCCP LineDevice Structure
 */
struct sccp_linedevice {
	sccp_device_t *device;											/*!< SCCP Device */
	sccp_line_t *line;											/*!< SCCP Line */
	SCCP_LIST_ENTRY (sccp_linedevice_t) list;								/*!< Device Linked List Entry */

	sccp_cfwd_information_t cfwdAll;									/*!< cfwdAll information */
	sccp_cfwd_information_t cfwdBusy;									/*!< cfwdBusy information */
	//sccp_cfwd_information_t cfwdNoAnswer;									/*!< cfwdNoAnswer information */

	sccp_subscription_id_t subscriptionId;									/*!< for addressing individual devices on shared line */
	char label[SCCP_MAX_LABEL];										/*!<  */

	uint8_t lineInstance;											/*!< line instance of this->line on this->device */
	boolean_t (*isPickupAllowed) (void);
	void (*resetPickup)(lineDevicePtr ld);
	void (*disallowPickup)(lineDevicePtr ld);
	const sccp_cfwd_information_t * const (*getCallForward)(constLineDevicePtr ld, sccp_callforward_t type);
	void (*setCallForward)(lineDevicePtr ld, sccp_callforward_t type, sccp_cfwd_information_t *value);
};														/*!< SCCP Line-Device Structure */

SCCP_API void SCCP_CALL sccp_linedevice_allocate(constDevicePtr d, constLinePtr l, uint8_t lineInstance, sccp_subscription_id_t *subscriptionId);
SCCP_API void SCCP_CALL sccp_linedevice_destroy(constDevicePtr d, linePtr l);

SCCP_API void SCCP_CALL sccp_linedevice_createLineButtonsArray(devicePtr device);
SCCP_API void SCCP_CALL sccp_linedevice_deleteLineButtonsArray(devicePtr device);

#define sccp_linedevice_find(_x,_y) __sccp_linedevice_find(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_linedevice_findByLineInstance(_x,_y) __sccp_linedevice_findByLineInstance(_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)
SCCP_API sccp_linedevice_t * SCCP_CALL __sccp_linedevice_find(const sccp_device_t * device, const sccp_line_t * line, const char *filename, int lineno, const char *func);
SCCP_API sccp_linedevice_t * SCCP_CALL __sccp_linedevice_findByLineInstance(const sccp_device_t * device, uint8_t instance, const char *filename, int lineno, const char *func);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
