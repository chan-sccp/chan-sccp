/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */

/**
 * \page intro_sk SoftKeys
 * \ref sk_select The SELECT softkey <br />
 * \ref sk_DirTrfr The DirTrfr softkey
 * 
 * 
 */


#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_utils.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_indicate.h"
#include "sccp_protocol.h"
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/utils.h>
#ifdef CS_SCCP_PICKUP
#include <asterisk/features.h>
#include <asterisk/callerid.h>
#endif
#ifdef CS_AST_HAS_NEW_DEVICESTATE
#include <asterisk/devicestate.h>
#endif
#ifdef CS_AST_HAS_AST_STRING_FIELD
#include <asterisk/stringfields.h>
#endif

void sccp_sk_redial(sccp_device_t * d , sccp_line_t * l, sccp_channel_t * c) {
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Redial Softkey.\n",d->id);

	if (ast_strlen_zero(d->lastNumber)) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No number to redial\n", d->id);
		return;
	}

	if (c) {
		if (c->state == SCCP_CHANNELSTATE_OFFHOOK) {
			/* we have a offhook channel */
			ast_mutex_lock(&c->lock);
			sccp_copy_string(c->dialedNumber, d->lastNumber, sizeof(c->dialedNumber));
			sccp_log(10)(VERBOSE_PREFIX_3 "%s: Get ready to redial number %s\n", d->id, d->lastNumber);
			c->digittimeout = time(0)+1;
			ast_mutex_unlock(&c->lock);
		}
		/* here's a KEYMODE error. nothing to do */
		return;
	}
	if (!l)
		l = d->currentLine;
	sccp_channel_newcall(l, d->lastNumber);
}

void sccp_sk_newcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!l)
		l = d->currentLine;
	sccp_channel_newcall(l, NULL);
}

void sccp_sk_hold(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_dev_displayprompt(d, 0, 0, "No call to put on hold.",5);
		return;
	}
	sccp_channel_hold(c);
}

void sccp_sk_resume(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: No call to resume. Ignoring\n", d->id);
		return;
	}
	sccp_channel_resume(c);
}

void sccp_sk_transfer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Transfer when there is no active call\n", d->id);
		return;
	}
	sccp_channel_transfer(c);

}

void sccp_sk_endcall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Endcall with no call in progress\n", d->id);
		return;
	}
	sccp_channel_endcall(c);
}


void sccp_sk_dnd(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	/* actually the line param is not used */
	sccp_line_t * l1 = NULL;
	

	if(l){
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: We have a line \"%s\", set DND to line\n", d->id, l->name);
		if (!l->dndmode) {
			sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_DND " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, 10);
			return;
		}
	
		ast_mutex_lock(&l->lock);
		if(l->dndmode == SCCP_DNDMODE_USERDEFINED){
			switch (l->dnd) {
				case SCCP_DNDMODE_OFF:
					l->dnd = SCCP_DNDMODE_REJECT;
					break;
				case SCCP_DNDMODE_REJECT:
					l->dnd = SCCP_DNDMODE_SILENT;
					break;
				case SCCP_DNDMODE_SILENT:
					l->dnd = SCCP_DNDMODE_OFF;
					break;
				default:
					l->dnd = (l->dnd) ? 0 : 1;
					break;
			}
		} else{
			l->dnd = (l->dnd) ? 0 : 1;
		}
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: DND-line status: %d\n", d->id, l->dnd);
		ast_mutex_unlock(&l->lock);
	}else{

		if (!d->dndmode) {
			sccp_dev_displayprompt(d, 0, 0, SKINNY_DISP_DND " " SKINNY_DISP_SERVICE_IS_NOT_ACTIVE, 10);
			return;
		}
	
		ast_mutex_lock(&d->lock);
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

	
		if (d->dndmode == SCCP_DNDMODE_REJECT || d->dnd == SCCP_DNDMODE_REJECT) {
			l1 = d->lines;
			while (l1) {
				sccp_log(10)(VERBOSE_PREFIX_3 "%s: Notify the dnd status (%s) to asterisk for line %s\n", d->id, d->dnd ? "on" : "off", l1->name);
				if (d->dnd)
 					sccp_hint_notify_linestate(l1, SCCP_DEVICESTATE_DND, NULL);
	 			else
		 			sccp_hint_notify_linestate(l1, SCCP_DEVICESTATE_ONHOOK, NULL);
				l1 = l1->next_on_device;
			}
		}
		ast_mutex_unlock(&d->lock);
		sccp_dev_dbput(d);
	}
	sccp_dev_check_displayprompt(d);
}


void sccp_sk_backspace(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	int len;
	if (!c)
		return;
	if (c->state != SCCP_CHANNELSTATE_DIALING)
		return;
	ast_mutex_lock(&c->lock);
	len = strlen(c->dialedNumber)-1;
	if (len >= 0)
		c->dialedNumber[len] = '\0';
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: backspacing dial number %s\n", c->device->id, c->dialedNumber);
	ast_mutex_unlock(&c->lock);
}

void sccp_sk_answer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_channel_answer(c);
}


/**
 * bridge two selected channels 
 * \page sk_DirTrfr DirTrfr
 * \section sk_DirTrfr_howto How to use the DirTrfr
 * 
 */
void sccp_sk_dirtrfr(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	struct sccp_selected_channel *s;
	sccp_channel_t *chan1 = NULL, *chan2 = NULL, *tmp = NULL;
	uint8_t numSelectedChannels =0;
	
	if(!d)
		return;
	ast_mutex_lock(&d->lock);
	s = d->selectedChannels;
	if(!s)
		return;
	
	chan1 = s->c;
	while(s){
		numSelectedChannels++;
		if(2 == numSelectedChannels)
			chan2= s->c;
		s=s->next;
	}
	
	if(chan1 && chan2){
		//for using the sccp_channel_transfer_complete function
		//chan2 must be in RINGOUT or CONNECTED state
		if(chan2->state != SCCP_CHANNELSTATE_CONNECTED && chan1->state == SCCP_CHANNELSTATE_CONNECTED){
			tmp = chan1;
			chan1 = chan2;
			chan2 = tmp;
		} else if (chan1->state == SCCP_CHANNELSTATE_HOLD && chan2->state == SCCP_CHANNELSTATE_HOLD){
			//resume chan2 if both channels are on hold
			ast_mutex_unlock(&d->lock);
			sccp_channel_resume(chan2);
			ast_mutex_lock(&d->lock);
		}
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: State of chan1 is: %d\n", d->id, chan1->state);
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: State of chan2 is: %d\n", d->id, chan2->state);
		d->transfer_channel = chan1;

		ast_mutex_unlock(&d->lock);
		sccp_channel_transfer_complete(chan2);
	}else{
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: We need 2 channels to transfer\n", d->id);
		ast_mutex_unlock(&d->lock);
	}
}


/**
 * 
 * \page sk_select select
 * \section sk_select_howto How to use the select softkey
 * 
 * The "Select" softkey is used for bridging tow channels (redirect).
 * Select your first channel and press the select softkey. On the display this channel is marked
 * with a checkmark.
 * By selecting the second channel, it is also marked with a checkmark and the \ref sk_DirTrfr DirTrfr 
 * is enabled. Press this key to bridge both channels.  
 * 
 * 
 * 
 * 
 * 
 * 
 */
void sccp_sk_select(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	struct sccp_selected_channel *cur = NULL, *par = NULL;
	uint8_t numSelectedChannels =0;
	sccp_moo_t 			* r1;
  
	if(NULL == d)
	{
		sccp_log(10)(VERBOSE_PREFIX_3 "Null device!");
		return;
	}
  
	if(NULL == c)
	{
		sccp_log(10)(VERBOSE_PREFIX_3 "Null channel!");
		return;
	}
	int status = 0;
  
	ast_mutex_lock(&d->lock);
 
	cur = d->selectedChannels;
	while(NULL != cur) {
		if(c == cur->c) {
			status = 0;
			if(NULL == par) {
				d->selectedChannels = cur->next;
				free(cur);
			}
		else {
			par->next = cur->next;
			free(cur);
		}
			break;
		} else {
			par = cur;
			cur = cur->next;
		}
	}
  
	if(NULL == cur)
	{
		par = malloc(sizeof(struct sccp_selected_channel));
		par->c = c;
		par->next = d->selectedChannels;
		d->selectedChannels = par;
		status = 1;
	}
  
	//count number of selected channels
	cur = d->selectedChannels;
	while(cur){
		numSelectedChannels++;
		cur=cur->next;
	}
	if(numSelectedChannels == 2)
		sccp_sk_set_keystate(d,l, c, KEYMODE_CONNTRANS, 5, 1);
	else
		sccp_sk_set_keystate(d,l, c, KEYMODE_CONNTRANS, 5, 0);
  
	REQ(r1, CallSelectStatMessage);
	r1->msg.CallSelectStatMessage.lel_status = htolel(status);
	r1->msg.CallSelectStatMessage.lel_lineInstance = htolel(l->instance);
	r1->msg.CallSelectStatMessage.lel_callReference = htolel(c->callid);
	sccp_dev_send(d, r1);
  
	ast_mutex_unlock(&d->lock);
}

void sccp_sk_cfwdall(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c || !c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Call forward with no channel active\n", d->id);
		return;
	}
	if (c->state != SCCP_CHANNELSTATE_RINGOUT && c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_line_cfwd(l, SCCP_CFWD_NONE, NULL);
		return;
	}
	sccp_line_cfwd(l, SCCP_CFWD_ALL, c->dialedNumber);
}

void sccp_sk_cfwdbusy(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!c || !c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: Call forward with no channel active\n", d->id);
		return;
	}
	if (c->state != SCCP_CHANNELSTATE_RINGOUT && c->state != SCCP_CHANNELSTATE_CONNECTED) {
		sccp_line_cfwd(l, SCCP_CFWD_NONE, NULL);
		return;
	}
	sccp_line_cfwd(l, SCCP_CFWD_BUSY, c->dialedNumber);

}

void sccp_sk_cfwdnoanswer(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	sccp_log(10)(VERBOSE_PREFIX_3 "### CFwdNoAnswer Softkey pressed - NOT SUPPORTED\n");
}

void sccp_sk_park(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifdef CS_SCCP_PARK
	sccp_channel_park(c);
#else
	sccp_log(10)(VERBOSE_PREFIX_3 "### Native park was not compiled in\n");
#endif
}

void sccp_sk_trnsfvm(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!l->trnsfvm) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM pressed but not configured in sccp.conf\n", d->id);
		return;
	}
	if (!c || !c->owner) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM with no channel active\n", d->id);
		return;
	}
	if (c->state != SCCP_CHANNELSTATE_RINGIN && c->state != SCCP_CHANNELSTATE_CALLWAITING) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: TRANSVM pressed in no ringing state\n", d->id);
		return;
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: TRANSVM to %s\n", d->id, l->trnsfvm);
#ifdef CS_AST_HAS_AST_STRING_FIELD
		ast_string_field_set(c->owner, call_forward, l->trnsfvm);
#else
		sccp_copy_string(c->owner->call_forward, l->trnsfvm, sizeof(c->owner->call_forward));
#endif
	ast_setstate(c->owner, AST_STATE_BUSY);
	ast_queue_control(c->owner, AST_CONTROL_BUSY);
}

void sccp_sk_private(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
	if (!d->private) {
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: private function is not active on this device\n", d->id);
		return;
	}
	ast_mutex_lock(&c->lock);
	c->private = (c->private) ? 0 : 1;
	if(c->private){
		sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_PRIVATE, 0);
	}else{
		sccp_dev_displayprompt(d, c->line->instance, c->callid, SKINNY_DISP_ENTER_NUMBER, 1);
	}

	sccp_log(1)(VERBOSE_PREFIX_3 "%s: Private %s on call %d\n", d->id, c->private ? "enabled" : "disabled", c->callid);
	ast_mutex_unlock(&c->lock);
}

void sccp_sk_gpickup(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
#ifndef CS_SCCP_PICKUP
	sccp_log(10)(VERBOSE_PREFIX_3 "### Native PICKUP was not compiled in\n");
#else
	uint8_t res = 0;
	struct ast_channel *ast, *original = NULL;
#ifndef CS_AST_CHANNEL_HAS_CID
	char * name, * number, *cidtmp; // For the callerid parse below
#endif

	if (!l)
		l = d->currentLine;
	if (!l)
		l = d->lines;
	if (!l)
		return;

	if (!l->pickupgroup) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: pickupgroup not configured in sccp.conf\n", d->id);
		return;
	}
	c = sccp_channel_find_bystate_on_line(l, SCCP_CHANNELSTATE_OFFHOOK);
	if (!c)
		c = sccp_channel_newcall(l, NULL);
	if (!c) {
			ast_log(LOG_ERROR, "%s: Can't allocate SCCP channel for line %s\n",d->id, l->name);
			return;
	}

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Starting the PICKUP stuff\n", d->id);
	/* convert the outgoing call in a incoming call */
	/* let's the startchannel thread go down */
	ast = c->owner;
	if (!ast) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: error: no channel to handle\n", d->id);
		/* let the channel goes down to the invalid number */
		return;
	}

	if (ast_pickup_call(ast)) {
		sccp_log(10)(VERBOSE_PREFIX_3 "%s: pickup error\n", d->id);
		/* let the channel goes down to the invalid number */
		return;
	}

	while (ast_mutex_trylock(&ast->lock)) {
		ast_log(LOG_DEBUG, "SCCP: Waiting to lock the channel for pickup\n");
		usleep(1000);
		ast = c->owner;
	}
	
	original = ast->masqr;

	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Pickup the call from %s\n", d->id, original->name);

	res = (int) (ast->blocker);
	
	ast_mutex_unlock(&ast->lock);

	if (res)
		ast_queue_hangup(ast);
	else
		ast_hangup(ast);

	if (original) {
#ifdef CS_AST_CHANNEL_HAS_CID
		sccp_channel_set_callingparty(c, original->cid.cid_name, original->cid.cid_num);
#else
		if (original->callerid && (cidtmp = strdup(original->callerid))) {
			ast_callerid_parse(cidtmp, &name, &number);
			sccp_channel_set_callingparty(c, name, number);
			free(cidtmp);
			cidtmp = NULL;
		}
#endif
	}
	ast_mutex_lock(&c->lock);
	c->calltype = SKINNY_CALLTYPE_INBOUND;
	sccp_indicate_nolock(c, SCCP_CHANNELSTATE_CONNECTED);
	ast_mutex_unlock(&c->lock);
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
	int i;
	
	
	if(!l || !c || !d || !d->session)
		return;

	
	REQ(r, SelectSoftKeysMessage);
	r->msg.SelectSoftKeysMessage.lel_lineInstance  = htolel(l->instance);
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
	sccp_log(10)(VERBOSE_PREFIX_3 "%s: Send softkeyset to %s(%d) on line %d  and call %d\n", d->id, skinny_softkeyset2str(5), 5, l->instance, c->callid);
	sccp_dev_send(d, r);

}
