#ifndef __SCCP_PROTOCOL_H
#define __SCCP_PROTOCOL_H

#define SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW		3		/* At least we require protocol V.3 */
#define SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH		17		/* We support up to protocol V.11 */

#define DEFAULT_SCCP_PORT					2000 	/*< SCCP uses port 2000. */
#define DEFAULT_SCCP_BACKLOG				2 		/*< the listen baklog. */
#define SCCP_MAX_AUTOLOGIN					100 	/*< Maximum allowed of autologins per device */
#define SCCP_KEEPALIVE						5 		/*< Default keepalive time if not specified in sccp.conf. */

/* internal chan_sccp call state c->callstate */
typedef enum{
	SCCP_CHANNELSTATE_DOWN=0,
	SCCP_CHANNELSTATE_OFFHOOK=1,
	SCCP_CHANNELSTATE_ONHOOK=2,
	SCCP_CHANNELSTATE_RINGOUT=3,
	SCCP_CHANNELSTATE_RINGING=4,
	SCCP_CHANNELSTATE_CONNECTED=5,
	SCCP_CHANNELSTATE_BUSY=6,
	SCCP_CHANNELSTATE_CONGESTION=7,
	SCCP_CHANNELSTATE_HOLD=8,
	SCCP_CHANNELSTATE_CALLWAITING=9,
	SCCP_CHANNELSTATE_CALLTRANSFER=10,
	SCCP_CHANNELSTATE_CALLPARK=11,
	SCCP_CHANNELSTATE_PROCEED=12,
	SCCP_CHANNELSTATE_CALLREMOTEMULTILINE=13,
	SCCP_CHANNELSTATE_INVALIDNUMBER=14,
	SCCP_CHANNELSTATE_DIALING=20,
	SCCP_CHANNELSTATE_PROGRESS=21,
	SCCP_CHANNELSTATE_GETDIGITS=0xA0,
	SCCP_CHANNELSTATE_CALLCONFERENCE=0xA1,
	SCCP_CHANNELSTATE_SPEEDDIAL=0xA2,
	SCCP_CHANNELSTATE_DIGITSFOLL=0xA3,
	SCCP_CHANNELSTATE_INVALIDCONFERENCE=0xA4,
	SCCP_CHANNELSTATE_CONNECTEDCONFERENCE=0xA5,
	
	SCCP_CHANNELSTATE_ZOMBIE=0xFE,
	SCCP_CHANNELSTATE_DND=0xFF
} sccp_channelState_t;



typedef enum{
	SCCP_DEVICESTATE_ONHOOK=0,
	SCCP_DEVICESTATE_OFFHOOK=1,
	SCCP_DEVICESTATE_UNAVAILABLE=2,
	SCCP_DEVICESTATE_DND=3,
	SCCP_DEVICESTATE_FWDALL=4
} sccp_devicestate_t;


#define SCCP_CFWD_NONE						0
#define SCCP_CFWD_ALL						1
#define SCCP_CFWD_BUSY						2
#define SCCP_CFWD_NOANSWER					3

/* skinny protocol call states */
#define SKINNY_CALLSTATE_OFFHOOK			 	1
#define SKINNY_CALLSTATE_ONHOOK 			 	2
#define SKINNY_CALLSTATE_RINGOUT			 	3
#define SKINNY_CALLSTATE_RINGIN 			 	4
#define SKINNY_CALLSTATE_CONNECTED			 	5
#define SKINNY_CALLSTATE_BUSY				 	6
#define SKINNY_CALLSTATE_CONGESTION 		 	7
#define SKINNY_CALLSTATE_HOLD				 	8
#define SKINNY_CALLSTATE_CALLWAITING		 	9
#define SKINNY_CALLSTATE_CALLTRANSFER		 	10
#define SKINNY_CALLSTATE_CALLPARK			 	11
#define SKINNY_CALLSTATE_PROCEED			 	12
#define SKINNY_CALLSTATE_CALLREMOTEMULTILINE 	13
#define SKINNY_CALLSTATE_INVALIDNUMBER		 	14
#define SKINNY_CALLSTATE_HOLDYELLOW				15
#define SKINNY_CALLSTATE_INTERCOMONEWAY			16
#define SKINNY_CALLSTATE_HOLDRED				17


typedef enum{
	SKINNY_CALLTYPE_INBOUND=1,
	SKINNY_CALLTYPE_OUTBOUND=2,
	SKINNY_CALLTYPE_FORWARD=3
} skinny_calltype_t;

typedef enum{
       SKINNY_CALLPRIORITY_HIGHEST=0,
       SKINNY_CALLPRIORITY_HIGH=1,
       SKINNY_CALLPRIORITY_MEDIUM=2,
       SKINNY_CALLPRIORITY_LOW=3,
       SKINNY_CALLPRIORITY_NORMAL=4
} skinny_callPriority_t;

typedef enum{
       SKINNY_CALLINFO_VISIBILITY_DEFAULT=0,
       SKINNY_CALLINFO_VISIBILITY_COLLAPSED=1,
       SKINNY_CALLINFO_VISIBILITY_HIDDEN=2
} skinny_callinfo_visibility_t;

typedef enum{
	SKINNY_CALLSECURITYSTATE_UNKNOWN=0,
	SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED=1,
	SKINNY_CALLSECURITYSTATE_AUTHENTICATED=2
} skinny_callstate_security_t;

#define StationMaxDeviceNameSize					16
#define StationMaxButtonTemplateSize				42
#define StationMaxButtonTemplateNameSize			44
#define StationDateTemplateSize 					6
//#define StationMaxDisplayTextSize					33
#define StationMaxDisplayTextSize					32
#define StationMaxDisplayNotifySize 				32
#define StationMaxDirnumSize						24
#define StationMaxNameSize							40
#define StationMaxSoftKeyDefinition 				32
#define StationMaxSoftKeySetDefinition				16
#define StationMaxSoftKeyIndex						16
#define StationMaxSoftKeyLabelSize					16
#define StationMaxVersionSize						16
#define ServerMaxNameSize							48
#define StationMaxServiceURLSize					256

/* skinny tones skinny_tone2str */
#define SKINNY_TONE_SILENCE 				0
#define SKINNY_TONE_DTMF1					1
#define SKINNY_TONE_DTMF2					2
#define SKINNY_TONE_DTMF3					3
#define SKINNY_TONE_DTMF4					4
#define SKINNY_TONE_DTMF5					5
#define SKINNY_TONE_DTMF6					6
#define SKINNY_TONE_DTMF7					7
#define SKINNY_TONE_DTMF8					8
#define SKINNY_TONE_DTMF9					9
#define SKINNY_TONE_DTMF0					0xa
#define SKINNY_TONE_DTMFSTAR				0xe
#define SKINNY_TONE_DTMFPOUND				0xf
#define SKINNY_TONE_DTMFA					0x10
#define SKINNY_TONE_DTMFB					0x11
#define SKINNY_TONE_DTMFC					0x12
#define SKINNY_TONE_DTMFD					0x13
#define SKINNY_TONE_INSIDEDIALTONE			0x21
#define SKINNY_TONE_OUTSIDEDIALTONE 		0x22
#define SKINNY_TONE_LINEBUSYTONE			0x23
#define SKINNY_TONE_ALERTINGTONE			0x24
#define SKINNY_TONE_REORDERTONE 			0x25
#define SKINNY_TONE_RECORDERWARNINGTONE 	0x26
#define SKINNY_TONE_RECORDERDETECTEDTONE	0x27
#define SKINNY_TONE_REVERTINGTONE			0x28
#define SKINNY_TONE_RECEIVEROFFHOOKTONE 	0x29
#define SKINNY_TONE_PARTIALDIALTONE 		0x2a
#define SKINNY_TONE_NOSUCHNUMBERTONE		0x2b
#define SKINNY_TONE_BUSYVERIFICATIONTONE	0x2c
#define SKINNY_TONE_CALLWAITINGTONE 		0x2d
#define SKINNY_TONE_CONFIRMATIONTONE		0x2e
#define SKINNY_TONE_CAMPONINDICATIONTONE	0x2f
#define SKINNY_TONE_RECALLDIALTONE			0x30
#define SKINNY_TONE_ZIPZIP					0x31
#define SKINNY_TONE_ZIP 					0x32
#define SKINNY_TONE_BEEPBONK				0x33
#define SKINNY_TONE_MUSICTONE				0x34
#define SKINNY_TONE_HOLDTONE				0x35
#define SKINNY_TONE_TESTTONE				0x36
#define SKINNY_TONE_ADDCALLWAITING			0x40
#define SKINNY_TONE_PRIORITYCALLWAIT		0x41
#define SKINNY_TONE_RECALLDIAL				0x42
#define SKINNY_TONE_BARGIN					0x43
#define SKINNY_TONE_DISTINCTALERT			0x44
#define SKINNY_TONE_PRIORITYALERT			0x45
#define SKINNY_TONE_REMINDERRING			0x46
#define SKINNY_TONE_MF1 					0x50
#define SKINNY_TONE_MF2 					0x51
#define SKINNY_TONE_MF3 					0x52
#define SKINNY_TONE_MF4 					0x53
#define SKINNY_TONE_MF5 					0x54
#define SKINNY_TONE_MF6 					0x55
#define SKINNY_TONE_MF7 					0x56
#define SKINNY_TONE_MF8 					0x57
#define SKINNY_TONE_MF9 					0x58
#define SKINNY_TONE_MF0 					0x59
#define SKINNY_TONE_MFKP1					0x5a
#define SKINNY_TONE_MFST					0x5b
#define SKINNY_TONE_MFKP2					0x5c
#define SKINNY_TONE_MFSTP					0x5d
#define SKINNY_TONE_MFST3P					0x5e
#define SKINNY_TONE_MILLIWATT				0x5f
#define SKINNY_TONE_MILLIWATTTEST			0x60
#define SKINNY_TONE_HIGHTONE				0x61
#define SKINNY_TONE_FLASHOVERRIDE			0x62
#define SKINNY_TONE_FLASH					0x63
#define SKINNY_TONE_PRIORITY				0x64
#define SKINNY_TONE_IMMEDIATE				0x65
#define SKINNY_TONE_PREAMPWARN				0x66
#define SKINNY_TONE_2105HZ					0x67
#define SKINNY_TONE_2600HZ					0x68
#define SKINNY_TONE_440HZ					0x69
#define SKINNY_TONE_300HZ					0x6a
#define SKINNY_TONE_NOTONE					0x7f

/* alarm skinny_alarm2str*/
#define SKINNY_ALARM_CRITICAL					0
#define SKINNY_ALARM_WARNING					1
#define SKINNY_ALARM_INFORMATIONAL				2
#define SKINNY_ALARM_UNKNOWN					4
#define SKINNY_ALARM_MAJOR						7
#define SKINNY_ALARM_MINOR						8
#define SKINNY_ALARM_MARGINAL					10
#define SKINNY_ALARM_TRACEINFO					20

/* devices type */
#define SKINNY_DEVICETYPE_30SPPLUS					1
#define SKINNY_DEVICETYPE_12SPPLUS					2
#define SKINNY_DEVICETYPE_12SP						3
#define SKINNY_DEVICETYPE_12						4
#define SKINNY_DEVICETYPE_30VIP 					5
#define SKINNY_DEVICETYPE_ATA186					12
#define SKINNY_DEVICETYPE_ATA188					12 /*!< ATA188 -FS */
#define SKINNY_DEVICETYPE_VIRTUAL30SPPLUS			20
#define SKINNY_DEVICETYPE_PHONEAPPLICATION			21
#define SKINNY_DEVICETYPE_ANALOGACCESS				30
#define SKINNY_DEVICETYPE_DIGITALACCESSPRI			40
#define SKINNY_DEVICETYPE_DIGITALACCESST1			41
#define SKINNY_DEVICETYPE_DIGITALACCESSTITAN2		42
#define SKINNY_DEVICETYPE_ANALOGACCESSELVIS 		47
#define SKINNY_DEVICETYPE_DIGITALACCESSLENNON		49
#define SKINNY_DEVICETYPE_CONFERENCEBRIDGE			50
#define SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO		51
#define SKINNY_DEVICETYPE_H225						60
#define SKINNY_DEVICETYPE_H323PHONE 				61
#define SKINNY_DEVICETYPE_H323TRUNK 				62
#define SKINNY_DEVICETYPE_MUSICONHOLD				70
#define SKINNY_DEVICETYPE_PILOT 					71
#define SKINNY_DEVICETYPE_TAPIPORT					72
#define SKINNY_DEVICETYPE_TAPIROUTEPOINT			73
#define SKINNY_DEVICETYPE_VOICEINBOX				80
#define SKINNY_DEVICETYPE_VOICEINBOXADMIN			81
#define SKINNY_DEVICETYPE_LINEANNUNCIATOR			82
#define SKINNY_DEVICETYPE_ROUTELIST 				90
#define SKINNY_DEVICETYPE_LOADSIMULATOR 			100
#define SKINNY_DEVICETYPE_MEDIATERMINATIONPOINT 	110
#define SKINNY_DEVICETYPE_MEDIATERMINATIONPOINTYOKO	111
#define SKINNY_DEVICETYPE_MGCPSTATION				120
#define SKINNY_DEVICETYPE_MGCPTRUNK 				121
#define SKINNY_DEVICETYPE_RASPROXY					122
#define SKINNY_DEVICETYPE_NOTDEFINED				255

#define SKINNY_DEVICETYPE_CISCO7902 				30008
#define SKINNY_DEVICETYPE_CISCO7905 				20000
#define SKINNY_DEVICETYPE_CISCO7906					369	/*!< 7906 -FS */

#define SKINNY_DEVICETYPE_CISCO7910					6 	/*!< 7910 */
#define SKINNY_DEVICETYPE_CISCO7911 				307
#define SKINNY_DEVICETYPE_CISCO7912					30007

#define SKINNY_DEVICETYPE_CISCO7914 				124 /*!< Expansion module */

/* This are from CCM6 -FS */
#define SKINNY_DEVICETYPE_CISCO7915_12BUTTONS		227	/*!< 7915 12-Button Line Expansion Module */
#define SKINNY_DEVICETYPE_CISCO7915					228	/*!< 7915 24-Button Line Expansion Module */
#define SKINNY_DEVICETYPE_CISCO7916_12BUTTONS		229	/*!< 7916 12-Button Line Expansion Module */
#define SKINNY_DEVICETYPE_CISCO7916					230	/*!< 7916 24-Button Line Expansion Module */

#define SKINNY_DEVICETYPE_CISCO7920 				30002
#define SKINNY_DEVICETYPE_CISCO7921 				365
#define SKINNY_DEVICETYPE_CISCO7925 				484 /*!< 7925 -FS */

#define SKINNY_DEVICETYPE_CISCO7931					348
#define SKINNY_DEVICETYPE_CISCO7935 				9
#define SKINNY_DEVICETYPE_CISCO7936 				30019
#define SKINNY_DEVICETYPE_CISCO7937					431

#define SKINNY_DEVICETYPE_CISCO7940					8 	/*!< 7940 */
#define SKINNY_DEVICETYPE_CISCO7941 				115
#define SKINNY_DEVICETYPE_CISCO7941GE 				309
#define SKINNY_DEVICETYPE_CISCO7942					434
#define SKINNY_DEVICETYPE_CISCO7945					435

#define SKINNY_DEVICETYPE_CISCO7960					7 	/*< 7960 */
#define SKINNY_DEVICETYPE_CISCO7961 				30018
#define SKINNY_DEVICETYPE_CISCO7961GE 				308
#define SKINNY_DEVICETYPE_CISCO7962					404	// -FS
#define SKINNY_DEVICETYPE_CISCO7965 				436

#define SKINNY_DEVICETYPE_CISCO7970 				30006
#define SKINNY_DEVICETYPE_CISCO7971 				119
#define SKINNY_DEVICETYPE_CISCO7975					437

#define SKINNY_DEVICETYPE_CISCO7985					302

#define SKINNY_DEVICETYPE_NOKIA_E_SERIES			275 /*< Nokia S60 */

#define SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR 	30016

#define SKINNY_DEVICETYPE_GATEWAY_AN 				30027 /*< Analog gateway */
#define SKINNY_DEVICETYPE_GATEWAY_BRI 				30028 /*< BRI gateway */


#define SKINNY_DEVICE_RS_NONE					0
#define SKINNY_DEVICE_RS_PROGRESS				1
#define SKINNY_DEVICE_RS_FAILED 				2
#define SKINNY_DEVICE_RS_OK 					3
#define SKINNY_DEVICE_RS_TIMEOUT				4

/* stimulus */
#define SKINNY_STIMULUS_LASTNUMBERREDIAL		1
#define SKINNY_STIMULUS_SPEEDDIAL				2
#define SKINNY_STIMULUS_HOLD					3
#define SKINNY_STIMULUS_TRANSFER				4
#define SKINNY_STIMULUS_FORWARDALL				5
#define SKINNY_STIMULUS_FORWARDBUSY 			6
#define SKINNY_STIMULUS_FORWARDNOANSWER 		7
#define SKINNY_STIMULUS_DISPLAY 				8
#define SKINNY_STIMULUS_LINE					9
#define SKINNY_STIMULUS_T120CHAT				0xA
#define SKINNY_STIMULUS_T120WHITEBOARD			0xB
#define SKINNY_STIMULUS_T120APPLICATIONSHARING	0xC
#define SKINNY_STIMULUS_T120FILETRANSFER		0xD
#define SKINNY_STIMULUS_VIDEO					0xE
#define SKINNY_STIMULUS_VOICEMAIL				0xF
#define SKINNY_STIMULUS_AUTOANSWER				0x11
#define SKINNY_STIMULUS_GENERICAPPB1			0x21
#define SKINNY_STIMULUS_GENERICAPPB2			0x22
#define SKINNY_STIMULUS_GENERICAPPB3			0x23
#define SKINNY_STIMULUS_GENERICAPPB4			0x24
#define SKINNY_STIMULUS_GENERICAPPB5			0x25
#define SKINNY_STIMULUS_MEETMECONFERENCE		0x7b
#define SKINNY_STIMULUS_CONFERENCE				0x7d
#define SKINNY_STIMULUS_CALLPARK				0x7e
#define SKINNY_STIMULUS_CALLPICKUP				0x7f
#define SKINNY_STIMULUS_GROUPCALLPICKUP 		0x80

#define SKINNY_BUTTONTYPE_UNUSED					0x00
#define SKINNY_BUTTONTYPE_LASTNUMBERREDIAL			0x01
#define SKINNY_BUTTONTYPE_SPEEDDIAL 				0x02
#define SKINNY_BUTTONTYPE_HOLD						0x03
#define SKINNY_BUTTONTYPE_TRANSFER					0x04
#define SKINNY_BUTTONTYPE_FORWARDALL				0x05
#define SKINNY_BUTTONTYPE_FORWARDBUSY				0x06
#define SKINNY_BUTTONTYPE_FORWARDNOANSWER			0x07
#define SKINNY_BUTTONTYPE_DISPLAY					0x08
#define SKINNY_BUTTONTYPE_LINE						0x09
#define SKINNY_BUTTONTYPE_T120CHAT					0x0a
#define SKINNY_BUTTONTYPE_T120WHITEBOARD			0x0b
#define SKINNY_BUTTONTYPE_T120APPLICATIONSHARING	0x0c
#define SKINNY_BUTTONTYPE_T120FILETRANSFER			0x0d
#define SKINNY_BUTTONTYPE_VIDEO 					0x0e
#define SKINNY_BUTTONTYPE_VOICEMAIL 				0x0f
#define SKINNY_BUTTONTYPE_ANSWERRELEASE 			0x10
#define SKINNY_BUTTONTYPE_AUTOANSWER				0x11
#define SKINNY_BUTTONTYPE_FEATURE        			0x13
#define SKINNY_BUTTONTYPE_SERVICEURL				0x14


#define SKINNY_BUTTONTYPE_GENERICAPPB1				0x21
#define SKINNY_BUTTONTYPE_GENERICAPPB2				0x22
#define SKINNY_BUTTONTYPE_GENERICAPPB3				0x23
#define SKINNY_BUTTONTYPE_GENERICAPPB4				0x24
#define SKINNY_BUTTONTYPE_GENERICAPPB5				0x25

#define SKINNY_BUTTONTYPE_MESSAGES					0xc2
#define SKINNY_BUTTONTYPE_DIRECTORY					0xc3
#define SKINNY_BUTTONTYPE_APPLICATION				0xc5
#define SKINNY_BUTTONTYPE_HEADSET					0xc6

#define SKINNY_BUTTONTYPE_MEETMECONFERENCE			0x7B
#define SKINNY_BUTTONTYPE_CONFERENCE				0x7D
#define SKINNY_BUTTONTYPE_CALLPARK					0x7E
#define SKINNY_BUTTONTYPE_CALLPICKUP				0x7F
#define SKINNY_BUTTONTYPE_GROUPCALLPICKUP			0x80
#define SKINNY_BUTTONTYPE_KEYPAD					0xf0
#define SKINNY_BUTTONTYPE_AEC						0xfd
#define SKINNY_BUTTONTYPE_UNDEFINED 				0xff

/* this is not a skinny button type. Used for speedial/line buttons */
#define SKINNY_BUTTONTYPE_MULTI 				0xc0
#define SCCP_BUTTONTYPE_LINE					0xc1
#define SCCP_BUTTONTYPE_SPEEDDIAL				0xc2
#define SCCP_BUTTONTYPE_HINT					0xc3


/* unregister */
#define SKINNY_UNREGISTERSTATUS_OK				0
#define SKINNY_UNREGISTERSTATUS_ERROR				1
#define SKINNY_UNREGISTERSTATUS_NAK 				2 	/*< we have an active channel */

/* lamp */
typedef enum {
       SKINNY_LAMP_OFF=1,                                                      /*< Lamp Off, 0% Duty */
       SKINNY_LAMP_ON=2,                                                       /*< Lamp On, 100% Duty */
       SKINNY_LAMP_WINK=3,                                                     /*< Lamp slow blink, ~90% Duty */
       SKINNY_LAMP_FLASH=4,                                            /*< Lamp very fast blink, ~70% Duty */
       SKINNY_LAMP_BLINK=5                                                     /*< Lamp slow blink, ~50% Duty */
} sccp_lampMode_t;

/* media silence suppression */
#define SKINNY_MEDIA_SILENCESUPPRESSION_OFF 			0
#define SKINNY_MEDIA_SILENCESUPPRESSION_ON			1

/* media echo cancel */
#define SKINNY_MEDIA_ECHOCANCELLATION_OFF			0
#define SKINNY_MEDIA_ECHOCANCELLATION_ON			1

/* G723 bitrate */
#define SKINNY_MEDIA_G723BRATE_NONE 				0
#define SKINNY_MEDIA_G723BRATE_5_3				1
#define SKINNY_MEDIA_G723BRATE_6_4				2

/* device reset and restart */
#define SKINNY_DEVICE_RESET 					1
#define SKINNY_DEVICE_RESTART					2

/* statistic processing */
#define SKINNY_STATSPROCESSING_CLEAR				0
#define SKINNY_STATSPROCESSING_DONOTCLEAR			1

/* ringer types */
#define SKINNY_STATION_RINGOFF					1
#define SKINNY_STATION_INSIDERING				2
#define SKINNY_STATION_OUTSIDERING				3
#define SKINNY_STATION_FEATURERING				4
#define SKINNY_STATION_SILENTRING				5
#define SKINNY_STATION_URGENTRING				6

/* speaker status */
#define SKINNY_STATIONSPEAKER_ON				1
#define SKINNY_STATIONSPEAKER_OFF				2

/* mic status */
#define SKINNY_STATIONMIC_ON					1
#define SKINNY_STATIONMIC_OFF					2

/* headset status */
#define SKINNY_STATIONHEADSET_ON				1
#define SKINNY_STATIONHEADSET_OFF				2

/* device dtmfmode */
#define SCCP_DTMFMODE_INBAND					0
#define SCCP_DTMFMODE_OUTOFBAND 				1

#define SCCP_AUTOANSWER_NONE					0 	/*!< 1 way audio - MIC OFF */
#define SCCP_AUTOANSWER_1W						1 	/*!< 1 way audio - MIC OFF */
#define SCCP_AUTOANSWER_2W						2 	/*!< 2 way audio - MIC ON */

#define SCCP_DNDMODE_OFF						0 	/*!< dnd not permitted on the device */
#define SCCP_DNDMODE_REJECT						1 	/*!< busy signal */
#define SCCP_DNDMODE_SILENT						2 	/*!< ringing state with no ringer tone */
#define SCCP_DNDMODE_USERDEFINED				3 	/*!< the user defines the mode by pressing the softkey */

#define SCCP_BLINDTRANSFER_RING					0 	/*!< default */
#define SCCP_BLINDTRANSFER_MOH					1 	/*!< music on hold */

#define MAX_CUSTOM_PICTURES						6
#define MAX_LAYOUT_WITH_SAME_SERVICE			5
#define MAX_SERVICE_TYPE						4
#define DeviceMaxCapabilities       			18  /* max capabilities allowed in Cap response message */
#define StationMaxCapabilities       			DeviceMaxCapabilities
#define StationMaxVideoCapabilities				10
#define StationMaxDataCapabilities   			5
#define MAX_LEVEL_PREFERENCE		 			4

typedef struct {
	uint8_t instance;
	uint8_t type;
	void *ptr;
} btnlist;

typedef struct {
  const char * type;
  int buttonCount;
  const btnlist * buttons;
} button_modes;

typedef enum {
  /* Client -> Server */
  KeepAliveMessage								= 0x0000,
  RegisterMessage								= 0x0001,
  IpPortMessage 								= 0x0002,
  KeypadButtonMessage							= 0x0003,
  EnblocCallMessage 							= 0x0004,
  StimulusMessage								= 0x0005,
  OffHookMessage								= 0x0006,
  OnHookMessage 								= 0x0007,
  HookFlashMessage								= 0x0008,
  ForwardStatReqMessage 						= 0x0009,
  SpeedDialStatReqMessage						= 0x000A,
  LineStatReqMessage							= 0x000B,
  ConfigStatReqMessage							= 0x000C,
  TimeDateReqMessage							= 0x000D,
  ButtonTemplateReqMessage						= 0x000E,
  VersionReqMessage 							= 0x000F,
  CapabilitiesResMessage						= 0x0010,
  MediaPortListMessage							= 0x0011,
  ServerReqMessage								= 0x0012,
  AlarmMessage									= 0x0020,
  MulticastMediaReceptionAck					= 0x0021,
  OpenReceiveChannelAck 						= 0x0022,
  ConnectionStatisticsRes						= 0x0023,
  OffHookWithCgpnMessage						= 0x0024,
  SoftKeySetReqMessage							= 0x0025,
  SoftKeyEventMessage							= 0x0026,
  UnregisterMessage 							= 0x0027,
  SoftKeyTemplateReqMessage 					= 0x0028,
  RegisterTokenReq								= 0x0029,
  HeadsetStatusMessage							= 0x002B,
  MediaResourceNotification 					= 0x002C,
  RegisterAvailableLinesMessage 				= 0x002D,
  DeviceToUserDataMessage						= 0x002E,
  DeviceToUserDataResponseMessage				= 0x002F,
  UpdateCapabilitiesMessage 					= 0x0030,
  OpenMultiMediaReceiveChannelAckMessage		= 0x0031,
  ClearConferenceMessage						= 0x0032,
  ServiceURLStatReqMessage						= 0x0033,
  FeatureStatReqMessage 						= 0x0034,
  CreateConferenceResMessage					= 0x0035,
  DeleteConferenceResMessage					= 0x0036,
  ModifyConferenceResMessage					= 0x0037,
  AddParticipantResMessage						= 0x0038,
  AuditConferenceResMessage 					= 0x0039,
  AuditParticipantResMessage					= 0x0040,
  DeviceToUserDataVersion1Message				= 0x0041,
  DeviceToUserDataResponseVersion1Message		= 0x0042,

  /* This are from protocol V 11 CCM7 */
  DialedPhoneBookMessage						= 0x0048,
  AccessoryStatusMessage						= 0x0049,
  Unknown_0x004A_Message						= 0x004A,

  /* Server -> Client */
  RegisterAckMessage							= 0x0081,
  StartToneMessage								= 0x0082,
  StopToneMessage								= 0x0083,
  // ??
  SetRingerMessage								= 0x0085,
  SetLampMessage								= 0x0086,
  SetHkFDetectMessage							= 0x0087,
  SetSpeakerModeMessage 						= 0x0088,
  SetMicroModeMessage							= 0x0089,
  StartMediaTransmission						= 0x008A,
  StopMediaTransmission 						= 0x008B,
  StartMediaReception							= 0x008C,
  StopMediaReception							= 0x008D,
  // ?
  CallInfoMessage								= 0x008F,

  ForwardStatMessage							= 0x0090,
  SpeedDialStatMessage							= 0x0091,
  LineStatMessage								= 0x0092,
  ConfigStatMessage 							= 0x0093,
  DefineTimeDate								= 0x0094,
  StartSessionTransmission						= 0x0095,
  StopSessionTransmission						= 0x0096,
  ButtonTemplateMessage 						= 0x0097,
  VersionMessage								= 0x0098,
  DisplayTextMessage							= 0x0099,
  ClearDisplay									= 0x009A,
  CapabilitiesReqMessage						= 0x009B,
  EnunciatorCommandMessage						= 0x009C,
  RegisterRejectMessage 						= 0x009D,
  ServerResMessage								= 0x009E,
  Reset 										= 0x009F,

  KeepAliveAckMessage							= 0x0100,
  StartMulticastMediaReception					= 0x0101,
  StartMulticastMediaTransmission				= 0x0102,
  StopMulticastMediaReception					= 0x0103,
  StopMulticastMediaTransmission				= 0x0104,
  OpenReceiveChannel							= 0x0105,
  CloseReceiveChannel							= 0x0106,
  ConnectionStatisticsReq						= 0x0107,
  SoftKeyTemplateResMessage 					= 0x0108,
  SoftKeySetResMessage							= 0x0109,

  SelectSoftKeysMessage 						= 0x0110,
  CallStateMessage								= 0x0111,
  DisplayPromptStatusMessage					= 0x0112,
  ClearPromptStatusMessage						= 0x0113,
  DisplayNotifyMessage							= 0x0114,
  ClearNotifyMessage							= 0x0115,
  ActivateCallPlaneMessage						= 0x0116,
  DeactivateCallPlaneMessage					= 0x0117,
  UnregisterAckMessage							= 0x0118,
  BackSpaceReqMessage							= 0x0119,
  RegisterTokenAck								= 0x011A,
  RegisterTokenReject							= 0x011B,
  StartMediaFailureDetection					= 0x011C,
  DialedNumberMessage							= 0x011D,
  UserToDeviceDataMessage						= 0x011E,
  FeatureStatMessage							= 0x011F,
  DisplayPriNotifyMessage						= 0x0120,
  ClearPriNotifyMessage 						= 0x0121,
  StartAnnouncementMessage						= 0x0122,
  StopAnnouncementMessage						= 0x0123,
  AnnouncementFinishMessage 					= 0x0124,
  NotifyDtmfToneMessage 						= 0x0127,
  SendDtmfToneMessage							= 0x0128,
  SubscribeDtmfPayloadReqMessage				= 0x0129,
  SubscribeDtmfPayloadResMessage				= 0x012A,
  SubscribeDtmfPayloadErrMessage				= 0x012B,
  UnSubscribeDtmfPayloadReqMessage				= 0x012C,
  UnSubscribeDtmfPayloadResMessage				= 0x012D,
  UnSubscribeDtmfPayloadErrMessage				= 0x012E,
  ServiceURLStatMessage 						= 0x012F,
  CallSelectStatMessage 						= 0x0130,
  OpenMultiMediaChannelMessage					= 0x0131,
  StartMultiMediaTransmission					= 0x0132,
  StopMultiMediaTransmission					= 0x0133,
  MiscellaneousCommandMessage					= 0x0134,
  FlowControlCommandMessage 					= 0x0135,
  CloseMultiMediaReceiveChannel 				= 0x0136,
  CreateConferenceReqMessage					= 0x0137,
  DeleteConferenceReqMessage					= 0x0138,
  ModifyConferenceReqMessage					= 0x0139,
  AddParticipantReqMessage						= 0x013A,
  DropParticipantReqMessage 					= 0x013B,
  AuditConferenceReqMessage 					= 0x013C,
  AuditParticipantReqMessage					= 0x013D,
  UserToDeviceDataVersion1Message				= 0x013F,

  /* sent by us */
  Unknown_0x0143_Message						= 0x0143,
  Unknown_0x0144_Message						= 0x0144,
  DisplayDynamicPromptStatusMessage				= 0x0145,
  LineStatDynamicMessage						= 0x0147,
  SpeedDialStatDynamicMessage					= 0x0149,
  CallInfoDynamicMessage						= 0x014A,

  /* received from phone */
  StartMediaTransmissionAck						= 0x0154,
  Unknown_0x0159_Message						= 0x0159
} sccp_message_t;

#define SCCP_ACCESSORY_NONE				0x00 /* Added for compatibility with old phones -FS */
#define SCCP_ACCESSORY_HEADSET			0x01
#define SCCP_ACCESSORY_HANDSET			0x02
#define SCCP_ACCESSORY_SPEAKER			0x03

#define SCCP_ACCESSORYSTATE_NONE		0x00 /* Added for compatibility with old phones -FS */
#define SCCP_ACCESSORYSTATE_OFFHOOK		0x01
#define SCCP_ACCESSORYSTATE_ONHOOK		0x02

typedef struct {
  char					deviceName[StationMaxDeviceNameSize];
  uint32_t				lel_userid;
  uint32_t				lel_instance;
} StationIdentifier;

typedef struct {
  uint8_t instanceNumber;	/*< set to instance number or StationKeyPadButton value */
  uint8_t buttonDefinition; /*< set to one of the preceding Bt values */
} StationButtonDefinition;

typedef struct {
  uint32_t lel_payloadCapability;
  uint32_t lel_maxFramesPerPacket;
  union {
	uint8_t futureUse[8];
	uint32_t lel_g723BitRate;
  } PAYLOADS;
} MediaCapabilityStructure;

typedef struct {
  char		 softKeyLabel[StationMaxSoftKeyLabelSize];
  uint32_t lel_softKeyEvent;
} StationSoftKeyDefinition;

typedef struct {
  uint8_t  softKeyTemplateIndex[StationMaxSoftKeyIndex];
  uint16_t les_softKeyInfoIndex[StationMaxSoftKeyIndex];
} StationSoftKeySetDefinition;

typedef struct{
  char		 serverName[ServerMaxNameSize];
} ServerIdentifier;

/* structure for UpdateCapabilitiesMessage
 * \since 20080111 */
typedef struct{
	uint32_t	layout;
} layoutConfig_t;

typedef struct{
	uint32_t	transmitPreference;
	uint32_t	format;
	uint32_t	maxBitRate;
	uint32_t	minBitRate;
	uint32_t	MPI;
	uint32_t	serviceNumber;
} levelPreference_t;

typedef struct{
	uint32_t		layoutCount;
	layoutConfig_t	layout[MAX_LAYOUT_WITH_SAME_SERVICE];
	uint32_t		serviceNum;
	uint32_t		maxStreams;
	uint32_t		maxConferences;
	uint32_t		activeConferenceOnRegistration;
} serviceResource_t;

typedef struct{
	uint32_t	customPictureFormatWidth;
	uint32_t	customPictureFormatHeight;
	uint32_t	customPictureFormatpixelAspectRatio;
	uint32_t	customPictureFormatpixelclockConversionCode;
	uint32_t	customPictureFormatpixelclockDivisor;
} customPictureFormat_t;

typedef struct{
	uint32_t	lel_payloadCapability;
	uint32_t	lel_maxFramesPerPacket;
	uint32_t	lel_unknown[2]; // this are related to G.723
} audioCaps_t;

typedef struct{
	uint32_t	lel_payloadCapability;
	uint32_t	lel_transmitOrReceive;
	uint32_t	lel_levelPreferenceCount;

	levelPreference_t	levelPreference[MAX_LEVEL_PREFERENCE];

	uint32_t	lel_codec_options[2];

	/**
	 * Codec options contains data specific for every codec
	 *
	 * Here is a list of known parameters per codec
		// H.261
		uint32_t	lel_temporalSpatialTradeOffCapability;
		uint32_t	lel_stillImageTransmission;

		// H.263
		uint32_t	lel_h263_capability_bitfield;
		uint32_t	lel_annexNandWFutureUse;

		// Video
		uint32_t	lel_modelNumber;
		uint32_t	lel_bandwidth;
	*/
} videoCap_t;


typedef struct{
	uint32_t	payloadCapability;
	uint32_t	transmitOrReceive;
	uint32_t	protocolDependentData;
	uint32_t	maxBitRate;
} dataCaps_t;

typedef union {

	  /* 0x0048 is phone call list update (comes from phone 7921
	   * phones when you press the dial button without a number.
	   * It looks like this:
	   *
	   * 00000000 - 00 00 00 00 01 00 00 00 4B 0D 00 00 34 30 30 30 ........K...4000
	   * 00000010 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000020 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000030 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000040 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000050 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000060 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000070 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000080 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000090 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 000000F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * 00000100 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	   * First DWORD is Index shifted of 4 bits (i think the 4 bits param is something)
	   * Second DWORD is line instance related to the call
	   * Third DWORD is unknown, Next there is the phone number [260 bytes]
	   */

	struct {
		uint32_t	lel_NumberIndex; /* this must be shifted 4 bits right */
		uint32_t	lel_lineinstance;
		uint32_t	lel_unknown;
		char		phonenumber[260]; /* i don't know if this is exact */
	} DialedPhoneBookMessage;

	/* AccessoryStatusMessage (0x0073):
	 * This indicates the phone headset, handset or speaker status.
	 *
	 * FIRST DWORD:  0x0001 = HEADSET, 0x0002 = HANDSET, 0x0003 = SPEAKERMODE
	 * SECOND DWORD: 0x0001 = OFFHOOK, 0x0002 = ONHOOK
	 * THIRD DWORD:  ALWAYS 0x0000 UNKNOWN
	 */
	struct {
		uint32_t	lel_AccessoryID;
		uint32_t	lel_AccessoryStatus;
		uint32_t	lel_unknown;
	} AccessoryStatusMessage;

	// Message 0x4A len 12 (cisco ip communicator uses it)
	// 00000000 - 03 00 00 00 01 00 00 00 00 00 00 00             ............
	struct {
		uint32_t	lel_unknown1;
		uint32_t	lel_unknown2;
		uint32_t	lel_unknown3;
	} Unknown_0x004A_Message;

	struct { // INCOMPLETE
		uint32_t nn;
	} Unknown_0x0143_Message;

	// Message 0x144 len 0x10
	// 0000   14 00 00 00 00 00 00 00 44 01 00 00 0a 00 00 00  ........D.......
	// 0010   05 00 00 00 80 17 31 30 34 00 00 00              ......104...
	struct { // INCOMPLETE
		uint32_t nn;
	} Unknown_0x0144_Message; // this set caller and called id

	struct {
		uint32_t	lel_messageTimeout;
		uint32_t 	lel_lineInstance;
		uint32_t 	lel_callReference;
		/* here follow the message string
		 * take care rest of size should
		 * be calculated with string
		 * size + 1 (NULL TERMINATION)
		 * padded by 4
		 */
		uint32_t	dummy; // this is just for addressing, it doesn't matter
	} DisplayDynamicPromptStatusMessage;  // this is just the header :)

	struct {
		uint32_t lel_lineNumber;
		uint32_t lel_lineType;
		uint32_t dummy;
	} LineStatDynamicMessage;

	struct {
		uint32_t lel_speedDialNumber;
		uint32_t dummy;
	} SpeedDialStatDynamicMessage;

	struct {
		uint32_t lel_lineId;
		uint32_t lel_callRef;
		uint32_t lel_callType; 							/*!< INBOUND=1, OUTBOUND=2, FORWARD=3 */
		uint32_t originalCdpnRedirectReason;
		uint32_t lastRedirectingReason;
		uint32_t lel_callInstance;
		uint32_t lel_callSecurityStatus;
		uint32_t partyPIRestrictionBits;					/*!< partyPIRestrictionBits struct
															0 RestrictCallingPartyName
															1 RestrictCallingPartyNumber
															2 RestrictCalledPartyName
															3 RestrictCalledPartyNumber
															4 RestrictOriginalCalledPartyName
															5 RestrictOriginalCalledPartyNumber
															6 RestrictLastRedirectPartyName
															7 RestrictLastRedirectPartyNumber */
		uint32_t dummy;
/*
 * 	Here there are the following informations:
 *
		char callingParty[StationMaxDirnumSize];
		char calledParty[StationMaxDirnumSize];
		char originalCalledParty[StationMaxDirnumSize];
		char lastRedirectingParty[StationMaxDirnumSize];
		char cgpnVoiceMailbox[StationMaxDirnumSize];
		char cdpnVoiceMailbox[StationMaxDirnumSize];
		char originalCdpnVoiceMailbox[StationMaxDirnumSize];
		char lastRedirectingVoiceMailbox[StationMaxDirnumSize];
		char callingPartyName[StationMaxNameSize];
		char calledPartyName[StationMaxNameSize];
		char originalCalledPartyName[StationMaxNameSize];
		char lastRedirectingPartyName[StationMaxNameSize];
*/
	} CallInfoDynamicMessage;

	/*
	 * Unhandled SCCP Message: unknown(0x0159) 168 bytes length
	 * 00000000 - 01 00 00 00 01 00 00 00 02 00 00 00 00 00 00 00 ................
	 * 00000010 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000020 - 00 00 00 00 6D 61 78 2D 63 68 61 69 6E 65 64 3D ....max-chained=
	 * 00000030 - 32 20 64 65 76 69 63 65 4C 69 6E 65 3D 31 34 00 2 deviceLine=14.
	 * 00000040 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000050 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000060 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000070 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000080 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000090 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 000000A0 - 00 00 00 00 00 00 00 00                         ........
	 *
	 *	This was sent by a single 7970 with one CP-7914 attached.
	 *	Message is related to Addons and i suppose it notifies their
	 *	number, capacity and index.
	 */

	struct {
		char dummy[168];
	} Unknown_0x0159_Message;

	struct {
		uint32_t lel_callReference;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference1;
		uint32_t lel_unknown1;				/*	i think this switches from IPv4 to IPv6 (0x00 in IPv4) */
		/* include IPv6 support */
		char bel_ipAddr[16]; 				/*!< This field is apparently in big-endian format,
												even though most other fields are in
												little-endian format. */
		uint32_t lel_portNumber;
		uint32_t lel_smtStatus; 			/*!< startmediatransmission status */
		uint32_t lel_unknown2;
	} StartMediaTransmissionAck;

	/* this is from a 7941
	 Unhandled SCCP Message: unknown(0x0154) 44 bytes length
	 00000000 - 03 00 00 00 FC FF FF FF 03 00 00 00 00 00 00 00 ................
	 00000010 - C0 A8 09 24 00 00 00 00 00 00 00 00 00 00 00 00 ...$............
	 00000020 - 00 00 00 00 03 00 00 00 00 00 00 00             ............
	 */
	struct {
		uint32_t lel_callReference;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference1;
		uint32_t lel_unknown1;				/*	i think this switches from IPv4 to IPv6 (0x00 in IPv4) */
		/* include IPv6 support */
		char bel_ipAddr[16]; 				/*!< This field is apparently in big-endian format,
												even though most other fields are in
												little-endian format. */
		uint32_t lel_portNumber;
		uint32_t lel_smtStatus; 			/*!< startmediatransmission status */
		uint32_t lel_unknown2;
	} StartMediaTransmissionAck_v17;

	// No struct
  struct { } StationKeepAliveMessage;

  struct {
	StationIdentifier sId;
	uint32_t		lel_stationIpAddr;
	uint32_t		lel_deviceType;
	uint32_t		lel_maxStreams;
	uint32_t		lel__unknown1;
	uint8_t 		protocolVer;
	uint8_t 		lel__unknown2;
	uint8_t 		lel__unknown3;
	uint8_t 		lel__unknown4;
	uint32_t		lel__unknown5;
	uint32_t		lel__unknown6;
	uint32_t		lel__unknown7;

	// 7910:
	// 02 00 00 00 // protocolVer (1st bit)
	// 08 00 00 00 == 8
	// 00 00 00 00
	// 02 00 00 00 == 2
	// ce f1 00 00 // == (61092 / 206 / 241) 1668 dn-size 420
  } RegisterMessage;

  struct {

	// All char arrays are in multiples of 32bit

	char			deviceName[StationMaxDeviceNameSize];
	uint32_t		lel_stationUserId;
	uint32_t		lel_stationInstance;
	char			userName[StationMaxNameSize];
	char			serverName[StationMaxNameSize];
	uint32_t		lel_numberLines;
	uint32_t		lel_numberSpeedDials;
  } ConfigStatMessage;

  struct {
	uint16_t 		les_rtpMediaPort;
	uint16_t 		unknown;
  } IpPortMessage;

  struct {
	uint32_t lel_kpButton;
	uint32_t lel_lineInstance;
	uint32_t lel_callReference;
  } KeypadButtonMessage;

  struct {
	char calledParty[StationMaxDirnumSize];
  } EnblocCallMessage;

  struct {
	uint32_t lel_stimulus;
	uint32_t lel_stimulusInstance; /* normally set to 1 (except speed dial and line) */
  } StimulusMessage;


	struct {} OffHookMessage;
	struct {} OnHookMessage;
	struct {} HookFlashMessage;

	struct {
		uint32_t lel_deviceType;
		uint32_t lel_numberOfInServiceStreams;
		uint32_t lel_maxStreamsPerConf;
		uint32_t lel_numberOfOutOfServiceStreams;
	} MediaResourceNotification;

	struct {
		uint32_t lel_lineNumber;
	} ForwardStatReqMessage;

	struct {
		uint32_t lel_speedDialNumber;
	} SpeedDialStatReqMessage;

	struct {
		uint32_t lel_lineNumber;
	} LineStatReqMessage;

	struct {} ConfigStatReqMessage;
	struct {} TimeDateReqMessage;
	struct {} ButtonTemplateReqMessage;
	struct {} VersionReqMessage;

	struct {
		uint32_t					 lel_count;
		MediaCapabilityStructure caps[18];
	} CapabilitiesResMessage;

	struct {} MediaPortListMessage;
	struct {} ServerReqMessage;

	struct {
		uint32_t lel_alarmSeverity;
		char	 text[80];
		uint32_t lel_parm1;
		uint32_t lel_parm2;
	} AlarmMessage;

	/* 0x34 FeatureStatReqMessage */
	struct {
		uint32_t lel_featureInstance;
	} FeatureStatReqMessage;

	struct {
		uint32_t lel_serviceURLIndex;
	} ServiceURLStatReqMessage;

/*
	[Jan  6 17:05:50] WARNING[29993]: sccp_actions.c:59 sccp_handle_unknown_message:
	Unhandled SCCP Message: 48 - UpdateCapabilitiesMessage with length 1844

	 This is from a 7970 with no video caps
	 00000000 - 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000010 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000020 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000030 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000040 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000050 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000060 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000070 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000080 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000090 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000100 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000110 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000120 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000130 - 00 00 00 00 00 00 00 00 19 00 00 00 28 00 00 00 ............(...
	 00000140 - 00 00 00 00 00 00 00 00 04 00 00 00 28 00 00 00 ............(...
	 00000150 - 00 00 00 00 00 00 00 00 02 00 00 00 28 00 00 00 ............(...
	 00000160 - 00 00 00 00 00 00 00 00 0F 00 00 00 3C 00 00 00 ............<...
	 00000170 - 00 00 00 00 00 00 00 00 10 00 00 00 3C 00 00 00 ............<...
	 00000180 - 00 00 00 00 00 00 00 00 0B 00 00 00 3C 00 00 00 ............<...
	 00000190 - 00 00 00 00 00 00 00 00 0C 00 00 00 3C 00 00 00 ............<...
	 000001A0 - 00 00 00 00 00 00 00 00 01 01 00 00 01 00 00 00 ................
	 000001B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000200 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000210 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000220 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000230 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000240 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000250 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000260 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000270 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000280 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000290 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000300 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000310 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000320 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000330 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000340 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000350 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000360 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000370 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000380 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000390 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000400 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000410 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000420 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000430 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000440 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000450 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000460 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000470 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000480 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000490 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000500 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000510 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000520 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000530 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000540 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000550 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000560 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000570 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000580 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000590 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000600 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000610 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000620 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000630 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000640 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000650 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000660 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000670 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000680 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000690 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000700 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000710 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000720 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000730 - 00 00 00 00                                     ....

	 this is from a 7940 with Video Advantage on PC Port
	 00000000 - 08 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000010 - 00 00 00 00 22 4B 01 00 0C CC CC CC 00 11 21 F1 ...."K..........
	 00000020 - 22 4B 00 74 AA AA 03 00 00 0C 20 00 02 B4 B8 67 "K.t...... ....g
	 00000030 - 00 01 00 13 53 45 50 30 30 31 31 32 31 46 31 32 ....SEP001122334
	 00000040 - 32 34 42 00 02 00 11 00 00 00 01 01 01 CC 00 04 455.............
	 00000050 - 0A 0A 0A D2 00 03 00 0A 50 6F 72 74 20 31 00 04 ........Port 1..
	 00000060 - 00 08 00 00 00 90 00 05 00 10 50 30 30 33 30 38 ..........P00308
	 00000070 - 30 30 31 30 30 30 00 06 00 17 43 69 73 63 6F 20 001000....Cisco
	 00000080 - 49 50 20 50 68 6F 6E 65 20 37 39 34 00 00 00 00 IP Phone 794....
	 00000090 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000000F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000100 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000110 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000120 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000130 - 00 00 00 00 00 00 00 00 19 00 00 00 78 00 00 00 ............x...
	 00000140 - 00 00 00 00 00 00 00 00 04 00 00 00 28 00 00 00 ............(...
	 00000150 - 00 00 00 00 00 00 00 00 02 00 00 00 28 00 00 00 ............(...
	 00000160 - 00 00 00 00 00 00 00 00 0F 00 00 00 3C 00 00 00 ............<...
	 00000170 - 00 00 00 00 00 00 00 00 10 00 00 00 3C 00 00 00 ............<...
	 00000180 - 00 00 00 00 00 00 00 00 0B 00 00 00 3C 00 00 00 ............<...
	 00000190 - 00 00 00 00 00 00 00 00 0C 00 00 00 3C 00 00 00 ............<...
	 000001A0 - 00 00 00 00 00 00 00 00 01 01 00 00 04 00 00 00 ................
	 000001B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000001F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000200 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000210 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000220 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000230 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000240 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000250 - 00 00 00 00 00 00 00 00 65 00 00 00 03 00 00 00 ........e.......
	 00000260 - 02 00 00 00 32 00 00 00 02 00 00 00 98 3A 00 00 ....2........:..
	 00000270 - F4 01 00 00 01 00 00 00 00 00 00 00 33 00 00 00 ............3...
	 00000280 - 03 00 00 00 98 3A 00 00 F4 01 00 00 01 00 00 00 .....:..........
	 00000290 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000002C0 - 00 00 00 00 00 00 00 00 00 00 00 00 67 00 00 00 ............g...
	 000002D0 - 03 00 00 00 01 00 00 00 34 00 00 00 03 00 00 00 ........4.......
	 000002E0 - 98 3A 00 00 F4 01 00 00 01 00 00 00 00 00 00 00 .:..............
	 000002F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000300 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000310 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000320 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000330 - 00 00 00 00 00 00 00 00 40 00 00 00 32 00 00 00 ........@...2...
	 00000340 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000350 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000360 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000370 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000380 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000390 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000003F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000400 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000410 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000420 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000430 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000440 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000450 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000460 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000470 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000480 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000490 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000004F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000500 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000510 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000520 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000530 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000540 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000550 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000560 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000570 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000580 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000590 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000005F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000600 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000610 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000620 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000630 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000640 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000650 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000660 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000670 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000680 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000690 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006A0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006B0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006C0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006D0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006E0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 000006F0 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000700 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000710 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000720 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 00000730 - 00 00 00 00                                     ....
*/
	struct {
		uint32_t	lel_audioCapCount;
		uint32_t	lel_videoCapCount;
		uint32_t	lel_dataCapCount;
		uint32_t	RTPPayloadFormat;
		uint32_t	customPictureFormatCount;

		customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES];

		uint32_t	activeStreamsOnRegistration;
		uint32_t	maxBW;
		uint32_t	serviceResourceCount;

		serviceResource_t serviceResource[MAX_SERVICE_TYPE];

		audioCaps_t	audioCaps[StationMaxCapabilities];
		videoCap_t	videoCaps[StationMaxVideoCapabilities];
		dataCaps_t	dataCaps[StationMaxDataCapabilities];

		uint32_t	unknown;
	} UpdateCapabilitiesMessage;
	/*
	struct {
		char dummy[1844];
	} UpdateCapabilitiesMessage;
	*/

	struct {
		uint32_t	receptionStatus;
		uint32_t	passThruPartyID;
	} MulticastMediaReceptionAck;

	struct {
		uint32_t lel_orcStatus; 			/*!< receiveChanStatus */
		uint32_t bel_ipAddr; 				/*!< This field is apparently in big-endian format,
												even though most other fields are in
												little-endian format. */
		uint32_t lel_portNumber;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference;
	} OpenReceiveChannelAck;

	/*	this is v17 open receive channel ack
	 * 	0000   28 00 00 00 11 00 00 00 22 00 00 00 ST AT US xx  (.......".......
	 *	0010   00 00 00 00 IP IP IP IP 00 00 00 00 00 00 00 00  ......."........
	 *	0020   00 00 00 00 3a 4c 00 00 53 00 00 01 54 ae 9d 01  ....:L..S...T...
	 */
	struct {
		uint32_t lel_orcStatus; 			/*!< receiveChanStatus */
		uint32_t lel_unknown1;				/*	i think this switches from IPv4 to IPv6 (0x00 in IPv4) */
		/* include IPv6 support */
		char bel_ipAddr[16]; 				/*!< This field is apparently in big-endian format,
												even though most other fields are in
												little-endian format. */
		uint32_t lel_portNumber;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference;
	} OpenReceiveChannelAck_v17;

	struct {
		char		DirectoryNumber[StationMaxDirnumSize];
		uint32_t	lel_CallIdentifier;
		uint32_t	lel_StatsProcessingType;
		uint32_t	lel_SentPackets;
		uint32_t	lel_SentOctets;
		uint32_t	lel_RecvdPackets;
		uint32_t	lel_RecvdOctets;
		uint32_t	lel_LostPkts;
		uint32_t	lel_Jitter;
		uint32_t	lel_latency;
	} ConnectionStatisticsRes;


	struct {
		char    calledParty[StationMaxDirnumSize];
	} OffHookWithCgpnMessage;
	struct {} SoftKeySetReqMessage;

	struct {
		uint32_t lel_softKeyEvent;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} SoftKeyEventMessage;

	struct {} UnregisterMessage;
	struct {} SoftKeyTemplateReqMessage;
	struct {} RegisterTokenReq;

	struct {
		uint32_t lel_hsMode;
	} HeadsetStatusMessage;

	struct {
		uint32_t lel_keepAliveInterval;
		char	 dateTemplate[StationDateTemplateSize];
		// uint16_t les__filler1;
		uint8_t	filler1;
		uint8_t filler2;
		uint32_t lel_secondaryKeepAliveInterval;
		uint8_t  protocolVer;
		uint8_t  unknown1;
		uint8_t  unknown2;
		uint8_t  unknown3;
	} RegisterAckMessage;

	struct {
		uint32_t lel_tone;
		uint32_t lel_toneTimeout;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} StartToneMessage;

	struct {
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		/* protocol v12 mod */
		uint32_t lel_unknown1;
	} StopToneMessage;

	struct {
		uint32_t lel_ringMode;
		uint32_t lel_unknown1; /* always 1 */
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} SetRingerMessage;

	struct {
		uint32_t lel_stimulus;
		uint32_t lel_stimulusInstance;
		uint32_t lel_lampMode;
	} SetLampMessage;

	struct {} SetHkFDetectMessage;

	struct {
		uint32_t lel_speakerMode;
	} SetSpeakerModeMessage;

	struct {
		uint32_t lel_micMode;
	} SetMicroModeMessage;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t bel_remoteIpAddr; 						/*!< This field is apparently in big-endian
														format, even though most other fields are
														little-endian. */
		uint32_t lel_remotePortNumber;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadType; 						/*!< Media_PayloadType */
		uint32_t lel_precedenceValue;
		uint32_t lel_ssValue;
		uint32_t lel_maxFramesPerPacket;
		uint32_t lel_g723BitRate; 						/*!< only used with G.723 payload */
		uint32_t lel_conferenceId1;
		uint32_t  unknown1;
		uint32_t  unknown2;
		uint32_t  unknown3;
		uint32_t  unknown4;
		uint32_t  unknown5;
		uint32_t  unknown6;
		uint32_t  unknown7;
		uint32_t  unknown8;
		uint32_t  unknown9;
		uint32_t  unknown10;
		/* protocol v11 mods */
		uint32_t unknown11;
		uint32_t unknown12;
		uint32_t unknown13;
		uint32_t unknown14;
		uint32_t lel_rtpDTMFPayload; /* this is often set to 0x65 (101) */
		uint32_t lel_rtptimeout;
		/* protocol v11 fields */
		uint32_t unknown15;
		uint32_t unknown16;
	} StartMediaTransmission;

	/* StartMediaTransmission v17
	 * 0000   88 00 00 00 11 00 00 00 8a 00 00 00 54 ae 9d 01  ............T...
	 * 0010   53 00 00 01 00 00 00 00 c0 a8 09 2c 00 00 00 00  S..........,....
	 * 0020   00 00 00 00 00 00 00 00 c6 4c 00 00 14 00 00 00  .........L......
	 * 0030   04 00 00 00 b8 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0040   00 00 00 00 54 ae 9d 01 00 00 00 00 00 00 00 00  ....T...........
	 * 0050   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0060   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0070   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0080   00 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00  ................
	 */
	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_unknown1;
		char bel_remoteIpAddr[16]; 						/*!< This field is apparently in big-endian
														format, even though most other fields are
														little-endian. */
		uint32_t lel_remotePortNumber;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadType; 						/*!< Media_PayloadType */
		uint32_t lel_precedenceValue;
		uint32_t lel_ssValue;
		uint32_t lel_maxFramesPerPacket;
		uint32_t lel_g723BitRate; 						/*!< only used with G.723 payload */
		uint32_t lel_conferenceId1;
		uint32_t lel_unknown2;
		uint32_t lel_unknown3;
		uint32_t lel_unknown4;
		uint32_t lel_unknown5;
		uint32_t lel_unknown6;
		uint32_t lel_unknown7;
		uint32_t lel_unknown8;
		uint32_t lel_unknown9;
		uint32_t lel_unknown10;
		uint32_t lel_unknown11;
		uint32_t lel_unknown12;
		uint32_t lel_unknown13;
		uint32_t lel_unknown14;
		uint32_t lel_unknown15;
		uint32_t lel_rtpDTMFPayload; /* this is often set to 0x65 (101) */
		uint32_t lel_rtptimeout; /* this is set to 0x0A */
		uint32_t lel_unknown18;
		uint32_t lel_unknown19;
	} StartMediaTransmission_v17;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_conferenceId1;
		uint32_t lel_unknown1; /* proto v17 */
	} StopMediaTransmission;

	struct {} StartMediaReception;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
	} StopMediaReception;

	struct {
		char callingPartyName[StationMaxNameSize];
		char callingParty[StationMaxDirnumSize];
		char calledPartyName[StationMaxNameSize];
		char calledParty[StationMaxDirnumSize];
		uint32_t  lel_lineId;
		uint32_t  lel_callRef;
		uint32_t  lel_callType; 							/*!< INBOUND=1, OUTBOUND=2, FORWARD=3 */
		char originalCalledPartyName[StationMaxNameSize];
		char originalCalledParty[StationMaxDirnumSize];
		char lastRedirectingPartyName[StationMaxNameSize];
		char lastRedirectingParty[StationMaxDirnumSize];
		uint32_t originalCdpnRedirectReason;
		uint32_t lastRedirectingReason;
		char cgpnVoiceMailbox[StationMaxDirnumSize];
		char cdpnVoiceMailbox[StationMaxDirnumSize];
		char originalCdpnVoiceMailbox[StationMaxDirnumSize];
		char lastRedirectingVoiceMailbox[StationMaxDirnumSize];
		uint32_t lel_callInstance;
		uint32_t lel_callSecurityStatus;
		uint32_t partyPIRestrictionBits;					/*!< partyPIRestrictionBits struct
															0 RestrictCallingPartyName
															1 RestrictCallingPartyNumber
															2 RestrictCalledPartyName
															3 RestrictCalledPartyNumber
															4 RestrictOriginalCalledPartyName
															5 RestrictOriginalCalledPartyNumber
															6 RestrictLastRedirectPartyName
															7 RestrictLastRedirectPartyNumber */
	} CallInfoMessage;

	struct {
		char calledParty[StationMaxDirnumSize];
		uint32_t  lel_lineId;
		uint32_t  lel_callRef;
	} DialedNumberMessage;

	struct {
		uint32_t lel_status; 								/*!< 0 inactive 1 active */
		uint32_t lel_lineNumber;
		uint32_t lel_cfwdallstatus;
		char cfwdallnumber[StationMaxDirnumSize];
		uint32_t lel_cfwdbusystatus;
		char cfwdbusynumber[StationMaxDirnumSize];
		uint32_t lel_cfwdnoanswerstatus;
		char cfwdnoanswernumber[StationMaxDirnumSize];
	} ForwardStatMessage;

	struct {
		uint32_t lel_speedDialNumber;
		char	 speedDialDirNumber[StationMaxDirnumSize];
		char	 speedDialDisplayName[StationMaxNameSize];
	} SpeedDialStatMessage;

	struct {
		uint32_t lel_lineNumber;
		char	lineDirNumber[StationMaxDirnumSize];
		char	lineFullyQualifiedDisplayName[StationMaxNameSize];
		char	lineDisplayName[StationMaxButtonTemplateNameSize];
	} LineStatMessage;

	struct {
		uint32_t lel_year;
		uint32_t lel_month;
		uint32_t lel_dayOfWeek;
		uint32_t lel_day;
		uint32_t lel_hour;
		uint32_t lel_minute;
		uint32_t lel_seconds;
		uint32_t lel_milliseconds;
		uint32_t lel_systemTime;
	} DefineTimeDate;

	struct {} StartSessionTransmission;
	struct {} StopSessionTransmission;

	struct {
		uint32_t					lel_buttonOffset;
		uint32_t					lel_buttonCount;
		uint32_t					lel_totalButtonCount;
		StationButtonDefinition definition[StationMaxButtonTemplateSize];
	} ButtonTemplateMessage;

	struct {
		char requiredVersion[StationMaxVersionSize];
	} VersionMessage;

	struct {
		char	 displayMessage[StationMaxDisplayTextSize];
/*		uint32_t lel_displayTimeout; */
	} DisplayTextMessage;

	struct {
		uint32_t unknown;
	} ClearDisplay;

	struct {} CapabilitiesReqMessage;
	struct {} EnunciatorCommandMessage;

	struct {
		char text[StationMaxDisplayTextSize];
	} RegisterRejectMessage;


	struct {
		ServerIdentifier	server[5];
		uint32_t		serverListenPort[5];
		uint32_t		serverIpAddr[5];
	} ServerResMessage;


	struct {
		uint32_t lel_resetType;
	} Reset;

	struct {} KeepAliveAckMessage;
	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_passThruPartyID;
		uint32_t lel_multicastIpAddress;
		uint32_t lel_multicastPort;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadCapability;
		uint32_t lel_echoCancelType;
		uint32_t lel_g723BitRate;
	} StartMulticastMediaReception;

	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_passThruPartyID;
		uint32_t lel_multicastIpAddress;
		uint32_t lel_multicastPort;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadCapability;
		uint32_t lel_precedenceValue;
		uint32_t lel_silenceSuppression;
		uint32_t lel_maxFramesPerPacket;
		uint32_t lel_g723BitRate;
	} StartMulticastMediaTransmission;

	/*
	 * based on miscCommandType we have other struct
	 * following the Miscellaneous Command Message
	 */
	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference;
		uint32_t lel_miscCommandType;
	} MiscellaneousCommandMessage;

	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_passThruPartyID;
	} StopMulticastMediaReception;

	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_passThruPartyID;
	} StopMulticastMediaTransmission;

	/* this is v11 message from ccm7
	 * 0000   60 00 00 00 00 00 00 00 05 01 00 00 5b ae 9d 01  `...........[...
	 * 0010   5a 00 00 01 14 00 00 00 04 00 00 00 00 00 00 00  Z...............
	 * 0020   00 00 00 00 5b ae 9d 01 00 00 00 00 00 00 00 00  ....[...........
	 * 0030   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0040   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0050   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0060   00 00 00 00 0a 00 00 00                          ........
	 */
	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadType; 						/*!< Media_PayloadType */
		uint32_t lel_vadValue;
		uint32_t lel_g723BitRate; 						/*!< only used with G.723 payload */
		/* protocol version 5 fields */
		uint32_t lel_conferenceId1;
		uint32_t unknown1;
		uint32_t unknown2;
		uint32_t unknown3;
		uint32_t unknown4;
		uint32_t unknown5;
		uint32_t unknown6;
		uint32_t unknown7;
		uint32_t unknown8;
		uint32_t unknown9;
		uint32_t unknown10;
		/* protocol version 11 fields */
		uint32_t unknown11;
		uint32_t unknown12;
		uint32_t unknown13;
		uint32_t unknown14;
		uint32_t lel_rtpDTMFPayload; /* this is often set to 0x65 (101) */
		uint32_t lel_rtptimeout; /* this is always 0x0A */
		/* protocol version 15 fields */
		uint32_t unknown15;
		uint32_t unknown16;
		char 	 bel_remoteIpAddr[16];
		uint32_t lel_unknown17; /* this is always 0xFA0 */
	} OpenReceiveChannel;

	/*	OpenReceiveChannel v17
	 * 0000   84 00 00 00 11 00 00 00 05 01 00 00 ec bc 68 01  ..............h.
	 * 0010   07 00 00 01 14 00 00 00 04 00 00 00 00 00 00 00  ................
	 * 0020   00 00 00 00 ec bc 68 01 00 00 00 00 00 00 00 00  ......h.........
	 * 0030   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0040   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0050   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0060   00 00 00 00 0a 00 00 00 00 00 00 00 00 00 00 00  ................
	 * 0070   00 00 00 00 c0 a8 09 2c 00 00 00 00 00 00 00 00  .......,........
	 * 0080   00 00 00 00 a0 0f 00 00 00 00 00 00              ............
	 */
	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadType; 						/*!< Media_PayloadType */
		uint32_t lel_vadValue;
		uint32_t lel_g723BitRate; 						/*!< only used with G.723 payload */
		/* protocol version 5 fields */
		uint32_t lel_conferenceId1;
		uint32_t unknown1;
		uint32_t unknown2;
		uint32_t unknown3;
		uint32_t unknown4;
		uint32_t unknown5;
		uint32_t unknown6;
		uint32_t unknown7;
		uint32_t unknown8;
		uint32_t unknown9;
		uint32_t unknown10;
		uint32_t unknown11;
		uint32_t unknown12;
		uint32_t unknown13;
		uint32_t unknown14;
		uint32_t lel_rtpDTMFPayload; /* this is often set to 0x65 (101) */
		uint32_t lel_rtptimeout; /* always 0x0A - could be a timeout for RTP */
		uint32_t unknown17;
		uint32_t unknown18;
		uint32_t unknown19;
		char 	 bel_remoteIpAddr[16];
		uint32_t lel_unknown20; /* this is always 0xFA0 */
		uint32_t unknown21;
	} OpenReceiveChannel_v17;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		/* version 5 fields */
		uint32_t lel_conferenceId1;
	} CloseReceiveChannel;

	struct {			// Request Statistics from Phone
		char								DirectoryNumber[StationMaxDirnumSize];
		uint32_t							lel_callReference;
		uint32_t							lel_StatsProcessing;
	} ConnectionStatisticsReq;

	struct {
		uint32_t							lel_softKeyOffset;
		uint32_t							lel_softKeyCount;
		uint32_t							lel_totalSoftKeyCount;
		StationSoftKeyDefinition   			definition[StationMaxSoftKeyDefinition];
	} SoftKeyTemplateResMessage;

	struct {
		uint32_t							lel_softKeySetOffset;
		uint32_t							lel_softKeySetCount;
		uint32_t							lel_totalSoftKeySetCount;
		StationSoftKeySetDefinition 		definition[StationMaxSoftKeySetDefinition];
	} SoftKeySetResMessage;

	struct {
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_softKeySetIndex;
		uint32_t les_validKeyMask;
	} SelectSoftKeysMessage;

	struct {
		uint32_t	lel_callState;
		uint32_t	lel_lineInstance;
		uint32_t	lel_callReference;
		uint32_t	lel_visibility;
		uint32_t	lel_priority;
		uint32_t	lel_unknown3;
	} CallStateMessage;

	struct {
		uint32_t lel_messageTimeout;
		char	 promptMessage[32];
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} DisplayPromptStatusMessage;

	struct {
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} ClearPromptStatusMessage;

	struct {
		uint32_t lel_displayTimeout;
		char	 displayMessage[StationMaxDisplayNotifySize];
	} DisplayNotifyMessage;

	struct {} ClearNotifyMessage;


    /* 0x11F FeatureStatMessage */
	struct {
		uint32_t lel_featureInstance;
		uint32_t lel_featureID;
		char featureTextLabel[StationMaxNameSize];
		uint32_t lel_featureStatus;
	} FeatureStatMessage;

	struct {
		uint32_t lel_serviceURLIndex;
		char	 URL[StationMaxServiceURLSize];
		char	 label[StationMaxNameSize];
	} ServiceURLStatMessage;

  struct {
    uint32_t lel_status;
		uint32_t lel_callReference;
    uint32_t lel_lineInstance;
	} CallSelectStatMessage;

	struct {
		uint32_t lel_displayTimeout;
		uint32_t lel_priority;
		char	 displayMessage[StationMaxDisplayNotifySize];
	} DisplayPriNotifyMessage;

	struct {} ClearPriNotifyMessage;

	struct {
		uint32_t lel_lineInstance;
	} ActivateCallPlaneMessage;

	struct {} DeactivateCallPlaneMessage;

	struct {
		uint32_t lel_status;
	} UnregisterAckMessage;

	struct {
		uint32_t 	lel_lineInstance;
		uint32_t 	lel_callReference;
	} BackSpaceReqMessage;

	struct {} RegisterTokenAck;
	struct {} RegisterTokenReject;

	struct{
		uint32_t	lel_conferenceID;
		uint32_t	lel_numberOfReservedParticipants;
		uint32_t	lel_resourceTypes;
		uint32_t	lel_appID;
		uint32_t	lel_appConfID;
		uint32_t	lel_appData;
		uint32_t	lel_data_length;
		uint32_t	lel__passThruData;
	}CreateConferenceReqMessage;

	struct{
		uint32_t	lel_conferenceID;		
	}DeleteConferenceReqMessage;

} sccp_data_t;

typedef struct {
	uint32_t		length;
	uint32_t		lel_reserved;
	uint32_t		lel_messageId;
	sccp_data_t		msg;
} sccp_moo_t;

/* So in theory, a message should never be bigger than this.
 * If it is, we abort the connection */
#define SCCP_MAX_PACKET sizeof(sccp_moo_t)

#include "sccp_softkeys.h"
#include "sccp_labels.h"

static const uint8_t softkeysmap[] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_HOLD,
	SKINNY_LBL_TRANSFER,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
	SKINNY_LBL_CFWDNOANSWER,
	SKINNY_LBL_BACKSPACE,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_RESUME,
	SKINNY_LBL_ANSWER,
	SKINNY_LBL_INFO,
	SKINNY_LBL_CONFRN,
	SKINNY_LBL_PARK,
	SKINNY_LBL_JOIN,
	SKINNY_LBL_MEETME,
	SKINNY_LBL_PICKUP,
	SKINNY_LBL_GPICKUP,
	SKINNY_LBL_RMLSTC,
	SKINNY_LBL_CALLBACK,
	SKINNY_LBL_BARGE,
	SKINNY_LBL_DND,
	SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE, /* Should be: Acct */
	SKINNY_LBL_CALLPARK_REVERSION,       /* Should be: Flash */
	SKINNY_LBL_SERVICE_IS_NOT_ACTIVE,    /* Should be: Login */
	SKINNY_LBL_EMPTY,                    /* Should be: HLog */
	SKINNY_LBL_CONFLIST,
	SKINNY_LBL_SELECT,
	SKINNY_LBL_PRIVATE,
	SKINNY_LBL_TRNSFVM,
	SKINNY_LBL_DIRTRFR,
	SKINNY_LBL_IDIVERT,
//	SKINNY_LBL_CBARGE,
};

typedef struct {
  const uint8_t id;
  const uint8_t * ptr;
  const uint8_t count;
} softkey_modes;

#define KEYMODE_ONHOOK				0
#define KEYMODE_CONNECTED			1
#define KEYMODE_ONHOLD				2
#define KEYMODE_RINGIN				3
#define KEYMODE_OFFHOOK 			4
#define KEYMODE_CONNTRANS			5
#define KEYMODE_DIGITSFOLL			6
#define KEYMODE_CONNCONF			7
#define KEYMODE_RINGOUT 			8
#define KEYMODE_OFFHOOKFEAT 		9
#define KEYMODE_INUSEHINT			10

static const uint8_t skSet_Onhook [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_CFWDALL,
//	SKINNY_LBL_CFWDBUSY,
//	SKINNY_LBL_CFWDNOANSWER,
#ifdef CS_SCCP_PICKUP
	SKINNY_LBL_PICKUP,
	SKINNY_LBL_GPICKUP,
#endif
//	SKINNY_LBL_CONFLIST,
	SKINNY_LBL_DND,
};

static const uint8_t skSet_Connected [] = {
	SKINNY_LBL_HOLD,
	SKINNY_LBL_ENDCALL,
#ifdef CS_SCCP_PARK
	SKINNY_LBL_PARK,
#endif
#ifdef CS_SCCP_DIRTRFR
	SKINNY_LBL_SELECT,
#endif
  	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
//	SKINNY_LBL_CFWDNOANSWER,
	SKINNY_LBL_IDIVERT,
};

static const uint8_t skSet_Onhold [] = {
	SKINNY_LBL_RESUME,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
#ifdef CS_SCCP_CONFERENCE
	SKINNY_LBL_CONFLIST,
	//SKINNY_LBL_CONFRN,
#endif
#ifdef CS_SCCP_DIRTRFR
	SKINNY_LBL_SELECT,
	SKINNY_LBL_DIRTRFR,
#endif
	SKINNY_LBL_IDIVERT,
};

static const uint8_t skSet_Ringin [] = {
	SKINNY_LBL_ANSWER,
	SKINNY_LBL_ENDCALL,
//	SKINNY_LBL_TRNSFVM,
	SKINNY_LBL_IDIVERT,
//	SKINNY_LBL_TRANSFER,
//	SKINNY_LBL_DIRTRFR
};

static const uint8_t skSet_Offhook [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_PRIVATE,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
//	SKINNY_LBL_CFWDNOANSWER,
#ifdef CS_SCCP_PICKUP
	SKINNY_LBL_PICKUP,
	SKINNY_LBL_GPICKUP,
#endif
	SKINNY_LBL_MEETME,
	SKINNY_LBL_BARGE,
//	SKINNY_LBL_CBARGE,
};

static const uint8_t skSet_Conntrans [] = {
	SKINNY_LBL_HOLD,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
#ifdef CS_SCCP_CONFERENCE
	SKINNY_LBL_CONFRN,
#endif
#ifdef CS_SCCP_PARK
	SKINNY_LBL_PARK,
#endif
#ifdef CS_SCCP_DIRTRFR
	SKINNY_LBL_SELECT,
	SKINNY_LBL_DIRTRFR,
#endif
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
//	SKINNY_LBL_CFWDNOANSWER,
};

static const uint8_t skSet_DigitsFoll []  = {
	SKINNY_LBL_BACKSPACE,
	SKINNY_LBL_ENDCALL,
};

static const uint8_t skSet_Connconf []	= {
	SKINNY_LBL_HOLD,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_JOIN,
};

static const uint8_t skSet_RingOut [] = {
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_IDIVERT,
//	SKINNY_LBL_CFWDBUSY,
//	SKINNY_LBL_CFWDNOANSWER,
};

static const uint8_t skSet_Offhookfeat [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_ENDCALL,
};

// in use hint keyset
static const uint8_t skSet_InUseHint [] = {
//	SKINNY_LBL_NEWCALL,
//	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_PICKUP,
	SKINNY_LBL_BARGE,
};


static const softkey_modes SoftKeyModes [] = {
/* According to CCM dump:
 OnHook(0), Connected(1), OnHold(2), RingIn(3)
 OffHook(4), ConnectedWithTransfer(5)
 Digitsafterdialingfirstdigit(6), Connected with Conference (7)
 RingOut (8), OffHookWithFeatures (9), InUseHint(10)
 */
  { KEYMODE_ONHOOK, 		skSet_Onhook,		sizeof(skSet_Onhook)/sizeof(uint8_t)},
  { KEYMODE_CONNECTED,		skSet_Connected,	sizeof(skSet_Connected)/sizeof(uint8_t)},
  { KEYMODE_ONHOLD, 		skSet_Onhold,		sizeof(skSet_Onhold)/sizeof(uint8_t)},
  { KEYMODE_RINGIN, 		skSet_Ringin,		sizeof(skSet_Ringin)/sizeof(uint8_t)},
  { KEYMODE_OFFHOOK,		skSet_Offhook,		sizeof(skSet_Offhook)/sizeof(uint8_t)},
  { KEYMODE_CONNTRANS,		skSet_Conntrans,	sizeof(skSet_Conntrans)/sizeof(uint8_t)},
  { KEYMODE_DIGITSFOLL, 	skSet_DigitsFoll,	sizeof(skSet_DigitsFoll)/sizeof(uint8_t)},
  { KEYMODE_CONNCONF,		skSet_Connconf, 	sizeof(skSet_Connconf)/sizeof(uint8_t)},
  { KEYMODE_RINGOUT,		skSet_RingOut,		sizeof(skSet_RingOut)/sizeof(uint8_t)},
  { KEYMODE_OFFHOOKFEAT,	skSet_Offhookfeat,	sizeof(skSet_Offhookfeat)/sizeof(uint8_t)},
  { KEYMODE_INUSEHINT,		skSet_InUseHint,	sizeof(skSet_InUseHint)/sizeof(uint8_t)},
};

#endif /* __SCCP_PROTOCOL_H */

