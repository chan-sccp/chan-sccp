#include "../../src/config.h"
#include "../../src/common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "../../src/sccp_actions.h"
#include "../../src/sccp_device.h"
#include "../../src/sccp_session.h"
#include "../../src/sccp_channel.h"
#include "../../src/sccp_line.h"
//#include "sccp_utils.h"
//#include "sccp_pbx.h"
//#include "sccp_conference.h"
//#include "sccp_config.h"
//#include "sccp_features.h"
//#include "sccp_indicate.h"
//#include "sccp_featureParkingLot.h"

#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(sccp_test_actions_openChannelReceiveAck)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "openReceiveChannelAck";
			info->category = "/channels/chan_sccp/actions/";
			info->summary = "chan-sccp-b-actions-openReceiveChannelAck";
			info->description = "chan-sccp-b-actions-openReceiveChannelAck";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}

	/* Start Mocking */
	AUTO_RELEASE(sccp_device_t, d, sccp_device_create("MOCKDEV"));
	d->protocol = sccp_protocol_getDeviceProtocol(d, 3);
	d->skinny_type = SKINNY_DEVICETYPE_CISCO7970;
	pbx_test_validate(test, d != NULL);

	sccp_session_t *s = sccp_session_testhelper_create_mocksession();
	sccp_session_retainDevice(s, d);

	AUTO_RELEASE(sccp_line_t, l, sccp_line_create("MOCKLINE"));
	snprintf(l->id, SCCP_MAX_LINE_ID, "mocky");
	pbx_test_validate(test, l != NULL);

	sccp_buttonconfig_t *buttonconfig = sccp_calloc(1, sizeof(sccp_buttonconfig_t));
	buttonconfig->index = 0;
	buttonconfig->type = LINE;
	buttonconfig->label = pbx_strdup("MOCK");
	buttonconfig->button.line.name = pbx_strdup("MOCKLINE");
	SCCP_LIST_INSERT_TAIL(&d->buttonconfig, buttonconfig, list);

	btnlist *btn = sccp_calloc(sizeof *btn, StationMaxButtonTemplateSize);
	sccp_dev_build_buttontemplate(d, btn);
	btn[0].type = SKINNY_BUTTONTYPE_LINE;
	btn[0].ptr = sccp_line_retain(l);
	buttonconfig->instance = btn[0].instance = SCCP_FIRST_LINEINSTANCE;
	sccp_line_addDevice((sccp_line_t *) btn[0].ptr, d, btn[0].instance, buttonconfig->button.line.subscriptionId);

	AUTO_RELEASE(sccp_channel_t, c, sccp_channel_allocate(l, d));
	pbx_test_validate(test, c != NULL);
	/* End Mocking */

	/* Start Test */
	c->state = SCCP_CHANNELSTATE_RINGING;
	c->rtp.audio.receiveChannelState = SCCP_RTP_STATUS_PROGRESS;

	/* Test No RTP Instance*/
	c->rtp.audio.instance = NULL;

	sccp_msg_t *msg = sccp_build_packet(OpenReceiveChannelAck, sizeof(msg->data.OpenReceiveChannelAck.v3));
        msg->data.OpenReceiveChannelAck.v3.lel_callReference = htolel(c->callid);
        msg->data.OpenReceiveChannelAck.v3.lel_passThruPartyId = htolel(c->passthrupartyid);
        msg->data.OpenReceiveChannelAck.v3.lel_mediastatus = htolel(SKINNY_MEDIASTATUS_Ok);
        msg->data.OpenReceiveChannelAck.v3.lel_portNumber = htolel(10000);
	pbx_test_validate(test, msg != NULL);

	pbx_test_status_update(test, "Test without RTP instance (callReference:%d, passThruPartyId:%d, mediastatus:%s(%d)\n", c->callid, c->passthrupartyid, skinny_mediastatus2str(SKINNY_MEDIASTATUS_Ok), SKINNY_MEDIASTATUS_Ok);
	sccp_actions_testhelper_callMessageHandler(s, d, msg);
	pbx_test_validate(test, c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE);
	//pbx_test_status_update(test, "Result\n", c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE ? "Success" : "Failure");

	/* Test MediaStatus:Ok -> Active*/
	struct rtp_instance {int a;} fake_rtp_instance;
	c->rtp.audio.instance = (PBX_RTP_TYPE *) &fake_rtp_instance;
	
	c->rtp.audio.receiveChannelState = SCCP_RTP_STATUS_PROGRESS;
        msg->data.OpenReceiveChannelAck.v3.lel_callReference = htolel(c->callid);
        msg->data.OpenReceiveChannelAck.v3.lel_passThruPartyId = htolel(c->passthrupartyid);
        msg->data.OpenReceiveChannelAck.v3.lel_mediastatus = htolel(SKINNY_MEDIASTATUS_Ok);
        msg->data.OpenReceiveChannelAck.v3.lel_portNumber = htolel(10000);
	pbx_test_validate(test, msg != NULL);

	pbx_test_status_update(test, "Test without RTP instance (callReference:%d, passThruPartyId:%d, mediastatus:%s(%d)\n", c->callid, c->passthrupartyid, skinny_mediastatus2str(SKINNY_MEDIASTATUS_Ok), SKINNY_MEDIASTATUS_Ok);
	sccp_actions_testhelper_callMessageHandler(s, d, msg);
	pbx_test_validate(test, c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_ACTIVE);
	//pbx_test_status_update(test, "Result\n", c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_ACTIVE ? "Success" : "Failure");

	/* Test MediaStatus:Unknown ->Inactive*/
	c->rtp.audio.receiveChannelState = SCCP_RTP_STATUS_PROGRESS;
        msg->data.OpenReceiveChannelAck.v3.lel_callReference = htolel(c->callid);
        msg->data.OpenReceiveChannelAck.v3.lel_passThruPartyId = htolel(c->passthrupartyid);
        msg->data.OpenReceiveChannelAck.v3.lel_mediastatus = htolel(SKINNY_MEDIASTATUS_Unknown);
        msg->data.OpenReceiveChannelAck.v3.lel_portNumber = htolel(10000);
	pbx_test_validate(test, msg != NULL);
	sccp_actions_testhelper_callMessageHandler(s, d, msg);
	pbx_test_validate(test, c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE);
	pbx_test_status_update(test, "Tested handle_open_receive_channel_ack with callReference:%d, passThruPartyId:%d, mediastatus:%s(%d), result %s\n", c->callid, c->passthrupartyid, skinny_mediastatus2str(SKINNY_MEDIASTATUS_Ok), SKINNY_MEDIASTATUS_Ok, c->rtp.audio.receiveChannelState == SCCP_RTP_STATUS_INACTIVE ? "Success" : "Failure");
	/* End Test */

	/* Start Cleanup */
	if (msg) {
		sccp_free(msg);
	}

	sccp_channel_clean(c);
	sccp_channel_release(&c);
	pbx_test_validate(test, c == NULL);

	sccp_line_clean(l, TRUE);
	sccp_line_release(&l);
	pbx_test_validate(test, l == NULL);

	sccp_session_releaseDevice(s);
	sccp_session_testhelper_destroy_mocksession(s);
	sccp_free(s);
	d->session = NULL;

	sccp_dev_clean(d, TRUE);
	sccp_device_release(&d);
	pbx_test_validate(test, d == NULL);
	/* end Cleanup */

	return AST_TEST_PASS;
}
static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_test_actions_openChannelReceiveAck);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_test_actions_openChannelReceiveAck);
}
#endif
