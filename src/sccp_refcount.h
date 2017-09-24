/*!
 * \file        sccp_refcount.h
 * \brief       SCCP RefCount Header
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

#include "sccp_cli.h"

/* forward declarations */
struct mansession;
struct message;
typedef struct ast_str pbx_str_t;

__BEGIN_C_EXTERN__
#define REFCOUNT_INDENTIFIER_SIZE 32
enum sccp_refcounted_types {
	SCCP_REF_PARTICIPANT = 1,
	SCCP_REF_CONFERENCE,
	SCCP_REF_EVENT,
	SCCP_REF_CHANNEL,
	SCCP_REF_LINEDEVICE,
	SCCP_REF_LINE,
	SCCP_REF_DEVICE,
#if CS_TEST_FRAMEWORK
	SCCP_REF_TEST,
#endif
};

enum sccp_refcount_runstate {
	SCCP_REF_RUNNING = 1,
	SCCP_REF_STOPPED = 0,
	SCCP_REF_DESTROYED = -1
};

SCCP_API void SCCP_CALL sccp_refcount_init(void);
SCCP_API void SCCP_CALL sccp_refcount_destroy(void);
SCCP_API int __PURE__ SCCP_CALL sccp_refcount_isRunning(void);
SCCP_API void * SCCP_CALL  const sccp_refcount_object_alloc(size_t size, enum sccp_refcounted_types type, const char *identifier, void *destructor);
SCCP_API void SCCP_CALL sccp_refcount_updateIdentifier(const void * const ptr, const char * const identifier);
SCCP_API void * SCCP_CALL  const sccp_refcount_retain(const void * const ptr, const char *filename, int lineno, const char *func);
SCCP_API void * SCCP_CALL  const sccp_refcount_release(const void * * const ptr, const char *filename, int lineno, const char *func);
SCCP_API void SCCP_CALL sccp_refcount_replace(const void * * const replaceptr, const void *const newptr, const char *filename, int lineno, const char *func);
SCCP_API int SCCP_CALL sccp_show_refcount(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
SCCP_API void SCCP_CALL sccp_refcount_autorelease(void *ptr);
#if CS_REFCOUNT_DEBUG
SCCP_API void SCCP_CALL sccp_refcount_addWeakParent(const void * const ptr, const void * const parentWeakPtr);
SCCP_API void SCCP_CALL sccp_refcount_removeWeakParent(const void * const ptr, const void * const parentWeakPtr);
SCCP_API void SCCP_CALL sccp_refcount_gen_report(const void * const ptr, pbx_str_t **buf);
#endif

typedef struct {
	const void ** const ptr;
	const char *file;
	const char *func;
	int line;
} auto_ref_t;

#define AUTO_RELEASE2(_type,_var,_initial,_file,_func,_line,_counter) \
_type *_var = _initial; auto_ref_t __attribute__((cleanup(sccp_refcount_autorelease),unused)) ref##_counter = {(const void **const)&_var, _file,_func,_line}
#define AUTO_RELEASE1(_type,_var,_initial,_file,_func,_line,_counter) AUTO_RELEASE2(_type,_var,_initial,_file,_func,_line,_counter)
#define AUTO_RELEASE(_type,_var,_initial) AUTO_RELEASE1(_type,_var,_initial,__FILE__,__PRETTY_FUNCTION__,__LINE__,__COUNTER__)

#ifdef CS_EXPERIMENTAL
SCCP_API int SCCP_CALL sccp_refcount_force_release(long findobj, char *identifier);
#endif

#define sccp_refcount_retain_type(_type, _x) 		({											\
	pbx_assert(PTR_TYPE_CMP(const _type *const, _x ) == 1); 										\
	sccp_refcount_retain(_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);									\
})
#define sccp_refcount_release_type(_type,_x)		({											\
	pbx_assert(PTR_TYPE_CMP(_type * *const, _x ) == 1);		 									\
	sccp_refcount_release((const void ** const)_x, __FILE__, __LINE__, __PRETTY_FUNCTION__);						\
})
#define sccp_refcount_refreplace_type(_type,_x, _y) 	({											\
	pbx_assert(PTR_TYPE_CMP(_type * *const, _x) == 1 && PTR_TYPE_CMP(const _type *const, _y) == 1);						\
	sccp_refcount_replace((const void ** const)_x, _y, __FILE__, __LINE__, __PRETTY_FUNCTION__);						\
})

__END_C_EXTERN__

#if 0 /* UNUSED */
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


/* example use */
#if 0
	sccp_device_t *d = NULL;
	WITHREF(channel) {
		GETWITHREF(d, channel->privateData->device) {
			channel->privateData->microphone = enabled;
			pbx_log(LOG_NOTICE, "Within retain section\n");
			if (enabled) {
				channel->isMicrophoneEnabled = sccp_always_true;
				if ((channel->rtp.audio.mediaTransmissionState & SCCP_RTP_STATUS_ACTIVE)) {
					sccp_dev_set_microphone(d, SKINNY_STATIONMIC_ON);
				}
			} else {
				channel->isMicrophoneEnabled = sccp_always_false;
				if ((channel->rtp.audio.mediaTransmissionState & SCCP_RTP_STATUS_ACTIVE)) {
					sccp_dev_set_microphone(d, SKINNY_STATIONMIC_OFF);
				}
			}
		}
	}
#endif

#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off
