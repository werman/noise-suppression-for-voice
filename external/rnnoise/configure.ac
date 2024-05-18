# autoconf source script for generating configure

dnl The package_version file will be automatically synced to the git revision
dnl by the update_version script when configured in the repository, but will
dnl remain constant in tarball releases unless it is manually edited.
m4_define([CURRENT_VERSION],
          m4_esyscmd([ ./update_version 2>/dev/null || true
                       if test -e package_version; then
                           . ./package_version
                           printf "$PACKAGE_VERSION"
                       else
                           printf "unknown"
                       fi ]))

AC_INIT([rnnoise],[CURRENT_VERSION],[jmvalin@jmvalin.ca])
AC_CONFIG_SRCDIR([src/denoise.c])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([1.11 foreign no-define dist-zip subdir-objects])
AM_MAINTAINER_MODE([enable])

AC_C_INLINE

LT_INIT

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_DEFINE([RNNOISE_BUILD], [], [This is a build of the library])

dnl Library versioning for libtool.
dnl Please update these for releases.
dnl CURRENT, REVISION, AGE
dnl - library source changed -> increment REVISION
dnl - interfaces added/removed/changed -> increment CURRENT, REVISION = 0
dnl - interfaces added -> increment AGE
dnl - interfaces removed -> AGE = 0

OP_LT_CURRENT=4
OP_LT_REVISION=1
OP_LT_AGE=4

AC_SUBST(OP_LT_CURRENT)
AC_SUBST(OP_LT_REVISION)
AC_SUBST(OP_LT_AGE)

CC_CHECK_CFLAGS_APPEND(
  [-pedantic -Wall -Wextra -Wno-sign-compare -Wno-parentheses -Wno-long-long])

# Platform-specific tweaks
case $host in
  *-mingw*)
    # -std=c89 causes some warnings under mingw.
    CC_CHECK_CFLAGS_APPEND([-U__STRICT_ANSI__])
    # We need WINNT>=0x501 (WindowsXP) for getaddrinfo/freeaddrinfo.
    # It's okay to define this even when HTTP support is disabled, as it only
    #  affects header declarations, not linking (unless we actually use some
    #  XP-only functions).
    AC_DEFINE_UNQUOTED(_WIN32_WINNT,0x501,
     [We need at least WindowsXP for getaddrinfo/freeaddrinfo])
    host_mingw=true
    ;;
esac
AM_CONDITIONAL(OP_WIN32, test "$host_mingw" = "true")

AC_ARG_ENABLE([assertions],
  AS_HELP_STRING([--enable-assertions], [Enable assertions in code]),,
  enable_assertions=no)

AS_IF([test "$enable_assertions" = "yes"], [
  AC_DEFINE([OP_ENABLE_ASSERTIONS], [1], [Enable assertions in code])
])

AC_ARG_ENABLE([examples],
  AS_HELP_STRING([--disable-examples], [Do not build example applications]),,
  enable_examples=yes)
AM_CONDITIONAL([OP_ENABLE_EXAMPLES], [test "$enable_examples" = "yes"])

AC_ARG_ENABLE([dnn-debug-float],
              AS_HELP_STRING([--enable-dnn-debug-float], [Use floating-point DNN computation everywhere]),,
  enable_dnn_debug_float=no)

AS_IF([test "$enable_dnn_debug_float" = "no"], [
       AC_DEFINE([DISABLE_DEBUG_FLOAT], [1], [Disable DNN debug float])
])

OPUS_X86_SSE4_1_CFLAGS='-msse4.1'
OPUS_X86_AVX2_CFLAGS='-mavx -mfma -mavx2'
AC_SUBST([OPUS_X86_SSE4_1_CFLAGS])
AC_SUBST([OPUS_X86_AVX2_CFLAGS])
AC_ARG_ENABLE([x86-rtcd],
  AS_HELP_STRING([--enable-x86-rtcd], [x86 rtcd]),,
  enable_x86_rtcd=no)
AM_CONDITIONAL([RNN_ENABLE_X86_RTCD], [test "$enable_x86_rtcd" = "yes"])

AS_IF([test "$enable_x86_rtcd" = "yes"], [
  AC_DEFINE([RNN_ENABLE_X86_RTCD], [1], [Enable x86 rtcd])
  AC_DEFINE([CPU_INFO_BY_ASM], [1], [RTCD from ASM only for now])
])

AS_CASE(["$ac_cv_search_lrintf"],
  ["no"],[],
  ["none required"],[],
  [lrintf_lib="$ac_cv_search_lrintf"])

LT_LIB_M

AC_SUBST([lrintf_lib])

CC_ATTRIBUTE_VISIBILITY([default], [
  CC_FLAG_VISIBILITY([CFLAGS="${CFLAGS} -fvisibility=hidden"])
])

dnl Check for doxygen
AC_ARG_ENABLE([doc],
  AS_HELP_STRING([--disable-doc], [Do not build API documentation]),,
  [enable_doc=yes]
)

AS_IF([test "$enable_doc" = "yes"], [
  AC_CHECK_PROG([HAVE_DOXYGEN], [doxygen], [yes], [no])
  AC_CHECK_PROG([HAVE_DOT], [dot], [yes], [no])
],[
  HAVE_DOXYGEN=no
])

AM_CONDITIONAL([HAVE_DOXYGEN], [test "$HAVE_DOXYGEN" = "yes"])

AC_CONFIG_FILES([
  Makefile
  rnnoise.pc
  rnnoise-uninstalled.pc
  doc/Doxyfile
])
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT

AC_MSG_NOTICE([
------------------------------------------------------------------------
  $PACKAGE_NAME $PACKAGE_VERSION: Automatic configuration OK.

    Assertions ................... ${enable_assertions}

    Hidden visibility ............ ${cc_cv_flag_visibility}

    API code examples ............ ${enable_examples}
    API documentation ............ ${enable_doc}
------------------------------------------------------------------------
])
