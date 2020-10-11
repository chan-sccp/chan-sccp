/*!
 * \file	sccp_featureParkingLot.c
 * \brief	SCCP ParkingLot Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2015-Sept-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_mwi.h"
#include "sccp_vector.h"
#include "sccp_line.h"
#include "sccp_device.h"
#include "sccp_linedevice.h"
#include "sccp_utils.h"

#if CS_AST_HAS_EVENT
#	include <asterisk/event.h>
#elif HAVE_PBX_STASIS_H
#	include <asterisk/stasis.h>
#endif
#ifdef HAVE_PBX_MWI_H				// ast_mwi_state_type
#	include <asterisk/mwi.h>
#else
#	ifdef HAVE_PBX_APP_H                                        // ast_mwi_state_type
#		include <asterisk/app.h>
#	endif
#endif
#include <asterisk/cli.h>

pbx_mutex_t subscriptions_lock;
#define subscription_lock()		({pbx_mutex_lock(&subscriptions_lock);})		// discard const
#define subscription_unlock()		({pbx_mutex_unlock(&subscriptions_lock);})		// discard const

//typedef struct pbx_event_sub pbx_event_subscription_t;
typedef struct {
	sccp_mailbox_t *mailbox;
	constLinePtr line;
#if MWI_USE_EVENT
	pbx_event_subscription_t *pbx_subscription;
#else
	int sched;
#endif
} mwi_subscription_t;
SCCP_VECTOR(sccp_subscription_vector, mwi_subscription_t *) subscriptions;

/* Forward Declarations */
void NotifyLine(constLinePtr line, int newmsgs, int oldmsgs);

/* =======================
 * Pbx Event CallBacks
 * ======================= */
#if defined(CS_AST_HAS_EVENT)
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);

	// split uniqueid
	char *context, *mbox = NULL;
	mbox = context = pbx_strdupa((subscription->mailbox)->uniqueid);
	strsep(&context, "@");
	if (sccp_strlen_zero(context)) {
		context = "default";
	}

	RAII(pbx_event_t *, event, NULL, pbx_event_destroy);
	event = pbx_event_get_cached(	AST_EVENT_MWI,
					AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, mbox,
					AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, context,
					AST_EVENT_IE_END);

	if (event) {
		int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
		int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
		NotifyLine(subscription->line, newmsgs, oldmsgs);
	}
}
static void pbx_mwi_event(const pbx_event_t *event, void *data)
{
	mwi_subscription_t *subscription = (mwi_subscription_t *)data;
	if (!subscription || !subscription->line || !event) {
		pbx_log(LOG_ERROR, "SCCP: MWI Event skipped (%p, %p)\n", subscription, event);
		return;
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s, event:%p\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid, event);

	int newmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_NEWMSGS);
	int oldmsgs = pbx_event_get_ie_uint(event, AST_EVENT_IE_OLDMSGS);
	NotifyLine(subscription->line, newmsgs, oldmsgs);
}
static pbx_event_subscription_t *pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);

	pbx_event_subscription_t *pbx_subscription = NULL;
	
	// split uniqueid
	char *context, *mbox = NULL;
	mbox = context = pbx_strdupa((subscription->mailbox)->uniqueid);
	strsep(&context, "@");
	if (sccp_strlen_zero(context)) {
		context = "default";
	}

#if ASTERISK_VERSION_NUMBER >= 10800
	pbx_subscription = pbx_event_subscribe(AST_EVENT_MWI, pbx_mwi_event, "mailbox subscription", subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, mbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, context, AST_EVENT_IE_NEWMSGS, AST_EVENT_IE_PLTYPE_EXISTS, AST_EVENT_IE_END);
#else
	pbx_subscription = pbx_event_subscribe(AST_EVENT_MWI, pbx_mwi_event, subscription, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, mbox, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, context, AST_EVENT_IE_END);
#endif
	if (!pbx_subscription) {
		pbx_log(LOG_ERROR, "SCCP: PBX MWI event could not be subscribed to for mailbox %s\n", (subscription->mailbox)->uniqueid);
	}
	pbxMailboxGetCached(subscription);
	return pbx_subscription;
}
static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	if(subscription->pbx_subscription) {
		pbx_event_unsubscribe(subscription->pbx_subscription);
	}
}
#elif defined(CS_AST_HAS_STASIS)
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);
	RAII(struct stasis_message *, mwi_message, NULL, ao2_cleanup);
	mwi_message = stasis_cache_get(ast_mwi_state_cache(), ast_mwi_state_type(), (subscription->mailbox)->uniqueid);
	if (mwi_message) {
		struct ast_mwi_state *mwi_state = (struct ast_mwi_state *) stasis_message_data(mwi_message);
		NotifyLine(subscription->line, mwi_state->new_msgs, mwi_state->old_msgs);
	}
}
static void pbx_mwi_event(void *data, struct stasis_subscription *sub, struct stasis_message *msg)
{
	mwi_subscription_t *subscription = (mwi_subscription_t *)data;
	struct ast_mwi_state *mwi_state = NULL;
	//if (!subscription || !subscription->line || stasis_subscription_final_message(sub, msg)) {
	if (!subscription || !subscription->line) {
		pbx_log(LOG_ERROR, "SCCP: MWI Event skipped (%p, %s)\n", subscription, stasis_message_type_name(stasis_message_type(msg)));
		return;
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s, msgtype:%s\n", 
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid, stasis_message_type_name(stasis_message_type(msg)));
	if (pbx_mwi_state_type() == stasis_message_type(msg) && (mwi_state = (struct ast_mwi_state *) stasis_message_data(msg))) {
		NotifyLine(subscription->line, mwi_state->new_msgs, mwi_state->old_msgs);
	} else {
		// only required on some asterisk-16.1.1 versions, where new events only arrive if the cache has been read
		pbxMailboxGetCached(subscription);
	}
}
static pbx_event_subscription_t * pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	pbx_event_subscription_t *pbx_subscription = NULL;
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);

#	if ASTERISK_VERSION_GROUP >= 117
	pbx_subscription = (pbx_event_subscription_t *)pbx_mwi_subscribe_pool(subscription->mailbox->uniqueid, pbx_mwi_event, subscription);
#	else
	struct stasis_topic * mailbox_specific_topic = pbx_mwi_topic((subscription->mailbox)->uniqueid);
	if (mailbox_specific_topic) {
		pbx_subscription = stasis_subscribe_pool(mailbox_specific_topic, pbx_mwi_event, subscription);
#		if CS_AST_HAS_STASIS_SUBSCRIPTION_SET_FILTER
		stasis_subscription_accept_message_type(pbx_subscription, pbx_mwi_state_type());
		stasis_subscription_accept_message_type(pbx_subscription, stasis_subscription_change_type());
		//stasis_subscription_accept_message_type(subscription->pbx_subscription, stasis_subscription_change_type());
		stasis_subscription_set_filter(pbx_subscription, STASIS_SUBSCRIPTION_FILTER_SELECTIVE);
#		endif
	}
#	endif
	//pbxMailboxGetCached(subscription);
	return pbx_subscription;
}

static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);

	if(subscription->pbx_subscription) {
#	if ASTERISK_VERSION_GROUP >= 117
		pbx_mwi_unsubscribe_and_join((struct pbx_mwi_subscriber *)subscription->pbx_subscription);
#	else
		stasis_unsubscribe_and_join(subscription->pbx_subscription);
#	endif
	}
}
/* discard polling implementation */
/*
#else
static void pbxMailboxGetCached(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);
	if (pbx_app_inboxcount(subscription->uniqueid, &(mailbox->newmsgs), &(mailbox->oldmsgs))) {
		pbx_log(LOG_ERROR, "Failed to retrieve messages from mailbox:%s\n", mailbox->uniqueid);
	}
	NotifyLine(subscription->line, newmsgs, oldmsgs);
}
static void pbxMailboxReschedule(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);
	if ((subscription->schedUpdate = iPbx.sched_add(interval * 1000, pbxMailboxGetCached, subscription)) < 0) {
		pbx_log(LOG_ERROR, "Error creating mailbox subscription.\n");
	}
}
static int pbx_mwi_event(const void *data)
{
	mwi_subscription_t *subscription = data;
	if (!subscription) {
		// error
	}
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);
	pbxMailboxGetCached(subscription);
	pbxMailboxReschedule(subscription);
}

static void pbxMailboxSubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__, (subscription->mailbox)->uniqueid);
	pbx_mwi_event(subscription);
}
static void pbxMailboxUnsubscribe(mwi_subscription_t *subscription)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_1 "%s: (mwi::%s) uniqueid:%s\n",
		(subscription->line)->name, __PRETTY_FUNCTION__,  (subscription->mailbox)->uniqueid);
	subscription->sched = SCCP_SCHED_DEL(subscription->schedUpdate);
}
*/
#endif

/* ===========================
 * Create/Destroy Subscription
 * =========================== */
static void createSubscription(sccp_mailbox_t * mailbox, constLinePtr line)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) uniqueid:%s\n",
		line->name, __PRETTY_FUNCTION__, mailbox->uniqueid);

	mwi_subscription_t *subscription = (mwi_subscription_t *)sccp_calloc(sizeof *subscription,1);
	if (!subscription) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return;
	}
	subscription->mailbox = mailbox;
	*(sccp_line_t **)&(subscription->line) = sccp_line_retain(line);                                        //! const cast / emplace line
	if (!subscription->line) {
		pbx_log(LOG_ERROR, "Could not retain the line, to assign to this subscription\n");
		sccp_free(subscription);
		return;
	}

	subscription_lock();
	SCCP_VECTOR_APPEND(&subscriptions, subscription);
	subscription_unlock();

	subscription->pbx_subscription = pbxMailboxSubscribe(subscription);
}

static void removeSubscription(sccp_mailbox_t * mailbox, constLinePtr line)
{
	mwi_subscription_t *removed = NULL;
	boolean_t found_one = FALSE;
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::%s) uniqueid:%s\n",
		line->name, __PRETTY_FUNCTION__, mailbox->uniqueid);

	do {
		found_one = FALSE;
		subscription_lock();
		for(uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++) {
			mwi_subscription_t * subscription = SCCP_VECTOR_GET(&subscriptions, idx);
			if(subscription->mailbox == mailbox && subscription->line == line) {
				SCCP_VECTOR_REMOVE_UNORDERED(&subscriptions, idx);
				removed = subscription;
				found_one = TRUE;
				break;
			}
		}
		subscription_unlock();
		if(removed) {
			pbxMailboxUnsubscribe(removed);
			sccp_line_release(&removed->line);
			sccp_free(removed);
		}
	} while (found_one);
}

/*!
 * \note this does not lock the subscriptions_lock, to prevent potential (future) deadlock in pbxMailboxUnsubscribe
 * Only call this function, after unsubscribing all sccp_events
 */
static void removeAllSubscriptions(void)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "SCCP: (mwi::removeAllSubscriptions)\n");
	for (uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++) {
		mwi_subscription_t *subscription = SCCP_VECTOR_GET(&subscriptions, idx);
		if (subscription) {
			SCCP_VECTOR_REMOVE_UNORDERED(&subscriptions, idx);
			pbxMailboxUnsubscribe(subscription);
			sccp_line_release(&subscription->line);
			sccp_free(subscription);
		}
	}
}

/* ===========================
 * Handle SCCP Events
 * =========================== */
static void handleLineCreationEvent(const sccp_event_t * event)
{
	if (!event || !event->lineInstance.line) {
		pbx_log(LOG_ERROR, "Event or line not provided\n");
		return;
	}
	sccp_line_t *line = event->lineInstance.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::handleLineCreationEvent)\n", line->name);
	sccp_mailbox_t *mailbox = NULL;
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&(line->mailboxes), mailbox, list) {
		createSubscription(mailbox, line);
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

static void handleLineDestructionEvent(const sccp_event_t * event)
{
	if (!event || !event->lineInstance.line) {
		pbx_log(LOG_ERROR, "Eevent or line not provided\n");
		return;
	}
	sccp_line_t *line = event->lineInstance.line;

	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::handleLineDestructionEvent)\n", line->name);

	sccp_mailbox_t *mailbox = NULL;
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&(line->mailboxes), mailbox, list) {
		removeSubscription(mailbox, line);
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
}

/* ==================================
 * Inform the line of any MWI changes
 * ================================== */
void NotifyLine(constLinePtr l, int newmsgs, int oldmsgs)
{
	sccp_log((DEBUGCAT_MWI)) (VERBOSE_PREFIX_2 "%s: (mwi::NotifyLine) Notify newmsgs:%d oldmsgs:%d\n", l->name, newmsgs, oldmsgs);

	sccp_line_setMWI(l, newmsgs, oldmsgs);

	sccp_linedevice_t * ld = NULL;
	if (SCCP_LIST_GETSIZE(&l->devices)) {
		SCCP_LIST_LOCK(&l->devices);
		SCCP_LIST_TRAVERSE(&l->devices, ld, list) {
			AUTO_RELEASE(sccp_device_t, d, sccp_device_retain(ld->device));
			if (d) {
				sccp_linedevice_indicateMWI(ld);
				sccp_device_setMWI(d);
			}
		}
		SCCP_LIST_UNLOCK(&l->devices);
	}
}

/*!
 * \brief Show MWI Subscriptions
 * \param fd Fd as int
 * \param total Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 *
 * \called_from_asterisk
 */
/* ==================================
 * CLI / MIW Output
 * ================================== */
static int showSubscriptions(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	//sccp_mailboxLine_t *mailboxLine = NULL;
	//char linebuf[31] = "";
	int local_line_total = 0;

	subscription_lock();
#define CLI_AMI_TABLE_NAME MWISubscriptions
#define CLI_AMI_TABLE_PER_ENTRY_NAME MailboxSubscriber
#define CLI_AMI_TABLE_ITERATOR for (uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&subscriptions); idx++)
#define CLI_AMI_TABLE_BEFORE_ITERATION                                            \
	mwi_subscription_t * subscription = SCCP_VECTOR_GET(&subscriptions, idx); \
	constLinePtr line = subscription->line;

#if defined (CS_AST_HAS_EVENT)
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-30.30",	s,	30,	(subscription->mailbox)->uniqueid)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-20.20",	s,	20,	line->name)							\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)				\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->pbx_subscription ? "YES" : "NO")

#elif defined(CS_AST_HAS_STASIS)
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-30.30",	s,	30,	(subscription->mailbox)->uniqueid)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-20.20",	s,	20,	line->name)							\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)				\
 		CLI_AMI_TABLE_FIELD(Sub,		"-3.3",		s,	3,	subscription->pbx_subscription ? "YES" : "NO")			\
		CLI_AMI_TABLE_FIELD(AstUniq,		"36.36",	s,	36,	subscription->pbx_subscription ? 				\
											stasis_subscription_uniqueid(subscription->pbx_subscription) : "")
#else
#define CLI_AMI_TABLE_FIELDS 																\
 		CLI_AMI_TABLE_FIELD(Mailbox,		"-30.30",	s,	30,	(subscription->mailbox)->uniqueid)				\
 		CLI_AMI_TABLE_FIELD(LineName,		"-20.20",	s,	20,	line->name)							\
 		CLI_AMI_TABLE_FIELD(New,		"3.3",		d,	3,	line->voicemailStatistic.newmsgs)				\
 		CLI_AMI_TABLE_FIELD(Old,		"3.3",		d,	3,	line->voicemailStatistic.oldmsgs)
#endif
#include "sccp_cli_table.h"
		local_line_total++;
	subscription_unlock();
	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

/* ==================================
 * Module Init / Register SCCP Events
 * ================================== */
static void module_start(void)
{
	pbx_log(LOG_NOTICE, "SCCP: (mwi::module_start)\n");
	SCCP_VECTOR_INIT(&subscriptions,10);
	pbx_mutex_init(&subscriptions_lock);

	sccp_event_subscribe(SCCP_EVENT_LINEINSTANCE_CREATED, handleLineCreationEvent,TRUE);
	sccp_event_subscribe(SCCP_EVENT_LINEINSTANCE_DESTROYED, handleLineDestructionEvent, FALSE);
}

static void module_stop(void)
{
	pbx_log(LOG_NOTICE, "SCCP: (mwi::module_stop)\n");
	sccp_event_unsubscribe(SCCP_EVENT_LINEINSTANCE_DESTROYED, handleLineDestructionEvent);
	sccp_event_unsubscribe(SCCP_EVENT_LINEINSTANCE_CREATED, handleLineCreationEvent);

	removeAllSubscriptions();
	SCCP_VECTOR_FREE(&subscriptions);
	pbx_mutex_destroy(&subscriptions_lock);
}

/* =====================================
 * Assign external function to interface
 * ===================================== */
const VoicemailInterface iVoicemail = {
	.startModule = module_start,
	.stopModule = module_stop,
	.showSubscriptions = showSubscriptions,
};
