/*!
 * \file	sccp_xml.c
 * \brief       SCCP XML Class
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#include <config.h>
#include "common.h"
#include "sccp_xml.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#if defined(CS_EXPERIMENTAL_XML)
#include "sccp_utils.h"

#if HAVE_LIBXML2
#include <libxml/tree.h>
#endif

#if HAVE_LIBXSLT
#include <libxslt/transform.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>
#endif

#if HAVE_LIBEXSLT_EXSLT_H
#include <asterisk/paths.h>
#include <libexslt/exslt.h>
#endif

/* forward declarations */

/* private variables */

/* external functions */
static __attribute__ ((malloc)) xmlDoc * createDoc(void)
{
	xmlInitParser();
	exsltRegisterAll();
	xmlDoc *doc = xmlNewDoc((const xmlChar *) "1.0");
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: (createDoc) doc:%p\n", doc);
	return doc;
}

static xmlNode * createNode(const char * const name)
{
	xmlNode * node = xmlNewNode(NULL, (const xmlChar *) name);
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: (addElement) name:%s -> node: %p\n", name, node);
	return node;
}

static xmlNode * addElement(xmlNode *const parentNode, const char * const name, const char *const content)
{
	xmlNode * node = xmlNewChild(parentNode, NULL, (const xmlChar *) name, (const xmlChar *) content);
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: (addElement) parent:%p, name:%s, content:%s -> node: %p\n", parentNode, name, content, node);
	return node;
}

static void addProperty(xmlNode *const parentNode, const char * const key, const char *const format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[DEFAULT_PBX_STR_BUFFERSIZE];
	vsnprintf(tmp, DEFAULT_PBX_STR_BUFFERSIZE, format, args);
	va_end(args);
	
	xmlNewProp(parentNode, (const xmlChar *)key, (const xmlChar *) tmp);
}

static void setRootElement(xmlDoc * const doc, xmlNode * const node)
{
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: (setRootElement) doc:%p, node:%p\n", doc, node);
	xmlDocSetRootElement(doc, node);
}

static __attribute__ ((malloc)) char * dump(xmlDoc * const doc, boolean_t indent)
{
	char *output;
	int output_len = 0;
	xmlDocDumpFormatMemoryEnc(doc, (xmlChar **)&output, &output_len, "UTF-8", indent ? 1 : 0);
	
	sccp_log(DEBUGCAT_NEWCODE) (VERBOSE_PREFIX_2 "SCCP: (dump) doc:%p -> XML:\n%s\n", doc, output);
	return output;
}

#if HAVE_LIBEXSLT_EXSLT_H
static void applyStyleSheet(xmlDoc * const doc, const char *const styleSheetName, const char *const language, sccp_xml_outputfmt_t outputfmt)
{
//	impl


//	xsltCleanupGlobals();
//	xsltFreeStylesheet(cur)
//	xsltCleanupGlobals()
}
#endif

static void destroyDoc(xmlDoc *const * doc)
{
	if (doc && *doc) {
		xmlFreeDoc((xmlDoc *) * doc);
		*(xmlDoc **)doc = NULL;
	}
	xmlCleanupParser();
	xmlMemoryDump();
	xmlCleanupGlobals();
}

/* private functions */

/* Assign to interface */
const XMLInterface iXML = {
	.createDoc = createDoc,
	.createNode = createNode,
	.addElement = addElement,
	.addProperty = addProperty,
	.setRootElement = setRootElement,

#if HAVE_LIBEXSLT_EXSLT_H	
	//.setBaseDir = setBaseDir,
	//.getBaseDir = getBaseDir,
	.applyStyleSheet = applyStyleSheet,
#endif
	.dump = dump,
	.destroyDoc = destroyDoc,
};

#else
const XMLInterface iXML = { 0 };
#endif // defined(CS_EXPERIMENTAL_XML)


#if CS_TEST_FRAMEWORK
#include <asterisk/test.h>
AST_TEST_DEFINE(sccp_xml_test)
{
	switch(cmd) {
		case TEST_INIT:
			info->name = "xml";
			info->category = "/channels/chan_sccp/";
			info->summary = "xml tests";
			info->description = "chan-sccp-b xml tests";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}
	
	xmlDoc * doc = iXML.createDoc();
	pbx_test_validate(test, doc != NULL);
	
	xmlNode *root= iXML.createNode("root");
	iXML.setRootElement(doc, root);
	pbx_test_validate(test, root != NULL);
	
	xmlNode *group = iXML.addElement(root, "group", NULL);
	pbx_test_validate(test, group != NULL);

	xmlNode *val1 = iXML.addElement(group, "val1", "content1");
	iXML.addProperty(val1, "type", "%s", "text_value");
	iXML.addElement(group, "val2", NULL);
	iXML.addElement(group, "val3", NULL);

	char *check = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><group><val1 type=\"text_value\">content1</val1><val2/><val3/></group></root>\n";
	
	char *result = iXML.dump(doc, FALSE);
	pbx_test_validate(test,  0 == strcmp(check, result));
	sccp_free(result);
	
	iXML.destroyDoc(&doc);
	pbx_test_validate(test, doc == NULL);
	
	return AST_TEST_PASS;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(sccp_xml_test);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(sccp_xml_test);
}
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
