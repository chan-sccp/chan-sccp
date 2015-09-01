/*!
 * \file        sccp_xml.c
 * \brief       SCCP XML Class
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#include <config.h>
#include "common.h"
#include "sccp_xml.h"
#include "sccp_xml_embedded.h"
#include "sccp_utils.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

#if CS_EXPERIMENTAL_XML

#include "asterisk/paths.h"

#if HAVE_LIBXSLT_XSLTINTERNALS_H
#if HAVE_LIBXML_TREE_H
#include <libxml/tree.h>
#endif
#if HAVE_LIBXSLT_TRANSFORM_H
#include <libxslt/transform.h>
#endif
#if HAVE_LIBXSLT_XSLTINTERNALS_H
#include <libxslt/xsltInternals.h>
#endif
#if HAVE_LIBXSLT_XSLTUTILS_H
#include <libxslt/xsltutils.h>
#endif
#if HAVE_LIBXSLT_EXTENSIONS_H
#include <libxslt/extensions.h>
#endif
#if HAVE_LIBEXSLT_EXSLT_H
#include <libexslt/exslt.h>
#endif
#endif

static boolean_t __sccp_xml_isRunning = FALSE;

static void sccp_xml_module_init() 
{
        xmlInitParser();
        exsltRegisterAll();
}

static void sccp_xml_module_destroy() 
{
        xsltCleanupGlobals();
        xmlCleanupParser();
        //xmlMemoryDump();
}

sccp_xml_doc_t *sccp_xml_new(void)
{
        xmlDoc *doc;

        doc = xmlNewDoc((const xmlChar *) "1.0"); 
        return (sccp_xml_doc_t *) doc;
}

sccp_xml_node_t *sccp_xml_new_node(const char *name)
{
        xmlNode *node;
        if (!name) {  
                return NULL;
        }

        node = xmlNewNode(NULL, (const xmlChar *) name);

        return (sccp_xml_node_t *) node;
}

void sccp_xml_set_root(sccp_xml_doc_t *doc, sccp_xml_node_t *node)
{
        if (!doc || !node) {
                return;
        }

        xmlDocSetRootElement((xmlDoc *) doc, (xmlNode *) node);
}

sccp_xml_node_t *sccp_xml_new_child(sccp_xml_node_t *parent, const char *child_name, const char *content)
{
        xmlNode *child;

        if (!parent || !child_name) {
                return NULL;
        }

        child = xmlNewChild((xmlNode *) parent, NULL, (const xmlChar *) child_name, (const xmlChar *) content);
        return (sccp_xml_node_t *) child;
}

sccp_xml_node_t *sccp_xml_new_child_int_content(sccp_xml_node_t *parent, const char *child_name, int content)
{
        char tmp_int_content[100]="";
	snprintf(tmp_int_content, sizeof(tmp_int_content), "%d", content);
        return sccp_xml_new_child(parent, child_name, tmp_int_content);
}

void sccp_xml_add_error_node(sccp_xml_doc_t *doc, int errorno, const char *errordescr)
{
	sccp_xml_node_t * error_node = sccp_xml_new_node("error");
	sccp_xml_set_root(doc, error_node);
	sccp_xml_new_child_int_content(error_node, "errorno", errorno);
	sccp_xml_new_child(error_node, "errordescr", errordescr);
}

void sccp_xml_dump_root(sccp_xml_doc_t *doc)
{
	xmlChar *xmlbuff;
	int buffersize;
	xmlDocDumpFormatMemory((xmlDocPtr)doc, &xmlbuff, &buffersize, 1);
	sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_1 "%s\n", xmlbuff);
}

void sccp_xml_close(sccp_xml_doc_t *doc)
{
        if (!doc) {
                return;
        }

        xmlFreeDoc((xmlDoc *) doc);
        doc = NULL;
	xmlCleanupParser();
}

static const char * const outformat_ext[] = {
	[SCCP_OUTPUT_FORMAT_NULL] = "",
	[SCCP_OUTPUT_FORMAT_HTML] = "html",
	[SCCP_OUTPUT_FORMAT_XML] =  "xml",
	[SCCP_OUTPUT_FORMAT_XSL] =  "xsl",
	[SCCP_OUTPUT_FORMAT_CXML] = "cxml",
};

static char * sccp_xml_searchWebDirForFile(const char *filename, sccp_webservice_output_format_t outformat, const char *extension) 
{
        char filepath[PATH_MAX] = "";
        if (!sccp_strlen_zero(GLOB(webdir))) {
                if (outformat) {
                        snprintf(filepath,sizeof(filepath),"%s/%s_%s.%s",GLOB(webdir), filename, outformat ? outformat_ext[outformat] : "", extension);
                } else {
                        snprintf(filepath,sizeof(filepath),"%s/%s.%s",GLOB(webdir), filename, extension);
                }
                if (access(filepath, F_OK ) == -1) {
        		pbx_log(LOG_ERROR, "\nSCCP: (sccp_xml_searchWebDirForFile) file: '%s' could not be found\n", filepath);
        		filepath[0] = '\0';
                }
        } else {
       		pbx_log(LOG_ERROR, "\nSCCP: (sccp_xml_searchWebDirForFile) Global WebDir '%s' not set to a valid directory\n", GLOB(webdir));
        }
        return strdup(filepath);
}

void sccp_xml_to_ast_str(sccp_xml_doc_t *doc, struct ast_str **out) 
{
	xmlChar *xmlbuff;
	int buffersize;
	xmlDocDumpFormatMemory((xmlDocPtr)doc, &xmlbuff, &buffersize, 1);
	ast_str_append(out, 0, "%s", (char *) xmlbuff);
}

boolean_t sccp_xml_applyStyleSheet(sccp_xml_doc_t *doc, struct ast_str **out, const char *stylesheet, const char *locales, sccp_webservice_output_format_t outformat)
{
        boolean_t result = FALSE;
	xsltStylesheetPtr style = NULL;
	xmlDocPtr tmpdoc = NULL;
	xmlChar *xmlbuff;
	int buffersize;
        sccp_xml_dump_root(doc);

        char *translate_filename = sccp_xml_searchWebDirForFile("translate", SCCP_OUTPUT_FORMAT_NULL, "xml");
        char *stylesheet_filename = sccp_xml_searchWebDirForFile(stylesheet, outformat, "xsl");

        char params[4][PATH_MAX];
        sccp_copy_string(params[0],"locales",PATH_MAX);
        sccp_copy_string(params[1],locales,PATH_MAX);
        sccp_copy_string(params[2],"translationFile",PATH_MAX);
        sccp_copy_string(params[3],translate_filename,PATH_MAX);

//        sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_3 "SCCP: stylesheet: %s, fallback_stylesheet: %s\n", stylesheet_filename, fallback_stylesheet);
	if (!sccp_strlen_zero(stylesheet_filename)) {
        	sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_3 "SCCP: parse stylesheet: %s\n", stylesheet_filename);
                style = xsltParseStylesheetFile((const xmlChar *) stylesheet_filename );
        } else if (!sccp_strlen_zero(stylesheet)) {
                int i;
                for(i=0; i<ARRAY_LEN(fallback_stylesheets); i++) {
                        if (sccp_strcaseequals(stylesheet,fallback_stylesheets[i].name)) {
                                break;
                        }
                }
                if (!sccp_strlen_zero(fallback_stylesheets[i].xsl)) {
                        sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_3 "SCCP: parse build-in stylesheet\n");
                        tmpdoc = xmlReadMemory(fallback_stylesheets[i].xsl, strlen(fallback_stylesheets[i].xsl),"static.xsl",NULL,0);
                        if (tmpdoc) {
                                style = xsltParseStylesheetDoc(tmpdoc);
                        } else {
                                pbx_log(LOG_ERROR, "SCCP: (sccp_xml_applyStyleSheet) error reading fallback stylesheet\n");
                                goto EXIT;
                        }
                } else {
                        pbx_log(LOG_ERROR, "SCCP: (sccp_xml_applyStyleSheet) failed to find fallback stylesheet\n");
                        goto EXIT;
                }
        } else {
                pbx_log(LOG_ERROR, "SCCP: (sccp_xml_applyStyleSheet) failed to find matching stylesheet\n");
                goto EXIT;
        }
        if (style) {
        	sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_4 "SCCP: apply stylesheet\n");
                xmlDocPtr res = xsltApplyStylesheet(style, (xmlDocPtr)doc, (const char **)params);
                if (res) {
                	sccp_log(DEBUGCAT_XML)(VERBOSE_PREFIX_4 "SCCP: save result\n");
                        xsltSaveResultToString(&xmlbuff, &buffersize, res, style);
                        ast_str_append(out, 0, "%s", (char *)xmlbuff);
                        xmlFreeDoc(res);
                        result = TRUE;
                } else {
                        pbx_log(LOG_ERROR, "SCCP: (sccp_xml_applyStyleSheet) error stylesheet could not be parsed using this xmlDoc\n");
                }
                xsltFreeStylesheet(style);
        } else {
                pbx_log(LOG_ERROR, "SCCP: (sccp_xml_applyStyleSheet) failed to parse stylesheet\n");
        }
EXIT:
        if (stylesheet_filename) {
                sccp_free(stylesheet_filename);
        }
        if (translate_filename) {
                sccp_free(translate_filename);
        }
        return result;
}

void sccp_xml_module_start() 
{
        if (!__sccp_xml_isRunning) {
                sccp_xml_module_init();
                __sccp_xml_isRunning = TRUE;
        }
}

void sccp_xml_module_stop() 
{
        if (__sccp_xml_isRunning) {
                sccp_xml_module_destroy();
                __sccp_xml_isRunning = FALSE;
        }
}


#endif // CS_EXPERIMENTAL_XML
