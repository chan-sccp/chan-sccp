#pragma once
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
