/*!
 * \file 	sccp_softkeys_b.c
 * \brief 	SCCP SoftKeys
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \version 	$LastChangedDate$
 * \todo
 * 
 */

/*!
 * \page sccp_softkeys  Defined SoftKeys
 *
 * \ref sk_redial		\n
 * \ref sk_newcall		\n
 * \ref sk_hold			\n
 * \ref sk_resume		\n
 * \ref sk_transfer		\n
 * \ref sk_endcall		\n
 * \ref sk_dnd			\n
 * \ref sk_backspace		\n
 * \ref sk_answer		\n
 * \ref sk_select		\n
 * \ref sk_DirTrfr		\n
 * \ref sk_cfwdall		\n
 * \ref sk_cfwdbusy		\n
 * \ref sk_cfwdnoanswer		\n
 * \ref sk_park			\n
 * \ref sk_private		\n
 * \ref sk_conference		\n
 * \ref sk_join			\n
 * \ref sk_barge		\n
 * \ref sk_cbarge		\n
 * \ref sk_meetme		\n
 * \ref sk_pickup		\n
 * \ref sk_gpickup		\n
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_utils.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_indicate.h"
#include "sccp_protocol.h"
#include "sccp_features.h"
#include "sccp_pbx.h"
#include "sccp_actions.h"
#include "sccp_hint.h"
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/utils.h>
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#include <asterisk/callerid.h>
#include <asterisk/causes.h>
#endif
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#ifdef CS_AST_HAS_AST_STRING_FIELD
#include <asterisk/stringfields.h>
#endif

/*!
 * \page sk_redial 		The Redial SoftKey
 * \section sk_Redial_howto 	How to use the Redial Softkey
 *
 * Using the Redial Button you can Redial the last Called Number on this Line or Device
 */
 
/*!
 * \brief Redial last Dialed Number by this Device
 * \n Usage: \ref sk_redial
 * \param s Session
 * \param lineinst Line Instance as uint32_t
 * \param callid CallID as uint32_t
 */
void sccp_sk_redial(sccp_device_t * d , sccp_line_t * l, sccp_channel_t * c) {
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Redial Softkey.\n",d->id);

	if (ast_strlen_zero(d->lastNumber)) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No number to redial\n", d->id);
		return;
	}

	if (c) {
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
			/* we have a offhook channel */
			sccp_channel_lock(c);
			sccp_copy_string(c->dialedNumber, d->lastNumber, sizeof(c->dialedNumber));
			sccp_channel_unlock(c);
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Get ready to redial number %s\n", d->id, d->lastNumber);
			// c->digittimeout = time(0)+1;
			SCCP_SCHED_DEL(sched, c->digittimeout);
			sccp_pbx_softswitch(c);
		}
		/* here's a KEYMODE error. nothing to do */
		return;
	}
	if (!l)
		l = d->currentLine;
	sccp_channel_newcall(l, d, d->lastNumber, SKINNY_CALLTYPE_OUTBOUND);
}

/*!
 * \page sk_newcall 		The NewCall SoftKey
 * \section sk_NewCall_howto 	How to use the NewCall Softkey
 *
 * Using the New Call Button you can initiate a new Call just like picking up the receiver. 
 * You can either type in the number to be called before or afterwards.
 */
/*!
 * \brief Initiate a New Call
 * \n Usage: \ref sk_newcall
 * \param s Session
 * \param lineinst Line Instance as uint32_t
 * \param callid CallID as uint32_t
 */
void sccp_sk_newcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!l){
		/* use default line if it is set */
		if(d && d->defaultLineInstance >0){
			sccp_log(64)(VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		}
	}
	if(!l)
		l = d->currentLine;

	sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
}


/*!
 * \page sk_hold 		The Hold SoftKey
 * \section sk_Hold_howto 	How to use the Hold Softkey
 *
 * Using the Hold Button you can hold the current line and undertake some other action
 * You can later resume the held line use \ref sk_resume
 */
/*!
 * \brief Hold Call on Current Line
 * \n Usage: \ref sk_hold
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_hold(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_dev_displayprompt(d, 0, 0, "No call to put on hold.",5);
		return;
	}
	sccp_channel_hold(c);
}

/*!
 * \page sk_resume 		The Resume SoftKey
 * \section sk_Resume_howto 	How to use the Resume Softkey
 *
 * Using the Resume Button you can resume a previously help line which has been put on hold using \ref sk_hold
 */
/*!
 * \brief Resume Call on Current Line
 * \n Usage: \ref sk_resume
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_resume(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No call to resume. Ignoring\n", d->id);
		return;
	}
	sccp_channel_resume(d, c);
}

/*!
 * \page sk_transfer 		The Transfer SoftKey
 * \section sk_Resume_howto 	How to use the Transfer Softkey
 *
 * Using the Transfer Button you can Transfer the Currentline to another Number
 */
/*!
 * \brief Transfer Call on Current Line
 * \n Usage: \ref sk_transfer
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_transfer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Transfer when there is no active call\n", d->id);
		return;
	}
	sccp_channel_transfer(c);
}

/*!
 * \page sk_endcall 		The End Call SoftKey
 * \section sk_endcall_howto 	How to use the End Call Softkey
 *
 * Using the End Call Button you can End the Call on the Current Line
 */
/*!
 * \brief End Call on Current Line
 * \n Usage: \ref sk_endcall
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_endcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Endcall with no call in progress\n", d->id);
		return;
	}
	sccp_channel_endcall(c);
}

/*!
 * \page sk_dnd 		The Do Not Disturb (DND) SoftKey
 * \section sk_dnd_howto 	How to use the Do Not Disturb (DND) Softkey
 *
 * Using the Do Not Disturb (DND) Button you set your device/line to the Do Not Disturb Status. 
 * When receiving a call your phone will not ring and the People Call You Will get a Busy Signal
 */
/*!
 * \brief Set DND on Current Line if Line is Active otherwise set on Device
 * \n Usage: \ref sk_dnd
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_dnd(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	/* actually the line param is not used */
	sccp_line_t * l1 = NULL;

	if (!d->dndmode) {
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_DND " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, 10);
		return;
	}

	if(d->dndmode == SCCP_DNDMODE_USERDEFINED){
		switch (d->dnd) {
			case SCCP_DNDMODE_OFF:
				d->dnd = SCCP_DNDMODE_REJECT;
				break;
			case SCCP_DNDMODE_REJECT:
				d->dnd = SCCP_DNDMODE_SILENT;
				break;
			case SCCP_DNDMODE_SILENT:
				d->dnd = SCCP_DNDMODE_OFF;
				break;
			default:
				d->dnd = (d->dnd) ? 0 : 1;
				break;
		}
	} else{
		d->dnd = (d->dnd) ? 0 : 1;
	}


	if ( (d->dndmode == SCCP_DNDMODE_USERDEFINED) || d->dndmode == SCCP_DNDMODE_REJECT || d->dnd == SCCP_DNDMODE_REJECT) {
		sccp_buttonconfig_t *buttonconfig;
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			if(buttonconfig->type == LINE ){
				l1 = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
				if(l1){
					sccp_log(10)(VERBOSE_PREFIX_3 "%s: Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dnd ? "on" : "off", l1->name);
					if (d->dnd){
		//				sccp_hint_notify_linestate(l1, d, SCCP_DEVICESTATE_ONHOOK, SCCP_DEVICESTATE_DND);
						sccp_hint_lineStatusChanged(l1, d, NULL, SCCP_DEVICESTATE_ONHOOK, SCCP_DEVICESTATE_DND);
					}else{
		//	 			sccp_hint_notify_linestate(l1, d, SCCP_DEVICESTATE_DND, SCCP_DEVICESTATE_ONHOOK);
						sccp_hint_lineStatusChanged(l1, d, NULL, SCCP_DEVICESTATE_DND, SCCP_DEVICESTATE_ONHOOK);
					}
				}
			}
		}
	}

	sccp_feat_changed(d, SCCP_FEATURE_DND);
	sccp_dev_dbput(d);
	sccp_dev_check_displayprompt(d);
}


/*!
 * \page sk_backspace 		The BackSpace SoftKey
 * \section sk_backspace_howto 	How to use the BackSpace (<<) Softkey
 *
 * Using the backspace (<<) Button you can erase the last entered digit/character
 */
/*!
 * \brief BackSpace Last Entered Number
 * \n Usage: \ref sk_backspace
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_backspace(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	int len;
	int instance;

	if (!c || !l)
		return;

	if ((c->state != SCCP_CHANNELSTATE_DIALING) &&
		(c->state != SCCP_CHANNELSTATE_DIGITSFOLL) &&
		(c->state != SCCP_CHANNELSTATE_OFFHOOK)	) {
		return;
	}

	sccp_channel_lock(c);

	len = strlen(c->dialedNumber);

	/* we have no number, so nothing to process */
	if (!len) {
		sccp_channel_unlock(c);
		return;
	}

	if (len > 1) {
		c->dialedNumber[len -1] = '\0';
		/* removing scheduled dial */
		SCCP_SCHED_DEL(sched, c->digittimeout);
		/* rescheduling dial timeout (one digit) */
		if(!(c->digittimeout = sccp_sched_add(sched, GLOB(digittimeout) * 1000, sccp_pbx_sched_dial, c))) {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_sk_backspace) Unable to reschedule dialing in '%d' s\n", GLOB(digittimeout));
		}
	} else if (len == 1) {
		c->dialedNumber[len -1] = '\0';
		/* removing scheduled dial */
		SCCP_SCHED_DEL(sched, c->digittimeout);
		/* rescheduling dial timeout (no digits) */
		if(!(c->digittimeout = sccp_sched_add(sched, GLOB(firstdigittimeout) * 1000, sccp_pbx_sched_dial, c))) {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_sk_backspace) Unable to reschedule dialing in '%d' s\n", GLOB(firstdigittimeout));
		}
	}
	// sccp_log(10)(VERBOSE_PREFIX_3 "%s: backspacing dial number %s\n", c->device->id, c->dialedNumber);
	sccp_handle_dialtone_nolock(c);
	sccp_channel_unlock(c);
	instance = sccp_device_find_index_for_line(c->device, c->line->name);
	sccp_handle_backspace(d, instance, c->callid);
}


/*!
 * \page sk_answer 		The Answer SoftKey
 * \section sk_Answer_howto 	How to use the Answer Softkey
 *
+ * Using the Answer Button you can Answer an Incoming Call
 */
/*!
 * \brief Answer Incoming Call
 * \n Usage: \ref sk_answer
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_answer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_channel_answer(d, c);
}


/*!
 * \page sk_DirTrfr 		The DirTrfr SoftKey
 * \section sk_DirTrfr_howto 	How to use the DirTrfr
 *
 * When two channels have been selected using the Select SoftKey (\ref sk_select) then
 * using the DirTrfr SoftKey can be used to Connect Both Lines to one another (bridging both channels).
 */
/*!
 * \brief Bridge two selected channels
 * \n Usage: \ref sk_DirTrfr
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_dirtrfr(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {

	sccp_selectedchannel_t *x;
	sccp_channel_t *chan1 = NULL, *chan2 = NULL, *tmp = NULL;
	uint8_t numSelectedChannels = 0;

	if(!d)
		return;

	if((numSelectedChannels = sccp_device_selectedchannels_count(d)) < 2) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: We need 2 channels to transfer\n", d->id);
		sccp_mutex_unlock(&d->lock);
		return;
	}

	SCCP_LIST_LOCK(&d->selectedChannels);
	x = SCCP_LIST_FIRST(&d->selectedChannels);
	chan1 = x->channel;
	chan2 = SCCP_LIST_NEXT(x, list)->channel;
	SCCP_LIST_UNLOCK(&d->selectedChannels);

	if(chan1 && chan2){
		//for using the sccp_channel_transfer_complete function
		//chan2 must be in RINGOUT or CONNECTED state
		if(chan2->state != SCCP_CHANNELSTATE_CONNECTED && chan1->state == SCCP_CHANNELSTATE_CONNECTED){
			tmp = chan1;
			chan1 = chan2;
			chan2 = tmp;
		} else if (chan1->state == SCCP_CHANNELSTATE_HOLD && chan2->state == SCCP_CHANNELSTATE_HOLD){
			//resume chan2 if both channels are on hold
			sccp_channel_resume(d, chan2);
		}
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) First Channel Status (%d), Second Channel Status (%d)\n", DEV_ID_LOG(d), chan1->state, chan2->state);
		sccp_device_lock(d);
		d->transfer_channel = chan1;

		sccp_device_unlock(d);
		sccp_channel_transfer_complete(chan2);

	}

}


/*!
 * \page sk_select 		The Select SoftKey
 * \section sk_select_howto 	How to use the select softkey
 *
 * The "Select" softkey is used for bridging two channels (redirect).
 * Select your first channel and press the select softkey. On the display this channel is marked
 * with a checkmark.
 * By selecting the second channel, it is also marked with a checkmark and the \ref sk_DirTrfr DirTrfr
 * is enabled. Press this key to bridge both channels.
 *
 **/
/*!
 * \brief Select a Line for further processing by for example DirTrfr
 * \n Usage: \ref sk_select
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_select(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_selectedchannel_t *x = NULL;
	sccp_moo_t * r1;
	uint8_t numSelectedChannels = 0, status = 0;
	int instance;

	if(!d) {
		sccp_log(10)(VERBOSE_PREFIX_3 "SCCP: (sccp_sk_select) Can't select a channel without a device\n");
		return;
	}
	if(!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: (sccp_sk_select) No channel to be selected\n", DEV_ID_LOG(d));
		return;
	}

	if((x = sccp_device_find_selectedchannel(d, c))) {
		SCCP_LIST_LOCK(&d->selectedChannels);
		SCCP_LIST_REMOVE(&d->selectedChannels, x, list);
		SCCP_LIST_UNLOCK(&d->selectedChannels);
		ast_free(x);
	} else {
		x = ast_malloc(sizeof(sccp_selectedchannel_t));
		x->channel = c;
		SCCP_LIST_LOCK(&d->selectedChannels);
		SCCP_LIST_INSERT_HEAD(&d->selectedChannels, x, list);
		SCCP_LIST_UNLOCK(&d->selectedChannels);
		status = 1;
	}

	numSelectedChannels = sccp_device_selectedchannels_count(d);

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: (sccp_sk_select) '%d' channels selected\n", DEV_ID_LOG(d), numSelectedChannels);

	instance = sccp_device_find_index_for_line(d, l->name);
	REQ(r1, CallSelectStatMessage);
	r1->msg.CallSelectStatMessage.lel_status = htolel(status);
	r1->msg.CallSelectStatMessage.lel_lineInstance = htolel(instance);
	r1->msg.CallSelectStatMessage.lel_callReference = htolel(c->callid);
	sccp_dev_send(d, r1);

	/*
	if(numSelectedChannels >= 2)
		sccp_sk_set_keystate(d, l, c, KEYMODE_CONNTRANS, 5, 1);
	else
		sccp_sk_set_keystate(d, l, c, KEYMODE_CONNTRANS, 5, 0);
	*/

}


/*!
 * \page sk_cfwdall 		The Call Forward All (CFwdAll) SoftKey
 * \section sk_cfwdall_howto 	How to use the Call Forward All Button
 *
 * Using this Button you can set the current line / device to forward all incoming call to another number
 * This number can be another SCCP/SIP/IAX Device or even an external number (if permitted to call externally)
 */
/*!
 * \brief Set Call Forward All on Current Line
 * \n Usage: \ref sk_cfwdall
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_cfwdall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l){
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_ALL);
	}else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);

}


/*!
 * \page sk_cfwdbusy 		The Call Forward if Busy (CFwdBusy) SoftKey
 * \section sk_cfwdbusy_howto 	How to use the Call Forward if Busy Button
 *
 * Using this Button you can set the current line / device to forward incoming calls when busy to another number
 * This number can be another SCCP/SIP/IAX Device or even an external number (if permitted to call externally)
 */
/*!
 * \brief Set Call Forward when Busy on Current Line
 * \n Usage: \ref sk_cfwdbusy
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_cfwdbusy(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_BUSY);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}


/*!
 * \page sk_cfwdnoanswer 		The Call Forward if No-Answer (CFwdBusy) SoftKey
 * \section sk_cfwdnoanswer_howto 	How to use the Call Forward if No-Answer Button
 *
 * Using this Button you can set the current line / device to forward incoming calls when you don't/can't answer to another number
 * This number can be another SCCP/SIP/IAX Device or even an external number (if permitted to call externally)
 */
/*!
 * \brief Set Call Forward when No Answer on Current Line
 * \n Usage: \ref sk_cfwdnoanswer 
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_cfwdnoanswer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_NOANSWER);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}


/*!
 * \page sk_park 		The Park SoftKey
 * \section sk_park_howto 	How to use the Park Button
 *
 * Using the Park Button make it possible to Send a Call to the Asterisk Parking Lot
 * Channels parked in the Parking Lot can be Picked-Up by Another Phone Using the Pickup Button or the Pickup Number 
 * Configured in features.conf under pickupexten
 */
/*!
 * \brief Park Call on Current Line
 * \n Usage: \ref sk_park
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_park(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_SCCP_PARK
	sccp_channel_park(c);
#else
	sccp_log(10)(VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
}


/*!
 * \page sk_transfer 		The Transfer SoftKey
 * \section sk_transfer_howto 	How to use the Transfer Button
 *
 * Using the Transfer Button gives you the possibily to Transfer the Current Line to a New Number
 * After pressing the Transfer button you are asked to Enter a New Number to which the Line will have to be transfered
 */
/*!
 * \brief Transfer to VoiceMail on Current Line
 * \n Usage: \ref sk_transfer
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_trnsfvm(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_feat_idivert(d, l, c);
}


/*!
 * \page sk_private 		The Private SoftKey
 * \section sk_private_howto 	How to use the Private Button
 *
 * Using the Private Button before dialing a New Outside Line will Prevent the Number dialed to be monitored by
 * Devices that use a HINT. It does not prevent the number turning up in Asterisk Logging or the Console Window.
 */
/*!
 * \brief Initiate Private Call on Current Line
 * \n Usage: \ref sk_private
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_private(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	uint8_t	instance;

	if (!d->privacyFeature.enabled) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: private function is not active on this device\n", d->id);
		return;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	sccp_mutex_lock(&c->lock);
	c->private = (c->private) ? FALSE : TRUE;
	if(c->private){
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_PRIVATE, 0);
	}else{
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 1);
	}

	/* force update on remote devices */
	__sccp_indicate_remote_device(d, c, c->state, 0,__FILE__, __LINE__, __PRETTY_FUNCTION__);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Private %s on call %d\n", d->id, c->private ? "enabled" : "disabled", c->callid);
	sccp_mutex_unlock(&c->lock);
}

/*!
 * \page sk_conference 		The Conference SoftKey
 * \section sk_conference_howto 	How to use the Conference Button
 *
 * Using the Conference Button makes it possible to set up a Simple Three Way Conference. There are two ways this can be done:
 * Option 1: You Already have to Lines Occupied (One is active and the other Held), which you would like to put in a Conference; Simple Press the Conference Button
 * Option 2: You have one line Occupied and would like to call Someone else and Join them all into a Conference;
 *           Press the Conference Button and you will be asked for the 3 party number which need to be added to the conference, 
 *           once this line is connected, you press the Conference Button Again and the Conference Starts.
 */
/*!
 * \brief Put Current Line into Conference
 * \n Usage: \ref sk_conference
 * \param d Device
 * \param line Line
 * \param c Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_sk_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_feat_conference(d, l, c);
}


/*!
 * \page sk_join 		The Join SoftKey
 * \section sk_join_howto 	How to use the Join Button
 *
 * Using the Join Button makes it possible to Add another member/line to an already running Conference
 * When you are in a conference and you receive an Incoming Call you can join this channel/line to the running Conference
 */
/*!
 * \brief Join Current Line to Conference
 * \n Usage: \ref sk_join
 * \param d Device
 * \param line Line
 * \param c Channel
 * \todo Conferencing option needs to be build and implemented
 */
void sccp_sk_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_feat_join(d, l, c);
}


/*!
 * \page sk_barge  		The Barge SoftKey
 * \section sk_barge_howto 	How to use the Barge Button
 *
 * Using the Barge Button makes it possible to listen in on an On-Going Call on another Line / Device
 */
/*!
 * \brief Barge into Call on the Current Line
 * \n Usage: \ref sk_barge
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_barge(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_barge(l,d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}

/*!
 * \page sk_cbarge 		The Cbarge SoftKey
 * \section sk_cbarge_howto 	How to use the Cbarge Button
 *
 * Using the Barge Button makes it possible to listen in on an On-Going Call on another Channel
 */
/*!
 * \brief Barge into Call on the Current Line
 * \n Usage: \ref sk_cbarge
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_cbarge(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_cbarge(l,d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}

/*!
 * \page sk_meetme 		The Meetme SoftKey
 * \section sk_meetme_howto 	How to use the Meetme Button
 *
 * Using the Conference Button makes it possible to set up a Simple Three Way Conference. There are two ways this can be done:
 * Option 1: You Already have to Lines Occupied (One is active and the other Held), which you would like to put in a Conference; Simple Press the Conference Button
 * Option 2: You have one line Occupied and would like to call Someone else and Join them all into a Conference;
 *           Press the Conference Button and you will be asked for the 3 party number which need to be added to the conference, 
 *           once this line is connected, you press the Conference Button Again and the Conference Starts.
 * \sa sk_conference
 */
/*!
 * \brief Put Current Line in to Meetme Conference
 * \n Usage: \ref sk_meetme
 * \param d Device
 * \param line Line
 * \param c Channel
 * \todo Conferencing option needs to be build and implemented 
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_sk_meetme(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_meetme(l, d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}



/*!
 * \page sk_pickup 		The Pickup SoftKey
 * \section sk_pickup_howto 	How to use the Pickup Button
 *
 * Using the Pickup Button Makes it possible to Pickup a Parked Call from the Parking Lot
 * This is equivalent to using the keys defined in features.conf under pickupexten
*/
/*!
 * \brief Pickup Parked Call
 * \n Usage: \ref sk_pickup
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_pickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifndef CS_SCCP_PICKUP
	sccp_log(10)(VERBOSE_PREFIX_3 "### Native EXTENSION PICKUP was not compiled in\n");
#else
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_directpickup(l, d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
#endif
}


/*!
 * \page sk_gpickup 		The GPickup SoftKey
 * \section sk_gpickup_howto 	How to use the Group Pickup Button
 *
 * Using the GroupPickup Button Makes it possible to Pickup a Ringing Line in defined in your PickupGroup
 * This is equivalent to using the keys defined in features.conf under pickupexten
 */
/*!
 * \brief Pickup Ringing Line from Pickup Group
 * \n Usage: \ref sk_gpickup
 * \param d Device
 * \param line Line
 * \param c Channel
 */
void sccp_sk_gpickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifndef CS_SCCP_PICKUP
	sccp_log(10)(VERBOSE_PREFIX_3 "### Native GROUP PICKUP was not compiled in\n");
#else
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_grouppickup(l,d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);

#endif
}


/**
 * \brief sets a SoftKey to a specified status (on/off)
 *
 * \param d device
 * \param l active line
 * \param c active channel
 * \param keymode int of KEYMODE_*
 * \param softkeyindex index of the SoftKey to set
 * \param status 0 for off otherwise on
 *
 *
 * \todo use SoftKeyLabel instead of softkeyindex
 *
 */
void sccp_sk_set_keystate(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c, unsigned int keymode, unsigned int softkeyindex, unsigned int status) {
	sccp_moo_t * r;
	uint32_t mask;
	int i, instance;


	if(!l || !c || !d || !d->session)
		return;

	instance = sccp_device_find_index_for_line(d, l->name);
	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance  = htolel(instance);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(c->callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(keymode);
	r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF; /* htolel(65535); */

	mask = 1;
	for(i=1;i<=softkeyindex;i++){
		mask = (mask<<1);
	}

	if(status==0)//disable softkey
		mask = ~(r->msg.SelectSoftKeysMessage.les_validKeyMask & mask);
	else
		mask = r->msg.SelectSoftKeysMessage.les_validKeyMask | mask;

	r->msg.SelectSoftKeysMessage.les_validKeyMask = mask;
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, skinny_softkeyset2str(5), 5, instance, c->callid);
	sccp_dev_send(d, r);

}
