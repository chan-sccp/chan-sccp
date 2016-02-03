/*!
 * \file        chan_sccp.h
 * \brief       Chan SCCP Main Class
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \brief       Main chan_sccp Class
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \warning     File has been Lined up using 8 Space TABS
 *
 */
#pragma once
#include "config.h"
#include "define.h"
//#include "sccp_codec.h"

__BEGIN_C_EXTERN__
SCCP_API int SCCP_CALL load_config(void);
SCCP_API int SCCP_CALL sccp_preUnload(void);
SCCP_API int SCCP_CALL sccp_reload(void);
SCCP_API boolean_t SCCP_CALL sccp_prePBXLoad(void);
SCCP_API boolean_t SCCP_CALL sccp_postPBX_load(void);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
