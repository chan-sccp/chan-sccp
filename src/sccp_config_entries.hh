/*!
 * \file        sccp_config_entries.hh
 * \brief       SCCP Config Entries Header
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \remarks     Only paramaters directly related to chan-sccp configuration should be stored in this source file.
 */

/* dyn config */
#define DATA_TYPE(_v,_w,_x,_y,_z) SCCP_CONFIG_DATATYPE_##_v, _w, _x, _y,_z						// make DataTypeParsers easier to extend when they need extra parameters

/* straight forward data types */
#define TYPE_BOOLEAN DATA_TYPE(BOOLEAN, NULL, NULL, NULL, NULL)
#define TYPE_INT DATA_TYPE(INT, NULL, NULL, NULL, NULL)
#define TYPE_UINT DATA_TYPE(UINT, NULL, NULL, NULL, NULL)
#define TYPE_STRING DATA_TYPE(STRING, NULL, NULL, NULL, NULL)
#define TYPE_STRINGPTR DATA_TYPE(STRINGPTR, NULL, NULL, NULL, NULL)
#define TYPE_CHAR DATA_TYPE(CHAR, NULL, NULL, NULL, NULL)

/* custom parser */
//#define TYPE_PARSER(_x) DATA_TYPE(PARSER, _x, NULL, #_x)
#define TYPE_PARSER(_x) DATA_TYPE(PARSER, _x, NULL, NULL, #_x)
#define TYPE_ENUM(_x,_y) DATA_TYPE(ENUM, NULL, _x##_##_y##_str2intval, _x##_##_y##_all_entries, NULL)

#define SCCP_CONFIG_REVISION "$Revision$"

/*!
 * \brief List of SCCP Config Options for SCCP Globals
 */
/* *INDENT-OFF* */
static const SCCPConfigOption sccpGlobalConfigOptions[]={
	{"debug", 			G_OBJ_REF(debug), 			TYPE_PARSER(sccp_config_parse_debug),						SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NOUPDATENEEDED,		"core",				"console debug level or categories\n"
																														  							"examples: debug = 11; debug = mwi,event,core; debug = all; debug = none or 0\n"
																																					"possible categories:\n"
																																					"core, hint, rtp, device, line, action, channel, cli, config, feature, feature_button, softkey, indicate, pbx\n"
																																					"socket, mwi, event, adv_feature, conference, buttontemplate, speeddial, codec, realtime, lock, parkinglot, newcode, high, all, none\n"},
	{"servername", 			G_OBJ_REF(servername), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		"Asterisk",			"show this name on the device registration\n"},
	{"keepalive", 			G_OBJ_REF(keepalive), 			TYPE_UINT,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"60",				"Phone keep alive message every 60 secs. Used to check the voicemail and keep an open connection between server and phone (nat).\n"
																										  											"Don't set any lower than 60 seconds.\n"},
	{"context", 			G_OBJ_REF(context), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"default",			"pbx dialplan context\n"},
	{"dateformat", 			G_OBJ_REF(dateformat), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"M/D/Y",			"M-D-Y in any order. Use M/D/YA (for 12h format)\n"},
	{"bindaddr", 			G_OBJ_REF(bindaddr), 			TYPE_PARSER(sccp_config_parse_ipaddress),					SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0",			"replace with the ip address of the asterisk server (RTP important param)\n"}, 
	{"port", 			G_OBJ_REF(bindaddr),			TYPE_PARSER(sccp_config_parse_port),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"2000",				"port to listen on (Skinny default:2000)\n"},
#ifdef HAVE_OPENSSL
	{"secbindaddr", 		G_OBJ_REF(secbindaddr),			TYPE_PARSER(sccp_config_parse_ipaddress),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0",			"ip-address to use for for secure ssl/tls connections\n"}, 
	{"secport", 			G_OBJ_REF(secbindaddr),			TYPE_PARSER(sccp_config_parse_port),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"2443",				"secure port to list on (Skinny default:2443)\n"},
	{"certfile",			G_OBJ_REF(cert_file),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"security certificate file (search path:Asterisk etc directory). If this field starts with '/' the absolute path will be used.\n"},
#endif
	{"disallow|allow", 		G_OBJ_REF(global_preferences),		TYPE_PARSER(sccp_config_parse_codec_preferences),				SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NEEDDEVICERESET,		"all|ulaw,alaw",		"First disallow all codecs, for example 'all', then allow codecs in order of preference (Multiple lines allowed)\n"},
	{"deny|permit", 		G_OBJ_REF(ha),	 			TYPE_PARSER(sccp_config_parse_deny_permit),					SCCP_CONFIG_FLAG_REQUIRED | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0/0.0.0.0|internal",	"Deny every address except for the only one allowed. example: '0.0.0.0/0.0.0.0'\n"
																																					"Accept class C 192.168.1.0 example '192.168.1.0/255.255.255.0'\n"
																																					"You may have multiple rules for masking traffic.\n"
																																					"Rules are processed from the first to the last.\n"
																																					"This General rule is valid for all incoming connections. It's the 1st filter.\n"
																																					"using 'internal' will allow the 10.0.0.0, 172.16.0.0 and 192.168.0.0 networks\n"},
	{"localnet", 			G_OBJ_REF(localaddr), 			TYPE_PARSER(sccp_config_parse_deny_permit),					SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NEEDDEVICERESET,		"internal",			"All RFC 1918 addresses are local networks, example '192.168.1.0/255.255.255.0'\n"},
	{"externip", 			G_OBJ_REF(externip), 			TYPE_PARSER(sccp_config_parse_ipaddress),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0.0.0.0",			"External IP Address of the firewall, required in case the PBX is running on a seperate host behind it. IP Address that we're going to use when setting up the RTP media stream for the pbx source address.\n"},
	{"externhost", 			G_OBJ_REF(externhost), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,  					SCCP_CONFIG_NEEDDEVICERESET,		"",				"Resolve Hostname (if dynamic) that we're going to resolve when setting up the RTP media stream (only active if externip=0.0.0.0 and host is natted.)\n"},
	{"externrefresh", 		G_OBJ_REF(externrefresh), 		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,  					SCCP_CONFIG_NEEDDEVICERESET,		"60",				"Expire time in seconds for the hostname (dns resolution)\n"},
	{"firstdigittimeout", 		G_OBJ_REF(firstdigittimeout), 		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"16",				"Dialing timeout for the 1st digit\n"},
	{"digittimeout", 		G_OBJ_REF(digittimeout), 		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"8",				"More digits\n"},
	{"digittimeoutchar", 		G_OBJ_REF(digittimeoutchar), 		TYPE_CHAR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"#",				"You can force the channel to dial with this char in the dialing state\n"},
	{"recorddigittimeoutchar", 	G_OBJ_REF(recorddigittimeoutchar), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"You can force the channel to dial with this char in the dialing state\n"},
	{"simulate_enbloc",	 	G_OBJ_REF(simulate_enbloc), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Use simulated enbloc dialing to speedup connection when dialing while onhook (older phones)\n"},
	{"ringtype",		 	G_OBJ_REF(ringtype),			TYPE_ENUM(skinny,ringtype),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"outside",			"Ringtype for incoming calls (default='outside')\n"},
	{"autoanswer_ring_time", 	G_OBJ_REF(autoanswer_ring_time),	TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"1",				"Ringing time in seconds for the autoanswer.\n"},
	{"autoanswer_tone", 		G_OBJ_REF(autoanswer_tone), 		TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Zip",				"Autoanswer confirmation tone. For a complete list of tones: `grep SKINNY_TONE_ sccp_enum.in.`\n"
																														  							"not all the tones can be played in a connected state, so you have to try.\n"},
	{"remotehangup_tone", 		G_OBJ_REF(remotehangup_tone), 		TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Zip",				"Passive hangup notification. 0 for none\n"},
	{"transfer", 			G_OBJ_REF(transfer),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"enable or disable the transfer capability. It does remove the transfer softkey (you can do that manually)\n"},
	{"transfer_tone", 		G_OBJ_REF(transfer_tone), 		TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0",				"Confirmation tone on transfer. Works only between SCCP devices\n"},
	{"transfer_on_hangup",		G_OBJ_REF(transfer_on_hangup), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Complete transfer on hangup, without pressing transfer a second time.\n"
																																					"Will complete transfer, when the transferer puts the receiver on hook, after the destination has been reached.\n"
																																					"To cancel the transfer, either press resume on the transfered channel, press the 'endcall' softkey, or have the receiving party hangup first.\n"},
	{"dnd_tone", 			G_OBJ_REF(dnd_tone),	 		TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Silence",			"Sets to 0 to disable the dnd tone\n"},
	{"callwaiting_tone", 		G_OBJ_REF(callwaiting_tone), 		TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Call Waiting Tone",		"Sets to 0 to disable the callwaiting tone\n"},
	{"callwaiting_interval", 	G_OBJ_REF(callwaiting_interval),	TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"0",				"Callwaiting ring interval in seconds. Set to 0 to disable the callwaiting ringing interval.\n"},
	{"musicclass", 			G_OBJ_REF(musicclass), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"default",			"Sets the default music on hold class\n"},
	{"language", 			G_OBJ_REF(language), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"en",				"Default language setting\n"},
#ifdef CS_MANAGER_EVENTS
	{"callevents", 			G_OBJ_REF(callevents), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Generate manager events when phone\n"
																																					"Performs events (e.g. hold)\n"},
#endif
	{"accountcode", 		G_OBJ_REF(accountcode), 		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"skinny",			"Accountcode to ease billing\n"},
	{"sccp_tos", 			G_OBJ_REF(sccp_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0x68",				"Sets the default sccp signaling packets Type of Service (TOS)  (defaults to 0x68 = 01101000 = 104 = DSCP:011010 = AF31)\n"
																																					"Others possible values : [CS?, AF??, EF], [0x??], [lowdelay, throughput, reliability, mincost(solaris)], none\n"},
	{"sccp_cos", 			G_OBJ_REF(sccp_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"4",				"sets the default sccp signaling packets Class of Service (COS).\n"},
	{"audio_tos", 			G_OBJ_REF(audio_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0xB8",				"sets the default audio/rtp packets Type of Service (TOS) (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF)\n"},
	{"audio_cos", 			G_OBJ_REF(audio_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"6",				"sets the default audio/rtp packets Class of Service (COS).\n"},
	{"video_tos", 			G_OBJ_REF(video_tos), 			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0x88",				"sets the default video/rtp packets Type of Service (TOS) (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41)\n"},
	{"video_cos", 			G_OBJ_REF(video_cos), 			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"5",				"sets the default video/rtp packets Class of Service (COS).\n"},
	{"echocancel", 			G_OBJ_REF(echocancel), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"sets the phone echocancel for all devices\n"},
	{"silencesuppression", 		G_OBJ_REF(silencesuppression), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"sets the silence suppression for all devices\n"
																																					"we don't have to trust the phone ip address, but the ip address of the connection\n"},
	{"trustphoneip", 		G_OBJ_REF(trustphoneip), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		"no",				"(OBSOLETE) The phone has a ip address. It could be private, so if the phone is behind NAT.\n"},
	{"earlyrtp", 			G_OBJ_REF(earlyrtp), 			TYPE_ENUM(sccp,earlyrtp),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"progress",			"valid options: none, offhook, immediate, dial, ringout and progress.\n"
																																					"The audio stream will be open in the 'progress' state by default. Immediate forces overlap dialing.\n"},
	{"dnd", 			G_OBJ_REF(dndFeature), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		"on",				"(OBSOLETE) Turn on the dnd softkey for all devices. Valid values are 'off', 'on' (replaced by in favor of 'dndFeature').\n"},
	{"dndFeature",			G_OBJ_REF(dndFeature), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"on",				"Turn on the dnd softkey for all devices. Valid values are 'off', 'on'.\n"},
	{"private", 			G_OBJ_REF(privacy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"permit the private function softkey\n"},
	{"mwilamp", 			G_OBJ_REF(mwilamp), 			TYPE_ENUM(skinny,lampmode),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"on",				"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"}, 
	{"mwioncall", 			G_OBJ_REF(mwioncall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Set the MWI on call.\n"},
	{"blindtransferindication", 	G_OBJ_REF(blindtransferindication),	TYPE_ENUM(sccp,blindtransferindication),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"ring",				"moh or ring. the blind transfer should ring the caller or just play music on hold\n"},
	{"cfwdall", 			G_OBJ_REF(cfwdall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward ALL stuff and softkeys\n"},
	{"cfwdbusy", 			G_OBJ_REF(cfwdbusy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward BUSY stuff and softkeys\n"},
	{"cfwdnoanswer", 		G_OBJ_REF(cfwdnoanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"yes",				"activate the callforward NOANSWER stuff and softkeys\n"},
	{"cfwdnoanswer_timeout",	G_OBJ_REF(cfwdnoanswer_timeout),	TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"30",				"timeout after which callforward noanswer (when active) will be triggered. default is 30 seconds\n"},
	{"nat", 			G_OBJ_REF(nat), 			TYPE_ENUM(sccp,nat),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"auto",				"Global NAT support.\n"},
	{"directrtp", 			G_OBJ_REF(directrtp), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"This option allow devices to do direct RTP sessions.\n"},
	{"allowoverlap", 		G_OBJ_REF(useoverlap), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Enable overlap dialing support. If enabled, starts dialing immediately and sends remaing digits as DTMF/inband.\n"
																																					"Use with extreme caution as it is very dialplan and provider dependent.\n"},
	{"callgroup", 			G_OBJ_REF(callgroup), 			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"We are in caller groups 1,3,4. Valid for all lines\n"},
#ifdef CS_SCCP_PICKUP	
	{"pickupgroup", 		G_OBJ_REF(pickupgroup), 		TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"We can do call pick-p for call group 1,3,4,5. Valid for all lines\n"},
	{"directed_pickup", 		G_OBJ_REF(directed_pickup), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable Pickup button to do directed pickup from a specific extension.\n"},
	{"directed_pickup_context",	G_OBJ_REF(directed_pickup_context),	TYPE_PARSER(sccp_config_parse_context),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"context where direct pickup search for extensions. If not set, the current context of the picking line will be used.\n"},
	{"directed_pickup_modeanswer", 	G_OBJ_REF(pickup_modeanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_DEPRECATED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"(DEPRECATED) Automatically Answer when using Directed Pickup. (default=on) (Replaced by in favor of pickup_modeanswer).\n"},
	{"pickup_modeanswer", 		G_OBJ_REF(pickup_modeanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Automatically Answer when using Directed Pickup. (default=on)"},
#ifdef CS_AST_HAS_NAMEDGROUP
	{"namedcallgroup",		G_OBJ_REF(namedcallgroup),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the named caller groups\n"},
	{"namedpickupgroup",		G_OBJ_REF(namedpickupgroup),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the named pickup groups (can pickup calls from remote phones which are in this callgroup\n"},
#endif
#endif
	{"callhistory_answered_elsewhere", G_OBJ_REF(callhistory_answered_elsewhere),TYPE_ENUM(skinny,callHistoryDisposition),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Ignore",			"Where to store callinfo for calls answered on a remote device. Options: Ignore, Missed Calls (or Placed Calls, Received Calls which are less usefull)"},
	{"amaflags", 			G_OBJ_REF(amaflags), 			TYPE_PARSER(sccp_config_parse_amaflags),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"default",			"Sets the default AMA flag code stored in the CDR record\n"},
	{"protocolversion", 		0,				0,	TYPE_STRING,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		"20",				"(OBSOLETE) skinny version protocol.\n"},
	{"callanswerorder", 		G_OBJ_REF(callanswerorder), 		TYPE_ENUM(sccp,call_answer_order),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"oldestfirst",			"oldestfirst or lastestfirst\n"},
	{"regcontext", 			G_OBJ_REF(regcontext), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"",				"SCCP Lines will we added to this context in asterisk for Dundi lookup purposes.\n"
																																					"Do not set to an already created/used context. The context will be autocreated. You can share the sip/iax regcontext if you like.\n"},
#ifdef CS_SCCP_REALTIME
	{"devicetable", 		G_OBJ_REF(realtimedevicetable), 	TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"sccpdevice",			"datebasetable for devices\n"},
	{"linetable", 			G_OBJ_REF(realtimelinetable), 		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"sccpline",			"datebasetable for lines\n"},
#endif
	{"meetme", 			G_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"enable/disable conferencing via meetme (on/off), make sure you have one of the meetme apps mentioned below activated in module.conf\n"
																																	"when switching meetme=on it will search for the first of these three possible meetme applications and set these defaults\n"
																																					" - {'MeetMe', 'qd'},\n"
																																					" - {'ConfBridge', 'Mac'},\n"
																																					" - {'Konference', 'MTV'}\n"},
	{"meetmeopts", 			G_OBJ_REF(meetmeopts), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"qxd",				"options to send the meetme application, defaults are dependent on meetme app see the list above\n"
																																					"Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see meetme specific documentation\n"},
																																					
	{"jbenable", 			G_OBJ_REF(global_jbconf),	 	TYPE_PARSER(sccp_config_parse_jbflags_enable),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Enables the use of a jitterbuffer on the receiving side of a sccp channel.\n"
																																					"An enabled jitterbuffer will be used only if the sending side can create and the receiving side can not accept jitter.\n"
																																					"The sccp channel can accept jitter, thus a jitterbuffer on the receive sccp side will beused only if it is forced and enabled.\n"},
#ifdef HAVE_ICONV
	{"phonecodepage", 		G_OBJ_REF(iconvcodepage), 		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"ISO8859-1",			"Translation codepage for old phones, used to convert labels/buttonnames/descriptions from UTF-8 to this codepage, on pre-java phones. DEFAULT:ISO8859-1\n"},
#endif
	{"jbforce", 			G_OBJ_REF(global_jbconf),		TYPE_PARSER(sccp_config_parse_jbflags_force),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Forces the use of a jitterbuffer on the receive side of a sccp channel. Setting this to yes, will disable direcrtp\n"},
	{"jblog", 			G_OBJ_REF(global_jbconf),		TYPE_PARSER(sccp_config_parse_jbflags_log),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Enables jitterbuffer frame logging.\n"},
	{"jbmaxsize", 		offsize(struct ast_jb_conf,max_size),offsetof(struct sccp_global_vars,global_jbconf),		TYPE_PARSER(sccp_config_parse_jbflags_maxsize),	SCCP_CONFIG_FLAG_NONE,				SCCP_CONFIG_NOUPDATENEEDED,		"200",				"Max length of the jitterbuffer in milliseconds.\n"},
	{"jbresyncthreshold", 	offsize(struct ast_jb_conf,resync_threshold),offsetof(struct sccp_global_vars,global_jbconf), 	TYPE_PARSER(sccp_config_parse_jbflags_jbresyncthreshold), SCCP_CONFIG_FLAG_NONE,		SCCP_CONFIG_NOUPDATENEEDED,		"1000",				"Jump in the frame timestamps over which the jitterbuffer is\n"
																																					"resynchronized. Useful to improve the quality of the voice, with\n"
																																					"big jumps in/broken timestamps, usually sent from exotic devices\n"
																																					"and programs.\n"},
	{"jbimpl", 		offsize(struct ast_jb_conf,impl),offsetof(struct sccp_global_vars,global_jbconf), 		TYPE_PARSER(sccp_config_parse_jbflags_impl), SCCP_CONFIG_FLAG_NONE,				SCCP_CONFIG_NOUPDATENEEDED,		"fixed",			"Jitterbuffer implementation, used on the receiving side of a\n"
																																					"sccp channel. Two implementations are currently available\n"
																																					"- 'fixed' (with size always equals to jbmaxsize)\n"
																																					"- 'adaptive' (with variable size, actually the new jb of IAX2).\n"},
	{"hotline_enabled", 		G_OBJ_REF(allowAnonymous), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Setting the hotline Feature on a device, will make it connect to a predefined extension as soon as the Receiver\n"
																																					"is picked up or the 'New Call' Button is pressed. No number has to be given. This works even on devices which\n"
																																					"have no entry in the config file or realtime database.\n"
																																					"The hotline function can be used in different circumstances, for example at a door, where you want people to be\n"
																																					"able to only call one number, or for unprovisioned phones to only be able to call the helpdesk to get their phone\n"
																																					"set up. If hotline_enabled = yes, any device which is not included in the configuration explicitly will be allowed\n"
																																					"to registered as a guest device. All such devices will register on a single shared line called 'hotline'.\n"},
	{"hotline_extension", 		offsize(sccp_hotline_t,exten),offsetof(struct sccp_global_vars,hotline),		TYPE_PARSER(sccp_config_parse_hotline_exten),	SCCP_CONFIG_FLAG_NONE,				SCCP_CONFIG_NEEDDEVICERESET,		"111",				"Extension automatically dialed when going offhook with this device (adhoc/plar)"},
	{"hotline_context",		offsize(sccp_line_t,context),offsetof(struct sccp_global_vars,hotline),			TYPE_PARSER(sccp_config_parse_hotline_context),	SCCP_CONFIG_FLAG_NONE,				SCCP_CONFIG_NEEDDEVICERESET,		"default",			"Context use whe dialing the hotline_extension"},
	{"hotline_label", 		offsize(sccp_line_t,label),offsetof(struct sccp_global_vars,hotline),			TYPE_PARSER(sccp_config_parse_hotline_label),	SCCP_CONFIG_FLAG_NONE,				SCCP_CONFIG_NEEDDEVICERESET,		"hotline",			"Label on the display for this hotline"},
	{"fallback",			G_OBJ_REF(token_fallback),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Immediately fallback to primary/master server when it becomes available (master/slave asterisk cluster) (TokenRequest)\n"
																																					"Possible values are: true/false/odd/even/script.\n" 
																																					"active/passive cluster: true on active/false on passive\n" 
																																					"active/active cluster: even on active1/off on active2\n"
																																					"more complex cluster: use script. It will be called with three arguments, namely mac-address, ip-address, devicetype.\n"
																																					"                      and it should return 'ACK' (without the quotes) to acknowledge the token, or a value for the number of seconds to backoff and try again.\n" 
																																					"Value can be changed online via CLI/AMI command 'sccp set fallback true/false/odd/even/script'\n"},
	{"backoff_time", 		G_OBJ_REF(token_backoff_time),		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"60",				"Time to wait before re-asking to fallback to primary server (Token Reject Backoff Time)\n"},
	{"server_priority", 		G_OBJ_REF(server_priority),		TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"1",				"Server Priority for fallback: 1=Primary, 2=Secondary, 3=Tertiary etc\n"
																																					"For active-active (fallback=odd/even) use 1 for both\n"},
//#if defined(CS_EXPERIMENTAL_XML)
//	{"webdir",			G_OBJ_REF(webdir),			TYPE_PARSER(sccp_config_parse_webdir),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"Directory where xslt stylesheets can be found.\n"},
//#endif
};

/*!
 * \brief List of SCCP Config Options for SCCP Devices
 */
static const SCCPConfigOption sccpDeviceConfigOptions[] = {
	{"name", 			0, 				0,	TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device name\n"},
	{"device", 			D_OBJ_REF(config_type),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device type\n"},
	{"devicetype", 			D_OBJ_REF(config_type),			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device type\n"},
	{"type", 			0,				0,	TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		"device",			"used for device templates, value will be inherited.\n"},
	{"description", 		D_OBJ_REF(description),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"device description\n"},
	{"keepalive", 			D_OBJ_REF(keepalive), 			TYPE_UINT,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"set keepalive to 60\n"},
	{"tzoffset", 			D_OBJ_REF(tz_offset), 			TYPE_INT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"0",				"time zone offset\n"},
	{"disallow|allow", 		D_OBJ_REF(preferences), 		TYPE_PARSER(sccp_config_parse_codec_preferences),				SCCP_CONFIG_FLAG_DEPRECATED | SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,	"(DEPRECATED) Same as entry in [general] section. Replace by setting codec preferences per line instead.\n"},
	{"transfer", 			D_OBJ_REF(transfer),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable or disable the transfer capability. It does remove the transfer softkey\n"},
	{"park", 			D_OBJ_REF(park),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"take a look to the compile how-to. Park stuff is not compiled by default.\n"},
	{"cfwdall", 			D_OBJ_REF(cfwdall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"activate the call forward stuff and soft keys\n"},
	{"cfwdbusy", 			D_OBJ_REF(cfwdbusy), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"allow call forward when line is busy\n"},
	{"cfwdnoanswer", 		D_OBJ_REF(cfwdnoanswer),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"no",				"allow call forward when line if not being answered\n"},
#if 0
	{"dnd",	 			0,				0,	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"(OBSOLETE) This setting has moved to the line definition. To allow/disallow dnd you should use dndFeature in device and setup dnd per line.\n"},
#endif
	{"dndFeature",	 		D_OBJ_REF(dndFeature.enabled),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"allow usage do not disturb button\n"},
	{"dnd",				D_OBJ_REF(dndmode),			TYPE_ENUM(sccp,dndmode),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"allow setting dnd action for this device. Valid values are 'off', 'reject' (busy signal), 'silent' (ringer = silent) or 'user' (not used at the moment). . The value 'on' has been made obsolete in favor of 'reject'\n"},
	{"dtmfmode", 			0,				0,	TYPE_STRING,									SCCP_CONFIG_FLAG_OBSOLETE,					SCCP_CONFIG_NOUPDATENEEDED,		"",				"(OBSOLETE) (don't use).\n"},
	{"force_dtmfmode", 		D_OBJ_REF(dtmfmode), 			TYPE_ENUM(sccp,dtmfmode),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"auto",				"auto, skinny or rfc2833. Some phone models with bad firmware do send dtmf in a messed up order and need to be forced to skinny mode.\n"},
	{"deny|permit", 		D_OBJ_REF(ha),	 			TYPE_PARSER(sccp_config_parse_deny_permit),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NEEDDEVICERESET,	NULL,				"Same as entry in [general] section\n"
																																					"This device can register only using this ip address\n"},
	{"audio_tos", 			D_OBJ_REF(audio_tos),			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 				SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"sets the audio/rtp packets Type of Service (TOS) (defaults to 0xb8 = 10111000 = 184 = DSCP:101110 = EF).\n"
																																					"Others possible values : 0x??, lowdelay, throughput, reliability, mincost(solaris), none.\n"},
	{"audio_cos", 			D_OBJ_REF(audio_cos),			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 				SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"sets the audio/rtp packets Class of Service (COS)\n"},
	{"video_tos", 			D_OBJ_REF(video_tos),			TYPE_PARSER(sccp_config_parse_tos),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the video/rtp packets Type of Service (TOS) (defaults to 0x88 = 10001000 = 136 = DSCP:100010 = AF41).\n"},
	{"video_cos", 			D_OBJ_REF(video_cos),			TYPE_PARSER(sccp_config_parse_cos),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT, 				SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"sets the video/rtp packets Class of Service (COS).\n"},
	{"trustphoneip", 		D_OBJ_REF(trustphoneip), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_OBSOLETE,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,				"(OBSOLETE) The phone has a ip address. It could be private, so if the phone is behind NAT\n"
																																					"we don't have to trust the phone ip address, but the ip address of the connection.\n"},
	{"nat", 			D_OBJ_REF(nat), 			TYPE_ENUM(sccp,nat),								SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Device NAT support. Currently nat is automatically detected in most cases.\n"},
	{"directrtp", 			D_OBJ_REF(directrtp), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"This option allow devices to do direct RTP sessions.\n"},
	{"earlyrtp", 			D_OBJ_REF(earlyrtp), 			TYPE_ENUM(sccp,earlyrtp),							SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"valid options: none, offhook, immediate, dial, ringout and progress.\n"
																																					"The audio stream will be open in the 'progress' state by default. Immediate forces overlap dialing.\n"},
	{"private", 			D_OBJ_REF(privacyFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"permit the private function softkey for this device\n"},
	{"privacy", 			D_OBJ_REF(privacyFeature),	 	TYPE_PARSER(sccp_config_parse_privacyFeature),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"full = disable hints notification on devices, on = hints showed depending on privacy key, off = hints always showed\n"},
	{"mwilamp",			D_OBJ_REF(mwilamp), 			TYPE_ENUM(skinny,lampmode),							SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the MWI lamp style when MWI active to on, off, wink, flash or blink\n"},
	{"mwioncall", 			D_OBJ_REF(mwioncall), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the MWI on call.\n"},
	{"meetme", 			D_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable conferencing via app_meetme (on/off)\n"},
	{"meetmeopts", 			D_OBJ_REF(meetmeopts), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"options to send the app_meetme application (default 'qd' = quiet,dynamic pin)\n"																																					"Other options (A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see app_meetme documentation\n"},
	{"softkeyset", 			D_OBJ_REF(softkeyDefinition),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NEEDDEVICERESET,		"default",			"use specified softkeyset with name 'default'\n"},
	{"useRedialMenu", 		D_OBJ_REF(useRedialMenu), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"show redial phone book list instead of dialing the last number (adv_feature). Requires a Phone Service block in SEP....cnf.xml to work correct on Java phones (See conf/tftp/SEP example files)\n"},
#ifdef CS_SCCP_PICKUP
	{"directed_pickup", 		D_OBJ_REF(directed_pickup), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_DEPRECATED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"(DEPRECATED) enable/disable Pickup button to do directed pickup from a specific extension. (Deprecated: use line->directed_pickup instead).\n"},
	{"directed_pickup_context", 	D_OBJ_REF(directed_pickup_context),	TYPE_PARSER(sccp_config_parse_context),						SCCP_CONFIG_FLAG_DEPRECATED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"(DEPRECATED) context where direct pickup search for extensions. If not set, current context will be use. (Deprecated: use line->directed_pickup_context instead).\n"},
	{"directed_pickup_modeanswer", 	D_OBJ_REF(pickup_modeanswer),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_DEPRECATED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"(DEPRECATED) Automatically Answer when using Directed Pickup. (default=on) (Deprecated: use line->pickup_modeanswer instead).\n"},
#endif
	{"callhistory_answered_elsewhere", D_OBJ_REF(callhistory_answered_elsewhere),TYPE_ENUM(skinny,callHistoryDisposition),					SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Where to store callinfo for calls answered on a remote device. Options: Options: Ignore, Missed Calls (or Placed Calls, Received Calls which are less usefull)"},
	{"monitor", 			D_OBJ_REF(monitorFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				""},
	{"allowoverlap", 		D_OBJ_REF(overlapFeature.enabled), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Allow for Overlap dialing (Continue dialing after the first part of the number has already been send to the pstn)"},
	{"setvar", 			D_OBJ_REF(variables),			TYPE_PARSER(sccp_config_parse_variables),					SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
																																					"format setvar=param=value, for example setvar=sipno=12345678\n"},
	{"permithost", 			D_OBJ_REF(permithosts), 		TYPE_PARSER(sccp_config_parse_permithosts),					SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"permit/deny but by resolved hostname"},
	{"addon", 			D_OBJ_REF(addons),	 		TYPE_PARSER(sccp_config_parse_addons),						SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"One or Two of 7914, 7915, 7916"},
	{"button", 			D_OBJ_REF(buttonconfig), 		TYPE_PARSER(sccp_config_parse_button),						SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"Buttons come in the following flavours (empty, line, speeddial, service, feature).\n"
																																					"Examples (read the documentation for more examples/combinations):\n"
																																					" - button = line,1234\n"
																																					" - button = line,1234,default\n"
																																					" - button = empty\n"
																																					" - button = line,98099@11:Phone1\n"
																																					" - button = line,98099@12:Phone2@ButtonLabel!silent            ; append cidnum:'12' and cidname:'Phone2' to line-ci with label 'ButtonLabel', don't ring when dialed directly\n"
																																					" - button = line,98099@+12:Phone2@ButtonLabel!silent           ; same as the previous line\n"
																																					" - button = line,98099@=12:Phone2!silent                       ; overwrite line-cid instead of appending\n"
																																					" - button = speeddial,Phone 2 Line 1, 98021, 98021@hints\n"
																																					" - button = feature,cfwdall,1234\n                             ; feature, label, feature_type=cfwdall, exten\n"
																																					" - button = feature,PDefault,ParkingLot,default                ; feature, label, feature_type=ParkingLot, parkinglotContext[,RetrieveSingle]\n"
																																					" - button = feature,PDefault,ParkingLot,default,RetrieveSingle ; feature, label, feature_type=ParkingLot, parkinglotContext[,RetrieveSinglen]\n"
																																					" - button = feature,TriStateDND,DND                            ; feature, label, feature_type=DND\n"
																																					" - button = feature,BusyDND,DND,busy                           ; feature, label, feature_type=DND, busy\n"
																																					" - button = feature,SilentDND,DND,silent                       ; feature, label, feature_type=DND, silent\n"},
/*	{"digittimeout", 		D_OBJ_REF(digittimeout), 		TYPE_INT,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"8",				"More digits\n"},*/
	{"allowRinginNotification", 	D_OBJ_REF(allowRinginNotification), 	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"allow ringin notification for hinted extensions. experimental configuration param that may be removed in further version\n"},
#ifdef CS_SCCP_CONFERENCE
	{"conf_allow",			D_OBJ_REF(allow_conference),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Allow the use of conference"},
	{"conf_music_on_hold_class",	D_OBJ_REF(conf_music_on_hold_class),	TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"default",			"Play music on hold of this class when no moderator is listening on the conference. If set to an empty string, no music on hold will be played."},
	{"conf_play_general_announce",	D_OBJ_REF(conf_play_general_announce),	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Playback General Announcements (like: 'You are Entering/Leaving the conference. Not supported on 7936/7937 conference phones.')"},
	{"conf_play_part_announce",	D_OBJ_REF(conf_play_part_announce),	TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Playback Personal/Participant Announcements, (like: 'You have been muted / You have been kicked. '). Not supported on 7936/7937 conference phones."},
	{"conf_mute_on_entry",		D_OBJ_REF(conf_mute_on_entry),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"no",				"Mute new participants from the start. Not supported on 7936/7937 conference phones."},
	{"conf_show_conflist",		D_OBJ_REF(conf_show_conflist),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"yes",				"Automatically show conference list to the moderator. Not supported on 7936/7937 conference phones."},
#endif
#ifdef HAVE_ICONV
	{"phonecodepage", 		D_OBJ_REF(iconvcodepage), 		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Translation codepage for old phones, used to convert labels/buttonnames/descriptions from UTF-8 to this codepage, on pre-java phones. DEFAULT:ISO8859-1\n"},
#endif
	{"backgroundImage",		D_OBJ_REF(backgroundImage),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the Background Image after device registered. Image must be set as URI to a http served file."},
	{"backgroundThumbnail",		D_OBJ_REF(backgroundTN),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the Background Thumbnail after device registered. Thumbnail Image must be set as URI to a http served file. This is required on some of the 89xx series phones."},
	{"ringtone",			D_OBJ_REF(ringtone),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Set the Ring Tone after device registered. Ring Tone must be set as URI to a http served file."},
	{"imageversion",		D_OBJ_REF(imageversion),		TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"ImageVersion to be loaded on the device."},
};

/*!
 * \brief List of SCCP Config Options for SCCP Lines
 */
static const SCCPConfigOption sccpLineConfigOptions[] = {
	{"name", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"line name\n"},
	{"line", 			0,	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"line name\n"},
	{"type", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		"line",				"used for line templates, value will be inherited.\n"},
	{"id", 				L_OBJ_REF(id),				TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"id\n"},
	{"pin", 			L_OBJ_REF(pin), 			TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"pin\n"},
	{"label", 			L_OBJ_REF(label), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_REQUIRED, 					SCCP_CONFIG_NEEDDEVICERESET,		NULL,				"label\n"},
	{"description", 		L_OBJ_REF(description),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"description\n"},
	{"context", 			L_OBJ_REF(context), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_REQUIRED,SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"pbx dialing context\n"},
	{"cid_name", 			L_OBJ_REF(cid_name), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"callerid name\n"},
	{"cid_num", 			L_OBJ_REF(cid_num), 			TYPE_STRING,									SCCP_CONFIG_FLAG_REQUIRED,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"callerid number\n"},
	{"defaultSubscriptionId_name",	L_OBJ_REF(defaultSubscriptionId.name), 	TYPE_STRING,						 			SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Name used on a shared line when no name is specified on the line button for the device\n"},
	{"defaultSubscriptionId_number",L_OBJ_REF(defaultSubscriptionId.number),TYPE_STRING,						 			SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Number used on a shared line when no name is specified on the line button for the device\n"},
	{"callerid", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_OBSOLETE, 					SCCP_CONFIG_NOUPDATENEEDED, 		NULL,				"(OBSOLETE) callerid param. Use cid_num and cid_name.\n"},
	{"mailbox", 			L_OBJ_REF(mailboxes), 			TYPE_PARSER(sccp_config_parse_mailbox),						SCCP_CONFIG_FLAG_NONE,	 					SCCP_CONFIG_NEEDDEVICERESET, 		"",				"Mailbox to store messages in. Format 'mailbox@context' or 'mailbox' when you use 'default' context\n"},
	{"vmnum", 			L_OBJ_REF(vmnum), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Number to dial to get to the users Mailbox\n"},
	{"adhocNumber", 		L_OBJ_REF(adhocNumber), 		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"Adhoc Number or Private-line automatic ring down (PLAR):\n"
																																					"Adhoc/PLAR circuits have statically configured endpoints and do not require the user dialing to connect calls.\n"
																																					" - The adhocNumber is dialed as soon as the Phone is taken off-hook or when the new-call button is pressed.\n"
																																					" - The number will not be dialed when choosing a line; so when you choose a line you can enter a number manually.\n"},
	{"meetme", 			L_OBJ_REF(meetme), 			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable conferencing via meetme, make sure you have one of the meetme apps mentioned below activated in module.conf.\n"
																																					"When switching meetme=on it will search for the first of these three possible meetme applications and set these defaults.\n"
																																					"Meetme=>'qd', ConfBridge=>'Mac', Konference=>'MTV'\n"},
	{"meetmenum",			L_OBJ_REF(meetmenum), 			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"This extension will receive meetme requests, SCCP_MEETME_ROOM channel variable will\n"
																																					"contain the room number dialed into simpleswitch (this parameter is going to be removed).\n"},
	{"meetmeopts", 			L_OBJ_REF(meetmeopts),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_DEVICE_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"options to send the meetme application, defaults are dependent on meetme app see the list above.\n"
																																					"Other options (app_meetme: A,a,b,c,C,d,D,E,e,F,i,I,l,L,m,M,o,p,P,q,r,s,S,t,T,w,x,X,1) see conferencing app for specific documentation\n"},
	{"transfer", 			L_OBJ_REF(transfer),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"per line transfer capability\n"},
	{"incominglimit", 		L_OBJ_REF(incominglimit),		TYPE_UINT,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"6",				"allow x number of incoming calls (call waiting)\n"},
	{"echocancel",			L_OBJ_REF(echocancel),			TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the phone echocancel for this line\n"},
	{"silencesuppression",		L_OBJ_REF(silencesuppression),		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the silence suppression for this line\n"},
	{"language",			L_OBJ_REF(language),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the language setting per line\n"},
	{"musicclass",			L_OBJ_REF(musicclass),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the music on hold class per line\n"},
	{"accountcode",			L_OBJ_REF(accountcode),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"accountcode for this line to make billing per call possible\n"},
	{"amaflags",			L_OBJ_REF(amaflags),			TYPE_PARSER(sccp_config_parse_amaflags),					SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"sets the AMA flags stored in the CDR record for this line\n"},
	{"callgroup",			L_OBJ_REF(callgroup),			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the caller groups this line is a member of\n"},
#ifdef CS_SCCP_PICKUP
	{"pickupgroup",			L_OBJ_REF(pickupgroup),			TYPE_PARSER(sccp_config_parse_group),						SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the pickup groups this line is a member of (this phone can pickup calls from remote phones which are in this caller group\n"},
	{"directed_pickup", 		L_OBJ_REF(directed_pickup), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"enable/disable Pickup button to do directed pickup from a specific extension.\n"},
	{"directed_pickup_context",	L_OBJ_REF(directed_pickup_context),	TYPE_PARSER(sccp_config_parse_context),						SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"context where direct pickup search for extensions. If not set, current context or this line will be used.\n"},
	{"pickup_modeanswer", 		L_OBJ_REF(pickup_modeanswer), 		TYPE_BOOLEAN,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"Automatically Answer when using Directed Pickup. (default=on)"},
#ifdef CS_AST_HAS_NAMEDGROUP
	{"namedcallgroup",		L_OBJ_REF(namedcallgroup),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the named caller groups this line is a member of (ast111)\n"},
	{"namedpickupgroup",		L_OBJ_REF(namedpickupgroup),		TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT,				SCCP_CONFIG_NOUPDATENEEDED,		"",				"sets the named pickup groups this line is a member of (this phone can pickup calls from remote phones which are in this caller group (ast111)\n"},
#endif
#endif
	{"disallow|allow", 		L_OBJ_REF(preferences), 		TYPE_PARSER(sccp_config_parse_codec_preferences),				SCCP_CONFIG_FLAG_GET_GLOBAL_DEFAULT | SCCP_CONFIG_FLAG_MULTI_ENTRY,	SCCP_CONFIG_NOUPDATENEEDED,	NULL,				"Same as entry in [general] section"},
	{"parkinglot",			L_OBJ_REF(parkinglot),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"parkinglot assigned to this line\n"},
	{"trnsfvm",			L_OBJ_REF(trnsfvm),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"extension to redirect the caller to for voice mail\n"},
	{"secondary_dialtone_digits",	L_OBJ_REF(secondary_dialtone_digits),	TYPE_PARSER(sccp_config_parse_secondaryDialtoneDigits),				SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"digits to indicate an external line to user (secondary dialtone) (max 9 digits)\n"},
	{"secondary_dialtone_tone",	L_OBJ_REF(secondary_dialtone_tone),	TYPE_ENUM(skinny,tone),								SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"Outside Dial Tone",		"outside dialtone frequency\n"},
	{"setvar",			L_OBJ_REF(variables),			TYPE_PARSER(sccp_config_parse_variables),					SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"extra variables to be set on line initialization multiple entries possible (for example the sip number to use when dialing outside)\n"
																																					"format setvar=param=value, for example setvar=sipno=12345678\n"},
	{"dnd",				L_OBJ_REF(dndmode),			TYPE_ENUM(sccp,dndmode),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"",				"allow setting dnd action for this line. Valid values are 'off', 'reject' (busy signal), 'silent' (ringer = silent) or 'user' (not used at the moment). . The value 'on' has been made obsolete in favor of 'reject'\n"},
	{"regexten",			L_OBJ_REF(regexten),			TYPE_STRINGPTR,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		NULL,				"SCCP Lines will we added to the regcontext with this number for Dundi look up purpose\n"},
#ifdef CS_SCCP_VIDEO
	{"videomode",			L_OBJ_REF(videomode),			TYPE_ENUM(sccp,video_mode),							SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"auto",				"Automatic or Manual video mode. Valid values are 'auto', 'user' or 'off'. When set to 'auto', video will automatically start if both parties have a compatible code enabled. In 'user' mode the user needs to press the vidmode softkey before video will be tried. Default:'auto'.\n"
																																					"Note: On the 8945 phones you need to go to Settings->Preferences->CameraSettings and set 'Auto Transmit Video' to 'Off'\n"},
#endif
};

/*!
 * \brief List of SCCP Config Options for SCCP SoftKey
 */
static const SCCPConfigOption sccpSoftKeyConfigOptions[] = {
	{"type",			0,	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"softkeyset",										"This should be set to softkeyset"},
	{"name", 			0, 	0, 				TYPE_STRING,									SCCP_CONFIG_FLAG_IGNORE,					SCCP_CONFIG_NOUPDATENEEDED,		"default",										"softkeyset name\n"},
	{"onhook",			S_OBJ_REF(modes[KEYMODE_ONHOOK]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial, newcall, cfwdall, cfwdbusy, cfwdnoanswer, pickup, gpickup, dnd, private",	"displayed when we are on hook"},
	{"connected",			S_OBJ_REF(modes[KEYMODE_CONNECTED]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"hold, endcall, park, vidmode, select, cfwdall, cfwdbusy, idivert, monitor",		"displayed when we have a connected call"},
	{"onhold",			S_OBJ_REF(modes[KEYMODE_ONHOLD]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"resume, newcall, endcall, transfer, conflist, select, dirtrfr, idivert, meetme",	"displayed when we have a call on hold"},
	{"ringin",			S_OBJ_REF(modes[KEYMODE_RINGIN]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"answer, endcall, transvm, idivert",							"displayed when we have an incoming call"},
	{"offhook",			S_OBJ_REF(modes[KEYMODE_OFFHOOK]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial, endcall, private, cfwdall, cfwdbusy, cfwdnoanswer, pickup, gpickup, meetme, barge",	"displayed when the phone is taken off hook"},
	{"conntrans",			S_OBJ_REF(modes[KEYMODE_CONNTRANS]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"hold, endcall, transfer, conf, park, select, dirtrfr, monitor, vidmode, meetme, cfwdall, cfwdbusy","displayed when we are connected and could transfer a call"},
	{"digitsfoll",			S_OBJ_REF(modes[KEYMODE_DIGITSFOLL]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"back, endcall, dial",									"displayed when one or more digits have been entered, more are expected"},
	{"connconf",			S_OBJ_REF(modes[KEYMODE_CONNCONF]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"conflist, newcall, endcall, hold, vidmode, monitor",					"displayed when we are in a conference"},
	{"ringout",			S_OBJ_REF(modes[KEYMODE_RINGOUT]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"empty, endcall, transfer",								"displayed when We are calling someone"},
	{"offhookfeat",			S_OBJ_REF(modes[KEYMODE_OFFHOOKFEAT]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"resume, newcall, endcall",								"displayed wenn we went offhook using a feature"},
	{"onhint",			S_OBJ_REF(modes[KEYMODE_INUSEHINT]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial, newcall, pickup, gpickup",							"displayed when a hint is activated"},
	{"onstealable",			S_OBJ_REF(modes[KEYMODE_ONHOOKSTEALABLE]),TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"redial, newcall, barge, intrcpt",							"displayed when there is a call we could steal on one of the neighboring phones"},
	{"holdconf",			S_OBJ_REF(modes[KEYMODE_HOLDCONF]), 	TYPE_STRING,									SCCP_CONFIG_FLAG_NONE,						SCCP_CONFIG_NOUPDATENEEDED,		"resume, newcall, endcall, join",							"displayed when we are a conference moderator, have the conference on hold and have another active call"},
	{"uriaction", 			S_OBJ_REF(softkeyCbMap),		TYPE_STRING,									SCCP_CONFIG_FLAG_MULTI_ENTRY,					SCCP_CONFIG_NOUPDATENEEDED, 		NULL,											"softkey uri action to replace default handling. Format: uriaction = softkeyname, uri[,uri...]\n. URI can be an embedded cisco action (like Key:Service, Play:1041.raw) or a URL"
																																												"If uri is a url the following parameters will be added to it: devicename, linename, channelname, callid, linkedid, uniqueid, appid, transactionid"},
};
/* *INDENT-ON* */

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
