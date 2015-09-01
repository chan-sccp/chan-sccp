/*!
 * \file        sccp_webservice.c
 * \brief       SCCP WebService Class
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#include <config.h>
#include "common.h"
#include "sccp_webservice.h"
#include "sccp_xml.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#if HAVE_PBX_HTTP_H
#include "asterisk/tcptls.h"
#include "asterisk/http.h"
#include "asterisk/paths.h"
#include "asterisk/file.h"

#define MAX_PREFIX 80
#define DEFAULT_PORT 8088
#define DEFAULT_SESSION_LIMIT 100
static int __sccp_webservice_cookie_timeout;
static int __sccp_webservice_uri_registered;
static char *__sccp_webservice_uri;
static boolean_t __sccp_webservice_isRunning = FALSE;

typedef struct sccp_webservice_handler sccp_webservice_handler_t;

struct sccp_webservice_handler {
	char uri[StationMaxServiceURLSize];
	sccp_webservice_callback_t callback;
	sccp_webservice_output_format_t outformat;
//	char fallback_stylesheet[PATH_MAX];
	SCCP_LIST_ENTRY (sccp_webservice_handler_t) list;
};

SCCP_LIST_HEAD (, sccp_webservice_handler_t) sccp_webservice_handlers;

static void sccp_webservice_module_init() 
{
	/* Read part of asterisk http.conf to build webservice_uri */
	struct ast_config *cfg;
	struct ast_variable *v;
	struct ast_flags config_flags = {0};

	boolean_t enabled = FALSE;
	int bindport = 0;
	struct ast_sockaddr * addr = NULL;
	int num_addrs = 0;
	char prefix[MAX_PREFIX] = "";

	cfg = ast_config_load2("http.conf", "http", config_flags);
	if (cfg == CONFIG_STATUS_FILEMISSING || cfg == CONFIG_STATUS_FILEINVALID) {
	        goto EXIT;
	}

	if (cfg) {
	        v = ast_variable_browse(cfg, "general");
	        for (; v; v = v->next) {
			if (!strcasecmp(v->name, "enabled")) {
				enabled = ast_true(v->value);
                        } else if (!strcasecmp(v->name, "bindport")) {
                                if (ast_parse_arg(v->value, PARSE_UINT32 | PARSE_IN_RANGE | PARSE_DEFAULT, &bindport, DEFAULT_PORT, 0, 65535)) {
                                        ast_log(LOG_WARNING, "Invalid port %s specified. Using default port %"PRId32, v->value, DEFAULT_PORT);
                                }
			} else if (!strcasecmp(v->name, "bindaddr")) {
                                if (!(num_addrs = ast_sockaddr_resolve(&addr, v->value, 0, AST_AF_UNSPEC))) {
                                        ast_log(LOG_WARNING, "Invalid bind address %s\n", v->value);
                                }
                        } else if (!strcasecmp(v->name, "prefix")) {
                                if (!ast_strlen_zero(v->value)) {
                                	prefix[0] = '/';
                                        ast_copy_string(prefix + 1, v->value, sizeof(prefix) - 1);
                                } else {
                                        prefix[0] = '\0';
                                }
                        } else if (!strcasecmp(v->name, "sessionlimit")) {
                                if (ast_parse_arg(v->value, PARSE_INT32|PARSE_DEFAULT|PARSE_IN_RANGE,
                                                        &__sccp_webservice_cookie_timeout, DEFAULT_SESSION_LIMIT, 1, INT_MAX)) {
                                        ast_log(LOG_WARNING, "Invalid %s '%s' at line %d of http.conf\n",
                                                        v->name, v->value, v->lineno);
                                }
			}
	        }
        }
	if (enabled && num_addrs) {
		struct ast_str *uri_str = ast_str_create(100);
		ast_str_append(&uri_str, sizeof(uri_str), "http://%s:%d%s", ast_sockaddr_stringify_host(&addr[0]), bindport, prefix);
		__sccp_webservice_uri = strdup(ast_str_buffer(uri_str));
	}
        
        /* Local Settings */
	SCCP_LIST_HEAD_INIT(&sccp_webservice_handlers);
	__sccp_webservice_isRunning = TRUE;

EXIT:
	if (addr) {
		ast_free(addr);
	}
}

static void sccp_webservice_module_destroy() 
{
	sccp_webservice_handler_t *handler = NULL;
	
	SCCP_LIST_LOCK(&sccp_webservice_handlers);
	while ((handler = SCCP_LIST_REMOVE_HEAD(&sccp_webservice_handlers, list))) {
	}
	SCCP_LIST_UNLOCK(&sccp_webservice_handlers);

	SCCP_LIST_HEAD_DESTROY(&sccp_webservice_handlers);
	if (__sccp_webservice_uri) {
		sccp_free(__sccp_webservice_uri);
	}
	__sccp_webservice_isRunning = FALSE;
}

boolean_t sccp_webservice_isRunning() 
{
	return __sccp_webservice_isRunning;
}

char *sccp_webservice_get_base_uri() 
{
	return __sccp_webservice_uri;
}

boolean_t sccp_webservice_register_handler(const char *uri, sccp_webservice_callback_t callback, sccp_webservice_output_format_t outformat) 
{
	sccp_webservice_handler_t *handler = malloc(sizeof(sccp_webservice_handler_t));
	if (!handler) {
		return FALSE;
	}
	sccp_copy_string(handler->uri, uri, sizeof(handler->uri));
	handler->callback = callback;
	handler->outformat = outformat;
	
	sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_1 "SCCP: (sccp_webservice_register_handler) registered handler->uri: '%s'\n", handler->uri);

	SCCP_LIST_LOCK(&sccp_webservice_handlers);
	SCCP_LIST_INSERT_HEAD(&sccp_webservice_handlers, handler, list);
	SCCP_LIST_UNLOCK(&sccp_webservice_handlers);
	
	return TRUE;
}

boolean_t sccp_webservice_unregister_handler(const char *uri) 
{
	boolean_t success = FALSE;
	sccp_webservice_handler_t *handler = NULL;
	
	SCCP_LIST_LOCK(&sccp_webservice_handlers);
	SCCP_LIST_TRAVERSE_SAFE_BEGIN(&sccp_webservice_handlers, handler, list) {
		if (sccp_strcaseequals(uri, handler->uri)) {
			SCCP_LIST_REMOVE_CURRENT(list);
			success = TRUE;
		}
	}
	SCCP_LIST_TRAVERSE_SAFE_END;
	SCCP_LIST_UNLOCK(&sccp_webservice_handlers);
	return success;
}

static const char * const content_type[] = {
	[SCCP_OUTPUT_FORMAT_HTML] = "html",
	[SCCP_OUTPUT_FORMAT_XML] =  "xml",
	[SCCP_OUTPUT_FORMAT_CXML] = "xml",
};

static int sccp_webservice_parser (struct ast_tcptls_session_instance *ser,
                                 enum ast_http_method method,
                                 const struct ast_sockaddr *remote_address,
                                 const char *uri,
                                 PBX_VARIABLE_TYPE *get_params,
                                 PBX_VARIABLE_TYPE *headers)
{
	sccp_webservice_handler_t *handler = NULL;
	struct ast_str *http_header = NULL, *out = NULL;
	sccp_webservice_output_format_t outformat;
	struct ast_variable *params = get_params;
	struct sccp_xml_doc *doc = NULL;

	sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_1 "SCCP: (sccp_webservice_parser) Handling Callback\n");
	
	const char *requested_handler = sccp_get_str_variable_byKey(params, "handler");

	SCCP_LIST_LOCK(&sccp_webservice_handlers);
	SCCP_LIST_TRAVERSE(&sccp_webservice_handlers, handler, list) {
		if (sccp_strcaseequals(requested_handler, handler->uri)) {
			sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_2 "SCCP: (sccp_webservice_parser) handler->uri: '%s', requested_handler: '%s'\n", handler->uri, requested_handler);
			break;
		}
	}
	SCCP_LIST_UNLOCK(&sccp_webservice_handlers);

        if (method == AST_HTTP_POST) {
                params = ast_http_get_post_vars(ser, headers);
        }
	
	const char *user_agent = sccp_get_str_variable_byKey(headers, "User-Agent");
	const char *locales = sccp_get_str_variable_byKey(headers, "Accept-Language");
	const char *requested_outformat = sccp_get_str_variable_byKey(params, "outformat");

	if (DEBUG) {
		PBX_VARIABLE_TYPE *header;
		sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_4 "request headers:\n");
		for(header = headers;header;header = header->next) {
			sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_4 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", header->name, header->value);
		}
		PBX_VARIABLE_TYPE *param;
		sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_4 "request parameters:\n");
		for(param = params;param;param = param->next) {
			sccp_log(DEBUGCAT_WEBSERVICE)(VERBOSE_PREFIX_4 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", param->name, param->value);
		}
	}
	
	if (!sccp_strlen_zero(requested_outformat)) {
		if (!strcasecmp(requested_outformat, "html")) {
			outformat = SCCP_OUTPUT_FORMAT_HTML;
		} else if (!strcasecmp(requested_outformat, "xml")) {
			outformat = SCCP_OUTPUT_FORMAT_XML;
		} else if (!strcasecmp(requested_outformat, "xsl")) {
			outformat = SCCP_OUTPUT_FORMAT_XSL;
		} else if (!strcasecmp(requested_outformat, "cxml")) {
			outformat = SCCP_OUTPUT_FORMAT_CXML;
		} else {
			outformat = SCCP_OUTPUT_FORMAT_XML;
		}
	} else {
		if (strcasecmp(user_agent,"Allegro-Software-WebClient") > 0) {
			outformat = SCCP_OUTPUT_FORMAT_CXML;
		} else {
			outformat = SCCP_OUTPUT_FORMAT_HTML;
		}
	}

	if (handler) {	
		http_header = ast_str_create(80);
		out = ast_str_create(4196);
		
		if (http_header == NULL) {
			pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) ast_str_create() out of memory\n");
			ast_http_error(ser, 500, "Server Error", "Internal Server Error\nast_str_create() out of memory\n");
			goto EXIT;
		}

		if ((doc = handler->callback(params, headers))) {
			sccp_log(DEBUGCAT_WEBSERVICE) (VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) Handling Callback: %s, remote-address: %s\n", uri, ast_sockaddr_stringify(&ser->remote_address));
			ast_str_append(&http_header, 0,
				"Content-type: text/%s\r\n"
				"Cache-Control: no-cache;\r\n"
				"Set-Cookie: sccp_id=\"%08x\"; Version=1; Max-Age=%d\r\n"
				"Pragma: SuppressEvents\r\n",
				content_type[outformat],
				1, 
				__sccp_webservice_cookie_timeout
			);

			if (handler->outformat != outformat) {
				if (!sccp_xml_applyStyleSheet(doc, &out, handler->uri, locales, outformat)) {
					ast_http_error(ser, 500, "Server Error", "Internal Server Error\n(sccp_webservice_parser) stylesheet could not be found\n");
					goto EXIT;
				}
			} else {
				sccp_xml_to_ast_str(doc, &out);
			}

			ast_http_send(ser, method, 200, NULL, http_header, out, 0, 0);
			http_header = out = NULL;
		} else {
			pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) URI Handler failed\n");
			ast_http_error(ser, 500, "Server Error", "Internal Server Error\n(sccp_webservice_parser) URI Handler failed\n");
			goto EXIT;
		}
	} else {
		pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) Requested URI handler '%s' has not been registered with this server\n", requested_handler);
		ast_http_error(ser, 500, "Server Error", "Internal Server Error\n(sccp_webservice_parser) Requested URI handler has not been registered with this server\n");
		goto EXIT;
	}

EXIT:
	if (http_header) {
		ast_free(http_header);
	}
	if (doc) {
		sccp_xml_close(doc);
	}
	if (out) {
		ast_free(out);
	}
	return 0;	
}

static int sccp_webservice_callback(struct ast_tcptls_session_instance *ser, const struct ast_http_uri *urih, const char *uri, enum ast_http_method method, struct ast_variable *get_params,  struct ast_variable *headers)
{
	struct ast_sockaddr ser_remote_address_tmp;
	int retval = -1;

	if (__sccp_webservice_isRunning) {
		pbx_log(LOG_NOTICE, "SCCP: (sccp_http_callback) Incoming request\n");
		ast_sockaddr_copy(&ser_remote_address_tmp, &ser->remote_address);
		retval = sccp_webservice_parser(ser, method, &ser_remote_address_tmp, uri, get_params, headers);
		ast_sockaddr_copy(&ser->remote_address, &ser_remote_address_tmp);
	}
	return retval;
}

static struct ast_http_uri sccp_webservice_uri = {
	.description = "SCCP Cisco IP HTTP Event Interface",
	.uri = "sccp",
	.callback = sccp_webservice_callback,
	.data = NULL,
	.key = __FILE__,
};

void sccp_webservice_module_start() 
{
	sccp_webservice_module_init();
	if (!__sccp_webservice_uri_registered) {
		ast_http_uri_link(&sccp_webservice_uri);
		__sccp_webservice_uri_registered = 1;
	}
}


void sccp_webservice_module_stop() 
{
	sccp_webservice_module_destroy();
	if (__sccp_webservice_uri_registered) {
		ast_http_uri_unlink(&sccp_webservice_uri);
		__sccp_webservice_uri_registered = 0;
	}
}
#endif // HAVE_PBX_HTTP_H
