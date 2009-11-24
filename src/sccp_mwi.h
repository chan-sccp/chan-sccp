/*!
 * \file 	sccp_mwi.h
 * \brief 	SCCP Message Waiting Indicator Header
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \date	2008-11-22
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 */

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

struct sccp_mailboxLine {
        sccp_line_t				* line;				/*!< SCCP Line */
        SCCP_LIST_ENTRY(sccp_mailboxLine_t) 	list;				/*!< Line Linked List Entry */
};										/*!< SCCP MailBox Line Structure */

/**
 * \brief we hold a mailbox event subscription in sccp_mailbox_subscription_t.
 *
 * Each line that holds a subscription for this mailbox is listed in
 */
typedef struct sccp_mailbox_subscriber_list 	sccp_mailbox_subscriber_list_t;

struct sccp_mailbox_subscriber_list {
        char 					mailbox[60];			/*!< MailBox */
        char 					context[60];			/*!< Context */

        SCCP_LIST_HEAD(, sccp_mailboxLine_t) 	sccp_mailboxLine;		/*!< SCCP Mail Box Line */
        SCCP_LIST_ENTRY(sccp_mailbox_subscriber_list_t) list;			/*!< MailBox Subscriber Linked List Entry */

        struct {
                int				newmsgs;			/*!< New Messages */
                int				oldmsgs;			/*!< Old Messages */
        }currentVoicemailStatistic;						/*!< Current VoiceMail Statistic Structure */

        struct {
                int				newmsgs;			/*!< New Messages */
                int				oldmsgs;			/*!< Old Messages */
        }previousVoicemailStatistic;						/*!< Previous VoiceMail Statistics Structure */

#ifdef CS_AST_HAS_EVENT

        struct ast_event_sub 			* event_sub;			/*!< Astersik Event Sub */
#endif
};										/*!< SCCP MailBox Subscriber List Structure */

SCCP_LIST_HEAD(, sccp_mailbox_subscriber_list_t) sccp_mailbox_subscriptions;

void sccp_mwi_module_start(void);
void sccp_mwi_stopMonitor(void);
int sccp_mwi_startMonitor(void);
void sccp_mwi_check(sccp_device_t *device);


#ifdef CS_AST_HAS_EVENT

void sccp_mwi_event(const struct ast_event *event, void *data);
void sccp_mwi_subscribeMailbox(sccp_line_t **l, sccp_mailbox_t **m);
void sccp_mwi_unsubscribeMailbox(sccp_mailbox_t **mailbox);
void sccp_mwi_addMailboxSubscription(char *mailbox, char *context, sccp_line_t **line);

#else
void *sccp_mwi_progress(void *data);
#endif


#endif /*SCCP_MWI_H_*/
