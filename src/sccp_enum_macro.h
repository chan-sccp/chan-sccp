/*!
 * \file        sccp_enum_macro.h
 * \brief       SCCP Enum Macro
 * \author      Diederik de Groot <ddegroot [at] sf.net>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-04-15 01:04:06 +0200 (Mon, 15 Apr 2013) $
 * $Revision: 4568 $
 */

#if ( !defined(__SCCP_ENUM_MACRO_H) || defined(GENERATE_ENUM_STRINGS) )

#if (!defined(GENERATE_ENUM_STRINGS))
    #define __SCCP_ENUM_MACRO_H
#endif

/* enum macro definitions */
#undef ENUM_ELEMENT
#undef BEGIN_ENUM
#undef END_ENUM

#ifndef GENERATE_ENUM_STRINGS												// header definition (1st pass)
    #define ENUM_ELEMENT( element, index, str) element index,
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) typedef enum tag_##ENUM_NAME {
    #define END_ENUM( NAMESPACE, ENUM_NAME ) } NAMESPACE##_##ENUM_NAME##_t; 								\
        gcc_inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index);
#else															// class definition (2nd pass)
#  ifdef DEBUG														// switch/case on index (Debug mode)
    #define ENUM_ELEMENT( element, index, str) case element: return str;
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) 											\
        gcc_inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index) { 						\
                switch(index) {												
    #define END_ENUM( NAMESPACE, ENUM_NAME ) 	};										\
     		return "SCCP: ERROR lookup in " #NAMESPACE "_" #ENUM_NAME "_t";								\
        };
#  else 														// return array[index] directly (Non-Debug Mode)
    #define ENUM_ELEMENT( element, index, str ) [element] = str,
    #define BEGIN_ENUM( NAMESPACE, ENUM_NAME ) char* NAMESPACE ##_##ENUM_NAME##_map_enum2str[] = {
    #define END_ENUM( NAMESPACE, ENUM_NAME ) }; 											\
        gcc_inline const char* ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t index) { return NAMESPACE##_##ENUM_NAME##_map_enum2str[index]; }
#  endif
#endif

#endif // (!defined(__SCCP_ENUM_MACRO_H) || defined(GENERATE_ENUM_STRINGS))
