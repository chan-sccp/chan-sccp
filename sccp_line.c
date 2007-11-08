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

#include "config.h"

#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_channel.h"
#include <asterisk/utils.h>

/* Kills a line's channels. */
/* Called with a lock on l->lock */
void sccp_line_kill(sccp_line_t * l) {
	sccp_channel_t * c;
	sccp_channel_t * t;
	
	if (!l)
		return;
	c = l->channels;
	while (c) {
		t = c->next;
		sccp_channel_endcall(c);
		c = t;
	}
}

void sccp_line_delete_nolock(sccp_line_t * l) {
	if (!l)
		return;

	/* remove from the global lines list */
	if (l->next) { /* not the last one */
		ast_mutex_lock(&l->next->lock);
		l->next->prev = l->prev;
		ast_mutex_unlock(&l->next->lock);
	}
	if (l->prev) { /* not the first one */
		ast_mutex_lock(&l->prev->lock);
		l->prev->next = l->next;
		ast_mutex_unlock(&l->prev->lock);
	} else { /* the first one */
		ast_mutex_lock(&GLOB(lines_lock));
		GLOB(lines) = l->next;
		ast_mutex_unlock(&GLOB(lines_lock));
	}

	/* remove the line from the device lines list */
	if (l->next_on_device) { /* not the last one */
		ast_mutex_lock(&l->next_on_device->lock);
		l->next_on_device->prev_on_device = l->prev_on_device;
		ast_mutex_unlock(&l->next_on_device->lock);
	}
	if (l->prev_on_device) { /* not the first one */
		ast_mutex_lock(&l->prev_on_device->lock);
		l->prev_on_device->next_on_device = l->next_on_device;
		ast_mutex_unlock(&l->prev_on_device->lock);
	} else { /* the first one */
		ast_mutex_lock(&l->lock);
		l->device->lines = l->next_on_device;
		ast_mutex_unlock(&l->lock);
	}
	if (l->cfwd_num)
			free(l->cfwd_num);
	if (l->trnsfvm)
			free(l->trnsfvm);

	ast_mutex_destroy(&l->lock);
	free(l);
}

void sccp_line_cfwd(sccp_line_t * l, uint8_t type, char * number) {
	sccp_device_t * d;
	if (!l)
		return;
	
	d = l->device;
	if (!d)
		return;
	
	if (l->cfwd_num) {
		free(l->cfwd_num);
		l->cfwd_num = NULL;
	}
	
	if (type == SCCP_CFWD_NONE) {
		l->cfwd_type = SCCP_CFWD_NONE;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward disabled on line %s\n", d->id, l->name);
	} else {
		if (!number || ast_strlen_zero(number)) {
			sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward to an empty number. Invalid\n", d->id);
			return;
		}
		l->cfwd_num = strdup(number);
		l->cfwd_type = type;
		sccp_log(1)(VERBOSE_PREFIX_3 "%s: Call Forward enabled on line %s to number %s\n", d->id, l->name, number);
	}
	sccp_dev_starttone(d, SKINNY_TONE_ZIPZIP, l->instance, 0, 0);
	sccp_dev_forward_status(l);
	sccp_dev_dbput(d);
}
