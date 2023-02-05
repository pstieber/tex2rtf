dnl Process this file with autoconf to produce a configure script.
AC_INIT([tex2rtf], [5.0.0], [pstieber@verizon.net], [tex2rtf])

AC_CONFIG_MACRO_DIR([m4])

dnl Change the location of the auxiliary configure files to config.
AC_CONFIG_AUX_DIR([config])

AC_CONFIG_SRCDIR([src/tex2rtf.cpp])

AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_HEADERS(src/config.h)

dnl Checks for programs.

AC_PROG_CPP

dnl C-compiler checks
dnl   defines CC with the compiler to use
dnl   defines GCC with yes if using gcc
dnl   defines GCC empty if not using gcc
dnl   defines CFLAGS
dnl
dnl this magic incantation is needed to prevent AC_PROG_CC from setting the
dnl default CFLAGS (something like "-g -O2") -- we don't need this as we add
dnl -g and -O flags ourselves below
CFLAGS=${CFLAGS:=}
AC_PROG_CC

AC_LANG_SAVE
AC_LANG([C++])

dnl C++-compiler checks
dnl   defines CXX with the compiler to use
dnl   defines GXX with yes if using gxx
dnl   defines GXX empty if not using gxx
dnl   defines CXXFLAGS
dnl
dnl see CFLAGS line above
CXXFLAGS=${CXXFLAGS:=}
AC_PROG_CXX

AC_LANG_RESTORE

dnl Make autoconf use C++ for its tests.
AC_LANG(C++)

dnl Checks for libraries.
     
dnl Checks for header files.
AC_CHECK_HEADERS([string.h fcntl.h sys/file.h sys/param.h])

dnl Checks for library functions.
AC_FUNC_ALLOCA

AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(
  debug,
  AS_HELP_STRING(
    [--enable-debug],
    [enable debugging code (off by default)]),
  ac_arg_debug="$enableval",
  ac_arg_debug="no")
AC_MSG_RESULT(${enable_debug-no})

AC_MSG_CHECKING([whether to enable optimization])
AC_ARG_ENABLE(
  optimize,
  AS_HELP_STRING(
    [--enable-optimize@<:@=ARG@:>@],
    [enable optimized code with ARG = 0, 1, 2, or 3 (off by default ARG = 0)]),
  [opt_level="$enableval" ac_arg_optimize="yes"],
  ac_arg_optimize="no")
AC_MSG_RESULT(${enable_optimize-no})

AC_MSG_CHECKING([whether to enable profiling])
AC_ARG_ENABLE(
  profile,
  AS_HELP_STRING(
    [--enable-profile],
    [enable gprof profiling code (off by default)]),
  ac_arg_profile="$enableval",
  ac_arg_profile="no")
AC_MSG_RESULT(${enable_profile-no})

AC_CANONICAL_HOST

dnl To support a new system, you need to add its canonical name (as determined
dnl by config.sub or specified by the configure command line) to this "case"
dnl and also define the shared library flags below - search for
dnl SHARED_LIB_SETUP to find the exact place.
case "${host}" in
  *-darwin* )
  ;;
  *-*-linux* )
  ;;
  *)
    AC_MSG_ERROR(unknown system type ${host}.)
esac

dnl Check for wxWidgets
WX_CONFIG_OPTIONS
MIN_WX_VERSION="3.0.0"
WX_CONFIG_CHECK(
  [$MIN_WX_VERSION],
  [],
  [AC_MSG_ERROR([
    wxWidgets must be installed on your system, but the
    wx-config script couldn't be found.

    Please check that wx-config is in your PATH, the
    directory where wxWidgets libraries are installed
    (returned by 'wx-config --libs' command) is in
    LIBRARY_PATH or the equivalent variable, and the
    wxWidgets version is $MIN_WX_VERSION or above.
  ])],
  [std],
  [--debug=$ac_arg_debug])

TEX2RTF_PATH=\${top_srcdir}
AC_SUBST(TEX2RTF_PATH)

if test x$ac_arg_debug = xyes ; then
  TLDEBUG="-g -fno-inline"
else
  TLDEBUG="-DNDEBUG"
fi

PROFILE=
if test x$ac_arg_profile = xyes ; then
  dnl The debug flag must be on for profiling to work.
  if test x$TLDEBUG = x ; then
    TLDEBUG="-g -fno-inline"
  fi
  PROFILE="-pg"
fi

OPTIMISE=-O0
if test x$ac_arg_optimize = xyes ; then
  if test x$opt_level = x; then
    OPTIMISE="-O2"
  elif test x$opt_level = x0 ; then
    OPTIMISE="-O1"
  elif test x$opt_level = x1 ; then
    OPTIMISE="-O1"
  elif test x$opt_level = x2 ; then
    OPTIMISE="-O2"
  elif test x$opt_level = x3 ; then
    OPTIMISE="-O3"
  fi
fi

EXTRA_CFLAGS="$TLDEBUG $PROFILE $OPTIMISE"

CPPFLAGS=`echo -Wall -Wno-unknown-pragmas $BITMAPS_FLAGS $WX_CPPFLAGS | \
sed 's/ \+/ /g'`

CFLAGS=`echo $EXTRA_CFLAGS $CFLAGS $WX_CFLAGS_ONLY | \
sed 's/ \\+/ /g'`

CXXFLAGS=`echo -std=c++11 $EXTRA_CFLAGS $CXXFLAGS $WX_CXXFLAGS_ONLY | \
sed 's/ \+/ /g'`

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
