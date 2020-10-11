/*!
 * \file	forward_declarations.h
 * \brief	Forward Declarations
 * \author	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */
#pragma once

__BEGIN_C_EXTERN__
/* global types */
#if defined(HAVE_UNALIGNED_BUSERROR)										// for example sparc64
typedef unsigned long sccp_group_t;                                                                             //!< SCCP callgroup / pickupgroup
#else
typedef ULONG sccp_group_t;                                        //!< SCCP callgroup / pickupgroup
#endif

typedef struct sccp_session sccp_session_t;                                              //!< SCCP Session Structure
typedef struct sccp_linedevice sccp_linedevice_t;                                        //!< SCCP Line Connected to Devices
typedef struct sccp_device sccp_device_t;                                                //!< SCCP Device Structure
typedef struct sccp_line sccp_line_t;                                                    //!< SCCP Line Structure
typedef struct sccp_channel sccp_channel_t;                                              //!< SCCP Channel Structure
typedef struct sccp_speed sccp_speed_t;                                                  //!< SCCP Speed Structure
typedef struct sccp_service sccp_service_t;                                              //!< SCCP Service Structure
typedef struct sccp_addon sccp_addon_t;                                                  //!< SCCP Add-On Structure
typedef struct sccp_hint sccp_hint_t;                                                    //!< SCCP Hint Structure
typedef struct sccp_hostname sccp_hostname_t;                                            //!< SCCP HostName Structure
typedef struct sccp_rtp sccp_rtp_t;                                                      //!< SCCP RTP Structure
typedef struct sccp_header sccp_header_t;                                                //!< Skinny Protocol Structure
typedef struct sccp_msg sccp_msg_t;                                                      //!< Skinny Message Structure

/* we are promissing not to change the pointer (don't cast away `*const`), when we use the object pointer */
#define sessionPtr sccp_session_t *const
#define devicePtr sccp_device_t *const
#define linePtr sccp_line_t *const
#define channelPtr sccp_channel_t *const
#define lineDevicePtr sccp_linedevice_t * const
#define conferencePtr sccp_conference_t *const
#define rtpPtr        sccp_rtp_t * const
#define messagePtr    sccp_msg_t * const
/* we are promissing not to change the object nor the pointer to the object, when we use this object pointer */
#define constSessionPtr const sccp_session_t *const
#define constDevicePtr const sccp_device_t *const
#define constLinePtr const sccp_line_t *const
#define constChannelPtr const sccp_channel_t *const
#define constLineDevicePtr const sccp_linedevice_t * const
#define constConferencePtr const sccp_conference_t *const
#define constRtpPtr        const sccp_rtp_t * const
#define constMessagePtr const sccp_msg_t * const
#ifdef CS_DEVSTATE_FEATURE
typedef struct sccp_devstate_specifier sccp_devstate_specifier_t;                                             //!< SCCP Custom DeviceState Specifier Structure
#endif
typedef struct sccp_feature_configuration sccp_featureConfiguration_t;                                        //!< SCCP Feature Configuration Structure
typedef struct sccp_selectedchannel sccp_selectedchannel_t;                                                   //!< SCCP Selected Channel Structure
typedef struct sccp_ast_channel_name sccp_ast_channel_name_t;                                                 //!< SCCP Asterisk Channel Name Structure
typedef struct sccp_buttonconfig sccp_buttonconfig_t;                                                         //!< SCCP Button Config Structure
typedef struct sccp_hotline sccp_hotline_t;                                                                   //!< SCCP Hotline Structure
typedef struct sccp_callinfo sccp_callinfo_t;                                                                 //!< SCCP Call Information Structure
typedef struct sccp_call_statistics sccp_call_statistics_t;                                                   //!< SCCP Call Statistic Structure
typedef struct softKeySetConfiguration sccp_softKeySetConfiguration_t;                                        //!< SoftKeySet configuration
typedef struct sccp_mailbox sccp_mailbox_t;                                                                   //!< SCCP Mailbox Type Definition
typedef struct subscription sccp_subscription_t;                                                              //!< SCCP SubscriptionId Structure
typedef struct sccp_conference sccp_conference_t;                                                             //!< SCCP Conference Structure
typedef struct sccp_private_channel_data sccp_private_channel_data_t;                                         //!< SCCP Private Channel Data Structure
typedef struct sccp_private_device_data sccp_private_device_data_t;                                           //!< SCCP Private Device Data Structure
typedef struct sccp_cfwd_information sccp_cfwd_information_t;                                                 //!< SCCP CallForward information Structure
typedef struct sccp_buttonconfig_list sccp_buttonconfig_list_t;                                               //!< SCCP ButtonConfig List Structure
typedef struct sccp_threadpool sccp_threadpool_t;                                                             //!< SCCP ThreadPool Structure
typedef struct _xmlDoc xmlDoc;
typedef struct _xmlNode xmlNode;

#ifndef SOLARIS
#  if defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
typedef _Bool boolean_t;
#    define FALSE false
#    define TRUE true
#  else
// typedef enum { FALSE = 0, TRUE = 1 } boolean_t;								//!< Asterisk Reverses True and False
#define TRUE 1
#define FALSE 0
typedef bool boolean_t;
#  endif
#else
#  define FALSE B_FALSE
#  define TRUE B_TRUE
#endif

/* CompileTime Annotations */
#define do_expect(_x) __builtin_expect(_x,1)
#define dont_expect(_x) __builtin_expect(_x,0)
#if defined(CCC_ANALYZER)
#define NONENULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
#define NONENULL(...) 
#endif

/* callback function types */
typedef void sk_func(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c);
typedef int (*sccp_sched_cb) (const void *data);
__END_C_EXTERN__
