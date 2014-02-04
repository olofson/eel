dnl -- Begin libpng autoconf macro
dnl Copyright (c) 2002 Patrick McFarland
dnl Under the GPL License
dnl When copying, include from Begin to End libpng autoconf macro, including
dnl those tags, so others can easily copy it too. (Maybe someday this will
dnl become libpng.m4?)
dnl
dnl AM_PATH_LIBPNG([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Tests for libpng, outputs LIBPNG_VERSION, LIBPNG_LIBS, and LIBPNG_CFLAGS
AC_DEFUN([AM_PATH_LIBPNG],
[dnl
dnl
dnl

dnl Comment out this section, and the other labled parts to disable the user
dnl having a choice about being able to disable libpng or not. Recommended
dnl you use AC_MSG_ERROR(LIBPNG >= 1.0.0 is required) for the
dnl ACTION-IF-NOT-FOUND if you plan on disabling user choice.

dnl <--- disable for no user choice part #1
AC_ARG_ENABLE(libpng,[  --disable-libpng        Build without libpng support ],,enable_libpng=yes)
dnl --->

AC_ARG_WITH(libpng-prefix,[  --with-libpng-prefix=PFX Prefix where libpng is installed (optional)], libpng_prefix="$withval", libpng_prefix="")

min_libpng_version=ifelse([$1], ,1.2.0,$1)
tempLIBS="$LIBS"
tempCFLAGS="$CFLAGS"
if test x$libpng_prefix != x ; then
  LIBPNG_LIBS="-L$libpng_prefix"
  LIBPNG_CFLAGS="-I$libpng_prefix"
fi
LIBPNG_LIBS="$LIBPNG_LIBS -lpng -lm"
LIBS="$LIBS $LIBPNG_LIBS"
CFLAGS="$CFLAGS $LIBPNG_CFLAGS"

AC_MSG_CHECKING(for libpng - version >= $min_libpng_version)
with_libpng=no

dnl <--- disable for no user choice part #2
if test x$enable_libpng != xno; then
dnl --->

  AC_TRY_RUN([
  #include <png.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>

  char* my_strdup (char *str)
  {
    char *new_str;

    if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
    else new_str = NULL;

    return new_str;
  }

  int main (int argc, char *argv[])
  {
    int major, minor, micro, libpng_major_version, libpng_minor_version, libpng_micro_version;

    char *libpngver, *tmp_version;

    libpngver = PNG_LIBPNG_VER_STRING;

    FILE *fp = fopen("conf.libpngtest", "a");
    if ( fp ) {
      fprintf(fp, "%s", libpngver);
      fclose(fp);
    }

    /* HP/UX 9 (%@#!) writes to sscanf strings */
    tmp_version = my_strdup("$min_libpng_version");
    if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
      printf("%s, bad version string for\n\tmin_libpng_version... ", "$min_libpng_version");
      exit(1);
    }
    if (sscanf(libpngver, "%d.%d.%d", &libpng_major_version, &libpng_minor_version, &libpng_micro_version) != 3) {
      printf("%s, bad version string given\n", libpngver);
      puts("\tby libpng, sometimes due to very old libpngs that didnt correctly");
      printf("\tdefine their version. Please upgrade if you are running an\n\told libpng... ");
      exit(1);
    }
    if ((libpng_major_version > major) ||
       ((libpng_major_version == major) && (libpng_minor_version > minor)) ||
       ((libpng_major_version == major) && (libpng_minor_version == minor) && (libpng_micro_version >= micro)))
    {
      return 0;
    }
    else
    {
      return 1;
    }
  }
  ],with_libpng=yes,,[AC_MSG_WARN(Cross Compiling, Assuming libpng is avalible)])

  if test x$with_libpng = xyes; then
    AC_MSG_RESULT(yes)
    LIBPNG_VERSION=$(<conf.libpngtest)
    ifelse([$2], , :, [$2])
  else
    AC_MSG_RESULT(no)
    LIBPNG_CFLAGS=""
    LIBPNG_LIBS=""
    LIBPNG_VERSION=""
    ifelse([$3], , :, [$3])
  fi
  LIBS="$tempLIBS"
  CFLAGS="$tempCFLAGS"
  rm conf.libpngtest
  AC_SUBST(LIBPNG_CFLAGS)
  AC_SUBST(LIBPNG_VERSION)
  AC_SUBST(LIBPNG_LIBS)

dnl <--- disable for no user choice part #3
else
  AC_MSG_RESULT(disabled by user)
fi
dnl --->
])
dnl -- End libpng autoconf macro
