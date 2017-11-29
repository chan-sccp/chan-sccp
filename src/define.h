/*!
 * \file        chan_sccp.h
 * \brief       Chan SCCP Main Class
 *
 * An implementation of Skinny Client Control Protocol (SCCP)
 *
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \brief       Main chan_sccp Class
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \warning     File has been Lined up using 8 Space TABS
 *
 */

#pragma once
#include "config.h"
#include <pthread.h>

/*!
 * \note most of these should be moved to autoconf/asterisk.m4 and be defined in config.h
 */
#if !defined(__BEGIN_C_EXTERN__)
#  if defined(__cplusplus) || defined(c_plusplus) 
#    define __BEGIN_C_EXTERN__ 		\
extern "C" {
#    define __END_C_EXTERN__ 		\
}
#  else
#    define __BEGIN_C_EXTERN__ 
#    define __END_C_EXTERN__ 
#  endif
#endif

#if !defined(SCCP_API)
#if defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L && defined(__GNUC__)
#  if !defined(__clang__)
#    define gcc_inline __inline__
#  else
#    define gcc_inline
#  endif
#  define SCCP_API extern __attribute__((__visibility__("hidden")))
#  define SCCP_API_VISIBLE extern __attribute__((__visibility__("default")))
#  define SCCP_INLINE SCCP_API gcc_inline
#  define SCCP_CALL 
#define __PURE__ __attribute__((pure))
#define __CONST__ __attribute__((const))
#else
#  define gcc_inline
#  define SCCP_API extern
#  define SCCP_API_VISIBLE extern
#  define SCCP_INLINE SCCP_API gcc_inline
#  define SCCP_CALL 
#define __PURE__ 
#define __CONST__ 
#endif
#endif

#if defined(RUNNING_STATIC_ANALYSIS)
#define __NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
#define __NONNULL(...) 
#endif

#define sccp_mutex_t pbx_mutex_t

/* Add bswap function if necessary */
#if HAVE_BYTESWAP_H
#  include <byteswap.h>
#else
#  if HAVE_SYS_BYTEORDER_H
#    include <sys/byteorder.h>
#  elif HAVE_SYS_ENDIAN_H
#    include <sys/endian.h>
#  else
#    ifndef HAVE_BSWAP_16
SCCP_INLINE unsigned short __bswap_16(unsigned short x)
{
        return (x >> 8) | (x << 8);
}
#    endif
#    ifndef HAVE_BSWAP_32
SCCP_LINE unsigned int __bswap_32(unsigned int x)
{
        return (__bswap_16(x & 0xffff) << 16) | (__bswap_16(x >> 16));
}
#    endif
#    ifndef HAVE_BSWAP_64
SCCP_LINE unsigned long long __bswap_64(unsigned long long x)
{
        return (((unsigned long long) __bswap_32(x & 0xffffffffull)) << 32) | (__bswap_32(x >> 32));
}
#    endif
#  endif
#endif

/* Byte swap based on platform endianes */
#if SCCP_LITTLE_ENDIAN == 1
#define letohl(x) (x)
#define letohs(x) (x)
#define htolel(x) (x)
#define htoles(x) (x)
#else
#define letohs(x) __bswap_16(x)
#define htoles(x) __bswap_16(x)
#define letohl(x) __bswap_32(x)
#define htolel(x) __bswap_32(x)
#endif

#define SCCP_TECHTYPE_STR "SCCP"

#define RET_STRING(_a) #_a
#define STRINGIFY(_a) RET_STRING(_a)

/* Versioning */
/*
#ifndef SCCP_VERSION
#define SCCP_VERSION "custom"
#endif

#ifndef SCCP_BRANCH
#define SCCP_BRANCH "trunk"
#endif
*/

#define SCCP_FILENAME_MAX 80
#if defined(PATH_MAX)
#define SCCP_PATH_MAX PATH_MAX
#else
#define	SCCP_PATH_MAX 2048
#endif

#define SCCP_MIN_KEEPALIVE 30
#define SCCP_LOCK_TRIES 10
#define SCCP_LOCK_USLEEP 100
#define SCCP_MIN_DTMF_DURATION 80										// 80 ms
#define SCCP_FIRST_LINEINSTANCE 1										/* whats the instance of the first line */
#define SCCP_FIRST_SERVICEINSTANCE 1										/* whats the instance of the first service button */
#define SCCP_FIRST_SPEEDDIALINSTANCE 1										/* whats the instance of the first speeddial button */

#define SCCP_DISPLAYSTATUS_TIMEOUT 5

/* Simulated Enbloc Dialing */
//#define SCCP_SIM_ENBLOC_DEVIATION 3.5
#define SCCP_SIM_ENBLOC_MAX_PER_DIGIT 400
//#define SCCP_SIM_ENBLOC_MIN_DIGIT 3
#define SCCP_SIM_ENBLOC_MIN_DIGIT 4
#define SCCP_SIM_ENBLOC_TIMEOUT 2

#define SCCP_HANGUP_TIMEOUT 15000

#define THREADPOOL_MIN_SIZE 2
#define THREADPOOL_MAX_SIZE 10
#define THREADPOOL_RESIZE_INTERVAL 10

#define CAS32_TYPE int
#define SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT 2000									// ms
#define SCCP_BACKTRACE_SIZE 10
#define SCCP_DEVICE_MWILIGHT 30

#define DEFAULT_PBX_STR_BUFFERSIZE 512

/*! \todo I don't like the -1 returned value */
#define sccp_true(x) (pbx_true(x) ? 1 : 0)
#define sccp_false(x) (pbx_false(x) ? 1 : 0)

#define GLOB(x) sccp_globals->x

/* Lock Macro for Globals */
#define sccp_globals_lock(x)			pbx_mutex_lock(&sccp_globals->x)
#define sccp_globals_unlock(x)			pbx_mutex_unlock(&sccp_globals->x)
#define sccp_globals_trylock(x)			pbx_mutex_trylock(&sccp_globals->x)

#define DEV_ID_LOG(x) (x && !sccp_strlen_zero(x->id)) ? x->id : "SCCP"

#define PTR_TYPE_CMP(_type,_ptr) 					\
({									\
	/*__builtin_types_compatible_p(typeof(_ptr), _type) == 1)*/	\
	_type __attribute__((unused)) __dummy = (_ptr);			\
	1;								\
})

/* (temporary) forward declarations */
/* this can be removed by using a pointer version of mutex and rwlock in structures below */
#define StationMaxServiceURLSize			256
struct pbx_mutex_info {
        pthread_mutex_t mutex;
        struct ast_lock_track *track;
        unsigned int tracking:1;
};

struct pbx_rwlock_info {
        pthread_rwlock_t lock;
        struct ast_lock_track *track;
        unsigned int tracking:1;
};
typedef struct pbx_mutex_info pbx_mutex_t;
typedef struct pbx_rwlock_info pbx_rwlock_t;

#define AUTO_MUTEX(varname, lock) SCOPED_LOCK(varname, (lock), pbx_mutex_lock, pbx_mutex_unlock)
#define AUTO_RDLOCK(varname, lock) SCOPED_LOCK(varname, (lock), pbx_rwlock_rdlock, pbx_rwlock_unlock)
#define AUTO_WRLOCK(varname, lock) SCOPED_LOCK(varname, (lock), pbx_rwlock_wrlock, pbx_rwlock_unlock)
/* example AUTO_RDLOCK(lock, &s->lock); */

/* check to see if is pointer is actually already being cleaned up */
#if __WORDSIZE == 64
#define isPointerDead(_x) (sizeof(char*) == 4 ? (uintptr_t)_x == 0xdeaddead : (uintptr_t)_x == 0xdeaddeaddeaddead)
#else
#define isPointerDead(_x) ((uintptr_t)_x == 0xdeaddead)
#endif

/* deny the use of unsafe functions */
#define __strcat strcat
#define strcat(...) _Pragma("GCC error \"use snprint instead of strcat\"")

#define __strncat strncat
#undef strncat
#define strncat(...) _Pragma("GCC error \"use snprint instead of strncat\"")

#define __strcpy strcpy
#define strcpy(...) _Pragma("GCC error \"use sccp copy string instead of_strcpy\"")

#define __strncpy strcpy
#undef strncpy
#define strncpy(...) _Pragma("GCC error \"use sccp copy string instead of strncpy\"")

#define __sprintf sprintf
#define sprintf(...) _Pragma("GCC error \"use snprintf instead of sprintf\"")

#define __vsprintf vsprintf
#define vsprintf(...) _Pragma("GCC error \"use vsnprintf instead of vsprintf\"")

#define __gets gets
#define gets(...) _Pragma("GCC error \"use fgets instead of gets\"")

#define __atoi atoi
#define atoi(...) _Pragma("GCC error \"use sccp atoi instead of atoi\"")

#define __strdupa strdupa
#undef strdupa
#define strdupa(...) _Pragma("GCC error \"use pbx_strdupa instead of strdupa\"")

#if defined(__clang__)
typedef void (^sccp_raii_cleanup_block_t)(void);
static inline void sccp_raii_cleanup_block(sccp_raii_cleanup_block_t *b) { (*b)(); }
#define RAII(vartype, varname, initval, dtor)										\
    sccp_raii_cleanup_block_t _raii_cleanup_ ## varname __attribute__((cleanup(sccp_raii_cleanup_block),unused)) = NULL;\
    __block vartype varname = initval;											\
    _raii_cleanup_ ## varname = ^{ {(void)dtor(varname);} }
#elif defined(__GNUC__)
#define RAII(vartype, varname, initval, dtor)										\
    auto void _dtor_ ## varname (vartype * v);										\
    void _dtor_ ## varname (vartype * v) { dtor(*v); }									\
    vartype varname __attribute__((cleanup(_dtor_ ## varname))) = (initval)
#else
    #error "Cannot compile Asterisk: unknown and unsupported compiler."
#endif /* #if __GNUC__ */

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
