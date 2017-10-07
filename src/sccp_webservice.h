/*!
 * \file	sccp_xml.h
 * \brief	SCCP XML Header
 * \author	Diederik de Groot <ddegroot [at] users.sf.net>
 * \date	2016-Nov-16
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $date$
 * $revision$
 */
#pragma once

//#if defined(HAVE_PBX_HTTP_H) && defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBXML2) && defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
//#endif

#include "sccp_enum.h"
#include "forward_declarations.h"

__BEGIN_C_EXTERN__
/* interface */
//typedef sccp_xml_doc_t *(*sccp_webservice_callback_t) (PBX_VARIABLE_TYPE *params, PBX_VARIABLE_TYPE *headers);
typedef boolean_t (*sccp_webservice_callback_t) (PBX_VARIABLE_TYPE *params, PBX_VARIABLE_TYPE *headers, pbx_str_t **result);

typedef struct {
	void (*start)(void);
	void (*stop)(void);
	boolean_t (*isRunning)(void);
	const char *const (*getBaseURL)(void);
	boolean_t (*addHandler)(const char *const uri, sccp_webservice_callback_t callback, sccp_xml_outputfmt_t outputfmt);
	boolean_t (*removeHandler)(const char *const uri);
} WebServiceInterface;

extern const WebServiceInterface iWebService;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
