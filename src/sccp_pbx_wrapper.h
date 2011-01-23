
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


struct sccp_pbx_cb {
	int (* const rtp_set_peer)(sccp_channel_t *channel, int codecs, int nat_active);
	int (* const rtp_destroy)(sccp_channel_t *channel);
	int (* const rtp_stop)(sccp_channel_t *channel);
	int (* const rtp_codec)(sccp_channel_t *channel);
	boolean_t (* const rtp_audio_create)(const sccp_channel_t *channel);
	uint8_t (* const get_payloadType)(sccp_channel_t *channel, skinny_media_payload codec);
	payloadType = ast_rtp_lookup_code(channel->rtp.video.rtp, 1, channel->rtp.video.readFormat);
	
	char *(* const pbx_get_callerid_name)(const sccp_channel_t *channel);
	char *(* const pbx_get_callerid_number)(const sccp_channel_t *channel);
};

extern struct sccp_pbx_cb sccp_pbx;
#define PBX(x) sccp_pbx.x


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

#    if ASTERISK_VERSION_NUM < 10600
#        define pbx_rtp_change_source ast_rtp_new_source
#        define pbx_rtp_new_source ast_rtp_new_source
#    else
#        define pbx_rtp_change_source ast_rtp_change_source
#        define pbx_rtp_new_source ast_rtp_new_source
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
char *sccp_wrapper_asterisk_callerid_name(const sccp_channel_t *channel);
char *sccp_wrapper_asterisk_callerid_number(const sccp_channel_t *channel);

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


boolean_t sccp_wrapper_asterisk_create_audio_rtp(const sccp_channel_t * c);

#if ASTERISK_VERSION_NUM < 10800
int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *addr);
void pbx_rtp_get_us(struct ast_rtp *rtp, struct sockaddr_in *addr);
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *addr);
#endif

#if ASTERISK_VERSION_NUM >= 10400
enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(struct ast_channel *ast, struct ast_rtp **rtp);
#endif

#ifndef CS_AST_HAS_RTP_ENGINE
enum ast_rtp_get_result sccp_wrapper_asterisk_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp **rtp);
#else
enum ast_rtp_glue_result sccp_wrapper_asterisk_get_vrtp_peer(struct ast_channel *ast, struct ast_rtp_instance **rtp);
#endif

#if ASTERISK_VERSION_NUM >= 10400
#        if ASTERISK_VERSION_NUM >= 10400 && ASTERISK_VERSION_NUM < 10600
int sccp_wrapper_asterisk_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, int codecs, int nat_active);
#        else
int sccp_wrapper_asterisk_set_rtp_peer(struct ast_channel *ast, struct ast_rtp *rtp, struct ast_rtp *vrtp, struct ast_rtp *trtp, int codecs, int nat_active);
#        endif
#endif

#ifdef ASTERISK_VERSION_NUM >= 10800
uint8_t sccp_wrapper_asterisk18_get_payloadType(struct sccp_rtp *rtp, skinny_media_payload codec);
#else
uint8_t sccp_wrapper_asterisk_get_payloadType(struct sccp_rtp *rtp, skinny_media_payload codec);
#endif

#endif
