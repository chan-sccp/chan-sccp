/*!
 * \file	sccp_featureConference.c
 * \brief	SCCP Conference Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2017-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_featureConference.h"

#if CS_SCCP_CONFERENCE
#include "sccp_utils.h"
#include "sccp_vector.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_channel.h"
#include "sccp_features.h"
#include "sccp_labels.h"
#include "revision.h"

static const uint32_t appID = APPID_CONFERENCE;

#ifdef HAVE_PBX_APP_H
#  include <asterisk/app.h>
#endif

#include <asterisk/stasis.h>
#include <asterisk/stasis_bridges.h>
#include <asterisk/stasis_channels.h>

/* forward declarations */

/* private variables */

/* private functions */
//#define sccp_conference_lock(x)	pbx_mutex_lock(&((sccp_conference_t * const)(x))->lock);			// discard const
//#define sccp_conference_unlock(x)	pbx_mutex_unlock(&((sccp_conference_t * const)(x))->lock);			// discard const
static void bridge_cb(void *data, struct stasis_subscription *sub, struct stasis_message *message) {
}

static void channel_cb(void *data, struct stasis_subscription *sub, struct stasis_message *message) {
}

typedef struct conflist {
	struct stasis_subscription *bsub;
	struct stasis_subscription *csub[1];
} conflist_t;
static conflist_t *conflist;

static int subscribeConferenceTopic() {
	struct stasis_topic *bridge_topic;
	struct ast_bridge *bridge = {0};
	bridge_topic = ast_bridge_topic(bridge);
	if (!bridge_topic) {
		return -1;
	}
	if (!(conflist->bsub = stasis_subscribe(bridge_topic, bridge_cb, NULL))) {
                return -2;
        }
        //generic_list->current_state = ast_device_state(monitor->interface->device_name);
        //ao2_t_link(generic_monitors, generic_list, "linking new generic monitor instance list");
        return 0;
}

static int subscribeChannelTopic() {
	struct stasis_topic *channel_topic;
	struct ast_channel *channel = {0};
	channel_topic = ast_channel_topic(channel);
	if (!channel_topic) {
		return -1;
	}
	if (!(conflist->csub[0] = stasis_subscribe(channel_topic, channel_cb, NULL))) {
                return -2;
        }
        //generic_list->current_state = ast_device_state(monitor->interface->device_name);
        //ao2_t_link(generic_monitors, generic_list, "linking new generic monitor instance list");
        return 0;
}

#define conf_autocontext "sccp_conference"
#define conf_registrar VCS_SHORT_HASH
static const char * const conf_app = "ConfBridge";
static int autocontext_created = 0;
//static const char * const slatrunk_app = "SLATrunk";

static int addDialplanContext(const char *conference) {
	if (!ast_context_find(conf_autocontext)) {
		/*
		if (!ast_context_find_or_create(NULL, NULL, conf_autocontext, conf_registrar)) {
			pbx_log(LOG_ERROR, "Failed to automatically find or create context '%s' for '%s'!\n", conf_autocontext, conf_registrar);
			return -1;
		}
		if (ast_add_extension(conf_autocontext, 0, "start", 1, NULL, NULL, "NOOP", "Chan-SCCP Conference Start", NULL, conf_registrar)) {
			ast_log(LOG_ERROR, "Failed to automatically create extension for trunk '%s'!\n", conf_app);
			return -2;
		}
		if (ast_add_extension(conf_autocontext, 0, "start", 2, NULL, NULL, conf_app, "", NULL, conf_registrar)) {
			ast_log(LOG_ERROR, "Failed to automatically create extension for trunk '%s'!\n", conf_app);
			return -2;
		}
		*/
		
		/*
			// The extension for when the handset goes off-hook. exten => station1,1,SLAStation(station1)
			if (ast_add_extension(station->autocontext, 0, station->name, 1,
				NULL, NULL, slastation_app, ast_strdup(station->name), ast_free_ptr, sla_registrar)) {
				ast_log(LOG_ERROR, "Failed to automatically create extension "
					"for trunk '%s'!\n", station->name);
				return -1;
			}
		
		if (ast_add_extension(station->autocontext, 0, exten, 1, NULL, NULL, slastation_app, ast_strdup(exten), ast_free_ptr, sla_registrar)) {
			ast_log(LOG_ERROR, "Failed to automatically create extension "
				"for trunk '%s'!\n", station->name);
			return -3;
		}
		// Hint for this line button exten => station1_line1,hint,SLA:station1_line1
		if (ast_add_extension(station->autocontext, 0, exten, PRIORITY_HINT, NULL, NULL, hint, NULL, NULL, sla_registrar)) {
			ast_log(LOG_ERROR, "Failed to automatically create hint "
				"for trunk '%s'!\n", station->name);
			return -4;
		}
		*/
		autocontext_created = 1;
	}
	return 0;
}

static int removeDialplanContext(const char *conference) {
	if (autocontext_created) {
		ast_context_destroy_by_name(NULL, conf_registrar);
	}
	return 0;
}

/* public functions */
static int startConference(void) {
	return 0;
}

static int endConference(const char *conference) {
	return 0;
}

static void __attribute__((constructor)) _init(void)
{
	addDialplanContext("");
}

static void __attribute__((destructor)) _destroy(void)
{
	removeDialplanContext("");
}

/* Assign to interface */
const ConferenceInterface iConference = {
	.startConference = startConference,
	//.handleConfSoftKey = ,
	//.handleConfListSoftKey = ,
	//.showConfList = ,
	//.hideConfList = ,
	//.handleButtonPress = ,
	//.handleDevice2User = ,
	//.inviteToConference = ,
	//.moveIntoConference = ,
	.endConference = endConference,
};
#else
const ConferenceInterface iConference = { 0 };
#endif
