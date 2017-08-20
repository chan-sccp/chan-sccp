/*!
 * \file	sccp_featureParkingLot.c
 * \brief	SCCP ParkingLot Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_featureParkingLot.h"

#ifdef CS_SCCP_PARK

#include "sccp_utils.h"
#include "sccp_vector.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_features.h"
#include "sccp_labels.h"

static const uint32_t appID = APPID_VISUALPARKINGLOT;

#ifdef HAVE_PBX_APP_H
#  include <asterisk/app.h>
#endif

/* asterisk-11 */
/*
Event: ParkedCall
Privilege: call,all
Timestamp: 1460205775.670404
Exten: 701
Channel: SCCP/10041-0000000e
Parkinglot: default
From: SCCP/10011-0000000f
Timeout: 45
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: <unknown>
ConnectedLineName: <unknown>
Uniqueid: 1460205775.48

Event: UnParkedCall
Privilege: call,all
Timestamp: 1460205785.934545
Exten: 701
Channel: SCCP/10041-0000000e
Parkinglot: default
From: SCCP/10031-00000010
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: <unknown>
ConnectedLineName: <unknown>
Uniqueid: 1460205775.48

Event: ParkedCallGiveUp
Privilege: call,all
Timestamp: 1460819185.922496
Exten: 701
Channel: SCCP/10041-00000001
Parkinglot: default
CallerIDNum: 10041
CallerIDName: PHONE4
ConnectedLineNum: 10011
ConnectedLineName: Diederik-Phone1
UniqueID: 1460819174.54

Event: ParkedCallTimeOut
Privilege: call,all
Timestamp: 1460974082.683646
Exten: 701
Channel: SCCP/10011-00000003
Parkinglot: default
CallerIDNum: 10011
CallerIDName: Diederik-Phone1
ConnectedLineNum: 10031
ConnectedLineName: Diederik-Phone3
UniqueID: 1460974037.17
*/

/* asterisk-13 */
/*
Event: ParkedCall
Privilege: call,all
SequenceNumber: 118
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461160476.0
ParkeeLinkedid: 1461160476.0
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 45
ParkingDuration: 0

UnParkedCall
Privilege: call,all
SequenceNumber: 1091
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461161791.29
ParkeeLinkedid: 1461161791.29
RetrieverChannel: SCCP/10041-00000003
RetrieverChannelState: 6
RetrieverChannelStateDesc: Up
RetrieverCallerIDNum: 10041
RetrieverCallerIDName: PHONE4
RetrieverConnectedLineNum: <unknown>
RetrieverConnectedLineName: <unknown>
RetrieverLanguage: en
RetrieverAccountCode: 79005
RetrieverContext: internal
RetrieverExten: 701
RetrieverPriority: 1
RetrieverUniqueid: 1461161803.31
RetrieverLinkedid: 1461161803.31
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 35
ParkingDuration: 10

Event: ParkedCallGiveUp
Privilege: call,all
SequenceNumber: 142
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000001
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: internal
ParkeeExten: 10031
ParkeePriority: 3
ParkeeUniqueid: 1461160476.0
ParkeeLinkedid: 1461160476.0
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 36
ParkingDuration: 9

Event: ParkedCallTimeOut
Privilege: call,all
SequenceNumber: 427
File: parking/parking_manager.c
Line: 676
Func: parked_call_message_response
ParkeeChannel: SCCP/10011-00000007
ParkeeChannelState: 6
ParkeeChannelStateDesc: Up
ParkeeCallerIDNum: 10011
ParkeeCallerIDName: Diederik-Phone1
ParkeeConnectedLineNum: <unknown>
ParkeeConnectedLineName: <unknown>
ParkeeLanguage: en
ParkeeAccountCode: 10011
ParkeeContext: park-dial
ParkeeExten: SCCP_10031
ParkeePriority: 1
ParkeeUniqueid: 1461160740.12
ParkeeLinkedid: 1461160740.12
ParkerDialString: SCCP/10031
Parkinglot: default
ParkingSpace: 701
ParkingTimeout: 0
ParkingDuration: 45
*/

/* forward declarations */
struct parkinglot;
typedef struct parkinglot sccp_parkinglot_t;
static void notifyLocked(sccp_parkinglot_t *pl);

typedef struct plslot plslot_t;
typedef struct plobserver plobserver_t;

/* private variables */
struct plslot {
	int slot;
	const char *exten;
	const char *from;
	const char *channel;
	const char *callerid_num;
	const char *callerid_name;
	const char *connectedline_num;
	const char *connectedline_name;
};

struct plobserver {
	sccp_device_t * device;
	uint8_t instance;
	uint8_t transactionId;
};

struct parkinglot {
	pbx_mutex_t lock;
	char *context;
	SCCP_VECTOR(, plobserver_t) observers;
	SCCP_VECTOR(, plslot_t) slots;
	SCCP_LIST_ENTRY(sccp_parkinglot_t) list;
};

#define ICONSTATE_NEW_ON 0x020303												// option:closed, color=yellow, flashspeed=slow
#define ICONSTATE_NEW_OFF 0x010000												// option:open, color=off, flashspeed=None
#define ICONSTATE_OLD_ON 1													// option:closed
#define ICONSTATE_OLD_OFF 0													// option:open

/* private functions */
#define sccp_parkinglot_lock(x)		pbx_mutex_lock(&((sccp_parkinglot_t * const)(x))->lock);				// discard const
#define sccp_parkinglot_unlock(x)	pbx_mutex_unlock(&((sccp_parkinglot_t * const)(x))->lock);				// discard const

SCCP_LIST_HEAD(, sccp_parkinglot_t) parkinglots;
#define OBSERVER_CB_CMP(elem, value) ((elem).device == (value).device && (elem).instance == (value).instance)
#define SLOT_CB_CMP(elem, value) ((elem).slot == (value))

#define SLOT_CLEANUP(elem) 							\
	if ((elem).exten) {sccp_free((elem).exten);}				\
	if ((elem).from) {sccp_free((elem).from);}				\
	if ((elem).channel) {sccp_free((elem).channel);}			\
	if ((elem).callerid_num) {sccp_free((elem).callerid_num);}		\
	if ((elem).callerid_name) {sccp_free((elem).callerid_name);}		\
	if ((elem).connectedline_num) {sccp_free((elem).connectedline_num);}	\
	if ((elem).connectedline_name) {sccp_free((elem).connectedline_name);}


/* exported functions */
static sccp_parkinglot_t * addParkinglot(const char *parkinglot)
{
	pbx_assert(parkinglot != NULL);

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (addParkinglot) %s\n", parkinglot);
	sccp_parkinglot_t *pl = sccp_calloc(sizeof(sccp_parkinglot_t), 1);

	pl->context = pbx_strdup(parkinglot);
	pbx_mutex_init(&pl->lock);
	SCCP_VECTOR_INIT(&pl->observers,1);
	SCCP_VECTOR_INIT(&pl->slots,1);

	SCCP_RWLIST_WRLOCK(&parkinglots);
	SCCP_RWLIST_INSERT_HEAD(&parkinglots, pl, list);
	SCCP_RWLIST_UNLOCK(&parkinglots);
	return pl;
}

static int removeParkinglot(sccp_parkinglot_t *pl)
{
	pbx_assert(pl != NULL && pl != NULL);

	int res = FALSE;
	sccp_parkinglot_t *removed = NULL;
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (removeParkinglot) %s\n", pl->context);

	SCCP_RWLIST_WRLOCK(&parkinglots);
	removed = SCCP_RWLIST_REMOVE(&parkinglots, pl, list);
	SCCP_RWLIST_UNLOCK(&parkinglots);
	sccp_parkinglot_unlock(pl);

	if (removed) {
		if (removed->context) {
			sccp_free(removed->context);
		}
		SCCP_VECTOR_RESET(&removed->observers, SCCP_VECTOR_ELEM_CLEANUP_NOOP);
		SCCP_VECTOR_FREE(&removed->observers);
		SCCP_VECTOR_RESET(&removed->slots, SLOT_CLEANUP);
		SCCP_VECTOR_FREE(&removed->slots);
		pbx_mutex_destroy(&removed->lock);
		res = TRUE;
	}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (removeParkinglot) done\n");
	return res;
}

/* returns locked pl */
static sccp_parkinglot_t * const findParkinglotByContext(const char *parkinglot)
{
	pbx_assert(parkinglot != NULL);

	sccp_parkinglot_t *pl = NULL;
	SCCP_RWLIST_RDLOCK(&parkinglots);
	SCCP_RWLIST_TRAVERSE(&parkinglots, pl, list) {
		sccp_parkinglot_lock(pl);
		if (sccp_strcaseequals(pl->context, parkinglot)) {
			//sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (findParkinglotByContext) found match:%s\n", pl->context);
			// returning locked
			break;
		}
		sccp_parkinglot_unlock(pl);
	}
	SCCP_RWLIST_UNLOCK(&parkinglots);
	return pl;
}

/* returns locked pl */
static sccp_parkinglot_t * const findCreateParkinglot(const char *parkinglot, boolean_t create)
{
	pbx_assert(parkinglot != NULL);

	//sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (findCreateParkinglot) %s (create:%s)\n", parkinglot, create ? "TRUE" : "FALSE");
	sccp_parkinglot_t *pl = findParkinglotByContext(parkinglot);
	if (!pl && create) {
		if (!(pl = addParkinglot(parkinglot))) {
			//pbx_log(LOG_NOTICE, "SCCP: (findCreateParkinglot) Could not add ParkingLot: %s\n", parkinglot);
			return NULL;
		}
		//sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (findCreateParkinglot) New %s Created\n", parkinglot);
		sccp_parkinglot_lock(pl);
	}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (findCreateParkinglot) Found:%s \n", pl ? "TRUE" : "FALSE");
	return pl;
}

// observer
static int attachObserver(const char *options, sccp_device_t * device, uint8_t instance)
{
	pbx_assert(options != NULL && device != NULL);
	int res = FALSE;

	char *parse = pbx_strdupa(options);
	AST_DECLARE_APP_ARGS(args, AST_APP_ARG(parkinglot); AST_APP_ARG(flags); );
	AST_STANDARD_APP_ARGS(args, parse);
	
	if (!sccp_strlen_zero(args.parkinglot)) {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (attachObserver) device:%s at instance:%d\n", args.parkinglot, device->id, instance);
		RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(args.parkinglot, TRUE), sccp_parkinglot_unlock);
		if (pl) {
			plobserver_t observer = {
				.device = device,
				.instance = instance,
				.transactionId = 0,
			};

			/* upgrade to wrlock */
			if (SCCP_VECTOR_APPEND(&pl->observers, observer) == 0) {
				res = TRUE;
			}
		}
	}
	return res;
}

static int detachObserver(const char *options, sccp_device_t * device, uint8_t instance)
{
	pbx_assert(options != NULL && device != NULL);
	int res = FALSE;

	char *parse = pbx_strdupa(options);
	AST_DECLARE_APP_ARGS(args, AST_APP_ARG(parkinglot); AST_APP_ARG(flags); );
	AST_STANDARD_APP_ARGS(args, parse);

	if (!sccp_strlen_zero(args.parkinglot)) {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (detachObserver) device:%s at instance:%d\n", args.parkinglot, device->id, instance);
		sccp_parkinglot_t *pl = findCreateParkinglot(args.parkinglot, FALSE);
		if (pl) {
			plobserver_t cmp = {
				.device = device,
				.instance = instance,
			};

			if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(&pl->observers, cmp, OBSERVER_CB_CMP, SCCP_VECTOR_ELEM_CLEANUP_NOOP) == 0) {
				if (SCCP_VECTOR_SIZE(&pl->observers) == 0) {
					removeParkinglot(pl);	// will unlock and destroy pl
				} else {
					sccp_parkinglot_unlock(pl);				
				}
				res = TRUE;
			} else {
				sccp_parkinglot_unlock(pl);
			}
		}
	}
	return res;
}

static char * const getParkingLotCXML(sccp_parkinglot_t *pl, int protocolversion, uint8_t instance, uint32_t transactionId, char **const outbuf)
{
	pbx_assert(pl != NULL && outbuf != NULL);

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (getParkingLotCXML) with version:%d\n", pl->context, protocolversion);
	*outbuf = NULL;
	if (SCCP_VECTOR_SIZE(&pl->slots)) {
		pbx_str_t *buf = ast_str_create(DEFAULT_PBX_STR_BUFFERSIZE);
		pbx_str_append(&buf, 0, "<?xml version=\"1.0\"?>");
		if (protocolversion < 15) {
			pbx_str_append(&buf, 0, "<CiscoIPPhoneMenu>");
		} else {
			pbx_str_append(&buf, 0, "<CiscoIPPhoneMenu appId='%d' onAppClosed='%d'>", appID, appID);
		}
		pbx_str_append(&buf, 0, "<Title>Parked Calls</Title>");
		pbx_str_append(&buf, 0, "<Prompt>Choose a ParkingLot Slot</Prompt>");
		uint8_t idx;
		for (idx = 0; idx < SCCP_VECTOR_SIZE(&pl->slots); idx++) {
			plslot_t *slot = SCCP_VECTOR_GET_ADDR(&pl->slots, idx);
			pbx_str_append(&buf, 0, "<MenuItem>");
			pbx_str_append(&buf, 0, "<Name>%s (%s) by %s</Name>", slot->callerid_name, slot->callerid_num, !sccp_strcaseequals(slot->connectedline_name, "<unknown>") ? slot->connectedline_name : slot->from);
			pbx_str_append(&buf, 0, "<URL>UserCallData:%d:%d:%d:%d:%s/%s</URL>", appID, instance, 0, transactionId, pl->context, slot->exten);
			pbx_str_append(&buf, 0, "</MenuItem>");
		}
		pbx_str_append(&buf, 0, "<SoftKeyItem>");
		pbx_str_append(&buf, 0, "<Name>Dial</Name>");
		pbx_str_append(&buf, 0, "<Position>1</Position>");
		pbx_str_append(&buf, 0, "<URL>UserDataSoftKey:Select:%d:DIAL/%d</URL>", appID, transactionId);
		pbx_str_append(&buf, 0, "</SoftKeyItem>\n");
		pbx_str_append(&buf, 0, "<SoftKeyItem>");
		pbx_str_append(&buf, 0, "<Name>Exit</Name>");
		pbx_str_append(&buf, 0, "<Position>3</Position>");
		pbx_str_append(&buf, 0, "<URL>UserDataSoftKey:Select:%d:EXIT/%d</URL>", appID, transactionId);
		pbx_str_append(&buf, 0, "</SoftKeyItem>\n");

		pbx_str_append(&buf, 0, "</CiscoIPPhoneMenu>");
		*outbuf = pbx_strdup(pbx_str_buffer(buf));
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (getParkingLotCXML) with version:%d, result:\n[%s]\n", pl->context, protocolversion, *outbuf);
		sccp_free(buf);
	}
	return *outbuf;
}

static void __showVisualParkingLot(sccp_parkinglot_t *pl, constDevicePtr d, plobserver_t * observer)
{
	pbx_assert(pl != NULL && d != NULL && observer != NULL);
	uint32_t transactionId = sccp_random();
	char *xmlStr;

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (showVisualParkingLot) showing on device:%s, instance:%d\n", pl->context, observer->device->id, observer->instance);
	if ((xmlStr = getParkingLotCXML(pl, d->protocolversion, observer->instance, transactionId, &xmlStr))) {
		sccp_parkinglot_unlock(pl);
		d->protocol->sendUserToDeviceDataVersionMessage(d, appID, 0, 0, transactionId, xmlStr, 0);
		sccp_free(xmlStr);
		sccp_parkinglot_lock(pl);
	} else {
		sccp_parkinglot_unlock(pl);
		sccp_dev_displayprinotify(d, SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
		sccp_parkinglot_lock(pl);
	}
	observer->transactionId = transactionId;
}

static void __hideVisualParkingLot(sccp_parkinglot_t *pl, constDevicePtr d, plobserver_t *observer)
{
	pbx_assert(pl != NULL && observer != NULL);

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (hideVisualParkingLot) device:%s, instance:%d\n", pl->context, d->id, observer->instance);
	uint32_t transactionId = observer->transactionId;
	sccp_parkinglot_unlock(pl);

	char xmlStr[DEFAULT_PBX_STR_BUFFERSIZE];
	if (d->protocolversion < 15) {
		snprintf(xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Init:Services\"/></CiscoIPPhoneExecute>");
	} else {
		snprintf(xmlStr, DEFAULT_PBX_STR_BUFFERSIZE, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"App:Close:%d\"/></CiscoIPPhoneExecute>", appID);
	}
	d->protocol->sendUserToDeviceDataVersionMessage(observer->device, appID, 0, 0, transactionId, xmlStr, 0);

	sccp_parkinglot_lock(pl);
	observer->transactionId = 0;
}

static void hideVisualParkingLot(const char *parkinglot, constDevicePtr d, uint8_t instance)
{
	pbx_assert(parkinglot != NULL &&  d != NULL);

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (hideVisualParkingLot) device:%s, instance:%d, size:%d\n", parkinglot, d->id, instance, (int)SCCP_VECTOR_SIZE(&pl->observers));
		uint8_t idx;
		for (idx = 0; idx < SCCP_VECTOR_SIZE(&pl->observers); idx++) {
			plobserver_t *observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
			if (observer->device == d && observer->instance == instance) {
				__hideVisualParkingLot(pl, d, observer);
			}
		}
	}
}

static void notifyDevice(const char *options, constDevicePtr device)
{
	pbx_assert(options != NULL && device != NULL);
	uint8_t idx = 0;
	uint32_t iconstate = 0;
	plobserver_t *observer = NULL;

	char *parse = pbx_strdupa(options);
	AST_DECLARE_APP_ARGS(args, AST_APP_ARG(parkinglot); AST_APP_ARG(flags); );
	AST_STANDARD_APP_ARGS(args, parse);

	if (!sccp_strlen_zero(args.parkinglot)) {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (notifyDevice) notifyDevice:%s\n", args.parkinglot, device->id);
		RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(args.parkinglot, TRUE), sccp_parkinglot_unlock);
		if (pl) {
			int numslots = SCCP_VECTOR_SIZE(&pl->slots);
			for (idx = 0; idx < SCCP_VECTOR_SIZE(&pl->observers); idx++) {
				observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
				if (observer && observer->device == device) {
					sccp_buttonconfig_t *config = NULL;
					if (device->protocolversion < 15) {
						sccp_device_setLamp(device, SKINNY_STIMULUS_PARKINGLOT, 0, numslots ? SKINNY_LAMP_ON : SKINNY_LAMP_OFF);
						iconstate = numslots ? ICONSTATE_OLD_ON : ICONSTATE_OLD_OFF;
					} else {
						iconstate = numslots ? ICONSTATE_NEW_ON : ICONSTATE_NEW_OFF;
					}

					// change button state
					SCCP_LIST_LOCK(&device->buttonconfig);
					SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
						if (config->type == FEATURE && config->instance == observer->instance) {
							config->button.feature.status = iconstate;
						}
					}
					SCCP_LIST_UNLOCK(&device->buttonconfig);

					// update already displayed visual parkinglot window
					if (observer->transactionId) {
						if (numslots > 0 && !device->active_channel) {
							__showVisualParkingLot(pl, device, observer);
						} else {
							__hideVisualParkingLot(pl, device, observer);
						}
					}

					sccp_feat_changed(device, NULL, SCCP_FEATURE_PARKINGLOT);
				}
			}
		}
	}
}

static void notifyLocked(sccp_parkinglot_t *pl)
{
	pbx_assert(pl != NULL);

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (notify)\n", pl->context);
	sccp_device_t *device = NULL;
	uint8_t idx = 0;
	uint32_t iconstate = 0;
	plobserver_t *observer = NULL;

	int numslots = SCCP_VECTOR_SIZE(&pl->slots);
	for (idx = 0; idx < SCCP_VECTOR_SIZE(&pl->observers); idx++) {
		observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
		if (observer) {
			device = sccp_device_retain(observer->device);
			if (device) {
				sccp_buttonconfig_t *config = NULL;

				if (device->protocolversion < 15) {
					//sccp_device_setLamp(device, SKINNY_STIMULUS_PARKINGLOT, 0, numslots ? SKINNY_LAMP_ON : SKINNY_LAMP_OFF);
					sccp_device_setLamp(device, SKINNY_STIMULUS_VOICEMAIL, 0, numslots ? SKINNY_LAMP_ON : SKINNY_LAMP_OFF);
					iconstate = numslots ? ICONSTATE_OLD_ON : ICONSTATE_OLD_OFF;
				} else {
					iconstate = numslots ? ICONSTATE_NEW_ON : ICONSTATE_NEW_OFF;
				}

				// change button state
				SCCP_LIST_LOCK(&device->buttonconfig);
				SCCP_LIST_TRAVERSE(&device->buttonconfig, config, list) {
					if (config->type == FEATURE && config->instance == observer->instance) {
						config->button.feature.status = iconstate;
					}
				}
				SCCP_LIST_UNLOCK(&device->buttonconfig);

				// update already displayed visual parkinglot window
				if (observer->transactionId) {
					if (numslots > 0 && !device->active_channel) {
						__showVisualParkingLot(pl, device, observer);
					} else {
						__hideVisualParkingLot(pl, device, observer);
					}
				}

				sccp_feat_changed(device, NULL, SCCP_FEATURE_PARKINGLOT);
				sccp_device_release(&device);
			}
		}
	}
}

// slot
static int addSlot(const char *parkinglot, int slot, struct message *m)
{
	pbx_assert(parkinglot != NULL && m != NULL);

	int res = FALSE;

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (addSlot) adding to slot:%d\n", parkinglot, slot);

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		if (SCCP_VECTOR_GET_CMP(&pl->slots, slot, SLOT_CB_CMP) == NULL) {
			plslot_t new_slot = { 
				.slot = slot,
				.exten = pbx_strdup(astman_get_header(m, PARKING_SLOT)),
				.from = pbx_strdup(astman_get_header(m, PARKING_FROM)),
				.channel = pbx_strdup(astman_get_header(m, PARKING_PREFIX "Channel")),
				.callerid_num = pbx_strdup(astman_get_header(m, PARKING_PREFIX "CallerIDNum")),
				.callerid_name = pbx_strdup(astman_get_header(m, PARKING_PREFIX "CallerIDName")),
				.connectedline_num = pbx_strdup(astman_get_header(m, PARKING_PREFIX "ConnectedLineNum")),
				.connectedline_name = pbx_strdup(astman_get_header(m, PARKING_PREFIX "ConnectedLineName")),
			};
			if (SCCP_VECTOR_APPEND(&pl->slots, new_slot) == 0)  {
				notifyLocked(pl);
				res = TRUE;
			}
		} else {
			notifyLocked(pl);
		}
	} else {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (addSlot) ParkingLot:%s is not being observed\n", parkinglot);
	}
	return res;
}

static int removeSlot(const char *parkinglot, int slot)
{
	pbx_assert(parkinglot != NULL);

	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (removeSlot) removing slot:%d\n", parkinglot, slot);
	int res = FALSE;

	RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(parkinglot, TRUE), sccp_parkinglot_unlock);
	if (pl) {
		if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(&pl->slots, slot, SLOT_CB_CMP, SLOT_CLEANUP) == 0) {
			notifyLocked(pl);
			res = TRUE;
		}
	} else {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (removeSlot) ParkingLot:%s is not being observed\n", parkinglot);
	}
	return !res;
}

/*
 * Handle Park Feature Button Press
 * -If we have an active call -> pressing the park feature key, will park that call
 * -If we are not on an active call:
 * 	- If there is 0 parked calls: Display Status Message, "No parked calls'
 * 	- If there is 1 parked call: Unpark that call immediatly
 *	- If there is more than 1 parked call: display the visual parking lot representation.
 */
static void handleButtonPress(const char *options, constDevicePtr d, uint8_t instance) 
{
	pbx_assert(options != NULL && d != NULL);
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) options:%s, instance:%d\n", d->id, options, instance);

	char *parse = pbx_strdupa(options);
	AST_DECLARE_APP_ARGS(args, AST_APP_ARG(parkinglot); AST_APP_ARG(flags); );
	AST_STANDARD_APP_ARGS(args, parse);

	AUTO_RELEASE(sccp_channel_t, channel , sccp_device_getActiveChannel(d));
	if (channel && channel->state != SCCP_CHANNELSTATE_OFFHOOK && channel->state != SCCP_CHANNELSTATE_HOLD) {
		sccp_channel_park(channel);
	} else if (!sccp_strlen_zero(args.parkinglot)){
		RAII(sccp_parkinglot_t *, pl, findCreateParkinglot(args.parkinglot, TRUE), sccp_parkinglot_unlock);
		if (pl) {
			if (SCCP_VECTOR_SIZE(&pl->slots) == 0) {
				sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) 0 slot occupied. Show statusBar message\n", args.parkinglot);
				sccp_dev_displayprinotify(d, SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
			} else {
				if (sccp_strcaseequals(args.flags, "RetrieveSingle") && SCCP_VECTOR_SIZE(&pl->slots) == 1) {
					sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) 1 slot occupied -> Unpark Call Immediately\n", args.parkinglot);
					plslot_t *slot = SCCP_VECTOR_GET_ADDR(&pl->slots, 0);
					if (slot) {
						AUTO_RELEASE(sccp_line_t, line , channel ? sccp_line_retain(channel->line) : d->currentLine ? sccp_dev_getActiveLine(d) : sccp_line_find_byid(d, d->defaultLineInstance));
						AUTO_RELEASE(sccp_channel_t, new_channel , NULL);
						new_channel = sccp_channel_newcall(line, d, slot->exten, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);		/* implicit release */
					}
				} else {
					sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (handleButtonPress) multiple slots occupied -> Show Visual ParkingLot\n", args.parkinglot);
					uint8_t idx;
					for (idx = 0; idx < SCCP_VECTOR_SIZE(&pl->observers); idx++) {
						plobserver_t *observer = SCCP_VECTOR_GET_ADDR(&pl->observers, idx);
						if (observer->device == d && observer->instance == instance) {
							__showVisualParkingLot(pl, d, observer);
						}
					}
				}
			}
		}
	}
}

static void handleDevice2User(const char *parkinglot, constDevicePtr d, const char *slot_exten, uint8_t instance, uint32_t transactionId)
{
	pbx_assert(d != NULL);
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) instance:%d, transactionId:%d\n", d->id, instance, transactionId);

	if (d->dtu_softkey.action && d->dtu_softkey.transactionID == transactionId) {
		if (sccp_strequals(d->dtu_softkey.action, "DIAL")) {
			AUTO_RELEASE(sccp_line_t, line , d->currentLine ? sccp_dev_getActiveLine(d) : sccp_line_find_byid(d, d->defaultLineInstance));
			AUTO_RELEASE(sccp_channel_t, new_channel , NULL);
			new_channel = sccp_channel_newcall(line, d, slot_exten, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);		/* implicit release */
		} else if (sccp_strequals(d->dtu_softkey.action, "EXIT")) {
			hideVisualParkingLot(parkinglot, d, instance);
		}
	}
}
/* Assign to interface */
const ParkingLotInterface iParkingLot = {
	.attachObserver = attachObserver,
	.detachObserver = detachObserver,
	.addSlot = addSlot,
	.removeSlot = removeSlot,
	.handleButtonPress = handleButtonPress,
	.handleDevice2User = handleDevice2User,
	.notifyDevice = notifyDevice,
};
#else
const ParkingLotInterface iParkingLot = { 0 };
#endif
