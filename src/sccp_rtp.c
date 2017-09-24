/*!
 * \file        sccp_rtp.c
 * \brief       SCCP RTP Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */

/*!
 * =============================
 * Example Networks
 * =============================
 * tokyo 200.0.0.254 / 172.20.0.0
 * 7905:172.20.0.5
 *
 * havana 150.0.0.254 / 192.168.0.0
 * 7970:192.168.0.5
 *
 * amsterdam 100.0.0.254 / 10.10.0.0 (IP-Forward) (NoNat)
 * (PBX):100.0.0.1 & 10.10.0.1)
 * 7941: 10.10.0.5
 * 7942: 10.10.0.6
 *
 * berlin 80.0.0.254 / 10.20.0.0 (Port Forward) (Nat) sccp.conf needs externip
 * (PBX): 10.20.0.1
 * 7941: 10.20.0.5
 *
 * ====================================
 * Example Calls
 * ====================================
 * amsterdam -> amsterdam via amsterdam : inDirectRTP
 * 172.20.0.5 ->                                           10.0.0.1:PBX:10.0.0.1                                           -> 10.0.0.6
 * leg1:                                                    us           them
 * leg2:                                                   them           us
 *
 * amsterdam -> amsterdam via amsterdam : DirectRTP
 * 172.20.0.5 ->                                           10.0.0.1:PBX:10.0.0.1                                           -> 10.0.0.6
 * leg1:us                                                                                                                      them
 * leg2:them                                                                                                                     us
 *
 * tokyo -> amsterdam via amsterdam (Single NAT : IP-Forward) inDirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet ->               100.0.0.1:PBX:10.0.0.1                                           -> 10.0.0.5
 * leg1:              us                                                  them
 * leg2:                                                     them                                                                us
 *
 * tokyo -> amsterdam via amsterdam (Single NAT : IP-Forward) DirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet ->               100.0.0.1:PBX:10.0.0.1                                           -> 10.0.0.5
 * leg1:              us                                                                                                        them
 * leg2:             them                                                                                                        us
 *
 * tokyo -> havana via amsterdam (Single NAT : IP-Forward) inDirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet ->               100.0.0.1:PBX:100.0.0.1               -> Internet -> 150.0.0.254 -> 192.168.0.5
 * leg1:              us                                                  them
 * leg2:                                                    them                                                   us
 *
 * tokyo -> havana via amsterdam (Single NAT : IP-Forward) DirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet ->               100.0.0.1:PBX:100.0.0.1               -> Internet -> 150.0.0.254 -> 192.168.0.5
 * leg1:              us                                                                                          them
 * leg2:             them                                                                                          us
 *
 * tokyo -> havana via berlin (Double Nat : Port-Forward on PBX Location) inDirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet -> 80.0.0.254 -> 10.20.0.1:PBX:10.20.0.1 -> 80.0.0.254 -> Internet -> 150.0.0.254 -> 192.168.0.5
 * leg1:              us                                                               them
 * leg2:                                       them                                                                us
 *
 * tokyo -> havana via berlin (Double Nat : Port-Forward on PBX Location) DirectRTP
 * 172.20.0.5 -> 200.0.0.254 -> Internet -> 80.0.0.254 -> 10.20.0.1:PBX:10.20.0.1 -> 80.0.0.254 -> Internet -> 150.0.0.254 -> 192.168.0.5
 * leg1:              us                                                                                          them
 * leg2:             them                                                                                          us
 *
 * ====================================
 * How to name the addresses
 * ====================================
 * 172.20.0.5 / 192.168.0.5 = phone  			= physicalIP	=> (and phone_remote for the phone on the other side of the channel) This information does not get send to the pbx
 * 200.0.0.254 / 150.0.0.254 = d->session->sin		= reachableVia	=> (can be equal to physicalIP), remote ip-address + port of the connection (gotten from physicalIP)
									=> written to phone(us) during openreceivechannel
									=> written to phone_remote(them) during startmediatransmission
 * 100.0.0.254 / 100.0.0.254 = externalip		= rtp->remote	=> only required when double nat
 * 100.0.0.1 / 100.0.0.1 = d->session->ourip		= rtp->remote	=> local ip-address + port of the phone's connection
 * 10.0.0.1 =  d->session->ourip			= rtp->remote	=> local ip-address + port of the phone's connection
 */

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_rtp.h"
#include "sccp_session.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \brief create a new rtp server
 * \todo refactor iPbx.rtp_???_server to include sccp_rtp_type_t
 */
boolean_t sccp_rtp_createServer(constDevicePtr d, channelPtr c, sccp_rtp_type_t type)
{
	boolean_t rtpResult = FALSE;
	sccp_rtp_t *rtp = NULL;

	if (!c || !d) {
		return FALSE;
	}

	switch(type) {
		case SCCP_RTP_AUDIO:
			rtp = &(c->rtp.audio);
			break;
#if CS_SCCP_VIDEO
		case SCCP_RTP_VIDEO:
			rtp = &(c->rtp.video);
			break;
#endif
		default:
			pbx_log(LOG_ERROR, "%s: (sccp_rtp_createRTPServer) unknown/unhandled rtp type, cancelling\n", c->designator);
			return FALSE;
	}

	if (rtp->instance) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_3 "%s: we already have a %s server, we use this one\n", c->designator, sccp_rtp_type2str(type));
		return TRUE;
	}
	rtp->type = type;

	if (iPbx.rtp_create_instance) {
		rtpResult = iPbx.rtp_create_instance(d, c, rtp);
	} else {
		pbx_log(LOG_ERROR, "we should start our own rtp server, but we don't have one\n");
		return FALSE;
	}
	struct sockaddr_storage *phone_remote = &rtp->phone_remote;

	if (!sccp_rtp_getUs(rtp, phone_remote)) {
		pbx_log(LOG_WARNING, "%s: Did not get our rtp part\n", c->currentDeviceId);
		return FALSE;
	}

	uint16_t port = sccp_rtp_getServerPort(rtp);
	sccp_session_getOurIP(d->session, phone_remote, 0);
	sccp_netsock_setPort(phone_remote, port);

	if (ast_test_flag(GLOB(global_jbconf), AST_JB_ENABLED)) {
		if (ast_test_flag(GLOB(global_jbconf), AST_JB_FORCED) || !d->directrtp) {
			pbx_jb_configure(c->owner, GLOB(global_jbconf));
		}
	}

	char buf[NI_MAXHOST + NI_MAXSERV];
	sccp_copy_string(buf, sccp_netsock_stringify(phone_remote), sizeof(buf));
	boolean_t isMappedIPv4 = sccp_netsock_ipv4_mapped(phone_remote, phone_remote);
	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (createAudioServer) updated phone %s destination to : %s, family:%s, mapped: %s\n", c->designator, sccp_rtp_type2str(type), buf, sccp_netsock_is_IPv4(phone_remote) ? "IPv4" : "IPv6", isMappedIPv4 ? "True" : "False");

	return rtpResult;
}

/*!
 * \brief request the port to be used for RTP, early on, so that we can use it during bridging, even before open_receive_ack has been received (directrtp)
 */
int sccp_rtp_requestRTPPorts(constDevicePtr device, channelPtr channel)
{
	pbx_assert(device != NULL && channel != NULL);

	sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (requestRTPPort) request rtp port from phone\n", device->id);
	device->protocol->sendPortRequest(device, channel, SKINNY_MEDIA_TRANSPORT_TYPE_RTP, SKINNY_MEDIA_TYPE_AUDIO);

#ifdef CS_SCCP_VIDEO
 	if (sccp_device_isVideoSupported(device) && channel->videomode != SCCP_VIDEO_MODE_OFF) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (requestRTPPort) request vrtp port from phone\n", device->id);
		if (channel->rtp.video.instance || sccp_rtp_createServer(device, channel, SCCP_RTP_VIDEO)) {
			device->protocol->sendPortRequest(device, channel, SKINNY_MEDIA_TRANSPORT_TYPE_RTP, SKINNY_MEDIA_TYPE_MAIN_VIDEO);
		}
	}
#endif
	return 1;
}

/*!
 * \brief Stop an RTP Source.
 * \param channel SCCP Channel
 */
void sccp_rtp_stop(constChannelPtr channel)
{
	if (!channel) {
		return;
	}
	if (iPbx.rtp_stop) {
		if (channel->rtp.audio.instance) {
			PBX_RTP_TYPE *rtp = (PBX_RTP_TYPE *) channel->rtp.audio.instance;		/* discard const */
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "%s: Stopping PBX audio rtp transmission on channel %s\n", channel->currentDeviceId, channel->designator);
			iPbx.rtp_stop(rtp);
		}
		if (channel->rtp.video.instance) {
			PBX_RTP_TYPE *rtp = (PBX_RTP_TYPE *) channel->rtp.video.instance;		/* discard const */
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_4 "%s: Stopping PBX video rtp transmission on channel %s\n", channel->currentDeviceId, channel->designator);
			iPbx.rtp_stop(rtp);
		}
	} else {
		pbx_log(LOG_ERROR, "no pbx function to stop rtp\n");
	}
}

/*!
 * \brief Destroy RTP Source.
 * \param c SCCP Channel
 */
void sccp_rtp_destroy(constChannelPtr c)
{
	sccp_rtp_t *audio = (sccp_rtp_t *) &(c->rtp.audio);
	sccp_rtp_t *video = (sccp_rtp_t *) &(c->rtp.video);

	if (audio->instance) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX rtp server on channel %s\n", c->currentDeviceId, c->designator);
		iPbx.rtp_destroy(audio->instance);
		audio->instance = NULL;
	}

	if (video->instance) {
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: destroying PBX vrtp server on channel %s\n", c->currentDeviceId, c->designator);
		iPbx.rtp_destroy(video->instance);
		video->instance = NULL;
	}
}

/*!
 * \brief update the phones destination address (it's peer)
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_peer(constChannelPtr c, sccp_rtp_t * const rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_netsock_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: ( sccp_rtp_set_peer ) remote information are invalid, don't change anything\n", c->currentDeviceId);
		return;
	}

	/* check if we have new information, which requires us to update */
	if (sccp_netsock_equals(new_peer, &rtp->phone_remote)) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) remote information is equal to the current info, ignore change\n", c->currentDeviceId);
		return;
	}

	memcpy(&rtp->phone_remote, new_peer, sizeof rtp->phone_remote);
	pbx_log(LOG_NOTICE, "%s: ( sccp_rtp_set_peer ) Set new remote address to %s\n", c->currentDeviceId, sccp_netsock_stringify(&rtp->phone_remote));

	if (rtp->mediaTransmissionState != SCCP_RTP_STATUS_INACTIVE) {
		/* Shutdown any early-media or previous media on re-invite */
		/*! \todo we should wait for the acknowledgement to get back. We don't have a function/procedure in place to do this at this moment in time (sccp_dev_send_wait) */
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_peer) Restart media transmission on channel %d\n", c->currentDeviceId, c->callid);

		/*! \todo we should check if this is a video or audio rtp */
		sccp_channel_updateMediaTransmission(c);
	}
}

/*!
 * \brief update phones source rtp address
 * \param c SCCP Channel
 * \param rtp SCCP RTP
 * \param new_peer socket info to remote device
 */
void sccp_rtp_set_phone(constChannelPtr c, sccp_rtp_t * const rtp, struct sockaddr_storage *new_peer)
{
	/* validate socket */
	if (sccp_netsock_getPort(new_peer) == 0) {
		sccp_log((DEBUGCAT_RTP)) (VERBOSE_PREFIX_2 "%s: (sccp_rtp_set_phone) remote information are invalid, don't change anything\n", c->currentDeviceId);
		return;
	}

	AUTO_RELEASE(sccp_device_t, device , sccp_channel_getDevice(c));

	if (device) {
		/* check if we have new infos */
		char peerIpStr[NI_MAXHOST + NI_MAXSERV];
		char remoteIpStr[NI_MAXHOST + NI_MAXSERV];
		char phoneIpStr[NI_MAXHOST + NI_MAXSERV];
		if (device->nat >= SCCP_NAT_ON) {
			/* Rewrite ip-addres to the outside source address using the phones connection (device->sin) */
			sccp_copy_string(peerIpStr, sccp_netsock_stringify(new_peer), sizeof(peerIpStr));
			uint16_t port = sccp_netsock_getPort(new_peer);
			sccp_session_getSas(device->session, new_peer);
			sccp_netsock_ipv4_mapped(new_peer, new_peer);
			sccp_netsock_setPort(new_peer, port);
		}

		memcpy(&rtp->phone, new_peer, sizeof(rtp->phone));
		if (iPbx.rtp_setPhoneAddress && rtp->instance) {
			iPbx.rtp_setPhoneAddress(rtp, new_peer, device->nat >= SCCP_NAT_ON ? 1 : 0);

			sccp_copy_string(remoteIpStr, sccp_netsock_stringify(&rtp->phone_remote), sizeof(remoteIpStr));
			sccp_copy_string(phoneIpStr, sccp_netsock_stringify(&rtp->phone), sizeof(phoneIpStr));
			if (device->nat >= SCCP_NAT_ON) {
				sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell %s  to send RTP/UDP media from %s to %s (NAT:%s)\n", DEV_ID_LOG(device), device->directrtp ? "Peer" : "PBX ", remoteIpStr, phoneIpStr, peerIpStr);
			} else {
				sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: Tell %s  to send RTP/UDP media from %s to %s (NoNat)\n", DEV_ID_LOG(device), device->directrtp ? "Peer" : "PBX ", remoteIpStr, phoneIpStr);
			}
		}

	}
}

/*! \todo move the refreshing of the hostname->ip-address to another location (for example scheduler) to re-enable dns hostname lookup */
int sccp_rtp_updateNatRemotePhone(constChannelPtr c, sccp_rtp_t *const rtp)
{
	int res = 0;
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));
	if (d) {
		struct sockaddr_storage sus = { 0 };
		struct sockaddr_storage *phone_remote = &rtp->phone_remote;
		sccp_session_getOurIP(d->session, &sus, 0);

		uint16_t usFamily = (sccp_netsock_is_IPv6(&sus) && !sccp_netsock_is_mapped_IPv4(&sus)) ? AF_INET6 : AF_INET;
		uint16_t remoteFamily = (sccp_netsock_is_IPv6(phone_remote) && !sccp_netsock_is_mapped_IPv4(phone_remote)) ? AF_INET6 : AF_INET;

		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: checkNat us: %s, usFamily: %s\n", d->id, sccp_netsock_stringify(&sus), (usFamily == AF_INET6) ? "IPv6" : "IPv4");
		sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: checkNat remote: %s, remoteFamily: %s\n", d->id, sccp_netsock_stringify(phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");
		if (d->nat >= SCCP_NAT_ON) {
			uint16_t port = sccp_rtp_getServerPort(rtp);					// get rtp server port
			if (!sccp_netsock_getExternalAddr(phone_remote, remoteFamily)) {		// get externip/externhost ip-address (PBX behind NAT Firewall)
				sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_2 "%s: no externip/externhost set, falling back to using incoming interface address:%s\n", d->id, sccp_netsock_stringify(&sus));
				memcpy(phone_remote, &sus, sizeof(struct sockaddr_storage));
			}
			if (usFamily != remoteFamily) {
				sccp_netsock_ipv4_mapped(phone_remote, phone_remote);			// we need this to convert mapped IPv4 to real IPv4 address
			}
			sccp_netsock_setPort(phone_remote, port);
			sccp_log(DEBUGCAT_RTP) (VERBOSE_PREFIX_3 "%s: (updateNatRemotePhone) new remote: %s, new remoteFamily: %s\n", d->id, sccp_netsock_stringify(phone_remote), (remoteFamily == AF_INET6) ? "IPv6" : "IPv4");
			res = 1;
		}
	}
	return res;
}


/*!
 * \brief Get Audio Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getAudioPeerInfo(constChannelPtr c, sccp_rtp_t **rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	AUTO_RELEASE(sccp_device_t, device , sccp_channel_getDevice(c));

	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.audio);

	result = SCCP_RTP_INFO_AVAILABLE;
	// \todo add apply_ha(d->ha, &sin) check here instead
	if (device->directrtp && device->nat <= SCCP_NAT_AUTO_OFF && !c->conference) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}
	return result;
}

#ifdef CS_SCCP_VIDEO
/*!
 * \brief Get Video Peer RTP Information
 */
sccp_rtp_info_t sccp_rtp_getVideoPeerInfo(constChannelPtr c, sccp_rtp_t ** rtp)
{
	sccp_rtp_info_t result = SCCP_RTP_INFO_NORTP;

	AUTO_RELEASE(sccp_device_t, device , sccp_channel_getDevice(c));

	if (!device) {
		return SCCP_RTP_INFO_NORTP;
	}

	*rtp = &(((sccp_channel_t *) c)->rtp.video);

	result = SCCP_RTP_INFO_AVAILABLE;
	if (device->directrtp && device->nat <= SCCP_NAT_AUTO_OFF && !c->conference) {
		result |= SCCP_RTP_INFO_ALLOW_DIRECTRTP;
	}
	return result;
}

/*!
 * \brief Get Video Peer
 */
boolean_t sccp_rtp_getVideoPeer(constChannelPtr c, struct sockaddr_storage **new_peer)
{
	sccp_rtp_t *video = (sccp_rtp_t *) &(c->rtp.video);
	*new_peer = &video->phone_remote;
	return TRUE;
}
#endif

/*!
 * \brief Get Audio Peer
 */
boolean_t sccp_rtp_getAudioPeer(constChannelPtr c, struct sockaddr_storage **new_peer)
{
	sccp_rtp_t *audio = (sccp_rtp_t *) &(c->rtp.audio);
	*new_peer = &audio->phone_remote;
	return TRUE;
}

/*!
 * \brief Get Payload Type
 */
uint8_t sccp_rtp_get_payloadType(const sccp_rtp_t * const rtp, skinny_codec_t codec)
{
	if (iPbx.rtp_get_payloadType) {
		return iPbx.rtp_get_payloadType(rtp, codec);
	} 
	return 97;
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getUs(const sccp_rtp_t *rtp, struct sockaddr_storage *us)
{
	if (rtp->instance) {
		iPbx.rtp_getUs(rtp->instance, us);
		return TRUE;
	} 
	// us = &rtp->phone_remote;
	return FALSE;
}

uint16_t sccp_rtp_getServerPort(const sccp_rtp_t * const rtp)
{
	uint16_t port = 0;
	struct sockaddr_storage sas;

	sccp_rtp_getUs(rtp, &sas);

	port = sccp_netsock_getPort(&sas);
	return port;
}

/*!
 * \brief Retrieve Phone Socket Information
 */
boolean_t sccp_rtp_getPeer(const sccp_rtp_t * const rtp, struct sockaddr_storage *them)
{
	if (rtp->instance) {
		iPbx.rtp_getPeer(rtp->instance, them);
		return TRUE;
	} 
	return FALSE;
}

/*!
 * \brief Get Sample Rate
 */
int sccp_rtp_get_sampleRate(skinny_codec_t codec)
{
	if (iPbx.rtp_get_sampleRate) {
		return iPbx.rtp_get_sampleRate(codec);
	} 
	return 3840;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
