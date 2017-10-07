/*!
 * \file	sccp_webservice.c
 * \brief	SCCP WebService Class
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2016-Nov-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "");

#include "sccp_webservice.h"

//#ifdef CS_EXPERIMENTAL_XML
#if defined(HAVE_PBX_HTTP_H) && defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBEXSLT_EXSLT_H)

#include "sccp_utils.h"
#include "sccp_vector.h"
#include <asterisk/tcptls.h>
#include <asterisk/http.h>
#include <asterisk/paths.h>
#include <asterisk/file.h>

/* forward declarations */

/* private variables */
#define MAX_PREFIX 80
#define DEFAULT_PORT 8088
#define DEFAULT_SESSION_LIMIT 100

static boolean_t running = FALSE;
static int cookie_timeout;
static char baseURL[StationMaxServiceURLSize] = { 0 };

typedef struct handler {
	char uri[StationMaxServiceURLSize];
	sccp_webservice_callback_t callback;
	sccp_xml_outputfmt_t outputfmt;
} handler_t;

SCCP_VECTOR_RW(, handler_t) handlers;

#define HANDLER_CB_CMP(elem, value) (sccp_strcaseequals((elem).uri, (value)))

/* private functions */
static boolean_t parse_manager_conf(void)
{
	boolean_t result = FALSE;
	struct ast_config *cfg;
	struct ast_variable *v;
	struct ast_flags config_flags = {0};

	int manager_enabled = 0;
	int webmanager_enabled = 0;

	cfg = ast_config_load2("manager.conf", "http", config_flags);
	if (cfg && cfg != CONFIG_STATUS_FILEINVALID) {
	        v = ast_variable_browse(cfg, "general");
	        for (; v; v = v->next) {
	                if (!strcasecmp(v->name, "enabled")) {
        	                manager_enabled = ast_true(v->value);
        	                if (!manager_enabled) {
					pbx_log(LOG_NOTICE, "SCCP: manager.conf:enabled=yes are required\n");
        	                	break;
        	                }
			} else if (!strcasecmp(v->name, "webenabled")) {
                        	webmanager_enabled = ast_true(v->value);
        	                if (!webmanager_enabled) {
					pbx_log(LOG_NOTICE, "SCCP: manager.conf:webenabled=yes are required\n");
        	                	break;
        	                }
			} else {
			}
		}
	        ast_config_destroy(cfg);
	} else {
		pbx_log(LOG_NOTICE, "SCCP: manager.conf file not found or invalid\n");
	}
	if (manager_enabled && webmanager_enabled) {
		result = TRUE;
	}
	return result;
}

static boolean_t parse_http_conf(char *const uri_str)
{
	boolean_t result = FALSE;
	struct ast_config *cfg;
	struct ast_variable *v;
	struct ast_flags config_flags = {0};

	int http_enabled = 0;
	int bindport = 0;
	struct ast_sockaddr * addr = NULL;
	int num_addrs = 0;
	char prefix[MAX_PREFIX] = "";

	cfg = ast_config_load2("http.conf", "http", config_flags);
	if (cfg && cfg != CONFIG_STATUS_FILEINVALID) {
		v = ast_variable_browse(cfg, "general");
		for (; v; v = v->next) {
			if (!strcasecmp(v->name, "enabled")) {
				http_enabled = ast_true(v->value);
				if (!http_enabled) {
					pbx_log(LOG_NOTICE, "SCCP: http.conf:enabled=yes are required\n");
					break;
				}
			} else if (!strcasecmp(v->name, "bindport")) {
				if (ast_parse_arg(v->value, PARSE_UINT32 | PARSE_IN_RANGE | PARSE_DEFAULT, &bindport, DEFAULT_PORT, 0, 65535)) {
					ast_log(LOG_WARNING, "Invalid port %s specified. Using default port %"PRId32, v->value, DEFAULT_PORT);
					break;
				}
			} else if (!strcasecmp(v->name, "bindaddr")) {
				if (!(num_addrs = ast_sockaddr_resolve(&addr, v->value, 0, AST_AF_UNSPEC))) {
					ast_log(LOG_WARNING, "Invalid bind address %s\n", v->value);
					break;
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
					&cookie_timeout, DEFAULT_SESSION_LIMIT, 1, INT_MAX)) {
					ast_log(LOG_WARNING, "Invalid %s '%s' at line %d of http.conf\n", v->name, v->value, v->lineno);
				}
			}
		}
		ast_config_destroy(cfg);
		if (http_enabled && num_addrs) {
			snprintf(uri_str, StationMaxServiceURLSize, "http://%s:%d%s", ast_sockaddr_stringify_host(&addr[0]), bindport, prefix);
			result = TRUE;
		}
	} else {
		pbx_log(LOG_NOTICE, "SCCP: http.conf file not found or invalid\n");
	}
	if (addr) {
		ast_free(addr);
	}
	
	return result;
}

static const char * sccp_get_str_variable_byKey(PBX_VARIABLE_TYPE *params, char *key)
{
	PBX_VARIABLE_TYPE *param;
	for(param = params;param;param = param->next) {
		if (!strcasecmp(key, param->name)) {
			return param->value;
			break;
		}
	}
	return NULL;
}

/*
static int sccp_get_int_variable_byKey(PBX_VARIABLE_TYPE *params, char *key)
{
	const char *value = sccp_get_str_variable_byKey(params, key);
	if (value) {
		return sccp_atoi(value, strlen(value));
	}
	return -1;
}
*/

static handler_t * get_request_handler(PBX_VARIABLE_TYPE * request_params)
{
	const char *uri = sccp_get_str_variable_byKey(request_params, "handler");
	handler_t *handler = NULL;

	SCCP_VECTOR_RW_RDLOCK(&handlers);
	handler = SCCP_VECTOR_GET_CMP(&handlers, uri, HANDLER_CB_CMP); 
	SCCP_VECTOR_RW_UNLOCK(&handlers);

	if (!handler) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) no handler found for uri:%s\n", uri);
	}
	
	return handler;
}

/*
static int parse_request_headers(PBX_VARIABLE_TYPE * request_headers, const char *locale)
{
	locale = sccp_get_str_variable_byKey(request_headers, "Accept-Language");

	return 0;
}

static int parse_request_params()
{
	return 0;
}

static int parse_request_uri()
{
	return 0;
}
static int parse_outputfmt(PBX_VARIABLE_TYPE * request_params, PBX_VARIABLE_TYPE *request_headers, sccp_xml_outputfmt_t *outputfmt)
{
	const char *requested_outputfmt = sccp_get_str_variable_byKey(request_params, "outformat");
	if (!sccp_strlen_zero(requested_outputfmt)) {
		*outputfmt = sccp_xml_outputfmt_str2val(requested_outputfmt);
		return 0;
	}
	const char *user_agent = sccp_get_str_variable_byKey(request_headers, "User-Agent");
	if (!sccp_strlen_zero(user_agent) && strcasecmp(user_agent,"Allegro-Software-WebClient") > 0) {
		*outputfmt = SCCP_XML_OUTPUTFMT_CXML;
		return 0;
	}
	return -2;
}
*/

static int request_parser (
	struct ast_tcptls_session_instance 	* ser,
	enum ast_http_method 			method,
	const struct ast_sockaddr 		* remote_address,
	const char 				* request_uri,
	PBX_VARIABLE_TYPE 			* request_params,
	PBX_VARIABLE_TYPE 			* request_headers
) {
	int result = 0;
	handler_t *handler = NULL;
	sccp_xml_outputfmt_t outputfmt = SCCP_XML_OUTPUTFMT_NULL;
	//const char *locale = NULL;
	pbx_str_t *http_header = NULL;
	pbx_str_t *out = NULL;
	
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (sccp_webservice_parser) Handling Callback\n");

	handler = get_request_handler(request_params);
	//result |= parse_request_headers(request_headers, locale);
	//result |= parse_outputfmt(request_params, request_headers, &outputfmt);
	//result |= parse_request_params();
	//result |= parse_request_uri();

	//if (DEBUG) {
		PBX_VARIABLE_TYPE *header;
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "request headers:\n");
		for(header = request_headers;header;header = header->next) {
			sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", header->name, header->value);
		}
		PBX_VARIABLE_TYPE *param;
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "request parameters:\n");
		for(param = request_params;param;param = param->next) {
			sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", param->name, param->value);
		}
	//}
	sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "SCCP: handler:%p, result:%d\n", handler, result);
	
	do {
		if (handler && result == 0) {
			pbx_str_t *http_header = pbx_str_create(80);
			pbx_str_t *out = pbx_str_create(4196);
			if (!http_header || !out) {
				pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) ast_str_create() out of memory\n");
				ast_http_error(ser, 500, "Server Error", "Internal Server Error\nast_str_create() out of memory\n");
				break;
			}
			
			if (handler->callback(request_params, request_headers, &out)) {
				sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) Handling Callback: %s, remote-address: %s\n", request_uri, ast_sockaddr_stringify(&ser->remote_address));
				ast_str_append(&http_header, 0,
					"Content-type: text/%s\r\n"
					"Cache-Control: no-cache;\r\n"
					"Set-Cookie: sccp_id=\"%08x\"; Version=1; Max-Age=%d\r\n"
					"Pragma: SuppressEvents\r\n",
					sccp_xml_outputfmt2str(outputfmt),
					1, 
					cookie_timeout
				);

				if (handler->outputfmt != outputfmt) {
					//if (!sccp_xml_applyStyleSheet(doc, &out, handler->uri, locales, outformat)) {
					//	ast_http_error(ser, 500, "Server Error", "Internal Server Error\n(sccp_webservice_parser) stylesheet could not be found\n");
					//	goto EXIT;
					//}
				}

				ast_http_send(ser, method, 200, NULL, http_header, out, 0, 0);
				http_header = out = NULL;
				break;
			}
		} else {
			pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) could not parse the uri or headers\n");
			ast_http_error(ser, 500, "Server Error", "Internal Server Error\nURI or Headers could not be parsed\n");
			break;
		}
	} while(0);

	if (http_header) {
		ast_free(http_header);
	}
	if (out) {
		ast_free(out);
	}
	return result;
}

static int sccp_webservice_callback(struct ast_tcptls_session_instance *ser, const struct ast_http_uri *urih, const char *uri, enum ast_http_method method, PBX_VARIABLE_TYPE *get_params,  PBX_VARIABLE_TYPE *headers)
{
	struct ast_sockaddr ser_remote_address_tmp;
	int retval = -1;
	struct ast_variable *params = get_params;

	if (running) {
		pbx_log(LOG_NOTICE, "SCCP: (sccp_http_callback) Handle incoming %s request for %s, %p, %p\n", method == AST_HTTP_POST ? "POST" : "GET", uri, get_params, headers);

		if (method == AST_HTTP_POST) {
			params = ast_http_get_post_vars(ser, headers);
		}
		
		ast_sockaddr_copy(&ser_remote_address_tmp, &ser->remote_address);
		retval = request_parser(ser, method, &ser_remote_address_tmp, uri, params, headers);
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

/* begin test */
static boolean_t sccp_webservice_test(PBX_VARIABLE_TYPE *params, PBX_VARIABLE_TYPE *headers, pbx_str_t **result) {
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_test) Test Webservice\n");
	pbx_str_append(result, 0, "<html>");
	pbx_str_append(result, 0, "<body>");
	pbx_str_append(result, 0, "<h1>TEST RESULT</h1>");
	pbx_str_append(result, 0, "</body>");
	pbx_str_append(result, 0, "</html>");
	return TRUE;
}

/* end test */

static void webservice_module_start(void)
{
	if (!running && parse_manager_conf() && parse_http_conf(baseURL)) {
		pbx_log(LOG_NOTICE, "SCCP: (sccp_webservice_module_starting...\n");
		SCCP_VECTOR_RW_INIT(&handlers, 1);

		/* begin test */
		iWebService.addHandler("1234", sccp_webservice_test, SCCP_XML_OUTPUTFMT_HTML);
		/* end test */

		ast_http_uri_link(&sccp_webservice_uri);
		pbx_log(LOG_NOTICE, "SCCP: (sccp_webservice_module_started\n");
		running = TRUE;
	}
}

static void webservice_module_stop(void)
{
	if (running) {
		ast_http_uri_unlink(&sccp_webservice_uri);
		SCCP_VECTOR_RW_WRLOCK(&handlers);
		SCCP_VECTOR_RESET(&handlers, SCCP_VECTOR_ELEM_CLEANUP_NOOP);
		SCCP_VECTOR_RW_UNLOCK(&handlers);
		SCCP_VECTOR_RW_FREE(&handlers);
		running = FALSE;
	}
}


/* exported functions */
static boolean_t isRunning(void)
{
	return running;
}

static const char *const getBaseURL(void)
{
	return baseURL;
}

static boolean_t addHandler(const char *const uri, sccp_webservice_callback_t callback, sccp_xml_outputfmt_t outputfmt)
{
	boolean_t result = FALSE;
	handler_t handler = {
		.uri = "",
		.callback = callback,
		.outputfmt = outputfmt,
	};
	sccp_copy_string(handler.uri, uri, sizeof(handler.uri));
	
	SCCP_VECTOR_RW_WRLOCK(&handlers);
	if (SCCP_VECTOR_APPEND(&handlers, handler) == 0) {
		result = TRUE;
	}
	SCCP_VECTOR_RW_UNLOCK(&handlers);
	return result;
}

static boolean_t removeHandler(const char *const uri)
{

	boolean_t result = FALSE;
	SCCP_VECTOR_RW_WRLOCK(&handlers);
	if (SCCP_VECTOR_REMOVE_CMP_UNORDERED(&handlers, uri, HANDLER_CB_CMP, SCCP_VECTOR_ELEM_CLEANUP_NOOP) == 0) {
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_1 "SCCP: (sccp_webservice) removed handler for uri: '%s'\n", uri);
		result = TRUE;
	}
	SCCP_VECTOR_RW_UNLOCK(&handlers);
	
	return result;
}

/* Assign to interface */
const WebServiceInterface iWebService = {
	.start = webservice_module_start,
	.stop = webservice_module_stop,
	.isRunning = isRunning,
	.getBaseURL = getBaseURL,
	.addHandler = addHandler,
	.removeHandler = removeHandler,
};
#else
const WebServiceInterface iWebService = { 0 };
#endif
