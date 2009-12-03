/*!
 * \file 	sccp_lists.h
 * \brief 	SCCP Lists Header
 * \author 	Federico Santulli <info [at] chan-sccp.org>
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * \note	Code derives from Asterisk 1.6 "linkedlists.h"
 *
 *		File is not directly included to get benefit from lists also in
 *		previous Asterisk releases (like 1.2)
 */
#ifndef _SCCP_LISTS_H
#define _SCCP_LISTS_H
#include "sccp_lock.h"

/* Main list head */
#define SCCP_LIST_HEAD(name, type)
struct name {
	ast_mutex_t lock;							/*!< Asterisk: Lock Me Up and Tie me Down */
	type *first;								/*!< Pointer to First Record */
	type *last;								/*!< Pointer to Last Record */
	uint16_t size;								/*!< Size */
} 										/*!< Name Structure */

/* Initialize list head */
#define SCCP_LIST_HEAD_SET(head, entry) do {
	(head)->first = (entry);
	(head)->last = (entry);
	ast_mutex_init(&(head)->lock);
} while (0)

 /* List Item */
#define SCCP_LIST_ENTRY(type)
struct {
	type *next;
}

/* List First Item */
#define	SCCP_LIST_FIRST(head)	((head)->first)
/* List Last Item */
#define	SCCP_LIST_LAST(head)	((head)->last)
/* List Next Item */
#define SCCP_LIST_NEXT(elm, field)	((elm)->field.next)
/* List Clear */
#define	SCCP_LIST_EMPTY(head)	(SCCP_LIST_FIRST(head) == NULL)
/* List Explore Routine */
#define SCCP_LIST_TRAVERSE(head,var,field)
for ((var) = (head)->first; (var); (var) = (var)->field.next)

/* List Safe Explore Routine */
#define SCCP_LIST_TRAVERSE_SAFE_BEGIN(head, var, field) {
	typeof((head)) __list_head = head;
	typeof(__list_head->first) __list_next;
	typeof(__list_head->first) __list_prev = NULL;
	typeof(__list_head->first) __new_prev = NULL;
	for ((var) = __list_head->first, __new_prev = (var),
	__list_next = (var) ? (var)->field.next : NULL;
	(var);
	__list_prev = __new_prev, (var) = __list_next,
	__new_prev = (var),
	__list_next = (var) ? (var)->field.next : NULL
//)
}
/* Current List Item Removal */
#define SCCP_LIST_REMOVE_CURRENT(field) do {
	__new_prev->field.next = NULL;
	__new_prev = __list_prev;
	if (__list_prev)
		__list_prev->field.next = __list_next;
	else
		__list_head->first = __list_next;
	if (!__list_next)
		__list_head->last = __list_prev;
	(head)->size--;
} while (0)

/* Move Current List Item */
#define SCCP_LIST_MOVE_CURRENT(newhead, field) do {
	typeof ((newhead)->first) __list_cur = __new_prev;
	SCCP_LIST_REMOVE_CURRENT(field);
	SCCP_LIST_INSERT_TAIL((newhead), __list_cur, field);
} while (0)

/* List Item Insertion before Current */
#define SCCP_LIST_INSERT_BEFORE_CURRENT(elm, field) do {
	if (__list_prev)
	{
	(elm)->field.next = __list_prev->field.next;
	__list_prev->field.next = elm;
	} else {
		(elm)->field.next = __list_head->first;
	__list_head->first = (elm);
	}
	__new_prev = (elm);
	(head)->size++;
} while (0)

 /* List Traverse End (Parentesis) */
#define SCCP_LIST_TRAVERSE_SAFE_END }

/* List Head Init */
#define SCCP_LIST_HEAD_INIT(head) {
	(head)->first = NULL;
	(head)->last = NULL;
	ast_mutex_init(&(head)->lock);
	(head)->size = 0;
}

/* List Head Destroy*/
#define SCCP_LIST_HEAD_DESTROY(head) {
	(head)->first = NULL;
	(head)->last = NULL;
	ast_mutex_destroy(&(head)->lock);
	(head)->size = 0;
}

 /* List Item Insertion After */
#define SCCP_LIST_INSERT_AFTER(head, listelm, elm, field) do {
	(elm)->field.next = (listelm)->field.next;
	(listelm)->field.next = (elm);
	if ((head)->last == (listelm))
		(head)->last = (elm);
	(head)->size++;
} while (0)

  /* List Insertion in Alphanumeric Order */
#define SCCP_LIST_INSERT_SORTALPHA(head, elm, field, sortfield) do {
	if (!(head)->first)
	{
		(head)->first = (elm);
		(head)->last = (elm);
	} else {
	typeof((head)->first) cur = (head)->first, prev = NULL;
	while (cur && strcmp(cur->sortfield, elm->sortfield) < 0) {
			prev = cur;
			cur = cur->field.next;
		}
		if (!prev) {
			SCCP_LIST_INSERT_HEAD(head, elm, field);
		} else if (!cur) {
			SCCP_LIST_INSERT_TAIL(head, elm, field);
		} else {
			SCCP_LIST_INSERT_AFTER(head, prev, elm, field);
		}
	}
	(head)->size++;
} while (0)

/* Inserts a list item at the head of a list.*/
#define SCCP_LIST_INSERT_HEAD(head, elm, field) do {	\
		(elm)->field.next = (head)->first;				\
		(head)->first = (elm);							\
		if (!(head)->last)								\
			(head)->last = (elm);						\
		(head)->size++;									\
} while (0)

/* Inserts a list item at the head of a list */
#define SCCP_LIST_INSERT_TAIL(head, elm, field) do {	\
      if (!(head)->first) {								\
		(head)->first = (elm);							\
		(head)->last = (elm);							\
      } else {											\
		(head)->last->field.next = (elm);				\
		(head)->last = (elm);							\
      }													\
      (head)->size++;									\
} while (0)

/* Append a whole list to another */
#define SCCP_LIST_APPEND_LIST(head, list, field) do {	\
      if (!(head)->first) {								\
		(head)->first = (list)->first;					\
		(head)->last = (list)->last;					\
      } else {											\
		(head)->last->field.next = (list)->first;		\
		(head)->last = (list)->last;					\
      }													\
      (list)->first = NULL;								\
      (list)->last = NULL;								\
} while (0)

/* Remove the head item from a list giving back a pointer to it. */
#define SCCP_LIST_REMOVE_HEAD(head, field) ({			\
		typeof((head)->first) cur = (head)->first;		\
		if (cur) {										\
			(head)->first = cur->field.next;			\
			cur->field.next = NULL;						\
			if ((head)->last == cur)					\
				(head)->last = NULL;					\
		}												\
		cur;											\
	})

/* Remove an item from a list */
#define SCCP_LIST_REMOVE(head, elm, field) ({			\
	__typeof(elm) __res = NULL; 						\
	if ((head)->first == (elm)) {						\
		__res = (head)->first;                      	\
		(head)->first = (elm)->field.next;				\
		if ((head)->last == (elm))						\
			(head)->last = NULL;						\
	} else {											\
		typeof(elm) curelm = (head)->first;				\
		while (curelm && (curelm->field.next != (elm)))	\
			curelm = curelm->field.next;				\
		if (curelm) { 									\
			__res = (elm); 								\
			curelm->field.next = (elm)->field.next;		\
			if ((head)->last == (elm))					\
				(head)->last = curelm;					\
		} 												\
	}													\
	(elm)->field.next = NULL;                                       \
	(__res); \
})


#endif /* _SCCP_LISTS_H */

