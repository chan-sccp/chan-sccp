/*!
 * \file        chan_sccp_enums.hh
 * \brief       SCCP Enums
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-04-04 12:59:26 +0200 (Thu, 04 Apr 2013) $
 * $Revision: 4514 $
 */
///////////////////////////////
// The enum examples
///////////////////////////////

//
// BEGIN_ENUM(sccp,Months)		/* enum2str with index value (starting at 2) */
//        ENUM_ELEMENT(january,=2,"jan")
//        ENUM_ELEMENT(february,,"feb")
//        ENUM_ELEMENT(march,,"mrch")
// END_ENUM(sccp,Months)
//
// /* will generate this in sccp_enum.h */
// typedef enum tag_Months {
//        january =2,
//        february ,
//        march ,
// } sccp_Months_t; 
//
// inline const char* Months2str(sccp_Months_t index);
//
// /* and this in sccp_enum.c */
// inline const char* Months2str(sccp_Months_t index) { 
// 	switch(index) { 
//         	case january: return "jan";
//         	case february: return "feb";
//         	case march: return "mrch";
// 	};
//	return "SCCP: ERROR lookup in " "skinny" "_" "Months" "_t"; };
// };

//
// Simple verification method
//
// cpp -I. chan_sccp.h > chan_sccp.h
// cpp -I. chan_sccp.c > chan_sccp.c

///////////////////////////////
// The sccp enum declarations
///////////////////////////////

/*! 
 * \brief internal chan_sccp call state (c->callstate) (Enum)
 */
BEGIN_ENUM(sccp,channelstate)
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DOWN 				,=0, 	"DOWN")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_OFFHOOK 				,=1, 	"OFFHOOK")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_ONHOOK 				,=2, 	"ONHOOK")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_RINGOUT 				,=3,	"RINGOUT")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_RINGING 				,=4,	"RINGING")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONNECTED 			,=5,	"CONNECTED")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_BUSY 				,=6,	"BUSY   ")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONGESTION 			,=7,	"CONGESTION")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_HOLD 				,=8,	"HOLD   ")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLWAITING 			,=9,	"CALLWAITING")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLTRANSFER 			,=10,	"CALLTRANSFER")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLPARK 			,=11,	"CALLPARK")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_PROCEED 				,=12,	"PROCEED")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLREMOTEMULTILINE 		,=13,	"CALLREMOTEMULTILINE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_INVALIDNUMBER 			,=14,	"INVALIDNUMBER")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DIALING 				,=20,	"DIALING")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_PROGRESS 			,=21,	"PROGRESS")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_GETDIGITS 			,=0xA0,	"GETDIGITS")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLCONFERENCE 			,=0xA1,	"CALLCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_SPEEDDIAL 			,=0xA2,	"SPEEDDIAL")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DIGITSFOLL 			,=0xA3,	"DIGITSFOLL")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_INVALIDCONFERENCE 		,=0xA4,	"INVALIDCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONNECTEDCONFERENCE 		,=0xA5,	"CONNECTEDCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_BLINDTRANSFER 			,=0xA6,	"BLINDTRANSFER")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_ZOMBIE 				,=0xFE,	"ZOMBIE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DND 				,=0xFF,	"DND")
END_ENUM(sccp,channelstate)												/*!< internal Chan_SCCP Call State c->callstate */

/*!
 * \brief Skinny Miscellaneous Command Type (Enum)
 */
BEGIN_ENUM(sccp,miscCommandType)
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE		,,	"videoFreezePicture") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE	,,	"videoFastUpdatePicture") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEGOB		,,	"videoFastUpdateGOB") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEMB		,,	"videoFastUpdateMB") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_LOSTPICTURE			,,	"lostPicture") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_LOSTPARTIALPICTURE		,,	"lostPartialPicture") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_RECOVERYREFERENCEPICTURE	,,	"recoveryReferencePicture") 
        ENUM_ELEMENT(SKINNY_MISCCOMMANDTYPE_TEMPORALSPATIALTRADEOFF	,,	"temporalSpatialTradeOff") 
END_ENUM(sccp,miscCommandType)

BEGIN_ENUM(sccp,devicestate)
        ENUM_ELEMENT(SCCP_DEVICESTATE_ONHOOK				,,	"On Hook"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_OFFHOOK				,,	"Off Hook"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_UNAVAILABLE			,,	"Unavailable"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_DND				,,	"Do Not Disturb")
        ENUM_ELEMENT(SCCP_DEVICESTATE_FWDALL				,,	"Forward All"	)
END_ENUM(sccp,devicestate)

BEGIN_ENUM(sccp,callforward)
        ENUM_ELEMENT(SCCP_CFWD_NONE		 			,,	"None")
        ENUM_ELEMENT(SCCP_CFWD_ALL 					,,	"All")
        ENUM_ELEMENT(SCCP_CFWD_BUSY 					,,	"Busy")
        ENUM_ELEMENT(SCCP_CFWD_NOANSWER 				,,	"NoAnswer")
END_ENUM(sccp,callforward)

/*!
 * \brief SCCP Dtmf Mode (ENUM)
 */
BEGIN_ENUM(sccp,dtmfmode)
        ENUM_ELEMENT(SCCP_DTMFMODE_INBAND				,,	"Dtmf Inband")
        ENUM_ELEMENT(SCCP_DTMFMODE_OUTOFBAND 				,,	"Dtmf OutOfBand")
END_ENUM(sccp,dtmfmode)

/*!
 * \brief SCCP Autoanswer (ENUM)
 */
BEGIN_ENUM(sccp,autoanswer)
        ENUM_ELEMENT(SCCP_AUTOANSWER_NONE				,,	"AutoAnswer None")
        ENUM_ELEMENT(SCCP_AUTOANSWER_1W 				,,	"AutoAnswer 1-Way")
        ENUM_ELEMENT(SCCP_AUTOANSWER_2W 				,,	"AutoAnswer Both Ways")
END_ENUM(sccp,autoanswer)

/*!
 * \brief SCCP DNDMode (ENUM)
 */
BEGIN_ENUM(sccp,dndmode)
        ENUM_ELEMENT(SCCP_DNDMODE_OFF					,,	"Off")
        ENUM_ELEMENT(SCCP_DNDMODE_REJECT				,,	"Reject")
        ENUM_ELEMENT(SCCP_DNDMODE_SILENT				,,	"Silent")
        ENUM_ELEMENT(SCCP_DNDMODE_USERDEFINED				,,	"UserDefined")
END_ENUM(sccp,dndmode)

BEGIN_ENUM(sccp,accessory)
        ENUM_ELEMENT(SCCP_ACCESSORY_NONE	 			,,	"None")
        ENUM_ELEMENT(SCCP_ACCESSORY_HEADSET	 			,,	"Headset")
        ENUM_ELEMENT(SCCP_ACCESSORY_HANDSET	 			,,	"Handset") 
        ENUM_ELEMENT(SCCP_ACCESSORY_SPEAKER	 			,,	"Speaker")
END_ENUM(sccp,accessory)

BEGIN_ENUM(sccp,accessorystate)
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_NONE	 			,,	"None")
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_ONHOOK	 			,,	"On Hook") 
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_OFFHOOK	 		,,	"Off Hook")
END_ENUM(sccp,accessorystate)

BEGIN_ENUM(sccp,config_buttontype)
        ENUM_ELEMENT(LINE			 			,,	"Line")
        ENUM_ELEMENT(SPEEDDIAL	 					,,	"Speeddial") 
        ENUM_ELEMENT(SERVICE				 		,,	"Service")
        ENUM_ELEMENT(FEATURE	 					,,	"Feature")
        ENUM_ELEMENT(EMPTY	 					,,	"Empty")
END_ENUM(sccp,config_buttontype)


