/*!
 * \file 	define.h
 * \brief 	SCCP PBX Redefinition Asterisk Header
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#ifndef SCCP_AST_DEFINE_H_
#    define SCCP_AST_DEFINE_H_
// type redefinitions
#    define pbx_variable ast_variable
#    define pbx_context ast_context

// sccp redefinitions
#    ifdef HAVE_LIBGC
#        undef malloc
#        undef calloc
#        undef realloc
#        undef strdup
#        define malloc(x) GC_MALLOC(x)
#        define calloc(n,x) GC_MALLOC((n)*(x))
#        define realloc(p,x) GC_REALLOC((p),(x))
#        define strdup(s) GC_STRDUP((s))
#        define sccp_malloc(x) GC_MALLOC(x)
#        define sccp_calloc(n,x) GC_MALLOC((n)*(x))
#        define sccp_realloc(p,x) GC_REALLOC((p),(x))
#        define sccp_strdup(s) GC_STRDUP((s))
#        undef free
#        undef sccp_free
#        if DEBUG > 0
#            define free(x) { (x) = NULL; GC_FREE(x); }
#            define sccp_free(x) { (x) = NULL; GC_FREE(x); }
#        else
#            define free(x) { (x) = NULL; }
#            define sccp_free(x) { (x) = NULL; }
#        endif
#    else
#        define sccp_malloc ast_malloc
#        define sccp_calloc ast_calloc
#        define sccp_realloc ast_realloc
#        define sccp_strdup ast_strdup
#        define sccp_free ast_free
#    endif
#    define sccp_asprintf ast_asprintf
#    define sccp_vasprintf ast_vasprintf
#    define sccp_calloc_cache ast_calloc_cache

// codec / format redefinitions
#    define pbx_codec_pref_index ast_codec_pref_index
#    define pbx_codec_pref_getsize ast_codec_pref_getsize
#    define pbx_codec2str ast_codec2str

// application / module / cli redefinitions
#    define pbx_register_application ast_register_application2
#    define pbx_unregister_application ast_unregister_application
#    define pbx_custom_function_register __ast_custom_function_register
#    define pbx_custom_function_unregister ast_custom_function_unregister
#    define pbx_channel_register ast_channel_register
#    define pbx_channel_register_ex ast_channel_register_ex
#    define pbx_channel_unregister ast_channel_unregister

// general redefinitions
#    define pbx_check_hangup ast_check_hangup
#    define pbx_channel_lock ast_channel_lock
#    define pbx_channel_unlock ast_channel_unlock
#    define pbx_sched_wait ast_sched_wait
#    define pbx_io_wait ast_io_wait
#    define pbx_gethostbyname ast_gethostbyname
#    define pbx_apply_ha ast_apply_ha
#    define pbx_pickup_ext ast_pickup_ext
#    define pbx_moh_stop ast_moh_stop
#    define pbx_channel_alloc ast_channel_alloc
#    define pbx_null_frame ast_null_frame
#    define pbx_jb_configure ast_jb_configure
#    define pbx_rtp_proto_register ast_rtp_proto_register
#    define pbx_rtp_proto_unregister ast_rtp_proto_unregister
#    define pbx_rtp_glue_register ast_rtp_glue_register
#    define pbx_rtp_glue_unregister ast_rtp_glue_unregister
#    define pbx_add_extension ast_add_extension
#    define pbx_app_has_voicemail ast_app_has_voicemail
#    define pbx_app_inboxcount ast_app_inboxcount
#    define pbx_app_separate_args ast_app_separate_args
#    define pbx_bridge_features_init ast_bridge_features_init
#    define pbx_bridge_join ast_bridge_join
#    define pbx_bridge_new ast_bridge_new
#    define pbx_bridge_remove ast_bridge_remove
#    define pbx_bridge_result ast_bridge_result
#    define pbx_build_string ast_build_string
#    define pbx_callerid_parse ast_callerid_parse
#    define pbx_calloc ast_calloc
#    define pbx_canmatch_extension ast_canmatch_extension
#    define pbx_category_browse ast_category_browse
#    define pbx_cause ast_cause
#    define pbx_cdr_amaflags2int ast_cdr_amaflags2int
#    define pbx_cdr_flags2str ast_cdr_flags2str
#    define pbx_channel_defer_dtmf ast_channel_defer_dtmf
#    define pbx_channel_get_by_name ast_channel_get_by_name
#    define pbx_channel_iterator_all_new ast_channel_iterator_all_new
#    define pbx_channel_iterator_destroy ast_channel_iterator_destroy
#    define pbx_channel_lock ast_channel_lock
#    define pbx_channel_masquerade ast_channel_masquerade
#    define pbx_channel_queue_connected_line_update ast_channel_queue_connected_line_update
#    define pbx_channel_register ast_channel_register
#    define pbx_channel_search_locked ast_channel_search_locked
#    define pbx_channel_tech ast_channel_tech
#    define pbx_channel_trylock ast_channel_trylock
#    define pbx_channel_undefer_dtmf ast_channel_undefer_dtmf
#    define pbx_channel_unref ast_channel_unref
#    define pbx_channel_unregister ast_channel_unregister
#    define pbx_check_hangup ast_check_hangup
#    define pbx_clear_flag ast_clear_flag
#    define pbx_cli ast_cli
#    define pbx_cli_entry ast_cli_entry
#    define pbx_cli_register ast_cli_register
#    define pbx_cli_register_multiple ast_cli_register_multiple
#    define pbx_cli_unregister ast_cli_unregister
#    define pbx_cli_unregister_multiple ast_cli_unregister_multiple
#    define pbx_codec_pref_string ast_codec_pref_string
#    define pbx_cond_t ast_cond_t
#    define pbx_cond_broadcast ast_cond_broadcast
#    define pbx_cond_destroy ast_cond_destroy
#    define pbx_cond_init ast_cond_init
#    define pbx_cond_signal ast_cond_signal
#    define pbx_cond_timedwait ast_cond_timedwait
#    define pbx_cond_wait ast_cond_wait
#    define pbx_config_destroy ast_config_destroy
#    define pbx_copy_string ast_copy_string
#    define pbx_custom_function ast_custom_function
#    define pbx_db_freetree ast_db_freetree
#    define pbx_device_state_changed ast_device_state_changed
#    define pbx_devstate_changed ast_devstate_changed
#    define pbx_devstate_val ast_devstate_val
#    define pbx_digit_usecs ast_digit_usecs
#    define pbx_do_masquerade ast_do_masquerade
#    define pbx_event_get_ie_str ast_event_get_ie_str
#    define pbx_event_get_ie_uint ast_event_get_ie_uint
#    define pbx_event_new ast_event_new
#    define pbx_event_sub ast_event_sub
#    define pbx_event_subscribe ast_event_subscribe
#    define pbx_event_unsubscribe ast_event_unsubscribe
#    define pbx_exists_extension ast_exists_extension
#    define pbx_extension_state_add ast_extension_state_add
#    define pbx_extension_state_del ast_extension_state_del
#    define pbx_extension_state ast_extension_state
#    define pbx_get_group ast_get_group
#    define pbx_get_hint ast_get_hint
#    define pbx_gethostbyname ast_gethostbyname
#    define pbx_ignore_pattern ast_ignore_pattern
#    define pbx_indicate ast_indicate
#    define pbx_io_wait ast_io_wait
#    define pbx_jb_read_conf ast_jb_read_conf
#    define pbx_load_realtime ast_load_realtime
#    define pbx_log ast_log
#    define pbx_malloc ast_malloc
#    define pbx_manager_register2 ast_manager_register2
#    define pbx_manager_unregister ast_manager_unregister
#    define pbx_matchmore_extension ast_matchmore_extension
#    define pbx_moh_stop ast_moh_stop
#    define pbx_mutex_t ast_mutex_t
#    define pbx_mutex_destroy ast_mutex_destroy
#    define pbx_mutex_init ast_mutex_init
#    define pbx_mutex_lock ast_mutex_lock
#    define pbx_mutex_trylock ast_mutex_trylock
#    define pbx_mutex_unlock ast_mutex_unlock
#    define pbx_park_call ast_park_call
#    define pbx_party_name_free ast_party_name_free
#    define pbx_party_number_free ast_party_number_free
#    define pbx_pbx_run ast_pbx_run
#    define pbx_pbx_start ast_pbx_start
#    define pbx_pickup_ext ast_pickup_ext
#    define pbx_poll ast_poll
#    define pbx_print_group ast_print_group
#    define pbx_pthread_create ast_pthread_create
#    define pbx_pthread_create_background ast_pthread_create_background
#    define pbx_pthread_create_detached_background ast_pthread_create_detached_background
#    define pbx_pthread_mutex_lock ast_pthread_mutex_lock
#    define pbx_pthread_mutex_trylock ast_pthread_mutex_trylock
#    define pbx_pthread_mutex_unlock ast_pthread_mutex_unlock
//#    define pbx_queue_control ast_queue_control
//#    define pbx_queue_frame ast_queue_frame
#    define pbx_queue_hangup ast_queue_hangup
#    define pbx_random ast_random
#    define pbx_realloc ast_realloc
#    define pbx_register_file_version ast_register_file_version
#    define pbx_rtp_bridge ast_rtp_bridge
#    define pbx_rtp_codecs_payloads_set_rtpmap_type_rate ast_rtp_codecs_payloads_set_rtpmap_type_rate
#    define pbx_rtp_glue_register ast_rtp_glue_register
#    define pbx_rtp_glue_result ast_rtp_glue_result
#    define pbx_rtp_glue_unregister ast_rtp_glue_unregister
#    define pbx_rtp_instance_bridge ast_rtp_instance_bridge
#    define pbx_rtp_instance_change_source ast_rtp_instance_change_source
#    define pbx_rtp_instance_destroy ast_rtp_instance_destroy
#    define pbx_rtp_instance_fd ast_rtp_instance_fd
#    define pbx_rtp_instance_get_codecs ast_rtp_instance_get_codecs
#    define pbx_rtp_instance_get_local_address ast_rtp_instance_get_local_address
#    define pbx_rtp_instance_get_remote_address ast_rtp_instance_get_remote_address
#    define pbx_rtp_instance_new ast_rtp_instance_new
#    define pbx_rtp_instance_read ast_rtp_instance_read
#    define pbx_rtp_instance_set_prop ast_rtp_instance_set_prop
#    define pbx_rtp_instance_set_qos ast_rtp_instance_set_qos
#    define pbx_rtp_instance_set_remote_address ast_rtp_instance_set_remote_address
#    define pbx_rtp_instance_stop ast_rtp_instance_stop
#    define pbx_rtp_instance_update_source ast_rtp_instance_update_source
#    define pbx_rtp_instance_write ast_rtp_instance_write
#    define pbx_rtp_set_vars ast_rtp_set_vars
#    define pbx_rwlock_destroy ast_rwlock_destroy
#    define pbx_rwlock_init ast_rwlock_init
#    define pbx_rwlock_rdlock ast_rwlock_rdlock
#    define pbx_rwlock_trylock ast_rwlock_trylock
#    define pbx_rwlock_tryrdlock ast_rwlock_tryrdlock
#    define pbx_rwlock_trywrlock ast_rwlock_trywrlock
#    define pbx_rwlock_unlock ast_rwlock_unlock
#    define pbx_rwlock_wrlock ast_rwlock_wrlock
#    define pbx_safe_sleep ast_safe_sleep
#    define pbx_sched_add ast_sched_add
#    define pbx_sched_del ast_sched_del
#    define pbx_sched_runq ast_sched_runq
#    define pbx_sched_wait ast_sched_wait
#    define pbx_sched_when ast_sched_when
#    define pbx_set_callerid ast_set_callerid
#    define pbx_setcallstate ast_setcallstate
#    define pbx_set_flag ast_set_flag
#    define pbx_set_read_format ast_set_read_format
#    define pbx_setstate ast_setstate
#    define pbx_set_write_format ast_set_write_format
#    define pbx_skip_blanks ast_skip_blanks
#    define pbx_skip_nonblanks ast_skip_nonblanks
#    define pbx_sockaddr_from_sin ast_sockaddr_from_sin
#    define pbx_sockaddr_port ast_sockaddr_port
#    define pbx_sockaddr_to_sin ast_sockaddr_to_sin
#    define pbx_state2str ast_state2str
#    define pbx_str_alloca ast_str_alloca
#    define pbx_str_append ast_str_append
#    define pbx_str_buffer ast_str_buffer
#    define pbx_strdup ast_strdup
#    define pbx_strdupa ast_strdupa
#    define pbx_stream_and_wait ast_stream_and_wait
#    define pbx_string_field_build ast_string_field_build
#    define pbx_string_field_set ast_string_field_set
#    define pbx_strip ast_strip
#    define pbx_test_flag ast_test_flag
#    define pbx_set_flag ast_set_flag
#    define pbx_set2_flag ast_set2_flag
#    define pbx_trim_blanks ast_trim_blanks
#    define pbx_true ast_true
#    define pbx_tvnow ast_tvnow
#    define pbx_unregister_file_version ast_unregister_file_version
#    define pbx_update_use_count ast_update_use_count
#    define pbx_variable_browse ast_variable_browse
#    define pbx_variable_retrieve ast_variable_retrieve
#    define pbx_variables_destroy ast_variables_destroy
#    define pbx_strlen_zero ast_strlen_zero
#    define pbx_event_sub ast_event_sub
#    define pbx_context_find ast_context_find
#    define pbx_bridge_impart ast_bridge_impart
#    define pbx_bridge_depart ast_bridge_depart
#    define pbx_hangup ast_hangup

#    define pbx_atomic_fetchadd_int ast_atomic_fetchadd_int
#endif
