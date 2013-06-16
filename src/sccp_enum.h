/*!
 * \file        sccp_enum.h
 * \brief       SCCP Enum Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-04-15 01:04:06 +0200 (Mon, 15 Apr 2013) $
 * $Revision: 4568 $
 */
#ifndef __SCCP_ENUM_H
#define __SCCP_ENUM_H

/* enum macro definitions */
    #define ENUM_ELEMENT( element, index, str) element index,
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) typedef enum tag_##ENUM_NAME {
    #define END_ENUM( NAMESPACE, ENUM_NAME ) } NAMESPACE##_##ENUM_NAME##_t; 								\
        inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index);
/* end enum macro definitions */

#include "sccp_enum_entries.hh"

#undef ENUM_ELEMENT
#undef BEGIN_ENUM
#undef END_ENUM

// TEMP / Compatibility between source changes
#define channelstate2str channelState2str 
#define devicestatus2str devicestate2str
#define sccp_lampMode_t skinny_lampMode_t
#define lampmode2str lampMode2str
#define sccp_BLFState_t skinny_busylampfield_state_t
#define deviceregistrationstatus2str registrationstate2str
#define sccp_autoanswer_type_t sccp_autoanswer_t
#define accessorystatus2str accessorystate2str
#define keymode2description keymode2str
#endif // __SCCP_ENUM_H
