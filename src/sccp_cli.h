/*!
 * \file 	sccp_cli.h
 * \brief 	SCCP CLI Header
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
#ifndef __SCCP_CLI_H
#define __SCCP_CLI_H

void sccp_register_cli(void);
void sccp_unregister_cli(void);

// CLI_ENTRY
//   param1=registration_name
//   param2=function to execute when called
//   param3=cli string to be types as array of strings
//   param4=registration description
//   param5=usage string
#if ASTERISK_VERSION_NUM >= 10600
  #define CLI_ENTRY_COMPLETE(_function_name,_called_function,_cli_command,_descr,_usage,_completer)	\
	static char *_function_name(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){	\
		if (cmd == CLI_INIT) {								\
			e->command = implode(_cli_command," \t");				\
			e->usage = _usage;							\
			return NULL;								\
		} else if (cmd == CLI_GENERATE) 						\
			return _completer((char *)a->line, (char *)a->word, a->pos, a->n);	\
												\
		if (a->argc < (int)sizeof(*_cli_command)-1) 					\
			return CLI_SHOWUSAGE;							\
												\
		if(_called_function(a->fd, a->argc, a->argv) == RESULT_SUCCESS)			\
			return CLI_SUCCESS;							\
		else										\
			return CLI_FAILURE;							\
	};
  #define CLI_ENTRY(_function_name,_called_function,_cli_command,_descr,_usage)	\
	static char *_function_name(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a){	\
		if (cmd == CLI_INIT) {								\
			e->command = implode(_cli_command," \t");				\
			e->usage = _usage;							\
			return NULL;								\
		} else if (cmd == CLI_GENERATE)							\
			return NULL;								\
												\
		if (a->argc != sizeof(*_cli_command)-1) 					\
			return CLI_SHOWUSAGE;							\
												\
		if(_called_function(a->fd, a->argc, a->argv) == RESULT_SUCCESS)			\
			return CLI_SUCCESS;							\
		else										\
			return CLI_FAILURE;							\
	};
#else
  #define CLI_ENTRY_COMPLETE(_function_name,_called_function,_cli_command,_descr,_usage,_completer)	\
	static struct ast_cli_entry cli_show_lines = {						\
	  _cli_command,										\
	  _called_function,									\
	  _descr,										\
	  _usage,										\
	  _completer										\
	};
  #define CLI_ENTRY(_function_name,_called_function,_cli_command,_descr,_usage,_completer)	\
	static struct ast_cli_entry cli_show_lines = {						\
	  _cli_command,										\
	  _called_function,									\
	  _descr,										\
	  _usage										\
	};
#endif // ASTERISK_VERSION_NUM >= 10600


#endif // __SCCP_CLI_H
