
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
 * \note	Copied several buttons and labels from Federico Santulli / Kerin Francis Millar sccp_label.h file
 *              (which he parsed via a perl conversion script)
 * 		Copied several buttons and labels from Federico's sccp_label.h file, which he parsed via a perl conversion script
 *
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$  
 */

#ifndef __SCCP_LABELS_H
#    define __SCCP_LABELS_H

#    define SKINNY_LBL_EMPTY                                             	0	/*<! fake button */
#    define SKINNY_LBL_ACCT							2	/*<! Copied from FS / KFM until LBL #81 */
#    define SKINNY_LBL_FLASH							3
#    define SKINNY_LBL_LOGIN							4
#    define SKINNY_LBL_DEVICE_IN_HOME_LOCATION					5
#    define SKINNY_LBL_DEVICE_IN_ROAMING_LOCATION				6
#    define SKINNY_LBL_ENTER_AUTHORIZATION_CODE					7
#    define SKINNY_LBL_ENTER_CLIENT_MATTER_CODE					8
#    define SKINNY_LBL_CALL_S_AVAILABLE_FOR_PICKUP				9
#    define SKINNY_LBL_CM_FALLBACK_SERVICE_OPERATING				10
#    define SKINNY_LBL_MAX_PHONES_EXCEEDED					11
#    define SKINNY_LBL_WAITING_TO_RE_HOME					12
#    define SKINNY_LBL_PLEASE_END_CALL						13
#    define SKINNY_LBL_PAGING							14
#    define SKINNY_LBL_SELECT_LINE						15
#    define SKINNY_LBL_TRANSFER_DESTINATION_IS_BUSY				16
#    define SKINNY_LBL_SELECT_A_SERVICE						17
#    define SKINNY_LBL_LOCAL_SERVICES						18
#    define SKINNY_LBL_ENTER_SEARCH_CRITERIA					19
#    define SKINNY_LBL_NIGHT_SERVICE						20
#    define SKINNY_LBL_NIGHT_SERVICE_ACTIVE					21
#    define SKINNY_LBL_NIGHT_SERVICE_DISABLED					22
#    define SKINNY_LBL_LOGIN_SUCCESSFUL						23
#    define SKINNY_LBL_WRONG_PIN						24
#    define SKINNY_LBL_PLEASE_ENTER_PIN						25
#    define SKINNY_LBL_OF							26
#    define SKINNY_LBL_RECORDS_1_TO						27
#    define SKINNY_LBL_NO_RECORD_FOUND						28
#    define SKINNY_LBL_SEARCH_RESULTS						29
#    define SKINNY_LBL_CALL_S_IN_QUEUE						30
#    define SKINNY_LBL_JOIN_TO_HUNT_GROUP					31
#    define SKINNY_LBL_READY							32
#    define SKINNY_LBL_NOTREADY							33
#    define SKINNY_LBL_CALL_ON_HOLD						34
#    define SKINNY_LBL_HOLD_REVERSION						35
#    define SKINNY_LBL_SETUP_FAILED						36
#    define SKINNY_LBL_NO_RESOURCES						37
#    define SKINNY_LBL_DEVICE_NOT_AUTHORIZED					38
#    define SKINNY_LBL_MONITORING						39
#    define SKINNY_LBL_RECORDING_AWAITING_CALL_TO_BE_ACTIVE			40
#    define SKINNY_LBL_RECORDING_ALREADY_IN_PROGRESS				41
#    define SKINNY_LBL_INACTIVE_RECORDING_SESSION				42
#    define SKINNY_LBL_MOBILITY							43
#    define SKINNY_LBL_WHISPER							44
#    define SKINNY_LBL_FORWARD_ALL						45
#    define SKINNY_LBL_MALICIOUS_CALL_ID					46
#    define SKINNY_LBL_GROUP_PICKUP						47
#    define SKINNY_LBL_REMOVE_LAST_PARTICIPANT					48
#    define SKINNY_LBL_OTHER_PICKUP						49
#    define SKINNY_LBL_VIDEO							50
#    define SKINNY_LBL_END_CALL							51
#    define SKINNY_LBL_CONFERENCE_LIST						52
#    define SKINNY_LBL_QUALITY_REPORTING_TOOL					53
#    define SKINNY_LBL_HUNT_GROUP						54
#    define SKINNY_LBL_USE_LINE_OR_JOIN_TO_COMPLETE				55
#    define SKINNY_LBL_DO_NOT_DISTURB						56
#    define SKINNY_LBL_DO_NOT_DISTURB_IS_ACTIVE					57
#    define SKINNY_LBL_CFWDALL_LOOP_DETECTED					58
#    define SKINNY_LBL_CFWDALL_HOPS_EXCEEDED					59
#    define SKINNY_LBL_ABBRDIAL							60
#    define SKINNY_LBL_PICKUP_IS_UNAVAILABLE					61
#    define SKINNY_LBL_CONFERENCE_IS_UNAVAILABLE				62
#    define SKINNY_LBL_MEETME_IS_UNAVAILABLE					63
#    define SKINNY_LBL_CANNOT_RETRIEVE_PARKED_CALL				64
#    define SKINNY_LBL_CANNOT_SEND_CALL_TO_MOBILE				65
#    define SKINNY_LBL_RECORD							67
#    define SKINNY_LBL_CANNOT_MOVE_CONVERSATION					68
#    define SKINNY_LBL_CW_OFF							69
#    define SKINNY_LBL_RECORDING						79
#    define SKINNY_LBL_RECORDING_FAILED						80
#    define SKINNY_LBL_CONNECTING_PLEASE_WAIT					81	/*<! Copied from FS / KFM starting at LBL #2 */
#    define SKINNY_LBL_REDIAL                                            	101
#    define SKINNY_LBL_NEWCALL                                           	102
#    define SKINNY_LBL_HOLD                                              	103
#    define SKINNY_LBL_TRANSFER                                          	104
#    define SKINNY_LBL_CFWDALL                                           	105
#    define SKINNY_LBL_CFWDBUSY                                          	106
#    define SKINNY_LBL_CFWDNOANSWER                                      	107
#    define SKINNY_LBL_BACKSPACE                                         	108
#    define SKINNY_LBL_ENDCALL                                           	109
#    define SKINNY_LBL_RESUME                                            	110
#    define SKINNY_LBL_ANSWER                                            	111
#    define SKINNY_LBL_INFO                                              	112
#    define SKINNY_LBL_CONFRN                                            	113
#    define SKINNY_LBL_PARK                                              	114
#    define SKINNY_LBL_JOIN                                              	115
#    define SKINNY_LBL_MEETME                                            	116
#    define SKINNY_LBL_PICKUP                                            	117
#    define SKINNY_LBL_GPICKUP                                           	118
#    define SKINNY_LBL_YOUR_CURRENT_OPTIONS                              	119
#    define SKINNY_LBL_OFF_HOOK                                          	120
#    define SKINNY_LBL_ON_HOOK                                           	121
#    define SKINNY_LBL_RING_OUT                                          	122
#    define SKINNY_LBL_FROM                                              	123
#    define SKINNY_LBL_CONNECTED                                         	124
#    define SKINNY_LBL_BUSY                                              	125
#    define SKINNY_LBL_LINE_IN_USE                                       	126
#    define SKINNY_LBL_CALL_WAITING                                      	127
#    define SKINNY_LBL_CALL_TRANSFER                                     	128
#    define SKINNY_LBL_CALL_PARK                                         	129
#    define SKINNY_LBL_CALL_PROCEED                                      	130
#    define SKINNY_LBL_IN_USE_REMOTE                                     	131
#    define SKINNY_LBL_ENTER_NUMBER                                      	132
#    define SKINNY_LBL_CALL_PARK_AT                                      	133
#    define SKINNY_LBL_PRIMARY_ONLY                                      	134
#    define SKINNY_LBL_TEMP_FAIL                                         	135
#    define SKINNY_LBL_YOU_HAVE_VOICEMAIL                                	136
#    define SKINNY_LBL_FORWARDED_TO                                      	137
#    define SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE                       	138
#    define SKINNY_LBL_NO_CONFERENCE_BRIDGE                              	139
#    define SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL                      	140
#    define SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT                    	141
#    define SKINNY_LBL_IN_CONFERENCE_ALREADY                             	142
#    define SKINNY_LBL_NO_PARTICIPANT_INFO                               	143
#    define SKINNY_LBL_EXCEED_MAXIMUM_PARTIES                            	144
#    define SKINNY_LBL_KEY_IS_NOT_ACTIVE                                 	145
#    define SKINNY_LBL_ERROR_NO_LICENSE                                  	146
#    define SKINNY_LBL_ERROR_DBCONFIG                                    	147
#    define SKINNY_LBL_ERROR_DATABASE                                    	148
#    define SKINNY_LBL_ERROR_PASS_LIMIT                                  	149
#    define SKINNY_LBL_ERROR_UNKNOWN                                     	150
#    define SKINNY_LBL_ERROR_MISMATCH                                    	151
#    define SKINNY_LBL_CONFERENCE                                        	152
#    define SKINNY_LBL_PARK_NUMBER                                       	153
#    define SKINNY_LBL_PRIVATE                                           	154
#    define SKINNY_LBL_NOT_ENOUGH_BANDWIDTH                              	155
#    define SKINNY_LBL_UNKNOWN_NUMBER                                    	156
#    define SKINNY_LBL_RMLSTC                                            	157
#    define SKINNY_LBL_VOICEMAIL                                         	158
#    define SKINNY_LBL_IMMDIV                                            	159
#    define SKINNY_LBL_INTRCPT                                           	160
#    define SKINNY_LBL_SETWTCH                                           	161
#    define SKINNY_LBL_TRNSFVM                                           	162
#    define SKINNY_LBL_DND                                               	163
#    define SKINNY_LBL_DIVALL                                            	164
#    define SKINNY_LBL_CALLBACK                                          	165
#    define SKINNY_LBL_NETWORK_CONGESTION_REROUTING                      	166
#    define SKINNY_LBL_BARGE                                             	167
#    define SKINNY_LBL_FAILED_TO_SETUP_BARGE                             	168
#    define SKINNY_LBL_ANOTHER_BARGE_EXISTS                              	169
#    define SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE                          	170
#    define SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE                          	171
#    define SKINNY_LBL_CALLPARK_REVERSION                                	172
#    define SKINNY_LBL_SERVICE_IS_NOT_ACTIVE                             	173
#    define SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER                      	174
#    define SKINNY_LBL_QRT                                               	175
#    define SKINNY_LBL_MCID                                              	176
#    define SKINNY_LBL_DIRTRFR                                           	177
#    define SKINNY_LBL_SELECT                                            	178
#    define SKINNY_LBL_CONFLIST                                          	179
#    define SKINNY_LBL_IDIVERT                                           	180
#    define SKINNY_LBL_CBARGE                                            	181
#    define SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER                         	182
#    define SKINNY_LBL_CAN_NOT_JOIN_CALLS                                	183
#    define SKINNY_LBL_MCID_SUCCESSFUL                                   	184
#    define SKINNY_LBL_NUMBER_NOT_CONFIGURED                             	185
#    define SKINNY_LBL_SECURITY_ERROR                                    	186
#    define SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE                       	187
#    define SKINNY_LBL_VIDEO_MODE					        188

/*!
 * \brief Skinny LABEL Structure
 */
static const struct skinny_label {
	uint16_t label;
	const char *const text;
} skinny_labels[] = {
	/* *INDENT-OFF* */
	{SKINNY_LBL_EMPTY, ""},
	{SKINNY_LBL_ACCT, "Acct"},
	{SKINNY_LBL_FLASH, "Flash"},
	{SKINNY_LBL_LOGIN, "Login"},
	{SKINNY_LBL_DEVICE_IN_HOME_LOCATION, "Device in home location"},
	{SKINNY_LBL_DEVICE_IN_ROAMING_LOCATION, "Device in roaming location"},
	{SKINNY_LBL_ENTER_AUTHORIZATION_CODE, "Enter authorization code"},
	{SKINNY_LBL_ENTER_CLIENT_MATTER_CODE, "Enter client matter code"},
	{SKINNY_LBL_CALL_S_AVAILABLE_FOR_PICKUP, "Calls available for pickup"},
	{SKINNY_LBL_CM_FALLBACK_SERVICE_OPERATING, "Call Manager Fallback Service Operating"},
	{SKINNY_LBL_MAX_PHONES_EXCEEDED, "Maximum number of phone exceeded"},
	{SKINNY_LBL_WAITING_TO_RE_HOME, "Waiting to be rehomed"},
	{SKINNY_LBL_PLEASE_END_CALL, "Please END Call"},
	{SKINNY_LBL_PAGING, "Paging"},
	{SKINNY_LBL_SELECT_LINE, "Select Line"},
	{SKINNY_LBL_TRANSFER_DESTINATION_IS_BUSY, "Tranfer destination is busy"},
	{SKINNY_LBL_SELECT_A_SERVICE, "Select a service"},
	{SKINNY_LBL_LOCAL_SERVICES, "Local Services"},
	{SKINNY_LBL_ENTER_SEARCH_CRITERIA, "Enter Search Criteria"},
	{SKINNY_LBL_NIGHT_SERVICE, "Night Service"},
	{SKINNY_LBL_NIGHT_SERVICE_ACTIVE, "Night Service Active"},
	{SKINNY_LBL_NIGHT_SERVICE_DISABLED, "Night Service Disabled"},
	{SKINNY_LBL_LOGIN_SUCCESSFUL, "Login Successfull"},
	{SKINNY_LBL_WRONG_PIN, "Wrong Pin"},
	{SKINNY_LBL_PLEASE_ENTER_PIN, "Please enter pin"},
	{SKINNY_LBL_OF, "Of"},
	{SKINNY_LBL_RECORDS_1_TO, "Records 1 to "},
	{SKINNY_LBL_NO_RECORD_FOUND, "No records found"},
	{SKINNY_LBL_SEARCH_RESULTS, "Search Results"},
	{SKINNY_LBL_CALL_S_IN_QUEUE, "Calls in Queue"},
	{SKINNY_LBL_JOIN_TO_HUNT_GROUP, "Join to Hunt Group"},
	{SKINNY_LBL_READY, "Ready"},
	{SKINNY_LBL_NOTREADY, "Not Ready"},
	{SKINNY_LBL_CALL_ON_HOLD, "Call on hold"},
	{SKINNY_LBL_HOLD_REVERSION, "Hold Reversion"},
	{SKINNY_LBL_SETUP_FAILED, "Setup Failed"},
	{SKINNY_LBL_NO_RESOURCES, "No Resources"},
	{SKINNY_LBL_DEVICE_NOT_AUTHORIZED, "Device not authorized"},
	{SKINNY_LBL_MONITORING, "Monitoring"},
	{SKINNY_LBL_RECORDING_AWAITING_CALL_TO_BE_ACTIVE, "Recording awaiting call to be active"},
	{SKINNY_LBL_RECORDING_ALREADY_IN_PROGRESS, "Recording already in progress"},
	{SKINNY_LBL_INACTIVE_RECORDING_SESSION, "Inactive recording session"},
	{SKINNY_LBL_MOBILITY, "Mobility"},
	{SKINNY_LBL_WHISPER, "Whisper"},
	{SKINNY_LBL_FORWARD_ALL, "Forward All"},
	{SKINNY_LBL_MALICIOUS_CALL_ID, "Malicious Call ID"},
	{SKINNY_LBL_GROUP_PICKUP, "Group Pickup"},
	{SKINNY_LBL_REMOVE_LAST_PARTICIPANT, "Remove last participant"},
	{SKINNY_LBL_OTHER_PICKUP, "Other Pickup"},
	{SKINNY_LBL_VIDEO, "Video"},
	{SKINNY_LBL_END_CALL, "End Call"},
	{SKINNY_LBL_CONFERENCE_LIST, "Conference List"},
	{SKINNY_LBL_QUALITY_REPORTING_TOOL, "Quality Reporting Tool"},
	{SKINNY_LBL_HUNT_GROUP, "Hunt Group"},
	{SKINNY_LBL_USE_LINE_OR_JOIN_TO_COMPLETE, "Use line or join to complete"},
	{SKINNY_LBL_DO_NOT_DISTURB, "Do not disturb"},
	{SKINNY_LBL_DO_NOT_DISTURB_IS_ACTIVE, "Do not disturb active"},
	{SKINNY_LBL_CFWDALL_LOOP_DETECTED, "CFWDALL Loop Detected"},
	{SKINNY_LBL_CFWDALL_HOPS_EXCEEDED, "CFWDALL # Hops Exceeded"},
	{SKINNY_LBL_ABBRDIAL, "AbbrDial"},
	{SKINNY_LBL_PICKUP_IS_UNAVAILABLE, "Pickup is unavailable"},
	{SKINNY_LBL_CONFERENCE_IS_UNAVAILABLE, "Conference in unavailable"},
	{SKINNY_LBL_MEETME_IS_UNAVAILABLE, "Meetme is unavailable"},
	{SKINNY_LBL_CANNOT_RETRIEVE_PARKED_CALL, "Cannot Retrieve Parked Call"},
	{SKINNY_LBL_CANNOT_SEND_CALL_TO_MOBILE, "Cannot send call to mobile"},
	{SKINNY_LBL_RECORD, "Record"},
	{SKINNY_LBL_CANNOT_MOVE_CONVERSATION, "Cannot move conversation"},
	{SKINNY_LBL_CW_OFF, "Call Waiting Off"},
	{SKINNY_LBL_RECORDING, "Recording"},
	{SKINNY_LBL_RECORDING_FAILED, "Recording Failed"},
	{SKINNY_LBL_CONNECTING_PLEASE_WAIT, "Connecting please wait"},
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
#    define SKINNY_DISP_ACCT						"\36\2"	/*<! Copied from FS / KFM until DISP "\36\121" */
#    define SKINNY_DISP_FLASH						"\36\3"
#    define SKINNY_DISP_LOGIN						"\36\4"
#    define SKINNY_DISP_DEVICE_IN_HOME_LOCATION				"\36\5"
#    define SKINNY_DISP_DEVICE_IN_ROAMING_LOCATION			"\36\6"
#    define SKINNY_DISP_ENTER_AUTHORIZATION_CODE			"\36\7"

#    define SKINNY_DISP_ENTER_CLIENT_MATTER_CODE			"\36\10"
#    define SKINNY_DISP_CALL_S_AVAILABLE_FOR_PICKUP			"\36\11"
#    define SKINNY_DISP_CM_FALLBACK_SERVICE_OPERATING			"\36\12"
#    define SKINNY_DISP_MAX_PHONES_EXCEEDED				"\36\13"
#    define SKINNY_DISP_WAITING_TO_RE_HOME				"\36\14"
#    define SKINNY_DISP_PLEASE_END_CALL					"\36\15"
#    define SKINNY_DISP_PAGING						"\36\16"
#    define SKINNY_DISP_SELECT_LINE					"\36\17"

#    define SKINNY_DISP_TRANSFER_DESTINATION_IS_BUSY			"\36\20"
#    define SKINNY_DISP_SELECT_A_SERVICE				"\36\21"
#    define SKINNY_DISP_LOCAL_SERVICES					"\36\22"
#    define SKINNY_DISP_ENTER_SEARCH_CRITERIA				"\36\23"
#    define SKINNY_DISP_NIGHT_SERVICE					"\36\24"
#    define SKINNY_DISP_NIGHT_SERVICE_ACTIVE				"\36\25"
#    define SKINNY_DISP_NIGHT_SERVICE_DISABLED				"\36\26"
#    define SKINNY_DISP_LOGIN_SUCCESSFUL				"\36\27"

#    define SKINNY_DISP_WRONG_PIN					"\36\30"
#    define SKINNY_DISP_PLEASE_ENTER_PIN				"\36\31"
#    define SKINNY_DISP_OF						"\36\32"
#    define SKINNY_DISP_RECORDS_1_TO					"\36\33"
#    define SKINNY_DISP_NO_RECORD_FOUND					"\36\34"
#    define SKINNY_DISP_SEARCH_RESULTS					"\36\35"
#    define SKINNY_DISP_CALL_S_IN_QUEUE					"\36\36"
#    define SKINNY_DISP_JOIN_TO_HUNT_GROUP				"\36\37"

#    define SKINNY_DISP_READY						"\36\40"
#    define SKINNY_DISP_NOTREADY					"\36\41"
#    define SKINNY_DISP_CALL_ON_HOLD					"\36\42"
#    define SKINNY_DISP_HOLD_REVERSION					"\36\43"
#    define SKINNY_DISP_SETUP_FAILED					"\36\44"
#    define SKINNY_DISP_NO_RESOURCES					"\36\45"
#    define SKINNY_DISP_DEVICE_NOT_AUTHORIZED				"\36\46"
#    define SKINNY_DISP_MONITORING					"\36\47"

#    define SKINNY_DISP_RECORDING_AWAITING_CALL_TO_BE_ACTIVE		"\36\50"
#    define SKINNY_DISP_RECORDING_ALREADY_IN_PROGRESS			"\36\51"
#    define SKINNY_DISP_INACTIVE_RECORDING_SESSION			"\36\52"
#    define SKINNY_DISP_MOBILITY					"\36\53"
#    define SKINNY_DISP_WHISPER						"\36\54"
#    define SKINNY_DISP_FORWARD_ALL					"\36\55"
#    define SKINNY_DISP_MALICIOUS_CALL_ID				"\36\56"
#    define SKINNY_DISP_GROUP_PICKUP					"\36\57"

#    define SKINNY_DISP_REMOVE_LAST_PARTICIPANT				"\36\60"
#    define SKINNY_DISP_OTHER_PICKUP					"\36\61"
#    define SKINNY_DISP_VIDEO						"\36\62"
#    define SKINNY_DISP_END_CALL					"\36\63"
#    define SKINNY_DISP_CONFERENCE_LIST					"\36\64"
#    define SKINNY_DISP_QUALITY_REPORTING_TOOL				"\36\65"
#    define SKINNY_DISP_HUNT_GROUP					"\36\66"
#    define SKINNY_DISP_USE_LINE_OR_JOIN_TO_COMPLETE			"\36\67"

#    define SKINNY_DISP_DO_NOT_DISTURB					"\36\70"
#    define SKINNY_DISP_DO_NOT_DISTURB_IS_ACTIVE			"\36\71"
#    define SKINNY_DISP_CFWDALL_LOOP_DETECTED				"\36\72"
#    define SKINNY_DISP_CFWDALL_HOPS_EXCEEDED				"\36\73"
#    define SKINNY_DISP_ABBRDIAL					"\36\74"
#    define SKINNY_DISP_PICKUP_IS_UNAVAILABLE				"\36\75"
#    define SKINNY_DISP_CONFERENCE_IS_UNAVAILABLE			"\36\76"
#    define SKINNY_DISP_MEETME_IS_UNAVAILABLE				"\36\77"

#    define SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL			"\36\100"
#    define SKINNY_DISP_CANNOT_SEND_CALL_TO_MOBILE			"\36\101"
#    define SKINNY_DISP_RECORD						"\36\103"
#    define SKINNY_DISP_CANNOT_MOVE_CONVERSATION			"\36\104"
#    define SKINNY_DISP_CW_OFF						"\36\105"
#    define SKINNY_DISP_RECORDING					"\36\117"

#    define SKINNY_DISP_RECORDING_FAILED				"\36\120"
#    define SKINNY_DISP_CONNECTING_PLEASE_WAIT				"\36\121"	/*<! Copied from FS / KFM starting at DISP "\36\2" */

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
#    define SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP                	"\200\137"	/*<! - FS */
#    define SKINNY_DISP_PARK_SLOT_UNAVAILABLE                       	"\200\136"	/*<! - FS */
#    define SKINNY_DISP_LOGGED_OUT_OF_HUNT_GROUP                    	"\200\135"	/*<! - FS */
#    define SKINNY_DISP_HLOG                                        	"\200\134"	/*<! - FS */

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
	const char *skinny_disp;
	const char *message;
} ast_skinny_causes[] = {
	/* *INDENT-OFF* */
        { AST_CAUSE_UNALLOCATED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED		, "Unallocated (unassigned) number" },
        { AST_CAUSE_NO_ROUTE_TRANSIT_NET,	SKINNY_DISP_UNKNOWN_NUMBER			, "No route to specified transmit network" },
        { AST_CAUSE_NO_ROUTE_DESTINATION,	SKINNY_DISP_UNKNOWN_NUMBER			, "No route to destination" },
        { AST_CAUSE_CHANNEL_UNACCEPTABLE,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Channel unacceptable" },   
        { AST_CAUSE_CALL_AWARDED_DELIVERED,	SKINNY_DISP_CONNECTED				, "Call awarded and being delivered in an established channel" },
  	{ AST_CAUSE_NORMAL_CLEARING,		SKINNY_DISP_ON_HOOK 				, "Normal Clearing" },
        { AST_CAUSE_USER_BUSY, 			SKINNY_DISP_BUSY				, "User busy" },
	{ AST_CAUSE_NO_USER_RESPONSE,		SKINNY_DISP_EMPTY				, "No user responding" },
        { AST_CAUSE_NO_ANSWER,			SKINNY_DISP_EMPTY				, "User alerting, no answer" },
        { AST_CAUSE_CALL_REJECTED,		SKINNY_DISP_BUSY				, "Call Rejected" },   
	{ AST_CAUSE_NUMBER_CHANGED,		SKINNY_DISP_NUMBER_NOT_CONFIGURED		, "Number changed" },
        { AST_CAUSE_DESTINATION_OUT_OF_ORDER,	SKINNY_DISP_TEMP_FAIL				, "Destination out of order" },
        { AST_CAUSE_INVALID_NUMBER_FORMAT, 	SKINNY_DISP_UNKNOWN_NUMBER 			, "Invalid number format" },
        { AST_CAUSE_FACILITY_REJECTED,		SKINNY_DISP_TEMP_FAIL				, "Facility rejected" },
	{ AST_CAUSE_RESPONSE_TO_STATUS_ENQUIRY,	SKINNY_DISP_TEMP_FAIL				, "Response to STATus ENQuiry" },
        { AST_CAUSE_NORMAL_UNSPECIFIED,		SKINNY_DISP_TEMP_FAIL				, "Normal, unspecified" },
  	{ AST_CAUSE_NORMAL_CIRCUIT_CONGESTION,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Circuit/channel congestion" },
        { AST_CAUSE_NETWORK_OUT_OF_ORDER, 	SKINNY_DISP_NETWORK_CONGESTION_REROUTING	, "Network out of order" },
  	{ AST_CAUSE_NORMAL_TEMPORARY_FAILURE, 	SKINNY_DISP_TEMP_FAIL				, "Temporary failure" },
        { AST_CAUSE_SWITCH_CONGESTION,		SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Switching equipment congestion" }, 
	{ AST_CAUSE_ACCESS_INFO_DISCARDED,	SKINNY_DISP_SECURITY_ERROR			, "Access information discarded" },
        { AST_CAUSE_REQUESTED_CHAN_UNAVAIL,	SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Requested channel not available" },
	{ AST_CAUSE_PRE_EMPTED,			SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER	, "Pre-empted" },
        { AST_CAUSE_FACILITY_NOT_SUBSCRIBED,	SKINNY_DISP_TEMP_FAIL				, "Facility not subscribed" },
	{ AST_CAUSE_OUTGOING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR			, "Outgoing call barred" },
        { AST_CAUSE_INCOMING_CALL_BARRED,	SKINNY_DISP_SECURITY_ERROR			, "Incoming call barred" },
	{ AST_CAUSE_BEARERCAPABILITY_NOTAUTH,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not authorized" },
        { AST_CAUSE_BEARERCAPABILITY_NOTAVAIL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not available" },
        { AST_CAUSE_BEARERCAPABILITY_NOTIMPL,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Bearer capability not implemented" },
        { AST_CAUSE_CHAN_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Channel not implemented" },
	{ AST_CAUSE_FACILITY_NOT_IMPLEMENTED,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Facility not implemented" },
        { AST_CAUSE_INVALID_CALL_REFERENCE,	SKINNY_DISP_TEMP_FAIL				, "Invalid call reference value" },
	{ AST_CAUSE_INCOMPATIBLE_DESTINATION,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Incompatible destination" },
        { AST_CAUSE_INVALID_MSG_UNSPECIFIED,	SKINNY_DISP_ERROR_UNKNOWN			, "Invalid message unspecified" },
        { AST_CAUSE_MANDATORY_IE_MISSING,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL	, "Mandatory information element is missing" },
        { AST_CAUSE_MESSAGE_TYPE_NONEXIST,	SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL	, "Message type nonexist." },
  	{ AST_CAUSE_WRONG_MESSAGE,		SKINNY_DISP_ERROR_MISMATCH			, "Wrong message" },
        { AST_CAUSE_IE_NONEXIST,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Info. element nonexist or not implemented" },
	{ AST_CAUSE_INVALID_IE_CONTENTS,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Invalid information element contents" },
        { AST_CAUSE_WRONG_CALL_STATE,		SKINNY_DISP_TEMP_FAIL				, "Message not compatible with call state" },
	{ AST_CAUSE_RECOVERY_ON_TIMER_EXPIRE,	SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT		, "Recover on timer expiry" },
        { AST_CAUSE_MANDATORY_IE_LENGTH_ERROR,	SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Mandatory IE length error" },
	{ AST_CAUSE_PROTOCOL_ERROR,		SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE		, "Protocol error, unspecified" },
        { AST_CAUSE_INTERWORKING,		SKINNY_DISP_NETWORK_CONGESTION_REROUTING	, "Interworking, unspecified" },
	/* *INDENT-ON* */
};
#endif
