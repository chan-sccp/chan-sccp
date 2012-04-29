
/*!
 * \file 	sccp_utils.h
 * \brief 	SCCP Utils Header
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date$
 * $Revision$
 */
#ifndef __SCCP_UTILS_H
#    define __SCCP_UTILS_H

void sccp_dump_packet(unsigned char *messagebuffer, int len);

void sccp_permithost_addnew(sccp_device_t * d, const char *config_string);
void sccp_serviceURL_addnew(sccp_device_t * d, const char *config_string, uint8_t index);
void sccp_speeddial_addnew(sccp_device_t * d, const char *speed_config_string, uint8_t index);
void sccp_addon_addnew(sccp_device_t * d, const char *addon_config_type);
int sccp_addons_taps(sccp_device_t * d);
void sccp_addons_clear(sccp_device_t * d);
char *sccp_addons_list(sccp_device_t * d);

void sccp_safe_sleep(int ms);
struct ast_variable *sccp_create_variable(const char *buf);
void sccp_device_add_line(sccp_device_t * d, char *name);
sccp_channel_t *get_sccp_channel_from_ast_channel(struct ast_channel *ast_chan);
sccp_selectedchannel_t *sccp_device_find_selectedchannel(sccp_device_t * d, sccp_channel_t * c);
uint8_t sccp_device_selectedchannels_count(sccp_device_t * d);

const char *sccp_channel_toString(sccp_channel_t * c);

sccp_device_t *sccp_device_find_byid(const char *name, boolean_t useRealtime);

#    define sccp_device_find_byname(x) sccp_device_find_byid(x)

sccp_line_t *sccp_line_find_byname_wo(const char *name, uint8_t realtime);

#    define sccp_line_find_byname(x) sccp_line_find_byname_wo(x, 1)
sccp_line_t *sccp_line_find_byinstance(sccp_device_t * d, uint16_t instance);
sccp_channel_t *sccp_find_channel_on_line_byid_locked(sccp_line_t * l, uint32_t id);

#    ifdef CS_SCCP_REALTIME
sccp_device_t *sccp_device_find_realtime(const char *name);

#        define sccp_device_find_realtime_byid(x) sccp_device_find_realtime(x)
#        define sccp_device_find_realtime_byname(x) sccp_device_find_realtime(x)
sccp_line_t *sccp_line_find_realtime_byname(const char *name);
#    endif

sccp_channel_t *sccp_channel_find_byid_locked(uint32_t id);
sccp_channel_t *sccp_find_channel_on_line_byid_locked(sccp_line_t * l, uint32_t id);
sccp_channel_t *sccp_channel_find_bypassthrupartyid_locked(uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_on_line_bypassthrupartyid_locked(sccp_line_t *l, uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_on_device_bypassthrupartyid_locked(sccp_device_t *d, uint32_t passthrupartyid);
sccp_channel_t *sccp_channel_find_bystate_on_line_nolock(sccp_line_t * l, uint8_t state);
sccp_channel_t *sccp_channel_find_bystate_on_line_locked(sccp_line_t * l, uint8_t state);
sccp_channel_t *sccp_channel_find_bycallstate_on_line_locked(sccp_line_t * l, uint8_t state);
sccp_channel_t *sccp_channel_find_bystate_on_device_locked(sccp_device_t * d, uint8_t state);

void sccp_ast_setstate(const sccp_channel_t * c, int state);

void sccp_dev_dbput(sccp_device_t * d);
void sccp_dev_dbclean(void);

#    define _ARR2STR(arrayname, lookup_var, lookup_val, return_var) \
        ({ \
        uint32_t i; \
        for (i = 0; i < ARRAY_LEN(arrayname); i++) { \
                if (arrayname[i].lookup_var == lookup_val) { \
                        return arrayname[i].return_var; \
                } \
        } \
        sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_1 "_ARR2STR Lookup Failed for " #arrayname "." #lookup_var "=%i\n", lookup_val); \
        return ""; \
        })

// SCCP Lookups
const char *message2str(uint32_t value);
const char *channelstate2str(uint32_t value);
const char *astdevicestate2str(uint32_t value);
const char *accessory2str(uint32_t value);
const char *accessorystatus2str(uint32_t value);
const char *extensionstatus2str(uint32_t value);
const char *dndmode2str(uint32_t value);
const char *sccp_buttontype2str(uint32_t value);
const char *callforward2str(uint32_t value);
const char *callforward2longstr(uint32_t value);

// SKINNY Lookups
const char *tone2str(uint32_t value);
const char *alarm2str(uint32_t value);
const char *devicetype2str(uint32_t value);
const char *stimulus2str(uint32_t value);
const char *buttontype2str(uint32_t value);
const char *lampmode2str(uint32_t value);
const char *station2str(uint32_t value);
const char *label2str(uint32_t value);
const char *calltype2str(uint32_t value);
const char *keymode2str(uint32_t value);
const char *keymode2shortname(uint32_t value);
const char *deviceregistrationstatus2str(uint32_t value);
const char *devicestatus2str(uint32_t value);
const char *astcause2skinnycause(int value);
const char *astcause2skinnycause_message(int value);
const char *codec2str(uint32_t value);
char *sccp_codecs2str(char *buf, size_t size, uint32_t * formats);
const char *skinny_ringermode2str(uint8_t type);

const char *array2str(uint8_t type, uint32_t value);

uint32_t sccp_codec_ast2skinny(int fmt);
int sccp_codec_skinny2ast(uint32_t fmt);

struct composedId sccp_parseComposedId(const char *labelString, unsigned int maxLength);

#    ifndef CS_AST_HAS_STRINGS
char *ast_skip_blanks(char *str);
char *ast_trim_blanks(char *str);
char *ast_skip_nonblanks(char *str);
char *ast_strip(char *s);
#    endif

#    ifndef CS_AST_HAS_APP_SEPARATE_ARGS
unsigned int sccp_app_separate_args(char *buf, char delim, char **array, int arraylen);
#    endif

void sccp_util_handleFeatureChangeEvent(const sccp_event_t ** e);
int sccp_softkeyindex_find_label(sccp_device_t * d, unsigned int keymode, unsigned int softkey);

//sccp_device_t *sccp_device_find_byipaddress(unsigned long s_addr);
sccp_device_t *sccp_device_find_byipaddress(struct sockaddr_in sin);

#    if ASTERISK_VERSION_NUMBER >= 10600
#        ifdef HAVE_PBX_DEVICESTATE_H
enum ast_device_state sccp_channelState2AstDeviceState(sccp_channelState_t state);
#        endif
#    endif
sccp_feature_type_t sccp_featureStr2featureID(const char *str);
boolean_t sccp_util_matchSubscriptionId(const sccp_channel_t * channel, const char *SubscriptionIdNum);
sccp_linedevices_t *sccp_util_getDeviceConfiguration(sccp_device_t * device, sccp_line_t * line);
int32_t sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug_value);
char *sccp_get_debugcategories(int32_t debugvalue);
sccp_moo_t *sccp_utils_buildLineStatDynamicMessage(uint32_t lineInstance, const char *dirNum, const char *fqdn, const char *lineDisplayName);
char **explode(char *str, char *sep);
boolean_t implode(char *str[], char *sep, char **res);

#    ifdef HAVE_LIBGC
void gc_warn_handler(char *msg, GC_word p);
#    endif
int socket_equals(struct sockaddr_in *s0, struct sockaddr_in *s1);
void sendUserToDeviceVersion1Message(sccp_device_t * d, uint32_t appID, uint32_t lineInstance, uint32_t callReference, uint32_t transactionID, char data[]);
#endif
boolean_t sccp_strcasecmp(const char *data1, const char *data2);

#ifdef CS_SCCP_VIDEO
void sccp_util_getVideoChannelVariables(sccp_channel_t * channel);
#endif
