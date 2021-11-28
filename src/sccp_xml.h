/*!
 * \file	sccp_xml.h
 * \brief	SCCP XML Header
 * \author	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 */
#pragma once

//#if defined(HAVE_LIBXML2) && defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
//#endif

#include "forward_declarations.h"

__BEGIN_C_EXTERN__
/* interface */
typedef struct {
	xmlDoc * (* const createDoc)(void);
	xmlDoc * (* const createDocFromStr)(const char * inbuf, int length);
	xmlDoc * (* const createDocFromPbxStr)(const pbx_str_t * inbuf);

	xmlNode * (* const createNode)(const char * const name);
	xmlNode * (* const addElement)(xmlNode * const parentNode, const char * const name, const char * const content);
	void (* const addProperty)(xmlNode * const parentNode, const char * const key, const char * const format, ...) __attribute__((format(printf, 3, 4)));

	void (* const setRootElement)(xmlDoc * const doc, xmlNode * const node);

#if defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
	//	int (*const setBaseDir)(const char * const baseDir);
	//	const char * const (*const getBaseDir)(void);
	boolean_t (* const applyStyleSheet)(xmlDoc * const doc, PBX_VARIABLE_TYPE * pbx_params);
	boolean_t (* const applyStyleSheetByName)(xmlDoc * const doc, const char * const styleSheetFileName, PBX_VARIABLE_TYPE * pbx_params, char ** result);
#endif

	char * (* const dump)(xmlDoc * const doc, boolean_t indent);
	void (* const destroyDoc)(xmlDoc * const * doc);
} XMLInterface;

extern const XMLInterface iXML;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
