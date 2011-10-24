
/*!
 * \file 	sccp_config_entries.h
 * \brief 	SCCP Config Entries Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note 	To find out more about the reload function see \ref sccp_config_reload
 * \remarks     Only methods directly related to chan-sccp configuration should be stored in this source file.
 *
 * $Date: 2010-11-17 18:10:34 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2154 $
 */

/* dyn config */

/*!
 * \brief List of SCCP Config Options for SCCP Globals
 */
static const SCCPConfigOption sccpGlobalConfigOptions[]={
/* *INDENT-OFF* */
  {"servername", 			G_OBJ_REF(servername), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"Asterisk",	NULL,			"show this name on the device registration\n"},
  {"keepalive", 			G_OBJ_REF(keepalive), 			SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"60",		NULL,			"Phone keep alive message every 60 secs. Used to check the voicemail and keep an open connection between server and phone (nat). \n"
																													"Don't set any lower than 60 seconds.\n"},
  {"debug", 				G_OBJ_REF(debug), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"core",		sccp_config_parse_debug,"console debug level or categories\n"
																													"examples: debug = 11 | debug = mwi,event,core | debug = all | debug = none or 0\n"
																													"possible categories: \n"
																													"core, sccp, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey, indicate, pbx\n"
																													"socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime, lock, newcode, high, all, none\n"},
  {"context", 				G_OBJ_REF(context), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"sccp",		sccp_config_parse_context,"pbx dialplan context\n"},
  {"dateformat", 			G_OBJ_REF(dateformat), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"D.M.Y",	NULL,			"M-D-Y in any order. Use M/D/YA (for 12h format)\n"},
  {"bindaddr", 				G_OBJ_REF(bindaddr), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0",	sccp_config_parse_ipaddress,			"replace with the ip address of the asterisk server (RTP important param)\n"}, 
  {"port", 				G_OBJ_REF(bindaddr),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"2000",		NULL,			"listen on port 2000 (Skinny, default)\n"},
  {"disallow", 				G_OBJ_REF(global_preferences),		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"",		sccp_config_parse_disallow_codec,"First disallow all codecs, for example 'all'\n"},
  {"allow", 				G_OBJ_REF(global_preferences),		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"",		sccp_config_parse_allow_codec,"Allow codecs in order of preference (Multiple lines allowed)\n"},
  {"deny", 				G_OBJ_REF(ha),	 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0/0.0.0.0",	sccp_config_parse_deny,	"Deny every address except for the only one allowed. example: '0.0.0.0/0.0.0.0'\n"},
  {"permit", 				G_OBJ_REF(ha), 				SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"internal",	sccp_config_parse_permit,"Accept class C 192.168.1.0 example '192.168.1.0/255.255.255.0'\n"
																													"You may have multiple rules for masking traffic.\n"
																													"Rules are processed from the first to the last.\n"
																													"This General rule is valid for all incoming connections. It's the 1st filter.\n"
																													"using 'internal' will allow the 10.0.0.0, 172.16.0.0 and 192.168.0.0 networks\n"},
  {"quality_over_size",			G_OBJ_REF(prefer_quality_over_size),	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"true",		NULL,			"When making decisions during codec selections prefer sound quality over packet size (default true)\n"},
  {"localnet", 				G_OBJ_REF(localaddr), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		" ",		sccp_config_parse_permit,"All RFC 1918 addresses are local networks, example '192.168.1.0/255.255.255.0'\n"},
  {"externip", 				G_OBJ_REF(externip), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"",		sccp_config_parse_ipaddress,"IP Address that we're going to notify in RTP media stream\n"},
  {"externhost", 			G_OBJ_REF(externhost), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"",		NULL,			"Hostname (if dynamic) that we're going to notify in RTP media stream\n"},
  {"externrefresh", 			G_OBJ_REF(externrefresh), 		SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"60",		NULL,		"Expire time in seconds for the hostname (dns resolution)\n"},
  {"firstdigittimeout", 		G_OBJ_REF(firstdigittimeout), 		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"16",		NULL,			"Dialing timeout for the 1st digit \n"},
  {"digittimeout", 			G_OBJ_REF(digittimeout), 		SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"8",		NULL,			"More digits\n"},
  {"digittimeoutchar", 			G_OBJ_REF(digittimeoutchar), 		SCCP_CONFIG_DATATYPE_CHAR,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"#",		NULL,			"You can force the channel to dial with this char in the dialing state\n"},
  {"recorddigittimeoutchar", 		G_OBJ_REF(recorddigittimeoutchar), 	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"false",	NULL,			"You can force the channel to dial with this char in the dialing state\n"},
  {"simulate_enbloc",	 		G_OBJ_REF(simulate_enbloc), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"true",		NULL,			"Use simulated enbloc dialing to speedup connection when dialing while onhook (older phones)\n"},
  {"autoanswer_ring_time", 		G_OBJ_REF(autoanswer_ring_time),	SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"1",		NULL,			"Ringing time in seconds for the autoanswer, the default is 1\n"},
  {"autoanswer_tone", 			G_OBJ_REF(autoanswer_tone), 		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"0x32",		NULL,			"Autoanswer confirmation tone. For a complete list of tones: grep SKINNY_TONE sccp_protocol.h\n"
																													"not all the tones can be played in a connected state, so you have to try.\n"},
  {"remotehangup_tone", 		G_OBJ_REF(remotehangup_tone), 		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"0x32",		NULL,			"Passive hangup notification. 0 for none\n"},
  {"transfer_tone", 			G_OBJ_REF(transfer_tone), 		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"0",		NULL,			"Confirmation tone on transfer. Works only between SCCP devices\n"},
  {"callwaiting_tone", 			G_OBJ_REF(callwaiting_tone), 		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"0x2d",		NULL,			"Sets to 0 to disable the callwaiting tone\n"},
  {"musicclass", 			G_OBJ_REF(musicclass), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"default",	NULL,			"Sets the default music on hold class\n"},
  {"language", 				G_OBJ_REF(language), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"en",		NULL,			"Default language setting\n"},
#ifdef CS_MANAGER_EVENTS
  {"callevents", 			G_OBJ_REF(callevents), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on ",		NULL,			"Generate manager events when phone \n"
																													"Performs events (e.g. hold)\n"},
#endif
  {"accountcode", 			G_OBJ_REF(accountcode), 		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"skinny",	NULL,			"Accountcode to ease billing\n"},
  {"sccp_tos", 				G_OBJ_REF(sccp_tos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0x68",		sccp_config_parse_tos,	"Sets the default sccp signaling packets Type of Service (TOS)  (defaults to 0x68 = 01101000 = 104 = DSCP:011010 = AF31)\n"
																													"Others possible values : [CS?, AF??, EF], [0x??], [lowdelay, throughput, reliability, mincost(solaris)], none\n"},
  {"sccp_cos", 				G_OBJ_REF(sccp_cos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"4",		sccp_config_parse_cos,	"sets the default sccp signaling packets Class of Service (COS) (defaults to 4)\n"},
  {"audio_tos", 			G_OBJ_REF(audio_tos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0xB8",		sccp_config_parse_tos,	"sets the default audio/rtp packets Type of Service (TOS)       (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF)\n"},
  {"audio_cos", 			G_OBJ_REF(audio_cos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"6",		sccp_config_parse_cos,	"sets the default audio/rtp packets Class of Service (COS)      (defaults to 6)\n"},
  {"video_tos", 			G_OBJ_REF(video_tos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0x88",		sccp_config_parse_tos,	"sets the default video/rtp packets Type of Service (TOS)       (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41)\n"},
  {"video_cos", 			G_OBJ_REF(video_cos), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"5",		sccp_config_parse_cos,	"sets the default video/rtp packets Class of Service (COS)      (defaults to 5)\n"},
  {"echocancel", 			G_OBJ_REF(echocancel), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",		NULL,			"sets the phone echocancel for all devices\n"},
  {"silencesuppression", 		G_OBJ_REF(silencesuppression), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off",		NULL,			"sets the silence suppression for all devices\n"
																													"we don't have to trust the phone ip address, but the ip address of the connection\n"},
  {"trustphoneip", 			G_OBJ_REF(trustphoneip), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"no",		NULL,			"The phone has a ip address. It could be private, so if the phone is behind NAT \n"},
  {"earlyrtp", 				G_OBJ_REF(earlyrtp), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"progress",	sccp_config_parse_earlyrtp,"valid options: none, offhook, dial, ringout and progress. default is progress.\n"
																													"The audio stream will be open in the progress and connected state by default.\n"},
  {"dnd", 				G_OBJ_REF(dndmode), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"reject",	sccp_config_parse_dnd,	"turn on the dnd softkey for all devices. Valid values are 'off', 'on' (busy signal), 'reject' (busy signal), 'silent' (ringer = silent)\n"},
  {"private", 				G_OBJ_REF(privacy), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",		NULL,			"permit the private function softkey\n"},
  {"mwilamp", 				G_OBJ_REF(mwilamp), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",		sccp_config_parse_mwilamp,"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"},
  {"mwioncall", 			G_OBJ_REF(mwioncall), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off",		NULL,			"Set the MWI on call.\n"},
  {"blindtransferindication", 		G_OBJ_REF(blindtransferindication),	SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"ring",		sccp_config_parse_blindtransferindication,"moh or ring. the blind transfer should ring the caller or just play music on hold\n"},
  {"cfwdall", 				G_OBJ_REF(cfwdall), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"on",		NULL,			"activate the callforward ALL stuff and softkeys\n"},
  {"cfwdbusy", 				G_OBJ_REF(cfwdbusy), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"on",		NULL,			"activate the callforward BUSY stuff and softkeys\n"},
  {"cfwdnoanswer", 			G_OBJ_REF(cfwdnoanswer), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"on",		NULL,			"activate the callforward NOANSWER stuff and softkeys\n"},
  {"nat", 				G_OBJ_REF(nat), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"off",		NULL,			"Global NAT support (default Off)\n"},
  {"directrtp", 			G_OBJ_REF(directrtp), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off",		NULL,			"This option allow devices to do direct RTP sessions (default Off)\n"},
  {"allowoverlap", 			G_OBJ_REF(useoverlap), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off ",		NULL,			"Enable overlap dialing support. If enabled, starts dialing immediately and sends remaing digits as DTMF/inband.\n"
																													"Use with extreme caution as it is very dialplan and provider dependent. (Default is off)\n"},
  {"callgroup", 			G_OBJ_REF(callgroup), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"",		sccp_config_parse_group,"We are in caller groups 1,3,4. Valid for all lines\n"},
#ifdef CS_SCCP_PICKUP
  {"pickupgroup", 			G_OBJ_REF(pickupgroup), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"",		sccp_config_parse_group,"We can do call pick-p for call group 1,3,4,5. Valid for all lines\n"},
  {"pickupmodeanswer", 			G_OBJ_REF(pickupmodeanswer), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"",		NULL,			"We can do call pick-p for call group 1,3,4,5. Valid for all lines\n"},
#endif
  {"amaflags", 				G_OBJ_REF(amaflags), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"",		sccp_config_parse_amaflags,"Sets the default AMA flag code stored in the CDR record\n"},
//  {"protocolversion", 			G_OBJ_REF(protocolversion), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_OBSOLETE,		SCCP_CONFIG_NOUPDATENEEDED,		"17",		sccp_config_parse_protocolversion,"skinny version protocol. Just for testing. 1 to 17 (excluding 12-14)\n"},
  {"protocolversion", 			G_OBJ_REF(protocolversion), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_OBSOLETE,		SCCP_CONFIG_NOUPDATENEEDED,		"20",		NULL,"skinny version protocol. (excluding 12-14)\n"},
  {"callanswerorder", 			G_OBJ_REF(callanswerorder), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"oldestfirst",	sccp_config_parse_callanswerorder,"oldestfirst or lastestfirst\n"},
  {"regcontext", 			G_OBJ_REF(regcontext), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"sccpregistration",sccp_config_parse_regcontext,	"SCCP Lines will we added to this context in asterisk for Dundi lookup purposes. \n"
																													"Don not set to a manually created context. The context will be autocreated. You can share the sip/iax context if you like.\n"},
#ifdef CS_SCCP_REALTIME
  {"devicetable", 			G_OBJ_REF(realtimedevicetable), 	SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"sccpdevice",	NULL,			"datebasetable for devices\n"},
  {"linetable", 			G_OBJ_REF(realtimelinetable), 		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"sccpline",	NULL,			"datebasetable for lines\n"},
#endif
  {"meetme", 				G_OBJ_REF(meetme), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",		NULL,			"enable/disable conferencing via meetme (on/off), make sure you have one of the meetme apps mentioned below activated in module.conf\n"
																													"when switching meetme=on it will search for the first of these three possible meetme applications and set these defaults\n"
																													" - {'MeetMe', 'qd'},\n"
																													" - {'ConfBridge', 'Mac'},\n"
																													" - {'Konference', 'MTV'}\n"},
  {"meetmeopts", 			G_OBJ_REF(meetmeopts), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"qxd",		NULL,			" options to send the meetme application, defaults are dependent on meetme app see the list above\n"
																													"Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see meetme specific documentation\n"},

/* \todo need a solution to set boolean flags in a bit array */
/*
#if ASTERISK_VERSION_NUMBER >= 10400
	// handle jb in configuration just let asterisk do that
	if (!pbx_jb_read_conf(&GLOB(global_jbconf), v->name, v->value)) {
		// Found a jb parameter
		continue;
	}
#endif
*/

//  {"jbenable", 				G_OBJ_REF(global_jbconf.flags) /*<<0*/, SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"no",		NULL,			" Enables the use of a jitterbuffer on the receiving side of a\n"
//																													"sccp channel. Defaults to 'no'. An enabled jitterbuffer will\n"
//																													"be used only if the sending side can create and the receiving\n"
//																													"side can not accept jitter. The sccp channel can accept\n"
//																													"jitter, thus a jitterbuffer on the receive sccp side will be\n"
//																													"used only if it is forced and enabled.\n"},
//  {"jbforce", 				G_OBJ_REF(global_jbconf.flags) /*<<1*/,	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"no",		NULL,			"Forces the use of a jitterbuffer on the receive side of a sccp\n"
//																													"channel. Defaults to 'no'.\n"},
//  {"jbmaxsize", 			G_OBJ_REF(global_jbconf.max_size),	SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"200",		NULL,			"Max length of the jitterbuffer in milliseconds.\n"},
//  {"jbresyncthreshold", 		G_OBJ_REF(global_jbconf.resync_threshold),SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"1000",		NULL,			"Jump in the frame timestamps over which the jitterbuffer is\n"
//																													"resynchronized. Useful to improve the quality of the voice, with\n"
//																													"big jumps in/broken timestamps, usually sent from exotic devices\n"
//																													"and programs. Defaults to 1000.\n"},
//  {"jbimpl", 				G_OBJ_REF(global_jbconf.impl),		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"fixed",	NULL,			"Jitterbuffer implementation, used on the receiving side of a\n"
//																													"sccp channel. Two implementations are currently available\n"
//																													"- 'fixed' (with size always equals to jbmaxsize)\n"
//																													"- 'adaptive' (with variable size, actually the new jb of IAX2).\n"
//																													"Defaults to fixed.\n"},
//  {"jblog", 				G_OBJ_REF(global_jbconf.flags) /*<<2*/,	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"no",		NULL,			" Enables jitterbuffer frame logging. Defaults to 'no'.\n"},
//

  {"hotline_enabled", 			G_OBJ_REF(allowAnonymous), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"no",		NULL,			" Setting the hotline Feature on a device, will make it connect to a predefined extension as soon as the Receiver\n"
																													"is picked up or the 'New Call' Button is pressed. No number has to be given. This works even on devices which \n"
																													"have no entry in the config file or realtime database. \n"
																													"The hotline function can be used in different circumstances, for example at a door, where you want people to be \n"
																													"able to only call one number, or for unprovisioned phones to only be able to call the helpdesk to get their phone\n"
																													"set up	If hotline_enabled = yes, any device which is not included in the configuration explicitly will be allowed \n"
																													"to registered as a guest device. All such devices will register on a single shared line called 'hotline'.\n"},

  {"hotline_context",			offsetof(struct sccp_global_vars,hotline), offsize(struct sccp_line,context),	SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"sccp",		sccp_config_parse_hotline_context,			""},
  {"hotline_extension", 		offsetof(struct sccp_global_vars,hotline), offsize(sccp_hotline_t,exten),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"111",		sccp_config_parse_hotline_exten,			""},
  {"fallback",				G_OBJ_REF(token_fallback),		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"false",	NULL, 			"Immediately fallback to primairy/master server when it becomes available (master/slave asterisk cluster) (TokenRequest)\n"
																													"Possible values are: true/false/odd/even (odd/even uses the last digit of the MAC address to make the decision)\n"
																													"Value can be changed online via CLI/AMI command \"fallback=[true/false]\"\n"},
  {"backoff_time", 			G_OBJ_REF(token_backoff_time),		SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"60",		NULL,			"Time to wait before re-asking to fallback to primairy server (Token Reject Backoff Time)\n"},
/* *INDENT-ON* */
};

/*!
 * \brief List of SCCP Config Options for SCCP Devices
 */
static const SCCPConfigOption sccpDeviceConfigOptions[] = {
/* *INDENT-OFF* */
  {"name", 				0, 				0,	SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"device name\n"},
  {"type", 				0, 				0,	SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"type name\n"},
  {"device", 				D_OBJ_REF(config_type),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"device type\n"},
  {"devicetype", 			D_OBJ_REF(config_type),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"device type\n"},
  {"type", 				D_OBJ_REF(config_type),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"used for device templates, value will be inherited.\n"},
  {"description", 			D_OBJ_REF(description),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"device description\n"},
  {"keepalive", 			D_OBJ_REF(keepalive), 			SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"set keepalive to 60\n"},
  {"tzoffset", 				D_OBJ_REF(tz_offset), 			SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		"0",	NULL, 				"time zone offset\n"},
  {"disallow", 				D_OBJ_REF(preferences), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_disallow_codec,""},
  {"allow", 				D_OBJ_REF(preferences), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_allow_codec,""},
  {"transfer", 				D_OBJ_REF(transfer),			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",	NULL, 				"enable or disable the transfer capability. It does remove the transfer softkey\n"},
  {"park", 				D_OBJ_REF(park),			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",	NULL, 				"take a look to the compile how-to. Park stuff is not compiled by default\n"},
  {"cfwdall", 				D_OBJ_REF(cfwdall), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		"off",	NULL, 				"activate the call forward stuff and soft keys\n"},
  {"cfwdbusy", 				D_OBJ_REF(cfwdbusy), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		"off",	NULL, 				"allow call forward when line is busy\n"},
  {"cfwdnoanswer", 			D_OBJ_REF(cfwdnoanswer),		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		"off",	NULL, 				"allow call forward when line if not being answered\n"},
  {"dnd",	 			D_OBJ_REF(dndFeature.enabled),		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_OBSOLETE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"parameter 'dnd' is obsolete. This setting has moved to the line definition. To allow/disallow dnd you should use dndFeature in device and setup dnd per line.\n"},
  {"dndFeature",	 		D_OBJ_REF(dndFeature.enabled),		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",	NULL, 				"allow usage do not disturb button\n"},
  {"dtmfmode", 				D_OBJ_REF(dtmfmode), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		"inband",sccp_config_parse_dtmfmode,	"inband or outofband. outofband is the native cisco dtmf tone play.\n"
                                                                                                                                                                                                                                        "Some phone model does not play dtmf tones while connected (bug?), so the default is inband\n"},
  {"imageversion", 			D_OBJ_REF(imageversion), 		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL,				"useful to upgrade old firmwares (the ones that do not load *.xml from the tftp server)\n"},
  {"deny", 				D_OBJ_REF(ha),	 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NEEDDEVICERESET,		NULL,	sccp_config_parse_deny,		"Same as general\n"},
  {"permit", 				D_OBJ_REF(ha), 				SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NEEDDEVICERESET,		NULL,	sccp_config_parse_permit,	"This device can register only using this ip address\n"},
  {"audio_tos", 			D_OBJ_REF(audio_tos),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 	SCCP_CONFIG_NEEDDEVICERESET, 		NULL,	sccp_config_parse_tos, 		"sets the audio/rtp packets Type of Service (TOS)  (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF)\n"
                                                                                                                                                                                                                                        "Others possible values : 0x??, lowdelay, throughput, reliability, mincost(solaris), none\n"},
  {"audio_cos", 			D_OBJ_REF(audio_cos),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 	SCCP_CONFIG_NEEDDEVICERESET, 		NULL,	sccp_config_parse_cos, 		"sets the audio/rtp packets Class of Service (COS) (defaults to 6)\n"},
  {"video_tos", 			D_OBJ_REF(video_tos),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 	SCCP_CONFIG_NEEDDEVICERESET,		NULL, 	sccp_config_parse_tos, 		"sets the video/rtp packets Type of Service (TOS)  (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41)\n"},
  {"video_cos", 			D_OBJ_REF(video_cos),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 	SCCP_CONFIG_NEEDDEVICERESET, 		NULL,	sccp_config_parse_cos, 		"sets the video/rtp packets Class of Service (COS) (defaults to 5)\n"},
  {"trustphoneip", 			D_OBJ_REF(trustphoneip), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"The phone has a ip address. It could be private, so if the phone is behind NAT \n"
                                                                                                                                                                                                                                        "we don't have to trust the phone ip address, but the ip address of the connection\n"},
  {"nat", 				D_OBJ_REF(nat), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Device NAT support (default Off)\n"},
  {"directrtp", 			D_OBJ_REF(directrtp), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"This option allow devices to do direct RTP sessions (default Off)								\n"},
  {"earlyrtp", 				D_OBJ_REF(earlyrtp), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_earlyrtp, 	"valid options: none, offhook, dial, ringout and progress. default is progress.\n"
                                                                                                                                                                                                                                        "The audio stream will be open in the progress and connected state by default.\n"},
  {"private", 				D_OBJ_REF(privacyFeature.enabled), 	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"permit the private function softkey for this device\n"},
  {"privacy", 				D_OBJ_REF(privacyFeature),	 	SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_privacyFeature,"permit the private function softkey for this device\n"},
  {"mwilamp",				D_OBJ_REF(mwilamp), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_mwilamp, 	"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"},
  {"mwioncall", 			D_OBJ_REF(mwioncall), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Set the MWI on call.\n"},
  {"meetme", 				D_OBJ_REF(meetme), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"enable/disable conferencing via app_meetme (on/off)\n"},
  {"meetmeopts", 			D_OBJ_REF(meetmeopts), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"options to send the app_meetme application (default 'qd' = quiet,dynamic pin)\n"
                                                                                                                                                                                                                                        "Other options (A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see app_meetme documentation\n"},
  {"softkeyset", 			D_OBJ_REF(softkeyDefinition),		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 			"use specified softkeyset with name softkeyset1\n"},
#ifdef CS_ADV_FEATURES
  {"useRedialMenu", 			D_OBJ_REF(useRedialMenu), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off",	NULL, 				"show the redial phone book list instead of dialing the last number (adv_feature)\n"},
#endif
#ifdef CS_SCCP_PICKUP
  {"pickupexten", 			D_OBJ_REF(pickupexten), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"off",	NULL, 				"enable Pickup function to direct pickup an extension\n"},
  {"pickupcontext", 			D_OBJ_REF(pickupcontext), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"sccp",	sccp_config_parse_context, 				"context where direct pickup search for extensions. if not set it will be ignored.\n"},
  {"pickupmodeanswer", 			D_OBJ_REF(pickupmodeanswer), 		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"on",	NULL, 				"on = asterisk way, the call has been answered when picked up\n"},
#endif
  {"monitor", 				D_OBJ_REF(monitorFeature.enabled), 	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				""},
  {"allowoverlap", 			D_OBJ_REF(overlapFeature.enabled), 	SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				""},
  {"setvar", 				D_OBJ_REF(variables),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,		 	SCCP_CONFIG_NOUPDATENEEDED, 		NULL, 	sccp_config_parse_variables,	"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
                                                                                                                                                                                                                                        "format setvar=param=value, for example setvar=sipno=12345678\n"},
  {"permithost", 			D_OBJ_REF(permithosts), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	sccp_config_parse_permithosts,	""},
  {"addon", 				D_OBJ_REF(addons),	 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	sccp_config_parse_addons,	""},
  {"dtmfmode", 				D_OBJ_REF(dtmfmode),	 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_dtmfmode,	""},
  {"button", 				D_OBJ_REF(buttonconfig), 		SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NEEDDEVICERESET,		NULL,	sccp_config_parse_button,	""},
  {"digittimeout", 			D_OBJ_REF(digittimeout), 		SCCP_CONFIG_DATATYPE_INT,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		"8",	NULL,				"More digits\n"},
/* *INDENT-ON* */
};

/*!
 * \brief List of SCCP Config Options for SCCP Lines
 */
static const SCCPConfigOption sccpLineConfigOptions[] = {
/* *INDENT-OFF* */
  {"name", 				0, 	0, 				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"line name\n"},
  {"line", 				0,	0, 				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"line name\n"},
  {"type", 				0, 	0, 				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"line\n"},
  {"id", 				L_OBJ_REF(id),				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"id\n"},
  {"pin", 				L_OBJ_REF(pin), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_REQUIRED,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"pin\n"},
  {"label", 				L_OBJ_REF(label), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_REQUIRED, 		SCCP_CONFIG_NEEDDEVICERESET,		NULL,	NULL, 				"label\n"},
  {"description", 			L_OBJ_REF(description),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"description\n"},
  {"context", 				L_OBJ_REF(context), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"pbx dialing context\n"},
  {"cid_name", 				L_OBJ_REF(cid_name), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_REQUIRED,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"callerid name\n"},
  {"cid_num", 				L_OBJ_REF(cid_num), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_REQUIRED,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"callerid number\n"},
  {"defaultSubscriptionId_name",	L_OBJ_REF(defaultSubscriptionId.name), 	SCCP_CONFIG_DATATYPE_STRING, 	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Name used on a shared line when no name is specified on the line button for the device\n"},
  {"defaultSubscriptionId_number",	L_OBJ_REF(defaultSubscriptionId.number),SCCP_CONFIG_DATATYPE_STRING, 	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Number used on a shared line when no name is specified on the line button for the device\n"},
  {"callerid", 				0, 	0, 				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_OBSOLETE, 		SCCP_CONFIG_NOUPDATENEEDED, 		NULL, 	NULL, 				"obsolete callerid param. Use cid_num and cid_name\n"},
  {"mailbox", 				L_OBJ_REF(mailboxes), 			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,	 		SCCP_CONFIG_NOUPDATENEEDED, 		NULL, 	sccp_config_parse_mailbox, 	"Mailbox to store messages in\n"},
  {"vmnum", 				L_OBJ_REF(vmnum), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Number to dial to get to the users Mailbox\n"},
  {"adhocNumber", 			L_OBJ_REF(adhocNumber), 		SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Adhoc Number or Private-line automatic ring down (PLAR):\n"
                                                                                                                                                                                                                                        "Adhoc/PLAR circuits have statically configured endpoints and do not require the user dialing to connect calls.\n"
                                                                                                                                                                                                                                        " - The adhocNumber is dialed as soon as the Phone is taken off-hook or when the new-call button is pressed.\n"
                                                                                                                                                                                                                                        " - The number will not be dialed when choosing a line; so when you choose a line you can enter a number manually.\n"},
  {"meetme", 				L_OBJ_REF(meetme), 			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"enable/disable conferencing via meetme, make sure you have one of the meetme apps mentioned below activated in module.conf.\n"
                                                                                                                                                                                                                                        "When switching meetme=on it will search for the first of these three possible meetme applications and set these defaults.\n"
                                                                                                                                                                                                                                        "Meetme=>'qd', ConfBridge=>'Mac', Konference=>'MTV'\n"},
  {"meetmenum",				L_OBJ_REF(meetmenum), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"This extension will receive meetme requests, SCCP_MEETME_ROOM channel variable will\n"
					                                                                                                                                                                                                "contain the room number dialed into simpleswitch (this parameter is going to be removed).\n"},
  {"meetmeopts", 			L_OBJ_REF(meetmeopts),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"options to send the meetme application, defaults are dependent on meetme app see the list above.\n"
                                                                                                                                                                                                                                        "Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see conferencing app for specific documentation\n"},
  {"transfer", 				L_OBJ_REF(transfer),			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"per line transfer capability\n"},
  {"incominglimit", 			L_OBJ_REF(incominglimit),		SCCP_CONFIG_DATATYPE_UINT,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"6",	NULL, 				"allow x number of incoming calls (call waiting)\n"},
  {"echocancel", 			L_OBJ_REF(echocancel),			SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"sets the phone echocancel for this line\n"},
  {"silencesuppression",		L_OBJ_REF(silencesuppression),		SCCP_CONFIG_DATATYPE_BOOLEAN,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"sets the silence suppression for this line\n"},
  {"language", 				L_OBJ_REF(language),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"sets the language setting per line\n"},
  {"musicclass", 			L_OBJ_REF(musicclass),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"sets the music on hold class per line\n"},
  {"accountcode", 			L_OBJ_REF(accountcode),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"accountcode for this line to make billing per call possible\n"},
  {"amaflags", 				L_OBJ_REF(amaflags),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,		 	SCCP_CONFIG_NOUPDATENEEDED,	 	NULL, 	sccp_config_parse_amaflags, 	"sets the AMA flags stored in the CDR record for this line\n"},
  {"callgroup", 			L_OBJ_REF(callgroup),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,		 	SCCP_CONFIG_NOUPDATENEEDED,		NULL,	sccp_config_parse_group,	"sets the caller groups this line is a member of\n"},
  {"pickupgroup", 			L_OBJ_REF(pickupgroup),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,		 	SCCP_CONFIG_NOUPDATENEEDED,		NULL, 	sccp_config_parse_group,	"sets the pickup groups this line is a member of (this phone can pickup calls from remote phones which are in this caller group\n"},
  {"trnsfvm", 				L_OBJ_REF(trnsfvm),			SCCP_CONFIG_DATATYPE_STRINGPTR,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"extension to redirect the caller to for voice mail\n"},
  {"secondary_dialtone_digits", 	L_OBJ_REF(secondary_dialtone_digits),	SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,	 	"9", 	sccp_config_parse_secondaryDialtoneDigits, 	"digits to indicate an external line to user (secondary dialtone) (max 9 digits)\n"},
  {"secondary_dialtone_tone", 		L_OBJ_REF(secondary_dialtone_tone),	SCCP_CONFIG_DATATYPE_UINT, 	SCCP_CONFIG_FLAG_NONE, 			SCCP_CONFIG_NOUPDATENEEDED, 		"0x22", NULL,			 	"outside dialtone frequency\n"},
  {"setvar", 				L_OBJ_REF(variables),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_NONE,		 	SCCP_CONFIG_NOUPDATENEEDED, 		NULL, 	sccp_config_parse_variables,	"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
                                                                                                                                                                                                                                        "format setvar=param=value, for example setvar=sipno=12345678\n"},
  {"dnd", 				L_OBJ_REF(dndmode),			SCCP_CONFIG_DATATYPE_GENERIC,	SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 	SCCP_CONFIG_NOUPDATENEEDED, 		"reject", sccp_config_parse_dnd, 		"allow setting dnd for this line. Valid values are 'off', 'on' (busy signal), 'reject' (busy signal), 'silent' (ringer = silent) or user to toggle on phone\n"},
  {"regexten", 				L_OBJ_REF(regexten),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"SCCP Lines will we added to the regcontext with this number for Dundi look up purpose\n"
                                                                                                                                                                                                                                        "If regexten is not filled in the line name (categoryname between []) will be used\n"},
/* begin deprecated / obsolete test data */
  {"test1", 				L_OBJ_REF(regexten),			SCCP_CONFIG_DATATYPE_STRING, 	SCCP_CONFIG_FLAG_OBSOLETE,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Parameter 'test' is now obsolete, please use parameter 'test3', see CHANGES and or documentation\n"},
  {"test2", 				L_OBJ_REF(regexten),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_DEPRECATED,		SCCP_CONFIG_NOUPDATENEEDED,		NULL,	NULL, 				"Parameter 'test' is now deprecated, please use parameter 'test3' in the future, see CHANGES and or documentation\n"},
/* end test data */
/* *INDENT-ON* */
};

/*!
 * \brief List of SCCP Config Options for SCCP SoftKey
 */
static const SCCPConfigOption sccpSoftKeyConfigOptions[] = {
  {"type",				0,	0, 				SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_IGNORE,		SCCP_CONFIG_NOUPDATENEEDED,		"softkeyset",								NULL, 				""},
  {"name", 				S_OBJ_REF(name),			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		NULL,									NULL, 				"softkeyset name\n"},
  {"connected",				S_OBJ_REF(modes[0]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"hold,endcall,park,select,cfwdall,cfwdbusy,idivert",			NULL, 				""},
  {"onhold",				S_OBJ_REF(modes[1]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"resume,newcall,endcall,transfer,conflist,select,dirtrfr,idivert,meetme",NULL, 				""},
  {"ringin",				S_OBJ_REF(modes[2]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"answer,endcall,transvm,idivert",					NULL, 				""},
  {"offhook",				S_OBJ_REF(modes[3]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"redial,endcall,private,cfwdall,cfwdbusy,pickup,gpickup,meetme,barge",	NULL, 				""},
  {"conntrans",				S_OBJ_REF(modes[4]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"hold,endcall,transfer,conf,park,select,dirtrfr,meetme,cfwdall,cfwdbusy",NULL, 				""},
  {"digitsfoll",			S_OBJ_REF(modes[5]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"back,endcall",								NULL, 				""},
  {"connconf",				S_OBJ_REF(modes[6]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"conflist,endcall,join,hold",						NULL, 				""},
  {"ringout",				S_OBJ_REF(modes[7]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"endcall,transfer,cfwdall,idivert",					NULL, 				""},
  {"offhookfeat",			S_OBJ_REF(modes[8]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"redial,endcall",							NULL, 				""},
  {"onhint",				S_OBJ_REF(modes[9]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"newcall,pickup,barge",							NULL, 				""},
  {"onstealable",			S_OBJ_REF(modes[10]), 			SCCP_CONFIG_DATATYPE_STRING,	SCCP_CONFIG_FLAG_NONE,			SCCP_CONFIG_NOUPDATENEEDED,		"redial,newcall,cfwdall,pickup,gpickup,dnd,intrcpt",			NULL, 				""},
};

