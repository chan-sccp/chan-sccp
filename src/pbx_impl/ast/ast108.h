/*!
 * \file        ast108.h
 * \brief       SCCP PBX Asterisk Header
 * \author      Marcello Ceshia
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */

#ifndef SCCP_AST_MAJOR_H_
#define SCCP_AST_MAJOR_H_

#include <config.h>
#include "../../common.h"

#define pbx_channel_unref(c) ({ ao2_ref(c, -1); (PBX_CHANNEL_TYPE *) (NULL); })
#define pbx_channel_ref(c) ({ ao2_ref(c, 1); (PBX_CHANNEL_TYPE *) c; })
#define sccp_sched_context_destroy sched_context_destroy
#define pbx_manager_register ast_manager_register2

typedef int64_t ast_format_t;
int skinny_codecs2pbx_codec_pref(skinny_codec_t * skinny_codecs, struct ast_codec_pref *astCodecPref);
int sccp_wrapper_asterisk_set_rtp_peer(PBX_CHANNEL_TYPE * ast, PBX_RTP_TYPE * rtp, PBX_RTP_TYPE * vrtp, PBX_RTP_TYPE * trtp, int codecs, int nat_active);
void *sccp_do_monitor(void *data);
int sccp_restart_monitor(void);
char *pbx_getformatname(format_t format);
char *pbx_getformatname_multiple(char *buf, size_t size, format_t format);

#define pbx_channel_name(x) x->name

#define NEWCONST const												// old functions used without const
#define OLDCONST												// new function used with const

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define CLI_AMI_OUTPUT(fd, s, ...) 										\
	if (NULL != s) {											\
		astman_append(s, __VA_ARGS__);									\
		local_total++;											\
	} else {												\
		ast_cli(fd, __VA_ARGS__);									\
	}

#define CLI_AMI_OUTPUT_PARAM(param, width, fmt, ...) 							\
	if (NULL != s) {											\
		astman_append(s, "%s: " fmt "\r\n", param, __VA_ARGS__);					\
		local_total++;											\
	} else {												\
		ast_cli(fd, "%-*.*s %s " fmt "\n", width, width, param, ":", __VA_ARGS__);			\
	}

#define CLI_AMI_OUTPUT_BOOL(param, width, value) 								\
	if (NULL != s) {											\
		astman_append(s, "%s: %s\r\n", param, ((value) ? "on" : "off"));				\
		local_total++;											\
	} else {												\
		ast_cli(fd, "%-*.*s %s %s\n", width, width, param, ":", ((value) ? "on" : "off")); 		\
	}

#define CLI_AMI_OUTPUT_YES_NO(param, width, value) 								\
	if (NULL != s) {											\
		astman_append(s, "%s: %s\r\n", param, ((value) ? "yes" : "no"));				\
		local_total++;											\
	} else {												\
		ast_cli(fd, "%-*.*s %s %s\n", width, width, param, ":", ((value) ? "yes" : "no")); 		\
	}

#define CLI_AMI_ERROR(fd, s, m, fmt, ...) 									\
/*	pbx_log(LOG_WARNING, "SCCP CLI ERROR: " fmt, __VA_ARGS__);						*/\
	if (NULL != s) {											\
		astman_send_error(s, m, fmt);									\
		local_total++;											\
	} else {												\
		ast_cli(fd, "SCCP CLI ERROR: " fmt, __VA_ARGS__);						\
	}													\
	return RESULT_FAILURE;

// CLI_ENTRY
//   param1=registration_name
//   param2=function to execute when called
//   param3=cli string to be types as array of strings
//   param4=registration description
//   param5=usage string

#define CLI_AMI_ENTRY(_FUNCTION_NAME,_CALLED_FUNCTION,_DESCR,_USAGE, _COMPLETER_REPEAT)				\
	static int manager_ ## _FUNCTION_NAME(struct mansession *s, const struct message *m)			\
	{													\
		const char *id = astman_get_header(m, "ActionID");						\
		char *cli_ami_params[] = { CLI_COMMAND, CLI_AMI_PARAMS };					\
		char *arguments[ARRAY_LEN(cli_ami_params)];							\
		uint8_t x = 0, i = 0; 										\
		for (x=0, i=0;x<ARRAY_LEN(cli_ami_params);x++) {						\
			if(NULL != cli_ami_params[x] && strlen(cli_ami_params[x]) > 0){				\
				arguments[i++]=(char *)astman_get_header(m, cli_ami_params[x]);		 	\
			} 											\
		}												\
		char idtext[256] = "";										\
		int total = 0;											\
		if (!pbx_strlen_zero(id)) {									\
			snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);				\
		}												\
		astman_send_ack(s, m, AMI_COMMAND);								\
		_CALLED_FUNCTION(-1, &total, s, m, ARRAY_LEN(arguments), arguments);				\
		astman_append(s,										\
		"Event: " _DESCR " Complete\r\n"								\
		"EventList: Complete\r\n"									\
		"ListItems: %d\r\n"										\
		"%s"												\
		"\r\n\r\n", total, idtext);  									\
		return 0;											\
	}													\
														\
	static char * cli_ ## _FUNCTION_NAME(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {	\
		const char *cli_command[] = { CLI_COMMAND, NULL };						\
		static sccp_cli_completer_t cli_complete[] = { CLI_COMPLETE };					\
		static char command[80]="";									\
		if (cmd == CLI_INIT) {										\
		 	ast_join(command, sizeof(command), cli_command);					\
			e->command = command;									\
			e->usage = _USAGE;									\
			return NULL;										\
		} else if (cmd == CLI_GENERATE) {								\
        		uint8_t completer;									\
			for (completer=0; completer<ARRAY_LEN(cli_complete); completer++) {			\
				if ((unsigned)a->pos == (completer + ARRAY_LEN(cli_command) - 1) || _COMPLETER_REPEAT ) {\
					return sccp_exec_completer(cli_complete[completer], (char *)a->line, (char *)a->word, a->pos, a->n);\
				}										\
			}											\
			return NULL;										\
		}												\
		if (a->argc < (int)(ARRAY_LEN(cli_command)-1)) 							\
			return CLI_SHOWUSAGE;									\
														\
		static char *cli_ami_params[] = { CLI_COMMAND, CLI_AMI_PARAMS };				\
		struct message m = { 0 };									\
		size_t hdrlen; 											\
		uint8_t x;											\
                for (x = 0; x < ARRAY_LEN(cli_ami_params) && x < a->argc; x++) {				\
                        hdrlen = strlen(cli_ami_params[x]) + 2 + strlen(a->argv[x]) + 1;			\
                        m.headers[m.hdrcount] = alloca(hdrlen);							\
                        snprintf((char *) m.headers[m.hdrcount], hdrlen, "%s: %s", cli_ami_params[x], a->argv[x]);	\
                        m.hdrcount++;                                        					\
                }												\
		switch (_CALLED_FUNCTION(a->fd, NULL, NULL, &m, a->argc, (char **) a->argv)) {			\
			case RESULT_SUCCESS: return CLI_SUCCESS;						\
			case RESULT_FAILURE: return CLI_FAILURE;						\
			case RESULT_SHOWUSAGE: return CLI_SHOWUSAGE;						\
			default: return CLI_FAILURE;								\
		}												\
	};
#define CLI_ENTRY(_FUNCTION_NAME,_CALLED_FUNCTION,_DESCR,_USAGE, _COMPLETER_REPEAT)				\
	static char *_FUNCTION_NAME(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {			\
		const char *cli_command[] = { CLI_COMMAND, NULL };						\
		static sccp_cli_completer_t cli_complete[] = { CLI_COMPLETE };					\
		static char command[80]="";									\
		if (cmd == CLI_INIT) {										\
		 	ast_join(command, sizeof(command), cli_command);					\
			e->command = command;									\
			e->usage = _USAGE;									\
			return NULL;										\
		} else if (cmd == CLI_GENERATE) {								\
                        uint8_t completer;									\
			for (completer=0; completer<ARRAY_LEN(cli_complete); completer++) {			\
				if ((unsigned)a->pos == (completer + ARRAY_LEN(cli_command) -1) || _COMPLETER_REPEAT ) {\
					return sccp_exec_completer(cli_complete[completer], (char *)a->line, (char *)a->word, a->pos, a->n);\
				}										\
			}											\
			return NULL;										\
		}												\
		if (a->argc < (int)(ARRAY_LEN(cli_command)-1)) 							\
			return CLI_SHOWUSAGE;									\
														\
		switch (_CALLED_FUNCTION(a->fd, a->argc, (char **) a->argv)) {					\
			case RESULT_SUCCESS: return CLI_SUCCESS;						\
			case RESULT_FAILURE: return CLI_FAILURE;						\
			case RESULT_SHOWUSAGE: return CLI_SHOWUSAGE;						\
			default: return CLI_FAILURE;								\
		}												\
	};
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
#endif														/* SCCP_AST108_H_ */
