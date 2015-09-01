/*!
 * \file        sccp_webservice.h
 * \brief       SCCP WebService Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */

#ifndef __SCCP_WEBSERVICE_H
#  define __SCCP_WEBSERVICE_H

#if HAVE_LIBXML_TREE_H
#include <libxml/tree.h>
#endif

typedef enum  {
        SCCP_OUTPUT_FORMAT_NULL,
        SCCP_OUTPUT_FORMAT_HTML,
        SCCP_OUTPUT_FORMAT_XML,
        SCCP_OUTPUT_FORMAT_XSL,
        SCCP_OUTPUT_FORMAT_CXML,
} sccp_webservice_output_format_t;

typedef sccp_xml_doc_t *(*sccp_webservice_callback_t) (PBX_VARIABLE_TYPE *params, PBX_VARIABLE_TYPE *headers);
boolean_t sccp_webservice_register_handler(const char *uri, sccp_webservice_callback_t callback, sccp_webservice_output_format_t outformat);
boolean_t sccp_webservice_unregister_handler(const char *uri);
boolean_t sccp_webservice_isRunning();
char *sccp_webservice_get_base_uri();

void sccp_webservice_module_start();
void sccp_webservice_module_stop();

#endif
