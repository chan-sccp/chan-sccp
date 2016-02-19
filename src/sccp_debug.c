/*!
 * \file        sccp_debug.c
 * \brief       SCCP Debug Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \since       2016-02-02
 */
#include "config.h"
#include "common.h"
#include "sccp_debug.h"

SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \brief Parse a debug categories line to debug int
 * \param arguments Array of Arguments
 * \param startat Start Point in the Arguments Array
 * \param argc Count of Arguments
 * \param new_debug_value as uint32_t
 * \return new_debug_value as uint32_t
 */
int32_t sccp_parse_debugline(char *arguments[], int startat, int argc, int32_t new_debug_value)
{
	int argi;
	uint32_t i;
	char *argument = "";
	char *token = "";
	const char delimiters[] = " ,\t";
	boolean_t subtract = 0;

	if (sscanf( arguments[startat], "%d", &new_debug_value) != 1) {
		for (argi = startat; argi < argc; argi++) {
			argument = arguments[argi];
			if (!strncmp(argument, "none", 4) || !strncmp(argument, "off", 3)) {
				new_debug_value = 0;
				break;
			} else if (!strncmp(argument, "no", 2)) {
				subtract = 1;
			} else if (!strncmp(argument, "all", 3)) {
				new_debug_value = 0;
				for (i = 0; i < ARRAY_LEN(sccp_debug_categories); i++) {
					if (!subtract) {
						new_debug_value += sccp_debug_categories[i].category;
					}
				}
			} else {
				// parse comma separated debug_var
				boolean_t matched = FALSE;
				token = strtok(argument, delimiters);
				while (token != NULL) {
					// match debug level name to enum
					for (i = 0; i < ARRAY_LEN(sccp_debug_categories); i++) {
						if (strcasecmp(token, sccp_debug_categories[i].key) == 0) {
							if (subtract) {
								if ((new_debug_value & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
									new_debug_value -= sccp_debug_categories[i].category;
								}
							} else {
								if ((new_debug_value & sccp_debug_categories[i].category) != sccp_debug_categories[i].category) {
									new_debug_value += sccp_debug_categories[i].category;
								}
							}
							matched=TRUE;
						}
					}
					if (!matched) {
						pbx_log(LOG_NOTICE, "SCCP: unknown debug value '%s'\n", token);
					}
					token = strtok(NULL, delimiters);
				}
			}
		}
	}
	return new_debug_value;
}

/*!
 * \brief Write the current debug value to debug categories
 * \param debugvalue DebugValue as uint32_t
 * \return string containing list of categories comma seperated (you need to free it)
 */
char *sccp_get_debugcategories(int32_t debugvalue)
{
	uint32_t i;
	char *res = NULL;
	char *tmpres = NULL;
	size_t size = 0;

	for (i = 2; i < ARRAY_LEN(sccp_debug_categories); ++i) {
		if ((debugvalue & sccp_debug_categories[i].category) == sccp_debug_categories[i].category) {
			size_t new_size = size;

			new_size += strlen(sccp_debug_categories[i].key) + 1 /*sizeof(sep) */  + 1;
			tmpres = sccp_realloc(res, new_size);
			if (tmpres == NULL) {
				pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
				sccp_free(res);
				return NULL;
			}
			res = tmpres;
			if (size == 0) {
				strncpy(res, sccp_debug_categories[i].key, new_size - 1);
			} else {
				snprintf(res + strlen(res), new_size-1, ",%s", sccp_debug_categories[i].key);
			}

			size = new_size;
		}
	}

	return res;
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
