
/*!
 * \file 	sccp_channel.c
 * \brief 	SCCP Channel Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks	Purpose: 	SCCP Channels
 * 		When to use:	Only methods directly related to sccp channels should be stored in this source file.
 *   		Relationships: 	SCCP Channels connect Asterisk Channels to SCCP Lines
 */

#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
//#include <asterisk/pbx.h>
//#include <asterisk/utils.h>
//#include <asterisk/causes.h>
//#include <asterisk/callerid.h>
//#include <asterisk/musiconhold.h>
//#ifndef CS_AST_HAS_TECH_PVT
//#    include <asterisk/channel_pvt.h>
//#endif
//#ifdef CS_SCCP_PARK
//#    include <asterisk/features.h>
//#endif
//#ifdef CS_MANAGER_EVENTS
//#    include <asterisk/manager.h>
//#endif
static uint32_t callCount = 1;

AST_MUTEX_DEFINE_STATIC(callCountLock);

/*!
 * \brief Allocate SCCP Channel on Device
 * \param l SCCP Line
 * \param device SCCP Device
 * \return a *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- callCountLock
 * 	- channel
 */
sccp_channel_t *sccp_channel_allocate_locked(sccp_line_t * l, sccp_device_t * device)
{
	/* this just allocate a sccp channel (not the asterisk channel, for that look at sccp_pbx_channel_allocate) */
	sccp_channel_t *c;

	/* If there is no current line, then we can't make a call in, or out. */
	if (!l) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on a device with no lines\n");
		return NULL;
	}

	if (device && !device->session) {
		ast_log(LOG_ERROR, "SCCP: Tried to open channel on device %s without a session\n", device->id);
		return NULL;
	}

	c = ast_malloc(sizeof(sccp_channel_t));
	if (!c) {
		/* error allocating memory */
		ast_log(LOG_ERROR, "%s: No memory to allocate channel on line %s\n", l->id, l->name);
		return NULL;
	}
	memset(c, 0, sizeof(sccp_channel_t));

	sccp_mutex_init(&c->lock);
	sccp_channel_lock(c);

	/* this is for dialing scheduler */
	c->digittimeout = -1;
	/* maybe usefull -FS */
	c->owner = NULL;
	/* default ringermode SKINNY_STATION_OUTSIDERING. Change it with SCCPRingerMode app */
	c->ringermode = SKINNY_STATION_OUTSIDERING;
	/* inbound for now. It will be changed later on outgoing calls */
	c->calltype = SKINNY_CALLTYPE_INBOUND;
	c->answered_elsewhere = FALSE;

	/* callcount limit should be reset at his upper limit :) */
	if (callCount == 0xFFFFFFFF)
		callCount = 1;

	sccp_mutex_lock(&callCountLock);
	c->callid = callCount++;
	c->passthrupartyid = c->callid ^ 0xFFFFFFFF;
	sccp_mutex_unlock(&callCountLock);

	c->line = l;
	c->peerIsSCCP = 0;
	c->isCodecFix = FALSE;
	c->device = device;
	sccp_channel_updateChannelCapability_locked(c);

	sccp_line_addChannel(l, c);

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: New channel number: %d on line %s\n", l->id, c->callid, l->name);

	return c;
}

/*!
 * \brief Update Channel Capability
 * \param channel a *locked* SCCP Channel
 */
void sccp_channel_updateChannelCapability_locked(sccp_channel_t * channel)
{
	if (!channel)
		return;


	if (!channel->device) {
		channel->capability = AST_FORMAT_ALAW | AST_FORMAT_ULAW | AST_FORMAT_G729A;
		memcpy(&channel->codecs, &GLOB(global_codecs), sizeof(channel->codecs));
	} else {
		channel->capability = channel->device->capability;
//              if(sccp_device_isVideoSupported(channel->device)){
//                      channel->capability |= channel->device->capability;
//              }
		memcpy(&channel->codecs, &channel->device->codecs, sizeof(channel->codecs));

		/* asterisk requested format, we can not handle with this device */
		if (!(channel->format & channel->capability)) {
			channel->format = ast_codec_choose(&channel->codecs, channel->capability, 1);
		}
	}

	if (channel->isCodecFix == FALSE) {
		/* we does not have set a preferred format before */
		channel->format = ast_codec_choose(&channel->codecs, channel->capability, 1);
	}

	/* We assume that the owner has been already locked. */
	if (channel->owner) {
		channel->owner->nativeformats = channel->format;		/* if we set nativeformats to a single format, we force asterisk to translate stream */

		channel->owner->rawreadformat = channel->format;
		channel->owner->rawwriteformat = channel->format;

		channel->owner->writeformat = channel->format;			/*|AST_FORMAT_H263|AST_FORMAT_H264|AST_FORMAT_H263_PLUS; */
		channel->owner->readformat = channel->format;			/*|AST_FORMAT_H263|AST_FORMAT_H264|AST_FORMAT_H263_PLUS; */

//              if( (channel->capability & AST_FORMAT_VIDEO_MASK) ){
//                      channel->owner->writeformat |= (channel->capability & AST_FORMAT_VIDEO_MASK);
//                      channel->owner->readformat |= (channel->capability & AST_FORMAT_VIDEO_MASK);
//              }

		ast_set_read_format(channel->owner, channel->format);
		ast_set_write_format(channel->owner, channel->format);
	}
#if 0										// (DD)
	char s1[512], s2[512];
	sccp_log(2) (VERBOSE_PREFIX_3 "SCCP: SCCP/%s-%08x, capabilities: %s(%d) USED: %s(%d) \n", channel->line->name, channel->callid, pbx_getformatname_multiple(s1, sizeof(s1) - 1, channel->capability), channel->capability, pbx_getformatname_multiple(s2, sizeof(s2) - 1, channel->format), channel->format);
#endif
}

/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP Channel
 */
sccp_channel_t *sccp_channel_get_active_nolock(sccp_device_t * d)
{
	sccp_channel_t *c;

	if (!d)
		return NULL;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Getting the active channel on device.\n", d->id);

	c = d->active_channel;

	if (!c || c->state == SCCP_CHANNELSTATE_DOWN)
		return NULL;

	return c;
}

/*!
 * \brief Get Active Channel
 * \param d SCCP Device
 * \return SCCP *locked* Channel
 */
sccp_channel_t *sccp_channel_get_active_locked(sccp_device_t * d)
{
	sccp_channel_t *c = sccp_channel_get_active_nolock(d);

	if (c)
		sccp_channel_lock(c);

	return c;
}

/*!
 * \brief Set SCCP Channel to Active
 * \param d SCCP Device
 * \param c SCCP Channel
 * 
 * \lock
 * 	- device
 */
void sccp_channel_set_active(sccp_device_t * d, sccp_channel_t * c)
{
	if (!d)
		return;

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Set the active channel %d on device\n", DEV_ID_LOG(d), (c) ? c->callid : 0);
	sccp_device_lock(d);
	d->active_channel = c;
	d->currentLine = c->line;
	sccp_device_unlock(d);
}

/*!
 * \brief Send Call Information to Device/Channel
 * \param device SCCP Device
 * \param c SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_find_index_for_line()
 */
static void sccp_channel_send_staticCallinfo(sccp_device_t * device, sccp_channel_t * c)
{
	sccp_moo_t *r;

	uint8_t instance = 0;

	if (!device || !c)
		return;

	sccp_device_lock(device);
	instance = sccp_device_find_index_for_line(device, c->line->name);
	sccp_device_unlock(device);

	REQ(r, CallInfoMessage);

	if (c->device == device) {
		if (c->callInfo.callingPartyName) {
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		}
		if (c->callInfo.callingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callInfo.callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

		if (c->callInfo.calledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		if (c->callInfo.calledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->callInfo.calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

		if (c->callInfo.originalCalledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.originalCalledPartyName, c->callInfo.originalCalledPartyName, sizeof(r->msg.CallInfoMessage.originalCalledPartyName));
		if (c->callInfo.originalCalledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.originalCalledParty, c->callInfo.originalCalledPartyNumber, sizeof(r->msg.CallInfoMessage.originalCalledParty));

		if (c->callInfo.lastRedirectingPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingPartyName, c->callInfo.lastRedirectingPartyName, sizeof(r->msg.CallInfoMessage.lastRedirectingPartyName));
		if (c->callInfo.lastRedirectingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingParty, c->callInfo.lastRedirectingPartyNumber, sizeof(r->msg.CallInfoMessage.lastRedirectingParty));

		if (c->callInfo.originalCdpnRedirectReason)
			r->msg.CallInfoMessage.originalCdpnRedirectReason = htolel(c->callInfo.originalCdpnRedirectReason);
		if (c->callInfo.lastRedirectingReason)
			r->msg.CallInfoMessage.lastRedirectingReason = htolel(c->callInfo.lastRedirectingReason);

		if (c->callInfo.cgpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.cgpnVoiceMailbox, c->callInfo.cgpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cgpnVoiceMailbox));
		if (c->callInfo.cdpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.cdpnVoiceMailbox, c->callInfo.cdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cdpnVoiceMailbox));

		if (c->callInfo.originalCdpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.originalCdpnVoiceMailbox, c->callInfo.originalCdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.originalCdpnVoiceMailbox));
		if (c->callInfo.lastRedirectingVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox, c->callInfo.lastRedirectingVoiceMailbox, sizeof(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox));

		r->msg.CallInfoMessage.lel_lineId = htolel(instance);
		r->msg.CallInfoMessage.lel_callRef = htolel(c->callid);
		r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);
		r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	} else {
		/* remote device notification */
		if (c->callInfo.callingPartyName) {
			sccp_copy_string(r->msg.CallInfoMessage.callingPartyName, c->callInfo.callingPartyName, sizeof(r->msg.CallInfoMessage.callingPartyName));
		}
		if (c->callInfo.callingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.callingParty, c->callInfo.callingPartyNumber, sizeof(r->msg.CallInfoMessage.callingParty));

		if (c->callInfo.calledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.calledPartyName, c->callInfo.calledPartyName, sizeof(r->msg.CallInfoMessage.calledPartyName));
		if (c->callInfo.calledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.calledParty, c->callInfo.calledPartyNumber, sizeof(r->msg.CallInfoMessage.calledParty));

		if (c->callInfo.originalCalledPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.originalCalledPartyName, c->callInfo.originalCalledPartyName, sizeof(r->msg.CallInfoMessage.originalCalledPartyName));
		if (c->callInfo.originalCalledPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.originalCalledParty, c->callInfo.originalCalledPartyNumber, sizeof(r->msg.CallInfoMessage.originalCalledParty));

		if (c->callInfo.lastRedirectingPartyName)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingPartyName, c->callInfo.lastRedirectingPartyName, sizeof(r->msg.CallInfoMessage.lastRedirectingPartyName));
		if (c->callInfo.lastRedirectingPartyNumber)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingParty, c->callInfo.lastRedirectingPartyNumber, sizeof(r->msg.CallInfoMessage.lastRedirectingParty));

		if (c->callInfo.originalCdpnRedirectReason)
			r->msg.CallInfoMessage.originalCdpnRedirectReason = htolel(c->callInfo.originalCdpnRedirectReason);
		if (c->callInfo.lastRedirectingReason)
			r->msg.CallInfoMessage.lastRedirectingReason = htolel(c->callInfo.lastRedirectingReason);

		if (c->callInfo.cgpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.cgpnVoiceMailbox, c->callInfo.cgpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cgpnVoiceMailbox));
		if (c->callInfo.cdpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.cdpnVoiceMailbox, c->callInfo.cdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.cdpnVoiceMailbox));

		if (c->callInfo.originalCdpnVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.originalCdpnVoiceMailbox, c->callInfo.originalCdpnVoiceMailbox, sizeof(r->msg.CallInfoMessage.originalCdpnVoiceMailbox));
		if (c->callInfo.lastRedirectingVoiceMailbox)
			sccp_copy_string(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox, c->callInfo.lastRedirectingVoiceMailbox, sizeof(r->msg.CallInfoMessage.lastRedirectingVoiceMailbox));

		r->msg.CallInfoMessage.lel_lineId = htolel(instance);
		r->msg.CallInfoMessage.lel_callRef = htolel(c->callid);
		r->msg.CallInfoMessage.lel_callType = htolel(c->calltype);
		r->msg.CallInfoMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_UNKNOWN);
	}

	sccp_dev_send(device, r);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: Send callinfo for %s channel %d on line instance %d" "\n\tcallerid: %s" "\n\tcallerName: %s\n", (device) ? device->id : "(null)", calltype2str(c->calltype), c->callid, instance, c->callInfo.callingPartyNumber, c->callInfo.callingPartyName);

}

/*!
 * \brief Send Call Information to Device/Channel via CallInfoDynamicMessage
 * \param device SCCP Device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_find_index_for_line()
 */
static void sccp_channel_send_dynamicCallinfo(sccp_device_t * device, sccp_channel_t * channel)
{
	sccp_moo_t *r;

	int dataSize = 16;

	int usableFields = dataSize;						/* acording to the protocol version, we can not us all fieldes */

	char *data[dataSize];

	int data_len[dataSize];

	int i = 0;

	int dummy_len = 0;

	uint8_t instance = 0;							/* line instance on device */

	if (device->inuseprotocolversion < 7) {
		/* fallback to CallInfoMessage */
		sccp_channel_send_staticCallinfo(device, channel);
		return;
	}

	sccp_device_lock(device);
	instance = sccp_device_find_index_for_line(device, channel->line->name);
	sccp_device_unlock(device);
	memset(data, 0, sizeof(data));

	if (device->inuseprotocolversion < 16) {
		usableFields = 12;

		data[0] = (strlen(channel->callInfo.callingPartyNumber) > 0) ? channel->callInfo.callingPartyNumber : NULL;
		data[1] = (strlen(channel->callInfo.calledPartyNumber) > 0) ? channel->callInfo.calledPartyNumber : NULL;
		data[2] = (strlen(channel->callInfo.originalCalledPartyNumber) > 0) ? channel->callInfo.originalCalledPartyNumber : NULL;
		data[3] = (strlen(channel->callInfo.lastRedirectingPartyNumber) > 0) ? channel->callInfo.lastRedirectingPartyNumber : NULL;
		data[4] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
		data[5] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;
		data[6] = (strlen(channel->callInfo.originalCdpnVoiceMailbox) > 0) ? channel->callInfo.originalCdpnVoiceMailbox : NULL;
		data[7] = (strlen(channel->callInfo.lastRedirectingVoiceMailbox) > 0) ? channel->callInfo.lastRedirectingVoiceMailbox : NULL;
		data[8] = (strlen(channel->callInfo.callingPartyName) > 0) ? channel->callInfo.callingPartyName : NULL;
		data[9] = (strlen(channel->callInfo.calledPartyName) > 0) ? channel->callInfo.calledPartyName : NULL;
		data[10] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
		data[11] = (strlen(channel->callInfo.lastRedirectingPartyName) > 0) ? channel->callInfo.lastRedirectingPartyName : NULL;
	} else {
		usableFields = 16;
		data[0] = (strlen(channel->callInfo.callingPartyNumber) > 0) ? channel->callInfo.callingPartyNumber : NULL;
		data[1] = (strlen(channel->callInfo.originalCallingPartyNumber) > 0) ? channel->callInfo.originalCallingPartyNumber : NULL;
		data[2] = (strlen(channel->callInfo.calledPartyNumber) > 0) ? channel->callInfo.calledPartyNumber : NULL;
		data[3] = (strlen(channel->callInfo.originalCalledPartyNumber) > 0) ? channel->callInfo.originalCalledPartyNumber : NULL;
		data[4] = (strlen(channel->callInfo.lastRedirectingPartyNumber) > 0) ? channel->callInfo.lastRedirectingPartyNumber : NULL;
		data[5] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
		data[6] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;
		data[7] = (strlen(channel->callInfo.originalCdpnVoiceMailbox) > 0) ? channel->callInfo.originalCdpnVoiceMailbox : NULL;
		data[8] = (strlen(channel->callInfo.lastRedirectingVoiceMailbox) > 0) ? channel->callInfo.lastRedirectingVoiceMailbox : NULL;
		data[9] = (strlen(channel->callInfo.callingPartyName) > 0) ? channel->callInfo.callingPartyName : NULL;
		data[10] = (strlen(channel->callInfo.calledPartyName) > 0) ? channel->callInfo.calledPartyName : NULL;
		data[11] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
		data[12] = (strlen(channel->callInfo.lastRedirectingPartyName) > 0) ? channel->callInfo.lastRedirectingPartyName : NULL;
		data[13] = (strlen(channel->callInfo.originalCalledPartyName) > 0) ? channel->callInfo.originalCalledPartyName : NULL;
		data[14] = (strlen(channel->callInfo.cgpnVoiceMailbox) > 0) ? channel->callInfo.cgpnVoiceMailbox : NULL;
		data[15] = (strlen(channel->callInfo.cdpnVoiceMailbox) > 0) ? channel->callInfo.cdpnVoiceMailbox : NULL;
	}

	for (i = 0; i < usableFields; i++) {
		if (data[i] != NULL) {
			data_len[i] = strlen(data[i]);
			dummy_len += data_len[i];
		} else {
			data_len[i] = 1;
		}
	}

	int hdr_len = sizeof(r->msg.CallInfoDynamicMessage) + (usableFields - 4);

	int padding = ((dummy_len + hdr_len) % 4);

	padding = (padding > 0) ? 4 - padding : 0;

	r = sccp_build_packet(CallInfoDynamicMessage, hdr_len + dummy_len + padding);

	r->msg.CallInfoDynamicMessage.lel_lineId = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_callRef = htolel(channel->callid);
	r->msg.CallInfoDynamicMessage.lel_callType = htolel(channel->calltype);
	r->msg.CallInfoDynamicMessage.partyPIRestrictionBits = 0;
	r->msg.CallInfoDynamicMessage.lel_callSecurityStatus = htolel(SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED);
	r->msg.CallInfoDynamicMessage.lel_callInstance = htolel(instance);
	r->msg.CallInfoDynamicMessage.lel_originalCdpnRedirectReason = htolel(channel->callInfo.originalCdpnRedirectReason);
	r->msg.CallInfoDynamicMessage.lel_lastRedirectingReason = htolel(channel->callInfo.lastRedirectingReason);

	if (dummy_len) {
		int bufferSize = dummy_len + usableFields;

		char buffer[bufferSize];

		memset(&buffer[0], 0, bufferSize);
		int pos = 0;

		for (i = 0; i < usableFields; i++) {
			sccp_log(DEBUGCAT_PBX) (VERBOSE_PREFIX_3 "SCCP: cid field %d, value: '%s'\n", i, (data[i]) ? data[i] : "");
			if (data[i]) {
				memcpy(&buffer[pos], data[i], data_len[i]);
				pos += data_len[i] + 1;
			} else {
				pos += 1;
			}
		}
		memcpy(&r->msg.CallInfoDynamicMessage.dummy, &buffer[0], bufferSize);
	}
//      sccp_dump_packet((unsigned char *)&r->msg, hdr_len + dummy_len + padding);
	sccp_dev_send(device, r);
}

/*!
 * \brief Send Call Information to Device/Channel
 *
 * Wrapper function that calls sccp_channel_send_staticCallinfo or sccp_channel_send_dynamicCallinfo
 *
 * \param device SCCP Device
 * \param channel SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_send_callinfo(sccp_device_t * device, sccp_channel_t * channel)
{

	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_3 "%s: send callInfo of callid %d\n", DEV_ID_LOG(device), (channel) ? channel->callid : 0);

	if (device->inuseprotocolversion < 7) {
		/* fallback to CallInfoMessage */
		return sccp_channel_send_staticCallinfo(device, channel);
	} else {
		return sccp_channel_send_dynamicCallinfo(device, channel);
	}
}

/*!
 * \brief Send Dialed Number to SCCP Channel device
 * \param c SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_send_dialednumber(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *device;

	uint8_t instance;

	if (sccp_strlen_zero(c->callInfo.calledPartyNumber))
		return;

	if (!c->device)
		return;
	device = c->device;

	REQ(r, DialedNumberMessage);

	instance = sccp_device_find_index_for_line(device, c->line->name);
	sccp_copy_string(r->msg.DialedNumberMessage.calledParty, c->callInfo.calledPartyNumber, sizeof(r->msg.DialedNumberMessage.calledParty));

	r->msg.DialedNumberMessage.lel_lineId = htolel(instance);
	r->msg.DialedNumberMessage.lel_callRef = htolel(c->callid);
	sccp_dev_send(device, r);
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Send the dialed number %s for %s channel %d\n", device->id, c->callInfo.calledPartyNumber, calltype2str(c->calltype), c->callid);
}

/*!
 * \brief Set Call State for SCCP Channel c, and Send this State to SCCP Device d.
 * \param c SCCP Channel
 * \param state channel state
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_setSkinnyCallstate(sccp_channel_t * c, skinny_callstate_t state)
{
	c->previousChannelState = c->state;
	c->state = state;

	return;
}

/*!
 * \brief Set CallingParty on SCCP Channel c
 * \param c SCCP Channel
 * \param name Name as char
 * \param number Number as char
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number)
{
	if (!c)
		return;

	if (name && strncmp(name, c->callInfo.callingPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->callInfo.callingPartyName, name, sizeof(c->callInfo.callingPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Name %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.callingPartyName, c->callid);
	}

	if (number && strncmp(number, c->callInfo.callingPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->callInfo.callingPartyNumber, number, sizeof(c->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set callingParty Number %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.callingPartyNumber, c->callid);
	}
	return;
}

/*!
 * \brief Set CalledParty on SCCP Channel c
 * \param c SCCP Channel
 * \param name Called Party Name
 * \param number Called Party Number
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number)
{
	if (!c)
		return;

	if (name && strncmp(name, c->callInfo.calledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->callInfo.calledPartyName, name, sizeof(c->callInfo.calledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set calledParty Name %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.calledPartyName, c->callid);
	}

	if (number && strncmp(number, c->callInfo.calledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->callInfo.calledPartyNumber, number, sizeof(c->callInfo.callingPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set calledParty Number %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.calledPartyNumber, c->callid);
	}
}

/*!
 * \brief Set Original CalledParty on SCCP Channel c (Used during Forward)
 * \param c SCCP Channel
 * \param name Name as char
 * \param number Number as char
 *
 * \callgraph
 * \callergraph
 */
void sccp_channel_set_originalCalledparty(sccp_channel_t * c, char *name, char *number)
{
	if (!c)
		return;

	if (name && strncmp(name, c->callInfo.originalCalledPartyName, StationMaxNameSize - 1)) {
		sccp_copy_string(c->callInfo.originalCalledPartyName, name, sizeof(c->callInfo.originalCalledPartyName));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Name %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.originalCalledPartyName, c->callid);
	}

	if (number && strncmp(number, c->callInfo.originalCalledPartyNumber, StationMaxDirnumSize - 1)) {
		sccp_copy_string(c->callInfo.originalCalledPartyNumber, number, sizeof(c->callInfo.originalCalledPartyNumber));
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "%s: Set originalCalledParty Number %s on channel %d\n", DEV_ID_LOG(c->device), c->callInfo.originalCalledPartyNumber, c->callid);
	}
	return;
}

/*!
 * \brief Request Call Statistics for SCCP Channel
 * \param c SCCP Channel
 */
void sccp_channel_StatisticsRequest(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *d;

	if (!c || !(d = c->device))
		return;

	REQ(r, ConnectionStatisticsReq);

	/* \todo We need to test what we have to copy in the DirectoryNumber */
	if (c->calltype == SKINNY_CALLTYPE_OUTBOUND)
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->callInfo.calledPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));
	else
		sccp_copy_string(r->msg.ConnectionStatisticsReq.DirectoryNumber, c->callInfo.callingPartyNumber, sizeof(r->msg.ConnectionStatisticsReq.DirectoryNumber));

	r->msg.ConnectionStatisticsReq.lel_callReference = htolel((c) ? c->callid : 0);
	r->msg.ConnectionStatisticsReq.lel_StatsProcessing = htolel(SKINNY_STATSPROCESSING_CLEAR);
	sccp_dev_send(d, r);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Device is Requesting CallStatisticsAndClear\n", (d && d->id) ? d->id : "SCCP");
}

/*!
 * \brief Tell Device to Open a RTP Receive Channel
 *
 * At this point we choose the codec for receive channel and tell them to device.
 * We will get a OpenReceiveChannelAck message that includes all information.
 *
 * \param c a locked SCCP Channel
 * 
 * \lock
 * 	  - see sccp_channel_updateChannelCapability_locked()
 * 	  - see sccp_channel_start_rtp()
 * 	  - see sccp_device_find_index_for_line()
 * 	  - see sccp_dev_starttone()
 * 	  - see sccp_dev_send()
 */
void sccp_channel_openreceivechannel_locked(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *d = NULL;

	int payloadType;

	int packetSize;

	struct sockaddr_in *them;

	uint16_t instance;

	if (!c || !c->device)
		return;

	d = c->device;

	sccp_channel_updateChannelCapability_locked(c);
	c->isCodecFix = TRUE;

#if ASTERISK_VERSION_NUM >= 10400
	struct ast_format_list fmt = pbx_codec_pref_getsize(&c->codecs, c->format & AST_FORMAT_AUDIO_MASK);

	payloadType = sccp_codec_ast2skinny(fmt.bits);
	packetSize = fmt.cur_ms;
#else
	payloadType = sccp_codec_ast2skinny(c->format);				// was c->format
	packetSize = 20;
#endif
	if (!payloadType) {
		c->isCodecFix = FALSE;
		sccp_channel_updateChannelCapability_locked(c);
		c->isCodecFix = TRUE;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: channel %s payloadType %d\n", c->device->id, (c->owner) ? c->owner->name : "NULL", payloadType);

	/* create the rtp stuff. It must be create before setting the channel AST_STATE_UP. otherwise no audio will be played */
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Ask the device to open a RTP port on channel %d. Codec: %s, echocancel: %s\n", c->device->id, c->callid, codec2str(payloadType), c->line->echocancel ? "ON" : "OFF");
	if (!c->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Starting RTP on channel %s-%08X\n", DEV_ID_LOG(c->device), c->line->name, c->callid);
	}
	if (!c->rtp.audio.rtp && !sccp_rtp_createAudioServer(c)) {
		ast_log(LOG_WARNING, "%s: Error opening RTP for channel %s-%08X\n", DEV_ID_LOG(c->device), c->line->name, c->callid);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_starttone(c->device, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
		return;
	}

#if ASTERISK_VERSION_NUM >= 10600
 	if(c->format & AST_FORMAT_SLINEAR16){
 		//payloadType = 25;
 		//c->rtp.audio.rtp.current_RTP_PT[payloadType].code = AST_FORMAT_SLINEAR16;
 		//ast_rtp_set_m_type(c->rtp.audio.rtp, payloadType);
 		//ast_rtp_set_rtpmap_type_rate(c->rtp.audio.rtp, payloadType, "audio", "L16", 0, 16000);
 	}
#endif

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Open receive channel with format %s[%d] (%d ms), payload %d, echocancel: %d\n", c->device->id, codec2str(payloadType), c->format, packetSize, payloadType, c->line->echocancel);

	if (d->inuseprotocolversion >= 17) {
		r = sccp_build_packet(OpenReceiveChannel, sizeof(r->msg.OpenReceiveChannel_v17));
		sccp_rtp_getAudioPeer(c, &them);
		memcpy(&r->msg.OpenReceiveChannel_v17.bel_remoteIpAddr, &them->sin_addr, 4);
		r->msg.OpenReceiveChannel_v17.lel_conferenceId = htolel(c->callid);
		r->msg.OpenReceiveChannel_v17.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.OpenReceiveChannel_v17.lel_millisecondPacketSize = htolel(packetSize);
		/* if something goes wrong the default codec is ulaw */
		r->msg.OpenReceiveChannel_v17.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.OpenReceiveChannel_v17.lel_vadValue = htolel(c->line->echocancel);
		r->msg.OpenReceiveChannel_v17.lel_conferenceId1 = htolel(c->callid);
		r->msg.OpenReceiveChannel_v17.lel_rtptimeout = htolel(10);
		r->msg.OpenReceiveChannel_v17.lel_unknown20 = htolel(4000);
	} else {
		REQ(r, OpenReceiveChannel);
		r->msg.OpenReceiveChannel.lel_conferenceId = htolel(c->callid);
		r->msg.OpenReceiveChannel.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.OpenReceiveChannel.lel_millisecondPacketSize = htolel(packetSize);
		/* if something goes wrong the default codec is ulaw */
		r->msg.OpenReceiveChannel.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.OpenReceiveChannel.lel_vadValue = htolel(c->line->echocancel);
		r->msg.OpenReceiveChannel.lel_conferenceId1 = htolel(c->callid);
		r->msg.OpenReceiveChannel.lel_rtptimeout = htolel(10);
	}
	sccp_dev_send(c->device, r);
	c->mediaStatus.receive = TRUE;

	//ast_rtp_set_vars(c->owner, c->rtp.audio.rtp);
	//sccp_channel_openMultiMediaChannel(c);
}

void sccp_channel_openMultiMediaChannel(sccp_channel_t * channel)
{
	sccp_moo_t *r;

	uint32_t skinnyFormat;

	int payloadType;

	uint32_t sampleRate;

	uint8_t lineInstance;

	if (channel->device && (channel->rtp.video.status & SCCP_RTP_STATUS_RECEIVE)) {
		return;
	}

	channel->rtp.video.status |= SCCP_RTP_STATUS_RECEIVE;
	skinnyFormat = sccp_codec_ast2skinny(channel->rtp.video.writeFormat);

	if (skinnyFormat == 0) {
		ast_log(LOG_NOTICE, "SCCP: Unable to find skinny format for %d\n", channel->rtp.video.writeFormat);
		return;
	}

	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, skinnyFormat);
	lineInstance = sccp_device_find_index_for_line(channel->device, channel->line->name);

	if (payloadType == -1) {
		payloadType = 97;
		sampleRate = 3840;
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Open receive multimedia channel with format %s[%d] skinnyFormat %s[%d], payload %d\n", DEV_ID_LOG(channel->device), pbx_codec2str(channel->rtp.video.writeFormat), channel->rtp.video.writeFormat, codec2str(skinnyFormat), skinnyFormat, payloadType);

	if (channel->device->inuseprotocolversion < 15) {
		r = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(r->msg.OpenMultiMediaChannelMessage));

		r->msg.OpenMultiMediaChannelMessage.lel_conferenceID = htolel(channel->callid);
		r->msg.OpenMultiMediaChannelMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.OpenMultiMediaChannelMessage.lel_payloadCapability = htolel(skinnyFormat);
		r->msg.OpenMultiMediaChannelMessage.lel_lineInstance = htolel(lineInstance);
		r->msg.OpenMultiMediaChannelMessage.lel_callReference = htolel(channel->callid);
		r->msg.OpenMultiMediaChannelMessage.lel_payloadType = htolel(payloadType);
		r->msg.OpenMultiMediaChannelMessage.audioParameter.millisecondPacketSize = htolel(sampleRate);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.h261VideoCapability.temporalSpatialTradeOffCapability = htolel(0x00000040);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.h261VideoCapability.stillImageTransmission = htolel(0x00000032);	//= htolel(0x00000024);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.h263VideoCapability.h263CapabilityBitfield = htolel(0x4c3a525b);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.h263VideoCapability.annexNandwFutureUse = htolel(0x202d2050);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.vieoVideoCapability.modelNumber = htolel(0x203a5048);
		r->msg.OpenMultiMediaChannelMessage.videoParameter.vieoVideoCapability.bandwidth = htolel(0x4e202c30);
		r->msg.OpenMultiMediaChannelMessage.dataParameter.protocolDependentData = 0;
		r->msg.OpenMultiMediaChannelMessage.dataParameter.maxBitRate = 0;
	} else {
		r = sccp_build_packet(OpenMultiMediaChannelMessage, sizeof(r->msg.OpenMultiMediaChannelMessage_v17));

		r->msg.OpenMultiMediaChannelMessage_v17.lel_conferenceID = htolel(channel->callid);
		r->msg.OpenMultiMediaChannelMessage_v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.OpenMultiMediaChannelMessage_v17.lel_payloadCapability = htolel(skinnyFormat);
		r->msg.OpenMultiMediaChannelMessage_v17.lel_lineInstance = htolel(lineInstance);
		r->msg.OpenMultiMediaChannelMessage_v17.lel_callReference = htolel(channel->callid);
		r->msg.OpenMultiMediaChannelMessage_v17.lel_payloadType = htolel(payloadType);
		r->msg.OpenMultiMediaChannelMessage_v17.audioParameter.millisecondPacketSize = htolel(sampleRate);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.h261VideoCapability.temporalSpatialTradeOffCapability = htolel(0x00000040);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.h261VideoCapability.stillImageTransmission = htolel(0x00000032);	//= htolel(0x00000024);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.h263VideoCapability.h263CapabilityBitfield = htolel(0x4c3a525b);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.h263VideoCapability.annexNandwFutureUse = htolel(0x202d2050);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.vieoVideoCapability.modelNumber = htolel(0x203a5048);
		r->msg.OpenMultiMediaChannelMessage_v17.videoParameter.vieoVideoCapability.bandwidth = htolel(0x4e202c30);
		r->msg.OpenMultiMediaChannelMessage_v17.dataParameter.protocolDependentData = 0;
		r->msg.OpenMultiMediaChannelMessage_v17.dataParameter.maxBitRate = 0;
	}

	sccp_dev_send(channel->device, r);
}

void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel)
{
	sccp_moo_t *r;

	int skinnyFormat, payloadType;
	sccp_device_t 		*d = NULL;
	struct sockaddr_in 	sin;
	struct ast_hostent 	ahp;
	struct hostent 		*hp;
	int packetSize = 20;							/* \todo unused? */

	channel->rtp.video.readFormat = AST_FORMAT_H264;
	skinnyFormat = sccp_codec_ast2skinny(channel->rtp.video.readFormat);
	packetSize = 3840;

	if (!channel->rtp.video.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start vrtp media transmission, maybe channel is down %s-%08X\n", channel->device->id, channel->line->name, channel->callid);
		return;
	}

	if (!(d = channel->device))
		return;

	/* lookup payloadType */
	payloadType = sccp_rtp_get_payloadType(&channel->rtp.video, skinnyFormat);
	if (payloadType == -1) {
		//TODO handle payload error
		payloadType = 97;
	}
	
#if ASTERISK_VERSION_NUM >= 10600
	switch(channel->rtp.video.readFormat){
	  case AST_FORMAT_H263:
	    ast_rtp_set_m_type(channel->rtp.video.rtp, payloadType);
	    ast_rtp_set_rtpmap_type_rate(channel->rtp.video.rtp, channel->rtp.video.readFormat, "video", "H263", 0, 0);
	    break;
	  case AST_FORMAT_H264:
	    ast_rtp_set_m_type(channel->rtp.video.rtp, payloadType);
	    ast_rtp_set_rtpmap_type_rate(channel->rtp.video.rtp, channel->rtp.video.readFormat, "video", "H264", 0, 0);
	  break;
	}
#endif

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: using payload %d\n", channel->device->id, payloadType);

	ast_rtp_get_us(channel->rtp.video.rtp, &sin);

	if (channel->device->nat) {
		if (GLOB(externip.sin_addr.s_addr)) {
			if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
				time(&GLOB(externexpire));
				GLOB(externexpire) += GLOB(externrefresh);
				if ((hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
					memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
				} else
					ast_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
			}
			memcpy(&sin.sin_addr, &GLOB(externip.sin_addr), 4);
		}
	}

	if (d->inuseprotocolversion < 15) {
		r = sccp_build_packet(StartMultiMediaTransmission, sizeof(r->msg.StartMultiMediaTransmission));
		r->msg.StartMultiMediaTransmission.lel_conferenceID = htolel(channel->callid);
		r->msg.StartMultiMediaTransmission.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StartMultiMediaTransmission.lel_payloadCapability = htolel(skinnyFormat);
		memcpy(&r->msg.StartMultiMediaTransmission.bel_remoteIpAddr, &sin.sin_addr, 4);
		r->msg.StartMultiMediaTransmission.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
		r->msg.StartMultiMediaTransmission.lel_callReference = htolel(channel->callid);
		r->msg.StartMultiMediaTransmission.lel_payloadType = payloadType;
		r->msg.StartMultiMediaTransmission.lel_DSCPValue = htolel(136);
		r->msg.StartMultiMediaTransmission.audioParameter.millisecondPacketSize = htolel(packetSize);
		r->msg.StartMultiMediaTransmission.audioParameter.lel_echoCancelType = 0;
		r->msg.StartMultiMediaTransmission.videoParameter.bitRate = 0;
		r->msg.StartMultiMediaTransmission.videoParameter.pictureFormatCount = 0;
		r->msg.StartMultiMediaTransmission.videoParameter.confServiceNum = 0;
		r->msg.StartMultiMediaTransmission.videoParameter.dummy = 0;
		r->msg.StartMultiMediaTransmission.videoParameter.h261VideoCapability.temporalSpatialTradeOffCapability = htolel(0x00000040);	/* profile */
		r->msg.StartMultiMediaTransmission.videoParameter.h261VideoCapability.stillImageTransmission = htolel(0x00000032);	/* level */
		r->msg.StartMultiMediaTransmission.dataParameter.protocolDependentData = htolel(0x002415f8);
		r->msg.StartMultiMediaTransmission.dataParameter.maxBitRate = htolel(0x098902c4);
	} else {

		r = sccp_build_packet(StartMultiMediaTransmission, sizeof(r->msg.StartMultiMediaTransmission_v17));
		r->msg.StartMultiMediaTransmission_v17.lel_conferenceID = htolel(channel->callid);
		r->msg.StartMultiMediaTransmission_v17.lel_passThruPartyId = htolel(channel->passthrupartyid);
		r->msg.StartMultiMediaTransmission_v17.lel_payloadCapability = htolel(skinnyFormat);
		memcpy(&r->msg.StartMultiMediaTransmission_v17.bel_remoteIpAddr, &sin.sin_addr, 4);
		r->msg.StartMultiMediaTransmission_v17.lel_remotePortNumber = htolel(ntohs(sin.sin_port));
		r->msg.StartMultiMediaTransmission_v17.lel_callReference = htolel(channel->callid);
		r->msg.StartMultiMediaTransmission_v17.lel_payloadType = payloadType;
		r->msg.StartMultiMediaTransmission_v17.lel_DSCPValue = htolel(136);
		r->msg.StartMultiMediaTransmission_v17.audioParameter.millisecondPacketSize = htolel(packetSize);
		r->msg.StartMultiMediaTransmission_v17.audioParameter.lel_echoCancelType = 0;
		r->msg.StartMultiMediaTransmission_v17.videoParameter.bitRate = 0;
		r->msg.StartMultiMediaTransmission_v17.videoParameter.pictureFormatCount = 0;
		r->msg.StartMultiMediaTransmission_v17.videoParameter.confServiceNum = 0;
		r->msg.StartMultiMediaTransmission_v17.videoParameter.dummy = 0;
		r->msg.StartMultiMediaTransmission_v17.videoParameter.h261VideoCapability.temporalSpatialTradeOffCapability = htolel(0x00000040);	/* profile */
		r->msg.StartMultiMediaTransmission_v17.videoParameter.h261VideoCapability.stillImageTransmission = htolel(0x00000032);	/* level */
		r->msg.StartMultiMediaTransmission_v17.videoParameter.h263VideoCapability.h263CapabilityBitfield = htolel(0x4c3a525b);
		r->msg.StartMultiMediaTransmission_v17.videoParameter.h263VideoCapability.annexNandwFutureUse = htolel(0x202d2050);
		r->msg.StartMultiMediaTransmission_v17.videoParameter.vieoVideoCapability.modelNumber = htolel(0x203a5048);
		r->msg.StartMultiMediaTransmission_v17.videoParameter.vieoVideoCapability.bandwidth = htolel(0x4e202c30);
		r->msg.StartMultiMediaTransmission_v17.dataParameter.protocolDependentData = htolel(0x002415f8);
		r->msg.StartMultiMediaTransmission_v17.dataParameter.maxBitRate = htolel(0x098902c4);
	}

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send VRTP media to %s:%d with codec: %s(%d) (%d ms), payloadType %d, tos %d, silencesuppression: %s\n", channel->device->id, pbx_inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), pbx_codec2str(channel->rtp.video.readFormat), channel->rtp.video.readFormat, packetSize, payloadType, channel->line->audio_tos, channel->line->silencesuppression ? "ON" : "OFF");
	sccp_dev_send(channel->device, r);

	r = sccp_build_packet(FlowControlCommandMessage, sizeof(r->msg.FlowControlCommandMessage));
	r->msg.FlowControlCommandMessage.lel_conferenceID = htolel(channel->callid);
	r->msg.FlowControlCommandMessage.lel_passThruPartyId = htolel(channel->passthrupartyid);
	r->msg.FlowControlCommandMessage.lel_callReference = htolel(channel->callid);
	r->msg.FlowControlCommandMessage.maxBitRate = htolel(500000);
	sccp_dev_send(channel->device, r);
}

/*!
 * \brief Tell a Device to Start Media Transmission.
 *
 * We choose codec according to c->format.
 *
 * \param c SCCP Channel
 * \note rtp should be started before, otherwise we do not start transmission
 */
void sccp_channel_startmediatransmission(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *d = NULL;

	struct ast_hostent ahp;

	struct hostent *hp;

	int payloadType;

	int packetSize;

#if ASTERISK_VERSION_NUM >= 10400
	struct ast_format_list fmt;
#endif
	if (!c->rtp.audio.rtp) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can't start rtp media transmission, maybe channel is down %s-%08X\n", c->device->id, c->line->name, c->callid);
		return;
	}

	if (!(d = c->device))
		return;

	if (d->nat) {
		// replace us.sin_addr if we are natted
		if (GLOB(externip.sin_addr.s_addr)) {
			if (GLOB(externexpire) && (time(NULL) >= GLOB(externexpire))) {
				time(&GLOB(externexpire));
				GLOB(externexpire) += GLOB(externrefresh);
				if ((hp = pbx_gethostbyname(GLOB(externhost), &ahp))) {
					memcpy(&GLOB(externip.sin_addr), hp->h_addr, sizeof(GLOB(externip.sin_addr)));
				} else
					ast_log(LOG_NOTICE, "Warning: Re-lookup of '%s' failed!\n", GLOB(externhost));
			}
			memcpy(&c->rtp.audio.phone_remote.sin_addr, &GLOB(externip.sin_addr), 4);
		}
	} else {
//              sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: get remote peer", c->device->id);
//              if(!ast_rtp_get_peer(c->rtp.audio.rtp, &c->rtp.audio.phone_remote)){
//                        sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 " failed\n");
//              }else{
//                        sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 " '%s:%d'\n",pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr), ntohs(c->rtp.audio.phone_remote.sin_port));
//              }
//        
//              /* */
//              if(c->rtp.audio.phone_remote.sin_port==0){
//                      sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: get our rtp server address", c->device->id);
//                      ast_rtp_get_us(c->rtp.audio.rtp, &c->rtp.audio.phone_remote);
//                      sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 " '%s:%d'\n",pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr), ntohs(c->rtp.audio.phone_remote.sin_port));
//              }
	}

#if ASTERISK_VERSION_NUM >= 10400
	fmt = pbx_codec_pref_getsize(&c->device->codecs, c->format);
	payloadType = sccp_codec_ast2skinny(fmt.bits);
	packetSize = fmt.cur_ms;
#else
	payloadType = sccp_codec_ast2skinny(c->format);				// was c->format
	packetSize = 20;
#endif
	
	
	if (d->inuseprotocolversion < 17) {
		REQ(r, StartMediaTransmission);
		r->msg.StartMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StartMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StartMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		r->msg.StartMediaTransmission.lel_millisecondPacketSize = htolel(packetSize);
		r->msg.StartMediaTransmission.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.StartMediaTransmission.lel_precedenceValue = htolel(c->line->audio_tos);
		r->msg.StartMediaTransmission.lel_ssValue = htolel(c->line->silencesuppression);	// Silence supression
		r->msg.StartMediaTransmission.lel_maxFramesPerPacket = htolel(0);
		r->msg.StartMediaTransmission.lel_rtptimeout = htolel(10);
		r->msg.StartMediaTransmission.bel_remoteIpAddr = htolel(c->rtp.audio.phone_remote.sin_addr.s_addr);
		r->msg.StartMediaTransmission.lel_remotePortNumber = htolel(ntohs(c->rtp.audio.phone_remote.sin_port));
	} else {
		r = sccp_build_packet(StartMediaTransmission, sizeof(r->msg.StartMediaTransmission_v17));
		r->msg.StartMediaTransmission_v17.lel_conferenceId = htolel(c->callid);
		r->msg.StartMediaTransmission_v17.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StartMediaTransmission_v17.lel_conferenceId1 = htolel(c->callid);
		r->msg.StartMediaTransmission_v17.lel_millisecondPacketSize = htolel(packetSize);
		r->msg.StartMediaTransmission_v17.lel_payloadType = htolel((payloadType) ? payloadType : 4);
		r->msg.StartMediaTransmission_v17.lel_precedenceValue = htolel(c->line->audio_tos);
		r->msg.StartMediaTransmission_v17.lel_ssValue = htolel(c->line->silencesuppression);	// Silence supression
		r->msg.StartMediaTransmission_v17.lel_maxFramesPerPacket = htolel(0);
		r->msg.StartMediaTransmission_v17.lel_rtptimeout = htolel(10);
		memcpy(&r->msg.StartMediaTransmission_v17.bel_remoteIpAddr, &c->rtp.audio.phone_remote.sin_addr, 4);
		r->msg.StartMediaTransmission_v17.lel_remotePortNumber = htolel(ntohs(c->rtp.audio.phone_remote.sin_port));
	}
	sccp_dev_send(c->device, r);
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell device to send RTP media to: '%s:%d' with codec: %s(%d) (%d ms), tos %d, silencesuppression: %s\n", c->device->id, pbx_inet_ntoa(c->rtp.audio.phone_remote.sin_addr), ntohs(c->rtp.audio.phone_remote.sin_port), codec2str(payloadType), payloadType, packetSize, c->line->audio_tos, c->line->silencesuppression ? "ON" : "OFF");

#ifdef CS_SCCP_VIDEO
	if (sccp_device_isVideoSupported(c->device)) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We can have video, try to start vrtp\n", DEV_ID_LOG(c->device));
		if (!c->rtp.video.rtp && !sccp_rtp_createVideoServer(c)) {
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: can not start vrtp\n", DEV_ID_LOG(c->device));
		} else {
			sccp_channel_startMultiMediaTransmission(c);
		}
	}else{
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: We have no video support\n", DEV_ID_LOG(c->device));
	}
#else
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Video support not enabled\n", DEV_ID_LOG(c->device));
#endif
}

/*!
 * \brief Tell Device to Close an RTP Receive Channel and Stop Media Transmission
 * \param c SCCP Channel
 * \note sccp_channel_stopmediatransmission is explicit call within this function!
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_dev_send()
 */
void sccp_channel_closereceivechannel_locked(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *d = c->device;

	if (d) {
		REQ(r, CloseReceiveChannel);
		r->msg.CloseReceiveChannel.lel_conferenceId = htolel(c->callid);
		r->msg.CloseReceiveChannel.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.CloseReceiveChannel.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Close openreceivechannel on channel %d\n", DEV_ID_LOG(d), c->callid);
	}
	c->isCodecFix = FALSE;
	c->mediaStatus.receive = FALSE;
	c->rtp.audio.status &= ~SCCP_RTP_STATUS_RECEIVE;

	if (c->rtp.video.rtp) {
		REQ(r, CloseMultiMediaReceiveChannel);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId = htolel(c->callid);
		r->msg.CloseMultiMediaReceiveChannel.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.CloseMultiMediaReceiveChannel.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
	}

	sccp_channel_stopmediatransmission_locked(c);
}

/*!
 * \brief Tell device to Stop Media Transmission.
 *
 * Also RTP will be Stopped/Destroyed and Call Statistic is requested.
 * \param c SCCP Channel
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_stop_rtp()
 * 	  - see sccp_dev_send()
 */
void sccp_channel_stopmediatransmission_locked(sccp_channel_t * c)
{
	sccp_moo_t *r;

	sccp_device_t *d = NULL;

	if (!c)
		return;

	d = c->device;

	REQ(r, StopMediaTransmission);
	if (d) {
		r->msg.StopMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StopMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StopMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Stop media transmission on channel %d\n", (d && d->id) ? d->id : "(none)", c->callid);
	}
	c->mediaStatus.transmit = FALSE;
	// stopping rtp
	if (c->rtp.audio.rtp) {
		sccp_rtp_stop(c);
	}
	c->rtp.audio.status &= ~SCCP_RTP_STATUS_TRANSMIT;

	// stopping vrtp
	if (c->rtp.video.rtp) {
		REQ(r, StopMultiMediaTransmission);
		r->msg.StopMultiMediaTransmission.lel_conferenceId = htolel(c->callid);
		r->msg.StopMultiMediaTransmission.lel_passThruPartyId = htolel(c->passthrupartyid);
		r->msg.StopMultiMediaTransmission.lel_conferenceId1 = htolel(c->callid);
		sccp_dev_send(d, r);
	}

	/* requesting statistics */
	sccp_channel_StatisticsRequest(c);
}

/*!
 * \brief Update Channel Media Type / Native Bridged Format Match
 * \param c SCCP Channel
 * \note Copied function from v2 (FS)
 */
void sccp_channel_updatemediatype_locked(sccp_channel_t * c)
{
	struct ast_channel *bridged = NULL;

	/* checking for ast_channel owner */
	if (!c->owner) {
		return;
	}
	/* is owner ast_channel a zombie ? */
	if (ast_test_flag(c->owner, AST_FLAG_ZOMBIE)) {
		return;								/* c->owner is zombie, leaving */
	}
	/* channel is hanging up */
	if (c->owner->_softhangup != 0) {
		return;
	}
	/* channel is not running */
	if (!c->owner->pbx) {
		return;
	}
	/* check for briged ast_channel */
	if (!(bridged = CS_AST_BRIDGED_CHANNEL(c->owner))) {
		return;								/* no bridged channel, leaving */
	}
	/* is bridged ast_channel a zombie ? */
	if (ast_test_flag(bridged, AST_FLAG_ZOMBIE)) {
		return;								/* bridged channel is zombie, leaving */
	}
	/* channel is hanging up */
	if (bridged->_softhangup != 0) {
		return;
	}
	/* channel is not running */
	if (!bridged->pbx) {
		return;
	}
	if (c->state != SCCP_CHANNELSTATE_ZOMBIE) {
		ast_log(LOG_NOTICE, "%s: Channel %s -> nativeformats:%d - r:%d/w:%d - rr:%d/rw:%d\n", DEV_ID_LOG(c->device), bridged->name, bridged->nativeformats, bridged->writeformat, bridged->readformat,
#ifdef CS_AST_HAS_TECH_PVT
			bridged->rawreadformat, bridged->rawwriteformat
#else
			bridged->pvt->rawwriteformat, bridged->pvt->rawreadformat
#endif
		    );
		if (!(bridged->nativeformats & c->owner->nativeformats) == (bridged->nativeformats & c->device->capability)) {
			ast_log(LOG_NOTICE, "%s: Doing the dirty thing.\n", DEV_ID_LOG(c->device));
			c->owner->nativeformats = c->format = bridged->nativeformats;
			sccp_channel_closereceivechannel_locked(c);
			usleep(100);
			sccp_channel_openreceivechannel_locked(c);
			ast_set_read_format(c->owner, c->format);
			ast_set_write_format(c->owner, c->format);
		}
	}
}

int sccp_channel_destroy_callback(const void *data)
{
	sccp_channel_t *c = (sccp_channel_t *) data;

	if (!c)
		return 0;

	sccp_channel_lock(c);
	sccp_channel_destroy_locked(c);

	return 0;
}

/*!
 * \brief Hangup this channel.
 * \param c *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- line->channels
 * 	  - see sccp_channel_endcall()
 */
void sccp_channel_endcall_locked(sccp_channel_t * c)
{
	uint8_t res = 0;

	if (!c || !c->line) {
		ast_log(LOG_WARNING, "No channel or line or device to hangup\n");
		return;
	}

	/* this is a station active endcall or onhook */
	sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL)) (VERBOSE_PREFIX_2 "%s: Ending call %d on line %s (%s)\n", DEV_ID_LOG(c->device), c->callid, c->line->name, sccp_indicate2str(c->state));

	/* end all call forward channels (our childs) */
	sccp_channel_t *channel;

	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if (channel->parentChannel == c) {
			/* No need to lock because c->line->channels is already locked. */
			sccp_channel_endcall_locked(channel);
		}
	}
	SCCP_LIST_UNLOCK(&c->line->channels);

	/**
	workaround to fix issue with 7960 and protocol version != 6
	7960 loses callplane when cancel transfer (end call on other channel).
	This script set the hold state for transfer_channel explicitly -MC
	*/
	if (c->device && c->device->transfer_channel && c->device->transfer_channel != c) {
		uint8_t instance = sccp_device_find_index_for_line(c->device, c->device->transfer_channel->line->name);

		sccp_dev_set_lamp(c->device, SKINNY_STIMULUS_LINE, instance, SKINNY_LAMP_WINK);
		sccp_device_sendcallstate(c->device, instance, c->device->transfer_channel->callid, SKINNY_CALLSTATE_HOLD, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
		sccp_dev_set_keyset(c->device, instance, c->device->transfer_channel->callid, KEYMODE_ONHOLD);
		c->device->transfer_channel = NULL;
	}

	if (c->owner) {
		/* Is there a blocker ? */
		res = (c->owner->pbx || c->owner->blocker);

		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending %s hangup request to %s\n", DEV_ID_LOG(c->device), res ? "(queue)" : "(force)", c->owner->name);

		c->owner->hangupcause = AST_CAUSE_NORMAL_CLEARING;
		if ((c->owner->_softhangup & AST_SOFTHANGUP_APPUNLOAD) != 0) {
			c->owner->hangupcause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
		} else {
			if (c->owner->masq != NULL) {				// we are masquerading
				c->owner->hangupcause = AST_CAUSE_ANSWERED_ELSEWHERE;
			}
		}

		/* force hangup for invalid dials */
		if (c->state == SCCP_CHANNELSTATE_INVALIDNUMBER || c->state == SCCP_CHANNELSTATE_OFFHOOK) {
			sccp_log((DEBUGCAT_CORE | DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "%s: Sending force hangup request to %s\n", DEV_ID_LOG(c->device), c->owner->name);
			ast_hangup(c->owner);
		} else {
			if (res) {
				c->owner->_softhangup |= AST_SOFTHANGUP_DEV;
				ast_queue_hangup(c->owner);
			} else {
				ast_hangup(c->owner);
			}
		}
	} else {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_1 "%s: No Asterisk channel to hangup for sccp channel %d on line %s\n", DEV_ID_LOG(c->device), c->callid, c->line->name);
	}
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \return TRUE if the newcall is successful
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_newcall_locked()
 */
boolean_t sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype)
{
	sccp_channel_t *c;

	c = sccp_channel_newcall_locked(l, device, dial, calltype);
	if (c) {
		sccp_channel_unlock(c);
		return TRUE;
	} else
		return FALSE;
}

/*!
 * \brief Allocate a new Outgoing Channel.
 *
 * \param l SCCP Line that owns this channel
 * \param device SCCP Device that owns this channel
 * \param dial Dialed Number as char
 * \param calltype Calltype as int
 * \return a *locked* SCCP Channel or NULL if something is wrong
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- channel
 * 	  - see sccp_channel_set_active()
 * 	  - see sccp_indicate_nolock()
 */
sccp_channel_t *sccp_channel_newcall_locked(sccp_line_t * l, sccp_device_t * device, char *dial, uint8_t calltype)
{
	/* handle outgoing calls */
	sccp_channel_t *c;

	if (!l) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a line is not defined!\n");
		return NULL;
	}

	if (!device || sccp_strlen_zero(device->id)) {
		ast_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if a device is not defined!\n");
		return NULL;
	}

	/* look if we have a call to put on hold */
	if ((c = sccp_channel_get_active_locked(device)) && (NULL == c->conference)) {
		/* there is an active call, let's put it on hold first */
		int ret = sccp_channel_hold_locked(c);

		sccp_channel_unlock(c);
		if (!ret)
			return NULL;
	}

	c = sccp_channel_allocate_locked(l, device);

	if (!c) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", device->id, l->name);
		return NULL;
	}

	c->ss_action = SCCP_SS_DIAL;						/* softswitch will catch the number to be dialed */
	c->ss_data = 0;								/* nothing to pass to action */

	c->calltype = calltype;

	sccp_channel_set_active(device, c);

	/* copy the number to dial in the ast->exten */
	if (dial) {
		sccp_copy_string(c->dialedNumber, dial, sizeof(c->dialedNumber));
		sccp_indicate_locked(device, c, SCCP_CHANNELSTATE_SPEEDDIAL);
	} else {
		sccp_indicate_locked(device, c, SCCP_CHANNELSTATE_OFFHOOK);
	}

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate_locked(c)) {
		ast_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", device->id, l->name);
		sccp_indicate_locked(device, c, SCCP_CHANNELSTATE_CONGESTION);
		return c;
	}

	sccp_ast_setstate(c, AST_STATE_OFFHOOK);

	if (device->earlyrtp == SCCP_CHANNELSTATE_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openreceivechannel_locked(c);
	}

	if (dial) {
		sccp_pbx_softswitch_locked(c);
		return c;
	}

	if ((c->digittimeout = sccp_sched_add(sched, GLOB(firstdigittimeout) * 1000, sccp_pbx_sched_dial, c)) < 0) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unable to schedule dialing in '%d' ms\n", GLOB(firstdigittimeout));
	}

	return c;
}

/*!
 * \brief Answer an Incoming Call.
 * \param device SCCP Device who answers
 * \param c incoming *locked* SCCP channel
 * \todo handle codec choose
 *
 * \callgraph
 * \callergraph
 *
 * \lock
 * 	- line
 * 	- line->channels
 * 	  - see sccp_channel_endcall()
 */
void sccp_channel_answer_locked(sccp_device_t * device, sccp_channel_t * c)
{
	sccp_line_t *l;

	sccp_device_t *d;

	sccp_channel_t *c1;

#ifdef CS_AST_HAS_FLAG_MOH
	struct ast_channel *bridged;
#endif

	if (!c || !c->line) {
		ast_log(LOG_ERROR, "SCCP: Channel %d has no line\n", (c ? c->callid : 0));
		return;
	}

	if (!c->owner) {
		ast_log(LOG_ERROR, "SCCP: Channel %d has no owner\n", c->callid);
		return;
	}

	l = c->line;
	d = (c->state == SCCP_CHANNELSTATE_HOLD) ? device : c->device;

	/* channel was on hold, restore active -> inc. channelcount */
	if (c->state == SCCP_CHANNELSTATE_HOLD) {
		sccp_line_lock(c->line);
		c->line->statistic.numberOfActiveChannels--;
		sccp_line_unlock(c->line);
	}

	if (!d) {
		if (!device) {
			ast_log(LOG_ERROR, "SCCP: Channel %d has no device\n", (c ? c->callid : 0));
			return;
		}
		d = device;
	}
	c->device = d;

	sccp_channel_updateChannelCapability_locked(c);

	/* answering an incoming call */
	/* look if we have a call to put on hold */
	if ((c1 = sccp_channel_get_active_locked(d)) != NULL) {
		/* If there is a ringout or offhook call, we end it so that we can answer the call. */
		if (c1->state == SCCP_CHANNELSTATE_OFFHOOK || c1->state == SCCP_CHANNELSTATE_RINGOUT) {
			sccp_channel_endcall_locked(c1);
		} else if (!sccp_channel_hold_locked(c1)) {
			/* there is an active call, let's put it on hold first */
			sccp_channel_unlock(c1);
			return;
		}
		sccp_channel_unlock(c1);
		c1 = NULL;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answer the channel %s-%08X\n", DEV_ID_LOG(d), l->name, c->callid);

	/* end callforwards */
	sccp_channel_t *channel;

	SCCP_LIST_LOCK(&c->line->channels);
	SCCP_LIST_TRAVERSE(&c->line->channels, channel, list) {
		if (channel->parentChannel == c) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hangup cfwd channel %s-%08X\n", DEV_ID_LOG(d), l->name, channel->callid);
			/* No need to lock because c->line->channels is already locked. */
			sccp_channel_endcall_locked(channel);
			c->answered_elsewhere = TRUE;
		}
	}
	SCCP_LIST_UNLOCK(&c->line->channels);
	/* */

	sccp_channel_set_active(d, c);
	sccp_dev_set_activeline(d, c->line);

	/* the old channel state could be CALLTRANSFER, so the bridged channel is on hold */
	/* do we need this ? -FS */
#ifdef CS_AST_HAS_FLAG_MOH
	bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (bridged && ast_test_flag(bridged, AST_FLAG_MOH)) {
		pbx_moh_stop(bridged);
		ast_clear_flag(bridged, AST_FLAG_MOH);
	}
#endif

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Answering channel with state '%s' (%d)\n", DEV_ID_LOG(c->device), ast_state2str(c->owner->_state), c->owner->_state);
	ast_queue_control(c->owner, AST_CONTROL_ANSWER);

	/* \todo Check if the following breaks protocol! */
	/* It seems to break 7910. Test and fix in this case further. */
	 if (c->state != SCCP_CHANNELSTATE_OFFHOOK)
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_OFFHOOK);
		

	sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_CONNECTED);
}

/*!
 * \brief Put channel on Hold.
 *
 * \param c *locked* SCCP Channel
 * \return Status as in (0 if something was wrong, otherwise 1)
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 */
int sccp_channel_hold_locked(sccp_channel_t * c)
{
	sccp_line_t *l;

	sccp_device_t *d;

	uint16_t instance;

	if (!c)
		return 0;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->device;

	if (c->state == SCCP_CHANNELSTATE_HOLD) {
		ast_log(LOG_WARNING, "SCCP: Channel already on hold\n");
		return 0;
	}

	/* put on hold an active call */
	if (c->state != SCCP_CHANNELSTATE_CONNECTED && c->state != SCCP_CHANNELSTATE_PROCEED) {	// TOLL FREE NUMBERS STAYS ALWAYS IN CALL PROGRESS STATE
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't put on hold an inactive channel %s-%08X (%s)\n", d->id, l->name, c->callid, sccp_indicate2str(c->state));
		/* hard button phones need it */
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, 5);
		return 0;
	}

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Hold the channel %s-%08X\n", d->id, l->name, c->callid);

	struct ast_channel *peer;

	peer = CS_AST_BRIDGED_CHANNEL(c->owner);

	if (peer) {
#ifdef CS_AST_RTP_NEW_SOURCE
		ast_rtp_new_source(c->rtp.audio.rtp);
#endif
		pbx_moh_start(peer, NULL, l->musicclass);

#ifdef CS_AST_HAS_FLAG_MOH
		ast_set_flag(peer, AST_FLAG_MOH);
#endif
	}
#ifdef CS_AST_CONTROL_HOLD
	if (!c->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "C owner disappeared! Can't free ressources\n");
		return 0;
	}
	sccp_ast_queue_control(c, AST_CONTROL_HOLD);
#endif

	sccp_device_lock(d);
	d->active_channel = NULL;
	sccp_device_unlock(d);
	sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_HOLD);			// this will also close (but not destroy) the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: On\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", c->owner->name, c->owner->uniqueid);
#endif

	if (l) {
		l->statistic.numberOfActiveChannels--;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", c->passthrupartyid, c->state);

	return 1;
}

/*!
 * \brief Resume a channel that is on hold.
 * \param device device who resumes the channel
 * \param c channel
 * \param swap_channels Swap Channels as Boolean
 * \return 0 if something was wrong, otherwise 1
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	- channel
 * 	  - see sccp_channel_updateChannelCapability_locked()
 * 	- channel
 */
int sccp_channel_resume_locked(sccp_device_t * device, sccp_channel_t * c, boolean_t swap_channels)
{
	sccp_line_t *l;

	sccp_device_t *d;

	sccp_channel_t *hold;

	uint16_t instance;

	if (!c || !c->owner)
		return 0;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return 0;
	}

	l = c->line;
	d = c->device;

	/* on remote device pickups the call */
	if (d != device)
		d = device;

	/* look if we have a call to put on hold */
	if (swap_channels && (hold = sccp_channel_get_active_locked(d))) {
		/* there is an active call, let's put it on hold first */
		int ret = sccp_channel_hold_locked(hold);

		sccp_channel_unlock(hold);
		hold = NULL;

		if (!ret)
			return 0;
	}

	if (c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED) {
		sccp_channel_hold_locked(c);
	}

	/* resume an active call */
	if (c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER && c->state != SCCP_CHANNELSTATE_CALLCONFERENCE) {
		/* something wrong on the code let's notify it for a fix */
		ast_log(LOG_ERROR, "%s can't resume the channel %s-%08X. Not on hold\n", d->id, l->name, c->callid);
		instance = sccp_device_find_index_for_line(d, l->name);
		sccp_dev_displayprompt(d, instance, c->callid, "No active call to put on hold", 5);
		return 0;
	}

	/* check if we are in the middle of a transfer */
	sccp_device_lock(d);
	if (d->transfer_channel == c) {
		d->transfer_channel = NULL;
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer on the channel %s-%08X\n", d->id, l->name, c->callid);
	}

	if (d->conference_channel == c) {
		d->conference_channel = NULL;
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Conference on the channel %s-%08X\n", d->id, l->name, c->callid);
	}

	sccp_device_unlock(d);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Resume the channel %s-%08X\n", d->id, l->name, c->callid);

	struct ast_channel *peer;

	sccp_channel_t *sccp_peer;

	peer = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (peer) {
		sccp_peer = get_sccp_channel_from_ast_channel(peer);
		pbx_moh_stop(peer);
#ifdef CS_AST_RTP_NEW_SOURCE
		if (c->rtp.audio.rtp)
			ast_rtp_new_source(c->rtp.audio.rtp);
#endif

		// this is for STABLE version
#ifdef CS_AST_HAS_FLAG_MOH
		ast_clear_flag(peer, AST_FLAG_MOH);
#endif
	}
#ifdef CS_AST_CONTROL_HOLD
#    ifdef CS_AST_RTP_NEW_SOURCE
	if (c->rtp.audio.rtp)
		ast_rtp_new_source(c->rtp.audio.rtp);
#    endif
	sccp_ast_queue_control(c, AST_CONTROL_UNHOLD);
#endif

	sccp_rtp_stop(c);

	c->device = d;

	/* force codec update */
	c->isCodecFix = FALSE;
	sccp_channel_updateChannelCapability_locked(c);
	c->isCodecFix = TRUE;
	/* */

	c->state = SCCP_CHANNELSTATE_HOLD;
	sccp_rtp_createAudioServer(c);

	sccp_channel_set_active(d, c);
#ifdef CS_AST_CONTROL_SRCUPDATE
	sccp_ast_queue_control(c, AST_CONTROL_SRCUPDATE);			// notify changes e.g codec
#endif
	sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_CONNECTED);		// this will also reopen the RTP stream

#ifdef CS_MANAGER_EVENTS
	if (GLOB(callevents))
		manager_event(EVENT_FLAG_CALL, "Hold", "Status: Off\r\n" "Channel: %s\r\n" "Uniqueid: %s\r\n", c->owner->name, c->owner->uniqueid);
#endif

	/* state of channel is set down from the remoteDevices, so correct channel state */
	c->state = SCCP_CHANNELSTATE_CONNECTED;
	if (l) {
		l->statistic.numberOfHoldChannels--;
	}

	sccp_log((DEBUGCAT_CHANNEL + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "C partyID: %u state: %d\n", c->passthrupartyid, c->state);
	return 1;
}

/*!
 * \brief Cleanup Channel before Free.
 * \param c SCCP Channel
 * \note we assume channel is locked
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 * 	  - see sccp_device_find_selectedchannel()
 * 	  - device->selectedChannels
 */
void sccp_channel_clean_locked(sccp_channel_t * c)				// we assume channel is locked
{
	sccp_line_t *l;

	sccp_device_t *d;

	sccp_selectedchannel_t *x;

	if (!c)
		return;

	d = c->device;
	l = c->line;
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Cleaning channel %08x\n", c->callid);

	/* mark the channel DOWN so any pending thread will terminate */
	if (c->owner) {
		ast_setstate(c->owner, AST_STATE_DOWN);
		c->owner = NULL;
	}

	/* this is in case we are destroying the session */
	if (c->state != SCCP_CHANNELSTATE_DOWN)
		sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_ONHOOK);

	sccp_rtp_stop(c);

	if (d) {
		/* deactive the active call if needed */
		sccp_device_lock(d);

		if (d->active_channel == c)
			d->active_channel = NULL;
		if (d->transfer_channel == c)
			d->transfer_channel = NULL;
		if (d->conference_channel == c)
			d->conference_channel = NULL;

		if ((x = sccp_device_find_selectedchannel(d, c))) {
			SCCP_LIST_LOCK(&d->selectedChannels);
			SCCP_LIST_REMOVE(&d->selectedChannels, x, list);
			SCCP_LIST_UNLOCK(&d->selectedChannels);
			ast_free(x);
		}
		sccp_device_unlock(d);
	}
}

/*!
 * \brief Destroy Channel
 * \param c SCCP Channel
 * \note We assume channel is locked
 *
 * \callgraph
 * \callergraph
 *
 * \warning
 * 	- line->channels is not always locked
 * 
 * \lock
 * 	- channel
 */
void sccp_channel_destroy_locked(sccp_channel_t * c)
{
	if (!c)
		return;

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Destroying channel %08x\n", c->callid);

	sccp_channel_unlock(c);
	sccp_mutex_destroy(&c->lock);
	ast_free(c);

	return;
}

/*!
 * \brief Handle Transfer Request (Pressing the Transfer Softkey)
 * \param c *locked* SCCP Channel
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 */
void sccp_channel_transfer_locked(sccp_channel_t * c)
{
	sccp_device_t *d;

	sccp_channel_t *newcall = NULL;

	uint32_t blindTransfer = 0;

	uint16_t instance;

	if (!c)
		return;

	if (!c->line || !c->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", c->callid);
		return;
	}

	d = c->device;

	if (!d->transfer || !c->line->transfer) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer disabled on device or line\n", (d && d->id) ? d->id : "SCCP");
		return;
	}

	sccp_device_lock(d);
	/* are we in the middle of a transfer? */
	/* TODO: It might be a bad idea to allow that a new transfer be initiated
	   by pressing transfer on the channel to be transferred twice. (-DD) */
	if (d->transfer_channel && (d->transfer_channel != c)) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: In the middle of a Transfer. Going to transfer completion\n", (d && d->id) ? d->id : "SCCP");
		sccp_device_unlock(d);
		sccp_channel_transfer_complete(c);
		return;
	}

	d->transfer_channel = c;
	sccp_device_unlock(d);
	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer request from line channel %s-%08X\n", (d && d->id) ? d->id : "SCCP", (c->line && c->line->name) ? c->line->name : "(null)", c->callid);

	if (!c->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_DEVICE | DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: No bridged channel to transfer on %s-%08X\n", (d && d->id) ? d->id : "SCCP", (c->line && c->line->name) ? c->line->name : "(null)", c->callid);
		instance = sccp_device_find_index_for_line(d, c->line->name);
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}
	/*  TODO: If this is the only place where the call is put on hold, the code should reflect the importance better. 
	   TODO: Add meaningful error reporting here. Check for missing cleanup. (-DD) */
	if ((c->state != SCCP_CHANNELSTATE_HOLD && c->state != SCCP_CHANNELSTATE_CALLTRANSFER) && !sccp_channel_hold_locked(c))
		return;
	if (c->state != SCCP_CHANNELSTATE_CALLTRANSFER)
		//sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_CALLTRANSFER);
		newcall = sccp_channel_newcall_locked(c->line, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
	/* set a var for BLINDTRANSFER. It will be removed if the user manually answer the call Otherwise it is a real BLINDTRANSFER */
	if (blindTransfer || (newcall && newcall->owner && c->owner && CS_AST_BRIDGED_CHANNEL(c->owner))) {
		pbx_builtin_setvar_helper(newcall->owner, "BLINDTRANSFER", CS_AST_BRIDGED_CHANNEL(c->owner)->name);
		pbx_builtin_setvar_helper(CS_AST_BRIDGED_CHANNEL(c->owner), "BLINDTRANSFER", newcall->owner->name);
	}
	sccp_channel_unlock(newcall);
}

/*!
 * \brief Handle Transfer Ringing Thread
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- asterisk channel
 */
static void *sccp_channel_transfer_ringing_thread(void *data)
{
	char *name = data;

	struct ast_channel *ast;

	if (!name)
		return NULL;

	sleep(1);
	ast = ast_get_channel_by_name_locked(name);
	ast_free(name);

	if (!ast)
		return NULL;

	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (Ringing within Transfer %s(%p)\n", ast->name, (void *)ast);
	if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_RING) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Send ringing indication to %s(%p)\n", ast->name, (void *)ast);
		ast_indicate(ast, AST_CONTROL_RINGING);
	} else if (GLOB(blindtransferindication) == SCCP_BLINDTRANSFER_MOH) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: (sccp_channel_transfer_ringing_thread) Started music on hold for channel %s(%p)\n", ast->name, (void *)ast);
		pbx_moh_start(ast, NULL, NULL);
	}
	pbx_channel_unlock(ast);
	return NULL;
}

/*!
 * \brief Bridge Two Channels
 * \param cDestinationLocal Local Destination SCCP Channel
 * \todo Find a way solve the chan->state problem
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- device
 */
void sccp_channel_transfer_complete(sccp_channel_t * cDestinationLocal)
{
	struct ast_channel *astcSourceRemote = NULL, *astcDestinationLocal = NULL, *astcDestinationRemote = NULL;

	sccp_channel_t *cSourceLocal;

	sccp_channel_t *cSourceRemote = NULL;

	sccp_channel_t *cDestinationRemote;

	sccp_device_t *d = NULL;

	pthread_attr_t attr;

	pthread_t t;

	uint16_t instance;

	if (!cDestinationLocal)
		return;

	if (!cDestinationLocal->line || !cDestinationLocal->device) {
		ast_log(LOG_WARNING, "SCCP: weird error. The channel has no line or device on channel %d\n", cDestinationLocal->callid);
		return;
	}
	// Obtain the device from which the transfer was initiated
	d = cDestinationLocal->device;
	sccp_device_lock(d);
	// Obtain the source channel on that device
	cSourceLocal = d->transfer_channel;
	sccp_device_unlock(d);

	sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Complete transfer from %s-%08X\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid);
	instance = sccp_device_find_index_for_line(d, cDestinationLocal->line->name);

	if (cDestinationLocal->state != SCCP_CHANNELSTATE_RINGOUT && cDestinationLocal->state != SCCP_CHANNELSTATE_CONNECTED) {
		ast_log(LOG_WARNING, "SCCP: Failed to complete transfer. The channel is not ringing or connected\n");
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, cDestinationLocal->callid, 0);
		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (!cDestinationLocal->owner || !cSourceLocal->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer error, asterisk channel error %s-%08X and %s-%08X\n", d->id, cDestinationLocal->line->name, cDestinationLocal->callid, cSourceLocal->line->name, cSourceLocal->callid);
		return;
	}

	astcSourceRemote = CS_AST_BRIDGED_CHANNEL(cSourceLocal->owner);
	astcDestinationRemote = CS_AST_BRIDGED_CHANNEL(cDestinationLocal->owner);
	astcDestinationLocal = cDestinationLocal->owner;

	if (!astcSourceRemote || !astcDestinationLocal) {
		ast_log(LOG_WARNING, "SCCP: Failed to complete transfer. Missing asterisk transferred or transferee channel\n");

		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
		ast_indicate(astcSourceRemote, AST_CONTROL_RINGING);		// Shouldn't this be ALERTING?
		/* starting the ringing thread */
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (ast_pthread_create(&t, &attr, sccp_channel_transfer_ringing_thread, strdup(astcSourceRemote->name))) {
			ast_log(LOG_WARNING, "%s: Unable to create thread for the blind transfer ring indication. %s\n", d->id, strerror(errno));
		}

		/* changing callerid for source part */
		cSourceRemote = CS_AST_CHANNEL_PVT(astcSourceRemote);
		if (cSourceRemote) {
			if (CS_AST_CHANNEL_PVT_IS_SCCP(astcSourceRemote)) {
				// SCCP CallerID Exchange
				if (cSourceLocal->calltype == SKINNY_CALLTYPE_INBOUND) {
					/* copy old callerid */
					sccp_copy_string(cSourceRemote->callInfo.originalCalledPartyName, cSourceRemote->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.originalCalledPartyName));
					sccp_copy_string(cSourceRemote->callInfo.originalCalledPartyNumber, cSourceRemote->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.originalCalledPartyNumber));

					sccp_copy_string(cSourceRemote->callInfo.calledPartyName, cDestinationLocal->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.calledPartyName));
					sccp_copy_string(cSourceRemote->callInfo.calledPartyNumber, cDestinationLocal->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.calledPartyNumber));

				} else if (cSourceLocal->calltype == SKINNY_CALLTYPE_OUTBOUND) {
					/* copy old callerid */
					sccp_copy_string(cSourceRemote->callInfo.originalCallingPartyName, cSourceRemote->callInfo.callingPartyName, sizeof(cSourceRemote->callInfo.originalCallingPartyName));
					sccp_copy_string(cSourceRemote->callInfo.originalCallingPartyNumber, cSourceRemote->callInfo.callingPartyNumber, sizeof(cSourceRemote->callInfo.originalCallingPartyNumber));

					sccp_copy_string(cSourceRemote->callInfo.callingPartyName, cDestinationLocal->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.callingPartyName));
					sccp_copy_string(cSourceRemote->callInfo.callingPartyNumber, cDestinationLocal->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.callingPartyNumber));
				}

				sccp_channel_send_callinfo(cSourceRemote->device, cSourceRemote);
			} else {
				// Other Tech->Type CallerID Exchange
				/*! \todo how about other types like SIP and IAX... How are we going to implement the callerid exchange for them. */
				sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Blind %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(astcSourceRemote));
			}
		}

	}
	if (ast_channel_masquerade(astcDestinationLocal, astcSourceRemote)) {
		ast_log(LOG_WARNING, "SCCP: Failed to masquerade %s into %s\n", astcDestinationLocal->name, astcSourceRemote->name);

		sccp_dev_displayprompt(d, instance, cDestinationLocal->callid, SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER, 5);
		return;
	}

	if (cDestinationLocal->state == SCCP_CHANNELSTATE_RINGOUT) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Blind transfer. Signalling ringing state to %s\n", d->id, astcSourceRemote->name);
	}

	if (!cSourceLocal->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Peer owner disappeared! Can't free ressources\n");
		return;
	}

	sccp_device_lock(d);
	d->transfer_channel = NULL;
	sccp_device_unlock(d);

	if (!astcDestinationRemote) {
		/* the channel was ringing not answered yet. BLIND TRANSFER */
// TEST
//              if(cDestinationLocal->rtp)
//                      sccp_channel_destroy_rtp(cDestinationLocal);
		return;
	}

	/* it's a SCCP channel destination on transfer */
	cDestinationRemote = CS_AST_CHANNEL_PVT(astcDestinationRemote);

	/* change callInfo on our destination */
	if (cDestinationRemote) {
		sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: Transfer for Channel Type %s\n", CS_AST_CHANNEL_PVT_TYPE(astcSourceRemote));

		if (CS_AST_CHANNEL_PVT_IS_SCCP(astcDestinationRemote)) {
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer confirmation destination on channel %s\n", d->id, astcDestinationRemote->name);

			/* change callInfo on destination part */
			sccp_copy_string(cDestinationRemote->callInfo.originalCallingPartyName, cDestinationLocal->callInfo.callingPartyName, sizeof(cDestinationRemote->callInfo.originalCallingPartyName));
			sccp_copy_string(cDestinationRemote->callInfo.originalCallingPartyNumber, cDestinationLocal->callInfo.callingPartyNumber, sizeof(cDestinationRemote->callInfo.originalCallingPartyNumber));

			if (cSourceLocal->calltype == SKINNY_CALLTYPE_INBOUND) {
				sccp_copy_string(cDestinationRemote->callInfo.callingPartyName, cSourceLocal->callInfo.callingPartyName, sizeof(cDestinationRemote->callInfo.originalCallingPartyName));
				sccp_copy_string(cDestinationRemote->callInfo.callingPartyNumber, cSourceLocal->callInfo.callingPartyNumber, sizeof(cDestinationRemote->callInfo.originalCallingPartyNumber));
			} else {
				sccp_copy_string(cDestinationRemote->callInfo.callingPartyName, cSourceLocal->callInfo.calledPartyName, sizeof(cDestinationRemote->callInfo.originalCallingPartyName));
				sccp_copy_string(cDestinationRemote->callInfo.callingPartyNumber, cSourceLocal->callInfo.calledPartyNumber, sizeof(cDestinationRemote->callInfo.originalCallingPartyNumber));
			}
			sccp_channel_send_callinfo(cDestinationRemote->device, cDestinationRemote);

		} else {
			ast_set_callerid(astcDestinationRemote, cSourceLocal->callInfo.callingPartyNumber, cSourceLocal->callInfo.callingPartyName, NULL);
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(astcDestinationRemote));
		}								// no sccp channel
	}

	/* change callInfo on our source */
	cSourceRemote = CS_AST_CHANNEL_PVT(astcSourceRemote);
	if (cSourceRemote) {
		if (CS_AST_CHANNEL_PVT_IS_SCCP(astcSourceRemote)) {		/* change callInfo on our source */
			sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Transfer confirmation source on channel %s\n", DEV_ID_LOG(d), astcSourceRemote->name);

			if (cSourceLocal->calltype == SKINNY_CALLTYPE_INBOUND) {
				/* copy old callerid */
				sccp_copy_string(cSourceRemote->callInfo.originalCalledPartyName, cSourceRemote->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.originalCalledPartyName));
				sccp_copy_string(cSourceRemote->callInfo.originalCalledPartyNumber, cSourceRemote->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.originalCalledPartyNumber));

				sccp_copy_string(cSourceRemote->callInfo.calledPartyName, cDestinationLocal->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.calledPartyName));
				sccp_copy_string(cSourceRemote->callInfo.calledPartyNumber, cDestinationLocal->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.calledPartyNumber));

				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set originalCalledPartyNumber %s, calledPartyNumber %s\n", DEV_ID_LOG(cSourceRemote->device), cSourceRemote->callInfo.originalCalledPartyNumber, cSourceRemote->callInfo.calledPartyNumber);

			} else if (cSourceLocal->calltype == SKINNY_CALLTYPE_OUTBOUND) {
				/* copy old callerid */
				sccp_copy_string(cSourceRemote->callInfo.originalCallingPartyName, cSourceRemote->callInfo.callingPartyName, sizeof(cSourceRemote->callInfo.originalCallingPartyName));
				sccp_copy_string(cSourceRemote->callInfo.originalCallingPartyNumber, cSourceRemote->callInfo.callingPartyNumber, sizeof(cSourceRemote->callInfo.originalCallingPartyNumber));

				sccp_copy_string(cSourceRemote->callInfo.callingPartyName, cDestinationLocal->callInfo.calledPartyName, sizeof(cSourceRemote->callInfo.callingPartyName));
				sccp_copy_string(cSourceRemote->callInfo.callingPartyNumber, cDestinationLocal->callInfo.calledPartyNumber, sizeof(cSourceRemote->callInfo.callingPartyNumber));

				sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: set originalCalledPartyNumber %s, calledPartyNumber %s\n", DEV_ID_LOG(cSourceRemote->device), cSourceRemote->callInfo.originalCalledPartyNumber, cSourceRemote->callInfo.calledPartyNumber);
			}

			sccp_channel_send_callinfo(cSourceRemote->device, cSourceRemote);
		} else {
			sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "SCCP: %s Transfer, callerid exchange need to be implemented\n", CS_AST_CHANNEL_PVT_TYPE(astcSourceRemote));
			if (CS_AST_CHANNEL_PVT_CMP_TYPE(astcSourceRemote, "SIP")) {
//                              sccp_log(DEBUGCAT_CHANNEL)(VERBOSE_PREFIX_3 "SCCP: %s Transfer, cid_num '%s', cid_name '%s'\n", CS_AST_CHANNEL_PVT_TYPE(astcSourceRemote), astcSourceRemote->cid_num, astcSourceRemote->cid_name);
				;
			}
		}								// if (CS_AST_CHANNEL_PVT_IS_SCCP(astcSourceRemote)) {
	}
	if (GLOB(transfer_tone) && cDestinationLocal->state == SCCP_CHANNELSTATE_CONNECTED) {
		/* while connected not all the tones can be played */
		sccp_dev_starttone(cDestinationLocal->device, GLOB(autoanswer_tone), instance, cDestinationLocal->callid, 0);
	}
}

/*!
 * \brief Forward a Channel
 * \param parent SCCP parent channel
 * \param lineDevice SCCP LineDevice
 * \param fwdNumber fwdNumber as char *
 *
 * \callgraph
 * \callergraph
 * 
 * \lock
 * 	- channel
 */
void sccp_channel_forward(sccp_channel_t * parent, sccp_linedevices_t * lineDevice, char *fwdNumber)
{
	sccp_channel_t *forwarder = NULL;

	char dialedNumber[256];

	if (!parent) {
		ast_log(LOG_ERROR, "We can not forward a call without parent channel\n");
		return;
	}

	sccp_copy_string(dialedNumber, fwdNumber, sizeof(dialedNumber));

	forwarder = sccp_channel_allocate_locked(parent->line, NULL);

	if (!forwarder) {
		ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel\n", lineDevice->device->id);
		return;
	}

	forwarder->parentChannel = parent;
	forwarder->ss_action = SCCP_SS_DIAL;					/* softswitch will catch the number to be dialed */
	forwarder->ss_data = 0;							// nothing to pass to action
	forwarder->calltype = SKINNY_CALLTYPE_OUTBOUND;

	/* copy the number to dial in the ast->exten */
	sccp_copy_string(forwarder->dialedNumber, dialedNumber, sizeof(forwarder->dialedNumber));
	sccp_log(DEBUGCAT_CHANNEL) (VERBOSE_PREFIX_3 "Incoming: %s Forwarded By: %s Forwarded To: %s", parent->callInfo.callingPartyName, lineDevice->line->cid_name, dialedNumber);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate_locked(forwarder)) {
		ast_log(LOG_WARNING, "%s: Unable to allocate a new channel for line %s\n", lineDevice->device->id, forwarder->line->name);
		sccp_line_removeChannel(forwarder->line, forwarder);
		sccp_channel_clean_locked(forwarder);
		sccp_channel_destroy_locked(forwarder);
		return;
	}

	/* setting callerid */
	char fwd_from_name[254];

	sprintf(fwd_from_name, "%s -> %s", lineDevice->line->cid_num, parent->callInfo.callingPartyName);

	//forwarder->owner->cid.cid_num = strdup(parent->callInfo.callingPartyNumber);
	if (PBX(pbx_set_callerid_number))
		PBX(pbx_set_callerid_number) (forwarder->owner, (const char *)&parent->callInfo.callingPartyNumber);

	//forwarder->owner->cid.cid_name = strdup(fwd_from_name);
	if (PBX(pbx_set_callerid_name))
		PBX(pbx_set_callerid_name) (forwarder->owner, (const char *)&fwd_from_name);


        // reverted to old behaviour, pbx_set_callerid_ani produces crash
//	forwarder->owner->cid.cid_ani = strdup(dialedNumber);
	if (PBX(pbx_set_callerid_ani))
		PBX(pbx_set_callerid_ani) (forwarder->owner, (const char *)&dialedNumber);

        // reverted to old behaviour, pbx_set_callerid_dnid produces crash
	forwarder->owner->cid.cid_dnid = strdup(dialedNumber);
//	if (PBX(pbx_set_callerid_dnid))
//		PBX(pbx_set_callerid_dnid) (forwarder->owner, (const char *)&dialedNumber);

        // reverted to old behaviour, pbx_set_callerid_rdnis produces crash
	forwarder->owner->cid.cid_rdnis = strdup(forwarder->line->cid_num);
//	if (PBX(pbx_set_callerid_rdnis))
//		PBX(pbx_set_callerid_rdnis) (forwarder->owner, (const char *)&forwarder->line->cid_num);

#ifdef CS_AST_CHANNEL_HAS_CID
	forwarder->owner->cid.cid_ani2 = -1;
#endif

	/* dial forwarder */
	sccp_copy_string(forwarder->owner->exten, dialedNumber, sizeof(forwarder->owner->exten));
	//sccp_ast_setstate(forwarder, AST_STATE_OFFHOOK);
	PBX(set_callstate) (forwarder, AST_STATE_OFFHOOK);
	if (!sccp_strlen_zero(dialedNumber) && !pbx_check_hangup(forwarder->owner)
	    && ast_exists_extension(forwarder->owner, forwarder->line->context, dialedNumber, 1, forwarder->line->cid_num)) {
		/* found an extension, let's dial it */
		ast_log(LOG_NOTICE, "%s: (sccp_channel_forward) channel %s-%08x is dialing number %s\n", "SCCP", forwarder->line->name, forwarder->callid, strdup(dialedNumber));
		/* Answer dialplan command works only when in RINGING OR RING ast_state */
		sccp_ast_setstate(forwarder, AST_STATE_RING);
		if (ast_pbx_start(forwarder->owner)) {
			ast_log(LOG_WARNING, "%s: invalide number\n", "SCCP");
		}
	}

	sccp_channel_unlock(forwarder);
}

#ifdef CS_SCCP_PARK

/*!
 * \brief Dual Structure
 */
struct sccp_dual {
	struct ast_channel *chan1;

	struct ast_channel *chan2;
};

/*!
 * \brief Channel Park Thread
 * \param stuff Stuff
 * \todo Some work to do i guess
 * \todo replace parameter stuff with something sensable
 */
static void *sccp_channel_park_thread(void *stuff)
{
	struct ast_channel *chan1, *chan2;

	struct sccp_dual *dual;

	struct ast_frame *f;

	int ext;

	int res;

	char extstr[20];

	sccp_channel_t *c;

	memset(&extstr, 0, sizeof(extstr));

	dual = stuff;
	chan1 = dual->chan1;
	chan2 = dual->chan2;
	ast_free(dual);
	f = ast_read(chan1);
	if (f)
		ast_frfree(f);
	res = ast_park_call(chan1, chan2, 0, &ext);
	if (!res) {
		extstr[0] = 128;
		extstr[1] = SKINNY_LBL_CALL_PARK_AT;
		sprintf(&extstr[2], " %d", ext);
		/* XXX this chan is perhaps destroyed when we are here. */
		c = CS_AST_CHANNEL_PVT(chan2);
		sccp_dev_displaynotify(c->device, extstr, 10);
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Parked channel %s on %d\n", DEV_ID_LOG(c->device), chan1->name, ext);
	}
	ast_hangup(chan2);
	return NULL;
}

/*!
 * \brief Park an SCCP Channel
 * \param c SCCP Channel
 */
void sccp_channel_park(sccp_channel_t * c)
{
	sccp_device_t *d;

	sccp_line_t *l;

	struct sccp_dual *dual;

	struct ast_channel *chan1m, *chan2m, *bridged;

	pthread_t th;

	uint16_t instance;

	if (!c)
		return;

	d = c->device;
	l = c->line;
	if (!d)
		return;

	if (!d->park) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Park disabled on device\n", d->id);
		return;
	}

	if (!c->owner) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk channel\n", d->id);
		return;
	}
	bridged = CS_AST_BRIDGED_CHANNEL(c->owner);
	if (!bridged) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Can't Park: no asterisk bridged channel\n", d->id);
		return;
	}
	sccp_channel_lock(c);
	sccp_indicate_locked(d, c, SCCP_CHANNELSTATE_CALLPARK);
	sccp_channel_unlock(c);

	boolean_t channelResult;

// #    if ASTERISK_VERSION_NUM < 10400
//      chan1m = ast_channel_alloc(0);
// #    else
//      chan1m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08X", l->name, c->callid);
// #    endif
	channelResult = PBX(alloc_pbxChannel) (c, (void **)&chan1m);
	if (!channelResult) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		return;
	}
// #    if ASTERISK_VERSION_NUM < 10400
//      chan2m = ast_channel_alloc(0);
// #    else
//      chan2m = ast_channel_alloc(0, AST_STATE_DOWN, l->cid_num, l->cid_name, l->accountcode, c->dialedNumber, l->context, l->amaflags, "SCCP/%s-%08X", l->name, c->callid);
// #    endif
	channelResult = PBX(alloc_pbxChannel) (c, (void **)&chan2m);
	if (!channelResult) {
		sccp_log((DEBUGCAT_CHANNEL | DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Park Failed: can't create asterisk channel\n", d->id);

		instance = sccp_device_find_index_for_line(c->device, c->line->name);
		sccp_dev_displayprompt(c->device, instance, c->callid, SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE, 0);
		ast_hangup(chan1m);
		return;
	}
#    ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan1m, name, "Parking/%s", bridged->name);
#    else
	snprintf(chan1m->name, sizeof(chan1m->name), "Parking/%s", bridged->name);
#    endif
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
#    ifdef CS_AST_HAS_AST_STRING_FIELD
	ast_string_field_build(chan2m, name, "SCCPParking/%s", c->owner->name);
#    else
	snprintf(chan2m->name, sizeof(chan2m->name), "SCCPParking/%s", c->owner->name);
#    endif

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

	dual = ast_malloc(sizeof(struct sccp_dual));
	if (dual) {
		/* XXX at this point, perhaps it's not a good idea to store channel objects
		 * in this struct because they can be destroyed between this thread creation
		 * and the call of sccp_channel_park_thread() function (if we are really
		 * not lucky. */
		memset(dual, 0, sizeof(struct sccp_dual));
		dual->chan1 = chan1m;
		dual->chan2 = chan2m;
		if (!ast_pthread_create(&th, NULL, sccp_channel_park_thread, dual))
			return;
		ast_free(dual);
	}
}
#endif
