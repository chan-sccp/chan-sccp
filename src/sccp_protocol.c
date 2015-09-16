/*!
 * \file        sccp_protocol.c
 * \brief       SCCP Protocol implementation.
 * This file does the protocol implementation only. It should not be used as a controller.
 * \author      Marcello Ceschia <marcello.ceschia [at] users.sourceforge.net>
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"
#include "sccp_protocol.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_utils.h"
#include "sccp_rtp.h"
#include "sccp_socket.h"
#include <asterisk/unaligned.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$");

/* CallInfo Message */

/* =================================================================================================================== Send Messages */

/*!
 * \brief Send CallInfoMessage (V3)
 */
static void sccp_protocol_sendOldCallInfoV3(constDevicePtr device, constChannelPtr channel, uint8_t instance)
{
	sccp_msg_t *msg = NULL;

	if (!channel) {
		return;
	}

	REQ(msg, CallInfoMessage);

	if (!sccp_strlen_zero(channel->oldCallInfo.callingPartyName)) {
		sccp_copy_string(msg->data.CallInfoMessage.callingPartyName, channel->oldCallInfo.callingPartyName, sizeof(msg->data.CallInfoMessage.callingPartyName));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.callingPartyNumber)) {
		sccp_copy_string(msg->data.CallInfoMessage.callingParty, channel->oldCallInfo.callingPartyNumber, sizeof(msg->data.CallInfoMessage.callingParty));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.calledPartyName)) {
		sccp_copy_string(msg->data.CallInfoMessage.calledPartyName, channel->oldCallInfo.calledPartyName, sizeof(msg->data.CallInfoMessage.calledPartyName));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.calledPartyNumber)) {
		sccp_copy_string(msg->data.CallInfoMessage.calledParty, channel->oldCallInfo.calledPartyNumber, sizeof(msg->data.CallInfoMessage.calledParty));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.originalCalledPartyName)) {
		sccp_copy_string(msg->data.CallInfoMessage.originalCalledPartyName, channel->oldCallInfo.originalCalledPartyName, sizeof(msg->data.CallInfoMessage.originalCalledPartyName));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.originalCalledPartyNumber)) {
		sccp_copy_string(msg->data.CallInfoMessage.originalCalledParty, channel->oldCallInfo.originalCalledPartyNumber, sizeof(msg->data.CallInfoMessage.originalCalledParty));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.lastRedirectingPartyName)) {
		sccp_copy_string(msg->data.CallInfoMessage.lastRedirectingPartyName, channel->oldCallInfo.lastRedirectingPartyName, sizeof(msg->data.CallInfoMessage.lastRedirectingPartyName));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.lastRedirectingPartyNumber)) {
		sccp_copy_string(msg->data.CallInfoMessage.lastRedirectingParty, channel->oldCallInfo.lastRedirectingPartyNumber, sizeof(msg->data.CallInfoMessage.lastRedirectingParty));
	}
	if (channel->oldCallInfo.originalCdpnRedirectReason) {
		msg->data.CallInfoMessage.originalCdpnRedirectReason = htolel(channel->oldCallInfo.originalCdpnRedirectReason);
	}
	if (channel->oldCallInfo.lastRedirectingReason) {
		msg->data.CallInfoMessage.lastRedirectingReason = htolel(channel->oldCallInfo.lastRedirectingReason);
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.cgpnVoiceMailbox)) {
		sccp_copy_string(msg->data.CallInfoMessage.cgpnVoiceMailbox, channel->oldCallInfo.cgpnVoiceMailbox, sizeof(msg->data.CallInfoMessage.cgpnVoiceMailbox));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.cdpnVoiceMailbox)) {
		sccp_copy_string(msg->data.CallInfoMessage.cdpnVoiceMailbox, channel->oldCallInfo.cdpnVoiceMailbox, sizeof(msg->data.CallInfoMessage.cdpnVoiceMailbox));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.originalCdpnVoiceMailbox)) {
		sccp_copy_string(msg->data.CallInfoMessage.originalCdpnVoiceMailbox, channel->oldCallInfo.originalCdpnVoiceMailbox, sizeof(msg->data.CallInfoMessage.originalCdpnVoiceMailbox));
	}
	if (!sccp_strlen_zero(channel->oldCallInfo.lastRedirectingVoiceMailbox)) {
		sccp_copy_string(msg->data.CallInfoMessage.lastRedirectingVoiceMailbox, channel->oldCallInfo.lastRedirectingVoiceMailbox, sizeof(msg->data.CallInfoMessage.lastRedirectingVoiceMailbox));
	}

	msg->data.CallInfoMessage.lel_lineInstance = htolel(instance);
	msg->data.CallInfoMessage.lel_callReference = htolel(channel->callid);
	msg->data.CallInfoMessage.lel_callType = htolel(channel->calltype);
	msg->data.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(channel->calltype), channel->callid, instance, channel->oldCallInfo.callingPartyNumber, channel->oldCallInfo.callingPartyName);
}

/*!
 * \brief Send CallInfoMessage (V7)
 */
static void sccp_protocol_sendOldCallInfoV7(constDevicePtr device, constChannelPtr channel, uint8_t instance)
{
	sccp_msg_t *msg = NULL;

	if (!channel) {
		return;
	}

	unsigned int dataSize = 12;
	const char *data[dataSize];
	int data_len[dataSize];
	unsigned int i = 0;
	int dummy_len = 0;

	//memset(data, 0, sizeof(data));
	memset(data, 0, dataSize);

	data[0] = (strlen(channel->oldCallInfo.callingPartyNumber) > 0) ? channel->oldCallInfo.callingPartyNumber : NULL;
	data[1] = (strlen(channel->oldCallInfo.calledPartyNumber) > 0) ? channel->oldCallInfo.calledPartyNumber : NULL;
	data[2] = (strlen(channel->oldCallInfo.originalCalledPartyNumber) > 0) ? channel->oldCallInfo.originalCalledPartyNumber : NULL;
	data[3] = (strlen(channel->oldCallInfo.lastRedirectingPartyNumber) > 0) ? channel->oldCallInfo.lastRedirectingPartyNumber : NULL;
	data[4] = (strlen(channel->oldCallInfo.cgpnVoiceMailbox) > 0) ? channel->oldCallInfo.cgpnVoiceMailbox : NULL;
	data[5] = (strlen(channel->oldCallInfo.cdpnVoiceMailbox) > 0) ? channel->oldCallInfo.cdpnVoiceMailbox : NULL;
	data[6] = (strlen(channel->oldCallInfo.originalCdpnVoiceMailbox) > 0) ? channel->oldCallInfo.originalCdpnVoiceMailbox : NULL;
	data[7] = (strlen(channel->oldCallInfo.lastRedirectingVoiceMailbox) > 0) ? channel->oldCallInfo.lastRedirectingVoiceMailbox : NULL;
	data[8] = (strlen(channel->oldCallInfo.callingPartyName) > 0) ? channel->oldCallInfo.callingPartyName : NULL;
	data[9] = (strlen(channel->oldCallInfo.calledPartyName) > 0) ? channel->oldCallInfo.calledPartyName : NULL;
	data[10] = (strlen(channel->oldCallInfo.originalCalledPartyName) > 0) ? channel->oldCallInfo.originalCalledPartyName : NULL;
	data[11] = (strlen(channel->oldCallInfo.lastRedirectingPartyName) > 0) ? channel->oldCallInfo.lastRedirectingPartyName : NULL;

	for (i = 0; i < dataSize; i++) {
		if (data[i] != NULL) {
			data_len[i] = strlen(data[i]);
			dummy_len += data_len[i];
		} else {
			data_len[i] = 0;									//! \todo this should be 1?
		}
	}

	int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) + (dataSize - 4);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len + padding);

	msg->data.CallInfoDynamicMessage.lel_lineInstance = htolel(instance);
	msg->data.CallInfoDynamicMessage.lel_callReference = htolel(channel->callid);
	msg->data.CallInfoDynamicMessage.lel_callType = htolel(channel->calltype);
	msg->data.CallInfoDynamicMessage.partyPIRestrictionBits = 0;
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
	//msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(instance);
	msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(channel->callid);
	msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(channel->oldCallInfo.originalCdpnRedirectReason);
	msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(channel->oldCallInfo.lastRedirectingReason);

	if (dummy_len) {
		// int bufferSize = dummy_len + ARRAY_LEN(data);
		int bufferSize = dummy_len + dataSize;
		char buffer[bufferSize];

		memset(&buffer[0], 0, bufferSize);
		int pos = 0;

		// for (i = 0; i < ARRAY_LEN(data); i++) {
		for (i = 0; i < dataSize; i++) {
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: cid field %d, value: '%s'\n", i, (data[i]) ? data[i] : "");
			if (data[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&msg->data.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V7) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(channel->calltype), channel->callid, instance, channel->oldCallInfo.callingPartyNumber, channel->oldCallInfo.callingPartyName);
}

/*!
 * \brief Send CallInfoMessage (V16)
 */
static void sccp_protocol_sendOldCallInfoV16(constDevicePtr device, constChannelPtr channel, uint8_t instance)
{
	sccp_msg_t *msg = NULL;

	if (!channel) {
		return;
	}
	unsigned int dataSize = 16;
	const char *data[dataSize];
	int data_len[dataSize];
	unsigned int i = 0;
	int dummy_len = 0;

	//memset(data, 0, sizeof(data));
	memset(data, 0, dataSize);

	data[0] = (strlen(channel->oldCallInfo.callingPartyNumber) > 0) ? channel->oldCallInfo.callingPartyNumber : NULL;
	data[1] = (strlen(channel->oldCallInfo.originalCallingPartyNumber) > 0) ? channel->oldCallInfo.originalCallingPartyNumber : NULL;
	data[2] = (strlen(channel->oldCallInfo.calledPartyNumber) > 0) ? channel->oldCallInfo.calledPartyNumber : NULL;
	data[3] = (strlen(channel->oldCallInfo.originalCalledPartyNumber) > 0) ? channel->oldCallInfo.originalCalledPartyNumber : NULL;
	data[4] = (strlen(channel->oldCallInfo.lastRedirectingPartyNumber) > 0) ? channel->oldCallInfo.lastRedirectingPartyNumber : NULL;
	data[5] = (strlen(channel->oldCallInfo.cgpnVoiceMailbox) > 0) ? channel->oldCallInfo.cgpnVoiceMailbox : NULL;
	data[6] = (strlen(channel->oldCallInfo.cdpnVoiceMailbox) > 0) ? channel->oldCallInfo.cdpnVoiceMailbox : NULL;
	data[7] = (strlen(channel->oldCallInfo.originalCdpnVoiceMailbox) > 0) ? channel->oldCallInfo.originalCdpnVoiceMailbox : NULL;
	data[8] = (strlen(channel->oldCallInfo.lastRedirectingVoiceMailbox) > 0) ? channel->oldCallInfo.lastRedirectingVoiceMailbox : NULL;
	data[9] = (strlen(channel->oldCallInfo.callingPartyName) > 0) ? channel->oldCallInfo.callingPartyName : NULL;
	data[10] = (strlen(channel->oldCallInfo.calledPartyName) > 0) ? channel->oldCallInfo.calledPartyName : NULL;
	data[11] = (strlen(channel->oldCallInfo.originalCalledPartyName) > 0) ? channel->oldCallInfo.originalCalledPartyName : NULL;
	data[12] = (strlen(channel->oldCallInfo.lastRedirectingPartyName) > 0) ? channel->oldCallInfo.lastRedirectingPartyName : NULL;
	data[13] = (strlen(channel->oldCallInfo.originalCalledPartyName) > 0) ? channel->oldCallInfo.originalCalledPartyName : NULL;
	data[14] = (strlen(channel->oldCallInfo.cgpnVoiceMailbox) > 0) ? channel->oldCallInfo.cgpnVoiceMailbox : NULL;
	data[15] = (strlen(channel->oldCallInfo.cdpnVoiceMailbox) > 0) ? channel->oldCallInfo.cdpnVoiceMailbox : NULL;

	for (i = 0; i < dataSize; i++) {
		if (data[i] != NULL) {
			data_len[i] = strlen(data[i]);
			dummy_len += data_len[i];
		} else {
			data_len[i] = 0;									//! \todo this should be 1?
		}
	}

	int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) + (dataSize - 4);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len + padding);

	msg->data.CallInfoDynamicMessage.lel_lineInstance = htolel(instance);
	msg->data.CallInfoDynamicMessage.lel_callReference = htolel(channel->callid);
	msg->data.CallInfoDynamicMessage.lel_callType = htolel(channel->calltype);
	msg->data.CallInfoDynamicMessage.partyPIRestrictionBits = 0;
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
	//msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(instance);
	msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(channel->callid);
	msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(channel->oldCallInfo.originalCdpnRedirectReason);
	msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(channel->oldCallInfo.lastRedirectingReason);

	if (dummy_len) {
		// int bufferSize = dummy_len + ARRAY_LEN(data);
		int bufferSize = dummy_len + dataSize;
		char buffer[bufferSize];

		memset(&buffer[0], 0, bufferSize);
		int pos = 0;

		// for (i = 0; i < ARRAY_LEN(data); i++) {
		for (i = 0; i < dataSize; i++) {
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: cid field %d, value: '%s'\n", i, (data[i]) ? data[i] : "");
			if (data[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&msg->data.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}
	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V16) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(channel->calltype), channel->callid, instance, channel->oldCallInfo.callingPartyNumber, channel->oldCallInfo.callingPartyName);
}

static void sccp_protocol_sendCallInfoV3 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, constDevicePtr device)
{
 	assert(device != NULL);
	sccp_msg_t *msg = NULL;

	REQ(msg, CallInfoMessage);

	sccp_calleridpresence_t presentation = CALLERID_PRESENCE_ALLOWED;
	sccp_callinfo_getter(ci,
		SCCP_CALLINFO_CALLEDPARTY_NAME, &msg->data.CallInfoMessage.calledPartyName,
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &msg->data.CallInfoMessage.calledParty,
		SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, &msg->data.CallInfoMessage.cdpnVoiceMailbox,
		SCCP_CALLINFO_CALLINGPARTY_NAME, &msg->data.CallInfoMessage.callingPartyName,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &msg->data.CallInfoMessage.callingParty,
		SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, &msg->data.CallInfoMessage.cgpnVoiceMailbox,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &msg->data.CallInfoMessage.originalCalledPartyName,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &msg->data.CallInfoMessage.originalCalledParty,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, &msg->data.CallInfoMessage.originalCdpnVoiceMailbox,
		//SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, 
		//SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &msg->data.CallInfoMessage.lastRedirectingPartyName,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &msg->data.CallInfoMessage.lastRedirectingParty,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, &msg->data.CallInfoMessage.lastRedirectingVoiceMailbox,
		//SCCP_CALLINFO_HUNT_PILOT_NAME,
		//SCCP_CALLINFO_HUNT_PILOT_NUMBER,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &msg->data.CallInfoMessage.originalCdpnRedirectReason,
		SCCP_CALLINFO_LAST_REDIRECT_REASON, &msg->data.CallInfoMessage.lastRedirectingReason,
		SCCP_CALLINFO_PRESENTATION, &presentation,
		SCCP_CALLINFO_KEY_SENTINEL);
	msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0x0 : 0xf;
	msg->data.CallInfoMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.CallInfoMessage.lel_callReference = htolel(callid);
	msg->data.CallInfoMessage.lel_callType = htolel(calltype);
	msg->data.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, lineInstance, msg->data.CallInfoMessage.callingParty, msg->data.CallInfoMessage.callingPartyName);
	sccp_dev_send(device, msg);
}

static void sccp_protocol_sendCallInfoV7 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, constDevicePtr device)
{
 	assert(device != NULL);
	sccp_msg_t *msg = NULL;

	sccp_calleridpresence_t presentation = CALLERID_PRESENCE_ALLOWED;
	char *data = NULL;
	int data_len = 0;

	unsigned int dataSize = sccp_callinfo_getString(ci, data, &data_len, 
					SCCP_CALLINFO_CALLINGPARTY_NUMBER,
					SCCP_CALLINFO_CALLEDPARTY_NUMBER,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER,
					SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL,
					SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL,
					SCCP_CALLINFO_CALLINGPARTY_NAME,
					SCCP_CALLINFO_CALLEDPARTY_NAME,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME,
					SCCP_CALLINFO_KEY_SENTINEL);
		
	if (dataSize > 0) {
		int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) + (dataSize - 4);
		int padding = ((data_len + hdr_len) % 4);
		padding = (padding > 0) ? 4 - padding : 4;

		msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + data_len + padding);

		msg->data.CallInfoDynamicMessage.lel_lineInstance = htolel(lineInstance);
		msg->data.CallInfoDynamicMessage.lel_callReference = htolel(callid);
		msg->data.CallInfoDynamicMessage.lel_callType = htolel(calltype);
		msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
		msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(callid);

		sccp_callinfo_getter(ci, 
			SCCP_CALLINFO_PRESENTATION, &presentation,
			SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason,
			SCCP_CALLINFO_LAST_REDIRECT_REASON, &msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason,
			SCCP_CALLINFO_KEY_SENTINEL);

		msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0x0 : 0xf;

		memcpy(&msg->data.CallInfoDynamicMessage.dummy, &data[0], data_len);

		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, lineInstance, msg->data.CallInfoMessage.callingParty, msg->data.CallInfoMessage.callingPartyName);
		sccp_dev_send(device, msg);
	}
	sccp_free(data);
}

static void sccp_protocol_sendCallInfoV16 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, constDevicePtr device)
{
 	assert(device != NULL);
	sccp_msg_t *msg = NULL;

	sccp_calleridpresence_t presentation = CALLERID_PRESENCE_ALLOWED;
	char *data = NULL;
	int data_len = 0;

	unsigned int dataSize = sccp_callinfo_getString(ci, data, &data_len, 
					SCCP_CALLINFO_CALLINGPARTY_NUMBER,
					SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER,
					SCCP_CALLINFO_CALLEDPARTY_NUMBER,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER,
					SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL,
					SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL,
					SCCP_CALLINFO_CALLINGPARTY_NAME,
					SCCP_CALLINFO_CALLEDPARTY_NAME,
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME,
					SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME,
					SCCP_CALLINFO_HUNT_PILOT_NUMBER,
					SCCP_CALLINFO_HUNT_PILOT_NAME,
					SCCP_CALLINFO_KEY_SENTINEL);

	if (dataSize > 0) {
		int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) + (dataSize - 4);
		int padding = ((data_len + hdr_len) % 4);
		padding = (padding > 0) ? 4 - padding : 4;

		msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + data_len + padding);

		msg->data.CallInfoDynamicMessage.lel_lineInstance = htolel(lineInstance);
		msg->data.CallInfoDynamicMessage.lel_callReference = htolel(callid);
		msg->data.CallInfoDynamicMessage.lel_callType = htolel(calltype);
		msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
		msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(callid);

		sccp_callinfo_getter(ci, 
			SCCP_CALLINFO_PRESENTATION, &presentation,
			SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason,
			SCCP_CALLINFO_LAST_REDIRECT_REASON, &msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason,
			SCCP_CALLINFO_KEY_SENTINEL);

		msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0x0 : 0xf;

		memcpy(&msg->data.CallInfoDynamicMessage.dummy, &data[0], data_len);

		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, lineInstance, msg->data.CallInfoMessage.callingParty, msg->data.CallInfoMessage.callingPartyName);
		sccp_dev_send(device, msg);
	}
	sccp_free(data);
}

/* done - oldCallInfoMessage */

/* DialedNumber Message */

/*!
 * \brief Send DialedNumber Message (V3)
 */
static void sccp_protocol_sendDialedNumberV3(constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const char dialedNumber[SCCP_MAX_EXTENSION])
{
	sccp_msg_t *msg = NULL;

	REQ(msg, DialedNumberMessage);

	sccp_copy_string(msg->data.DialedNumberMessage.v3.calledParty, dialedNumber, sizeof(msg->data.DialedNumberMessage.v3.calledParty));

	msg->data.DialedNumberMessage.v3.lel_lineInstance = htolel(lineInstance);
	msg->data.DialedNumberMessage.v3.lel_callReference = htolel(callid);

	sccp_dev_send(device, msg);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number:%s channel %d\n", device->id, dialedNumber, callid);
}

/*!
 * \brief Send DialedNumber Message (V18)
 *
 */
static void sccp_protocol_sendDialedNumberV18(constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const char dialedNumber[SCCP_MAX_EXTENSION])
{
	sccp_msg_t *msg = NULL;

	REQ(msg, DialedNumberMessage);

	sccp_copy_string(msg->data.DialedNumberMessage.v18.calledParty, dialedNumber, sizeof(msg->data.DialedNumberMessage.v18.calledParty));

	msg->data.DialedNumberMessage.v18.lel_lineInstance = htolel(lineInstance);
	msg->data.DialedNumberMessage.v18.lel_callReference = htolel(callid);

	sccp_dev_send(device, msg);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number:%s channel %d\n", device->id, dialedNumber, callid);
}

/* done - DialedNumber Message */

/* Display Prompt Message */

/*!
 * \brief Send Display Prompt Message (Static)
 */
static void sccp_protocol_sendStaticDisplayprompt(constDevicePtr device, uint8_t lineInstance, uint32_t callid, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, DisplayPromptStatusMessage);
	msg->data.DisplayPromptStatusMessage.lel_messageTimeout = htolel(timeout);
	msg->data.DisplayPromptStatusMessage.lel_callReference = htolel(callid);
	msg->data.DisplayPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	sccp_copy_string(msg->data.DisplayPromptStatusMessage.promptMessage, message, sizeof(msg->data.DisplayPromptStatusMessage.promptMessage));

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", device->id, lineInstance, callid, timeout);
}

/*!
 * \brief Send Display Prompt Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayprompt(constDevicePtr device, uint8_t lineInstance, uint32_t callid, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	int msg_len = strlen(message);
	int hdr_len = sizeof(msg->data.DisplayDynamicPromptStatusMessage) - 3;
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;
	msg = sccp_build_packet(DisplayDynamicPromptStatusMessage, hdr_len + msg_len + padding);
	msg->data.DisplayDynamicPromptStatusMessage.lel_messageTimeout = htolel(timeout);
	msg->data.DisplayDynamicPromptStatusMessage.lel_callReference = htolel(callid);
	msg->data.DisplayDynamicPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	memcpy(&msg->data.DisplayDynamicPromptStatusMessage.dummy, message, msg_len);

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", device->id, lineInstance, callid, timeout);
}

/* done - display prompt */

/* Display Notify  Message */

/*!
 * \brief Send Display Notify Message (Static)
 */
static void sccp_protocol_sendStaticDisplayNotify(constDevicePtr device, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, DisplayNotifyMessage);
	msg->data.DisplayNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(msg->data.DisplayNotifyMessage.displayMessage, message, sizeof(msg->data.DisplayNotifyMessage.displayMessage));

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/*!
 * \brief Send Display Notify Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayNotify(constDevicePtr device, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	int msg_len = strlen(message);
	int hdr_len = sizeof(msg->data.DisplayDynamicNotifyMessage) - sizeof(msg->data.DisplayDynamicNotifyMessage.dummy);
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	msg = sccp_build_packet(DisplayDynamicNotifyMessage, hdr_len + msg_len + padding);
	msg->data.DisplayDynamicNotifyMessage.lel_displayTimeout = htolel(timeout);
	memcpy(&msg->data.DisplayDynamicNotifyMessage.dummy, message, msg_len);

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/* done - display notify */

/* Display Priority Notify Message */

/*!
 * \brief Send Priority Display Notify Message (Static)
 */
static void sccp_protocol_sendStaticDisplayPriNotify(constDevicePtr device, uint8_t priority, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, DisplayPriNotifyMessage);
	msg->data.DisplayPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	msg->data.DisplayPriNotifyMessage.lel_priority = htolel(priority);
	sccp_copy_string(msg->data.DisplayPriNotifyMessage.displayMessage, message, sizeof(msg->data.DisplayPriNotifyMessage.displayMessage));

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/*!
 * \brief Send Priority Display Notify Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayPriNotify(constDevicePtr device, uint8_t priority, uint8_t timeout, const char *message)
{
	sccp_msg_t *msg = NULL;

	int msg_len = strlen(message);
	int hdr_len = sizeof(msg->data.DisplayDynamicPriNotifyMessage) - 3;
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;
	msg = sccp_build_packet(DisplayDynamicPriNotifyMessage, hdr_len + msg_len + padding);
	msg->data.DisplayDynamicPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	msg->data.DisplayDynamicPriNotifyMessage.lel_priority = htolel(priority);
	memcpy(&msg->data.DisplayDynamicPriNotifyMessage.dummy, message, msg_len);

	sccp_dev_send(device, msg);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/* done - display notify */

/* callForwardStatus Message */

/*!
 * \brief Send Call Forward Status Message
 */
static void sccp_protocol_sendCallForwardStatus(constDevicePtr device, const sccp_linedevices_t * linedevice)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	msg->data.ForwardStatMessage.v3.lel_lineNumber = htolel(linedevice->lineInstance);
	msg->data.ForwardStatMessage.v3.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;

	if (linedevice->cfwdAll.enabled) {
		msg->data.ForwardStatMessage.v3.lel_cfwdallstatus = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdallnumber, linedevice->cfwdAll.number, sizeof(msg->data.ForwardStatMessage.v3.cfwdallnumber));
	}
	if (linedevice->cfwdBusy.enabled) {
		msg->data.ForwardStatMessage.v3.lel_cfwdbusystatus = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
	}

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Call Forward Status Message (V19)
 */
static void sccp_protocol_sendCallForwardStatusV18(constDevicePtr device, const sccp_linedevices_t * linedevice)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	msg->data.ForwardStatMessage.v18.lel_lineNumber = htolel(linedevice->lineInstance);
	msg->data.ForwardStatMessage.v18.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;

	if (linedevice->cfwdAll.enabled) {

		msg->data.ForwardStatMessage.v18.lel_cfwdallstatus = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdallnumber, linedevice->cfwdAll.number, sizeof(msg->data.ForwardStatMessage.v18.cfwdallnumber));

	}
	if (linedevice->cfwdBusy.enabled) {

		msg->data.ForwardStatMessage.v18.lel_cfwdbusystatus = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
	}

	//msg->data.ForwardStatMessage.v18.lel_unknown = 0x000000FF;
	sccp_dev_send(device, msg);
}

/* done - send callForwardStatus */

/* registerAck Message */

/*!
 * \brief Send Register Acknowledgement Message (V3)
 */
static void sccp_protocol_sendRegisterAckV3(constDevicePtr device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, RegisterAckMessage);

	/* just for documentation */
	msg->data.RegisterAckMessage.protocolVer2 = 0x00;							// 0x00;
	msg->data.RegisterAckMessage.phoneFeatures1 = 0x00;							// 0x00;
	msg->data.RegisterAckMessage.phoneFeatures2 = 0x00;							// 0x00;

	msg->data.RegisterAckMessage.maxProtocolVer = device->protocol->version;
	msg->data.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	msg->data.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	if (!sccp_strlen_zero(dateformat)) {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, dateformat, sizeof(msg->data.RegisterAckMessage.dateTemplate));
	} else {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, "M/D/Y", sizeof(msg->data.RegisterAckMessage.dateTemplate));
	}
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Register Acknowledgement Message (V4)
 */
static void sccp_protocol_sendRegisterAckV4(constDevicePtr device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, RegisterAckMessage);

	msg->data.RegisterAckMessage.protocolVer2 = 0x20;							// 0x20;
	msg->data.RegisterAckMessage.phoneFeatures1 = 0xF1;							// 0xF1;
	msg->data.RegisterAckMessage.phoneFeatures2 = 0xFE;							// 0xFF;

	msg->data.RegisterAckMessage.maxProtocolVer = device->protocol->version;

	msg->data.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	msg->data.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	if (!sccp_strlen_zero(dateformat)) {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, dateformat, sizeof(msg->data.RegisterAckMessage.dateTemplate));
	} else {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, "M/D/Y", sizeof(msg->data.RegisterAckMessage.dateTemplate));
	}
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Register Acknowledgement Message (V11)
 */
static void sccp_protocol_sendRegisterAckV11(constDevicePtr device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, RegisterAckMessage);

	msg->data.RegisterAckMessage.protocolVer2 = 0x20;							// 0x20;
	msg->data.RegisterAckMessage.phoneFeatures1 = 0xF1;							// 0xF1;
	msg->data.RegisterAckMessage.phoneFeatures2 = 0xFF;							// 0xFF;

	msg->data.RegisterAckMessage.maxProtocolVer = device->protocol->version;
	msg->data.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	msg->data.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	if (!sccp_strlen_zero(dateformat)) {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, dateformat, sizeof(msg->data.RegisterAckMessage.dateTemplate));
	} else {
		sccp_copy_string(msg->data.RegisterAckMessage.dateTemplate, "M/D/Y", sizeof(msg->data.RegisterAckMessage.dateTemplate));
	}
	sccp_dev_send(device, msg);
}

/* done - registerACK */

/*!
 * \brief Send Open Receive Channel (V3)
 */
static void sccp_protocol_sendOpenReceiveChannelV3(constDevicePtr device, constChannelPtr channel)
{
	int packetSize = 20;											/*! \todo calculate packetSize */
	sccp_msg_t *msg = sccp_build_packet(OpenReceiveChannel, sizeof(msg->data.OpenReceiveChannel.v3));

	msg->data.OpenReceiveChannel.v3.lel_conferenceId = htolel(channel->callid);
	msg->data.OpenReceiveChannel.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.OpenReceiveChannel.v3.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.OpenReceiveChannel.v3.lel_payloadType = htolel(channel->rtp.audio.writeFormat);
	msg->data.OpenReceiveChannel.v3.lel_vadValue = htolel(channel->line->echocancel);
	msg->data.OpenReceiveChannel.v3.lel_callReference = htolel(channel->callid);
	//msg->data.OpenReceiveChannel.v3.lel_remotePortNumber = htolel(4000);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.OpenReceiveChannel.v3.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.OpenReceiveChannel.v3.lel_RFC2833Type = htolel(101);
	}
	msg->data.OpenReceiveChannel.v3.lel_dtmfType = htolel(10);;

	/* Source Ip Address */
	struct sockaddr_storage sas;

	memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
	sccp_socket_ipv4_mapped(&sas, &sas);

	struct sockaddr_in *in = (struct sockaddr_in *) &sas;

	memcpy(&msg->data.OpenReceiveChannel.v3.bel_remoteIpAddr, &in->sin_addr, 4);
	msg->data.OpenReceiveChannel.v3.lel_remotePortNumber = htolel(sccp_socket_getPort(&sas));

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Open Receive Channel (V17)
 */
static void sccp_protocol_sendOpenReceiveChannelV17(constDevicePtr device, constChannelPtr channel)
{
	int packetSize = 20;											/*! \todo calculate packetSize */
	sccp_msg_t *msg = sccp_build_packet(OpenReceiveChannel, sizeof(msg->data.OpenReceiveChannel.v17));

	msg->data.OpenReceiveChannel.v17.lel_conferenceId = htolel(channel->callid);
	msg->data.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.OpenReceiveChannel.v17.lel_payloadType = htolel(channel->rtp.audio.writeFormat);
	msg->data.OpenReceiveChannel.v17.lel_vadValue = htolel(channel->line->echocancel);
	msg->data.OpenReceiveChannel.v17.lel_callReference = htolel(channel->callid);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.OpenReceiveChannel.v17.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.OpenReceiveChannel.v17.lel_RFC2833Type = htolel(101);
	}
	msg->data.OpenReceiveChannel.v17.lel_dtmfType = htolel(10);;

	/* Source Ip Address */
	struct sockaddr_storage sas;

	//memcpy(&sas, &device->session->sin, sizeof(struct sockaddr_storage));
	memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
	sccp_socket_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v17.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.OpenReceiveChannel.v17.lel_ipv46 = htolel(1);
		msg->data.OpenReceiveChannel.v17.lel_requestedIpAddrType = htolel(1);				//for ipv6 this value have to me > 0, lel_ipv46 doesn't matter
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v17.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.OpenReceiveChannel.v17.lel_remotePortNumber = htolel(sccp_socket_getPort(&sas));
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Open Receive Channel (v22)
 */
static void sccp_protocol_sendOpenReceiveChannelv22(constDevicePtr device, constChannelPtr channel)
{
	int packetSize = 20;											/*! \todo calculate packetSize */
	sccp_msg_t *msg = sccp_build_packet(OpenReceiveChannel, sizeof(msg->data.OpenReceiveChannel.v22));

	msg->data.OpenReceiveChannel.v22.lel_conferenceId = htolel(channel->callid);
	msg->data.OpenReceiveChannel.v22.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.OpenReceiveChannel.v22.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.OpenReceiveChannel.v22.lel_payloadType = htolel(channel->rtp.audio.writeFormat);
	msg->data.OpenReceiveChannel.v22.lel_vadValue = htolel(channel->line->echocancel);
	msg->data.OpenReceiveChannel.v22.lel_callReference = htolel(channel->callid);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.OpenReceiveChannel.v22.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.OpenReceiveChannel.v22.lel_RFC2833Type = htolel(101);
	}
	msg->data.OpenReceiveChannel.v22.lel_dtmfType = htolel(10);;

	/* Source Ip Address */
	struct sockaddr_storage sas;

	//memcpy(&sas, &device->session->sin, sizeof(struct sockaddr_storage));
	memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
	sccp_socket_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v22.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.OpenReceiveChannel.v22.lel_ipv46 = htolel(1);						//for ipv6 this value have to me > 0, lel_ipv46 doesn't matter
		msg->data.OpenReceiveChannel.v22.lel_requestedIpAddrType = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v22.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.OpenReceiveChannel.v22.lel_remotePortNumber = htolel(sccp_socket_getPort(&sas));
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Open MultiMediaChannel Message (V3)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV3(constDevicePtr device, constChannelPtr channel, uint32_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(msg->data.OpenMultiMediaChannelMessage.v3));

	msg->data.OpenMultiMediaChannelMessage.v3.lel_conferenceID = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_payloadCapability = htolel(skinnyFormat);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_lineInstance = htolel(lineInstance);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_callReference = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_payloadType = htolel(payloadType);
	//msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormatCount           = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormat[0].format      = htolel(4);
	//msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormat[0].mpi         = htolel(30);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.profile = htolel(64);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.level = htolel(50);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.macroblockspersec = htolel(40500);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.macroblocksperframe = htolel(1620);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.decpicbuf = htolel(8100);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.brandcpb = htolel(10000);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.confServiceNum = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v3.videoParameter.bitRate = htolel(bitRate);

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Open MultiMediaChannel Message (V17)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV17(constDevicePtr device, constChannelPtr channel, uint32_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(msg->data.OpenMultiMediaChannelMessage.v17));

	msg->data.OpenMultiMediaChannelMessage.v17.lel_conferenceID = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_payloadCapability = htolel(skinnyFormat);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_lineInstance = htolel(lineInstance);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_callReference = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_payloadType = htolel(payloadType);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.confServiceNum = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.bitRate = htolel(bitRate);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormatCount       = htolel(0);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormat[0].format = htolel(4);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormat[0].mpi = htolel(1);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.profile = htolel(64);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.level = htolel(50);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.macroblockspersec = htolel(40500);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.macroblocksperframe = htolel(1620);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.decpicbuf = htolel(8100);
	msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.brandcpb = htolel(10000);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy1                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy2                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy3                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy4                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy5                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy6                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy7                   = htolel(0);
	//msg->data.OpenMultiMediaChannelMessage.v17.videoParameter.dummy8                   = htolel(0);

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start Media Transmission (V3)
 */
static void sccp_protocol_sendStartMediaTransmissionV3(constDevicePtr device, constChannelPtr channel)
{
	sccp_msg_t *msg = sccp_build_packet(StartMediaTransmission, sizeof(msg->data.StartMediaTransmission.v3));
	int packetSize = 20;											/*! \todo calculate packetSize */

	msg->data.StartMediaTransmission.v3.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v3.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v3.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.StartMediaTransmission.v3.lel_payloadType = htolel(channel->rtp.audio.readFormat);
	msg->data.StartMediaTransmission.v3.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v3.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v3.lel_maxFramesPerPacket = htolel(0);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.StartMediaTransmission.v3.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.StartMediaTransmission.v3.lel_RFC2833Type = htolel(101);
	}
	msg->data.StartMediaTransmission.v3.lel_dtmfType = htolel(10);;

	if (channel->rtp.audio.phone_remote.ss_family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.audio.phone_remote;

		memcpy(&msg->data.StartMediaTransmission.v3.bel_remoteIpAddr, &in->sin_addr, 4);
	} else {
		/* \todo add warning */
	}
	msg->data.StartMediaTransmission.v3.lel_remotePortNumber = htolel(sccp_socket_getPort(&channel->rtp.audio.phone_remote));

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start Media Transmission (V17)
 */
static void sccp_protocol_sendStartMediaTransmissionV17(constDevicePtr device, constChannelPtr channel)
{
	sccp_msg_t *msg = sccp_build_packet(StartMediaTransmission, sizeof(msg->data.StartMediaTransmission.v17));
	int packetSize = 20;											/*! \todo calculate packetSize */

	msg->data.StartMediaTransmission.v17.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v17.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v17.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.StartMediaTransmission.v17.lel_payloadType = htolel(channel->rtp.audio.readFormat);
	msg->data.StartMediaTransmission.v17.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v17.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v17.lel_maxFramesPerPacket = htolel(0);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.StartMediaTransmission.v17.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.StartMediaTransmission.v17.lel_RFC2833Type = htolel(101);
	}
	msg->data.StartMediaTransmission.v17.lel_dtmfType = htolel(10);;

	if (channel->rtp.audio.phone_remote.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &channel->rtp.audio.phone_remote;

		memcpy(&msg->data.StartMediaTransmission.v17.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.StartMediaTransmission.v17.lel_ipv46 = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.audio.phone_remote;

		memcpy(&msg->data.StartMediaTransmission.v17.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.StartMediaTransmission.v17.lel_remotePortNumber = htolel(sccp_socket_getPort(&channel->rtp.audio.phone_remote));
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start Media Transmission (v22)
 */
static void sccp_protocol_sendStartMediaTransmissionv22(constDevicePtr device, constChannelPtr channel)
{
	sccp_msg_t *msg = sccp_build_packet(StartMediaTransmission, sizeof(msg->data.StartMediaTransmission.v22));
	int packetSize = 20;											/*! \todo calculate packetSize */

	msg->data.StartMediaTransmission.v22.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v22.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v22.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v22.lel_millisecondPacketSize = htolel(packetSize);
	msg->data.StartMediaTransmission.v22.lel_payloadType = htolel(channel->rtp.audio.readFormat);
	msg->data.StartMediaTransmission.v22.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v22.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v22.lel_maxFramesPerPacket = htolel(0);
	if (SCCP_DTMFMODE_SKINNY == channel->dtmfmode) {
		msg->data.StartMediaTransmission.v22.lel_RFC2833Type = htolel(0);
	} else {
		msg->data.StartMediaTransmission.v22.lel_RFC2833Type = htolel(101);
	}
	msg->data.StartMediaTransmission.v22.lel_dtmfType = htolel(10);;

	if (channel->rtp.audio.phone_remote.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &channel->rtp.audio.phone_remote;

		memcpy(&msg->data.StartMediaTransmission.v22.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.StartMediaTransmission.v22.lel_ipv46 = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.audio.phone_remote;

		memcpy(&msg->data.StartMediaTransmission.v22.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.StartMediaTransmission.v22.lel_remotePortNumber = htolel(sccp_socket_getPort(&channel->rtp.audio.phone_remote));
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start MultiMedia Transmission (V3)
 */
static void sccp_protocol_sendStartMultiMediaTransmissionV3(constDevicePtr device, constChannelPtr channel, int payloadType, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(StartMultiMediaTransmission, sizeof(msg->data.StartMultiMediaTransmission.v3));

	msg->data.StartMultiMediaTransmission.v3.lel_conferenceID = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMultiMediaTransmission.v3.lel_payloadCapability = htolel(channel->rtp.video.readFormat);
	msg->data.StartMultiMediaTransmission.v3.lel_callReference = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v3.lel_payloadType = htolel(payloadType);
	msg->data.StartMultiMediaTransmission.v3.lel_DSCPValue = htolel(136);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.bitRate = htolel(bitRate);
	//msg->data.StartMultiMediaTransmission.v3.videoParameter.pictureFormatCount            = htolel(0);
	//msg->data.StartMultiMediaTransmission.v3.videoParameter.pictureFormat[0].format       = htolel(4);
	//msg->data.StartMultiMediaTransmission.v3.videoParameter.pictureFormat[0].mpi          = htolel(30);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.profile = htolel(0x40);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.level = htolel(0x32);				/* has to be >= 15 to work with 7985 */
	msg->data.StartMultiMediaTransmission.v3.videoParameter.macroblockspersec = htolel(40500);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.macroblocksperframe = htolel(1620);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.decpicbuf = htolel(8100);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.brandcpb = htolel(10000);
	msg->data.StartMultiMediaTransmission.v3.videoParameter.confServiceNum = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v3.lel_remotePortNumber = htolel(sccp_socket_getPort(&channel->rtp.video.phone_remote));
	if (channel->rtp.video.phone_remote.ss_family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.video.phone_remote;

		memcpy(&msg->data.StartMultiMediaTransmission.v3.bel_remoteIpAddr, &in->sin_addr, 4);
	} else {
		/* \todo add warning */
	}

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start MultiMedia Transmission (V17)
 */
static void sccp_protocol_sendStartMultiMediaTransmissionV17(constDevicePtr device, constChannelPtr channel, int payloadType, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(StartMultiMediaTransmission, sizeof(msg->data.StartMultiMediaTransmission.v17));

	msg->data.StartMultiMediaTransmission.v17.lel_conferenceID = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMultiMediaTransmission.v17.lel_payloadCapability = htolel(channel->rtp.video.readFormat);
	msg->data.StartMultiMediaTransmission.v17.lel_callReference = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v17.lel_payloadType = htolel(payloadType);
	msg->data.StartMultiMediaTransmission.v17.lel_DSCPValue = htolel(136);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.confServiceNum = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.bitRate = htolel(bitRate);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.pictureFormatCount        = htolel(1);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.pictureFormat[0].format   = htolel(4);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.pictureFormat[0].mpi      = htolel(1);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.profile = htolel(64);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.level = htolel(50);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.macroblockspersec = htolel(40500);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.macroblocksperframe = htolel(1620);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.decpicbuf = htolel(8100);
	msg->data.StartMultiMediaTransmission.v17.videoParameter.brandcpb = htolel(10000);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy1 = htolel(1);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy2 = htolel(2);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy3 = htolel(3);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy4 = htolel(4);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy5 = htolel(5);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy6 = htolel(6);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy7 = htolel(7);
	//msg->data.StartMultiMediaTransmission.v17.videoParameter.dummy8 = htolel(8);

	msg->data.StartMultiMediaTransmission.v17.lel_remotePortNumber = htolel(sccp_socket_getPort(&channel->rtp.video.phone_remote));
	if (channel->rtp.video.phone_remote.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &channel->rtp.video.phone_remote;

		memcpy(&msg->data.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.StartMultiMediaTransmission.v17.lel_ipv46 = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.video.phone_remote;

		memcpy(&msg->data.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	sccp_dump_msg(msg);
	sccp_dev_send(device, msg);
}

/* fastPictureUpdate */
static void sccp_protocol_sendFastPictureUpdate(constDevicePtr device, constChannelPtr channel)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, MiscellaneousCommandMessage);

	msg->data.MiscellaneousCommandMessage.lel_conferenceId = htolel(channel->callid);
	msg->data.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.MiscellaneousCommandMessage.lel_callReference = htolel(channel->callid);
	msg->data.MiscellaneousCommandMessage.lel_miscCommandType = htolel(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);

	sccp_dev_send(device, msg);
}

/* done - fastPictureUpdate */

/* sendUserToDeviceData Message */

/*!
 * \brief Send User To Device Message (V1)
 */
static void sccp_protocol_sendUserToDeviceDataVersion1Message(constDevicePtr device, uint32_t appID, uint32_t lineInstance, uint32_t callReference, uint32_t transactionID, const void *xmlData, uint8_t priority)
{
	int data_len = strlen(xmlData);
	int msg_len = 0;
	int hdr_len = 0;
	int padding = 0;

	if (device->protocolversion > 17) {
		int num_segments = data_len / StationMaxXMLMessage + 1;
		int segment = 0;
		sccp_msg_t *msg[num_segments];

		hdr_len = sizeof(msg[0]->data.UserToDeviceDataVersion1Message);
		int Sequence = 0x0000;
		int xmlDataStart = 0;

		while (data_len) {
			if (data_len > StationMaxXMLMessage) {
				msg_len = StationMaxXMLMessage;
			} else {
				msg_len = data_len;
				Sequence = 0x0002;
			}
			data_len -= msg_len;

			padding = ((msg_len + hdr_len) % 4);
			padding = (padding > 0) ? 4 - padding : 4;
			msg[segment] = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len + padding);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_appID = htolel(appID);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(lineInstance);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_callReference = htolel(callReference);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_displayPriority = htolel(priority);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_dataLength = htolel(msg_len);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_sequenceFlag = htolel(Sequence);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_conferenceID = htolel(callReference);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_appInstanceID = htolel(appID);
			msg[segment]->data.UserToDeviceDataVersion1Message.lel_routing = htolel(1);
			if (Sequence == 0x0000) {
				Sequence = 0x0001;
			}

			if (msg_len) {
				memcpy(&msg[segment]->data.UserToDeviceDataVersion1Message.data, xmlData + xmlDataStart, msg_len);
				xmlDataStart += msg_len;
			}
			sccp_dev_send(device, msg[segment]);
			usleep(10);
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message sent to device  (hdr_len: %d, msglen: %d/%d, padding: %d, msg-size: %d).\n", DEV_ID_LOG(device), hdr_len, msg_len, (int) strlen(xmlData), padding, hdr_len + msg_len + padding);
			segment++;
		}
	} else if (data_len < StationMaxXMLMessage) {
		sccp_msg_t *msg = NULL;

		hdr_len = sizeof(msg->data.UserToDeviceDataVersion1Message);
		msg_len = data_len;
		padding = ((msg_len + hdr_len) % 4);
		padding = (padding > 0) ? 4 - padding : 4;

		msg = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len + padding);
		msg->data.UserToDeviceDataVersion1Message.lel_appID = htolel(appID);
		msg->data.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(lineInstance);
		msg->data.UserToDeviceDataVersion1Message.lel_callReference = htolel(callReference);
		msg->data.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
		msg->data.UserToDeviceDataVersion1Message.lel_sequenceFlag = htolel(0x0002);
		msg->data.UserToDeviceDataVersion1Message.lel_displayPriority = htolel(priority);
		msg->data.UserToDeviceDataVersion1Message.lel_dataLength = htolel(msg_len);

		if (msg_len) {
			memcpy(&msg->data.UserToDeviceDataVersion1Message.data, xmlData, msg_len);
		}
		sccp_dev_send(device, msg);
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message sent to device  (hdr_len: %d, msglen: %d, padding: %d, msg-size: %d).\n", DEV_ID_LOG(device), hdr_len, msg_len, padding, hdr_len + msg_len + padding);
	} else {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message to large to send to device  (msg-size: %d). Skipping !\n", DEV_ID_LOG(device), data_len);
	}
}

/* done - sendUserToDeviceData */

/* sendConnectionStatisticsReq Message */
/*!
 * \brief Send Start MultiMedia Transmission (V3)
 */
static void sccp_protocol_sendConnectionStatisticsReqV3(constDevicePtr device, constChannelPtr channel, uint8_t clear)
{
	sccp_msg_t *msg = sccp_build_packet(ConnectionStatisticsReq, sizeof(msg->data.ConnectionStatisticsReq.v3));

	if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
		sccp_copy_string(msg->data.ConnectionStatisticsReq.v3.DirectoryNumber, channel->oldCallInfo.calledPartyNumber, sizeof(msg->data.ConnectionStatisticsReq.v3.DirectoryNumber));
	} else {
		sccp_copy_string(msg->data.ConnectionStatisticsReq.v3.DirectoryNumber, channel->oldCallInfo.callingPartyNumber, sizeof(msg->data.ConnectionStatisticsReq.v3.DirectoryNumber));
	}
	msg->data.ConnectionStatisticsReq.v3.lel_callReference = htolel((channel) ? channel->callid : 0);
	msg->data.ConnectionStatisticsReq.v3.lel_StatsProcessing = htolel(clear);
	msg->header.lel_protocolVer = htolel(device->protocol->version);
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start MultiMedia Transmission (V17)
 */
static void sccp_protocol_sendConnectionStatisticsReqV19(constDevicePtr device, constChannelPtr channel, uint8_t clear)
{
	sccp_msg_t *msg = sccp_build_packet(ConnectionStatisticsReq, sizeof(msg->data.ConnectionStatisticsReq.v19));

	if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
		sccp_copy_string(msg->data.ConnectionStatisticsReq.v19.DirectoryNumber, channel->oldCallInfo.calledPartyNumber, sizeof(msg->data.ConnectionStatisticsReq.v19.DirectoryNumber));
	} else {
		sccp_copy_string(msg->data.ConnectionStatisticsReq.v19.DirectoryNumber, channel->oldCallInfo.callingPartyNumber, sizeof(msg->data.ConnectionStatisticsReq.v19.DirectoryNumber));
	}
	msg->data.ConnectionStatisticsReq.v19.lel_callReference = htolel((channel) ? channel->callid : 0);
	msg->data.ConnectionStatisticsReq.v19.lel_StatsProcessing = htolel(clear);
	msg->header.lel_protocolVer = htolel(device->protocol->version);
	sccp_dev_send(device, msg);
}

/* done - sendUserToDeviceData */

/*! \todo need a protocol implementation for ConnectionStatisticsReq using Version 19 and higher */

/*! \todo need a protocol implementation for ForwardStatMessage using Version 19 and higher */

/* =================================================================================================================== Parse Received Messages */

/*!
 * \brief OpenReceiveChannelAck
 */
static void sccp_protocol_parseOpenReceiveChannelAckV3(constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*mediastatus = letohl(msg->data.OpenReceiveChannelAck.v3.lel_mediastatus);
	*callReference = letohl(msg->data.OpenReceiveChannelAck.v3.lel_callReference);
	*passthrupartyid = letohl(msg->data.OpenReceiveChannelAck.v3.lel_passThruPartyId);

	ss->ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) ss;

	memcpy(&sin->sin_addr, &msg->data.OpenReceiveChannelAck.v3.bel_ipAddr, sizeof(sin->sin_addr));
	sin->sin_port = htons(htolel(msg->data.OpenReceiveChannelAck.v3.lel_portNumber));
}

static void sccp_protocol_parseOpenReceiveChannelAckV17(constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*mediastatus = letohl(msg->data.OpenReceiveChannelAck.v17.lel_mediastatus);
	*callReference = letohl(msg->data.OpenReceiveChannelAck.v17.lel_callReference);
	*passthrupartyid = letohl(msg->data.OpenReceiveChannelAck.v17.lel_passThruPartyId);

	if (letohl(msg->data.OpenReceiveChannelAck.v17.lel_ipv46) == 0) {					// read ipv4 address
		ss->ss_family = AF_INET;
		struct sockaddr_in *sin = (struct sockaddr_in *) ss;

		memcpy(&sin->sin_addr, &msg->data.OpenReceiveChannelAck.v17.bel_ipAddr, sizeof(sin->sin_addr));
		sin->sin_port = htons(htolel(msg->data.OpenReceiveChannelAck.v17.lel_portNumber));
	} else {												// read ipv6 address
		/* what to do with IPv4-mapped IPv6 addresses */
		ss->ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;

		memcpy(&sin6->sin6_addr, &msg->data.OpenReceiveChannelAck.v17.bel_ipAddr, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(htolel(msg->data.OpenReceiveChannelAck.v17.lel_portNumber));
	}
	//sccp_dump_msg((sccp_msg_t *)msg);
}

static void sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3(constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*mediastatus = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v3.lel_mediastatus);
	*passthrupartyid = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v3.lel_passThruPartyId);
	*callReference = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v3.lel_callReference);

	ss->ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) ss;

	memcpy(&sin->sin_addr, &msg->data.OpenMultiMediaReceiveChannelAckMessage.v3.bel_ipAddr, sizeof(sin->sin_addr));
	sin->sin_port = htons(htolel(msg->data.OpenMultiMediaReceiveChannelAckMessage.v3.lel_portNumber));
}

static void sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17(constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*mediastatus = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_mediastatus);
	*passthrupartyid = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_passThruPartyId);
	*callReference = letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_callReference);

	if (letohl(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_ipv46) == 0) {			// read ipv4 address
		ss->ss_family = AF_INET;
		struct sockaddr_in *sin = (struct sockaddr_in *) ss;

		memcpy(&sin->sin_addr, &msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.bel_ipAddr, sizeof(sin->sin_addr));
		sin->sin_port = htons(htolel(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_portNumber));
	} else {												// read ipv6 address
		/* what to do with IPv4-mapped IPv6 addresses */
		ss->ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;

		memcpy(&sin6->sin6_addr, &msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.bel_ipAddr, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(htolel(msg->data.OpenMultiMediaReceiveChannelAckMessage.v17.lel_portNumber));
	}
}

/*!
 * \brief StartMediaTransmissionAck
 */
static void sccp_protocol_parseStartMediaTransmissionAckV3(constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss)
{
	*partyID = letohl(msg->data.StartMediaTransmissionAck.v3.lel_passThruPartyId);
	*callID = letohl(msg->data.StartMediaTransmissionAck.v3.lel_callReference);
	*callID1 = letohl(msg->data.StartMediaTransmissionAck.v3.lel_callReference1);
	*mediastatus = letohl(msg->data.StartMediaTransmissionAck.v3.lel_mediastatus);

	ss->ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) ss;

	memcpy(&sin->sin_addr, &msg->data.StartMediaTransmissionAck.v3.bel_ipAddr, sizeof(sin->sin_addr));
	sin->sin_port = htons(htolel(msg->data.StartMediaTransmissionAck.v3.lel_portNumber));
}

static void sccp_protocol_parseStartMediaTransmissionAckV17(constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss)
{
	*partyID = letohl(msg->data.StartMediaTransmissionAck.v17.lel_passThruPartyId);
	*callID = letohl(msg->data.StartMediaTransmissionAck.v17.lel_callReference);
	*callID1 = letohl(msg->data.StartMediaTransmissionAck.v17.lel_callReference1);
	*mediastatus = letohl(msg->data.StartMediaTransmissionAck.v17.lel_mediastatus);

	if (letohl(msg->data.StartMediaTransmissionAck.v17.lel_ipv46) == 0) {					// read ipv4 address
		ss->ss_family = AF_INET;
		struct sockaddr_in *sin = (struct sockaddr_in *) ss;

		memcpy(&sin->sin_addr, &msg->data.StartMediaTransmissionAck.v17.bel_ipAddr, sizeof(sin->sin_addr));
		sin->sin_port = htons(htolel(msg->data.StartMediaTransmissionAck.v17.lel_portNumber));
	} else {												// read ipv6 address
		/* what to do with IPv4-mapped IPv6 addresses */
		ss->ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;

		memcpy(&sin6->sin6_addr, &msg->data.StartMediaTransmissionAck.v17.bel_ipAddr, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(htolel(msg->data.StartMediaTransmissionAck.v17.lel_portNumber));
	}
}

/*!
 * \brief StartMultiMediaTransmissionAck
 */
static void sccp_protocol_parseStartMultiMediaTransmissionAckV3(constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss)
{
	*partyID = letohl(msg->data.StartMultiMediaTransmissionAck.v3.lel_passThruPartyId);
	*callID = letohl(msg->data.StartMultiMediaTransmissionAck.v3.lel_callReference);
	*callID1 = letohl(msg->data.StartMultiMediaTransmissionAck.v3.lel_callReference1);
	*mediastatus = letohl(msg->data.StartMultiMediaTransmissionAck.v3.lel_mediastatus);

	ss->ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) ss;

	memcpy(&sin->sin_addr, &msg->data.StartMultiMediaTransmissionAck.v3.bel_ipAddr, sizeof(sin->sin_addr));
	sin->sin_port = htons(htolel(msg->data.StartMultiMediaTransmissionAck.v3.lel_portNumber));
}

static void sccp_protocol_parseStartMultiMediaTransmissionAckV17(constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage *ss)
{
	*partyID = letohl(msg->data.StartMultiMediaTransmissionAck.v17.lel_passThruPartyId);
	*callID = letohl(msg->data.StartMultiMediaTransmissionAck.v17.lel_callReference);
	*callID1 = letohl(msg->data.StartMultiMediaTransmissionAck.v17.lel_callReference1);
	*mediastatus = letohl(msg->data.StartMultiMediaTransmissionAck.v17.lel_mediastatus);

	if (letohl(msg->data.StartMultiMediaTransmissionAck.v17.lel_ipv46) == 0) {				// read ipv4 address
		ss->ss_family = AF_INET;
		struct sockaddr_in *sin = (struct sockaddr_in *) ss;

		memcpy(&sin->sin_addr, &msg->data.StartMultiMediaTransmissionAck.v17.bel_ipAddr, sizeof(struct in_addr));
		sin->sin_port = htons(htolel(msg->data.StartMultiMediaTransmissionAck.v17.lel_portNumber));
	} else {												// read ipv6 address
		/* what to do with IPv4-mapped IPv6 addresses */
		ss->ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;

		memcpy(&sin6->sin6_addr, &msg->data.StartMultiMediaTransmissionAck.v17.bel_ipAddr, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(htolel(msg->data.StartMultiMediaTransmissionAck.v17.lel_portNumber));
	}
}

/*!
 * \brief EnblocCallMessage
 */
static void sccp_protocol_parseEnblocCallV3(constMessagePtr msg, char *calledParty, uint32_t * lineInstance)
{
	sccp_copy_string(calledParty, msg->data.EnblocCallMessage.v3.calledParty, StationMaxDirnumSize);
	*lineInstance = 0;											// v3 - v16 don't provicde lineInstance during enbloc dialing
}

static void sccp_protocol_parseEnblocCallV17(constMessagePtr msg, char *calledParty, uint32_t * lineInstance)
{
	sccp_copy_string(calledParty, msg->data.EnblocCallMessage.v17.calledParty, StationMaxDirnumSize);
	*lineInstance = letohl(msg->data.EnblocCallMessage.v17.lel_lineInstance);
}

static void sccp_protocol_parseEnblocCallV22(constMessagePtr msg, char *calledParty, uint32_t * lineInstance)
{
	sccp_copy_string(calledParty, msg->data.EnblocCallMessage.v18.calledParty, 25);
#if defined(HAVE_UNALIGNED_BUSERROR)
	*lineInstance = letohl(get_unaligned_uint32((const void *) &msg->data.EnblocCallMessage.v18.lel_lineInstance));
#else
	*lineInstance = letohl(msg->data.EnblocCallMessage.v18.lel_lineInstance);
#endif
}

/* =================================================================================================================== Map Messages to Protocol Version */

/*! 
 * \brief SCCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *sccpProtocolDefinition[] = {
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 3, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV3, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV3, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},	/* default impl */
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 5, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV3, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 9, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV7, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 10, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV7, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 11, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV7, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 15, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV7, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 16, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 17, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 18, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 19, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 20, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 21, TimeDateReqMessage, sccp_protocol_sendOldCallInfoV16, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 22,
				  TimeDateReqMessage,
				  sccp_protocol_sendOldCallInfoV16,
				  sccp_protocol_sendCallInfoV16,
				  sccp_protocol_sendDialedNumberV18,
				  sccp_protocol_sendRegisterAckV11,
				  sccp_protocol_sendDynamicDisplayprompt,
				  sccp_protocol_sendDynamicDisplayNotify,
				  sccp_protocol_sendDynamicDisplayPriNotify,
				  sccp_protocol_sendCallForwardStatusV18,
				  sccp_protocol_sendUserToDeviceDataVersion1Message,
				  sccp_protocol_sendFastPictureUpdate,
				  sccp_protocol_sendOpenReceiveChannelv22,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17,
				  sccp_protocol_sendStartMediaTransmissionv22,
				  sccp_protocol_sendConnectionStatisticsReqV19,
				  sccp_protocol_parseOpenReceiveChannelAckV17,
				  sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17,
				  sccp_protocol_parseStartMediaTransmissionAckV17,
				  sccp_protocol_parseStartMultiMediaTransmissionAckV17,
				  sccp_protocol_parseEnblocCallV22},
};

/*! 
 * \brief SPCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *spcpProtocolDefinition[] = {
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 0, RegisterAvailableLinesMessage, sccp_protocol_sendOldCallInfoV3, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3},
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 8, RegisterAvailableLinesMessage, sccp_protocol_sendOldCallInfoV3, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV19,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV17},
};

/*! 
 * \brief Get Maximum Supported Version Number by Protocol Type
 */
uint8_t sccp_protocol_getMaxSupportedVersionNumber(int type)
{
	switch (type) {
		case SCCP_PROTOCOL:
			return ARRAY_LEN(sccpProtocolDefinition) - 1;
		case SPCP_PROTOCOL:
			return ARRAY_LEN(spcpProtocolDefinition) - 1;
		default:
			return 0;
	}
}

gcc_inline boolean_t sccp_protocol_isProtocolSupported(uint8_t type, uint8_t version)
{
	const sccp_deviceProtocol_t **protocolDef = NULL;
	size_t protocolArraySize = 0 ;

	switch (type) {
		case SCCP_PROTOCOL:
			protocolArraySize = ARRAY_LEN(sccpProtocolDefinition);
			protocolDef = sccpProtocolDefinition;
			break;
		case SPCP_PROTOCOL:
			protocolArraySize = ARRAY_LEN(spcpProtocolDefinition);
			protocolDef = spcpProtocolDefinition;
			break;
		default:
			pbx_log(LOG_WARNING, "SCCP: Unknown Protocol\n");
	}

	return (version < protocolArraySize && protocolDef[version] != NULL) ? TRUE : FALSE;
}

/*!
 * \brief Get Maximum Possible Protocol Supported by Device
 */
const sccp_deviceProtocol_t *sccp_protocol_getDeviceProtocol(constDevicePtr device, int type)
{

	uint8_t i;
	uint8_t version = device->protocolversion;
	const sccp_deviceProtocol_t **protocolDef;
	size_t protocolArraySize;
	uint8_t returnProtocol;

	sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "SCCP: searching for our capability for device protocol version %d\n", version);

	if (type == SCCP_PROTOCOL) {
		protocolArraySize = ARRAY_LEN(sccpProtocolDefinition);
		protocolDef = sccpProtocolDefinition;
		returnProtocol = 3;										// setting minimally returned protocol
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "SCCP: searching for our capability for device protocol SCCP\n");
	} else {
		protocolArraySize = ARRAY_LEN(spcpProtocolDefinition);
		protocolDef = spcpProtocolDefinition;
		returnProtocol = 0;
		sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "SCCP: searching for our capability for device protocol SPCP\n");
	}

	for (i = (protocolArraySize - 1); i > 0; i--) {

		if (protocolDef[i] != NULL && version >= protocolDef[i]->version) {
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: found protocol version '%d' at %d\n", protocolDef[i]->type == SCCP_PROTOCOL ? "SCCP" : "SPCP", protocolDef[i]->version, i);
			returnProtocol = i;
			break;
		}
	}

	return protocolDef[returnProtocol];
}

const char *skinny_keymode2longstr(skinny_keymode_t keymode)
{
	switch (keymode) {
		case KEYMODE_ONHOOK:
			return "On Hook";
		case KEYMODE_CONNECTED:
			return "Connected";
		case KEYMODE_ONHOLD:
			return "On Hold";
		case KEYMODE_RINGIN:
			return "Incoming Call Ringing";
		case KEYMODE_OFFHOOK:
			return "Off Hook";
		case KEYMODE_CONNTRANS:
			return "Connect with Transferable Call";
		case KEYMODE_DIGITSFOLL:
			return "More Digits Following";
		case KEYMODE_CONNCONF:
			return "Connected to Conference";
		case KEYMODE_RINGOUT:
			return "Outgoing Call Ringing";
		case KEYMODE_OFFHOOKFEAT:
			return "Off Hook with Features";
		case KEYMODE_INUSEHINT:
			return "Hint is in use";
		case KEYMODE_ONHOOKSTEALABLE:
			return "On Hook with Stealable Call Present";
		default:
			return "Unknown KeyMode";
	}
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
