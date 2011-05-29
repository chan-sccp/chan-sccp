/*!
 * \file 	sccp_lock.h
 * \brief 	SCCP Lock Header
 * \author 	Federico Santulli <fsantulli [at] users.sourceforge.net>
 * \note	Mutex lock code derived from Asterisk 1.4
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_LOCK_H
#    define __SCCP_LOCK_H
#    include "config.h"
#    include <asterisk/lock.h>
#    define sccp_mutex_init(x)          		ast_mutex_init(x)
#    define sccp_mutex_destroy(x)       		ast_mutex_destroy(x)

#    if ASTERISK_VERSION_NUMBER >= 10400
	/* Channel Mutex Macros for Asterisk 1.4 and above */
#        define sccp_ast_channel_lock(x)    	ast_channel_lock(x)
#        define sccp_ast_channel_unlock(x)  	ast_channel_unlock(x)
#        define sccp_ast_channel_trylock(x) 	ast_channel_trylock(x)
#        if ASTERISK_VERSION_NUMBER >= 10600
#            define AST_CHANNEL_DEADLOCK_AVOIDANCE(x)	CHANNEL_DEADLOCK_AVOIDANCE(x)
#        else
#            define AST_CHANNEL_DEADLOCK_AVOIDANCE(x)	DEADLOCK_AVOIDANCE(&x->lock)
#        endif
#    else
	/* Channel Mutex Macros for Asterisk 1.2 */
#        define sccp_ast_channel_lock(x)    	sccp_mutex_lock(&x->lock)
#        define sccp_ast_channel_unlock(x) 	sccp_mutex_unlock(&x->lock)
#        define sccp_ast_channel_trylock(x) 	sccp_mutex_trylock(&x->lock)
#        define AST_CHANNEL_DEADLOCK_AVOIDANCE(x)	do { \
	                                               sccp_ast_channel_unlock(x); \
		                                       usleep(1); \
		                                       sccp_ast_channel_lock(x); \
		                                while(0)
#        define AST_DEADLOCK_AVOIDANCE(x)	DEADLOCK_AVOIDANCE(&x->lock)
#    endif									// ASTERISK_VERSION_NUMBER >= 10400

#    ifndef CS_AST_DEBUG_CHANNEL_LOCKS
	/* Macro for Generic Mutex */
#        define sccp_mutex_lock(x)		ast_mutex_lock(x)
#        define sccp_mutex_lock_desc(x,y) 	ast_mutex_lock(x)
#        define sccp_mutex_unlock(x)		ast_mutex_unlock(x)
#        define sccp_mutex_trylock(x)		ast_mutex_trylock(x)

	/* Macro for Lines */
#        define sccp_line_lock(x)		ast_mutex_lock(&x->lock)
#        define sccp_line_unlock(x)         	ast_mutex_unlock(&x->lock)
#        define sccp_line_trylock(x)		ast_mutex_trylock(&x->lock)

	/* Macro for Devices */
#        define sccp_device_lock(x)		ast_mutex_lock(&x->lock)
#        define sccp_device_unlock(x)		ast_mutex_unlock(&x->lock)
#        define sccp_device_trylock(x)		ast_mutex_trylock(&x->lock)

	/* Macro for Channels */
#        define sccp_channel_lock_dbg(x,w,y,z)	ast_mutex_lock(&x->lock)
#        define sccp_channel_lock(x)		ast_mutex_lock(&x->lock)
#        define sccp_channel_unlock(x)		ast_mutex_unlock(&x->lock)
#        define sccp_channel_trylock(x)		ast_mutex_trylock(&x->lock)

	/* Macro for Sessions */
#        define sccp_session_lock(x)		ast_mutex_lock(&x->lock)
#        define sccp_session_unlock(x)		ast_mutex_unlock(&x->lock)
#        define sccp_session_trylock(x)		ast_mutex_trylock(&x->lock)

	/* Macro for Globals */
#        define sccp_globals_lock(x)		ast_mutex_lock(&sccp_globals->x)
#        define sccp_globals_unlock(x)		ast_mutex_unlock(&sccp_globals->x)
#        define sccp_globals_trylock(x)		ast_mutex_trylock(&sccp_globals->x)

	/* Macro for Lists */
#        define SCCP_LIST_LOCK(x)		ast_mutex_lock(&(x)->lock)
#        define SCCP_LIST_UNLOCK(x)		ast_mutex_unlock(&(x)->lock)
#        define SCCP_LIST_TRYLOCK(x)		ast_mutex_trylock(&(x)->lock)

	/* Macro for read/write Lists */
#        define SCCP_RWLIST_RDLOCK(x)		ast_rwlock_rdlock(&(x)->lock)
#        define SCCP_RWLIST_WRLOCK(x)		ast_rwlock_wrlock(&(x)->lock)
#        define SCCP_RWLIST_UNLOCK(x)		ast_rwlock_unlock(&(x)->lock)
#        define SCCP_RWLIST_TRYLOCK(x)		ast_rwlock_trylock(&(x)->lock)
#        define SCCP_RWLIST_TRYRDLOCK(x)	ast_rwlock_tryrdlock(&(x)->lock)
#        define SCCP_RWLIST_TRYWRLOCK(x)	ast_rwlock_trywrlock(&(x)->lock)
#    else									// CS_AST_DEBUG_CHANNEL_LOCKS
	/* Macro for Generic Mutex */
int __sccp_mutex_lock(ast_mutex_t * p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
int __sccp_mutex_unlock(ast_mutex_t * p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
int __sccp_mutex_trylock(ast_mutex_t * p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
#        define sccp_mutex_lock(a)          	__sccp_mutex_lock(a, "(sccp unspecified [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_mutex_lock_desc(a, b) 	__sccp_mutex_lock(a, b, __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_mutex_unlock(a)        	__sccp_mutex_unlock(a, "(sccp unspecified [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_mutex_trylock(a)       	__sccp_mutex_trylock(a, "(sccp unspecified [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Lines */
#        define sccp_line_lock(a)           	__sccp_mutex_lock(&a->lock, "(sccp line [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_line_unlock(a)         	__sccp_mutex_unlock(&a->lock, "(sccp line [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_line_trylock(a)        	__sccp_mutex_trylock(&a->lock, "(sccp line [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Devices */
#        define sccp_device_lock(a)		__sccp_mutex_lock(&a->lock, "(sccp device [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_device_unlock(a)		__sccp_mutex_unlock(&a->lock, "(sccp device [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_device_trylock(a)		__sccp_mutex_trylock(&a->lock, "(sccp device [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Channels */
#        define sccp_channel_lock(a)		__sccp_mutex_lock(&a->lock, "(sccp channel [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_channel_lock_dbg(a,b,c,d) 	__sccp_mutex_lock(&a->lock, "(sccp channel [" #a "])", b, c, d)
#        define sccp_channel_unlock(a)     	__sccp_mutex_unlock(&a->lock, "(sccp channel [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_channel_trylock(a)		__sccp_mutex_trylock(&a->lock, "(sccp channel [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Sessions */
#        define sccp_session_lock(a)		__sccp_mutex_lock(&a->lock, "(sccp session [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_session_unlock(a)		__sccp_mutex_unlock(&a->lock, "(sccp session [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_session_trylock(a)		__sccp_mutex_trylock(&a->lock, "(sccp session [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Globals */
#        define sccp_globals_lock(a)		__sccp_mutex_lock(&sccp_globals->a, "(sccp globals [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_globals_unlock(a)		__sccp_mutex_unlock(&sccp_globals->a, "(sccp globals [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define sccp_globals_trylock(a)		__sccp_mutex_trylock(&sccp_globals->a, "(sccp globals [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for Lists */
#        define SCCP_LIST_LOCK(a)		__sccp_mutex_lock(&(a)->lock, "(SCCP LIST [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define SCCP_LIST_UNLOCK(a)		__sccp_mutex_unlock(&(a)->lock, "(SCCP LIST [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#        define SCCP_LIST_TRYLOCK(a)		__sccp_mutex_trylock(&(a)->lock, "(SCCP LIST [" #a "])", __FILE__, __LINE__, __PRETTY_FUNCTION__)

	/* Macro for read/write Lists */
	/* \todo add __sccp_rwlock implementation for debugging in sccp_lock.c */
#        define SCCP_RWLIST_RDLOCK(x)		ast_rwlock_rdlock(&(x)->lock)
#        define SCCP_RWLIST_WRLOCK(x)		ast_rwlock_wrlock(&(x)->lock)
#        define SCCP_RWLIST_UNLOCK(x)		ast_rwlock_unlock(&(x)->lock)
#        define SCCP_RWLIST_TRYLOCK(x)		ast_rwlock_trylock(&(x)->lock)
#        define SCCP_RWLIST_TRYRDLOCK(x)	ast_rwlock_tryrdlock(&(x)->lock)
#        define SCCP_RWLIST_TRYWRLOCK(x)	ast_rwlock_trywrlock(&(x)->lock)
#    endif									// CS_AST_DEBUG_CHANNEL_LOCKS
#endif										// __SCCP_LOCK_H
