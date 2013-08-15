/*!
 * \file        sccp_devstate.h
 * \brief       SCCP device state Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2013-08-15
 *
 * $Date$
 * $Revision$  
 */

#ifndef SCCP_DEVSTATE_H_
#define SCCP_DEVSTATE_H_

#include <config.h>
#include "common.h"


BEGIN_ENUM(sccp,devstate_state)
        ENUM_ELEMENT(IDLE				,=0 << 0,	"IDLE")
        ENUM_ELEMENT(INUSE				,=1 << 1,	"INUSE")
END_ENUM(sccp,devstate_state)


void sccp_devstate_module_start(void);
void sccp_devstate_module_stop(void);

#endif							/* SCCP_DEVSTATE_H_ */ 
