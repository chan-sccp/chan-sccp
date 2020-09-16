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
#include "sccp_linedevice.h"
#include "sccp_session.h"
#include "sccp_utils.h"
#include <asterisk/unaligned.h>
SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \brief Build an SCCP Message Packet
 * \param[in] t SCCP Message Text
 * \param[out] pkt_len Packet Length
 * \return SCCP Message
 */
messagePtr __attribute__((malloc)) sccp_build_packet(sccp_mid_t t, size_t pkt_len)
{
	int padding = ((pkt_len + 8) % 4);
	padding = (padding > 0) ? 4 - padding : 0;

	sccp_msg_t * msg = (sccp_msg_t *)sccp_calloc(1, pkt_len + SCCP_PACKET_HEADER + padding);

	if(!msg) {
		pbx_log(LOG_WARNING, "SCCP: Packet memory allocation error\n");
		return NULL;
	}
	msg->header.length = htolel(pkt_len + 4 + padding);
	msg->header.lel_messageId = htolel(t);

	// sccp_log(DEBUGCAT_DEVICE)("SCCP: (sccp_build_packet) created packet type:0x%x, msg_size=%lu, hdr_len=%lu\n", t, pkt_len + SCCP_PACKET_HEADER + padding, pkt_len + 4 + padding)
	return msg;
}

/* CallInfo Message */

/* =================================================================================================================== Send Messages */
static void sccp_protocol_sendCallInfoV3 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, const skinny_callsecuritystate_t callsecurityState, constDevicePtr device)
{
 	pbx_assert(device != NULL);
	sccp_msg_t * msg = NULL;

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

	// 7920's exception. They don't seem to reverse the interpretation of the presentation flag
	if (device->skinny_type == SKINNY_DEVICETYPE_CISCO7920) {
		msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0x0 : 0xf;
	} else {
		msg->data.CallInfoMessage.partyPIRestrictionBits = presentation ? 0xf : 0x0;
	}
	msg->data.CallInfoMessage.lel_lineInstance = htolel(lineInstance);
	msg->data.CallInfoMessage.lel_callReference = htolel(callid);
	msg->data.CallInfoMessage.lel_callType = htolel(calltype);
	msg->data.CallInfoMessage.lel_callInstance = htolel(callInstance);
	msg->data.CallInfoMessage.lel_callSecurityStatus = htolel(callsecurityState);
	msg->data.CallInfoMessage.lel_originalCdpnRedirectReason = htolel(originalCdpnRedirectReason);
	msg->data.CallInfoMessage.lel_lastRedirectingReason = htolel(lastRedirectingReason);

	//sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V3) for %s channel %d/%d on line instance %d\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, callInstance, lineInstance);
	//if ((GLOB(debug) & (DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) != 0) {
	//	iCallInfo.Print2log(ci, "SCCP: (sendCallInfoV3)");
	//}
	sccp_dev_send(device, msg);
}

static void sccp_protocol_sendCallInfoV7 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, const skinny_callsecuritystate_t callsecurityState, constDevicePtr device)
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
	//! note callSecurityStatus:
	// when indicating ringout we should set SKINNY_CALLSECURITYSTATE_UNKNOWN
	// when indicating connected we should set SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(callsecurityState);
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
	//	sccp_dump_msg(msg);
	//}
	sccp_dev_send(device, msg);
}

static void sccp_protocol_sendCallInfoV16 (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, const skinny_callsecuritystate_t callsecurityState, constDevicePtr device)
{
 	pbx_assert(device != NULL);
	sccp_msg_t *msg = NULL;

	unsigned int dataSize = 15;
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
		SCCP_CALLINFO_HUNT_PILOT_NUMBER, &data[13],
		SCCP_CALLINFO_HUNT_PILOT_NAME, &data[14],
		SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, &originalCdpnRedirectReason,
		SCCP_CALLINFO_LAST_REDIRECT_REASON, &lastRedirectingReason,
		SCCP_CALLINFO_PRESENTATION, &presentation,
		SCCP_CALLINFO_KEY_SENTINEL);

	unsigned int field = 0;
	int data_len = 0;
	int dummy_len = 0;
	uint8_t *dummy = (uint8_t *)sccp_calloc(sizeof(uint8_t), dataSize * StationMaxNameSize);
	if (!dummy) {
		return;
	}
	for (field = 0; field < dataSize; field++) {
		data_len = strlen(data[field]) + 1; 		//add NULL terminator
		memcpy(dummy + dummy_len, data[field], data_len);
		dummy_len += data_len;
	}
	int hdr_len = sizeof(msg->data.CallInfoDynamicMessage) - 4;
	msg = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len);
	msg->data.CallInfoDynamicMessage.lel_lineInstance		= htolel(lineInstance);
	msg->data.CallInfoDynamicMessage.lel_callReference		= htolel(callid);
	msg->data.CallInfoDynamicMessage.lel_callType			= htolel(calltype);
	msg->data.CallInfoDynamicMessage.partyPIRestrictionBits		= presentation ? 0x0 : 0xf;
	msg->data.CallInfoDynamicMessage.lel_callSecurityStatus		= htolel(callsecurityState);
	msg->data.CallInfoDynamicMessage.lel_callInstance		= htolel(callInstance);
	msg->data.CallInfoDynamicMessage.lel_originalCdpnRedirectReason	= htolel(originalCdpnRedirectReason);
	msg->data.CallInfoDynamicMessage.lel_lastRedirectingReason	= htolel(lastRedirectingReason);
	memcpy(&msg->data.CallInfoDynamicMessage.dummy, dummy, dummy_len);
	sccp_free(dummy);
	
	//sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) (VERBOSE_PREFIX_3 "%s: Send callinfo(V20) for %s channel %d/%d on line instance %d\n", (device) ? device->id : "(null)", skinny_calltype2str(calltype), callid, callInstance, lineInstance);
	//if ((GLOB(debug) & (DEBUGCAT_CHANNEL | DEBUGCAT_LINE | DEBUGCAT_INDICATE)) != 0) {
	//	iCallInfo.Print2log(ci, "SCCP: (sendCallInfoV16)");
	//	sccp_dump_msg(msg);
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
static void sccp_protocol_sendCallForwardStatus(constDevicePtr device, const sccp_linedevice_t * ld)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	msg->data.ForwardStatMessage.v3.lel_activeForward = (ld->cfwd[SCCP_CFWD_ALL].enabled || ld->cfwd[SCCP_CFWD_BUSY].enabled || ld->cfwd[SCCP_CFWD_NOANSWER].enabled) ? htolel(1) : 0;
	msg->data.ForwardStatMessage.v3.lel_lineNumber = htolel(ld->lineInstance);

	if(ld->cfwd[SCCP_CFWD_ALL].enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardAllActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdallnumber, ld->cfwd[SCCP_CFWD_ALL].number, sizeof(msg->data.ForwardStatMessage.v3.cfwdallnumber));
	} else if(ld->cfwd[SCCP_CFWD_BUSY].enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardBusyActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdbusynumber, ld->cfwd[SCCP_CFWD_BUSY].number, sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
	} else if(ld->cfwd[SCCP_CFWD_NOANSWER].enabled) {
		msg->data.ForwardStatMessage.v3.lel_forwardNoAnswerActive = htolel(1);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdnoanswernumber, ld->cfwd[SCCP_CFWD_NOANSWER].number, sizeof(msg->data.ForwardStatMessage.v3.cfwdnoanswernumber));
	} else {
		msg->data.ForwardStatMessage.v3.lel_forwardAllActive = htolel(0);
		msg->data.ForwardStatMessage.v3.lel_forwardBusyActive = htolel(0);
		msg->data.ForwardStatMessage.v3.lel_forwardNoAnswerActive = htolel(0);
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdallnumber, "", sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdbusynumber, "", sizeof(msg->data.ForwardStatMessage.v3.cfwdbusynumber));
		sccp_copy_string(msg->data.ForwardStatMessage.v3.cfwdnoanswernumber, "", sizeof(msg->data.ForwardStatMessage.v3.cfwdnoanswernumber));
	}
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Call Forward Status Message (V19)
 * \todo need more information about lel_activeForward and lel_forwardAllActive values.
 */
static void sccp_protocol_sendCallForwardStatusV18(constDevicePtr device, const sccp_linedevice_t * ld)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, ForwardStatMessage);
	// activeForward / lel_forwardAllActive =  used 4 before... tcpdump shows 2(enbloc ?) or 8(single keypad ?)
	// msg->data.ForwardStatMessage.v18.lel_activeForward = (ld->cfwdAll.enabled || ld->cfwdBusy.enabled) ? htolel(2) : 0;   // should this be 2 instead ?
	msg->data.ForwardStatMessage.v18.lel_lineNumber = htolel(ld->lineInstance);
	if(ld->cfwd[SCCP_CFWD_ALL].enabled) {
		msg->data.ForwardStatMessage.v18.lel_activeForward = 2;
		msg->data.ForwardStatMessage.v18.lel_forwardAllActive = htolel(2);	// needs more information about the possible values and their meaning // 2 ?
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdallnumber, ld->cfwd[SCCP_CFWD_ALL].number, sizeof(msg->data.ForwardStatMessage.v18.cfwdallnumber));
	} else if(ld->cfwd[SCCP_CFWD_BUSY].enabled) {
		msg->data.ForwardStatMessage.v18.lel_activeForward = 2;
		msg->data.ForwardStatMessage.v18.lel_forwardBusyActive = htolel(2);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdbusynumber, ld->cfwd[SCCP_CFWD_BUSY].number, sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
	} else if(ld->cfwd[SCCP_CFWD_NOANSWER].enabled) {
		msg->data.ForwardStatMessage.v18.lel_activeForward = 2;
		msg->data.ForwardStatMessage.v18.lel_forwardNoAnswerActive = htolel(2);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdnoanswernumber, ld->cfwd[SCCP_CFWD_NOANSWER].number, sizeof(msg->data.ForwardStatMessage.v18.cfwdnoanswernumber));
	} else {
		msg->data.ForwardStatMessage.v18.lel_activeForward = 0;
		msg->data.ForwardStatMessage.v18.lel_forwardAllActive = htolel(0);
		msg->data.ForwardStatMessage.v18.lel_forwardBusyActive = htolel(0);
		msg->data.ForwardStatMessage.v18.lel_forwardNoAnswerActive = htolel(0);
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdallnumber, "", sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdbusynumber, "", sizeof(msg->data.ForwardStatMessage.v18.cfwdbusynumber));
		sccp_copy_string(msg->data.ForwardStatMessage.v18.cfwdnoanswernumber, "", sizeof(msg->data.ForwardStatMessage.v18.cfwdnoanswernumber));
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
	msg->data.RegisterAckMessage.protocolFeatures.protocolVersion = device->protocol->version;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[0] = 0x00;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[1] = 0x00;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[2] = 0x00;

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

	msg->data.RegisterAckMessage.protocolFeatures.protocolVersion = device->protocol->version;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[0] = 0x20;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[1] = 0xF1;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[2] = 0xFE;

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

	msg->data.RegisterAckMessage.protocolFeatures.protocolVersion = device->protocol->version;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[0] = 0x20;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[1] = 0xF1;
	msg->data.RegisterAckMessage.protocolFeatures.phoneFeatures[2] = 0xFF;

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
	msg->data.OpenReceiveChannel.v3.lel_codecType = htolel(channel->rtp.audio.reception.format);
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
	msg->data.OpenReceiveChannel.v17.lel_codecType = htolel(channel->rtp.audio.reception.format);
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
		msg->data.OpenReceiveChannel.v17.lel_requestedIpAddrType = htolel(SKINNY_IPADDR_IPV6);				//for ipv6 this value have to me > 0, lel_ipv46 doesn't matter
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
	msg->data.OpenReceiveChannel.v22.lel_codecType = htolel(channel->rtp.audio.reception.format);
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
		msg->data.OpenReceiveChannel.v22.lel_requestedIpAddrType = htolel(SKINNY_IPADDR_IPV6);
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
static void sccp_protocol_sendOpenMultiMediaChannelV3(constDevicePtr device, constChannelPtr channel, skinny_codec_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(msg->data.OpenMultiMediaChannelMessage.v3));

	msg->data.OpenMultiMediaChannelMessage.v3.lel_conferenceID = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_passThruPartyID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_codecType = htolel(skinnyFormat);
	//msg->data.OpenMultiMediaChannelMessage.v3.lel_codecType = htolel(channel->rtp.video.reception.format);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_lineInstance = htolel(lineInstance);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_callReference = htolel(channel->callid);
	
	msg->data.OpenMultiMediaChannelMessage.v3.payloadType.lel_payload_rfc_number = htolel(0);
	msg->data.OpenMultiMediaChannelMessage.v3.payloadType.lel_payloadType = htolel(payloadType);
	
	msg->data.OpenMultiMediaChannelMessage.v3.lel_isConferenceCreator = htolel(0);
	
	skinny_OpenMultiMediaReceiveChannelUnion_t *capability = &(msg->data.OpenMultiMediaChannelMessage.v3.capability);
	{
		capability->vidParameters.lel_bitRate = htolel(bitRate);
		capability->vidParameters.lel_pictureFormatCount = htolel(1);
		
		capability->vidParameters.pictureFormat[0].format = htolel(4);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[0].mpi = htolel(1);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[1].format = htolel(2);
		capability->vidParameters.pictureFormat[1].mpi = htolel(1);
		capability->vidParameters.pictureFormat[2].format = htolel(1);
		capability->vidParameters.pictureFormat[2].mpi = htolel(1);
		capability->vidParameters.pictureFormat[3].format = htolel(0);
		capability->vidParameters.pictureFormat[3].mpi = htolel(1);
		//capability->vidParameters.pictureFormat[4].format = htolel(0);
		//capability->vidParameters.pictureFormat[4].mpi = htolel(0);
		
		capability->vidParameters.lel_confServiceNum = htolel(0);

		skinny_ChannelVideoParametersUnion_t *channelVideoParams = &(capability->vidParameters.capability);
		{
			if (skinnyFormat == SKINNY_CODEC_H261) {
				channelVideoParams->h261.lel_temporalSpatialTradeOffCapability = htolel(1);	// ??
				channelVideoParams->h261.lel_stillImageTransmission = htolel(0);		// ??
			} else if (skinnyFormat == SKINNY_CODEC_H263) {						//
				channelVideoParams->h263.lel_capabilityBitfield = htolel(0);			// ??
				channelVideoParams->h263.lel_annexNandWFutureUse = htolel(0);			// ?? 
			} else if (skinnyFormat == SKINNY_CODEC_H263P) {					// H263P / aka:Vieo / H263-1998
				//CIF=1,QCIF=1
				channelVideoParams->h263P.lel_modelNumber = htolel(0);				// ??
				channelVideoParams->h263P.lel_bandwidth = htolel(0);				// ?? 90000
			} else if (skinnyFormat == SKINNY_CODEC_H264) {						// aka: MPEG4-AVC
				channelVideoParams->h264.lel_profile = htolel(64);				// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_level = htolel(43);				// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxMBPS = htolel(40500);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxFS = htolel(1620);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxDPB = htolel(8100);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxBRandCPB = htolel(10000);			// should be taken from UpdateCapabilitiesMessage
			} else {
				// error
			}
		}
	}
	// Leave empty for now, until we find out more about encryption
	//msg->data.OpenMultiMediaChannelMessage.v3.RxEncryptionInfo = {0};
	msg->data.OpenMultiMediaChannelMessage.v3.lel_streamPassThroughID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v3.lel_associatedStreamID = htolel(channel->callid);		// We should use a random number and link it up 
														// with the MultiMediaTransmission
	//sccp_dump_msg(msg);
	sccp_dev_send(device, msg);
}
/*!
 * \brief Send Open MultiMediaChannel Message (V12)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV12(constDevicePtr device, constChannelPtr channel, skinny_codec_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(msg->data.OpenMultiMediaChannelMessage.v12));

	msg->data.OpenMultiMediaChannelMessage.v12.lel_conferenceID = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v12.lel_passThruPartyID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v12.lel_codecType = htolel(skinnyFormat);
	//msg->data.OpenMultiMediaChannelMessage.v12.lel_codecType = htolel(channel->rtp.video.reception.format);
	msg->data.OpenMultiMediaChannelMessage.v12.lel_lineInstance = htolel(lineInstance);
	msg->data.OpenMultiMediaChannelMessage.v12.lel_callReference = htolel(channel->callid);
	
	msg->data.OpenMultiMediaChannelMessage.v12.payloadType.lel_payload_rfc_number = htolel(0);
	msg->data.OpenMultiMediaChannelMessage.v12.payloadType.lel_payloadType = htolel(payloadType);
	
	msg->data.OpenMultiMediaChannelMessage.v12.lel_isConferenceCreator = htolel(0);
	
	skinny_OpenMultiMediaReceiveChannelUnion_t *capability = &(msg->data.OpenMultiMediaChannelMessage.v12.capability);
	{
		capability->vidParameters.lel_bitRate = htolel(bitRate);
		capability->vidParameters.lel_pictureFormatCount = htolel(1);
		
		capability->vidParameters.pictureFormat[0].format = htolel(4);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[0].mpi = htolel(1);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[1].format = htolel(2);
		capability->vidParameters.pictureFormat[1].mpi = htolel(1);
		capability->vidParameters.pictureFormat[2].format = htolel(1);
		capability->vidParameters.pictureFormat[2].mpi = htolel(1);
		capability->vidParameters.pictureFormat[3].format = htolel(0);
		capability->vidParameters.pictureFormat[3].mpi = htolel(1);
		//capability->vidParameters.pictureFormat[4].format = htolel(0);
		//capability->vidParameters.pictureFormat[4].mpi = htolel(0);
		
		capability->vidParameters.lel_confServiceNum = htolel(0);

		skinny_ChannelVideoParametersUnion_t *channelVideoParams = &(capability->vidParameters.capability);
		{
			if (skinnyFormat == SKINNY_CODEC_H261) {
				channelVideoParams->h261.lel_temporalSpatialTradeOffCapability = htolel(1);	// ??
				channelVideoParams->h261.lel_stillImageTransmission = htolel(0);		// ??
			} else if (skinnyFormat == SKINNY_CODEC_H263) {						//
				channelVideoParams->h263.lel_capabilityBitfield = htolel(0);			// ??
				channelVideoParams->h263.lel_annexNandWFutureUse = htolel(0);			// ?? 
			} else if (skinnyFormat == SKINNY_CODEC_H263P) {					// H263P / aka:Vieo / H263-1998
				//CIF=1,QCIF=1
				channelVideoParams->h263P.lel_modelNumber = htolel(0);				// ??
				channelVideoParams->h263P.lel_bandwidth = htolel(0);				// ?? 90000
			} else if (skinnyFormat == SKINNY_CODEC_H264) {						// aka: MPEG4-AVC
				channelVideoParams->h264.lel_profile = htolel(64);				// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_level = htolel(43);				// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxMBPS = htolel(40500);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxFS = htolel(1620);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxDPB = htolel(8100);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxBRandCPB = htolel(10000);			// should be taken from UpdateCapabilitiesMessage
			} else {
				// error
			}
		}
	}
	// Leave empty for now, until we find out more about encryption
	//msg->data.OpenMultiMediaChannelMessage.v12.RxEncryptionInfo = {0};
	msg->data.OpenMultiMediaChannelMessage.v12.lel_streamPassThroughID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v12.lel_associatedStreamID = htolel(channel->callid);		// We should use a random number and link it up 
														// with the MultiMediaTransmission

	/* Source Ip Address */
	struct sockaddr_storage sas;
	if (device->directrtp) {
		sccp_rtp_getPeer(&channel->rtp.video, &sas);
	} else {
		sccp_rtp_getUs(&channel->rtp.video, &sas);
	}
	sccp_netsock_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;
		memcpy(&msg->data.OpenMultiMediaChannelMessage.v12.bel_sourceIpAddr, &in->sin_addr, 4);
	} else {
		// error
	}
	msg->data.OpenMultiMediaChannelMessage.v12.lel_sourcePortNumber=htolel(sccp_netsock_getPort(&sas));
	
	//sccp_dump_msg(msg);
	sccp_dev_send(device, msg);
}
/*!
 * \brief Send Open MultiMediaChannel Message (V17)
 */
static void sccp_protocol_sendOpenMultiMediaChannelV17(constDevicePtr device, constChannelPtr channel, skinny_codec_t skinnyFormat, int payloadType, uint8_t lineInstance, int bitRate)
{
	sccp_msg_t *msg = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(msg->data.OpenMultiMediaChannelMessage.v17));

	msg->data.OpenMultiMediaChannelMessage.v17.lel_conferenceID = htolel(channel->callid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_passThruPartyID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_codecType = htolel(skinnyFormat);
	//msg->data.OpenMultiMediaChannelMessage.v17.lel_codecType = htolel(channel->rtp.video.reception.format);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_lineInstance = htolel(lineInstance);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_callReference = htolel(channel->callid);
	
	msg->data.OpenMultiMediaChannelMessage.v17.payloadType.lel_payload_rfc_number = htolel(0);
	msg->data.OpenMultiMediaChannelMessage.v17.payloadType.lel_payloadType = htolel(payloadType);
	
	msg->data.OpenMultiMediaChannelMessage.v17.lel_isConferenceCreator = htolel(0);
	

	skinny_OpenMultiMediaReceiveChannelUnion_t *capability = &(msg->data.OpenMultiMediaChannelMessage.v17.capability);
	{
		capability->vidParameters.lel_bitRate = htolel(bitRate);
		capability->vidParameters.lel_pictureFormatCount = htolel(1);
		
		capability->vidParameters.pictureFormat[0].format = htolel(4);					// should be taken from UpdateCapabilitiesMessage
/*
	MPI = Minimum Picture interval. 1=means 29.7 frames, 2=halfs that to 14.9.
	If the receiver does not specify the picture size/MPI optional parameter, then it SHOULD be ready to receive QCIF resolution with MPI=1.
*/
		capability->vidParameters.pictureFormat[0].format = htolel(4);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[0].mpi = htolel(1);					// should be taken from UpdateCapabilitiesMessage
		capability->vidParameters.pictureFormat[1].format = htolel(2);
		capability->vidParameters.pictureFormat[1].mpi = htolel(1);
		capability->vidParameters.pictureFormat[2].format = htolel(1);
		capability->vidParameters.pictureFormat[2].mpi = htolel(1);
		capability->vidParameters.pictureFormat[3].format = htolel(0);
		capability->vidParameters.pictureFormat[3].mpi = htolel(1);
		//capability->vidParameters.pictureFormat[4].format = htolel(0);
		//capability->vidParameters.pictureFormat[4].mpi = htolel(0);
		
		capability->vidParameters.lel_confServiceNum = htolel(0);

		skinny_ChannelVideoParametersUnion_t *channelVideoParams = &(capability->vidParameters.capability);
		{
			if (skinnyFormat == SKINNY_CODEC_H261) {
				channelVideoParams->h261.lel_temporalSpatialTradeOffCapability = htolel(1);	// ??
				channelVideoParams->h261.lel_stillImageTransmission = htolel(0);		// ??
			} else if (skinnyFormat == SKINNY_CODEC_H263) {						// https://tools.ietf.org/html/rfc4629
				channelVideoParams->h263.lel_capabilityBitfield = htolel(0);			// ??
				channelVideoParams->h263.lel_annexNandWFutureUse = htolel(0);			// ?? 
			} else if (skinnyFormat == SKINNY_CODEC_H263P) {					// H263P / aka:Vieo / H263-1998
				//CIF=1,QCIF=1
				channelVideoParams->h263P.lel_modelNumber = htolel(0);				// ??
				channelVideoParams->h263P.lel_bandwidth = htolel(0);				// ?? 90000
			} else if (skinnyFormat == SKINNY_CODEC_H264) {						// aka: MPEG4-AVC
				/*
				      PROFILE:  profile number, in the range 0 through 10,
				      specifying the supported H.263 annexes/subparts based on H.263
				      annex X [H263].  The annexes supported in each profile are listed
				      in table X.1 of H.263 annex X.  If no profile or H.263 annex is
				      specified, then the stream is Baseline H.263 (profile 0 of H.263
				      annex X).
				*/
				channelVideoParams->h264.lel_profile = htolel(64);				// should be taken from UpdateCapabilitiesMessage
				/*
				      LEVEL:  Level of bitstream operation, in the range 0 through 100,
				      specifying the level of computational complexity of the decoding
				      process.  The level are described in table X.2 of H.263 annex X.
				*/
				channelVideoParams->h264.lel_level = htolel(43);				// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxMBPS = htolel(40500);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxFS = htolel(1620);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxDPB = htolel(8100);			// should be taken from UpdateCapabilitiesMessage
				channelVideoParams->h264.lel_customMaxBRandCPB = htolel(10000);			// should be taken from UpdateCapabilitiesMessage
			} else {
				// error
			}
		}
	}
	// Leave empty for now, until we find out more about encryption
	//msg->data.OpenMultiMediaChannelMessage.v17.RxEncryptionInfo = {0};
	msg->data.OpenMultiMediaChannelMessage.v17.lel_streamPassThroughID = htolel(channel->passthrupartyid);
	msg->data.OpenMultiMediaChannelMessage.v17.lel_associatedStreamID = htolel(channel->callid);		// We should use a random number and link it up 
														// with the MultiMediaTransmission

	/* Source Ip Address */
	struct sockaddr_storage sas;
	if (device->directrtp) {
		sccp_rtp_getPeer(&channel->rtp.video, &sas);
	} else {
		sccp_rtp_getUs(&channel->rtp.video, &sas);
	}
	sccp_netsock_ipv4_mapped(&sas, &sas);

	if (sas.ss_family == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &sas;
		msg->data.OpenMultiMediaChannelMessage.v17.sourceIpAddr.lel_ipAddrType = htolel(SKINNY_IPADDR_IPV6);
		
		// Also take into account that we could be using IPv46 (ie: both of them)
		if (sccp_netsock_is_mapped_IPv4(&sas)) {
			msg->data.OpenMultiMediaChannelMessage.v17.lel_requestedIpAddrType = htolel(SKINNY_IPADDR_IPV46);
		} else {
			msg->data.OpenMultiMediaChannelMessage.v17.lel_requestedIpAddrType = htolel(SKINNY_IPADDR_IPV6);
		}
		memcpy(&msg->data.OpenMultiMediaChannelMessage.v17.sourceIpAddr.stationIpAddr, &in6->sin6_addr, 16);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *) &sas;
		msg->data.OpenMultiMediaChannelMessage.v17.sourceIpAddr.lel_ipAddrType = htolel(SKINNY_IPADDR_IPV4);
		msg->data.OpenMultiMediaChannelMessage.v17.lel_requestedIpAddrType = htolel(SKINNY_IPADDR_IPV4);
		memcpy(&msg->data.OpenMultiMediaChannelMessage.v17.sourceIpAddr.stationIpAddr, &in->sin_addr, 4);
	}
	msg->data.OpenMultiMediaChannelMessage.v17.lel_sourcePortNumber=htolel(sccp_netsock_getPort(&sas));
	
	//sccp_dump_msg(msg);
	sccp_dev_send(device, msg);
}

/*!
 * \brief Send Start Media Transmission (V3)
 */
static void sccp_protocol_sendStartMediaTransmissionV3(constDevicePtr device, constChannelPtr channel)
{
	sccp_msg_t *msg = sccp_build_packet(StartMediaTransmission, sizeof(msg->data.StartMediaTransmission.v3));

        uint framing = iPbx.get_codec_framing ? iPbx.get_codec_framing(channel) : 20;
        uint dtmf_payload_code = iPbx.get_dtmf_payload_code ? iPbx.get_dtmf_payload_code(channel) : 101;

	msg->data.StartMediaTransmission.v3.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v3.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v3.lel_millisecondPacketSize = htolel(framing);
	//msg->data.StartMediaTransmission.v3.lel_payloadType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v3.lel_codecType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v3.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v3.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v3.lel_maxFramesPerPacket = htolel(0);
	msg->data.StartMediaTransmission.v3.lel_RFC2833Type = htolel(dtmf_payload_code);
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

        uint framing = iPbx.get_codec_framing ? iPbx.get_codec_framing(channel) : 20;
        uint dtmf_payload_code = iPbx.get_dtmf_payload_code ? iPbx.get_dtmf_payload_code(channel) : 101;
        
	msg->data.StartMediaTransmission.v17.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v17.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v17.lel_millisecondPacketSize = htolel(framing);
	//msg->data.StartMediaTransmission.v17.lel_payloadType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v17.lel_codecType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v17.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v17.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v17.lel_maxFramesPerPacket = htolel(0);
	msg->data.StartMediaTransmission.v17.lel_RFC2833Type = htolel(dtmf_payload_code);
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

        uint framing = iPbx.get_codec_framing ? iPbx.get_codec_framing(channel) : 20;
        uint dtmf_payload_code = iPbx.get_dtmf_payload_code ? iPbx.get_dtmf_payload_code(channel) : 101;

	msg->data.StartMediaTransmission.v22.lel_conferenceId = htolel(channel->callid);
	msg->data.StartMediaTransmission.v22.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.StartMediaTransmission.v22.lel_callReference = htolel(channel->callid);
	msg->data.StartMediaTransmission.v22.lel_millisecondPacketSize = htolel(framing);
	//msg->data.StartMediaTransmission.v22.lel_payloadType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v22.lel_codecType = htolel(channel->rtp.audio.transmission.format);
	msg->data.StartMediaTransmission.v22.lel_precedenceValue = htolel((uint32_t)device->audio_tos);
	msg->data.StartMediaTransmission.v22.lel_ssValue = htolel(channel->line->silencesuppression);		// Silence supression
	msg->data.StartMediaTransmission.v22.lel_maxFramesPerPacket = htolel(0);
	msg->data.StartMediaTransmission.v22.lel_RFC2833Type = htolel(dtmf_payload_code);
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
	//uint payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, video->transmission.format);

	msg->data.StartMultiMediaTransmission.v3.lel_conferenceID = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v3.lel_passThruPartyId = htolel(channel->passthrupartyid);
	//msg->data.StartMultiMediaTransmission.v3.lel_payloadCapability = htolel(channel->rtp.video.transmission.format);
	msg->data.StartMultiMediaTransmission.v3.lel_codecType = htolel(channel->rtp.video.transmission.format);
	msg->data.StartMultiMediaTransmission.v3.lel_callReference = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v3.lel_payload_rfc_number = htolel(0);
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

	//sccp_dump_msg(msg);
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
	//msg->data.StartMultiMediaTransmission.v17.lel_payloadCapability = htolel(channel->rtp.video.transmission.format);
	msg->data.StartMultiMediaTransmission.v17.lel_codecType = htolel(channel->rtp.video.transmission.format);
	msg->data.StartMultiMediaTransmission.v17.lel_callReference = htolel(channel->callid);
	msg->data.StartMultiMediaTransmission.v17.lel_payload_rfc_number = htolel(0);
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
	//sccp_dump_msg(msg);
	sccp_dev_send(device, msg);
}

/* fastPictureUpdate */
static void sccp_protocol_sendMultiMediaCommand(constDevicePtr device, constChannelPtr channel, skinny_miscCommandType_t command)
{
	sccp_msg_t *msg = NULL;

	REQ(msg, MiscellaneousCommandMessage);

	msg->data.MiscellaneousCommandMessage.lel_conferenceId = htolel(channel->callid);
	msg->data.MiscellaneousCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	msg->data.MiscellaneousCommandMessage.lel_callReference = htolel(channel->callid);
	msg->data.MiscellaneousCommandMessage.lel_miscCommandType = htolel(command);

	sccp_dev_send(device, msg);
}

/* done - fastPictureUpdate */

/* sendUserToDeviceData Message */

/*!
 * \brief Send User To Device Message (V1)
 */
static void sccp_protocol_sendUserToDeviceDataVersion1Message(constDevicePtr device, uint32_t appID, uint32_t lineInstance, uint32_t callReference, uint32_t transactionID, const char *xmlData, uint8_t priority)
{
	int data_len = strlen(xmlData);
	int msg_len = 0;
	int hdr_len = 0;

	if (device->protocolversion > 17) {
		int segment = 0;

		sccp_msg_t *msg = NULL;
		hdr_len = sizeof(msg->data.UserToDeviceDataVersion1Message);
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
			msg = sccp_build_packet(UserToDeviceDataVersion1Message, hdr_len + msg_len);
			if (!msg) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				return;
			}
			msg->data.UserToDeviceDataVersion1Message.lel_appID = htolel(appID);
			msg->data.UserToDeviceDataVersion1Message.lel_lineInstance = htolel(lineInstance);
			msg->data.UserToDeviceDataVersion1Message.lel_callReference = htolel(callReference);
			msg->data.UserToDeviceDataVersion1Message.lel_transactionID = htolel(transactionID);
			msg->data.UserToDeviceDataVersion1Message.lel_displayPriority = htolel(priority);
			msg->data.UserToDeviceDataVersion1Message.lel_dataLength = htolel(msg_len);
			msg->data.UserToDeviceDataVersion1Message.lel_sequenceFlag = htolel(Sequence);
			msg->data.UserToDeviceDataVersion1Message.lel_conferenceID = htolel(callReference);
			msg->data.UserToDeviceDataVersion1Message.lel_appInstanceID = htolel(appID);
			msg->data.UserToDeviceDataVersion1Message.lel_routing = htolel(1);
			if (Sequence == 0x0000) {
				Sequence = 0x0001;
			}

			if (msg_len) {
				memcpy(&msg->data.UserToDeviceDataVersion1Message.data, xmlData + xmlDataStart, msg_len);
				xmlDataStart += msg_len;
			}

			sccp_dump_msg(msg);
			sccp_dev_send(device, msg);

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
		sccp_dump_msg(msg);
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

/* sendLineStatResponse Message */
/*!
 * \brief Send Start Line State Response Message (V3)
 */
static void sccp_protocol_sendLineStatRespV3(constDevicePtr d, uint32_t lineNumber, char *dirNumber, char *fullyQualifiedDisplayName, char *displayName)
{
	sccp_msg_t *msg = NULL;
	REQ(msg, LineStatMessage);
	msg->data.LineStatMessage.lel_lineNumber = htolel(lineNumber);
	d->copyStr2Locale(d, msg->data.LineStatMessage.lineDirNumber, dirNumber, sizeof(msg->data.LineStatMessage.lineDirNumber));
	d->copyStr2Locale(d, msg->data.LineStatMessage.lineFullyQualifiedDisplayName, fullyQualifiedDisplayName, sizeof(msg->data.LineStatMessage.lineFullyQualifiedDisplayName));
	d->copyStr2Locale(d, msg->data.LineStatMessage.lineDisplayName, displayName, sizeof(msg->data.LineStatMessage.lineDisplayName));

	//Bit-field: 1-Original Dialed 2-Redirected Dialed, 4-Calling line ID, 8-Calling name ID
	//msg->data.LineStatMessage.lel_lineDisplayOptions = 0x01 & 0x08;
	msg->data.LineStatMessage.lel_lineDisplayOptions = htolel(15);		// value : 0 or 15
	sccp_dev_send(d, msg);
}

/*!
 * \brief Send Start Line State Response Message (V17)
 */
static void sccp_protocol_sendLineStatRespV17(constDevicePtr d, uint32_t lineNumber, char *dirNumber, char *fullyQualifiedDisplayName, char *displayName)
{
	int dirNumLen = dirNumber ? sccp_strlen(dirNumber): 0;
	int fqdnLen = fullyQualifiedDisplayName ? sccp_strlen(fullyQualifiedDisplayName): 0;
	int displayNameLen = displayName ? sccp_strlen(displayName): 0;
	int dummyLen = dirNumLen + fqdnLen + displayNameLen;

 	int pktLen = SCCP_PACKET_HEADER + dummyLen;
	sccp_msg_t *msg = sccp_build_packet(LineStatDynamicMessage, pktLen);
	msg->data.LineStatDynamicMessage.lel_lineNumber = htolel(lineNumber);

	if (dummyLen) {
		char *dummyPtr = msg->data.LineStatDynamicMessage.dummy;
		d->copyStr2Locale(d, dummyPtr, dirNumber, dirNumLen+1);
		dummyPtr += dirNumLen + 1;
		d->copyStr2Locale(d, dummyPtr, fullyQualifiedDisplayName, fqdnLen+1);	
		dummyPtr += fqdnLen + 1;
		d->copyStr2Locale(d, dummyPtr, displayName, displayNameLen+1);
		dummyPtr += displayNameLen + 1;
	}

	//Bit-field: 1-Original Dialed 2-Redirected Dialed, 4-Calling line ID, 8-Calling name ID
	//int lineDisplayOptions = 0x01 & 0x08;
	//int lineDisplayOptions = htolel(15);
	//msg->data.LineStatDynamicMessage.lel_lineDisplayOptions = htolel(lineDisplayOptions);
	sccp_dev_send(d, msg);
}

/*
static void sccp_protocol_sendLineStatRespV17(constDevicePtr d, uint32_t lineNumber, char *dirNumber, char *fullyQualifiedDisplayName, char *displayName)
{
	sccp_msg_t *msg = NULL;
	REQ(msg, LineStatDynamicMessage);
	msg->data.LineStatDynamicMessage.lel_lineNumber = htolel(lineNumber);
	d->copyStr2Locale(d, msg->data.LineStatDynamicMessage.lineDirNumber, dirNumber, sizeof(msg->data.LineStatDynamicMessage.lineDirNumber));
	d->copyStr2Locale(d, msg->data.LineStatDynamicMessage.lineFullyQualifiedDisplayName, fullyQualifiedDisplayName, sizeof(msg->data.LineStatDynamicMessage.lineFullyQualifiedDisplayName));
	d->copyStr2Locale(d, msg->data.LineStatDynamicMessage.lineTextLabel, displayName, sizeof(msg->data.LineStatDynamicMessage.lineTextLabel));

	//Bit-field: 1-Original Dialed 2-Redirected Dialed, 4-Calling line ID, 8-Calling name ID
	//msg->data.LineStatDynamicMessage.lel_lineDisplayOptions = 0x01 & 0x08;
	msg->data.LineStatDynamicMessage.lel_lineDisplayOptions = htolel(15);
	sccp_dev_send(d, msg);
}
*/
/* done - sendLineStat */

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
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 3, TimeDateReqMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV3, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},	/* default impl */
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 5, TimeDateReqMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendStaticDisplayprompt, sccp_protocol_sendStaticDisplayNotify, sccp_protocol_sendStaticDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 8, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 9, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 10, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 11, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 15, TimeDateReqMessage, sccp_protocol_sendCallInfoV7, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV12,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 16, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3, sccp_protocol_sendOpenMultiMediaChannelV12,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 17, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17, 
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 18, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV17, sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV3},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 19, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 20, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 21, TimeDateReqMessage, sccp_protocol_sendCallInfoV16, sccp_protocol_sendDialedNumberV18, sccp_protocol_sendRegisterAckV11, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatusV18, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV17,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17, sccp_protocol_sendStartMediaTransmissionV17, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17, sccp_protocol_parseStartMediaTransmissionAckV17, sccp_protocol_parseStartMultiMediaTransmissionAckV17, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
	&(sccp_deviceProtocol_t) {SCCP_PROTOCOL, 22, TimeDateReqMessage, sccp_protocol_sendCallInfoV16,sccp_protocol_sendDialedNumberV18,sccp_protocol_sendRegisterAckV11,sccp_protocol_sendDynamicDisplayprompt,sccp_protocol_sendDynamicDisplayNotify,sccp_protocol_sendDynamicDisplayPriNotify,sccp_protocol_sendCallForwardStatusV18,sccp_protocol_sendUserToDeviceDataVersion1Message,sccp_protocol_sendMultiMediaCommand,sccp_protocol_sendOpenReceiveChannelv22,
				  sccp_protocol_sendOpenMultiMediaChannelV17,
				  sccp_protocol_sendStartMultiMediaTransmissionV17,sccp_protocol_sendStartMediaTransmissionv22,sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV17,
				  sccp_protocol_parseOpenReceiveChannelAckV17,sccp_protocol_parseOpenMultiMediaReceiveChannelAckV17,sccp_protocol_parseStartMediaTransmissionAckV17,sccp_protocol_parseStartMultiMediaTransmissionAckV17,sccp_protocol_parseEnblocCallV22, sccp_protocol_parsePortResponseV19},
};

/*! 
 * \brief SPCP Protocol Version to Message Mapping
 */
static const sccp_deviceProtocol_t *spcpProtocolDefinition[] = {
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 0, RegisterAvailableLinesMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV3, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV3, sccp_protocol_parsePortResponseV3},
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&(sccp_deviceProtocol_t) {SPCP_PROTOCOL, 8, RegisterAvailableLinesMessage, sccp_protocol_sendCallInfoV3, sccp_protocol_sendDialedNumberV3, sccp_protocol_sendRegisterAckV4, sccp_protocol_sendDynamicDisplayprompt, sccp_protocol_sendDynamicDisplayNotify, sccp_protocol_sendDynamicDisplayPriNotify, sccp_protocol_sendCallForwardStatus, sccp_protocol_sendUserToDeviceDataVersion1Message, sccp_protocol_sendMultiMediaCommand, sccp_protocol_sendOpenReceiveChannelV3,
				  sccp_protocol_sendOpenMultiMediaChannelV3,
				  sccp_protocol_sendStartMultiMediaTransmissionV3, sccp_protocol_sendStartMediaTransmissionV3, sccp_protocol_sendConnectionStatisticsReqV19, sccp_protocol_sendPortRequest,sccp_protocol_sendPortClose, sccp_protocol_sendLineStatRespV3,
				  sccp_protocol_parseOpenReceiveChannelAckV3, sccp_protocol_parseOpenMultiMediaReceiveChannelAckV3, sccp_protocol_parseStartMediaTransmissionAckV3, sccp_protocol_parseStartMultiMediaTransmissionAckV3, sccp_protocol_parseEnblocCallV17, sccp_protocol_parsePortResponseV19},
};

/*! 
 * \brief Get Maximum Supported Version Number by Protocol Type
 */
uint8_t __CONST__ sccp_protocol_getMaxSupportedVersionNumber(int type)
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
	uint8_t version = device->protocolversion;
	const sccp_deviceProtocol_t ** protocolDef = NULL;
	size_t protocolArraySize = 0;
	uint8_t returnProtocol = 0;

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

	for(uint8_t i = (protocolArraySize - 1); i > 0; i--) {
		if (protocolDef[i] != NULL && version >= protocolDef[i]->version) {
			sccp_log(DEBUGCAT_DEVICE) (VERBOSE_PREFIX_3 "%s: found protocol version '%d' at %d\n", protocolDef[i]->type == SCCP_PROTOCOL ? "SCCP" : "SPCP", protocolDef[i]->version, i);
			returnProtocol = i;
			break;
		}
	}

	return protocolDef[returnProtocol];
}

const char * const __CONST__ skinny_keymode2longstr(skinny_keymode_t keymode)
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
		case KEYMODE_CALLCOMPLETION:
			return "CallCompletion on offer/active";
		case KEYMODE_CALLBACK:
			return "Callback available";
		default:
			return "Unknown KeyMode";
	}
}

const struct messageinfo sccp_messageinfo[] = {
	/* clang-format off */
	/* DEV -> PBX */
	[KeepAliveMessage] = { KeepAliveMessage, "Keep Alive Message", offsize(sccp_data_t, StationKeepAliveMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[RegisterMessage] = { RegisterMessage, "Register Message", offsize(sccp_data_t, RegisterMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[IpPortMessage] = { IpPortMessage, "Ip-Port Message", offsize(sccp_data_t, IpPortMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[KeypadButtonMessage] = { KeypadButtonMessage, "Keypad Button Message", offsize(sccp_data_t, KeypadButtonMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	/**/[EnblocCallMessage] = { EnblocCallMessage, "Enbloc Call Message", offsize(sccp_data_t, EnblocCallMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[StimulusMessage] = { StimulusMessage, "Stimulus Message", offsize(sccp_data_t, StimulusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[OffHookMessage] = { OffHookMessage, "Off-Hook Message", offsize(sccp_data_t, OffHookMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[OnHookMessage] = { OnHookMessage, "On-Hook Message", offsize(sccp_data_t, OnHookMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[HookFlashMessage] = { HookFlashMessage, "Hook-Flash Message", offsize(sccp_data_t, HookFlashMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[ForwardStatReqMessage] = { ForwardStatReqMessage, "Forward State Request", offsize(sccp_data_t, ForwardStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[SpeedDialStatReqMessage] = { SpeedDialStatReqMessage, "Speed-Dial State Request", offsize(sccp_data_t, SpeedDialStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[LineStatReqMessage] = { LineStatReqMessage, "Line State Request", offsize(sccp_data_t, LineStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[ConfigStatReqMessage] = { ConfigStatReqMessage, "Config State Request", offsize(sccp_data_t, ConfigStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	/**/[TimeDateReqMessage] = { TimeDateReqMessage, "Time Date Request", offsize(sccp_data_t, TimeDateReqMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[ButtonTemplateReqMessage] = { ButtonTemplateReqMessage, "Button Template Request", offsize(sccp_data_t, ButtonTemplateReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[VersionReqMessage] = { VersionReqMessage, "Version Request", offsize(sccp_data_t, VersionReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[CapabilitiesResMessage] = { CapabilitiesResMessage, "Capabilities Response Message", offsize(sccp_data_t, CapabilitiesResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[MediaPortListMessage] = { MediaPortListMessage, "Media Port List Message", offsize(sccp_data_t, MediaPortListMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[ServerReqMessage] = { ServerReqMessage, "Server Request", offsize(sccp_data_t, ServerReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[AlarmMessage] = { AlarmMessage, "Alarm Message", offsize(sccp_data_t, AlarmMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[MulticastMediaReceptionAck] = { MulticastMediaReceptionAck, "Multicast Media Reception Acknowledge", offsize(sccp_data_t, MulticastMediaReceptionAck), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[OpenReceiveChannelAck] = { OpenReceiveChannelAck, "Open Receive Channel Acknowledge", offsize(sccp_data_t, OpenReceiveChannelAck), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	/*[ConnectionStatisticsRes] = { ConnectionStatisticsRes, "Connection Statistics Response", offsize(sccp_data_t, ConnectionStatisticsRes), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },*/
	[ConnectionStatisticsRes] = { ConnectionStatisticsRes, "Connection Statistics Response", offsize(sccp_data_t, ConnectionStatisticsRes), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[OffHookMessageWithCallingPartyMessage] = { OffHookMessageWithCallingPartyMessage, "Off-Hook With Cgpn Message", offsize(sccp_data_t, OffHookMessageWithCallingPartyMessage), SKINNY_MSGTYPE_EVENT,
						    SKINNY_MSGDIRECTION_DEV2PBX },
	[SoftKeySetReqMessage] = { SoftKeySetReqMessage, "SoftKey Set Request", offsize(sccp_data_t, SoftKeySetReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[SoftKeyEventMessage] = { SoftKeyEventMessage, "SoftKey Event Message", offsize(sccp_data_t, SoftKeyEventMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[UnregisterMessage] = { UnregisterMessage, "Unregister Message", offsize(sccp_data_t, UnregisterMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[SoftKeyTemplateReqMessage] = { SoftKeyTemplateReqMessage, "SoftKey Template Request", offsize(sccp_data_t, SoftKeyTemplateReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[RegisterTokenRequest] = { RegisterTokenRequest, "Register Token Request", offsize(sccp_data_t, RegisterTokenRequest), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	/**/[MediaTransmissionFailure] = { MediaTransmissionFailure, "Media Transmission Failure", offsize(sccp_data_t, MediaTransmissionFailure), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[HeadsetStatusMessage] = { HeadsetStatusMessage, "Headset Status Message", offsize(sccp_data_t, HeadsetStatusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[MediaResourceNotification] = { MediaResourceNotification, "Media Resource Notification", offsize(sccp_data_t, MediaResourceNotification), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[RegisterAvailableLinesMessage] = { RegisterAvailableLinesMessage, "Register Available Lines Message", offsize(sccp_data_t, RegisterAvailableLinesMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[DeviceToUserDataMessage] = { DeviceToUserDataMessage, "Device To User Data Message", offsize(sccp_data_t, DeviceToUserDataMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[DeviceToUserDataResponseMessage] = { DeviceToUserDataResponseMessage, "Device To User Data Response", offsize(sccp_data_t, DeviceToUserDataResponseMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[UpdateCapabilitiesMessage] = { UpdateCapabilitiesMessage, "Update Capabilities Message", offsize(sccp_data_t, UpdateCapabilitiesMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[OpenMultiMediaReceiveChannelAckMessage] = { OpenMultiMediaReceiveChannelAckMessage, "Open MultiMedia Receive Channel Acknowledge", offsize(sccp_data_t, OpenMultiMediaReceiveChannelAckMessage),
						     SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[ClearConferenceMessage] = { ClearConferenceMessage, "Clear Conference Message", offsize(sccp_data_t, ClearConferenceMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ServiceURLStatReqMessage] = { ServiceURLStatReqMessage, "Service URL State Request", offsize(sccp_data_t, ServiceURLStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[FeatureStatReqMessage] = { FeatureStatReqMessage, "Feature State Request", offsize(sccp_data_t, FeatureStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[CreateConferenceResMessage] = { CreateConferenceResMessage, "Create Conference Response", offsize(sccp_data_t, CreateConferenceResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[DeleteConferenceResMessage] = { DeleteConferenceResMessage, "Delete Conference Response", offsize(sccp_data_t, DeleteConferenceResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[ModifyConferenceResMessage] = { ModifyConferenceResMessage, "Modify Conference Response", offsize(sccp_data_t, ModifyConferenceResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[AddParticipantResMessage] = { AddParticipantResMessage, "Add Participant Response", offsize(sccp_data_t, AddParticipantResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[AuditConferenceResMessage] = { AuditConferenceResMessage, "Audit Conference Response", offsize(sccp_data_t, AuditConferenceResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[AuditParticipantResMessage] = { AuditParticipantResMessage, "Audit Participant Response", offsize(sccp_data_t, AuditParticipantResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[DeviceToUserDataVersion1Message] = { DeviceToUserDataVersion1Message, "Device To User Data Version1 Message", offsize(sccp_data_t, DeviceToUserDataVersion1Message), SKINNY_MSGTYPE_EVENT,
					      SKINNY_MSGDIRECTION_DEV2PBX },
	[DeviceToUserDataResponseVersion1Message] = { DeviceToUserDataResponseVersion1Message, "Device To User Data Version1 Response", offsize(sccp_data_t, DeviceToUserDataResponseVersion1Message), SKINNY_MSGTYPE_EVENT,
						      SKINNY_MSGDIRECTION_DEV2PBX },
	[SubscriptionStatReqMessage] = { SubscriptionStatReqMessage, "Subscription Status Request (DialedPhoneBook)", offsize(sccp_data_t, SubscriptionStatReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[AccessoryStatusMessage] = { AccessoryStatusMessage, "Accessory Status Message", offsize(sccp_data_t, AccessoryStatusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },

	/* PBX -> DEV */
	[RegisterAckMessage] = { RegisterAckMessage, "Register Acknowledge", offsize(sccp_data_t, RegisterAckMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[StartToneMessage] = { StartToneMessage, "Start Tone Message", offsize(sccp_data_t, StartToneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[StopToneMessage] = { StopToneMessage, "Stop Tone Message", offsize(sccp_data_t, StopToneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SetRingerMessage] = { SetRingerMessage, "Set Ringer Message", offsize(sccp_data_t, SetRingerMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SetLampMessage] = { SetLampMessage, "Set Lamp Message", offsize(sccp_data_t, SetLampMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SetHookFlashDetectMessage] = { SetHookFlashDetectMessage, "Set HookFlash Detect Message", offsize(sccp_data_t, SetHookFlashDetectMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SetSpeakerModeMessage] = { SetSpeakerModeMessage, "Set Speaker Mode Message", offsize(sccp_data_t, SetSpeakerModeMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SetMicroModeMessage] = { SetMicroModeMessage, "Set Micro Mode Message", offsize(sccp_data_t, SetMicroModeMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/*spa*/[StartMediaTransmission] = { StartMediaTransmission, "Start Media Transmission", offsize(sccp_data_t, StartMediaTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopMediaTransmission] = { StopMediaTransmission, "Stop Media Transmission", offsize(sccp_data_t, StopMediaTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StartMediaReception] = { StartMediaReception, "Start Media Reception", offsize(sccp_data_t, StartMediaReception), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopMediaReception] = { StopMediaReception, "Stop Media Reception", offsize(sccp_data_t, StopMediaReception), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallInfoMessage] = { CallInfoMessage, "Call Information Message", offsize(sccp_data_t, CallInfoMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ForwardStatMessage] = { ForwardStatMessage, "Forward State Message", offsize(sccp_data_t, ForwardStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[SpeedDialStatMessage] = { SpeedDialStatMessage, "SpeedDial State Message", offsize(sccp_data_t, SpeedDialStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[LineStatMessage] = { LineStatMessage, "Line State Message", offsize(sccp_data_t, LineStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[ConfigStatMessage] = { ConfigStatMessage, "Config State Message", offsize(sccp_data_t, ConfigStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[ConfigStatDynamicMessage] = { ConfigStatDynamicMessage, "Config State Dynamic Message", offsize(sccp_data_t, ConfigStatDynamicMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[DefineTimeDate] = { DefineTimeDate, "Define Time Date", offsize(sccp_data_t, DefineTimeDate), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StartSessionTransmission] = { StartSessionTransmission, "Start Session Transmission", offsize(sccp_data_t, StartSessionTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopSessionTransmission] = { StopSessionTransmission, "Stop Session Transmission", offsize(sccp_data_t, StopSessionTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ButtonTemplateMessage] = { ButtonTemplateMessage, "Button Template Message", offsize(sccp_data_t, ButtonTemplateMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	//[ButtonTemplateMessageSingle] = {ButtonTemplateMessageSingle,			"Button Template Message Single",		offsize(sccp_data_t, ButtonTemplateMessageSingle)},
	[VersionMessage] = { VersionMessage, "Version Message", offsize(sccp_data_t, VersionMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayTextMessage] = { DisplayTextMessage, "Display Text Message", offsize(sccp_data_t, DisplayTextMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ClearDisplay] = { ClearDisplay, "Clear Display", offsize(sccp_data_t, ClearDisplay), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },

	[CapabilitiesReqMessage] = { CapabilitiesReqMessage, "Capabilities Request", offsize(sccp_data_t, CapabilitiesReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },

	[EnunciatorCommandMessage] = { EnunciatorCommandMessage, "Enunciator Command Message", offsize(sccp_data_t, EnunciatorCommandMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[RegisterRejectMessage] = { RegisterRejectMessage, "Register Reject Message", offsize(sccp_data_t, RegisterRejectMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[ServerResMessage] = { ServerResMessage, "Server Response", offsize(sccp_data_t, ServerResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[Reset] = { Reset, "Reset", offsize(sccp_data_t, Reset), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[KeepAliveAckMessage] = { KeepAliveAckMessage, "Keep Alive Acknowledge", offsize(sccp_data_t, KeepAliveAckMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StartMulticastMediaReception] = { StartMulticastMediaReception, "Start MulticastMedia Reception", offsize(sccp_data_t, StartMulticastMediaReception), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StartMulticastMediaTransmission] = { StartMulticastMediaTransmission, "Start MulticastMedia Transmission", offsize(sccp_data_t, StartMulticastMediaTransmission), SKINNY_MSGTYPE_EVENT,
						  SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopMulticastMediaReception] = { StopMulticastMediaReception, "Stop MulticastMedia Reception", offsize(sccp_data_t, StopMulticastMediaReception), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopMulticastMediaTransmission] = { StopMulticastMediaTransmission, "Stop MulticastMedia Transmission", offsize(sccp_data_t, StopMulticastMediaTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[OpenReceiveChannel] = { OpenReceiveChannel, "Open Receive Channel", offsize(sccp_data_t, OpenReceiveChannel), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[CloseReceiveChannel] = { CloseReceiveChannel, "Close Receive Channel", offsize(sccp_data_t, CloseReceiveChannel), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/*[ConnectionStatisticsReq] = { ConnectionStatisticsReq, "Connection Statistics Request", offsize(sccp_data_t, ConnectionStatisticsReq), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },**/
	[ConnectionStatisticsReq] = { ConnectionStatisticsReq, "Connection Statistics Request", offsize(sccp_data_t, ConnectionStatisticsReq), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SoftKeyTemplateResMessage] = { SoftKeyTemplateResMessage, "SoftKey Template Response", offsize(sccp_data_t, SoftKeyTemplateResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[SoftKeySetResMessage] = { SoftKeySetResMessage, "SoftKey Set Response", offsize(sccp_data_t, SoftKeySetResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[SelectSoftKeysMessage] = { SelectSoftKeysMessage, "Select SoftKeys Message", offsize(sccp_data_t, SelectSoftKeysMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallStateMessage] = { CallStateMessage, "Call State Message", offsize(sccp_data_t, CallStateMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayPromptStatusMessage] = { DisplayPromptStatusMessage, "Display Prompt Status Message", offsize(sccp_data_t, DisplayPromptStatusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ClearPromptStatusMessage] = { ClearPromptStatusMessage, "Clear Prompt Status Message", offsize(sccp_data_t, ClearPromptStatusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayNotifyMessage] = { DisplayNotifyMessage, "Display Notify Message", offsize(sccp_data_t, DisplayNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ClearNotifyMessage] = { ClearNotifyMessage, "Clear Notify Message", offsize(sccp_data_t, ClearNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ActivateCallPlaneMessage] = { ActivateCallPlaneMessage, "Activate Call Plane Message", offsize(sccp_data_t, ActivateCallPlaneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[DeactivateCallPlaneMessage] = { DeactivateCallPlaneMessage, "Deactivate Call Plane Message", offsize(sccp_data_t, DeactivateCallPlaneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[UnregisterAckMessage] = { UnregisterAckMessage, "Unregister Acknowledge", offsize(sccp_data_t, UnregisterAckMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[BackSpaceResMessage] = { BackSpaceResMessage, "Back Space Response", offsize(sccp_data_t, BackSpaceResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[RegisterTokenAck] = { RegisterTokenAck, "Register Token Acknowledge", offsize(sccp_data_t, RegisterTokenAck), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[RegisterTokenReject] = { RegisterTokenReject, "Register Token Reject", offsize(sccp_data_t, RegisterTokenReject), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[StartMediaFailureDetection] = { StartMediaFailureDetection, "Start Media Failure Detection", offsize(sccp_data_t, StartMediaFailureDetection), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[DialedNumberMessage] = { DialedNumberMessage, "Dialed Number Message", offsize(sccp_data_t, DialedNumberMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[UserToDeviceDataMessage] = { UserToDeviceDataMessage, "User To Device Data Message", offsize(sccp_data_t, UserToDeviceDataMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[FeatureStatMessage] = { FeatureStatMessage, "Feature State Message", offsize(sccp_data_t, FeatureStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayPriNotifyMessage] = { DisplayPriNotifyMessage, "Display Pri Notify Message", offsize(sccp_data_t, DisplayPriNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ClearPriNotifyMessage] = { ClearPriNotifyMessage, "Clear Pri Notify Message", offsize(sccp_data_t, ClearPriNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[StartAnnouncementMessage] = { StartAnnouncementMessage, "Start Announcement Message", offsize(sccp_data_t, StartAnnouncementMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[StopAnnouncementMessage] = { StopAnnouncementMessage, "Stop Announcement Message", offsize(sccp_data_t, StopAnnouncementMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[AnnouncementFinishMessage] = { AnnouncementFinishMessage, "Announcement Finish Message", offsize(sccp_data_t, AnnouncementFinishMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[NotifyDtmfToneMessage] = { NotifyDtmfToneMessage, "Notify DTMF Tone Message", offsize(sccp_data_t, NotifyDtmfToneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SendDtmfToneMessage] = { SendDtmfToneMessage, "Send DTMF Tone Message", offsize(sccp_data_t, SendDtmfToneMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SubscribeDtmfPayloadReqMessage] = { SubscribeDtmfPayloadReqMessage, "Subscribe DTMF Payload Request", offsize(sccp_data_t, SubscribeDtmfPayloadReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[SubscribeDtmfPayloadResMessage] = { SubscribeDtmfPayloadResMessage, "Subscribe DTMF Payload Response", offsize(sccp_data_t, SubscribeDtmfPayloadResMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_DEV2PBX },
	[SubscribeDtmfPayloadErrMessage] = { SubscribeDtmfPayloadErrMessage, "Subscribe DTMF Payload Error Message", offsize(sccp_data_t, SubscribeDtmfPayloadErrMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[UnSubscribeDtmfPayloadReqMessage] = { UnSubscribeDtmfPayloadReqMessage, "UnSubscribe DTMF Payload Request", offsize(sccp_data_t, UnSubscribeDtmfPayloadReqMessage), SKINNY_MSGTYPE_REQUEST,
					       SKINNY_MSGDIRECTION_UNKNOWN },
	[UnSubscribeDtmfPayloadResMessage] = { UnSubscribeDtmfPayloadResMessage, "UnSubscribe DTMF Payload Response", offsize(sccp_data_t, UnSubscribeDtmfPayloadResMessage), SKINNY_MSGTYPE_RESPONSE,
					       SKINNY_MSGDIRECTION_UNKNOWN },
	[UnSubscribeDtmfPayloadErrMessage] = { UnSubscribeDtmfPayloadErrMessage, "UnSubscribe DTMF Payload Error Message", offsize(sccp_data_t, UnSubscribeDtmfPayloadErrMessage), SKINNY_MSGTYPE_EVENT,
					       SKINNY_MSGDIRECTION_UNKNOWN },
	[ServiceURLStatMessage] = { ServiceURLStatMessage, "ServiceURL State Message", offsize(sccp_data_t, ServiceURLStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallSelectStatMessage] = { CallSelectStatMessage, "Call Select State Message", offsize(sccp_data_t, CallSelectStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[OpenMultiMediaChannelMessage] = { OpenMultiMediaChannelMessage, "Open MultiMedia Channel Message", offsize(sccp_data_t, OpenMultiMediaChannelMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	/*spa*/[StartMultiMediaTransmission] = { StartMultiMediaTransmission, "Start MultiMedia Transmission", offsize(sccp_data_t, StartMultiMediaTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StopMultiMediaTransmission] = { StopMultiMediaTransmission, "Stop MultiMedia Transmission", offsize(sccp_data_t, StopMultiMediaTransmission), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[MiscellaneousCommandMessage] = { MiscellaneousCommandMessage, "Miscellaneous Command Message", offsize(sccp_data_t, MiscellaneousCommandMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[FlowControlCommandMessage] = { FlowControlCommandMessage, "Flow Control Command Message", offsize(sccp_data_t, FlowControlCommandMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[CloseMultiMediaReceiveChannel] = { CloseMultiMediaReceiveChannel, "Close MultiMedia Receive Channel", offsize(sccp_data_t, CloseMultiMediaReceiveChannel), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[CreateConferenceReqMessage] = { CreateConferenceReqMessage, "Create Conference Request", offsize(sccp_data_t, CreateConferenceReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[DeleteConferenceReqMessage] = { DeleteConferenceReqMessage, "Delete Conference Request", offsize(sccp_data_t, DeleteConferenceReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[ModifyConferenceReqMessage] = { ModifyConferenceReqMessage, "Modify Conference Request", offsize(sccp_data_t, ModifyConferenceReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[AddParticipantReqMessage] = { AddParticipantReqMessage, "Add Participant Request", offsize(sccp_data_t, AddParticipantReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[DropParticipantReqMessage] = { DropParticipantReqMessage, "Drop Participant Request", offsize(sccp_data_t, DropParticipantReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[AuditConferenceReqMessage] = { AuditConferenceReqMessage, "Audit Conference Request", offsize(sccp_data_t, AuditConferenceReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[AuditParticipantReqMessage] = { AuditParticipantReqMessage, "Audit Participant Request", offsize(sccp_data_t, AuditParticipantReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[UserToDeviceDataVersion1Message] = { UserToDeviceDataVersion1Message, "User To Device Data Version1 Message", offsize(sccp_data_t, UserToDeviceDataVersion1Message), SKINNY_MSGTYPE_EVENT,
					      SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayDynamicNotifyMessage] = { DisplayDynamicNotifyMessage, "Display Dynamic Notify Message", offsize(sccp_data_t, DisplayDynamicNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[DisplayDynamicPriNotifyMessage] = { DisplayDynamicPriNotifyMessage, "Display Dynamic Priority Notify Message", offsize(sccp_data_t, DisplayDynamicPriNotifyMessage), SKINNY_MSGTYPE_EVENT,
					     SKINNY_MSGDIRECTION_UNKNOWN },
	[DisplayDynamicPromptStatusMessage] = { DisplayDynamicPromptStatusMessage, "Display Dynamic Prompt Status Message", offsize(sccp_data_t, DisplayDynamicPromptStatusMessage), SKINNY_MSGTYPE_EVENT,
						SKINNY_MSGDIRECTION_UNKNOWN },
	[FeatureStatDynamicMessage] = { FeatureStatDynamicMessage, "SpeedDial State Dynamic Message", offsize(sccp_data_t, FeatureStatDynamicMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[LineStatDynamicMessage] = { LineStatDynamicMessage, "Line State Dynamic Message", offsize(sccp_data_t, LineStatDynamicMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[ServiceURLStatDynamicMessage] = { ServiceURLStatDynamicMessage, "Service URL Stat Dynamic Messages", offsize(sccp_data_t, ServiceURLStatDynamicMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[SpeedDialStatDynamicMessage] = { SpeedDialStatDynamicMessage, "SpeedDial Stat Dynamic Message", offsize(sccp_data_t, SpeedDialStatDynamicMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallInfoDynamicMessage] = { CallInfoDynamicMessage, "Call Information Dynamic Message", offsize(sccp_data_t, CallInfoDynamicMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[SubscriptionStatMessage] = { SubscriptionStatMessage, "Subscription Status Response (Dialed Number)", offsize(sccp_data_t, SubscriptionStatMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[NotificationMessage] = { NotificationMessage, "Notify Call List (CallListStatusUpdate)", offsize(sccp_data_t, NotificationMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/*spa*/[StartMediaTransmissionAck] = { StartMediaTransmissionAck, "Start Media Transmission Acknowledge", offsize(sccp_data_t, StartMediaTransmissionAck), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/**/[StartMultiMediaTransmissionAck] = { StartMultiMediaTransmissionAck, "Start Media Transmission Acknowledge", offsize(sccp_data_t, StartMultiMediaTransmissionAck), SKINNY_MSGTYPE_EVENT,
						 SKINNY_MSGDIRECTION_PBX2DEV },
	[CallHistoryDispositionMessage] = { CallHistoryDispositionMessage, "Call History Disposition", offsize(sccp_data_t, CallHistoryDispositionMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[LocationInfoMessage] = { LocationInfoMessage, "Location/Wifi Information", offsize(sccp_data_t, LocationInfoMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[ExtensionDeviceCaps] = { ExtensionDeviceCaps, "Extension Device Capabilities Message", offsize(sccp_data_t, ExtensionDeviceCaps), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[XMLAlarmMessage] = { XMLAlarmMessage, "XML-AlarmMessage", offsize(sccp_data_t, XMLAlarmMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[MediaPathCapabilityMessage] = { MediaPathCapabilityMessage, "MediaPath Capability Message", offsize(sccp_data_t, MediaPathCapabilityMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[FlowControlNotifyMessage] = { FlowControlNotifyMessage, "FlowControl Notify Message", offsize(sccp_data_t, FlowControlNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallCountReqMessage] = { CallCountReqMessage, "CallCount Request Message", offsize(sccp_data_t, CallCountReqMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	/*new*/
	[UpdateCapabilitiesV2Message] = { UpdateCapabilitiesV2Message, "Update Capabilities V2", offsize(sccp_data_t, UpdateCapabilitiesV2Message), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[UpdateCapabilitiesV3Message] = { UpdateCapabilitiesV3Message, "Update Capabilities V3", offsize(sccp_data_t, UpdateCapabilitiesV3Message), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_DEV2PBX },
	[PortResponseMessage] = { PortResponseMessage, "Port Response Message", offsize(sccp_data_t, PortResponseMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSResvNotifyMessage] = { QoSResvNotifyMessage, "QoS Resv Notify Message", offsize(sccp_data_t, QoSResvNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSErrorNotifyMessage] = { QoSErrorNotifyMessage, "QoS Error Notify Message", offsize(sccp_data_t, QoSErrorNotifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[PortRequestMessage] = { PortRequestMessage, "Port Request Message", offsize(sccp_data_t, PortRequestMessage), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_PBX2DEV },
	[PortCloseMessage] = { PortCloseMessage, "Port Close Message", offsize(sccp_data_t, PortCloseMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSListenMessage] = { QoSListenMessage, "QoS Listen Message", offsize(sccp_data_t, QoSListenMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSPathMessage] = { QoSPathMessage, "QoS Path Message", offsize(sccp_data_t, QoSPathMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSTeardownMessage] = { QoSTeardownMessage, "QoS Teardown Message", offsize(sccp_data_t, QoSTeardownMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[UpdateDSCPMessage] = { UpdateDSCPMessage, "Update DSCP Message", offsize(sccp_data_t, UpdateDSCPMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[QoSModifyMessage] = { QoSModifyMessage, "QoS Modify Message", offsize(sccp_data_t, QoSModifyMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	[MwiResponseMessage] = { MwiResponseMessage, "Mwi Response Message", offsize(sccp_data_t, MwiResponseMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[CallCountRespMessage] = { CallCountRespMessage, "CallCount Response Message", offsize(sccp_data_t, CallCountRespMessage), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[RecordingStatusMessage] = { RecordingStatusMessage, "Recording Status Message", offsize(sccp_data_t, RecordingStatusMessage), SKINNY_MSGTYPE_EVENT, SKINNY_MSGDIRECTION_PBX2DEV },
	/* clang-format on */
};

const struct messageinfo spcp_messageinfo[] = {
	/* clang-format off */
	[SPCPRegisterTokenRequest - SPCP_MESSAGE_OFFSET] = { SPCPRegisterTokenRequest, "SPCP Register Token Request", offsize(sccp_data_t, SPCPRegisterTokenRequest), SKINNY_MSGTYPE_REQUEST, SKINNY_MSGDIRECTION_DEV2PBX },
	[SPCPRegisterTokenAck - SPCP_MESSAGE_OFFSET] = { SPCPRegisterTokenAck, "SPCP RegisterMessageACK", offsize(sccp_data_t, SPCPRegisterTokenAck), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	[SPCPRegisterTokenReject - SPCP_MESSAGE_OFFSET] = { SPCPRegisterTokenReject, "SPCP RegisterMessageReject", offsize(sccp_data_t, SPCPRegisterTokenReject), SKINNY_MSGTYPE_RESPONSE, SKINNY_MSGDIRECTION_PBX2DEV },
	//[UnknownVGMessage - SPCP_MESSAGE_OFFSET		] = {UnknownVGMessage,		"Unknown Message (VG224)",			offsize(sccp_data_t, UnknownVGMessage)},
	/* clang-format on */
};

gcc_inline struct messageinfo * lookupMsgInfoStruct(uint32_t messageId)
{
	if(messageId <= SCCP_MESSAGE_HIGH_BOUNDARY) {
		return (struct messageinfo *)&sccp_messageinfo[messageId];
	}
	if(messageId >= SPCP_MESSAGE_LOW_BOUNDARY && messageId <= SPCP_MESSAGE_HIGH_BOUNDARY) {
		return (struct messageinfo *)&spcp_messageinfo[messageId - SPCP_MESSAGE_OFFSET];
	}
	pbx_log(LOG_ERROR, "SCCP: (session::lookupMsgInfo) messageId out of bounds: %d < %u > %d. Or messageId unknown. discarding message.\n", SCCP_MESSAGE_LOW_BOUNDARY, messageId, SPCP_MESSAGE_HIGH_BOUNDARY);
	return NULL;
}

gcc_inline const char * msginfo2str(sccp_mid_t msgId)
{														/* sccp_protocol.h */
	struct messageinfo * info = lookupMsgInfoStruct(msgId);
	return info->text;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
