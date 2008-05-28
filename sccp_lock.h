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
#ifndef __SCCP_LOCK_H
#define __SCCP_LOCK_H
#include "config.h"
#include <asterisk/lock.h>
#define sccp_mutex_init(x)          ast_mutex_init(x)
#define sccp_mutex_destroy(x)       ast_mutex_destroy(x)

#ifdef ASTERISK_CONFIG_1_2
/* Channel Mutex Macros for Asterisk 1.2 */
#define sccp_ast_channel_lock(x)    sccp_mutex_lock(&x->lock)
#define sccp_ast_channel_unlock(x)  sccp_mutex_unlock(&x->lock)
#define sccp_ast_channel_trylock(x) sccp_mutex_trylock(&x->lock)
#else
/* Channel Mutex Macros for Asterisk 1.4 and above */
#define sccp_ast_channel_lock(x)    ast_channel_lock(x)
#define sccp_ast_channel_unlock(x)  ast_channel_unlock(x)
#define sccp_ast_channel_trylock(x) ast_channel_trylock(x)
#endif

#ifndef CS_AST_DEBUG_CHANNEL_LOCKS
/* Macro for Generic Mutex */
#define sccp_mutex_lock(x)		    ast_mutex_lock(x)
#define sccp_mutex_unlock(x)		ast_mutex_unlock(x)
#define sccp_mutex_trylock(x)		ast_mutex_trylock(x)
/* Macro for Lines */
#define sccp_line_lock(x)		    ast_mutex_lock(&x->lock)
#define sccp_line_unlock(x)         ast_mutex_unlock(&x->lock)
#define sccp_line_trylock(x)		ast_mutex_trylock(&x->lock)
/* Macro for Devices */
#define sccp_device_lock(x)		    ast_mutex_lock(&x->lock)
#define sccp_device_unlock(x)		ast_mutex_unlock(&x->lock)
#define sccp_device_trylock(x)		ast_mutex_trylock(&x->lock)
/* Macro for Channels */
#define sccp_channel_lock(x)		ast_mutex_lock(&x->lock)
#define sccp_channel_unlock(x)		ast_mutex_unlock(&x->lock)
#define sccp_channel_trylock(x)		ast_mutex_trylock(&x->lock)
#else
#define sccp_mutex_lock(a)          __sccp_mutex_lock(a, "(unspecified)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_line_lock(a)           __sccp_mutex_lock(&a->lock, "(line)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_device_lock(a)		    __sccp_mutex_lock(&a->lock, "(device)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_channel_lock(a)		__sccp_mutex_lock(&a->lock, "(channel)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
/*! \brief Lock AST channel (and print debugging output)
\note You need to enable DEBUG_CHANNEL_LOCKS for this function */
int __sccp_mutex_lock(ast_mutex_t *p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
#define sccp_mutex_unlock(a)        __sccp_mutex_unlock(a, "(unspecified)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_line_unlock(a)         __sccp_mutex_unlock(&a->lock, "(line)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_device_unlock(a)		__sccp_mutex_unlock(&a->lock, "(device)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_channel_unlock(a)      __sccp_mutex_unlock(&a->lock, "(channel)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
/*! \brief Unlock AST channel (and print debugging output)
\note You need to enable DEBUG_CHANNEL_LOCKS for this function
*/
int __sccp_mutex_unlock(ast_mutex_t *p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
#define sccp_mutex_trylock(a)       __sccp_mutex_trylock(a, "(unspecified)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_line_trylock(a)        __sccp_mutex_trylock(&a->lock, "(line)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_device_trylock(a)		__sccp_mutex_trylock(&a->lock, "(device)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define sccp_channel_trylock(a)		__sccp_mutex_trylock(&a->lock, "(channel)", __FILE__, __LINE__, __PRETTY_FUNCTION__)
/*! \brief Lock AST channel (and print debugging output)
\note   You need to enable DEBUG_CHANNEL_LOCKS for this function */
int __sccp_mutex_trylock(ast_mutex_t *p_ast_mutex, const char *itemnametolock, const char *filename, int lineno, const char *func);
#endif
#endif
