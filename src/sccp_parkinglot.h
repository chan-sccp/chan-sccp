/*!
 * \file	sccp_parkinglot.h
 * \brief	SCCP ParkingLot Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#pragma once

#if ASTERISK_VERSION_GROUP < 113
#define PARKING_PREFIX ""
#define PARKING_FROM "From"
#define PARKING_SLOT "Exten"
#else
#define PARKING_PREFIX "Parkee"
#define PARKING_FROM "ParkeeExten"
#define PARKING_SLOT "ParkingSpace"
#endif

// forward declarations
struct parkinglot;
typedef struct parkinglot sccp_parkinglot_t;

__BEGIN_C_EXTERN__
typedef struct {
	int (*attachObserver)(const char *parkinglot, sccp_device_t * device, uint8_t instance);
	int (*detachObserver)(const char *parkinglot, sccp_device_t * device, uint8_t instance);
	int (*addSlot)(const char *parkinglot, int slot, struct message *m);
	int (*removeSlot)(const char *parkinglot, int slot);
	void (*showCXML) (const char *parkinglot, constDevicePtr d, uint8_t instance);
} ParkingLotInterface;

extern const ParkingLotInterface iParkingLot;

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
