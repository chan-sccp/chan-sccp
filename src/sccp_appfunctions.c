/*!
 * \file        sccp_appfunctions.c
 * \brief       SCCP application / dialplan functions Class
 * \author      Diederik de Groot (ddegroot [at] sourceforge.net)
 * \date        18-03-2011
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 */
#include "config.h"
#include "common.h"
#include "sccp_appfunctions.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_mwi.h"
#include "sccp_conference.h"
#include "sccp_session.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/callerid.h>
#include <asterisk/module.h>		// ast_register_application2
#ifdef HAVE_PBX_APP_H
#  include <asterisk/app.h>
#endif

/*** DOCUMENTATION
	<function name="SCCPDevice" language="en_US">
		<synopsis>
			Retrieves information about an SCCP Device.
		</synopsis>
		<syntax argsep=",">
			<parameter name="deviceId" required="true">
				<para>Device to be queried.</para>
				<optionlist>
					<option name="current"><para>The current device.</para></option>
					<option name="SEPxxxxxxxxxx"><para>string containing the SEP+Mac-address of the device to be queried.</para></option>
				</optionlist>
			</parameter>
			<parameter name="option" required="true" multiple="true">
				<para>One of the following options:</para>
				<optionlist>
					<option name="ip"><para>Ip-Address of the device (string).</para></option>
					<option name="id"><para>Device id (string).</para></option>
					<option name="status"><para>Current Device State (string).</para></option>
					<option name="description"><para>Device Description (string).</para></option>
					<option name="config_type"><para>Config Type (string).</para></option>
					<option name="skinny_type"><para>Skinny Type (string).</para></option>
					<option name="tz_offset"><para>Timezone Offset (integer).</para></option>
					<option name="image_version"><para>Loaded image version (string).</para></option>
					<option name="accessory_status"><para>Accessory Status (string).</para></option>
					<option name="registration_state"><para>Registration State (string).</para></option>
					<option name="codecs"><para>Codec Preferences (string).</para></option>
					<option name="capability"><para>Codec Capabilities (string).</para></option>
					<option name="lines_registered"><para>Has Registered Lines (boolean).</para></option>
					<option name="lines_count"><para>Number of lines (integer).</para></option>
					<option name="last_number"><para>Last number dialed (extension).</para></option>
					<option name="early_rtp"><para>Early RTP Setting (string).</para></option>
					<option name="supported_protocol_version"><para>Supported Protocol by Device (integer).</para></option>
					<option name="used_protocol_version"><para>Currently Used Protocol (integer).</para></option>
					<option name="mwi_light"><para>Current MWI Light Status (boolean).</para></option>
					<option name="dnd_feature"><para>DND Feature (boolean).</para></option>
					<option name="dnd_state"><para>DND State (string).</para></option>
					<option name="dnd_action"><para>DND Action (string).</para></option>
					<option name="dynamic"><para>Is Realtime Device (boolean).</para></option>
					<option name="realtime"><para>Is Realtime Device (boolean).</para></option>
					<option name="active_channel"><para>CallID of active Channel (integer).</para></option>
					<option name="transfer_channel"><para>CallID of channel being transfered (integer).</para></option>
					<option name="allow_conference"><para>Allow Conference (boolean).</para></option>
					<option name="conf_play_general_announce"><para>Play General Announcements (boolean).</para></option>
					<option name="conf_play_part_announce"><para>Play Announcement to participants (boolean).</para></option>
					<option name="conf_mute_on_entry"><para>Are all participant muted when entering the conference (boolean).</para></option>
					<option name="conf_music_on_hold_class"><para>Name of the Music on Hold Class used for conferences (string).</para></option>
					<option name="conf_show_conflist"><para>Should the conference list be displayed when conference is used (boolean).</para></option>
					<option name="conflist_active"><para>Is the conference list currently actively shown (boolean).</para></option>
					<option name="current_line"><para>Currently Active Line ID (integer).</para></option>
					<option name="button_config"><para>Array of buttons associated with this device (comma seperated string).</para></option>
					<option name="pending_delete"><para>Reload is active and device is going to be removed (boolean).</para></option>
					<option name="pending_update"><para>Reload is active and device is going to be updated (boolean).</para></option>
					<option name="rtpqos"><para>Aggregated Call Statistics for this device (string).</para></option>
					<option name="peerip"><para>IP-Address/port Associated with the device session (NO-NAT).</para></option>
					<option name="recvip"><para>IP-Address/Port Actual Source IP-Address Reported by the phone upon registration (NAT).</para></option>
					<option name="chanvar[setvar]" hasparams="true">
						<argument name="setvar" required="true">
							<para>Name of the <replaceable>setvar</replaceable>, associated with this device, to be queried (string).</para>
						</argument>
					</option>
					<option name="codec[codec]" hasparams="true">
						<argument name="codec" required="true">
							<para>Number of Skinny <replaceable>codec</replaceable> to be queried (integer).</para>
						</argument>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This function is used to query sccp device information from insde the dialplan.</para>
			<example>
				Set(ipaddress=${SCCPDevice(SEPxxxxxxxxxx,codecs)});
			</example>
			<para>This function can also be used together with Hash() and Array();</para>
			<para>In this case, the device is only queried once and the results are stored in a Hash Table/Array.</para>
			<example>
				Set(HASH(_SCCP_DEVICE)=${SCCPDevice(current,ip,description,codec[2])});
				Noop(Desc: ${HASH(SCCP_DEVICE,description)});
				Noop(Ip: ${HASH(SCCP_DEVICE,ip)});
				Noop(Codec2: ${HASH(SCCP_DEVICE,codec[2])});
			</example>
		</description>
		<see-also>
			<ref type="application">Array</ref>
			<ref type="application">Hash</ref>
		</see-also>
	</function>
	<function name="SCCPLine" language="en_US">
		<synopsis>
			Retrieves information about an SCCP Line.
		</synopsis>
		<syntax argsep=",">
			<parameter name="lineName" required="true">
				<para>Line to be queried.</para>
				<optionlist>
					<option name="current"><para>The current line.</para></option>
					<option name="parent"><para>The forwarding line (In case this line was created to forward a call).</para></option>
					<option name="LineName"><para>string specifying the name of the line to be queried.</para></option>
				</optionlist>
			</parameter>
			<parameter name="option" required="true" multiple="true">
				<para>One of the following options:</para>
				<optionlist>
					<option name="id"><para>Line Identifier (integer).</para></option>
					<option name="name"><para>Line Name (string).</para></option>
					<option name="description"><para>Line Description (string).</para></option>
					<option name="label"><para>Line Label (string).</para></option>
					<option name="vmnum"><para>Voicemail extension to be dialed when the voicemail soft/hard key is pressed (extension).</para></option>
					<option name="trnsfvm"><para>Extension to be dialed when redirecting a call to voicemail (extension).</para></option>
					<option name="meetme"><para>is meetme enabled (boolean).</para></option>
					<option name="meetmenum"><para>Extension dialed when pressing the meetme softkey (extension).</para></option>
					<option name="meetmeopts"><para>Options set when meetme applicaton is started (string).</para></option>
					<option name="context"><para>PBX Context used when this line starts an outbound call (context).</para></option>
					<option name="language"><para>language (string).</para></option>
					<option name="accountcode"><para>accountcode (string).</para></option>
					<option name="musicclass"><para>musicclass (string).</para></option>
					<option name="amaflags"><para>amaflags (string).</para></option>
					<option name="dnd_action"><para>dnd_action (string).</para></option>
					<option name="callgroup"><para>call group this line is part of (integer-array).</para></option>
					<option name="pickupgroup"><para>pickup group this line is part of (integer-array).</para></option>
					<option name="named_callgroup"><para>named call group this line is part of (integer-array).</para></option>
					<option name="named_pickupgroup"><para>named pickup group this line is part of (integer-array).</para></option>
					<option name="cid_name"><para>CallerID Name (string).</para></option>
					<option name="cid_num"><para>CallerID Number (string).</para></option>
					<option name="incoming_limit"><para>Maximum number of inbound calls that will be accepted to this line (integer).</para></option>
					<option name="channel_count"><para>Current number of active channels allocated on this line (integer).</para></option>
					<option name="dynamic"><para>Is Realtime Line (boolean).</para></option>
					<option name="realtime"><para>Is Realtime Line (boolean).</para></option>
					<option name="pending_delete"><para>Reload is active and line is going to be removed (boolean).</para></option>
					<option name="pending_update"><para>Reload is active and line is going to be updated (boolean).</para></option>
					<option name="regexten"><para>Registration Extension (used by Dundi). Allocated in the dialplan when the line is registered (string).</para></option>
					<option name="regcontext"><para>Registration Context (used by Dundi). Allocated in the dialplan when the line is registered (string).</para></option>
					<option name="adhoc_number"><para>If this is a hotline/plar line, this returns the extension to be dialed when the phone goes offhook (extension).</para></option>
					<option name="newmsgs"><para>Number of new voicemail messages (integer).</para></option>
					<option name="oldmsgs"><para>Number of old voicemail messages (integer).</para></option>
					<option name="num_devices"><para>Number of devices this line has been registed on (integer).</para></option>
					<option name="mailboxes"><para>Returns a comma seperated list of voicemail mailboxes connected to this line (csv).</para></option>
					<option name="cfwd"><para>Returns a comma seperated list of callforward set on this line (csv).</para></option>
					<option name="devices"><para>Returns a comma seperated list of devicesId's that are connected to this line(csv).</para></option>
					<option name="chanvar[setvar]" hasparams="true">
						<argument name="setvar" required="true">
							<para>Name of the <replaceable>setvar</replaceable>, associated with this device, to be queried (string).</para>
						</argument>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This function is used to query sccp device information from insde the dialplan.</para>
			<example>
				Set(ipaddress=${SCCPLine(12345,codecs)});
				Set(devices=${SCCPLine(current,devices)});
				Set(cfwd=${SCCPLine(parent,cfwd)});
			</example>
			<para>This function can also be used together with Hash() and Array();</para>
			<para>In this case, the line is only queried once and the results are stored in a Hash Table/Array.</para>
			<example>
				Set(HASH(_SCCP_LINE)=${SCCPLine(current,id,description,devices)});
				Noop(Desc: ${HASH(SCCP_LINE,description)});
				Noop(Id: ${HASH(SCCP_LINE,id)});
				Noop(Devs: ${HASH(SCCP_LINE,devices)});
			</example>
		</description>
		<see-also>
			<ref type="application">Array</ref>
			<ref type="application">Hash</ref>
			<ref type="application">SCCPDevice</ref>
		</see-also>
	</function>
	<function name="SCCPChannel" language="en_US">
		<synopsis>
			Retrieves information about an SCCP Channel.
		</synopsis>
		<syntax argsep=",">
			<parameter name="channel" required="true">
				<para>Line to be queried.</para>
				<optionlist>
					<option name="current"><para>The current channel.</para></option>
					<option name="ChannelName"><para>Retrieve the channel by PBX Channel Name.</para></option>
					<option name="CallId"><para>Retrieve the channel by Call ID.</para></option>
				</optionlist>
			</parameter>
			<parameter name="option" required="true" multiple="true">
				<para>One of the following options:</para>
				<optionlist>
					<option name="callid"><para>Channel CallID (integer).</para></option>
					<option name="id"><para>Channel ID (integer).</para></option>
					<option name="format"><para>Channel RTP Read Format (integer).</para></option>
					<option name="codecs"><para>Channel RTP Read Codec (string).</para></option>
					<option name="capability"><para>Channel Codec Capabilities (string).</para></option>
					<option name="calledPartyName"><para>Called Party Name (string).</para></option>
					<option name="calledPartyNumber"><para>Called Party Number (string).</para></option>
					<option name="callingPartyName"><para>Calling Party Name (string).</para></option>
					<option name="callingPartyNumber"><para>calling Party Number (string).</para></option>
					<option name="originalCallingPartyName"><para>Original Calling Party Name (string).</para></option>
					<option name="originalCallingPartyNumber"><para>Original Calling Party Number (string).</para></option>
					<option name="originalCalledPartyName"><para>Original Called Party Name (string).</para></option>
					<option name="originalCalledPartyNumber"><para>Original Called Party Number (string).</para></option>
					<option name="lastRedirectingPartyName"><para>Last Redirecting Party Name (string).</para></option>
					<option name="lastRedirectingPartyNumber"><para>Last Redirecting Party Number (string).</para></option>
					<option name="cgpnVoiceMailbox"><para>Calling Party Voice Mailbox (string).</para></option>
					<option name="cdpnVoiceMailbox"><para>Called Party Voice Mailbox (string).</para></option>
					<option name="originalCdpnVoiceMailbox"><para>Original Called Party Voice Mailbox (string).</para></option>
					<option name="lastRedirectingVoiceMailbox"><para>Last Redirecting Voice Mailbox (string).</para></option>
					<option name="passthrupartyid"><para>RTP / Media Passthrough Party Id (integer).</para></option>
					<option name="state"><para>ChannelState (string).</para></option>
					<option name="previous_state"><para>Previous ChannelState (string).</para></option>
					<option name="calltype"><para>Call Type (inbound/outbound) (string).</para></option>
					<option name="dialed_number"><para>Dialed Number (only on outbound call) (extension).</para></option>
					<option name="device"><para>Current DeviceId this channel is attached to (only active calls) (string).</para></option>
					<option name="line"><para>Current LineName this channel is attached to (only active calls) (string).</para></option>
					<option name="answered_elsewhere"><para>This call has been answered somewhere else (boolean).</para></option>
					<option name="privacy"><para>Privacy (boolean).</para></option>
					<option name="ss_action"><para>SoftSwitch Action responsible for this channel (string).</para></option>
					<option name="conference_id"><para>Conference Id associated to this channel (string).</para></option>
					<option name="conference_participant_id"><para>Conference Participant Id (string).</para></option>
					<option name="parent"><para>Channel Forwarding Parent CallID. When this is a channel originating from a forwarded call, this will link back to the callid of the original call. (integer).</para></option>
					<option name="bridgepeer"><para>Remote Bridge Peer connected to this channel (string).</para></option>
					<option name="peerip"><para>IP-Address/port Associated with the device session (NO-NAT).</para></option>
					<option name="recvip"><para>IP-Address/Port Actual Source IP-Address Reported by the phone upon registration (NAT).</para></option>
					<option name="rtpqos"><para>Aggregated Call Statistics for this device (string).</para></option>
					<option name="codec[codec]" hasparams="true">
						<argument name="codec" required="true">
							<para>Number of Skinny <replaceable>codec</replaceable> to be queried (integer).</para>
						</argument>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This function is used to query sccp device information from insde the dialplan.</para>
			<example>
				Set(state=${SCCPChannel(12345,state)});
				Set(device=${SCCPChannel(current,device)});
				Set(calltype=${SCCPChannel(SCCP/12345-00000015,calltype)});
			</example>
			<para>This function can also be used together with Hash() and Array();</para>
			<para>In this case, the channel is only queried once and the results are stored in a Hash Table/Array.</para>
			<example>
				Set(HASH(_SCCP_CHANNEL)=${SCCPChannel(current,bridgepeer,peerip,recvip)});
				Noop(PIp: ${HASH(SCCP_CHANNEL,peerip)});
				Noop(RIp: ${HASH(SCCP_CHANNEL,recvip)});
				Noop(Bridge: ${HASH(SCCP_CHANNEL,bridgepeer)});
			</example>
		</description>
		<see-also>
			<ref type="application">Array</ref>
			<ref type="application">Hash</ref>
			<ref type="application">SCCPDevice</ref>
			<ref type="application">SCCPLine</ref>
		</see-also>
	</function>
	<application name="SCCPSetCalledParty" language="en_US">
		<synopsis>
			Set the callerid of the called party.
		</synopsis>
		<syntax>
			<parameter name="callerid" required="true">
				<para>CallerID String, following the format "'<replaceable>Name</replaceable>' &lt;<replaceable>extension</replaceable>&gt;"</para>
			</parameter>
		</syntax>
		<description>
			<para>Usage: SCCPSetCalledParty(\"<replaceable>Name</replaceable>\" &lt;<replaceable>ext</replaceable>&gt;);</para>
			<para>Sets the name and number of the called party for use with chan_sccp.</para>
			<note>
				<para>DEPRECATED:please use generic 'Set(CHANNEL(calledparty)=\"<replaceable>name</replaceable> &lt;<replaceable>exten</replaceable>&gt;\");' instead</para>
			</note>
		</description>
		<see-also>
			<ref type="application">Channel</ref>
		</see-also>
	</application>
	<application name="SCCPSetCodec" language="en_US">
		<synopsis>
			Set the prefered codec for the current sccp channel to be used before dialing the destination channel.
		</synopsis>
		<syntax>
			<parameter name="codec" required="true">
				<para>String specifying the codec to be used.</para>
			</parameter>
		</syntax>
		<description>
			<para>Usage: SCCPSetCodec(<replaceable>Codec Name</replaceable>);</para>
			<para>Example: SCCPSetCodec(alaw);</para>
			<note>
				<para>This has to be done before dialing the destination</para>
				<para>DEPRECATED:please use generic 'Set(CHANNEL(codec)=<replaceable>Codec Name</replaceable>);' instead</para>
			</note>
		</description>
		<see-also>
			<ref type="application">Channel</ref>
		</see-also>
	</application>
	<application name="SCCPSetMessage" language="en_US">
		<synopsis>
			Send a message to the statusline of the phone connected to this channel.
		</synopsis>
		<syntax>
			<parameter name="message" required="true">
				<para>String specifying the message that should be send.</para>
			</parameter>
			<parameter name="timeout" required="false">
				<para>Number of seconds the message should be displayed.</para>
				<para>If timeout is ommitted, the message will remain until the next/empty message.</para>
			</parameter>
			<parameter name="priority" required="false">
				<para>Use priority to set/clear priority notifications.</para>
				<para>Higher priority levels overrule lower ones.</para>
			</parameter>
		</syntax>
		<description>
			<para>Usage: SCCPSetMessage(<replaceable>Message Text</replaceable>, <replaceable>timeout</replaceable>, <replaceable>priority</replaceable>);</para>
			<para>Example: SCCPSetMessage("Test Test", 10);</para>
		</description>
	</application>
 ***/

PBX_THREADSTORAGE(coldata_buf);
PBX_THREADSTORAGE(colnames_buf);

/*!
 * \brief ${SCCPDevice()} Dialplan function - reads device data
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 */
static int sccp_func_sccpdevice(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;												// we should make this a finite length
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;
	int addcomma = 0;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPDevice(): usage of ':' to separate arguments is deprecated. Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "ip");
	}

	AUTO_RELEASE(sccp_device_t, d , NULL);

	if (!strncasecmp(data, "current", 7)) {
		AUTO_RELEASE(sccp_channel_t, c , get_sccp_channel_from_pbx_channel(chan));

		if (c) {
			if (!(d = sccp_channel_getDevice(c))) {
				pbx_log(LOG_WARNING, "SCCPDevice(): SCCP Device not available\n");
				return -1;
			}
		} else {
			/* pbx_log(LOG_WARNING, "SCCPDevice(): No current SCCP channel found\n"); */
			return -1;
		}
	} else {
		if (!(d = sccp_device_find_byid(data, FALSE))) {
			pbx_log(LOG_WARNING, "SCCPDevice(): SCCP Device not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (d) {
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			addcomma = 0;
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			
			if (!strcasecmp(token, "ip")) {
				sccp_session_t *s = d->session;

				if (s) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getOurIP(s, &sas, 0);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), buf_len);
				}
			} else if (!strcasecmp(token, "id")) {
				sccp_copy_string(buf, d->id, buf_len);
			} else if (!strcasecmp(token, "status")) {
				sccp_copy_string(buf, sccp_devicestate2str(sccp_device_getDeviceState(d)), buf_len);
			} else if (!strcasecmp(token, "description")) {
				sccp_copy_string(buf, d->description, buf_len);
			} else if (!strcasecmp(token, "config_type")) {
				sccp_copy_string(buf, d->config_type, buf_len);
			} else if (!strcasecmp(token, "skinny_type")) {
				sccp_copy_string(buf, skinny_devicetype2str(d->skinny_type), buf_len);
			} else if (!strcasecmp(token, "tz_offset")) {
				snprintf(buf, buf_len, "%d", d->tz_offset);
			} else if (!strcasecmp(token, "image_version")) {
				sccp_copy_string(buf, d->loadedimageversion, buf_len);
			} else if (!strcasecmp(token, "accessory_status")) {
				sccp_accessory_t activeAccessory = sccp_device_getActiveAccessory(d);
				snprintf(buf, buf_len, "%s:%s", sccp_accessory2str(activeAccessory), sccp_accessorystate2str(sccp_device_getAccessoryStatus(d, activeAccessory)));
			} else if (!strcasecmp(token, "registration_state")) {
				sccp_copy_string(buf, skinny_registrationstate2str(sccp_device_getRegistrationState(d)), buf_len);
			} else if (!strcasecmp(token, "codecs")) {
				sccp_codec_multiple2str(buf, buf_len - 1, d->preferences.audio, ARRAY_LEN(d->preferences.audio));
			} else if (!strcasecmp(token, "capability")) {
				sccp_codec_multiple2str(buf, buf_len - 1, d->capabilities.audio, ARRAY_LEN(d->capabilities.audio));
			} else if (!strcasecmp(token, "lines_registered")) {
				sccp_copy_string(buf, d->linesRegistered ? "yes" : "no", buf_len);
			} else if (!strcasecmp(token, "lines_count")) {
				snprintf(buf, buf_len, "%d", d->linesCount);
			} else if (!strcasecmp(token, "last_number")) {
				sccp_copy_string(buf, d->redialInformation.number, buf_len);
			} else if (!strcasecmp(token, "early_rtp")) {
				snprintf(buf, buf_len, "%s", sccp_earlyrtp2str(d->earlyrtp));
			} else if (!strcasecmp(token, "supported_protocol_version")) {
				snprintf(buf, buf_len, "%d", d->protocolversion);
			} else if (!strcasecmp(token, "used_protocol_version")) {
				snprintf(buf, buf_len, "%d", d->inuseprotocolversion);
			} else if (!strcasecmp(token, "mwi_light")) {
				sccp_copy_string(buf, d->mwilight ? "ON" : "OFF", buf_len);
			} else if (!strcasecmp(token, "dnd_feature")) {
				sccp_copy_string(buf, (d->dndFeature.enabled) ? "ON" : "OFF", buf_len);
			} else if (!strcasecmp(token, "dnd_state")) {
				sccp_copy_string(buf, sccp_dndmode2str(d->dndFeature.status), buf_len);
			} else if (!strcasecmp(token, "dnd_action")) {
				sccp_copy_string(buf, sccp_dndmode2str(d->dndmode), buf_len);
			} else if (!strcasecmp(token, "dynamic") || !strcasecmp(token, "realtime")) {
#ifdef CS_SCCP_REALTIME
				sccp_copy_string(buf, d->realtime ? "yes" : "no", buf_len);
#else
				sccp_copy_string(buf, "not supported", buf_len);
#endif
			} else if (!strcasecmp(token, "active_channel")) {
				snprintf(buf, buf_len, "%d", d->active_channel->callid);
			} else if (!strcasecmp(token, "transfer_channel")) {
				snprintf(buf, buf_len, "%d", d->transferChannels.transferee->callid);
#ifdef CS_SCCP_CONFERENCE
//			} else if (!strcasecmp(token, "conference_id")) {
//				snprintf(buf, buf_len, "%d", d->conference->id);
			} else if (!strcasecmp(token, "allow_conference")) {
				snprintf(buf, buf_len, "%s", d->allow_conference ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_play_general_announce")) {
				snprintf(buf, buf_len, "%s", d->conf_play_general_announce ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_play_part_announce")) {
				snprintf(buf, buf_len, "%s", d->conf_play_part_announce ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_mute_on_entry")) {
				snprintf(buf, buf_len, "%s", d->conf_mute_on_entry ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conf_music_on_hold_class")) {
				snprintf(buf, buf_len, "%s", d->conf_music_on_hold_class);
			} else if (!strcasecmp(token, "conf_show_conflist")) {
				snprintf(buf, buf_len, "%s", d->conf_show_conflist ? "ON" : "OFF");
			} else if (!strcasecmp(token, "conflist_active")) {
				snprintf(buf, buf_len, "%s", d->conferencelist_active ? "ON" : "OFF");
#endif
			} else if (!strcasecmp(token, "current_line")) {
				sccp_copy_string(buf, d->currentLine->id, buf_len);
			} else if (!strcasecmp(token, "button_config")) {
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
				sccp_buttonconfig_t *config;

				SCCP_LIST_LOCK(&d->buttonconfig);
				SCCP_LIST_TRAVERSE(&d->buttonconfig, config, list) {
					switch (config->type) {
						case LINE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->button.line.name ? config->button.line.name : "");
							break;
						case SPEEDDIAL:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.speeddial.ext ? config->button.speeddial.ext : "");
							break;
						case SERVICE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.service.url ? config->button.service.url : "");
							break;
						case FEATURE:
							pbx_str_append(&lbuf, 0, "%s[%d,%s,%s,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type), config->label, config->button.feature.options ? config->button.feature.options : "");
							break;
						case EMPTY:
							pbx_str_append(&lbuf, 0, "%s[%d,%s]", addcomma++ ? "," : "", config->instance, sccp_config_buttontype2str(config->type));
							break;
						case SCCP_CONFIG_BUTTONTYPE_SENTINEL:
							break;
					}
				}
				SCCP_LIST_UNLOCK(&d->buttonconfig);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "pending_delete")) {
				sccp_copy_string(buf, d->pendingDelete ? "yes" : "no", buf_len);
			} else if (!strcasecmp(token, "pending_update")) {
				sccp_copy_string(buf, d->pendingUpdate ? "yes" : "no", buf_len);
			} else if (!strcasecmp(token, "peerip")) {							// NO-NAT (Ip-Address Associated with the Session->sin)
				if (d->session) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getOurIP(d->session, &sas, 0);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(token, "recvip")) {							// NAT (Actual Source IP-Address Reported by the phone upon registration)
				if (d->session) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getSas(d->session, &sas);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(colname, "rtpqos")) {
				sccp_call_statistics_t *call_stats = d->call_statistics;
				snprintf(buf, buf_len, "Packets sent: %d;rcvd: %d;lost: %d;jitter: %d;latency: %d;MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d", call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_LAST].latency,
				       call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
				       call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio,
				       (int) call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);
			} else if (!strncasecmp(token, "chanvar[", 8)) {
				char *chanvar = token + 8;
				PBX_VARIABLE_TYPE *v;

				chanvar = strsep(&chanvar, "]");
				for (v = d->variables; v; v = v->next) {
					if (!strcasecmp(v->name, chanvar)) {
						sccp_copy_string(buf, v->value, buf_len);
					}
				}
			} else if (!strncasecmp(token, "codec[", 6)) {
				char *codecnum;

				codecnum = token + 6;									// move past the '[' 
				codecnum = strsep(&codecnum, "]");							// trim trailing ']' if any 
				int codec_int = sccp_atoi(codecnum, strlen(codecnum));
				if (skinny_codecs[codec_int].key) {
					sccp_copy_string(buf, codec2name(codec_int), buf_len);
				} else {
					buf[0] = '\0';
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPDevice(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}
			
			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPDevice */
static struct pbx_custom_function sccpdevice_function = {
	.name = "SCCPDevice",
	.read = sccp_func_sccpdevice,
};

/*!
 * \brief  ${SCCPLine()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 * 
 */
static int sccp_func_sccpline(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;
	int addcomma = 0;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPLine(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "id");
	}
	AUTO_RELEASE(sccp_line_t, l , NULL);
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
			/* pbx_log(LOG_WARNING, "SCCPLine(): Not an SCCP Channel\n"); */
			return -1;
		}

		if (!c->line) {
			pbx_log(LOG_WARNING, "SCCPLine(): SCCP Line not available\n");
			return -1;
		}
		l = sccp_line_retain(c->line);
	} else if (!strncasecmp(data, "parent", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {

			/* pbx_log(LOG_WARNING, "SCCPLine(): Not an SCCP Channel\n"); */
			return -1;
		}

		if (!c->parentChannel || !c->parentChannel->line) {
			pbx_log(LOG_WARNING, "SCCPLine(): SCCP Line not available\n");
			return -1;
		}
		l = sccp_line_retain(c->parentChannel->line);
	} else {
		if (!(l = sccp_line_find_byname(data, TRUE))) {
			pbx_log(LOG_WARNING, "SCCPLine(): SCCP Line not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (l) {
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			addcomma = 0;
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			
			if (!strcasecmp(token, "id")) {
				sccp_copy_string(buf, l->id, len);
			} else if (!strcasecmp(token, "name")) {
				sccp_copy_string(buf, l->name, len);
			} else if (!strcasecmp(token, "description")) {
				sccp_copy_string(buf, l->description, len);
			} else if (!strcasecmp(token, "label")) {
				sccp_copy_string(buf, l->label, len);
			} else if (!strcasecmp(token, "vmnum")) {
				sccp_copy_string(buf, l->vmnum, len);
			} else if (!strcasecmp(token, "trnsfvm")) {
				sccp_copy_string(buf, l->trnsfvm, len);
			} else if (!strcasecmp(token, "meetme")) {
				sccp_copy_string(buf, l->meetme ? "on" : "off", len);
			} else if (!strcasecmp(token, "meetmenum")) {
				sccp_copy_string(buf, l->meetmenum, len);
			} else if (!strcasecmp(token, "meetmeopts")) {
				sccp_copy_string(buf, l->meetmeopts, len);
			} else if (!strcasecmp(token, "context")) {
				sccp_copy_string(buf, l->context, len);
			} else if (!strcasecmp(token, "language")) {
				sccp_copy_string(buf, l->language, len);
			} else if (!strcasecmp(token, "accountcode")) {
				sccp_copy_string(buf, l->accountcode, len);
			} else if (!strcasecmp(token, "musicclass")) {
				sccp_copy_string(buf, l->musicclass, len);
			} else if (!strcasecmp(token, "amaflags")) {
				sccp_copy_string(buf, l->amaflags ? "yes" : "no", len);
			} else if (!strcasecmp(token, "dnd_action")) {
				sccp_copy_string(buf, sccp_dndmode2str(l->dndmode), buf_len);
			} else if (!strcasecmp(token, "callgroup")) {
				pbx_print_group(buf, buf_len, l->callgroup);
			} else if (!strcasecmp(token, "pickupgroup")) {
#ifdef CS_SCCP_PICKUP
				pbx_print_group(buf, buf_len, l->pickupgroup);
#else
				sccp_copy_string(buf, "not supported", len);
#endif
#ifdef CS_AST_HAS_NAMEDGROUP
			} else if (!strcasecmp(token, "named_callgroup")) {
				ast_copy_string(buf, l->namedcallgroup, len);
			} else if (!strcasecmp(token, "named_pickupgroup")) {
#ifdef CS_SCCP_PICKUP
				ast_copy_string(buf, l->namedpickupgroup, len);
#else
				sccp_copy_string(buf, "not supported", len);
#endif
#endif
			} else if (!strcasecmp(token, "cid_name")) {
				sccp_copy_string(buf, l->cid_name, len);
			} else if (!strcasecmp(token, "cid_num")) {
				sccp_copy_string(buf, l->cid_num, len);
			} else if (!strcasecmp(token, "incoming_limit")) {
				snprintf(buf, buf_len, "%d", l->incominglimit);
			} else if (!strcasecmp(token, "channel_count")) {
				snprintf(buf, buf_len, "%d", SCCP_RWLIST_GETSIZE(&l->channels));
			} else if (!strcasecmp(token, "dynamic") || !strcasecmp(token, "realtime")) {
#ifdef CS_SCCP_REALTIME
				sccp_copy_string(buf, l->realtime ? "Yes" : "No", len);
#else
				sccp_copy_string(buf, "not supported", len);
#endif
			} else if (!strcasecmp(token, "pending_delete")) {
				sccp_copy_string(buf, l->pendingDelete ? "yes" : "no", len);
			} else if (!strcasecmp(token, "pending_update")) {
				sccp_copy_string(buf, l->pendingUpdate ? "yes" : "no", len);
			} else if (!strcasecmp(token, "regexten")) {
				sccp_copy_string(buf, l->regexten ? l->regexten : "Unset", len);
			} else if (!strcasecmp(token, "regcontext")) {
				sccp_copy_string(buf, l->regcontext ? l->regcontext : "Unset", len);
			} else if (!strcasecmp(token, "adhoc_number")) {
				sccp_copy_string(buf, l->adhocNumber ? l->adhocNumber : "No", len);
			} else if (!strcasecmp(token, "newmsgs")) {
				snprintf(buf, buf_len, "%d", l->voicemailStatistic.newmsgs);
			} else if (!strcasecmp(token, "oldmsgs")) {
				snprintf(buf, buf_len, "%d", l->voicemailStatistic.oldmsgs);
			} else if (!strcasecmp(token, "num_devices")) {
				snprintf(buf, buf_len, "%d", SCCP_LIST_GETSIZE(&l->devices));
			} else if (!strcasecmp(token, "mailboxes")) {
				sccp_mailbox_t *mailbox;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
				SCCP_LIST_LOCK(&l->mailboxes);
				SCCP_LIST_TRAVERSE(&l->mailboxes, mailbox, list) {
					pbx_str_append(&lbuf, 0, "%s%s%s%s", addcomma++ ? "," : "", mailbox->mailbox, mailbox->context ? "@" : "", mailbox->context ? mailbox->context : "");
				}
				SCCP_LIST_UNLOCK(&l->mailboxes);
				snprintf(buf, buf_len, "%s", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "cfwd")) {
				sccp_linedevices_t *linedevice = NULL;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

				SCCP_LIST_LOCK(&l->devices);
				SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
					if (linedevice) {
						pbx_str_append(&lbuf, 0, "%s[id:%s,cfwdAll:%s,num:%s,cfwdBusy:%s,num:%s]", addcomma++ ? "," : "", linedevice->device->id, linedevice->cfwdAll.enabled ? "on" : "off", linedevice->cfwdAll.number, linedevice->cfwdBusy.enabled ? "on" : "off", linedevice->cfwdBusy.number);
					}
				}
				SCCP_LIST_UNLOCK(&l->devices);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strcasecmp(token, "devices")) {
				sccp_linedevices_t *linedevice = NULL;
				pbx_str_t *lbuf = pbx_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);

				SCCP_LIST_LOCK(&l->devices);
				SCCP_LIST_TRAVERSE(&l->devices, linedevice, list) {
					if (linedevice) {
						pbx_str_append(&lbuf, 0, "%s%s", addcomma++ ? "," : "", linedevice->device->id);
					}
				}
				SCCP_LIST_UNLOCK(&l->devices);
				snprintf(buf, buf_len, "[ %s ]", pbx_str_buffer(lbuf));
			} else if (!strncasecmp(token, "chanvar[", 8)) {
				char *chanvar = token + 8;

				PBX_VARIABLE_TYPE *v;

				chanvar = strsep(&chanvar, "]");
				for (v = l->variables; v; v = v->next) {
					if (!strcasecmp(v->name, chanvar)) {
						sccp_copy_string(buf, v->value, len);
					}
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPLine(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}

			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPLine */
static struct pbx_custom_function sccpline_function = {
	.name = "SCCPLine",
	.read = sccp_func_sccpline,
};

/*!
 * \brief  ${SCCPChannel()} Dialplan function - reads sccp line data 
 * \param chan Asterisk Channel
 * \param cmd Command as char
 * \param data Extra data as char
 * \param output Buffer as chan*
 * \param len Lenght as size_t
 * \return Status as int
 *
 * \author Diederik de Groot <ddegroot@users.sourceforce.net>
 * 
 * \called_from_asterisk
 */
static int sccp_func_sccpchannel(PBX_CHANNEL_TYPE * chan, NEWCONST char *cmd, char *data, char *output, size_t len)
{
	PBX_CHANNEL_TYPE *ast;
	pbx_str_t *coldata = pbx_str_thread_get(&coldata_buf, 16);
	pbx_str_t *colnames = pbx_str_thread_get(&colnames_buf, 16);
	char *colname;
	uint16_t buf_len = 1024;
	char buf[1024] = "";
	char *token = NULL;

	if ((colname = strchr(data, ':'))) {									/*! \todo Will be deprecated after 1.4 */
		static int deprecation_warning = 0;

		*colname++ = '\0';
		if (deprecation_warning++ % 10 == 0) {
			pbx_log(LOG_WARNING, "SCCPChannel(): usage of ':' to separate arguments is deprecated.  Please use ',' instead.\n");
		}
	} else if ((colname = strchr(data, ','))) {
		*colname++ = '\0';
	} else {
		colname = sccp_alloca(16);
		if (!colname) {
			return -1;
		}
		snprintf(colname, 16, "callid");
	}

	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!strncasecmp(data, "current", 7)) {
		if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
			return -1;										/* Not a SCCP channel. */
		}
	} else if (iPbx.getChannelByName(data, &ast) && (c = get_sccp_channel_from_pbx_channel(ast)) != NULL) {
		/* continue with sccp channel */

	} else {
		uint32_t callid = sccp_atoi(data, strlen(data));

		if (!(c = sccp_channel_find_byid(callid))) {
			pbx_log(LOG_WARNING, "SCCPChannel(): SCCP Channel not available\n");
			return -1;
		}
	}
	pbx_str_reset(colnames);
	pbx_str_reset(coldata);
	if (c) {
		sccp_callinfo_t *ci = sccp_channel_getCallInfo(c);
		snprintf(colname + strlen(colname), sizeof *colname, ",");
		token = strtok(colname, ",");
		while (token != NULL) {
			token = pbx_trim_blanks(token);
			
			/** copy request tokens for HASH() */
			if (pbx_str_strlen(colnames)) {
				pbx_str_append(&colnames, 0, ",");
			}
			pbx_str_append_escapecommas(&colnames, 0, token, sccp_strlen(token));
			/** */
			

			if (!strcasecmp(token, "callid") || !strcasecmp(token, "id")) {
				snprintf(buf, buf_len, "%d", c->callid);
			} else if (!strcasecmp(token, "format")) {
				snprintf(buf, buf_len, "%d", c->rtp.audio.readFormat);
			} else if (!strcasecmp(token, "codecs")) {
				sccp_copy_string(buf, codec2name(c->rtp.audio.readFormat), len);
			} else if (!strcasecmp(token, "capability")) {
				sccp_codec_multiple2str(buf, buf_len - 1, c->capabilities.audio, ARRAY_LEN(c->capabilities.audio));
			} else if (!strcasecmp(token, "calledPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "calledPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "callingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "callingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCallingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCallingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCalledPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCalledPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingPartyName")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingPartyNumber")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "cgpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "cdpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "originalCdpnVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "lastRedirectingVoiceMailbox")) {
				iCallInfo.Getter(ci, SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL, buf, SCCP_CALLINFO_KEY_SENTINEL);
			} else if (!strcasecmp(token, "passthrupartyid")) {
				snprintf(buf, buf_len, "%d", c->passthrupartyid);
			} else if (!strcasecmp(token, "state")) {
				sccp_copy_string(buf, sccp_channelstate2str(c->state), len);
			} else if (!strcasecmp(token, "previous_state")) {
				sccp_copy_string(buf, sccp_channelstate2str(c->previousChannelState), len);
			} else if (!strcasecmp(token, "calltype")) {
				sccp_copy_string(buf, skinny_calltype2str(c->calltype), len);
			} else if (!strcasecmp(token, "ringtype")) {
				sccp_copy_string(buf, skinny_ringtype2str(c->ringermode), len);
			} else if (!strcasecmp(token, "dialed_number")) {
				sccp_copy_string(buf, c->dialedNumber, len);
			} else if (!strcasecmp(token, "device")) {
				sccp_copy_string(buf, c->currentDeviceId, len);
			} else if (!strcasecmp(token, "line")) {
				sccp_copy_string(buf, c->line->name, len);
			} else if (!strcasecmp(token, "answered_elsewhere")) {
				sccp_copy_string(buf, c->answered_elsewhere ? "yes" : "no", len);
			} else if (!strcasecmp(token, "privacy")) {
				sccp_copy_string(buf, c->privacy ? "yes" : "no", len);
			} else if (!strcasecmp(token, "softswitch_action")) {
				snprintf(buf, buf_len, "%s (%d)", sccp_softswitch2str(c->softswitch_action), c->softswitch_action);
			// } else if (!strcasecmp(token, "monitorEnabled")) {
				//sccp_copy_string(buf, c->monitorEnabled ? "yes" : "no", len);
#ifdef CS_SCCP_CONFERENCE
			} else if (!strcasecmp(token, "conference_id")) {
				snprintf(buf, buf_len, "%d", c->conference_id);
			} else if (!strcasecmp(token, "conference_participant_id")) {
				snprintf(buf, buf_len, "%d", c->conference_participant_id);
#endif
			} else if (!strcasecmp(token, "parent")) {
				snprintf(buf, buf_len, "%d", c->parentChannel->callid);
			} else if (!strcasecmp(token, "bridgepeer")) {
				PBX_CHANNEL_TYPE *bridgechannel;
				if (c->owner && (bridgechannel = iPbx.get_bridged_channel(c->owner))) {
					snprintf(buf, buf_len, "%s", pbx_channel_name(bridgechannel));
					pbx_channel_unref(bridgechannel);
				} else {
					snprintf(buf, buf_len, "<unknown>");
				}
			} else if (!strcasecmp(token, "peerip")) {							// NO-NAT (Ip-Address Associated with the Session->sin)
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getOurIP(d->session, &sas, 0);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(token, "recvip")) {							// NAT (Actual Source IP-Address Reported by the phone upon registration)
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					struct sockaddr_storage sas = { 0 };
					sccp_session_getSas(d->session, &sas);
					sccp_copy_string(buf, sccp_netsock_stringify(&sas), len);
				}
			} else if (!strcasecmp(colname, "rtpqos")) {
				AUTO_RELEASE(sccp_device_t, d , NULL);
				if ((d = sccp_channel_getDevice(c))) {
					sccp_call_statistics_t *call_stats = d->call_statistics;
					snprintf(buf, buf_len, "Packets sent: %d;rcvd: %d;lost: %d;jitter: %d;latency: %d;MLQK=%.4f;MLQKav=%.4f;MLQKmn=%.4f;MLQKmx=%.4f;MLQKvr=%.2f|ICR=%.4f;CCR=%.4f;ICRmx=%.4f|CS=%d;SCS=%d", call_stats[SCCP_CALLSTATISTIC_LAST].packets_sent, call_stats[SCCP_CALLSTATISTIC_LAST].packets_received, call_stats[SCCP_CALLSTATISTIC_LAST].packets_lost, call_stats[SCCP_CALLSTATISTIC_LAST].jitter, call_stats[SCCP_CALLSTATISTIC_LAST].latency,
					       call_stats[SCCP_CALLSTATISTIC_LAST].opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].avg_opinion_score_listening_quality,
					       call_stats[SCCP_CALLSTATISTIC_LAST].mean_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].max_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].variance_opinion_score_listening_quality, call_stats[SCCP_CALLSTATISTIC_LAST].interval_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].cumulative_concealement_ratio, call_stats[SCCP_CALLSTATISTIC_LAST].max_concealement_ratio,
					       (int) call_stats[SCCP_CALLSTATISTIC_LAST].concealed_seconds, (int) call_stats[SCCP_CALLSTATISTIC_LAST].severely_concealed_seconds);
				}
			} else if (!strncasecmp(token, "codec[", 6)) {
				char *codecnum;

				codecnum = token + 6;									// move past the '[' 
				codecnum = strsep(&codecnum, "]");							// trim trailing ']' if any 
				int codec_int = sccp_atoi(codecnum, strlen(codecnum));
				if (skinny_codecs[codec_int].key) {
					sccp_copy_string(buf, codec2name(codec_int), buf_len);
				} else {
					buf[0] = '\0';
				}
			} else {
				pbx_log(LOG_WARNING, "SCCPChannel(%s): unknown colname: %s\n", data, token);
				buf[0] = '\0';
			}

			/** copy buf to coldata */
			pbx_str_append_escapecommas(&coldata, 0, buf, buf_len);
			token = strtok(NULL, ",");
			if (token != NULL) {
				pbx_str_append(&coldata, 0, ",");
			}
			buf[0] = '\0';
			/** */
		}
		
		pbx_builtin_setvar_helper(chan, "~ODBCFIELDS~", pbx_str_buffer(colnames));	/* setvar ODBCFIELDS so that results can be used by HASH() and ARRAY() */
		sccp_copy_string(output, pbx_str_buffer(coldata), len);
	}
	return 0;
}

/*! \brief Stucture to declare a dialplan function: SCCPChannel */
static struct pbx_custom_function sccpchannel_function = {
	.name = "SCCPChannel",
	.read = sccp_func_sccpchannel,
};

/*!
 * \brief       Set the Preferred Codec for a SCCP channel via the dialplan
 * \param       chan Asterisk Channel
 * \param       data single codec name
 * \return      Success as int
 * 
 * \called_from_asterisk
 * \deprecated
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_prefcodec(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);
	int res;

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetCodec: Not an SCCP channel\n");
		return -1;
	}

	res = sccp_channel_setPreferredCodec(c, data);
	pbx_log(LOG_WARNING, "SCCPSetCodec: Is now deprecated. Please use 'Set(CHANNEL(codec)=%s)' insteadl.\n", (char *) data);
	return res ? 0 : -1;
}
static char *prefcodec_name = "SCCPSetCodec";

/*!
 * \brief       Set the Name and Number of the Called Party to the Calling Phone
 * \param       chan Asterisk Channel
 * \param       data CallerId in format "Name" \<number\>
 * \return      Success as int
 * 
 * \called_from_asterisk
 * \deprecated
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_calledparty(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	char *text = (char *) data;
	char *num, *name;
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetCalledParty: Not an SCCP channel\n");
		return 0;
	}

	if (!text) {
		pbx_log(LOG_WARNING, "SCCPSetCalledParty: No CalledParty Information Provided\n");
		return 0;
	}

	pbx_callerid_parse(text, &name, &num);
	sccp_channel_set_calledparty(c, name, num);
	return 0;
}
static char *calledparty_name = "SCCPSetCalledParty";

/*!
 * \brief       It allows you to send a message to the calling device.
 * \author      Frank Segtrop <fs@matflow.net>
 * \param       chan asterisk channel
 * \param       data message to sent - if empty clear display
 * \version     20071112_1944
 *
 * \called_from_asterisk
 */
#if ASTERISK_VERSION_NUMBER >= 10800
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, const char *data)
#else
static int sccp_app_setmessage(PBX_CHANNEL_TYPE * chan, void *data)
#endif
{
	AUTO_RELEASE(sccp_channel_t, c , NULL);

	if (!(c = get_sccp_channel_from_pbx_channel(chan))) {
		pbx_log(LOG_WARNING, "SCCPSetMessage: Not an SCCP channel\n");
		return 0;
	}

	int timeout = 0;
	int priority = -1;

	char *parse = pbx_strdupa(data);
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(text);
		AST_APP_ARG(timeout);
		AST_APP_ARG(priority);
	);
	AST_STANDARD_APP_ARGS(args, parse);

	if (!sccp_strlen_zero(args.timeout)) {
		timeout = sccp_atoi(args.timeout, strlen(args.timeout));
	}
	if (!sccp_strlen_zero(args.priority)) {
		priority = sccp_atoi(args.priority, strlen(args.priority));
	}

	AUTO_RELEASE(sccp_device_t, d , NULL);
	if (!(d = sccp_channel_getDevice(c))) {
		pbx_log(LOG_WARNING, "SCCPSetMessage: Not an SCCP device provided\n");
		return 0;
	}

	pbx_log(LOG_WARNING, "SCCPSetMessage: text:'%s', prio:%d, timeout:%d\n", args.text, priority, timeout);
	if (!sccp_strlen_zero(args.text)) {
		if (priority > -1) {
			sccp_dev_displayprinotify(d, args.text, priority, timeout);
		} else {
			sccp_dev_set_message(d, args.text, timeout, TRUE, FALSE);
		}
	} else {
		if (priority > -1) {
			sccp_dev_cleardisplayprinotify(d, priority);
		} else {
			sccp_dev_clear_message(d, TRUE);
		}
	}
	return 0;
}
static char *setmessage_name = "SCCPSetMessage";

//#include "pbx_impl/ast113/ast113.h"
int sccp_register_dialplan_functions(void)
{
	int result = 0;

	/* Register application functions */
	result = iPbx.register_application(calledparty_name, sccp_app_calledparty);
	result |= iPbx.register_application(setmessage_name, sccp_app_setmessage);
	result |= iPbx.register_application(prefcodec_name, sccp_app_prefcodec);

	/* Register dialplan functions */
	result |= iPbx.register_function(&sccpdevice_function);
	result |= iPbx.register_function(&sccpline_function);
	result |= iPbx.register_function(&sccpchannel_function);

	return result;
}

int sccp_unregister_dialplan_functions(void)
{
	int result = 0;

	/* Unregister applications functions */
	result = iPbx.unregister_application(calledparty_name);
	result |= iPbx.unregister_application(setmessage_name);
	result |= iPbx.unregister_application(prefcodec_name);

	/* Unregister dial plan functions */
	result |= iPbx.unregister_function(&sccpdevice_function);
	result |= iPbx.unregister_function(&sccpline_function);
	result |= iPbx.unregister_function(&sccpchannel_function);

	return result;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
