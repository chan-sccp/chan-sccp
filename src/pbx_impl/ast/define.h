/*!
 * \file        pbx_impl/ast/define.h
 * \brief       SCCP PBX Redefinition Asterisk Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#include "config.h"

// type redefinitions
#define pbx_variable ast_variable
#define pbx_context ast_context
#define pbx_module ast_module
// sccp redefinitions
#define sccp_malloc ast_malloc
#define sccp_calloc ast_calloc
#define sccp_realloc ast_realloc
//#define sccp_strdup ast_strdup
#define sccp_free(_x) {ast_free((void *)(_x)); (_x) = NULL; }
#define sccp_asprintf ast_asprintf
#define sccp_vasprintf ast_vasprintf
#define sccp_calloc_cache ast_calloc_cache
#define sccp_alloca(_size) __builtin_alloca((_size))
#define CS_BRIDGEPEERNAME "BRIDGEPEER"

#if !defined(SCANBUILD) && defined(DEV_MODE)
#define pbx_assert ast_assert
#else
#define pbx_assert assert
#endif

/* Lock Macro's */
#define sccp_mutex_init(_x)          		pbx_mutex_init((_x))
#define sccp_mutex_destroy(_x)       		pbx_mutex_destroy((_x))
#define sccp_mutex_lock(_x)			pbx_mutex_lock((_x))
#define sccp_mutex_lock_desc(_x,_y) 		pbx_mutex_lock_desc((_x),(_y))
#define sccp_mutex_unlock(_x)			pbx_mutex_unlock((_x))
#define sccp_mutex_trylock(_x)			pbx_mutex_trylock((_x))
#ifdef SCOPED_LOCK
#	undef SCOPED_LOCK
#endif
#define SCOPED_LOCK(varname, lock, lockfunc, unlockfunc) \
	RAII(typeof((lock)), varname, ({                 \
		     lockfunc((lock));                   \
		     (lock);                             \
	     }),                                         \
	     unlockfunc)
#ifdef SCOPED_MUTEX
#	undef SCOPED_MUTEX
#endif
#define SCOPED_MUTEX(varname, lock) SCOPED_LOCK(varname, (lock), pbx_mutex_lock, pbx_mutex_unlock)
#ifdef SCOPED_RDLOCK
#	undef SCOPED_RDLOCK
#endif
#define SCOPED_RDLOCK(varname, lock) SCOPED_LOCK(varname, (lock), pbx_rwlock_rdlock, pbx_rwlock_unlock)
#ifdef SCOPED_WRLOCK
#	undef SCOPED_WRLOCK
#endif
#define SCOPED_WRLOCK(varname, lock) SCOPED_LOCK(varname, (lock), pbx_rwlock_wrlock, pbx_rwlock_unlock)

// SCCP_FILE_VERSION definition
#if defined(LOW_MEMORY)
#  define SCCP_FILE_VERSION(_file, _version)
#else
#  if CS_AST_REGISTER_FILE_VERSION
#    define SCCP_FILE_VERSION(_file, _version) ASTERISK_FILE_VERSION((_file), (_version))
#  else
#    if ASTERISK_VERSION_GROUP >= 114
#      define SCCP_FILE_VERSION(_file, _version)
#    else
#      define SCCP_FILE_VERSION(_file, _version) ASTERISK_REGISTER_FILE()
#    endif
#  endif
#endif

// codec / format redefinitions
#define pbx_codec_pref_index ast_codec_pref_index
#define pbx_codec_pref_getsize ast_codec_pref_getsize
#define pbx_codec2str ast_codec2str

// application / module / cli redefinitions
#define pbx_channel_register ast_channel_register
#define pbx_channel_register_ex ast_channel_register_ex
#define pbx_channel_unregister ast_channel_unregister

// general redefinitions
#define pbx_check_hangup ast_check_hangup
#define pbx_check_hangup_locked ast_check_hangup_locked
#define pbx_channel_lock ast_channel_lock
#define pbx_channel_unlock ast_channel_unlock
#define pbx_sched_wait ast_sched_wait
#define pbx_io_wait ast_io_wait
#define pbx_gethostbyname ast_gethostbyname
#define pbx_get_version ast_get_version
#define pbx_apply_ha ast_apply_ha
#define pbx_moh_stop ast_moh_stop
#define pbx_channel_alloc ast_channel_alloc
#define pbx_null_frame ast_null_frame
#define pbx_jb_configure ast_jb_configure
#define pbx_rtp_proto_register ast_rtp_proto_register
#define pbx_rtp_proto_unregister ast_rtp_proto_unregister
#define pbx_rtp_glue_register ast_rtp_glue_register
#define pbx_rtp_glue_unregister ast_rtp_glue_unregister
#define pbx_add_extension ast_add_extension
#define pbx_app_has_voicemail ast_app_has_voicemail
#define pbx_app_inboxcount ast_app_inboxcount
#define pbx_app_separate_args ast_app_separate_args
#define pbx_bridge_features_init ast_bridge_features_init

#if ASTERISK_VERSION_NUMBER < 11010
#define pbx_bridge_join(_bridge, _channel, _replace, _features, _tech_optimizations, _flags) ast_bridge_join((_bridge), (_channel), (_replace), (_features))
#else
#ifndef CS_BRIDGE_JOIN_PASSREFERENCE
#define pbx_bridge_join(_bridge, _channel, _replace, _features, _tech_optimizations, _flags) ast_bridge_join((_bridge), (_channel), (_replace), (_features), (_tech_optimizations))
#else
#define pbx_bridge_join(_bridge, _channel, _replace, _features, _tech_optimizations, _flags) ast_bridge_join((_bridge), (_channel), (_replace), (_features), (_tech_optimizations), (_flags))
#endif
#endif

#ifndef CS_BRIDGE_BASE_NEW
#define pbx_bridge_new(_a,_b,_c,_d,_e) ast_bridge_new((_a), (_b))
#else
#define pbx_bridge_new(_a,_b,_c,_d,_e) ast_bridge_base_new((_a), (_b), (_c), (_d), (_e))
#endif
#define pbx_bridge_remove ast_bridge_remove
#define pbx_bridge_kick ast_bridge_kick
#define pbx_bridge_result ast_bridge_result

#if ASTERISK_VERSION_GROUP < 110
#define pbx_bridge_impart(_bridge, _underlying_channel, _swap_channel, _features, _allow_hangup) ast_bridge_impart((_bridge), (_underlying_channel), (_swap_channel), (_features))
#else
#define pbx_bridge_impart(_bridge, _underlying_channel, _swap_channel, _features, _allow_hangup) ast_bridge_impart((_bridge), (_underlying_channel), (_swap_channel), (_features), (_allow_hangup))
#endif
#ifndef CS_BRIDGE_DEPART_ONLY_CHANNEL
#define pbx_bridge_depart(_x, _y) ast_bridge_depart((_x),(_y))
#else
#define pbx_bridge_depart(_x, _y) ast_bridge_depart((_y))
#endif

#define pbx_bridge_destroy(_x, _y) ast_bridge_destroy((_x))
#define pbx_bridge_features_cleanup ast_bridge_features_cleanup
#define pbx_bridge_change_state ast_bridge_change_state
#define pbx_bridge_lock ast_bridge_lock
#define pbx_bridge_unlock ast_bridge_unlock
#define pbx_bridge_suspend ast_bridge_suspend
#define pbx_bridge_unsuspend ast_bridge_unsuspend

#define pbx_request ast_request
#define pbx_build_string ast_build_string
#define pbx_callerid_parse ast_callerid_parse
#define pbx_calloc ast_calloc
#define pbx_canmatch_extension ast_canmatch_extension
#define pbx_category_browse ast_category_browse
#define pbx_cause ast_cause
#if ASTERISK_VERSION_GROUP < 112
#define pbx_ama_flags_type long int 
#else
#define pbx_ama_flags_type enum ama_flags
#endif
#define pbx_cdr_amaflags2int ast_cdr_amaflags2int
#define pbx_cdr_flags2str ast_cdr_flags2str
#define pbx_channel_amaflags2string ast_cdr_flags2str
#define pbx_channel_string2amaflag ast_cdr_amaflags2int
#define pbx_cdr_start ast_cdr_start
#define pbx_cdr_update ast_cdr_update
#define pbx_channel_defer_dtmf ast_channel_defer_dtmf
#define pbx_channel_get_by_name ast_channel_get_by_name
#define pbx_channel_iterator_all_new ast_channel_iterator_all_new
#define pbx_channel_iterator_destroy ast_channel_iterator_destroy
#define pbx_channel_lock ast_channel_lock
#define pbx_channel_masquerade ast_channel_masquerade
#define pbx_channel_queue_connected_line_update ast_channel_queue_connected_line_update
#define pbx_channel_register ast_channel_register
#define pbx_channel_search_locked ast_channel_search_locked
#define pbx_channel_trylock ast_channel_trylock
#define pbx_channel_undefer_dtmf ast_channel_undefer_dtmf
#define pbx_channel_unregister ast_channel_unregister

#define pbx_cli ast_cli
#define pbx_cli_entry ast_cli_entry
#define pbx_cli_register ast_cli_register
#define pbx_cli_unregister ast_cli_unregister
#define pbx_codec_pref_string ast_codec_pref_string
#define pbx_cond_t ast_cond_t
#define pbx_cond_broadcast ast_cond_broadcast
#define pbx_cond_destroy ast_cond_destroy
#define pbx_cond_init ast_cond_init
#define pbx_cond_signal ast_cond_signal
#define pbx_cond_timedwait(_cond, _mutex,_ts) ast_cond_timedwait((_cond), (ast_mutex_t*)(_mutex), (_ts))
#define pbx_cond_wait(_cond, _mutex) ast_cond_wait((_cond), (ast_mutex_t*)(_mutex))
#define pbx_config_destroy ast_config_destroy
#define pbx_copy_string ast_copy_string
#define pbx_custom_function ast_custom_function
#define pbx_db_freetree ast_db_freetree

#if CS_CACHEABLE_DEVICESTATE
#define pbx_devstate_changed(_a,_b,_c) ast_devstate_changed((_a),AST_DEVSTATE_CACHABLE,(_b),(_c))
#define pbx_devstate_changed_literal(_a,_b) ast_devstate_changed_literal((_a),AST_DEVSTATE_CACHABLE,(_b))
#elif defined(CS_DEVICESTATE)
#define pbx_devstate_changed(_a,_b,_c) ast_devstate_changed((_a),(_b),(_c))
#define pbx_devstate_changed_literal(_a,_b) ast_devstate_changed_literal((_a),(_b))
#else
#define pbx_devstate_changed(_a,_b,_c) ast_device_state_changed((_b),(_c))
#define pbx_devstate_changed_literal(_a,_b) ast_device_state_changed_literal((_b))
#endif

#if defined(CS_AST_ENABLE_DISTRIBUTED_DEVSTATE)
#define pbx_enable_distributed_devstate() ast_enable_distributed_devstate()
#else
#define pbx_enable_distributed_devstate()
#endif

#define pbx_devstate_val ast_devstate_val
#define pbx_digit_usecs ast_digit_usecs
#define pbx_do_masquerade ast_do_masquerade
typedef struct ast_event pbx_event_t;

#define pbx_event_get_ie_str ast_event_get_ie_str
#define pbx_event_get_ie_uint ast_event_get_ie_uint
#define pbx_event_new ast_event_new
/*
#if defined( CS_AST_HAS_STASIS )
#define pbx_event_sub stasis_subscription
#else
#define pbx_event_sub ast_event_sub
#endif
*/
#if defined( HAVE_PBX_STASIS_H )
#define pbx_event_sub stasis_subscription
#define pbx_mwi_state_type ast_mwi_state_type
#else
#define pbx_event_sub ast_event_sub
#endif
typedef struct pbx_event_sub pbx_event_subscription_t;
#define pbx_mwi_topic                ast_mwi_topic
#define pbx_mwi_subscribe_pool       ast_mwi_subscribe_pool
#define pbx_mwi_unsubscribe_and_join ast_mwi_unsubscribe_and_join
#define pbx_mwi_subscriber           ast_mwi_subscriber
#define pbx_event_subscribe ast_event_subscribe
#define pbx_event_unsubscribe ast_event_unsubscribe
#define pbx_event_queue_and_cache ast_event_queue_and_cache
#define pbx_exists_extension ast_exists_extension
#define pbx_extension_state_add ast_extension_state_add
#define pbx_extension_state_del ast_extension_state_del
#define pbx_extension_state ast_extension_state
#define pbx_event_get_cached ast_event_get_cached
#define pbx_event_destroy ast_event_destroy
#define pbx_get_group ast_get_group
#define pbx_get_hint ast_get_hint
#define pbx_gethostbyname ast_gethostbyname
#define pbx_ignore_pattern ast_ignore_pattern
#define pbx_indicate ast_indicate
#define pbx_io_wait ast_io_wait
#define pbx_jb_read_conf ast_jb_read_conf
#define pbx_load_realtime ast_load_realtime
#define pbx_log ast_log
#define pbx_malloc ast_malloc
#define pbx_manager_register_xml ast_manager_register_xml
#define pbx_manager_unregister ast_manager_unregister
#define pbx_matchmore_extension ast_matchmore_extension
#define pbx_moh_stop ast_moh_stop

//#define pbx_mutex_t ast_mutex_t
#define pbx_mutex_destroy(_x) ast_mutex_destroy((ast_mutex_t *)(_x))
#define pbx_mutex_init(_x) ast_mutex_init((ast_mutex_t *)(_x))
#define pbx_mutex_init_notracking(_x) ast_mutex_init_notracking((ast_mutex_t *)(_x))

//#define pbx_rwlock_t ast_rwlock_t
#define pbx_rwlock_destroy(_x) ast_rwlock_destroy((ast_rwlock_t *)(_x))
#define pbx_rwlock_init(_x) ast_rwlock_init((ast_rwlock_t *)(_x))
#define pbx_rwlock_init_notracking(_x) ast_rwlock_init_notracking((ast_rwlock_t *)(_x))

#if CS_LOCK_DEBUG
#define pbx_mutex_lock(_x) {ast_debug(4, "[%d] %s:%d (%s) MUTEX_LOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_mutex_lock((ast_mutex_t *)(_x));}
#define pbx_mutex_trylock(_x) {ast_debug(4, "[%d] %s:%d (%s) MUTEX_TRYLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_mutex_trylock((ast_mutex_t *)(_x));}
#define pbx_mutex_unlock(_x) {ast_mutex_unlock((ast_mutex_t *)_x); ast_debug(4, "[%d] %s:%d (%s) MUTEX_UNLOCK: " #(_x) ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x));}
#define pbx_rwlock_rdlock(_x) {ast_debug(5, "[%d] %s:%d (%s) RWLOCK_RDLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_rwlock_rdlock((ast_rwlock_t *)(_x));}
#define pbx_rwlock_wrlock(_x) {ast_debug(5, "[%d] %s:%d (%s) RWLOCK_WRLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_rwlock_wrlock((ast_rwlock_t *)(_x));}
#define pbx_rwlock_tryrdlock(_x) {ast_debug(5, "[%d] %s:%d (%s) RWLOCK_TRYRDLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_rwlock_tryrdlock((ast_rwlock_t *)(_x));}
#define pbx_rwlock_trywrlock(_x) {ast_debug(5, "[%d] %s:%d (%s) RWLOCK_TRYWRLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x)); ast_rwlock_trywrlock((ast_rwlock_t *)(_x));}
#define pbx_rwlock_unlock(_x) {ast_rwlock_unlock((ast_rwlock_t *)(_x)); ast_debug(5, "[%d] %s:%d (%s) RWLOCK_UNLOCK: " #_x ": %p\n", (unsigned int) pthread_self(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (_x));}
#else
#define pbx_mutex_lock(_x) ({ast_mutex_lock((ast_mutex_t *)(_x));})
#define pbx_mutex_trylock(_x) ({ast_mutex_trylock((ast_mutex_t *)(_x));})
#define pbx_mutex_unlock(_x) ({ast_mutex_unlock((ast_mutex_t *)(_x));})
#define pbx_rwlock_rdlock(_x) ({ast_rwlock_rdlock((ast_rwlock_t *)(_x));})
#define pbx_rwlock_wrlock(_x) ({ast_rwlock_wrlock((ast_rwlock_t *)(_x));})
#define pbx_rwlock_tryrdlock(_x) ({ast_rwlock_tryrdlock((ast_rwlock_t *)(_x));})
#define pbx_rwlock_trywrlock(_x) ({ast_rwlock_trywrlock((ast_rwlock_t *)(_x));})
#define pbx_rwlock_unlock(_x) ({ast_rwlock_unlock((ast_rwlock_t *)(_x));})
#endif

#define pbx_park_call ast_park_call
#define pbx_party_name_free ast_party_name_free
#define pbx_party_number_free ast_party_number_free
#define pbx_pbx_run ast_pbx_run
// #define pbx_pbx_start ast_pbx_start
#define pbx_poll ast_poll
#define pbx_print_group ast_print_group
#define pbx_print_namedgroup ast_print_namedgroup
#define pbx_pthread_create ast_pthread_create
#define pbx_pthread_create_background ast_pthread_create_background
#define pbx_pthread_create_detached_background ast_pthread_create_detached_background
#define pbx_pthread_mutex_lock ast_pthread_mutex_lock
#define pbx_pthread_mutex_trylock ast_pthread_mutex_trylock
#define pbx_pthread_mutex_unlock ast_pthread_mutex_unlock
#define __pbx_pthread_mutex_unlock __ast_pthread_mutex_unlock
#define __pbx_pthread_mutex_lock __ast_pthread_mutex_lock
#define __pbx_pthread_mutex_trylock __ast_pthread_mutex_trylock
#define pbx_queue_hangup ast_queue_hangup
#define pbx_random ast_random
#define pbx_realloc ast_realloc
#define pbx_rtp_bridge ast_rtp_bridge
#define pbx_rtp_codecs_payloads_set_rtpmap_type_rate ast_rtp_codecs_payloads_set_rtpmap_type_rate
#define pbx_rtp_glue_register ast_rtp_glue_register
#define pbx_rtp_glue_result ast_rtp_glue_result
#define pbx_rtp_glue_unregister ast_rtp_glue_unregister
#define pbx_rtp_instance_bridge ast_rtp_instance_bridge
#define pbx_rtp_instance_change_source ast_rtp_instance_change_source
#define pbx_rtp_instance_destroy ast_rtp_instance_destroy
#define pbx_rtp_instance_fd ast_rtp_instance_fd
#define pbx_rtp_instance_get_codecs ast_rtp_instance_get_codecs
#define pbx_rtp_instance_get_local_address ast_rtp_instance_get_local_address
#define pbx_rtp_instance_get_remote_address ast_rtp_instance_get_remote_address
#define pbx_rtp_instance_new ast_rtp_instance_new
#define pbx_rtp_instance_read ast_rtp_instance_read
#define pbx_rtp_instance_set_prop ast_rtp_instance_set_prop
#define pbx_rtp_instance_set_qos ast_rtp_instance_set_qos
#define pbx_rtp_instance_set_remote_address ast_rtp_instance_set_remote_address
#define pbx_rtp_instance_stop ast_rtp_instance_stop
#define pbx_rtp_instance_update_source ast_rtp_instance_update_source
#define pbx_rtp_instance_write ast_rtp_instance_write
#define pbx_rtp_set_vars ast_rtp_set_vars
#define pbx_safe_sleep ast_safe_sleep
#define pbx_sched_add ast_sched_add
#define pbx_sched_del ast_sched_del
#define pbx_sched_runq ast_sched_runq
#define pbx_sched_wait ast_sched_wait
#define pbx_sched_when ast_sched_when
#define pbx_set_callerid ast_set_callerid
#define pbx_setcallstate ast_setcallstate
#define pbx_set_read_format ast_set_read_format
#define pbx_setstate ast_setstate
#define pbx_set_write_format ast_set_write_format
#define pbx_skip_blanks ast_skip_blanks
#define pbx_skip_nonblanks ast_skip_nonblanks
#define pbx_sockaddr_from_sin ast_sockaddr_from_sin
#define pbx_sockaddr_port ast_sockaddr_port
#define pbx_sockaddr_to_sin ast_sockaddr_to_sin
#define pbx_state2str ast_state2str
#define pbx_str_t struct ast_str
#define pbx_str_create ast_str_create
#define pbx_str_alloca(init_len)							\
({											\
	pbx_str_t *__pbx_str_buf;							\
	__pbx_str_buf = (pbx_str_t *)ast_alloca(sizeof(*__pbx_str_buf) + (init_len));	\
	__pbx_str_buf->__AST_STR_LEN = (init_len);					\
	__pbx_str_buf->__AST_STR_USED = 0;						\
	__pbx_str_buf->__AST_STR_TS = DS_ALLOCA;					\
	__pbx_str_buf->__AST_STR_STR[0] = '\0';						\
	(__pbx_str_buf);								\
})
#define pbx_str_append ast_str_append
#define pbx_str_reset ast_str_reset
#define pbx_str_strlen ast_str_strlen
#define pbx_str_size ast_str_size
#define pbx_str_thread_get ast_str_thread_get
#define pbx_str_append_escapecommas ast_str_append_escapecommas
#define pbx_str_buffer ast_str_buffer
#define PBX_THREADSTORAGE AST_THREADSTORAGE
#define pbx_strdup ast_strdup
#define pbx_strdupa ast_strdupa
#define pbx_stream_and_wait ast_stream_and_wait
#define pbx_say_number ast_say_number
#define pbx_fileexists ast_fileexists
#if ASTERISK_VERSION_GROUP < 111 && defined(__clang__)
#undef AST_STRING_FIELD_ALLOCATION
#define AST_STRING_FIELD_ALLOCATION(_x) *((ast_string_field_allocation *) ((ast_string_field_allocation)(_x) - __alignof__(ast_string_field_allocation)))
#endif
#define pbx_string_field_build ast_string_field_build
#define pbx_string_field_set ast_string_field_set
#define pbx_strip ast_strip
#define pbx_trim_blanks ast_trim_blanks
#define pbx_true ast_true
#define pbx_false ast_false
#define pbx_tvnow ast_tvnow
#if CS_AST_REGISTER_FILE_VERSION
#define pbx_register_file_version ast_register_file_version
#define pbx_unregister_file_version ast_unregister_file_version
#endif
#define pbx_update_use_count ast_update_use_count
#define pbx_variable_browse ast_variable_browse
#define pbx_variable_new ast_variable_new
#define pbx_variable_retrieve ast_variable_retrieve
#define pbx_variables_destroy ast_variables_destroy
#define pbx_strlen_zero ast_strlen_zero
#define pbx_context_find ast_context_find
#if CS_AST_HAS_CONTEXT_DESTROY_BY_NAME
#define pbx_context_destroy_by_name ast_context_destroy_by_name
#else
#define pbx_context_destroy_by_name ast_context_destroy
#endif
#define pbx_context_destroy ast_context_destroy
#define PBX_CONTEXT_TYPE struct ast_context
#define pbx_hangup ast_hangup
#define pbx_atomic_fetchadd_int ast_atomic_fetchadd_int
#define pbx_clear_flag ast_clear_flag
#define pbx_set_flag ast_set_flag
#define pbx_test_flag ast_test_flag
#define pbx_set2_flag ast_set2_flag

// Fixes for asterisk-trunk, need to sorted later
#define pbx_channel_flags(_a) (_a)										/* needed in asterisk trunk */
#define pbx_channel_uniqueid(_a) (_a)->uniqueid
#define pbx_channel_call_forward(_a) (_a)->call_forward
#define pbx_channel_appl(_a) (_a)->appl
#define pbx_channel_state(_a) (_a)->_state
#define pbx_channel_pbx(_a) (_a)->pbx
#define pbx_channel_hangupcause(_a) (_a)->hangupcause
#define pbx_channel_set_hangupcause(_a,_b) (_a)->hangupcause = (_b)
#define pbx_channel_softhangup(_a) (_a)->_softhangup
#define pbx_channel_context(_a) (_a)->context
#define pbx_channel_nativeformats(_a) (_a)->nativeformats
#define pbx_channel_exten(_a) (_a)->exten
#define pbx_channel_priority(_a) (_a)->priority
#define pbx_channel_macroexten(_a) (_a)->macroexten
#define pbx_channel_macrocontext(_a) (_a)->macrocontext
#define pbx_channel_dialcontext(_a) (_a)->dialcontext
#define pbx_channel_callgroup(_a) (_a)->callgroup
#define pbx_channel_masq(_a) (_a)->masq
#define pbx_channel_setwhentohangup_tv(_a, _b) (_a)->whentohangup = (_b)
#define pbx_channel_blocker(_a) (_a)->blocker
#define pbx_channel_blockproc(_a) (_a)->blockproc
#define pbx_channel_tech(_a) (_a)->tech
#define pbx_channel_bridge(_a) (_a)->bridge
#define pbx_channel_set_bridge(_a,_b) (_a)->bridge = (_b)
#define pbx_channel_language(_a) (_a)->language
#define pbx_channel_cdr(_a) (_a)->cdr
#define pbx_channel_call_forward_set(_a,_b) ast_string_field_set((_a), call_forward, (_b));
#define pbx_channel_varshead(_a) &(_a)->varshead
#define pbx_pickup_call ast_pickup_call
#define pbx_channel_monitor(_a) (_a)->monitor

#define pbx_bridge_change_state ast_bridge_change_state
#define pbx_channel_redirecting_effective_from(_a) (_a)->redirecting.from
#define pbx_channel_redirecting_effective_to(_a) (_a)->redirecting.to
#define pbx_channel_redirecting_effective_orig(_a) (_a)->redirecting.orig
#define pbx_channel_connected_id(_a) (_a)->connected.id
#define pbx_channel_connected_source(_a) (_a)->connected.source
#define pbx_connected_line_source_name(_a) ast_connected_line_source_name(_a)

// typedef struct ast_cc_config_params pbx_cc_config_params_t;
#define pbx_cc_config_params             ast_cc_config_params
#define pbx_cc_config_params_init        ast_cc_config_params_init
#define pbx_channel_cc_params_init       ast_channel_cc_params_init
#define pbx_cc_default_config_params     ast_cc_default_config_params
#define pbx_cc_copy_config_params        ast_cc_copy_config_params
#define pbx_set_cc_agent_policy          ast_set_cc_agent_policy
#define pbx_cc_is_config_param           ast_cc_is_config_param
#define pbx_cc_set_param                 ast_cc_set_param
#define pbx_cc_config_params_destroy     ast_cc_config_params_destroy
#define pbx_channel_get_cc_config_params ast_channel_get_cc_config_params
#define pbx_queue_cc_frame               ast_queue_cc_frame
#define pbx_cc_service_t                 enum ast_cc_service_type
#define pbx_channel_get_device_name      ast_channel_get_device_name
#define PBX_CHANNEL_NAME                 AST_CHANNEL_NAME

#define pbx_streamfile ast_streamfile
#if CS_TEST_FRAMEWORK
#define pbx_test_validate_cleanup ast_test_validate_cleanup
#define pbx_test_status_update ast_test_status_update
#ifdef SCANBUILD
#define pbx_test_validate(_test,_assertion) assert((_assertion));
#else
#define pbx_test_validate ast_test_validate
#endif
#endif
#define pbx_channel_callid ast_channel_callid
#if !defined(CS_AST_CHANNEL_CALLID_TYPEDEF)
#	define pbx_callid_t                           struct ast_callid *
#	define pbx_channel_callid_set(_chan, _callid) ast_channel_callid_set(_chan, (struct ast_callid *)_callid)
#	define pbx_callid_threadassoc_add(_callid)    ast_callid_threadassoc_add((struct ast_callid *)_callid)
#else
#	define pbx_callid_t                           ast_callid
#	define pbx_channel_callid_set(_chan, _callid) ast_channel_callid_set(_chan, (ast_callid)_callid)
#	define pbx_callid_threadassoc_add(_callid)    ast_callid_threadassoc_add((ast_callid)_callid)
#endif
#define pbx_create_callid             (pbx_callid_t) ast_create_callid
#define pbx_callid_threadassoc_remove ast_callid_threadassoc_remove

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
