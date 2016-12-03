/*!
 * \file	sccp_visualvoicemail.h
 * \brief	SCCP Visual Voicemail  Header
 * \author	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$
 */
#pragma once

//#if defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBXML2) && defined(HAVE_LIBXSLT) && defined(HAVE_LIBEXSLT_EXSLT_H)
//#endif

#include "forward_declarations.h"

__BEGIN_C_EXTERN__
/* interface */
typedef struct {
	xmlDoc * (*createDoc)(void);
	void (*destroyDoc)(xmlDoc *const * doc);
} VVMInterface;

extern const VVMInterface iVVM;
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
