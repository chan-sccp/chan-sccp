/*!
 * \file	sccp_featureCallCompletion.c
 * \brief	SCCP CallCompletion Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2020-Sept-22
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_featureCallCompletion.h"

#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_labels.h"
#include "sccp_utils.h"

static const uint32_t _appID = APPID_CC;

/* forward declarations */

/* private variables */
struct sccp_cc_agent_pvt {
	int offer_timer_id;
	pbx_callid_t original_callid;
	char original_exten[DEFAULT_PBX_STR_BUFFERSIZE];
	char deviceid[StationMaxDeviceNameSize];
	char linename[StationMaxNameSize];
	uint8_t lineInstance;
	char is_available;
	skinny_keymode_t restore_keymode;
	int core_id;
	uint32_t transactionID;
};

/* private functions */
static int agent_init(struct ast_cc_agent * agent, PBX_CHANNEL_TYPE * ast)
{
	struct sccp_cc_agent_pvt * agent_pvt = ast_calloc(1, sizeof(*agent_pvt));
	if(!agent_pvt) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return -1;
	}
	ast_assert(!strcmp(ast_channel_tech(ast)->type, "SCCP"));
	sccp_channel_t * channel = ast_channel_tech_pvt(ast);
	pbx_log(LOG_NOTICE, "SCCP_CC: agent_init for %s\n", channel->designator);

	agent_pvt->original_callid = (pbx_callid_t)ast_channel_callid(ast);
	sccp_copy_string(agent_pvt->original_exten, channel->dialedNumber, sizeof(agent_pvt->original_exten));
	agent_pvt->offer_timer_id = -1;
	agent->private_data = agent_pvt;
	if(channel) {
		sccp_copy_string(agent_pvt->linename, channel->line->name, sizeof(agent_pvt->linename));
		AUTO_RELEASE(sccp_device_t, d, sccp_channel_getDevice(channel));
		if(d) {
			sccp_copy_string(agent_pvt->deviceid, d->id, sizeof(agent_pvt->deviceid));
			agent_pvt->restore_keymode = sccp_dev_get_keymode(d);
			agent_pvt->lineInstance = sccp_device_find_index_for_line(d, channel->line->name);
		}
		channel->line->cc_state = SCCP_CC_OFFERED;
	}
	agent_pvt->core_id = agent->core_id;
	agent_pvt->transactionID = sccp_random() % 1000;
	return 0;
}

static int agent_offer_timer_expire(const void * data)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: start offer timer -> set softkeyset:'callback'\n");
	struct ast_cc_agent * agent = (struct ast_cc_agent *)data;
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;

	agent_pvt->offer_timer_id = -1;
	AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(agent_pvt->linename, FALSE));
	if(l) {
		l->cc_core_id = -1;
		l->cc_state = SCCP_CC_OFFER_TIMEDOUT;
	}
	AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(agent_pvt->deviceid, FALSE));
	if(d) {
		sccp_dev_set_keyset(d, agent_pvt->lineInstance, 0, agent_pvt->restore_keymode);
		sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_CALLBACK);
	}
	return ast_cc_failed(agent->core_id, "SCCP CC agent %s's offer timer expired", agent->device_name);
}

static int agent_start_offer_timer(struct ast_cc_agent * agent)
{
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;
	pbx_log(LOG_NOTICE, "SCCP_CC: start offer timer for %s@%s-> set softkeyset:'callback'\n", agent_pvt->linename, agent_pvt->deviceid);
	int when = ast_get_cc_offer_timer(agent->cc_params) * 1000;
	agent_pvt->offer_timer_id = iPbx.sched_add(when, agent_offer_timer_expire, agent);
	AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(agent_pvt->linename, FALSE));
	if(l) {
		l->cc_core_id = agent->core_id;
		// l->cc_state = SCCP_CC_OFFERED;
	}
	AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(agent_pvt->deviceid, FALSE));
	if(d) {
		pbx_str_t * buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		ast_str_append(&buf, DEFAULT_PBX_STR_BUFFERSIZE, "%s %s", SKINNY_DISP_CALLBACK, agent_pvt->original_exten);
		sccp_device_addMessageToStack(d, SCCP_MESSAGE_PRIORITY_CALLBACK, pbx_str_buffer(buf));
		sccp_dev_set_keyset(d, agent_pvt->lineInstance, 0, KEYMODE_CALLCOMPLETION);
	}
	return 0;
}
static int agent_stop_offer_timer(struct ast_cc_agent * agent)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: stop offer timer -> revert softkeyset:'default'\n");
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;
	iPbx.sched_del(agent_pvt->offer_timer_id);
	AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(agent_pvt->linename, FALSE));
	if(l) {
		l->cc_core_id = -1;
		l->cc_state = SCCP_CC_OFFER_TIMEDOUT;
	}
	AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(agent_pvt->deviceid, FALSE));
	if(d) {
		sccp_dev_set_keyset(d, agent_pvt->lineInstance, 0, agent_pvt->restore_keymode);
		sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_CALLBACK);
	}
	return 0;
}
static void agent_respond(struct ast_cc_agent * agent, enum ast_cc_agent_response_reason reason)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: agent respond\n");
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;
	if(reason == AST_CC_AGENT_RESPONSE_SUCCESS) {
		AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(agent_pvt->linename, FALSE));
		if(l) {
			l->cc_state = SCCP_CC_QUEUED;
		}
		AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(agent_pvt->deviceid, FALSE));
		if(d) {
			// sccp_dev_displayprompt(d, agent_pvt->lineInstance, 0, "Callback scheduled", SCCP_DISPLAYSTATUS_TIMEOUT);
			pbx_str_t * buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
			ast_str_append(&buf, DEFAULT_PBX_STR_BUFFERSIZE, "%s %s:%s", SKINNY_DISP_CALLBACK, SKINNY_DISP_CALLS_IN_QUEUE, agent_pvt->original_exten);
			sccp_device_addMessageToStack(d, SCCP_MESSAGE_PRIORITY_CALLBACK, pbx_str_buffer(buf));
			sccp_dev_starttone(d, SKINNY_TONE_CAMPONINDICATIONTONE, agent_pvt->lineInstance, 0, SKINNY_TONEDIRECTION_USER);
			sccp_dev_set_keyset(d, agent_pvt->lineInstance, 0, KEYMODE_CALLBACK);
		}
	}
	agent_pvt->is_available = TRUE;
}
static int agent_status_request(struct ast_cc_agent * agent)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: status request\n");
	// ast_cc_agent_status_response(agent->core_id, ast_device_state(agent->device_name));
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;
	enum ast_device_state state = agent_pvt->is_available ? AST_DEVICE_NOT_INUSE : AST_DEVICE_INUSE;
	return ast_cc_agent_status_response(agent->core_id, state);
}
static int agent_start_monitoring(struct ast_cc_agent * agent)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: start monitoring -> update softkeys:'callback'-> show status, 'cancel':allow cc cancellation\n");
	/* To start monitoring just means to wait for an incoming PUBLISH
	 * to tell us that the caller has become available again. No special
	 * action is needed
	 */
	return 0;
}

static int sendForm(constDevicePtr d, constLinePtr l, const char *exten, uint8_t lineInstance, int core_id, uint32_t transactionID)
{
	pbx_str_t * xmlStr = pbx_str_create(2048);
	pbx_str_append(&xmlStr, 0, "<CiscoIPPhoneText appId=\"%d\" onAppClosed=\"%d\">", _appID, _appID);
	pbx_str_append(&xmlStr, 0, "<Title>Call Completion</Title>");
	if (exten) {
		if (l->cc_state == SCCP_CC_PARTY_AVAILABLE) {
			pbx_str_append(&xmlStr, 0, "<Prompt>Party is available for callback</Prompt>");
		} else {
			pbx_str_append(&xmlStr, 0, "<Prompt>Scheduled</Prompt>");
		}
		pbx_str_append(&xmlStr, 0, "<Text>CallCompletion Scheduled for %s</Text>", exten);
	} else {
		pbx_str_append(&xmlStr, 0, "<Prompt>Call Completion not available</Prompt>");
		pbx_str_append(&xmlStr, 0, "<Text>Call Completion is not available at this time.</Text>");
	}
	if (l->cc_state == SCCP_CC_PARTY_AVAILABLE) {
		pbx_str_append(&xmlStr, 0, "<SoftKeyItem>");
		pbx_str_append(&xmlStr, 0, "<Name>Call</Name>");
		pbx_str_append(&xmlStr, 0, "<Position>1</Position>");
		pbx_str_append(&xmlStr, 0, "<URL>UserCallData:%d:%d:%d:%d:DIAL/%s</URL>", _appID, lineInstance, core_id, transactionID, exten);
		pbx_str_append(&xmlStr, 0, "</SoftKeyItem>");
	}
	pbx_str_append(&xmlStr, 0, "<SoftKeyItem>");
	pbx_str_append(&xmlStr, 0, "<Name>Cancel</Name>");
	pbx_str_append(&xmlStr, 0, "<Position>3</Position>");
	pbx_str_append(&xmlStr, 0, "<URL>UserCallData:%d:%d:%d:%d:CANCEL</URL>", _appID, lineInstance, core_id, transactionID);
	pbx_str_append(&xmlStr, 0, "</SoftKeyItem>");
	pbx_str_append(&xmlStr, 0, "</CiscoIPPhoneText>");
	d->protocol->sendUserToDeviceDataVersionMessage(d, _appID, lineInstance, 0, transactionID, pbx_str_buffer(xmlStr), 2);
	sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_3 "%s: (%s) pushing:%s\n", d->id, __func__, pbx_str_buffer(xmlStr));
	sccp_free(xmlStr);
	return 0;
}

static int agent_recall(struct ast_cc_agent * agent)
{
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;

	pbx_log(LOG_NOTICE, "SCCP_CC: agent recall: destination is free to call back, play beep, display status bar message, update softkeyset:'recall'\n");
	iPbx.sched_del(agent_pvt->offer_timer_id);

	sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "%s: CallCompletion previously calledparty %s is available now, via:%s (core_id:%d)\n", agent_pvt->linename, agent->device_name, agent_pvt->original_exten, agent->core_id);
	AUTO_RELEASE(sccp_line_t, l, sccp_line_find_byname(agent_pvt->linename, FALSE));
	AUTO_RELEASE(sccp_device_t, d, sccp_device_find_byid(agent_pvt->deviceid, FALSE));
	if(l && d) {
		uint8_t lineInstance = agent_pvt->lineInstance;
		uint32_t transactionID = agent_pvt->transactionID;
		if (agent_pvt->is_available) {
			l->cc_state = SCCP_CC_PARTY_AVAILABLE;
		}
		
		sccp_device_clearMessageFromStack(d, SCCP_MESSAGE_PRIORITY_CALLBACK);
		sendForm(d, l, agent_pvt->original_exten, lineInstance, agent->core_id, transactionID);

		pbx_str_t * buf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		ast_str_append(&buf, DEFAULT_PBX_STR_BUFFERSIZE, "%s:%s %s", SKINNY_DISP_CALLBACK, agent_pvt->original_exten, SKINNY_DISP_READY);
		sccp_dev_displayprompt(d, lineInstance, 0, pbx_str_buffer(buf), GLOB(digittimeout));
		sccp_dev_set_keyset(d, agent_pvt->lineInstance, 0, KEYMODE_CALLBACK);
		return 0;
	}
	return -1;
}
static void agent_destructor(struct ast_cc_agent * agent)
{
	struct sccp_cc_agent_pvt * agent_pvt = agent->private_data;
	pbx_log(LOG_NOTICE, "SCCP_CC: agent destructor for %s@%s\n", agent_pvt->linename, agent_pvt->deviceid);
	sccp_free(agent_pvt);
}

static struct ast_cc_agent_callbacks pbx_cc_agent_callbacks = {
	.type = "SCCP",
	.init = agent_init,
	.start_offer_timer = agent_start_offer_timer,
	.stop_offer_timer = agent_stop_offer_timer,
	.respond = agent_respond,
	.status_request = agent_status_request,
	.start_monitoring = agent_start_monitoring,
	.callee_available = agent_recall,
	.destructor = agent_destructor,
};

/* exported functions */
static int register_agent_callback(void)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: registering cc_agent callbacks\n");
	return !ast_cc_agent_register(&pbx_cc_agent_callbacks);
}
static void unregister_agent_callback(void)
{
	pbx_log(LOG_NOTICE, "SCCP_CC: registering cc_agent callbacks\n");
	ast_cc_agent_unregister(&pbx_cc_agent_callbacks);
}

static int pushForm(constDevicePtr d, constLinePtr l)
{
	return sendForm(d, l, NULL, 0 /*lineInstance*/, l->cc_core_id, 0);
}

static void handleDevice2User(constDevicePtr d, uint32_t appID, uint8_t instance, uint32_t callReference, uint32_t transactionId, const char *data)
{
	pbx_assert(d != NULL);
	sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) instance:%d, transactionId:%d, callReference:%d, data:%s\n", d->id, instance, transactionId, callReference, data);
	if (sccp_strequals(data, "CANCEL")) {
		sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) CANCEL\n", d->id);
		// cancel call completion
		// core_id = callReference;
		// agent_stop_offer_timer(agent);
		// agent_destructor(agent);
	} else if (strcasestr(data, "DIAL/")) {
		sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) DIAL\n", d->id);
		char exten[SCCP_MAX_EXTENSION];
		if (sscanf(data, "DIAL/%s", exten) == 1) {
			sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "%s: (handleDevice2Usewr) DIAL-exten:%s\n", d->id, exten);
			AUTO_RELEASE(sccp_line_t, line , d->currentLine ? sccp_dev_getActiveLine(d) : sccp_line_find_byid(d, d->defaultLineInstance));
			AUTO_RELEASE(sccp_channel_t, new_channel, sccp_channel_newcall(line, d, exten, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL));                                        // implicit release
		}
	}

	char xmlData[512] = "";
	if (d->protocolversion >= 15) {
		snprintf(xmlData, sizeof(xmlData), "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"App:Close:0\"/></CiscoIPPhoneExecute>");
	} else {
		snprintf(xmlData, sizeof(xmlData), "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Init:Services\"/></CiscoIPPhoneExecute>");
	}
	d->protocol->sendUserToDeviceDataVersionMessage(d, appID, callReference, instance, transactionId, xmlData, 2);
}

/* Assign to interface */
#if 1
const CallCompletionInterface iCallCompletion = {
	.registerCcAgentCallback = register_agent_callback,
	.unregisterCcAgentCallback = unregister_agent_callback,
	.handleDevice2User = handleDevice2User,
	.pushForm = pushForm,
};
#else
const CallCompletionInterface iCallCompletion = { 0 };
#endif
