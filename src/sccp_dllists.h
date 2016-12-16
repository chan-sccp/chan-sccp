/*!
 * \file        sccp_dllists.h
 * \brief       SCCP Double Linked Lists Header
 * \note        Double Linked List Code derived from Asterisk 1.6.1 "dlinkedlists.h"
 * \author      Mark Spencer <markster@digium.com>
 *              Kevin P. Fleming <kpfleming@digium.com>
 * \note        File is not directly included to get benefit of lists also in previous Asterisk releases (like 1.2)
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
#pragma once

/* Lock Macro for Lists */
#define SCCP_LIST_LOCK(x)			pbx_mutex_lock(&(x)->lock)
#define SCCP_LIST_UNLOCK(x)			pbx_mutex_unlock(&(x)->lock)
#define SCCP_LIST_TRYLOCK(x)			pbx_mutex_trylock(&(x)->lock)

/* Lock Macro for read/write Lists */
#define SCCP_RWLIST_RDLOCK(x)			pbx_rwlock_rdlock(&(x)->lock)
#define SCCP_RWLIST_WRLOCK(x)			pbx_rwlock_wrlock(&(x)->lock)
#define SCCP_RWLIST_UNLOCK(x)			pbx_rwlock_unlock(&(x)->lock)
#define SCCP_RWLIST_TRYRDLOCK(x)		pbx_rwlock_tryrdlock(&(x)->lock)
#define SCCP_RWLIST_TRYWRLOCK(x)		pbx_rwlock_trywrlock(&(x)->lock)

/* Main list head */
#define SCCP_LIST_HEAD(name, type)									\
struct name {												\
	pbx_mutex_t lock;										\
	type *first;											\
	type *last;											\
	uint32_t size;											\
}

#define SCCP_RWLIST_HEAD(name, type)									\
struct name {												\
	pbx_rwlock_t lock;										\
	type *first;											\
	type *last;											\
	uint32_t size;											\
}

/* Initialize list head */
#define SCCP_LIST_HEAD_SET(head, entry) do {								\
	(head)->first = (entry);									\
	(head)->last = (entry);										\
	if(entry)											\
		(head)->size = 1;									\
	pbx_mutex_init(&(head)->lock);									\
} while (0)

/* Initialize rwlist head */
#define SCCP_RWLIST_HEAD_SET(head, entry) do {								\
	(head)->first = (entry);									\
	(head)->last = (entry);										\
	if(entry)											\
		(head)->size = 1;									\
	pbx_rwlock_init(&(head)->lock);									\
} while (0)

/* List Item */
#define SCCP_LIST_ENTRY(type)										\
struct {												\
	type *prev;											\
	type *next;											\
}
#define SCCP_RWLIST_ENTRY SCCP_LIST_ENTRY

/* List First Item */
#define SCCP_LIST_FIRST(head)	((head)->first)
#define SCCP_RWLIST_FIRST SCCP_LIST_FIRST

/* List Last Item */
#define SCCP_LIST_LAST(head)	((head)->last)
#define SCCP_RWLIST_LAST SCCP_LIST_LAST

/* List Next Item */
#define SCCP_LIST_NEXT(elm, field)	((elm)->field.next)
#define SCCP_RWLIST_NEXT SCCP_LIST_NEXT

/* List Prev Item */
#define SCCP_LIST_PREV(elm, field)	((elm)->field.prev)
#define SCCP_RWLIST_PREV SCCP_LIST_PREV

/* List Clear */
#define SCCP_LIST_EMPTY(head)	(SCCP_LIST_FIRST(head) == NULL)
#define SCCP_RWLIST_EMPTY SCCP_LIST_EMPTY

/* List Explore Routine */
#define SCCP_LIST_TRAVERSE(head,var,field) 								\
		for((var) = (head)->first; (var); (var) = (var)->field.next)
#define SCCP_RWLIST_TRAVERSE SCCP_LIST_TRAVERSE

/* List Safe Explore Routine */
#define SCCP_LIST_TRAVERSE_SAFE_BEGIN(head, var, field) {						\
	typeof((head)) __list_head = head;								\
	typeof(__list_head->first) __list_next;								\
	typeof(__list_head->first) __list_prev = NULL;							\
	typeof(__list_head->first) __new_prev = NULL;							\
	for ((var) = __list_head->first, __new_prev = (var),						\
	      __list_next = (var) ? (var)->field.next : NULL;						\
	     (var);											\
	     __list_prev = __new_prev, (var) = __list_next,						\
	     __new_prev = (var),									\
	     __list_next = (var) ? (var)->field.next : NULL						\
	    )
#define SCCP_RWLIST_TRAVERSE_SAFE_BEGIN SCCP_LIST_TRAVERSE_SAFE_BEGIN

/* Current List Item Removal */
#define SCCP_LIST_REMOVE_CURRENT(field) do { 								\
	__new_prev->field.next = NULL;									\
	__new_prev->field.prev = NULL;									\
	if (__list_next)										\
		__new_prev = __list_prev;								\
	else												\
		__new_prev = NULL;									\
	if (__list_prev) {                                  						\
		if (__list_next)									\
			__list_next->field.prev = __list_prev;						\
		__list_prev->field.next = __list_next;							\
	} else {                                            						\
		__list_head->first = __list_next;							\
		if (__list_next)									\
			__list_next->field.prev = NULL;							\
	}												\
	if (!__list_next)										\
		__list_head->last = __list_prev; 							\
	__list_head->size--; 										\
} while (0)
#define SCCP_RWLIST_REMOVE_CURRENT SCCP_LIST_REMOVE_CURRENT

/* Move Current List Item */
#define SCCP_LIST_MOVE_CURRENT(newhead, field) do { 							\
	typeof ((newhead)->first) __list_cur = __new_prev;						\
	SCCP_LIST_REMOVE_CURRENT(field);								\
	SCCP_LIST_INSERT_TAIL((newhead), __list_cur, field);						\
	} while (0)
#define SCCP_RWLIST_MOVE_CURRENT SCCP_LIST_MOVE_CURRENT

/* Move Current List Item Backward */
#define SCCP_LIST_MOVE_CURRENT_BACKWARDS(newhead, field) do { 						\
	typeof ((newhead)->first) __list_cur = __new_prev;						\
	if (!__list_next) {										\
		AST_DLLIST_REMOVE_CURRENT(field);							\
		__new_prev = NULL;									\
		AST_DLLIST_INSERT_HEAD((newhead), __list_cur, field);					\
	} else {											\
		AST_DLLIST_REMOVE_CURRENT(field);							\
		AST_DLLIST_INSERT_HEAD((newhead), __list_cur, field);					\
	}} while (0)
#define SCCP_RWLIST_MOVE_CURRENT_BACKWARDS SCCP_LIST_MOVE_CURRENT_BACKWARDS

/* List Item Insertion before element */
#define SCCP_LIST_INSERT_BEFORE(head, listelm, elm, field) do {						\
	(elm)->field.next = (listelm);									\
	(elm)->field.prev = (listelm)->field.prev;							\
	if ((listelm)->field.prev)									\
		(listelm)->field.prev->field.next = (elm);						\
	(listelm)->field.prev = (elm);									\
	if ((head)->first == (listelm))									\
		(head)->first = (elm);									\
	(head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_BEFORE SCCP_LIST_INSERT_BEFORE

/* List Item Insertion before Current */
#define SCCP_LIST_INSERT_BEFORE_CURRENT(elm, field) do {						\
	if (__list_prev) {										\
		(elm)->field.next = __list_prev->field.next;						\
		(elm)->field.prev = __list_prev;							\
		if (__list_prev->field.next)                        					\
			__list_prev->field.next->field.prev = (elm);					\
		__list_prev->field.next = (elm);							\
	} else {											\
		(elm)->field.next = __list_head->first;							\
		__list_head->first->field.prev = (elm);							\
		(elm)->field.prev = NULL;								\
		__list_head->first = (elm);								\
	}												\
	(head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_BEFORE_CURRENT SCCP_LIST_INSERT_BEFORE_CURRENT

/* List Item Insertion before Current Backwards */
#define SCCP_LIST_INSERT_BEFORE_CURRENT_BACKWARDS(elm, field) do {					\
	if (__list_next) {										\
		(elm)->field.next = __list_next;							\
		(elm)->field.prev = __new_prev;								\
		__new_prev->field.next = (elm);	   							\
		__list_next->field.prev = (elm);	   						\
	} else {											\
		(elm)->field.prev = __list_head->last;							\
		(elm)->field.next = NULL;								\
		__list_head->last->field.next = (elm);							\
		__list_head->last = (elm);								\
	}												\
	(head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_BEFORE_CURRENT_BACKWARDS SCCP_LIST_INSERT_BEFORE_CURRENT_BACKWARDS

/* List Traverse End (Parentesis) */
#define SCCP_LIST_TRAVERSE_SAFE_END (void) __list_prev; /* to quiet compiler */ }
#define SCCP_RWLIST_TRAVERSE_SAFE_END SCCP_LIST_TRAVERSE_SAFE_END

/* List Backward Explore Routine */
#define SCCP_LIST_TRAVERSE_BACKWARDS(head,var,field) 							\
	for((var) = (head)->last; (var); (var) = (var)->field.prev)
#define SCCP_RWLIST_TRAVERSE_BACKWARDS SCCP_LIST_TRAVERSE_BACKWARDS

/* List Safe Backward Explore Routine */
#define SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_BEGIN(head, var, field) {					\
	typeof((head)) __list_head = head;								\
	typeof(__list_head->first) __list_prev;								\
	typeof(__list_head->first) __list_next = NULL;							\
	typeof(__list_head->first) __new_next = NULL;							\
	for ((var) = __list_head->last, __new_next = (var),						\
	      __list_prev = (var) ? (var)->field.prev : NULL;						\
	     (var);											\
	     __list_next = __new_next, (var) = __list_prev,						\
	     __new_next = (var),									\
	     __list_prev = (var) ? (var)->field.prev : NULL						\
	    )
#define SCCP_RWLIST_TRAVERSE_BACKWARDS_SAFE_BEGIN SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_BEGIN

/* List Backward Traverse End (Parentesis) */
#define SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_END (void) __list_next; /* to quiet compiler */ } 
#define SCCP_RWLIST_TRAVERSE_BACKWARDS_SAFE_END  SCCP_LIST_TRAVERSE_BACKWARDS_SAFE_END

/* List Head Init */
#define SCCP_LIST_HEAD_INIT(head) {									\
	(head)->first = NULL;										\
	(head)->last = NULL;										\
	pbx_mutex_init_notracking(&(head)->lock);							\
	(head)->size=0;											\
}
#define SCCP_RWLIST_HEAD_INIT(head) {									\
	(head)->first = NULL;										\
	(head)->last = NULL;										\
	pbx_rwlock_init_notracking(&(head)->lock);							\
	(head)->size=0;											\
}

/* List Head Destroy */
#define SCCP_LIST_HEAD_DESTROY(head) {									\
	(head)->first = NULL;										\
	(head)->last = NULL;										\
	pbx_mutex_destroy(&(head)->lock);								\
	(head)->size=0;											\
}
#define SCCP_RWLIST_HEAD_DESTROY(head) {								\
	(head)->first = NULL;										\
	(head)->last = NULL;										\
	pbx_rwlock_destroy(&(head)->lock);								\
	(head)->size=0;											\
}

/* List Item Insertion After */
#define SCCP_LIST_INSERT_AFTER(head, listelm, elm, field) do {						\
	(elm)->field.next = (listelm)->field.next;							\
	(elm)->field.prev = (listelm);									\
	if ((listelm)->field.next)									\
		(listelm)->field.next->field.prev = (elm);						\
	(listelm)->field.next = (elm);									\
	if ((head)->last == (listelm))									\
		(head)->last = (elm);									\
	(head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_AFTER SCCP_LIST_INSERT_AFTER

/* List Insertion in Alphanumeric Order */
#define SCCP_LIST_INSERT_SORTALPHA(head, elm, field, sortfield) do { 					\
	if (!(head)->first) {                                           				\
		(head)->first = (elm);                                      				\
		(head)->last = (elm);                                       				\
	} else {                                                        				\
		typeof((head)->first) cur = (head)->first, prev = NULL;     				\
		while (cur && sccp_strversioncmp(cur->sortfield, elm->sortfield) < 0) { 		\
			prev = cur;                                            				\
			cur = cur->field.next;                                 				\
		}                                                           				\
		if (!prev) {                                                				\
			SCCP_LIST_INSERT_HEAD(head, elm, field);               				\
		} else if (!cur) {                                          				\
			SCCP_LIST_INSERT_TAIL(head, elm, field);               				\
		} else {                                                    				\
			SCCP_LIST_INSERT_AFTER(head, prev, elm, field);        				\
		}                                                           				\
	}                                                               				\
	(head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_SORTALPHA SCCP_LIST_INSERT_SORTALPHA

/* Inserts a list item at the head of a list. */
#define SCCP_LIST_INSERT_HEAD(head, elm, field) do {							\
		(elm)->field.next = (head)->first;							\
		if ((head)->first)                          						\
			(head)->first->field.prev = (elm);						\
		(elm)->field.prev = NULL;								\
		(head)->first = (elm);									\
		if (!(head)->last)									\
			(head)->last = (elm);								\
		(head)->size++;										\
} while (0)
#define SCCP_RWLIST_INSERT_HEAD SCCP_LIST_INSERT_HEAD

/* Inserts a list item at the tail of a list */
#define SCCP_LIST_INSERT_TAIL(head, elm, field) do {							\
      if (!(head)->first) {										\
		(head)->first = (elm);									\
		(head)->last = (elm);									\
		(elm)->field.next = NULL;								\
		(elm)->field.prev = NULL;								\
      } else {												\
		(head)->last->field.next = (elm);							\
		(elm)->field.prev = (head)->last;							\
		(elm)->field.next = NULL;								\
		(head)->last = (elm);									\
      }													\
      (head)->size++;											\
} while (0)
#define SCCP_RWLIST_INSERT_TAIL SCCP_LIST_INSERT_TAIL

/* Append a whole list to another */
#define SCCP_LIST_APPEND_LIST(head, list, field) do {							\
      if (!(head)->first) {										\
		(head)->first = (list)->first;								\
		(head)->last = (list)->last;								\
		(head)->size = (list)->size;								\
      } else {												\
		(head)->last->field.next = (list)->first;						\
		(list)->first->field.prev = (head)->last;						\
		(head)->last = (list)->last;								\
		(head)->size += (list)->size;								\
      }													\
      (list)->first = NULL;										\
      (list)->last = NULL;										\
} while (0)
#define SCCP_RWLIST_APPEND_LIST SCCP_LIST_APPEND_LIST

/* Remove the head item from a list giving back a pointer to it. */
#define SCCP_LIST_REMOVE_HEAD(head, field) ({								\
		typeof((head)->first) cur = (head)->first;						\
		if (cur) {										\
			(head)->first = cur->field.next;						\
			if (cur->field.next)                						\
				cur->field.next->field.prev = NULL;					\
			cur->field.next = NULL;								\
			if ((head)->last == cur)							\
				(head)->last = NULL;							\
        		(head)->size--;									\
		}											\
		cur;											\
	})
#define SCCP_RWLIST_REMOVE_HEAD SCCP_LIST_REMOVE_HEAD

/* Remove an item from a list */
#define SCCP_LIST_REMOVE(head, elm, field) ({								\
	__typeof(elm) __res = (elm);									\
	if ((head)->first == (elm)) {									\
		(head)->first = (elm)->field.next;							\
		if ((elm)->field.next)									\
			(elm)->field.next->field.prev = NULL;						\
		if ((head)->last == (elm))								\
			(head)->last = NULL;								\
		(head)->size--;										\
	} else if ((elm)->field.prev != NULL) {								\
		(elm)->field.prev->field.next = (elm)->field.next;					\
		if ((elm)->field.next)									\
			(elm)->field.next->field.prev = (elm)->field.prev;				\
		if ((head)->last == (elm))								\
			(head)->last = (elm)->field.prev;						\
		(head)->size--;										\
	}												\
	(elm)->field.next = NULL;									\
	(elm)->field.prev = NULL;									\
	(__res);											\
})
#define SCCP_RWLIST_REMOVE SCCP_LIST_REMOVE

/* Expensive SCCP_LIST_FIND version: only used during refcount issue finding */
/*
#define SCCP_LIST_FIND(_head, _type, _var, _field, _compare, _retain, _file, _line, _func) ({		\
	_type *_var;											\
	_type *__tmp_##_var##_line;									\
	for((_var) = (_head)->first; (_var); (_var) = (_var)->_field.next) {				\
		__tmp_##_var##_line = sccp_refcount_retain((_var), _file, _line, _func);		\
	        if (__tmp_##_var##_line) {								\
	        	if (_compare) {									\
	        		if (!_retain) {								\
		 		        sccp_refcount_release(__tmp_##_var##_line, _file, _line, _func);\
	        		}									\
				break;									\
		        }										\
 		        sccp_refcount_release(__tmp_##_var##_line, _file, _line, _func);		\
		} else {										\
			pbx_log(LOG_ERROR, "SCCP (%s:%d:%s): Failed to get reference to variable during SCCP_LIST_FIND\n", _file, _line, _func);\
			(_var) = NULL;									\
		}											\
	}                                                                                               \
	(_var);												\
})
*/
#define SCCP_LIST_FIND(_head, _type, _var, _field, _compare, _retain, _file, _line, _func) ({		\
	_type *_var;											\
	for((_var) = (_head)->first; (_var); (_var) = (_var)->_field.next) {				\
		if (_compare) {										\
			if (_retain) {									\
				sccp_refcount_retain((_var), _file, _line, _func);			\
			}										\
			break;										\
		}											\
	}                                                                                               \
	(_var);												\
})

#define SCCP_RWLIST_FIND SCCP_LIST_FIND

#define SCCP_LIST_GETSIZE(head) (head)->size
#define SCCP_RWLIST_GETSIZE SCCP_LIST_GETSIZE
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
