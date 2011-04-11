
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

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#include <asterisk/translate.h>
#if ASTERISK_VERSION_NUM >= 10400
#    ifdef CS_AST_HAS_TECH_PVT
#        define SET_CAUSE(x)	*cause = x;
#    else
#        define SET_CAUSE(x)
#    endif
void *sccp_create_hotline(void);

/*!
 * \brief	Buffer for Jitterbuffer use
 */
static struct ast_jb_conf default_jbconf = {
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
#    ifdef CS_AST_JB_TARGET_EXTRA
	.impl = "",
	.target_extra = -1
#    else
	.impl = ""
#    endif
};
#endif

/*!
 * \brief	Global null frame
 */
struct ast_frame sccp_null_frame;						/*!< Asterisk Structure */

/*!
 * \brief	Global variables
 */
struct sccp_global_vars *sccp_globals = 0;

/*!
 * \brief	Global scheduler and IO context
 */
struct sched_context *sched = 0;

struct io_context *io = 0;

#ifdef CS_AST_HAS_TECH_PVT

/*!
 * \brief	handle request coming from asterisk
 * \param	type	type of data as char
 * \param	format	format of data as int
 * \param	data	actual data
 * \param 	cause	Cause of the request
 * \return	Asterisk Channel
 * 
 * \called_from_asterisk
 */
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause)
#else

/*!
 * \brief	handle request coming from asterisk
 * \param	type	type of data as char
 * \param	format	format of data as int
 * \param	data	actual data
 * \return	Asterisk Channel
 * 
 * \called_from_asterisk
 * 
 * \warning
 * 	- line->devices is not always locked
 */
struct ast_channel *sccp_request(char *type, int format, void *data)
#endif
{
	struct composedId lineSubscriptionId;

	sccp_line_t *l = NULL;

	sccp_channel_t *c = NULL;

	char *options = NULL, *lineName = NULL;

	int optc = 0;

	char *optv[2];

	int opti = 0;

	int oldformat = format;

	memset(&lineSubscriptionId, 0, sizeof(struct composedId));
	SET_CAUSE(AST_CAUSE_NOTDEFINED);

	if (!type) {
		ast_log(LOG_NOTICE, "Attempt to call the wrong type of channel\n");
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}

	if (!data) {
		ast_log(LOG_NOTICE, "Attempt to call SCCP/ failed\n");
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}

	/* we leave the data unchanged */
	lineName = strdup(data);

	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	lineSubscriptionId = sccp_parseComposedId(lineName, 80);

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: Asterisk asked to create a channel type=%s, format=%d, line=%s, subscriptionId.number=%s, options=%s\n", type, format, lineSubscriptionId.mainId, lineSubscriptionId.subscriptionId.number, (options) ? options : "");

	l = sccp_line_find_byname(lineSubscriptionId.mainId);

	if (!l) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineSubscriptionId.mainId);
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}

	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (SCCP_LIST_FIRST(&l->devices) == NULL) {
		sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}

	sccp_log((DEBUGCAT_SCCP + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	c = sccp_channel_allocate_locked(l, NULL);
	if (!c) {
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}

	/* set subscriberId for individual device addressing */
	if (!sccp_strlen_zero(lineSubscriptionId.subscriptionId.number)) {
		sccp_copy_string(c->subscriptionId.number, lineSubscriptionId.subscriptionId.number, sizeof(c->subscriptionId.number));
		if (!sccp_strlen_zero(lineSubscriptionId.subscriptionId.name)) {
			sccp_copy_string(c->subscriptionId.name, lineSubscriptionId.subscriptionId.name, sizeof(c->subscriptionId.name));
			//ast_log(LOG_NOTICE, "%s: calling subscriber id=%s\n, name=%s", l->id, c->subscriptionId.number,c->subscriptionId.name);
		} else {
			//ast_log(LOG_NOTICE, "%s: calling subscriber id=%s\n", l->id, c->subscriptionId.number);
		}
	} else {
		sccp_copy_string(c->subscriptionId.number, l->defaultSubscriptionId.number, sizeof(c->subscriptionId.number));
		sccp_copy_string(c->subscriptionId.name, l->defaultSubscriptionId.name, sizeof(c->subscriptionId.name));
		//ast_log(LOG_NOTICE, "%s: calling all subscribers\n", l->id);
	}

	if (!sccp_pbx_channel_allocate_locked(c)) {
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		sccp_line_removeChannel(c->line, c);
		sccp_channel_destroy_locked(c);
		c = NULL;
		goto OUT;
	}
#if 0
	sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "Line %s has %d device%s\n", l->name, l->devices.size, (l->devices.size > 1) ? "s" : "");
	if (l->devices.size < 2) {
		if (!c->owner) {
			sccp_log((DEBUGCAT_DEVICE | DEBUGCAT_LINE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: channel has no owner\n", l->name);
			SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
			sccp_channel_delete(c);
			c = NULL;
			goto OUT;
		}

		sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "%s: Call forward type: %d\n", l->name, l->cfwd_type);
		if (l->cfwd_type == SCCP_CFWD_ALL) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "%s: Call forward (all) to %s\n", l->name, l->cfwd_num);
#    ifdef CS_AST_HAS_AST_STRING_FIELD
			ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#    else
			sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#    endif
		} else if (l->cfwd_type == SCCP_CFWD_BUSY && SCCP_RWLIST_GETSIZE(l->channels) > 1) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "%s: Call forward (busy) to %s\n", l->name, l->cfwd_num);
#    ifdef CS_AST_HAS_AST_STRING_FIELD
			ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#    else
			sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#    endif
		}
	}
#endif

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* we have a single device given */

	/*
	   if(c->device){
	   if(c->device->session){
	   hasSession = TRUE;
	   format &= c->device->capability;
	   }
	   }else{
	   sccp_linedevices_t *linedevice;

	   SCCP_LIST_LOCK(&l->devices);
	   SCCP_LIST_TRAVERSE(&l->devices, linedevice, list){
	   if(!linedevice->device)
	   continue;

	   device = linedevice->device;
	   // \todo TODO check capability on shared lines
	   format &= device->capability;
	   if(device->session)
	   hasSession = TRUE;
	   }
	   SCCP_LIST_UNLOCK(&l->devices);
	   } */

	if (l->devices.size == 0) {
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_PBX | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP/%s we have no registered devices for this line.\n", l->name);
		SET_CAUSE(AST_CAUSE_REQUESTED_CHAN_UNAVAIL);
		goto OUT;
	}
	/*
	   if (!format) {
	   format = oldformat;
	   res = ast_translator_best_choice(&format, &GLOB(global_capability));
	   if (res < 0) {
	   ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format '%d'\n", oldformat);
	   SET_CAUSE(AST_CAUSE_CHANNEL_UNACCEPTABLE);
	   goto OUT;
	   }
	   } */

	c->format = oldformat;
	c->isCodecFix = TRUE;
	sccp_channel_updateChannelCapability_locked(c);

	/* we don't need to parse any options when we have a call forward status */
//      if (c->owner && !sccp_strlen_zero(c->owner->call_forward))
//              goto OUT;

	/* check for the channel params */
	if (options && (optc = sccp_app_separate_args(options, '/', optv, sizeof(optv) / sizeof(optv[0])))) {

		for (opti = 0; opti < optc; opti++) {
			if (!strncasecmp(optv[opti], "aa", 2)) {
				/* let's use the old style auto answer aa1w and aa2w */
				if (!strncasecmp(optv[opti], "aa1w", 4)) {
					c->autoanswer_type = SCCP_AUTOANSWER_1W;
					optv[opti] += 4;
				} else if (!strncasecmp(optv[opti], "aa2w", 4)) {
					c->autoanswer_type = SCCP_AUTOANSWER_2W;
					optv[opti] += 4;
				} else if (!strncasecmp(optv[opti], "aa=", 3)) {
					optv[opti] += 3;
					if (!strncasecmp(optv[opti], "1w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti] += 2;
					} else if (!strncasecmp(optv[opti], "2w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_2W;
						optv[opti] += 2;
					}
				}

				/* since the pbx ignores autoanswer_cause unless SCCP_RWLIST_GETSIZE(l->channels) > 1, it is safe to set it if provided */
				if (!sccp_strlen_zero(optv[opti]) && (c->autoanswer_type)) {
					if (!strcasecmp(optv[opti], "b"))
						c->autoanswer_cause = AST_CAUSE_BUSY;
					else if (!strcasecmp(optv[opti], "u"))
						c->autoanswer_cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
					else if (!strcasecmp(optv[opti], "c"))
						c->autoanswer_cause = AST_CAUSE_CONGESTION;
				}
#ifdef CS_AST_HAS_TECH_PVT
				if (c->autoanswer_cause)
					*cause = c->autoanswer_cause;
#endif
				/* check for ringer options */
			} else if (!strncasecmp(optv[opti], "ringer=", 7)) {
				optv[opti] += 7;
				if (!strcasecmp(optv[opti], "inside"))
					c->ringermode = SKINNY_STATION_INSIDERING;
				else if (!strcasecmp(optv[opti], "outside"))
					c->ringermode = SKINNY_STATION_OUTSIDERING;
				else if (!strcasecmp(optv[opti], "feature"))
					c->ringermode = SKINNY_STATION_FEATURERING;
				else if (!strcasecmp(optv[opti], "silent"))
					c->ringermode = SKINNY_STATION_SILENTRING;
				else if (!strcasecmp(optv[opti], "urgent"))
					c->ringermode = SKINNY_STATION_URGENTRING;
				else
					c->ringermode = SKINNY_STATION_OUTSIDERING;

			} else {
				ast_log(LOG_WARNING, "%s: Wrong option %s\n", l->id, optv[opti]);
			}
		}

	}

 OUT:
	if (lineName)
		sccp_free(lineName);
	if (c)
		sccp_channel_unlock(c);

	sccp_restart_monitor();
	return (c ? c->owner : NULL);
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

	// \todo TODO handle dnd on device
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
 * \brief 	Controller function to handle Received Messages
 * \param 	r Message as sccp_moo_t
 * \param 	s Session as sccp_session_t
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s)
{
	if (!s) {
		ast_log(LOG_ERROR, "%s: (sccp_handle_message) Client does not have a sessions, Required !\n", s->device->id ? s->device->id : "SCCP");
		ast_free(r);
		return -1;
	}

	if (!r) {
		ast_log(LOG_ERROR, "%s: (sccp_handle_message) No Message Specified.\n, Required !", s->device->id ? s->device->id : "SCCP");
		ast_free(r);
		return 0;
	}
	uint32_t mid = letohl(r->lel_messageId);

	//sccp_log((DEBUGCAT_MESSAGE | DEBUGCAT_CORE))(VERBOSE_PREFIX_3 "%s: last keepAlive within %d (%d)\n", (s->device)?s->device->id:"null", (uint32_t)(time(0) - s->lastKeepAlive), (s->device)?s->device->keepalive:0 );

	s->lastKeepAlive = time(0);						/* always update keepalive */

	/* Check if all necessary information is available */
	if ((!s->device) && (mid != RegisterMessage && mid != RegisterTokenReq && mid != AlarmMessage && mid != KeepAliveMessage && mid != XMLAlarmMessage && mid != IpPortMessage && mid != SPARegisterMessage)) {
		ast_log(LOG_WARNING, "SCCP: Client sent %s without first registering. Attempting reconnect.\n", message2str(mid));
		ast_free(r);
		return 0;
		
	} else if (s->device) {
		if (s->device != sccp_device_find_byipaddress(s->sin)) {
			// IP Address has changed mid session
			if (s->device->nat == 1) {
				// We are natted, what should we do, Not doing anything for now, just sending warning -- DdG
				ast_log(LOG_WARNING, "%s: Device (%s) attempted to send messages via a different ip-address (%s).\n", DEV_ID_LOG(s->device), pbx_inet_ntoa(s->sin.sin_addr), pbx_inet_ntoa(s->device->session->sin.sin_addr));
				// \todo write auto recover ip-address change during session with natted device should be be implemented
				/*
				   s->device->session->sin.sin_addr.s_addr = s->sin.sin_addr.s_addr;
				   s->device->nat = 1;
				   sccp_device_reset(s->device, r);
				   sccp_session_unlock(s);
				   sccp_session_close(s,5);
				   sccp_session_close(s->device->session,5);
				   destroy_session(s,5);
				   destroy_session(s->device->session,5);
				   sccp_session_lock(s);
				 */
			} else {
				// We are not natted, but the ip-address has changed
				ast_log(LOG_ERROR, "(sccp_handle_message): SCCP: Device is attempting to send message via a different ip-address.\nIf this is behind a firewall please set it up in sccp.conf with nat=1.\n");
				ast_free(r);
				return 0;
			}
		} else if (s->device && (!s->device->session || s->device->session != s)) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_MESSAGE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "%s: cross device session (Removing Old Session)\n", DEV_ID_LOG(s->device));
			// removed, returning 0 will take care of destroying the session for us.
//
//			SCCP_RWLIST_WRLOCK(&GLOB(sessions));
//			SCCP_LIST_REMOVE(&GLOB(sessions), s, list);
//			SCCP_RWLIST_UNLOCK(&GLOB(sessions));

//			pthread_cancel(s->session_thread);
			ast_free(r);
			return 0;
		}
	}

	if (mid != KeepAliveMessage) {
		if (s && s->device) {
			sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "%s: >> Got message (%x) %s\n", s->device->id, mid, message2str(mid));
		} else {
			sccp_log((DEBUGCAT_MESSAGE)) (VERBOSE_PREFIX_3 "SCCP: >> Got message (%x) %s\n", mid, message2str(mid));
		}
	}

	switch (mid) {
	case AlarmMessage:
		sccp_handle_alarm(s, r);
		break;
	case RegisterMessage:
#ifdef CS_ADV_FEATURES
		sccp_handle_register(s, r);
		break;
#endif
	case RegisterTokenReq:
#ifdef CS_ADV_FEATURES
		sccp_handle_tokenreq(s, r);
		break;
#else
		sccp_handle_register(s, r);
		break;
#endif
	case SPARegisterMessage:
		sccp_handle_SPAregister(s, r);
		break;
	case UnregisterMessage:
		sccp_handle_unregister(s, r);
		break;
	case KeepAliveMessage:
		sccp_session_sendmsg(s->device, KeepAliveAckMessage);
		break;
	case IpPortMessage:
		/* obsolete message */
		//s->rtpPort = letohs(r->msg.IpPortMessage.les_rtpMediaPort);
		break;
	case VersionReqMessage:
		sccp_handle_version(s, r);
		break;
	case CapabilitiesResMessage:
		sccp_handle_capabilities_res(s, r);
		break;
	case ButtonTemplateReqMessage:
		sccp_handle_button_template_req(s, r);
		break;
	case SoftKeyTemplateReqMessage:
		sccp_handle_soft_key_template_req(s, r);
		break;
	case SoftKeySetReqMessage:
		sccp_handle_soft_key_set_req(s, r);
		break;
	case LineStatReqMessage:
		sccp_handle_line_number(s, r);
		break;
	case SpeedDialStatReqMessage:
		sccp_handle_speed_dial_stat_req(s, r);
		break;
	case StimulusMessage:
		sccp_handle_stimulus(s, r);
		break;
	case OffHookMessage:
		sccp_handle_offhook(s, r);
		break;
	case OnHookMessage:
		sccp_handle_onhook(s, r);
		break;
	case HeadsetStatusMessage:
		sccp_handle_headset(s, r);
		break;
	case TimeDateReqMessage:
		sccp_handle_time_date_req(s, r);
		break;
	case KeypadButtonMessage:
		sccp_handle_keypad_button(s, r);
		break;
	case SoftKeyEventMessage:
		sccp_handle_soft_key_event(s, r);
		break;
	case OpenReceiveChannelAck:
		sccp_handle_open_receive_channel_ack(s, r);
		break;
	case OpenMultiMediaReceiveChannelAckMessage:
		sccp_handle_OpenMultiMediaReceiveAck(s, r);
		break;
	case ConnectionStatisticsRes:
		sccp_handle_ConnectionStatistics(s, r);
		break;
	case ServerReqMessage:
		sccp_handle_ServerResMessage(s, r);
		break;
	case ConfigStatReqMessage:
		sccp_handle_ConfigStatMessage(s, r);
		break;
	case EnblocCallMessage:
		sccp_handle_EnblocCallMessage(s, r);
		break;
	case RegisterAvailableLinesMessage:
		sccp_handle_AvailableLines(s->device);
		break;
	case ForwardStatReqMessage:
		sccp_handle_forward_stat_req(s, r);
		break;
	case FeatureStatReqMessage:
		sccp_handle_feature_stat_req(s, r);
		break;
	case ServiceURLStatReqMessage:
		sccp_handle_services_stat_req(s, r);
		break;
	case AccessoryStatusMessage:
		sccp_handle_accessorystatus_message(s, r);
		break;
	case DialedPhoneBookMessage:
		sccp_handle_dialedphonebook_message(s, r);
		break;
	case UpdateCapabilitiesMessage:
		sccp_handle_updatecapabilities_message(s, r);
		break;
	case StartMediaTransmissionAck:
		sccp_handle_startmediatransmission_ack(s, r);
		break;
	case Unknown_0x004A_Message:
		if ((GLOB(debug) & DEBUGCAT_MESSAGE) == DEBUGCAT_MESSAGE) {
			sccp_handle_unknown_message(s, r);
		}
		break;
	case Unknown_0x0143_Message:
		if ((GLOB(debug) & DEBUGCAT_MESSAGE) == DEBUGCAT_MESSAGE) {
			sccp_handle_unknown_message(s, r);
		}
		break;
	case Unknown_0x0144_Message:
		if ((GLOB(debug) & DEBUGCAT_MESSAGE) == DEBUGCAT_MESSAGE) {
			sccp_handle_unknown_message(s, r);
		}
		break;
	case SpeedDialStatDynamicMessage:
		sccp_handle_speed_dial_stat_req(s, r);
		break;
	case ExtensionDeviceCaps:
		if ((GLOB(debug) & DEBUGCAT_MESSAGE) == DEBUGCAT_MESSAGE) {
			sccp_handle_unknown_message(s, r);
		}
		break;
	case XMLAlarmMessage:
		if ((GLOB(debug) & DEBUGCAT_MESSAGE) == DEBUGCAT_MESSAGE) {
			sccp_handle_unknown_message(s, r);
		}
		break;
	case DeviceToUserDataVersion1Message:
		sccp_handle_device_to_user(s,r);
		break;
	case DeviceToUserDataResponseVersion1Message:
		sccp_handle_device_to_user_response(s,r);
		break;
	default:
		sccp_handle_unknown_message(s, r);
	}

	ast_free(r);
	return 1;
}

/**
 * \brief load the configuration from sccp.conf
 *
 */
static int load_config(void)
{
	int oldport = ntohs(GLOB(bindaddr.sin_port));

	int on = 1;

#if ASTERISK_VERSION_NUM >= 10400
	/* Copy the default jb config over global_jbconf */
	memcpy(&GLOB(global_jbconf), &default_jbconf, sizeof(struct ast_jb_conf));

	/* Setup the monitor thread default */
	GLOB(monitor_thread) = AST_PTHREADT_NULL;				// ADDED IN SVN 414 -FS
	GLOB(mwiMonitorThread) = AST_PTHREADT_NULL;
#endif

	memset(&GLOB(global_codecs), 0, sizeof(GLOB(global_codecs)));
	memset(&GLOB(bindaddr), 0, sizeof(GLOB(bindaddr)));
	GLOB(allowAnonymus) = TRUE;

#ifdef CS_SCCP_REALTIME
	sccp_copy_string(GLOB(realtimedevicetable), "sccpdevice", sizeof(GLOB(realtimedevicetable)));
	sccp_copy_string(GLOB(realtimelinetable), "sccpline", sizeof(GLOB(realtimelinetable)));
#endif

#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	sccp_log(0) (VERBOSE_PREFIX_2 "Platform byte order   : LITTLE ENDIAN\n");
#else
	sccp_log(0) (VERBOSE_PREFIX_2 "Platform byte order   : BIG ENDIAN\n");
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
		GLOB(descriptor) = socket(AF_INET, SOCK_STREAM, 0);

		on = 1;
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
		if (setsockopt(GLOB(descriptor), IPPROTO_IP, IP_TOS, &GLOB(sccp_tos), sizeof(GLOB(sccp_tos))) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket TOS to %d: %s\n", GLOB(sccp_tos), strerror(errno));
		else if (GLOB(sccp_tos))
			sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_1 "Using SCCP Socket ToS mark %d\n", GLOB(sccp_tos));
		if (setsockopt(GLOB(descriptor), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));
#if defined(linux)
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_PRIORITY, &GLOB(sccp_cos), sizeof(GLOB(sccp_cos))) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket COS to %d: %s\n", GLOB(sccp_cos), strerror(errno));
		else if (GLOB(sccp_cos))
			sccp_log(DEBUGCAT_SOCKET) (VERBOSE_PREFIX_1 "Using SCCP Socket CoS mark %d\n", GLOB(sccp_cos));
#endif

		if (GLOB(descriptor) < 0) {
			ast_log(LOG_WARNING, "Unable to create SCCP socket: %s\n", strerror(errno));
		} else {
			if (bind(GLOB(descriptor), (struct sockaddr *)&GLOB(bindaddr), sizeof(GLOB(bindaddr))) < 0) {
				ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));

			if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
				ast_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
				close(GLOB(descriptor));
				GLOB(descriptor) = -1;
				return 0;
			}
			sccp_log(0) (VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", pbx_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
			GLOB(reload_in_progress) = FALSE;
			ast_pthread_create(&GLOB(socket_thread), NULL, sccp_socket_thread, NULL);

		}
	}

	sccp_restart_monitor();

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
 * \brief 	start monitoring thread of chan_sccp
 * \param 	data
 * 
 * \lock
 * 	- monitor_lock
 */
void *sccp_do_monitor(void *data)
{
	int res;

	/* This thread monitors all the interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for (;;) {
		pthread_testcancel();
		/* Wait for sched or io */
		res = pbx_sched_wait(sched);
		if ((res < 0) || (res > 1000)) {
			res = 1000;
		}
		res = pbx_io_wait(io, res);
		ast_mutex_lock(&GLOB(monitor_lock));
		if (res >= 0) {
			ast_sched_runq(sched);
		}
		ast_mutex_unlock(&GLOB(monitor_lock));
	}
	/* Never reached */
	return NULL;

}

/*!
 * \brief 	restart the monitoring thread of chan_sccp
 * \return	Success as int
 * 
 * \lock
 * 	- monitor_lock
 */
int sccp_restart_monitor(void)
{
	/* If we're supposed to be stopped -- stay stopped */
	if (GLOB(monitor_thread) == AST_PTHREADT_STOP)
		return 0;

	ast_mutex_lock(&GLOB(monitor_lock));
	if (GLOB(monitor_thread) == pthread_self()) {
		ast_mutex_unlock(&GLOB(monitor_lock));
		sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Cannot kill myself\n");
		return -1;
	}
	if (GLOB(monitor_thread) != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(GLOB(monitor_thread), SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&GLOB(monitor_thread), NULL, sccp_do_monitor, NULL) < 0) {
			ast_mutex_unlock(&GLOB(monitor_lock));
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_SCCP)) (VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&GLOB(monitor_lock));
	return 0;
}


#if ASTERISK_VERSION_NUM < 10400

/*!
 * \brief 	Load the actual chan_sccp module
 * \return	Success as int
 */
int load_module()
#else
static int load_module(void)
#endif
{
#ifdef HAVE_LIBGC
	GC_INIT();
	(void)GC_set_warn_proc(gc_warn_handler);
#    if DEBUG > 0
	GC_find_leak = 1;
#    endif
#endif
	/* make globals */
	sccp_globals = ast_malloc(sizeof(struct sccp_global_vars));
	sccp_event_listeners = ast_malloc(sizeof(struct sccp_event_subscriptions));
	if (!sccp_globals || !sccp_event_listeners) {
		ast_log(LOG_ERROR, "No free memory for SCCP global vars. SCCP channel type disabled\n");
#if ASTERISK_VERSION_NUM < 10400
		return -1;
#else
		return AST_MODULE_LOAD_FAILURE;
#endif
	}

	/* Initialize memory */
	memset(&sccp_null_frame, 0, sizeof(sccp_null_frame));

	sched = sched_context_create();
	if (!sched) {
		ast_log(LOG_WARNING, "Unable to create schedule context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}
	io = io_context_create();
	if (!io) {
		ast_log(LOG_WARNING, "Unable to create I/O context. SCCP channel type disabled\n");
		return AST_MODULE_LOAD_FAILURE;
	}

	memset(sccp_globals, 0, sizeof(struct sccp_global_vars));
	memset(sccp_event_listeners, 0, sizeof(struct sccp_event_subscriptions));
	ast_mutex_init(&GLOB(lock));
	ast_mutex_init(&GLOB(usecnt_lock));
	ast_mutex_init(&GLOB(monitor_lock));
	SCCP_RWLIST_HEAD_INIT(&GLOB(sessions));
	SCCP_RWLIST_HEAD_INIT(&GLOB(devices));
	SCCP_RWLIST_HEAD_INIT(&GLOB(lines));

	SCCP_LIST_HEAD_INIT(&sccp_event_listeners->subscriber);

	sccp_mwi_module_start();
	sccp_hint_module_start();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_start();
#endif
	sccp_event_subscribe(SCCP_EVENT_FEATURECHANGED, sccp_util_handleFeatureChangeEvent);

	/* GLOB() is a macro for sccp_globals-> */
	GLOB(descriptor) = -1;
	GLOB(ourport) = 2000;
	GLOB(externrefresh) = 60;
	GLOB(keepalive) = SCCP_KEEPALIVE;
	sccp_copy_string(GLOB(date_format), "D/M/YA", sizeof(GLOB(date_format)));
	sccp_copy_string(GLOB(context), "default", sizeof(GLOB(context)));
	sccp_copy_string(GLOB(servername), "Asterisk", sizeof(GLOB(servername)));

	/* Wait up to 16 seconds for first digit */
	GLOB(firstdigittimeout) = 16;
	/* How long to wait for following digits */
	GLOB(digittimeout) = 8;
	/* Yes, these are all that the phone supports (except it's own 'Wideband 256k') */
	GLOB(global_capability) = AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_G729A | AST_FORMAT_H263;

	GLOB(debug) = 1;
	GLOB(sccp_tos) = (0x68 & 0xff);						// AF31
	GLOB(audio_tos) = (0xB8 & 0xff);					// EF
	GLOB(video_tos) = (0x88 & 0xff);					// AF41
	GLOB(sccp_cos) = 4;
	GLOB(audio_cos) = 6;
	GLOB(video_cos) = 5;
	GLOB(echocancel) = 1;
	GLOB(silencesuppression) = 0;
	GLOB(dndmode) = SCCP_DNDMODE_REJECT;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = 1;							/* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	GLOB(amaflags) = ast_cdr_amaflags2int("documentation");
	GLOB(callAnswerOrder) = ANSWER_OLDEST_FIRST;
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	GLOB(hotline) = ast_malloc(sizeof(sccp_hotline_t));
	GLOB(earlyrtp) = SCCP_CHANNELSTATE_PROGRESS;
	memset(GLOB(hotline), 0, sizeof(sccp_hotline_t));

	sccp_create_hotline();

	if (!load_config()) {
#ifdef CS_AST_HAS_TECH_PVT
		if (pbx_channel_register(&sccp_tech)) {
#else
		if (pbx_channel_register_ex("SCCP", "SCCP", GLOB(global_capability), sccp_request, sccp_devicestate)) {
#endif
			ast_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			return -1;
		}
	}
#ifndef CS_AST_HAS_RTP_ENGINE
	pbx_rtp_proto_register(&sccp_rtp);
#else
	pbx_rtp_glue_register(&sccp_rtp);
#endif

#ifdef CS_SCCP_MANAGER
	sccp_register_management();
#endif

	sccp_register_cli();
	sccp_register_dialplan_functions();

	/* And start the monitor for the first time */
	sccp_restart_monitor();

	return 0;
}

#if ASTERISK_VERSION_NUM >= 10400

/*!
 * \brief Schedule free memory
 * \param ptr pointer
 * \return Success as int
 */
int sccp_sched_free(void *ptr)
{
	if (!ptr)
		return -1;

	ast_free(ptr);
	return 0;

}
#endif

#if ASTERISK_VERSION_NUM < 10400

/*!
 * \brief 	Unload the chan_sccp module
 * \return	Success as int
 * 
 * \lock
 * 	- channel->owner
 * 	- lines
 * 	- monitor_lock
 * 	- devices
 * 	- lines
 * 	- sessions
 * 	- socket_lock
 */
int unload_module()
#else
static int unload_module(void)
#endif
{
	sccp_device_t *d;

	sccp_line_t *l;

	sccp_channel_t *c;

	sccp_session_t *s;

	int openchannels = 0;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unloading Module\n");

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

	/* temporary fix to close open channels */
	/* \todo Temporary fix to unload Module. Needs to be looked at */
	struct ast_channel *astChannel = NULL;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channels\n");
	while ((astChannel = pbx_channel_walk_locked(astChannel)) != NULL) {
		if (!pbx_check_hangup(astChannel)) {
			if ((c = get_sccp_channel_from_ast_channel(astChannel))) {
				astChannel->hangupcause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
				astChannel->_softhangup = AST_SOFTHANGUP_APPUNLOAD;
				sccp_channel_lock(c);
				sccp_channel_endcall_locked(c);
				sccp_channel_unlock(c);
				ast_safe_sleep(astChannel, 100);
				openchannels++;
			}
		}
		pbx_channel_unlock(astChannel);
	}
	sccp_safe_sleep(openchannels * 1000);					// wait for everything to settle

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP RTP protocol\n");
	pbx_rtp_proto_unregister(&sccp_rtp);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Unregister SCCP Channel Tech\n");
#ifdef CS_AST_HAS_TECH_PVT
	pbx_channel_unregister(&sccp_tech);
#else
	pbx_channel_unregister("SCCP");
#endif
	sccp_unregister_dialplan_functions();
	sccp_unregister_cli();

	sccp_mwi_module_stop();
	sccp_hint_module_stop();

#ifdef CS_SCCP_MANAGER
	sccp_unregister_management();
#endif
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
	ast_free(GLOB(hotline));

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

	sccp_mutex_destroy(&GLOB(socket_lock));
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Killed the socket thread\n");

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing bind\n");
	if (GLOB(ha))
		ast_free_ha(GLOB(ha));

	if (GLOB(localaddr))
		ast_free_ha(GLOB(localaddr));

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_SOCKET)) (VERBOSE_PREFIX_2 "SCCP: Removing io/sched\n");
	if (io)
		io_context_destroy(io);
	if (sched)
		sched_context_destroy(sched);

	ast_mutex_destroy(&GLOB(usecnt_lock));
	ast_mutex_destroy(&GLOB(lock));
	ast_free(sccp_globals);

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Removing monitor thread\n");
	sccp_globals_lock(monitor_lock);
	if ((GLOB(monitor_thread) != AST_PTHREADT_NULL) && (GLOB(monitor_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(monitor_thread));
		pthread_kill(GLOB(monitor_thread), SIGURG);
#ifndef HAVE_LIBGC
		pthread_join(GLOB(monitor_thread), NULL);
#endif
	}
	GLOB(monitor_thread) = AST_PTHREADT_STOP;
	sccp_globals_unlock(monitor_lock);
	sccp_mutex_destroy(&GLOB(monitor_lock));

	ast_log(LOG_NOTICE, "Running Cleanup\n");
#ifdef HAVE_LIBGC
	CHECK_LEAKS();
//      GC_gcollect();
#endif
	ast_log(LOG_NOTICE, "Module chan_sccp unloaded\n");
	return 0;
}

#if ASTERISK_VERSION_NUM >= 10400
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
#else
/*!
 * \brief 	number of instances of chan_sccp
 * \return	res number of instances
 */
int usecount()
{
	int res;

	sccp_globals_lock(usecnt_lock);
	res = GLOB(usecnt);
	sccp_globals_unlock(usecnt_lock);
	return res;
}

/*!
 * \brief 	Asterisk GPL Key
 * \return 	the asterisk key
 */
char *key()
{
	return ASTERISK_GPL_KEY;
}

/*!
 * \brief 	echo the module description
 * \return	chan_sccp description
 */
char *description()
{
	return ("Skinny Client Control Protocol (SCCP). Release: " SCCP_VERSION " " SCCP_BRANCH " - " SCCP_REVISION " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
}
#endif

#ifdef CS_DEVSTATE_FEATURE
const char devstate_astdb_family[] = "CustomDevstate";
#endif
