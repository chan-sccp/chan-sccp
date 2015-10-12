/*!
 * \file        sccp_atomic.h
 * \brief       SCCP Atomic Header
 * \author      Diederik de Groot <ddegroot[at] users.sf.net>
 * \note        Mutex lock code derived from Asterisk 1.4
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */
#pragma once

#ifdef HAVE_ATOMIC_OPS_H
#include <atomic_ops.h>
#endif

// Declare CAS32 / CAS_PTR and CAS_TYPE for easy reference in other functions
#ifdef SCCP_ATOMIC

#ifdef SCCP_BUILTIN_INCR
#define ATOMIC_INCR(_a,_b,_c) 		__sync_fetch_and_add(_a,_b)
#define ATOMIC_DECR(_a,_b,_c) 		__sync_fetch_and_add(_a,-_b)
#else
#define ATOMIC_INCR(_a,_b,_c) 		AO_fetch_and_add((volatile size_t *)_a,_b)
#define ATOMIC_DECR(_a,_b,_c) 		AO_fetch_and_add((volatile size_t *)_a,-_b)
#endif

#ifdef SCCP_BUILTIN_CAS32
#define CAS32(_a,_b,_c, _d) 		__sync_val_compare_and_swap(_a, _b, _c)
#define CAS32_TYPE int
#else
#define CAS32(_a,_b,_c, _d) 		AO_compare_and_swap(_a, _b, _c)
#define CAS32_TYPE int
#endif

#ifdef SCCP_BUILTIN_CAS_PTR
#define CAS_PTR(_a,_b,_c, _d) 		__sync_bool_compare_and_swap(_a, _b, _c)
#else
#define CAS_PTR(_a,_b,_c, _d) 		AO_compare_and_swap((uintptr_t *)_a, (uintptr_t)_b, (uintptr_t)_c)
#endif

#else														/* SCCP_ATOMIC */

#define CAS32_TYPE 				int
#define ATOMIC_INCR(_a,_b,_c)			\
	({					\
                CAS32_TYPE res=0;		\
                pbx_mutex_lock(_c);		\
                res = *_a;			\
                *_a += _b;			\
                pbx_mutex_unlock(_c);		\
                res;				\
	})
#define ATOMIC_DECR(_a,_b,_c) 		ATOMIC_INCR(_a,-_b,_c)
#define CAS32(_a,_b,_c,_d)			\
        ({					\
                CAS32_TYPE res=0;		\
                pbx_mutex_lock(_d);		\
                if (*_a == _b) {		\
                        res = _b;		\
                        *_a = _c;		\
                }				\
                pbx_mutex_unlock(_d);		\
                res;				\
        })
#define CAS_PTR(_a,_b,_c,_d) 		\
        ({					\
                boolean_t res = 0;		\
                pbx_mutex_lock(_d);		\
                if (*_a == _b)	{		\
                        res = 1;		\
                        *_a = _c;		\
                }				\
                pbx_mutex_unlock(_d);		\
                res;				\
        })
#endif														/* SCCP_ATOMIC */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
