
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

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

void *sccp_create_hotline(void);

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
		return SCCP_REQUEST_STATUS_UNAVAIL;
	}

	lineSubscriptionId = sccp_parseComposedId(lineName, 80);

	l = sccp_line_find_byname(lineSubscriptionId.mainId);

	if (!l) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineSubscriptionId.mainId);
		return SCCP_REQUEST_STATUS_UNAVAIL;
	}
	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (l->devices.size == 0) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		return SCCP_REQUEST_STATUS_UNAVAIL;
	}
	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	*channel = my_sccp_channel = sccp_channel_allocate_locked(l, NULL);

	if (!my_sccp_channel) {
		return SCCP_REQUEST_STATUS_UNAVAIL;
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

//      my_sccp_channel->rtp.audio.writeState |= SCCP_CODEC_STATUS_REMOTE_PREFERRED;
//      //my_sccp_channel->rtp.audio.writeFormat = codec;
// 
//      my_sccp_channel->rtp.audio.readState |= SCCP_CODEC_STATUS_REMOTE_PREFERRED;
//      //my_sccp_channel->rtp.audio.readFormat = codec;

	uint8_t size = (capabilityLength < sizeof(my_sccp_channel->remoteCapabilities.audio)) ? capabilityLength : sizeof(my_sccp_channel->remoteCapabilities.audio);

	memset(&my_sccp_channel->remoteCapabilities.audio, 0, sizeof(my_sccp_channel->remoteCapabilities.audio));
	memcpy(&my_sccp_channel->remoteCapabilities.audio, capabilities, size);

	/** set requested codec */
//      if(requestedCodec != SKINNY_CODEC_NONE){
//              my_sccp_channel->rtp.audio.writeFormat = requestedCodec;
//              my_sccp_channel->rtp.audio.writeState = SCCP_RTP_STATUS_REQUESTED;
//      }
//      sccp_log(DEBUGCAT_CODEC) (VERBOSE_PREFIX_3 "requested codec (%d)\n", my_sccp_channel->rtp.audio.writeFormat);

	/** done */

//      uint8_t x;
//      for (x = 0; x < capabilityLength && x <= sizeof(my_sccp_channel->remoteCapabilities.audio); x++) {
//              my_sccp_channel->remoteCapabilities.audio[x] = capabilities[x];
//      }

	/** do not set prefered codec, because we do not know the device capabilities */
//      my_sccp_channel->rtp.audio.writeState |= SCCP_CODEC_STATUS_REMOTE_PREFERRED;
//      my_sccp_channel->rtp.audio.readState |= SCCP_CODEC_STATUS_REMOTE_PREFERRED;

	my_sccp_channel->autoanswer_type = autoanswer_type;
	my_sccp_channel->autoanswer_cause = autoanswer_cause;
	my_sccp_channel->ringermode = ringermode;

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

	l = sccp_line_find_byname(lineName);
	if (!l)
		res = AST_DEVICE_INVALID;
	else if (SCCP_LIST_FIRST(&l->devices) == NULL)
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
	else if (sccp_channel_find_bystate_on_line_nolock(l, SCCP_CHANNELSTATE_RINGING))
#    ifdef CS_AST_DEVICE_RINGINUSE
		if (sccp_channel_find_bystate_on_line_nolock(l, SCCP_CHANNELSTATE_CONNECTED))
			res = AST_DEVICE_RINGINUSE;
		else
#    endif
			res = AST_DEVICE_RINGING;
#endif
#ifdef CS_AST_DEVICE_ONHOLD
	else if (sccp_channel_find_bystate_on_line_nolock(l, SCCP_CHANNELSTATE_HOLD))
		res = AST_DEVICE_ONHOLD;
#endif
	else
		res = AST_DEVICE_INUSE;

	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE | DEBUGCAT_HINT)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked for the state (%d) of the line %s\n", res, (char *)data);

	return res;
}

/*!
 * \brief Local Function to check for Valid Session, Message and Device
 * \param s SCCP Session
 * \param r SCCP Moo
 * \param msg Message
 * \return -1 or Device;
 */
static sccp_device_t *check_session_message_device(sccp_session_t * s, sccp_moo_t * r, const char *msg)
{
	sccp_device_t *d = NULL;

	if (!s || (s->fds[0].fd < 0)) {
		pbx_log(LOG_ERROR, "(%s) Session no longer valid\n", msg);
		return NULL;
	}

	if (!r) {
		pbx_log(LOG_ERROR, "(%s) No Message Provided\n", msg);
		return NULL;
	}

	if (!(d = s->device)) {
		pbx_log(LOG_ERROR, "No valid Device available to handle %s for\n", msg);
		return NULL;
	}

	if (s != s->device->session) {
		pbx_log(LOG_WARNING, "(%s) Provided Session and Device Session are not the same!!\n", msg);
	}

	if ((GLOB(debug) & (DEBUGCAT_MESSAGE | DEBUGCAT_ACTION)) != 0) {
		uint32_t mid = letohl(r->lel_messageId);

		pbx_log(LOG_NOTICE, "%s: SCCP Handle Message: %s(0x%04X) %d bytes length\n", DEV_ID_LOG(d), message2str(mid), mid, r->length);
		sccp_dump_packet((unsigned char *)&r->msg.RegisterMessage, (r->length < SCCP_MAX_PACKET) ? (int)r->length : (int)SCCP_MAX_PACKET);
	}

	return d;
}

/*!
 * \brief SCCP Message Handler Callback
 *
 * Used to map SKinny Message Id's to their Handling Implementations
 */
struct sccp_messageMap_cb {
	uint32_t messageId;
	void (*const messageHandler_cb) (sccp_session_t * s, sccp_device_t * d, sccp_moo_t * r);
	boolean_t deviceIsNecessary;
};

/*!
 * \brief SCCP Message Handler Callback Mapping
 *
 * Used to map SKinny Message Id's to their Handling Implementations
 */
static const struct sccp_messageMap_cb messagesCbMap[] = {
	{KeepAliveMessage, sccp_handle_KeepAliveMessage, FALSE},		/* 7985 sends a KeepAliveMessage before register */
	{OffHookMessage, sccp_handle_offhook, TRUE},
	{OnHookMessage, sccp_handle_onhook, TRUE},
	{SoftKeyEventMessage, sccp_handle_soft_key_event, TRUE},
	{OpenReceiveChannelAck, sccp_handle_open_receive_channel_ack, TRUE},
	{OpenMultiMediaReceiveChannelAckMessage, sccp_handle_OpenMultiMediaReceiveAck, TRUE},
	{StartMediaTransmissionAck, sccp_handle_startmediatransmission_ack, TRUE},

	{IpPortMessage, NULL, TRUE},
	{VersionReqMessage, sccp_handle_version, TRUE},
	{CapabilitiesResMessage, sccp_handle_capabilities_res, TRUE},
	{ButtonTemplateReqMessage, sccp_handle_button_template_req, TRUE},
	{SoftKeyTemplateReqMessage, sccp_handle_soft_key_template_req, TRUE},
	{SoftKeySetReqMessage, sccp_handle_soft_key_set_req, TRUE},
	{LineStatReqMessage, sccp_handle_line_number, TRUE},
	{SpeedDialStatReqMessage, sccp_handle_speed_dial_stat_req, TRUE},
	{StimulusMessage, sccp_handle_stimulus, TRUE},

	{HeadsetStatusMessage, sccp_handle_headset, TRUE},
	{TimeDateReqMessage, sccp_handle_time_date_req, TRUE},
	{KeypadButtonMessage, sccp_handle_keypad_button, TRUE},

	{ConnectionStatisticsRes, sccp_handle_ConnectionStatistics, TRUE},
	{ServerReqMessage, sccp_handle_ServerResMessage, TRUE},
	{ConfigStatReqMessage, sccp_handle_ConfigStatMessage, TRUE},
	{EnblocCallMessage, sccp_handle_EnblocCallMessage, TRUE},
	{RegisterAvailableLinesMessage, sccp_handle_AvailableLines, TRUE},
	{ForwardStatReqMessage, sccp_handle_forward_stat_req, TRUE},
	{FeatureStatReqMessage, sccp_handle_feature_stat_req, TRUE},
	{ServiceURLStatReqMessage, sccp_handle_services_stat_req, TRUE},
	{AccessoryStatusMessage, sccp_handle_accessorystatus_message, TRUE},
	{DialedPhoneBookMessage, sccp_handle_dialedphonebook_message, TRUE},
	{UpdateCapabilitiesMessage, sccp_handle_updatecapabilities_message, TRUE},
	{Unknown_0x004A_Message, sccp_handle_unknown_message, TRUE},
	{DisplayDynamicNotifyMessage, sccp_handle_unknown_message, TRUE},
	{DisplayDynamicPriNotifyMessage, sccp_handle_unknown_message, TRUE},
	{SpeedDialStatDynamicMessage, sccp_handle_speed_dial_stat_req, TRUE},
	{ExtensionDeviceCaps, sccp_handle_unknown_message, TRUE},
	{DeviceToUserDataVersion1Message, sccp_handle_device_to_user, TRUE},
	{DeviceToUserDataResponseVersion1Message, sccp_handle_device_to_user_response, TRUE},
	{RegisterTokenRequest, sccp_handle_token_request, FALSE},
	{UnregisterMessage, sccp_handle_unregister, TRUE},
	{RegisterMessage, sccp_handle_register, FALSE},
	{AlarmMessage, sccp_handle_alarm, FALSE},
	{XMLAlarmMessage, sccp_handle_XMLAlarmMessage, FALSE},
	{SPCPRegisterTokenRequest, sccp_handle_SPCPTokenReq, FALSE},
	{StartMultiMediaTransmissionAck, sccp_handle_startmultimediatransmission_ack, TRUE},
	{MediaTransmissionFailure, sccp_handle_mediatransmissionfailure, TRUE},
	{MiscellaneousCommandMessage, sccp_handle_miscellaneousCommandMessage, TRUE},
};

typedef struct sccp_messageMap_cb sccp_messageMap_cb_t;

/*!
 * \brief Get SCCP Message Handler by Message Id
 * \param messageId Skinny Message Id
 * \return Message CallBack Entry
 */
static const sccp_messageMap_cb_t *sccp_getMessageMap_by_MessageId(uint32_t messageId)
{
	uint32_t i;

	for (i = 0; i < ARRAY_LEN(messagesCbMap); i++) {
		if (messagesCbMap[i].messageId == messageId) {
			return &messagesCbMap[i];
		}
	}

	return NULL;
}

/*!
 * \brief 	Controller function to handle Received Messages
 * \param 	r Message as sccp_moo_t
 * \param 	s Session as sccp_session_t
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s)
{
	const sccp_messageMap_cb_t *messageMap_cb = NULL;
	uint32_t mid;

	if (!s) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_handle_message) Client does not have a sessions, Required !\n");
		if (r) {
			sccp_free(r);
		}
		return -1;
	}

	if (!r) {
		pbx_log(LOG_ERROR, "%s: (sccp_handle_message) No Message Specified.\n, Required !", DEV_ID_LOG(s->device));
		return 0;
	}

	mid = letohl(r->lel_messageId);

	s->lastKeepAlive = time(0);	/** always update keepalive */
//      sccp_device_t *tmpDevice=NULL;

	sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: >> Got message (%x) %s\n", DEV_ID_LOG(s->device), mid, message2str(mid));

	/* search for message handler */
	messageMap_cb = sccp_getMessageMap_by_MessageId(mid);

	/* we dont know how to handle event */
	if (!messageMap_cb) {
		pbx_log(LOG_WARNING, "Don't know how to handle message %d\n", mid);
		sccp_handle_unknown_message(s, s->device, r);
		return 1;
	}

	if (messageMap_cb->messageHandler_cb && messageMap_cb->deviceIsNecessary == TRUE && !check_session_message_device(s, r, message2str(mid))) {
		return 0;
	}
	if (messageMap_cb->messageHandler_cb) {
		if (messageMap_cb->deviceIsNecessary == TRUE) {
			sccp_device_lock(s->device);
		}

		messageMap_cb->messageHandler_cb(s, s->device, r);

		if (messageMap_cb->deviceIsNecessary == TRUE) {
			sccp_device_unlock(s->device);
		}
	}

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
	GLOB(monitor_thread) = AST_PTHREADT_NULL;				// ADDED IN SVN 414 -FS
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

	if (!sccp_config_general(SCCP_CONFIG_READINITIAL)) {
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

		memset(&hints, 0, sizeof hints);				// make sure the struct is empty
		hints.ai_family = AF_UNSPEC;					// don't care IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM;				// TCP stream sockets

		if (&GLOB(bindaddr.sin_addr) != NULL) {
			snprintf(port_str, sizeof(port_str), "%d", ntohs(GLOB(bindaddr.sin_port)));
			if ((status = getaddrinfo(pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), port_str, &hints, &res)) != 0) {
				pbx_log(LOG_WARNING, "Failed to get addressinfo for %s:%d, error: %s!\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), gai_strerror(status));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
		} else {
			hints.ai_flags = AI_PASSIVE;				// fill in my IP for me
			if ((status = getaddrinfo(NULL, "cisco_sccp", &hints, &res)) != 0) {
				pbx_log(LOG_WARNING, "Failed to get addressinfo, error: %s!\n", gai_strerror(status));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
		}
		GLOB(descriptor) = socket(res->ai_family, res->ai_socktype, res->ai_protocol);	// need to add code to handle multiple interfaces (multi homed server) -> multiple socket descriptors
#else
		GLOB(descriptor) = socket(AF_INET, SOCK_STREAM, 0);		//replaced
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
			if (bind(GLOB(descriptor), res->ai_addr, res->ai_addrlen) < 0) {	// using addrinfo hints
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
 * \brief 	create a hotline
 * 
 * \lock
 * 	- lines
 */
void *sccp_create_hotline(void)
{
	sccp_line_t *hotline;

	hotline = sccp_line_create();
#ifdef CS_SCCP_REALTIME
	hotline->realtime = TRUE;
#endif

	sccp_copy_string(hotline->name, "Hotline", sizeof(hotline->name));
	sccp_copy_string(hotline->cid_name, "hotline", sizeof(hotline->cid_name));
	sccp_copy_string(hotline->cid_num, "hotline", sizeof(hotline->cid_name));
	sccp_copy_string(hotline->context, "default", sizeof(hotline->context));
	sccp_copy_string(hotline->label, "hotline", sizeof(hotline->label));
	sccp_copy_string(hotline->adhocNumber, "111", sizeof(hotline->adhocNumber));

	//sccp_copy_string(hotline->mailbox, "hotline", sizeof(hotline->mailbox));

	SCCP_RWLIST_WRLOCK(&GLOB(lines));
	SCCP_RWLIST_INSERT_HEAD(&GLOB(lines), hotline, list);
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	GLOB(hotline)->line = hotline;
	sccp_copy_string(GLOB(hotline)->exten, "111", sizeof(GLOB(hotline)->exten));

	return NULL;
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
	sccp_event_listeners = (struct sccp_event_subscriptions *)sccp_malloc(sizeof(struct sccp_event_subscriptions));
	if (!sccp_globals || !sccp_event_listeners) {
		pbx_log(LOG_ERROR, "No free memory for SCCP global vars. SCCP channel type disabled\n");
		return FALSE;
	}

	/* Initialize memory */
	memset(&sccp_null_frame, 0, sizeof(sccp_null_frame));

	memset(sccp_globals, 0, sizeof(struct sccp_global_vars));
	memset(sccp_event_listeners, 0, sizeof(struct sccp_event_subscriptions));
	pbx_mutex_init(&GLOB(lock));
	pbx_mutex_init(&GLOB(usecnt_lock));
	pbx_mutex_init(&GLOB(monitor_lock));
	SCCP_RWLIST_HEAD_INIT(&GLOB(sessions));
	SCCP_RWLIST_HEAD_INIT(&GLOB(devices));
	SCCP_RWLIST_HEAD_INIT(&GLOB(lines));
	SCCP_LIST_HEAD_INIT(&sccp_event_listeners->subscriber);

	sccp_mwi_module_start();
	sccp_hint_module_start();
	sccp_manager_module_start();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_start();
#endif
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend);

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
	GLOB(sccp_tos) = (0x68 & 0xff);						// AF31
	GLOB(audio_tos) = (0xB8 & 0xff);					// EF
	GLOB(video_tos) = (0x88 & 0xff);					// AF41
	GLOB(sccp_cos) = 4;
	GLOB(audio_cos) = 6;
	GLOB(video_cos) = 5;
	GLOB(echocancel) = TRUE;
	GLOB(silencesuppression) = TRUE;
	GLOB(dndmode) = SCCP_DNDMODE_REJECT;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = TRUE;							/* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	GLOB(amaflags) = pbx_cdr_amaflags2int("documentation");
	GLOB(callanswerorder) = ANSWER_OLDEST_FIRST;
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	GLOB(hotline) = (sccp_hotline_t *) sccp_malloc(sizeof(sccp_hotline_t));
	GLOB(cfg) = sccp_config_getConfig();
	GLOB(earlyrtp) = SCCP_CHANNELSTATE_PROGRESS;

	memset(GLOB(hotline), 0, sizeof(sccp_hotline_t));

	sccp_create_hotline();

	return TRUE;
}

boolean_t sccp_postPBX_load()
{
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
	sccp_device_t *d;
	sccp_line_t *l;
	sccp_session_t *s;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unloading Module\n");

	pbx_config_destroy(GLOB(cfg));

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Descriptor\n");
	close(GLOB(descriptor));
	GLOB(descriptor) = -1;

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

	//! \todo make this pbx independend
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channels\n");

	/* temporary fix to close open channels */
	/*! \todo Temporary fix to unload Module. Needs to be looked at */

/*
	PBX_CHANNEL_TYPE *pbx_channel = NULL;
	int openchannels = 0;
	sccp_channel_t *sccp_channel;
	while ((pbx_channel = pbx_channel_walk_locked(pbx_channel)) != NULL) {
		if (!pbx_check_hangup(pbx_channel)) {
			if ((sccp_channel = get_sccp_channel_from_pbx_channel(pbx_channel))) {
				pbx_channel->hangupcause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
				pbx_channel->_softhangup = AST_SOFTHANGUP_APPUNLOAD;
				sccp_channel_lock(sccp_channel);
				sccp_channel_endcall_locked(sccp_channel);
				sccp_channel_unlock(sccp_channel);
				pbx_safe_sleep(pbx_channel, 100);
				openchannels++;
			}
		}
		pbx_channel_unlock(pbx_channel);
	}
	sccp_safe_sleep(openchannels * 1000);				// wait for everything to settle
*/

	/* removing devices */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Devices\n");
	SCCP_RWLIST_WRLOCK(&GLOB(devices));
	while ((d = SCCP_LIST_REMOVE_HEAD(&GLOB(devices), list))) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		sccp_dev_clean(d, TRUE, 0);
	}
	if (SCCP_RWLIST_EMPTY(&GLOB(devices)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(devices));
	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	/* hotline will be removed by line removing function */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Hotline\n");
	GLOB(hotline)->line = NULL;
	sccp_free(GLOB(hotline));

	/* removing lines */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Lines\n");
	SCCP_RWLIST_WRLOCK(&GLOB(lines));
	while ((l = SCCP_RWLIST_REMOVE_HEAD(&GLOB(lines), list))) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP: Removing line %s\n", l->name);
		sccp_line_clean(l, FALSE);
	}
	if (SCCP_RWLIST_EMPTY(&GLOB(lines)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(lines));
	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	/* removing sessions */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing Sessions\n");
	SCCP_RWLIST_WRLOCK(&GLOB(sessions));
	while ((s = SCCP_LIST_REMOVE_HEAD(&GLOB(sessions), list))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", pbx_inet_ntoa(s->sin.sin_addr));
		pthread_cancel(s->session_thread);
	}
	SCCP_RWLIST_UNLOCK(&GLOB(sessions));

	if (SCCP_LIST_EMPTY(&GLOB(sessions)))
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(sessions));

	sccp_manager_module_stop();

	sccp_mutex_destroy(&GLOB(socket_lock));
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the socket thread\n");

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing bind\n");
	if (GLOB(ha))
		sccp_free_ha(GLOB(ha));

	if (GLOB(localaddr))
		sccp_free_ha(GLOB(localaddr));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing io/sched\n");

	pbx_mutex_destroy(&GLOB(usecnt_lock));
	pbx_mutex_destroy(&GLOB(lock));
	pbx_log(LOG_NOTICE, "SCCP chan_sccp unloaded\n");
	return 0;
}

/*!
 * \brief PBX Independent Function to be called when starting module reload
 * \return Success as int
 */
int sccp_reload(void)
{

	return 0;
}

#ifdef CS_DEVSTATE_FEATURE
const char devstate_db_family[] = "CustomDevstate";
#endif
