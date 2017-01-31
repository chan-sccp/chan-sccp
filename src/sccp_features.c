/*!
 * \file        sccp_features.c
 * \brief       SCCP Features Class
 * \author      Federico Santulli <fsantulli [at] users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 *
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

#include "config.h"
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_featureButton.h"
#include "sccp_features.h"
#include "sccp_line.h"
#include "sccp_pbx.h"
#include "sccp_conference.h"
#include "sccp_indicate.h"
#include "sccp_management.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "");

#include <asterisk/causes.h>		//AST_CAUSE_NORMAL_CLEARING

#if CS_SCCP_PICKUP
#  if defined(CS_AST_DO_PICKUP) && defined(HAVE_PBX_FEATURES_H)
#    include <asterisk/features.h>
#    if ASTERISK_VERSION_GROUP >= 112
#      include <asterisk/pickup.h>
#    endif
#  endif
#endif

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

	AUTO_RELEASE(sccp_linedevices_t, linedevice , sccp_linedevice_find(device, l));

	if (!linedevice) {
		pbx_log(LOG_ERROR, "%s: Device does not have line configured \n", DEV_ID_LOG(device));
		return;
	}

	/* if call forward is active -> disable */
	if (linedevice->cfwdAll.enabled && type == SCCP_CFWD_ALL) {
		sccp_line_cfwd(l, device, SCCP_CFWD_NONE, NULL);
		sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDALL, linedevice->lineInstance, SKINNY_LAMP_OFF);
		return;
	} else if (linedevice->cfwdBusy.enabled && type == SCCP_CFWD_BUSY) {
		sccp_line_cfwd(l, device, SCCP_CFWD_NONE, NULL);
		sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDBUSY, linedevice->lineInstance, SKINNY_LAMP_OFF);
		return;
	} else if (type == SCCP_CFWD_NOANSWER) {
		sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "### CFwdNoAnswer NOT SUPPORTED\n");
		sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDNOANSWER, linedevice->lineInstance, SKINNY_LAMP_OFF);
		return;
	}
	/* look if we have a call  */
	AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(device));
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
						sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, linedevice->lineInstance, c->callid, SKINNY_TONEDIRECTION_USER);
						sccp_channel_endcall(c);
						return;
					}
				} else if (iPbx.channel_is_bridged(c)) {					// check if we have an ast channel to get callerid from
					// if we have an incoming or forwarded call, let's get number from callerid :) -FS
					char *number = NULL;
					if (iPbx.get_callerid_name) {
						iPbx.get_callerid_number(c->owner, &number);
					}
					if (number) {
						sccp_line_cfwd(l, device, type, number);
						pbx_log(LOG_ERROR, "%s: 4\n", DEV_ID_LOG(device));
						// we are on call, so no tone has been played until now :)
						sccp_dev_starttone(device, SKINNY_TONE_ZIPZIP, linedevice->lineInstance, c->callid, SKINNY_TONEDIRECTION_USER);
						sccp_channel_endcall(c);
						sccp_free(number);
						return;
					}
					// if we where here it is because there is no number in callerid. Put call on hold and ask for a call forward number :) -FS
					if (!sccp_channel_hold(c)) {
						// if can't hold  it means there is no active call, so return as we're already waiting a number to dial
						sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
						return;
					}
				}
			} else if (c->state == SCCP_CHANNELSTATE_OFFHOOK && sccp_strlen_zero(c->dialedNumber)) {
				pbx_log(LOG_ERROR, "%s: 5\n", DEV_ID_LOG(device));
				sccp_dev_stoptone(device, linedevice->lineInstance, (c && c->callid) ? c->callid : 0);
				// changing SOFTSWITCH_DIALING mode to SOFTSWITCH_GETFORWARDEXTEN
				c->softswitch_action = SCCP_SOFTSWITCH_GETFORWARDEXTEN;				/* SoftSwitch will catch a number to be dialed */
				c->ss_data = type;								/* this should be found in thread */
				sccp_indicate(device, c, SCCP_CHANNELSTATE_GETDIGITS);
				iPbx.set_callstate(c, AST_STATE_OFFHOOK);
				return;
			} else {
				// we cannot allocate a channel, or ask an extension to pickup.
				sccp_dev_displayprompt(device, 0, 0, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
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
			if (!sccp_channel_hold(c)) {								// put on hold, other call is allready in progress
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
	switch (type) {
		case SCCP_CFWD_ALL:
			sccp_dev_displayprompt(device, linedevice->lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER_TO_FORWARD_TO, SCCP_DISPLAYSTATUS_TIMEOUT);
			sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDALL, linedevice->lineInstance, SKINNY_LAMP_FLASH);
			break;
		case SCCP_CFWD_BUSY:
			sccp_dev_displayprompt(device, linedevice->lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER_TO_FORWARD_TO, SCCP_DISPLAYSTATUS_TIMEOUT);
			sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDBUSY, linedevice->lineInstance, SKINNY_LAMP_FLASH);
			break;
		/*
		// already tested for at the beginning of this function: currently we do not support cfwd_noasnwer, so no need for handling it here
		case SCCP_CFWD_NOANSWER:
			sccp_dev_displayprompt(device, linedevice->lineInstance, c->callid, SKINNY_DISP_ENTER_NUMBER_TO_FORWARD_TO, SCCP_DISPLAYSTATUS_TIMEOUT);
			sccp_device_setLamp(device, SKINNY_STIMULUS_FORWARDNOANSWER, linedevice->lineInstance, SKINNY_LAMP_FLASH);
			break;
		*/
		default:
			sccp_dev_displayprompt(device, linedevice->lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
			break;
	}			
	if (device->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
		sccp_channel_openReceiveChannel(c);
	}
}

#ifdef CS_SCCP_PICKUP
/*!
 * sccp pickup helper function
 * \note: function is called with target locked
 */
static int sccp_feat_perform_pickup(constDevicePtr d, channelPtr c, PBX_CHANNEL_TYPE *target, boolean_t answer)
{
	int res = 0;
	pbx_assert(c != NULL);

#if CS_AST_DO_PICKUP
	PBX_CHANNEL_TYPE *original = c->owner;
	char *target_name = NULL;
	char *target_number = NULL;
	if (iPbx.get_callerid_name) {
		iPbx.get_callerid_name(target, &target_name);
	}
	if (iPbx.get_callerid_number) {
		iPbx.get_callerid_number(target, &target_number);
	}

//	if (res == 0) {
		sccp_channel_stop_schedule_digittimout(c);
		c->calltype = SKINNY_CALLTYPE_INBOUND;						// reset call direction
		c->state = SCCP_CHANNELSTATE_RINGING;
		c->ringermode = answer ? SKINNY_RINGTYPE_SILENT : SKINNY_RINGTYPE_FEATURE;
		c->answered_elsewhere = TRUE;
		int lineInstance = sccp_device_find_index_for_line(d, c->line->name);
		if (d->directed_pickup_modeanswer) {
			sccp_dev_set_keyset(d, lineInstance, c->callid, KEYMODE_RINGIN);		// setting early to prevent getting multiple pickup button presses
		}

		char called_number[StationMaxDirnumSize] = {0};
		char called_name[StationMaxNameSize] = {0};

		/* Gather CallInfo */
		sccp_callinfo_t *callinfo_orig = NULL;
		callinfo_orig = sccp_channel_getCallInfo(c);
		iCallInfo.Getter(callinfo_orig,							// picker
			SCCP_CALLINFO_CALLEDPARTY_NAME, &called_name,				// name of picker
			SCCP_CALLINFO_CALLEDPARTY_NUMBER, &called_number,
			SCCP_CALLINFO_KEY_SENTINEL);

		//pbx_channel_ref(original);
		res = ast_do_pickup(original, target);
		pbx_channel_unlock(target);
		if (!res) {									// directed pickup succeeded
			sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (perform_pickup) pickup succeeded on call: %s\n", DEV_ID_LOG(d), c->designator);
			/* disconnect from masquaraded zombie channel */
			sccp_channel_setDevice(c, NULL);
			pbx_channel_set_hangupcause(original, AST_CAUSE_ANSWERED_ELSEWHERE);
			
			/* continue with masquaraded channel */
			pbx_channel_set_hangupcause(c->owner, AST_CAUSE_NORMAL_CLEARING);	// reset picked up channel

			callinfo_orig = sccp_channel_getCallInfo(c);
			iCallInfo.Setter(callinfo_orig, 					// update calling end
				SCCP_CALLINFO_CALLEDPARTY_NAME, called_name, 			// channel picking up
				SCCP_CALLINFO_CALLEDPARTY_NUMBER, called_number, 
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME, target_name, 
				SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER, target_number, 
				SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, 4,
				SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME, called_name,
				SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER, called_number,
				SCCP_CALLINFO_HUNT_PILOT_NAME, target_name,			// display orig called using HUNT
				SCCP_CALLINFO_HUNT_PILOT_NUMBER, target_number,			// gets displayed as 'FOR'
				SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON, 4,
				SCCP_CALLINFO_LAST_REDIRECT_REASON, 4,
				SCCP_CALLINFO_KEY_SENTINEL);

			sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (perform_pickup) channel:%s, modeanser: %s\n", DEV_ID_LOG(d), c->designator, answer ? "yes" : "no");
			if (answer) {
				sccp_channel_setDevice(c, d);
				sccp_dev_setActiveLine((sccp_device_t *)d, c->line);

				/* emulate previous indications, before signalling connected */
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_RINGIN, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_dev_set_cplane(d, lineInstance, 1);
				sccp_device_sendcallstate(d, lineInstance, c->callid, SKINNY_CALLSTATE_OFFHOOK, SKINNY_CALLPRIORITY_LOW, SKINNY_CALLINFO_VISIBILITY_DEFAULT);
				sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);
			} else {
				/* remove previous call plane, used to dial pickup extension */
				sccp_dev_deactivate_cplane(d);
				sccp_parse_alertinfo(c->owner, &c->ringermode);
				sccp_indicate(d, c, SCCP_CHANNELSTATE_RINGING);
				sccp_dev_set_cplane(d, lineInstance, 1);
			}
			/* hangup masqueraded zombie channel*/
			if (pbx_test_flag(pbx_channel_flags(original), AST_FLAG_ZOMBIE)) {
				pbx_hangup(original);
			}
		} else {									// pickup failed
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: (perform_pickup) Giving Up\n");
			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL " " SKINNY_DISP_OPICKUP, SCCP_DISPLAYSTATUS_TIMEOUT);
			sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, lineInstance, c->callid, SKINNY_TONEDIRECTION_USER);
			sccp_channel_schedule_hangup(c, 5000);
		}
//	}
	//pbx_channel_unlock(target);
#else
	pbx_log(LOG_NOTICE, "%s: (directed_pickup) no support for pickup in asterisk\n");
#endif
	return res;
}

/*!
 * \brief Handle Direct Pickup of Line
 * \param d SCCP Device
 * \param l SCCP Line
 * \param maybe_c Optional SCCP Channel
 * \return SCCP Channel
 *
 */
void sccp_feat_handle_directed_pickup(constDevicePtr d, constLinePtr l, channelPtr maybe_c)
{
#if CS_AST_DO_PICKUP
	if (!l || !d) {
		pbx_log(LOG_ERROR, "SCCP: Can't allocate SCCP channel if line or device are not defined!\n");
		return;
	}
	{
		AUTO_RELEASE(sccp_linedevices_t, ld , sccp_linedevice_find(d, l));
		if (!ld->isPickupAllowed()) {
			pbx_log(LOG_NOTICE, "%s: (directed_pickup) pickup button has been disabled for line:%s (already pressed pickup on this call).\n", d->id, l->name);
			return;
		}
		sccp_linedevice_disallowPickup(ld);
	}
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_getEmptyChannel(l, d, maybe_c, SKINNY_CALLTYPE_INBOUND, NULL, NULL));
	if (c) {
		c->softswitch_action = SCCP_SOFTSWITCH_GETPICKUPEXTEN;						/* SoftSwitch will catch a number to be dialed */
		c->ss_data = 0;											/* not needed here */
		sccp_indicate(d, c, SCCP_CHANNELSTATE_GETDIGITS);
		iPbx.set_callstate(c, AST_STATE_OFFHOOK);
		sccp_channel_stop_schedule_digittimout(c);

		if (d->directed_pickup_modeanswer && d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
			sccp_channel_openReceiveChannel(c);
		}
	}
#else
	pbx_log(LOG_NOTICE, "%s: (directed_pickup) no support for pickup in asterisk\n");
#endif
}

/*!
 * \brief Handle Direct Pickup of Extension
 * \param d SCCP Device
 * \param c SCCP Channel
 * \param lineInstance Line Instance as uint8_t 
 * \param exten Extension as char
 * \return Success as int
 *
 * \lock
 *  - asterisk channel
 */
int sccp_feat_directed_pickup(constDevicePtr d, channelPtr c, uint32_t lineInstance, const char *exten)
{
#if CS_AST_DO_PICKUP
	int res = -1;

	/* assertions */
	pbx_assert(c && c->line && c->owner && d);
	if (!c->line->pickupgroup
#if CS_AST_HAS_NAMEDGROUP
	    && sccp_strlen_zero(c->line->namedpickupgroup)
#endif
	    ) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (directedpickup) pickupgroup not configured in sccp.conf\n", d->id);
		return -1;
	}

	char *context;
	if (sccp_strlen_zero(exten)) {
		pbx_log(LOG_ERROR, "SCCP: (directed_pickup) zero exten. Giving up.\n");
		return -1;
	}

	if (!iPbx.findPickupChannelByExtenLocked) {
		pbx_log(LOG_WARNING, "SCCP: (directed_pickup) findPickupChannelByExtenLocked not implemented for this asterisk version. Giving up.\n");
		return -1;
	}
	/* end assertions */

	if ((context = strchr(exten, '@'))) {
		*context++ = '\0';
	} else {
		if (!sccp_strlen_zero(d->directed_pickup_context)) {
			context = pbx_strdupa(d->directed_pickup_context);
		} else {
			context = pbx_strdupa(pbx_channel_context(c->owner));
		}
	}
	if (sccp_strlen_zero(context)) {
		pbx_log(LOG_ERROR, "SCCP: (directed_pickup) We could not find a context for this line. Giving up !\n");
		return -1;
	}

	/* do pickup */
	PBX_CHANNEL_TYPE *target = NULL;									/* potential pickup target */
	PBX_CHANNEL_TYPE *original = c->owner;
	if (pbx_channel_ref(original)) {
		// make sure the new channel does not participate in the potential pickup candidates
		if (iPbx.set_callgroup) {
			iPbx.set_callgroup(c, 0);
		}
		if (iPbx.set_named_callgroups) {
			iPbx.set_named_callgroups(c, NULL);
		}

		pbx_log(LOG_NOTICE, "%s: executing directed_pickup for %s@%s\n", c->designator, exten, context);
		target = iPbx.findPickupChannelByExtenLocked(original, exten, context);
		if (target) {
			res = sccp_feat_perform_pickup(d, c, target, d->directed_pickup_modeanswer);			/* unlocks target */
			target = pbx_channel_unref(target);
		} else {
			sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (directed_pickup) findPickupChannelByExtenLocked failed on call: %s\n", DEV_ID_LOG(d), c->designator);
			sccp_dev_displayprinotify(d, SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
			if (c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_DOWN) {
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, SKINNY_TONEDIRECTION_USER);
			} else {
				sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, lineInstance, c->callid, SKINNY_TONEDIRECTION_USER);
			}
			sccp_channel_schedule_hangup(c, 500);
		}
		pbx_channel_unref(original);
	} else {
		pbx_log(LOG_NOTICE, "SCCP: Unable to grab a reference of the original channel owner\n");
	}
#else
	pbx_log(LOG_NOTICE, "%s: (directed_pickup) no support for pickup in asterisk\n");
#endif
	return res;
}

/*!
 * \brief Handle Group Pickup Feature
 * \param d SCCP Device
 * \param l SCCP Line
 * \param lineInstance Line Instance as uint8_t
 * \param maybe_c Optioonal SCCP Channel
 * \return Success as int
 * 
 * \todo backport from trunk
 *
 * \lock
 *  - asterisk channel
 *
 * \todo Fix callerid setting before calling ast_pickup_call
 */
int sccp_feat_grouppickup(constDevicePtr d, constLinePtr l, uint32_t lineInstance, channelPtr maybe_c)
{
	int res = -1;

	/* assertions */
	pbx_assert(d != NULL && l != NULL);
#if CS_AST_DO_PICKUP
	if (!iPbx.findPickupChannelByGroupLocked) {
		pbx_log(LOG_WARNING, "SCCP: (directed_pickup) findPickupChannelByExtenLocked not implemented for this asterisk version. Giving up.\n");
		return -1;
	}

	if (!l->pickupgroup
#if CS_AST_HAS_NAMEDGROUP
	    && sccp_strlen_zero(l->namedpickupgroup)
#endif
	    ) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: (grouppickup) pickupgroup not configured in sccp.conf\n", d->id);
		return -1;
	}
	{
		AUTO_RELEASE(sccp_linedevices_t, ld , sccp_linedevice_find(d, l));
		if (!ld->isPickupAllowed()) {
			pbx_log(LOG_NOTICE, "%s: (directed_pickup) pickup button has been disabled for line:%s (already pressed pickup on this call).\n", d->id, l->name);
			return -1 ;
		}
		sccp_linedevice_disallowPickup(ld);
	}
	/* end assertions */

	pbx_log(LOG_NOTICE, "%s: (gpickup) get channel: %s.\n", d->id, maybe_c ? maybe_c->designator : "<null>");
	/* re-use/create channel for pickup */
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_getEmptyChannel(l, d, maybe_c, SKINNY_CALLTYPE_INBOUND, NULL, NULL));
	if (c) {
		// make sure the new channel does not participate in the potential pickup candidates
		if (iPbx.set_callgroup) {
			iPbx.set_callgroup(c, 0);
		}
		if (iPbx.set_named_callgroups) {
			iPbx.set_named_callgroups(c, NULL);
		}

		pbx_log(LOG_NOTICE, "%s: executing_gpickup\n", c->designator);
		PBX_CHANNEL_TYPE *target = NULL;									/* potential pickup target */
		PBX_CHANNEL_TYPE *original = c->owner;
		if (pbx_channel_ref(original)) {
			sccp_channel_stop_schedule_digittimout(c);
			if ((target = iPbx.findPickupChannelByGroupLocked(c->owner))) {
				res = sccp_feat_perform_pickup(d, c, target, d->directed_pickup_modeanswer);			/* unlocks target */
				target = pbx_channel_unref(target);
				//res = 0;
			} else {
				sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (gpickup) findPickupChannelByExtenLocked failed on call: %s\n", DEV_ID_LOG(d), c->designator);
				sccp_dev_displayprinotify(d, SKINNY_DISP_NO_CALL_AVAILABLE_FOR_PICKUP, SCCP_MESSAGE_PRIORITY_TIMEOUT, 5);
				if (c->state == SCCP_CHANNELSTATE_ONHOOK || c->state == SCCP_CHANNELSTATE_DOWN) {
					sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, 0, 0, SKINNY_TONEDIRECTION_USER);
				} else {
					sccp_dev_starttone(d, SKINNY_TONE_BEEPBONK, lineInstance, c->callid, SKINNY_TONEDIRECTION_USER);
				}
				sccp_channel_schedule_hangup(c, 500);
			}
			pbx_channel_unref(original);
		} else {
			pbx_log(LOG_NOTICE, "SCCP: Unable to grab a reference of the original channel owner\n");
		}
	}
#else
	pbx_log(LOG_NOTICE, "%s: (directed_pickup) no support for pickup in asterisk\n");
#endif
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
		AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(d));

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

	AUTO_RELEASE(sccp_line_t, l , sccp_line_find_byid(d, lineInstance));

	if (!l) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: No line with instance %d found.\n", d->id, lineInstance);

		// TODO(dkgroot): workaround to solve the voicemail button issue with old hint style and speeddials before first line -MC
		if (d->defaultLineInstance) {
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		}
	}
	if (l) {
		if (!sccp_strlen_zero(l->vmnum)) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "%s: Dialing voicemail %s\n", d->id, l->vmnum);
			AUTO_RELEASE(sccp_channel_t, new_channel , NULL);

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

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_getEmptyChannel(l, d, channel, SKINNY_CALLTYPE_OUTBOUND, NULL, NULL));
	if (c) {
		c->softswitch_action = SCCP_SOFTSWITCH_GETCONFERENCEROOM;
		c->ss_data = 0;											/* not needed here */
		c->calltype = SKINNY_CALLTYPE_OUTBOUND;
		sccp_indicate(d, c, SCCP_CHANNELSTATE_DIALING);
		iPbx.set_callstate(c, AST_STATE_OFFHOOK);
		sccp_channel_stop_schedule_digittimout(c);
		if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
			sccp_channel_openReceiveChannel(c);
	 	}
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
 * \param lineInstance lineInstance as uint8_t
 * \param c SCCP Channel
 * \return Success as int
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 *
 */
void sccp_feat_conference_start(constDevicePtr device, const uint32_t lineInstance, channelPtr c)
{
	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(device));

	if (!d || !c) {
		pbx_log(LOG_NOTICE, "%s: (sccp_feat_conference_start) Missing Device or Channel\n", DEV_ID_LOG(device));
		return;
	}
#ifdef CS_SCCP_CONFERENCE
	AUTO_RELEASE(sccp_channel_t, channel , NULL);
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
					AUTO_RELEASE(sccp_line_t, line , sccp_line_retain(d->buttonTemplate[i].ptr));

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
	AUTO_RELEASE(sccp_device_t, d , sccp_device_retain(device));

	if (!c || !d) {
		pbx_log(LOG_NOTICE, "%s: (sccp_feat_join) Missing Device or Channel\n", DEV_ID_LOG(d));
		return;
	}
#if CS_SCCP_CONFERENCE
	AUTO_RELEASE(sccp_channel_t, newparticipant_channel , sccp_device_getActiveChannel(d));
	sccp_channel_t *moderator_channel = NULL;
	PBX_CHANNEL_TYPE *bridged_channel = NULL;

	if (!d->allow_conference) {
		pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (!d->conference) {
		pbx_log(LOG_NOTICE, "%s: There is currently no active conference on this device. Start Conference First.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_NO_CONFERENCE_BRIDGE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (!newparticipant_channel) {
		pbx_log(LOG_NOTICE, "%s: No active channel on device to join to the conference.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_CAN_NOT_COMPLETE_CONFERENCE, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else if (newparticipant_channel->conference) {
		pbx_log(LOG_NOTICE, "%s: Channel is already part of a conference.\n", DEV_ID_LOG(d));
		sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_IN_CONFERENCE_ALREADY, SCCP_DISPLAYSTATUS_TIMEOUT);
	} else {
		AUTO_RELEASE(sccp_conference_t, conference , sccp_conference_retain(d->conference));

		SCCP_LIST_LOCK(&((sccp_line_t *const)l)->channels);
		SCCP_LIST_TRAVERSE(&l->channels, moderator_channel, list) {
			if (conference == moderator_channel->conference ) {
				break;
			}
		}
		SCCP_LIST_UNLOCK(&((sccp_line_t *const)l)->channels);
		sccp_conference_hold(conference);								// make sure conference is on hold (should already be on hold)
		if (moderator_channel) {
			if (newparticipant_channel && moderator_channel != newparticipant_channel) {
				sccp_channel_hold(newparticipant_channel);
				pbx_log(LOG_NOTICE, "%s: Joining new participant to conference\n", DEV_ID_LOG(d));
				if ((bridged_channel = iPbx.get_bridged_channel(newparticipant_channel->owner))) {
					sccp_log((DEBUGCAT_CONFERENCE + DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: sccp conference: channel %s, state: %s.\n", DEV_ID_LOG(d), pbx_channel_name(bridged_channel), sccp_channelstate2str(newparticipant_channel->state));
					if (!sccp_conference_addParticipatingChannel(conference, moderator_channel, newparticipant_channel, bridged_channel)) {
						sccp_dev_displayprompt(device, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
					}
					pbx_channel_unref(bridged_channel);
				} else {
					pbx_log(LOG_ERROR, "%s: sccp conference: bridgedchannel for channel %s could not be found\n", DEV_ID_LOG(d), pbx_channel_name(newparticipant_channel->owner));
				}
			} else {
				pbx_log(LOG_NOTICE, "%s: conference moderator could not be found on this phone\n", DEV_ID_LOG(d));
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_INVALID_CONFERENCE_PARTICIPANT, SCCP_DISPLAYSTATUS_TIMEOUT);
			}
			sccp_conference_update(conference);
			sccp_channel_resume(d, moderator_channel, FALSE);
		} else {
			pbx_log(LOG_NOTICE, "%s: Cannot use the JOIN button within a conference itself\n", DEV_ID_LOG(d));
			sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_KEY_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
		}
	}
#else
	pbx_log(LOG_NOTICE, "%s: conference not enabled\n", DEV_ID_LOG(d));
	sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, SCCP_DISPLAYSTATUS_TIMEOUT);
#endif
}

/*!
 * \brief Handle Conference List
 * \param d SCCP Device
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
		if (c && c->conference) {
			d->conferencelist_active = TRUE;
			sccp_conference_show_list(c->conference, c);
		}
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
		AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(d));

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
			} 
			if (!sccp_channel_hold(c)) {
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_allocate(l, d));

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

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
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

	unsigned int eid = sccp_random();
	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_retain(data));

	if (!c) {
		pbx_log(LOG_NOTICE, "SCCP: no channel provided for meetme feature. exiting\n");
		return NULL;
	}
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

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
		sccp_indicate(d, c, SCCP_CHANNELSTATE_CONNECTED);

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
		AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(d));

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
			} if (!sccp_channel_hold(c)) {
				/* there is an active call, let's put it on hold first */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_allocate(l, d));

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

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
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
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

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
		AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(d));

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
			} if (!sccp_channel_hold(c)) {
				/* there is an active call, let's put it on hold first */
				sccp_dev_displayprompt(d, lineInstance, c->callid, SKINNY_DISP_TEMP_FAIL, SCCP_DISPLAYSTATUS_TIMEOUT);
				return;
			}
		}
	}

	AUTO_RELEASE(sccp_channel_t, c , sccp_channel_allocate(l, d));

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

	if (d->earlyrtp <= SCCP_EARLYRTP_OFFHOOK && !c->rtp.audio.instance) {
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
	AUTO_RELEASE(sccp_device_t, d , sccp_channel_getDevice(c));

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

	AUTO_RELEASE(sccp_channel_t, c , sccp_device_getActiveChannel(d));

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
			AUTO_RELEASE(sccp_channel_t, new_channel , NULL);

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
 * \brief Switch Monitor Feature on/off
 *
 * \note: iPbx.feature_monitor will ask asterisk to start/stop automon
 * \note: asterisk ami events are evaluated and callback via sccp_asterisk_managerHookHelper upon change, which
 *        will in turn call sccp_feat_changed to update the monitor state (if such a state change happens)
 * \see: sccp_management.c for more information
 */
void sccp_feat_monitor(constDevicePtr device, constLinePtr no_line, uint32_t no_lineInstance, constChannelPtr maybe_channel)
{
	sccp_featureConfiguration_t *monitorFeature = (sccp_featureConfiguration_t *)&device->monitorFeature;		/* discard const */
	if (!maybe_channel) {
		if (monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_REQUESTED) {
			monitorFeature->status &= ~SCCP_FEATURE_MONITOR_STATE_REQUESTED;
		} else {
			monitorFeature->status |= SCCP_FEATURE_MONITOR_STATE_REQUESTED;
		}
	} else {
		constChannelPtr channel = maybe_channel;
		struct ast_str *amiCommandStr = ast_str_alloca(DEFAULT_PBX_STR_BUFFERSIZE);
		char *outStr;
		if (!(monitorFeature->status & SCCP_FEATURE_MONITOR_STATE_ACTIVE)) {
			pbx_str_append(&amiCommandStr,0 ,"Action: Monitor\r\n");
			pbx_str_append(&amiCommandStr,0 ,"Channel: %s\r\n", pbx_channel_name(channel->owner));
			pbx_str_append(&amiCommandStr,0 ,"File: mixmonitor-%s-%d_%s.wav\r\n", channel->line->name, channel->callid, iPbx.getChannelUniqueID(channel));
			pbx_str_append(&amiCommandStr,0 ,"Format: wav\r\n");
			pbx_str_append(&amiCommandStr,0 ,"Mix: true\r\n");
			pbx_str_append(&amiCommandStr,0 ,"\r\n");
			//monitorFeature->status &= ~SCCP_FEATURE_MONITOR_STATE_ACTIVE;					/* no need to change status, will be done by sccp_asterisk_managerHookHelper */
		} else {
			pbx_str_append(&amiCommandStr,0 ,"Action: StopMonitor\r\n");
			pbx_str_append(&amiCommandStr,0 ,"Channel: %s\r\n", pbx_channel_name(channel->owner));
			pbx_str_append(&amiCommandStr,0 ,"\r\n");
			//monitorFeature->status |= SCCP_FEATURE_MONITOR_STATE_ACTIVE;
		}
		if (sccp_manager_action2str(pbx_str_buffer(amiCommandStr), &outStr) && outStr) {
			if (	
				sccp_strequals(outStr, "Response: Success\r\nMessage: Started monitoring channel\r\n\r\n") ||
				sccp_strequals(outStr, "Response: Success\r\nMessage: Stopped monitoring channel\r\n\r\n")
			) {
				sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_feat_monitor) AMI monitor request sent successfully.\n", DEV_ID_LOG(device));
				// sccp_asterisk_managerHookHelper will catch the result and update the softkey / featureButton accordingly.
			} else {
				sccp_dev_displayprinotify(device, SKINNY_DISP_RECORDING_FAILED, SCCP_MESSAGE_PRIORITY_MONITOR, SCCP_DISPLAYSTATUS_TIMEOUT*3);
				pbx_log(LOG_ERROR, "%s: (sccp_feat_monitor) AMI monitor request failed.\n", DEV_ID_LOG(device));
				monitorFeature->status = SCCP_FEATURE_MONITOR_STATE_DISABLED;
			}
			sccp_free(outStr);
		} else {
			pbx_log(LOG_ERROR, "%s: (sccp_feat_monitor) AMI monitor request failed.\n", DEV_ID_LOG(device));
			monitorFeature->status = SCCP_FEATURE_MONITOR_STATE_DISABLED;
		}
	}
	sccp_log((DEBUGCAT_FEATURE)) (VERBOSE_PREFIX_3 "%s: (sccp_feat_monitor) new monitor status:%s (%d)\n", device->id, sccp_feature_monitor_state2str(monitorFeature->status), monitorFeature->status);
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
