/*!
 * \file        ast112_announce.c
 * \brief       SCCP PBX Conference Announcement Channel Tech
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        Referencing ConfBridge Announcement Channel Created by Richard Mudgett <rmudgett@digium.com>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */

#include <asterisk.h>
#include "define.h"

#include <asterisk/channel.h>
#include <asterisk/bridge.h>
#include <asterisk/core_unreal.h>
#include "ast_announce.h"

//ASTERISK_FILE_VERSION(__FILE__, "");

/* ------------------------------------------------------------------- */
/*! ConfBridge announcer channel private. */
struct announce_pvt {
	/*! Unreal channel driver base class values. */
	struct ast_unreal_pvt base;
	/*! Conference bridge associated with this announcer. */
	// struct ast_bridge *bridge;
};

static int announce_call(struct ast_channel *chan, const char *addr, int timeout)
{
	/* Make sure anyone calling ast_call() for this channel driver is going to fail. */
	return -1;
}

static int announce_hangup(struct ast_channel *ast)
{
	struct announce_pvt *p = ast_channel_tech_pvt(ast);
	int res;

	if (!p) {
		return -1;
	}

	/* give the pvt a ref to fulfill calling requirements. */
	ao2_ref(p, +1);
	res = ast_unreal_hangup(&p->base, ast);
	ao2_ref(p, -1);

	return res;
}

static void announce_pvt_destructor(void *vdoomed)
{
	struct announce_pvt *doomed = vdoomed;

	// ao2_cleanup(doomed->bridge);
	// doomed->bridge = NULL;
	ast_unreal_destructor(&doomed->base);
}

static struct ast_channel *announce_request(const char *type, struct ast_format_cap *cap, const struct ast_assigned_ids *assignedids, const struct ast_channel *requestor, const char *data, int *cause)
{
	struct ast_channel *chan;
	const char *conf_name = data;

	/* Allocate a new private structure and then Asterisk channels */
	struct announce_pvt *pvt = (struct announce_pvt *) ast_unreal_alloc(sizeof(*pvt), announce_pvt_destructor, cap);
	if (!pvt) {
		return NULL;
	}
	ast_set_flag(&pvt->base, AST_UNREAL_NO_OPTIMIZATION);
	ast_copy_string(pvt->base.name, conf_name, sizeof(pvt->base.name));

#if !defined(CS_AST_CHANNEL_CALLID_TYPEDEF)
	chan = ast_unreal_new_channels(&pvt->base, sccpconf_announce_get_tech(), AST_STATE_UP, AST_STATE_UP, NULL, NULL, assignedids, requestor, NULL);
#else
	chan = ast_unreal_new_channels(&pvt->base, sccpconf_announce_get_tech(), AST_STATE_UP, AST_STATE_UP, NULL, NULL, assignedids, requestor, 0);
#endif
	if (chan) {
		ast_answer(pvt->base.owner);
		ast_answer(pvt->base.chan);
		if (ast_channel_add_bridge_role(pvt->base.chan, "announcer")) {
			ast_hangup(chan);
			chan = NULL;
		}
	}
	ao2_cleanup(pvt);

	return chan;
}

static struct ast_channel_tech announce_tech = {
	.type = "SCCPCBAnn",
	.description = "SCCP Conference Bridge Announcing Channel",
	.requester = announce_request,
	.call = announce_call,
	.hangup = announce_hangup,

	.send_digit_begin = ast_unreal_digit_begin,
	.send_digit_end = ast_unreal_digit_end,
	.read = ast_unreal_read,
	.write = ast_unreal_write,
	.write_video = ast_unreal_write,
	.exception = ast_unreal_read,
	.indicate = ast_unreal_indicate,
	.fixup = ast_unreal_fixup,
	.send_html = ast_unreal_sendhtml,
	.send_text = ast_unreal_sendtext,
	.queryoption = ast_unreal_queryoption,
	.setoption = ast_unreal_setoption,
	.properties = AST_CHAN_TP_INTERNAL,
};

struct ast_channel_tech *sccpconf_announce_get_tech(void)
{
	return &announce_tech;
}

void sccpconf_announce_channel_depart(struct ast_channel *chan)
{
	struct announce_pvt *p = ast_channel_tech_pvt(chan);

	if (!p) {
		return;
	}

	ao2_ref(p, +1);
	ao2_lock(p);
	if (!ast_test_flag(&p->base, AST_UNREAL_CARETAKER_THREAD)) {
		ao2_unlock(p);
		ao2_ref(p, -1);
		return;
	}
	ast_clear_flag(&p->base, AST_UNREAL_CARETAKER_THREAD);
	chan = p->base.chan;
	ao2_unlock(p);
	ao2_ref(p, -1);
	if (chan) {
		ast_bridge_depart(chan);
		ast_channel_unref(chan);
	}
}

int sccpconf_announce_channel_push(struct ast_channel *ast, struct ast_bridge *bridge)
{
	struct ast_bridge_features *features;
	struct ast_channel *chan;
	struct announce_pvt *p = NULL;

	{
		ast_channel_lock(ast);
		p = ast_channel_tech_pvt(ast);
		if (!p) {
			ast_channel_unlock(ast);
			return -1;
		}
		ao2_ref(p, +1);
		chan = p->base.chan;
		if (!chan) {
			ast_channel_unlock(ast);
			ao2_cleanup(p);
			return -1;
		}
		ast_channel_ref(chan);
		ast_channel_unlock(ast);
	}

	features = ast_bridge_features_new();
	if (!features) {
		ast_channel_unref(chan);
		ao2_cleanup(p);
		return -1;
	}
	ast_set_flag(&features->feature_flags, AST_BRIDGE_CHANNEL_FLAG_IMMOVABLE);

	// Impart the output channel into the bridge
	if (ast_bridge_impart(bridge, chan, NULL, features, AST_BRIDGE_IMPART_CHAN_DEPARTABLE)) {
		ast_bridge_features_destroy(features);
		ast_channel_unref(chan);
		ao2_cleanup(p);
		return -1;
	}
	ao2_lock(p);
	ast_set_flag(&p->base, AST_UNREAL_CARETAKER_THREAD);
	ao2_unlock(p);
	ao2_cleanup(p);
	return 0;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
