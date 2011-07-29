
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
#    include "common.h"

struct sccp_pbx_cb {
	boolean_t(*const alloc_pbxChannel) (const sccp_channel_t * line, void **pbx_channel);
	int (*const set_callstate) (const sccp_channel_t * channel, int state);

	int (*const rtp_set_peer) (sccp_channel_t * channel, int codecs, int nat_active);
	int (*const rtp_destroy) (sccp_channel_t * channel);
	int (*const rtp_stop) (sccp_channel_t * channel);
	int (*const rtp_codec) (sccp_channel_t * channel);
	 boolean_t(*const rtp_audio_create) (const sccp_channel_t * channel, struct ast_rtp ** new_rtp);
	 boolean_t(*const rtp_video_create) (const sccp_channel_t * channel, struct ast_rtp ** new_rtp);
	 uint8_t(*const rtp_get_payloadType) (const struct sccp_rtp * rtp, skinny_media_payload codec);

	char *(*const pbx_get_callerid_name)(const sccp_channel_t * channel);
	char *(*const pbx_get_callerid_number)(const sccp_channel_t * channel);
	char *(*const pbx_get_callerid_ani)(const sccp_channel_t * channel);
	char *(*const pbx_get_callerid_dnid)(const sccp_channel_t * channel);
	char *(*const pbx_get_callerid_rdnis)(const sccp_channel_t * channel);

	void (*const pbx_set_callerid_name) (const PBX_CHANNEL_TYPE * pbx_chan, const char *name);
	void (*const pbx_set_callerid_number) (const PBX_CHANNEL_TYPE * pbx_chan, const char *number);
	void (*const pbx_set_callerid_ani) (PBX_CHANNEL_TYPE * pbx_chan, const char *ani);
	void (*const pbx_set_callerid_dnid) (const PBX_CHANNEL_TYPE * pbx_chan, const char *dnid);
	void (*const pbx_set_callerid_rdnis) (const PBX_CHANNEL_TYPE * pbx_chan, const char *rdnis);
};

extern struct sccp_pbx_cb sccp_pbx;

#    define PBX(x) sccp_pbx.x

#    if ASTERISK_VERSION_NUMBER >= 10800
typedef int64_t format_t;
#    else
typedef int format_t;
#    endif

#    if ASTERISK_VERSION_NUMBER < 10600
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

#    if ASTERISK_VERSION_NUMBER >= 10600
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
#    if ASTERISK_VERSION_NUMBER < 10600
#        define pbx_register_application(a, b, c, d, e) ast_register_application(a, b, c, d)
#    else
#        define pbx_register_application ast_register_application2
#    endif
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
#    define pbx_db_gettree ast_db_gettree
#    define pbx_db_put ast_db_put
#    define pbx_db_get ast_db_get
#    define pbx_moh_stop ast_moh_stop

#    if ASTERISK_VERSION_NUMBER < 10400
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

#    ifndef CS_AST_HAS_RTP_ENGINE
#        define pbx_rtp_change_source ast_rtp_change_source
#        ifdef 	CS_AST_RTP_NEW_SOURCE
#            define pbx_rtp_new_source ast_rtp_new_source
#        else
#            define pbx_rtp_new_source ast_rtp_change_source
#        endif
#    else
#        define pbx_rtp_change_source ast_rtp_instance_change_source
#        define pbx_rtp_new_source ast_rtp_instance_update_source
#    endif

// replacement implementations
PBX_CHANNEL_TYPE *pbx_channel_walk_locked(PBX_CHANNEL_TYPE * target);

struct ast_channel *sccp_asterisk_channel_search_locked(int (*is_match) (struct ast_channel *, void *), void *data);
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
#ifdef CS_MANAGER_EVENTS
void pbxman_send_listack(struct mansession *s, const struct message *m, char *msg, char *listflag);
#endif
int pbx_moh_start(PBX_CHANNEL_TYPE * chan, const char *mclass, const char *interpclass);

#    ifdef HAVE_ASTERISK
sccp_callinfo_t *sccp_wrapper_asterisk_get_callerid(PBX_CHANNEL_TYPE * ast_chan);
char *sccp_wrapper_asterisk_get_callerid_name(const sccp_channel_t * channel);
char *sccp_wrapper_asterisk_get_callerid_number(const sccp_channel_t * channel);

int sccp_wrapper_asterisk_set_callerid(PBX_CHANNEL_TYPE * ast_chan, sccp_callinfo_t * callInfo);
void sccp_wrapper_asterisk_set_callerid_name(const PBX_CHANNEL_TYPE * pbx_chan, const char *name);
void sccp_wrapper_asterisk_set_callerid_number(const PBX_CHANNEL_TYPE * pbx_chan, const char *number);
void sccp_wrapper_asterisk_set_callerid_ani(PBX_CHANNEL_TYPE * pbx_chan, const char *ani);
void sccp_wrapper_asterisk_set_callerid_dnid(const PBX_CHANNEL_TYPE * pbx_chan, const char *dnid);
void sccp_wrapper_asterisk_set_callerid_rdnis(const PBX_CHANNEL_TYPE * pbx_chan, const char *rdnis);

int sccp_wrapper_asterisk_setCallState(const sccp_channel_t * channel, int state);
boolean_t sccp_wrapper_asterisk_create_video_rtp(const sccp_channel_t * c, struct ast_rtp **new_rtp);
boolean_t sccp_wrapper_asterisk_create_audio_rtp(const sccp_channel_t * c, struct ast_rtp **new_rtp);

#        ifdef CS_AST_HAS_RTP_ENGINE
enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);	/*! \todo wrong definition */
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active);	/*! \todo wrong definition */
enum ast_rtp_glue_result sccp_wrapper_asterisk_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);
#        else

#            if ASTERISK_VERSION_NUMBER >= 10400 && ASTERISK_VERSION_NUMBER < 10600
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, int codecs, int nat_active);
#            else
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active);
#            endif

enum ast_rtp_get_result sccp_wrapper_asterisk_get_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);
enum ast_rtp_get_result sccp_wrapper_asterisk_get_vrtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE ** rtp);
#        endif									//#ifdef CS_AST_HAS_RTP_ENGINE

boolean_t sccp_wrapper_asterisk_allocPBXChannel(const sccp_channel_t * channel, void **pbx_channel);
uint8_t sccp_wrapper_asterisk_get_payloadType(const struct sccp_rtp *rtp, skinny_media_payload codec);
int pbx_rtp_get_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);
void pbx_rtp_get_us(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);
void pbx_rtp_set_peer(PBX_RTP_TYPE * rtp, struct sockaddr_in *addr);

#    endif									//ifdef HAVE_ASTERISK

#endif
