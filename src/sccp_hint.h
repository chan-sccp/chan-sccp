/*!
 * \file        sccp_hint.h
 * \brief       SCCP Hint Header
 * \author      Marcello Ceschia <marcelloceschia [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2009-01-16
 *
 * $Date: 2010-11-17 12:03:44 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2130 $  
 */

#ifndef SCCP_HINT_H_
#define SCCP_HINT_H_

sccp_channelState_t sccp_hint_getLinestate(const char *linename, const char *deviceId);
void sccp_hint_module_start(void);
void sccp_hint_module_stop(void);

#endif														/* SCCP_HINT_H_ */
