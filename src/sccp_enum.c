
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
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 4618 $")
#define GENERATE_ENUM_STRINGS  // Start string generation
#include "sccp_enum_entries.hh"
#undef GENERATE_ENUM_STRINGS   // Stop string generation

