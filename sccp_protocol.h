#ifndef __SCCP_PROTOCOL_H
#define __SCCP_PROTOCOL_H

#define DEFAULT_SCCP_PORT					2000 	/*< SCCP uses port 2000. */
#define DEFAULT_SCCP_BACKLOG					2 	/*< the listen baklog. */
#define SCCP_MAX_AUTOLOGIN					100 	/*< Maximum allowed of autologins per device */
#define SCCP_KEEPALIVE						5 	/*< Default keepalive time if not specified in sccp.conf. */

/* internal chan_sccp call state c->callstate */
#define  SCCP_CHANNELSTATE_DOWN 				0
#define  SCCP_CHANNELSTATE_OFFHOOK				1
#define  SCCP_CHANNELSTATE_ONHOOK				2
#define  SCCP_CHANNELSTATE_RINGOUT				3
#define  SCCP_CHANNELSTATE_RINGIN				4
#define  SCCP_CHANNELSTATE_CONNECTED				5
#define  SCCP_CHANNELSTATE_BUSY 				6
#define  SCCP_CHANNELSTATE_CONGESTION				7
#define  SCCP_CHANNELSTATE_HOLD 				8
#define  SCCP_CHANNELSTATE_CALLWAITING				9
#define  SCCP_CHANNELSTATE_CALLTRANSFER 			10
#define  SCCP_CHANNELSTATE_CALLPARK 				11
#define  SCCP_CHANNELSTATE_PROCEED				12
#define  SCCP_CHANNELSTATE_CALLREMOTEMULTILINE			13
#define  SCCP_CHANNELSTATE_INVALIDNUMBER			14
#define  SCCP_CHANNELSTATE_DIALING				20

#define SCCP_DEVICESTATE_ONHOOK 				0
#define SCCP_DEVICESTATE_OFFHOOK				1
#define SCCP_DEVICESTATE_UNAVAILABLE				2
#define SCCP_DEVICESTATE_DND					3
#define SCCP_DEVICESTATE_FWDALL 				4

#define SCCP_CFWD_NONE						0
#define SCCP_CFWD_ALL						1
#define SCCP_CFWD_BUSY						2

/* skinny protocol call states */
#define SKINNY_CALLSTATE_OFFHOOK			 	1
#define SKINNY_CALLSTATE_ONHOOK 			 	2
#define SKINNY_CALLSTATE_RINGOUT			 	3
#define SKINNY_CALLSTATE_RINGIN 			 	4
#define SKINNY_CALLSTATE_CONNECTED			 	5
#define SKINNY_CALLSTATE_BUSY				 	6
#define SKINNY_CALLSTATE_CONGESTION 		 		7
#define SKINNY_CALLSTATE_HOLD				 	8
#define SKINNY_CALLSTATE_CALLWAITING		 		9
#define SKINNY_CALLSTATE_CALLTRANSFER		 		10
#define SKINNY_CALLSTATE_CALLPARK			 	11
#define SKINNY_CALLSTATE_PROCEED			 	12
#define SKINNY_CALLSTATE_CALLREMOTEMULTILINE 			13
#define SKINNY_CALLSTATE_INVALIDNUMBER		 		14

#define SKINNY_CALLTYPE_INBOUND 			 	1
#define SKINNY_CALLTYPE_OUTBOUND			 	2
#define SKINNY_CALLTYPE_FORWARD 			 	3

#define SKINNY_CALLPRIORITY_HIGHEST			 	0
#define SKINNY_CALLPRIORITY_HIGH			 	1
#define SKINNY_CALLPRIORITY_MEDIUM			 	2
#define SKINNY_CALLPRIORITY_LOW				 	3
#define SKINNY_CALLPRIORITY_NORMAL			 	4

#define SKINNY_CALLSECURITYSTATE_UNKNOWN			0
#define SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED		1
#define SKINNY_CALLSECURITYSTATE_AUTHENTICATED	 		2

#define StationMaxDeviceNameSize				16
#define StationMaxButtonTemplateSize				42
#define StationMaxButtonTemplateNameSize			44
#define StationDateTemplateSize 				6
#define StationMaxDisplayTextSize				33
#define StationMaxDisplayNotifySize 				32
#define StationMaxDirnumSize					24
#define StationMaxNameSize					40
#define StationMaxSoftKeyDefinition 				32
#define StationMaxSoftKeySetDefinition				16
#define StationMaxSoftKeyIndex					16
#define StationMaxSoftKeyLabelSize				16
#define StationMaxVersionSize				16
#define ServerMaxNameSize					48
#define StationMaxServiceURLSize				256

/* skinny tones skinny_tone2str */
#define SKINNY_TONE_SILENCE 					0
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
#define SKINNY_TONE_DTMFSTAR					0xe
#define SKINNY_TONE_DTMFPOUND					0xf
#define SKINNY_TONE_DTMFA					0x10
#define SKINNY_TONE_DTMFB					0x11
#define SKINNY_TONE_DTMFC					0x12
#define SKINNY_TONE_DTMFD					0x13
#define SKINNY_TONE_INSIDEDIALTONE				0x21
#define SKINNY_TONE_OUTSIDEDIALTONE 				0x22
#define SKINNY_TONE_LINEBUSYTONE				0x23
#define SKINNY_TONE_ALERTINGTONE				0x24
#define SKINNY_TONE_REORDERTONE 				0x25
#define SKINNY_TONE_RECORDERWARNINGTONE 			0x26
#define SKINNY_TONE_RECORDERDETECTEDTONE			0x27
#define SKINNY_TONE_REVERTINGTONE				0x28
#define SKINNY_TONE_RECEIVEROFFHOOKTONE 			0x29
#define SKINNY_TONE_PARTIALDIALTONE 				0x2a
#define SKINNY_TONE_NOSUCHNUMBERTONE				0x2b
#define SKINNY_TONE_BUSYVERIFICATIONTONE			0x2c
#define SKINNY_TONE_CALLWAITINGTONE 				0x2d
#define SKINNY_TONE_CONFIRMATIONTONE				0x2e
#define SKINNY_TONE_CAMPONINDICATIONTONE			0x2f
#define SKINNY_TONE_RECALLDIALTONE				0x30
#define SKINNY_TONE_ZIPZIP					0x31
#define SKINNY_TONE_ZIP 					0x32
#define SKINNY_TONE_BEEPBONK					0x33
#define SKINNY_TONE_MUSICTONE					0x34
#define SKINNY_TONE_HOLDTONE					0x35
#define SKINNY_TONE_TESTTONE					0x36
#define SKINNY_TONE_ADDCALLWAITING				0x40
#define SKINNY_TONE_PRIORITYCALLWAIT				0x41
#define SKINNY_TONE_RECALLDIAL					0x42
#define SKINNY_TONE_BARGIN					0x43
#define SKINNY_TONE_DISTINCTALERT				0x44
#define SKINNY_TONE_PRIORITYALERT				0x45
#define SKINNY_TONE_REMINDERRING				0x46
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
#define SKINNY_TONE_MILLIWATT					0x5f
#define SKINNY_TONE_MILLIWATTTEST				0x60
#define SKINNY_TONE_HIGHTONE					0x61
#define SKINNY_TONE_FLASHOVERRIDE				0x62
#define SKINNY_TONE_FLASH					0x63
#define SKINNY_TONE_PRIORITY					0x64
#define SKINNY_TONE_IMMEDIATE					0x65
#define SKINNY_TONE_PREAMPWARN					0x66
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
#define SKINNY_ALARM_MAJOR					7
#define SKINNY_ALARM_MINOR					8
#define SKINNY_ALARM_MARGINAL					10
#define SKINNY_ALARM_TRACEINFO					20

/* devices type */
#define SKINNY_DEVICETYPE_30SPPLUS				1
#define SKINNY_DEVICETYPE_12SPPLUS				2
#define SKINNY_DEVICETYPE_12SP					3
#define SKINNY_DEVICETYPE_12					4
#define SKINNY_DEVICETYPE_30VIP 				5
#define SKINNY_DEVICETYPE_CISCO7910				6 	/*< 7910 */
#define SKINNY_DEVICETYPE_CISCO7960				7 	/*< 7960 */
#define SKINNY_DEVICETYPE_CISCO7940				8 	/*< 7940 */
#define SKINNY_DEVICETYPE_CISCO7935 				9
#define SKINNY_DEVICETYPE_ATA186				12
#define SKINNY_DEVICETYPE_VIRTUAL30SPPLUS			20
#define SKINNY_DEVICETYPE_PHONEAPPLICATION			21
#define SKINNY_DEVICETYPE_ANALOGACCESS				30
#define SKINNY_DEVICETYPE_DIGITALACCESSPRI			40
#define SKINNY_DEVICETYPE_DIGITALACCESST1			41
#define SKINNY_DEVICETYPE_DIGITALACCESSTITAN2			42
#define SKINNY_DEVICETYPE_ANALOGACCESSELVIS 			47
#define SKINNY_DEVICETYPE_DIGITALACCESSLENNON			49
#define SKINNY_DEVICETYPE_CONFERENCEBRIDGE			50
#define SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO			51
#define SKINNY_DEVICETYPE_H225					60
#define SKINNY_DEVICETYPE_H323PHONE 				61
#define SKINNY_DEVICETYPE_H323TRUNK 				62
#define SKINNY_DEVICETYPE_MUSICONHOLD				70
#define SKINNY_DEVICETYPE_PILOT 				71
#define SKINNY_DEVICETYPE_TAPIPORT				72
#define SKINNY_DEVICETYPE_TAPIROUTEPOINT			73
#define SKINNY_DEVICETYPE_VOICEINBOX				80
#define SKINNY_DEVICETYPE_VOICEINBOXADMIN			81
#define SKINNY_DEVICETYPE_LINEANNUNCIATOR			82
#define SKINNY_DEVICETYPE_ROUTELIST 				90
#define SKINNY_DEVICETYPE_LOADSIMULATOR 			100
#define SKINNY_DEVICETYPE_MEDIATERMINATIONPOINT 		110
#define SKINNY_DEVICETYPE_MEDIATERMINATIONPOINTYOKO 		111
#define SKINNY_DEVICETYPE_MGCPSTATION				120
#define SKINNY_DEVICETYPE_MGCPTRUNK 				121
#define SKINNY_DEVICETYPE_RASPROXY				122
#define SKINNY_DEVICETYPE_NOTDEFINED				255
#define SKINNY_DEVICETYPE_CISCO7920 				30002
#define SKINNY_DEVICETYPE_CISCO7970 				30006
#define SKINNY_DEVICETYPE_CISCO7912				30007
#define SKINNY_DEVICETYPE_CISCO7902 				30008
#define SKINNY_DEVICETYPE_CISCO7905 				20000
#define SKINNY_DEVICETYPE_CISCO7936 				30019
#define SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR 		30016
#define SKINNY_DEVICETYPE_CISCO7961 				30018
#define SKINNY_DEVICETYPE_CISCO7971 				119
#define SKINNY_DEVICETYPE_CISCO7941 				115
#define SKINNY_DEVICETYPE_CISCO7961GE 				308
#define SKINNY_DEVICETYPE_CISCO7941GE 				309
#define SKINNY_DEVICETYPE_CISCO7911 				307
#define SKINNY_DEVICETYPE_CISCO7985 				302


#define SKINNY_DEVICE_RS_NONE					0
#define SKINNY_DEVICE_RS_PROGRESS				1
#define SKINNY_DEVICE_RS_FAILED 				2
#define SKINNY_DEVICE_RS_OK 					3
#define SKINNY_DEVICE_RS_TIMEOUT				4

/* stimulus */
#define SKINNY_STIMULUS_LASTNUMBERREDIAL			1
#define SKINNY_STIMULUS_SPEEDDIAL				2
#define SKINNY_STIMULUS_HOLD					3
#define SKINNY_STIMULUS_TRANSFER				4
#define SKINNY_STIMULUS_FORWARDALL				5
#define SKINNY_STIMULUS_FORWARDBUSY 				6
#define SKINNY_STIMULUS_FORWARDNOANSWER 			7
#define SKINNY_STIMULUS_DISPLAY 				8
#define SKINNY_STIMULUS_LINE					9
#define SKINNY_STIMULUS_T120CHAT				0xA
#define SKINNY_STIMULUS_T120WHITEBOARD				0xB
#define SKINNY_STIMULUS_T120APPLICATIONSHARING			0xC
#define SKINNY_STIMULUS_T120FILETRANSFER			0xD
#define SKINNY_STIMULUS_VIDEO					0xE
#define SKINNY_STIMULUS_VOICEMAIL				0xF
#define SKINNY_STIMULUS_AUTOANSWER				0x11
#define SKINNY_STIMULUS_GENERICAPPB1				0x21
#define SKINNY_STIMULUS_GENERICAPPB2				0x22
#define SKINNY_STIMULUS_GENERICAPPB3				0x23
#define SKINNY_STIMULUS_GENERICAPPB4				0x24
#define SKINNY_STIMULUS_GENERICAPPB5				0x25
#define SKINNY_STIMULUS_MEETMECONFERENCE			0x7b
#define SKINNY_STIMULUS_CONFERENCE				0x7d
#define SKINNY_STIMULUS_CALLPARK				0x7e
#define SKINNY_STIMULUS_CALLPICKUP				0x7f
#define SKINNY_STIMULUS_GROUPCALLPICKUP 			0x80

#define SKINNY_BUTTONTYPE_UNUSED				0
#define SKINNY_BUTTONTYPE_LASTNUMBERREDIAL			1
#define SKINNY_BUTTONTYPE_SPEEDDIAL 				2
#define SKINNY_BUTTONTYPE_HOLD					3
#define SKINNY_BUTTONTYPE_TRANSFER				4
#define SKINNY_BUTTONTYPE_FORWARDALL				5
#define SKINNY_BUTTONTYPE_FORWARDBUSY				6
#define SKINNY_BUTTONTYPE_FORWARDNOANSWER			7
#define SKINNY_BUTTONTYPE_DISPLAY				8
#define SKINNY_BUTTONTYPE_LINE					9
#define SKINNY_BUTTONTYPE_T120CHAT				0xa
#define SKINNY_BUTTONTYPE_T120WHITEBOARD			0xb
#define SKINNY_BUTTONTYPE_T120APPLICATIONSHARING		0xc
#define SKINNY_BUTTONTYPE_T120FILETRANSFER			0xd
#define SKINNY_BUTTONTYPE_VIDEO 				0xe
#define SKINNY_BUTTONTYPE_VOICEMAIL 				0xf
#define SKINNY_BUTTONTYPE_ANSWERRELEASE 			0x10
#define SKINNY_BUTTONTYPE_AUTOANSWER				0x11
#define SKINNY_BUTTONTYPE_SERVICEURL				0x14
#define SKINNY_BUTTONTYPE_GENERICAPPB1				0x21
#define SKINNY_BUTTONTYPE_GENERICAPPB2				0x22
#define SKINNY_BUTTONTYPE_GENERICAPPB3				0x23
#define SKINNY_BUTTONTYPE_GENERICAPPB4				0x24
#define SKINNY_BUTTONTYPE_GENERICAPPB5				0x25
#define SKINNY_BUTTONTYPE_MEETMECONFERENCE			0x7B
#define SKINNY_BUTTONTYPE_CONFERENCE				0x7D
#define SKINNY_BUTTONTYPE_CALLPARK				0x7E
#define SKINNY_BUTTONTYPE_CALLPICKUP				0x7F
#define SKINNY_BUTTONTYPE_GROUPCALLPICKUP			0x80
#define SKINNY_BUTTONTYPE_KEYPAD				0xf0
#define SKINNY_BUTTONTYPE_AEC					0xfd
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
#define SKINNY_LAMP_OFF 					1		/*< Lamp Off, 0% Duty */
#define SKINNY_LAMP_ON						2		/*< Lamp On, 100% Duty */
#define SKINNY_LAMP_WINK					3		/*< Lamp slow blink, ~90% Duty */
#define SKINNY_LAMP_FLASH					4		/*< Lamp very fast blink, ~70% Duty */
#define SKINNY_LAMP_BLINK					5		/*< Lamp slow blink, ~50% Duty */

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
  UserToDeviceDataVersion1Message				= 0x013F	

} sccp_message_t;

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

typedef union {
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
		uint32_t lel_featureIndex;
	} FeatureStatReqMessage;
	
	struct {
		uint32_t lel_serviceURLIndex;
	} ServiceURLStatReqMessage;

	struct {} MulticastMediaReceptionAck;

	struct {
		uint32_t lel_orcStatus; 			/*!< OpenReceiveChanStatus */
		uint32_t bel_ipAddr; 				/*!< This field is apparently in big-endian format, 
												even though most other fields are in
												little-endian format. */
		uint32_t lel_portNumber;
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference;
	} OpenReceiveChannelAck;

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


	struct {} OffHookWithCgpnMessage;
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
		uint16_t les__filler1;
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
	} StartMediaTransmission;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_conferenceId1;
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
	struct {} StartMulticastMediaReception;
	struct {} StartMulticastMediaTransmission;
	struct {} StopMulticastMediaReception;
	struct {} StopMulticastMediaTransmission;

	struct {
		uint32_t lel_conferenceId;
		uint32_t lel_passThruPartyId;
		uint32_t lel_millisecondPacketSize;
		uint32_t lel_payloadType; 						/*!< Media_PayloadType */
		uint32_t lel_vadValue;
		uint32_t lel_g723BitRate; 						/*!< only used with G.723 payload */
		/* protocol version 5 fields */
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
	} OpenReceiveChannel;

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
		uint32_t	lel_unknown1;
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
		uint32_t lel_featureIndex;
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

	struct {} BackSpaceReqMessage;
	struct {} RegisterTokenAck;
	struct {} RegisterTokenReject;

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
  SKINNY_LBL_DIRTRFR
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
#define KEYMODE_MYST				10

static const uint8_t skSet_Onhook [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_DND,
	SKINNY_LBL_GPICKUP
//	SKINNY_LBL_CFWDALL
};

static const uint8_t skSet_Connected [] = {
	SKINNY_LBL_HOLD,
	SKINNY_LBL_ENDCALL,
#ifdef CS_SCCP_PARK
	SKINNY_LBL_PARK,
#endif
  SKINNY_LBL_SELECT,
  SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY
};

static const uint8_t skSet_Onhold [] = {
	SKINNY_LBL_RESUME,
	SKINNY_LBL_NEWCALL,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
  SKINNY_LBL_SELECT,
	SKINNY_LBL_DIRTRFR
};

static const uint8_t skSet_Ringin [] = {
	SKINNY_LBL_ANSWER,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRNSFVM
//	SKINNY_LBL_TRANSFER,
//	SKINNY_LBL_DIRTRFR
};

static const uint8_t skSet_Offhook [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_PRIVATE,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY,
	SKINNY_LBL_GPICKUP,
	SKINNY_LBL_DND
};

static const uint8_t skSet_Conntrans [] = {
	SKINNY_LBL_HOLD,
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
#ifdef CS_SCCP_PARK
	SKINNY_LBL_PARK,
#endif
  SKINNY_LBL_SELECT,
	SKINNY_LBL_DIRTRFR,
  SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY
};

static const uint8_t skSet_DigitsFoll []  = {
	SKINNY_LBL_BACKSPACE,
	SKINNY_LBL_ENDCALL,
};

static const uint8_t skSet_Connconf []	= {
	SKINNY_LBL_EMPTY
};

static const uint8_t skSet_RingOut [] = {
	SKINNY_LBL_ENDCALL,
	SKINNY_LBL_TRANSFER,
	SKINNY_LBL_CFWDALL,
	SKINNY_LBL_CFWDBUSY
};

static const uint8_t skSet_Offhookfeat [] = {
	SKINNY_LBL_REDIAL,
	SKINNY_LBL_ENDCALL
};

static const uint8_t skSet_Myst [] = {
	SKINNY_LBL_NEWCALL
};


static const softkey_modes SoftKeyModes [] = {
/* According to CCM dump:
 OnHook(0), Connected(1), OnHold(2), RingIn(3)
 OffHook(4), ConnectedWithTransfer(5)
 Digitsafterdialingfirstdigit(6), Connected with Conference (7)
 RingOut (8), OffHookWithFeatures (9), Unknown (10)
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
  { KEYMODE_MYST,			skSet_Myst, 		sizeof(skSet_Myst)/sizeof(uint8_t)},
};

#endif
