/*!
 * \file 	sccp_labels.h
 * \brief 	SCCP Labels Header
 * 
 * SCCP Button Number References and SCCP Display Number References 
 *
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_LABELS_H
#    define __SCCP_LABELS_H

#    define SKINNY_LBL_EMPTY                                             	0
										/*< fake button */
#    define SKINNY_LBL_REDIAL                                            	1
#    define SKINNY_LBL_NEWCALL                                           	2
#    define SKINNY_LBL_HOLD                                              	3
#    define SKINNY_LBL_TRANSFER                                          	4
#    define SKINNY_LBL_CFWDALL                                           	5
#    define SKINNY_LBL_CFWDBUSY                                          	6
#    define SKINNY_LBL_CFWDNOANSWER                                      	7
#    define SKINNY_LBL_BACKSPACE                                         	8
#    define SKINNY_LBL_ENDCALL                                           	9
#    define SKINNY_LBL_RESUME                                            	10
#    define SKINNY_LBL_ANSWER                                            	11
#    define SKINNY_LBL_INFO                                              	12
#    define SKINNY_LBL_CONFRN                                            	13
#    define SKINNY_LBL_PARK                                              	14
#    define SKINNY_LBL_JOIN                                              	15
#    define SKINNY_LBL_MEETME                                            	16
#    define SKINNY_LBL_PICKUP                                            	17
#    define SKINNY_LBL_GPICKUP                                           	18
#    define SKINNY_LBL_YOUR_CURRENT_OPTIONS                              	19
#    define SKINNY_LBL_OFF_HOOK                                          	20
#    define SKINNY_LBL_ON_HOOK                                           	21
#    define SKINNY_LBL_RING_OUT                                          	22
#    define SKINNY_LBL_FROM                                              	23
#    define SKINNY_LBL_CONNECTED                                         	24
#    define SKINNY_LBL_BUSY                                              	25
#    define SKINNY_LBL_LINE_IN_USE                                       	26
#    define SKINNY_LBL_CALL_WAITING                                      	27
#    define SKINNY_LBL_CALL_TRANSFER                                     	28
#    define SKINNY_LBL_CALL_PARK                                         	29
#    define SKINNY_LBL_CALL_PROCEED                                      	30
#    define SKINNY_LBL_IN_USE_REMOTE                                     	31
#    define SKINNY_LBL_ENTER_NUMBER                                      	32
#    define SKINNY_LBL_CALL_PARK_AT                                      	33
#    define SKINNY_LBL_PRIMARY_ONLY                                      	34
#    define SKINNY_LBL_TEMP_FAIL                                         	35
#    define SKINNY_LBL_YOU_HAVE_VOICEMAIL                                	36
#    define SKINNY_LBL_FORWARDED_TO                                      	37
#    define SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE                       	38
#    define SKINNY_LBL_NO_CONFERENCE_BRIDGE                              	39
#    define SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL                      	40
#    define SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT                    	41
#    define SKINNY_LBL_IN_CONFERENCE_ALREADY                             	42
#    define SKINNY_LBL_NO_PARTICIPANT_INFO                               	43
#    define SKINNY_LBL_EXCEED_MAXIMUM_PARTIES                            	44
#    define SKINNY_LBL_KEY_IS_NOT_ACTIVE                                 	45
#    define SKINNY_LBL_ERROR_NO_LICENSE                                  	46
#    define SKINNY_LBL_ERROR_DBCONFIG                                    	47
#    define SKINNY_LBL_ERROR_DATABASE                                    	48
#    define SKINNY_LBL_ERROR_PASS_LIMIT                                  	49
#    define SKINNY_LBL_ERROR_UNKNOWN                                     	50
#    define SKINNY_LBL_ERROR_MISMATCH                                    	51
#    define SKINNY_LBL_CONFERENCE                                        	52
#    define SKINNY_LBL_PARK_NUMBER                                       	53
#    define SKINNY_LBL_PRIVATE                                           	54
#    define SKINNY_LBL_NOT_ENOUGH_BANDWIDTH                              	55
#    define SKINNY_LBL_UNKNOWN_NUMBER                                    	56
#    define SKINNY_LBL_RMLSTC                                            	57
#    define SKINNY_LBL_VOICEMAIL                                         	58
#    define SKINNY_LBL_IMMDIV                                            	59
#    define SKINNY_LBL_INTRCPT                                           	60
#    define SKINNY_LBL_SETWTCH                                           	61
#    define SKINNY_LBL_TRNSFVM                                           	62
#    define SKINNY_LBL_DND                                               	63
#    define SKINNY_LBL_DIVALL                                            	64
#    define SKINNY_LBL_CALLBACK                                          	65
#    define SKINNY_LBL_NETWORK_CONGESTION_REROUTING                      	66
#    define SKINNY_LBL_BARGE                                             	67
#    define SKINNY_LBL_FAILED_TO_SETUP_BARGE                             	68
#    define SKINNY_LBL_ANOTHER_BARGE_EXISTS                              	69
#    define SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE                          	70
#    define SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE                          	71
#    define SKINNY_LBL_CALLPARK_REVERSION                                	72
#    define SKINNY_LBL_SERVICE_IS_NOT_ACTIVE                             	73
#    define SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER                      	74
#    define SKINNY_LBL_QRT                                               	75
#    define SKINNY_LBL_MCID                                              	76
#    define SKINNY_LBL_DIRTRFR                                           	77
#    define SKINNY_LBL_SELECT                                            	78
#    define SKINNY_LBL_CONFLIST                                          	79
#    define SKINNY_LBL_IDIVERT                                           	80
#    define SKINNY_LBL_CBARGE                                            	81
#    define SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER                         	82
#    define SKINNY_LBL_CAN_NOT_JOIN_CALLS                                	83
#    define SKINNY_LBL_MCID_SUCCESSFUL                                   	84
#    define SKINNY_LBL_NUMBER_NOT_CONFIGURED                             	85
#    define SKINNY_LBL_SECURITY_ERROR                                    	86
#    define SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE                       	87
#    define SKINNY_LBL_VIDEO_MODE					        88

/*!
 * \brief Skinny LABEL Structure
 */
static const struct skinny_label {
	uint16_t label;
	const char *const text;
} skinny_labels[] = {
	/* *INDENT-OFF* */
	{SKINNY_LBL_EMPTY, "Empty"},
	{SKINNY_LBL_REDIAL, "Redial"},
	{SKINNY_LBL_NEWCALL, "NewCall"},
	{SKINNY_LBL_HOLD, "Hold"},
	{SKINNY_LBL_TRANSFER, "Transfer"},
	{SKINNY_LBL_CFWDALL, "CFwdALL"},
	{SKINNY_LBL_CFWDBUSY, "CFwdBusy"},
	{SKINNY_LBL_CFWDNOANSWER, "CFwdNoAnswer"},
	{SKINNY_LBL_BACKSPACE, "&lt;&lt;"},
	{SKINNY_LBL_ENDCALL, "EndCall"},
	{SKINNY_LBL_RESUME, "Resume"},
	{SKINNY_LBL_ANSWER, "Answer"},
	{SKINNY_LBL_INFO, "Info"},
	{SKINNY_LBL_CONFRN, "Confrn"},
	{SKINNY_LBL_PARK, "Park"},
	{SKINNY_LBL_JOIN, "Join"},
	{SKINNY_LBL_MEETME, "MeetMe"},
	{SKINNY_LBL_PICKUP, "PickUp"},
	{SKINNY_LBL_GPICKUP, "GPickUp"},
	{SKINNY_LBL_YOUR_CURRENT_OPTIONS, "Your current options"},
	{SKINNY_LBL_OFF_HOOK, "Off Hook"},
	{SKINNY_LBL_ON_HOOK, "On Hook"},
	{SKINNY_LBL_RING_OUT, "Ring out"},
	{SKINNY_LBL_FROM, "From "},
	{SKINNY_LBL_CONNECTED, "Connected"},
	{SKINNY_LBL_BUSY, "Busy"},
	{SKINNY_LBL_LINE_IN_USE, "Line In Use"},
	{SKINNY_LBL_CALL_WAITING, "Call Waiting"},
	{SKINNY_LBL_CALL_TRANSFER, "Call Transfer"},
	{SKINNY_LBL_CALL_PARK, "Call Park"},
	{SKINNY_LBL_CALL_PROCEED, "Call Proceed"},
	{SKINNY_LBL_IN_USE_REMOTE, "In Use Remote"},
	{SKINNY_LBL_ENTER_NUMBER, "Enter number"},
	{SKINNY_LBL_CALL_PARK_AT, "Call park At"},
	{SKINNY_LBL_PRIMARY_ONLY, "Primary Only"},
	{SKINNY_LBL_TEMP_FAIL, "Temp Fail"},
	{SKINNY_LBL_YOU_HAVE_VOICEMAIL, "You Have VoiceMail"},
	{SKINNY_LBL_FORWARDED_TO, "Forwarded to"},
	{SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE, "Can Not Complete Conference"},
	{SKINNY_LBL_NO_CONFERENCE_BRIDGE, "No Conference Bridge"},
	{SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL, "Can Not Hold Primary Control"},
	{SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT, "Invalid Conference Participant"},
	{SKINNY_LBL_IN_CONFERENCE_ALREADY, "In Conference Already"},
	{SKINNY_LBL_NO_PARTICIPANT_INFO, "No Participant Info"},
	{SKINNY_LBL_EXCEED_MAXIMUM_PARTIES, "Exceed Maximum Parties"},
	{SKINNY_LBL_KEY_IS_NOT_ACTIVE, "Key Is Not Active"},
	{SKINNY_LBL_ERROR_NO_LICENSE, "Error No License"},
	{SKINNY_LBL_ERROR_DBCONFIG, "Error DBConfig"},
	{SKINNY_LBL_ERROR_DATABASE, "Error Database"},
	{SKINNY_LBL_ERROR_PASS_LIMIT, "Error Pass Limit"},
	{SKINNY_LBL_ERROR_UNKNOWN, "Error Unknown"},
	{SKINNY_LBL_ERROR_MISMATCH, "Error Mismatch"},
	{SKINNY_LBL_CONFERENCE, "Conference"},
	{SKINNY_LBL_PARK_NUMBER, "Park Number"},
	{SKINNY_LBL_PRIVATE, "Private"},
	{SKINNY_LBL_NOT_ENOUGH_BANDWIDTH, "Not Enough Bandwidth"},
	{SKINNY_LBL_UNKNOWN_NUMBER, "Unknown Number"},
	{SKINNY_LBL_RMLSTC, "RmLstC"},
	{SKINNY_LBL_VOICEMAIL, "Voicemail"},
	{SKINNY_LBL_IMMDIV, "ImmDiv"},
	{SKINNY_LBL_INTRCPT, "Intrcpt"},
	{SKINNY_LBL_SETWTCH, "SetWtch"},
	{SKINNY_LBL_TRNSFVM, "TrnsfVM"},
	{SKINNY_LBL_DND, "DND"},
	{SKINNY_LBL_DIVALL, "DivAll"},
	{SKINNY_LBL_CALLBACK, "CallBack"},
	{SKINNY_LBL_NETWORK_CONGESTION_REROUTING, "Network congestion,rerouting"},
	{SKINNY_LBL_BARGE, "Barge"},
	{SKINNY_LBL_FAILED_TO_SETUP_BARGE, "Failed to setup Barge"},
	{SKINNY_LBL_ANOTHER_BARGE_EXISTS, "Another Barge exists"},
	{SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE, "Incompatible device type"},
	{SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE, "No Park Number Available"},
	{SKINNY_LBL_CALLPARK_REVERSION, "CallPark Reversion"},
	{SKINNY_LBL_SERVICE_IS_NOT_ACTIVE, "Service is not Active"},
	{SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER, "High Traffic Try Again Later"},
	{SKINNY_LBL_QRT, "QRT"},
	{SKINNY_LBL_MCID, "MCID"},
	{SKINNY_LBL_DIRTRFR, "DirTrfr"},
	{SKINNY_LBL_SELECT, "Select"},
	{SKINNY_LBL_CONFLIST, "ConfList"},
	{SKINNY_LBL_IDIVERT, "iDivert"},
	{SKINNY_LBL_CBARGE, "cBarge"},
	{SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER, "Can Not Complete Transfer"},
	{SKINNY_LBL_CAN_NOT_JOIN_CALLS, "Can Not Join Calls"},
	{SKINNY_LBL_MCID_SUCCESSFUL, "Mcid Successful"},
	{SKINNY_LBL_NUMBER_NOT_CONFIGURED, "Number Not Configured"},
	{SKINNY_LBL_SECURITY_ERROR, "Security Error"},
	{SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE, "Video Bandwidth Unavailable"},
	{SKINNY_LBL_VIDEO_MODE, "Video Mode"}
	/* *INDENT-ON* */
};

#    define SKINNY_DISP_EMPTY                                		""
#    define SKINNY_DISP_REDIAL                                		"\200\1"
#    define SKINNY_DISP_NEWCALL                               		"\200\2"
#    define SKINNY_DISP_HOLD                                  		"\200\3"
#    define SKINNY_DISP_TRANSFER                              		"\200\4"
#    define SKINNY_DISP_CFWDALL                               		"\200\5"
#    define SKINNY_DISP_CFWDBUSY                              		"\200\6"
#    define SKINNY_DISP_CFWDNOANSWER                          		"\200\7"

#    define SKINNY_DISP_BACKSPACE                             		"\200\10"
#    define SKINNY_DISP_ENDCALL                               		"\200\11"
#    define SKINNY_DISP_RESUME                                		"\200\12"
#    define SKINNY_DISP_ANSWER                                		"\200\13"
#    define SKINNY_DISP_INFO                                  		"\200\14"
#    define SKINNY_DISP_CONFRN                                		"\200\15"
#    define SKINNY_DISP_PARK                                  		"\200\16"
#    define SKINNY_DISP_JOIN                                  		"\200\17"

#    define SKINNY_DISP_MEETME                                		"\200\20"
#    define SKINNY_DISP_PICKUP                                		"\200\21"
#    define SKINNY_DISP_GPICKUP                               		"\200\22"
#    define SKINNY_DISP_YOUR_CURRENT_OPTIONS                  		"\200\23"
#    define SKINNY_DISP_OFF_HOOK                              		"\200\24"
#    define SKINNY_DISP_ON_HOOK                               		"\200\25"
#    define SKINNY_DISP_RING_OUT                              		"\200\26"
#    define SKINNY_DISP_FROM                                  		"\200\27"

#    define SKINNY_DISP_CONNECTED                             		"\200\30"
#    define SKINNY_DISP_BUSY                                  		"\200\31"
#    define SKINNY_DISP_LINE_IN_USE                           		"\200\32"
#    define SKINNY_DISP_CALL_WAITING                          		"\200\33"
#    define SKINNY_DISP_CALL_TRANSFER                         		"\200\34"
#    define SKINNY_DISP_CALL_PARK                             		"\200\35"
#    define SKINNY_DISP_CALL_PROCEED                          		"\200\36"
#    define SKINNY_DISP_IN_USE_REMOTE                         		"\200\37"

#    define SKINNY_DISP_ENTER_NUMBER                          		"\200\40"
#    define SKINNY_DISP_CALL_PARK_AT                          		"\200\41"
#    define SKINNY_DISP_PRIMARY_ONLY                          		"\200\42"
#    define SKINNY_DISP_TEMP_FAIL                             		"\200\43"
#    define SKINNY_DISP_YOU_HAVE_VOICEMAIL                    		"\200\44"
#    define SKINNY_DISP_FORWARDED_TO                          		"\200\45"
#    define SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE           		"\200\46"
#    define SKINNY_DISP_NO_CONFERENCE_BRIDGE                  		"\200\47"

#    define SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL          		"\200\50"
#    define SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT        		"\200\51"
#    define SKINNY_DISP_IN_CONFERENCE_ALREADY                 		"\200\52"
#    define SKINNY_DISP_NO_PARTICIPANT_INFO                   		"\200\53"
#    define SKINNY_DISP_EXCEED_MAXIMUM_PARTIES                		"\200\54"
#    define SKINNY_DISP_KEY_IS_NOT_ACTIVE                     		"\200\55"
#    define SKINNY_DISP_ERROR_NO_LICENSE                      		"\200\56"
#    define SKINNY_DISP_ERROR_DBCONFIG                        		"\200\57"

#    define SKINNY_DISP_ERROR_DATABASE                        		"\200\60"
#    define SKINNY_DISP_ERROR_PASS_LIMIT                      		"\200\61"
#    define SKINNY_DISP_ERROR_UNKNOWN                         		"\200\62"
#    define SKINNY_DISP_ERROR_MISMATCH                        		"\200\63"
#    define SKINNY_DISP_CONFERENCE                            		"\200\64"
#    define SKINNY_DISP_PARK_NUMBER                           		"\200\65"
#    define SKINNY_DISP_PRIVATE                               		"\200\66"
#    define SKINNY_DISP_NOT_ENOUGH_BANDWIDTH                  		"\200\67"

#    define SKINNY_DISP_UNKNOWN_NUMBER                        		"\200\70"
#    define SKINNY_DISP_RMLSTC                                		"\200\71"
#    define SKINNY_DISP_VOICEMAIL                             		"\200\72"
#    define SKINNY_DISP_IMMDIV                                		"\200\73"
#    define SKINNY_DISP_INTRCPT                               		"\200\74"
#    define SKINNY_DISP_SETWTCH                               		"\200\75"
#    define SKINNY_DISP_TRNSFVM                               		"\200\76"
#    define SKINNY_DISP_DND                                   		"\200\77"

#    define SKINNY_DISP_DIVALL                                		"\200\100"
#    define SKINNY_DISP_CALLBACK                              		"\200\101"
#    define SKINNY_DISP_NETWORK_CONGESTION_REROUTING          		"\200\102"
#    define SKINNY_DISP_BARGE                                 		"\200\103"
#    define SKINNY_DISP_FAILED_TO_SETUP_BARGE                 		"\200\104"
#    define SKINNY_DISP_ANOTHER_BARGE_EXISTS                  		"\200\105"
#    define SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE              		"\200\106"
#    define SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE              		"\200\107"

#    define SKINNY_DISP_CALLPARK_REVERSION                    		"\200\110"
#    define SKINNY_DISP_SERVICE_IS_NOT_ACTIVE                 		"\200\111"
#    define SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER          		"\200\112"
#    define SKINNY_DISP_QRT                                   		"\200\113"
#    define SKINNY_DISP_MCID                                  		"\200\114"
#    define SKINNY_DISP_DIRTRFR                               		"\200\115"
#    define SKINNY_DISP_SELECT                                		"\200\116"
#    define SKINNY_DISP_CONFLIST                              		"\200\117"

#    define SKINNY_DISP_IDIVERT                               		"\200\120"
#    define SKINNY_DISP_CBARGE                                		"\200\121"
#    define SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER             		"\200\122"
#    define SKINNY_DISP_CAN_NOT_JOIN_CALLS                    		"\200\123"
#    define SKINNY_DISP_MCID_SUCCESSFUL                       		"\200\124"
#    define SKINNY_DISP_NUMBER_NOT_CONFIGURED                 		"\200\125"
#    define SKINNY_DISP_SECURITY_ERROR                        		"\200\126"
#    define SKINNY_DISP_VIDEO_BANDWIDTH_UNAVAILABLE           		"\200\127"

#    define SKINNY_DISP_VIDMODE						"\200\130"
#    define SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT			"\200\131"
#    define SKINNY_DISP_MAX_HOLD_DURATION_TIMEOUT			"\200\132"
#    define SKINNY_DISP_OPICKUP						"\200\133"
#    define SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP                	"\200\137"      /*<! - FS */
#    define SKINNY_DISP_PARK_SLOT_UNAVAILABLE                       	"\200\136"      /*<! - FS */
#    define SKINNY_DISP_LOGGED_OUT_OF_HUNT_GROUP                    	"\200\135"      /*<! - FS */
#    define SKINNY_DISP_HLOG                                        	"\200\134"      /*<! - FS */

#    define SKINNY_DISP_EXTERNAL_TRANSFER_RESTRICTED			"\200\141"
#    define SKINNY_DISP_NO_LINE_AVAILABLE_FOR_PICKUP			"\200\142"	/*<! - FS */
#    define SKINNY_DISP_UNKNOWN_1					"\200\143"
#    define SKINNY_DISP_UNKNOWN_2					"\200\144"
#    define SKINNY_DISP_MAC_ADDRESS					"\200\145"
#    define SKINNY_DISP_HOST_NAME					"\200\146"
#    define SKINNY_DISP_DOMAIN_NAME					"\200\147"

#    define SKINNY_DISP_IP_ADDRESS					"\200\150"
#    define SKINNY_DISP_SUBNET_MASK					"\200\151"
#    define SKINNY_DISP_TFTP_SERVER_1					"\200\152"
#    define SKINNY_DISP_DEFAULT_ROUTER_1				"\200\153"
#    define SKINNY_DISP_DEFAULT_ROUTER_2				"\200\154"
#    define SKINNY_DISP_DEFAULT_ROUTER_3				"\200\155"
#    define SKINNY_DISP_DEFAULT_ROUTER_4				"\200\156"
#    define SKINNY_DISP_DEFAULT_ROUTER_5				"\200\157"

#    define SKINNY_DISP_DNS_SERVER_1					"\200\160"
#    define SKINNY_DISP_DNS_SERVER_2					"\200\161"
#    define SKINNY_DISP_DNS_SERVER_3					"\200\162"
#    define SKINNY_DISP_DNS_SERVER_4					"\200\163"
#    define SKINNY_DISP_DNS_SERVER_5					"\200\164"
#    define SKINNY_DISP_OPERATIONAL_VLAN_ID				"\200\165"
#    define SKINNY_DISP_ADMIN_VLAN_ID					"\200\166"
#    define SKINNY_DISP_CALL_MANAGER_1					"\200\167"

#    define SKINNY_DISP_CALL_MANAGER_2					"\200\170"
#    define SKINNY_DISP_CALL_MANAGER_3					"\200\171"
#    define SKINNY_DISP_CALL_MANAGER_4					"\200\172"
#    define SKINNY_DISP_CALL_MANAGER_5					"\200\173"
#    define SKINNY_DISP_INFORMATION_URL					"\200\174"
#    define SKINNY_DISP_DIRECTORIES_URL					"\200\175"
#    define SKINNY_DISP_MESSAGES_URL					"\200\176"
#    define SKINNY_DISP_SERVICES_URL					"\200\177"

/*!
 * \brief Ast Cause - Skinny DISP Mapping
 */
static const struct ast_skinny_cause {
	int ast_cause;
	const char * skinny_disp;
	const char * message;
} ast_skinny_causes[] = {
	/* *INDENT-OFF* */
        { AST_CAUSE_UNALLOCATED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED				, "Unallocated (unassigned) number" },
        { AST_CAUSE_NO_ROUTE_TRANSIT_NET,	SKINNY_DISP_UNKNOWN_NUMBER					, "No route to specified transmit network" },
        { AST_CAUSE_NO_ROUTE_DESTINATION,	SKINNY_DISP_UNKNOWN_NUMBER					, "No route to destination" },
        { AST_CAUSE_CHANNEL_UNACCEPTABLE,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Channel unacceptable" },   
        { AST_CAUSE_CALL_AWARDED_DELIVERED,	SKINNY_DISP_CALL_PROCEED				, "Call awarded and being delivered in an established channel" },
  	{ AST_CAUSE_NORMAL_CLEARING,		SKINNY_DISP_ON_HOOK 				, "Normal Clearing" },
        { AST_CAUSE_USER_BUSY, 			SKINNY_DISP_BUSY				, "User busy" },
	{ AST_CAUSE_NO_USER_RESPONSE,		SKINNY_DISP_EMPTY				, "No user responding" },
        { AST_CAUSE_NO_ANSWER,			SKINNY_DISP_EMPTY				, "User alerting, no answer" },
        { AST_CAUSE_CALL_REJECTED,		SKINNY_DISP_BUSY				, "Call Rejected" },   
	{ AST_CAUSE_NUMBER_CHANGED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED				, "Number changed" },
        { AST_CAUSE_DESTINATION_OUT_OF_ORDER,	SKINNY_DISP_TEMP_FAIL				, "Destination out of order" },
        { AST_CAUSE_INVALID_NUMBER_FORMAT, 	SKINNY_DISP_UNKNOWN_NUMBER 			, "Invalid number format" },
        { AST_CAUSE_FACILITY_REJECTED,		SKINNY_DISP_TEMP_FAIL				, "Facility rejected" },
	{ AST_CAUSE_RESPONSE_TO_STATUS_ENQUIRY,	SKINNY_DISP_TEMP_FAIL				, "Response to STATus ENQuiry" },
        { AST_CAUSE_NORMAL_UNSPECIFIED,		SKINNY_DISP_TEMP_FAIL				, "Normal, unspecified" },
  	{ AST_CAUSE_NORMAL_CIRCUIT_CONGESTION,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Circuit/channel congestion" },
        { AST_CAUSE_NETWORK_OUT_OF_ORDER, 	SKINNY_DISP_NETWORK_CONGESTION_REROUTING	, "Network out of order" },
  	{ AST_CAUSE_NORMAL_TEMPORARY_FAILURE, 	SKINNY_DISP_TEMP_FAIL				, "Temporary failure" },
        { AST_CAUSE_SWITCH_CONGESTION,		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Switching equipment congestion" }, 
	{ AST_CAUSE_ACCESS_INFO_DISCARDED,	SKINNY_DISP_SECURITY_ERROR				, "Access information discarded" },
        { AST_CAUSE_REQUESTED_CHAN_UNAVAIL,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER				, "Requested channel not available" },
	{ AST_CAUSE_PRE_EMPTED,			SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER				, "Pre-empted" },
        { AST_CAUSE_FACILITY_NOT_SUBSCRIBED,	SKINNY_DISP_TEMP_FAIL				, "Facility not subscribed" },
	{ AST_CAUSE_OUTGOING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR				, "Outgoing call barred" },
        { AST_CAUSE_INCOMING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR				, "Incoming call barred" },
	{ AST_CAUSE_BEARERCAPABILITY_NOTAUTH,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Bearer capability not authorized" },
        { AST_CAUSE_BEARERCAPABILITY_NOTAVAIL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Bearer capability not available" },
        { AST_CAUSE_BEARERCAPABILITY_NOTIMPL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Bearer capability not implemented" },
        { AST_CAUSE_CHAN_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Channel not implemented" },
	{ AST_CAUSE_FACILITY_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Facility not implemented" },
        { AST_CAUSE_INVALID_CALL_REFERENCE,	SKINNY_DISP_TEMP_FAIL				, "Invalid call reference value" },
	{ AST_CAUSE_INCOMPATIBLE_DESTINATION,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Incompatible destination" },
        { AST_CAUSE_INVALID_MSG_UNSPECIFIED,	SKINNY_DISP_ERROR_UNKNOWN				, "Invalid message unspecified" },
        { AST_CAUSE_MANDATORY_IE_MISSING,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL				, "Mandatory information element is missing" },
        { AST_CAUSE_MESSAGE_TYPE_NONEXIST,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL				, "Message type nonexist." },
  	{ AST_CAUSE_WRONG_MESSAGE,		SKINNY_DISP_ERROR_MISMATCH				, "Wrong message" },
        { AST_CAUSE_IE_NONEXIST,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Info. element nonexist or not implemented" },
	{ AST_CAUSE_INVALID_IE_CONTENTS,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Invalid information element contents" },
        { AST_CAUSE_WRONG_CALL_STATE,		SKINNY_DISP_TEMP_FAIL				, "Message not compatible with call state" },
	{ AST_CAUSE_RECOVERY_ON_TIMER_EXPIRE,	SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT				, "Recover on timer expiry" },
        { AST_CAUSE_MANDATORY_IE_LENGTH_ERROR,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Mandatory IE length error" },
	{ AST_CAUSE_PROTOCOL_ERROR,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE				, "Protocol error, unspecified" },
        { AST_CAUSE_INTERWORKING,		SKINNY_DISP_NETWORK_CONGESTION_REROUTING				, "Interworking, unspecified" },
	/* *INDENT-ON* */
};
#endif
