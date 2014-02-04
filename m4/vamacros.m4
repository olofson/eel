dnl Check for support for variadic macros.
dnl (Stolen from rra-c-util, http://www.eyrie.org/~eagle/software/rra-c-util/)
dnl
dnl This file defines two macros for probing for compiler support for variadic
dnl macros.  Provided are RRA_C_C99_VAMACROS, which checks for support for the
dnl C99 variadic macro syntax, namely:
dnl
dnl     #define macro(...) fprintf(stderr, __VA_ARGS__)
dnl
dnl and RRA_C_GNU_VAMACROS, which checks for support for the older GNU
dnl variadic macro syntax, namely:
dnl
dnl    #define macro(args...) fprintf(stderr, args)
dnl
dnl They set HAVE_C99_VAMACROS or HAVE_GNU_VAMACROS as appropriate.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2006, 2008, 2009
dnl     Board of Trustees, Leland Stanford Jr. University
dnl
dnl See ../COPYING for licensing terms.

AC_DEFUN([_RRA_C_C99_VAMACROS_SOURCE], [[
#include <stdio.h>
#define error(...) fprintf(stderr, __VA_ARGS__)

int
main(void) {
    error("foo");
    error("foo %d", 0);
    return 0;
}
]])

AC_DEFUN([RRA_C_C99_VAMACROS],
[AC_CACHE_CHECK([for C99 variadic macros], [rra_cv_c_c99_vamacros],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_RRA_C_C99_VAMACROS_SOURCE])],
        [rra_cv_c_c99_vamacros=yes],
        [rra_cv_c_c99_vamacros=no])])
 AS_IF([test x"$rra_cv_c_c99_vamacros" = xyes],
    [AC_DEFINE([HAVE_C99_VAMACROS], 1,
        [Define if the compiler supports C99 variadic macros.])])])

AC_DEFUN([_RRA_C_GNU_VAMACROS_SOURCE], [[
#include <stdio.h>
#define error(args...) fprintf(stderr, args)

int
main(void) {
    error("foo");
    error("foo %d", 0);
    return 0;
}
]])

AC_DEFUN([RRA_C_GNU_VAMACROS],
[AC_CACHE_CHECK([for GNU-style variadic macros], [rra_cv_c_gnu_vamacros],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_RRA_C_GNU_VAMACROS_SOURCE])],
        [rra_cv_c_gnu_vamacros=yes],
        [rra_cv_c_gnu_vamacros=no])])
 AS_IF([test x"$rra_cv_c_gnu_vamacros" = xyes],
    [AC_DEFINE([HAVE_GNU_VAMACROS], 1,
        [Define if the compiler supports GNU-style variadic macros.])])])
