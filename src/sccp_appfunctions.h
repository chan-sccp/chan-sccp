
/*!
 * \file 	sccp_appfunctions.h
 * \brief 	SCCP Application / Dialplan Function Headers
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date: 2010-09-16 14:23:34 +0200 (Do, 16. Sep 2010) $
 * $Revision: 1867 $
 */

#ifndef __SCCP_APPFUNCTIONS_H
#    define __SCCP_APPFUNCTIONS_H

int sccp_register_dialplan_functions(void);
int sccp_unregister_dialplan_functions(void);

#endif
