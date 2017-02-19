/*!
 * \file        sccp_protocol.c
 * \brief       SCCP Protocol implementation.
 * This file does the protocol implementation only. It should not be used as a controller.
 * \author      Marcello Ceschia <marcello.ceschia [at] users.sourceforge.net>
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_enum.h"
#include "sccp_line.h"
#include "sccp_protocol.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include <asterisk/unaligned.h>

SCCP_FILE_VERSION(__FILE__, "");

/* CallInfo Message */

/* =================================================================================================================== Send Messages */
static void sccp_protocol_sendCallInfoV3 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, constDevicePtr device)
{
 	pbx_assert(device != NULL);
	sccp_msg_t *msg;

	REQ(msg, CallInfoMessage);

	int originalCdpnRedirectReason = 0;
	int lastRedirectingReason = 0;
	sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;

	iCallInfo.Getter(ci,
		SCCP_CALLINFO_CALLEDPARTY_NAME, &msg->data.CallInfoMessage.calledPartyName,
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &msg->data.CallInfoMessage.calledParty,
		SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, &msg->data.CallInfoMessage.cdpnVoiceMailbox,
		SCCP_CALLINFO_CALLINGPARTY_NAME, &msg->data.CallInfoMessage.callingPartyName,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &msg->data.CallInfoMessage.callingParty,
		SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, &msg->data.CallInfoMessage.cgpnVoiceMailbox,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &msg->data.CallInfoMessage.originalCalledPartyName,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &msg->data.CallInfoMessage.originalCalledParty,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, &msg->data.CallInfoMessage.originalCdpnVoiceMailbox,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &msg->data.CallInfoMessage.lastRedirectingPartyName,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &msg->data.CallInfoMessage.lastRedirectingParty,
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, &msg->data.CallInfoMessage.lastRedirectingVoiceMailbox,
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &originalCdpnRedirectReason,
		SCCP_CALLINFO_LAST_REDIRECT_REASON, &lastRedirectingReason,
		SCCP_CALLINFO_PRESENTATION, &presentation,
		SCCP_CALLINFO_KEY_SENTINEL);

	msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0xf : 0x0;
	msg->data.CallInfoMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.CallInfoMessage.lel_callReference = htolel(callid);
	msg->data.CallInfoMessage.lel_callType = htolel(calltype);
	msg->data.CallInfoMessage.lel_callInstance = htolel(callInstance);
	msg->data.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	msg->data.CallInfoMessage.lel_originalCdpnRedirectReason = htolel(originalCdpnRedirectReason);
	msg->data.CallInfoMessage.lel_lastRedirectingReason = htolel(lastRedirectingReason);

	//sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d/%d on line instance %d\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, callInstance, lineInstance);
	//if ((GLOB(debug) & (DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) != 0) {
	//	iCallInfo.Print2log(ci, "SCCP: (sendCallInfoV3)");
	//}
	sccp_dev_send(device, msg);
}

static void sccp_protocol_sendCallInfoV7 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, constDevicePtr device)
{
 	pbx_assert(device != NULL);
	sccp_msg_t *msg = NULL;

	unsigned int dataSize = 12;
	char data[dataSize][StationMaxNameSize];
	int data_len[dataSize];
	unsigned int i = 0;
	int dummy_len = 0;

	memset(data, 0, dataSize * StationMaxNameSize);
	
	int originalCdpnRedirectReason = 0;
	int lastRedirectingReason = 0;
	sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;
	iCallInfo.Getter(ci,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &data[0],
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &data[1],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &data[2],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &data[3],
		SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, &data[4],
		SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, &data[5],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, &data[6],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, &data[7],
		SCCP_CALLINFO_CALLINGPARTY_NAME, &data[8],
		SCCP_CALLINFO_CALLEDPARTY_NAME, &data[9],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &data[10],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &data[11],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &originalCdpnRedirectReason,
		SCCP_CALLINFO_LAST_REDIRECT_REASON, &lastRedirectingReason,
		SCCP_CALLINFO_PRESENTATION, &presentation,
		SCCP_CALLINFO_KEY_SENTINEL);


	for (i = 0; i < dataSize; i++) {
		data_len[i] = strlen(data[i]);
		dummy_len += data_len[i];
	}

	int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) + (dataSize - 3);
	msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len);

	msg->data.CallInfoDynamicMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.CallInfoDynamicMessage.lel_callReference = htolel(callid);
	msg->data.CallInfoDynamicMessage.lel_callType = htolel(calltype);
	msg->data.CallInfoDynamicMessage.partyPIRestrictionBits = presentation ? 0x0 : 0xf;
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	msg->data.CallInfoDynamicMessage.lel_callInstance = htolel(callInstance);
	msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(originalCdpnRedirectReason);
	msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(lastRedirectingReason);

	if (dummy_len) {
		int bufferSize = dummy_len + dataSize;
		char buffer[bufferSize];
		int pos = 0;

		memset(&buffer[0], 0, bufferSize);
		for (i = 0; i < dataSize; i++) {
			if (data_len[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&msg->data.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}

	//sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V7) for %s channel %d/%d on line instance %d\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, callInstance, lineInstance);
	//if ((GLOB(debug) & (DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) != 0) {
	//	iCallInfo.Print2log(ci, "SCCP: (sendCallInfoV7)");
	//}
	sccp_dev_send(device, msg);
}

static void sccp_protocol_sendCallInfoV16 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, constDevicePtr device)
{
 	pbx_assert(device != NULL);
	sccp_msg_t *msg = NULL;

	unsigned int dataSize = 16;
	char data[dataSize][StationMaxNameSize];
	memset(data, 0, dataSize * StationMaxNameSize);
	
	int originalCdpnRedirectReason = 0;
	int lastRedirectingReason = 0;
	sccp_callerid_presentation_t presentation = CALLERID_PRESENTATION_ALLOWED;
	iCallInfo.Getter(ci,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &data[0],
		SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, &data[1],
		SCCP_CALLINFO_CALLEDPARTY_NUMBER, &data[2],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, &data[3],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, &data[4],
		SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, &data[5],
		SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, &data[6],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, &data[7],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, &data[8],
		SCCP_CALLINFO_CALLINGPARTY_NAME, &data[9],
		SCCP_CALLINFO_CALLEDPARTY_NAME, &data[10],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, &data[11],
		SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, &data[12],
		SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, &data[13],
		SCCP_CALLINFO_HUNT_PILOT_NUMBER, &data[14],
		SCCP_CALLINFO_HUNT_PILOT_NAME, &data[15],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &originalCdpnRedirectReason,
		SCCP_CALLINFO_LAST_REDIRECT_REASON, &lastRedirectingReason,
		SCCP_CALLINFO_PRESENTATION, &presentation,
		SCCP_CALLINFO_KEY_SENTINEL);

	unsigned int field = 0;
	int data_len = 0;
	int dummy_len = 0;
	uint8_t *dummy = sccp_calloc(sizeof(uint8_t), dataSize * StationMaxNameSize);
	if (!dummy) {
		return;
	}
	for (field = 0; field < dataSize; field++) {
		data_len = strlen(data[field]) + 1 /* add NULL terminator */;
		memcpy(dummy + dummy_len, data[field], data_len);
		dummy_len += data_len;
	}
	int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) - 4;
	msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len);
	msg->data.CallInfoDynamicMessage.lel_lineInstance		= htolel(lineInstance);
	msg->data.CallInfoDynamicMessage.lel_callReference		= htolel(callid);
	msg->data.CallInfoDynamicMessage.lel_callType			= htolel(calltype);
	msg->data.CallInfoDynamicMessage.partyPIRestrictionBits		= presentation ? 0x0 : 0xf;
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus		= htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	msg->data.CallInfoDynamicMessage.lel_callInstance		= htolel(callInstance);
	msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason	= htolel(originalCdpnRedirectReason);
	msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason	= htolel(lastRedirectingReason);
	memcpy(&msg->data.CallInfoDynamicMessage.dummy, dummy, dummy_len);
	sccp_free(dummy);
	
	//sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V20) for %s channel %d/%d on line instance %d\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, callInstance, lineInstance);
	//if ((GLOB(debug) & (DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) != 0) {
	//	iCallInfo.Print2log(ci, "SCCP: (sendCallInfoV16)");
	//}
	sccp_dev_send(device, msg);
}
/* done - CallInfoMessage */


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
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number:%s, callid:%d, lineInstance:%d\n", device->id, dialedNumber, callid, lineInstance);
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
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number:%s, callid:%d, lineInstance:%d\n", device->id, dialedNumber, callid, lineInstance);
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
	msg = sccp_build_packet(DisplayDynamicPromptStatusMessage, hdr_len + msg_len);
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

	int hdr_len = sizeof(msg->data.DisplayDynamicNotifyMessage) - 3;
	msg = sccp_build_packet(DisplayDynamicNotifyMessage, hdr_len + msg_len);
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
	msg = sccp_build_packet(DisplayDynamicPriNotifyMessage, hdr_len + msg_len);
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
 * \todo need more information about lel_activeForward and lel_forwardAllActive values.
 */
static void sccp_protocol_sendCallForwardStatus(constDevicePtr device, const sccp_linedevices_t * linedevice)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	msg->data.ForwardStatMessage.v3.lel_activeForward = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(1) : 0;
	msg->data.ForwardStatMessage.v3.lel_lineNumber = htolel(linedevice->lineInstance);

	if (linedevice->cfwdAll.enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardAllActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdallnumber, linedevice->cfwdAll.number, sizeof(msg->data.ForwardStatMessage.v3.cfwdallnumber));
	}
	if (linedevice->cfwdBusy.enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardBusyActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
	}
	/*
	if (linedevice->cfwdNoAnswer.enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardBusyActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
	}
	*/

	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Call Forward Status Message (V19)
 * \todo need more information about lel_activeForward and lel_forwardAllActive values.
 */
static void sccp_protocol_sendCallForwardStatusV18(constDevicePtr device, const sccp_linedevices_t * linedevice)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	msg->data.ForwardStatMessage.v18.lel_activeForward = (linedevice->cfwdAll.enabled || linedevice->cfwdBusy.enabled) ? htolel(4) : 0;
	msg->data.ForwardStatMessage.v18.lel_lineNumber = htolel(linedevice->lineInstance);

	if (linedevice->cfwdAll.enabled) {
		msg->data.ForwardStatMessage.v18.lel_forwardAllActive = htolel(4);	// needs more information about the possible values and their meaning
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdallnumber, linedevice->cfwdAll.number, sizeof(msg->data.ForwardStatMessage.v18.cfwdallnumber));
	}
	if (linedevice->cfwdBusy.enabled) {
		msg->data.ForwardStatMessage.v18.lel_forwardBusyActive = htolel(4);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
	}
	/*
	if (linedevice->cfwdNoAnswer.enabled) {
		msg->data.ForwardStatMessage.v18.lel_forwardBusyActive = htolel(4);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdbusynumber, linedevice->cfwdBusy.number, sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
	}
	*/

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
	sccp_netsock_ipv4_mapped(&sas, &sas);

	struct sockaddr_in *in = (struct sockaddr_in *) &sas;

	memcpy(&msg->data.OpenReceiveChannel.v3.bel_remoteIpAddr, &in->sin_addr, 4);
	msg->data.OpenReceiveChannel.v3.lel_remotePortNumber = htolel(sccp_netsock_getPort(&sas));

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
	sccp_netsock_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v17.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.OpenReceiveChannel.v17.lel_ipv46 = htolel(1);
		msg->data.OpenReceiveChannel.v17.lel_requestedIpAddrType = htolel(1);				//for ipv6 this value have to me > 0, lel_ipv46 doesn't matter
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v17.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.OpenReceiveChannel.v17.lel_remotePortNumber = htolel(sccp_netsock_getPort(&sas));
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
	sccp_netsock_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v22.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.OpenReceiveChannel.v22.lel_ipv46 = htolel(1);						//for ipv6 this value have to me > 0, lel_ipv46 doesn't matter
		msg->data.OpenReceiveChannel.v22.lel_requestedIpAddrType = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;

		memcpy(&msg->data.OpenReceiveChannel.v22.bel_remoteIpAddr, &in->sin_addr, 4);
	}
	msg->data.OpenReceiveChannel.v22.lel_remotePortNumber = htolel(sccp_netsock_getPort(&sas));
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
	msg->data.StartMediaTransmission.v3.lel_remotePortNumber = htolel(sccp_netsock_getPort(&channel->rtp.audio.phone_remote));

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
	msg->data.StartMediaTransmission.v17.lel_remotePortNumber = htolel(sccp_netsock_getPort(&channel->rtp.audio.phone_remote));
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
	msg->data.StartMediaTransmission.v22.lel_remotePortNumber = htolel(sccp_netsock_getPort(&channel->rtp.audio.phone_remote));
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
	msg->data.StartMultiMediaTransmission.v3.lel_remotePortNumber = htolel(sccp_netsock_getPort(&channel->rtp.video.phone_remote));
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

	msg->data.StartMultiMediaTransmission.v17.lel_remotePortNumber = htolel(sccp_netsock_getPort(&channel->rtp.video.phone_remote));
	if (channel->rtp.video.phone_remote.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &channel->rtp.video.phone_remote;

		memcpy(&msg->data.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &in6->sin6_addr, 16);
		msg->data.StartMultiMediaTransmission.v17.lel_ipv46 = htolel(1);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &channel->rtp.video.phone_remote;

		memcpy(&msg->data.StartMultiMediaTransmission.v17.bel_remoteIpAddr, &in->sin_addr, 4);
	}
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

	if (device->protocolversion > 17) {
		int num_segments = data_len / (StationMaxXMLMessage + 1);
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

			msg[segment] = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len);
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
			sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message sent to device  (hdr_len: %d, msglen: %d/%d, msg-size: %d).\n", DEV_ID_LOG(device), hdr_len, msg_len, (int) strlen(xmlData), hdr_len + msg_len);
			segment++;
		}
	} else if (data_len < StationMaxXMLMessage) {
		sccp_msg_t *msg = NULL;

		hdr_len = sizeof(msg->data.UserToDeviceDataVersion1Message);
		msg_len = data_len;

		msg = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len);
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
		sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_1 "%s: (sccp_protocol_sendUserToDeviceDataVersion1Message) Message sent to device  (hdr_len: %d, msglen: %d, msg-size: %d).\n", DEV_ID_LOG(device), hdr_len, msg_len, hdr_len + msg_len);
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
		iCallInfo.Getter(sccp_channel_getCallInfo(channel),	SCCP_CALLINFO_CALLEDPARTY_NUMBER, &msg->data.ConnectionStatisticsReq.v3.DirectoryNumber, SCCP_CALLINFO_KEY_SENTINEL);
	} else {
		iCallInfo.Getter(sccp_channel_getCallInfo(channel),	SCCP_CALLINFO_CALLINGPARTY_NUMBER, &msg->data.ConnectionStatisticsReq.v3.DirectoryNumber, SCCP_CALLINFO_KEY_SENTINEL);
	}
	msg->data.ConnectionStatisticsReq.v3.lel_callReference = htolel((channel) ? channel->callid : 0);
	msg->data.ConnectionStatisticsReq.v3.lel_StatsProcessing = htolel(clear);
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start MultiMedia Transmission (V17)
 */
static void sccp_protocol_sendConnectionStatisticsReqV19(constDevicePtr device, constChannelPtr channel, uint8_t clear)
{
	sccp_msg_t *msg = sccp_build_packet(ConnectionStatisticsReq, sizeof(msg->data.ConnectionStatisticsReq.v19));

	if (channel->calltype == SKINNY_CALLTYPE_OUTBOUND) {
		iCallInfo.Getter(sccp_channel_getCallInfo(channel),	SCCP_CALLINFO_CALLEDPARTY_NUMBER,  &msg->data.ConnectionStatisticsReq.v19.DirectoryNumber, SCCP_CALLINFO_KEY_SENTINEL);
	} else {
		iCallInfo.Getter(sccp_channel_getCallInfo(channel),	SCCP_CALLINFO_CALLINGPARTY_NUMBER, &msg->data.ConnectionStatisticsReq.v19.DirectoryNumber, SCCP_CALLINFO_KEY_SENTINEL);
	}
	msg->data.ConnectionStatisticsReq.v19.lel_callReference = htolel((channel) ? channel->callid : 0);
	msg->data.ConnectionStatisticsReq.v19.lel_StatsProcessing = htolel(clear);
	sccp_dev_send(device, msg);
}
/* done - sendUserToDeviceData */


/* sendPortRequest */
/*!
 * \brief Send PortRequest (V3)
 */
static void sccp_protocol_sendPortRequest(constDevicePtr device, constChannelPtr channel, skinny_mediaTransportType_t mediaTransportType, skinny_mediaType_t mediaType)
{
	struct sockaddr_storage sas;
	memcpy(&sas, &channel->rtp.audio.phone_remote, sizeof(struct sockaddr_storage));
	sccp_msg_t *msg = sccp_build_packet(PortRequestMessage, sizeof(msg->data.PortRequestMessage));

	msg->data.PortRequestMessage.lel_conferenceId = htolel(channel->callid);
	msg->data.PortRequestMessage.lel_callReference = htolel(channel->callid);
	msg->data.PortRequestMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.PortRequestMessage.lel_mediaTransportType = htolel(mediaTransportType);
	msg->data.PortRequestMessage.lel_ipv46 = htolel(sccp_netsock_is_IPv6(&sas) ? 1 : 0);
	msg->data.PortRequestMessage.lel_mediaType = htolel(mediaType);

	sccp_dev_send(device, msg);
}
/* done - sendPortRequest */

/* sendPortClose */
/*!
 * \brief Send PortClose
 */
static void sccp_protocol_sendPortClose(constDevicePtr device, constChannelPtr channel, skinny_mediaType_t mediaType)
{
	if (device->protocol && device->protocol->version >= 11) {
		sccp_msg_t *msg = sccp_build_packet(PortCloseMessage, sizeof(msg->data.PortCloseMessage));
		msg->data.PortCloseMessage.lel_conferenceId = htolel(channel->callid);
		msg->data.PortCloseMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
		msg->data.PortCloseMessage.lel_callReference = htolel(channel->callid);
		msg->data.PortCloseMessage.lel_mediaType = htolel(mediaType);
		sccp_dev_send(device, msg);
	}
}
/* done - sendPortClose */

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
	sccp_copy_string(calledParty, msg->data.EnblocCallMessage.v18u.calledParty, 25);

	/* message exists in both packed and unpacked version */
	/* 8945 v22 unpacked */
	// 00000000 - 24 00 00 00 16 00 00 00  04 00 00 00 39 38 30 31  - $...........9801
	// 00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............
	// 00000020 - 00 00 00 00 00 00 00 00  01 00 00 00              - ............
	
	/* 7970 v22 packed*/
	// 00000000 - 24 00 00 00 16 00 00 00  04 00 00 00 39 38 30 31  - $...........9801
	// 00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............
	// 00000020 - 00 00 00 00 00 01 00 00  00 00 00 00              - ............
	
	if (letohl(msg->data.EnblocCallMessage.v18u.lel_lineInstance) > 0) {
		*lineInstance = letohl(msg->data.EnblocCallMessage.v18u.lel_lineInstance);
	} else {
#if defined(HAVE_UNALIGNED_BUSERROR)
		*lineInstance = letohl(get_unaligned_uint32((const void *) &msg->data.EnblocCallMessage.v18p.lel_lineInstance));
#else
		*lineInstance = letohl(msg->data.EnblocCallMessage.v18p.lel_lineInstance);
#endif
	}
}

/* sendPortResponse */
static void sccp_protocol_parsePortResponseV3(constMessagePtr msg, uint32_t *conferenceId, uint32_t *callReference, uint32_t *passThruPartyId, struct sockaddr_storage *ss, uint32_t * RTCPPortNumber, skinny_mediaType_t *mediaType)
{
	*conferenceId = letohl(msg->data.PortResponseMessage.v3.lel_conferenceId);
	*callReference = letohl(msg->data.PortResponseMessage.v3.lel_callReference);
	*passThruPartyId = letohl(msg->data.PortResponseMessage.v3.lel_passThruPartyId);

	ss->ss_family = AF_INET;
	struct sockaddr_in *sin = (struct sockaddr_in *) ss;
	memcpy(&sin->sin_addr, &msg->data.PortResponseMessage.v3.bel_ipAddr, sizeof(sin->sin_addr));
	sin->sin_port = htons(htolel(msg->data.PortResponseMessage.v3.lel_portNumber));

	*RTCPPortNumber = letohl(msg->data.PortResponseMessage.v3.lel_RTCPPortNumber);
	*mediaType = SKINNY_MEDIATYPE_SENTINEL;
}

static void sccp_protocol_parsePortResponseV19(constMessagePtr msg, uint32_t *conferenceId, uint32_t *callReference, uint32_t *passThruPartyId, struct sockaddr_storage *ss, uint32_t * RTCPPortNumber, skinny_mediaType_t *mediaType)
{
	*conferenceId = letohl(msg->data.PortResponseMessage.v19.lel_conferenceId);
	*callReference = letohl(msg->data.PortResponseMessage.v19.lel_callReference);
	*passThruPartyId = letohl(msg->data.PortResponseMessage.v19.lel_passThruPartyId);

	if (letohl(msg->data.PortResponseMessage.v19.lel_ipv46) == 0) {			// read ipv4 address
		ss->ss_family = AF_INET;
		struct sockaddr_in *sin = (struct sockaddr_in *) ss;

		memcpy(&sin->sin_addr, &msg->data.PortResponseMessage.v19.bel_ipAddr, sizeof(sin->sin_addr));
		sin->sin_port = htons(htolel(msg->data.PortResponseMessage.v19.lel_portNumber));
	} else {												// read ipv6 address
		ss->ss_family = AF_INET6;
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ss;

		memcpy(&sin6->sin6_addr, &msg->data.PortResponseMessage.v19.bel_ipAddr, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(htolel(msg->data.PortResponseMessage.v19.lel_portNumber));
	}
	*RTCPPortNumber = letohl(msg->data.PortResponseMessage.v19.lel_RTCPPortNumber);
	*mediaType = letohl(msg->data.PortResponseMessage.v19.lel_mediaType);
}
/* done - sendPortResponse */

/* =================================================================================================================== Map Messages to Protocol Version */

/*! 
 * \brief SCCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *sccpProtocolDefinition[] = {
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 3, TimeDateReqMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV3, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},	/* default impl */
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 5, TimeDateReqMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 8, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 9, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 10, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 11, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 15, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 16, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 17, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 18, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 19, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 20, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 21, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 22, TimeDateReqMessage, sccp_protocol_sendCallInfoV16,sccp_protocol_sendDialedNumberV18,sccp_protocol_sendRegisterAckV11,sccp_protocol_sendDynamicDisplayprompt,sccp_protocol_sendDynamicDisplayNotify,sccp_protocol_sendDynamicDisplayPriNotify,sccp_protocol_sendCallForwardStatusV18,sccp_protocol_sendUserToDeviceDataVersion1Message,sccp_protocol_sendFastPictureUpdate,sccp_protocol_sendOpenReceiveChannelv22,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17,sccp_protocol_sendStartMediaTransmissionv22,sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV17,sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17,sccp_protocol_parseStartMediaTransmissionAckV17,sccp_protocol_parseStartMultiMediaTransmissionAckV17,sccp_protocol_parseEnblocCallV22, sccp_protocol_parsePortResponseV19},
};

/*! 
 * \brief SPCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *spcpProtocolDefinition[] = {
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 0, RegisterAvailableLinesMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 8, RegisterAvailableLinesMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendFastPictureUpdate, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
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
		case KEYMODE_HOLDCONF:
			return "Have a Conference On Hold";
		default:
			return "Unknown KeyMode";
	}
}

const struct messagetype sccp_messagetypes[] = {
	/* *INDENT-OFF* */
	[KeepAliveMessage] = {KeepAliveMessage,						"Keep Alive Message",				offsize(sccp_data_t, StationKeepAliveMessage)},
	[RegisterMessage] = {RegisterMessage,						"Register Message",				offsize(sccp_data_t, RegisterMessage)},
	[IpPortMessage] = {IpPortMessage,						"Ip-Port Message",				offsize(sccp_data_t, IpPortMessage)},
	[KeypadButtonMessage] = {KeypadButtonMessage,					"Keypad Button Message",			offsize(sccp_data_t, KeypadButtonMessage)},
	[EnblocCallMessage] = {EnblocCallMessage,					"Enbloc Call Message",				offsize(sccp_data_t, EnblocCallMessage)},
	[StimulusMessage] = {StimulusMessage,						"Stimulus Message",				offsize(sccp_data_t, StimulusMessage)},
	[OffHookMessage] = {OffHookMessage,						"Off-Hook Message",				offsize(sccp_data_t, OffHookMessage)},
	[OnHookMessage] = {OnHookMessage,						"On-Hook Message",				offsize(sccp_data_t, OnHookMessage)},
	[HookFlashMessage] = {HookFlashMessage,						"Hook-Flash Message",				offsize(sccp_data_t, HookFlashMessage)},
	[ForwardStatReqMessage] = {ForwardStatReqMessage,				"Forward State Request",			offsize(sccp_data_t, ForwardStatReqMessage)},
	[SpeedDialStatReqMessage] = {SpeedDialStatReqMessage,				"Speed-Dial State Request",			offsize(sccp_data_t, SpeedDialStatReqMessage)},
	[LineStatReqMessage] = {LineStatReqMessage,					"Line State Request",				offsize(sccp_data_t, LineStatReqMessage)},
	[ConfigStatReqMessage] = {ConfigStatReqMessage,					"Config State Request",				offsize(sccp_data_t, ConfigStatReqMessage)},
	[TimeDateReqMessage] = {TimeDateReqMessage,					"Time Date Request",				offsize(sccp_data_t, TimeDateReqMessage)},
	[ButtonTemplateReqMessage] = {ButtonTemplateReqMessage,				"Button Template Request",			offsize(sccp_data_t, ButtonTemplateReqMessage)},
	[VersionReqMessage] = {VersionReqMessage,					"Version Request",				offsize(sccp_data_t, VersionReqMessage)},
	[CapabilitiesResMessage] = {CapabilitiesResMessage,				"Capabilities Response Message",		offsize(sccp_data_t, CapabilitiesResMessage)},
	[MediaPortListMessage] = {MediaPortListMessage,					"Media Port List Message",			offsize(sccp_data_t, MediaPortListMessage)},
	[ServerReqMessage] = {ServerReqMessage,						"Server Request",				offsize(sccp_data_t, ServerReqMessage)},
	[AlarmMessage] = {AlarmMessage,							"Alarm Message",				offsize(sccp_data_t, AlarmMessage)},
	[MulticastMediaReceptionAck] = {MulticastMediaReceptionAck,			"Multicast Media Reception Acknowledge",	offsize(sccp_data_t, MulticastMediaReceptionAck)},
	[OpenReceiveChannelAck] = {OpenReceiveChannelAck,				"Open Receive Channel Acknowledge",		offsize(sccp_data_t, OpenReceiveChannelAck)},
	[ConnectionStatisticsRes] = {ConnectionStatisticsRes,				"Connection Statistics Response",		offsize(sccp_data_t, ConnectionStatisticsRes)},
	[OffHookWithCgpnMessage] = {OffHookWithCgpnMessage,				"Off-Hook With Cgpn Message",			offsize(sccp_data_t, OffHookWithCgpnMessage)},
	[SoftKeySetReqMessage] = {SoftKeySetReqMessage,					"SoftKey Set Request",				offsize(sccp_data_t, SoftKeySetReqMessage)},
	[SoftKeyEventMessage] = {SoftKeyEventMessage,					"SoftKey Event Message",			offsize(sccp_data_t, SoftKeyEventMessage)},
	[UnregisterMessage] = {UnregisterMessage,					"Unregister Message",				offsize(sccp_data_t, UnregisterMessage)},
	[SoftKeyTemplateReqMessage] = {SoftKeyTemplateReqMessage,			"SoftKey Template Request",			offsize(sccp_data_t, SoftKeyTemplateReqMessage)},
	[RegisterTokenRequest] = {RegisterTokenRequest,					"Register Token Request",			offsize(sccp_data_t, RegisterTokenRequest)},
	[MediaTransmissionFailure] = {MediaTransmissionFailure,				"Media Transmission Failure",			offsize(sccp_data_t, MediaTransmissionFailure)}, 
	[HeadsetStatusMessage] = {HeadsetStatusMessage,					"Headset Status Message",			offsize(sccp_data_t, HeadsetStatusMessage)},
	[MediaResourceNotification] = {MediaResourceNotification,			"Media Resource Notification",			offsize(sccp_data_t, MediaResourceNotification)},
	[RegisterAvailableLinesMessage] = {RegisterAvailableLinesMessage,		"Register Available Lines Message",		offsize(sccp_data_t, RegisterAvailableLinesMessage)},
	[DeviceToUserDataMessage] = {DeviceToUserDataMessage,				"Device To User Data Message",			offsize(sccp_data_t, DeviceToUserDataMessage)},
	[DeviceToUserDataResponseMessage] = {DeviceToUserDataResponseMessage,		"Device To User Data Response",			offsize(sccp_data_t, DeviceToUserDataResponseMessage)},
	[UpdateCapabilitiesMessage] = {UpdateCapabilitiesMessage,			"Update Capabilities Message",			offsize(sccp_data_t, UpdateCapabilitiesMessage)},
	[OpenMultiMediaReceiveChannelAckMessage] = {OpenMultiMediaReceiveChannelAckMessage,	"Open MultiMedia Receive Channel Acknowledge",	offsize(sccp_data_t, OpenMultiMediaReceiveChannelAckMessage)},
	[ClearConferenceMessage] = {ClearConferenceMessage,				"Clear Conference Message",			offsize(sccp_data_t, ClearConferenceMessage)},
	[ServiceURLStatReqMessage] = {ServiceURLStatReqMessage,				"Service URL State Request",			offsize(sccp_data_t, ServiceURLStatReqMessage)},
	[FeatureStatReqMessage] = {FeatureStatReqMessage,				"Feature State Request",			offsize(sccp_data_t, FeatureStatReqMessage)},
	[CreateConferenceResMessage] = {CreateConferenceResMessage,			"Create Conference Response",			offsize(sccp_data_t, CreateConferenceResMessage)},
	[DeleteConferenceResMessage] = {DeleteConferenceResMessage,			"Delete Conference Response",			offsize(sccp_data_t, DeleteConferenceResMessage)},
	[ModifyConferenceResMessage] = {ModifyConferenceResMessage,			"Modify Conference Response",			offsize(sccp_data_t, ModifyConferenceResMessage)},
	[AddParticipantResMessage] = {AddParticipantResMessage,				"Add Participant Response",			offsize(sccp_data_t, AddParticipantResMessage)},
	[AuditConferenceResMessage] = {AuditConferenceResMessage,			"Audit Conference Response",			offsize(sccp_data_t, AuditConferenceResMessage)},
	[AuditParticipantResMessage] = {AuditParticipantResMessage,			"Audit Participant Response",			offsize(sccp_data_t, AuditParticipantResMessage)},
	[DeviceToUserDataVersion1Message] = {DeviceToUserDataVersion1Message,		"Device To User Data Version1 Message",		offsize(sccp_data_t, DeviceToUserDataVersion1Message)},
	[DeviceToUserDataResponseVersion1Message] = {DeviceToUserDataResponseVersion1Message,	"Device To User Data Version1 Response",offsize(sccp_data_t, DeviceToUserDataResponseVersion1Message)},
	[SubscriptionStatReqMessage] = {SubscriptionStatReqMessage,			"Subscription Status Request (DialedPhoneBook)", offsize(sccp_data_t, SubscriptionStatReqMessage)},
	[AccessoryStatusMessage] = {AccessoryStatusMessage,				"Accessory Status Message",			offsize(sccp_data_t, AccessoryStatusMessage)},
	[RegisterAckMessage] = {RegisterAckMessage,					"Register Acknowledge",				offsize(sccp_data_t, RegisterAckMessage)},
	[StartToneMessage] = {StartToneMessage,						"Start Tone Message",				offsize(sccp_data_t, StartToneMessage)},
	[StopToneMessage] = {StopToneMessage,						"Stop Tone Message",				offsize(sccp_data_t, StopToneMessage)},
	[SetRingerMessage] = {SetRingerMessage,						"Set Ringer Message",				offsize(sccp_data_t, SetRingerMessage)},
	[SetLampMessage] = {SetLampMessage,						"Set Lamp Message",				offsize(sccp_data_t, SetLampMessage)},
	[SetHookFlashDetectMessage] = {SetHookFlashDetectMessage,			"Set HookFlash Detect Message",			offsize(sccp_data_t, SetHookFlashDetectMessage)},
	[SetSpeakerModeMessage] = {SetSpeakerModeMessage,				"Set Speaker Mode Message",			offsize(sccp_data_t, SetSpeakerModeMessage)},
	[SetMicroModeMessage] = {SetMicroModeMessage,					"Set Micro Mode Message",			offsize(sccp_data_t, SetMicroModeMessage)},
	[StartMediaTransmission] = {StartMediaTransmission,				"Start Media Transmission",			offsize(sccp_data_t, StartMediaTransmission)},
	[StopMediaTransmission] = {StopMediaTransmission,				"Stop Media Transmission",			offsize(sccp_data_t, StopMediaTransmission)},
	[StartMediaReception] = {StartMediaReception,					"Start Media Reception",			offsize(sccp_data_t, StartMediaReception)},
	[StopMediaReception] = {StopMediaReception,					"Stop Media Reception",				offsize(sccp_data_t, StopMediaReception)},
	[CallInfoMessage] = {CallInfoMessage,						"Call Information Message",			offsize(sccp_data_t, CallInfoMessage)},
	[ForwardStatMessage] = {ForwardStatMessage,					"Forward State Message",			offsize(sccp_data_t, ForwardStatMessage)},
	[SpeedDialStatMessage] = {SpeedDialStatMessage,					"SpeedDial State Message",			offsize(sccp_data_t, SpeedDialStatMessage)},
	[LineStatMessage] = {LineStatMessage,						"Line State Message",				offsize(sccp_data_t, LineStatMessage)},
	[ConfigStatMessage] = {ConfigStatMessage,					"Config State Message",				offsize(sccp_data_t, ConfigStatMessage)},
	[ConfigStatDynamicMessage] = {ConfigStatDynamicMessage,				"Config State Dynamic Message",			offsize(sccp_data_t, ConfigStatDynamicMessage)},
	[DefineTimeDate] = {DefineTimeDate,						"Define Time Date",				offsize(sccp_data_t, DefineTimeDate)},
	[StartSessionTransmission] = {StartSessionTransmission,				"Start Session Transmission",			offsize(sccp_data_t, StartSessionTransmission)},
	[StopSessionTransmission] = {StopSessionTransmission,				"Stop Session Transmission",			offsize(sccp_data_t, StopSessionTransmission)},
	[ButtonTemplateMessage] = {ButtonTemplateMessage,				"Button Template Message",			offsize(sccp_data_t, ButtonTemplateMessage)},
	//[ButtonTemplateMessageSingle] = {ButtonTemplateMessageSingle,			"Button Template Message Single",		offsize(sccp_data_t, ButtonTemplateMessageSingle)},
	[VersionMessage] = {VersionMessage,						"Version Message",				offsize(sccp_data_t, VersionMessage)},
	[DisplayTextMessage] = {DisplayTextMessage,					"Display Text Message",				offsize(sccp_data_t, DisplayTextMessage)},
	[ClearDisplay] = {ClearDisplay,							"Clear Display",				offsize(sccp_data_t, ClearDisplay)},
	[CapabilitiesReqMessage] = {CapabilitiesReqMessage,				"Capabilities Request",				offsize(sccp_data_t, CapabilitiesReqMessage)},
	[EnunciatorCommandMessage] = {EnunciatorCommandMessage,				"Enunciator Command Message",			offsize(sccp_data_t, EnunciatorCommandMessage)},
	[RegisterRejectMessage] = {RegisterRejectMessage,				"Register Reject Message",			offsize(sccp_data_t, RegisterRejectMessage)},
	[ServerResMessage] = {ServerResMessage,						"Server Response",				offsize(sccp_data_t, ServerResMessage)},
	[Reset] = {Reset,								"Reset",					offsize(sccp_data_t, Reset)},
	[KeepAliveAckMessage] = {KeepAliveAckMessage,					"Keep Alive Acknowledge",			offsize(sccp_data_t, KeepAliveAckMessage)},
	[StartMulticastMediaReception] = {StartMulticastMediaReception,			"Start MulticastMedia Reception",		offsize(sccp_data_t, StartMulticastMediaReception)},
	[StartMulticastMediaTransmission] = {StartMulticastMediaTransmission,		"Start MulticastMedia Transmission",		offsize(sccp_data_t, StartMulticastMediaTransmission)},
	[StopMulticastMediaReception] = {StopMulticastMediaReception,			"Stop MulticastMedia Reception",		offsize(sccp_data_t, StopMulticastMediaReception)},
	[StopMulticastMediaTransmission] = {StopMulticastMediaTransmission,		"Stop MulticastMedia Transmission",		offsize(sccp_data_t, StopMulticastMediaTransmission)},
	[OpenReceiveChannel] = {OpenReceiveChannel,					"Open Receive Channel",				offsize(sccp_data_t, OpenReceiveChannel)},
	[CloseReceiveChannel] = {CloseReceiveChannel,					"Close Receive Channel",			offsize(sccp_data_t, CloseReceiveChannel)},
	[ConnectionStatisticsReq] = {ConnectionStatisticsReq,				"Connection Statistics Request",		offsize(sccp_data_t, ConnectionStatisticsReq)},
	[SoftKeyTemplateResMessage] = {SoftKeyTemplateResMessage,			"SoftKey Template Response",			offsize(sccp_data_t, SoftKeyTemplateResMessage)},
	[SoftKeySetResMessage] = {SoftKeySetResMessage,					"SoftKey Set Response",				offsize(sccp_data_t, SoftKeySetResMessage)},
	[SelectSoftKeysMessage] = {SelectSoftKeysMessage,				"Select SoftKeys Message",			offsize(sccp_data_t, SelectSoftKeysMessage)},
	[CallStateMessage] = {CallStateMessage,						"Call State Message",				offsize(sccp_data_t, CallStateMessage)},
	[DisplayPromptStatusMessage] = {DisplayPromptStatusMessage,			"Display Prompt Status Message",		offsize(sccp_data_t, DisplayPromptStatusMessage)},
	[ClearPromptStatusMessage] = {ClearPromptStatusMessage,				"Clear Prompt Status Message",			offsize(sccp_data_t, ClearPromptStatusMessage)},
	[DisplayNotifyMessage] = {DisplayNotifyMessage,					"Display Notify Message",			offsize(sccp_data_t, DisplayNotifyMessage)},
	[ClearNotifyMessage] = {ClearNotifyMessage,					"Clear Notify Message",				offsize(sccp_data_t, ClearNotifyMessage)},
	[ActivateCallPlaneMessage] = {ActivateCallPlaneMessage,				"Activate Call Plane Message",			offsize(sccp_data_t, ActivateCallPlaneMessage)},
	[DeactivateCallPlaneMessage] = {DeactivateCallPlaneMessage,			"Deactivate Call Plane Message",		offsize(sccp_data_t, DeactivateCallPlaneMessage)},
	[UnregisterAckMessage] = {UnregisterAckMessage,					"Unregister Acknowledge",			offsize(sccp_data_t, UnregisterAckMessage)},
	[BackSpaceResMessage] = {BackSpaceResMessage,					"Back Space Response",				offsize(sccp_data_t, BackSpaceResMessage)},
	[RegisterTokenAck] = {RegisterTokenAck,						"Register Token Acknowledge",			offsize(sccp_data_t, RegisterTokenAck)},
	[RegisterTokenReject] = {RegisterTokenReject,					"Register Token Reject",			offsize(sccp_data_t, RegisterTokenReject)},
	[StartMediaFailureDetection] = {StartMediaFailureDetection,			"Start Media Failure Detection",		offsize(sccp_data_t, StartMediaFailureDetection)},
	[DialedNumberMessage] = {DialedNumberMessage,					"Dialed Number Message",			offsize(sccp_data_t, DialedNumberMessage)},
	[UserToDeviceDataMessage] = {UserToDeviceDataMessage,				"User To Device Data Message",			offsize(sccp_data_t, UserToDeviceDataMessage)},
	[FeatureStatMessage] = {FeatureStatMessage,					"Feature State Message",			offsize(sccp_data_t, FeatureStatMessage)},
	[DisplayPriNotifyMessage] = {DisplayPriNotifyMessage,				"Display Pri Notify Message",			offsize(sccp_data_t, DisplayPriNotifyMessage)},
	[ClearPriNotifyMessage] = {ClearPriNotifyMessage,				"Clear Pri Notify Message",			offsize(sccp_data_t, ClearPriNotifyMessage)},
	[StartAnnouncementMessage] = {StartAnnouncementMessage,				"Start Announcement Message",			offsize(sccp_data_t, StartAnnouncementMessage)},
	[StopAnnouncementMessage] = {StopAnnouncementMessage,				"Stop Announcement Message",			offsize(sccp_data_t, StopAnnouncementMessage)},
	[AnnouncementFinishMessage] = {AnnouncementFinishMessage,			"Announcement Finish Message",			offsize(sccp_data_t, AnnouncementFinishMessage)},
	[NotifyDtmfToneMessage] = {NotifyDtmfToneMessage,				"Notify DTMF Tone Message",			offsize(sccp_data_t, NotifyDtmfToneMessage)},
	[SendDtmfToneMessage] = {SendDtmfToneMessage,					"Send DTMF Tone Message",			offsize(sccp_data_t, SendDtmfToneMessage)},
	[SubscribeDtmfPayloadReqMessage] = {SubscribeDtmfPayloadReqMessage,		"Subscribe DTMF Payload Request",		offsize(sccp_data_t, SubscribeDtmfPayloadReqMessage)},
	[SubscribeDtmfPayloadResMessage] = {SubscribeDtmfPayloadResMessage,		"Subscribe DTMF Payload Response",		offsize(sccp_data_t, SubscribeDtmfPayloadResMessage)},
	[SubscribeDtmfPayloadErrMessage] = {SubscribeDtmfPayloadErrMessage,		"Subscribe DTMF Payload Error Message",		offsize(sccp_data_t, SubscribeDtmfPayloadErrMessage)},
	[UnSubscribeDtmfPayloadReqMessage] = {UnSubscribeDtmfPayloadReqMessage,		"UnSubscribe DTMF Payload Request",		offsize(sccp_data_t, UnSubscribeDtmfPayloadReqMessage)},
	[UnSubscribeDtmfPayloadResMessage] = {UnSubscribeDtmfPayloadResMessage,		"UnSubscribe DTMF Payload Response",		offsize(sccp_data_t, UnSubscribeDtmfPayloadResMessage)},
	[UnSubscribeDtmfPayloadErrMessage] = {UnSubscribeDtmfPayloadErrMessage,		"UnSubscribe DTMF Payload Error Message",	offsize(sccp_data_t, UnSubscribeDtmfPayloadErrMessage)},
	[ServiceURLStatMessage] = {ServiceURLStatMessage,				"ServiceURL State Message",			offsize(sccp_data_t, ServiceURLStatMessage)},
	[CallSelectStatMessage] = {CallSelectStatMessage,				"Call Select State Message",			offsize(sccp_data_t, CallSelectStatMessage)},
	[OpenMultiMediaChannelMessage] = {OpenMultiMediaChannelMessage,			"Open MultiMedia Channel Message",		offsize(sccp_data_t, OpenMultiMediaChannelMessage)},
	[StartMultiMediaTransmission] = {StartMultiMediaTransmission,			"Start MultiMedia Transmission",		offsize(sccp_data_t, StartMultiMediaTransmission)},
	[StopMultiMediaTransmission] = {StopMultiMediaTransmission,			"Stop MultiMedia Transmission",			offsize(sccp_data_t, StopMultiMediaTransmission)},
	[MiscellaneousCommandMessage] = {MiscellaneousCommandMessage,			"Miscellaneous Command Message",		offsize(sccp_data_t, MiscellaneousCommandMessage)},
	[FlowControlCommandMessage] = {FlowControlCommandMessage,			"Flow Control Command Message",			offsize(sccp_data_t, FlowControlCommandMessage)},
	[CloseMultiMediaReceiveChannel] = {CloseMultiMediaReceiveChannel,		"Close MultiMedia Receive Channel",		offsize(sccp_data_t, CloseMultiMediaReceiveChannel)},
	[CreateConferenceReqMessage] = {CreateConferenceReqMessage,			"Create Conference Request",			offsize(sccp_data_t, CreateConferenceReqMessage)},
	[DeleteConferenceReqMessage] = {DeleteConferenceReqMessage,			"Delete Conference Request",			offsize(sccp_data_t, DeleteConferenceReqMessage)},
	[ModifyConferenceReqMessage] = {ModifyConferenceReqMessage,			"Modify Conference Request",			offsize(sccp_data_t, ModifyConferenceReqMessage)},
	[AddParticipantReqMessage] = {AddParticipantReqMessage,				"Add Participant Request",			offsize(sccp_data_t, AddParticipantReqMessage)},
	[DropParticipantReqMessage] = {DropParticipantReqMessage,			"Drop Participant Request",			offsize(sccp_data_t, DropParticipantReqMessage)},
	[AuditConferenceReqMessage] = {AuditConferenceReqMessage,			"Audit Conference Request",			offsize(sccp_data_t, AuditConferenceReqMessage)},
	[AuditParticipantReqMessage] = {AuditParticipantReqMessage,			"Audit Participant Request",			offsize(sccp_data_t, AuditParticipantReqMessage)},
	[UserToDeviceDataVersion1Message] = {UserToDeviceDataVersion1Message,		"User To Device Data Version1 Message",		offsize(sccp_data_t, UserToDeviceDataVersion1Message)},
	[DisplayDynamicNotifyMessage] = {DisplayDynamicNotifyMessage,			"Display Dynamic Notify Message",		offsize(sccp_data_t, DisplayDynamicNotifyMessage)},
	[DisplayDynamicPriNotifyMessage] = {DisplayDynamicPriNotifyMessage,		"Display Dynamic Priority Notify Message",	offsize(sccp_data_t, DisplayDynamicPriNotifyMessage)},
	[DisplayDynamicPromptStatusMessage] = {DisplayDynamicPromptStatusMessage,"Display Dynamic Prompt Status Message",	offsize(sccp_data_t, DisplayDynamicPromptStatusMessage)},
	[FeatureStatDynamicMessage] = {FeatureStatDynamicMessage,			"SpeedDial State Dynamic Message",		offsize(sccp_data_t, FeatureStatDynamicMessage)},
	[LineStatDynamicMessage] = {LineStatDynamicMessage,				"Line State Dynamic Message",			offsize(sccp_data_t, LineStatDynamicMessage)},
	[ServiceURLStatDynamicMessage] = {ServiceURLStatDynamicMessage,			"Service URL Stat Dynamic Messages",		offsize(sccp_data_t, ServiceURLStatDynamicMessage)},
	[SpeedDialStatDynamicMessage] = {SpeedDialStatDynamicMessage,			"SpeedDial Stat Dynamic Message",		offsize(sccp_data_t, SpeedDialStatDynamicMessage)},
	[CallInfoDynamicMessage] = {CallInfoDynamicMessage,				"Call Information Dynamic Message",		offsize(sccp_data_t, CallInfoDynamicMessage)},
	[SubscriptionStatMessage] = {SubscriptionStatMessage,				"Subscription Status Response (Dialed Number)", offsize(sccp_data_t, SubscriptionStatMessage)},
	[NotificationMessage] = {NotificationMessage,					"Notify Call List (CallListStatusUpdate)",	offsize(sccp_data_t, NotificationMessage)},
	[StartMediaTransmissionAck] = {StartMediaTransmissionAck,			"Start Media Transmission Acknowledge",		offsize(sccp_data_t, StartMediaTransmissionAck)},
	[StartMultiMediaTransmissionAck] = {StartMultiMediaTransmissionAck,		"Start Media Transmission Acknowledge",		offsize(sccp_data_t, StartMultiMediaTransmissionAck)},
	[CallHistoryDispositionMessage] = {CallHistoryDispositionMessage,		"Call History Disposition",			offsize(sccp_data_t, CallHistoryDispositionMessage)},
	[LocationInfoMessage] = {LocationInfoMessage,					"Location/Wifi Information",			offsize(sccp_data_t, LocationInfoMessage)},
	[ExtensionDeviceCaps] = {ExtensionDeviceCaps,					"Extension Device Capabilities Message",	offsize(sccp_data_t, ExtensionDeviceCaps)},
	[XMLAlarmMessage] = {XMLAlarmMessage,						"XML-AlarmMessage",				offsize(sccp_data_t, XMLAlarmMessage)},
	[MediaPathCapabilityMessage] = {MediaPathCapabilityMessage,			"MediaPath Capability Message",			offsize(sccp_data_t, MediaPathCapabilityMessage)},
	[FlowControlNotifyMessage] = {FlowControlNotifyMessage,				"FlowControl Notify Message",			offsize(sccp_data_t, FlowControlNotifyMessage)},
	[CallCountReqMessage] = {CallCountReqMessage,					"CallCount Request Message",			offsize(sccp_data_t, CallCountReqMessage)},
/*new*/
	[UpdateCapabilitiesV2Message] = {UpdateCapabilitiesV2Message,			"Update Capabilities V2",			offsize(sccp_data_t, UpdateCapabilitiesV2Message)},
	[UpdateCapabilitiesV3Message] = {UpdateCapabilitiesV3Message,			"Update Capabilities V3",			offsize(sccp_data_t, UpdateCapabilitiesV3Message)},
	[PortResponseMessage] = {PortResponseMessage,					"Port Response Message",			offsize(sccp_data_t, PortResponseMessage)},
	[QoSResvNotifyMessage] = {QoSResvNotifyMessage,					"QoS Resv Notify Message",			offsize(sccp_data_t, QoSResvNotifyMessage)},
	[QoSErrorNotifyMessage] = {QoSErrorNotifyMessage,				"QoS Error Notify Message",			offsize(sccp_data_t, QoSErrorNotifyMessage)},
	[PortRequestMessage] = {PortRequestMessage,					"Port Request Message",				offsize(sccp_data_t, PortRequestMessage)},
	[PortCloseMessage] = {PortCloseMessage,						"Port Close Message",				offsize(sccp_data_t, PortCloseMessage)},
	[QoSListenMessage] = {QoSListenMessage,						"QoS Listen Message",				offsize(sccp_data_t, QoSListenMessage)},
	[QoSPathMessage] = {QoSPathMessage,						"QoS Path Message",				offsize(sccp_data_t, QoSPathMessage)},
	[QoSTeardownMessage] = {QoSTeardownMessage,					"QoS Teardown Message",				offsize(sccp_data_t, QoSTeardownMessage)},
	[UpdateDSCPMessage] = {UpdateDSCPMessage,					"Update DSCP Message",				offsize(sccp_data_t, UpdateDSCPMessage)},
	[QoSModifyMessage] = {QoSModifyMessage,						"QoS Modify Message",				offsize(sccp_data_t, QoSModifyMessage)},
	[MwiResponseMessage] = {MwiResponseMessage,					"Mwi Response Message",				offsize(sccp_data_t, MwiResponseMessage)},
	[CallCountRespMessage] = {CallCountRespMessage,					"CallCount Response Message",			offsize(sccp_data_t, CallCountRespMessage)},
	[RecordingStatusMessage] = {RecordingStatusMessage,				"Recording Status Message",			offsize(sccp_data_t, RecordingStatusMessage)},
	/* *INDENT-ON* */
};

const struct messagetype spcp_messagetypes[] = {
	/* *INDENT-OFF* */
	[SPCPRegisterTokenRequest - SPCP_MESSAGE_OFFSET	] = {SPCPRegisterTokenRequest,	"SPCP Register Token Request",			offsize(sccp_data_t, SPCPRegisterTokenRequest)},
	[SPCPRegisterTokenAck - SPCP_MESSAGE_OFFSET	] = {SPCPRegisterTokenAck,	"SPCP RegisterMessageACK",			offsize(sccp_data_t, SPCPRegisterTokenAck)},
	[SPCPRegisterTokenReject - SPCP_MESSAGE_OFFSET	] = {SPCPRegisterTokenReject,	"SPCP RegisterMessageReject",			offsize(sccp_data_t, SPCPRegisterTokenReject)},
	//[UnknownVGMessage - SPCP_MESSAGE_OFFSET		] = {UnknownVGMessage,		"Unknown Message (VG224)",			offsize(sccp_data_t, UnknownVGMessage)},
	/* *INDENT-ON* */
};

gcc_inline const char *msgtype2str(sccp_mid_t msgId)
{														/* sccp_protocol.h */
	if (msgId >= SPCP_MESSAGE_OFFSET && (msgId - SPCP_MESSAGE_OFFSET) < ARRAY_LEN(spcp_messagetypes)) {
		return spcp_messagetypes[msgId - SPCP_MESSAGE_OFFSET].text;
	}
	if (msgId < ARRAY_LEN(sccp_messagetypes)) {
		return sccp_messagetypes[msgId].text;
	} 
	return "SCCP: Requested MessageId does not exist";
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
