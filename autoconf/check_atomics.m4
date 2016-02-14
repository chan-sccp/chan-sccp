
# -*- Autoconf -*-
#
# Copyright (c)      2010  Sandia Corporation
#
# Largely stolen from Qthreads
#

# SCCP_CHECK_ATOMICS([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SCCP_CHECK_ATOMICS], [
AC_ARG_ENABLE([builtin-atomics],
     [AS_HELP_STRING([--disable-builtin-atomics],
	                 [force the use of inline-assembly (if possible) rather than compiler-builtins for atomics. This is useful for working around some compiler bugs; normally, it's preferable to use compiler builtins.])])
AS_IF([test "x$enable_builtin_atomics" != xno], [
AC_CHECK_HEADERS([ia64intrin.h ia32intrin.h])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-32],
  [sccp_cv_atomic_CAS32],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>
#include <stdint.h> /* for uint32_t */

int main()
{
uint32_t bar=1, old=1, new=2;
uint32_t foo = __sync_val_compare_and_swap(&bar, old, new);
return (int)foo;
}]])],
		[sccp_cv_atomic_CAS32="yes"],
		[sccp_cv_atomic_CAS32="no"])])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-64],
  [sccp_cv_atomic_CAS64],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>
#include <stdint.h> /* for uint64_t */

int main()
{
uint64_t bar=1, old=1, new=2;
uint64_t foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[sccp_cv_atomic_CAS64="yes"],
		[sccp_cv_atomic_CAS64="no"])])
AC_CACHE_CHECK([whether compiler supports builtin atomic CAS-ptr],
  [sccp_cv_atomic_CASptr],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
void *bar=(void*)1, *old=(void*)1, *new=(void*)2;
void *foo = __sync_val_compare_and_swap(&bar, old, new);
return (int)(long)foo;
}]])],
		[sccp_cv_atomic_CASptr="yes"],
		[sccp_cv_atomic_CASptr="no"])])
AS_IF([test "x$sccp_cv_atomic_CAS32" = "xyes" && test "x$sccp_cv_atomic_CAS64" = "xyes" && test "x$sccp_cv_atomic_CASptr" = "xyes"],
	  [sccp_cv_atomic_CAS=yes],
	  [sccp_cv_atomic_CAS=no])
AC_CACHE_CHECK([whether compiler supports builtin atomic incr],
  [sccp_cv_atomic_incr],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_IA64INTRIN_H
# include <ia64intrin.h>
#elif HAVE_IA32INTRIN_H
# include <ia32intrin.h>
#endif
#include <stdlib.h>

int main()
{
long bar=1;
long foo = __sync_fetch_and_add(&bar, 1);
return foo;
}]])],
		[sccp_cv_atomic_incr="yes"],
		[sccp_cv_atomic_incr="no"])
   ])
AS_IF([test "$sccp_cv_atomic_CAS" = "yes"],
	  [AC_CACHE_CHECK([whether ia64intrin.h is required],
	    [sccp_cv_require_ia64intrin_h],
		[AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdlib.h>

int main()
{
long bar=1, old=1, new=2;
long foo = __sync_val_compare_and_swap(&bar, old, new);
return foo;
}]])],
		[sccp_cv_require_ia64intrin_h="no"],
		[sccp_cv_require_ia64intrin_h="yes"])])])
])
AC_CACHE_CHECK([whether the compiler supports CMPXCHG16B],
  [sccp_cv_cmpxchg16b],
  [AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stdint.h> /* for uint64_t */
struct m128 {
uint64_t a,b;
} one, two, tmp;
int main(void)
{
one.a = 1;
one.b = 2;
two.a = 3;
two.b = 4;
tmp.a = 5;
tmp.b = 6;
__asm__ __volatile__ ("lock cmpxchg16b (%2)"
:"=a"(tmp.a),"=d"(tmp.b)
:"r"(&two),"a"(two.a),"d"(two.b),"b"(one.a),"c"(one.b)
:"memory");
return tmp.a;
}]])],
    [sccp_cv_cmpxchg16b="yes"],
	[sccp_cv_cmpxchg16b="no"])])
AS_IF([test "x$sccp_cv_cmpxchg16b" = "xyes"],
[AC_CACHE_CHECK([whether the CPU supports CMPXCHG16B],
  [sccp_cv_cpu_cmpxchg16b],
  [AC_ARG_ENABLE([cross-cmpxchg16b],
     [AS_HELP_STRING([--enable-cross-cmpxchg16b],
	     [when cross compiling, ordinarily we asume that cmpxchg16b is not available, however this option forces us to assume that cmpxchg16b IS available])])
   AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdint.h> /* for uint64_t */
struct m128 {
uint64_t a,b;
} one, two, tmp;
int main(void)
{
one.a = 1;
one.b = 2;
two.a = 3;
two.b = 4;
tmp.a = 5;
tmp.b = 6;
__asm__ __volatile__ ("lock cmpxchg16b (%2)"
:"=a"(tmp.a),"=d"(tmp.b)
:"r"(&two),"a"(two.a),"d"(two.b),"b"(one.a),"c"(one.b)
:"memory");
return 0;
}]])],
    [sccp_cv_cpu_cmpxchg16b="yes"],
	[sccp_cv_cpu_cmpxchg16b="no"],
	[AS_IF([test "x$enable_cross_cmpxchg16b" == "xyes"],
	       [sccp_cv_cpu_cmpxchg16b="assuming yes"],
	       [sccp_cv_cpu_cmpxchg16b="assuming no"])])])
		   ])
AS_IF([test "x$sccp_cv_cpu_cmpxchg16b" == "xassuming yes"],[sccp_cv_cpu_cmpxchg16b="yes"])
AS_IF([test "x$sccp_cv_cpu_cmpxchg16b" == "xyes"],
      [AC_DEFINE([HAVE_CMPXCHG16B],[1],[if the compiler and cpu can both handle the 128-bit CMPXCHG16B instruction])])
AS_IF([test "$sccp_cv_require_ia64intrin_h" = "yes"],
	  [AC_DEFINE([SCCP_NEEDS_INTEL_INTRIN],[1],[if this header is necessary for builtin atomics])])
AS_IF([test "x$sccp_cv_atomic_CASptr" = "xyes"],
      [AC_DEFINE([SCCP_BUILTIN_CAS_PTR],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on pointers])])
AS_IF([test "x$sccp_cv_atomic_CAS32" = "xyes"],
      [AC_DEFINE([SCCP_BUILTIN_CAS32],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on 32-bit ints])])
AS_IF([test "x$sccp_cv_atomic_CAS64" = "xyes"],
      [AC_DEFINE([SCCP_BUILTIN_CAS64],[1],
	  	[if the compiler supports __sync_val_compare_and_swap on 64-bit ints])])
AS_IF([test "x$sccp_cv_atomic_CAS" = "xyes"],
	[AC_DEFINE([SCCP_BUILTIN_CAS],[1],[if the compiler supports __sync_val_compare_and_swap])])
AS_IF([test "x$sccp_cv_atomic_incr" = "xyes"],
	[AC_DEFINE([SCCP_BUILTIN_INCR],[1],[if the compiler supports __sync_fetch_and_add])])
	
	
dnl AS_IF([test "$sccp_cv_atomic_CAS" = "yes" -a "x$sccp_cv_atomic_CASptr" = "xyes" -a "$sccp_cv_atomic_incr" = "yes"],
AS_IF([test "$sccp_cv_atomic_CAS32" = "yes" -a "x$sccp_cv_atomic_CASptr" = "xyes" -a "$sccp_cv_atomic_incr" = "yes"],
                [
                 	using_atomic_buildin="yes"
                 	AC_DEFINE([SCCP_ATOMIC],1,[Defined SCCP Atomic])
  		 	AC_DEFINE([SCCP_ATOMIC_BUILTINS],[1],[if the compiler supports __sync_val_compare_and_swap])
		 	$1
		],[
			using_atomic_buildin="no"
			CC_works=0
			$2
		])
])


# SCCP_CHECK_ATOMIC_OPS([action-if-found], [action-if-not-found])
# ------------------------------------------------------------------------------
AC_DEFUN([SCCP_CHECK_ATOMIC_OPS], [
	AC_ARG_ENABLE([atomic_ops],
	     [AS_HELP_STRING([--disable-atomic-ops],
	                 [fallback if compiler-builtins for atomic functions are not available (using http://www.hpl.hp.com/research/linux/atomic_ops)])
	],enable_atomic_ops=$enableval, enable_atomic_ops=yes)
	AS_IF([test "x$using_atomic_buildin" = "xno" -a "x$enable_atomic_ops" = "xyes"],[
                AC_CHECK_HEADERS([atomic_ops.h],[
                        AC_DEFINE([SCCP_ATOMIC],1,[Defined SCCP Atomic])
                        AC_DEFINE([SCCP_ATOMIC_OPS],1,[Found Atomic Ops Library])
                        AC_DEFINE([AO_REQUIRE_CAS],1,[Defined AO_REQUIRE_CAS])
dnl                        LDFLAGS="$LDFLAGS -llibatomic_ops"
dnl                        AC_SUBST([LDFLAGS])
			$1
                ], [
                        AC_MSG_RESULT('Your platform does not support atomic operations and atomic_ops.h could not be found.')
                        AC_MSG_RESULT('Please install the libatomic-ops-dev / libatomic-ops-devel package for your platform, or')
                        AC_MSG_RESULT('Download the necessary library from http://www.hpl.hp.com/research/linux/atomic_ops so that these operations can be emulated')
			CC_works=0
dnl                        exit 254
			$2
                ])
        ])
])
