/*!
 * \file        chan_sccp.c
 * \brief       An implementation of Skinny Client Control Protocol (SCCP)
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \brief       Main chan_sccp Class
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \remarks
 * Purpose:     This source file should be used only for asterisk module related content.
 * When to use: Methods communicating to asterisk about module initialization, status, destruction
 * Relations:   Main hub for all other sourcefiles.
 *
 * $Date$
 * $Revision$
 */

#include <config.h>
#include "common.h"

#include "sccp_pbx.h"
#include "sccp_protocol.h"
#include "sccp_socket.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_features.h"
#include "sccp_utils.h"
#include "sccp_indicate.h"
#include "sccp_hint.h"
#include "sccp_actions.h"
#include "sccp_featureButton.h"
#include "sccp_mwi.h"
#include "sccp_config.h"
#include "sccp_conference.h"
#include "sccp_labels.h"
#include "sccp_softkeys.h"
#include "sccp_cli.h"
#include "sccp_appfunctions.h"
#include "sccp_management.h"
#include "sccp_rtp.h"

#include "sccp_devstate.h"
#include "revision.h"
#include <signal.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$");

/*!
 * \brief       Buffer for Jitterbuffer use
 */
#if defined(__cplusplus) || defined(c_plusplus)
static ast_jb_conf default_jbconf = {
flags:	0,
max_size:-1,
resync_threshold:-1,
impl:	"",
#ifdef CS_AST_JB_TARGETEXTRA
target_extra:-1,
#endif
};
#else
static struct ast_jb_conf default_jbconf = {
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
	.impl = "",
#ifdef CS_AST_JB_TARGETEXTRA
	.target_extra = -1,
#endif
};
#endif

/*!
 * \brief       Global null frame
 */
static PBX_FRAME_TYPE sccp_null_frame;										/*!< Asterisk Structure */

/*!
 * \brief       Global variables
 */
struct sccp_global_vars *sccp_globals = 0;

/*!
 * \brief SCCP Request Channel
 * \param lineName              Line Name as Char
 * \param requestedCodec        Requested Skinny Codec
 * \param capabilities          Array of Skinny Codec Capabilities
 * \param capabilityLength      Length of Capabilities Array
 * \param autoanswer_type       SCCP Auto Answer Type
 * \param autoanswer_cause      SCCP Auto Answer Cause
 * \param ringermode            Ringer Mode
 * \param channel               SCCP Channel
 * \return SCCP Channel Request Status
 * 
 * \called_from_asterisk
 */
sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel)
{
	struct composedId lineSubscriptionId;
	sccp_channel_t *my_sccp_channel = NULL;
	AUTO_RELEASE sccp_line_t *l = NULL;

	memset(&lineSubscriptionId, 0, sizeof(struct composedId));

	if (!lineName) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	lineSubscriptionId = sccp_parseComposedId(lineName, 80);

	l = sccp_line_find_byname(lineSubscriptionId.mainId, FALSE);

	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineSubscriptionId.mainId);
		return SCCP_REQUEST_STATUS_LINEUNKNOWN;
	}
	sccp_log_and((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (SCCP_RWLIST_GETSIZE(&l->devices) == 0) {
		sccp_log((DEBUGCAT_DEVICE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		return SCCP_REQUEST_STATUS_LINEUNAVAIL;
	}
	sccp_log_and((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	*channel = my_sccp_channel = sccp_channel_allocate(l, NULL);

	if (!my_sccp_channel) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	/* set subscriberId for individual device addressing */
	if (!sccp_strlen_zero(lineSubscriptionId.subscriptionId.number)) {
		sccp_copy_string(my_sccp_channel->subscriptionId.number, lineSubscriptionId.subscriptionId.number, sizeof(my_sccp_channel->subscriptionId.number));
		if (!sccp_strlen_zero(lineSubscriptionId.subscriptionId.name)) {
			sccp_copy_string(my_sccp_channel->subscriptionId.name, lineSubscriptionId.subscriptionId.name, sizeof(my_sccp_channel->subscriptionId.name));
		} else {
			//pbx_log(LOG_NOTICE, "%s: calling subscriber id=%s\n", l->id, my_sccp_channel->subscriptionId.number);
		}
	} else {
		sccp_copy_string(my_sccp_channel->subscriptionId.number, l->defaultSubscriptionId.number, sizeof(my_sccp_channel->subscriptionId.number));
		sccp_copy_string(my_sccp_channel->subscriptionId.name, l->defaultSubscriptionId.name, sizeof(my_sccp_channel->subscriptionId.name));
		//pbx_log(LOG_NOTICE, "%s: calling all subscribers\n", l->id);
	}

	uint8_t size = (capabilityLength < sizeof(my_sccp_channel->remoteCapabilities.audio)) ? capabilityLength : sizeof(my_sccp_channel->remoteCapabilities.audio);

	memset(&my_sccp_channel->remoteCapabilities.audio, 0, sizeof(my_sccp_channel->remoteCapabilities.audio));
	memcpy(&my_sccp_channel->remoteCapabilities.audio, capabilities, size);

	/** set requested codec as prefered codec */
	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "prefered audio codec (%d)\n", requestedCodec);
	if (requestedCodec != SKINNY_CODEC_NONE) {
		my_sccp_channel->preferences.audio[0] = requestedCodec;
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "SCCP: prefered audio codec (%d)\n", my_sccp_channel->preferences.audio[0]);
	}

	/** done */

	my_sccp_channel->autoanswer_type = autoanswer_type;
	my_sccp_channel->autoanswer_cause = autoanswer_cause;
	my_sccp_channel->ringermode = ringermode;
	my_sccp_channel->hangupRequest = sccp_wrapper_asterisk_requestQueueHangup;
	return SCCP_REQUEST_STATUS_SUCCESS;
}

/*!
 * \brief Local Function to check for Valid Session, Message and Device
 * \param s SCCP Session
 * \param msg SCCP Msg
 * \param msgtypestr Message
 * \param deviceIsNecessary Is a valid device necessary for this message to be processed, if it is, the device is retain during execution of this particular message parser
 * \return -1 or Device;
 */
inline static sccp_device_t *check_session_message_device(sccp_session_t * s, sccp_msg_t * msg, const char *msgtypestr, boolean_t deviceIsNecessary)
{
	sccp_device_t *d = NULL;

	if (!GLOB(module_running)) {
		pbx_log(LOG_ERROR, "Chan-sccp-b module is not up and running at this moment\n");
		goto EXIT;
	}

	if (!s || (s->fds[0].fd < 0)) {
		pbx_log(LOG_ERROR, "(%s) Session no longer valid\n", msgtypestr);
		goto EXIT;
	}

	if (!msg) {
		pbx_log(LOG_ERROR, "(%s) No Message Provided\n", msgtypestr);
		goto EXIT;
	}

	if (deviceIsNecessary && !s->device) {
		pbx_log(LOG_WARNING, "No valid Session Device available to handle %s for, but device is needed\n", msgtypestr);
		goto EXIT;
	}
	if (deviceIsNecessary && !(d = sccp_device_retain(s->device))) {
		pbx_log(LOG_WARNING, "Session Device vould not be retained, to handle %s for, but device is needed\n", msgtypestr);
		goto EXIT;
	}

	if (deviceIsNecessary && d && d->session && s != d->session) {
		pbx_log(LOG_WARNING, "(%s) Provided Session and Device Session are not the same. Rejecting message handling\n", msgtypestr);
		sccp_session_crossdevice_cleanup(s, d->session, FALSE);
		d = d ? sccp_device_release(d) : NULL;
		goto EXIT;
	}

EXIT:
	if (msg && (GLOB(debug) & (DEBUGCAT_MESSAGE + DEBUGCAT_ACTION)) != 0) {
		uint32_t mid = letohl(msg->header.lel_messageId);

		pbx_log(LOG_NOTICE, "%s: SCCP Handle Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(d), msgtype2str(mid), mid, msg ? msg->header.length : 0);
		sccp_dump_msg(msg);
	}
	return d;
}

/*!
 * \brief SCCP Message Handler Callback
 *
 * Used to map SKinny Message Id's to their Handling Implementations
 */
struct messageMap_cb {
	void (*const messageHandler_cb) (sccp_session_t * s, sccp_device_t * d, sccp_msg_t * msg);
	boolean_t deviceIsNecessary;
};

static const struct messageMap_cb sccpMessagesCbMap[] = {
	[KeepAliveMessage] = {sccp_handle_KeepAliveMessage, FALSE},						// on 7985,6911 phones and tokenmsg, a KeepAliveMessage is send before register/token 
	[OffHookMessage] = {sccp_handle_offhook, TRUE},
	[OnHookMessage] = {sccp_handle_onhook, TRUE},
	[SoftKeyEventMessage] = {sccp_handle_soft_key_event, TRUE},
	[OpenReceiveChannelAck] = {sccp_handle_open_receive_channel_ack, TRUE},
	[OpenMultiMediaReceiveChannelAckMessage] = {sccp_handle_OpenMultiMediaReceiveAck, TRUE},
	[StartMediaTransmissionAck] = {sccp_handle_startmediatransmission_ack, TRUE},
	[IpPortMessage] = {NULL, TRUE},
	[VersionReqMessage] = {sccp_handle_version, TRUE},
	[CapabilitiesResMessage] = {sccp_handle_capabilities_res, TRUE},
	[ButtonTemplateReqMessage] = {sccp_handle_button_template_req, TRUE},
	[SoftKeyTemplateReqMessage] = {sccp_handle_soft_key_template_req, TRUE},
	[SoftKeySetReqMessage] = {sccp_handle_soft_key_set_req, TRUE},
	[LineStatReqMessage] = {sccp_handle_line_number, TRUE},
	[SpeedDialStatReqMessage] = {sccp_handle_speed_dial_stat_req, TRUE},
	[StimulusMessage] = {sccp_handle_stimulus, TRUE},
	[HeadsetStatusMessage] = {sccp_handle_headset, TRUE},
	[TimeDateReqMessage] = {sccp_handle_time_date_req, TRUE},
	[KeypadButtonMessage] = {sccp_handle_keypad_button, TRUE},
	[ConnectionStatisticsRes] = {sccp_handle_ConnectionStatistics, TRUE},
	[ServerReqMessage] = {sccp_handle_ServerResMessage, TRUE},
	[ConfigStatReqMessage] = {sccp_handle_ConfigStatMessage, TRUE},
	[EnblocCallMessage] = {sccp_handle_EnblocCallMessage, TRUE},
	[RegisterAvailableLinesMessage] = {sccp_handle_AvailableLines, TRUE},
	[ForwardStatReqMessage] = {sccp_handle_forward_stat_req, TRUE},
	[FeatureStatReqMessage] = {sccp_handle_feature_stat_req, TRUE},
	[ServiceURLStatReqMessage] = {sccp_handle_services_stat_req, TRUE},
	[AccessoryStatusMessage] = {sccp_handle_accessorystatus_message, TRUE},
	[DialedPhoneBookMessage] = {sccp_handle_dialedphonebook_message, TRUE},
	[UpdateCapabilitiesMessage] = {sccp_handle_updatecapabilities_message, TRUE},
	[MediaPathCapabilityMessage] = {sccp_handle_unknown_message, TRUE},
	[DisplayDynamicNotifyMessage] = {sccp_handle_unknown_message, TRUE},
	[DisplayDynamicPriNotifyMessage] = {sccp_handle_unknown_message, TRUE},
	[ExtensionDeviceCaps] = {sccp_handle_unknown_message, TRUE},
	[DeviceToUserDataVersion1Message] = {sccp_handle_device_to_user, TRUE},
	[DeviceToUserDataResponseVersion1Message] = {sccp_handle_device_to_user_response, TRUE},
	[RegisterTokenRequest] = {sccp_handle_token_request, FALSE},
	[UnregisterMessage] = {sccp_handle_unregister, FALSE},
	[RegisterMessage] = {sccp_handle_register, FALSE},
	[AlarmMessage] = {sccp_handle_alarm, FALSE},
	[XMLAlarmMessage] = {sccp_handle_XMLAlarmMessage, FALSE},
	[StartMultiMediaTransmissionAck] = {sccp_handle_startmultimediatransmission_ack, TRUE},
	[MediaTransmissionFailure] = {sccp_handle_mediatransmissionfailure, TRUE},
	[MiscellaneousCommandMessage] = {sccp_handle_miscellaneousCommandMessage, TRUE},
};

static const struct messageMap_cb spcpMessagesCbMap[] = {
	[SPCPRegisterTokenRequest - SPCP_MESSAGE_OFFSET] = {sccp_handle_SPCPTokenReq, FALSE},
};
/*!
 * \brief       Controller function to handle Received Messages
 * \param       msg Message as sccp_msg_t
 * \param       s Session as sccp_session_t
 */
int sccp_handle_message(sccp_msg_t * msg, sccp_session_t * s)
{
	const struct messageMap_cb *messageMap_cb = NULL;
	uint32_t mid = 0;
	sccp_device_t *device = NULL;

	if (!s) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_handle_message) Client does not have a session which is required. Exiting sccp_handle_message !\n");
		if (msg) {
			sccp_free(msg);
		}
		return -1;
	}

	if (!msg) {
		pbx_log(LOG_ERROR, "%s: (sccp_handle_message) No Message Specified.\n which is required, Exiting sccp_handle_message !\n", DEV_ID_LOG(s->device));
		return -2;
	}

	mid = letohl(msg->header.lel_messageId);

	/* search for message handler */
	if (mid >= SPCP_MESSAGE_OFFSET) {
		messageMap_cb = &spcpMessagesCbMap[mid - SPCP_MESSAGE_OFFSET]; 
	} else {
		messageMap_cb = &sccpMessagesCbMap[mid];
	}
	sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: >> Got message %s (%x)\n", DEV_ID_LOG(s->device), msgtype2str(mid), mid);

	/* we dont know how to handle event */
	if (!messageMap_cb) {
		pbx_log(LOG_WARNING, "SCCP: Unknown Message %x. Don't know how to handle it. Skipping.\n", mid);
		sccp_handle_unknown_message(s, device, msg);
		return 0;
	}

	device = check_session_message_device(s, msg, msgtype2str(mid), messageMap_cb->deviceIsNecessary);	/* retained device returned */

	if (messageMap_cb->messageHandler_cb && messageMap_cb->deviceIsNecessary == TRUE && !device) {
		pbx_log(LOG_ERROR, "SCCP: Device is required to handle this message %s(%x), but none is provided. Exiting sccp_handle_message\n", msgtype2str(mid), mid);
		return -3;
	}
	if (messageMap_cb->messageHandler_cb) {
		messageMap_cb->messageHandler_cb(s, device, msg);
	}
	s->lastKeepAlive = time(0);

	if (device && device->registrationState == SKINNY_DEVICE_RS_PROGRESS && mid == device->protocol->registrationFinishedMessageId) {
		sccp_dev_set_registered(device, SKINNY_DEVICE_RS_OK);
		char servername[StationMaxDisplayNotifySize];

		snprintf(servername, sizeof(servername), "%s %s", GLOB(servername), SKINNY_DISP_CONNECTED);
		sccp_dev_displaynotify(device, servername, 5);
	}
	device = device ? sccp_device_release(device) : NULL;
	return 0;
}

/**
 * \brief load the configuration from sccp.conf
 * \todo should be pbx independent
 */
int load_config(void)
{
	int oldPort = 0;											//ntohs(GLOB(bindaddr));
	int newPort = 0;
	int on = 1;
	char addrStr[INET6_ADDRSTRLEN];

	oldPort = sccp_socket_getPort(&GLOB(bindaddr));

	/* Copy the default jb config over global_jbconf */
	memcpy(&GLOB(global_jbconf), &default_jbconf, sizeof(struct ast_jb_conf));

	/* Setup the monitor thread default */
#if ASTERISK_VERSION_GROUP < 110
	GLOB(monitor_thread) = AST_PTHREADT_NULL;								// ADDED IN SVN 414 -FS
#endif
	GLOB(mwiMonitorThread) = AST_PTHREADT_NULL;

	memset(&GLOB(bindaddr), 0, sizeof(GLOB(bindaddr)));
	GLOB(allowAnonymous) = TRUE;

#ifdef CS_SCCP_REALTIME
	sccp_copy_string(GLOB(realtimedevicetable), "sccpdevice", sizeof(GLOB(realtimedevicetable)));
	sccp_copy_string(GLOB(realtimelinetable), "sccpline", sizeof(GLOB(realtimelinetable)));
#endif

#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Platform byte order   : LITTLE ENDIAN\n");
#else
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Platform byte order   : BIG ENDIAN\n");
#endif
	if (sccp_config_getConfig(TRUE) > CONFIG_STATUS_FILE_OK) {
		pbx_log(LOG_ERROR, "Error loading configfile !");
		return FALSE;
	}

	if (!sccp_config_general(SCCP_CONFIG_READINITIAL)) {
		pbx_log(LOG_ERROR, "Error parsing configfile !");
		return 0;
	}
	sccp_config_readDevicesLines(SCCP_CONFIG_READINITIAL);

	/* ok the config parse is done */
	newPort = sccp_socket_getPort(&GLOB(bindaddr));
	if ((GLOB(descriptor) > -1) && (newPort != oldPort)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

	if (GLOB(descriptor) < 0) {
		int status;
		struct addrinfo hints, *res;
		char port_str[15] = "";

		memset(&hints, 0, sizeof hints);								// make sure the struct is empty
		hints.ai_family = AF_UNSPEC;									// don't care IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM;								// TCP stream sockets
		hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;					// fill in my IP for me

		if (sccp_socket_getPort(&GLOB(bindaddr)) > 0) {
			snprintf(port_str, sizeof(port_str), "%d", sccp_socket_getPort(&GLOB(bindaddr)));
		} else {
			snprintf(port_str, sizeof(port_str), "%s", "cisco-sccp");
		}

		sccp_copy_string(addrStr, sccp_socket_stringify_addr(&GLOB(bindaddr)), sizeof(addrStr));

		if ((status = getaddrinfo(sccp_socket_stringify_addr(&GLOB(bindaddr)), port_str, &hints, &res)) != 0) {
			pbx_log(LOG_WARNING, "Failed to get addressinfo for %s:%s, error: %s!\n", sccp_socket_stringify_addr(&GLOB(bindaddr)), port_str, gai_strerror(status));
			close(GLOB(descriptor));
			GLOB(descriptor) = -1;
			return 0;
		}
		GLOB(descriptor) = socket(res->ai_family, res->ai_socktype, res->ai_protocol);			// need to add code to handle multiple interfaces (multi homed server) -> multiple socket descriptors

		on = 1;

		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
			pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
		}
		if (setsockopt(GLOB(descriptor), IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0) {
			pbx_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
		} else if (GLOB(sccp_tos)) {
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_1 "Using SCCP Socket ToS mark %d\n", GLOB(sccp_tos));
		}
		if (setsockopt(GLOB(descriptor), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) {
			pbx_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
		}
#if defined(linux)
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0) {
			pbx_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
		} else if (GLOB(sccp_cos)) {
			sccp_log((DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_1 "Using SCCP Socket CoS mark %d\n", GLOB(sccp_cos));
		}
#endif

		if (GLOB(descriptor) < 0) {
			pbx_log(LOG_WARNING, "Unable to create SCCP socket: %s\n", strerror(errno));
		} else {
			/* get ip-address string */
			if (bind(GLOB(descriptor), res->ai_addr, res->ai_addrlen) < 0) {
				pbx_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n", addrStr, sccp_socket_getPort(&GLOB(bindaddr)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", addrStr, sccp_socket_getPort(&GLOB(bindaddr)));

			sccp_socket_setoptions(GLOB(descriptor));
			
			if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
				pbx_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", addrStr, sccp_socket_getPort(&GLOB(bindaddr)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", addrStr, sccp_socket_getPort(&GLOB(bindaddr)));
			GLOB(reload_in_progress) = FALSE;
			pbx_pthread_create(&GLOB(socket_thread), NULL, sccp_socket_thread, NULL);

		}
		freeaddrinfo(res);
	}

	return 0;
}

/*!
 * \brief       Load the actual chan_sccp module
 * \return      Success as int
 */
boolean_t sccp_prePBXLoad(void)
{
	pbx_log(LOG_NOTICE, "preloading pbx module\n");

	/* make globals */
	sccp_globals = (struct sccp_global_vars *) sccp_malloc(sizeof(struct sccp_global_vars));
	if (!sccp_globals) {
		pbx_log(LOG_ERROR, "No free memory for SCCP global vars. SCCP channel type disabled\n");
		return FALSE;
	}

	/* Initialize memory */
	memset(&sccp_null_frame, 0, sizeof(sccp_null_frame));
	memset(sccp_globals, 0, sizeof(struct sccp_global_vars));
	GLOB(debug) = DEBUGCAT_CORE;

	//sccp_event_listeners = (struct sccp_event_subscriptions *)sccp_malloc(sizeof(struct sccp_event_subscriptions));
	//memset(sccp_event_listeners, 0, sizeof(struct sccp_event_subscriptions));
	//SCCP_LIST_HEAD_INIT(&sccp_event_listeners->subscriber);

	pbx_mutex_init(&GLOB(lock));
	pbx_mutex_init(&GLOB(usecnt_lock));
#if ASTERISK_VERSION_GROUP < 110
	pbx_mutex_init(&GLOB(monitor_lock));
#endif

	/* init refcount */
	sccp_refcount_init();

	SCCP_RWLIST_HEAD_INIT(&GLOB(sessions));
	SCCP_RWLIST_HEAD_INIT(&GLOB(devices));
	SCCP_RWLIST_HEAD_INIT(&GLOB(lines));

	GLOB(general_threadpool) = sccp_threadpool_init(THREADPOOL_MIN_SIZE);

	sccp_event_module_start();
#if defined(CS_DEVSTATE_FEATURE)
	sccp_devstate_module_start();
#endif
	sccp_mwi_module_start();
	sccp_hint_module_start();
	sccp_manager_module_start();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_start();
#endif
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay, TRUE);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend, TRUE);

	GLOB(descriptor) = -1;

	//GLOB(bindaddr.sin_port) = DEFAULT_SCCP_PORT;
	GLOB(bindaddr).ss_family = AF_INET;
	((struct sockaddr_in *) &GLOB(bindaddr))->sin_port = DEFAULT_SCCP_PORT;

	GLOB(externrefresh) = 60;
	GLOB(keepalive) = SCCP_KEEPALIVE;
	//sccp_copy_string(GLOB(dateformat), "M/D/YA", sizeof(GLOB(dateformat)));
	//sccp_copy_string(GLOB(context), "default", sizeof(GLOB(context)));
	//sccp_copy_string(GLOB(servername), "Asterisk", sizeof(GLOB(servername)));

	/* Wait up to 16 seconds for first digit */
	GLOB(firstdigittimeout) = 16;
	/* How long to wait for following digits */
	GLOB(digittimeout) = 8;

	GLOB(debug) = 1;
	GLOB(sccp_tos) = (0x68 & 0xff);										// AF31
	GLOB(audio_tos) = (0xB8 & 0xff);									// EF
	GLOB(video_tos) = (0x88 & 0xff);									// AF41
	GLOB(sccp_cos) = 4;
	GLOB(audio_cos) = 6;
	GLOB(video_cos) = 5;
	GLOB(echocancel) = TRUE;
	GLOB(silencesuppression) = TRUE;
	GLOB(dndFeature) = TRUE;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = TRUE;											/* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	GLOB(amaflags) = pbx_channel_string2amaflag("documentation");
	GLOB(callanswerorder) = SCCP_ANSWER_OLDEST_FIRST;
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	GLOB(earlyrtp) = SCCP_EARLYRTP_PROGRESS;
	sccp_create_hotline();
	return TRUE;
}

boolean_t sccp_postPBX_load(void)
{
	pbx_mutex_lock(&GLOB(lock));

	// initialize SCCP_REVISIONSTR and SCCP_REVISIONSTR
#ifdef VCS_SHORT_HASH
#ifdef VCS_WC_MODIFIED
	sprintf(SCCP_REVISIONSTR, "%sM", VCS_SHORT_HASH);
#else
	sprintf(SCCP_REVISIONSTR, "%s", VCS_SHORT_HASH);
#endif
#else
	sprintf(SCCP_REVISIONSTR, "%s", SCCP_REVISION);
#endif
	sprintf(SCCP_VERSIONSTR, "Skinny Client Control Protocol (SCCP). Release: %s %s - %s (built by '%s' on '%s')\n", SCCP_VERSION, SCCP_BRANCH, SCCP_REVISIONSTR, BUILD_USER, BUILD_DATE);

#if CS_TEST_FRAMEWORK
	sccp_utils_register_tests();
#endif
	GLOB(module_running) = TRUE;
	sccp_refcount_schedule_cleanup((const void *) 0);
	pbx_mutex_unlock(&GLOB(lock));
	return TRUE;
}

/*!
 * \brief Schedule free memory
 * \param ptr pointer
 * \return Success as int
 */
int sccp_sched_free(void *ptr)
{
	if (!ptr) {
		return -1;
	}
	sccp_free(ptr);
	return 0;

}

/*!
 * \brief PBX Independent Function to be called before unloading the module
 * \return Success as int
 */
int sccp_preUnload(void)
{
	pbx_mutex_lock(&GLOB(lock));
	GLOB(module_running) = FALSE;
	pbx_mutex_unlock(&GLOB(lock));

	sccp_device_t *d;
	sccp_line_t *l;
	sccp_session_t *s;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unloading Module\n");

#if CS_TEST_FRAMEWORK
	sccp_utils_unregister_tests();
#endif

	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Descriptor\n");
	close(GLOB(descriptor));
	GLOB(descriptor) = -1;

	//! \todo make this pbx independend
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channels\n");

	/* removing devices */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Devices\n");
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(devices), d, list) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		d->realtime = TRUE;										/* use realtime, to fully clear the device configuration */
		sccp_dev_clean(d, TRUE, 0);									// performs a device reset if it has a session
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_RWLIST_EMPTY(&GLOB(devices))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(devices));
	}

	/* hotline will be removed by line removing function */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Lines\n");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Removing Hotline\n");
	sccp_line_removeFromGlobals(GLOB(hotline)->line);
	GLOB(hotline)->line = sccp_line_release(GLOB(hotline)->line);
	sccp_free(GLOB(hotline));

	/* removing lines */
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Removing line %s\n", l->name);
		sccp_line_clean(l, TRUE);
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_RWLIST_EMPTY(&GLOB(lines))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(lines));
	}
	usleep(100);												// wait for events to finalize

	/* removing sessions */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Sessions\n");
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(sessions), s, list) {
		sccp_socket_stop_sessionthread(s, SKINNY_DEVICE_RS_NONE);
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_LIST_EMPTY(&GLOB(sessions))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(sessions));
	}

	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killing the socket thread\n");
	sccp_globals_lock(socket_lock);
	if ((GLOB(socket_thread) != AST_PTHREADT_NULL) && (GLOB(socket_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(socket_thread));
		pthread_kill(GLOB(socket_thread), SIGURG);
		pthread_join(GLOB(socket_thread), NULL);
	}
	GLOB(socket_thread) = AST_PTHREADT_STOP;
	sccp_globals_unlock(socket_lock);

	sccp_manager_module_stop();
	sccp_devstate_module_stop();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_stop();
#endif
	sccp_softkey_clear();

	sccp_mutex_destroy(&GLOB(socket_lock));
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the socket thread\n");

	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing bind\n");
	if (GLOB(ha)) {
		sccp_free_ha(GLOB(ha));
	}
	if (GLOB(localaddr)) {
		sccp_free_ha(GLOB(localaddr));
	}
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing io/sched\n");

	sccp_hint_module_stop();
	sccp_event_module_stop();

	sccp_threadpool_destroy(GLOB(general_threadpool));
	sccp_log((DEBUGCAT_CORE + DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the threadpool\n");
	sccp_refcount_destroy();
	if (GLOB(config_file_name)) {
		sccp_free(GLOB(config_file_name));
	}
	//if (GLOB(token_fallback)) {
	//  sccp_free(GLOB(token_fallback));
	//}
	sccp_config_cleanup_dynamically_allocated_memory(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT);

	pbx_mutex_destroy(&GLOB(usecnt_lock));
	pbx_mutex_destroy(&GLOB(lock));
	//pbx_log(LOG_NOTICE, "SCCP chan_sccp unloaded\n");
	return 0;
}

/*!
 * \brief PBX Independent Function to be called when starting module reload
 * \return Success as int
 */
int sccp_reload(void)
{
	sccp_readingtype_t readingtype;
	int returnval = 0;

	pbx_mutex_lock(&GLOB(lock));
	if (GLOB(reload_in_progress) == TRUE) {
		pbx_log(LOG_ERROR, "SCCP reloading already in progress.\n");
		returnval = 1;
		goto EXIT;
	}

	sccp_config_file_status_t cfg = sccp_config_getConfig(FALSE);

	switch (cfg) {
		case CONFIG_STATUS_FILE_NOT_CHANGED:
			pbx_log(LOG_NOTICE, "config file '%s' has not change, skipping reload.\n", GLOB(config_file_name));
			returnval = 0;
			break;
		case CONFIG_STATUS_FILE_OK:
			pbx_log(LOG_NOTICE, "SCCP reloading configuration.\n");
			readingtype = SCCP_CONFIG_READRELOAD;
			GLOB(reload_in_progress) = TRUE;
			if (!sccp_config_general(readingtype)) {
				pbx_log(LOG_ERROR, "Unable to reload configuration.\n");
				returnval = 2;
				break;
			}
			sccp_config_readDevicesLines(readingtype);
			returnval = 3;
			break;
		case CONFIG_STATUS_FILE_OLD:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_NOT_SCCP:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "\n\n --> You are using an configuration file is not following the sccp format, please check '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_NOT_FOUND:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_INVALID:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
			returnval = 4;
			break;
	}
EXIT:
	GLOB(reload_in_progress) = FALSE;
	pbx_mutex_unlock(&GLOB(lock));
	return returnval;
}

#ifdef CS_DEVSTATE_FEATURE
const char devstate_db_family[] = "CustomDevstate";
#endif

/*
 * deprecated
 */
int sccp_updateExternIp(void)
{
	/* setup hostname -> externip */
	/*! \todo change using singular h_addr to h_addr_list (name may resolve to multiple ip-addresses */
	/*
	   struct ast_hostent ahp;
	   struct hostent *hp;
	   if (!sccp_strlen_zero(GLOB(externhost))) {
	   if (!(hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
	   pbx_log(LOG_WARNING, "Invalid address resolution for externhost keyword: %s\n", GLOB(externhost));
	   } else {
	   memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
	   time(&GLOB(externexpire));
	   }
	   }
	 */
	return 0;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
