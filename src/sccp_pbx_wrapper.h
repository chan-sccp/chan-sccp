
/*!
 * \file 	sccp_pbx_wrapper.h
 * \brief 	SCCP PBX Wrapper Header
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#ifndef __SCCP_PBX_WRAPPER_H
#    define __SCCP_PBX_WRAPPER_H

#    if ASTERISK_VERSION_NUM >= 10800
typedef int64_t format_t;
#    else
typedef int format_t;
#    endif

#    if ASTERISK_VERSION_NUM < 10600
// Fake implementation
struct ast_hashtab {
};

/*! Options for ast_config_load()
 */
enum {
	/*! Load the configuration, including comments */
	CONFIG_FLAG_WITHCOMMENTS = (1 << 0),
	/*! On a reload, give us a -1 if the file hasn't changed. */
	CONFIG_FLAG_FILEUNCHANGED = (1 << 1),
	/*! Don't attempt to cache mtime on this config file. */
	CONFIG_FLAG_NOCACHE = (1 << 2),
};
#    endif

#    if ASTERISK_VERSION_NUM >= 10600
#        define NEWCONST const							// old functions used without const
#        define OLDCONST							// new function used with const
#    else
#        define NEWCONST							// old functions used without const
#        define OLDCONST const							// new function used with const
#    endif

// sccp redefinitions
#    define sccp_strdup ast_strdup
#    define sccp_strdupa ast_strdupa
#    define sccp_strlen_zero ast_strlen_zero

// codec / format redefinitions
#    define pbx_codec_pref_index ast_codec_pref_index
#    define pbx_codec_pref_getsize ast_codec_pref_getsize
#    define pbx_codec2str ast_codec2str

// application / module / cli redefinitions
#    define pbx_register_application ast_register_application
#    define pbx_unregister_application ast_unregister_application
#    define pbx_custom_function_register ast_custom_function_register
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
#    define pbx_db_gettree ast_db_gettree
#    define pbx_db_put ast_db_put
#    define pbx_db_get ast_db_get
#    define pbx_moh_stop ast_moh_stop

#    if ASTERISK_VERSION_NUM < 10400
#        define pbx_channel_alloc(_x, ...) ast_channel_alloc(_x)
#        define pbx_null_frame NULL
#        define pbx_jb_configure(...)
#        define pbx_rtp_proto_register(...)
#        define pbx_rtp_proto_unregister(...)
#        define pbx_rtp_glue_register(...)
#        define pbx_rtp_glue_unregister(...)
#    else
#        define pbx_channel_alloc ast_channel_alloc
#        define pbx_null_frame ast_null_frame
#        define pbx_jb_configure ast_jb_configure
#        define pbx_rtp_proto_register ast_rtp_proto_register
#        define pbx_rtp_proto_unregister ast_rtp_proto_unregister
#        define pbx_rtp_glue_register ast_rtp_glue_register
#        define pbx_rtp_glue_unregister ast_rtp_glue_unregister
#    endif

// Differences in functions between 1.6 and 1.8
// enum ast_extension_states {
// typedef int (*ast_state_cb_type)(char *context, char* id, enum ast_extension_states state, void *data);
// enum ast_pbx_result ast_pbx_start(struct ast_channel *c);
// enum ast_extension_states ast_devstate_to_extenstate(enum ast_device_state devstate);
// int ast_extension_state(struct ast_channel *c, const char *context, const char *exten);
// const char *ast_extension_state2str(int extension_state);
// int ast_extension_state_add(const char *context, const char *exten,
// int ast_extension_state_del(int id, ast_state_cb_type callback);
// int ast_exists_extension(struct ast_channel *c, const char *context, const char *exten,
// int ast_canmatch_extension(struct ast_channel *c, const char *context,
// int ast_matchmore_extension(struct ast_channel *c, const char *context,
// int ast_ignore_pattern(const char *context, const char *pattern);
// struct ast_ha *ast_duplicate_ha_list(struct ast_ha *original);
// char *ast_read_textfile(const char *file);
// int ast_db_deltree(const char *family, const char *keytree);

// utilities
char *get_pbx_callerid_name(struct ast_channel *ast_chan);

char *get_pbx_callerid_number(struct ast_channel *ast_chan);

sccp_callinfo_t *get_pbx_callerid(struct ast_channel *ast_chan);

int set_pbx_callerid(struct ast_channel *ast_chan, sccp_callinfo_t * callInfo);

// replacement implementations
struct ast_channel *pbx_channel_walk_locked(struct ast_channel *target);

struct ast_ha *pbx_append_ha(NEWCONST char *sense, const char *stuff, struct ast_ha *path, int *error);

struct ast_context *pbx_context_find_or_create(struct ast_context **extcontexts, struct ast_hashtab *exttable, const char *name, const char *registrar);

struct ast_config *pbx_config_load(const char *filename, const char *who_asked, struct ast_flags flags);

struct ast_variable *pbx_variable_new(struct ast_variable *v);

char *pbx_getformatname(format_t format);

char *pbx_getformatname_multiple(char *buf, size_t size, format_t format);

const char *pbx_inet_ntoa(struct in_addr ia);

int pbx_str2cos(const char *value, unsigned int *cos);

int pbx_str2tos(const char *value, unsigned int *tos);

int pbx_context_remove_extension(const char *context, const char *extension, int priority, const char *registrar);

void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag);

int pbx_moh_start(struct ast_channel *chan, const char *mclass, const char *interpclass);

int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *addr);
void pbx_rtp_get_us(struct ast_rtp *rtp, struct sockaddr_in *addr);
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *addr);

#endif
