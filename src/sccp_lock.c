/*!
 * \file 	sccp_lock.c
 * \brief 	SCCP Lock Class
 * \author 	Federico Santulli <info [at] chan-sccp.org>
 * \note	Mutex lock code derived from Asterisk 1.4
 * \note 	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 * 
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
#define CS_LOCKS_DEBUG_ALL

/*!
 * \brief Mutex Unlock AST MUTEX (and print debugging output)
 * \param mutex Mutex as ast_mutex_t
 * \param itemnametolock Item-Name to Lock as char
 * \param filename FileName as char
 * \param lineno LineNumber as int
 * \param func Function as char
 * \return Result as int
 * \note You need to enable CS_AST_DEBUG__LOCKS for this function
 */
int __sccp_mutex_unlock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
        int res = 0;

#ifdef CS_LOCKS_DEBUG_ALL

        if (strncasecmp(filename, "sccp_socket.c", 13))
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking %s\n", filename, lineno, func, itemnametolock);

#endif

        if (!p_mutex) {
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking non-existing mutex\n", filename, lineno, func);
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

#ifndef CS_AST_HAS_TRACK
        if ((count = p_mutex->reentrancy)) {
#else

        if ((count = p_mutex->track.reentrancy)) {
#endif

                if (strncasecmp(filename, "sccp_socket", 11)) {
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
                }
        }

#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
        if (!res) {
                if (strncasecmp(filename, "sccp_socket.c", 13))
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was unlocked\n", filename, lineno, func, itemnametolock);
        }

#endif

        if (res == EINVAL) {
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s had no lock by this thread. Failed unlocking\n", filename, lineno, func, itemnametolock);
        }

        if (res == EPERM) {
                /* We had no lock, so okay any way*/
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked at all \n", filename, lineno, func, itemnametolock);
                res = 0;
        }

        return res;
}

/*!
 * \brief Mutex Lock (and print debugging output)
 * \param mutex Mutex as ast_mutex_t
 * \param itemnametolock Item-Name to Lock as char
 * \param filename FileName as char
 * \param lineno LineNumber as int
 * \param func Function as char
 * \return Result as int
 * \note You need to enable DEBUG__LOCKS for this function
 */
int __sccp_mutex_lock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func) {
        int res;
#ifdef CS_LOCKS_DEBUG_ALL

        if (strncasecmp(filename, "sccp_socket.c", 13))
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Locking %s\n", filename, lineno, func, itemnametolock);

#endif

#ifdef CS_AST_DEBUG_THREADS
        res = __ast_pthread_mutex_lock(filename, lineno, func, itemnametolock, p_mutex);

#else
        res = ast_mutex_lock(p_mutex);

#endif

#ifdef CS_LOCKS_DEBUG_ALL
#ifdef CS_AST_DEBUG_THREADS
        int count = 0;

#ifndef CS_AST_HAS_TRACK
        if ((count = p_mutex->reentrancy)) {
#else

        if ((count = p_mutex->track.reentrancy)) {
#endif

                if (strncasecmp(filename, "sccp_socket", 11)) {
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
                }
        }

#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
        if (!res) {
                if (strncasecmp(filename, "sccp_socket.c", 13))
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
        }

#endif

        if (res == EDEADLK) {
                /* We had no lock, so okey any way */
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked by us. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
        }

        if (res == EINVAL) {
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);
        }

        return res;
}

/*!
 * \brief Try to get a Lock (and print debugging output)
 * \param mutex Mutex as ast_mutex_t
 * \param itemnametolock Item-Name to Lock as char
 * \param filename FileName as char
 * \param lineno LineNumber as int
 * \param func Function as char
 * \return Result as int
 * \note You need to enable DEBUG__LOCKS for this function
 */
int __sccp_mutex_trylock(ast_mutex_t *p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func) {
        int res;

#ifdef CS_LOCKS_DEBUG_ALL

        if (strncasecmp(filename, "sccp_socket.c", 13))
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Trying to lock %s\n", filename, lineno, func, itemnametolock);

#endif

#ifdef CS_AST_DEBUG_THREADS
        res = __ast_pthread_mutex_trylock(filename, lineno, func, itemnametolock, p_mutex);

#else
        res = ast_mutex_trylock(p_mutex);

#endif

#ifdef CS_LOCKS_DEBUG_ALL
#ifdef CS_AST_DEBUG_THREADS
        int count = 0;

#ifndef CS_AST_HAS_TRACK
        if ((count = p_mutex->reentrancy)) {
#else

        if ((count = p_mutex->track.reentrancy)) {
#endif

                if (strncasecmp(filename, "sccp_socket", 11)) {
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
                }
        }

#endif
#endif

#ifdef CS_LOCKS_DEBUG_ALL
        if (!res) {
                if (strncasecmp(filename, "sccp_socket", 11))
                        sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
        }

#endif

        if (res == EBUSY) {
                /* We failed to lock */
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s failed to lock. Not waiting around...\n", filename, lineno, func, itemnametolock);
        }

        if (res == EDEADLK) {
                /* We had no lock, so okey any way*/
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
        }

        if (res == EINVAL)
                sccp_log(99)(VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);

        return res;
}

#endif
