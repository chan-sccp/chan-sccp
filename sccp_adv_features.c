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
 * Features Code by Federico Santulli 
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 *
 * Part of this features code rests private due to free effort of implementing
 *
 */
 
 #include "config.h"
 
 #include "sccp_adv_features.h"

 void dummyproc1(void) {
	/* this is a dummy proc */
	if(0)
		dummyproc2();
 }
 
 void dummyproc2(void) {
	/* this is a dummy proc */
	if(0)
		dummyproc1();	
 }
 
