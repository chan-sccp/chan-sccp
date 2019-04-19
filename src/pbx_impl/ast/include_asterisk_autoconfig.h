#pragma once
#include "config.h"
__BEGIN_C_EXTERN__
#include <asterisk/autoconfig.h>

/*
 * undefine asterisk PACKAGE_ variables, so that we can set our own. 
 * This prevents a compile warning that a define is overwritten
 */ 
#undef PACKAGE_NAME 
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_BUGREPORT
#undef PACKAGE_URL
#undef PACKAGE_VERSION
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;

