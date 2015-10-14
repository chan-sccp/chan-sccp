/*!
 * \file        sccp_features.c
 * \brief       SCCP Features Class
 * \author      Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 *
 * $Date$
 * $Revision$
 */

/*!
 * \remarks     
 * Purpose:     SCCP Features
 * When to use: Only methods directly related to handling phone features should be stored in this source file.
 *              Phone Features are Capabilities of the phone, like:
 *               - CallForwarding
 *               - Dialing
 *               - Changing to Do Not Disturb(DND) Status
 *               .
 * Relations:   These Features are called by FeatureButtons. Features can in turn call on Actions.
 */

#include <config.h>
#include "common.h"
#include "sccp_features.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include "sccp_line.h"
#include "sccp_featureButton.h"
#include "sccp_pbx.h"
#include "sccp_utils.h"
#include "sccp_conference.h"
#include "sccp_indicate.h"
//#include "sccp_rtp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");

/*!
 * \brief Handle Call Forwarding
 * \param l SCCP Line
 * \param device SCCP Device
 * \param type CallForward Type (NONE, ALL, BUSY, NOANSWER) as SCCP_CFWD_*
 * \return SCCP Channel
 *
 * \callgraph
 * \callergraph
 */
void sccp_feat_handle_callforward(constLinePtr l, constDevicePtr device, sccp_callforward_t type)
{
	if (!l) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line is not specified!\n");
		return;
	}

	if (!device) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if device is not specified!\n");
		return;
	}

	AUTO_RELEASE sccp_linedevices_t *linedevice = sccp_linedevice_find(device, l);

	if (!linedevice) {
		pbx_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return;
	}

	/* if call forward is active and you asked about the same call forward maybe you would disable */
	if ((linedevice->cfwdAll.enabled && type == SCCP_CFWD_ALL)
	    || (linedevice->cfwdBusy.enabled && type == SCCP_CFWD_BUSY)) {

		sccp_line_cfwd(l, device, SCCP_CFWD_NONE, NULL);
		return;
	} else {
		if (type == SCCP_CFWD_NOANSWER) {
			sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "### CFwdNoAnswer NOT SUPPORTED\n");
			sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
			return;
		}
	}

	/* look if we have a call  */
	AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(device);

	if (c) {
		if (c->softswitch_action == SCCP_SOFTSWITCH_GETFORWARDEXTEN) {
			// we have a channel, checking if
			if (c->state == SCCP_CHANNELSTATE_RINGOUT || c->state == SCCP_CHANNELSTATE_CONNECTED || c->state == SCCP_CHANNELSTATE_PROCEED || c->state == SCCP_CHANNELSTATE_BUSY || c->state == SCCP_CHANNELSTATE_CONGESTION) {
				if (c->calltype == SKINNY_CALLTYPE_OUTBOUND) {
					pbx_log(LOG_ERROR, "%s: 1\n", DEV_ID_LOG(device));
					// if we have an outbound call, we can set callforward to dialed number -FS
					if (!sccp_strlen_zero(c->dialedNumber)) {		// checking if we have a number !
						pbx_log(LOG_ERROR, "%s: 2\n", DEV_ID_LOG(device));
						sccp_line_cfwd(l, device, type, c->dialedNumber);
						// we are on call, so no tone has been played until now :)
						//sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, instance, 0, 0);
						sccp_channel_endcall(c);
						return;
					}
				} else if (iPbx.channel_is_bridged(c)) {					// check if we have an ast channel to get callerid from
					// if we have an incoming or forwarded call, let's get number from callerid :) -FS
					char *number = NULL;

					pbx_log(LOG_ERROR, "%s: 3\n", DEV_ID_LOG(device));

					if (iPbx.get_callerid_name) {
						iPbx.get_callerid_number(c, &number);
					}
					if (number) {
						sccp_line_cfwd(l, device, type, number);
						pbx_log(LOG_ERROR, "%s: 4\n", DEV_ID_LOG(device));
						// we are on call, so no tone has been played until now :)
						sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, linedevice->lineInstance, c->callid, 1);
						sccp_channel_endcall(c);
						sccp_free(number);
						return;
					}
					// if we where here it's cause there is no number in callerid,, so put call on hold and ask for a call forward number :) -FS
					if (!sccp_channel_hold(c)) {
						// if can't hold  it means there is no active call, so return as we're already waiting a number to dial
						sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
						return;
					}
				}
			} else if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				pbx_log(LOG_ERROR, "%s: 5\n", DEV_ID_LOG(device));
				// we are dialing but without entering a number :D -FS
				sccp_dev_stoptone(device, linedevice->lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETFORWARDEXTEN;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = type;								/* this should be found in thread */
				// changing channelstate to GETDIGITS
				//sccp_indicate(device, c, SCCP_CHANNELSTATE_OFFHOOK);                          /* Removal requested by Antonio */
				sccp_indicate(device, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
			} else {
				// we cannot allocate a channel, or ask an extension to pickup.
				sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		} else {
			/* see case for channel */
		}
	}

	if (!c) {
		// if we where here there is no call in progress, so we should allocate a channel.
		c = sccp_channel_allocate(l, device);
		if (!c) {
			pbx_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n", DEV_ID_LOG(device), l->name);
			return;
		}
		if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
			pbx_log(LOG_WARNING, "%s: (handle_callforward) Unable to allocate a new channel for line %s\n", DEV_ID_LOG(device), l->name);
			sccp_indicate(device, c, SCCP_CHANNELSTATE_CONGESTION);					// implicitly retained device by sccp_action
			return;
		}

	} else {
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {

			/** we just opened a channel for cfwd, switch softswitch_action = SCCP_SOFTSWITCH_GETFORWARDEXTEN */
			sccp_channel_stop_schedule_digittimout(c);
			// we are dialing but without entering a number :D -FS
			sccp_dev_stoptone(device, linedevice->lineInstance, c->callid);

		} else {
			// other call in progress, put on hold
			int ret = sccp_channel_hold(c);

			if (!ret) {
				pbx_log(LOG_ERROR, "%s: Active call '%d' could not be put on hold\n", DEV_ID_LOG(device), c->callid);
				return;
			}
		}
	}

	c->softswitch_action = SCCP_SOFTSWITCH_GETFORWARDEXTEN;							/* SoftSwitch will catch a number to be dialed */
	c->ss_data = type;											/* this should be found in thread */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	sccp_indicate(device, c, SCCP_CHANNELSTATE_GETDIGITS);
	iPbx.set_callstate(c, AST_STATE_OFFHOOK);
	sccp_dev_displayprompt(device, linedevice->lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER_TO_FORWARD_TO, SCCP_DISPLAYSTATUS_TIMEOUT);

	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	if (device->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(c);
	}
}

#ifdef CS_SCCP_PICKUP

/*!
 * \brief Handle Direct Pickup of Line
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param d SCCP Device
 * \return SCCP Channel
 *
 */
void sccp_feat_handle_directed_pickup(constLinePtr l, uint8_t lineInstance, constDevicePtr d)
{

	if (!l || !d || sccp_strlen(d->id) < 3) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}

	/* look if we have a call */
	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

		if (c) {
			// we have a channel, checking if
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				// we are dialing but without entering a number :D -FS
				sccp_dev_stoptone(d, lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETPICKUPEXTEN;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = 0;									/* this should be found in thread */
				// changing channelstate to GETDIGITS
				sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
			} else {
				/* there is an active call, let's put it on hold first */
				if (!sccp_channel_hold(c)) {
					return;
				}
			}
		}
	}

	AUTO_RELEASE sccp_channel_t *c = sccp_channel_allocate(l, d);

	if (!c) {
		pbx_log(LOG_ERROR, "%s: (handle_directed_pickup) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return;
	}

	c->softswitch_action = SCCP_SOFTSWITCH_GETPICKUPEXTEN;							/* SoftSwitch will catch a number to be dialed */
	c->ss_data = 0;												/* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
		pbx_log(LOG_WARNING, "%s: (handle_directed_pickup) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return;
	}

	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(c);
	}
}

/*!
 * \brief Handle Direct Pickup of Extension
 * \param c *locked* SCCP Channel
 * \param exten Extension as char
 * \return Success as int
 *
 * \lock
 *  - asterisk channel
 */
int sccp_feat_directed_pickup(channelPtr c, const char *exten)
{
	int res = -1;

#if CS_AST_DO_PICKUP
	PBX_CHANNEL_TYPE *original = NULL;									/* destination */

	char *context;

	if (sccp_strlen_zero(exten)) {
		pbx_log(LOG_ERROR, "SCCP: (directed_pickup) zero exten. Giving up.\n");
		return -1;
	}

	if (!c || !c->owner) {
		pbx_log(LOG_ERROR, "SCCP: (directed_pickup) no channel found. Giving up.\n");
		return -1;
	}

	original = c->owner;
	if (!c->line) {
		pbx_log(LOG_WARNING, "SCCP: (directed_pickup) no line found. Giving up.\n");
		return -1;
	}

	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);

	if (!d) {
		pbx_log(LOG_WARNING, "SCCP: (directed_pickup) no device found. Giving up.\n");
		return -1;
	}

	if (!c->line->pickupgroup
#if CS_AST_HAS_NAMEDGROUP
	    && sccp_strlen_zero(c->line->namedpickupgroup)
#endif
	    ) {
		pbx_log(LOG_WARNING, "%s: (directed pickup) no pickupgroup(s) configured for this line. Giving up.\n", d->id);
		return -1;
	}

	if (!iPbx.findPickupChannelByExtenLocked) {
		pbx_log(LOG_WARNING, "SCCP: (directed_pickup) findPickupChannelByExtenLocked not implemented for this asterisk version. Giving up.\n");
		return -1;
	}

	/* copying extension into our buffer */
	if ((context = strchr(exten, '@'))) {
		*context++ = '\0';
	} else {
		if (!sccp_strlen_zero(d->directed_pickup_context)) {
			context = (char *) strdupa(d->directed_pickup_context);
		} else {
			context = (char *) strdupa(pbx_channel_context(original));
		}
	}

	if (sccp_strlen_zero(context)) {
		pbx_log(LOG_ERROR, "SCCP: (directed_pickup) We could not find a context for this line. Giving up !\n");
		return -1;
	}

	PBX_CHANNEL_TYPE *target = NULL;									/* potential pickup target */
	PBX_CHANNEL_TYPE *tmp = NULL;
	const char *ringermode = NULL;

	char *name = NULL;
	char *number = NULL;

	pbx_log(LOG_NOTICE, "SCCP: (directed_pickup)\n");
	target = iPbx.findPickupChannelByExtenLocked(original, exten, context);
	if (target) {
		/* fixup callinfo */
		AUTO_RELEASE sccp_channel_t *tmpChannel = get_sccp_channel_from_pbx_channel(target);
		if (tmpChannel) {
			if (iPbx.get_callerid_name) {
				iPbx.get_callerid_name(tmpChannel, &name);
			}
			if (iPbx.get_callerid_number) {
				iPbx.get_callerid_number(tmpChannel, &number);
			}
		}

		if ((tmp = iPbx.get_bridged_channel(target))) {
			pbx_log(LOG_NOTICE, "SCCP: (directed_pickup) %s callerid is ('%s'-'%s')\n", pbx_channel_name(tmp), name ? name : "", number ? number : "");
			pbx_channel_unref(tmp);
		} else {	
			pbx_log(LOG_NOTICE, "SCCP: (directed_pickup) %s callerid is ('%s'-'%s')\n", pbx_channel_name(target), name ? name : "", number ? number : "");
		}
		tmp = NULL;
		res = 0;
		if (d->directed_pickup_modeanswer) {
			if ((res = ast_answer(original))) {							// \todo: remove res in this line: Although the value stored to 'res' is used in the enclosing expression, the value is never actually read from 'res'
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (directed_pickup) Unable to answer '%s'\n", iPbx.getChannelName(c));
				res = -1;
			} else if ((res = iPbx.queue_control(original, AST_CONTROL_ANSWER))) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (directed_pickup) Unable to queue answer on '%s'\n", iPbx.getChannelName(c));
				res = -1;
			}
		}

		if (res == 0) {
			sccp_channel_stop_schedule_digittimout(c);
			c->calltype = SKINNY_CALLTYPE_INBOUND;
			c->state = SCCP_CHANNELSTATE_PROCEED;
			c->answered_elsewhere = TRUE;
			AUTO_RELEASE sccp_device_t *orig_device = NULL;
			AUTO_RELEASE sccp_channel_t *orig_channel = get_sccp_channel_from_pbx_channel(original);
			if (orig_channel) {
				orig_device = sccp_channel_getDevice_retained(orig_channel);
			}

			char picker_number[StationMaxDirnumSize] = {0}, called_number[StationMaxDirnumSize] = {0};
			char picker_name[StationMaxNameSize] = {0}, called_name[StationMaxNameSize] = {0};

			/* Gather CallInfo */
			sccp_callinfo_t *callinfo_picker = sccp_channel_getCallInfo(c);
			sccp_callinfo_t *callinfo_orig = sccp_channel_getCallInfo(orig_channel);
			sccp_callinfo_getter(callinfo_picker,							/* picker */
				SCCP_CALLINFO_CALLINGPARTY_NAME, &picker_name,					/* name of picker */
				SCCP_CALLINFO_CALLINGPARTY_NUMBER, &picker_number,
				SCCP_CALLINFO_KEY_SENTINEL);

			sccp_callinfo_getter(callinfo_orig,							/* picker */
				SCCP_CALLINFO_CALLEDPARTY_NAME, &called_name,					/* name of picker */
				SCCP_CALLINFO_CALLEDPARTY_NUMBER, &called_number,
				SCCP_CALLINFO_KEY_SENTINEL);

			res = ast_do_pickup(original, target);
			if (!res) {
				/* directed pickup succeeded */
				sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (directed_pickup) pickup succeeded on call: %s\n", DEV_ID_LOG(d), c->designator);
				sccp_rtp_stop(c);								/* stop previous audio */
				pbx_channel_set_hangupcause(original, AST_CAUSE_ANSWERED_ELSEWHERE);
				if (orig_device && orig_channel) {
					//sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (directed_pickup) hangup: %s\n", DEV_ID_LOG(orig_device), orig_channel->designator);
					sccp_indicate(orig_device, orig_channel, SCCP_CHANNELSTATE_ONHOOK);
				}
				pbx_hangup(original);								/* hangup masqueraded zombie channel */

				pbx_channel_set_hangupcause(c->owner, AST_CAUSE_NORMAL_CLEARING);		/* reset picked up channel */
				sccp_channel_setDevice(c, d);
				sccp_channel_updateChannelCapability(c);

				/* Update CallInfo */
				sccp_callinfo_t *callinfo_orig = sccp_channel_getCallInfo(orig_channel);
				sccp_callinfo_setter(callinfo_orig, 							/* update calling end */
					SCCP_CALLINFO_CALLEDPARTY_NAME, picker_name, 					/* channel picking up */
					SCCP_CALLINFO_CALLEDPARTY_NUMBER, picker_number, 
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, called_name, 
					SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, called_number, 
					SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, 1,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, picker_name,
					SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, picker_number,
					SCCP_CALLINFO_LAST_REDIRECT_REASON, 4,
					SCCP_CALLINFO_KEY_SENTINEL);
					
				if (d->directed_pickup_modeanswer) {
					pbx_setstate(c->owner, AST_STATE_UP);
					sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
					
					/* force 7940/7960 to display the callplane (something is suppressing it along the way, have not been able to find what, yet) */
					uint8_t instance = sccp_device_find_index_for_line(d, c->line->name);
					sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_CONNECTED, SKINNY_CALLPRIORITY_NORMAL, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				} else {
					uint8_t instance;

					instance = sccp_device_find_index_for_line(d, c->line->name);
					sccp_dev_stoptone(d, instance, c->callid);
					sccp_dev_set_speaker(d, SKINNY_STATIONSPEAKER_OFF);
					c->ringermode = SKINNY_RINGTYPE_OUTSIDE;				// default ring
					ringermode = pbx_builtin_getvar_helper(c->owner, "ALERT_INFO");
					if (ringermode && !sccp_strlen_zero(ringermode)) {
						sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Found ALERT_INFO=%s\n", ringermode);
						if (strcasecmp(ringermode, "inside") == 0) {
							c->ringermode = SKINNY_RINGTYPE_INSIDE;
						} else if (strcasecmp(ringermode, "feature") == 0) {
							c->ringermode = SKINNY_RINGTYPE_FEATURE;
						} else if (strcasecmp(ringermode, "silent") == 0) {
							c->ringermode = SKINNY_RINGTYPE_SILENT;
						} else if (strcasecmp(ringermode, "urgent") == 0) {
							c->ringermode = SKINNY_RINGTYPE_URGENT;
						}
					}
					sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGING);
					iPbx.set_callstate(c, AST_STATE_RINGING);
				}
			}
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (directed_pickup) Giving Up\n");
		}
		pbx_channel_unlock(target);
		target = pbx_channel_unref(target);
	} else {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (directed_pickup) findPickupChannelByExtenLocked failed on callid: %s\n", DEV_ID_LOG(d), c->designator);
	}
	if (!res) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (directed_pickup) Exten '%s@%s' Picked up Succesfully\n", exten, context);
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (directed_pickup) Failed to pickup up Exten '%s@%s'\n", exten, context);
		int instance = sccp_device_find_index_for_line(d, c->line->name);

		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_channel_closeAllMediaTransmitAndReceive(d, c);
		sccp_dev_starttone(d, SKINNY_TONE_REORDERTONE, instance, c->callid, 0);
		sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
	}
#endif
	return res;
}

/*!
 * \brief Handle Group Pickup Feature
 * \param l SCCP Line
 * \param d SCCP Device
 * \return Success as int
 * 
 * \todo backport from trunk
 *
 * \lock
 *  - asterisk channel
 *
 * \todo Fix callerid setting before calling ast_pickup_call
 */
int sccp_feat_grouppickup(constLinePtr l, constDevicePtr d)
{
	int res = 0;

	if (!l || !d || sccp_strlen_zero(d->id)) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (grouppickup) no line or device\n");
		return -1;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) starting grouppickup\n", DEV_ID_LOG(d));

	if (!l->pickupgroup
#if CS_AST_HAS_NAMEDGROUP
	    && sccp_strlen_zero(l->namedpickupgroup)
#endif
	    ) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) pickupgroup not configured in sccp.conf\n", d->id);
		return -1;
	}
#if 0
	/* re-use/create channel for pickup */
	AUTO_RELEASE sccp_channel_t *c = NULL;

	if ((c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_OFFHOOK)) && !pbx_test_flag(pbx_channel_flags(c->owner), AST_FLAG_ZOMBIE)) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) Already offhook, reusing callid %d\n", d->id, c->callid);
	} else {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) Starting new channel\n", d->id);
		if (!(c = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL))) {
			pbx_log(LOG_ERROR, "%s: Could not create a new channel to use for grouppickup\n", d->id);
			return -1;
		}
	}

	sccp_channel_stop_schedule_digittimout(c);

	if (iPbx.getPickupExtension(channel, c->dialedNumber)) {
		sccp_pbx_softswitch(c);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) finished\n", DEV_ID_LOG(d));
		res = 0;
	}
#else
	PBX_CHANNEL_TYPE *dest = NULL;

	if (!iPbx.feature_pickup) {
		pbx_log(LOG_ERROR, "%s: (grouppickup) GPickup feature not implemented\n", d->id);
	}

	/* create channel for pickup */
	AUTO_RELEASE sccp_channel_t *c = NULL;

	if ((c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_OFFHOOK)) && !pbx_test_flag(pbx_channel_flags(c->owner), AST_FLAG_ZOMBIE)) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) Already offhook, reusing callid %d\n", d->id, c->callid);
		dest = c->owner;
	} else {
		/* emulate a new call so we end up in the same state as when a new call has been started */
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) Starting new channel\n", d->id);
		if ((c = sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL))) {
			dest = c->owner;
		}
	}
	if (!dest) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) Failed to get offhook channel or create new channel\n", d->id);
		return -2;
	}
	/* prepare for call pickup */
	sccp_channel_stop_schedule_digittimout(c);

	/* change the call direction, we know it is a pickup, so it should be an inbound call */
	c->calltype = SKINNY_CALLTYPE_INBOUND;
	/* done, change call direction */

	char cid_name[StationMaxNameSize] = {0};
	char cid_num[StationMaxDirnumSize] = {0};
	sccp_callinfo_t *callinfo = sccp_channel_getCallInfo(c);
	sccp_callinfo_getter(callinfo, 
		SCCP_CALLINFO_CALLINGPARTY_NAME, &cid_name,
		SCCP_CALLINFO_CALLINGPARTY_NUMBER, &cid_num,
		SCCP_CALLINFO_KEY_SENTINEL);

	c->state = SCCP_CHANNELSTATE_PROCEED;
	c->answered_elsewhere = TRUE;

	res = pbx_pickup_call(dest);										/* Should be replaced with a iPbx.feature_pickup() call, which could give information back why the pickup failed (elsewhere or no match) */
	if (!res) {
		/* gpickup succeeded */
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) pickup succeeded on callid: %d\n", DEV_ID_LOG(d), c->callid);
		sccp_rtp_stop(c);										/* stop previous audio */
		pbx_channel_set_hangupcause(dest, AST_CAUSE_ANSWERED_ELSEWHERE);
		pbx_hangup(dest);										/* hangup masqueraded zombie channel */

		pbx_channel_set_hangupcause(c->owner, AST_CAUSE_NORMAL_CLEARING);
		sccp_channel_setDevice(c, d);
		sccp_channel_updateChannelCapability(c);
		sccp_callinfo_setter(callinfo,
			SCCP_CALLINFO_CALLEDPARTY_NAME, cid_name,
			SCCP_CALLINFO_CALLEDPARTY_NUMBER, cid_num,
			SCCP_CALLINFO_CALLINGPARTY_NAME, "",
			SCCP_CALLINFO_CALLINGPARTY_NUMBER, "",
			SCCP_CALLINFO_KEY_SENTINEL);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);						/* connect calls - reinstate audio */
	} else {
		/* call pickup failed, restore previous situation */
		c->answered_elsewhere = FALSE;

		/* send gpickup failure */
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) pickup failed (someone else picked it up already or not in the correct callgroup)\n", DEV_ID_LOG(d));
		int instance = sccp_device_find_index_for_line(d, l->name);

		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_channel_closeAllMediaTransmitAndReceive(d, c);
		sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, instance, c->callid, 2);
		sccp_channel_schedule_hangup(c, SCCP_HANGUP_TIMEOUT);
	}
#endif
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) finished (%d)\n", DEV_ID_LOG(d), res);
	return res;
}
#endif														// CS_SCCP_PICKUP

/*!
 * \brief Handle VoiceMail
 * \param d SCCP Device
 * \param lineInstance LineInstance as uint8_t
 *
 */
void sccp_feat_voicemail(constDevicePtr d, uint8_t lineInstance)
{
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Voicemail Button pressed on line (%d)\n", d->id, lineInstance);

	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

		if (c) {
			if (!c->line || sccp_strlen_zero(c->line->vmnum)) {
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, lineInstance);
				return;
			}
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK || c->state == SCCP_CHANNELSTATE_DIALING) {
				sccp_copy_string(c->dialedNumber, c->line->vmnum, sizeof(c->dialedNumber));
				sccp_channel_stop_schedule_digittimout(c);
				sccp_pbx_softswitch(c);
				return;
			}

			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
			return;
		}
	}

	if (!lineInstance) {
		if (d->defaultLineInstance) {
			lineInstance = d->defaultLineInstance;
		} else {
			lineInstance = 1;
		}
	}

	AUTO_RELEASE sccp_line_t *l = sccp_line_find_byid(d, lineInstance);

	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line with instance %d found.\n", d->id, lineInstance);

		//TODO workaround to solve the voicemail button issue with old hint style and speeddials before first line -MC
		if (d->defaultLineInstance) {
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		}
	}
	if (l) {
		if (!sccp_strlen_zero(l->vmnum)) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Dialing voicemail %s\n", d->id, l->vmnum);
			AUTO_RELEASE sccp_channel_t *new_channel = NULL;

			new_channel = sccp_channel_newcall(l, d, l->vmnum, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		} else {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No voicemail number configured on line %d\n", d->id, lineInstance);
		}
	} else {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line with defaultLineInstance %d found. Not Dialing Voicemail Extension.\n", d->id, d->defaultLineInstance);
	}
}

/*!
 * \brief Handle Divert/Transfer Call to VoiceMail
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_feat_idivert(constDevicePtr d, constLinePtr l, constChannelPtr c)
{
	int instance;

	if (!l) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: TRANSVM pressed but no line found\n", d->id);
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_TRANSVM_WITH_NO_LINE, SCCP_DISPLAYSTATUS_TIMEOUT);
		return;
	}
	if (!l->trnsfvm) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: TRANSVM pressed but not configured in sccp.conf\n", d->id);
		return;
	}
	if (!c || !c->owner) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: TRANSVM with no channel active\n", d->id);
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_TRANSVM_WITH_NO_CHANNEL, SCCP_DISPLAYSTATUS_TIMEOUT);
		return;
	}

	if (c->state != SCCP_CHANNELSTATE_RINGING && c->state != SCCP_CHANNELSTATE_CALLWAITING) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: TRANSVM pressed in no ringing state\n", d->id);
		return;
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: TRANSVM to %s\n", d->id, l->trnsfvm);
	iPbx.setChannelCallForward(c, l->trnsfvm);
	instance = sccp_device_find_index_for_line(d, l->name);
	sccp_device_sendcallstate(d, instance, c->callid, SKINNY_CALLSTATE_PROCEED, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);	/* send connected, so it is not listed as missed call */
	pbx_setstate(c->owner, AST_STATE_BUSY);
	iPbx.queue_control(c->owner, AST_CONTROL_BUSY);
}

/*!
 * \brief Handle 3-Way Phone Based Conferencing on a Device
 * \param d SCCP Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param channel SCCP Channel
 * \return SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 *
 */
void sccp_feat_handle_conference(constDevicePtr d, constLinePtr l, uint8_t lineInstance, channelPtr channel)
{
#ifdef CS_SCCP_CONFERENCE
	if (!l || !d || sccp_strlen_zero(d->id)) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}

	if (!d->allow_conference) {
		if (lineInstance && channel && channel->callid) {
			sccp_dev_displayprompt(d, lineInstance, channel->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		} else {
			sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		}
		pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
		return;
	}

/*	if (sccp_device_numberOfChannels(d) < 2) {
		sccp_dev_displayprompt(d, lineInstance, channel->callid, SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE, SCCP_DISPLAYSTATUS_TIMEOUT);
		pbx_log(LOG_NOTICE, "%s: You need at least 2 participant to start a conference\n", DEV_ID_LOG(d));
		return;
	}*/

	/* look if we have a call */
	if (channel) {
		if (!sccp_channel_hold(channel)) {
			sccp_dev_displayprompt(d, lineInstance, channel->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
			return;
		}
	}

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Allocating new channel for conference\n");
	AUTO_RELEASE sccp_channel_t *c = sccp_channel_allocate(l, d);

	if (c) {
		c->softswitch_action = SCCP_SOFTSWITCH_GETCONFERENCEROOM;					/* SoftSwitch will catch a number to be dialed */
		c->ss_data = 0;											/* not needed here */
		c->calltype = SKINNY_CALLTYPE_OUTBOUND;
		sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
		iPbx.set_callstate(c, AST_STATE_OFFHOOK);

		/* ok the number exist. allocate the asterisk channel */
		if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
			pbx_log(LOG_WARNING, "%s: (sccp_feat_handle_conference) Unable to allocate a new channel for line %s\n", d->id, l->name);
			sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
			return;
		}

		iPbx.set_callstate(c, AST_STATE_OFFHOOK);
		/* removing scheduled dial */
		sccp_channel_stop_schedule_digittimout(c);

		sccp_pbx_softswitch(c);
	} else {
		pbx_log(LOG_ERROR, "%s: (sccp_feat_handle_conference) Can't allocate SCCP channel for line %s\n", DEV_ID_LOG(d), l->name);
		return;
	}
#endif
}

/*!
 * \brief Handle Conference
 * \param device SCCP Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param c SCCP Channel
 * \return Success as int
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 *
 */
void sccp_feat_conference_start(constDevicePtr device, const uint32_t lineInstance, channelPtr c)
{
	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (!d || !c) {
		pbx_log(LOG_NOTICE, "%s: (sccp_feat_conference_start) Missing Device or Channel\n", DEV_ID_LOG(device));
		return;
	}
#ifdef CS_SCCP_CONFERENCE
	AUTO_RELEASE sccp_channel_t *channel = NULL;
	sccp_selectedchannel_t *selectedChannel = NULL;
	boolean_t selectedFound = FALSE;
	PBX_CHANNEL_TYPE *bridged_channel = NULL;

	uint8_t num = sccp_device_numberOfChannels(d);
	//int instance = sccp_device_find_index_for_line(d, l->name);

	sccp_log_and((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: sccp_device_numberOfChannels %d.\n", DEV_ID_LOG(d), num);

	if (d->conference /* && num > 3 */ ) {
		/* if we have selected channels, add this to conference */

		SCCP_LIST_LOCK(&d->selectedChannels);
		SCCP_LIST_TRAVERSE(&d->selectedChannels, selectedChannel, list) {
			channel = sccp_channel_retain(selectedChannel->channel);
			if (channel && channel != c) {
				if (channel != d->active_channel && channel->state == SCCP_CHANNELSTATE_HOLD) {
					if ((bridged_channel = iPbx.get_bridged_channel(channel->owner))) {
						sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: sccp conference: channel %s, state: %s.\n", DEV_ID_LOG(d), pbx_channel_name(bridged_channel), sccp_channelstate2str(channel->state));
						if (!sccp_conference_addParticipatingChannel(d->conference, c, channel, bridged_channel)) {
							sccp_dev_displayprompt(device, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
						}
						pbx_channel_unref(bridged_channel);
					} else {
						pbx_log(LOG_ERROR, "%s: sccp conference: bridgedchannel for channel %s could not be found\n", DEV_ID_LOG(d), pbx_channel_name(channel->owner));
					}
				} else {
					sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "%s: sccp conference: Channel %s is Active on Shared Line on Other Device... Skipping.\n", DEV_ID_LOG(d), channel->designator);
				}
			}
			selectedFound = TRUE;
		}
		SCCP_LIST_UNLOCK(&d->selectedChannels);

		/* If no calls were selected, add all calls to the conference, across all lines. */
		if (FALSE == selectedFound) {
			// all channels on this phone
			uint8_t i = 0;

			for (i = 0; i < StationMaxButtonTemplateSize; i++) {
				if (d->buttonTemplate[i].type == SKINNY_BUTTONTYPE_LINE && d->buttonTemplate[i].ptr) {
					AUTO_RELEASE sccp_line_t *line = sccp_line_retain(d->buttonTemplate[i].ptr);

					if (line) {
						SCCP_LIST_LOCK(&line->channels);
						SCCP_LIST_TRAVERSE(&line->channels, channel, list) {
							if (channel != d->active_channel && channel->state == SCCP_CHANNELSTATE_HOLD) {
								if ((bridged_channel = iPbx.get_bridged_channel(channel->owner))) {
									sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: sccp conference: channel %s, state: %s.\n", DEV_ID_LOG(d), pbx_channel_name(bridged_channel), sccp_channelstate2str(channel->state));
									if (!sccp_conference_addParticipatingChannel(d->conference, c, channel, bridged_channel)) {
										sccp_dev_displayprompt(device, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
									}
									pbx_channel_unref(bridged_channel);
								} else {
									pbx_log(LOG_ERROR, "%s: sccp conference: bridgedchannel for channel %s could not be found\n", DEV_ID_LOG(d), pbx_channel_name(channel->owner));
								}
							} else {
								sccp_log(DEBUGCAT_CONFERENCE) (VERBOSE_PREFIX_3 "%s: sccp conference: Channel %s is Active on Shared Line on Other Device...Skipping.\n", DEV_ID_LOG(d), channel->designator);
							}
						}
						SCCP_LIST_UNLOCK(&line->channels);
					}
				}

			}
		}
		sccp_conference_start(d->conference);
	} else {
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE, SCCP_DISPLAYSTATUS_TIMEOUT);
		pbx_log(LOG_NOTICE, "%s: conference could not be created\n", DEV_ID_LOG(d));
	}
#else
	sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: conference not enabled\n", DEV_ID_LOG(d));
	sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
#endif
	return;
}

/*!
 * \brief Handle Join a Conference
 * \param device SCCP Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param c SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_feat_join(constDevicePtr device, constLinePtr l, uint8_t lineInstance, channelPtr c)
{
	AUTO_RELEASE sccp_device_t *d = sccp_device_retain(device);

	if (!c || !d) {
		pbx_log(LOG_NOTICE, "%s: (sccp_feat_join) Missing Device or Channel\n", DEV_ID_LOG(d));
		return;
	}
#if CS_SCCP_CONFERENCE
	sccp_channel_t *newparticipant_channel = NULL;
	sccp_channel_t *moderator_channel = NULL;
	PBX_CHANNEL_TYPE *bridged_channel = NULL;

	if (!d->allow_conference) {
		pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (!d->conference) {
		pbx_log(LOG_NOTICE, "%s: There is currently no active conference on this device. Start Conference First.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_NO_CONFERENCE_BRIDGE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (!d->active_channel) {
		pbx_log(LOG_NOTICE, "%s: No active channel on device to join to the conference.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (d->active_channel->conference) {
		pbx_log(LOG_NOTICE, "%s: Channel is already part of a conference.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_IN_CONFERENCE_ALREADY, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else {
		sccp_conference_hold(d->conference);								// make sure conference is on hold
		newparticipant_channel = d->active_channel;

		SCCP_LIST_LOCK(&((sccp_line_t *const)l)->channels);
		SCCP_LIST_TRAVERSE(&l->channels, moderator_channel, list) {
			if (d->conference == moderator_channel->conference) {
				break;
			}
		}
		SCCP_LIST_UNLOCK(&((sccp_line_t *const)l)->channels);

		if (moderator_channel != newparticipant_channel) {
			if (moderator_channel && newparticipant_channel) {
				sccp_channel_resume(d, moderator_channel, TRUE);				// swap active channel

				pbx_log(LOG_NOTICE, "%s: Joining new participant to conference %d.\n", DEV_ID_LOG(d), d->conference->id);
				if ((bridged_channel = iPbx.get_bridged_channel(newparticipant_channel->owner))) {
					sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: sccp conference: channel %s, state: %s.\n", DEV_ID_LOG(d), pbx_channel_name(bridged_channel), sccp_channelstate2str(newparticipant_channel->state));
					if (!sccp_conference_addParticipatingChannel(d->conference, moderator_channel, newparticipant_channel, bridged_channel)) {
						sccp_dev_displayprompt(device, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
					}
					pbx_channel_unref(bridged_channel);
				} else {
					pbx_log(LOG_ERROR, "%s: sccp conference: bridgedchannel for channel %s could not be found\n", DEV_ID_LOG(d), pbx_channel_name(newparticipant_channel->owner));
				}

				sccp_feat_conflist(d, lineInstance, moderator_channel);
				sccp_conference_update(d->conference);
			} else {
				pbx_log(LOG_NOTICE, "%s: conference moderator could not be found on this phone\n", DEV_ID_LOG(d));
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
			}
		} else {
			pbx_log(LOG_NOTICE, "%s: Cannot use the JOIN button within a conference itself\n", DEV_ID_LOG(d));
			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		}
		sccp_conference_resume(d->conference);								// make sure conference is resumed
	}
#else
	pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
	sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
#endif
}

/*!
 * \brief Handle Conference List
 * \param d SCCP Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param c SCCP Channel
 * \return Success as int
 */
void sccp_feat_conflist(devicePtr d, uint8_t lineInstance, constChannelPtr c)
{
	if (d) {
#ifdef CS_SCCP_CONFERENCE
		if (!d->allow_conference) {
			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
			pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
			return;
		}
		d->conferencelist_active = TRUE;
		sccp_conference_show_list(c->conference, c);
#else
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
#endif
	}
}

/*!
 * \brief Handle 3-Way Phone Based Conferencing on a Device
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param d SCCP Device
 * \return SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 *
 */
void sccp_feat_handle_meetme(constLinePtr l, uint8_t lineInstance, constDevicePtr d)
{
	if (!l || !d || sccp_strlen_zero(d->id)) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}

	/* look if we have a call */
	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

		if (c) {
			// we have a channel, checking if
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				// we are dialing but without entering a number :D -FS
				sccp_dev_stoptone(d, lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETMEETMEROOM;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = 0;									/* this should be found in thread */
				// changing channelstate to GETDIGITS
				sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
				/* there is an active call, let's put it on hold first */
			} else if (!sccp_channel_hold(c)) {
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE sccp_channel_t *c = sccp_channel_allocate(l, d);

	if (!c) {
		pbx_log(LOG_ERROR, "%s: (handle_meetme) Can't allocate SCCP channel for line %s\n", DEV_ID_LOG(d), l->name);
		return;
	}

	c->softswitch_action = SCCP_SOFTSWITCH_GETMEETMEROOM;							/* SoftSwitch will catch a number to be dialed */
	c->ss_data = 0;												/* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	//sccp_device_setActiveChannel(d, c);
	sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
		pbx_log(LOG_WARNING, "%s: (handle_meetme) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return;
	}

	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(c);
	}

	/* removing scheduled dial */
	sccp_channel_stop_schedule_digittimout(c);
}

/*!
 * \brief SCCP Meetme Application Config Structure
 */
static struct meetmeAppConfig {
	const char *appName;
	const char *defaultMeetmeOption;
} meetmeApps[] = {
	/* *INDENT-OFF* */
	{"MeetMe", 	"qd"}, 
	{"ConfBridge", 	"Mac"}, 
	{"Konference", 	"MTV"}
	/* *INDENT-ON* */
};

/*!
 * \brief a Meetme Application Thread
 * \param data Data
 * \author Federico Santulli
 *
 */
static void *sccp_feat_meetme_thread(void *data)
{
	struct meetmeAppConfig *app = NULL;

	char ext[SCCP_MAX_EXTENSION];
	char context[SCCP_MAX_CONTEXT];

	char meetmeopts[SCCP_MAX_CONTEXT];

#if ASTERISK_VERSION_NUMBER >= 10600
#define SCCP_CONF_SPACER ','
#endif

#if ASTERISK_VERSION_NUMBER >= 10400 && ASTERISK_VERSION_NUMBER < 10600
#define SCCP_CONF_SPACER '|'
#endif

#if ASTERISK_VERSION_NUMBER >= 10400
	unsigned int eid = pbx_random();
#else
	unsigned int eid = random();

#define SCCP_CONF_SPACER '|'
#endif
	AUTO_RELEASE sccp_channel_t * c = sccp_channel_retain(data);

	if (!c) {
		pbx_log(LOG_NOTICE, "SCCP: no channel provided for meetme feature. exiting\n");
		return NULL;
	}
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);

	if (!d) {
		pbx_log(LOG_NOTICE, "SCCP: no device provided for meetme feature. exiting\n");
		return NULL;
	}

	/* searching for meetme app */
	uint32_t i;

	for (i = 0; i < sizeof(meetmeApps) / sizeof(struct meetmeAppConfig); i++) {
		if (pbx_findapp(meetmeApps[i].appName)) {
			app = &(meetmeApps[i]);
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: using '%s' for meetme\n", meetmeApps[i].appName);
			break;
		}
	}
	/* finish searching for meetme app */

	if (!app) {												// \todo: remove res in this line: Although the value stored to 'res' is used in the enclosing expression, the value is never actually read from 'res'
		pbx_log(LOG_WARNING, "SCCP: No MeetMe application available!\n");
		//c = sccp_channel_retain(c);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
		sccp_channel_set_calledparty(c, SKINNY_DISP_CONFERENCE, c->dialedNumber);
		sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_PROCEED);
		sccp_channel_send_callinfo(d, c);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDCONFERENCE);
		return NULL;
	}
	// SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE
	if (c && c->owner) {
		if (!pbx_channel_context(c->owner) || sccp_strlen_zero(pbx_channel_context(c->owner))) {
			return NULL;
		}
		if (!sccp_strlen_zero(c->line->meetmeopts)) {
			snprintf(meetmeopts, sizeof(meetmeopts), "%s%c%s", c->dialedNumber, SCCP_CONF_SPACER, c->line->meetmeopts);
		} else if (!sccp_strlen_zero(d->meetmeopts)) {
			snprintf(meetmeopts, sizeof(meetmeopts), "%s%c%s", c->dialedNumber, SCCP_CONF_SPACER, d->meetmeopts);
		} else if (!sccp_strlen_zero(GLOB(meetmeopts))) {
			snprintf(meetmeopts, sizeof(meetmeopts), "%s%c%s", c->dialedNumber, SCCP_CONF_SPACER, GLOB(meetmeopts));
		} else {
			snprintf(meetmeopts, sizeof(meetmeopts), "%s%c%s", c->dialedNumber, SCCP_CONF_SPACER, app->defaultMeetmeOption);
		}

		sccp_copy_string(context, pbx_channel_context(c->owner), sizeof(context));

		snprintf(ext, sizeof(ext), "sccp_meetme_temp_conference_%ud", eid);

		if (!pbx_exists_extension(NULL, context, ext, 1, NULL)) {
			pbx_add_extension(context, 1, ext, 1, NULL, NULL, app->appName, meetmeopts, NULL, "sccp_feat_meetme_thread");
			pbx_log(LOG_WARNING, "SCCP: create extension exten => %s,%d,%s(%s)\n", ext, 1, app->appName, meetmeopts);
		}
		// sccp_copy_string(c->owner->exten, ext, sizeof(c->owner->exten));
		iPbx.setChannelExten(c, ext);

		c = sccp_channel_retain(c);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
		sccp_channel_set_calledparty(c, SKINNY_DISP_CONFERENCE, c->dialedNumber);
		//  sccp_channel_setSkinnyCallstate(c, SKINNY_CALLSTATE_PROCEED);
		sccp_channel_setChannelstate(c, SCCP_CHANNELSTATE_PROCEED);
		sccp_channel_send_callinfo(d, c);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTEDCONFERENCE);

		if (pbx_pbx_run(c->owner)) {
			sccp_indicate(d, c, SCCP_CHANNELSTATE_INVALIDCONFERENCE);
			pbx_log(LOG_WARNING, "SCCP: SCCP_CHANNELSTATE_INVALIDCONFERENCE\n");
		}
		ast_context_remove_extension(context, ext, 1, NULL);
	}
	return NULL;
}

/*!
 * \brief Start a Meetme Application Thread
 * \param c SCCP Channel
 * \author Federico Santulli
 */
void sccp_feat_meetme_start(channelPtr c)
{
	sccp_threadpool_add_work(GLOB(general_threadpool), (void *) sccp_feat_meetme_thread, (void *) c);
}

/*!
 * \brief Handle Barging into a Call
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param d SCCP Device
 * \return SCCP Channel
 *
 */
void sccp_feat_handle_barge(constLinePtr l, uint8_t lineInstance, constDevicePtr d)
{

	if (!l || !d || sccp_strlen_zero(d->id)) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}

	/* look if we have a call */
	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

		if (c) {
			// we have a channel, checking if
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				// we are dialing but without entering a number :D -FS
				sccp_dev_stoptone(d, lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETBARGEEXTEN;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = 0;									/* this should be found in thread */
				// changing channelstate to GETDIGITS
				sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
			} else if (!sccp_channel_hold(c)) {
				/* there is an active call, let's put it on hold first */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE sccp_channel_t *c = sccp_channel_allocate(l, d);

	if (!c) {
		pbx_log(LOG_ERROR, "%s: (handle_barge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return;
	}

	c->softswitch_action = SCCP_SOFTSWITCH_GETBARGEEXTEN;							/* SoftSwitch will catch a number to be dialed */
	c->ss_data = 0;												/* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;
	sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
		pbx_log(LOG_WARNING, "%s: (handle_barge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return;
	}

	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(c);
	}
}

/*!
 * \brief Barging into a Call Feature
 * \param c SCCP Channel
 * \param exten Extention as char
 * \return Success as int
 */
int sccp_feat_barge(constChannelPtr c, const char * const exten)
{
	/* sorry but this is private code -FS */
	if (!c) {
		return -1;
	}
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);

	if (!d) {
		return -1;
	}
	uint8_t instance = sccp_device_find_index_for_line(d, c->line->name);

	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	return 1;
}

/*!
 * \brief Handle Barging into a Conference
 * \param l SCCP Line
 * \param lineInstance lineInstance as uint8_t
 * \param d SCCP Device
 * \return SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 *
 */
void sccp_feat_handle_cbarge(constLinePtr l, uint8_t lineInstance, constDevicePtr d)
{

	if (!l || !d || sccp_strlen(d->id) < 3) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}

	/* look if we have a call */
	{
		AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

		if (c) {
			// we have a channel, checking if
			if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				// we are dialing but without entering a number :D -FS
				sccp_dev_stoptone(d, lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETBARGEEXTEN;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = 0;									/* this should be found in thread */
				// changing channelstate to GETDIGITS
				sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
			} else if (!sccp_channel_hold(c)) {
				/* there is an active call, let's put it on hold first */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE sccp_channel_t *c = sccp_channel_allocate(l, d);

	if (!c) {
		pbx_log(LOG_ERROR, "%s: (handle_cbarge) Can't allocate SCCP channel for line %s\n", d->id, l->name);
		return;
	}

	c->softswitch_action = SCCP_SOFTSWITCH_GETCBARGEROOM;							/* SoftSwitch will catch a number to be dialed */
	c->ss_data = 0;												/* not needed here */

	c->calltype = SKINNY_CALLTYPE_OUTBOUND;

	//sccp_device_setActiveChannel(d, c);
	sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	/* ok the number exist. allocate the asterisk channel */
	if (!sccp_pbx_channel_allocate(c, NULL, NULL)) {
		pbx_log(LOG_WARNING, "%s: (handle_cbarge) Unable to allocate a new channel for line %s\n", d->id, l->name);
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONGESTION);
		return;
	}

	iPbx.set_callstate(c, AST_STATE_OFFHOOK);

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.rtp) {
		sccp_channel_openReceiveChannel(c);
	}
}

/*!
 * \brief Barging into a Conference Feature
 * \param c SCCP Channel
 * \param conferencenum Conference Number as char
 * \return Success as int
 */
int sccp_feat_cbarge(constChannelPtr c, const char * const conferencenum)
{
	/* sorry but this is private code -FS */
	if (!c) {
		return -1;
	}
	AUTO_RELEASE sccp_device_t *d = sccp_channel_getDevice_retained(c);

	if (!d) {
		return -1;
	}
	uint8_t instance = sccp_device_find_index_for_line(d, c->line->name);

	sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	return 1;
}

/*!
 * \brief Hotline Feature
 *
 * Setting the hotline Feature on a device, will make it connect to a predefined extension as soon as the Receiver
 * is picked up or the "New Call" Button is pressed. No number has to be given.
 *
 * \param d SCCP Device
 * \param line SCCP Line
 */
void sccp_feat_adhocDial(constDevicePtr d, constLinePtr line)
{
	if (!d || !d->session || !line) {
		return;
	}
	sccp_log((DEBUGCAT_FEATURE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_3 "%s: handling hotline\n", d->id);

	AUTO_RELEASE sccp_channel_t *c = sccp_device_getActiveChannel(d);

	if (c) {
		if ((c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK)) {
			sccp_copy_string(c->dialedNumber, line->adhocNumber, sizeof(c->dialedNumber));
			sccp_channel_stop_schedule_digittimout(c);

			sccp_pbx_softswitch(c);
			return;
		}
		//sccp_pbx_senddigits(c, line->adhocNumber);
		if (iPbx.send_digits) {
			iPbx.send_digits(c, line->adhocNumber);
		}
	} else {
		// Pull up a channel
		if (GLOB(hotline)->line) {
			AUTO_RELEASE sccp_channel_t *new_channel = NULL;

			new_channel = sccp_channel_newcall(line, d, line->adhocNumber, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL);
		}
	}
}

/*!
 * \brief Handler to Notify Features have Changed
 * \param device SCCP Device
 * \param linedevice SCCP LineDevice
 * \param featureType SCCP Feature Type
 * 
 */
void sccp_feat_changed(constDevicePtr device, const sccp_linedevices_t * const linedevice, sccp_feature_type_t featureType)
{
	if (device) {
		sccp_featButton_changed(device, featureType);

		sccp_event_t event = {{{0}}};
		event.type = SCCP_EVENT_FEATURE_CHANGED;
		event.event.featureChanged.device = sccp_device_retain(device);
		event.event.featureChanged.optional_linedevice = linedevice ? sccp_linedevice_retain(linedevice) : NULL;
		event.event.featureChanged.featureType = featureType;
		sccp_event_fire(&event);
		sccp_log(DEBUGCAT_FEATURE) (VERBOSE_PREFIX_3 "%s: Feature %s Change Event Scheduled\n", device->id, sccp_feature_type2str(featureType));
	}
}

/*!
 * \brief Feature Monitor
 * \param device SCCP Device
 * \param no_line NULL pointer
 * \param no_lineInstance 0 Value
 * \param channel SCCP Channel
 */
void sccp_feat_monitor(constDevicePtr device, constLinePtr no_line, uint32_t no_lineInstance, constChannelPtr channel)
{
	sccp_featureConfiguration_t *monitorFeature = (sccp_featureConfiguration_t *)&device->monitorFeature;		/* discard const */
	if (!channel) {
		if (monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) {
			monitorFeature->status &= ~SCCP_FEATURE_MONITOR_STATE_REQUESTED;
		} else {
			monitorFeature->status |= SCCP_FEATURE_MONITOR_STATE_REQUESTED;
		}
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_feat_monitor) No active channel to monitor, setting monitor state to requested (%d)\n", device->id, monitorFeature->status);
	} else {
		/* check if we need to start or stop monitor */
		/*
		   if (((monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) >> 1) == (monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
		   sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: no need to update monitor state (%d)\n", device->id, monitorFeature->status);
		   return;
		   }
		 */
		if (iPbx.feature_monitor(channel)) {
			if (monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_ACTIVE) {		// Just toggle the state, we don't get information back about the asterisk monitor status (async call)
				monitorFeature->status &= ~SCCP_FEATURE_MONITOR_STATE_ACTIVE;
			} else {
				monitorFeature->status |= SCCP_FEATURE_MONITOR_STATE_ACTIVE;
			}
		} else {											// monitor feature missing
			monitorFeature->status = SCCP_FEATURE_MONITOR_STATE_DISABLED;
		}
	}
	sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_feat_monitor) monitor status: %d\n", device->id, monitorFeature->status);
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
