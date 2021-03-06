/*!
 * \file        sccp_cli_table.h
 * \brief       SCCP CLI Table Macro Header
 * \author      Diederik de Groot <ddegroot [at] users.sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 *
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef _CLI_AMI_TABLE_INCLUDE
#define _CLI_AMI_TABLE_INCLUDE

#define _CLI_AMI_TABLE_LIST_LOCK(...) CLI_AMI_TABLE_LIST_LOCK(__VA_ARGS__)
#define _CLI_AMI_TABLE_LIST_ITERATOR(...) CLI_AMI_TABLE_LIST_ITERATOR(__VA_ARGS__)
#define _CLI_AMI_TABLE_LIST_UNLOCK(...) CLI_AMI_TABLE_LIST_UNLOCK(__VA_ARGS__)

#define MERGE2_(_a,_b) _a##_b
#define UNIQUE_VAR(_a,_b) MERGE2_(_a,_b)

#		define AMI_OUTPUT_PARAM(param, width, fmt, ...)                               \
			{                                                                      \
				char camelParam[width + 1];                                    \
				CLI_AMI_CAMEL_PARAM (param, camelParam);                       \
				astman_append (s, "%s: " fmt "\r\n", camelParam, __VA_ARGS__); \
				local_line_total++;                                            \
			}
#endif

// =========== Code To be generated by include file ===============
pbx_cli(fd, "\n");
#ifdef CLI_AMI_TABLE_LIST_ITER_TYPE
CLI_AMI_TABLE_LIST_ITER_TYPE *CLI_AMI_TABLE_LIST_ITER_VAR = NULL;
#endif

#ifndef CLI_AMI_TABLE_BEFORE_ITERATION
#define CLI_AMI_TABLE_BEFORE_ITERATION
#endif
#ifndef CLI_AMI_TABLE_AFTER_ITERATION
#define CLI_AMI_TABLE_AFTER_ITERATION
#endif

/* print headers */
int UNIQUE_VAR(table_width_, CLI_AMI_TABLE_NAME) = 0;
int UNIQUE_VAR(table_entries_, CLI_AMI_TABLE_NAME) = 0;
const char * UNIQUE_VAR(id_, CLI_AMI_TABLE_NAME)            = s && m ? astman_get_header(m, "ActionID") : "";
char         UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME)[256];
if (!sccp_strlen_zero(UNIQUE_VAR(id_, CLI_AMI_TABLE_NAME))) {
	snprintf(UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME), sizeof(UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME)), "ActionID: %s\r\n", UNIQUE_VAR(id_, CLI_AMI_TABLE_NAME));
} else {
	UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME)[0] = '\0';
}
#	define astman_append_inc(...)                                                                                                                                                                                          \
		({                                                                                                                                                                                                              \
			astman_append(__VA_ARGS__);                                                                                                                                                                             \
			local_line_total++;                                                                                                                                                                                     \
		})

char UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME)[256];
snprintf(UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME), sizeof(UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME)), "Event: SCCP%sEntry\r\n", STRINGIFY(CLI_AMI_TABLE_PER_ENTRY_NAME));

// table used variable declarations
#	define CLI_AMI_TABLE_FIELD(_a, _b, _c, _d, _e) UNIQUE_VAR(table_width_, CLI_AMI_TABLE_NAME) = UNIQUE_VAR(table_width_, CLI_AMI_TABLE_NAME) + _d + 1;
#	define CLI_AMI_TABLE_UTF8_FIELD                CLI_AMI_TABLE_FIELD
CLI_AMI_TABLE_FIELDS
#undef CLI_AMI_TABLE_FIELD
if (!s)
{
pbx_cli(fd, "+--- %s %.*s+\n", STRINGIFY(CLI_AMI_TABLE_NAME), UNIQUE_VAR(table_width_, CLI_AMI_TABLE_NAME) - (int) strlen(STRINGIFY(CLI_AMI_TABLE_NAME)) - 4, "------------------------------------------------------------------------------------------------------------------------------------------------------------------");

pbx_cli(fd, "| ");
#define CLI_AMI_TABLE_FIELD(_a,_b,_c,_d,_e) pbx_cli(fd,"%*s ",-_d,#_a);
CLI_AMI_TABLE_FIELDS
#undef CLI_AMI_TABLE_FIELD
    pbx_cli(fd, "|\n");

pbx_cli(fd, "+ ");
#define CLI_AMI_TABLE_FIELD(_a,_b,_c,_d,_e) pbx_cli(fd,"%." STRINGIFY(_d) "s ",	"==================================================================================================================================================================");
CLI_AMI_TABLE_FIELDS
#undef CLI_AMI_TABLE_FIELD
    pbx_cli(fd, "+\n");
} else {
	astman_append_inc(s, "Event: TableStart\r\n");
	astman_append_inc(s, "TableName: %s\r\n", STRINGIFY(CLI_AMI_TABLE_NAME));
	if (s && m && sccp_strcaseequals(astman_get_header(m, "TableFormatVersion"), "2")) {
		snprintf(UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME), sizeof(UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME)), "Event: %s_Entry\r\n", STRINGIFY(CLI_AMI_TABLE_PER_ENTRY_NAME));
		astman_append_inc(s, "TableFormatVersion: %s\r\n", "2");
	}
	astman_append_inc(s, "%s\r\n", UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME));
	local_line_total++;
}

	/* iterator through list */
if (!s) {
#define CLI_AMI_TABLE_FIELD(_a,_b,_c,_d,_e) pbx_cli(fd,"%" _b #_c " ",_e);
#undef CLI_AMI_TABLE_UTF8_FIELD
#define CLI_AMI_TABLE_UTF8_FIELD(_a,_b,_c,_d,_e) pbx_cli(fd,"%-*" #_c " ", sccp_utf8_columnwidth(_d,_e), _e);
#ifdef CLI_AMI_TABLE_LIST_ITERATOR
	_CLI_AMI_TABLE_LIST_LOCK(CLI_AMI_TABLE_LIST_ITER_HEAD);
	_CLI_AMI_TABLE_LIST_ITERATOR(CLI_AMI_TABLE_LIST_ITER_HEAD, CLI_AMI_TABLE_LIST_ITER_VAR, list) {
#else
	CLI_AMI_TABLE_ITERATOR {
#endif
		CLI_AMI_TABLE_BEFORE_ITERATION pbx_cli(fd, "| ");
		CLI_AMI_TABLE_FIELDS pbx_cli(fd, "|\n");
	CLI_AMI_TABLE_AFTER_ITERATION}
#ifdef CLI_AMI_TABLE_LIST_ITERATOR
	_CLI_AMI_TABLE_LIST_UNLOCK(CLI_AMI_TABLE_LIST_ITER_HEAD);
#endif
#undef CLI_AMI_TABLE_FIELD
#undef CLI_AMI_TABLE_UTF8_FIELD
#define CLI_AMI_TABLE_UTF8_FIELD CLI_AMI_TABLE_FIELD
} else {
#	define CLI_AMI_TABLE_FIELD1(_paramstr, _fmtstr, _vargs)        CLI_AMI_OUTPUT_PARAM(_paramstr, 0, _fmtstr, _vargs);
#	define CLI_AMI_TABLE_FIELD(_param, _width, _fmt, _len, _vargs) CLI_AMI_TABLE_FIELD1(#        _param, "%" #        _fmt, _vargs)
#	ifdef CLI_AMI_TABLE_LIST_ITERATOR
	_CLI_AMI_TABLE_LIST_LOCK(CLI_AMI_TABLE_LIST_ITER_HEAD);
	_CLI_AMI_TABLE_LIST_ITERATOR(CLI_AMI_TABLE_LIST_ITER_HEAD, CLI_AMI_TABLE_LIST_ITER_VAR, list) {
#else
	CLI_AMI_TABLE_ITERATOR {
#endif
		CLI_AMI_TABLE_BEFORE_ITERATION
		UNIQUE_VAR(table_entries_, CLI_AMI_TABLE_NAME)++;
		astman_append_inc(s, "%s", UNIQUE_VAR(eventText_, CLI_AMI_TABLE_NAME));

		astman_append_inc(s, "ChannelType: SCCP\r\n");
		astman_append_inc(s, "ChannelObjectType: %s\r\n", STRINGIFY(CLI_AMI_TABLE_PER_ENTRY_NAME));
		CLI_AMI_TABLE_FIELDS
		astman_append_inc(s, "%s\r\n", UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME));
		local_line_total++;
		CLI_AMI_TABLE_AFTER_ITERATION
	}
#ifdef CLI_AMI_TABLE_LIST_ITERATOR
	_CLI_AMI_TABLE_LIST_UNLOCK(CLI_AMI_TABLE_LIST_ITER_HEAD);
#endif
#	undef CLI_AMI_TABLE_FIELD1
#	undef CLI_AMI_TABLE_FIELD
}

	/* print footer */
if (!s) {
	pbx_cli(fd, "+%.*s+\n", UNIQUE_VAR(table_width_, CLI_AMI_TABLE_NAME) + 1, "------------------------------------------------------------------------------------------------------------------------------------------------------------------");

} else {
	astman_append_inc(s, "Event: TableEnd\r\n");
	astman_append_inc(s, "TableName: %s\r\n", STRINGIFY(CLI_AMI_TABLE_NAME));
	astman_append_inc(s, "TableEntries: %d\r\n", UNIQUE_VAR(table_entries_, CLI_AMI_TABLE_NAME));
	astman_append_inc(s, "%s\r\n", UNIQUE_VAR(idText_, CLI_AMI_TABLE_NAME));
	local_line_total++;
}

#ifdef CLI_AMI_TABLE_NAME
#undef CLI_AMI_TABLE_NAME
#endif

#ifdef CLI_AMI_TABLE_PER_ENTRY_NAME
#undef CLI_AMI_TABLE_PER_ENTRY_NAME
#endif

#ifdef CLI_AMI_TABLE_LIST_ITER_TYPE
#undef CLI_AMI_TABLE_LIST_ITER_TYPE
#endif

#ifdef CLI_AMI_TABLE_LIST_ITER_VAR
#undef CLI_AMI_TABLE_LIST_ITER_VAR
#endif

#ifdef CLI_AMI_TABLE_LIST_ITER_HEAD
#undef CLI_AMI_TABLE_LIST_ITER_HEAD
#endif

#ifdef CLI_AMI_TABLE_LIST_LOCK
#undef CLI_AMI_TABLE_LIST_LOCK
#endif

#ifdef CLI_AMI_TABLE_LIST_ITERATOR
#undef CLI_AMI_TABLE_LIST_ITERATOR
#endif

#ifdef CLI_AMI_TABLE_ITERATOR
#undef CLI_AMI_TABLE_ITERATOR
#endif

#ifdef CLI_AMI_TABLE_BEFORE_ITERATION
#undef CLI_AMI_TABLE_BEFORE_ITERATION
#endif

#ifdef CLI_AMI_TABLE_AFTER_ITERATION
#undef CLI_AMI_TABLE_AFTER_ITERATION
#endif

#ifdef CLI_AMI_TABLE_LIST_UNLOCK
#undef CLI_AMI_TABLE_LIST_UNLOCK
#endif

#ifdef CLI_AMI_TABLE_FIELDS
#undef CLI_AMI_TABLE_FIELDS
#endif
	// =========== End of Code To be generated by include file ============
#endif														/* DOXYGEN_SHOULD_SKIP_THIS */
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
