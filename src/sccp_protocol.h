/*!
 * \file        sccp_protocol.h
 * \brief       SCCP Protocol Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * \note        Thank you Federico Santulli for SPCP support
 */
#pragma once
#include "define.h"
#include "sccp_codec.h"
#include "sccp_labels.h"
#include "sccp_softkeys.h"
#include "sccp_enum.h"

__BEGIN_C_EXTERN__
#define SCCP_DRIVER_SUPPORTED_PROTOCOL_LOW		3							/*!< At least we require protocol V.3 */
#define SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH		22							/*!< We support up to protocol V.17 */

#define UNKNOWN_PROTOCOL				0
#define SCCP_PROTOCOL					1
#define SPCP_PROTOCOL					2

#define DEFAULT_SCCP_PORT				2000							/*!< SCCP uses port 2000. */
#define DEFAULT_SCCP_SECURE_PORT			2443							/*!< SCCP secure port 2443. */
#define DEFAULT_SCCP_BACKLOG				16							/*!< the listen baklog. */
#define SCCP_MAX_AUTOLOGIN				100							/*!< Maximum allowed of autologins per device */
#define SCCP_KEEPALIVE					5							/*!< Default keepalive time if not specified in sccp.conf. */


#define SKINNY_PHONE_FEATURES_PROTOCOLVERSION 		0xFF
#define SKINNY_PHONE_FEATURES_ABBRDIAL 			1<<31
#define SKINNY_PHONE_FEATURES_DYNAMIC_MESSAGES 		1<<24
#define SKINNY_PHONE_FEATURES_UNKNOWN1 			1<<21
#define SKINNY_PHONE_FEATURES_UTF8 			1<<20
#define SKINNY_PHONE_FEATURES_UNKNOWN2	 		1<<22
#define SKINNY_PHONE_FEATURES_RFC2833			1<<26

/*
#define SKINNY_PHONE_FEATURES_INTERNALIZATION		1<<31
#define SKINNY_PHONE_FEATURES_STATION_MAX_FEATURE_MASK	1<<30
#define SKINNY_PHONE_FEATURES_MEDIA_ENCRYPTION_SUPPORT	1<<29
#define SKINNY_PHONE_FEATURES_INTERNAL_CM_MEDIA		1<<28
#define SKINNY_PHONE_FEATURES_AUTHENTICATED_SIGNALING	1<<27
#define SKINNY_PHONE_FEATURES_RESTRICT_PI		1<<26
#define SKINNY_PHONE_FEATURES_AUTO_IDLE_DISABLE		1<<25
#define SKINNY_PHONE_FEATURES_UNICODE_CAPABLE		1<<24
#define SKINNY_PHONE_FEATURES_RSVP_SUPPORT		1<<23
#define SKINNY_PHONE_FEATURES_KFACTOR_CAPABILITY	1<<22
#define SKINNY_PHONE_FEATURES_DICTIONARY_NEW_RANGE	1<<21
#define SKINNY_PHONE_FEATURES_SNF_NETWORK_CAPABILITY	1<<20
#define SKINNY_PHONE_FEATURES_MULTIMEDIA_SUPPORT	1<<19
*/

#define SKINNY_MaxAnnouncementList			32
#define SKINNY_StationMaxMonitorParties			16

#define StationMaxDeviceNameSize			16
#define StationMaxButtonTemplateSize			56
#define StationMaxButtonTemplateNameSize		44
#define StationDateTemplateSize 			7
#define StationMaxDisplayTextSize			32
#define StationMaxDisplayNotifySize 			32
#define StationMaxDirnumSize				24
#define StationDynamicDirnumSize			25
#define StationMaxNameSize				40
#define StationDynamicNameSize				121
#define StationMaxSoftKeyDefinition 			32
#define StationMaxSoftKeySetDefinition			16
#define StationMaxSoftKeyIndex				16
#define StationMaxSoftKeyLabelSize			16
#define StationMaxVersionSize				16
#define StationMaxServers				5
#define ServerMaxNameSize				48
#define StationMaxServiceURLSize			256
#define StationMaxPorts					16
#define StationMaxAlarmMessage				80
#define StationMaxXMLMessage				2000
#define StationMaxXMLAlarmMessage			2048
#define StationMaxImageVersionSize			32

#define APPID_CONFERENCE				9081
#define APPID_CONFERENCE_INVITE				9082
#define APPID_PROVISION					9083
#define APPID_DEVICECAPABILITIES			9084
#define APPID_URIHOOK					9085
#define APPID_BACKGROUND				9086
#define APPID_RINGTONE					9087
#define APPID_STREAM					9088
#define APPID_PUSH					9089
#define APPID_VISUALPARKINGLOT				9090

/*
 * \todo Merge sccp_button_type_t / sccp_buttontypes[] and this define
 */
#define SCCP_BUTTONTYPE_MULTI				0xF1
#define SCCP_BUTTONTYPE_LINE				0xF2
#define SCCP_BUTTONTYPE_SPEEDDIAL			0xF3
#define SCCP_BUTTONTYPE_HINT				0xF4
#define SCCP_BUTTONTYPE_ABBRDIAL			0xF5

/* unregister */
#define SKINNY_UNREGISTERSTATUS_OK			0
#define SKINNY_UNREGISTERSTATUS_ERROR			1
#define SKINNY_UNREGISTERSTATUS_NAK 			2							/*!< we have an active channel */

/* media silence suppression */
#define SKINNY_MEDIA_SILENCESUPPRESSION_OFF 		0
#define SKINNY_MEDIA_SILENCESUPPRESSION_ON		1

/* media echo cancel */
#define SKINNY_MEDIA_ECHOCANCELLATION_OFF		0
#define SKINNY_MEDIA_ECHOCANCELLATION_ON		1

/* G723 bitrate */
#define SKINNY_MEDIA_G723BRATE_NONE 			0
#define SKINNY_MEDIA_G723BRATE_5_3			1
#define SKINNY_MEDIA_G723BRATE_6_4			2

/* device reset and restart */
#define SKINNY_DEVICE_RESET 				1
#define SKINNY_DEVICE_RESTART				2
#define SKINNY_DEVICE_RELOAD_CONFIG			3

/* statistic processing */
#define SKINNY_STATSPROCESSING_CLEAR			0
#define SKINNY_STATSPROCESSING_DONOTCLEAR		1

/* speaker status */
#define SKINNY_STATIONSPEAKER_ON			1
#define SKINNY_STATIONSPEAKER_OFF			2

/* mic status */
#define SKINNY_STATIONMIC_ON				1
#define SKINNY_STATIONMIC_OFF				2

/* headset status */
#define SKINNY_STATIONHEADSET_ON			1
#define SKINNY_STATIONHEADSET_OFF			2

#define MAX_CUSTOM_PICTURES				6
#define MAX_LAYOUT_WITH_SAME_SERVICE			5
#define MAX_SERVICE_TYPE				4
#define MAX_LEVEL_PREFERENCE		 		4

/*!
 * \brief SCCP Device->Button List Structure
 */
typedef struct {
	uint8_t instance;											/*!< Button Instance */
	uint8_t type;												/*!< Button Type */
	void *ptr;												/*!< Pointer to the Line */
} btnlist;													/*!< Button List Structure */

/*!
 * \brief SKINNY Message Types Enum
 */

typedef enum {
	/* *INDENT-OFF* */

#define SCCP_MESSAGE_LOW_BOUNDARY			KeepAliveMessage		/*0x0000*/
	/* Client -> Server */
	KeepAliveMessage 				= 0x0000,
	RegisterMessage 				= 0x0001,
	IpPortMessage 					= 0x0002,
	KeypadButtonMessage 				= 0x0003,
	EnblocCallMessage 				= 0x0004,
	StimulusMessage 				= 0x0005,
	OffHookMessage 					= 0x0006,
	OnHookMessage 					= 0x0007,
	HookFlashMessage 				= 0x0008,
	ForwardStatReqMessage 				= 0x0009,
	SpeedDialStatReqMessage 			= 0x000A,
	LineStatReqMessage 				= 0x000B,
	ConfigStatReqMessage 				= 0x000C,
	TimeDateReqMessage 				= 0x000D,
	ButtonTemplateReqMessage 			= 0x000E,
	VersionReqMessage 				= 0x000F,
	CapabilitiesResMessage 				= 0x0010,
	MediaPortListMessage 				= 0x0011,
	ServerReqMessage 				= 0x0012,
	AlarmMessage 					= 0x0020,
	MulticastMediaReceptionAck 			= 0x0021,
	OpenReceiveChannelAck 				= 0x0022,
	ConnectionStatisticsRes 			= 0x0023,
	OffHookWithCgpnMessage 				= 0x0024,
	SoftKeySetReqMessage 				= 0x0025,
	SoftKeyEventMessage 				= 0x0026,
	UnregisterMessage 				= 0x0027,
	SoftKeyTemplateReqMessage 			= 0x0028,
	RegisterTokenRequest 				= 0x0029,
	MediaTransmissionFailure 			= 0x002A,
	HeadsetStatusMessage 				= 0x002B,
	MediaResourceNotification 			= 0x002C,
	RegisterAvailableLinesMessage 			= 0x002D,
	DeviceToUserDataMessage 			= 0x002E,
	DeviceToUserDataResponseMessage 		= 0x002F,
	UpdateCapabilitiesMessage			= 0x0030,
	OpenMultiMediaReceiveChannelAckMessage 		= 0x0031,
	ClearConferenceMessage 				= 0x0032,
	ServiceURLStatReqMessage 			= 0x0033,
	FeatureStatReqMessage 				= 0x0034,
	CreateConferenceResMessage 			= 0x0035,
	DeleteConferenceResMessage 			= 0x0036,
	ModifyConferenceResMessage 			= 0x0037,
	AddParticipantResMessage 			= 0x0038,
	AuditConferenceResMessage 			= 0x0039,
	AuditParticipantResMessage 			= 0x0040,
	DeviceToUserDataVersion1Message 		= 0x0041,
	DeviceToUserDataResponseVersion1Message 	= 0x0042,
	
	UpdateCapabilitiesV2Message 			= 0x0043,	/*new (2013-12-9)*/
	UpdateCapabilitiesV3Message			= 0x0044,	/* DynamicUpdateCapabilitiesMessage */ /*!< @see https://sourceforge.net/p/chan-sccp-b/bugs/181/ */
	
	PortResponseMessage				= 0x0045,	/*new (2013-12-9)*/
	QoSResvNotifyMessage 				= 0x0046,	/*new (2013-12-9)*/
	QoSErrorNotifyMessage 				= 0x0047,	/*new (2013-12-9)*/

	/* This are from protocol V 11 CCM7 */
	SubscriptionStatReqMessage			= 0x0048,	/* Subscription Stat Request */
	AccessoryStatusMessage 				= 0x0049,	/* MediaPath Event Message */
	MediaPathCapabilityMessage			= 0x004A,	/*new name (2013-12-9)*/
	MwiNotificationMessage 				= 0x004C,

	/* Server -> Client */
	RegisterAckMessage 				= 0x0081,
	StartToneMessage 				= 0x0082,
	StopToneMessage 				= 0x0083,
	// ??
	SetRingerMessage 				= 0x0085,
	SetLampMessage 					= 0x0086,
	SetHookFlashDetectMessage 			= 0x0087,
	SetSpeakerModeMessage 				= 0x0088,
	SetMicroModeMessage 				= 0x0089,
	StartMediaTransmission 				= 0x008A,
	StopMediaTransmission 				= 0x008B,
	StartMediaReception 				= 0x008C,
	StopMediaReception 				= 0x008D,
	// ?
	CallInfoMessage 				= 0x008F,

	ForwardStatMessage 				= 0x0090,
	SpeedDialStatMessage 				= 0x0091,
	LineStatMessage 				= 0x0092,
	ConfigStatMessage 				= 0x0093,
	DefineTimeDate 					= 0x0094,
	StartSessionTransmission 			= 0x0095,
	StopSessionTransmission 			= 0x0096,
	ButtonTemplateMessage 				= 0x0097,
	//ButtonTemplateMessageSingle 			= 0x0097,
	VersionMessage 					= 0x0098,
	DisplayTextMessage 				= 0x0099,
	ClearDisplay 					= 0x009A,
	CapabilitiesReqMessage 				= 0x009B,
	EnunciatorCommandMessage 			= 0x009C,
	RegisterRejectMessage 				= 0x009D,
	ServerResMessage 				= 0x009E,
	Reset 						= 0x009F,

	KeepAliveAckMessage 				= 0x0100,
	StartMulticastMediaReception 			= 0x0101,
	StartMulticastMediaTransmission 		= 0x0102,
	StopMulticastMediaReception 			= 0x0103,
	StopMulticastMediaTransmission 			= 0x0104,
	OpenReceiveChannel 				= 0x0105,
	CloseReceiveChannel 				= 0x0106,
	ConnectionStatisticsReq 			= 0x0107,
	SoftKeyTemplateResMessage 			= 0x0108,
	SoftKeySetResMessage 				= 0x0109,

	SelectSoftKeysMessage 				= 0x0110,
	CallStateMessage 				= 0x0111,
	DisplayPromptStatusMessage 			= 0x0112,
	ClearPromptStatusMessage 			= 0x0113,
	DisplayNotifyMessage 				= 0x0114,
	ClearNotifyMessage 				= 0x0115,
	ActivateCallPlaneMessage 			= 0x0116,
	DeactivateCallPlaneMessage 			= 0x0117,
	UnregisterAckMessage 				= 0x0118,
	BackSpaceResMessage 				= 0x0119,
	RegisterTokenAck 				= 0x011A,
	RegisterTokenReject 				= 0x011B,
	StartMediaFailureDetection 			= 0x011C,
	DialedNumberMessage 				= 0x011D,
	DialedNumberMessageV19 				= 0x011D,
	UserToDeviceDataMessage 			= 0x011E,
	FeatureStatMessage 				= 0x011F,
	DisplayPriNotifyMessage 			= 0x0120,
	ClearPriNotifyMessage 				= 0x0121,
	StartAnnouncementMessage 			= 0x0122,
	StopAnnouncementMessage 			= 0x0123,
	AnnouncementFinishMessage 			= 0x0124,
	
	NotifyDtmfToneMessage 				= 0x0127,
	SendDtmfToneMessage 				= 0x0128,
	SubscribeDtmfPayloadReqMessage 			= 0x0129,
	SubscribeDtmfPayloadResMessage 			= 0x012A,
	SubscribeDtmfPayloadErrMessage 			= 0x012B,
	UnSubscribeDtmfPayloadReqMessage 		= 0x012C,
	UnSubscribeDtmfPayloadResMessage 		= 0x012D,
	UnSubscribeDtmfPayloadErrMessage 		= 0x012E,
	ServiceURLStatMessage 				= 0x012F,
	CallSelectStatMessage 				= 0x0130,
	OpenMultiMediaChannelMessage 			= 0x0131,
	StartMultiMediaTransmission 			= 0x0132,
	StopMultiMediaTransmission 			= 0x0133,
	MiscellaneousCommandMessage 			= 0x0134,
	FlowControlCommandMessage 			= 0x0135,
	CloseMultiMediaReceiveChannel 			= 0x0136,
	CreateConferenceReqMessage 			= 0x0137,
	DeleteConferenceReqMessage 			= 0x0138,
	ModifyConferenceReqMessage 			= 0x0139,
	AddParticipantReqMessage 			= 0x013A,
	DropParticipantReqMessage 			= 0x013B,
	AuditConferenceReqMessage 			= 0x013C,
	AuditParticipantReqMessage 			= 0x013D,
	UserToDeviceDataVersion1Message 		= 0x013F,

	/* sent by us */
	VideoDisplayCommandMessage 			= 0x0140,
	FlowControlNotifyMessage			= 0x0141,	/*new name (2013-12-9)*/
	ConfigStatDynamicMessage			= 0x0142,	/*new (2013-12-9)*/
	DisplayDynamicNotifyMessage 			= 0x0143,
	DisplayDynamicPriNotifyMessage 			= 0x0144,
	DisplayDynamicPromptStatusMessage 		= 0x0145,
	FeatureStatDynamicMessage 			= 0x0146,
	LineStatDynamicMessage 				= 0x0147,
	ServiceURLStatDynamicMessage 			= 0x0148,
	SpeedDialStatDynamicMessage 			= 0x0149,
	CallInfoDynamicMessage 				= 0x014A,
	
	PortRequestMessage				= 0x014B,	/*new (2013-12-9)*/
	PortCloseMessage 				= 0x014C,	/*new (2013-12-9)*/
	QoSListenMessage 				= 0x014D,	/*new (2013-12-9)*/
	QoSPathMessage 					= 0x014E,	/*new (2013-12-9)*/
	QoSTeardownMessage 				= 0x014F,	/*new (2013-12-9)*/	
	UpdateDSCPMessage 				= 0x0150,	/*new (2013-12-9)*/
	QoSModifyMessage 				= 0x0151,	/*new (2013-12-9)*/

	/* received from phone */
	SubscriptionStatMessage 			= 0x0152,	/* Subscription Stat Message */
	NotificationMessage 				= 0x0153,	/* Notification Message  / CallListStateUpdate*/
	StartMediaTransmissionAck 			= 0x0154,
	StartMultiMediaTransmissionAck 			= 0x0155,
	CallHistoryDispositionMessage 			= 0x0156,
	LocationInfoMessage				= 0x0157,	/* new (2015-09-08), send by 7925/7926 using firmware version 1.4.7.3 */

	MwiResponseMessage 				= 0x0158,	/*new (2013-12-9)*/
	ExtensionDeviceCaps 				= 0x0159,
	XMLAlarmMessage 				= 0x015A,
	CallCountReqMessage				= 0x015E,	/*new name (2013-12-9)*/		/*!< @see https://sourceforge.net/p/chan-sccp-b/bugs/173/ */
	CallCountRespMessage				= 0x015F,	/*new (2013-12-9)*/
	RecordingStatusMessage 				= 0x0160,	/*new (2013-12-9)*/

#define SCCP_MESSAGE_HIGH_BOUNDARY			RecordingStatusMessage		/*0x0160*/
#define SPCP_MESSAGE_OFFSET 				SPCP_MESSAGE_LOW_BOUNDARY
#define SPCP_MESSAGE_LOW_BOUNDARY			SPCPRegisterTokenRequest	/*0x8000*/
	/* SPCP client -> server; */
	SPCPRegisterTokenRequest 			= 0x8000,
	/* SPCP server -> client */
	SPCPRegisterTokenAck 				= 0x8100,
	SPCPRegisterTokenReject 			= 0x8101,

	//UnknownVGMessage				= 0xFF00,	/* Unknown Message (VG224). Reported by Ahmet Zaim */
//#define SPCP_MESSAGE_HIGH_BOUNDARY			UnknownVGMessage		/*0xFF00*/
#define SPCP_MESSAGE_HIGH_BOUNDARY			SPCPRegisterTokenReject
/*
	SPCPPlatformInfoGetReq				= 0xFF02,
	SPCPPlatformInfoGetRsp				= 0xFF03,
	SPCPPlatformInfoGetRej				= 0xFF04,
*/
	/* *INDENT-ON* */
} sccp_mid_t;													/*!< SKINNY Message ID Enum */


/*=====================================================================================================*/

/*!
 * \brief SKINNY Station Identifier Structure
 */
typedef struct {
	char deviceName[StationMaxDeviceNameSize];								/*!< Device name */
	uint32_t lel_userid;											/*!< User ID */
	uint32_t lel_instance;											/*!< Instance */
} StationIdentifier;												/*!< SKINNY Station Identifier Structure */

/*!
 * \brief SKINNY Station Button Definition Structure
 */
typedef struct {
	uint8_t instanceNumber;											/*!< set to instance number or StationKeyPadButton value */
	uint8_t buttonDefinition;										/*!< set to one of the preceding Bt values */
} StationButtonDefinition;											/*!< SKINNY Station Button Definition Structure */

typedef struct {
	uint8_t key[16];
	uint8_t salt[16];
} EncryptionKey;

typedef struct {
	skinny_encryptionMethod_t algorithm;
	uint16_t keylen;
	uint16_t saltlen;
	EncryptionKey keyData;
	uint32_t isMKIPresent;
	uint32_t keyDerivationRate;
} EncryptionInfo;

/*!
 * \brief SKINNY Media Payload Capability Structure
 */
typedef struct {
	skinny_codec_t lel_payloadCapability;									/*!< Payload Capability (Codec Used) */
	uint32_t lel_maxFramesPerPacket;									/*!< Maximum Number of Frames per IP Packet / Number of milliseconds of audio per RTP Packet */
	union {
		uint8_t futureUse[8];
		uint8_t lel_g723BitRate;									/*!< g723 Bit Rate (1=5.3 Kbps, 2=6.4 Kbps) */
	} PAYLOADS;												/*!< SKINNY Media Payload Capability Payloads Union */
} MediaCapabilityStructure;											/*!< SKINNY Media Payload Capability Structure */

/*!
 * \brief SKINNY Station Soft Key Definition Structure
 */
typedef struct {
	char softKeyLabel[StationMaxSoftKeyLabelSize];								/*!< Soft Key Label */
	uint32_t lel_softKeyEvent;										/*!< Soft Key Event */
} StationSoftKeyDefinition;											/*!< SKINNY Station Soft Key Definition Structure */

/*!
 * \brief SKINNY Station Soft Key Set Definition Structure
 */
typedef struct {
	uint8_t softKeyTemplateIndex[StationMaxSoftKeyIndex];							/*!< Soft Key Template Index */
	uint16_t les_softKeyInfoIndex[StationMaxSoftKeyIndex];							/*!< Soft Key Info Index */
} StationSoftKeySetDefinition;											/*!< SKINNY Station Soft Key Set Definition Structure */

/*!
 * \brief SKINNY Server Identifier Structure
 */
typedef struct {
	char serverName[ServerMaxNameSize];									/*!< Server Name */
} ServerIdentifier;												/*!< SKINNY Server Identifier Structure */

/*! 
 * \brief SKINNY Layout Config Structure
 * \note Part of Update Capabilities Message Struct
 * \since 20080111
 */
typedef struct {
	uint32_t layout;											/*!< Layout */
} layoutConfig_t;												/*!< SKINNY Layout Config Structure */

/*!
 * \brief SKINNY Video Level Preference Structure
 */
typedef struct {
	uint32_t lel_transmitPreference;									/*!< Transmit Preference */
	uint32_t lel_format;											/*!< Format / Codec */
	uint32_t lel_maxBitRate;										/*!< Maximum BitRate */
	uint32_t lel_minBitRate;										/*!< Minimum BitRate */
	uint32_t lel_MPI;											/*!<  */
	uint32_t lel_serviceNumber;										/*!< Service Number */
} levelPreference_t;												/*!< SKINNY Level Preference Structure */

/*!
 * \brief SKINNY Service Resource Structure
 */
typedef struct {
	uint32_t layoutCount;											/*!< Layout Count */
	layoutConfig_t layout[MAX_LAYOUT_WITH_SAME_SERVICE];							/*!< Layout */
	uint32_t serviceNum;											/*!< Service Number */
	uint32_t maxStreams;											/*!< Maximum number of Streams */
	uint32_t maxConferences;										/*!< Maximum number of Conferences */
	uint32_t activeConferenceOnRegistration;								/*!< Active Conference On Registration */
} serviceResource_t;												/*!< SKINNY Service Resource Structure */

/*!
 * \brief SKINNY Conference Resource
 */
typedef struct {
	uint32_t lel_activeStreamsOnRegistration;
	uint32_t lel_maxBW;
	uint32_t lel_serviceResourceCount;
	serviceResource_t serviceResource[MAX_SERVICE_TYPE];
} confResource_t ;

/*!
 * \brief SKINNY Picture Format Structure
 */
typedef struct {
	uint32_t lel_width;											/*!< Picture Width */
	uint32_t lel_height;											/*!< Picture Height */
	uint32_t lel_pixelAspectRatio;										/*!< Picture Pixel Aspect Ratio */
	uint32_t lel_pixelclockConversionCode;									/*!< Picture Pixel Conversion Code  */
	uint32_t lel_pixelclockDivisor;										/*!< Picture Pixel Divisor */
} customPictureFormat_t;											/*!< SKINNY Picture Format Structure */

typedef union {
	uint32_t lel_g723BitRate;										/*!< G723 Bit Rate : Enum(5.3: 0x01, 6.3: 0x02) */
	struct {
		uint32_t lel_capabilityAndVersion;
		uint32_t lel_modulationAndModem2833Support;
	} modemRelay;
	struct {
		uint16_t lel_chan0MaxPayload;
		uint16_t lel_chan2MaxPayload;
		uint16_t lel_chan3MaxPayload;
		uint16_t lel_chan2MaxWindow;
	} sprtPayload;
	struct {
		uint32_t lel_standardSupportField;
		uint32_t lel_vendorSupportField;
	} SupportEntry;
	struct {
		uint8_t codecMode ;
		uint8_t dynamicPayload;
		uint8_t codecParam1;
		uint8_t codecParam2;
	} codecParams;
} payload_t;

/*!
 * \brief SKINNY Audio Capabilities Structure
 */
typedef struct {
	skinny_codec_t lel_payloadCapability;									/*!< PayLoad Capability */
	uint32_t lel_maxFramesPerPacket;									/*!< Maximum Number of Frames per IP Packet */
	payload_t payloads;
} audioCap_t;													/*!< SKINNY Audio Capabilities Structure */

/*!
 * \brief SKINNY Audio Parameters Structure
 */
typedef struct {
	uint32_t millisecondPacketSize;										/*!< Packet Size per Milli Second */
	uint32_t lel_echoCancelType;										/*!< Echo Cancelation Type */
	uint32_t lel_g723BitRate;										/*!< only used with G.723 payload */
} audioParameter_t;												/*!< SKINNY Audio Parameters Structure */

/*!
 * \brief SKINNY Picture Format Structure
 */
typedef struct {
	uint32_t format;											/*!< Picture Format */
	uint32_t mpi;												/*!< MPI */
} pictureFormat_t;												/*!< SKINNY Picture Format Structure */

/*!
 * \brief SKINNY H261 Video Capability Structure
 */
typedef struct {
	uint32_t lel_temporalSpatialTradeOffCapability;								/*!< Temporal Spatial Trade Off Capability */
	uint32_t lel_stillImageTransmission;									/*!< Still Image Transmission */
} h261_VideoCapability_t;											/*!< SKINNY H261 Video Capability Structure */

/*!
 * \brief SKINNY H263 Video Capability Structure
 */
typedef struct {
	uint32_t lel_capabilityBitfield;									/*!< H263 Capability Bit-Field */
	uint32_t lel_annexNandWFutureUse;									/*!< Annex Nandw for Future Use */
} h263_VideoCapability_t;											/*!< SKINNY H262 Video Capability Structure */

/*!
 * \brief SKINNY vieo / h263P Video Capability Structure
 */
typedef struct {
	uint32_t lel_modelNumber;										/*!< Model Number */
	uint32_t lel_bandwidth;											/*!< BandWidth */
} h263P_VideoCapability_t;											/*!< SKINNY vieo/h263P Video Capability Structure */

/*!
 * \brief SKINNY H264 Video Capability Structure
 */
typedef struct {
	uint32_t lel_profile;											/*!< H264 profile */
	uint32_t lel_level;											/*!< H264 level */
	//uint32_t lel_customMaxMBPS;
	//uint32_t lel_customMaxFS;
	//uint32_t lel_customMaxDPB;
	//uint32_t lel_customMaxBRandCPB;
} h264_VideoCapability_t;

typedef union {
	h261_VideoCapability_t h261;
	h263_VideoCapability_t h263;
	h263P_VideoCapability_t h263P;										/*!< Also known as Vieo */
} videoCapabilityUnionV1_t;

/*!
 * \brief SKINNY Video Capabilities Structure
 */
typedef struct {
	skinny_codec_t lel_payloadCapability;									/*!< PayLoad Capability */
	uint32_t lel_transmitOrReceive;										/*!< Transmit or Receive */
	//uint32_t lel_protocolDependentData;
	//uint32_t lel_maxBitRate;
	uint32_t lel_levelPreferenceCount;									/*!< Level of Preference Count */
	levelPreference_t levelPreference[MAX_LEVEL_PREFERENCE];						/*!< Level Preference */
	videoCapabilityUnionV1_t capability;
} videoCapV1_t;													/*!< SKINNY Video Capabilities Structure */

typedef union {
	h261_VideoCapability_t h261;
	h263_VideoCapability_t h263;
	h263P_VideoCapability_t h263P;										/*!< Also known as Vieo */
	h264_VideoCapability_t h264;
} videoCapabilityUnionV2_t;
/*!
 * \brief SKINNY Video Capabilities Structure
 */
typedef struct {
	skinny_codec_t lel_payloadCapability;									/*!< PayLoad Capability */
	uint32_t lel_transmitOrReceive;										/*!< Transmit or Receive */
	uint32_t lel_levelPreferenceCount;									/*!< Level of Preference Count */

	levelPreference_t levelPreference[MAX_LEVEL_PREFERENCE];						/*!< Level Preference */
	videoCapabilityUnionV2_t capability;
} videoCapV2_t;													/*!< SKINNY Video Capabilities Structure */


/*!
 * \brief SKINNY Video Capabilities Structure
 */
typedef struct {
	skinny_codec_t lel_payloadCapability;									/*!< PayLoad Capability */
	uint32_t lel_transmitOrReceive;										/*!< Transmit or Receive */
	uint32_t lel_levelPreferenceCount;									/*!< Level of Preference Count */

	levelPreference_t levelPreference[MAX_LEVEL_PREFERENCE];						/*!< Level Preference */
	uint32_t lel_encryptionCapability;									/*!< Encryption Capability. ENUM(NotEncrCapable:0x00, EncrCapable: 0x01) */
	videoCapabilityUnionV2_t capability;
	uint32_t lel_ipv46;											/*!< Enum ModeIPv4: 0x00 / ModeIPv6: 0x01 / Both: 0x02 */
} videoCapV3_t;													/*!< SKINNY Video Capabilities Structure */

/*!
 * \brief SKINNY Data Capabilities Structure V1
 */
typedef struct {
	uint32_t lel_payloadCapability;										/*!< Payload Capability */
	uint32_t lel_transmitOrReceive;										/*!< Transmit or Receive */
	uint32_t lel_protocolDependentData;									/*!< Protocol Dependent Data */
	uint32_t lel_maxBitRate;										/*!< Maximum BitRate */
} dataCapV1_t;													/*!< SKINNY Data Capabilities Structure */

/*!
 * \brief SKINNY Data Capabilities Structure V2
 */
typedef struct {
	uint32_t lel_payloadCapability;										/*!< Payload Capability */
	uint32_t lel_transmitOrReceive;										/*!< Transmit or Receive */
	uint32_t lel_protocolDependentData;									/*!< Protocol Dependent Data */
	uint32_t lel_maxBitRate;										/*!< Maximum BitRate */
	uint32_t lel_encryptionCapability;									/*!< Encryption Capability. ENUM(NotEncrCapable:0x00, EncrCapable: 0x01) */
} dataCapV2_t;													/*!< SKINNY Data Capabilities Structure */

/*!
 * \brief SKINNY Data Parameter Structure
 */
typedef struct {
	uint32_t lel_protocolDependentData;									/*!< Protocol Dependent Data */
	uint32_t lel_maxBitRate;										/*!< Maximum BitRate */
} dataParameter_t;												/*!< SKINNY Data Parameter Structure */

/*! 
 * \brief SKINNY Video Parameter Structure 
 * \since 20110718
 */
typedef struct {
	uint32_t bitRate;											/*!< BitRate (default 384) */
	uint32_t pictureFormatCount;										/*!< Picture Format Count (default 0) */
	pictureFormat_t pictureFormat[5];									/*!< Picture Format Array */
	uint32_t confServiceNum;										/*!< Conf Service Number */
	uint32_t profile;
	uint32_t level;
	uint32_t macroblockspersec;
	uint32_t macroblocksperframe;
	uint32_t decpicbuf;
	uint32_t brandcpb;
	uint16_t dummy1;
	uint16_t dummy2;
	uint32_t dummy3;
	uint32_t dummy4;
	uint32_t dummy5;
	uint32_t dummy6;
	uint32_t dummy7;
	uint32_t dummy8;
} videoParameter_t;												/*!< SKINNY Video Parameter Structure */

/*! 
 * \brief SKINNY AnnouncementList Structure
 */
typedef struct {
	uint32_t lel_local;
	uint32_t lel_country;
	uint32_t lel_deviceTone;
} skinny_announcementList_t;											/*!< SKINNY AnnouncementList Structure */

typedef struct {
	uint32_t lel_active;
	struct {
		uint32_t lel_capsAndVersion;
		uint32_t lel_2833Support;
	} modemRelay;
	struct {
		uint16_t lel_chan0MaxPayload;
		uint16_t lel_chan2MaxPayload;
		uint16_t lel_chan3MaxPayload;
		uint16_t lel_chan2MaxWindow;
	} sptrPayload;
	struct {
		uint32_t lel_standardSupportField;
		uint32_t lel_vendorSupportField;
	} sse;
	struct {
		uint8_t nse;
		uint8_t rfc2833;
		uint8_t sse;
		uint8_t v150sptr;
		uint8_t noaudio;
		uint8_t FutureUse1;
		uint8_t FutureUse2;
		uint8_t FutureUse3;
	} dynamicPayload;
} skinny_latentCapsInfo_t;

/*!
 * \brief SKINNY Protocol Message Data Union
 */
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
		uint32_t lel_transactionID;									/*!< TransactionID */ /*Number Index (this must be shifted 4 bits right) */
		uint32_t lel_featureID;										/*!< LineInstance / BLF: 0x0001 */
		uint32_t lel_timer;										/*!< Timer */
		char subscriptionID[256];									/*!< SubscriptionID */
	} SubscriptionStatReqMessage;										/*!< SubscriptionStatReqMessage Message Structure */
	//} DialedPhoneBookMessage;										/*!< Dialed Phone Book Message Structure */

	struct {
		uint32_t lel_transactionID;									/*!< TransactionID */ /*!< Number Index (this must be shifted 4 bits right) */
		uint32_t lel_featureID;										/*!< LineInstance / BLF: 0x00001 */
		uint32_t lel_timer;										/*!< Timer */
		uint32_t lel_cause;										/*!< Cause (Enum): OK: 0x00, RouteFail:0x01, AuthFail:0x02, Timeout:0x03, TrunkTerm:0x04, TrunkForbidden:0x05, Throttle:0x06 */
	} SubscriptionStatMessage;										/*!< SubscriptionStatMessage */
	//} DialedPhoneBookAckMessage;										/*!< Dialed Phone Book Acknowledgement Structure */

	struct {
		uint32_t lel_transactionID;									/*!< TransactionID */ /*!< Number Index (this must be shifted 4 bits right) */
		uint32_t lel_featureID;										/*!< LineInstance / BLF: 0x01*/
		uint32_t lel_status;										/*!< Status */
		uint32_t lel_text[97];										/*~< Text */
	} NotificationMessage;											/*!< NotificationMessage / CallListStatusUpdate */
	//}CallListStateUpdate

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		char data[StationMaxXMLMessage];
	} UserToDeviceDataMessage;										/*!< User to Device Message Structure */

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		uint32_t lel_sequenceFlag;
		uint32_t lel_displayPriority;
		uint32_t lel_conferenceID;
		uint32_t lel_appInstanceID;
		uint32_t lel_routing;
		//char data[StationMaxXMLMessage];                                                              // dummy char for variable length message (Causes issues on 7960, needs further research)
		char data[];								// dummy char for variable length message (Revert)
	} UserToDeviceDataVersion1Message;									/*!< User to Device Version1 Message Structure */

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		char data[StationMaxXMLMessage];
	} DeviceToUserDataMessage;										/*!< Device to User Message Structure */

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		char data[StationMaxXMLMessage];
	} DeviceToUserDataResponseMessage;									/*!< Device to User Response Message Structure */

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		uint32_t lel_sequenceFlag;
		uint32_t lel_displayPriority;
		uint32_t lel_conferenceID;
		uint32_t lel_appInstanceID;
		uint32_t lel_routing;
		char data[StationMaxXMLMessage];
	} DeviceToUserDataVersion1Message;									/*!< Device to User Version1 Message Structure */

	struct {
		uint32_t lel_appID;
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
		uint32_t lel_transactionID;
		uint32_t lel_dataLength;
		uint32_t lel_sequenceFlag;
		uint32_t lel_displayPriority;
		uint32_t lel_conferenceID;
		uint32_t lel_appInstanceID;
		uint32_t lel_routing;
		char data[StationMaxXMLMessage];
	} DeviceToUserDataResponseVersion1Message;

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

	   diff:
	   >   00000000 - 08 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 ................
	   >   00000010 - 00 00 00 00 22 4B 01 00 0C CC CC CC 00 11 21 F1 ...."K..........
	   >   00000020 - 22 4B 00 74 AA AA 03 00 00 0C 20 00 02 B4 B8 67 "K.t...... ....g
	   >   00000030 - 00 01 00 13 53 45 50 30 30 31 31 32 31 46 31 32 ....SEP001122334
	   >   00000040 - 32 34 42 00 02 00 11 00 00 00 01 01 01 CC 00 04 455.............
	   >   00000050 - 0A 0A 0A D2 00 03 00 0A 50 6F 72 74 20 31 00 04 ........Port 1..
	   >   00000060 - 00 08 00 00 00 90 00 05 00 10 50 30 30 33 30 38 ..........P00308
	   >   00000070 - 30 30 31 30 30 30 00 06 00 17 43 69 73 63 6F 20 001000....Cisco
	   >   00000080 - 49 50 20 50 68 6F 6E 65 20 37 39 34 00 00 00 00 IP Phone 794....
	   >   00000130 - 00 00 00 00 00 00 00 00 19 00 00 00 78 00 00 00 ............x...
	   >   000001A0 - 00 00 00 00 00 00 00 00 01 01 00 00 04 00 00 00 ................
	   >   00000250 - 00 00 00 00 00 00 00 00 65 00 00 00 03 00 00 00 ........e.......
	   >   00000260 - 02 00 00 00 32 00 00 00 02 00 00 00 98 3A 00 00 ....2........:..
	   >   00000270 - F4 01 00 00 01 00 00 00 00 00 00 00 33 00 00 00 ............3...
	   >   00000280 - 03 00 00 00 98 3A 00 00 F4 01 00 00 01 00 00 00 .....:..........
	   >   000002C0 - 00 00 00 00 00 00 00 00 00 00 00 00 67 00 00 00 ............g...
	   >   000002D0 - 03 00 00 00 01 00 00 00 34 00 00 00 03 00 00 00 ........4.......
	   >   000002E0 - 98 3A 00 00 F4 01 00 00 01 00 00 00 00 00 00 00 .:..............
	   >   00000330 - 00 00 00 00 00 00 00 00 40 00 00 00 32 00 00 00 ........@...2...
	 */
	/* 7960 with video advantage on PC Port*/
	/*
	  00000000 - 34 07 00 00 00 00 00 00  30 00 00 00 08 00 00 00  - 4.......0....... 			audioCapCount
	  00000010 - 02 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................ VideoCap, dataCap, RTPPayload, customPictureFormatCount
	  00000020 - 00 00 01 00 0C CC CC CC  00 1B 54 CA 49 9B 00 7B  - ..........T.I..{ 
	  00000030 - AA AA 03 00 00 0C 20 00  02 B4 85 F7 00 01 00 13  - ...... .........
	  00000040 - 53 45 50 30 30 31 42 35  34 43 41 34 39 39 42 00  - SEP001B54CA499B.
	  00000050 - 02 00 11 00 00 00 01 01  01 CC 00 04 0A 0F 0F 8B  - ................
	  00000060 - 00 03 00 0A 50 6F 72 74  20 31 00 04 00 08 00 00  - ....Port 1......
	  00000070 - 04 90 00 05 00 10 50 30  30 33 30 38 30 31 30 32  - ......P003080102
	  00000080 - 30 30 00 06 00 17 43 69  73 63 6F 20 49 50 20 50  - 00....Cisco IP P
	  00000090 - 68 6F 6E 65 20 37 39 36  00 00 00 00 00 00 00 00  - hone 796........
	  000000A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000000B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000000C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000000D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000000E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000000F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000100 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000110 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000120 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000130 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000140 - 00 00 00 00 19 00 00 00  78 00 00 00 00 00 00 00  - ........x.......
	  00000150 - 00 00 00 00 04 00 00 00  28 00 00 00 00 00 00 00  - ........(.......
	  00000160 - 00 00 00 00 02 00 00 00  28 00 00 00 00 00 00 00  - ........(.......
	  00000170 - 00 00 00 00 0F 00 00 00  3C 00 00 00 00 00 00 00  - ........<.......
	  00000180 - 00 00 00 00 10 00 00 00  3C 00 00 00 00 00 00 00  - ........<.......
	  00000190 - 00 00 00 00 0B 00 00 00  3C 00 00 00 00 00 00 00  - ........<.......
	  000001A0 - 00 00 00 00 0C 00 00 00  3C 00 00 00 00 00 00 00  - ........<.......
	  000001B0 - 00 00 00 00 01 01 00 00  04 00 00 00 00 00 00 00  - ................
	  000001C0 - 00 00 00 00 00 00 00 00  00 00 00 00 2D 43 69 73  - ............-Cis
	  000001D0 - 63 6F 49 50 50 68 6F 6E  65 53 44 4B 56 65 72 73  - coIPPhoneSDKVers
	  000001E0 - 69 6F 6E 3A 20 34 2E 30  2E 31 0D 0A 78 2D 43 69  - ion: 4.0.1..x-Ci
	  000001F0 - 73 63 6F 49 50 50 68 6F  6E 65 44 69 73 70 6C 61  - scoIPPhoneDispla
	  00000200 - 79 3A 20 31 33 33 2C 36  35 2C 32 2C 47 0D 0A 0D  - y: 133,65,2,G...
	  00000210 - 0A 6F 6E 65 44 69 73 70  6C 61 79 3A 20 31 33 33  - .oneDisplay: 133
	  00000220 - 2C 36 35 2C 32 2C 47 0D  0A 43 6F 6F 6B 69 65 3A  - ,65,2,G..Cookie:
	  00000230 - 20 50 48 50 53 45 53 53  49 44 3D 6D 30 70 34 65  -  PHPSESSID=m0p4e
	  00000240 - 72 66 6C 69 32 72 30 70  34 6A 71 76 64 69 31 72  - rfli2r0p4jqvdi1r
	  00000250 - 6B 32 74 66 36 0D 0A 0D  0A 00 00 00 00 00 00 00  - k2tf6...........
	  00000260 - 00 00 00 00 65 00 00 00  03 00 00 00 02 00 00 00  - ....e...........
	  00000270 - 32 00 00 00 02 00 00 00  98 3A 00 00 F4 01 00 00  - 2........:......
	  00000280 - 01 00 00 00 00 00 00 00  33 00 00 00 03 00 00 00  - ........3.......
	  00000290 - 98 3A 00 00 F4 01 00 00  01 00 00 00 00 00 00 00  - .:..............
	  000002A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000002B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000002C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000002D0 - 00 00 00 00 00 00 00 00  67 00 00 00 03 00 00 00  - ........g.......
	  000002E0 - 01 00 00 00 34 00 00 00  03 00 00 00 98 3A 00 00  - ....4........:..
	  000002F0 - F4 01 00 00 01 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000300 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000310 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000320 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000330 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000340 - 00 00 00 00 40 00 00 00  32 00 00 00 40 00 00 00  - ....@...2...@...
	  00000350 - 32 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 2...............
	  00000360 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000370 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000380 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000390 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000003F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000400 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000410 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000420 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000430 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000440 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000450 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000460 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000470 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000480 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000490 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000004F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000500 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000510 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000520 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000530 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000540 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000550 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000560 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000570 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000580 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000590 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000005F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000600 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000610 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000620 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000630 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000640 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000650 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000660 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000670 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000680 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000690 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  000006F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000700 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000710 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000720 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	  00000730 - 00 00 00 00 00 00 00 00  00 00 00 00              - ............
	*/	 
	struct {
		union {
			struct {
				uint32_t lel_audioCapCount;									/*!< Audio Capability Count */
				uint32_t lel_videoCapCount;									/*!< Video Capability Count */
				uint32_t lel_dataCapCount;									/*!< Data Capability Count */
				uint32_t lel_RTPPayloadFormat;									/*!< RTP Payload Format */
				uint32_t lel_customPictureFormatCount;								/*!< Custom Picture Format Count */
				customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES];					/*!< Custom Picture Format */
				confResource_t confResources;
				audioCap_t audioCaps[SKINNY_MAX_CAPABILITIES];							/*!< Audio Capabilities */
				videoCapV1_t videoCaps[SKINNY_MAX_VIDEO_CAPABILITIES];						/*!< Video Capabilities */
				dataCapV1_t dataCaps[SKINNY_MAX_DATA_CAPABILITIES];						/*!< Data Capabilities */
			} v3;
			struct {
				uint32_t lel_audioCapCount;									/*!< Audio Capability Count */
				uint32_t lel_videoCapCount;									/*!< Video Capability Count */
				uint32_t lel_dataCapCount;									/*!< Data Capability Count */
				uint32_t lel_RTPPayloadFormat;									/*!< RTP Payload Format */
				uint32_t lel_customPictureFormatCount;								/*!< Custom Picture Format Count */
				customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES];					/*!< Custom Picture Format */
				confResource_t confResources;
				audioCap_t audioCaps[SKINNY_MAX_CAPABILITIES];							/*!< Audio Capabilities */
				videoCapV2_t videoCaps[SKINNY_MAX_VIDEO_CAPABILITIES];						/*!< Video Capabilities V2 */
				dataCapV1_t dataCaps[SKINNY_MAX_DATA_CAPABILITIES];						/*!< Data Capabilities */
			} v16;
		};
	} UpdateCapabilitiesMessage;										/*!< Update Capabilities Message Structure */

	struct {
		uint32_t lel_audioCapCount;									/*!< Audio Capability Count */
		uint32_t lel_videoCapCount;									/*!< Video Capability Count */
		uint32_t lel_dataCapCount;									/*!< Data Capability Count */
		uint32_t lel_RTPPayloadFormat;									/*!< RTP Payload Format */
		uint32_t lel_customPictureFormatCount;								/*!< Custom Picture Format Count */
		customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES];					/*!< Custom Picture Format */
		confResource_t confResources;
		audioCap_t audioCaps[SKINNY_MAX_CAPABILITIES];							/*!< Audio Capabilities */
		videoCapV2_t videoCaps[SKINNY_MAX_VIDEO_CAPABILITIES];						/*!< Video Capabilities V2 */
		dataCapV1_t dataCaps[SKINNY_MAX_DATA_CAPABILITIES];						/*!< Data Capabilities */
	} UpdateCapabilitiesV2Message;										/*!< */

	struct {
		uint32_t lel_audioCapCount;									/*!< Audio Capability Count */
		uint32_t lel_videoCapCount;									/*!< Video Capability Count */
		uint32_t lel_dataCapCount;									/*!< Data Capability Count */
		uint32_t lel_RTPPayloadFormat;									/*!< RTP Payload Format */
		uint32_t lel_customPictureFormatCount;								/*!< Custom Picture Format Count */
		customPictureFormat_t customPictureFormat[MAX_CUSTOM_PICTURES];					/*!< Custom Picture Format */
		confResource_t confResources;
		audioCap_t audioCaps[SKINNY_MAX_CAPABILITIES];							/*!< Audio Capabilities */
		videoCapV3_t videoCaps[SKINNY_MAX_VIDEO_CAPABILITIES];						/*!< Video Capabilities V3 */
		dataCapV2_t dataCaps[SKINNY_MAX_DATA_CAPABILITIES];						/*!< Data Capabilities */
		uint32_t unknown[70];										/*!< Hmmm */
	} UpdateCapabilitiesV3Message;									/*!< @see https://sourceforge.net/p/chan-sccp-b/bugs/181 */

	/* AccessoryStatusMessage (0x0073):
	 * This indicates the phone headset, handset or speaker status.
	 *
	 * FIRST DWORD:  0x0001 = HEADSET, 0x0002 = HANDSET, 0x0003 = SPEAKERMODE
	 * SECOND DWORD: 0x0001 = OFFHOOK, 0x0002 = ONHOOK
	 * THIRD DWORD:  ALWAYS 0x0000 UNKNOWN
	 */
	struct {
		uint32_t lel_AccessoryID;									/*!< Accessory ID (0x1=HeadSet, 0x2=HandSet, 0x3=Speaker) (MediaPathID) */
		uint32_t lel_AccessoryStatus;									/*!< Accessory Status (0x1=On, 0x2=Off) (MediaPathEvent) */
	} AccessoryStatusMessage;										/*!< Accessory Status Message Structure (MediaPathEventMessage) */

	// Message 0x4A len 12 (cisco ip communicator uses it)
	// 00000000 - 0C 00 00 00 16 00 00 00  4A 00 00 00 03 00 00 00  - ........J.......
	// 00000010 - 01 00 00 00                                       - ....
	struct {
		uint32_t lel_MediaPathID;									/*!< mediaPathID (0x1=HeadSet, 0x2=HandSet, 0x3=Speaker) */
		uint32_t lel_MediaPathCapabilities;								/*!< mediaPathCapabilities (0x1=Enable, 0x2=Disable, 0x3=Monitor) */
	} MediaPathCapabilityMessage;										/*!< \todo MediaPath Capability Message Structure */

	struct {												// INCOMPLETE
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_maxBitRate;									/*!< maxBitRate */
	} FlowControlNotifyMessage;										/*!< \todo FlowControlNotify Message Structure */

	struct {
		uint32_t lel_featureIndex;									/*!< Instance */
		uint32_t lel_featureID;										/*!< always 0x15 */
		uint32_t lel_featureStatus;									/*!< skinny_busylampfield_state_t */
		char featureTextLabel[StationDynamicNameSize];							/*!< SpeedDial Display Name \todo shoud be dynamic - readMessage - OVERRUN remaining bytes=29 messageType=0x146 */
	} FeatureStatDynamicMessage;										/*!< Speed Dial Stat Dynamic Message Structure */

	struct {
		uint32_t lel_Number;
		char DirNumber[StationMaxDirnumSize];
		char DisplayName[StationMaxNameSize];
	} SpeedDialStatDynamicMessage;										/*!< Speed Dial Stat Dynamic Message Structure */

	struct {												// OK
		uint32_t lel_displayTimeout;
		uint32_t dummy;
	} DisplayDynamicNotifyMessage;										// 0x0143

	struct {												// OK
		uint32_t lel_displayTimeout;
		uint32_t lel_priority;
		uint32_t dummy;
	} DisplayDynamicPriNotifyMessage;									// 0x0144;

	struct {
		uint32_t lel_messageTimeout;									/*!< Message Timeout */
		uint32_t lel_lineInstance;									/*!< Line Instance  */
		uint32_t lel_callReference;									/*!< Call Reference */
		/* here follow the message string
		 * take care rest of size should
		 * be calculated with string
		 * size + 1 (NULL TERMINATION)
		 * padded by 4
		 */
		uint32_t dummy;											/*!< Dummy, this is just for addressing, it doesn't matter */
	} DisplayDynamicPromptStatusMessage;									/*!< Display Dynamic Prompt Status Message Structure */

	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_serviceNum;
	} ClearConferenceMessage;

	struct {
		uint32_t lel_lineNumber;									/*!< Line Number */
		uint32_t lel_lineType;										/*!< Line Type: Bit-field: 1-Original Dialed 2-Redirected Dialed, 4-Calling line ID, 8-Calling name ID */
		char dummy[310];										/*!< Dummy (25+141+141) + 3 terminators */
	} LineStatDynamicMessage;										/*!< Line Stat Dynmic Message Structure */

	struct {
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_callType;										/*!< Call Type (INBOUND=1, OUTBOUND=2, FORWARD=3) */
		uint32_t lel_originalCdpnRedirectReason;							/*!< Original CalledParty Redirect Reason */
		uint32_t lel_lastRedirectingReason;								/*!< Last Redirecting Reason */
		uint32_t lel_callInstance;									/*!< Call Instance */
		uint32_t lel_callSecurityStatus;								/*!< Call Security Status */
		uint32_t partyPIRestrictionBits;								/*!< Party PI Restriction Bits Structure
														   0 RestrictCallingPartyName
														   1 RestrictCallingPartyNumber
														   2 RestrictCalledPartyName
														   3 RestrictCalledPartyNumber
														   4 RestrictOriginalCalledPartyName
														   5 RestrictOriginalCalledPartyNumber
														   6 RestrictLastRedirectPartyName
														   7 RestrictLastRedirectPartyNumber */
		uint32_t dummy;											/*!< Dummy */
														/* Dummy: Consists of:						// v3 - >v18
														 char callingParty[StationDynamicDirnumSize];		 	// 24 - 25
														 char calledParty[StationDynamicDirnumSize];			// 24 - 25
														 char originalCalledParty[StationDynamicDirnumSize];		// 24 - 25
														 char lastRedirectingParty[StationDynamicDirnumSize];		// 24 - 25
														 char cgpnVoiceMailbox[StationDynamicDirnumSize];		// 24 - 25
														 char cdpnVoiceMailbox[StationDynamicDirnumSize];		// 24 - 25
														 char originalCdpnVoiceMailbox[StationDynamicDirnumSize];	// 24 - 25	
														 char lastRedirectingVoiceMailbox[StationDynamicDirnumSize];	// 24 - 25
														 char callingPartyName[StationDynamicNameSize];			// 40 - 121
														 char calledPartyName[StationDynamicNameSize];			// 40 - 121
														 char originalCalledPartyName[StationDynamicNameSize];		// 40 - 121
														 char lastRedirectingPartyName[StationDynamicNameSize];		// 40 - 121
														 char HuntPilotNumber[StationDynamicDirnumSize];		// 24 - 25
														 char HuntPilotName[StationDynamicNameSize];			// 40 - 121
														 */
														 
														/* V2 (XML)
														char callingParty[25];
														char AlternateCallingParty[25];
														char calledParty[25];
														char originalCalledParty[25];
														char lastRedirectingParty[25];
														char cgpnVoiceMailbox[25];
														char cdpnVoiceMailbox[25];
														char originalCdpnVoiceMailbox[25];
														char lastRedirectingVoiceMailbox[25];
														char callingPartyName[121];
														char calledPartyName[121];
														char originalCalledPartyName[121];
														char lastRedirectingPartyName[121];
														char HuntPilotNumber[25];
														char HuntPilotName[121];
														*/
	} CallInfoDynamicMessage;										/*!< Call Information Dynamic Message Structure */

	struct {
		union {
			/* this is from a 7941
			   Unhandled SCCP Message: unknown(0x0154) 44 bytes length
			   00000000 - 03 00 00 00 FC FF FF FF 03 00 00 00 00 00 00 00 ................
			   00000010 - C0 A8 09 24 00 00 00 00 00 00 00 00 00 00 00 00 ...$............
			   00000020 - 00 00 00 00 03 00 00 00 00 00 00 00             ............
			 */
			struct {
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference1;							/*!< Call Reference1 */
				uint32_t bel_ipAddr;								/*!< Ip Address Array (This field is apparently in big-endian format, even though most other fields are in little-endian format.) */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_mediastatus;							/*!< Start Media Transmission Status */
			} v3;

			/* this is from a 6911
			   StartMediaTransmissionAck_v17(0x0154)
			   0000  00 0c 29 03 e3 98 0c 85  25 a7 a0 02 08 00 45 00   ..)..... %.....E.
			   0010  00 68 8d dd 40 00 40 06  08 4c c0 a8 11 7b c0 a8   .h..@.@. .L...{..
			   0020  11 9b 09 af 07 d0 3f c3  48 70 8d 0a 18 15 80 18   ......?. Hp......
			   0030  11 6c d3 0c 00 00 01 01  08 0a 00 01 ce 04 01 65   .l...... .......e
			   0040  cf 13 2c 00 00 00 11 00  00 00 54 01 00 00 00 00   ..,..... ..T.....   ; lel_reserved = 17 (11), message_id 0x154
			   0050  00 00 00 00 00 00 15 00  00 00 00 00 00 00 c0 a8   ........ ........   ; callreference = 0, passthrupartyid=0, callreference1=21 (15), unknown=0
			   0060  11 7b 00 00 00 00 00 00  00 00 00 00 00 00 98 2b   .{...... .......+   ; ip-address=c0a8117b, portnumber=982b0000, smtStatus=0
			   0070  00 00 00 00 00 00                                  ......           
			 */
			struct {
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference1;							/*!< Call Reference 1 */
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_ipAddr[16];								/*!< This field is apparently in big-endian format, even though most other fields are in little-endian format. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_mediastatus;							/*!< startmediatransmission status */
			} v17;
		};
	} StartMediaTransmissionAck;										/*!< Start Media Transmission Acknowledgement Structure */

	// No struct

	struct {
	} StationKeepAliveMessage;										/*!< Station Keep Alive Message */

	/* this is register message from 7940 with load 8.1.1 (protocol 0)
	 * 00000010 - 00 00 00 00 01 00 00 00 0A 0A 0A D2 07 00 00 00 ................
	 * 00000020 - 00 00 00 00 00 00 00 00 0B 00 60 85 00 00 00 00 ................
	 * 00000030 - 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000040 - 00 00 00 00                                     ....
	 */

        /* this is register message from 7936 with load  (protocol 0)
	 * 00000000 - 30 00 00 00 00 00 00 00 01 00 00 00 53 45 50 30 0...........SEP0
	 * 00000010 - 30 30 34 46 32 45 32 34 30 41 33 00 00 00 00 00 004F2E240A3.....
	 * 00000020 - 01 00 00 00 c0 a8 04 c8 43 75 00 00 00 00 00 00 ........Cu......
	 * 00000030 - 00 00 00 01 04 00 00 00                         ........
         */

	/* Register message from Cisco 7941 with load v8.5.3 (protocol 17)
	 *
	 * 00000000   53 45 50 30 30 31 46 39 45 32 36 36 33 36 39 00 SEP001F9E266369.
	 * 00000010   00 00 00 00 00 00 00 00 c0 a8 09 24 73 00 00 00 ...........$s...
	 * 00000020   05 00 00 00 00 00 00 00 11 00 72 85 01 00 00 00 ..........r.....
	 * 00000030   00 00 00 00 00 1f 9e 26 63 69 00 00 00 00 00 00 .......&ci......
	 * 00000040   03 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 ................
	 * 00000050   00 00 00 00 00 00 00 00 00 00 00 00 53 43 43 50 ............SCCP
	 * 00000060   34 31 2e 38 2d 35 2d 33 53 00 00 00 00 00 00 00 41.8-5-3S.......
	 * 00000070   00 00 00 00 00 00 00 00 00 00 00 00             ............
	 */

	/* Register message from Cisco 6961 with load v8.5.3 (protocol 15)
	 * 00000000 - 53 45 50 30 30 31 31 32 31 46 46 38 41 33 45 00 SEP001121FF8A3
	 * 00000010 - 00 00 00 00 01 00 00 00 53 DA 5E 14 F1 01 00 00 ........S.^...
	 * 00000020 - 28 00 00 00 00 00 00 00 0F 20 70 85 00 00 00 00 (........ p...
	 * 00000030 - 00 00 00 00 30 30 31 31 32 31 46 46 38 41 33 45 ....001121FF8A
	 * 00000040 - 00 00 00 00
	 */

	/* Register message from Cisco 7960 with v8.1.2 (protocol 11)
	 * 00000000 - 53 45 50 30 30 31 42 35 33 35 43 44 33 44 36 00 SEP001B535CD3D6.
	 * 00000010 - 00 00 00 00 01 00 00 00 0A 0F 0F B2 07 00 00 00 ................
	 * 00000020 - 00 00 00 00 00 00 00 00 0B 00 60 85 00 00 00 00 ................
	 * 00000030 - 00 00 00 00 00 00 00 00 00 00 00 00 52 49 4E 47 ............RING
	 * 00000040 - 00 00 00 00                                     ....
	 */
	struct {
		StationIdentifier sId;										/*!< Station Identifier */
		uint32_t stationIpAddr;										/*c0:a8:09:24=192.168.9.36 *//*!< Station IP Address */
		uint32_t lel_deviceType;									/*73:00:00:00=115 *//*!< Device Type as part of SKINNY_DEVICETYPE_* */
		uint32_t lel_maxStreams;									/*05:00:00:00=5 *//*!< Max Streams */
		uint32_t lel_activeStreams;									/*00:00:00:00=0 *//*!< number of active streams */
		uint32_t phone_features;									/*11:00:72:85   =17. *//*!< PhoneFeatures (ProtocolVersion + Features) */
		uint32_t lel_maxConferences;
		uint32_t lel_activeConferences;									/*00:00:00:00 *//*!< Unknown */
		uint8_t macAddress[12];										/* *//*!< byte array */
		uint32_t lel_ipV4AddressScope;
		uint32_t lel_maxNumberOfLines;
		char ipv6Address[16];
		uint32_t lel_ipV6AddressScope;
		char loadInfo[32];
		char configVersionStamp[48];									/* New since 11.5 */

		/* 7910:
		   02 00 00 00 // protocolVer (1st bit)
		   08 00 00 00 == 8
		   00 00 00 00
		   02 00 00 00 == 2
		   ce f1 00 00 // == (61092 / 206 / 241) 1668 dn-size 420
		 */
	} RegisterMessage;											/*!< Register Message Structure */

	struct {
		// All char arrays are in multiples of 32bit
		struct {
			char deviceName[StationMaxDeviceNameSize];						/*!< Device Name */
			uint32_t lel_stationUserId;								/*!< Station User ID (Not In Use) */
			uint32_t lel_stationInstance;								/*!< Station Instance */
		} station_identifier;										/*!< Station Identifier */

		char userName[StationMaxNameSize];								/*!< User Name */
		char serverName[StationMaxNameSize];								/*!< Server Name */
		uint32_t lel_numberLines;									/*!< Number of Lines configured */
		uint32_t lel_numberSpeedDials;									/*!< Number of SpeedDials configured */
	} ConfigStatMessage;											/*!< Configuration Status Message - Server -> Client */

	struct {
		// All char arrays are in multiples of 32bit
		struct {
			char deviceName[StationMaxDeviceNameSize];						/*!< Device Name */
			uint32_t lel_stationUserId;								/*!< Station User ID (Not In Use) */
			uint32_t lel_stationInstance;								/*!< Station Instance */
		} station_identifier;										/*!< Station Identifier */

		char userName[StationMaxNameSize];								/*!< User Name */
		char serverName[StationMaxNameSize];								/*!< Server Name */
		uint32_t lel_numberLines;									/*!< Number of Lines configured */
		uint32_t lel_numberSpeedDials;									/*!< Number of SpeedDials configured */
	} ConfigStatDynamicMessage;										/*!< Configuration Status Message - Server -> Client */

	struct {
		uint32_t lel_rtpMediaPort;									/*!< RTP Media Port */
	} IpPortMessage;											/*!< Ip Port Message - Superseded by including the IP Port info in the OpenReceiveChannelAck Message  */

	struct {
		uint32_t lel_kpButton;										/*!< KeyPad Button */
		uint32_t lel_lineInstance;									/*!< Line Instance on device */
		uint32_t lel_callReference;									/*!< Call Reference - current channel identifier */
	} KeypadButtonMessage;											/*!< KeyPad Button Message - Client -> Server */

	/* 8945 v22 unpacked */
	// 00000000 - 24 00 00 00 16 00 00 00  04 00 00 00 39 38 30 31  - $...........9801
	// 00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............
	// 00000020 - 00 00 00 00 00 00 00 00  01 00 00 00              - ............
	
	/* 7970 v22 packed*/
	// 00000000 - 24 00 00 00 16 00 00 00  04 00 00 00 39 38 30 31  - $...........9801
	// 00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............
	// 00000020 - 00 00 00 00 00 01 00 00  00 00 00 00              - ............

	struct {
		union {
			struct {
				char calledParty[StationMaxDirnumSize];						/*!< Called Party */
			} v3;
			struct {
				char calledParty[StationMaxDirnumSize];						/*!< Called Party */
				uint32_t lel_lineInstance;
			} v17;
			/* 8945 */
			struct {										/* packing needed because of char[25] */
				char calledParty[25];								/*!< Called Party */
				uint32_t lel_lineInstance;
			} v18u;
			/* 7970 */
			struct __attribute__ ((__packed__)) {							/* packing needed because of char[25] */
				char calledParty[25];								/*!< Called Party */
				uint32_t lel_lineInstance;
			} v18p;
		};
	} EnblocCallMessage;											/*!< Enbloc Call Message - Client -> Server */

	struct {
		uint32_t lel_stimulus;										/*!< Stimulus (ENUM: skinny_stimulus) */
		uint32_t lel_stimulusInstance;									/*!< Stimulus Instance (normally set to 1 (except speed dial and line)) */
		uint32_t lel_callReference;									/*!< Call Reference - current channel identifier */
		uint32_t lel_stimulusStatus;									/*!< Stimulus Status */
	} StimulusMessage;											/*!< Stimulus Message - Client -> Server */

	/*
	 * 7960: 00000000 - 0C 00 00 00 00 00 00 00  06 00 00 00 00 00 00 00  - ................
	 *       00000010 - 00 00 00 00                                       - ....
	 *
	 * 7962: 00000000 - 0C 00 00 00 16 00 00 00  06 00 00 00 00 00 00 00  - ................
	 *       00000010 - 00 00 00 00                                       - ....
	 */
	struct {
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} OffHookMessage;											/*!< Off Hook Message Structure */

	struct {
		char callingPartyNumber[StationMaxDirnumSize];							/*!< Calling Party Number */
	} OffHookMessageWithCallingPartyNum;									/*!< Off Hook With Calling Party Number Message Structure 
														   Goes Off Hook and provides a Calling Party Number to the PBX used by multiline Devices
														 */

	/*
	 * 7960: 00000000 - 0C 00 00 00 00 00 00 00  07 00 00 00 00 00 00 00  - ................
	 *       00000010 - 00 00 00 00                                       - ....
	 *
	 * 7962: 00000000 - 0C 00 00 00 16 00 00 00  07 00 00 00 00 00 00 00  - ................
	 *       00000010 - 00 00 00 00                                       - ....
	 */
	struct {
		uint32_t lel_buttonIndex;									/*!< Button Index instead of lineInstance */
		uint32_t lel_callReference;
	} OnHookMessage;											/*!< On Hook Message Structure */

	struct {
		uint32_t lel_lineInstance;
		uint32_t lel_callReference;
	} HookFlashMessage;											/*!< Hook Flash Message Structure */

	struct {
		uint32_t lel_deviceType;									/*!< Device Type as part of SKINNY_DEVICETYPE_* */
		uint32_t lel_numberOfInServiceStreams;								/*!< Number Of In-Service Streams  */
		uint32_t lel_maxStreamsPerConf;									/*!< Maximum Streams Per Configuration */
		uint32_t lel_numberOfOutOfServiceStreams;							/*!< Number of Out-Of_Service Streams */
	} MediaResourceNotification;										/*!< Media Resource Notification - Client -> Server */

	struct {
		uint32_t lel_lineNumber;									/*!< lineNumber for which Forward should be set */
	} ForwardStatReqMessage;										/*!< Forward Status Request Message - Client -> Server */

	struct {
		uint32_t lel_speedDialNumber;									/*!< instance on device */
	} SpeedDialStatReqMessage;										/*!< Speed Dial Status Request Message - Client -> Server */

	struct {
		uint32_t lel_lineNumber;									/*!< instance on device */
	} LineStatReqMessage;											/*!< Line Status Request Message - Client -> Server */

	struct {
	} ConfigStatReqMessage;											/*!< Configuration Status Request Message - Client -> Server */

	struct {
	} TimeDateReqMessage;											/*!< Time Date Request Message  - Client -> Server */

	struct {
		uint32_t lel_totalButtonCount;									/*!< number of available buttons */
	} ButtonTemplateReqMessage;										/*!< Button Template Request Message - Client -> Server  */

	struct {
	} VersionReqMessage;											/*!< Version Request Message - Client -> Server */

	struct {
		uint32_t lel_count;										/*!< Count */
		MediaCapabilityStructure caps[SKINNY_MAX_CAPABILITIES];						/*!< MediaCapabilities */
	} CapabilitiesResMessage;										/*!< Capabilities Resource Message - Client -> Server */

	struct {
		uint32_t portCount;										/*!< portCount contains the total number of RTP ports supported by the device. */
		uint32_t rtpMediaPort[StationMaxPorts];								/*!< RtpMediaPort contains the list of RTP port numbers. */
	} MediaPortListMessage;											/*!< Media Port List Message Structure
														   Used instead of the StationIpPort message for Devices which have multiple RTP stream ports. 
														   Devices like media bridges */

	struct {
	} ServerReqMessage;											/*!< Server Request Message - Client -> Server */

	struct {
		uint32_t lel_alarmSeverity;									/*!< Alarm Severity Level */
		char text[StationMaxAlarmMessage];								/*!< Alarm Text */
		uint32_t lel_parm1;										/*!< Alarm Parameter 1 */
		uint32_t lel_parm2;										/*!< Alarm Parameter 2 */
	} AlarmMessage;												/*!< Alarm Message - Client -> Server */

	/* 0x34 FeatureStatReqMessage */
	struct {
		uint32_t lel_featureIndex;									/*!< index on device */
		uint32_t lel_featureCapabilities;								/*!< feature capabilities */
	} FeatureStatReqMessage;										/*!< Feature Stat Request Message - Client -> Server */

	struct {
		uint32_t lel_serviceURLIndex;									/*!< instance on device */
	} ServiceURLStatReqMessage;										/*!< Service URL Stat Request Message - Client -> Server */


	struct {
		uint32_t receptionStatus;									/*!< Reception Status */
		uint32_t passThruPartyID;									/*!< Pass Through Party ID */
	} MulticastMediaReceptionAck;										/*!< Multicast Media Reception Acknowledgement Message - Client -> Server */

	struct {
		union {
			struct {
				uint32_t lel_mediastatus;							/*!< receiveChanStatus (OK:0x00 / Error: 0x01)*/
				uint32_t bel_ipAddr;								/*!< This field is apparently in big-endian format,
														   even though most other fields are in
														   little-endian format. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference;							/*!< Call Reference */
			} v3;
			struct {
				uint32_t lel_mediastatus;							/*!< Receive Channel Status (OK:0x00 / Error: 0x01)*/
				uint32_t lel_ipv46;								/*!< ipv4 (0)/ ipv6 (1) */
				char bel_ipAddr[16];								/*!< This field is apparently in big-endian format,
														   even though most other fields are in
														   little-endian format. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference;							/*!< Call Reference */
			} v17;
		};
	} OpenReceiveChannelAck;										/*!< Open Receive Channel Acknowledgement */

	struct {
		union {
			struct {
				uint32_t lel_mediastatus;							/*!< receiveChanStatus (OK:0x00 / Error: 0x01)*/
				uint32_t bel_ipAddr;								/*!< This field is apparently in big-endian format,
														   even though most other fields are in
														   little-endian format. */
				uint32_t lel_portNumber;
				uint32_t lel_passThruPartyId;
				uint32_t lel_callReference;
			} v3;

			struct {
				uint32_t lel_mediastatus;							/*!< status (OK:0x00 / Error: 0x01)*/
				uint32_t lel_ipv46;								/*!< ipv4 (0)/ ipv6 (1) */
				char bel_ipAddr[16];								/*!< This field is apparently in big-endian format,
														   even though most other fields are in
														   little-endian format. */
				uint32_t lel_portNumber;
				uint32_t lel_passThruPartyId;
				uint32_t lel_callReference;
			} v17;
		};
	} OpenMultiMediaReceiveChannelAckMessage;

	struct {
		union {

			/*
			   00000000 - 9C 02 00 00 00 00 00 00  23 00 00 00 39 38 30 33  - ........#...9803
			   00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............
			   00000020 - 00 00 00 00 04 00 00 00  00 00 00 00 2F 00 00 00  - ............/...
			   00000030 - 94 1F 00 00 30 00 00 00  40 20 00 00 00 00 00 00  - ....0...@ ......
			   00000040 - 00 00 00 00 00 00 00 00  6F 00 00 00 4D 4C 51 4B  - ........o...MLQK
			   00000050 - 3D 30 2E 30 30 30 30 3B  4D 4C 51 4B 61 76 3D 30  - =0.0000;MLQKav=0
			   00000060 - 2E 30 30 30 30 3B 4D 4C  51 4B 6D 6E 3D 30 2E 30  - .0000;MLQKmn=0.0
			   00000070 - 30 30 30 3B 4D 4C 51 4B  6D 78 3D 30 2E 30 30 30  - 000;MLQKmx=0.000
			   00000080 - 30 3B 4D 4C 51 4B 76 72  3D 30 2E 39 35 3B 43 43  - 0;MLQKvr=0.95;CC
			   00000090 - 52 3D 30 2E 30 30 30 30  3B 49 43 52 3D 30 2E 30  - R=0.0000;ICR=0.0
			   000000A0 - 30 30 30 3B 49 43 52 6D  78 3D 30 2E 30 30 30 30  - 000;ICRmx=0.0000
			   000000B0 - 3B 43 53 3D 30 3B 53 43  53 3D 30 00 00 00 00 00  - ;CS=0;SCS=0.....
			   000000C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000000D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000000E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000000F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000100 - 00 00 00 00 00 00 00 00  00 00 00 00 00 48 54 54  - .............HTT
			   00000110 - 50 5F 31 32 36 36 32 5F  31 32 37 33 35 00 00 00  - P_12662_12735...
			   00000120 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000130 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000140 - 00 00 00 00 00 00 00 00  8F 12 00 00 00 80 00 00  - ................
			   00000150 - A4 1C 48 00 0A 0F 0F C3  08 00 03 00 94 29 41 00  - ..H..........)A.
			   00000160 - 20 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  -  ...............
			   00000170 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000180 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000190 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001C0 - 00 00 00 00 74 18 4C 76  0A 0F 0F C3 00 00 00 00  - ....t.Lv........
			   000001D0 - 69 96 B8 00 CC 42 48 00  00 00 82 07 0A 00 00 00  - i....BH.........
			   000001E0 - 24 3D 41 00 00 00 01 03  01 00 00 00 0C 00 00 00  - $=A.............
			   000001F0 - 44 97 41 00 84 02 00 00  01 02 00 00 77 B9 94 37  - D.A.........w..7
			   00000200 - 04 00 00 00 03 00 00 00  08 00 00 00 03 01 01 01  - ................
			   00000210 - 01 01 01 01 01 01 01 01  01 01 06 01 01 01 01 01  - ................
			   00000220 - 01 01 01 01 01 01 01 01  01 01 03 00 00 00 00 00  - ................
			   00000230 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000240 - 00 00 00 00 00 00 00 00  00 00 00 00 4C 49 53 54  - ............LIST
			   00000250 - 2E 58 4D 4C 00 00 00 00  00 00 00 00 00 00 00 00  - .XML............
			   00000260 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000270 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000280 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000290 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000002A0 - 00 00 00 00                                       - ....
			 */
			struct {
				char DirectoryNumber[StationMaxDirnumSize];					/*!< Directory Number */
				uint32_t lel_CallIdentifier;							/*!< Call Identifier */
				uint32_t lel_StatsProcessingType;						/*!< Stats Processing Type */
				uint32_t lel_SentPackets;							/*!< Sent Packets */
				uint32_t lel_SentOctets;							/*!< Sent Octets */
				uint32_t lel_RecvdPackets;							/*!< Received Packets */
				uint32_t lel_RecvdOctets;							/*!< Received Octets */
				uint32_t lel_LostPkts;								/*!< Lost Packets */
				uint32_t lel_Jitter;								/*!< Jitter */
				uint32_t lel_latency;								/*!< Latency */
				uint32_t lel_QualityStatsSize;
				char QualityStats[600];								/*!< QualityStatistics */
			} v3;											/*!< Connection Statistics Response Message - Server -> Client */
			/*
			   00000000 - A0 02 00 00 14 00 00 00  23 00 00 00 31 33 37 00  - ........#...137.
			   00000010 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000020 - 00 00 00 00 00 00 00 00  03 00 00 00 00 00 00 00  - ................              // unknown1=0, callIdentifier=3, StatsProcessingType=0, 
			   00000030 - 99 00 00 00 A0 5F 00 00  91 00 00 00 A0 5A 00 00  - ....._.......Z..              // sendPackets=99=153,sendOctets=A05F=41055, RecvdPackets=91=145, RecvdOctets=A05A=41050
			   00000040 - 00 00 00 00 00 00 00 00  00 00 00 00 74 00 00 00  - ............t...              // lostPkts=0, Jitter=0, Latency=0, unknown2=74
			   00000050 - 4C 6F 67 20 34 37 36 3A  20 6D 6F 73 20 30 2E 30  - Log 476: mos 0.0              //
			   00000060 - 30 30 30 2C 20 61 76 67  4D 6F 73 20 30 2E 30 30  - 000, avgMos 0.00
			   00000070 - 30 30 2C 20 6D 61 78 4D  6F 73 20 30 2E 30 30 30  - 00, maxMos 0.000
			   00000080 - 30 2C 20 6D 69 6E 4D 6F  73 20 30 2E 30 30 30 30  - 0, minMos 0.0000
			   00000090 - 2C 20 43 53 20 30 2C 20  53 43 53 20 30 2C 20 43  - , CS 0, SCS 0, C
			   000000A0 - 43 52 20 30 2E 30 30 30  30 2C 20 49 43 52 20 30  - CR 0.0000, ICR 0
			   000000B0 - 2E 30 30 30 30 2C 20 6D  61 78 43 52 20 30 2E 30  - .0000, maxCR 0.0
			   000000C0 - 30 30 30 0A 00 00 00 00  00 00 00 00 00 00 00 00  - 000.............
			   000000D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000000E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000000F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000100 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000110 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000120 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000130 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000140 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000150 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000160 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000170 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000180 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000190 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001B0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001C0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001D0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001E0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000001F0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000200 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000210 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000220 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000230 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000240 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000250 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000260 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000270 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000280 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   00000290 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
			   000002A0 - 00 00 00 00 00 00 00 00                           - ........
			 */
			struct {
				char DirectoryNumber[StationMaxDirnumSize];					/*!< Directory Number */
				uint32_t lel_padding;
				uint32_t lel_CallIdentifier;							/*!< Call Identifier */
				uint32_t lel_StatsProcessingType;						/*!< Stats Processing Type */
				uint32_t lel_SentPackets;							/*!< Sent Packets */
				uint32_t lel_SentOctets;							/*!< Sent Octets */
				uint32_t lel_RecvdPackets;							/*!< Received Packets */
				uint32_t lel_RecvdOctets;							/*!< Received Octets */
				uint32_t lel_LostPkts;								/*!< Lost Packets */
				uint32_t lel_Jitter;								/*!< Jitter */
				uint32_t lel_latency;								/*!< Latency */
				uint32_t lel_QualityStatsSize;
				char QualityStats[600];								/*!< QualityStatistics */
			} v20;											/*!< Connection Statistics Response Message - Client -> Server (used when protocol version >= 19) */

			/*
			   00000000 - B4 00 00 00 16 00 00 00  23 00 00 00 39 38 30 31  - ........#...9801      // DirectoryNumber= 98031
			   00000010 - 31 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - 1...............      
			   00000020 - 00 00 00 00 00 00 00 00  01 00 00 00 00 5C 00 00  - .............\..      // callIdentifier=1, StatsProcessingType= 0, sendPackets=5c=92
			   00000030 - 00 D0 3D 00 00 59 00 00  00 CC 3B 00 00 00 00 00  - ..=..Y....;.....      // sendOctets=D03D=53309, RecvdPackets=59=89, RecvdOctets=cc3b=52283
			   00000040 - 00 00 00 00 00 00 00 00  00 6F 00 00 00 4D 4C 51  - .........o...MLQ      // lostPkts=0, Jitter=0, Latency=0, unknown2=6F000000
			   00000050 - 4B 3D 30 2E 30 30 30 30  3B 4D 4C 51 4B 61 76 3D  - K=0.0000;MLQKav=
			   00000060 - 30 2E 30 30 30 30 3B 4D  4C 51 4B 6D 6E 3D 30 2E  - 0.0000;MLQKmn=0.
			   00000070 - 30 30 30 30 3B 4D 4C 51  4B 6D 78 3D 30 2E 30 30  - 0000;MLQKmx=0.00
			   00000080 - 30 30 3B 49 43 52 3D 30  2E 30 30 30 30 3B 43 43  - 00;ICR=0.0000;CC
			   00000090 - 52 3D 30 2E 30 30 30 30  3B 49 43 52 6D 78 3D 30  - R=0.0000;ICRmx=0
			   000000A0 - 2E 30 30 30 30 3B 43 53  3D 30 3B 53 43 53 3D 30  - .0000;CS=0;SCS=0
			   000000B0 - 3B 4D 4C 51 4B 76 72 3D  30 2E 39 35              - ;MLQKvr=0.95
			 */

#pragma pack(push)
#pragma pack(1)
			struct {
				//char DirectoryNumber[25];
				char DirectoryNumber[28];
				uint32_t lel_CallIdentifier;							/*!< Call Identifier */
				uint8_t lel_StatsProcessingType;						/*!< Stats Processing Type */
				uint32_t lel_SentPackets;							/*!< Sent Packets */
				uint32_t lel_SentOctets;							/*!< Sent Octets */
				uint32_t lel_RecvdPackets;							/*!< Received Packets */
				uint32_t lel_RecvdOctets;							/*!< Received Octets */
				uint32_t lel_LostPkts;								/*!< Lost Packets */
				uint32_t lel_Jitter;								/*!< Jitter */
				uint32_t lel_latency;								/*!< Latency */
				uint32_t lel_QualityStatsSize;
				char QualityStats[600];								/*!< QualityStatistics */
			} v22;											/*!< Connection Statistics Response Message - Client -> Server (used when protocol version >= 22) */
#pragma pack(pop)
		};
	} ConnectionStatisticsRes;

	struct {
		char calledParty[StationMaxDirnumSize];								/*!< Called Party Array */
	} OffHookWithCgpnMessage;										/*!< Off Hook With Calling Party Name Message Structure */

	struct {
	} SoftKeySetReqMessage;											/*!< Soft Key Set Request Message Structure */

	struct {
		uint32_t lel_softKeyEvent;									/*!< Soft Key Event */
		uint32_t lel_lineInstance;									/*!< Line Instance on device (indicated callplane) */
		uint32_t lel_callReference;									/*!< Call Reference ID */
	} SoftKeyEventMessage;

	struct {
		uint32_t lel_UnregisterReason;
	} UnregisterMessage;											/*!< Unregister Message Structure */

	struct {
	} SoftKeyTemplateReqMessage;										/*!< Soft Key Template Request Message Structure */

	struct {
		StationIdentifier sId;										/*!< Station Identifier */
		uint32_t lel_stationIpAddr;									/*!< Station IP Address */
		uint32_t lel_deviceType;									/*!< Device Type as part of SKINNY_DEVICETYPE_* */
		char ipv6Address[16];
		uint32_t unknown;										/*!< Active Call ?? */
	} RegisterTokenRequest;											/*!< Register Token Request */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		uint32_t bel_remoteIpAddr;									/*!< This field is apparently in big-endian */
		uint32_t lel_remotePortNumber;									/*!< Remote Port Number */
		uint32_t lel_callRef;										/*!< Call Reference */
	} MediaTransmissionFailure;

	struct {
		uint32_t lel_hsMode;										/*!< Head Set Mode */
	} HeadsetStatusMessage;											/*!< Headset Status Message Structure */

	struct {
		uint32_t lel_keepAliveInterval;									/*!< Keep Alive Interval to the Primary Server */
		char dateTemplate[StationDateTemplateSize];							/*!< Date Template */
		uint8_t alignmentPadding;									/*!< Filler 1 */
		uint32_t lel_secondaryKeepAliveInterval;							/*!< Keep Alive Interval to the Secundairy Server */
		uint8_t maxProtocolVer;										/*!< Protocol Version */
		uint8_t protocolVer2;										/*!< Protocol Version Part 2 */
		uint8_t phoneFeatures1;										/*!< Phone Features Part 1*/
		uint8_t phoneFeatures2;										/*!< Phone Features Part 2 */
	} RegisterAckMessage;											/*!< Register Acknowledgement Message Structure */

	struct {
		uint32_t lel_tone;										/*!< Tone (SKINNY_TONE ENUM)*/
		uint32_t lel_toneDirection;									/*!< Tone Direction (0 = User, 1=Network, 2=Both) */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
	} StartToneMessage;											/*!< Start Tone Message Structure */

	struct {
		/* protocol v3 */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
		/* protocol > v11 */
		uint32_t lel_tone;										/*!< Tone (SKINNY_TONE ENUM)*/
	} StopToneMessage;

	struct {
		uint32_t lel_ringMode;										/*!< Ring Mode */
		uint32_t lel_ringDuration;									/*!< Ring Duration (Normal:0x01 /Single:0x02) */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
	} SetRingerMessage;											/*!< Set Ringer Message Structure */

	struct {
		uint32_t lel_stimulus;										/*!< Stimulus */
		uint32_t lel_stimulusInstance;									/*!< Stimulus Instance */
		uint32_t lel_lampMode;										/*!< Lamp Mode (MWI) */
	} SetLampMessage;											/*!< Set Lamp Message Structure */

	struct {
	} SetHookFlashDetectMessage;											/*!< Set Hkf Detect Message Structure */

	struct {
		uint32_t lel_speakerMode;									/*!< Spreaker Mode */
	} SetSpeakerModeMessage;										/*!< Set Speaker Mode Message Structure */

	struct {
		uint32_t lel_micMode;										/*!< Microphone Mode */
	} SetMicroModeMessage;											/*!< Set Microphone Mode Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t bel_remoteIpAddr;							/*!< This field is apparently in big-endian
														   format, even though most other fields are
														   little-endian. */
				uint32_t lel_remotePortNumber;							/*!< Remote Port Number */
				uint32_t lel_millisecondPacketSize;						/*!< Packet Size per MilliSecond */
				uint32_t lel_payloadType;							/*!< Media_PayloadType */
				uint32_t lel_precedenceValue;							/*!< Precedence Value */
				uint32_t lel_ssValue;								/*!< Silence Suppression Value */
				uint32_t lel_maxFramesPerPacket;						/*!< Maximum Frames per Packet */
				uint32_t lel_g723BitRate;							/*!< only used with G.723 payload */
				uint32_t lel_callReference;							/*!< Conference ID 1 */
				EncryptionInfo encryptioninfo;
				/* protocol v11 mods */
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMF PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Type */
				/* protocol v11 fields */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
			} v3;

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
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_ipv46;								/*!< ipv4 (0)/ ipv6 (1) */
				char bel_remoteIpAddr[16];							/*!< This field is apparently in big-endian
														   format, even though most other fields are
														   little-endian. */
				uint32_t lel_remotePortNumber;							/*!< Remote Port Number */
				uint32_t lel_millisecondPacketSize;						/*!< Packet Size per Millisecond */
				uint32_t lel_payloadType;							/*!< Media_PayloadType */
				uint32_t lel_precedenceValue;							/*!< Precedence Value */
				uint32_t lel_ssValue;								/*!< Silence Suppression Value */
				uint32_t lel_maxFramesPerPacket;						/*!< Maximum Frames per Packet */
				uint32_t lel_g723BitRate;							/*!< G.723 BitRate (only used with G.723 payload) */
				uint32_t lel_callReference;							/*!< Conference ID 1 */
				EncryptionInfo encryptioninfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMP PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Timeout (this is set to 0x0A) */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
			} v17;

			struct {
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_ipv46;								/*!< ipv4 (0)/ ipv6 (1) */
				char bel_remoteIpAddr[16];							/*!< This field is apparently in big-endian
														   format, even though most other fields are
														   little-endian. */
				uint32_t lel_remotePortNumber;							/*!< Remote Port Number */
				uint32_t lel_millisecondPacketSize;						/*!< Packet Size per Millisecond */
				uint32_t lel_payloadType;							/*!< Media_PayloadType */
				uint32_t lel_precedenceValue;							/*!< Precedence Value */
				uint32_t lel_ssValue;								/*!< Silence Suppression Value */
				uint32_t lel_maxFramesPerPacket;						/*!< Maximum Frames per Packet */
				uint32_t lel_g723BitRate;							/*!< G.723 BitRate (only used with G.723 payload) */
				uint32_t lel_callReference;							/*!< Conference ID 1 */
				EncryptionInfo encryptioninfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMP PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Timeout (this is set to 0x0A) */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
				skinny_latentCapsInfo_t latentCapsInfo;
			} v22;
		};
	} StartMediaTransmission;										/*!< Start Media Transmission Structure */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;									/*!< Conference ID 1 */
		uint32_t lel_portHandlingFlag;									/*!< 0 = Close Port, 1 = Keep Port Open */
	} StopMediaTransmission;										/*!< Stop Media Transmission Structure */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;									/*!< Conference ID 1 */
		uint32_t lel_portHandlingFlag;									/*!< 0 = Close Port, 1 = Keep Port Open */
	} StopMultiMediaTransmission;

	struct {
	} StartMediaReception;

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID 1 */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
	} StopMediaReception;											/*!< Stop Media Reception Structure */

	struct {
		char callingPartyName[StationMaxNameSize];							/*!< Calling Party Name */
		char callingParty[StationMaxDirnumSize];							/*!< Calling Party ID */
		char calledPartyName[StationMaxNameSize];							/*!< Called Party Name */
		char calledParty[StationMaxDirnumSize];								/*!< Called Party ID */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_callType;										/*!< Call Type (INBOUND=1, OUTBOUND=2, FORWARD=3) */
		char originalCalledPartyName[StationMaxNameSize];						/*!< Original Calling Party Name */
		char originalCalledParty[StationMaxDirnumSize];							/*!< Original Calling Party ID */
		char lastRedirectingPartyName[StationMaxNameSize];						/*!< Original Called Party Name */
		char lastRedirectingParty[StationMaxDirnumSize];						/*!< Original Called Party ID */
		uint32_t lel_originalCdpnRedirectReason;								/*!< Original Called Party Redirect Reason */
		uint32_t lel_lastRedirectingReason;									/*!< Last Redirecting Reason */
		char cgpnVoiceMailbox[StationMaxDirnumSize];							/*!< Calling Party Voicemail Box */
		char cdpnVoiceMailbox[StationMaxDirnumSize];							/*!< Called Party Voicemail Box */
		char originalCdpnVoiceMailbox[StationMaxDirnumSize];						/*!< Original Called Party VoiceMail Box */
		char lastRedirectingVoiceMailbox[StationMaxDirnumSize];						/*!< Last Redirecting VoiceMail Box */
		uint32_t lel_callInstance;									/*!< Call Instance */
		uint32_t lel_callSecurityStatus;								/*!< Call Security Status */
		uint32_t partyPIRestrictionBits;								/*!< Party PI Restriction Bits (
														   0 RestrictCallingPartyName
														   1 RestrictCallingPartyNumber
														   2 RestrictCalledPartyName
														   3 RestrictCalledPartyNumber
														   4 RestrictOriginalCalledPartyName
														   5 RestrictOriginalCalledPartyNumber
														   6 RestrictLastRedirectPartyName
														   7 RestrictLastRedirectPartyNumber) */
	} CallInfoMessage;											/*!< Call Info Message Structure */

 	struct {
 		union {
 			struct {
				char calledParty[StationMaxDirnumSize];						/*!< Called Party */
				uint32_t lel_lineInstance;							/*!< Line Instance */
				uint32_t lel_callReference;							/*!< Call Reference */
 			} v3;
#pragma pack(push)
#pragma pack(1)
 			struct {
				char calledParty[StationDynamicDirnumSize];					/*!< Called Party */
				uint32_t lel_lineInstance;							/*!< Line Instance */
				uint32_t lel_callReference;							/*!< Call Reference */
 			} v18;
#pragma pack(pop)
 		};
 	} DialedNumberMessage;											/*!< Dialed Number Message Structure */
	
 	struct {
		union {
			struct {
				uint32_t lel_activeForward;							/*!< Status (0=inactive, 1=active, 4=cm ??) */
				uint32_t lel_lineNumber;							/*!< Line Number */
				uint32_t lel_forwardAllActive;							/*!< Call Forward All Status */
				char cfwdallnumber[StationMaxDirnumSize];					/*!< Call Forward All Number */
				uint32_t lel_forwardBusyActive;							/*!< Call Forward on Busy Status */
				char cfwdbusynumber[StationMaxDirnumSize];					/*!< Call Forward on Busy Number */
				uint32_t lel_forwardNoAnswerActive;						/*!< Call Forward on No-Answer Status */
				char cfwdnoanswernumber[StationMaxDirnumSize];					/*!< Call Forward on No-Answer Number */
			} v3;
#pragma pack(push)
#pragma pack(1)
			struct {
				uint32_t lel_activeForward;							/*!< Status (0=inactive, 1=active, 4=cm ??) */
				uint32_t lel_lineNumber;							/*!< Line Number */
				uint32_t lel_forwardAllActive;							/*!< Call Forward All Status */
				char cfwdallnumber[StationDynamicDirnumSize];					/*!< Call Forward All Number */
				uint32_t lel_forwardBusyActive;							/*!< Call Forward on Busy Status */
				char cfwdbusynumber[StationDynamicDirnumSize];					/*!< Call Forward on Busy Number */
				uint32_t lel_forwardNoAnswerActive;						/*!< Call Forward on No-Answer Status */
				char cfwdnoanswernumber[StationDynamicDirnumSize];				/*!< Call Forward on No-Answer Number */
			} v18;											
#pragma pack(pop)
		};
 	} ForwardStatMessage;											/*!< Forward Status Message Structure */

	struct {
		uint32_t lel_speedDialNumber;									/*!< SpeedDial Number */
		char speedDialDirNumber[StationMaxDirnumSize];							/*!< SpeedDial Dir Number */
		char speedDialDisplayName[StationMaxNameSize];							/*!< SpeedDial Display Name */
	} SpeedDialStatMessage;											/*!< SpeedDial Status Message Structure */

	struct {
		uint32_t lel_lineNumber;									/*!< Line Number */
		char lineDirNumber[StationMaxDirnumSize];							/*!< Line Dir Number */
		char lineFullyQualifiedDisplayName[StationMaxNameSize];						/*!< Line Fully Qualified Display Name */
		char lineDisplayName[StationMaxButtonTemplateNameSize];						/*!< Line Display Name */
		uint32_t lineDisplayOptions;
	} LineStatMessage;											/*!< Line Status Messages Structure */

	struct {
		uint32_t lel_year;										/*!< Year */
		uint32_t lel_month;										/*!< Month */
		uint32_t lel_dayOfWeek;										/*!< Day of the Week (\todo Starting su=0|1) */
		uint32_t lel_day;										/*!< Day */
		uint32_t lel_hour;										/*!< Hour */
		uint32_t lel_minute;										/*!< Minute */
		uint32_t lel_seconds;										/*!< Seconds */
		uint32_t lel_milliseconds;									/*!< MilliSeconds */
		uint32_t lel_systemTime;									/*!< System Time */
	} DefineTimeDate;											/*!< Definition of Date/Time Structure */

	struct {
		union {
			struct {
				uint32_t bel_ipAddr;
				uint32_t lel_sessionType;							/*!< SKINNY SESSION TYPE: 
															Chat:0x01, 
															Whiteboard:0x02, 
															ApplicationSharing: 0x04, 
															FileTransfer:0x08, 
															Video:0x10 // 10 !??!
														*/
			} v3;
			struct {
				uint32_t lel_ipv46;
				char bel_ipAddr[16];
				uint32_t lel_sessionType;							/*!< SKINNY SESSION TYPE: Chat:0x01, Whiteboard:0x02, ApplicationSharing: 0x04, FileTransfer:0x08, Video:0x10 */
			} v16;
		};
	} StartSessionTransmission;										/*!< Start Session Transmission Structure */

	struct {
		union {
			struct {
				uint32_t bel_ipAddr;
				uint32_t lel_sessionType;							/*!< SKINNY SESSION TYPE: 
															Chat:0x01, 
															Whiteboard:0x02, 
															ApplicationSharing: 0x04, 
															FileTransfer:0x08, 
															Video:0x10 // 10 !??!
														*/
			} v3;
			struct {
				uint32_t lel_ipv46;
				char bel_ipAddr[16];
				uint32_t lel_sessionType;							/*!< SKINNY SESSION TYPE: Chat:0x01, Whiteboard:0x02, ApplicationSharing: 0x04, FileTransfer:0x08, Video:0x10 */
			} v16;
		};
	} StopSessionTransmission;										/*!< Stop Session Transmission Structure */

	struct {
		uint32_t lel_buttonOffset;									/*!< Button OffSet */
		uint32_t lel_buttonCount;									/*!< Button Count */
		uint32_t lel_totalButtonCount;									/*!< Total Number of Buttons */
		StationButtonDefinition definition[StationMaxButtonTemplateSize];				/*!< Station Button Definition */
	} ButtonTemplateMessage;										/*!< Button Template Message Structure */

	struct {
		uint32_t lel_buttonOffset;									/*!< Button OffSet */
		uint32_t lel_buttonCount;									/*!< Button Count */
		uint32_t lel_totalButtonCount;									/*!< Total Number of Buttons */
		uint32_t dummy;											/*!< Station Button Definition */
	} ButtonTemplateMessageDynamic;										/*!< Identical to ButtonTemplateMessage */

	struct {
		char requiredVersion[StationMaxVersionSize];							/*!< Required Version */
	} VersionMessage;											/*!< Version Message Structure */

	struct {
		char displayMessage[StationMaxDisplayTextSize];							/*!< Display Message */
		/*              uint32_t        lel_displayTimeout; *//*!< Display Timeout */
	} DisplayTextMessage;											/*!< Display Text Message */

	struct {
		uint32_t unknown;
	} ClearDisplay;

	struct {
	} CapabilitiesReqMessage;										/*!< Capabilities Reqest Message Structure */

	struct {
	} EnunciatorCommandMessage;										/*!< Enunciator Command Message Structure */

	struct {
		char text[StationMaxDisplayTextSize];								/*!< Rejection Text */
	} RegisterRejectMessage;										/*!< Register Reject Message Structure */

	struct {
		union {
			struct {
				ServerIdentifier server[StationMaxServers];							/*!< Server Identifier */
				uint32_t serverListenPort[StationMaxServers];							/*!< Server is Listening on Port */
				uint32_t serverIpAddr[StationMaxServers];							/*!< Server IP Port */
			} v3;
			struct {
				ServerIdentifier server[StationMaxServers];							/*!< Server Identifier */
				uint32_t serverListenPort[StationMaxServers];							/*!< Server is Listening on Port */
				struct {
					uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
					char bel_ipAddr[16];								/*!< Multicast IP Address */
				} serverIpAddr[StationMaxServers];
			} v17;			
		};
	} ServerResMessage;											/*!< Server Result Message Structure */

	struct {
		uint32_t lel_resetType;										/*!< Reset Type (1=Reset, 2=Restart) */
	} Reset;												/*!< Reset Message Structure */

	struct {
	} KeepAliveAckMessage;											/*!< Keep Aliver Acknowledgement Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyID;							/*!< Pass Through Party ID */
				uint32_t bel_ipAddr;								/*!< Multicast IP Address */
				uint32_t lel_Port;								/*!< Multicast Port */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				skinny_codec_t lel_payloadCapability;						/*!< PayLoad Capability */
				uint32_t lel_echoCancelType;							/*!< Echo Cancelation Type */
				uint32_t lel_g723BitRate;							/*!< G.723 Bit Rate (only applies to G.723 */
			} v3;
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyID;							/*!< Pass Through Party ID */
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_ipAddr[16];								/*!< Multicast IP Address */
				uint32_t lel_Port;								/*!< Multicast Port */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				skinny_codec_t lel_payloadCapability;						/*!< PayLoad Capability */
				uint32_t lel_echoCancelType;							/*!< Echo Cancelation Type */
				uint32_t lel_g723BitRate;							/*!< G.723 Bit Rate (only applies to G.723 */
			} v16;
		};
	} StartMulticastMediaReception;										/*!< Start Multicast Media Reception Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyID;							/*!< Pass Through Party ID */
				uint32_t bel_ipAddr;								/*!< Multicast IP Address */
				uint32_t lel_Port;								/*!< Multicast Port */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				skinny_codec_t lel_payloadCapability;						/*!< PayLoad Capability */
				uint32_t lel_precedenceValue;							/*!< Precedence Value */
				uint32_t lel_silenceSuppression;						/*!< Silence Suppression */
				uint32_t lel_maxFramesPerPacket;						/*!< Max Frames Per Packet */
				uint32_t lel_g723BitRate;							/*!< G.723 Bit Rate (only applies to G.723 */
			} v3;
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyID;							/*!< Pass Through Party ID */
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_ipAddr[16];								/*!< Multicast IP Address */
				uint32_t lel_Port;								/*!< Multicast Port */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				skinny_codec_t lel_payloadCapability;						/*!< PayLoad Capability */
				uint32_t lel_precedenceValue;							/*!< Precedence Value */
				uint32_t lel_silenceSuppression;						/*!< Silence Suppression */
				uint32_t lel_maxFramesPerPacket;						/*!< Max Frames Per Packet */
				uint32_t lel_g723BitRate;							/*!< G.723 Bit Rate (only applies to G.723 */
			} v16;
		};
	} StartMulticastMediaTransmission;									/*!< Start Multicast Media Transmission Message Structure */

	/*
	 * based on miscCommandType we have other struct
	 * following the Miscellaneous Command Message
	 */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_miscCommandType;									/*!< Miscellaneous Command Type 
															videoFreezePicture: 		0x00
															videoFastUpdatePicture:		0x01
															videoFastUpdateGOB: 		0x02
															videoFastUpdateMB: 		0x03
															lostPicture: 			0x04
															lostPartialPicture: 		0x05
															recoveryReferencePicture:	0x06
															temporalSpatialTradeOff:	0x07
														*/
		union {
			uint32_t lel_none;
			
			struct {										/* this one does not seem to exist */
				uint32_t bel_remoteIpAddr;
				uint32_t unknown1;
				uint32_t unknown2;
				uint32_t unknown3;
				uint32_t unknown4;
				uint32_t lel_value1;
				uint32_t lel_value2;
				uint32_t lel_value3;
				uint32_t lel_value4;
			} videoFastUpdatePicture;

			struct {
				uint32_t lel_firstGOB;
				uint32_t lel_numberOfGOBs;
			} videoFastUpdateGOB;

			struct {
				uint32_t lel_firstGOB;
				uint32_t lel_firstMB;
				uint32_t lel_numberOfMBs;
			} videoFastUpdateMB;

			struct {
				uint32_t lel_pictureNumber;
				uint32_t lel_longTermPictureIndex;
			} lostPicture;

			struct {
				struct {
					uint32_t lel_pictureNumber;
					uint32_t lel_longTermPictureIndex;
				} pictureReference;
				uint32_t lel_firstMB;
				uint32_t lel_numberOfMBs;
			} lostPartialPicture;
			
			struct {
				uint32_t lel_PictureCount;
				struct {
					uint32_t lel_pictureNumber;
					uint32_t lel_longTermPictureIndex;
				} pictureReference[4];
			} recoveryReferencePicture;
			
			uint32_t lel_temporalSpatialTradeOff;
		} data;
	} MiscellaneousCommandMessage;										/*!< Miscellaneous Command Message Structure */

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_passThruPartyID;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;
	} StopMulticastMediaReception;										/*!< Stop Multicast Media Reception Message Structure */

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_passThruPartyID;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;
	} StopMulticastMediaTransmission;									/*!< Stop Multicast Media Transmission Message Structure */

	struct {
		union {
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
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				uint32_t lel_payloadType;							/*!< Media_Payload Type */
				uint32_t lel_vadValue;								/*!< Voice Activity Detection Value */
				uint32_t lel_g723BitRate;							/*!< G.723 Payload (Only applies to G.723) */
				/* protocol version 5 fields */
				uint32_t lel_callReference;							/*!< Conference ID */
				EncryptionInfo encryptioninfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMF PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Timeout (this is always 0x0A) */
				/* protocol version 15 fields */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
				uint32_t bel_remoteIpAddr;							/*!< Ipv4 Address */
				uint32_t lel_remotePortNumber;							/*!< this is always 0xFA0 */
			} v3;

			/*      OpenReceiveChannel v17
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
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				uint32_t lel_payloadType;							/*!< Media_Payload Type */
				uint32_t lel_vadValue;								/*!< Voice Activity Detection Value */
				uint32_t lel_g723BitRate;							/*!< G.723 Payload (Only applies to G.723) */
				/* protocol version 5 fields */
				uint32_t lel_callReference;							/*!< Conference ID */
				EncryptionInfo encryptioninfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMF PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Timeout (this is always 0x0A) */
				/* protocol version 15 fields */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_remoteIpAddr[16];
				uint32_t lel_remotePortNumber;							/*!< this is always 0xFA0 */
				uint32_t lel_requestedIpAddrType;						/*!< Unknown */
			} v17;

			struct {
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_millisecondPacketSize;						/*!< Millisecond Packet Size */
				uint32_t lel_payloadType;							/*!< Media_Payload Type */
				uint32_t lel_vadValue;								/*!< Voice Activity Detection Value */
				uint32_t lel_g723BitRate;							/*!< G.723 Payload (Only applies to G.723) */
				/* protocol version 5 fields */
				uint32_t lel_callReference;							/*!< Conference ID */
				EncryptionInfo encryptioninfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
				uint32_t lel_RFC2833Type;							/*!< RTP DTMF PayLoad (this is often set to 0x65 (101)) */
				uint32_t lel_dtmfType;								/*!< RTP Timeout (this is always 0x0A) */
				/* protocol version 15 fields */
				uint32_t lel_mixingMode;
				uint32_t lel_direction;
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_remoteIpAddr[16];
				uint32_t lel_remotePortNumber;							/*!< this is always 0xFA0 */
				uint32_t lel_requestedIpAddrType;						/*!< Unknown */
				uint32_t lel_audioLevelAdjustment;
				skinny_latentCapsInfo_t latentCapsInfo;
			} v22;
		};
	} OpenReceiveChannel;											/*!< Open Receive Channel Message Structure */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		/* version 5 fields */
		uint32_t lel_callReference;									/*!< Conference ID 1 */
		uint32_t lel_portHandlingFlag;									/*!< 0 = Close Port, 1 = Keep Port Open */
	} CloseReceiveChannel;											/*!< Close Receive Channel */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		/* version 5 fields */
		uint32_t lel_callReference;									/*!< Conference ID 1 */
		uint32_t lel_portHandlingFlag;									/*!< 0 = Close Port, 1 = Keep Port Open */
	} CloseMultiMediaReceiveChannel;

	struct {												// Request Statistics from Phone
		union {
			struct {
				char DirectoryNumber[StationMaxDirnumSize];					/*!< Directory Number */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_StatsProcessing;							/*!< Statistics Processing */
			} v3;
			struct {
				char DirectoryNumber[25];							/*!< Directory Number */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_StatsProcessing;							/*!< Statistics Processing */
			} v19;
		};
	} ConnectionStatisticsReq;										/*!< Connection Statistics Request Message Structure */

	struct {
		uint32_t lel_softKeyOffset;									/*!< Soft Key Off Set */
		uint32_t lel_softKeyCount;									/*!< Soft Key Count */
		uint32_t lel_totalSoftKeyCount;									/*!< Total Number of Soft Keys */
		StationSoftKeyDefinition definition[1];								/*!< Station Soft Key Definition (dummy) */
	} SoftKeyTemplateResMessage;										/*!< Soft Key Template Result Message Structure */

	struct {
		uint32_t lel_softKeySetOffset;									/*!< Soft Key Off Set */
		uint32_t lel_softKeySetCount;									/*!< Soft Key Count */
		uint32_t lel_totalSoftKeySetCount;								/*!< Total Number of Soft Keys */
		StationSoftKeySetDefinition definition[StationMaxSoftKeySetDefinition];				/*!< Station Soft Key Definition */
	} SoftKeySetResMessage;											/*!< Soft Key Set Result Message Structure */

	struct {
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_softKeySetIndex;									/*!< Soft Key Set Index */
		uint32_t les_validKeyMask;									/*!< Valid Key Mask */
	} SelectSoftKeysMessage;										/*!< Select Soft Keys Message Structure */

	struct {
		uint32_t lel_callState;										/*!< Call State: ENUM: skinny_callstate_ */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_visibility;									/*!< Visibility / Privacy: ENUM : skinny_callinfo_ */
		struct {
			uint32_t lel_level;									/*!< Level */
			uint32_t lel_domain;									/*!< Domain */
		} precedence;											/*!< Precedency / Priority */
	} CallStateMessage;											/*!< Call State Message Structure */

	struct {
		uint32_t lel_messageTimeout;									/*!< Message Timeout */
		char promptMessage[32];										/*!< Prompt Message (Max Lenght 32) */
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
	} DisplayPromptStatusMessage;										/*!< Display Prompt Status Message Structure */

	struct {
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
	} ClearPromptStatusMessage;										/*!< Clear Prompt Status Message Structure */

	struct {
		uint32_t lel_displayTimeout;									/*!< Display Timeout */
		char displayMessage[StationMaxDisplayNotifySize];						/*!< Display Message */
	} DisplayNotifyMessage;											/*!< Display Notify Message Structure */

	struct {
	} ClearNotifyMessage;											/*!< Clear Notify Message Structure */

	/* 0x11F FeatureStatMessage */
	struct {
		uint32_t lel_featureIndex;									/*!< Feature Instance */
		uint32_t lel_featureID;										/*!< Feature ID */
		char featureTextLabel[StationMaxNameSize];							/*!< Feature Text Label */
		uint32_t lel_featureStatus;									/*!< Feature Status */
	} FeatureStatMessage;											/*!< Feature Status Message Structure */

	struct {
		uint32_t lel_serviceURLIndex;									/*!< Service URL Index */
		char URL[StationMaxServiceURLSize];								/*!< Actual URL */
		char label[StationMaxNameSize];									/*!< Label */
	} ServiceURLStatMessage;										/*!< Service URL Stat Message Structure */

	struct {												// Used Above Protocol 7 */
		uint32_t lel_serviceURLIndex;									/*!< Service URL Index */
		uint32_t dummy;											/*!< Dummy */
	} ServiceURLStatDynamicMessage;										/*!< Service URL Stat Message Structure */

	struct {
		uint32_t lel_status;										/*!< Status */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_lineInstance;									/*!< Line Instance */
	} CallSelectStatMessage;										/*!< Call Select Status Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				skinny_codec_t lel_payloadCapability;						/*!< payload capability */
				uint32_t lel_lineInstance;							/*!< Line Instance */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_payload_rfc_number;						/*!<  */
				uint32_t lel_payloadType;							/*!< payload type */
				uint32_t lel_isConferenceCreator;						/*!< we can set it to 0 */

				videoParameter_t videoParameter;						/*!< Video Parameter */
			} v3;

			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				skinny_codec_t lel_payloadCapability;						/*!< payload capability */
				uint32_t lel_lineInstance;							/*!< Line Instance */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_payload_rfc_number;						/*!<  */
				uint32_t lel_payloadType;							/*!< payload type */
				uint32_t lel_isConferenceCreator;						/*!< we can set it to 0 */

				videoParameter_t videoParameter;						/*!< Video Parameter */
				uint32_t lel_dummy[16];
			} v17;
		};
	} OpenMultiMediaChannelMessage;										/*!< Open Multi Media Channel Message Structure */

	struct {
		union {
			/*!
			 * \since 20100104 -MC
			 * \note update 20100722
			 * 0000   ac 00 00 00 00 00 00 00 32 01 00 00 0b 00 00 01
			 0010   82 00 00 01 67 00 00 00 ac 11 01 66 45 15 00 00
			 0020   0b 00 00 01 00 00 00 00 61 00 00 00 88 00 00 00
			 0030   00 0f 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			 0040   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			 0050   00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
			 0060   00 00 00 00 40 00 00 00 32 00 00 00 5b 52 3a 4c
			 0070   50 20 2d 20 48 50 3a 20 30 2c 20 4e f8 15 24 00
			 0080   c4 02 89 09 9c ee 5a 0a 88 06 18 00 54 ef 5a 0a
			 0090   64 7e fb 77 58 31 f8 77 ff ff ff ff 64 ef 5a 0a
			 00a0   c2 b7 fc 77 78 07 18 00 20 16 24 00 40 16 24 00
			 00b0   20 16 24 00
			 * 
			 * 
			 */
			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				skinny_codec_t lel_payloadCapability;						/*!< payload capability */

				uint32_t bel_remoteIpAddr;							/*!< This field is apparently in big-endian
														   format, even though most other fields are
														   little-endian. */
				uint32_t lel_remotePortNumber;							/*!< Remote Port Number */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_payload_rfc_number;						/*!< Payload RFC Number */
				uint32_t lel_payloadType;							/*!< payload type */
				uint32_t lel_DSCPValue;								/*!< DSCP Value */

				videoParameter_t videoParameter;						/*!< Video Parameter */
			} v3;

			struct {
				uint32_t lel_conferenceID;							/*!< Conference ID */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				skinny_codec_t lel_payloadCapability;						/*!< payload capability */
				uint32_t lel_ipv46;								/*!<  */

				char bel_remoteIpAddr[16];							/*!< This field is apparently in big-endian
														   format, even though most other fields are
														   little-endian. */
				uint32_t lel_remotePortNumber;							/*!< Remote Port Number */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_payload_rfc_number;						/*!< Payload RFC Number */
				uint32_t lel_payloadType;							/*!< payload type */
				uint32_t lel_DSCPValue;								/*!< DSCP Value */

				videoParameter_t videoParameter;						/*!< Data Parameter */
				EncryptionInfo encryptionInfo;
				uint32_t lel_streamPassThroughID;						/*!< Stream Pass-Through ID */
				uint32_t lel_assocStreamID;							/*!< Associated Stream ID */
			} v17;
		};
	} StartMultiMediaTransmission;										/*!< Start MultiMedia Transmission Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference1;							/*!< Call Reference1 */
				uint32_t bel_ipAddr;								/*!< This field is apparently in big-endian format, even though most other fields are little-endian. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_mediastatus;							/*!< Start Media Transmission Status */
				uint32_t lel_unknown;								/*!< Unknown (Does this actually Exist) */
			} v3;

			struct {
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through Party ID */
				uint32_t lel_callReference1;							/*!< Call Reference 1 */
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_ipAddr[16];								/*!< This field is apparently in big-endian format, even though most other fields are in little-endian format. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_mediastatus;							/*!< startmediatransmission status */
				uint32_t lel_unknown;								/*!< Unknown (Does this actually Exist)*/
			} v17;
		};
	} StartMultiMediaTransmissionAck;									/*!< Start Media Transmission Acknowledgement Structure */

	struct {
		uint32_t lel_displayTimeout;									/*!< Display Timeout */
		uint32_t lel_priority;										/*!< Priority */
		char displayMessage[StationMaxDisplayNotifySize];						/*!< Display Message */
	} DisplayPriNotifyMessage;										/*!< Display Priority Notify Message Structure */

	struct {
		uint32_t lel_priority;										/*!< Priority */
	} ClearPriNotifyMessage;										/*!< Clear Priority Notify Message Structure */

	struct {
		uint32_t lel_lineInstance;									/*!< Line Instance */
	} ActivateCallPlaneMessage;										/*!< Activate Call Plane Message Structure */

	struct {
	} DeactivateCallPlaneMessage;										/*!< Deactivate Call Plane Message Structure */

	struct {
		uint32_t lel_status;										/*!< Status */
	} UnregisterAckMessage;											/*!< Unregister Ackknowledge Message Structure */

	struct {
		uint32_t lel_lineInstance;									/*!< Line Instance */
		uint32_t lel_callReference;									/*!< Call Reference */
	} BackSpaceResMessage;											/*!< Back Space Request Message Message Structure */

	struct {
	} RegisterTokenAck;											/*!< Register Token Ackknowledge Message Structure */

	struct {
		uint32_t lel_tokenRejWaitTime;									/*!< Back Off Time */
	} RegisterTokenReject;											/*!< Register Token Reject Message Structure */

	/*!
	 * 
	 * \since 20100103 -MC
	 */
	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_passThruPartyId;									/*!< Pass Through Party ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t maxBitRate;										/*!< Maximum BitRate */
	} FlowControlCommandMessage;

	/* SCCP Firmware version > 9.1 */
	struct {
		char le_data[StationMaxXMLAlarmMessage];							/*!< XML Alarm Message Data */
	} XMLAlarmMessage;											/*!< XML Alarm Message Structure */

	struct {
		uint32_t lel_unknown;
	} CallCountReqMessage;											/*!< CallCount Request Message Structure (@see https://sourceforge.net/p/chan-sccp-b/bugs/173/?page=1) */

	/* SPA */
	struct {
		StationIdentifier sId;										/*!< Station Identifier */
		uint32_t lel_stationIpAddr;									/*!< Station IP Address */
		uint32_t lel_deviceType;									/*!< Device Type as part of SKINNY_DEVICETYPE_* */
		uint32_t maxStreams;										/*!< Max Streams */
	} SPCPRegisterTokenRequest;

	struct {
		uint32_t lel_features;
	} SPCPRegisterTokenAck;

	struct {
		uint32_t lel_features;
	} SPCPRegisterTokenReject;

	/*
	 * Unhandled SCCP Message: unknown(0x0159) 168 bytes length
	 00000000 - A8 00 00 00 00 00 00 00  59 01 00 00 01 00 00 00  - ........Y.......
	 00000010 - 01 00 00 00 02 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000020 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000030 - 6D 61 78 2D 63 68 61 69  6E 65 64 3D 32 20 64 65  - max-chained=2 de
	 00000040 - 76 69 63 65 4C 69 6E 65  3D 31 34 00 00 00 00 00  - viceLine=14.....
	 00000050 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000060 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000070 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000080 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 00000090 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 000000A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................	 
	 *
	 *      This was sent by a single 7970 with one CP-7914 attached.
	 *      Message is related to Addons and i suppose it notifies their
	 *      number, capacity and index.
	 */
	struct {
		uint32_t unknown1;
		uint32_t unknown2;
		uint32_t unknown3;
		char dummy[152];
	} ExtensionDeviceCaps;

	struct {
		uint32_t lel_disposition;									/*!< Call History Disposition Enum */
		uint32_t lel_lineInstance;									
		uint32_t lel_callReference;
	} CallHistoryDispositionMessage;

	/*
	 * Fixed Size XML Message
	 * 00000000 - 68 09 00 00 00 00 00 00  57 01 00 00 3C 49 6E 74  - h.......W...<Int
	 * 00000010 - 65 72 66 61 63 65 31 3E  3C 77 69 66 69 3E 3C 42  - erface1><wifi><B
	 * 00000020 - 53 53 49 44 3E 45 38 3A  45 44 3A 46 33 3A 31 30  - SSID>E8:ED:F3:10
	 * 00000030 - 3A 32 39 3A 46 44 3C 2F  42 53 53 49 44 3E 3C 53  - :29:FD</BSSID><S
	 * 00000040 - 53 49 44 3E 77 70 61 5F  70 68 79 3C 2F 53 53 49  - SID>wpa_phy</SSI
	 * 00000050 - 44 3E 3C 41 50 4E 61 6D  65 3E 4D 6F 73 5F 41 50  - D><APName>Mos_AP
	 * 00000060 - 33 5F 37 63 36 39 2E 66  36 3C 2F 41 50 4E 61 6D  - 3_7c69.f6</APNam
	 * 00000070 - 65 3E 3C 2F 77 69 66 69  3E 3C 4F 66 66 50 72 65  - e></wifi><OffPre
	 * 00000080 - 6D 3E 3C 2F 4F 66 66 50  72 65 6D 3E 3C 2F 49 6E  - m></OffPrem></In
	 * 00000090 - 74 65 72 66 61 63 65 31  3E 00 00 00 00 00 00 00  - terface1>.......
	 * 000000A0 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 * ...
	 * 00000960 - 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  - ................
	 */
	struct {
		char xmldata[2404];
	} LocationInfoMessage;

	// empty / unresearched structs
	// 00000000 - 08 00 00 00 00 00 00 00  2D 00 00 00 00 00 00 00  // 7960 -- 6 buttons
	// 00000000 - 08 00 00 00 16 00 00 00  2D 00 00 00 02 00 00 00  // 7962 -- 6 buttons
	// 00000000 - 08 00 00 00 16 00 00 00  2D 00 00 00 02 00 00 00  // 7970 -- 8 buttons
	struct {
		uint32_t maxAvailLines;
	} RegisterAvailableLinesMessage;

	struct {
	} StartMediaFailureDetection;

	struct {
	} NotifyDtmfToneMessage;

	struct {
	} SendDtmfToneMessage;

	struct {
	} SubscribeDtmfPayloadReqMessage;

	struct {
	} SubscribeDtmfPayloadResMessage;

	struct {
	} SubscribeDtmfPayloadErrMessage;

	struct {
	} UnSubscribeDtmfPayloadReqMessage;

	struct {
	} UnSubscribeDtmfPayloadResMessage;

	struct {
	} UnSubscribeDtmfPayloadErrMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_numberOfReservedParticipants;							/*!< Number of Reserved Participants */
		uint32_t lel_resourceType;									/*!< Resource Type: Conf:0x00, IVR:0x01 */
		uint32_t lel_appID;										/*!< Application ID */
		char appConfID[32];										/*!< Application ConfID */
		char lel_appData[24];										/*!< Application Data */
		uint32_t lel_dataLength;									/*!< Application DataLength */
		char lel_passThruData[];									/*!< Pass Through Data (variable size / max 2000)*/
	} CreateConferenceReqMessage;										/*!< Create Conference Request Message Structure */

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_result;										/*!< Result ENUM(OK:0x00, ResourceNotAvailable: 0x01, ConferenceAlreadyExists: 0x02, SystemErr:0x03) */
		uint32_t lel_dataLength;									/*!< Application DataLength */
		char lel_passThruData[];									/*!< Pass Through Data (variable size / max 2000)*/
	} CreateConferenceResMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
	} DeleteConferenceReqMessage;										/*!< Delete Conference Request Message Structure */

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_result;										/*!< Result ENUM(OK:0x00, ConferenceDoesNotExist: 0x01, SystemErr: 0x02)*/
	} DeleteConferenceResMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_numberOfReservedParticipants;							/*!< Number of Participants to be reserved */
		uint32_t lel_appID;										/*!< App ID */
		char appConfID[StationMaxDisplayTextSize];							/*!< app Conf ID */
		char appData[StationMaxDirnumSize];								/*!< Add Data */
		uint32_t lel_dataLength;									/*!< Application DataLength */
		char lel_passThruData[];									/*!< Pass Through Data (variable size / max 2000)*/
	} ModifyConferenceReqMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_result;										/*!< Result ENUM(OK:0x00, ResourceNotAvailable: 0x01, ConferenceDoesNotExist: 0x02, InvalidParameter: 0x03, MoreActiveCallsThanReserved: 0x04, InvalidResourceType:0x05, SystemErr:0x06) */
		uint32_t lel_dataLength;									/*!< Application DataLength */
		char lel_passThruData[];									/*!< Pass Through Data (variable size / max 2000)*/
	} ModifyConferenceResMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t partyPIRestrictionBits;								/*!< Party PI Restriction Bits Structure
														   0 RestrictCallingPartyName
														   1 RestrictCallingPartyNumber
														   2 RestrictCalledPartyName
														   3 RestrictCalledPartyNumber
														   4 RestrictOriginalCalledPartyName
														   5 RestrictOriginalCalledPartyNumber
														   6 RestrictLastRedirectPartyName
														   7 RestrictLastRedirectPartyNumber */
		char participantName[StationMaxNameSize];							/*!< Participant Name */
		char participantNumber[StationMaxDirnumSize];							/*!< Participant Number */
		char conferenceName[StationMaxDisplayTextSize];							/*!< Conference Name*/
	} AddParticipantReqMessage;										/*!< Add Participent to Conference Request Message*/

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_result;										/*!< Result ENUM(OK:0x00, ResourceNotAvailable: 0x01, ConferenceDoesNotExist: 0x02, DeplicateCallReference:0x03, SystemErr:0x04) */
		uint32_t lel_bridgeParticipantID;								/*!< Birdge Participant ID */
	} AddParticipantResMessage;

	struct {
	} AuditConferenceReqMessage;

	struct {
		uint32_t lel_last;										/*!< last */
		uint32_t lel_numberOfEntries;									/*!< Number Of Entries */
		struct {
			uint32_t lel_conferenceID;								/*!< Conference ID */
			uint32_t lel_resourceType;								/*!< Resource Type. Enum(Conf:0x00, IVR:0x01) */
			uint32_t lel_numberOfReservedParticipants;						/*!< Number of Reserved Participants */
			uint32_t lel_numberOfActiveParticipants;						/*!< Number of Active Participants */
			uint32_t lel_appID;									/*!< App ID */
			char appConfID[StationMaxDisplayTextSize];						/*!< app Conf ID */
			char appData[StationMaxDirnumSize];							/*!< Add Data */
		} conferenceEntry[];										/*!< variable size / max 32*/
	} AuditConferenceResMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
	} AuditParticipantReqMessage;

	struct {
		uint32_t lel_result;										/*!< Result. Enum(OK:0x00, ConferenceDoesNotExist:0x01) */
		uint32_t lel_last;										/*!< last */
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_numberOfEntries;									/*!< Number Of Entries */
		char participantEntry[];									/*!< variable size: max 256 */
	} AuditParticipantResMessage;

	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t partyPIRestrictionBits;								/*!< Party PI Restriction Bits Structure
														   0 RestrictCallingPartyName
														   1 RestrictCallingPartyNumber
														   2 RestrictCalledPartyName
														   3 RestrictCalledPartyNumber
														   4 RestrictOriginalCalledPartyName
														   5 RestrictOriginalCalledPartyNumber
														   6 RestrictLastRedirectPartyName
														   7 RestrictLastRedirectPartyNumber */
		char participantName[StationMaxNameSize];							/*!< Participant Name */
		char participantNumber[StationMaxDirnumSize];							/*!< Participant Number */
		char conferenceName[StationMaxDisplayTextSize];							/*!< Conference Name*/
	} ChangeParticipantReqMessage;
	
	struct {
		uint32_t lel_conferenceID;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
	} DropParticipantReqMessage;

	struct {
		skinny_announcementList_t announcements[SKINNY_MaxAnnouncementList];
		uint32_t lel_endOfAnnAck;
		uint32_t lel_conferenceID;
		uint32_t lel_matrixConfPartyID[SKINNY_StationMaxMonitorParties];
		uint32_t lel_hearingConfPartyMask;
		uint32_t lel_annPlayMode;
	} StartAnnouncementMessage;

	struct {
		uint32_t lel_conferenceID;
	} StopAnnouncementMessage;

	struct {
		uint32_t lel_conferenceID;
		uint32_t lel_annPlayStatus;
	} AnnouncementFinishMessage;

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_passThruPartyId;									/*!< Pass Through ID*/
		uint32_t lel_mediaTransportType;								/*!< Skinny mediaTransportType Enum */
		uint32_t lel_ipv46;										/*!< ipv4 / ipv6 */
		uint32_t lel_mediaType;										/*!< Skinny mediaType Enum */
	} PortRequestMessage;											/*!< Port Request Message Structure */

	struct {
		union {
			struct {
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through ID*/
				uint32_t bel_ipAddr;								/*!< Ip Address Array (This field is apparently in big-endian format, even though most other fields are in little-endian format.) */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_RTCPPortNumber;							/*!< RTCP Port Number */
			} v3;
			struct {
				uint32_t lel_conferenceId;							/*!< Conference ID */
				uint32_t lel_callReference;							/*!< Call Reference */
				uint32_t lel_passThruPartyId;							/*!< Pass Through ID*/
				uint32_t lel_ipv46;								/*!< ipv4 / ipv6 */
				char bel_ipAddr[16];								/*!< This field is apparently in big-endian format, even though most other fields are in little-endian format. */
				uint32_t lel_portNumber;							/*!< Port Number */
				uint32_t lel_RTCPPortNumber;							/*!< RTCP Port Number */
				uint32_t lel_mediaType;								/*!< Skinny Media Type (Enum) */
			} v19;
		};
	} PortResponseMessage;											/*!< Port Response Message Structure */

	struct {
		uint32_t lel_conferenceId;									/*!< Conference ID */
		uint32_t lel_callReference;									/*!< Call Reference */
		uint32_t lel_passThruPartyId;									/*!< Pass Through ID*/
		uint32_t lel_mediaType;										/*!< Skinny Media Type (Enum) */
	} PortCloseMessage;											/*!< Port Close Message Structure */

	struct {
	} QoSResvNotifyMessage;											/*!< QoS Resv Notify Message Structure */

	struct {
	} QoSErrorNotifyMessage;										/*!< QoS Error Notify Message Structure */


	struct {
	} QoSListenMessage;											/*!< QoS Listen Message Structure */

	struct {
	} QoSPathMessage;											/*!< QoS Path Message Structure */

	struct {
	} QoSTeardownMessage;											/*!< QoS Teardown Message Structure */

	struct {
	} UpdateDSCPMessage;											/*!< Update DSCP Message Structure */

	struct {
	} QoSModifyMessage;											/*!< QoS Modify Message Structure */

	struct {
	} MwiResponseMessage;											/*!< Mwi Response Message Structure */

	struct {
	} CallCountRespMessage;											/*!< CallCount Response Message Structure */

	struct {
		uint32_t lel_callReference;
		uint32_t lel_status;
	} RecordingStatusMessage;										/*!< Recording Status Message Structure */

	//struct {
	//} UnknownVGMessage;
} sccp_data_t;													/*!< SKINNY Data Structure */

/*!
 * \brief SKINNY Message Header Structure
 */
struct sccp_header {
	uint32_t length;											/*!< Message Length */
	uint32_t lel_protocolVer;										/*!< Protocol Version Message */
	uint32_t lel_messageId;											/*!< Message ID, the messageId is not part of the skinny header, so it is counted in length */
};

/*!
 * \brief SKINNY Message Structure
 */
struct sccp_msg {
	sccp_header_t header;											/*!< Message Header */
	sccp_data_t data;											/*!< Message [SCCP Data] */
};

//#define messagePtr sccp_msg_t * const
//#define constMessagePtr const sccp_msg_t * const

/* So in theory, a message should never be bigger than this.
 * If it is, we abort the connection */
#define SCCP_MAX_PACKET sizeof(sccp_msg_t)
#define SCCP_PACKET_HEADER sizeof(sccp_header_t)

/*!
 * \brief SCCP Message Type Structure
 */
#define offsize(TYPE, MEMBER) sizeof(((TYPE *)0)->MEMBER)
struct messagetype {
	sccp_mid_t messageId;	
	const char *const text;
	const size_t size;
};

extern const struct messagetype sccp_messagetypes[];
extern const struct messagetype spcp_messagetypes[];
SCCP_INLINE const char * SCCP_CALL msgtype2str(sccp_mid_t msgId);

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
	SKINNY_LBL_MONITOR,
	SKINNY_LBL_CALLBACK,
	SKINNY_LBL_BARGE,
	SKINNY_LBL_DND,
	SKINNY_LBL_CONFLIST,
	SKINNY_LBL_SELECT,
	SKINNY_LBL_PRIVATE,
	SKINNY_LBL_TRNSFVM,
	SKINNY_LBL_DIRTRFR,
	SKINNY_LBL_IDIVERT,
	SKINNY_LBL_VIDEO_MODE,
	SKINNY_LBL_INTRCPT,
	SKINNY_LBL_EMPTY,
	SKINNY_LBL_DIAL,
	//SKINNY_LBL_CBARGE,
};														/*!< SKINNY Soft Keys Map as INT */


/*!
 * \brief SKINNY Soft Key Modes Structure
 */
typedef struct {
	uint8_t *ptr;												/*!< Point to next Mode */
	uint8_t id;												/*!< Soft Key ID */
	uint8_t count;												/*!< Soft Key Count */
} softkey_modes;												/*!< SKINNY Soft Key Modes Structure */

/*!
 * \brief SCCP Device Protocol Callback Structure
 *
 * Connect Specific CallBack-Functions to Particular SCCP Protocol Versions
 */
typedef struct {
	//const char *name;											/*! protocol name ( SCCP | SPCP ) */
	const uint16_t type;											/*! (SCCP_PROTOCOL | SPCP_PROTOCOL) */
	const uint8_t version;											/*! the protocol version number */
	const uint16_t registrationFinishedMessageId;								/*! use this message id to determine that the device is fully registered */

	/* protocol callbacks */
	/* send messages */
	void (*const sendCallInfo) (const sccp_callinfo_t * const ci, const uint32_t callid, const skinny_calltype_t calltype, const uint8_t lineInstance, const uint8_t callInstance, constDevicePtr device);
	void (*const sendDialedNumber) (constDevicePtr device, const uint8_t lineInstance, const uint32_t callid, const char dialedNumber[SCCP_MAX_EXTENSION]);
	void (*const sendRegisterAck) (constDevicePtr device, uint8_t keepAliveInterval, uint8_t secondaryKeepAlive, char *dateformat);
	void (*const displayPrompt) (constDevicePtr device, uint8_t lineInstance, uint32_t callid, uint8_t timeout, const char *message);
	void (*const displayNotify) (constDevicePtr device, uint8_t timeout, const char *message);
	void (*const displayPriNotify) (constDevicePtr device, uint8_t priority, uint8_t timeout, const char *message);
	void (*const sendCallforwardMessage) (constDevicePtr device, const sccp_linedevices_t * linedevice);
	void (*const sendUserToDeviceDataVersionMessage) (constDevicePtr device, uint32_t appID, uint32_t lineInstance, uint32_t callReference, uint32_t transactionID, const void *xmlData, uint8_t priority);
	void (*const sendFastPictureUpdate) (constDevicePtr device, constChannelPtr channel);
	void (*const sendOpenReceiveChannel) (constDevicePtr device, constChannelPtr channel);
	void (*const sendOpenMultiMediaChannel) (constDevicePtr device, constChannelPtr channel, uint32_t skinnyFormat, int payloadType, uint8_t linInstance, int bitrate);
	void (*const sendStartMultiMediaTransmission) (constDevicePtr device, constChannelPtr channel, int payloadType, int bitRate);
	void (*const sendStartMediaTransmission) (constDevicePtr device, constChannelPtr channel);
	void (*const sendConnectionStatisticsReq) (constDevicePtr device, constChannelPtr channel, uint8_t clear);
	void (*const sendPortRequest) (constDevicePtr device, constChannelPtr channel, skinny_mediaTransportType_t mediaTransportType, skinny_mediaType_t mediaType);
	void (*const sendPortClose) (constDevicePtr device, constChannelPtr channel, skinny_mediaType_t mediaType);

	/* parse received messages */
	void (*const parseOpenReceiveChannelAck) (constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage * ss, uint32_t * passthrupartyid, uint32_t * callReference);
	void (*const parseOpenMultiMediaReceiveChannelAck) (constMessagePtr msg, skinny_mediastatus_t * mediastatus, struct sockaddr_storage * ss, uint32_t * passthrupartyid, uint32_t * callReference);
	void (*const parseStartMediaTransmissionAck) (constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage * ss);
	void (*const parseStartMultiMediaTransmissionAck) (constMessagePtr msg, uint32_t * partyID, uint32_t * callID, uint32_t * callID1, skinny_mediastatus_t * mediastatus, struct sockaddr_storage * ss);
	void (*const parseEnblocCall) (constMessagePtr msg, char *calledParty, uint32_t * lineInstance);
	void (*const parsePortResponse) (constMessagePtr msg, uint32_t *conferenceId, uint32_t *callReference, uint32_t *passThruPartyId, struct sockaddr_storage *ss, uint32_t * RTCPPortNumber, skinny_mediaType_t *mediaType);
} sccp_deviceProtocol_t;											/*!< SCCP Device Protocol Callback Structure */

SCCP_API boolean_t SCCP_CALL sccp_protocol_isProtocolSupported(uint8_t type, uint8_t version);
SCCP_API uint8_t SCCP_CALL sccp_protocol_getMaxSupportedVersionNumber(int type);
SCCP_API const sccp_deviceProtocol_t * SCCP_CALL sccp_protocol_getDeviceProtocol(constDevicePtr device, int type);
SCCP_API const char * SCCP_CALL skinny_keymode2longstr(skinny_keymode_t keymode);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
