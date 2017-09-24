/*!
 * \file        sccp_labels.c
 * \brief       SCCP Labels Class
 *
 * SCCP Button Number References and SCCP Display Number References
 *
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_labels.h"

/*!
 * \brief Skinny LABEL Structure
 */
static const struct skinny_label {
	const char *const text;
	uint16_t label;
} skinny_labels[] = {
	/* INDENT-OFF */
	{"Empty", SKINNY_LBL_EMPTY},
	{"Redial", SKINNY_LBL_REDIAL},
	{"NewCall", SKINNY_LBL_NEWCALL},
	{"Hold", SKINNY_LBL_HOLD},
	{"Transfer", SKINNY_LBL_TRANSFER},
	{"CFwdALL", SKINNY_LBL_CFWDALL},
	{"CFwdBusy", SKINNY_LBL_CFWDBUSY},
	{"CFwdNoAnswer", SKINNY_LBL_CFWDNOANSWER},
	{"<<", SKINNY_LBL_BACKSPACE},
	{"EndCall", SKINNY_LBL_ENDCALL},
	{"Resume", SKINNY_LBL_RESUME},
	{"Answer", SKINNY_LBL_ANSWER},
	{"Info", SKINNY_LBL_INFO},
	{"Confrn", SKINNY_LBL_CONFRN},
	{"Park", SKINNY_LBL_PARK},
	{"Join", SKINNY_LBL_JOIN},
	{"MeetMe", SKINNY_LBL_MEETME},
	{"PickUp", SKINNY_LBL_PICKUP},
	{"GPickUp", SKINNY_LBL_GPICKUP},
	{"Your current options", SKINNY_LBL_YOUR_CURRENT_OPTIONS},
	{"Off Hook", SKINNY_LBL_OFF_HOOK},
	{"On Hook", SKINNY_LBL_ON_HOOK},
	{"Ring out", SKINNY_LBL_RING_OUT},
	{"From ", SKINNY_LBL_FROM},
	{"Connected", SKINNY_LBL_CONNECTED},
	{"Busy", SKINNY_LBL_BUSY},
	{"Line In Use", SKINNY_LBL_LINE_IN_USE},
	{"Call Waiting", SKINNY_LBL_CALL_WAITING},
	{"Call Transfer", SKINNY_LBL_CALL_TRANSFER},
	{"Call Park", SKINNY_LBL_CALL_PARK},
	{"Call Proceed", SKINNY_LBL_CALL_PROCEED},
	{"In Use Remote", SKINNY_LBL_IN_USE_REMOTE},
	{"Enter number", SKINNY_LBL_ENTER_NUMBER},
	{"Call park At", SKINNY_LBL_CALL_PARK_AT},
	{"Primary Only", SKINNY_LBL_PRIMARY_ONLY},
	{"Temp Fail", SKINNY_LBL_TEMP_FAIL},
	{"You Have a VoiceMail", SKINNY_LBL_YOU_HAVE_VOICEMAIL},
	{"Forwarded to", SKINNY_LBL_FORWARDED_TO},
	{"Can Not Complete Conference", SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE},
	{"No Conference Bridge", SKINNY_LBL_NO_CONFERENCE_BRIDGE},
	{"Can Not Hold Primary Control", SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL},
	{"Invalid Conference Participant", SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT},
	{"In Conference Already", SKINNY_LBL_IN_CONFERENCE_ALREADY},
	{"No Participant Info", SKINNY_LBL_NO_PARTICIPANT_INFO},
	{"Exceed Maximum Parties", SKINNY_LBL_EXCEED_MAXIMUM_PARTIES},
	{"Key Is Not Active", SKINNY_LBL_KEY_IS_NOT_ACTIVE},
	{"Error No License", SKINNY_LBL_ERROR_NO_LICENSE},
	{"Error DBConfig", SKINNY_LBL_ERROR_DBCONFIG},
	{"Error Database", SKINNY_LBL_ERROR_DATABASE},
	{"Error Pass Limit", SKINNY_LBL_ERROR_PASS_LIMIT},
	{"Error Unknown", SKINNY_LBL_ERROR_UNKNOWN},
	{"Error Mismatch", SKINNY_LBL_ERROR_MISMATCH},
	{"Conference", SKINNY_LBL_CONFERENCE},
	{"Park Number", SKINNY_LBL_PARK_NUMBER},
	{"Private", SKINNY_LBL_PRIVATE},
	{"Not Enough Bandwidth", SKINNY_LBL_NOT_ENOUGH_BANDWIDTH},
	{"Unknown Number", SKINNY_LBL_UNKNOWN_NUMBER},
	{"RmLstC", SKINNY_LBL_RMLSTC},
	{"Voicemail", SKINNY_LBL_VOICEMAIL},
	{"Immediate Divert", SKINNY_LBL_IMMDIV},
	{"Intercept", SKINNY_LBL_INTRCPT},
	{"SetWtch", SKINNY_LBL_SETWTCH},
	{"TransferVoiceMail", SKINNY_LBL_TRNSFVM},
	{"DND", SKINNY_LBL_DND},
	{"DivertAll", SKINNY_LBL_DIVALL},
	{"CallBack", SKINNY_LBL_CALLBACK},
	{"Network congestion,rerouting", SKINNY_LBL_NETWORK_CONGESTION_REROUTING},
	{"Barge", SKINNY_LBL_BARGE},
	{"Failed to setup Barge", SKINNY_LBL_FAILED_TO_SETUP_BARGE},
	{"Another Barge exists", SKINNY_LBL_ANOTHER_BARGE_EXISTS},
	{"Incompatible device type", SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE},
	{"No Park Number Available", SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE},
	{"CallPark Reversion", SKINNY_LBL_CALLPARK_REVERSION},
	{"Service is not Active", SKINNY_LBL_SERVICE_IS_NOT_ACTIVE},
	{"High Traffic Try Again Later", SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER},
	{"QRT", SKINNY_LBL_QRT},
	{"MCID", SKINNY_LBL_MCID},
	{"DirTrfr", SKINNY_LBL_DIRTRFR},
	{"Select", SKINNY_LBL_SELECT},
	{"ConfList", SKINNY_LBL_CONFLIST},
	{"iDivert", SKINNY_LBL_IDIVERT},
	{"cBarge", SKINNY_LBL_CBARGE},
	{"Can Not Complete Transfer", SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER},
	{"Can Not Join Calls", SKINNY_LBL_CAN_NOT_JOIN_CALLS},
	{"Mcid Successful", SKINNY_LBL_MCID_SUCCESSFUL},
	{"Number Not Configured", SKINNY_LBL_NUMBER_NOT_CONFIGURED},
	{"Security Error", SKINNY_LBL_SECURITY_ERROR},
	{"Video Bandwidth Unavailable", SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE},
	{"Video Mode", SKINNY_LBL_VIDEO_MODE},
	{"Record", SKINNY_LBL_MONITOR},
	{"Dial", SKINNY_LBL_DIAL},
	{"Queue", SKINNY_LBL_QUEUE},
	/* INDENT-ON */
};
gcc_inline const char *label2str(uint16_t value)
{
	uint32_t i;
	for (i = 0; i < ARRAY_LEN(skinny_labels); i++) {
		if (skinny_labels[i].label == value) {
			return skinny_labels[i].text;
		}
	}
	pbx_log(LOG_ERROR, "Label could not be found for skinny_labels.label:%i\n", value);
	return "";
}

gcc_inline uint32_t labelstr2int(const char *str)
{
	uint32_t i;
	for (i = 0; i < ARRAY_LEN(skinny_labels); i++) {
		if (!strcasecmp(skinny_labels[i].text, str)) {
			return skinny_labels[i].label;
		}
	}
	pbx_log(LOG_ERROR, "Label could not be found for skinny_labels.text:%s\n", str);
	return 0;
}
