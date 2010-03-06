/*!
 * \file 	chan_sccp.c
 * \brief 	An implementation of Skinny Client Control Protocol (SCCP)
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \brief 	Main chan_sccp Class
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \date        $Date$
 * \version     $Revision$
 */

#define AST_MODULE "chan_sccp"

#include "config.h"
#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_hint.h"
#include "sccp_lock.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_cli.h"
#include "sccp_line.h"
#include "sccp_socket.h"
#include "sccp_pbx.h"
#include "sccp_indicate.h"
#include "sccp_config.h"
#include "sccp_management.h"
#include "sccp_mwi.h"
#include "sccp_conference.h"
#include <ctype.h>
#include <unistd.h>
#include <asterisk/pbx.h>
#ifdef CS_AST_HAS_APP_SEPARATE_ARGS
#include <asterisk/app.h>
#endif
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#include <asterisk/translate.h>
#ifdef CS_AST_HAS_AST_STRING_FIELD
#include <asterisk/stringfields.h>
#endif
#include <asterisk/astdb.h>
#include <asterisk/rtp.h>
#include <asterisk/frame.h>
#include <asterisk/channel.h>

#ifndef ASTERISK_CONF_1_2

void *sccp_create_hotline(void);

/*!
 * \brief	Buffer for Jitterbuffer use
 */
static struct ast_jb_conf default_jbconf =
{
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
	.impl = ""
};
#endif

/*!
 * \brief	Global null frame
 */
struct ast_frame 				sccp_null_frame;		/*!< Asterisk Structure */

/*!
 * \brief	Global variables
 */
struct sccp_global_vars 		* sccp_globals = 0;

/*!
 * \brief	Global scheduler and IO context
 */
struct sched_context 			* sched = 0;
struct io_context 				* io    = 0;


#ifndef ASTERISK_CONF_1_2
/*!
 * \brief	Buffer for Jitterbuffer use
 */
#ifndef CS_AST_HAS_RTP_ENGINE
struct ast_rtp_protocol sccp_rtp = {
	.type = "SCCP",
	.get_rtp_info = sccp_channel_get_rtp_peer,
	.set_rtp_peer = sccp_channel_set_rtp_peer,
	.get_vrtp_info = sccp_channel_get_vrtp_peer,
	.get_codec = sccp_device_get_codec,
};
#else
/*!
 * \brief using rtp enginge
 */
struct ast_rtp_glue sccp_rtp = {
	.type = "SCCP",
	.get_rtp_info = sccp_channel_get_rtp_peer,
	.get_vrtp_info = sccp_channel_get_vrtp_peer,
	.update_peer = sccp_channel_set_rtp_peer,
	.get_codec = sccp_device_get_codec,
};
#endif

#endif

#ifdef CS_AST_HAS_TECH_PVT
/*!
 * \brief	handle request coming from asterisk
 * \param	type	type of data as char
 * \param	format	format of data as int
 * \param	data	actual data
 * \param 	cause	Cause of the request
 * \return	Asterisk Channel
 */
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause) {
#else
/*!
 * \brief	handle request coming from asterisk
 * \param	type	type of data as char
 * \param	format	format of data as int
 * \param	data	actual data
 * \return	Asterisk Channel
 */
struct ast_channel *sccp_request(char *type, int format, void *data) {
#endif

	struct composedId lineSubscriptionId;
	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	char *options = NULL, *lineName = NULL;
	int optc = 0;
	char *optv[2];
	int opti = 0;
	int oldformat = format;


	memset(&lineSubscriptionId, 0, sizeof(struct composedId));
#ifdef CS_AST_HAS_TECH_PVT
	*cause = AST_CAUSE_NOTDEFINED;
#endif

	if (!type) {
		ast_log(LOG_NOTICE, "Attempt to call the wrong type of channel\n");
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	if (!data) {
		ast_log(LOG_NOTICE, "Attempt to call SCCP/ failed\n");
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	/* we leave the data unchanged */
	lineName = strdup(data);

	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	lineSubscriptionId = sccp_parseComposedId(lineName, 80);

	sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked to create a channel type=%s, format=%d, line=%s, subscriptionId.number=%s, options=%s\n", type, format, lineSubscriptionId.mainId, lineSubscriptionId.subscriptionId.number, (options) ? options : "");

	l = sccp_line_find_byname(lineSubscriptionId.mainId);

	if (!l) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineSubscriptionId.mainId);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}


	sccp_log(64)(VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (SCCP_LIST_FIRST(&l->devices) == NULL) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP/%s isn't currently registered anywhere.\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}

	sccp_log(64)(VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
	/* call forward check */
	
	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	 c = sccp_channel_allocate(l, NULL);
	 if (!c) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	 }

	
	/* set subscriberId for individual device addressing */
	if (!ast_strlen_zero(lineSubscriptionId.subscriptionId.number)) {
			sccp_copy_string(c->subscriptionId.number, lineSubscriptionId.subscriptionId.number, sizeof(c->subscriptionId.number));
			// Here it would be nice to add the name suffix as well.
			// However, this information is not available 
			// without explicit lookup over all relevant devices.
			ast_log(LOG_NOTICE, "%s: calling subscriber %s\n", l->id, c->subscriptionId.number);
	} else {
			sccp_copy_string(c->subscriptionId.number, l->defaultSubscriptionId.number, sizeof(c->subscriptionId.number));
			sccp_copy_string(c->subscriptionId.name, l->defaultSubscriptionId.name, sizeof(c->subscriptionId.name));
			ast_log(LOG_NOTICE, "%s: calling all subscribers with id %s\n", l->id, c->subscriptionId.number);
	}


if (!sccp_pbx_channel_allocate(c)) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		sccp_channel_delete(c);
		c = NULL;
		goto OUT;
	}


	
#if 0
	sccp_log(10)(VERBOSE_PREFIX_3 "Line %s has %d device%s\n", l->name, l->devices.size, (l->devices.size>1)?"s":"");
	if(l->devices.size < 2){
		if(!c->owner){
			sccp_log(64)(VERBOSE_PREFIX_3 "%s: channel has no owner\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
			*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
			sccp_channel_delete(c);
			c = NULL;
			goto OUT;
		}

		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call forward type: %d\n", l->name, l->cfwd_type);
		if (l->cfwd_type == SCCP_CFWD_ALL) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call forward (all) to %s\n", l->name, l->cfwd_num);
#ifdef CS_AST_HAS_AST_STRING_FIELD
			ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#else
			sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#endif
		} else if (l->cfwd_type == SCCP_CFWD_BUSY && l->channelCount > 1) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call forward (busy) to %s\n", l->name, l->cfwd_num);
#ifdef CS_AST_HAS_AST_STRING_FIELD
			ast_string_field_set(c->owner, call_forward, l->cfwd_num);
#else
			sccp_copy_string(c->owner->call_forward, l->cfwd_num, sizeof(c->owner->call_forward));
#endif
		}
	}
#endif

	sccp_log(1)(VERBOSE_PREFIX_1 "[SCCP] in file %s, line %d (%s)\n" ,__FILE__, __LINE__, __PRETTY_FUNCTION__);
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
			//TODO check capability on shared lines
			format &= device->capability;
			if(device->session)
				hasSession = TRUE;
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}*/

	if (l->devices.size == 0) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP/%s we have no registered devices for this line.\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}
	/*
	if (!format) {
		format = oldformat;
		res = ast_translator_best_choice(&format, &GLOB(global_capability));
		if (res < 0) {
			ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format '%d'\n", oldformat);
#ifdef CS_AST_HAS_TECH_PVT
			*cause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
#endif
			goto OUT;
		}
	}*/


	 c->format = oldformat;
	 c->isCodecFix = TRUE;
	 sccp_channel_updateChannelCapability(c);

	/* we don't need to parse any options when we have a call forward status */
// 	if (c->owner && !ast_strlen_zero(c->owner->call_forward))
// 		goto OUT;

	/* check for the channel params */
	if (options && (optc = sccp_app_separate_args(options, '/', optv, sizeof(optv) / sizeof(optv[0])))) {

		for (opti = 0; opti < optc; opti++) {
			if (!strncasecmp(optv[opti], "aa", 2)) {
				/* let's use the old style auto answer aa1w and aa2w */
				if (!strncasecmp(optv[opti], "aa1w", 4)) {
						c->autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti]+=4;
				} else if (!strncasecmp(optv[opti], "aa2w", 4)) {
						c->autoanswer_type = SCCP_AUTOANSWER_2W;
						optv[opti]+=4;
				} else if (!strncasecmp(optv[opti], "aa=", 3)) {
					optv[opti] += 3;
					if (!strncasecmp(optv[opti], "1w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_1W;
						optv[opti]+=2;
					} else if (!strncasecmp(optv[opti], "2w", 2)) {
						c->autoanswer_type = SCCP_AUTOANSWER_2W;
						optv[opti]+=2;
					}
				}

				/* since the pbx ignores autoanswer_cause unless channelCount > 1, it is safe to set it if provided */
				if (!ast_strlen_zero(optv[opti]) && (c->autoanswer_type)) {
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

	sccp_restart_monitor();
	return (c && c->owner ? c->owner : NULL);
}

/*!
 * \brief returns the state of device
 * \param data name of device
 * \return devicestate of AST_DEVICE_*
 */
int sccp_devicestate(void *data) {
	sccp_line_t * l =  NULL;
	int res = AST_DEVICE_UNKNOWN;
	char *lineName = NULL, *options = NULL;
	
	
	/* exclude options */
	lineName = strdup(data);
	if ((options = strchr(lineName, '/'))) {
		*options = '\0';
		options++;
	}

	l = sccp_line_find_byname(lineName);
	if (!l)
		res = AST_DEVICE_INVALID;
	else if (SCCP_LIST_FIRST(&l->devices) == NULL)
		res = AST_DEVICE_UNAVAILABLE;

	//TODO handle dnd on device
//	else if ((l->device->dnd && l->device->dndmode == SCCP_DNDMODE_REJECT)
//			|| (l->dnd && (l->dndmode == SCCP_DNDMODE_REJECT
//					|| (l->dndmode == SCCP_DNDMODE_USERDEFINED && l->dnd == SCCP_DNDMODE_REJECT) )) )
//		res = AST_DEVICE_BUSY;
	else if (l->incominglimit && l->channelCount == l->incominglimit)
		res = AST_DEVICE_BUSY;
	else if (!l->channelCount)
		res = AST_DEVICE_NOT_INUSE;
#ifdef CS_AST_DEVICE_RINGING
	else if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGING))
		res = AST_DEVICE_RINGING;
#endif
#ifdef CS_AST_DEVICE_RINGINUSE
	else if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_RINGING) && sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_CONNECTED))
		res = AST_DEVICE_RINGINUSE;
#endif
#ifdef CS_AST_DEVICE_ONHOLD
	else if (sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_HOLD))
		res = AST_DEVICE_ONHOLD;
#endif
	else
		res = AST_DEVICE_INUSE;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked for the state (%d) of the line %s\n", res, (char *)data);

	return res;
}

/*!
 * \brief 	Controller function to handle Received Messages
 * \param 	r Message as sccp_moo_t
 * \param 	s Session as sccp_session_t
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s) {
	uint32_t  mid = letohl(r->lel_messageId);
	//sccp_log(1)(VERBOSE_PREFIX_3 "%s: last keepAlive within %d (%d)\n", (s->device)?s->device->id:"null", (uint32_t)(time(0) - s->lastKeepAlive), (s->device)?s->device->keepalive:0 );
	s->lastKeepAlive = time(0); /* always update keepalive */

  //sccp_log(10)(VERBOSE_PREFIX_3 "%s(%d): >> Got message %s\n", (s->device)?s->device->id:"null", s->sin.sin_addr.s_addr ,message2str(mid));
  //sccp_log(10)(VERBOSE_PREFIX_3 "%s(%d): >> Got message %s\n", (s->device)?s->device->id:"null", (s->device)?s->device->session->sin.sin_addr.s_addr:0 ,message2str(mid));
  /* try to handle nat problem.
   * devices behind nat box can not do anything after ip address change */
//  if(s && s->device && s->device->session){
//	    if(s->sin.sin_addr.s_addr != s->device->session->sin.sin_addr.s_addr){
//		  ast_log(LOG_WARNING, "We have one device with diffenent ip addresse. Restart each session.\n");
//		  s->device->nat = 1;
//		  //sccp_moo_t * r;
//		  //REQ(r, Reset);
//		  //r->msg.Reset.lel_resetType = htolel(SKINNY_DEVICE_RESTART);
//		  //sccp_dev_send(s->device, r);
//		  sccp_session_close(s);
//		  sccp_session_close(s->device->session);
//
//	}
//  }

	if ( (!s->device) && (mid != RegisterMessage && mid != UnregisterMessage && mid != RegisterTokenReq && mid != AlarmMessage && mid != KeepAliveMessage && mid != IpPortMessage)) {
		ast_log(LOG_WARNING, "SCCP: Client sent %s without first registering. Attempting reconnect.\n", message2str(mid));
		if(!(s->device = sccp_device_find_byipaddress(s->sin.sin_addr.s_addr))) {
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Device attempt to reconnect failed. Restarting device\n");
			ast_free(r);
			return 0;
		} else {
			/* this prevent loops -FS */
			if(s->device->session != s) {
				sccp_log(1)(VERBOSE_PREFIX_3 "%s: cross device session\n", DEV_ID_LOG(s->device));
				sccp_session_close(s->device->session);
				s->device->session = s;
			}
		}
	}



	if (mid != KeepAliveMessage) {
		if (s && s->device) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: >> Got message %s\n", s->device->id, message2str(mid));
		} else {
			sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: >> Got message %s\n", message2str(mid));
		}
	}

	switch (mid) {

		case AlarmMessage:
		      sccp_handle_alarm(s, r);
		      break;
		      case RegisterMessage:
		      case RegisterTokenReq:
		      sccp_handle_register(s, r);
		      break;
		case UnregisterMessage:
		      sccp_handle_unregister(s, r);
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
		case KeepAliveMessage:
		      sccp_session_sendmsg(s->device, KeepAliveAckMessage);
		      break;
		case IpPortMessage:
		      /* obsolete message */
		      s->rtpPort = letohs(r->msg.IpPortMessage.les_rtpMediaPort);
		      break;
		case OpenReceiveChannelAck:
		      sccp_handle_open_receive_channel_ack(s, r);
		      break;
		case ConnectionStatisticsRes:
		      sccp_handle_ConnectionStatistics(s,r);
		      break;
		case ServerReqMessage:
		      sccp_handle_ServerResMessage(s,r);
		      break;
		case ConfigStatReqMessage:
		      sccp_handle_ConfigStatMessage(s,r);
		      break;
		case EnblocCallMessage:
		      sccp_handle_EnblocCallMessage(s,r);
		      break;
		case RegisterAvailableLinesMessage:
		      //if (s->device)
			  //    sccp_dev_set_registered(s->device, SKINNY_DEVICE_RS_OK);
		      break;
		case ForwardStatReqMessage:
		      sccp_handle_forward_stat_req(s,r);
		      break;
		case FeatureStatReqMessage:
		      sccp_handle_feature_stat_req(s,r);
		      break;
		case ServiceURLStatReqMessage:
		      sccp_handle_services_stat_req(s,r);
		      break;
		case AccessoryStatusMessage:
		      sccp_handle_accessorystatus_message(s,r);
		      break;
		case DialedPhoneBookMessage:
		      sccp_handle_dialedphonebook_message(s,r);
		      break;
		case UpdateCapabilitiesMessage:
		      sccp_handle_updatecapabilities_message(s,r);
		      break;
		case StartMediaTransmissionAck:
		      sccp_handle_startmediatransmission_ack(s, r);
		      break;
		case Unknown_0x004A_Message:
                      if (GLOB(debug) >20) {
		        sccp_handle_unknown_message(s,r);
		      }
		      break;
		case Unknown_0x0143_Message:
                      if (GLOB(debug) >20) {
		        sccp_handle_unknown_message(s,r);
		      }
		      break;
		case Unknown_0x0144_Message:
                      if (GLOB(debug) >20) {
		        sccp_handle_unknown_message(s,r);
		      }
		      break;
		case Unknown_0x0149_Message:
                      if (GLOB(debug) >20) {
		        sccp_handle_unknown_message(s,r);
		      }
		      break;
		case Unknown_0x0159_Message:
                      if (GLOB(debug) >20) {
		        sccp_handle_unknown_message(s,r);
		      }
		      break;
		default:
		      sccp_handle_unknown_message(s,r);
	}

	ast_free(r);
	return 1;
}




/**
 * \brief reload the configuration from sccp.conf
 *
 */
static int reload_config(void) {
	int						oldport	= ntohs(GLOB(bindaddr.sin_port));
	int						on		= 1;
//	int						tos		= 0;
//	char					pref_buf[128];
//	struct ast_hostent		ahp;
//	struct hostent			*hp;
//	struct ast_ha 			*na;
#ifdef ASTERISK_CONF_1_2
	char					iabuf[INET_ADDRSTRLEN];
#endif


#ifndef ASTERISK_CONF_1_2
	/* Copy the default jb config over global_jbconf */
	memcpy(&GLOB(global_jbconf), &default_jbconf, sizeof(struct ast_jb_conf));

	/* Setup the monitor thread default */
	GLOB(monitor_thread) = AST_PTHREADT_NULL;	// ADDED IN SVN 414 -FS
	GLOB(mwiMonitorThread) = AST_PTHREADT_NULL;
#endif

	memset(&GLOB(global_codecs), 0, sizeof(GLOB(global_codecs)));
	memset(&GLOB(bindaddr), 0, sizeof(GLOB(bindaddr)));
	GLOB(allowAnonymus) = TRUE;

#ifdef CS_SCCP_REALTIME
	sccp_copy_string(GLOB(realtimedevicetable), "sccpdevice", sizeof(GLOB(realtimedevicetable)));
	sccp_copy_string(GLOB(realtimelinetable) , "sccpline", sizeof(GLOB(realtimelinetable)) );
#endif

#if SCCP_PLATFORM_BYTE_ORDER == SCCP_LITTLE_ENDIAN
	sccp_log(0)(VERBOSE_PREFIX_2 "Platform byte order   : LITTLE ENDIAN\n");
#else
	sccp_log(0)(VERBOSE_PREFIX_2 "Platform byte order   : BIG ENDIAN\n");
#endif

	if( !sccp_config_general() ){
		return 0;
	}
	sccp_config_readDevicesLines(SCCP_CONFIG_READINITIAL);
	/* ok the config parse is done */
	if ((GLOB(descriptor) > -1) && (ntohs(GLOB(bindaddr.sin_port)) != oldport)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

	if (GLOB(descriptor) < 0)
	{
		GLOB(descriptor) = socket(AF_INET, SOCK_STREAM, 0);

		on = 1;
		if (setsockopt(GLOB(descriptor), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket to SO_REUSEADDR mode: %s\n", strerror(errno));
		if (setsockopt(GLOB(descriptor), IPPROTO_IP, IP_TOS, &GLOB(tos), sizeof(GLOB(tos))) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket TOS to IPTOS_LOWDELAY: %s\n", strerror(errno));
		if (setsockopt(GLOB(descriptor), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
			ast_log(LOG_WARNING, "Failed to set SCCP socket to TCP_NODELAY: %s\n", strerror(errno));

		if (GLOB(descriptor) < 0) {

			ast_log(LOG_WARNING, "Unable to create SCCP socket: %s\n", strerror(errno));
	}
	else
	{
		if (bind(GLOB(descriptor), (struct sockaddr *)&GLOB(bindaddr), sizeof(GLOB(bindaddr))) < 0) {
#ifdef ASTERISK_CONF_1_2
			ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n",	ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
#else
			ast_log(LOG_WARNING, "Failed to bind to %s:%d: %s!\n",	ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)),strerror(errno));
#endif
			close(GLOB(descriptor));
			GLOB(descriptor) = -1;
			return 0;
		}
#ifdef ASTERISK_CONF_1_2
		ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
		ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", 	ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif

		if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
#ifdef ASTERISK_CONF_1_2
			ast_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)), strerror(errno));
#else
			ast_log(LOG_WARNING, "Failed to start listening to %s:%d: %s\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)),	strerror(errno));
#endif
			close(GLOB(descriptor));
			GLOB(descriptor) = -1;
			return 0;
		}
#ifdef ASTERISK_CONF_1_2
		sccp_log(0)(VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", ast_inet_ntoa(iabuf, sizeof(iabuf), GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#else
		sccp_log(0)(VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", ast_inet_ntoa(GLOB(bindaddr.sin_addr)), ntohs(GLOB(bindaddr.sin_port)));
#endif
		ast_pthread_create(&GLOB(socket_thread), NULL, sccp_socket_thread, NULL);
	}
  }

  
  sccp_mwi_module_stop();
  sccp_restart_monitor();
  return 0;
}


/*!
 * \brief 	create a hotline
 */
void *sccp_create_hotline(void){
      sccp_line_t	*hotline;

      hotline = sccp_line_create();
      sccp_copy_string(hotline->name, "Hotline", sizeof(hotline->name));
      sccp_copy_string(hotline->cid_name, "hotline", sizeof(hotline->cid_name));
      sccp_copy_string(hotline->cid_num, "hotline", sizeof(hotline->cid_name));
      sccp_copy_string(hotline->context, "default", sizeof(hotline->context));
      sccp_copy_string(hotline->label, "hotline", sizeof(hotline->label));
      sccp_copy_string(hotline->adhocNumber, "111", sizeof(hotline->adhocNumber));
      
      //sccp_copy_string(hotline->mailbox, "hotline", sizeof(hotline->mailbox));

      SCCP_LIST_LOCK(&GLOB(lines));
      SCCP_LIST_INSERT_HEAD(&GLOB(lines), hotline, list);
      SCCP_LIST_UNLOCK(&GLOB(lines));

      GLOB(hotline)->line = hotline;
      sccp_copy_string(GLOB(hotline)->exten, "111", sizeof(GLOB(hotline)->exten));

      return NULL;
}

/*!
 * \brief 	start monitoring thread of chan_sccp
 * \param 	data
 */
void *sccp_do_monitor(void *data)
{
	int res;

	/* This thread monitors all the interfaces which are not yet in use
	   (and thus do not have a separate thread) indefinitely */
	/* From here on out, we die whenever asked */
	for(;;) {
		pthread_testcancel();
		/* Wait for sched or io */
		res = ast_sched_wait(sched);
		if ((res < 0) || (res > 1000)) {
			res = 1000;
		}
		res = ast_io_wait(io, res);
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
 */
int sccp_restart_monitor(void)
{
	/* If we're supposed to be stopped -- stay stopped */
	if (GLOB(monitor_thread) == AST_PTHREADT_STOP)
		return 0;

	ast_mutex_lock(&GLOB(monitor_lock));
	if (GLOB(monitor_thread) == pthread_self()) {
		ast_mutex_unlock(&GLOB(monitor_lock));
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Cannot kill myself\n");
		return -1;
	}
	if (GLOB(monitor_thread) != AST_PTHREADT_NULL) {
		/* Wake up the thread */
		pthread_kill(GLOB(monitor_thread), SIGURG);
	} else {
		/* Start a new monitor */
		if (ast_pthread_create_background(&GLOB(monitor_thread), NULL, sccp_do_monitor, NULL) < 0) {
			ast_mutex_unlock(&GLOB(monitor_lock));
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_restart_monitor) Unable to start monitor thread.\n");
			return -1;
		}
	}
	ast_mutex_unlock(&GLOB(monitor_lock));
	return 0;
}

/*!
 * \brief 	Set the Called Party name a number over an SCCP channel made by chan_sccp.
 */
static char *sccp_setcalledparty_descrip = "  SetCalledParty(\"Name\" <ext>) sets the name and number of the called party for use with chan_sccp\n";

/*!
 * \brief 	Set the Name and Number of the Called Party to the Calling Phone
 * \param 	chan Asterisk Channel
 * \param 	data CallerId in format "Name" \<number\>
 * \return	Success as int
 */
static int sccp_setcalledparty_exec(struct ast_channel *chan, void *data) {
	char tmp[256] = "";
	char * num, * name;
	sccp_channel_t * c = CS_AST_CHANNEL_PVT(chan);

#ifndef CS_AST_HAS_TECH_PVT
	if (strcasecmp(chan->type, "SCCP") != 0)
		return 0;
#else
	if (strcasecmp(chan->tech->type, "SCCP") != 0)
		return 0;
#endif

	if (!data || !c)
		return 0;

	sccp_copy_string(tmp, (char *)data, sizeof(tmp));
	ast_callerid_parse(tmp, &name, &num);
	sccp_channel_set_calledparty(c, name, num);

	return 0;
}


static char *sccp_setmessage_descrip = "  SetMessage(\"Message\") sets a display message for use with chan_sccp. Clear the display with empty message\n";
/*!
 * \brief	It allows you to send a message to the calling device.
 * \author	Frank Segtrop <fs@matflow.net>
 * \param	chan asterisk channel
 * \param	data message to sent - if empty clear display
 * \version	20071112_1944
 */
static int sccp_setmessage_exec(struct ast_channel *chan, void *data) {
	char tmp[256] 		= "";
	sccp_channel_t 	* c = NULL;
	sccp_device_t 	* d;

	c = CS_AST_CHANNEL_PVT(chan);
#ifndef CS_AST_HAS_TECH_PVT
 	if (strcasecmp(chan->type, "SCCP") != 0)
 		return 0;
#else
 	if (strcasecmp(chan->tech->type, "SCCP") != 0)
 		return 0;
#endif

 	if (!data || !c ||!c->device)
 		return 0;

 	sccp_copy_string(tmp, (char *)data, sizeof(tmp));

	d = c->device;
	sccp_device_lock(d);
	if (strlen(tmp)>0) {
		sccp_dev_displayprinotify(d,tmp,5,0);
		sccp_dev_displayprompt(d,0,0,tmp,0);
		d->phonemessage = strdup(tmp);
		ast_db_put("SCCPM", d->id, tmp);
	}
	else {
		sccp_dev_displayprinotify(d,"Message off",5,1);
		sccp_dev_displayprompt(d,0,0,"Message off",1);
		d->phonemessage = NULL;
		ast_db_del("SCCPM", d->id);
	}
	sccp_device_unlock(d);
	return 0;
}

#ifndef ASTERISK_CONF_1_2
/*!
 * \brief	Initialize and Astersik RTP Bridge
 * \param	c0	Asterisk Channel 0
 * \param	c1	Asterisk Channel 1
 * \param	flags	Flags as int
 * \param	fo	Asterisk Frame
 * \param	rc	Asterisk Channel
 * \param	timeoutms Time Out in Millisecs as int
 * \return 	Asterisk Bridge Result as enum
 */
enum ast_bridge_result sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms) {
	struct ast_rtp *p0 = NULL, *p1 = NULL;		/* Audio RTP Channels */
	enum ast_rtp_get_result audio_p0_res = AST_RTP_GET_FAILED;
	enum ast_rtp_get_result audio_p1_res = AST_RTP_GET_FAILED;
	enum ast_bridge_result res = AST_BRIDGE_FAILED;

	sccp_channel_t *pvt0 = NULL, *pvt1 = NULL;

	/* Lock channels */
	sccp_ast_channel_lock(c0);
	while (ast_channel_trylock(c1)) {
		sccp_ast_channel_unlock(c0);
		usleep(1);
		sccp_ast_channel_lock(c0);
	}

	/* Ensure neither channel got hungup during lock avoidance */
	if (ast_check_hangup(c0) || ast_check_hangup(c1)) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_rtp_bridge) Got hangup while attempting to bridge '%s' and '%s'\n", c0->name, c1->name);
		ast_channel_unlock(c0);
		ast_channel_unlock(c1);
		return AST_BRIDGE_FAILED;
	}

	/* Get channel specific interface structures */
	pvt0 = c0->tech_pvt;
	pvt1 = c1->tech_pvt;

	/* Get audio and video interface (if native bridge is possible) */
	audio_p0_res = sccp_channel_get_rtp_peer(c0, &p0);
	audio_p1_res = sccp_channel_get_rtp_peer(c1, &p1);

	/* Check if a bridge is possible (partial/native) */
	if (audio_p0_res == AST_RTP_GET_FAILED || audio_p1_res == AST_RTP_GET_FAILED) {
		/* Somebody doesn't want to play... */
		sccp_ast_channel_unlock(c0);
		sccp_ast_channel_unlock(c1);
		return AST_BRIDGE_FAILED_NOWARN;
	}

	/* If either side can only do a partial bridge, then don't try for a true native bridge */
	if (audio_p0_res == AST_RTP_TRY_PARTIAL || audio_p1_res == AST_RTP_TRY_PARTIAL) {
		struct ast_format_list fmt0, fmt1;

		/* In order to do Packet2Packet bridging both sides must be in the same rawread/rawwrite */
		if (c0->rawreadformat != c1->rawwriteformat || c1->rawreadformat != c0->rawwriteformat) {
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_rtp_bridge) Cannot packet2packet bridge - raw formats are incompatible\n");
			sccp_ast_channel_unlock(c0);
			sccp_ast_channel_unlock(c1);
			return AST_BRIDGE_FAILED_NOWARN;
		}

		/* They must also be using the same packetization */
		fmt0 = ast_codec_pref_getsize(&pvt0->device->codecs, c0->rawreadformat);
		fmt1 = ast_codec_pref_getsize(&pvt1->device->codecs, c1->rawreadformat);
		if (fmt0.cur_ms != fmt1.cur_ms) {
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_rtp_bridge) Cannot packet2packet bridge - packetization settings prevent it\n");
			sccp_ast_channel_unlock(c0);
			sccp_ast_channel_unlock(c1);
			return AST_BRIDGE_FAILED_NOWARN;
		}

		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_rtp_bridge) Packet2Packet bridging '%s' and '%s'\n", c0->name, c1->name);
		res = AST_BRIDGE_FAILED_NOWARN;
	} else {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: (sccp_rtp_bridge) Native bridging '%s' and '%s'\n", c0->name, c1->name);
#warning "The purpose of this code is obscure and must be clarified (-DD)"

/*
		sccp_pbx_set_rtp_peer(c0, pvt1->rtp, NULL, NULL, pvt1->device->codecs, 0);
		sccp_pbx_set_rtp_peer(c1, pvt0->rtp, NULL, NULL, pvt0->device->codecs, 0);
*/
		res = AST_BRIDGE_FAILED; //_NOWARN;
	}

	sccp_ast_channel_unlock(c0);
	sccp_ast_channel_unlock(c1);

	return res;
}
#endif

#ifdef ASTERISK_CONF_1_2
/*!
 * \brief 	Load the actual chan_sccp module
 * \return	Success as int
 */
int load_module() {
#else
static int load_module(void) {
#endif
	/* make globals */
	sccp_globals = ast_malloc(sizeof(struct sccp_global_vars));
	sccp_event_listeners = ast_malloc(sizeof(struct sccp_event_subscriptions));
	if (!sccp_globals || !sccp_event_listeners) {
		ast_log(LOG_ERROR, "No free memory for SCCP global vars. SCCP channel type disabled\n");
#ifdef ASTERISK_CONF_1_2
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

	memset(sccp_globals,0,sizeof(struct sccp_global_vars));
	memset(sccp_event_listeners,0,sizeof(struct sccp_event_subscriptions));
	ast_mutex_init(&GLOB(lock));
	ast_mutex_init(&GLOB(usecnt_lock));
	ast_mutex_init(&GLOB(monitor_lock));
	SCCP_LIST_HEAD_INIT(&GLOB(sessions));
	SCCP_LIST_HEAD_INIT(&GLOB(devices));
	SCCP_LIST_HEAD_INIT(&GLOB(lines));

	/* */
	SCCP_LIST_HEAD_INIT(&sccp_mailbox_subscriptions);
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
	GLOB(keepalive)  = SCCP_KEEPALIVE;
	sccp_copy_string(GLOB(date_format), "D/M/YA", sizeof(GLOB(date_format)));
	sccp_copy_string(GLOB(context), "default", sizeof(GLOB(context)));
	sccp_copy_string(GLOB(servername), "Asterisk", sizeof(GLOB(servername)));

	/* Wait up to 16 seconds for first digit */
	GLOB(firstdigittimeout) = 16;
	/* How long to wait for following digits */
	GLOB(digittimeout) = 8;
	/* Yes, these are all that the phone supports (except it's own 'Wideband 256k') */
	GLOB(global_capability) = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A | AST_FORMAT_H263;

	GLOB(debug) = 1;
	GLOB(fdebug) = 0;
	GLOB(tos) = (0x68 & 0xff);
	GLOB(rtptos) = (184 & 0xff);
	GLOB(echocancel) = 1;
	GLOB(silencesuppression) = 0;
	GLOB(dndmode) = SCCP_DNDMODE_REJECT;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = 1; /* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	GLOB(amaflags) = ast_cdr_amaflags2int("documentation");
	GLOB(callAnswerOrder) = ANSWER_OLDEST_FIRST;
	GLOB(hotline) = ast_malloc(sizeof(sccp_hotline_t));
	memset(GLOB(hotline),0,sizeof(sccp_hotline_t));

	sccp_create_hotline();

	if (!reload_config()) {
#ifdef CS_AST_HAS_TECH_PVT
		if (ast_channel_register(&sccp_tech)) {
#else
		if (ast_channel_register_ex("SCCP", "SCCP", GLOB(global_capability), sccp_request , sccp_devicestate)) {
#endif
			ast_log(LOG_ERROR, "Unable to register channel class SCCP\n");
			return -1;
		}
	}

#ifndef ASTERISK_CONF_1_2
#ifndef CS_AST_HAS_RTP_ENGINE
	ast_rtp_proto_register(&sccp_rtp);
#else
	ast_rtp_glue_register(&sccp_rtp);
#endif
#endif

#ifdef CS_SCCP_MANAGER
	sccp_register_management();
#endif

	sccp_register_cli();
	ast_register_application("SetCalledParty", sccp_setcalledparty_exec, "Sets the name of the called party", sccp_setcalledparty_descrip);
	ast_register_application("SetMessage", sccp_setmessage_exec, "Sets a message on the calling device", sccp_setmessage_descrip);

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
int sccp_sched_free(void *ptr){
	if(!ptr)
		return -1;
	
	ast_free(ptr);
	return 0;
	
}
#endif


#ifdef ASTERISK_CONF_1_2
/*!
 * \brief 	Unload the chan_sccp module
 * \return	Success as int
 */
int unload_module() {
	char iabuf[INET_ADDRSTRLEN];
#else
static int unload_module(void) {
#endif
	sccp_channel_t *c;
	sccp_line_t * l;
	sccp_device_t * d;
	sccp_session_t * s;

#ifndef ASTERISK_CONF_1_2
	ast_rtp_proto_unregister(&sccp_rtp);
#endif
#ifdef CS_SCCP_MANAGER
	sccp_unregister_management();
#endif

#ifdef CS_AST_HAS_TECH_PVT
	ast_channel_unregister(&sccp_tech);
#else
	ast_channel_unregister("SCCP");
#endif
	ast_unregister_application("SetMessage");
	ast_unregister_application("SetCalledParty");
	sccp_unregister_cli();
	sccp_mwi_module_stop();

	sccp_hint_module_stop();

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing monitor thread\n");
	sccp_globals_lock(monitor_lock);
	if ((GLOB(monitor_thread) != AST_PTHREADT_NULL) && (GLOB(monitor_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(monitor_thread));
		pthread_kill(GLOB(monitor_thread), SIGURG);
		pthread_join(GLOB(monitor_thread), NULL);
	}
	GLOB(monitor_thread) = AST_PTHREADT_STOP;
	sccp_globals_unlock(monitor_lock);
	sccp_mutex_destroy(&GLOB(monitor_lock));


	/* removing devices */
	SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE(&GLOB(devices), d, list){
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		sccp_dev_clean(d, TRUE, 0);
	}
	SCCP_LIST_UNLOCK(&GLOB(devices));

	if(SCCP_LIST_EMPTY(&GLOB(devices)))
		SCCP_LIST_HEAD_DESTROY(&GLOB(devices));

	/* hotline will be removed by line removing function */
	GLOB(hotline)->line = NULL;
	ast_free(GLOB(hotline));

	/* removing lines */
	SCCP_LIST_LOCK(&GLOB(lines));
	while ((l = SCCP_LIST_REMOVE_HEAD(&GLOB(lines), list))) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing line %s\n", l->name);

		/* removing channels */
		SCCP_LIST_LOCK(&l->channels);
		while ((c = SCCP_LIST_REMOVE_HEAD(&l->channels, list))) {
			sccp_channel_cleanbeforedelete(c);
			sccp_channel_delete_no_lock(c);
		}
		SCCP_LIST_UNLOCK(&l->channels);
		SCCP_LIST_HEAD_DESTROY(&l->channels);

		//if (l->cfwd_num)
		//	ast_free(l->cfwd_num);
		if (l->trnsfvm)
			ast_free(l->trnsfvm);
		ast_free(l);
	}
	SCCP_LIST_UNLOCK(&GLOB(lines));
	SCCP_LIST_HEAD_DESTROY(&GLOB(lines));




	/* removing sessions */
	SCCP_LIST_LOCK(&GLOB(sessions));
	while ((s = SCCP_LIST_REMOVE_HEAD(&GLOB(sessions), list))) {
#ifdef ASTERISK_CONF_1_2
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", ast_inet_ntoa(iabuf, sizeof(iabuf), s->sin.sin_addr));
#else
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing session %s\n", ast_inet_ntoa(s->sin.sin_addr));
#endif
		if (s->fd > -1)
			close(s->fd);
		ast_mutex_destroy(&s->lock);
		ast_free(s);
	}
	SCCP_LIST_UNLOCK(&GLOB(sessions));
	SCCP_LIST_HEAD_DESTROY(&GLOB(sessions));

	close(GLOB(descriptor));
	GLOB(descriptor) = -1;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Killing the socket thread\n");

	sccp_globals_lock(socket_lock);
	if ((GLOB(socket_thread) != AST_PTHREADT_NULL) &&
		(GLOB(socket_thread) != AST_PTHREADT_STOP)) {
		pthread_cancel(GLOB(socket_thread));
		pthread_kill(GLOB(socket_thread), SIGURG);
		pthread_join(GLOB(socket_thread), NULL);
	}
	GLOB(socket_thread) = AST_PTHREADT_STOP;
	sccp_globals_unlock(socket_lock);
	sccp_mutex_destroy(&GLOB(socket_lock));

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Killed the socket thread\n");

	if (GLOB(ha))
		ast_free_ha(GLOB(ha));

	if (GLOB(localaddr))
		ast_free_ha(GLOB(localaddr));

	if (io)
		io_context_destroy(io);
	if (sched)
		sched_context_destroy(sched);

	ast_mutex_destroy(&GLOB(usecnt_lock));
	ast_mutex_destroy(&GLOB(lock));
	ast_free(sccp_globals);

	return 0;
}

#ifndef ASTERISK_CONF_1_2
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Skinny Client Control Protocol (SCCP). Release: " SCCP_BRANCH " - " SCCP_REVISION " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
#else
/*!
 * \brief 	number of instances of chan_sccp
 * \return	res number of instances
 */
int usecount() {
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
char *key() {
        return ASTERISK_GPL_KEY;
}

/*!
 * \brief 	echo the module description
 * \return	chan_sccp description
 */
char *description() {
	return ("Skinny Client Control Protocol (SCCP). Release: " SCCP_BRANCH " - " SCCP_REVISION " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
}
#endif
