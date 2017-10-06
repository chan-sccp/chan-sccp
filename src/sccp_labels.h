/*!
 * \file        sccp_labels.h
 * \brief       SCCP Labels Header
 * 
 * SCCP Button Number References and SCCP Display Number References 
 *
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#define SKINNY_LBL_EMPTY                                             	0					/*< fake button */
#define SKINNY_LBL_REDIAL                                            	1
#define SKINNY_LBL_NEWCALL                                           	2
#define SKINNY_LBL_HOLD                                              	3
#define SKINNY_LBL_TRANSFER                                          	4
#define SKINNY_LBL_CFWDALL                                           	5
#define SKINNY_LBL_CFWDBUSY                                          	6
#define SKINNY_LBL_CFWDNOANSWER                                      	7
#define SKINNY_LBL_BACKSPACE                                         	8
#define SKINNY_LBL_ENDCALL                                           	9
#define SKINNY_LBL_RESUME                                            	10
#define SKINNY_LBL_ANSWER                                            	11
#define SKINNY_LBL_INFO                                              	12
#define SKINNY_LBL_CONFRN                                            	13
#define SKINNY_LBL_PARK                                              	14
#define SKINNY_LBL_JOIN                                              	15
#define SKINNY_LBL_MEETME                                            	16
#define SKINNY_LBL_PICKUP                                            	17
#define SKINNY_LBL_GPICKUP                                           	18
#define SKINNY_LBL_YOUR_CURRENT_OPTIONS                              	19
#define SKINNY_LBL_OFF_HOOK                                          	20
#define SKINNY_LBL_ON_HOOK                                           	21
#define SKINNY_LBL_RING_OUT                                          	22
#define SKINNY_LBL_FROM                                              	23
#define SKINNY_LBL_CONNECTED                                         	24
#define SKINNY_LBL_BUSY                                              	25
#define SKINNY_LBL_LINE_IN_USE                                       	26
#define SKINNY_LBL_CALL_WAITING                                      	27
#define SKINNY_LBL_CALL_TRANSFER                                     	28
#define SKINNY_LBL_CALL_PARK                                         	29
#define SKINNY_LBL_CALL_PROCEED                                      	30
#define SKINNY_LBL_IN_USE_REMOTE                                     	31
#define SKINNY_LBL_ENTER_NUMBER                                      	32
#define SKINNY_LBL_CALL_PARK_AT                                      	33
#define SKINNY_LBL_PRIMARY_ONLY                                      	34
#define SKINNY_LBL_TEMP_FAIL                                         	35
#define SKINNY_LBL_YOU_HAVE_VOICEMAIL                                	36
#define SKINNY_LBL_FORWARDED_TO                                      	37
#define SKINNY_LBL_CAN_NOT_COMPLETE_CONFERENCE                       	38
#define SKINNY_LBL_NO_CONFERENCE_BRIDGE                              	39
#define SKINNY_LBL_CAN_NOT_HOLD_PRIMARY_CONTROL                      	40
#define SKINNY_LBL_INVALID_CONFERENCE_PARTICIPANT                    	41
#define SKINNY_LBL_IN_CONFERENCE_ALREADY                             	42
#define SKINNY_LBL_NO_PARTICIPANT_INFO                               	43
#define SKINNY_LBL_EXCEED_MAXIMUM_PARTIES                            	44
#define SKINNY_LBL_KEY_IS_NOT_ACTIVE                                 	45
#define SKINNY_LBL_ERROR_NO_LICENSE                                  	46
#define SKINNY_LBL_ERROR_DBCONFIG                                    	47
#define SKINNY_LBL_ERROR_DATABASE                                    	48
#define SKINNY_LBL_ERROR_PASS_LIMIT                                  	49
#define SKINNY_LBL_ERROR_UNKNOWN                                     	50
#define SKINNY_LBL_ERROR_MISMATCH                                    	51
#define SKINNY_LBL_CONFERENCE                                        	52
#define SKINNY_LBL_PARK_NUMBER                                       	53
#define SKINNY_LBL_PRIVATE                                           	54
#define SKINNY_LBL_NOT_ENOUGH_BANDWIDTH                              	55
#define SKINNY_LBL_UNKNOWN_NUMBER                                    	56
#define SKINNY_LBL_RMLSTC                                            	57					/* Remove Last Conference Participant from the Conference (Moderator Only) */
#define SKINNY_LBL_VOICEMAIL                                         	58
#define SKINNY_LBL_IMMDIV                                            	59					/* Immediate Divert to Voicemail */
#define SKINNY_LBL_INTRCPT                                           	60
#define SKINNY_LBL_SETWTCH                                           	61
#define SKINNY_LBL_TRNSFVM                                           	62
#define SKINNY_LBL_DND                                               	63
#define SKINNY_LBL_DIVALL                                            	64
#define SKINNY_LBL_CALLBACK                                          	65
#define SKINNY_LBL_NETWORK_CONGESTION_REROUTING                      	66
#define SKINNY_LBL_BARGE                                             	67
#define SKINNY_LBL_FAILED_TO_SETUP_BARGE                             	68
#define SKINNY_LBL_ANOTHER_BARGE_EXISTS                              	69
#define SKINNY_LBL_INCOMPATIBLE_DEVICE_TYPE                          	70
#define SKINNY_LBL_NO_PARK_NUMBER_AVAILABLE                          	71
#define SKINNY_LBL_CALLPARK_REVERSION                                	72
#define SKINNY_LBL_SERVICE_IS_NOT_ACTIVE                             	73
#define SKINNY_LBL_HIGH_TRAFFIC_TRY_AGAIN_LATER                      	74
#define SKINNY_LBL_QRT                                               	75
#define SKINNY_LBL_MCID                                              	76
#define SKINNY_LBL_DIRTRFR                                           	77
#define SKINNY_LBL_SELECT                                            	78
#define SKINNY_LBL_CONFLIST                                          	79
#define SKINNY_LBL_IDIVERT                                           	80
#define SKINNY_LBL_CBARGE                                            	81
#define SKINNY_LBL_CAN_NOT_COMPLETE_TRANSFER                         	82
#define SKINNY_LBL_CAN_NOT_JOIN_CALLS                                	83
#define SKINNY_LBL_MCID_SUCCESSFUL                                   	84
#define SKINNY_LBL_NUMBER_NOT_CONFIGURED                             	85
#define SKINNY_LBL_SECURITY_ERROR                                    	86
#define SKINNY_LBL_VIDEO_BANDWIDTH_UNAVAILABLE                       	87
#define SKINNY_LBL_VIDEO_MODE						88
#define SKINNY_LBL_QUEUE						100
#define SKINNY_LBL_DIAL							201
#define SKINNY_LBL_MONITOR						202

SCCP_INLINE const char * SCCP_CALL label2str(uint16_t value);
SCCP_INLINE SCCP_CALL uint32_t labelstr2int(const char *str);

#define SKINNY_DISP_EMPTY                                		""

#define SKINNY_DISP_ACCT						"\36\02"				/* Set Account Code / During Call Setup or Connected Call / Should be added to CDR */
#define SKINNY_DISP_FLASH						"\36\03"				/* Hook Flash */
#define SKINNY_DISP_LOGIN						"\36\04"				/* Provides personal identification number (PIN) access to restricted phone features */
#define SKINNY_DISP_DEVICE_IN_HOME_LOCATION				"\36\05"
#define SKINNY_DISP_DEVICE_IN_ROAMING_LOCATION				"\36\06"
#define SKINNY_DISP_ENTER_AUTHORIZATION_CODE				"\36\07"

#define SKINNY_DISP_ENTER_CLIENT_MATTER_CODE				"\36\10"
#define SKINNY_DISP_CALLS_AVAILABLE_FOR_PICKUP				"\36\11"
#define SKINNY_DISP_CM_FALLBACK_SERVICE_OPERATING			"\36\12"
#define SKINNY_DISP_MAX_PHONES_EXCEEDED					"\36\13"
#define SKINNY_DISP_WAITING_TO_REHOME					"\36\14"
#define SKINNY_DISP_PLEASE_END_CALL					"\36\15"
#define SKINNY_DISP_PAGING						"\36\16"
#define SKINNY_DISP_SELECT_LINE						"\36\17"

#define SKINNY_DISP_TRANSFER_DESTINATION_IS_BUSY			"\36\20"
#define SKINNY_DISP_SELECT_A_SERVICE					"\36\21"
#define SKINNY_DISP_LOCAL_SERVICES					"\36\22"
#define SKINNY_DISP_ENTER_SEARCH_CRITERIA				"\36\23"
#define SKINNY_DISP_NIGHT_SERVICE					"\36\24"				/* We should provide access to Night Service via a Feature Option */
#define SKINNY_DISP_NIGHT_SERVICE_ACTIVE				"\36\25"
#define SKINNY_DISP_NIGHT_SERVICE_DISABLED				"\36\26"
#define SKINNY_DISP_LOGIN_SUCCESSFUL					"\36\27"

#define SKINNY_DISP_WRONG_PIN						"\36\30"
#define SKINNY_DISP_PLEASE_ENTER_PIN					"\36\31"
#define SKINNY_DISP_OF							"\36\32"
#define SKINNY_DISP_RECORDS_1_TO					"\36\33"
#define SKINNY_DISP_NO_RECORD_FOUND					"\36\34"
#define SKINNY_DISP_SEARCH_RESULTS					"\36\35"
#define SKINNY_DISP_CALLS_IN_QUEUE					"\36\36"
#define SKINNY_DISP_JOIN_TO_HUNT_GROUP					"\36\37"

#define SKINNY_DISP_READY						"\36\40"
#define SKINNY_DISP_NOTREADY						"\36\41"
#define SKINNY_DISP_CALL_ON_HOLD					"\36\42"
#define SKINNY_DISP_HOLD_REVERSION					"\36\43"
#define SKINNY_DISP_SETUP_FAILED					"\36\44"
#define SKINNY_DISP_NO_RESOURCES					"\36\45"
#define SKINNY_DISP_DEVICE_NOT_AUTHORIZED				"\36\46"
#define SKINNY_DISP_MONITORING						"\36\47"

#define SKINNY_DISP_RECORDING_AWAITING_CALL_TO_BE_ACTIVE		"\36\50"
#define SKINNY_DISP_RECORDING_ALREADY_IN_PROGRESS			"\36\51"
#define SKINNY_DISP_INACTIVE_RECORDING_SESSION				"\36\52"
#define SKINNY_DISP_MOBILITY						"\36\53"
#define SKINNY_DISP_WHISPER						"\36\54"
#define SKINNY_DISP_FORWARD_ALL						"\36\55"
#define SKINNY_DISP_MALICIOUS_CALL_ID					"\36\56"
#define SKINNY_DISP_GROUP_PICKUP					"\36\57"

#define SKINNY_DISP_REMOVE_LAST_PARTICIPANT				"\36\60"
#define SKINNY_DISP_OTHER_PICKUP					"\36\61"
#define SKINNY_DISP_VIDEO						"\36\62"
#define SKINNY_DISP_END_CALL						"\36\63"
#define SKINNY_DISP_CONFERENCE_LIST					"\36\64"
#define SKINNY_DISP_QUALITY_REPORTING_TOOL				"\36\65"
#define SKINNY_DISP_HUNT_GROUP						"\36\66"				/* HLOG Button */
#define SKINNY_DISP_USE_LINE_OR_JOIN_TO_COMPLETE			"\36\67"

#define SKINNY_DISP_DO_NOT_DISTURB					"\36\70"
#define SKINNY_DISP_DO_NOT_DISTURB_IS_ACTIVE				"\36\71"
#define SKINNY_DISP_CFWDALL_LOOP_DETECTED				"\36\72"
#define SKINNY_DISP_CFWDALL_HOPS_EXCEEDED				"\36\73"
#define SKINNY_DISP_ABBRDIAL						"\36\74"
#define SKINNY_DISP_PICKUP_IS_UNAVAILABLE				"\36\75"
#define SKINNY_DISP_CONFERENCE_IS_UNAVAILABLE				"\36\76"
#define SKINNY_DISP_MEETME_IS_UNAVAILABLE				"\36\77"

#define SKINNY_DISP_CANNOT_RETRIEVE_PARKED_CALL				"\36\100"
#define SKINNY_DISP_CANNOT_SEND_CALL_TO_MOBILE				"\36\101"
#define SKINNY_DISP_RECORD						"\36\103"
#define SKINNY_DISP_CANNOT_MOVE_CONVERSATION				"\36\104"
#define SKINNY_DISP_CW_OFF						"\36\105"				/* Call Waiting Off */
#define SKINNY_DISP_COACHING						"\36\106"
#define SKINNY_DISP_RECORDING						"\36\117"

#define SKINNY_DISP_RECORDING_FAILED					"\36\120"
#define SKINNY_DISP_CONNECTING						"\36\121"

#define SKINNY_DISP_REDIAL                                		"\200\1"
#define SKINNY_DISP_NEWCALL                               		"\200\2"
#define SKINNY_DISP_HOLD                                  		"\200\3"
#define SKINNY_DISP_TRANSFER                              		"\200\4"
#define SKINNY_DISP_CFWDALL                               		"\200\5"
#define SKINNY_DISP_CFWDBUSY                              		"\200\6"
#define SKINNY_DISP_CFWDNOANSWER                          		"\200\7"

#define SKINNY_DISP_BACKSPACE                             		"\200\10"
#define SKINNY_DISP_ENDCALL                               		"\200\11"
#define SKINNY_DISP_RESUME                                		"\200\12"
#define SKINNY_DISP_ANSWER                                		"\200\13"
#define SKINNY_DISP_INFO                                  		"\200\14"
#define SKINNY_DISP_CONFRN                                		"\200\15"
#define SKINNY_DISP_PARK                                  		"\200\16"
#define SKINNY_DISP_JOIN                                  		"\200\17"

#define SKINNY_DISP_MEETME                                		"\200\20"
#define SKINNY_DISP_PICKUP                                		"\200\21"
#define SKINNY_DISP_GPICKUP                               		"\200\22"
#define SKINNY_DISP_YOUR_CURRENT_OPTIONS                  		"\200\23"
#define SKINNY_DISP_OFF_HOOK                              		"\200\24"
#define SKINNY_DISP_ON_HOOK                               		"\200\25"
#define SKINNY_DISP_RING_OUT                              		"\200\26"
#define SKINNY_DISP_FROM                                  		"\200\27"

#define SKINNY_DISP_CONNECTED                             		"\200\30"
#define SKINNY_DISP_BUSY                                  		"\200\31"
#define SKINNY_DISP_LINE_IN_USE                           		"\200\32"
#define SKINNY_DISP_CALL_WAITING                          		"\200\33"
#define SKINNY_DISP_CALL_TRANSFER                         		"\200\34"
#define SKINNY_DISP_CALL_PARK                             		"\200\35"
#define SKINNY_DISP_CALL_PROCEED                          		"\200\36"
#define SKINNY_DISP_IN_USE_REMOTE                         		"\200\37"

#define SKINNY_DISP_ENTER_NUMBER                          		"\200\40"
#define SKINNY_DISP_CALL_PARK_AT                          		"\200\41"
#define SKINNY_DISP_PRIMARY_ONLY                          		"\200\42"
#define SKINNY_DISP_TEMP_FAIL                             		"\200\43"
#define SKINNY_DISP_YOU_HAVE_VOICEMAIL                    		"\200\44"
#define SKINNY_DISP_FORWARDED_TO                          		"\200\45"
#define SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE           		"\200\46"
#define SKINNY_DISP_NO_CONFERENCE_BRIDGE                  		"\200\47"

#define SKINNY_DISP_CAN_NOT_HOLD_PRIMARY_CONTROL          		"\200\50"
#define SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT        		"\200\51"
#define SKINNY_DISP_IN_CONFERENCE_ALREADY                 		"\200\52"
#define SKINNY_DISP_NO_PARTICIPANT_INFO                   		"\200\53"
#define SKINNY_DISP_EXCEED_MAXIMUM_PARTIES                		"\200\54"
#define SKINNY_DISP_KEY_IS_NOT_ACTIVE                     		"\200\55"
#define SKINNY_DISP_ERROR_NO_LICENSE                      		"\200\56"
#define SKINNY_DISP_ERROR_DBCONFIG                        		"\200\57"

#define SKINNY_DISP_ERROR_DATABASE                        		"\200\60"
#define SKINNY_DISP_ERROR_PASS_LIMIT                      		"\200\61"
#define SKINNY_DISP_ERROR_UNKNOWN                         		"\200\62"
#define SKINNY_DISP_ERROR_MISMATCH                        		"\200\63"
#define SKINNY_DISP_CONFERENCE                            		"\200\64"
#define SKINNY_DISP_PARK_NUMBER                           		"\200\65"
#define SKINNY_DISP_PRIVATE                               		"\200\66"
#define SKINNY_DISP_NOT_ENOUGH_BANDWIDTH                  		"\200\67"

#define SKINNY_DISP_UNKNOWN_NUMBER                        		"\200\70"
#define SKINNY_DISP_RMLSTC                                		"\200\71"
#define SKINNY_DISP_VOICEMAIL                             		"\200\72"
#define SKINNY_DISP_IMMDIV                                		"\200\73"
#define SKINNY_DISP_INTRCPT                               		"\200\74"
#define SKINNY_DISP_SETWTCH                               		"\200\75"
#define SKINNY_DISP_TRNSFVM                               		"\200\76"
#define SKINNY_DISP_DND                                   		"\200\77"

#define SKINNY_DISP_DIVALL                                		"\200\100"
#define SKINNY_DISP_CALLBACK                              		"\200\101"				/*!< Call Completion */
#define SKINNY_DISP_NETWORK_CONGESTION_REROUTING          		"\200\102"
#define SKINNY_DISP_BARGE                                 		"\200\103"
#define SKINNY_DISP_FAILED_TO_SETUP_BARGE                 		"\200\104"
#define SKINNY_DISP_ANOTHER_BARGE_EXISTS                  		"\200\105"
#define SKINNY_DISP_INCOMPATIBLE_DEVICE_TYPE              		"\200\106"
#define SKINNY_DISP_NO_PARK_NUMBER_AVAILABLE              		"\200\107"

#define SKINNY_DISP_CALLPARK_REVERSION                    		"\200\110"
#define SKINNY_DISP_SERVICE_IS_NOT_ACTIVE                 		"\200\111"
#define SKINNY_DISP_HIGH_TRAFFIC_TRY_AGAIN_LATER          		"\200\112"
#define SKINNY_DISP_QRT                                   		"\200\113"				/*<! Quality Request */
#define SKINNY_DISP_MCID                                  		"\200\114"
#define SKINNY_DISP_DIRTRFR                               		"\200\115"
#define SKINNY_DISP_SELECT                                		"\200\116"
#define SKINNY_DISP_CONFLIST                              		"\200\117"

#define SKINNY_DISP_IDIVERT                               		"\200\120"
#define SKINNY_DISP_CBARGE                                		"\200\121"
#define SKINNY_DISP_CAN_NOT_COMPLETE_TRANSFER             		"\200\122"
#define SKINNY_DISP_CAN_NOT_JOIN_CALLS                    		"\200\123"
#define SKINNY_DISP_MCID_SUCCESSFUL                       		"\200\124"
#define SKINNY_DISP_NUMBER_NOT_CONFIGURED                 		"\200\125"
#define SKINNY_DISP_SECURITY_ERROR                        		"\200\126"
#define SKINNY_DISP_VIDEO_BANDWIDTH_UNAVAILABLE           		"\200\127"

#define SKINNY_DISP_VIDMODE						"\200\130"
#define SKINNY_DISP_MAX_CALL_DURATION_TIMEOUT				"\200\131"
#define SKINNY_DISP_MAX_HOLD_DURATION_TIMEOUT				"\200\132"
#define SKINNY_DISP_OPICKUP						"\200\133"				/*<! */
#define SKINNY_DISP_HLOG                                        	"\200\134"				/*<! Huntgroup/queue Login/Logout */
#define SKINNY_DISP_LOGGED_OUT_OF_HUNT_GROUP                    	"\200\135"				/*<! - FS */
#define SKINNY_DISP_PARK_SLOT_UNAVAILABLE                       	"\200\136"				/*<! - FS */
#define SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP                	"\200\137"				/*<! - FS */

#define SKINNY_DISP_EXTERNAL_TRANSFER_RESTRICTED			"\200\141"
#define SKINNY_DISP_NO_LINE_AVAILABLE_FOR_PICKUP			"\200\142"				/*<! - FS */
#define SKINNY_DISP_PATH_REPLACEMENT_IN_PROGRESS			"\200\143"
#define SKINNY_DISP_UNKNOWN_2						"\200\144"
#define SKINNY_DISP_MAC_ADDRESS						"\200\145"
#define SKINNY_DISP_HOST_NAME						"\200\146"
#define SKINNY_DISP_DOMAIN_NAME						"\200\147"

#define SKINNY_DISP_IP_ADDRESS						"\200\150"
#define SKINNY_DISP_SUBNET_MASK						"\200\151"
#define SKINNY_DISP_TFTP_SERVER_1					"\200\152"
#define SKINNY_DISP_DEFAULT_ROUTER_1					"\200\153"
#define SKINNY_DISP_DEFAULT_ROUTER_2					"\200\154"
#define SKINNY_DISP_DEFAULT_ROUTER_3					"\200\155"
#define SKINNY_DISP_DEFAULT_ROUTER_4					"\200\156"
#define SKINNY_DISP_DEFAULT_ROUTER_5					"\200\157"

#define SKINNY_DISP_DNS_SERVER_1					"\200\160"
#define SKINNY_DISP_DNS_SERVER_2					"\200\161"
#define SKINNY_DISP_DNS_SERVER_3					"\200\162"
#define SKINNY_DISP_DNS_SERVER_4					"\200\163"
#define SKINNY_DISP_DNS_SERVER_5					"\200\164"
#define SKINNY_DISP_OPERATIONAL_VLAN_ID					"\200\165"
#define SKINNY_DISP_ADMIN_VLAN_ID					"\200\166"
#define SKINNY_DISP_CALL_MANAGER_1					"\200\167"

#define SKINNY_DISP_CALL_MANAGER_2					"\200\170"
#define SKINNY_DISP_CALL_MANAGER_3					"\200\171"
#define SKINNY_DISP_CALL_MANAGER_4					"\200\172"
#define SKINNY_DISP_CALL_MANAGER_5					"\200\173"
#define SKINNY_DISP_INFORMATION_URL					"\200\174"
#define SKINNY_DISP_DIRECTORIES_URL					"\200\175"
#define SKINNY_DISP_MESSAGES_URL					"\200\176"
#define SKINNY_DISP_SERVICES_URL					"\200\177"

// Need to be translated
//#define SKINNY_DISP_MONITOR                                           "Record"
#define SKINNY_DISP_DIAL						"Dial"
#define SKINNY_DISP_CALL_PROGRESS					"Call Progress"
#define SKINNY_DISP_SILENT						"Silent"
#define SKINNY_DISP_ENTER_NUMBER_TO_FORWARD_TO				"Enter number to forward to"

// Errors needing to be translated
#define SKINNY_DISP_NO_LINES_REGISTERED					"No lines registered!"
#define SKINNY_DISP_NO_LINE_TO_TRANSFER					"No line found to transfer"
#define SKINNY_DISP_NO_LINE_AVAILABLE					"No Line Available"
#define SKINNY_DISP_NO_MORE_DIGITS					"No more digits"
#define SKINNY_DISP_NO_ACTIVE_CALL_TO_PUT_ON_HOLD			"No Active call to put on hold"
#define SKINNY_DISP_TRANSVM_WITH_NO_CHANNEL				"TRANSVM with no channel active"
#define SKINNY_DISP_TRANSVM_WITH_NO_LINE				"TRANSVM with no line active"
#define SKINNY_DISP_NOT_ENOUGH_CALLS_TO_TRANSFER			"Not enough calls to transfer"
#define SKINNY_DISP_MORE_THAN_TWO_CALLS					"More that two calls"
#define SKINNY_DISP_USE							"use"
#define SKINNY_DISP_PRIVATE_FEATURE_NOT_ACTIVE				"Private Feature is not active"
#define SKINNY_DISP_PRIVATE_WITHOUT_LINE_CHANNEL			"Private without line or channel"
#define SKINNY_DISP_NO_CHANNEL_TO_PERFORM_XXXXXXX_ON			"No Channel to perform %s on !"
#define SKINNY_GIVING_UP						"Giving Up"
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
