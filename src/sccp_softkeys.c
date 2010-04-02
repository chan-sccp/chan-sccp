/*!
 * \file 	sccp_softkeys.c
 * \brief 	SCCP SoftKeys Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \date        $Date$
 * \version     $Revision$  
 */

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

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
 * \brief Global list of softkeys
 */
//SCCP_LIST_HEAD(softKeySetConfigList, sccp_softKeySetConfiguration_t) softKeySetConfig;	/*!< List of SoftKeySets */
struct softKeySetConfigList softKeySetConfig;							/*!< List of SoftKeySets */
 
/*!
 * \brief Redial last Dialed Number by this Device
 * \n Usage: \ref sk_redial
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_redial(sccp_device_t * d , sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Redial Pressed\n", DEV_ID_LOG(d));

	if (ast_strlen_zero(d->lastNumber)) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: No number to redial\n", d->id);
		return;
	}

	if (c) {
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
			/* we have a offhook channel */
			sccp_channel_lock(c);
			sccp_copy_string(c->dialedNumber, d->lastNumber, sizeof(c->dialedNumber));
			sccp_channel_unlock(c);
			sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: Get ready to redial number %s\n", d->id, d->lastNumber);
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
 * \brief Initiate a New Call
 * \n Usage: \ref sk_newcall
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_newcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey NewCall Pressed\n", DEV_ID_LOG(d));
	if (!l){
		/* use default line if it is set */
		if(d && d->defaultLineInstance >0){
			sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY | SCCP_VERBOSE_LEVEL_LINE))(VERBOSE_PREFIX_3 "using default line with instance: %u", d->defaultLineInstance);
			l = sccp_line_find_byid(d, d->defaultLineInstance);
		}
	}
	if(!l)
		l = d->currentLine;
	
	if(strlen(l->adhocNumber)>0)
		sccp_channel_newcall(l, d, l->adhocNumber, SKINNY_CALLTYPE_OUTBOUND);
	else
		sccp_channel_newcall(l, d, NULL, SKINNY_CALLTYPE_OUTBOUND);
}


/*!
 * \brief Hold Call on Current Line
 * \n Usage: \ref sk_hold
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_hold(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Hold Pressed\n", DEV_ID_LOG(d));
	if (!c) {
		sccp_dev_displayprompt(d, 0, 0, "No call to put on hold.",5);
		return;
	}
	sccp_channel_hold(c);
}

/*!
 * \brief Resume Call on Current Line
 * \n Usage: \ref sk_resume
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_resume(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Resume Pressed\n", DEV_ID_LOG(d));
	if (!c) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: No call to resume. Ignoring\n", d->id);
		return;
	}
	sccp_channel_resume(d, c);
}

/*!
 * \brief Transfer Call on Current Line
 * \n Usage: \ref sk_transfer
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_transfer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Transfer Pressed\n", DEV_ID_LOG(d));
	if (!c) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: Transfer when there is no active call\n", d->id);
		return;
	}
	sccp_channel_transfer(c);
}

/*!
 * \brief End Call on Current Line
 * \n Usage: \ref sk_endcall
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_endcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey EndCall Pressed\n", DEV_ID_LOG(d));
	if (!c) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: Endcall with no call in progress\n", d->id);
		return;
	}
	sccp_channel_endcall(c);
}

/*!
 * \brief Set DND on Current Line if Line is Active otherwise set on Device
 * \n Usage: \ref sk_dnd
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_dnd(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey DND Pressed\n", DEV_ID_LOG(d));
	/* actually the line param is not used */
	sccp_line_t * l1 = NULL;

	if (!d->dndFeature.enabled) {
		sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_DND " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, 10);
		return;
	}

	switch (d->dndFeature.status) {
		case SCCP_DNDMODE_OFF:
			d->dndFeature.status = SCCP_DNDMODE_REJECT;
			break;
		case SCCP_DNDMODE_REJECT:
			d->dndFeature.status = SCCP_DNDMODE_SILENT;
			break;
		case SCCP_DNDMODE_SILENT:
			d->dndFeature.status = SCCP_DNDMODE_OFF;
			break;
		default:
			d->dndFeature.status = SCCP_DNDMODE_OFF;
			break;
	}



	if ( d->dndFeature.status == SCCP_DNDMODE_REJECT || d->dndFeature.status == SCCP_DNDMODE_OFF) {
		sccp_buttonconfig_t *buttonconfig;
		SCCP_LIST_TRAVERSE(&d->buttonconfig, buttonconfig, list) {
			if(buttonconfig->type == LINE ){
				l1 = sccp_line_find_byname_wo(buttonconfig->button.line.name,FALSE);
				if(l1){
					sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dndFeature.status ? "on" : "off", l1->name);
					if (d->dndFeature.status == SCCP_DNDMODE_REJECT){
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
	sccp_dev_check_displayprompt(d);
}


/*!
 * \brief BackSpace Last Entered Number
 * \n Usage: \ref sk_backspace
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_backspace(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Backspace Pressed\n", DEV_ID_LOG(d));
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
		if( (c->digittimeout = sccp_sched_add(sched, GLOB(digittimeout) * 1000, sccp_pbx_sched_dial, c)) < 0) {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_sk_backspace) Unable to reschedule dialing in '%d' s\n", GLOB(digittimeout));
		}
	} else if (len == 1) {
		c->dialedNumber[len -1] = '\0';
		/* removing scheduled dial */
		SCCP_SCHED_DEL(sched, c->digittimeout);
		/* rescheduling dial timeout (no digits) */
		if( (c->digittimeout = sccp_sched_add(sched, GLOB(firstdigittimeout) * 1000, sccp_pbx_sched_dial, c)) < 0) {
			sccp_log(1)(VERBOSE_PREFIX_1 "SCCP: (sccp_sk_backspace) Unable to reschedule dialing in '%d' s\n", GLOB(firstdigittimeout));
		}
	}
	// sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: backspacing dial number %s\n", c->device->id, c->dialedNumber);
	sccp_handle_dialtone_nolock(c);
	sccp_channel_unlock(c);

	sccp_device_lock(c->device);
	instance = sccp_device_find_index_for_line(c->device, c->line->name);
	sccp_device_unlock(c->device);

	sccp_handle_backspace(d, instance, c->callid);
}


/*!
 * \brief Answer Incoming Call
 * \n Usage: \ref sk_answer
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_answer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Answer Pressed\n", DEV_ID_LOG(d));
	sccp_channel_answer(d, c);
}


/*!
 * \brief Bridge two selected channels
 * \n Usage: \ref sk_dirtrfr
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_dirtrfr(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Direct Transfer Pressed\n", DEV_ID_LOG(d));

	sccp_selectedchannel_t *x;
	sccp_channel_t *chan1 = NULL, *chan2 = NULL, *tmp = NULL;
	uint8_t numSelectedChannels = 0;

	if(!d)
		return;

	if((numSelectedChannels = sccp_device_selectedchannels_count(d)) != 2) {
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
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: (sccp_sk_dirtrfr) First Channel Status (%d), Second Channel Status (%d)\n", DEV_ID_LOG(d), chan1->state, chan2->state);
		sccp_device_lock(d);
		d->transfer_channel = chan1;

		sccp_device_unlock(d);
		sccp_channel_transfer_complete(chan2);

	}

}


/*!
 * \brief Select a Line for further processing by for example DirTrfr
 * \n Usage: \ref sk_select
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_select(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Select Pressed\n", DEV_ID_LOG(d));
	sccp_selectedchannel_t *x = NULL;
	sccp_moo_t * r1;
	uint8_t numSelectedChannels = 0, status = 0;
	int instance;

	if(!d) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "SCCP: (sccp_sk_select) Can't select a channel without a device\n");
		return;
	}
	if(!c) {
		sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: (sccp_sk_select) No channel to be selected\n", DEV_ID_LOG(d));
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

	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: (sccp_sk_select) '%d' channels selected\n", DEV_ID_LOG(d), numSelectedChannels);

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
 * \brief Set Call Forward All on Current Line
 * \n Usage: \ref sk_cfwdall
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_cfwdall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Call Forward All Pressed\n", DEV_ID_LOG(d));
        if (!l && d) {
                l = sccp_line_find_byid(d, 1);
        }
	if(l){
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_ALL);
	}else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);

}

/*!
 * \brief Set Call Forward when Busy on Current Line
 * \n Usage: \ref sk_cfwdbusy
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_cfwdbusy(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Call Forward Busy Pressed\n", DEV_ID_LOG(d));
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_BUSY);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);

}

/*!
 * \brief Set Call Forward when No Answer on Current Line
 * \n Usage: \ref sk_cfwdnoanswer
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_cfwdnoanswer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Call Forward NoAnswer Pressed\n", DEV_ID_LOG(d));
	if(!l && d) {
		l = sccp_line_find_byid(d, 1);
	}
	if(l)
		sccp_feat_handle_callforward(l, d, SCCP_CFWD_NOANSWER);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);

	/*
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "### CFwdNoAnswer Softkey pressed - NOT SUPPORTED\n");
	*/
}

/*!
 * \brief Park Call on Current Line
 * \n Usage: \ref sk_park
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_park(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Park Pressed\n", DEV_ID_LOG(d));
#ifdef CS_SCCP_PARK
	sccp_channel_park(c);
#else
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
}

/*!
 * \brief Transfer to VoiceMail on Current Line
 * \n Usage: \ref sk_transfer
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_trnsfvm(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Transfer Voicemail Pressed\n", DEV_ID_LOG(d));
	sccp_feat_idivert(d, l, c);
}

/*!
 * \brief Initiate Private Call on Current Line
 * \n Usage: \ref sk_private
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_private(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Private Pressed\n", DEV_ID_LOG(d));
	uint8_t	instance;

	if (!d->privacyFeature.enabled) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: private function is not active on this device\n", d->id);
		return;
	}

	instance = sccp_device_find_index_for_line(d, l->name);
	sccp_mutex_lock(&c->lock);
	c->privacy = (c->privacy) ? FALSE : TRUE;
	if(c->privacy){
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_PRIVATE, 0);
	}else{
		sccp_dev_displayprompt(d, instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 1);
	}

	/* force update on remote devices */
	__sccp_indicate_remote_device(d, c, c->state, 0,__FILE__, __LINE__, __PRETTY_FUNCTION__);

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Private %s on call %d\n", d->id, c->privacy ? "enabled" : "disabled", c->callid);
	sccp_mutex_unlock(&c->lock);
}

/*!
 * \brief Put Current Line into Conference
 * \n Usage: \ref sk_conference
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_sk_conference(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Conference Pressed\n", DEV_ID_LOG(d));
	sccp_feat_conference(d, l, c);
}


/*!
 * \brief Join Current Line to Conference
 * \n Usage: \ref sk_join
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 */
void sccp_sk_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Join Pressed\n", DEV_ID_LOG(d));
	sccp_feat_join(d, l, c);
}


/*!
 * \brief Barge into Call on the Current Line
 * \n Usage: \ref sk_barge
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_barge(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Barge Pressed\n", DEV_ID_LOG(d));
        if (!l && d) {
                l = sccp_line_find_byid(d, 1);
        }
	if(l)
		sccp_feat_handle_barge(l,d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}

/*!
 * \brief Barge into Call on the Current Line
 * \n Usage: \ref sk_cbarge
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_cbarge(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey cBarge Pressed\n", DEV_ID_LOG(d));
        if (!l && d) {
                l = sccp_line_find_byid(d, 1);
        }
	if(l)
		sccp_feat_handle_cbarge(l,d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}

/*!
 * \brief Put Current Line in to Meetme Conference
 * \n Usage: \ref sk_meetme
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 * \todo Conferencing option needs to be build and implemented
 *       Using and External Conference Application Instead of Meetme makes it possible to use app_Conference, app_MeetMe, app_Konference and/or others
 */
void sccp_sk_meetme(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Meetme Pressed\n", DEV_ID_LOG(d));
        if (!l && d) {
                l = sccp_line_find_byid(d, 1);
        }
	if(l)
		sccp_feat_handle_meetme(l, d);
	else
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: No line (%d) found\n", d->id, 1);
}



/*!
 * \brief Pickup Parked Call
 * \n Usage: \ref sk_pickup
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_pickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Pickup Pressed\n", DEV_ID_LOG(d));
#ifndef CS_SCCP_PICKUP
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "### Native EXTENSION PICKUP was not compiled in\n");
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
 * \brief Pickup Ringing Line from Pickup Group
 * \n Usage: \ref sk_gpickup
 * \param d SCCP Device
 * \param l SCCP Line
 * \param c SCCP Channel
 */
void sccp_sk_gpickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c)
{
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: SoftKey Group Pickup Pressed\n", DEV_ID_LOG(d));
#ifndef CS_SCCP_PICKUP
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "### Native GROUP PICKUP was not compiled in\n");
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


/*!
 * \brief sets a SoftKey to a specified status (on/off)
 *
 * \param d SCCP Device
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
	uint32_t mask, validKeyMask;
	int i, instance;


	if(!l || !c || !d || !d->session)
		return;

	instance = sccp_device_find_index_for_line(d, l->name);
	
	
	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance  = htolel(instance);
	r->msg.SelectSoftKeysMessage.lel_callReference = htolel(c->callid);
	r->msg.SelectSoftKeysMessage.lel_softKeySetIndex = htolel(keymode);
	//r->msg.SelectSoftKeysMessage.les_validKeyMask = 0xFFFFFFFF; /* htolel(65535); */
	validKeyMask = 0xFFFFFFFF;

	mask = 1;
	for(i=1;i<=softkeyindex;i++){
		mask = (mask<<1);
	}

	if(status==0)//disable softkey
		mask = ~(validKeyMask & mask);
	else
		mask = validKeyMask | mask;

	r->msg.SelectSoftKeysMessage.les_validKeyMask = htolel(mask);
	sccp_log((SCCP_VERBOSE_LEVEL_SOFTKEY))(VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, keymode2str(5), 5, instance, c->callid);
	sccp_dev_send(d, r);

}
