/*!
 * \file 	sccp_adv_features.c
 * \brief 	SCCP Advanced Features Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \author	Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$
 */
#include "config.h"

#if ASTERISK_VERSION_NUM >= 10400
#include <asterisk.h>
#endif

#include "chan_sccp.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_features.h"
#include "sccp_lock.h"
#include "sccp_actions.h"
#include "sccp_utils.h"
#include "sccp_device.h"
#include "sccp_pbx.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_indicate.h"
#include <asterisk/pbx.h>
#include <asterisk/utils.h>
#include <asterisk/causes.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/channel_pvt.h>
#endif
#include <asterisk/callerid.h>
#include <asterisk/musiconhold.h>
#ifdef CS_SCCP_PARK
#include <asterisk/features.h>
#endif
 
#include "sccp_adv_features.h"

/*!
 * \brief Dummy Procedure 1
 */
void dummyproc1(void)
{
        /* this is a dummy proc */
        if (0)
                dummyproc2();
}

/*!
 * \brief Dummy Procedure 2
 */
void dummyproc2(void)
{
        /* this is a dummy proc */
        if (0)
                dummyproc1();
}
 
void sccp_advfeat_conference(sccp_device_t *d, sccp_line_t *l, sccp_channel_t *c) {

	if (!d || !d->session)
		return;

	sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_ADV_FEATURE))(VERBOSE_PREFIX_3 "%s: handling conference\n", d->id);
	c = sccp_channel_get_active(d);
	if (c) {
                sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_ADV_FEATURE | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: existing call %d\n", d->id, c->callid);
		sccp_channel_lock(c);
		if ( (c->state == SCCP_CHANNELSTATE_DIALING) || (c->state == SCCP_CHANNELSTATE_OFFHOOK) ) {
                        sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_ADV_FEATURE | DEBUGCAT_CHANNEL))(VERBOSE_PREFIX_3 "%s: calling '**' for %d\n", d->id, c->callid);
			sccp_copy_string(c->dialedNumber, "**", sizeof(c->dialedNumber));
			sccp_channel_unlock(c);

			SCCP_SCHED_DEL(sched, c->digittimeout);
                        sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_ADV_FEATURE))(VERBOSE_PREFIX_3 "%s: redirecting %d\n to '**'", d->id, c->callid);
                        sccp_pbx_softswitch(c);
			return;
		}
		sccp_channel_unlock(c);
		sccp_pbx_senddigits(c, "**");
	} else {
                sccp_log((DEBUGCAT_CONFERENCE | DEBUGCAT_ADV_FEATURE))(VERBOSE_PREFIX_3 "%s: new call\n", d->id);
		if (GLOB(hotline)->line) {
			sccp_channel_newcall(l, d, "**", SKINNY_CALLTYPE_OUTBOUND);
		}
	}
}

void sccp_advfeat_join(sccp_device_t * d, sccp_line_t * l, sccp_channel_t * c) {
}

int sccp_advfeat_barge(sccp_channel_t * c, char * exten) {
  return 1;
}

int sccp_advfeat_cbarge(sccp_channel_t * c, char * conferencenum) {
  return 1;
}
