/*!
 * \file	sccp_vector.h
 * \brief       SCCP Vector Header
 * \note	Vector Code derived from Asterisk 12 "vector.h"
 * 		Copyright (C) 2013, Digium, Inc.
 * \author 	David M. Lee, II <dlee@digium.com>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#pragma once

/*! \file
 *
 * \brief Vector container support.
 *
 * A vector is a variable length array, with properties that can be useful when
 * order doesn't matter.
 *  - Appends are asymptotically constant time.
 *  - Unordered removes are constant time.
 *  - Search is linear time
 *
 * \author David M. Lee, II <dlee@digium.com>
 * \since 12
 */

/*!
 * \brief Define a vector structure
 *
 * \param name Optional vector struct name.
 * \param type Vector element type.
 */
#define SCCP_VECTOR(name, type)										\
	struct name {											\
		type *elems;										\
		size_t max;										\
		size_t current;										\
	}

/*!
 * \brief Define a vector structure with a read/write lock
 *
 * \param name Optional vector struct name.
 * \param type Vector element type.
 */
#define SCCP_VECTOR_RW(name, type)									\
	struct name {											\
		type *elems;										\
		size_t max;	  									\
		size_t current;										\
		pbx_rwlock_t lock;									\
	}

/*!
 * \brief Initialize a vector
 *
 * If \a size is 0, then no space will be allocated until the vector is
 * appended to.
 *
 * \param vec Vector to initialize.
 * \param size Initial size of the vector.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 */
#define SCCP_VECTOR_INIT(vec, size) ({									\
	size_t __size = (size);										\
	size_t alloc_size = __size * sizeof(*((vec)->elems));						\
	(vec)->elems = alloc_size ? ast_calloc(1, alloc_size) : NULL;					\
	(vec)->current = 0;										\
	if ((vec)->elems) {										\
		(vec)->max = __size;									\
	} else {											\
		(vec)->max = 0;										\
	}												\
	(alloc_size == 0 || (vec)->elems != NULL) ? 0 : -1;						\
})

/*!
 * \brief Initialize a vector with a read/write lock
 *
 * If \a size is 0, then no space will be allocated until the vector is
 * appended to.
 *
 * \param vec Vector to initialize.
 * \param size Initial size of the vector.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 */
#define SCCP_VECTOR_RW_INIT(vec, size) ({								\
	int __sccp_vector_res = -1;									\
	if (SCCP_VECTOR_INIT(vec, size) == 0) {								\
		__sccp_vector_res = pbx_rwlock_init(&(vec)->lock);					\
	}												\
	__sccp_vector_res;										\
})

/*!
 * \brief Deallocates this vector.
 *
 * If any code to free the elements of this vector needs to be run, that should
 * be done prior to this call.
 *
 * \param vec Vector to deallocate.
 */
#define SCCP_VECTOR_FREE(vec) do {									\
	ast_free((vec)->elems);										\
	(vec)->elems = NULL;										\
	(vec)->max = 0;											\
	(vec)->current = 0;										\
} while (0)

/*!
 * \brief Deallocates this vector pointer.
 *
 * If any code to free the elements of this vector need to be run, that should
 * be done prior to this call.
 *
 * \param vec Pointer to a malloc'd vector structure.
 */
#define SCCP_VECTOR_PTR_FREE(vec) do {									\
	SCCP_VECTOR_FREE(vec);										\
	ast_free(vec);											\
} while (0)

/*!
 * \brief Deallocates this locked vector
 *
 * If any code to free the elements of this vector need to be run, that should
 * be done prior to this call.
 *
 * \param vec Vector to deallocate.
 */
#define SCCP_VECTOR_RW_FREE(vec) do {									\
	SCCP_VECTOR_FREE(vec);										\
	pbx_rwlock_destroy(&(vec)->lock);								\
} while(0)

/*!
 * \brief Deallocates this locked vector pointer.
 *
 * If any code to free the elements of this vector need to be run, that should
 * be done prior to this call.
 *
 * \param vec Pointer to a malloc'd vector structure.
 */
#define SCCP_VECTOR_RW_PTR_FREE(vec) do {								\
	SCCP_VECTOR_RW_FREE(vec);									\
	ast_free(vec);											\
} while(0)

/*!
 * \internal
 */
#define __sccp_make_room(idx, vec) ({ 										\
	int __sccp_vector_res1 = 0;										\
	/*sccp_log(DEBUGCAT_NEWCODE)("SCCP: make_room\n");*/							\
	do {													\
		if ((vec)->elems && (idx) >= (vec)->max) {							\
			size_t new_max = ((idx) + 1) * 2;							\
			typeof((vec)->elems) new_elems = ast_calloc(1,new_max * sizeof(*new_elems));		\
			if (new_elems) {									\
				if((vec)->elems) {								\
					memcpy(new_elems, (vec)->elems,(vec)->current * sizeof(*new_elems)); 	\
					ast_free((vec)->elems);							\
				}										\
				(vec)->elems = new_elems;							\
				(vec)->max = new_max;								\
			} else {										\
				__sccp_vector_res1 = -1;							\
				break;										\
			}											\
		}												\
	} while(0);												\
	__sccp_vector_res1;											\
})

/*!
 * \brief Append an element to a vector, growing the vector if needed.
 *
 * \param vec Vector to append to.
 * \param elem Element to append.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 */
#define SCCP_VECTOR_APPEND(vec, elem) ({								\
	int __sccp_vector_res = 0;									\
	do {												\
		if (__sccp_make_room((vec)->current, vec) != 0) { 					\
			__sccp_vector_res = -1;								\
			break;										\
		} 											\
		if ((vec)->elems) {									\
			/*sccp_log(DEBUGCAT_NEWCODE)("SCCP: vector_append\n");*/			\
			(vec)->elems[(vec)->current++] = (elem);					\
		}											\
	} while (0);											\
	__sccp_vector_res;										\
})

/*!
 * \brief Replace an element at a specific position in a vector, growing the vector if needed.
 *
 * \param vec Vector to replace into.
 * \param idx Position to replace.
 * \param elem Element to replace.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 *
 * \warning This macro will overwrite anything already present at the position provided.
 *
 * \warning Use of this macro with the expectation that the element will remain at the provided
 * index means you can not use the UNORDERED assortment of macros. These macros alter the ordering
 * of the vector itself.
 */
#define SCCP_VECTOR_REPLACE(vec, idx, elem) ({								\
	int __sccp_vector_res = 0;									\
	do {												\
		if (__sccp_make_room((idx), vec) != 0) {						\
			__sccp_vector_res = -1;								\
			break;										\
		}											\
		(vec)->elems[(idx)] = (elem);								\
		if (((idx) + 1) > (vec)->current) {							\
			(vec)->current = (idx) + 1;							\
		}											\
	} while(0);											\
	__sccp_vector_res;										\
})

/*!
 * \brief Insert an element at a specific position in a vector, growing the vector if needed.
 *
 * \param vec Vector to insert into.
 * \param idx Position to insert at.
 * \param elem Element to insert.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 *
 * \warning This macro will shift existing elements right to make room for the new element.
 *
 * \warning Use of this macro with the expectation that the element will remain at the provided
 * index means you can not use the UNORDERED assortment of macros. These macros alter the ordering
 * of the vector itself.
 */
#define SCCP_VECTOR_INSERT_AT(vec, idx, elem) ({ 							\
	int __sccp_vector_res = 0;									\
	size_t __move;											\
	do {												\
		if (__sccp_make_room(((idx) > (vec)->current ? (idx) : (vec)->current), vec) != 0) {	\
			__sccp_vector_res = -1;								\
			break;										\
		}											\
		if ((vec)->current > 0 && (idx) < (vec)->current) {					\
			__move = ((vec)->current - (idx)) * sizeof(typeof((vec)->elems[0]));		\
			memmove(&(vec)->elems[(idx) + 1], &(vec)->elems[(idx)], __move);		\
		}											\
		(vec)->elems[(idx)] = (elem);								\
		(vec)->current = ((idx) > (vec)->current ? (idx) : (vec)->current) + 1;			\
	} while (0);											\
	__sccp_vector_res;										\
})

/*!
 * \brief Add an element into a sorted vector
 *
 * \param vec Sorted vector to add to.
 * \param elem Element to insert.
 * \param cmp A strcmp compatible compare function.
 *
 * \return 0 on success.
 * \return Non-zero on failure.
 *
 * \warning Use of this macro on an unsorted vector will produce unpredictable results
 */
#define SCCP_VECTOR_ADD_SORTED(vec, elem, cmp) ({							\
	int __sccp_vector_res = 0;									\
	size_t __sccp_vector_idx = (vec)->current;							\
	do {												\
		if (__sccp_make_room((vec)->current, vec) != 0) {					\
			__sccp_vector_res = -1;								\
			break;										\
		}											\
		while (__idx > 0 && (cmp((vec)->elems[__sccp_vector_idx - 1], elem) > 0)) {		\
			(vec)->elems[__idx] = (vec)->elems[__sccp_vector_idx - 1];			\
			__idx--;									\
		}											\
		(vec)->elems[__sccp_vector_idx] = elem;							\
		(vec)->current++;									\
	} while (0);											\
	__sccp_vector_res;										\
})

/*!
 * \brief Remove an element from a vector by index.
 *
 * Note that elements in the vector may be reordered, so that the remove can
 * happen in constant time.
 *
 * \param vec Vector to remove from.
 * \param idx Index of the element to remove.
 * \param preserve_order Preserve the vector order.
 *
 * \return The element that was removed.
 */
#define SCCP_VECTOR_REMOVE(vec, idx, preserve_order) ({							\
	typeof((vec)->elems[0]) __sccp_vector_res1;							\
	size_t __sccp_vector_idx1 = (idx);								\
	ast_assert(__sccp_vector_idx1 < (vec)->current);						\
	__sccp_vector_res1 = (vec)->elems[__sccp_vector_idx1];						\
	if ((preserve_order)) {									\
		size_t __move;										\
		__move = ((vec)->current - (__sccp_vector_idx1) - 1) * sizeof(typeof((vec)->elems[0]));	\
		memmove(&(vec)->elems[__sccp_vector_idx1], &(vec)->elems[__sccp_vector_idx1 + 1], __move);\
		(vec)->current--;									\
	} else {											\
		(vec)->elems[__sccp_vector_idx1] = (vec)->elems[--(vec)->current];			\
	};												\
	__sccp_vector_res1;										\
})

/*!
 * \brief Remove an element from an unordered vector by index.
 *
 * Note that elements in the vector may be reordered, so that the remove can
 * happen in constant time.
 *
 * \param vec Vector to remove from.
 * \param idx Index of the element to remove.
 * \return The element that was removed.
 */
#define SCCP_VECTOR_REMOVE_UNORDERED(vec, idx)								\
	SCCP_VECTOR_REMOVE(vec, idx, 0)

/*!
 * \brief Remove an element from a vector by index while maintaining order.
 *
 * \param vec Vector to remove from.
 * \param idx Index of the element to remove.
 * \return The element that was removed.
 */
#define SCCP_VECTOR_REMOVE_ORDERED(vec, idx)								\
	SCCP_VECTOR_REMOVE(vec, idx, 1)

/*!
 * \brief Remove an element from a vector that matches the given comparison
 *
 * \param vec Vector to remove from.
 * \param value Value to pass into comparator.
 * \param cmp Comparator function/macros (called as \c cmp(elem, value))
 * \param cleanup How to cleanup a removed element macro/function.
 *
 * \return 0 if element was removed.
 * \return Non-zero if element was not in the vector.
 */
#define SCCP_VECTOR_REMOVE_CMP_UNORDERED(vec, value, cmp, cleanup) ({					\
	int __sccp_vector_res = -1;									\
	size_t __sccp_vector_idx;									\
	typeof(value) __value = (value);								\
	for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; ++__sccp_vector_idx) {		\
		if (cmp((vec)->elems[__sccp_vector_idx], __value)) {					\
			cleanup((vec)->elems[__sccp_vector_idx]);					\
			SCCP_VECTOR_REMOVE_UNORDERED((vec), __sccp_vector_idx);				\
			__sccp_vector_res = 0;								\
			break;										\
		}											\
	}												\
	__sccp_vector_res;										\
})

/*!
 * \brief Remove an element from a vector that matches the given comparison while maintaining order
 *
 * \param vec Vector to remove from.
 * \param value Value to pass into comparator.
 * \param cmp Comparator function/macros (called as \c cmp(elem, value))
 * \param cleanup How to cleanup a removed element macro/function.
 *
 * \return 0 if element was removed.
 * \return Non-zero if element was not in the vector.
 */
#define SCCP_VECTOR_REMOVE_CMP_ORDERED(vec, value, cmp, cleanup) ({					\
	int __sccp_vector_res = -1;									\
	size_t __sccp_vector_idx;									\
	typeof(value) __value = (value);								\
	for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; ++__sccp_vector_idx) {		\
		if (cmp((vec)->elems[__sccp_vector_idx], __value)) {					\
			cleanup((vec)->elems[__sccp_vector_idx]);					\
			SCCP_VECTOR_REMOVE_ORDERED((vec), __sccp_vector_idx);				\
			__sccp_vector_res = 0;								\
			break;										\
		}											\
	}												\
	__sccp_vector_res;										\
})

/*!
 * \brief Default comparator for SCCP_VECTOR_REMOVE_ELEM_UNORDERED()
 *
 * \param elem Element to compare against
 * \param value Value to compare with the vector element.
 *
 * \return 0 if element does not match.
 * \return Non-zero if element matches.
 */
#define SCCP_VECTOR_ELEM_DEFAULT_CMP(elem, value) ((elem) == (value))

/*!
 * \brief Vector element cleanup that does nothing.
 *
 * \param elem Element to cleanup
 *
 * \return Nothing
 */
#define SCCP_VECTOR_ELEM_CLEANUP_NOOP(elem)

/*!
 * \brief Remove an element from a vector.
 *
 * \param vec Vector to remove from.
 * \param elem Element to remove
 * \param cleanup How to cleanup a removed element macro/function.
 *
 * \return 0 if element was removed.
 * \return Non-zero if element was not in the vector.
 */
#define SCCP_VECTOR_REMOVE_ELEM_UNORDERED(vec, elem, cleanup) ({					\
	SCCP_VECTOR_REMOVE_CMP_UNORDERED((vec), (elem),							\
		SCCP_VECTOR_ELEM_DEFAULT_CMP, cleanup);							\
})

/*!
 * \brief Remove an element from a vector while maintaining order.
 *
 * \param vec Vector to remove from.
 * \param elem Element to remove
 * \param cleanup How to cleanup a removed element macro/function.
 *
 * \return 0 if element was removed.
 * \return Non-zero if element was not in the vector.
 */
#define SCCP_VECTOR_REMOVE_ELEM_ORDERED(vec, elem, cleanup) ({						\
	SCCP_VECTOR_REMOVE_CMP_ORDERED((vec), (elem),							\
		SCCP_VECTOR_ELEM_DEFAULT_CMP, cleanup);							\
})

/*!
 * \brief Get the number of elements in a vector.
 *
 * \param vec Vector to query.
 * \return Number of elements in the vector.
 */
#define SCCP_VECTOR_SIZE(vec) (vec)->current

/*!
 * \brief Reset vector.
 *
 * \param vec Vector to reset.
 * \param cleanup A cleanup callback or SCCP_VECTOR_ELEM_CLEANUP_NOOP.
 */
#define SCCP_VECTOR_RESET(vec, cleanup) ({								\
	SCCP_VECTOR_CALLBACK_VOID(vec, cleanup);							\
	(vec)->current = 0;										\
})

/*!
 * \brief Get an address of element in a vector.
 *
 * \param vec Vector to query.
 * \param idx Index of the element to get address of.
 */
#define SCCP_VECTOR_GET_ADDR(vec, idx) ({								\
	size_t __sccp_vector_idx = (idx);								\
	ast_assert(__sccp_vector_idx < (vec)->current);							\
	&(vec)->elems[__sccp_vector_idx];								\
})

/*!
 * \brief Get an element from a vector.
 *
 * \param vec Vector to query.
 * \param idx Index of the element to get.
 */
#define SCCP_VECTOR_GET(vec, idx) ({									\
	size_t __sccp_vector_idx = (idx);								\
	ast_assert(__sccp_vector_idx < (vec)->current);							\
	(vec)->elems[__sccp_vector_idx];								\
})

/*!
 * \brief Get an element from a vector that matches the given comparison
 *
 * \param vec Vector to get from.
 * \param value Value to pass into comparator.
 * \param cmp Comparator function/macros (called as \c cmp(elem, value))
 *
 * \return a pointer to the element that was found or NULL
 */
#define SCCP_VECTOR_GET_CMP(vec, value, cmp) ({								\
	void *__sccp_vector_res = NULL;									\
	size_t __sccp_vector_idx;									\
	typeof(value) __value = (value);								\
	for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; ++__sccp_vector_idx) {		\
		if (cmp((vec)->elems[__sccp_vector_idx], __value)) {					\
			__sccp_vector_res = &(vec)->elems[__sccp_vector_idx];				\
			break;										\
		}											\
	}												\
	__sccp_vector_res;										\
})

/*!
 * \brief Default callback for SCCP_VECTOR_CALLBACK()
 *
 * \param element Element to compare against
 * param value Value to compare with the vector element.
 *
 * \return CMP_MATCH always.
 */
#define SCCP_VECTOR_MATCH_ALL(element) (CMP_MATCH)


/*!
 * \brief Execute a callback on every element in a vector returning the first matched
 *
 * \param vec Vector to operate on.
 * \param callback A callback that takes at least 1 argument (the element)
 * plus number of optional arguments
 * \param default_value A default value to return if no elements matched
 *
 * \return the first element matched before CMP_STOP was returned
 * or the end of the vector was reached. Otherwise, default_value
 */
#define SCCP_VECTOR_CALLBACK(vec, callback, default_value, ...) ({					\
	size_t __sccp_vector_idx;									\
	typeof((vec)->elems[0]) __sccp_vector_res = default_value;					\
	for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; __sccp_vector_idx++) {		\
		int rc = callback((vec)->elems[__sccp_vector_idx], ##__VA_ARGS__);			\
		if (rc & CMP_MATCH) {									\
			__sccp_vector_res = (vec)->elems[__sccp_vector_idx];				\
			break;										\
		}											\
		if (rc & CMP_STOP) {									\
			break;										\
		}											\
	}												\
	__sccp_vector_res;										\
})

/*!
 * \brief Execute a callback on every element in a vector returning the matching
 * elements in a new vector
 *
 * This macro basically provides a filtered clone.
 *
 * \param vec Vector to operate on.
 * \param callback A callback that takes at least 1 argument (the element)
 * plus number of optional arguments
 *
 * \return a vector containing the elements matched before CMP_STOP was returned
 * or the end of the vector was reached. The vector may be empty and could be NULL
 * if there was not enough memory to allocate it's control structure.
 *
 * \warning The returned vector must have SCCP_VECTOR_PTR_FREE()
 * called on it after you've finished with it.
 *
 * \note The type of the returned vector must be traceable to the original vector.
 *
 * The following will resut in "error: assignment from incompatible pointer type"
 * because these declare 2 different structures.
 *
 * \code
 * SCCP_VECTOR(, char *) vector_1;
 * SCCP_VECTOR(, char *) *vector_2;
 *
 * vector_2 = SCCP_VECTOR_CALLBACK_MULTIPLE(&vector_1, callback);
 * \endcode
 *
 * This will work because you're using the type of the first
 * to declare the second:
 *
 * \code
 * SCCP_VECTOR(mytype, char *) vector_1;
 * struct mytype *vector_2 = NULL;
 *
 * vector_2 = SCCP_VECTOR_CALLBACK_MULTIPLE(&vector_1, callback);
 * \endcode
 *
 * This will also work because you're declaring both vector_1 and
 * vector_2 from the same definition.
 *
 * \code
 * SCCP_VECTOR(, char *) vector_1, *vector_2 = NULL;
 *
 * vector_2 = SCCP_VECTOR_CALLBACK_MULTIPLE(&vector_1, callback);
 * \endcode
 */
#define SCCP_VECTOR_CALLBACK_MULTIPLE(vec, callback, ...) ({						\
	size_t __sccp_vector_idx;									\
	typeof((vec)) new_vec = NULL;									\
	do {												\
		new_vec = ast_malloc(sizeof(*new_vec));							\
		if (!new_vec) {										\
			break;										\
		}											\
		if (SCCP_VECTOR_INIT(new_vec, SCCP_VECTOR_SIZE((vec))) != 0) {				\
			ast_free(new_vec);								\
			new_vec = NULL;									\
			break;										\
		}											\
		for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; __sccp_vector_idx++) {	\
			int rc = callback((vec)->elems[__sccp_vector_idx], ##__VA_ARGS__);		\
			if (rc & CMP_MATCH) {								\
				SCCP_VECTOR_APPEND(new_vec, (vec)->elems[__sccp_vector_idx]);		\
			}										\
			if (rc & CMP_STOP) {								\
				break;									\
			}										\
		}											\
	} while(0);											\
	new_vec;											\
})

/*!
 * \brief Execute a callback on every element in a vector disregarding callback return
 *
 * \param vec Vector to operate on.
 * \param callback A callback that takes at least 1 argument (the element)
 * plus number of optional arguments
 */
#define SCCP_VECTOR_CALLBACK_VOID(vec, callback, ...) ({						\
	size_t __sccp_vector_idx;									\
	for (__sccp_vector_idx = 0; __sccp_vector_idx < (vec)->current; __sccp_vector_idx++) {		\
		callback((vec)->elems[__sccp_vector_idx], ##__VA_ARGS__);				\
	}					\
})

#define SCCP_VECTOR_RW_RDLOCK(vec) pbx_rwlock_rdlock(&(vec)->lock)
#define SCCP_VECTOR_RW_WRLOCK(vec) pbx_rwlock_wrlock(&(vec)->lock)
#define SCCP_VECTOR_RW_UNLOCK(vec) pbx_rwlock_unlock(&(vec)->lock)
#define SCCP_VECTOR_RW_RDLOCK_TRY(vec) pbx_rwlock_tryrdlock(&(vec)->lock)
#define SCCP_VECTOR_RW_WRLOCK_TRY(vec) pbx_rwlock_trywrlock(&(vec)->lock)
#define SCCP_VECTOR_RW_RDLOCK_TIMED(vec, timespec) pbx_rwlock_timedrdlock(&(vec)->lock, timespec)
#define SCCP_VECTOR_RW_WRLOCK_TIMED(vec, timespec) pbx_rwlock_timedwrlock(&(vec)->lock, timespec)


// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
