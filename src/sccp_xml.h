/*!
 * \file	sccp_xml.h
 * \brief       SCCP XML Header
 * \author      Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#pragma once

#if defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBXML2)

#include "forward_declarations.h"
struct _xmlDoc;
typedef struct _xmlDoc xmlDoc;

struct _xmlNode;
typedef struct _xmlNode xmlNode;

__BEGIN_C_EXTERN__
/* interface */
typedef struct {
	xmlDoc * (*createDoc)(void);

	xmlNode * (*createNode)(const char * const name);
	xmlNode * (*addElement)(xmlNode *const parentNode, const char * const name, const char *const content);
	void (*addProperty)(xmlNode *const parentNode, const char * const key, const char *const format, ...);
	
	void (*setRootElement)(xmlDoc *const doc, xmlNode *const node);

#if defined(HAVE_LIBEXSLT_EXSLT_H)
//	int (*setBaseDir)(const char * const baseDir);
//	const char * const (*getBaseDir)(void);
	void (*applyStyleSheet)(xmlDoc * const doc, const char *const styleSheetName, const char *const language, sccp_webservice_outputfmt_t outputfmt);
#endif

	char * (*dump)(xmlDoc * const doc, boolean_t indent);
	void (*destroyDoc)(xmlDoc *const * doc);
} XMLInterface;

extern const XMLInterface iXML;
__END_C_EXTERN__
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
