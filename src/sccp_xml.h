/*!
 * \file	sccp_xml.h
 * \brief	SCCP XML Header
 * \author	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#pragma once

//#if defined(HAVE_LIBXML2) && defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
//#endif

#include "forward_declarations.h"

__BEGIN_C_EXTERN__
/* interface */
typedef struct {
	xmlDoc * (*createDoc)(void);
	xmlDoc * (*createDocFromStr)(const char *inbuf, int length);
	xmlDoc * (*createDocFromPbxStr)(const pbx_str_t *inbuf);

	xmlNode * (*createNode)(const char * const name);
	xmlNode * (*addElement)(xmlNode *const parentNode, const char * const name, const char *const content);
	void (*addProperty)(xmlNode *const parentNode, const char * const key, const char *const format, ...) __attribute__ ((format (printf, 3, 4))); 
	
	void (*setRootElement)(xmlDoc *const doc, xmlNode *const node);

#if defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
//	int (*setBaseDir)(const char * const baseDir);
//	const char * const (*getBaseDir)(void);
	boolean_t (*applyStyleSheet)(xmlDoc * const doc, PBX_VARIABLE_TYPE *pbx_params);
	boolean_t (*applyStyleSheetByName)(xmlDoc * const doc, const char *const styleSheetFileName, PBX_VARIABLE_TYPE *pbx_params, char **result);
#endif

	char * (*dump)(xmlDoc * const doc, boolean_t indent);
	void (*destroyDoc)(xmlDoc *const * doc);
} XMLInterface;

extern const XMLInterface iXML;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
