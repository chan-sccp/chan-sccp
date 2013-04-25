/*!
 * \file        sccp_refcount.h
 * \brief       SCCP RefCount Header
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_REFCOUNT_H
#define __SCCP_REFCOUNT_H

#include <setjmp.h>

#define REFCOUNT_INDENTIFIER_SIZE 25
enum sccp_refcounted_types {
	SCCP_REF_DEVICE = 1,
	SCCP_REF_LINE,
	SCCP_REF_CHANNEL,
	SCCP_REF_LINEDEVICE,
	SCCP_REF_EVENT,
	SCCP_REF_TEST,
	SCCP_REF_CONFERENCE,
	SCCP_REF_PARTICIPANT,
};

enum sccp_refcount_runstate {
        SCCP_REF_RUNNING = 1,
        SCCP_REF_STOPPED = 0,
        SCCP_REF_DESTROYED = -1
};

typedef struct refcount_object RefCountedObject;

void sccp_refcount_init(void);
void sccp_refcount_destroy(void);
int sccp_refcount_isRunning(void);
int sccp_refcount_schedule_cleanup(const void *data);
void *sccp_refcount_object_alloc(size_t size, enum sccp_refcounted_types type, const char *identifier, void *destructor);
void sccp_refcount_updateIdentifier(void *ptr, char *identifier);
inline void *sccp_refcount_retain(void *ptr, const char *filename, int lineno, const char *func);
inline void *sccp_refcount_release(const void *ptr, const char *filename, int lineno, const char *func);
void sccp_refcount_print_hashtable(int fd);

/* *INDENT-OFF* */
/* Automatically Retain/Release */
#define __GET_WITH_REF1(_dst,_src,_file,_line,_func) 												\
        _dst = _src;																\
        int with_counter_##_line=4;														\
        while (with_counter_##_line-- > 0)													\
                if (3 == with_counter_##_line) {		/* ENTRY */									\
                        if (!_dst || !(_dst = sccp_refcount_retain(_dst,_file,_line,_func))) {							\
                                pbx_log(LOG_NOTICE, "[%s:%d] %s: Failed to retain (%p)\n",  _file,_line,_func,_src);				\
                                with_counter_##_line=0;												\
                                break;														\
                        } else { pbx_log(LOG_NOTICE, "retain  %p, %d, %d\n", _src, _line, with_counter_##_line); }				\
                } else																\
                if (1 == with_counter_##_line) {		/* EXIT */									\
                        if ((_dst = sccp_refcount_release(_dst,_file,_line,_func)) != NULL) {							\
                                pbx_log(LOG_NOTICE, "[%s:%d] %s: Failed to release (%p)\n", _file,_line,_func,_src);				\
                        } else { pbx_log(LOG_NOTICE, "release %p, %d, %d\n", _src, _line, with_counter_##_line); }				\
                        break;															\
                } else          				/* DO INBETWEEN*/

#define __GET_WITH_REF(_dst,_src,_file,_line,_func) __GET_WITH_REF1(_dst,_src,_file,_line,_func)
#define GETWITHREF(_dst,_src) __GET_WITH_REF(_dst,_src,__FILE__,__LINE__,__PRETTY_FUNCTION__)

/* Call with_get_ref after creating unique local variable to use during retain/release */
#define __TOKENPASTE(x, y) x ## y
#define __TOKENPASTE2(x, y) __TOKENPASTE(x, y)
#define __WITH_REF(_src,_file,_line,_func) 													\
        typeof(_src) __TOKENPASTE(sccp_with_ref_,_line);											\
        __GET_WITH_REF(__TOKENPASTE(sccp_with_ref_,_line),_src,_file,_line,_func)
#define WITHREF(_src) __WITH_REF(_src,__FILE__,__LINE__,__PRETTY_FUNCTION__)

/* *INDENT-ON* */
#endif														// __SCCP_REFCOUNT_H
