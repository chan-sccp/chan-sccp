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
#  define __BEGIN_EXTERN__ __BEGIN_C_EXTERN__
#  define __END_EXTERN__ __END_C_EXTERN__
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
#else
#  define gcc_inline
#  define SCCP_API extern
#  define SCCP_API_VISIBLE extern
#  define SCCP_INLINE SCCP_API gcc_inline
#  define SCCP_CALL 
#endif
#endif

#if defined(RUNNING_STATIC_ANALYSIS) && defined(SCAN_BUILD)
#define __NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
#define __NONNULL(...) 
#endif

#define sccp_mutex_t pbx_mutex_t

/* Add bswap function if necessary */
#if HAVE_BYTESWAP_H
#include <byteswap.h>
#else
#if HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#elif HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifndef HAVE_BSWAP_16
#warn HAVE_BSWAP_16
static SCCP_INLINE unsigned short __bswap_16(unsigned short x)
{
	return (x >> 8) | (x << 8);
}
#endif
#ifndef HAVE_BSWAP_32
static SCCP_LINE unsigned int __bswap_32(unsigned int x)
{
	return (bswap_16(x & 0xffff) << 16) | (bswap_16(x >> 16));
}
#endif
#ifndef HAVE_BSWAP_64
static SCCP_LINE unsigned long long __bswap_64(unsigned long long x)
{
	return (((unsigned long long) bswap_32(x & 0xffffffffull)) << 32) | (bswap_32(x >> 32));
}
#endif
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
#ifndef SCCP_VERSION
#define SCCP_VERSION "custom"
#endif

#ifndef SCCP_BRANCH
#define SCCP_BRANCH "trunk"
#endif

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
#define SCCP_SIM_ENBLOC_DEVIATION 3.5
#define SCCP_SIM_ENBLOC_MAX_PER_DIGIT 400
#define SCCP_SIM_ENBLOC_MIN_DIGIT 3
#define SCCP_SIM_ENBLOC_TIMEOUT 2

#define SCCP_HANGUP_TIMEOUT 15000

#define THREADPOOL_MIN_SIZE 2
#define THREADPOOL_MAX_SIZE 10
#define THREADPOOL_RESIZE_INTERVAL 10

#define CHANNEL_DESIGNATOR_SIZE 32
#define SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT 2000									// ms
#define SCCP_BACKTRACE_SIZE 10
#define SCCP_DEVICE_MWILIGHT 31

#define DEFAULT_PBX_STR_BUFFERSIZE 512

/*! \todo I don't like the -1 returned value */
#define sccp_true(x) (pbx_true(x) ? 1 : 0)
#define sccp_false(x) (pbx_false(x) ? 1 : 0)

// When DEBUGCAT_HIGH is set, we use ast_log instead of ast_verbose
#define sccp_log1(...) { if ((sccp_globals->debug & (DEBUGCAT_FILELINEFUNC)) == DEBUGCAT_FILELINEFUNC) { ast_log(AST_LOG_NOTICE, __VA_ARGS__); } else { ast_verbose(__VA_ARGS__); } }
#define sccp_log(_x) if ((sccp_globals->debug & (_x))) sccp_log1
#define sccp_log_and(_x) if ((sccp_globals->debug & (_x)) == (_x)) sccp_log1

#define GLOB(x) sccp_globals->x

/* Lock Macro for Globals */
#define sccp_globals_lock(x)			pbx_mutex_lock(&sccp_globals->x)
#define sccp_globals_unlock(x)			pbx_mutex_unlock(&sccp_globals->x)
#define sccp_globals_trylock(x)			pbx_mutex_trylock(&sccp_globals->x)

#define DEV_ID_LOG(x) (x && !sccp_strlen_zero(x->id)) ? x->id : "SCCP"

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->tech_pvt)
#else
#define CS_AST_CHANNEL_PVT(x) ((sccp_channel_t*)x->pvt->pvt)
#endif

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT_TYPE(x) x->tech->type
#else
#define CS_AST_CHANNEL_PVT_TYPE(x) x->type
#endif

#ifdef CS_AST_HAS_TECH_PVT
#define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->tech->type, y, strlen(y))
#else
#define CS_AST_CHANNEL_PVT_CMP_TYPE(x,y) !strncasecmp(x->type, y, strlen(y))
#endif

#define CS_AST_CHANNEL_PVT_IS_SCCP(x) CS_AST_CHANNEL_PVT_CMP_TYPE(x,"SCCP")

#ifdef AST_MAX_EXTENSION
#define SCCP_MAX_EXTENSION AST_MAX_EXTENSION
#else
#define SCCP_MAX_EXTENSION 80
#endif
#define SCCP_MAX_AUX 16

#ifdef AST_MAX_CONTEXT
#define SCCP_MAX_CONTEXT AST_MAX_CONTEXT
#else
#define SCCP_MAX_CONTEXT 80
#endif

#ifdef MAX_LANGUAGE
#define SCCP_MAX_LANGUAGE MAX_LANGUAGE
#else
#define SCCP_MAX_LANGUAGE 20
#endif

#undef SCCP_MAX_ACCOUNT_CODE
#ifdef AST_MAX_ACCOUNT_CODE
#define SCCP_MAX_ACCOUNT_CODE AST_MAX_ACCOUNT_CODE
#else
#define SCCP_MAX_ACCOUNT_CODE 50
#endif

#ifdef MAX_MUSICCLASS
#define SCCP_MAX_MUSICCLASS MAX_MUSICCLASS
#else
#define SCCP_MAX_MUSICCLASS 80
#endif

#define SCCP_MAX_HOSTNAME_LEN SCCP_MAX_EXTENSION 
#define SCCP_MAX_MESSAGESTACK 10
#define SCCP_MAX_SOFTKEYSET_NAME 48
#define SCCP_MAX_SOFTKEY_MASK 16
#define SCCP_MAX_SOFTKEY_MODES 16
#define SCCP_MAX_DEVICE_DESCRIPTION 40
#define SCCP_MAX_DEVICE_CONFIG_TYPE 16
#define SCCP_MAX_LABEL SCCP_MAX_EXTENSION
#define SCCP_MAX_BUTTON_OPTIONS 256
#define SCCP_MAX_DEVSTATE_SPECIFIER 256
#define SCCP_MAX_LINE_ID 8
#define SCCP_MAX_LINE_PIN 8
#define SCCP_MAX_SECONDARY_DIALTONE_DIGITS 10
#define SCCP_MAX_DATE_FORMAT 8
#define SCCP_MAX_REALTIME_TABLE_NAME 45

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

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
