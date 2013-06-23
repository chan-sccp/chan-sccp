/*!
 * \file        sccp_event_enums.hh
 * \brief       SCCP Event Enums
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2013-04-04 12:59:26 +0200 (Thu, 04 Apr 2013) $
 * $Revision: 4514 $
 */
/*!
 * \brief SCCP Event Type ENUM
 */
BEGIN_ENUM(sccp,event_type)
        ENUM_ELEMENT(SCCP_EVENT_LINE_CREATED				,=1<<0,	"Line Created")
        ENUM_ELEMENT(SCCP_EVENT_LINE_CHANGED				,=1<<1,	"Line Changed")
        ENUM_ELEMENT(SCCP_EVENT_LINE_DELETED				,=1<<2,	"Line Deleted")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_ATTACHED				,=1<<3,	"Device Attached")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_DETACHED				,=1<<4,	"Device Detached")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_PREREGISTERED			,=1<<5,	"Device Preregistered")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_REGISTERED			,=1<<6,	"Device Registered")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_UNREGISTERED			,=1<<7,	"Device Unregistered")
        ENUM_ELEMENT(SCCP_EVENT_FEATURE_CHANGED				,=1<<8,	"Feature Changed")
        ENUM_ELEMENT(SCCP_EVENT_LINESTATUS_CHANGED			,=1<<9,	"LineStatus Changed")
END_ENUM(sccp,event_type)
