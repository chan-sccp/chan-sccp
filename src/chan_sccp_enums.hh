/*!
 * \file        chan_sccp_enums.hh
 * \brief       SCCP Enums
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */
///////////////////////////////
// The enum examples
///////////////////////////////
// Simple verification method
//
// gcc -E -I. chan_sccp.h |indent > chan_sccp.h
// gcc -E -I. chan_sccp.c |indent > chan_sccp.c

///////////////////////////////
// The sccp enum declarations
///////////////////////////////

/*! 
 * \brief internal chan_sccp call state (c->callstate) (Enum)
 */
BEGIN_ENUM(sccp,channelstate,ENUMMACRO_SPARSE)
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DOWN 				,= 0, 	"DOWN")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_ONHOOK 				,= 1, 	"ONHOOK")

        ENUM_ELEMENT(SCCP_CHANNELSTATE_OFFHOOK 				,= 10, 	"OFFHOOK")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_GETDIGITS 			,= 11,	"GETDIGITS")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DIGITSFOLL 			,= 12,	"DIGITSFOLL")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_SPEEDDIAL 			,= 13,	"SPEEDDIAL")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_DIALING 				,= 14,	"DIALING")

        ENUM_ELEMENT(SCCP_CHANNELSTATE_RINGOUT 				,= 20,	"RINGOUT")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_RINGING 				,= 21,	"RINGING")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_PROCEED 				,= 22,	"PROCEED")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_PROGRESS 			,= 23,	"PROGRESS")

        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONNECTED 			,= 30,	"CONNECTED")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONNECTEDCONFERENCE 		,= 31,	"CONNECTEDCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_HOLD 				,= 32,	"HOLD   ")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLWAITING 			,= 34,	"CALLWAITING")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLPARK 			,= 35,	"CALLPARK")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLREMOTEMULTILINE 		,= 36,	"CALLREMOTEMULTILINE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLCONFERENCE	 		,= 37,	"CALLCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CALLTRANSFER 			,= 38,	"CALLTRANSFER")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_BLINDTRANSFER 			,= 39,	"BLINDTRANSFER")

        ENUM_ELEMENT(SCCP_CHANNELSTATE_DND 				,= 40,	"DND")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_BUSY 				,= 41,	"BUSY   ")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_CONGESTION 			,= 42,	"CONGESTION")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_INVALIDNUMBER 			,= 43 ,	"INVALIDNUMBER")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_INVALIDCONFERENCE 		,= 44,	"INVALIDCONFERENCE")
        ENUM_ELEMENT(SCCP_CHANNELSTATE_ZOMBIE 				,= 45,	"ZOMBIE")
END_ENUM(sccp,channelstate,ENUMMACRO_SPARSE)			 								/*!< internal Chan_SCCP Call State c->callstate */

BEGIN_ENUM(sccp,channelstatereason,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_CHANNELSTATEREASON_NORMAL			,= 0,	"NORMAL")
        ENUM_ELEMENT(SCCP_CHANNELSTATEREASON_TRANSFER			,= 1,	"TRANSFER")
        ENUM_ELEMENT(SCCP_CHANNELSTATEREASON_CALLFORWARD		,= 2,	"CALLFORWARD")
        ENUM_ELEMENT(SCCP_CHANNELSTATEREASON_CONFERENCE			,= 3,	"CONFERENCE")
END_ENUM(sccp,channelstatereason,ENUMMACRO_SPARSE)

BEGIN_ENUM(sccp,earlyrtp,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_EARLYRTP_IMMEDIATE 				,, 	"Immediate")
        ENUM_ELEMENT(SCCP_EARLYRTP_OFFHOOK 				,, 	"OffHook")
        ENUM_ELEMENT(SCCP_EARLYRTP_DIALING 				,, 	"Dialing")
        ENUM_ELEMENT(SCCP_EARLYRTP_RINGOUT 				,,	"Ringout")
        ENUM_ELEMENT(SCCP_EARLYRTP_PROGRESS 				,,	"Progress")
        ENUM_ELEMENT(SCCP_EARLYRTP_NONE 				,,	"None")
END_ENUM(sccp,earlyrtp,ENUMMACRO_INCREMENTAL)												/*!< internal Chan_SCCP Call State c->callstate */

BEGIN_ENUM(sccp,devicestate,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_DEVICESTATE_ONHOOK				,,	"On Hook"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_OFFHOOK				,,	"Off Hook"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_UNAVAILABLE			,,	"Unavailable"	)
        ENUM_ELEMENT(SCCP_DEVICESTATE_DND				,,	"Do Not Disturb")
        ENUM_ELEMENT(SCCP_DEVICESTATE_FWDALL				,,	"Forward All"	)
END_ENUM(sccp,devicestate,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,callforward,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_CFWD_NONE		 			,,	"None")
        ENUM_ELEMENT(SCCP_CFWD_ALL 					,,	"All")
        ENUM_ELEMENT(SCCP_CFWD_BUSY 					,,	"Busy")
        ENUM_ELEMENT(SCCP_CFWD_NOANSWER 				,,	"NoAnswer")
END_ENUM(sccp,callforward,ENUMMACRO_INCREMENTAL)

/*!
 * \brief SCCP Dtmf Mode (ENUM)
 */
BEGIN_ENUM(sccp,dtmfmode,ENUMMACRO_INCREMENTAL)
	ENUM_ELEMENT(SCCP_DTMFMODE_AUTO					,,	"AUTO")
	ENUM_ELEMENT(SCCP_DTMFMODE_RFC2833				,,	"RFC2833")
	ENUM_ELEMENT(SCCP_DTMFMODE_SKINNY				,,	"SKINNY")
END_ENUM(sccp,dtmfmode,ENUMMACRO_INCREMENTAL)

/*!
 * \brief SCCP Autoanswer (ENUM)
 */
BEGIN_ENUM(sccp,autoanswer,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_AUTOANSWER_NONE				,,	"AutoAnswer None")
        ENUM_ELEMENT(SCCP_AUTOANSWER_1W 				,,	"AutoAnswer 1-Way")
        ENUM_ELEMENT(SCCP_AUTOANSWER_2W 				,,	"AutoAnswer Both Ways")
END_ENUM(sccp,autoanswer,ENUMMACRO_INCREMENTAL)

/*!
 * \brief SCCP DNDMode (ENUM)
 */
BEGIN_ENUM(sccp,dndmode,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_DNDMODE_OFF					,,	"Off")
        ENUM_ELEMENT(SCCP_DNDMODE_REJECT				,,	"Reject")
        ENUM_ELEMENT(SCCP_DNDMODE_SILENT				,,	"Silent")
        ENUM_ELEMENT(SCCP_DNDMODE_USERDEFINED				,,	"UserDefined")
END_ENUM(sccp,dndmode,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,accessory,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_ACCESSORY_NONE	 			,,	"None")
        ENUM_ELEMENT(SCCP_ACCESSORY_HEADSET	 			,,	"Headset")
        ENUM_ELEMENT(SCCP_ACCESSORY_HANDSET	 			,,	"Handset") 
        ENUM_ELEMENT(SCCP_ACCESSORY_SPEAKER	 			,,	"Speaker")
END_ENUM(sccp,accessory,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,accessorystate,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_NONE	 			,,	"None")
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_ONHOOK	 			,,	"On Hook") 
        ENUM_ELEMENT(SCCP_ACCESSORYSTATE_OFFHOOK	 		,,	"Off Hook")
END_ENUM(sccp,accessorystate,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,config_buttontype,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(LINE			 			,,	"Line")
        ENUM_ELEMENT(SPEEDDIAL	 					,,	"Speeddial") 
        ENUM_ELEMENT(SERVICE				 		,,	"Service")
        ENUM_ELEMENT(FEATURE	 					,,	"Feature")
        ENUM_ELEMENT(EMPTY	 					,,	"Empty")
END_ENUM(sccp,config_buttontype,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,devstate_state,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_DEVSTATE_IDLE					,=0 << 0,	"IDLE")
        ENUM_ELEMENT(SCCP_DEVSTATE_INUSE				,=1 << 1,	"INUSE")
END_ENUM(sccp,devstate_state,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,blindtransferindication,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_BLINDTRANSFER_RING				,=0,	"RING")
        ENUM_ELEMENT(SCCP_BLINDTRANSFER_MOH				,=1,	"MOH")
END_ENUM(sccp,blindtransferindication,ENUMMACRO_INCREMENTAL)

BEGIN_ENUM(sccp,call_answer_order,ENUMMACRO_INCREMENTAL)
#ifndef CS_EXPERIMENTAL
        ENUM_ELEMENT(SCCP_ANSWER_OLDEST_FIRST			,=0,	"OldestFirst")
        ENUM_ELEMENT(SCCP_ANSWER_LAST_FIRST			,=1,	"LastFirst")
#else
        ENUM_ELEMENT(SCCP_ANSWER_OLDEST_FIRST				,=0,	"OldestFirst")
        ENUM_ELEMENT(SCCP_ANSWER_LAST_FIRST				,=1,	"LastFirst")
#endif
END_ENUM(sccp,call_answer_order,ENUMMACRO_INCREMENTAL)

#ifdef CS_EXPERIMENTAL
BEGIN_ENUM(sccp,nat,ENUMMACRO_INCREMENTAL)
        ENUM_ELEMENT(SCCP_NAT_AUTO					,= 0,	"Auto")
        ENUM_ELEMENT(SCCP_NAT_OFF					,= 1,	"Off")
        ENUM_ELEMENT(SCCP_NAT_AUTO_OFF					,= 2,	"(Auto) Off")
        ENUM_ELEMENT(SCCP_NAT_ON					,= 3,	"On")
        ENUM_ELEMENT(SCCP_NAT_AUTO_ON					,= 4,	"(Auto) On")
END_ENUM(sccp,nat,ENUMMACRO_INCREMENTAL)

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
