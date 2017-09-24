/*!
 * \file        sccp_utils.h
 * \brief       SCCP Utils Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once
#include "config.h"

#ifdef strdupa
#define sccp_strdupa strdupa
#else
#ifndef pbx_strdupa
#define pbx_strdupa(s)						\
	(__extension__						\
	({							\
		const char *__old = (s);			\
		size_t __len = strlen(__old) + 1;		\
		char *__new = __builtin_alloca(__len);		\
		memcpy (__new, __old, __len);			\
		__new;						\
	}))
#endif
#define strdupa sccp_strdupa
#endif

__BEGIN_C_EXTERN__

SCCP_API void SCCP_CALL sccp_dump_packet(unsigned char *messagebuffer, int len);
SCCP_API void SCCP_CALL sccp_dump_msg(const sccp_msg_t * const msg);
SCCP_API int SCCP_CALL sccp_addons_taps(sccp_device_t * d);
SCCP_API void SCCP_CALL sccp_addons_clear(sccp_device_t * d);
SCCP_API void SCCP_CALL sccp_safe_sleep(int ms);
SCCP_API void SCCP_CALL sccp_pbx_setcallstate(sccp_channel_t * channel, int state);
#if UNUSEDCODE // 2015-11-01
SCCP_API void SCCP_CALL sccp_dev_dbclean(void);
#endif

#define _ARR2STR(arrayname, lookup_var, lookup_val, return_var) \
        ({ \
        uint32_t i; \
        for (i = 0; i < ARRAY_LEN(arrayname); i++) { \
                if (arrayname[i].lookup_var == lookup_val) { \
                        return arrayname[i].return_var; \
                } \
        } \
        pbx_log(LOG_ERROR, "_ARR2STR Lookup Failed for " #arrayname "." #lookup_var "=%i\n", lookup_val); \
        return ""; \
        })
#define _STRARR2INT(arrayname, lookup_var, lookup_val, return_var) \
        ({ \
        uint32_t i; \
        for (i = 0; i < ARRAY_LEN(arrayname); i++) { \
                if (!strcasecmp(arrayname[i].lookup_var, lookup_val)) { \
                        return (uint32_t)arrayname[i].return_var; \
                } \
        } \
        pbx_log(LOG_ERROR, "_STRARR2INT Lookup Failed for " #arrayname "." #lookup_var "=%s\n", lookup_val); \
        return 0; \
        })
#define _ARR2INT(arrayname, lookup_var, lookup_val, return_var) \
        ({ \
        uint32_t i; \
        for (i = 0; i < ARRAY_LEN(arrayname); i++) { \
                if (arrayname[i].lookup_var == lookup_val) { \
                        return arrayname[i].return_var; \
                } \
        } \
        pbx_log(LOG_ERROR, "_ARR2STR Lookup Failed for " #arrayname "." #lookup_var "=%i\n", lookup_val); \
        return -1; \
        })

// SCCP Lookups
// SKINNY Lookups
SCCP_INLINE const char * SCCP_CALL featureType2str(sccp_feature_type_t value);
SCCP_INLINE const char * SCCP_CALL debugcat_keys(void);
#if UNUSEDCODE // 2015-11-01
SCCP_INLINE SCCP_CALL uint32_t debugcat2int(const char *str);
#endif

SCCP_API const char * SCCP_CALL skinny_ringermode2str(uint8_t type);
SCCP_API const char * __PURE__ SCCP_CALL sccp_channel_toString(sccp_channel_t * c);

SCCP_API int SCCP_CALL sccp_parseComposedId(const char *labelString, unsigned int maxLength, sccp_subscription_id_t *subscriptionId, char extension[SCCP_MAX_EXTENSION]);

#ifndef HAVE_PBX_STRINGS_H
SCCP_API char * SCCP_CALL pbx_skip_blanks(char *str);
SCCP_API char * SCCP_CALL pbx_trim_blanks(char *str);
SCCP_API char * SCCP_CALL pbx_skip_nonblanks(char *str);
SCCP_API char * SCCP_CALL pbx_strip(char *s);
#endif

#ifndef CS_AST_HAS_APP_SEPARATE_ARGS
SCCP_API unsigned SCCP_CALL int sccp_app_separate_args(char *buf, char delim, char **array, int arraylen);
#endif

SCCP_API void SCCP_CALL sccp_util_featureStorageBackend(const sccp_event_t * event);
#if 0 /* unused */
SCCP_API int SCCP_CALL sccp_softkeyindex_find_label(sccp_device_t * d, unsigned int keymode, unsigned int softkey);
#endif

//sccp_device_t *sccp_device_find_byipaddress(unsigned long s_addr);
//sccp_device_t *sccp_device_find_byipaddress(struct sockaddr_storage *sin);

SCCP_API sccp_feature_type_t SCCP_CALL sccp_featureStr2featureID(const char *str);
SCCP_API boolean_t __PURE__ SCCP_CALL sccp_util_matchSubscriptionId(const sccp_channel_t * channel, const char *subscriptionIdNum);
SCCP_INLINE SCCP_CALL boolean_t sccp_netsock_equals(const struct sockaddr_storage * const s0, const struct sockaddr_storage *const s1);
SCCP_INLINE SCCP_CALL size_t sccp_strlen(const char *data);
SCCP_INLINE SCCP_CALL boolean_t sccp_strlen_zero(const char *data);
SCCP_INLINE SCCP_CALL boolean_t sccp_strequals(const char *data1, const char *data2);
SCCP_INLINE SCCP_CALL boolean_t sccp_strcaseequals(const char *data1, const char *data2);
SCCP_API int __PURE__ SCCP_CALL sccp_strIsNumeric(const char *s);

SCCP_API void SCCP_CALL sccp_free_ha(struct sccp_ha *ha);
SCCP_API int SCCP_CALL sccp_apply_ha(const struct sccp_ha *ha, const struct sockaddr_storage *addr);
SCCP_API int SCCP_CALL sccp_apply_ha_default(const struct sccp_ha *ha, const struct sockaddr_storage *addr, int defaultValue);

SCCP_API int SCCP_CALL sccp_sockaddr_split_hostport(char *str, char **host, char **port, int flags);
SCCP_API int SCCP_CALL sccp_sockaddr_storage_parse(struct sockaddr_storage *addr, const char *str, int flags);
SCCP_API struct sccp_ha * SCCP_CALL sccp_append_ha(const char *sense, const char *stuff, struct sccp_ha *path, int *error);
SCCP_API void SCCP_CALL sccp_print_ha(struct ast_str *buf, int buflen, struct sccp_ha *path);
SCCP_API void SCCP_CALL sccp_print_group(struct ast_str *buf, int buflen, sccp_group_t group);
SCCP_API int SCCP_CALL sccp_strversioncmp(const char *s1, const char *s2);
SCCP_API char * SCCP_CALL sccp_dec2binstr(char *buf, size_t size, int value);
SCCP_INLINE void SCCP_CALL sccp_copy_string(char *dst, const char *src, size_t size);
SCCP_API char * SCCP_CALL sccp_trimwhitespace(char *str);
SCCP_INLINE int SCCP_CALL sccp_atoi(const char * const buf, size_t buflen);
SCCP_INLINE boolean_t SCCP_CALL sccp_utils_convUtf8toLatin1(ICONV_CONST char *utf8str, char *buf, size_t len);
SCCP_API long SCCP_CALL int sccp_random(void);
SCCP_INLINE boolean_t SCCP_CALL sccp_always_false(void);
SCCP_INLINE boolean_t SCCP_CALL sccp_always_true(void);

#ifdef DEBUG
SCCP_API void SCCP_CALL sccp_do_backtrace(void);
#endif
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
