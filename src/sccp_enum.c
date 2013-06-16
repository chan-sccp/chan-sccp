
/*!
 * \file        sccp_enum.c
 * \brief       SCCP Enum Class
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note                Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note                This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-05-09 14:06:42 +0200 (Thu, 09 May 2013) $
 * $Revision: 4618 $
 */

#include <config.h>
#include "sccp_enum.h"

/* enum macro definitions */
#  ifdef DEBUG														// switch/case on index (Debug mode)
    #define ENUM_ELEMENT( element, index, str) case element: return str;
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) 											\
        inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index) { 						\
                switch(index) {												
    #define END_ENUM( NAMESPACE, ENUM_NAME ) 	};										\
     		return "SCCP: ERROR lookup in " #NAMESPACE "_" #ENUM_NAME "_t";								\
        };
#  else 														// return array[index] directly (Non-Debug Mode)
    #define ENUM_ELEMENT( element, index, str ) [element] = str,
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) char* NAMESPACE ##_##ENUM_NAME##_map_enum2str[] = {
    #define END_ENUM( NAMESPACE, ENUM_NAME ) }; 											\
        inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index) { return NAMESPACE##_##ENUM_NAME##_map_enum2str[index]; }
#  endif
/* end enum macro definitions */

#include "sccp_enum_entries.hh"

#undef ENUM_ELEMENT
#undef BEGIN_ENUM
#undef END_ENUM


