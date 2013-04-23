
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

/* CallInfo Message */

/* =================================================================================================================== Send Messages */

/*!
 * \brief Send CallInfoMessage (V3)
 */
static void sccp_device_sendCallinfoV3(const sccp_device_t * device, const sccp_channel_t * channel, uint8_t instance)
{
	sccp_moo_t *r;

	if (!channel) {
		return;
	}

	REQ(r, CallInfoMessage);

	if (channel->callInfo.callingPartyName) {
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, channel->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
	}
	if (channel->callInfo.callingPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, channel->callInfo.callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

	if (channel->callInfo.calledPartyName)
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, channel->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));

	if (channel->callInfo.calledPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, channel->callInfo.calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

	if (channel->callInfo.originalCalledPartyName)
		sccp_copy_string(r->msg.CallInfoMessage.originalCalledPartyName, channel->callInfo.originalCalledPartyName, sizeof(r->msg.CallInfoMessage.originalCalledPartyName));

	if (channel->callInfo.originalCalledPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.originalCalledParty, channel->callInfo.originalCalledPartyNumber, sizeof(r->msg.CallInfoMessage.originalCalledParty));

	if (channel->callInfo.lastRedirectingPartyName)
		sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingPartyName, channel->callInfo.lastRedirectingPartyName, sizeof(r->msg.CallInfoMessage.lastRedirectingPartyName));

	if (channel->callInfo.lastRedirectingPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingParty, channel->callInfo.lastRedirectingPartyNumber, sizeof(r->msg.CallInfoMessage.lastRedirectingParty));

	if (channel->callInfo.originalCdpnRedirectReason)
		r->msg.CallInfoMessage.originalCdpnRedirectReason = htolel(channel->callInfo.originalCdpnRedirectReason);

	if (channel->callInfo.lastRedirectingReason)
		r->msg.CallInfoMessage.lastRedirectingReason = htolel(channel->callInfo.lastRedirectingReason);

	if (channel->callInfo.cgpnVoiceMailbox)
		sccp_copy_string(r->msg.CallInfoMessage.cgpnVoiceMailbox, channel->callInfo.cgpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cgpnVoiceMailbox));

	if (channel->callInfo.cdpnVoiceMailbox)
		sccp_copy_string(r->msg.CallInfoMessage.cdpnVoiceMailbox, channel->callInfo.cdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cdpnVoiceMailbox));

	if (channel->callInfo.originalCdpnVoiceMailbox)
		sccp_copy_string(r->msg.CallInfoMessage.originalCdpnVoiceMailbox, channel->callInfo.originalCdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.originalCdpnVoiceMailbox));

	if (channel->callInfo.lastRedirectingVoiceMailbox)
		sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox, channel->callInfo.lastRedirectingVoiceMailbox, sizeof(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox));

	r->msg.CallInfoMessage.lel_lineId = htolel(instance);
	r->msg.CallInfoMessage.lel_callRef = htolel(channel->callid);
	r->msg.CallInfoMessage.lel_callType = htolel(channel->calltype);
	r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send callinfo for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", calltype2str(channel->calltype), channel->callid, instance, channel->callInfo.callingPartyNumber, channel->callInfo.callingPartyName);
}

/*!
 * \brief Send CallInfoMessage (V7)
 */
static void sccp_device_sendCallinfoV7(const sccp_device_t * device, const sccp_channel_t * channel, uint8_t instance)
{
	sccp_moo_t *r;

	if (!channel) {
		return;
	}

	unsigned int dataSize = 12;
	const char *data[dataSize];
	int data_len[dataSize];
	unsigned int i = 0;
	int dummy_len = 0;

	memset(data, 0, sizeof(data));

	data[0] = (strlen(channel->callInfo.callingPartyNumber) > 0) ? channel->callInfo.callingPartyNumber : NULL;
	data[1] = (strlen(channel->callInfo.calledPartyNumber) > 0) ? channel->callInfo.calledPartyNumber : NULL;
	data[2] = (strlen(channel->callInfo.originalCalledPartyNumber) > 0) ? channel->callInfo.originalCalledPartyNumber : NULL;
	data[3] = (strlen(channel->callInfo.lastRedirectingPartyNumber) > 0) ? channel->callInfo.lastRedirectingPartyNumber : NULL;
	data[4] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
	data[5] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;
	data[6] = (strlen(channel->callInfo.originalCdpnVoiceMailbox) > 0) ? channel->callInfo.originalCdpnVoiceMailbox : NULL;
	data[7] = (strlen(channel->callInfo.lastRedirectingVoiceMailbox) > 0) ? channel->callInfo.lastRedirectingVoiceMailbox : NULL;
	data[8] = (strlen(channel->callInfo.callingPartyName) > 0) ? channel->callInfo.callingPartyName : NULL;
	data[9] = (strlen(channel->callInfo.calledPartyName) > 0) ? channel->callInfo.calledPartyName : NULL;
	data[10] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
	data[11] = (strlen(channel->callInfo.lastRedirectingPartyName) > 0) ? channel->callInfo.lastRedirectingPartyName : NULL;

	for (i = 0; i < dataSize; i++) {
		if (data[i] != NULL) {
			data_len[i] = strlen(data[i]);
			dummy_len += data_len[i];
		} else {
			data_len[i] = 0;									//! \todo this should be 1?
		}
	}

	int hdr_len = sizeof(r->msg.CallInfoDynamicMessage) + (dataSize - 4);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 0;

	r = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len + padding);

	r->msg.CallInfoDynamicMessage.lel_lineId = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_callRef = htolel(channel->callid);
	r->msg.CallInfoDynamicMessage.lel_callType = htolel(channel->calltype);
	r->msg.CallInfoDynamicMessage.partyPIRestrictionBits = 0;
	r->msg.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
	r->msg.CallInfoDynamicMessage.lel_callInstance = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(channel->callInfo.originalCdpnRedirectReason);
	r->msg.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(channel->callInfo.lastRedirectingReason);

	if (dummy_len) {
		int bufferSize = dummy_len + ARRAY_LEN(data);
		char buffer[bufferSize];

		memset(&buffer[0], 0, bufferSize);
		int pos = 0;

		for (i = 0; i < ARRAY_LEN(data); i++) {
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: cid field %d, value: '%s'\n", i, (data[i]) ? data[i] : "");
			if (data[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&r->msg.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}

	sccp_dev_send(device, r);
}

/*!
 * \brief Send CallInfoMessage (V16)
 */
static void sccp_protocol_sendCallinfoV16(const sccp_device_t * device, const sccp_channel_t * channel, uint8_t instance)
{
	sccp_moo_t *r;

	if (!channel) {
		return;
	}
	unsigned int dataSize = 16;
	const char *data[dataSize];
	int data_len[dataSize];
	unsigned int i = 0;
	int dummy_len = 0;

	memset(data, 0, sizeof(data));

	data[0] = (strlen(channel->callInfo.callingPartyNumber) > 0) ? channel->callInfo.callingPartyNumber : NULL;
	data[1] = (strlen(channel->callInfo.originalCallingPartyNumber) > 0) ? channel->callInfo.originalCallingPartyNumber : NULL;
	data[2] = (strlen(channel->callInfo.calledPartyNumber) > 0) ? channel->callInfo.calledPartyNumber : NULL;
	data[3] = (strlen(channel->callInfo.originalCalledPartyNumber) > 0) ? channel->callInfo.originalCalledPartyNumber : NULL;
	data[4] = (strlen(channel->callInfo.lastRedirectingPartyNumber) > 0) ? channel->callInfo.lastRedirectingPartyNumber : NULL;
	data[5] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
	data[6] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;
	data[7] = (strlen(channel->callInfo.originalCdpnVoiceMailbox) > 0) ? channel->callInfo.originalCdpnVoiceMailbox : NULL;
	data[8] = (strlen(channel->callInfo.lastRedirectingVoiceMailbox) > 0) ? channel->callInfo.lastRedirectingVoiceMailbox : NULL;
	data[9] = (strlen(channel->callInfo.callingPartyName) > 0) ? channel->callInfo.callingPartyName : NULL;
	data[10] = (strlen(channel->callInfo.calledPartyName) > 0) ? channel->callInfo.calledPartyName : NULL;
	data[11] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
	data[12] = (strlen(channel->callInfo.lastRedirectingPartyName) > 0) ? channel->callInfo.lastRedirectingPartyName : NULL;
	data[13] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
	data[14] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
	data[15] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;

	for (i = 0; i < dataSize; i++) {
		if (data[i] != NULL) {
			data_len[i] = strlen(data[i]);
			dummy_len += data_len[i];
		} else {
			data_len[i] = 0;									//! \todo this should be 1?
		}
	}

	int hdr_len = sizeof(r->msg.CallInfoDynamicMessage) + (dataSize - 4);
	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 0;

	r = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len + padding);

	r->msg.CallInfoDynamicMessage.lel_lineId = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_callRef = htolel(channel->callid);
	r->msg.CallInfoDynamicMessage.lel_callType = htolel(channel->calltype);
	r->msg.CallInfoDynamicMessage.partyPIRestrictionBits = 0;
	r->msg.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
	r->msg.CallInfoDynamicMessage.lel_callInstance = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(channel->callInfo.originalCdpnRedirectReason);
	r->msg.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(channel->callInfo.lastRedirectingReason);

	if (dummy_len) {
		int bufferSize = dummy_len + ARRAY_LEN(data);
		char buffer[bufferSize];

		memset(&buffer[0], 0, bufferSize);
		int pos = 0;

		for (i = 0; i < ARRAY_LEN(data); i++) {
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: cid field %d, value: '%s'\n", i, (data[i]) ? data[i] : "");
			if (data[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&r->msg.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}
	sccp_dev_send(device, r);
}

/* done - callInfoMessage */

/* DialedNumber Message */

/*!
 * \brief Send DialedNumber Message (V3)
 */
static void sccp_protocol_sendDialedNumberV3(const sccp_device_t * device, const sccp_channel_t * channel)
{
	sccp_moo_t *r;
	uint8_t instance;

	REQ(r, DialedNumberMessage);

	instance = sccp_device_find_index_for_line(device, channel->line->name);
	sccp_copy_string(r->msg.DialedNumberMessage.calledParty, channel->callInfo.calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));

	r->msg.DialedNumberMessage.lel_lineId = htolel(instance);
	r->msg.DialedNumberMessage.lel_callRef = htolel(channel->callid);

	sccp_dev_send(device, r);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", device->id, channel->callInfo.calledPartyNumber, calltype2str(channel->calltype), channel->callid);
}

/*!
 * \brief Send DialedNumber Message (V19)
 *
 * \todo this message is wrong, but we get 'Placed Calls' working on V >= 19
 */
static void sccp_protocol_sendDialedNumberV19(const sccp_device_t * device, const sccp_channel_t * channel)
{
	sccp_moo_t *r;
	uint8_t instance;

	REQ(r, DialedNumberMessageV19);

	instance = sccp_device_find_index_for_line(device, channel->line->name);

	sccp_copy_string(r->msg.DialedNumberMessageV19.calledParty, channel->callInfo.calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));

	r->msg.DialedNumberMessageV19.lel_lineId = htolel(instance);
	r->msg.DialedNumberMessageV19.lel_callRef = htolel(channel->callid);

	sccp_dev_send(device, r);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", device->id, channel->callInfo.calledPartyNumber, calltype2str(channel->calltype), channel->callid);
}

/* done - DialedNumber Message */

/* Display Prompt Message */

/*!
 * \brief Send Display Prompt Message (Static)
 */
static void sccp_protocol_sendStaticDisplayprompt(const sccp_device_t * device, uint8_t lineInstance, uint8_t callid, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	REQ(r, DisplayPromptStatusMessage);
	r->msg.DisplayPromptStatusMessage.lel_messageTimeout = htolel(timeout);
	r->msg.DisplayPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.DisplayPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	sccp_copy_string(r->msg.DisplayPromptStatusMessage.promptMessage, message, sizeof(r->msg.DisplayPromptStatusMessage.promptMessage));

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", device->id, lineInstance, callid, timeout);
}

/*!
 * \brief Send Display Prompt Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayprompt(const sccp_device_t * device, uint8_t lineInstance, uint8_t callid, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	int msg_len = strlen(message);
	int hdr_len = sizeof(r->msg.DisplayDynamicPromptStatusMessage) - 3;
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 0;
	r = sccp_build_packet(DisplayDynamicPromptStatusMessage, hdr_len + msg_len + padding);
	r->msg.DisplayDynamicPromptStatusMessage.lel_messageTimeout = htolel(timeout);
	r->msg.DisplayDynamicPromptStatusMessage.lel_callReference = htolel(callid);
	r->msg.DisplayDynamicPromptStatusMessage.lel_lineInstance = htolel(lineInstance);
	memcpy(&r->msg.DisplayDynamicPromptStatusMessage.dummy, message, msg_len);

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display prompt on line %d, callid %d, timeout %d\n", device->id, lineInstance, callid, timeout);
}

/* done - display prompt */

/* Display Notify  Message */

/*!
 * \brief Send Display Notify Message (Static)
 */
static void sccp_protocol_sendStaticDisplayNotify(const sccp_device_t * device, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	REQ(r, DisplayNotifyMessage);
	r->msg.DisplayNotifyMessage.lel_displayTimeout = htolel(timeout);
	sccp_copy_string(r->msg.DisplayNotifyMessage.displayMessage, message, sizeof(r->msg.DisplayNotifyMessage.displayMessage));

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/*!
 * \brief Send Display Notify Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayNotify(const sccp_device_t * device, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	int msg_len = strlen(message);
	int hdr_len = sizeof(r->msg.DisplayDynamicNotifyMessage) - sizeof(r->msg.DisplayDynamicNotifyMessage.dummy);
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 4;

	r = sccp_build_packet(DisplayDynamicNotifyMessage, hdr_len + msg_len + padding);
	r->msg.DisplayDynamicNotifyMessage.lel_displayTimeout = htolel(timeout);
	memcpy(&r->msg.DisplayDynamicNotifyMessage.dummy, message, msg_len);

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/* done - display notify */

/* Display Priority Notify Message */

/*!
 * \brief Send Priority Display Notify Message (Static)
 */
static void sccp_protocol_sendStaticDisplayPriNotify(const sccp_device_t * device, uint8_t priority, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	REQ(r, DisplayPriNotifyMessage);
	r->msg.DisplayPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	r->msg.DisplayPriNotifyMessage.lel_priority = htolel(priority);
	sccp_copy_string(r->msg.DisplayPriNotifyMessage.displayMessage, message, sizeof(r->msg.DisplayPriNotifyMessage.displayMessage));

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/*!
 * \brief Send Priority Display Notify Message (Dynamic)
 */
static void sccp_protocol_sendDynamicDisplayPriNotify(const sccp_device_t * device, uint8_t priority, uint8_t timeout, const char *message)
{
	sccp_moo_t *r;

	int msg_len = strlen(message);
	int hdr_len = sizeof(r->msg.DisplayDynamicPriNotifyMessage) - 3;
	int padding = ((msg_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 0;
	r = sccp_build_packet(DisplayDynamicPriNotifyMessage, hdr_len + msg_len + padding);
	r->msg.DisplayDynamicPriNotifyMessage.lel_displayTimeout = htolel(timeout);
	r->msg.DisplayDynamicPriNotifyMessage.lel_priority = htolel(priority);
	memcpy(&r->msg.DisplayDynamicPriNotifyMessage.dummy, message, msg_len);

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Display notify timeout %d\n", device->id, timeout);
}

/* done - display notify */

/* callForwardStatus Message */

/*!
 * \brief Send Call Forward Status Message
 */
static void sccp_protocol_sendCallForwardStatus(const sccp_device_t * device, const void *data)
{
	sccp_moo_t *r;
	const sccp_linedevices_t *linedevice = (sccp_linedevices_t *) data;

	REQ(r, ForwardStatMessage);
	r->msg.ForwardStatMessage.lel_lineNumber = htolel(linedevice->lineInstance);
	r->msg.ForwardStatMessage.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;

	if (linedevice->cfwdAll.enabled) {
		r->msg.ForwardStatMessage.lel_cfwdallstatus = htolel(1);
		sccp_copy_string(r->msg.ForwardStatMessage.cfwdallnumber, linedevice->cfwdAll.number, sizeof(r->msg.ForwardStatMessage.cfwdallnumber));
	}
	if (linedevice->cfwdBusy.enabled) {
		r->msg.ForwardStatMessage.lel_cfwdbusystatus = htolel(1);
		sccp_copy_string(r->msg.ForwardStatMessage.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(r->msg.ForwardStatMessage.cfwdbusynumber));
	}

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Call Forward Status Message (V19)
 */
static void sccp_protocol_sendCallForwardStatusV19(const sccp_device_t * device, const void *data)
{
	sccp_moo_t *r;
	const sccp_linedevices_t *linedevice = (sccp_linedevices_t *) data;

	REQ(r, ForwardStatMessageV19);
	r->msg.ForwardStatMessageV19.lel_lineNumber = htolel(linedevice->lineInstance);
	r->msg.ForwardStatMessageV19.lel_status = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;

	if (linedevice->cfwdAll.enabled) {

		r->msg.ForwardStatMessageV19.lel_cfwdallstatus = htolel(1);
		sccp_copy_string(r->msg.ForwardStatMessageV19.cfwdallnumber, linedevice->cfwdAll.number, sizeof(r->msg.ForwardStatMessageV19.cfwdallnumber));

	}
	if (linedevice->cfwdBusy.enabled) {

		r->msg.ForwardStatMessageV19.lel_cfwdbusystatus = htolel(1);
		sccp_copy_string(r->msg.ForwardStatMessageV19.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(r->msg.ForwardStatMessageV19.cfwdbusynumber));
	}

	r->msg.ForwardStatMessageV19.lel_unknown = 0x000000FF;
	sccp_dev_send(device, r);
}

/* done - send callForwardStatus */

/* registerAck Message */

/*!
 * \brief Send Register Acknowledgement Message (V3)
 */
static void sccp_protocol_sendRegisterAckV3(const sccp_device_t * device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_moo_t *r;

	REQ(r, RegisterAckMessage);

	/* just for documentation */
#if 0
	r->msg.RegisterAckMessage.unknown1 = 0x00;
	r->msg.RegisterAckMessage.unknown2 = 0x00;
	r->msg.RegisterAckMessage.unknown3 = 0x00;
#endif
	r->msg.RegisterAckMessage.protocolVer = device->protocol->version;
	r->msg.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	r->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	memcpy(r->msg.RegisterAckMessage.dateTemplate, dateformat, sizeof(r->msg.RegisterAckMessage.dateTemplate));
	sccp_dev_send(device, r);
}

/*!
 * \brief Send Register Acknowledgement Message (V4)
 */
static void sccp_protocol_sendRegisterAckV4(const sccp_device_t * device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_moo_t *r;

	REQ(r, RegisterAckMessage);

	r->msg.RegisterAckMessage.unknown1 = 0x20;								// 0x00;
	r->msg.RegisterAckMessage.unknown2 = 0x00;								// 0x00;
	r->msg.RegisterAckMessage.unknown3 = 0xFE;								// 0xFE;

	r->msg.RegisterAckMessage.protocolVer = device->protocol->version;

	r->msg.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	r->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	memcpy(r->msg.RegisterAckMessage.dateTemplate, dateformat, sizeof(r->msg.RegisterAckMessage.dateTemplate));
	sccp_dev_send(device, r);
}

/*!
 * \brief Send Register Acknowledgement Message (V11)
 */
static void sccp_protocol_sendRegisterAckV11(const sccp_device_t * device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat)
{
	sccp_moo_t *r;

	REQ(r, RegisterAckMessage);

	r->msg.RegisterAckMessage.unknown1 = 0x20;								// 0x00;
	r->msg.RegisterAckMessage.unknown2 = 0xF1;								// 0xF1;
	r->msg.RegisterAckMessage.unknown3 = 0xFF;								// 0xFF;

	r->msg.RegisterAckMessage.protocolVer = device->protocol->version;
	r->msg.RegisterAckMessage.lel_keepAliveInterval = htolel(keepAliveInterval);
	r->msg.RegisterAckMessage.lel_secondaryKeepAliveInterval = htolel(secondaryKeepAlive);

	memcpy(r->msg.RegisterAckMessage.dateTemplate, dateformat, sizeof(r->msg.RegisterAckMessage.dateTemplate));
	sccp_dev_send(device, r);
}

/* done - registerACK */

/*!
 * \brief Send Open Receive Channel (V3)
 */
static void sccp_protocol_sendOpenReceiveChannelV3(const sccp_device_t * device, const sccp_channel_t * channel)
{
	int packetSize = 20;											/*! \todo calculate packetSize */
	sccp_moo_t *r = sccp_build_packet(OpenReceiveChannel, sizeof(r->msg.OpenReceiveChannel.v3));

	r->msg.OpenReceiveChannel.v3.lel_conferenceId = htolel(channel->callid);
	r->msg.OpenReceiveChannel.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.OpenReceiveChannel.v3.lel_millisecondPacketSize = htolel(packetSize);
	r->msg.OpenReceiveChannel.v3.lel_payloadType = htolel(channel->rtp.audio.writeFormat);
	r->msg.OpenReceiveChannel.v3.lel_vadValue = htolel(channel->line->echocancel);
	r->msg.OpenReceiveChannel.v3.lel_conferenceId1 = htolel(channel->callid);
	r->msg.OpenReceiveChannel.v3.lel_rtptimeout = htolel(10);

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Open Receive Channel (V17)
 */
static void sccp_protocol_sendOpenReceiveChannelV17(const sccp_device_t * device, const sccp_channel_t * channel)
{
	struct sockaddr_in *them;
	int packetSize = 20;											/*! \todo calculate packetSize */
	sccp_moo_t *r = sccp_build_packet(OpenReceiveChannel, sizeof(r->msg.OpenReceiveChannel.v17));

	sccp_rtp_getAudioPeer((sccp_channel_t *) channel, &them);

	r->msg.OpenReceiveChannel.v17.lel_conferenceId = htolel(channel->callid);
	r->msg.OpenReceiveChannel.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.OpenReceiveChannel.v17.lel_millisecondPacketSize = htolel(packetSize);
	r->msg.OpenReceiveChannel.v17.lel_payloadType = htolel(channel->rtp.audio.writeFormat);
	r->msg.OpenReceiveChannel.v17.lel_vadValue = htolel(channel->line->echocancel);
	r->msg.OpenReceiveChannel.v17.lel_conferenceId1 = htolel(channel->callid);
	r->msg.OpenReceiveChannel.v17.lel_rtptimeout = htolel(10);
	r->msg.OpenReceiveChannel.v17.lel_unknown17 = htolel(4000);

	//      if (channel->rtp.audio.phone_remote.sin_family = AF_INET)
	r->msg.OpenReceiveChannel.v17.lel_ipv46 = htolel(0);
	memcpy(&r->msg.OpenReceiveChannel.v17.bel_remoteIpAddr, &them->sin_addr, INET_ADDRSTRLEN);
	//      } else {
	//              r->msg.OpenReceiveChannel.v17.lel_ipv46 = htolel(1);
	//              memcpy(&r->msg.OpenReceiveChannel.v17.bel_remoteIpAddr, &them->sin_addr, INET6_ADDRSTRLEN);
	//      }

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Open MultiMediaChannel Message (V3)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV3(const sccp_device_t * device, const sccp_channel_t * channel, uint32_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_moo_t *r = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(r->msg.OpenMultiMediaChannelMessage.v3));

	r->msg.OpenMultiMediaChannelMessage.v3.lel_conferenceID = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.OpenMultiMediaChannelMessage.v3.lel_payloadCapability = htolel(skinnyFormat);
	r->msg.OpenMultiMediaChannelMessage.v3.lel_lineInstance = htolel(lineInstance);
	r->msg.OpenMultiMediaChannelMessage.v3.lel_callReference = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v3.lel_payloadType = htolel(payloadType);
	//      r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormatCount           = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormat[0].format      = htolel(4);
	//      r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.pictureFormat[0].mpi         = htolel(30);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.profile = htolel(64);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.level = htolel(50);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.macroblockspersec = htolel(40500);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.macroblocksperframe = htolel(1620);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.decpicbuf = htolel(8100);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.brandcpb = htolel(10000);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.confServiceNum = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v3.videoParameter.bitRate = htolel(bitRate);

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Open MultiMediaChannel Message (V17)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV17(const sccp_device_t * device, const sccp_channel_t * channel, uint32_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_moo_t *r = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(r->msg.OpenMultiMediaChannelMessage.v17));

	r->msg.OpenMultiMediaChannelMessage.v17.lel_conferenceID = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.OpenMultiMediaChannelMessage.v17.lel_payloadCapability = htolel(skinnyFormat);
	r->msg.OpenMultiMediaChannelMessage.v17.lel_lineInstance = htolel(lineInstance);
	r->msg.OpenMultiMediaChannelMessage.v17.lel_callReference = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v17.lel_payloadType = htolel(payloadType);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.confServiceNum = htolel(channel->callid);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.bitRate = htolel(bitRate);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormatCount       = htolel(0);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormat[0].format = htolel(4);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.pictureFormat[0].mpi = htolel(1);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.profile = htolel(64);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.level = htolel(50);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.macroblockspersec = htolel(40500);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.macroblocksperframe = htolel(1620);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.decpicbuf = htolel(8100);
	r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.brandcpb = htolel(10000);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy1                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy2                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy3                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy4                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy5                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy6                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy7                   = htolel(0);
	//      r->msg.OpenMultiMediaChannelMessage.v17.videoParameter.dummy8                   = htolel(0);

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Start MultiMedia Transmission (V3)
 */
static void sccp_protocol_sendStartMediaTransmissionV3(const sccp_device_t * device, const sccp_channel_t * channel)
{
	sccp_moo_t *r = sccp_build_packet(StartMediaTransmission, sizeof(r->msg.StartMediaTransmission.v3));
	int packetSize = 20;											/*! \todo calculate packetSize */

	r->msg.StartMediaTransmission.v3.lel_conferenceId = htolel(channel->callid);
	r->msg.StartMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.StartMediaTransmission.v3.lel_conferenceId1 = htolel(channel->callid);
	r->msg.StartMediaTransmission.v3.lel_millisecondPacketSize = htolel(packetSize);
	r->msg.StartMediaTransmission.v3.lel_payloadType = htolel(channel->rtp.audio.readFormat);
	r->msg.StartMediaTransmission.v3.lel_precedenceValue = htolel(device->audio_tos);
	r->msg.StartMediaTransmission.v3.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	r->msg.StartMediaTransmission.v3.lel_maxFramesPerPacket = htolel(0);
	r->msg.StartMediaTransmission.v3.lel_rtptimeout = htolel(10);
	r->msg.StartMediaTransmission.v3.lel_remotePortNumber = htolel(ntohs(channel->rtp.audio.phone_remote.sin_port));
	memcpy(&r->msg.StartMediaTransmission.v3.bel_remoteIpAddr, &channel->rtp.audio.phone_remote.sin_addr, 4);

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Start MultiMedia Transmission (V17)
 */
static void sccp_protocol_sendStartMediaTransmissionV17(const sccp_device_t * device, const sccp_channel_t * channel)
{
	sccp_moo_t *r = sccp_build_packet(StartMediaTransmission, sizeof(r->msg.StartMediaTransmission.v17));
	int packetSize = 20;											/*! \todo calculate packetSize */

	r->msg.StartMediaTransmission.v17.lel_conferenceId = htolel(channel->callid);
	r->msg.StartMediaTransmission.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.StartMediaTransmission.v17.lel_conferenceId1 = htolel(channel->callid);
	r->msg.StartMediaTransmission.v17.lel_millisecondPacketSize = htolel(packetSize);
	r->msg.StartMediaTransmission.v17.lel_payloadType = htolel(channel->rtp.audio.readFormat);
	r->msg.StartMediaTransmission.v17.lel_precedenceValue = htolel(device->audio_tos);
	r->msg.StartMediaTransmission.v17.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	r->msg.StartMediaTransmission.v17.lel_maxFramesPerPacket = htolel(0);
	r->msg.StartMediaTransmission.v17.lel_rtptimeout = htolel(10);
	r->msg.StartMediaTransmission.v17.lel_remotePortNumber = htolel(ntohs(channel->rtp.audio.phone_remote.sin_port));
	//      if (channel->rtp.audio.phone_remote.sin_family = AF_INET)
	r->msg.StartMediaTransmission.v17.lel_ipv46 = htolel(0);
	memcpy(&r->msg.StartMediaTransmission.v17.bel_remoteIpAddr, &channel->rtp.audio.phone_remote.sin_addr, INET_ADDRSTRLEN);
	//      } else {
	//              r->msg.StartMediaTransmission.v17.lel_ipv46 = htolel(1);
	//              memcpy(&r->msg.StartMediaTransmission.v17.bel_remoteIpAddr, &channel->rtp.audio.phone_remote.sin_addr, INET6_ADDRSTRLEN);
	//      }

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Start MultiMedia Transmission (V3)
 */
static void sccp_protocol_sendStartMultiMediaTransmissionV3(const sccp_device_t * device, const sccp_channel_t * channel, int payloadType, int bitRate, struct sockaddr_in sin)
{
	sccp_moo_t *r = sccp_build_packet(StartMultiMediaTransmission, sizeof(r->msg.StartMultiMediaTransmission.v3));

	r->msg.StartMultiMediaTransmission.v3.lel_conferenceID = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.StartMultiMediaTransmission.v3.lel_payloadCapability = htolel(channel->rtp.video.readFormat);
	r->msg.StartMultiMediaTransmission.v3.lel_callReference = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v3.lel_payloadType = htolel(payloadType);
	r->msg.StartMultiMediaTransmission.v3.lel_DSCPValue = htolel(136);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.bitRate = htolel(bitRate);
	//      r->msg.StartMultiMediaTransmission.v3.videoParameter.pictureFormatCount            = htolel(0);
	//      r->msg.StartMultiMediaTransmission.v3.videoParameter.pictureFormat[0].format       = htolel(4);
	//      r->msg.StartMultiMediaTransmission.v3.videoParameter.pictureFormat[0].mpi          = htolel(30);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.profile = htolel(0x40);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.level = htolel(0x32);				/* has to be >= 15 to work with 7985 */
	r->msg.StartMultiMediaTransmission.v3.videoParameter.macroblockspersec = htolel(40500);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.macroblocksperframe = htolel(1620);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.decpicbuf = htolel(8100);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.brandcpb = htolel(10000);
	r->msg.StartMultiMediaTransmission.v3.videoParameter.confServiceNum = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v3.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
	memcpy(&r->msg.StartMultiMediaTransmission.v3.bel_remoteIpAddr, &channel->rtp.video.phone_remote.sin_addr, 4);

	sccp_dev_send(device, r);
}

/*!
 * \brief Send Start MultiMedia Transmission (V17)
 */
static void sccp_protocol_sendStartMultiMediaTransmissionV17(const sccp_device_t * device, const sccp_channel_t * channel, int payloadType, int bitRate, struct sockaddr_in sin)
{
	sccp_moo_t *r = sccp_build_packet(StartMultiMediaTransmission, sizeof(r->msg.StartMultiMediaTransmission.v17));

	r->msg.StartMultiMediaTransmission.v17.lel_conferenceID = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.StartMultiMediaTransmission.v17.lel_payloadCapability = htolel(channel->rtp.video.readFormat);
	r->msg.StartMultiMediaTransmission.v17.lel_callReference = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v17.lel_payloadType = htolel(payloadType);
	r->msg.StartMultiMediaTransmission.v17.lel_DSCPValue = htolel(136);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.confServiceNum = htolel(channel->callid);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.bitRate = htolel(bitRate);
	//      r->msg.StartMultiMediaTransmission.v17.videoParameter.pictureFormatCount        = htolel(1);
	//      r->msg.StartMultiMediaTransmission.v17.videoParameter.pictureFormat[0].format   = htolel(4);
	//      r->msg.StartMultiMediaTransmission.v17.videoParameter.pictureFormat[0].mpi      = htolel(1);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.profile = htolel(64);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.level = htolel(50);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.macroblockspersec = htolel(40500);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.macroblocksperframe = htolel(1620);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.decpicbuf = htolel(8100);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.brandcpb = htolel(10000);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy1 = htolel(1);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy2 = htolel(2);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy3 = htolel(3);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy4 = htolel(4);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy5 = htolel(5);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy6 = htolel(6);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy7 = htolel(7);
	r->msg.StartMultiMediaTransmission.v17.videoParameter.dummy8 = htolel(8);
	//      if (channel->rtp.audio.phone_remote.sin_family = AF_INET)
	r->msg.StartMultiMediaTransmission.v17.lel_ipv46 = htolel(0);
	r->msg.StartMultiMediaTransmission.v17.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
	memcpy(&r->msg.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &channel->rtp.video.phone_remote.sin_addr, INET_ADDRSTRLEN);
	//      } else {
	//              r->msg.StartMultiMediaTransmission.v17.lel_ipv46 = htolel(1);
	//              r->msg.StartMultiMediaTransmission.v17.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
	//              memcpy(&r->msg.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &channel->rtp.video.phone_remote.sin_addr, INET6_ADDRSTRLEN);
	//      }

	sccp_dev_send(device, r);
}

/* fastPictureUpdate */
static void sccp_protocol_sendFastPictureUpdate(const sccp_device_t * device, const sccp_channel_t * channel)
{
	sccp_moo_t *r;

	REQ(r, MiscellaneousCommandMessage);

	r->msg.MiscellaneousCommandMessage.lel_conferenceId = htolel(channel->callid);
	r->msg.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.MiscellaneousCommandMessage.lel_callReference = htolel(channel->callid);
	r->msg.MiscellaneousCommandMessage.lel_miscCommandType = htolel(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE);

	sccp_dev_send(device, r);
}

/* done - fastPictureUpdate */

/* sendUserToDeviceData Message */

/*!
 * \brief Send User To Device Message (V1)
 */
static void sccp_protocol_sendUserToDeviceDataVersion1Message(const sccp_device_t * device, uint32_t appID, uint32_t lineInstance, uint32_t callReference, uint32_t transactionID, const void *xmlData, uint8_t priority)
{
	sccp_moo_t *r = NULL;

	int msg_len = strlen(xmlData);
	int hdr_len = sizeof(r->msg.UserToDeviceDataVersion1Message);
	int padding = ((msg_len + hdr_len) % 4);
	padding = (padding > 0) ? 4 - padding : 0;

	if (device->protocolversion > 17 || msg_len < StationMaxXMLMessage) {
		r = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len + padding);
		r->msg.UserToDeviceDataVersion1Message.lel_appID = htolel(appID);
		r->msg.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(lineInstance);
		r->msg.UserToDeviceDataVersion1Message.lel_callReference = htolel(callReference);
		r->msg.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
		r->msg.UserToDeviceDataVersion1Message.lel_sequenceFlag = htolel(0x0002);
		r->msg.UserToDeviceDataVersion1Message.lel_displayPriority = htolel(priority);
		r->msg.UserToDeviceDataVersion1Message.lel_dataLength = htolel(msg_len);

		if (msg_len) {
			memcpy(&r->msg.UserToDeviceDataVersion1Message.data, xmlData, msg_len);
		}
		sccp_dev_send(device, r);
		sccp_log(DEBUGCAT_HIGH)(VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message sent to device  (hdr_len: %d, msglen: %d, padding: %d, r-size: %d).\n", DEV_ID_LOG(device), hdr_len, msg_len, padding, hdr_len + msg_len + padding);
	} else {
		sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message to large to send to device  (hdr_len: %d, msglen: %d, padding: %d, r-size: %d). Skipping !\n", DEV_ID_LOG(device), hdr_len, msg_len, padding, hdr_len + msg_len + padding);
	}
}

/* done - sendUserToDeviceData */

/*! \todo need a protocol implementation for ConnectionStatisticsReq using Version 19 and higher */

/*! \todo need a protocol implementation for ForwardStatMessage using Version 19 and higher */

/* =================================================================================================================== Parse Received Messages */
/*!
 * \brief OpenReceiveChannelAck
 */
static void sccp_protocol_parseOpenReceiveChannelAckV3(const sccp_moo_t * r, uint32_t * status, struct sockaddr_in *sin, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*status = letohl(r->msg.OpenReceiveChannelAck.v3.lel_status);
	*callReference = letohl(r->msg.OpenReceiveChannelAck.v3.lel_callReference);
	*passthrupartyid = letohl(r->msg.OpenReceiveChannelAck.v3.lel_passThruPartyId);

	sin->sin_family = AF_INET;
	//      memcpy(&sin->sin_addr, &r->msg.OpenReceiveChannelAck.v3.bel_ipAddr, INET_ADDRSTRLEN);
	memcpy(&sin->sin_addr, &r->msg.OpenReceiveChannelAck.v3.bel_ipAddr, 4);
	sin->sin_port = htons(htolel(r->msg.OpenReceiveChannelAck.v3.lel_portNumber));
}

static void sccp_protocol_parseOpenReceiveChannelAckV17(const sccp_moo_t * r, uint32_t * status, struct sockaddr_in *sin, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*status = letohl(r->msg.OpenReceiveChannelAck.v17.lel_status);
	*callReference = letohl(r->msg.OpenReceiveChannelAck.v17.lel_callReference);
	*passthrupartyid = letohl(r->msg.OpenReceiveChannelAck.v17.lel_passThruPartyId);

	if (letohl(r->msg.OpenReceiveChannelAck.v17.lel_ipv46) == 0) {						// read ipv4 address
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &r->msg.OpenReceiveChannelAck.v17.bel_ipAddr, INET_ADDRSTRLEN);
		sin->sin_port = htons(htolel(r->msg.OpenReceiveChannelAck.v17.lel_portNumber));
	} else {												// read ipv6 address
		/*! \todo This is not correct, we should be returning type sockaddr_storage instead of sockaddr_in */
		sin->sin_family = AF_INET6;
		memcpy(&sin->sin_addr, &r->msg.OpenReceiveChannelAck.v17.bel_ipAddr, INET6_ADDRSTRLEN);
		sin->sin_port = htons(htolel(r->msg.OpenReceiveChannelAck.v17.lel_portNumber));
	}
}

static void sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3(const sccp_moo_t * r, uint32_t * status, struct sockaddr_in *sin, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*status = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v3.lel_status);
	*passthrupartyid = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v3.lel_passThruPartyId);
	*callReference = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v3.lel_callReference);

	sin->sin_family = AF_INET;
	memcpy(&sin->sin_addr, &r->msg.OpenMultiMediaReceiveChannelAckMessage.v3.bel_ipAddr, INET_ADDRSTRLEN);
	sin->sin_port = htons(htolel(r->msg.OpenMultiMediaReceiveChannelAckMessage.v3.lel_portNumber));
}

static void sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17(const sccp_moo_t * r, uint32_t * status, struct sockaddr_in *sin, uint32_t * passthrupartyid, uint32_t * callReference)
{
	*status = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.lel_status);
	*passthrupartyid = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.lel_passThruPartyId);
	*callReference = letohl(r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.lel_callReference);

	if (letohl(r->msg.OpenReceiveChannelAck.v17.lel_ipv46) == 0) {						// read ipv4 address
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.bel_ipAddr, INET_ADDRSTRLEN);
		sin->sin_port = htons(htolel(r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.lel_portNumber));
	} else {												// read ipv6 address
		/*! \todo This is not correct, we should be returning type sockaddr_storage instead of sockaddr_in */
		sin->sin_family = AF_INET6;
		memcpy(&sin->sin_addr, &r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.bel_ipAddr, INET6_ADDRSTRLEN);
		sin->sin_port = htons(htolel(r->msg.OpenMultiMediaReceiveChannelAckMessage.v17.lel_portNumber));
	}
}

/*!
 * \brief StartMediaTransmissionAck
 */
static void sccp_protocol_parseStartMediaTransmissionAckV3(const sccp_moo_t * r, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, uint32_t * status, struct sockaddr_in *sin)
{
	*partyID = letohl(r->msg.StartMediaTransmissionAck.v3.lel_passThruPartyId);
	*callID = letohl(r->msg.StartMediaTransmissionAck.v3.lel_callReference);
	*callID1 = letohl(r->msg.StartMediaTransmissionAck.v3.lel_callReference1);
	*status = letohl(r->msg.StartMediaTransmissionAck.v3.lel_smtStatus);

	sin->sin_family = AF_INET;
	memcpy(&sin->sin_addr, &r->msg.StartMediaTransmissionAck.v3.bel_ipAddr, INET_ADDRSTRLEN);
	sin->sin_port = htons(htolel(r->msg.StartMediaTransmissionAck.v3.lel_portNumber));
}

static void sccp_protocol_parseStartMediaTransmissionAckV17(const sccp_moo_t * r, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, uint32_t * status, struct sockaddr_in *sin)
{
	*partyID = letohl(r->msg.StartMediaTransmissionAck.v17.lel_passThruPartyId);
	*callID = letohl(r->msg.StartMediaTransmissionAck.v17.lel_callReference);
	*callID1 = letohl(r->msg.StartMediaTransmissionAck.v17.lel_callReference1);
	*status = letohl(r->msg.StartMediaTransmissionAck.v17.lel_smtStatus);

	if (letohl(r->msg.StartMediaTransmissionAck.v17.lel_ipv46) == 0) {					// read ipv4 address
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &r->msg.StartMediaTransmissionAck.v17.bel_ipAddr, INET_ADDRSTRLEN);
	} else {												// read ipv6 address
		sin->sin_family = AF_INET6;
		memcpy(&sin->sin_addr, &r->msg.StartMediaTransmissionAck.v17.bel_ipAddr, INET6_ADDRSTRLEN);
	}
	sin->sin_port = htons(htolel(r->msg.StartMediaTransmissionAck.v17.lel_portNumber));
}

/*!
 * \brief StartMultiMediaTransmissionAck
 */
static void sccp_protocol_parseStartMultiMediaTransmissionAckV3(const sccp_moo_t * r, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, uint32_t * status, struct sockaddr_in *sin)
{
	*partyID = letohl(r->msg.StartMultiMediaTransmissionAck.v3.lel_passThruPartyId);
	*callID = letohl(r->msg.StartMultiMediaTransmissionAck.v3.lel_callReference);
	*callID1 = letohl(r->msg.StartMultiMediaTransmissionAck.v3.lel_callReference1);
	*status = letohl(r->msg.StartMultiMediaTransmissionAck.v3.lel_smtStatus);

	sin->sin_family = AF_INET;
	memcpy(&sin->sin_addr, &r->msg.StartMultiMediaTransmissionAck.v3.bel_ipAddr, INET_ADDRSTRLEN);
	sin->sin_port = htons(htolel(r->msg.StartMultiMediaTransmissionAck.v3.lel_portNumber));
}

static void sccp_protocol_parseStartMultiMediaTransmissionAckV17(const sccp_moo_t * r, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, uint32_t * status, struct sockaddr_in *sin)
{
	*partyID = letohl(r->msg.StartMultiMediaTransmissionAck.v17.lel_passThruPartyId);
	*callID = letohl(r->msg.StartMultiMediaTransmissionAck.v17.lel_callReference);
	*callID1 = letohl(r->msg.StartMultiMediaTransmissionAck.v17.lel_callReference1);
	*status = letohl(r->msg.StartMultiMediaTransmissionAck.v17.lel_smtStatus);

	if (letohl(r->msg.StartMultiMediaTransmissionAck.v17.lel_ipv46) == 0) {					// read ipv4 address
		sin->sin_family = AF_INET;
		memcpy(&sin->sin_addr, &r->msg.StartMultiMediaTransmissionAck.v17.bel_ipAddr, INET_ADDRSTRLEN);
	} else {												// read ipv6 address
		sin->sin_family = AF_INET6;
		memcpy(&sin->sin_addr, &r->msg.StartMultiMediaTransmissionAck.v17.bel_ipAddr, INET6_ADDRSTRLEN);
	}
	sin->sin_port = htons(htolel(r->msg.StartMultiMediaTransmissionAck.v17.lel_portNumber));
}

/* =================================================================================================================== Map Messages to Protocol Version */

/*! 
 * \brief SCCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *sccpProtocolDefinition[] = {
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 3, sccp_device_sendCallinfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV3, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},	/* default impl */
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 5, sccp_device_sendCallinfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 9, sccp_device_sendCallinfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	&(sccp_deviceProtocol_t) {"SCCP", 10, sccp_device_sendCallinfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	&(sccp_deviceProtocol_t) {"SCCP", 11, sccp_device_sendCallinfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 15, sccp_device_sendCallinfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	&(sccp_deviceProtocol_t) {"SCCP", 16, sccp_protocol_sendCallinfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	&(sccp_deviceProtocol_t) {"SCCP", 17, sccp_protocol_sendCallinfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17},
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 19, sccp_protocol_sendCallinfoV16, sccp_protocol_sendDialedNumberV19, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV19, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17},
	&(sccp_deviceProtocol_t) {"SCCP", 20, sccp_protocol_sendCallinfoV16, sccp_protocol_sendDialedNumberV19, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV19, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17},
	NULL,
	&(sccp_deviceProtocol_t) {"SCCP", 22, sccp_protocol_sendCallinfoV16, sccp_protocol_sendDialedNumberV19, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV19, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17},
};

/*! 
 * \brief SPCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *spcpProtocolDefinition[] = {
	&(sccp_deviceProtocol_t) {"SPCP", 0, sccp_device_sendCallinfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {"SPCP", 8, sccp_device_sendCallinfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3},
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

/*!
 * \brief Get Maximum Possible Protocol Supported by Device
 */
const sccp_deviceProtocol_t *sccp_protocol_getDeviceProtocol(const sccp_device_t * device, int type)
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
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: found protocol version '%d' at %d\n", protocolDef[i]->name, protocolDef[i]->version, i);
			returnProtocol = i;
			break;
		}
	}

	return protocolDef[returnProtocol];
}
