###############################################################
# EEL m4 macros
# David Olofson 2003, 2005
###############################################################


###############################################################
# Generate "helper script" for linking against EEL.
###############################################################
# Changed to make automake >= 1.8 happy (David Olofson)
# Stolen from Sam Lantinga
# stolen from Manish Singh
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor
###############################################################

dnl----------------------------------------------------------------------------------
dnl AM_PATH_EEL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for EEL, and define EEL_CFLAGS and EEL_LIBS
dnl----------------------------------------------------------------------------------
AC_DEFUN([AM_PATH_EEL],
[dnl
dnl Get the cflags and libraries from the eel-config script
dnl
AC_ARG_WITH(eel-prefix,[  --with-eel-prefix=PFX   Prefix where EEL is installed (optional)],
            eel_prefix="$withval", eel_prefix="")
AC_ARG_WITH(eel-exec-prefix,[  --with-eel-exec-prefix=PFX Exec prefix where EEL is installed (optional)],
            eel_exec_prefix="$withval", eel_exec_prefix="")
AC_ARG_ENABLE(eeltest, [  --disable-eeltest       Do not try to compile and run a test EEL program],
		    , enable_eeltest=yes)

  if test x$eel_exec_prefix != x ; then
     eel_args="$eel_args --exec-prefix=$eel_exec_prefix"
     if test x${EEL_CONFIG+set} != xset ; then
        EEL_CONFIG=$eel_exec_prefix/bin/eel-config
     fi
  fi
  if test x$eel_prefix != x ; then
     eel_args="$eel_args --prefix=$eel_prefix"
     if test x${EEL_CONFIG+set} != xset ; then
        EEL_CONFIG=$eel_prefix/bin/eel-config
     fi
  fi

  AC_REQUIRE([AC_CANONICAL_TARGET])
  AC_PATH_PROG(EEL_CONFIG, eel-config, no)
  min_eel_version=ifelse([$1], ,0.11.0,$1)
  AC_MSG_CHECKING(for EEL - version >= $min_eel_version)
  no_eel=""
  if test "$EEL_CONFIG" = "no" ; then
    no_eel=yes
  else
    EEL_CFLAGS=`$EEL_CONFIG $eelconf_args --cflags`
    EEL_LIBS=`$EEL_CONFIG $eelconf_args --libs`

    eel_major_version=`$EEL_CONFIG $eel_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    eel_minor_version=`$EEL_CONFIG $eel_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    eel_micro_version=`$EEL_CONFIG $eel_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_eeltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $EEL_CFLAGS"
      LIBS="$LIBS $EEL_LIBS"
dnl
dnl Now check if the installed EEL is sufficiently new. (Also sanity
dnl checks the results of eel-config to some extent
dnl
      rm -f conf.eeltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "EEL.h"

char*
my_strdup (char *str)
{
  char *new_str;

  if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;

  return new_str;
}

int main (int argc, char *argv[])
{
  int major, minor, micro;
  char *tmp_version;

  /* This hangs on some systems (?)
  system ("touch conf.eeltest");
  */
  { FILE *fp = fopen("conf.eeltest", "a"); if ( fp ) fclose(fp); }

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_eel_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_eel_version");
     exit(1);
   }

   if (($eel_major_version > major) ||
      (($eel_major_version == major) && ($eel_minor_version > minor)) ||
      (($eel_major_version == major) && ($eel_minor_version == minor) && ($eel_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'eel-config --version' returned %d.%d.%d, but the minimum version\n", $eel_major_version, $eel_minor_version, $eel_micro_version);
      printf("*** of EEL required is %d.%d.%d. If eel-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If eel-config was wrong, set the environment variable EEL_CONFIG\n");
      printf("*** to point to the correct copy of eel-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_eel=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_eel" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])
  else
     AC_MSG_RESULT(no)
     if test "$EEL_CONFIG" = "no" ; then
       echo "*** The eel-config script installed by EEL could not be found"
       echo "*** If EEL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the EEL_CONFIG environment variable to the"
       echo "*** full path to eel-config."
     else
       if test -f conf.eeltest ; then
        :
       else
          echo "*** Could not run EEL test program, checking why..."
          CFLAGS="$CFLAGS $EEL_CFLAGS"
          LIBS="$LIBS $EEL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include "eel.h"

int main(int argc, char *argv[])
{ return 0; }
#undef  main
#define main K_and_R_C_main
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding EEL or finding the wrong"
          echo "*** version of EEL. If it is not finding EEL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means EEL was incorrectly installed"
          echo "*** or that you have moved EEL since it was installed. In the latter case, you"
          echo "*** may want to edit the eel-config script: $EEL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     EEL_CFLAGS=""
     EEL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(EEL_CFLAGS)
  AC_SUBST(EEL_LIBS)
  rm -f conf.eeltest
])

