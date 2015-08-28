/*!
 * \file        sccp_channel.h
 * \brief       SCCP Channel Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_CHANNEL_H
#define __SCCP_CHANNEL_H

#define sccp_channel_retain(_x) 	({ast_assert(_x != NULL);sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_channel_release(_x) 	({ast_assert(_x != NULL);sccp_refcount_release(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);})
#define sccp_channel_refreplace(_x, _y)	sccp_refcount_replace((void **)&_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__)

/*!
 * \brief SCCP CallInfo Structure
 */
struct sccp_callinfo {
	char calledPartyName[StationMaxNameSize];								/*!< Called Party Name */
	char calledPartyNumber[StationMaxDirnumSize];								/*!< Called Party Number */
	char cdpnVoiceMailbox[StationMaxDirnumSize];								/*!< Called Party Voicemail Box */

	char callingPartyName[StationMaxNameSize];								/*!< Calling Party Name */
	char callingPartyNumber[StationMaxDirnumSize];								/*!< Calling Party Number */
	char cgpnVoiceMailbox[StationMaxDirnumSize];								/*!< Calling Party Voicemail Box */

	char originalCalledPartyName[StationMaxNameSize];							/*!< Original Calling Party Name */
	char originalCalledPartyNumber[StationMaxDirnumSize];							/*!< Original Calling Party ID */
	char originalCdpnVoiceMailbox[StationMaxDirnumSize];							/*!< Original Called Party VoiceMail Box */

	char originalCallingPartyName[StationMaxNameSize];							/*!< Original Calling Party Name */
	char originalCallingPartyNumber[StationMaxDirnumSize];							/*!< Original Calling Party ID */

	char lastRedirectingPartyName[StationMaxNameSize];							/*!< Original Called Party Name */
	char lastRedirectingPartyNumber[StationMaxDirnumSize];							/*!< Original Called Party ID */
	char lastRedirectingVoiceMailbox[StationMaxDirnumSize];							/*!< Last Redirecting VoiceMail Box */

	uint32_t originalCdpnRedirectReason;									/*!< Original Called Party Redirect Reason */
	uint32_t lastRedirectingReason;										/*!< Last Redirecting Reason */
	int presentation;											/*!< Should this callerinfo be shown (privacy) */

	unsigned int cdpnVoiceMailbox_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int calledParty_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int cgpnVoiceMailbox_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int callingParty_valid:1;									/*!< TRUE if the name information is valid/present */
	unsigned int originalCdpnVoiceMailbox_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int originalCalledParty_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int originalCallingParty_valid:1;								/*!< TRUE if the name information is valid/present */
	unsigned int lastRedirectingVoiceMailbox_valid:1;							/*!< TRUE if the name information is valid/present */
	unsigned int lastRedirectingParty_valid:1;								/*!< TRUE if the name information is valid/present */
};														/*!< SCCP CallInfo Structure */

/*!
 * \brief SCCP Channel Structure
 * \note This contains the current channel information
 */
struct sccp_channel {
	uint32_t callid;											/*!< Call ID */
	uint32_t passthrupartyid;										/*!< Pass Through ID */
	sccp_channelstate_t state;										/*!< Internal channel state SCCP_CHANNELSTATE_* */
	sccp_channelstate_t previousChannelState;								/*!< Previous channel state SCCP_CHANNELSTATE_* */
	sccp_channelstatereason_t channelStateReason;								/*!< Reason the new/current state was set (for example to handle HOLD differently for transfer then normal) */
	skinny_calltype_t calltype;										/*!< Skinny Call Type as SKINNY_CALLTYPE_* */
	
	PBX_CHANNEL_TYPE *owner;										/*!< Asterisk Channel Owner */
	sccp_line_t *line;											/*!< SCCP Line */
	SCCP_LIST_ENTRY (sccp_channel_t) list;									/*!< Channel Linked List */
	char dialedNumber[SCCP_MAX_EXTENSION];									/*!< Last Dialed Number */
	char designator[CHANNEL_DESIGNATOR_SIZE];
	sccp_subscription_id_t subscriptionId;
	boolean_t answered_elsewhere;										/*!< Answered Elsewhere */
	boolean_t privacy;											/*!< Private */

#if DEBUG
	sccp_device_t *(*getDevice_retained) (const sccp_channel_t * channel, const char *filename, int lineno, const char *func);	/*!< temporary function to retrieve refcounted device */
#else
	sccp_device_t *(*getDevice_retained) (const sccp_channel_t * channel);					/*!< temporary function to retrieve refcounted device */
#endif
	void (*setDevice) (sccp_channel_t * channel, const sccp_device_t * device);				/*!< set refcounted device connected to the channel */
	char currentDeviceId[StationMaxDeviceNameSize];								/*!< Returns a constant char of the Device Id if available */

	sccp_private_channel_data_t *privateData;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< our channel Capability in preference order */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];
	} capabilities;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Preferences */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Preferences */
	} preferences;

	struct {
		skinny_codec_t audio[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Audio Codec Capabilities */
		skinny_codec_t video[SKINNY_MAX_CAPABILITIES];							/*!< SCCP Video Codec Capabilities */
	} remoteCapabilities;

	struct {
		uint32_t digittimeout;										/*!< Digit Timeout on Dialing State (Enbloc-Emu) */
		boolean_t deactivate;										/*!< Deactivate Enbloc-Emulation (Time Deviation Found) */
		uint32_t totaldigittime;									/*!< Total Time used to enter Number (Enbloc-Emu) */
		uint32_t totaldigittimesquared;									/*!< Total Time Squared used to enter Number (Enbloc-Emu) */
	} enbloc;

	struct {
#ifndef SCCP_ATOMIC
		sccp_mutex_t lock;
#endif
		volatile CAS32_TYPE deny;
		int digittimeout;										/*!< Schedule for Timeout on Dialing State */
		int hangup;											/*!< Automatic hangup after invalid/congested indication */
	} scheduler;

	sccp_dtmfmode_t dtmfmode;										/*!< DTMF Mode (0 inband - 1 outofband) */
	struct {
		sccp_rtp_t audio;										/*!< Asterisk RTP */
		sccp_rtp_t video;										/*!< Video RTP session */
		//sccp_rtp_t text;										/*!< Video RTP session */
		uint8_t peer_dtmfmode;
	} rtp;

	uint16_t ringermode;											/*!< Ringer Mode */
	uint16_t autoanswer_cause;										/*!< Auto Answer Cause */
	sccp_autoanswer_t autoanswer_type;									/*!< Auto Answer Type */

	/* don't allow sccp phones to monitor (hint) this call */
	sccp_softswitch_t softswitch_action;									/*!< Simple Switch Action. This is used in dial thread to collect numbers for callforward, pickup and so on -FS */
	uint16_t ss_data;											/*!< Simple Switch Integer param */
	uint16_t subscribers;											/*!< Used to determine if a sharedline should be hungup immediately, if everybody declined the call */

	sccp_channel_t *parentChannel;										/*!< if we are a cfwd channel, our parent is this */

	sccp_conference_t *conference;										/*!< are we part of a conference? */ /*! \todo to be removed instead of conference_id */
	uint32_t conference_id;											/*!< Conference ID (might be safer to use instead of conference) */
	uint32_t conference_participant_id;									/*!< Conference Participant ID */

	int32_t maxBitRate;
	boolean_t peerIsSCCP;											/*!< Indicates that channel-peer is also SCCP */
	void (*setMicrophone) (sccp_channel_t * channel, boolean_t on);
	boolean_t (*hangupRequest) (sccp_channel_t * channel);
	boolean_t (*isMicrophoneEnabled) (void);

	char *musicclass;											/*!< Music Class */

	sccp_callinfo_t callInfo;
	sccp_video_mode_t videomode;										/*!< Video Mode (0 off - 1 user - 2 auto) */

#if ASTERISK_VERSION_GROUP >= 111
	int16_t pbx_callid_created;
#endif
};														/*!< SCCP Channel Structure */

/*!
 * \brief SCCP Currently Selected Channel Structure
 */
struct sccp_selectedchannel {
	sccp_channel_t *channel;										/*!< SCCP Channel */
	SCCP_LIST_ENTRY (sccp_selectedchannel_t) list;								/*!< Selected Channel Linked List Entry */
};														/*!< SCCP Selected Channel Structure */
/* live cycle */
sccp_channel_t *sccp_channel_allocate(sccp_line_t * l, sccp_device_t * device);					// device is optional
sccp_channel_t *sccp_channel_newcall(sccp_line_t * l, sccp_device_t * device, const char *dial, uint8_t calltype, PBX_CHANNEL_TYPE * parentChannel, const void *ids);

void sccp_channel_updateChannelDesignator(sccp_channel_t * c);
void sccp_channel_updateMusicClass(sccp_channel_t * c, const sccp_line_t *l);
void sccp_channel_updateChannelCapability(sccp_channel_t * channel);
void sccp_channel_send_callinfo(const sccp_device_t * device, const sccp_channel_t * c);
void sccp_channel_send_callinfo2(sccp_channel_t * c);
void sccp_channel_setChannelstate(sccp_channel_t * channel, sccp_channelstate_t state);
void sccp_channel_display_callInfo(sccp_channel_t * channel);
void sccp_channel_set_callingparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_set_calledparty(sccp_channel_t * c, char *name, char *number);
boolean_t sccp_channel_set_originalCallingparty(sccp_channel_t * channel, char *name, char *number);
boolean_t sccp_channel_set_originalCalledparty(sccp_channel_t * c, char *name, char *number);
void sccp_channel_reset_calleridPresenceParameter(sccp_channel_t * c);
void sccp_channel_set_calleridPresenceParameter(sccp_channel_t * c, sccp_calleridpresence_t presenceParameter);
void sccp_channel_connect(sccp_channel_t * c);
void sccp_channel_disconnect(sccp_channel_t * c);

void sccp_channel_openReceiveChannel(sccp_channel_t * c);
void sccp_channel_closeReceiveChannel(sccp_channel_t * c, boolean_t KeepPortOpen);
void sccp_channel_updateReceiveChannel(sccp_channel_t * c);
void sccp_channel_openMultiMediaReceiveChannel(sccp_channel_t * channel);
void sccp_channel_closeMultiMediaReceiveChannel(sccp_channel_t * channel, boolean_t KeepPortOpen);
void sccp_channel_updateMultiMediaReceiveChannel(sccp_channel_t * channel);

void sccp_channel_startMediaTransmission(sccp_channel_t * c);
void sccp_channel_stopMediaTransmission(sccp_channel_t * c, boolean_t KeepPortOpen);
void sccp_channel_updateMediaTransmission(sccp_channel_t * channel);
void sccp_channel_startMultiMediaTransmission(sccp_channel_t * channel);
void sccp_channel_stopMultiMediaTransmission(sccp_channel_t * channel, boolean_t KeepPortOpen);
void sccp_channel_updateMultiMediaTransmission(sccp_channel_t * channel);

void sccp_channel_closeAllMediaTransmitAndReceive(sccp_device_t * d, sccp_channel_t * channel);

boolean_t sccp_channel_transfer_on_hangup(sccp_channel_t * channel);
gcc_inline void sccp_channel_stop_schedule_digittimout(sccp_channel_t * channel);
gcc_inline void sccp_channel_schedule_hangup(sccp_channel_t * channel, uint timeout);
gcc_inline void sccp_channel_schedule_digittimout(sccp_channel_t * channel, uint timeout);
void sccp_channel_end_forwarding_channel(sccp_channel_t * channel);
void sccp_channel_endcall(sccp_channel_t * c);
void sccp_channel_StatisticsRequest(sccp_channel_t * c);
void sccp_channel_answer(const sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_stop_and_deny_scheduled_tasks(sccp_channel_t * channel);
void sccp_channel_clean(sccp_channel_t * c);
void sccp_channel_transfer(sccp_channel_t * c, sccp_device_t * device);
void sccp_channel_transfer_release(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_transfer_cancel(sccp_device_t * d, sccp_channel_t * c);
void sccp_channel_transfer_complete(sccp_channel_t * c);
int sccp_channel_hold(sccp_channel_t * c);
int sccp_channel_resume(sccp_device_t * device, sccp_channel_t * c, boolean_t swap_channels);
int sccp_channel_forward(sccp_channel_t * parent, sccp_linedevices_t * lineDevice, char *fwdNumber);

#if DEBUG
#define sccp_channel_getDevice_retained(_x) __sccp_channel_getDevice_retained(_x, __FILE__,__LINE__,__PRETTY_FUNCTION__)
sccp_device_t *__sccp_channel_getDevice_retained(const sccp_channel_t * channel, const char *filename, int lineno, const char *func);
#else
sccp_device_t *sccp_channel_getDevice_retained(const sccp_channel_t * channel);
#endif
void sccp_channel_setDevice(sccp_channel_t * channel, const sccp_device_t * device);
const char *sccp_channel_device_id(const sccp_channel_t * channel);

#ifdef CS_SCCP_PARK
void sccp_channel_park(sccp_channel_t * c);
#endif

boolean_t sccp_channel_setPreferredCodec(sccp_channel_t * c, const void *data);
boolean_t sccp_channel_setVideoMode(sccp_channel_t * c, const void *data);
int sccp_channel_callwaiting_tone_interval(sccp_device_t * d, sccp_channel_t * c);
const char *sccp_channel_getLinkedId(const sccp_channel_t * channel);

// find channel
sccp_channel_t *sccp_channel_find_byid(uint32_t id);
sccp_channel_t *sccp_find_channel_on_line_byid(sccp_line_t * l, uint32_t id);
sccp_channel_t *sccp_channel_find_bypassthrupartyid(uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_bystate_on_line(sccp_line_t * l, sccp_channelstate_t state);
sccp_channel_t *sccp_channel_find_bystate_on_device(sccp_device_t * d, sccp_channelstate_t state);
sccp_channel_t *sccp_find_channel_by_lineInstance_and_callid(const sccp_device_t * d, const uint32_t lineInstance, const uint32_t callid);
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid(sccp_device_t * d, uint32_t passthrupartyid);
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c);
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d);

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
