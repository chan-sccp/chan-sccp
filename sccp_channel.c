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

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/musiconhold.h>
#ifdef CS_SCCP_PARK
#include <asterisk/features.h>
#endif

static uint32_t callCount = 1;
AST_MUTEX_DEFINE_STATIC(callCountLock);

sccp_channel_t * sccp_channel_allocate(sccp_line_t * l) {
/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t * c;
	sccp_device_t * d;

  /* If there is no current line, then we can't make a call in, or out. */
	if (!l) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on a device with no lines\n");
	return NULL;
	}

	if (!l->device) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on NULL device\n");
		return NULL;
	}
	
	d = l->device;

	if (!d->session) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", d->id);
		return NULL;
	}

	c = malloc(sizeof(sccp_channel_t));
	if (!c) {
		/* error allocating memory */
		ast_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n",d->id, l->name);
		return NULL;
	}
	memset(c, 0, sizeof(sccp_channel_t));
	ast_mutex_init(&c->lock);

	/* default ringermode SKINNY_STATION_OUTSIDERING. Change it with SCCPRingerMode app */
	c->ringermode = SKINNY_STATION_OUTSIDERING;
	/* inbound for now. It will be changed later on outgoing calls */
	c->calltype = SKINNY_CALLTYPE_INBOUND;
	/* the dialing thread will set the hangupok = 0 */
	c->hangupok = 1;

	ast_mutex_lock(&callCountLock);
	c->callid = callCount++;
	ast_mutex_unlock(&callCountLock);

	c->line = l;
	c->device = d;

	ast_mutex_lock(&l->lock);
	l->channelCount++;
	ast_mutex_unlock(&l->lock);

	ast_mutex_lock(&d->lock);
	d->channelCount++;
	ast_mutex_unlock(&d->lock);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", d->id, c->callid, l->name);

	/* put it on the top of the lists */

	ast_mutex_lock(&GLOB(channels_lock));
	c->next = GLOB(channels);
	if (GLOB(channels)) /* first one */
		GLOB(channels)->prev = c;
	GLOB(channels) = c;
	ast_mutex_unlock(&GLOB(channels_lock));

	c->next_on_line = l->channels;
	if (l->channels) /* first one */
		l->channels->prev_on_line = c;
	l->channels = c;

	return c;
}

sccp_channel_t * sccp_channel_get_active(sccp_device_t * d) {
	sccp_channel_t * c;
/* uint8_t tries = SCCP_LOCK_TRIES; */

	if (!d)
		return NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Getting the active channel on device\n",d->id);
	
	ast_mutex_lock(&GLOB(channels_lock));
	ast_mutex_lock(&d->lock);
	c = d->active_channel;
/*
	while (tries && ast_mutex_trylock(&c->lock)) {
		tries--;
		usleep(SCCP_LOCK_USLEEP);
	}

	if (!tries) {
		ast_log(LOG_ERROR, "%s: Failed to lock channel (%d)\n", DEV_ID_LOG(d), c->callid);
		c = NULL;
	}
*/
	ast_mutex_unlock(&d->lock);
	ast_mutex_unlock(&GLOB(channels_lock));
	return c;
}

void sccp_channel_set_active(sccp_channel_t * c) {
	sccp_device_t * d = c->device;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (c) ? c->callid : 0);
	ast_mutex_lock(&d->lock);
	d->active_channel = c;
	d->currentLine = c->line;
	ast_mutex_unlock(&d->lock);
}

void sccp_channel_send_callinfo(sccp_channel_t * c) {
	sccp_moo_t * r;

	REQ(r, CallInfoMessage);

	if (c->callingPartyName)
		sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
	if (c->callingPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

	if (c->calledPartyName)
		sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
	if (c->calledPartyNumber)
		sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

	r->msg.CallInfoMessage.lel_lineId   = htolel(c->line->instance);
	r->msg.CallInfoMessage.lel_callRef  = htolel(c->callid);
	r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);
	r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	
	if (c->line->device){
		sccp_dev_send(c->line->device, r);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send callinfo for %s channel %d\n", c->line->device->id, skinny_calltype2str(c->calltype), c->callid);
	}else
		return;
}

void sccp_channel_send_dialednumber(sccp_channel_t * c) {
	sccp_moo_t * r;

	if (ast_strlen_zero(c->calledPartyNumber))
		return;

	REQ(r, DialedNumberMessage);

	sccp_copy_string(r->msg.DialedNumberMessage.calledParty, c->calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));

	r->msg.DialedNumberMessage.lel_lineId   = htolel(c->line->instance);
	r->msg.DialedNumberMessage.lel_callRef  = htolel(c->callid);
	sccp_dev_send(c->line->device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", c->line->device->id, c->calledPartyNumber, skinny_calltype2str(c->calltype), c->callid);
}

void sccp_channel_set_callstate(sccp_channel_t * c, uint8_t state) {
	c->callstate = state;
	sccp_channel_set_callstate_full(c->device, c->line->instance, c->callid, state);
}

/* temporary function for hints */
void sccp_channel_set_callstate_full(sccp_device_t * d, uint8_t instance, uint32_t callid, uint8_t state) {
	sccp_moo_t * r;

	if (!d)
		return;
	REQ(r, CallStateMessage);
	r->msg.CallStateMessage.lel_callState = htolel(state);
	r->msg.CallStateMessage.lel_lineInstance	= htolel(instance);
	r->msg.CallStateMessage.lel_callReference = htolel(callid);
/*    r->msg.CallStateMessage.lel_unknown1 = htolel(0); */
        r->msg.CallStateMessage.lel_priority = htolel(SKINNY_CALLPRIORITY_NORMAL);
/*    r->msg.CallStateMessage.lel_unknown3 = htolel(2); */
	sccp_dev_send(d, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send and Set the call state %s(%d) on call %d\n", d->id, sccp_callstate2str(state), state, callid);
}

void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number) {
	sccp_device_t * d;

	if (!c || !c->device)
		return;

	d = c->device;

	if (name && strncmp(name, c->callingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->callingPartyName, name, sizeof(c->callingPartyName));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set callingParty Name %s on channel %d\n", d->id, c->callingPartyName, c->callid);
	}

	if (number && strncmp(number, c->callingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->callingPartyNumber, number, sizeof(c->callingPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set callingParty Number %s on channel %d\n", d->id, c->callingPartyNumber, c->callid);
	}

	return;
}

void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number) {
	sccp_device_t * d;

	if (!c || !c->device)
		return;

	d = c->device;

	if (name && strncmp(name, c->calledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->calledPartyName, name, sizeof(c->calledPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set calledParty Name %s on channel %d\n", d->id, c->calledPartyName, c->callid);
	}

	if (number && strncmp(number, c->calledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->calledPartyNumber, number, sizeof(c->callingPartyNumber));
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Set calledParty Number %s on channel %d\n", d->id, c->calledPartyNumber, c->callid);
	}
}

void sccp_channel_StatisticsRequest(sccp_channel_t * c) {
	sccp_moo_t * r;

	if (!c)
		return;

	REQ(r, ConnectionStatisticsReq);

	/* XXX need to test what we have to copy in the DirectoryNumber */
	if (c->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->calledPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));
	else
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->callingPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));

	r->msg.ConnectionStatisticsReq.lel_callReference = htolel((c) ? c->callid : 0);
	r->msg.ConnectionStatisticsReq.lel_StatsProcessing = htolel(SKINNY_STATSPROCESSING_CLEAR);
	sccp_dev_send(c->line->device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Requesting CallStatisticsAndClear from Phone\n", c->line->device->id);
}

void sccp_channel_openreceivechannel(sccp_channel_t * c) {
	sccp_moo_t * r;
	int payloadType = sccp_codec_ast2skinny(c->format);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: readformat %d, payload %d\n", c->line->device->id, c->owner->readformat, payloadType);

	REQ(r, OpenReceiveChannel);
	r->msg.OpenReceiveChannel.lel_conferenceId = htolel(c->callid);
	r->msg.OpenReceiveChannel.lel_passThruPartyId = htolel(c->callid) ;
	r->msg.OpenReceiveChannel.lel_millisecondPacketSize = htolel(20);
	/* if something goes wrong the default codec is ulaw */
	r->msg.OpenReceiveChannel.lel_payloadType = htolel((payloadType) ? payloadType : 4);
	r->msg.OpenReceiveChannel.lel_vadValue = htolel(c->line->echocancel);
	r->msg.OpenReceiveChannel.lel_conferenceId1 = htolel(c->callid);
	sccp_dev_send(c->line->device, r);
	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", c->line->device->id, c->callid, skinny_codec2str(payloadType), c->line->echocancel ? "ON" : "OFF");
	if (!c->rtp) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Starting RTP on channel %s-%d\n", DEV_ID_LOG(c->device), c->line->name, c->callid);
		sccp_channel_start_rtp(c);
	}
	if (!c->rtp) {
		ast_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%d\n", DEV_ID_LOG(c->device), c->line->name, c->callid);
		sccp_dev_starttone(c->line->device, SKINNY_TONE_REORDERTONE, c->line->instance, c->callid, 0);
	}
}

void sccp_channel_startmediatransmission(sccp_channel_t * c) {
	sccp_moo_t * r;
	struct sockaddr_in sin;
	struct ast_hostent ahp;
	struct hostent *hp;
	int payloadType = sccp_codec_ast2skinny(c->format);
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif

	if (!c->rtp) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%d\n", c->device->id, c->line->name, c->callid);
		return;
	}

	ast_rtp_get_us(c->rtp, &sin);
	ast_rtp_settos(c->rtp, c->line->rtptos);

	REQ(r, StartMediaTransmission);
	r->msg.StartMediaTransmission.lel_conferenceId = htolel(c->callid);
	r->msg.StartMediaTransmission.lel_passThruPartyId = htolel(c->callid);

	if (c->device->nat) {
		if (GLOB(externip.sin_addr.s_addr)) {
			if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
				time(&GLOB(externexpire));
				GLOB(externexpire) += GLOB(externrefresh);
				if ((hp = ast_gethostbyname(GLOB(externhost), &ahp))) {
					memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
				} else
					ast_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
			}
			memcpy(&sin.sin_addr, &GLOB(externip.sin_addr), 4);
		}
	}
	memcpy(&r->msg.StartMediaTransmission.bel_remoteIpAddr, &sin.sin_addr, 4);
	r->msg.StartMediaTransmission.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
	r->msg.StartMediaTransmission.lel_millisecondPacketSize = htolel(20);
	r->msg.StartMediaTransmission.lel_payloadType = htolel((payloadType) ? payloadType : 4);
	r->msg.StartMediaTransmission.lel_precedenceValue = htolel(c->line->rtptos);
	r->msg.StartMediaTransmission.lel_ssValue = htolel(c->line->silencesuppression); // Silence supression
	r->msg.StartMediaTransmission.lel_maxFramesPerPacket = 0;
	r->msg.StartMediaTransmission.lel_conferenceId1 = htolel(c->callid);
	sccp_dev_send(c->line->device, r);
#ifdef ASTERISK_CONF_1_2
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to %s:%d with codec: %s, tos %d, silencesuppression: %s\n",c->line->device->id, ast_inet_ntoa(iabuf, sizeof(iabuf), sin.sin_addr), ntohs(sin.sin_port), skinny_codec2str(payloadType), c->line->rtptos, c->line->silencesuppression ? "ON" : "OFF");
#else
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to %s:%d with codec: %s, tos %d, silencesuppression: %s\n",c->line->device->id, ast_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), skinny_codec2str(payloadType), c->line->rtptos, c->line->silencesuppression ? "ON" : "OFF");
#endif
}


void sccp_channel_closereceivechannel(sccp_channel_t * c) {
	sccp_moo_t * r;
	REQ(r, CloseReceiveChannel);
	r->msg.CloseReceiveChannel.lel_conferenceId = htolel(c ? c->callid : 0);
	r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(c ? c->callid : 0);
	r->msg.OpenReceiveChannel.lel_conferenceId1 = htolel(c ? c->callid : 0);
	sccp_dev_send(c->line->device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Close openreceivechannel on channel %d\n",c->line->device->id, c->callid);
	sccp_channel_stopmediatransmission(c);
}

void sccp_channel_stopmediatransmission(sccp_channel_t * c) {
	sccp_moo_t * r;
	if (c->rtp) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Stopping phone media transmission on channel %s-%d\n", c->device->id, c->line->name, c->callid);
		sccp_channel_stop_rtp(c);
	}
	REQ(r, StopMediaTransmission);
	r->msg.CloseReceiveChannel.lel_conferenceId = htolel(c ? c->callid : 0);
	r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(c ? c->callid : 0);
	sccp_dev_send(c->line->device, r);
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Stop media transmission on channel %d\n",c->line->device->id, c->callid);
	/* requesting statistics */
	sccp_channel_StatisticsRequest(c);
}

void sccp_channel_endcall(sccp_channel_t * c) {
	sccp_device_t * d;
	struct ast_channel * ast;
	uint8_t res = 0;

	if (!c || !c->device)
		return;

	ast_mutex_lock(&c->lock);

	/* this is a station active endcall or onhook */
	d = c->device;
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Ending call %d on line %s\n", DEV_ID_LOG(d), c->callid, c->line->name);
	
	if (c->rtp) {
		sccp_channel_closereceivechannel(c);
		sccp_channel_stop_rtp(c);
	}

	if ( c->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_ONHOOK);

	ast = c->owner;
	
	if (!ast) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(d), c->callid, c->line->name);
		ast_mutex_unlock(&c->lock);
		return;
	}

	/* what type of hangup? */
//	res = (c->calltype == SKINNY_CALLTYPE_INBOUND || ast->pbx || ast->blocker || CS_AST_BRIDGED_CHANNEL(ast));
	/* is there a blocker? */
	res = (ast->pbx ||ast->blocker);
	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Sending %s hangup request to %s\n", DEV_ID_LOG(d), res ? "(queue)" : "(force)", ast->name);

	ast_mutex_unlock(&c->lock);

	if (res) {
		ast_queue_hangup(ast);
	} else {
		ast_hangup(ast);
	}

	return;
}

sccp_channel_t * sccp_channel_newcall(sccp_line_t * l, char * dial) {
	/* handle outgoing calls */
	sccp_channel_t * c;
	sccp_device_t * d;
	pthread_attr_t attr;
	pthread_t t;

	if (!l || !l->device) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return NULL;
	}

	d = l->device;

	/* look if we have a call to put on hold */
	if ( (c = sccp_channel_get_active(d)) ) {
		/* there is an active call, let's put it on hold first */
		if (!sccp_channel_hold(c))
			return NULL;
	}

	c = sccp_channel_allocate(l);

	if (!c) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return NULL;
	}
	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_OFFHOOK);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c)) {
		ast_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp ==  SCCP_CHANNELSTATE_OFFHOOK && !c->rtp) {
		sccp_channel_openreceivechannel(c);
	}

	/* copy the number to dial in the ast->exten */
	if (dial)
		sccp_copy_string(c->dialedNumber, dial, sizeof(c->dialedNumber));

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  /* let's call it */
	if (ast_pthread_create(&t, &attr, sccp_pbx_startchannel, c->owner)) {
		ast_log(LOG_WARNING, "%s: Unable to create switch thread for channel (%s-%d) %s\n", d->id, l->name, c->callid, strerror(errno));
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONGESTION);
	}
	return c;
}

void sccp_channel_answer(sccp_channel_t * c) {
	sccp_line_t * l;
	sccp_device_t * d;
	sccp_channel_t * hold;
	struct ast_channel * bridged;

	if (!c || !c->line || !c->line->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", (c ? c->callid : 0) );
		return;
	}
	l = c->line;
	d = c->line->device;
	/* answering an incoming call */
	/* look if we have a call to put on hold */
	if ( (hold = sccp_channel_get_active(d)) ) {
		/* there is an active call, let's put it on hold first */
		if (!sccp_channel_hold(hold))
			return;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Answer the channel %s-%d\n", d->id, l->name, c->callid);
	sccp_channel_set_active(c);
	sccp_dev_set_activeline(c->line);
	if (c->owner) {
		/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
		bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
		if (bridged && ast_test_flag(bridged, AST_FLAG_MOH)) {
			ast_moh_stop(bridged);
#ifndef CS_AST_HAS_FLAG_MOH
			ast_clear_flag(bridged, AST_FLAG_MOH);
#endif
		}
	}
	/* @Marcello: Here you assume that it is not neccessary to tell the phone
	   something it already knows ;-) But I am not sure if this would be needed
	   nevertheless to log all incoming answered calls properly. We will have to
	   investigate this further. (-DD) */
	if (c->state != SCCP_CHANNELSTATE_OFFHOOK)
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_OFFHOOK);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONNECTED);
	ast_queue_control(c->owner, AST_CONTROL_ANSWER);
}

int sccp_channel_hold(sccp_channel_t * c) {
	sccp_line_t * l;
	sccp_device_t * d;
	struct ast_channel * peer;

	if (!c)
		return 0;
		
	if (!c->line || !c->line->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->line->device;

	if (c->state == SCCP_CHANNELSTATE_HOLD)
		return 0;
	/* put on hold an active call */
	if (c->state != SCCP_CHANNELSTATE_CONNECTED) {
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't put on hold an inactive channel %s-%d\n", d->id, l->name, c->callid);
		/* hard button phones need it */
		sccp_dev_displayprompt(d, l->instance, c->callid, "No active call to put on hold.",5);
		return 0;
	}

	peer = CS_AST_BRIDGED_CHANNEL(c->owner);

	if (peer) {
#ifdef ASTERISK_CONF_1_2
		ast_moh_start(peer, NULL);
#else
		ast_moh_start(peer, NULL, l->musicclass);
#endif
#ifndef CS_AST_HAS_FLAG_MOH
		ast_set_flag(peer, AST_FLAG_MOH);
#endif
	}
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Hold the channel %s-%d\n", d->id, l->name, c->callid);
	ast_mutex_lock(&d->lock);
	d->active_channel = NULL;
	ast_mutex_unlock(&d->lock);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_HOLD);

#ifdef CS_AST_CONTROL_HOLD
	 ast_mutex_lock(&d->lock);
	 if (!c->owner) {
		 sccp_log(1)(VERBOSE_PREFIX_3 "C owner disappeared! Can't free ressources\n");
		 ast_mutex_unlock(&d->lock);
		 return 0;
	 }
	ast_queue_control(c->owner, AST_CONTROL_HOLD);
	ast_mutex_unlock(&d->lock);
#endif
	return 1;
}

int sccp_channel_resume(sccp_channel_t * c) {
	sccp_line_t * l;
	sccp_device_t * d;
	sccp_channel_t * hold;
	struct ast_channel * peer;

	if (!c || !c->owner)
		return 0;

	if (!c->line || !c->line->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->line->device;

	/* look if we have a call to put on hold */
	if ( (hold = sccp_channel_get_active(d)) ) {
		/* there is an active call, let's put it on hold first */
		if (!sccp_channel_hold(hold))
			return 0;
	}

	/* resume an active call */
	if (c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER) {
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't resume the channel %s-%d. Not on hold\n", d->id, l->name, c->callid);
		sccp_dev_displayprompt(d, l->instance, c->callid, "No active call to put on hold",5);
		return 0;
	}

	/* check if we are in the middle of a transfer */
	ast_mutex_lock(&d->lock);
	if (d->transfer_channel == c) {
		d->transfer_channel = NULL;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%d\n", d->id, l->name, c->callid);
	}
	ast_mutex_unlock(&d->lock);

	peer = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (peer) {
		ast_moh_stop(peer);
            /* this is for STABLE version */
#ifndef CS_AST_HAS_FLAG_MOH
		ast_clear_flag(peer, AST_FLAG_MOH);
#endif
	}

	sccp_channel_set_active(c);
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_CONNECTED);
#ifdef CS_AST_CONTROL_HOLD
	ast_queue_control(c->owner, AST_CONTROL_UNHOLD);
#endif
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Resume the channel %s-%d\n", d->id, l->name, c->callid);
	return 1;
}

void sccp_channel_delete(sccp_channel_t * c) {
	ast_mutex_lock(&GLOB(channels_lock));
	sccp_channel_delete_no_lock(c);
	ast_mutex_unlock(&GLOB(channels_lock));
}

/* this does no lock the main channels pointer */
void sccp_channel_delete_no_lock(sccp_channel_t * c) {
	sccp_line_t * l;
	sccp_device_t * d;
	struct sccp_selected_channel *cur = NULL, *par = NULL;

	if (!c)
		return;

	ast_mutex_lock(&c->lock);

	l = c->line;
	d = c->device;

	if (d->transfer_channel == c)
		d->transfer_channel = NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Deleting channel %d from line %s\n", DEV_ID_LOG(d), c->callid, l ? l->name : "(null)");

	/* mark the channel DOWN so any pending thread will terminate */
	if (c->owner) {
		ast_setstate(c->owner, AST_STATE_DOWN);
		c->owner = NULL;
	}

	/* this is in case we are destroying the session */
	if (c->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate_nolock(c, SCCP_CHANNELSTATE_ONHOOK);

	if (c->rtp)
		sccp_channel_stop_rtp(c);

	/* remove from the global channels list */

	if (c->next) { /* not the last one */
		ast_mutex_lock(&c->next->lock);
		c->next->prev = c->prev;
		ast_mutex_unlock(&c->next->lock);
	}
	if (c->prev) { /* not the first one */
		ast_mutex_lock(&c->prev->lock);
		c->prev->next = c->next;
		ast_mutex_unlock(&c->prev->lock);
	}
	else { /* the first one */
		GLOB(channels) = c->next;
	}

	/* remove the channel from the channels line list */
	if (c->next_on_line) { /* not the last one */
		ast_mutex_lock(&c->next_on_line->lock);
		c->next_on_line->prev_on_line = c->prev_on_line;
		ast_mutex_unlock(&c->next_on_line->lock);
	}
	if (c->prev_on_line) { /* not the first one */
		ast_mutex_lock(&c->prev_on_line->lock);
		c->prev_on_line->next_on_line = c->next_on_line;
		ast_mutex_unlock(&c->prev_on_line->lock);
	}
	else { /* the first one */
		ast_mutex_lock(&l->lock);
		l->channels = c->next_on_line;
		ast_mutex_unlock(&l->lock);
	}

	ast_mutex_lock(&l->lock);
/*	if (l->activeChannel == c)
		l->activeChannel = NULL; */
	l->channelCount--;
	ast_mutex_unlock(&l->lock);

	/* deactive the active call if needed */
	ast_mutex_lock(&d->lock);
	d->channelCount--;
	if (d->active_channel == c)
		d->active_channel = NULL;	
	ast_mutex_unlock(&d->lock);
	
	//remove selected channels
	ast_mutex_lock(&d->lock);
	cur = d->selectedChannels;
	while(NULL != cur) {
	    if(c == cur->c) {
	    	if(NULL == par)
	    		d->selectedChannels = cur->next;
	    	else 
	    		par->next = cur->next;
	    	free(cur);
	    } 
	    par = cur;
	    cur = cur->next;
	}
	ast_mutex_unlock(&d->lock);
	//remove selected channels
	
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Deleted channel %d from line %s\n", DEV_ID_LOG(d), c->callid, l ? l->name : "(null)");
	ast_mutex_unlock(&c->lock);
	ast_mutex_destroy(&c->lock);
	free(c);
	return;
}

void sccp_channel_start_rtp(sccp_channel_t * c) {
	sccp_session_t * s;
#ifdef ASTERISK_CONF_1_2
	char iabuf[INET_ADDRSTRLEN];
#endif
	
	if (!c || !c->device)
		return;
	s = c->device->session;
	if (!s)
		return;

/* No need to lock, because already locked in the sccp_indicate.c */
/*	ast_mutex_lock(&c->lock); */
#ifdef ASTERISK_CONF_1_2
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", c->device->id, ast_inet_ntoa(iabuf, sizeof(iabuf), s->ourip));
#else
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Creating rtp server connection at %s\n", c->device->id, ast_inet_ntoa(s->ourip));
#endif
	c->rtp = ast_rtp_new_with_bindaddr(NULL, NULL, 1, 0, s->ourip);
	if (c->device->nat)
		ast_rtp_setnat(c->rtp, 1);

	if (c->rtp && c->owner)
		c->owner->fds[0] = ast_rtp_fd(c->rtp);

/*	ast_mutex_unlock(&c->lock); */
}

void sccp_channel_stop_rtp(sccp_channel_t * c) {
	if (c->rtp) {
		if (c->owner)
			c->owner->fds[0] = -1;
		ast_rtp_destroy(c->rtp);
		c->rtp = NULL;
	}
}

void sccp_channel_transfer(sccp_channel_t * c) {
	sccp_device_t * d;
	sccp_channel_t * newcall = NULL;

	if (!c)
		return;

	if (!c->line || !c->line->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return;
	}

	d = c->device;

	if (!d->transfer || !c->line->transfer) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Tranfer disabled on device or line\n", d->id);
		return;
	}

	ast_mutex_lock(&d->lock);
	/* are we in the middle of a transfer? */
	if (d->transfer_channel && (d->transfer_channel != c)) {
		ast_mutex_unlock(&d->lock);
		sccp_channel_transfer_complete(c);
		return;
	}

	d->transfer_channel = c;
	ast_mutex_unlock(&d->lock);
	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%d\n", d->id, c->line->name, c->callid);

	if (!c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No bridged channel to transfer on %s-%d\n", d->id, c->line->name, c->callid);
		sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}
	if ( (c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER) && !sccp_channel_hold(c) )
		return;
	if (c->state != SCCP_CHANNELSTATE_CALLTRANSFER)
		sccp_indicate_lock(c, SCCP_CHANNELSTATE_CALLTRANSFER);
	newcall = sccp_channel_newcall(c->line, NULL);
	/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER*/
	if (newcall && newcall->owner && c->owner && CS_AST_BRIDGED_CHANNEL(c->owner))
		pbx_builtin_setvar_helper(newcall->owner, "_BLINDTRANSFER", CS_AST_BRIDGED_CHANNEL(c->owner)->name);
}

static void * sccp_channel_transfer_ringing_thread(void *data) {
	char * name = data;
	struct ast_channel * ast;

	if (!name)
		return NULL;

	sleep(1);
	ast = ast_get_channel_by_name_locked(name);
	free(name);

	if (!ast)
		return NULL;

	sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: Send ringing indication to %s(%p)\n", ast->name, ast);
	if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING)
		ast_indicate(ast, AST_CONTROL_RINGING);
	else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH)
#ifdef ASTERISK_CONF_1_2
		ast_moh_start(ast, NULL);
#else
		ast_moh_start(ast, NULL, NULL);
#endif
	ast_mutex_unlock(&ast->lock);
	return NULL;
}

/**
 * bridge two channels
 * 
 * \todo find a way solve the chan->state problem
 */
void sccp_channel_transfer_complete(sccp_channel_t * cDestinationLocal) {
#ifndef CS_AST_CHANNEL_HAS_CID
	char *name, *number, *cidtmp;
#endif
	struct ast_channel	*astcSourceRemote = NULL, *astcDestinationLocal = NULL, *astcDestinationRemote = NULL;
	sccp_channel_t * cSourceLocal;
	sccp_device_t * d = NULL;
	pthread_attr_t attr;
	pthread_t t;

	if (!cDestinationLocal)
		return;

	if (!cDestinationLocal->line || !cDestinationLocal->line->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", cDestinationLocal->callid);
		return;
	}
	
	// Obtain the device from which the transfer was initiated
	d = cDestinationLocal->line->device;
	ast_mutex_lock(&d->lock);
	// Obtain the source channel on that device
	cSourceLocal = d->transfer_channel;
	ast_mutex_unlock(&d->lock);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%d\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid);

	

	if (cDestinationLocal->state != SCCP_CHANNELSTATE_RINGOUT && cDestinationLocal->state != SCCP_CHANNELSTATE_CONNECTED) {
		ast_log(LOG_WARNING, "Failed to complete transfer. The channel is not ringing or connected\n");
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, cDestinationLocal->line->instance, cDestinationLocal->callid, 0);
		sccp_dev_displayprompt(d, cDestinationLocal->line->instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (!cDestinationLocal->owner || !cSourceLocal->owner) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%d and %s-%d\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid, cSourceLocal->line->name, cSourceLocal->callid);
		return;
	}

	astcSourceRemote = CS_AST_BRIDGED_CHANNEL(cSourceLocal->owner);
	astcDestinationRemote = CS_AST_BRIDGED_CHANNEL(cDestinationLocal->owner);
	astcDestinationLocal = cDestinationLocal->owner;

	//FIXME - segmentation fault on log
//	sccp_log(1)(VERBOSE_PREFIX_3 "%s: transferred: %s(%p)\npeer->owner: %s(%p)\ndestination: %s(%p)\nc->owner:%s(%p)\n", d->id,
//	transferred->name, transferred,
//	peer->owner->name, peer->owner,
//	destination->name, destination,
//	c->owner->name, c->owner);

	if (!astcSourceRemote || !astcDestinationLocal) {
		ast_log(LOG_WARNING, "Failed to complete transfer. Missing asterisk transferred or transferee channel\n");
		sccp_dev_displayprompt(d, cDestinationLocal->line->instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
		ast_indicate(astcSourceRemote, AST_CONTROL_RINGING); // Shouldn't this be ALERTING?
		/* starting the ringing thread */
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (ast_pthread_create(&t, &attr, sccp_channel_transfer_ringing_thread, strdup(astcSourceRemote->name))) {
			ast_log(LOG_WARNING, "%s: Unable to create thread for the blind transfer ring indication. %s\n", d->id, strerror(errno));
		}
	}

	if (astcDestinationLocal->cdr && astcSourceRemote->cdr) {
		astcDestinationLocal->cdr = ast_cdr_append(astcDestinationLocal->cdr, astcSourceRemote->cdr);
	} else if (astcSourceRemote->cdr) {
		astcDestinationLocal->cdr = astcSourceRemote->cdr;
	}
	astcSourceRemote->cdr = NULL;

	if (ast_channel_masquerade(astcDestinationLocal, astcSourceRemote)) {
		ast_log(LOG_WARNING, "Failed to masquerade %s into %s\n", astcDestinationLocal->name, astcSourceRemote->name);
		sccp_dev_displayprompt(d, cDestinationLocal->line->instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
	}

	
	ast_mutex_lock(&d->lock);
	if (cSourceLocal->owner){
		ast_mutex_unlock(&d->lock);
		ast_queue_hangup(cSourceLocal->owner);
	}else {
		sccp_log(1)(VERBOSE_PREFIX_3 "Peer owner disappeared! Can't free ressources\n");
		ast_mutex_unlock(&d->lock);
		return;
	}
	ast_mutex_unlock(&astcDestinationLocal->lock); // Where was the transferee locked at first?!
	//ast_mutex_lock(&d->lock);

	ast_mutex_lock(&d->lock);
	d->transfer_channel = NULL;
	ast_mutex_unlock(&d->lock);
  
  

	if (!astcDestinationRemote) {
		/* the channel was ringing not answered yet. BLIND TRANSFER */
  return;
}

#ifndef CS_AST_HAS_TECH_PVT
	if (strncasecmp(astcDestinationRemote->type,"SCCP",4)) {
#else
	if (strncasecmp(astcDestinationRemote->tech->type,"SCCP",4)) {
#endif
		/* nothing to do with different channel types */
		return;
	}

	/* it's a SCCP channel destination on transfer */
	cDestinationLocal = CS_AST_CHANNEL_PVT(astcDestinationRemote);

	if (cDestinationLocal) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Transfer confirmation destination on channel %s\n", d->id, astcDestinationRemote->name);
		/* display the transferred CID info to destination */
#ifdef CS_AST_CHANNEL_HAS_CID
		sccp_channel_set_callingparty(cDestinationLocal, astcSourceRemote->cid.cid_name, astcSourceRemote->cid.cid_num);
#else
		if (astcSourceRemote->callerid && (cidtmp = strdup(astcSourceRemote->callerid))) {
			ast_callerid_parse(cidtmp, &name, &number);
			sccp_channel_set_callingparty(cDestinationLocal, name, number);
			free(cidtmp);
		}
#endif
		sccp_channel_send_callinfo(cDestinationLocal);
		if (GLOB(transfer_tone) && cDestinationLocal->state == SCCP_CHANNELSTATE_CONNECTED)
			/* while connected not all the tones can be played */
			sccp_dev_starttone(cDestinationLocal->device, GLOB(autoanswer_tone), cDestinationLocal->line->instance, cDestinationLocal->callid, 0);
	}
}

#ifdef CS_SCCP_PARK

struct sccp_dual {
	struct ast_channel *chan1;
	struct ast_channel *chan2;
};

static void * sccp_channel_park_thread(void *stuff) {
	struct ast_channel *chan1, *chan2;
	struct sccp_dual *dual;
	struct ast_frame *f;
	int ext;
	int res;
	char extstr[20];
	sccp_channel_t * c;
	memset(&extstr, 0 , sizeof(extstr));

	dual = stuff;
	chan1 = dual->chan1;
	chan2 = dual->chan2;
	free(dual);
	f = ast_read(chan1);
	if (f)
		ast_frfree(f);
	res = ast_park_call(chan1, chan2, 0, &ext);
	if (!res) {
		extstr[0] = 128;
		extstr[1] = SKINNY_LBL_CALL_PARK_AT;
		sprintf(&extstr[2]," %d",ext);
		c = CS_AST_CHANNEL_PVT(chan2);
//		sccp_dev_displayprompt(c->device, c->line->instance, c->callid, extstr, 0);
		sccp_dev_displaynotify(c->device, extstr, 10);
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Parked channel %s on %d\n", DEV_ID_LOG(c->device), chan1->name, ext);
	}
	ast_hangup(chan2);
	return NULL;
}

void sccp_channel_park(sccp_channel_t * c) {
	sccp_device_t * d;
	sccp_line_t      * l;
	struct sccp_dual *dual;
	struct ast_channel *chan1m, *chan2m, *bridged;
	pthread_t th;

	if (!c)
		return;

	d = c->device;
	l = c->line;
	if (!d)
		return;

	if (!d->park) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park disabled on device\n", d->id);
		return;
	}

	if (!c->owner) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk channel\n", d->id);
		return;
	}
	bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (!bridged) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk bridged channel\n", d->id);
		return;
	}
	sccp_indicate_lock(c, SCCP_CHANNELSTATE_CALLPARK);

#ifdef ASTERISK_CONF_1_2
	chan1m = ast_channel_alloc(0);
#else
	chan1m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, "SCCP/%s", l->name, NULL, 0, NULL);
#endif
       // chan1m = ast_channel_alloc(0); function changed in 1.4.0
       // Assuming AST_STATE_DOWN is suitable.. need to check
	if (!chan1m) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);
		sccp_dev_displayprompt(c->device, c->line->instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		return;
	}
#ifdef ASTERISK_CONF_1_2
	chan2m = ast_channel_alloc(0);
#else
	chan2m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, "SCCP/%s", l->name,  NULL, 0, NULL);
#endif
       // chan2m = ast_channel_alloc(0); function changed in 1.4.0
       // Assuming AST_STATE_DOWN is suitable.. need to check
	if (!chan1m) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);
		sccp_dev_displayprompt(c->device, c->line->instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		ast_hangup(chan1m);
		return;
	}

#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan1m, name, "Parking/%s", bridged->name);
#else
	snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", bridged->name);
#endif
	//snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", bridged->name);/
	/* Make formats okay */
	chan1m->readformat = bridged->readformat;
	chan1m->writeformat = bridged->writeformat;
	ast_channel_masquerade(chan1m, bridged);
	/* Setup the extensions and such */
	sccp_copy_string(chan1m->context, bridged->context, sizeof(chan1m->context));
	sccp_copy_string(chan1m->exten, bridged->exten, sizeof(chan1m->exten));
	chan1m->priority = bridged->priority;

	/* We make a clone of the peer channel too, so we can play
	   back the announcement */
#ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan2m, name, "SCCPParking/%s", c->owner->name);
#else
	snprintf(chan2m->name, sizeof(chan2m->name), "SCCPParking/%s", c->owner->name);
#endif
	//snprintf(chan2m->name, sizeof (chan2m->name), "SCCPParking/%s",c->owner->name);

	/* Make formats okay */
	chan2m->readformat = c->owner->readformat;
	chan2m->writeformat = c->owner->writeformat;
	ast_channel_masquerade(chan2m, c->owner);
	/* Setup the extensions and such */
	sccp_copy_string(chan2m->context, c->owner->context, sizeof(chan2m->context));
	sccp_copy_string(chan2m->exten, c->owner->exten, sizeof(chan2m->exten));
	chan2m->priority = c->owner->priority;
	if (ast_do_masquerade(chan2m)) {
		ast_log(LOG_WARNING, "SCCP: Masquerade failed :(\n");
		ast_hangup(chan2m);
		return;
	}

	dual = malloc(sizeof(struct sccp_dual));
	if (dual) {
		memset(d, 0, sizeof(*dual));
		dual->chan1 = chan1m;
		dual->chan2 = chan2m;
		if (!ast_pthread_create(&th, NULL, sccp_channel_park_thread, dual))
			return;
		free(dual);
	}
}
#endif

