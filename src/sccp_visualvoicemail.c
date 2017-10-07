/*!
 * \file	sccp_visualvoicemail.c
 * \brief	SCCP Visual Voicemail Class
 * \author	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#include "sccp_visualvoicemail.h"
#include "sccp_xml.h"

#if defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBXML2) && defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)

#include <asterisk/app.h>
#include <asterisk/tcptls.h>
#include <asterisk/http.h>
#include <asterisk/paths.h>
#include <asterisk/file.h>

/* forward declarations */

/* private variables */

/* external functions */

/* callback functions */
static int sccp_folders_callback(struct ast_tcptls_session_instance *ser, const struct ast_http_uri *urih, const char *uri, enum ast_http_method method, PBX_VARIABLE_TYPE *get_params,  PBX_VARIABLE_TYPE *headers)
{
	int retval = 500;
	struct ast_variable *params = get_params;
	int cookie_timeout = 60;

	if (method == AST_HTTP_POST) {
		params = ast_http_get_post_vars(ser, headers);
	}

	if (DEBUG) {
		PBX_VARIABLE_TYPE *header;
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "request headers:\n");
		for(header = headers;header;header = header->next) {
			sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", header->name, header->value);
		}
		PBX_VARIABLE_TYPE *param;
		sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "request parameters:\n");
		for(param = params;param;param = param->next) {
			sccp_log(DEBUGCAT_NEWCODE)(VERBOSE_PREFIX_3 "SCCP: (sccp_webservice_parser) key: %s, value: %s\n", param->name, param->value);
		}
	}
	
	pbx_str_t *http_header = pbx_str_create(80);
	pbx_str_t *out = pbx_str_create(4196);
	if (!http_header || !out) {
		pbx_log(LOG_ERROR, "SCCP: (sccp_webservice_parser) ast_str_create() out of memory\n");
		ast_http_error(ser, 500, "Server Error", "Internal Server Error\nast_str_create() out of memory\n");
		return 500;;
	}

	if (!ast_vm_is_registered()) {
		pbx_log(LOG_ERROR, "SCCP: (visualvoicemail) No voicemail provider registered.\n");
		ast_http_error(ser, retval, "Server Error", "No voicemail provider registered.\n");
		return retval;
	}
	
	struct ast_vm_mailbox_snapshot *res = ast_vm_mailbox_snapshot_create(
		/*const char *mailbox*/ "98041",
		/*const char *context*/ "default",
		/*const char *folder*/ NULL, 
		/*int descending*/ 0, 
		/*enum ast_vm_snapshot_sort_val sort_val*/ AST_VM_SNAPSHOT_SORT_BY_ID,
		/*int combine_INBOX_and_OLD*/ TRUE);
	if (res) {
		sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "SCCP: res:%p, Number of messages:%d, Number of folders:%d\n", res, res->total_msg_num, res->folders);
		
		//struct ast_vm_msg_snapshot *msg;
		
		xmlDoc * doc = iXML.createDoc();
		xmlNode *root= iXML.createNode("voicemails");
		iXML.setRootElement(doc, root);
		xmlNode *group = iXML.addElement(root, "folders", NULL);
		for (int folder = 0; folder < res->folders; folder++) {
			const char *foldername = ast_vm_index_to_foldername(folder);
			xmlNode *val = iXML.addElement(group, foldername, NULL);
			iXML.addProperty(val, "href", "http://1234/folder/%s", foldername);
			/*
			AST_LIST_TRAVERSE(&res->snapshots[folder], msg, msg) {
				sccp_log(DEBUGCAT_CORE)(VERBOSE_PREFIX_2 "%d: folder:%d, msg_id:%s, callerid:%s, callerchan:%s, exten:%s, origdate:%s, origtime:%s, duration:%s, folder_name:%s, flag:%s\n", 
					msg->msg_number,
					folder,
					msg->msg_id,
					msg->callerid,
					msg->callerchan,
					msg->exten,
					msg->origdate,
					msg->origtime,
					msg->duration,
					msg->folder_name,
					msg->flag
				);
				node1 = iXML.addChild(node, "voicemail", NULL);
				iXML.addProperty(node1, "msg_id", "%d", msg->msg_id);
				iXML.addProperty(node1, "msg_number", "%d", msg->msg_number);
				//iXML.addProperty(node1, "callerid", "%s", msg->callerid);
				//iXML.addProperty(node1, "callerchan", "%s", msg->callerchan);
				//iXML.addProperty(node1, "exten", "%s", msg->exten);
				//iXML.addProperty(node1, "origdate", "%s", msg->origdate);
				iXML.addProperty(node1, "origtime", "%d", msg->origtime);
				iXML.addProperty(node1, "duration", "%d", msg->duration);
				//iXML.addProperty(node1, "folder_name", "%s", msg->folder_name);
				//iXML.addProperty(node1, "flag", "%s", msg->flag);
			}
			*/
		}
		char *result = iXML.dump(doc, FALSE);
		ast_str_append(&out, 0, "%s", result);
		sccp_free(result);
		iXML.destroyDoc(&doc);

		ast_vm_mailbox_snapshot_destroy(res);
	}

	ast_str_append(&http_header, 0,
		"Content-type: text/%s\r\n"
		"Cache-Control: no-cache;\r\n"
		"Set-Cookie: sccp_id=\"%08x\"; Version=1; Max-Age=%d\r\n"
		"Pragma: SuppressEvents\r\n",
		"xml",
		1, 
		cookie_timeout
	);

	ast_http_send(ser, method, 200, NULL, http_header, out, 0, 0);
	
	return retval;
}

/* private functions */
static struct ast_http_uri sccp_webservice_uri = {
	.description = "SCCP IP HTTP Event Interface VVM",
	.uri = "sccp/vvm/folders",
	.callback = sccp_folders_callback,
	.data = NULL,
	.key = __FILE__,
};

static void __attribute__((constructor)) init_xml(void)
{
	ast_http_uri_link(&sccp_webservice_uri);
}

static void __attribute__((destructor)) destroy_xml(void)
{
	ast_http_uri_unlink(&sccp_webservice_uri);
}

/* Assign to interface */
const VVMInterface iVVM = {
	//.createDoc = createDoc,
	//.destroyDoc = destroyDoc,
};

#else
const VVMInterface iVVM = { 0 };
#endif // defined(CS_EXPERIMENTAL_XML)


#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(sccp_visualvoicemail_test)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "vvm";
			info->category = "/channels/chan_sccp/";
			info->summary = "visual voicemail tests";
			info->description = "chan-sccp-b visual voicemail tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	
	return AST_TEST_PASS;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_visualvoicemail_test);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_visualvoicemail_test);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
