#ifndef SCCP_MWI_H_
#define SCCP_MWI_H_

#include "asterisk.h"
#include "sccp_event.h"

#ifdef CS_AST_HAS_EVENT
#include "asterisk/event.h"
#endif

/**
 * holding line information for mailbox subscription
 */
typedef struct sccp_mailboxLine sccp_mailboxLine_t;
struct sccp_mailboxLine{

	sccp_line_t		*line;
	SCCP_LIST_ENTRY(sccp_mailboxLine_t) list;
};

/**
 * we hold a mailbox event subscription in sccp_mailbox_subscription_t.
 *
 * Each line that holds a subscription for this mailbox is listed in
 */
typedef struct sccp_mailbox_subscriber_list sccp_mailbox_subscriber_list_t;
struct sccp_mailbox_subscriber_list{
	char mailbox[60];
	char context[60];

	SCCP_LIST_HEAD(, sccp_mailboxLine_t) sccp_mailboxLine;
	SCCP_LIST_ENTRY(sccp_mailbox_subscriber_list_t) list;

	struct{
		int			newmsgs;
		int			oldmsgs;
	}currentVoicemailStatistic;

	struct{
		int			newmsgs;
		int			oldmsgs;
	}previousVoicemailStatistic;

#ifdef CS_AST_HAS_EVENT
	struct ast_event_sub 				*event_sub;
#endif
};


SCCP_LIST_HEAD(, sccp_mailbox_subscriber_list_t) sccp_mailbox_subscriptions;



void sccp_mwi_module_start(void);
void sccp_mwi_stopMonitor(void);
int sccp_mwi_startMonitor(void);
void sccp_mwi_check(sccp_device_t *device);

#ifdef CS_AST_HAS_EVENT

void sccp_mwi_event(const struct ast_event *event, void *data);
void sccp_mwi_subscribeMailbox(sccp_line_t **l, sccp_mailbox_t **m);
void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t **mailbox);

void sccp_mwi_linecreatedEvent(const sccp_event_t **event);
void sccp_mwi_deviceAttachedEvent(const sccp_event_t **event);
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t **line);

#else
void *sccp_mwi_progress(void *data);
#endif


#endif /*SCCP_MWI_H_*/
