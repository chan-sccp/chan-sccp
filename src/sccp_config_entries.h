
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
#define DATA_TYPE(_w,_x,_y,_z) SCCP_CONFIG_DATATYPE_##_w, _x, _y,_z					// make DataTypeParsers easier to extend when they need extra parameters

/* straight forward data types */
#define TYPE_BOOLEAN DATA_TYPE(BOOLEAN, NULL, NULL, NULL)
#define TYPE_INT DATA_TYPE(INT, NULL, NULL, NULL)
#define TYPE_UINT DATA_TYPE(UINT, NULL, NULL, NULL)
#define TYPE_STRING DATA_TYPE(STRING, NULL, NULL, NULL)
#define TYPE_STRINGPTR DATA_TYPE(STRINGPTR, NULL, NULL, NULL)
#define TYPE_CHAR DATA_TYPE(CHAR, NULL, NULL, NULL)

/* complex data type parsers */

/* TYPE_ENUM2INT convert regular key index enum to int */
/* where _enum2int_cb should be passed an enum as string and will return it's int value (by config->size)*/
#define TYPE_ENUM2INT(_enum2int_cb,_fullkeys_cb) DATA_TYPE(ENUM2INT, NULL, _enum2int_cb,_fullkeys_cb)

/* where _enum2str_cb should be passed an enum as string and will return it's str value after being checked*/
//#define TYPE_ENUM2STR(_enum2str_cb,_fullkeys_cb) DATA_TYPE(ENUM2STR, NULL, _enum2int_cb,_fullkeys_cb)

/* where _csv2str_cb should be passed any comma seperated string and will return it's str value after being checked*/
//#define TYPE_CSV2STR(_csv2str_cb) DATA_TYPE(CSV2STR, NULL, _csv2str_cb, NULL)

/* custom parser */
#define TYPE_PARSER(_x) DATA_TYPE(PARSER, _x, NULL, NULL)
/*!
 * \brief List of SCCP Config Options for SCCP Globals
 */
/* *INDENT-OFF* */
static const SCCPConfigOption sccpGlobalConfigOptions[]={
	{"servername", 			G_OBJ_REF(servername), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		"Asterisk",			"show this name on the device registration\n"},
	{"keepalive", 			G_OBJ_REF(keepalive), 			TYPE_INT,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"60",				"Phone keep alive message every 60 secs. Used to check the voicemail and keep an open connection between server and phone (nat). \n"
																										  											"Don't set any lower than 60 seconds.\n"},
//#if CS_EXPERIMENTAL
//	{"debug", 			G_OBJ_REF(debug), 			TYPE_ENUM2INT(debugcat2int,debugcat_keys),					SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NOUPDATENEEDED,		"core",				"console debug level or categories\n"
//#else	
	{"debug", 			G_OBJ_REF(debug), 			TYPE_PARSER(sccp_config_parse_debug),						SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NOUPDATENEEDED,		"core",				"console debug level or categories\n"
//#endif	
																														  							"examples: debug = 11 | debug = mwi,event,core | debug = all | debug = none or 0\n"
																																					"possible categories: \n"
																																					"core, sccp, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey, indicate, pbx\n"
																																					"socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime, lock, newcode, high, all, none\n"},
	{"context", 			G_OBJ_REF(context), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"sccp",				"pbx dialplan context\n"},
	{"dateformat", 			G_OBJ_REF(dateformat), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"D.M.Y",			"M-D-Y in any order. Use M/D/YA (for 12h format)\n"},
	{"bindaddr", 			G_OBJ_REF(bindaddr), 			TYPE_PARSER(sccp_config_parse_ipaddress),					SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0",			"replace with the ip address of the asterisk server (RTP important param)\n"}, 
	{"port", 			G_OBJ_REF(bindaddr),			TYPE_PARSER(sccp_config_parse_port),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"2000",				"listen on port 2000 (Skinny, default)\n"},
	{"disallow", 			G_OBJ_REF(global_preferences),		TYPE_PARSER(sccp_config_parse_disallow_codec),					SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,		"",				"First disallow all codecs, for example 'all'\n"},
	{"allow", 			G_OBJ_REF(global_preferences),		TYPE_PARSER(sccp_config_parse_allow_codec),					SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,		"",				"Allow codecs in order of preference (Multiple lines allowed)\n"},
	{"deny", 			G_OBJ_REF(ha),	 			TYPE_PARSER(sccp_config_parse_deny),						SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0/0.0.0.0",		"Deny every address except for the only one allowed. example: '0.0.0.0/0.0.0.0'\n"},
	{"permit", 			G_OBJ_REF(ha), 				TYPE_PARSER(sccp_config_parse_permit),						SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,		"internal",			"Accept class C 192.168.1.0 example '192.168.1.0/255.255.255.0'\n"
																																					"You may have multiple rules for masking traffic.\n"
																																					"Rules are processed from the first to the last.\n"
																																					"This General rule is valid for all incoming connections. It's the 1st filter.\n"
																																					"using 'internal' will allow the 10.0.0.0, 172.16.0.0 and 192.168.0.0 networks\n"},
	{"quality_over_size",		G_OBJ_REF(prefer_quality_over_size),	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"When making decisions during codec selections prefer sound quality over packet size (default true)\n"},
	{"localnet", 			G_OBJ_REF(localaddr), 			TYPE_PARSER(sccp_config_parse_permit),						SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NEEDDEVICERESET,		"",				"All RFC 1918 addresses are local networks, example '192.168.1.0/255.255.255.0'\n"},
	{"externip", 			G_OBJ_REF(externip), 			TYPE_PARSER(sccp_config_parse_ipaddress),					SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NEEDDEVICERESET,		"",				"IP Address that we're going to notify in RTP media stream\n"},
	{"externhost", 			G_OBJ_REF(externhost), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"",				"Hostname (if dynamic) that we're going to notify in RTP media stream\n"},
	{"externrefresh", 		G_OBJ_REF(externrefresh), 		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"60",				"Expire time in seconds for the hostname (dns resolution)\n"},
	{"firstdigittimeout", 		G_OBJ_REF(firstdigittimeout), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"16",				"Dialing timeout for the 1st digit \n"},
	{"digittimeout", 		G_OBJ_REF(digittimeout), 		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"8",				"More digits\n"},
	{"digittimeoutchar", 		G_OBJ_REF(digittimeoutchar), 		TYPE_CHAR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"#",				"You can force the channel to dial with this char in the dialing state\n"},
	{"recorddigittimeoutchar", 	G_OBJ_REF(recorddigittimeoutchar), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"You can force the channel to dial with this char in the dialing state\n"},
	{"simulate_enbloc",	 	G_OBJ_REF(simulate_enbloc), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Use simulated enbloc dialing to speedup connection when dialing while onhook (older phones)\n"},
	{"autoanswer_ring_time", 	G_OBJ_REF(autoanswer_ring_time),	TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"1",				"Ringing time in seconds for the autoanswer, the default is 1\n"},
	{"autoanswer_tone", 		G_OBJ_REF(autoanswer_tone), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0x32",				"Autoanswer confirmation tone. For a complete list of tones: grep SKINNY_TONE sccp_protocol.h\n"
																														  							"not all the tones can be played in a connected state, so you have to try.\n"},
	{"remotehangup_tone", 		G_OBJ_REF(remotehangup_tone), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0x32",				"Passive hangup notification. 0 for none\n"},
	{"transfer_tone", 		G_OBJ_REF(transfer_tone), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0",				"Confirmation tone on transfer. Works only between SCCP devices\n"},
	{"callwaiting_tone", 		G_OBJ_REF(callwaiting_tone), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0x2d",				"Sets to 0 to disable the callwaiting tone\n"},
	{"callwaiting_interval", 	G_OBJ_REF(callwaiting_interval),	TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0",				"Callwaiting ring interval in seconds. Set to 0 to disable the callwaiting ringing interval (default: 0)\n"},
	{"musicclass", 			G_OBJ_REF(musicclass), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"default",			"Sets the default music on hold class\n"},
	{"language", 			G_OBJ_REF(language), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"en",				"Default language setting\n"},
#ifdef CS_MANAGER_EVENTS
	{"callevents", 			G_OBJ_REF(callevents), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Generate manager events when phone \n"
																																					"Performs events (e.g. hold)\n"},
#endif
	{"accountcode", 		G_OBJ_REF(accountcode), 		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"skinny",			"Accountcode to ease billing\n"},
	{"sccp_tos", 			G_OBJ_REF(sccp_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0x68",				"Sets the default sccp signaling packets Type of Service (TOS)  (defaults to 0x68 = 01101000 = 104 = DSCP:011010 = AF31)\n"
																																					"Others possible values : [CS?, AF??, EF], [0x??], [lowdelay, throughput, reliability, mincost(solaris)], none\n"},
	{"sccp_cos", 			G_OBJ_REF(sccp_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"4",				"sets the default sccp signaling packets Class of Service (COS) (defaults to 4)\n"},
	{"audio_tos", 			G_OBJ_REF(audio_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0xB8",				"sets the default audio/rtp packets Type of Service (TOS)       (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF)\n"},
	{"audio_cos", 			G_OBJ_REF(audio_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"6",				"sets the default audio/rtp packets Class of Service (COS)      (defaults to 6)\n"},
	{"video_tos", 			G_OBJ_REF(video_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0x88",				"sets the default video/rtp packets Type of Service (TOS)       (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41)\n"},
	{"video_cos", 			G_OBJ_REF(video_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"5",				"sets the default video/rtp packets Class of Service (COS)      (defaults to 5)\n"},
	{"echocancel", 			G_OBJ_REF(echocancel), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"sets the phone echocancel for all devices\n"},
	{"silencesuppression", 		G_OBJ_REF(silencesuppression), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"sets the silence suppression for all devices\n"
																																					"we don't have to trust the phone ip address, but the ip address of the connection\n"},
	{"trustphoneip", 		G_OBJ_REF(trustphoneip), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"The phone has a ip address. It could be private, so if the phone is behind NAT \n"},
	{"earlyrtp", 			G_OBJ_REF(earlyrtp), 			TYPE_PARSER(sccp_config_parse_earlyrtp),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"progress",			"valid options: none, offhook, dial, ringout and progress. default is progress.\n"
																																					"The audio stream will be open in the progress and connected state by default.\n"},
	{"dnd", 			G_OBJ_REF(dndmode), 			TYPE_PARSER(sccp_config_parse_dnd),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"reject",			"turn on the dnd softkey for all devices. Valid values are 'off', 'on' (busy signal), 'reject' (busy signal), 'silent' (ringer = silent)\n"},
	{"private", 			G_OBJ_REF(privacy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"permit the private function softkey\n"},
	{"mwilamp", 			G_OBJ_REF(mwilamp), 			TYPE_PARSER(sccp_config_parse_mwilamp),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"},
	{"mwioncall", 			G_OBJ_REF(mwioncall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Set the MWI on call.\n"},
	{"blindtransferindication", 	G_OBJ_REF(blindtransferindication),	TYPE_PARSER(sccp_config_parse_blindtransferindication),				SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"ring",				"moh or ring. the blind transfer should ring the caller or just play music on hold\n"},
	{"cfwdall", 			G_OBJ_REF(cfwdall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward ALL stuff and softkeys\n"},
	{"cfwdbusy", 			G_OBJ_REF(cfwdbusy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward BUSY stuff and softkeys\n"},
	{"cfwdnoanswer", 		G_OBJ_REF(cfwdnoanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward NOANSWER stuff and softkeys\n"},
	{"nat", 			G_OBJ_REF(nat), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"no",				"Global NAT support (default Off)\n"},
	{"directrtp", 			G_OBJ_REF(directrtp), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"This option allow devices to do direct RTP sessions (default Off)\n"},
	{"allowoverlap", 		G_OBJ_REF(useoverlap), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Enable overlap dialing support. If enabled, starts dialing immediately and sends remaing digits as DTMF/inband.\n"
																																					"Use with extreme caution as it is very dialplan and provider dependent. (Default is off)\n"},
	{"callgroup", 			G_OBJ_REF(callgroup), 			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"We are in caller groups 1,3,4. Valid for all lines\n"},
#ifdef CS_SCCP_PICKUP	
	{"pickupgroup", 		G_OBJ_REF(pickupgroup), 		TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"We can do call pick-p for call group 1,3,4,5. Valid for all lines\n"},
	{"pickupmodeanswer", 		G_OBJ_REF(pickupmodeanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"We can do call pick-p for call group 1,3,4,5. Valid for all lines\n"},
#endif
	{"amaflags", 			G_OBJ_REF(amaflags), 			TYPE_PARSER(sccp_config_parse_amaflags),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"Sets the default AMA flag code stored in the CDR record\n"},
	{"protocolversion", 		G_OBJ_REF(protocolversion), 		TYPE_STRING,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		"20",				"skinny version protocol. (excluding 12-14)\n"},
	{"callanswerorder", 		G_OBJ_REF(callanswerorder), 		TYPE_PARSER(sccp_config_parse_callanswerorder),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"oldestfirst",			"oldestfirst or lastestfirst\n"},
	{"regcontext", 			G_OBJ_REF(regcontext), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"sccpregistration",		"SCCP Lines will we added to this context in asterisk for Dundi lookup purposes. \n"
																																					"Don not set to a manually created context. The context will be autocreated. You can share the sip/iax context if you like.\n"},
#ifdef CS_SCCP_REALTIME
	{"devicetable", 		G_OBJ_REF(realtimedevicetable), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"sccpdevice",			"datebasetable for devices\n"},
	{"linetable", 			G_OBJ_REF(realtimelinetable), 		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"sccpline",			"datebasetable for lines\n"},
#endif
	{"meetme", 			G_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"enable/disable conferencing via meetme (on/off), make sure you have one of the meetme apps mentioned below activated in module.conf\n"
																																	"when switching meetme=on it will search for the first of these three possible meetme applications and set these defaults\n"
																																					" - {'MeetMe', 'qd'},\n"
																																					" - {'ConfBridge', 'Mac'},\n"
																																					" - {'Konference', 'MTV'}\n"},
	{"meetmeopts", 			G_OBJ_REF(meetmeopts), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"qxd",				"options to send the meetme application, defaults are dependent on meetme app see the list above\n"
																																					"Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see meetme specific documentation\n"},
	{"jbenable", 			G_OBJ_REF(global_jbconf),	 	TYPE_PARSER(sccp_config_parse_jbflags_enable),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				" Enables the use of a jitterbuffer on the receiving side of a\n"
																																					"sccp channel. Defaults to 'no'. An enabled jitterbuffer will\n"
																																					"be used only if the sending side can create and the receiving\n"
																																					"side can not accept jitter. The sccp channel can accept\n"
																																					"jitter, thus a jitterbuffer on the receive sccp side will be\n"
																																					"used only if it is forced and enabled.\n"},
	{"jbforce", 			G_OBJ_REF(global_jbconf),		TYPE_PARSER(sccp_config_parse_jbflags_force),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Forces the use of a jitterbuffer on the receive side of a sccp\n"
																																					"channel. Defaults to 'no'.\n"},
	{"jblog", 			G_OBJ_REF(global_jbconf),		TYPE_PARSER(sccp_config_parse_jbflags_log),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Enables jitterbuffer frame logging. Defaults to 'no'.\n"},
	{"jbmaxsize", 			G_OBJ_REF(global_jbconf.max_size),	TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"200",				"Max length of the jitterbuffer in milliseconds.\n"},
	{"jbresyncthreshold", 		G_OBJ_REF(global_jbconf.resync_threshold),TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"1000",				"Jump in the frame timestamps over which the jitterbuffer is\n"
																																					"resynchronized. Useful to improve the quality of the voice, with\n"
																																					"big jumps in/broken timestamps, usually sent from exotic devices\n"
																																					"and programs. Defaults to 1000.\n"},
	{"jbimpl", 			G_OBJ_REF(global_jbconf.impl),		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"fixed",			"Jitterbuffer implementation, used on the receiving side of a\n"
																																					"sccp channel. Two implementations are currently available\n"
																																					"- 'fixed' (with size always equals to jbmaxsize)\n"
																																					"- 'adaptive' (with variable size, actually the new jb of IAX2).\n"
																																					"Defaults to fixed.\n"},
	{"hotline_enabled", 		G_OBJ_REF(allowAnonymous), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				" Setting the hotline Feature on a device, will make it connect to a predefined extension as soon as the Receiver\n"
																																					"is picked up or the 'New Call' Button is pressed. No number has to be given. This works even on devices which \n"
																																					"have no entry in the config file or realtime database. \n"
																																					"The hotline function can be used in different circumstances, for example at a door, where you want people to be \n"
																																					"able to only call one number, or for unprovisioned phones to only be able to call the helpdesk to get their phone\n"
																																					"set up	If hotline_enabled = yes, any device which is not included in the configuration explicitly will be allowed \n"
																																					"to registered as a guest device. All such devices will register on a single shared line called 'hotline'.\n"},

	{"hotline_context",	offsetof(struct sccp_global_vars,hotline), offsize(struct sccp_line,context),	TYPE_PARSER(sccp_config_parse_hotline_context),	SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"sccp",				""},
	{"hotline_extension", 	offsetof(struct sccp_global_vars,hotline), offsize(sccp_hotline_t,exten),	TYPE_PARSER(sccp_config_parse_hotline_exten),	SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"111",				""},
	{"fallback",			G_OBJ_REF(token_fallback),		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Immediately fallback to primairy/master server when it becomes available (master/slave asterisk cluster) (TokenRequest)\n"
																																					"Possible values are: true/false/odd/even (odd/even uses the last digit of the MAC address to make the decision)\n"
																																					"Value can be changed online via CLI/AMI command \"fallback=[true/false]\"\n"},
	{"backoff_time", 		G_OBJ_REF(token_backoff_time),		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"60",				"Time to wait before re-asking to fallback to primairy server (Token Reject Backoff Time)\n"},
};

/*!
 * \brief List of SCCP Config Options for SCCP Devices
 */
static const SCCPConfigOption sccpDeviceConfigOptions[] = {
	{"name", 			0, 				0,	TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device name\n"},
	{"device", 			D_OBJ_REF(config_type),			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"device type\n"},
	{"devicetype", 			D_OBJ_REF(config_type),			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device type\n"},
	{"type", 			D_OBJ_REF(config_type),			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"used for device templates, value will be inherited.\n"},
	{"description", 		D_OBJ_REF(description),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"device description\n"},
	{"keepalive", 			D_OBJ_REF(keepalive), 			TYPE_INT,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"set keepalive to 60\n"},
	{"tzoffset", 			D_OBJ_REF(tz_offset), 			TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0",				"time zone offset\n"},
	{"disallow", 			D_OBJ_REF(preferences), 		TYPE_PARSER(sccp_config_parse_disallow_codec),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,				""},
	{"allow", 			D_OBJ_REF(preferences), 		TYPE_PARSER(sccp_config_parse_allow_codec),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,				""},
	{"transfer", 			D_OBJ_REF(transfer),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"enable or disable the transfer capability. It does remove the transfer softkey\n"},
	{"park", 			D_OBJ_REF(park),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"take a look to the compile how-to. Park stuff is not compiled by default\n"},
	{"cfwdall", 			D_OBJ_REF(cfwdall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"activate the call forward stuff and soft keys\n"},
	{"cfwdbusy", 			D_OBJ_REF(cfwdbusy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"allow call forward when line is busy\n"},
	{"cfwdnoanswer", 		D_OBJ_REF(cfwdnoanswer),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"allow call forward when line if not being answered\n"},
	{"dnd",	 			D_OBJ_REF(dndFeature.enabled),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"parameter 'dnd' is obsolete. This setting has moved to the line definition. To allow/disallow dnd you should use dndFeature in device and setup dnd per line.\n"},
	{"dndFeature",	 		D_OBJ_REF(dndFeature.enabled),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"allow usage do not disturb button\n"},
	{"dtmfmode", 			D_OBJ_REF(dtmfmode), 			TYPE_PARSER(sccp_config_parse_dtmfmode),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"inband",			"inband or outofband. outofband is the native cisco dtmf tone play.\n"
																																					"Some phone model does not play dtmf tones while connected (bug?), so the default is inband\n"},
	{"imageversion", 		D_OBJ_REF(imageversion), 		TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"useful to upgrade old firmwares (the ones that do not load *.xml from the tftp server)\n"},
	{"deny", 			D_OBJ_REF(ha),	 			TYPE_PARSER(sccp_config_parse_deny),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,	NULL,				"Same as general\n"},
	{"permit", 			D_OBJ_REF(ha), 				TYPE_PARSER(sccp_config_parse_permit),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,	NULL,				"This device can register only using this ip address\n"},
	{"audio_tos", 			D_OBJ_REF(audio_tos),			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 				SCCP_CONFIG_NEEDDEVICERESET, 		NULL,				"sets the audio/rtp packets Type of Service (TOS)  (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF)\n"
																																					"Others possible values : 0x??, lowdelay, throughput, reliability, mincost(solaris), none\n"},
	{"audio_cos", 			D_OBJ_REF(audio_cos),			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 				SCCP_CONFIG_NEEDDEVICERESET, 		NULL,				"sets the audio/rtp packets Class of Service (COS) (defaults to 6)\n"},
	{"video_tos", 			D_OBJ_REF(video_tos),			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 				SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"sets the video/rtp packets Type of Service (TOS)  (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41)\n"},
	{"video_cos", 			D_OBJ_REF(video_cos),			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT, 				SCCP_CONFIG_NEEDDEVICERESET, 		NULL,				"sets the video/rtp packets Class of Service (COS) (defaults to 5)\n"},
	{"trustphoneip", 		D_OBJ_REF(trustphoneip), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"The phone has a ip address. It could be private, so if the phone is behind NAT \n"
																																					"we don't have to trust the phone ip address, but the ip address of the connection\n"},
	{"nat", 			D_OBJ_REF(nat), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,				"Device NAT support (default Off)\n"},
	{"directrtp", 			D_OBJ_REF(directrtp), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"This option allow devices to do direct RTP sessions (default Off)								\n"},
	{"earlyrtp", 			D_OBJ_REF(earlyrtp), 			TYPE_PARSER(sccp_config_parse_earlyrtp),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"valid options: none, offhook, dial, ringout and progress. default is progress.\n"
																																					"The audio stream will be open in the progress and connected state by default.\n"},
	{"private", 			D_OBJ_REF(privacyFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"permit the private function softkey for this device\n"},
	{"privacy", 			D_OBJ_REF(privacyFeature),	 	TYPE_PARSER(sccp_config_parse_privacyFeature),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"permit the private function softkey for this device\n"},
	{"mwilamp",			D_OBJ_REF(mwilamp), 			TYPE_PARSER(sccp_config_parse_mwilamp),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"},
	{"mwioncall", 			D_OBJ_REF(mwioncall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the MWI on call.\n"},
	{"meetme", 			D_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable conferencing via app_meetme (on/off)\n"},
	{"meetmeopts", 			D_OBJ_REF(meetmeopts), 			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"options to send the app_meetme application (default 'qd' = quiet,dynamic pin)\n"
																																					"Other options (A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see app_meetme documentation\n"},
	{"softkeyset", 			D_OBJ_REF(softkeyDefinition),		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"use specified softkeyset with name softkeyset1\n"},
#ifdef CS_ADV_FEATURES
	{"useRedialMenu", 		D_OBJ_REF(useRedialMenu), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"show the redial phone book list instead of dialing the last number (adv_feature)\n"},
#endif
#ifdef CS_SCCP_PICKUP
	{"pickupexten", 		D_OBJ_REF(pickupexten), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"enable Pickup function to direct pickup an extension\n"},
	{"pickupcontext", 		D_OBJ_REF(pickupcontext), 		TYPE_PARSER(sccp_config_parse_context),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"sccp",				"context where direct pickup search for extensions. if not set it will be ignored.\n"},
	{"pickupmodeanswer", 		D_OBJ_REF(pickupmodeanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"on = asterisk way, the call has been answered when picked up\n"},
#endif
	{"monitor", 			D_OBJ_REF(monitorFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				""},
	{"allowoverlap", 		D_OBJ_REF(overlapFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Allow for Overlap dialing (Continue dialing after the first part of the number has already been send to the pstn)"},
	{"setvar", 			D_OBJ_REF(variables),			TYPE_PARSER(sccp_config_parse_variables),					SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
																																					"format setvar=param=value, for example setvar=sipno=12345678\n"},
	{"permithost", 			D_OBJ_REF(permithosts), 		TYPE_PARSER(sccp_config_parse_permithosts),					SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"permit/deny but by resolved hostname"},
	{"addon", 			D_OBJ_REF(addons),	 		TYPE_PARSER(sccp_config_parse_addons),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"One of 7914, 7915, 7916"},
	{"button", 			D_OBJ_REF(buttonconfig), 		TYPE_PARSER(sccp_config_parse_button),						SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,		SCCP_CONFIG_NEEDDEVICERESET,		NULL,				""},
	{"digittimeout", 		D_OBJ_REF(digittimeout), 		TYPE_INT,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"8",				"More digits\n"},
};

/*!
 * \brief List of SCCP Config Options for SCCP Lines
 */
static const SCCPConfigOption sccpLineConfigOptions[] = {
	{"name", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"line name\n"},
	{"line", 			0,	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"line name\n"},
	{"type", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"line\n"},
	{"id", 				L_OBJ_REF(id),				TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"id\n"},
	{"pin", 			L_OBJ_REF(pin), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"pin\n"},
	{"label", 			L_OBJ_REF(label), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED, 					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"label\n"},
	{"description", 		L_OBJ_REF(description),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"description\n"},
	{"context", 			L_OBJ_REF(context), 			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"pbx dialing context\n"},
	{"cid_name", 			L_OBJ_REF(cid_name), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"callerid name\n"},
	{"cid_num", 			L_OBJ_REF(cid_num), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"callerid number\n"},
	{"defaultSubscriptionId_name",	L_OBJ_REF(defaultSubscriptionId.name), 	TYPE_STRING,						 			SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Name used on a shared line when no name is specified on the line button for the device\n"},
	{"defaultSubscriptionId_number",L_OBJ_REF(defaultSubscriptionId.number),TYPE_STRING,						 			SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Number used on a shared line when no name is specified on the line button for the device\n"},
	{"callerid", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_OBSOLETE, 					SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"obsolete callerid param. Use cid_num and cid_name\n"},
	{"mailbox", 			L_OBJ_REF(mailboxes), 			TYPE_PARSER(sccp_config_parse_mailbox),						SCCP_CONFIG_FLAG_NONE,	 					SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"Mailbox to store messages in\n"},
	{"vmnum", 			L_OBJ_REF(vmnum), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Number to dial to get to the users Mailbox\n"},
	{"adhocNumber", 		L_OBJ_REF(adhocNumber), 		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Adhoc Number or Private-line automatic ring down (PLAR):\n"
																																					"Adhoc/PLAR circuits have statically configured endpoints and do not require the user dialing to connect calls.\n"
																																					" - The adhocNumber is dialed as soon as the Phone is taken off-hook or when the new-call button is pressed.\n"
																																					" - The number will not be dialed when choosing a line; so when you choose a line you can enter a number manually.\n"},
	{"meetme", 			L_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable conferencing via meetme, make sure you have one of the meetme apps mentioned below activated in module.conf.\n"
																																					"When switching meetme=on it will search for the first of these three possible meetme applications and set these defaults.\n"
																																					"Meetme=>'qd', ConfBridge=>'Mac', Konference=>'MTV'\n"},
	{"meetmenum",			L_OBJ_REF(meetmenum), 			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"This extension will receive meetme requests, SCCP_MEETME_ROOM channel variable will\n"
																																					"contain the room number dialed into simpleswitch (this parameter is going to be removed).\n"},
	{"meetmeopts", 			L_OBJ_REF(meetmeopts),			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"options to send the meetme application, defaults are dependent on meetme app see the list above.\n"
																																					"Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see conferencing app for specific documentation\n"},
	{"transfer", 			L_OBJ_REF(transfer),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"per line transfer capability\n"},
	{"incominglimit", 		L_OBJ_REF(incominglimit),		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"6",				"allow x number of incoming calls (call waiting)\n"},
	{"echocancel", 			L_OBJ_REF(echocancel),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the phone echocancel for this line\n"},
	{"silencesuppression",		L_OBJ_REF(silencesuppression),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the silence suppression for this line\n"},
	{"language", 			L_OBJ_REF(language),			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the language setting per line\n"},
	{"musicclass", 			L_OBJ_REF(musicclass),			TYPE_STRING,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the music on hold class per line\n"},
	{"accountcode", 		L_OBJ_REF(accountcode),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"accountcode for this line to make billing per call possible\n"},
	{"amaflags", 			L_OBJ_REF(amaflags),			TYPE_PARSER(sccp_config_parse_amaflags),					SCCP_CONFIG_FLAG_NONE,		 				SCCP_CONFIG_NOUPDATENEEDED,	 	NULL,				"sets the AMA flags stored in the CDR record for this line\n"},
	{"callgroup", 			L_OBJ_REF(callgroup),			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,		 				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the caller groups this line is a member of\n"},
	{"pickupgroup", 		L_OBJ_REF(pickupgroup),			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,		 				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the pickup groups this line is a member of (this phone can pickup calls from remote phones which are in this caller group\n"},
	{"trnsfvm", 			L_OBJ_REF(trnsfvm),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"extension to redirect the caller to for voice mail\n"},
	{"secondary_dialtone_digits", 	L_OBJ_REF(secondary_dialtone_digits),	TYPE_PARSER(sccp_config_parse_secondaryDialtoneDigits),				SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,	 	"",				"digits to indicate an external line to user (secondary dialtone) (max 9 digits)\n"},
	{"secondary_dialtone_tone", 	L_OBJ_REF(secondary_dialtone_tone),	TYPE_UINT,						 			SCCP_CONFIG_FLAG_NONE, 						SCCP_CONFIG_NOUPDATENEEDED, 		"0x22",				"outside dialtone frequency\n"},
	{"setvar", 			L_OBJ_REF(variables),			TYPE_PARSER(sccp_config_parse_variables),					SCCP_CONFIG_FLAG_NONE | SCCP_CONFIG_FLAG_MULTI_ENTRY,	 	SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
																																					"format setvar=param=value, for example setvar=sipno=12345678\n"},
	{"dnd", 			L_OBJ_REF(dndmode),			TYPE_PARSER(sccp_config_parse_dnd),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 				SCCP_CONFIG_NOUPDATENEEDED, 		"reject",			"allow setting dnd for this line. Valid values are 'off', 'on' (busy signal), 'reject' (busy signal), 'silent' (ringer = silent) or user to toggle on phone\n"},
	{"regexten", 			L_OBJ_REF(regexten),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"SCCP Lines will we added to the regcontext with this number for Dundi look up purpose\n"
																																					"If regexten is not filled in the line name (categoryname between []) will be used\n"},
};

/*!
 * \brief List of SCCP Config Options for SCCP SoftKey
 */
static const SCCPConfigOption sccpSoftKeyConfigOptions[] = {
	{"type",			0,	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		"softkeyset",								""},
	{"name", 			S_OBJ_REF(name),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,									"softkeyset name\n"},
	{"connected",			S_OBJ_REF(modes[0]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"hold,endcall,park,select,cfwdall,cfwdbusy,idivert",			""},
	{"onhold",			S_OBJ_REF(modes[1]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"resume,newcall,endcall,transfer,conflist,select,dirtrfr,idivert,meetme",""},
	{"ringin",			S_OBJ_REF(modes[2]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"answer,endcall,transvm,idivert",					""},
	{"offhook",			S_OBJ_REF(modes[3]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial,endcall,private,cfwdall,cfwdbusy,pickup,gpickup,meetme,barge",	""},
	{"conntrans",			S_OBJ_REF(modes[4]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"hold,endcall,transfer,conf,park,select,dirtrfr,meetme,cfwdall,cfwdbusy",""},
	{"digitsfoll",			S_OBJ_REF(modes[5]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"back,endcall",								""},
	{"connconf",			S_OBJ_REF(modes[6]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"conflist,endcall,join,hold",						""},
	{"ringout",			S_OBJ_REF(modes[7]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"endcall,transfer,cfwdall,idivert",					""},
	{"offhookfeat",			S_OBJ_REF(modes[8]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial,endcall",							""},
	{"onhint",			S_OBJ_REF(modes[9]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"newcall,pickup,barge",							""},
	{"onstealable",			S_OBJ_REF(modes[10]), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial,newcall,cfwdall,pickup,gpickup,dnd,intrcpt",			""},
};
/* *INDENT-ON* */
