
/*!
 * \file 	sccp_lock.c
 * \brief 	SCCP Lock Class
 * \author 	Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note		Mutex lock code derived from Asterisk 1.4
 * \note		This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \file
 * \page sccp_lock Locking in SCCP
 * 
 * \section lock_types Lock Types and their Objects
 *
 * In chan-sccp-b we use different types of locks for different occasions. It depents mainly on the object which needs to
 * be locked, how long this lock will last and if the object should be shared most of the time between multiple threads.
 *
 * We make a distinction between locks for global lists, global objects and list/objects used/owned by a global object. 
 * 
 * Not all owned lists / objects have their own lock. Only owned lists / objects that change frequently during their livetime.
 *
 * for example:
 * - list of devices (rwlock, shared) -> multiple readers or 1 writer can have this lock on the list
 *   once the device you where looking for it need to be locked exclusively, for read or write, to prevent changes:
 *   - device (currently mutex) -> multiple readers or 1 writer can have this lock on the list
 *     (note at the moment it is still a mutex but we might gradually change then to a rwlock.)
 *     - list of selectedChannels(mutex)
 *       - selectedChannel (mutex)
 *
 * global lists (in this order):
 * - sessions (mutex)
 * - devices (rwlock)
 * - lines (rwlock)
 *   - channels (mutex -> rwlock)
 *   .
 * .
 * <small>(mutex -> rwlock = currently using mutex, should change to rwlock soon)</small>
 *
 * global objects (in this order):
 * - sccp_session (mutex)
 * - sccp_device (mutex -> rwlock)
 * - sccp_line (mutex -> rwlock)
 *   - sccp_channel (mutex ->rwlock)
 *   .
 * .
 *
 * object owned lists (e.g for device):
 * - buttonconfig (no lock -> lock parent)			// should be renamed to buttonconfigs to indicate it's list status
 * - selectedChannels (mutex)					// channels added / removed frequently
 * - addons (no lock -> lock parent)
 * - permithosts (no lock -> lock parent)
 * - devstateSpecifiers (no lock -> lock parent)		// added once
 * .
 * 
 * object owned objects (e.g. for device):
 * - buttonconfig (no lock -> lock parent)
 * - selectedChannel (no lock -> lock parent)			// state does not change
 * - addon (no lock -> lock parent)
 * - permithost (no lock -> lock parent)
 * - devstateSpecifier (mutex)					// state changes frequently
 * .
 *
 * \section lock_order Locking order
 * 
 * - Rule1: 	Locks should be taking in order to prevent deadlocks. 
 * 		So always lock the highest order first:
 * 	- Global list (in order device, line, channel)
 * 	- Global object (in order device, line, channel)
 * 	- Owned list (in order specified in Rule2/Rule3) having rwlock / mutex, otherwise lock parent
 * 	- Owned object (in order specified in Rule2/Rule3) havind rwlock / mutex, otherwise lock parent
 *	.
 *
 * - Rule2: For locks on owned lists / objects, you must release as soon as possible and prevent nesting. 
 * - Rule3: If nesting can not be avoided, lock in order of the struct in chan_sccp.h providing the owned list. 
 * .
 *
 * \section lock_deadlock Prevent Deadlocks
 * \section lock_starvation Prevent Starvation
 * \section lock_assumptions Locking Assumptions
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#ifdef CS_AST_DEBUG_CHANNEL_LOCKS
#    define CS_LOCKS_DEBUG_ALL

/*!
 * \brief Mutex Unlock AST MUTEX (and print debugging output)
 * \param mutex Mutex as ast_mutex_t
 * \param itemnametolock Item-Name to Lock as char
 * \param filename FileName as char
 * \param lineno LineNumber as int
 * \param func Function as char
 * \return Result as int
 *
 * \note You need to enable DEBUG__LOCKS for this function
 */
int __sccp_mutex_unlock(ast_mutex_t * p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res = 0;

#    ifdef CS_LOCKS_DEBUG_ALL
	if (strncasecmp(filename, "sccp_socket.c", 13))
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking %s\n", filename, lineno, func, itemnametolock);

#    endif
	if (!p_mutex) {
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Unlocking non-existing mutex\n", filename, lineno, func);
		return 0;
	}
#    ifdef CS_AST_DEBUG_THREADS
	res = __pbx_pthread_mutex_unlock(filename, lineno, func, itemnametolock, p_mutex);

#    else
	res = pbx_mutex_unlock(p_mutex);
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
#        ifdef CS_AST_DEBUG_THREADS
	int count = 0;

#            ifndef CS_AST_HAS_TRACK
	if ((count = p_mutex->reentrancy)) {
#            else

	if ((count = p_mutex->track.reentrancy)) {
#            endif

		if (strncasecmp(filename, "sccp_socket", 11)) {
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
		}
	}
#        endif
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
	if (!res) {
		if (strncasecmp(filename, "sccp_socket.c", 13))
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was unlocked\n", filename, lineno, func, itemnametolock);
	}
#    endif

	if (res == EINVAL) {
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s had no lock by this thread. Failed unlocking\n", filename, lineno, func, itemnametolock);
	}

	if (res == EPERM) {
		/* We had no lock, so okay any way */
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked at all \n", filename, lineno, func, itemnametolock);
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
int __sccp_mutex_lock(ast_mutex_t * p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res;

#    ifdef CS_LOCKS_DEBUG_ALL
	if (strncasecmp(filename, "sccp_socket.c", 13))
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Locking %s\n", filename, lineno, func, itemnametolock);

#        if ASTERISK_VERSION_NUMBER >= 10601
	if ((sccp_globals->debug & DEBUGCAT_THREADLOCK) != 0) {
		log_show_lock(p_mutex);
	}
#        endif
#    endif
#    ifdef CS_AST_DEBUG_THREADS
	res = __pbx_pthread_mutex_lock(filename, lineno, func, itemnametolock, p_mutex);
#    else
	res = pbx_mutex_lock(p_mutex);
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
#        ifdef CS_AST_DEBUG_THREADS
	int count = 0;

#            ifndef CS_AST_HAS_TRACK
	if ((count = p_mutex->reentrancy)) {
#            else

	if ((count = p_mutex->track.reentrancy)) {
#            endif
		if (strncasecmp(filename, "sccp_socket", 11)) {
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
		}
	}
#        endif
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
	if (!res) {
		if (strncasecmp(filename, "sccp_socket.c", 13))
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
	}
#    endif

	if (res == EDEADLK) {
		/* We had no lock, so okey any way */
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked by us. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
	}

	if (res == EINVAL) {
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);
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
int __sccp_mutex_trylock(ast_mutex_t * p_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func)
{
	int res;

#    ifdef CS_LOCKS_DEBUG_ALL
	if (strncasecmp(filename, "sccp_socket.c", 13))
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: Trying to lock %s\n", filename, lineno, func, itemnametolock);

#        if ASTERISK_VERSION_NUMBER >= 10601
	if ((sccp_globals->debug & DEBUGCAT_THREADLOCK) != 0) {
		log_show_lock(p_mutex);
	}
#        endif
#    endif
#    ifdef CS_AST_DEBUG_THREADS
	res = __pbx_pthread_mutex_trylock(filename, lineno, func, itemnametolock, p_mutex);
#    else
	res = pbx_mutex_trylock(p_mutex);
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
#        ifdef CS_AST_DEBUG_THREADS
	int count = 0;

#            ifndef CS_AST_HAS_TRACK
	if ((count = p_mutex->reentrancy)) {
#            else

	if ((count = p_mutex->track.reentrancy)) {
#            endif
		if (strncasecmp(filename, "sccp_socket", 11)) {
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s now have %d locks (recursive)\n", filename, lineno, func, itemnametolock, count);
		}
	}
#        endif
#    endif

#    ifdef CS_LOCKS_DEBUG_ALL
	if (!res) {
		if (strncasecmp(filename, "sccp_socket", 11))
			sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was locked\n", filename, lineno, func, itemnametolock);
	}
#    endif

	if (res == EBUSY) {
		/* We failed to lock */
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s failed to lock. Not waiting around...\n", filename, lineno, func, itemnametolock);
	}

	if (res == EDEADLK) {
		/* We had no lock, so okey any way */
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s was not locked. Lock would cause deadlock.\n", filename, lineno, func, itemnametolock);
	}

	if (res == EINVAL)
		sccp_log((DEBUGCAT_LOCK)) (VERBOSE_PREFIX_3 "::::==== %s line %d (%s) SCCP_MUTEX: %s lock failed. No mutex.\n", filename, lineno, func, itemnametolock);

	return res;
}
#endif

void * RefCountedObjectAlloc(size_t size, void *destructor)
{
	RefCountedObject * o;
	char * ptr;
	o = ( RefCountedObject * )calloc( sizeof( RefCountedObject ) + size, 1 );
	ptr = ( char * )o;
	ptr += sizeof( RefCountedObject);
	ast_atomic_fetchadd_int(&o->refcount, 1);
	o->data = ptr;
	o->destructor = destructor;
	return ( void * )ptr;
} 

int sccp_retain(void * ptr) 
{
	RefCountedObject * o;
	char * cptr;

	cptr = (char *)ptr;
	cptr -= sizeof(RefCountedObject);
	o = (RefCountedObject *)cptr;

	ast_atomic_fetchadd_int(&o->refcount, 1);
	return 0;
}

int sccp_release(void * ptr) 
{
	RefCountedObject * o;
	char * cptr;
	int beforeVal=0;

	cptr = (char *)ptr;
	cptr -= sizeof(RefCountedObject);
	o = (RefCountedObject *)cptr;

	beforeVal=ast_atomic_fetchadd_int(&o->refcount, -1);
	if( (beforeVal-1) == 0 ) {
		pbx_log(LOG_NOTICE, "ReferenceCount for %p, reached 0 -> cleaning up!\n", o);
		o->destructor(o);
	}
	return 0;
}
