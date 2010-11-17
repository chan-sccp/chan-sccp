/*!
 * \file 	sccp_dllists.h
 * \brief 	SCCP Double Linked Lists Header
 * \note	Double Linked List Code derived from Asterisk 1.6.1 "dlinkedlists.h"
 * \author	Sergio Chersovani <mlists [at] c-net.it>
 *		Mark Spencer <markster@digium.com>
 *              Kevin P. Fleming <kpfleming@digium.com>
 * \note  	File is not directly included to get benefit of lists also in previous Asterisk releases (like 1.2)
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef _SCCP_DLLISTS_H
#    define _SCCP_DLLISTS_H

#    include "sccp_lock.h"

/* Main list head */
#    define SCCP_LIST_HEAD(name, type)	\
struct name {						\
	type *first;					\
	type *last;						\
	ast_mutex_t lock;				\
	uint16_t size;					\
}

/* Initialize list head */
#    define SCCP_LIST_HEAD_SET(head, entry) do {	\
	(head)->first = (entry);					\
	(head)->last = (entry);						\
	if(entry)									\
		(head)->size = 1;						\
	ast_mutex_init(&(head)->lock);				\
} while (0)

/* List Item */
#    define SCCP_LIST_ENTRY(type)		\
struct {							\
	type *prev;						\
	type *next;						\
}

/* List First Item */
#    define	SCCP_LIST_FIRST(head)	((head)->first)

/* List Last Item */
#    define	SCCP_LIST_LAST(head)	((head)->last)

/* List Next Item */
#    define SCCP_LIST_NEXT(elm, field)	((elm)->field.next)

/* List Prev Item */
#    define SCCP_LIST_PREV(elm, field)	((elm)->field.prev)

/* List Clear */
#    define	SCCP_LIST_EMPTY(head)	(SCCP_LIST_FIRST(head) == NULL)

/* List Explore Routine */
#    define SCCP_LIST_TRAVERSE(head,var,field) 				\
	for((var) = (head)->first; (var); (var) = (var)->field.next)

/* List Safe Explore Routine */
#    define SCCP_LIST_TRAVERSE_SAFE_BEGIN(head, var, field) {	\
	typeof((head)) __list_head = head;						\
	typeof(__list_head->first) __list_next;					\
	typeof(__list_head->first) __list_prev = NULL;			\
	typeof(__list_head->first) __new_prev = NULL;			\
	for ((var) = __list_head->first, __new_prev = (var),	\
	      __list_next = (var) ? (var)->field.next : NULL;	\
	     (var);												\
	     __list_prev = __new_prev, (var) = __list_next,		\
	     __new_prev = (var),								\
	     __list_next = (var) ? (var)->field.next : NULL		\
	    )

/* Current List Item Removal */
#    define SCCP_LIST_REMOVE_CURRENT(field) do { 				\
	__new_prev->field.next = NULL;							\
	__new_prev->field.prev = NULL;							\
	if (__list_next)										\
		__new_prev = __list_prev;							\
	else													\
		__new_prev = NULL;									\
	if (__list_prev) {                                  	\
		if (__list_next)									\
			__list_next->field.prev = __list_prev;			\
		__list_prev->field.next = __list_next;				\
	} else {                                            	\
		__list_head->first = __list_next;					\
		if (__list_next)									\
			__list_next->field.prev = NULL;					\
	}														\
	if (!__list_next)										\
		__list_head->last = __list_prev; 					\
	__list_head->size--; \
	} while (0)

/* Move Current List Item */
#    define SCCP_LIST_MOVE_CURRENT(newhead, field) do { 		\
	typeof ((newhead)->first) __list_cur = __new_prev;		\
	SCCP_LIST_REMOVE_CURRENT(field);						\
	SCCP_LIST_INSERT_TAIL((newhead), __list_cur, field);	\
	} while (0)

/* Move Current List Item Backward */
#    define SCCP_LIST_MOVE_CURRENT_BACKWARDS(newhead, field) do { 	\
	typeof ((newhead)->first) __list_cur = __new_prev;			\
	if (!__list_next) {											\
		AST_DLLIST_REMOVE_CURRENT(field);						\
		__new_prev = NULL;										\
		AST_DLLIST_INSERT_HEAD((newhead), __list_cur, field);	\
	} else {													\
		AST_DLLIST_REMOVE_CURRENT(field);						\
		AST_DLLIST_INSERT_HEAD((newhead), __list_cur, field);	\
	}} while (0)

/* List Item Insertion before element */
#    define SCCP_LIST_INSERT_BEFORE(head, listelm, elm, field) do {		\
	(elm)->field.next = (listelm);							\
	(elm)->field.prev = (listelm)->field.prev;				\
	if ((listelm)->field.prev)								\
		(listelm)->field.prev->field.next = (elm);			\
	(listelm)->field.prev = (elm);							\
	if ((head)->first == (listelm))							\
		(head)->first = (elm);								\
	(head)->size++;											\
} while (0)

/* List Item Insertion before Current */
#    define SCCP_LIST_INSERT_BEFORE_CURRENT(elm, field) do {	\
	if (__list_prev) {										\
		(elm)->field.next = __list_prev->field.next;		\
		(elm)->field.prev = __list_prev;					\
		if (__list_prev->field.next)                        \
			__list_prev->field.next->field.prev = (elm);	\
		__list_prev->field.next = (elm);					\
	} else {												\
		(elm)->field.next = __list_head->first;				\
		__list_head->first->field.prev = (elm);				\
		(elm)->field.prev = NULL;							\
		__list_head->first = (elm);							\
	}														\
	(head)->size++;											\
} while (0)

/* List Item Insertion before Current Backwards */
#    define SCCP_LIST_INSERT_BEFORE_CURRENT_BACKWARDS(elm, field) do {			\
	if (__list_next) {										\
		(elm)->field.next = __list_next;					\
		(elm)->field.prev = __new_prev;						\
		__new_prev->field.next = (elm);	   					\
		__list_next->field.prev = (elm);	   				\
	} else {												\
		(elm)->field.prev = __list_head->last;				\
		(elm)->field.next = NULL;							\
		__list_head->last->field.next = (elm);				\
		__list_head->last = (elm);							\
	}														\
	(head)->size++;											\
} while (0)

/* List Traverse End (Parentesis) */
#    define SCCP_LIST_TRAVERSE_SAFE_END  }

/* List Backward Explore Routine */
#    define SCCP_LIST_TRAVERSE_BACKWARDS(head,var,field) 		\
	for((var) = (head)->last; (var); (var) = (var)->field.prev)

/* List Safe Backward Explore Routine */
#    define SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_BEGIN(head, var, field) { 	\
	typeof((head)) __list_head = head;					\
	typeof(__list_head->first) __list_next;				\
	typeof(__list_head->first) __list_prev = NULL;		\
	typeof(__list_head->first) __new_prev = NULL;		\
	for ((var) = __list_head->last, __new_prev = (var),	\
			 __list_next = NULL, __list_prev = (var) ? (var)->field.prev : NULL;	\
	     (var);											\
	     __list_next = __new_prev, (var) = __list_prev,	\
	     __new_prev = (var),							\
	     __list_prev = (var) ? (var)->field.prev : NULL	\
	    )

/* List Backward Traverse End (Parentesis) */
#    define SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_END  }

/* List Head Init */
#    define SCCP_LIST_HEAD_INIT(head) {	\
	(head)->first = NULL;			\
	(head)->last = NULL;			\
	ast_mutex_init(&(head)->lock);	\
	(head)->size=0;					\
}
/* List Head Destroy*/
#    define SCCP_LIST_HEAD_DESTROY(head) {	\
	(head)->first = NULL;				\
	(head)->last = NULL;				\
	ast_mutex_destroy(&(head)->lock);	\
	(head)->size=0;						\
}

/* List Item Insertion After */
#    define SCCP_LIST_INSERT_AFTER(head, listelm, elm, field) do {	\
	(elm)->field.next = (listelm)->field.next;					\
	(elm)->field.prev = (listelm);								\
	if ((listelm)->field.next)									\
		(listelm)->field.next->field.prev = (elm);				\
	(listelm)->field.next = (elm);								\
	if ((head)->last == (listelm))								\
		(head)->last = (elm);									\
	(head)->size++;												\
} while (0)

/* List Insertion in Alphanumeric Order */
#    define SCCP_LIST_INSERT_SORTALPHA(head, elm, field, sortfield) do { 	\
	if (!(head)->first) {                                           	\
		(head)->first = (elm);                                      	\
		(head)->last = (elm);                                       	\
	} else {                                                        	\
		typeof((head)->first) cur = (head)->first, prev = NULL;     	\
		while (cur && strcmp(cur->sortfield, elm->sortfield) < 0) { 	\
			prev = cur;                                             	\
			cur = cur->field.next;                                  	\
		}                                                           	\
		if (!prev) {                                                	\
			SCCP_LIST_INSERT_HEAD(head, elm, field);                 	\
		} else if (!cur) {                                          	\
			SCCP_LIST_INSERT_TAIL(head, elm, field);                 	\
		} else {                                                    	\
			SCCP_LIST_INSERT_AFTER(head, prev, elm, field);          	\
		}                                                           	\
	}                                                               	\
	(head)->size++;														\
} while (0)

/* Inserts a list item at the head of a list.*/
#    define SCCP_LIST_INSERT_HEAD(head, elm, field) do {	\
		(elm)->field.next = (head)->first;				\
		if ((head)->first)                          	\
			(head)->first->field.prev = (elm);			\
		(elm)->field.prev = NULL;						\
		(head)->first = (elm);							\
		if (!(head)->last)								\
			(head)->last = (elm);						\
		(head)->size++;									\
} while (0)

/* Inserts a list item at the tail of a list */
#    define SCCP_LIST_INSERT_TAIL(head, elm, field) do {	\
      if (!(head)->first) {								\
		(head)->first = (elm);							\
		(head)->last = (elm);               		    \
		(elm)->field.next = NULL;						\
		(elm)->field.prev = NULL;						\
      } else {											\
		(head)->last->field.next = (elm);				\
		(elm)->field.prev = (head)->last;				\
		(elm)->field.next = NULL;						\
		(head)->last = (elm);							\
      }													\
      (head)->size++;									\
} while (0)

/* Append a whole list to another */
#    define SCCP_LIST_APPEND_LIST(head, list, field) do {	\
      if (!(head)->first) {								\
		(head)->first = (list)->first;					\
		(head)->last = (list)->last;					\
		(head)->size = (list)->size;					\
      } else {											\
		(head)->last->field.next = (list)->first;		\
		(list)->first->field.prev = (head)->last;		\
		(head)->last = (list)->last;					\
		(head)->size += (list)->size;					\
      }													\
      (list)->first = NULL;								\
      (list)->last = NULL;								\
} while (0)

/* Remove the head item from a list giving back a pointer to it. */
#    define SCCP_LIST_REMOVE_HEAD(head, field) ({			\
		typeof((head)->first) cur = (head)->first;		\
		if (cur) {										\
			(head)->first = cur->field.next;			\
			if (cur->field.next)                		\
				cur->field.next->field.prev = NULL;		\
			cur->field.next = NULL;						\
			if ((head)->last == cur)					\
				(head)->last = NULL;					\
		}									\
		(head)->size--;								\
		cur;											\
	})

/* Remove an item from a list */
#    define SCCP_LIST_REMOVE(head, elm, field) ({					\
	__typeof(elm) __res = (elm);								\
	if ((head)->first == (elm)) {								\
		(head)->first = (elm)->field.next;						\
		if ((elm)->field.next)									\
			(elm)->field.next->field.prev = NULL;				\
		if ((head)->last == (elm))								\
			(head)->last = NULL;								\
		(head)->size--;												\
	} else if ((elm)->field.prev != NULL) {													\
		(elm)->field.prev->field.next = (elm)->field.next;		\
		if ((elm)->field.next)									\
			(elm)->field.next->field.prev = (elm)->field.prev;	\
		if ((head)->last == (elm))								\
			(head)->last = (elm)->field.prev;					\
		(head)->size--;												\
	}															\
	(elm)->field.next = NULL;									\
	(elm)->field.prev = NULL;									\
	(__res);													\
})

#endif										/* _SCCP_DLLISTS_H */
