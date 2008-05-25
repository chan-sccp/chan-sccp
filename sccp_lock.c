/*
 * (SCCP*)
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * Sergio Chersovani (mlists@c-net.it)
 *
 * Reworked, but based on chan_sccp code.
 * The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 * Modified by Jan Czmok and Julien Goodwin
 *
 * Mutex lock code derived from Asterisk 1.4 adapted by Federico Santulli
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */

#include "config.h"
#ifndef ASTERISK_CONF_1_2
#include <asterisk.h>
#endif
#include "chan_sccp.h"
#include "sccp_lock.h"
#include "sccp_device.h"
#include <asterisk/lock.h>
#ifndef CS_AST_HAS_TECH_PVT
#include <asterisk/_pvt.h>
#endif

#ifdef CS_AST_DEBUG_CHANNEL_LOCKS
#undef CS_LOCKS_DEBUG_ALL
/*! \brief Unlock AST MUTEX (and print debugging output) 
\note You need to enable CS_AST_DEBUG__LOCKS for this function
*/
int __sccp_mutex_unlock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res = 0;

#ifdef CS_LOCKS_DEBUG_ALL
	sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking %s\n", filename, lineno, func, itemnametolock);
#endif
	
	if (!p_mutex) {
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking non-existing mutex\n", filename, lineno, func);
		return 0;
	}


#ifdef CS_AST_DEBUG_THREADS
	res = __ast_pthread_mutex_unlock(filename, lineno, func, itemnametolock, p_mutex);
#else
	res = ast_mutex_unlock(p_mutex);
#endif

#ifdef CS_LOCKS_DEBUG_ALL
#ifdef CS_AST_DEBUG_THREADS
    int count = 0;
	if ((count = p_mutex->reentrancy))
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Still have %d locks (recursive)\n", count);
#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
	if (!res)
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was unlocked\n", filename, lineno, func, itemnametolock);
#endif

	if (res == EINVAL) {
        sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s had no lock by this thread. Failed unlocking\n", filename, lineno, func, itemnametolock);
	}

	if (res == EPERM) 
    {
		/* We had no lock, so okay any way*/
        sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked at all \n", filename, lineno, func, itemnametolock);
   	    res = 0;
    }
    
	return res;
}

/*! \brief Lock (and print debugging output)
\note You need to enable DEBUG__LOCKS for this function */
int __sccp_mutex_lock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res;
#ifdef CS_LOCKS_DEBUG_ALL
    sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Locking %s\n", filename, lineno, func, itemnametolock);
#endif

#ifdef CS_AST_DEBUG_THREADS
	res = __ast_pthread_mutex_lock(filename, lineno, func, itemnametolock, p_mutex);
#else
	res = ast_mutex_lock(p_mutex);
#endif

#ifdef CS_LOCKS_DEBUG_ALL
#ifdef CS_AST_DEBUG_THREADS
    int count = 0;
	if ((count = p_mutex->reentrancy))
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Now have %d locks (recursive)\n", count);
#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
	if (!res)
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
#endif

	if (res == EDEADLK) 
    {
	    /* We had no lock, so okey any way */
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked by us. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
	}
	if (res == EINVAL) 
    {
        sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);
	}

	return res;
}

/*! \brief Lock (and print debugging output)
\note	You need to enable DEBUG__LOCKS for this function */
int __sccp_mutex_trylock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res;

#ifdef CS_LOCKS_DEBUG_ALL
	sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Trying to lock %s\n", filename, lineno, func, itemnametolock);
#endif

#ifdef CS_AST_DEBUG_THREADS
	res = __ast_pthread_mutex_trylock(filename, lineno, func, itemnametolock, p_mutex);
#else
	res = ast_mutex_trylock(p_mutex);
#endif

#ifdef CS_LOCKS_DEBUG_ALL
#ifdef CS_AST_DEBUG_THREADS
    int count = 0;
	if ((count = p_mutex->reentrancy))
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Now have %d locks (recursive)\n", count);
#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
	if (!res)
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
#endif

	if (res == EBUSY) {
		/* We failed to lock */
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s failed to lock. Not waiting around...\n", filename, lineno, func, itemnametolock);
	}
	if (res == EDEADLK) {
		/* We had no lock, so okey any way*/
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
	}
	if (res == EINVAL)
		sccp_log(10)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);

	return res;
}

#endif
