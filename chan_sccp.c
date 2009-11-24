/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */



/**
 * \mainpage chan_sccp-b User documentation
 *
 * \ref intro_sk The Softkeys
 *
 *
 */
#define AST_MODULE "chan_sccp"

#include "config.h"


#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
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








/* for jitter buffer use */
static struct ast_jb_conf default_jbconf =
{
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
	.impl = ""
};
#endif

#ifndef ASTERISK_CONF_1_2
struct ast_rtp_protocol sccp_rtp = {
	.type = "SCCP",
	.get_rtp_info = sccp_channel_get_rtp_peer,
	.set_rtp_peer = sccp_channel_set_rtp_peer,
	.get_codec = sccp_device_get_codec,
};
#endif

/**
 *
 *
 *
 *
 */
#ifdef CS_AST_HAS_TECH_PVT
struct ast_channel *sccp_request(const char *type, int format, void *data, int *cause) {
#else
struct ast_channel *sccp_request(char *type, int format, void *data) {
#endif

	sccp_device_t 	*device = NULL;
	sccp_line_t * l = NULL;
	sccp_channel_t * c = NULL;
	char *options = NULL, *deviceName=NULL, *lineName = NULL;
	int optc = 0;
	char *optv[2];
	int opti = 0;
	int res = 0;
	int oldformat = format;

	boolean_t	hasSession = FALSE;

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

	if ((deviceName = strchr(lineName, '@'))) {
		*deviceName = '\0';
		deviceName++;
	}

	if ((options = strchr(deviceName?deviceName:lineName, '/'))) {
		*options = '\0';
		options++;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Asterisk asked to create a channel type=%s, format=%d, data=%s, device=%s options=%s\n", type, format, lineName,(deviceName)?deviceName:"" , (options) ? options : "");

	l = sccp_line_find_byname(lineName);

	if (!l) {
		sccp_log(1)(VERBOSE_PREFIX_3 "SCCP/%s does not exist!\n", lineName);
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
	sccp_line_lock(l);


	sccp_line_unlock(l);

	// Allocate a new SCCP channel.
	/* on multiline phone we set the line when answering or switching lines */
	 c = sccp_channel_allocate(l, NULL);
	 if (!c) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	 }

	if (!sccp_pbx_channel_allocate(c)) {
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		sccp_channel_delete(c);
		c = NULL;
		goto OUT;
	}
sccp_log(64)(VERBOSE_PREFIX_3 "Line %s has %d device%s\n", l->name, l->devices.size, (l->devices.size>1)?"s":"");
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



	 /* we have a single device given */
	if(deviceName){
		if( !(c->device = sccp_device_find_byid(deviceName, TRUE)) ){
			/* but can't find it -> cleanup*/
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		sccp_channel_delete(c);
		c = NULL;
		goto OUT;
		}
	}


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
	}

	if (!hasSession) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP/%s we have no registered devices for this line.\n", l->name);
#ifdef CS_AST_HAS_TECH_PVT
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
#endif
		goto OUT;
	}
	if (!format) {
		format = oldformat;
		res = ast_translator_best_choice(&format, &l->capability);
		if (res < 0) {
			ast_log(LOG_NOTICE, "Asked to get a channel of unsupported format '%d'\n", oldformat);
#ifdef CS_AST_HAS_TECH_PVT
			*cause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
#endif
			goto OUT;
		}
	}


	 c->format = oldformat;



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

/**
 * returns the state of device
 *
 * \param data name of device
 * \return devicestate of AST_DEVICE_*
 */
int sccp_devicestate(void * data) {
	sccp_line_t * l =  NULL;
	int res = AST_DEVICE_UNKNOWN;

	l = sccp_line_find_byname((char*)data);
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




/**
 *
 * \brief controller function to handle received messages
 * \param r message
 * \param s session
 */
uint8_t sccp_handle_message(sccp_moo_t * r, sccp_session_t * s) {
	uint32_t  mid = letohl(r->lel_messageId);
	s->lastKeepAlive = time(0); /* always update keepalive */

  //sccp_log(10)(VERBOSE_PREFIX_3 "%s(%d): >> Got message %s\n", (s->device)?s->device->id:"null", s->sin.sin_addr.s_addr ,sccpmsg2str(mid));
  //sccp_log(10)(VERBOSE_PREFIX_3 "%s(%d): >> Got message %s\n", (s->device)?s->device->id:"null", (s->device)?s->device->session->sin.sin_addr.s_addr:0 ,sccpmsg2str(mid));
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
		ast_log(LOG_WARNING, "SCCP: Client sent %s without first registering. Attempting reconnect.\n", sccpmsg2str(mid));
		if(!(s->device = sccp_device_find_byipaddress(s->sin.sin_addr.s_addr))) {
			sccp_log(1)(VERBOSE_PREFIX_3 "SCCP: Device attempt to reconnect failed. Restarting device\n");
			ast_free(r);
			sccp_device_reset(s, SKINNY_DEVICE_RESTART);


			return 0;
		} else {
			/* this prevent loops -FS */
			if(s->device->session != s) {
				sccp_session_close(s->device->session);
				s->device->session = s;
			}
		}
	}



	if (mid != KeepAliveMessage) {
		if (s && s->device) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: >> Got message %s\n", s->device->id, sccpmsg2str(mid));
		} else {
			sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: >> Got message %s\n", sccpmsg2str(mid));
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
	sccp_mwi_check(s->device);
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
	if (s->device)
		sccp_dev_set_registered(s->device, SKINNY_DEVICE_RS_OK);
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
  default:
	sccp_handle_unknown_message(s,r);
  }

  ast_free(r);
  return 1;
}



/**
 * \brief creates configured line from \link ast_variable asterisk variable \endlink
 * \return configured line
 * \note also used by realtime functionality to line device from \link ast_variable asterisk variable \endlink
 */
sccp_line_t * build_lines_wo(struct ast_variable *v, uint8_t realtime) {
	sccp_line_t * l = NULL;
	int amaflags = 0;
	int secondary_dialtone_tone = 0;
	char * tmp;

	l = sccp_line_create();
 	while(v) {
 			if((!strcasecmp(v->name, "line"))
#ifdef CS_SCCP_REALTIME
			|| (!strcasecmp(v->name, "name"))
#endif
 			) {
 				if (!ast_strlen_zero(v->value)) {
					tmp = strdup(v->value);
 					sccp_copy_string(l->name, ast_strip(tmp), sizeof(l->name));
 					ast_free(tmp);
					tmp = NULL;
 					/* search for existing line */
 					if (sccp_line_find_byname_wo(l->name, 0)) {
 						ast_log(LOG_WARNING, "SCCP: Line '%s' already exists\n", l->name);
 						ast_free(l);
 					} else {
#ifdef CS_SCCP_REALTIME
 						l->realtime = realtime;
#endif
 						SCCP_LIST_LOCK(&GLOB(lines));
 						SCCP_LIST_INSERT_HEAD(&GLOB(lines), l, list);
 						SCCP_LIST_UNLOCK(&GLOB(lines));
 						sccp_log(1)(VERBOSE_PREFIX_3 "chan_sccp.conf: Added line '%s'\n", l->name);
 						return l;
 					}
 				} else {
 					ast_log(LOG_WARNING, "Wrong line param: '%s => %s'\n", v->name, v->value);
 					ast_free(l);
 				}
				if(!realtime)
					l = sccp_line_create();
 			} else if (!strcasecmp(v->name, "id")) {
 				sccp_copy_string(l->id, v->value, sizeof(l->id));
 			} else if (!strcasecmp(v->name, "pin")) {
 				sccp_copy_string(l->pin, v->value, sizeof(l->pin));
 			} else if (!strcasecmp(v->name, "label")) {
 				sccp_copy_string(l->label, v->value, sizeof(l->label));
 			} else if (!strcasecmp(v->name, "description")) {
 				sccp_copy_string(l->description, v->value, sizeof(l->description));
 			} else if (!strcasecmp(v->name, "context")) {
 				sccp_copy_string(l->context, v->value, sizeof(l->context));
 			} else if (!strcasecmp(v->name, "cid_name")) {
 				sccp_copy_string(l->cid_name, v->value, sizeof(l->cid_name));
 			} else if (!strcasecmp(v->name, "cid_num")) {
 				sccp_copy_string(l->cid_num, v->value, sizeof(l->cid_num));
 			} else if (!strcasecmp(v->name, "callerid")) {
 				ast_log(LOG_WARNING, "obsolete callerid param. Use cid_num and cid_name\n");
 			} else if (!strcasecmp(v->name, "mailbox")) {
 				sccp_mailbox_t *mailbox = NULL;
 				char *context, *mbox = NULL;

 				mbox = context = ast_strdupa(v->value);
 				if (ast_strlen_zero(mbox))
 					continue;

 				if (!(mailbox = ast_calloc(1, sizeof(*mailbox))))
 					continue;


 				strsep(&context, "@");
 				mailbox->mailbox = ast_strdup(mbox);
 				mailbox->context = ast_strdup(context);

 				SCCP_LIST_INSERT_TAIL(&l->mailboxes, mailbox, list);
 				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Added mailbox '%s@%s'\n", l->name, mailbox->mailbox, (mailbox->context)?mailbox->context:"default");

 				//sccp_copy_string(l->mailbox, v->value, sizeof(l->mailbox));
 			} else if (!strcasecmp(v->name, "vmnum")) {
 				sccp_copy_string(l->vmnum, v->value, sizeof(l->vmnum));
 			} else if (!strcasecmp(v->name, "meetmenum")) {
 				sccp_copy_string(l->meetmenum, v->value, sizeof(l->meetmenum));
 			} else if (!strcasecmp(v->name, "transfer")) {
 				l->transfer = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "incominglimit")) {
 				l->incominglimit = atoi(v->value);
 				if (l->incominglimit < 1)
					l->incominglimit = 1;
 				/* this is the max call phone limits. Just a guess. It's not important */
 				if (l->incominglimit > 99)
					l->incominglimit = 99;
 			} else if (!strcasecmp(v->name, "echocancel")) {
 					l->echocancel = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "silencesuppression")) {
 				l->silencesuppression = sccp_true(v->value);
 			} else if (!strcasecmp(v->name, "rtptos")) {
 				if (sscanf(v->value, "%d", &l->rtptos) == 1)
 					l->rtptos &= 0xff;
 			} else if (!strcasecmp(v->name, "language")) {
 				sccp_copy_string(l->language, v->value, sizeof(l->language));
 			} else if (!strcasecmp(v->name, "musicclass")) {
 				sccp_copy_string(l->musicclass, v->value, sizeof(l->musicclass));
 			} else if (!strcasecmp(v->name, "accountcode")) {
 				sccp_copy_string(l->accountcode, v->value, sizeof(l->accountcode));
 			} else if (!strcasecmp(v->name, "amaflags")) {
 				amaflags = ast_cdr_amaflags2int(v->value);
 				if (amaflags < 0) {
 					ast_log(LOG_WARNING, "Invalid AMA Flags: %s at line %d\n", v->value, v->lineno);
 				} else {
 					l->amaflags = amaflags;
 				}
 			} else if (!strcasecmp(v->name, "callgroup")) {
 				l->callgroup = ast_get_group(v->value);
 	#ifdef CS_SCCP_PICKUP
 			} else if (!strcasecmp(v->name, "pickupgroup")) {
 				l->pickupgroup = ast_get_group(v->value);
 	#endif
 			} else if (!strcasecmp(v->name, "trnsfvm")) {
 				if (!ast_strlen_zero(v->value)) {
					l->trnsfvm = strdup(v->value);
 				}
 			} else if (!strcasecmp(v->name, "secondary_dialtone_digits")) {
 				if (strlen(v->value) > 9)
 					ast_log(LOG_WARNING, "secondary_dialtone_digits value '%s' is too long at line %d of SCCP.CONF. Max 9 digits\n", v->value, v->lineno);
 				sccp_copy_string(l->secondary_dialtone_digits, v->value, sizeof(l->secondary_dialtone_digits));
 			} else if (!strcasecmp(v->name, "secondary_dialtone_tone")) {
 				if (sscanf(v->value, "%i", &secondary_dialtone_tone) == 1) {
 					if (secondary_dialtone_tone >= 0 && secondary_dialtone_tone <= 255)
 						l->secondary_dialtone_tone = secondary_dialtone_tone;
 					else
 						l->secondary_dialtone_tone = SKINNY_TONE_OUTSIDEDIALTONE;
 				} else {
 					ast_log(LOG_WARNING, "Invalid secondary_dialtone_tone value '%s' at line %d of SCCP.CONF. Default is OutsideDialtone (0x22)\n", v->value, v->lineno);
 				}
 			} else if (!strcasecmp(v->name, "setvar")) {
				struct ast_variable *newvar = NULL;

				newvar = sccp_create_variable(v->value);
				if(newvar){
					sccp_log(10)(VERBOSE_PREFIX_3 "Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);
					newvar->next = l->variables;
					l->variables = newvar;
				}
 			} else if (!strcasecmp(v->name, "dnd")) {
				if (!strcasecmp(v->value, "reject")) {
					l->dndmode = SCCP_DNDMODE_REJECT;
				} else if (!strcasecmp(v->value, "silent")) {
					l->dndmode = SCCP_DNDMODE_SILENT;
				} else if (!strcasecmp(v->value, "user")) {
					l->dndmode = SCCP_DNDMODE_USERDEFINED;
				} else {
					/* 0 is off and 1 (on) is reject */
					l->dndmode = sccp_true(v->value);
				}
			} else {
 				ast_log(LOG_WARNING, "Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
 			}
 			v = v->next;
 		}

	return l;
}


/**
 * \brief Create device from ast_variable
 * \return configured device
 * \note also used by realtime functionality to build device from \link ast_variable asterisk variable \endlink
 */
sccp_device_t *build_devices_wo(struct ast_variable *v, uint8_t realtime) {
	sccp_device_t 	*d = NULL;
	uint8_t 		speeddial_index = 1;
	uint8_t			serviceURLIndex = 1;
	char 			message[256]="";							//device message
	int				res;

	/* for button config */
	char 			*buttonType = NULL, *buttonName = NULL, *buttonOption=NULL, *buttonArgs=NULL;
	char 			k_button[256];
//	sccp_buttonconfig_t	* lastButton = NULL, * currentButton = NULL;
	uint8_t			instance=0;
	char 			*splitter;

	d = sccp_device_create();
	if(!v)
		sccp_log(10)(VERBOSE_PREFIX_3 "no variable given\n");
	while (v) {
			sccp_log(10)(VERBOSE_PREFIX_3 "%s = %s\n", v->name, v->value);
			if ((!strcasecmp(v->name, "device"))
#ifdef CS_SCCP_REALTIME
			|| (!strcasecmp(v->name, "name"))
#endif
			) {
				if ((strlen(v->value) == 15) && ((strncmp(v->value, "SEP",3) == 0) || (strncmp(v->value, "ATA",3)==0)) ) {
					sccp_copy_string(d->id, v->value, sizeof(d->id));
					instance=0;
					res=ast_db_get("SCCPM", d->id, message, sizeof(message));				//load save message from ast_db
					if (!res)
						d->phonemessage=strdup(message);									//set message on device if we have a result
					strcpy(message,"");
#ifdef CS_SCCP_REALTIME
					d->realtime = realtime;
#endif
					SCCP_LIST_LOCK(&GLOB(devices));
					SCCP_LIST_INSERT_HEAD(&GLOB(devices), d, list);
					SCCP_LIST_UNLOCK(&GLOB(devices));
					if(!realtime) {
						sccp_log(1)(VERBOSE_PREFIX_3 "Added device '%s' (%s)\n", d->id, d->config_type);
					} else {
						sccp_log(1)(VERBOSE_PREFIX_3 "Added device '%s'\n", d->id);
					}
				} else {
					ast_log(LOG_WARNING, "Wrong device param: %s => %s\n", v->name, v->value);
					sccp_dev_clean(d, TRUE); /* clean and destroy*/
					instance=0;
				}
				if(!realtime) {
					d = sccp_device_create();
					speeddial_index = 1;
					serviceURLIndex = 1;
				}
			}
			else if (!strcasecmp(v->name, "keepalive")) {
				d->keepalive = atoi(v->value);
			} else if (!strcasecmp(v->name, "permit") || !strcasecmp(v->name, "deny")) {
#ifndef ASTERISK_CONF_1_6
				d->ha = ast_append_ha(v->name, v->value, d->ha);
#else
				d->ha = ast_append_ha(v->name, v->value, d->ha, NULL );
#endif
			} else if (!strcasecmp(v->name, "button")) {
				sccp_log(0)(VERBOSE_PREFIX_3 "Found buttonconfig: %s\n", v->value);
				sccp_copy_string(k_button, v->value, sizeof(k_button));
				splitter = k_button;
				buttonType = strsep(&splitter, ",");
				buttonName = strsep(&splitter, ",");
				buttonOption = strsep(&splitter, ",");
				buttonArgs = splitter;

				sccp_log(99)(VERBOSE_PREFIX_3 "ButtonType: %s\n", buttonType);
				sccp_log(99)(VERBOSE_PREFIX_3 "ButtonName: %s\n", buttonName);
				sccp_log(99)(VERBOSE_PREFIX_3 "ButtonOption: %s\n", buttonOption);
				if (!strcasecmp(buttonType, "line") && buttonName){
					if(!buttonName)
						continue;
					sccp_config_addLine(d, (buttonName)?ast_strip(buttonName):NULL, ++instance);
					if(buttonOption && !strcasecmp(buttonOption, "default")){
						d->defaultLineInstance = instance;
						sccp_log(99)(VERBOSE_PREFIX_3 "set defaultLineInstance to: %u\n", instance);
					}
				} else if (!strcasecmp(buttonType, "empty")){
					sccp_config_addEmpty(d, ++instance);
				} else if (!strcasecmp(buttonType, "speeddial")){
					sccp_config_addSpeeddial(d, (buttonName)?ast_strip(buttonName):"speeddial", (buttonOption)?ast_strip(buttonOption):NULL, (buttonArgs)?ast_strip(buttonArgs):NULL, ++instance);
				} else if (!strcasecmp(buttonType, "feature") && buttonName){
					sccp_config_addFeature(d, (buttonName)?ast_strip(buttonName):"feature", (buttonOption)?ast_strip(buttonOption):NULL, buttonArgs, ++instance );
				} else if (!strcasecmp(buttonType, "service") && buttonName && !ast_strlen_zero(buttonName) ){
					sccp_config_addService(d, (buttonName)?ast_strip(buttonName):"service", (buttonOption)?ast_strip(buttonOption):NULL, ++instance);
				}
			} else if (!strcasecmp(v->name, "permithost")) {
				sccp_permithost_addnew(d, v->value);
			} else if (!strcasecmp(v->name, "type")) {
				sccp_copy_string(d->config_type, v->value, sizeof(d->config_type));
			} else if (!strcasecmp(v->name, "addon")) {
				sccp_addon_addnew(d, v->value);
			} else if (!strcasecmp(v->name, "tzoffset")) {
				/* timezone offset */
				d->tz_offset = atoi(v->value);
			} else if (!strcasecmp(v->name, "autologin")) {
				char *mb, *cur, tmp[256];

				sccp_copy_string(tmp, v->value, sizeof(tmp));
				mb = tmp;
				while((cur = strsep(&mb, ","))) {
					ast_strip(cur);
					if (ast_strlen_zero(cur)) {
						sccp_config_addEmpty(d, ++instance);
						continue;
					}

					sccp_log(10)(VERBOSE_PREFIX_3 "%s: Auto logging into %s\n", d->id, cur);
					sccp_config_addLine(d, cur, ++instance);
				}

				//sccp_copy_string(d->autologin, v->value, sizeof(d->autologin));
			} else if (!strcasecmp(v->name, "description")) {
				sccp_copy_string(d->description, v->value, sizeof(d->description));
			} else if (!strcasecmp(v->name, "imageversion")) {
				sccp_copy_string(d->imageversion, v->value, sizeof(d->imageversion));
			} else if (!strcasecmp(v->name, "allow")) {
				ast_parse_allow_disallow(&d->codecs, &d->capability, v->value, 1);
			} else if (!strcasecmp(v->name, "disallow")) {
				ast_parse_allow_disallow(&d->codecs, &d->capability, v->value, 0);
			} else if (!strcasecmp(v->name, "transfer")) {
				d->transfer = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "cfwdall")) {
				d->cfwdall = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "cfwdbusy")) {
				d->cfwdbusy = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "cfwdnoanswer")) {
				d->cfwdnoanswer = sccp_true(v->value);
#ifdef CS_SCCP_PICKUP
			} else if (!strcasecmp(v->name, "pickupexten")) {
				d->pickupexten = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "pickupcontext")) {
				if(!ast_strlen_zero(v->value))
					d->pickupcontext = strdup(v->value);
			} else if (!strcasecmp(v->name, "pickupmodeanswer")) {
				d->pickupmodeanswer = sccp_true(v->value);
#endif
			} else if (!strcasecmp(v->name, "dnd")) {
				if (!strcasecmp(v->value, "reject")) {
					d->dndmode = SCCP_DNDMODE_REJECT;
				} else if (!strcasecmp(v->value, "silent")) {
					d->dndmode = SCCP_DNDMODE_SILENT;
				} else if (!strcasecmp(v->value, "user")) {
					d->dndmode = SCCP_DNDMODE_USERDEFINED;
				} else {
					/* 0 is off and 1 (on) is reject */
					d->dndmode = sccp_true(v->value);
				}
			} else if (!strcasecmp(v->name, "nat")) {
				d->nat = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "directrtp")) {
				d->directrtp = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "allowoverlap")) {
				d->overlapFeature.enabled = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "trustphoneip")) {
				d->trustphoneip = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "private")) {
				d->privacyFeature.enabled = sccp_true(v->value);
			} else if (!strcasecmp(v->name, "privacy")) {
				if (!strcasecmp(v->value, "full")) {
					d->privacyFeature.status = 0;
					d->privacyFeature.status = ~d->privacyFeature.status;
					//d->privacyFeature.status = 0xFFFFFFFF;
				} else {
					d->privacyFeature.status = 0;
					//d->privacyFeature.enabled = sccp_true(v->value);
				}
			} else if (!strcasecmp(v->name, "earlyrtp")) {
				if (!strcasecmp(v->value, "none"))
					d->earlyrtp = 0;
				else if (!strcasecmp(v->value, "offhook"))
					d->earlyrtp = SCCP_CHANNELSTATE_OFFHOOK;
				else if (!strcasecmp(v->value, "dial"))
					d->earlyrtp = SCCP_CHANNELSTATE_DIALING;
				else if (!strcasecmp(v->value, "ringout"))
					d->earlyrtp = SCCP_CHANNELSTATE_RINGOUT;
				else
					ast_log(LOG_WARNING, "Invalid earlyrtp state value at line %d, should be 'none', 'offhook', 'dial', 'ringout'\n", v->lineno);
			} else if (!strcasecmp(v->name, "dtmfmode")) {
				if (!strcasecmp(v->value, "outofband"))
					d->dtmfmode = SCCP_DTMFMODE_OUTOFBAND;
		#ifdef CS_SCCP_PARK
			} else if (!strcasecmp(v->name, "park")) {
				d->park = sccp_true(v->value);
		#endif
			} else if (!strcasecmp(v->name, "speeddial")) {
				if(ast_strlen_zero(v->value)){
					sccp_config_addEmpty(d, ++instance);
				}else{
//					//sccp_speeddial_addnew(d, v->value, speeddial_index);
					sccp_copy_string(k_button, v->value, sizeof(k_button));
					splitter = k_button;
					buttonType = strsep(&splitter, ",");
					buttonName = strsep(&splitter, ",");
					if(splitter)
						buttonOption = strsep(&splitter, ",");
					else
						buttonOption = NULL;
//
					sccp_config_addSpeeddial(d, ast_strip(buttonName), ast_strip(buttonType), (buttonOption)?ast_strip(buttonOption):NULL, ++instance);
				}

			} else if (!strcasecmp(v->name, "mwilamp")) {
				if (!strcasecmp(v->value, "off"))
					d->mwilamp = SKINNY_LAMP_OFF;
				else if (!strcasecmp(v->value, "on"))
					d->mwilamp = SKINNY_LAMP_ON;
				else if (!strcasecmp(v->value, "wink"))
					d->mwilamp = SKINNY_LAMP_WINK;
				else if (!strcasecmp(v->value, "flash"))
					d->mwilamp = SKINNY_LAMP_FLASH;
				else if (!strcasecmp(v->value, "blink"))
					d->mwilamp = SKINNY_LAMP_BLINK;
				else
					ast_log(LOG_WARNING, "Invalid mwilamp value at line %d, should be 'off', 'on', 'wink', 'flash' or 'blink'\n", v->lineno);
			} else if (!strcasecmp(v->name, "mwioncall")) {
					d->mwioncall = sccp_true(v->value);
			}else if (!strcasecmp(v->name, "setvar")) {
				struct ast_variable *newvar = NULL;
				newvar = sccp_create_variable(v->value);
				if(newvar){
					sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Add new channelvariable to line %s. Value is: %s \n",newvar->name ,newvar->value);
					newvar->next = d->variables;
					d->variables = newvar;
				}
 			} else if (!strcasecmp(v->name, "serviceURL")) {
 				//sccp_serviceURL_addnew(d, v->value, serviceURLIndex);
 				ast_log(LOG_WARNING, "SCCP: Service: %s\n", v->value);
 				if(ast_strlen_zero(v->value))
					sccp_config_addEmpty(d, ++instance);
				else{
					sccp_copy_string(k_button, v->value, sizeof(k_button));
					splitter = k_button;
					buttonType = strsep(&splitter, ",");
					buttonName = strsep(&splitter, ",");


					sccp_config_addService(d, ast_strip(buttonType), ast_strip(buttonName), ++instance);
				}
			} else {
				ast_log(LOG_WARNING, "SCCP: Unknown param at line %d: %s = %s\n", v->lineno, v->name, v->value);
			}
			v = v->next;
		}

	sccp_dev_dbget(d); /* load saved settings from ast db */
	return d;
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
	sccp_config_readdevices(SCCP_CONFIG_READINITIAL);
	sccp_config_readLines(SCCP_CONFIG_READINITIAL);

	/* ok the config parse is done */
	if ((GLOB(descriptor) > -1) && (ntohs(GLOB(bindaddr.sin_port)) != oldport)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

	if (GLOB(descriptor) < 0)
	{
		//GLOB(descriptor) = socket(AF_UNSPEC, SOCK_STREAM, 0);
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

  sccp_dev_dbclean();
  sccp_mwi_stopMonitor();
  sccp_restart_monitor();
  return 0;
}


void *sccp_create_hotline(void){
      sccp_line_t	*hotline;

      hotline = sccp_line_create();
      sccp_copy_string(hotline->name, "hotline", sizeof(hotline->name));
      sccp_copy_string(hotline->cid_name, "hotline", sizeof(hotline->cid_name));
      sccp_copy_string(hotline->cid_num, "***", sizeof(hotline->cid_name));
      sccp_copy_string(hotline->context, "default", sizeof(hotline->context));
      sccp_copy_string(hotline->label, "hotline", sizeof(hotline->label));
      //sccp_copy_string(hotline->mailbox, "hotline", sizeof(hotline->mailbox));

      SCCP_LIST_LOCK(&GLOB(lines));
      SCCP_LIST_INSERT_HEAD(&GLOB(lines), hotline, list);
      SCCP_LIST_UNLOCK(&GLOB(lines));

      GLOB(hotline)->line = hotline;
      sccp_copy_string(GLOB(hotline)->exten, "111", sizeof(GLOB(hotline)->exten));

      return NULL;
}

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

static char *sccp_setcalledparty_descrip = "  SetCalledParty(\"Name\" <ext>) sets the name and number of the called party for use with chan_sccp\n";
/**
 * \brief Set the name an number of the called party to the calling phone
 * \param chan asterisk channel
 * \param data callerid in format "Name" <number>
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
/**
 * \brief It allows you to send a message to the calling device.
 * \author Frank Segtrop <fs@matflow.net>
 * \param chan
 * \param data message to sent - if empty clear display
 * \version 20071112_1944
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
enum ast_bridge_result sccp_rtp_bridge(struct ast_channel *c0, struct ast_channel *c1, int flags, struct ast_frame **fo, struct ast_channel **rc, int timeoutms)
{
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

	ast_log(LOG_ERROR, "register event listeners\n");
	sccp_subscribe_event(SCCP_EVENT_LINECREATED, sccp_mwi_linecreatedEvent);
	sccp_subscribe_event(SCCP_EVENT_DEVICEATTACHED, sccp_mwi_deviceAttachedEvent);

	sccp_hint_module_start();


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
	GLOB(global_capability) = AST_FORMAT_ALAW|AST_FORMAT_ULAW|AST_FORMAT_G729A;

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
	GLOB(private) = 1; /* permit private function */
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
	ast_rtp_proto_register(&sccp_rtp);
#endif
#ifdef CS_SCCP_MANAGER
	sccp_register_management();
#endif

	sccp_register_cli();
	ast_register_application("SetCalledParty", sccp_setcalledparty_exec, "Sets the name of the called party", sccp_setcalledparty_descrip);
	ast_register_application("SetMessage", sccp_setmessage_exec, "Sets a message on the calling device", sccp_setmessage_descrip);

	/* And start the monitor for the first time */
	sccp_restart_monitor();
	sccp_mwi_startMonitor();

	return 0;
}






#ifdef ASTERISK_CONF_1_2
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
	sccp_mwi_stopMonitor();

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
	//SCCP_LIST_LOCK(&GLOB(devices));
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&GLOB(devices), d, list){
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		sccp_dev_clean(d, TRUE);
	}
	SCCP_LIST_TRAVERSE_SAFE_END;

//	while ((d = SCCP_LIST_REMOVE_HEAD(&GLOB(devices), list))) {
//		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
//		sccp_dev_clean(d, TRUE);
//	}
//	SCCP_LIST_UNLOCK(&GLOB(devices));
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


		if (l->cfwd_num)
			ast_free(l->cfwd_num);
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
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Skinny Client Control Protocol (SCCP). Release: " SCCP_BRANCH " - " SCCP_VERSION " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
#else
int usecount() {
	int res;
	sccp_globals_lock(usecnt_lock);
	res = GLOB(usecnt);
	sccp_globals_unlock(usecnt_lock);
	return res;
}

char *key() {
	return ASTERISK_GPL_KEY;
}

char *description() {
	return ("Skinny Client Control Protocol (SCCP). Release: " SCCP_BRANCH " - " SCCP_VERSION " (built by '" BUILD_USER "' on '" BUILD_DATE "')");
}
#endif
