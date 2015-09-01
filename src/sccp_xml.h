/*!
 * \file        sccp_xml.h
 * \brief       SCCP XML Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_XML_H
#  define __SCCP_XML_H

#include "sccp_webservice.h"

#if HAVE_LIBXML_TREE_H
#include <libxml/tree.h>
#endif

sccp_xml_doc_t *sccp_xml_new(void);
sccp_xml_node_t *sccp_xml_new_node(const char *name);
sccp_xml_node_t *sccp_xml_new_child(sccp_xml_node_t *parent, const char *child_name, const char *content);
sccp_xml_node_t *sccp_xml_new_child_int_content(sccp_xml_node_t *parent, const char *child_name, int content);
void sccp_xml_set_root(sccp_xml_doc_t *doc, sccp_xml_node_t *node);
void sccp_xml_add_error_node(sccp_xml_doc_t *doc, int errorno, const char *errordescr);
void sccp_xml_dump_root(sccp_xml_doc_t *doc);
void sccp_xml_close(sccp_xml_doc_t *doc);
                                                
void sccp_xml_to_ast_str(sccp_xml_doc_t *doc, struct ast_str **out);
boolean_t sccp_xml_applyStyleSheet(sccp_xml_doc_t *doc, struct ast_str **out, const char *stylesheet, const char *language, sccp_webservice_output_format_t outformat);

void sccp_xml_module_start();
void sccp_xml_module_stop();


#endif
