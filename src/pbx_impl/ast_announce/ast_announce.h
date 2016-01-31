/*!
 * \file        ast112_announce.c
 * \brief       SCCP PBX Conference Announcement Channel Tech
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Referencing ConfBridge Announcement Channel Created by Richard Mudgett <rmudgett@digium.com>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
struct ast_channel_tech *sccpconf_announce_get_tech(void);
void sccpconf_announce_channel_depart(struct ast_channel *chan);
int sccpconf_announce_channel_push(struct ast_channel *ast, struct ast_bridge *bridge);
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
