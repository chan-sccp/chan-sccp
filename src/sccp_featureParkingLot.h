/*!
 * \file	sccp_featureParkingLot.h
 * \brief	SCCP ParkingLot Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#pragma once
#ifdef CS_SCCP_PARK

//#define AMI13 "1.3"	// user by asterisk-11
//#define AMI25 "2.5.0"	// used by asterisk-12
//#define AMI28 "2.8"	// used by asterisk-13
//#if $AMI_VERSION == $AMI25 || $AMI_VERSION == $AMI28

#if ASTERISK_VERSION_GROUP >= 112
#define PARKING_PREFIX "Parkee"
#define PARKING_FROM "ParkeeExten"
#define PARKING_SLOT "ParkingSpace"
#else
#define PARKING_PREFIX ""
#define PARKING_FROM "From"
#define PARKING_SLOT "Exten"
#endif

#endif
__BEGIN_C_EXTERN__
typedef struct {
	int (*const attachObserver)(sccp_device_t * device, const sccp_buttonconfig_t * const buttonConfig);
	int (*const detachObserver)(sccp_device_t * device, const sccp_buttonconfig_t * const buttonConfig);
	int (*const addSlot)(const char * parkinglot, int slot, struct message * m);
	int (*const removeSlot)(const char * parkinglot, int slot);
	// void (*const showCXML) (const char *parkinglot, constDevicePtr d, uint8_t instance);
	void (*const handleButtonPress)(constDevicePtr d, const sccp_buttonconfig_t * const buttonConfig);
	void (*const handleDevice2User)(const char * parkinglot, constDevicePtr d, const char * slot_exten, uint8_t instance, uint32_t transactionId);
	void (*const notifyDevice)(constDevicePtr device, const sccp_buttonconfig_t * const buttonConfig);
} ParkingLotInterface;

extern const ParkingLotInterface iParkingLot;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
