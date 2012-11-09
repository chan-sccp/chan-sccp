
/*!
 * \file 	chan_sccp.c
 * \brief 	An implementation of Skinny Client Control Protocol (SCCP)
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \brief 	Main chan_sccp Class
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \remarks	Purpose: 	This source file should be used only for asterisk module related content.
 * 		When to use:	Methods communicating to asterisk about module initialization, status, destruction
 *   		Relationships: 	Main hub for all other sourcefiles.
 *
 * $Date$
 * $Revision$
 */

//#define AST_MODULE "chan_sccp"

#include <config.h>
#include "common.h"
#include <signal.h>

SCCP_FILE_VERSION(__FILE__, "$Revision$")

/*!
 * \brief	Buffer for Jitterbuffer use
 */
#if defined(__cplusplus) || defined(c_plusplus)
static ast_jb_conf default_jbconf = {
 flags:0,
 max_size:-1,
 resync_threshold:-1,
 impl:	"",
#    ifdef CS_AST_JB_TARGET_EXTRA
 target_extra:-1,
#    endif
};
#else
static struct ast_jb_conf default_jbconf = {
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
	.impl = "",
#    ifdef CS_AST_JB_TARGET_EXTRA
	.target_extra = -1,
#    endif
};
#endif

/*!
 * \brief	Global null frame
 */
PBX_FRAME_TYPE sccp_null_frame;							/*!< Asterisk Structure */

/*!
 * \brief	Global variables
 */
struct sccp_global_vars *sccp_globals = 0;

/*!
 * \brief	SCCP Request Channel
 * \param	lineName 		Line Name as Char
 * \param	requestedCodec 		Requested Skinny Codec
 * \param 	capabilities		Array of Skinny Codec Capabilities
 * \param 	capabilityLength 	Length of Capabilities Array
 * \param	autoanswer_type 	SCCP Auto Answer Type
 * \param	autoanswer_cause 	SCCP Auto Answer Cause
 * \param 	ringermode		Ringer Mode
 * \param 	channel			SCCP Channel
 * \return	SCCP Channel Request Status
 * 
 * \called_from_asterisk
 */
sccp_channel_request_status_t sccp_requestChannel(const char *lineName, skinny_codec_t requestedCodec, skinny_codec_t capabilities[], uint8_t capabilityLength, sccp_autoanswer_type_t autoanswer_type, uint8_t autoanswer_cause, int ringermode, sccp_channel_t ** channel)
{
	struct composedId lineSubscriptionId;
	sccp_channel_t *my_sccp_channel = NULL;
	sccp_line_t *l = NULL;

	memset(&lineSubscriptionId, 0, sizeof(struct composedId));

	if (!lineName) {
		return SCCP_REQUEST_STATUS_ERROR;
	}

	lineSubscriptionId = sccp_parseComposedId(lineName, 80);

	l = sccp_line_find_byname(lineSubscriptionId.mainId);

	if (!l) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineSubscriptionId.mainId);
		return SCCP_REQUEST_STATUS_LINEUNKNOWN;
	}
	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (l->devices.size == 0) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		l = sccp_line_release(l);
		return SCCP_REQUEST_STATUS_LINEUNAVAIL;
	}
	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	*channel = my_sccp_channel = sccp_channel_allocate(l, NULL);

	if (!my_sccp_channel) {
		l = sccp_line_release(l);
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
	sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "prefered audio codec (%d)\n", requestedCodec);
	if (requestedCodec != SKINNY_CODEC_NONE) {

		/** do not set write/read format, because we do not know the device capabilities on shared lines -MC */
//              my_sccp_channel->rtp.audio.writeFormat = requestedCodec;
//              my_sccp_channel->rtp.audio.writeState = SCCP_RTP_STATUS_REQUESTED;
		my_sccp_channel->preferences.audio[0] = requestedCodec;
		sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "SCCP: prefered audio codec (%d)\n", my_sccp_channel->preferences.audio[0]);
	}
//      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "requested codec (%d)\n", my_sccp_channel->rtp.audio.writeFormat);

	/** done */

	my_sccp_channel->autoanswer_type = autoanswer_type;
	my_sccp_channel->autoanswer_cause = autoanswer_cause;
	my_sccp_channel->ringermode = ringermode;
	l = sccp_line_release(l);

	return SCCP_REQUEST_STATUS_SUCCESS;
}

/*!
 * \brief returns the state of device
 * \param data name of device
 * \return devicestate of AST_DEVICE_*
 *
 * \called_from_asterisk
 * 
 * \warning
 * 	- line->devices is not always locked
 */
int sccp_devicestate(void *data)
{
	sccp_line_t *l = NULL;
	int res = AST_DEVICE_UNKNOWN;
	char *lineName = (char *)data, *options = NULL;

	/* exclude options */
	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	if ((l = sccp_line_find_byname(lineName))) {
		if (SCCP_LIST_FIRST(&l->devices) == NULL)
			res = AST_DEVICE_UNAVAILABLE;

		//! \todo handle dnd on device
		//      else if ((l->device->dnd && l->device->dndmode == SCCP_DNDMODE_REJECT)
		//                      || (l->dnd && (l->dndmode == SCCP_DNDMODE_REJECT
		//                                      || (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT) )) )
		//              res = AST_DEVICE_BUSY;
		else if (l->incominglimit && SCCP_RWLIST_GETSIZE(l->channels) == l->incominglimit)
			res = AST_DEVICE_BUSY;
		else if (!SCCP_RWLIST_GETSIZE(l->channels))
			res = AST_DEVICE_NOT_INUSE;
#ifdef CS_AST_DEVICE_RINGING
		else if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGING))
#    ifdef CS_AST_DEVICE_RINGINUSE
			if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED))
				res = AST_DEVICE_RINGINUSE;
			else
#    endif
				res = AST_DEVICE_RINGING;
#endif
#ifdef CS_AST_DEVICE_ONHOLD
		else if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))
			res = AST_DEVICE_ONHOLD;
#endif
		else
			res = AST_DEVICE_INUSE;

		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE | DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked for the device state (%d) of the line %s\n", res, (char *)data);
		l = sccp_line_release(l);
	} else {
		res = AST_DEVICE_INVALID;
	}

	return res;
}

/*!
 * \brief Local Function to check for Valid Session, Message and Device
 * \param s SCCP Session
 * \param r SCCP Moo
 * \param msg Message
 * \return -1 or Device;
 */
inline static sccp_device_t *check_session_message_device(sccp_session_t * s, sccp_moo_t * r, const char *msg, boolean_t deviceIsNecessary)
{
	sccp_device_t *d = NULL;

	if (!GLOB(module_running)) {
		pbx_log(LOG_ERROR, "Chan-sccp-b module is not up and running at this moment\n");
		goto EXIT;
	}

	if (!s || (s->fds[0].fd < 0)) {
		pbx_log(LOG_ERROR, "(%s) Session no longer valid\n", msg);
		goto EXIT;
	}

	if (!r) {
		pbx_log(LOG_ERROR, "(%s) No Message Provided\n", msg);
		goto EXIT;
	}

	if (deviceIsNecessary && !s->device) {
		pbx_log(LOG_WARNING, "No valid Session Device available to handle %s for, but device is needed\n", msg);
		goto EXIT;
	}
	if (deviceIsNecessary && !(d = sccp_device_retain(s->device))) {
		pbx_log(LOG_WARNING, "Session Device vould not be retained, to handle %s for, but device is needed\n", msg);
		goto EXIT;
	}

	if (deviceIsNecessary && d && d->session && s != d->session) {
		pbx_log(LOG_WARNING, "(%s) Provided Session and Device Session are not the same. Rejecting message handling\n", msg);
		s = sccp_session_crossdevice_cleanup(s, d, "No Crossover Allowed -> Reset");
		d = d ? sccp_device_release(d) : NULL;
		goto EXIT;
	}

 EXIT:
	if ((GLOB(debug) & (DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) != 0) {
		uint32_t mid = letohl(r->lel_messageId);

		pbx_log(LOG_NOTICE, "%s: SCCP Handle Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(d), mid ? message2str(mid) : NULL, mid ? mid : 0, r ? r->length : 0);
		sccp_dump_packet((unsigned char *)&r->msg, (r->length < SCCP_MAX_PACKET) ? (int)r->length : (int)SCCP_MAX_PACKET);
	}
	return d;
}

/*!
 * \brief SCCP Message Handler Callback
 *
 * Used to map SKinny Message Id's to their Handling Implementations
 */
struct sccp_messageMap_cb {
	void (*const messageHandler_cb) (sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
	boolean_t deviceIsNecessary;
};
typedef struct sccp_messageMap_cb sccp_messageMap_cb_t;

static const struct sccp_messageMap_cb messagesCbMap[] = {
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
	[Unknown_0x004A_Message] = {sccp_handle_unknown_message, TRUE},
	[DisplayDynamicNotifyMessage] = {sccp_handle_unknown_message, TRUE},
	[DisplayDynamicPriNotifyMessage] = {sccp_handle_unknown_message, TRUE},
	[SpeedDialStatDynamicMessage] = {sccp_handle_speed_dial_stat_req, TRUE},
	[ExtensionDeviceCaps] = {sccp_handle_unknown_message, TRUE},
	[DeviceToUserDataVersion1Message] = {sccp_handle_device_to_user, TRUE},
	[DeviceToUserDataResponseVersion1Message] = {sccp_handle_device_to_user_response, TRUE},
	[RegisterTokenRequest] = {sccp_handle_token_request, FALSE},
	[UnregisterMessage] = {sccp_handle_unregister, FALSE},
	[RegisterMessage] = {sccp_handle_register, FALSE},
	[AlarmMessage] = {sccp_handle_alarm, FALSE},
	[XMLAlarmMessage] = {sccp_handle_XMLAlarmMessage, FALSE},
	[SPCPRegisterTokenRequest] = {sccp_handle_SPCPTokenReq, FALSE},
	[StartMultiMediaTransmissionAck] = {sccp_handle_startmultimediatransmission_ack, TRUE},
	[MediaTransmissionFailure] = {sccp_handle_mediatransmissionfailure, TRUE},
	[MiscellaneousCommandMessage] = {sccp_handle_miscellaneousCommandMessage, TRUE},
};

/*!
 * \brief 	Controller function to handle Received Messages
 * \param 	r Message as sccp_moo_t
 * \param 	s Session as sccp_session_t
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s)
{
	const sccp_messageMap_cb_t *messageMap_cb = NULL;
	uint32_t mid = 0;
	sccp_device_t *device = NULL;

	if (!s) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_handle_message) Client does not have a session which is required. Exiting sccp_handle_message !\n");
		if (r) {
			sccp_free(r);
		}
		return -1;
	}

	if (!r) {
		pbx_log(LOG_ERROR, "%s: (sccp_handle_message) No Message Specified.\n which is required, Exiting sccp_handle_message !\n", DEV_ID_LOG(s->device));
		return -1;
	}

	mid = letohl(r->lel_messageId);

	/* search for message handler */
	messageMap_cb = &messagesCbMap[mid];
	sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: >> Got message %s (%x)\n", DEV_ID_LOG(s->device), message2str(mid), mid);

	/* we dont know how to handle event */
	if (!messageMap_cb) {
		pbx_log(LOG_WARNING, "SCCP: Unknown Message %x. Don't know how to handle it. Skipping.\n", mid);
		sccp_handle_unknown_message(s, device, r);
		return 1;
	}

	device = check_session_message_device(s, r, message2str(mid), messageMap_cb->deviceIsNecessary);	/* retained device returned */

	if (messageMap_cb->messageHandler_cb && messageMap_cb->deviceIsNecessary == TRUE && !device) {
		pbx_log(LOG_ERROR, "SCCP: Device is required to handle this message %s(%x), but none is provided. Exiting sccp_handle_message\n", message2str(mid), mid);
		return 0;
	}
	if (messageMap_cb->messageHandler_cb) {
		messageMap_cb->messageHandler_cb(s, device, r);
	}
	s->lastKeepAlive = time(0);

	device = device ? sccp_device_release(device) : NULL;
	return 1;
}

/**
 * \brief load the configuration from sccp.conf
 * \todo should be pbx independent
 */
int load_config(void)
{
	int oldport = ntohs(GLOB(bindaddr.sin_port));
	int on = 1;

	/* Copy the default jb config over global_jbconf */
	memcpy(&GLOB(global_jbconf), &default_jbconf, sizeof(struct ast_jb_conf));

	/* Setup the monitor thread default */
	GLOB(monitor_thread) = AST_PTHREADT_NULL;								// ADDED IN SVN 414 -FS
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
	if (sccp_config_getConfig(FALSE) > CONFIG_STATUS_FILE_OK) {
		pbx_log(LOG_ERROR, "Error loading configfile !");
		return FALSE;
	}

	if (!sccp_config_general(SCCP_CONFIG_READINITIAL)) {
		pbx_log(LOG_ERROR, "Error parsing configfile !");
		return 0;
	}
	sccp_config_readDevicesLines(SCCP_CONFIG_READINITIAL);
	/* ok the config parse is done */
	if ((GLOB(descriptor) > -1) && (ntohs(GLOB(bindaddr.sin_port)) != oldport)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

	if (GLOB(descriptor) < 0) {
#ifdef CS_EXPERIMENTAL_NEWIP
		int status;
		struct addrinfo hints, *res;
		char port_str[5] = "";

		memset(&hints, 0, sizeof hints);								// make sure the struct is empty
		hints.ai_family = AF_UNSPEC;									// don't care IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM;								// TCP stream sockets

		if (&GLOB(bindaddr.sin_addr) != NULL) {
			snprintf(port_str, sizeof(port_str), "%d", ntohs(GLOB(bindaddr.sin_port)));
			if ((status = getaddrinfo(pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), port_str, &hints, &res)) != 0) {
				pbx_log(LOG_WARNING, "Failed to get addressinfo for %s:%d, error: %s!\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), gai_strerror(status));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
		} else {
			hints.ai_flags = AI_PASSIVE;								// fill in my IP for me
			if ((status = getaddrinfo(NULL, "cisco_sccp", &hints, &res)) != 0) {
				pbx_log(LOG_WARNING, "Failed to get addressinfo, error: %s!\n", gai_strerror(status));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
		}
		GLOB(descriptor) = socket(res->ai_family, res->ai_socktype, res->ai_protocol);			// need to add code to handle multiple interfaces (multi homed server) -> multiple socket descriptors
#else
		GLOB(descriptor) = socket(AF_INET, SOCK_STREAM, 0);						//replaced
#endif
		on = 1;
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			pbx_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
		if (setsockopt(GLOB(descriptor), IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0)
			pbx_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
		else if (GLOB(sccp_tos))
			sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_1 "Using SCCP Socket ToS mark %d\n", GLOB(sccp_tos));
		if (setsockopt(GLOB(descriptor), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
			pbx_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
#if defined(linux)
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0)
			pbx_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
		else if (GLOB(sccp_cos))
			sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_1 "Using SCCP Socket CoS mark %d\n", GLOB(sccp_cos));
#endif

		if (GLOB(descriptor) < 0) {
			pbx_log(LOG_WARNING, "Unable to create SCCP socket: %s\n", strerror(errno));
		} else {
#ifdef CS_EXPERIMENTAL_NEWIP
			if (bind(GLOB(descriptor), res->ai_addr, res->ai_addrlen) < 0) {			// using addrinfo hints
#else
			if (bind(GLOB(descriptor), (struct sockaddr *)&GLOB(bindaddr), sizeof(GLOB(bindaddr))) < 0) {	//replaced
#endif
				pbx_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));

			if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
				pbx_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
			GLOB(reload_in_progress) = FALSE;
			pbx_pthread_create(&GLOB(socket_thread), NULL, sccp_socket_thread, NULL);

		}
#ifdef CS_EXPERIMENTAL_NEWIP
		freeaddrinfo(res);
#endif
	}
	//! \todo how can we handle this ?
	//sccp_restart_monitor();

	return 0;
}

/*!
 * \brief Add Callback Functon to PBX Scheduler
 * \param when number of seconds from this point in time as int
 * \param callback CallBack Function to be called when the time has passed
 * \param data	Extraneous Data 
 * \return sceduled id as int
 */
int sccp_sched_add(int when, sccp_sched_cb callback, const void *data)
{

	if (!PBX(sched_add))
		return 1;

	return PBX(sched_add) (when, callback, data);
}

/*!
 * \brief Remove Callback Functon from PBX Scheduler
 * \param id ID of scheduled callback as int
 * \return success as int
 */
int sccp_sched_del(int id)
{
	if (!PBX(sched_del))
		return 1;

	return PBX(sched_del) (id);
}

/*!
 * \brief 	Load the actual chan_sccp module
 * \return	Success as int
 */
boolean_t sccp_prePBXLoad()
{
	pbx_log(LOG_NOTICE, "preloading pbx module\n");
#ifdef HAVE_LIBGC
	GC_INIT();
	(void)GC_set_warn_proc(gc_warn_handler);

	GC_enable();
#    if DEBUG > 0
	GC_find_leak = 1;
#    endif
#endif

	/* make globals */
	sccp_globals = (struct sccp_global_vars *)sccp_malloc(sizeof(struct sccp_global_vars));
	if (!sccp_globals) {
		pbx_log(LOG_ERROR, "No free memory for SCCP global vars. SCCP channel type disabled\n");
		return FALSE;
	}

	/* Initialize memory */
	memset(&sccp_null_frame, 0, sizeof(sccp_null_frame));
	memset(sccp_globals, 0, sizeof(struct sccp_global_vars));
	GLOB(debug) = DEBUGCAT_CORE;

//      sccp_event_listeners = (struct sccp_event_subscriptions *)sccp_malloc(sizeof(struct sccp_event_subscriptions));
//      memset(sccp_event_listeners, 0, sizeof(struct sccp_event_subscriptions));
//      SCCP_LIST_HEAD_INIT(&sccp_event_listeners->subscriber);

	pbx_mutex_init(&GLOB(lock));
	pbx_mutex_init(&GLOB(usecnt_lock));
	pbx_mutex_init(&GLOB(monitor_lock));

	/* init refcount */
	sccp_refcount_init();

	SCCP_RWLIST_HEAD_INIT(&GLOB(sessions));
	SCCP_RWLIST_HEAD_INIT(&GLOB(devices));
	SCCP_RWLIST_HEAD_INIT(&GLOB(lines));

	GLOB(general_threadpool) = sccp_threadpool_init(THREADPOOL_MIN_SIZE);

	sccp_event_module_start();
	sccp_mwi_module_start();
	sccp_hint_module_start();
	sccp_manager_module_start();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_start();
#endif
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay, TRUE);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend, TRUE);

//      sccp_set_config_defaults(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT);
	/* GLOB() is a macro for sccp_globals-> */
	GLOB(descriptor) = -1;
//      GLOB(ourport) = 2000;
	GLOB(bindaddr.sin_port) = 2000;
	GLOB(externrefresh) = 60;
	GLOB(keepalive) = SCCP_KEEPALIVE;
	sccp_copy_string(GLOB(dateformat), "D/M/YA", sizeof(GLOB(dateformat)));
	sccp_copy_string(GLOB(context), "default", sizeof(GLOB(context)));
	sccp_copy_string(GLOB(servername), "Asterisk", sizeof(GLOB(servername)));

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
	GLOB(dndmode) = SCCP_DNDMODE_REJECT;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = TRUE;											/* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	GLOB(amaflags) = pbx_cdr_amaflags2int("documentation");
	GLOB(callanswerorder) = ANSWER_OLDEST_FIRST;
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	GLOB(earlyrtp) = SCCP_CHANNELSTATE_PROGRESS;
	sccp_create_hotline();
	return TRUE;
}

#if DEBUG

/* 
 * Segfault Handler
 * Copied from 	Author : Andrew Tridgell <junkcode@tridgell.net>
 * 		URL    : http://www.samba.org/ftp/unpacked/junkcode/segv_handler/
 */
/*
static int segv_handler(int sig)
{
	char cmd[100];
	char progname[100];
	char *p;
	int n;

	n = readlink("/proc/self/exe", progname, sizeof(progname));
	progname[n] = 0;

	p = strrchr(progname, '/');
	*p = 0;

	snprintf(cmd, sizeof(cmd), "chan-sccp-b_backtrace %d > /var/log/asterisk/chan-sccp-b_%s.%d.backtrace 2>&1", (int)getpid(), p + 1, (int)getpid());
	system(cmd);
	signal(SIGSEGV, SIG_DFL);
	return 0;
}

static void segv_init() __attribute__ ((constructor));
void segv_init(void)
{
	signal(SIGSEGV, (sighandler_t) segv_handler);
	signal(SIGBUS, (sighandler_t) segv_handler);
}
*/
#endif

boolean_t sccp_postPBX_load()
{
	pbx_mutex_lock(&GLOB(lock));
	GLOB(module_running) = TRUE;
#if DEBUG
//	segv_init();
#endif
	sccp_refcount_schedule_cleanup((const void *)0);
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
	if (!ptr)
		return -1;

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

//      pbx_config_destroy(GLOB(cfg));
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Descriptor\n");
	close(GLOB(descriptor));
	GLOB(descriptor) = -1;

	//! \todo make this pbx independend
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channels\n");

	/* removing devices */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Devices\n");
	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	while ((d = SCCP_LIST_REMOVE_HEAD(&GLOB(devices), list))) {
//              sccp_device_release(d);                 // released by sccp_dev_clean
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		d->realtime = TRUE;										/* use realtime, to fully clear the device configuration */
//              sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
		sccp_dev_clean(d, TRUE, 0);									// performs a device reset if it has a session
	}
	if (SCCP_RWLIST_EMPTY(&GLOB(devices)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(devices));
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	/* hotline will be removed by line removing function */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Hotline\n");
	sccp_line_removeFromGlobals(GLOB(hotline)->line);
	GLOB(hotline)->line = sccp_line_release(GLOB(hotline)->line);
	sccp_free(GLOB(hotline));

	/* removing lines */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Lines\n");
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Removing line %s\n", l->name);
		sccp_line_clean(l, TRUE);
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_RWLIST_EMPTY(&GLOB(lines)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(lines));

	usleep(500);												// wait for events to finalize

	/* removing sessions */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Sessions\n");
	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
	while ((s = SCCP_LIST_REMOVE_HEAD(&GLOB(sessions), list))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", pbx_inet_ntoa(s->sin.sin_addr));
		sccp_socket_stop_sessionthread(s);
////            pthread_cancel(s->session_thread);
	}
	if (SCCP_LIST_EMPTY(&GLOB(sessions)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(sessions));
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killing the socket thread\n");
	sccp_globals_lock(socket_lock);
	if ((GLOB(socket_thread) != AST_PTHREADT_NULL) && (GLOB(socket_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(socket_thread));
		pthread_kill(GLOB(socket_thread), SIGURG);
#ifndef HAVE_LIBGC
		pthread_join(GLOB(socket_thread), NULL);
#endif
	}
	GLOB(socket_thread) = AST_PTHREADT_STOP;
	sccp_globals_unlock(socket_lock);

	sccp_manager_module_stop();
	sccp_softkey_clear();

	sccp_mutex_destroy(&GLOB(socket_lock));
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the socket thread\n");

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing bind\n");
	if (GLOB(ha))
		sccp_free_ha(GLOB(ha));

	if (GLOB(localaddr))
		sccp_free_ha(GLOB(localaddr));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing io/sched\n");

	sccp_hint_module_stop();
	sccp_event_module_stop();

	sccp_threadpool_destroy(GLOB(general_threadpool));
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the threadpool\n");
	sccp_refcount_destroy();
	pbx_mutex_destroy(&GLOB(usecnt_lock));
	pbx_mutex_destroy(&GLOB(lock));
//      pbx_log(LOG_NOTICE, "SCCP chan_sccp unloaded\n");
	return 0;
}

/*!
 * \brief PBX Independent Function to be called when starting module reload
 * \return Success as int
 */
int sccp_reload(void)
{
#ifdef CS_DYNAMIC_CONFIG
	sccp_readingtype_t readingtype;
	int returnval = 0;

	pbx_mutex_lock(&GLOB(lock));
	if (GLOB(reload_in_progress) == TRUE) {
		pbx_log(LOG_ERROR, "SCCP reloading already in progress.\n");
		pbx_mutex_unlock(&GLOB(lock));
		return 1;
	}

	sccp_config_file_status_t cfg = sccp_config_getConfig(TRUE);

	switch (cfg) {
		case CONFIG_STATUS_FILE_NOT_CHANGED:
			pbx_log(LOG_NOTICE, "config file '%s' has not change, skipping reload.\n", GLOB(config_file_name));
			returnval = 0;
			break;
		case CONFIG_STATUS_FILE_OK:
			pbx_log(LOG_NOTICE, "SCCP reloading configuration.\n");
			readingtype = SCCP_CONFIG_READRELOAD;
			GLOB(reload_in_progress) = TRUE;
			pbx_mutex_unlock(&GLOB(lock));
			if (!sccp_config_general(readingtype)) {
				pbx_log(LOG_ERROR, "Unable to reload configuration.\n");
				GLOB(reload_in_progress) = FALSE;
				pbx_mutex_unlock(&GLOB(lock));
				return 2;
			}
			sccp_config_readDevicesLines(readingtype);
			pbx_mutex_lock(&GLOB(lock));
			GLOB(reload_in_progress) = FALSE;
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
	pbx_mutex_unlock(&GLOB(lock));
	return returnval;
#else
	pbx_log(LOG_ERROR, "SCCP configuration reload not implemented yet! use unload and load.\n");
	return 5;
#endif
}

#ifdef CS_DEVSTATE_FEATURE
const char devstate_db_family[] = "CustomDevstate";
#endif
