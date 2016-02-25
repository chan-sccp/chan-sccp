/*!
 * \file	sccp_atomic.h
 * \brief	SCCP Atomic Header
 * \author	Diederik de Groot <ddegroot[at] users.sf.net>
 * \note	Mutex lock code derived from Asterisk 1.4
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 */
#pragma once

#ifdef HAVE_ATOMIC_OPS_H
#include <atomic_ops.h>
#endif

// Declare CAS32 / CAS_PTR and CAS_TYPE for easy reference in other functions
#ifdef SCCP_ATOMIC

#ifdef SCCP_BUILTIN_INCR
#define ATOMIC_INCR(_a,_b,_c) 		__sync_fetch_and_add(_a, _b)
#define ATOMIC_DECR(_a,_b,_c) 		__sync_fetch_and_add(_a, -_b)
#define ATOMIC_FETCH(_a,_c) 		__sync_fetch_and_add(_a, 0)
#else
#define ATOMIC_INCR(_a,_b,_c) 		AO_fetch_and_add((volatile size_t *)_a, _b)
#define ATOMIC_DECR(_a,_b,_c) 		AO_fetch_and_add((volatile size_t *)_a, -_b)
#define ATOMIC_FETCH(_a,_c)		AO_fetch_and_add((volatile size_t *)_a, 0)
#endif

#ifdef SCCP_BUILTIN_CAS32
#define CAS32(_a,_b,_c, _d) 		__sync_val_compare_and_swap(_a, _b, _c)
#else
#define CAS32(_a,_b,_c, _d) 		AO_compare_and_swap(_a, _b, _c)
#endif

#ifdef SCCP_BUILTIN_CAS_PTR
#define CAS_PTR(_a,_b,_c, _d) 		__sync_bool_compare_and_swap(_a, _b, _c)
#else
#define CAS_PTR(_a,_b,_c, _d) 		AO_compare_and_swap((uintptr_t *)_a, (uintptr_t)_b, (uintptr_t)_c)
#endif

#else														/* SCCP_ATOMIC */
//#define CAS32_TYPE			int
#if defined (__i386__) || defined(__x86_64__)
#define ATOMIC_INCR(_a,_b,_c)	 	ast_atomic_fetchadd_int(_a, _b)
#define ATOMIC_DECR(_a,_b,_c)	 	ast_atomic_fetchadd_int(_a, -_b)
#define ATOMIC_FETCH(_a,_c)		ast_atomic_fetchadd_int(_a, 0)
#else
#define ATOMIC_INCR(_a,_b,_c)							\
	({									\
		CAS32_TYPE __res=0;						\
		int __tries = 0;						\
		int locked = 1;							\
		while (pbx_mutex_trylock(_c) != 0) {				\
			pbx_log(LOG_NOTICE, "SCCP: atomic try lock failed, %d times\n", __tries);	\
			usleep(100);						\
			sched_yield();						\
			/* cheating, processing unlocked after 10 tries, to prevent deadlock */	\
			if (__tries++ > 10) {locked = 0;break;}			\
		};								\
		__res = *_a;							\
		*_a += _b;							\
		if (locked) {pbx_mutex_unlock(_c);}				\
		__res;								\
	})
#define ATOMIC_DECR(_a,_b,_c) 		ATOMIC_INCR(_a,-_b,_c)
#define ATOMIC_FETCH(_a,_c) 		ATOMIC_INCR(_a,0,_c)
#endif
#define CAS32(_a,_b,_c,_d)							\
	({									\
		CAS32_TYPE __res=-298;						\
		if (pbx_mutex_trylock(_d) != 0) {				\
			pbx_log(LOG_NOTICE, "SCCP: atomic cas32 try lock failed\n");\
			sched_yield();						\
			return;							\
		};								\
		__res = *_a;							\
		if (__res == _b) {						\
			*_a = _c;						\
		}								\
		pbx_mutex_unlock(_d);						\
		__res;								\
	})
#define CAS_PTR(_a,_b,_c,_d) 							\
	({									\
		boolean_t __res = FALSE;						\
		if (pbx_mutex_trylock(_d) != 0) {				\
			pbx_log(LOG_NOTICE, "SCCP: atomic casptr try lock failed\n");\
			sched_yield();						\
			return;							\
		};								\
		if (*_a == _b)	{						\
			__res = TRUE;						\
			*_a = _c;						\
		}								\
		pbx_mutex_unlock(_d);						\
		__res;								\
	})
#endif														/* SCCP_ATOMIC */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
